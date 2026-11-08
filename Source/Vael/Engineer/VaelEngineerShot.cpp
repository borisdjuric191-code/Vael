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
	// The sling throws a stone: the rock shape of the mage's stones, a few centimetres, its ash look tinted by the ammo
	UStaticMesh* Rock = bInStone ? LoadObject<UStaticMesh>(nullptr, TEXT("/Game/Rock_Collection_04/Meshes/Rock_03/StaticMeshes/SM_Rock_03.SM_Rock_03"), nullptr, LOAD_NoWarn | LOAD_Quiet) : nullptr;
	UMaterialInterface* RockLook = Rock != nullptr ? LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Vael/Maps/Aschenmark/Kulisse/Pack/MI_AM_Rock_03.MI_AM_Rock_03"), nullptr, LOAD_NoWarn | LOAD_Quiet) : nullptr;
	if (Rock != nullptr)
	{
		Shot->bStone = true;
		Shot->Body->SetStaticMesh(Rock);
		const FVector Extent = Rock->GetBoundingBox().GetSize();
		Shot->Body->SetWorldScale3D(FVector(Size * 1.6f / FMath::Max(Extent.GetMax(), 1.0f)));
		Shot->Spin = FRotator(FMath::FRandRange(500.0f, 900.0f), FMath::FRandRange(-300.0f, 300.0f), FMath::FRandRange(300.0f, 700.0f));
		if (RockLook != nullptr)
		{
			Shot->Body->SetMaterial(0, RockLook);
			if (UMaterialInstanceDynamic* Material = Shot->Body->CreateAndSetMaterialInstanceDynamic(0))
			{
				// Plain stones stay grey; special ammo shows its colour and glows a little in the cracks
				const bool bSpecial = InAmmo != VaelEngineerGear::Standard;
				Material->SetVectorParameterValue(TEXT("Tint"), bSpecial ? FMath::Lerp(FLinearColor(0.6f, 0.58f, 0.56f), Color, 0.7f) : FLinearColor(0.7f, 0.68f, 0.65f));
				Material->SetScalarParameterValue(TEXT("Asche"), 0.15f);
				Material->SetScalarParameterValue(TEXT("Glut"), bSpecial ? 0.6f : 0.0f);
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
		if (!bHitSomething && AmmoGear != nullptr && (AmmoGear->Effect == EVaelAmmoEffect::Shards || AmmoGear->Effect == EVaelAmmoEffect::Heal))
		{
			Eng->ApplyBurst(Location, Ammo, Damage);
		}
		if (bStone)
		{
			VaelEffects::SpawnAt(this, VaelAssets::LoadOptional(UVaelMagicSettings::Get()->DustPuffEffect), Location, FRotator::ZeroRotator, FLinearColor(0.3f, 0.27f, 0.24f), 18.0f);
		}
		Destroy();
	}
}
