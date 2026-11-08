// Copyright Epic Games, Inc. All Rights Reserved.

#include "Debug/VaelWorldBuilder.h"
#include "Engine/World.h"
#include "Landscape.h"
#include "LandscapeInfo.h"
#include "LandscapeSubsystem.h"
#include "Materials/MaterialInterface.h"
#include "Misc/FileHelper.h"
#include "Engine/Texture2D.h"
#include "Engine/Texture2DArray.h"
#include "Engine/StaticMesh.h"
#include "Engine/Engine.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "Vael.h"

#if WITH_EDITOR
#include "Exporters/Exporter.h"
#include "Factories/LevelFactory.h"
#include "Misc/StringOutputDevice.h"
#include "UnrealExporter.h"
#endif

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

FString UVaelWorldBuilder::CopyActorsAsText(UObject* WorldContext, const TArray<AActor*>& Actors)
{
	FStringOutputDevice Text;
#if WITH_EDITOR
	// As the editor's Ctrl+C does, but with the actors given instead of the selection (works in a commandlet too)
	UWorld* World = WorldContext != nullptr ? WorldContext->GetWorld() : nullptr;
	if (World != nullptr)
	{
		const FSelectedActorExportObjectInnerContext Context(Actors.FilterByPredicate([](const AActor* Actor) { return Actor != nullptr; }));
		UExporter::ExportToOutputDevice(&Context, World, nullptr, Text, TEXT("copy"), 0, PPF_DeepCompareInstances | PPF_ExportsNotFullyQualified, true);
	}
#endif
	return Text;
}

TArray<AActor*> UVaelWorldBuilder::PasteActorsFromText(UObject* WorldContext, const FString& Text)
{
	TArray<AActor*> Pasted;
#if WITH_EDITOR
	// As the editor's Ctrl+V does: the level factory makes the actors where they were; the new ones are those that weren't there
	UWorld* World = WorldContext != nullptr ? WorldContext->GetWorld() : nullptr;
	if (World == nullptr || Text.IsEmpty())
	{
		return Pasted;
	}
	ULevel* Level = World->PersistentLevel;
	TSet<AActor*> Before(Level->Actors);
	ULevelFactory* Factory = NewObject<ULevelFactory>();
	const TCHAR* Start = *Text;
	Factory->FactoryCreateText(ULevel::StaticClass(), Level, Level->GetFName(), RF_Transactional, nullptr, TEXT("paste"), Start, Start + Text.Len(), GWarn);
	for (AActor* Actor : Level->Actors)
	{
		if (Actor != nullptr && !Before.Contains(Actor))
		{
			Pasted.Add(Actor);
		}
	}
#endif
	return Pasted;
}

bool UVaelWorldBuilder::FillTextureArray(UTexture2DArray* Array, const TArray<UTexture2D*>& Textures)
{
#if WITH_EDITOR
	if (Array == nullptr || Textures.IsEmpty())
	{
		return false;
	}
	Array->Modify();
	Array->SourceTextures.Reset();
	for (UTexture2D* Texture : Textures)
	{
		Array->SourceTextures.Add(Texture);
	}
	const bool bDone = Array->UpdateSourceFromSourceTextures(true);
	Array->PostEditChange();
	Array->MarkPackageDirty();
	return bDone;
#else
	return false;
#endif
}

AActor* UVaelWorldBuilder::SpawnInstanceCell(UObject* WorldContext, FVector Origin, const TArray<FVaelInstanceBatch>& Batches, FName Tag,
	const FString& Folder, const FString& Label)
{
	UWorld* World = GEngine != nullptr ? GEngine->GetWorldFromContextObject(WorldContext, EGetWorldErrorMode::LogAndReturnNull) : nullptr;
	if (World == nullptr)
	{
		return nullptr;
	}
	AActor* Actor = World->SpawnActor<AActor>(AActor::StaticClass(), FTransform(Origin));
	if (Actor == nullptr)
	{
		return nullptr;
	}
	USceneComponent* Root = NewObject<USceneComponent>(Actor, TEXT("Zelle"));
	Root->CreationMethod = EComponentCreationMethod::Instance;
	Root->SetMobility(EComponentMobility::Static);
	Actor->SetRootComponent(Root);
	Actor->AddInstanceComponent(Root);
	Root->RegisterComponent();
	Root->SetWorldLocation(Origin);

	for (const FVaelInstanceBatch& Batch : Batches)
	{
		if (Batch.Mesh == nullptr || Batch.Transforms.IsEmpty())
		{
			continue;
		}
		UHierarchicalInstancedStaticMeshComponent* Instances = NewObject<UHierarchicalInstancedStaticMeshComponent>(Actor,
			MakeUniqueObjectName(Actor, UHierarchicalInstancedStaticMeshComponent::StaticClass(), Batch.Mesh->GetFName()));
		Instances->CreationMethod = EComponentCreationMethod::Instance;
		Instances->SetMobility(EComponentMobility::Static);
		Instances->SetStaticMesh(Batch.Mesh);
		for (int32 Slot = 0; Slot < Batch.Materials.Num(); ++Slot)
		{
			if (Batch.Materials[Slot] != nullptr)
			{
				Instances->SetMaterial(Slot, Batch.Materials[Slot]);
			}
		}
		Instances->SetupAttachment(Root);
		Actor->AddInstanceComponent(Instances);
		Instances->RegisterComponent();
		if (Batch.bCollision)
		{
			Instances->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);
		}
		else
		{
			Instances->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			Instances->SetCanEverAffectNavigation(false);
		}
		if (Batch.CullDistance > 0.0f)
		{
			Instances->SetCullDistances(0, FMath::RoundToInt32(Batch.CullDistance));
		}
		Instances->AddInstances(Batch.Transforms, false, true);
	}
	Actor->Tags.Add(Tag);
#if WITH_EDITOR
	Actor->SetFolderPath(FName(*Folder));
	Actor->SetActorLabel(Label);
#endif
	return Actor;
}
