// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "VaelWorldMap.generated.h"

class UTexture2D;

/** A name written on the map, like "Ewiger Hain" */
USTRUCT(BlueprintType)
struct FVaelMapLabel
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Map")
	FText Name;

	/** Where the name stands, in world cm (X, Y) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Map")
	FVector2D Location = FVector2D::ZeroVector;

	/** 0 a small place, 1 an area, 2 the heart of the region */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Map", meta = (ClampMin = 0, ClampMax = 2))
	int32 Size = 1;
};

/**
 *  The map of a level for the mini map and the big map: a square picture lying over a square of the world, and the names on it.
 *  Which level uses which map is set under Project Settings > Game > Vael UI > Level Maps.
 *  Made by Scripts/CreateWorldMaps.py from the picture of the blockout kit (wf_karte.py).
 */
UCLASS(BlueprintType)
class UVaelWorldMap : public UDataAsset
{
	GENERATED_BODY()

public:

	/** Name of the map, shown over the big map */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Map")
	FText Title;

	/** The painted map, north (world -Y) at the top of the picture, world +X to the right */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Map")
	TSoftObjectPtr<UTexture2D> Picture;

	/** World corner (X, Y) at the top left of the picture, in cm */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Map")
	FVector2D WorldMin = FVector2D::ZeroVector;

	/** Width and height of the world square the picture covers, in cm */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Map", meta = (ClampMin = 100))
	float WorldSize = 100000.0f;

	/** World rectangle the big map shows at least, the land without the open sea around it (X, Y min and max in cm); empty: the whole picture */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Map")
	FVector2D FocusMin = FVector2D::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Map")
	FVector2D FocusMax = FVector2D::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Map")
	TArray<FVaelMapLabel> Labels;

	/** Place in the picture (0..1) of a world location */
	FVector2D WorldToPicture(const FVector2D& World) const { return (World - WorldMin) / FMath::Max(WorldSize, 1.0f); }
};
