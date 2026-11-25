// Copyright Epic Games, Inc. All Rights Reserved.

#include "Creatures/VaelKobold.h"
#include "AbilitySystemComponent.h"
#include "Combat/VaelArcShot.h"
#include "Combat/VaelAttributeSet.h"
#include "Combat/VaelGroundStrike.h"
#include "Combat/VaelCombatStatics.h"
#include "Components/CapsuleComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Creatures/VaelPollenBurst.h"
#include "Creatures/VaelSequenceAnimInstance.h"
#include "Creatures/VaelTropfseuche.h"
#include "Engine/StaticMesh.h"
#include "Engine/TargetPoint.h"
#include "EngineUtils.h"
#include "Misc/PackageName.h"
#include "UObject/Package.h"
#include "Items/VaelMaterial.h"
#include "Items/VaelMaterialBag.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Player/VaelCharacter.h"
#include "UI/VaelCombatTextSubsystem.h"
#include "UI/VaelNoticeSubsystem.h"
#include "Vael.h"
#include "World/VaelIndoorLevel.h"

#define LOCTEXT_NAMESPACE "VaelKobold"

namespace
{
	const FLinearColor KoboldKristall(1.0f, 0.2f, 0.6f);

	/**
	 *  The ore a Sprengmeister charge is plugged with (Boris 2026-11-23: all eleven ore colours mixed, each its own blast):
	 *  colour, element, how far it reaches and throws, and what it leaves on whoever is caught
	 */
	struct FVaelErzLadung
	{
		const TCHAR* Name;
		FLinearColor Color;
		EVaelElement Element;
		float RadiusScale;
		float Knockback;
		EVaelStatus Status;
		float StatusSeconds;
	};

	const FVaelErzLadung ErzLadungen[] = {
		{ TEXT("Funkenerz"), FLinearColor(1.0f, 0.35f, 0.05f), EVaelElement::Fire, 1.0f, 300.0f, EVaelStatus::Burning, 3.0f },
		{ TEXT("Rosteisen"), FLinearColor(1.0f, 0.45f, 0.12f), EVaelElement::Earth, 0.9f, 450.0f, EVaelStatus::None, 0.0f },
		{ TEXT("Schwefelkies"), FLinearColor(1.0f, 0.85f, 0.3f), EVaelElement::Fire, 1.25f, 200.0f, EVaelStatus::Confused, 2.0f },
		{ TEXT("Gruenspan"), FLinearColor(0.1f, 0.9f, 0.5f), EVaelElement::Earth, 1.1f, 250.0f, EVaelStatus::Slowed, 3.0f },
		{ TEXT("Salzspat"), FLinearColor(1.0f, 0.55f, 0.65f), EVaelElement::Water, 1.2f, 200.0f, EVaelStatus::Wet, 4.0f },
		{ TEXT("Drahtsilber"), FLinearColor(0.7f, 0.85f, 1.0f), EVaelElement::Air, 1.0f, 600.0f, EVaelStatus::None, 0.0f },
		{ TEXT("Blutzinnober"), FLinearColor(1.0f, 0.12f, 0.25f), EVaelElement::Fire, 0.85f, 350.0f, EVaelStatus::Burning, 4.0f },
		{ TEXT("Kobaltbluete"), FLinearColor(1.0f, 0.2f, 0.6f), EVaelElement::Air, 1.1f, 400.0f, EVaelStatus::None, 0.0f },
		{ TEXT("Siechglas"), FLinearColor(0.75f, 1.0f, 0.2f), EVaelElement::Earth, 1.15f, 150.0f, EVaelStatus::Infested, 0.0f },
		{ TEXT("Rippenspat"), FLinearColor(1.0f, 0.65f, 0.3f), EVaelElement::Earth, 0.8f, 500.0f, EVaelStatus::Slowed, 2.0f },
		{ TEXT("Kernerz"), FLinearColor(0.3f, 1.0f, 0.95f), EVaelElement::Water, 1.3f, 300.0f, EVaelStatus::Frozen, 1.2f },
	};

	const FVaelErzLadung& PickErzLadung()
	{
		return ErzLadungen[FMath::RandHelper(UE_ARRAY_COUNT(ErzLadungen))];
	}

	/** The Kontoraeltester's puffs: not the thistle spider's grey pollen, but bright and a little silly (Boris 2026-11-23) */
	void BuntePuffs(UWorld* World, const FVector& Location, float Radius, float Lifetime)
	{
		static const FLinearColor Bunt[] = { FLinearColor(1.0f, 0.3f, 0.75f), FLinearColor(0.25f, 0.95f, 0.9f), FLinearColor(1.0f, 0.85f, 0.2f),
			FLinearColor(0.6f, 1.0f, 0.3f), FLinearColor(0.65f, 0.4f, 1.0f), FLinearColor(1.0f, 0.55f, 0.2f) };
		const int32 Count = UE_ARRAY_COUNT(Bunt);
		const int32 First = FMath::RandHelper(Count);
		for (int32 Index = 0; Index < 3; ++Index)
		{
			const FLinearColor& A = Bunt[(First + Index * 2) % Count];
			const FLinearColor& B = Bunt[(First + Index * 2 + 1) % Count];
			const FVector Offset(FMath::FRandRange(-0.4f, 0.4f) * Radius, FMath::FRandRange(-0.4f, 0.4f) * Radius, Index * 0.25f * Radius);
			AVaelPollenBurst::Spawn(World, Location + Offset, Radius * (0.6f + 0.2f * Index), A, B, Lifetime * (0.8f + 0.2f * Index));
		}
	}
	/** The pale blue-white of the cave crystal in the lantern */
	const FLinearColor LanternColor(0.7f, 0.85f, 1.0f);
	/** Brightness of the lantern (cd): it hangs right next to the body, more washes it white */
	const float LanternCandelas = 3.5f;
	/** How bright the lantern flares while it lures */
	const float LureFlare = 3.0f;

	FVaelNightAttack MakeAttack(FName Clip, FVector2D Range, float HitTime, float Reach, float HitRadius, float Damage, float Cooldown,
		float Lunge = 0.0f, EVaelElement Element = EVaelElement::Earth)
	{
		FVaelNightAttack Attack;
		Attack.Clip = Clip;
		Attack.Range = Range;
		Attack.HitTime = HitTime;
		Attack.Reach = Reach;
		Attack.HitRadius = HitRadius;
		Attack.Damage = Damage;
		Attack.Cooldown = Cooldown;
		Attack.Lunge = Lunge;
		Attack.Element = Element;
		return Attack;
	}
}

// ------------------------------------------------------------------ Data

UVaelKoboldData::UVaelKoboldData()
{
	CreatureClass = AVaelKobold::StaticClass();
	AssetRegion = TEXT("Eisenschlund");
	// Under the ground day and night are the same to them
	DayAwareness = 1.0f;
	NightAwareness = 1.0f;
	AggroRange = 1500.0f;
	TurnRate = 220.0f;
	AttackPause = 0.8f;
	SizeRange = FVector2D(0.85f, 1.0f);
	IdleClip = TEXT("Wach_Idle");
	MoveClip = TEXT("Schleichen");
	HitClip = TEXT("Treffer");
	DeathClip = TEXT("Tod");
	LookBone = TEXT("head");
	LookMaxYaw = 70.0f;
	TwitchBones = { TEXT("head") };
	TwitchAngle = 8.0f;
	// Wind blows their tricks away (weakness table, Eisenschlund round 4)
	ElementMultipliers.Add(EVaelElement::Air, 1.5f);
}

UVaelKoboldDiebData::UVaelKoboldDiebData()
{
	DisplayName = LOCTEXT("Dieb", "Kobold-Dieb");
	KindId = TEXT("Kobold_Dieb");
	CompendiumId = KindId;
	// The rig of 2026-11-23 (clean hands, gear, five looks: kobold_variante.py K_<Body>_<0..4>)
	AssetName = TEXT("K_Schleimkratzer_0");
	Rolle = EVaelKoboldRolle::Dieb;
	MaxHealth = 45.0f;
	MoveSpeed = 62.0f;
	MoveClipSpeed = 40.0f;
	CollisionRadius = 22.0f;
	CollisionHalfHeight = 52.0f;
	KeepDistance = 60.0f;
	FleeSpeed = 220.0f;
	// A quick grab: the claws go for the bag
	Attacks.Add(MakeAttack(TEXT("Kratzen"), FVector2D(0.0f, 120.0f), 0.4f, 55.0f, 80.0f, 4.0f, 3.0f, 130.0f));
}

UVaelKoboldWaechterData::UVaelKoboldWaechterData()
{
	DisplayName = LOCTEXT("Waechter", "Kobold-Hortwächter");
	KindId = TEXT("Kobold_Waechter");
	CompendiumId = KindId;
	// The rig of 2026-11-23 (clean hands, gear, five looks: kobold_variante.py K_<Body>_<0..4>)
	AssetName = TEXT("K_Hortwaechter_0");
	Rolle = EVaelKoboldRolle::Waechter;
	MaxHealth = 170.0f;
	MoveSpeed = 72.0f;
	MoveClipSpeed = 51.0f;
	CollisionRadius = 30.0f;
	CollisionHalfHeight = 67.0f;
	KeepDistance = 80.0f;
	KnockbackMultiplier = 0.5f;
	Attacks.Add(MakeAttack(TEXT("Schlag"), FVector2D(0.0f, 140.0f), 0.6f, 75.0f, 95.0f, 18.0f, 2.6f, 60.0f));
	Attacks.Add(MakeAttack(TEXT("Kratzen"), FVector2D(0.0f, 110.0f), 0.4f, 60.0f, 80.0f, 9.0f, 1.6f));
}

UVaelKoboldZaubererData::UVaelKoboldZaubererData()
{
	DisplayName = LOCTEXT("Zauberer", "Kobold-Kontorältester");
	KindId = TEXT("Kobold_Zauberer");
	CompendiumId = KindId;
	// The rig of 2026-11-23 (clean hands, gear, five looks: kobold_variante.py K_<Body>_<0..4>)
	AssetName = TEXT("K_Kontoraeltester_0");
	Rolle = EVaelKoboldRolle::Zauberer;
	MaxHealth = 70.0f;
	MoveSpeed = 60.0f;
	MoveClipSpeed = 43.6f;
	CollisionRadius = 24.0f;
	CollisionHalfHeight = 57.0f;
	// Keeps away and lets the others fight
	KeepDistance = 520.0f;
	// Air = false copies, Earth = a misleading sign at the feet of its prey; both do no damage themselves
	Attacks.Add(MakeAttack(TEXT("Zaubern"), FVector2D(300.0f, 1000.0f), 0.8f, 0.0f, 0.0f, 0.0f, 14.0f, 0.0f, EVaelElement::Air));
	Attacks.Add(MakeAttack(TEXT("Zaubern"), FVector2D(150.0f, 900.0f), 0.8f, 0.0f, 0.0f, 0.0f, 7.0f, 0.0f, EVaelElement::Earth));
	Attacks.Add(MakeAttack(TEXT("Kratzen"), FVector2D(0.0f, 100.0f), 0.4f, 50.0f, 75.0f, 7.0f, 2.0f));
}

UVaelKoboldTreiberData::UVaelKoboldTreiberData()
{
	DisplayName = LOCTEXT("Treiber", "Kobold-Sklaventreiber");
	KindId = TEXT("Kobold_Treiber");
	CompendiumId = KindId;
	// The rig of 2026-11-23 (clean hands, gear, five looks: kobold_variante.py K_<Body>_<0..4>)
	AssetName = TEXT("K_Sklaventreiber_0");
	Rolle = EVaelKoboldRolle::Treiber;
	MaxHealth = 150.0f;
	MoveSpeed = 95.0f;
	MoveClipSpeed = 70.0f;
	CollisionRadius = 30.0f;
	CollisionHalfHeight = 87.0f;
	KeepDistance = 180.0f;
	KnockbackMultiplier = 0.7f;
	// The vial does little harm itself: the Tropfseuche in it is the danger
	ThrowDamage = 5.0f;
	ThrowRadius = 170.0f;
	// The whip reaches far: it stays out of reach of a sword and lashes
	Attacks.Add(MakeAttack(TEXT("Peitschen"), FVector2D(0.0f, 300.0f), 0.5f, 220.0f, 90.0f, 15.0f, 2.4f));
	Attacks.Add(MakeAttack(TEXT("Werfen"), FVector2D(350.0f, 1000.0f), 0.6f, 0.0f, 0.0f, 0.0f, 9.0f));
}

UVaelKoboldSprengmeisterData::UVaelKoboldSprengmeisterData()
{
	DisplayName = LOCTEXT("Sprengmeister", "Kobold-Sprengmeister");
	KindId = TEXT("Kobold_Sprengmeister");
	CompendiumId = KindId;
	// The rig of 2026-11-23 (clean hands, gear, five looks: kobold_variante.py K_<Body>_<0..4>)
	AssetName = TEXT("K_Sprengmeister_0");
	Rolle = EVaelKoboldRolle::Sprengmeister;
	MaxHealth = 120.0f;
	MoveSpeed = 70.0f;
	MoveClipSpeed = 50.0f;
	CollisionRadius = 34.0f;
	CollisionHalfHeight = 80.0f;
	// Keeps away and lets its charges fight
	KeepDistance = 480.0f;
	ThrowFlight = 1.1f;
	ThrowDamage = 22.0f;
	ThrowRadius = 220.0f;
	Attacks.Add(MakeAttack(TEXT("Werfen"), FVector2D(300.0f, 1100.0f), 0.6f, 0.0f, 0.0f, 0.0f, 4.5f, 0.0f, EVaelElement::Fire));
	Attacks.Add(MakeAttack(TEXT("Legen"), FVector2D(0.0f, 800.0f), 0.8f, 0.0f, 0.0f, 0.0f, 12.0f, 0.0f, EVaelElement::Fire));
	Attacks.Add(MakeAttack(TEXT("Kratzen"), FVector2D(0.0f, 110.0f), 0.4f, 55.0f, 80.0f, 8.0f, 2.0f));
	// What he carries (25.11.): the ore of his charges, the pink stoppers, the wire; for the engineer's Sprengmeister craft
	Loot.Add(FVaelLootEntry(TEXT("/Game/Vael/Items/Materials/DA_Material_Funkenerz.DA_Material_Funkenerz"), 0.6f, 1, 2));
	Loot.Add(FVaelLootEntry(TEXT("/Game/Vael/Items/Materials/DA_Material_Kobaltbluete.DA_Material_Kobaltbluete"), 0.35f));
	Loot.Add(FVaelLootEntry(TEXT("/Game/Vael/Items/Materials/DA_Material_Drahtsilber.DA_Material_Drahtsilber"), 0.2f));
}

UVaelKoboldLaternentraegerData::UVaelKoboldLaternentraegerData()
{
	DisplayName = LOCTEXT("Laternentraeger", "Kobold-Laternenträger");
	KindId = TEXT("Kobold_Laternentraeger");
	CompendiumId = KindId;
	// The rig of 2026-11-23 (clean hands, gear, five looks: kobold_variante.py K_<Body>_<0..4>)
	AssetName = TEXT("K_Laternentraeger_0");
	Rolle = EVaelKoboldRolle::Laternentraeger;
	MaxHealth = 55.0f;
	MoveSpeed = 55.0f;
	MoveClipSpeed = 35.5f;
	CollisionRadius = 22.0f;
	CollisionHalfHeight = 44.0f;
	// Stays far off in the dark where only its light is seen
	KeepDistance = 650.0f;
	FleeSpeed = 200.0f;
	Attacks.Add(MakeAttack(TEXT("Locken"), FVector2D(250.0f, 1300.0f), 0.8f, 0.0f, 0.0f, 0.0f, 9.0f, 0.0f, EVaelElement::Air));
	Attacks.Add(MakeAttack(TEXT("Kratzen"), FVector2D(0.0f, 100.0f), 0.4f, 45.0f, 70.0f, 5.0f, 2.2f));
}

// ------------------------------------------------------------------ Blast trap

AVaelSprengfalle::AVaelSprengfalle()
{
	PrimaryActorTick.bCanEverTick = true;
	Charge = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Charge"));
	Charge->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Sphere.Sphere")));
	Charge->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Charge->SetWorldScale3D(FVector(0.22f, 0.22f, 0.14f));
	RootComponent = Charge;
	Glow = CreateDefaultSubobject<UPointLightComponent>(TEXT("Glow"));
	Glow->SetupAttachment(Charge);
	Glow->SetCastShadows(false);
	Glow->SetLightColor(KoboldKristall);
	Glow->SetIntensity(0.0f);
}

AVaelSprengfalle* AVaelSprengfalle::Lay(AActor* InOwner, const FVector& GroundLocation, const UVaelKoboldData* Data)
{
	UWorld* World = InOwner != nullptr ? InOwner->GetWorld() : nullptr;
	if (World == nullptr || Data == nullptr)
	{
		return nullptr;
	}
	FActorSpawnParameters Parameters;
	Parameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AVaelSprengfalle* Trap = World->SpawnActor<AVaelSprengfalle>(GroundLocation + FVector(0.0f, 0.0f, 6.0f), FRotator::ZeroRotator, Parameters);
	if (Trap == nullptr)
	{
		return nullptr;
	}
	Trap->Layer = InOwner;
	Trap->Radius = Data->TrapRadius;
	Trap->ArmSeconds = Data->TrapArmSeconds;
	Trap->Fuse = Data->TrapFuse;
	Trap->Lifetime = Data->TrapSeconds;
	Trap->Damage = Data->TrapDamage;
	// A small charge with a glowing crystal plug of one of the ores
	Trap->Color = PickErzLadung().Color;
	Trap->Glow->SetLightColor(Trap->Color);
	Trap->Look = UVaelTropfseuche::MakeLumpLook(Trap, EVaelSeuchenStamm::Verwesung);
	if (Trap->Look != nullptr)
	{
		Trap->Look->SetVectorParameterValue(TEXT("Farbe"), Trap->Color);
		Trap->Look->SetScalarParameterValue(TEXT("Wabern"), 0.0f);
		Trap->Charge->SetMaterial(0, Trap->Look);
	}
	Trap->Glow->SetAttenuationRadius(Data->TrapRadius * 1.5f);
	Trap->Glow->SetRelativeLocation(FVector(0.0f, 0.0f, 150.0f));
	UE_LOG(LogVael, Log, TEXT("'%s' lays a blast trap at %s"), *GetNameSafe(InOwner), *GroundLocation.ToCompactString());
	return Trap;
}

void AVaelSprengfalle::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	Age += DeltaSeconds;
	if (Age >= Lifetime && FuseLeft < 0.0f)
	{
		// The fuse cord rots: it fizzles out
		AVaelPollenBurst::Spawn(GetWorld(), GetActorLocation(), 50.0f, FLinearColor(0.3f, 0.28f, 0.3f), Color, 0.8f);
		Destroy();
		return;
	}
	// A slow glow while it waits, a fast blink once the fuse burns
	const bool bArmed = Age >= ArmSeconds;
	const float Beat = FuseLeft >= 0.0f ? 0.5f + 0.5f * FMath::Sin(Age * 40.0f) : bArmed ? 0.35f + 0.25f * FMath::Sin(Age * 3.0f) : 0.15f;
	if (Look != nullptr)
	{
		Look->SetScalarParameterValue(TEXT("Staerke"), 2.5f * Beat);
		Look->SetScalarParameterValue(TEXT("Sichtbar"), 1.0f);
	}
	Glow->SetIntensity(4.0f * Beat);
	if (FuseLeft >= 0.0f)
	{
		FuseLeft -= DeltaSeconds;
		if (FuseLeft <= 0.0f)
		{
			FVaelSpellHit Hit;
			Hit.Damage = Damage;
			Hit.Element = EVaelElement::Fire;
			Hit.Knockback = 350.0f;
			AVaelGroundStrike::SpawnStrike(Layer.IsValid() ? Layer.Get() : this, GetActorLocation(), Hit, Radius * 1.8f, 0.05f, Color);
			AVaelPollenBurst::Spawn(GetWorld(), GetActorLocation() + FVector(0.0f, 0.0f, 40.0f), Radius * 1.6f, FLinearColor(0.35f, 0.3f, 0.3f), Color, 1.2f);
			UE_LOG(LogVael, Log, TEXT("A blast trap goes off at %s"), *GetActorLocation().ToCompactString());
			Destroy();
		}
		return;
	}
	if (!bArmed)
	{
		return;
	}
	for (TActorIterator<AVaelCharacter> It(GetWorld()); It; ++It)
	{
		if (!It->IsDowned() && FVector::Dist2D(It->GetActorLocation(), GetActorLocation()) <= Radius + It->GetSimpleCollisionRadius())
		{
			// The tripwire snaps: a short moment to jump away
			FuseLeft = Fuse;
			UVaelCombatTextSubsystem::PostReaction(*It, LOCTEXT("TrapTriggered", "Stolperdraht!"));
			UE_LOG(LogVael, Log, TEXT("'%s' trips a blast trap"), *It->GetName());
			break;
		}
	}
}

// ------------------------------------------------------------------ Misleading sign

AVaelIrrzeichen::AVaelIrrzeichen()
{
	PrimaryActorTick.bCanEverTick = true;
	Disc = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Disc"));
	Disc->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cylinder.Cylinder")));
	Disc->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Disc->SetCastShadow(false);
	RootComponent = Disc;
	Glow = CreateDefaultSubobject<UPointLightComponent>(TEXT("Glow"));
	Glow->SetupAttachment(Disc);
	Glow->SetCastShadows(false);
	Glow->SetLightColor(KoboldKristall);
	Glow->SetIntensity(0.0f);
}

AVaelIrrzeichen* AVaelIrrzeichen::Place(AActor* InCaster, const FVector& GroundLocation, float InRadius, float Seconds, float InConfusion)
{
	UWorld* World = InCaster != nullptr ? InCaster->GetWorld() : nullptr;
	if (World == nullptr)
	{
		return nullptr;
	}
	FActorSpawnParameters Parameters;
	Parameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AVaelIrrzeichen* Sign = World->SpawnActor<AVaelIrrzeichen>(GroundLocation + FVector(0.0f, 0.0f, 2.0f), FRotator::ZeroRotator, Parameters);
	if (Sign == nullptr)
	{
		return nullptr;
	}
	Sign->Caster = InCaster;
	Sign->Radius = InRadius;
	Sign->Lifetime = Seconds;
	Sign->Confusion = InConfusion;
	// A thin glowing disc in the colour of the Kobold crystals (the cylinder is 100 cm across and high)
	Sign->Disc->SetWorldScale3D(FVector(InRadius / 50.0f, InRadius / 50.0f, 0.015f));
	Sign->Look = UVaelTropfseuche::MakeLumpLook(Sign, EVaelSeuchenStamm::Verwesung);
	if (Sign->Look != nullptr)
	{
		Sign->Look->SetVectorParameterValue(TEXT("Farbe"), KoboldKristall);
		Sign->Look->SetScalarParameterValue(TEXT("Wabern"), 0.0f);
		Sign->Disc->SetMaterial(0, Sign->Look);
	}
	Sign->Glow->SetAttenuationRadius(InRadius * 2.0f);
	Sign->Glow->SetRelativeLocation(FVector(0.0f, 0.0f, 3000.0f));
	UE_LOG(LogVael, Log, TEXT("'%s' lays a misleading sign at %s"), *GetNameSafe(InCaster), *GroundLocation.ToCompactString());
	return Sign;
}

void AVaelIrrzeichen::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	Age += DeltaSeconds;
	const float Shown = FMath::Clamp(Age / 0.6f, 0.0f, 1.0f) * FMath::Clamp((Lifetime - Age) / 0.8f, 0.0f, 1.0f);
	const float Beat = 0.6f + 0.4f * FMath::Sin(Age * 6.0f);
	if (Look != nullptr)
	{
		Look->SetScalarParameterValue(TEXT("Staerke"), 2.0f * Shown * Beat);
		Look->SetScalarParameterValue(TEXT("Sichtbar"), Shown);
	}
	Glow->SetIntensity(5.0f * Shown * Beat);
	if (Age >= Lifetime)
	{
		Destroy();
		return;
	}
	// Armed once it glows fully: whoever stands on it loses the direction
	if (Age > 0.6f && Age >= NextCheck)
	{
		NextCheck = Age + 0.25f;
		for (TActorIterator<AVaelCharacter> It(GetWorld()); It; ++It)
		{
			if (!It->IsDowned() && FVector::Dist2D(It->GetActorLocation(), GetActorLocation()) <= Radius + It->GetSimpleCollisionRadius()
				&& !UVaelCombatStatics::HasStatus(*It, EVaelStatus::Confused))
			{
				UVaelCombatStatics::ApplyStatus(Caster.Get(), *It, EVaelStatus::Confused, Confusion);
				UE_LOG(LogVael, Log, TEXT("'%s' steps on a misleading sign and is confused"), *It->GetName());
			}
		}
	}
}

// ------------------------------------------------------------------ Kobold

AVaelKobold::AVaelKobold()
{
}

TSubclassOf<UVaelCreatureData> AVaelKobold::GetDefaultDataClass() const
{
	return UVaelKoboldData::StaticClass();
}

void AVaelKobold::BeginPlay()
{
	Super::BeginPlay();
	if (bIllusion)
	{
		// A false copy: one hit and it is gone
		if (UAbilitySystemComponent* AbilitySystem = GetAbilitySystemComponent())
		{
			AbilitySystem->SetNumericAttributeBase(UVaelAttributeSet::GetMaxHealthAttribute(), 1.0f);
			AbilitySystem->SetNumericAttributeBase(UVaelAttributeSet::GetHealthAttribute(), 1.0f);
		}
		// Compendium (Boris 2026-11-23): the real one casts a shadow, the false ones don't
		GetMesh()->SetCastShadow(false);
	}
	// The lantern bearer's false light hangs in its lantern (kobold_skelett.py puts the bone laterne_licht there)
	if (GetData<UVaelKoboldData>()->Rolle == EVaelKoboldRolle::Laternentraeger && GetMesh()->DoesSocketExist(TEXT("laterne_licht")))
	{
		LanternLight = NewObject<UPointLightComponent>(this);
		LanternLight->SetupAttachment(GetMesh(), TEXT("laterne_licht"));
		LanternLight->SetIntensityUnits(ELightUnits::Candelas);
		LanternLight->SetIntensity(LanternCandelas);
		LanternLight->SetAttenuationRadius(750.0f);
		LanternLight->SetSourceRadius(3.0f);
		LanternLight->SetCastShadows(true);
		LanternLight->SetLightColor(LanternColor);
		LanternLight->RegisterComponent();
		LanternTime = FMath::FRandRange(0.0f, 10.0f);
	}
	TickWhip();
	UE_LOG(LogVael, Log, TEXT("'%s' is a living Kobold (%s, %s)%s"), *GetName(), *GetCreatureName().ToString(), *GetData<UVaelKoboldData>()->AssetName,
		bIllusion ? TEXT(", a false copy") : TEXT(""));
}

float AVaelKobold::GetOutgoingDamageMultiplier() const
{
	return bIllusion ? 0.0f : Super::GetOutgoingDamageMultiplier();
}

void AVaelKobold::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	TickLantern(DeltaSeconds);
	TickWhip();
	if (bIllusion && !IsDead())
	{
		IllusionLeft -= DeltaSeconds;
		if (IllusionLeft <= 0.0f)
		{
			Vanish();
		}
	}
}

void AVaelKobold::TickLantern(float DeltaSeconds)
{
	if (LanternLight == nullptr)
	{
		return;
	}
	LanternTime += DeltaSeconds;
	LightOutLeft = FMath::Max(LightOutLeft - DeltaSeconds, 0.0f);
	// Out after a hit or in death; it comes back slowly, flickering like a draught on a flame
	const float Wanted = IsDead() || LightOutLeft > 0.0f ? 0.0f : 1.0f;
	LightShown = FMath::FInterpTo(LightShown, Wanted, DeltaSeconds, Wanted > LightShown ? 1.5f : 6.0f);
	const float Flicker = 0.85f + 0.1f * FMath::Sin(LanternTime * 7.3f) + 0.05f * FMath::Sin(LanternTime * 23.0f);
	LanternFlare = FMath::FInterpTo(LanternFlare, 0.0f, DeltaSeconds, 1.2f);
	LanternLight->SetIntensity(LanternCandelas * LightShown * Flicker * (1.0f + LanternFlare));
}

void AVaelKobold::ShowSlot(FName Slot, bool bShown)
{
	const int32 Index = GetMesh()->GetMaterialIndex(Slot);
	if (Index != INDEX_NONE)
	{
		if (UMaterialInstanceDynamic* Material = Cast<UMaterialInstanceDynamic>(GetMesh()->GetMaterial(Index)))
		{
			Material->SetScalarParameterValue(TEXT("Zeigen"), bShown ? 1.0f : 0.0f);
		}
	}
}

void AVaelKobold::TickWhip()
{
	// The Peitschen clip reaches to the hip first; at 0.15 s the hand has it (kobold_lebend_clips.py)
	const bool bInHand = GetCurrentAttackClip() == TEXT("Peitschen") && GetAttackTime() >= 0.15f && !IsDead();
	if (bInHand != bWhipInHand)
	{
		bWhipInHand = bInHand;
		ShowSlot(TEXT("Peitsche_Hand"), bInHand);
		ShowSlot(TEXT("Peitsche_Huefte"), !bInHand);
	}
}

void AVaelKobold::TickBehavior(float DeltaSeconds)
{
	const UVaelKoboldData* Data = GetData<UVaelKoboldData>();
	if (bHiding)
	{
		// Dark and away from whoever hit it, then back to luring
		float Distance = 0.0f;
		const AActor* Chaser = FindTarget(6000.0f, &Distance);
		const FVector Away = Chaser != nullptr ? (GetActorLocation() - Chaser->GetActorLocation()).GetSafeNormal2D() : GetActorForwardVector();
		TurnTowards(GetActorLocation() + Away * 400.0f, DeltaSeconds * 1.5f);
		MoveInDirection(GetActorForwardVector(), Data->FleeSpeed);
		Loop(Data->RunClip, 0.2f);
		if (Animator != nullptr)
		{
			Animator->SetRate(FMath::Clamp(GetVelocity().Size2D() / FMath::Max(91.0f * Size, 1.0f), 0.6f, 1.8f));
		}
		HideLeft -= DeltaSeconds;
		bHiding = HideLeft > 0.0f;
		return;
	}
	if (!bFleeing)
	{
		Super::TickBehavior(DeltaSeconds);
		return;
	}
	// Running off with what it took, away from the nearest player
	float Distance = 0.0f;
	const AActor* Chaser = FindTarget(6000.0f, &Distance);
	const FVector Away = Chaser != nullptr ? (GetActorLocation() - Chaser->GetActorLocation()).GetSafeNormal2D() : GetActorForwardVector();
	TurnTowards(GetActorLocation() + Away * 400.0f, DeltaSeconds * 1.5f);
	MoveInDirection(GetActorForwardVector(), Data->FleeSpeed);
	Loop(Data->RunClip, 0.2f);
	if (Animator != nullptr)
	{
		Animator->SetRate(FMath::Clamp(GetVelocity().Size2D() / FMath::Max(91.0f * Size, 1.0f), 0.6f, 1.8f));
	}
	FleeLeft -= DeltaSeconds;
	if (FleeLeft <= 0.0f)
	{
		UVaelNoticeSubsystem::Post(this, LOCTEXT("ThiefGone", "Der Dieb ist weg"),
			LOCTEXT("ThiefGoneDetail", "Der Kobold ist mit deiner Beute in den Stollen verschwunden."), KoboldKristall, 3.5f);
		UE_LOG(LogVael, Log, TEXT("'%s' got away with %d stolen stacks"), *GetName(), Stolen.Num());
		Vanish();
	}
}

void AVaelKobold::OnPreyHit(AVaelCharacter* Player, const FVaelNightAttack& Attack)
{
	if (!bIllusion && GetData<UVaelKoboldData>()->Rolle == EVaelKoboldRolle::Dieb && Attack.Clip == TEXT("Kratzen"))
	{
		Steal(Player);
	}
}

void AVaelKobold::OnAttackLanded(const FVaelNightAttack& Attack)
{
	if (bIllusion)
	{
		return;
	}
	const EVaelKoboldRolle Rolle = GetData<UVaelKoboldData>()->Rolle;
	if (Attack.Clip == TEXT("Zaubern"))
	{
		if (Attack.Element == EVaelElement::Air)
		{
			CastIllusions();
		}
		else
		{
			CastSign();
		}
	}
	else if (Attack.Clip == TEXT("Werfen"))
	{
		if (Rolle == EVaelKoboldRolle::Treiber)
		{
			ThrowVial();
		}
		else
		{
			ThrowCharge();
		}
	}
	else if (Attack.Clip == TEXT("Legen"))
	{
		LayTrap();
	}
	else if (Attack.Clip == TEXT("Locken"))
	{
		Lure();
	}
}

void AVaelKobold::OnDamageTaken(float Damage, const FGameplayTagContainer& DamageTags)
{
	Super::OnDamageTaken(Damage, DamageTags);
	// Hit, the lantern bearer puts its light out and slips away into the dark
	if (LanternLight != nullptr && !IsDead() && LightOutLeft <= 0.0f && Damage > 0.0f)
	{
		const UVaelKoboldData* Data = GetData<UVaelKoboldData>();
		LightOutLeft = Data->LightOutSeconds;
		bHiding = true;
		HideLeft = Data->HideSeconds;
		UVaelCombatTextSubsystem::PostReaction(this, LOCTEXT("LightOut", "Licht aus!"));
		UE_LOG(LogVael, Log, TEXT("'%s' puts its lantern out and hides"), *GetName());
	}
}

void AVaelKobold::ThrowVial()
{
	const UVaelKoboldData* Data = GetData<UVaelKoboldData>();
	UWorld* World = GetWorld();
	FVector Ground;
	if (!Prey.IsValid() || !AVaelCreature::FindGround(World, Prey->GetActorLocation() + FVector(0.0f, 0.0f, 100.0f), Ground))
	{
		return;
	}
	// A glass vial of Tropfseuche: little harm in the shards, but whoever stands in the splash is infested
	const EVaelSeuchenStamm Strain = UVaelTropfseuche::PickStrain(World, Ground);
	const FLinearColor Color = UVaelTropfseucheSettings::Get()->GetStrain(Strain).Color;
	FVaelSpellHit Hit;
	Hit.Damage = Data->ThrowDamage;
	Hit.Element = EVaelElement::Earth;
	AVaelGroundStrike::SpawnStrike(this, Ground, Hit, Data->ThrowRadius, Data->ThrowFlight, Color);
	AVaelArcShot::Lob(World, GetMesh()->GetSocketLocation(TEXT("hand_r")), Ground, Data->ThrowFlight, Data->ThrowApex, Color, 12.0f);
	const float Radius = Data->ThrowRadius;
	TWeakObjectPtr<AActor> Thrower(this);
	FTimerHandle Splash;
	World->GetTimerManager().SetTimer(Splash, FTimerDelegate::CreateWeakLambda(World, [World, Thrower, Ground, Radius, Strain]()
	{
		AVaelPollenBurst::Spawn(World, Ground + FVector(0.0f, 0.0f, 30.0f), Radius, UVaelTropfseucheSettings::Get()->GetStrain(Strain).Color,
			UVaelTropfseucheSettings::Get()->GetStrain(Strain).Color, 1.0f);
		for (TActorIterator<AVaelCharacter> It(World); It; ++It)
		{
			if (!It->IsDowned() && FVector::Dist2D(It->GetActorLocation(), Ground) <= Radius + It->GetSimpleCollisionRadius())
			{
				UVaelTropfseuche::Infest(Thrower.Get(), *It, Strain);
			}
		}
	}), Data->ThrowFlight, false);
	UE_LOG(LogVael, Log, TEXT("'%s' throws a vial of Tropfseuche (%s)"), *GetName(), *UVaelTropfseucheSettings::Get()->GetStrain(Strain).Name.ToString());
}

void AVaelKobold::ThrowCharge()
{
	const UVaelKoboldData* Data = GetData<UVaelKoboldData>();
	FVector Ground;
	if (!Prey.IsValid() || !AVaelCreature::FindGround(GetWorld(), Prey->GetActorLocation() + FVector(0.0f, 0.0f, 100.0f), Ground))
	{
		return;
	}
	// A crystal charge plugged with one of the eleven ores: the mark on the ground gives the flight time to jump away, and its
	// colour tells what it does
	const FVaelErzLadung& Erz = PickErzLadung();
	FVaelSpellHit Hit;
	Hit.Damage = Data->ThrowDamage;
	Hit.Element = Erz.Element;
	Hit.Knockback = Erz.Knockback;
	const float Radius = Data->ThrowRadius * Erz.RadiusScale;
	UWorld* World = GetWorld();
	AVaelGroundStrike::SpawnStrike(this, Ground, Hit, Radius, Data->ThrowFlight, Erz.Color);
	AVaelArcShot::Lob(World, GetMesh()->GetSocketLocation(TEXT("hand_r")), Ground, Data->ThrowFlight, Data->ThrowApex, Erz.Color, 14.0f);
	const FLinearColor Color = Erz.Color;
	const EVaelStatus Status = Erz.Status;
	const float StatusSeconds = Erz.StatusSeconds;
	TWeakObjectPtr<AActor> Thrower(this);
	FTimerHandle Blast;
	World->GetTimerManager().SetTimer(Blast, FTimerDelegate::CreateWeakLambda(World, [World, Thrower, Ground, Radius, Color, Status, StatusSeconds]()
	{
		AVaelPollenBurst::Spawn(World, Ground + FVector(0.0f, 0.0f, 40.0f), Radius * 1.2f, Color, FLinearColor::White, 0.9f);
		if (Status == EVaelStatus::None)
		{
			return;
		}
		for (TActorIterator<AVaelCharacter> It(World); It; ++It)
		{
			if (It->IsDowned() || FVector::Dist2D(It->GetActorLocation(), Ground) > Radius + It->GetSimpleCollisionRadius())
			{
				continue;
			}
			if (Status == EVaelStatus::Infested)
			{
				UVaelTropfseuche::Infest(Thrower.Get(), *It, UVaelTropfseuche::PickStrain(World, Ground));
			}
			else
			{
				UVaelCombatStatics::ApplyStatus(Thrower.Get(), *It, Status, StatusSeconds);
			}
		}
	}), Data->ThrowFlight, false);
	UE_LOG(LogVael, Log, TEXT("'%s' throws a crystal charge of %s"), *GetName(), Erz.Name);
}

void AVaelKobold::LayTrap()
{
	// On the way between it and its prey, so whoever charges at it runs into the wire
	FVector Spot = GetActorLocation() + GetActorForwardVector() * 120.0f;
	if (Prey.IsValid())
	{
		const FVector ToPrey = Prey->GetActorLocation() - GetActorLocation();
		Spot = GetActorLocation() + ToPrey.GetSafeNormal2D() * FMath::Clamp(ToPrey.Size2D() * 0.5f, 120.0f, 350.0f);
	}
	FVector Ground;
	if (AVaelCreature::FindGround(GetWorld(), Spot + FVector(0.0f, 0.0f, 150.0f), Ground))
	{
		AVaelSprengfalle::Lay(this, Ground, GetData<UVaelKoboldData>());
	}
}

void AVaelKobold::Lure()
{
	AVaelCharacter* Player = Cast<AVaelCharacter>(Prey.Get());
	if (Player == nullptr || LanternLight == nullptr || LightOutLeft > 0.0f)
	{
		return;
	}
	// The false light flares: whoever looks into it loses the way
	LanternFlare = LureFlare;
	UVaelCombatStatics::ApplyStatus(this, Player, EVaelStatus::Confused, GetData<UVaelKoboldData>()->LureConfusion);
	AVaelPollenBurst::Spawn(GetWorld(), LanternLight->GetComponentLocation(), 70.0f, LanternColor, LanternColor, 1.0f);
	UVaelCombatTextSubsystem::PostReaction(Player, LOCTEXT("Lured", "Gelockt"));
	UE_LOG(LogVael, Log, TEXT("'%s' lures '%s' with its false light"), *GetName(), *Player->GetName());
}

void AVaelKobold::Steal(AVaelCharacter* Player)
{
	UVaelMaterialBag* Bag = Player != nullptr ? Player->GetMaterialBag() : nullptr;
	if (Bag == nullptr)
	{
		return;
	}
	// Anything but potions
	TArray<FVaelMaterialStack> Stacks = Bag->GetStacks();
	Stacks.RemoveAll([](const FVaelMaterialStack& Stack) { return Stack.Material == nullptr || Stack.Count <= 0 || Stack.Material->IsPotion(); });
	if (Stacks.IsEmpty())
	{
		return;
	}
	const FVaelMaterialStack& Stack = Stacks[FMath::RandHelper(Stacks.Num())];
	const int32 Count = FMath::Min(GetData<UVaelKoboldData>()->StealCount, Stack.Count);
	if (!Bag->RemoveMaterial(Stack.Material, Count))
	{
		return;
	}
	Stolen.Add({ Player, Stack.Material.Get(), Count });
	UVaelCombatTextSubsystem::PostReaction(Player, FText::Format(LOCTEXT("KoboldSteals", "−{0} {1}"), FText::AsNumber(Count), Stack.Material->DisplayName));
	UE_LOG(LogVael, Log, TEXT("'%s' steals %d x %s and runs"), *GetName(), Count, *GetNameSafe(Stack.Material));
	bFleeing = true;
	FleeLeft = GetData<UVaelKoboldData>()->FleeSeconds;
}

void AVaelKobold::ReturnLoot()
{
	int32 Returned = 0;
	for (const FVaelStolenGoods& Goods : Stolen)
	{
		AVaelCharacter* Victim = Goods.Owner.Get();
		if (Victim != nullptr && Goods.Material.IsValid() && Victim->GetMaterialBag() != nullptr)
		{
			Victim->GetMaterialBag()->AddMaterial(Goods.Material.Get(), Goods.Count);
			Returned += Goods.Count;
		}
	}
	Stolen.Reset();
	if (Returned > 0)
	{
		UVaelNoticeSubsystem::Post(this, LOCTEXT("LootBack", "Beute zurück"),
			FText::Format(LOCTEXT("LootBackDetail", "Der Kobold lässt fallen, was er gestohlen hat: {0} Teile."), FText::AsNumber(Returned)), KoboldKristall, 3.5f);
	}
}

void AVaelKobold::CastIllusions()
{
	const UVaelKoboldData* Data = GetData<UVaelKoboldData>();
	UWorld* World = GetWorld();
	const FVector Side = FVector::CrossProduct(GetActorForwardVector(), FVector::UpVector).GetSafeNormal();
	for (int32 Index = 0; Index < Data->IllusionCount; ++Index)
	{
		const float Sign = Index % 2 == 0 ? 1.0f : -1.0f;
		const FVector Spot = GetActorLocation() + Side * Sign * FMath::FRandRange(160.0f, 260.0f) + GetActorForwardVector() * FMath::FRandRange(-80.0f, 80.0f)
			+ FVector(0.0f, 0.0f, 200.0f);
		FVector Ground;
		if (!AVaelCreature::FindGround(World, Spot, Ground))
		{
			continue;
		}
		const float Seconds = Data->IllusionSeconds;
		AVaelCreature::SpawnCreature(World, const_cast<UVaelKoboldData*>(Data), Ground, GetActorRotation(), nullptr,
			[Seconds](AVaelCreature* Creature)
			{
				if (AVaelKobold* Copy = Cast<AVaelKobold>(Creature))
				{
					Copy->MakeIllusion(Seconds);
				}
			});
		BuntePuffs(World, Ground + FVector(0.0f, 0.0f, 50.0f), 90.0f, 1.2f);
	}
	UE_LOG(LogVael, Log, TEXT("'%s' makes %d false copies of itself"), *GetName(), Data->IllusionCount);
}

void AVaelKobold::CastSign()
{
	const UVaelKoboldData* Data = GetData<UVaelKoboldData>();
	if (!Prey.IsValid())
	{
		return;
	}
	FVector Ground;
	if (AVaelCreature::FindGround(GetWorld(), Prey->GetActorLocation() + FVector(0.0f, 0.0f, 100.0f), Ground))
	{
		AVaelIrrzeichen::Place(this, Ground, Data->SignRadius, Data->SignSeconds, Data->SignConfusion);
	}
}

void AVaelKobold::Vanish()
{
	if (bIllusion || GetData<UVaelKoboldData>()->Rolle == EVaelKoboldRolle::Zauberer)
	{
		BuntePuffs(GetWorld(), GetActorLocation(), 80.0f, 1.0f);
	}
	else
	{
		AVaelPollenBurst::Spawn(GetWorld(), GetActorLocation(), 80.0f, FLinearColor(0.6f, 0.55f, 0.7f), KoboldKristall, 1.0f);
	}
	Destroy();
}

void AVaelKobold::Die()
{
	if (bIllusion)
	{
		// Only a false image: it bursts into dust
		Vanish();
		return;
	}
	ReturnLoot();
	bFleeing = false;
	Super::Die();
}

// ------------------------------------------------------------------ In the caves

UVaelNightCreatureData* UVaelKobolde::MaybeVariant(UVaelNightCreatureData* Base, float Chance)
{
	if (Base == nullptr || !Base->AssetName.StartsWith(TEXT("K_")) || !Base->AssetName.EndsWith(TEXT("_0")))
	{
		return Base;
	}
	// The looks K_<Body>_1 .. _4 that were built and imported (Tools/Nachtwesen/kobold_variante.py), found once per body
	static TMap<TWeakObjectPtr<UVaelNightCreatureData>, TArray<TObjectPtr<UVaelNightCreatureData>>> Looks;
	TArray<TObjectPtr<UVaelNightCreatureData>>* Found = Looks.Find(Base);
	if (Found == nullptr)
	{
		Found = &Looks.Add(Base);
		const FString Stem = Base->AssetName.LeftChop(1);
		for (int32 Index = 1; Index <= 4; ++Index)
		{
			const FString Name = Stem + FString::FromInt(Index);
			const FString Package = FString::Printf(TEXT("/Game/Vael/Kreaturen/%s/%s/SK_%s"), *Base->AssetRegion, *Name, *Name);
			if (FPackageName::DoesPackageExist(Package))
			{
				UVaelNightCreatureData* Look = NewObject<UVaelNightCreatureData>(GetTransientPackage(), Base->GetClass());
				Look->AssetName = Name;
				Look->AddToRoot();
				Found->Add(Look);
			}
		}
		UE_LOG(LogVael, Log, TEXT("Kobold %s: %d further looks"), *Base->AssetName, Found->Num());
	}
	if (Found->IsEmpty() || FMath::FRand() >= Chance)
	{
		return Base;
	}
	return (*Found)[FMath::RandHelper(Found->Num())].Get();
}

UVaelKoboldData* UVaelKobolde::GetData(EVaelKoboldRolle Rolle)
{
	// Four of five look different from the plain body
	return Cast<UVaelKoboldData>(MaybeVariant(GetBaseData(Rolle), 0.8f));
}

UVaelKoboldData* UVaelKobolde::GetBaseData(EVaelKoboldRolle Rolle)
{
	switch (Rolle)
	{
	case EVaelKoboldRolle::Dieb:	return GetMutableDefault<UVaelKoboldDiebData>();
	case EVaelKoboldRolle::Zauberer:	return GetMutableDefault<UVaelKoboldZaubererData>();
	case EVaelKoboldRolle::Treiber:		return GetMutableDefault<UVaelKoboldTreiberData>();
	case EVaelKoboldRolle::Sprengmeister:	return GetMutableDefault<UVaelKoboldSprengmeisterData>();
	case EVaelKoboldRolle::Laternentraeger:	return GetMutableDefault<UVaelKoboldLaternentraegerData>();
	default:						return GetMutableDefault<UVaelKoboldWaechterData>();
	}
}

void UVaelKobolde::SpawnInCaves(UWorld* World, TFunctionRef<bool(const ATargetPoint*)> Take, TArray<TWeakObjectPtr<AVaelCreature>>* Born)
{
	if (World == nullptr || !World->IsGameWorld() || !AVaelIndoorLevel::HasIndoorPlaces(World))
	{
		return;
	}
	// Who lives where (Boris 2026-11-22: Kobolde spread through the caves; their Stollen a secret and heavily guarded)
	struct FMix
	{
		float Chance;
		int32 MinCount;
		int32 MaxCount;
		TArray<TPair<EVaelKoboldRolle, float>> Roles;
	};
	const FMix Stollen{ 1.0f, 2, 3, { { EVaelKoboldRolle::Waechter, 0.3f }, { EVaelKoboldRolle::Dieb, 0.2f }, { EVaelKoboldRolle::Zauberer, 0.12f },
		{ EVaelKoboldRolle::Sprengmeister, 0.16f }, { EVaelKoboldRolle::Laternentraeger, 0.14f }, { EVaelKoboldRolle::Treiber, 0.08f } } };
	const FMix Ameisen{ 0.75f, 1, 2, { { EVaelKoboldRolle::Treiber, 0.4f }, { EVaelKoboldRolle::Dieb, 0.25f }, { EVaelKoboldRolle::Waechter, 0.15f },
		{ EVaelKoboldRolle::Sprengmeister, 0.1f }, { EVaelKoboldRolle::Laternentraeger, 0.1f } } };
	const FMix Streuner{ 0.22f, 1, 1, { { EVaelKoboldRolle::Dieb, 0.5f }, { EVaelKoboldRolle::Laternentraeger, 0.25f }, { EVaelKoboldRolle::Sprengmeister, 0.25f } } };
	const UVaelTropfseucheSettings* Seuche = UVaelTropfseucheSettings::Get();
	int32 Count = 0;
	for (TActorIterator<ATargetPoint> It(World); It; ++It)
	{
		if (!It->ActorHasTag(TEXT("VaelHoehleGegner")))
		{
			continue;
		}
		const FString Place = AVaelIndoorLevel::PlaceKeyAt(World, It->GetActorLocation());
		// The places of the Tropfseuche hosts stay theirs
		if (Seuche->HostLevels.ContainsByPredicate([&Place](const FString& Name) { return Place.Contains(Name); }) || !Take(*It))
		{
			continue;
		}
		const FMix& Mix = Place.Contains(TEXT("L_KoboldStollen")) ? Stollen : Place.Contains(TEXT("L_Ameisenbau")) ? Ameisen : Streuner;
		if (FMath::FRand() > Mix.Chance)
		{
			continue;
		}
		float Total = 0.0f;
		for (const TPair<EVaelKoboldRolle, float>& Entry : Mix.Roles)
		{
			Total += Entry.Value;
		}
		for (int32 Index = FMath::RandRange(Mix.MinCount, Mix.MaxCount); Index > 0; --Index)
		{
			float Roll = FMath::FRand() * Total;
			EVaelKoboldRolle Rolle = EVaelKoboldRolle::Dieb;
			for (const TPair<EVaelKoboldRolle, float>& Entry : Mix.Roles)
			{
				Rolle = Entry.Key;
				Roll -= Entry.Value;
				if (Roll <= 0.0f)
				{
					break;
				}
			}
			FVector Ground;
			if (AVaelCreature::FindGround(World, It->GetActorLocation() + FVector(FMath::FRandRange(-150.0f, 150.0f), FMath::FRandRange(-150.0f, 150.0f), 200.0f), Ground))
			{
				if (AVaelCreature* Kobold = AVaelCreature::SpawnCreature(World, GetData(Rolle), Ground, FRotator(0.0f, FMath::FRandRange(0.0f, 360.0f), 0.0f)))
				{
					++Count;
					if (Born != nullptr)
					{
						Born->Add(Kobold);
					}
				}
			}
		}
	}
	if (Count > 0)
	{
		UE_LOG(LogVael, Log, TEXT("Kobolde: %d living Kobolde in %s"), Count, *UWorld::RemovePIEPrefix(World->GetMapName()));
	}
}

#undef LOCTEXT_NAMESPACE
