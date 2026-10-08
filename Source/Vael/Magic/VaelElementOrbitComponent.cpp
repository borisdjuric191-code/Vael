// Copyright Epic Games, Inc. All Rights Reserved.

#include "Magic/VaelElementOrbitComponent.h"
#include "UObject/ConstructorHelpers.h"
#include "Components/PointLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Magic/VaelElementComponent.h"
#include "Magic/VaelMagicSettings.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "VaelAssets.h"

namespace
{
	/** Diameter of the engine sphere in cm */
	constexpr float OrbitSphereDiameter = 100.0f;

	/** Seconds a new orb needs to fly out from the middle to the circle */
	constexpr float OrbitAppearTime = 0.3f;

	/** A new orb pops up this much bigger, swings and settles */
	constexpr float OrbitPopSize = 0.5f;
	constexpr float OrbitPopDecay = 8.0f;
	constexpr float OrbitPopSwing = 20.0f;

	/** How fast the orbs glide to their new share of the circle when one joins */
	constexpr float OrbitSpreadSpeed = 6.0f;

	/** How far the orbs float up and down in cm */
	constexpr float OrbitFloatHeight = 3.0f;

	/** Number of flames a fire orb drags behind, and how quickly the first one follows the orb */
	constexpr int32 OrbitFlameCount = 5;
	constexpr float OrbitFlameFollowSpeed = 16.0f;

	/** Number of drops of a water orb, seconds each one falls and how far in cm */
	constexpr int32 OrbitDropCount = 3;
	constexpr float OrbitDropTime = 1.3f;
	constexpr float OrbitDropDistance = 55.0f;

	/** Number of wind streaks of an air orb */
	constexpr int32 OrbitStreakCount = 4;

	/** Number of crumbs of an earth orb, seconds each one falls */
	constexpr int32 OrbitCrumbCount = 3;
	constexpr float OrbitCrumbTime = 1.7f;

	/** Largest side of the rock mesh in cm, to bring it to the size of an orb */
	constexpr float OrbitRockSize = 240.0f;

	/** Material parameters */
	const FName OrbitColorParameter(TEXT("Color"));
	const FName OrbitGlowParameter(TEXT("Glow"));
	const FName OrbitRimParameter(TEXT("Rim"));

	/** Looks of the elements */
	const FLinearColor OrbitFireCore(1.0f, 0.55f, 0.16f);
	const FLinearColor OrbitFireFlame(1.0f, 0.24f, 0.05f);
	const FLinearColor OrbitWaterCore(0.04f, 0.22f, 0.6f);
	const FLinearColor OrbitWaterSheen(0.35f, 0.7f, 1.0f);
	const FLinearColor OrbitAirWind(0.75f, 0.9f, 1.0f);
	const FLinearColor OrbitEarthRock(0.32f, 0.24f, 0.17f);
	const FLinearColor OrbitMarkCore(0.12f, 0.02f, 0.2f);
	const FLinearColor OrbitMarkHalo(0.64f, 0.3f, 1.0f);
}

UVaelElementOrbitComponent::UVaelElementOrbitComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = true;

	// The circle keeps its place in the world while the mage turns
	SetUsingAbsoluteRotation(true);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> Sphere(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	if (Sphere.Succeeded())
	{
		SphereMesh = Sphere.Object;
	}

	static ConstructorHelpers::FObjectFinder<UMaterialInterface> ShapeMaterial(TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
	if (ShapeMaterial.Succeeded())
	{
		CoreMaterial = ShapeMaterial.Object;
		GlowMaterial = ShapeMaterial.Object;
	}
}

void UVaelElementOrbitComponent::BeginPlay()
{
	Super::BeginPlay();

	// The own materials and the rock take over from the engine shapes as soon as they exist
	const UVaelMagicSettings* MagicSettings = UVaelMagicSettings::Get();

	if (UMaterialInterface* Core = VaelAssets::LoadOptional(MagicSettings->ElementOrbCoreMaterial))
	{
		CoreMaterial = Core;
	}

	if (UMaterialInterface* Glow = VaelAssets::LoadOptional(MagicSettings->ElementOrbGlowMaterial))
	{
		GlowMaterial = Glow;
	}

	RockMesh = VaelAssets::LoadOptional(MagicSettings->ElementOrbRockMesh);
}

void UVaelElementOrbitComponent::ShowQueue(const UVaelElementComponent& Elements)
{
	const TArray<EVaelElement>& Queue = Elements.GetQueue();

	// The queue only grows at its end; anything else, like a cast, starts the circle anew
	bool bGrew = Queue.Num() >= Orbs.Num();
	for (int32 Index = 0; bGrew && Index < Orbs.Num(); ++Index)
	{
		bGrew = Orbs[Index].Element == Queue[Index];
	}

	if (!bGrew)
	{
		ClearOrbs();
	}

	const float Now = GetWorld()->GetTimeSeconds();

	for (int32 Index = Orbs.Num(); Index < Queue.Num(); ++Index)
	{
		FVaelElementOrb& Orb = Orbs.AddDefaulted_GetRef();
		Orb.Element = Queue[Index];
		Orb.bFromEnvironment = Elements.IsFromEnvironment(Index);
		Orb.BirthTime = Now;
		Orb.Size = OrbDiameter / OrbitSphereDiameter * (Orb.bFromEnvironment ? EnvironmentScale : 1.0f);

		// A new orb appears where its share of the circle will be, the others make room
		Orb.AngleOffset = Index * 360.0f / Queue.Num();

		BuildOrb(Orb);
	}
}

UStaticMeshComponent* UVaelElementOrbitComponent::AddShape(USceneComponent* Parent, UStaticMesh* Mesh, UMaterialInterface* Material, const FLinearColor& Color, float Glow, float Rim)
{
	UStaticMeshComponent* Shape = NewObject<UStaticMeshComponent>(GetOwner());
	Shape->SetupAttachment(Parent);
	Shape->SetStaticMesh(Mesh);
	Shape->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Shape->SetCastShadow(false);
	Shape->bReceivesDecals = false;
	Shape->SetRelativeScale3D(FVector(KINDA_SMALL_NUMBER));
	Shape->RegisterComponent();

	if (Material != nullptr)
	{
		Shape->SetMaterial(0, Material);

		if (UMaterialInstanceDynamic* Dynamic = Shape->CreateAndSetMaterialInstanceDynamic(0))
		{
			Dynamic->SetVectorParameterValue(OrbitColorParameter, Color);
			Dynamic->SetScalarParameterValue(OrbitGlowParameter, Glow);
			Dynamic->SetScalarParameterValue(OrbitRimParameter, Rim);
		}
	}

	return Shape;
}

void UVaelElementOrbitComponent::AddLight(FVaelElementOrb& Orb, const FLinearColor& Color, float Intensity)
{
	Orb.Light = NewObject<UPointLightComponent>(GetOwner());
	Orb.Light->SetupAttachment(Orb.Pivot);
	Orb.Light->SetMobility(EComponentMobility::Movable);
	Orb.Light->SetIntensityUnits(ELightUnits::Candelas);
	Orb.Light->SetIntensity(Intensity);
	Orb.Light->SetLightColor(Color);
	Orb.Light->SetAttenuationRadius(260.0f);
	Orb.Light->SetSourceRadius(4.0f);
	Orb.Light->SetCastShadows(false);
	Orb.Light->RegisterComponent();

	Orb.LightIntensity = Intensity;
}

void UVaelElementOrbitComponent::BuildOrb(FVaelElementOrb& Orb)
{
	Orb.Pivot = NewObject<USceneComponent>(GetOwner());
	Orb.Pivot->SetupAttachment(this);
	Orb.Pivot->RegisterComponent();

	switch (Orb.Element)
	{
	case EVaelElement::Fire:
		// A small glowing ball in a flickering halo that drags its flames behind
		Orb.Parts.Add(AddShape(Orb.Pivot, SphereMesh, CoreMaterial, OrbitFireCore, 14.0f));
		Orb.Parts.Add(AddShape(Orb.Pivot, SphereMesh, GlowMaterial, OrbitFireCore, 2.5f));

		for (int32 FlameIndex = 0; FlameIndex < OrbitFlameCount; ++FlameIndex)
		{
			const FLinearColor FlameColor = FMath::Lerp(OrbitFireCore, OrbitFireFlame, (FlameIndex + 1.0f) / OrbitFlameCount);
			UStaticMeshComponent* Flame = AddShape(this, SphereMesh, GlowMaterial, FlameColor, 3.0f - FlameIndex * 0.35f);

			// Flames stay behind in the world instead of turning with the circle
			Flame->SetUsingAbsoluteLocation(true);
			Flame->SetWorldLocation(GetComponentLocation());
			Orb.Trail.Add(Flame);
		}

		AddLight(Orb, OrbitFireCore, 7.0f);
		break;

	case EVaelElement::Water:
		// A wobbling ball of water with a pale sheen, drops fall from it
		Orb.Parts.Add(AddShape(Orb.Pivot, SphereMesh, CoreMaterial, OrbitWaterCore, 0.6f));
		Orb.Parts.Add(AddShape(Orb.Pivot, SphereMesh, GlowMaterial, OrbitWaterSheen, 1.6f, 1.0f));

		for (int32 DropIndex = 0; DropIndex < OrbitDropCount; ++DropIndex)
		{
			Orb.Parts.Add(AddShape(Orb.Pivot, SphereMesh, CoreMaterial, OrbitWaterSheen, 0.8f));
		}

		AddLight(Orb, OrbitWaterSheen, 2.5f);
		break;

	case EVaelElement::Air:
		// A ball of whirling wind: a bright middle in a thin shell, streaks racing around it in tilted circles
		Orb.Parts.Add(AddShape(Orb.Pivot, SphereMesh, GlowMaterial, OrbitAirWind, 3.0f));
		Orb.Parts.Add(AddShape(Orb.Pivot, SphereMesh, GlowMaterial, OrbitAirWind, 1.2f, 1.0f));

		for (int32 StreakIndex = 0; StreakIndex < OrbitStreakCount; ++StreakIndex)
		{
			Orb.Parts.Add(AddShape(Orb.Pivot, SphereMesh, GlowMaterial, OrbitAirWind, 4.0f));
		}
		break;

	case EVaelElement::Earth:
		// A tumbling rock that keeps losing crumbs
		for (int32 RockIndex = 0; RockIndex <= OrbitCrumbCount; ++RockIndex)
		{
			Orb.Parts.Add(RockMesh != nullptr ? AddShape(Orb.Pivot, RockMesh, nullptr, OrbitEarthRock, 0.0f) : AddShape(Orb.Pivot, SphereMesh, CoreMaterial, OrbitEarthRock, 0.0f));
		}
		break;

	case EVaelElement::Mark:
		// A dark heart in a restless violet halo
		Orb.Parts.Add(AddShape(Orb.Pivot, SphereMesh, CoreMaterial, OrbitMarkCore, 2.0f));
		Orb.Parts.Add(AddShape(Orb.Pivot, SphereMesh, GlowMaterial, OrbitMarkHalo, 3.0f, 1.0f));
		AddLight(Orb, OrbitMarkHalo, 4.0f);
		break;
	}
}

void UVaelElementOrbitComponent::DestroyOrb(FVaelElementOrb& Orb)
{
	for (UStaticMeshComponent* Shape : Orb.Parts)
	{
		if (Shape != nullptr)
		{
			Shape->DestroyComponent();
		}
	}

	for (UStaticMeshComponent* Shape : Orb.Trail)
	{
		if (Shape != nullptr)
		{
			Shape->DestroyComponent();
		}
	}

	if (Orb.Light != nullptr)
	{
		Orb.Light->DestroyComponent();
	}

	if (Orb.Pivot != nullptr)
	{
		Orb.Pivot->DestroyComponent();
	}
}

void UVaelElementOrbitComponent::ClearOrbs()
{
	for (FVaelElementOrb& Orb : Orbs)
	{
		DestroyOrb(Orb);
	}

	Orbs.Reset();
}

void UVaelElementOrbitComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (Orbs.IsEmpty())
	{
		return;
	}

	const float Time = GetWorld()->GetTimeSeconds();
	OrbitAngle = FMath::Fmod(OrbitAngle + OrbitSpeed * DeltaTime, 360.0f);

	for (int32 Index = 0; Index < Orbs.Num(); ++Index)
	{
		FVaelElementOrb& Orb = Orbs[Index];
		if (Orb.Pivot == nullptr)
		{
			continue;
		}

		// Every orb glides to its even share of the circle
		const float TargetOffset = Index * 360.0f / Orbs.Num();
		Orb.AngleOffset = FMath::FInterpTo(Orb.AngleOffset, TargetOffset, DeltaTime, OrbitSpreadSpeed);

		// A new orb flies out from the middle, pops up and settles
		const float Age = Time - Orb.BirthTime;
		const float Appear = FMath::SmoothStep(0.0f, 1.0f, Age / OrbitAppearTime);
		const float Grow = Appear * (1.0f + OrbitPopSize * FMath::Exp(-Age * OrbitPopDecay) * FMath::Cos(Age * OrbitPopSwing));

		const float Angle = FMath::DegreesToRadians(OrbitAngle + Orb.AngleOffset);
		Orb.Pivot->SetRelativeLocation(FVector(FMath::Cos(Angle) * OrbitRadius * Appear, FMath::Sin(Angle) * OrbitRadius * Appear, OrbitFloatHeight * FMath::Sin(Time * 2.4f + Index * 1.1f)));

		switch (Orb.Element)
		{
		case EVaelElement::Fire:
			AnimateFire(Orb, Index, Time, DeltaTime, Grow);
			break;

		case EVaelElement::Water:
			AnimateWater(Orb, Index, Time, Grow);
			break;

		case EVaelElement::Air:
			AnimateAir(Orb, Index, Time, Grow);
			break;

		case EVaelElement::Earth:
			AnimateEarth(Orb, Index, Time, Grow);
			break;

		case EVaelElement::Mark:
			AnimateMark(Orb, Index, Time, Grow);
			break;
		}
	}
}

void UVaelElementOrbitComponent::AnimateFire(FVaelElementOrb& Orb, int32 Index, float Time, float DeltaTime, float Grow)
{
	const float Flicker = FMath::PerlinNoise1D(Time * 6.0f + Index * 13.7f);

	Orb.Parts[0]->SetRelativeScale3D(FVector(Orb.Size * 0.8f * Grow * (1.0f + 0.08f * FMath::Sin(Time * 17.0f + Index))));
	Orb.Parts[1]->SetRelativeScale3D(FVector(Orb.Size * 1.7f * Grow * (1.0f + 0.2f * Flicker)));

	// Each flame chases the one before it and rises a little, so the fire trails behind the orb and licks upwards
	FVector Target = Orb.Pivot->GetComponentLocation();
	for (int32 FlameIndex = 0; FlameIndex < Orb.Trail.Num(); ++FlameIndex)
	{
		UStaticMeshComponent* Flame = Orb.Trail[FlameIndex];
		const FVector Location = FMath::VInterpTo(Flame->GetComponentLocation(), Target + FVector(0.0f, 0.0f, 1.5f), DeltaTime, OrbitFlameFollowSpeed - FlameIndex * 1.5f);

		Flame->SetWorldLocation(Location);
		Flame->SetRelativeScale3D(FVector(Orb.Size * Grow * (1.25f - 0.2f * FlameIndex) * (1.0f + 0.18f * FMath::Sin(Time * 21.0f + FlameIndex * 2.3f + Index))));

		Target = Location;
	}

	Orb.Light->SetIntensity(Orb.LightIntensity * Grow * (1.0f + 0.3f * Flicker));
}

void UVaelElementOrbitComponent::AnimateWater(FVaelElementOrb& Orb, int32 Index, float Time, float Grow)
{
	// The ball wobbles: it swells along one axis while it shrinks along the others
	const float Wave = Time * 5.0f + Index;
	const FVector Wobble(1.0f + 0.12f * FMath::Sin(Wave), 1.0f + 0.12f * FMath::Sin(Wave + 2.1f), 1.0f + 0.12f * FMath::Sin(Wave + 4.2f));

	Orb.Parts[0]->SetRelativeScale3D(Wobble * Orb.Size * Grow);
	Orb.Parts[1]->SetRelativeScale3D(Wobble * Orb.Size * 1.18f * Grow);

	// Drops form below the ball, fall faster and faster and vanish
	for (int32 DropIndex = 0; DropIndex < OrbitDropCount; ++DropIndex)
	{
		const float Fall = FMath::Frac((Time + DropIndex * OrbitDropTime / OrbitDropCount + Index * 0.2f) / OrbitDropTime);
		const float Form = FMath::Clamp(Fall / 0.2f, 0.0f, 1.0f);
		const float Vanish = 1.0f - FMath::SmoothStep(0.75f, 1.0f, Fall);
		const float Side = (DropIndex - (OrbitDropCount - 1) * 0.5f) * Orb.Size * 22.0f;

		UStaticMeshComponent* Drop = Orb.Parts[2 + DropIndex];
		Drop->SetRelativeLocation(FVector(Side, -Side * 0.5f, -Orb.Size * OrbitSphereDiameter * 0.42f - OrbitDropDistance * FMath::Square(Fall)));
		Drop->SetRelativeScale3D(FVector(0.8f, 0.8f, 1.25f) * Orb.Size * 0.24f * Form * Vanish * Grow);
	}

	Orb.Light->SetIntensity(Orb.LightIntensity * Grow * (1.0f + 0.1f * FMath::Sin(Wave)));
}

void UVaelElementOrbitComponent::AnimateAir(FVaelElementOrb& Orb, int32 Index, float Time, float Grow)
{
	Orb.Parts[0]->SetRelativeScale3D(FVector(Orb.Size * 0.5f * Grow * (1.0f + 0.1f * FMath::Sin(Time * 14.0f + Index))));
	Orb.Parts[1]->SetRelativeScale3D(FVector(Orb.Size * 1.2f * Grow));

	// Streaks of wind race around the middle, each in its own tilted circle and direction
	for (int32 StreakIndex = 0; StreakIndex < OrbitStreakCount; ++StreakIndex)
	{
		const FQuat Tilt = FRotator(StreakIndex == 0 ? 0.0f : 62.0f, StreakIndex * 120.0f, 0.0f).Quaternion();
		const float Speed = (StreakIndex % 2 == 0 ? 1.0f : -1.0f) * (760.0f + StreakIndex * 110.0f);
		const FQuat Spin(FVector::UpVector, FMath::DegreesToRadians(Time * Speed + StreakIndex * 77.0f));

		UStaticMeshComponent* Streak = Orb.Parts[2 + StreakIndex];
		Streak->SetRelativeRotation(Tilt * Spin);
		Streak->SetRelativeScale3D(FVector(1.3f, 0.14f, 0.14f) * Orb.Size * Grow);
	}
}

void UVaelElementOrbitComponent::AnimateEarth(FVaelElementOrb& Orb, int32 Index, float Time, float Grow)
{
	// The rock mesh is far bigger than an engine sphere, the brown sphere that stands in for it isn't
	const float RockScale = RockMesh != nullptr ? Orb.Size * OrbitSphereDiameter * 1.25f / OrbitRockSize : Orb.Size;

	Orb.Parts[0]->SetRelativeRotation(FRotator(Time * 23.0f + Index * 40.0f, Time * 31.0f, Time * 17.0f));
	Orb.Parts[0]->SetRelativeScale3D(FVector(RockScale * Grow));

	// Crumbs break off below, tumble down and vanish
	for (int32 CrumbIndex = 0; CrumbIndex < OrbitCrumbCount; ++CrumbIndex)
	{
		const float Fall = FMath::Frac((Time + CrumbIndex * OrbitCrumbTime / OrbitCrumbCount + Index * 0.3f) / OrbitCrumbTime);
		const float Vanish = 1.0f - FMath::SmoothStep(0.8f, 1.0f, Fall);
		const float Side = (CrumbIndex - (OrbitCrumbCount - 1) * 0.5f) * Orb.Size * 30.0f;

		UStaticMeshComponent* Crumb = Orb.Parts[1 + CrumbIndex];
		Crumb->SetRelativeLocation(FVector(Side, Side * 0.6f, -Orb.Size * OrbitSphereDiameter * 0.3f - OrbitDropDistance * FMath::Square(Fall)));
		Crumb->SetRelativeRotation(FRotator(Time * 190.0f + CrumbIndex * 70.0f, Time * 140.0f, 0.0f));
		Crumb->SetRelativeScale3D(FVector(RockScale * (0.2f + 0.05f * CrumbIndex) * Vanish * Grow));
	}
}

void UVaelElementOrbitComponent::AnimateMark(FVaelElementOrb& Orb, int32 Index, float Time, float Grow)
{
	// The halo twitches without a rhythm, like something alive
	const float Twitch = FMath::PerlinNoise1D(Time * 3.1f + Index * 7.3f);

	Orb.Parts[0]->SetRelativeScale3D(FVector(Orb.Size * 0.9f * Grow));
	Orb.Parts[1]->SetRelativeScale3D(FVector(Orb.Size * (1.6f + 0.35f * Twitch) * Grow));
	Orb.Light->SetIntensity(Orb.LightIntensity * Grow * (1.0f + 0.4f * Twitch));
}
