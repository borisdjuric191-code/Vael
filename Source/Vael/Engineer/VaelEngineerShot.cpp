// Copyright Epic Games, Inc. All Rights Reserved.

#include "Engineer/VaelEngineerShot.h"

#include "Combat/VaelCharacterBase.h"
#include "Combat/VaelCombatStatics.h"
#include "Components/StaticMeshComponent.h"
#include "Engineer/VaelEngineerComponent.h"
#include "Engineer/VaelEngineerDevice.h"
#include "Engineer/VaelEngineerGear.h"
#include "EngineUtils.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Player/VaelCharacter.h"
#include "UObject/ConstructorHelpers.h"
#include "Engine/StaticMesh.h"
#include "Magic/VaelMagicSettings.h"
#include "Magic/VaelSpellEffects.h"
#include "NiagaraSystem.h"
#include "VaelAssets.h"
#include "Components/PointLightComponent.h"
#include "Magic/VaelElementTypes.h"

namespace
{
	constexpr float ShotHitRadius = 55.0f;
	constexpr float ShotDeviceRadius = 70.0f;
}

AVaelEngineerShot::AVaelEngineerShot()
{
	PrimaryActorTick.bCanEverTick = true;

	Body = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Body"));
	Body->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Body->SetCastShadow(true);
	RootComponent = Body;

	static ConstructorHelpers::FObjectFinder<UStaticMesh> ShotMesh(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> ShotMaterial(TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
	if (ShotMesh.Succeeded())
	{
		Body->SetStaticMesh(ShotMesh.Object);
	}
	if (ShotMaterial.Succeeded())
	{
		Body->SetMaterial(0, ShotMaterial.Object);
	}
}

AVaelEngineerShot::FLook AVaelEngineerShot::GetLook(FName Ammo)
{
	FLook Look;
	const FVaelGear* Gear = Ammo != VaelEngineerGear::Standard ? VaelEngineerGear::Find(Ammo) : nullptr;
	if (Gear == nullptr)
	{
		return Look;
	}
	Look.Color = Gear->Color;
	Look.Scale = 1.35f;
	Look.Glow = 0.6f;
	switch (Gear->Effect)
	{
	case EVaelAmmoEffect::Heavy: Look.Scale = 2.0f; Look.Glow = 0.0f; Look.Color = FLinearColor(0.16f, 0.15f, 0.15f); break;
	case EVaelAmmoEffect::Sticky: Look.bDrop = true; Look.Scale = 1.5f; Look.Color = FLinearColor(0.85f, 0.7f, 0.12f); break;
	case EVaelAmmoEffect::Smoke: Look.Glow = 0.0f; Look.Color = FLinearColor(0.35f, 0.34f, 0.34f); break;
	case EVaelAmmoEffect::Crackle: Look.Glow = 2.0f; Look.Light = 900.0f; Look.Color = FLinearColor(1.0f, 0.45f, 0.08f); break;
	case EVaelAmmoEffect::Sleep: Look.Glow = 1.0f; Look.Color = FLinearColor(0.45f, 0.22f, 0.6f); break;
	case EVaelAmmoEffect::Glow: Look.bDrop = true; Look.Glow = 3.0f; Look.Light = 2500.0f; Look.Color = FLinearColor(1.0f, 0.97f, 0.85f); break;
	case EVaelAmmoEffect::Thorn: Look.Scale = 1.2f; Look.Glow = 0.3f; Look.Color = FLinearColor(0.45f, 0.08f, 0.06f); break;
	case EVaelAmmoEffect::Dew: Look.bDrop = true; Look.Scale = 1.4f; Look.Color = FLinearColor(0.35f, 0.6f, 0.9f); break;
	case EVaelAmmoEffect::Nid: Look.Glow = 1.6f; Look.Light = 600.0f; Look.Color = FLinearColor(0.28f, 0.05f, 0.35f); break;
	case EVaelAmmoEffect::Fire: Look.Glow = 1.6f; Look.Light = 700.0f; break;
	case EVaelAmmoEffect::Heal: Look.bDrop = true; Look.Glow = 2.0f; Look.Light = 900.0f; break;
	default: break;
	}
	return Look;
}

AVaelEngineerShot* AVaelEngineerShot::Launch(UVaelEngineerComponent* InEngineer, const FVector& From, const FVector& To, float Speed, float InArc, float InDamage,
	FName InAmmo, bool bInPierce, const FLinearColor& Color, float Size, bool bInStone)
{
	UWorld* World = InEngineer != nullptr ? InEngineer->GetWorld() : nullptr;
	if (World == nullptr)
	{
		return nullptr;
	}
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AVaelEngineerShot* Shot = World->SpawnActor<AVaelEngineerShot>(From, (To - From).Rotation(), Params);
	if (Shot == nullptr)
	{
		return nullptr;
	}
	Shot->Engineer = InEngineer;
	Shot->Start = From;
	Shot->End = To;
	Shot->LastLocation = From;
	Shot->FlightTime = FMath::Max(FVector::Dist(From, To) / FMath::Max(Speed, 100.0f), 0.05f);
	Shot->Arc = InArc;
	Shot->Damage = InDamage;
	Shot->Ammo = InAmmo;
	const FVaelGear* WeaponGear = InEngineer->GetWeapon();
	Shot->WeaponId = WeaponGear != nullptr ? WeaponGear->Id : NAME_None;
	Shot->bPierce = bInPierce;
	// What it looks like and what it trails: its own colour, size and glow, puffs along the way, a light for the bright ones
	const FLook Look = GetLook(InAmmo);
	const FVaelGear* AmmoGear = InAmmo != VaelEngineerGear::Standard ? VaelEngineerGear::Find(InAmmo) : nullptr;
	const EVaelAmmoEffect Effect = AmmoGear != nullptr ? AmmoGear->Effect : EVaelAmmoEffect::None;
	const UVaelMagicSettings* Magic = UVaelMagicSettings::Get();
	switch (Effect)
	{
	case EVaelAmmoEffect::Heavy: Shot->TrailPuff = VaelAssets::LoadOptional(Magic->DustPuffEffect); Shot->TrailColor = FLinearColor(0.3f, 0.28f, 0.25f); Shot->TrailRadius = 9.0f; Shot->TrailEvery = 90.0f; break;
	case EVaelAmmoEffect::Sticky: Shot->TrailPuff = VaelAssets::LoadOptional(Magic->MudEffect); Shot->TrailColor = FLinearColor(0.85f, 0.7f, 0.12f); Shot->TrailRadius = 5.0f; Shot->TrailEvery = 70.0f; break;
	case EVaelAmmoEffect::Smoke: Shot->TrailPuff = VaelAssets::LoadOptional(Magic->SteamEffect); Shot->TrailColor = FLinearColor(0.3f, 0.29f, 0.28f); Shot->TrailRadius = 16.0f; Shot->TrailEvery = 45.0f; break;
	case EVaelAmmoEffect::Crackle:
	case EVaelAmmoEffect::Fire: Shot->TrailPuff = VaelEffects::Load(EVaelElement::Fire).Impact; Shot->TrailColor = FLinearColor(1.0f, 0.5f, 0.1f); Shot->TrailRadius = 7.0f; Shot->TrailEvery = 60.0f; break;
	case EVaelAmmoEffect::Sleep: Shot->TrailPuff = VaelAssets::LoadOptional(Magic->DustPuffEffect); Shot->TrailColor = FLinearColor(0.5f, 0.25f, 0.7f); Shot->TrailRadius = 10.0f; Shot->TrailEvery = 45.0f; break;
	case EVaelAmmoEffect::Glow:
	case EVaelAmmoEffect::Heal: Shot->TrailPuff = VaelAssets::LoadOptional(Magic->HandEffect); Shot->TrailColor = Look.Color; Shot->TrailRadius = 8.0f; Shot->TrailEvery = 50.0f; break;
	case EVaelAmmoEffect::Thorn: Shot->TrailPuff = VaelAssets::LoadOptional(Magic->DustPuffEffect); Shot->TrailColor = FLinearColor(0.45f, 0.08f, 0.06f); Shot->TrailRadius = 5.0f; Shot->TrailEvery = 60.0f; break;
	case EVaelAmmoEffect::Dew: Shot->TrailPuff = VaelAssets::LoadOptional(Magic->WaterBoltTrailEffect); Shot->TrailColor = Look.Color; Shot->TrailRadius = 7.0f; Shot->TrailEvery = 40.0f; break;
	case EVaelAmmoEffect::Nid: Shot->TrailPuff = VaelAssets::LoadOptional(Magic->DustPuffEffect); Shot->TrailColor = FLinearColor(0.12f, 0.02f, 0.16f); Shot->TrailRadius = 11.0f; Shot->TrailEvery = 40.0f; break;
	case EVaelAmmoEffect::Poison:
	case EVaelAmmoEffect::Acid: Shot->TrailPuff = VaelAssets::LoadOptional(Magic->SteamEffect); Shot->TrailColor = Look.Color; Shot->TrailRadius = 8.0f; Shot->TrailEvery = 60.0f; break;
	default: break;
	}
	if (Look.Light > 0.0f)
	{
		Shot->Glow = NewObject<UPointLightComponent>(Shot, TEXT("Glow"));
		Shot->Glow->SetupAttachment(Shot->Body);
		Shot->Glow->SetUsingAbsoluteScale(true);
		Shot->Glow->SetIntensity(Look.Light);
		Shot->Glow->SetLightColor(Look.Color);
		Shot->Glow->SetAttenuationRadius(320.0f);
		Shot->Glow->SetCastShadows(false);
		Shot->Glow->RegisterComponent();
	}
	// A drop (glue, water, light, healing): a round blob in its colour instead of a stone
	if (bInStone && Look.bDrop)
	{
		Shot->bStone = true;
		Shot->Body->SetWorldScale3D(FVector(Size * Look.Scale / 50.0f) * FVector(1.25f, 1.0f, 0.9f));
		if (UMaterialInstanceDynamic* Material = Shot->Body->CreateAndSetMaterialInstanceDynamic(0))
		{
			Material->SetVectorParameterValue(TEXT("Color"), Look.Color * (1.0f + Look.Glow));
		}
		Shot->Spin = FRotator(0.0f, FMath::FRandRange(-200.0f, 200.0f), 0.0f);
		return Shot;
	}

	// The sling throws a stone: the rock shape of the mage's stones, a few centimetres, its ash look tinted by the ammo
	UStaticMesh* Rock = bInStone ? LoadObject<UStaticMesh>(nullptr, TEXT("/Game/Rock_Collection_04/Meshes/Rock_03/StaticMeshes/SM_Rock_03.SM_Rock_03"), nullptr, LOAD_NoWarn | LOAD_Quiet) : nullptr;
	UMaterialInterface* RockLook = Rock != nullptr ? LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Vael/Maps/Aschenmark/Kulisse/Pack/MI_AM_Rock_03.MI_AM_Rock_03"), nullptr, LOAD_NoWarn | LOAD_Quiet) : nullptr;
	if (Rock != nullptr)
	{
		Shot->bStone = true;
		Shot->Body->SetStaticMesh(Rock);
		const FVector Extent = Rock->GetBoundingBox().GetSize();
		Shot->Body->SetWorldScale3D(FVector(Size * 1.6f * Look.Scale / FMath::Max(Extent.GetMax(), 1.0f)));
		Shot->Spin = FRotator(FMath::FRandRange(500.0f, 900.0f), FMath::FRandRange(-300.0f, 300.0f), FMath::FRandRange(300.0f, 700.0f));
		if (RockLook != nullptr)
		{
			Shot->Body->SetMaterial(0, RockLook);
			if (UMaterialInstanceDynamic* Material = Shot->Body->CreateAndSetMaterialInstanceDynamic(0))
			{
				// Plain stones stay grey; special ammo shows its own colour and glows in the cracks as its kind does
				const bool bSpecial = InAmmo != VaelEngineerGear::Standard;
				Material->SetVectorParameterValue(TEXT("Tint"), bSpecial ? FMath::Lerp(FLinearColor(0.6f, 0.58f, 0.56f), Look.Color, 0.85f) : FLinearColor(0.7f, 0.68f, 0.65f));
				Material->SetScalarParameterValue(TEXT("Asche"), 0.15f);
				Material->SetScalarParameterValue(TEXT("Glut"), Look.Glow);
			}
		}
		VaelEffects::SpawnAt(World, VaelAssets::LoadOptional(UVaelMagicSettings::Get()->DustPuffEffect), From, FRotator::ZeroRotator, FLinearColor(0.32f, 0.29f, 0.26f), 6.0f);
		return Shot;
	}
	Shot->Body->SetWorldScale3D(FVector(Size / 50.0f) * FVector(Speed > 3000.0f ? 3.2f : 1.3f, 1.0f, 1.0f));
	if (UMaterialInstanceDynamic* Material = Shot->Body->CreateAndSetMaterialInstanceDynamic(0))
	{
		Material->SetVectorParameterValue(TEXT("Color"), Color);
	}
	return Shot;
}

void AVaelEngineerShot::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	UVaelEngineerComponent* Eng = Engineer.Get();
	if (Eng == nullptr)
	{
		Destroy();
		return;
	}

	Age += DeltaSeconds;
	const float Alpha = FMath::Clamp(Age / FlightTime, 0.0f, 1.0f);
	const FVector Location = FMath::Lerp(Start, End, Alpha) + FVector(0.0f, 0.0f, 4.0f * Arc * Alpha * (1.0f - Alpha));
	SetActorLocation(Location);
	if (bStone)
	{
		// A thrown stone tumbles
		SetActorRotation(Spin * Age);
	}
	// Its trail: a puff every so many centimetres
	if (TrailPuff != nullptr && TrailEvery > 0.0f)
	{
		TrailWay += FVector::Dist(Location, LastLocation);
		while (TrailWay >= TrailEvery)
		{
			TrailWay -= TrailEvery;
			VaelEffects::SpawnAt(this, TrailPuff, Location, FRotator::ZeroRotator, TrailColor, TrailRadius);
		}
	}
	else if (!(Location - LastLocation).IsNearlyZero())
	{
		SetActorRotation((Location - LastLocation).Rotation());
	}

	// What it passed through since the last frame: enemies first, then the devices of the engineer
	const FVector From = LastLocation;
	LastLocation = Location;
	const AActor* Shooter = Eng->GetOwner();
	for (TActorIterator<AVaelCharacterBase> It(GetWorld()); It; ++It)
	{
		AVaelCharacterBase* Enemy = *It;
		if (Enemy->IsA<AVaelCharacter>() || Hit.Contains(Enemy) || Enemy->GetHealth() <= 0.0f || !UVaelCombatStatics::CanDamage(Shooter, Enemy))
		{
			continue;
		}
		const FVector Closest = FMath::ClosestPointOnSegment(Enemy->GetActorLocation(), From, Location);
		if (FVector::Dist2D(Closest, Enemy->GetActorLocation()) > ShotHitRadius + Enemy->GetSimpleCollisionRadius() * 0.5f
			|| FMath::Abs(Closest.Z - Enemy->GetActorLocation().Z) > 160.0f)
		{
			continue;
		}
		Hit.Add(Enemy);
		bHitSomething = true;
		SpawnImpact(Closest);
		Eng->ApplyHit(Enemy, Closest, Damage, Ammo, Start, WeaponId);
		if (!bPierce)
		{
			Destroy();
			return;
		}
	}
	for (TActorIterator<AVaelEngineerDevice> It(GetWorld()); It; ++It)
	{
		AVaelEngineerDevice* Device = *It;
		if (Device->GetEngineer() != Eng || Hit.Contains(Device))
		{
			continue;
		}
		const FVector Closest = FMath::ClosestPointOnSegment(Device->GetActorLocation(), From, Location);
		if (FVector::Dist2D(Closest, Device->GetActorLocation()) > ShotDeviceRadius || FMath::Abs(Closest.Z - Device->GetActorLocation().Z) > 140.0f)
		{
			continue;
		}
		Hit.Add(Device);
		bHitSomething = true;
		Eng->OnDeviceShot(Device, Ammo);
		Destroy();
		return;
	}

	if (Alpha >= 1.0f)
	{
		// On the ground: shards and healing burst where it lands
		const FVaelGear* AmmoGear = Ammo != VaelEngineerGear::Standard ? VaelEngineerGear::Find(Ammo) : nullptr;
		// Smoke and crackle burst where they land too, not only on an enemy
		if (!bHitSomething && AmmoGear != nullptr && (AmmoGear->Effect == EVaelAmmoEffect::Shards || AmmoGear->Effect == EVaelAmmoEffect::Heal
			|| AmmoGear->Effect == EVaelAmmoEffect::Smoke || AmmoGear->Effect == EVaelAmmoEffect::Crackle))
		{
			Eng->ApplyBurst(Location, Ammo, Damage);
		}
		if (!bHitSomething)
		{
			SpawnImpact(Location);
		}
		Destroy();
	}
}

void AVaelEngineerShot::SpawnImpact(const FVector& Location) const
{
	const UVaelMagicSettings* Magic = UVaelMagicSettings::Get();
	const FVaelGear* Gear = Ammo != VaelEngineerGear::Standard ? VaelEngineerGear::Find(Ammo) : nullptr;
	const EVaelAmmoEffect Effect = Gear != nullptr ? Gear->Effect : EVaelAmmoEffect::None;
	const FLook Look = GetLook(Ammo);
	UNiagaraSystem* System = VaelAssets::LoadOptional(Magic->DustPuffEffect);
	FLinearColor Color(0.3f, 0.27f, 0.24f);
	float Radius = 18.0f;
	switch (Effect)
	{
	case EVaelAmmoEffect::Heavy: System = VaelAssets::LoadOptional(Magic->DustBurstEffect); Radius = 70.0f; break;
	case EVaelAmmoEffect::Sticky: System = VaelAssets::LoadOptional(Magic->MudEffect); Color = Look.Color; Radius = 45.0f; break;
	case EVaelAmmoEffect::Sleep: Color = Look.Color; Radius = 90.0f; break;
	case EVaelAmmoEffect::Glow: case EVaelAmmoEffect::Heal: System = VaelAssets::LoadOptional(Magic->HandEffect); Color = Look.Color; Radius = 60.0f; break;
	case EVaelAmmoEffect::Thorn: Color = Look.Color; Radius = 30.0f; break;
	case EVaelAmmoEffect::Dew: System = VaelAssets::LoadOptional(Magic->SplashEffect); Color = Look.Color; Radius = 60.0f; break;
	case EVaelAmmoEffect::Nid: System = VaelAssets::LoadOptional(Magic->DustBurstEffect); Color = FLinearColor(0.12f, 0.02f, 0.16f); Radius = 60.0f; break;
	case EVaelAmmoEffect::Fire: System = VaelEffects::Load(EVaelElement::Fire).Impact; Color = FLinearColor(1.0f, 0.5f, 0.1f); Radius = 40.0f; break;
	case EVaelAmmoEffect::Poison: case EVaelAmmoEffect::Acid: System = VaelAssets::LoadOptional(Magic->SteamEffect); Color = Look.Color; Radius = 50.0f; break;
	default: break;
	}
	VaelEffects::SpawnAt(this, System, Location, FRotator::ZeroRotator, Color, Radius);
}

