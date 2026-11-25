// Copyright Epic Games, Inc. All Rights Reserved.

#include "Engineer/VaelEngineerDevice.h"

#include "Combat/VaelCharacterBase.h"
#include "Combat/VaelCombatStatics.h"
#include "Creatures/VaelCreature.h"
#include "Creatures/VaelAshRoller.h"
#include "Magic/VaelEnvironmentStatics.h"
#include "World/VaelFireSpot.h"
#include "World/VaelLocalWeather.h"
#include "World/VaelRegion.h"
#include "World/VaelTimeOfDay.h"
#include "World/VaelBalance.h"
#include "Magic/VaelMagicSettings.h"
#include "Magic/VaelSpellEffects.h"
#include "NiagaraSystem.h"
#include "VaelAssets.h"
#include "Components/BoxComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engineer/VaelEngineerComponent.h"
#include "Engineer/VaelEngineerShot.h"
#include "EngineUtils.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Player/VaelCharacter.h"
#include "UI/VaelCombatTextSubsystem.h"
#include "UObject/ConstructorHelpers.h"
#include "Vael.h"

#define LOCTEXT_NAMESPACE "VaelEngineerDevice"

namespace
{
	constexpr float TrapRadius = 110.0f;
	constexpr float MineRadius = 340.0f;
	constexpr float MineDamage = 80.0f;
	constexpr float TurretRange = 1200.0f;
	constexpr float TurretInterval = 1.0f;
	constexpr float TurretLife = 25.0f;
	/** Grabmine: armed after this, then whoever steps this close sets it off */
	constexpr float BuriedArmSeconds = 0.8f;
	constexpr float BuriedRadius = 120.0f;
	/** Rauchtopf: how long the smoke stands and how far it reaches */
	constexpr float SmokeLife = 8.0f;
	constexpr float SmokeRadius = 420.0f;
	/** By fire a mine bursts this much harder and wider; on wet ground its steam blinds this long */
	constexpr float FireMineDamage = 1.5f;
	constexpr float FireMineRadius = 1.3f;
	constexpr float SteamConfused = 2.5f;
	/** Wind drives the smoke this share of its speed (at most this fast) and spreads it this much wider; by fire it smoulders */
	constexpr float SmokeDrift = 0.35f;
	constexpr float SmokeDriftMax = 220.0f;
	constexpr float SmokeWindSpread = 1.25f;
	constexpr float SmokeFireDps = 4.0f;
	/** Glutzapfer: how long it taps, how far and how often it spits, and how hard */
	constexpr float TapperLife = 15.0f;
	constexpr float TapperRange = 650.0f;
	constexpr float TapperInterval = 0.8f;
	constexpr float TapperDamage = 14.0f;
	/** Säurefass: steps that set it off, the pool, its bite */
	constexpr float AcidTrigger = 110.0f;
	constexpr float AcidPoolRadius = 320.0f;
	constexpr float AcidPoolSeconds = 8.0f;
	constexpr float AcidDps = 6.0f;
	/** Löschpulver: how far it reaches, how long it keeps them cool */
	constexpr float DouseRadius = 450.0f;
	constexpr float DouseWet = 6.0f;
	/** The pouch drops this far in this long before it bursts */
	constexpr float DouseDrop = 160.0f;
	constexpr float DouseFall = 0.22f;
	/** A device rises out of the ground this long when laid (the Glutzapfer, the Säurefass) */
	constexpr float RiseSeconds = 0.35f;
	/** Sichelkreisel: wind-up, pulse, reach, cut; it whirls 4 s after the wind-up */
	constexpr float WhirlWindUp = 0.4f;
	constexpr float WhirlPulse = 0.4f;
	constexpr float WhirlRadius = 190.0f;
	constexpr float WhirlDamage = 10.0f;
	constexpr float WhirlLife = 4.4f;
	/** Würgnetz: arming, trigger, reach, hold, the bite when they tear free, the pull to shadow */
	constexpr float NetArm = 0.6f;
	constexpr float NetTrigger = 150.0f;
	constexpr float NetRadius = 280.0f;
	constexpr float NetHold = 2.5f;
	constexpr float NetTearDamage = 15.0f;
	constexpr float NetShadowPull = 0.5f;
	/** Blendlaterne: reach (30 % more at night), flash every, blind for, life */
	constexpr float LanternRadius = 550.0f;
	constexpr float LanternPulse = 3.0f;
	constexpr float LanternBlind = 2.5f;
	constexpr float LanternLife = 30.0f;
	/** Sturmwarner: who it warns, how long they stay marked, the gust in wind, life */
	constexpr float WarnerRadius = 1200.0f;
	constexpr float WarnerMark = 4.0f;
	constexpr float WarnerGustEvery = 2.0f;
	constexpr float WarnerGustRadius = 500.0f;
	constexpr float WarnerLife = 60.0f;
	/** Rollwall: speed, size of the rolled ball, who it bumps, how hard, how long the wall stands, its size */
	constexpr float RollSpeed = 850.0f;
	constexpr float RollBallRadius = 40.0f;
	constexpr float RollHitRadius = 110.0f;
	constexpr float RollDamage = 10.0f;
	constexpr float WallSeconds = 10.0f;
	constexpr float WallHalfWidth = 130.0f;
	constexpr float WallHeight = 160.0f;
	/** Rammschild: its size, how long it stands, who it reaches, how fast one must come to bounce off, what that costs, the
	 *  pricks of its spikes, and how often the same enemy is struck */
	constexpr float RamHalfWidth = 90.0f;
	constexpr float RamHeight = 125.0f;
	constexpr float RamLife = 25.0f;
	constexpr float RamReach = 150.0f;
	constexpr float RamTouch = 95.0f;
	constexpr float RamChargeSpeed = 280.0f;
	constexpr float RamBounceDamage = 22.0f;
	constexpr float RamStun = 1.6f;
	constexpr float RamSpikeDamage = 5.0f;
	constexpr float RamCooldown = 1.0f;
	/** Stolperdraht: arming, half its length, the blast where it is crossed */
	constexpr float TripArm = 0.8f;
	constexpr float TripHalfLength = 200.0f;
	constexpr float TripRadius = 220.0f;
	constexpr float TripDamage = 30.0f;
	constexpr float TripLife = 120.0f;
	/** Kristallladung: fuse, blast, damage */
	constexpr float OreFuse = 1.5f;
	constexpr float OreRadius = 260.0f;
	constexpr float OreDamage = 26.0f;

	/** The moving part of a device that has one, and how high it sits on the base */
	const TCHAR* PartOf(EVaelDeviceType Type)
	{
		switch (Type)
		{
		case EVaelDeviceType::Tapper: return TEXT("Glutzapfer_Duese");
		case EVaelDeviceType::Acid: return TEXT("Saeurefass_Deckel");
		case EVaelDeviceType::Whirl: return TEXT("Sichelkreisel_Kreisel");
		case EVaelDeviceType::Net: return TEXT("Wuergnetz_Netz");
		case EVaelDeviceType::Warner: return TEXT("Sturmwarner_Fahne");
		case EVaelDeviceType::Rollwall: return TEXT("Rollwall_Kugel");
		default: return nullptr;
		}
	}

	float PartHeight(EVaelDeviceType Type)
	{
		switch (Type)
		{
		case EVaelDeviceType::Tapper: return 64.0f;
		case EVaelDeviceType::Acid: return 43.0f;
		case EVaelDeviceType::Whirl: return 34.0f;
		case EVaelDeviceType::Warner: return 150.0f;
		case EVaelDeviceType::Rollwall: return RollBallRadius;
		default: return 0.0f;
		}
	}
}

AVaelEngineerDevice::AVaelEngineerDevice()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickInterval = 0.1f;

	Base = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Base"));
	Base->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	RootComponent = Base;

	Top = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Top"));
	Top->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Top->SetupAttachment(Base);

	Glow = CreateDefaultSubobject<UPointLightComponent>(TEXT("Glow"));
	Glow->SetupAttachment(Base);
	Glow->SetIntensityUnits(ELightUnits::Candelas);
	Glow->SetIntensity(3.0f);
	Glow->SetAttenuationRadius(220.0f);
	Glow->SetCastShadows(false);
	Glow->SetRelativeLocation(FVector(0.0f, 0.0f, 60.0f));

	Pool = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Pool"));
	Pool->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Pool->SetCastShadow(false);
	Pool->SetupAttachment(Base);
	Pool->SetUsingAbsoluteRotation(true);
	Pool->SetUsingAbsoluteScale(true);
	Pool->SetVisibility(false);

	WallBox = CreateDefaultSubobject<UBoxComponent>(TEXT("WallBox"));
	WallBox->SetupAttachment(Base);
	WallBox->SetUsingAbsoluteScale(true);
	WallBox->SetBoxExtent(FVector(25.0f, WallHalfWidth, WallHeight * 0.5f));
	WallBox->SetRelativeLocation(FVector(0.0f, 0.0f, WallHeight * 0.5f));
	WallBox->SetCollisionProfileName(TEXT("BlockAllDynamic"));
	WallBox->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

AVaelEngineerDevice* AVaelEngineerDevice::Lay(UVaelEngineerComponent* InEngineer, const FVaelGear& Gear, const FVector& Location)
{
	UWorld* World = InEngineer != nullptr ? InEngineer->GetWorld() : nullptr;
	if (World == nullptr)
	{
		return nullptr;
	}

	// On the ground below the aim point
	FVector Ground = Location;
	FHitResult Hit;
	FCollisionQueryParams Query(SCENE_QUERY_STAT(VaelDeviceGround), false, InEngineer->GetOwner());
	if (World->LineTraceSingleByChannel(Hit, Location + FVector(0.0f, 0.0f, 300.0f), Location - FVector(0.0f, 0.0f, 600.0f), ECC_Visibility, Query))
	{
		Ground = Hit.ImpactPoint;
	}

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AVaelEngineerDevice* Device = World->SpawnActor<AVaelEngineerDevice>(Ground, FRotator(0.0f, FMath::FRandRange(0.0f, 360.0f), 0.0f), Params);
	if (Device == nullptr)
	{
		return nullptr;
	}
	Device->Engineer = InEngineer;
	Device->Type = Gear.Device;
	Device->Id = Gear.Id;
	Device->Born = World->GetTimeSeconds();
	Device->LaidZ = Ground.Z;
	// the moving devices are watched every frame (the others four times less often are enough)
	if (Gear.Device >= EVaelDeviceType::Tapper)
	{
		Device->SetActorTickInterval(0.0f);
	}
	// the pouch of powder is thrown: it drops in from above and bursts where it lands
	if (Gear.Device == EVaelDeviceType::Douse)
	{
		Device->SetActorLocation(Ground + FVector(0.0f, 0.0f, DouseDrop));
	}
	Device->LifeTime = Gear.Device == EVaelDeviceType::Turret ? TurretLife : Gear.Device == EVaelDeviceType::Smoke ? SmokeLife
		: Gear.Device == EVaelDeviceType::Tapper ? TapperLife : Gear.Device == EVaelDeviceType::Douse ? 0.8f : Gear.Device == EVaelDeviceType::Whirl ? WhirlLife
		: Gear.Device == EVaelDeviceType::Lantern ? LanternLife : Gear.Device == EVaelDeviceType::Warner ? WarnerLife
		: Gear.Device == EVaelDeviceType::Ram ? RamLife : Gear.Device == EVaelDeviceType::Tripwire ? TripLife : Gear.Device == EVaelDeviceType::OreCharge ? OreFuse + 0.6f : 120.0f;
	Device->ChargeEffect = Gear.Effect;
	Device->ChargeColor = Gear.Color;

	// The look of each: a flat plate with jaws, a round bladder, a post with a head
	UStaticMesh* Cylinder = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	UStaticMesh* Sphere = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	UStaticMesh* Cone = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cone.Cone"));
	UMaterialInterface* Material = LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
	switch (Gear.Device)
	{
	case EVaelDeviceType::Trap:
		Device->Base->SetStaticMesh(Cylinder);
		Device->Base->SetRelativeScale3D(FVector(0.9f, 0.9f, 0.06f));
		Device->Top->SetStaticMesh(Cone);
		Device->Top->SetRelativeLocation(FVector(0.0f, 0.0f, 10.0f));
		Device->Top->SetRelativeScale3D(FVector(0.55f, 0.55f, 0.18f));
		break;
	case EVaelDeviceType::Mine:
		Device->Base->SetStaticMesh(Sphere);
		Device->Base->SetRelativeScale3D(FVector(0.45f, 0.45f, 0.32f));
		Device->Top->SetStaticMesh(Cylinder);
		Device->Top->SetRelativeLocation(FVector(0.0f, 0.0f, 60.0f));
		Device->Top->SetRelativeScale3D(FVector(0.2f, 0.2f, 0.6f));
		break;
	case EVaelDeviceType::BuriedMine:
		// a crust plate with a glowing gland under it; it sinks into the ground once armed
		Device->Base->SetStaticMesh(Cylinder);
		Device->Base->SetRelativeScale3D(FVector(0.55f, 0.55f, 0.08f));
		Device->Top->SetStaticMesh(Sphere);
		Device->Top->SetRelativeLocation(FVector(0.0f, 0.0f, 40.0f));
		Device->Top->SetRelativeScale3D(FVector(0.35f, 0.35f, 0.25f));
		break;
	case EVaelDeviceType::Smoke:
		// a small pot of wax that bursts; the smoke itself is the effect
		Device->Base->SetStaticMesh(Cylinder);
		Device->Base->SetRelativeScale3D(FVector(0.25f, 0.25f, 0.22f));
		Device->Top->SetStaticMesh(Sphere);
		Device->Top->SetRelativeLocation(FVector(0.0f, 0.0f, 70.0f));
		Device->Top->SetRelativeScale3D(FVector(0.6f, 0.6f, 0.3f));
		break;
	case EVaelDeviceType::Tapper:
		// a squat iron chimney on the fire with the jaw on top as the nozzle
		Device->Base->SetStaticMesh(Cylinder);
		Device->Base->SetRelativeScale3D(FVector(0.4f, 0.4f, 0.7f));
		Device->Top->SetStaticMesh(Cone);
		Device->Top->SetRelativeLocation(FVector(0.0f, 0.0f, 70.0f));
		Device->Top->SetRelativeScale3D(FVector(0.8f, 0.8f, 0.5f));
		break;
	case EVaelDeviceType::Acid:
		// a small barrel, the acid sloshing at the bung
		Device->Base->SetStaticMesh(Cylinder);
		Device->Base->SetRelativeScale3D(FVector(0.35f, 0.35f, 0.45f));
		Device->Top->SetStaticMesh(Sphere);
		Device->Top->SetRelativeLocation(FVector(0.0f, 0.0f, 55.0f));
		Device->Top->SetRelativeScale3D(FVector(0.5f, 0.5f, 0.2f));
		break;
	case EVaelDeviceType::Douse:
		// a pouch of powder that bursts where it lands
		Device->Base->SetStaticMesh(Sphere);
		Device->Base->SetRelativeScale3D(FVector(0.3f, 0.3f, 0.22f));
		Device->Top->SetVisibility(false);
		break;
	default:
		Device->Base->SetStaticMesh(Cylinder);
		Device->Base->SetRelativeScale3D(FVector(0.35f, 0.35f, 1.1f));
		Device->Top->SetStaticMesh(Sphere);
		Device->Top->SetRelativeLocation(FVector(0.0f, 0.0f, 70.0f));
		Device->Top->SetRelativeScale3D(FVector(1.7f, 1.7f, 0.5f));
		break;
	}
	// The built piece (Tools/Ingenieur/vael_ingenieur_werkstuecke.py) where it exists, the shapes otherwise
	const FString PiecePath = FString::Printf(TEXT("/Game/Vael/Characters/Ingenieur/Werkstuecke/SM_Ing_%s.SM_Ing_%s"), *Gear.Id.ToString(), *Gear.Id.ToString());
	if (UStaticMesh* Piece = LoadObject<UStaticMesh>(nullptr, *PiecePath, nullptr, LOAD_NoWarn | LOAD_Quiet))
	{
		Device->Base->SetStaticMesh(Piece);
		Device->Base->SetRelativeScale3D(FVector::OneVector);
		Device->Base->EmptyOverrideMaterials();
		Device->Top->SetVisibility(false);
		// a moving part of its own: the nozzle of the Glutzapfer, the lid of the Säurefass
		const TCHAR* PartName = PartOf(Gear.Device);
		if (UStaticMesh* Part = PartName != nullptr ? LoadObject<UStaticMesh>(nullptr, *FString::Printf(TEXT("/Game/Vael/Characters/Ingenieur/Werkstuecke/SM_Ing_%s.SM_Ing_%s"), PartName, PartName), nullptr, LOAD_NoWarn | LOAD_Quiet) : nullptr)
		{
			Device->Top->SetStaticMesh(Part);
			Device->Top->EmptyOverrideMaterials();
			Device->Top->SetRelativeScale3D(FVector::OneVector);
			Device->Top->SetRelativeLocation(FVector(0.0f, 0.0f, PartHeight(Gear.Device)));
			Device->Top->SetVisibility(true);
			Device->bHasPart = true;
		}
		Device->Glow->SetLightColor(Gear.Color);
		Device->bBuiltPiece = true;
		return Device;
	}
	for (UStaticMeshComponent* Part : { Device->Base.Get(), Device->Top.Get() })
	{
		Part->SetMaterial(0, Material);
		if (UMaterialInstanceDynamic* Dynamic = Part->CreateAndSetMaterialInstanceDynamic(0))
		{
			Dynamic->SetVectorParameterValue(TEXT("Color"), Part == Device->Top ? Gear.Color : Gear.Color * 0.55f);
		}
	}
	Device->Glow->SetLightColor(Gear.Color);
	return Device;
}

void AVaelEngineerDevice::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	UVaelEngineerComponent* Eng = Engineer.Get();
	const float Now = GetWorld()->GetTimeSeconds();
	if (Eng == nullptr || Now - Born > LifeTime || (SnappedAt >= 0.0f && Now - SnappedAt > 0.8f))
	{
		Destroy();
		return;
	}
	Glow->SetIntensity(2.0f + 1.5f * FMath::Sin(Now * 4.0f));
	Animate(Now, DeltaSeconds);
	if (TickNewer(Now, DeltaSeconds))
	{
		return;
	}

	const AActor* Shooter = Eng->GetOwner();
	if (Type == EVaelDeviceType::BuriedMine)
	{
		const float Age = Now - Born;
		if (Age < BuriedArmSeconds)
		{
			// digs in: sinks until only a faint glow shows where it lies (so he knows, the enemies don't)
			const float Sink = FMath::Clamp(Age / BuriedArmSeconds, 0.0f, 1.0f);
			if (bBuiltPiece)
			{
				// the built plate sinks until its rim is level with the ground
				const FVector At = GetActorLocation();
				SetActorLocation(FVector(At.X, At.Y, LaidZ - 4.0f * Sink));
			}
			else
			{
				// (the base is the root and stays; only the gland on top sinks into the plate)
				Top->SetRelativeLocation(FVector(0.0f, 0.0f, 40.0f - 38.0f * Sink));
			}
			return;
		}
		Glow->SetIntensity(0.6f + 0.3f * FMath::Sin(Now * 2.0f));
		for (TActorIterator<AVaelCharacterBase> It(GetWorld()); It; ++It)
		{
			if (!It->IsA<AVaelCharacter>() && It->GetHealth() > 0.0f && UVaelCombatStatics::CanDamage(Shooter, *It)
				&& FVector::Dist2D(It->GetActorLocation(), GetActorLocation()) < BuriedRadius + It->GetSimpleCollisionRadius() * 0.5f)
			{
				UE_LOG(LogVael, Log, TEXT("Engineer: %s steps on a Grabmine"), *It->GetName());
				Detonate(VaelEngineerGear::Standard);
				return;
			}
		}
	}
	else if (Type == EVaelDeviceType::Smoke)
	{
		// Wind drives the smoke along and spreads it (Umwelt 25.11.); by fire it smoulders and burns whoever stands in it
		FVector Wind = AVaelRegion::GetWindAt(GetWorld(), GetActorLocation()) + AVaelLocalWeather::GetWindAt(GetWorld(), GetActorLocation());
		Wind.Z = 0.0f;
		const bool bWindy = Wind.Size() > 50.0f;
		if (bWindy)
		{
			SmokeDrifted += (Wind * SmokeDrift).GetClampedToMaxSize(SmokeDriftMax) * DeltaSeconds;
			SetActorLocation(GetActorLocation() + (Wind * SmokeDrift).GetClampedToMaxSize(SmokeDriftMax) * DeltaSeconds);
		}
		const bool bSmoulder = UVaelEnvironmentStatics::IsElementInEnvironment(this, EVaelElement::Fire);
		const float Reach = SmokeRadius * (bWindy ? SmokeWindSpread : 1.0f);
		// new puffs every two seconds (every second while it drifts, so the cloud follows)
		if (Now >= NextShot)
		{
			NextShot = Now + (bWindy ? 1.0f : 2.0f);
			VaelEffects::SpawnAt(this, VaelAssets::LoadOptional(UVaelMagicSettings::Get()->SteamEffect), GetActorLocation(), FRotator::ZeroRotator,
				bSmoulder ? FLinearColor(0.55f, 0.3f, 0.18f) : FLinearColor(0.32f, 0.3f, 0.29f), Reach * 0.9f);
		}
		int32 Inside = 0;
		for (TActorIterator<AVaelCreature> It(GetWorld()); It; ++It)
		{
			if (!It->IsDead() && UVaelCombatStatics::CanDamage(Shooter, *It) && FVector::Dist2D(It->GetActorLocation(), GetActorLocation()) < Reach)
			{
				UVaelCombatStatics::ApplyStatus(Eng->GetOwner(), *It, EVaelStatus::Confused, 0.6f);
				It->ApplyBlind(0.6f);
				if (bSmoulder)
				{
					UVaelCombatStatics::DealDamage(Eng->GetOwner(), *It, SmokeFireDps * DeltaSeconds, EVaelElement::Fire);
				}
				++Inside;
			}
		}
		SmokeHeld = FMath::Max(SmokeHeld, Inside);
	}
	else if (Type == EVaelDeviceType::Tapper && Now >= NextShot)
	{
		// Taps the fire it stands by and spits it at the nearest enemy: a burst of flames where it lands; a Brandherd burns lower
		// with every spit and goes out when drawn too low, then the Glutzapfer has nothing left and goes quiet
		AVaelFireSpot* Spot = UVaelEnvironmentStatics::FindFireSpot(this);
		if (!UVaelEnvironmentStatics::IsElementInEnvironment(this, EVaelElement::Fire))
		{
			Glow->SetIntensity(0.3f);
			return;
		}
		AVaelCharacterBase* Nearest = nullptr;
		float Best = TapperRange;
		for (TActorIterator<AVaelCharacterBase> It(GetWorld()); It; ++It)
		{
			if (It->IsA<AVaelCharacter>() || It->GetHealth() <= 0.0f || !UVaelCombatStatics::CanDamage(Shooter, *It))
			{
				continue;
			}
			const float Distance = FVector::Dist2D(It->GetActorLocation(), GetActorLocation());
			if (Distance < Best)
			{
				Best = Distance;
				Nearest = *It;
			}
		}
		if (Nearest != nullptr)
		{
			NextShot = Now + TapperInterval;
			AVaelFireSpot::SpawnBurst(GetWorld(), Nearest->GetActorLocation(), 120.0f);
			UVaelCombatStatics::DealDamage(Eng->GetOwner(), Nearest, TapperDamage * Eng->GetDamageScale(), EVaelElement::Fire);
			UVaelCombatStatics::ApplyStatus(Eng->GetOwner(), Nearest, EVaelStatus::Burning, 3.0f, 4.0f);
			if (Spot != nullptr)
			{
				Spot->GiveFire(GetActorLocation());
			}
			NozzleYaw = (Nearest->GetActorLocation() - GetActorLocation()).Rotation().Yaw - GetActorRotation().Yaw;
			KickAt = Now;
			Glow->SetIntensity(30.0f);
			++TapperShots;
		}
	}
	else if (Type == EVaelDeviceType::Acid)
	{
		if (PoolUntil < 0.0f)
		{
			for (TActorIterator<AVaelCharacterBase> It(GetWorld()); It; ++It)
			{
				if (!It->IsA<AVaelCharacter>() && It->GetHealth() > 0.0f && UVaelCombatStatics::CanDamage(Shooter, *It)
					&& FVector::Dist2D(It->GetActorLocation(), GetActorLocation()) < AcidTrigger + It->GetSimpleCollisionRadius() * 0.5f)
				{
					BurstAcid();
					break;
				}
			}
		}
		else if (Now > PoolUntil)
		{
			Destroy();
			return;
		}
		else
		{
			// the pool eats the armour: whoever stands in it is marked (takes more from everything) and burns slowly
			for (TActorIterator<AVaelCreature> It(GetWorld()); It; ++It)
			{
				if (!It->IsDead() && UVaelCombatStatics::CanDamage(Shooter, *It) && FVector::Dist2D(It->GetActorLocation(), GetActorLocation()) < PoolRadius)
				{
					UVaelCombatStatics::ApplyStatus(Eng->GetOwner(), *It, EVaelStatus::Marked, 1.0f);
					UVaelCombatStatics::DealDamage(Eng->GetOwner(), *It, AcidDps * DeltaSeconds, EVaelElement::Earth);
					++AcidBitten;
				}
			}
		}
	}
	else if (Type == EVaelDeviceType::Douse && !bDetonated && Now - Born >= DouseFall)
	{
		// Bursts at once: fires that were lit (not the placed campfires) go out, burning things cool down and are wet a while
		bDetonated = true;
		int32 PutOut = 0;
		for (TActorIterator<AVaelFireSpot> It(GetWorld()); It; ++It)
		{
			if (It->BurnTime > 0.0f && It->CanGiveFire() && FVector::Dist2D(It->GetActorLocation(), GetActorLocation()) < DouseRadius)
			{
				It->PutOut(0.5f);
				++PutOut;
			}
		}
		int32 Cooled = 0;
		for (TActorIterator<AVaelCharacterBase> It(GetWorld()); It; ++It)
		{
			if (It->GetHealth() <= 0.0f || FVector::Dist2D(It->GetActorLocation(), GetActorLocation()) > DouseRadius)
			{
				continue;
			}
			Cooled += UVaelCombatStatics::RemoveStatus(*It, EVaelStatus::Burning) ? 1 : 0;
			if (!It->IsA<AVaelCharacter>() && UVaelCombatStatics::CanDamage(Shooter, *It))
			{
				// a Glutkriecher cools down: wet, its fuse is slow to catch
				UVaelCombatStatics::ApplyStatus(Eng->GetOwner(), *It, EVaelStatus::Wet, DouseWet);
			}
		}
		VaelEffects::SpawnAt(this, VaelAssets::LoadOptional(UVaelMagicSettings::Get()->DustBurstEffect), GetActorLocation(), FRotator::ZeroRotator,
			FLinearColor(0.78f, 0.76f, 0.72f), DouseRadius * 0.5f);
		UVaelCombatTextSubsystem::PostReaction(this, LOCTEXT("Doused", "Gelöscht!"));
		UE_LOG(LogVael, Log, TEXT("Engineer: Löschpulver puts out %d fires, cools %d"), PutOut, Cooled);
	}
	else if (Type == EVaelDeviceType::Trap && SnappedAt < 0.0f)
	{
		for (TActorIterator<AVaelCharacterBase> It(GetWorld()); It; ++It)
		{
			if (It->IsA<AVaelCharacter>() || It->GetHealth() <= 0.0f || !UVaelCombatStatics::CanDamage(Shooter, *It)
				|| FVector::Dist2D(It->GetActorLocation(), GetActorLocation()) > TrapRadius + It->GetSimpleCollisionRadius() * 0.5f)
			{
				continue;
			}
			// Snaps shut: holds fast and bites
			SnappedAt = Now;
			UVaelCombatStatics::DealDamage(Eng->GetOwner(), *It, 20.0f, EVaelElement::Earth);
			UVaelCombatStatics::ApplyStatus(Eng->GetOwner(), *It, EVaelStatus::Frozen, 3.0f);
			Top->SetRelativeScale3D(FVector(0.75f, 0.75f, 0.05f));
			if (bBuiltPiece)
			{
				// the jaws snap shut: the piece squashes flat
				Base->SetRelativeScale3D(FVector(1.0f, 0.55f, 0.6f));
			}
			UVaelCombatTextSubsystem::PostReaction(*It, LOCTEXT("Snapped", "Fangeisen!"));
			UE_LOG(LogVael, Log, TEXT("Engineer: trap snaps on %s"), *It->GetName());
			break;
		}
	}
	else if (Type == EVaelDeviceType::Turret && Now >= NextShot)
	{
		AVaelCharacterBase* Nearest = nullptr;
		float Best = TurretRange;
		for (TActorIterator<AVaelCharacterBase> It(GetWorld()); It; ++It)
		{
			if (It->IsA<AVaelCharacter>() || It->GetHealth() <= 0.0f || !UVaelCombatStatics::CanDamage(Shooter, *It))
			{
				continue;
			}
			const float Distance = FVector::Dist2D(It->GetActorLocation(), GetActorLocation());
			if (Distance < Best)
			{
				Best = Distance;
				Nearest = *It;
			}
		}
		if (Nearest != nullptr)
		{
			NextShot = Now + TurretInterval;
			const FName ShotAmmo = LoadedShots > 0 ? Loaded : VaelEngineerGear::Standard;
			const FVaelGear* AmmoGear = ShotAmmo != VaelEngineerGear::Standard ? VaelEngineerGear::Find(ShotAmmo) : nullptr;
			const FVector From = GetActorLocation() + FVector(0.0f, 0.0f, 80.0f);
			AVaelEngineerShot::Launch(Eng, From, Nearest->GetActorLocation(), 2600.0f, 40.0f, 9.0f * Eng->GetDamageScale() * (1.0f + 0.5f * (Eng->GetTierScale() - 1.0f)), ShotAmmo, false,
				AmmoGear != nullptr ? AmmoGear->Color : FLinearColor(0.55f, 0.5f, 0.45f), 8.0f);
			LoadedShots = FMath::Max(LoadedShots - 1, 0);
			SetActorRotation(FRotator(0.0f, (Nearest->GetActorLocation() - GetActorLocation()).Rotation().Yaw, 0.0f));
		}
	}
}

void AVaelEngineerDevice::Detonate(FName ShotAmmo)
{
	UVaelEngineerComponent* Eng = Engineer.Get();
	if (bDetonated || Eng == nullptr)
	{
		return;
	}
	bDetonated = true;

	// Mines close by go off with it and make it stronger: x(1 + 0.3 per mine in the radius)
	TArray<AVaelEngineerDevice*> Chain;
	for (TActorIterator<AVaelEngineerDevice> It(GetWorld()); It; ++It)
	{
		if (*It != this && !It->bDetonated && VaelIsMine(It->Type) && It->GetEngineer() == Eng
			&& FVector::Dist2D(It->GetActorLocation(), GetActorLocation()) < MineRadius * 1.2f)
		{
			Chain.Add(*It);
		}
	}
	const float ChainScale = 1.0f + 0.3f * Chain.Num();
	for (int32 Index = 0; Index < Chain.Num(); ++Index)
	{
		TWeakObjectPtr<AVaelEngineerDevice> Next = Chain[Index];
		FTimerHandle Handle;
		GetWorld()->GetTimerManager().SetTimer(Handle, FTimerDelegate::CreateWeakLambda(Next.Get(), [Next, ShotAmmo]()
		{
			if (Next.IsValid())
			{
				Next->Detonate(ShotAmmo);
			}
		}), 0.12f * (Index + 1), false);
	}

	// What lies round it (Umwelt 25.11., like the mage's elements): by fire (a Brandherd, a Glutfeld, Glutdisteln) the
	// Glutknall bursts half again as hard and wider and leaves flames; on wet ground (puddles, rain) the Dampfknall
	// scalds and leaves a cloud of steam that blinds
	const bool bFire = UVaelEnvironmentStatics::IsElementInEnvironment(this, EVaelElement::Fire);
	const bool bSteam = !bFire && UVaelEnvironmentStatics::IsElementInEnvironment(this, EVaelElement::Water);
	const float Radius = MineRadius * (bFire ? FireMineRadius : 1.0f);
	const float Damage = MineDamage * Eng->GetDamageScale() * ChainScale * (bFire ? FireMineDamage : 1.0f);
	if (bFire)
	{
		AVaelFireSpot::SpawnBurst(GetWorld(), GetActorLocation(), Radius * 0.8f);
	}
	else if (bSteam)
	{
		VaelEffects::SpawnAt(this, VaelAssets::LoadOptional(UVaelMagicSettings::Get()->SteamEffect), GetActorLocation(), FRotator::ZeroRotator,
			FLinearColor(0.92f, 0.92f, 0.95f), Radius);
	}

	// The burst: everyone around takes the blast and the effect of the shot that set it off
	int32 Struck = 0;
	for (TActorIterator<AVaelCharacterBase> It(GetWorld()); It; ++It)
	{
		if (It->IsA<AVaelCharacter>() || It->GetHealth() <= 0.0f || !UVaelCombatStatics::CanDamage(Eng->GetOwner(), *It)
			|| FVector::Dist2D(It->GetActorLocation(), GetActorLocation()) > Radius)
		{
			continue;
		}
		Eng->ApplyHit(*It, It->GetActorLocation(), Damage, ShotAmmo);
		UVaelCombatStatics::ApplyStatus(Eng->GetOwner(), *It, EVaelStatus::Burning, bFire ? 5.0f : 3.0f, bFire ? 5.0f : 3.0f);
		if (bSteam)
		{
			UVaelCombatStatics::ApplyStatus(Eng->GetOwner(), *It, EVaelStatus::Confused, SteamConfused);
		}
		++Struck;
	}
	UVaelCombatTextSubsystem::PostReaction(this, bFire ? LOCTEXT("BoomFire", "Glutknall!") : bSteam ? LOCTEXT("BoomSteam", "Dampfknall!") : LOCTEXT("Boom", "Knall!"));
	UE_LOG(LogVael, Log, TEXT("Engineer: mine bursts with %s, %d struck, chain x%.1f, %s"), *ShotAmmo.ToString(), Struck, ChainScale,
		bFire ? TEXT("by fire") : bSteam ? TEXT("on wet ground") : TEXT("plain"));

	// A flash, then gone
	Glow->SetIntensity(400.0f);
	Glow->SetAttenuationRadius(Radius * 1.5f);
	Base->SetRelativeScale3D(bBuiltPiece ? FVector(2.2f) : FVector(3.2f));
	SnappedAt = GetWorld()->GetTimeSeconds() - 0.6f;
	Type = EVaelDeviceType::Trap;
}

bool AVaelEngineerDevice::TickNewer(float Now, float DeltaSeconds)
{
	UVaelEngineerComponent* Eng = Engineer.Get();
	AActor* Thrower = Eng != nullptr ? Eng->GetOwner() : nullptr;
	const float Age = Now - Born;
	const FVector Here = GetActorLocation();
	const auto Enemies = [this, Thrower](const FVector& Center, float Radius)
	{
		TArray<AVaelCharacterBase*> Found;
		for (TActorIterator<AVaelCharacterBase> It(GetWorld()); It; ++It)
		{
			if (!It->IsA<AVaelCharacter>() && It->GetHealth() > 0.0f && UVaelCombatStatics::CanDamage(Thrower, *It)
				&& FVector::Dist2D(It->GetActorLocation(), Center) < Radius + It->GetSimpleCollisionRadius() * 0.5f)
			{
				Found.Add(*It);
			}
		}
		return Found;
	};

	switch (Type)
	{
	case EVaelDeviceType::Whirl:
	{
		// Winds up, then whirls its sickles: every pulse cuts whoever is near and throws them out a little; the glowing straw
		// of the doll sets them alight
		if (Age >= WhirlWindUp && Now >= NextPulse)
		{
			NextPulse = Now + WhirlPulse;
			for (AVaelCharacterBase* Enemy : Enemies(Here, WhirlRadius))
			{
				FVaelSpellHit Cut;
				Cut.Damage = WhirlDamage * Eng->GetDamageScale();
				Cut.Element = EVaelElement::Earth;
				Cut.Knockback = 260.0f;
				Cut.Status = EVaelStatus::Burning;
				Cut.StatusDuration = 2.0f;
				Cut.StatusDamagePerSecond = 2.0f;
				UVaelCombatStatics::ApplySpellHit(Thrower, Enemy, Cut, (Enemy->GetActorLocation() - Here).GetSafeNormal2D());
				++Work;
			}
		}
		return true;
	}
	case EVaelDeviceType::Net:
	{
		// Armed after a moment; the first enemy that steps near springs it: everyone round is held, and they bleed tearing free
		if (SprungAt < 0.0f && Age >= NetArm && Enemies(Here, NetTrigger).Num() > 0)
		{
			SprungAt = Now;
			LifeTime = Age + NetHold + 0.6f;
			TArray<TWeakObjectPtr<AVaelCharacterBase>> Held;
			for (AVaelCharacterBase* Enemy : Enemies(Here, NetRadius))
			{
				UVaelCombatStatics::ApplyStatus(Thrower, Enemy, EVaelStatus::Frozen, NetHold);
				Held.Add(Enemy);
				++Work;
			}
			// shadow plants: the net draws him a little towards shadow, like the shadow shots
			if (const FVaelGear* Gear = VaelEngineerGear::Find(Id))
			{
				UVaelBalanceSubsystem::ChangePlayer(Thrower, NetShadowPull, Gear->Name);
			}
			TWeakObjectPtr<AActor> WeakOwner = Thrower;
			FTimerHandle Tear;
			GetWorld()->GetTimerManager().SetTimer(Tear, FTimerDelegate::CreateWeakLambda(this, [Held, WeakOwner]()
			{
				for (const TWeakObjectPtr<AVaelCharacterBase>& Each : Held)
				{
					if (Each.IsValid() && Each->GetHealth() > 0.0f)
					{
						UVaelCombatStatics::DealDamage(WeakOwner.Get(), Each.Get(), NetTearDamage, EVaelElement::Earth);
						UVaelCombatTextSubsystem::PostReaction(Each.Get(), LOCTEXT("TornFree", "Losgerissen"));
					}
				}
			}), NetHold, false);
			UVaelCombatTextSubsystem::PostReaction(this, LOCTEXT("NetSprung", "Würgnetz!"));
			UE_LOG(LogVael, Log, TEXT("Engineer: Würgnetz holds %d"), Work);
		}
		return true;
	}
	case EVaelDeviceType::Lantern:
	{
		// A strong warm light; every few seconds the shutter snaps open and the flash blinds whoever is in reach (farther at
		// night, when the light carries)
		const float Night = UVaelTimeOfDay::NightFactorOf(this);
		const float Reach = LanternRadius * (1.0f + 0.3f * Night);
		Glow->SetIntensity(40.0f + 360.0f * FMath::Clamp(1.0f - (Now - KickAt) / 0.25f, 0.0f, 1.0f));
		Glow->SetAttenuationRadius(Reach * 1.4f);
		if (Age >= 0.5f && Now >= NextPulse)
		{
			NextPulse = Now + LanternPulse;
			KickAt = Now;
			for (AVaelCharacterBase* Enemy : Enemies(Here, Reach))
			{
				Enemy->ApplyBlind(LanternBlind);
				UVaelCombatStatics::ApplyStatus(Thrower, Enemy, EVaelStatus::Confused, 1.0f);
				UVaelCombatTextSubsystem::PostReaction(Enemy, LOCTEXT("LanternBlind", "Geblendet"));
				++Work;
			}
		}
		return true;
	}
	case EVaelDeviceType::Warner:
	{
		// The spores whistle at whoever comes near: each enemy is marked once (it takes more damage and he sees it coming).
		// In wind the vane catches it and throws a gust that pushes everyone near away.
		for (AVaelCharacterBase* Enemy : Enemies(Here, WarnerRadius))
		{
			if (!Touched.Contains(Enemy))
			{
				Touched.Add(Enemy);
				UVaelCombatStatics::ApplyStatus(Thrower, Enemy, EVaelStatus::Marked, WarnerMark);
				UVaelCombatTextSubsystem::PostReaction(Enemy, LOCTEXT("Warned", "Gewarnt"));
				++Work;
			}
		}
		FVector Wind = AVaelRegion::GetWindAt(GetWorld(), Here) + AVaelLocalWeather::GetWindAt(GetWorld(), Here);
		Wind.Z = 0.0f;
		NozzleYaw += (60.0f + Wind.Size() * 1.5f) * DeltaSeconds;
		if (Wind.Size() > 50.0f && Now >= NextPulse)
		{
			NextPulse = Now + WarnerGustEvery;
			KickAt = Now;
			for (AVaelCharacterBase* Enemy : Enemies(Here, WarnerGustRadius))
			{
				FVaelSpellHit Gust;
				Gust.Element = EVaelElement::Air;
				Gust.Knockback = 750.0f;
				UVaelCombatStatics::ApplyNatureHit(Enemy, Gust, (Enemy->GetActorLocation() - Here).GetSafeNormal2D());
				++Work;
			}
			VaelEffects::SpawnAt(this, VaelAssets::LoadOptional(UVaelMagicSettings::Get()->DustBurstEffect), Here, FRotator::ZeroRotator,
				FLinearColor(0.42f, 0.39f, 0.36f), WarnerGustRadius * 0.6f);
		}
		return true;
	}
	case EVaelDeviceType::Ram:
	{
		// Stands across the way at once; whoever charged or rolled into it bounces off hurt and stunned, whoever leans on it
		// is pricked by the spikes. A roller that crashed on it (lying on its back) counts as charging too.
		if (!bWallUp)
		{
			bWallUp = true;
			WallSince = Now;
			WallBox->SetBoxExtent(FVector(30.0f, RamHalfWidth, RamHeight * 0.5f));
			WallBox->SetRelativeLocation(FVector(0.0f, 0.0f, RamHeight * 0.5f));
			WallBox->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
		}
		const FVector Front = GetActorForwardVector();
		const FVector Right = GetActorRightVector();
		for (AVaelCharacterBase* Enemy : Enemies(Here, RamReach))
		{
			const FVector To = Enemy->GetActorLocation() - Here;
			const float Ahead = FVector::DotProduct(To, Front);
			if (Ahead < -20.0f || FMath::Abs(FVector::DotProduct(To, Right)) > RamHalfWidth + 40.0f)
			{
				continue;
			}
			// how fast it comes at the wall, now or the frame before (it may already have been stopped by the box)
			const float Toward = -FVector::DotProduct(Enemy->GetVelocity(), Front);
			float& Seen = RamSpeed.FindOrAdd(Enemy);
			const float Came = FMath::Max(Toward, Seen);
			Seen = Toward;
			const float* Last = RamTouched.Find(Enemy);
			if (Last != nullptr && Now - *Last < RamCooldown)
			{
				continue;
			}
			const AVaelAshRoller* Roller = Cast<AVaelAshRoller>(Enemy);
			const bool bCrashed = Roller != nullptr && Roller->GetRollerState() == EVaelRollerState::OnBack && !RamTouched.Contains(Enemy);
			FVaelSpellHit Hit;
			Hit.Element = EVaelElement::Earth;
			if (Came > RamChargeSpeed || bCrashed)
			{
				Hit.Damage = RamBounceDamage * Eng->GetDamageScale();
				Hit.Knockback = 900.0f;
				Hit.Status = EVaelStatus::Frozen;
				Hit.StatusDuration = RamStun;
				KickAt = Now;
				++Work;
				UVaelCombatTextSubsystem::PostReaction(Enemy, LOCTEXT("RamBounce", "Abgeprallt!"));
				UE_LOG(LogVael, Log, TEXT("Engineer: %s bounces off the Rammschild (%.0f cm/s%s)"), *Enemy->GetName(), Came, bCrashed ? TEXT(", crashed roller") : TEXT(""));
			}
			else if (Ahead < RamTouch + Enemy->GetSimpleCollisionRadius())
			{
				Hit.Damage = RamSpikeDamage * Eng->GetDamageScale();
				Hit.Knockback = 350.0f;
			}
			else
			{
				continue;
			}
			UVaelCombatStatics::ApplySpellHit(Thrower, Enemy, Hit, Front);
			RamTouched.Add(Enemy, Now);
		}
		return true;
	}
	case EVaelDeviceType::Tripwire:
	{
		// Armed after a moment; whoever crosses the wire sets off the charge where it catches, and the wire is spent
		if (Age < TripArm || bDetonated)
		{
			return true;
		}
		const FVector Right = GetActorRightVector();
		const FVector Front = GetActorForwardVector();
		for (AVaelCharacterBase* Enemy : Enemies(Here, TripHalfLength + 60.0f))
		{
			const FVector To = Enemy->GetActorLocation() - Here;
			const float Along = FVector::DotProduct(To, Right);
			if (FMath::Abs(Along) > TripHalfLength || FMath::Abs(FVector::DotProduct(To, Front)) > 25.0f + Enemy->GetSimpleCollisionRadius() * 0.6f)
			{
				continue;
			}
			bDetonated = true;
			LifeTime = Age + 0.5f;
			const FVector At = Here + Right * Along;
			Work = ChargeBurst(At, TripRadius, TripDamage, EVaelAmmoEffect::Fire);
			UVaelCombatTextSubsystem::PostReaction(this, LOCTEXT("TripBoom", "Stolperdraht!"));
			UE_LOG(LogVael, Log, TEXT("Engineer: %s crosses the Stolperdraht, %d struck"), *Enemy->GetName(), Work);
			break;
		}
		return true;
	}
	case EVaelDeviceType::OreCharge:
	{
		// The ground glows where it lies, faster and faster, then it bursts with the effect of its ore
		if (Age < OreFuse)
		{
			const float Fuse = Age / OreFuse;
			Glow->SetLightColor(ChargeColor);
			Glow->SetAttenuationRadius(OreRadius);
			Glow->SetIntensity(6.0f + 40.0f * Fuse * (0.5f + 0.5f * FMath::Sin(Now * (8.0f + 30.0f * Fuse))));
			return true;
		}
		if (!bDetonated)
		{
			bDetonated = true;
			LifeTime = Age + 0.4f;
			Work = ChargeBurst(Here, OreRadius, OreDamage, ChargeEffect);
			Glow->SetIntensity(400.0f);
			UE_LOG(LogVael, Log, TEXT("Engineer: %s bursts, %d struck"), *Id.ToString(), Work);
		}
		return true;
	}
	case EVaelDeviceType::Rollwall:
	{
		if (bRolling)
		{
			// Rolls along the ground to its spot; enemies in the way fly aside (each once); a rock stops it early
			FVector To = RollTarget - Here;
			To.Z = 0.0f;
			const FVector Way = To.GetSafeNormal();
			const float Step = RollSpeed * DeltaSeconds;
			FVector Next = Here + Way * FMath::Min(Step, To.Size());
			FHitResult Hit;
			FCollisionQueryParams Query(SCENE_QUERY_STAT(VaelRollwall), false, this);
			Query.AddIgnoredActor(Thrower);
			if (GetWorld()->LineTraceSingleByChannel(Hit, Next + FVector(0.0f, 0.0f, 150.0f), Next - FVector(0.0f, 0.0f, 400.0f), ECC_WorldStatic, Query))
			{
				Next.Z = Hit.ImpactPoint.Z;
			}
			const bool bBlocked = GetWorld()->LineTraceSingleByChannel(Hit, Here + FVector(0.0f, 0.0f, 50.0f), Next + FVector(0.0f, 0.0f, 50.0f), ECC_WorldStatic, Query)
				&& Cast<APawn>(Hit.GetActor()) == nullptr && Hit.ImpactNormal.Z < 0.5f;
			if (!bBlocked)
			{
				SetActorLocation(Next);
				RollSpun += FMath::RadiansToDegrees(Step / RollBallRadius);
			}
			const FVector Side = FVector::CrossProduct(FVector::UpVector, Way);
			for (AVaelCharacterBase* Enemy : Enemies(GetActorLocation(), RollHitRadius))
			{
				if (!Touched.Contains(Enemy))
				{
					Touched.Add(Enemy);
					FVaelSpellHit Bump;
					Bump.Damage = RollDamage * Eng->GetDamageScale();
					Bump.Element = EVaelElement::Earth;
					Bump.Knockback = 650.0f;
					const FVector Aside = Side * (FVector::DotProduct(Enemy->GetActorLocation() - Here, Side) >= 0.0f ? 1.0f : -1.0f);
					UVaelCombatStatics::ApplySpellHit(Thrower, Enemy, Bump, (Aside + Way * 0.5f).GetSafeNormal());
					++Work;
				}
			}
			if (bBlocked || To.Size() <= Step)
			{
				// springs up into a wall across the way it rolled
				bRolling = false;
				bWallUp = true;
				WallSince = Now;
				LaidZ = GetActorLocation().Z;
				SetActorRotation(FRotator(0.0f, Way.Rotation().Yaw, 0.0f));
				LifeTime = Age + WallSeconds;
				WallBox->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
				VaelEffects::SpawnAt(this, VaelAssets::LoadOptional(UVaelMagicSettings::Get()->DustBurstEffect), GetActorLocation(), FRotator::ZeroRotator,
					FLinearColor(0.35f, 0.3f, 0.26f), 120.0f);
				UVaelCombatTextSubsystem::PostReaction(this, LOCTEXT("WallUp", "Rollwall!"));
				UE_LOG(LogVael, Log, TEXT("Engineer: Rollwall stands after knocking %d aside%s"), Work, bBlocked ? TEXT(" (stopped by rock)") : TEXT(""));
			}
		}
		return true;
	}
	default:
		return false;
	}
}


int32 AVaelEngineerDevice::ChargeBurst(const FVector& At, float Radius, float Damage, EVaelAmmoEffect Effect)
{
	// A burst of the Sprengmeister's kind (25.11.): fire sets alight, salt soaks, verdigris slows
	UVaelEngineerComponent* Eng = Engineer.Get();
	AActor* Thrower = Eng != nullptr ? Eng->GetOwner() : nullptr;
	if (Eng == nullptr)
	{
		return 0;
	}
	FVaelSpellHit Hit;
	Hit.Damage = Damage * Eng->GetDamageScale();
	Hit.Knockback = 350.0f;
	switch (Effect)
	{
	case EVaelAmmoEffect::Dew:
		Hit.Element = EVaelElement::Water;
		Hit.Status = EVaelStatus::Wet;
		Hit.StatusDuration = 6.0f;
		break;
	case EVaelAmmoEffect::Sticky:
		Hit.Element = EVaelElement::Earth;
		Hit.Status = EVaelStatus::Slowed;
		Hit.StatusDuration = 4.0f;
		break;
	default:
		Hit.Element = EVaelElement::Fire;
		Hit.Status = EVaelStatus::Burning;
		Hit.StatusDuration = 3.0f;
		Hit.StatusDamagePerSecond = 3.0f;
		break;
	}
	int32 Struck = 0;
	for (TActorIterator<AVaelCharacterBase> It(GetWorld()); It; ++It)
	{
		if (It->IsA<AVaelCharacter>() || It->GetHealth() <= 0.0f || !UVaelCombatStatics::CanDamage(Thrower, *It)
			|| FVector::Dist2D(It->GetActorLocation(), At) > Radius + It->GetSimpleCollisionRadius() * 0.5f)
		{
			continue;
		}
		UVaelCombatStatics::ApplySpellHit(Thrower, *It, Hit, (It->GetActorLocation() - At).GetSafeNormal2D());
		++Struck;
	}
	if (Hit.Element == EVaelElement::Fire)
	{
		AVaelFireSpot::SpawnBurst(GetWorld(), At, Radius * 0.8f);
	}
	VaelEffects::SpawnAt(this, VaelAssets::LoadOptional(UVaelMagicSettings::Get()->DustBurstEffect), At, FRotator::ZeroRotator, ChargeColor, Radius);
	return Struck;
}
void AVaelEngineerDevice::StartRoll(const FVector& Target)
{
	RollTarget = Target;
	bRolling = true;
	Touched.Reset();
	SetActorTickInterval(0.0f);
}

void AVaelEngineerDevice::Animate(float Now, float DeltaSeconds)
{
	const float Age = Now - Born;
	// the size it was laid with (the built pieces 1, the placeholder shapes their own), everything below scales from it
	if (BaseScale.IsZero())
	{
		BaseScale = Base->GetRelativeScale3D();
	}
	switch (Type)
	{
	case EVaelDeviceType::Tapper:
	{
		// Rises out of the ground with a little overshoot, then breathes with the fire it draws
		const float Rise = FMath::Clamp(Age / RiseSeconds, 0.0f, 1.0f);
		const float Up = Rise < 1.0f ? FMath::Sin(Rise * PI * 0.5f) * (1.0f + 0.12f * FMath::Sin(Rise * PI)) : 1.0f;
		const float Kick = FMath::Clamp(1.0f - (Now - KickAt) / 0.3f, 0.0f, 1.0f);
		const float Breath = 1.0f + 0.025f * FMath::Sin(Now * 5.0f);
		Base->SetRelativeScale3D(BaseScale * FVector(1.0f + 0.08f * Kick, 1.0f + 0.08f * Kick, FMath::Max(Up, 0.05f) * Breath * (1.0f - 0.1f * Kick)));
		if (bHasPart)
		{
			// the jaw turns smoothly to its target and kicks back when it spits; without fire it droops
			const bool bFire = UVaelEnvironmentStatics::IsElementInEnvironment(this, EVaelElement::Fire);
			const FRotator Was = Top->GetRelativeRotation();
			const float Yaw = FMath::FInterpTo(Was.Yaw, Was.Yaw + FRotator::NormalizeAxis(NozzleYaw - Was.Yaw), DeltaSeconds, 8.0f);
			const float Pitch = bFire ? 12.0f * Kick : -25.0f;
			Top->SetRelativeRotation(FRotator(FMath::FInterpTo(Was.Pitch, Pitch, DeltaSeconds, 12.0f), Yaw, 0.0f));
			Top->SetRelativeLocation(FVector(0.0f, 0.0f, 64.0f) - FRotator(0.0f, Yaw, 0.0f).Vector() * 8.0f * Kick);
		}
		Glow->SetIntensity(4.0f + 26.0f * Kick + 2.0f * FMath::Sin(Now * 9.0f));
		break;
	}
	case EVaelDeviceType::Acid:
	{
		if (BurstAt < 0.0f)
		{
			// Rises in, then rocks gently; with an enemy close it rattles and the lid jumps on the bubbles
			const float Rise = FMath::Clamp(Age / RiseSeconds, 0.0f, 1.0f);
			bool bNear = false;
			for (TActorIterator<AVaelCreature> It(GetWorld()); It && !bNear; ++It)
			{
				bNear = !It->IsDead() && FVector::Dist2D(It->GetActorLocation(), GetActorLocation()) < 350.0f;
			}
			const float Shake = bNear ? 4.0f : 1.5f;
			const float Speed = bNear ? 14.0f : 2.2f;
			Base->SetRelativeScale3D(BaseScale * FVector(1.0f, 1.0f, FMath::Max(FMath::Sin(Rise * PI * 0.5f), 0.05f)));
			SetActorRotation(FRotator(Shake * FMath::Sin(Now * Speed), GetActorRotation().Yaw, Shake * FMath::Cos(Now * Speed * 0.8f)));
			if (bHasPart)
			{
				const float Bubble = FMath::Max(FMath::Sin(Now * (bNear ? 11.0f : 3.0f)), 0.0f);
				Top->SetRelativeLocation(FVector(0.0f, 0.0f, 43.0f + Bubble * (bNear ? 2.5f : 0.8f)));
			}
			break;
		}
		const float Since = Now - BurstAt;
		// the barrel swells and is gone in a tenth of a second
		Base->SetRelativeScale3D(BaseScale * (1.0f + 1.5f * Since));
		Base->SetVisibility(Since < 0.1f);
		SetActorRotation(FRotator(0.0f, GetActorRotation().Yaw, 0.0f));
		// the lid flies off, tumbling, and is gone when it lands
		if (bHasPart && Top->IsVisible())
		{
			LidVelocity.Z -= 980.0f * DeltaSeconds;
			Top->SetWorldLocation(Top->GetComponentLocation() + LidVelocity * DeltaSeconds);
			Top->AddLocalRotation(FRotator(LidSpin * DeltaSeconds, 0.0f, LidSpin * 0.6f * DeltaSeconds));
			Top->SetVisibility(Top->GetComponentLocation().Z > LaidZ);
		}
		// the pool spreads in a third of a second, heaves while it eats, and shrinks away in its last second
		const float Left = PoolUntil - Now;
		const float Spread = FMath::SmoothStep(0.0f, 0.35f, Since) * FMath::Clamp(Left, 0.0f, 1.0f);
		const float Heave = 1.0f + 0.04f * FMath::Sin(Now * 6.0f);
		Pool->SetWorldScale3D(FVector(PoolRadius / 50.0f * Spread * Heave, PoolRadius / 50.0f * Spread * (2.0f - Heave), 0.02f));
		Glow->SetIntensity(6.0f * Spread);
		break;
	}
	case EVaelDeviceType::Whirl:
	{
		// The rotor of sickles winds up and whirls, and runs down in its last half second
		const float Left = LifeTime - Age;
		const float Speed = FMath::Clamp(Age / WhirlWindUp, 0.0f, 1.0f) * FMath::Clamp(Left / 0.5f, 0.0f, 1.0f);
		NozzleYaw = FMath::Fmod(NozzleYaw + 1080.0f * Speed * DeltaSeconds, 360.0f);
		if (bHasPart)
		{
			Top->SetRelativeRotation(FRotator(0.0f, NozzleYaw, 0.0f));
			Top->SetRelativeLocation(FVector(0.0f, 0.0f, PartHeight(Type) + 6.0f * Speed));
		}
		Glow->SetIntensity(2.0f + 10.0f * Speed);
		break;
	}
	case EVaelDeviceType::Net:
	{
		// Folded it lies flat; sprung, the net flies up and spreads over everyone near, then sinks onto them
		if (bHasPart)
		{
			if (SprungAt < 0.0f)
			{
				Top->SetVisibility(false);
			}
			else
			{
				const float Since = Now - SprungAt;
				const float Spread = FMath::SmoothStep(0.0f, 0.25f, Since);
				Top->SetVisibility(true);
				Top->SetRelativeScale3D(FVector(FMath::Max(Spread, 0.05f) * NetRadius / 100.0f, FMath::Max(Spread, 0.05f) * NetRadius / 100.0f, 1.0f));
				Top->SetRelativeLocation(FVector(0.0f, 0.0f, 120.0f * FMath::Sin(FMath::Clamp(Since / 0.45f, 0.0f, 1.0f) * PI) + 10.0f));
				Base->SetVisibility(Since < 0.1f);
			}
		}
		break;
	}
	case EVaelDeviceType::Lantern:
	{
		// Hangs from its post and sways a little; the flash makes it jump
		const float Flash = FMath::Clamp(1.0f - (Now - KickAt) / 0.25f, 0.0f, 1.0f);
		SetActorRotation(FRotator(3.0f * FMath::Sin(Now * 1.3f) + 4.0f * Flash, GetActorRotation().Yaw, 2.0f * FMath::Cos(Now * 1.1f)));
		break;
	}
	case EVaelDeviceType::Warner:
	{
		// The vane turns with the wind (NozzleYaw is driven by the work of the device), the post shudders at each gust
		const float Gust = FMath::Clamp(1.0f - (Now - KickAt) / 0.4f, 0.0f, 1.0f);
		if (bHasPart)
		{
			Top->SetRelativeRotation(FRotator(0.0f, NozzleYaw, 6.0f * Gust * FMath::Sin(Now * 40.0f)));
		}
		Base->SetRelativeScale3D(BaseScale * FVector(1.0f, 1.0f, 1.0f + 0.03f * Gust * FMath::Sin(Now * 30.0f)));
		break;
	}
	case EVaelDeviceType::Rollwall:
	{
		if (!bWallUp)
		{
			// rolled up into its ball it rolls on, the wall folded away
			Base->SetVisibility(false);
			if (bHasPart)
			{
				Top->SetVisibility(true);
				FVector Way = RollTarget - GetActorLocation();
				Way.Z = 0.0f;
				SetActorRotation(FRotator(0.0f, Way.IsNearlyZero() ? GetActorRotation().Yaw : Way.Rotation().Yaw, 0.0f));
				Top->SetRelativeRotation(FRotator(-RollSpun, 0.0f, 0.0f));
			}
		}
		else
		{
			// springs open: the plates fan up out of the ball with a little overshoot, and fold back in its last half second
			const float Since = Now - WallSince;
			const float Open = FMath::Clamp(Since / 0.25f, 0.0f, 1.0f);
			const float Up = (Open < 1.0f ? FMath::Sin(Open * PI * 0.5f) * (1.0f + 0.15f * FMath::Sin(Open * PI)) : 1.0f) * FMath::Clamp((LifeTime - Age) / 0.5f, 0.0f, 1.0f);
			Base->SetVisibility(true);
			Base->SetRelativeScale3D(BaseScale * FVector(1.0f, FMath::Max(Up, 0.05f), FMath::Max(Up, 0.05f)));
			if (bHasPart)
			{
				Top->SetVisibility(Open < 1.0f);
			}
		}
		break;
	}
	case EVaelDeviceType::Ram:
	{
		// Shoots up out of the ground with a little overshoot, trembles when something bounces off, sinks in its last half second
		const float Rise = FMath::Clamp(Age / RiseSeconds, 0.0f, 1.0f);
		const float Up = FMath::Sin(Rise * PI * 0.5f) * (1.0f + 0.12f * FMath::Sin(Rise * PI)) * FMath::Clamp((LifeTime - Age) / 0.5f, 0.0f, 1.0f);
		const float Since = Now - KickAt;
		const float Shake = Since < 0.35f ? 0.04f * (1.0f - Since / 0.35f) * FMath::Sin(Since * 70.0f) : 0.0f;
		Base->SetRelativeScale3D(BaseScale * FVector(1.0f - Shake, 1.0f + Shake, FMath::Max(Up, 0.05f)));
		break;
	}
	case EVaelDeviceType::Douse:
	{
		// Drops in from above, squashes on the ground and puffs out
		const float Fall = FMath::Clamp(Age / DouseFall, 0.0f, 1.0f);
		const FVector At = GetActorLocation();
		SetActorLocation(FVector(At.X, At.Y, LaidZ + DouseDrop * (1.0f - Fall * Fall)));
		AddActorLocalRotation(FRotator(0.0f, 0.0f, 720.0f * DeltaSeconds * (1.0f - Fall)));
		if (Fall >= 1.0f)
		{
			const float Puff = FMath::Clamp((Age - DouseFall) / 0.3f, 0.0f, 1.0f);
			Base->SetRelativeScale3D(BaseScale * FVector(1.0f + 1.6f * Puff, 1.0f + 1.6f * Puff, FMath::Max(0.6f - 0.6f * Puff, 0.02f)));
			Base->SetVisibility(Puff < 0.95f);
		}
		break;
	}
	default:
		break;
	}
}

void AVaelEngineerDevice::BurstAcid()
{
	if (Type != EVaelDeviceType::Acid || PoolUntil >= 0.0f)
	{
		return;
	}
	// In rain or on wet ground the acid runs wider (Umwelt 25.11.)
	const bool bWet = UVaelEnvironmentStatics::IsElementInEnvironment(this, EVaelElement::Water);
	PoolRadius = AcidPoolRadius * (bWet ? 1.4f : 1.0f);
	PoolUntil = GetWorld()->GetTimeSeconds() + AcidPoolSeconds;
	LifeTime = GetWorld()->GetTimeSeconds() - Born + AcidPoolSeconds + 1.0f;
	// the barrel bursts (Animate swells it, throws the lid off, and spreads the green pool where it stood)
	BurstAt = GetWorld()->GetTimeSeconds();
	LidVelocity = FVector(FMath::FRandRange(-120.0f, 120.0f), FMath::FRandRange(-120.0f, 120.0f), 520.0f);
	LidSpin = FMath::FRandRange(540.0f, 900.0f) * (FMath::RandBool() ? 1.0f : -1.0f);
	Pool->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cylinder.Cylinder")));
	Pool->SetMaterial(0, LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial")));
	if (UMaterialInstanceDynamic* Acid = Pool->CreateAndSetMaterialInstanceDynamic(0))
	{
		Acid->SetVectorParameterValue(TEXT("Color"), FLinearColor(0.45f, 0.75f, 0.15f));
	}
	Pool->SetWorldLocation(FVector(GetActorLocation().X, GetActorLocation().Y, LaidZ + 2.0f));
	Pool->SetWorldScale3D(FVector(0.01f, 0.01f, 0.02f));
	Pool->SetVisibility(true);
	Glow->SetLightColor(FLinearColor(0.5f, 0.9f, 0.2f));
	VaelEffects::SpawnAt(this, VaelAssets::LoadOptional(UVaelMagicSettings::Get()->WaterBoltTrailEffect), GetActorLocation(), FRotator::ZeroRotator,
		FLinearColor(0.5f, 0.85f, 0.15f), PoolRadius * 0.6f);
	UVaelCombatTextSubsystem::PostReaction(this, LOCTEXT("AcidBurst", "Säure!"));
	UE_LOG(LogVael, Log, TEXT("Engineer: Säurefass bursts, pool %.0f cm%s"), PoolRadius, bWet ? TEXT(" (wet ground)") : TEXT(""));
}

void AVaelEngineerDevice::LoadAmmo(FName ShotAmmo)
{
	Loaded = ShotAmmo;
	LoadedShots = 6;
	if (const FVaelGear* Gear = VaelEngineerGear::Find(ShotAmmo))
	{
		Top->SetVectorParameterValueOnMaterials(TEXT("Color"), FVector(Gear->Color));
		Glow->SetLightColor(Gear->Color);
		Glow->SetIntensity(8.0f);
		UVaelCombatTextSubsystem::PostReaction(this, FText::Format(LOCTEXT("Loaded", "Geladen: {0}"), Gear->Name));
	}
}

#undef LOCTEXT_NAMESPACE
