// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/SceneComponent.h"
#include "VaelCloudCards.generated.h"

class UInstancedStaticMeshComponent;
class UMaterialInterface;

/**
 *  Many soft cards of rolling cloud (M_Vael_Wolkenkarte) that overlap into one clean mass: mist, steam, whirling sand.
 *  Each card faces the camera or lies flat, plays the cloud loop from its own frame and has its own size and solidity.
 *  The owner moves the cards (SetCard) and calls Commit once a frame.
 */
UCLASS()
class UVaelCloudCardsComponent : public USceneComponent
{
	GENERATED_BODY()

public:

	UVaelCloudCardsComponent();

	/** Makes the cards' mesh with this material (an instance of M_Vael_Wolkenkarte) */
	void Init(UMaterialInterface* Material);

	/** Adds a card; place in the frame of this component, size its width in cm. Returns its index */
	int32 AddCard(const FVector& Place, float Size, float Solid, bool bFlat = false);

	/** Moves, sizes and fades a card */
	void SetCard(int32 Index, const FVector& Place, float Size, float Solid);

	/** Changes how bright a card is (for fire cards: how hot) */
	void SetBrightness(int32 Index, float Brightness);

	/** Turns the cards to the camera and sends everything to the renderer */
	void Commit();

	int32 Num() const { return Cards.Num(); }

private:

	struct FCard
	{
		FVector Place = FVector::ZeroVector;
		float Size = 100.0f;
		float Solid = 1.0f;
		float Roll = 0.0f;
		float Frame = 0.0f;
		bool bFlat = false;
	};

	TArray<FCard> Cards;

	UPROPERTY(Transient)
	TObjectPtr<UInstancedStaticMeshComponent> Instances;

	TArray<FTransform> Transforms;
};
