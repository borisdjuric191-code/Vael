// Copyright Epic Games, Inc. All Rights Reserved.

#include "Nature/VaelMoonEave.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/PointLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Items/VaelMaterialBag.h"
#include "Player/VaelCharacter.h"
#include "UI/VaelNoticeSubsystem.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "UObject/ConstructorHelpers.h"
#include "World/VaelRegion.h"
#include "World/VaelTimeOfDay.h"

namespace
{
	/** Engine shapes: the plane and the cylinder are 100 cm across, the cylinder 100 cm tall around its middle */
	constexpr float ShapeSize = 100.0f;

	/** Swaying: degrees and swings per second of the bud, and of the threads */
	constexpr float BudSwayAngle = 2.2f;
	constexpr float BudSwaySpeed = 0.11f;
	constexpr float ThreadSwayAngle = 6.0f;
	constexpr float ThreadSwaySpeed = 0.3f;

	/** How bright everything glows by day, as a share of the night */
	constexpr float DayGlow = 0.4f;

	/** Seconds between two looks at the region */
	constexpr float SurroundingsInterval = 2.0f;

	/** The iso camera looks along this yaw; the curtain turns its face to it */
	constexpr float DefaultCameraYaw = 45.0f;

	/** Fallen petals: how they turn while drifting (degrees per second) */
	constexpr float PetalSpin = 4.0f;

	/** Tag of the parts Build makes, so they can be found and made again */
	const FName BuiltTag(TEXT("MondtraufeTeil"));

	/** Night factor from which a night counts as begun, for ripening */
	constexpr float RipeningNight = 0.7f;

	/** Ripe Mondtropfen glow this much brighter and breathe at this pace (per second) */
	constexpr float RipeGlow = 3.0f;
	constexpr float RipePulse = 0.8f;
}

AVaelMoonEave::AVaelMoonEave()
{
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));

	static ConstructorHelpers::FObjectFinder<UStaticMesh> PlaneMesh(TEXT("/Engine/BasicShapes/Plane.Plane"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CylinderMesh(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));

	const auto MakePart = [this](const TCHAR* Name, USceneComponent* Parent, UStaticMesh* Mesh)
	{
		UStaticMeshComponent* Part = CreateDefaultSubobject<UStaticMeshComponent>(Name);
		Part->SetupAttachment(Parent);
		Part->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Part->bReceivesDecals = false;
		if (Mesh != nullptr)
		{
			Part->SetStaticMesh(Mesh);
		}
		return Part;
	};

	Arch = MakePart(TEXT("Arch"), RootComponent, nullptr);
	Arch->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	Arch->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);

	BudPivot = CreateDefaultSubobject<USceneComponent>(TEXT("BudPivot"));
	BudPivot->SetupAttachment(RootComponent);
	Bud = MakePart(TEXT("Bud"), BudPivot, nullptr);
	Stalk = MakePart(TEXT("Stalk"), BudPivot, CylinderMesh.Object);

	Tip = CreateDefaultSubobject<USceneComponent>(TEXT("Tip"));
	Tip->SetupAttachment(BudPivot);

	Curtain = MakePart(TEXT("Curtain"), RootComponent, PlaneMesh.Object);
	Curtain->SetCastShadow(false);
	Pond = MakePart(TEXT("Pond"), RootComponent, CylinderMesh.Object);
	Pond->SetCastShadow(false);
	Ring = MakePart(TEXT("Ring"), RootComponent, PlaneMesh.Object);
	Ring->SetCastShadow(false);

	Glow = CreateDefaultSubobject<UPointLightComponent>(TEXT("Glow"));
	Glow->SetupAttachment(Tip);
	Glow->SetRelativeLocation(FVector(0.0f, 0.0f, -35.0f));
	Glow->SetIntensityUnits(ELightUnits::Candelas);
	Glow->SetAttenuationRadius(480.0f);
	Glow->SetCastShadows(false);

	ArchMesh = TSoftObjectPtr<UStaticMesh>(FSoftObjectPath(TEXT("/Game/Vael/Requisiten/Wurzelforst/Wurzelbogen/SM_Wurzelbogen.SM_Wurzelbogen")));
	BudMesh = TSoftObjectPtr<UStaticMesh>(FSoftObjectPath(TEXT("/Game/Vael/Requisiten/Wurzelforst/Mondtraufe/SM_Mondtraufe.SM_Mondtraufe")));
	BudMaterial = TSoftObjectPtr<UMaterialInterface>(FSoftObjectPath(TEXT("/Game/Vael/Requisiten/Wurzelforst/Mondtraufe/M_WF_Mondtraufe.M_WF_Mondtraufe")));
	DropMesh = TSoftObjectPtr<UStaticMesh>(FSoftObjectPath(TEXT("/Game/Vael/Items/Loot/Modelle/SM_Mondtropfen.SM_Mondtropfen")));
	DropMaterial = TSoftObjectPtr<UMaterialInterface>(FSoftObjectPath(TEXT("/Game/Vael/Items/Loot/Materialien/MI_Loot_Mondtropfen.MI_Loot_Mondtropfen")));
	ThreadMaterial = TSoftObjectPtr<UMaterialInterface>(FSoftObjectPath(TEXT("/Game/Vael/Requisiten/Wurzelforst/Mondtraufe/M_Mondtraufe_Faden.M_Mondtraufe_Faden")));
	CurtainMaterial = TSoftObjectPtr<UMaterialInterface>(FSoftObjectPath(TEXT("/Game/Vael/Requisiten/Wurzelforst/Mondtraufe/M_Mondtraufe_Vorhang.M_Mondtraufe_Vorhang")));
	RingMaterial = TSoftObjectPtr<UMaterialInterface>(FSoftObjectPath(TEXT("/Game/Vael/Requisiten/Wurzelforst/Mondtraufe/M_Mondtraufe_Ring.M_Mondtraufe_Ring")));
	PondMaterial = TSoftObjectPtr<UMaterialInterface>(FSoftObjectPath(TEXT("/Game/VFX/Water/Materials/MI_Water_Pond.MI_Water_Pond")));
	PetalMesh = TSoftObjectPtr<UStaticMesh>(FSoftObjectPath(TEXT("/Game/Vael/Items/Loot/Modelle/SM_Mondtraufe_Blatt.SM_Mondtraufe_Blatt")));
	PetalMaterial = TSoftObjectPtr<UMaterialInterface>(FSoftObjectPath(TEXT("/Game/Vael/Requisiten/Wurzelforst/Mondtraufe/M_Mondtraufe_Blatt.M_Mondtraufe_Blatt")));

	PrimaryActorTick.bCanEverTick = true;
}

void AVaelMoonEave::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);

	ClearBuilt();
	Build();
}

void AVaelMoonEave::BeginPlay()
{
	Super::BeginPlay();

	// Played in the editor the plant is a copy without its made parts' bookkeeping (and without its dynamic materials): build it anew
	ClearBuilt();
	Build();
	if (bHarvestable)
	{
		UVaelInteractionSubsystem::Register(this);
	}

	// The curtain shows its face to the camera
	float CameraYaw = DefaultCameraYaw;
	if (const APlayerController* PlayerController = GetWorld()->GetFirstPlayerController())
	{
		if (PlayerController->PlayerCameraManager != nullptr)
		{
			CameraYaw = PlayerController->PlayerCameraManager->GetCameraRotation().Yaw;
		}
	}
	Curtain->SetWorldRotation(FRotator(0.0f, CameraYaw + 90.0f, 90.0f));

	UpdateSurroundings();
	UpdateGlow();
}

void AVaelMoonEave::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	UVaelInteractionSubsystem::Unregister(this);
	Super::EndPlay(EndPlayReason);
}

void AVaelMoonEave::ClearBuilt()
{
	TArray<UActorComponent*> Made = GetComponentsByTag(USceneComponent::StaticClass(), BuiltTag);
	for (UActorComponent* Part : Made)
	{
		Part->DestroyComponent();
	}
	ThreadPivots.Reset();
	Threads.Reset();
	Drops.Reset();
	Petals.Reset();
	PetalStates.Reset();
	GlowMaterials.Reset();
	BudLook = nullptr;
}

void AVaelMoonEave::Build()
{
	const auto Load = [](const auto& Soft) { return Soft.IsNull() ? nullptr : Soft.LoadSynchronous(); };
	const auto Dynamic = [this](UStaticMeshComponent* Part, UMaterialInterface* Material)
	{
		if (Material != nullptr)
		{
			Part->SetMaterial(0, Material);
		}
		return Part->CreateAndSetMaterialInstanceDynamic(0);
	};

	// Arch, scaled to its height; the bud hangs from the measured point on it
	float ArchScale = 1.0f;
	if (UStaticMesh* ArchModel = Load(ArchMesh))
	{
		Arch->SetStaticMesh(ArchModel);
		const FBox Bounds = ArchModel->GetBoundingBox();
		ArchScale = ArchHeight / FMath::Max(Bounds.Max.Z - Bounds.Min.Z, 1.0f);
		Arch->SetRelativeScale3D(FVector(ArchScale));
	}
	Arch->SetRelativeLocationAndRotation(ArchOffset, FRotator(0.0f, ArchYaw, 0.0f));
	const FVector Hang = ArchOffset + FRotator(0.0f, ArchYaw, 0.0f).RotateVector(HangPoint * ArchScale);
	BudPivot->SetRelativeLocation(Hang);

	// The bud stands on its tip in the model: lowered by its length it hangs with the stalk's end at the pivot
	if (UStaticMesh* BudModel = Load(BudMesh))
	{
		Bud->SetStaticMesh(BudModel);
		const FBox Bounds = BudModel->GetBoundingBox();
		const float BudScale = BudLength / FMath::Max(Bounds.Max.Z - Bounds.Min.Z, 1.0f);
		Bud->SetRelativeScale3D(FVector(BudScale));
		Bud->SetRelativeLocation(FVector(0.0f, 0.0f, -Bounds.Max.Z * BudScale));
	}
	BudLook = Dynamic(Bud, Load(BudMaterial));

	// The stalk reaches on up into the branches, in the bark of the tree it hangs from
	Stalk->SetVisibility(StalkExtension > 0.0f);
	Stalk->SetRelativeLocation(FVector(0.0f, 0.0f, StalkExtension * 0.5f - 5.0f));
	Stalk->SetRelativeScale3D(FVector(StalkRadius * 2.0f / ShapeSize, StalkRadius * 2.0f / ShapeSize, (StalkExtension + 10.0f) / ShapeSize));
	if (Arch->GetStaticMesh() != nullptr)
	{
		Stalk->SetMaterial(0, Arch->GetMaterial(0));
	}
	Tip->SetRelativeLocation(FVector(0.0f, 0.0f, -BudLength + 2.0f));

	// Below: the pond it brings, the ring of light on it and the curtain from the tip down to it
	const FVector Under(Hang.X, Hang.Y, 0.0f);
	const float WaterHeight = WaterLevel;
	Pond->SetVisibility(bOwnPond);
	Pond->SetRelativeLocation(Under + FVector(0.0f, 0.0f, WaterHeight - 2.0f));
	Pond->SetRelativeScale3D(FVector(PondRadius * 2.0f / ShapeSize, PondRadius * 2.0f / ShapeSize, 0.04f));
	if (UMaterialInterface* Water = Load(PondMaterial))
	{
		Pond->SetMaterial(0, Water);
	}

	Ring->SetRelativeLocation(Under + FVector(0.0f, 0.0f, WaterHeight + 0.6f));
	Ring->SetRelativeScale3D(FVector(RingSize));
	GlowMaterials.Add(Dynamic(Ring, Load(RingMaterial)));

	const float TipHeight = Hang.Z - BudLength;
	Glow->SetAttenuationRadius(LightRadius);
	Glow->SetRelativeLocation(FVector(0.0f, 0.0f, -BudLength * 0.3f));
	const float CurtainHeight = FMath::Max(TipHeight - WaterHeight, 10.0f);
	Curtain->SetRelativeLocation(Under + FVector(0.0f, 0.0f, WaterHeight + CurtainHeight * 0.5f));
	Curtain->SetRelativeScale3D(FVector(CurtainWidth / ShapeSize, CurtainHeight / ShapeSize, 1.0f));
	Curtain->SetWorldRotation(FRotator(0.0f, DefaultCameraYaw + 90.0f, 90.0f));
	GlowMaterials.Add(Dynamic(Curtain, Load(CurtainMaterial)));

	// Threads with their Mondtropfen, of uneven length, around the tip; the place decides their lengths
	FRandomStream Random(static_cast<int32>(GetTypeHash(FIntVector(FMath::RoundToInt(GetActorLocation().X / 10.0f), FMath::RoundToInt(GetActorLocation().Y / 10.0f), 3))));
	UStaticMesh* Cylinder = Pond->GetStaticMesh();
	UStaticMesh* DropModel = Load(DropMesh);
	UMaterialInterface* ThreadLook = Load(ThreadMaterial);
	UMaterialInterface* DropLook = Load(DropMaterial);
	ThreadLengths.Reset();
	ThreadPhases.Reset();
	for (int32 Index = 0; Index < NumThreads; ++Index)
	{
		const float Angle = Index * 2.39996f;
		const float Length = Random.FRandRange(ThreadLength.X, ThreadLength.Y);
		ThreadLengths.Add(Length);
		ThreadPhases.Add(Random.FRandRange(0.0f, UE_TWO_PI));

		USceneComponent* Pivot = NewObject<USceneComponent>(this);
		Pivot->CreationMethod = EComponentCreationMethod::UserConstructionScript;
		Pivot->SetupAttachment(Tip);
		Pivot->SetRelativeLocation(FVector(FMath::Cos(Angle), FMath::Sin(Angle), 0.0f) * ThreadSpread * FMath::Sqrt((Index + 0.5f) / NumThreads));
		Pivot->ComponentTags.Add(BuiltTag);
		Pivot->RegisterComponent();
		ThreadPivots.Add(Pivot);

		UStaticMeshComponent* Thread = NewObject<UStaticMeshComponent>(this);
		Thread->CreationMethod = EComponentCreationMethod::UserConstructionScript;
		Thread->SetupAttachment(Pivot);
		Thread->SetStaticMesh(Cylinder);
		Thread->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Thread->SetCastShadow(false);
		Thread->SetRelativeLocation(FVector(0.0f, 0.0f, -Length * 0.5f));
		Thread->SetRelativeScale3D(FVector(ThreadThickness / ShapeSize, ThreadThickness / ShapeSize, Length / ShapeSize));
		Thread->ComponentTags.Add(BuiltTag);
		Thread->RegisterComponent();
		Threads.Add(Thread);
		GlowMaterials.Add(Dynamic(Thread, ThreadLook));

		UStaticMeshComponent* Drop = NewObject<UStaticMeshComponent>(this);
		Drop->CreationMethod = EComponentCreationMethod::UserConstructionScript;
		Drop->SetupAttachment(Pivot);
		Drop->SetStaticMesh(DropModel);
		Drop->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Drop->SetCastShadow(false);
		Drop->SetRelativeLocation(FVector(0.0f, 0.0f, -Length));
		Drop->SetRelativeRotation(FRotator(0.0f, Random.FRandRange(0.0f, 360.0f), 0.0f));
		Drop->SetRelativeScale3D(FVector(DropScale * Random.FRandRange(0.8f, 1.2f)));
		Drop->ComponentTags.Add(BuiltTag);
		Drop->RegisterComponent();
		Drops.Add(Drop);
		Dynamic(Drop, DropLook);
	}

	// Fallen petals are made now and shown as they fall; each has its own spot on the pond
	UStaticMesh* PetalModel = Load(PetalMesh);
	UMaterialInterface* PetalLook = Load(PetalMaterial);
	for (int32 Index = 0; Index < MaxFallenPetals && PetalModel != nullptr; ++Index)
	{
		FPetal State;
		const float Angle = Random.FRandRange(0.0f, UE_TWO_PI);
		State.Spot = FVector2D(Under.X, Under.Y) + FVector2D(FMath::Cos(Angle), FMath::Sin(Angle)) * FMath::Max(PondRadius, BudLength * 1.5f) * FMath::Sqrt(Random.FRandRange(0.05f, 0.6f));
		State.Yaw = Random.FRandRange(0.0f, 360.0f);
		State.Drift = Random.FRandRange(-1.0f, 1.0f);
		PetalStates.Add(State);

		UStaticMeshComponent* Petal = NewObject<UStaticMeshComponent>(this);
		Petal->CreationMethod = EComponentCreationMethod::UserConstructionScript;
		Petal->SetupAttachment(RootComponent);
		Petal->SetStaticMesh(PetalModel);
		if (PetalLook != nullptr)
		{
			Petal->SetMaterial(0, PetalLook);
		}
		Petal->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Petal->SetRelativeScale3D(FVector(PetalScale));
		Petal->SetVisibility(false);
		Petal->ComponentTags.Add(BuiltTag);
		Petal->RegisterComponent();
		Petals.Add(Petal);
	}

	Glow->SetLightColor(LightColor);
	UpdateGlow();
}

void AVaelMoonEave::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	Age += DeltaSeconds;
	if (Age >= NextSurroundingsAge)
	{
		NextSurroundingsAge = Age + SurroundingsInterval;
		UpdateSurroundings();
		UpdateGlow();
	}

	// The bud swings slowly on its stalk, the threads a little livelier beneath it
	const float Swing = Age * BudSwaySpeed * UE_TWO_PI;
	BudPivot->SetRelativeRotation(FRotator(BudSwayAngle * FMath::Sin(Swing), 0.0f, BudSwayAngle * 0.8f * FMath::Sin(Swing * 0.73f + 1.1f)));
	for (int32 Index = 0; Index < ThreadPivots.Num(); ++Index)
	{
		const float Phase = Age * ThreadSwaySpeed * UE_TWO_PI + ThreadPhases[Index];
		ThreadPivots[Index]->SetRelativeRotation(FRotator(ThreadSwayAngle * FMath::Sin(Phase), 0.0f, ThreadSwayAngle * 0.7f * FMath::Sin(Phase * 0.81f + 0.6f)));
	}

	// Petals: falling ones flutter down from the bud, fallen ones drift slowly round the pond
	const FVector Under(BudPivot->GetRelativeLocation().X, BudPivot->GetRelativeLocation().Y, 0.0f);
	for (int32 Index = 0; Index < Petals.Num(); ++Index)
	{
		FPetal& State = PetalStates[Index];
		if (State.FallStart < 0.0f)
		{
			continue;
		}

		const FVector2D Offset = State.Spot - FVector2D(Under.X, Under.Y);
		const float DriftAngle = FMath::DegreesToRadians(State.Drift * (Age - State.FallStart) * 1.5f);
		const FVector2D Drifted = FVector2D(Under.X, Under.Y) + FVector2D(Offset.X * FMath::Cos(DriftAngle) - Offset.Y * FMath::Sin(DriftAngle), Offset.X * FMath::Sin(DriftAngle) + Offset.Y * FMath::Cos(DriftAngle));
		const FVector OnWater(Drifted.X, Drifted.Y, WaterLevel + 1.0f + 0.6f * FMath::Sin(Age * 1.3f + Index));

		const float Fall = FMath::Clamp((Age - State.FallStart) / PetalFallSeconds, 0.0f, 1.0f);
		if (Fall < 1.0f)
		{
			const FVector Path = FMath::Lerp(State.FallFrom, OnWater, Fall);
			const float Flutter = FMath::Sin(Fall * 9.0f + Index) * 30.0f * (1.0f - Fall);
			Petals[Index]->SetRelativeLocation(Path + FVector(Flutter, Flutter * 0.6f, 0.0f));
			Petals[Index]->SetRelativeRotation(FRotator(60.0f * FMath::Sin(Fall * 7.0f) * (1.0f - Fall), State.Yaw + Fall * 220.0f, 45.0f * FMath::Cos(Fall * 5.0f) * (1.0f - Fall)));
		}
		else
		{
			Petals[Index]->SetRelativeLocation(OnWater);
			Petals[Index]->SetRelativeRotation(FRotator(0.0f, State.Yaw + 220.0f + (Age - State.FallStart) * PetalSpin * State.Drift, 0.0f));
		}
	}
}

void AVaelMoonEave::UpdateSurroundings()
{
	AVaelRegion* Region = AVaelRegion::GetRegionAt(GetWorld(), GetActorLocation());
	const float Corruption = Region != nullptr ? Region->GetCorruption() : 0.0f;
	Wilt = FMath::Clamp((Corruption - WiltCorruption.X) / FMath::Max(WiltCorruption.Y - WiltCorruption.X, 1.0f), 0.0f, 1.0f);

	// In a pure night now and then a Mondtropfen ripens on the great one
	const float Night = UVaelTimeOfDay::NightFactorOf(this);
	const bool bNight = Night >= RipeningNight;
	if (bNight && !bWasNight && bHarvestable && Wilt <= RipeMaxWilt && RipeDrops < FMath::Min(MaxRipe, Drops.Num()) && FMath::FRand() < RipenChance)
	{
		++RipeDrops;
	}
	bWasNight = bNight;
	// A sick land takes the ripe ones back
	if (Wilt > RipeMaxWilt)
	{
		RipeDrops = 0;
	}

	// One petal at a time: the plant loses them one after another as the land sickens, and they come back when it heals
	const int32 Wanted = FMath::RoundToInt(Wilt * Petals.Num());
	int32 Fallen = 0;
	for (const FPetal& State : PetalStates)
	{
		Fallen += State.FallStart >= 0.0f ? 1 : 0;
	}

	if (Fallen < Wanted)
	{
		FPetal& State = PetalStates[Fallen];
		State.FallStart = Age;
		const float Side = FMath::FRandRange(0.0f, UE_TWO_PI);
		State.FallFrom = BudPivot->GetRelativeLocation() + FVector(FMath::Cos(Side) * 30.0f, FMath::Sin(Side) * 30.0f, -BudLength * 0.45f);
		Petals[Fallen]->SetVisibility(true);
	}
	else if (Fallen > Wanted && Fallen > 0)
	{
		PetalStates[Fallen - 1].FallStart = -1.0f;
		Petals[Fallen - 1]->SetVisibility(false);
	}
}

void AVaelMoonEave::UpdateGlow()
{
	const UWorld* World = GetWorld();
	const float Night = World != nullptr && World->IsGameWorld() ? UVaelTimeOfDay::NightFactorOf(this) : 1.0f;
	const float Strength = FMath::Lerp(DayGlow, 1.0f, Night) * (1.0f - 0.75f * Wilt);

	Glow->SetIntensity(FMath::Lerp(LightIntensity.Y, LightIntensity.X, Night) * (1.0f - 0.7f * Wilt));
	for (UMaterialInstanceDynamic* Look : GlowMaterials)
	{
		if (Look != nullptr)
		{
			Look->SetScalarParameterValue(TEXT("Staerke"), Strength);
			Look->SetVectorParameterValue(TEXT("Farbe"), LightColor);
		}
	}

	if (BudLook != nullptr)
	{
		BudLook->SetScalarParameterValue(TEXT("Leuchten"), Strength);
		BudLook->SetScalarParameterValue(TEXT("Welke"), Wilt);
		BudLook->SetVectorParameterValue(TEXT("Glut"), LightColor);
	}

	// The Mondtropfen go out one after another as the plant sickens
	const int32 Lit = FMath::RoundToInt(Drops.Num() * (1.0f - 0.9f * Wilt));
	for (int32 Index = 0; Index < Drops.Num(); ++Index)
	{
		if (UMaterialInstanceDynamic* Look = Cast<UMaterialInstanceDynamic>(Drops[Index]->GetMaterial(0)))
		{
			const float Ripe = Index < RipeDrops ? RipeGlow * (0.75f + 0.25f * FMath::Sin(Age * RipePulse * UE_TWO_PI + Index)) : 1.0f;
			Look->SetScalarParameterValue(TEXT("Leuchten"), Index < Lit ? 1.2f * FMath::Lerp(0.6f, 1.0f, Night) * Ripe : 0.08f);
		}
	}
}

void AVaelMoonEave::RipenAll()
{
	RipeDrops = bHarvestable ? FMath::Min(MaxRipe, Drops.Num()) : 0;
	UpdateGlow();
}

bool AVaelMoonEave::CanInteract(const AVaelCharacter* Player) const
{
	return bHarvestable && RipeDrops > 0 && Wilt <= RipeMaxWilt;
}

void AVaelMoonEave::Interact(AVaelCharacter* Player)
{
	if (!CanInteract(Player))
	{
		return;
	}

	// The ripest one is taken; it comes loose, or it melts into the water before the hand closes round it
	--RipeDrops;
	const FLinearColor Moon(0.86f, 0.84f, 1.0f);
	if (FMath::FRand() < PickChance)
	{
		UVaelMaterialBag::GiveToGroup(this, { PickLoot });
		UVaelNoticeSubsystem::Post(this, NSLOCTEXT("VaelNature", "MoonDropPicked", "Ein Mondtropfen löst sich"), NSLOCTEXT("VaelNature", "MoonDropPickedDetail", "Kühl und schwer liegt er in der Hand."), Moon, 3.0f);
	}
	else
	{
		UVaelNoticeSubsystem::Post(this, NSLOCTEXT("VaelNature", "MoonDropMelted", "Der Mondtropfen zerfließt"), NSLOCTEXT("VaelNature", "MoonDropMeltedDetail", "Er rinnt ins Wasser, bevor du ihn fassen kannst."), Moon, 3.0f);
	}
	UpdateGlow();
}

FText AVaelMoonEave::GetInteractPrompt() const
{
	return NSLOCTEXT("VaelNature", "PickMoonDrop", "Mondtropfen pflücken");
}
