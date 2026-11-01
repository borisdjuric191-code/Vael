// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/SceneComponent.h"
#include "VaelHandSteam.generated.h"

class UVaelCloudCardsComponent;

/** A little steam rising from the hand of a spent mage after a beam, one puff after the other. Removes itself once the last puff has gone. */
UCLASS()
class UVaelHandSteamComponent : public USceneComponent
{
	GENERATED_BODY()

public:

	UVaelHandSteamComponent();

	/** Lets the puffs rise for the seconds; false without the material of the clouds */
	bool Start(float InSeconds);

	//~Begin UActorComponent
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	//~End UActorComponent

private:

	UPROPERTY(Transient)
	TObjectPtr<UVaelCloudCardsComponent> Cards;

	/** Where each puff left the hand, in the world */
	TArray<FVector> Births;

	float Seconds = 0.6f;
	float Age = 0.0f;
};
