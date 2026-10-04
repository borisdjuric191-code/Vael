// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "VaelCharacter.generated.h"

class UStaticMeshComponent;

/**
 *  A directly controlled player character seen from the shared isometric camera.
 *  Turns towards the aim direction of its controller and can dodge roll.
 */
UCLASS()
class AVaelCharacter : public ACharacter
{
	GENERATED_BODY()

private:

	/** Placeholder disc at the character's feet showing the player color */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UStaticMeshComponent> PlayerMarker;

protected:

	/** Speed during a dodge roll */
	UPROPERTY(EditAnywhere, Category="Dodge", meta = (ClampMin = 0))
	float DodgeSpeed = 1400.0f;

	/** Length of a dodge roll in seconds */
	UPROPERTY(EditAnywhere, Category="Dodge", meta = (ClampMin = 0))
	float DodgeDuration = 0.4f;

	/** Time from the start of a dodge roll until the next one is allowed, in seconds */
	UPROPERTY(EditAnywhere, Category="Dodge", meta = (ClampMin = 0))
	float DodgeCooldown = 0.9f;

	/** Placeholder for the roll animation: height scale of the mesh while dodging */
	UPROPERTY(EditAnywhere, Category="Dodge", meta = (ClampMin = 0.1, ClampMax = 1))
	float DodgeMeshSquash = 0.6f;

	/** Vector parameter that tints the character materials in the player color */
	UPROPERTY(EditAnywhere, Category="Appearance")
	FName BodyTintParameter = TEXT("Paint Tint");

	/** Vector parameter that colors the marker disc */
	UPROPERTY(EditAnywhere, Category="Appearance")
	FName MarkerColorParameter = TEXT("Color");

public:

	/** Constructor */
	AVaelCharacter();

	/** Initialization */
	virtual void BeginPlay() override;

	/** Update */
	virtual void Tick(float DeltaSeconds) override;

	/** Applies the color of the possessing player */
	virtual void PossessedBy(AController* NewController) override;

	/** Starts a dodge roll in the given world direction, or in the facing direction if it is zero. Returns false while on cooldown. */
	UFUNCTION(BlueprintCallable, Category="Dodge")
	bool StartDodge(const FVector& WorldDirection);

	/** True while a dodge roll is in progress */
	UFUNCTION(BlueprintPure, Category="Dodge")
	bool IsDodging() const { return bIsDodging; }

	/** Tints the character and its marker */
	UFUNCTION(BlueprintCallable, Category="Appearance")
	void SetPlayerColor(const FLinearColor& Color);

	/** Returns the marker component **/
	UStaticMeshComponent* GetPlayerMarker() const { return PlayerMarker.Get(); }

private:

	/** Ends the dodge roll and returns to normal movement */
	void EndDodge();

	/** Mesh scale outside of a dodge roll */
	FVector DefaultMeshScale = FVector::OneVector;

	/** Normalized direction of the current dodge roll */
	FVector DodgeDirection = FVector::ZeroVector;

	/** World time at which the current dodge roll ends */
	float DodgeEndTime = 0.0f;

	/** World time from which the next dodge roll is allowed */
	float NextDodgeTime = 0.0f;

	/** True while a dodge roll is in progress */
	bool bIsDodging = false;
};
