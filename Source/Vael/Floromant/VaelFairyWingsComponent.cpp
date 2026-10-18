// Copyright Epic Games, Inc. All Rights Reserved.

#include "Floromant/VaelFairyWingsComponent.h"
#include "Floromant/VaelFloromantSettings.h"
#include "GameFramework/Actor.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "ProceduralMeshComponent.h"
#include "VaelAssets.h"

namespace
{
	/** Grid of each wing sheet: points across and down */
	constexpr int32 WingColumns = 6;
	constexpr int32 WingRows = 8;

	/** Height of the root of the wing in the picture, as share from the top */
	constexpr float WingRootV = 0.43f;

	/** How far the tip of the wing curves back, in cm */
	constexpr float WingCurve = 10.0f;

	/** Degrees the wings stand back from the shoulders at rest, and how far each beat swings them */
	constexpr float WingRestAngle = 18.0f;
	constexpr float WingBeatAngle = 14.0f;

	/** Speed above which the character counts as walking, in cm/s */
	constexpr float WingWalkSpeed = 60.0f;

	/** Material parameters of M_Floromant_Fluegel */
	const FName WingGlowParameter(TEXT("Glow"));
	const FName WingGlowColorParameter(TEXT("GlowColor"));
}

UVaelFairyWingsComponent::UVaelFairyWingsComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;
}

void UVaelFairyWingsComponent::SetWingsShown(bool bInShown)
{
	bShown = bInShown;
	if (bShown && Sheets.IsEmpty())
	{
		BuildWings();
	}

	SetVisibility(bShown, true);
	SetComponentTickEnabled(bShown);
}

void UVaelFairyWingsComponent::SetGlow(const FLinearColor& Color, float Strength)
{
	if (WingMaterial != nullptr)
	{
		WingMaterial->SetVectorParameterValue(WingGlowColorParameter, Color);
		WingMaterial->SetScalarParameterValue(WingGlowParameter, Strength);
	}
}

void UVaelFairyWingsComponent::BuildWings()
{
	const UVaelFloromantSettings* Settings = UVaelFloromantSettings::Get();
	const FVector2D Size = Settings->WingSize;

	if (UMaterialInterface* Material = VaelAssets::LoadOptional(Settings->WingMaterial))
	{
		WingMaterial = UMaterialInstanceDynamic::Create(Material, this);
	}

	// One sheet in the space of its hinge: it reaches out along Y, up and down along Z, and curves back along -X towards the tip
	TArray<FVector> Vertices;
	TArray<FVector> Normals;
	TArray<FVector2D> UVs;
	TArray<int32> Triangles;

	for (int32 Row = 0; Row <= WingRows; ++Row)
	{
		const float V = Row / static_cast<float>(WingRows);
		for (int32 Column = 0; Column <= WingColumns; ++Column)
		{
			const float U = Column / static_cast<float>(WingColumns);
			Vertices.Add(FVector(-WingCurve * U * U, U * Size.X, (WingRootV - V) * Size.Y));
			Normals.Add(FVector(-1.0f, 0.0f, 0.0f));
			UVs.Add(FVector2D(U, V));
		}
	}

	for (int32 Row = 0; Row < WingRows; ++Row)
	{
		for (int32 Column = 0; Column < WingColumns; ++Column)
		{
			const int32 A = Row * (WingColumns + 1) + Column;
			const int32 B = A + 1;
			const int32 C = A + WingColumns + 1;
			const int32 D = C + 1;
			Triangles.Append({ A, B, C, B, D, C });
		}
	}

	// Left and right: the left wing is the right one mirrored
	for (int32 Side = 0; Side < 2; ++Side)
	{
		const float Mirror = Side == 0 ? -1.0f : 1.0f;

		USceneComponent* Hinge = NewObject<USceneComponent>(GetOwner());
		Hinge->SetupAttachment(this);
		Hinge->SetRelativeLocation(FVector(Settings->WingRoot.X, Settings->WingRoot.Y * Mirror, Settings->WingRoot.Z));
		Hinge->RegisterComponent();
		Hinges.Add(Hinge);

		UProceduralMeshComponent* Sheet = NewObject<UProceduralMeshComponent>(GetOwner());
		Sheet->SetupAttachment(Hinge);
		Sheet->SetRelativeScale3D(FVector(1.0f, Mirror, 1.0f));
		Sheet->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Sheet->SetCastShadow(true);
		Sheet->bReceivesDecals = false;
		Sheet->RegisterComponent();
		Sheet->CreateMeshSection_LinearColor(0, Vertices, Triangles, Normals, UVs, TArray<FLinearColor>(), TArray<FProcMeshTangent>(), false);
		if (WingMaterial != nullptr)
		{
			Sheet->SetMaterial(0, WingMaterial);
		}
		Sheets.Add(Sheet);
	}
}

void UVaelFairyWingsComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	const UVaelFloromantSettings* Settings = UVaelFloromantSettings::Get();
	const AActor* Owner = GetOwner();
	const bool bMoving = Owner != nullptr && Owner->GetVelocity().Size2D() > WingWalkSpeed;
	BeatPhase += DeltaTime * 2.0f * UE_PI * (bMoving ? Settings->WingBeatMoving : Settings->WingBeatIdle);

	// The wings open and fold back around their hinges; the beat is quick on the way back and lingers when open, like a moth
	const float Beat = FMath::Sin(BeatPhase);
	const float Swing = WingRestAngle + WingBeatAngle * (Beat > 0.0f ? Beat : Beat * 0.6f);
	const float Flutter = 3.0f * FMath::Sin(BeatPhase * 2.3f);

	for (int32 Side = 0; Side < Hinges.Num(); ++Side)
	{
		const float Mirror = Side == 0 ? -1.0f : 1.0f;
		Hinges[Side]->SetRelativeRotation(FRotator(0.0f, Swing * Mirror, Flutter * Mirror));
	}
}
