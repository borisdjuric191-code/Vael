// Copyright Epic Games, Inc. All Rights Reserved.

#include "Magic/VaelFireRoller.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "Magic/VaelDebrisBurst.h"
#include "Magic/VaelFlakes.h"
#include "Magic/VaelGroundArea.h"
#include "Magic/VaelMagicSettings.h"
#include "Magic/VaelSpellEffects.h"
#include "Magic/VaelWindStreaks.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "VaelAssets.h"
#include "World/VaelFireComponent.h"
#include "World/VaelFireSpot.h"
#include "World/VaelGround.h"

namespace
{
	/** Radius of the drum against its width, and how much it grows from its start to its end */
	constexpr float DrumRadiusShare = 0.17f;
	constexpr float GrowFrom = 0.75f;
	constexpr float GrowTo = 1.3f;
	constexpr float GrowSeconds = 1.3f;

	/** Tongues: rings across the drum and tongues round each, their length in cm; they lick outwards and trail back and up */
	constexpr int32 TongueRings = 11;
	constexpr int32 TonguesPerRing = 10;
	constexpr int32 InnerTongues = 4;
	constexpr float TongueLength = 80.0f;
	constexpr float TongueTrail = 0.55f;
	constexpr float TongueRise = 0.35f;

	/** Size of a tongue mesh in cm */
	constexpr float TongueMeshSize = 100.0f;

	/** Streaks of wind pushing from behind, how often each starts over, their width and trail */
	constexpr int32 WindStreaks = 10;
	constexpr float StreakRestart = 0.4f;
	constexpr float StreakWidth = 5.0f;
	constexpr float StreakTrail = 160.0f;

	/** Embers flung ahead per second */
	constexpr float RollerEmberRate = 45.0f;

	/** The burning trail: a row every so many cm, two patches across it, their seconds and damage per second */
	constexpr float TrailRowSpacing = 160.0f;
	constexpr float TrailSeconds = 3.5f;
	constexpr float TrailDamagePerSecond = 10.0f;

	/** Seconds the flames take to sink after the projectile is gone, and the embers to fade */
	constexpr float SinkSeconds = 0.4f;
	constexpr float FadeSeconds = 1.6f;

	constexpr float LightIntensity = 6000.0f;
	const FLinearColor FireColor = FLinearColor(1.0f, 0.42f, 0.1f);
	const FName StrengthParameter(TEXT("Staerke"));
}

AVaelFireRoller::AVaelFireRoller()
{
	PrimaryActorTick.bCanEverTick = true;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));

	Light = CreateDefaultSubobject<UPointLightComponent>(TEXT("Light"));
	Light->SetupAttachment(RootComponent);
	Light->SetCastShadows(false);
	Light->SetLightColor(FireColor);
	Light->SetIntensity(0.0f);
}

AVaelFireRoller* AVaelFireRoller::Spawn(AActor* InCarrier, float InWidth, APawn* InInstigator)
{
	UWorld* World = InCarrier != nullptr ? InCarrier->GetWorld() : nullptr;
	if (World == nullptr)
	{
		return nullptr;
	}
	FActorSpawnParameters Parameters;
	Parameters.Instigator = InInstigator;
	Parameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AVaelFireRoller* Roller = World->SpawnActor<AVaelFireRoller>(AVaelFireRoller::StaticClass(), FTransform(InCarrier->GetActorLocation()), Parameters);
	if (Roller != nullptr)
	{
		Roller->Carrier = InCarrier;
		Roller->Width = InWidth;
		Roller->LastCarrier = InCarrier->GetActorLocation();
		const FVector Flight = InCarrier->GetVelocity().GetSafeNormal2D();
		Roller->Forward = Flight.IsNearlyZero() ? InCarrier->GetActorForwardVector().GetSafeNormal2D() : Flight;
		Roller->Build();
	}
	return Roller;
}

void AVaelFireRoller::Build()
{
	const UVaelFireSettings* FireSettings = UVaelFireSettings::Get();
	Light->SetAttenuationRadius(Width * 1.5f);

	// The tongues of the fire kit, one instanced mesh per tongue shape
	if (UMaterialInterface* Base = VaelAssets::LoadOptional(FireSettings->TongueMaterial))
	{
		TongueMaterial = UMaterialInstanceDynamic::Create(Base, this);
		TongueMaterial->SetScalarParameterValue(StrengthParameter, 1.0f);
		for (const TSoftObjectPtr<UStaticMesh>& MeshAsset : FireSettings->TongueMeshes)
		{
			if (UStaticMesh* Mesh = VaelAssets::LoadOptional(MeshAsset))
			{
				UInstancedStaticMeshComponent* Instances = NewObject<UInstancedStaticMeshComponent>(this);
				Instances->SetupAttachment(RootComponent);
				Instances->SetUsingAbsoluteLocation(true);
				Instances->SetUsingAbsoluteRotation(true);
				Instances->SetUsingAbsoluteScale(true);
				Instances->SetWorldTransform(FTransform::Identity);
				Instances->SetStaticMesh(Mesh);
				Instances->SetMaterial(0, TongueMaterial);
				Instances->SetCollisionEnabled(ECollisionEnabled::NoCollision);
				Instances->SetCastShadow(false);
				Instances->SetCanEverAffectNavigation(false);
				Instances->NumCustomDataFloats = 3;
				Instances->RegisterComponent();
				TongueMeshes.Add(Instances);
			}
		}
	}

	// Rings of tongues across the drum, shorter towards its ends, every one at another moment of its life; short ones close
	// round the axis fill the drum so it reads as a solid roll of fire
	for (int32 Ring = 0; Ring < TongueRings; ++Ring)
	{
		const float Across = Ring / (TongueRings - 1.0f) - 0.5f;
		for (int32 Index = 0; Index < TonguesPerRing + InnerTongues; ++Index)
		{
			const bool bInner = Index >= TonguesPerRing;
			FTongue& Tongue = Tongues.AddDefaulted_GetRef();
			Tongue.Around = (Index + FMath::FRandRange(-0.3f, 0.3f) + Ring * 0.5f) * UE_TWO_PI / (bInner ? InnerTongues : TonguesPerRing);
			Tongue.Across = Across + FMath::FRandRange(-0.03f, 0.03f);
			Tongue.Depth = bInner ? 0.1f : 0.55f;
			Tongue.Length = (1.0f - 0.45f * FMath::Abs(Across) * 2.0f) * FMath::FRandRange(0.75f, 1.1f) * (bInner ? 0.6f : 1.0f);
			Tongue.Life = FMath::FRandRange(FireSettings->TongueLifeMin, FireSettings->TongueLifeMax);
			Tongue.Age = FMath::FRandRange(0.0f, Tongue.Life);
			Tongue.Yaw = FMath::FRandRange(0.0f, 360.0f);
			Tongue.Mesh = TongueMeshes.IsEmpty() ? 0 : FMath::RandRange(0, TongueMeshes.Num() - 1);
		}
	}
	for (const FTongue& Tongue : Tongues)
	{
		if (TongueMeshes.IsValidIndex(Tongue.Mesh))
		{
			TongueMeshes[Tongue.Mesh]->AddInstance(FTransform(FQuat::Identity, GetActorLocation(), FVector::ZeroVector), true);
		}
	}

	if (UMaterialInterface* StreakMaterial = VaelAssets::LoadOptional(UVaelMagicSettings::Get()->StreakMaterial))
	{
		Streaks = NewObject<UVaelWindStreaksComponent>(this);
		Streaks->SetupAttachment(RootComponent);
		Streaks->RegisterComponent();
		Streaks->Init(StreakMaterial);
		for (int32 Index = 0; Index < WindStreaks; ++Index)
		{
			Streaks->AddStreak(GetActorLocation(), StreakWidth, StreakTrail);
		}
	}

	Embers = NewObject<UVaelFlakesComponent>(this);
	Embers->SetupAttachment(RootComponent);
	Embers->RegisterComponent();
}

void AVaelFireRoller::UpdateDrum(float DeltaSeconds)
{
	const UVaelFireSettings* FireSettings = UVaelFireSettings::Get();
	const float Grow = FMath::Lerp(GrowFrom, GrowTo, FMath::Clamp(Age / GrowSeconds, 0.0f, 1.0f));
	const float Strength = Collapsed < 0.0f ? FMath::Clamp(Age / 0.12f, 0.0f, 1.0f) : FMath::Clamp(1.0f - Collapsed / SinkSeconds, 0.0f, 1.0f);
	const float Radius = Width * DrumRadiusShare * Grow;
	const FVector Side(-Forward.Y, Forward.X, 0.0f);

	Light->SetWorldLocation(Axis);
	Light->SetIntensity(LightIntensity * Strength * Grow * (0.85f + 0.15f * FMath::PerlinNoise1D(Age * 9.0f)));

	if (TongueMeshes.IsEmpty())
	{
		return;
	}

	TArray<TArray<FTransform>> Placed;
	TArray<TArray<float>> Data;
	Placed.SetNum(TongueMeshes.Num());
	Data.SetNum(TongueMeshes.Num());
	for (FTongue& Tongue : Tongues)
	{
		// One life after another, as the tongues of every fire
		Tongue.Age += DeltaSeconds;
		if (Tongue.Age >= Tongue.Life)
		{
			Tongue.Age -= Tongue.Life;
			Tongue.Life = FMath::FRandRange(FireSettings->TongueLifeMin, FireSettings->TongueLifeMax);
			Tongue.Yaw = FMath::FRandRange(0.0f, 360.0f);
		}
		const float Life = Tongue.Age / Tongue.Life;
		const float Grown = FMath::SmoothStep(0.0f, 0.22f, Life);
		const float Leaving = FMath::SmoothStep(0.62f, 1.0f, Life);
		const float Fade = 1.0f - FMath::SmoothStep(0.8f, 1.0f, Life);

		// Round the drum as it rolls (the top going forward); the tongues lick out and are dragged back and up by the run,
		// those under it are hidden in the ground
		const float Angle = Tongue.Around + Roll;
		const FVector Radial = FVector::UpVector * FMath::Cos(Angle) + Forward * FMath::Sin(Angle);
		const float Shown = FMath::SmoothStep(-0.55f, 0.1f, static_cast<float>(Radial.Z));
		const FVector Up = (Radial - Forward * TongueTrail + FVector::UpVector * TongueRise).GetSafeNormal();
		const float Height = FMath::Max(TongueLength * Tongue.Length * Grow * Grown * (1.0f + 0.35f * Leaving) * Strength, 0.1f);
		const float Scale = Height / TongueMeshSize;
		const float Thin = FireSettings->TongueWidth * (0.75f + 0.25f * Grown) * (1.0f - 0.45f * Leaving);
		const FVector Foot = Axis + Side * Tongue.Across * Width * 0.95f + Radial * Radius * Tongue.Depth + Up * Height * 0.4f * Leaving;
		const FQuat Turn = FRotationMatrix::MakeFromZ(Up).ToQuat() * FQuat(FVector::UpVector, FMath::DegreesToRadians(Tongue.Yaw));
		Placed[Tongue.Mesh].Add(FTransform(Turn, Foot, FVector(Scale * Thin, Scale * Thin, Scale)));
		Data[Tongue.Mesh].Append({ Fade * Shown, Life, Scale });
	}
	for (int32 Index = 0; Index < TongueMeshes.Num(); ++Index)
	{
		if (Placed[Index].IsEmpty())
		{
			continue;
		}
		TongueMeshes[Index]->BatchUpdateInstancesTransforms(0, Placed[Index], true, false, true);
		for (int32 Instance = 0; Instance < Placed[Index].Num(); ++Instance)
		{
			TongueMeshes[Index]->SetCustomData(Instance, TArrayView<const float>(Data[Index].GetData() + Instance * 3, 3), Instance == Placed[Index].Num() - 1);
		}
	}
}

void AVaelFireRoller::UpdateWind(float DeltaSeconds)
{
	const float Grow = FMath::Lerp(GrowFrom, GrowTo, FMath::Clamp(Age / GrowSeconds, 0.0f, 1.0f));
	const float Radius = Width * DrumRadiusShare * Grow;
	const FVector Side(-Forward.Y, Forward.X, 0.0f);
	const bool bRolling = Collapsed < 0.0f;

	// Streaks of wind push into the back of the drum, each running a short way with it and starting over somewhere else
	if (Streaks != nullptr)
	{
		const float Shown = bRolling ? FMath::Clamp(Age / 0.15f, 0.0f, 1.0f) : FMath::Clamp(1.0f - Collapsed / SinkSeconds, 0.0f, 1.0f);
		for (int32 Index = 0; Index < WindStreaks; ++Index)
		{
			const float Phase = Age / StreakRestart + Index / static_cast<float>(WindStreaks);
			const int32 Round = FMath::FloorToInt32(Phase);
			const float Run = Phase - Round;
			FRandomStream Spot(Index * 977 + Round * 131);
			const FVector Head = Axis - Forward * (Radius + Spot.FRandRange(20.0f, 90.0f) * (1.0f - Run))
				+ Side * Spot.FRandRange(-0.5f, 0.5f) * Width + FVector::UpVector * Spot.FRandRange(-0.4f, 1.6f) * Radius;
			if (Run < DeltaSeconds / StreakRestart + 0.001f)
			{
				Streaks->Restart(Index, Head);
			}
			Streaks->MoveHead(Index, Head, Shown * FMath::Sin(UE_PI * Run));
		}
		Streaks->Commit();
	}

	// Embers flung up and ahead off the front of the drum
	if (Embers != nullptr && bRolling)
	{
		const float Speed = FMath::Max(Carrier.IsValid() ? Carrier->GetVelocity().Size2D() : 0.0f, 300.0f);
		EmberCredit += RollerEmberRate * Grow * DeltaSeconds;
		for (; EmberCredit >= 1.0f; EmberCredit -= 1.0f)
		{
			const FVector From = Axis + Forward * Radius * 0.8f + Side * FMath::FRandRange(-0.5f, 0.5f) * Width + FVector::UpVector * FMath::FRandRange(0.0f, 1.0f) * Radius;
			const FVector Throw = Forward * Speed * FMath::FRandRange(1.0f, 1.5f) + FVector(0.0f, 0.0f, FMath::FRandRange(150.0f, 320.0f)) + FMath::VRand() * 60.0f;
			Embers->Add(EVaelFlake::Ember, From, Throw, FMath::FRandRange(4.0f, 7.0f), GroundZ);
		}
	}
}

void AVaelFireRoller::LayTrail()
{
	// Rows of burning patches behind the drum, three across, which hurt whoever of the caster's enemies walks in
	const FVector Side(-Forward.Y, Forward.X, 0.0f);
	const float Radius = Width * DrumRadiusShare;
	while (Rolled >= NextTrailRow)
	{
		NextTrailRow += TrailRowSpacing;
		for (const float Across : { -0.25f, 0.25f })
		{
			FVector Spot = Axis - Forward * Radius + Side * Across * Width;
			FHitResult GroundHit;
			Spot.Z = VaelGround::TraceGround(GetWorld(), Spot + FVector(0.0f, 0.0f, 150.0f), Spot - FVector(0.0f, 0.0f, 300.0f), GroundHit, GetInstigator()) ? GroundHit.Location.Z : GroundZ;
			AVaelGroundArea::SpawnArea(GetWorld(), Spot + FVector(0.0f, 0.0f, 2.0f), EVaelElement::Fire, Width * 0.17f, TrailSeconds, TrailDamagePerSecond, GetInstigator());
		}
	}
}

void AVaelFireRoller::Collapse()
{
	// It breaks apart in a burst of flames and sparks where it ends
	Collapsed = 0.0f;
	AVaelFireSpot::SpawnBurst(GetWorld(), FVector(Axis.X, Axis.Y, GroundZ), Width * 0.4f);
	FVaelDebris Sparks;
	Sparks.Count = 24;
	Sparks.Spread = Width * 0.4f;
	Sparks.Speed = 260.0f;
	Sparks.Lift = 420.0f;
	Sparks.Lifetime = 0.9f;
	Sparks.MinSize = 2.0f;
	Sparks.MaxSize = 5.0f;
	Sparks.Color = FireColor;
	Sparks.Glow = 6.0f;
	Sparks.bSoft = true;
	AVaelDebrisBurst::Spawn(this, Axis, Sparks);
	SetLifeSpan(FadeSeconds + SinkSeconds);
}

void AVaelFireRoller::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	Age += DeltaSeconds;

	const AActor* Rider = Carrier.Get();
	if (Collapsed < 0.0f && Rider == nullptr)
	{
		Collapse();
	}

	if (Collapsed >= 0.0f)
	{
		Collapsed += DeltaSeconds;
	}
	else
	{
		// Rolls on the ground below the projectile, turning with the way it went
		const FVector Now = Rider->GetActorLocation();
		const FVector Moved = (Now - LastCarrier) * FVector(1.0f, 1.0f, 0.0f);
		LastCarrier = Now;
		if (Moved.SizeSquared() > 1.0f)
		{
			Forward = Moved.GetSafeNormal();
		}
		FHitResult GroundHit;
		GroundZ = VaelGround::TraceGround(GetWorld(), Now + FVector(0.0f, 0.0f, 100.0f), Now - FVector(0.0f, 0.0f, 400.0f), GroundHit, GetInstigator()) ? GroundHit.Location.Z : Now.Z - 100.0f;
		const float Grow = FMath::Lerp(GrowFrom, GrowTo, FMath::Clamp(Age / GrowSeconds, 0.0f, 1.0f));
		const float Radius = Width * DrumRadiusShare * Grow;
		Axis = FVector(Now.X, Now.Y, GroundZ + Radius);
		Rolled += Moved.Size();
		Roll += Moved.Size() / FMath::Max(Radius, 1.0f);
		SetActorLocation(Axis);
		LayTrail();
	}

	UpdateDrum(DeltaSeconds);
	UpdateWind(DeltaSeconds);
	if (Embers != nullptr && Collapsed >= 0.0f)
	{
		Embers->SetFade(FMath::Clamp(1.0f - (Collapsed - SinkSeconds) / FadeSeconds, 0.0f, 1.0f));
	}
}
