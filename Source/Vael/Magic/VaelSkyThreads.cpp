// Copyright Epic Games, Inc. All Rights Reserved.

#include "Magic/VaelSkyThreads.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Magic/VaelMagicSettings.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Player/VaelCharacter.h"
#include "VaelAssets.h"

namespace
{
	/** Three threads: from the mouth and from both hands */
	constexpr int32 ThreadStrands = 3;

	/** Motes per second on each thread, seconds each takes up to the sky, how wide the threads braid round each other in cm, how often */
	constexpr float ThreadRate = 60.0f;
	constexpr float ThreadFlight = 0.7f;
	constexpr float ThreadBraid = 16.0f;
	constexpr float ThreadTurns = 2.0f;

	/** Width of a mote in cm, its glow; the threads start a moment after the arms began to rise */
	constexpr float MoteWidth = 5.5f;
	constexpr float MoteGlow = 5.0f;
	constexpr float ThreadStart = 0.12f;

	/** How high the threads first rise straight up before they bend over to the sky point */
	constexpr float ThreadRise = 220.0f;
}

UVaelSkyThreadsComponent::UVaelSkyThreadsComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	SetUsingAbsoluteLocation(true);
	SetUsingAbsoluteRotation(true);
	SetUsingAbsoluteScale(true);
}

void UVaelSkyThreadsComponent::Start(const FVector& InSkyPoint, const FLinearColor& Color, float InRelease)
{
	SkyPoint = InSkyPoint;
	Release = FMath::Max(InRelease, 0.05f);

	Motes = NewObject<UInstancedStaticMeshComponent>(GetOwner());
	Motes->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Sphere.Sphere")));
	Motes->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Motes->SetCastShadow(false);
	Motes->bReceivesDecals = false;
	Motes->SetUsingAbsoluteLocation(true);
	Motes->SetUsingAbsoluteRotation(true);
	Motes->SetUsingAbsoluteScale(true);
	Motes->SetupAttachment(this);
	Motes->SetWorldTransform(FTransform::Identity);
	Motes->RegisterComponent();
	if (UMaterialInterface* IceMaterial = VaelAssets::LoadOptional(UVaelMagicSettings::Get()->HailIceMaterial))
	{
		UMaterialInstanceDynamic* Light = UMaterialInstanceDynamic::Create(IceMaterial, this);
		Light->SetVectorParameterValue(TEXT("Farbe"), Color);
		Light->SetScalarParameterValue(TEXT("Leuchten"), MoteGlow);
		Motes->SetMaterial(0, Light);
	}
}

FVector UVaelSkyThreadsComponent::GetSource(int32 Strand) const
{
	const AVaelCharacter* Mage = Cast<AVaelCharacter>(GetOwner());
	if (Mage == nullptr)
	{
		return GetComponentLocation();
	}
	if (Strand == 0)
	{
		return Mage->GetMouthLocation();
	}
	const USkeletalMeshComponent* Body = Mage->GetMesh();
	const FName Hand = Strand == 1 ? TEXT("hand_l") : TEXT("hand_r");
	return Body->GetBoneIndex(Hand) != INDEX_NONE ? Body->GetBoneLocation(Hand) : Mage->GetMouthLocation();
}

void UVaelSkyThreadsComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	if (Motes == nullptr)
	{
		return;
	}
	Age += DeltaTime;

	// New motes leave mouth and hands while the mage murmurs
	if (Age >= ThreadStart && Age < Release)
	{
		Credit += ThreadRate * DeltaTime;
		for (; Credit >= 1.0f; Credit -= 1.0f)
		{
			for (int32 Strand = 0; Strand < ThreadStrands; ++Strand)
			{
				FMote& Mote = Flying.AddDefaulted_GetRef();
				Mote.From = GetSource(Strand);
				Mote.Strand = Strand;
			}
		}
	}

	// Each rises on a bent way up to the sky point, the three threads braiding round the way
	Frame.Reset();
	for (int32 Index = Flying.Num() - 1; Index >= 0; --Index)
	{
		FMote& Mote = Flying[Index];
		Mote.Age += DeltaTime;
		const float Along = Mote.Age / ThreadFlight;
		if (Along >= 1.0f)
		{
			Flying.RemoveAtSwap(Index);
			continue;
		}
		const FVector Bend = Mote.From + FVector(0.0f, 0.0f, ThreadRise);
		const FVector Way = FMath::Lerp(FMath::Lerp(Mote.From, Bend, Along), FMath::Lerp(Bend, SkyPoint, Along), Along);
		const FVector Heading = (FMath::Lerp(Bend, SkyPoint, Along) - FMath::Lerp(Mote.From, Bend, Along)).GetSafeNormal(UE_SMALL_NUMBER, FVector::UpVector);
		const FVector Across = FVector::CrossProduct(Heading, FVector::ForwardVector).GetSafeNormal(UE_SMALL_NUMBER, FVector::RightVector);
		const FVector Over = FVector::CrossProduct(Across, Heading);
		const float Angle = Along * ThreadTurns * UE_TWO_PI + Mote.Strand * UE_TWO_PI / ThreadStrands;
		const FVector Place = Way + (Across * FMath::Cos(Angle) + Over * FMath::Sin(Angle)) * ThreadBraid * FMath::Sin(UE_PI * Along);
		const float Size = MoteWidth * FMath::Min(1.0f, (1.0f - Along) * 5.0f) / 100.0f;
		Frame.Add(FTransform(FQuat::Identity, Place, FVector(Size)));
	}

	if (Flying.IsEmpty() && Age >= Release)
	{
		Motes->DestroyComponent();
		DestroyComponent();
		return;
	}

	const int32 Shown = Frame.Num();
	for (int32 Index = Motes->GetInstanceCount(); Index < Shown; ++Index)
	{
		Motes->AddInstance(Frame[Index], true);
	}
	for (int32 Index = Shown; Index < Motes->GetInstanceCount(); ++Index)
	{
		Frame.Add(FTransform(FQuat::Identity, GetComponentLocation(), FVector::ZeroVector));
	}
	if (Frame.Num() > 0)
	{
		Motes->BatchUpdateInstancesTransforms(0, Frame, true, true, true);
	}
}
