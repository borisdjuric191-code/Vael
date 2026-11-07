// Copyright Epic Games, Inc. All Rights Reserved.

#include "Items/VaelMaterialBag.h"
#include "Items/VaelPotionEffects.h"
#include "Player/VaelCharacter.h"
#include "Save/VaelSaveGame.h"
#include "Items/VaelMaterial.h"
#include "Items/VaelMaterialDrop.h"
#include "Engine/World.h"
#include "UI/VaelNoticeSubsystem.h"
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

bool UVaelMaterialBag::UseMaterial(const UVaelMaterial* Material)
{
	if (Material == nullptr || Material->Use == EVaelMaterialUse::None || GetCount(Material) <= 0)
	{
		return false;
	}

	// Potions of the witch's kitchen: drunk or thrown (UVaelPotionEffects)
	if (Material->Category == EVaelMaterialCategory::Potion)
	{
		if (AVaelCharacter* Player = Cast<AVaelCharacter>(GetOwner()))
		{
			return UVaelPotionEffects::Use(Player, Material);
		}
	}

	// Pouring over plants (the plant takes its light or shadow form in its next growth cycle) comes later
	UVaelNoticeSubsystem::Post(GetOwner(), Material->DisplayName, LOCTEXT("UseLater", "Das geht noch nicht."), Material->Color, 2.0f);
	return false;
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
		if (Pair.Key == nullptr)
		{
			continue;
		}

		// A material with a stack size takes as many fields as it needs
		const int32 PerField = Pair.Key->StackSize > 0 ? Pair.Key->StackSize : Pair.Value;
		for (int32 Left = Pair.Value; Left > 0; Left -= PerField)
		{
			Stacks.Add({ Pair.Key, FMath::Min(Left, PerField) });
		}
	}

	Stacks.StableSort([](const FVaelMaterialStack& A, const FVaelMaterialStack& B)
	{
		// potions first, so what was brewed at the root table is always in sight and at hand
		const bool bPotionA = A.Material->Category == EVaelMaterialCategory::Potion;
		const bool bPotionB = B.Material->Category == EVaelMaterialCategory::Potion;
		if (bPotionA != bPotionB)
		{
			return bPotionA;
		}
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

		// Loot with a model falls to the ground, one copy per player in a small ring, and waits to be picked up
		const bool bOnGround = Material->LiesOnGround();
		int32 PlayerIndex = 0;
		const float Turn = FMath::FRandRange(0.0f, UE_TWO_PI);
		for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
		{
			const APlayerController* PlayerController = It->Get();
			AVaelCharacter* Player = PlayerController != nullptr ? PlayerController->GetPawn<AVaelCharacter>() : nullptr;
			if (Player == nullptr)
			{
				continue;
			}

			if (bOnGround)
			{
				const float Angle = Turn + PlayerIndex++ * UE_TWO_PI / 4.0f;
				const FVector Spot = Source->GetActorLocation() + FVector(FMath::Cos(Angle), FMath::Sin(Angle), 0.0f) * FMath::FRandRange(60.0f, 110.0f);
				AVaelMaterialDrop::DropMaterial(World, Spot, Material, Count, Player);
			}
			else
			{
				Player->GetMaterialBag()->AddMaterial(Material, Count);
			}
		}

		if (bOnGround)
		{
			continue;
		}

		const FText Label = Count > 1 ? FText::Format(LOCTEXT("LootCount", "{0} ×{1}"), Material->DisplayName, Count) : Material->DisplayName;
		UVaelCombatTextSubsystem::PostPickup(Source, Label, Material->Color);

		UE_LOG(LogVael, Verbose, TEXT("'%s' gives %d x %s to every player"), *GetNameSafe(Source), Count, *Material->DisplayName.ToString());
	}
}

#undef LOCTEXT_NAMESPACE

void UVaelMaterialBag::WriteSave(FVaelSavedPlayer& Saved) const
{
	Saved.Materials.Reset();
	for (const TPair<TObjectPtr<const UVaelMaterial>, int32>& Stack : Counts)
	{
		if (Stack.Key != nullptr && Stack.Value > 0)
		{
			Saved.Materials.Add(FSoftObjectPath(Stack.Key.Get()), Stack.Value);
		}
	}
}

void UVaelMaterialBag::ReadSave(const FVaelSavedPlayer& Saved)
{
	Counts.Reset();
	for (const TPair<FSoftObjectPath, int32>& Stack : Saved.Materials)
	{
		// Materials that left the game since the save was written are dropped
		if (const UVaelMaterial* Material = Cast<UVaelMaterial>(Stack.Key.TryLoad()))
		{
			Counts.Add(Material, Stack.Value);
		}
	}

	OnMaterialsChanged.Broadcast();
}
