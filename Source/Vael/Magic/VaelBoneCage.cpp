// Copyright Epic Games, Inc. All Rights Reserved.

#include "Magic/VaelBoneCage.h"
#include "Vael.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "Magic/VaelDebrisBurst.h"
#include "Magic/VaelMagicSettings.h"
#include "Magic/VaelSpellEffects.h"
#include "World/VaelGround.h"

namespace
{
	/** Bones of a cage; seconds they take to grow and to crumble */
	constexpr int32 CageBones = 9;
	constexpr float CageGrowTime = 0.22f;
	constexpr float CageCrumbleTime = 0.3f;

	/** Size of the engine cone and the engine sphere in cm */
	constexpr float CageShapeSize = 100.0f;

	/** How wide a bone is at its foot, in cm */
	constexpr float CageBoneWidth = 24.0f;

	const FLinearColor CageBone(0.82f, 0.76f, 0.62f);
}

AVaelBoneCage::AVaelBoneCage()
{
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	PrimaryActorTick.bCanEverTick = true;
}

AVaelBoneCage* AVaelBoneCage::Spawn(AActor* Caster, const AActor* Target, float Seconds)
{
	UWorld* World = Caster != nullptr ? Caster->GetWorld() : nullptr;
	if (World == nullptr || Target == nullptr || Seconds <= 0.0f)
	{
		return nullptr;
	}

	FVector Origin;
	FVector Extent;
	Target->GetActorBounds(true, Origin, Extent);

	FVector Foot = Target->GetActorLocation();
	FHitResult GroundHit;
	if (VaelGround::TraceGround(World, Foot + FVector(0.0f, 0.0f, 150.0f), Foot - FVector(0.0f, 0.0f, 400.0f), GroundHit, Target))
	{
		Foot = GroundHit.Location;
	}

	const FTransform SpawnTransform(Foot);
	AVaelBoneCage* Cage = World->SpawnActorDeferred<AVaelBoneCage>(AVaelBoneCage::StaticClass(), SpawnTransform, Caster, Cast<APawn>(Caster), ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (Cage != nullptr)
	{
		Cage->CageRadius = FMath::Clamp(FMath::Max(Extent.X, Extent.Y) * 1.05f, 70.0f, 260.0f);
		Cage->CageHeight = FMath::Clamp(Extent.Z * 2.1f, 150.0f, 420.0f);
		Cage->Seconds = Seconds;
		UE_LOG(LogVael, Log, TEXT("Bone cage around %s for %.1f s"), *GetNameSafe(Target), Seconds);
		Cage->FinishSpawning(SpawnTransform);
	}
	return Cage;
}

void AVaelBoneCage::BeginPlay()
{
	Super::BeginPlay();

	UMaterialInterface* CoreMaterial = nullptr;
	UMaterialInterface* GlowMaterial = nullptr;
	VaelEffects::LoadLookMaterials(CoreMaterial, GlowMaterial);
	UStaticMesh* Cone = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cone.Cone"));
	const FLinearColor Mark = UVaelMagicSettings::Get()->GetElementColor(EVaelElement::Mark);

	for (int32 BoneIndex = 0; BoneIndex < CageBones; ++BoneIndex)
	{
		for (TArray<TObjectPtr<UStaticMeshComponent>>* Parts : { &Lower, &Upper })
		{
			UStaticMeshComponent* Part = VaelEffects::AddLookShape(this, CoreMaterial, CageBone, 0.0f);
			Part->SetStaticMesh(Cone);
			Part->SetVisibility(true);
			Parts->Add(Part);
		}
		Glows.Add(VaelEffects::AddLookShape(this, GlowMaterial, Mark, 4.0f));
		Glows.Last()->SetVisibility(true);
	}

	Tick(0.0f);
}

void AVaelBoneCage::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	Age += DeltaSeconds;
	if (Age >= Seconds + CageCrumbleTime)
	{
		FVaelDebris Shards;
		Shards.Count = 14;
		Shards.Spread = CageRadius * 0.7f;
		Shards.Speed = 200.0f;
		Shards.Lift = 240.0f;
		Shards.Lifetime = 0.6f;
		Shards.MinSize = 5.0f;
		Shards.MaxSize = 12.0f;
		Shards.Color = CageBone;
		AVaelDebrisBurst::Spawn(this, GetActorLocation() + FVector(0.0f, 0.0f, CageHeight * 0.4f), Shards);
		Destroy();
		return;
	}

	// Each bone a little after the one before; at the end they sink back
	for (int32 BoneIndex = 0; BoneIndex < Lower.Num(); ++BoneIndex)
	{
		const float Delay = BoneIndex * 0.012f;
		const float Grown = FMath::InterpEaseOut(0.0f, 1.0f, FMath::Clamp((Age - Delay) / CageGrowTime, 0.0f, 1.0f), 3.0f)
			* (1.0f - FMath::Clamp((Age - Seconds) / CageCrumbleTime, 0.0f, 1.0f));

		const float Angle = BoneIndex * UE_TWO_PI / Lower.Num();
		const FVector Out(FMath::Cos(Angle), FMath::Sin(Angle), 0.0f);
		const FVector Foot = Out * CageRadius * 1.05f;

		// Up and a little outward, then bent in over the head
		const FVector Knee = Foot + (Out * 0.12f + FVector::UpVector).GetSafeNormal() * CageHeight * 0.62f * Grown;
		const FVector Tip = Knee + (-Out * 0.85f + FVector::UpVector * 0.55f).GetSafeNormal() * CageHeight * 0.52f * Grown;

		const auto PlaceBone = [](UStaticMeshComponent* Part, const FVector& From, const FVector& To, float Width)
		{
			const float Length = FMath::Max(FVector::Dist(From, To), 0.1f);
			Part->SetRelativeLocation((From + To) * 0.5f);
			Part->SetRelativeRotation(FRotationMatrix::MakeFromZ(To - From).Rotator());
			Part->SetRelativeScale3D(FVector(Width, Width, Length) / CageShapeSize);
		};

		// A cone narrows upward: the knee is a slim joint, the tip a claw
		PlaceBone(Lower[BoneIndex], Foot, Knee + (Knee - Foot) * 0.1f, CageBoneWidth);
		PlaceBone(Upper[BoneIndex], Knee, Tip, CageBoneWidth * 0.62f);

		Glows[BoneIndex]->SetRelativeLocation(Foot + FVector(0.0f, 0.0f, 4.0f));
		Glows[BoneIndex]->SetRelativeScale3D(FVector(0.55f, 0.55f, 0.08f) * Grown);
	}
}
