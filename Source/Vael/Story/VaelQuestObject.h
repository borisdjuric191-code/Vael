// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Player/VaelInteractable.h"
#include "VaelQuestObject.generated.h"

class UStaticMeshComponent;

/**
 *  Something in the world a quest sends the players to look at, like the papers in the Gildenspeicher.
 *  Using it shows what the players find and reports an event to the quests.
 *  It can only be used while a quest waits for that event. Placeholder look: a small box.
 */
UCLASS()
class AVaelQuestObject : public AActor, public IVaelInteractable
{
	GENERATED_BODY()

	/** Placeholder look */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UStaticMeshComponent> Mesh;

protected:

	/** Event reported to the quests when the players use it, like "BeweisGefunden" */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Quest")
	FName EventName;

	/** Name shown over what the players find, like "Kassette der Gilde" */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Quest")
	FText Title;

	/** What pressing the button does, like "Durchsuchen" */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Quest")
	FText Prompt;

	/** What the players find, read like the lines of a talk */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Quest", meta = (MultiLine = true))
	TArray<FText> Lines;

	/** Placeholder color */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Quest")
	FLinearColor Color = FLinearColor(0.42f, 0.33f, 0.2f);

public:

	/** Constructor */
	AVaelQuestObject();

	//~Begin IVaelInteractable
	virtual bool CanInteract(const AVaelCharacter* Player) const override;
	virtual void Interact(AVaelCharacter* Player) override;
	virtual FText GetInteractPrompt() const override { return Prompt; }
	//~End IVaelInteractable

	FName GetEventName() const { return EventName; }

protected:

	/** Registers it as something to use and colors the placeholder */
	virtual void BeginPlay() override;

	/** Unregisters it */
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
};
