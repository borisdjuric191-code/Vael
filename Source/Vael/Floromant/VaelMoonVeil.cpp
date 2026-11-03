// Copyright Epic Games, Inc. All Rights Reserved.

#include "Floromant/VaelMoonVeil.h"
#include "Combat/VaelCharacterBase.h"
#include "Combat/VaelCombatStatics.h"
#include "Components/PointLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Floromant/VaelPlantArmData.h"
#include "Magic/VaelSpellProjectile.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"
#include "VaelAssets.h"
#include "Vael.h"
#include "Floromant/VaelBindweedLasso.h"
#include "Magic/VaelDebrisBurst.h"
#include "Player/VaelCharacter.h"

namespace
{
	/** Size of the engine plane in cm */
	constexpr float VeilPlaneSize = 100.0f;

	/** How long the veil takes to fall open and to fade */
	constexpr float VeilRiseSeconds = 0.45f;
	constexpr float VeilFadeSeconds = 0.9f;

	/** The Mondtropfen of the veil are bigger than the loot, so they read from the camera */
	constexpr float VeilDropScale = 2.4f;

	/** Conditions an ally walking through is cleansed of */
	const EVaelStatus CleansedStatuses[] = { EVaelStatus::Burning, EVaelStatus::Pitched, EVaelStatus::Marked, EVaelStatus::Confused };

	bool VeilOnPlayerSide(const AActor* Actor)
	{
		if (const AVaelCharacterBase* Character = Cast<AVaelCharacterBase>(Actor))
		{
			return Character->IsOnPlayerSide();
		}
		const APawn* Pawn = Cast<APawn>(Actor);
		return Pawn != nullptr && Pawn->IsPlayerControlled();
	}
}

AVaelMoonVeil::AVaelMoonVeil()
{
	PrimaryActorTick.bCanEverTick = true;

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent = Root;

	Curtain = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Curtain"));
	Curtain->SetupAttachment(Root);
	static ConstructorHelpers::FObjectFinder<UStaticMesh> PlaneMesh(TEXT("/Engine/BasicShapes/Plane.Plane"));
	if (PlaneMesh.Succeeded())
	{
		Curtain->SetStaticMesh(PlaneMesh.Object);
	}
	Curtain->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Curtain->SetCastShadow(false);
	Curtain->bReceivesDecals = false;
	Curtain->SetCanEverAffectNavigation(false);

	Light = CreateDefaultSubobject<UPointLightComponent>(TEXT("Light"));
	Light->SetupAttachment(Root);
	Light->SetMobility(EComponentMobility::Movable);
	Light->SetIntensityUnits(ELightUnits::Candelas);
	Light->SetIntensity(0.0f);
	Light->SetCastShadows(false);
	Light->SetAttenuationRadius(600.0f);
}

AVaelMoonVeil* AVaelMoonVeil::Raise(APawn* InOwner, const UVaelPlantArmData* InPlant, const FVector& Ground, const FVector& Facing, float SizeScale)
{
	UWorld* World = InOwner != nullptr ? InOwner->GetWorld() : nullptr;
	if (World == nullptr || InPlant == nullptr)
	{
		return nullptr;
	}
	FActorSpawnParameters Parameters;
	Parameters.Owner = InOwner;
	Parameters.Instigator = InOwner;
	Parameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	const FRotator Rotation(0.0f, Facing.GetSafeNormal2D().Rotation().Yaw, 0.0f);
	AVaelMoonVeil* Veil = World->SpawnActor<AVaelMoonVeil>(AVaelMoonVeil::StaticClass(), FTransform(Rotation, Ground), Parameters);
	if (Veil != nullptr)
	{
		Veil->Plant = InPlant;
		Veil->Wide = InPlant->VeilWidth * SizeScale;
		Veil->Tall = InPlant->VeilHeight * FMath::Sqrt(SizeScale);
		Veil->Lifetime = InPlant->VeilSeconds;
		Veil->BuildLook();
	}
	return Veil;
}

void AVaelMoonVeil::BuildLook()
{
	// The plane stands up across the facing, its normal along the actor's forward
	Curtain->SetRelativeRotation(FRotator(0.0f, 90.0f, 90.0f));
	if (UMaterialInterface* Look = VaelAssets::LoadOptional(Plant->VeilMaterial))
	{
		CurtainLook = Curtain->CreateDynamicMaterialInstance(0, Look);
		CurtainLook->SetVectorParameterValue(TEXT("Farbe"), Plant->VeilColor);
	}
	Light->SetLightColor(Plant->VeilColor);
	Light->SetRelativeLocation(FVector(-40.0f, 0.0f, Tall * 0.6f));

	UStaticMesh* DropMesh = VaelAssets::LoadOptional(Plant->DropMesh);
	UStaticMesh* Cylinder = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	UMaterialInterface* DropLook = VaelAssets::LoadOptional(Plant->DropMaterial);
	UMaterialInterface* ThreadLook = VaelAssets::LoadOptional(Plant->ThreadMaterial);
	if (DropMesh == nullptr || Cylinder == nullptr)
	{
		return;
	}
	const auto MakePart = [this](UStaticMesh* Mesh, UMaterialInterface* Look)
	{
		UStaticMeshComponent* Part = NewObject<UStaticMeshComponent>(this);
		Part->SetMobility(EComponentMobility::Movable);
		Part->SetStaticMesh(Mesh);
		Part->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Part->SetCanEverAffectNavigation(false);
		Part->SetCastShadow(false);
		Part->SetupAttachment(Root);
		Part->RegisterComponent();
		if (Look != nullptr)
		{
			Part->SetMaterial(0, Look);
		}
		return Part;
	};

	// The drops hang from the top edge on threads of uneven length, as from the bud of the plant
	FRandomStream Random(GetUniqueID());
	for (int32 Index = 0; Index < FMath::Max(3, FMath::RoundToInt(Plant->VeilDrops * Wide / FMath::Max(Plant->VeilWidth, 1.0f))); ++Index)
	{
		Threads.Add(MakePart(Cylinder, ThreadLook));
		UStaticMeshComponent* Drop = MakePart(DropMesh, DropLook);
		Drops.Add(Drop);
		DropLooks.Add(Drop->CreateAndSetMaterialInstanceDynamic(0));
		DropLengths.Add(Random.FRandRange(0.25f, 0.7f));
		DropFlares.Add(0.0f);
	}
}

void AVaelMoonVeil::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (Plant == nullptr)
	{
		Destroy();
		return;
	}

	Age += DeltaSeconds;
	const float Fade = FMath::Clamp((Lifetime - Age) / VeilFadeSeconds, 0.0f, 1.0f);
	if (Fade <= 0.0f)
	{
		Destroy();
		return;
	}
	Rise = FMath::Min(1.0f, Age / VeilRiseSeconds);
	const float Open = FMath::InterpEaseOut(0.0f, 1.0f, Rise, 2.0f);
	RippleStrength = FMath::Max(0.0f, RippleStrength - DeltaSeconds * 2.2f);
	const float Time = GetWorld()->GetTimeSeconds();

	// The curtain falls open from the top edge, as light running down
	const float Height = Tall * Open;
	Curtain->SetRelativeLocation(FVector(0.0f, 0.0f, Tall - Height * 0.5f));
	Curtain->SetRelativeScale3D(FVector(Wide / VeilPlaneSize, FMath::Max(Height, 1.0f) / VeilPlaneSize, 1.0f));
	const float Strength = Fade * (1.0f + 1.6f * RippleStrength + 0.12f * FMath::Sin(Time * 1.7f));
	if (CurtainLook != nullptr)
	{
		CurtainLook->SetScalarParameterValue(TEXT("Staerke"), Strength);
		CurtainLook->SetScalarParameterValue(TEXT("Hoehe"), FMath::Max(Height, 1.0f));
		CurtainLook->SetScalarParameterValue(TEXT("BogenTiefe"), Tall * Plant->VeilArch);
		// The ring of the last hit runs out from its point and fades
		CurtainLook->SetScalarParameterValue(TEXT("Welle"), RippleStrength);
		CurtainLook->SetScalarParameterValue(TEXT("WellenRadius"), (1.0f - RippleStrength) * 260.0f);
		CurtainLook->SetVectorParameterValue(TEXT("WellenOrt"), FLinearColor(GetActorTransform().TransformPositionNoScale(RippleLocal)));
	}
	Light->SetIntensity(14.0f * Strength);

	for (int32 Index = 0; Index < Drops.Num(); ++Index)
	{
		DropFlares[Index] = FMath::Max(0.0f, DropFlares[Index] - DeltaSeconds * 1.5f);
		const float Share = (Index + 0.5f) / Drops.Num();
		const float Y = (Share - 0.5f) * Wide * 0.92f;
		const FVector Top(0.0f, Y, GetTop(Y));
		const float Length = GetTop(Y) * DropLengths[Index] * Open;
		const float Swing = 0.06f * FMath::Sin(Time * 1.3f + Index * 2.1f) + 0.35f * DropFlares[Index] * FMath::Sin(Time * 14.0f);
		const FVector Hang = FVector(Swing, 0.3f * Swing, -1.0f).GetSafeNormal();
		const FVector End = Top + Hang * Length;

		Threads[Index]->SetRelativeLocationAndRotation((Top + End) * 0.5f, FRotationMatrix::MakeFromZ(Hang).Rotator());
		Threads[Index]->SetRelativeScale3D(FVector(0.006f, 0.006f, FMath::Max(Length, 1.0f) / 100.0f));
		Drops[Index]->SetRelativeLocationAndRotation(End, FRotationMatrix::MakeFromZ(-Hang).Rotator());
		Drops[Index]->SetRelativeScale3D(FVector(VeilDropScale * Fade * (1.0f + 0.3f * DropFlares[Index])));
		if (DropLooks[Index] != nullptr)
		{
			DropLooks[Index]->SetScalarParameterValue(TEXT("Leuchten"), (1.6f + 5.0f * DropFlares[Index]) * Fade);
		}
	}

	if (Rise >= 1.0f && Fade >= 1.0f)
	{
		StopShots();
	}
	Cleanse();
	TickPair(DeltaSeconds);
}

float AVaelMoonVeil::GetTop(float Y) const
{
	const float Across = FMath::Clamp(2.0f * Y / FMath::Max(Wide, 1.0f), -1.0f, 1.0f);
	return Tall * (1.0f - Plant->VeilArch * (1.0f - FMath::Sqrt(1.0f - Across * Across)));
}

float AVaelMoonVeil::SideOf(const FVector& Location) const
{
	return FVector::DotProduct(Location - GetActorLocation(), GetActorForwardVector()) >= 0.0f ? 1.0f : -1.0f;
}

bool AVaelMoonVeil::Crosses(const FVector& From, const FVector& To) const
{
	const FTransform& Transform = GetActorTransform();
	const FVector A = Transform.InverseTransformPositionNoScale(From);
	const FVector B = Transform.InverseTransformPositionNoScale(To);
	if ((A.X >= 0.0f) == (B.X >= 0.0f))
	{
		return false;
	}
	const FVector Point = FMath::Lerp(A, B, A.X / (A.X - B.X));
	return FMath::Abs(Point.Y) <= Wide * 0.5f && Point.Z >= -60.0f && Point.Z <= GetTop(Point.Y) + 40.0f;
}

void AVaelMoonVeil::Ripple(const FVector& Location)
{
	RippleStrength = 1.0f;
	RippleLocal = GetActorTransform().InverseTransformPositionNoScale(Location);
	// The drops nearest to the point flare
	for (int32 Index = 0; Index < Drops.Num(); ++Index)
	{
		const float Y = ((Index + 0.5f) / Drops.Num() - 0.5f) * Wide;
		DropFlares[Index] = FMath::Max(DropFlares[Index], FMath::Clamp(1.0f - FMath::Abs(Y - RippleLocal.Y) / 150.0f, 0.0f, 1.0f));
	}
}

void AVaelMoonVeil::StopShots()
{
	const float Reach = Wide * 0.5f + 400.0f;
	for (TActorIterator<AVaelSpellProjectile> It(GetWorld()); It; ++It)
	{
		AVaelSpellProjectile* Shot = *It;
		if (Shot->IsActorBeingDestroyed() || FVector::DistSquared2D(Shot->GetActorLocation(), GetActorLocation()) > Reach * Reach)
		{
			continue;
		}
		// Where the shot was a moment ago against where it is: did it fly into the curtain?
		const FVector Now = Shot->GetActorLocation();
		const FVector Before = Now - Shot->GetVelocity() * GetWorld()->GetDeltaSeconds() * 1.5f;
		if (!VeilOnPlayerSide(Shot->GetInstigator()) && Crosses(Before, Now))
		{
			UE_LOG(LogVael, Display, TEXT("Moon veil: stopped a shot of %s"), *GetNameSafe(Shot->GetInstigator()));
			Ripple(Now);
			Shot->Block();
			++StoppedShots;
			// Sternenfall: the stopped shot falls back as a spark on the enemy nearest to it
			if (Pair == EVaelSymbiosis::Sternenfall)
			{
				if (const AActor* Enemy = VaelSymbiosis::FindTarget(GetInstigator(), Now, 900.0f))
				{
					FVaelSpellHit Spark;
					Spark.Damage = 6.0f;
					Spark.Element = EVaelElement::Fire;
					AVaelSpellProjectile::Launch(GetInstigator(), Now, (Enemy->GetActorLocation() - Now).GetSafeNormal2D(), Spark, 1400.0f, 12.0f, 1.5f, FLinearColor(1.0f, 0.55f, 0.2f));
				}
			}
		}
	}
}

void AVaelMoonVeil::Cleanse()
{
	for (TActorIterator<AVaelCharacterBase> It(GetWorld()); It; ++It)
	{
		AVaelCharacterBase* Ally = *It;
		if (!Ally->IsOnPlayerSide() || FVector::DistSquared2D(Ally->GetActorLocation(), GetActorLocation()) > FMath::Square(Wide))
		{
			continue;
		}
		const float Side = SideOf(Ally->GetActorLocation());
		const float* LastSide = AllySides.Find(Ally);
		if (LastSide != nullptr && *LastSide != Side && Crosses(Ally->GetActorLocation() - GetActorForwardVector() * Side * 60.0f, Ally->GetActorLocation()))
		{
			for (const EVaelStatus Status : CleansedStatuses)
			{
				UVaelCombatStatics::RemoveStatus(Ally, Status);
			}
			Ripple(Ally->GetActorLocation());
			// Mondrose: who walks through also gets a small heart shield
			if (Pair == EVaelSymbiosis::Mondrose)
			{
				if (AVaelCharacter* Player = Cast<AVaelCharacter>(Ally))
				{
					Player->AddHeartShield(15.0f, 6.0f);
				}
			}
		}
		AllySides.Add(Ally, Side);
	}
}

float AVaelMoonVeil::ScaleDamage(AVaelCharacterBase* Target, const AActor* Attacker, float Damage, const FGameplayTagContainer& DamageTags)
{
	if (Target == nullptr || Attacker == nullptr || Damage <= 0.0f || Attacker == Target)
	{
		return Damage;
	}
	const bool bTargetOnPlayerSide = Target->IsOnPlayerSide();
	if (bTargetOnPlayerSide == VeilOnPlayerSide(Attacker))
	{
		return Damage;
	}
	float Scale = 1.0f;
	for (TActorIterator<AVaelMoonVeil> It(Target->GetWorld()); It; ++It)
	{
		AVaelMoonVeil* Veil = *It;
		if (Veil->Plant == nullptr || Veil->Rise < 1.0f || !Veil->Crosses(Attacker->GetActorLocation(), Target->GetActorLocation()))
		{
			continue;
		}
		// What enemies strike through it is swallowed, what her side casts through it hits harder
		if (bTargetOnPlayerSide)
		{
			Scale = FMath::Min(Scale, Veil->Plant->VeilEnemyScale);
			UE_LOG(LogVael, Display, TEXT("Moon veil: swallowed %.1f damage of %s"), Damage, *GetNameSafe(Attacker));
			Veil->Ripple(Target->GetActorLocation());
		}
		else
		{
			Scale = FMath::Max(Scale, Veil->Plant->VeilAlliedScale);
			// Glutvorhang: what her side casts through it catches fire
			if (Veil->Pair == EVaelSymbiosis::Glutvorhang)
			{
				UVaelCombatStatics::ApplyStatus(const_cast<AActor*>(Attacker), Target, EVaelStatus::Burning, 3.0f, 4.0f);
			}
		}
	}
	return Damage * Scale;
}

void AVaelMoonVeil::SetPair(EVaelSymbiosis InPair, const UVaelPlantArmData* InOther, float LifetimeScale)
{
	Pair = InPair;
	Other = InOther;
	Lifetime *= LifetimeScale;
	if (CurtainLook != nullptr && Pair != EVaelSymbiosis::None)
	{
		// The veil takes on a little of the other plant's colour
		CurtainLook->SetVectorParameterValue(TEXT("Farbe"), FMath::Lerp(Plant->VeilColor, VaelSymbiosis::GetColor(Pair), 0.35f));
	}
}

void AVaelMoonVeil::TickPair(float DeltaSeconds)
{
	if (Pair == EVaelSymbiosis::None || Rise < 1.0f)
	{
		return;
	}

	// Wehender Schleier: it wanders slowly ahead and pushes the shots before it back
	if (Pair == EVaelSymbiosis::WehenderSchleier)
	{
		AddActorWorldOffset(GetActorForwardVector() * 70.0f * DeltaSeconds);
	}

	PairCountdown -= DeltaSeconds;
	if (PairCountdown > 0.0f)
	{
		return;
	}
	PairCountdown = 0.5f;
	APawn* Gardener = GetInstigator();
	const FTransform& Transform = GetActorTransform();

	for (TActorIterator<AVaelCharacterBase> It(GetWorld()); It; ++It)
	{
		AVaelCharacterBase* Character = *It;
		const FVector Local = Transform.InverseTransformPositionNoScale(Character->GetActorLocation());
		if (FMath::Abs(Local.Y) > Wide * 0.5f + 60.0f || FMath::Abs(Local.X) > 450.0f || Character->GetHealth() <= 0.0f)
		{
			continue;
		}
		const bool bAlly = Character->IsOnPlayerSide();
		switch (Pair)
		{
		case EVaelSymbiosis::Taustrom:
			// It drips: enemies before it are soaked
			if (!bAlly && Local.X > 0.0f && UVaelCombatStatics::CanDamage(Gardener, Character))
			{
				UVaelCombatStatics::ApplyStatus(Gardener, Character, EVaelStatus::Wet, 4.0f);
			}
			break;
		case EVaelSymbiosis::Mondranken:
			// Tendrils on the veil catch enemies that come close to go through, once each
			if (!bAlly && FMath::Abs(Local.X) < 160.0f && Other != nullptr && !Snared.Contains(Character) && UVaelCombatStatics::CanDamage(Gardener, Character))
			{
				Snared.Add(Character);
				FVaelLassoSpec Spec;
				Spec.HoldSeconds = 2.5f;
				Spec.bOverGround = true;
				Spec.Color = VaelSymbiosis::GetColor(Pair);
				AVaelBindweedLasso::Throw(Gardener, Other, GetActorLocation() + Transform.TransformVectorNoScale(FVector(0.0f, Local.Y, 5.0f)), Character, Spec, false);
				Ripple(Character->GetActorLocation());
			}
			break;
		case EVaelSymbiosis::Mondhalle:
			// Between the two veils of the hall the allies are cleansed again and again
			if (bAlly && FMath::Abs(Local.X) < 320.0f)
			{
				for (const EVaelStatus Status : CleansedStatuses)
				{
					UVaelCombatStatics::RemoveStatus(Character, Status);
				}
			}
			break;
		default:
			break;
		}
	}
}

void AVaelMoonVeil::Destroyed()
{
	// Bernsteinschleier: at its end the resin veil bursts into sticky splinters
	if (Pair == EVaelSymbiosis::Bernsteinschleier && Plant != nullptr && GetWorld() != nullptr)
	{
		APawn* Gardener = GetInstigator();
		for (TActorIterator<AVaelCharacterBase> It(GetWorld()); It; ++It)
		{
			if (!It->IsOnPlayerSide() && FVector::Dist2D(It->GetActorLocation(), GetActorLocation()) < Wide * 0.5f + 150.0f && UVaelCombatStatics::CanDamage(Gardener, *It))
			{
				UVaelCombatStatics::ApplyStatus(Gardener, *It, EVaelStatus::Pitched, 5.0f);
			}
		}
		FVaelDebris Splinters;
		Splinters.Count = 24;
		Splinters.Spread = Wide * 0.4f;
		Splinters.Speed = 300.0f;
		Splinters.Lift = 200.0f;
		Splinters.Lifetime = 1.0f;
		Splinters.MinSize = 3.0f;
		Splinters.MaxSize = 7.0f;
		Splinters.Color = FLinearColor(1.0f, 0.45f, 0.05f);
		AVaelDebrisBurst::Spawn(Gardener, GetActorLocation() + FVector(0.0f, 0.0f, Tall * 0.5f), Splinters);
	}
	Super::Destroyed();
}
