// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "GameFramework/Actor.h"
#include "Player/VaelInteractable.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Tickable.h"
#include "VaelGarden.generated.h"

class AVaelCharacter;
class AVaelSownPlant;
class UDecalComponent;
class UInstancedStaticMeshComponent;
class UMaterialInterface;
class UPointLightComponent;
class UStaticMesh;
class UStaticMeshComponent;
class UVaelMaterial;
class UVaelSaveGame;

/** How rare a seed plant is (Herbarium: 4 common, 3 special, 2 rare, 1 legendary per region) */
UENUM(BlueprintType)
enum class EVaelPlantRarity : uint8
{
	Common		UMETA(DisplayName = "häufig"),
	Special		UMETA(DisplayName = "besonders"),
	Rare		UMETA(DisplayName = "selten"),
	Legendary	UMETA(DisplayName = "legendär")
};

/** One kind of plant that can grow from the surprise seed of the Keimpelz */
USTRUCT(BlueprintType)
struct FVaelSeedPlant
{
	GENERATED_BODY()

	/** Name in the save and in the code */
	UPROPERTY(config, EditAnywhere, Category="Plant")
	FName Id;

	/** Name of the light form */
	UPROPERTY(config, EditAnywhere, Category="Plant")
	FText DisplayName;

	/** Name of the shadow form, which grows when a marked player sows it; empty if the plant has none */
	UPROPERTY(config, EditAnywhere, Category="Plant")
	FText ShadowName;

	UPROPERTY(config, EditAnywhere, Category="Plant")
	EVaelPlantRarity Rarity = EVaelPlantRarity::Common;

	/** What one harvest gives of the light form and of the shadow form, one of each */
	UPROPERTY(config, EditAnywhere, Category="Plant")
	TArray<TSoftObjectPtr<UVaelMaterial>> LightHarvest;

	UPROPERTY(config, EditAnywhere, Category="Plant")
	TArray<TSoftObjectPtr<UVaelMaterial>> ShadowHarvest;

	bool HasShadowForm() const { return !ShadowName.IsEmpty(); }

	/** Model of the creature factory (mode "requisite") */
	UPROPERTY(config, EditAnywhere, Category="Plant")
	TSoftObjectPtr<UStaticMesh> Mesh;

	/** Its own seed, which players get once Albrun has researched the plant */
	UPROPERTY(config, EditAnywhere, Category="Plant")
	TSoftObjectPtr<UVaelMaterial> Seed;

	/** Share of the surprise seeds that turn into this plant, weighed against the others */
	UPROPERTY(config, EditAnywhere, Category="Plant", meta = (ClampMin = 0))
	float Chance = 1.0f;

	/** Color and strength of its light */
	UPROPERTY(config, EditAnywhere, Category="Plant")
	FLinearColor GlowColor = FLinearColor::White;

	UPROPERTY(config, EditAnywhere, Category="Plant", meta = (ClampMin = 0))
	float GlowStrength = 1800.0f;
};

/** A plant a player has sown, as it is kept in the save */
USTRUCT()
struct FVaelSavedPlant
{
	GENERATED_BODY()

	/** Name of the map it stands in */
	UPROPERTY()
	FName Level;

	UPROPERTY()
	FName Plant;

	UPROPERTY()
	FVector Location = FVector::ZeroVector;

	UPROPERTY()
	float Yaw = 0.0f;

	/** Garden clock at which it was sown, in seconds */
	UPROPERTY()
	float SownAt = 0.0f;

	/** Grew in its shadow form, as the one who sowed it was marked */
	UPROPERTY()
	bool bShadow = false;

	/** Has been harvested; a plant carries only once */
	UPROPERTY()
	bool bHarvested = false;
};

/**
 *  Rules of the seed garden: which plants the surprise seed can become, how long Albrun's pot takes and how sowing works.
 *  Edited under Project Settings > Game > Vael Garden, stored in DefaultGame.ini.
 */
UCLASS(config=Game, defaultconfig, meta = (DisplayName = "Vael Garden"))
class UVaelGardenSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:

	UVaelGardenSettings();

	static const UVaelGardenSettings* Get() { return GetDefault<UVaelGardenSettings>(); }

	virtual FName GetCategoryName() const override { return TEXT("Game"); }

	/** The seed of the Keimpelz: nobody knows what grows from it before Albrun has raised it */
	UPROPERTY(config, EditAnywhere, Category="Pot")
	TSoftObjectPtr<UVaelMaterial> SurpriseSeed;

	/** Seconds a surprise seed grows in the pot */
	UPROPERTY(config, EditAnywhere, Category="Pot", meta = (ClampMin = 1))
	float PotSeconds = 120.0f;

	/** Seeds of the plant the group gets when Albrun has looked at what grew */
	UPROPERTY(config, EditAnywhere, Category="Pot", meta = (ClampMin = 1))
	int32 SeedsPerStudy = 3;

	/** Name of the person the pot stands beside, and where it stands seen from them, in cm */
	UPROPERTY(config, EditAnywhere, Category="Pot")
	FString PotKeeper = TEXT("Albrun");

	UPROPERTY(config, EditAnywhere, Category="Pot")
	FVector PotOffset = FVector(40.0f, 150.0f, 0.0f);

	/** Seconds the button is held to sow a seed */
	UPROPERTY(config, EditAnywhere, Category="Sowing", meta = (ClampMin = 0.1))
	float SowHoldSeconds = 0.6f;

	/** A seed lands this far in front of the player, and sown plants keep this far apart, in cm */
	UPROPERTY(config, EditAnywhere, Category="Sowing", meta = (ClampMin = 0))
	float SowDistance = 90.0f;

	UPROPERTY(config, EditAnywhere, Category="Sowing", meta = (ClampMin = 0))
	float SowSpacing = 130.0f;

	/** Seconds a sown plant takes to grow up */
	UPROPERTY(config, EditAnywhere, Category="Sowing", meta = (ClampMin = 0.1))
	float GrowSeconds = 6.0f;

	/** A region is clean once plants stand this many cm apart all over it: every sown plant takes its share of that from the
	 *  corruption of its region, once. A plant sown closer to another one takes less, as it covers less new ground. */
	UPROPERTY(config, EditAnywhere, Category="Sowing", meta = (ClampMin = 100))
	float CleanseSpacing = 800.0f;

	/** Corruption a sown plant takes from a region without bounds, which has no area to reckon with */
	UPROPERTY(config, EditAnywhere, Category="Sowing", meta = (ClampMin = 0))
	float CleansePerPlant = 0.8f;

	/** The seed takes on what is in the one who sows it: below this much of their own corruption the light form grows,
	 *  above the second the shadow form, in between either at even odds */
	UPROPERTY(config, EditAnywhere, Category="Forms", meta = (ClampMin = 0, ClampMax = 100))
	float LightBelowCorruption = 30.0f;

	UPROPERTY(config, EditAnywhere, Category="Forms", meta = (ClampMin = 0, ClampMax = 100))
	float ShadowAboveCorruption = 70.0f;

	/** Laid over the model and the grass of a shadow form: a modulating material with the parameters Color and ColorEdge */
	UPROPERTY(config, EditAnywhere, Category="Forms")
	TSoftObjectPtr<UMaterialInterface> ShadowOverlay;

	/** The marked land around a shadow form: near black at the ground, violet above, and the light of the plant */
	UPROPERTY(config, EditAnywhere, Category="Forms")
	FLinearColor ShadowGreenColor = FLinearColor(0.02f, 0.004f, 0.03f);

	UPROPERTY(config, EditAnywhere, Category="Forms")
	FLinearColor ShadowGreenColorLight = FLinearColor(0.16f, 0.02f, 0.10f);

	UPROPERTY(config, EditAnywhere, Category="Forms")
	FLinearColor ShadowGlowColor = FLinearColor(0.6f, 0.08f, 0.85f);

	/** Chance that a harvest gives a seed back, for common and for special plants (rarer ones never do) */
	UPROPERTY(config, EditAnywhere, Category="Harvest", meta = (ClampMin = 0, ClampMax = 1))
	float SeedReturnCommon = 0.5f;

	UPROPERTY(config, EditAnywhere, Category="Harvest", meta = (ClampMin = 0, ClampMax = 1))
	float SeedReturnSpecial = 0.25f;

	/** Luck of the light way: a fairy-kissed harvest glitters and gives twice */
	UPROPERTY(config, EditAnywhere, Category="Harvest", meta = (ClampMin = 0, ClampMax = 1))
	float FairyKissChance = 0.1f;

	/** Risk of the shadow way: harvesting a shadow form may sting, and the harvester's own corruption grows by this much */
	UPROPERTY(config, EditAnywhere, Category="Harvest", meta = (ClampMin = 0, ClampMax = 1))
	float MarkStingChance = 0.1f;

	UPROPERTY(config, EditAnywhere, Category="Harvest", meta = (ClampMin = 0))
	float MarkStingCorruption = 1.0f;

	/** A harvested plant stays standing this much smaller and with this share of its light; it never carries again */
	UPROPERTY(config, EditAnywhere, Category="Harvest", meta = (ClampMin = 0.1, ClampMax = 1))
	float HarvestedScale = 0.7f;

	UPROPERTY(config, EditAnywhere, Category="Harvest", meta = (ClampMin = 0, ClampMax = 1))
	float HarvestedGlow = 0.25f;

	/** The plants a surprise seed can become */
	UPROPERTY(config, EditAnywhere, Category="Plants")
	TArray<FVaelSeedPlant> Plants;

	/** Ground cover that spreads around every sown plant (a decal with the parameters Color and ColorLight) */
	UPROPERTY(config, EditAnywhere, Category="Green")
	TSoftObjectPtr<UMaterialInterface> GreenMaterial;

	/** Radius of the green in cm: right after sowing, after the quick seconds, and after the slow seconds on top of them */
	UPROPERTY(config, EditAnywhere, Category="Green", meta = (ClampMin = 0))
	float GreenStartRadius = 120.0f;

	UPROPERTY(config, EditAnywhere, Category="Green", meta = (ClampMin = 0))
	float GreenQuickRadius = 320.0f;

	UPROPERTY(config, EditAnywhere, Category="Green", meta = (ClampMin = 1))
	float GreenQuickSeconds = 40.0f;

	UPROPERTY(config, EditAnywhere, Category="Green", meta = (ClampMin = 0))
	float GreenFullRadius = 520.0f;

	UPROPERTY(config, EditAnywhere, Category="Green", meta = (ClampMin = 1))
	float GreenSlowSeconds = 900.0f;

	/** Plants closer together than this many cm are neighbours: every neighbour lets the green reach this share further,
	 *  up to the most neighbours that count, so plants sown near each other soon grow into one oasis */
	UPROPERTY(config, EditAnywhere, Category="Green", meta = (ClampMin = 0))
	float GreenLinkDistance = 1100.0f;

	UPROPERTY(config, EditAnywhere, Category="Green", meta = (ClampMin = 0))
	float GreenNeighbourBonus = 0.35f;

	UPROPERTY(config, EditAnywhere, Category="Green", meta = (ClampMin = 0))
	int32 GreenMaxNeighbours = 5;

	/** Grass and moss that sprout in the green, this many per square meter, at most this many around one plant */
	UPROPERTY(config, EditAnywhere, Category="Green")
	TArray<TSoftObjectPtr<UStaticMesh>> GreenMeshes;

	UPROPERTY(config, EditAnywhere, Category="Green", meta = (ClampMin = 0))
	float GreenTuftsPerSquareMeter = 0.4f;

	UPROPERTY(config, EditAnywhere, Category="Green", meta = (ClampMin = 0))
	int32 GreenMaxTufts = 260;

	/** Width of a tuft in cm, between these two; every model is sized to it, whatever size it was made in */
	UPROPERTY(config, EditAnywhere, Category="Green")
	FVector2D GreenTuftWidth = FVector2D(35.0f, 70.0f);
};

/**
 *  The seed garden of the group: which plants Albrun has researched, what grows in the pot and what the players have sown.
 *  Surprise seeds of the Keimpelz go into Albrun's pot; once a plant is researched its own seeds can be sown anywhere,
 *  and what was sown stays and is kept in the save.
 */
UCLASS()
class UVaelGardenSubsystem : public UGameInstanceSubsystem, public FTickableGameObject
{
	GENERATED_BODY()

public:

	/** The garden of the world of the object, null outside the game */
	static UVaelGardenSubsystem* Get(const UObject* WorldContext);

	/** The plant of that name, null if there is none */
	static const FVaelSeedPlant* FindPlant(FName PlantId);

	bool IsResearched(FName PlantId) const { return Researched.Contains(PlantId); }
	int32 GetNumResearched() const { return Researched.Num(); }
	int32 GetNumSown() const { return Sown.Num(); }

	/** What a surprise seed becomes: a kind nobody has researched yet while there is one, so chance never blocks for long */
	FName PickSurprise() const;

	/** Albrun has looked at a grown plant: it is researched from now on and the group gets its seeds */
	void Research(FName PlantId, const AActor* Source);

	/** True if the player carries a seed of a researched plant */
	bool CanSow(const AVaelCharacter* Player) const;

	/** Sows one seed of a researched plant in front of the player. Returns null if they have none or there is no room. */
	AVaelSownPlant* Sow(AVaelCharacter* Player);

	/** The form a seed takes in the hands of a sower with this much corruption of their own: true for the shadow form */
	static bool RollShadowForm(float SowerCorruption);

	/** Harvests a grown plant for the group: its ingredients, perhaps a seed back, and the luck or risk of its way.
	 *  Returns false if it can't be harvested (still growing, or harvested before). */
	bool Harvest(AVaelSownPlant* Plant, AVaelCharacter* Player);

	/** Number of sown plants in their shadow form and of harvested ones, for tests */
	int32 GetNumShadow() const;
	int32 GetNumHarvested() const;

	/** A level has started: puts the pot beside its keeper and lets the plants sown there stand again */
	void OnLevelStarted(UWorld* World);

	/** How far the green reaches around the plant sown at the location now, in cm: it grows with the time since sowing and with every neighbour */
	float GetGreenRadius(const UWorld* World, const FVector& PlantLocation) const;

	/** The furthest the green of one plant can ever reach, in cm */
	static float GetMaxGreenRadius();

	//~Begin FTickableGameObject
	virtual void Tick(float DeltaTime) override;
	virtual bool IsTickable() const override { return !IsTemplate(); }
	virtual UWorld* GetTickableGameObjectWorld() const override;
	virtual TStatId GetStatId() const override { RETURN_QUICK_DECLARE_CYCLE_STAT(UVaelGardenSubsystem, STATGROUP_Tickables); }
	//~End FTickableGameObject

	/** What grows in the pot, none while it is empty, and the seconds it still needs */
	FName PotPlant = NAME_None;
	float PotSecondsLeft = 0.0f;

	void WriteSave(UVaelSaveGame& Save) const;
	void ReadSave(const UVaelSaveGame& Save);

private:

	/** Whatever appears in the level while playing stays free of the green on the ground */
	void OnActorSpawned(AActor* Actor);

	/** The saved entry of the plant standing at the location in that world, null if there is none */
	FVaelSavedPlant* FindSown(const UWorld* World, const FVector& Location);

	TSet<FName> Researched;
	TArray<FVaelSavedPlant> Sown;

	/** Seconds the garden has been played */
	float Clock = 0.0f;
};

/** What Albrun's pot holds */
UENUM(BlueprintType)
enum class EVaelPotState : uint8
{
	Empty,
	/** A surprise seed grows; nobody can tell yet what it becomes */
	Growing,
	/** Grown: the plant shows itself and waits to be looked at */
	Ready
};

/**
 *  Albrun's pot: a surprise seed of the Keimpelz is planted here, grows for a while and shows what it is.
 *  Looking at the grown plant researches it and gives the group its seeds.
 */
UCLASS()
class AVaelSeedPot : public AActor, public IVaelInteractable
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UStaticMeshComponent> Pot;

	/** The shoot nobody can name yet */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UStaticMeshComponent> Sprout;

	/** The grown plant */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UStaticMeshComponent> Plant;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UPointLightComponent> Glow;

public:

	AVaelSeedPot();

	//~Begin IVaelInteractable
	virtual bool CanInteract(const AVaelCharacter* Player) const override;
	virtual void Interact(AVaelCharacter* Player) override;
	virtual FText GetInteractPrompt() const override;
	//~End IVaelInteractable

	EVaelPotState GetPotState() const;

	/** Lets what grows be grown at once, for tests */
	void FinishGrowing();

	virtual void Tick(float DeltaSeconds) override;

protected:

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:

	/** Shows the model of what has grown */
	void RefreshPlant();

	FName ShownPlant = NAME_None;
};

/** A tuft of grass or moss that is still growing up in the green of a plant */
struct FVaelGrowingTuft
{
	int32 Mesh = 0;
	int32 Instance = 0;
	float Age = 0.0f;
	FTransform Grown;
};

/** A place where a tuft sprouts once the green has reached it */
struct FVaelTuftSpot
{
	FVector2D Offset = FVector2D::ZeroVector;
	float Distance = 0.0f;
	float Yaw = 0.0f;
	float Scale = 1.0f;
	int32 Mesh = 0;
};

/**
 *  A plant grown from a seed a player has sown. It grows up, glows and stays for good; green spreads around it, and grass and moss sprout in it.
 *  In its shadow form it is violet and the land around it turns marked instead of green. Once grown it can be harvested a single time.
 */
UCLASS()
class AVaelSownPlant : public AActor, public IVaelInteractable
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UStaticMeshComponent> Model;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UPointLightComponent> Glow;

	/** The green ground around it */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UDecalComponent> Green;

	/** Grass and moss in the green, one component per model */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UInstancedStaticMeshComponent>> Tufts;

public:

	AVaelSownPlant();

	/** Lets a plant stand at a place on the ground; grown at once if it was sown in an earlier session */
	static AVaelSownPlant* Plant(UWorld* World, FName PlantId, const FVector& GroundLocation, float Yaw, bool bGrown, bool bShadow = false, bool bHarvested = false);

	FName GetPlantId() const { return PlantId; }
	bool IsShadowForm() const { return bShadow; }
	bool IsHarvested() const { return bHarvested; }
	bool IsGrown() const;

	/** Name of the form that grew */
	FText GetFormName() const;

	/** It has been harvested: it stays, smaller and dimmer, and never carries again */
	void MarkHarvested();

	//~Begin IVaelInteractable
	virtual bool CanInteract(const AVaelCharacter* Player) const override;
	virtual void Interact(AVaelCharacter* Player) override;
	virtual FText GetInteractPrompt() const override;
	//~End IVaelInteractable

	/** How far its green reaches now in cm, and how many tufts stand in it, for tests */
	float GetGreenRadius() const { return GreenRadius; }
	int32 GetNumTufts() const { return NextTuft; }

	/** Lets its tufts within the radius around the location vanish, so they don't hide what was sown there */
	void ClearTuftsNear(const FVector& Location, float Radius);

	virtual void Tick(float DeltaSeconds) override;

protected:

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:

	FName PlantId = NAME_None;
	bool bShadow = false;
	bool bHarvested = false;

	/** How far it has settled into its harvested look, 0 to 1 */
	float SpentShare = 0.0f;

	/** Seconds since it was sown */
	float Age = 0.0f;

	float GlowStrength = 0.0f;
	float SizeScale = 1.0f;
	float BaseYaw = 0.0f;

	/** Lets the green follow the garden and tufts sprout where it has arrived */
	void UpdateGreen(float DeltaSeconds);

	/** Radius of the green as shown, in cm, the radius it grows towards and the seconds until the garden is asked again */
	float GreenRadius = 0.0f;
	float GreenTarget = 0.0f;
	float GreenAskCountdown = 0.0f;

	/** True until the first look if it stood here before the session: its green and tufts are there at once */
	bool bInstantGreen = false;

	/** Where tufts can sprout, from the plant outwards, and how many of them have */
	TArray<FVaelTuftSpot> TuftSpots;
	int32 NextTuft = 0;

	TArray<FVaelGrowingTuft> GrowingTufts;

	/** Width of each tuft model as it was made, in cm */
	TArray<float> TuftModelWidths;
};
