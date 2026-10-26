// Copyright Epic Games, Inc. All Rights Reserved.

#include "Floromant/VaelPoppyCapsule.h"
#include "Combat/VaelCharacterBase.h"
#include "Combat/VaelCombatStatics.h"
#include "Components/PointLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Floromant/VaelGlimmDome.h"
#include "Floromant/VaelPlantArmData.h"
#include "Floromant/VaelRoseHeart.h"
#include "GameFramework/Pawn.h"
#include "Magic/VaelDebrisBurst.h"
#include "Magic/VaelGroundArea.h"
#include "Magic/VaelLightningBolt.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "World/VaelFireComponent.h"
#include "World/VaelFireSpot.h"
#include "World/VaelGround.h"
#include "VaelAssets.h"

namespace
{
	/** Height of the capsule mesh in cm (Tools/Pflanzenarm/funkenmohn_knospe.py) */
	constexpr float CapsuleMeshHeight = 10.0f;

	/** Petals that circle in a Funkenwirbel */
	constexpr int32 WhirlPetalCount = 10;

	/** Sparks that fly out where a capsule bursts */
	void PoppySparks(const UObject* WorldContext, const FVector& Location, float Radius, const FLinearColor& Color, int32 Count)
	{
		FVaelDebris Sparks;
		Sparks.Count = Count;
		Sparks.Spread = 70.0f;
		Sparks.Speed = Radius * 2.6f;
		Sparks.Lift = Radius * 2.2f;
		Sparks.Gravity = 700.0f;
		Sparks.Drag = 1.6f;
		Sparks.Lifetime = 0.9f;
		Sparks.MinSize = 2.0f;
		Sparks.MaxSize = 5.0f;
		Sparks.Color = Color;
		Sparks.Glow = 8.0f;
		Sparks.bLandOnGround = true;
		AVaelDebrisBurst::Spawn(WorldContext, Location, Sparks);
	}

	/** Soft puffs of pollen or embers that drift */
	void PoppyPuffs(const UObject* WorldContext, const FVector& Location, float Radius, const FLinearColor& Color, int32 Count, float Rise, float Glow)
	{
		FVaelDebris Puffs;
		Puffs.Count = Count;
		Puffs.Spread = 80.0f;
		Puffs.Speed = Radius * 0.9f;
		Puffs.Lift = Rise;
		Puffs.Gravity = -30.0f;
		Puffs.Drag = 2.5f;
		Puffs.Lifetime = 1.4f;
		Puffs.MinSize = 8.0f;
		Puffs.MaxSize = 22.0f;
		Puffs.Color = Color;
		Puffs.Glow = Glow;
		Puffs.bSoft = true;
		Puffs.bLandOnGround = false;
		AVaelDebrisBurst::Spawn(WorldContext, Location, Puffs);
	}
}

AVaelPoppyCapsule::AVaelPoppyCapsule()
{
	PrimaryActorTick.bCanEverTick = true;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
}

float AVaelPoppyCapsule::GetFlightTime(float Distance)
{
	return 0.45f + Distance / 1900.0f;
}

AVaelPoppyCapsule* AVaelPoppyCapsule::Throw(APawn* Instigator, const UVaelPlantArmData* InPlant, const FVector& From, const FVector& To, const FVaelPoppyThrow& InThrow, bool bInShadow)
{
	UWorld* World = Instigator != nullptr ? Instigator->GetWorld() : nullptr;
	if (World == nullptr || InPlant == nullptr)
	{
		return nullptr;
	}
	const FTransform SpawnTransform(FRotator(0.0f, FMath::FRandRange(0.0f, 360.0f), 0.0f), From);
	AVaelPoppyCapsule* Capsule = World->SpawnActorDeferred<AVaelPoppyCapsule>(AVaelPoppyCapsule::StaticClass(), SpawnTransform, Instigator, Instigator, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (Capsule == nullptr)
	{
		return nullptr;
	}
	Capsule->Plant = const_cast<UVaelPlantArmData*>(InPlant);
	Capsule->PartnerPlant = const_cast<UVaelPlantArmData*>(InThrow.Partner);
	Capsule->Spec = InThrow;
	Capsule->bShadow = bInShadow;
	Capsule->Start = From;
	Capsule->Goal = Capsule->GroundAt(To);
	const float Distance = FVector::Dist2D(From, Capsule->Goal);
	Capsule->FlightTime = InThrow.Burst == EVaelPoppyBurst::Fuse ? 0.0f : GetFlightTime(Distance);
	Capsule->Apex = 60.0f + Distance * 0.35f;
	Capsule->Phase = InThrow.Delay > 0.0f ? EPhase::Waiting : (Capsule->FlightTime > 0.0f ? EPhase::Flying : EPhase::Landed);
	Capsule->FinishSpawning(SpawnTransform);
	Capsule->BuildLook();
	if (Capsule->Phase == EPhase::Landed)
	{
		Capsule->SetActorLocation(Capsule->Goal);
	}
	Capsule->SetLifeSpan(InThrow.Delay + Capsule->FlightTime + InThrow.Fuse + InThrow.Linger + 6.0f);
	return Capsule;
}

FVector AVaelPoppyCapsule::GroundAt(const FVector& Location) const
{
	FHitResult Hit;
	if (VaelGround::TraceGround(GetWorld(), Location + FVector(0.0f, 0.0f, 400.0f), Location - FVector(0.0f, 0.0f, 800.0f), Hit, this))
	{
		return Hit.ImpactPoint;
	}
	return Location;
}

void AVaelPoppyCapsule::BuildLook()
{
	// The capsule in the plant's own material, glowing like an ember (violet-black as Schwarzmohn)
	Capsule = NewObject<UStaticMeshComponent>(this);
	Capsule->SetMobility(EComponentMobility::Movable);
	UStaticMesh* Mesh = VaelAssets::LoadOptional(Plant->CapsuleMesh);
	Capsule->SetStaticMesh(Mesh != nullptr ? Mesh : LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Sphere.Sphere")));
	Capsule->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Capsule->SetCastShadow(false);
	Capsule->SetupAttachment(RootComponent);
	Capsule->RegisterComponent();
	Material = UMaterialInstanceDynamic::Create(VaelAssets::LoadOptional(Plant->Material), this);
	if (Material != nullptr)
	{
		Material->SetScalarParameterValue(TEXT("Wuchs"), 1.0f);
		Material->SetScalarParameterValue(TEXT("Wind"), 0.0f);
		Material->SetScalarParameterValue(TEXT("Schatten"), bShadow ? 1.0f : 0.0f);
		Capsule->SetMaterial(0, Material);
	}
	const float MeshScale = Mesh != nullptr ? Spec.Size / CapsuleMeshHeight : Spec.Size / 100.0f;
	Capsule->SetRelativeScale3D(FVector(Spec.Burst == EVaelPoppyBurst::Fuse ? 0.01f : MeshScale));
	Capsule->SetVisibility(Phase != EPhase::Waiting);

	const FLinearColor Glow = bShadow ? Plant->SleepColor : Plant->EmberColor;
	Light = NewObject<UPointLightComponent>(this);
	Light->SetupAttachment(RootComponent);
	Light->SetMobility(EComponentMobility::Movable);
	Light->SetIntensityUnits(ELightUnits::Candelas);
	Light->SetIntensity(Phase == EPhase::Waiting ? 0.0f : 6.0f);
	Light->SetLightColor(Glow);
	Light->SetAttenuationRadius(260.0f);
	Light->SetCastShadows(false);
	Light->RegisterComponent();

	// A short tail of flames of the fire kit; the sleep capsule smokes violet instead
	if (!bShadow && Spec.Burst != EVaelPoppyBurst::Fuse)
	{
		Trail = NewObject<UVaelFireComponent>(this);
		Trail->SetupAttachment(RootComponent);
		Trail->RegisterComponent();
		FVaelFireShape Flames;
		Flames.Radius = Spec.Size * 0.35f;
		Flames.Height = Spec.Size * 2.2f;
		Flames.Heat = 1.2f;
		Flames.Smoke = 0.35f;
		Flames.Speed = 2.4f;
		Flames.Tongues = 6;
		Flames.bFloating = true;
		Flames.bLight = false;
		Trail->Ignite(Flames, 0.05f);
		if (Phase == EPhase::Waiting)
		{
			Trail->SetVisibility(false, true);
		}
	}
}

void AVaelPoppyCapsule::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (Plant == nullptr)
	{
		Destroy();
		return;
	}
	PhaseTime += DeltaSeconds;
	const float Time = GetWorld()->GetTimeSeconds();

	switch (Phase)
	{
	case EPhase::Waiting:
	{
		if (PhaseTime < Spec.Delay)
		{
			return;
		}
		Phase = FlightTime > 0.0f ? EPhase::Flying : EPhase::Landed;
		PhaseTime = 0.0f;
		Capsule->SetVisibility(true);
		Light->SetIntensity(6.0f);
		if (Trail != nullptr)
		{
			Trail->SetVisibility(true, true);
		}
		if (Phase == EPhase::Landed)
		{
			SetActorLocation(Goal);
		}
		return;
	}
	case EPhase::Flying:
	{
		// A high arc to the goal, tumbling
		const float Share = FMath::Clamp(PhaseTime / FlightTime, 0.0f, 1.0f);
		const FVector Here = FMath::Lerp(Start, Goal, Share) + FVector(0.0f, 0.0f, 4.0f * Apex * Share * (1.0f - Share));
		const FVector Way = Here - GetActorLocation();
		SetActorLocation(Here);
		Capsule->SetRelativeRotation(FRotator(Time * 420.0f, Time * 260.0f, Time * 170.0f));
		if (Trail != nullptr && !Way.IsNearlyZero())
		{
			Trail->SetFlowDirection(-Way.GetSafeNormal() + FVector(0.0f, 0.0f, 0.3f));
		}
		if (bShadow && FMath::FRand() < DeltaSeconds * 12.0f)
		{
			PoppyPuffs(this, Here, 30.0f, Plant->SleepColor * 0.5f, 1, 10.0f, 1.0f);
		}

		// Rooted throws break up in the air into several capsules
		if (Spec.Split > 1 && !bSplit && Share >= 0.45f)
		{
			SplitUp();
			return;
		}
		if (Share >= 1.0f)
		{
			Phase = EPhase::Landed;
			PhaseTime = 0.0f;
			Capsule->SetRelativeRotation(FRotator(0.0f, FMath::FRandRange(0.0f, 360.0f), FMath::FRandRange(-25.0f, 25.0f)));
			if (Trail != nullptr)
			{
				Trail->Extinguish(0.3f, true);
				Trail = nullptr;
			}
			FVaelDebris Earth;
			Earth.Count = 6;
			Earth.Speed = 120.0f;
			Earth.Lift = 160.0f;
			Earth.Lifetime = 0.6f;
			Earth.MinSize = 2.0f;
			Earth.MaxSize = 5.0f;
			Earth.Color = FLinearColor(0.16f, 0.12f, 0.09f);
			AVaelDebrisBurst::Spawn(this, Goal, Earth);
		}
		return;
	}
	case EPhase::Landed:
	{
		// It glows and swells faster and faster; a Zündschnur poppy grows out of the ground first
		const float Share = Spec.Fuse > 0.0f ? FMath::Clamp(PhaseTime / Spec.Fuse, 0.0f, 1.0f) : 1.0f;
		const float MeshScale = Spec.Size / CapsuleMeshHeight;
		const float Pulse = 1.0f + 0.12f * Share * FMath::Sin(Time * (14.0f + 30.0f * Share));
		const float Sprout = Spec.Burst == EVaelPoppyBurst::Fuse ? FMath::SmoothStep(0.0f, 0.6f, Share) : 1.0f;
		Capsule->SetRelativeScale3D(FVector(MeshScale * Pulse * FMath::Max(Sprout, 0.01f)));
		if (Material != nullptr)
		{
			Material->SetScalarParameterValue(TEXT("Aufleuchten"), Share * 0.4f);
		}
		Light->SetIntensity(6.0f + 30.0f * Share);
		if (Share >= 1.0f)
		{
			if (Burst())
			{
				Phase = EPhase::Lingering;
				PhaseTime = 0.0f;
				LingerLeft = Spec.Linger;
			}
			else
			{
				Finish();
			}
		}
		return;
	}
	case EPhase::Lingering:
		Linger(DeltaSeconds);
		return;
	}
}

void AVaelPoppyCapsule::SplitUp()
{
	bSplit = true;
	APawn* Thrower = GetInstigator();
	const FVector Here = GetActorLocation();
	for (int32 Index = 0; Index < Spec.Split; ++Index)
	{
		FVaelPoppyThrow Part = Spec;
		Part.Split = 0;
		Part.Delay = 0.0f;
		Part.Size *= 0.75f;
		Part.Radius *= 0.75f;
		Part.Damage *= 0.65f;
		const float Angle = 2.0f * UE_PI * Index / Spec.Split + FMath::FRandRange(-0.3f, 0.3f);
		const FVector Spot = Index == 0 ? Goal : Goal + FVector(FMath::Cos(Angle), FMath::Sin(Angle), 0.0f) * Spec.Radius * 1.3f;
		if (AVaelPoppyCapsule* Piece = Throw(Thrower, Plant, Here, Spot, Part, bShadow))
		{
			// Already high in the air: only a short drop left
			Piece->FlightTime = FMath::Max(FlightTime * 0.55f, 0.2f);
			Piece->Apex = 30.0f;
		}
	}
	PoppySparks(this, Here, 40.0f, bShadow ? Plant->SleepColor : Plant->EmberColor, 10);
	Finish();
}

void AVaelPoppyCapsule::SparkBurst(const FVector& Location, float InRadius, float InDamage, bool bBig)
{
	APawn* Thrower = GetInstigator();
	AVaelFireSpot::SpawnBurst(GetWorld(), Location, InRadius * 0.7f);
	PoppySparks(this, Location + FVector(0.0f, 0.0f, 15.0f), InRadius, Plant->EmberColor, bBig ? 40 : 22);
	if (Thrower != nullptr && InDamage > 0.0f)
	{
		FVaelSpellHit Hit;
		Hit.Damage = InDamage;
		Hit.Element = EVaelElement::Fire;
		Hit.Status = EVaelStatus::Burning;
		Hit.StatusDuration = 3.0f;
		UVaelCombatStatics::ApplySpellHitInRadius(Thrower, Location, InRadius, Hit);
	}
}

bool AVaelPoppyCapsule::Burst()
{
	APawn* Thrower = GetInstigator();
	const FVector Here = Goal;
	Capsule->SetVisibility(false);

	// Schwarzmohn: the capsule breaks into a cloud of sleep pollen instead of fire
	if (bShadow && Spec.Burst == EVaelPoppyBurst::Sparks)
	{
		PoppyPuffs(this, Here + FVector(0.0f, 0.0f, 20.0f), Spec.Radius, Plant->SleepColor, 18, 40.0f, 1.5f);
		PoppySparks(this, Here, Spec.Radius * 0.6f, FLinearColor(0.08f, 0.03f, 0.12f), 14);
		Light->SetIntensity(8.0f);
		Spec.Burst = EVaelPoppyBurst::Sleep;
		Spec.Linger = Plant->SleepCloudSeconds;
		return true;
	}

	switch (Spec.Burst)
	{
	case EVaelPoppyBurst::Sparks:
	case EVaelPoppyBurst::Fuse:
	{
		SparkBurst(Here, Spec.Radius, Spec.Damage, false);
		if (Spec.PatchSeconds > 0.0f && Thrower != nullptr)
		{
			AVaelGroundArea::SpawnArea(GetWorld(), Here, EVaelElement::Fire, Spec.Radius * 0.55f, Spec.PatchSeconds, Spec.PatchDamagePerSecond, Thrower);
		}
		return false;
	}
	case EVaelPoppyBurst::Firework:
	{
		// A small burst, and the little capsules fly out in every direction and burst in turn, crackling
		SparkBurst(Here, Spec.Radius * 0.6f, Spec.Damage * 0.5f, true);
		for (int32 Index = 0; Index < Spec.Scatter; ++Index)
		{
			FVaelPoppyThrow Small;
			Small.Burst = EVaelPoppyBurst::Sparks;
			Small.Size = Spec.Size * 0.45f;
			Small.Radius = Spec.Radius * 0.55f;
			Small.Damage = Spec.Damage * 0.45f;
			Small.Fuse = FMath::FRandRange(0.05f, 0.35f);
			Small.PatchSeconds = Index % 3 == 0 ? Spec.PatchSeconds * 0.5f : 0.0f;
			Small.PatchDamagePerSecond = Spec.PatchDamagePerSecond;
			const float Angle = 2.0f * UE_PI * Index / FMath::Max(Spec.Scatter, 1) + FMath::FRandRange(-0.25f, 0.25f);
			const FVector Spot = Here + FVector(FMath::Cos(Angle), FMath::Sin(Angle), 0.0f) * Spec.Radius * FMath::FRandRange(1.1f, 2.2f);
			if (AVaelPoppyCapsule* Piece = Throw(Thrower, Plant, Here + FVector(0.0f, 0.0f, 30.0f), Spot, Small, false))
			{
				Piece->FlightTime = FMath::FRandRange(0.35f, 0.6f);
				Piece->Apex = FMath::FRandRange(80.0f, 160.0f);
			}
		}
		return false;
	}
	case EVaelPoppyBurst::Whirl:
	{
		// A column of flames that turns, with burning petals circling it
		SparkBurst(Here, Spec.Radius * 0.7f, Spec.Damage * 0.6f, false);
		Column = NewObject<UVaelFireComponent>(this);
		Column->SetupAttachment(RootComponent);
		Column->RegisterComponent();
		Column->SetWorldLocation(Here);
		FVaelFireShape Flames;
		Flames.Radius = Spec.Radius * 0.3f;
		Flames.Height = Spec.Radius * 1.8f;
		Flames.Heat = 1.35f;
		Flames.Smoke = 0.4f;
		Flames.Speed = 2.6f;
		Flames.Tongues = 12;
		Flames.bLight = false;
		Column->Ignite(Flames, 0.25f);
		if (UStaticMesh* PetalMesh = VaelAssets::LoadOptional(Plant->PetalMesh))
		{
			for (int32 Index = 0; Index < WhirlPetalCount; ++Index)
			{
				UStaticMeshComponent* Petal = NewObject<UStaticMeshComponent>(this);
				Petal->SetMobility(EComponentMobility::Movable);
				Petal->SetStaticMesh(PetalMesh);
				Petal->SetMaterial(0, Material);
				Petal->SetCollisionEnabled(ECollisionEnabled::NoCollision);
				Petal->SetCastShadow(false);
				Petal->SetupAttachment(RootComponent);
				Petal->RegisterComponent();
				WhirlPetals.Add(Petal);
			}
		}
		return true;
	}
	case EVaelPoppyBurst::Storm:
	{
		// Rain bursts out of it and soaks everything around; the sparks follow a moment later as lightning
		PoppySparks(this, Here + FVector(0.0f, 0.0f, 20.0f), Spec.Radius, FLinearColor(0.35f, 0.75f, 1.0f), 30);
		if (Thrower != nullptr)
		{
			FVaelSpellHit Soak;
			Soak.Damage = Spec.Damage * 0.3f;
			Soak.Element = EVaelElement::Water;
			Soak.Status = EVaelStatus::Wet;
			Soak.StatusDuration = 6.0f;
			UVaelCombatStatics::ApplySpellHitInRadius(Thrower, Here, Spec.Radius * 1.4f, Soak);
		}
		Light->SetLightColor(FLinearColor(0.5f, 0.75f, 1.0f));
		return true;
	}
	case EVaelPoppyBurst::Bubble:
	{
		// A glass bubble rises around the spot: the enemies inside are shut in while the sparks whirl
		Light->SetIntensity(20.0f);
		if (PartnerPlant != nullptr && Thrower != nullptr)
		{
			Bubble = AVaelGlimmDome::Raise(Thrower, this, PartnerPlant, EVaelSymbiosis::Glutblase, Spec.Radius * 1.1f, bShadow);
		}
		for (TActorIterator<AVaelCharacterBase> It(GetWorld()); It; ++It)
		{
			if (UVaelCombatStatics::CanDamage(Thrower, *It) && FVector::Dist2D(It->GetActorLocation(), Here) < Spec.Radius * 1.1f)
			{
				It->ApplyStun(Spec.Linger);
			}
		}
		return true;
	}
	case EVaelPoppyBurst::Salve:
	{
		// Embers that burn enemies and close the wounds of allies
		SparkBurst(Here, Spec.Radius * 0.6f, Spec.Damage * 0.5f, false);
		if (Thrower != nullptr)
		{
			AVaelGroundArea::SpawnArea(GetWorld(), Here, EVaelElement::Fire, Spec.Radius, Spec.Linger, Spec.PatchDamagePerSecond, Thrower);
		}
		Light->SetLightColor(FLinearColor(1.0f, 0.75f, 0.35f));
		return true;
	}
	default:
		return false;
	}
}

void AVaelPoppyCapsule::Linger(float DeltaSeconds)
{
	APawn* Thrower = GetInstigator();
	const FVector Here = Goal;
	LingerLeft -= DeltaSeconds;
	PulseCountdown -= DeltaSeconds;
	const bool bPulse = PulseCountdown <= 0.0f;
	if (bPulse)
	{
		PulseCountdown = 0.25f;
	}
	const float Time = GetWorld()->GetTimeSeconds();

	switch (Spec.Burst)
	{
	case EVaelPoppyBurst::Sleep:
	{
		// Who breathes the pollen grows slow, and after a while falls asleep
		if (bPulse)
		{
			PoppyPuffs(this, Here + FVector(FMath::FRandRange(-1.0f, 1.0f), FMath::FRandRange(-1.0f, 1.0f), 0.3f) * Spec.Radius * 0.6f, Spec.Radius * 0.5f, Plant->SleepColor, 3, 15.0f, 1.2f);
			for (TActorIterator<AVaelCharacterBase> It(GetWorld()); It; ++It)
			{
				if (!UVaelCombatStatics::CanDamage(Thrower, *It) || FVector::Dist2D(It->GetActorLocation(), Here) > Spec.Radius)
				{
					continue;
				}
				float& Seconds = Breathed.FindOrAdd(*It);
				if (Seconds < 0.0f)
				{
					continue;
				}
				Seconds += 0.25f;
				UVaelCombatStatics::ApplyStatus(Thrower, *It, EVaelStatus::Slowed, 0.6f);
				if (Seconds >= Spec.Drowsy)
				{
					It->ApplySleep(Spec.Sleep);
					PoppyPuffs(this, It->GetActorLocation() + FVector(0.0f, 0.0f, 60.0f), 30.0f, Plant->SleepColor, 4, 30.0f, 2.0f);
					// Once asleep it isn't put to sleep again by the same cloud
					Seconds = -1.0f;
				}
			}
		}
		Light->SetIntensity(8.0f * FMath::Clamp(LingerLeft, 0.0f, 1.0f));
		break;
	}
	case EVaelPoppyBurst::Whirl:
	{
		// It draws enemies in and burns them; the petals spiral up round the flames
		for (TActorIterator<AVaelCharacterBase> It(GetWorld()); It; ++It)
		{
			if (UVaelCombatStatics::CanDamage(Thrower, *It) && FVector::Dist2D(It->GetActorLocation(), Here) < Spec.Radius * 1.5f)
			{
				It->ApplyPull(Here, 260.0f, DeltaSeconds);
			}
		}
		if (bPulse && Thrower != nullptr)
		{
			FVaelSpellHit Burn;
			Burn.Damage = Spec.Damage * 0.12f;
			Burn.Element = EVaelElement::Fire;
			Burn.Status = EVaelStatus::Burning;
			Burn.StatusDuration = 2.0f;
			UVaelCombatStatics::ApplySpellHitInRadius(Thrower, Here, Spec.Radius * 0.6f, Burn);
		}
		if (Column != nullptr)
		{
			Column->SetFlowDirection(FVector(FMath::Cos(Time * 6.0f) * 0.25f, FMath::Sin(Time * 6.0f) * 0.25f, 1.0f));
		}
		for (int32 Index = 0; Index < WhirlPetals.Num(); ++Index)
		{
			const float Share = FMath::Frac(Index / static_cast<float>(WhirlPetals.Num()) + Time * 0.35f);
			const float Angle = Time * 7.0f + Index * 2.4f;
			const float Reach = Spec.Radius * FMath::Lerp(0.35f, 0.75f, Share);
			const FVector Spot = Here + FVector(FMath::Cos(Angle) * Reach, FMath::Sin(Angle) * Reach, Spec.Radius * 1.6f * Share);
			WhirlPetals[Index]->SetWorldLocationAndRotation(Spot, FRotator(Time * 300.0f + Index * 40.0f, FMath::RadiansToDegrees(Angle), 70.0f));
			WhirlPetals[Index]->SetWorldScale3D(FVector(1.2f * FMath::Sin(Share * UE_PI) + 0.2f));
		}
		if (Material != nullptr)
		{
			Material->SetScalarParameterValue(TEXT("Aufleuchten"), 0.6f + 0.3f * FMath::Sin(Time * 9.0f));
		}
		if (LingerLeft < 0.6f && Column != nullptr)
		{
			Column->Extinguish(0.6f, true);
			Column = nullptr;
		}
		break;
	}
	case EVaelPoppyBurst::Storm:
	{
		if (!bLightningDone && PhaseTime > 0.3f)
		{
			ChainLightning();
			bLightningDone = true;
		}
		Light->SetIntensity(bLightningDone ? 4.0f * FMath::Max(LingerLeft, 0.0f) : 10.0f);
		break;
	}
	case EVaelPoppyBurst::Bubble:
	{
		// The enemies stay shut in; the sparks swirl inside the glass until it bursts
		for (TActorIterator<AVaelCharacterBase> It(GetWorld()); It; ++It)
		{
			const float Distance = FVector::Dist2D(It->GetActorLocation(), Here);
			if (UVaelCombatStatics::CanDamage(Thrower, *It) && Distance < Spec.Radius * 1.4f && Distance > Spec.Radius * 0.6f)
			{
				It->ApplyPull(Here, 300.0f, DeltaSeconds);
			}
		}
		if (bPulse)
		{
			FVaelDebris Swirl;
			Swirl.Count = 8;
			Swirl.Spread = 180.0f;
			Swirl.Speed = Spec.Radius * 1.4f;
			Swirl.Lift = Spec.Radius;
			Swirl.Gravity = 200.0f;
			Swirl.Drag = 2.0f;
			Swirl.Lifetime = 0.5f;
			Swirl.MinSize = 2.0f;
			Swirl.MaxSize = 4.0f;
			Swirl.Color = Plant->EmberColor;
			Swirl.Glow = 8.0f;
			Swirl.bLandOnGround = false;
			AVaelDebrisBurst::Spawn(this, Here + FVector(0.0f, 0.0f, Spec.Radius * 0.4f), Swirl);
		}
		Light->SetIntensity(20.0f + 15.0f * FMath::Sin(Time * 20.0f));
		if (LingerLeft <= 0.0f)
		{
			if (AVaelGlimmDome* Glass = Bubble.Get())
			{
				Glass->Lower();
			}
			SparkBurst(Here, Spec.Radius * 1.25f, Spec.Damage * 1.6f, true);
		}
		break;
	}
	case EVaelPoppyBurst::Salve:
	{
		// Every half second the allies in the embers heal a little; embers drift up and petals settle
		if (bPulse && FMath::Fmod(PhaseTime, 0.5f) < 0.26f)
		{
			VaelRoseCare::HealAround(this, Here, Spec.Radius, 0.02f);
			PoppyPuffs(this, Here + FVector(FMath::FRandRange(-1.0f, 1.0f), FMath::FRandRange(-1.0f, 1.0f), 0.0f) * Spec.Radius * 0.6f, Spec.Radius * 0.3f, FLinearColor(1.0f, 0.6f, 0.2f), 3, 120.0f, 4.0f);
			if (FMath::FRand() < 0.35f)
			{
				VaelRoseCare::Petals(this, Here + FVector(0.0f, 0.0f, 200.0f), Here, false, 4);
			}
		}
		Light->SetIntensity(10.0f * FMath::Clamp(LingerLeft, 0.0f, 1.0f));
		break;
	}
	default:
		break;
	}

	if (LingerLeft <= 0.0f)
	{
		Finish();
	}
}

void AVaelPoppyCapsule::ChainLightning()
{
	APawn* Thrower = GetInstigator();
	const FVector Here = Goal + FVector(0.0f, 0.0f, 60.0f);
	const FLinearColor Color(0.6f, 0.8f, 1.0f);

	// The wet enemies around, nearest first; the lightning leaps from the burst to one after the other
	TArray<AVaelCharacterBase*> Wet;
	for (TActorIterator<AVaelCharacterBase> It(GetWorld()); It; ++It)
	{
		if (UVaelCombatStatics::CanDamage(Thrower, *It) && FVector::Dist2D(It->GetActorLocation(), Goal) < Spec.Radius * 2.5f && UVaelCombatStatics::HasStatus(*It, EVaelStatus::Wet))
		{
			Wet.Add(*It);
		}
	}
	Wet.Sort([this](const AVaelCharacterBase& A, const AVaelCharacterBase& B)
	{
		return FVector::DistSquared2D(A.GetActorLocation(), Goal) < FVector::DistSquared2D(B.GetActorLocation(), Goal);
	});

	FVaelSpellHit Bolt;
	Bolt.Damage = Spec.Damage;
	Bolt.Element = EVaelElement::Air;
	Bolt.bLightning = true;
	FVector From = Here;
	for (int32 Index = 0; Index < FMath::Min(Wet.Num(), 6); ++Index)
	{
		const FVector To = Wet[Index]->GetActorLocation();
		AVaelLightningBolt::Spawn(this, From, To, Color, 0.3f, 1.4f, true);
		if (Thrower != nullptr)
		{
			UVaelCombatStatics::ApplySpellHit(Thrower, Wet[Index], Bolt, (To - From).GetSafeNormal2D());
		}
		From = To;
	}

	// Without anyone wet the sparks still crackle into the ground around
	if (Wet.IsEmpty())
	{
		for (int32 Index = 0; Index < 3; ++Index)
		{
			const float Angle = FMath::FRandRange(0.0f, 2.0f * UE_PI);
			AVaelLightningBolt::Spawn(this, Here, Goal + FVector(FMath::Cos(Angle), FMath::Sin(Angle), 0.0f) * Spec.Radius, Color, 0.25f, 1.0f, true);
		}
	}
}

void AVaelPoppyCapsule::Finish()
{
	if (Trail != nullptr)
	{
		Trail->Extinguish(0.3f, true);
		Trail = nullptr;
	}
	if (Column != nullptr)
	{
		Column->Extinguish(0.5f, true);
		Column = nullptr;
	}
	if (AVaelGlimmDome* Glass = Bubble.Get())
	{
		Glass->Lower();
	}
	Destroy();
}
