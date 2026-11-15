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

protected:

	virtual void BeginPlay() override;

private:

	UPROPERTY(VisibleAnywhere, Category="Components")
	TObjectPtr<USphereComponent> Mouth;

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
	bool IsTravelling() const { return bTravelling; }

private:

	FName PendingLevel;
	FVector2D PendingPlace = FVector2D::ZeroVector;
	FText PendingMessage;
	TSet<int32> Landed;
	bool bTravelling = false;
};
