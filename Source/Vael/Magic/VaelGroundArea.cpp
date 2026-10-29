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
#include "World/VaelAtmosphere.h"
#include "World/VaelWeatherRain.h"
#include "World/VaelRegion.h"
#include "World/VaelWorldSettings.h"
#include "World/VaelFireSpot.h"
#include "World/VaelFireComponent.h"
#include "World/VaelGround.h"
#include "Magic/VaelEnvironmentStatics.h"
#include "Magic/VaelCloudCards.h"

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
	if (CloudCards != nullptr)
	{
		SetActorTickEnabled(true);
	}
}

void AVaelGroundArea::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (CloudCards != nullptr)
	{
		UpdateVeil(DeltaSeconds);
	}
	if (bSteamCloud)
	{
		WetInSteam(DeltaSeconds);
	}

	// A cloud of steam cast around its caster goes along with them
	if (bSteamCloud && bFollowsInstigator && GetInstigator() != nullptr)
	{
		const APawn* Caster = GetInstigator();
		SetActorLocation(Caster->GetActorLocation() - FVector(0.0f, 0.0f, Caster->GetSimpleCollisionHalfHeight() - 2.0f));
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
		if (!IsInRange(It->GetActorLocation()) || It->IsAboveGround())
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

	// Fire laid in mist doesn't burn: the mist there boils into a small cloud of steam
	if (InElement == EVaelElement::Fire && InEffect == EVaelGroundEffect::None && QuenchInMist(World, Location))
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
			UVaelAtmosphere::Add(World, Area->GetActorLocation(), EVaelAir::Moisture, UVaelWorldSettings::Get()->QuenchMoisture);

			Area->Destroy();
			++NumExtinguished;
		}
	}

	return NumExtinguished;
}

AVaelGroundArea* AVaelGroundArea::FindEffectAt(const UWorld* World, const FVector& Location, EVaelGroundEffect InEffect)
{
	for (TActorIterator<AVaelGroundArea> It(World); World != nullptr && It; ++It)
	{
		if (It->Effect == InEffect && !It->IsActorBeingDestroyed() && It->IsInRange(Location))
		{
			return *It;
		}
	}
	return nullptr;
}

bool AVaelGroundArea::QuenchInMist(UWorld* World, const FVector& Location)
{
	const AVaelGroundArea* Mist = FindEffectAt(World, Location, EVaelGroundEffect::Veil);
	if (Mist == nullptr)
	{
		return false;
	}
	// The steam blinds the enemies of whoever laid the mist
	UE_LOG(LogVael, Log, TEXT("Fire boils mist into steam at %s"), *Location.ToCompactString());
	UVaelAtmosphere::Add(World, Location, EVaelAir::Moisture, UVaelWorldSettings::Get()->QuenchMoisture);
	const UVaelMagicSettings* MagicSettings = UVaelMagicSettings::Get();
	if (FindEffectAt(World, Location, EVaelGroundEffect::Blind) == nullptr)
	{
		SpawnArea(World, Location, EVaelElement::Water, MagicSettings->MistFireSteamRadius, MagicSettings->MistFireSteamSeconds, 0.0f, Mist->GetInstigator(), true, EVaelGroundEffect::Blind);
	}
	return true;
}

int32 AVaelGroundArea::BlowClouds(const UWorld* World, const FVector& Origin, const FVector& Direction, float Range, float HalfAngleDegrees, float Distance)
{
	const float MinCosine = FMath::Cos(FMath::DegreesToRadians(HalfAngleDegrees));
	const FVector Flat = Direction.GetSafeNormal2D();
	int32 NumBlown = 0;
	for (TActorIterator<AVaelGroundArea> It(World); World != nullptr && It; ++It)
	{
		AVaelGroundArea* Area = *It;
		if (Area->CloudCards == nullptr)
		{
			continue;
		}
		// Caught by the wind: its middle in the cone, or the cone's reach ends inside it
		const FVector ToArea = Area->GetActorLocation() - Origin;
		const bool bInCone = ToArea.Size2D() <= Range + Area->Radius * 0.5f && FVector::DotProduct(ToArea.GetSafeNormal2D(), Flat) >= MinCosine;
		if (!bInCone && !Area->IsInRange(Origin + Flat * Range))
		{
			continue;
		}
		// A cloud that follows its caster stays with them and only thins; the rest drifts off with the wind
		if (!Area->bFollowsInstigator)
		{
			Area->SetActorLocation(Area->GetActorLocation() + Flat * Distance);
		}
		const float Remaining = Area->GetLifeSpan();
		if (Remaining > 0.0f)
		{
			Area->SetLifeSpan(FMath::Max(Remaining * UVaelMagicSettings::Get()->WindCloudLifeShare, 0.3f));
		}
		++NumBlown;
	}
	if (NumBlown > 0)
	{
		UE_LOG(LogVael, Log, TEXT("Wind blows %d clouds away"), NumBlown);
	}
	return NumBlown;
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
	const AVaelCharacterBase* VictimCharacter = Cast<AVaelCharacterBase>(Victim);
	if (World == nullptr || (VictimCharacter != nullptr && VictimCharacter->IsAboveGround()))
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

void AVaelGroundArea::ShowRain()
{
	// The rain itself is the look: no disc, no effect of its element
	Disc->SetVisibility(false);
	TArray<UNiagaraComponent*> Visuals;
	GetComponents<UNiagaraComponent>(Visuals);
	for (UNiagaraComponent* Visual : Visuals)
	{
		Visual->DestroyComponent();
	}

	RainLook = AVaelWeatherRain::SpawnPatch(GetWorld(), GetActorLocation(), Radius, Lifetime > 0.0f ? Lifetime : 9.0f);
}

void AVaelGroundArea::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// A called rain stops with its area
	if (RainLook.IsValid())
	{
		RainLook->StopPatch();
	}

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
	UMaterialInterface* CloudMaterial = VaelAssets::LoadOptional(MagicSettings->CloudCardMaterial);
	if (CloudMaterial == nullptr)
	{
		return false;
	}
	bSteamCloud = Effect == EVaelGroundEffect::Blind;
	const FVector Center = GetActorLocation();

	// Water or fire close by feed the mist: it swells to half again its size (Boris 2026-10-25)
	if (!bSteamCloud && (UVaelEnvironmentStatics::IsElementInEnvironment(GetInstigator(), EVaelElement::Water) || UVaelEnvironmentStatics::IsElementInEnvironment(GetInstigator(), EVaelElement::Fire)))
	{
		Radius *= MagicSettings->MistSourceSwell;
		RefreshLook();
	}

	CloudCards = NewObject<UVaelCloudCardsComponent>(this);
	CloudCards->SetupAttachment(RootComponent);
	CloudCards->SetUsingAbsoluteScale(true);
	CloudCards->SetUsingAbsoluteRotation(true);
	CloudCards->RegisterComponent();
	CloudCards->SetWorldScale3D(FVector::OneVector);
	CloudCards->Init(CloudMaterial);

	const auto AddCard = [this](const FVector& Place, float Size, float Strength, bool bFlat)
	{
		CloudCards->AddCard(Place, Size, 0.0f, bFlat);
		CardPlaces.Add(Place);
		CardDistances.Add(Place.Size2D());
		CardStrengths.Add(Strength);
		CardSizes.Add(Size);
		CardPhases.Add(FMath::FRandRange(0.0f, UE_TWO_PI));
	};

	if (bSteamCloud)
	{
		// Steam: a ball of cloud on the ground, its cards spread through it and thickest towards its skin; cast at the
		// caster's feet it is theirs and goes along with them
		const APawn* Caster = GetInstigator();
		bFollowsInstigator = Caster != nullptr && FVector::Dist2D(Caster->GetActorLocation(), Center) < Radius * 0.25f;
		const FVector Middle(0.0f, 0.0f, Radius * MagicSettings->SteamBallLift);
		for (int32 Index = 0; Index < MagicSettings->SteamCards; ++Index)
		{
			// Evenly over the ball (golden angle), at a depth between the middle and the skin
			const float Height = 1.0f - 2.0f * (Index + 0.5f) / MagicSettings->SteamCards;
			const float Across = FMath::Sqrt(1.0f - FMath::Square(Height));
			const float Around = Index * 2.39996f;
			const float Depth = FMath::Lerp(0.25f, 0.95f, FMath::Sqrt(FMath::FRand()));
			const FVector Place = Middle + FVector(FMath::Cos(Around) * Across, FMath::Sin(Around) * Across, Height * 0.85f) * Radius * Depth;
			AddCard(Place, Radius * FMath::FRandRange(0.75f, 1.1f), FMath::FRandRange(0.55f, 0.85f), false);
		}
	}
	else
	{
		// Mist: flat cards lying over the ground as a blanket, upright see-through wisps above it, thinner the higher
		const float Reach = Radius * MagicSettings->MistReachShare;
		const float Spacing = MagicSettings->MistCardSpacing;
		for (float X = -Reach; X <= Reach; X += Spacing)
		{
			for (float Y = -Reach; Y <= Reach; Y += Spacing)
			{
				const FVector2D Spot = FVector2D(X, Y) + FMath::RandPointInCircle(Spacing * 0.4f);
				if (Spot.Size() > Reach)
				{
					continue;
				}
				FHitResult GroundHit;
				const FVector Above = Center + FVector(Spot.X, Spot.Y, 150.0f);
				const float Ground = VaelGround::TraceGround(GetWorld(), Above, Above - FVector(0.0f, 0.0f, 450.0f), GroundHit, GetInstigator()) ? GroundHit.Location.Z - Center.Z : 0.0f;
				AddCard(FVector(Spot.X, Spot.Y, Ground + FMath::FRandRange(8.0f, MagicSettings->MistBlanketHeight * 0.7f)), Spacing * 2.3f, 1.0f, true);
				if (FMath::FRand() < MagicSettings->MistWispShare)
				{
					const float Lift = FMath::FRandRange(MagicSettings->MistBlanketHeight + 20.0f, 200.0f);
					AddCard(FVector(Spot.X, Spot.Y, Ground + Lift), Spacing * 2.0f, FMath::GetMappedRangeValueClamped(FVector2D(MagicSettings->MistBlanketHeight, 200.0f), FVector2D(0.45f, 0.12f), Lift), false);
				}
			}
		}
	}
	CloudCards->Commit();
	Disc->SetVisibility(false);
	return true;
}

void AVaelGroundArea::SetBreathOrigin(const FVector& WorldLocation, const FVector& Forward)
{
	bBlown = true;
	BreathOrigin = WorldLocation - GetActorLocation();
	BreathForward = FVector(Forward.X, Forward.Y, 0.0f).GetSafeNormal();
	BreathReach = 1.0f;
	for (const FVector& Place : CardPlaces)
	{
		BreathReach = FMath::Max(BreathReach, FVector::Dist(Place, BreathOrigin));
	}
}

void AVaelGroundArea::UpdateVeil(float DeltaSeconds)
{
	const UVaelMagicSettings* MagicSettings = UVaelMagicSettings::Get();
	CloudAge += DeltaSeconds;
	const float Remaining = GetLifeSpan() > 0.0f ? GetLifeSpan() : 1.0f;
	const float Fade = FMath::Clamp(Remaining / MagicSettings->MistFadeSeconds, 0.0f, 1.0f);

	// Blown out of the mouth (Boris 2026-10-30): the cards leave the lips nearest first, fly out small and swell to their place
	if (bBlown)
	{
		const float Stream = MagicSettings->BreathStreamSeconds;
		const FVector Drift = FVector(FMath::Sin(CloudAge * 0.13f), FMath::Cos(CloudAge * 0.11f), 0.0f) * MagicSettings->MistDrift;
		for (int32 Index = 0; Index < CloudCards->Num(); ++Index)
		{
			const float Leave = FVector::Dist(CardPlaces[Index], BreathOrigin) / BreathReach * Stream * 0.6f;
			const float Flight = FMath::Clamp((CloudAge - Leave) / (Stream * 0.4f), 0.0f, 1.0f);
			const float Out = 1.0f - FMath::Square(1.0f - Flight);
			const float Phase = CardPhases[Index] + CloudAge * MagicSettings->SteamChurn;
			const FVector Stir = bSteamCloud
				? FVector(FMath::Sin(Phase), FMath::Cos(Phase * 0.8f), FMath::Sin(Phase * 1.3f) * 0.6f) * Radius * 0.08f
				: Drift * (CardStrengths[Index] < 1.0f ? 1.6f : 1.0f) + FVector(FMath::Sin(CloudAge * 0.4f + CardPhases[Index]), -FMath::Sin(CloudAge * 0.4f + CardPhases[Index]), 0.0f) * 12.0f;
			// The breath first shoots out ahead of the face, then billows round to where the card settles (a bent path)
			const FVector Settle = CardPlaces[Index] + Stir;
			const FVector Ahead = BreathOrigin + BreathForward * FMath::Max(120.0f, FVector::Dist(Settle, BreathOrigin) * 0.6f);
			const FVector Place = FMath::Lerp(FMath::Lerp(BreathOrigin, Ahead, Out), FMath::Lerp(Ahead, Settle, Out), Out);
			CloudCards->SetCard(Index, Place, CardSizes[Index] * FMath::Lerp(0.25f, 1.0f, Out), CardStrengths[Index] * FMath::Min(1.0f, Flight * 3.0f) * Fade);
		}
		CloudCards->Commit();
		return;
	}

	if (bSteamCloud)
	{
		// Steam billows in: the cloud swells up in a moment, then churns slowly, every card on its own small round
		const float Grow = FMath::Clamp(CloudAge / 0.6f, 0.0f, 1.0f);
		const float Swell = 1.0f - FMath::Square(1.0f - Grow);
		const FVector Middle(0.0f, 0.0f, Radius * MagicSettings->SteamBallLift);
		for (int32 Index = 0; Index < CloudCards->Num(); ++Index)
		{
			const float Phase = CardPhases[Index] + CloudAge * MagicSettings->SteamChurn;
			const FVector Churn = FVector(FMath::Sin(Phase), FMath::Cos(Phase * 0.8f), FMath::Sin(Phase * 1.3f) * 0.6f) * Radius * 0.08f;
			const FVector Place = Middle + (CardPlaces[Index] - Middle) * Swell + Churn;
			CloudCards->SetCard(Index, Place, CardSizes[Index] * (0.6f + 0.4f * Swell) * (1.0f + 0.06f * FMath::Sin(Phase * 0.7f)), CardStrengths[Index] * Swell * Fade);
		}
		CloudCards->Commit();
		return;
	}

	// The mist wells out evenly from the middle: a card shows once the front has passed it; the wisps follow a little behind,
	// all drift slowly with the air, and in the last moments the whole veil thins away
	const float Reach = Radius * MagicSettings->MistReachShare;
	const float Front = Reach * FMath::Clamp(CloudAge / MagicSettings->VeilPullSeconds, 0.0f, 1.0f);
	const float EdgeWidth = Reach * 0.25f;
	const FVector Drift = FVector(FMath::Sin(CloudAge * 0.13f), FMath::Cos(CloudAge * 0.11f), 0.0f) * MagicSettings->MistDrift;
	for (int32 Index = 0; Index < CloudCards->Num(); ++Index)
	{
		const bool bWisp = CardStrengths[Index] < 1.0f;
		const float Behind = bWisp ? Front - EdgeWidth * 0.5f : Front;
		const float Edge = FMath::Clamp((Behind - CardDistances[Index]) / EdgeWidth, 0.0f, 1.0f);
		const float Sway = FMath::Sin(CloudAge * 0.4f + CardPhases[Index]);
		const FVector Place = CardPlaces[Index] + Drift * (bWisp ? 1.6f : 1.0f) + FVector(Sway, -Sway, 0.0f) * 12.0f;
		CloudCards->SetCard(Index, Place, CardSizes[Index], CardStrengths[Index] * Edge * Fade);
	}
	CloudCards->Commit();
}

void AVaelGroundArea::WetInSteam(float DeltaSeconds)
{
	// Who stays in the steam longer than a moment gets wet; who leaves starts again
	const UVaelMagicSettings* MagicSettings = UVaelMagicSettings::Get();
	for (TActorIterator<AVaelCharacterBase> It(GetWorld()); It; ++It)
	{
		if (!IsInRange(It->GetActorLocation()) || !UVaelCombatStatics::CanDamage(GetInstigator(), *It))
		{
			SteamTimes.Remove(*It);
			continue;
		}
		float& Time = SteamTimes.FindOrAdd(*It);
		Time += DeltaSeconds;
		if (Time >= MagicSettings->SteamWetDelay)
		{
			UVaelCombatStatics::ApplyStatus(GetInstigator(), *It, EVaelStatus::Wet, MagicSettings->SteamWetSeconds);
			Time = MagicSettings->SteamWetDelay - 0.5f;
		}
	}
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
