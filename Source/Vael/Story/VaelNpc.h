// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Player/VaelInteractable.h"
#include "VaelNpc.generated.h"

class UCapsuleComponent;
class UStaticMeshComponent;
class UVaelDialogue;
class UVaelShop;

/**
 *  A person players can talk to, like Edda Krell in the camp. Placed in a level with a dialogue asset.
 *  The first talk tells the intro, later talks give hints that fit the state of the world.
 *  Placeholder look: a robed figure of engine shapes.
 */
UCLASS()
class AVaelNpc : public AActor, public IVaelInteractable
{
	GENERATED_BODY()

	/** Blocks the way like a person */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UCapsuleComponent> Capsule;

	/** Placeholder body */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UStaticMeshComponent> Body;

	/** Placeholder head */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UStaticMeshComponent> Head;

protected:

	/** What the person says */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Npc")
	TObjectPtr<UVaelDialogue> Dialogue;

	/** Goods the person sells for guild coins; the shop opens after each talk. Empty for people who sell nothing. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Npc")
	TObjectPtr<UVaelShop> Shop;

	/** Color of the placeholder robe */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Npc")
	FLinearColor RobeColor = FLinearColor(0.32f, 0.24f, 0.16f);

public:

	/** Constructor */
	AVaelNpc();

	//~Begin IVaelInteractable
	virtual bool CanInteract(const AVaelCharacter* Player) const override { return Dialogue != nullptr; }
	virtual void Interact(AVaelCharacter* Player) override;
	virtual FText GetInteractPrompt() const override;
	//~End IVaelInteractable

	/** True while the person has not told their first talk yet, or a quest waits for a talk with them; the HUD shows a "!" over them */
	bool HasNews() const;

	/** Name shown over the person */
	FText GetDisplayName() const;

protected:

	/** Registers the person as someone to talk to and colors the placeholder */
	virtual void BeginPlay() override;

	/** Unregisters them */
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:

	/** Key under which the progress remembers the intro */
	FName GetSpeakerKey() const;
};
