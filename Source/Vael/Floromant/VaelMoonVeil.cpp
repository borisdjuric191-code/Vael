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

AVaelMoonVeil* AVaelMoonVeil::Raise(APawn* InOwner, const UVaelPlantArmData* InPlant, const FVector& Ground, const FVector& Facing)
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
	Light->SetRelativeLocation(FVector(-40.0f, 0.0f, Plant->VeilHeight * 0.6f));

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
	for (int32 Index = 0; Index < Plant->VeilDrops; ++Index)
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
	const float Fade = FMath::Clamp((Plant->VeilSeconds - Age) / VeilFadeSeconds, 0.0f, 1.0f);
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
	const float Height = Plant->VeilHeight * Open;
	Curtain->SetRelativeLocation(FVector(0.0f, 0.0f, Plant->VeilHeight - Height * 0.5f));
	Curtain->SetRelativeScale3D(FVector(Plant->VeilWidth / VeilPlaneSize, FMath::Max(Height, 1.0f) / VeilPlaneSize, 1.0f));
	const float Strength = Fade * (1.0f + 1.6f * RippleStrength + 0.12f * FMath::Sin(Time * 1.7f));
	if (CurtainLook != nullptr)
	{
		CurtainLook->SetScalarParameterValue(TEXT("Staerke"), Strength);
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
		const FVector Top(0.0f, (Share - 0.5f) * Plant->VeilWidth * 0.92f, Plant->VeilHeight);
		const float Length = Plant->VeilHeight * DropLengths[Index] * Open;
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
	return FMath::Abs(Point.Y) <= Plant->VeilWidth * 0.5f && Point.Z >= -60.0f && Point.Z <= Plant->VeilHeight + 40.0f;
}

void AVaelMoonVeil::Ripple(const FVector& Location)
{
	RippleStrength = 1.0f;
	RippleLocal = GetActorTransform().InverseTransformPositionNoScale(Location);
	// The drops nearest to the point flare
	for (int32 Index = 0; Index < Drops.Num(); ++Index)
	{
		const float Y = ((Index + 0.5f) / Drops.Num() - 0.5f) * Plant->VeilWidth;
		DropFlares[Index] = FMath::Max(DropFlares[Index], FMath::Clamp(1.0f - FMath::Abs(Y - RippleLocal.Y) / 150.0f, 0.0f, 1.0f));
	}
}

void AVaelMoonVeil::StopShots()
{
	const float Reach = Plant->VeilWidth * 0.5f + 400.0f;
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
		}
	}
}

void AVaelMoonVeil::Cleanse()
{
	for (TActorIterator<AVaelCharacterBase> It(GetWorld()); It; ++It)
	{
		AVaelCharacterBase* Ally = *It;
		if (!Ally->IsOnPlayerSide() || FVector::DistSquared2D(Ally->GetActorLocation(), GetActorLocation()) > FMath::Square(Plant->VeilWidth))
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
		}
	}
	return Damage * Scale;
}
