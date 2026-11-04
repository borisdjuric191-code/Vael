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

	TickPairLook(DeltaSeconds, Open, Fade);

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
	if (Pair == EVaelSymbiosis::None)
	{
		return;
	}

	// Every partner gives the veil a look of its own (Boris 2026-11-04): its colour in the light, how thick and how fast the
	// light runs, and what falls, rises or glitters in it
	FLinearColor Tone = FMath::Lerp(Plant->VeilColor, VaelSymbiosis::GetColor(Pair), 0.6f);
	float Sheet = 0.22f;
	float Flow = 0.3f;
	switch (Pair)
	{
	case EVaelSymbiosis::Glutvorhang:		Tone = FLinearColor(1.0f, 0.32f, 0.04f); Sheet = 0.3f; Flow = 0.5f; break;
	case EVaelSymbiosis::Taustrom:			Tone = FLinearColor(0.2f, 0.65f, 1.0f); Flow = 0.9f; break;
	case EVaelSymbiosis::WehenderSchleier:	Tone = FLinearColor(0.65f, 1.0f, 0.75f); Flow = 1.1f; break;
	case EVaelSymbiosis::Bernsteinschleier:	Tone = FLinearColor(1.0f, 0.5f, 0.05f); Sheet = 0.5f; Flow = 0.15f; break;
	case EVaelSymbiosis::Sternenfall:		Tone = FLinearColor(1.0f, 0.82f, 0.4f); Sheet = 0.16f; break;
	case EVaelSymbiosis::Mondranken:		Tone = FLinearColor(0.8f, 1.0f, 0.4f); break;
	case EVaelSymbiosis::Mondrose:			Tone = FLinearColor(1.0f, 0.2f, 0.35f); Sheet = 0.28f; break;
	case EVaelSymbiosis::Mondkuppel:		Tone = FLinearColor(1.0f, 0.85f, 0.5f); Sheet = 0.3f; break;
	case EVaelSymbiosis::Mondhalle:			Tone = FLinearColor(0.85f, 0.9f, 1.0f); Sheet = 0.3f; break;
	case EVaelSymbiosis::Feentor:			Tone = FLinearColor(0.85f, 0.45f, 1.0f); break;
	default: break;
	}
	PairLookColor = Tone;
	// The threads take the colour too
	for (UStaticMeshComponent* Thread : Threads)
	{
		if (UMaterialInstanceDynamic* Look = Thread->CreateAndSetMaterialInstanceDynamic(0))
		{
			Look->SetVectorParameterValue(TEXT("Farbe"), Tone);
		}
	}
	if (CurtainLook != nullptr)
	{
		CurtainLook->SetVectorParameterValue(TEXT("Farbe"), PairLookColor);
		CurtainLook->SetScalarParameterValue(TEXT("Flaeche"), Sheet);
		CurtainLook->SetScalarParameterValue(TEXT("Fliessen"), Flow);
	}
	Light->SetLightColor(PairLookColor);

	// Partners that bring a body of their own: vines climbing up from the foot, roses hanging from the arch
	UStaticMesh* Part = nullptr;
	int32 Count = 0;
	if (Pair == EVaelSymbiosis::Mondranken && Other != nullptr && !Other->VineMeshes.IsEmpty())
	{
		Part = VaelAssets::LoadOptional(Other->VineMeshes[0]);
		Count = 7;
	}
	else if (Pair == EVaelSymbiosis::Mondrose && Other != nullptr)
	{
		Part = VaelAssets::LoadOptional(Other->ShoulderMesh);
		Count = 4;
	}
	UMaterialInterface* PartLook = Other != nullptr ? VaelAssets::LoadOptional(Other->Material) : nullptr;
	for (int32 Index = 0; Part != nullptr && Index < Count; ++Index)
	{
		UStaticMeshComponent* Piece = NewObject<UStaticMeshComponent>(this);
		Piece->SetMobility(EComponentMobility::Movable);
		Piece->SetStaticMesh(Part);
		Piece->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Piece->SetCanEverAffectNavigation(false);
		Piece->SetCastShadow(false);
		Piece->SetupAttachment(Root);
		Piece->RegisterComponent();
		if (PartLook != nullptr)
		{
			Piece->SetMaterial(0, PartLook);
		}
		Piece->SetVisibility(false);
		PairParts.Add(Piece);
	}
}

void AVaelMoonVeil::TickPairLook(float DeltaSeconds, float Open, float Fade)
{
	if (Pair == EVaelSymbiosis::None)
	{
		return;
	}
	const float Time = GetWorld()->GetTimeSeconds();

	// The vines climb up the veil from the foot, the roses hang along the arch and sway
	for (int32 Index = 0; Index < PairParts.Num(); ++Index)
	{
		UStaticMeshComponent* Piece = PairParts[Index];
		const float Share = (Index + 0.5f) / PairParts.Num();
		const float Y = (Share - 0.5f) * Wide * 0.9f;
		Piece->SetVisibility(Fade > 0.05f);
		if (Pair == EVaelSymbiosis::Mondranken)
		{
			const float Climb = FMath::Clamp(Age / 1.5f, 0.0f, 1.0f) * GetTop(Y) * 0.75f * Fade;
			const FRotator Lean(8.0f * FMath::Sin(Time * 1.2f + Index), 0.0f, 10.0f * FMath::Sin(Index * 2.3f));
			Piece->SetRelativeLocationAndRotation(FVector(8.0f * FMath::Sin(Index * 1.7f), Y, 0.0f), Lean);
			Piece->SetRelativeScale3D(FVector(0.9f, 0.9f, FMath::Max(Climb, 1.0f) / 100.0f));
		}
		else
		{
			const float Size = 2.2f * Open * Fade;
			Piece->SetRelativeLocationAndRotation(FVector(-25.0f, Y, GetTop(Y) - 45.0f), FRotator(180.0f + 10.0f * FMath::Sin(Time * 1.4f + Index), Index * 70.0f, 0.0f));
			Piece->SetRelativeScale3D(FVector(FMath::Max(Size, 0.01f)));
		}
	}

	// What falls, rises or glitters in the light of the veil
	PairSparkCountdown -= DeltaSeconds;
	if (PairSparkCountdown > 0.0f || Fade < 0.2f)
	{
		return;
	}
	PairSparkCountdown = 0.04f;
	const FTransform& Transform = GetActorTransform();
	const float Y = FMath::FRandRange(-0.48f, 0.48f) * Wide;
	FVaelDebris Bits;
	Bits.Count = 4;
	Bits.Spread = 30.0f;
	Bits.Speed = 30.0f;
	Bits.Lift = 0.0f;
	Bits.Gravity = 0.0f;
	Bits.Drag = 1.0f;
	Bits.Lifetime = 0.9f;
	Bits.MinSize = 2.0f;
	Bits.MaxSize = 4.5f;
	Bits.Color = PairLookColor;
	Bits.Glow = 7.0f;
	Bits.bLandOnGround = false;
	FVector Local(0.0f, Y, FMath::FRandRange(0.2f, 0.95f) * GetTop(Y));
	switch (Pair)
	{
	case EVaelSymbiosis::Glutvorhang:
		// Embers trickle down the veil
		Local.Z = GetTop(Y) * FMath::FRandRange(0.7f, 1.0f);
		Bits.Gravity = 260.0f;
		Bits.Color = FLinearColor(1.0f, 0.38f, 0.06f);
		break;
	case EVaelSymbiosis::Taustrom:
		// Drops run down and fall on the ground before it
		Local.Z = GetTop(Y) * FMath::FRandRange(0.6f, 1.0f);
		Bits.Gravity = 900.0f;
		Bits.Color = FLinearColor(0.45f, 0.85f, 1.0f);
		Bits.bLandOnGround = true;
		Bits.Glow = 3.0f;
		break;
	case EVaelSymbiosis::WehenderSchleier:
		// Streaks and bits of leaf blow forward through it
		Bits.Speed = 260.0f;
		Bits.Color = FLinearColor(0.75f, 0.95f, 0.7f);
		Bits.Glow = 2.0f;
		Bits.MaxSize = 6.0f;
		break;
	case EVaelSymbiosis::Sternenfall:
		// Little stars glitter all over it
		Bits.Count = 3;
		Bits.Lifetime = 0.5f;
		Bits.Glow = 12.0f;
		Bits.Color = FLinearColor(1.0f, 0.85f, 0.45f);
		break;
	case EVaelSymbiosis::Bernsteinschleier:
		// Drops of resin slowly sink
		Bits.Gravity = 120.0f;
		Bits.Color = FLinearColor(1.0f, 0.5f, 0.05f);
		break;
	case EVaelSymbiosis::Mondrose:
		// Rose petals drift in it
		Bits.Gravity = 40.0f;
		Bits.Glow = 2.0f;
		Bits.MaxSize = 6.0f;
		Bits.Color = FLinearColor(0.95f, 0.3f, 0.45f);
		break;
	case EVaelSymbiosis::Feentor:
		// Fairy dust rises
		Bits.Gravity = -80.0f;
		Bits.Color = FLinearColor(1.0f, 0.75f, 1.0f);
		break;
	default:
		Bits.Gravity = -30.0f;
		break;
	}
	AVaelDebrisBurst::Spawn(this, Transform.TransformPositionNoScale(Local), Bits);
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
