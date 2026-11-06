// Copyright Epic Games, Inc. All Rights Reserved.

#include "Floromant/VaelPlantTower.h"
#include "Floromant/VaelFloromantComponent.h"
#include "Floromant/VaelEyeBerry.h"
#include "Floromant/VaelMoonVeil.h"
#include "World/VaelGround.h"
#include "Components/SplineMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Floromant/VaelBindweedLasso.h"
#include "Floromant/VaelFairyEscort.h"
#include "Floromant/VaelFloromantSettings.h"
#include "Floromant/VaelGlimmDome.h"
#include "Floromant/VaelPlantArmData.h"
#include "Floromant/VaelPlant.h"
#include "Floromant/VaelRoseHeart.h"
#include "Floromant/VaelSymbiosisKnotComponent.h"
#include "Magic/VaelDebrisBurst.h"
#include "Magic/VaelSpellEffects.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Player/VaelCharacter.h"
#include "UI/VaelCombatTextSubsystem.h"
#include "World/VaelSafeZone.h"
#include "Combat/VaelCombatStatics.h"
#include "Vael.h"
#include "VaelAssets.h"

namespace
{
	/** Twisted vines of the column, stalks of the crown */
	constexpr int32 TowerColumnVines = 4;
	constexpr int32 TowerCrownStalks = 5;

	/** Glutdisteln in the throat ring of the plant dragon, all fired in one drumfire */
	constexpr int32 DragonDrumHeads = 8;

	/** Seconds a tower needs to grow out of the ground, and to wither */
	constexpr float TowerGrowTime = 0.8f;
	constexpr float TowerWitherTime = 0.6f;

	/** Length of the stalk meshes along +Z, in cm (Tools/Pflanzenarm) */
	constexpr float TowerStalkMeshLength = 100.0f;

	/** Shared trunk of a symbiosis: how much taller than a tower, where on it the legs of both towers meet, how often the two
	 *  strands twist round each other above, and the pieces each vine is split into up the trunk */
	constexpr float TrunkHeightScale = 1.3f;
	constexpr float TrunkMerge = 0.3f;
	constexpr float TrunkTwist = 3.0f * UE_PI;
	constexpr int32 TrunkPieces = 3;

	/** Part of the fusion in which the towers lean in until their tops meet; the trunk already grows up meanwhile, so the crowns
	 *  hardly sink on their way into the blossom */
	constexpr float TrunkGather = 0.5f;

	float TrunkGatherAlpha(float Fuse)
	{
		return FMath::SmoothStep(0.0f, TrunkGather, Fuse);
	}

	/** How far up the trunk has grown */
	float TrunkTop(float Fuse)
	{
		return TrunkMerge + (1.0f - TrunkMerge) * FMath::SmoothStep(0.1f, 1.0f, Fuse);
	}

	/** Point and direction of the trunk's middle line at T (0 foot, 1 top), bent as a bow like a single column */
	FVector TrunkPoint(const FVector& Center, const FVector& Bend, float Length, float T)
	{
		const float Angle = Bend.Size();
		if (Angle < KINDA_SMALL_NUMBER)
		{
			return Center + FVector::UpVector * Length * T;
		}
		return Center + Bend / Angle * Length * (1.0f - FMath::Cos(Angle * T)) / Angle + FVector::UpVector * Length * FMath::Sin(Angle * T) / Angle;
	}

	FVector TrunkTangent(const FVector& Bend, float T)
	{
		const float Angle = Bend.Size();
		return Angle < KINDA_SMALL_NUMBER ? FVector::UpVector : FVector::UpVector * FMath::Cos(Angle * T) + Bend / Angle * FMath::Sin(Angle * T);
	}

	/** One vine of a tower's strand: the two strands (Phase 0 and pi) wind round the middle line, the vines of each round their
	 *  strand twice as fast; at the top they draw together into the blossom */
	FVector TrunkVine(const FVector& Center, const FVector& Bend, float Length, float T, float Phase, int32 Index, int32 Count)
	{
		const FVector Middle = TrunkPoint(Center, Bend, Length, T);
		const FQuat Tilt = FQuat::FindBetweenNormals(FVector::UpVector, TrunkTangent(Bend, T));
		const FVector U = Tilt.RotateVector(FVector::ForwardVector);
		const FVector V = Tilt.RotateVector(FVector::RightVector);
		const float Narrow = 1.0f - 0.6f * FMath::SmoothStep(0.8f, 1.0f, T);
		const float Up = T - TrunkMerge;
		const float StrandAngle = Phase + TrunkTwist * Up;
		const float VineAngle = Index * 2.0f * UE_PI / FMath::Max(Count, 1) + 2.0f * TrunkTwist * Up;
		return Middle + (U * FMath::Cos(StrandAngle) + V * FMath::Sin(StrandAngle)) * 14.0f * Narrow + (U * FMath::Cos(VineAngle) + V * FMath::Sin(VineAngle)) * 7.0f * Narrow;
	}

	/** Derivative of a trunk vine by T */
	FVector TrunkVineSlope(const FVector& Center, const FVector& Bend, float Length, float T, float Phase, int32 Index, int32 Count)
	{
		constexpr float Step = 0.01f;
		return (TrunkVine(Center, Bend, Length, T + Step, Phase, Index, Count) - TrunkVine(Center, Bend, Length, T - Step, Phase, Index, Count)) / (2.0f * Step);
	}

	/** The first mesh of the lists that can be loaded */
	UStaticMesh* TowerFirstMesh(std::initializer_list<const TArray<TSoftObjectPtr<UStaticMesh>>*> Lists, int32 Pick = 0)
	{
		for (const TArray<TSoftObjectPtr<UStaticMesh>>* List : Lists)
		{
			if (!List->IsEmpty())
			{
				if (UStaticMesh* Loaded = VaelAssets::LoadOptional((*List)[Pick % List->Num()]))
				{
					return Loaded;
				}
			}
		}
		return nullptr;
	}
}

AVaelPlantTower::AVaelPlantTower()
{
	PrimaryActorTick.bCanEverTick = true;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
}

AVaelPlantTower* AVaelPlantTower::PlantTower(AVaelCharacter* Gardener, UVaelPlantArmData* InPlant, const FVector& GroundLocation, bool bInShadow)
{
	UWorld* World = Gardener != nullptr ? Gardener->GetWorld() : nullptr;
	if (World == nullptr || InPlant == nullptr)
	{
		return nullptr;
	}

	const FTransform SpawnTransform(FRotator(0.0f, FMath::FRandRange(0.0f, 360.0f), 0.0f), GroundLocation);
	AVaelPlantTower* Tower = World->SpawnActorDeferred<AVaelPlantTower>(AVaelPlantTower::StaticClass(), SpawnTransform, Gardener, Gardener, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (Tower == nullptr)
	{
		return nullptr;
	}
	Tower->Plant = InPlant;
	Tower->bShadow = bInShadow && !InPlant->IsLightOnly();
	Tower->LifeLeft = UVaelFloromantSettings::Get()->TowerLifetime;
	Tower->LookDirection = Gardener->GetActorForwardVector();
	Tower->FinishSpawning(SpawnTransform);
	Tower->Build();

	// A Glimmklee tower doesn't fire: it holds a smaller glass dome around itself
	if (InPlant->bShield)
	{
		Tower->Dome = AVaelGlimmDome::Raise(Gardener, Tower, InPlant, EVaelSymbiosis::None, InPlant->TowerShieldRadius, Tower->bShadow);
	}

	// Earth breaks open where it goes in
	FVaelDebris Earth;
	Earth.Count = 16;
	Earth.Spread = 50.0f;
	Earth.Speed = 220.0f;
	Earth.Lift = 260.0f;
	Earth.Lifetime = 0.9f;
	Earth.MinSize = 3.0f;
	Earth.MaxSize = 8.0f;
	Earth.Color = FLinearColor(0.16f, 0.12f, 0.09f);
	AVaelDebrisBurst::Spawn(Gardener, GroundLocation, Earth);

	// A tower close by of a plant it can pair with: they grow together, the closest one first
	const UVaelFloromantSettings* Settings = UVaelFloromantSettings::Get();
	AVaelPlantTower* Closest = nullptr;
	float Best = FMath::Square(1000.0f);
	for (TActorIterator<AVaelPlantTower> It(World); It; ++It)
	{
		AVaelPlantTower* Other = *It;
		if (Other == Tower || !Other->IsStanding() || Other->Partner.IsValid() || VaelSymbiosis::Find(InPlant, Other->Plant) == EVaelSymbiosis::None)
		{
			continue;
		}
		const float Distance = FVector::DistSquared2D(Other->GetActorLocation(), GroundLocation);
		// Two Schlotwinde span a Rankenbrücke between them from much further apart (Boris 2026-11-06: up to 10 m)
		const bool bBridge = InPlant->bLasso && Other->Plant->bLasso;
		if (Distance < (bBridge ? FMath::Square(1000.0f) : FMath::Square(Settings->SymbiosisRadius)) && Distance < Best)
		{
			Best = Distance;
			Closest = Other;
		}
	}
	if (Closest != nullptr)
	{
		Tower->FuseWith(Closest);
	}
	return Tower;
}

AVaelCharacter* AVaelPlantTower::GetGardener() const
{
	return Cast<AVaelCharacter>(GetInstigator());
}

USplineMeshComponent* AVaelPlantTower::MakeStalk(UStaticMesh* Mesh)
{
	USplineMeshComponent* Stalk = NewObject<USplineMeshComponent>(this);
	Stalk->SetMobility(EComponentMobility::Movable);
	Stalk->SetForwardAxis(ESplineMeshAxis::Z, false);
	Stalk->SetStaticMesh(Mesh);
	Stalk->SetMaterial(0, Material);
	Stalk->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Stalk->SetCanEverAffectNavigation(false);
	Stalk->SetupAttachment(RootComponent);
	Stalk->RegisterComponent();
	return Stalk;
}

void AVaelPlantTower::Build()
{
	Material = UMaterialInstanceDynamic::Create(VaelAssets::LoadOptional(Plant->Material), this);
	if (Material != nullptr)
	{
		Material->SetScalarParameterValue(TEXT("Schatten"), bShadow ? 1.0f : 0.0f);
		Material->SetScalarParameterValue(TEXT("Wind"), Plant->RestWind);
	}

	// The column: the plant's thick vines twisted round each other
	for (int32 Index = 0; Index < TowerColumnVines; ++Index)
	{
		Column.Add(MakeStalk(TowerFirstMesh({ &Plant->VineMeshes, &Plant->ArmMeshes, &Plant->FingerMeshes }, Index)));
	}

	// A question-mark tower: each vine goes on over the top of the curl as its hanging neck down into the crown
	bHook = IsHookTower();
	if (bHook)
	{
		// Two pieces per vine: the neck hanging down from the curl, and the head lifting out of the dip to look straight ahead
		for (const USplineMeshComponent* Vine : Column)
		{
			HookNeck.Add(MakeStalk(Vine->GetStaticMesh()));
			HookNeck.Add(MakeStalk(Vine->GetStaticMesh()));
		}
	}

	// A Schlotwinde: its own vines that lash and catch, instead of a crown while it stands alone
	if (Plant->bLasso)
	{
		BuildBindweedTower();
	}

	// The crown: the big rose of the Rußrose with its heart, the open rose of the Taublatt, or the finger stalks with their heads
	if (UStaticMesh* RoseMesh = Plant->bVineArm ? VaelAssets::LoadOptional(Plant->ShoulderMesh) : nullptr)
	{
		UStaticMeshComponent* Rose = NewObject<UStaticMeshComponent>(this);
		Rose->SetMobility(EComponentMobility::Movable);
		Rose->SetStaticMesh(RoseMesh);
		Rose->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Rose->SetupAttachment(RootComponent);
		Rose->RegisterComponent();
		CrownPieces.Add(Rose);
		return;
	}
	if (UStaticMesh* PetalMesh = VaelAssets::LoadOptional(Plant->PetalMesh))
	{
		for (int32 Index = 0; Index < Plant->InnerPetals + Plant->OuterPetals; ++Index)
		{
			UStaticMeshComponent* Petal = NewObject<UStaticMeshComponent>(this);
			Petal->SetMobility(EComponentMobility::Movable);
			Petal->SetStaticMesh(PetalMesh);
			UMaterialInterface* PartMaterial = VaelAssets::LoadOptional(Plant->PartMaterial);
			Petal->SetMaterial(0, PartMaterial != nullptr ? PartMaterial : Material.Get());
			Petal->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			Petal->SetupAttachment(RootComponent);
			Petal->RegisterComponent();
			CrownPieces.Add(Petal);
		}
		if (UStaticMesh* CoreMesh = VaelAssets::LoadOptional(Plant->CoreMesh))
		{
			CrownCore = NewObject<UStaticMeshComponent>(this);
			CrownCore->SetMobility(EComponentMobility::Movable);
			CrownCore->SetStaticMesh(CoreMesh);
			UMaterialInterface* PartMaterial = VaelAssets::LoadOptional(Plant->PartMaterial);
			CrownCore->SetMaterial(0, PartMaterial != nullptr ? PartMaterial : Material.Get());
			CrownCore->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			CrownCore->SetupAttachment(RootComponent);
			CrownCore->RegisterComponent();
		}
		if (Plant->bFairies)
		{
			Escort = GetWorld()->SpawnActor<AVaelFairyEscort>();
			if (Escort != nullptr)
			{
				Escort->Setup(GetGardener(), Plant, 0.0f, this, 170.0f);
			}
		}
		return;
	}

	UStaticMesh* TipMesh = VaelAssets::LoadOptional(Plant->TipMesh);
	StalkCharges.Init(0.0f, TowerCrownStalks);
	StalkRegrow.Init(1.0f, TowerCrownStalks);
	StalkKicks.Init(0.0f, TowerCrownStalks);
	StalkTips.Init(GetActorLocation(), TowerCrownStalks);
	StalkDirections.Init(FVector::UpVector, TowerCrownStalks);
	UMaterialInterface* GlowMaterial = nullptr;
	if (!Plant->bShield)
	{
		UMaterialInterface* Core = nullptr;
		VaelEffects::LoadLookMaterials(Core, GlowMaterial);
	}
	for (int32 Index = 0; Index < TowerCrownStalks; ++Index)
	{
		// The glow ball the head charges in before it fires (a Glimmklee tower doesn't fire)
		if (GlowMaterial != nullptr)
		{
			UStaticMeshComponent* Orb = VaelEffects::AddLookShape(this, GlowMaterial, VaelSymbiosis::GetPlantColor(Plant, bShadow), 0.0f);
			Orb->SetUsingAbsoluteLocation(true);
			Orb->SetUsingAbsoluteRotation(true);
			Orb->SetUsingAbsoluteScale(true);
			Orb->SetVisibility(false);
			ChargeOrbs.Add(Orb);
		}
		CrownStalks.Add(MakeStalk(TowerFirstMesh({ &Plant->FingerMeshes, &Plant->ArmMeshes }, Index)));
		if (TipMesh != nullptr)
		{
			UStaticMeshComponent* Tip = NewObject<UStaticMeshComponent>(this);
			Tip->SetMobility(EComponentMobility::Movable);
			Tip->SetStaticMesh(TipMesh);
			Tip->SetMaterial(0, Material);
			Tip->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			Tip->SetupAttachment(RootComponent);
			Tip->RegisterComponent();
			CrownPieces.Add(Tip);
		}
	}
}

bool AVaelPlantTower::GetTrunk(FVector& OutCenter, FVector& OutBend, float& OutLength, float& OutFuse) const
{
	const AVaelPlantTower* Other = Partner.Get();
	if (Symbiosis == EVaelSymbiosis::None || Other == nullptr)
	{
		return false;
	}
	const AVaelPlantTower* Leader = bLeads ? this : Other;
	OutFuse = Leader->FuseGrow;
	OutCenter = (GetActorLocation() + Other->GetActorLocation()) * 0.5f;
	OutBend = Leader->Bend;
	const float Shown = FMath::Clamp(FMath::Min(Grow, Other->Grow), 0.0f, 1.0f);
	const float Grown = 1.0f + (UVaelFloromantSettings::Get()->TowerCallSizeScale - 1.0f) * FMath::Max(Giant, Other->Giant);
	OutLength = UVaelFloromantSettings::Get()->TowerHeight * TrunkHeightScale * FMath::Sin(Shown * UE_HALF_PI) * Grown;
	return OutFuse > 0.0f;
}

FVector AVaelPlantTower::GetCrown() const
{
	// Fused, the crown rides the top of the shared trunk as part of the mixed blossom
	FVector Center, TrunkBend;
	float Length = 0.0f;
	float Fuse = 0.0f;
	if (GetTrunk(Center, TrunkBend, Length, Fuse))
	{
		return FMath::Lerp(GetOwnCrown(), TrunkPoint(Center, TrunkBend, Length, TrunkTop(Fuse)), TrunkGatherAlpha(Fuse));
	}
	return GetOwnCrown();
}

FVector AVaelPlantTower::GetCrownAxis() const
{
	FVector Center, TrunkBend;
	float Length = 0.0f;
	float Fuse = 0.0f;
	if (GetTrunk(Center, TrunkBend, Length, Fuse))
	{
		return FMath::Lerp(GetOwnCrownAxis(), TrunkTangent(TrunkBend, TrunkTop(Fuse)), TrunkGatherAlpha(Fuse)).GetSafeNormal();
	}
	return GetOwnCrownAxis();
}

bool AVaelPlantTower::IsHookTower() const
{
	if (Plant == nullptr)
	{
		return false;
	}
	FString Id = Plant->GetName();
	Id.RemoveFromStart(TEXT("DA_Pflanzenarm_"));
	return UVaelFloromantSettings::Get()->HookTowerPlants.Contains(FName(*Id));
}

namespace
{
	/** Flat direction a question-mark tower hangs its crown to */
	FVector HookForward(const FVector& LookDirection)
	{
		const FVector Flat = LookDirection.GetSafeNormal2D();
		return Flat.IsNearlyZero() ? FVector::ForwardVector : Flat;
	}
}

FVector AVaelPlantTower::GetHookApex() const
{
	const float Length = UVaelFloromantSettings::Get()->TowerHeight * FMath::Sin(FMath::Clamp(Grow, 0.0f, 1.0f) * UE_HALF_PI)
		* (1.0f + (UVaelFloromantSettings::Get()->TowerCallSizeScale - 1.0f) * Giant);
	// The bend of the spring only sways the curl a little; the neck below swings further
	return GetActorLocation() + HookForward(LookDirection) * Length * 0.05f + FVector::UpVector * Length + Bend * Length * 0.25f;
}

FVector AVaelPlantTower::GetOwnCrown() const
{
	if (bHook)
	{
		// The crown hangs down in front of the curl at about half the height
		const UVaelFloromantSettings* Settings = UVaelFloromantSettings::Get();
		const float Length = Settings->TowerHeight * FMath::Sin(FMath::Clamp(Grow, 0.0f, 1.0f) * UE_HALF_PI) * (1.0f + (Settings->TowerCallSizeScale - 1.0f) * Giant);
		return GetActorLocation() + HookForward(LookDirection) * Length * Settings->HookHeadReach + FVector::UpVector * Length * Settings->HookHeadHeight + Bend * Length * 0.4f;
	}

	// The column bends as a bow of constant curve: its length stays, the crown sinks and moves out the more it bends
	const float Length = UVaelFloromantSettings::Get()->TowerHeight * FMath::Sin(FMath::Clamp(Grow, 0.0f, 1.0f) * UE_HALF_PI)
		* (1.0f + (UVaelFloromantSettings::Get()->TowerCallSizeScale - 1.0f) * Giant);
	const float Angle = Bend.Size();
	if (Angle < KINDA_SMALL_NUMBER)
	{
		return GetActorLocation() + FVector(0.0f, 0.0f, Length);
	}
	const FVector Out = Bend / Angle;
	return GetActorLocation() + Out * Length * (1.0f - FMath::Cos(Angle)) / Angle + FVector::UpVector * Length * FMath::Sin(Angle) / Angle;
}

FVector AVaelPlantTower::GetOwnCrownAxis() const
{
	if (bHook)
	{
		// The lifted head looks straight ahead, where it shoots
		return (HookForward(LookDirection) + Bend * 0.5f).GetSafeNormal();
	}
	const float Angle = Bend.Size();
	return Angle < KINDA_SMALL_NUMBER ? FVector::UpVector : (FVector::UpVector * FMath::Cos(Angle) + Bend / Angle * FMath::Sin(Angle));
}

bool AVaelPlantTower::AimsLikeHydra() const
{
	return Symbiosis == EVaelSymbiosis::None && !Plant->bShield && !Plant->bVineArm && !Plant->bMortar && !Plant->bLasso && !Plant->bFairies;
}

void AVaelPlantTower::UpdateBend(float DeltaSeconds)
{
	// A long frame must not blow the spring up
	const float Step = FMath::Min(DeltaSeconds, 0.04f);
	const float Time = GetWorld()->GetTimeSeconds();
	const float Seed = GetUniqueID() * 0.37f;

	// The crown turns towards what it fires at
	const AActor* Goal = Target.Get();
	if (Goal != nullptr)
	{
		const FVector Way = (Goal->GetActorLocation() - GetActorLocation()).GetSafeNormal2D();
		if (!Way.IsNearlyZero())
		{
			LookDirection = FMath::VInterpNormalRotationTo(LookDirection, Way, Step, 150.0f);
		}
	}
	AimBlend = FMath::FInterpTo(AimBlend, Goal != nullptr && !bWithering ? 1.0f : 0.0f, Step, 4.0f);
	BudOpen = FMath::Max(0.0f, BudOpen - Step * 1.6f);

	// Where it wants to lean: a shooter bows its neck far towards the target, a Mohnmörser only a little, shield and rose stand
	// upright and rock gently in the air; all of them wander a little so they look alive
	float Lean = 0.0f;
	float Wander = 0.05f;
	if (bHook && (bDragon || bAbsorbed))
	{
		// The dragon snake leans like a single one, a little further while it aims
		Lean = FMath::Lerp(0.0f, 0.3f, AimBlend);
	}
	else if (Symbiosis != EVaelSymbiosis::None)
	{
		// The shared trunk (bent by the leader) bows its blossom towards the target, less than a single neck
		Lean = FMath::Lerp(0.06f, 0.45f, AimBlend);
	}
	else if (bHook)
	{
		// The hanging head only reaches out a little towards its target; the curl turns with the look
		Lean = FMath::Lerp(0.0f, 0.25f, AimBlend);
	}
	else if (AimsLikeHydra())
	{
		Lean = FMath::Lerp(0.12f, 0.95f, AimBlend);
	}
	else if (Plant->bMortar)
	{
		Lean = FMath::Lerp(0.05f, 0.25f, AimBlend);
		Wander = 0.06f;
	}
	else if (Plant->bShield || Plant->bVineArm)
	{
		Wander = 0.11f;
	}
	const FVector Rock(FMath::Sin(Time * 0.9f + Seed) + 0.4f * FMath::Sin(Time * 2.1f + Seed * 2.3f), FMath::Cos(Time * 0.7f + Seed * 1.7f) + 0.4f * FMath::Sin(Time * 1.7f + Seed), 0.0f);
	const FVector Rest = LookDirection * Lean + Rock * Wander;

	// A damped spring: turning, the kick of a shot and the swing of a throw overshoot a little and settle
	const float Stiffness = 32.0f;
	const float Damping = 4.5f;
	BendVelocity += ((Rest - Bend) * Stiffness - BendVelocity * Damping) * Step;
	Bend += BendVelocity * Step;
	Bend.Z = 0.0f;
	if (Bend.Size() > 1.25f)
	{
		Bend = Bend.GetSafeNormal() * 1.25f;
	}
}

void AVaelPlantTower::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (Escort != nullptr)
	{
		Escort->Destroy();
		Escort = nullptr;
	}
	Super::EndPlay(EndPlayReason);
}

void AVaelPlantTower::Wither()
{
	if (bWithering)
	{
		return;
	}
	bWithering = true;
	if (AVaelVineWall* Wall = Bridge.Get())
	{
		Wall->WitherNow();
	}
	if (AVaelGlimmDome* Glass = Dome.Get())
	{
		Glass->Lower();
	}

	// Grown together they wither together
	if (AVaelPlantTower* Other = Partner.Get())
	{
		Other->Wither();
	}
}

void AVaelPlantTower::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (Plant == nullptr)
	{
		Destroy();
		return;
	}

	if (bWithering)
	{
		Grow -= DeltaSeconds / TowerWitherTime;
		if (Grow <= 0.0f)
		{
			Destroy();
			return;
		}
	}
	else
	{
		Grow = FMath::Min(1.0f, Grow + DeltaSeconds / TowerGrowTime);
		LifeLeft -= DeltaSeconds;

		// It withers when its time is over, when its Floromant is gone, or when the tower it grew together with is gone
		const bool bLostPartner = Symbiosis != EVaelSymbiosis::None && (!Partner.IsValid() || !Partner->IsStanding());
		if (LifeLeft <= 0.0f || GetGardener() == nullptr || bLostPartner)
		{
			Wither();
		}
	}

	if (Material != nullptr)
	{
		Material->SetScalarParameterValue(TEXT("Wuchs"), FMath::Clamp(Grow, 0.0f, 1.0f));
	}

	// The tower that leads a symbiosis grows the knot between both and acts for both; the other only stands
	const bool bActs = !bWithering && Grow >= 1.0f && (Symbiosis == EVaelSymbiosis::None || bLeads);
	if (bActs && !IsBindweedTower())
	{
		// Called up by the Turmruf it fires faster
		FireCountdown -= DeltaSeconds * (1.0f + (UVaelFloromantSettings::Get()->TowerCallFireScale - 1.0f) * Giant);
		if (FireCountdown <= 0.0f)
		{
			const UVaelFloromantSettings* Settings = UVaelFloromantSettings::Get();
			AActor* Goal = AVaelSafeZone::IsSafe(GetWorld(), GetActorLocation()) ? nullptr : VaelSymbiosis::FindTarget(GetGardener(), GetActorLocation(), Symbiosis == EVaelSymbiosis::None && Plant->bLasso ? Plant->SnareRange : Plant->ShotRange);
			Target = Goal;
			// A shooter fires only once its crown looks at the target, like a Hydra
			const FVector GoalWay = Goal != nullptr ? (Goal->GetActorLocation() - GetActorLocation()).GetSafeNormal2D() : FVector::ZeroVector;
			if (Goal != nullptr && (AimsLikeHydra() || bDragon) && (FVector::DotProduct(LookDirection, GoalWay) < 0.95f || AimBlend < 0.7f))
			{
				FireCountdown = 0.05f;
			}
			else if (Goal != nullptr)
			{
				Fire(Goal, 1.0f);
				FireCountdown = Symbiosis != EVaelSymbiosis::None ? VaelSymbiosis::GetTowerInterval(Symbiosis) : (Plant->bLasso ? Plant->SnareInterval : Plant->bVeil ? Plant->VeilSeconds + 2.0f : Plant->bEyeBerry ? Plant->EyeSeconds : Plant->FireInterval * Settings->TowerFireIntervalScale);
			}
			else
			{
				FireCountdown = 0.3f;
			}
		}
	}

	// A Rußrose tower, or the leader of a rose pair, beats with care every two seconds
	const bool bBeats = !bWithering && Grow >= 1.0f && ((Symbiosis == EVaelSymbiosis::None && Plant->bVineArm) || (VaelSymbiosis::HasRose(Symbiosis) && bLeads));
	if (bBeats)
	{
		RoseBeatCountdown -= DeltaSeconds;
		if (RoseBeatCountdown <= 0.0f)
		{
			RoseBeatCountdown = 2.0f;
			VaelSymbiosis::RoseBeat(GetGardener(), Symbiosis, GetActorLocation(), 1.0f, bShadow);
		}
	}

	// The giant form of the Turmruf grows in and fades again
	CallLeft = FMath::Max(0.0f, CallLeft - DeltaSeconds);
	Giant = FMath::FInterpTo(Giant, CallLeft > 0.0f && !bWithering ? 1.0f : 0.0f, DeltaSeconds, 3.0f);

	// A tower that sank into a dragon snake is not seen any more; it only keeps the pair alive
	if (bAbsorbed)
	{
		return;
	}

	UpdatePairCharge(DeltaSeconds);
	UpdateStalkShots(DeltaSeconds);
	UpdateLook(DeltaSeconds);
	if (bDragon)
	{
		UpdateDragon(DeltaSeconds);
	}
	if (!WindTendrils.IsEmpty())
	{
		// Both Schlotwinde of a Rankenbrücke act, each with its own vines
		UpdateBindweedTower(DeltaSeconds, IsBindweedTower() ? !bWithering && Grow >= 1.0f : bActs);
	}

	if (Knot != nullptr && Partner.IsValid())
	{
		const float Before = FuseGrow;
		FuseGrow = bWithering ? FMath::Min(FuseGrow, Grow) : FMath::Min(1.0f, FuseGrow + DeltaSeconds / UVaelFloromantSettings::Get()->SymbiosisGrowTime);

		// The moment both columns meet in the middle: a flash in the colour of the pair and a puff of both plants' pieces
		if (Before < TrunkGather && FuseGrow >= TrunkGather)
		{
			Knot->Flash();
			FVaelDebris Burst;
			Burst.Count = 22;
			Burst.Spread = 30.0f;
			Burst.Speed = 260.0f;
			Burst.Lift = 180.0f;
			Burst.Lifetime = 1.1f;
			Burst.MinSize = 3.0f;
			Burst.MaxSize = 7.0f;
			Burst.Color = VaelSymbiosis::GetColor(Symbiosis);
			AVaelDebrisBurst::Spawn(GetGardener(), GetCrown(), Burst);
		}

		// The knot is the glowing heart in the middle of the mixed blossom; its own vines are not needed any more
		const FVector Heart = GetCrown() + GetCrownAxis() * 8.0f;
		Knot->Update(Heart, Heart, Heart, FuseGrow);
	}
}

void AVaelPlantTower::FuseWith(AVaelPlantTower* Other)
{
	const UVaelFloromantSettings* Settings = UVaelFloromantSettings::Get();
	Symbiosis = VaelSymbiosis::Find(Plant, Other->Plant);
	if (Symbiosis == EVaelSymbiosis::None)
	{
		return;
	}
	if (AVaelCharacter* Gardener = GetGardener(); Gardener != nullptr && Gardener->GetFloromant() != nullptr)
	{
		Gardener->GetFloromant()->LearnPair(Symbiosis);
	}

	Partner = Other;
	Other->Partner = this;
	Other->Symbiosis = Symbiosis;
	bLeads = true;
	Other->bLeads = false;

	// Together they stand longer
	LifeLeft = FMath::Max(LifeLeft, Other->LifeLeft) + Settings->SymbiosisLifetimeBonus;
	Other->LifeLeft = LifeLeft;
	FireCountdown = Settings->SymbiosisGrowTime;

	// Two Glutdistel snakes (Boris 2026-11-06): the new one sinks into the ground, its glow runs through the old snake up to the
	// head, and the head grows into a plant dragon that fires a drumfire of thistles out of its jaws
	if (Symbiosis == EVaelSymbiosis::Glutbusch && bHook && Other->bHook && !Other->bDragon)
	{
		bLeads = false;
		Other->bLeads = true;
		bAbsorbed = true;
		SetActorHiddenInGame(true);
		Other->FeedFrom = GetActorLocation();
		Other->BuildDragon();
		Other->FireCountdown = 1.8f;
		if (AVaelCharacter* Gardener = GetGardener())
		{
			UVaelCombatTextSubsystem::PostPickup(Gardener, FText::Format(NSLOCTEXT("VaelSymbiosis", "TowersFuse", "Symbiose: {0}"), VaelSymbiosis::GetName(Symbiosis)), VaelSymbiosis::GetColor(Symbiosis));
		}
		return;
	}

	// Two Schlotwinde (Boris 2026-11-06): no shared crown; both stay as they are and span a living fence between them, the
	// Rankenbrücke: it holds enemies back and entangles them, the vines of both pull enemies against it and lash those caught
	if (Symbiosis == EVaelSymbiosis::Windenzaun)
	{
		const FVector A = GetActorLocation();
		const FVector B = Other->GetActorLocation();
		// Always to be seen: at least 2.6 m long, also when the towers stand right beside each other (then across her view)
		const float Span = FVector::Dist2D(A, B);
		FVector Way = (B - A).GetSafeNormal2D();
		if (Span < 40.0f)
		{
			const AVaelCharacter* Gardener = GetGardener();
			Way = Gardener != nullptr ? Gardener->GetActorRightVector().GetSafeNormal2D() : FVector::RightVector;
		}
		const float LengthScale = FMath::Max(Span + 60.0f, 260.0f) / FMath::Max(Settings->WallLength, 1.0f);
		AVaelVineWall* Wall = AVaelVineWall::Raise(GetWorld(), (A + B) * 0.5f, Way.Rotation().Yaw - 90.0f, LifeLeft, 1.1f, GetGardener(), Plant, bShadow, LengthScale);
		Bridge = Wall;
		Other->Bridge = Wall;
		if (AVaelCharacter* Gardener = GetGardener())
		{
			UVaelCombatTextSubsystem::PostPickup(Gardener, FText::Format(NSLOCTEXT("VaelSymbiosis", "TowersFuse", "Symbiose: {0}"), NSLOCTEXT("VaelSymbiosis", "Rankenbruecke", "Rankenbrücke")), VaelSymbiosis::GetColor(Symbiosis));
		}
		return;
	}

	// A pair with a Glimmklee raises one dome of the pair over both towers instead of the single domes
	if (VaelSymbiosis::HasDome(Symbiosis))
	{
		for (AVaelPlantTower* Tower : { this, Other })
		{
			if (AVaelGlimmDome* Glass = Tower->Dome.Get())
			{
				Glass->Lower();
			}
			Tower->Dome = nullptr;
		}
		const UVaelPlantArmData* Glimm = Plant->bShield ? Plant.Get() : Other->Plant.Get();
		const float Radius = Symbiosis == EVaelSymbiosis::Lichtkuppel ? 400.0f : 300.0f;
		Dome = AVaelGlimmDome::Raise(GetGardener(), this, Glimm, Symbiosis, Radius, bShadow);
		if (AVaelGlimmDome* Glass = Dome.Get())
		{
			Glass->SetAnchored(1.0f);
		}
	}

	// Each tower gets the pieces its vines twist up the shared trunk with
	for (AVaelPlantTower* Tower : { this, Other })
	{
		for (const USplineMeshComponent* Vine : Tower->Column)
		{
			for (int32 Piece = 0; Piece < TrunkPieces; ++Piece)
			{
				USplineMeshComponent* Strand = Tower->MakeStalk(Vine->GetStaticMesh());
				Strand->SetVisibility(false);
				Tower->Braid.Add(Strand);
			}
		}
	}

	// The sparks that run into the heart of the blossom while the pair charges
	{
		UMaterialInterface* Core = nullptr;
		UMaterialInterface* GlowMaterial = nullptr;
		VaelEffects::LoadLookMaterials(Core, GlowMaterial);
		for (int32 Index = 0; Index < 10; ++Index)
		{
			UStaticMeshComponent* Orb = VaelEffects::AddLookShape(this, GlowMaterial, VaelSymbiosis::GetColor(Symbiosis), 0.0f);
			Orb->SetUsingAbsoluteLocation(true);
			Orb->SetUsingAbsoluteRotation(true);
			Orb->SetUsingAbsoluteScale(true);
			Orb->SetVisibility(false);
			StreamOrbs.Add(Orb);
		}
	}

	Knot = NewObject<UVaelSymbiosisKnotComponent>(this, TEXT("SymbiosisKnot"));
	Knot->SetupAttachment(RootComponent);
	Knot->RegisterComponent();
	Knot->Setup(Plant, Other->Plant, VaelSymbiosis::GetColor(Symbiosis), bShadow);

	if (AVaelCharacter* Gardener = GetGardener())
	{
		UVaelCombatTextSubsystem::PostPickup(Gardener, FText::Format(NSLOCTEXT("VaelSymbiosis", "TowersFuse", "Symbiose: {0}"), VaelSymbiosis::GetName(Symbiosis)), VaelSymbiosis::GetColor(Symbiosis));
	}
	UE_LOG(LogVael, Log, TEXT("Towers '%s' and '%s' grow together: %s"), *GetNameSafe(this), *GetNameSafe(Other), *VaelSymbiosis::GetName(Symbiosis).ToString());
}

void AVaelPlantTower::FireNow(float Power)
{
	if (bWithering || Grow < 1.0f)
	{
		return;
	}
	// The tower the other leads passes the wave on to it
	if (Symbiosis != EVaelSymbiosis::None && !bLeads)
	{
		if (AVaelPlantTower* Leader = Partner.Get())
		{
			Leader->FireNow(Power);
		}
		return;
	}
	// A rose beats at once, stronger
	if ((Symbiosis == EVaelSymbiosis::None && Plant->bVineArm) || VaelSymbiosis::HasRose(Symbiosis))
	{
		VaelSymbiosis::RoseBeat(GetGardener(), Symbiosis, GetActorLocation(), Power, bShadow);
	}
	if (AActor* Goal = VaelSymbiosis::FindTarget(GetGardener(), GetActorLocation(), Plant->ShotRange))
	{
		Target = Goal;
		Fire(Goal, Power);
	}
}

void AVaelPlantTower::Fire(AActor* Goal, float Power)
{
	AVaelCharacter* Gardener = GetGardener();
	if (Gardener == nullptr || Goal == nullptr)
	{
		return;
	}

	// A Glimmklee or Rußrose tower alone doesn't shoot: it holds its dome or beats with care
	if (Symbiosis == EVaelSymbiosis::None && (Plant->bShield || Plant->bVineArm))
	{
		return;
	}

	// A Feenragwurz tower sends a fairy of its ring at the enemy; she comes back and rests
	if (Symbiosis == EVaelSymbiosis::None && Plant->bFairies)
	{
		if (Escort != nullptr && Escort->Send(Goal, bShadow))
		{
			VaelSymbiosis::MuzzlePuff(this, Plant, GetCrown(), bShadow, 1.2f);
		}
		return;
	}

	// Windenfalle: tendrils crawl over the ground from the foot to the enemies around and hold them a while
	if (Symbiosis == EVaelSymbiosis::None && Plant->bLasso)
	{
		FVaelLassoSpec Spec;
		Spec.HoldSeconds = Plant->SnareHold;
		Spec.bOverGround = true;
		Spec.DamagePerSecond = bShadow ? Plant->StrangleDamagePerSecond : 0.0f;
		Spec.DamageGrowth = bShadow ? Plant->StrangleGrowth : 0.0f;
		Spec.Power = Power;
		Spec.Color = bShadow ? Plant->StrangleColor : Plant->PollenColor;
		for (AActor* Caught : AVaelBindweedLasso::FindTargets(Gardener, GetActorLocation(), FVector::ZeroVector, Plant->SnareRange, 180.0f, Plant->SnareTargets))
		{
			AVaelBindweedLasso::Throw(Gardener, Plant, GetActorLocation() + FVector(0.0f, 0.0f, 5.0f), Caught, Spec, bShadow);
		}
		BendVelocity -= LookDirection * 1.5f;
		VaelSymbiosis::MuzzlePuff(this, Plant, GetCrown(), bShadow, 1.5f);
		return;
	}

	// The plant dragon opens its jaws and the thistle ring in its throat fires round and round (UpdateDragon)
	if (bDragon)
	{
		BurstLeft = DragonDrumHeads;
		BurstTimer = 0.0f;
		BurstPower = Power;
		BurstTarget = Goal;
		return;
	}

	// A pair with a Glimmklee works through its dome, one with a Rußrose through its heartbeat; the other plant fires its own
	// shots out of the knot, the burning glass hotter, and the seeds of the Aschenrose heal the allies near the tower
	if ((VaelSymbiosis::HasDome(Symbiosis) || VaelSymbiosis::HasRose(Symbiosis)) && Partner.IsValid())
	{
		const UVaelPlantArmData* Other = Partner->Plant.Get();
		const UVaelPlantArmData* Shooter = !Plant->bShield && !Plant->bVineArm ? Plant.Get() : (Other != nullptr && !Other->bShield && !Other->bVineArm ? Other : nullptr);
		if (Shooter == nullptr)
		{
			return;
		}
		const FVector Middle = GetCrown() + GetCrownAxis() * 25.0f;
		const float Hotter = Symbiosis == EVaelSymbiosis::Glutglas ? 1.5f : 1.0f;
		VaelSymbiosis::FireShot(Gardener, Shooter, Middle, (Goal->GetActorLocation() - Middle).GetSafeNormal(), Power * Hotter, bShadow, UVaelFloromantSettings::Get()->TowerShotScale, 1.0f, FVector::Dist2D(Middle, Goal->GetActorLocation()));
		if (Symbiosis == EVaelSymbiosis::Aschenrose && !bShadow)
		{
			VaelRoseCare::HealAround(Gardener, GetActorLocation(), 600.0f, 0.02f * Power);
		}
		if (Knot != nullptr)
		{
			Knot->Flash();
			BendVelocity -= LookDirection * 1.2f;
			VaelSymbiosis::MuzzlePuff(this, Plant, Middle, bShadow, 1.6f);
			VaelSymbiosis::MuzzlePuff(this, Partner->Plant, Middle, bShadow, 1.6f);
		}
		return;
	}

	if (Symbiosis != EVaelSymbiosis::None && Partner.IsValid())
	{
		// The pair acts out of the mixed blossom on top of the shared trunk
		const FVector Middle = GetCrown() + GetCrownAxis() * 25.0f;
		const FVector Way = Goal->GetActorLocation() - Middle;
		VaelSymbiosis::Fire(Gardener, Symbiosis, Plant, Partner->Plant, Middle, Way, Power, 0.0f, bShadow, Way.Size2D(), GetVeilScale());
		if (Knot != nullptr)
		{
			Knot->Flash();
			BendVelocity -= LookDirection * 1.2f;
			VaelSymbiosis::MuzzlePuff(this, Plant, Middle, bShadow, 1.6f);
			VaelSymbiosis::MuzzlePuff(this, Partner->Plant, Middle, bShadow, 1.6f);
		}
		return;
	}

	// A Mondtraufe tower raises its veil between itself and the enemy, a Wachbeere tower throws an eye berry at it
	if (Symbiosis == EVaelSymbiosis::None && Plant->bVeil)
	{
		const FVector Way = (Goal->GetActorLocation() - GetActorLocation()).GetSafeNormal2D();
		FVector Place = GetActorLocation() + Way * FMath::Min(FVector::Dist2D(Goal->GetActorLocation(), GetActorLocation()) * 0.5f, 400.0f);
		FHitResult Ground;
		if (VaelGround::TraceGround(GetWorld(), Place + FVector(0.0f, 0.0f, 400.0f), Place - FVector(0.0f, 0.0f, 1200.0f), Ground, this))
		{
			Place = Ground.ImpactPoint;
		}
		AVaelMoonVeil::Raise(Gardener, Plant, Place, Way);
		VaelSymbiosis::MuzzlePuff(this, Plant, GetCrown(), bShadow, 1.5f);
		return;
	}
	if (Symbiosis == EVaelSymbiosis::None && Plant->bEyeBerry)
	{
		FVector Place = Goal->GetActorLocation();
		FHitResult Ground;
		if (VaelGround::TraceGround(GetWorld(), Place + FVector(0.0f, 0.0f, 300.0f), Place - FVector(0.0f, 0.0f, 800.0f), Ground, this))
		{
			Place = Ground.ImpactPoint;
		}
		AVaelEyeBerry::Throw(Gardener, Plant, GetCrown(), Place, bShadow);
		VaelSymbiosis::MuzzlePuff(this, Plant, GetCrown(), bShadow, 1.3f);
		return;
	}

	// The shot leaves the head whose turn it is, which flies off as the shot; without stalks the crown where it looks
	const FVector Axis = GetCrownAxis();
	FVector Muzzle = GetCrown() + Axis * (CrownCore != nullptr ? Plant->CoreLength * 1.8f : 20.0f);
	if (StalkTips.IsValidIndex(NextStalk) && !CrownStalks.IsEmpty())
	{
		Muzzle = StalkTips[NextStalk];
		StalkRegrow[NextStalk] = 0.0f;
		StalkKicks[NextStalk] = 1.0f;
		StalkCharges[NextStalk] = 0.0f;
		VaelSymbiosis::MuzzlePuff(this, Plant, Muzzle, bShadow, 1.3f);
		NextStalk = (NextStalk + 1) % StalkTips.Num();
	}
	else if (Plant->bMortar)
	{
		VaelSymbiosis::MuzzlePuff(this, Plant, Muzzle, bShadow, 1.6f);
	}
	FVector Toward = Goal->GetActorLocation() - Muzzle;
	// A question-mark tower shoots flat and straight out of its hanging head like the arms, not down from above
	if (bHook)
	{
		Toward.Z = 0.0f;
	}
	const FVector Spread = FRotator(0.0f, FMath::FRandRange(-Plant->ShotSpread, Plant->ShotSpread), 0.0f).RotateVector(Toward);
	const UVaelFloromantSettings* Settings = UVaelFloromantSettings::Get();
	// A Mohnmörser throws its capsule to where the enemy stands
	VaelSymbiosis::FireShot(Gardener, Plant, Muzzle, Spread.GetSafeNormal(), Power, bShadow, Settings->TowerShotScale, 1.0f, Spread.Size2D());

	// A shooter's neck kicks back; a Mohnmörser swings forward with the throw, opens its bud and rocks back and forth
	if (Plant->bMortar)
	{
		BendVelocity += LookDirection * 5.5f;
		BudOpen = 1.0f;
	}
	else
	{
		BendVelocity -= LookDirection * FMath::Clamp(2.0f * Plant->FireInterval, 0.6f, 2.5f);
	}
}

void AVaelPlantTower::UpdateStalkShots(float DeltaSeconds)
{
	if (StalkCharges.IsEmpty())
	{
		return;
	}
	const float Interval = FMath::Max(Plant->FireInterval * UVaelFloromantSettings::Get()->TowerFireIntervalScale, 0.05f);
	const float RegrowSeconds = FMath::Max(Interval * StalkCharges.Num() * 0.8f, 0.3f);
	const bool bAiming = AimsLikeHydra() && !bWithering && Grow >= 1.0f && Target.IsValid();
	for (int32 Index = 0; Index < StalkCharges.Num(); ++Index)
	{
		StalkRegrow[Index] = FMath::Min(1.0f, StalkRegrow[Index] + DeltaSeconds / RegrowSeconds);
		StalkKicks[Index] = FMath::Max(0.0f, StalkKicks[Index] - DeltaSeconds * 6.0f);

		// The head whose turn it is charges up over the pause before its shot; in the blossom all heads charge with the pair
		const AVaelPlantTower* Leader = Symbiosis != EVaelSymbiosis::None ? (bLeads ? this : Partner.Get()) : nullptr;
		const float Shared = Leader != nullptr ? Leader->PairCharge * 0.8f : 0.0f;
		const float Goal = FMath::Max(Shared, bAiming && Index == NextStalk ? FMath::Clamp(1.0f - FireCountdown / Interval, 0.25f, 1.0f) : 0.0f);
		StalkCharges[Index] = FMath::FInterpTo(StalkCharges[Index], Goal, DeltaSeconds, Goal > StalkCharges[Index] ? 20.0f : 10.0f);

		if (UStaticMeshComponent* Orb = ChargeOrbs.IsValidIndex(Index) ? ChargeOrbs[Index].Get() : nullptr)
		{
			const float Glow = StalkCharges[Index];
			Orb->SetVisibility(Glow > 0.03f);
			if (Glow > 0.03f)
			{
				Orb->SetWorldLocation(StalkTips[Index] + StalkDirections[Index] * FMath::Min(Plant->TipSize, 12.0f) * 0.6f);
				Orb->SetWorldScale3D(FVector(FMath::Min(Plant->TipSize * 1.6f, 16.0f) * Glow / 100.0f));
				VaelEffects::SetLookGlow(Orb, 4.0f * Glow);
			}
		}
	}
}

void AVaelPlantTower::UpdatePairCharge(float DeltaSeconds)
{
	AVaelPlantTower* Other = Partner.Get();
	if (!bLeads || Other == nullptr || Symbiosis == EVaelSymbiosis::None)
	{
		return;
	}

	// It charges over the last stretch before the pair acts, and falls off after
	const float Lead = FMath::Min(VaelSymbiosis::GetTowerInterval(Symbiosis), 0.9f);
	const bool bReady = !bWithering && FuseGrow >= 1.0f && Target.IsValid();
	const float Goal = bReady ? FMath::Clamp(1.0f - FireCountdown / Lead, 0.0f, 1.0f) : 0.0f;
	PairCharge = FMath::FInterpTo(PairCharge, Goal, DeltaSeconds, Goal > PairCharge ? 14.0f : 8.0f);
	if (Knot != nullptr)
	{
		Knot->SetCharge(PairCharge);
	}

	// Where the light comes from: the heads of the crown stalks of both towers, or a ring round the blossom for roses and petals
	const FVector Heart = GetCrown() + GetCrownAxis() * 8.0f;
	TArray<FVector, TInlineAllocator<12>> Sources;
	for (const AVaelPlantTower* Tower : { static_cast<const AVaelPlantTower*>(this), static_cast<const AVaelPlantTower*>(Other) })
	{
		if (!Tower->StalkTips.IsEmpty() && !Tower->CrownStalks.IsEmpty())
		{
			Sources.Append(Tower->StalkTips);
		}
		else
		{
			for (int32 Index = 0; Index < 4; ++Index)
			{
				const float Angle = Index * UE_HALF_PI + (Tower == this ? 0.0f : 0.8f);
				Sources.Add(Heart + FVector(FMath::Cos(Angle), FMath::Sin(Angle), -0.3f) * 30.0f);
			}
		}
	}

	// Sparks run from them into the heart, faster the fuller the charge
	const float Time = GetWorld()->GetTimeSeconds();
	for (int32 Index = 0; Index < StreamOrbs.Num(); ++Index)
	{
		UStaticMeshComponent* Orb = StreamOrbs[Index];
		Orb->SetVisibility(PairCharge > 0.05f && !Sources.IsEmpty());
		if (PairCharge <= 0.05f || Sources.IsEmpty())
		{
			continue;
		}
		const float Way = FMath::Frac(Time * (1.2f + 1.5f * PairCharge) + Index * 0.37f);
		const FVector Source = Sources[Index % Sources.Num()];
		// A little arc outwards on the way in
		const FVector Bow = (Source - Heart).GetSafeNormal() * 10.0f * FMath::Sin(Way * UE_PI);
		Orb->SetWorldLocation(FMath::Lerp(Source, Heart, Way * Way) + Bow);
		Orb->SetWorldScale3D(FVector((2.0f + 3.0f * (1.0f - Way)) * PairCharge / 100.0f));
		VaelEffects::SetLookGlow(Orb, 5.0f * PairCharge);
	}
}

void AVaelPlantTower::UpdateLook(float DeltaSeconds)
{
	UpdateBend(DeltaSeconds);

	const FVector Base = GetActorLocation();
	const float Shown = FMath::Clamp(Grow, 0.0f, 1.0f);
	const FVector Crown = GetCrown();
	const float Height = FMath::Max(UVaelFloromantSettings::Get()->TowerHeight * FMath::Sin(Shown * UE_HALF_PI) * (1.0f + (UVaelFloromantSettings::Get()->TowerCallSizeScale - 1.0f) * Giant), 1.0f);
	const float Time = GetWorld()->GetTimeSeconds();

	// Fused: the shared trunk, how far the column has leaned in (Gather) and how far the twisted trunk has grown up (Top)
	FVector TrunkCenter, TrunkBend;
	float TrunkLength = 0.0f;
	float Fuse = 0.0f;
	const bool bFused = GetTrunk(TrunkCenter, TrunkBend, TrunkLength, Fuse);
	const float Gather = bFused ? TrunkGatherAlpha(Fuse) : 0.0f;
	const float Top = TrunkTop(Fuse);
	const float Phase = bLeads ? 0.0f : UE_PI;

	// The crown grows bigger in the mixed blossom
	// The crown of a dragon snake withers away while the dragon head grows out of it
	const float Bloom = Shown * (1.0f + 0.3f * Gather) * (1.0f + 0.3f * Giant) * (bDragon ? FMath::Max(0.02f, 1.0f - 2.0f * DragonGrow) : 1.0f);

	// The frame of the crown turns smoothly with the bend of the column (no flip when it stands upright); in the blossom the
	// second crown turns half a step so the pieces of both plants sit between each other
	const FVector Axis = GetCrownAxis();
	const FQuat Tilt = FQuat::FindBetweenNormals(FVector::UpVector, Axis) * FQuat(FVector::UpVector, bLeads ? 0.0f : 0.6f * Gather);
	const FVector U = Tilt.RotateVector(FVector::ForwardVector);
	const FVector V = Tilt.RotateVector(FVector::RightVector);

	// Column: the vines wind round each other up to the crown, following the bow of the bend; fused, they lean in to the foot of
	// the shared trunk and twist up it round the vines of the other tower
	const FTransform& Transform = GetActorTransform();
	for (int32 Index = 0; Index < Column.Num(); ++Index)
	{
		const float A0 = Index * 2.0f * UE_PI / Column.Num() + GetUniqueID() * 0.3f;
		const float A1 = A0 + UE_PI * 0.8f;
		const FVector Out0(FMath::Cos(A0), FMath::Sin(A0), 0.0f);
		const FVector Out1 = U * FMath::Cos(A1) + V * FMath::Sin(A1);
		const FVector Around0(-FMath::Sin(A0), FMath::Cos(A0), 0.0f);
		const FVector Around1 = V * FMath::Cos(A1) - U * FMath::Sin(A1);
		const FVector Start = Base + Out0 * 16.0f - FVector(0.0f, 0.0f, 6.0f);
		FVector StartTangent = FVector::UpVector * Height * 1.1f + Around0 * Height * 0.5f;
		FVector End = Crown + Out1 * 7.0f;
		FVector EndTangent = Axis * Height * 0.9f + Around1 * Height * 0.4f;
		float EndScale = 1.8f;

		// A question-mark tower, bent like a rearing snake (Boris 2026-11-06): the column rises bulging back, turns over forward at
		// the top of the curl, the neck hangs down to the dip, and from there the head lifts again so the plant looks straight
		// ahead out of it; the curl and the dip sway sideways against each other like a snake
		const bool bHookShape = bHook && HookNeck.IsValidIndex(Index * 2 + 1);
		FVector NeckEnd = End;
		FVector Dip = End;
		FVector DipTangent = EndTangent;
		FVector HeadTangent = EndTangent;
		if (bHookShape)
		{
			const FVector Forward = HookForward(LookDirection);
			const FVector Side(-Forward.Y, Forward.X, 0.0f);
			const float Sway = Time * 0.9f + GetUniqueID() * 0.37f;
			const FQuat ApexTilt = FQuat::FindBetweenNormals(FVector::UpVector, Forward);
			const FVector ApexOut = ApexTilt.RotateVector(FVector(FMath::Cos(A1), FMath::Sin(A1), 0.0f));
			const FVector ApexAround = ApexTilt.RotateVector(FVector(-FMath::Sin(A1), FMath::Cos(A1), 0.0f));
			End = GetHookApex() + Side * FMath::Sin(Sway) * Height * 0.07f + ApexOut * 9.0f;
			EndTangent = Forward * Height * 0.7f + ApexAround * Height * 0.2f;
			StartTangent = FVector::UpVector * Height * 1.2f - Forward * Height * 0.5f + Around0 * Height * 0.3f;
			EndScale = 2.3f;

			// The dip: the lowest point of the hanging neck, a little in front of and below the head
			Dip = Base + Forward * Height * 0.42f + FVector::UpVector * Height * 0.32f + Bend * Height * 0.35f
				+ Side * FMath::Sin(Sway - 1.4f) * Height * 0.08f + Out1 * 8.0f;
			DipTangent = (Forward * 0.55f - FVector::UpVector + Bend * 0.4f).GetSafeNormal() * Height * 0.55f + Around1 * Height * 0.1f;

			// The head arrives level, so the crown on it looks straight out
			HeadTangent = Axis * Height * 0.45f;

			// The first vine stands for the middle line (the dragon's sparks run up it)
			if (Index == 0)
			{
				HookPoints[0] = Start;
				HookPoints[1] = End;
				HookPoints[2] = Dip;
				HookPoints[3] = NeckEnd;
				HookTangents[0] = StartTangent;
				HookTangents[1] = EndTangent;
				HookTangents[2] = DipTangent;
				HookTangents[3] = HeadTangent;
			}
		}

		if (bFused)
		{
			const FVector Merge = TrunkVine(TrunkCenter, TrunkBend, TrunkLength, TrunkMerge, Phase, Index, Column.Num());
			const float Span = FVector::Dist(Start, Merge);
			End = FMath::Lerp(End, Merge, Gather);
			StartTangent = FMath::Lerp(StartTangent, (FVector::UpVector * 0.8f + (Merge - Start).GetSafeNormal2D() * 0.5f) * Span, Gather);
			EndTangent = FMath::Lerp(EndTangent, TrunkVineSlope(TrunkCenter, TrunkBend, TrunkLength, TrunkMerge, Phase, Index, Column.Num()).GetSafeNormal() * Span, Gather);
			EndScale = FMath::Lerp(1.8f, 2.4f, Gather);
		}

		USplineMeshComponent* Vine = Column[Index];
		Vine->SetStartAndEnd(Transform.InverseTransformPosition(Start), Transform.InverseTransformVectorNoScale(StartTangent),
			Transform.InverseTransformPosition(End), Transform.InverseTransformVectorNoScale(EndTangent), false);
		Vine->SetStartScale(FVector2D(3.0f), false);
		Vine->SetEndScale(FVector2D(EndScale), false);
		Vine->SetStartRoll(A0, false);
		Vine->SetEndRoll(A1 + 2.0f, false);
		Vine->UpdateMesh();

		if (bHookShape)
		{
			// Fused, neck and head draw into the shared trunk and are gone once both columns have met
			const float NeckShown = 1.0f - Gather;
			const FVector DipAt = FMath::Lerp(Dip, End, Gather);
			const FVector HeadAt = FMath::Lerp(NeckEnd, End, Gather);
			const float Thin = FMath::Max(NeckShown, 0.2f);
			const FVector Points[3] = { End, DipAt, HeadAt };
			const FVector Tangents[3] = { EndTangent, DipTangent * NeckShown, HeadTangent * NeckShown };
			const float Scales[3] = { EndScale, 2.0f, 1.7f };
			for (int32 Piece = 0; Piece < 2; ++Piece)
			{
				USplineMeshComponent* Neck = HookNeck[Index * 2 + Piece];
				Neck->SetVisibility(NeckShown > 0.02f);
				if (NeckShown <= 0.02f)
				{
					continue;
				}
				Neck->SetStartAndEnd(Transform.InverseTransformPosition(Points[Piece]), Transform.InverseTransformVectorNoScale(Tangents[Piece]),
					Transform.InverseTransformPosition(Points[Piece + 1]), Transform.InverseTransformVectorNoScale(Tangents[Piece + 1]), false);
				Neck->SetStartScale(FVector2D(Scales[Piece] * Thin), false);
				Neck->SetEndScale(FVector2D(Scales[Piece + 1] * Thin), false);
				Neck->SetStartRoll(A1 + 2.0f + Piece * 1.5f, false);
				Neck->SetEndRoll(A1 + 3.5f + Piece * 1.5f, false);
				Neck->UpdateMesh();
			}
		}

		// Up the trunk, in pieces, as far as it has grown
		for (int32 Piece = 0; Piece < TrunkPieces; ++Piece)
		{
			if (!Braid.IsValidIndex(Index * TrunkPieces + Piece))
			{
				break;
			}
			USplineMeshComponent* Strand = Braid[Index * TrunkPieces + Piece];
			const bool bShow = bFused && Top > TrunkMerge + 0.01f;
			Strand->SetVisibility(bShow);
			if (!bShow)
			{
				continue;
			}
			const float T0 = TrunkMerge + (Top - TrunkMerge) * Piece / TrunkPieces;
			const float T1 = TrunkMerge + (Top - TrunkMerge) * (Piece + 1) / TrunkPieces;
			const FVector P0 = TrunkVine(TrunkCenter, TrunkBend, TrunkLength, T0, Phase, Index, Column.Num());
			const FVector P1 = TrunkVine(TrunkCenter, TrunkBend, TrunkLength, T1, Phase, Index, Column.Num());
			const FVector S0 = TrunkVineSlope(TrunkCenter, TrunkBend, TrunkLength, T0, Phase, Index, Column.Num()) * (T1 - T0);
			const FVector S1 = TrunkVineSlope(TrunkCenter, TrunkBend, TrunkLength, T1, Phase, Index, Column.Num()) * (T1 - T0);
			Strand->SetStartAndEnd(Transform.InverseTransformPosition(P0), Transform.InverseTransformVectorNoScale(S0),
				Transform.InverseTransformPosition(P1), Transform.InverseTransformVectorNoScale(S1), false);
			Strand->SetStartScale(FVector2D(FMath::Lerp(2.4f, 1.3f, (T0 - TrunkMerge) / (1.0f - TrunkMerge))), false);
			Strand->SetEndScale(FVector2D(FMath::Lerp(2.4f, 1.3f, (T1 - TrunkMerge) / (1.0f - TrunkMerge))), false);
			Strand->SetStartRoll(A0 + Piece * 2.0f, false);
			Strand->SetEndRoll(A0 + Piece * 2.0f + 2.0f, false);
			Strand->UpdateMesh();
		}
	}

	// Crown of stalks: they fan up and out of the top, each ending in its head
	for (int32 Index = 0; Index < CrownStalks.Num(); ++Index)
	{
		const float Angle = Index * 2.0f * UE_PI / CrownStalks.Num() + 0.4f;
		const FVector Out = U * FMath::Cos(Angle) + V * FMath::Sin(Angle);
		// Aiming, the stalks draw together into a bundle towards the target like the fingers of the hand; the one whose turn it
		// is draws back while it charges and snaps forward with its shot
		const float Fan = FMath::Lerp(bHook ? 0.45f : 0.65f, 0.1f, AimBlend);
		// The hanging head of a question-mark tower fans its stalks ahead and upwards, never into the ground
		const FVector Lift = bHook ? FVector::UpVector * 0.15f * (1.0f - AimBlend) : FVector::ZeroVector;
		const FVector Direction = (Axis * 0.75f + Out * Fan + Lift + Out * 0.06f * FMath::Sin(Time * 1.3f + Index)).GetSafeNormal();
		const float Pull = StalkCharges.IsValidIndex(Index) ? StalkKicks[Index] * 0.2f - StalkCharges[Index] * 0.15f : 0.0f;
		const float Length = Plant->FingerLength * 1.3f * Bloom * (1.0f + Pull);
		const FVector End = Crown + Direction * Length;
		USplineMeshComponent* Stalk = CrownStalks[Index];
		Stalk->SetStartAndEnd(Transform.InverseTransformPosition(Crown), Transform.InverseTransformVectorNoScale(Axis * Length),
			Transform.InverseTransformPosition(End), Transform.InverseTransformVectorNoScale(Direction * Length), false);
		Stalk->SetStartScale(FVector2D(Plant->FingerThickness * 1.8f), false);
		Stalk->SetEndScale(FVector2D(Plant->FingerThickness * 1.4f), false);
		Stalk->UpdateMesh();
		if (StalkTips.IsValidIndex(Index))
		{
			StalkTips[Index] = End;
			StalkDirections[Index] = Direction;
		}
		if (CrownPieces.IsValidIndex(Index))
		{
			// The head swells while it charges; after its shot it is gone and grows again
			const float Regrown = StalkRegrow.IsValidIndex(Index) ? FMath::SmoothStep(0.0f, 1.0f, StalkRegrow[Index]) : 1.0f;
			const float Swell = StalkCharges.IsValidIndex(Index) ? 1.0f + 0.35f * StalkCharges[Index] : 1.0f;
			CrownPieces[Index]->SetWorldLocationAndRotation(End, FRotationMatrix::MakeFromZ(Direction).Rotator());
			CrownPieces[Index]->SetWorldScale3D(FVector(FMath::Max(Plant->TipSize * 1.6f / 10.0f * Bloom * Regrown * Swell, 0.001f)));
		}
	}

	// The big rose of the Rußrose on top, beating with its heart
	if (Plant->bVineArm && CrownPieces.Num() == 1)
	{
		const float Beat = 1.0f + 0.06f * FMath::Square(FMath::Sin(Time * 3.0f));
		CrownPieces[0]->SetWorldLocationAndRotation(Crown - Axis * 6.0f, FRotationMatrix::MakeFromZ(Axis).Rotator());
		CrownPieces[0]->SetWorldScale3D(FVector(Plant->ShoulderRoseSize * 1.6f / 10.0f * Bloom * Beat));
	}
	if (CrownCore != nullptr)
	{
		CrownCore->SetWorldLocationAndRotation(Crown - Axis * 3.0f, FRotationMatrix::MakeFromZ(Axis).Rotator());
		CrownCore->SetWorldScale3D(FVector(FMath::Max(Plant->CoreLength * 1.8f / 10.0f * Bloom, 0.001f)));
	}
	// Crown of petals: the rose stands open on top, breathing, the inner ring half shut; aiming it closes round the hole it fires
	// from like the hand; the poppy of a Mohnmörser stays a shut bud that bursts open at each throw
	else if (CrownStalks.IsEmpty())
	{
		const float Open = Plant->bMortar ? FMath::Lerp(0.15f, 1.0f, FMath::Sqrt(BudOpen)) : FMath::Lerp(1.0f, 0.55f, AimBlend);
		for (int32 Index = 0; Index < CrownPieces.Num(); ++Index)
		{
			const bool bInner = Index < Plant->InnerPetals;
			const int32 Count = bInner ? Plant->InnerPetals : Plant->OuterPetals;
			const int32 InRing = bInner ? Index : Index - Plant->InnerPetals;
			const float Angle = 2.0f * UE_PI * (InRing + (bInner ? 0.0f : 0.5f)) / FMath::Max(Count, 1);
			const float Size = Plant->PetalSize * 1.8f * (bInner ? 0.72f : 1.0f);
			const float Lean = FMath::DegreesToRadians((bInner ? 15.0f : Plant->RoseOpenAngle * 0.8f) * Open + 4.0f * FMath::Sin(Time * 1.5f + Index));
			const FVector Out = U * FMath::Cos(Angle) + V * FMath::Sin(Angle);
			const FVector Up = (Axis * FMath::Cos(Lean) + Out * FMath::Sin(Lean)).GetSafeNormal();
			const FVector Outside = (Out - Up * FVector::DotProduct(Out, Up)).GetSafeNormal();
			const FVector Foot = Crown + Out * (bInner ? 6.0f : 11.0f) * Bloom;
			CrownPieces[Index]->SetWorldLocationAndRotation(Foot, FRotationMatrix::MakeFromXZ(Outside, Up).Rotator());
			CrownPieces[Index]->SetWorldScale3D(FVector(Size / 10.0f * Bloom));
		}
	}
}

void AVaelPlantTower::CallUp(float Seconds)
{
	if (bWithering)
	{
		return;
	}
	// A call while it already stands giant grows the veil of a Mondtraufe pair one step more
	CallStacks = CallLeft > 0.0f ? FMath::Min(CallStacks + 1, 3) : 1;
	// The pair grows up together, and the call keeps it standing at least that long
	CallLeft = FMath::Max(CallLeft, Seconds);
	LifeLeft = FMath::Max(LifeLeft, Seconds);
	if (AVaelPlantTower* Other = Partner.Get())
	{
		Other->CallLeft = FMath::Max(Other->CallLeft, Seconds);
		Other->LifeLeft = FMath::Max(Other->LifeLeft, Seconds);
	}
	VaelSymbiosis::MuzzlePuff(this, Plant, GetCrown(), bShadow, 2.5f);
}

float AVaelPlantTower::GetVeilScale() const
{
	// Fused it is a little bigger than from the hands; every Turmruf while it stands giant adds a little more
	const AVaelPlantTower* Other = Partner.Get();
	const int32 Stacks = FMath::Max(CallLeft > 0.0f ? CallStacks : 0, Other != nullptr && Other->CallLeft > 0.0f ? Other->CallStacks : 0);
	return FMath::Min(1.2f + 0.12f * Stacks, 1.5f);
}

// ---------------------------------------------------------------- Schlotwinde tower (Boris 2026-11-06)

namespace
{
	constexpr int32 WindVines = 4;
	constexpr int32 WindPieces = 10;
	constexpr int32 WindTrumpetsEach = 2;
	/** Seconds a lash takes, the share winding up, and how many enemies a tower holds at once */
	constexpr float WindLashSeconds = 0.5f;
	constexpr float WindWindUp = 0.3f;
	constexpr int32 WindMaxLassos = 2;
}

bool AVaelPlantTower::IsBindweedTower() const
{
	return Plant != nullptr && Plant->bLasso && (Symbiosis == EVaelSymbiosis::None || Symbiosis == EVaelSymbiosis::Windenzaun);
}

void AVaelPlantTower::BuildBindweedTower()
{
	UStaticMesh* VineMesh = nullptr;
	for (const TSoftObjectPtr<UStaticMesh>& Mesh : Plant->FingerMeshes)
	{
		VineMesh = VaelAssets::LoadOptional(Mesh) != nullptr ? VaelAssets::LoadOptional(Mesh) : VineMesh;
	}
	UStaticMesh* TipMesh = VaelAssets::LoadOptional(Plant->TipMesh);
	UStaticMesh* TrumpetMesh = VaelAssets::LoadOptional(Plant->LeafMesh);
	const auto MakePiece = [this](UStaticMesh* Mesh)
	{
		UStaticMeshComponent* Piece = NewObject<UStaticMeshComponent>(this);
		Piece->SetMobility(EComponentMobility::Movable);
		Piece->SetStaticMesh(Mesh);
		Piece->SetMaterial(0, Material);
		Piece->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Piece->SetUsingAbsoluteLocation(true);
		Piece->SetUsingAbsoluteRotation(true);
		Piece->SetUsingAbsoluteScale(true);
		Piece->SetupAttachment(RootComponent);
		Piece->RegisterComponent();
		return Piece;
	};
	for (int32 Vine = 0; Vine < WindVines && VineMesh != nullptr; ++Vine)
	{
		for (int32 Piece = 0; Piece < WindPieces; ++Piece)
		{
			USplineMeshComponent* Stalk = MakeStalk(VineMesh);
			Stalk->SetUsingAbsoluteLocation(true);
			Stalk->SetUsingAbsoluteRotation(true);
			Stalk->SetUsingAbsoluteScale(true);
			Stalk->SetWorldTransform(FTransform::Identity);
			WindTendrils.Add(Stalk);
		}
		if (TipMesh != nullptr)
		{
			WindTips.Add(MakePiece(TipMesh));
		}
		for (int32 Index = 0; Index < WindTrumpetsEach && TrumpetMesh != nullptr; ++Index)
		{
			WindTrumpets.Add(MakePiece(TrumpetMesh));
		}
	}
	WindWhipTime.Init(-1.0f, WindVines);
	WindCooldown.Init(0.0f, WindVines);
	WindWhipDir.Init(FVector::ForwardVector, WindVines);
	WindWhipReach.Init(300.0f, WindVines);
	WindWhipTarget.SetNum(WindVines);
	WindLassos.SetNum(WindVines);
	WindHit.Init(true, WindVines);
	WindBound.Init(true, WindVines);
}

void AVaelPlantTower::UpdateBindweedTower(float DeltaSeconds, bool bActs)
{
	const bool bAlone = IsBindweedTower();
	for (USplineMeshComponent* Stalk : WindTendrils)
	{
		Stalk->SetVisibility(bAlone && Grow > 0.05f);
	}
	for (UStaticMeshComponent* Piece : WindTips)
	{
		Piece->SetVisibility(bAlone && Grow > 0.3f);
	}
	for (UStaticMeshComponent* Piece : WindTrumpets)
	{
		Piece->SetVisibility(bAlone && Grow > 0.5f);
	}
	// Alone it has no crown: the crown stalks only show once it grows together with another tower
	for (USplineMeshComponent* Stalk : CrownStalks)
	{
		Stalk->SetVisibility(!bAlone && Stalk->IsVisible());
	}
	for (UStaticMeshComponent* Piece : CrownPieces)
	{
		Piece->SetVisibility(!bAlone && Piece->IsVisible());
	}
	for (UStaticMeshComponent* Orb : ChargeOrbs)
	{
		Orb->SetVisibility(!bAlone && Orb->IsVisible());
	}
	// Alone, the column is only a thick tangle on the ground; the vines that rise out of it are the tower
	if (bAlone)
	{
		const float Shown0 = FMath::Clamp(Grow, 0.0f, 1.0f);
		for (int32 Index = 0; Index < Column.Num(); ++Index)
		{
			const float A0 = Index * 2.0f * UE_PI / Column.Num() + GetUniqueID() * 0.3f;
			const float A1 = A0 + UE_PI * 1.1f;
			const FVector Start(FMath::Cos(A0) * 30.0f, FMath::Sin(A0) * 30.0f, -8.0f);
			const FVector End(FMath::Cos(A1) * 12.0f, FMath::Sin(A1) * 12.0f, 34.0f * Shown0);
			const FVector Around(-FMath::Sin(A0), FMath::Cos(A0), 0.0f);
			USplineMeshComponent* Tangle = Column[Index];
			Tangle->SetStartAndEnd(Start, Around * 60.0f + FVector(0.0f, 0.0f, 30.0f), End, FVector(0.0f, 0.0f, 50.0f) - Around * 30.0f, false);
			Tangle->SetStartScale(FVector2D(4.5f * Shown0), false);
			Tangle->SetEndScale(FVector2D(3.5f * Shown0), false);
			Tangle->UpdateMesh();
		}
	}
	if (!bAlone || WindTendrils.IsEmpty())
	{
		for (TWeakObjectPtr<AVaelBindweedLasso>& Lasso : WindLassos)
		{
			if (Lasso.IsValid())
			{
				Lasso->Finish(EVaelLassoEnd::Release);
			}
			Lasso = nullptr;
		}
		return;
	}

	AVaelCharacter* Gardener = GetGardener();
	const UVaelFloromantSettings* Settings = UVaelFloromantSettings::Get();
	const float Time = GetWorld()->GetTimeSeconds();
	const FVector Base = GetActorLocation();
	const FVector Crown = GetCrown();
	const float Shown = FMath::Clamp(Grow, 0.0f, 1.0f);
	const float Big = 1.0f + 0.4f * Giant;
	if (AVaelVineWall* Fence = Bridge.Get(); Fence != nullptr && bLeads)
	{
		Fence->SetSwell(FMath::Max(Giant, Partner.IsValid() ? Partner->Giant : 0.0f));
	}
	// The vines ARE the tower (Boris 2026-11-06): they rise from a thick tangle on the ground, wind round each other up to its height
	const float Length = Settings->TowerHeight * 1.05f * Big * FMath::Sin(Shown * UE_HALF_PI);
	const float Inner = Plant->WhipReach * 0.95f * Big;
	const float Outer = Plant->LassoRange * Big;

	// Choose what each vine does: close enemies are lashed, those further out caught and pulled in (at most two at once)
	if (bActs && Gardener != nullptr && !AVaelSafeZone::IsSafe(GetWorld(), Base))
	{
		int32 Holding = 0;
		for (const TWeakObjectPtr<AVaelBindweedLasso>& Lasso : WindLassos)
		{
			Holding += Lasso.IsValid() && !Lasso->IsDone() ? 1 : 0;
		}
		const TArray<AActor*> Enemies = AVaelBindweedLasso::FindTargets(Gardener, Base, FVector::ZeroVector, Outer, 180.0f, 8);
		for (AActor* Enemy : Enemies)
		{
			const float Distance = FVector::Dist2D(Enemy->GetActorLocation(), Base);
			bool bTaken = false;
			for (int32 Vine = 0; Vine < WindVines; ++Vine)
			{
				bTaken |= WindWhipTarget[Vine].Get() == Enemy && WindWhipTime[Vine] >= 0.0f;
				bTaken |= WindLassos[Vine].IsValid() && !WindLassos[Vine]->IsDone() && WindLassos[Vine]->GetTarget() == Enemy;
			}
			if (bTaken)
			{
				continue;
			}
			const bool bClose = Distance <= Inner;
			if (!bClose && Holding >= WindMaxLassos)
			{
				continue;
			}
			// The free vine on the side of the enemy
			int32 Best = INDEX_NONE;
			float BestDot = -2.0f;
			const FVector Way = (Enemy->GetActorLocation() - Base).GetSafeNormal2D();
			for (int32 Vine = 0; Vine < WindVines; ++Vine)
			{
				const bool bFree = WindCooldown[Vine] <= 0.0f && WindWhipTime[Vine] < 0.0f && (!WindLassos[Vine].IsValid() || WindLassos[Vine]->IsDone());
				const float Angle = Vine * 2.0f * UE_PI / WindVines + GetUniqueID() * 0.3f;
				const float Dot = FVector::DotProduct(FVector(FMath::Cos(Angle), FMath::Sin(Angle), 0.0f), Way);
				if (bFree && Dot > BestDot)
				{
					Best = Vine;
					BestDot = Dot;
				}
			}
			if (Best == INDEX_NONE)
			{
				break;
			}
			if (bClose)
			{
				WindWhipTime[Best] = 0.0f;
				WindWhipDir[Best] = Way;
				WindWhipReach[Best] = FMath::Clamp(Distance + 60.0f, 160.0f, Inner * 1.1f);
				WindWhipTarget[Best] = Enemy;
				WindHit[Best] = false;
				WindCooldown[Best] = WindLashSeconds + 0.35f;
			}
			else
			{
				FVaelLassoSpec Spec;
				Spec.HoldSeconds = 0.9f;
				Spec.OnEnd = bShadow ? EVaelLassoEnd::Slam : EVaelLassoEnd::Yank;
				Spec.DamagePerSecond = bShadow ? Plant->StrangleDamagePerSecond : 0.0f;
				Spec.DamageGrowth = bShadow ? Plant->StrangleGrowth : 0.0f;
				Spec.Color = bShadow ? Plant->StrangleColor : Plant->PollenColor;
				Spec.bPullToPoint = true;
				Spec.PullPoint = Base + Way * Inner * 0.5f;
				// With a Rankenbrücke the enemy is flung against the fence, on its own side
				if (Bridge.IsValid() && Partner.IsValid())
				{
					const FVector Other = Partner->GetActorLocation();
					const FVector Line = Other - Base;
					const float Along = FMath::Clamp(FVector::DotProduct(Enemy->GetActorLocation() - Base, Line) / FMath::Max(Line.SizeSquared(), 1.0f), 0.15f, 0.85f);
					const FVector OnLine = Base + Line * Along;
					Spec.PullPoint = OnLine + (Enemy->GetActorLocation() - OnLine).GetSafeNormal2D() * 55.0f;
				}
				const UVaelFloromantComponent* Floromant = Gardener->GetFloromant();
				Spec.Power = Floromant != nullptr ? Floromant->GetPlantPower(Plant) : 1.0f;
				if (AVaelBindweedLasso* Lasso = AVaelBindweedLasso::Throw(Gardener, Plant, Base + FVector(0.0f, 0.0f, Length * 0.8f), Enemy, Spec, bShadow))
				{
					WindLassos[Best] = Lasso;
					WindBound[Best] = false;
					WindCooldown[Best] = 1.2f;
					++Holding;
					VaelSymbiosis::MuzzlePuff(this, Plant, Crown, bShadow, 1.2f);
				}
			}
		}
	}

	// Each vine: rising from the top of the column, curving in so the tips stop short of each other; lashing it winds back and
	// sweeps across its enemy in a wide flat bow; throwing a lasso it reaches out after it
	const int32 TrumpetsEach = WindTrumpets.Num() / FMath::Max(WindVines, 1);
	for (int32 Vine = 0; Vine < WindVines; ++Vine)
	{
		WindCooldown[Vine] = FMath::Max(0.0f, WindCooldown[Vine] - DeltaSeconds);
		const float Angle = Vine * 2.0f * UE_PI / WindVines + GetUniqueID() * 0.3f;
		const FVector Out(FMath::Cos(Angle), FMath::Sin(Angle), 0.0f);
		const FVector Around(-Out.Y, Out.X, 0.0f);
		const FVector Root = Base + Out * 16.0f + FVector(0.0f, 0.0f, 22.0f);

		// The lash
		float Lash = 0.0f;
		float StrikeTime = -1.0f;
		if (WindWhipTime[Vine] >= 0.0f)
		{
			const float Before = WindWhipTime[Vine] / WindLashSeconds;
			WindWhipTime[Vine] += DeltaSeconds;
			StrikeTime = WindWhipTime[Vine] / WindLashSeconds;
			const float HitAt = WindWindUp + 0.45f * (1.0f - WindWindUp);
			if (!WindHit[Vine] && StrikeTime >= HitAt)
			{
				WindHit[Vine] = true;
				// It strikes its enemy and whoever stands close to it
				FVaelSpellHit Hit;
				const UVaelFloromantComponent* Floromant = Gardener != nullptr ? Gardener->GetFloromant() : nullptr;
				Hit.Damage = Plant->WhipDamage * (Floromant != nullptr ? Floromant->GetPlantPower(Plant) : 1.0f);
				Hit.Element = EVaelElement::Earth;
				Hit.Knockback = Plant->WhipKnockback * 0.4f;
				Hit.StunDuration = Plant->WhipStun;
				const FVector Strike = Base + WindWhipDir[Vine] * WindWhipReach[Vine] * 0.8f;
				for (AActor* Struck : AVaelBindweedLasso::FindTargets(Gardener, Strike, FVector::ZeroVector, 160.0f, 180.0f, 4))
				{
					// Caught in the Rankenbrücke it takes half again as much
					const AVaelVineWall* Fence = Bridge.Get();
					FVaelSpellHit StruckHit = Hit;
					StruckHit.Damage *= Fence != nullptr && Fence->IsEntangled(Struck) ? 1.5f : 1.0f;
					UVaelCombatStatics::ApplySpellHit(Gardener, Struck, StruckHit, (Struck->GetActorLocation() - Base).GetSafeNormal2D());
					VaelSymbiosis::MuzzlePuff(this, Plant, Struck->GetActorLocation() + FVector(0.0f, 0.0f, 40.0f), bShadow, 1.4f);
				}
			}
			if (StrikeTime >= 1.0f)
			{
				WindWhipTime[Vine] = -1.0f;
				StrikeTime = -1.0f;
			}
			(void)Before;
		}
		if (StrikeTime >= 0.0f)
		{
			const float X = StrikeTime >= WindWindUp ? (StrikeTime - WindWindUp) / (1.0f - WindWindUp) : 0.0f;
			Lash = StrikeTime < WindWindUp ? 0.4f * StrikeTime / WindWindUp : FMath::Max(0.4f * (1.0f - X), FMath::Sin(UE_PI * X));
		}
		const FVector Aim = WindWhipDir[Vine];
		const FVector Side = FVector::CrossProduct(FVector::UpVector, Aim).GetSafeNormal();
		const float StartSide = Vine % 2 == 0 ? 1.0f : -1.0f;
		const FVector Diagonal = (Side * StartSide + FVector::UpVector * 0.35f).GetSafeNormal();
		const FVector BackHigh = (Diagonal * 0.6f - Aim * 0.5f + FVector::UpVector * 0.7f).GetSafeNormal();
		const float Arc = FMath::DegreesToRadians(Plant->WhipArcDegrees * 0.8f);
		const auto StrokeAt = [&](float Share)
		{
			const float A = Arc * FMath::Cos(UE_PI * Share);
			return (Aim * FMath::Cos(A) + Diagonal * FMath::Sin(A) - FVector::UpVector * 0.25f).GetSafeNormal();
		};
		const auto StrokeDirection = [&](float P)
		{
			P = FMath::Clamp(P, 0.0f, 1.0f);
			if (P < WindWindUp)
			{
				return FMath::Lerp(StrokeAt(0.0f), BackHigh, FMath::SmoothStep(0.0f, 1.0f, P / WindWindUp)).GetSafeNormal();
			}
			const float X = (P - WindWindUp) / (1.0f - WindWindUp);
			return FMath::Lerp(BackHigh, StrokeAt(X * X * (3.0f - 2.0f * X)), FMath::SmoothStep(0.0f, 0.3f, X)).GetSafeNormal();
		};
		const float Reach = FMath::Lerp(Length, WindWhipReach[Vine], FMath::Clamp(Lash, 0.0f, 1.0f));
		// Pulled in against the Rankenbrücke: tied to it a while, so it fills with enemies for the vines to lash
		if (AVaelBindweedLasso* Done = WindLassos[Vine].Get(); Done != nullptr && Done->IsDone() && !WindBound[Vine])
		{
			WindBound[Vine] = true;
			if (AVaelVineWall* Fence = Bridge.Get())
			{
				Fence->Bind(Done->GetTarget(), 2.5f);
			}
		}
		AVaelBindweedLasso* Lasso = WindLassos[Vine].Get();
		const bool bThrowing = Lasso != nullptr && !Lasso->IsDone();

		const auto PointAt = [&](float S)
		{
			// At rest: the vines wind round each other up into the tower, the tips bending in at the top and stopping a hand short
			// of each other; slowly the whole tower turns and the tips dance
			const float Spin = Angle + S * 1.3f * 2.0f * UE_PI + Time * 0.25f;
			const float Radius = FMath::Lerp(16.0f, 9.0f, S) + 6.0f * FMath::Sin(S * UE_PI);
			const FVector Ring(FMath::Cos(Spin), FMath::Sin(Spin), 0.0f);
			FVector Point = Root + FVector::UpVector * Length * S + Ring * Radius - Out * 16.0f * FMath::Min(S * 4.0f, 1.0f);
			// the top fifth bends in towards the middle
			const float TopBend = FMath::SmoothStep(0.78f, 1.0f, S);
			Point += (Ring * -Radius * 0.6f - FVector::UpVector * Length * 0.05f) * TopBend;
			Point += Around * 9.0f * FMath::Square(S) * FMath::Sin(Time * 1.6f + Vine * 1.7f) + Out * 5.0f * FMath::Square(S) * FMath::Sin(Time * 1.1f + Vine);
			if (bThrowing)
			{
				// Reaching after the lasso, out of which it grows
				const FVector Goal = Lasso->GetTarget() != nullptr ? Lasso->GetTarget()->GetActorLocation() : Root;
				const FVector Toward = Root + (Goal - Root).GetSafeNormal() * Length * 0.6f * S + FVector::UpVector * 30.0f * FMath::Sin(S * UE_PI);
				Point = FMath::Lerp(Point, Toward, 0.85f);
			}
			else if (StrikeTime >= 0.0f)
			{
				FVector Lashed = Root;
				const int32 Steps = 10;
				for (int32 Step = 1; Step <= Steps; ++Step)
				{
					const float Along = S * Step / Steps;
					Lashed += StrokeDirection(StrikeTime - 0.42f * Along) * Reach * S / Steps;
				}
				Point = FMath::Lerp(Point, Lashed, FMath::Min(1.0f, Lash * 2.2f));
			}
			return Point;
		};
		if (bThrowing)
		{
			Lasso->SetStart(PointAt(1.0f));
		}

		const float Thickness = Plant->TendrilThickness * Plant->TendrilBulk * (1.0f + 0.6f * Lash) * Shown * Big;
		for (int32 Piece = 0; Piece < WindPieces; ++Piece)
		{
			USplineMeshComponent* Stalk = WindTendrils[Vine * WindPieces + Piece];
			const float A = Piece / static_cast<float>(WindPieces) * Shown;
			const float B = (Piece + 1) / static_cast<float>(WindPieces) * Shown;
			const FVector P0 = PointAt(A);
			const FVector P1 = PointAt(B);
			const float Span = FVector::Dist(P0, P1);
			// The points are in world space; the pieces follow the tower, so into their own space
			const FTransform& Local = Stalk->GetComponentTransform();
			Stalk->SetStartAndEnd(Local.InverseTransformPosition(P0), Local.InverseTransformVectorNoScale((PointAt(A + 0.02f) - PointAt(FMath::Max(A - 0.02f, 0.0f))).GetSafeNormal() * Span),
				Local.InverseTransformPosition(P1), Local.InverseTransformVectorNoScale((PointAt(B + 0.02f) - PointAt(B - 0.02f)).GetSafeNormal() * Span), false);
			Stalk->SetStartScale(FVector2D(Thickness * FMath::Lerp(1.3f, 0.6f, A)), false);
			Stalk->SetEndScale(FVector2D(Thickness * FMath::Lerp(1.3f, 0.6f, B)), false);
			Stalk->SetStartRoll(Piece * 1.3f + Vine, false);
			Stalk->SetEndRoll(Piece * 1.3f + Vine + 1.1f, false);
			Stalk->UpdateMesh();
		}
		if (WindTips.IsValidIndex(Vine))
		{
			const FVector Tip = PointAt(Shown);
			WindTips[Vine]->SetWorldLocationAndRotation(Tip, FRotationMatrix::MakeFromZ((Tip - PointAt(Shown * 0.9f)).GetSafeNormal()).Rotator());
			WindTips[Vine]->SetWorldScale3D(FVector(Plant->TipSize / 10.0f * 1.4f * Shown * Big));
		}
		for (int32 Index = 0; Index < TrumpetsEach; ++Index)
		{
			const float S = Shown * (0.3f + 0.4f * Index);
			const FVector Here = PointAt(S);
			const FVector Mouth = (Out + FVector::UpVector * 0.4f + (Index % 2 == 0 ? Around : -Around) * 0.5f).GetSafeNormal();
			UStaticMeshComponent* Trumpet = WindTrumpets[Vine * TrumpetsEach + Index];
			Trumpet->SetWorldLocationAndRotation(Here, FRotationMatrix::MakeFromZ(Mouth).Rotator());
			Trumpet->SetWorldScale3D(FVector(1.5f * Shown * Big));
		}
	}
}

// --- Plant dragon ---------------------------------------------------------------------------------------------------------

namespace
{
	/** One vine of the dragon head in its own frame (cm at a 2 m tower; X ahead, Y right, Z up, the crown of the snake at 0):
	 *  from A to B with their tangents, thick to thin; jaw vines turn down with the jaw; mirrored ones come in pairs */
	struct FDragonVine
	{
		FVector A, TA, B, TB;
		float S0, S1;
		bool bJaw;
		bool bMirror;
	};

	const FDragonVine DragonVineTable[] =
	{
		// Skull: the ridge down the middle, brows, the ridge over the eyes, cheeks, upper lip and the curled nostrils
		{ FVector(-18, 0, 20), FVector(70, 0, 25), FVector(80, 0, 6), FVector(50, 0, -30), 1.8f, 1.0f, false, false },
		{ FVector(-14, 12, 18), FVector(60, 6, 30), FVector(62, 8, 10), FVector(50, -4, -30), 1.6f, 1.0f, false, true },
		{ FVector(8, 13, 22), FVector(30, 10, 18), FVector(46, 16, 15), FVector(20, 3, -12), 1.4f, 0.8f, false, true },
		{ FVector(-14, 19, 2), FVector(60, 4, 0), FVector(76, 9, -2), FVector(60, -10, -4), 1.7f, 1.0f, false, true },
		{ FVector(4, 16, -8), FVector(60, 0, 0), FVector(84, 4, -4), FVector(40, -6, 4), 1.3f, 0.8f, false, true },
		{ FVector(70, 5, 8), FVector(20, 8, 10), FVector(82, 9, -3), FVector(0, 0, -20), 1.0f, 0.7f, false, true },
		// Lower jaw: jaw line, chin and the inside of the jaw
		{ FVector(-10, 17, -10), FVector(50, -2, -15), FVector(74, 5, -16), FVector(50, -8, 5), 1.7f, 0.9f, true, true },
		{ FVector(4, 0, -22), FVector(60, 0, -6), FVector(70, 0, -18), FVector(40, 0, 6), 1.5f, 0.9f, true, false },
		{ FVector(0, 10, -16), FVector(60, -2, 0), FVector(66, 4, -12), FVector(40, -4, 0), 1.1f, 0.7f, true, true },
		// Horns sweeping back and curling, long whiskers, small back horns, a beard under the chin
		{ FVector(-8, 10, 22), FVector(-30, 10, 50), FVector(-62, 26, 56), FVector(-30, 6, -30), 1.6f, 0.5f, false, true },
		{ FVector(-6, 20, -2), FVector(-20, 30, -10), FVector(-72, 46, -14), FVector(-40, 10, 10), 1.3f, 0.3f, false, true },
		{ FVector(-16, 6, 16), FVector(-40, 4, 20), FVector(-46, 12, 34), FVector(-10, 4, 30), 1.2f, 0.4f, false, true },
		{ FVector(10, 6, -22), FVector(-10, 4, -30), FVector(-24, 10, -48), FVector(-30, 0, -10), 1.0f, 0.3f, true, true },
	};

	/** Hinge of the jaw and how far it opens */
	const FVector DragonJawHinge(-6.0f, 0.0f, -6.0f);
	constexpr float DragonJawDegrees = 38.0f;

	/** Mane: stalks with thistle heads on the back of the skull, and a few more along the lifted neck */
	constexpr int32 DragonSkullMane = 14;
	constexpr int32 DragonNeckMane = 6;

	/** Sparks that run up the snake while the head grows */
	constexpr int32 DragonSurgeOrbs = 8;

	FVector DragonJawTurn(const FVector& Local, float JawOpen, bool bPoint)
	{
		const float Angle = FMath::DegreesToRadians(DragonJawDegrees) * JawOpen;
		const float C = FMath::Cos(Angle);
		const float S = FMath::Sin(Angle);
		const FVector From = bPoint ? Local - DragonJawHinge : Local;
		const FVector Turned(From.X * C + From.Z * S, From.Y, From.Z * C - From.X * S);
		return bPoint ? Turned + DragonJawHinge : Turned;
	}

	FVector DragonHermite(const FVector& P0, const FVector& T0, const FVector& P1, const FVector& T1, float T)
	{
		const float T2 = T * T;
		const float T3 = T2 * T;
		return P0 * (2.0f * T3 - 3.0f * T2 + 1.0f) + T0 * (T3 - 2.0f * T2 + T) + P1 * (3.0f * T2 - 2.0f * T3) + T1 * (T3 - T2);
	}
}

FVector AVaelPlantTower::GetHookCurve(float T) const
{
	const int32 Piece = FMath::Clamp(FMath::FloorToInt(T), 0, 2);
	const float Along = FMath::Clamp(T - Piece, 0.0f, 1.0f);
	return DragonHermite(HookPoints[Piece], HookTangents[Piece], HookPoints[Piece + 1], HookTangents[Piece + 1], Along);
}

void AVaelPlantTower::BuildDragon()
{
	if (bDragon)
	{
		return;
	}
	bDragon = true;
	DragonGrow = 0.0f;

	int32 Pick = 0;
	for (const FDragonVine& Vine : DragonVineTable)
	{
		for (int32 Side = 0; Side < (Vine.bMirror ? 2 : 1); ++Side)
		{
			USplineMeshComponent* Piece = MakeStalk(TowerFirstMesh({ &Plant->VineMeshes, &Plant->ArmMeshes, &Plant->FingerMeshes }, Pick++));
			Piece->SetVisibility(false);
			DragonVines.Add(Piece);
		}
	}

	if (UStaticMesh* TipMesh = VaelAssets::LoadOptional(Plant->TipMesh))
	{
		for (int32 Index = 0; Index < DragonDrumHeads + DragonSkullMane + DragonNeckMane; ++Index)
		{
			UStaticMeshComponent* Head = NewObject<UStaticMeshComponent>(this);
			Head->SetMobility(EComponentMobility::Movable);
			Head->SetStaticMesh(TipMesh);
			Head->SetMaterial(0, Material);
			Head->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			Head->SetCanEverAffectNavigation(false);
			Head->SetupAttachment(RootComponent);
			Head->RegisterComponent();
			Head->SetVisibility(false);
			DragonHeads.Add(Head);
		}
	}
	DrumRegrow.Init(1.0f, DragonDrumHeads);

	for (int32 Index = 0; Index < DragonSkullMane + DragonNeckMane; ++Index)
	{
		USplineMeshComponent* Stalk = MakeStalk(TowerFirstMesh({ &Plant->FingerMeshes, &Plant->ArmMeshes }, Index));
		Stalk->SetVisibility(false);
		DragonManeStalks.Add(Stalk);
	}

	UMaterialInterface* Core = nullptr;
	UMaterialInterface* GlowMaterial = nullptr;
	VaelEffects::LoadLookMaterials(Core, GlowMaterial);
	if (GlowMaterial != nullptr)
	{
		for (int32 Index = 0; Index < 3 + DragonSurgeOrbs; ++Index)
		{
			UStaticMeshComponent* Orb = VaelEffects::AddLookShape(this, GlowMaterial, VaelSymbiosis::GetColor(EVaelSymbiosis::Glutbusch), 0.0f);
			Orb->SetUsingAbsoluteLocation(true);
			Orb->SetUsingAbsoluteRotation(true);
			Orb->SetUsingAbsoluteScale(true);
			Orb->SetVisibility(false);
			DragonGlows.Add(Orb);
		}
	}
}

void AVaelPlantTower::UpdateDragon(float DeltaSeconds)
{
	const float Before = DragonGrow;
	DragonGrow = bWithering ? FMath::Min(DragonGrow, FMath::Max(Grow, 0.0f)) : FMath::Min(1.0f, DragonGrow + DeltaSeconds / 1.6f);
	const float Appear = FMath::SmoothStep(0.45f, 1.0f, DragonGrow);
	const float Time = GetWorld()->GetTimeSeconds();
	const FTransform& Transform = GetActorTransform();

	// The moment the head breaks out of the crown: a burst of embers
	if (Before < 0.5f && DragonGrow >= 0.5f)
	{
		FVaelDebris Burst;
		Burst.Count = 26;
		Burst.Spread = 30.0f;
		Burst.Speed = 280.0f;
		Burst.Lift = 200.0f;
		Burst.Lifetime = 1.1f;
		Burst.MinSize = 3.0f;
		Burst.MaxSize = 8.0f;
		Burst.Color = VaelSymbiosis::GetColor(EVaelSymbiosis::Glutbusch);
		AVaelDebrisBurst::Spawn(GetGardener(), GetCrown(), Burst);
	}

	// The old crown is gone once the head has broken out
	for (USplineMeshComponent* Stalk : CrownStalks)
	{
		Stalk->SetVisibility(DragonGrow < 0.5f);
	}
	for (UStaticMeshComponent* Piece : CrownPieces)
	{
		Piece->SetVisibility(DragonGrow < 0.5f);
	}

	// The jaw: shut and breathing at rest, ajar while it charges, wide open through the drumfire
	const bool bCharging = Target.IsValid() && !bWithering && FireCountdown < 0.7f;
	const float JawGoal = BurstLeft > 0 ? 1.0f : (bCharging ? 0.35f : 0.06f + 0.04f * FMath::Sin(Time * 1.7f));
	Jaw = FMath::FInterpTo(Jaw, JawGoal, DeltaSeconds, JawGoal > Jaw ? 10.0f : 5.0f);
	DrumAngle += DeltaSeconds * (BurstLeft > 0 ? 10.0f : 1.2f);

	// The frame of the head on the crown of the snake, sized with the tower
	const FVector Crown = GetCrown();
	FVector Forward = GetCrownAxis().GetSafeNormal2D();
	if (Forward.IsNearlyZero())
	{
		Forward = FVector::ForwardVector;
	}
	const FVector Side = FVector::CrossProduct(FVector::UpVector, Forward);
	const UVaelFloromantSettings* Settings = UVaelFloromantSettings::Get();
	const float Height = Settings->TowerHeight * FMath::Sin(FMath::Clamp(Grow, 0.0f, 1.0f) * UE_HALF_PI) * (1.0f + (Settings->TowerCallSizeScale - 1.0f) * Giant);
	const float Size = Height / 200.0f * Appear;
	auto ToWorld = [&](const FVector& Local) { return Crown + (Forward * Local.X + Side * Local.Y + FVector::UpVector * Local.Z) * Size; };
	auto ToWorldDir = [&](const FVector& Local) { return (Forward * Local.X + Side * Local.Y + FVector::UpVector * Local.Z) * Size; };
	const bool bShown = Appear > 0.02f;

	// Skull, jaw and horns
	int32 Piece = 0;
	for (const FDragonVine& Vine : DragonVineTable)
	{
		for (int32 Mirror = 0; Mirror < (Vine.bMirror ? 2 : 1); ++Mirror)
		{
			USplineMeshComponent* Spline = DragonVines.IsValidIndex(Piece) ? DragonVines[Piece].Get() : nullptr;
			++Piece;
			if (Spline == nullptr)
			{
				continue;
			}
			Spline->SetVisibility(bShown);
			if (!bShown)
			{
				continue;
			}
			const FVector Flip(1.0f, Mirror == 0 ? 1.0f : -1.0f, 1.0f);
			FVector A = Vine.A * Flip;
			FVector TA = Vine.TA * Flip;
			FVector B = Vine.B * Flip;
			FVector TB = Vine.TB * Flip;
			if (Vine.bJaw)
			{
				A = DragonJawTurn(A, Jaw, true);
				TA = DragonJawTurn(TA, Jaw, false);
				B = DragonJawTurn(B, Jaw, true);
				TB = DragonJawTurn(TB, Jaw, false);
			}
			// The whiskers and horns stir a little
			B.Z += FMath::Sin(Time * 1.4f + Piece * 0.9f) * (Vine.S1 < 0.6f ? 4.0f : 0.8f);
			Spline->SetStartAndEnd(Transform.InverseTransformPosition(ToWorld(A)), Transform.InverseTransformVectorNoScale(ToWorldDir(TA)),
				Transform.InverseTransformPosition(ToWorld(B)), Transform.InverseTransformVectorNoScale(ToWorldDir(TB)), false);
			Spline->SetStartScale(FVector2D(Vine.S0 * Size), false);
			Spline->SetEndScale(FVector2D(Vine.S1 * Size), false);
			Spline->SetStartRoll(Piece * 1.3f, false);
			Spline->SetEndRoll(Piece * 1.3f + 2.0f, false);
			Spline->UpdateMesh();
		}
	}

	// The mane: thistle stalks bristling back off the skull and up off the lifted neck, bushy
	const float HeadScale = FMath::Max(Plant->TipSize * 1.6f / 10.0f, 0.01f);
	for (int32 Index = 0; Index < DragonManeStalks.Num(); ++Index)
	{
		USplineMeshComponent* Stalk = DragonManeStalks[Index];
		UStaticMeshComponent* Head = DragonHeads.IsValidIndex(DragonDrumHeads + Index) ? DragonHeads[DragonDrumHeads + Index].Get() : nullptr;
		Stalk->SetVisibility(bShown);
		if (Head != nullptr)
		{
			Head->SetVisibility(bShown);
		}
		if (!bShown)
		{
			continue;
		}
		const float SideSign = Index % 2 == 0 ? 1.0f : -1.0f;
		const float Sway = FMath::Sin(Time * 1.6f + Index * 0.7f) * 0.08f;
		FVector Root;
		FVector Direction;
		float Length;
		if (Index < DragonSkullMane)
		{
			const float Row = Index / 2;
			Root = ToWorld(FVector(-10.0f - Row * 5.0f, SideSign * (4.0f + Row * 1.5f), 20.0f - Row * 1.5f));
			Direction = (-Forward * 0.8f + Side * SideSign * (0.45f + Sway) + FVector::UpVector * (0.75f - Row * 0.05f)).GetSafeNormal();
			Length = (28.0f + Row * 2.0f) * Size;
		}
		else
		{
			const float Along = 2.25f + 0.7f * (Index - DragonSkullMane) / FMath::Max(DragonNeckMane - 1, 1);
			Root = GetHookCurve(Along);
			Direction = (FVector::UpVector * 0.8f + Side * SideSign * (0.5f + Sway) - Forward * 0.3f).GetSafeNormal();
			Length = 26.0f * Size;
		}
		const FVector End = Root + Direction * Length;
		Stalk->SetStartAndEnd(Transform.InverseTransformPosition(Root), Transform.InverseTransformVectorNoScale(Direction * Length),
			Transform.InverseTransformPosition(End), Transform.InverseTransformVectorNoScale(Direction * Length), false);
		Stalk->SetStartScale(FVector2D(Plant->FingerThickness * 1.6f * Size), false);
		Stalk->SetEndScale(FVector2D(Plant->FingerThickness * 1.2f * Size), false);
		Stalk->UpdateMesh();
		if (Head != nullptr)
		{
			Head->SetWorldLocationAndRotation(End, FRotationMatrix::MakeFromZ(Direction).Rotator());
			Head->SetWorldScale3D(FVector(FMath::Max(HeadScale * 0.8f * Size, 0.001f)));
		}
	}

	// The thistle ring in the throat, turning; the mouth is the middle between the jaws
	const FVector MouthLocal(30.0f, 0.0f, -9.0f - Jaw * 10.0f);
	for (int32 Index = 0; Index < DrumRegrow.Num(); ++Index)
	{
		DrumRegrow[Index] = FMath::Min(1.0f, DrumRegrow[Index] + DeltaSeconds / 0.9f);
		UStaticMeshComponent* Head = DragonHeads.IsValidIndex(Index) ? DragonHeads[Index].Get() : nullptr;
		if (Head == nullptr)
		{
			continue;
		}
		Head->SetVisibility(bShown && DrumRegrow[Index] > 0.05f);
		const float Angle = DrumAngle + Index * 2.0f * UE_PI / DragonDrumHeads;
		const FVector Local = MouthLocal + FVector(0.0f, FMath::Cos(Angle) * 9.0f, FMath::Sin(Angle) * 9.0f);
		Head->SetWorldLocationAndRotation(ToWorld(Local), FRotationMatrix::MakeFromZ(Forward).Rotator());
		Head->SetWorldScale3D(FVector(FMath::Max(HeadScale * 0.75f * Size * FMath::SmoothStep(0.0f, 1.0f, DrumRegrow[Index]), 0.001f)));
	}

	// Eyes and the glow in the throat
	if (DragonGlows.Num() >= 3)
	{
		for (int32 Eye = 0; Eye < 2; ++Eye)
		{
			UStaticMeshComponent* Orb = DragonGlows[Eye];
			Orb->SetVisibility(bShown);
			Orb->SetWorldLocation(ToWorld(FVector(38.0f, Eye == 0 ? 14.0f : -14.0f, 15.0f)));
			Orb->SetWorldScale3D(FVector(FMath::Max(6.0f * Size / 100.0f, 0.001f)));
			VaelEffects::SetLookGlow(Orb, 3.0f + 4.0f * Jaw);
		}
		UStaticMeshComponent* Throat = DragonGlows[2];
		const float Heat = FMath::Clamp((Jaw - 0.1f) / 0.6f, 0.0f, 1.0f);
		Throat->SetVisibility(bShown && Heat > 0.03f);
		Throat->SetWorldLocation(ToWorld(MouthLocal - FVector(6.0f, 0.0f, 0.0f)));
		Throat->SetWorldScale3D(FVector(FMath::Max(14.0f * Size * Heat / 100.0f, 0.001f)));
		VaelEffects::SetLookGlow(Throat, 5.0f * Heat);
	}

	// While the head grows, sparks run from where the other Glutdistel sank in, into the foot and up the snake
	for (int32 Index = 0; Index < DragonSurgeOrbs; ++Index)
	{
		UStaticMeshComponent* Orb = DragonGlows.IsValidIndex(3 + Index) ? DragonGlows[3 + Index].Get() : nullptr;
		if (Orb == nullptr)
		{
			continue;
		}
		const float Way = DragonGrow * 1.5f - Index * 0.06f;
		const bool bRuns = Way > 0.0f && Way < 1.0f && !bWithering;
		Orb->SetVisibility(bRuns);
		if (!bRuns)
		{
			continue;
		}
		const FVector Place = Way < 0.25f
			? FMath::Lerp(FeedFrom, GetActorLocation(), Way / 0.25f) + FVector(0.0f, 0.0f, 8.0f + 20.0f * FMath::Sin(Way / 0.25f * UE_PI))
			: GetHookCurve((Way - 0.25f) / 0.75f * 3.0f);
		Orb->SetWorldLocation(Place);
		Orb->SetWorldScale3D(FVector(9.0f / 100.0f));
		VaelEffects::SetLookGlow(Orb, 6.0f);
	}

	// The drumfire: one thistle after the other round the ring, flat and straight at the target
	AVaelCharacter* Gardener = GetGardener();
	if (bWithering)
	{
		BurstLeft = 0;
	}
	else if (BurstLeft > 0 && Gardener != nullptr)
	{
		BurstTimer -= DeltaSeconds;
		if (Jaw > 0.6f && BurstTimer <= 0.0f)
		{
			BurstTimer = 0.075f;
			--BurstLeft;
			const int32 Index = NextDrum;
			NextDrum = (NextDrum + 1) % DragonDrumHeads;
			const float Angle = DrumAngle + Index * 2.0f * UE_PI / DragonDrumHeads;
			const FVector Muzzle = ToWorld(MouthLocal + FVector(14.0f, FMath::Cos(Angle) * 9.0f, FMath::Sin(Angle) * 9.0f));
			FVector Toward = BurstTarget.IsValid() ? BurstTarget->GetActorLocation() - Muzzle : Forward;
			Toward.Z = 0.0f;
			Toward = FRotator(0.0f, FMath::FRandRange(-6.0f, 6.0f), 0.0f).RotateVector(Toward.GetSafeNormal());
			VaelSymbiosis::FireShot(Gardener, Plant, Muzzle, Toward, BurstPower, bShadow, Settings->TowerShotScale * 0.85f, 0.6f);
			VaelSymbiosis::MuzzlePuff(this, Plant, Muzzle, bShadow, 0.8f);
			if (DrumRegrow.IsValidIndex(Index))
			{
				DrumRegrow[Index] = 0.0f;
			}
			BendVelocity -= Forward * 0.35f;
		}
	}
}
