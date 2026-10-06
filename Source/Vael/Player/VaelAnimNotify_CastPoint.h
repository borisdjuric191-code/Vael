// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "VaelAnimNotify_CastPoint.generated.h"

/**
 *  Marks the moment in a cast animation at which the spell leaves the hand.
 *  Placed on the cast montages; the formula ability waits for it before the spell appears.
 */
UCLASS(meta = (DisplayName = "Vael Cast Point"))
class UVaelAnimNotify_CastPoint : public UAnimNotify
{
	GENERATED_BODY()

public:

	//~Begin UAnimNotify
	virtual FString GetNotifyName_Implementation() const override;
	virtual void Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference) override;
	//~End UAnimNotify
};
