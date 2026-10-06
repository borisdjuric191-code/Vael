// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "VaelMarkSource.generated.h"

class AVaelCharacter;
class UNiagaraComponent;
class UNiagaraSystem;
class UStaticMeshComponent;

/**
 *  A place where the Mark, the blood of the Sleeper, wells up: a dangerous hotspot placed in a level.
 *  While it is open it slowly raises the corruption of its region, lets mages draw the Mark from it once the Mark has awakened,
 *  and attacks players who stand in it for too long. Sealing it (later through an offering and its guardian) takes a big
 *  part of the corruption away from the region for good.
 *  Placeholder look: a violet disc, grey once sealed.
 */
UCLASS()
class AVaelMarkSource : public AActor
{
	GENERATED_BODY()

	/** Placeholder look */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UStaticMeshComponent> Disc;

protected:

	/** Radius of the source in cm (prototype: 2.3 tiles) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Mark Source", meta = (ClampMin = 1))
	float Radius = 322.0f;

	/** True for a source that is sealed from the start */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Mark Source")
	bool bSealed = false;

	/** Own look of the open source. Empty: NS_Vael_MarkSource from the effects folder, or the placeholder disc. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Mark Source")
	TObjectPtr<UNiagaraSystem> OpenEffect;

public:

	/** Constructor */
	AVaelMarkSource();

	/** Applies the radius to the placeholder look */
	virtual void OnConstruction(const FTransform& Transform) override;

	/** Initialization */
	virtual void BeginPlay() override;

	/** Update */
	virtual void Tick(float DeltaSeconds) override;

	/** Closes the source for good and takes corruption away from its region. Returns false if it was sealed already. */
	UFUNCTION(BlueprintCallable, Category="Mark Source")
	bool Seal();

	/** True once the source is sealed */
	UFUNCTION(BlueprintPure, Category="Mark Source")
	bool IsSealed() const { return bSealed; }

	/** True if the location is inside the open source, or closer to its edge than the extra distance */
	bool IsInside(const FVector& Location, float ExtraDistance = 0.0f) const;

	/** Returns the open source at the location, null if there is none */
	static AVaelMarkSource* FindOpenSourceAt(const UWorld* World, const FVector& Location, float ExtraDistance = 0.0f);

private:

	/** Raises the corruption of the region while the source is open */
	void FeedRegion(float DeltaSeconds);

	/** Attacks players who stand in the source for too long */
	void AttackPlayers(float DeltaSeconds);

	/** Colors and sizes the placeholder, starts or stops the effect */
	void RefreshLook();

	/** Seconds each player has stood in the source without a break */
	TMap<TWeakObjectPtr<AVaelCharacter>, float> TimeInside;

	/** Seconds until the next attack on each player inside */
	TMap<TWeakObjectPtr<AVaelCharacter>, float> NextAttack;

	/** The effect of the open source, null while the placeholder shows */
	UPROPERTY(Transient)
	TObjectPtr<UNiagaraComponent> OpenEffectComponent;
};
