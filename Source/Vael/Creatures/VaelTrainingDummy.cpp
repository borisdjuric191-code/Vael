// Copyright Epic Games, Inc. All Rights Reserved.

#include "Creatures/VaelTrainingDummy.h"
#include "UObject/ConstructorHelpers.h"
#include "AbilitySystemComponent.h"
#include "Combat/VaelAttributeSet.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"

AVaelTrainingDummy::AVaelTrainingDummy()
{
	GetCapsuleComponent()->InitCapsuleSize(42.f, 96.0f);

	// Nobody controls the dummy, but it still has to slide when knocked back
	AutoPossessAI = EAutoPossessAI::Disabled;
	GetCharacterMovement()->bRunPhysicsWithNoController = true;

	// Placeholder look from engine assets: a cylinder filling the capsule
	Body = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Body"));
	Body->SetupAttachment(RootComponent);
	Body->SetRelativeScale3D(FVector(0.8f, 0.8f, 1.9f));
	Body->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> BodyMesh(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	if (BodyMesh.Succeeded())
	{
		Body->SetStaticMesh(BodyMesh.Object);
	}

	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = true;
}

void AVaelTrainingDummy::BeginPlay()
{
	Super::BeginPlay();

	GetAbilitySystemComponent()->GetGameplayAttributeValueChangeDelegate(UVaelAttributeSet::GetHealthAttribute()).AddUObject(this, &AVaelTrainingDummy::OnHealthChanged);
}

void AVaelTrainingDummy::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	// Back to full health some time after the last hit
	if (GetHealth() < GetMaxHealth() && GetWorld()->GetTimeSeconds() - LastHitTime >= RecoverDelay)
	{
		GetAbilitySystemComponent()->SetNumericAttributeBase(UVaelAttributeSet::GetHealthAttribute(), GetMaxHealth());
	}

#if ENABLE_DRAW_DEBUG
	const FString Status = FString::Printf(TEXT("%.0f / %.0f"), GetHealth(), GetMaxHealth());
	DrawDebugString(GetWorld(), FVector(0.f, 0.f, 130.f), Status, this, GetHealth() > 0.0f ? FColor::White : FColor::Red, 0.f, true);
#endif
}

void AVaelTrainingDummy::OnHealthChanged(const FOnAttributeChangeData& ChangeData)
{
	if (ChangeData.NewValue < ChangeData.OldValue)
	{
		LastHitTime = GetWorld()->GetTimeSeconds();
	}
}
