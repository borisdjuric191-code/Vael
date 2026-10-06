// Copyright Epic Games, Inc. All Rights Reserved.

#include "World/VaelMarkSource.h"
#include "UObject/ConstructorHelpers.h"
#include "Combat/VaelCombatStatics.h"
#include "Combat/VaelGroundStrike.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Magic/VaelMagicSettings.h"
#include "Magic/VaelSpellEffects.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "NiagaraComponent.h"
#include "NiagaraSystem.h"
#include "Player/VaelCharacter.h"
#include "UI/VaelNoticeSubsystem.h"
#include "Vael.h"
#include "VaelAssets.h"
#include "World/VaelRegion.h"
#include "World/VaelWorldSettings.h"

namespace
{
	/** Radius of the engine cylinder mesh used as placeholder */
	constexpr float MarkSourcePlaceholderRadius = 50.0f;

	/** Color of a sealed source */
	const FLinearColor SealedSourceColor(0.12f, 0.1f, 0.12f);

	/** General look of open sources, made in the editor */
	const TCHAR* MarkSourceEffectPath = TEXT("/Game/Vael/Effects/NS_Vael_MarkSource.NS_Vael_MarkSource");
}

AVaelMarkSource::AVaelMarkSource()
{
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
}

void AVaelMarkSource::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);

	RefreshLook();
}

void AVaelMarkSource::BeginPlay()
{
	Super::BeginPlay();

	RefreshLook();
}

void AVaelMarkSource::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (bSealed)
	{
		return;
	}

	FeedRegion(DeltaSeconds);
	AttackPlayers(DeltaSeconds);
}

void AVaelMarkSource::FeedRegion(float DeltaSeconds)
{
	const UVaelWorldSettings* WorldSettings = UVaelWorldSettings::Get();
	AVaelRegion* Region = AVaelRegion::GetRegionAt(GetWorld(), GetActorLocation());

	if (Region != nullptr && Region->GetCorruption() < WorldSettings->SourceCorruptionCap)
	{
		const float Growth = WorldSettings->SourceCorruptionPerMinute / 60.0f * DeltaSeconds;
		Region->AddCorruption(FMath::Min(Growth, WorldSettings->SourceCorruptionCap - Region->GetCorruption()));
	}
}

void AVaelMarkSource::AttackPlayers(float DeltaSeconds)
{
	const UVaelWorldSettings* WorldSettings = UVaelWorldSettings::Get();

	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		const APlayerController* PlayerController = It->Get();
		AVaelCharacter* Player = PlayerController != nullptr ? PlayerController->GetPawn<AVaelCharacter>() : nullptr;
		if (Player == nullptr)
		{
			continue;
		}

		// Leaving the source, or going down, lets it forget the player
		if (Player->IsDowned() || !IsInside(Player->GetActorLocation()))
		{
			TimeInside.Remove(Player);
			NextAttack.Remove(Player);
			continue;
		}

		float& Time = TimeInside.FindOrAdd(Player);
		Time += DeltaSeconds;

		if (Time < WorldSettings->SourceGraceTime)
		{
			continue;
		}

		// The source reaches out of the ground after the player, with a short warning
		float& Countdown = NextAttack.FindOrAdd(Player);
		Countdown -= DeltaSeconds;

		if (Countdown <= 0.0f)
		{
			Countdown = WorldSettings->SourceStrikeInterval;

			FVaelSpellHit Hit;
			Hit.Damage = WorldSettings->SourceStrikeDamage;
			Hit.Element = EVaelElement::Mark;

			const FVector Feet = Player->GetActorLocation() - FVector(0.0f, 0.0f, Player->GetSimpleCollisionHalfHeight());
			AVaelGroundStrike::SpawnStrike(this, Feet, Hit, WorldSettings->SourceStrikeRadius, WorldSettings->SourceStrikeWarning,
				UVaelMagicSettings::Get()->GetElementColor(EVaelElement::Mark));
		}
	}
}

bool AVaelMarkSource::Seal()
{
	if (bSealed)
	{
		return false;
	}

	bSealed = true;
	TimeInside.Reset();
	NextAttack.Reset();
	RefreshLook();

	if (AVaelRegion* Region = AVaelRegion::GetRegionAt(GetWorld(), GetActorLocation()))
	{
		Region->AddCorruption(-UVaelWorldSettings::Get()->SourceSealCleansing);
	}

	UE_LOG(LogVael, Log, TEXT("Mark source '%s' is sealed"), *GetNameSafe(this));

	UVaelNoticeSubsystem::Post(this, NSLOCTEXT("VaelWorld", "SourceSealed", "Die Mark-Quelle ist versiegelt"),
		NSLOCTEXT("VaelWorld", "SourceSealedDetail", "Die Wunde schließt sich, das Land atmet auf."), FLinearColor(FColor(159, 224, 168)), 4.0f);

	return true;
}

bool AVaelMarkSource::IsInside(const FVector& Location, float ExtraDistance) const
{
	return !bSealed && FVector::DistSquared2D(Location, GetActorLocation()) <= FMath::Square(Radius + ExtraDistance);
}

AVaelMarkSource* AVaelMarkSource::FindOpenSourceAt(const UWorld* World, const FVector& Location, float ExtraDistance)
{
	if (World == nullptr)
	{
		return nullptr;
	}

	for (TActorIterator<AVaelMarkSource> It(World); It; ++It)
	{
		if (It->IsInside(Location, ExtraDistance))
		{
			return *It;
		}
	}

	return nullptr;
}

void AVaelMarkSource::RefreshLook()
{
	Disc->SetWorldScale3D(FVector(Radius / MarkSourcePlaceholderRadius, Radius / MarkSourcePlaceholderRadius, 0.02f));

	const FLinearColor MarkColor = UVaelMagicSettings::Get()->GetElementColor(EVaelElement::Mark);

	UMaterialInstanceDynamic* Material = Cast<UMaterialInstanceDynamic>(Disc->GetMaterial(0));
	if (Material == nullptr)
	{
		Material = Disc->CreateAndSetMaterialInstanceDynamic(0);
	}

	if (Material != nullptr)
	{
		Material->SetVectorParameterValue(TEXT("Color"), bSealed ? SealedSourceColor : MarkColor);
	}

	if (!HasActorBegunPlay())
	{
		return;
	}

	// The real look of an open source replaces the placeholder; a sealed source shows the grey scar
	if (bSealed)
	{
		if (OpenEffectComponent != nullptr)
		{
			OpenEffectComponent->Deactivate();
		}

		Disc->SetVisibility(true);
		return;
	}

	if (OpenEffectComponent == nullptr)
	{
		UNiagaraSystem* Effect = OpenEffect != nullptr ? OpenEffect.Get() : Cast<UNiagaraSystem>(VaelAssets::LoadOptional(FSoftObjectPath(MarkSourceEffectPath)));
		OpenEffectComponent = VaelEffects::Attach(Effect, Disc, NAME_None, MarkColor, Radius, false);
	}

	Disc->SetVisibility(OpenEffectComponent == nullptr);
}
