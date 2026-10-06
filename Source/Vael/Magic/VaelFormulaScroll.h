// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Magic/VaelElementTypes.h"
#include "VaelFormulaScroll.generated.h"

class USphereComponent;
class UStaticMeshComponent;

/**
 *  A scroll lying in the world that holds a sealed formula.
 *  The first player who walks over it picks it up and the whole group learns the formula.
 *  Placed in the level; the elements name the formula, the label says where it was found.
 */
UCLASS()
class AVaelFormulaScroll : public AActor
{
	GENERATED_BODY()

	/** Players who touch this pick the scroll up */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<USphereComponent> PickupSphere;

	/** Placeholder look: a rolled up sheet */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UStaticMeshComponent> Mesh;

public:

	/** Constructor */
	AVaelFormulaScroll();

	/** Elements of the formula the scroll teaches, in any order */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Scroll")
	TArray<EVaelElement> Elements;

	/** Name of the scroll in the message when it is picked up, for example "Schriftrolle der Kapelle" */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Scroll")
	FText Label;

	/** Players this close pick the scroll up, in cm (prototype: 1 tile) */
	UPROPERTY(EditAnywhere, Category="Scroll", meta = (ClampMin = 1))
	float PickupRadius = 140.0f;

	/** Placeholder color */
	UPROPERTY(EditAnywhere, Category="Scroll")
	FLinearColor Color = FLinearColor(0.91f, 0.86f, 0.75f);

	/** Spawns a scroll on the ground at the location, for tests */
	static AVaelFormulaScroll* SpawnScroll(UWorld* World, const FVector& GroundLocation, TConstArrayView<EVaelElement> InElements, const FText& InLabel);

	/** Update: the scroll bobs and turns slowly */
	virtual void Tick(float DeltaSeconds) override;

	//~Begin AActor
	virtual void OnConstruction(const FTransform& Transform) override;
	//~End AActor

protected:

	/** Picks the scroll up once a player touches it */
	virtual void BeginPlay() override;

	/** Called when something touches the pickup sphere */
	UFUNCTION()
	void OnPickupOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

private:

	/** Teaches the formula and removes the scroll */
	void PickUp(AActor* Player);

	/** Height of the mesh above the ground when the game starts */
	float BaseMeshHeight = 0.0f;
};
