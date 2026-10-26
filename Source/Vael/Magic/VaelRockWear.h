// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "VaelRockWear.generated.h"

class UPrimitiveComponent;

/**
 *  Remembers how much earth mages have torn out of each rock of the level (a placed rock, or one rock of a scattered set).
 *  A rock gives earth a few times, then it is worn out for a while.
 */
UCLASS()
class UVaelRockWear : public UWorldSubsystem
{
	GENERATED_BODY()

public:

	/** The wear of the world of the object, null outside a game world */
	static UVaelRockWear* Get(const UObject* WorldContext);

	/** True if the rock (item for one rock of a scattered set, else INDEX_NONE) can't give earth right now */
	bool IsWornOut(const UPrimitiveComponent* Rock, int32 Item) const;

	/** Earth is torn out of the rock once; returns how often it can still give earth */
	int32 Wear(const UPrimitiveComponent* Rock, int32 Item);

private:

	struct FRockKey
	{
		TWeakObjectPtr<const UPrimitiveComponent> Rock;
		int32 Item = INDEX_NONE;

		bool operator==(const FRockKey& Other) const { return Rock == Other.Rock && Item == Other.Item; }
		friend uint32 GetTypeHash(const FRockKey& Key) { return HashCombine(GetTypeHash(Key.Rock), ::GetTypeHash(Key.Item)); }
	};

	struct FRockState
	{
		/** Times earth was torn out since it was last whole */
		int32 Uses = 0;

		/** World time it was worn out at */
		double WornOutTime = 0.0;
	};

	/** True if the rock has given all its earth and not recovered yet */
	bool IsWornOut(const FRockState& State) const;

	TMap<FRockKey, FRockState> Rocks;
};
