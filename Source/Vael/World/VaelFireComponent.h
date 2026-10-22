// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ProceduralMeshComponent.h"
#include "Engine/DeveloperSettings.h"
#include "VaelFireComponent.generated.h"

class UDecalComponent;
class UMaterialInstanceDynamic;
class UMaterialInterface;
class UPointLightComponent;

/** Values of the fire kit (Feuer-Baukasten): its materials and how many fires may light up their surroundings at once */
UCLASS(config=Game, defaultconfig, meta = (DisplayName = "Vael Fire"))
class UVaelFireSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:

	/** Constructor */
	UVaelFireSettings();

	static const UVaelFireSettings* Get() { return GetDefault<UVaelFireSettings>(); }

	virtual FName GetCategoryName() const override { return TEXT("Game"); }

	/** Flame tongues, sparks and scorch marks, made by Scripts/CreateFireKit.py */
	UPROPERTY(config, EditAnywhere, Category="Materials")
	TSoftObjectPtr<UMaterialInterface> FlameMaterial;

	UPROPERTY(config, EditAnywhere, Category="Materials")
	TSoftObjectPtr<UMaterialInterface> SparkMaterial;

	UPROPERTY(config, EditAnywhere, Category="Materials")
	TSoftObjectPtr<UMaterialInterface> ScorchMaterial;

	UPROPERTY(config, EditAnywhere, Category="Materials")
	TSoftObjectPtr<UMaterialInterface> SmokeMaterial;

	/** Simulated fire played as flipbooks (Tools/Feuer/feuer_simulation.py): a single flame, a broad fire, a burning ball */
	UPROPERTY(config, EditAnywhere, Category="Materials")
	TSoftObjectPtr<UMaterialInterface> FlameFlipbook;

	UPROPERTY(config, EditAnywhere, Category="Materials")
	TSoftObjectPtr<UMaterialInterface> FireFlipbook;

	UPROPERTY(config, EditAnywhere, Category="Materials")
	TSoftObjectPtr<UMaterialInterface> BallFlipbook;

	/** Simulated fire as a real volume (Tools/Feuer/feuer_volumen.py, Scripts/CreateFireVolumes.py): fires on the ground and
	 *  burning balls burn as a body of whirling flame seen from every side instead of cards */
	UPROPERTY(config, EditAnywhere, Category="Volumes")
	bool bUseVolumes = true;

	/** A fire on the ground (SVT_Feuer_Brand) */
	UPROPERTY(config, EditAnywhere, Category="Volumes")
	TSoftObjectPtr<UMaterialInterface> VolumeFire;

	/** A burning ball (SVT_Feuer_Kugel) */
	UPROPERTY(config, EditAnywhere, Category="Volumes")
	TSoftObjectPtr<UMaterialInterface> VolumeBall;

	/** Centimetres per simulated metre for each centimetre of the fire's radius: the simulated fire is half a metre wide in
	 *  radius, the simulated ball a quarter of a metre */
	UPROPERTY(config, EditAnywhere, Category="Volumes", meta = (ClampMin = 0.1))
	float VolumeScalePerRadius = 1.5f;

	UPROPERTY(config, EditAnywhere, Category="Volumes", meta = (ClampMin = 0.1))
	float BallVolumeScalePerRadius = 4.0f;

	/** Height of the foot of the simulated fire above the floor of its domain, in simulated metres */
	UPROPERTY(config, EditAnywhere, Category="Volumes")
	float VolumeFloor = 0.1f;

	/** Scale at which the volume material glows and smokes as it is set; bigger fires are made thinner, smaller ones denser */
	UPROPERTY(config, EditAnywhere, Category="Volumes", meta = (ClampMin = 1))
	float VolumeReferenceScale = 100.0f;

	/** Height of the simulated ball's middle above the ground of its domain, in simulated metres */
	UPROPERTY(config, EditAnywhere, Category="Volumes")
	float BallVolumeCentre = 0.65f;

	/** Frames per second the volume plays at */
	UPROPERTY(config, EditAnywhere, Category="Volumes", meta = (ClampMin = 1))
	float VolumeFrameRate = 24.0f;

	/** Most fires that burn as volumes at once; the others burn as cards */
	UPROPERTY(config, EditAnywhere, Category="Volumes", meta = (ClampMin = 0))
	int32 MaxVolumeFires = 10;

	/** Smaller fires stay cards: a few centimetres of flame show no whirls, and the volumes are kept for the big fires */
	UPROPERTY(config, EditAnywhere, Category="Volumes", meta = (ClampMin = 0))
	float MinVolumeRadius = 30.0f;

	UPROPERTY(config, EditAnywhere, Category="Volumes", meta = (ClampMin = 0))
	float MinBallVolumeRadius = 12.0f;

	/** Most fires that cast light at once; the others burn without */
	UPROPERTY(config, EditAnywhere, Category="Light", meta = (ClampMin = 0))
	int32 MaxLitFires = 12;

	/** Colour of the light of a fire, and its brightness in candela for a fire of 50 cm radius */
	UPROPERTY(config, EditAnywhere, Category="Light")
	FLinearColor LightColor = FLinearColor(1.0f, 0.36f, 0.1f);

	UPROPERTY(config, EditAnywhere, Category="Light", meta = (ClampMin = 0))
	float LightIntensity = 14.0f;

	/** Flame tongues per square metre of fire, and the fewest and most one fire has */
	UPROPERTY(config, EditAnywhere, Category="Flames", meta = (ClampMin = 0.1))
	float TonguesPerSquareMetre = 5.0f;

	UPROPERTY(config, EditAnywhere, Category="Flames", meta = (ClampMin = 1))
	int32 MinTongues = 1;

	UPROPERTY(config, EditAnywhere, Category="Flames", meta = (ClampMin = 1))
	int32 MaxTongues = 16;

	/** Seconds a scorch mark stays after its fire went out, while its embers die down */
	UPROPERTY(config, EditAnywhere, Category="Scorch", meta = (ClampMin = 0))
	float ScorchLinger = 18.0f;
};

/** Shape of one fire */
USTRUCT(BlueprintType)
struct FVaelFireShape
{
	GENERATED_BODY()

	/** Reach of the burning patch in cm: a disc on the ground, or half the width of a fire on a wall */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Fire", meta = (ClampMin = 1))
	float Radius = 40.0f;

	/** Height of the tallest flames in cm */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Fire", meta = (ClampMin = 1))
	float Height = 90.0f;

	/** Heat of the flames: 1 = an ordinary fire, more is whiter and brighter */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Fire", meta = (ClampMin = 0))
	float Heat = 1.0f;

	/** Dark smoke at the tips of the flames */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Fire", meta = (ClampMin = 0))
	float Smoke = 0.6f;

	/** How fast the flames lick upwards */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Fire", meta = (ClampMin = 0))
	float Speed = 1.2f;

	/** Sparks rising out of it, light flickering around it, a scorch mark under it */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Fire")
	bool bSparks = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Fire")
	bool bLight = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Fire")
	bool bScorch = false;

	/** A fire in the air (on a projectile): its feet don't sink into the ground */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Fire")
	bool bFloating = false;

	/** A ball of fire: the tongues sit on a sphere of Radius and lick outwards all around (and along the flow) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Fire")
	bool bBall = false;

	/** How much the flames of a ball lick outwards (1) instead of along the flow direction (0) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Fire", meta = (ClampMin = 0, ClampMax = 1))
	float BallSpread = 0.6f;

	/** Number of flame tongues, 0 to let the size decide */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Fire", meta = (ClampMin = 0))
	int32 Tongues = 0;

	/** Zero for a fire on the ground; for a fire on a wall the direction out of the wall: its tongues line up along the wall and still rise straight up */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Fire")
	FVector WallNormal = FVector::ZeroVector;
};

/**
 *  A living fire made of flame tongues (crossed cards with M_Feuer that waver, lick and snake), rising sparks (M_Feuer_Funken),
 *  a flickering light and an optional scorch mark (M_Feuer_Brandfleck). The flames always rise straight up, whatever the
 *  component is attached to: a patch of ground, a wall, a burning creature. It fades in when lit and out when put out.
 */
UCLASS(ClassGroup=(Vael), meta=(BlueprintSpawnableComponent))
class UVaelFireComponent : public UProceduralMeshComponent
{
	GENERATED_BODY()

public:

	/** Constructor */
	UVaelFireComponent(const FObjectInitializer& ObjectInitializer);

	/** Builds the fire in this shape and lets it flare up over the seconds */
	void Ignite(const FVaelFireShape& InShape, float FadeIn = 0.3f);

	/** Lets the fire die down over the seconds; with bDestroyWhenOut the component goes once flames and scorch mark are gone */
	void Extinguish(float FadeOut = 0.8f, bool bDestroyWhenOut = true);

	/** The way the flames lick, straight up by default; a burning projectile turns them backwards */
	void SetFlowDirection(const FVector& Direction);

	/** Size of the flames compared with their shape, for flames that grow and shrink like the tail of a speeding spark */
	void SetSizeScale(float Scale);

	/** Strength of the fire, 0 to 1, and whether it still burns */
	float GetStrength() const { return Strength; }
	bool IsBurning() const { return TargetStrength > 0.0f || Strength > 0.0f; }

	const FVaelFireShape& GetShape() const { return Shape; }

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

protected:

	virtual void OnUnregister() override;

	/** The cards are unfolded by the material, so the bounds cover the whole fire, not the base points */
	virtual FBoxSphereBounds CalcBounds(const FTransform& LocalToWorld) const override;

private:

	/** Builds the cards of the flames and the sparks */
	void BuildCards();

	/** Takes or gives back one of the lights all fires share */
	void TakeLight();
	void ReleaseLight();

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> FlameMaterial;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> SparkMaterial;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> SmokeMaterial;

	/** The fire as a real volume, when it burns as one */
	UPROPERTY(Transient)
	TObjectPtr<class UHeterogeneousVolumeComponent> Volume;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> VolumeMaterial;

	/** Makes the volume of the fire, if this fire burns as one */
	void BuildVolume();

	/** Gives back the volume, so another fire may burn as one */
	void ReleaseVolume();

	/** Sets the volume on the ground of the fire or around the middle of the ball */
	void PlaceVolume();

	/** Scale of the volume at full size */
	float VolumeScale = 1.0f;

	/** Scale of the volume at its present size */
	float VolumeSize = 1.0f;

	/** Height of the volume in simulated metres, once its first frame told it */
	float VolumeHeight = 0.0f;

	/** The broad fire of the bed under the flames */
	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> BedMaterial;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> ScorchMaterial;

	UPROPERTY(Transient)
	TObjectPtr<UPointLightComponent> Light;

	UPROPERTY(Transient)
	TObjectPtr<UDecalComponent> Scorch;

	FVaelFireShape Shape;

	float Strength = 0.0f;
	float TargetStrength = 0.0f;
	float FadeSpeed = 3.0f;
	bool bDestroyWhenOut = false;

	/** Seconds since the flames went out, while the scorch mark cools and fades */
	float OutTime = -1.0f;

	/** Random start of the flicker of the light */
	float FlickerPhase = 0.0f;

	/** Fires that hold a light now */
	static int32 NumLitFires;

	/** Fires that burn as volumes at the moment */
	static int32 NumVolumeFires;
};
