// Copyright Epic Games, Inc. All Rights Reserved.

#include "Floromant/VaelPlant.h"
#include "AbilitySystemComponent.h"
#include "Combat/VaelAttributeSet.h"
#include "Combat/VaelCombatStatics.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Creatures/VaelCreature.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Floromant/VaelFloromantSettings.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Magic/VaelDebrisBurst.h"
#include "Magic/VaelGameplayTags.h"
#include "Magic/VaelGroundArea.h"
#include "Magic/VaelSpellEffects.h"
#include "Magic/VaelSpellProjectile.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Nature/VaelGarden.h"
#include "Player/VaelCharacter.h"
#include "ProceduralMeshComponent.h"
#include "Vael.h"
#include "VaelAssets.h"

namespace
{
	/** Half height of the capsule every plant stands in */
	constexpr float PlantCapsuleHalfHeight = 50.0f;

	/** Seconds a plant needs to wither */
	constexpr float WitherDuration = 0.7f;

	/** The plant grows in a little higher than its model, so the soft edge of the mask clears the top */
	constexpr float GrowMaskOvershoot = 1.15f;

	/** Size of the model at the very start of the growth, before it pops up to its full size */
	constexpr float GrowStartScale = 0.3f;

	/** Grow height the material gets once the plant has grown: far above any plant */
	constexpr float FullyGrownHeight = 100000.0f;

	/** How fast squash, lean and the giant form ease, per second */
	constexpr float SquashRecovery = 7.0f;
	constexpr float GiantEase = 3.0f;

	/** Degrees per second a shooting plant turns towards its enemy */
	constexpr float PlantTurnSpeed = 240.0f;

	/** Seconds between two pulses of an effect around the plant */
	constexpr float PulseInterval = 1.0f;

	/** Shots of fire seeds and pollen darts: seconds between them, seconds of glowing and ducking before, speed, burst */
	constexpr float FireSeedInterval = 1.2f;
	constexpr float FireSeedWindup = 0.35f;
	constexpr float PollenInterval = 0.7f;
	constexpr float PollenWindup = 0.18f;
	constexpr float ShotSpeed = 1400.0f;
	constexpr float FireSeedBurstRadius = 160.0f;

	/** Rings along each root and corners around each ring */
	constexpr int32 RootRings = 14;
	constexpr int32 RootSides = 6;

	/** Material parameters of M_Floromant_Pflanze */
	const FName GroundZParameter(TEXT("GroundZ"));
	const FName PlantHeightParameter(TEXT("PlantHeight"));
	const FName GrowHeightParameter(TEXT("GrowHeight"));
	const FName GrowColorParameter(TEXT("GrowColor"));
	const FName FlashParameter(TEXT("Flash"));
	const FName FlashColorParameter(TEXT("FlashColor"));
	const FName TintParameter(TEXT("Tint"));

	/** Ease that swings a little beyond 1 before it settles: the pop of a plant shooting up */
	float EaseOutBack(float X)
	{
		constexpr float C1 = 1.70158f;
		constexpr float C3 = C1 + 1.0f;
		const float T = X - 1.0f;
		return 1.0f + C3 * T * T * T + C1 * T * T;
	}

	/** Material of roots and vines with its own instance */
	UMaterialInstanceDynamic* MakeRootMaterial(UObject* Outer, float GroundZ, float Height)
	{
		UMaterialInterface* Material = VaelAssets::LoadOptional(UVaelFloromantSettings::Get()->RootMaterial);
		if (Material == nullptr)
		{
			Material = LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
		}
		UMaterialInstanceDynamic* Instance = UMaterialInstanceDynamic::Create(Material, Outer);
		Instance->SetScalarParameterValue(GroundZParameter, GroundZ);
		Instance->SetScalarParameterValue(PlantHeightParameter, Height);
		return Instance;
	}
}

void VaelRoots::AddRoot(TArray<FVector>& Vertices, TArray<FVector>& Normals, TArray<FVector2D>& UVs, TArray<int32>& Triangles,
	TFunctionRef<FVector(float)> PointAt, float Thickness, bool bPointed, float Seed)
{
	const int32 First = Vertices.Num();
	for (int32 RingIndex = 0; RingIndex <= RootRings; ++RingIndex)
	{
		const float Along = RingIndex / static_cast<float>(RootRings);
		const FVector Center = PointAt(Along);
		const FVector Forward = (PointAt(FMath::Min(Along + 0.02f, 1.0f)) - PointAt(FMath::Max(Along - 0.02f, 0.0f))).GetSafeNormal();
		FVector Right = FVector::CrossProduct(Forward, FVector::UpVector);
		Right = Right.IsNearlyZero() ? FVector::RightVector : Right.GetSafeNormal();
		const FVector Up = FVector::CrossProduct(Right, Forward);

		// Thick at the start, thin at the tip of a spike; knots along the way
		const float Taper = bPointed ? FMath::Lerp(1.0f, 0.15f, Along) : 1.0f - 0.45f * FMath::Sin(Along * UE_PI);
		const float Knot = 1.0f + 0.25f * FMath::Max(0.0f, FMath::Sin(Along * 13.0f + Seed));

		for (int32 SideIndex = 0; SideIndex < RootSides; ++SideIndex)
		{
			const float Angle = SideIndex * 2.0f * UE_PI / RootSides;
			const FVector Out = Right * FMath::Cos(Angle) + Up * FMath::Sin(Angle);
			Vertices.Add(Center + Out * Thickness * Taper * Knot);
			Normals.Add(Out);
			UVs.Add(FVector2D(SideIndex / static_cast<float>(RootSides), Along * 2.0f));
		}
	}

	for (int32 RingIndex = 0; RingIndex < RootRings; ++RingIndex)
	{
		for (int32 SideIndex = 0; SideIndex < RootSides; ++SideIndex)
		{
			const int32 A = First + RingIndex * RootSides + SideIndex;
			const int32 B = First + RingIndex * RootSides + (SideIndex + 1) % RootSides;
			Triangles.Append({ A, B, A + RootSides, B, B + RootSides, A + RootSides });
		}
	}
}

// ---------------------------------------------------------------- Plant

AVaelPlant::AVaelPlant()
{
	GetCapsuleComponent()->InitCapsuleSize(30.0f, PlantCapsuleHalfHeight);

	// Plants are found by spells like any pawn, but nobody bumps into them: walking through the leaves is fine
	GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
	GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
	GetCapsuleComponent()->CanCharacterStepUpOn = ECB_No;

	// Rooted: no controller, no movement
	AutoPossessAI = EAutoPossessAI::Disabled;
	GetCharacterMovement()->GravityScale = 0.0f;
	GetCharacterMovement()->bRunPhysicsWithNoController = false;

	GetMesh()->SetVisibility(false);
	GetMesh()->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	Body = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Body"));
	Body->SetupAttachment(RootComponent);
	Body->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Body->bReceivesDecals = false;

	StartingMana = 0.0f;

	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = true;
}

AVaelPlant* AVaelPlant::GrowPlant(UVaelPlantData* InData, AVaelCharacter* InGardener, const FVector& GroundLocation, float Yaw, bool bInShadow)
{
	UWorld* World = InGardener != nullptr ? InGardener->GetWorld() : nullptr;
	if (World == nullptr || InData == nullptr)
	{
		return nullptr;
	}

	const FTransform SpawnTransform(FRotator(0.0f, Yaw, 0.0f), GroundLocation + FVector(0.0f, 0.0f, PlantCapsuleHalfHeight));
	AVaelPlant* Plant = World->SpawnActorDeferred<AVaelPlant>(AVaelPlant::StaticClass(), SpawnTransform, InGardener, InGardener, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (Plant == nullptr)
	{
		return nullptr;
	}

	Plant->Data = InData;
	Plant->Gardener = InGardener;
	Plant->bShadow = bInShadow;
	Plant->GroundZ = GroundLocation.Z;
	Plant->StartingHealth = InData->Health;
	Plant->FinishSpawning(SpawnTransform);

	UE_LOG(LogVael, Log, TEXT("'%s' grows %s"), *GetNameSafe(InGardener), *InData->GetName(bInShadow).ToString());
	return Plant;
}

AVaelPlant* AVaelPlant::ActForWildPlant(UVaelPlantData* InData, AVaelCharacter* Caller, const FVector& GroundLocation, bool bInShadow, float Duration, bool bGiant)
{
	UWorld* World = Caller != nullptr ? Caller->GetWorld() : nullptr;
	if (World == nullptr || InData == nullptr)
	{
		return nullptr;
	}

	const FTransform SpawnTransform(GroundLocation + FVector(0.0f, 0.0f, PlantCapsuleHalfHeight));
	AVaelPlant* Plant = World->SpawnActorDeferred<AVaelPlant>(AVaelPlant::StaticClass(), SpawnTransform, Caller, Caller, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (Plant == nullptr)
	{
		return nullptr;
	}

	// Invisible, untouchable and already grown: the wild plant itself stays as it is
	Plant->Data = InData;
	Plant->Gardener = Caller;
	Plant->bShadow = bInShadow;
	Plant->bWild = true;
	Plant->GroundZ = GroundLocation.Z;
	Plant->Age = UVaelFloromantSettings::Get()->GrowTime;
	Plant->FinishSpawning(SpawnTransform);
	Plant->Call(Duration, bInShadow);
	if (bGiant)
	{
		Plant->MakeGiant(Duration);
	}
	Plant->SetLifeSpan(Duration + 0.5f);
	return Plant;
}

void AVaelPlant::BeginPlay()
{
	Super::BeginPlay();

	GetCharacterMovement()->DisableMovement();
	GetCharacterMovement()->SetComponentTickEnabled(false);

	if (bWild)
	{
		Body->SetVisibility(false);
		GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		return;
	}

	BuildLook();
	UpdateLook(0.0f);

	// Earth sprays up where it breaks out of the ground, with a few sparks in its colour
	const FVector Ground(GetActorLocation().X, GetActorLocation().Y, GroundZ);

	FVaelDebris Earth;
	Earth.Count = 16;
	Earth.Spread = 25.0f;
	Earth.Speed = 240.0f;
	Earth.Lift = 380.0f;
	Earth.Lifetime = 0.7f;
	Earth.MinSize = 4.0f;
	Earth.MaxSize = 10.0f;
	Earth.Color = FLinearColor(0.11f, 0.075f, 0.05f);
	Earth.RockShare = 0.15f;
	AVaelDebrisBurst::Spawn(this, Ground, Earth);
	Puff(GetColor(), 30.0f, 10, 160.0f);
}

void AVaelPlant::BuildLook()
{
	UStaticMesh* PlantMesh = VaelAssets::LoadOptional(Data->Mesh);
	if (PlantMesh == nullptr)
	{
		// Placeholder while the model is missing: a stretched sphere
		PlantMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	}

	Body->SetStaticMesh(PlantMesh);
	GrowingPart = Body;

	// Scaled to its height with a little variation, standing with its lowest point on the ground, turned at random
	const FBox Bounds = PlantMesh != nullptr ? PlantMesh->GetBoundingBox() : FBox(FVector(-50.0f), FVector(50.0f));
	const float Scale = Data->Height / FMath::Max(Bounds.GetSize().Z, 1.0f) * FMath::FRandRange(0.92f, 1.08f);
	GrownScale = FVector(Scale);

	const FVector Center = Bounds.GetCenter();
	Body->SetRelativeLocation(FVector(-Center.X * Scale, -Center.Y * Scale, GroundZ - GetActorLocation().Z - Bounds.Min.Z * Scale));
	Body->SetRelativeRotation(FRotator(FMath::FRandRange(-4.0f, 4.0f), FMath::FRandRange(0.0f, 360.0f), FMath::FRandRange(-4.0f, 4.0f)));

	if (UMaterialInterface* Material = VaelAssets::LoadOptional(Data->Material))
	{
		UMaterialInstanceDynamic* Instance = UMaterialInstanceDynamic::Create(Material, this);
		Instance->SetScalarParameterValue(GroundZParameter, GroundZ);
		Instance->SetScalarParameterValue(PlantHeightParameter, Data->Height);
		Instance->SetVectorParameterValue(GrowColorParameter, GetColor());
		Instance->SetVectorParameterValue(FlashColorParameter, GetColor());
		if (bShadow)
		{
			// The shadow form: the same plant, darkened and drained towards violet
			Instance->SetVectorParameterValue(TintParameter, FLinearColor(0.45f, 0.32f, 0.55f));
		}
		for (int32 MaterialIndex = 0; MaterialIndex < FMath::Max(Body->GetNumMaterials(), 1); ++MaterialIndex)
		{
			Body->SetMaterial(MaterialIndex, Instance);
		}
		Materials.Add(Instance);
	}

	// The shadow overlay of the garden on top, like on the shadow forms growing there
	if (bShadow)
	{
		if (UMaterialInterface* Overlay = VaelAssets::LoadOptional(UVaelGardenSettings::Get()->ShadowOverlay))
		{
			Body->SetOverlayMaterial(Overlay);
		}
	}
}

FLinearColor AVaelPlant::GetColor() const
{
	return Data == nullptr ? FLinearColor::White : (bShadow || bActAsShadow) ? Data->ShadowColor : Data->Color;
}

EVaelPlantEffect AVaelPlant::GetActiveEffect() const
{
	return Data != nullptr ? Data->GetEffect(bActAsShadow) : EVaelPlantEffect::None;
}

float AVaelPlant::GetPower() const
{
	const float Giant = FMath::Lerp(1.0f, UVaelFloromantSettings::Get()->GiantPower, GiantAmount);
	return Data != nullptr ? Data->Power * Giant : 0.0f;
}

void AVaelPlant::Call(float Duration, bool bInActAsShadow)
{
	CallTimeLeft = FMath::Max(CallTimeLeft, Duration);
	bActAsShadow = bInActAsShadow;
	bCallStarted = false;
	PulseCountdown = 0.0f;
	Flash(0.5f, 1.2f);
	Squash(1.12f);
}

void AVaelPlant::MakeGiant(float Duration)
{
	GiantTimeLeft = FMath::Max(GiantTimeLeft, Duration);
}

void AVaelPlant::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	Age += DeltaSeconds;
	GiantTimeLeft = FMath::Max(0.0f, GiantTimeLeft - DeltaSeconds);
	GiantAmount = FMath::FInterpTo(GiantAmount, GiantTimeLeft > 0.0f ? 1.0f : 0.0f, DeltaSeconds, GiantEase);
	if (!bWild)
	{
		UpdateLook(DeltaSeconds);
	}

	if (bWithering)
	{
		WitherTime += DeltaSeconds;
		if (WitherTime >= WitherDuration)
		{
			Destroy();
		}
		return;
	}

	if (CallTimeLeft > 0.0f && Age >= UVaelFloromantSettings::Get()->GrowTime)
	{
		TickCall(DeltaSeconds);
		CallTimeLeft -= DeltaSeconds;
	}

	if (!bWild && Data != nullptr && Age >= Data->Lifetime)
	{
		Wither();
	}
}

void AVaelPlant::UpdateLook(float DeltaSeconds)
{
	if (Data == nullptr)
	{
		return;
	}

	const float Growth = FMath::Clamp(Age / UVaelFloromantSettings::Get()->GrowTime, 0.0f, 1.0f);
	const float Wilt = bWithering ? FMath::Clamp(WitherTime / WitherDuration, 0.0f, 1.0f) : 0.0f;
	const float Giant = FMath::Lerp(1.0f, UVaelFloromantSettings::Get()->GiantScale, GiantAmount);

	// The mask fades the plant in from the bottom up while it grows and takes it back from the top when it withers
	float GrowHeight = Growth < 1.0f ? FMath::SmoothStep(0.0f, 1.0f, Growth) * Data->Height * GrowMaskOvershoot : FullyGrownHeight;
	if (bWithering)
	{
		GrowHeight = (1.0f - Wilt) * Data->Height * Giant * GrowMaskOvershoot;
	}

	FlashStrength = FMath::Max(0.0f, FlashStrength - DeltaSeconds * FlashFade);
	for (UMaterialInstanceDynamic* Material : Materials)
	{
		Material->SetScalarParameterValue(GrowHeightParameter, GrowHeight);
		Material->SetScalarParameterValue(FlashParameter, FlashStrength + (IsActing() ? 0.05f + 0.04f * FMath::Sin(Age * 7.0f) : 0.0f));
		Material->SetScalarParameterValue(PlantHeightParameter, Data->Height * Giant);
	}

	// Size: pops up while it grows, swinging a little beyond its size; squashes and leans for its actions; sinks when it withers
	SquashAmount = FMath::FInterpTo(SquashAmount, 1.0f, DeltaSeconds, SquashRecovery);
	LeanAmount = FMath::FInterpTo(LeanAmount, 0.0f, DeltaSeconds, SquashRecovery);

	if (GrowingPart != nullptr)
	{
		const float Pop = Growth < 1.0f ? FMath::Lerp(GrowStartScale, 1.0f, EaseOutBack(Growth)) : 1.0f;
		const float Sink = 1.0f - 0.35f * Wilt;
		const float Wide = 1.0f / FMath::Sqrt(FMath::Max(SquashAmount, 0.1f));
		GrowingPart->SetRelativeScale3D(GrownScale * Pop * Sink * Giant * FVector(Wide, Wide, SquashAmount));

		FRotator Rotation = GrowingPart->GetRelativeRotation();
		Rotation.Pitch = -LeanAmount;
		GrowingPart->SetRelativeRotation(Rotation);
	}
}

void AVaelPlant::Flash(float Seconds, float Strength)
{
	FlashStrength = FMath::Max(FlashStrength, Strength);
	FlashFade = Strength / FMath::Max(Seconds, 0.05f);
}

void AVaelPlant::Squash(float Amount, float LeanDegrees)
{
	SquashAmount = Amount;
	LeanAmount = LeanDegrees;
}

void AVaelPlant::Wither()
{
	if (bWithering)
	{
		return;
	}

	bWithering = true;
	WitherTime = 0.0f;
	if (bWild)
	{
		return;
	}

	// Dry leaves fall off
	FVaelDebris Leaves;
	Leaves.Count = 12;
	Leaves.Spread = 30.0f;
	Leaves.Speed = 90.0f;
	Leaves.Lift = 120.0f;
	Leaves.Gravity = 300.0f;
	Leaves.Drag = 2.5f;
	Leaves.Lifetime = 1.1f;
	Leaves.MinSize = 3.0f;
	Leaves.MaxSize = 7.0f;
	Leaves.Color = FLinearColor(0.16f, 0.1f, 0.05f);
	AVaelDebrisBurst::Spawn(this, FVector(GetActorLocation().X, GetActorLocation().Y, GroundZ + (Data != nullptr ? Data->Height : 80.0f) * 0.6f), Leaves);
}

void AVaelPlant::OnHealthChanged(float OldValue, float NewValue)
{
	Super::OnHealthChanged(OldValue, NewValue);

	if (NewValue >= OldValue || bWithering)
	{
		return;
	}

	if (NewValue <= 0.0f)
	{
		UE_LOG(LogVael, Log, TEXT("%s of '%s' burns down"), Data != nullptr ? *Data->GetName(bShadow).ToString() : TEXT("plant"), *GetNameSafe(Gardener.Get()));
		Wither();
		return;
	}

	Squash(0.9f);
}

float AVaelPlant::GetIncomingDamageMultiplier(const FGameplayTagContainer& DamageTags) const
{
	// Only the fire of enemies harms a plant; other players can't hurt it anyway
	return !bWild && DamageTags.HasTag(VaelTags::GetElementTag(EVaelElement::Fire)) ? 1.0f : 0.0f;
}

AVaelCreature* AVaelPlant::FindEnemy(float Range) const
{
	AVaelCreature* Nearest = nullptr;
	float NearestDistance = Range;

	for (TActorIterator<AVaelCreature> It(GetWorld()); It; ++It)
	{
		const float Distance = FVector::Dist2D(It->GetActorLocation(), GetActorLocation());
		if (!It->IsDead() && !It->IsOnPlayerSide() && Distance < NearestDistance)
		{
			Nearest = *It;
			NearestDistance = Distance;
		}
	}

	return Nearest;
}

TArray<AVaelCreature*> AVaelPlant::FindEnemies(float Radius) const
{
	TArray<AVaelCreature*> Enemies;
	for (TActorIterator<AVaelCreature> It(GetWorld()); It; ++It)
	{
		if (!It->IsDead() && !It->IsOnPlayerSide() && FVector::Dist2D(It->GetActorLocation(), GetActorLocation()) < Radius)
		{
			Enemies.Add(*It);
		}
	}
	return Enemies;
}

TArray<AVaelCharacter*> AVaelPlant::FindAllies(float Radius) const
{
	TArray<AVaelCharacter*> Allies;
	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		AVaelCharacter* Player = It->Get() != nullptr ? It->Get()->GetPawn<AVaelCharacter>() : nullptr;
		if (Player != nullptr && !Player->IsDowned() && FVector::Dist2D(Player->GetActorLocation(), GetActorLocation()) < Radius)
		{
			Allies.Add(Player);
		}
	}
	return Allies;
}

void AVaelPlant::Puff(const FLinearColor& Color, float Spread, int32 Count, float Lift) const
{
	FVaelDebris Drops;
	Drops.Count = Count;
	Drops.Spread = Spread;
	Drops.Speed = 30.0f;
	Drops.Lift = Lift;
	Drops.Gravity = -140.0f;
	Drops.Drag = 1.0f;
	Drops.Lifetime = 1.0f;
	Drops.MinSize = 4.0f;
	Drops.MaxSize = 8.0f;
	Drops.Color = Color;
	Drops.Glow = 2.5f;
	Drops.bSoft = true;
	Drops.bLandOnGround = false;
	AVaelDebrisBurst::Spawn(this, FVector(GetActorLocation().X, GetActorLocation().Y, GroundZ + 15.0f), Drops);
}

void AVaelPlant::TickCall(float DeltaSeconds)
{
	const UVaelFloromantSettings* Settings = UVaelFloromantSettings::Get();
	const EVaelPlantEffect Effect = GetActiveEffect();
	const float Power = GetPower();
	const float Radius = Data->Radius * FMath::Lerp(1.0f, Settings->GiantScale, GiantAmount * 0.5f);
	const FVector Ground(GetActorLocation().X, GetActorLocation().Y, GroundZ);
	AVaelCharacter* Floromant = Gardener.Get();

	// Effects that happen once at the start of the call
	if (!bCallStarted)
	{
		bCallStarted = true;
		switch (Effect)
		{
		case EVaelPlantEffect::VineWall:
		{
			// Across the way to the nearest enemy, or ahead of the Floromant
			const AActor* Enemy = FindEnemy(Radius * 2.0f);
			const FVector Toward = Enemy != nullptr ? Enemy->GetActorLocation() - GetActorLocation() : (Floromant != nullptr ? Floromant->GetActorForwardVector() : GetActorForwardVector());
			const FVector Direction = Toward.GetSafeNormal2D();
			AVaelVineWall::Raise(GetWorld(), Ground + Direction * 160.0f, Direction.Rotation().Yaw, Settings->WallLifetime * FMath::Max(1.0f, CallTimeLeft / Settings->CallDuration),
				FMath::Lerp(1.0f, Settings->GiantScale, GiantAmount), this);
			++PulseCount;
			break;
		}
		case EVaelPlantEffect::Smoke:
			AVaelGroundArea::SpawnArea(GetWorld(), Ground, EVaelElement::Water, Radius * 0.7f, CallTimeLeft + 1.0f, 0.0f, this, false, EVaelGroundEffect::Blind);
			++PulseCount;
			break;
		case EVaelPlantEffect::PoisonPool:
			AVaelGroundArea::SpawnArea(GetWorld(), Ground, EVaelElement::Earth, Radius * 0.5f, CallTimeLeft + 2.0f, Power, this, false, EVaelGroundEffect::Slow);
			++PulseCount;
			break;
		case EVaelPlantEffect::FireBurst:
			Flash(0.5f, 2.0f);
			Squash(0.75f);
			PulseCountdown = 0.5f;
			break;
		default:
			break;
		}
	}

	// Shooting plants turn to their enemy, glow up and duck before each shot
	if (Effect == EVaelPlantEffect::FireSeeds || Effect == EVaelPlantEffect::PollenDarts)
	{
		const bool bFire = Effect == EVaelPlantEffect::FireSeeds;
		const float Interval = bFire ? FireSeedInterval : PollenInterval;
		const float Windup = bFire ? FireSeedWindup : PollenWindup;
		PulseCountdown -= DeltaSeconds;

		AActor* Target = bWindingUp ? WindupTarget.Get() : FindEnemy(Radius * 2.2f);
		if (Target != nullptr)
		{
			const float TargetYaw = (Target->GetActorLocation() - GetActorLocation()).Rotation().Yaw;
			SetActorRotation(FRotator(0.0f, FMath::FixedTurn(GetActorRotation().Yaw, TargetYaw, PlantTurnSpeed * DeltaSeconds), 0.0f));
		}

		if (!bWindingUp)
		{
			if (PulseCountdown > Windup || Target == nullptr)
			{
				PulseCountdown = FMath::Max(PulseCountdown, Target == nullptr ? Windup : 0.0f);
				return;
			}
			bWindingUp = true;
			WindupTarget = Target;
			Flash(Windup + 0.15f, 1.3f);
			Squash(0.8f);
			return;
		}

		if (PulseCountdown > 0.0f)
		{
			return;
		}
		bWindingUp = false;
		PulseCountdown = Interval;
		if (Target != nullptr)
		{
			Shoot(Target, bFire);
		}
		return;
	}

	// Everything else pulses around the plant
	PulseCountdown -= DeltaSeconds;
	if (PulseCountdown > 0.0f)
	{
		return;
	}
	PulseCountdown = PulseInterval;

	const TArray<AVaelCreature*> Enemies = FindEnemies(Radius);
	const TArray<AVaelCharacter*> Allies = FindAllies(Radius);
	bool bReached = false;

	switch (Effect)
	{
	case EVaelPlantEffect::BlindAndSaft:
		for (AVaelCreature* Enemy : Enemies)
		{
			Enemy->ApplyBlind(1.6f);
		}
		if (Floromant != nullptr)
		{
			UAbilitySystemComponent* AbilitySystem = Floromant->GetAbilitySystemComponent();
			AbilitySystem->SetNumericAttributeBase(UVaelAttributeSet::GetManaAttribute(), FMath::Min(Floromant->GetMaxMana(), Floromant->GetMana() + Power * 0.5f));
		}
		bReached = true;
		break;
	case EVaelPlantEffect::WetAndSlow:
		for (AVaelCreature* Enemy : Enemies)
		{
			UVaelCombatStatics::ApplyStatus(this, Enemy, EVaelStatus::Wet, 4.0f);
			UVaelCombatStatics::ApplyStatus(this, Enemy, EVaelStatus::Slowed, 2.0f);
		}
		bReached = !Enemies.IsEmpty();
		break;
	case EVaelPlantEffect::HealAllies:
		for (AVaelCharacter* Ally : Allies)
		{
			UVaelCombatStatics::Heal(Ally, Power);
		}
		bReached = !Allies.IsEmpty();
		break;
	case EVaelPlantEffect::Cleanse:
		for (AVaelCharacter* Ally : Allies)
		{
			for (const EVaelStatus Status : { EVaelStatus::Burning, EVaelStatus::Wet, EVaelStatus::Frozen, EVaelStatus::Slowed })
			{
				UVaelCombatStatics::RemoveStatus(Ally, Status);
			}
			UVaelCombatStatics::Heal(Ally, Power * 0.3f);
		}
		bReached = !Allies.IsEmpty();
		break;
	case EVaelPlantEffect::FireBurst:
	{
		FVaelSpellHit Hit;
		Hit.Damage = Power * 2.5f;
		Hit.Element = EVaelElement::Fire;
		Hit.Knockback = 400.0f;
		UVaelCombatStatics::ApplySpellHitInRadius(this, GetActorLocation(), Radius * 0.6f, Hit);
		AVaelGroundArea::SpawnArea(GetWorld(), Ground, EVaelElement::Fire, Radius * 0.45f, 5.0f, Power * 0.6f, this);
		FVaelDebris Sparks;
		Sparks.Count = 24;
		Sparks.Spread = 30.0f;
		Sparks.Speed = 520.0f;
		Sparks.Lift = 300.0f;
		Sparks.Lifetime = 0.7f;
		Sparks.MinSize = 3.0f;
		Sparks.MaxSize = 8.0f;
		Sparks.Color = FLinearColor(1.0f, 0.45f, 0.1f);
		Sparks.Glow = 5.0f;
		Sparks.bSoft = true;
		AVaelDebrisBurst::Spawn(this, Ground + FVector(0.0f, 0.0f, 40.0f), Sparks);
		++PulseCount;
		CallTimeLeft = 0.0f;
		Wither();
		return;
	}
	case EVaelPlantEffect::Confuse:
		for (AVaelCreature* Enemy : Enemies)
		{
			UVaelCombatStatics::ApplyStatus(this, Enemy, EVaelStatus::Fevered, 3.0f);
		}
		bReached = !Enemies.IsEmpty();
		break;
	case EVaelPlantEffect::Hold:
		for (AVaelCreature* Enemy : FindEnemies(Radius * 0.6f))
		{
			Enemy->ApplyStun(1.1f);
			UVaelCombatStatics::DealDamage(this, Enemy, Power * 0.3f, EVaelElement::Earth);
			bReached = true;
		}
		break;
	case EVaelPlantEffect::LifeSteal:
		for (AVaelCreature* Enemy : Enemies)
		{
			UVaelCombatStatics::DealDamage(this, Enemy, Power, EVaelElement::Earth);
			if (Floromant != nullptr)
			{
				UVaelCombatStatics::Heal(Floromant, Power * 0.6f);
			}
		}
		bReached = !Enemies.IsEmpty();
		break;
	case EVaelPlantEffect::Weaken:
		for (AVaelCreature* Enemy : Enemies)
		{
			UVaelCombatStatics::ApplyStatus(this, Enemy, EVaelStatus::Marked, 4.0f);
		}
		bReached = !Enemies.IsEmpty();
		break;
	case EVaelPlantEffect::Sleep:
		for (AVaelCreature* Enemy : FindEnemies(Radius * 0.7f))
		{
			Enemy->ApplyStun(2.5f);
			bReached = true;
		}
		PulseCountdown = 2.0f;
		break;
	default:
		break;
	}

	PulseCount += bReached ? 1 : 0;
	if (!bWild)
	{
		Flash(0.5f, bReached ? 0.45f : 0.2f);
		Squash(1.07f);
	}
	Puff(GetColor(), Radius * 0.8f, 12, 110.0f);
}

void AVaelPlant::Shoot(AActor* Target, bool bFire)
{
	// Snaps forwards and spits from its head, flying flat at the height of the head
	Squash(1.18f, 14.0f);

	const float Height = (Data != nullptr ? Data->Height : 80.0f) * FMath::Lerp(1.0f, UVaelFloromantSettings::Get()->GiantScale, GiantAmount);
	const FVector Muzzle = FVector(GetActorLocation().X, GetActorLocation().Y, GroundZ + FMath::Min(Height * 0.8f, 140.0f)) + GetActorForwardVector() * 25.0f;
	FVector Direction = Target->GetActorLocation() - Muzzle;
	Direction.Z = 0.0f;
	if (Direction.IsNearlyZero())
	{
		return;
	}

	FVaelSpellHit Hit;
	Hit.Damage = GetPower();
	Hit.Element = bFire ? EVaelElement::Fire : EVaelElement::Air;

	const FTransform SpawnTransform(Direction.Rotation(), Muzzle);
	AVaelSpellProjectile* Shot = GetWorld()->SpawnActorDeferred<AVaelSpellProjectile>(AVaelSpellProjectile::StaticClass(), SpawnTransform, this, this, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (Shot == nullptr)
	{
		return;
	}

	const float Range = (Data != nullptr ? Data->Radius : 400.0f) * 2.4f;
	Shot->InitSpell(Hit, ShotSpeed, bFire ? 14.0f : 9.0f, Range / ShotSpeed, 0, GetColor());
	if (bFire)
	{
		FVaelSpellHit Burst;
		Burst.Damage = GetPower() * 0.8f;
		Burst.Element = EVaelElement::Fire;
		Shot->SetExplosion(Burst, FireSeedBurstRadius);
	}
	Shot->SetEffects(VaelEffects::Load(Hit.Element));
	Shot->FinishSpawning(SpawnTransform);
	++ShotCount;
}

// ---------------------------------------------------------------- Vine wall

AVaelVineWall::AVaelVineWall()
{
	// Holds back pawns only; players are let through one by one, shots and the camera pass anyway
	Barrier = CreateDefaultSubobject<UBoxComponent>(TEXT("Barrier"));
	RootComponent = Barrier;
	Barrier->SetCollisionObjectType(ECC_WorldDynamic);
	Barrier->SetCollisionResponseToAllChannels(ECR_Ignore);
	Barrier->SetCollisionResponseToChannel(ECC_Pawn, ECR_Block);
	Barrier->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Barrier->CanCharacterStepUpOn = ECB_No;
	Barrier->SetMobility(EComponentMobility::Movable);

	Roots = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("Roots"));
	Roots->SetupAttachment(RootComponent);
	Roots->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	PrimaryActorTick.bCanEverTick = true;
}

AVaelVineWall* AVaelVineWall::Raise(UWorld* World, const FVector& GroundLocation, float Yaw, float InLifetime, float Scale, APawn* InInstigator)
{
	if (World == nullptr)
	{
		return nullptr;
	}

	FActorSpawnParameters Parameters;
	Parameters.Instigator = InInstigator;
	Parameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AVaelVineWall* Wall = World->SpawnActor<AVaelVineWall>(GroundLocation, FRotator(0.0f, Yaw, 0.0f), Parameters);
	if (Wall != nullptr)
	{
		Wall->GroundZ = GroundLocation.Z;
		Wall->Lifetime = InLifetime;
		Wall->Build(Scale);
	}
	return Wall;
}

void AVaelVineWall::Build(float Scale)
{
	const UVaelFloromantSettings* Settings = UVaelFloromantSettings::Get();
	const float Length = Settings->WallLength * Scale;
	const float Height = Settings->WallHeight * Scale;
	const float Thickness = Settings->WallThickness;

	// The wall stands across the yaw: along the right of the actor
	Barrier->SetBoxExtent(FVector(Thickness * 0.5f, Length * 0.5f, Height * 0.5f));
	Roots->SetRelativeLocation(FVector(0.0f, 0.0f, -Height * 0.5f));
	SetActorLocation(GetActorLocation() + FVector(0.0f, 0.0f, Height * 0.5f));

	// Vines break out of the ground: arches that cross each other and spikes that rise and curl
	TArray<FVector> Vertices;
	TArray<FVector> Normals;
	TArray<FVector2D> UVs;
	TArray<int32> Triangles;

	const int32 NumRoots = Settings->WallRoots;
	for (int32 RootIndex = 0; RootIndex < NumRoots; ++RootIndex)
	{
		const bool bSpike = RootIndex % 3 == 1;
		const float Place = NumRoots > 1 ? RootIndex / static_cast<float>(NumRoots - 1) : 0.5f;
		const float CenterY = (Place - 0.5f) * Length * 0.9f;
		const float Span = Length * FMath::FRandRange(0.14f, 0.3f);
		const float Top = Height * FMath::FRandRange(bSpike ? 0.7f : 0.3f, bSpike ? 1.05f : 0.8f);
		const float Side = FMath::FRandRange(-0.35f, 0.35f) * Thickness;
		const float Seed = FMath::FRandRange(0.0f, 100.0f);
		const float Direction = FMath::RandBool() ? 1.0f : -1.0f;

		VaelRoots::AddRoot(Vertices, Normals, UVs, Triangles, [&](float Along)
		{
			if (bSpike)
			{
				const float Curl = FMath::Square(Along) * 0.35f * Top * Direction;
				return FVector(Side + FMath::Sin(Along * 5.0f + Seed) * 6.0f, CenterY + Curl, Along * Top);
			}
			const float Y = CenterY + (Along - 0.5f) * Span * Direction;
			const float Z = FMath::Sin(Along * UE_PI) * Top;
			return FVector(Side + FMath::Sin(Along * 7.0f + Seed) * 8.0f + (Along - 0.5f) * 14.0f, Y, Z - 8.0f);
		}, FMath::FRandRange(bSpike ? 9.0f : 8.0f, bSpike ? 15.0f : 13.0f) * FMath::Sqrt(Scale), bSpike, Seed);
	}

	Roots->CreateMeshSection_LinearColor(0, Vertices, Triangles, Normals, UVs, TArray<FLinearColor>(), TArray<FProcMeshTangent>(), false);
	Material = MakeRootMaterial(this, GroundZ, Height);
	Roots->SetMaterial(0, Material);
}

void AVaelVineWall::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	Age += DeltaSeconds;
	const float Grow = FMath::Clamp(Age / 0.6f, 0.0f, 1.0f);
	const float Wilt = FMath::Clamp((Age - Lifetime) / 0.7f, 0.0f, 1.0f);
	const float Height = UVaelFloromantSettings::Get()->WallHeight * 2.0f;

	if (Material != nullptr)
	{
		Material->SetScalarParameterValue(GrowHeightParameter, Wilt > 0.0f ? (1.0f - Wilt) * Height : Grow < 1.0f ? Grow * Height : FullyGrownHeight);
	}

	// Holds once it has grown, lets go when it withers
	if (Grow >= 1.0f && Wilt <= 0.0f && Barrier->GetCollisionEnabled() == ECollisionEnabled::NoCollision)
	{
		LetPlayersThrough();
		Barrier->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	}
	if (Wilt > 0.0f)
	{
		Barrier->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}
	if (Wilt >= 1.0f)
	{
		Destroy();
		return;
	}

	PassCountdown -= DeltaSeconds;
	if (PassCountdown <= 0.0f)
	{
		PassCountdown = 1.0f;
		LetPlayersThrough();
	}
}

bool AVaelVineWall::IsHolding() const
{
	return Barrier->GetCollisionEnabled() != ECollisionEnabled::NoCollision;
}

void AVaelVineWall::LetPlayersThrough()
{
	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		if (const AVaelCharacter* Player = It->Get() != nullptr ? It->Get()->GetPawn<AVaelCharacter>() : nullptr)
		{
			Player->GetCapsuleComponent()->IgnoreActorWhenMoving(this, true);
		}
	}
}

// ---------------------------------------------------------------- Root grip

AVaelRootGrip::AVaelRootGrip()
{
	Roots = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("Roots"));
	RootComponent = Roots;
	Roots->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	PrimaryActorTick.bCanEverTick = true;
}

AVaelRootGrip* AVaelRootGrip::Spawn(AVaelCharacter* InCaster, const FVector& GroundLocation)
{
	UWorld* World = InCaster != nullptr ? InCaster->GetWorld() : nullptr;
	if (World == nullptr)
	{
		return nullptr;
	}

	FActorSpawnParameters Parameters;
	Parameters.Instigator = InCaster;
	Parameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AVaelRootGrip* Grip = World->SpawnActor<AVaelRootGrip>(GroundLocation, FRotator(0.0f, FMath::FRandRange(0.0f, 360.0f), 0.0f), Parameters);
	if (Grip != nullptr)
	{
		Grip->Caster = InCaster;
		Grip->GroundZ = GroundLocation.Z;
		Grip->Build();
	}
	return Grip;
}

void AVaelRootGrip::Build()
{
	// Roots around the spot that rise and curl inwards over it, like fingers closing
	const float Radius = UVaelFloromantSettings::Get()->GripRadius;
	TArray<FVector> Vertices;
	TArray<FVector> Normals;
	TArray<FVector2D> UVs;
	TArray<int32> Triangles;

	constexpr int32 NumRoots = 9;
	for (int32 RootIndex = 0; RootIndex < NumRoots; ++RootIndex)
	{
		const float Angle = RootIndex * 2.0f * UE_PI / NumRoots + FMath::FRandRange(-0.2f, 0.2f);
		const FVector Out(FMath::Cos(Angle), FMath::Sin(Angle), 0.0f);
		const float Reach = Radius * FMath::FRandRange(0.55f, 0.95f);
		const float Top = FMath::FRandRange(110.0f, 170.0f);
		const float Seed = FMath::FRandRange(0.0f, 100.0f);
		VaelRoots::AddRoot(Vertices, Normals, UVs, Triangles, [&](float Along)
		{
			// Out of the ground at the rim, up and over towards the middle
			const float Inward = FMath::Lerp(Reach, Reach * 0.15f, FMath::Square(Along));
			return Out * Inward + FVector(0.0f, 0.0f, FMath::Sin(Along * UE_PI * 0.85f) * Top) + FVector(FMath::Sin(Along * 6.0f + Seed) * 6.0f, 0.0f, 0.0f);
		}, FMath::FRandRange(9.0f, 14.0f), true, Seed);
	}

	Roots->CreateMeshSection_LinearColor(0, Vertices, Triangles, Normals, UVs, TArray<FLinearColor>(), TArray<FProcMeshTangent>(), false);
	Material = MakeRootMaterial(this, GroundZ, 170.0f);
	Roots->SetMaterial(0, Material);

	FVaelDebris Earth;
	Earth.Count = 22;
	Earth.Spread = Radius * 0.8f;
	Earth.Speed = 200.0f;
	Earth.Lift = 420.0f;
	Earth.Lifetime = 0.8f;
	Earth.Color = FLinearColor(0.11f, 0.075f, 0.05f);
	Earth.RockShare = 0.2f;
	AVaelDebrisBurst::Spawn(this, GetActorLocation(), Earth);
}

void AVaelRootGrip::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	const UVaelFloromantSettings* Settings = UVaelFloromantSettings::Get();
	Age += DeltaSeconds;
	const float Grow = FMath::Clamp(Age / 0.3f, 0.0f, 1.0f);
	const float Wilt = FMath::Clamp((Age - Settings->GripDuration) / 0.5f, 0.0f, 1.0f);
	if (Material != nullptr)
	{
		Material->SetScalarParameterValue(GrowHeightParameter, Wilt > 0.0f ? (1.0f - Wilt) * 200.0f : Grow < 1.0f ? Grow * 200.0f : FullyGrownHeight);
	}
	if (Wilt >= 1.0f)
	{
		Destroy();
		return;
	}

	// Holds and chokes every enemy inside, a little each half second
	TickCountdown -= DeltaSeconds;
	if (Wilt > 0.0f || TickCountdown > 0.0f)
	{
		return;
	}
	TickCountdown = 0.5f;

	AActor* Attacker = Caster.IsValid() ? static_cast<AActor*>(Caster.Get()) : this;
	for (TActorIterator<AVaelCreature> It(GetWorld()); It; ++It)
	{
		if (!It->IsDead() && !It->IsOnPlayerSide() && FVector::Dist2D(It->GetActorLocation(), GetActorLocation()) < Settings->GripRadius)
		{
			It->ApplyStun(0.6f);
			UVaelCombatStatics::DealDamage(Attacker, *It, Settings->GripDamagePerSecond * 0.5f, EVaelElement::Earth);
			++HeldCount;
		}
	}
}

// ---------------------------------------------------------------- Thorn whip

AVaelThornWhip::AVaelThornWhip()
{
	Vine = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("Vine"));
	RootComponent = Vine;
	Vine->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Vine->SetUsingAbsoluteLocation(true);
	Vine->SetUsingAbsoluteRotation(true);
	PrimaryActorTick.bCanEverTick = true;
}

AVaelThornWhip* AVaelThornWhip::Lash(AActor* Caster, const FVector& From, const FVector& To)
{
	UWorld* World = Caster != nullptr ? Caster->GetWorld() : nullptr;
	if (World == nullptr)
	{
		return nullptr;
	}

	AVaelThornWhip* Whip = World->SpawnActor<AVaelThornWhip>(From, FRotator::ZeroRotator);
	if (Whip != nullptr)
	{
		Whip->Start = From;
		Whip->End = To;
		Whip->Seed = FMath::FRandRange(0.0f, 100.0f);
		Whip->Material = MakeRootMaterial(Whip, From.Z - 200.0f, 400.0f);
		// Fresh green with pink thorns instead of the brown of old roots
		Whip->Material->SetVectorParameterValue(TintParameter, FLinearColor(0.18f, 0.32f, 0.08f));
		Whip->Material->SetVectorParameterValue(GrowColorParameter, FLinearColor(1.0f, 0.4f, 0.7f));
		Whip->Rebuild();
		Whip->SetLifeSpan(0.45f);
	}
	return Whip;
}

void AVaelThornWhip::Rebuild()
{
	// Lashes out fast and pulls back a little slower; a wave runs along it
	const float Out = FMath::Clamp(Age / 0.12f, 0.0f, 1.0f);
	const float Back = FMath::Clamp((Age - 0.18f) / 0.25f, 0.0f, 1.0f);
	const float Reach = FMath::Max(0.05f, Out * (1.0f - Back));
	const FVector Side = FVector::CrossProduct(End - Start, FVector::UpVector).GetSafeNormal();

	TArray<FVector> Vertices;
	TArray<FVector> Normals;
	TArray<FVector2D> UVs;
	TArray<int32> Triangles;
	VaelRoots::AddRoot(Vertices, Normals, UVs, Triangles, [&](float Along)
	{
		const float T = Along * Reach;
		const float Wave = FMath::Sin(T * UE_PI * 3.0f - Age * 40.0f) * 20.0f * FMath::Sin(T * UE_PI);
		return FMath::Lerp(Start, End, T) + Side * Wave + FVector(0.0f, 0.0f, FMath::Sin(T * UE_PI) * 40.0f);
	}, 5.0f, true, Seed);

	if (Vine->GetNumSections() == 0)
	{
		Vine->CreateMeshSection_LinearColor(0, Vertices, Triangles, Normals, UVs, TArray<FLinearColor>(), TArray<FProcMeshTangent>(), false);
		Vine->SetMaterial(0, Material);
	}
	else
	{
		Vine->UpdateMeshSection_LinearColor(0, Vertices, Normals, UVs, TArray<FLinearColor>(), TArray<FProcMeshTangent>());
	}
}

void AVaelThornWhip::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	Age += DeltaSeconds;
	Rebuild();
}
