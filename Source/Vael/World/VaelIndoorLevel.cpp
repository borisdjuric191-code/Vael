// Copyright Epic Games, Inc. All Rights Reserved.

#include "World/VaelIndoorLevel.h"

#include "Components/BillboardComponent.h"
#include "Creatures/VaelTropfseuche.h"
#include "Creatures/VaelKobold.h"
#include "EngineUtils.h"
#include "World/VaelRegion.h"
#include "GameFramework/Pawn.h"
#include "Camera/PlayerCameraManager.h"
#include "Kismet/GameplayStatics.h"
#include "Creatures/VaelCreature.h"
#include "Engine/TargetPoint.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "UI/VaelNoticeSubsystem.h"

#define LOCTEXT_NAMESPACE "VaelIndoorLevel"

namespace
{
	/** A line for each cave of the Eisenschlund, shown when the group walks into it (the common thread: Eisen, Gilde, Seuche) */
	FText CaveLine(const FString& Key)
	{
		if (Key == TEXT("L_Gildenmine"))		return LOCTEXT("LineGilde", "Die Stollen der Gilde: Gleise, Loren und warmes Grubenlicht.");
		if (Key == TEXT("L_FunkenerzAdern"))	return LOCTEXT("LineFunken", "Glühende Adern im schwarzen Fels. Hier brennt das Funkenerz.");
		if (Key == TEXT("L_Olmgrotte"))			return LOCTEXT("LineOlm", "Kaltes Wasser und das blaue Licht des Salzspats.");
		if (Key == TEXT("L_Gebeinkammer"))		return LOCTEXT("LineGebein", "Die Katakomben der Gilde. Zwischen den Knochen leuchtet der Schleim.");
		if (Key == TEXT("L_Ameisenbau"))		return LOCTEXT("LineAmeisen", "Gefressene Gänge, Kristallreste, die Brut der Schürfameisen.");
		if (Key == TEXT("L_Rostschwamm"))		return LOCTEXT("LineRost", "Die Luft schmeckt nach Rost und Gift.");
		if (Key == TEXT("L_TiefsterSchacht"))	return LOCTEXT("LineSchacht", "Unter euch nur Tiefe. Darüber spannen sich die Rippen der Urdrachin.");
		if (Key == TEXT("L_KoboldStollen"))		return LOCTEXT("LineKobold", "Ihr habt den Kobold-Stollen gefunden. Falsches Licht, und irgendwo der Hort.");
		return FText::GetEmpty();
	}
}

AVaelIndoorLevel::AVaelIndoorLevel()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickInterval = 0.1f;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));

#if WITH_EDITORONLY_DATA
	// Something to click in the editor; not visible in the game
	if (UBillboardComponent* Sprite = CreateEditorOnlyDefaultSubobject<UBillboardComponent>(TEXT("Sprite")))
	{
		Sprite->SetupAttachment(RootComponent);
		Sprite->bIsScreenSizeScaled = true;
	}
#endif
}

bool AVaelIndoorLevel::IsIndoor(const UWorld* World)
{
	return Find(World) != nullptr;
}

bool AVaelIndoorLevel::IsIndoorAt(const UWorld* World, const FVector& Location)
{
	return FindAt(World, Location) != nullptr;
}

const AVaelIndoorLevel* AVaelIndoorLevel::Find(const UWorld* World)
{
	if (World == nullptr)
	{
		return nullptr;
	}
	for (TActorIterator<AVaelIndoorLevel> It(const_cast<UWorld*>(World)); It; ++It)
	{
		if (It->bWholeLevel)
		{
			return *It;
		}
	}
	return nullptr;
}

const AVaelIndoorLevel* AVaelIndoorLevel::FindAt(const UWorld* World, const FVector& Location)
{
	if (World == nullptr)
	{
		return nullptr;
	}
	for (TActorIterator<AVaelIndoorLevel> It(const_cast<UWorld*>(World)); It; ++It)
	{
		if (It->Covers(Location))
		{
			return *It;
		}
	}
	return nullptr;
}

bool AVaelIndoorLevel::HasIndoorPlaces(const UWorld* World)
{
	return World != nullptr && TActorIterator<AVaelIndoorLevel>(const_cast<UWorld*>(World));
}

bool AVaelIndoorLevel::Covers(const FVector& Location) const
{
	if (bWholeLevel)
	{
		return true;
	}
	const FVector Local = Location - GetActorLocation();
	return FMath::Abs(Local.X) <= Extent.X && FMath::Abs(Local.Y) <= Extent.Y && FMath::Abs(Local.Z) <= Extent.Z;
}

FString AVaelIndoorLevel::PlaceKeyAt(const UWorld* World, const FVector& Location)
{
	if (World == nullptr)
	{
		return FString();
	}
	for (TActorIterator<AVaelRegion> It(const_cast<UWorld*>(World)); It; ++It)
	{
		if (It->Tags.Num() > 0 && It->Tags[0].ToString().StartsWith(TEXT("L_")) && It->Contains(Location))
		{
			return It->Tags[0].ToString();
		}
	}
	return UWorld::RemovePIEPrefix(World->GetMapName());
}

void AVaelIndoorLevel::FadeCeilings()
{
	if (Ceilings.IsEmpty())
	{
		return;
	}
	const APlayerCameraManager* Camera = UGameplayStatics::GetPlayerCameraManager(this, 0);
	if (Camera == nullptr)
	{
		return;
	}
	const FVector Eye = Camera->GetCameraLocation();
	TSet<TObjectPtr<AActor>> Covering;
	for (int32 Index = 0; Index < 4; ++Index)
	{
		const APawn* Pawn = UGameplayStatics::GetPlayerPawn(this, Index);
		if (Pawn == nullptr)
		{
			continue;
		}
		// A ceiling covers the player when it lies near the line from the player's chest up to the camera, below the camera
		const FVector Chest = Pawn->GetActorLocation() + FVector(0.0f, 0.0f, 60.0f);
		for (AActor* Ceiling : Ceilings)
		{
			if (Ceiling == nullptr)
			{
				continue;
			}
			FVector Origin, Size;
			Ceiling->GetActorBounds(true, Origin, Size);
			const float Height = Origin.Z + Size.Z;
			if (Height < Chest.Z || FVector::DistSquared2D(Origin, Chest) > FMath::Square(1200.0f))
			{
				continue;
			}
			// Where the line of sight passes the ceiling's middle height
			const float T = FMath::Clamp((Origin.Z - Chest.Z) / FMath::Max(Eye.Z - Chest.Z, 1.0f), 0.0f, 1.0f);
			const FVector Pass = Chest + (Eye - Chest) * T;
			if (FMath::Abs(Pass.X - Origin.X) <= Size.X + 60.0f && FMath::Abs(Pass.Y - Origin.Y) <= Size.Y + 60.0f)
			{
				Covering.Add(Ceiling);
			}
		}
	}
	for (AActor* Ceiling : Covering)
	{
		if (!HiddenCeilings.Contains(Ceiling))
		{
			Ceiling->SetActorHiddenInGame(true);
		}
	}
	for (AActor* Ceiling : HiddenCeilings)
	{
		if (Ceiling != nullptr && !Covering.Contains(Ceiling))
		{
			Ceiling->SetActorHiddenInGame(false);
		}
	}
	HiddenCeilings = MoveTemp(Covering);
}

void AVaelIndoorLevel::RefreshCeilings()
{
	// In a streamed world the ceilings load and unload with the ground: look again now and then while someone is inside
	Ceilings.Reset();
	for (TActorIterator<AActor> It(GetWorld()); It; ++It)
	{
		if (It->ActorHasTag(TEXT("VaelDecke")) && Covers(It->GetActorLocation()))
		{
			Ceilings.Add(*It);
		}
	}
}

void AVaelIndoorLevel::TickDwellers()
{
	UWorld* World = GetWorld();
	TArray<FVector> Inside;
	for (int32 Index = 0; Index < 4; ++Index)
	{
		if (const APawn* Pawn = UGameplayStatics::GetPlayerPawn(this, Index); Pawn != nullptr && Covers(Pawn->GetActorLocation()))
		{
			Inside.Add(Pawn->GetActorLocation());
		}
	}
	const auto Near = [&Inside](const FVector& Location, float Radius)
	{
		return Inside.ContainsByPredicate([&](const FVector& At) { return FVector::DistSquared2D(At, Location) <= Radius * Radius; });
	};
	// Places of dwellers near a player whose floor has loaded: each place once per visit of the level
	constexpr float ComeRadius = 6000.0f;
	TArray<TWeakObjectPtr<AVaelCreature>> Born;
	if (!Inside.IsEmpty())
	{
		const auto Take = [this, World, &Near](const ATargetPoint* Point)
		{
			if (UsedPlaces.Contains(Point->GetFName()) || !Covers(Point->GetActorLocation()) || !Near(Point->GetActorLocation(), ComeRadius))
			{
				return false;
			}
			FVector Ground;
			if (!AVaelCreature::FindGround(World, Point->GetActorLocation() + FVector(0.0f, 0.0f, 200.0f), Ground)
				|| FMath::Abs(Ground.Z - Point->GetActorLocation().Z) > 150.0f)
			{
				return false;
			}
			UsedPlaces.Add(Point->GetFName());
			return true;
		};
		UVaelTropfseuche::SpawnHosts(World, Take, &Born);
		UVaelKobolde::SpawnInCaves(World, Take, &Born);
		Dwellers.Append(Born);
	}
	// Far from every player: held still and hidden until someone comes near again
	constexpr float HoldRadius = 15000.0f;
	Dwellers.RemoveAll([](const TWeakObjectPtr<AVaelCreature>& Each) { return !Each.IsValid(); });
	for (const TWeakObjectPtr<AVaelCreature>& Each : Dwellers)
	{
		AVaelCreature* Creature = Each.Get();
		const bool bHold = !Near(Creature->GetActorLocation(), HoldRadius);
		if (bHold == Frozen.Contains(Each))
		{
			continue;
		}
		UCharacterMovementComponent* Movement = Creature->FindComponentByClass<UCharacterMovementComponent>();
		if (bHold)
		{
			Frozen.Add(Each);
			Creature->SetActorTickEnabled(false);
			Creature->SetActorHiddenInGame(true);
			if (Movement != nullptr)
			{
				Movement->DisableMovement();
				Movement->SetComponentTickEnabled(false);
			}
		}
		else
		{
			Frozen.Remove(Each);
			Creature->SetActorTickEnabled(true);
			Creature->SetActorHiddenInGame(false);
			if (Movement != nullptr)
			{
				Movement->SetComponentTickEnabled(true);
				Movement->SetMovementMode(MOVE_Walking);
			}
		}
	}
}

void AVaelIndoorLevel::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!bWholeLevel)
	{
		DwellerWait -= DeltaSeconds;
		if (DwellerWait <= 0.0f)
		{
			DwellerWait = 1.0f;
			TickDwellers();
		}
		const APawn* Pawn = UGameplayStatics::GetPlayerPawn(this, 0);
		if (Pawn == nullptr || !Covers(Pawn->GetActorLocation()))
		{
			// Outside the caves nothing more to do (no ceilings to fade, no cave names to show)
			bRegionKnown = false;
			return;
		}
		CeilingWait -= DeltaSeconds;
		if (CeilingWait <= 0.0f)
		{
			CeilingWait = 2.0f;
			RefreshCeilings();
		}
	}
	FadeCeilings();
	NoticeWait -= DeltaSeconds;
	if (NoticeWait > 0.0f)
	{
		return;
	}
	NoticeWait = 0.5f;
	const APawn* Pawn = UGameplayStatics::GetPlayerPawn(this, 0);
	if (Pawn == nullptr)
	{
		return;
	}
	const AVaelRegion* Region = AVaelRegion::GetRegionAt(GetWorld(), Pawn->GetActorLocation());
	const AVaelRegion* Cave = Region != nullptr && Region->Tags.Num() > 0 && Region->Tags[0].ToString().StartsWith(TEXT("L_")) ? Region : nullptr;
	if (bRegionKnown && Cave == ShownRegion.Get())
	{
		return;
	}
	// The first look only remembers where the group starts: the travel itself already says where it is (in the open world the
	// way down says so too)
	const bool bShow = bRegionKnown && Cave != nullptr;
	ShownRegion = Cave;
	bRegionKnown = true;
	if (bShow)
	{
		const FString Key = Cave->Tags[0].ToString();
		const bool bSecret = Key == TEXT("L_KoboldStollen");
		UVaelNoticeSubsystem::Post(this, Cave->GetRegionName(), CaveLine(Key),
			bSecret ? FLinearColor(1.0f, 0.55f, 0.8f) : FLinearColor(FColor(232, 214, 186)), bSecret ? 5.0f : 3.5f);
	}
}

void AVaelIndoorLevel::BeginPlay()
{
	Super::BeginPlay();
	if (!bWholeLevel)
	{
		// In the open world the caves stream in when someone comes: ceilings and dwellers come then (Tick)
		return;
	}
	RefreshCeilings();
	UVaelTropfseuche::SpawnHosts(GetWorld());
	UVaelKobolde::SpawnInCaves(GetWorld());
}

#undef LOCTEXT_NAMESPACE
