// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/SceneComponent.h"
#include "VaelSkyThreads.generated.h"

class UInstancedStaticMeshComponent;

/**
 *  The call of a weather spell made visible (Boris 2026-11-02): while the mage murmurs with the arms raised, small glowing signs
 *  rise from the mouth and both hands in three threads that braid round each other up to the place in the sky where the cloud
 *  will gather, in the colour of the spell's element. They stop with the release and are gone once the last has arrived.
 */
UCLASS()
class UVaelSkyThreadsComponent : public USceneComponent
{
	GENERATED_BODY()

public:

	UVaelSkyThreadsComponent();

	/** Calls up to a place in the sky until the release, seconds from now */
	void Start(const FVector& InSkyPoint, const FLinearColor& Color, float InRelease);

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

private:

	struct FMote
	{
		FVector From = FVector::ZeroVector;
		int32 Strand = 0;
		float Age = 0.0f;
		/** Which of the signs of the atlas, and how it turns as it rises */
		float Glyph = 0.0f;
		float Roll = 0.0f;
		float Spin = 0.0f;
	};

	/** Where a thread starts now: 0 the mouth, 1 and 2 the hands */
	FVector GetSource(int32 Strand) const;

	UPROPERTY(Transient)
	TObjectPtr<UInstancedStaticMeshComponent> Motes;

	TArray<FMote> Flying;
	TArray<FTransform> Frame;
	TArray<float> FrameGlyphs;

	/** True when the signs are shown, false for plain motes of light when their material is missing */
	bool bGlyphs = false;

	FVector SkyPoint = FVector::ZeroVector;
	float Release = 0.5f;
	float Age = 0.0f;
	float Credit = 0.0f;
};
