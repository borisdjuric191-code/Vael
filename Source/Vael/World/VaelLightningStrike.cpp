// Copyright Epic Games, Inc. All Rights Reserved.

#include "World/VaelLightningStrike.h"
#include "UObject/ConstructorHelpers.h"
#include "AbilitySystemComponent.h"
#include "Combat/VaelCombatStatics.h"
#include "Components/StaticMeshComponent.h"
#include "Creatures/VaelCreature.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Magic/VaelGameplayTags.h"
#include "Magic/VaelLightningBolt.h"
#include "Player/VaelCharacter.h"
#include "Combat/VaelHitFeedbackSubsystem.h"
#include "World/VaelRegion.h"
#include "World/VaelWorldSettings.h"

namespace
{
	/** Size of the engine basic shapes used as placeholders */
	constexpr float LightningShapeSize = 100.0f;

	/** Seconds the bolt lasts and its width */
	constexpr float BoltDuration = 0.32f;
	constexpr float BoltThickness = 2.0f;

	/** Colour of the lightning of a storm */
	const FLinearColor StormBoltColor(0.55f, 0.72f, 1.0f);

	/** Height of the bolt in cm */
	constexpr float BoltHeight = 1200.0f;
}

AVaelLightningStrike::AVaelLightningStrike()
{
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));

	WarningDisc = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("WarningDisc"));
	WarningDisc->SetupAttachment(RootComponent);
	WarningDisc->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	WarningDisc->SetCastShadow(false);


	static ConstructorHelpers::FObjectFinder<UStaticMesh> CylinderMesh(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	if (CylinderMesh.Succeeded())
	{
		WarningDisc->SetStaticMesh(CylinderMesh.Object);
	}

	static ConstructorHelpers::FObjectFinder<UMaterialInterface> ShapeMaterial(TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
	if (ShapeMaterial.Succeeded())
	{
		WarningDisc->SetMaterial(0, ShapeMaterial.Object);
	}

	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = true;
}

AVaelLightningStrike* AVaelLightningStrike::SpawnStrike(AVaelRegion* Region, const FVector& GroundLocation)
{
	UWorld* World = Region != nullptr ? Region->GetWorld() : nullptr;
	if (World == nullptr)
	{
		return nullptr;
	}

	FActorSpawnParameters SpawnParameters;
	SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	AVaelLightningStrike* Strike = World->SpawnActor<AVaelLightningStrike>(AVaelLightningStrike::StaticClass(), FTransform(GroundLocation), SpawnParameters);
	if (Strike == nullptr)
	{
		return nullptr;
	}

	Strike->Region = Region;

	// The warning covers the reach for players
	const float Diameter = UVaelWorldSettings::Get()->LightningPlayerRadius * 2.0f / LightningShapeSize;
	Strike->WarningDisc->SetRelativeLocation(FVector(0.0f, 0.0f, 1.5f));
	Strike->WarningDisc->SetRelativeScale3D(FVector(Diameter, Diameter, 0.02f));

	if (UMaterialInstanceDynamic* WarningMaterial = Strike->WarningDisc->CreateAndSetMaterialInstanceDynamic(0))
	{
		WarningMaterial->SetVectorParameterValue(TEXT("Color"), FLinearColor(0.35f, 0.45f, 0.6f));
	}


	return Strike;
}

void AVaelLightningStrike::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	Elapsed += DeltaSeconds;

	const float Warning = UVaelWorldSettings::Get()->LightningWarning;
	if (!bStruck && Elapsed >= Warning)
	{
		Strike();
	}
	else if (bStruck)
	{
		Destroy();
	}
}

void AVaelLightningStrike::Strike()
{
	bStruck = true;

	WarningDisc->SetVisibility(false);
	AVaelLightningBolt::Spawn(this, GetActorLocation() + FVector(0.0f, 0.0f, BoltHeight), GetActorLocation(), StormBoltColor, BoltDuration, BoltThickness, true);

	if (AVaelRegion* OwningRegion = Region.Get())
	{
		OwningRegion->NotifyLightning();
		UVaelHitFeedbackSubsystem::Shake(this, 0.3f);
	}

	const UVaelWorldSettings* Settings = UVaelWorldSettings::Get();
	const FVector Center = GetActorLocation();

	// Lightning knows no sides: players and creatures, each with their own reach and damage; wet targets take double
	for (TActorIterator<AVaelCharacterBase> It(GetWorld()); It; ++It)
	{
		const bool bPlayer = It->IsA<AVaelCharacter>();
		const bool bCreature = It->IsA<AVaelCreature>();
		const float Radius = bPlayer ? Settings->LightningPlayerRadius : Settings->LightningCreatureRadius;

		// Gear like the Sturmmantel lets lightning pass its wearer by
		const bool bWarded = It->GetAbilitySystemComponent()->HasMatchingGameplayTag(VaelTags::Gear_WeatherWard);
		if ((!bPlayer && !bCreature) || bWarded || FVector::Dist2D(It->GetActorLocation(), Center) > Radius)
		{
			continue;
		}

		FVaelSpellHit Hit;
		Hit.Damage = bPlayer ? Settings->LightningPlayerDamage : Settings->LightningCreatureDamage;
		Hit.Element = EVaelElement::Air;
		Hit.bLightning = true;

		UVaelCombatStatics::ApplyNatureHit(*It, Hit, It->GetActorLocation() - Center);
	}
}
