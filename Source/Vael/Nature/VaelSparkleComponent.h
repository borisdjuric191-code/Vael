// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/SceneComponent.h"
#include "VaelSparkleComponent.generated.h"

class UInstancedStaticMeshComponent;
class UMaterialInstanceDynamic;
class UStaticMeshComponent;

/** How a model glints: small stars that flare up on its surface for a moment, so it is seen from afar */
USTRUCT(BlueprintType)
struct FVaelSparkle
{
	GENERATED_BODY()

	/** Stars that flare up per second; 0 for none */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Sparkle", meta = (ClampMin = 0))
	float Rate = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Sparkle")
	FLinearColor Color = FLinearColor::White;

	/** Every now and then a star takes this colour instead (the red heart of a violet crystal) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Sparkle")
	FLinearColor SecondColor = FLinearColor::White;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Sparkle", meta = (ClampMin = 0, ClampMax = 1))
	float SecondShare = 0.0f;

	/** Width of a star at its brightest in cm (each one between 60 and 130 % of it) and its brightness */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Sparkle", meta = (ClampMin = 1))
	float Size = 14.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Sparkle", meta = (ClampMin = 0))
	float Brightness = 8.0f;

	/** Only the upper share of the model glints (1 all of it, 0.4 its top 40 %): crystals sit on dull rock */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Sparkle", meta = (ClampMin = 0.05, ClampMax = 1))
	float TopShare = 1.0f;

	/** The glow of the model (GlutStaerke) swells and fades by this share, once every PulsePeriod seconds; 0 for a steady glow */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Sparkle", meta = (ClampMin = 0, ClampMax = 1))
	float Pulse = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Sparkle", meta = (ClampMin = 0.2))
	float PulsePeriod = 3.0f;
};

/**
 *  Lets a static model glint: stars (SM_Funkeln, built in Tools/Kreaturenfabrik/blender/vael_funkeln.py) flare up at
 *  random places of its surface, turned to the camera, and its glow can pulse. Works only while the model is in the
 *  picture. Used by ores and the pearl plants.
 */
UCLASS()
class VAEL_API UVaelSparkleComponent : public USceneComponent
{
	GENERATED_BODY()

public:

	UVaelSparkleComponent();

	/** Glints on this model from now on; Strength scales the number and brightness of the stars (0 switches them off) */
	void Show(UStaticMeshComponent* InModel, const FVaelSparkle& InSparkle, float Strength = 1.0f);

	/** The steady glow the pulse swings around, and the material it is set on */
	void SetGlow(UMaterialInstanceDynamic* Material, float Glow);

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

private:

	struct FGlint
	{
		FVector Place = FVector::ZeroVector;
		float Age = 0.0f;
		float Life = 0.0f;
		float Size = 0.0f;
		float Turn = 0.0f;
		bool bSecond = false;
	};

	void SamplePlaces();
	UInstancedStaticMeshComponent* AddStars(const FLinearColor& Color);

	UPROPERTY(Transient)
	TObjectPtr<UStaticMeshComponent> Model;

	UPROPERTY(Transient)
	TObjectPtr<UInstancedStaticMeshComponent> Stars;

	UPROPERTY(Transient)
	TObjectPtr<UInstancedStaticMeshComponent> SecondStars;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> GlowMaterial;

	FVaelSparkle Sparkle;
	TArray<FVector> Places;
	TArray<FGlint> Glints;
	float Strength = 1.0f;
	float Credit = 0.0f;
	float BaseGlow = 0.0f;
	float Seed = 0.0f;
};
