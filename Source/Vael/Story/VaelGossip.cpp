// Copyright Epic Games, Inc. All Rights Reserved.

#include "Story/VaelGossip.h"
#include "Story/VaelQuestSubsystem.h"

int32 UVaelGossip::PickLine(const UObject* WorldContext, int32 LastIndex) const
{
	const UVaelQuestSubsystem* Quests = UVaelQuestSubsystem::Get(WorldContext);

	TArray<int32> Usual;
	TArray<int32> Decided;
	for (int32 Index = 0; Index < Lines.Num(); ++Index)
	{
		if (Index == LastIndex || Lines[Index].Line.IsEmpty())
		{
			continue;
		}

		if (Lines[Index].RequiredFlag.IsNone())
		{
			Usual.Add(Index);
		}
		else if (Quests != nullptr && Quests->HasFlag(Lines[Index].RequiredFlag))
		{
			Decided.Add(Index);
		}
	}

	// What the group decided is the talk of the camp
	const TArray<int32>& Pool = !Decided.IsEmpty() && (Usual.IsEmpty() || FMath::RandBool()) ? Decided : Usual;
	return Pool.IsEmpty() ? INDEX_NONE : Pool[FMath::RandRange(0, Pool.Num() - 1)];
}
