// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "VaelWorldBuilder.generated.h"

class UMaterialInterface;
class UTexture2D;
class UTexture2DArray;
class UStaticMesh;

/** One mesh with its materials and where its copies stand, for UVaelWorldBuilder::SpawnInstanceCell */
USTRUCT(BlueprintType)
struct FVaelInstanceBatch
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Vael|World")
	TObjectPtr<UStaticMesh> Mesh;

	/** Material per slot; empty keeps the materials of the mesh */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Vael|World")
	TArray<TObjectPtr<UMaterialInterface>> Materials;

	/** World transforms of the copies */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Vael|World")
	TArray<FTransform> Transforms;

	/** Blocks like a wall; small things (twigs, pebbles) are walked through */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Vael|World")
	bool bCollision = true;

	/** Copies farther than this from the camera are not drawn (cm), 0 = always */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Vael|World")
	float CullDistance = 0.0f;
};

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

	/** The actors as the text the editor copies (Ctrl+C), without the clipboard; empty in a game */
	UFUNCTION(BlueprintCallable, Category="Vael|World", meta = (WorldContext = "WorldContext"))
	static FString CopyActorsAsText(UObject* WorldContext, const TArray<AActor*>& Actors);

	/** Pastes actors copied as text into the world of the context object, where they were; returns the new actors */
	UFUNCTION(BlueprintCallable, Category="Vael|World", meta = (WorldContext = "WorldContext"))
	static TArray<AActor*> PasteActorsFromText(UObject* WorldContext, const FString& Text);

	/** Fills a texture array from textures of the same size (layer i = texture i) and rebuilds it; false if that fails.
	 *  Python cannot reach the source textures of an array, the ground of the Aschenmark needs them. */
	UFUNCTION(BlueprintCallable, Category="Vael|World")
	static bool FillTextureArray(UTexture2DArray* Array, const TArray<UTexture2D*>& Textures);

	/** One actor for a patch of land with a hierarchical instanced mesh per batch: thousands of scenery pieces as a few
	 *  hundred actors, streamed by World Partition like any other actor. Returns the actor. */
	UFUNCTION(BlueprintCallable, Category="Vael|World", meta = (WorldContext = "WorldContext"))
	static AActor* SpawnInstanceCell(UObject* WorldContext, FVector Origin, const TArray<FVaelInstanceBatch>& Batches, FName Tag,
		const FString& Folder, const FString& Label);
};
