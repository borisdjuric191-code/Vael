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

/** One simulated fire played as a volume */
USTRUCT(BlueprintType)
struct FVaelFireVolumeKind
{
	GENERATED_BODY()

	/** Volume material instance with its Sparse Volume Texture (MI_Feuer_Volumen_<Kind>) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Fire")
	TSoftObjectPtr<UMaterialInterface> Material;

	/** Radius of the burning bed in the simulation, in simulated metres: a fire of radius R cm is scaled to R / BedRadius */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Fire", meta = (ClampMin = 0.01))
	float BedRadius = 0.5f;

	/** Biggest fire in cm of radius shown with this stage; bigger fires blend into the next one */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Fire", meta = (ClampMin = 0))
	float UpToRadius = 100000.0f;
};

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

	/** Fire built by Boris in Niagara: when set, every fire on the ground or a wall (GroundNiagara) and every burning ball
	 *  (BallNiagara) burns as this system instead of the tongues. It gets the user parameters Radius and Hoehe (cm) and Staerke
	 *  (0..1, fades in and out); the system is spawned at the foot of the fire and points up */
	UPROPERTY(config, EditAnywhere, Category="Niagara")
	TSoftObjectPtr<class UNiagaraSystem> GroundNiagara;

	UPROPERTY(config, EditAnywhere, Category="Niagara")
	TSoftObjectPtr<class UNiagaraSystem> BallNiagara;

	/** For fire spells: the burning trail of a projectile (Funke, Feuerball) and the blast where something burning explodes.
	 *  The trail system is attached to the projectile and follows it */
	UPROPERTY(config, EditAnywhere, Category="Niagara")
	TSoftObjectPtr<class UNiagaraSystem> TrailNiagara;

	UPROPERTY(config, EditAnywhere, Category="Niagara")
	TSoftObjectPtr<class UNiagaraSystem> BurstNiagara;

	/** For pack systems that ignore Radius/Hoehe: the fire radius (cm) at which the system looks right unscaled.
	 *  The Niagara component is scaled by Radius / this; 0 = never scale (own systems read Radius themselves) */
	UPROPERTY(config, EditAnywhere, Category="Niagara", meta=(ClampMin="0"))
	float NiagaraReferenceRadius = 0.0f;

	UPROPERTY(config, EditAnywhere, Category="Niagara", meta=(ClampMin="0.05"))
	FVector2D NiagaraScaleRange = FVector2D(0.3f, 4.0f);

	/** Flames as 3D tongues (Tools/Feuer/feuer_zunge.py, Scripts/CreateFireTongues.py): meshes that lick, sway and tear off at
	 *  the tip. A Brandherd grows them along the arms of an invisible star, a campfire burns as a ring, a ball all around */
	UPROPERTY(config, EditAnywhere, Category="Tongues")
	bool bUseTongues = true;

	UPROPERTY(config, EditAnywhere, Category="Tongues")
	TArray<TSoftObjectPtr<class UStaticMesh>> TongueMeshes;

	UPROPERTY(config, EditAnywhere, Category="Tongues")
	TSoftObjectPtr<UMaterialInterface> TongueMaterial;

	/** Arms of the star of a Brandherd, and the space between two tongues along an arm compared with the radius (at least
	 *  the given cm) */
	UPROPERTY(config, EditAnywhere, Category="Tongues", meta = (ClampMin = 1))
	int32 StarArms = 6;

	UPROPERTY(config, EditAnywhere, Category="Tongues", meta = (ClampMin = 0.05))
	float TongueSpacing = 0.24f;

	UPROPERTY(config, EditAnywhere, Category="Tongues", meta = (ClampMin = 1))
	float MinTongueSpacing = 14.0f;

	/** Seconds a tongue lives, before a new one grows in its place (shortest and longest) */
	UPROPERTY(config, EditAnywhere, Category="Tongues", meta = (ClampMin = 0.1))
	float TongueLifeMin = 0.9f;

	UPROPERTY(config, EditAnywhere, Category="Tongues", meta = (ClampMin = 0.1))
	float TongueLifeMax = 1.7f;

	/** Width of a tongue compared with its height (the mesh is 30 cm wide for 100 cm) */
	UPROPERTY(config, EditAnywhere, Category="Tongues", meta = (ClampMin = 0.1))
	float TongueWidth = 1.7f;

	/** Flames, smoke and ember bed drawn on cards, the old 2D fire; off, the cards keep only the sparks */
	UPROPERTY(config, EditAnywhere, Category="Materials")
	bool bCardFlames = false;

	/** Simulated fire as a real volume (Tools/Feuer/feuer_volumen.py, Scripts/CreateFireVolumes.py): fires on the ground and
	 *  burning balls burn as a body of whirling flame seen from every side instead of cards */
	UPROPERTY(config, EditAnywhere, Category="Volumes")
	bool bUseVolumes = false;

	/** The stages a fire on the ground burns in, smallest first: embers, a fire, a big fire. A fire that grows past the
	 *  radius of a stage blends into the next one */
	UPROPERTY(config, EditAnywhere, Category="Volumes")
	TArray<FVaelFireVolumeKind> GroundVolumes;

	/** A burning ball (SVT_Feuer_Kugel) */
	UPROPERTY(config, EditAnywhere, Category="Volumes")
	FVaelFireVolumeKind BallVolume;

	/** A campfire, a fire placed in a level: a few logs over embers, burning calmly (SVT_Feuer_Lager) */
	UPROPERTY(config, EditAnywhere, Category="Volumes")
	FVaelFireVolumeKind CampfireVolume;

	/** Height of the foot of the simulated fire above the floor of its domain, in simulated metres */
	UPROPERTY(config, EditAnywhere, Category="Volumes")
	float VolumeFloor = 0.1f;

	/** Height of the simulated ball's middle above the floor of its domain, in simulated metres */
	UPROPERTY(config, EditAnywhere, Category="Volumes")
	float BallVolumeCentre = 0.65f;

	/** Scale at which the volume material glows and smokes as it is set; bigger fires are made thinner, smaller ones denser */
	UPROPERTY(config, EditAnywhere, Category="Volumes", meta = (ClampMin = 1))
	float VolumeReferenceScale = 100.0f;

	/** Frames per second the volume plays at: a little slower than simulated, so the fire burns calmly */
	UPROPERTY(config, EditAnywhere, Category="Volumes", meta = (ClampMin = 1))
	float VolumeFrameRate = 16.0f;

	/** Seconds a fire takes to blend from one stage into the next */
	UPROPERTY(config, EditAnywhere, Category="Volumes", meta = (ClampMin = 0.05))
	float StageBlendSeconds = 1.2f;

	/** Most fires that burn as volumes at once; the others burn as cards */
	UPROPERTY(config, EditAnywhere, Category="Volumes", meta = (ClampMin = 0))
	int32 MaxVolumeFires = 16;

	/** Smaller fires stay cards: a few centimetres of flame show no whirls, and the volumes are kept for the big fires */
	UPROPERTY(config, EditAnywhere, Category="Volumes", meta = (ClampMin = 0))
	float MinVolumeRadius = 6.0f;

	UPROPERTY(config, EditAnywhere, Category="Volumes", meta = (ClampMin = 0))
	float MinBallVolumeRadius = 5.0f;

	/** Test of a fire simulated while the game runs (NiagaraFluids 3D gas, Scripts/CreateLiveFire.py), for fires with bLive */
	UPROPERTY(config, EditAnywhere, Category="Live")
	TSoftObjectPtr<class UNiagaraSystem> LiveFireSystem;

	/** Width of the simulated space compared with the radius of the fire, and cells of its grid along the longest side */
	UPROPERTY(config, EditAnywhere, Category="Live", meta = (ClampMin = 1))
	float LiveSizePerRadius = 4.0f;

	UPROPERTY(config, EditAnywhere, Category="Live", meta = (ClampMin = 16))
	int32 LiveResolution = 96;

	/** The charred shadow of the roots under a fire on the ground (M_Feuer_Brandschatten): it creeps outwards from the middle */
	UPROPERTY(config, EditAnywhere, Category="Roots")
	TSoftObjectPtr<UMaterialInterface> RootMaterial;

	/** How far the roots reach compared with the radius of the fire */
	UPROPERTY(config, EditAnywhere, Category="Roots", meta = (ClampMin = 0.5))
	float RootReach = 1.6f;

	/** Seconds the roots take to creep out all the way while the fire is fed */
	UPROPERTY(config, EditAnywhere, Category="Roots", meta = (ClampMin = 0.1))
	float RootGrowSeconds = 7.0f;

	/** Seconds after the last feeding over which the creeping slows down to a halt */
	UPROPERTY(config, EditAnywhere, Category="Roots", meta = (ClampMin = 0.1))
	float RootSlowdownSeconds = 4.0f;

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

	/** A Brandherd: fire that lands on a burning patch feeds it instead of burning beside it. The patch grows by the area of
	 *  the fire it takes in (this share of it), up to the biggest radius in cm */
	UPROPERTY(config, EditAnywhere, Category="Brandherd", meta = (ClampMin = 0))
	float FeedShare = 0.5f;

	UPROPERTY(config, EditAnywhere, Category="Brandherd", meta = (ClampMin = 1))
	float MaxHerdRadius = 320.0f;

	/** How fast it grows towards its new size, in cm of radius per second */
	UPROPERTY(config, EditAnywhere, Category="Brandherd", meta = (ClampMin = 1))
	float HerdGrowSpeed = 60.0f;

	/** Seconds it keeps its size after the last feeding; then it shrinks slowly (cm per second) to this share of the size it
	 *  was lit with, and burns down calmly */
	UPROPERTY(config, EditAnywhere, Category="Brandherd", meta = (ClampMin = 0))
	float HerdHoldSeconds = 6.0f;

	UPROPERTY(config, EditAnywhere, Category="Brandherd", meta = (ClampMin = 0))
	float HerdShrinkSpeed = 8.0f;

	UPROPERTY(config, EditAnywhere, Category="Brandherd", meta = (ClampMin = 0, ClampMax = 1))
	float HerdShrinkTo = 0.6f;
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

	/** A blast of flames where something burning explodes (AVaelFireSpot::SpawnBurst) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Fire")
	bool bBurst = false;

	/** A campfire: burns as the calm campfire simulation, not as a Brandherd */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Fire")
	bool bCampfire = false;

	/** Simulated live while the game runs (a test of NiagaraFluids) instead of a baked volume */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Fire")
	bool bLive = false;

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

	/** Grows or shrinks the burning patch to this radius in cm: the flames, the roots under them and the light grow with it,
	 *  and a fire on the ground passes into its next stage (embers, fire, big fire) when it outgrows one */
	void SetRadius(float NewRadius);

	/** Fire was added: the roots under it creep on at full pace, and slow down again when nothing more comes */
	void Feed();

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

	/** The volume of the stage the fire grew out of, while it fades away */
	UPROPERTY(Transient)
	TObjectPtr<class UHeterogeneousVolumeComponent> OldVolume;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> OldVolumeMaterial;

	/** One instanced mesh per kind of tongue */
	UPROPERTY(Transient)
	TArray<TObjectPtr<class UInstancedStaticMeshComponent>> Tongues;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> TongueMaterial;

	/** Where a tongue stands: its foot, the way it points, its full height, which mesh, and how far along an arm of the star
	 *  it stands (it shows once the fire has crept that far; 0 for always) */
	struct FTongue
	{
		FVector Foot = FVector::ZeroVector;
		FVector Up = FVector::UpVector;
		float Height = 100.0f;
		float Yaw = 0.0f;
		int32 Mesh = 0;
		float Along = 0.0f;

		/** Its life: a tongue grows out of the ground, dances, its tip tears off and rises, it fades, and a new one grows in its
		 *  place, a little apart and turned another way */
		float Age = 0.0f;
		float Life = 1.0f;
		float Size = 1.0f;
		FVector Shift = FVector::ZeroVector;
	};
	TArray<FTongue> TongueSpots;

	/** Lays out the tongues for the shape, and lets them live: where the fire has reached, each in its own time */
	void BuildTongues();
	void UpdateTongues(float DeltaTime = 0.0f);

	/** Growth the tongues were last put for */
	float TonguesGrowth = -1.0f;

	/** Boris' Niagara fire, when the settings name one */
	UPROPERTY(Transient)
	TObjectPtr<class UNiagaraComponent> NiagaraFire;

	/** Starts Boris' Niagara fire, true if it burns */
	bool BuildNiagaraFire();

	/** The live simulated fire, for a fire with bLive */
	UPROPERTY(Transient)
	TObjectPtr<class UNiagaraComponent> LiveFire;

	/** Starts the live simulated fire, true if it runs */
	bool BuildLiveFire();

	/** Makes the volume of the fire, if this fire burns as one, or changes it to the stage that fits its size */
	void BuildVolume();

	/** Makes a volume of one kind of simulated fire, nullptr if there is none */
	class UHeterogeneousVolumeComponent* MakeVolume(const FVaelFireVolumeKind& Kind);

	/** Gives back the volumes, so other fires may burn as ones */
	void ReleaseVolume();

	/** Sets the volume on the ground of the fire or around the middle of the ball */
	void PlaceVolume(class UHeterogeneousVolumeComponent* Which, float Size, float& KnownHeight);

	/** The kind of simulated fire that fits the shape now: the ball, or a stage on the ground; nullptr for none */
	const FVaelFireVolumeKind* PickVolumeKind(int32& OutIndex) const;

	/** Stage the volume shows (index into GroundVolumes, INDEX_NONE for the ball or no volume) */
	int32 VolumeStage = INDEX_NONE;

	/** How far the new volume has blended in over the old one, 0 to 1 */
	float VolumeBlend = 1.0f;

	/** Simulated metres of the bed of the kind shown: the volume's scale is the radius over it */
	float VolumeBedRadius = 0.5f;
	float OldVolumeSize = 1.0f;

	/** Scale of the volume at full size */
	float VolumeScale = 1.0f;

	/** Scale of the volume at its present size */
	float VolumeSize = 1.0f;

	/** Present size compared with the shape, as SetSizeScale set it */
	float SizeScale = 1.0f;

	/** Height of the volumes in simulated metres, once their first frame told it */
	float VolumeHeight = 0.0f;
	float OldVolumeHeight = 0.0f;

	/** How far the burning roots have crept out (0 to 1), and seconds since the fire was last fed */
	float RootGrowth = 0.0f;
	float SinceFed = 0.0f;

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
