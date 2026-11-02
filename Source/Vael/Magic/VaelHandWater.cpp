// Copyright Epic Games, Inc. All Rights Reserved.

#include "Magic/VaelHandWater.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Magic/VaelMagicSettings.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "VaelAssets.h"

namespace
{
	/** Size of the engine sphere in cm */
	constexpr float BeadSphereSize = 100.0f;

	/** Beads winding round each forearm: how many, how far round the arm in cm, how big at their fullest, turns per second */
	constexpr int32 CoilBeads = 9;
	constexpr float CoilRadius = 5.5f;
	constexpr float CoilBeadSize = 3.6f;
	constexpr float CoilTurnRate = 1.6f;

	/** Drops running off a hand per second while it gathers, and after the throw for DripAfter seconds */
	constexpr float GatherDrips = 26.0f;
	constexpr float AfterDrips = 7.0f;
	constexpr float DripAfter = 1.0f;

	/** Drops flung off each hand with the throw, how fast forward and up */
	constexpr int32 FlungDrops = 18;
	constexpr float FlingSpeedMin = 350.0f;
	constexpr float FlingSpeedMax = 650.0f;
	constexpr float FlingLiftMin = 120.0f;
	constexpr float FlingLiftMax = 300.0f;

	/** A drop lasts at most this long, falls with gravity, and is drawn out along its way by its speed */
	constexpr float DropLife = 1.2f;
	constexpr float DropGravity = 980.0f;
	constexpr float DropStretchSpeed = 120.0f;

	/** Gathering stops this long after the release, whatever happens */
	constexpr float GatherLimit = 1.6f;

	constexpr float WaterGlow = 1.6f;
}

UVaelHandWaterComponent::UVaelHandWaterComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	SetUsingAbsoluteLocation(true);
	SetUsingAbsoluteRotation(true);
	SetUsingAbsoluteScale(true);
}

void UVaelHandWaterComponent::Start(USkeletalMeshComponent* InBody, bool bInBothHands, float InRelease)
{
	Body = InBody;
	bBothHands = bInBothHands;
	Release = FMath::Max(InRelease, 0.05f);

	Beads = NewObject<UInstancedStaticMeshComponent>(GetOwner());
	Beads->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Sphere.Sphere")));
	Beads->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Beads->SetCastShadow(false);
	Beads->bReceivesDecals = false;
	Beads->SetUsingAbsoluteLocation(true);
	Beads->SetUsingAbsoluteRotation(true);
	Beads->SetUsingAbsoluteScale(true);
	Beads->SetupAttachment(this);
	Beads->SetWorldTransform(FTransform::Identity);
	Beads->RegisterComponent();

	const UVaelMagicSettings* MagicSettings = UVaelMagicSettings::Get();
	if (UMaterialInterface* IceMaterial = VaelAssets::LoadOptional(MagicSettings->HailIceMaterial))
	{
		UMaterialInstanceDynamic* Water = UMaterialInstanceDynamic::Create(IceMaterial, this);
		Water->SetVectorParameterValue(TEXT("Farbe"), MagicSettings->GetElementColor(EVaelElement::Water));
		Water->SetScalarParameterValue(TEXT("Leuchten"), WaterGlow);
		Beads->SetMaterial(0, Water);
	}
}

bool UVaelHandWaterComponent::GetHand(int32 Side, FVector& OutElbow, FVector& OutWrist, FVector& OutTips) const
{
	const USkeletalMeshComponent* Mesh = Body.Get();
	const TCHAR* Suffix = Side == 0 ? TEXT("r") : TEXT("l");
	const FName Lower(*FString::Printf(TEXT("lowerarm_%s"), Suffix));
	const FName Hand(*FString::Printf(TEXT("hand_%s"), Suffix));
	const FName Middle(*FString::Printf(TEXT("middle_03_%s"), Suffix));
	if (Mesh == nullptr || Mesh->GetBoneIndex(Lower) == INDEX_NONE || Mesh->GetBoneIndex(Hand) == INDEX_NONE)
	{
		return false;
	}
	OutElbow = Mesh->GetBoneLocation(Lower);
	OutWrist = Mesh->GetBoneLocation(Hand);
	OutTips = Mesh->GetBoneIndex(Middle) != INDEX_NONE ? Mesh->GetBoneLocation(Middle) : OutWrist + (OutWrist - OutElbow).GetSafeNormal() * 12.0f;
	return true;
}

void UVaelHandWaterComponent::Shed(const FVector& Location, const FVector& Velocity, float Size)
{
	FDrop& Drop = Drops.AddDefaulted_GetRef();
	Drop.Location = Location;
	Drop.Velocity = Velocity;
	Drop.Size = Size;
}

void UVaelHandWaterComponent::Fling(const FVector& Forward)
{
	if (bFlung || bFinished)
	{
		return;
	}
	bFlung = true;
	FlungAge = Age;

	// Everything that clung to the hand leaves it forward and a little up, the beads of the coil with it
	for (int32 Side = 0; Side < (bBothHands ? 2 : 1); ++Side)
	{
		FVector Elbow, Wrist, Tips;
		if (!GetHand(Side, Elbow, Wrist, Tips))
		{
			continue;
		}
		for (int32 Index = 0; Index < FlungDrops + CoilBeads; ++Index)
		{
			const FVector From = FMath::Lerp(Elbow, Tips, FMath::FRand());
			const FVector Throw = Forward * FMath::FRandRange(FlingSpeedMin, FlingSpeedMax) + FVector(0.0f, 0.0f, FMath::FRandRange(FlingLiftMin, FlingLiftMax)) + FMath::VRand() * 80.0f;
			Shed(From + FMath::VRand() * 3.0f, Throw, FMath::FRandRange(1.5f, 2.8f));
		}
	}
}

void UVaelHandWaterComponent::Finish()
{
	bFinished = true;
}

void UVaelHandWaterComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	if (Beads == nullptr)
	{
		return;
	}
	Age += DeltaTime;
	Frame.Reset();

	const AActor* Owner = GetOwner();
	const float GroundZ = Owner != nullptr ? Owner->GetActorLocation().Z - Owner->GetSimpleCollisionHalfHeight() : -UE_BIG_NUMBER;
	const bool bGathering = !bFinished && !bFlung && Age < Release + GatherLimit;
	const bool bDripping = !bFinished && (bGathering || (bFlung && Age - FlungAge < DripAfter));
	const float Full = FMath::SmoothStep(0.0f, Release, Age);

	for (int32 Side = 0; Side < (bBothHands ? 2 : 1); ++Side)
	{
		FVector Elbow, Wrist, Tips;
		if (!GetHand(Side, Elbow, Wrist, Tips))
		{
			continue;
		}
		const FVector Swing = bHasLastWrist && DeltaTime > 0.0f ? (Wrist - LastWrist[Side]) / DeltaTime : FVector::ZeroVector;
		LastWrist[Side] = Wrist;

		// The coil: beads spiral round the forearm out towards the fingers, more and bigger the closer the throw
		if (bGathering)
		{
			const FVector Axis = (Tips - Elbow).GetSafeNormal();
			const FVector Across = FVector::CrossProduct(Axis, FVector::UpVector).GetSafeNormal(UE_SMALL_NUMBER, FVector::ForwardVector);
			const FVector Over = FVector::CrossProduct(Across, Axis);
			for (int32 Index = 0; Index < CoilBeads; ++Index)
			{
				const float Along = FMath::Fractional(Index / static_cast<float>(CoilBeads) + Age * 0.9f);
				const float Angle = (Along * 2.5f + Age * CoilTurnRate) * UE_TWO_PI + Side * UE_PI;
				const FVector Place = FMath::Lerp(Elbow, Tips, 0.2f + 0.85f * Along) + (Across * FMath::Cos(Angle) + Over * FMath::Sin(Angle)) * CoilRadius;
				// Each bead swells in from the elbow and is biggest at the hand
				const float Size = CoilBeadSize * Full * FMath::Sin(UE_PI * FMath::Clamp(Along * 1.1f, 0.0f, 1.0f)) / BeadSphereSize;
				Frame.Add(FTransform(FQuat::Identity, Place, FVector(Size)));
			}
		}

		// Drops run off the fingertips, flung along with the swing of the hand
		if (bDripping)
		{
			DripCredit[Side] += (bGathering ? GatherDrips * FMath::Max(Full, 0.3f) : AfterDrips) * DeltaTime;
			for (; DripCredit[Side] >= 1.0f; DripCredit[Side] -= 1.0f)
			{
				Shed(Tips + FMath::VRand() * 3.0f, Swing * 0.5f + FMath::VRand() * 30.0f, FMath::FRandRange(1.8f, 3.2f));
			}
		}
	}
	bHasLastWrist = true;

	// The drops fall, drawn out into threads by their speed, until they reach the ground
	for (int32 Index = Drops.Num() - 1; Index >= 0; --Index)
	{
		FDrop& Drop = Drops[Index];
		Drop.Age += DeltaTime;
		Drop.Velocity.Z -= DropGravity * DeltaTime;
		Drop.Location += Drop.Velocity * DeltaTime;
		if (Drop.Age > DropLife || Drop.Location.Z < GroundZ)
		{
			Drops.RemoveAtSwap(Index);
			continue;
		}
		const float Speed = Drop.Velocity.Size();
		const float Width = Drop.Size / BeadSphereSize;
		const FQuat Way = Speed > 1.0f ? FRotationMatrix::MakeFromZ(Drop.Velocity / Speed).ToQuat() : FQuat::Identity;
		Frame.Add(FTransform(Way, Drop.Location, FVector(Width, Width, Width * (1.0f + Speed / DropStretchSpeed))));
	}

	// All gone: the component goes too
	const bool bDone = Drops.IsEmpty() && !bDripping && !bGathering;
	if (bDone)
	{
		Beads->DestroyComponent();
		DestroyComponent();
		return;
	}

	const int32 Shown = Frame.Num();
	for (int32 Index = Beads->GetInstanceCount(); Index < Shown; ++Index)
	{
		Beads->AddInstance(Frame[Index], true);
	}
	for (int32 Index = Shown; Index < Beads->GetInstanceCount(); ++Index)
	{
		Frame.Add(FTransform(FQuat::Identity, GetComponentLocation(), FVector::ZeroVector));
	}
	if (Frame.Num() > 0)
	{
		Beads->BatchUpdateInstancesTransforms(0, Frame, true, true, true);
	}
}
