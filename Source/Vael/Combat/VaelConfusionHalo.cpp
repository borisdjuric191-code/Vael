// Copyright Epic Games, Inc. All Rights Reserved.

#include "Combat/VaelConfusionHalo.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInterface.h"

namespace
{
	constexpr int32 SpiralCount = 3;

	/** Made by Scripts/BuildStatusVerwirrung.py */
	const TCHAR* SpiralMeshPath = TEXT("/Game/Vael/Effects/Status/SM_Status_Spirale.SM_Status_Spirale");
	const TCHAR* SpiralMaterialPath = TEXT("/Game/Vael/Effects/Status/M_Status_Verwirrung.M_Status_Verwirrung");
}

UVaelConfusionHalo::UVaelConfusionHalo()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = true;
	SetCollisionEnabled(ECollisionEnabled::NoCollision);
	SetGenerateOverlapEvents(false);
	SetCastShadow(false);
	TranslucencySortPriority = 10;
	NumCustomDataFloats = 1;
}

void UVaelConfusionHalo::OnRegister()
{
	Super::OnRegister();

	if (GetStaticMesh() == nullptr)
	{
		SetStaticMesh(LoadObject<UStaticMesh>(nullptr, SpiralMeshPath));
		if (UMaterialInterface* Material = LoadObject<UMaterialInterface>(nullptr, SpiralMaterialPath))
		{
			SetMaterial(0, Material);
		}
	}
	if (GetInstanceCount() == 0)
	{
		for (int32 Index = 0; Index < SpiralCount; ++Index)
		{
			const int32 Instance = AddInstance(FTransform::Identity);
			SetCustomDataValue(Instance, 0, static_cast<float>(Index) / SpiralCount, false);
		}
	}
	Time = FMath::FRandRange(0.0f, 10.0f);
}

void UVaelConfusionHalo::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	Time += DeltaTime;
	TArray<FTransform> Transforms;
	Transforms.Reserve(SpiralCount);
	for (int32 Index = 0; Index < SpiralCount; ++Index)
	{
		// Around the head on a wobbling circle, each spiral bobbing and spinning its own way
		const float Angle = UE_TWO_PI * (Time * OrbitSpeed + static_cast<float>(Index) / SpiralCount);
		const float Wobble = Radius * (1.0f + 0.25f * FMath::Sin(Time * 1.7f + Index * 2.1f));
		const FVector Location(FMath::Cos(Angle) * Wobble, FMath::Sin(Angle) * Wobble, Height + 6.0f * FMath::Sin(Time * 2.3f + Index * 2.0f));
		const float Spin = (Index % 2 == 0 ? 1.0f : -1.0f) * 360.0f * SpinSpeed * Time;
		const FRotator Tilt(18.0f * FMath::Sin(Time * 1.3f + Index), FMath::RadiansToDegrees(Angle), 0.0f);
		const FQuat Rotation = Tilt.Quaternion() * FQuat(FVector::UpVector, FMath::DegreesToRadians(Spin));
		const float Size = SpiralScale * (1.0f + 0.15f * FMath::Sin(Time * 3.1f + Index * 1.3f));
		Transforms.Add(FTransform(Rotation, Location, FVector(Size)));
	}
	BatchUpdateInstancesTransforms(0, Transforms, false, true, false);
}
