// Copyright Epic Games, Inc. All Rights Reserved.

#include "Player/VaelAnimInstance.h"
#include "AbilitySystemComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Magic/VaelGameplayTags.h"
#include "Player/VaelCharacter.h"

void UVaelAnimInstance::NativeInitializeAnimation()
{
	Super::NativeInitializeAnimation();

	Character = Cast<AVaelCharacter>(TryGetPawnOwner());
}

void UVaelAnimInstance::NativeUpdateAnimation(float DeltaSeconds)
{
	Super::NativeUpdateAnimation(DeltaSeconds);

	if (Character == nullptr)
	{
		Character = Cast<AVaelCharacter>(TryGetPawnOwner());
		if (Character == nullptr)
		{
			return;
		}
	}

	const FVector Velocity = Character->GetVelocity();
	const FVector GroundVelocity(Velocity.X, Velocity.Y, 0.0f);

	Speed = GroundVelocity.Size();
	bIsMoving = Speed > MovingThreshold;

	// The Floromant hovers instead of walking: its legs stay still while it glides
	const bool bHovering = Character->IsFloromant();
	if (bHovering)
	{
		Speed = 0.0f;
		bIsMoving = false;
	}

	// Keep the last direction while standing, so the blend doesn't jump back to forward
	if (bIsMoving)
	{
		Direction = (GroundVelocity.Rotation() - Character->GetActorRotation()).GetNormalized().Yaw;
	}

	const UCharacterMovementComponent* Movement = Character->GetCharacterMovement();
	bIsFalling = Movement != nullptr && Movement->IsFalling();

	bIsDodging = Character->IsDodging() && !bHovering;
	bIsDowned = Character->IsDowned();

	const UAbilitySystemComponent* AbilitySystem = Character->GetAbilitySystemComponent();
	bIsCasting = AbilitySystem != nullptr && (AbilitySystem->HasMatchingGameplayTag(VaelTags::State_Casting) || AbilitySystem->HasMatchingGameplayTag(VaelTags::State_Channeling));
}
