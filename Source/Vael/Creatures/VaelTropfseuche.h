// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Engine/DeveloperSettings.h"
#include "Magic/VaelElementTypes.h"
#include "Nature/VaelGarden.h"
#include "VaelTropfseuche.generated.h"

class AVaelCharacter;
class UMaterialInstanceDynamic;
class UMaterialInterface;
class UMeshComponent;
class UStaticMesh;
class UStaticMeshComponent;
class USkeletalMeshComponent;

/**
 *  The strains of the Tropfseuche, the bacteria that settled in the sleeping, weakened Urdrachin where the caves of the
 *  Eisenschlund suit them (Boris and team, 2026-11-21). They look like real snottites and take over whom they infest like
 *  Cordyceps; every strain raises the dead. The rarer, the stronger and the more aggressive.
 */
UENUM(BlueprintType)
enum class EVaelSeuchenStamm : uint8
{
	/** Green, common: its hosts are tougher */
	Verwesung	UMETA(DisplayName = "Verwesung (grün)"),
	/** Amber, special: smells sweet, lures, sticks */
	Honig		UMETA(DisplayName = "Honig (bernstein)"),
	/** Ice blue, special: its hosts are sluggish but strike hard */
	Kaelte		UMETA(DisplayName = "Kälte (eisblau)"),
	/** Violet, rare, at Nid sources: what its hosts strike is marked */
	Nid			UMETA(DisplayName = "Nid (violett)"),
	/** Red, legendary: its hosts are fast and raging */
	Rausch		UMETA(DisplayName = "Rausch (rot)")
};

/** One strain: how often it is met, what it does to its hosts and to infested players, and how its film looks */
USTRUCT(BlueprintType)
struct FVaelSeuchenStamm
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Strain")
	FText Name;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Strain")
	EVaelPlantRarity Rarity = EVaelPlantRarity::Common;

	/** Share among the strains where it is met (its favourite places multiply it) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Strain", meta = (ClampMin = 0))
	float Weight = 1.0f;

	/** Life and blows of its hosts against an ordinary one */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Host", meta = (ClampMin = 0.1))
	float HostHealth = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Host", meta = (ClampMin = 0.1))
	float HostDamage = 1.0f;

	/** Pace of everything its hosts do (walking, animations, attack pauses): below 1 sluggish, above 1 raging */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Host", meta = (ClampMin = 0.3, ClampMax = 2.5))
	float HostPace = 1.0f;

	/** What a blow of its hosts leaves on a player besides the infestation, and for how long */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Host")
	EVaelStatus HitStatus = EVaelStatus::None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Host", meta = (ClampMin = 0))
	float HitStatusSeconds = 0.0f;

	/** Chance that an attack, a spell or a dodge of an infested player goes wrong: the body resists */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Player", meta = (ClampMin = 0, ClampMax = 1))
	float Misfire = 0.1f;

	/** How far the steps of an infested player drift off course, in degrees at the most */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Player", meta = (ClampMin = 0, ClampMax = 120))
	float Drift = 15.0f;

	/** Seconds an infestation lasts if nothing washes it off */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Player", meta = (ClampMin = 1))
	float InfestSeconds = 12.0f;

	/** The film (parameters of M_Tropfseuche_Huelle) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Look")
	FLinearColor Color = FLinearColor(0.35f, 1.0f, 0.2f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Look", meta = (ClampMin = 0))
	float Glow = 3.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Look", meta = (ClampMin = 0, ClampMax = 1))
	float Cover = 0.55f;

	/** cm per second the pattern runs down */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Look")
	float Flow = 6.0f;

	/** Seconds per pulse, and 0 = a soft pulse ... 1 = jagged jolts */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Look", meta = (ClampMin = 0.1))
	float Pulse = 2.6f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Look", meta = (ClampMin = 0, ClampMax = 1))
	float Jerk = 0.0f;

	/** cm the film lifts off the skin */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Look", meta = (ClampMin = 0))
	float Wobble = 0.8f;

	/** How much of the film stays bare between its thick patches (0 = covers everything, 0.7 = a few patches) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Look", meta = (ClampMin = 0, ClampMax = 1))
	float Patches = 0.45f;

	/** Lumps of slime it puts on an infested player (models without lumps of their own) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Look", meta = (ClampMin = 0))
	int32 PlayerLumps = 8;
};

/**
 *  Values of the Tropfseuche. Edited under Project Settings > Game > Vael Tropfseuche, stored in DefaultGame.ini.
 */
UCLASS(config=Game, defaultconfig, meta = (DisplayName = "Vael Tropfseuche"))
class UVaelTropfseucheSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:

	UVaelTropfseucheSettings();

	static const UVaelTropfseucheSettings* Get() { return GetDefault<UVaelTropfseucheSettings>(); }

	virtual FName GetCategoryName() const override { return TEXT("Game"); }

	/** The five strains, in the order of EVaelSeuchenStamm */
	UPROPERTY(config, EditAnywhere, Category="Strains")
	TArray<FVaelSeuchenStamm> Strains;

	/** The film laid over whatever it infests */
	UPROPERTY(config, EditAnywhere, Category="Look")
	TSoftObjectPtr<UMaterialInterface> ShellMaterial;

	/** The thick, wet slime of lumps, threads and the heart in the ribs */
	UPROPERTY(config, EditAnywhere, Category="Look")
	TSoftObjectPtr<UMaterialInterface> LumpMaterial;

	UPROPERTY(config, EditAnywhere, Category="Look")
	TArray<TSoftObjectPtr<UStaticMesh>> LumpMeshes;

	/** Levels where a strain is met far more often (short level name), and by how much */
	UPROPERTY(config, EditAnywhere, Category="Strains")
	TMap<FString, EVaelSeuchenStamm> FavouritePlaces;

	UPROPERTY(config, EditAnywhere, Category="Strains", meta = (ClampMin = 1))
	float FavouriteWeight = 4.0f;

	/** Slime icicles of a strain within this many cm make it far more likely there too */
	UPROPERTY(config, EditAnywhere, Category="Strains", meta = (ClampMin = 0))
	float IcicleRadius = 2500.0f;

	/** A dead host rises again after these seconds, at most this many times, unless water reached its body */
	UPROPERTY(config, EditAnywhere, Category="Rising")
	FVector2D RiseDelay = FVector2D(6.0f, 10.0f);

	UPROPERTY(config, EditAnywhere, Category="Rising", meta = (ClampMin = 0))
	int32 MaxRises = 2;

	/** Levels whose places for enemies (TargetPoints tagged VaelHoehleGegner) bring Schleimgaenger, and how many per place */
	UPROPERTY(config, EditAnywhere, Category="Caves")
	TArray<FString> HostLevels;

	UPROPERTY(config, EditAnywhere, Category="Caves")
	FIntPoint HostsPerPlace = FIntPoint(1, 2);

	UPROPERTY(config, EditAnywhere, Category="Caves", meta = (ClampMin = 0, ClampMax = 1))
	float PlaceChance = 0.75f;

	const FVaelSeuchenStamm& GetStrain(EVaelSeuchenStamm Strain) const;
};

/**
 *  An infestation on a player: the film over the body (a second skin that follows the body's pose, so it never takes
 *  the place of the balance mark), the drifting steps and the body that now and then resists. Water, a potion or the
 *  end of its time washes it off; then the component removes itself.
 */
UCLASS()
class UVaelSeuchenBefall : public UActorComponent
{
	GENERATED_BODY()

public:

	UVaelSeuchenBefall();

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	virtual void OnUnregister() override;

	void Start(EVaelSeuchenStamm InStrain, float Seconds);

	/** Ends the infestation and removes the film */
	void End();

	/** The body resisted: the film flares, the body jerks */
	void Resisted();

	EVaelSeuchenStamm GetStrain() const { return Strain; }

private:

	void BuildShell();

	EVaelSeuchenStamm Strain = EVaelSeuchenStamm::Verwesung;
	float Flare = 0.0f;
	bool bEnded = false;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> ShellLook;

	UPROPERTY(Transient)
	TArray<TObjectPtr<USkeletalMeshComponent>> Shell;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> LumpLook;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UStaticMeshComponent>> Lumps;

	void AddLumps(USkeletalMeshComponent* Body);
};

/** The Tropfseuche: picking a strain, its film, infesting and washing off */
class UVaelTropfseuche
{
public:

	/** Rolls the strain for a host at a place: by weight, more of a strain in its favourite level and near its slime */
	static EVaelSeuchenStamm PickStrain(const UWorld* World, const FVector& Location);

	/** A film material for the strain */
	static UMaterialInstanceDynamic* MakeLook(UObject* Outer, EVaelSeuchenStamm Strain);

	/** A material for the thick slime of the strain: lumps and threads, or (bHeart) the glowing heart in the ribs */
	static UMaterialInstanceDynamic* MakeLumpLook(UObject* Outer, EVaelSeuchenStamm Strain, bool bHeart = false);

	/** Infests a player (renews the time, a stronger strain takes over) */
	static void Infest(AActor* Attacker, AVaelCharacter* Player, EVaelSeuchenStamm Strain);

	/** Washes the infestation off; true if there was one */
	static bool Cure(AActor* Target);

	/** The infestation on the target, or null */
	static UVaelSeuchenBefall* GetInfestation(const AActor* Target);

	/** Rolls whether the infested body resists what its player wants to do now; true = it resisted (the action is lost) */
	static bool Resists(AActor* Player);

	/** Degrees the steps of an infested player drift right now (0 if not infested) */
	static float GetDrift(const AActor* Player, float Time);

	/** Water on the ground at the place: a puddle, a spring, a spell's water */
	static bool IsWaterAt(const UWorld* World, const FVector& Location);

	/** Fills the places for enemies of a cave with Schleimgaenger (levels in the settings) */
	static void SpawnHosts(UWorld* World);
};
