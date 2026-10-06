// Copyright Epic Games, Inc. All Rights Reserved.

#include "World/VaelGround.h"
#include "Components/PrimitiveComponent.h"
#include "Engine/HitResult.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"

namespace VaelGround
{
	bool TraceGround(const UWorld* World, const FVector& Start, const FVector& End, FHitResult& OutHit, const AActor* IgnoredActor)
	{
		if (World == nullptr)
		{
			return false;
		}

		// Floors may be static or movable, depending on how the level was built
		FCollisionObjectQueryParams ObjectParams;
		ObjectParams.AddObjectTypesToQuery(ECC_WorldStatic);
		ObjectParams.AddObjectTypesToQuery(ECC_WorldDynamic);

		FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(VaelTraceGround), false, IgnoredActor);

		TArray<FHitResult> Hits;
		World->LineTraceMultiByObjectType(Hits, Start, End, ObjectParams, QueryParams);
		Hits.Sort([](const FHitResult& A, const FHitResult& B) { return A.Distance < B.Distance; });

		// Ground is whatever stops a character; projectiles, pickups and patches only overlap them
		for (const FHitResult& Hit : Hits)
		{
			const UPrimitiveComponent* Component = Hit.GetComponent();
			if (Component != nullptr && Component->GetCollisionResponseToChannel(ECC_Pawn) == ECR_Block && !Cast<APawn>(Hit.GetActor()))
			{
				OutHit = Hit;
				return true;
			}
		}

		return false;
	}
}
