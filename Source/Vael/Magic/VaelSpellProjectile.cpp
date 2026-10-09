// Copyright Epic Games, Inc. All Rights Reserved.

#include "Magic/VaelSpellProjectile.h"
#include "UObject/ConstructorHelpers.h"
#include "Components/PointLightComponent.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Combat/VaelHitFeedbackSubsystem.h"
#include "DrawDebugHelpers.h"
#include "Engine/World.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "Magic/VaelGroundArea.h"
#include "Magic/VaelMagicSettings.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "NiagaraSystem.h"
#include "VaelAssets.h"
#include "World/VaelGround.h"

namespace
{
	/** Radius of the engine sphere mesh used as placeholder */
	constexpr float PlaceholderSphereRadius = 50.0f;

	/** Camera shake of a spell explosion, as in the prototype */
	constexpr float ExplosionShake = 0.2f;

	/** The spark is this many times longer and thinner than the collision radius of its projectile */
	constexpr float SparkLengthShare = 2.4f;
	constexpr float SparkThicknessShare = 0.42f;

	/** Pieces of the tail behind the spark */
	constexpr int32 SparkTailCount = 3;

	/** Embers a spark can have in the air at once, seconds between two and seconds each one glows */
	constexpr int32 SparkEmberCount = 12;
	constexpr float SparkEmberInterval = 0.055f;
	constexpr float SparkEmberLifetime = 0.55f;

	/** Embers start this fast sideways, keep a share of the flight speed and fall, in cm/s and cm/s² */
	constexpr float SparkEmberSpread = 110.0f;
	constexpr float SparkEmberForwardShare = 0.12f;
	constexpr float SparkEmberGravity = 520.0f;

	/** Embers a hit throws out on top of those already in the air */
	constexpr int32 SparkImpactEmbers = 7;

	/** The ball of water is this much smaller than the collision radius of its projectile, and swells and shrinks by this share */
	constexpr float WaterOrbSizeShare = 0.92f;
	constexpr float WaterOrbWobble = 0.06f;

	/** Drops a ball of water bursts into, how fast they shoot out in cm/s, how they fall in cm/s² and how long they last */
	constexpr int32 WaterBurstDropCount = 18;
	constexpr float WaterBurstSpeed = 1050.0f;
	constexpr float WaterBurstGravity = 950.0f;
	constexpr float WaterBurstDropLifetime = 0.5f;

	/** The ring of spray grows to this many times the ball within this time; its glow, the flash of the light and the shake of the camera */
	constexpr float WaterBurstRingGrowth = 4.2f;
	constexpr float WaterBurstRingTime = 0.2f;
	constexpr float WaterBurstGlow = 3.0f;
	constexpr float WaterBurstFlash = 5.0f;
	constexpr float WaterBurstShake = 0.12f;

	/** The pointed stone: number of rocks it is made of, its length and thickness as shares of the collision radius, its spin in degrees per second */
	constexpr int32 RockShardParts = 3;
	constexpr float RockShardLengthShare = 2.6f;
	constexpr float RockShardThicknessShare = 0.85f;
	constexpr float RockShardSpin = 540.0f;

	/** Largest side of the rock mesh in cm */
	constexpr float RockShardMeshSize = 230.0f;

	/** Sand and pebbles the stone can have in the air at once, seconds between two, seconds each one lasts, how they fall in cm/s² */
	constexpr int32 RockShardLooseCount = 24;
	constexpr float RockShardShedInterval = 0.035f;
	constexpr float RockShardLooseLifetime = 0.6f;
	constexpr float RockShardGravity = 980.0f;

	/** How fast the pieces fly off when the stone shatters, in cm/s, and the shake of the camera */
	constexpr float RockShardShatterSpeed = 620.0f;
	constexpr float RockShardShake = 0.1f;

	/** Material parameters of the glow materials */
	const FName LookColorParameter(TEXT("Color"));
	const FName LookGlowParameter(TEXT("Glow"));
	const FName LookRimParameter(TEXT("Rim"));
}

AVaelSpellProjectile::AVaelSpellProjectile()
{
	// Walls stop the projectile, characters are passed through and checked in OnOverlap
	Collision = CreateDefaultSubobject<USphereComponent>(TEXT("Collision"));
	Collision->InitSphereRadius(30.0f);
	Collision->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Collision->SetCollisionObjectType(ECC_WorldDynamic);
	Collision->SetCollisionResponseToAllChannels(ECR_Ignore);
	Collision->SetCollisionResponseToChannel(ECC_WorldStatic, ECR_Block);
	Collision->SetCollisionResponseToChannel(ECC_PhysicsBody, ECR_Block);
	Collision->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	Collision->SetGenerateOverlapEvents(true);
	Collision->CanCharacterStepUpOn = ECB_No;
	RootComponent = Collision;

	Collision->OnComponentBeginOverlap.AddDynamic(this, &AVaelSpellProjectile::OnOverlap);

	// Placeholder look from engine assets
	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	Mesh->SetupAttachment(RootComponent);
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Mesh->SetCastShadow(false);
	Mesh->bReceivesDecals = false;

	static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereMesh(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	if (SphereMesh.Succeeded())
	{
		Mesh->SetStaticMesh(SphereMesh.Object);
	}

	static ConstructorHelpers::FObjectFinder<UMaterialInterface> SphereMaterial(TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
	if (SphereMaterial.Succeeded())
	{
		Mesh->SetMaterial(0, SphereMaterial.Object);
	}

	// Flies straight, no gravity
	Movement = CreateDefaultSubobject<UProjectileMovementComponent>(TEXT("Movement"));
	Movement->ProjectileGravityScale = 0.0f;
	Movement->bRotationFollowsVelocity = true;
	Movement->InitialSpeed = 1500.0f;
	Movement->MaxSpeed = 1500.0f;

	Movement->OnProjectileStop.AddDynamic(this, &AVaelSpellProjectile::OnStopped);

	InitialLifeSpan = 1.1f;

	// Only water projectiles tick, see InitSpell
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;
}

void AVaelSpellProjectile::InitSpell(const FVaelSpellHit& InHit, float Speed, float Radius, float Lifetime, int32 Pierce, const FLinearColor& Color)
{
	Hit = InHit;
	RemainingPierce = Pierce;
	SpellColor = Color;

	// Water puts out the fires it flies over, which has to be checked every frame
	PrimaryActorTick.bStartWithTickEnabled = Hit.Element == EVaelElement::Water;

	Collision->SetSphereRadius(Radius);
	Mesh->SetRelativeScale3D(FVector(Radius / PlaceholderSphereRadius));

	if (UMaterialInstanceDynamic* Material = Mesh->CreateAndSetMaterialInstanceDynamic(0))
	{
		Material->SetVectorParameterValue(TEXT("Color"), Color);
	}

	Movement->InitialSpeed = Speed;
	Movement->MaxSpeed = Speed;

	InitialLifeSpan = Lifetime;

	// The caster never blocks or triggers their own spell
	if (AActor* Caster = GetInstigator())
	{
		Collision->IgnoreActorWhenMoving(Caster, true);
	}
}

void AVaelSpellProjectile::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (Hit.Element == EVaelElement::Water && !bFlightEnded)
	{
		AVaelGroundArea::ExtinguishFires(GetWorld(), GetActorLocation(), Collision->GetScaledSphereRadius());
	}

	if (!LookShapes.IsEmpty())
	{
		AnimateLook(DeltaSeconds);
	}
}

UStaticMeshComponent* AVaelSpellProjectile::AddLookShape(UMaterialInterface* Material, const FLinearColor& ShapeColor, float Glow, float Rim, bool bLoose, UStaticMesh* OwnMesh)
{
	UStaticMeshComponent* Shape = NewObject<UStaticMeshComponent>(this);
	Shape->SetupAttachment(RootComponent);
	Shape->SetStaticMesh(OwnMesh != nullptr ? OwnMesh : Mesh->GetStaticMesh().Get());
	Shape->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Shape->SetCastShadow(false);
	Shape->bReceivesDecals = false;

	if (bLoose)
	{
		// Loose pieces keep their place in the world instead of flying along, and wait hidden until they are shed
		Shape->SetUsingAbsoluteLocation(true);
		Shape->SetUsingAbsoluteRotation(true);
		Shape->SetVisibility(false);
	}

	Shape->RegisterComponent();

	// A mesh of its own, like the rock, keeps its own material
	if (OwnMesh != nullptr)
	{
		return Shape;
	}

	Shape->SetMaterial(0, Material);

	if (UMaterialInstanceDynamic* Dynamic = Shape->CreateAndSetMaterialInstanceDynamic(0))
	{
		Dynamic->SetVectorParameterValue(LookColorParameter, ShapeColor);
		Dynamic->SetScalarParameterValue(LookGlowParameter, Glow);
		Dynamic->SetScalarParameterValue(LookRimParameter, Rim);
	}

	return Shape;
}

void AVaelSpellProjectile::AddLookLight(const FLinearColor& LightColor, float Intensity)
{
	LookLight = NewObject<UPointLightComponent>(this);
	LookLight->SetupAttachment(RootComponent);
	LookLight->SetMobility(EComponentMobility::Movable);
	LookLight->SetIntensityUnits(ELightUnits::Candelas);
	LookLight->SetIntensity(Intensity);
	LookLight->SetLightColor(LightColor);
	LookLight->SetAttenuationRadius(320.0f);
	LookLight->SetSourceRadius(4.0f);
	LookLight->SetCastShadows(false);
	LookLight->RegisterComponent();

	LookLightIntensity = Intensity;
}

void AVaelSpellProjectile::BuildLook()
{
	// The glowing materials of the element orbs; the plain engine material while they don't exist
	const UVaelMagicSettings* MagicSettings = UVaelMagicSettings::Get();
	UMaterialInterface* Plain = Mesh->GetMaterial(0);
	UMaterialInterface* CoreMaterial = VaelAssets::LoadOptional(MagicSettings->ElementOrbCoreMaterial);
	UMaterialInterface* GlowMaterial = VaelAssets::LoadOptional(MagicSettings->ElementOrbGlowMaterial);
	CoreMaterial = CoreMaterial != nullptr ? CoreMaterial : Plain;
	GlowMaterial = GlowMaterial != nullptr ? GlowMaterial : Plain;

	const float Radius = Collision->GetScaledSphereRadius();

	if (Look == EVaelProjectileLook::Spark)
	{
		const FLinearColor Hot = FMath::Lerp(SpellColor, FLinearColor::White, 0.45f);
		const FLinearColor Dying = SpellColor * FLinearColor(1.0f, 0.45f, 0.3f);

		// A white-hot streak in a halo of its color, with a tail that thins out behind it
		LookShapes.Add(AddLookShape(CoreMaterial, Hot, 18.0f));
		LookShapes.Add(AddLookShape(GlowMaterial, SpellColor, 3.0f));

		for (int32 TailIndex = 0; TailIndex < SparkTailCount; ++TailIndex)
		{
			LookShapes.Add(AddLookShape(GlowMaterial, FMath::Lerp(SpellColor, Dying, (TailIndex + 1.0f) / SparkTailCount), 2.6f - TailIndex * 0.5f));
		}

		// Embers the spark sheds in flight
		for (int32 EmberIndex = 0; EmberIndex < SparkEmberCount; ++EmberIndex)
		{
			LoosePieces.Add(AddLookShape(GlowMaterial, FMath::Lerp(SpellColor, Dying, 0.5f), 5.0f, 0.0f, true));
		}

		LooseSize = Radius * SparkThicknessShare * 0.9f / (PlaceholderSphereRadius * 2.0f);
		LooseLifetime = SparkEmberLifetime;
		LooseGravity = SparkEmberGravity;
		LooseDrag = 2.2f;
		bLooseFlicker = true;

		AddLookLight(SpellColor, 9.0f);
	}
	else if (Look == EVaelProjectileLook::WaterOrb)
	{
		const FLinearColor Deep = SpellColor * FLinearColor(0.25f, 0.45f, 0.8f);
		const FLinearColor Bright = FMath::Lerp(SpellColor, FLinearColor::White, 0.35f);

		// A round ball of water with a pale sheen, two drops trailing behind, and the ring of its burst waiting inside
		LookShapes.Add(AddLookShape(CoreMaterial, Deep, 0.7f));
		LookShapes.Add(AddLookShape(GlowMaterial, Bright, 1.8f, 1.0f));
		LookShapes.Add(AddLookShape(CoreMaterial, Bright, 0.8f));
		LookShapes.Add(AddLookShape(CoreMaterial, Bright, 0.8f));

		UStaticMeshComponent* Ring = AddLookShape(GlowMaterial, Bright, WaterBurstGlow, 1.0f);
		Ring->SetVisibility(false);
		BurstMaterial = Cast<UMaterialInstanceDynamic>(Ring->GetMaterial(0));
		LookShapes.Add(Ring);

		// Drops the ball bursts into
		for (int32 DropIndex = 0; DropIndex < WaterBurstDropCount; ++DropIndex)
		{
			LoosePieces.Add(AddLookShape(CoreMaterial, Bright, 0.9f, 0.0f, true));
		}

		LooseSize = Radius * 0.3f / (PlaceholderSphereRadius * 2.0f);
		LooseLifetime = WaterBurstDropLifetime;
		LooseGravity = WaterBurstGravity;
		LooseDrag = 2.6f;
		bLooseFlicker = false;

		AddLookLight(Bright, 3.0f);
	}
	else if (Look == EVaelProjectileLook::RockShard)
	{
		// The rock of the earth orb; brown spheres stand in for it while the pack isn't installed
		UStaticMesh* RockMesh = VaelAssets::LoadOptional(MagicSettings->ElementOrbRockMesh);
		bRealRock = RockMesh != nullptr;

		const FLinearColor Stone = SpellColor * 0.55f;
		const FLinearColor Sand = FMath::Lerp(SpellColor, FLinearColor(0.75f, 0.65f, 0.5f), 0.6f);

		// Three rocks in a row, each smaller than the one behind it, taper into a point
		for (int32 PartIndex = 0; PartIndex < RockShardParts; ++PartIndex)
		{
			LookShapes.Add(AddLookShape(CoreMaterial, Stone, 0.0f, 0.0f, false, RockMesh));
		}

		// What the stone loses in flight: mostly grains of sand, every third piece a pebble
		const float PieceScale = Radius / (bRealRock ? RockShardMeshSize : PlaceholderSphereRadius * 2.0f);
		for (int32 PieceIndex = 0; PieceIndex < RockShardLooseCount; ++PieceIndex)
		{
			const bool bPebble = PieceIndex % 3 == 0;
			LoosePieces.Add(bPebble ? AddLookShape(CoreMaterial, Stone, 0.0f, 0.0f, true, RockMesh) : AddLookShape(CoreMaterial, Sand, 0.0f, 0.0f, true));
			LoosePieceSizes.Add(bPebble ? PieceScale * FMath::FRandRange(0.16f, 0.3f) : Radius * FMath::FRandRange(0.05f, 0.1f) / (PlaceholderSphereRadius * 2.0f));
		}

		LooseLifetime = RockShardLooseLifetime;
		LooseGravity = RockShardGravity;
		LooseDrag = 1.5f;
		bLooseTumble = true;
	}
	else
	{
		return;
	}

	LooseVelocities.Init(FVector::ZeroVector, LoosePieces.Num());
	LooseAges.Init(-1.0f, LoosePieces.Num());

	Mesh->SetVisibility(false);
	SetActorTickEnabled(true);
	AnimateLook(0.0f);
}

void AVaelSpellProjectile::ShedPiece(const FVector& Location, const FVector& Velocity)
{
	if (LoosePieces.IsEmpty())
	{
		return;
	}

	LooseAges[NextLoosePiece] = 0.0f;
	LooseVelocities[NextLoosePiece] = Velocity;
	LoosePieces[NextLoosePiece]->SetWorldLocation(Location);
	LoosePieces[NextLoosePiece]->SetRelativeScale3D(FVector(LoosePieceSizes.IsValidIndex(NextLoosePiece) ? LoosePieceSizes[NextLoosePiece] : LooseSize));
	LoosePieces[NextLoosePiece]->SetVisibility(true);

	NextLoosePiece = (NextLoosePiece + 1) % LoosePieces.Num();
}

void AVaelSpellProjectile::AnimateLook(float DeltaSeconds)
{
	const float Time = GetWorld()->GetTimeSeconds();

	if (bFlightEnded)
	{
		// What is left of the spell plays out, then the projectile goes
		AfterglowTime += DeltaSeconds;
		if (AfterglowTime >= LooseLifetime)
		{
			Destroy();
			return;
		}

		if (Look == EVaelProjectileLook::WaterOrb)
		{
			AnimateWaterBurst();
		}
	}
	else if (Look == EVaelProjectileLook::Spark)
	{
		AnimateSpark(Time, DeltaSeconds);
	}
	else if (Look == EVaelProjectileLook::WaterOrb)
	{
		AnimateWaterOrb(Time);
	}
	else
	{
		AnimateRock(Time, DeltaSeconds);
	}

	// Loose pieces fall, slow down in the air and shrink as they die
	for (int32 PieceIndex = 0; PieceIndex < LoosePieces.Num(); ++PieceIndex)
	{
		if (LooseAges[PieceIndex] < 0.0f)
		{
			continue;
		}

		LooseAges[PieceIndex] += DeltaSeconds;
		if (LooseAges[PieceIndex] >= LooseLifetime)
		{
			LooseAges[PieceIndex] = -1.0f;
			LoosePieces[PieceIndex]->SetVisibility(false);
			continue;
		}

		LooseVelocities[PieceIndex].Z -= LooseGravity * DeltaSeconds;
		LooseVelocities[PieceIndex] *= FMath::Max(0.0f, 1.0f - LooseDrag * DeltaSeconds);

		const float Dying = 1.0f - LooseAges[PieceIndex] / LooseLifetime;
		const float Flicker = bLooseFlicker ? 0.8f + 0.4f * FMath::Sin(Time * 40.0f + PieceIndex * 2.7f) : 1.0f;

		// Sand and pebbles keep their size until they are nearly gone, embers and drops shrink all the way
		const float Size = LoosePieceSizes.IsValidIndex(PieceIndex) ? LoosePieceSizes[PieceIndex] : LooseSize;
		const float Shrink = bLooseTumble ? FMath::Min(1.0f, Dying * 4.0f) : Dying;

		LoosePieces[PieceIndex]->SetWorldLocation(LoosePieces[PieceIndex]->GetComponentLocation() + LooseVelocities[PieceIndex] * DeltaSeconds);
		LoosePieces[PieceIndex]->SetRelativeScale3D(FVector(Size * Shrink * Flicker));

		if (bLooseTumble)
		{
			LoosePieces[PieceIndex]->SetWorldRotation(FRotator(LooseAges[PieceIndex] * 620.0f + PieceIndex * 50.0f, LooseAges[PieceIndex] * 440.0f, 0.0f));
		}
	}
}

void AVaelSpellProjectile::AnimateRock(float Time, float DeltaSeconds)
{
	const float Radius = Collision->GetScaledSphereRadius();
	const float MeshSize = bRealRock ? RockShardMeshSize : PlaceholderSphereRadius * 2.0f;
	const float Length = Radius * RockShardLengthShare;
	const float Thickness = Radius * RockShardThicknessShare;

	// The stone spins around its flight like a thrown spearhead; the rocks sit one before the other and get smaller towards the tip
	for (int32 PartIndex = 0; PartIndex < LookShapes.Num(); ++PartIndex)
	{
		const float Along = PartIndex / FMath::Max(LookShapes.Num() - 1.0f, 1.0f);
		const float Taper = FMath::Lerp(1.0f, 0.38f, Along);

		UStaticMeshComponent* Part = LookShapes[PartIndex];
		Part->SetRelativeLocation(FVector(Length * FMath::Lerp(-0.2f, 0.42f, Along), 0.0f, 0.0f));
		Part->SetRelativeRotation(FRotator(0.0f, 0.0f, Time * RockShardSpin + PartIndex * 47.0f));
		Part->SetRelativeScale3D(FVector(Length * FMath::Lerp(0.6f, 0.34f, Along), Thickness * Taper, Thickness * Taper) / MeshSize);
	}

	// Sand and pebbles come off the back and are left behind
	NextShedCountdown -= DeltaSeconds;
	if (NextShedCountdown <= 0.0f)
	{
		NextShedCountdown = RockShardShedInterval * FMath::FRandRange(0.6f, 1.5f);

		const FVector Forward = GetActorForwardVector();
		ShedPiece(GetActorLocation() - Forward * Radius * FMath::FRandRange(0.3f, 1.0f) + FMath::VRand() * Radius * 0.25f,
			Forward * Movement->Velocity.Size() * 0.06f + FMath::VRand() * FMath::FRandRange(40.0f, 130.0f));
	}
}

void AVaelSpellProjectile::AnimateSpark(float Time, float DeltaSeconds)
{
	const float Radius = Collision->GetScaledSphereRadius();
	const float Length = Radius * SparkLengthShare / (PlaceholderSphereRadius * 2.0f);
	const float Thickness = Radius * SparkThicknessShare / (PlaceholderSphereRadius * 2.0f);
	const float Flicker = FMath::PerlinNoise1D(Time * 14.0f);

	// The streak lies along the flight; core and halo flicker against each other
	LookShapes[0]->SetRelativeScale3D(FVector(Length, Thickness, Thickness) * (1.0f + 0.1f * Flicker));
	LookShapes[1]->SetRelativeScale3D(FVector(Length * 1.5f, Thickness * 2.6f, Thickness * 2.6f) * (1.0f - 0.15f * Flicker));

	for (int32 TailIndex = 0; TailIndex < SparkTailCount; ++TailIndex)
	{
		const float Thin = 1.0f - (TailIndex + 1.0f) / (SparkTailCount + 1.5f);
		const float Waver = FMath::Sin(Time * 26.0f + TailIndex * 1.9f);

		UStaticMeshComponent* Piece = LookShapes[2 + TailIndex];
		Piece->SetRelativeLocation(FVector(-Radius * SparkLengthShare * 0.55f * (TailIndex + 1.0f), Waver * Radius * 0.07f, 0.0f));
		Piece->SetRelativeScale3D(FVector(Length * 0.9f, Thickness * 2.0f * Thin, Thickness * 2.0f * Thin) * (1.0f + 0.12f * Waver));
	}

	LookLight->SetIntensity(LookLightIntensity * (1.0f + 0.3f * Flicker));

	// Shed the next ember: it leaves the spark sideways, keeps a little of the flight speed and falls
	NextShedCountdown -= DeltaSeconds;
	if (NextShedCountdown <= 0.0f)
	{
		NextShedCountdown = SparkEmberInterval * FMath::FRandRange(0.6f, 1.5f);

		const FVector Forward = GetActorForwardVector();
		ShedPiece(GetActorLocation() - Forward * Radius * FMath::FRandRange(0.2f, 1.2f),
			Forward * Movement->Velocity.Size() * SparkEmberForwardShare + FMath::VRand() * SparkEmberSpread * FMath::FRandRange(0.4f, 1.0f) + FVector(0.0f, 0.0f, 60.0f));
	}
}

void AVaelSpellProjectile::AnimateWaterOrb(float Time)
{
	const float Radius = Collision->GetScaledSphereRadius();
	const float Size = Radius * WaterOrbSizeShare / PlaceholderSphereRadius;

	// Round, with a slow wobble: the ball swells a little along one axis while it shrinks along the others
	const float Wave = Time * 7.0f;
	const FVector Wobble(1.0f + WaterOrbWobble * FMath::Sin(Wave), 1.0f + WaterOrbWobble * FMath::Sin(Wave + 2.1f), 1.0f + WaterOrbWobble * FMath::Sin(Wave + 4.2f));

	LookShapes[0]->SetRelativeScale3D(Wobble * Size);
	LookShapes[1]->SetRelativeScale3D(Wobble * Size * 1.12f);

	// Two drops are dragged along behind and swing below the path of the ball
	for (int32 DropIndex = 0; DropIndex < 2; ++DropIndex)
	{
		const float Swing = FMath::Sin(Time * 9.0f + DropIndex * 2.4f);

		UStaticMeshComponent* Drop = LookShapes[2 + DropIndex];
		Drop->SetRelativeLocation(FVector(-Radius * (1.05f + 0.55f * DropIndex), Swing * Radius * 0.12f, -Radius * (0.15f + 0.2f * DropIndex)));
		Drop->SetRelativeScale3D(FVector(1.25f, 0.85f, 0.85f) * Size * (0.26f - 0.08f * DropIndex) * (1.0f + 0.15f * Swing));
	}

	LookLight->SetIntensity(LookLightIntensity * (1.0f + 0.1f * FMath::Sin(Wave)));
}

void AVaelSpellProjectile::AnimateWaterBurst()
{
	if (!bBurst)
	{
		return;
	}

	// The ring of spray shoots outwards and fades, the flash of light dies with it
	const float Progress = FMath::Clamp(AfterglowTime / WaterBurstRingTime, 0.0f, 1.0f);
	const float Reach = 1.0f - FMath::Square(1.0f - Progress);
	const float Size = Collision->GetScaledSphereRadius() * WaterOrbSizeShare / PlaceholderSphereRadius;

	UStaticMeshComponent* Ring = LookShapes.Last();
	Ring->SetRelativeScale3D(FVector(Size * (1.0f + WaterBurstRingGrowth * Reach)));
	Ring->SetVisibility(Progress < 1.0f);

	if (BurstMaterial != nullptr)
	{
		BurstMaterial->SetScalarParameterValue(LookGlowParameter, WaterBurstGlow * (1.0f - Progress));
	}

	LookLight->SetIntensity(LookLightIntensity * WaterBurstFlash * (1.0f - Progress));
}

void AVaelSpellProjectile::BeginAfterglow(bool bHitSomething)
{
	bFlightEnded = true;
	AfterglowTime = 0.0f;

	// The projectile stays where it ended and can't hit anything any more
	Collision->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Movement->StopMovementImmediately();
	Movement->Deactivate();
	SetLifeSpan(0.0f);

	const FVector Center = GetActorLocation();
	const float Radius = Collision->GetScaledSphereRadius();

	for (UStaticMeshComponent* Shape : LookShapes)
	{
		Shape->SetVisibility(false);
	}

	if (Look == EVaelProjectileLook::Spark)
	{
		// The embers already in the air glow out; a hit throws a last handful in all directions
		LookLight->SetVisibility(false);

		for (int32 EmberIndex = 0; bHitSomething && EmberIndex < SparkImpactEmbers; ++EmberIndex)
		{
			ShedPiece(Center, FMath::VRand() * SparkEmberSpread * FMath::FRandRange(1.2f, 2.6f) + FVector(0.0f, 0.0f, 140.0f));
		}
	}
	else if (Look == EVaelProjectileLook::WaterOrb)
	{
		// The ball bursts under pressure: drops shoot out all around and upwards, a ring of spray races after them.
		// A ball that just runs out in the air only falls apart.
		const int32 NumDrops = bHitSomething ? LoosePieces.Num() : LoosePieces.Num() / 4;
		const float Pressure = bHitSomething ? 1.0f : 0.2f;

		for (int32 DropIndex = 0; DropIndex < NumDrops; ++DropIndex)
		{
			FVector Direction = FMath::VRand();
			Direction.Z = FMath::Abs(Direction.Z) * 0.7f;
			Direction.Normalize();

			ShedPiece(Center + Direction * Radius * 0.4f, Direction * FMath::FRandRange(WaterBurstSpeed * 0.55f, WaterBurstSpeed) * Pressure + FVector(0.0f, 0.0f, 160.0f * Pressure));
		}

		bBurst = bHitSomething;

		if (bHitSomething)
		{
			LookShapes.Last()->SetVisibility(true);
			UVaelHitFeedbackSubsystem::Shake(this, WaterBurstShake);
		}
		else
		{
			LookLight->SetVisibility(false);
		}
	}
	else if (Look == EVaelProjectileLook::RockShard)
	{
		// The stone shatters on what it hits: splinters and sand fly off all around and drop heavily
		const int32 NumPieces = bHitSomething ? LoosePieces.Num() : LoosePieces.Num() / 4;

		for (int32 PieceIndex = 0; PieceIndex < NumPieces; ++PieceIndex)
		{
			FVector Direction = FMath::VRand();
			Direction.Z = FMath::Abs(Direction.Z) * 0.6f;
			Direction.Normalize();

			ShedPiece(Center + Direction * Radius * 0.3f, Direction * FMath::FRandRange(RockShardShatterSpeed * 0.4f, RockShardShatterSpeed) * (bHitSomething ? 1.0f : 0.3f) + FVector(0.0f, 0.0f, 120.0f));
		}

		if (bHitSomething)
		{
			UVaelHitFeedbackSubsystem::Shake(this, RockShardShake);
		}
	}
}

void AVaelSpellProjectile::OnOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if (OtherActor == nullptr || OtherActor == GetInstigator() || HitActors.Contains(OtherActor))
	{
		return;
	}

	// Allies are passed through
	if (UVaelCombatStatics::ApplySpellHit(GetInstigator(), OtherActor, Hit, GetVelocity()))
	{
		HitActors.Add(OtherActor);

		if (RemainingPierce > 0)
		{
			// Flying on: each pierced target gets its own impact, the last one comes with the end of the flight
			VaelEffects::PlayImpact(this, Effects, OtherActor->GetActorLocation(), Collision->GetScaledSphereRadius());
		}

		if (RemainingPierce-- <= 0)
		{
			// Later overlaps of the same move must not hit anything else
			Collision->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			EndFlight(true);
		}
	}
}

void AVaelSpellProjectile::OnStopped(const FHitResult& ImpactResult)
{
	EndFlight(true);
}

void AVaelSpellProjectile::LifeSpanExpired()
{
	EndFlight(false);
}

void AVaelSpellProjectile::SetEffects(const FVaelLoadedEffects& InEffects, UNiagaraSystem* InImpactAreaVisual)
{
	Effects = InEffects;
	ImpactAreaVisual = InImpactAreaVisual;
}

void AVaelSpellProjectile::BeginPlay()
{
	Super::BeginPlay();

	// The trail replaces the plain placeholder sphere, but plays together with a look of its own
	if (VaelEffects::Attach(Effects.Trail, Collision, NAME_None, Effects.Color, Collision->GetScaledSphereRadius()) != nullptr)
	{
		Mesh->SetVisibility(false);
	}

	BuildLook();
}

void AVaelSpellProjectile::SetImpactArea(EVaelElement Element, float Radius, float Lifetime, float DamagePerSecond, EVaelGroundEffect Effect)
{
	ImpactEffect = Effect;
	ImpactElement = Element;
	ImpactRadius = Radius;
	ImpactLifetime = Lifetime;
	ImpactDamagePerSecond = DamagePerSecond;
}

void AVaelSpellProjectile::SetPassesWalls()
{
	Collision->SetCollisionResponseToChannel(ECC_WorldStatic, ECR_Ignore);
	Collision->SetCollisionResponseToChannel(ECC_PhysicsBody, ECR_Ignore);
}

void AVaelSpellProjectile::SetExplosion(const FVaelSpellHit& InExplosionHit, float Radius)
{
	ExplosionHit = InExplosionHit;
	ExplosionRadius = Radius;
}

AVaelSpellProjectile* AVaelSpellProjectile::Launch(APawn* Attacker, const FVector& Location, const FVector& Direction, const FVaelSpellHit& InHit, float Speed, float Radius, float Lifetime, const FLinearColor& Color, float FireRadius, float FireLifetime, float FireDamagePerSecond)
{
	UWorld* World = Attacker != nullptr ? Attacker->GetWorld() : nullptr;
	const FVector FlightDirection = Direction.GetSafeNormal2D();
	if (World == nullptr || FlightDirection.IsNearlyZero())
	{
		return nullptr;
	}

	const FTransform SpawnTransform(FlightDirection.Rotation(), Location);

	AVaelSpellProjectile* Projectile = World->SpawnActorDeferred<AVaelSpellProjectile>(AVaelSpellProjectile::StaticClass(), SpawnTransform, Attacker, Attacker, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (Projectile != nullptr)
	{
		Projectile->InitSpell(InHit, Speed, Radius, Lifetime, 0, Color);
		Projectile->SetImpactArea(EVaelElement::Fire, FireRadius, FireLifetime, FireDamagePerSecond);
		Projectile->SetEffects(VaelEffects::Load(InHit.Element));
		Projectile->FinishSpawning(SpawnTransform);
	}

	return Projectile;
}

void AVaelSpellProjectile::EndFlight(bool bHitSomething)
{
	if (IsActorBeingDestroyed() || bFlightEnded)
	{
		return;
	}

	if (ExplosionRadius > 0.0f)
	{
		Explode();
	}

	// A projectile that just runs out in the air fizzles without an impact
	if (bHitSomething || ExplosionRadius > 0.0f)
	{
		VaelEffects::PlayImpact(this, Effects, GetActorLocation(), ExplosionRadius > 0.0f ? ExplosionRadius : Collision->GetScaledSphereRadius());
	}

	if (ImpactRadius > 0.0f)
	{
		FVector Location = GetActorLocation();

		// The patch lies on the ground below the projectile
		FHitResult GroundHit;
		if (VaelGround::TraceGround(GetWorld(), Location, Location - FVector(0.0f, 0.0f, 500.0f), GroundHit, this))
		{
			AVaelGroundArea::SpawnArea(GetWorld(), GroundHit.Location + FVector(0.0f, 0.0f, 2.0f), ImpactElement, ImpactRadius, ImpactLifetime, ImpactDamagePerSecond, GetInstigator(), true, ImpactEffect, ImpactAreaVisual);
		}
	}

	// A projectile with a look of its own stays for a moment, so its embers can glow out or its water can burst
	if (!LookShapes.IsEmpty())
	{
		BeginAfterglow(bHitSomething);
		return;
	}

	Destroy();
}

void AVaelSpellProjectile::Explode()
{
	UWorld* World = GetWorld();
	const FVector Center = GetActorLocation();

	UVaelCombatStatics::ApplySpellHitInRadius(GetInstigator(), Center, ExplosionRadius, ExplosionHit);

	UVaelHitFeedbackSubsystem::Shake(this, ExplosionShake);

#if ENABLE_DRAW_DEBUG
	// Placeholder look while no impact effect exists
	if (Effects.Impact == nullptr)
	{
		DrawDebugSphere(World, Center, ExplosionRadius, 24, UVaelMagicSettings::Get()->GetElementColor(ExplosionHit.Element).ToFColor(true), false, 0.3f);
	}
#endif
}
