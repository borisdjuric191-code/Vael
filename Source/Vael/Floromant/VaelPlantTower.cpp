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
#include "Combat/VaelHitFeedbackSubsystem.h"
#include "Magic/VaelSpellEffects.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Player/VaelCharacter.h"
#include "UI/VaelCombatTextSubsystem.h"
#include "World/VaelSafeZone.h"
#include "Combat/VaelCombatStatics.h"
#include "Combat/VaelCharacterBase.h"
#include "Magic/VaelElementTypes.h"
#include "Magic/VaelGroundArea.h"
#include "Magic/VaelCloudCards.h"
#include "Magic/VaelLightningBolt.h"
#include "Magic/VaelMagicSettings.h"
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
	// The dragon's neck is a chain of springy joints of its own (UpdateDragonNeck)
	if (bDragon && bNeckPlaced)
	{
		return NeckApex;
	}
	const float Length = UVaelFloromantSettings::Get()->TowerHeight * FMath::Sin(FMath::Clamp(Grow, 0.0f, 1.0f) * UE_HALF_PI)
		* (1.0f + (UVaelFloromantSettings::Get()->TowerCallSizeScale - 1.0f) * Giant);
	// The bend of the spring only sways the curl a little; the neck below swings further
	return GetActorLocation() + HookForward(LookDirection) * Length * 0.05f + FVector::UpVector * Length + Bend * Length * 0.25f;
}

FVector AVaelPlantTower::GetOwnCrown() const
{
	if (bHook && bDragon && bNeckPlaced)
	{
		return NeckHead;
	}
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
	if (bHook && bDragon && bNeckPlaced)
	{
		return HeadFacing;
	}
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
	if (WoodMaterial != nullptr)
	{
		WoodMaterial->SetScalarParameterValue(TEXT("Wuchs"), FMath::Clamp(Grow, 0.0f, 1.0f));
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
			// The dragon waits with its drumfire while it bites
			if (Goal != nullptr && (((AimsLikeHydra() || bDragon) && (FVector::DotProduct(LookDirection, GoalWay) < 0.95f || AimBlend < 0.7f)) || (bDragon && BiteTime >= 0.0f)))
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
	// Only once her Drachenerbe of the pair blooms (Keimgeflecht); before that they grow into the twisted trunk. The Glutdistel is
	// always the snake that carries the head; the other plant sinks in and builds the organ in its jaws (Dampfdrache: Taublatt)
	const AVaelCharacter* Grower = GetGardener();
	const bool bHeritage = Grower != nullptr && Grower->GetFloromant() != nullptr && Grower->GetFloromant()->IsHeritageAwake(Symbiosis);
	// Which plant carries the head: the Glutdistel before the Taublatt before any other,
	// with two of a kind the older tower
	auto BodyRank = [](const AVaelPlantTower* Tower)
	{
		FString Id = Tower->Plant != nullptr ? Tower->Plant->GetName() : FString();
		Id.RemoveFromStart(TEXT("DA_Pflanzenarm_"));
		return Id == TEXT("Glutdistel") ? 2 : Id == TEXT("Taublatt") ? 1 : 0;
	};
	AVaelPlantTower* Body = Other;
	AVaelPlantTower* Sunk = this;
	if (BodyRank(this) > BodyRank(Other))
	{
		Swap(Body, Sunk);
	}
	// The body must be a snake; the plant that sinks in may be any (the Glimmklee of a Glutglasdrache stands upright)
	if (VaelSymbiosis::HasHeritage(Symbiosis) && bHeritage && Body->bHook && !Other->bDragon && !bDragon)
	{
		if (AVaelGlimmDome* SunkDome = Sunk->Dome.Get())
		{
			SunkDome->Lower();
		}
		Sunk->Dome = nullptr;
		Sunk->bLeads = false;
		Body->bLeads = true;
		Sunk->bAbsorbed = true;
		Sunk->SetActorHiddenInGame(true);
		Body->FeedFrom = Sunk->GetActorLocation();
		Body->DragonPartner = Sunk->Plant;
		Body->PartnerMaterial = Sunk->Material;
		Body->bSteamDragon = Symbiosis == EVaelSymbiosis::Dampfstoss;
		Body->bThunderDragon = Symbiosis == EVaelSymbiosis::Funkenflug;
		Body->bSunDragon = Symbiosis == EVaelSymbiosis::Glutglas;
		Body->bWingDragon = Symbiosis == EVaelSymbiosis::Sturmgras;
		Body->bFloodDragon = Symbiosis == EVaelSymbiosis::Quellrose;
		Body->BuildDragon();
		Body->FireCountdown = 1.8f;
		if (AVaelCharacter* Gardener = GetGardener())
		{
			UVaelCombatTextSubsystem::PostPickup(Gardener, FText::Format(NSLOCTEXT("VaelSymbiosis", "TowersFuse", "Symbiose: {0}"), VaelSymbiosis::GetHeritageName(Symbiosis)), VaelSymbiosis::GetColor(Symbiosis));
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
	// The steam dragon squeezes its gland and breathes a jet of steam over a fan (UpdateDragon)
	// The thunder dragon opens its jaws, the blades crackle, and lightning strikes twice out of its throat (UpdateDragon)
	if (bDragon && bThunderDragon)
	{
		ThunderLeft = UVaelFloromantSettings::Get()->DragonThunderSeconds;
		ThunderTimer = 0.22f;
		ThunderStrikes = 2;
		ThunderPower = Power;
		BurstTarget = Goal;
		return;
	}
	// The flood dragon fans its frill open and a gush of water pours out of its jaws (UpdateDragon)
	if (bDragon && bFloodDragon)
	{
		SunLeft = UVaelFloromantSettings::Get()->DragonFloodSeconds;
		bSunLit = false;
		SunPower = Power;
		BurstTarget = Goal;
		return;
	}
	// The storm dragon roars, raises its wings and sweeps them forward: their blades fly off as a fan of darts (UpdateWings)
	if (bDragon && bWingDragon)
	{
		WingStrike = 0.0f;
		bWingFired = false;
		WingPower = Power;
		BurstTarget = Goal;
		return;
	}
	// The sun dragon opens its jaws and a focused sun beam leaves its burning glass, sweeping over the fan (UpdateDragon)
	if (bDragon && bSunDragon)
	{
		SunLeft = UVaelFloromantSettings::Get()->DragonSunSeconds;
		bSunLit = false;
		SunPower = Power;
		BurstTarget = Goal;
		return;
	}
	if (bDragon && bSteamDragon)
	{
		JetLeft = UVaelFloromantSettings::Get()->DragonSteamSeconds;
		JetTimer = 0.0f;
		JetPower = Power;
		JetPuffs = 0;
		BurstTarget = Goal;
		bSprayed = false;
		return;
	}
	if (bDragon)
	{
		BurstLeft = DragonDrumHeads;
		BurstTimer = 0.0f;
		BurstPower = Power;
		BurstTarget = Goal;
		bSprayed = false;
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

	// A Schnappwurz tower is a trap (2026-11-19): its buds snap at whoever comes close (its reach is short), bite, hold and
	// (as Würgmaul) drain; the crown lunges at the prey
	if (Symbiosis == EVaelSymbiosis::None && Plant->bJaw)
	{
		if (FVector::Dist2D(Goal->GetActorLocation(), GetActorLocation()) <= Plant->BiteReach * 1.4f && UVaelCombatStatics::CanDamage(Gardener, Goal))
		{
			const float Damage = Plant->BiteDamage * Power * UVaelFloromantSettings::Get()->TowerShotScale * 0.6f;
			if (bShadow)
			{
				VaelRoseCare::DrainAround(Gardener, Goal->GetActorLocation(), 60.0f, Damage, Plant->BiteDrainShare);
			}
			else
			{
				FVaelSpellHit Hit;
				Hit.Damage = Damage;
				Hit.Element = EVaelElement::Earth;
				Hit.StunDuration = Plant->BiteHold;
				UVaelCombatStatics::ApplySpellHit(Gardener, Goal, Hit, (Goal->GetActorLocation() - GetActorLocation()).GetSafeNormal2D());
			}
			VaelSymbiosis::MuzzlePuff(this, Plant, Goal->GetActorLocation() + FVector(0.0f, 0.0f, 40.0f), bShadow, 1.4f);
			BendVelocity += LookDirection * 4.0f;
		}
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
	if (bDragon)
	{
		const UVaelFloromantSettings* NeckSettings = UVaelFloromantSettings::Get();
		UpdateDragonNeck(DeltaSeconds, NeckSettings->TowerHeight * FMath::Sin(FMath::Clamp(Grow, 0.0f, 1.0f) * UE_HALF_PI) * (1.0f + (NeckSettings->TowerCallSizeScale - 1.0f) * Giant));
	}

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

	// The storm dragon is slim: its neck is thinner
	// The braided strands of the root wood are fuller than the plant vines, so they are laid thinner
	const float NeckSlim = (bDragon && bWingDragon ? 0.55f : 1.0f) * (WoodMaterial != nullptr ? 0.62f : 1.0f);

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

			// The dragon's neck runs through its own joints: the dip follows late, the curl turns over towards it
			if (bDragon && bNeckPlaced)
			{
				Dip = NeckDip + Out1 * 8.0f;
				DipTangent = (NeckHead - NeckApex) * 0.55f + Around1 * Height * 0.1f;
				EndTangent = ((NeckDip - NeckApex).GetSafeNormal() * 0.5f + Forward * 0.5f).GetSafeNormal() * Height * 0.7f + ApexAround * Height * 0.2f;
			}

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
		Vine->SetStartScale(FVector2D(3.0f * NeckSlim), false);
		Vine->SetEndScale(FVector2D(EndScale * NeckSlim), false);
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
			const float Scales[3] = { EndScale * NeckSlim, 2.0f * NeckSlim, 1.7f * NeckSlim };
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

	// The dragon of root wood moves on its own free spine: the snake it grew from is gone
	if (bDragon && !SpinePieces.IsEmpty() && Spine.Num() >= 3)
	{
		for (TArray<TObjectPtr<USplineMeshComponent>>* Vines : { &Column, &HookNeck })
		{
			for (USplineMeshComponent* Vine : *Vines)
			{
				if (Vine->IsVisible())
				{
					Vine->SetVisibility(false);
				}
			}
		}
		LaySpine(Height, NeckSlim);
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
	/** One vine of the dragon head in its own frame (cm at full size; X ahead, Y right, Z up, the crown of the snake at 0):
	 *  from A to B with their tangents, thick to thin; jaw vines sit in the jaw; mirrored ones come in pairs. Each is laid as a
	 *  bundle of a thick strand and thinner ones tangled round it, so the head is a dense mass of roots */
	struct FDragonVine
	{
		FVector A, TA, B, TB;
		float S0, S1;
		bool bJaw;
		bool bMirror;
	};

	const FDragonVine DragonVineTable[] =
	{
		// Skull: a heavy, round mass; ridge, the mass at the back, brows, the heavy ridge over the sunken eyes, cheeks, upper lip,
		// blunt nostrils
		{ FVector(-18, 0, 22), FVector(70, 0, 30), FVector(72, 0, 6), FVector(45, 0, -35), 2.8f, 1.6f, false, false },
		{ FVector(-24, 0, 8), FVector(10, 0, 40), FVector(-2, 0, 30), FVector(40, 0, 10), 3.2f, 2.4f, false, false },
		{ FVector(-22, 14, 6), FVector(10, 10, 30), FVector(2, 18, 24), FVector(40, 0, 0), 2.8f, 2.0f, false, true },
		{ FVector(-14, 12, 20), FVector(55, 6, 30), FVector(58, 9, 9), FVector(45, -4, -30), 2.5f, 1.5f, false, true },
		{ FVector(6, 14, 24), FVector(30, 10, 16), FVector(42, 17, 15), FVector(20, 3, -12), 2.6f, 1.4f, false, true },
		{ FVector(-14, 20, 2), FVector(55, 4, 0), FVector(68, 11, -2), FVector(55, -10, -4), 2.6f, 1.5f, false, true },
		{ FVector(2, 17, -8), FVector(55, 0, 0), FVector(76, 6, -4), FVector(35, -6, 4), 2.0f, 1.2f, false, true },
		{ FVector(62, 6, 8), FVector(18, 8, 8), FVector(74, 10, -3), FVector(0, 0, -18), 1.6f, 1.0f, false, true },
		// Lower jaw, deep and heavy: jaw line, chin, the inside
		{ FVector(-10, 18, -10), FVector(50, -2, -15), FVector(66, 7, -16), FVector(45, -8, 5), 2.6f, 1.4f, true, true },
		{ FVector(4, 0, -24), FVector(55, 0, -6), FVector(62, 0, -18), FVector(35, 0, 6), 2.4f, 1.4f, true, false },
		{ FVector(0, 11, -17), FVector(55, -2, 0), FVector(58, 5, -12), FVector(35, -4, 0), 1.8f, 1.1f, true, true },
		// Short horns of twisted root swept back, and roots hanging off the chin and jaw
		{ FVector(-10, 10, 24), FVector(-25, 8, 30), FVector(-38, 20, 40), FVector(-20, 4, -15), 2.0f, 0.6f, false, true },
		{ FVector(-16, 6, 18), FVector(-30, 4, 15), FVector(-34, 12, 30), FVector(-6, 4, 24), 1.6f, 0.5f, false, true },
		{ FVector(10, 6, -24), FVector(0, 2, -25), FVector(4, 9, -46), FVector(-6, 0, -20), 1.4f, 0.3f, true, true },
		{ FVector(30, 4, -22), FVector(0, 0, -20), FVector(32, 6, -40), FVector(0, 0, -20), 1.2f, 0.25f, true, true },
	};

	/** Strands per vine (the first thick, the others thinner and tangled round it) */
	constexpr int32 DragonStrands = 3;

	/** Hinge of the jaw and how far it opens */
	const FVector DragonJawHinge(-6.0f, 0.0f, -6.0f);
	constexpr float DragonJawDegrees = 38.0f;

	/** Size of the head against a 2 m tower */
	constexpr float DragonHeadScale = 0.62f;

	/** Long thorn teeth on each side of the upper and lower jaw, and the front teeth */
	constexpr int32 DragonTeethPerSide = 9;

	/** Root fibres sticking out of the skull all over, thistles grown into the tangle, root tufts along the neck */
	constexpr int32 DragonFibres = 46;
	constexpr int32 DragonSkullThistles = 6;
	constexpr int32 DragonNeckTufts = 10;
	constexpr int32 DragonNeckThistles = 3;

	/** Sparks that run up the snake while the head grows */
	constexpr int32 DragonSurgeOrbs = 8;

	/** Roots of the sunk partner winding up the neck, and the pieces each is laid in */
	constexpr int32 DragonPartnerRoots = 5;
	constexpr int32 DragonPartnerPieces = 8;

	/** Side shoots branching off each of those roots */
	constexpr int32 DragonPartnerSpurs = 3;

	/** Cloud cards of the steam jet, how many are born per second, how long one lives, how fast it leaves the jaws (cm/s, against
	 *  a 2 m tower), its size at birth and at the end, how dense it is; the eyes and sparks of the steam dragon */
	constexpr int32 DragonJetCards = 72;
	constexpr float DragonJetRate = 60.0f;
	constexpr float DragonJetLife = 1.1f;
	constexpr float DragonJetSpeed = 1500.0f;
	constexpr float DragonJetBirthSize = 40.0f;
	constexpr float DragonJetEndSize = 230.0f;
	constexpr float DragonJetSolid = 0.95f;
	const FLinearColor DragonSteamEyes(0.35f, 0.9f, 1.0f);
	const FLinearColor DragonThunderLight(0.6f, 0.8f, 1.0f);
	const FLinearColor DragonSunLight(1.0f, 0.85f, 0.4f);
	const FLinearColor DragonWindLight(0.75f, 1.0f, 0.6f);
	const FLinearColor DragonFloodLight(0.3f, 0.6f, 1.0f);

	/** Blades in each wing of the storm dragon */
	constexpr int32 DragonWingFeathers = 8;

	/** Ribs of root strands in each wing (the blades span the gaps between them, two in each) */
	constexpr int32 DragonWingRibs = 5;

	FVector DragonHermite(const FVector& P0, const FVector& T0, const FVector& P1, const FVector& T1, float T)
	{
		const float T2 = T * T;
		const float T3 = T2 * T;
		return P0 * (2.0f * T3 - 3.0f * T2 + 1.0f) + T0 * (T3 - 2.0f * T2 + T) + P1 * (3.0f * T2 - 2.0f * T3) + T1 * (T3 - T2);
	}

	/** Lays a spline piece once, in the frame of its parent */
	void LayPiece(USplineMeshComponent* Piece, const FVector& A, const FVector& TA, const FVector& B, const FVector& TB, float S0, float S1, float Roll)
	{
		Piece->SetStartAndEnd(A, TA, B, TB, false);
		Piece->SetStartScale(FVector2D(S0), false);
		Piece->SetEndScale(FVector2D(S1), false);
		Piece->SetStartRoll(Roll, false);
		Piece->SetEndRoll(Roll + 2.0f, false);
		Piece->UpdateMesh();
	}
}

FVector AVaelPlantTower::GetSpinePoint(float U) const
{
	const int32 Count = Spine.Num();
	if (Count < 2)
	{
		return GetActorLocation();
	}
	const float At = FMath::Clamp(U, 0.0f, 1.0f) * (Count - 1);
	const int32 K = FMath::Min(FMath::FloorToInt(At), Count - 2);
	const float F = At - K;
	// Catmull-Rom through the joints
	const FVector P0 = Spine[FMath::Max(K - 1, 0)];
	const FVector P1 = Spine[K];
	const FVector P2 = Spine[K + 1];
	const FVector P3 = Spine[FMath::Min(K + 2, Count - 1)];
	const float F2 = F * F;
	const float F3 = F2 * F;
	return 0.5f * ((2.0f * P1) + (P2 - P0) * F + (2.0f * P0 - 5.0f * P1 + 4.0f * P2 - P3) * F2 + (3.0f * P1 - P0 - 3.0f * P2 + P3) * F3);
}

FVector AVaelPlantTower::GetHookCurve(float T) const
{
	// The dragon's body is its free spine (0 at the roots, 3 at the head, as the question mark it grew from)
	if (bDragon && bNeckPlaced && Spine.Num() >= 2)
	{
		return GetSpinePoint(T / 3.0f);
	}
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
	FRandomStream Random(GetUniqueID() * 7 + 13);

	HeadFrame = NewObject<USceneComponent>(this);
	HeadFrame->SetupAttachment(RootComponent);
	HeadFrame->SetUsingAbsoluteLocation(true);
	HeadFrame->SetUsingAbsoluteRotation(true);
	HeadFrame->SetUsingAbsoluteScale(true);
	HeadFrame->RegisterComponent();
	HeadFrame->SetVisibility(false, true);

	JawFrame = NewObject<USceneComponent>(this);
	JawFrame->SetupAttachment(HeadFrame);
	JawFrame->RegisterComponent();
	JawFrame->SetRelativeLocation(DragonJawHinge);

	UMaterialInterface* PieceMaterial = Material;
	auto MakePiece = [this, PieceMaterial](UStaticMesh* Mesh, USceneComponent* Parent)
	{
		USplineMeshComponent* Piece = NewObject<USplineMeshComponent>(this);
		Piece->SetMobility(EComponentMobility::Movable);
		Piece->SetForwardAxis(ESplineMeshAxis::Z, false);
		Piece->SetStaticMesh(Mesh);
		Piece->SetMaterial(0, PieceMaterial);
		Piece->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Piece->SetCanEverAffectNavigation(false);
		Piece->SetupAttachment(Parent);
		Piece->RegisterComponent();
		DragonVines.Add(Piece);
		return Piece;
	};
	UStaticMesh* TipMesh = VaelAssets::LoadOptional(Plant->TipMesh);
	auto MakeHead = [this, TipMesh, PieceMaterial](USceneComponent* Parent)
	{
		UStaticMeshComponent* Head = NewObject<UStaticMeshComponent>(this);
		Head->SetMobility(EComponentMobility::Movable);
		Head->SetStaticMesh(TipMesh);
		Head->SetMaterial(0, PieceMaterial);
		Head->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Head->SetCanEverAffectNavigation(false);
		Head->SetupAttachment(Parent);
		Head->RegisterComponent();
		DragonHeads.Add(Head);
		return Head;
	};

	// The shared root wood (Boris 2026-11-19): every dragon has the same woody skull, jaw and neck; the body plant colours its
	// veins, the partner adds its colour to them, and the plants adorn it with their organs and leaves
	const UVaelFloromantSettings* WoodSettings = UVaelFloromantSettings::Get();
	UStaticMesh* SkullMesh = VaelAssets::LoadOptional(WoodSettings->DragonSkullMesh);
	UStaticMesh* JawMesh = VaelAssets::LoadOptional(WoodSettings->DragonJawMesh);
	if (UMaterialInterface* Wood = VaelAssets::LoadOptional(WoodSettings->DragonWoodMaterial))
	{
		WoodMaterial = UMaterialInstanceDynamic::Create(Wood, this);
		WoodMaterial->SetScalarParameterValue(TEXT("Schatten"), bShadow ? 1.0f : 0.0f);
		WoodMaterial->SetVectorParameterValue(TEXT("Leuchtfarbe"), VaelSymbiosis::GetPlantColor(Plant, bShadow));
		if (DragonPartner != nullptr)
		{
			WoodMaterial->SetVectorParameterValue(TEXT("Akzentfarbe"), VaelSymbiosis::GetPlantColor(DragonPartner, bShadow));
			WoodMaterial->SetScalarParameterValue(TEXT("Akzentanteil"), 0.6f);
		}
	}
	const bool bWood = WoodMaterial != nullptr && SkullMesh != nullptr && JawMesh != nullptr;
	if (bWood)
	{
		auto MakeWood = [this](UStaticMesh* Mesh, USceneComponent* Parent)
		{
			UStaticMeshComponent* Part = NewObject<UStaticMeshComponent>(this);
			Part->SetMobility(EComponentMobility::Movable);
			Part->SetStaticMesh(Mesh);
			Part->SetMaterial(0, WoodMaterial);
			Part->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			Part->SetCanEverAffectNavigation(false);
			Part->SetupAttachment(Parent);
			Part->RegisterComponent();
			return Part;
		};
		WoodSkull = MakeWood(SkullMesh, HeadFrame);
		// The jaw mesh has its origin at the hinge, like the jaw frame
		WoodJaw = MakeWood(JawMesh, JawFrame);

		// The snake itself turns to root wood: its vines become braided strands
		int32 StrandPick = 0;
		for (TArray<TObjectPtr<USplineMeshComponent>>* Vines : { &Column, &HookNeck })
		{
			for (USplineMeshComponent* Vine : *Vines)
			{
				if (UStaticMesh* Strand = VaelAssets::LoadOptional(WoodSettings->DragonStrandMeshes.IsEmpty() ? TSoftObjectPtr<UStaticMesh>() : WoodSettings->DragonStrandMeshes[StrandPick++ % WoodSettings->DragonStrandMeshes.Num()]))
				{
					Vine->SetStaticMesh(Strand);
				}
				Vine->SetMaterial(0, WoodMaterial);
			}
		}

		// The free spine: per vine one strand for each of the five pieces of the body, a thick core, and the roots in the ground
		// (laid each frame by LaySpine; the column and neck of the snake it grew from are hidden then)
		constexpr int32 Segments = 5;
		for (int32 Index = 0; Index < (Column.Num() + 1) * Segments; ++Index)
		{
			// One kind of strand along a whole vine, so its pieces meet seamlessly
			UStaticMesh* Strand = WoodSettings->DragonStrandMeshes.IsEmpty() ? nullptr : VaelAssets::LoadOptional(WoodSettings->DragonStrandMeshes[(Index / Segments) % WoodSettings->DragonStrandMeshes.Num()]);
			USplineMeshComponent* Piece = MakeStalk(Strand);
			Piece->SetMaterial(0, WoodMaterial);
			SpinePieces.Add(Piece);
		}
		for (int32 Index = 0; Index < 7; ++Index)
		{
			UStaticMesh* Strand = WoodSettings->DragonStrandMeshes.IsEmpty() ? nullptr : VaelAssets::LoadOptional(WoodSettings->DragonStrandMeshes[(Index + 1) % WoodSettings->DragonStrandMeshes.Num()]);
			USplineMeshComponent* Root = MakeStalk(Strand);
			Root->SetMaterial(0, WoodMaterial);
			GroundRoots.Add(Root);
		}
		// It bursts out of the earth (UpdateDragonNeck)
		EmergeTime = 0.0f;
		EmergeRumble = 0.0f;
	}

	// The storm dragon is slim: thinner strands in its skull
	const float Slim = bWingDragon ? 0.55f : 1.0f;

	// Skull and jaw: every vine a bundle of tangled strands (only without the root wood)
	int32 Pick = 0;
	for (const FDragonVine& Vine : bWood ? TArrayView<const FDragonVine>() : TArrayView<const FDragonVine>(DragonVineTable))
	{
		for (int32 Mirror = 0; Mirror < (Vine.bMirror ? 2 : 1); ++Mirror)
		{
			const FVector Flip(1.0f, Mirror == 0 ? 1.0f : -1.0f, 1.0f);
			const FVector Shift = Vine.bJaw ? DragonJawHinge : FVector::ZeroVector;
			for (int32 Strand = 0; Strand < DragonStrands; ++Strand)
			{
				const FVector Jitter = Strand == 0 ? FVector::ZeroVector : FVector(Random.FRandRange(-3.0f, 3.0f), Random.FRandRange(-4.0f, 4.0f), Random.FRandRange(-4.0f, 4.0f));
				const FVector Bow = Strand == 0 ? FVector::ZeroVector : Random.GetUnitVector() * 18.0f;
				const float Thin = Strand == 0 ? 1.0f : Random.FRandRange(0.45f, 0.65f);
				USplineMeshComponent* Piece = MakePiece(TowerFirstMesh({ &Plant->VineMeshes, &Plant->ArmMeshes, &Plant->FingerMeshes }, Pick++), Vine.bJaw ? JawFrame.Get() : HeadFrame.Get());
				LayPiece(Piece, Vine.A * Flip - Shift + Jitter, Vine.TA * Flip + Bow, Vine.B * Flip - Shift - Jitter, Vine.TB * Flip - Bow,
					Vine.S0 * Thin * Slim, Vine.S1 * Thin * Slim, Random.FRandRange(0.0f, 6.0f));
			}
		}
	}

	// Long thorn teeth hanging from the upper jaw and standing up from the lower one, the front ones longest (the root wood has
	// its own splinter teeth)
	for (int32 Mirror = 0; Mirror < (bWood ? 0 : 2); ++Mirror)
	{
		const float SideSign = Mirror == 0 ? 1.0f : -1.0f;
		for (int32 Tooth = 0; Tooth < DragonTeethPerSide; ++Tooth)
		{
			const float Along = Tooth / float(DragonTeethPerSide - 1);
			const float Length = FMath::Lerp(7.0f, 13.0f, Along) * Random.FRandRange(0.8f, 1.2f);
			// Upper
			{
				const FVector Root(FMath::Lerp(10.0f, 72.0f, Along), SideSign * FMath::Lerp(15.0f, 5.0f, Along), -6.0f);
				const FVector Tip = Root + FVector(-2.0f, -SideSign * 1.5f, -Length);
				LayPiece(MakePiece(TowerFirstMesh({ &Plant->FingerMeshes, &Plant->ArmMeshes }, Pick++), HeadFrame), Root, (Tip - Root), Tip, (Tip - Root) + FVector(-3.0f, 0.0f, 0.0f),
					0.75f, 0.06f, Random.FRandRange(0.0f, 6.0f));
			}
			// Lower (in the jaw)
			{
				const FVector Root(FMath::Lerp(8.0f, 62.0f, Along), SideSign * FMath::Lerp(16.0f, 6.0f, Along), -15.0f);
				const FVector Tip = Root + FVector(1.5f, -SideSign * 1.5f, Length * 0.85f);
				LayPiece(MakePiece(TowerFirstMesh({ &Plant->FingerMeshes, &Plant->ArmMeshes }, Pick++), JawFrame), Root - DragonJawHinge, (Tip - Root), Tip - DragonJawHinge, (Tip - Root) + FVector(3.0f, 0.0f, 0.0f),
					0.7f, 0.06f, Random.FRandRange(0.0f, 6.0f));
			}
		}
	}

	// Root fibres sticking out of the skull all over, bending back, and thistles grown into the tangle
	TArray<FVector> FibreRoots;
	for (int32 Index = 0; Index < DragonFibres; ++Index)
	{
		// A point on the upper half of the skull, roughly an egg round its middle
		FVector Out = Random.GetUnitVector();
		Out.Z = FMath::Abs(Out.Z) * 1.2f - 0.2f;
		Out = Out.GetSafeNormal();
		const FVector Root = FVector(20.0f, 0.0f, 6.0f) + Out * FVector(42.0f, 19.0f, 18.0f);
		const FVector Way = (Out + FVector(-0.5f, 0.0f, 0.15f) + Random.GetUnitVector() * 0.35f).GetSafeNormal();
		const float Length = Random.FRandRange(8.0f, 22.0f);
		const FVector Tip = Root + Way * Length;
		// The root wood brings its own shaggy fibres; the roots are still the places of the thistles
		const float Thin = Random.FRandRange(0.3f, 0.5f);
		const float FibreRoll = Random.FRandRange(0.0f, 6.0f);
		if (!bWood)
		{
			LayPiece(MakePiece(TowerFirstMesh({ &Plant->FingerMeshes, &Plant->ArmMeshes }, Pick++), HeadFrame), Root, Way * Length, Tip, (Way + FVector(-0.4f, 0.0f, -0.3f)).GetSafeNormal() * Length,
				Thin, 0.05f, FibreRoll);
		}
		FibreRoots.Add(Root + Way * 3.0f);
	}

	// Heads: the throat ring first (placed each frame), then the thistles in the tangle, then those of the neck
	if (TipMesh != nullptr)
	{
		for (int32 Index = 0; Index < DragonDrumHeads; ++Index)
		{
			MakeHead(HeadFrame);
		}
		const float HeadScale = FMath::Max(Plant->TipSize * 1.6f / 10.0f, 0.01f);
		for (int32 Index = 0; Index < DragonSkullThistles; ++Index)
		{
			UStaticMeshComponent* Thistle = MakeHead(HeadFrame);
			FVector Root = FibreRoots[(Index * 7 + 3) % FibreRoots.Num()];
			FVector Out = (Root - FVector(20.0f, 0.0f, 6.0f)).GetSafeNormal();
			// On the root wood they grow in an arc over the back of the skull, in the mane, so they don't read as more eyes
			if (bWood)
			{
				const float Around = FMath::Lerp(-1.1f, 1.1f, Index / float(FMath::Max(DragonSkullThistles - 1, 1)));
				Out = FVector(-0.6f, FMath::Sin(Around), FMath::Cos(Around)).GetSafeNormal();
				Root = FVector(-16.0f - 4.0f * (Index % 2), FMath::Sin(Around) * 17.0f, 13.0f + FMath::Cos(Around) * 15.0f);
			}
			Thistle->SetRelativeLocationAndRotation(Root, FRotationMatrix::MakeFromZ(Out).Rotator());
			Thistle->SetRelativeScale3D(FVector(HeadScale * 0.6f));
		}
		for (int32 Index = 0; Index < DragonNeckThistles; ++Index)
		{
			UStaticMeshComponent* Thistle = MakeHead(RootComponent);
			Thistle->SetUsingAbsoluteLocation(true);
			Thistle->SetUsingAbsoluteRotation(true);
			Thistle->SetUsingAbsoluteScale(true);
			Thistle->SetVisibility(false);
		}
	}
	DrumRegrow.Init(1.0f, DragonDrumHeads);

	// The steam gland: four petals of the sunk Taublatt round the throat, clamping a glowing Glutdistel (placed each frame); a
	// few more of its petals grow out of the tangle of the skull like leaves
	UStaticMesh* PetalMesh = DragonPartner != nullptr ? VaelAssets::LoadOptional(DragonPartner->PetalMesh) : nullptr;
	if (bSteamDragon && PetalMesh != nullptr)
	{
		UMaterialInterface* PetalMaterial = PartnerMaterial != nullptr ? PartnerMaterial.Get() : PieceMaterial;
		auto MakePetal = [this, PetalMesh, PetalMaterial]()
		{
			UStaticMeshComponent* Petal = NewObject<UStaticMeshComponent>(this);
			Petal->SetMobility(EComponentMobility::Movable);
			Petal->SetStaticMesh(PetalMesh);
			Petal->SetMaterial(0, PetalMaterial);
			Petal->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			Petal->SetCanEverAffectNavigation(false);
			Petal->SetupAttachment(HeadFrame);
			Petal->RegisterComponent();
			return Petal;
		};
		for (int32 Index = 0; Index < 4; ++Index)
		{
			GlandPetals.Add(MakePetal());
		}
		// The head adorned with Taublatt as a headdress (Boris 2026-11-19): a crown of large petals fanning up and back from the
		// back of the skull, two long petals sweeping back from the temples like horns, a ruff fanning back from each cheek, and
		// a petal over each eye
		const float LeafScale = DragonPartner->PetalSize / 15.0f;
		auto LayLeaf = [&](const FVector& Foot, const FVector& Up, const FVector& Face, float Length)
		{
			UStaticMeshComponent* Leaf = MakePetal();
			Leaf->SetRelativeLocationAndRotation(Foot, FRotationMatrix::MakeFromZX(Up.GetSafeNormal(), Face).Rotator());
			Leaf->SetRelativeScale3D(FVector(Length / 10.0f * LeafScale));
			HeadLeaves.Add(Leaf);
		};
		// Crown: a fan over the back of the skull, the middle petals longest, the outer ones leaning out to the sides
		constexpr int32 CrownLeaves = 9;
		for (int32 Index = 0; Index < CrownLeaves; ++Index)
		{
			const float Fan = FMath::Lerp(-1.15f, 1.15f, Index / float(CrownLeaves - 1));
			const FVector Up(-0.8f, FMath::Sin(Fan) * 0.9f, FMath::Cos(Fan) * 0.95f);
			const FVector FanWay(0.0f, FMath::Cos(Fan), -FMath::Sin(Fan));
			const FVector Face = (FanWay * 0.7f + FVector::CrossProduct(Up.GetSafeNormal(), FanWay) * 0.3f).GetSafeNormal();
			LayLeaf(FVector(-14.0f, FMath::Sin(Fan) * 10.0f, 20.0f + FMath::Cos(Fan) * 4.0f), Up, Face, 54.0f - 14.0f * FMath::Abs(Fan) / 1.15f);
		}
		constexpr int32 RuffLeaves = 5;
		for (int32 Side = 0; Side < 2; ++Side)
		{
			const float SideSign = Side == 0 ? 1.0f : -1.0f;
			// Temple petals sweeping back like horns
			LayLeaf(FVector(4.0f, SideSign * 16.0f, 20.0f), FVector(-1.0f, SideSign * 0.45f, 0.45f), FVector(0.0f, SideSign, 0.35f), 48.0f);
			for (int32 Index = 0; Index < RuffLeaves; ++Index)
			{
				const float Fan = FMath::Lerp(-0.9f, 0.9f, Index / float(RuffLeaves - 1));
				const FVector Out(0.0f, SideSign * FMath::Cos(Fan), FMath::Sin(Fan));
				LayLeaf(FVector(-8.0f, SideSign * 18.0f, 0.0f) + Out * 6.0f, FVector(-1.0f, 0.0f, 0.0f) + Out * 0.9f, Out, 40.0f - 8.0f * FMath::Abs(Fan));
			}
			LayLeaf(FVector(36.0f, SideSign * 14.0f, 18.0f), FVector(-0.8f, SideSign * 0.4f, 0.6f), FVector(0.0f, SideSign, 0.3f), 20.0f);
		}
	}

	// The flood dragon: a frill of its own Taublatt petals round the back of its head (placed each frame, it folds and fans), and
	// in its jaws a bulb of five petals shut round a heart with an opening at the front, out of which the jet shoots (like the
	// gland of the steam dragon, Boris 2026-11-19)
	if (bFloodDragon)
	{
		if (UStaticMesh* FrillMesh = VaelAssets::LoadOptional(Plant->PetalMesh))
		{
			for (int32 Index = 0; Index < 16; ++Index)
			{
				UStaticMeshComponent* Petal = NewObject<UStaticMeshComponent>(this);
				Petal->SetMobility(EComponentMobility::Movable);
				Petal->SetStaticMesh(FrillMesh);
				Petal->SetMaterial(0, PieceMaterial);
				Petal->SetCollisionEnabled(ECollisionEnabled::NoCollision);
				Petal->SetCanEverAffectNavigation(false);
				Petal->SetupAttachment(HeadFrame);
				Petal->RegisterComponent();
				// The first eleven are the frill, the last five the bulb
				if (Index < 11)
				{
					FrillPetals.Add(Petal);
				}
				else
				{
					GlandPetals.Add(Petal);
				}
			}
		}
	}

	// The thunder dragon: blades of the sunk Windhalm standing out of the cheeks like gills, long ones sweeping back from the
	// skull like horns, and a ring of short ones round the glowing core in its throat, pointing out of the jaws
	if (bThunderDragon && DragonPartner != nullptr)
	{
		UMaterialInterface* BladeMaterial = PartnerMaterial != nullptr ? PartnerMaterial.Get() : PieceMaterial;
		auto LayBlade = [&](const FVector& Foot, const FVector& Way, float Length, float Width, int32 MeshPick, bool bTip)
		{
			USplineMeshComponent* Blade = MakePiece(TowerFirstMesh({ &DragonPartner->FingerMeshes, &DragonPartner->ArmMeshes, &DragonPartner->VineMeshes }, MeshPick), HeadFrame);
			Blade->SetMaterial(0, BladeMaterial);
			const FVector Dir = Way.GetSafeNormal();
			// Each blade curves back a little towards its tip, like grass bent by wind
			const FVector Tip = Foot + Dir * Length + FVector(-0.25f, 0.0f, -0.1f) * Length * 0.3f;
			LayPiece(Blade, Foot, Dir * Length, Tip, (Dir + FVector(-0.5f, 0.0f, -0.15f)).GetSafeNormal() * Length, Width, Width * 0.12f, Random.FRandRange(0.0f, 6.0f));
			if (bTip)
			{
				BladeTips.Add(Tip);
			}
		};
		int32 BladePick = 0;
		for (int32 Side = 0; Side < 2; ++Side)
		{
			const float SideSign = Side == 0 ? 1.0f : -1.0f;
			// Gills: five blades fanning back and out of each cheek, the lower ones shorter
			for (int32 Index = 0; Index < 5; ++Index)
			{
				const float Along = Index / 4.0f;
				LayBlade(FVector(FMath::Lerp(18.0f, -12.0f, Along), SideSign * 17.0f, FMath::Lerp(-4.0f, 14.0f, Along)),
					FVector(-1.0f, SideSign * 0.65f, 0.15f + 0.35f * Along), FMath::Lerp(26.0f, 40.0f, Along), 0.9f, BladePick++, true);
			}
			// Horns: two long blades sweeping back from the skull
			for (int32 Index = 0; Index < 2; ++Index)
			{
				LayBlade(FVector(-6.0f - Index * 8.0f, SideSign * (7.0f + Index * 6.0f), 24.0f), FVector(-1.0f, SideSign * (0.18f + 0.15f * Index), 0.75f - 0.2f * Index),
					Index == 0 ? 66.0f : 52.0f, 1.3f, BladePick++, true);
			}
		}
		// The ring in the throat: six short blades round the core, pointing out of the jaws
		for (int32 Index = 0; Index < 6; ++Index)
		{
			const float Around = Index * UE_PI / 3.0f;
			const FVector Out(0.0f, FMath::Cos(Around), FMath::Sin(Around));
			LayBlade(FVector(16.0f, 0.0f, -12.0f) + Out * 6.0f, FVector(1.0f, 0.0f, 0.0f) + Out * 0.35f, 20.0f, 0.6f, BladePick++, false);
		}
	}

	// The sun dragon: a burning glass (the warm glass of the Glimmklee dome) held in its jaws, framed by a ring of glowing clover
	// leaves; more clover grows out of its brows and stands as a small crown on its skull
	if (bSunDragon && DragonPartner != nullptr)
	{
		UStaticMesh* Sphere = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Sphere.Sphere"));
		UMaterialInterface* Glass = VaelAssets::LoadOptional(UVaelFloromantSettings::Get()->GlassMaterial);
		if (Sphere != nullptr)
		{
			LensGlass = NewObject<UStaticMeshComponent>(this);
			LensGlass->SetMobility(EComponentMobility::Movable);
			LensGlass->SetStaticMesh(Sphere);
			LensGlass->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			LensGlass->SetCanEverAffectNavigation(false);
			LensGlass->SetCastShadow(false);
			LensGlass->SetupAttachment(HeadFrame);
			LensGlass->RegisterComponent();
			if (Glass != nullptr)
			{
				LensMaterial = UMaterialInstanceDynamic::Create(Glass, this);
				LensMaterial->SetVectorParameterValue(TEXT("Color"), FLinearColor(1.0f, 0.5f, 0.15f));
				LensGlass->SetMaterial(0, LensMaterial);
			}
		}
		UStaticMesh* Clover = VaelAssets::LoadOptional(DragonPartner->TipMesh);
		UMaterialInterface* CloverMaterial = PartnerMaterial != nullptr ? PartnerMaterial.Get() : PieceMaterial;
		const float CloverScale = FMath::Max(DragonPartner->TipSize * 1.6f / 10.0f, 0.01f);
		auto MakeClover = [this, Clover, CloverMaterial]()
		{
			UStaticMeshComponent* Leaf = NewObject<UStaticMeshComponent>(this);
			Leaf->SetMobility(EComponentMobility::Movable);
			Leaf->SetStaticMesh(Clover);
			Leaf->SetMaterial(0, CloverMaterial);
			Leaf->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			Leaf->SetCanEverAffectNavigation(false);
			Leaf->SetupAttachment(HeadFrame);
			Leaf->RegisterComponent();
			return Leaf;
		};
		if (Clover != nullptr)
		{
			for (int32 Index = 0; Index < 8; ++Index)
			{
				LensLeaves.Add(MakeClover());
			}
			auto SetClover = [&](const FVector& Place, const FVector& Out, float Scale)
			{
				UStaticMeshComponent* Leaf = MakeClover();
				Leaf->SetRelativeLocationAndRotation(Place, FRotationMatrix::MakeFromZ(Out.GetSafeNormal()).Rotator());
				Leaf->SetRelativeScale3D(FVector(CloverScale * Scale));
				HeadLeaves.Add(Leaf);
			};
			for (int32 Side = 0; Side < 2; ++Side)
			{
				const float SideSign = Side == 0 ? 1.0f : -1.0f;
				SetClover(FVector(38.0f, SideSign * 15.0f, 19.0f), FVector(-0.4f, SideSign * 0.6f, 0.7f), 1.1f);
				SetClover(FVector(28.0f, SideSign * 17.0f, 21.0f), FVector(-0.6f, SideSign * 0.7f, 0.5f), 0.9f);
			}
			for (int32 Index = 0; Index < 5; ++Index)
			{
				const float Fan = FMath::Lerp(-0.8f, 0.8f, Index / 4.0f);
				SetClover(FVector(-6.0f, FMath::Sin(Fan) * 10.0f, 26.0f), FVector(-0.5f, FMath::Sin(Fan), FMath::Cos(Fan) * 1.2f), 1.5f - 0.3f * FMath::Abs(Fan));
			}
		}

		// The pieces of its own thin sun beam: core and halo are stretched cylinders of glow, the end and the motes small balls
		UMaterialInterface* BeamCore = nullptr;
		UMaterialInterface* BeamGlow = nullptr;
		VaelEffects::LoadLookMaterials(BeamCore, BeamGlow);
		UStaticMesh* Cylinder = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
		if (BeamGlow != nullptr)
		{
			auto MakeGlow = [this, BeamGlow](const FLinearColor& Color, UStaticMesh* Mesh)
			{
				UStaticMeshComponent* Shape = VaelEffects::AddLookShape(this, BeamGlow, Color, 0.0f);
				if (Mesh != nullptr)
				{
					Shape->SetStaticMesh(Mesh);
				}
				Shape->SetUsingAbsoluteLocation(true);
				Shape->SetUsingAbsoluteRotation(true);
				Shape->SetUsingAbsoluteScale(true);
				Shape->SetCastShadow(false);
				Shape->SetVisibility(false);
				return Shape;
			};
			SunCore = MakeGlow(FLinearColor(1.0f, 0.97f, 0.85f), Cylinder);
			SunHalo = MakeGlow(DragonSunLight, Cylinder);
			SunEnd = MakeGlow(DragonSunLight, nullptr);
			for (int32 Index = 0; Index < 6; ++Index)
			{
				SunMotes.Add(MakeGlow(FLinearColor(1.0f, 0.95f, 0.7f), nullptr));
			}
		}
	}

	// The flood dragon's water jet uses the pieces of the sun beam, in the colours of water
	if (bFloodDragon)
	{
		UMaterialInterface* BeamCore = nullptr;
		UMaterialInterface* BeamGlow = nullptr;
		VaelEffects::LoadLookMaterials(BeamCore, BeamGlow);
		UStaticMesh* Cylinder = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
		if (BeamGlow != nullptr)
		{
			auto MakeGlow = [this, BeamGlow](const FLinearColor& Color, UStaticMesh* Mesh)
			{
				UStaticMeshComponent* Shape = VaelEffects::AddLookShape(this, BeamGlow, Color, 0.0f);
				if (Mesh != nullptr)
				{
					Shape->SetStaticMesh(Mesh);
				}
				Shape->SetUsingAbsoluteLocation(true);
				Shape->SetUsingAbsoluteRotation(true);
				Shape->SetUsingAbsoluteScale(true);
				Shape->SetCastShadow(false);
				Shape->SetVisibility(false);
				return Shape;
			};
			SunCore = MakeGlow(FLinearColor(0.85f, 0.95f, 1.0f), Cylinder);
			SunHalo = MakeGlow(DragonFloodLight, Cylinder);
			SunEnd = MakeGlow(FLinearColor(0.7f, 0.9f, 1.0f), nullptr);
			for (int32 Index = 0; Index < 8; ++Index)
			{
				SunMotes.Add(MakeGlow(FLinearColor(0.8f, 0.95f, 1.0f), nullptr));
			}
		}
	}

	// The wings of the storm dragon (Boris 2026-11-19): built of the root strands of its neck: an arm of two strands to the
	// elbow, ribs of strands fanning from it, and Windhalm blades spanned between the ribs as the skin; the blades are the darts
	if (bWingDragon)
	{
		UStaticMesh* StrandMesh = TowerFirstMesh({ &Plant->VineMeshes, &Plant->ArmMeshes, &Plant->FingerMeshes });
		if (bWood && !WoodSettings->DragonStrandMeshes.IsEmpty())
		{
			if (UStaticMesh* WoodStrand = VaelAssets::LoadOptional(WoodSettings->DragonStrandMeshes.Last()))
			{
				StrandMesh = WoodStrand;
			}
		}
		for (int32 Index = 0; Index < 2 * (2 + DragonWingRibs); ++Index)
		{
			USplineMeshComponent* Bone = MakeStalk(StrandMesh);
			if (bWood)
			{
				Bone->SetMaterial(0, WoodMaterial);
			}
			Bone->SetVisibility(false);
			WingBones.Add(Bone);
		}
		for (int32 Index = 0; Index < 2 * DragonWingFeathers; ++Index)
		{
			USplineMeshComponent* Blade = MakeStalk(TowerFirstMesh({ &Plant->FingerMeshes, &Plant->ArmMeshes, &Plant->VineMeshes }, Index));
			Blade->SetVisibility(false);
			WingBlades.Add(Blade);
		}
		FeatherGrow.Init(1.0f, 2 * DragonWingFeathers);
		FeatherTips.Init(FVector::ZeroVector, 2 * DragonWingFeathers);
	}

	// Roots of the partner in its colour, winding up round the neck from the foot to the head (laid each frame)
	if (DragonPartner != nullptr && (bSteamDragon || bThunderDragon || bSunDragon))
	{
		UStaticMesh* RootMesh = TowerFirstMesh({ &DragonPartner->VineMeshes, &DragonPartner->ArmMeshes, &DragonPartner->FingerMeshes });
		for (int32 Index = 0; Index < DragonPartnerRoots * (DragonPartnerPieces + DragonPartnerSpurs); ++Index)
		{
			USplineMeshComponent* PartnerRoot = MakeStalk(RootMesh);
			if (PartnerMaterial != nullptr)
			{
				PartnerRoot->SetMaterial(0, PartnerMaterial);
			}
			PartnerRoot->SetVisibility(false);
			PartnerVines.Add(PartnerRoot);
		}
	}

	// The cards of the steam jet, in the material of the mage's steam; they live in world space
	if (bSteamDragon)
	{
		if (UMaterialInterface* CloudMaterial = VaelAssets::LoadOptional(UVaelMagicSettings::Get()->CloudCardMaterial))
		{
			JetCards = NewObject<UVaelCloudCardsComponent>(this);
			JetCards->SetupAttachment(RootComponent);
			JetCards->SetUsingAbsoluteLocation(true);
			JetCards->SetUsingAbsoluteRotation(true);
			JetCards->SetUsingAbsoluteScale(true);
			JetCards->RegisterComponent();
			JetCards->SetWorldLocationAndRotation(FVector::ZeroVector, FRotator::ZeroRotator);
			JetCards->SetWorldScale3D(FVector::OneVector);
			JetCards->Init(CloudMaterial);
			for (int32 Index = 0; Index < DragonJetCards; ++Index)
			{
				JetCards->AddCard(FVector::ZeroVector, 1.0f, 0.0f);
			}
			JetCardPlaces.Init(FVector::ZeroVector, DragonJetCards);
			JetCardVelocities.Init(FVector::ZeroVector, DragonJetCards);
			JetCardAges.Init(-1.0f, DragonJetCards);
		}
	}

	// Shaggy root tufts along the lifted neck (thin roots of the root wood)
	UStaticMesh* FibreMesh = bWood ? VaelAssets::LoadOptional(WoodSettings->DragonFibreMesh) : nullptr;
	bWoodTufts = FibreMesh != nullptr;
	for (int32 Index = 0; Index < DragonNeckTufts; ++Index)
	{
		USplineMeshComponent* Tuft = MakeStalk(FibreMesh != nullptr ? FibreMesh : TowerFirstMesh({ &Plant->FingerMeshes, &Plant->ArmMeshes }, Index));
		if (FibreMesh != nullptr)
		{
			Tuft->SetMaterial(0, WoodMaterial);
		}
		Tuft->SetVisibility(false);
		DragonManeStalks.Add(Tuft);
	}

	UMaterialInterface* Core = nullptr;
	UMaterialInterface* GlowMaterial = nullptr;
	VaelEffects::LoadLookMaterials(Core, GlowMaterial);
	if (GlowMaterial != nullptr)
	{
		for (int32 Index = 0; Index < 3 + DragonSurgeOrbs; ++Index)
		{
			// The steam dragon looks out of turquoise eyes, and its sparks run up in the colour of the Taublatt; the throat glows
			const bool bTurquoise = (bSteamDragon || bThunderDragon || bSunDragon || bWingDragon || bFloodDragon) && Index != 2;
			UStaticMeshComponent* Orb = VaelEffects::AddLookShape(this, GlowMaterial, bTurquoise ? (bThunderDragon ? DragonThunderLight : bSunDragon ? DragonSunLight : bWingDragon ? DragonWindLight : bFloodDragon ? DragonFloodLight : DragonSteamEyes) : VaelSymbiosis::GetColor(EVaelSymbiosis::Glutbusch), 0.0f);
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
	if (HeadFrame == nullptr || JawFrame == nullptr)
	{
		return;
	}
	const float Before = DragonGrow;
	DragonGrow = bWithering ? FMath::Min(DragonGrow, FMath::Max(Grow, 0.0f)) : FMath::Min(1.0f, DragonGrow + DeltaSeconds / 1.6f);
	// The root-wood dragon comes out of the earth at its full size (see EmergeTime); it only shrinks again when it withers
	const bool bFromEarth = !SpinePieces.IsEmpty();
	const float Appear = bFromEarth && !bWithering ? 1.0f : FMath::SmoothStep(0.45f, 1.0f, DragonGrow);
	const float Time = GetWorld()->GetTimeSeconds();

	// The moment the head breaks out of the crown: a burst of embers
	if (Before < 0.5f && DragonGrow >= 0.5f && !bFromEarth)
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
		Burst.Glow = 5.0f;
		AVaelDebrisBurst::Spawn(GetGardener(), GetCrown(), Burst);
	}

	// The old crown is gone once the head has broken out
	for (USplineMeshComponent* Stalk : CrownStalks)
	{
		Stalk->SetVisibility(DragonGrow < 0.5f && !bFromEarth);
	}
	for (UStaticMeshComponent* Piece : CrownPieces)
	{
		Piece->SetVisibility(DragonGrow < 0.5f && !bFromEarth);
	}

	// The jaw: shut and breathing at rest, ajar while it charges, torn wide open through the drumfire
	const bool bCharging = Target.IsValid() && !bWithering && FireCountdown < 0.7f;
	// Biting it roars open while it draws back, tears wide open as it lunges and snaps shut on the prey
	float JawGoal = IsBreathing() ? (bSteamDragon ? 0.7f : bThunderDragon ? 0.85f : bSunDragon ? 0.6f : bFloodDragon ? 0.8f : 1.0f) : (bCharging ? 0.35f : 0.06f + 0.04f * FMath::Sin(Time * 1.7f));
	float JawSpeed = JawGoal > Jaw ? 10.0f : 5.0f;
	// Towering up out of the earth it roars with its jaws wide open
	if (EmergeTime >= 1.3f && EmergeTime < 2.4f)
	{
		JawGoal = 1.0f;
		JawSpeed = 8.0f;
	}
	if (BiteTime >= 0.0f)
	{
		JawGoal = BiteTime < 0.22f ? 0.75f : (BiteTime < 0.34f ? 1.0f : 0.0f);
		JawSpeed = BiteTime < 0.34f ? 14.0f : 34.0f;
	}
	Jaw = FMath::FInterpTo(Jaw, JawGoal, DeltaSeconds, JawSpeed);
	DrumAngle += DeltaSeconds * (BurstLeft > 0 ? 10.0f : 1.2f);

	// The head sits on the crown of the snake, sized with the tower
	FVector Forward = GetCrownAxis().GetSafeNormal2D();
	if (Forward.IsNearlyZero())
	{
		Forward = FVector::ForwardVector;
	}
	const UVaelFloromantSettings* Settings = UVaelFloromantSettings::Get();
	const float Height = Settings->TowerHeight * FMath::Sin(FMath::Clamp(Grow, 0.0f, 1.0f) * UE_HALF_PI) * (1.0f + (Settings->TowerCallSizeScale - 1.0f) * Giant);
	const float Size = Height / 200.0f * DragonHeadScale * Appear;
	const bool bShown = Appear > 0.02f;
	if (HeadFrame->IsVisible() != bShown)
	{
		HeadFrame->SetVisibility(bShown, true);
	}
	// It looks along the facing of its neck, down when it sniffs or strikes
	const FVector Facing = GetCrownAxis().IsNearlyZero() ? Forward : GetCrownAxis();
	HeadFrame->SetWorldLocationAndRotation(GetCrown(), FRotationMatrix::MakeFromXZ(Facing, FVector::UpVector).Rotator());
	// The storm dragon has a slim, long head
	HeadFrame->SetWorldScale3D(bWingDragon ? FVector(FMath::Max(Size * 1.1f, 0.001f), FMath::Max(Size * 0.72f, 0.001f), FMath::Max(Size * 0.8f, 0.001f)) : FVector(FMath::Max(Size, 0.001f)));
	// A slow snarl of the head, and the jaw on its hinge
	JawFrame->SetRelativeRotation(FRotator(-DragonJawDegrees * Jaw, 0.0f, 0.0f));
	const FTransform Head = HeadFrame->GetComponentTransform();
	// The veins of the root wood glow up as the jaws open
	if (WoodMaterial != nullptr)
	{
		WoodMaterial->SetScalarParameterValue(TEXT("Leuchten"), 0.5f + 3.0f * Jaw);
	}

	// The thistle ring in the throat, turning; the mouth is the middle between the jaws
	const FVector MouthLocal(28.0f, 0.0f, -9.0f - Jaw * 10.0f);
	const float HeadScale = FMath::Max(Plant->TipSize * 1.6f / 10.0f, 0.01f);
	for (int32 Index = 0; Index < DrumRegrow.Num(); ++Index)
	{
		DrumRegrow[Index] = FMath::Min(1.0f, DrumRegrow[Index] + DeltaSeconds / 0.9f);
		UStaticMeshComponent* Thistle = DragonHeads.IsValidIndex(Index) ? DragonHeads[Index].Get() : nullptr;
		if (Thistle == nullptr)
		{
			continue;
		}
		// The steam dragon keeps only one glowing thistle, in the heart of its gland
		if (bSteamDragon || bThunderDragon || bSunDragon || bWingDragon || bFloodDragon)
		{
			Thistle->SetVisibility(Index == 0 && HeadFrame->IsVisible());
			if (Index == 0)
			{
				const float Glow = 1.0f + 0.25f * Squeeze;
				Thistle->SetRelativeLocationAndRotation(MouthLocal - FVector(2.0f, 0.0f, 0.0f), FRotator(-90.0f, DrumAngle * 20.0f, 0.0f));
				Thistle->SetRelativeScale3D(FVector(HeadScale * 0.9f * Glow));
			}
			continue;
		}
		const float Angle = DrumAngle + Index * 2.0f * UE_PI / DragonDrumHeads;
		Thistle->SetRelativeLocationAndRotation(MouthLocal + FVector(0.0f, FMath::Cos(Angle) * 8.0f, FMath::Sin(Angle) * 8.0f), FRotator(-90.0f, 0.0f, 0.0f));
		Thistle->SetRelativeScale3D(FVector(FMath::Max(HeadScale * 0.7f * FMath::SmoothStep(0.0f, 1.0f, DrumRegrow[Index]), 0.001f)));
	}

	// The burning glass: a flat lens before the glowing thistle, framed by clover leaves; light gathers in it while the dragon
	// charges (it glows hotter and the leaves turn to it), and it burns white while the beam runs
	if (LensGlass != nullptr)
	{
		const bool bGathering = Target.IsValid() && !bWithering && FireCountdown < 1.0f && !IsBreathing();
		Squeeze = FMath::FInterpTo(Squeeze, bGathering ? 1.0f : (SunLeft > 0.0f ? 1.0f : 0.0f), DeltaSeconds, 4.0f);
		const FVector LensAt = MouthLocal + FVector(10.0f, 0.0f, 0.0f);
		LensGlass->SetRelativeLocationAndRotation(LensAt, FRotator::ZeroRotator);
		LensGlass->SetRelativeScale3D(FVector(0.035f, 0.11f, 0.11f) * (1.0f + 0.1f * Squeeze));
		if (LensMaterial != nullptr)
		{
			LensMaterial->SetScalarParameterValue(TEXT("Glow"), 0.3f + 1.0f * Squeeze + (SunLeft > 0.0f ? 1.2f : 0.0f));
		}
		const float CloverScale = DragonPartner != nullptr ? FMath::Max(DragonPartner->TipSize * 1.6f / 10.0f, 0.01f) : 1.0f;
		for (int32 Index = 0; Index < LensLeaves.Num(); ++Index)
		{
			const float Around = Index * 2.0f * UE_PI / LensLeaves.Num() + 0.2f * FMath::Sin(Time * 0.7f);
			const FVector Out(0.0f, FMath::Cos(Around), FMath::Sin(Around));
			// Spread round the rim, tipping forward to the light while it gathers
			const FVector Way = (Out + FVector(0.3f + 0.6f * Squeeze, 0.0f, 0.0f)).GetSafeNormal();
			LensLeaves[Index]->SetRelativeLocationAndRotation(LensAt + Out * 6.0f, FRotationMatrix::MakeFromZ(Way).Rotator());
			LensLeaves[Index]->SetRelativeScale3D(FVector(CloverScale * 0.5f));
		}
		SunMouth = Head.TransformPosition(LensAt + FVector(4.0f, 0.0f, 0.0f));
	}

	// The frill of the flood dragon: petals in a half ring round the back of the head; folded back flat against the neck at
	// rest, fanned wide open and upright while it charges and gushes, trembling a little
	if (!FrillPetals.IsEmpty())
	{
		const bool bFlaring = IsBreathing() || (Target.IsValid() && !bWithering && FireCountdown < 0.9f);
		FrillOpen = FMath::FInterpTo(FrillOpen, bFlaring ? 1.0f : 0.0f, DeltaSeconds, bFlaring ? 9.0f : 3.0f);
		const float PetalScale = FMath::Max(Plant->PetalSize * 2.6f / 10.0f, 0.01f);
		for (int32 Index = 0; Index < FrillPetals.Num(); ++Index)
		{
			// From one jaw hinge over the skull to the other
			const float Around = FMath::Lerp(-1.45f, 1.45f, Index / float(FrillPetals.Num() - 1));
			const FVector Out(0.0f, FMath::Sin(Around), FMath::Cos(Around));
			const float Lean = FMath::Lerp(1.25f, 0.15f, FrillOpen) + 0.04f * FMath::Sin(Time * 9.0f + Index) * FrillOpen;
			const FVector Up = (Out * FMath::Cos(Lean) - FVector::ForwardVector * FMath::Sin(Lean)).GetSafeNormal();
			const FVector Face = FVector::ForwardVector;
			FrillPetals[Index]->SetRelativeLocationAndRotation(FVector(-12.0f, 0.0f, 6.0f) + Out * 16.0f, FRotationMatrix::MakeFromZX(Up, Face).Rotator());
			FrillPetals[Index]->SetRelativeScale3D(FVector(PetalScale * (0.75f + 0.35f * FrillOpen) * (1.0f - 0.25f * FMath::Abs(Around) / 1.45f)));
		}
	}

	// The steam gland: four Taublatt petals grow forward from the back of the throat round the glowing thistle, their inner
	// sides to it like a shut rose. While the dragon charges they press tight round it; through the jet their tips spread a
	// little and the steam streams out between them
	if (!GlandPetals.IsEmpty())
	{
		// The bulb of the flood dragon stays pressed tight through its jet, the pressure behind the opening
		const bool bGlandCharging = (Target.IsValid() && !bWithering && FireCountdown < 0.9f && !IsBreathing()) || (bFloodDragon && SunLeft > 0.0f);
		Squeeze = FMath::FInterpTo(Squeeze, bGlandCharging ? 1.0f : 0.0f, DeltaSeconds, bGlandCharging ? 4.0f : 8.0f);
		const float Spread = JetLeft > 0.0f ? 1.0f : 0.0f;
		const float PetalScale = FMath::Max(26.0f / 10.0f * (DragonPartner != nullptr ? DragonPartner->PetalSize / 15.0f : 1.0f), 0.01f);
		const FVector Heart = MouthLocal - FVector(2.0f, 0.0f, 0.0f);
		for (int32 Index = 0; Index < GlandPetals.Num(); ++Index)
		{
			const float Around = Index * 2.0f * UE_PI / GlandPetals.Num() + UE_PI * 0.25f + 0.08f * FMath::Sin(Time * 2.0f + Index);
			const FVector Out(0.0f, FMath::Cos(Around), FMath::Sin(Around));
			// Lean of the petal out from the axis of the mouth: shut at rest, pressed while charging, spread through the jet
			const float Lean = FMath::DegreesToRadians(FMath::Lerp(14.0f, 2.0f, Squeeze) + 26.0f * Spread + 3.0f * FMath::Sin(Time * 3.1f + Index * 1.7f));
			const FVector Up = (FVector::ForwardVector * FMath::Cos(Lean) + Out * FMath::Sin(Lean)).GetSafeNormal();
			const FVector Outside = (Out - Up * FVector::DotProduct(Out, Up)).GetSafeNormal();
			const FVector Foot = Heart - FVector(12.0f, 0.0f, 0.0f) + Out * FMath::Lerp(7.0f, 5.0f, Squeeze);
			GlandPetals[Index]->SetRelativeLocationAndRotation(Foot, FRotationMatrix::MakeFromXZ(Outside, Up).Rotator());
			GlandPetals[Index]->SetRelativeScale3D(FVector(PetalScale * (1.0f - 0.08f * Squeeze)));
		}
	}

	// Root tufts and a few thistles along the lifted neck, bristling out of the snake
	const FVector Side = FVector::CrossProduct(FVector::UpVector, Forward);
	const float NeckSize = Height / 200.0f * Appear;
	const FTransform& Transform = GetActorTransform();
	for (int32 Index = 0; Index < DragonManeStalks.Num(); ++Index)
	{
		USplineMeshComponent* Tuft = DragonManeStalks[Index];
		Tuft->SetVisibility(bShown);
		const int32 ThistleIndex = DragonDrumHeads + DragonSkullThistles + Index / 3;
		if (!bShown)
		{
			if (Index % 3 == 1 && DragonHeads.IsValidIndex(ThistleIndex))
			{
				DragonHeads[ThistleIndex]->SetVisibility(false);
			}
			continue;
		}
		const float SideSign = Index % 2 == 0 ? 1.0f : -1.0f;
		const float Along = 2.0f + 0.95f * Index / FMath::Max(DragonNeckTufts - 1, 1);
		const FVector Root = GetHookCurve(Along);
		const float Sway = FMath::Sin(Time * 1.6f + Index * 0.7f) * 0.1f;
		const FVector Way = (FVector::UpVector * (0.6f + 0.3f * FMath::Frac(Index * 0.37f)) + Side * SideSign * (0.7f + Sway) - Forward * 0.4f).GetSafeNormal();
		const float Length = (16.0f + 10.0f * FMath::Frac(Index * 0.61f)) * NeckSize;
		const FVector Tip = Root + Way * Length;
		Tuft->SetStartAndEnd(Transform.InverseTransformPosition(Root), Transform.InverseTransformVectorNoScale(Way * Length),
			Transform.InverseTransformPosition(Tip), Transform.InverseTransformVectorNoScale((Way - FVector::UpVector * 0.5f).GetSafeNormal() * Length), false);
		// The thin root of the root wood is a few times thinner than the stalks of the plants
		const float TuftThick = bWoodTufts ? 3.0f : 1.0f;
		Tuft->SetStartScale(FVector2D(0.6f * NeckSize * TuftThick), false);
		Tuft->SetEndScale(FVector2D(0.06f * NeckSize * TuftThick), false);
		Tuft->UpdateMesh();

		if (Index % 3 == 1 && DragonHeads.IsValidIndex(ThistleIndex))
		{
			UStaticMeshComponent* Thistle = DragonHeads[ThistleIndex];
			Thistle->SetVisibility(true);
			Thistle->SetWorldLocationAndRotation(Root + Way * 4.0f * NeckSize, FRotationMatrix::MakeFromZ(Way).Rotator());
			Thistle->SetWorldScale3D(FVector(FMath::Max(HeadScale * 0.6f * NeckSize, 0.001f)));
		}
	}

	// The partner's roots wind up through the neck in their own colour, from the foot to the head, growing in with the head:
	// thin, crossing into a net, weaving in and out of the snake, with side shoots branching off (Boris 2026-11-19)
	if (!PartnerVines.IsEmpty())
	{
		const float Thick = Height / 200.0f;
		auto Frame = [this](float T, FVector& OutSide, FVector& OutUp)
		{
			const FVector Tangent = (GetHookCurve(FMath::Min(T + 0.02f, 3.0f)) - GetHookCurve(FMath::Max(T - 0.02f, 0.0f))).GetSafeNormal();
			OutSide = FVector::CrossProduct(Tangent, FVector::UpVector).GetSafeNormal();
			if (OutSide.IsNearlyZero())
			{
				OutSide = FVector::RightVector;
			}
			OutUp = FVector::CrossProduct(OutSide, Tangent).GetSafeNormal();
		};
		auto RootPoint = [&](int32 Root, float T)
		{
			FVector Side, Up;
			Frame(T, Side, Up);
			// Every other root winds the other way round, so they cross into a net; each weaves into the snake and out again
			const float Turn = Root % 2 == 0 ? 3.4f : -2.8f;
			const float Angle = Root * 2.0f * UE_PI / DragonPartnerRoots + T * Turn + Time * 0.25f * (Root % 2 == 0 ? 1.0f : -1.0f);
			const float Round = (10.0f + 5.0f * FMath::Sin(T * 6.5f + Root * 1.9f) - 2.0f * T / 3.0f) * Thick;
			return GetHookCurve(T) + (Side * FMath::Cos(Angle) + Up * FMath::Sin(Angle)) * Round;
		};
		const float Reach = 3.0f * FMath::SmoothStep(0.0f, 1.0f, Appear);

		// Short side shoots branching off each root, reaching out a little along the neck and bending back into it
		for (int32 Root = 0; Root < DragonPartnerRoots; ++Root)
		{
			for (int32 Spur = 0; Spur < DragonPartnerSpurs; ++Spur)
			{
				const int32 Slot = DragonPartnerRoots * DragonPartnerPieces + Root * DragonPartnerSpurs + Spur;
				if (!PartnerVines.IsValidIndex(Slot))
				{
					continue;
				}
				USplineMeshComponent* Shoot = PartnerVines[Slot];
				const float T = 0.4f + (Spur + 0.3f * Root / float(DragonPartnerRoots)) * 2.4f / DragonPartnerSpurs;
				const bool bShow = bShown && T < Reach;
				Shoot->SetVisibility(bShow);
				if (!bShow)
				{
					continue;
				}
				const FVector Start = RootPoint(Root, T);
				const FVector Middle = GetHookCurve(T);
				const FVector Out = (Start - Middle).GetSafeNormal();
				const FVector Along = (GetHookCurve(FMath::Min(T + 0.05f, 3.0f)) - GetHookCurve(FMath::Max(T - 0.05f, 0.0f))).GetSafeNormal();
				const float Length = (10.0f + 5.0f * ((Root + Spur) % 3)) * Thick;
				const FVector End = Start + (Out * 0.7f + Along * (Spur % 2 == 0 ? 0.7f : -0.7f)).GetSafeNormal() * Length;
				Shoot->SetStartAndEnd(Transform.InverseTransformPosition(Start), Transform.InverseTransformVectorNoScale((Out + Along * 0.3f) * Length),
					Transform.InverseTransformPosition(End), Transform.InverseTransformVectorNoScale((Along * (Spur % 2 == 0 ? 1.0f : -1.0f) - Out * 0.6f) * Length), false);
				Shoot->SetStartScale(FVector2D(0.3f * Thick), false);
				Shoot->SetEndScale(FVector2D(0.06f * Thick), false);
				Shoot->UpdateMesh();
			}
		}

		for (int32 Root = 0; Root < DragonPartnerRoots; ++Root)
		{
			for (int32 Piece = 0; Piece < DragonPartnerPieces; ++Piece)
			{
				USplineMeshComponent* Vine = PartnerVines[Root * DragonPartnerPieces + Piece];
				const float T0 = Reach * Piece / DragonPartnerPieces;
				const float T1 = Reach * (Piece + 1) / DragonPartnerPieces;
				const bool bShow = bShown && T1 > 0.05f;
				Vine->SetVisibility(bShow);
				if (!bShow)
				{
					continue;
				}
				const FVector P0 = RootPoint(Root, T0);
				const FVector P1 = RootPoint(Root, T1);
				const FVector S0 = (RootPoint(Root, FMath::Min(T0 + 0.05f, 3.0f)) - RootPoint(Root, FMath::Max(T0 - 0.05f, 0.0f))) / 0.1f * (T1 - T0);
				const FVector S1 = (RootPoint(Root, FMath::Min(T1 + 0.05f, 3.0f)) - RootPoint(Root, FMath::Max(T1 - 0.05f, 0.0f))) / 0.1f * (T1 - T0);
				Vine->SetStartAndEnd(Transform.InverseTransformPosition(P0), Transform.InverseTransformVectorNoScale(S0),
					Transform.InverseTransformPosition(P1), Transform.InverseTransformVectorNoScale(S1), false);
				// Thin roots, finer up towards the head
				Vine->SetStartScale(FVector2D(FMath::Lerp(0.55f, 0.28f, T0 / 3.0f) * Thick), false);
				Vine->SetEndScale(FVector2D(FMath::Lerp(0.55f, 0.28f, T1 / 3.0f) * Thick), false);
				Vine->SetStartRoll(Root * 2.0f + Piece * 1.3f, false);
				Vine->SetEndRoll(Root * 2.0f + Piece * 1.3f + 1.3f, false);
				Vine->UpdateMesh();
			}
		}
	}

	// Eyes sunk under the heavy brow, and the glow in the throat
	if (DragonGlows.Num() >= 3)
	{
		for (int32 Eye = 0; Eye < 2; ++Eye)
		{
			UStaticMeshComponent* Orb = DragonGlows[Eye];
			Orb->SetVisibility(bShown);
			Orb->SetWorldLocation(Head.TransformPosition(FVector(38.0f, Eye == 0 ? 13.0f : -13.0f, 13.0f)));
			Orb->SetWorldScale3D(FVector(FMath::Max(6.0f * Size / 100.0f, 0.001f)));
			VaelEffects::SetLookGlow(Orb, 3.0f + 4.0f * Jaw);
		}
		UStaticMeshComponent* Throat = DragonGlows[2];
		const float Heat = FMath::Clamp((Jaw - 0.1f) / 0.6f, 0.0f, 1.0f);
		Throat->SetVisibility(bShown && Heat > 0.03f);
		Throat->SetWorldLocation(Head.TransformPosition(MouthLocal - FVector(6.0f, 0.0f, 0.0f)));
		Throat->SetWorldScale3D(FVector(FMath::Max(16.0f * Size * Heat / 100.0f, 0.001f)));
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

	// The drumfire: when the jaws tear open earth sprays out, then one thistle after the other round the ring, flat and
	// straight at the target
	AVaelCharacter* Gardener = GetGardener();
	if (bWithering)
	{
		BurstLeft = 0;
	}
	else if (BurstLeft > 0 && Gardener != nullptr)
	{
		if (!bSprayed && Jaw > 0.55f)
		{
			bSprayed = true;
			FVaelDebris Earth;
			Earth.Count = 34;
			Earth.Spread = 14.0f;
			Earth.Speed = 320.0f;
			Earth.Lift = 120.0f;
			Earth.Lifetime = 0.9f;
			Earth.MinSize = 1.5f;
			Earth.MaxSize = 5.0f;
			Earth.Color = FLinearColor(0.12f, 0.09f, 0.06f);
			AVaelDebrisBurst::Spawn(Gardener, Head.TransformPosition(MouthLocal + FVector(30.0f, 0.0f, 0.0f)), Earth);
		}
		BurstTimer -= DeltaSeconds;
		if (Jaw > 0.6f && BurstTimer <= 0.0f)
		{
			BurstTimer = 0.075f;
			--BurstLeft;
			const int32 Index = NextDrum;
			NextDrum = (NextDrum + 1) % DragonDrumHeads;
			const float Angle = DrumAngle + Index * 2.0f * UE_PI / DragonDrumHeads;
			const FVector Muzzle = Head.TransformPosition(MouthLocal + FVector(16.0f, FMath::Cos(Angle) * 8.0f, FMath::Sin(Angle) * 8.0f));
			// The shots leave where the head looks (also down, it may hold its head high), so the fire sweeps with the neck
			const FVector Toward = FRotator(0.0f, FMath::FRandRange(-3.0f, 3.0f), 0.0f).RotateVector(GetCrownAxis().GetSafeNormal());
			VaelSymbiosis::FireShot(Gardener, Plant, Muzzle, Toward, BurstPower, bShadow, Settings->TowerShotScale * 0.85f, 0.6f);
			VaelSymbiosis::MuzzlePuff(this, Plant, Muzzle, bShadow, 0.8f);
			if (DrumRegrow.IsValidIndex(Index))
			{
				DrumRegrow[Index] = 0.0f;
			}
			// Every shot knocks the head back on its neck
			BendVelocity -= Forward * 0.2f;
			NeckHeadVelocity -= Forward * 230.0f * Size / DragonHeadScale - FVector::UpVector * 60.0f * Size / DragonHeadScale;
		}
	}

	// The steam jet: once the jaws are open a puff leaves the gland every tenth of a second along where the head looks, so the
	// steam sweeps with the neck over the fan
	if (bWithering)
	{
		JetLeft = 0.0f;
	}
	else if (JetLeft > 0.0f && Gardener != nullptr && Jaw > 0.45f)
	{
		JetLeft -= DeltaSeconds;
		JetTimer -= DeltaSeconds;
		if (JetTimer <= 0.0f)
		{
			JetTimer = 0.1f;
			PuffSteam(Head.TransformPosition(MouthLocal + FVector(16.0f, 0.0f, 0.0f)), Forward, Height);
			NeckHeadVelocity -= Forward * 70.0f * Size / DragonHeadScale;
		}
		JetLeft = FMath::Max(JetLeft, 0.0f);
	}

	// The sun beam (or the water jet of the flood dragon): once the jaws are open it is lit out of the mouth and follows the head
	// over the fan until its time is out
	if (bFloodDragon)
	{
		SunMouth = Head.TransformPosition(MouthLocal + FVector(12.0f, 0.0f, 0.0f));
		if (SunShown > 0.05f)
		{
			NeckHeadVelocity -= Forward * 40.0f * DeltaSeconds * 10.0f * Size / DragonHeadScale;
		}
	}
	if (bSunDragon || bFloodDragon)
	{
		if (bWithering)
		{
			SunLeft = 0.0f;
		}
		if (SunLeft > 0.0f)
		{
			SunLeft = FMath::Max(SunLeft - DeltaSeconds, 0.0f);
			if (!bSunLit && Jaw > 0.45f && Gardener != nullptr)
			{
				bSunLit = true;
				SunBurnTimer = 0.0f;
				SunFireTimer = 0.0f;
				VaelSymbiosis::MuzzlePuff(this, DragonPartner != nullptr ? DragonPartner.Get() : Plant.Get(), SunMouth, bShadow, 1.2f);
			}
		}
		else
		{
			bSunLit = false;
		}
		UpdateSunBeam(DeltaSeconds, Height);
	}

	// The wings of the storm dragon
	if (bWingDragon)
	{
		UpdateWings(DeltaSeconds, Height);
	}

	// The thunder: while it charges and breathes, arcs crackle between the tips of its blades; once the jaws are open lightning
	// strikes out of the throat, and a moment later a second, weaker strike follows
	if (bThunderDragon)
	{
		if (bWithering)
		{
			ThunderLeft = 0.0f;
		}
		const bool bCrackles = bShown && !bWithering && (ThunderLeft > 0.0f || (Target.IsValid() && FireCountdown < 0.9f));
		CrackleTimer -= DeltaSeconds;
		if (bCrackles && CrackleTimer <= 0.0f && BladeTips.Num() >= 2 && Gardener != nullptr)
		{
			CrackleTimer = FMath::FRandRange(0.06f, 0.14f);
			const int32 First = FMath::RandRange(0, BladeTips.Num() - 1);
			const int32 Second = (First + FMath::RandRange(1, BladeTips.Num() - 1)) % BladeTips.Num();
			AVaelLightningBolt::Spawn(this, Head.TransformPosition(BladeTips[First]), Head.TransformPosition(BladeTips[Second]), DragonThunderLight, 0.12f, 0.35f, false);
		}
		if (ThunderLeft > 0.0f && Gardener != nullptr)
		{
			ThunderLeft -= DeltaSeconds;
			ThunderTimer -= DeltaSeconds;
			if (ThunderStrikes > 0 && ThunderTimer <= 0.0f && Jaw > 0.5f)
			{
				StrikeThunder(Head.TransformPosition(MouthLocal + FVector(18.0f, 0.0f, 0.0f)), ThunderPower * (ThunderStrikes == 2 ? 1.0f : 0.55f), Height);
				--ThunderStrikes;
				ThunderTimer = 0.32f;
				NeckHeadVelocity -= Forward * 260.0f * Size / DragonHeadScale;
			}
			ThunderLeft = FMath::Max(ThunderLeft, 0.0f);
		}
	}

	if (JetCards != nullptr)
	{
		UpdateJetCards(DeltaSeconds, Head.TransformPosition(MouthLocal + FVector(14.0f, 0.0f, 0.0f)), Forward, Height);
	}

	// The bite: an enemy close to the foot is struck at; it draws back roaring, earth bursts at the foot, the head lunges out
	// on the stretched neck, the jaws snap shut on the prey (it burns and is thrown away), and the neck draws back
	BiteCooldown = FMath::Max(0.0f, BiteCooldown - DeltaSeconds);
	const FVector Base = GetActorLocation();
	const float Reach = Height * Settings->DragonBiteReach;
	if (BiteTime < 0.0f && BiteCooldown <= 0.0f && !IsBreathing() && Appear >= 0.99f && !bWithering && Gardener != nullptr)
	{
		if (AActor* Prey = VaelSymbiosis::FindTarget(Gardener, Base, Reach))
		{
			BiteTime = 0.0f;
			BiteTarget = Prey;
			BitePoint = Prey->GetActorLocation();
			bBitten = false;
		}
		else
		{
			// Nothing close: look again in a moment
			BiteCooldown = 0.2f;
		}
	}
	if (BiteTime >= 0.0f)
	{
		const float BiteBefore = BiteTime;
		BiteTime += DeltaSeconds;
		if (const AActor* Prey = BiteTarget.Get(); Prey != nullptr && !bBitten)
		{
			// Strike where it stands, as far as the neck reaches
			FVector Way = Prey->GetActorLocation() - Base;
			const float Flat = FMath::Min(Way.Size2D(), Reach);
			Way = Way.GetSafeNormal2D() * Flat;
			BitePoint = Base + Way + FVector::UpVector * FMath::Max(Prey->GetActorLocation().Z - Base.Z, Height * 0.2f);
		}
		if (BiteBefore < 0.22f && BiteTime >= 0.22f && Gardener != nullptr)
		{
			FVaelDebris Earth;
			Earth.Count = 26;
			Earth.Spread = 50.0f;
			Earth.Speed = 260.0f;
			Earth.Lift = 220.0f;
			Earth.Lifetime = 0.9f;
			Earth.MinSize = 2.0f;
			Earth.MaxSize = 6.0f;
			Earth.Color = FLinearColor(0.12f, 0.09f, 0.06f);
			AVaelDebrisBurst::Spawn(Gardener, Base + FVector(0.0f, 0.0f, 10.0f), Earth);
		}
		if (!bBitten && BiteTime >= 0.36f)
		{
			bBitten = true;
			AActor* Prey = BiteTarget.Get();
			const FVector Mouth = Head.TransformPosition(MouthLocal + FVector(20.0f, 0.0f, 0.0f));
			if (Gardener != nullptr && Prey != nullptr && FVector::Dist2D(Prey->GetActorLocation(), Base) <= Reach * 1.2f && UVaelCombatStatics::CanDamage(Gardener, Prey))
			{
				UVaelCombatStatics::DealDamage(Gardener, Prey, Settings->DragonBiteDamage, EVaelElement::Fire);
				UVaelCombatStatics::ApplyStatus(Gardener, Prey, EVaelStatus::Burning, Settings->DragonBiteBurnSeconds, 4.0f);
				if (AVaelCharacterBase* Bitten = Cast<AVaelCharacterBase>(Prey))
				{
					Bitten->ApplyKnockback((Prey->GetActorLocation() - Base).GetSafeNormal2D(), Settings->DragonBiteKnockback);
				}
				FVaelDebris Embers;
				Embers.Count = 20;
				Embers.Spread = 40.0f;
				Embers.Speed = 300.0f;
				Embers.Lift = 140.0f;
				Embers.Lifetime = 0.8f;
				Embers.MinSize = 2.0f;
				Embers.MaxSize = 6.0f;
				Embers.Color = VaelSymbiosis::GetColor(EVaelSymbiosis::Glutbusch);
				Embers.Glow = 5.0f;
				AVaelDebrisBurst::Spawn(Gardener, Mouth, Embers);
			}
			VaelSymbiosis::MuzzlePuff(this, Plant, Mouth, bShadow, 1.4f);
		}
		if (BiteTime >= 0.85f || bWithering)
		{
			BiteTime = -1.0f;
			BiteTarget = nullptr;
			BiteCooldown = Settings->DragonBiteInterval;
		}
	}
}

void AVaelPlantTower::UpdateDragonNeck(float DeltaSeconds, float Height)
{
	// A long frame must not blow the springs up
	const float Step = FMath::Min(DeltaSeconds, 0.04f);
	const UVaelFloromantSettings* Settings = UVaelFloromantSettings::Get();
	const FVector Base = GetActorLocation();
	const FVector Up = FVector::UpVector;
	const float Time = GetWorld()->GetTimeSeconds();
	const float Seed = GetUniqueID() * 0.37f;
	const float H = FMath::Max(Height, 1.0f);

	// Idle it looks around now and then and lowers its head to sniff; with a target it watches it
	const float Idle = 1.0f - AimBlend;
	const float LookYaw = Idle * (0.45f * FMath::Sin(Time * 0.31f + Seed) + 0.2f * FMath::Sin(Time * 0.83f + Seed * 1.3f));
	const float Sniff = Idle * FMath::SmoothStep(0.55f, 0.95f, FMath::Sin(Time * 0.23f + Seed * 2.1f));

	// Before the drumfire it draws back and rears up like a cobra
	const bool bCharging = Target.IsValid() && !bWithering && FireCountdown < 0.7f && !IsBreathing() && BiteTime < 0.0f;
	Coil = FMath::FInterpTo(Coil, bCharging ? 1.0f : 0.0f, Step, bCharging ? 5.0f : 12.0f);

	// Through the drumfire the head sweeps from one side of the fan to the other; charging it already turns to where it begins
	const float SweepWidth = FMath::DegreesToRadians(Settings->DragonSweepDegrees);
	if (IsBreathing())
	{
		BurstAge += Step;
		const float Along = FMath::Clamp((BurstAge - 0.15f) / GetBreathSeconds(), 0.0f, 1.0f);
		Sweep = SweepWidth * FMath::Sin((Along - 0.5f) * UE_PI);
	}
	else
	{
		BurstAge = 0.0f;
		Sweep = FMath::FInterpTo(Sweep, bCharging ? -SweepWidth : 0.0f, Step, 4.0f);
	}
	const float Burst = IsBreathing() ? 1.0f : 0.0f;

	// The head roams freely round its roots (Boris 2026-11-19): idle it wanders all round the foot, high and low, looking out the
	// way it is turned; with a target it keeps circling, rising and sinking on the target's side, always facing it. It holds
	// still while it breathes so the fire leaves straight, and moves slower while it charges
	RoamTime += Step * (IsBreathing() ? 0.0f : (bCharging ? 0.35f : 1.0f));
	const float Roam = RoamTime;
	const AActor* Goal = Target.Get();
	const FVector Forward = HookForward(LookDirection);
	const float Aim = AimBlend;
	const float Wander = FMath::Lerp(1.7f * FMath::Sin(Roam * 0.11f + Seed) + 0.8f * FMath::Sin(Roam * 0.27f + Seed * 2.0f),
		0.75f * FMath::Sin(Roam * 0.33f + Seed) + 0.25f * FMath::Sin(Roam * 0.71f + Seed * 1.4f), Aim);
	const FVector Out = Forward.RotateAngleAxis(FMath::RadiansToDegrees(Wander), Up);
	float Reach = H * (FMath::Lerp(0.55f, 0.5f, Aim) + 0.22f * FMath::Sin(Roam * 0.19f + Seed * 3.0f));
	float Rise = H * (FMath::Lerp(0.95f, 0.8f, Aim) + 0.3f * FMath::Sin(Roam * 0.23f + Seed * 1.1f) + 0.1f * FMath::Sin(Roam * 0.61f + Seed));
	// The storm dragon stands upright with its head held high over its roots (Boris 2026-11-19)
	if (bWingDragon)
	{
		Reach = H * (0.22f + 0.12f * FMath::Sin(Roam * 0.19f + Seed));
		Rise = H * (1.3f + 0.08f * FMath::Sin(Roam * 0.23f + Seed));
	}
	FVector HeadGoal = Base + Out * Reach + Up * Rise;
	// Sniffing it lowers its snout towards the ground ahead
	HeadGoal += Out * H * 0.15f * Sniff - Up * H * 0.5f * Sniff;
	// Charging it draws back over its roots and rears up like a cobra; breathing it pushes out
	FVector GoalWay = Out;
	if (Goal != nullptr)
	{
		const FVector ToGoal = (Goal->GetActorLocation() - Base).GetSafeNormal2D();
		if (!ToGoal.IsNearlyZero())
		{
			GoalWay = ToGoal;
		}
	}
	HeadGoal = FMath::Lerp(HeadGoal, Base - GoalWay * H * 0.1f + Out * H * 0.12f + Up * H * 1.2f, 0.6f * Coil);
	HeadGoal += GoalWay * H * 0.12f * Burst;
	const FVector HeadWay = Out;

	// Where it looks: idle out the way it roams, glancing about; aiming at the target (down too), sweeping the fan as it breathes
	float Pitch = -0.15f - 0.55f * Sniff;
	FVector FacingWay = Out.RotateAngleAxis(FMath::RadiansToDegrees(LookYaw), Up);
	FVector AimFacing = FVector::ZeroVector;
	if (Goal != nullptr && Aim > 0.05f)
	{
		AimFacing = (Goal->GetActorLocation() + Up * 40.0f - NeckHead).GetSafeNormal().RotateAngleAxis(FMath::RadiansToDegrees(Sweep), Up);
		AimFacing.Z = FMath::Clamp(AimFacing.Z, -0.75f, 0.35f);
		AimFacing.Normalize();
	}
	float HeadStiffness = Settings->DragonNeckStiffness;

	// The bite: draw back and up, lunge out to the prey on the stretched neck, then back
	if (BiteTime >= 0.0f)
	{
		FVector ToPrey = BitePoint - Base;
		ToPrey.Z = 0.0f;
		const FVector PreyWay = ToPrey.IsNearlyZero() ? HeadWay : ToPrey.GetSafeNormal();
		if (BiteTime < 0.22f)
		{
			const float Draw = FMath::SmoothStep(0.0f, 0.22f, BiteTime);
			HeadGoal = FMath::Lerp(HeadGoal, Base + PreyWay * H * 0.3f + Up * H * 0.95f, Draw);
			FacingWay = PreyWay;
			Pitch = -0.35f;
		}
		else if (BiteTime < 0.5f)
		{
			HeadGoal = BitePoint - PreyWay * H * 0.15f + Up * H * 0.08f;
			FacingWay = (BitePoint - HeadGoal).GetSafeNormal();
			Pitch = 0.0f;
			HeadStiffness *= 5.0f;
		}
	}
	HeadGoal.Z = FMath::Max(HeadGoal.Z, Base.Z + H * 0.15f);
	// It keeps its head out of its gardener: roaming close by, it bends round her instead of through her
	if (const AActor* Keeper = GetInstigator())
	{
		const FVector Her = Keeper->GetActorLocation();
		FVector Away = HeadGoal - Her;
		Away.Z = 0.0f;
		const float Room = 130.0f + H * 0.35f;
		if (Away.SizeSquared() < FMath::Square(Room) && FMath::Abs(HeadGoal.Z - Her.Z) < 140.0f)
		{
			const FVector Push = Away.IsNearlyZero() ? (HeadGoal - Base).GetSafeNormal2D() : Away.GetSafeNormal();
			HeadGoal = Her + Push * Room + FVector(0.0f, 0.0f, HeadGoal.Z - Her.Z);
		}
	}
	FVector FacingGoal = FacingWay.GetSafeNormal2D() * FMath::Cos(Pitch) + Up * FMath::Sin(Pitch);
	if (BiteTime >= 0.22f && BiteTime < 0.5f)
	{
		FacingGoal = FacingWay;
	}
	else if (BiteTime < 0.0f && !AimFacing.IsNearlyZero())
	{
		FacingGoal = FMath::Lerp(FacingGoal.GetSafeNormal(), AimFacing, FMath::Clamp(Aim * 1.5f, 0.0f, 1.0f));
	}

	// Bursting out of the earth (Boris 2026-11-19): the ground rumbles and cracks, the head breaks out of it and screws itself up,
	// the body slides out of the hole after it, it towers up high and roars, then settles into its own movement
	bool bUnderground = false;
	if (EmergeTime >= 0.0f)
	{
		const float Was = EmergeTime;
		EmergeTime += Step;
		const float T = EmergeTime;
		AVaelCharacter* Gardener = GetGardener();
		const FVector Side = FVector::CrossProduct(Up, Forward);
		auto Earth = [&](const FVector& At, int32 Count, float Speed, float Lift, float Spread)
		{
			FVaelDebris Clods;
			Clods.Count = Count;
			Clods.Spread = Spread;
			Clods.Speed = Speed;
			Clods.Lift = Lift;
			Clods.Lifetime = 1.1f;
			Clods.MinSize = 2.0f;
			Clods.MaxSize = 7.0f;
			Clods.Color = FLinearColor(0.14f, 0.1f, 0.07f);
			AVaelDebrisBurst::Spawn(Gardener, At, Clods);
		};
		// The rumble: earth jumps up here and there round the foot, the ground shakes a little
		EmergeRumble -= Step;
		if (T < 0.55f && EmergeRumble <= 0.0f && Gardener != nullptr)
		{
			EmergeRumble = 0.09f;
			const float Around = FMath::FRandRange(0.0f, 2.0f * UE_PI);
			Earth(Base + FVector(FMath::Cos(Around), FMath::Sin(Around), 0.0f) * FMath::FRandRange(10.0f, 60.0f) * H / 200.0f, 6, 120.0f, 90.0f, 10.0f);
			UVaelHitFeedbackSubsystem::Shake(this, 0.12f);
		}
		// The burst: the earth breaks open where it comes out
		if (Was < 0.55f && T >= 0.55f && Gardener != nullptr)
		{
			Earth(Base, 60, 420.0f, 520.0f, 45.0f * H / 200.0f);
			UVaelHitFeedbackSubsystem::Shake(this, 0.45f);
		}
		// While the body slides out, clods keep falling off it at the hole
		if (T >= 0.55f && T < 1.6f && EmergeRumble <= 0.0f && Gardener != nullptr)
		{
			EmergeRumble = 0.14f;
			Earth(Base + Up * 10.0f, 8, 180.0f, 220.0f, 25.0f);
		}

		FVector RiseGoal;
		FVector RiseFacing;
		if (T < 0.55f)
		{
			// Still under the ground, pushing up
			RiseGoal = Base - Up * H * (0.35f - 0.15f * T);
			RiseFacing = Up;
		}
		else if (T < 1.45f)
		{
			// Out, screwing itself up in a turn and a half
			const float RiseShare = FMath::SmoothStep(0.55f, 1.45f, T);
			const float Turn = RiseShare * 3.0f * UE_PI;
			const FVector Swirl = (Side * FMath::Cos(Turn) + Forward * FMath::Sin(Turn)) * H * 0.14f * (1.0f - RiseShare);
			RiseGoal = FMath::Lerp(Base - Up * H * 0.2f, Base + Up * H * 1.55f + Forward * H * 0.1f, RiseShare) + Swirl;
			RiseFacing = FMath::Lerp(Up, (Forward + Up * 0.4f).GetSafeNormal(), RiseShare).GetSafeNormal();
		}
		else
		{
			// Towering up, roaring at the sky, swaying
			RiseGoal = Base + Up * H * 1.5f + Forward * H * 0.15f + Side * FMath::Sin((T - 1.45f) * 5.0f) * H * 0.04f;
			RiseFacing = (Forward + Up * 0.45f).GetSafeNormal();
		}
		const float Settle = FMath::SmoothStep(2.2f, 3.2f, T);
		HeadGoal = FMath::Lerp(RiseGoal, HeadGoal, Settle);
		FacingGoal = FMath::Lerp(RiseFacing, FacingGoal.GetSafeNormal(), Settle);
		HeadStiffness *= FMath::Lerp(3.0f, 1.0f, Settle);
		bUnderground = T < 1.6f;
		if (T >= 3.2f)
		{
			EmergeTime = -1.0f;
		}
	}

	// The body: from the roots fast in the ground a curve rises and turns into the head; snake waves run along it, less while it
	// aims and hardly while it breathes. The joints follow their place on it late, the ones near the roots latest
	constexpr int32 Joints = 6;
	const float NeckLength = H * (bWingDragon ? 1.7f : 1.8f);
	const float Rest = NeckLength / (Joints - 1);
	const FVector Root = Base + Up * 4.0f;
	const float WaveAmount = H * 0.13f * (1.0f - 0.5f * Aim) * (1.0f - 0.7f * Burst) * (1.0f - 0.6f * Coil);
	auto BodyAt = [&](float S, const FVector& Head, const FVector& Facing)
	{
		const FVector Chord = Head - Root;
		FVector Flat = Chord.GetSafeNormal2D();
		if (Flat.IsNearlyZero())
		{
			Flat = Forward;
		}
		const FVector Side = FVector::CrossProduct(Up, Flat);
		// It rises from its roots leaning a little away from its head, like a snake rearing
		const FVector P1 = Root + Up * NeckLength * (bWingDragon ? 0.45f : 0.35f) - Flat * H * 0.12f;
		const FVector P2 = Head - Facing.GetSafeNormal() * NeckLength * 0.28f;
		const float T = 1.0f - S;
		const FVector Curve = Root * (T * T * T) + P1 * (3.0f * T * T * S) + P2 * (3.0f * T * S * S) + Head * (S * S * S);
		const float Envelope = FMath::Sin(S * UE_PI);
		return Curve + Side * WaveAmount * FMath::Sin(2.0f * UE_PI * 1.1f * S - Time * 1.7f + Seed) * Envelope
			+ Up * WaveAmount * 0.4f * FMath::Cos(2.0f * UE_PI * 0.8f * S - Time * 1.3f + Seed) * Envelope;
	};

	if (!bNeckPlaced || Spine.Num() != Joints)
	{
		bNeckPlaced = true;
		NeckHead = HeadGoal;
		NeckHeadVelocity = FVector::ZeroVector;
		HeadFacing = FacingGoal.GetSafeNormal();
		Spine.SetNum(Joints);
		SpineVelocity.Init(FVector::ZeroVector, Joints);
		for (int32 K = 0; K < Joints; ++K)
		{
			// Bursting out of the earth it starts coiled straight down in the ground below its roots
			Spine[K] = EmergeTime >= 0.0f ? Root - Up * Rest * 0.9f * K : BodyAt(K / float(Joints - 1), NeckHead, HeadFacing);
		}
		NeckHead = Spine[Joints - 1];
		NeckApex = Spine[2];
		NeckDip = Spine[3];
		return;
	}

	// Damped springs: the head leads, the body follows late and overshoots a little
	auto Follow = [Step](FVector& Point, FVector& Velocity, const FVector& Goal, float Stiffness)
	{
		const float Damping = 2.0f * 0.7f * FMath::Sqrt(Stiffness);
		Velocity += ((Goal - Point) * Stiffness - Velocity * Damping) * Step;
		Point += Velocity * Step;
	};
	Follow(NeckHead, NeckHeadVelocity, HeadGoal, HeadStiffness);
	HeadFacing = FMath::VInterpNormalRotationTo(HeadFacing, FacingGoal.GetSafeNormal(), Step, BiteTime >= 0.0f ? 720.0f : 260.0f);
	for (int32 K = 1; K < Joints - 1; ++K)
	{
		const float S = K / float(Joints - 1);
		// Coming out of the hole the body follows the head straight through it
		const FVector BodyGoal = bUnderground ? FMath::Lerp(Root, NeckHead, S) : BodyAt(S, NeckHead, HeadFacing);
		Follow(Spine[K], SpineVelocity[K], BodyGoal, Settings->DragonNeckStiffness * (bUnderground ? 1.2f : 0.25f + 0.45f * S));
	}
	Spine[0] = Root;
	Spine[Joints - 1] = NeckHead;

	// The body keeps its length: no joint gets further from the one below than its piece allows (it may coil up shorter), and the
	// body stays off the ground; so the head can only go as far as the neck reaches
	for (int32 K = 1; K < Joints; ++K)
	{
		const FVector Piece = Spine[K] - Spine[K - 1];
		const float Length = Piece.Size();
		FVector Placed = Spine[K - 1] + (Length > KINDA_SMALL_NUMBER ? Piece / Length : Up) * FMath::Clamp(Length, Rest * 0.5f, Rest * 1.08f);
		if (K >= 2 && !bUnderground)
		{
			// Just out of the earth the last of the body rises smoothly off the ground instead of popping up
			const float Floor = Base.Z + H * 0.1f;
			if (Placed.Z < Floor)
			{
				Placed.Z = EmergeTime >= 0.0f ? FMath::Lerp(Placed.Z, Floor, FMath::Min(1.0f, Step * 5.0f)) : Floor;
			}
		}
		const FVector Correction = Placed - Spine[K];
		Spine[K] = Placed;
		FVector& Velocity = K == Joints - 1 ? NeckHeadVelocity : SpineVelocity[K];
		if (!Correction.IsNearlyZero())
		{
			// What pushes against the length is lost
			const FVector Against = Correction.GetSafeNormal();
			Velocity -= Against * FMath::Min(FVector::DotProduct(Velocity, Against), 0.0f);
		}
	}
	NeckHead = Spine[Joints - 1];
	NeckApex = Spine[2];
	NeckDip = Spine[3];
}

void AVaelPlantTower::LaySpine(float Height, float NeckSlim)
{
	const int32 Joints = Spine.Num();
	const int32 Segments = Joints - 1;
	if (Joints < 3 || SpinePieces.Num() < Segments * 2)
	{
		return;
	}
	const int32 Vines = SpinePieces.Num() / Segments - 1;
	const FTransform& Transform = GetActorTransform();
	const float Size = FMath::Max(Height / 200.0f, 0.05f);
	const float Seed = GetUniqueID() * 0.3f;

	// Frames along the body, carried from joint to joint without twisting; the last one turns into the facing of the head
	TArray<FVector, TInlineAllocator<8>> Tangents, Normals, Binormals;
	for (int32 K = 0; K < Joints; ++K)
	{
		FVector Tangent = (Spine[FMath::Min(K + 1, Segments)] - Spine[FMath::Max(K - 1, 0)]).GetSafeNormal();
		if (K == Segments)
		{
			Tangent = (Tangent + HeadFacing).GetSafeNormal();
		}
		const FVector Before = K == 0 ? (FMath::Abs(Tangent.Z) < 0.9f ? FVector::UpVector : FVector::ForwardVector) : Normals[K - 1];
		const FVector Normal = (Before - Tangent * FVector::DotProduct(Before, Tangent)).GetSafeNormal();
		Tangents.Add(Tangent);
		Normals.Add(Normal);
		Binormals.Add(FVector::CrossProduct(Tangent, Normal));
	}

	// Each vine winds round the body, spread wide at the roots and close at the nape, where it runs into the skull; the core in
	// the middle fills the gaps between them
	for (int32 Vine = 0; Vine <= Vines; ++Vine)
	{
		const bool bCore = Vine == Vines;
		TArray<FVector, TInlineAllocator<8>> Points;
		TArray<float, TInlineAllocator<8>> Rolls;
		for (int32 K = 0; K < Joints; ++K)
		{
			const float Along = K / float(Segments);
			const float Angle = Vine * 2.0f * UE_PI / FMath::Max(Vines, 1) + Along * 4.2f + Seed;
			const float Round = bCore ? 0.0f : FMath::Lerp(K == 0 ? 15.0f : 10.0f, 4.0f, Along) * Size;
			FVector Point = Spine[K] + (Normals[K] * FMath::Cos(Angle) + Binormals[K] * FMath::Sin(Angle)) * Round;
			// Into the ground at the roots
			if (K == 0)
			{
				Point.Z -= 8.0f * Size;
			}
			Points.Add(Point);
			Rolls.Add(Angle);
		}
		for (int32 K = 0; K < Segments; ++K)
		{
			USplineMeshComponent* Piece = SpinePieces[Vine * Segments + K];
			if (Piece == nullptr)
			{
				continue;
			}
			const FVector T0 = (Points[FMath::Min(K + 1, Segments)] - Points[FMath::Max(K - 1, 0)]) * (K == 0 ? 1.0f : 0.5f);
			const FVector T1 = K + 1 == Segments ? Tangents[Segments] * FVector::Dist(Points[K], Points[K + 1])
				: (Points[K + 2] - Points[K]) * 0.5f;
			const float Thick0 = FMath::Lerp(3.0f, 1.6f, K / float(Segments)) * NeckSlim * (bCore ? 1.6f : 1.0f);
			const float Thick1 = FMath::Lerp(3.0f, 1.6f, (K + 1) / float(Segments)) * NeckSlim * (bCore ? 1.6f : 1.0f);
			Piece->SetStartAndEnd(Transform.InverseTransformPosition(Points[K]), Transform.InverseTransformVectorNoScale(T0),
				Transform.InverseTransformPosition(Points[K + 1]), Transform.InverseTransformVectorNoScale(T1), false);
			Piece->SetStartScale(FVector2D(Thick0), false);
			Piece->SetEndScale(FVector2D(Thick1), false);
			Piece->SetStartRoll(Rolls[K], false);
			Piece->SetEndRoll(Rolls[K + 1], false);
			Piece->UpdateMesh();
		}
	}

	// Roots spreading out from the foot and down into the ground, holding it fast
	for (int32 Index = 0; Index < GroundRoots.Num(); ++Index)
	{
		const float Angle = Index * 2.0f * UE_PI / GroundRoots.Num() + Seed * 2.0f + 0.3f * FMath::Sin(Index * 2.7f);
		const FVector Out(FMath::Cos(Angle), FMath::Sin(Angle), 0.0f);
		// They spread out of the cracked earth while the dragon bursts out
		const float Spread = EmergeTime >= 0.0f ? FMath::SmoothStep(0.5f, 1.8f, EmergeTime) : 1.0f;
		const float Length = FMath::Max((55.0f + 25.0f * FMath::Frac(Index * 0.618f)) * Size * Spread, 1.0f);
		const FVector Start = Spine[0] + Out * 10.0f * Size + FVector(0.0f, 0.0f, 10.0f * Size);
		const FVector End = Spine[0] + Out * Length - FVector(0.0f, 0.0f, 14.0f * Size);
		USplineMeshComponent* Root = GroundRoots[Index];
		Root->SetStartAndEnd(Transform.InverseTransformPosition(Start), Transform.InverseTransformVectorNoScale((Out * 0.7f - FVector::UpVector * 0.4f) * Length),
			Transform.InverseTransformPosition(End), Transform.InverseTransformVectorNoScale((Out - FVector::UpVector * 0.6f) * Length), false);
		Root->SetStartScale(FVector2D(2.2f * NeckSlim), false);
		Root->SetEndScale(FVector2D(0.5f * NeckSlim), false);
		Root->SetStartRoll(Angle, false);
		Root->SetEndRoll(Angle + 1.5f, false);
		Root->UpdateMesh();
	}
}

float AVaelPlantTower::GetBreathSeconds() const
{
	return bSteamDragon ? UVaelFloromantSettings::Get()->DragonSteamSeconds : bThunderDragon ? UVaelFloromantSettings::Get()->DragonThunderSeconds : bSunDragon ? UVaelFloromantSettings::Get()->DragonSunSeconds : bWingDragon ? 0.6f : bFloodDragon ? UVaelFloromantSettings::Get()->DragonFloodSeconds : DragonDrumHeads * 0.075f;
}

void AVaelPlantTower::PuffSteam(const FVector& Mouth, const FVector& Way, float Height)
{
	AVaelCharacter* Gardener = GetGardener();
	if (Gardener == nullptr)
	{
		return;
	}
	const UVaelFloromantSettings* Settings = UVaelFloromantSettings::Get();
	const float Grown = FMath::Max(Height / 200.0f, 0.1f);
	const float Reach = Settings->DragonSteamReach * Grown;
	const float Width = Settings->DragonSteamWidth * Grown;
	const FVector Flat = Way.GetSafeNormal2D();
	++JetPuffs;

	// Whoever stands in the jet is scalded, a tenth of a second at a time
	for (TActorIterator<AVaelCharacterBase> It(GetWorld()); It; ++It)
	{
		AVaelCharacterBase* Enemy = *It;
		if (!UVaelCombatStatics::CanDamage(Gardener, Enemy))
		{
			continue;
		}
		const FVector To = Enemy->GetActorLocation() - Mouth;
		const float Along = FVector::DotProduct(FVector(To.X, To.Y, 0.0f), Flat);
		const float Aside = FMath::Abs(FVector::CrossProduct(Flat, FVector(To.X, To.Y, 0.0f)).Z);
		// The jet widens a little as it flies
		if (Along > 0.0f && Along < Reach && Aside < Width * (0.6f + 0.6f * Along / Reach))
		{
			UVaelCombatStatics::DealDamage(Gardener, Enemy, Settings->DragonSteamDamagePerSecond * 0.1f * JetPower, EVaelElement::Fire);
		}
	}

	if (DragonPartner != nullptr && JetPuffs % 2 == 1)
	{
		VaelSymbiosis::MuzzlePuff(this, DragonPartner, Mouth, bShadow, 1.0f);
	}

	// Every third puff leaves a cloud of scalding steam where the jet comes down, streaming out of the jaws to it (like the
	// steam the mage blows): it blinds and burns whoever stands in it, and puts out fires
	if (JetPuffs % 3 == 1)
	{
		FVector Place = Mouth + Flat * Reach * FMath::FRandRange(0.6f, 0.95f);
		FHitResult Ground;
		if (VaelGround::TraceGround(GetWorld(), Place + FVector(0.0f, 0.0f, 300.0f), Place - FVector(0.0f, 0.0f, 1200.0f), Ground, this))
		{
			Place = Ground.ImpactPoint;
		}
		if (AVaelGroundArea* Cloud = AVaelGroundArea::SpawnArea(GetWorld(), Place, EVaelElement::Water, Settings->DragonSteamCloudRadius * FMath::Sqrt(Grown),
			Settings->DragonSteamCloudSeconds, Settings->DragonSteamCloudDamage * JetPower, Gardener, true, EVaelGroundEffect::Blind))
		{
			Cloud->SetBreathOrigin(Mouth, Flat);
		}
	}
}

void AVaelPlantTower::UpdateJetCards(float DeltaSeconds, const FVector& Mouth, const FVector& Way, float Height)
{
	const float Grown = FMath::Max(Height / 200.0f, 0.1f);
	const FVector Flat = Way.GetSafeNormal2D();
	const FVector Side = FVector::CrossProduct(FVector::UpVector, Flat);

	// While the jet runs, new cards are born at the gland, many a second, flying out fast with a little scatter
	if (JetLeft > 0.0f && Jaw > 0.45f && !bWithering)
	{
		JetSpawnDebt += DeltaSeconds * DragonJetRate;
		for (int32 Index = 0; Index < JetCardAges.Num() && JetSpawnDebt >= 1.0f; ++Index)
		{
			if (JetCardAges[Index] >= 0.0f)
			{
				continue;
			}
			JetSpawnDebt -= 1.0f;
			JetCardAges[Index] = 0.0f;
			JetCardPlaces[Index] = Mouth + Side * FMath::FRandRange(-4.0f, 4.0f) * Grown;
			const FVector Scatter = Side * FMath::FRandRange(-0.12f, 0.12f) + FVector::UpVector * FMath::FRandRange(-0.08f, 0.05f);
			JetCardVelocities[Index] = (Flat + Scatter).GetSafeNormal() * DragonJetSpeed * Grown * FMath::FRandRange(0.85f, 1.1f);
		}
		JetSpawnDebt = FMath::Min(JetSpawnDebt, 2.0f);
	}
	else
	{
		JetSpawnDebt = 0.0f;
	}

	// Each card slows in the air, sinks a little at first and then rises as the steam cools; it swells and fades like the
	// mage's breath of steam
	const float Drag = FMath::Exp(-2.4f * DeltaSeconds);
	for (int32 Index = 0; Index < JetCardAges.Num(); ++Index)
	{
		if (JetCardAges[Index] < 0.0f)
		{
			JetCards->SetCard(Index, FVector::ZeroVector, 1.0f, 0.0f);
			continue;
		}
		JetCardAges[Index] += DeltaSeconds;
		const float Life = JetCardAges[Index] / DragonJetLife;
		if (Life >= 1.0f)
		{
			JetCardAges[Index] = -1.0f;
			JetCards->SetCard(Index, FVector::ZeroVector, 1.0f, 0.0f);
			continue;
		}
		JetCardVelocities[Index] = JetCardVelocities[Index] * Drag + FVector::UpVector * 70.0f * Grown * DeltaSeconds;
		JetCardPlaces[Index] += JetCardVelocities[Index] * DeltaSeconds;
		const float Swell = 1.0f - FMath::Square(1.0f - Life);
		const float Fade = FMath::Min(Life * 10.0f, 1.0f) * FMath::Pow(1.0f - Life, 1.3f);
		JetCards->SetCard(Index, JetCardPlaces[Index], FMath::Lerp(DragonJetBirthSize, DragonJetEndSize, Swell) * Grown, DragonJetSolid * Fade);
	}
	JetCards->Commit();
}

void AVaelPlantTower::StrikeThunder(const FVector& Mouth, float Power, float Height)
{
	AVaelCharacter* Gardener = GetGardener();
	if (Gardener == nullptr)
	{
		return;
	}
	const UVaelFloromantSettings* Settings = UVaelFloromantSettings::Get();
	const float Grown = FMath::Max(Height / 200.0f, 0.1f);

	// First the one it aimed at, if it still stands; else the nearest in reach
	AActor* First = BurstTarget.Get();
	const AVaelCharacterBase* FirstCharacter = Cast<AVaelCharacterBase>(First);
	if (FirstCharacter == nullptr || FirstCharacter->IsDefeated())
	{
		First = VaelSymbiosis::FindTarget(Gardener, GetActorLocation(), Plant->ShotRange);
	}
	if (First == nullptr)
	{
		// Nothing there: it strikes the ground ahead
		AVaelLightningBolt::Spawn(this, Mouth, Mouth + HeadFacing.GetSafeNormal2D() * 500.0f * Grown - FVector(0.0f, 0.0f, Height * 0.5f), DragonThunderLight, 0.3f, 1.6f, true);
		return;
	}

	// Then it leaps from enemy to enemy, nearest first; a wet one, or one standing in steam, carries it further and farther
	FVaelSpellHit Bolt;
	Bolt.Element = EVaelElement::Air;
	Bolt.bLightning = true;
	TArray<AActor*> Struck;
	AActor* Current = First;
	FVector From = Mouth;
	int32 LeapsLeft = Settings->DragonThunderJumps;
	float Damage = Settings->DragonThunderDamage * Power;
	while (Current != nullptr)
	{
		const FVector To = Current->GetActorLocation() + FVector(0.0f, 0.0f, 30.0f);
		AVaelLightningBolt::Spawn(this, From, To, DragonThunderLight, 0.3f, Struck.IsEmpty() ? 1.8f : 1.3f, true);
		Bolt.Damage = Damage;
		UVaelCombatStatics::ApplySpellHit(Gardener, Current, Bolt, (To - From).GetSafeNormal2D());
		Struck.Add(Current);

		const bool bConducts = UVaelCombatStatics::HasStatus(Current, EVaelStatus::Wet)
			|| AVaelGroundArea::FindEffectAt(GetWorld(), Current->GetActorLocation(), EVaelGroundEffect::Blind) != nullptr;
		LeapsLeft += bConducts ? Settings->DragonThunderWetJumps : 0;
		if (LeapsLeft <= 0)
		{
			break;
		}
		--LeapsLeft;
		const float Range = Settings->DragonThunderJumpRange * (bConducts ? Settings->DragonThunderWetRange : 1.0f);

		AActor* Next = nullptr;
		float Best = Range * Range;
		for (TActorIterator<AVaelCharacterBase> It(GetWorld()); It; ++It)
		{
			if (Struck.Contains(*It) || It->IsDefeated() || !UVaelCombatStatics::CanDamage(Gardener, *It))
			{
				continue;
			}
			const float Distance = FVector::DistSquared(It->GetActorLocation(), Current->GetActorLocation());
			if (Distance < Best)
			{
				Best = Distance;
				Next = *It;
			}
		}
		From = To;
		Current = Next;
		Damage *= Settings->DragonThunderJumpKeep;
	}
}

void AVaelPlantTower::UpdateSunBeam(float DeltaSeconds, float Height)
{
	const UVaelFloromantSettings* Settings = UVaelFloromantSettings::Get();
	const bool bOn = bSunLit && SunLeft > 0.0f && !bWithering;
	// It shoots out of the glass at once and fades quickly when it ends
	SunShown = FMath::FInterpTo(SunShown, bOn ? 1.0f : 0.0f, DeltaSeconds, bOn ? 14.0f : 9.0f);
	const bool bVisible = SunShown > 0.02f;
	const float Grown = FMath::Max(Height / 200.0f, 0.1f);
	const float Time = GetWorld()->GetTimeSeconds();

	// Where it reaches: as far as it has shot out, stopped by walls
	const FVector Start = SunMouth;
	FVector Way = HeadFacing.GetSafeNormal2D();
	if (Way.IsNearlyZero())
	{
		Way = GetActorForwardVector();
	}
	float Length = (bFloodDragon ? Settings->DragonFloodReach : Settings->DragonSunReach) * Grown * SunShown;
	FHitResult Wall;
	FCollisionQueryParams Query(SCENE_QUERY_STAT(VaelSunBeam), false, this);
	bool bImpact = false;
	if (bVisible && GetWorld()->LineTraceSingleByObjectType(Wall, Start, Start + Way * Length, FCollisionObjectQueryParams(ECC_WorldStatic), Query))
	{
		Length = Wall.Distance;
		bImpact = true;
	}

	// The focused beam ends on the first enemy it meets (Boris 2026-11-19: it burns only where it strikes)
	AVaelCharacter* Gardener = GetGardener();
	AVaelCharacterBase* Struck = nullptr;
	if (bVisible && Gardener != nullptr)
	{
		const float Width = (bFloodDragon ? Settings->DragonFloodHalfWidth : Settings->DragonSunHalfWidth) * Grown;
		for (TActorIterator<AVaelCharacterBase> It(GetWorld()); It; ++It)
		{
			if (!UVaelCombatStatics::CanDamage(Gardener, *It))
			{
				continue;
			}
			const FVector To = It->GetActorLocation() - Start;
			const float Ahead = FVector::DotProduct(FVector(To.X, To.Y, 0.0f), Way);
			const float Aside = FMath::Abs(Way.X * To.Y - Way.Y * To.X);
			if (Ahead > 0.0f && Ahead < Length && Aside < Width)
			{
				Length = Ahead;
				Struck = *It;
				bImpact = true;
			}
		}
	}
	const FVector End = Start + Way * Length;

	// The look: a thin white-hot core inside a narrow golden halo, flickering, a glow where it ends, motes of light running out
	const FRotator Along = FRotationMatrix::MakeFromZ(Way).Rotator();
	const float Flicker = 0.85f + 0.15f * FMath::Sin(Time * 47.0f) * FMath::Sin(Time * 31.0f);
	auto Stretch = [&](UStaticMeshComponent* Shape, float Radius, float Glow)
	{
		if (Shape == nullptr)
		{
			return;
		}
		Shape->SetVisibility(bVisible);
		if (bVisible)
		{
			Shape->SetWorldLocationAndRotation((Start + End) * 0.5f, Along);
			Shape->SetWorldScale3D(FVector(Radius / 50.0f, Radius / 50.0f, FMath::Max(Length, 1.0f) / 100.0f));
			VaelEffects::SetLookGlow(Shape, Glow);
		}
	};
	// Water leaves the bulb as a narrow high-pressure jet; light as a thin white-hot line
	const float Thick = bFloodDragon ? 1.25f : 1.0f;
	Stretch(SunCore, 1.6f * Thick * Grown * Flicker * SunShown, (bFloodDragon ? 7.0f : 9.0f) * SunShown);
	Stretch(SunHalo, 5.5f * Thick * Grown * Flicker * SunShown, (bFloodDragon ? 1.2f : 1.8f) * SunShown);
	if (SunEnd != nullptr)
	{
		SunEnd->SetVisibility(bVisible);
		if (bVisible)
		{
			SunEnd->SetWorldLocation(End);
			SunEnd->SetWorldScale3D(FVector(22.0f * Grown * Flicker * SunShown / 100.0f));
			VaelEffects::SetLookGlow(SunEnd, 6.0f * SunShown);
		}
	}
	for (int32 Index = 0; Index < SunMotes.Num(); ++Index)
	{
		UStaticMeshComponent* Mote = SunMotes[Index];
		Mote->SetVisibility(bVisible);
		if (bVisible)
		{
			const float Share = FMath::Frac(Time * (bFloodDragon ? 5.0f : 2.2f) + Index / float(SunMotes.Num()));
			Mote->SetWorldLocation(Start + Way * Length * Share + FVector(0.0f, 0.0f, FMath::Sin(Time * 9.0f + Index) * 2.0f * Grown));
			Mote->SetWorldScale3D(FVector(3.5f * Grown * SunShown / 100.0f));
			VaelEffects::SetLookGlow(Mote, 5.0f * SunShown);
		}
	}

	if (!bOn || Gardener == nullptr || Length < 10.0f)
	{
		return;
	}

	// The water jet of the flood dragon: it soaks and pushes back the one it strikes and splashes where it hits; no fire
	if (bFloodDragon)
	{
		SunBurnTimer -= DeltaSeconds;
		if (SunBurnTimer <= 0.0f)
		{
			SunBurnTimer = 0.15f;
			if (Struck != nullptr)
			{
				UVaelCombatStatics::DealDamage(Gardener, Struck, Settings->DragonFloodDamagePerSecond * 0.15f * SunPower, EVaelElement::Water);
				UVaelCombatStatics::ApplyStatus(Gardener, Struck, EVaelStatus::Wet, 4.0f);
				Struck->ApplyKnockback(Way, Settings->DragonFloodPush);
			}
			if (bImpact)
			{
				VaelSymbiosis::MuzzlePuff(this, Plant, End, bShadow, 1.4f);
			}
		}
		return;
	}

	// It burns the one it strikes, a tenth of a second at a time
	SunBurnTimer -= DeltaSeconds;
	if (SunBurnTimer <= 0.0f)
	{
		SunBurnTimer = 0.1f;
		if (Struck != nullptr)
		{
			UVaelCombatStatics::DealDamage(Gardener, Struck, Settings->DragonSunDamagePerSecond * 0.1f * SunPower, EVaelElement::Fire);
		}
	}

	// Only where it strikes (an enemy or a wall) the ground catches fire, no trail through the air: a new fire only once the
	// point has moved on from the last one, or that one is burning low
	SunFireTimer -= DeltaSeconds;
	const float FireRadius = Settings->DragonSunFireRadius * FMath::Sqrt(Grown);
	if (bImpact && (SunFireTimer <= 0.0f || FVector::Dist2D(End, SunFirePlace) > FireRadius * 1.6f))
	{
		FHitResult Ground;
		if (VaelGround::TraceGround(GetWorld(), End + FVector(0.0f, 0.0f, 300.0f), End - FVector(0.0f, 0.0f, 1200.0f), Ground, this))
		{
			SunFireTimer = Settings->DragonSunFireSeconds * 0.8f;
			SunFirePlace = Ground.ImpactPoint;
			AVaelGroundArea::SpawnArea(GetWorld(), Ground.ImpactPoint + FVector(0.0f, 0.0f, 2.0f), EVaelElement::Fire, FireRadius,
				Settings->DragonSunFireSeconds, Settings->DragonSunFireDamage * SunPower, Gardener);
		}
	}
}

void AVaelPlantTower::UpdateWings(float DeltaSeconds, float Height)
{
	if (WingBlades.IsEmpty() || WingBones.IsEmpty())
	{
		return;
	}
	const float Grown = FMath::Max(Height / 200.0f, 0.1f);
	const float H = FMath::Max(Height, 1.0f);
	const float Time = GetWorld()->GetTimeSeconds();
	const float Appear = FMath::SmoothStep(0.45f, 1.0f, DragonGrow);
	const bool bShown = Appear > 0.02f && !bAbsorbed;
	const FVector Up = FVector::UpVector;
	FVector Forward = HeadFacing.GetSafeNormal2D();
	if (Forward.IsNearlyZero())
	{
		Forward = FVector::ForwardVector;
	}
	const FVector Side = FVector::CrossProduct(Up, Forward);

	// The strike: raise and draw back (0 to 0.2 s), then sweep forward so the fan flies ahead (loosed at 0.26 s), settle
	float SpreadGoal = 0.85f;
	float LiftGoal = 0.45f + 0.2f * FMath::Sin(Time * 2.3f);
	float SweepGoal = 0.0f;
	float Speed = 6.0f;
	const bool bCharging = Target.IsValid() && !bWithering && FireCountdown < 0.8f && WingStrike < 0.0f;
	if (bCharging)
	{
		SpreadGoal = 1.0f;
		LiftGoal = 0.9f;
		SweepGoal = -0.4f;
	}
	if (WingStrike >= 0.0f)
	{
		WingStrike += DeltaSeconds;
		const bool bDraw = WingStrike < 0.2f;
		SpreadGoal = 1.0f;
		LiftGoal = bDraw ? 1.0f : 0.15f;
		SweepGoal = bDraw ? -0.5f : 0.75f;
		Speed = bDraw ? 14.0f : 26.0f;
		if (!bWingFired && WingStrike >= 0.26f)
		{
			bWingFired = true;
			AVaelCharacter* Gardener = GetGardener();
			const UVaelFloromantSettings* Settings = UVaelFloromantSettings::Get();
			// Each grown blade flies off its wing straight ahead, the fan spread from the left wing's edge to the right wing's
			for (int32 Index = 0; Index < FeatherTips.Num() && Gardener != nullptr; ++Index)
			{
				if (FeatherGrow[Index] < 0.9f)
				{
					continue;
				}
				const bool bLeft = Index < DragonWingFeathers;
				const float Share = (Index % DragonWingFeathers) / float(DragonWingFeathers - 1);
				const float Yaw = (bLeft ? -1.0f : 1.0f) * FMath::Lerp(4.0f, 30.0f, Share);
				VaelSymbiosis::FireShot(Gardener, Plant, FeatherTips[Index], Forward.RotateAngleAxis(Yaw, Up), WingPower, bShadow, Settings->TowerShotScale * 0.9f, 0.55f);
				FeatherGrow[Index] = 0.0f;
				if (Index % 3 == 0)
				{
					VaelSymbiosis::MuzzlePuff(this, Plant, FeatherTips[Index], bShadow, 0.9f);
				}
			}
			BendVelocity -= Forward * 0.4f;
			NeckHeadVelocity -= Forward * 120.0f * Grown;
		}
		if (WingStrike >= 0.6f || bWithering)
		{
			WingStrike = -1.0f;
		}
	}
	WingSpread = FMath::FInterpTo(WingSpread, SpreadGoal * Appear, DeltaSeconds, Speed);
	WingLift = FMath::FInterpTo(WingLift, LiftGoal, DeltaSeconds, Speed);
	WingSweep = FMath::FInterpTo(WingSweep, SweepGoal, DeltaSeconds, Speed);

	const FTransform& Transform = GetActorTransform();
	auto Lay = [&](USplineMeshComponent* Piece, const FVector& A, const FVector& TA, const FVector& B, const FVector& TB, float S0, float S1, float Roll)
	{
		Piece->SetVisibility(bShown);
		if (!bShown)
		{
			return;
		}
		Piece->SetStartAndEnd(Transform.InverseTransformPosition(A), Transform.InverseTransformVectorNoScale(TA),
			Transform.InverseTransformPosition(B), Transform.InverseTransformVectorNoScale(TB), false);
		Piece->SetStartScale(FVector2D(S0), false);
		Piece->SetEndScale(FVector2D(S1), false);
		Piece->SetStartRoll(Roll, false);
		Piece->SetEndRoll(Roll + 0.8f, false);
		Piece->UpdateMesh();
	};

	// The wings leave the back of the upright body, between the low back bulge and the forward one
	const FVector Shoulder = GetHookCurve(1.45f) - Forward * 6.0f * Grown;
	const float Lift = 0.5f + 0.6f * WingLift;
	for (int32 Wing = 0; Wing < 2; ++Wing)
	{
		const float SideSign = Wing == 0 ? -1.0f : 1.0f;
		const FVector Out = Side * SideSign;

		// Arm: two strands from the shoulder up and out to the elbow, swinging forward with the strike
		const FVector ArmWay = (Out * FMath::Cos(Lift) + Up * FMath::Sin(Lift) + Forward * (0.8f * WingSweep - 0.35f)).GetSafeNormal();
		const FVector Elbow = Shoulder + Out * 8.0f * Grown + ArmWay * 0.35f * H * Appear;
		const FVector Mid = FMath::Lerp(Shoulder, Elbow, 0.5f) + Up * 0.05f * H * Appear;
		const int32 BoneBase = Wing * (2 + DragonWingRibs);
		Lay(WingBones[BoneBase], Shoulder, (Mid - Shoulder) * 1.2f, Mid, (Elbow - Shoulder) * 0.6f, 3.2f * Grown, 2.6f * Grown, SideSign);
		Lay(WingBones[BoneBase + 1], Mid, (Elbow - Shoulder) * 0.6f, Elbow, (Elbow - Mid) * 1.2f, 2.6f * Grown, 2.1f * Grown, SideSign + 1.0f);

		// Ribs: strands fanning from the elbow, from the leading one (up) to the trailing one (back and low); swept forward they
		// point ahead and the fan of the wing faces the enemy
		const FVector UpWay = (Up + Forward * (0.9f * WingSweep - 0.15f)).GetSafeNormal();
		const FVector BackWay = (-Forward * (1.0f - 0.7f * WingSweep) - Up * 0.35f + Out * 0.2f).GetSafeNormal();
		FVector RibTips[DragonWingRibs];
		for (int32 Rib = 0; Rib < DragonWingRibs; ++Rib)
		{
			const float Share = Rib / float(DragonWingRibs - 1);
			const float Angle = FMath::Lerp(0.0f, 1.35f, Share) * WingSpread;
			const FVector Way = (UpWay * FMath::Cos(Angle) + BackWay * FMath::Sin(Angle) + Out * 0.45f).GetSafeNormal();
			const float Length = FMath::Lerp(0.62f, 0.38f, Share) * H * Appear;
			RibTips[Rib] = Elbow + Way * Length - Up * Length * 0.08f + Up * FMath::Sin(Time * 5.0f + Rib) * 1.5f * Grown;
			Lay(WingBones[BoneBase + 2 + Rib], Elbow, Way * Length, RibTips[Rib], (Way - Up * 0.3f).GetSafeNormal() * Length,
				FMath::Lerp(2.1f, 1.5f, Share) * Grown, 0.45f * Grown, Rib * 0.9f);
		}

		// Skin: two Windhalm blades in each gap between two ribs, from near the elbow out to the edge between their tips
		for (int32 Gap = 0; Gap < DragonWingRibs - 1; ++Gap)
		{
			for (int32 Fill = 0; Fill < 2; ++Fill)
			{
				const int32 Index = Wing * DragonWingFeathers + Gap * 2 + Fill;
				if (!WingBlades.IsValidIndex(Index))
				{
					continue;
				}
				FeatherGrow[Index] = FMath::Min(1.0f, FeatherGrow[Index] + DeltaSeconds / 1.2f);
				const float BladeGrown = FMath::SmoothStep(0.0f, 1.0f, FeatherGrow[Index]);
				USplineMeshComponent* Blade = WingBlades[Index];
				const bool bShow = bShown && BladeGrown > 0.03f;
				Blade->SetVisibility(bShow);
				const FVector Edge = FMath::Lerp(RibTips[Gap], RibTips[Gap + 1], (Fill + 1) / 3.0f);
				const FVector Start = FMath::Lerp(Elbow, Edge, 0.1f);
				const FVector Tip = Start + (FMath::Lerp(Elbow, Edge, 0.94f) - Start) * BladeGrown;
				FeatherTips[Index] = Tip;
				if (!bShow)
				{
					continue;
				}
				const FVector Way = Tip - Start;
				Lay(Blade, Start, Way, Tip, Way, Plant->FingerThickness * 2.6f * Grown, Plant->FingerThickness * 1.2f * Grown, SideSign * 1.57f);
			}
		}
	}
}
