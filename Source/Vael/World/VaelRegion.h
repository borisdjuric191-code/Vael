// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "VaelRegion.generated.h"

class UBoxComponent;

/** Weather of a region */
UENUM(BlueprintType)
enum class EVaelWeather : uint8
{
	Clear,
	/** Ash rain: creatures are wet, fires die sooner */
	Rain,
	/** Air is everywhere, lightning strikes near the players */
	Storm
};

/**
 *  A region of the world with its own corruption and weather, like the Aschenmark.
 *  Place it in a level and size its box; a level without regions gets one covering everything when the game starts.
 *  Corruption grows with Mark cast inside the region and makes the weather wilder and the creatures stronger.
 */
UCLASS()
class AVaelRegion : public AActor
{
	GENERATED_BODY()

private:

	/** Area of the region, unless it covers the whole level */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UBoxComponent> Bounds;

protected:

	/** Name shown to the players */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Region")
	FText RegionName;

	/** True if the region is everywhere no other region is */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Region")
	bool bCoversWholeLevel = false;

	/** Corruption at the start, 0 to 100. Negative: the default of the project settings. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Region", meta = (ClampMin = -1, ClampMax = 100))
	float StartCorruption = -1.0f;

	/** Weather at the start */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Weather")
	EVaelWeather StartWeather = EVaelWeather::Clear;

	/** False keeps the start weather forever */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Weather")
	bool bChangingWeather = true;

public:

	/** Constructor */
	AVaelRegion();

	/** Initialization */
	virtual void BeginPlay() override;

	/** Update */
	virtual void Tick(float DeltaSeconds) override;

	/** Returns the region at a location: a placed region containing it, else the one covering the whole level, created if missing */
	static AVaelRegion* GetRegionAt(UWorld* World, const FVector& Location);

	/** True if the location lies in the region */
	bool Contains(const FVector& Location) const;

	/** Corruption from 0 (pure) to 100 (fully corrupted) */
	UFUNCTION(BlueprintPure, Category="Region")
	float GetCorruption() const { return Corruption; }

	/** Changes the corruption, kept between 0 and 100 */
	UFUNCTION(BlueprintCallable, Category="Region")
	void AddCorruption(float Amount);

	UFUNCTION(BlueprintCallable, Category="Region")
	void SetCorruption(float NewCorruption);

	UFUNCTION(BlueprintPure, Category="Region")
	EVaelWeather GetWeather() const { return Weather; }

	/** Changes the weather right away and tells the players */
	UFUNCTION(BlueprintCallable, Category="Weather")
	void SetWeather(EVaelWeather NewWeather);

	UFUNCTION(BlueprintPure, Category="Region")
	FText GetRegionName() const { return RegionName; }

	/** Name of a weather shown to the players */
	static FText GetWeatherName(EVaelWeather InWeather);

	/** Real time of the last lightning strike, for the flash on screen */
	double GetLastLightningTime() const { return LastLightningTime; }

	/** Remembers a strike for the flash on screen */
	void NotifyLightning() { LastLightningTime = FPlatformTime::Seconds(); }

private:

	/** Picks the next weather, wilder the more corrupted the region is */
	EVaelWeather PickNextWeather() const;

	/** Keeps the creatures in the region wet */
	void TickRain(float DeltaSeconds);

	/** Lets lightning fall near the players in the region */
	void TickStorm(float DeltaSeconds);

	/** True if at least one player stands in the region */
	bool HasPlayerInside() const;

	float Corruption = 0.0f;
	EVaelWeather Weather = EVaelWeather::Clear;

	/** Seconds until the weather changes */
	float WeatherTimeLeft = 0.0f;

	/** Seconds until the next lightning strike */
	float LightningCooldown = 0.0f;

	/** Seconds until the creatures get wet again */
	float RainRefreshTime = 0.0f;

	/** Real time of the last lightning strike */
	double LastLightningTime = -100.0;
};
