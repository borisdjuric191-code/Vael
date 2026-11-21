// Copyright Epic Games, Inc. All Rights Reserved.

#include "Creatures/VaelTropfseuche.h"
#include "Combat/VaelCombatStatics.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Creatures/VaelSchleimgaenger.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/TargetPoint.h"
#include "EngineUtils.h"
#include "Magic/VaelGroundArea.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Player/VaelCharacter.h"
#include "Vael.h"

#define LOCTEXT_NAMESPACE "VaelTropfseuche"

namespace
{
	FVaelSeuchenStamm MakeStrain(const FText& Name, EVaelPlantRarity Rarity, float Weight, float Health, float Damage, float Pace, EVaelStatus HitStatus,
		float HitSeconds, float Misfire, float Drift, float InfestSeconds, const FLinearColor& Color, float Glow, float Cover, float Flow, float Pulse, float Jerk,
		float Wobble)
	{
		FVaelSeuchenStamm Strain;
		Strain.Name = Name;
		Strain.Rarity = Rarity;
		Strain.Weight = Weight;
		Strain.HostHealth = Health;
		Strain.HostDamage = Damage;
		Strain.HostPace = Pace;
		Strain.HitStatus = HitStatus;
		Strain.HitStatusSeconds = HitSeconds;
		Strain.Misfire = Misfire;
		Strain.Drift = Drift;
		Strain.InfestSeconds = InfestSeconds;
		Strain.Color = Color;
		Strain.Glow = Glow;
		Strain.Cover = Cover;
		Strain.Flow = Flow;
		Strain.Pulse = Pulse;
		Strain.Jerk = Jerk;
		Strain.Wobble = Wobble;
		return Strain;
	}

	/** Slime icicles of the cave builder (SM_HK_Schleimzapfen_*, the colour in the name of their "Schleim" material) */
	struct FIcicle
	{
		FVector Location;
		EVaelSeuchenStamm Strain;
	};

	TMap<TWeakObjectPtr<const UWorld>, TArray<FIcicle>> IcicleCache;

	const TArray<FIcicle>& GetIcicles(const UWorld* World)
	{
		if (const TArray<FIcicle>* Found = IcicleCache.Find(World))
		{
			return *Found;
		}
		TArray<FIcicle>& Icicles = IcicleCache.Add(World);
		for (TActorIterator<AStaticMeshActor> It(World); It; ++It)
		{
			const UStaticMeshComponent* Component = It->GetStaticMeshComponent();
			if (Component == nullptr || Component->GetStaticMesh() == nullptr || !Component->GetStaticMesh()->GetName().Contains(TEXT("Schleimzapfen")))
			{
				continue;
			}
			const int32 Slot = Component->GetMaterialIndex(TEXT("Schleim"));
			const UMaterialInterface* Slime = Component->GetMaterial(Slot == INDEX_NONE ? 0 : Slot);
			const FString Colour = Slime != nullptr ? Slime->GetName() : FString();
			EVaelSeuchenStamm Strain = EVaelSeuchenStamm::Verwesung;
			if (Colour.Contains(TEXT("Bernstein")))
			{
				Strain = EVaelSeuchenStamm::Honig;
			}
			else if (Colour.Contains(TEXT("Tuerkis")))
			{
				Strain = EVaelSeuchenStamm::Kaelte;
			}
			else if (Colour.Contains(TEXT("Violett")))
			{
				Strain = EVaelSeuchenStamm::Nid;
			}
			else if (Colour.Contains(TEXT("Rot")))
			{
				Strain = EVaelSeuchenStamm::Rausch;
			}
			Icicles.Add({ It->GetActorLocation(), Strain });
		}
		return Icicles;
	}
}

UVaelTropfseucheSettings::UVaelTropfseucheSettings()
{
	ShellMaterial = TSoftObjectPtr<UMaterialInterface>(FSoftObjectPath(TEXT("/Game/Vael/Kreaturen/Eisenschlund/Tropfseuche/M_Tropfseuche_Huelle.M_Tropfseuche_Huelle")));

	// The rarer, the stronger and the more aggressive (Boris 2026-11-21); all numbers are proposals
	Strains.Add(MakeStrain(LOCTEXT("Verwesung", "Verwesung"), EVaelPlantRarity::Common, 55.0f, 1.4f, 1.0f, 1.0f, EVaelStatus::None, 0.0f,
		0.08f, 12.0f, 12.0f, FLinearColor(0.35f, 1.0f, 0.2f), 2.6f, 0.5f, 5.0f, 2.8f, 0.0f, 0.7f));
	Strains.Add(MakeStrain(LOCTEXT("Honig", "Honig"), EVaelPlantRarity::Special, 18.0f, 1.2f, 1.1f, 1.0f, EVaelStatus::Slowed, 2.5f,
		0.12f, 18.0f, 14.0f, FLinearColor(1.0f, 0.6f, 0.1f), 2.8f, 0.7f, 1.5f, 3.6f, 0.0f, 1.4f));
	Strains.Add(MakeStrain(LOCTEXT("Kaelte", "Kälte"), EVaelPlantRarity::Special, 15.0f, 1.3f, 1.5f, 0.8f, EVaelStatus::Slowed, 1.5f,
		0.12f, 14.0f, 14.0f, FLinearColor(0.1f, 0.9f, 0.8f), 2.4f, 0.45f, 1.0f, 5.0f, 0.0f, 0.4f));
	Strains.Add(MakeStrain(LOCTEXT("Nid", "Nid"), EVaelPlantRarity::Rare, 9.0f, 1.5f, 1.3f, 1.05f, EVaelStatus::Marked, 6.0f,
		0.18f, 24.0f, 16.0f, FLinearColor(0.6f, 0.2f, 1.0f), 3.2f, 0.55f, 4.0f, 2.2f, 0.35f, 0.9f));
	Strains.Add(MakeStrain(LOCTEXT("Rausch", "Rausch"), EVaelPlantRarity::Legendary, 3.0f, 1.6f, 1.6f, 1.35f, EVaelStatus::None, 0.0f,
		0.25f, 40.0f, 18.0f, FLinearColor(1.0f, 0.12f, 0.08f), 3.6f, 0.6f, 14.0f, 0.9f, 1.0f, 1.1f));

	FavouritePlaces.Add(TEXT("L_Gildenmine"), EVaelSeuchenStamm::Verwesung);
	FavouritePlaces.Add(TEXT("L_KoboldStollen"), EVaelSeuchenStamm::Honig);
	FavouritePlaces.Add(TEXT("L_TiefsterSchacht"), EVaelSeuchenStamm::Kaelte);
	FavouritePlaces.Add(TEXT("L_Gebeinkammer"), EVaelSeuchenStamm::Nid);
	FavouritePlaces.Add(TEXT("L_FunkenerzAdern"), EVaelSeuchenStamm::Rausch);

	HostLevels.Add(TEXT("L_Gildenmine"));
	HostLevels.Add(TEXT("L_Gebeinkammer"));
}

const FVaelSeuchenStamm& UVaelTropfseucheSettings::GetStrain(EVaelSeuchenStamm Strain) const
{
	const int32 Index = static_cast<int32>(Strain);
	static const FVaelSeuchenStamm Fallback;
	return Strains.IsValidIndex(Index) ? Strains[Index] : Fallback;
}

// ------------------------------------------------------------------ Infestation on a player

UVaelSeuchenBefall::UVaelSeuchenBefall()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickInterval = 0.0f;
}

void UVaelSeuchenBefall::Start(EVaelSeuchenStamm InStrain, float Seconds)
{
	const bool bNewStrain = Shell.Num() == 0 || InStrain != Strain;
	Strain = InStrain;
	bEnded = false;
	if (bNewStrain)
	{
		BuildShell();
	}
	Flare = 1.0f;
}

void UVaelSeuchenBefall::BuildShell()
{
	for (USkeletalMeshComponent* Old : Shell)
	{
		if (Old != nullptr)
		{
			Old->DestroyComponent();
		}
	}
	Shell.Reset();

	ShellLook = UVaelTropfseuche::MakeLook(this, Strain);
	AActor* Owner = GetOwner();
	if (ShellLook == nullptr || Owner == nullptr)
	{
		return;
	}
	// A second skin over every visible body mesh that follows its pose: the overlay slot of the player belongs to the
	// balance mark, so the film can't go there
	// The player's own meshes are mostly hidden: the look shown is a MetaHuman actor hanging on the body
	TArray<USkeletalMeshComponent*> Bodies;
	Owner->GetComponents<USkeletalMeshComponent>(Bodies);
	TArray<AActor*> Attached;
	Owner->GetAttachedActors(Attached, true, true);
	for (const AActor* Look : Attached)
	{
		TArray<USkeletalMeshComponent*> LookBodies;
		Look->GetComponents<USkeletalMeshComponent>(LookBodies);
		Bodies.Append(LookBodies);
	}
	for (USkeletalMeshComponent* Body : Bodies)
	{
		if (Body == nullptr || !Body->IsVisible() || Body->bHiddenInGame || Body->GetSkeletalMeshAsset() == nullptr || Body->ComponentHasTag(TEXT("VaelSeuchenHuelle")))
		{
			continue;
		}
		USkeletalMeshComponent* Skin = NewObject<USkeletalMeshComponent>(Body->GetOwner());
		Skin->ComponentTags.Add(TEXT("VaelSeuchenHuelle"));
		Skin->SetSkeletalMesh(Body->GetSkeletalMeshAsset());
		Skin->SetupAttachment(Body);
		Skin->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Skin->SetCastShadow(false);
		Skin->RegisterComponent();
		Skin->SetLeaderPoseComponent(Body);
		for (int32 Slot = 0; Slot < Skin->GetNumMaterials(); ++Slot)
		{
			Skin->SetMaterial(Slot, ShellLook);
		}
		Shell.Add(Skin);
	}
	UE_LOG(LogVael, Log, TEXT("Tropfseuche: film over %d of %d body meshes of '%s'"), Shell.Num(), Bodies.Num(), *Owner->GetName());
	for (const USkeletalMeshComponent* Body : Bodies)
	{
		UE_LOG(LogVael, Verbose, TEXT("  %s visible %d hidden %d mesh %s"), *Body->GetName(), Body->IsVisible(), Body->bHiddenInGame, *GetNameSafe(Body->GetSkeletalMeshAsset()));
	}
}

void UVaelSeuchenBefall::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	AActor* Owner = GetOwner();
	if (bEnded || Owner == nullptr)
	{
		return;
	}
	// Water washes it off; when the time ran out (or a potion took the status) it ends too
	if (UVaelCombatStatics::HasStatus(Owner, EVaelStatus::Wet))
	{
		UE_LOG(LogVael, Log, TEXT("Tropfseuche: water washes the infestation off '%s'"), *Owner->GetName());
		UVaelTropfseuche::Cure(Owner);
		return;
	}
	if (!UVaelCombatStatics::HasStatus(Owner, EVaelStatus::Infested))
	{
		End();
		return;
	}
	Flare = FMath::FInterpTo(Flare, 0.0f, DeltaTime, 3.0f);
	if (ShellLook != nullptr)
	{
		const FVaelSeuchenStamm& Look = UVaelTropfseucheSettings::Get()->GetStrain(Strain);
		// Faint on a player, so the figure stays readable; it flares up when the body resists
		ShellLook->SetScalarParameterValue(TEXT("Staerke"), Look.Glow * (0.35f + 1.2f * Flare));
		ShellLook->SetScalarParameterValue(TEXT("Deckung"), FMath::Min(1.0f, Look.Cover * (0.3f + 0.9f * Flare)));
	}
}

void UVaelSeuchenBefall::Resisted()
{
	Flare = 1.0f;
}

void UVaelSeuchenBefall::End()
{
	if (bEnded)
	{
		return;
	}
	bEnded = true;
	for (USkeletalMeshComponent* Skin : Shell)
	{
		if (Skin != nullptr)
		{
			Skin->DestroyComponent();
		}
	}
	Shell.Reset();
	DestroyComponent();
}

void UVaelSeuchenBefall::OnUnregister()
{
	for (USkeletalMeshComponent* Skin : Shell)
	{
		if (Skin != nullptr && Skin->IsRegistered())
		{
			Skin->DestroyComponent();
		}
	}
	Shell.Reset();
	Super::OnUnregister();
}

// ------------------------------------------------------------------ The Tropfseuche

EVaelSeuchenStamm UVaelTropfseuche::PickStrain(const UWorld* World, const FVector& Location)
{
	const UVaelTropfseucheSettings* Settings = UVaelTropfseucheSettings::Get();
	TArray<float> Weights;
	for (const FVaelSeuchenStamm& Strain : Settings->Strains)
	{
		Weights.Add(Strain.Weight);
	}
	if (Weights.Num() == 0)
	{
		return EVaelSeuchenStamm::Verwesung;
	}
	if (World != nullptr)
	{
		const FString Level = UWorld::RemovePIEPrefix(World->GetMapName());
		for (const TPair<FString, EVaelSeuchenStamm>& Place : Settings->FavouritePlaces)
		{
			if (Level.Contains(Place.Key) && Weights.IsValidIndex(static_cast<int32>(Place.Value)))
			{
				Weights[static_cast<int32>(Place.Value)] *= Settings->FavouriteWeight;
			}
		}
		// Its own slime nearby: once more as likely for each strain found within the radius
		TSet<EVaelSeuchenStamm> Near;
		for (const FIcicle& Icicle : GetIcicles(World))
		{
			if (FVector::Dist2D(Icicle.Location, Location) <= Settings->IcicleRadius)
			{
				Near.Add(Icicle.Strain);
			}
		}
		for (const EVaelSeuchenStamm Strain : Near)
		{
			if (Weights.IsValidIndex(static_cast<int32>(Strain)))
			{
				Weights[static_cast<int32>(Strain)] *= 2.0f;
			}
		}
	}
	float Total = 0.0f;
	for (const float Weight : Weights)
	{
		Total += Weight;
	}
	float Roll = FMath::FRandRange(0.0f, Total);
	for (int32 Index = 0; Index < Weights.Num(); ++Index)
	{
		Roll -= Weights[Index];
		if (Roll <= 0.0f)
		{
			return static_cast<EVaelSeuchenStamm>(Index);
		}
	}
	return EVaelSeuchenStamm::Verwesung;
}

UMaterialInstanceDynamic* UVaelTropfseuche::MakeLook(UObject* Outer, EVaelSeuchenStamm Strain)
{
	UMaterialInterface* Base = UVaelTropfseucheSettings::Get()->ShellMaterial.LoadSynchronous();
	if (Base == nullptr)
	{
		UE_LOG(LogVael, Warning, TEXT("Tropfseuche: the film material is missing (Scripts/BuildTropfseuche.py)"));
		return nullptr;
	}
	const FVaelSeuchenStamm& Look = UVaelTropfseucheSettings::Get()->GetStrain(Strain);
	UMaterialInstanceDynamic* Material = UMaterialInstanceDynamic::Create(Base, Outer);
	Material->SetVectorParameterValue(TEXT("Farbe"), Look.Color);
	Material->SetScalarParameterValue(TEXT("Staerke"), Look.Glow);
	Material->SetScalarParameterValue(TEXT("Deckung"), Look.Cover);
	Material->SetScalarParameterValue(TEXT("Fluss"), Look.Flow);
	Material->SetScalarParameterValue(TEXT("Puls"), Look.Pulse);
	Material->SetScalarParameterValue(TEXT("Zucken"), Look.Jerk);
	Material->SetScalarParameterValue(TEXT("Wabern"), Look.Wobble);
	return Material;
}

void UVaelTropfseuche::Infest(AActor* Attacker, AVaelCharacter* Player, EVaelSeuchenStamm Strain)
{
	if (Player == nullptr || Player->IsDowned())
	{
		return;
	}
	UVaelSeuchenBefall* Infestation = GetInfestation(Player);
	// A weaker strain doesn't push out a stronger one, it only renews it
	if (Infestation != nullptr && static_cast<int32>(Infestation->GetStrain()) > static_cast<int32>(Strain))
	{
		Strain = Infestation->GetStrain();
	}
	const FVaelSeuchenStamm& Settings = UVaelTropfseucheSettings::Get()->GetStrain(Strain);
	UVaelCombatStatics::ApplyStatus(Attacker, Player, EVaelStatus::Infested, Settings.InfestSeconds);
	if (!UVaelCombatStatics::HasStatus(Player, EVaelStatus::Infested))
	{
		return;
	}
	if (Infestation == nullptr)
	{
		Infestation = NewObject<UVaelSeuchenBefall>(Player);
		Infestation->RegisterComponent();
	}
	Infestation->Start(Strain, Settings.InfestSeconds);
	UE_LOG(LogVael, Log, TEXT("Tropfseuche: '%s' is infested (%s, %.0f s)"), *Player->GetName(), *Settings.Name.ToString(), Settings.InfestSeconds);
}

bool UVaelTropfseuche::Cure(AActor* Target)
{
	UVaelSeuchenBefall* Infestation = GetInfestation(Target);
	const bool bHad = UVaelCombatStatics::RemoveStatus(Target, EVaelStatus::Infested) || Infestation != nullptr;
	if (Infestation != nullptr)
	{
		Infestation->End();
	}
	return bHad;
}

UVaelSeuchenBefall* UVaelTropfseuche::GetInfestation(const AActor* Target)
{
	return Target != nullptr ? Target->FindComponentByClass<UVaelSeuchenBefall>() : nullptr;
}

bool UVaelTropfseuche::Resists(AActor* Player)
{
	UVaelSeuchenBefall* Infestation = GetInfestation(Player);
	if (Infestation == nullptr || !UVaelCombatStatics::HasStatus(Player, EVaelStatus::Infested))
	{
		return false;
	}
	if (FMath::FRand() >= UVaelTropfseucheSettings::Get()->GetStrain(Infestation->GetStrain()).Misfire)
	{
		return false;
	}
	// The body jerks and the action is lost
	Infestation->Resisted();
	if (AVaelCharacter* Character = Cast<AVaelCharacter>(Player))
	{
		Character->ApplyStun(0.3f);
	}
	UE_LOG(LogVael, Log, TEXT("Tropfseuche: the body of '%s' resists"), *Player->GetName());
	return true;
}

float UVaelTropfseuche::GetDrift(const AActor* Player, float Time)
{
	const UVaelSeuchenBefall* Infestation = GetInfestation(Player);
	if (Infestation == nullptr)
	{
		return 0.0f;
	}
	const float Most = UVaelTropfseucheSettings::Get()->GetStrain(Infestation->GetStrain()).Drift;
	return Most * (0.7f * FMath::Sin(Time * 1.3f) + 0.3f * FMath::Sin(Time * 3.1f + 1.0f));
}

bool UVaelTropfseuche::IsWaterAt(const UWorld* World, const FVector& Location)
{
	for (TActorIterator<AVaelGroundArea> It(World); World != nullptr && It; ++It)
	{
		if (It->GetElement() == EVaelElement::Water && !It->IsActorBeingDestroyed() && It->IsInRange(Location, 40.0f))
		{
			return true;
		}
	}
	return false;
}

void UVaelTropfseuche::SpawnHosts(UWorld* World)
{
	const UVaelTropfseucheSettings* Settings = UVaelTropfseucheSettings::Get();
	if (World == nullptr || !World->IsGameWorld())
	{
		return;
	}
	const FString Level = UWorld::RemovePIEPrefix(World->GetMapName());
	if (!Settings->HostLevels.ContainsByPredicate([&Level](const FString& Name) { return Level.Contains(Name); }))
	{
		return;
	}
	UVaelSchleimgaengerData* Data = GetMutableDefault<UVaelSchleimgaengerData>();
	int32 Places = 0;
	int32 Hosts = 0;
	for (TActorIterator<ATargetPoint> It(World); It; ++It)
	{
		if (!It->ActorHasTag(TEXT("VaelHoehleGegner")))
		{
			continue;
		}
		++Places;
		if (FMath::FRand() > Settings->PlaceChance)
		{
			continue;
		}
		const int32 Count = FMath::RandRange(Settings->HostsPerPlace.X, FMath::Max(Settings->HostsPerPlace.X, Settings->HostsPerPlace.Y));
		for (int32 Index = 0; Index < Count; ++Index)
		{
			const FVector Spot = It->GetActorLocation() + FVector(FMath::FRandRange(-160.0f, 160.0f), FMath::FRandRange(-160.0f, 160.0f), 200.0f);
			FVector Ground;
			if (!AVaelCreature::FindGround(World, Spot, Ground))
			{
				continue;
			}
			const EVaelSeuchenStamm Strain = PickStrain(World, Ground);
			if (AVaelCreature::SpawnCreature(World, Data, Ground, FRotator(0.0f, FMath::FRandRange(0.0f, 360.0f), 0.0f), nullptr,
				[Strain](AVaelCreature* Creature)
				{
					if (AVaelSchleimgaenger* Host = Cast<AVaelSchleimgaenger>(Creature))
					{
						Host->SetStrain(Strain);
					}
				}) != nullptr)
			{
				++Hosts;
			}
		}
	}
	UE_LOG(LogVael, Log, TEXT("Tropfseuche: %d Schleimgaenger at %d places in %s"), Hosts, Places, *Level);
}

#undef LOCTEXT_NAMESPACE
