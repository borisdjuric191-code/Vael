// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "VaelScriptStatics.generated.h"

/**
 *  Helpers for the Python scripts in Scripts/ that create data assets, for values Python can't build on its own.
 */
UCLASS()
class UVaelScriptStatics : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:

	/** Makes a tag container from tag names like "Gear.WeatherWard"; unknown names are skipped */
	UFUNCTION(BlueprintCallable, Category="Vael|Scripts")
	static FGameplayTagContainer MakeTagContainer(const TArray<FName>& TagNames);
};
