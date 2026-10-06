// Copyright Epic Games, Inc. All Rights Reserved.

#include "Player/VaelAnimNotify_CastPoint.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "Components/SkeletalMeshComponent.h"
#include "Magic/VaelGameplayTags.h"

FString UVaelAnimNotify_CastPoint::GetNotifyName_Implementation() const
{
	return TEXT("CastPoint");
}

void UVaelAnimNotify_CastPoint::Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference)
{
	Super::Notify(MeshComp, Animation, EventReference);

	AActor* Owner = MeshComp != nullptr ? MeshComp->GetOwner() : nullptr;
	if (Owner == nullptr || Owner->GetWorld() == nullptr || !Owner->GetWorld()->IsGameWorld())
	{
		// The editor preview has no ability system to tell
		return;
	}

	FGameplayEventData EventData;
	EventData.EventTag = VaelTags::Event_CastPoint;
	EventData.Instigator = Owner;

	UAbilitySystemBlueprintLibrary::SendGameplayEventToActor(Owner, VaelTags::Event_CastPoint, EventData);
}
