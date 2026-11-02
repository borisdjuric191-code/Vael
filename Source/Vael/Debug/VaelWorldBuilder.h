// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "VaelWorldBuilder.generated.h"

class UMaterialInterface;

/**
 *  Help for the world scripts (Scripts/BuildVaelWelt.py) with what Python can't do in the editor: a landscape made from a
 *  height file, split into streaming pieces for World Partition. Editor only; in a game it does nothing.
 */
UCLASS()
class UVaelWorldBuilder : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:

	/**
	 *  Makes a landscape in the world of the context object from a raw 16 bit height file (little endian, X runs fastest,
	 *  SizeX * SizeY values). The sizes must be a whole number of components of 2 x 63 quads plus one (127, 253 ... 2017, 2521).
	 *  Its lower corner sits at Location, Scale is cm per quad (Z 100: a height value of 128 is one metre).
	 *  GridSize > 0 splits it into streaming proxies of that many components. Returns the landscape, or null.
	 */
	UFUNCTION(BlueprintCallable, Category="Vael|World", meta = (WorldContext = "WorldContext"))
	static AActor* CreateLandscapeFromRaw(UObject* WorldContext, const FString& RawFile, int32 SizeX, int32 SizeY, FVector Location, FVector Scale,
		UMaterialInterface* Material, int32 GridSize);
};
