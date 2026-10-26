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

	/** Any direction across the given one */
	FVector TowerAcross(const FVector& Direction)
	{
		const FVector Side = FVector::CrossProduct(Direction, FVector::UpVector);
		return Side.IsNearlyZero() ? FVector::ForwardVector : Side.GetSafeNormal();
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

FVector AVaelPlantTower::GetCrown() const
{
	const float Height = UVaelFloromantSettings::Get()->TowerHeight * FMath::Sin(FMath::Clamp(Grow, 0.0f, 1.0f) * UE_HALF_PI);
	return GetActorLocation() + FVector(0.0f, 0.0f, Height);
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
			if (Goal != nullptr)
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
		FuseGrow = bWithering ? FMath::Min(FuseGrow, Grow) : FMath::Min(1.0f, FuseGrow + DeltaSeconds / UVaelFloromantSettings::Get()->SymbiosisGrowTime);
		const FVector Middle = (GetCrown() + Partner->GetCrown()) * 0.5f - FVector(0.0f, 0.0f, 35.0f);
		Knot->Update(GetCrown() - FVector(0.0f, 0.0f, 30.0f), Partner->GetCrown() - FVector(0.0f, 0.0f, 30.0f), Middle, FuseGrow);
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
		const FVector Middle = (GetCrown() + Partner->GetCrown()) * 0.5f - FVector(0.0f, 0.0f, 35.0f);
		const float Hotter = Symbiosis == EVaelSymbiosis::Glutglas ? 1.5f : 1.0f;
		VaelSymbiosis::FireShot(Gardener, Shooter, Middle, (Goal->GetActorLocation() - Middle).GetSafeNormal(), Power * Hotter, bShadow, UVaelFloromantSettings::Get()->TowerShotScale, 1.0f, FVector::Dist2D(Middle, Goal->GetActorLocation()));
		if (Symbiosis == EVaelSymbiosis::Aschenrose && !bShadow)
		{
			VaelRoseCare::HealAround(Gardener, GetActorLocation(), 600.0f, 0.02f * Power);
		}
		if (Knot != nullptr)
		{
			Knot->Flash();
		}
		return;
	}

	if (Symbiosis != EVaelSymbiosis::None && Partner.IsValid())
	{
		// The pair acts out of the knot between both towers
		const FVector Middle = (GetCrown() + Partner->GetCrown()) * 0.5f - FVector(0.0f, 0.0f, 35.0f);
		const FVector Way = Goal->GetActorLocation() - Middle;
		VaelSymbiosis::Fire(Gardener, Symbiosis, Plant, Partner->Plant, Middle, Way, Power, 0.0f, bShadow, Way.Size2D());
		if (Knot != nullptr)
		{
			Knot->Flash();
		}
		return;
	}

	const FVector Muzzle = GetCrown() + FVector(0.0f, 0.0f, 15.0f) + LookDirection * 20.0f;
	const FVector Spread = FRotator(0.0f, FMath::FRandRange(-Plant->ShotSpread, Plant->ShotSpread), 0.0f).RotateVector(Goal->GetActorLocation() - Muzzle);
	const UVaelFloromantSettings* Settings = UVaelFloromantSettings::Get();
	// A Mohnmörser throws its capsule to where the enemy stands
	VaelSymbiosis::FireShot(Gardener, Plant, Muzzle, Spread.GetSafeNormal(), Power, bShadow, Settings->TowerShotScale, 1.0f, Spread.Size2D());
}

void AVaelPlantTower::UpdateLook(float DeltaSeconds)
{
	const UVaelFloromantSettings* Settings = UVaelFloromantSettings::Get();
	const FVector Base = GetActorLocation();
	const float Shown = FMath::Clamp(Grow, 0.0f, 1.0f);
	const FVector Crown = GetCrown();
	const float Height = FMath::Max(Crown.Z - Base.Z, 1.0f);
	const float Time = GetWorld()->GetTimeSeconds();

	// The crown turns a little towards what it fires at
	if (const AActor* Goal = Target.Get())
	{
		const FVector Way = (Goal->GetActorLocation() - Crown).GetSafeNormal2D();
		if (!Way.IsNearlyZero())
		{
			LookDirection = FMath::VInterpNormalRotationTo(LookDirection, Way, DeltaSeconds, 120.0f);
		}
	}
	const FVector Axis = (FVector::UpVector + LookDirection * 0.3f).GetSafeNormal();
	const FVector U = TowerAcross(Axis);
	const FVector V = FVector::CrossProduct(Axis, U);

	// Column: the vines wind round each other up to the crown
	const FTransform& Transform = GetActorTransform();
	for (int32 Index = 0; Index < Column.Num(); ++Index)
	{
		const float A0 = Index * 2.0f * UE_PI / Column.Num() + GetUniqueID() * 0.3f;
		const float A1 = A0 + UE_PI * 0.8f;
		const FVector Out0(FMath::Cos(A0), FMath::Sin(A0), 0.0f);
		const FVector Out1(FMath::Cos(A1), FMath::Sin(A1), 0.0f);
		const FVector Around0(-FMath::Sin(A0), FMath::Cos(A0), 0.0f);
		const FVector Around1(-FMath::Sin(A1), FMath::Cos(A1), 0.0f);
		const FVector Start = Base + Out0 * 16.0f - FVector(0.0f, 0.0f, 6.0f);
		const FVector End = Crown + Out1 * 7.0f;
		USplineMeshComponent* Vine = Column[Index];
		Vine->SetStartAndEnd(Transform.InverseTransformPosition(Start), Transform.InverseTransformVectorNoScale(FVector::UpVector * Height * 1.1f + Around0 * Height * 0.5f),
			Transform.InverseTransformPosition(End), Transform.InverseTransformVectorNoScale(FVector::UpVector * Height * 0.9f + Around1 * Height * 0.4f), false);
		Vine->SetStartScale(FVector2D(3.0f), false);
		Vine->SetEndScale(FVector2D(1.8f), false);
		Vine->SetStartRoll(A0, false);
		Vine->SetEndRoll(A1 + 2.0f, false);
		Vine->UpdateMesh();
	}

	// Crown of stalks: they fan up and out of the top, each ending in its head
	for (int32 Index = 0; Index < CrownStalks.Num(); ++Index)
	{
		const float Angle = Index * 2.0f * UE_PI / CrownStalks.Num() + 0.4f;
		const FVector Out = U * FMath::Cos(Angle) + V * FMath::Sin(Angle);
		const FVector Direction = (Axis * 0.75f + Out * 0.65f + Out * 0.06f * FMath::Sin(Time * 1.3f + Index)).GetSafeNormal();
		const float Length = Plant->FingerLength * 1.3f * Shown;
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
			CrownPieces[Index]->SetWorldScale3D(FVector(Plant->TipSize * 1.6f / 10.0f * Shown));
		}
	}

	// The big rose of the Rußrose on top, beating with its heart
	if (Plant->bVineArm && CrownPieces.Num() == 1)
	{
		const float Beat = 1.0f + 0.06f * FMath::Square(FMath::Sin(Time * 3.0f));
		CrownPieces[0]->SetWorldLocationAndRotation(Crown - Axis * 6.0f, FRotationMatrix::MakeFromZ(Axis).Rotator());
		CrownPieces[0]->SetWorldScale3D(FVector(Plant->ShoulderRoseSize * 1.6f / 10.0f * Shown * Beat));
	}
	// Crown of petals: the rose stands open on top, breathing, the inner ring half shut
	else if (CrownStalks.IsEmpty())
	{
		for (int32 Index = 0; Index < CrownPieces.Num(); ++Index)
		{
			const bool bInner = Index < Plant->InnerPetals;
			const int32 Count = bInner ? Plant->InnerPetals : Plant->OuterPetals;
			const int32 InRing = bInner ? Index : Index - Plant->InnerPetals;
			const float Angle = 2.0f * UE_PI * (InRing + (bInner ? 0.0f : 0.5f)) / FMath::Max(Count, 1);
			const float Size = Plant->PetalSize * 1.8f * (bInner ? 0.72f : 1.0f);
			const float Lean = FMath::DegreesToRadians((bInner ? 15.0f : Plant->RoseOpenAngle * 0.8f) + 4.0f * FMath::Sin(Time * 1.5f + Index));
			const FVector Out = U * FMath::Cos(Angle) + V * FMath::Sin(Angle);
			const FVector Up = (Axis * FMath::Cos(Lean) + Out * FMath::Sin(Lean)).GetSafeNormal();
			const FVector Outside = (Out - Up * FVector::DotProduct(Out, Up)).GetSafeNormal();
			const FVector Foot = Crown + Out * (bInner ? 6.0f : 11.0f) * Shown;
			CrownPieces[Index]->SetWorldLocationAndRotation(Foot, FRotationMatrix::MakeFromXZ(Outside, Up).Rotator());
			CrownPieces[Index]->SetWorldScale3D(FVector(Size / 10.0f * Shown));
		}
	}
}
