// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/SceneComponent.h"
#include "VaelFairyWingsComponent.generated.h"

class UProceduralMeshComponent;

/**
 *  The moth wings of the Floromant: two thin, slightly curved sheets with the picture of a wing, built in code.
 *  They hang on hinges at the shoulder blades and beat slowly while standing, faster while walking, without a skeleton.
 *  Hidden while the character plays a mage.
 */
UCLASS()
class UVaelFairyWingsComponent : public USceneComponent
{
	GENERATED_BODY()

public:

	/** Constructor */
	UVaelFairyWingsComponent();

	//~Begin UActorComponent
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	//~End UActorComponent

	/** Grows or folds the wings away */
	void SetWingsShown(bool bShown);

	/** A few quick, strong beats, like when the Floromant darts aside */
	void Burst(float Seconds = 0.45f) { BurstTimeLeft = Seconds; }

	/** Lets the wings shine faintly in the colour, for example the colour of the picked seed; 0 strength for none */
	void SetGlow(const FLinearColor& Color, float Strength);

private:

	/** Builds both wings once they are first shown */
	void BuildWings();

	/** Hinge of each wing at the shoulder blade: left, right */
	UPROPERTY(Transient)
	TArray<TObjectPtr<USceneComponent>> Hinges;

	/** Sheet of each wing */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UProceduralMeshComponent>> Sheets;

	/** Instance of the wing material, null without the material */
	UPROPERTY(Transient)
	TObjectPtr<class UMaterialInstanceDynamic> WingMaterial;

	/** Angle of the beat, advanced by the beat rate */
	float BeatPhase = 0.0f;

	/** Seconds of quick, strong beats left */
	float BurstTimeLeft = 0.0f;

	bool bShown = false;
};
