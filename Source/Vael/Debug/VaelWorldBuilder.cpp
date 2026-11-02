// Copyright Epic Games, Inc. All Rights Reserved.

#include "Debug/VaelWorldBuilder.h"
#include "Engine/World.h"
#include "Landscape.h"
#include "LandscapeInfo.h"
#include "LandscapeSubsystem.h"
#include "Materials/MaterialInterface.h"
#include "Misc/FileHelper.h"
#include "Vael.h"

namespace
{
	/** Every landscape of the world scripts: 2 sections of 63 quads per component */
	constexpr int32 LandscapeSectionsPerComponent = 2;
	constexpr int32 LandscapeQuadsPerSection = 63;
}

AActor* UVaelWorldBuilder::CreateLandscapeFromRaw(UObject* WorldContext, const FString& RawFile, int32 SizeX, int32 SizeY, FVector Location, FVector Scale,
	UMaterialInterface* Material, int32 GridSize)
{
#if WITH_EDITOR
	UWorld* World = WorldContext != nullptr ? WorldContext->GetWorld() : nullptr;
	const int32 QuadsPerComponent = LandscapeSectionsPerComponent * LandscapeQuadsPerSection;
	if (World == nullptr || SizeX < 2 || SizeY < 2 || (SizeX - 1) % QuadsPerComponent != 0 || (SizeY - 1) % QuadsPerComponent != 0)
	{
		UE_LOG(LogVael, Error, TEXT("CreateLandscapeFromRaw: no world or sizes %d x %d are not whole components of %d quads"), SizeX, SizeY, QuadsPerComponent);
		return nullptr;
	}

	TArray<uint8> Bytes;
	if (!FFileHelper::LoadFileToArray(Bytes, *RawFile) || Bytes.Num() != SizeX * SizeY * 2)
	{
		UE_LOG(LogVael, Error, TEXT("CreateLandscapeFromRaw: %s missing or not %d x %d 16 bit values"), *RawFile, SizeX, SizeY);
		return nullptr;
	}
	TArray<uint16> Heights;
	Heights.SetNumUninitialized(SizeX * SizeY);
	FMemory::Memcpy(Heights.GetData(), Bytes.GetData(), Bytes.Num());

	TMap<FGuid, TArray<uint16>> HeightsPerLayer;
	HeightsPerLayer.Add(FGuid(), MoveTemp(Heights));
	TMap<FGuid, TArray<FLandscapeImportLayerInfo>> MaterialLayersPerLayer;
	MaterialLayersPerLayer.Add(FGuid(), TArray<FLandscapeImportLayerInfo>());

	ALandscape* Landscape = World->SpawnActor<ALandscape>(Location, FRotator::ZeroRotator);
	if (Landscape == nullptr)
	{
		return nullptr;
	}
	Landscape->LandscapeMaterial = Material;
	Landscape->SetActorRelativeScale3D(Scale);
	Landscape->Import(FGuid::NewGuid(), 0, 0, SizeX - 1, SizeY - 1, LandscapeSectionsPerComponent, LandscapeQuadsPerSection, HeightsPerLayer, *RawFile,
		MaterialLayersPerLayer, ELandscapeImportAlphamapType::Additive, TArrayView<const FLandscapeLayer>());

	// The heights land in an edit layer; the landscape itself only gets them when the layers are rendered together (needs the GPU)
	Landscape->ForceLayersFullUpdate();

	ULandscapeInfo* Info = Landscape->GetLandscapeInfo();
	if (Info != nullptr)
	{
		Info->UpdateLayerInfoMap(Landscape);
		if (GridSize > 0)
		{
			if (ULandscapeSubsystem* Landscapes = World->GetSubsystem<ULandscapeSubsystem>())
			{
				Landscapes->ChangeGridSize(Info, GridSize);
			}
		}
	}
	return Landscape;
#else
	return nullptr;
#endif
}
