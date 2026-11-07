// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Player/VaelInteractable.h"
#include "VaelRefugiumClutch.generated.h"

class UPointLightComponent;
class UStaticMesh;
class UStaticMeshComponent;
enum class EVaelDayPhase : uint8;

/** Where a bed place of a refugium stands in its life */
UENUM(BlueprintType)
enum class EVaelClutchState : uint8
{
	/** The story has not reached its stage yet: a dim clutch under silk */
	Sleeping,
	/** Its stage is reached: it glows up and hatches at the next dawn, or at once when a player tends it */
	Awake,
	/** Hatched: an open clutch bed that can be planted */
	Bed
};

/**
 *  One bed place of a refugium (Lore-Bibel, Refugium: 40 beds in the Glutkammer, 8 more with every stage).
 *  The stages come from story flags "Refugium.<Name>.Stufe2" to "Stufe5" (stage I is there from the start). A place of a
 *  later stage lies there as a sleeping clutch; once the story reaches its stage it wakes, glows and hatches into a bed at
 *  the next dawn, or at once when a player tends it. Hatched places are remembered as story flags, so they are in the save.
 */
UCLASS(meta = (DisplayName = "Vael Refugium Clutch"))
class AVaelRefugiumClutch : public AActor, public IVaelInteractable
{
	GENERATED_BODY()

public:

	AVaelRefugiumClutch();

	/** Refugium this place belongs to, as in the story flags ("Glutkammer") */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Refugium")
	FName Refugium = TEXT("Glutkammer");

	/** Stage that wakes it, 1 to 5 (1 = a bed from the start) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Refugium", meta = (ClampMin = "1", ClampMax = "5"))
	int32 Stage = 1;

	/** Name of the place in the save, unique in the refugium ("Halle03") */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Refugium")
	FName PlaceId;

	/** The two looks */
	UPROPERTY(EditAnywhere, Category = "Refugium")
	TSoftObjectPtr<UStaticMesh> SleepingMesh;

	UPROPERTY(EditAnywhere, Category = "Refugium")
	TSoftObjectPtr<UStaticMesh> BedMesh;

	EVaelClutchState GetState() const { return State; }

	/** Stage the story has reached for a refugium, 1 to 5 */
	static int32 GetReachedStage(const UObject* WorldContext, FName InRefugium);

	/** Sets the reached stage of a refugium (story, tests); clutches of that stage and below wake */
	static void SetReachedStage(const UObject* WorldContext, FName InRefugium, int32 InStage);

	/** Lets every awake clutch of the world hatch now (as if dawn had come) */
	static void HatchAllAwake(const UObject* WorldContext);

	//~Begin IVaelInteractable
	virtual bool CanInteract(const AVaelCharacter* Player) const override;
	virtual void Interact(AVaelCharacter* Player) override;
	virtual FText GetInteractPrompt() const override;
	//~End IVaelInteractable

	virtual void Tick(float DeltaSeconds) override;

	/** Shows the right look in the editor already: the bed for stage 1, the sleeping clutch for the others */
	virtual void OnConstruction(const FTransform& Transform) override;

protected:

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:

	/** Looks at the story and moves on to the state it allows */
	void Refresh();

	/** Opens into a bed, with a short flash unless bInstant */
	void Hatch(bool bInstant);

	void OnDayPhase(EVaelDayPhase Phase);

	FName HatchedFlag() const;

	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<UStaticMeshComponent> Sleeping;

	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<UStaticMeshComponent> Bed;

	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<UPointLightComponent> Glow;

	EVaelClutchState State = EVaelClutchState::Sleeping;

	/** Seconds into the hatching (opening) animation, below 0 when none runs */
	float Opening = -1.0f;

	/** Time for the glow to breathe */
	float Breath = 0.0f;

	FDelegateHandle PhaseHandle;
};
