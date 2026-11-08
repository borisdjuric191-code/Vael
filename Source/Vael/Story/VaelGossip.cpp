// Copyright Epic Games, Inc. All Rights Reserved.

#include "Story/VaelGossip.h"
#include "Story/VaelQuestSubsystem.h"

void UVaelGossip::GatherLines(TArray<const FVaelGossipLine*>& OutLines) const
{
	TArray<const UVaelGossip*> Visited;
	GatherLines(OutLines, Visited);
}

void UVaelGossip::GatherLines(TArray<const FVaelGossipLine*>& OutLines, TArray<const UVaelGossip*>& Visited) const
{
	if (Visited.Contains(this))
	{
		return;
	}
	Visited.Add(this);

	for (const FVaelGossipLine& Line : Lines)
	{
		OutLines.Add(&Line);
	}
	for (const UVaelGossip* Included : Includes)
	{
		if (Included != nullptr)
		{
			Included->GatherLines(OutLines, Visited);
		}
	}
}

const FVaelGossipLine* UVaelGossip::GetLine(int32 Index) const
{
	TArray<const FVaelGossipLine*> All;
	GatherLines(All);
	return All.IsValidIndex(Index) ? All[Index] : nullptr;
}

int32 UVaelGossip::PickLine(const UObject* WorldContext, int32 LastIndex, const FVaelNpcIdentity* Speaker) const
{
	const UVaelQuestSubsystem* Quests = UVaelQuestSubsystem::Get(WorldContext);

	TArray<const FVaelGossipLine*> All;
	GatherLines(All);

	TArray<int32> Usual;
	TArray<int32> Decided;
	for (int32 Index = 0; Index < All.Num(); ++Index)
	{
		const FVaelGossipLine& Line = *All[Index];
		if (Index == LastIndex || Line.Line.IsEmpty())
		{
			continue;
		}

		// Lines of a role or an attitude only from people who have it
		if (!Line.Role.IsNone() && (Speaker == nullptr || Speaker->Role != Line.Role))
		{
			continue;
		}
		if (Line.Prophet != EVaelProphetAttitude::None && (Speaker == nullptr || Speaker->Prophet != Line.Prophet))
		{
			continue;
		}

		if (Line.RequiredFlag.IsNone())
		{
			Usual.Add(Index);
		}
		else if (Quests != nullptr && Quests->HasFlag(Line.RequiredFlag))
		{
			Decided.Add(Index);
		}
	}

	// What the group decided is the talk of the camp
	const TArray<int32>& Pool = !Decided.IsEmpty() && (Usual.IsEmpty() || FMath::RandBool()) ? Decided : Usual;
	return Pool.IsEmpty() ? INDEX_NONE : Pool[FMath::RandRange(0, Pool.Num() - 1)];
}
