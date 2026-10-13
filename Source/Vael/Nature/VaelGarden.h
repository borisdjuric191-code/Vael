// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "GameFramework/Actor.h"
#include "Player/VaelInteractable.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "VaelGarden.generated.h"

class AVaelCharacter;
class AVaelSownPlant;
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

	/** Corruption every sown plant takes from its region, once */
	UPROPERTY(config, EditAnywhere, Category="Sowing", meta = (ClampMin = 0))
	float CleansePerPlant = 0.3f;

	/** The plants a surprise seed can become */
	UPROPERTY(config, EditAnywhere, Category="Plants")
	TArray<FVaelSeedPlant> Plants;
};

/**
 *  The seed garden of the group: which plants Albrun has researched, what grows in the pot and what the players have sown.
 *  Surprise seeds of the Keimpelz go into Albrun's pot; once a plant is researched its own seeds can be sown anywhere,
 *  and what was sown stays and is kept in the save.
 */
UCLASS()
class UVaelGardenSubsystem : public UGameInstanceSubsystem
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

	/** What grows in the pot, none while it is empty, and the seconds it still needs */
	FName PotPlant = NAME_None;
	float PotSecondsLeft = 0.0f;

	void WriteSave(UVaelSaveGame& Save) const;
	void ReadSave(const UVaelSaveGame& Save);

private:

	TSet<FName> Researched;
	TArray<FVaelSavedPlant> Sown;
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

/** A plant grown from a seed a player has sown. It grows up, glows and stays for good. */
UCLASS()
class AVaelSownPlant : public AActor
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UStaticMeshComponent> Model;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UPointLightComponent> Glow;

public:

	AVaelSownPlant();

	/** Lets a plant stand at a place on the ground; grown at once if it was sown in an earlier session */
	static AVaelSownPlant* Plant(UWorld* World, FName PlantId, const FVector& GroundLocation, float Yaw, bool bGrown);

	FName GetPlantId() const { return PlantId; }

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
};
