// Copyright Epic Games, Inc. All Rights Reserved.

#include "Debug/VaelStreamTest.h"
#include "Debug/VaelLogCatcher.h"
#include "AbilitySystemComponent.h"
#include "Combat/VaelAttributeSet.h"
#include "Creatures/VaelCreature.h"
#include "Engine/LevelStreaming.h"
#include "Engine/TargetPoint.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "HAL/PlatformMisc.h"
#include "HAL/PlatformTime.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Player/VaelCharacter.h"
#include "Vael.h"
#include "World/VaelGround.h"
#include "World/VaelRegion.h"
#include "WorldPartition/WorldPartitionSubsystem.h"

namespace
{
	/** Tag of the route points; their second tag is their number */
	const FName StreamRouteTag(TEXT("VaelRoute"));

	/** Running speed of the player in cm/s, a little above the real one to put some pressure on streaming */
	constexpr float StreamRunSpeed = 750.0f;

	/** Seconds of game time between two lines of the report */
	constexpr float StreamSampleInterval = 2.0f;

	/** Least and most seconds of warm-up; at least long enough for the spawners to find their ground */
	constexpr float StreamMinWarmUp = 4.0f;
	constexpr float StreamMaxWarmUp = 20.0f;

	/** Seconds the player stands at the end and back at the start */
	constexpr float StreamEndWait = 3.0f;
	constexpr float StreamStartWait = 12.0f;

	/** The ground is looked for this far ahead, in cm */
	constexpr float StreamGroundLookAhead = 300.0f;

	/** Creatures within this distance of the start count as the start group, and near the player in the samples */
	constexpr float StreamStartRadius = 6000.0f;
	constexpr float StreamNearRadius = 5000.0f;

	/** The whole check gives up after this many seconds of game time */
	constexpr float StreamTimeLimit = 900.0f;
}

AVaelStreamTest::AVaelStreamTest()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bTickEvenWhenPaused = true;
}

bool AVaelStreamTest::IsRequested()
{
	return FParse::Param(FCommandLine::Get(), TEXT("VaelStreamTest"));
}

void AVaelStreamTest::BeginPlay()
{
	Super::BeginPlay();

	LogCatcher = new FLogCatcher();
	GLog->AddOutputDevice(LogCatcher);

	AddLoadedRoutePoints();

	Report.Add(FString::Printf(TEXT("Vael stream test on %s, %s, %d route points"), *GetWorld()->GetMapName(),
		GetWorld()->IsPartitionedWorld() ? TEXT("open world") : TEXT("NOT an open world"), Route.Num()));

	LastFrameRealTime = FPlatformTime::Seconds();
}

int32 AVaelStreamTest::AddLoadedRoutePoints()
{
	// Points further on stream in only when the player comes near, so the route grows on the way
	TArray<TPair<int32, FVector>> Points;
	for (TActorIterator<ATargetPoint> It(GetWorld()); It; ++It)
	{
		const int32 Number = It->Tags.Num() >= 2 && It->Tags[0] == StreamRouteTag ? FCString::Atoi(*It->Tags[1].ToString()) : 0;
		if (Number > LastRouteNumber)
		{
			Points.Emplace(Number, It->GetActorLocation());
		}
	}
	Points.Sort([](const TPair<int32, FVector>& A, const TPair<int32, FVector>& B) { return A.Key < B.Key; });

	for (const TPair<int32, FVector>& Point : Points)
	{
		Route.Add(Point.Value);
		LastRouteNumber = Point.Key;
	}
	return Points.Num();
}

void AVaelStreamTest::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (LogCatcher != nullptr)
	{
		GLog->RemoveOutputDevice(LogCatcher);
		delete LogCatcher;
		LogCatcher = nullptr;
	}

	Super::EndPlay(EndPlayReason);
}

AVaelCharacter* AVaelStreamTest::GetPlayer() const
{
	const APlayerController* PlayerController = GetWorld()->GetFirstPlayerController();
	return PlayerController != nullptr ? PlayerController->GetPawn<AVaelCharacter>() : nullptr;
}

void AVaelStreamTest::RefillPlayer() const
{
	AVaelCharacter* Player = GetPlayer();
	UAbilitySystemComponent* AbilitySystem = Player != nullptr ? Player->GetAbilitySystemComponent() : nullptr;
	if (AbilitySystem != nullptr)
	{
		AbilitySystem->SetNumericAttributeBase(UVaelAttributeSet::GetHealthAttribute(), Player->GetMaxHealth());
		AbilitySystem->SetNumericAttributeBase(UVaelAttributeSet::GetCorruptionAttribute(), 0.0f);
	}
}

void AVaelStreamTest::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	// Real time, since game time runs at a fixed step in the check
	const double Now = FPlatformTime::Seconds();
	const double FrameTime = Now - LastFrameRealTime;
	LastFrameRealTime = Now;

	PhaseTime += DeltaSeconds;
	AVaelCharacter* Player = GetPlayer();

	if (GetGameTimeSinceCreation() > StreamTimeLimit)
	{
		Report.Add(FString::Printf(TEXT("FAILED: the check took longer than %.0f s and was stopped"), StreamTimeLimit));
		Finish();
		return;
	}

	if (Player == nullptr)
	{
		return;
	}

	RefillPlayer();

	if (Phase != EPhase::WarmUp && FrameTime > LongestFrame)
	{
		LongestFrame = FrameTime;
		LongestFrameAt = Player->GetActorLocation();
	}

	switch (Phase)
	{
	case EPhase::WarmUp:
	{
		const UWorldPartitionSubsystem* Streaming = GetWorld()->GetSubsystem<UWorldPartitionSubsystem>();
		const bool bStreamed = Streaming == nullptr || const_cast<UWorldPartitionSubsystem*>(Streaming)->IsAllStreamingCompleted();
		if (PhaseTime < StreamMinWarmUp || (!bStreamed && PhaseTime < StreamMaxWarmUp))
		{
			return;
		}

		if (Route.Num() == 0)
		{
			Report.Add(TEXT("FAILED: the level has no route points (target points tagged VaelRoute)"));
			Finish();
			return;
		}

		FHitResult GroundHit;
		const FVector Start = Player->GetActorLocation();
		const bool bOnGround = VaelGround::TraceGround(GetWorld(), Start + FVector(0.0f, 0.0f, 200.0f), Start - FVector(0.0f, 0.0f, 1000.0f), GroundHit, Player);
		Report.Add(FString::Printf(TEXT("Start at %s after %.1f s, %s, ground below %s"), *Start.ToCompactString(), PhaseTime,
			bStreamed ? TEXT("streaming done") : TEXT("streaming NOT done"), bOnGround ? TEXT("found") : TEXT("MISSING")));

		StartCreatures = CountCreaturesNear(Start, StreamStartRadius);
		Report.Add(FString::Printf(TEXT("Creatures near the start: %d"), StartCreatures));

		// The run goes from the start to the route points and back to the start
		Route.Insert(Start, 0);
		TargetIndex = 1;
		RouteStep = 1;

		if (UCharacterMovementComponent* Movement = Player->GetCharacterMovement())
		{
			Movement->SetMovementMode(MOVE_Flying);
		}

		EnterPhase(EPhase::Out);
		Sample(Player);
		return;
	}

	case EPhase::Out:
	case EPhase::Back:
	{
		const bool bMoved = MovePlayer(Player, DeltaSeconds);

		SampleTimeLeft -= DeltaSeconds;
		if (bMoved && SampleTimeLeft <= 0.0f)
		{
			Sample(Player);
		}

		if (Phase == EPhase::Out && TargetIndex >= Route.Num())
		{
			AddLoadedRoutePoints();
		}

		if (TargetIndex < 0 || TargetIndex >= Route.Num())
		{
			Sample(Player);
			EnterPhase(Phase == EPhase::Out ? EPhase::AtEnd : EPhase::AtStart);
		}
		return;
	}

	case EPhase::AtEnd:
		if (PhaseTime >= StreamEndWait)
		{
			Sample(Player);
			TargetIndex = Route.Num() - 2;
			RouteStep = -1;
			EnterPhase(EPhase::Back);
		}
		return;

	case EPhase::AtStart:
		// Long enough for the cell to load and its spawners to find their ground
		if (PhaseTime >= StreamStartWait)
		{
			Sample(Player);
			ReturnCreatures = CountCreaturesNear(Route[0], StreamStartRadius);
			Finish();
		}
		return;

	case EPhase::Done:
		return;
	}
}

bool AVaelStreamTest::MovePlayer(AVaelCharacter* Player, float DeltaSeconds)
{
	const FVector Location = Player->GetActorLocation();
	const FVector Target = Route[TargetIndex];
	FVector ToTarget = Target - Location;
	ToTarget.Z = 0.0f;

	const float Step = StreamRunSpeed * DeltaSeconds;
	const FVector Direction = ToTarget.GetSafeNormal();
	FVector Next = ToTarget.Size() <= Step ? FVector(Target.X, Target.Y, Location.Z) : Location + Direction * Step;

	// The ground a little ahead must be there, else the player waits for it like a falling player would not
	FHitResult GroundHit;
	const FVector Ahead = Next + Direction * StreamGroundLookAhead;
	if (!VaelGround::TraceGround(GetWorld(), Ahead + FVector(0.0f, 0.0f, 3000.0f), Ahead - FVector(0.0f, 0.0f, 5000.0f), GroundHit, Player))
	{
		if (!bWaitingForGround)
		{
			bWaitingForGround = true;
			GroundWaitStart = FPlatformTime::Seconds();
			++GroundWaits;
			Report.Add(FString::Printf(TEXT("  WAIT: no ground ahead at %s"), *Ahead.ToCompactString()));
		}
		return false;
	}

	if (bWaitingForGround)
	{
		bWaitingForGround = false;
		const double Waited = FPlatformTime::Seconds() - GroundWaitStart;
		GroundWaitSeconds += Waited;
		LongestGroundWait = FMath::Max(LongestGroundWait, Waited);
		Report.Add(FString::Printf(TEXT("  ground arrived after %.2f s real time"), Waited));
	}

	// Floats along the ground at standing height
	FHitResult HereHit;
	if (VaelGround::TraceGround(GetWorld(), Next + FVector(0.0f, 0.0f, 3000.0f), Next - FVector(0.0f, 0.0f, 5000.0f), HereHit, Player))
	{
		Next.Z = HereHit.Location.Z + Player->GetSimpleCollisionHalfHeight() + 5.0f;
	}

	Player->SetActorLocation(Next, false, nullptr, ETeleportType::TeleportPhysics);
	if (!Direction.IsNearlyZero())
	{
		Player->SetActorRotation(Direction.Rotation());
	}

	if (FVector::Dist2D(Next, Target) < 1.0f)
	{
		TargetIndex += RouteStep;
	}

	return true;
}

void AVaelStreamTest::Sample(const AVaelCharacter* Player)
{
	SampleTimeLeft = StreamSampleInterval;

	const FVector Location = Player->GetActorLocation();
	const int32 Cells = CountVisibleCells();
	MaxVisibleCells = FMath::Max(MaxVisibleCells, Cells);

	const AVaelRegion* Region = AVaelRegion::GetRegionAt(GetWorld(), Location);
	const FString RegionName = Region != nullptr ? Region->GetRegionName().ToString() : TEXT("none");
	if (RegionName != LastRegionName)
	{
		Report.Add(FString::Printf(TEXT("  region: %s -> %s"), LastRegionName.IsEmpty() ? TEXT("-") : *LastRegionName, *RegionName));
		LastRegionName = RegionName;
	}

	int32 AllCreatures = 0;
	for (TActorIterator<AVaelCreature> It(GetWorld()); It; ++It)
	{
		AllCreatures += !It->IsDead() && !It->IsOnPlayerSide() ? 1 : 0;
	}

	Report.Add(FString::Printf(TEXT("  at x %6.0f y %6.0f m: %2d cells, %3d creatures (%2d near), corruption %3.0f"), Location.X / 100.0f, Location.Y / 100.0f,
		Cells, AllCreatures, CountCreaturesNear(Location, StreamNearRadius), Region != nullptr ? Region->GetCorruption() : -1.0f));
}

int32 AVaelStreamTest::CountCreaturesNear(const FVector& Location, float Radius) const
{
	int32 Count = 0;
	for (TActorIterator<AVaelCreature> It(GetWorld()); It; ++It)
	{
		if (!It->IsDead() && !It->IsOnPlayerSide() && FVector::Dist2D(It->GetActorLocation(), Location) <= Radius)
		{
			++Count;
		}
	}
	return Count;
}

int32 AVaelStreamTest::CountVisibleCells() const
{
	int32 Count = 0;
	for (const ULevelStreaming* StreamingLevel : GetWorld()->GetStreamingLevels())
	{
		Count += StreamingLevel != nullptr && StreamingLevel->IsLevelVisible() ? 1 : 0;
	}
	return Count;
}

void AVaelStreamTest::EnterPhase(EPhase NewPhase)
{
	static const TCHAR* PhaseNames[] = { TEXT("warm-up"), TEXT("out"), TEXT("at the end"), TEXT("back"), TEXT("at the start"), TEXT("done") };

	Phase = NewPhase;
	PhaseTime = 0.0f;
	Report.Add(FString::Printf(TEXT("-- %s"), PhaseNames[static_cast<int32>(NewPhase)]));
}

void AVaelStreamTest::Finish()
{
	EnterPhase(EPhase::Done);

	TArray<FString> Warnings;
	TArray<FString> Errors;
	if (LogCatcher != nullptr)
	{
		LogCatcher->Take(Warnings, Errors);
	}

	Report.Add(TEXT(""));
	Report.Add(FString::Printf(TEXT("Creatures near the start: %d before, %d after coming back%s"), StartCreatures, ReturnCreatures,
		ReturnCreatures > StartCreatures ? TEXT(" - DOUBLES") : ReturnCreatures < StartCreatures ? TEXT(" - some MISSING") : TEXT(" - ok")));
	// Creatures that live on in the persistent level never unload; those of spawners belong to a cell
	TMap<FString, int32> Living;
	for (TActorIterator<AVaelCreature> It(GetWorld()); It; ++It)
	{
		if (!It->IsDead())
		{
			++Living.FindOrAdd(FString::Printf(TEXT("%s %s%s"), *It->GetClass()->GetName(), It->GetLevel() == GetWorld()->PersistentLevel ? TEXT("in the persistent level") : TEXT("in a cell"),
				It->IsOnPlayerSide() ? TEXT(", servant") : TEXT("")));
		}
	}
	for (const TPair<FString, int32>& Entry : Living)
	{
		Report.Add(FString::Printf(TEXT("Living at the end: %dx %s"), Entry.Value, *Entry.Key));
	}

	Report.Add(FString::Printf(TEXT("Waits for ground: %d, %.2f s altogether, longest %.2f s"), GroundWaits, GroundWaitSeconds, LongestGroundWait));
	Report.Add(FString::Printf(TEXT("Most cells at once: %d; longest frame %.0f ms at %s"), MaxVisibleCells, LongestFrame * 1000.0, *LongestFrameAt.ToCompactString()));

	// The same line only once, with how often it came
	auto AddCounted = [this](const TCHAR* Kind, const TArray<FString>& Lines)
	{
		TMap<FString, int32> Counted;
		for (const FString& Line : Lines)
		{
			++Counted.FindOrAdd(Line);
		}
		for (const TPair<FString, int32>& Entry : Counted)
		{
			Report.Add(FString::Printf(TEXT("%s (%dx): %s"), Kind, Entry.Value, *Entry.Key));
		}
	};
	AddCounted(TEXT("ERROR"), Errors);
	AddCounted(TEXT("warning"), Warnings);
	Report.Add(FString::Printf(TEXT("Done: %d errors, %d warnings"), Errors.Num(), Warnings.Num()));

	const FString ReportPath = FPaths::ProjectSavedDir() / TEXT("Logs") / TEXT("VaelStreamTest.txt");
	FFileHelper::SaveStringArrayToFile(Report, *ReportPath, FFileHelper::EEncodingOptions::ForceUTF8);
	UE_LOG(LogVael, Display, TEXT("Stream test finished, report in %s"), *ReportPath);

	SetActorTickEnabled(false);
	FPlatformMisc::RequestExit(false);
}
