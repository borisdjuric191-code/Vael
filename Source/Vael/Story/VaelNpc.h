// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Player/VaelInteractable.h"
#include "Story/VaelNpcIdentity.h"
#include "VaelNpc.generated.h"

class UCapsuleComponent;
class UStaticMeshComponent;
class UVaelDialogue;
class UVaelGossip;
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

	/** Gerede: what the person mutters when players walk by. A person with gossip but no dialogue is a bystander nobody can talk to. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Npc")
	TObjectPtr<UVaelGossip> Gossip;

	/** Color of the placeholder robe */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Npc")
	FLinearColor RobeColor = FLinearColor(0.32f, 0.24f, 0.16f);

	/** Who the person is: name, origin, role, settlement, age, what they think of the Prophet (Scripts/Data/Welt/AM_Siedlungen.json) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Npc")
	FVaelNpcIdentity Identity;

	/** The places of the day: work by day, the fire in the evening, sleep at night, or the stops of a carter.
	 *  Empty for people who stay where they are (the story people). Changes happen only where no player can see. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Npc")
	TArray<FVaelNpcPlace> Places;

	/** Nobody may be nearer than this to the old or the new spot when a person changes place (cm) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Npc")
	float UnseenDistance = 4500.0f;

public:

	/** Constructor */
	AVaelNpc();

	//~Begin IVaelInteractable
	virtual bool CanInteract(const AVaelCharacter* Player) const override { return Dialogue != nullptr && !bAsleep; }
	virtual void Interact(AVaelCharacter* Player) override;
	virtual FText GetInteractPrompt() const override;
	//~End IVaelInteractable

	/** True while the person has not told their first talk yet, or a quest waits for a talk with them; the HUD shows a "!" over them */
	bool HasNews() const;

	/** Name shown over the person */
	FText GetDisplayName() const;

	/** What the person mutters right now, empty while silent */
	FText GetGossipLine() const;

	/** Who the person is */
	const FVaelNpcIdentity& GetIdentity() const { return Identity; }

	/** Sets who the person is (placement scripts, later the deeds of the group) */
	UFUNCTION(BlueprintCallable, Category="Npc")
	void SetIdentity(const FVaelNpcIdentity& NewIdentity);

	/** Where the person should be at this hour, or null if they have no places */
	const FVaelNpcPlace* GetWantedPlace(float Hour) const;

	/** True while the person sleeps inside, out of sight */
	bool IsAsleep() const { return bAsleep; }

	/** Puts the person at the place of the hour at once, seen or not (test cheat) */
	void FollowDayNow();

	/** Update: mutters now and then while players are near */
	virtual void Tick(float DeltaSeconds) override;

protected:

	/** Registers the person as someone to talk to and colors the placeholder */
	virtual void BeginPlay() override;

	/** Unregisters them */
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:

	/** Key under which the progress remembers the intro */
	FName GetSpeakerKey() const;

	/** Placeholder look after age and build: children small, old people a little bent, strong people wider */
	void ApplyBodyShape();

	/** The name of another person of the same settlement, for lines that talk about a neighbour */
	FText PickNeighbourName() const;

	/** Goes to the place of the hour if no player can see the old or the new spot; bForce goes anyway */
	void FollowDay(bool bForce);

	/** Moves to a place; sleeping hides the person */
	void MoveTo(const FVaelNpcPlace& Place);

	/** Place the person is at now, INDEX_NONE before the first move */
	int32 CurrentPlace = INDEX_NONE;
	bool bAsleep = false;
	float NextDayCheckTime = 0.0f;

	/** The gossip line said now, with its placeholders filled in */
	FText GossipText;

	/** Line of the gossip said last, the time it fades, and the time the next one may come */
	int32 GossipIndex = INDEX_NONE;
	float GossipEndTime = 0.0f;
	float NextGossipTime = 0.0f;
};
