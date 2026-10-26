// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/SceneComponent.h"
#include "VaelFairyWingsComponent.generated.h"

class UProceduralMeshComponent;

/**
 *  The moth wings of the Floromant: two thin, slightly curved sheets with the picture of a wing, built in code.
 *  They hang on hinges at the shoulder blades and beat slowly while standing, faster while flying, without a skeleton;
 *  in steady fast flight they are held open to glide, braking makes them beat hard.
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

	/** How fast she flies (0 to 1 of her top speed) and how hard she brakes (0 to 1), told every frame by the character */
	void SetFlight(float InSpeedShare, float InBrake) { SpeedShare = InSpeedShare; Brake = InBrake; }

	/** 0 while the wings beat, 1 while they are held open to glide */
	float GetGlide() const { return Glide; }

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

	/** Told by the character: share of top speed and how hard it brakes */
	float SpeedShare = 0.0f;
	float Brake = 0.0f;

	/** Gliding: how far the wings are held open, seconds of steady fast flight, seconds until the next beats and of beats left */
	float Glide = 0.0f;
	float SteadyTime = 0.0f;
	float NextPumpTime = 0.0f;
	float PumpTimeLeft = 0.0f;

	bool bShown = false;
};
