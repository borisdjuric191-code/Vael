// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "VaelQuestMarker.generated.h"

class UBillboardComponent;

/**
 *  A place quests can send the players to, like the village ruins or the crater.
 *  Invisible in the game; reports to the quests whenever a player stands within its radius.
 */
UCLASS()
class AVaelQuestMarker : public AActor
{
	GENERATED_BODY()

#if WITH_EDITORONLY_DATA
	/** Icon in the editor */
	UPROPERTY(VisibleAnywhere, Category="Components")
	TObjectPtr<UBillboardComponent> Icon;
#endif

protected:

	/** Id quest steps of type Reach name, like "Dorf" */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Quest")
	FName MarkerId;

	/** Distance in cm a player has to come close */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Quest", meta = (ClampMin = 50))
	float Radius = 600.0f;

public:

	/** Constructor */
	AVaelQuestMarker();

	FName GetMarkerId() const { return MarkerId; }

protected:

	/** Starts looking for players */
	virtual void BeginPlay() override;

private:

	/** Tells the quests if a player stands inside */
	void CheckPlayers();

	FTimerHandle CheckTimer;
};
