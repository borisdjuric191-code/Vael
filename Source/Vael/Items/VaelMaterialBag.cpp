// Copyright Epic Games, Inc. All Rights Reserved.

#include "Items/VaelMaterialBag.h"
#include "Items/VaelMaterial.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Player/VaelCharacter.h"
#include "UI/VaelCombatTextSubsystem.h"
#include "Vael.h"
#include "VaelAssets.h"

#define LOCTEXT_NAMESPACE "VaelItems"

void UVaelMaterialBag::AddMaterial(const UVaelMaterial* Material, int32 Count)
{
	if (Material == nullptr || Count <= 0)
	{
		return;
	}

	Counts.FindOrAdd(Material) += Count;
	OnMaterialsChanged.Broadcast();
}

bool UVaelMaterialBag::RemoveMaterial(const UVaelMaterial* Material, int32 Count)
{
	int32* Current = Counts.Find(Material);
	if (Current == nullptr || Count <= 0 || *Current < Count)
	{
		return false;
	}

	*Current -= Count;
	if (*Current == 0)
	{
		Counts.Remove(Material);
	}

	OnMaterialsChanged.Broadcast();
	return true;
}

int32 UVaelMaterialBag::GetCount(const UVaelMaterial* Material) const
{
	const int32* Count = Counts.Find(Material);
	return Count != nullptr ? *Count : 0;
}

TArray<FVaelMaterialStack> UVaelMaterialBag::GetStacks() const
{
	TArray<FVaelMaterialStack> Stacks;
	for (const TPair<TObjectPtr<const UVaelMaterial>, int32>& Pair : Counts)
	{
		if (Pair.Key != nullptr)
		{
			Stacks.Add({ Pair.Key, Pair.Value });
		}
	}

	Stacks.Sort([](const FVaelMaterialStack& A, const FVaelMaterialStack& B)
	{
		const int32 ByRegion = A.Material->Region.CompareTo(B.Material->Region);
		return ByRegion != 0 ? ByRegion < 0 : A.Material->DisplayName.CompareTo(B.Material->DisplayName) < 0;
	});

	return Stacks;
}

void UVaelMaterialBag::GiveToGroup(const AActor* Source, const TArray<FVaelLootEntry>& Loot)
{
	UWorld* World = Source != nullptr ? Source->GetWorld() : nullptr;
	if (World == nullptr)
	{
		return;
	}

	for (const FVaelLootEntry& Entry : Loot)
	{
		const int32 Count = Entry.Roll();
		const UVaelMaterial* Material = Count > 0 ? VaelAssets::LoadOptional(Entry.Material) : nullptr;
		if (Material == nullptr)
		{
			continue;
		}

		for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
		{
			const APlayerController* PlayerController = It->Get();
			const AVaelCharacter* Player = PlayerController != nullptr ? PlayerController->GetPawn<AVaelCharacter>() : nullptr;
			if (Player != nullptr)
			{
				Player->GetMaterialBag()->AddMaterial(Material, Count);
			}
		}

		const FText Label = Count > 1 ? FText::Format(LOCTEXT("LootCount", "{0} ×{1}"), Material->DisplayName, Count) : Material->DisplayName;
		UVaelCombatTextSubsystem::PostPickup(Source, Label, Material->Color);

		UE_LOG(LogVael, Verbose, TEXT("'%s' gives %d x %s to every player"), *GetNameSafe(Source), Count, *Material->DisplayName.ToString());
	}
}

#undef LOCTEXT_NAMESPACE
