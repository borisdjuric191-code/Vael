// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Magic/VaelElementTypes.h"
#include "VaelElementSource.generated.h"

class UMaterialInterface;

/**
 *  Whose element it is: the look of one source of water (or later fire, ...) in the shared element kit.
 *  The mage's spell water, Mark water and swamp water are three sources of the same water; spells, ponds and enemies
 *  pick a source instead of bringing their own look (Designbibel, section "Wasser").
 */
UCLASS(BlueprintType)
class UVaelElementSource : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:

	/** Name shown in the editor and in tools */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Source")
	FText SourceName;

	/** Element this source belongs to */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Source")
	EVaelElement Element = EVaelElement::Water;

	/** Instance of the element's master material, used on surfaces and bodies like the water orb */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Look")
	TObjectPtr<UMaterialInterface> Material;

	/** Color of the light the element gives off */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Look")
	FLinearColor GlowColor = FLinearColor(0.15f, 0.55f, 1.0f);

	/** Color handed to the particle effects (Niagara user parameter of the element, e.g. User.WaterColor); alpha is their opacity */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Look")
	FLinearColor ParticleColor = FLinearColor(0.4f, 0.75f, 1.0f, 0.8f);

	/** 0 runs freely like clear water, 1 is thick and slow like Mark water; slows wobbling and movement of the look */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Look", meta = (ClampMin = 0, ClampMax = 1))
	float Viscosity = 0.0f;
};
