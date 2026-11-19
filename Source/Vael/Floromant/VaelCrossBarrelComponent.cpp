// Copyright Epic Games, Inc. All Rights Reserved.

#include "Floromant/VaelCrossBarrelComponent.h"
#include "Components/SplineMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Floromant/VaelFloromantSettings.h"
#include "Floromant/VaelPlantArmData.h"
#include "Floromant/VaelSymbiosis.h"
#include "GameFramework/Actor.h"
#include "Magic/VaelSpellEffects.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "VaelAssets.h"

namespace
{
	/** Strands braided into the barrel (two out of each wrist), the pieces each is laid in, thorns, crown pieces, light rings */
	constexpr int32 BarrelStrands = 4;
	constexpr int32 BarrelSegments = 6;

	/** Share of each strand that winds down the forearm, before the braid */
	constexpr float ForearmShare = 0.3f;
	constexpr int32 BarrelThorns = 8;
	constexpr int32 CrownPerPlant = 4;
	constexpr int32 BarrelRings = 3;

	/** Length of the grown barrel in cm, how many turns the braid makes along it */
	constexpr float BarrelLength = 75.0f;
	constexpr float BarrelTurns = 1.5f;

	/** Seconds of the kick back of a shot and of the flare of the crown */
	constexpr float KickTime = 0.25f;
	constexpr float FlareTime = 0.5f;

	/** Grows past its end a little and settles, so the barrel shoots out with a snap */
	float Overshoot(float T)
	{
		const float C = 1.70158f;
		const float X = FMath::Clamp(T, 0.0f, 1.0f) - 1.0f;
		return 1.0f + (C + 1.0f) * X * X * X + C * X * X;
	}

	USplineMeshComponent* MakePiece(AActor* Owner, USceneComponent* Parent, UStaticMesh* Mesh, UMaterialInterface* Material)
	{
		USplineMeshComponent* Piece = NewObject<USplineMeshComponent>(Owner);
		Piece->SetMobility(EComponentMobility::Movable);
		Piece->SetForwardAxis(ESplineMeshAxis::Z, false);
		Piece->SetStaticMesh(Mesh);
		Piece->SetMaterial(0, Material);
		Piece->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Piece->SetCanEverAffectNavigation(false);
		Piece->SetupAttachment(Parent);
		Piece->SetUsingAbsoluteLocation(true);
		Piece->SetUsingAbsoluteRotation(true);
		Piece->SetUsingAbsoluteScale(true);
		Piece->RegisterComponent();
		Piece->SetWorldLocationAndRotation(FVector::ZeroVector, FRotator::ZeroRotator);
		Piece->SetWorldScale3D(FVector::OneVector);
		Piece->SetVisibility(false);
		return Piece;
	}

	void LayPiece(USplineMeshComponent* Piece, const FVector& A, const FVector& TA, const FVector& B, const FVector& TB, float S0, float S1, float Roll)
	{
		Piece->SetVisibility(true);
		Piece->SetStartAndEnd(A, TA, B, TB, false);
		Piece->SetStartScale(FVector2D(FMath::Max(S0, 0.001f)), false);
		Piece->SetEndScale(FVector2D(FMath::Max(S1, 0.001f)), false);
		Piece->SetStartRoll(Roll, false);
		Piece->SetEndRoll(Roll + 1.5f, false);
		Piece->UpdateMesh();
	}
}

void UVaelCrossBarrelComponent::Begin(const UVaelPlantArmData* A, const UVaelPlantArmData* B, const FLinearColor& PairColor, bool bShadow)
{
	Clear();
	AActor* Owner = GetOwner();
	const UVaelFloromantSettings* Settings = UVaelFloromantSettings::Get();
	if (Owner == nullptr || A == nullptr || B == nullptr)
	{
		return;
	}
	Color = PairColor;
	Clock = 0.0f;
	KickLeft = 0.0f;
	FlareLeft = 0.0f;

	// The root wood, its veins in the colours of both plants
	if (UMaterialInterface* WoodBase = VaelAssets::LoadOptional(Settings->DragonWoodMaterial))
	{
		Wood = UMaterialInstanceDynamic::Create(WoodBase, this);
		Wood->SetScalarParameterValue(TEXT("Schatten"), bShadow ? 1.0f : 0.0f);
		Wood->SetVectorParameterValue(TEXT("Leuchtfarbe"), VaelSymbiosis::GetPlantColor(A, bShadow));
		Wood->SetVectorParameterValue(TEXT("Akzentfarbe"), VaelSymbiosis::GetPlantColor(B, bShadow));
		Wood->SetScalarParameterValue(TEXT("Akzentanteil"), 0.5f);
	}
	for (int32 Strand = 0; Strand < BarrelStrands; ++Strand)
	{
		UStaticMesh* Mesh = Settings->DragonStrandMeshes.IsEmpty() ? nullptr : VaelAssets::LoadOptional(Settings->DragonStrandMeshes[Strand % Settings->DragonStrandMeshes.Num()]);
		for (int32 Segment = 0; Segment < BarrelSegments; ++Segment)
		{
			Strands.Add(MakePiece(Owner, this, Mesh, Wood));
		}
	}
	UStaticMesh* ThornMesh = VaelAssets::LoadOptional(Settings->DragonFibreMesh);
	for (int32 Index = 0; Index < BarrelThorns && ThornMesh != nullptr; ++Index)
	{
		Thorns.Add(MakePiece(Owner, this, ThornMesh, Wood));
	}

	// The crown: pieces of both plants in turn, whatever each grows at its tips (heads, else petals, leaves or blades)
	for (int32 Index = 0; Index < 2 * CrownPerPlant; ++Index)
	{
		const UVaelPlantArmData* Plant = Index % 2 == 0 ? A : B;
		UStaticMesh* Mesh = VaelAssets::LoadOptional(Plant->TipMesh);
		float Size = Plant->TipSize * 1.6f / 10.0f;
		UMaterialInterface* Material = VaelAssets::LoadOptional(Plant->Material);
		if (Mesh == nullptr)
		{
			Mesh = VaelAssets::LoadOptional(Plant->PetalMesh);
			Size = Plant->PetalSize * 0.6f / 10.0f;
			if (UMaterialInterface* Part = VaelAssets::LoadOptional(Plant->PartMaterial))
			{
				Material = Part;
			}
		}
		if (Mesh == nullptr)
		{
			Mesh = VaelAssets::LoadOptional(Plant->LeafMesh);
			Size = Plant->LeafSize * 0.6f / 10.0f;
		}
		if (Mesh == nullptr && !Plant->FingerMeshes.IsEmpty())
		{
			// Blades: a stalk of a metre, shown a fifth as long
			Mesh = VaelAssets::LoadOptional(Plant->FingerMeshes[Index % Plant->FingerMeshes.Num()]);
			Size = 0.22f;
		}
		if (Mesh == nullptr)
		{
			continue;
		}
		UStaticMeshComponent* Piece = NewObject<UStaticMeshComponent>(Owner);
		Piece->SetMobility(EComponentMobility::Movable);
		Piece->SetStaticMesh(Mesh);
		Piece->SetMaterial(0, Material);
		Piece->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Piece->SetCanEverAffectNavigation(false);
		Piece->SetupAttachment(this);
		Piece->SetUsingAbsoluteLocation(true);
		Piece->SetUsingAbsoluteRotation(true);
		Piece->SetUsingAbsoluteScale(true);
		Piece->RegisterComponent();
		Piece->SetVisibility(false);
		Crown.Add(Piece);
		CrownSizes.Add(FMath::Max(Size, 0.05f));
	}

	// The heart in the crown and the rings of light running down the barrel
	UMaterialInterface* Core = nullptr;
	UMaterialInterface* GlowMaterial = nullptr;
	VaelEffects::LoadLookMaterials(Core, GlowMaterial);
	if (GlowMaterial != nullptr)
	{
		Heart = VaelEffects::AddLookShape(Owner, GlowMaterial, Color, 0.0f);
		Heart->SetUsingAbsoluteLocation(true);
		Heart->SetUsingAbsoluteRotation(true);
		Heart->SetUsingAbsoluteScale(true);
		UStaticMesh* Cylinder = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
		for (int32 Index = 0; Index < BarrelRings && Cylinder != nullptr; ++Index)
		{
			UStaticMeshComponent* Ring = VaelEffects::AddLookShape(Owner, GlowMaterial, Color, 0.0f);
			Ring->SetStaticMesh(Cylinder);
			Ring->SetUsingAbsoluteLocation(true);
			Ring->SetUsingAbsoluteRotation(true);
			Ring->SetUsingAbsoluteScale(true);
			Rings.Add(Ring);
		}
	}
}

void UVaelCrossBarrelComponent::Update(const FVector& LeftElbow, const FVector& LeftHand, const FVector& RightElbow, const FVector& RightHand, const FVector& Aim, float Grow, float Charge, float DeltaSeconds)
{
	Clock += DeltaSeconds;
	KickLeft = FMath::Max(0.0f, KickLeft - DeltaSeconds);
	FlareLeft = FMath::Max(0.0f, FlareLeft - DeltaSeconds / FlareTime);
	const bool bShown = Grow > 0.01f;

	const FVector Axis = Aim.GetSafeNormal2D().IsNearlyZero() ? FVector::ForwardVector : Aim.GetSafeNormal2D();
	const FVector U = FVector::UpVector;
	const FVector V = FVector::CrossProduct(Axis, U);
	// The shot kicks the barrel back; charged, it trembles
	const float Kick = FMath::Sin(UE_PI * FMath::Clamp(1.0f - KickLeft / KickTime, 0.0f, 1.0f)) * (KickLeft > 0.0f ? 1.0f : 0.0f);
	const FVector Tremble = (U * FMath::Sin(Clock * 47.0f) + V * FMath::Cos(Clock * 53.0f)) * 0.7f * Charge;
	const FVector Base = (LeftHand + RightHand) * 0.5f - Axis * 12.0f * Kick + Tremble;
	const float Length = BarrelLength * Overshoot(Grow);
	const float Thick = FMath::SmoothStep(0.0f, 0.4f, Grow);
	const float Spin = Clock * (0.4f + 3.0f * Charge);

	// Each strand winds from its elbow down the forearm round the arm (the arm itself turns to wood), then two of each arm are
	// drawn in to the middle and braided round each other along the aim
	auto Braid = [&](int32 Strand, float Along)
	{
		const FVector Elbow = Strand < 2 ? LeftElbow : RightElbow;
		const FVector Hand = Strand < 2 ? LeftHand : RightHand;
		if (Along <= ForearmShare)
		{
			const float T = Along / ForearmShare;
			const FVector Forearm = (Hand - Elbow).GetSafeNormal();
			FVector Side = FVector::CrossProduct(Forearm, FVector::UpVector).GetSafeNormal();
			if (Side.IsNearlyZero())
			{
				Side = V;
			}
			const FVector Over = FVector::CrossProduct(Side, Forearm);
			const float Angle = (Strand % 2) * UE_PI + T * UE_PI * 1.2f + (Strand < 2 ? 0.0f : 0.8f);
			return FMath::Lerp(Elbow, Hand, T) + (Side * FMath::Cos(Angle) + Over * FMath::Sin(Angle)) * 4.0f * FMath::Min(1.0f, Grow * 3.0f);
		}
		const float S = (Along - ForearmShare) / (1.0f - ForearmShare);
		const float Angle = Strand * UE_HALF_PI + S * BarrelTurns * 2.0f * UE_PI + Spin;
		const float Radius = (6.0f - 2.5f * S) * FMath::Min(1.0f, Grow * 1.5f);
		const FVector Around = U * FMath::Cos(Angle) + V * FMath::Sin(Angle);
		const FVector OnBarrel = Base + Axis * Length * S + Around * Radius;
		const FVector AtWrist = Hand + Around * 3.0f;
		return FMath::Lerp(AtWrist, OnBarrel, FMath::SmoothStep(0.0f, 0.3f, S));
	};
	for (int32 Strand = 0; Strand < BarrelStrands; ++Strand)
	{
		FVector Points[BarrelSegments + 1];
		for (int32 Index = 0; Index <= BarrelSegments; ++Index)
		{
			Points[Index] = Braid(Strand, Index / float(BarrelSegments));
		}
		for (int32 Segment = 0; Segment < BarrelSegments; ++Segment)
		{
			USplineMeshComponent* Piece = Strands.IsValidIndex(Strand * BarrelSegments + Segment) ? Strands[Strand * BarrelSegments + Segment].Get() : nullptr;
			if (Piece == nullptr)
			{
				continue;
			}
			if (!bShown)
			{
				Piece->SetVisibility(false);
				continue;
			}
			const FVector T0 = (Points[FMath::Min(Segment + 1, BarrelSegments)] - Points[FMath::Max(Segment - 1, 0)]) * (Segment == 0 ? 1.0f : 0.5f);
			const FVector T1 = (Points[FMath::Min(Segment + 2, BarrelSegments)] - Points[Segment]) * (Segment + 1 == BarrelSegments ? 1.0f : 0.5f);
			// Round the forearm the strands are already full, so the arm is wood at once; the braid thins towards the crown
			auto Girth = [Thick](float Along) { return (Along <= ForearmShare ? 0.5f : FMath::Lerp(0.55f, 0.3f, (Along - ForearmShare) / (1.0f - ForearmShare))) * Thick; };
			const float S0 = Girth(Segment / float(BarrelSegments));
			const float S1 = Girth((Segment + 1) / float(BarrelSegments));
			LayPiece(Piece, Points[Segment], T0, Points[Segment + 1], T1, S0, S1, Strand * 1.7f + Segment * 1.5f);
		}
	}

	// Thorns standing off the barrel, leaning back
	for (int32 Index = 0; Index < Thorns.Num(); ++Index)
	{
		USplineMeshComponent* Thorn = Thorns[Index];
		if (!bShown || Grow < 0.3f)
		{
			Thorn->SetVisibility(false);
			continue;
		}
		const float S = FMath::Lerp(0.3f, 0.85f, Index / float(FMath::Max(Thorns.Num() - 1, 1)));
		const float Angle = Index * 2.3f + Spin * 0.5f;
		const FVector Out = U * FMath::Cos(Angle) + V * FMath::Sin(Angle);
		const FVector Foot = Base + Axis * Length * S + Out * 5.0f;
		const float ThornLength = 9.0f * FMath::SmoothStep(0.3f, 0.8f, Grow) * (Index % 2 == 0 ? 1.0f : 0.7f);
		const FVector Way = (Out * 0.8f - Axis * 0.45f).GetSafeNormal();
		LayPiece(Thorn, Foot, Way * ThornLength, Foot + Way * ThornLength, (Way - Axis * 0.3f).GetSafeNormal() * ThornLength, 2.4f, 0.1f, Angle);
	}

	// The crown at the front: both plants in turn round the mouth, opening as it grows, flaring wide as the shot leaves
	const FVector Front = Base + Axis * Length;
	const float Bloom = FMath::SmoothStep(0.55f, 1.0f, Grow);
	const float Open = FMath::Lerp(0.35f, 0.95f, FlareLeft) + 0.08f * FMath::Sin(Clock * 3.0f) - 0.15f * Charge;
	for (int32 Index = 0; Index < Crown.Num(); ++Index)
	{
		UStaticMeshComponent* Piece = Crown[Index];
		Piece->SetVisibility(bShown && Bloom > 0.01f);
		if (!Piece->IsVisible())
		{
			continue;
		}
		const float Angle = Index * 2.0f * UE_PI / Crown.Num() + Clock * 0.6f;
		const FVector Out = U * FMath::Cos(Angle) + V * FMath::Sin(Angle);
		const FVector Up = (Axis * FMath::Cos(Open) + Out * FMath::Sin(Open)).GetSafeNormal();
		Piece->SetWorldLocationAndRotation(Front + Out * 5.0f, FRotationMatrix::MakeFromZX(Up, Out).Rotator());
		Piece->SetWorldScale3D(FVector(CrownSizes[Index] * Bloom));
	}

	// The heart swells and burns brighter the longer it charges
	if (Heart != nullptr)
	{
		Heart->SetVisibility(bShown && Bloom > 0.01f);
		Heart->SetWorldLocation(Front + Axis * 3.0f);
		Heart->SetWorldScale3D(FVector(FMath::Max((0.05f + 0.09f * Charge + 0.08f * FlareLeft) * Bloom, 0.001f)));
		VaelEffects::SetLookGlow(Heart, 3.0f + 8.0f * Charge + 10.0f * FlareLeft);
	}

	// Rings of light run down the barrel into the heart, faster the more it is charged
	for (int32 Index = 0; Index < Rings.Num(); ++Index)
	{
		UStaticMeshComponent* Ring = Rings[Index];
		Ring->SetVisibility(bShown && Grow > 0.5f);
		if (!Ring->IsVisible())
		{
			continue;
		}
		const float S = FMath::Frac(Clock * (0.8f + 2.2f * Charge) + Index / float(Rings.Num()));
		const float Radius = (6.0f - 2.5f * S) + 3.0f;
		Ring->SetWorldLocationAndRotation(Base + Axis * Length * S, FRotationMatrix::MakeFromZ(Axis).Rotator());
		Ring->SetWorldScale3D(FVector(Radius * 2.0f / 100.0f, Radius * 2.0f / 100.0f, 0.012f));
		VaelEffects::SetLookGlow(Ring, (0.25f + 1.2f * Charge) * FMath::Sin(UE_PI * S));
	}

	if (Wood != nullptr)
	{
		Wood->SetScalarParameterValue(TEXT("Leuchten"), 0.6f + 3.5f * Charge + 4.0f * FlareLeft);
	}
	Muzzle = Front + Axis * 12.0f;
}

void UVaelCrossBarrelComponent::Fire()
{
	KickLeft = KickTime;
	FlareLeft = 1.0f;
}

void UVaelCrossBarrelComponent::Clear()
{
	for (USplineMeshComponent* Piece : Strands)
	{
		if (Piece != nullptr)
		{
			Piece->DestroyComponent();
		}
	}
	for (USplineMeshComponent* Piece : Thorns)
	{
		if (Piece != nullptr)
		{
			Piece->DestroyComponent();
		}
	}
	for (UStaticMeshComponent* Piece : Crown)
	{
		if (Piece != nullptr)
		{
			Piece->DestroyComponent();
		}
	}
	for (UStaticMeshComponent* Piece : Rings)
	{
		if (Piece != nullptr)
		{
			Piece->DestroyComponent();
		}
	}
	if (Heart != nullptr)
	{
		Heart->DestroyComponent();
	}
	Strands.Reset();
	Thorns.Reset();
	Crown.Reset();
	CrownSizes.Reset();
	Rings.Reset();
	Heart = nullptr;
	Wood = nullptr;
}

void UVaelCrossBarrelComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	Clear();
	Super::EndPlay(EndPlayReason);
}
