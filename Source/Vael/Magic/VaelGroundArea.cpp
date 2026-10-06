// Copyright Epic Games, Inc. All Rights Reserved.

#include "Magic/VaelGroundArea.h"
#include "UObject/ConstructorHelpers.h"
#include "Combat/VaelCharacterBase.h"
#include "Combat/VaelCombatStatics.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Pawn.h"
#include "Magic/VaelMagicSettings.h"
#include "Magic/VaelSpellEffects.h"
#include "NiagaraSystem.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Vael.h"
#include "VaelAssets.h"
#include "World/VaelRegion.h"
#include "World/VaelWorldSettings.h"

namespace
{
	/** Radius of the engine cylinder mesh used as placeholder */
	constexpr float PlaceholderCylinderRadius = 50.0f;

	/** Seconds between two damage steps of an area */
	constexpr float DamageStepInterval = 0.5f;
}

AVaelGroundArea::AVaelGroundArea()
{
	// Placeholder look from engine assets
	Disc = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Disc"));
	Disc->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Disc->SetCastShadow(false);
	Disc->bReceivesDecals = false;
	RootComponent = Disc;

	static ConstructorHelpers::FObjectFinder<UStaticMesh> DiscMesh(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	if (DiscMesh.Succeeded())
	{
		Disc->SetStaticMesh(DiscMesh.Object);
	}

	static ConstructorHelpers::FObjectFinder<UMaterialInterface> DiscMaterial(TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
	if (DiscMaterial.Succeeded())
	{
		Disc->SetMaterial(0, DiscMaterial.Object);
	}

	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = true;
}

void AVaelGroundArea::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);

	RefreshLook();
}

void AVaelGroundArea::BeginPlay()
{
	Super::BeginPlay();

	RefreshLook();

	if (Lifetime > 0.0f)
	{
		// Fires lit in the rain die sooner, in a drought they burn longer
		const AVaelRegion* Region = Element == EVaelElement::Fire ? AVaelRegion::GetRegionAt(GetWorld(), GetActorLocation()) : nullptr;
		if (Region != nullptr && Region->GetWeather() == EVaelWeather::Rain)
		{
			Lifetime *= UVaelWorldSettings::Get()->RainFireLifetimeShare;
		}
		else if (Region != nullptr && Region->GetWeather() == EVaelWeather::Drought)
		{
			Lifetime *= UVaelWorldSettings::Get()->DroughtFireLifetimeMultiplier;
		}

		SetLifeSpan(Lifetime);
	}

	// Only areas that hurt somebody need to tick
	SetActorTickEnabled(DamagePerSecond > 0.0f && GetInstigator() != nullptr);

	// The real look replaces the placeholder disc as soon as the effect exists
	SpawnVisual();
}

void AVaelGroundArea::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	DamageStepTime += DeltaSeconds;
	if (DamageStepTime < DamageStepInterval)
	{
		return;
	}

	FVaelSpellHit Hit;
	Hit.Damage = DamagePerSecond * DamageStepTime;
	Hit.Element = Element;

	DamageStepTime = 0.0f;

	for (TActorIterator<AVaelCharacterBase> It(GetWorld()); It; ++It)
	{
		if (IsInRange(It->GetActorLocation()))
		{
			UVaelCombatStatics::ApplySpellHit(GetInstigator(), *It, Hit, FVector::ZeroVector);
		}
	}
}

AVaelGroundArea* AVaelGroundArea::SpawnArea(UWorld* World, const FVector& Location, EVaelElement InElement, float InRadius, float InLifetime, float InDamagePerSecond, APawn* InInstigator, bool bInExtinguishable, EVaelGroundEffect InEffect, UNiagaraSystem* InVisual)
{
	if (World == nullptr)
	{
		return nullptr;
	}

	const FTransform SpawnTransform(Location);

	AVaelGroundArea* Area = World->SpawnActorDeferred<AVaelGroundArea>(AVaelGroundArea::StaticClass(), SpawnTransform, InInstigator, InInstigator, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (Area != nullptr)
	{
		Area->Element = InElement;
		Area->Radius = InRadius;
		Area->Lifetime = InLifetime;
		Area->DamagePerSecond = InDamagePerSecond;
		Area->bExtinguishable = bInExtinguishable;
		Area->Effect = InEffect;
		Area->VisualOverride = InVisual;

		Area->FinishSpawning(SpawnTransform);
	}

	return Area;
}

int32 AVaelGroundArea::ExtinguishFires(const UWorld* World, const FVector& Location, float InRadius)
{
	int32 NumExtinguished = 0;

	for (TActorIterator<AVaelGroundArea> It(World); It; ++It)
	{
		AVaelGroundArea* Area = *It;
		if (Area->Element == EVaelElement::Fire && Area->bExtinguishable && Area->IsInRange(Location, InRadius))
		{
			UE_LOG(LogVael, Verbose, TEXT("Fire '%s' is put out"), *GetNameSafe(Area));

			Area->Destroy();
			++NumExtinguished;
		}
	}

	return NumExtinguished;
}

int32 AVaelGroundArea::SpreadFires(APawn* Caster, const FVector& Origin, const FVector& Direction, float Range, float HalfAngleDegrees)
{
	UWorld* World = Caster != nullptr ? Caster->GetWorld() : nullptr;
	if (World == nullptr)
	{
		return 0;
	}

	const UVaelMagicSettings* MagicSettings = UVaelMagicSettings::Get();
	const float MinCosine = FMath::Cos(FMath::DegreesToRadians(HalfAngleDegrees));

	// New fires are added while iterating, so collect the existing ones first
	TArray<AVaelGroundArea*> Fires;
	for (TActorIterator<AVaelGroundArea> It(World); It; ++It)
	{
		if (It->Element == EVaelElement::Fire)
		{
			Fires.Add(*It);
		}
	}

	int32 NumSpread = 0;
	for (const AVaelGroundArea* Fire : Fires)
	{
		if (NumSpread >= MagicSettings->MaxFireSpreads)
		{
			break;
		}

		const FVector FireLocation = Fire->GetActorLocation();
		const FVector ToFire = (FireLocation - Origin).GetSafeNormal2D();

		if (!Fire->IsInRange(Origin, Range) || FVector::DotProduct(ToFire, Direction) < MinCosine)
		{
			continue;
		}

		// Walls stop the fire
		const FVector NewLocation = FireLocation + Direction * MagicSettings->FireSpreadDistance;
		const FVector TraceOffset(0.0f, 0.0f, 50.0f);
		if (World->LineTraceTestByObjectType(FireLocation + TraceOffset, NewLocation + TraceOffset, FCollisionObjectQueryParams(ECC_WorldStatic)))
		{
			continue;
		}

		const float NewDamagePerSecond = Fire->DamagePerSecond > 0.0f ? Fire->DamagePerSecond : MagicSettings->FireSpreadDamagePerSecond;
		SpawnArea(World, NewLocation, EVaelElement::Fire, Fire->Radius * 1.1f, MagicSettings->FireSpreadLifetime, NewDamagePerSecond, Caster);

		UE_LOG(LogVael, Verbose, TEXT("Wind carries fire '%s' on to %s"), *GetNameSafe(Fire), *NewLocation.ToCompactString());

		++NumSpread;
	}

	return NumSpread;
}

bool AVaelGroundArea::IsFireNear(const UWorld* World, const FVector& Location, float ExtraDistance)
{
	if (World == nullptr)
	{
		return false;
	}

	for (TActorIterator<AVaelGroundArea> It(World); It; ++It)
	{
		if (It->Element == EVaelElement::Fire && It->IsInRange(Location, ExtraDistance))
		{
			return true;
		}
	}

	return false;
}

void AVaelGroundArea::GetEffectsOn(const AActor* Victim, bool& bOutBlinded, float& OutSpeedMultiplier)
{
	bOutBlinded = false;
	OutSpeedMultiplier = 1.0f;

	const UWorld* World = Victim != nullptr ? Victim->GetWorld() : nullptr;
	if (World == nullptr)
	{
		return;
	}

	const FVector Location = Victim->GetActorLocation();

	for (TActorIterator<AVaelGroundArea> It(World); It; ++It)
	{
		const AVaelGroundArea* Area = *It;
		if (Area->Effect == EVaelGroundEffect::None || !Area->IsInRange(Location))
		{
			continue;
		}

		// Areas of a caster only affect the enemies of the caster
		if (Area->GetInstigator() != nullptr && !UVaelCombatStatics::CanDamage(Area->GetInstigator(), Victim))
		{
			continue;
		}

		if (Area->Effect == EVaelGroundEffect::Blind)
		{
			bOutBlinded = true;
		}
		else if (Area->Effect == EVaelGroundEffect::Slow)
		{
			OutSpeedMultiplier = FMath::Min(OutSpeedMultiplier, UVaelMagicSettings::Get()->MudSpeedMultiplier);
		}
	}
}

bool AVaelGroundArea::IsInRange(const FVector& Location, float ExtraDistance) const
{
	return FVector::DistSquared2D(Location, GetActorLocation()) <= FMath::Square(Radius + ExtraDistance);
}

void AVaelGroundArea::RefreshLook()
{
	Disc->SetWorldScale3D(FVector(Radius / PlaceholderCylinderRadius, Radius / PlaceholderCylinderRadius, 0.02f));

	UMaterialInstanceDynamic* Material = Cast<UMaterialInstanceDynamic>(Disc->GetMaterial(0));
	if (Material == nullptr)
	{
		Material = Disc->CreateAndSetMaterialInstanceDynamic(0);
	}

	if (Material != nullptr)
	{
		Material->SetVectorParameterValue(TEXT("Color"), GetLookColor());
	}
}

FLinearColor AVaelGroundArea::GetLookColor() const
{
	// Steam is pale, everything else shows the color of its element
	const UVaelMagicSettings* MagicSettings = UVaelMagicSettings::Get();
	return Effect == EVaelGroundEffect::Blind ? MagicSettings->SteamColor : MagicSettings->GetElementColor(Element);
}

void AVaelGroundArea::SpawnVisual()
{
	UNiagaraSystem* Visual = VisualOverride;

	if (Visual == nullptr)
	{
		const UVaelMagicSettings* MagicSettings = UVaelMagicSettings::Get();

		// Steam and mud have their own look, or else the one of water and earth
		if (Effect == EVaelGroundEffect::Blind)
		{
			Visual = VaelAssets::LoadOptional(MagicSettings->SteamEffect);
		}
		else if (Effect == EVaelGroundEffect::Slow)
		{
			Visual = VaelAssets::LoadOptional(MagicSettings->MudEffect);
		}

		if (Visual == nullptr)
		{
			const EVaelElement LookElement = Effect == EVaelGroundEffect::Blind ? EVaelElement::Water : Effect == EVaelGroundEffect::Slow ? EVaelElement::Earth : Element;
			Visual = VaelEffects::Load(LookElement).Ground;
		}
	}

	if (VaelEffects::Attach(Visual, Disc, NAME_None, GetLookColor(), Radius) != nullptr)
	{
		Disc->SetVisibility(false);
	}
}
