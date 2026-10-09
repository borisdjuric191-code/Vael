// Copyright Epic Games, Inc. All Rights Reserved.

#include "Nature/VaelHarvestable.h"
#include "UObject/ConstructorHelpers.h"
#include "Combat/VaelCharacterBase.h"
#include "Combat/VaelCombatStatics.h"
#include "Compendium/VaelCompendiumSettings.h"
#include "Compendium/VaelCompendiumSubsystem.h"
#include "Components/PointLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "Items/VaelMaterialBag.h"
#include "Kismet/GameplayStatics.h"
#include "Components/AudioComponent.h"
#include "Sound/SoundAttenuation.h"
#include "Sound/SoundWaveProcedural.h"
#include "World/VaelMarkSource.h"
#include "Magic/VaelGroundArea.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Nature/VaelHarvestableData.h"
#include "Player/VaelCharacter.h"
#include "TimerManager.h"
#include "Vael.h"
#include "World/VaelRegion.h"

#define LOCTEXT_NAMESPACE "VaelNature"

namespace
{
	/** Seconds between two looks for researchers nearby */
	constexpr float HarvestableResearchInterval = 0.5f;

	/** Seconds between two looks at the region: corruption, weather */
	constexpr float HarvestableSurroundingsInterval = 1.0f;

	/** Seconds between two looks for someone touching it */
	constexpr float HarvestableTouchInterval = 0.25f;

	/** A gathered plant or fungus keeps this share of its stem */
	constexpr float HarvestedStemShare = 0.35f;

	/** A mined stone darkens to this share of its color, a wilted plant to this grey */
	constexpr float MinedStoneShade = 0.55f;
	const FLinearColor WiltedColor(0.06f, 0.055f, 0.05f);

	/** The engine's basic shapes are 100 cm across */
	constexpr float BasicShapeSize = 100.0f;

	/** Glow of a lantern plant in a pure and in a sick region, and of a burning crown */
	constexpr float PureGlowIntensity = 2600.0f;
	constexpr float SickGlowIntensity = 300.0f;
	constexpr float BurningGlowIntensity = 5000.0f;

	/** Above this corruption a lantern plant glows cold in its trait color */
	constexpr float ColdGlowCorruption = 60.0f;

	/** Seconds a drop takes to fall, and its size in cm */
	constexpr float DropFallSeconds = 0.6f;
	constexpr float DropSize = 9.0f;

	/** A breathing crown swells by this share, once every this many seconds */
	constexpr float BreathDepth = 0.12f;
	constexpr float BreathSeconds = 3.2f;

	/** A soaked crown swells by this share */
	constexpr float SoakedSwell = 1.25f;

	/** A burning crown grows by this share and flickers */
	constexpr float BurningSwell = 1.3f;

	/** A whistling cap shakes this many seconds */
	constexpr float WhistleShakeSeconds = 1.4f;

	/** A puffing ball swells to this size within this many seconds, then lies empty */
	constexpr float PuffSwell = 2.6f;
	constexpr float PuffSeconds = 0.35f;

	/** Veins stretch towards the Mark up to this share longer; all of them twitch for a moment once every this many seconds */
	constexpr float VeinStretch = 1.8f;
	constexpr float VeinTwitchEvery = 17.0f;
	constexpr float VeinTwitchSeconds = 0.35f;

	/** A cooled glowing stone shows this grey */
	const FLinearColor CooledStoneColor(0.09f, 0.08f, 0.075f);

	/** The whistle tone: sample rate, length and the pitch it glides between */
	constexpr int32 WhistleSampleRate = 22050;
	constexpr float WhistleSeconds = 1.4f;
	constexpr float WhistleLowHz = 1450.0f;
	constexpr float WhistleHighHz = 1720.0f;

	/** A thin pipe tone gliding up with a slight vibrato and some breath, faded in and out; made once, 16 bit mono */
	const TArray<uint8>& GetWhistleSamples()
	{
		static TArray<uint8> Bytes;
		if (!Bytes.IsEmpty())
		{
			return Bytes;
		}

		const int32 NumSamples = FMath::RoundToInt(WhistleSampleRate * WhistleSeconds);
		Bytes.SetNumUninitialized(NumSamples * sizeof(int16));
		int16* Samples = reinterpret_cast<int16*>(Bytes.GetData());

		FRandomStream Breath(7);
		float Phase = 0.0f;

		for (int32 Index = 0; Index < NumSamples; ++Index)
		{
			const float Time = static_cast<float>(Index) / WhistleSampleRate;
			const float Share = Time / WhistleSeconds;
			const float Frequency = FMath::Lerp(WhistleLowHz, WhistleHighHz, FMath::Sqrt(Share)) + 35.0f * FMath::Sin(UE_TWO_PI * 6.0f * Time);
			Phase += UE_TWO_PI * Frequency / WhistleSampleRate;

			const float Envelope = FMath::Min(Time / 0.15f, 1.0f) * FMath::Min((WhistleSeconds - Time) / 0.45f, 1.0f);
			const float Tone = FMath::Sin(Phase) + 0.25f * FMath::Sin(2.0f * Phase) + 0.12f * Breath.FRandRange(-1.0f, 1.0f);

			Samples[Index] = static_cast<int16>(FMath::Clamp(Tone * Envelope * 0.32f, -1.0f, 1.0f) * 32767.0f);
		}

		return Bytes;
	}

	/** The closest open Mark source, null if there is none */
	const AVaelMarkSource* FindClosestOpenSource(const UWorld* World, const FVector& Location, float& OutDistance)
	{
		const AVaelMarkSource* Closest = nullptr;
		OutDistance = UE_BIG_NUMBER;

		for (TActorIterator<AVaelMarkSource> It(World); It; ++It)
		{
			const float Distance = FVector::Dist2D(It->GetActorLocation(), Location);
			if (!It->IsSealed() && Distance < OutDistance)
			{
				Closest = *It;
				OutDistance = Distance;
			}
		}

		return Closest;
	}
}

AVaelHarvestable::AVaelHarvestable()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;

	// The actor stands on the ground, the shapes grow up from that point
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));

	Base = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Base"));
	Base->SetupAttachment(RootComponent);
	Base->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Base->SetCanEverAffectNavigation(false);

	Crown = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Crown"));
	Crown->SetupAttachment(RootComponent);
	Crown->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Crown->SetCanEverAffectNavigation(false);

	Drop = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Drop"));
	Drop->SetupAttachment(RootComponent);
	Drop->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Drop->SetCanEverAffectNavigation(false);
	Drop->SetCastShadow(false);
	Drop->SetVisibility(false);
	Drop->SetRelativeScale3D(FVector(DropSize / BasicShapeSize));

	Glow = CreateDefaultSubobject<UPointLightComponent>(TEXT("Glow"));
	Glow->SetupAttachment(RootComponent);
	Glow->SetAttenuationRadius(320.0f);
	Glow->SetCastShadows(false);
	Glow->SetVisibility(false);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CylinderFinder(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereFinder(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> ConeFinder(TEXT("/Engine/BasicShapes/Cone.Cone"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> ShapeMaterialFinder(TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));

	CylinderMesh = CylinderFinder.Object;
	SphereMesh = SphereFinder.Object;
	ConeMesh = ConeFinder.Object;
	Drop->SetStaticMesh(SphereMesh);

	if (ShapeMaterialFinder.Succeeded())
	{
		Base->SetMaterial(0, ShapeMaterialFinder.Object);
		Crown->SetMaterial(0, ShapeMaterialFinder.Object);
		Drop->SetMaterial(0, ShapeMaterialFinder.Object);
	}
}

void AVaelHarvestable::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);

	RefreshLook();
}

void AVaelHarvestable::BeginPlay()
{
	Super::BeginPlay();

	if (Data == nullptr)
	{
		UE_LOG(LogVael, Warning, TEXT("Harvestable '%s' has no data and can't be gathered"), *GetNameSafe(this));
		return;
	}

	UVaelInteractionSubsystem::Register(this);

	// Spread the looks of many harvestables over their intervals
	FTimerManager& Timers = GetWorldTimerManager();
	Timers.SetTimer(ResearchTimer, this, &AVaelHarvestable::UpdateResearch, HarvestableResearchInterval, true, FMath::FRandRange(0.0f, HarvestableResearchInterval));
	Timers.SetTimer(SurroundingsTimer, this, &AVaelHarvestable::UpdateSurroundings, HarvestableSurroundingsInterval, true, FMath::FRandRange(0.0f, HarvestableSurroundingsInterval));

	switch (Data->Trait)
	{
	case EVaelHarvestTrait::IgniteOnTouch:
		Timers.SetTimer(TraitTimer, this, &AVaelHarvestable::UpdateTrait, HarvestableTouchInterval, true, FMath::FRandRange(0.0f, HarvestableTouchInterval));
		SetActorTickEnabled(true);
		break;
	case EVaelHarvestTrait::HealingDrip:
		DropCountdown = FMath::FRandRange(0.0f, Data->TraitInterval);
		SetActorTickEnabled(true);
		break;
	case EVaelHarvestTrait::PuffOnStep:
		Timers.SetTimer(TraitTimer, this, &AVaelHarvestable::UpdateTrait, HarvestableTouchInterval, true, FMath::FRandRange(0.0f, HarvestableTouchInterval));
		SetActorTickEnabled(true);
		break;
	case EVaelHarvestTrait::StormWhistle:
		WhistleCountdown = FMath::FRandRange(0.0f, Data->TraitInterval);
		WhistleAttenuation = NewObject<USoundAttenuation>(this);
		WhistleAttenuation->Attenuation.bAttenuate = true;
		WhistleAttenuation->Attenuation.AttenuationShapeExtents = FVector(300.0f);
		WhistleAttenuation->Attenuation.FalloffDistance = 2600.0f;
		SetActorTickEnabled(true);
		break;
	case EVaelHarvestTrait::Breathing:
	case EVaelHarvestTrait::MarkVeins:
		SetActorTickEnabled(true);
		break;
	default:
		break;
	}

	UpdateSurroundings();
	RefreshLook();
}

void AVaelHarvestable::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	UVaelInteractionSubsystem::Unregister(this);
	GetWorldTimerManager().ClearAllTimersForObject(this);

	Super::EndPlay(EndPlayReason);
}

FName AVaelHarvestable::GetCompendiumId() const
{
	return Data != nullptr ? Data->CompendiumId : NAME_None;
}

FText AVaelHarvestable::GetInteractPrompt() const
{
	if (Data == nullptr)
	{
		return FText::GetEmpty();
	}

	switch (Data->Kind)
	{
	case EVaelHarvestKind::Stone:	return FText::Format(LOCTEXT("Mine", "{0} abbauen"), Data->DisplayName);
	case EVaelHarvestKind::Fungus:	return FText::Format(LOCTEXT("Gather", "{0} sammeln"), Data->DisplayName);
	default:						return FText::Format(LOCTEXT("Pick", "{0} pflücken"), Data->DisplayName);
	}
}

void AVaelHarvestable::Interact(AVaelCharacter* Player)
{
	if (!CanInteract(Player))
	{
		return;
	}

	UVaelMaterialBag::GiveToGroup(this, Data->Loot);

	if (UVaelCompendiumSubsystem* Compendium = UVaelCompendiumSubsystem::Get(this))
	{
		Compendium->AddSample(Data->CompendiumId);
	}

	BecomeBare();

	UE_LOG(LogVael, Log, TEXT("'%s' (%s) gathered by player %d"), *GetNameSafe(this), *Data->CompendiumId.ToString(), Player != nullptr ? Player->GetPlayerNumber() : 0);
}

void AVaelHarvestable::BecomeBare()
{
	bHarvested = true;
	BurnEndTime = -1.0f;
	DropAge = -1.0f;
	PuffAge = -1.0f;
	RefreshLook();

	if (Data->RegrowSeconds > 0.0f)
	{
		GetWorldTimerManager().SetTimer(RegrowTimer, this, &AVaelHarvestable::Regrow, Data->RegrowSeconds, false);
	}
}

void AVaelHarvestable::Regrow()
{
	bHarvested = false;
	RefreshLook();
}

bool AVaelHarvestable::ProvidesElement(EVaelElement Element, const FVector& Location) const
{
	return IsAvailable() && !IsCooled() && Data->bElementSource && Data->SourceElement == Element && FVector::Dist2D(Location, GetActorLocation()) <= Data->SourceRadius;
}

bool AVaelHarvestable::IsCooled() const
{
	const UWorld* World = GetWorld();
	return Data != nullptr && Data->Trait == EVaelHarvestTrait::SteamOnWater && World != nullptr && World->IsGameWorld() && World->GetTimeSeconds() < NextTraitTime;
}

void AVaelHarvestable::NotifySpellImpact(const UWorld* World, const FVector& Location, float Radius, EVaelElement Element)
{
	if (World == nullptr)
	{
		return;
	}

	for (TActorIterator<AVaelHarvestable> It(World); It; ++It)
	{
		// Measured from its middle: a spell stopped by a boulder hits its side, not its crystal
		const FVector Middle = It->GetActorLocation() + FVector(0.0f, 0.0f, It->Data->Height * 0.5f);
		if (!It->IsAvailable() || FVector::Dist(Middle, Location) > Radius + It->Data->Height * 0.8f)
		{
			continue;
		}

		switch (It->Data->Trait)
		{
		case EVaelHarvestTrait::BurstOnHit:
			It->Burst();
			break;
		case EVaelHarvestTrait::SteamOnWater:
			if (Element == EVaelElement::Water && !It->IsCooled())
			{
				It->Steam();
			}
			else if (Element == EVaelElement::Fire && It->IsCooled())
			{
				// Fire heats a cooled stone up again at once
				It->NextTraitTime = 0.0f;
				It->RefreshLook();
			}
			break;
		default:
			break;
		}
	}
}

void AVaelHarvestable::Steam()
{
	// The stored heat leaves at once: hot steam that blinds the creatures standing in it
	AVaelGroundArea::SpawnArea(GetWorld(), GetActorLocation() + FVector(0.0f, 0.0f, 2.0f), EVaelElement::Water, Data->TraitRadius, Data->TraitDuration, 0.0f, nullptr, true, EVaelGroundEffect::Blind);

	NextTraitTime = GetWorld()->GetTimeSeconds() + Data->TraitInterval;
	RefreshLook();
	UE_LOG(LogVael, Verbose, TEXT("'%s' lets out steam"), *GetNameSafe(this));
}

void AVaelHarvestable::Puff()
{
	// The ash smothers flames: burning people and fires close by go out
	for (TActorIterator<AVaelCharacterBase> It(GetWorld()); It; ++It)
	{
		if (FVector::Dist2D(It->GetActorLocation(), GetActorLocation()) <= Data->TraitRadius + It->GetSimpleCollisionRadius())
		{
			UVaelCombatStatics::RemoveStatus(*It, EVaelStatus::Burning);
		}
	}

	AVaelGroundArea::ExtinguishFires(GetWorld(), GetActorLocation(), Data->TraitRadius);

	PuffAge = 0.0f;
	NextTraitTime = GetWorld()->GetTimeSeconds() + Data->TraitInterval;
	UE_LOG(LogVael, Verbose, TEXT("'%s' puffs"), *GetNameSafe(this));
}

void AVaelHarvestable::Whistle(float Loudness)
{
	WhistleEndTime = GetWorld()->GetTimeSeconds() + WhistleShakeSeconds;
	UE_LOG(LogVael, Verbose, TEXT("'%s' whistles at %.0f %%"), *GetNameSafe(this), Loudness * 100.0f);
	PlayWhistleSound(FMath::Lerp(0.25f, 0.9f, Loudness));
}

void AVaelHarvestable::PlayWhistleSound(float Volume)
{
	if (GetNetMode() == NM_DedicatedServer || !FApp::CanEverRenderAudio())
	{
		return;
	}

	USoundWaveProcedural* Wave = NewObject<USoundWaveProcedural>(this);
	Wave->SetSampleRate(WhistleSampleRate);
	Wave->NumChannels = 1;
	Wave->Duration = WhistleSeconds;
	Wave->SoundGroup = SOUNDGROUP_Default;
	Wave->bLooping = false;

	const TArray<uint8>& Samples = GetWhistleSamples();
	Wave->QueueAudio(Samples.GetData(), Samples.Num());

	UGameplayStatics::SpawnSoundAtLocation(this, Wave, Crown->GetComponentLocation(), FRotator::ZeroRotator, Volume, FMath::FRandRange(0.9f, 1.12f), 0.0f, WhistleAttenuation);
}

void AVaelHarvestable::UpdateMarkVeins()
{
	float Distance = 0.0f;
	const AVaelMarkSource* Source = FindClosestOpenSource(GetWorld(), GetActorLocation(), Distance);

	const float OldStrength = VeinStrength;
	VeinStrength = Source != nullptr ? FMath::Clamp(1.0f - Distance / FMath::Max(Data->TraitRadius, 1.0f), 0.0f, 1.0f) : 0.0f;
	VeinYaw = Source != nullptr ? (Source->GetActorLocation() - GetActorLocation()).Rotation().Yaw - GetActorRotation().Yaw : 0.0f;

	if (!FMath::IsNearlyEqual(OldStrength, VeinStrength, 0.02f))
	{
		RefreshLook();
	}
}

void AVaelHarvestable::UpdateSurroundings()
{
	AVaelRegion* Region = AVaelRegion::GetRegionAt(GetWorld(), GetActorLocation());
	const float Corruption = Region != nullptr ? Region->GetCorruption() : 0.0f;

	const bool bWasAbsent = bAbsent;
	const bool bWasWilted = bWilted;
	const bool bWasSoaked = bSoaked;
	const float OldPurity = Purity;

	bAbsent = Corruption < Data->MinCorruption;
	bWilted = !bAbsent && Corruption > Data->MaxCorruption;
	bSoaked = Data->Trait == EVaelHarvestTrait::RainSoak && Region != nullptr && Region->GetWeather() == EVaelWeather::Rain;
	Purity = 1.0f - Corruption / 100.0f;

	// The Mark's own crystal only grows while its source is open
	if (Data->Trait == EVaelHarvestTrait::NeedsOpenSource)
	{
		float Distance = 0.0f;
		bAbsent |= FindClosestOpenSource(GetWorld(), GetActorLocation(), Distance) == nullptr || Distance > Data->TraitRadius;
	}

	const bool bFollowsPurity = Data->Trait == EVaelHarvestTrait::PurityGlow || Data->Trait == EVaelHarvestTrait::PurityClarity;
	if (bAbsent != bWasAbsent || bWilted != bWasWilted || bSoaked != bWasSoaked || IsCooled() != bShownCooled
		|| (bFollowsPurity && !FMath::IsNearlyEqual(Purity, OldPurity, 0.01f)))
	{
		RefreshLook();
	}

	if (Data->Trait == EVaelHarvestTrait::MarkVeins)
	{
		UpdateMarkVeins();
	}

	// The whistling cap hears a storm before anyone else: quietly while it is coming, loud while it rages
	if (Data->Trait == EVaelHarvestTrait::StormWhistle && IsAvailable() && Region != nullptr)
	{
		float Loudness = 0.0f;
		if (Region->GetWeather() == EVaelWeather::Storm)
		{
			Loudness = 1.0f;
		}
		else if (Region->GetComingWeather() == EVaelWeather::Storm && Region->GetWeatherTimeLeft() <= Data->TraitDuration)
		{
			Loudness = 1.0f - Region->GetWeatherTimeLeft() / FMath::Max(Data->TraitDuration, 1.0f);
		}

		WhistleCountdown -= HarvestableSurroundingsInterval;
		if (Loudness > 0.0f && WhistleCountdown <= 0.0f)
		{
			Whistle(Loudness);
			WhistleCountdown = Data->TraitInterval * FMath::FRandRange(0.8f, 1.2f);
		}
	}
}

void AVaelHarvestable::UpdateTrait()
{
	const bool bTouchTrait = Data->Trait == EVaelHarvestTrait::IgniteOnTouch || Data->Trait == EVaelHarvestTrait::PuffOnStep;
	if (!IsAvailable() || !bTouchTrait || PuffAge >= 0.0f)
	{
		return;
	}

	const float Now = GetWorld()->GetTimeSeconds();
	if (Now < NextTraitTime)
	{
		return;
	}

	// A puffball bursts under a foot; the thistle already when someone brushes past
	const float Reach = Data->Trait == EVaelHarvestTrait::PuffOnStep ? Data->Height * 0.5f : Data->TraitRadius;

	for (TActorIterator<AVaelCharacterBase> It(GetWorld()); It; ++It)
	{
		if (It->GetHealth() > 0.0f && FVector::Dist2D(It->GetActorLocation(), GetActorLocation()) <= Reach + It->GetSimpleCollisionRadius())
		{
			if (Data->Trait == EVaelHarvestTrait::PuffOnStep)
			{
				Puff();
			}
			else
			{
				Ignite();
			}
			return;
		}
	}
}

void AVaelHarvestable::Ignite()
{
	const float Now = GetWorld()->GetTimeSeconds();
	BurnEndTime = Now + Data->TraitDuration;
	NextTraitTime = Now + FMath::Max(Data->TraitInterval, Data->TraitDuration);

	// Everyone close catches fire, players and creatures alike: nature takes no sides
	for (TActorIterator<AVaelCharacterBase> It(GetWorld()); It; ++It)
	{
		if (It->GetHealth() > 0.0f && FVector::Dist2D(It->GetActorLocation(), GetActorLocation()) <= Data->TraitRadius + It->GetSimpleCollisionRadius())
		{
			UVaelCombatStatics::ApplyStatus(nullptr, *It, EVaelStatus::Burning, Data->TraitDuration, Data->TraitAmount);
		}
	}

	// The flames stand on the ground for a moment: a fire mages can draw from and water can put out
	AVaelGroundArea::SpawnArea(GetWorld(), GetActorLocation() + FVector(0.0f, 0.0f, 2.0f), EVaelElement::Fire, Data->TraitRadius, Data->TraitDuration, 0.0f, nullptr);

	RefreshLook();
	UE_LOG(LogVael, Verbose, TEXT("'%s' catches fire"), *GetNameSafe(this));
}

void AVaelHarvestable::Burst()
{
	// The smoke stays where the bubble was and blinds whoever stands in it
	AVaelGroundArea::SpawnArea(GetWorld(), GetActorLocation() + FVector(0.0f, 0.0f, 2.0f), EVaelElement::Air, Data->TraitRadius, Data->TraitDuration, 0.0f, nullptr, true, EVaelGroundEffect::Blind);

	BecomeBare();
	UE_LOG(LogVael, Verbose, TEXT("'%s' bursts into smoke"), *GetNameSafe(this));
}

void AVaelHarvestable::LandDrop()
{
	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		AVaelCharacter* Player = It->Get() != nullptr ? It->Get()->GetPawn<AVaelCharacter>() : nullptr;
		if (Player != nullptr && Player->GetHealth() > 0.0f && FVector::Dist2D(Player->GetActorLocation(), GetActorLocation()) <= Data->TraitRadius)
		{
			UVaelCombatStatics::Heal(Player, Data->TraitAmount);
		}
	}
}

void AVaelHarvestable::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (Data == nullptr)
	{
		return;
	}

	const float Now = GetWorld()->GetTimeSeconds();
	const bool bAvailable = IsAvailable();

	switch (Data->Trait)
	{
	case EVaelHarvestTrait::StormWhistle:
	{
		// The cap trembles while it whistles
		if (bAvailable && Now < WhistleEndTime)
		{
			const float Shake = 4.0f * FMath::Sin(Now * 41.0f) * (WhistleEndTime - Now) / WhistleShakeSeconds;
			Crown->SetRelativeRotation(FRotator(Shake, 0.0f, Shake * 0.6f));
		}
		else if (!Crown->GetRelativeRotation().IsNearlyZero())
		{
			Crown->SetRelativeRotation(FRotator::ZeroRotator);
		}
		break;
	}
	case EVaelHarvestTrait::PuffOnStep:
	{
		// The ball swells into its cloud, then lies empty
		if (PuffAge >= 0.0f)
		{
			PuffAge += DeltaSeconds;
			const float Share = FMath::Clamp(PuffAge / PuffSeconds, 0.0f, 1.0f);
			Crown->SetRelativeScale3D(CrownScale * FMath::Lerp(1.0f, PuffSwell, Share));

			if (Share >= 1.0f)
			{
				PuffAge = -1.0f;
				BecomeBare();
			}
		}
		break;
	}
	case EVaelHarvestTrait::MarkVeins:
	{
		// Now and then every vein in the land twitches at once: something very large moves below
		const float Phase = FMath::Fmod(Now, VeinTwitchEvery);
		const float Twitch = VeinStrength > 0.2f && Phase < VeinTwitchSeconds ? FMath::Sin(Phase / VeinTwitchSeconds * UE_PI) * 0.3f : 0.0f;
		Crown->SetRelativeScale3D(CrownScale * FVector(1.0f + Twitch, 1.0f, 1.0f + Twitch * 2.0f));
		break;
	}
	case EVaelHarvestTrait::Breathing:
	{
		const float Breath = 1.0f + BreathDepth * FMath::Sin(Now * UE_TWO_PI / BreathSeconds);
		Crown->SetRelativeScale3D(CrownScale * FVector(Breath, Breath, 1.0f + (Breath - 1.0f) * 0.5f));
		break;
	}
	case EVaelHarvestTrait::IgniteOnTouch:
	{
		const bool bBurning = bAvailable && Now < BurnEndTime;
		if (bBurning)
		{
			const float Flicker = 1.0f + 0.12f * FMath::Sin(Now * 23.0f) * FMath::Sin(Now * 7.0f);
			Crown->SetRelativeScale3D(CrownScale * BurningSwell * Flicker);
			Glow->SetIntensity(BurningGlowIntensity * Flicker);
		}
		else if (Glow->IsVisible())
		{
			// Burnt down: back to its glimmer
			RefreshLook();
		}
		break;
	}
	case EVaelHarvestTrait::HealingDrip:
	{
		if (!bAvailable)
		{
			Drop->SetVisibility(false);
			DropAge = -1.0f;
			break;
		}

		if (DropAge < 0.0f)
		{
			DropCountdown -= DeltaSeconds;
			if (DropCountdown <= 0.0f)
			{
				DropAge = 0.0f;
				DropCountdown = Data->TraitInterval;
				Drop->SetVisibility(true);
			}
			break;
		}

		// The drop swells under the crown, then falls ever faster
		DropAge += DeltaSeconds;
		const float Share = FMath::Clamp(DropAge / DropFallSeconds, 0.0f, 1.0f);
		const float Top = Crown->GetRelativeLocation().Z - CrownScale.Z * BasicShapeSize * 0.5f;
		Drop->SetRelativeLocation(FVector(0.0f, 0.0f, FMath::Lerp(Top, 0.0f, Share * Share)));

		if (Share >= 1.0f)
		{
			Drop->SetVisibility(false);
			DropAge = -1.0f;
			LandDrop();
		}
		break;
	}
	default:
		break;
	}
}

void AVaelHarvestable::UpdateResearch()
{
	UVaelCompendiumSubsystem* Compendium = UVaelCompendiumSubsystem::Get(this);
	const FName SubjectId = GetCompendiumId();
	if (Compendium == nullptr || Compendium->FindEntry(SubjectId) == nullptr)
	{
		GetWorldTimerManager().ClearTimer(ResearchTimer);
		return;
	}

	// Once the kind is observed, sighting and watching have nothing more to teach
	if (Compendium->GetProgress(SubjectId).Stage >= EVaelResearchStage::Observed)
	{
		GetWorldTimerManager().ClearTimer(ResearchTimer);
		return;
	}

	// What doesn't grow here can't be seen
	if (bAbsent)
	{
		return;
	}

	// Seen from afar; watched from close by, or from further through the spyglass, every watcher counting.
	// Plants and stones never flee, so unlike creatures they need no calm.
	const UVaelCompendiumSettings* Settings = UVaelCompendiumSettings::Get();
	const FVector Location = GetActorLocation();
	const float Reach = Settings->SpyglassFocusRadius + GetSimpleCollisionRadius();

	bool bSeen = false;
	float Watched = 0.0f;

	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		const AVaelCharacter* Player = It->Get() != nullptr ? It->Get()->GetPawn<AVaelCharacter>() : nullptr;
		if (Player == nullptr)
		{
			continue;
		}

		const float Distance = FVector::Dist2D(Player->GetActorLocation(), Location);
		bSeen |= Distance <= Settings->SightRange;

		if (Player->IsObserving() && FVector::Dist2D(Player->GetFocusLocation(), Location) <= Reach)
		{
			Watched += HarvestableResearchInterval * Settings->SpyglassObserveRate;
		}
		else if (Distance <= Settings->ObserveRange)
		{
			Watched += HarvestableResearchInterval;
		}
	}

	if (bSeen)
	{
		Compendium->Sight(SubjectId);
	}

	if (Watched > 0.0f)
	{
		Compendium->Observe(SubjectId, Watched);
	}
}

void AVaelHarvestable::RefreshLook()
{
	if (Data == nullptr)
	{
		return;
	}

	const float H = Data->Height;
	const bool bStone = Data->Kind == EVaelHarvestKind::Stone;
	const bool bBare = bHarvested || bWilted;

	// What doesn't grow here isn't there at all
	SetActorHiddenInGame(bAbsent);

	// Stones block the way like any rock; plants and fungi can be walked through
	Base->SetCollisionProfileName(bStone && !bAbsent ? TEXT("BlockAll") : TEXT("NoCollision"));
	Crown->SetRelativeRotation(FRotator::ZeroRotator);

	switch (Data->Kind)
	{
	case EVaelHarvestKind::Stone:
	{
		// A squat boulder with a crystal breaking out of its top
		Base->SetStaticMesh(SphereMesh);
		Base->SetRelativeScale3D(FVector(H * 1.4f, H * 1.15f, H) / BasicShapeSize);
		Base->SetRelativeLocation(FVector(0.0f, 0.0f, H * 0.4f));

		CrownScale = FVector(H * 0.35f, H * 0.35f, H * 0.75f) / BasicShapeSize;
		Crown->SetStaticMesh(ConeMesh);
		Crown->SetRelativeLocation(FVector(H * 0.15f, 0.0f, H * 0.95f));
		Crown->SetRelativeRotation(FRotator(0.0f, 0.0f, 12.0f));
		break;
	}
	case EVaelHarvestKind::Fungus:
	{
		// A thick stalk under a flat, wide cap
		const float StalkHeight = H * 0.65f * (bBare ? HarvestedStemShare : 1.0f);

		Base->SetStaticMesh(CylinderMesh);
		Base->SetRelativeScale3D(FVector(H * 0.22f, H * 0.22f, StalkHeight) / BasicShapeSize);
		Base->SetRelativeLocation(FVector(0.0f, 0.0f, StalkHeight * 0.5f));

		CrownScale = FVector(H * 0.85f, H * 0.85f, H * 0.35f) / BasicShapeSize;
		Crown->SetStaticMesh(SphereMesh);
		Crown->SetRelativeLocation(FVector(0.0f, 0.0f, H * 0.7f));
		break;
	}
	default:
	{
		// A thin stem under a round crown
		const float StemHeight = H * 0.8f * (bBare ? HarvestedStemShare : 1.0f);

		Base->SetStaticMesh(CylinderMesh);
		Base->SetRelativeScale3D(FVector(FMath::Max(H * 0.08f, 3.0f), FMath::Max(H * 0.08f, 3.0f), StemHeight) / BasicShapeSize);
		Base->SetRelativeLocation(FVector(0.0f, 0.0f, StemHeight * 0.5f));

		CrownScale = FVector(H * 0.4f) / BasicShapeSize;
		Crown->SetStaticMesh(SphereMesh);
		Crown->SetRelativeLocation(FVector(0.0f, 0.0f, H * 0.8f));
		break;
	}
	}

	if (bSoaked)
	{
		CrownScale *= SoakedSwell;
	}

	// Veins stretch towards the Mark that colors them
	if (Data->Trait == EVaelHarvestTrait::MarkVeins)
	{
		CrownScale.X *= FMath::Lerp(1.0f, VeinStretch, VeinStrength);
		Crown->SetRelativeRotation(FRotator(0.0f, VeinYaw, 0.0f));
	}

	Crown->SetRelativeScale3D(CrownScale);

	// What is taken is gone until it grows back; a mined stone stays, but dull
	const bool bCrownShown = !bBare && !bAbsent;
	Crown->SetVisibility(bCrownShown);

	// A bubble that bursts on a hit stops spells; pawns and everything else pass through it
	const bool bCatchesSpells = bCrownShown && Data->Trait == EVaelHarvestTrait::BurstOnHit;
	Crown->SetCollisionEnabled(bCatchesSpells ? ECollisionEnabled::QueryOnly : ECollisionEnabled::NoCollision);
	if (bCatchesSpells)
	{
		Crown->SetCollisionObjectType(ECC_PhysicsBody);
		Crown->SetCollisionResponseToAllChannels(ECR_Ignore);
		Crown->SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Block);
	}

	// Colors: the region's material below, the element's glow above; wilted plants turn grey, soaked moss its trait color
	FLinearColor BaseColor = Data->BaseColor;
	if (bWilted)
	{
		BaseColor = WiltedColor;
	}
	else if (bStone && bHarvested)
	{
		BaseColor *= MinedStoneShade;
	}

	FLinearColor CrownColor = bSoaked ? Data->TraitColor : Data->GlowColor;

	// The lantern plant's light follows the health of the region and turns cold where the Mark rules
	const float Corruption = (1.0f - Purity) * 100.0f;
	const bool bColdGlow = Data->Trait == EVaelHarvestTrait::PurityGlow && Corruption >= ColdGlowCorruption;
	if (bColdGlow)
	{
		CrownColor = Data->TraitColor;
	}

	switch (Data->Trait)
	{
	case EVaelHarvestTrait::PurityClarity:
		// Smoky in a sick land, clear in a healthy one
		CrownColor = FMath::Lerp(Data->GlowColor, Data->TraitColor, FMath::Clamp(Purity, 0.0f, 1.0f));
		break;
	case EVaelHarvestTrait::MarkVeins:
		CrownColor = FMath::Lerp(Data->GlowColor, Data->TraitColor, VeinStrength);
		break;
	case EVaelHarvestTrait::SteamOnWater:
		bShownCooled = IsCooled();
		if (bShownCooled)
		{
			CrownColor = CooledStoneColor;
		}
		break;
	default:
		break;
	}

	const UWorld* World = GetWorld();
	const bool bBurning = World != nullptr && World->IsGameWorld() && World->GetTimeSeconds() < BurnEndTime;

	Glow->SetRelativeLocation(Crown->GetRelativeLocation());
	if (bCrownShown && bBurning)
	{
		Glow->SetLightColor(Data->GlowColor);
		Glow->SetIntensity(BurningGlowIntensity);
		Glow->SetVisibility(true);
	}
	else if (bCrownShown && Data->Trait == EVaelHarvestTrait::PurityGlow)
	{
		Glow->SetLightColor(CrownColor);
		Glow->SetIntensity(FMath::Lerp(SickGlowIntensity, PureGlowIntensity, FMath::Clamp(Purity, 0.0f, 1.0f)));
		Glow->SetVisibility(true);
	}
	else
	{
		Glow->SetVisibility(false);
	}

	if (UMaterialInstanceDynamic* BaseMaterial = Base->CreateAndSetMaterialInstanceDynamic(0))
	{
		BaseMaterial->SetVectorParameterValue(TEXT("Color"), BaseColor);
	}

	if (UMaterialInstanceDynamic* CrownMaterial = Crown->CreateAndSetMaterialInstanceDynamic(0))
	{
		CrownMaterial->SetVectorParameterValue(TEXT("Color"), CrownColor);
	}

	if (UMaterialInstanceDynamic* DropMaterial = Drop->CreateAndSetMaterialInstanceDynamic(0))
	{
		DropMaterial->SetVectorParameterValue(TEXT("Color"), Data->GlowColor);
	}
}

#undef LOCTEXT_NAMESPACE
