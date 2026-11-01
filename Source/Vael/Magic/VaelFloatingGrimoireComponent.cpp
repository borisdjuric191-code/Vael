// Copyright Epic Games, Inc. All Rights Reserved.

#include "Magic/VaelFloatingGrimoireComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Magic/VaelElementComponent.h"
#include "Magic/VaelMagicSettings.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Player/VaelCharacter.h"
#include "ProceduralMeshComponent.h"
#include "VaelAssets.h"

namespace
{
	/** Sizes of the parts in cm, as in Tools/Grimoire/vael_grimoire_bauen.py */
	constexpr float GrimoirePageWidth = 30.0f;
	constexpr float GrimoirePageHeight = 40.0f;
	constexpr float GrimoireSpineWidth = 6.0f;
	constexpr float GrimoireBlockThickness = 2.6f;

	/** Share of the page texture that holds the edge of the pages, at its bottom */
	constexpr float GrimoireEdgeShare = 0.06f;

	/** Pages that can be in the air at once, and the cuts across each one */
	constexpr int32 GrimoireMaxLeaves = 3;
	constexpr int32 GrimoireLeafSegments = 12;

	/** The free edge of a turning page lags behind by this many degrees at the middle of the turn */
	constexpr float GrimoireLeafCurl = 70.0f;

	/** Floating: how high and how fast it bobs, how far it rocks */
	constexpr float GrimoireBobHeight = 3.0f;
	constexpr float GrimoireBobSpeed = 1.7f;
	constexpr float GrimoireRockDegrees = 2.5f;

	/** Degrees it leans per cm/s of the mage, and at most */
	constexpr float GrimoireLeanPerSpeed = 0.022f;
	constexpr float GrimoireLeanMax = 16.0f;
	constexpr float GrimoireLeanSpeed = 6.0f;

	/** Seconds the flash of a cast fades, how fast the glow changes colour */
	constexpr float GrimoireFlashTime = 0.45f;
	constexpr float GrimoireColorSpeed = 7.0f;

	/** Seconds between pages that turn by themselves */
	constexpr float GrimoireIdleTurnMin = 3.5f;
	constexpr float GrimoireIdleTurnMax = 7.0f;

	/** Share of its way from the hip the book has flown before it starts to open */
	constexpr float GrimoireOpenFromFly = 0.65f;

	/** Size of the book while it sits at the hip */
	constexpr float GrimoireHipScale = 0.25f;

	/** How fast it steps aside for the arms of a gesture and comes back */
	constexpr float GrimoireAsideSpeed = 8.0f;

	const FName GrimoireGlowColorParameter(TEXT("Leuchtfarbe"));
	const FName GrimoireGlowParameter(TEXT("Leuchten"));
}

UVaelFloatingGrimoireComponent::UVaelFloatingGrimoireComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = true;

	// The menu pauses the game in the moment the book opens
	PrimaryComponentTick.bTickEvenWhenPaused = true;
	PrimaryComponentTick.TickGroup = TG_PostPhysics;

	// It trails behind the mage instead of hanging on it
	SetUsingAbsoluteLocation(true);
	SetUsingAbsoluteRotation(true);
	SetUsingAbsoluteScale(true);
}

void UVaelFloatingGrimoireComponent::BeginPlay()
{
	Super::BeginPlay();

	Leaves.SetNum(GrimoireMaxLeaves);
	GlowColor = GlowTarget = IdleColor;
	LastRealTime = GetWorld()->GetRealTimeSeconds();

	if (const AVaelCharacter* Mage = Cast<AVaelCharacter>(GetOwner()))
	{
		if (UVaelElementComponent* Elements = Mage->GetElementComponent())
		{
			Elements->OnQueueChanged.AddUObject(this, &UVaelFloatingGrimoireComponent::OnQueueChanged);
			Elements->OnCastFinished.AddUObject(this, &UVaelFloatingGrimoireComponent::OnCastFinished);
		}
	}
}

void UVaelFloatingGrimoireComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (const AVaelCharacter* Mage = Cast<AVaelCharacter>(GetOwner()))
	{
		if (UVaelElementComponent* Elements = Mage->GetElementComponent())
		{
			Elements->OnQueueChanged.RemoveAll(this);
			Elements->OnCastFinished.RemoveAll(this);
		}
	}

	Super::EndPlay(EndPlayReason);
}

void UVaelFloatingGrimoireComponent::NoteCombat()
{
	LastCombatTime = GetWorld()->GetTimeSeconds();
}

void UVaelFloatingGrimoireComponent::NoteDanger(float Distance)
{
	if (Distance <= DangerDistance)
	{
		NoteCombat();
	}
}

bool UVaelFloatingGrimoireComponent::IsWanted() const
{
	const AVaelCharacter* Mage = Cast<AVaelCharacter>(GetOwner());
	if (!bAllowed || Mage == nullptr || Mage->IsDowned())
	{
		return false;
	}

	return bMenuOpen || bForced || GetWorld()->GetTimeSeconds() - LastCombatTime < LingerTime;
}

float UVaelFloatingGrimoireComponent::GetSecondsUntilOpen() const
{
	if (!IsWanted())
	{
		return 0.0f;
	}

	return (1.0f - Fly) * FlyTime * GrimoireOpenFromFly + (1.0f - Open) * OpenTime;
}

FVector UVaelFloatingGrimoireComponent::GetBookCentre() const
{
	return GetComponentLocation();
}

UStaticMeshComponent* UVaelFloatingGrimoireComponent::AddPart(USceneComponent* Parent, UStaticMesh* Mesh)
{
	UStaticMeshComponent* Part = NewObject<UStaticMeshComponent>(GetOwner());
	Part->SetupAttachment(Parent);
	Part->SetStaticMesh(Mesh);
	Part->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Part->bReceivesDecals = false;
	Part->SetMobility(EComponentMobility::Movable);
	Part->RegisterComponent();
	return Part;
}

bool UVaelFloatingGrimoireComponent::BuildBook()
{
	UStaticMesh* Front = VaelAssets::LoadOptional(FrontCoverMesh);
	UStaticMesh* Back = VaelAssets::LoadOptional(BackCoverMesh);
	UStaticMesh* SpineShape = VaelAssets::LoadOptional(SpineMesh);
	UStaticMesh* Pages = VaelAssets::LoadOptional(PagesMesh);
	if (Front == nullptr || Back == nullptr || SpineShape == nullptr || Pages == nullptr)
	{
		return false;
	}

	Body = NewObject<USceneComponent>(GetOwner());
	Body->SetupAttachment(this);
	Body->RegisterComponent();

	// The back cover lies still and the spine turns on it; the front cover hangs on the far end of the spine, each half of the pages on its cover
	Spine = AddPart(Body, SpineShape);
	RightCover = AddPart(Body, Back);
	LeftCover = AddPart(Body, Front);
	RightPages = AddPart(RightCover, Pages);
	LeftPages = AddPart(LeftCover, Pages);

	// The inside of the front cover looks down while the book is closed: its pages are mirrored onto that side
	LeftPages->SetRelativeScale3D(FVector(1.0f, 1.0f, -1.0f));

	UMaterialInterface* Page = VaelAssets::LoadOptional(PageMaterial);
	PageGlow = Page != nullptr ? UMaterialInstanceDynamic::Create(Page, this) : nullptr;
	if (PageGlow != nullptr)
	{
		RightPages->SetMaterial(0, PageGlow);
		LeftPages->SetMaterial(0, PageGlow);
	}

	LeafMesh = NewObject<UProceduralMeshComponent>(GetOwner());
	LeafMesh->SetupAttachment(Body);
	LeafMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	LeafMesh->bReceivesDecals = false;
	LeafMesh->RegisterComponent();

	PageLight = NewObject<UPointLightComponent>(GetOwner());
	PageLight->SetupAttachment(this);
	PageLight->SetMobility(EComponentMobility::Movable);
	PageLight->SetIntensityUnits(ELightUnits::Candelas);
	PageLight->SetAttenuationRadius(320.0f);
	PageLight->SetSourceRadius(10.0f);
	PageLight->SetCastShadows(false);
	// High enough to light the mage and the ground, not to wash out its own pages
	PageLight->SetRelativeLocation(FVector(0.0f, 0.0f, 70.0f));
	PageLight->RegisterComponent();

	return true;
}

void UVaelFloatingGrimoireComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	// The delta of a paused world can't be trusted, the clock of the world goes on
	const double Now = GetWorld()->GetRealTimeSeconds();
	const float DeltaSeconds = FMath::Min(static_cast<float>(Now - LastRealTime), 0.1f);
	LastRealTime = Now;
	RealTime += DeltaSeconds;

	const bool bWanted = IsWanted();
	if (!bBuilt)
	{
		if (!bWanted)
		{
			return;
		}

		bBuilt = BuildBook();
		if (!bBuilt)
		{
			bAllowed = false;
			return;
		}
	}

	// Out of the hip and then open; closed first and only then back
	const float WasFly = Fly;
	if (bWanted)
	{
		Fly = FMath::Min(Fly + DeltaSeconds / FlyTime, 1.0f);
		if (Fly >= GrimoireOpenFromFly)
		{
			Open = FMath::Min(Open + DeltaSeconds / OpenTime, 1.0f);
		}
	}
	else
	{
		Open = FMath::Max(Open - DeltaSeconds / OpenTime, 0.0f);
		if (Open <= 0.0f)
		{
			Fly = FMath::Max(Fly - DeltaSeconds / FlyTime, 0.0f);
		}
	}

	if (Fly <= 0.0f)
	{
		if (WasFly > 0.0f || Body->IsVisible())
		{
			SetVisibility(false, true);
		}
		return;
	}

	if (WasFly <= 0.0f)
	{
		// It starts at the hip wherever the mage stands now, not where it was put away
		const AActor* Mage = GetOwner();
		FollowLocation = Mage->GetActorLocation() + Mage->GetActorRotation().RotateVector(HipOffset);
		FollowYaw = Mage->GetActorRotation().Yaw;
		SetVisibility(true, true);
	}

	FollowMage(DeltaSeconds, RealTime);
	PoseBook(FMath::SmoothStep(0.0f, 1.0f, Open));

	// Pages turn by themselves now and then, a draught that only a grimoire feels
	if (Open >= 1.0f)
	{
		IdleTurnCountdown -= DeltaSeconds;
		if (IdleTurnCountdown <= 0.0f)
		{
			IdleTurnCountdown = FMath::FRandRange(GrimoireIdleTurnMin, GrimoireIdleTurnMax);
			TurnPage();
		}
	}
	AnimateLeaves(DeltaSeconds);

	// Glow: the colour of the newest element, a flash at a cast, none while closed
	GlowColor = FMath::CInterpTo(GlowColor, GlowTarget, DeltaSeconds, GrimoireColorSpeed);
	Flash = FMath::Max(Flash - DeltaSeconds / GrimoireFlashTime, 0.0f);
	const float Pulse = 1.0f + 0.12f * FMath::Sin(RealTime * 2.3f);
	const float Glow = FMath::Lerp(RestGlow * Pulse, CastGlow, Flash * Flash) * FMath::Square(Open);
	if (PageGlow != nullptr)
	{
		PageGlow->SetVectorParameterValue(GrimoireGlowColorParameter, GlowColor);
		PageGlow->SetScalarParameterValue(GrimoireGlowParameter, Glow);
	}
	PageLight->SetLightColor(GlowColor);
	PageLight->SetIntensity(LightIntensity * Glow / FMath::Max(RestGlow, 0.01f));
}

void UVaelFloatingGrimoireComponent::FollowMage(float DeltaSeconds, float Time)
{
	const AActor* Mage = GetOwner();
	const FRotator Facing(0.0f, Mage->GetActorRotation().Yaw, 0.0f);

	const float Along = FMath::SmoothStep(0.0f, 1.0f, Fly);
	const AVaelCharacter* Caster = Cast<AVaelCharacter>(Mage);
	Aside = FMath::FInterpTo(Aside, Caster != nullptr && Caster->IsCastStance() ? 1.0f : 0.0f, DeltaSeconds, GrimoireAsideSpeed);
	const FVector Target = Mage->GetActorLocation() + Facing.RotateVector(FMath::Lerp(HipOffset, FMath::Lerp(HoverOffset, AsideOffset, Aside), Along));
	FollowLocation = FMath::VInterpTo(FollowLocation, Target, DeltaSeconds, FollowSpeed);
	FollowYaw = FMath::FInterpTo(FollowYaw, FollowYaw + FMath::FindDeltaAngleDegrees(FollowYaw, Facing.Yaw), DeltaSeconds, TurnSpeed);

	// While the mage runs it leans back against the wind and out of turns; paused, it comes to rest
	const FVector Local = FRotator(0.0f, FollowYaw, 0.0f).UnrotateVector(GetWorld()->IsPaused() ? FVector::ZeroVector : Mage->GetVelocity());
	const FVector2D LeanTarget(
		FMath::Clamp(Local.X * GrimoireLeanPerSpeed, -GrimoireLeanMax, GrimoireLeanMax),
		FMath::Clamp(Local.Y * GrimoireLeanPerSpeed, -GrimoireLeanMax, GrimoireLeanMax));
	Lean = FMath::Vector2DInterpTo(Lean, LeanTarget, DeltaSeconds, GrimoireLeanSpeed);

	const float Bob = FMath::Sin(Time * GrimoireBobSpeed * 2.0f) * GrimoireBobHeight * Along;
	const float RockPitch = FMath::Sin(Time * GrimoireBobSpeed * 1.3f) * GrimoireRockDegrees * Along;
	const float RockRoll = FMath::Sin(Time * GrimoireBobSpeed * 0.9f + 1.0f) * GrimoireRockDegrees * Along;

	// The book lies with its spine towards the mage's forward line: turned a quarter so X of the book runs to the mage's right
	const FQuat Heading = FRotator(0.0f, FollowYaw, 0.0f).Quaternion();
	const FQuat Tilt = FRotator((TiltDegrees * Along + RockPitch) + Lean.X, 0.0f, RockRoll + Lean.Y).Quaternion();
	const FQuat Across = FRotator(0.0f, 90.0f, 0.0f).Quaternion();

	SetWorldLocationAndRotation(FollowLocation + FVector(0.0f, 0.0f, Bob), Heading * Tilt * Across);
}

void UVaelFloatingGrimoireComponent::PoseBook(float Openness)
{
	const float Bend = OpenBendDegrees * Openness;

	// The spine stands upright while closed and lies flat while open
	const float SpineAngle = 90.0f * (1.0f - Openness);
	Spine->SetRelativeRotation(FRotator(-SpineAngle, 0.0f, 0.0f));
	RightCover->SetRelativeRotation(FRotator(Bend, 0.0f, 0.0f));

	const float SpineRadians = FMath::DegreesToRadians(SpineAngle);
	LeftCover->SetRelativeLocation(FVector(-GrimoireSpineWidth * FMath::Cos(SpineRadians), 0.0f, GrimoireSpineWidth * FMath::Sin(SpineRadians)));
	LeftCover->SetRelativeRotation(FRotator(Openness * (180.0f - OpenBendDegrees), 0.0f, 0.0f));

	// Open, the pages close over the spine and meet in the gutter
	const float Overhang = -GrimoireSpineWidth * 0.5f * Openness;
	RightPages->SetRelativeLocation(FVector(Overhang, 0.0f, 0.0f));
	LeftPages->SetRelativeLocation(FVector(Overhang, 0.0f, 0.0f));

	// Whether open or closed, the middle of the book stays where the mage expects it
	const float Scale = BookScale * FMath::Lerp(GrimoireHipScale, 1.0f, FMath::SmoothStep(0.0f, 1.0f, Fly));
	const float Middle = FMath::Lerp(GrimoirePageWidth * 0.5f, -GrimoireSpineWidth * 0.5f, Openness);
	Body->SetRelativeLocation(FVector(-Middle * Scale, 0.0f, 0.0f));
	Body->SetRelativeScale3D(FVector(Scale));
}

void UVaelFloatingGrimoireComponent::TurnPage(float Delay)
{
	for (FVaelGrimoireLeaf& Leaf : Leaves)
	{
		if (!Leaf.bActive)
		{
			Leaf.bActive = true;
			Leaf.Progress = -Delay / PageTurnTime;
			return;
		}
	}
}

void UVaelFloatingGrimoireComponent::AnimateLeaves(float DeltaSeconds)
{
	const float Bottom = -GrimoirePageHeight * 0.5f;
	const float Top = GrimoirePageHeight * 0.5f;
	const float Segment = (GrimoirePageWidth + GrimoireSpineWidth * 0.5f) / GrimoireLeafSegments;

	TArray<int32> Triangles;
	for (int32 Index = 0; Index < GrimoireLeafSegments; ++Index)
	{
		const int32 A = Index * 2;
		Triangles.Append({ A, A + 1, A + 2, A + 1, A + 3, A + 2 });
	}

	for (int32 LeafIndex = 0; LeafIndex < Leaves.Num(); ++LeafIndex)
	{
		FVaelGrimoireLeaf& Leaf = Leaves[LeafIndex];
		if (!Leaf.bActive)
		{
			continue;
		}

		Leaf.Progress += DeltaSeconds / PageTurnTime;
		if (Leaf.Progress >= 1.0f)
		{
			Leaf.bActive = false;
			LeafMesh->SetMeshSectionVisible(LeafIndex, false);
			continue;
		}

		const float Progress = FMath::Max(Leaf.Progress, 0.0f);
		const float Eased = FMath::SmoothStep(0.0f, 1.0f, Progress);
		const float Base = FMath::Lerp(OpenBendDegrees, 180.0f - OpenBendDegrees, Eased);
		const float Curl = GrimoireLeafCurl * FMath::Sin(Progress * UE_PI);

		TArray<FVector> Vertices;
		TArray<FVector> Normals;
		TArray<FVector2D> UVs;
		Vertices.Reserve((GrimoireLeafSegments + 1) * 2);
		Normals.Reserve((GrimoireLeafSegments + 1) * 2);
		UVs.Reserve((GrimoireLeafSegments + 1) * 2);

		// From the gutter outwards: the page bends, its free edge lagging behind
		FVector2D Point(-GrimoireSpineWidth * 0.5f, GrimoireBlockThickness);
		for (int32 Index = 0; Index <= GrimoireLeafSegments; ++Index)
		{
			const float Share = Index / static_cast<float>(GrimoireLeafSegments);
			const float Radians = FMath::DegreesToRadians(Base + Curl * (0.35f - Share));
			const FVector Normal(-FMath::Sin(Radians), 0.0f, FMath::Cos(Radians));

			Vertices.Add(FVector(Point.X, Bottom, Point.Y));
			Vertices.Add(FVector(Point.X, Top, Point.Y));
			Normals.Add(Normal);
			Normals.Add(Normal);
			UVs.Add(FVector2D(Share, 1.0f - GrimoireEdgeShare));
			UVs.Add(FVector2D(Share, 0.0f));

			Point += FVector2D(FMath::Cos(Radians), FMath::Sin(Radians)) * Segment;
		}

		if (LeafMesh->GetProcMeshSection(LeafIndex) == nullptr || LeafMesh->GetProcMeshSection(LeafIndex)->ProcVertexBuffer.IsEmpty())
		{
			LeafMesh->CreateMeshSection_LinearColor(LeafIndex, Vertices, Triangles, Normals, UVs, TArray<FLinearColor>(), TArray<FProcMeshTangent>(), false);
			if (PageGlow != nullptr)
			{
				LeafMesh->SetMaterial(LeafIndex, PageGlow);
			}
		}
		else
		{
			LeafMesh->UpdateMeshSection_LinearColor(LeafIndex, Vertices, Normals, UVs, TArray<FLinearColor>(), TArray<FProcMeshTangent>());
		}
		LeafMesh->SetMeshSectionVisible(LeafIndex, Leaf.Progress >= 0.0f);
	}
}

void UVaelFloatingGrimoireComponent::OnQueueChanged()
{
	const AVaelCharacter* Mage = Cast<AVaelCharacter>(GetOwner());
	const UVaelElementComponent* Elements = Mage != nullptr ? Mage->GetElementComponent() : nullptr;
	if (Elements == nullptr)
	{
		return;
	}

	const TArray<EVaelElement>& Queue = Elements->GetQueue();
	if (Queue.Num() > LastQueueSize)
	{
		// A new element: the book turns to it
		NoteCombat();
		if (bBuilt && Open >= 1.0f)
		{
			TurnPage();
		}
	}
	LastQueueSize = Queue.Num();

	GlowTarget = Queue.IsEmpty() ? IdleColor : UVaelMagicSettings::Get()->GetElementColor(Queue.Last());
}

void UVaelFloatingGrimoireComponent::OnCastFinished(EVaelCastResult Result, const UVaelFormula* Formula)
{
	if (Result != EVaelCastResult::Success)
	{
		return;
	}

	NoteCombat();
	Flash = 1.0f;

	// The power runs through the book: a few pages riffle over
	if (bBuilt && Open >= 1.0f)
	{
		TurnPage();
		TurnPage(0.09f);
		TurnPage(0.18f);
	}
}
