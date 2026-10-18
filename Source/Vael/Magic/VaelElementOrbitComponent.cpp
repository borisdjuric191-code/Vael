// Copyright Epic Games, Inc. All Rights Reserved.

#include "Magic/VaelElementOrbitComponent.h"
#include "UObject/ConstructorHelpers.h"
#include "Components/PointLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Magic/VaelElementComponent.h"
#include "Magic/VaelEnvironmentStatics.h"
#include "Magic/VaelMagicSettings.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Camera/PlayerCameraManager.h"
#include "GameFramework/PlayerController.h"
#include "ProceduralMeshComponent.h"
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

	/** Rings and sides of the tube of drawn water; how high its arc bends and how far it winds sideways in cm */
	constexpr int32 OrbitStrandRings = 40;
	constexpr int32 OrbitStrandSides = 10;

	/** Tiny bubbles racing up inside the strand, and drops it spits out where it leaves the water */
	constexpr int32 OrbitStrandBubbles = 26;
	constexpr int32 OrbitStrandSpits = 10;
	constexpr float OrbitStrandArc = 70.0f;
	constexpr float OrbitStrandWind = 14.0f;

	/** Share of the strand's time it needs to reach the orb, and to drain into it at the end */
	constexpr float OrbitStrandRise = 0.3f;
	constexpr float OrbitStrandDrain = 0.35f;

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

		// Water drawn from the surroundings rises from that water to the orb
		FVector StrandStart;
		FLinearColor WaterColor;
		if (Orb.Element == EVaelElement::Water && Orb.bFromEnvironment && UVaelEnvironmentStatics::FindWaterDrawPoint(GetOwner(), StrandStart, WaterColor))
		{
			Orb.StrandStart = StrandStart;
			BuildStrand(Orb, WaterColor);
		}
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

void UVaelElementOrbitComponent::BuildStrand(FVaelElementOrb& Orb, const FLinearColor& WaterColor)
{
	UMaterialInterface* WaveMaterial = VaelAssets::LoadOptional(UVaelMagicSettings::Get()->WaveMaterial);
	if (WaveMaterial == nullptr)
	{
		return;
	}

	// A tube of water built anew every frame, lying in the world between the water and the orb, not on the circle
	Orb.Strand = NewObject<UProceduralMeshComponent>(GetOwner());
	Orb.Strand->SetupAttachment(this);
	Orb.Strand->SetUsingAbsoluteLocation(true);
	Orb.Strand->SetUsingAbsoluteRotation(true);
	Orb.Strand->SetUsingAbsoluteScale(true);
	Orb.Strand->SetWorldTransform(FTransform::Identity);
	Orb.Strand->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Orb.Strand->SetCastShadow(false);
	Orb.Strand->RegisterComponent();

	// It leaves the pond in the dark water of the pond and turns into spell water on its way up
	if (UMaterialInstanceDynamic* Material = UMaterialInstanceDynamic::Create(WaveMaterial, this))
	{
		Material->SetVectorParameterValue(TEXT("DeepColor"), WaterColor);
		Orb.Strand->SetMaterial(0, Material);
		Orb.Strand->SetMaterial(1, Material);
	}

	Orb.StrandSeed = FMath::FRandRange(0.0f, 100.0f);
}

void UVaelElementOrbitComponent::AnimateStrand(FVaelElementOrb& Orb, float Time)
{
	const float Age = Time - Orb.BirthTime;
	const float Progress = Age / UVaelMagicSettings::Get()->WaterDrawStrandTime;

	if (Progress >= 1.0f)
	{
		Orb.Strand->DestroyComponent();
		Orb.Strand = nullptr;
		return;
	}

	// The head shoots up from the water, at the end the tail leaves the water and the rest drains into the orb
	const float Head = FMath::Max(FMath::SmoothStep(0.0f, OrbitStrandRise, Progress), 0.02f);
	const float Tail = FMath::SmoothStep(1.0f - OrbitStrandDrain, 1.0f, Progress);

	// An arc from the water up to the orb that winds sideways like a rope being pulled
	const FVector Start = Orb.StrandStart;
	const FVector End = Orb.Pivot->GetComponentLocation();
	const FVector Bend = (Start + End) * 0.5f + FVector(0.0f, 0.0f, OrbitStrandArc);
	const FVector Side = FVector::CrossProduct(End - Start, FVector::UpVector).GetSafeNormal();
	const float Length = FVector::Dist(Start, End) / 100.0f;
	const float Seed = Orb.StrandSeed;

	auto PointAt = [&](float Along)
	{
		const float Rest = 1.0f - Along;
		const float Winding = OrbitStrandWind * FMath::Sin(Along * UE_PI * 3.0f - Age * 8.0f) * FMath::Sin(Along * UE_PI);
		return Start * Rest * Rest + Bend * 2.0f * Rest * Along + End * Along * Along + Side * Winding;
	};

	TArray<FVector> Vertices;
	TArray<FVector> Normals;
	TArray<FVector2D> UVs;
	TArray<FLinearColor> Colors;
	Vertices.Reserve((OrbitStrandRings + 1) * OrbitStrandSides);

	// Rings around the curve from the tail to the head; water is never round: every ring bulges and churns
	for (int32 RingIndex = 0; RingIndex <= OrbitStrandRings; ++RingIndex)
	{
		const float Share = RingIndex / static_cast<float>(OrbitStrandRings);
		const float Along = FMath::Lerp(Tail, Head, Share);
		const FVector Center = PointAt(Along);
		const FVector Forward = (PointAt(FMath::Min(Along + 0.01f, 1.0f)) - PointAt(FMath::Max(Along - 0.01f, 0.0f))).GetSafeNormal();
		const FVector Right = FVector::CrossProduct(Forward, FVector::UpVector).GetSafeNormal();
		const FVector Up = FVector::CrossProduct(Right, Forward);

		// Thick where it leaves the water, thin where it pours into the orb, both ends closing; swells run up towards the orb
		const float Taper = FMath::Min(1.0f, Share * 6.0f + 0.35f) * FMath::Min(1.0f, (1.0f - Share) * 8.0f + 0.1f);
		const float Radius = Orb.Size * 100.0f * FMath::Lerp(0.5f, 0.18f, Along) * Taper
			* (1.0f + 0.35f * FMath::Sin(Along * 16.0f - Age * 13.0f) + 0.25f * FMath::PerlinNoise1D(Along * 23.0f - Age * 17.0f + Seed));

		for (int32 SideIndex = 0; SideIndex < OrbitStrandSides; ++SideIndex)
		{
			const float Angle = SideIndex * 2.0f * UE_PI / OrbitStrandSides;
			const float Churn = 1.0f + 0.5f * FMath::PerlinNoise2D(FVector2D(Along * 9.0f - Age * 7.0f + Seed, Angle * 1.3f + Seed));
			const FVector Out = Right * FMath::Cos(Angle) + Up * FMath::Sin(Angle);

			Vertices.Add(Center + Out * Radius * Churn);
			Normals.Add(Out);
			UVs.Add(FVector2D(Angle / (2.0f * UE_PI) * 0.6f, Along * Length - Age * 2.5f));

			// Dark pond water below, bright spell water above, white churned in all along; thin at the very ends
			Colors.Add(FLinearColor(FMath::Pow(Along, 1.2f), 0.25f + 0.5f * Along, 0.0f, 0.95f * FMath::Min(1.0f, Taper * 1.5f)));
		}
	}

	TArray<int32> Triangles;
	for (int32 RingIndex = 0; RingIndex < OrbitStrandRings; ++RingIndex)
	{
		for (int32 SideIndex = 0; SideIndex < OrbitStrandSides; ++SideIndex)
		{
			const int32 A = RingIndex * OrbitStrandSides + SideIndex;
			const int32 B = RingIndex * OrbitStrandSides + (SideIndex + 1) % OrbitStrandSides;
			Triangles.Append({ A, B, A + OrbitStrandSides, B, B + OrbitStrandSides, A + OrbitStrandSides });
		}
	}

	// Tiny bubbles race up inside the water, a few drops spit out where it leaves the pond
	TArray<FVector> DotVertices;
	TArray<FVector> DotNormals;
	TArray<FVector2D> DotUVs;
	TArray<FLinearColor> DotColors;
	TArray<int32> DotTriangles;

	const APlayerController* PlayerController = GetWorld()->GetFirstPlayerController();
	const FVector Camera = PlayerController != nullptr && PlayerController->PlayerCameraManager != nullptr ? PlayerController->PlayerCameraManager->GetCameraLocation() : End + FVector(-600.0f, 0.0f, 900.0f);

	for (int32 DotIndex = 0; DotIndex < OrbitStrandBubbles + OrbitStrandSpits; ++DotIndex)
	{
		const bool bSpit = DotIndex >= OrbitStrandBubbles;
		const float Random = FMath::Frac(FMath::Sin(DotIndex * 12.9898f + Seed) * 43758.5453f);
		const float Random2 = FMath::Frac(Random * 7.31f + 0.17f);

		FVector Center;
		float Size;
		float Alpha = 0.9f;
		if (!bSpit)
		{
			const float Along = FMath::Frac(Random + Age * (0.8f + Random2 * 0.6f));
			if (Along < Tail || Along > Head)
			{
				Alpha = 0.0f;
			}
			const float Angle = Random2 * 2.0f * UE_PI;
			Center = PointAt(Along) + FVector(FMath::Cos(Angle), FMath::Sin(Angle), FMath::Sin(Angle * 2.0f)) * Orb.Size * 100.0f * 0.18f * Random;
			Size = FMath::Lerp(0.5f, 1.4f, Random2);
		}
		else
		{
			const float Life = FMath::Frac(Random + Age * 2.2f);
			const FVector Throw(FMath::Cos(Random2 * 6.28f) * 60.0f, FMath::Sin(Random2 * 6.28f) * 60.0f, 160.0f + 120.0f * Random);
			Center = Start + Throw * Life * 0.5f - FVector(0.0f, 0.0f, 490.0f * FMath::Square(Life * 0.5f));
			Size = FMath::Lerp(0.8f, 2.2f, Random);
			Alpha = Progress < 0.75f ? 0.85f * (1.0f - Life) : 0.0f;
		}

		const FVector ToCamera = (Camera - Center).GetSafeNormal();
		const FVector DotRight = FVector::CrossProduct(FVector::UpVector, ToCamera).GetSafeNormal() * Size * 0.5f;
		const FVector DotUp = FVector::CrossProduct(ToCamera, DotRight).GetSafeNormal() * Size * 0.5f;
		const int32 First = DotVertices.Num();

		DotVertices.Append({ Center - DotRight - DotUp, Center + DotRight - DotUp, Center + DotRight + DotUp, Center - DotRight + DotUp });
		DotUVs.Append({ FVector2D(0.0f, 0.0f), FVector2D(1.0f, 0.0f), FVector2D(1.0f, 1.0f), FVector2D(0.0f, 1.0f) });
		for (int32 Corner = 0; Corner < 4; ++Corner)
		{
			DotNormals.Add(ToCamera);
			DotColors.Add(FLinearColor(1.0f, 0.8f, 1.0f, Alpha));
		}
		DotTriangles.Append({ First, First + 2, First + 1, First, First + 3, First + 2 });
	}

	const TArray<FProcMeshTangent> NoTangents;
	if (Orb.Strand->GetNumSections() == 0)
	{
		Orb.Strand->CreateMeshSection_LinearColor(0, Vertices, Triangles, Normals, UVs, Colors, NoTangents, false);
		Orb.Strand->CreateMeshSection_LinearColor(1, DotVertices, DotTriangles, DotNormals, DotUVs, DotColors, NoTangents, false);
		return;
	}

	Orb.Strand->UpdateMeshSection_LinearColor(0, Vertices, Normals, UVs, Colors, NoTangents);
	Orb.Strand->UpdateMeshSection_LinearColor(1, DotVertices, DotNormals, DotUVs, DotColors, NoTangents);
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

	if (Orb.Strand != nullptr)
	{
		Orb.Strand->DestroyComponent();
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

		if (Orb.Strand != nullptr)
		{
			AnimateStrand(Orb, Time);
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
