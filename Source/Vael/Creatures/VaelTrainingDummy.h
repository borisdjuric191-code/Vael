// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Combat/VaelCharacterBase.h"
#include "VaelTrainingDummy.generated.h"

class UStaticMeshComponent;

/**
 *  Temporary target for testing spells until the real creatures exist.
 *  Takes damage and knockback, shows its health as text and recovers after falling to zero.
 */
UCLASS()
class AVaelTrainingDummy : public AVaelCharacterBase
{
	GENERATED_BODY()

private:

	/** Placeholder look */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UStaticMeshComponent> Body;

public:

	/** Constructor */
	AVaelTrainingDummy();

	/** Initialization */
	virtual void BeginPlay() override;

	/** Update */
	virtual void Tick(float DeltaSeconds) override;

protected:

	/** Remembers the time of the last hit */
	virtual void OnHealthChanged(float OldValue, float NewValue) override;

	/** Seconds after the last hit until the dummy is back at full health */
	UPROPERTY(EditAnywhere, Category="Attributes", meta = (ClampMin = 0))
	float RecoverDelay = 4.0f;

	/** World time of the last hit */
	float LastHitTime = 0.0f;
};
