// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "VaelScriptStatics.generated.h"

class UFontFace;

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

	/** One line per user parameter (name and type) and per emitter of a Niagara system, to see what a pack effect can be fed */
	UFUNCTION(BlueprintCallable, Category="Vael|Scripts")
	static FString DescribeNiagaraSystem(class UNiagaraSystem* System);

#if WITH_EDITOR
	/** Makes a font face asset from a TrueType file without the import dialog, which a script without a window can't show.
	 *  The font is kept inside the asset. Save it from the script afterwards. Asset path like /Game/Vael/UI/Fonts/AlegreyaSans-Regular */
	UFUNCTION(BlueprintCallable, Category="Vael|Scripts")
	static UFontFace* CreateFontFaceFromFile(const FString& FontFile, const FString& AssetPath);

	/** Makes and saves a font face asset in /Game/Vael/UI/Fonts for every .ttf in Scripts/Data/Fonts that has none yet (game run with -VaelMakeFonts) */
	static void MakeFontAssets();
#endif
};
