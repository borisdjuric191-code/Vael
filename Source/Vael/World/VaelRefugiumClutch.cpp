// Copyright Epic Games, Inc. All Rights Reserved.

#include "World/VaelRefugiumClutch.h"
#include "UI/VaelNoticeSubsystem.h"
#include "World/VaelBalance.h"
#include "World/VaelBalanceBoard.h"

#include "Components/PointLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Nature/VaelGarden.h"
#include "Player/VaelCharacter.h"
#include "Player/VaelPlayerController.h"
#include "Save/VaelSaveSubsystem.h"
#include "Vael.h"
#include "Story/VaelQuestSubsystem.h"
#include "World/VaelTimeOfDay.h"

#define LOCTEXT_NAMESPACE "VaelRefugiumClutch"

namespace
{
	/** Glow of each state: warm and dim asleep, bright and breathing awake, gone as a bed */
	const FLinearColor ClutchGlowColor(1.0f, 0.45f, 0.12f);
	constexpr float ClutchSleepingGlow = 3.0f;
	constexpr float ClutchAwakeGlow = 22.0f;
	constexpr float ClutchFlashGlow = 120.0f;
	/** A bed keeps a faint warm glow in its soil, so it does not sink into the dark */
	constexpr float ClutchBedGlow = 2.0f;

	/** Seconds the clutch takes to tear open into a bed */
	constexpr float ClutchOpenSeconds = 1.6f;

	FName StageFlag(FName Refugium, int32 Stage)
	{
		return FName(*FString::Printf(TEXT("Refugium.%s.Stufe%d"), *Refugium.ToString(), Stage));
	}
}

AVaelRefugiumClutch::AVaelRefugiumClutch()
{
	PrimaryActorTick.bCanEverTick = true;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));

	Sleeping = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Sleeping"));
	Sleeping->SetupAttachment(RootComponent);
	Bed = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Bed"));
	Bed->SetupAttachment(RootComponent);
	Glow = CreateDefaultSubobject<UPointLightComponent>(TEXT("Glow"));
	Glow->SetupAttachment(RootComponent);
	Glow->SetRelativeLocation(FVector(0.0f, 0.0f, 70.0f));
	Glow->SetLightColor(ClutchGlowColor);
	Glow->SetIntensityUnits(ELightUnits::Candelas);
	Glow->SetIntensity(ClutchSleepingGlow);
	Glow->SetAttenuationRadius(260.0f);
	Glow->SetCastShadows(false);

	SleepingMesh = TSoftObjectPtr<UStaticMesh>(FSoftObjectPath(TEXT("/Game/Vael/Refugium/Glutkammer/Probe/SM_GK_Gelege_Schlafend.SM_GK_Gelege_Schlafend")));
	BedMesh = TSoftObjectPtr<UStaticMesh>(FSoftObjectPath(TEXT("/Game/Vael/Refugium/Glutkammer/Probe/SM_GK_Gelegebeet.SM_GK_Gelegebeet")));
}

void AVaelRefugiumClutch::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	Sleeping->SetStaticMesh(SleepingMesh.LoadSynchronous());
	Bed->SetStaticMesh(BedMesh.LoadSynchronous());
	Sleeping->SetVisibility(Stage > 1);
	Bed->SetVisibility(Stage <= 1);
	Glow->SetVisibility(true);
}

int32 AVaelRefugiumClutch::GetReachedStage(const UObject* WorldContext, FName InRefugium)
{
	const UVaelQuestSubsystem* Quests = UVaelQuestSubsystem::Get(WorldContext);
	int32 Reached = 1;
	for (int32 Stage = 2; Quests != nullptr && Stage <= 5; ++Stage)
	{
		if (Quests->HasFlag(StageFlag(InRefugium, Stage)))
		{
			Reached = Stage;
		}
	}
	return Reached;
}

void AVaelRefugiumClutch::SetReachedStage(const UObject* WorldContext, FName InRefugium, int32 InStage)
{
	UVaelQuestSubsystem* Quests = UVaelQuestSubsystem::Get(WorldContext);
	if (Quests == nullptr)
	{
		return;
	}
	for (int32 Stage = 2; Stage <= FMath::Clamp(InStage, 1, 5); ++Stage)
	{
		Quests->SetFlag(StageFlag(InRefugium, Stage));
	}
	const UWorld* World = WorldContext != nullptr ? WorldContext->GetWorld() : nullptr;
	for (TActorIterator<AVaelRefugiumClutch> It(const_cast<UWorld*>(World)); World != nullptr && It; ++It)
	{
		It->Refresh();
	}
}

void AVaelRefugiumClutch::HatchAllAwake(const UObject* WorldContext)
{
	const UWorld* World = WorldContext != nullptr ? WorldContext->GetWorld() : nullptr;
	for (TActorIterator<AVaelRefugiumClutch> It(const_cast<UWorld*>(World)); World != nullptr && It; ++It)
	{
		if (It->State == EVaelClutchState::Awake)
		{
			It->Hatch(false);
		}
	}
}

FName AVaelRefugiumClutch::HatchedFlag() const
{
	return FName(*FString::Printf(TEXT("Refugium.%s.Beet.%s"), *Refugium.ToString(), *PlaceId.ToString()));
}

void AVaelRefugiumClutch::BeginPlay()
{
	Super::BeginPlay();
	Sleeping->SetStaticMesh(SleepingMesh.LoadSynchronous());
	Bed->SetStaticMesh(BedMesh.LoadSynchronous());

#if !UE_BUILD_SHIPPING
	// Tests and pictures: -VaelRefugiumStufe=3 sets the reached stage when the level starts
	int32 TestStage = 0;
	if (FParse::Value(FCommandLine::Get(), TEXT("VaelRefugiumStufe="), TestStage))
	{
		SetReachedStage(this, Refugium, TestStage);
	}
#endif

	Refresh();
	if (UVaelTimeOfDay* Clock = UVaelTimeOfDay::Get(this))
	{
		PhaseHandle = Clock->OnPhaseChanged.AddUObject(this, &AVaelRefugiumClutch::OnDayPhase);
	}
	UVaelInteractionSubsystem::Register(this);
}

void AVaelRefugiumClutch::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UVaelTimeOfDay* Clock = UVaelTimeOfDay::Get(this))
	{
		Clock->OnPhaseChanged.Remove(PhaseHandle);
	}
	UVaelInteractionSubsystem::Unregister(this);
	Super::EndPlay(EndPlayReason);
}

void AVaelRefugiumClutch::Refresh()
{
	if (State == EVaelClutchState::Bed)
	{
		return;
	}
	const UVaelQuestSubsystem* Quests = UVaelQuestSubsystem::Get(this);
	if (Stage <= 1 || (Quests != nullptr && Quests->HasFlag(HatchedFlag())))
	{
		if (Stage > 1)
		{
			UE_LOG(LogVael, Log, TEXT("Refugium %s: bed %s is open from the save"), *Refugium.ToString(), *PlaceId.ToString());
		}
		Hatch(true);
		return;
	}
	// Boris, 7 November 2026: a clutch tears open as soon as the story reaches its stage (no waiting for dawn)
	if (GetReachedStage(this, Refugium) >= Stage)
	{
		Hatch(false);
		return;
	}
	State = EVaelClutchState::Sleeping;
	Sleeping->SetVisibility(true);
	Bed->SetVisibility(false);
	Bed->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Glow->SetIntensity(State == EVaelClutchState::Awake ? ClutchAwakeGlow : ClutchSleepingGlow);
}

void AVaelRefugiumClutch::OnDayPhase(EVaelDayPhase Phase)
{
	// An awake clutch has had its night: it hatches with the first light
	if (Phase == EVaelDayPhase::Dawn && State == EVaelClutchState::Awake)
	{
		Hatch(false);
	}
}

void AVaelRefugiumClutch::Hatch(bool bInstant)
{
	if (State == EVaelClutchState::Bed)
	{
		return;
	}
	State = EVaelClutchState::Bed;
	if (UVaelQuestSubsystem* Quests = UVaelQuestSubsystem::Get(this); Quests != nullptr && Stage > 1)
	{
		Quests->SetFlag(HatchedFlag());
		UVaelSaveSubsystem::RequestSave(this);
	}
	Bed->SetVisibility(true);
	Bed->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	if (bInstant)
	{
		Sleeping->SetVisibility(false);
		Sleeping->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Bed->SetRelativeScale3D(FVector::OneVector);
		Glow->SetVisibility(true);
		Glow->SetIntensity(ClutchBedGlow);
		Glow->SetRelativeLocation(FVector(0.0f, 0.0f, 95.0f));
		Opening = -1.0f;
	}
	else
	{
		Opening = 0.0f;
		Glow->SetIntensity(ClutchFlashGlow);
	}
}

void AVaelRefugiumClutch::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	Breath += DeltaSeconds;

	if (bGuide && State == EVaelClutchState::Bed && Opening < 0.0f)
	{
		Glow->SetIntensity(25.0f + 20.0f * (0.5f + 0.5f * FMath::Sin(Breath * 3.0f)));
	}
	if (State == EVaelClutchState::Awake)
	{
		// awake: the glow breathes, the clutch swells a little with it
		const float Pulse = 0.5f + 0.5f * FMath::Sin(Breath * 2.2f);
		Glow->SetIntensity(ClutchAwakeGlow * (0.6f + 0.8f * Pulse));
		Sleeping->SetRelativeScale3D(FVector(1.0f + 0.03f * Pulse));
	}
	else if (Opening >= 0.0f)
	{
		// tearing open: the sleeping clutch sinks and shrinks, the bed rises out of it, the flash fades
		Opening += DeltaSeconds;
		const float T = FMath::Clamp(Opening / ClutchOpenSeconds, 0.0f, 1.0f);
		const float Ease = FMath::InterpEaseOut(0.0f, 1.0f, T, 2.5f);
		Sleeping->SetRelativeScale3D(FVector(FMath::Lerp(1.08f, 0.2f, Ease), FMath::Lerp(1.08f, 0.2f, Ease), FMath::Lerp(1.0f, 0.05f, Ease)));
		Bed->SetRelativeScale3D(FVector(FMath::Lerp(0.3f, 1.0f, Ease), FMath::Lerp(0.3f, 1.0f, Ease), FMath::Lerp(0.1f, 1.0f, Ease)));
		Glow->SetIntensity(FMath::Lerp(ClutchFlashGlow, ClutchBedGlow, T));
		if (T >= 1.0f)
		{
			Sleeping->SetVisibility(false);
			Sleeping->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			Opening = -1.0f;
		}
	}

#if !UE_BUILD_SHIPPING
	// Pictures: -VaelGelegeSchluepfen=4 lets the awake clutches hatch after that many seconds
	static float HatchAfter = -2.0f;
	if (HatchAfter == -2.0f)
	{
		HatchAfter = -1.0f;
		FParse::Value(FCommandLine::Get(), TEXT("VaelGelegeSchluepfen="), HatchAfter);
	}
	if (HatchAfter >= 0.0f && State == EVaelClutchState::Awake && GetWorld()->GetTimeSeconds() >= HatchAfter)
	{
		Hatch(false);
	}
#endif
}

bool AVaelRefugiumClutch::CanInteract(const AVaelCharacter* Player) const
{
	return State == EVaelClutchState::Awake || (State == EVaelClutchState::Bed && Opening < 0.0f && !HasPlant());
}

void AVaelRefugiumClutch::Interact(AVaelCharacter* Player)
{
	// tending the awake clutch (warming it) lets it hatch at once
	if (State == EVaelClutchState::Awake)
	{
		Hatch(false);
		return;
	}
	// an empty bed: the player picks a seed from the bag
	if (AVaelPlayerController* Controller = Player != nullptr ? Player->GetController<AVaelPlayerController>() : nullptr)
	{
		Controller->OpenSowing(this);
	}
}

FText AVaelRefugiumClutch::GetInteractPrompt() const
{
	return State == EVaelClutchState::Awake ? LOCTEXT("Tend", "Gelege wärmen") : LOCTEXT("Sow", "Säen");
}

bool AVaelRefugiumClutch::HasPlant() const
{
	const UVaelGardenSubsystem* Garden = UVaelGardenSubsystem::Get(this);
	return Garden != nullptr && Garden->HasPlantNear(GetWorld(), GetActorLocation(), 55.0f);
}

void AVaelRefugiumClutch::SetGuide(bool bOn)
{
	bGuide = bOn;
	Glow->SetVisibility(true);
	Glow->SetLightColor(bOn ? FLinearColor(1.0f, 0.85f, 0.4f) : ClutchGlowColor);
	Glow->SetIntensity(bOn ? 40.0f : ClutchBedGlow);
}

bool AVaelRefugiumClutch::SowFrom(AVaelCharacter* Player, FName PlantId, bool bShadow)
{
	UVaelGardenSubsystem* Garden = UVaelGardenSubsystem::Get(this);
	if (Garden == nullptr || State != EVaelClutchState::Bed || HasPlant())
	{
		return false;
	}
	// the soil of the bed sits 42 cm up inside the clutch (Tools/Refugium/glutkammer_probe.py, build_clutch_bed)
	const FVector Soil = GetActorLocation() + FVector(0.0f, 0.0f, 44.0f * GetActorScale3D().Z);
	if (Garden->SowAt(Player, PlantId, Soil, bShadow) == nullptr)
	{
		return false;
	}
	// The guided first bed: the board's lesson ends with what this bed will do to the land
	if (bGuide)
	{
		SetGuide(false);
		if (UVaelQuestSubsystem* Quests = UVaelQuestSubsystem::Get(this))
		{
			Quests->SetFlag(AVaelBalanceBoard::FirstBedFlag);
		}
		UVaelNoticeSubsystem::Post(this, LOCTEXT("FirstBed", "Dein erstes Beet"),
			bShadow ? LOCTEXT("FirstBedShadow", "Eine Schattenform: deine Waage ist gestiegen, und solange sie wächst, wächst auch das Nid der Aschenmark. Die Wurzeltafel zeigt es.")
					: LOCTEXT("FirstBedLight", "Eine Lichtform: solange sie wächst, nimmt sie der Aschenmark nach und nach ihr Nid. Die Wurzeltafel zeigt es."),
			bShadow ? UVaelBalanceSubsystem::GetZoneColor(EVaelBalanceZone::Shadow) : UVaelBalanceSubsystem::GetZoneColor(EVaelBalanceZone::Light), 7.0f);
	}
	return true;
}

#undef LOCTEXT_NAMESPACE
