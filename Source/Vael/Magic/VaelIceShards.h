// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "VaelIceShards.generated.h"

class UInstancedStaticMeshComponent;

/**
 *  Broken ice the Hagelsturm leaves where a strike landed. It lies a while and slowly thaws away; fire close by melts it
 *  in moments into a small puddle, water the mage can draw on again (Boris 2026-10-25: water, ice, fire, water).
 */
UCLASS()
class AVaelIceShards : public AActor
{
	GENERATED_BODY()

public:

	AVaelIceShards();

	/** Scatters shards over a patch of this radius around the ground point */
	static AVaelIceShards* Spawn(UWorld* World, const FVector& Ground, float InRadius, APawn* InInstigator);

	virtual void Tick(float DeltaSeconds) override;

private:

	void Build();

	/** True if fire burns close enough to melt the ice */
	bool IsFireClose() const;

	UPROPERTY(Transient)
	TObjectPtr<UInstancedStaticMeshComponent> Shards;

	TArray<FTransform> Rest;

	float Radius = 80.0f;
	float Age = 0.0f;

	/** Seconds since fire began to melt it, below 0 while none has */
	float MeltAge = -1.0f;
	float NextFireCheck = 0.0f;
};
