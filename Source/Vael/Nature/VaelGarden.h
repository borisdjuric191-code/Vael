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

/** One kind of plant that can grow from the surprise seed of the Keimpelz */
USTRUCT(BlueprintType)
struct FVaelSeedPlant
{
	GENERATED_BODY()

	/** Name in the save and in the code */
	UPROPERTY(config, EditAnywhere, Category="Plant")
	FName Id;

	UPROPERTY(config, EditAnywhere, Category="Plant")
	FText DisplayName;

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

/** A plant grown from a seed a player has sown. It grows up, glows and stays for good; green spreads around it, and grass and moss sprout in it. */
UCLASS()
class AVaelSownPlant : public AActor
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
	static AVaelSownPlant* Plant(UWorld* World, FName PlantId, const FVector& GroundLocation, float Yaw, bool bGrown);

	FName GetPlantId() const { return PlantId; }

	/** How far its green reaches now in cm, and how many tufts stand in it, for tests */
	float GetGreenRadius() const { return GreenRadius; }
	int32 GetNumTufts() const { return NextTuft; }

	/** Lets its tufts within the radius around the location vanish, so they don't hide what was sown there */
	void ClearTuftsNear(const FVector& Location, float Radius);

	virtual void Tick(float DeltaSeconds) override;

protected:

	virtual void BeginPlay() override;

private:

	FName PlantId = NAME_None;

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
