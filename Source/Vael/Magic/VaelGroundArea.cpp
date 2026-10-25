// Copyright Epic Games, Inc. All Rights Reserved.

#include "Magic/VaelGroundArea.h"
#include "UObject/ConstructorHelpers.h"
#include "Combat/VaelCharacterBase.h"
#include "Combat/VaelCombatStatics.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Pawn.h"
#include "Magic/VaelMagicSettings.h"
#include "Magic/VaelSpellEffects.h"
#include "NiagaraComponent.h"
#include "NiagaraSystem.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Vael.h"
#include "VaelAssets.h"
#include "World/VaelRegion.h"
#include "World/VaelWorldSettings.h"
#include "World/VaelFireSpot.h"
#include "World/VaelFireComponent.h"
#include "World/VaelGround.h"
#include "ProceduralMeshComponent.h"

namespace
{
	/** Radius of the engine cylinder mesh used as placeholder */
	constexpr float PlaceholderCylinderRadius = 50.0f;

	/** A ground effect of fixed size is repeated at most this often over one patch */
	constexpr int32 MaxRepeatedVisuals = 20;

	/** Seconds between two damage steps of an area */
	constexpr float DamageStepInterval = 0.5f;

	/** Seconds a condition of the area stays on those who leave it */
	constexpr float AreaStatusLinger = 1.5f;

	/** Corruption black water adds to its region per second */
	constexpr float BlackwaterCorruptionPerSecond = 0.3f;
}

AVaelGroundArea::AVaelGroundArea()
{
	// Placeholder look from engine assets
	Disc = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Disc"));
	Disc->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Disc->SetCastShadow(false);
	Disc->bReceivesDecals = false;
	RootComponent = Disc;

	static ConstructorHelpers::FObjectFinder<UStaticMesh> DiscMesh(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	if (DiscMesh.Succeeded())
	{
		Disc->SetStaticMesh(DiscMesh.Object);
	}

	static ConstructorHelpers::FObjectFinder<UMaterialInterface> DiscMaterial(TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
	if (DiscMaterial.Succeeded())
	{
		Disc->SetMaterial(0, DiscMaterial.Object);
	}

	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = true;
}

void AVaelGroundArea::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);

	RefreshLook();
}

void AVaelGroundArea::BeginPlay()
{
	Super::BeginPlay();

	RefreshLook();

	if (Lifetime > 0.0f)
	{
		// Fires lit in the rain die sooner, in a drought they burn longer
		const AVaelRegion* Region = Element == EVaelElement::Fire ? AVaelRegion::GetRegionAt(GetWorld(), GetActorLocation()) : nullptr;
		if (Region != nullptr && Region->GetWeather() == EVaelWeather::Rain)
		{
			Lifetime *= UVaelWorldSettings::Get()->RainFireLifetimeShare;
		}
		else if (Region != nullptr && Region->GetWeather() == EVaelWeather::Drought)
		{
			Lifetime *= UVaelWorldSettings::Get()->DroughtFireLifetimeMultiplier;
		}

		SetLifeSpan(Lifetime);
	}

	// Only areas that hurt somebody or lay a condition on them need to tick
	SetActorTickEnabled((DamagePerSecond > 0.0f || GetInflictedStatus() != EVaelStatus::None) && GetInstigator() != nullptr);

	// The real look replaces the placeholder disc as soon as the effect exists; a mist veil turns, so it ticks too
	SpawnVisual();
	if (MistMesh != nullptr || !SteamBalls.IsEmpty())
	{
		SetActorTickEnabled(true);
	}
}

void AVaelGroundArea::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (MistMesh != nullptr || !SteamBalls.IsEmpty())
	{
		UpdateVeil(DeltaSeconds);
	}

	DamageStepTime += DeltaSeconds;
	if (DamageStepTime < DamageStepInterval)
	{
		return;
	}

	FVaelSpellHit Hit;
	Hit.Damage = DamagePerSecond * DamageStepTime;
	Hit.Element = Element;

	DamageStepTime = 0.0f;

	// Black water and fever mist lay their condition on everyone inside, renewed while they stay
	const EVaelStatus InflictedStatus = GetInflictedStatus();

	for (TActorIterator<AVaelCharacterBase> It(GetWorld()); It; ++It)
	{
		if (!IsInRange(It->GetActorLocation()))
		{
			continue;
		}

		if (DamagePerSecond > 0.0f)
		{
			UVaelCombatStatics::ApplySpellHit(GetInstigator(), *It, Hit, FVector::ZeroVector);
		}

		if (InflictedStatus != EVaelStatus::None && UVaelCombatStatics::CanDamage(GetInstigator(), *It))
		{
			UVaelCombatStatics::ApplyStatus(GetInstigator(), *It, InflictedStatus, AreaStatusLinger);
		}
	}

	// Black water is a little Mark source of its own, and it seeps into the region
	if (Effect == EVaelGroundEffect::Blackwater)
	{
		if (AVaelRegion* Region = AVaelRegion::GetRegionAt(GetWorld(), GetActorLocation()))
		{
			Region->AddCorruption(BlackwaterCorruptionPerSecond * DamageStepInterval);
		}
	}
}

EVaelStatus AVaelGroundArea::GetInflictedStatus() const
{
	switch (Effect)
	{
	case EVaelGroundEffect::Blackwater:	return EVaelStatus::Marked;
	case EVaelGroundEffect::Fever:		return EVaelStatus::Fevered;
	default:							return EVaelStatus::None;
	}
}

AVaelGroundArea* AVaelGroundArea::SpawnArea(UWorld* World, const FVector& Location, EVaelElement InElement, float InRadius, float InLifetime, float InDamagePerSecond, APawn* InInstigator, bool bInExtinguishable, EVaelGroundEffect InEffect, UNiagaraSystem* InVisual)
{
	if (World == nullptr)
	{
		return nullptr;
	}

	const FTransform SpawnTransform(Location);

	AVaelGroundArea* Area = World->SpawnActorDeferred<AVaelGroundArea>(AVaelGroundArea::StaticClass(), SpawnTransform, InInstigator, InInstigator, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (Area != nullptr)
	{
		Area->Element = InElement;
		Area->Radius = InRadius;
		Area->Lifetime = InLifetime;
		Area->DamagePerSecond = InDamagePerSecond;
		Area->bExtinguishable = bInExtinguishable;
		Area->Effect = InEffect;
		Area->VisualOverride = InVisual;

		Area->FinishSpawning(SpawnTransform);
	}

	return Area;
}

int32 AVaelGroundArea::ExtinguishFires(const UWorld* World, const FVector& Location, float InRadius)
{
	int32 NumExtinguished = 0;

	for (TActorIterator<AVaelGroundArea> It(World); It; ++It)
	{
		AVaelGroundArea* Area = *It;
		if (Area->Element == EVaelElement::Fire && Area->bExtinguishable && Area->IsInRange(Location, InRadius))
		{
			UE_LOG(LogVael, Verbose, TEXT("Fire '%s' is put out"), *GetNameSafe(Area));

			Area->Destroy();
			++NumExtinguished;
		}
	}

	return NumExtinguished;
}

int32 AVaelGroundArea::SpreadFires(APawn* Caster, const FVector& Origin, const FVector& Direction, float Range, float HalfAngleDegrees)
{
	UWorld* World = Caster != nullptr ? Caster->GetWorld() : nullptr;
	if (World == nullptr)
	{
		return 0;
	}

	const UVaelMagicSettings* MagicSettings = UVaelMagicSettings::Get();
	const float MinCosine = FMath::Cos(FMath::DegreesToRadians(HalfAngleDegrees));

	// New fires are added while iterating, so collect the existing ones first
	TArray<AVaelGroundArea*> Fires;
	for (TActorIterator<AVaelGroundArea> It(World); It; ++It)
	{
		if (It->Element == EVaelElement::Fire)
		{
			Fires.Add(*It);
		}
	}

	int32 NumSpread = 0;
	for (const AVaelGroundArea* Fire : Fires)
	{
		if (NumSpread >= MagicSettings->MaxFireSpreads)
		{
			break;
		}

		const FVector FireLocation = Fire->GetActorLocation();
		const FVector ToFire = (FireLocation - Origin).GetSafeNormal2D();

		if (!Fire->IsInRange(Origin, Range) || FVector::DotProduct(ToFire, Direction) < MinCosine)
		{
			continue;
		}

		// Walls stop the fire
		const FVector NewLocation = FireLocation + Direction * MagicSettings->FireSpreadDistance;
		const FVector TraceOffset(0.0f, 0.0f, 50.0f);
		if (World->LineTraceTestByObjectType(FireLocation + TraceOffset, NewLocation + TraceOffset, FCollisionObjectQueryParams(ECC_WorldStatic)))
		{
			continue;
		}

		const float NewDamagePerSecond = Fire->DamagePerSecond > 0.0f ? Fire->DamagePerSecond : MagicSettings->FireSpreadDamagePerSecond;
		SpawnArea(World, NewLocation, EVaelElement::Fire, Fire->Radius * 1.1f, MagicSettings->FireSpreadLifetime, NewDamagePerSecond, Caster);

		UE_LOG(LogVael, Verbose, TEXT("Wind carries fire '%s' on to %s"), *GetNameSafe(Fire), *NewLocation.ToCompactString());

		++NumSpread;
	}

	return NumSpread;
}

bool AVaelGroundArea::IsFireNear(const UWorld* World, const FVector& Location, float ExtraDistance)
{
	if (World == nullptr)
	{
		return false;
	}

	for (TActorIterator<AVaelGroundArea> It(World); It; ++It)
	{
		if (It->Element == EVaelElement::Fire && It->IsInRange(Location, ExtraDistance))
		{
			return true;
		}
	}

	return false;
}

void AVaelGroundArea::GetEffectsOn(const AActor* Victim, bool& bOutBlinded, float& OutSpeedMultiplier)
{
	bOutBlinded = false;
	OutSpeedMultiplier = 1.0f;

	const UWorld* World = Victim != nullptr ? Victim->GetWorld() : nullptr;
	if (World == nullptr)
	{
		return;
	}

	const FVector Location = Victim->GetActorLocation();

	for (TActorIterator<AVaelGroundArea> It(World); It; ++It)
	{
		const AVaelGroundArea* Area = *It;
		if (Area->Effect == EVaelGroundEffect::None || !Area->IsInRange(Location))
		{
			continue;
		}

		// Areas of a caster only affect the enemies of the caster
		if (Area->GetInstigator() != nullptr && !UVaelCombatStatics::CanDamage(Area->GetInstigator(), Victim))
		{
			continue;
		}

		if (Area->Effect == EVaelGroundEffect::Blind)
		{
			bOutBlinded = true;
		}
		else if (Area->Effect == EVaelGroundEffect::Slow || Area->Effect == EVaelGroundEffect::Blackwater)
		{
			OutSpeedMultiplier = FMath::Min(OutSpeedMultiplier, UVaelMagicSettings::Get()->MudSpeedMultiplier);
		}
	}
}

bool AVaelGroundArea::IsHiddenInMist(const AActor* Target, const AActor* Observer)
{
	const UWorld* World = Target != nullptr ? Target->GetWorld() : nullptr;
	if (World == nullptr || Observer == nullptr)
	{
		return false;
	}

	// Close by, the observer still makes them out through the mist
	const FVector TargetLocation = Target->GetActorLocation();
	const FVector ObserverLocation = Observer->GetActorLocation();
	if (FVector::DistSquared2D(TargetLocation, ObserverLocation) <= FMath::Square(UVaelMagicSettings::Get()->MistSenseRange))
	{
		return false;
	}

	for (TActorIterator<AVaelGroundArea> It(World); It; ++It)
	{
		const AVaelGroundArea* Area = *It;
		if (Area->Effect != EVaelGroundEffect::Veil || !Area->IsInRange(TargetLocation))
		{
			continue;
		}

		// Mist only hides the caster and their allies, and only from those outside it
		if (Area->GetInstigator() != nullptr && UVaelCombatStatics::CanDamage(Area->GetInstigator(), Target))
		{
			continue;
		}

		if (!Area->IsInRange(ObserverLocation))
		{
			return true;
		}
	}

	return false;
}

bool AVaelGroundArea::IsInRange(const FVector& Location, float ExtraDistance) const
{
	return FVector::DistSquared2D(Location, GetActorLocation()) <= FMath::Square(Radius + ExtraDistance);
}

void AVaelGroundArea::RefreshLook()
{
	Disc->SetWorldScale3D(FVector(Radius / PlaceholderCylinderRadius, Radius / PlaceholderCylinderRadius, 0.02f));

	UMaterialInstanceDynamic* Material = Cast<UMaterialInstanceDynamic>(Disc->GetMaterial(0));
	if (Material == nullptr)
	{
		Material = Disc->CreateAndSetMaterialInstanceDynamic(0);
	}

	if (Material != nullptr)
	{
		Material->SetVectorParameterValue(TEXT("Color"), GetLookColor());
	}
}

FLinearColor AVaelGroundArea::GetLookColor() const
{
	// Steam is pale, mist a cold grey, everything else shows the color of its element
	const UVaelMagicSettings* MagicSettings = UVaelMagicSettings::Get();
	if (Effect == EVaelGroundEffect::Veil)
	{
		return MagicSettings->MistColor;
	}

	// A hot spring is water with a glow of fire
	if (Effect == EVaelGroundEffect::HotSpring)
	{
		return FMath::Lerp(MagicSettings->GetElementColor(EVaelElement::Water), MagicSettings->GetElementColor(EVaelElement::Fire), 0.35f);
	}

	return Effect == EVaelGroundEffect::Blind ? MagicSettings->SteamColor : MagicSettings->GetElementColor(Element);
}

void AVaelGroundArea::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (FireLook.IsValid())
	{
		if (EndPlayReason == EEndPlayReason::Destroyed)
		{
			FireLook->ReleaseUser();
		}
		else
		{
			FireLook->PutOut(0.01f);
		}
	}
	Super::EndPlay(EndPlayReason);
}

bool AVaelGroundArea::BuildVeil()
{
	const UVaelMagicSettings* MagicSettings = UVaelMagicSettings::Get();
	const FVector Center = GetActorLocation();

	// Steam: a ball of the white substance over the area, two shells turning against each other
	if (Effect == EVaelGroundEffect::Blind)
	{
		UMaterialInterface* BallMaterial = VaelAssets::LoadOptional(MagicSettings->SteamBallMaterial);
		UStaticMesh* Sphere = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Sphere.Sphere"));
		if (BallMaterial == nullptr || Sphere == nullptr)
		{
			return false;
		}
		for (const float Share : { 1.0f, 0.82f })
		{
			UStaticMeshComponent* Ball = NewObject<UStaticMeshComponent>(this);
			Ball->SetStaticMesh(Sphere);
			Ball->SetMaterial(0, BallMaterial);
			Ball->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			Ball->SetCastShadow(false);
			Ball->SetupAttachment(RootComponent);
			Ball->SetUsingAbsoluteScale(true);
			Ball->RegisterComponent();
			// The engine sphere is 100 cm across; the ball sits on the ground, a little sunk into it
			Ball->SetWorldLocation(Center + FVector(0.0f, 0.0f, Radius * Share * MagicSettings->SteamBallLift));
			Ball->SetWorldScale3D(FVector(Radius * Share / 50.0f));
			SteamBalls.Add(Ball);
		}
		Disc->SetVisibility(false);
		return true;
	}

	// The mist veil: rings of points over the ground, each layer of the blanket and of the wisps above at its own height
	UMaterialInterface* MistMaterial = VaelAssets::LoadOptional(MagicSettings->MistMaterial);
	if (MistMaterial == nullptr)
	{
		return false;
	}
	MistMesh = NewObject<UProceduralMeshComponent>(this);
	MistMesh->SetupAttachment(RootComponent);
	MistMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	MistMesh->SetCastShadow(false);
	// Its own size and turn: the disc it hangs on is scaled to the radius and pressed flat
	MistMesh->SetUsingAbsoluteScale(true);
	MistMesh->RegisterComponent();
	MistMesh->SetUsingAbsoluteRotation(true);
	MistMesh->SetWorldScale3D(FVector::OneVector);
	MistMesh->SetWorldRotation(FRotator::ZeroRotator);

	const int32 NumRings = MagicSettings->MistRings;
	const int32 NumSegments = MagicSettings->MistSegments;
	const float Reach = Radius * MagicSettings->MistReachShare;

	// The ground under each point of the rings, found once
	TArray<float> GroundHeights;
	for (int32 Ring = 0; Ring <= NumRings; ++Ring)
	{
		for (int32 Segment = 0; Segment < NumSegments; ++Segment)
		{
			const float Angle = Segment * UE_TWO_PI / NumSegments;
			const FVector Place = Center + FVector(FMath::Cos(Angle), FMath::Sin(Angle), 0.0f) * Reach * Ring / NumRings;
			FHitResult GroundHit;
			const bool bFound = VaelGround::TraceGround(GetWorld(), Place + FVector(0.0f, 0.0f, 150.0f), Place - FVector(0.0f, 0.0f, 300.0f), GroundHit, GetInstigator());
			GroundHeights.Add(bFound ? GroundHit.Location.Z - Center.Z : 0.0f);
		}
	}

	TArray<FVector> Vertices;
	TArray<int32> Triangles;
	TArray<FVector> Normals;
	TArray<FVector2D> UVs;
	TArray<FProcMeshTangent> Tangents;
	for (int32 Layer = 0; Layer < MagicSettings->MistLayerHeights.Num(); ++Layer)
	{
		const float Lift = MagicSettings->MistLayerHeights[Layer];
		const bool bWisp = Lift > MagicSettings->MistBlanketHeight;
		const int32 First = Vertices.Num();
		for (int32 Ring = 0; Ring <= NumRings; ++Ring)
		{
			for (int32 Segment = 0; Segment < NumSegments; ++Segment)
			{
				const float Angle = Segment * UE_TWO_PI / NumSegments;
				const float Distance = Reach * Ring / NumRings;
				Vertices.Add(FVector(FMath::Cos(Angle) * Distance, FMath::Sin(Angle) * Distance, GroundHeights[Ring * NumSegments + Segment] + Lift));
				Normals.Add(FVector::UpVector);
				UVs.Add(FVector2D(FMath::Cos(Angle), FMath::Sin(Angle)) * Distance / 100.0f);
				MistDistances.Add(Distance);
				MistWisps.Add(bWisp);
				MistColors.Add(FLinearColor(0.0f, bWisp ? 1.0f : 0.0f, 0.0f, 1.0f));
			}
		}
		for (int32 Ring = 0; Ring < NumRings; ++Ring)
		{
			for (int32 Segment = 0; Segment < NumSegments; ++Segment)
			{
				const int32 A = First + Ring * NumSegments + Segment;
				const int32 B = First + Ring * NumSegments + (Segment + 1) % NumSegments;
				const int32 C = A + NumSegments;
				const int32 D = B + NumSegments;
				Triangles.Append({ A, C, B, B, C, D });
			}
		}
	}
	MistMesh->CreateMeshSection_LinearColor(0, Vertices, Triangles, Normals, UVs, MistColors, Tangents, false);
	MistVertices = MoveTemp(Vertices);
	MistMesh->SetMaterial(0, MistMaterial);
	Disc->SetVisibility(false);
	return true;
}

void AVaelGroundArea::UpdateVeil(float DeltaSeconds)
{
	const UVaelMagicSettings* MagicSettings = UVaelMagicSettings::Get();
	CloudAge += DeltaSeconds;

	// The ball of steam turns slowly, its two shells against each other
	for (int32 Index = 0; Index < SteamBalls.Num(); ++Index)
	{
		SteamBalls[Index]->AddLocalRotation(FRotator(0.0f, (Index == 0 ? 1.0f : -1.4f) * MagicSettings->VeilTurnSpeed * DeltaSeconds, 0.0f));
	}
	if (MistMesh == nullptr)
	{
		return;
	}

	// The front creeps out fast at first and slows; the blanket is densest near the middle and thins out at its edge,
	// and in the last moments the whole veil thins away. The wisps follow the blanket a little behind
	const float Creep = FMath::Clamp(CloudAge / MagicSettings->VeilPullSeconds, 0.0f, 1.0f);
	const float Reach = Radius * MagicSettings->MistReachShare;
	const float Front = Reach * (1.0f - FMath::Square(1.0f - Creep));
	const float Remaining = GetLifeSpan() > 0.0f ? GetLifeSpan() : 1.0f;
	const float Fade = FMath::Clamp(Remaining / MagicSettings->MistFadeSeconds, 0.0f, 1.0f);
	const float EdgeWidth = Reach * 0.3f;
	for (int32 Index = 0; Index < MistColors.Num(); ++Index)
	{
		const float Distance = MistDistances[Index];
		const float Behind = MistWisps[Index] ? Front - EdgeWidth * 0.5f : Front;
		const float Edge = FMath::Clamp((Behind - Distance) / EdgeWidth, 0.0f, 1.0f);
		const float Inner = FMath::Lerp(1.0f, 0.55f, Distance / Reach);
		MistColors[Index].R = Edge * Inner * Fade;
	}
	// (with the points again: without them the new colours are not taken over)
	MistMesh->UpdateMeshSection_LinearColor(0, MistVertices, TArray<FVector>(), TArray<FVector2D>(), MistColors, TArray<FProcMeshTangent>());
}

void AVaelGroundArea::SpawnVisual()
{
	UNiagaraSystem* Visual = VisualOverride;
	float Spacing = 0.0f;
	float Scale = 1.0f;

	// A fire is made of living flames of the fire kit: a bed of tongues with sparks, light and a scorch mark
	if (Visual == nullptr && Element == EVaelElement::Fire && Effect == EVaelGroundEffect::None)
	{
		FVaelFireShape Shape;
		Shape.Radius = Radius;
		Shape.Height = FMath::Clamp(Radius * 1.9f, 60.0f, 220.0f);
		Shape.bScorch = true;
		// Fires laid next to each other (a Feuerstrahl sweeping over the ground) share one burning patch instead of flickering on top of each other
		FireLook = AVaelFireSpot::SpawnOrJoin(GetWorld(), GetActorLocation(), Shape, 0.0f, true);
		if (FireLook.IsValid())
		{
			Disc->SetVisibility(false);
			return;
		}
	}

	if (Visual == nullptr)
	{
		const UVaelMagicSettings* MagicSettings = UVaelMagicSettings::Get();

		// Steam and mist share their own look, mud has one; or else the one of water and earth
		if (Effect == EVaelGroundEffect::Blind || Effect == EVaelGroundEffect::Veil)
		{
			Visual = VaelAssets::LoadOptional(MagicSettings->SteamEffect);
			if (BuildVeil())
			{
				return;
			}
		}
		else if (Effect == EVaelGroundEffect::Slow)
		{
			Visual = VaelAssets::LoadOptional(MagicSettings->MudEffect);
		}

		if (Visual == nullptr)
		{
			const EVaelElement LookElement = Effect == EVaelGroundEffect::Blind || Effect == EVaelGroundEffect::Veil ? EVaelElement::Water : Effect == EVaelGroundEffect::Slow ? EVaelElement::Earth : Element;
			const FVaelLoadedEffects Loaded = VaelEffects::Load(LookElement);
			Visual = Loaded.Ground;
			Spacing = Loaded.GroundSpacing;
			Scale = Loaded.GroundScale;
		}
	}

	UNiagaraComponent* Middle = VaelEffects::Attach(Visual, Disc, NAME_None, GetLookColor(), Radius);
	if (Middle == nullptr)
	{
		return;
	}

	Disc->SetVisibility(false);

	if (Spacing <= 0.0f)
	{
		return;
	}

	// An effect of fixed size is repeated all over the patch, so a big fire is many flames and not one small one
	Middle->SetWorldScale3D(FVector(Scale));

	const int32 NumMore = FMath::Clamp(FMath::RoundToInt(FMath::Square(Radius / Spacing) * 1.2f) - 1, 0, MaxRepeatedVisuals);
	for (int32 Index = 0; Index < NumMore; ++Index)
	{
		// Evenly over the disc, in a spiral from the middle outwards, each one nudged a little
		const float Share = FMath::Sqrt((Index + 1.0f) / NumMore);
		const float Angle = (Index + 1) * 2.39996f;
		const FVector Offset = FVector(FMath::Cos(Angle), FMath::Sin(Angle), 0.0f) * Share * FMath::Max(Radius - Spacing * 0.4f, 0.0f) + FVector(FMath::FRandRange(-0.15f, 0.15f), FMath::FRandRange(-0.15f, 0.15f), 0.0f) * Spacing;

		if (UNiagaraComponent* More = VaelEffects::Attach(Visual, Disc, NAME_None, GetLookColor(), Radius))
		{
			More->SetWorldLocation(Disc->GetComponentLocation() + Offset);
			More->SetWorldScale3D(FVector(Scale * FMath::FRandRange(0.8f, 1.15f)));
		}
	}
}
