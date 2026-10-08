// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/SceneComponent.h"
#include "Magic/VaelElementTypes.h"
#include "VaelElementOrbitComponent.generated.h"

class UMaterialInterface;
class UPointLightComponent;
class UStaticMesh;
class UStaticMeshComponent;
class UVaelElementComponent;

/** One chosen element circling above the head of its mage */
USTRUCT()
struct FVaelElementOrb
{
	GENERATED_BODY()

	EVaelElement Element = EVaelElement::Fire;

	/** Elements drawn from the environment are bigger */
	bool bFromEnvironment = false;

	/** Moves along the circle; the parts hang on it */
	UPROPERTY()
	TObjectPtr<USceneComponent> Pivot;

	/** Shapes of the orb that move with the pivot: core, shell, drops, streaks ... in the order its element builds them */
	UPROPERTY()
	TArray<TObjectPtr<UStaticMeshComponent>> Parts;

	/** Shapes that follow the orb through the world, like the flames a fire orb drags behind */
	UPROPERTY()
	TArray<TObjectPtr<UStaticMeshComponent>> Trail;

	/** Light of the orb, null for elements that don't shine */
	UPROPERTY()
	TObjectPtr<UPointLightComponent> Light;

	/** Brightness of the light before its flicker, in candela */
	float LightIntensity = 0.0f;

	/** Time the element was chosen */
	float BirthTime = 0.0f;

	/** Place of the orb on the circle in degrees, relative to the first orb; glides to its share of the circle when orbs join */
	float AngleOffset = 0.0f;

	/** Size of the orb: scale of an engine sphere that is as big as its core */
	float Size = 0.15f;
};

/**
 *  Shows the queued elements of a mage as orbs circling above the head, evenly spread over the circle.
 *  Each element has its own look: fire burns and drags flames behind, water wobbles and drips,
 *  air is a ball of whirling wind, earth a crumbling rock.
 *  Placeholder made of engine shapes and two simple materials; Niagara effects can replace the parts later.
 */
UCLASS()
class UVaelElementOrbitComponent : public USceneComponent
{
	GENERATED_BODY()

public:

	/** Constructor */
	UVaelElementOrbitComponent();

	//~Begin UActorComponent
	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	//~End UActorComponent

	/** Shows the queue of the element component: new elements join the circle, a cast or cleared queue empties it */
	void ShowQueue(const UVaelElementComponent& Elements);

protected:

	/** Radius of the circle in cm */
	UPROPERTY(EditAnywhere, Category="Orbit", meta = (ClampMin = 0))
	float OrbitRadius = 40.0f;

	/** Speed of the circling in degrees per second */
	UPROPERTY(EditAnywhere, Category="Orbit")
	float OrbitSpeed = 120.0f;

	/** Diameter of an orb in cm */
	UPROPERTY(EditAnywhere, Category="Orbit", meta = (ClampMin = 1))
	float OrbDiameter = 15.0f;

	/** Elements drawn from the environment are this much bigger */
	UPROPERTY(EditAnywhere, Category="Orbit", meta = (ClampMin = 1))
	float EnvironmentScale = 1.4f;

private:

	/** Builds the shapes of an orb for its element */
	void BuildOrb(FVaelElementOrb& Orb);

	/** Removes the shapes of an orb */
	void DestroyOrb(FVaelElementOrb& Orb);

	/** Removes every orb */
	void ClearOrbs();

	/** Adds a shape to an orb. Without a material the mesh keeps its own. */
	UStaticMeshComponent* AddShape(USceneComponent* Parent, UStaticMesh* Mesh, UMaterialInterface* Material, const FLinearColor& Color, float Glow, float Rim = 0.0f);

	/** Adds the light of an orb */
	void AddLight(FVaelElementOrb& Orb, const FLinearColor& Color, float Intensity);

	/** Moves the shapes of an orb. Grow runs from 0 to 1 while the orb appears and swings a little beyond. */
	void AnimateFire(FVaelElementOrb& Orb, int32 Index, float Time, float DeltaTime, float Grow);
	void AnimateWater(FVaelElementOrb& Orb, int32 Index, float Time, float Grow);
	void AnimateAir(FVaelElementOrb& Orb, int32 Index, float Time, float Grow);
	void AnimateEarth(FVaelElementOrb& Orb, int32 Index, float Time, float Grow);
	void AnimateMark(FVaelElementOrb& Orb, int32 Index, float Time, float Grow);

	/** Orbs of the queued elements, in the order they were chosen */
	UPROPERTY(Transient)
	TArray<FVaelElementOrb> Orbs;

	/** Engine sphere all shapes are made of */
	UPROPERTY()
	TObjectPtr<UStaticMesh> SphereMesh;

	/** Solid, glowing material of the cores; the plain engine material while it doesn't exist */
	UPROPERTY(Transient)
	TObjectPtr<UMaterialInterface> CoreMaterial;

	/** See-through glow of shells, flames and wind; the core material while it doesn't exist */
	UPROPERTY(Transient)
	TObjectPtr<UMaterialInterface> GlowMaterial;

	/** Rock of the earth orb, null while the pack isn't installed: the orb is a brown sphere then */
	UPROPERTY(Transient)
	TObjectPtr<UStaticMesh> RockMesh;

	/** Angle of the first orb on the circle in degrees */
	float OrbitAngle = 0.0f;
};
