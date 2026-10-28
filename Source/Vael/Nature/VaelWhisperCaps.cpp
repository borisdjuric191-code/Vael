// Copyright Epic Games, Inc. All Rights Reserved.

#include "Nature/VaelWhisperCaps.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/SceneComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "World/VaelTimeOfDay.h"

namespace
{
	/** Diameter of a moss cushion at a foot and in the middle of the ring, in cm */
	constexpr float FootMossSize = 75.0f;
	constexpr float MiddleMossSize = 120.0f;

	/** Traces start this far above and end this far below the ring, in cm */
	constexpr float TraceUp = 300.0f;
	constexpr float TraceDown = 600.0f;

	/** No player within this distance (cm): the ring only follows the clock, slowly */
	constexpr float AwakeRange = 4500.0f;

	/** Glow of a staring cap (custom data 0) while they are silent */
	constexpr float StareGlow = 0.45f;
}

AVaelWhisperCaps::AVaelWhisperCaps()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickInterval = 0.05f;

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent = Root;

	Caps = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("Caps"));
	Caps->SetupAttachment(Root);
	Caps->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Caps->SetGenerateOverlapEvents(false);
	Caps->NumCustomDataFloats = 2;

	Moss = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("Moss"));
	Moss->SetupAttachment(Root);
	Moss->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Moss->SetGenerateOverlapEvents(false);
	Moss->SetCastShadow(false);

	Glow = CreateDefaultSubobject<UPointLightComponent>(TEXT("Glow"));
	Glow->SetupAttachment(Root);
	// Low, so it pools blue on the ground and the stems instead of washing out the faces
	Glow->SetRelativeLocation(FVector(0.0f, 0.0f, 35.0f));
	Glow->SetIntensityUnits(ELightUnits::Candelas);
	Glow->SetCastShadows(false);
	Glow->SetVolumetricScatteringIntensity(4.0f);
}

void AVaelWhisperCaps::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	BuildRing();
}

void AVaelWhisperCaps::BuildRing()
{
	Glow->SetLightColor(LightColor);
	Glow->SetAttenuationRadius(LightRadius);
	Glow->SetIntensity(LightIntensity.X);

	Caps->ClearInstances();
	Moss->ClearInstances();
	Caps->SetStaticMesh(CapMesh);
	if (CapMaterial != nullptr)
	{
		Caps->SetMaterial(0, CapMaterial);
	}
	if (CapMesh == nullptr)
	{
		return;
	}

	FRandomStream Random(Seed);
	UWorld* World = GetWorld();
	FCollisionQueryParams Query(SCENE_QUERY_STAT(VaelWhisperCaps), true, this);
	const FVector Middle = GetActorLocation();

	// Where the ground is below a spot of the ring (world), or the actor height where nothing is found
	auto GroundAt = [&](const FVector2D& Spot) -> float
	{
		FHitResult Hit;
		const FVector Start(Spot.X, Spot.Y, Middle.Z + TraceUp);
		const FVector End(Spot.X, Spot.Y, Middle.Z - TraceDown);
		if (World != nullptr && World->LineTraceSingleByChannel(Hit, Start, End, ECC_Visibility, Query))
		{
			return Hit.ImpactPoint.Z;
		}
		return Middle.Z;
	};

	const FTransform ToLocal = GetActorTransform().Inverse();
	TArray<FVector> Feet;
	for (int32 Index = 0; Index < Count; ++Index)
	{
		const float Angle = (Index + Random.FRandRange(-0.25f, 0.25f)) * UE_TWO_PI / Count;
		const float Reach = RingRadius * Random.FRandRange(0.7f, 1.15f);
		const FVector Out(FMath::Cos(Angle), FMath::Sin(Angle), 0.0f);
		const FVector2D Spot(Middle.X + Out.X * Reach, Middle.Y + Out.Y * Reach);
		const FVector Foot(Spot.X, Spot.Y, GroundAt(Spot) - Sink);

		// Face to the middle, a little off; lean in, so the hoods bow over the ring
		const float FaceToMiddle = FMath::RadiansToDegrees(Angle) + 180.0f + Random.FRandRange(-TurnJitter, TurnJitter);
		const FQuat Yaw(FVector::UpVector, FMath::DegreesToRadians(FaceToMiddle - FaceYaw));
		const FQuat Lean(FVector::CrossProduct(FVector::UpVector, -Out).GetSafeNormal(),
		                 FMath::DegreesToRadians(LeanIn * Random.FRandRange(0.5f, 1.3f)));
		const float Scale = Random.FRandRange(HeightRange.X, HeightRange.Y) / MeshHeight;
		const float Wide = Scale * Random.FRandRange(0.9f, 1.1f);

		const FTransform WorldTransform((Lean * Yaw).GetNormalized(), Foot, FVector(Wide, Wide, Scale));
		Caps->AddInstance(WorldTransform * ToLocal);
		Feet.Add(Foot);
	}
	for (int32 Index = 0; Index < Caps->GetInstanceCount(); ++Index)
	{
		Caps->SetCustomDataValue(Index, 1, Random.FRand(), false);
	}

	// Moss at every foot and a couple of cushions in the middle
	if (MossMeshes.Num() > 0)
	{
		UStaticMesh* MossMesh = MossMeshes[Random.RandRange(0, MossMeshes.Num() - 1)];
		Moss->SetStaticMesh(MossMesh);
		if (MossMesh != nullptr)
		{
			const FBox Bounds = MossMesh->GetBoundingBox();
			const float Across = FMath::Max3<float>(Bounds.GetSize().X, Bounds.GetSize().Y, 1.0f);
			auto AddMoss = [&](const FVector& At, float Size)
			{
				const float Scale = Size / Across * Random.FRandRange(0.8f, 1.2f);
				const FTransform WorldTransform(FRotator(0.0f, Random.FRandRange(0.0f, 360.0f), 0.0f), At + FVector(0.0f, 0.0f, Sink * 0.5f),
				                                FVector(Scale, Scale, Scale * 0.6f));
				Moss->AddInstance(WorldTransform * ToLocal);
			};
			for (const FVector& Foot : Feet)
			{
				AddMoss(Foot, FootMossSize);
			}
			for (int32 Index = 0; Index < 2; ++Index)
			{
				const FVector2D Spot(Middle.X + Random.FRandRange(-0.3f, 0.3f) * RingRadius, Middle.Y + Random.FRandRange(-0.3f, 0.3f) * RingRadius);
				AddMoss(FVector(Spot.X, Spot.Y, GroundAt(Spot) - Sink), MiddleMossSize);
			}
		}
	}
	Caps->MarkRenderStateDirty();
}

void AVaelWhisperCaps::BeginPlay()
{
	Super::BeginPlay();

	CapMaterialInstance = Caps->CreateDynamicMaterialInstance(0);
	const int32 Total = Caps->GetInstanceCount();
	BaseTransforms.SetNum(Total);
	for (int32 Index = 0; Index < Total; ++Index)
	{
		Caps->GetInstanceTransform(Index, BaseTransforms[Index], false);
	}
	Turn.Init(0.0f, Total);
	Whisper.Init(0.0f, Total);
	Pause = FMath::FRandRange(PauseRange.X, PauseRange.Y);
}

APawn* AVaelWhisperCaps::FindNearestPlayer(float& OutDistance) const
{
	APawn* Nearest = nullptr;
	OutDistance = TNumericLimits<float>::Max();
	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		APawn* Pawn = It->Get() != nullptr ? It->Get()->GetPawn() : nullptr;
		if (Pawn == nullptr)
		{
			continue;
		}
		const float Distance = FVector::Dist2D(Pawn->GetActorLocation(), GetActorLocation());
		if (Distance < OutDistance)
		{
			OutDistance = Distance;
			Nearest = Pawn;
		}
	}
	return Nearest;
}

void AVaelWhisperCaps::NextSpeaker(float Hurry)
{
	const int32 Total = BaseTransforms.Num();
	if (Total == 0)
	{
		return;
	}
	// Mostly the neighbour takes over, so the whisper runs round the ring; now and then someone across answers
	const int32 Step = FMath::FRand() < 0.7f ? 1 : FMath::RandRange(2, FMath::Max(2, Total - 1));
	Speaker = Speaker == INDEX_NONE ? FMath::RandRange(0, Total - 1) : (Speaker + Step) % Total;
	SpeakTime = 0.0f;
	Pause = FMath::FRandRange(PauseRange.X, PauseRange.Y) / Hurry;
}

void AVaelWhisperCaps::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	// Night: stronger glow and light (the caps stay well visible by day)
	const float Night = UVaelTimeOfDay::NightFactorOf(this);
	if (FMath::Abs(Night - LastNight) > 0.01f)
	{
		LastNight = Night;
		if (CapMaterialInstance != nullptr)
		{
			CapMaterialInstance->SetScalarParameterValue(TEXT("Nacht"), Night);
		}
	}

	const int32 Total = BaseTransforms.Num();
	float Distance = 0.0f;
	const APawn* Player = FindNearestPlayer(Distance);
	const bool bAwake = Player != nullptr && Distance < AwakeRange;
	SetActorTickInterval(bAwake ? 0.05f : 1.0f);
	if (!bAwake || Total == 0)
	{
		Glow->SetIntensity(FMath::Lerp(LightIntensity.X, LightIntensity.Y, Night));
		return;
	}

	// Close: silent and staring; near: whispering faster; far: calm
	const bool bNoticed = Distance < NoticeRange;
	const float Hurry = bNoticed ? FMath::Lerp(2.2f, 1.0f, FMath::Clamp((Distance - SilenceRange) / FMath::Max(NoticeRange - SilenceRange, 1.0f), 0.0f, 1.0f)) : 1.0f;
	Silence = FMath::FInterpConstantTo(Silence, Distance < SilenceRange ? 1.0f : 0.0f, DeltaSeconds, 1.5f);

	if (Speaker == INDEX_NONE || SpeakTime > WhisperTime / Hurry)
	{
		Pause -= DeltaSeconds;
		if (Pause <= 0.0f && Silence < 0.5f)
		{
			NextSpeaker(Hurry);
		}
		else if (Speaker != INDEX_NONE && SpeakTime > WhisperTime / Hurry)
		{
			Speaker = INDEX_NONE;
		}
	}

	TArray<FTransform> Transforms;
	Transforms.SetNum(Total);
	float Spoken = 0.0f;
	for (int32 Index = 0; Index < Total; ++Index)
	{
		// The speaker's mouth swells and falls with a quick flutter, like lips forming words
		float Target = 0.0f;
		if (Index == Speaker)
		{
			const float Share = FMath::Clamp(SpeakTime * Hurry / WhisperTime, 0.0f, 1.0f);
			Target = FMath::Sin(Share * UE_PI) * (0.7f + 0.3f * FMath::Sin(SpeakTime * 23.0f + Index));
		}
		Whisper[Index] = FMath::FInterpTo(Whisper[Index], Target, DeltaSeconds, 10.0f);
		const float Shown = FMath::Lerp(Whisper[Index], StareGlow, Silence);
		Caps->SetCustomDataValue(Index, 0, Shown, false);
		Spoken += Shown;

		// Turn the face after the player, as far as the cap can bend
		const FTransform& Base = BaseTransforms[Index];
		float Wanted = 0.0f;
		if (bNoticed)
		{
			const FVector Foot = GetActorTransform().TransformPosition(Base.GetLocation());
			const FVector Face = GetActorTransform().TransformVectorNoScale(Base.GetRotation().RotateVector(
				FRotator(0.0f, FaceYaw, 0.0f).Vector()));
			const float FaceYawWorld = FMath::RadiansToDegrees(FMath::Atan2(Face.Y, Face.X));
			const FVector ToPlayer = Player->GetActorLocation() - Foot;
			const float PlayerYaw = FMath::RadiansToDegrees(FMath::Atan2(ToPlayer.Y, ToPlayer.X));
			Wanted = FMath::Clamp(FMath::FindDeltaAngleDegrees(FaceYawWorld, PlayerYaw), -MaxTurn, MaxTurn);
		}
		Turn[Index] = FMath::FInterpConstantTo(Turn[Index], Wanted, DeltaSeconds, TurnSpeed);
		Transforms[Index] = Base;
		Transforms[Index].SetRotation(FQuat(FVector::UpVector, FMath::DegreesToRadians(Turn[Index])) * Base.GetRotation());
	}
	SpeakTime += DeltaSeconds;
	Caps->BatchUpdateInstancesTransforms(0, Transforms, false, true, false);

	Glow->SetIntensity(FMath::Lerp(LightIntensity.X, LightIntensity.Y, Night) * (1.0f + 0.35f * Spoken / Total));
}
