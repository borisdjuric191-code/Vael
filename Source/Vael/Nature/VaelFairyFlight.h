// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

class UInstancedStaticMeshComponent;

/**
 *  How the fairies of the Feenragwurz fly, wherever they are: around an orchid, or at the side of a Floromant.
 *  They have no skeleton: wings and legs are bent by the material from the custom data of their instances
 *  (0/1 = upper/lower wing angle, 2 = legs trailing, 3 = legs swinging, in radians).
 */
namespace VaelFairyFlight
{
	/** One fairy on her way */
	struct FFairy
	{
		FVector Location = FVector::ZeroVector;
		FVector Velocity = FVector::ZeroVector;
		float Yaw = 0.0f;
		float Pitch = 0.0f;
		float Roll = 0.0f;
		float BeatPhase = 0.0f;
		float Seed = 0.0f;
	};

	struct FSettings
	{
		/** Top flying speed in cm/s */
		float FlySpeed = 320.0f;

		/** Wing beats per second while hovering and at top speed */
		FVector2D BeatRate = FVector2D(5.0f, 9.0f);

		/** Size against the mesh */
		float Scale = 1.0f;
	};

	/** A small figure of eight around a place while she hovers */
	FVector Drift(const FFairy& Fairy, float Clock);

	/** Flies her towards a goal and turns and leans her. She looks where she flies, slowly along HoverLook */
	void Steer(FFairy& Fairy, const FVector& Goal, const FVector& HoverLook, float DeltaSeconds, const FSettings& Settings);

	/** Writes her into her instance; bLast sends the changes of the frame to the renderer */
	void Show(UInstancedStaticMeshComponent* Fairies, int32 Index, const FFairy& Fairy, float Clock, const FSettings& Settings, bool bLast);
}
