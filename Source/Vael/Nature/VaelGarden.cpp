// Copyright Epic Games, Inc. All Rights Reserved.

#include "Nature/VaelGarden.h"
#include "Components/DecalComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Creatures/VaelCreature.h"
#include "Engine/GameInstance.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Items/VaelMaterial.h"
#include "Items/VaelMaterialBag.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Nature/VaelHarvestable.h"
#include "Nature/VaelRainBloom.h"
#include "Player/VaelCharacter.h"
#include "Save/VaelSaveGame.h"
#include "Save/VaelSaveSubsystem.h"
#include "Story/VaelNpc.h"
#include "UI/VaelCombatTextSubsystem.h"
#include "UI/VaelNoticeSubsystem.h"
#include "UObject/ConstructorHelpers.h"
#include "Vael.h"
#include "VaelAssets.h"
#include "TimerManager.h"
#include "World/VaelGround.h"
#include "World/VaelRegion.h"

#define LOCTEXT_NAMESPACE "VaelGarden"

namespace
{
	/** Size of the engine shapes in cm */
	constexpr float GardenShapeSize = 100.0f;

	/** Height of the pot in cm, and how big the grown plant is shown in it */
	constexpr float GardenPotHeight = 45.0f;
	constexpr float GardenPotPlantScale = 0.55f;

	/** The green is projected this far up and down from the ground in cm, so it also climbs the foot of walls */
	constexpr float GreenDepth = 160.0f;

	/** Seconds between two looks at how far the green should reach, and how fast it follows, in cm per second at least */
	constexpr float GreenAskInterval = 0.5f;
	constexpr float GreenFollowSpeed = 25.0f;

	/** Tufts sprout once the green has passed them by this share, keep this far from the plant in cm,
	 *  stand at most this much higher or lower than it and take this many seconds to grow up */
	constexpr float TuftBehindEdge = 0.85f;
	constexpr float TuftMinDistance = 90.0f;

	/** Tufts keep this far in cm from everything that is sown or can be gathered, so nothing is overgrown */
	constexpr float TuftClearance = 90.0f;
	constexpr float TuftMaxHeightDifference = 120.0f;
	constexpr float TuftGrowSeconds = 3.0f;

	const FLinearColor GardenNoticeColor(0.6f, 1.0f, 0.55f);
	const FLinearColor GardenSproutColor(0.35f, 0.9f, 0.4f);

	/** Name of the map as the save knows it */
	FName GardenLevelName(const UWorld* World)
	{
		return FName(*UWorld::RemovePIEPrefix(World->GetMapName()));
	}

	FVaelLootEntry GardenSeeds(const TSoftObjectPtr<UVaelMaterial>& Seed, int32 Count)
	{
		FVaelLootEntry Entry;
		Entry.Material = Seed;
		Entry.Chance = 1.0f;
		Entry.MinCount = Count;
		Entry.MaxCount = Count;
		return Entry;
	}
}

UVaelGardenSettings::UVaelGardenSettings()
{
	SurpriseSeed = TSoftObjectPtr<UVaelMaterial>(FSoftObjectPath(TEXT("/Game/Vael/Items/Materials/DA_Material_Ueberraschungssaat.DA_Material_Ueberraschungssaat")));

	const auto Add = [this](const TCHAR* Id, const FText& Name, float Chance, const FLinearColor& Glow, float Strength)
	{
		FVaelSeedPlant& Plant = Plants.AddDefaulted_GetRef();
		Plant.Id = Id;
		Plant.DisplayName = Name;
		Plant.Mesh = TSoftObjectPtr<UStaticMesh>(FSoftObjectPath(FString::Printf(TEXT("/Game/Vael/Requisiten/Aschenmark/%s/SM_%s.SM_%s"), Id, Id, Id)));
		Plant.Seed = TSoftObjectPtr<UVaelMaterial>(FSoftObjectPath(FString::Printf(TEXT("/Game/Vael/Items/Materials/DA_Material_Samen_%s.DA_Material_Samen_%s"), Id, Id)));
		Plant.Chance = Chance;
		Plant.GlowColor = Glow;
		Plant.GlowStrength = Strength;
	};

	// The common ones first; the grove sapling is the rare prize
	Add(TEXT("Glimmklee"), LOCTEXT("Glimmklee", "Glimmklee"), 1.0f, FLinearColor(1.0f, 0.62f, 0.15f), 1800.0f);
	Add(TEXT("Russrose"), LOCTEXT("Russrose", "Rußrose"), 0.8f, FLinearColor(0.2f, 1.0f, 0.5f), 1700.0f);
	Add(TEXT("Taublatt"), LOCTEXT("Taublatt", "Taublatt"), 0.9f, FLinearColor(0.3f, 0.85f, 1.0f), 1800.0f);
	Add(TEXT("Milchstern"), LOCTEXT("Milchstern", "Milchstern"), 0.8f, FLinearColor(0.85f, 0.8f, 1.0f), 2000.0f);
	Add(TEXT("Funkenmohn"), LOCTEXT("Funkenmohn", "Funkenmohn"), 0.8f, FLinearColor(1.0f, 0.4f, 0.1f), 2000.0f);
	Add(TEXT("Windhalm"), LOCTEXT("Windhalm", "Windhalm"), 0.8f, FLinearColor(0.4f, 0.95f, 1.0f), 1700.0f);
	Add(TEXT("Schlotwinde"), LOCTEXT("Schlotwinde", "Schlotwinde"), 0.7f, FLinearColor(1.0f, 0.8f, 0.2f), 1900.0f);
	Add(TEXT("Hainschoessling"), LOCTEXT("Hainschoessling", "Hainschössling"), 0.15f, FLinearColor(1.0f, 0.88f, 0.4f), 3200.0f);

	// The green around sown plants: a decal made by Scripts/CreateGardenMaterials.py, and moss and grass of the Megascans pack
	GreenMaterial = TSoftObjectPtr<UMaterialInterface>(FSoftObjectPath(TEXT("/Game/Vael/Effects/M_Vael_Gruen.M_Vael_Gruen")));

	const auto AddGreen = [this](const TCHAR* Folder, const TCHAR* Name)
	{
		GreenMeshes.Add(TSoftObjectPtr<UStaticMesh>(FSoftObjectPath(FString::Printf(TEXT("/Game/%s/%s.%s"), Folder, Name, Name))));
	};

	// Mostly grass, some moss and flowers; models that are missing are left out
	const TCHAR* StyleHex = TEXT("StyleHex_Studio/Free_Packs/FREE_Stylized_Grass_Flowers_Foliage/Meshes");
	AddGreen(StyleHex, TEXT("SM_Fol_Grass_Clump_1"));
	AddGreen(StyleHex, TEXT("SM_Fol_Grass_Clump_2"));
	AddGreen(StyleHex, TEXT("SM_Fol_Grass_Clump_3"));
	AddGreen(StyleHex, TEXT("SM_Fol_Flower_Clump_1"));
	AddGreen(StyleHex, TEXT("SM_Fol_Flower_Clump_2"));
	AddGreen(StyleHex, TEXT("SM_Fol_Flower_Clump_3"));
	AddGreen(StyleHex, TEXT("SM_Fol_Flower_Clump_4"));
	AddGreen(TEXT("Megascans/3D_Assets/IcelandicMossClusters"), TEXT("SM_IcelandicMossClusters_00"));
}

UVaelGardenSubsystem* UVaelGardenSubsystem::Get(const UObject* WorldContext)
{
	const UWorld* World = WorldContext != nullptr ? WorldContext->GetWorld() : nullptr;
	const UGameInstance* GameInstance = World != nullptr ? World->GetGameInstance() : nullptr;
	return GameInstance != nullptr ? GameInstance->GetSubsystem<UVaelGardenSubsystem>() : nullptr;
}

const FVaelSeedPlant* UVaelGardenSubsystem::FindPlant(FName PlantId)
{
	return UVaelGardenSettings::Get()->Plants.FindByPredicate([PlantId](const FVaelSeedPlant& Plant) { return Plant.Id == PlantId; });
}

FName UVaelGardenSubsystem::PickSurprise() const
{
	const TArray<FVaelSeedPlant>& Plants = UVaelGardenSettings::Get()->Plants;

	// While something is still unknown the seed becomes one of those; after that anything
	const bool bAllKnown = !Plants.ContainsByPredicate([this](const FVaelSeedPlant& Plant) { return !Researched.Contains(Plant.Id); });
	const auto Counts = [this, bAllKnown](const FVaelSeedPlant& Plant) { return bAllKnown || !Researched.Contains(Plant.Id); };

	float Total = 0.0f;
	for (const FVaelSeedPlant& Plant : Plants)
	{
		Total += Counts(Plant) ? Plant.Chance : 0.0f;
	}

	FName Picked = NAME_None;
	float Roll = FMath::FRand() * Total;
	for (const FVaelSeedPlant& Plant : Plants)
	{
		if (!Counts(Plant))
		{
			continue;
		}

		Picked = Plant.Id;
		Roll -= Plant.Chance;
		if (Roll <= 0.0f)
		{
			break;
		}
	}

	return Picked;
}

void UVaelGardenSubsystem::Research(FName PlantId, const AActor* Source)
{
	const FVaelSeedPlant* Plant = FindPlant(PlantId);
	if (Plant == nullptr)
	{
		return;
	}

	const UVaelGardenSettings* Settings = UVaelGardenSettings::Get();
	const bool bNew = !Researched.Contains(PlantId);
	Researched.Add(PlantId);

	UVaelMaterialBag::GiveToGroup(Source, { GardenSeeds(Plant->Seed, Settings->SeedsPerStudy) });

	UVaelNoticeSubsystem::Post(Source,
		FText::Format(bNew ? LOCTEXT("Researched", "Albrun hat erforscht: {0}") : LOCTEXT("GrownAgain", "Wieder gewachsen: {0}"), Plant->DisplayName),
		FText::Format(LOCTEXT("ResearchedDetail", "{0} Samen für jeden. Zum Säen E oder L1 halten."), FText::AsNumber(Settings->SeedsPerStudy)), Plant->GlowColor, 6.0f);

	UVaelSaveSubsystem::RequestSave(Source);

	UE_LOG(LogVael, Log, TEXT("Garden: %s researched, %d kinds known"), *PlantId.ToString(), Researched.Num());
}

bool UVaelGardenSubsystem::CanSow(const AVaelCharacter* Player) const
{
	const UVaelMaterialBag* Bag = Player != nullptr ? Player->GetMaterialBag() : nullptr;
	if (Bag == nullptr)
	{
		return false;
	}

	for (const FVaelSeedPlant& Plant : UVaelGardenSettings::Get()->Plants)
	{
		if (Researched.Contains(Plant.Id) && Bag->GetCount(Plant.Seed.LoadSynchronous()) > 0)
		{
			return true;
		}
	}

	return false;
}

AVaelSownPlant* UVaelGardenSubsystem::Sow(AVaelCharacter* Player)
{
	UVaelMaterialBag* Bag = Player != nullptr ? Player->GetMaterialBag() : nullptr;
	if (Bag == nullptr)
	{
		return nullptr;
	}

	const UVaelGardenSettings* Settings = UVaelGardenSettings::Get();
	UWorld* World = Player->GetWorld();

	// The researched kind the player carries most of
	const FVaelSeedPlant* Picked = nullptr;
	const UVaelMaterial* PickedSeed = nullptr;
	int32 MostSeeds = 0;
	for (const FVaelSeedPlant& Plant : Settings->Plants)
	{
		const UVaelMaterial* Seed = Researched.Contains(Plant.Id) ? Plant.Seed.LoadSynchronous() : nullptr;
		const int32 Count = Seed != nullptr ? Bag->GetCount(Seed) : 0;
		if (Count > MostSeeds)
		{
			Picked = &Plant;
			PickedSeed = Seed;
			MostSeeds = Count;
		}
	}

	if (Picked == nullptr)
	{
		return nullptr;
	}

	FVector Ground = Player->GetActorLocation() + Player->GetActorForwardVector() * Settings->SowDistance;
	if (!AVaelCreature::FindGround(World, Ground, Ground))
	{
		return nullptr;
	}

	// Plants need room
	const FName Level = GardenLevelName(World);
	for (const FVaelSavedPlant& Other : Sown)
	{
		if (Other.Level == Level && FVector::Dist2D(Other.Location, Ground) < Settings->SowSpacing)
		{
			UVaelCombatTextSubsystem::PostReaction(Player, LOCTEXT("NoRoom", "Kein Platz"));
			return nullptr;
		}
	}

	// How much new ground it covers: all of its share if no other plant stands within the spacing, less the closer the nearest one
	float NearestPlant = Settings->CleanseSpacing;
	for (const FVaelSavedPlant& Other : Sown)
	{
		if (Other.Level == Level)
		{
			NearestPlant = FMath::Min(NearestPlant, FVector::Dist2D(Other.Location, Ground));
		}
	}
	const float NewGround = FMath::Square(NearestPlant / Settings->CleanseSpacing);

	if (!Bag->RemoveMaterial(PickedSeed, 1))
	{
		return nullptr;
	}

	const float Yaw = FMath::FRandRange(0.0f, 360.0f);
	AVaelSownPlant* Plant = AVaelSownPlant::Plant(World, Picked->Id, Ground, Yaw, false);
	if (Plant == nullptr)
	{
		Bag->AddMaterial(PickedSeed, 1);
		return nullptr;
	}

	FVaelSavedPlant& Saved = Sown.AddDefaulted_GetRef();
	Saved.Level = Level;
	Saved.Plant = Picked->Id;
	Saved.Location = Ground;
	Saved.Yaw = Yaw;
	Saved.SownAt = Clock;

	// Every plant takes a little of the corruption out of its land
	if (AVaelRegion* Region = AVaelRegion::GetRegionAt(World, Ground))
	{
		// Plants at the spacing all over the region take all of its corruption
		const float Area = Region->GetArea();
		const float FullShare = Area > 0.0f ? 100.0f * FMath::Square(Settings->CleanseSpacing / 100.0f) / Area : Settings->CleansePerPlant;
		Region->AddCorruption(-FullShare * NewGround);

		UE_LOG(LogVael, Log, TEXT("Garden: %s sown, region of %.0f m2, takes %.2f of its corruption (%.0f %% new ground)"), *Picked->Id.ToString(), Area, FullShare * NewGround, NewGround * 100.0f);
	}

	// Nothing may hide the new plant: tufts of its neighbours make room
	for (TActorIterator<AVaelSownPlant> It(World); It; ++It)
	{
		It->ClearTuftsNear(Ground, TuftClearance);
	}

	UVaelCombatTextSubsystem::PostPickup(Player, FText::Format(LOCTEXT("Sown", "{0} gesät"), Picked->DisplayName), Picked->GlowColor);
	UVaelSaveSubsystem::RequestSave(Player);

	return Plant;
}

void UVaelGardenSubsystem::OnLevelStarted(UWorld* World)
{
	if (World == nullptr)
	{
		return;
	}

	const UVaelGardenSettings* Settings = UVaelGardenSettings::Get();

	// The green lies on the land, not on what comes and goes on it: creatures, spells, loot
	World->AddOnActorSpawnedHandler(FOnActorSpawned::FDelegate::CreateUObject(this, &UVaelGardenSubsystem::OnActorSpawned));

	// The pot stands beside its keeper
	for (TActorIterator<AVaelNpc> It(World); It; ++It)
	{
		if (It->GetDisplayName().ToString() != Settings->PotKeeper)
		{
			continue;
		}

		FVector Ground = It->GetActorLocation() + It->GetActorRotation().RotateVector(Settings->PotOffset);
		if (AVaelCreature::FindGround(World, Ground, Ground))
		{
			World->SpawnActor<AVaelSeedPot>(AVaelSeedPot::StaticClass(), Ground, FRotator::ZeroRotator);
		}
		break;
	}

	// What was sown here stands again
	const FName Level = GardenLevelName(World);
	for (const FVaelSavedPlant& Saved : Sown)
	{
		if (Saved.Level == Level)
		{
			AVaelSownPlant::Plant(World, Saved.Plant, Saved.Location, Saved.Yaw, true);
		}
	}
}

void UVaelGardenSubsystem::OnActorSpawned(AActor* Actor)
{
	UWorld* World = Actor != nullptr ? Actor->GetWorld() : nullptr;
	if (World == nullptr)
	{
		return;
	}

	// A moment later, once it has built everything it is made of
	const TWeakObjectPtr<AActor> WeakActor(Actor);
	FTimerHandle Handle;
	World->GetTimerManager().SetTimer(Handle, FTimerDelegate::CreateLambda([WeakActor]()
	{
		VaelGround::RefuseDecals(WeakActor.Get());
	}), 0.05f, false);
}

float UVaelGardenSubsystem::GetGreenRadius(const UWorld* World, const FVector& PlantLocation) const
{
	if (World == nullptr)
	{
		return 0.0f;
	}

	const UVaelGardenSettings* Settings = UVaelGardenSettings::Get();
	const FName Level = GardenLevelName(World);

	// The plant itself tells how long the green has grown, the others near it how far it is carried
	float Age = 0.0f;
	int32 NumNeighbours = 0;
	for (const FVaelSavedPlant& Plant : Sown)
	{
		if (Plant.Level != Level)
		{
			continue;
		}

		const float Distance = FVector::Dist2D(Plant.Location, PlantLocation);
		if (Distance < 1.0f)
		{
			Age = FMath::Max(Clock - Plant.SownAt, 0.0f);
		}
		else if (Distance < Settings->GreenLinkDistance)
		{
			++NumNeighbours;
		}
	}

	// Quick at first, so sowing shows at once; then slowly, so coming back later is worth it
	const float Radius = Age < Settings->GreenQuickSeconds
		? FMath::InterpEaseOut(Settings->GreenStartRadius, Settings->GreenQuickRadius, Age / Settings->GreenQuickSeconds, 2.0f)
		: FMath::Lerp(Settings->GreenQuickRadius, Settings->GreenFullRadius, FMath::Clamp((Age - Settings->GreenQuickSeconds) / Settings->GreenSlowSeconds, 0.0f, 1.0f));

	return Radius * (1.0f + Settings->GreenNeighbourBonus * FMath::Min(NumNeighbours, Settings->GreenMaxNeighbours));
}

float UVaelGardenSubsystem::GetMaxGreenRadius()
{
	const UVaelGardenSettings* Settings = UVaelGardenSettings::Get();
	return FMath::Max(Settings->GreenFullRadius, Settings->GreenQuickRadius) * (1.0f + Settings->GreenNeighbourBonus * Settings->GreenMaxNeighbours);
}

void UVaelGardenSubsystem::Tick(float DeltaTime)
{
	const UWorld* World = GetTickableGameObjectWorld();
	if (World != nullptr && World->IsGameWorld() && World->HasBegunPlay())
	{
		Clock += DeltaTime;
	}
}

UWorld* UVaelGardenSubsystem::GetTickableGameObjectWorld() const
{
	const UGameInstance* GameInstance = GetGameInstance();
	return GameInstance != nullptr ? GameInstance->GetWorld() : nullptr;
}

void UVaelGardenSubsystem::WriteSave(UVaelSaveGame& Save) const
{
	Save.GardenClock = Clock;
	Save.ResearchedPlants = Researched;
	Save.SownPlants = Sown;
	Save.PotPlant = PotPlant;
	Save.PotSecondsLeft = PotSecondsLeft;
}

void UVaelGardenSubsystem::ReadSave(const UVaelSaveGame& Save)
{
	Researched = Save.ResearchedPlants;
	Sown = Save.SownPlants;
	PotPlant = Save.PotPlant;
	PotSecondsLeft = Save.PotSecondsLeft;
	Clock = Save.GardenClock;
}

AVaelSeedPot::AVaelSeedPot()
{
	PrimaryActorTick.bCanEverTick = true;

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CylinderMesh(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> ConeMesh(TEXT("/Engine/BasicShapes/Cone.Cone"));

	// A clay pot of engine shapes until it has a model of its own
	Pot = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Pot"));
	Pot->SetStaticMesh(CylinderMesh.Object);
	Pot->SetRelativeScale3D(FVector(55.0f, 55.0f, GardenPotHeight) / GardenShapeSize);
	RootComponent = Pot;

	Sprout = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Sprout"));
	Sprout->SetStaticMesh(ConeMesh.Object);
	Sprout->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Sprout->SetupAttachment(Pot);
	Sprout->SetUsingAbsoluteScale(true);
	Sprout->SetVisibility(false);

	Plant = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Plant"));
	Plant->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Plant->SetupAttachment(Pot);
	Plant->SetUsingAbsoluteScale(true);
	Plant->SetVisibility(false);

	Glow = CreateDefaultSubobject<UPointLightComponent>(TEXT("Glow"));
	Glow->SetupAttachment(Pot);
	Glow->SetUsingAbsoluteScale(true);
	Glow->SetIntensity(0.0f);
	Glow->SetAttenuationRadius(350.0f);
	Glow->SetCastShadows(false);
}

void AVaelSeedPot::BeginPlay()
{
	Super::BeginPlay();

	// The pot stands on the ground: the cylinder has its middle at its origin
	AddActorWorldOffset(FVector(0.0f, 0.0f, GardenPotHeight * 0.5f));

	if (UMaterialInstanceDynamic* PotMaterial = Pot->CreateAndSetMaterialInstanceDynamic(0))
	{
		PotMaterial->SetVectorParameterValue(TEXT("Color"), FLinearColor(0.42f, 0.2f, 0.12f));
	}
	if (UMaterialInstanceDynamic* SproutMaterial = Sprout->CreateAndSetMaterialInstanceDynamic(0))
	{
		SproutMaterial->SetVectorParameterValue(TEXT("Color"), GardenSproutColor);
	}

	UVaelInteractionSubsystem::Register(this);
}

void AVaelSeedPot::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	UVaelInteractionSubsystem::Unregister(this);

	Super::EndPlay(EndPlayReason);
}

EVaelPotState AVaelSeedPot::GetPotState() const
{
	const UVaelGardenSubsystem* Garden = UVaelGardenSubsystem::Get(this);
	if (Garden == nullptr || Garden->PotPlant.IsNone())
	{
		return EVaelPotState::Empty;
	}

	return Garden->PotSecondsLeft > 0.0f ? EVaelPotState::Growing : EVaelPotState::Ready;
}

void AVaelSeedPot::FinishGrowing()
{
	if (UVaelGardenSubsystem* Garden = UVaelGardenSubsystem::Get(this))
	{
		Garden->PotSecondsLeft = 0.0f;
	}
}

bool AVaelSeedPot::CanInteract(const AVaelCharacter* Player) const
{
	return GetPotState() != EVaelPotState::Growing;
}

FText AVaelSeedPot::GetInteractPrompt() const
{
	return GetPotState() == EVaelPotState::Ready ? LOCTEXT("PotLook", "Pflanze im Topf ansehen") : LOCTEXT("PotPlant", "Überraschungssaat einpflanzen");
}

void AVaelSeedPot::Interact(AVaelCharacter* Player)
{
	UVaelGardenSubsystem* Garden = UVaelGardenSubsystem::Get(this);
	if (Garden == nullptr || Player == nullptr)
	{
		return;
	}

	const UVaelGardenSettings* Settings = UVaelGardenSettings::Get();

	switch (GetPotState())
	{
	case EVaelPotState::Empty:
	{
		const UVaelMaterial* Seed = Settings->SurpriseSeed.LoadSynchronous();
		if (Seed == nullptr || Player->GetMaterialBag() == nullptr || !Player->GetMaterialBag()->RemoveMaterial(Seed, 1))
		{
			UVaelNoticeSubsystem::Post(this, LOCTEXT("PotNoSeed", "Keine Überraschungssaat dabei"),
				LOCTEXT("PotNoSeedDetail", "Sie steckt nur im Keimpelz mit hellen Samenkapseln. Er sprießt mit dem Regen, etwa jeder zweite trägt Kapseln."), GardenNoticeColor, 5.0f);
			return;
		}

		Garden->PotPlant = Garden->PickSurprise();
		Garden->PotSecondsLeft = Settings->PotSeconds;
		UVaelNoticeSubsystem::Post(this, LOCTEXT("PotPlanted", "Eingepflanzt"), LOCTEXT("PotPlantedDetail", "Albrun sieht nach der Saat. In ein paar Minuten zeigt sich, was daraus wird."), GardenNoticeColor, 5.0f);
		UVaelSaveSubsystem::RequestSave(this);
		break;
	}

	case EVaelPotState::Ready:
	{
		const FName Grown = Garden->PotPlant;
		Garden->PotPlant = NAME_None;
		Garden->PotSecondsLeft = 0.0f;
		Garden->Research(Grown, this);
		break;
	}

	default:
		break;
	}
}

void AVaelSeedPot::RefreshPlant()
{
	const UVaelGardenSubsystem* Garden = UVaelGardenSubsystem::Get(this);
	const FName Wanted = Garden != nullptr && GetPotState() == EVaelPotState::Ready ? Garden->PotPlant : FName(NAME_None);
	if (Wanted == ShownPlant)
	{
		return;
	}

	ShownPlant = Wanted;

	const FVaelSeedPlant* Kind = UVaelGardenSubsystem::FindPlant(Wanted);
	Plant->SetStaticMesh(Kind != nullptr ? Kind->Mesh.LoadSynchronous() : nullptr);
	Plant->SetVisibility(Kind != nullptr);
	Glow->SetLightColor(Kind != nullptr ? Kind->GlowColor : GardenSproutColor);
}

void AVaelSeedPot::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	UVaelGardenSubsystem* Garden = UVaelGardenSubsystem::Get(this);
	if (Garden == nullptr)
	{
		return;
	}

	const UVaelGardenSettings* Settings = UVaelGardenSettings::Get();
	const float Time = GetWorld()->GetTimeSeconds();

	if (!Garden->PotPlant.IsNone() && Garden->PotSecondsLeft > 0.0f)
	{
		Garden->PotSecondsLeft = FMath::Max(Garden->PotSecondsLeft - DeltaSeconds, 0.0f);
	}

	RefreshPlant();

	const EVaelPotState State = GetPotState();
	const FVector Top = GetActorLocation() + FVector(0.0f, 0.0f, GardenPotHeight * 0.5f);

	// A green shoot grows out of the earth; nobody can tell yet what it is
	const float Grown = State == EVaelPotState::Growing ? 1.0f - Garden->PotSecondsLeft / FMath::Max(Settings->PotSeconds, 1.0f) : 0.0f;
	const float SproutHeight = 6.0f + Grown * 34.0f;
	Sprout->SetVisibility(State == EVaelPotState::Growing);
	Sprout->SetWorldScale3D(FVector(6.0f + Grown * 8.0f, 6.0f + Grown * 8.0f, SproutHeight) / GardenShapeSize);
	Sprout->SetWorldLocation(Top + FVector(0.0f, 0.0f, SproutHeight * 0.5f));
	Sprout->SetWorldRotation(FRotator(FMath::Sin(Time * 1.6f) * 4.0f, 0.0f, FMath::Sin(Time * 1.1f) * 4.0f));

	// Grown, the plant shows itself and shines until someone looks at it
	Plant->SetWorldScale3D(FVector(GardenPotPlantScale));
	Plant->SetWorldLocation(Top);
	Plant->SetWorldRotation(FRotator(0.0f, Time * 12.0f, 0.0f));

	Glow->SetWorldLocation(Top + FVector(0.0f, 0.0f, 40.0f));
	Glow->SetIntensity(State == EVaelPotState::Ready ? 2600.0f + FMath::Sin(Time * 3.0f) * 700.0f : State == EVaelPotState::Growing ? 300.0f + Grown * 500.0f : 0.0f);
}

AVaelSownPlant::AVaelSownPlant()
{
	PrimaryActorTick.bCanEverTick = true;

	Model = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Model"));
	Model->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	RootComponent = Model;

	Glow = CreateDefaultSubobject<UPointLightComponent>(TEXT("Glow"));
	Glow->SetupAttachment(Model);
	Glow->SetUsingAbsoluteScale(true);
	Glow->SetRelativeLocation(FVector(0.0f, 0.0f, 60.0f));
	Glow->SetIntensity(0.0f);
	Glow->SetAttenuationRadius(450.0f);
	Glow->SetCastShadows(false);

	// Looks down at the ground; its size is its scale, so the model growing and swaying doesn't move it
	Green = CreateDefaultSubobject<UDecalComponent>(TEXT("Green"));
	Green->SetupAttachment(Model);
	Green->SetUsingAbsoluteRotation(true);
	Green->SetUsingAbsoluteScale(true);
	Green->SetRelativeRotation(FRotator(-90.0f, 0.0f, 0.0f));
	Green->DecalSize = FVector(GreenDepth, GardenShapeSize, GardenShapeSize);
	Green->SetVisibility(false);
}

AVaelSownPlant* AVaelSownPlant::Plant(UWorld* World, FName InPlantId, const FVector& GroundLocation, float Yaw, bool bGrown)
{
	if (World == nullptr || UVaelGardenSubsystem::FindPlant(InPlantId) == nullptr)
	{
		return nullptr;
	}

	const FTransform SpawnTransform(FRotator(0.0f, Yaw, 0.0f), GroundLocation);
	AVaelSownPlant* Sown = World->SpawnActorDeferred<AVaelSownPlant>(AVaelSownPlant::StaticClass(), SpawnTransform, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (Sown != nullptr)
	{
		Sown->PlantId = InPlantId;
		Sown->BaseYaw = Yaw;
		Sown->Age = bGrown ? UVaelGardenSettings::Get()->GrowSeconds : 0.0f;
		Sown->bInstantGreen = bGrown;

		// The same plant looks the same every time: its size hangs on its place
		FRandomStream Random(GetTypeHash(GroundLocation.GridSnap(10.0f)));
		Sown->SizeScale = Random.FRandRange(0.85f, 1.15f);
		Sown->FinishSpawning(SpawnTransform);
	}

	return Sown;
}

void AVaelSownPlant::BeginPlay()
{
	Super::BeginPlay();

	if (const FVaelSeedPlant* Kind = UVaelGardenSubsystem::FindPlant(PlantId))
	{
		Model->SetStaticMesh(Kind->Mesh.LoadSynchronous());
		Glow->SetLightColor(Kind->GlowColor);
		GlowStrength = Kind->GlowStrength;
	}

	Model->SetRelativeScale3D(FVector(0.01f));
	Model->SetReceivesDecals(false);

	const UVaelGardenSettings* Settings = UVaelGardenSettings::Get();

	if (UMaterialInterface* GreenMaterial = VaelAssets::LoadOptional(Settings->GreenMaterial))
	{
		Green->SetDecalMaterial(GreenMaterial);
		Green->SetWorldRotation(FRotator(-90.0f, BaseYaw, 0.0f));
		Green->SetVisibility(true);
	}

	// Grass and moss, one component per model that exists
	for (const TSoftObjectPtr<UStaticMesh>& GreenMesh : Settings->GreenMeshes)
	{
		UStaticMesh* Mesh = VaelAssets::LoadOptional(GreenMesh);
		if (Mesh == nullptr)
		{
			continue;
		}

		UInstancedStaticMeshComponent* Component = NewObject<UInstancedStaticMeshComponent>(this);
		Component->SetStaticMesh(Mesh);
		Component->SetMobility(EComponentMobility::Movable);
		Component->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Component->SetCanEverAffectNavigation(false);
		Component->SetCastShadow(false);
		Component->SetUsingAbsoluteLocation(true);
		Component->SetUsingAbsoluteRotation(true);
		Component->SetUsingAbsoluteScale(true);
		Component->SetupAttachment(RootComponent);
		Component->RegisterComponent();
		Tufts.Add(Component);

		const FVector Extent = Mesh->GetBounds().BoxExtent;
		TuftModelWidths.Add(FMath::Max(2.0f * FMath::Max(Extent.X, Extent.Y), 1.0f));
	}

	// Where tufts can sprout, the same every time for the same place, sorted from the plant outwards
	if (!Tufts.IsEmpty())
	{
		const float MaxRadius = UVaelGardenSubsystem::GetMaxGreenRadius();
		const int32 NumSpots = FMath::Min(FMath::RoundToInt(UE_PI * FMath::Square(MaxRadius / 100.0f) * Settings->GreenTuftsPerSquareMeter), Settings->GreenMaxTufts);

		FRandomStream Random(GetTypeHash(GetActorLocation().GridSnap(10.0f)) + 17);
		TuftSpots.Reserve(NumSpots);
		for (int32 Index = 0; Index < NumSpots; ++Index)
		{
			const float Angle = Random.FRandRange(0.0f, UE_TWO_PI);

			FVaelTuftSpot& Spot = TuftSpots.AddDefaulted_GetRef();
			Spot.Distance = FMath::Max(FMath::Sqrt(Random.FRand()) * MaxRadius, TuftMinDistance);
			Spot.Offset = FVector2D(FMath::Cos(Angle), FMath::Sin(Angle)) * Spot.Distance;
			Spot.Yaw = Random.FRandRange(0.0f, 360.0f);
			Spot.Mesh = Random.RandRange(0, Tufts.Num() - 1);
			Spot.Scale = Random.FRandRange(Settings->GreenTuftWidth.X, Settings->GreenTuftWidth.Y) / TuftModelWidths[Spot.Mesh];
		}

		TuftSpots.Sort([](const FVaelTuftSpot& A, const FVaelTuftSpot& B) { return A.Distance < B.Distance; });
	}
}

void AVaelSownPlant::ClearTuftsNear(const FVector& Location, float Radius)
{
	for (int32 MeshIndex = 0; MeshIndex < Tufts.Num(); ++MeshIndex)
	{
		UInstancedStaticMeshComponent* Component = Tufts[MeshIndex];
		bool bChanged = false;

		for (int32 Instance = 0; Instance < Component->GetInstanceCount(); ++Instance)
		{
			// Shrunk to nothing instead of removed, so the others keep their numbers
			FTransform Shown;
			if (!Component->GetInstanceTransform(Instance, Shown, true) || FVector::Dist2D(Shown.GetLocation(), Location) >= Radius)
			{
				continue;
			}

			Shown.SetScale3D(FVector(0.001f));
			Component->UpdateInstanceTransform(Instance, Shown, true, false);
			bChanged = true;

			GrowingTufts.RemoveAllSwap([MeshIndex, Instance](const FVaelGrowingTuft& Tuft) { return Tuft.Mesh == MeshIndex && Tuft.Instance == Instance; });
		}

		if (bChanged)
		{
			Component->MarkRenderStateDirty();
		}
	}
}

void AVaelSownPlant::UpdateGreen(float DeltaSeconds)
{
	// The garden knows how far the green should reach; the shown green follows it, so a new neighbour doesn't make it jump
	GreenAskCountdown -= DeltaSeconds;
	if (GreenAskCountdown <= 0.0f)
	{
		GreenAskCountdown = GreenAskInterval;

		const UVaelGardenSubsystem* Garden = UVaelGardenSubsystem::Get(this);
		GreenTarget = Garden != nullptr ? Garden->GetGreenRadius(GetWorld(), GetActorLocation()) : 0.0f;

		// A plant that stood here before the session shows its green at once
		if (bInstantGreen)
		{
			GreenRadius = GreenTarget;
		}
	}

	GreenRadius = FMath::FInterpConstantTo(GreenRadius, GreenTarget, DeltaSeconds, FMath::Max(GreenFollowSpeed, FMath::Abs(GreenTarget - GreenRadius) * 0.5f));
	Green->SetWorldScale3D(FVector(1.0f, GreenRadius / GardenShapeSize, GreenRadius / GardenShapeSize));

	// Tufts sprout where the green has arrived, a few at a time
	const FVector Origin = GetActorLocation();
	const int32 MaxNew = bInstantGreen ? TuftSpots.Num() : 2;
	for (int32 NumNew = 0; NumNew < MaxNew && TuftSpots.IsValidIndex(NextTuft) && TuftSpots[NextTuft].Distance <= GreenRadius * TuftBehindEdge; ++NumNew, ++NextTuft)
	{
		const FVaelTuftSpot& Spot = TuftSpots[NextTuft];
		const FVector Place = Origin + FVector(Spot.Offset.X, Spot.Offset.Y, 0.0f);

		FHitResult GroundHit;
		if (!VaelGround::TraceGround(GetWorld(), Place + FVector(0.0f, 0.0f, 300.0f), Place - FVector(0.0f, 0.0f, 600.0f), GroundHit)
			|| FMath::Abs(GroundHit.Location.Z - Origin.Z) > TuftMaxHeightDifference || GroundHit.ImpactNormal.Z < 0.8f)
		{
			continue;
		}

		// The green grows around what is sown or can be gathered, never over it
		bool bTaken = false;
		for (TActorIterator<AVaelSownPlant> It(GetWorld()); It && !bTaken; ++It)
		{
			bTaken = FVector::Dist2D(It->GetActorLocation(), GroundHit.Location) < TuftClearance;
		}
		for (TActorIterator<AVaelHarvestable> It(GetWorld()); It && !bTaken; ++It)
		{
			bTaken = FVector::Dist2D(It->GetActorLocation(), GroundHit.Location) < TuftClearance;
		}
		for (TActorIterator<AVaelRainBloom> It(GetWorld()); It && !bTaken; ++It)
		{
			bTaken = FVector::Dist2D(It->GetActorLocation(), GroundHit.Location) < TuftClearance;
		}
		if (bTaken)
		{
			continue;
		}

		FVaelGrowingTuft& Tuft = GrowingTufts.AddDefaulted_GetRef();
		Tuft.Mesh = Spot.Mesh;
		Tuft.Grown = FTransform(FRotator(0.0f, Spot.Yaw, 0.0f), GroundHit.Location, FVector(Spot.Scale));
		Tuft.Age = bInstantGreen ? TuftGrowSeconds : 0.0f;

		FTransform Start = Tuft.Grown;
		Start.SetScale3D(FVector(0.01f));
		Tuft.Instance = Tufts[Tuft.Mesh]->AddInstance(Start, true);
	}

	bInstantGreen = false;

	for (int32 Index = GrowingTufts.Num() - 1; Index >= 0; --Index)
	{
		FVaelGrowingTuft& Tuft = GrowingTufts[Index];
		Tuft.Age += DeltaSeconds;

		const float Grown = FMath::Clamp(Tuft.Age / TuftGrowSeconds, 0.0f, 1.0f);
		FTransform Shown = Tuft.Grown;
		Shown.SetScale3D(Tuft.Grown.GetScale3D() * FMath::InterpEaseOut(0.01f, 1.0f, Grown, 2.0f));
		Tufts[Tuft.Mesh]->UpdateInstanceTransform(Tuft.Instance, Shown, true, true);

		if (Grown >= 1.0f)
		{
			GrowingTufts.RemoveAtSwap(Index);
		}
	}
}

void AVaelSownPlant::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	Age += DeltaSeconds;

	// Pushes out of the ground, overshoots a little and settles; then it sways and its light breathes
	const float Grown = FMath::Clamp(Age / FMath::Max(UVaelGardenSettings::Get()->GrowSeconds, 0.1f), 0.0f, 1.0f);
	const float Size = FMath::InterpEaseOut(0.02f, 1.0f, Grown, 2.5f) * (1.0f + FMath::Sin(Grown * UE_PI) * 0.12f);
	const float Phase = Age * 1.4f + GetActorLocation().X * 0.013f;

	Model->SetRelativeScale3D(FVector(Size * SizeScale));
	Model->SetRelativeRotation(FRotator(FMath::Sin(Phase) * 2.5f, BaseYaw, FMath::Sin(Phase * 0.7f) * 2.0f));
	Glow->SetIntensity(GlowStrength * Grown * (0.8f + 0.2f * FMath::Sin(Age * 2.1f)));

	UpdateGreen(DeltaSeconds);
}

#undef LOCTEXT_NAMESPACE
