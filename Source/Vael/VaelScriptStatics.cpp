// Copyright Epic Games, Inc. All Rights Reserved.

#include "VaelScriptStatics.h"
#include "Vael.h"

FGameplayTagContainer UVaelScriptStatics::MakeTagContainer(const TArray<FName>& TagNames)
{
	FGameplayTagContainer Container;

	for (const FName TagName : TagNames)
	{
		const FGameplayTag Tag = FGameplayTag::RequestGameplayTag(TagName, false);
		if (Tag.IsValid())
		{
			Container.AddTag(Tag);
		}
		else
		{
			UE_LOG(LogVael, Warning, TEXT("MakeTagContainer: unknown tag %s"), *TagName.ToString());
		}
	}

	return Container;
}
