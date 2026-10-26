// Copyright Epic Games, Inc. All Rights Reserved.

#include "Floromant/VaelPlantTower.h"
#include "Components/SplineMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Floromant/VaelFloromantSettings.h"
#include "Floromant/VaelGlimmDome.h"
#include "Floromant/VaelPlantArmData.h"
#include "Floromant/VaelRoseHeart.h"
#include "Floromant/VaelSymbiosisKnotComponent.h"
#include "Magic/VaelDebrisBurst.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Player/VaelCharacter.h"
#include "UI/VaelCombatTextSubsystem.h"
#include "World/VaelSafeZone.h"
#include "Vael.h"
#include "VaelAssets.h"

namespace
{
	/** Twisted vines of the column, stalks of the crown */
	constexpr int32 TowerColumnVines = 4;
	constexpr int32 TowerCrownStalks = 5;

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

AVaelPlantTower* AVaelPlantTower::PlantTower(AVaelCharacter* Gardener, UVaelPlantArmData* InPlant, const FVector& GroundLocation)
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
	Tower->bShadow = Gardener->GetPath() == EVaelPath::Shadow;
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
	float Best = FMath::Square(Settings->SymbiosisRadius);
	for (TActorIterator<AVaelPlantTower> It(World); It; ++It)
	{
		AVaelPlantTower* Other = *It;
		if (Other == Tower || !Other->IsStanding() || Other->Partner.IsValid() || VaelSymbiosis::Find(InPlant, Other->Plant) == EVaelSymbiosis::None)
		{
			continue;
		}
		const float Distance = FVector::DistSquared2D(Other->GetActorLocation(), GroundLocation);
		if (Distance < Best)
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
			Petal->SetMaterial(0, Material);
			Petal->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			Petal->SetupAttachment(RootComponent);
			Petal->RegisterComponent();
			CrownPieces.Add(Petal);
		}
		return;
	}

	UStaticMesh* TipMesh = VaelAssets::LoadOptional(Plant->TipMesh);
	for (int32 Index = 0; Index < TowerCrownStalks; ++Index)
	{
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
	OutLength = UVaelFloromantSettings::Get()->TowerHeight * TrunkHeightScale * FMath::Sin(Shown * UE_HALF_PI);
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

FVector AVaelPlantTower::GetOwnCrown() const
{
	// The column bends as a bow of constant curve: its length stays, the crown sinks and moves out the more it bends
	const float Length = UVaelFloromantSettings::Get()->TowerHeight * FMath::Sin(FMath::Clamp(Grow, 0.0f, 1.0f) * UE_HALF_PI);
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
	const float Angle = Bend.Size();
	return Angle < KINDA_SMALL_NUMBER ? FVector::UpVector : (FVector::UpVector * FMath::Cos(Angle) + Bend / Angle * FMath::Sin(Angle));
}

bool AVaelPlantTower::AimsLikeHydra() const
{
	return Symbiosis == EVaelSymbiosis::None && !Plant->bShield && !Plant->bVineArm && !Plant->bMortar;
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
	if (Symbiosis != EVaelSymbiosis::None)
	{
		// The shared trunk (bent by the leader) bows its blossom towards the target, less than a single neck
		Lean = FMath::Lerp(0.06f, 0.45f, AimBlend);
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

void AVaelPlantTower::Wither()
{
	if (bWithering)
	{
		return;
	}
	bWithering = true;
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
	if (bActs)
	{
		FireCountdown -= DeltaSeconds;
		if (FireCountdown <= 0.0f)
		{
			const UVaelFloromantSettings* Settings = UVaelFloromantSettings::Get();
			AActor* Goal = AVaelSafeZone::IsSafe(GetWorld(), GetActorLocation()) ? nullptr : VaelSymbiosis::FindTarget(GetGardener(), GetActorLocation(), Plant->ShotRange);
			Target = Goal;
			// A shooter fires only once its crown looks at the target, like a Hydra
			const FVector GoalWay = Goal != nullptr ? (Goal->GetActorLocation() - GetActorLocation()).GetSafeNormal2D() : FVector::ZeroVector;
			if (Goal != nullptr && AimsLikeHydra() && (FVector::DotProduct(LookDirection, GoalWay) < 0.95f || AimBlend < 0.7f))
			{
				FireCountdown = 0.05f;
			}
			else if (Goal != nullptr)
			{
				Fire(Goal, 1.0f);
				FireCountdown = Symbiosis != EVaelSymbiosis::None ? VaelSymbiosis::GetTowerInterval(Symbiosis) : Plant->FireInterval * Settings->TowerFireIntervalScale;
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

	UpdateLook(DeltaSeconds);

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

	Partner = Other;
	Other->Partner = this;
	Other->Symbiosis = Symbiosis;
	bLeads = true;
	Other->bLeads = false;

	// Together they stand longer
	LifeLeft = FMath::Max(LifeLeft, Other->LifeLeft) + Settings->SymbiosisLifetimeBonus;
	Other->LifeLeft = LifeLeft;
	FireCountdown = Settings->SymbiosisGrowTime;

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
		}
		return;
	}

	if (Symbiosis != EVaelSymbiosis::None && Partner.IsValid())
	{
		// The pair acts out of the mixed blossom on top of the shared trunk
		const FVector Middle = GetCrown() + GetCrownAxis() * 25.0f;
		const FVector Way = Goal->GetActorLocation() - Middle;
		VaelSymbiosis::Fire(Gardener, Symbiosis, Plant, Partner->Plant, Middle, Way, Power, 0.0f, bShadow, Way.Size2D());
		if (Knot != nullptr)
		{
			Knot->Flash();
			BendVelocity -= LookDirection * 1.2f;
		}
		return;
	}

	// The shot leaves the crown where it looks
	const FVector Axis = GetCrownAxis();
	const FVector Muzzle = GetCrown() + Axis * (CrownStalks.IsEmpty() ? 20.0f : Plant->FingerLength * 1.1f);
	const FVector Spread = FRotator(0.0f, FMath::FRandRange(-Plant->ShotSpread, Plant->ShotSpread), 0.0f).RotateVector(Goal->GetActorLocation() - Muzzle);
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

void AVaelPlantTower::UpdateLook(float DeltaSeconds)
{
	UpdateBend(DeltaSeconds);

	const FVector Base = GetActorLocation();
	const float Shown = FMath::Clamp(Grow, 0.0f, 1.0f);
	const FVector Crown = GetCrown();
	const float Height = FMath::Max(UVaelFloromantSettings::Get()->TowerHeight * FMath::Sin(Shown * UE_HALF_PI), 1.0f);
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
	const float Bloom = Shown * (1.0f + 0.3f * Gather);

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
		// Aiming, the stalks gather forward like the fingers of the hand
		const float Fan = FMath::Lerp(0.65f, 0.3f, AimBlend);
		const FVector Direction = (Axis * 0.75f + Out * Fan + Out * 0.06f * FMath::Sin(Time * 1.3f + Index)).GetSafeNormal();
		const float Length = Plant->FingerLength * 1.3f * Bloom;
		const FVector End = Crown + Direction * Length;
		USplineMeshComponent* Stalk = CrownStalks[Index];
		Stalk->SetStartAndEnd(Transform.InverseTransformPosition(Crown), Transform.InverseTransformVectorNoScale(Axis * Length),
			Transform.InverseTransformPosition(End), Transform.InverseTransformVectorNoScale(Direction * Length), false);
		Stalk->SetStartScale(FVector2D(Plant->FingerThickness * 1.8f), false);
		Stalk->SetEndScale(FVector2D(Plant->FingerThickness * 1.4f), false);
		Stalk->UpdateMesh();
		if (CrownPieces.IsValidIndex(Index))
		{
			CrownPieces[Index]->SetWorldLocationAndRotation(End, FRotationMatrix::MakeFromZ(Direction).Rotator());
			CrownPieces[Index]->SetWorldScale3D(FVector(Plant->TipSize * 1.6f / 10.0f * Bloom));
		}
	}

	// The big rose of the Rußrose on top, beating with its heart
	if (Plant->bVineArm && CrownPieces.Num() == 1)
	{
		const float Beat = 1.0f + 0.06f * FMath::Square(FMath::Sin(Time * 3.0f));
		CrownPieces[0]->SetWorldLocationAndRotation(Crown - Axis * 6.0f, FRotationMatrix::MakeFromZ(Axis).Rotator());
		CrownPieces[0]->SetWorldScale3D(FVector(Plant->ShoulderRoseSize * 1.6f / 10.0f * Bloom * Beat));
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
