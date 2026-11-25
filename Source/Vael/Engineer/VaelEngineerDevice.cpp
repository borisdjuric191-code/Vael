// Copyright Epic Games, Inc. All Rights Reserved.

#include "Engineer/VaelEngineerDevice.h"

#include "Combat/VaelCharacterBase.h"
#include "Combat/VaelCombatStatics.h"
#include "Creatures/VaelCreature.h"
#include "Magic/VaelEnvironmentStatics.h"
#include "World/VaelFireSpot.h"
#include "World/VaelLocalWeather.h"
#include "World/VaelRegion.h"
#include "Magic/VaelMagicSettings.h"
#include "Magic/VaelSpellEffects.h"
#include "NiagaraSystem.h"
#include "VaelAssets.h"
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
	Device->LifeTime = Gear.Device == EVaelDeviceType::Turret ? TurretLife : Gear.Device == EVaelDeviceType::Smoke ? SmokeLife
		: Gear.Device == EVaelDeviceType::Tapper ? TapperLife : Gear.Device == EVaelDeviceType::Douse ? 0.8f : 120.0f;

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
			SetActorRotation(FRotator(0.0f, (Nearest->GetActorLocation() - GetActorLocation()).Rotation().Yaw, 0.0f));
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
	else if (Type == EVaelDeviceType::Douse && !bDetonated)
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
	// the barrel gone, a flat green pool in its place
	Base->SetVisibility(false);
	Top->SetVisibility(true);
	Top->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cylinder.Cylinder")));
	Top->SetUsingAbsoluteScale(true);
	Top->SetWorldScale3D(FVector(PoolRadius / 50.0f, PoolRadius / 50.0f, 0.02f));
	Top->SetRelativeLocation(FVector(0.0f, 0.0f, 2.0f));
	Top->SetVectorParameterValueOnMaterials(TEXT("Color"), FVector(0.45f, 0.75f, 0.15f));
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
