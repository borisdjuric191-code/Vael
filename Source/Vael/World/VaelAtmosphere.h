// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Magic/VaelElementTypes.h"
#include "VaelAtmosphere.generated.h"

class AVaelLocalWeather;
class UVaelCloudCardsComponent;

/** What the air over a place holds */
UENUM(BlueprintType)
enum class EVaelAir : uint8
{
	/** Rising heat of fires, burning creatures, lava */
	Heat,
	/** Steam and water in the air: steam clouds, melting ice, water poured on fire */
	Moisture,
	/** Cold of ice, frost and hail */
	Cold
};

/** Heat, moisture and cold in one cell of the air */
struct FVaelAirCell
{
	float Heat = 0.0f;
	float Moisture = 0.0f;
	float Cold = 0.0f;

	float& Get(EVaelAir Kind) { return Kind == EVaelAir::Heat ? Heat : Kind == EVaelAir::Moisture ? Moisture : Cold; }
	bool IsEmpty() const { return Heat < 0.01f && Moisture < 0.01f && Cold < 0.01f; }
};

/**
 *  The air of the world remembers what spells and creatures do to it (Boris 2026-10-26). Cells of about 10 m hold heat,
 *  moisture and cold that fade away slowly. Fires, steam and ice add to them by themselves; spells and creatures add on top.
 *  Where enough gathers, a weather of its own breaks out there for a while: a shower, a thunderstorm, a rising thermal,
 *  a wind front, a bank of cold fog or a burst of graupel (AVaelLocalWeather). Clouds and shimmer show it coming.
 */
UCLASS()
class UVaelAtmosphere : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:

	/** The air of the world of the object, null outside a game world */
	static UVaelAtmosphere* Get(const UObject* WorldContext);

	/** Adds heat, moisture or cold to the air over a location */
	static void Add(const UObject* WorldContext, const FVector& Location, EVaelAir Kind, float Amount);

	/** A spell of an element hit here: fire heats the air, water wets it */
	static void NotifySpellImpact(const UObject* WorldContext, const FVector& Location, float Radius, EVaelElement Element);

	/** Heat, moisture and cold over a location, summed over the cell and its neighbours, as one event would see them */
	FVaelAirCell Sample(const FVector& Location) const;

	//~Begin UTickableWorldSubsystem
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
	//~End UTickableWorldSubsystem

private:

	FIntPoint CellOf(const FVector& Location) const;
	FVector CenterOf(const FIntPoint& Cell) const;

	/** Fires, steam, ice and hot creatures add to the air by themselves */
	void Gather(float Step);

	/** Everything fades, faster in some weathers */
	void Fade(float Step);

	/** Breaks out a weather where enough has gathered */
	void CheckEvents();

	/** Clouds gather over moist air, heat shimmers over hot air, so players see a weather coming */
	void UpdateSigns();

	TMap<FIntPoint, FVaelAirCell> Cells;

	/** Ground height under each cell, for the signs */
	TMap<FIntPoint, float> CellGround;

	float StepTime = 0.0f;

	UPROPERTY(Transient)
	TArray<TObjectPtr<AVaelLocalWeather>> Events;

	/** The signs: one actor holding the cloud and shimmer cards */
	UPROPERTY(Transient)
	TObjectPtr<AActor> SignActor;

	UPROPERTY(Transient)
	TObjectPtr<UVaelCloudCardsComponent> CloudSigns;

	UPROPERTY(Transient)
	TObjectPtr<UVaelCloudCardsComponent> HeatSigns;

	UPROPERTY(Transient)
	TObjectPtr<UVaelCloudCardsComponent> ColdSigns;
};
