// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "VaelLegIKComponent.generated.h"

class UPoseableMeshComponent;

DECLARE_MULTICAST_DELEGATE(FVaelOnLegsPosed);

/**
 *  Procedural legs for the body family "Vielbeiner" (Spannhornkaefer, Glutkriecher, Kettenkrabben):
 *  every foot stays planted on the ground until its body has moved too far, then steps forward in an arc,
 *  the legs in two alternating groups (a tripod gait for six legs). Each leg is bent by inverse kinematics,
 *  the body follows the feet up and down slopes and tilts with them.
 *
 *  Needs no animation asset: it poses the bones of a poseable mesh directly. Legs are found by their fixed
 *  bone names leg_<L|R><1-n>_coxa / _femur / _tibia / _foot under the body bone, as the Blender script of the
 *  creature factory names them. Every value can be tuned in the editor.
 */
UCLASS(ClassGroup=(Vael), meta = (BlueprintSpawnableComponent))
class UVaelLegIKComponent : public UActorComponent
{
	GENERATED_BODY()

public:

	/** Bone that carries the legs; tilted and lifted to follow the feet */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Legs")
	FName BodyBone = TEXT("body");

	/** A planted foot steps once its resting spot is this far away, in cm */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Gait", meta = (ClampMin = 1))
	float StepDistance = 34.0f;

	/** Longest time of a step in seconds; steps get quicker when the body walks fast */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Gait", meta = (ClampMin = 0.02))
	float StepDuration = 0.2f;

	/** Shortest time of a step in seconds */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Gait", meta = (ClampMin = 0.02))
	float MinStepDuration = 0.09f;

	/** Height of the arc of a step in cm */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Gait", meta = (ClampMin = 0))
	float StepHeight = 16.0f;

	/** A step aims this many seconds of walking ahead, so the feet don't trail behind */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Gait", meta = (ClampMin = 0))
	float StepLead = 0.12f;

	/** The ground is searched from this high above a resting spot down to this deep below it, in cm */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Ground", meta = (ClampMin = 0))
	float TraceUp = 70.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Ground", meta = (ClampMin = 0))
	float TraceDown = 110.0f;

	/** The body tilts with the feet up to this many degrees */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Body", meta = (ClampMin = 0, ClampMax = 60))
	float MaxBodyTilt = 22.0f;

	/** Share of the height of the feet the body follows, 0 keeps it at the capsule */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Body", meta = (ClampMin = 0, ClampMax = 1))
	float BodyHeightFollow = 0.8f;

	/** How quickly the body settles into its tilt and height, higher is quicker */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Body", meta = (ClampMin = 0))
	float BodySmoothing = 9.0f;

	/** Up and down of the body with every step, in cm */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Body", meta = (ClampMin = 0))
	float BodyBob = 2.5f;

	/** Constructor */
	UVaelLegIKComponent();

	/** Finds the legs of the mesh and plants the feet where they stand. Call again after changing the mesh. */
	void Setup(UPoseableMeshComponent* InMesh);

	/** Plants every foot at its resting spot at once, after a teleport or a spawn */
	void PlantAllFeet();

	/** Pulls the feet under the body and lifts them, 0 standing, 1 curled up like a dead beetle */
	void SetCurl(float InCurl) { Curl = FMath::Clamp(InCurl, 0.0f, 1.0f); }

	/** Jolts the body away from a hit, in cm along a world direction; it springs back by itself */
	void AddBodyJolt(const FVector& WorldOffset);

	/** Extra tilt of the body, like rearing up to shoot: pitch and roll in degrees, kept until changed */
	void SetExtraBodyTilt(const FRotator& InTilt) { ExtraTilt = InTilt; }

	/** Number of legs found */
	int32 GetNumLegs() const { return Legs.Num(); }

	/** Number of feet in the air right now */
	int32 GetNumStepping() const;

	/** Steps taken since the setup, for tests */
	int32 GetStepCount() const { return StepCount; }

	/** World location of a foot */
	FVector GetFootLocation(int32 LegIndex) const;

	/** Called after the legs are posed each frame, so the creature can pose other bones like horns on top */
	FVaelOnLegsPosed OnLegsPosed;

	/** Poses the legs */
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

private:

	/** One leg and the state of its foot */
	struct FVaelLeg
	{
		FName Coxa;
		FName Femur;
		FName Tibia;
		FName Foot;

		/** Where the foot rests in the reference pose, in component space */
		FVector RestFoot = FVector::ZeroVector;

		/** Group that steps together; the groups take turns */
		int32 Group = 0;

		/** World location where the foot stands, or stood when its step began */
		FVector Planted = FVector::ZeroVector;

		/** Target of the current step */
		FVector StepTarget = FVector::ZeroVector;

		/** Progress of the step from 0 to 1; 1 while planted */
		float StepAlpha = 1.0f;

		/** Seconds the current step takes */
		float StepTime = 0.2f;

		/** World location of the foot this frame */
		FVector Current = FVector::ZeroVector;
	};

	/** Resting spot of a foot on the ground in the world, with the lead of the walk */
	FVector FindRestingSpot(const FVaelLeg& Leg, const FVector& Lead) const;

	/** Moves the feet: starts steps where needed and carries on the steps under way */
	void UpdateFeet(float DeltaTime);

	/** Tilts and lifts the body bone to the feet */
	void PoseBody(float DeltaTime);

	/** Bends one leg so its foot reaches the target, in world space */
	void SolveLeg(const FVaelLeg& Leg, const FVector& WorldTarget);

	UPROPERTY(Transient)
	TObjectPtr<UPoseableMeshComponent> Mesh;

	TArray<FVaelLeg> Legs;

	/** Bones reset to the reference pose before posing each frame */
	TArray<FName> PosedBones;

	/** Reference transform of the body bone in component space */
	FTransform RestBody = FTransform::Identity;

	/** Smoothed tilt and height of the body */
	FQuat BodyTilt = FQuat::Identity;
	float BodyHeight = 0.0f;

	/** Jolt of the body from hits, in world space, and its speed */
	FVector Jolt = FVector::ZeroVector;
	FVector JoltVelocity = FVector::ZeroVector;

	FRotator ExtraTilt = FRotator::ZeroRotator;

	/** Where the actor was last frame, for the walking speed */
	FVector LastActorLocation = FVector::ZeroVector;

	float Curl = 0.0f;

	int32 StepCount = 0;
};
