// Copyright Epic Games, Inc. All Rights Reserved.

#include "Items/VaelMaterialBag.h"
#include "Items/VaelMaterial.h"

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
