// Copyright Epic Games, Inc. All Rights Reserved.

#include "Nature/VaelFairyFlight.h"
#include "Components/InstancedStaticMeshComponent.h"

namespace
{
	/** The fairy mesh looks along +Y, an actor along +X */
	constexpr float MeshYaw = -90.0f;

	/** The middle of her body above her feet in the mesh, in cm: she turns and leans around it */
	constexpr float BodyHeight = 34.0f;

	/** How hard she is pulled to her target and how far from it she starts to slow down, in cm */
	constexpr float Steering = 6.0f;
	constexpr float BrakeDistance = 90.0f;

	/** Lean into the flight and into a turn at top speed, in degrees */
	constexpr float FlightLean = 48.0f;
	constexpr float TurnLean = 30.0f;

	/** How fast she turns, 1/s */
	constexpr float TurnSpeed = 6.0f;

	/** Hovering she drifts on a small figure of eight, in cm and radians per second */
	constexpr float DriftReach = 5.0f;
	constexpr float DriftSpeed = 1.4f;

	/** Wing angles in radians, 0 = as modelled, positive = forward: the middle of the beat and how far it swings */
	constexpr float BeatMiddle = -0.3f;
	constexpr float BeatSwing = 0.55f;

	/** The lower wings follow the upper ones by this much of a beat */
	constexpr float LowerWingLag = 0.9f;

	/** Legs trailing behind at top speed and hanging a little behind while hovering, and their slow swing, in radians */
	constexpr float LegTrailFlight = 0.95f;
	constexpr float LegTrailHover = 0.12f;
	constexpr float LegSwing = 0.13f;
}

namespace VaelFairyFlight
{
	FVector Drift(const FFairy& Fairy, float Clock)
	{
		const float Phase = Clock * DriftSpeed + Fairy.Seed;
		return FVector(FMath::Sin(Phase), FMath::Sin(Phase * 0.5f), FMath::Sin(Phase * 1.7f)) * DriftReach;
	}

	void Steer(FFairy& Fairy, const FVector& Goal, const FVector& HoverLook, float DeltaSeconds, const FSettings& Settings)
	{
		// Heads for the goal at full speed and slows down on the last stretch
		const FVector ToGoal = Goal - Fairy.Location;
		const float Speed = Settings.FlySpeed * FMath::Min(ToGoal.Size() / BrakeDistance, 1.0f);
		const FVector Wanted = ToGoal.GetSafeNormal() * Speed;
		const FVector Before = Fairy.Velocity;
		Fairy.Velocity = FMath::VInterpTo(Fairy.Velocity, Wanted, DeltaSeconds, Steering);
		Fairy.Location += Fairy.Velocity * DeltaSeconds;

		const float SpeedShare = FMath::Min(Fairy.Velocity.Size() / Settings.FlySpeed, 1.0f);
		const FVector Look = SpeedShare > 0.25f ? Fairy.Velocity : HoverLook;
		const float OldYaw = Fairy.Yaw;
		Fairy.Yaw = FMath::RInterpTo(FRotator(0.0f, Fairy.Yaw, 0.0f), FRotator(0.0f, Look.Rotation().Yaw, 0.0f), DeltaSeconds, TurnSpeed).Yaw;

		// Leans forward into the flight and sideways into the turn
		const FVector Forward = FRotator(0.0f, Fairy.Yaw, 0.0f).Vector();
		const float Along = FVector::DotProduct(Fairy.Velocity, Forward) / Settings.FlySpeed;
		const float Turning = DeltaSeconds > 0.0f ? FMath::FindDeltaAngleDegrees(OldYaw, Fairy.Yaw) / DeltaSeconds / 180.0f : 0.0f;
		Fairy.Pitch = FMath::FInterpTo(Fairy.Pitch, -FMath::Clamp(Along, -0.5f, 1.0f) * FlightLean, DeltaSeconds, TurnSpeed);
		Fairy.Roll = FMath::FInterpTo(Fairy.Roll, FMath::Clamp(Turning, -1.0f, 1.0f) * TurnLean, DeltaSeconds, TurnSpeed);

		// Beats harder the more she speeds up
		const float Effort = FMath::Min(SpeedShare + (Fairy.Velocity - Before).Size() / FMath::Max(DeltaSeconds, 0.001f) / (Settings.FlySpeed * Steering), 1.0f);
		Fairy.BeatPhase = FMath::Fmod(Fairy.BeatPhase + DeltaSeconds * UE_TWO_PI * FMath::Lerp(Settings.BeatRate.X, Settings.BeatRate.Y, Effort), UE_TWO_PI);
	}

	void Show(UInstancedStaticMeshComponent* Fairies, int32 Index, const FFairy& Fairy, float Clock, const FSettings& Settings, bool bLast)
	{
		// Turns around the middle of her body, her feet swing out behind her
		const FRotator Body(Fairy.Pitch, Fairy.Yaw, Fairy.Roll);
		const FQuat Turn = Body.Quaternion() * FRotator(0.0f, MeshYaw, 0.0f).Quaternion();
		const FVector Middle(0.0f, 0.0f, BodyHeight * Settings.Scale);
		const FVector Place = Fairy.Location + Middle - Turn.RotateVector(Middle);
		// With where she was, or the moving instance smears into a blur on screen
		FTransform Before;
		Fairies->GetInstanceTransform(Index, Before);
		Fairies->BatchUpdateInstancesTransforms(Index, { FTransform(Turn, Place, FVector(Settings.Scale)) }, { Before }, false, false, false);

		const float SpeedShare = FMath::Min(Fairy.Velocity.Size() / Settings.FlySpeed, 1.0f);
		const float Swing = FMath::Sin(Clock * 2.2f + Fairy.Seed);
		Fairies->SetCustomDataValue(Index, 0, BeatMiddle + BeatSwing * FMath::Sin(Fairy.BeatPhase), false);
		Fairies->SetCustomDataValue(Index, 1, BeatMiddle + BeatSwing * FMath::Sin(Fairy.BeatPhase - LowerWingLag), false);
		Fairies->SetCustomDataValue(Index, 2, FMath::Lerp(LegTrailHover, LegTrailFlight, SpeedShare), false);
		Fairies->SetCustomDataValue(Index, 3, LegSwing * Swing, bLast);
	}
}
