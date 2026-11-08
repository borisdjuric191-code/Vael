// Copyright Epic Games, Inc. All Rights Reserved.

#include "World/VaelBalance.h"
#include "World/VaelIndoorLevel.h"
#include "TimerManager.h"
#include "Camera/PlayerCameraManager.h"
#include "GameFramework/PlayerController.h"
#include "Combat/VaelCombatStatics.h"
#include "Items/VaelMaterialBag.h"
#include "Items/VaelMaterial.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Components/SkeletalMeshComponent.h"
#include "Player/VaelCharacter.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemInterface.h"
#include "Combat/VaelAttributeSet.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Nature/VaelGarden.h"
#include "Save/VaelSaveGame.h"
#include "Save/VaelSaveSubsystem.h"
#include "UI/VaelCombatTextSubsystem.h"
#include "Vael.h"
#include "World/VaelRegion.h"
#include "World/VaelTimeOfDay.h"

#define LOCTEXT_NAMESPACE "VaelBalance"

const TCHAR* UVaelBalanceSubsystem::RefugiumLevel = TEXT("L_Glutkammer");
const TCHAR* UVaelBalanceSubsystem::RefugiumRegion = TEXT("Aschenmark");

namespace
{
	/** What one bed of the refugium does to its region per in-game hour: light beds cleanse, shadow beds feed the Nid */
	constexpr float BedDriftPerHour = 0.08f;
}

UVaelBalanceSubsystem* UVaelBalanceSubsystem::Get(const UObject* WorldContext)
{
	const UWorld* World = WorldContext != nullptr ? WorldContext->GetWorld() : nullptr;
	const UGameInstance* GameInstance = World != nullptr ? World->GetGameInstance() : nullptr;
	return GameInstance != nullptr ? GameInstance->GetSubsystem<UVaelBalanceSubsystem>() : nullptr;
}

EVaelBalanceZone UVaelBalanceSubsystem::GetZone(float Corruption)
{
	return Corruption < LightBelow ? EVaelBalanceZone::Light : Corruption > ShadowAbove ? EVaelBalanceZone::Shadow : EVaelBalanceZone::Twilight;
}

FText UVaelBalanceSubsystem::GetZoneName(EVaelBalanceZone Zone)
{
	switch (Zone)
	{
	case EVaelBalanceZone::Light: return LOCTEXT("ZoneLight", "Licht");
	case EVaelBalanceZone::Shadow: return LOCTEXT("ZoneShadow", "Schatten");
	default: return LOCTEXT("ZoneTwilight", "Zwielicht");
	}
}

FLinearColor UVaelBalanceSubsystem::GetZoneColor(EVaelBalanceZone Zone)
{
	switch (Zone)
	{
	case EVaelBalanceZone::Light: return FLinearColor(FColor(240, 200, 110));
	case EVaelBalanceZone::Shadow: return FLinearColor(FColor(162, 77, 255));
	default: return FLinearColor(FColor(214, 120, 150));
	}
}

float UVaelBalanceSubsystem::GetPlayer(const AActor* Player)
{
	const IAbilitySystemInterface* Owner = Cast<IAbilitySystemInterface>(Player);
	const UAbilitySystemComponent* AbilitySystem = Owner != nullptr ? Owner->GetAbilitySystemComponent() : nullptr;
	return AbilitySystem != nullptr ? AbilitySystem->GetNumericAttribute(UVaelAttributeSet::GetCorruptionAttribute()) : 0.0f;
}

float UVaelBalanceSubsystem::ChangePlayer(AActor* Player, float Delta, const FText& Reason)
{
	const IAbilitySystemInterface* Owner = Cast<IAbilitySystemInterface>(Player);
	UAbilitySystemComponent* AbilitySystem = Owner != nullptr ? Owner->GetAbilitySystemComponent() : nullptr;
	if (AbilitySystem == nullptr || FMath::IsNearlyZero(Delta))
	{
		return GetPlayer(Player);
	}

	const float Before = AbilitySystem->GetNumericAttribute(UVaelAttributeSet::GetCorruptionAttribute());
	const float After = FMath::Clamp(Before + Delta, 0.0f, 100.0f);
	AbilitySystem->SetNumericAttributeBase(UVaelAttributeSet::GetCorruptionAttribute(), After);

	// Shows: violet when it rises, gold when it falls, with the reason; small steps of a long effect only every few
	if (UVaelBalanceSubsystem* Balance = Get(Player))
	{
		FChange& Change = Balance->LastChanges.FindOrAdd(Player);
		const double Now = FPlatformTime::Seconds();
		const bool bShow = FMath::Abs(Delta) >= 0.5f || Now - Change.Time > 1.5;
		Change.Delta = Now - Change.Time < 1.5 ? Change.Delta + Delta : Delta;
		Change.Time = Now;
		if (bShow)
		{
			const FText Amount = FText::AsNumber(FMath::RoundToInt(FMath::Abs(Change.Delta) * 10.0f) / 10.0f);
			const FText Line = Reason.IsEmpty() ? FText::Format(Delta > 0.0f ? LOCTEXT("UpPlain", "+{0} Verderbnis") : LOCTEXT("DownPlain", "−{0} Verderbnis"), Amount)
				: FText::Format(Delta > 0.0f ? LOCTEXT("Up", "+{0} Verderbnis · {1}") : LOCTEXT("Down", "−{0} Verderbnis · {1}"), Amount, Reason);
			UVaelCombatTextSubsystem::PostPickup(Player, Line, Delta > 0.0f ? GetZoneColor(EVaelBalanceZone::Shadow) : GetZoneColor(EVaelBalanceZone::Light));
		}
	}

	// Crossing into another zone is said aloud once
	if (GetZone(Before) != GetZone(After))
	{
		UVaelCombatTextSubsystem::PostReaction(Player, FText::Format(LOCTEXT("ZoneChange", "Deine Waage: {0}"), GetZoneName(GetZone(After))));
	}

	UE_LOG(LogVael, Log, TEXT("Balance: %s %.1f -> %.1f (%s)"), *GetNameSafe(Player), Before, After, *Reason.ToString());
	UVaelSaveSubsystem::RequestSave(Player);
	return After;
}

void UVaelBalanceSubsystem::ChangeRegionAt(const UObject* WorldContext, const FVector& Location, float Delta)
{
	UWorld* World = WorldContext != nullptr ? WorldContext->GetWorld() : nullptr;
	if (AVaelRegion* Region = World != nullptr ? AVaelRegion::GetRegionAt(World, Location) : nullptr)
	{
		Region->AddCorruption(Delta);
	}
}

void UVaelBalanceSubsystem::ChangeRegion(const FString& RegionName, float Delta)
{
	UWorld* World = GetGameInstance() != nullptr ? GetGameInstance()->GetWorld() : nullptr;
	// indoors (a refugium) the region of the level is only a stand-in: the change waits for the real one
	const bool bIndoor = AVaelIndoorLevel::IsIndoor(World);
	for (TActorIterator<AVaelRegion> It(World); World != nullptr && !bIndoor && It; ++It)
	{
		if (It->GetRegionName().ToString() == RegionName)
		{
			It->AddCorruption(Delta);
			return;
		}
	}
	PendingRegions.FindOrAdd(RegionName) += Delta;
}

float UVaelBalanceSubsystem::PurifyByRest(AVaelCharacter* Player, float Hours)
{
	UVaelMaterialBag* Bag = Player != nullptr ? Player->GetMaterialBag() : nullptr;
	if (Bag == nullptr || GetPlayer(Player) <= 0.0f)
	{
		return 0.0f;
	}
	for (const TCHAR* Name : { TEXT("Klartrunk"), TEXT("Lichttropfen") })
	{
		const UVaelMaterial* Cleanser = LoadObject<UVaelMaterial>(nullptr, *FString::Printf(TEXT("/Game/Vael/Items/Materials/DA_Material_%s.DA_Material_%s"), Name, Name));
		if (Cleanser != nullptr && Bag->GetCount(Cleanser) > 0)
		{
			Bag->RemoveMaterial(Cleanser, 1);
			const float Before = GetPlayer(Player);
			const float After = ChangePlayer(Player, -FMath::Max(Hours, 1.0f), FText::Format(LOCTEXT("RestReason", "Rast mit {0}"), Cleanser->DisplayName));
			return Before - After;
		}
	}
	UVaelCombatTextSubsystem::PostReaction(Player, LOCTEXT("RestHint", "Mit Klartrunk oder Lichttropfen läutert die Rast"));
	return 0.0f;
}

bool UVaelBalanceSubsystem::PurifyCrystal(AVaelCharacter* Player)
{
	UVaelMaterialBag* Bag = Player != nullptr ? Player->GetMaterialBag() : nullptr;
	const UVaelMaterial* Crystal = LoadObject<UVaelMaterial>(nullptr, TEXT("/Game/Vael/Items/Materials/DA_Material_Nidkristall.DA_Material_Nidkristall"));
	const UVaelMaterial* Purified = LoadObject<UVaelMaterial>(nullptr, TEXT("/Game/Vael/Items/Materials/DA_Material_GelaeuterterKristall.DA_Material_GelaeuterterKristall"));
	if (Bag == nullptr || Crystal == nullptr || Purified == nullptr || Bag->GetCount(Crystal) <= 0)
	{
		return false;
	}
	Bag->RemoveMaterial(Crystal, 1);
	Bag->AddMaterial(Purified, 1);
	ChangePlayer(Player, -5.0f, LOCTEXT("CrystalReason", "Nidkristall geläutert"));
	return true;
}

void UVaelBalanceSubsystem::RestGroup(UWorld* World, float Hours, bool bToMorning)
{
	if (World == nullptr)
	{
		return;
	}
	for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
	{
		if (It->Get() != nullptr && It->Get()->PlayerCameraManager != nullptr)
		{
			It->Get()->PlayerCameraManager->StartCameraFade(0.0f, 1.0f, 1.0f, FLinearColor::Black, false, true);
		}
	}
	FTimerHandle Handle;
	World->GetTimerManager().SetTimer(Handle, FTimerDelegate::CreateWeakLambda(World, [World, Hours, bToMorning]()
	{
		float Slept = Hours;
		if (UVaelTimeOfDay* Clock = UVaelTimeOfDay::Get(World))
		{
			const float Before = Clock->GetHour();
			if (bToMorning)
			{
				Clock->SkipToDawn();
			}
			else
			{
				Clock->SetHour(FMath::Fmod(Before + Hours, 24.0f));
			}
			Slept = bToMorning ? FMath::Fmod(Clock->GetHour() - Before + 24.0f, 24.0f) : Hours;
		}
		for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
		{
			APlayerController* PlayerController = It->Get();
			if (AVaelCharacter* Rested = PlayerController != nullptr ? PlayerController->GetPawn<AVaelCharacter>() : nullptr)
			{
				UVaelCombatStatics::Heal(Rested, Rested->GetMaxHealth());
				PurifyByRest(Rested, Slept);
			}
			if (PlayerController != nullptr && PlayerController->PlayerCameraManager != nullptr)
			{
				PlayerController->PlayerCameraManager->StartCameraFade(1.0f, 0.0f, 1.5f, FLinearColor::Black, false, false);
			}
		}
		UE_LOG(LogVael, Log, TEXT("Balance: the group rested %.1f hours"), Slept);
	}), 1.3f, false);
}

float UVaelBalanceSubsystem::GetRefugiumRegionValue() const
{
	UWorld* World = GetGameInstance() != nullptr ? GetGameInstance()->GetWorld() : nullptr;
	if (World != nullptr && !AVaelIndoorLevel::IsIndoor(World))
	{
		for (TActorIterator<AVaelRegion> It(World); It; ++It)
		{
			if (It->GetRegionName().ToString() == RefugiumRegion)
			{
				return It->GetCorruption();
			}
		}
	}
	float Saved = 0.0f;
	const UVaelSaveSubsystem* Save = UVaelSaveSubsystem::Get(World);
	if (Save != nullptr && Save->FindSavedRegionCorruption(TEXT("L_Aschenmark"), Saved))
	{
		return FMath::Clamp(Saved + PeekPending(RefugiumRegion), 0.0f, 100.0f);
	}
	return -1.0f;
}

float UVaelBalanceSubsystem::TakePending(const FString& RegionName)
{
	float Pending = 0.0f;
	PendingRegions.RemoveAndCopyValue(RegionName, Pending);
	return Pending;
}

bool UVaelBalanceSubsystem::GetLastChange(const AActor* Player, float& OutSecondsAgo, float& OutDelta) const
{
	const FChange* Change = LastChanges.Find(Player);
	if (Change == nullptr)
	{
		return false;
	}
	OutSecondsAgo = static_cast<float>(FPlatformTime::Seconds() - Change->Time);
	OutDelta = Change->Delta;
	return true;
}

float UVaelBalanceSubsystem::GetRefugiumDrift(int32* OutLightBeds, int32* OutShadowBeds) const
{
	int32 Light = 0;
	int32 Shadow = 0;
	if (const UVaelGardenSubsystem* Garden = GetGameInstance() != nullptr ? GetGameInstance()->GetSubsystem<UVaelGardenSubsystem>() : nullptr)
	{
		Garden->CountSown(FName(RefugiumLevel), Light, Shadow);
	}
	if (OutLightBeds != nullptr)
	{
		*OutLightBeds = Light;
	}
	if (OutShadowBeds != nullptr)
	{
		*OutShadowBeds = Shadow;
	}
	return (Shadow - Light) * BedDriftPerHour;
}

void UVaelBalanceSubsystem::UpdateMarks()
{
	UWorld* World = GetGameInstance() != nullptr ? GetGameInstance()->GetWorld() : nullptr;
	UMaterialInterface* MarkMaterial = World != nullptr ? LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Vael/Balance/M_Vael_Zeichnung.M_Vael_Zeichnung")) : nullptr;
	if (MarkMaterial == nullptr)
	{
		return;
	}
	for (TActorIterator<AVaelCharacter> It(World); It; ++It)
	{
		TObjectPtr<UMaterialInstanceDynamic>& Mark = Marks.FindOrAdd(*It);
		if (Mark == nullptr)
		{
			Mark = UMaterialInstanceDynamic::Create(MarkMaterial, this);
		}
		// every mesh of the body wears it (MetaHumans are several meshes); a new look of the hero gets it again
		// The Floromant keeps her own look: her Nid shows on the plants and the HUD (later on wings or clothes), never over the whole body
		const bool bWearsMark = !It->IsFloromant();
		TArray<USkeletalMeshComponent*> Meshes;
		It->GetComponents<USkeletalMeshComponent>(Meshes);
		for (USkeletalMeshComponent* Mesh : Meshes)
		{
			if (bWearsMark && Mesh->GetOverlayMaterial() != Mark)
			{
				Mesh->SetOverlayMaterial(Mark);
			}
			else if (!bWearsMark && Mesh->GetOverlayMaterial() == Mark)
			{
				Mesh->SetOverlayMaterial(nullptr);
			}
		}

		// light: a faint warm shimmer that fades towards 30; from there violet veins that grow with the value
		const float Value = GetPlayer(*It);
		const bool bVeins = Value >= LightBelow;
		const float Strength = bVeins ? 0.15f + 0.85f * FMath::Clamp((Value - LightBelow) / (100.0f - LightBelow), 0.0f, 1.0f) : 0.6f * (1.0f - Value / LightBelow);
		const FLinearColor Color = bVeins ? FMath::Lerp(GetZoneColor(EVaelBalanceZone::Twilight), GetZoneColor(EVaelBalanceZone::Shadow), FMath::Clamp((Value - 40.0f) / 40.0f, 0.0f, 1.0f))
			: GetZoneColor(EVaelBalanceZone::Light);
		Mark->SetScalarParameterValue(TEXT("Staerke"), Strength);
		Mark->SetScalarParameterValue(TEXT("Adern"), bVeins ? 1.0f : 0.0f);
		Mark->SetVectorParameterValue(TEXT("Farbe"), Color);
	}
}

void UVaelBalanceSubsystem::Tick(float DeltaTime)
{
	UpdateMarks();

	// The beds of the refugium work on their region by the clock of the world, also while nobody is down there
	const UVaelTimeOfDay* Clock = UVaelTimeOfDay::Get(GetGameInstance() != nullptr ? GetGameInstance()->GetWorld() : nullptr);
	if (Clock == nullptr)
	{
		return;
	}
	const float Hour = Clock->GetHour();
	if (BedClock <= 0.0f)
	{
		BedClock = Hour + 0.001f;
		return;
	}
	float Passed = Hour - (BedClock - 0.001f);
	if (Passed < 0.0f)
	{
		Passed += 24.0f;
	}
	if (Passed < 1.0f)
	{
		return;
	}
	BedClock = Hour + 0.001f;
	const float Drift = GetRefugiumDrift() * Passed;
	if (!FMath::IsNearlyZero(Drift))
	{
		ChangeRegion(RefugiumRegion, Drift);
		UE_LOG(LogVael, Log, TEXT("Balance: the beds of the Glutkammer move the %s by %.2f over %.1f h"), RefugiumRegion, Drift, Passed);
	}
}

void UVaelBalanceSubsystem::WriteSave(UVaelSaveGame& Save) const
{
	Save.PendingRegionCorruption = PendingRegions;
}

void UVaelBalanceSubsystem::ReadSave(const UVaelSaveGame& Save)
{
	PendingRegions = Save.PendingRegionCorruption;
}

#undef LOCTEXT_NAMESPACE
