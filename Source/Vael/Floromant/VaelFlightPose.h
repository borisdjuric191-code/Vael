// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

/** How the body of a flying Floromant hangs in the air this frame, worked out by the character for the animation; everything in the space of the mesh */
struct FVaelFlightPose
{
	/** 0 while she stands on the ground to fire, 1 while she flies */
	float Weight = 0.0f;

	/** Tip of the whole body around the pelvis */
	FQuat Tilt = FQuat::Identity;

	/** Way and degrees the feet swing out from under the body (horizontal, length = degrees) */
	FVector LegSwing = FVector::ZeroVector;

	/** Extra paddle of each leg in degrees, forward positive: left, right */
	float LegKick[2] = { 0.0f, 0.0f };

	/** Bend of the knees and point of the toes, in degrees */
	float KneeBend = 0.0f;
	float FootPoint = 0.0f;
};
