// Copyright Epic Games, Inc. All Rights Reserved.

#include "Story/VaelNpc.h"
#include "World/VaelBalance.h"
#include "UObject/ConstructorHelpers.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/GameInstance.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Player/VaelCharacter.h"
#include "Player/VaelPlayerController.h"
#include "Compendium/VaelCompendiumSubsystem.h"
#include "Story/VaelDialogue.h"
#include "Story/VaelGossip.h"
#include "Story/VaelQuestSubsystem.h"
#include "World/VaelGround.h"
#include "World/VaelProgressSubsystem.h"
#include "World/VaelTimeOfDay.h"

namespace
{
	/** Size of the person in cm */
	constexpr float NpcRadius = 40.0f;
	constexpr float NpcHalfHeight = 90.0f;

	/** Color of the placeholder skin */
	const FLinearColor NpcSkinColor(0.62f, 0.48f, 0.38f);
}

AVaelNpc::AVaelNpc()
{
	Capsule = CreateDefaultSubobject<UCapsuleComponent>(TEXT("Capsule"));
	Capsule->InitCapsuleSize(NpcRadius, NpcHalfHeight);
	Capsule->SetCollisionProfileName(TEXT("Pawn"));
	RootComponent = Capsule;

	static ConstructorHelpers::FObjectFinder<UStaticMesh> ConeMesh(TEXT("/Engine/BasicShapes/Cone.Cone"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereMesh(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> ShapeMaterial(TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));

	// A robe as a cone, a head as a sphere on top
	Body = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Body"));
	Body->SetupAttachment(RootComponent);
	Body->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Body->SetRelativeScale3D(FVector(0.8f, 0.8f, 1.5f));
	Body->SetRelativeLocation(FVector(0.0f, 0.0f, -15.0f));

	Head = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Head"));
	Head->SetupAttachment(RootComponent);
	Head->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Head->SetRelativeScale3D(FVector(0.32f));
	Head->SetRelativeLocation(FVector(0.0f, 0.0f, 70.0f));

	if (ConeMesh.Succeeded())
	{
		Body->SetStaticMesh(ConeMesh.Object);
	}
	if (SphereMesh.Succeeded())
	{
		Head->SetStaticMesh(SphereMesh.Object);
	}
	if (ShapeMaterial.Succeeded())
	{
		Body->SetMaterial(0, ShapeMaterial.Object);
		Head->SetMaterial(0, ShapeMaterial.Object);
	}

	// Only the gossip needs an update, and not often
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickInterval = 0.5f;
}

void AVaelNpc::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	const float Now = GetWorld()->GetTimeSeconds();
	if (Places.Num() > 0 && Now >= NextDayCheckTime)
	{
		NextDayCheckTime = Now + FMath::FRandRange(2.0f, 4.0f);
		FollowDay(false);
	}
	if (bAsleep || Gossip == nullptr || Now < NextGossipTime)
	{
		return;
	}

	// Somebody has to be near enough to hear it
	bool bHeard = false;
	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		const AVaelCharacter* Player = It->Get() != nullptr ? It->Get()->GetPawn<AVaelCharacter>() : nullptr;
		bHeard |= Player != nullptr && !Player->IsDowned() && GetDistanceTo(Player) < Gossip->Range;
	}
	if (!bHeard)
	{
		return;
	}

	const int32 Picked = Gossip->PickLine(this, GossipIndex, &Identity);
	const FVaelGossipLine* Line = Gossip->GetLine(Picked);
	if (Line != nullptr)
	{
		GossipIndex = Picked;
		GossipEndTime = Now + Gossip->ShowSeconds;

		// {Name}, {Ort}, {Nachbar}: the person talks about themself, their place and the people they live with
		FFormatNamedArguments Arguments;
		Arguments.Add(TEXT("Name"), GetDisplayName());
		Arguments.Add(TEXT("Ort"), Identity.SettlementName.IsEmpty() ? NSLOCTEXT("VaelStory", "GossipHere", "hier") : Identity.SettlementName);
		Arguments.Add(TEXT("Nachbar"), PickNeighbourName());
		GossipText = FText::Format(Line->Line, Arguments);
	}
	NextGossipTime = Now + Gossip->ShowSeconds + FMath::FRandRange(Gossip->MinPause, FMath::Max(Gossip->MinPause, Gossip->MaxPause));
}

FText AVaelNpc::GetGossipLine() const
{
	return Gossip != nullptr && GossipIndex != INDEX_NONE && GetWorld()->GetTimeSeconds() < GossipEndTime ? GossipText : FText::GetEmpty();
}

FText AVaelNpc::PickNeighbourName() const
{
	TArray<FText> Names;
	if (!Identity.Settlement.IsNone())
	{
		for (TActorIterator<AVaelNpc> It(GetWorld()); It; ++It)
		{
			if (*It != this && It->Identity.Settlement == Identity.Settlement && !It->GetDisplayName().IsEmpty())
			{
				Names.Add(It->GetDisplayName());
			}
		}
	}
	return Names.IsEmpty() ? NSLOCTEXT("VaelStory", "GossipSomeone", "jemand") : Names[FMath::RandRange(0, Names.Num() - 1)];
}

void AVaelNpc::SetIdentity(const FVaelNpcIdentity& NewIdentity)
{
	Identity = NewIdentity;
	ApplyBodyShape();
}

void AVaelNpc::ApplyBodyShape()
{
	// Whole person: children small, the young a bit short, old people a little bent
	float Height = 1.0f;
	switch (Identity.Age)
	{
	case EVaelNpcAge::Child: Height = 0.62f; break;
	case EVaelNpcAge::Young: Height = 0.94f; break;
	case EVaelNpcAge::Old: Height = 0.92f; break;
	default: break;
	}
	SetActorScale3D(FVector(Height));

	// Width of the robe after the build
	const float Width = Identity.Build == EVaelNpcBuild::Slim ? 0.68f : Identity.Build == EVaelNpcBuild::Strong ? 0.98f : 0.8f;
	Body->SetRelativeScale3D(FVector(Width, Width, 1.5f));
	Head->SetRelativeLocation(Identity.Age == EVaelNpcAge::Old ? FVector(8.0f, 0.0f, 64.0f) : FVector(0.0f, 0.0f, 70.0f));
}

void AVaelNpc::BeginPlay()
{
	Super::BeginPlay();

	VaelGround::RefuseDecals(this);
	ApplyBodyShape();

	if (UMaterialInstanceDynamic* RobeMaterial = Body->CreateAndSetMaterialInstanceDynamic(0))
	{
		RobeMaterial->SetVectorParameterValue(TEXT("Color"), RobeColor);
	}
	if (UMaterialInstanceDynamic* SkinMaterial = Head->CreateAndSetMaterialInstanceDynamic(0))
	{
		SkinMaterial->SetVectorParameterValue(TEXT("Color"), NpcSkinColor);
	}

	UVaelInteractionSubsystem::Register(this);

	// Streamed in out of sight: straight to the place of the hour
	FollowDay(true);
}

const FVaelNpcPlace* AVaelNpc::GetWantedPlace(float Hour) const
{
	auto Find = [this](EVaelNpcPlaceKind Kind) -> const FVaelNpcPlace*
	{
		return Places.FindByPredicate([Kind](const FVaelNpcPlace& Place) { return Place.Kind == Kind; });
	};
	if (Places.IsEmpty())
	{
		return nullptr;
	}

	// Everybody keeps their own hours, up to 45 minutes earlier or later; children go to bed early, old people a little early
	const uint32 Seed = GetTypeHash(GetDisplayName().ToString());
	const float Own = (static_cast<float>(Seed % 91) - 45.0f) / 60.0f;
	const float Bed = 22.5f + Own - (Identity.Age == EVaelNpcAge::Child ? 1.5f : Identity.Age == EVaelNpcAge::Old ? 0.5f : 0.0f);
	const float Rise = 6.5f + Own;
	const float Evening = 19.0f + Own * 0.5f;
	const bool bNight = Hour >= Bed || Hour < Rise;

	// Carters go from stop to stop through the day and sleep where the night finds them, or at their last stop
	TArray<const FVaelNpcPlace*> Stops;
	for (const FVaelNpcPlace& Place : Places)
	{
		if (Place.Kind == EVaelNpcPlaceKind::Stop)
		{
			Stops.Add(&Place);
		}
	}
	if (Stops.Num() > 0)
	{
		const FVaelNpcPlace* Sleep = Find(EVaelNpcPlaceKind::Sleep);
		if (bNight)
		{
			return Sleep != nullptr ? Sleep : Stops.Last();
		}
		const float Day = FMath::Clamp((Hour - Rise) / FMath::Max(Bed - Rise, 1.0f), 0.0f, 0.999f);
		return Stops[FMath::FloorToInt32(Day * Stops.Num())];
	}

	const FVaelNpcPlace* Work = Find(EVaelNpcPlaceKind::Work);
	const FVaelNpcPlace* Fire = Find(EVaelNpcPlaceKind::Fire);
	const FVaelNpcPlace* Sleep = Find(EVaelNpcPlaceKind::Sleep);
	const FVaelNpcPlace* Wanted = bNight ? Sleep : Hour >= Evening ? Fire : Work;
	if (Wanted == nullptr && bNight)
	{
		Wanted = Fire;
	}
	return Wanted != nullptr ? Wanted : Work != nullptr ? Work : &Places[0];
}

void AVaelNpc::FollowDayNow()
{
	FollowDay(true);
}

void AVaelNpc::FollowDay(bool bForce)
{
	const UVaelTimeOfDay* Clock = UVaelTimeOfDay::Get(this);
	const FVaelNpcPlace* Wanted = GetWantedPlace(Clock != nullptr ? Clock->GetHour() : 12.0f);
	if (Wanted == nullptr)
	{
		return;
	}
	const int32 WantedIndex = static_cast<int32>(Wanted - Places.GetData());
	if (WantedIndex == CurrentPlace)
	{
		return;
	}

	// Nobody may watch them go or arrive: like in a real camp, people are simply elsewhere when you look again
	if (!bForce)
	{
		for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
		{
			const APawn* Player = It->Get() != nullptr ? It->Get()->GetPawn() : nullptr;
			if (Player != nullptr && (FVector::Dist2D(Player->GetActorLocation(), GetActorLocation()) < UnseenDistance
				|| FVector::Dist2D(Player->GetActorLocation(), Wanted->Location) < UnseenDistance))
			{
				return;
			}
		}
	}
	CurrentPlace = WantedIndex;
	MoveTo(*Wanted);
}

void AVaelNpc::MoveTo(const FVaelNpcPlace& Place)
{
	SetActorLocationAndRotation(Place.Location, FRotator(0.0f, Place.Yaw, 0.0f), false, nullptr, ETeleportType::TeleportPhysics);
	bAsleep = Place.Kind == EVaelNpcPlaceKind::Sleep;
	SetActorHiddenInGame(bAsleep);
	SetActorEnableCollision(!bAsleep);
	if (bAsleep)
	{
		GossipIndex = INDEX_NONE;
	}
}

void AVaelNpc::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	UVaelInteractionSubsystem::Unregister(this);

	Super::EndPlay(EndPlayReason);
}

void AVaelNpc::Interact(AVaelCharacter* Player)
{
	AVaelPlayerController* PlayerController = Player != nullptr ? Player->GetController<AVaelPlayerController>() : nullptr;
	UVaelProgressSubsystem* Progress = GetGameInstance() != nullptr ? GetGameInstance()->GetSubsystem<UVaelProgressSubsystem>() : nullptr;
	if (PlayerController == nullptr || Dialogue == nullptr || Progress == nullptr)
	{
		return;
	}

	// A quest waiting for this talk speaks first
	TArray<FText> QuestLines;
	UVaelQuestSubsystem* Quests = UVaelQuestSubsystem::Get(this);
	const UVaelQuest* ChoiceQuest = nullptr;
	const bool bQuestTalk = Quests != nullptr && Quests->TakeTalk(GetSpeakerKey(), QuestLines, ChoiceQuest);

	// The first talk is the intro, later ones give the hints that fit now
	TArray<FText> Lines;
	if (!Progress->HasHeardIntro(GetSpeakerKey()) && !Dialogue->IntroLines.IsEmpty())
	{
		Lines = Dialogue->IntroLines;
		Progress->MarkIntroHeard(GetSpeakerKey());
	}
	else if (!bQuestTalk || QuestLines.IsEmpty())
	{
		Lines = Dialogue->GatherHints(GetWorld());
	}

	Lines.Append(QuestLines);

	// A researcher looks at the samples the group brought along
	UVaelCompendiumSubsystem* Compendium = Dialogue->bStudiesSamples ? UVaelCompendiumSubsystem::Get(this) : nullptr;
	TArray<FText> Studied;
	if (Compendium != nullptr && Compendium->StudyAtCamp(&Studied) > 0 && !Dialogue->StudyDoneLine.IsEmpty())
	{
		Lines.Add(FText::Format(Dialogue->StudyDoneLine, FText::Join(FText::FromString(TEXT(", ")), Studied)));
	}

	// Albrun purifies a Nidkristall the player carries (Die Waage): it becomes a Geläuterter Kristall, the player sinks by 5
	if (GetSpeakerKey() == FName(TEXT("DA_Dialogue_Albrun")) && UVaelBalanceSubsystem::PurifyCrystal(Player))
	{
		Lines.Add(NSLOCTEXT("VaelStory", "AlbrunPurifies", "Gib mir den Kristall. … So. Das Nid darin schläft wieder, und ein Stück davon ist aus dir gewichen. Behalte den geläuterten, er ist mehr wert, als er aussieht."));
	}

	if (Lines.IsEmpty())
	{
		Lines.Add(NSLOCTEXT("VaelStory", "NothingToSay", "…"));
	}

	PlayerController->StartDialogue(GetDisplayName(), Lines);

	// A talk that asks the players to decide ends in a list of answers
	PlayerController->OfferChoiceAfterDialogue(ChoiceQuest);

	// A trader shows their goods once the talk is read
	if (Shop != nullptr)
	{
		PlayerController->OpenShopAfterDialogue(Shop);
	}
}

FText AVaelNpc::GetInteractPrompt() const
{
	return NSLOCTEXT("VaelStory", "Talk", "Sprechen");
}

bool AVaelNpc::HasNews() const
{
	const UVaelProgressSubsystem* Progress = GetGameInstance() != nullptr ? GetGameInstance()->GetSubsystem<UVaelProgressSubsystem>() : nullptr;
	const UVaelQuestSubsystem* Quests = UVaelQuestSubsystem::Get(this);
	const UVaelCompendiumSubsystem* Compendium = UVaelCompendiumSubsystem::Get(this);

	return Dialogue != nullptr && ((!Dialogue->IntroLines.IsEmpty() && Progress != nullptr && !Progress->HasHeardIntro(GetSpeakerKey()))
		|| (Quests != nullptr && Quests->IsWaitingForTalk(GetSpeakerKey()))
		|| (Dialogue->bStudiesSamples && Compendium != nullptr && Compendium->CanStudy()));
}

FText AVaelNpc::GetDisplayName() const
{
	if (Dialogue != nullptr)
	{
		return Dialogue->SpeakerName;
	}
	return !Identity.Name.IsEmpty() ? Identity.Name : Gossip != nullptr ? Gossip->SpeakerName : FText::GetEmpty();
}

FName AVaelNpc::GetSpeakerKey() const
{
	return Dialogue != nullptr ? Dialogue->GetFName() : GetFName();
}
