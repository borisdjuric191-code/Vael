// Copyright Epic Games, Inc. All Rights Reserved.

#include "Engineer/VaelEngineerDevice.h"

#include "Combat/VaelCharacterBase.h"
#include "Combat/VaelCombatStatics.h"
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
	Device->LifeTime = Gear.Device == EVaelDeviceType::Turret ? TurretLife : 120.0f;

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
	if (Type == EVaelDeviceType::Trap && SnappedAt < 0.0f)
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
		if (*It != this && !It->bDetonated && It->Type == EVaelDeviceType::Mine && It->GetEngineer() == Eng
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

	// The burst: everyone around takes the blast and the effect of the shot that set it off
	int32 Struck = 0;
	for (TActorIterator<AVaelCharacterBase> It(GetWorld()); It; ++It)
	{
		if (It->IsA<AVaelCharacter>() || It->GetHealth() <= 0.0f || !UVaelCombatStatics::CanDamage(Eng->GetOwner(), *It)
			|| FVector::Dist2D(It->GetActorLocation(), GetActorLocation()) > MineRadius)
		{
			continue;
		}
		Eng->ApplyHit(*It, It->GetActorLocation(), MineDamage * Eng->GetDamageScale() * ChainScale, ShotAmmo);
		UVaelCombatStatics::ApplyStatus(Eng->GetOwner(), *It, EVaelStatus::Burning, 3.0f, 3.0f);
		++Struck;
	}
	UVaelCombatTextSubsystem::PostReaction(this, LOCTEXT("Boom", "Knall!"));
	UE_LOG(LogVael, Log, TEXT("Engineer: mine bursts with %s, %d struck, chain x%.1f"), *ShotAmmo.ToString(), Struck, ChainScale);

	// A flash, then gone
	Glow->SetIntensity(400.0f);
	Glow->SetAttenuationRadius(MineRadius * 1.5f);
	Base->SetRelativeScale3D(bBuiltPiece ? FVector(2.2f) : FVector(3.2f));
	SnappedAt = GetWorld()->GetTimeSeconds() - 0.6f;
	Type = EVaelDeviceType::Trap;
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
