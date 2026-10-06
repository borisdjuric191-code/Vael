// Copyright Epic Games, Inc. All Rights Reserved.

#include "Items/VaelPickupOrb.h"
#include "UObject/ConstructorHelpers.h"
#include "AbilitySystemComponent.h"
#include "Combat/VaelAttributeSet.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Player/VaelCharacter.h"
#include "UI/VaelCombatTextSubsystem.h"
#include "World/VaelGround.h"
#include "World/VaelWorldSettings.h"

namespace
{
	/** Size of the placeholder orb in cm */
	constexpr float OrbDiameter = 22.0f;

	/** Height of the orb above the ground and how far it bobs, in cm */
	constexpr float OrbFloatHeight = 30.0f;
	constexpr float OrbBobHeight = 6.0f;

	/** Colors of the orbs and their numbers, as in the prototype */
	const FLinearColor HealthOrbColor = FLinearColor(FColor(0xc8, 0x2a, 0x2a));
	const FLinearColor ManaOrbColor = FLinearColor(FColor(0x2f, 0x6f, 0xd8));
	const FLinearColor HealthTextColor = FLinearColor(FColor(0xff, 0x8a, 0x80));
	const FLinearColor ManaTextColor = FLinearColor(FColor(0x80, 0xb4, 0xff));
}

AVaelPickupOrb::AVaelPickupOrb()
{
	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Mesh->SetCastShadow(false);
	Mesh->SetWorldScale3D(FVector(OrbDiameter / 100.0f));
	RootComponent = Mesh;

	static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereMesh(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	if (SphereMesh.Succeeded())
	{
		Mesh->SetStaticMesh(SphereMesh.Object);
	}

	static ConstructorHelpers::FObjectFinder<UMaterialInterface> SphereMaterial(TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
	if (SphereMaterial.Succeeded())
	{
		Mesh->SetMaterial(0, SphereMaterial.Object);
	}

	PrimaryActorTick.bCanEverTick = true;
}

AVaelPickupOrb* AVaelPickupOrb::DropOrb(UWorld* World, const FVector& Location, EVaelOrbKind InKind)
{
	if (World == nullptr)
	{
		return nullptr;
	}

	// The orb floats above the ground below the spot
	FVector Ground = Location;
	FHitResult GroundHit;
	if (VaelGround::TraceGround(World, Location + FVector(0.0f, 0.0f, 80.0f), Location - FVector(0.0f, 0.0f, 500.0f), GroundHit, nullptr))
	{
		Ground = GroundHit.Location;
	}

	const FTransform SpawnTransform(Ground + FVector(0.0f, 0.0f, OrbFloatHeight));

	AVaelPickupOrb* Orb = World->SpawnActorDeferred<AVaelPickupOrb>(AVaelPickupOrb::StaticClass(), SpawnTransform, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (Orb != nullptr)
	{
		Orb->Kind = InKind;
		Orb->FinishSpawning(SpawnTransform);
	}

	return Orb;
}

void AVaelPickupOrb::BeginPlay()
{
	Super::BeginPlay();

	BaseHeight = GetActorLocation().Z;

	if (UMaterialInstanceDynamic* Material = Mesh->CreateAndSetMaterialInstanceDynamic(0))
	{
		Material->SetVectorParameterValue(TEXT("Color"), Kind == EVaelOrbKind::Health ? HealthOrbColor : ManaOrbColor);
	}
}

void AVaelPickupOrb::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	// Bobbing, each orb a little out of step with the others
	FVector Location = GetActorLocation();
	Location.Z = BaseHeight + FMath::Sin(GetWorld()->GetTimeSeconds() * 3.0f + GetUniqueID()) * OrbBobHeight;
	SetActorLocation(Location);

	const UVaelWorldSettings* WorldSettings = UVaelWorldSettings::Get();

	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		const APlayerController* PlayerController = It->Get();
		AVaelCharacter* Player = PlayerController != nullptr ? PlayerController->GetPawn<AVaelCharacter>() : nullptr;

		if (Player == nullptr || Player->IsDowned() || FVector::Dist2D(Player->GetActorLocation(), Location) > WorldSettings->OrbPickupRadius)
		{
			continue;
		}

		UAbilitySystemComponent* AbilitySystem = Player->GetAbilitySystemComponent();
		const FGameplayAttribute Attribute = Kind == EVaelOrbKind::Health ? UVaelAttributeSet::GetHealthAttribute() : UVaelAttributeSet::GetManaAttribute();
		const FGameplayAttribute MaxAttribute = Kind == EVaelOrbKind::Health ? UVaelAttributeSet::GetMaxHealthAttribute() : UVaelAttributeSet::GetMaxManaAttribute();
		const float Amount = Kind == EVaelOrbKind::Health ? WorldSettings->HealthOrbAmount : WorldSettings->ManaOrbAmount;

		const float Current = AbilitySystem->GetNumericAttribute(Attribute);
		const float Max = AbilitySystem->GetNumericAttribute(MaxAttribute);

		// Health orbs wait for someone who needs them
		if (Kind == EVaelOrbKind::Health && Current >= Max)
		{
			continue;
		}

		AbilitySystem->SetNumericAttributeBase(Attribute, FMath::Min(Max, Current + Amount));
		UVaelCombatTextSubsystem::PostPickup(Player, FText::Format(NSLOCTEXT("VaelItems", "OrbAmount", "+{0}"), FMath::RoundToInt(Amount)),
			Kind == EVaelOrbKind::Health ? HealthTextColor : ManaTextColor);

		Destroy();
		return;
	}
}
