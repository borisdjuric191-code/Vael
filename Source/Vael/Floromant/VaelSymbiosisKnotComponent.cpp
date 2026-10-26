// Copyright Epic Games, Inc. All Rights Reserved.

#include "Floromant/VaelSymbiosisKnotComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/SplineMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Floromant/VaelPlantArmData.h"
#include "GameFramework/Actor.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "VaelAssets.h"

namespace
{
	/** Vines from each plant to the knot, and pieces of each plant in the knot */
	constexpr int32 KnotVinesPerSide = 3;
	constexpr int32 KnotPiecesPerSide = 4;

	/** Brightness of the knot's light in candelas */
	constexpr float KnotLightIntensity = 18.0f;

	/** The vine mesh of a plant: its overgrowth vine, else a stalk of the arm or a finger */
	UStaticMesh* KnotVineMesh(const UVaelPlantArmData* Plant)
	{
		for (const TArray<TSoftObjectPtr<UStaticMesh>>* List : { &Plant->VineMeshes, &Plant->ArmMeshes, &Plant->FingerMeshes })
		{
			for (const TSoftObjectPtr<UStaticMesh>& Mesh : *List)
			{
				if (UStaticMesh* Loaded = VaelAssets::LoadOptional(Mesh))
				{
					return Loaded;
				}
			}
		}
		return nullptr;
	}

	/** What of a plant gets tangled in the knot, and its size in cm: a petal, a head, or a short stalk */
	UStaticMesh* KnotPieceMesh(const UVaelPlantArmData* Plant, float& OutSize)
	{
		if (UStaticMesh* Petal = VaelAssets::LoadOptional(Plant->PetalMesh))
		{
			OutSize = Plant->PetalSize * 0.8f;
			return Petal;
		}
		if (UStaticMesh* Head = VaelAssets::LoadOptional(Plant->TipMesh))
		{
			OutSize = Plant->TipSize * 1.3f;
			return Head;
		}
		// The Rußrose: a small rose like the one on the shoulder
		if (UStaticMesh* Rose = Plant->bVineArm ? VaelAssets::LoadOptional(Plant->ShoulderMesh) : nullptr)
		{
			OutSize = Plant->ShoulderRoseSize * 0.4f;
			return Rose;
		}
		for (const TSoftObjectPtr<UStaticMesh>& Mesh : Plant->FingerMeshes)
		{
			if (UStaticMesh* Loaded = VaelAssets::LoadOptional(Mesh))
			{
				// Pieces are scaled as if their mesh were 10 cm; stalks are 100 cm long, so a quarter of one is 2.5
				OutSize = 2.5f;
				return Loaded;
			}
		}
		return nullptr;
	}
}

void UVaelSymbiosisKnotComponent::Setup(const UVaelPlantArmData* A, const UVaelPlantArmData* B, const FLinearColor& Color, bool bShadow)
{
	Clear();
	if (A == nullptr || B == nullptr || GetOwner() == nullptr)
	{
		return;
	}

	SetUsingAbsoluteLocation(true);
	SetUsingAbsoluteRotation(true);
	SetUsingAbsoluteScale(true);
	SetWorldTransform(FTransform::Identity);
	GlowColor = Color;
	VinesPerSide = KnotVinesPerSide;

	FRandomStream Random(GetOwner()->GetUniqueID() + 7);
	for (const UVaelPlantArmData* Plant : { A, B })
	{
		UMaterialInstanceDynamic* Material = UMaterialInstanceDynamic::Create(VaelAssets::LoadOptional(Plant->Material), this);
		if (Material != nullptr)
		{
			Material->SetScalarParameterValue(TEXT("Wuchs"), 1.0f);
			Material->SetScalarParameterValue(TEXT("Wind"), 0.15f);
			Material->SetScalarParameterValue(TEXT("Schatten"), bShadow ? 1.0f : 0.0f);
		}
		Materials.Add(Material);

		UStaticMesh* VineMesh = KnotVineMesh(Plant);
		for (int32 Index = 0; Index < KnotVinesPerSide; ++Index)
		{
			USplineMeshComponent* Vine = NewObject<USplineMeshComponent>(GetOwner());
			Vine->SetMobility(EComponentMobility::Movable);
			Vine->SetForwardAxis(ESplineMeshAxis::Z, false);
			Vine->SetStaticMesh(VineMesh);
			Vine->SetMaterial(0, Material);
			Vine->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			Vine->SetCanEverAffectNavigation(false);
			Vine->SetupAttachment(this);
			Vine->RegisterComponent();
			Vines.Add(Vine);
		}

		float Size = 10.0f;
		UStaticMesh* PieceMesh = KnotPieceMesh(Plant, Size);
		for (int32 Index = 0; Index < KnotPiecesPerSide && PieceMesh != nullptr; ++Index)
		{
			UStaticMeshComponent* Piece = NewObject<UStaticMeshComponent>(GetOwner());
			Piece->SetMobility(EComponentMobility::Movable);
			Piece->SetStaticMesh(PieceMesh);
			Piece->SetMaterial(0, Material);
			Piece->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			Piece->SetCanEverAffectNavigation(false);
			Piece->SetupAttachment(this);
			Piece->RegisterComponent();
			Pieces.Add(Piece);
			PieceShapes.Add(FVector4(Random.VRand(), Size * Random.FRandRange(0.8f, 1.15f)));
		}
	}

	Glow = NewObject<UPointLightComponent>(GetOwner());
	Glow->SetupAttachment(this);
	Glow->SetMobility(EComponentMobility::Movable);
	Glow->SetIntensityUnits(ELightUnits::Candelas);
	Glow->SetIntensity(0.0f);
	Glow->SetAttenuationRadius(320.0f);
	Glow->SetCastShadows(false);
	Glow->SetLightColor(Color);
	Glow->RegisterComponent();
}

void UVaelSymbiosisKnotComponent::Update(const FVector& StartA, const FVector& StartB, const FVector& Knot, float Grow)
{
	if (Vines.IsEmpty())
	{
		return;
	}

	const float Time = GetWorld()->GetTimeSeconds();
	const float Delta = GetWorld()->GetDeltaSeconds();
	FlashLeft = FMath::Max(0.0f, FlashLeft - Delta * 2.5f);
	Grow = FMath::Clamp(Grow, 0.0f, 1.0f);

	// The vines of each side twist round each other on the way to the knot, each a little apart
	for (int32 Index = 0; Index < Vines.Num(); ++Index)
	{
		const bool bSideA = Index < VinesPerSide;
		const FVector Start = bSideA ? StartA : StartB;
		const FVector Way = Knot - Start;
		const float Length = Way.Size();
		if (Length < 1.0f)
		{
			Vines[Index]->SetVisibility(false);
			continue;
		}
		Vines[Index]->SetVisibility(Grow > 0.01f);

		const FVector Along = Way / Length;
		FVector Side = FVector::CrossProduct(Along, FVector::UpVector);
		Side = Side.IsNearlyZero() ? FVector::RightVector : Side.GetSafeNormal();
		const FVector Lift = FVector::CrossProduct(Side, Along);
		const float Angle = (Index % VinesPerSide) * 2.0f * UE_PI / VinesPerSide + (bSideA ? 0.0f : 0.9f);
		const FVector Offset = Side * FMath::Cos(Angle) + Lift * FMath::Sin(Angle);
		const FVector Twist = Side * FMath::Cos(Angle + UE_PI) + Lift * FMath::Sin(Angle + UE_PI);

		// The front grows from the plant to the knot; the sag lifts the vine in an arc like a climbing plant
		const FVector From = Start + Offset * 5.0f;
		const FVector To = FMath::Lerp(From, Knot + Twist * 3.0f, Grow);
		const float Span = FVector::Dist(From, To);
		Vines[Index]->SetStartAndEnd(From, (Along + FVector::UpVector * 0.5f + Offset * 0.3f).GetSafeNormal() * Span,
			To, (Along - FVector::UpVector * 0.25f + Twist * 0.3f).GetSafeNormal() * Span, false);
		Vines[Index]->SetStartScale(FVector2D(0.9f), false);
		Vines[Index]->SetEndScale(FVector2D(0.5f), false);
		Vines[Index]->SetStartRoll(Angle, false);
		Vines[Index]->SetEndRoll(Angle + 2.5f, false);
		Vines[Index]->UpdateMesh();
	}

	// The pieces of both plants tangle in the knot once the vines have arrived, turning slowly
	const float Tangle = FMath::Clamp((Grow - 0.6f) / 0.4f, 0.0f, 1.0f);
	for (int32 Index = 0; Index < Pieces.Num(); ++Index)
	{
		const FVector4& Shape = PieceShapes[Index];
		const FVector Out = FVector(Shape.X, Shape.Y, Shape.Z).GetSafeNormal();
		const FRotator Spin(0.0f, Time * 25.0f * (Index % 2 == 0 ? 1.0f : -1.0f), 0.0f);
		const FVector Direction = Spin.RotateVector(Out);
		Pieces[Index]->SetVisibility(Tangle > 0.01f);
		Pieces[Index]->SetWorldLocationAndRotation(Knot + Direction * 4.0f * Tangle, FRotationMatrix::MakeFromZ(Direction).Rotator());
		Pieces[Index]->SetWorldScale3D(FVector(Shape.W / 10.0f * Tangle * (1.0f + 0.35f * FlashLeft + 0.3f * Charge)));
	}

	if (Glow != nullptr)
	{
		Glow->SetWorldLocation(Knot);
		Glow->SetIntensity(KnotLightIntensity * Tangle * (1.0f + 1.5f * FlashLeft + 2.0f * Charge + 0.15f * FMath::Sin(Time * 3.0f)));
	}
}

void UVaelSymbiosisKnotComponent::Clear()
{
	for (USplineMeshComponent* Vine : Vines)
	{
		if (Vine != nullptr)
		{
			Vine->DestroyComponent();
		}
	}
	for (UStaticMeshComponent* Piece : Pieces)
	{
		if (Piece != nullptr)
		{
			Piece->DestroyComponent();
		}
	}
	if (Glow != nullptr)
	{
		Glow->DestroyComponent();
		Glow = nullptr;
	}
	Vines.Reset();
	Pieces.Reset();
	PieceShapes.Reset();
	Materials.Reset();
}

void UVaelSymbiosisKnotComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	Clear();
	Super::EndPlay(EndPlayReason);
}
