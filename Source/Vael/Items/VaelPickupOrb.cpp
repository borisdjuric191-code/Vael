// Copyright Epic Games, Inc. All Rights Reserved.

#include "Items/VaelPickupOrb.h"
#include "AbilitySystemComponent.h"
#include "Combat/VaelAttributeSet.h"
#include "Components/PointLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "Magic/VaelSpellEffects.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Player/VaelCharacter.h"
#include "ProceduralMeshComponent.h"
#include "UI/VaelCombatTextSubsystem.h"
#include "World/VaelGround.h"
#include "World/VaelWorldSettings.h"

namespace
{
	/** Length of one wing in cm: big enough to read at the far camera, where a metre is about 40 pixels */
	constexpr float WingReach = 38.0f;

	/** Outline points of a wing */
	constexpr int32 WingPoints = 24;

	/** Bright layers of a wing from the rim to the root: size against the wing, glow and shade of the colour */
	constexpr int32 WingLayers = 3;
	const float WingLayerSizes[WingLayers] = { 1.0f, 0.72f, 0.42f };
	const float WingLayerGlows[WingLayers] = { 1.5f, 4.0f, 8.0f };
	const float WingLayerShades[WingLayers] = { 0.45f, 1.0f, 1.0f };

	/** Height the moth hovers at above the ground and how far it wanders from there, in cm */
	constexpr float HoverHeight = 75.0f;
	constexpr float HoverWander = 34.0f;
	constexpr float HoverBob = 14.0f;

	/** Seconds it takes to rise from the fallen creature and to dissolve */
	constexpr float RiseTime = 0.7f;
	constexpr float DissolveTime = 0.2f;

	/** Wing beats per second while hovering and flying, and the highest and lowest angle in degrees: above 40 the wings turn edge-on to the game camera */
	constexpr float HoverBeatRate = 3.2f;
	constexpr float FlyBeatRate = 9.0f;
	constexpr float BeatHigh = 40.0f;
	constexpr float BeatLow = -10.0f;

	/** Flight to the player in cm/s, faster than running at the end, and how close it comes */
	constexpr float FlyStartSpeed = 240.0f;
	constexpr float FlyTopSpeed = 1100.0f;
	constexpr float FlyAcceleration = 1500.0f;
	constexpr float GiveDistance = 45.0f;

	/** Light on the ground below it */
	constexpr float LightIntensity = 1800.0f;
	constexpr float LightRadius = 320.0f;

	/** Breathing of the glow: per second and how strong */
	constexpr float BreathRate = 2.4f;
	constexpr float BreathDepth = 0.3f;

	/** Sparks around the moth */
	constexpr int32 MoteCount = 5;
	constexpr float MoteSize = 0.045f;
	constexpr float MoteOrbit = 34.0f;

	/** Colors of the moths and their numbers */
	const FLinearColor HealthOrbColor = FLinearColor(FColor(0xe8, 0x24, 0x1c));
	const FLinearColor ManaOrbColor = FLinearColor(FColor(0x1e, 0x62, 0xf0));
	const FLinearColor HealthTextColor = FLinearColor(FColor(0xff, 0x8a, 0x80));
	const FLinearColor ManaTextColor = FLinearColor(FColor(0x80, 0xb4, 0xff));

	/** Outline of the forewing, reaching along +Y and forward along +X: a pointed sail */
	FVector2D ForewingOutline(float Along)
	{
		const float Angle = FMath::DegreesToRadians(2.0f + 62.0f * Along);
		const float Peak = (Along - 0.42f) / 0.3f;
		const float Reach = WingReach * (0.5f + 0.5f * FMath::Exp(-Peak * Peak));
		return FVector2D(FMath::Sin(Angle), FMath::Cos(Angle)) * Reach;
	}

	/** Outline of the hindwing, round for health, with a swallowtail for mana */
	FVector2D HindwingOutline(float Along, bool bTail)
	{
		const float Angle = FMath::DegreesToRadians(-78.0f + 76.0f * Along);
		float Reach = WingReach * 0.6f * (0.7f + 0.3f * FMath::Sin(UE_PI * Along));
		if (bTail)
		{
			const float Peak = (Along - 0.16f) / 0.1f;
			Reach += WingReach * 0.5f * FMath::Exp(-Peak * Peak);
		}
		return FVector2D(FMath::Sin(Angle), FMath::Cos(Angle)) * Reach;
	}

	/** Adds a fan from the root of a wing to its outline, seen from both sides */
	void AddWingFan(TArray<FVector>& Vertices, TArray<int32>& Triangles, bool bFore, bool bTail, float Size, float Lift)
	{
		const int32 Centre = Vertices.Add(FVector(0.0f, 0.0f, Lift));

		for (int32 Point = 0; Point <= WingPoints; ++Point)
		{
			const float Along = Point / static_cast<float>(WingPoints);
			const FVector2D Edge = (bFore ? ForewingOutline(Along) : HindwingOutline(Along, bTail)) * Size;
			Vertices.Add(FVector(Edge.X, Edge.Y, Lift));

			if (Point > 0)
			{
				const int32 Last = Vertices.Num() - 1;
				Triangles.Append({ Centre, Last - 1, Last, Centre, Last, Last - 1 });
			}
		}
	}
}

AVaelPickupOrb::AVaelPickupOrb()
{
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));

	Light = CreateDefaultSubobject<UPointLightComponent>(TEXT("Light"));
	Light->SetupAttachment(RootComponent);
	Light->SetCastShadows(false);
	Light->SetIntensity(LightIntensity);
	Light->SetAttenuationRadius(LightRadius);

	PrimaryActorTick.bCanEverTick = true;
}

AVaelPickupOrb* AVaelPickupOrb::DropOrb(UWorld* World, const FVector& Location, EVaelOrbKind InKind)
{
	if (World == nullptr)
	{
		return nullptr;
	}

	// The moth hovers above the ground below the spot
	FVector Ground = Location;
	FHitResult GroundHit;
	if (VaelGround::TraceGround(World, Location + FVector(0.0f, 0.0f, 80.0f), Location - FVector(0.0f, 0.0f, 500.0f), GroundHit, nullptr))
	{
		Ground = GroundHit.Location;
	}

	const FTransform SpawnTransform(FVector(Location.X, Location.Y, FMath::Max(Location.Z, Ground.Z + 10.0f)));

	AVaelPickupOrb* Orb = World->SpawnActorDeferred<AVaelPickupOrb>(AVaelPickupOrb::StaticClass(), SpawnTransform, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (Orb != nullptr)
	{
		Orb->Kind = InKind;
		Orb->RiseStart = SpawnTransform.GetLocation();
		Orb->HoverHome = Ground + FVector(0.0f, 0.0f, HoverHeight);
		Orb->FinishSpawning(SpawnTransform);
	}

	return Orb;
}

void AVaelPickupOrb::BeginPlay()
{
	Super::BeginPlay();

	Seed = FMath::FRandRange(0.0f, 100.0f);
	BuildMoth();
	SetActorScale3D(FVector(0.3f));
}

void AVaelPickupOrb::BuildMoth()
{
	const bool bHealth = Kind == EVaelOrbKind::Health;
	const FLinearColor Color = bHealth ? HealthOrbColor : ManaOrbColor;
	const FLinearColor BrightColor = FMath::Lerp(Color, FLinearColor::White, 0.45f);

	Light->SetLightColor(Color);

	UMaterialInterface* CoreMaterial = nullptr;
	UMaterialInterface* GlowMaterial = nullptr;
	VaelEffects::LoadLookMaterials(CoreMaterial, GlowMaterial);

	TArray<UMaterialInstanceDynamic*> LayerMaterials;
	for (int32 Layer = 0; Layer < WingLayers; ++Layer)
	{
		UMaterialInstanceDynamic* Material = UMaterialInstanceDynamic::Create(CoreMaterial, this);
		Material->SetVectorParameterValue(VaelEffects::LookColorParameter, Layer == WingLayers - 1 ? BrightColor : Color * WingLayerShades[Layer]);
		Material->SetScalarParameterValue(VaelEffects::LookGlowParameter, WingLayerGlows[Layer]);
		Material->SetScalarParameterValue(VaelEffects::LookRimParameter, 0.0f);
		LayerMaterials.Add(Material);
	}
	WingCoreMaterial = LayerMaterials.Last();

	// One sheet in the space of its hinge: the body runs along X and the wing reaches out along Y. Left is right mirrored
	for (int32 Side = 0; Side < 2; ++Side)
	{
		const float Mirror = Side == 0 ? -1.0f : 1.0f;

		USceneComponent* Hinge = NewObject<USceneComponent>(this);
		Hinge->SetupAttachment(RootComponent);
		Hinge->SetRelativeLocation(FVector(0.0f, 1.5f * Mirror, 0.0f));
		Hinge->RegisterComponent();
		Hinges.Add(Hinge);

		UProceduralMeshComponent* Sheet = NewObject<UProceduralMeshComponent>(this);
		Sheet->SetupAttachment(Hinge);
		Sheet->SetRelativeScale3D(FVector(1.0f, Mirror, 1.0f));
		Sheet->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Sheet->SetCastShadow(false);
		Sheet->bReceivesDecals = false;
		Sheet->RegisterComponent();

		for (int32 Layer = 0; Layer < WingLayers; ++Layer)
		{
			TArray<FVector> Vertices;
			TArray<int32> Triangles;
			const float Lift = Layer * 0.4f;
			AddWingFan(Vertices, Triangles, true, false, WingLayerSizes[Layer], Lift);
			AddWingFan(Vertices, Triangles, false, !bHealth, WingLayerSizes[Layer], Lift);

			TArray<FVector> Normals;
			Normals.Init(FVector::UpVector, Vertices.Num());
			TArray<FVector2D> UVs;
			UVs.Init(FVector2D::ZeroVector, Vertices.Num());

			Sheet->CreateMeshSection_LinearColor(Layer, Vertices, Triangles, Normals, UVs, TArray<FLinearColor>(), TArray<FProcMeshTangent>(), false);
			Sheet->SetMaterial(Layer, LayerMaterials[Layer]);
		}
	}

	Body = VaelEffects::AddLookShape(this, CoreMaterial, BrightColor, WingLayerGlows[WingLayers - 1]);
	Body->SetRelativeScale3D(FVector(0.3f, 0.06f, 0.06f));
	Body->SetVisibility(true);

	for (int32 Index = 0; Index < MoteCount; ++Index)
	{
		UStaticMeshComponent* Mote = VaelEffects::AddLookShape(this, GlowMaterial, Color, 8.0f);
		Mote->SetRelativeScale3D(FVector(MoteSize));
		Mote->SetVisibility(true);
		Motes.Add(Mote);
	}
}

AVaelCharacter* AVaelPickupOrb::FindTaker() const
{
	AVaelCharacter* Closest = nullptr;
	float ClosestDistance = UVaelWorldSettings::Get()->OrbPickupRadius;

	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		const APlayerController* PlayerController = It->Get();
		AVaelCharacter* Player = PlayerController != nullptr ? PlayerController->GetPawn<AVaelCharacter>() : nullptr;

		if (Player == nullptr || Player->IsDowned())
		{
			continue;
		}

		const float Distance = FVector::Dist2D(Player->GetActorLocation(), GetActorLocation());
		if (Distance > ClosestDistance)
		{
			continue;
		}

		// Health moths wait for someone who needs them
		const UAbilitySystemComponent* AbilitySystem = Player->GetAbilitySystemComponent();
		if (Kind == EVaelOrbKind::Health && AbilitySystem->GetNumericAttribute(UVaelAttributeSet::GetHealthAttribute()) >= AbilitySystem->GetNumericAttribute(UVaelAttributeSet::GetMaxHealthAttribute()))
		{
			continue;
		}

		Closest = Player;
		ClosestDistance = Distance;
	}

	return Closest;
}

void AVaelPickupOrb::GiveTo(AVaelCharacter* Player)
{
	const UVaelWorldSettings* WorldSettings = UVaelWorldSettings::Get();

	UAbilitySystemComponent* AbilitySystem = Player->GetAbilitySystemComponent();
	const FGameplayAttribute Attribute = Kind == EVaelOrbKind::Health ? UVaelAttributeSet::GetHealthAttribute() : UVaelAttributeSet::GetManaAttribute();
	const FGameplayAttribute MaxAttribute = Kind == EVaelOrbKind::Health ? UVaelAttributeSet::GetMaxHealthAttribute() : UVaelAttributeSet::GetMaxManaAttribute();
	const float Amount = Kind == EVaelOrbKind::Health ? WorldSettings->HealthOrbAmount : WorldSettings->ManaOrbAmount;

	const float Current = AbilitySystem->GetNumericAttribute(Attribute);
	const float Max = AbilitySystem->GetNumericAttribute(MaxAttribute);

	AbilitySystem->SetNumericAttributeBase(Attribute, FMath::Min(Max, Current + Amount));
	UVaelCombatTextSubsystem::PostPickup(Player, FText::Format(NSLOCTEXT("VaelItems", "OrbAmount", "+{0}"), FMath::RoundToInt(Amount)),
		Kind == EVaelOrbKind::Health ? HealthTextColor : ManaTextColor);
}

void AVaelPickupOrb::Animate(float DeltaSeconds)
{
	BeatPhase += DeltaSeconds * 2.0f * UE_PI * (State == EMothState::Flying ? FlyBeatRate : HoverBeatRate);

	// Quick up, slower down, as a moth rows through the air
	const float Beat = 0.5f + 0.5f * FMath::Sin(BeatPhase + 0.4f * FMath::Sin(BeatPhase));
	const float BeatAngle = FMath::DegreesToRadians(FMath::Lerp(BeatLow, BeatHigh, Beat));
	for (int32 Side = 0; Side < Hinges.Num(); ++Side)
	{
		Hinges[Side]->SetRelativeRotation(FQuat(FVector::ForwardVector, Side == 0 ? -BeatAngle : BeatAngle));
	}

	const float Breath = 1.0f + BreathDepth * FMath::Sin(Clock * BreathRate + Seed);
	Light->SetIntensity(LightIntensity * Breath);
	VaelEffects::SetLookGlow(Body, WingLayerGlows[WingLayers - 1] * Breath);
	WingCoreMaterial->SetScalarParameterValue(VaelEffects::LookGlowParameter, WingLayerGlows[WingLayers - 1] * Breath);

	for (int32 Index = 0; Index < Motes.Num(); ++Index)
	{
		const float Angle = Clock * (0.9f + 0.2f * Index) + Index * 2.0f * UE_PI / MoteCount + Seed;
		const float Fall = FMath::Fractional(Clock * 0.35f + Index * 0.37f);
		Motes[Index]->SetRelativeLocation(FVector(FMath::Cos(Angle) * MoteOrbit, FMath::Sin(Angle) * MoteOrbit, 8.0f - 42.0f * Fall));
		Motes[Index]->SetRelativeScale3D(FVector(MoteSize * (1.0f - Fall * 0.7f)));
	}
}

void AVaelPickupOrb::FaceAlong(const FVector& Moved, float DeltaSeconds)
{
	FRotator Rotation = GetActorRotation();
	if (Moved.SizeSquared2D() > FMath::Square(8.0f * DeltaSeconds))
	{
		Rotation.Yaw = FMath::RInterpTo(FRotator(0.0f, Rotation.Yaw, 0.0f), FRotator(0.0f, Moved.Rotation().Yaw, 0.0f), DeltaSeconds, 4.0f).Yaw;
	}

	// Head up, swaying with the beats
	Rotation.Pitch = 22.0f;
	Rotation.Roll = 7.0f * FMath::Sin(BeatPhase * 0.5f + Seed);
	SetActorRotation(Rotation);
}

void AVaelPickupOrb::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	Clock += DeltaSeconds;
	StateTime += DeltaSeconds;

	const FVector Before = GetActorLocation();
	FVector Location = Before;

	if (State == EMothState::Rising)
	{
		const float Along = FMath::Clamp(StateTime / RiseTime, 0.0f, 1.0f);
		Location = FMath::Lerp(RiseStart, HoverHome, FMath::InterpEaseOut(0.0f, 1.0f, Along, 2.0f));
		SetActorScale3D(FVector(FMath::Lerp(0.3f, 1.0f, Along)));

		if (Along >= 1.0f)
		{
			State = EMothState::Hovering;
			StateTime = 0.0f;
		}
	}
	else if (State == EMothState::Dissolving)
	{
		const float Along = FMath::Clamp(StateTime / DissolveTime, 0.0f, 1.0f);
		SetActorScale3D(FVector(FMath::Max(1.0f - Along, 0.01f)));
		if (const AVaelCharacter* Player = Taker.Get())
		{
			Location = Player->GetActorLocation();
		}

		if (Along >= 1.0f)
		{
			Destroy();
			return;
		}
	}
	else
	{
		AVaelCharacter* Player = FindTaker();
		Taker = Player;

		if (Player != nullptr)
		{
			if (State != EMothState::Flying)
			{
				State = EMothState::Flying;
				FlySpeed = FlyStartSpeed;
			}

			FlySpeed = FMath::Min(FlyTopSpeed, FlySpeed + FlyAcceleration * DeltaSeconds);
			const FVector PlayerLocation = Player->GetActorLocation();
			Location = FMath::VInterpConstantTo(Before, PlayerLocation, DeltaSeconds, FlySpeed);

			if (FVector::Dist(Location, PlayerLocation) < GiveDistance)
			{
				GiveTo(Player);
				State = EMothState::Dissolving;
				StateTime = 0.0f;
			}
		}
		else
		{
			// A small, unsteady figure of eight around its place; after a player who got away it flutters back
			State = EMothState::Hovering;
			const FVector Goal = HoverHome + FVector(FMath::Sin(Clock * 0.8f + Seed) * HoverWander,
				FMath::Sin(Clock * 1.6f + Seed) * HoverWander * 0.6f,
				FMath::Sin(Clock * 2.3f + Seed) * HoverBob);
			Location = FMath::VInterpTo(Before, Goal, DeltaSeconds, 3.0f);
		}
	}

	SetActorLocation(Location);
	FaceAlong(Location - Before, DeltaSeconds);
	Animate(DeltaSeconds);
}
