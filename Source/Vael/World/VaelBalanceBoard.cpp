// Copyright Epic Games, Inc. All Rights Reserved.

#include "World/VaelBalanceBoard.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"

#include "Components/PointLightComponent.h"
#include "EngineUtils.h"
#include "Player/VaelCharacter.h"
#include "Player/VaelPlayerController.h"
#include "Story/VaelQuestSubsystem.h"
#include "World/VaelBalance.h"
#include "World/VaelRefugiumClutch.h"

#define LOCTEXT_NAMESPACE "VaelBalanceBoard"

const FName AVaelBalanceBoard::ExplainedFlag(TEXT("Refugium.Glutkammer.WaageErklaert"));
const FName AVaelBalanceBoard::FirstBedFlag(TEXT("Refugium.Glutkammer.ErstesBeet"));

AVaelBalanceBoard::AVaelBalanceBoard()
{
	PrimaryActorTick.bCanEverTick = true;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	Glow = CreateDefaultSubobject<UPointLightComponent>(TEXT("Glow"));
	Glow->SetupAttachment(RootComponent);
	Glow->SetRelativeLocation(FVector(60.0f, 0.0f, 160.0f));
	Glow->SetIntensityUnits(ELightUnits::Candelas);
	Glow->SetIntensity(10.0f);
	Glow->SetLightColor(FLinearColor(1.0f, 0.8f, 0.5f));
	Glow->SetAttenuationRadius(350.0f);
	Glow->SetCastShadows(false);
}

void AVaelBalanceBoard::BeginPlay()
{
	Super::BeginPlay();
	UVaelInteractionSubsystem::Register(this);

	const UVaelQuestSubsystem* Quests = UVaelQuestSubsystem::Get(this);
	// pictures of the showcase skip the talk unless they ask for it (-VaelBoardIntro)
	const bool bQuietPictures = FString(FCommandLine::Get()).Contains(TEXT("-VaelShowcase="));
	const bool bSkipTalk = bQuietPictures && !FParse::Param(FCommandLine::Get(), TEXT("VaelBoardIntro"));
	if (Quests != nullptr && !Quests->HasFlag(ExplainedFlag) && !bSkipTalk)
	{
		ExplainAt = GetWorld()->GetTimeSeconds() + 2.5f;
	}
	// The first bed is shown until something grows in it: the open bed of stage I nearest to the board
	if (Quests != nullptr && !Quests->HasFlag(FirstBedFlag))
	{
		AVaelRefugiumClutch* Nearest = nullptr;
		float Best = TNumericLimits<float>::Max();
		for (TActorIterator<AVaelRefugiumClutch> It(GetWorld()); It; ++It)
		{
			const float Distance = FVector::Dist2D(It->GetActorLocation(), GetActorLocation());
			if (It->Stage <= 1 && !It->HasPlant() && Distance < Best)
			{
				Nearest = *It;
				Best = Distance;
			}
		}
		if (Nearest != nullptr)
		{
			Nearest->SetGuide(true);
		}
	}
}

void AVaelBalanceBoard::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	UVaelInteractionSubsystem::Unregister(this);
	Super::EndPlay(EndPlayReason);
}

void AVaelBalanceBoard::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (ExplainAt > 0.0f && GetWorld()->GetTimeSeconds() >= ExplainAt)
	{
		ExplainAt = -1.0f;
		Explain();
	}
	// the board breathes softly in the colour of the region's zone
	const UVaelBalanceSubsystem* Balance = UVaelBalanceSubsystem::Get(this);
	const float Drift = Balance != nullptr ? Balance->GetRefugiumDrift() : 0.0f;
	const FLinearColor Color = Drift > 0.0f ? UVaelBalanceSubsystem::GetZoneColor(EVaelBalanceZone::Shadow) : UVaelBalanceSubsystem::GetZoneColor(EVaelBalanceZone::Light);
	Glow->SetLightColor(Color);
	Glow->SetIntensity(8.0f + 4.0f * FMath::Sin(GetWorld()->GetTimeSeconds() * 1.3f));
}

void AVaelBalanceBoard::Explain()
{
	UVaelQuestSubsystem* Quests = UVaelQuestSubsystem::Get(this);
	AVaelPlayerController* Controller = Cast<AVaelPlayerController>(GetWorld()->GetFirstPlayerController());
	if (Quests == nullptr || Controller == nullptr || Controller->GetPawn() == nullptr)
	{
		ExplainAt = GetWorld()->GetTimeSeconds() + 1.0f;
		return;
	}
	Quests->SetFlag(ExplainedFlag);
	const TArray<FText> Lines = {
		LOCTEXT("Explain1", "Sieh dir die Tafel an. Die Wurzeln haben sie geschrieben, nicht ich. Zwei Waagen: deine und die der Aschenmark."),
		LOCTEXT("Explain2", "Links das Licht, rechts der Schatten, in der Mitte die Dämmerung. Jedes Nid, das du nutzt, legt ein Gewicht auf die rechte Seite. Ein Nid-Zauber, eine Schattenform, ein Schattentrank."),
		LOCTEXT("Explain3", "Was du hier säst, wirkt nach oben. Jedes Lichtbeet nimmt der Aschenmark mit der Zeit etwas Nid, jedes Schattenbeet gibt ihr welches. Du wählst die Form selbst, beim Säen."),
		LOCTEXT("Explain4", "Und wenn du zu schwer wirst: Ein Reinigungstrank, eine Rast mit Klartrunk oder Lichttropfen, oder bring mir einen Nidkristall, dann läutere ich ihn. Nichts davon ist umsonst."),
		LOCTEXT("Explain5", "Das Gelege dort glimmt für dich. Säe dein erstes Beet, dann siehst du es selbst.")
	};
	Controller->StartDialogue(LOCTEXT("Albrun", "Albrun"), Lines);
}

bool AVaelBalanceBoard::CanInteract(const AVaelCharacter* Player) const
{
	return Player != nullptr;
}

void AVaelBalanceBoard::Interact(AVaelCharacter* Player)
{
	if (AVaelPlayerController* Controller = Player != nullptr ? Player->GetController<AVaelPlayerController>() : nullptr)
	{
		Controller->OpenBoard();
	}
}

FText AVaelBalanceBoard::GetInteractPrompt() const
{
	return LOCTEXT("Read", "Wurzeltafel lesen");
}

// ---------------------------------------------------------------- rest spot

AVaelRestSpot::AVaelRestSpot()
{
	PrimaryActorTick.bCanEverTick = false;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
}

void AVaelRestSpot::BeginPlay()
{
	Super::BeginPlay();
	UVaelInteractionSubsystem::Register(this);
}

void AVaelRestSpot::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	UVaelInteractionSubsystem::Unregister(this);
	Super::EndPlay(EndPlayReason);
}

void AVaelRestSpot::Interact(AVaelCharacter* Player)
{
	UVaelBalanceSubsystem::RestGroup(GetWorld(), 8.0f, false);
}

FText AVaelRestSpot::GetInteractPrompt() const
{
	return LOCTEXT("Rest", "Am Glutbecken rasten (8 Stunden)");
}

#undef LOCTEXT_NAMESPACE
