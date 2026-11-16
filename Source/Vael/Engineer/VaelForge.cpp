// Copyright Epic Games, Inc. All Rights Reserved.

#include "Engineer/VaelForge.h"

#include "Components/PointLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engineer/VaelEngineerComponent.h"
#include "EngineUtils.h"
#include "Magic/VaelElementTypes.h"
#include "Magic/VaelMagicSettings.h"
#include "Magic/VaelSpellEffects.h"
#include "NiagaraSystem.h"
#include "Player/VaelCharacter.h"
#include "Player/VaelPlayerController.h"
#include "Engineer/VaelEngineerGear.h"
#include "VaelAssets.h"

#define LOCTEXT_NAMESPACE "VaelForge"

namespace
{
	constexpr float ForgeReach = 600.0f;
}

AVaelForge::AVaelForge()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickInterval = 0.05f;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	Mesh->SetupAttachment(RootComponent);
	CoalGlow = CreateDefaultSubobject<UPointLightComponent>(TEXT("CoalGlow"));
	CoalGlow->SetupAttachment(RootComponent);
	CoalGlow->SetRelativeLocation(FVector(0.0f, 0.0f, 80.0f));
	CoalGlow->SetIntensityUnits(ELightUnits::Candelas);
	CoalGlow->SetIntensity(BaseIntensity);
	CoalGlow->SetLightColor(FLinearColor(1.0f, 0.42f, 0.12f));
	CoalGlow->SetAttenuationRadius(520.0f);
	CoalGlow->SetCastShadows(false);
}

void AVaelForge::BeginPlay()
{
	Super::BeginPlay();
	BaseIntensity = CoalGlow->Intensity;
	UVaelInteractionSubsystem::Register(this);
}

void AVaelForge::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	UVaelInteractionSubsystem::Unregister(this);
	Super::EndPlay(EndPlayReason);
}

void AVaelForge::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	Clock += DeltaSeconds;

	// An engineer near it built something: sparks off the anvil, the coals flare
	for (TActorIterator<AVaelCharacter> It(GetWorld()); It; ++It)
	{
		const UVaelEngineerComponent* Engineer = It->IsEngineer() ? It->GetEngineer() : nullptr;
		if (Engineer == nullptr || FVector::Dist2D(It->GetActorLocation(), GetActorLocation()) > ForgeReach || Engineer->GetSecondsSinceBuilt() > 0.5f)
		{
			continue;
		}
		const float BuiltAt = GetWorld()->GetTimeSeconds() - Engineer->GetSecondsSinceBuilt();
		float& Last = Answered.FindOrAdd(*It);
		if (FMath::IsNearlyEqual(Last, BuiltAt, 0.01f))
		{
			continue;
		}
		Last = BuiltAt;
		Flare = 1.0f;
		const FVector At = GetActorTransform().TransformPosition(SparkOffset);
		const FVaelGear* Gear = VaelEngineerGear::Find(Engineer->GetLastBuilt());
		const FLinearColor Color = Gear != nullptr ? FMath::Lerp(FLinearColor(1.0f, 0.55f, 0.15f), Gear->Color, 0.3f) : FLinearColor(1.0f, 0.55f, 0.15f);
		VaelEffects::SpawnAt(this, VaelEffects::Load(EVaelElement::Fire).Impact, At, FRotator::ZeroRotator, Color, 40.0f);
		VaelEffects::SpawnAt(this, VaelAssets::LoadOptional(UVaelMagicSettings::Get()->DustPuffEffect), At, FRotator::ZeroRotator, FLinearColor(0.3f, 0.26f, 0.22f), 22.0f);
	}

	// The coals breathe slowly and flicker a little; a flare dies down in a second
	Flare = FMath::Max(Flare - DeltaSeconds, 0.0f);
	const float Breath = 0.85f + 0.12f * FMath::Sin(Clock * 1.3f) + 0.05f * FMath::Sin(Clock * 7.1f + 0.4f);
	CoalGlow->SetIntensity(BaseIntensity * (Breath + 2.2f * Flare * Flare));
}

bool AVaelForge::CanInteract(const AVaelCharacter* Player) const
{
	return Player != nullptr && Player->IsEngineer() && Player->GetController<AVaelPlayerController>() != nullptr;
}

void AVaelForge::Interact(AVaelCharacter* Player)
{
	if (AVaelPlayerController* Controller = Player != nullptr ? Player->GetController<AVaelPlayerController>() : nullptr)
	{
		Player->GetEngineer()->Restock();
		Controller->OpenCrafting(true, static_cast<int32>(EVaelGearKind::Refine));
	}
}

FText AVaelForge::GetInteractPrompt() const
{
	return LOCTEXT("Forge", "An der Esse schmieden");
}

#undef LOCTEXT_NAMESPACE
