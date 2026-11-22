// Copyright Epic Games, Inc. All Rights Reserved.

#include "World/VaelIndoorLevel.h"

#include "Components/BillboardComponent.h"
#include "Creatures/VaelTropfseuche.h"
#include "Creatures/VaelKobold.h"
#include "EngineUtils.h"
#include "World/VaelRegion.h"
#include "GameFramework/Pawn.h"
#include "Kismet/GameplayStatics.h"
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
	PrimaryActorTick.TickInterval = 0.5f;
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
	return World != nullptr && TActorIterator<AVaelIndoorLevel>(const_cast<UWorld*>(World));
}

const AVaelIndoorLevel* AVaelIndoorLevel::Find(const UWorld* World)
{
	if (World == nullptr)
	{
		return nullptr;
	}
	TActorIterator<AVaelIndoorLevel> It(const_cast<UWorld*>(World));
	return It ? *It : nullptr;
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

void AVaelIndoorLevel::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
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
	// The first look only remembers where the group starts: the travel itself already says where it is
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
	UVaelTropfseuche::SpawnHosts(GetWorld());
	UVaelKobolde::SpawnInCaves(GetWorld());
}

#undef LOCTEXT_NAMESPACE
