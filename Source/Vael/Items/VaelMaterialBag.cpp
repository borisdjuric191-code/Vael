// Copyright Epic Games, Inc. All Rights Reserved.

#include "Items/VaelMaterialBag.h"
#include "Save/VaelSaveSubsystem.h"
#include "Items/VaelPotionEffects.h"
#include "Nature/VaelGarden.h"
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

	// Klartrunk and Nidtrunk are poured over a plant in a bed: it grows on in light or in shadow form (Die Waage)
	const FString Name = Material->GetName();
	if (Name == TEXT("DA_Material_Klartrunk") || Name == TEXT("DA_Material_Nidtrunk"))
	{
		AVaelCharacter* Player = Cast<AVaelCharacter>(GetOwner());
		UVaelGardenSubsystem* Garden = UVaelGardenSubsystem::Get(Player);
		if (Player != nullptr && Garden != nullptr && Garden->TurnPlant(Player, Name == TEXT("DA_Material_Nidtrunk"), 260.0f))
		{
			RemoveMaterial(Material, 1);
			return true;
		}
		UVaelNoticeSubsystem::Post(GetOwner(), Material->DisplayName, LOCTEXT("PourNear", "Stell dich an ein Beet mit einer Pflanze der anderen Form und gieß ihn darüber."), Material->Color, 3.0f);
		return false;
	}

	// Potions of the witch's kitchen: drunk or thrown (UVaelPotionEffects)
	if (Material->Category == EVaelMaterialCategory::Potion || Material->IsPotion())
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

bool UVaelMaterialBag::IsInTab(const UVaelMaterial* Material, int32 Tab)
{
	if (Material == nullptr)
	{
		return false;
	}
	// older assets keep the default category: potions are known by their heal, seeds and crystals by their names
	const FString Name = Material->GetName();
	const bool bPotion = Material->Category == EVaelMaterialCategory::Potion || Material->IsPotion();
	const bool bSeed = Material->Category == EVaelMaterialCategory::Seed || Name.StartsWith(TEXT("DA_Material_Samen_")) || Name == TEXT("DA_Material_Ueberraschungssaat");
	const bool bCrystal = Material->Category == EVaelMaterialCategory::Crystal || Name.Contains(TEXT("Kristall"));
	switch (Tab)
	{
	case 1: return bPotion;
	case 2: return bCrystal && !bPotion;
	case 3: return !bPotion && !bSeed && !bCrystal;
	case 4: return bSeed;
	default: return false;
	}
}

TArray<FVaelMaterialStack> UVaelMaterialBag::GetStacks(int32 Tab) const
{
	TArray<FVaelMaterialStack> Stacks = GetStacks();
	Stacks.RemoveAll([Tab](const FVaelMaterialStack& Stack) { return !IsInTab(Stack.Material, Tab); });
	return Stacks;
}

int32 UVaelMaterialBag::ToggleBelt(const UVaelMaterial* Potion)
{
	if (!IsInTab(Potion, 1))
	{
		return -1;
	}
	Belt.SetNum(BeltSize);
	const int32 Existing = Belt.IndexOfByKey(Potion);
	if (Existing != INDEX_NONE)
	{
		Belt[Existing] = nullptr;
		OnMaterialsChanged.Broadcast();
		return -1;
	}
	int32 Place = Belt[ActiveBelt] == nullptr ? ActiveBelt : Belt.IndexOfByKey(nullptr);
	Place = Place == INDEX_NONE ? ActiveBelt : Place;
	Belt[Place] = Potion;
	ActiveBelt = Place;
	OnMaterialsChanged.Broadcast();
	UVaelSaveSubsystem::RequestSave(GetOwner());
	return Place;
}

void UVaelMaterialBag::CycleBelt()
{
	Belt.SetNum(BeltSize);
	for (int32 Step = 1; Step <= BeltSize; ++Step)
	{
		const int32 Place = (ActiveBelt + Step) % BeltSize;
		if (Belt[Place] != nullptr)
		{
			ActiveBelt = Place;
			UVaelCombatTextSubsystem::PostPickup(GetOwner(), FText::Format(NSLOCTEXT("VaelItems", "BeltPlace", "Gürtel: {0}"), Belt[Place]->DisplayName), Belt[Place]->Color);
			return;
		}
	}
}

bool UVaelMaterialBag::UseBelt()
{
	Belt.SetNum(BeltSize);
	const UVaelMaterial* Potion = Belt[ActiveBelt];
	if (Potion == nullptr || GetCount(Potion) <= 0)
	{
		return false;
	}
	return UseMaterial(Potion);
}

void UVaelMaterialBag::WriteSave(FVaelSavedPlayer& Saved) const
{
	Saved.Belt.Reset();
	for (const TObjectPtr<const UVaelMaterial>& Potion : Belt)
	{
		Saved.Belt.Add(Potion != nullptr ? FSoftObjectPath(Potion.Get()) : FSoftObjectPath());
	}
	Saved.ActiveBelt = ActiveBelt;
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
	Belt.Reset();
	for (const FSoftObjectPath& Path : Saved.Belt)
	{
		Belt.Add(Path.IsValid() ? Cast<UVaelMaterial>(Path.TryLoad()) : nullptr);
	}
	Belt.SetNum(BeltSize);
	ActiveBelt = FMath::Clamp(Saved.ActiveBelt, 0, BeltSize - 1);
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
