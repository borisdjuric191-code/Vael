// Copyright Epic Games, Inc. All Rights Reserved.

#include "World/VaelFireComponent.h"
#include "Components/DecalComponent.h"
#include "Components/PointLightComponent.h"
#include "Engine/World.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "VaelAssets.h"

int32 UVaelFireComponent::NumLitFires = 0;

namespace
{
	/** Material parameters of the fire kit */
	const FName StrengthParameter(TEXT("Staerke"));
	const FName HeatParameter(TEXT("Hitze"));
	const FName SmokeParameter(TEXT("Dunkel"));
	const FName SpeedParameter(TEXT("Tempo"));
	const FName SparkHeightParameter(TEXT("Hoehe"));
	const FName EmberParameter(TEXT("Glut"));
	const FName CoverParameter(TEXT("Deckkraft"));
	const FName FlowParameter(TEXT("Richtung"));
	const FName SizeParameter(TEXT("Groesse"));
	const FName NormalFlowParameter(TEXT("NormalFluss"));

	/** Size of a spark card in cm */
	constexpr float SparkSize = 1.8f;

	const FString FireFolder = TEXT("/Game/Vael/Effects/Feuer");

	template<typename T>
	TSoftObjectPtr<T> FireAsset(const TCHAR* Name)
	{
		return TSoftObjectPtr<T>(FSoftObjectPath(FString::Printf(TEXT("%s/%s.%s"), *FireFolder, Name, Name)));
	}

	/** The cards of one fire: all four corners of a card sit on its base point; the material unfolds it towards the camera
	 *  from UV channel 0 (u across, v from the base up) and channel 1 (width, height) */
	struct FCards
	{
		TArray<FVector> Vertices;
		TArray<int32> Triangles;
		TArray<FVector2D> UVs;
		TArray<FVector2D> Sizes;
		TArray<FLinearColor> Colors;
		TArray<FVector> Normals;

		/** The normal is the way the flame licks when the material follows the normals (a ball of fire) */
		void Add(const FVector& Base, float Width, float Height, const FLinearColor& Color, const FVector& Normal)
		{
			const int32 First = Vertices.Num();
			Normals.Append({ Normal, Normal, Normal, Normal });
			Vertices.Append({ Base, Base, Base, Base });
			UVs.Append({ FVector2D(0.0f, 0.0f), FVector2D(1.0f, 0.0f), FVector2D(1.0f, 1.0f), FVector2D(0.0f, 1.0f) });
			Sizes.Append({ FVector2D(Width, Height), FVector2D(Width, Height), FVector2D(Width, Height), FVector2D(Width, Height) });
			Colors.Append({ Color, Color, Color, Color });
			Triangles.Append({ First, First + 2, First + 1, First, First + 3, First + 2 });
		}
	};
}

UVaelFireSettings::UVaelFireSettings()
{
	FlameMaterial = FireAsset<UMaterialInterface>(TEXT("M_Feuer"));
	SparkMaterial = FireAsset<UMaterialInterface>(TEXT("M_Feuer_Funken"));
	ScorchMaterial = FireAsset<UMaterialInterface>(TEXT("M_Feuer_Brandfleck"));
	SmokeMaterial = FireAsset<UMaterialInterface>(TEXT("M_Feuer_Rauch"));
	FlameFlipbook = FireAsset<UMaterialInterface>(TEXT("MI_Feuer_Flamme"));
	FireFlipbook = FireAsset<UMaterialInterface>(TEXT("MI_Feuer_Feuer"));
	BallFlipbook = FireAsset<UMaterialInterface>(TEXT("MI_Feuer_Kugel"));
}

UVaelFireComponent::UVaelFireComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryComponentTick.bCanEverTick = true;
	SetCollisionEnabled(ECollisionEnabled::NoCollision);
	SetCanEverAffectNavigation(false);
	SetCastShadow(false);
	bUseAsyncCooking = false;

	// The flames rise straight up whatever they hang on, and the sparks fly above the cards
	SetUsingAbsoluteRotation(true);
	SetUsingAbsoluteScale(true);
	BoundsScale = 1.0f;
}

void UVaelFireComponent::Ignite(const FVaelFireShape& InShape, float FadeIn)
{
	Shape = InShape;
	TargetStrength = 1.0f;
	FadeSpeed = 1.0f / FMath::Max(FadeIn, 0.01f);
	OutTime = -1.0f;
	FlickerPhase = FMath::FRandRange(0.0f, 100.0f);
	SetWorldRotation(FRotator::ZeroRotator);

	const UVaelFireSettings* Settings = UVaelFireSettings::Get();
	if (FlameMaterial == nullptr)
	{
		// Simulated fire where its flipbook exists, else the drawn flames
		UMaterialInterface* Flipbook = VaelAssets::LoadOptional(InShape.bBall ? Settings->BallFlipbook : Settings->FlameFlipbook);
		FlameMaterial = UMaterialInstanceDynamic::Create(Flipbook != nullptr ? Flipbook : VaelAssets::LoadOptional(Settings->FlameMaterial), this);
		if (UMaterialInterface* BedBase = VaelAssets::LoadOptional(Settings->FireFlipbook))
		{
			BedMaterial = UMaterialInstanceDynamic::Create(BedBase, this);
		}
		SparkMaterial = UMaterialInstanceDynamic::Create(VaelAssets::LoadOptional(Settings->SparkMaterial), this);
		if (UMaterialInterface* SmokeBase = VaelAssets::LoadOptional(Settings->SmokeMaterial))
		{
			SmokeMaterial = UMaterialInstanceDynamic::Create(SmokeBase, this);
		}
	}
	FlameMaterial->SetScalarParameterValue(HeatParameter, Shape.Heat);
	FlameMaterial->SetScalarParameterValue(SmokeParameter, Shape.Smoke);
	FlameMaterial->SetScalarParameterValue(SpeedParameter, Shape.Speed);
	SparkMaterial->SetScalarParameterValue(SparkHeightParameter, Shape.Height * 1.6f);
	const bool bBallFlipbook = Shape.bBall && FlameMaterial->Parent == VaelAssets::LoadOptional(Settings->BallFlipbook);
	FlameMaterial->SetScalarParameterValue(NormalFlowParameter, Shape.bBall && !bBallFlipbook ? Shape.BallSpread : 0.0f);
	if (SmokeMaterial != nullptr)
	{
		SmokeMaterial->SetScalarParameterValue(SmokeParameter, Shape.Smoke);
		SmokeMaterial->SetScalarParameterValue(SpeedParameter, Shape.Speed);
	}
	BuildCards();

	// A charred patch under the fire, or on the wall it burns on
	if (Shape.bScorch && Scorch == nullptr)
	{
		if (UMaterialInterface* ScorchBase = VaelAssets::LoadOptional(Settings->ScorchMaterial))
		{
			ScorchMaterial = UMaterialInstanceDynamic::Create(ScorchBase, this);
			Scorch = NewObject<UDecalComponent>(GetOwner());
			Scorch->SetDecalMaterial(ScorchMaterial);
			Scorch->SetupAttachment(this);
			Scorch->SetUsingAbsoluteRotation(true);
			const bool bWall = !Shape.WallNormal.IsNearlyZero();
			const float Size = Shape.Radius * 1.35f;
			Scorch->DecalSize = FVector(40.0f, Size, bWall ? Size * 1.3f : Size);
			Scorch->RegisterComponent();
			Scorch->SetWorldRotation(bWall ? (-Shape.WallNormal).Rotation() : FRotator(-90.0f, FMath::FRandRange(0.0f, 360.0f), 0.0f));
			Scorch->SetRelativeLocation(bWall ? FVector(0.0f, 0.0f, Shape.Radius * 0.3f) : FVector::ZeroVector);
		}
	}

	if (Shape.bLight)
	{
		TakeLight();
	}
	SetComponentTickEnabled(true);
}

void UVaelFireComponent::Extinguish(float FadeOut, bool bInDestroyWhenOut)
{
	TargetStrength = 0.0f;
	FadeSpeed = 1.0f / FMath::Max(FadeOut, 0.01f);
	bDestroyWhenOut = bInDestroyWhenOut;
}

void UVaelFireComponent::BuildCards()
{
	const UVaelFireSettings* Settings = UVaelFireSettings::Get();
	const bool bWall = !Shape.WallNormal.IsNearlyZero();
	const FVector Out = bWall ? FVector(Shape.WallNormal.X, Shape.WallNormal.Y, 0.0f).GetSafeNormal() : FVector::ZeroVector;
	const FVector Along = bWall ? FVector::CrossProduct(FVector::UpVector, Out).GetSafeNormal() : FVector::ForwardVector;

	// More tongues for a bigger fire, the tallest in the middle
	const float Area = bWall ? Shape.Radius * 2.0f * Shape.Radius * 1.6f : UE_PI * FMath::Square(Shape.Radius);
	const int32 NumTongues = Shape.Tongues > 0 ? Shape.Tongues : FMath::Clamp(FMath::RoundToInt32(Area / 10000.0f * Settings->TonguesPerSquareMetre), Settings->MinTongues, Settings->MaxTongues);

	FCards Flames;
	TArray<FVector> SparkBases;
	FRandomStream Random(FMath::Rand());

	// Places a point on the burning patch: along the wall, on the disc, or on the ball; returns how far out from the middle (0 to 1)
	const auto PlaceOnPatch = [&](float Spread, FVector& OutBase, FVector& OutNormal)
	{
		OutNormal = FVector::UpVector;
		if (Shape.bBall)
		{
			OutNormal = Random.GetUnitVector();
			OutBase = OutNormal * Shape.Radius * Random.FRandRange(0.4f, 0.8f);
			return 0.3f;
		}
		if (bWall)
		{
			const float Offset = Random.FRandRange(-1.0f, 1.0f) * Spread;
			OutBase = Along * Offset * Shape.Radius + Out * Random.FRandRange(4.0f, 10.0f) + FVector(0.0f, 0.0f, Random.FRandRange(-0.25f, 0.15f) * Shape.Radius);
			return FMath::Abs(Offset);
		}
		const float Share = FMath::Sqrt(Random.FRand()) * Spread;
		const float Angle = Random.FRandRange(0.0f, 2.0f * UE_PI);
		OutBase = FVector(FMath::Cos(Angle), FMath::Sin(Angle), 0.0f) * Share * Shape.Radius;
		return Share;
	};

	// A burning ball: the flipbook shows the ball with its flames, so a few large cards on its middle are enough.
	// The simulated ball is 28 % of the frame high and sits at 35 % of its height.
	const bool bBallFlipbook = Shape.bBall && FlameMaterial != nullptr && FlameMaterial->Parent == VaelAssets::LoadOptional(Settings->BallFlipbook);
	for (int32 Index = 0; bBallFlipbook && Index < FMath::Min(NumTongues, 3); ++Index)
	{
		const float Size = Shape.Radius * 2.0f / 0.28f * Random.FRandRange(0.92f, 1.08f);
		const FVector Base = Random.GetUnitVector() * Shape.Radius * 0.08f - FVector(0.0f, 0.0f, Size * 0.35f);
		Flames.Add(Base, Size, Size, FLinearColor(Random.FRand(), 1.0f, 0.0f, 1.0f), FVector::UpVector);
		SparkBases.Add(FVector::ZeroVector);
	}

	for (int32 Index = 0; !bBallFlipbook && Index < NumTongues; ++Index)
	{
		FVector Base;
		FVector Normal;
		float FromMiddle = PlaceOnPatch(0.85f, Base, Normal);
		if (Index == 0 && !bWall && !Shape.bBall)
		{
			Base = FVector::ZeroVector;
			FromMiddle = 0.0f;
		}

		const float Middle = 1.0f - 0.45f * FromMiddle;
		const float Height = Shape.Height * Middle * Random.FRandRange(0.55f, 1.0f);
		// The flipbook frames are square with the flame in their middle
		const float Width = Height * Random.FRandRange(0.85f, 1.0f);

		// The foot sits in the ground, so the ground hides the round bottom of the card and the flame grows out of it
		Base.Z -= bWall ? Height * 0.05f : Shape.bFloating ? 0.0f : Height * 0.03f;
		Flames.Add(Base, Width, Height, FLinearColor(Random.FRand(), 0.7f + 0.4f * Middle, 0.0f, 1.0f), Normal);
		SparkBases.Add(Base + FVector(0.0f, 0.0f, Height * 0.25f));
	}

	ClearAllMeshSections();
	CreateMeshSection_LinearColor(0, Flames.Vertices, Flames.Triangles, Flames.Normals, Flames.UVs, Flames.Sizes, TArray<FVector2D>(), TArray<FVector2D>(),
		Flames.Colors, TArray<FProcMeshTangent>(), false);
	SetMaterial(0, FlameMaterial);

	// A bed of broad simulated fire that clings to the ground or the wall, so the fire burns as one patch and not as single tongues
	if (!Shape.bFloating && BedMaterial != nullptr && Shape.Radius > 110.0f)
	{
		FCards Bed;
		const int32 NumBed = FMath::Clamp(NumTongues / 2, 1, 6);
		for (int32 Index = 0; Index < NumBed; ++Index)
		{
			FVector Base;
			FVector Normal;
			PlaceOnPatch(Index == 0 ? 0.0f : 0.6f, Base, Normal);
			const float Size = FMath::Max(Shape.Radius * Random.FRandRange(1.6f, 2.1f), Shape.Height * 0.55f);
			Base.Z -= bWall ? Size * 0.05f : Size * 0.03f;
			Bed.Add(Base, Size, Size * 1.1f, FLinearColor(Random.FRand(), 1.0f, 0.0f, 1.0f), Normal);
		}
		CreateMeshSection_LinearColor(3, Bed.Vertices, Bed.Triangles, Bed.Normals, Bed.UVs, Bed.Sizes, TArray<FVector2D>(), TArray<FVector2D>(),
			Bed.Colors, TArray<FProcMeshTangent>(), false);
		SetMaterial(3, BedMaterial);
	}

	// Sparks: tiny cards that the material lifts, each in its own loop
	if (Shape.bSparks)
	{
		FCards Sparks;
		const int32 NumSparks = FMath::Clamp(NumTongues * 2, 4, 60);
		for (int32 Index = 0; Index < NumSparks; ++Index)
		{
			const FVector Base = SparkBases[Index % SparkBases.Num()] + FVector(Random.FRandRange(-6.0f, 6.0f), Random.FRandRange(-6.0f, 6.0f), 0.0f);
			Sparks.Add(Base, SparkSize, SparkSize, FLinearColor(Random.FRand(), Random.FRand(), Random.FRandRange(0.5f, 1.3f), 1.0f), FVector::UpVector);
		}
		CreateMeshSection_LinearColor(1, Sparks.Vertices, Sparks.Triangles, Sparks.Normals, Sparks.UVs, Sparks.Sizes, TArray<FVector2D>(), TArray<FVector2D>(),
			Sparks.Colors, TArray<FProcMeshTangent>(), false);
		SetMaterial(1, SparkMaterial);
	}

	// A column of dark smoke above a smoky fire
	if (Shape.Smoke > 0.3f && SmokeMaterial != nullptr)
	{
		FCards Smoke;
		const int32 NumPuffs = FMath::Clamp(NumTongues / 4, 1, 5);
		for (int32 Index = 0; Index < NumPuffs; ++Index)
		{
			FVector Base;
			FVector Normal;
			PlaceOnPatch(0.4f, Base, Normal);
			Base.Z = (Shape.bBall ? 0.0f : Shape.Height * 0.4f) + Random.FRandRange(-0.1f, 0.1f) * Shape.Height;
			const float Height = Shape.Height * Random.FRandRange(1.4f, 2.0f);
			Smoke.Add(Base, Shape.Radius * 1.1f + Shape.Height * 0.35f, Height, FLinearColor(Random.FRand(), 0.0f, 0.0f, 1.0f), FVector::UpVector);
		}
		CreateMeshSection_LinearColor(2, Smoke.Vertices, Smoke.Triangles, Smoke.Normals, Smoke.UVs, Smoke.Sizes, TArray<FVector2D>(), TArray<FVector2D>(),
			Smoke.Colors, TArray<FProcMeshTangent>(), false);
		SetMaterial(2, SmokeMaterial);
	}
}

FBoxSphereBounds UVaelFireComponent::CalcBounds(const FTransform& LocalToWorld) const
{
	// The cards unfold from their base points in the material: the whole fire with its rising sparks
	const float Reach = Shape.Radius + Shape.Height * 0.5f;
	const FBox Box(FVector(-Reach, -Reach, -Shape.Height * 0.3f), FVector(Reach, Reach, Shape.Height * 2.0f));
	return FBoxSphereBounds(Box).TransformBy(LocalToWorld);
}

void UVaelFireComponent::TakeLight()
{
	if (Light != nullptr || NumLitFires >= UVaelFireSettings::Get()->MaxLitFires)
	{
		return;
	}
	++NumLitFires;
	Light = NewObject<UPointLightComponent>(GetOwner());
	Light->SetupAttachment(this);
	Light->SetUsingAbsoluteRotation(true);
	Light->SetRelativeLocation(FVector(0.0f, 0.0f, Shape.Height * 0.45f) + Shape.WallNormal * 25.0f);
	Light->SetLightColor(UVaelFireSettings::Get()->LightColor);
	Light->SetIntensityUnits(ELightUnits::Candelas);
	Light->SetIntensity(0.0f);
	Light->SetAttenuationRadius(300.0f + Shape.Radius * 7.0f);
	Light->SetCastShadows(false);
	Light->RegisterComponent();
}

void UVaelFireComponent::ReleaseLight()
{
	if (Light != nullptr)
	{
		Light->DestroyComponent();
		Light = nullptr;
		NumLitFires = FMath::Max(0, NumLitFires - 1);
	}
}

void UVaelFireComponent::OnUnregister()
{
	ReleaseLight();
	if (Scorch != nullptr)
	{
		Scorch->DestroyComponent();
		Scorch = nullptr;
	}
	Super::OnUnregister();
}

void UVaelFireComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	Strength = FMath::FInterpConstantTo(Strength, TargetStrength, DeltaTime, FadeSpeed);
	if (FlameMaterial != nullptr)
	{
		FlameMaterial->SetScalarParameterValue(StrengthParameter, Strength);
		SparkMaterial->SetScalarParameterValue(StrengthParameter, Strength);
		if (BedMaterial != nullptr)
		{
			BedMaterial->SetScalarParameterValue(StrengthParameter, Strength);
		}
		if (SmokeMaterial != nullptr)
		{
			SmokeMaterial->SetScalarParameterValue(StrengthParameter, Strength);
		}
	}

	// The light flickers with the flames: a slow breath and a quick restless flutter
	if (Light != nullptr)
	{
		const float Time = GetWorld()->GetTimeSeconds() + FlickerPhase;
		const float Flicker = 1.0f + 0.18f * FMath::Sin(Time * 2.3f) + 0.12f * FMath::Sin(Time * 7.9f + 1.3f) + 0.08f * FMath::Sin(Time * 17.0f);
		const float Size = FMath::Sqrt(Shape.Radius / 50.0f) * Shape.Heat;
		Light->SetIntensity(UVaelFireSettings::Get()->LightIntensity * Size * Strength * Flicker);
		if (Strength <= 0.0f && TargetStrength <= 0.0f)
		{
			ReleaseLight();
		}
	}

	// The scorch mark glows while the fire burns, its embers die down after, then it fades away
	if (Strength <= 0.0f && TargetStrength <= 0.0f)
	{
		OutTime = OutTime < 0.0f ? 0.0f : OutTime + DeltaTime;
	}
	if (ScorchMaterial != nullptr)
	{
		const float Linger = UVaelFireSettings::Get()->ScorchLinger;
		const float Cooling = OutTime < 0.0f ? 1.0f : FMath::Clamp(1.0f - OutTime / 4.0f, 0.0f, 1.0f);
		ScorchMaterial->SetScalarParameterValue(EmberParameter, FMath::Max(Strength, Cooling * 0.6f));
		ScorchMaterial->SetScalarParameterValue(CoverParameter, OutTime < 0.0f ? 0.9f * FMath::Min(1.0f, Strength * 3.0f) : 0.9f * FMath::Clamp(1.0f - OutTime / FMath::Max(Linger, 0.1f), 0.0f, 1.0f));
	}

	const bool bScorchGone = Scorch == nullptr || OutTime >= UVaelFireSettings::Get()->ScorchLinger;
	if (bDestroyWhenOut && OutTime >= 0.0f && bScorchGone)
	{
		DestroyComponent();
	}
}

void UVaelFireComponent::SetFlowDirection(const FVector& Direction)
{
	const FLinearColor Value(Direction.GetSafeNormal());
	for (UMaterialInstanceDynamic* Material : { FlameMaterial.Get(), SparkMaterial.Get(), SmokeMaterial.Get(), BedMaterial.Get() })
	{
		if (Material != nullptr)
		{
			Material->SetVectorParameterValue(FlowParameter, Value);
		}
	}
}

void UVaelFireComponent::SetSizeScale(float Scale)
{
	for (UMaterialInstanceDynamic* Material : { FlameMaterial.Get(), SparkMaterial.Get(), SmokeMaterial.Get(), BedMaterial.Get() })
	{
		if (Material != nullptr)
		{
			Material->SetScalarParameterValue(SizeParameter, Scale);
		}
	}
}
