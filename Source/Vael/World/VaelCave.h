// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "VaelCave.generated.h"

class USphereComponent;
class AVaelCharacter;

/**
 *  The mouth of a cave (Boris 2026-11-15: the caves of the Eisenschlund, each its own indoor level like the Glutkammer).
 *  A player who steps in takes the whole group into TargetLevel: what everyone carries and has learned goes along in the save.
 *  In the open world the mouth leads into the cave; in the cave it leads out, and the group lands at ReturnPlace in front of the
 *  mouth it came in by.
 */
UCLASS()
class AVaelCaveEntrance : public AActor
{
	GENERATED_BODY()

public:

	AVaelCaveEntrance();

	/** The level to go to, like "L_Gildenmine" or "L_Vael" */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Cave")
	FName TargetLevel;

	/** Where the group lands in the target level (world X, Y in cm); zero = at its PlayerStart */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Cave")
	FVector2D ReturnPlace = FVector2D::ZeroVector;

	/** Shown when the group arrives, like "Kernerz-Stollen" */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Cave")
	FText Message;

	/**
	 *  The way down or up inside one level (the caves of the Eisenschlund inside L_Vael since 2026-11-23): no level is opened, the
	 *  group is put down at Destination (cm; Z is the floor height there, the ground is looked for just above it)
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Cave")
	bool bInLevel = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Cave", meta = (EditCondition = "bInLevel"))
	FVector Destination = FVector::ZeroVector;

	/** How far the mouth takes whoever steps in (cm) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Cave")
	float MouthRadius = 220.0f;

	/**
	 *  Coming nearer than this (cm) shows Hint first, so nobody is taken by surprise (Boris 2026-11-23: in a tunnel he was suddenly
	 *  beamed out; the exits were plain pockets of the cave); 0 = no hint
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Cave")
	float HintRadius = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Cave")
	FText Hint;

protected:

	virtual void BeginPlay() override;

private:

	UPROPERTY(VisibleAnywhere, Category="Components")
	TObjectPtr<USphereComponent> Mouth;

	UPROPERTY(Transient)
	TObjectPtr<USphereComponent> HintArea;

	float LastHint = -100.0f;

	UFUNCTION()
	void OnNear(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex,
		bool bFromSweep, const FHitResult& SweepResult);

	/** Takes the group along if this player may go now */
	void TryTake(AVaelCharacter* Player);

	UFUNCTION()
	void OnEnter(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex,
		bool bFromSweep, const FHitResult& SweepResult);
};

/** Carries the group from one level to the next and puts each player down where the mouth said */
UCLASS()
class UVaelCaveTravel : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:

	static UVaelCaveTravel* Get(const UObject* WorldContext);

	/** Saves into memory (and the slot), remembers where to land, and opens the level */
	void Travel(const UObject* WorldContext, FName Level, const FVector2D& Place, const FText& Message);

	/** A player of the new level is set up: puts it down at the place, once per player, and shows the message */
	void OnPlayerReady(AVaelCharacter* Player);

	/** True while a travel is under way (the mouths of the new level wait a moment before they take anyone) */
	bool IsTravelling() const { return bTravelling || bHopping; }

	/** Within one level: every player down at a place (cm, Z = floor height), side by side, and the message */
	void Hop(const UObject* WorldContext, const FVector& Place, const FText& Message);

private:

	bool bHopping = false;
	FName PendingLevel;
	FVector2D PendingPlace = FVector2D::ZeroVector;
	FText PendingMessage;
	TSet<int32> Landed;
	bool bTravelling = false;
};
