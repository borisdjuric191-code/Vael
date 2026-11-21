// Copyright Epic Games, Inc. All Rights Reserved.

#include "Creatures/VaelSchleimgaenger.h"
#include "AbilitySystemComponent.h"
#include "Combat/VaelAttributeSet.h"
#include "Combat/VaelCombatStatics.h"
#include "Components/CapsuleComponent.h"
#include "Components/PointLightComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Components/SkeletalMeshComponent.h"
#include "Items/VaelMaterial.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Player/VaelCharacter.h"
#include "TimerManager.h"
#include "Vael.h"

#define LOCTEXT_NAMESPACE "VaelSchleimgaenger"

UVaelSchleimgaengerData::UVaelSchleimgaengerData()
{
	DisplayName = LOCTEXT("Schleimgaenger", "Schleimgänger");
	KindId = TEXT("Schleimgaenger");
	CompendiumId = TEXT("Schleimgaenger");
	CreatureClass = AVaelSchleimgaenger::StaticClass();
	AssetName = TEXT("Schleimgaenger");
	AssetRegion = TEXT("Eisenschlund");

	// Under the ground day and night are the same to it
	DayAwareness = 1.0f;
	NightAwareness = 1.0f;
	MaxHealth = 90.0f;
	MoveSpeed = 110.0f;
	CollisionRadius = 36.0f;
	CollisionHalfHeight = 90.0f;
	AggroRange = 1400.0f;
	WakeRange = 750.0f;
	KeepDistance = 110.0f;
	TurnRate = 150.0f;
	AttackPause = 0.9f;
	KnockbackMultiplier = 1.1f;
	// Bones held together by slime: water washes it out of the joints (weakness table, Eisenschlund round 4)
	ElementMultipliers.Add(EVaelElement::Water, 1.5f);

	IdleClip = TEXT("Stehen_Idle");
	MoveClip = TEXT("Gehen");
	MoveClipSpeed = 79.0f;
	LurkClip = TEXT("Liegen");
	WakeClip = TEXT("Aufstehen");
	HitClip = TEXT("Treffer");
	DeathClip = TEXT("Tod");

	FVaelNightAttack Blow;
	Blow.Clip = TEXT("Schlag");
	Blow.Range = FVector2D(0.0f, 170.0f);
	Blow.HitTime = 0.6f;
	Blow.Reach = 100.0f;
	Blow.HitRadius = 120.0f;
	Blow.Damage = 12.0f;
	Blow.Cooldown = 2.4f;
	Attacks.Add(Blow);

	// Both arms shoot forward, it lunges, grabs and holds a moment
	FVaelNightAttack Grab;
	Grab.Clip = TEXT("Packen");
	Grab.Range = FVector2D(120.0f, 330.0f);
	Grab.HitTime = 0.65f;
	Grab.Reach = 120.0f;
	Grab.HitRadius = 110.0f;
	Grab.Damage = 9.0f;
	Grab.StunSeconds = 0.7f;
	Grab.Lunge = 170.0f;
	Grab.Cooldown = 6.0f;
	Attacks.Add(Grab);

	// Cordyceps: the head, the neck and the shoulders are yanked now and then as if on a string
	LookBone = TEXT("head");
	LookMaxYaw = 60.0f;
	TwitchBones = { TEXT("head"), TEXT("neck_01"), TEXT("clavicle_l"), TEXT("clavicle_r"), TEXT("spine_03") };
	TwitchAngle = 22.0f;
}

AVaelSchleimgaenger::AVaelSchleimgaenger()
{
}

TSubclassOf<UVaelCreatureData> AVaelSchleimgaenger::GetDefaultDataClass() const
{
	return UVaelSchleimgaengerData::StaticClass();
}

void AVaelSchleimgaenger::BeginPlay()
{
	if (!bStrainSet)
	{
		Strain = UVaelTropfseuche::PickStrain(GetWorld(), GetActorLocation());
	}
	Super::BeginPlay();

	const FVaelSeuchenStamm& Settings = UVaelTropfseucheSettings::Get()->GetStrain(Strain);

	// Tougher, harder or faster by its strain
	if (UAbilitySystemComponent* AbilitySystem = GetAbilitySystemComponent())
	{
		const float Health = GetMaxHealth() * Settings.HostHealth;
		AbilitySystem->SetNumericAttributeBase(UVaelAttributeSet::GetMaxHealthAttribute(), Health);
		AbilitySystem->SetNumericAttributeBase(UVaelAttributeSet::GetHealthAttribute(), Health);
	}
	CustomTimeDilation = Settings.HostPace;

	Film = UVaelTropfseuche::MakeLook(this, Strain);
	// Debug: -VaelSeucheOhneFilm leaves the film off, to look at heart, web and lumps alone
	if (FParse::Param(FCommandLine::Get(), TEXT("VaelSeucheOhneFilm")))
	{
		Film = nullptr;
	}
	if (Film != nullptr)
	{
		GetMesh()->SetOverlayMaterial(Film);
	}
	// Lumps, web and heart are part of the model, they only take the colour of the strain
	const int32 SlimeSlot = GetMesh()->GetMaterialIndex(TEXT("Seuche"));
	const int32 HeartSlot = GetMesh()->GetMaterialIndex(TEXT("Herz"));
	if (SlimeSlot != INDEX_NONE)
	{
		Slime = UVaelTropfseuche::MakeLumpLook(this, Strain);
		GetMesh()->SetMaterial(SlimeSlot, Slime);
	}
	if (HeartSlot != INDEX_NONE)
	{
		Heart = UVaelTropfseuche::MakeLumpLook(this, Strain, true);
		GetMesh()->SetMaterial(HeartSlot, Heart);

		// A small light where the heart hangs (schleimgaenger_skelett.py puts it at about 142 cm, in the middle of the
		// rib cage), riding on spine_03 like the heart itself
		const USkeletalMesh* Model = GetMesh()->GetSkeletalMeshAsset();
		const int32 Chest = Model != nullptr ? Model->GetRefSkeleton().FindBoneIndex(TEXT("spine_03")) : INDEX_NONE;
		if (Chest != INDEX_NONE)
		{
			FTransform ChestPose = FTransform::Identity;
			for (int32 Bone = Chest; Bone != INDEX_NONE; Bone = Model->GetRefSkeleton().GetParentIndex(Bone))
			{
				ChestPose = ChestPose * Model->GetRefSkeleton().GetRefBonePose()[Bone];
			}
			HeartLight = NewObject<UPointLightComponent>(this);
			HeartLight->SetupAttachment(GetMesh(), TEXT("spine_03"));
			HeartLight->SetRelativeLocation(ChestPose.InverseTransformPosition(FVector(1.3f, 0.0f, 142.4f)));
			HeartLight->SetIntensityUnits(ELightUnits::Candelas);
			HeartLight->SetIntensity(6.0f);
			HeartLight->SetAttenuationRadius(90.0f);
			HeartLight->SetSourceRadius(4.0f);
			HeartLight->SetCastShadows(false);
			HeartLight->SetLightColor(Settings.Color);
			HeartLight->RegisterComponent();
		}
	}
	UE_LOG(LogVael, Log, TEXT("'%s' rises as a Schleimgaenger of the strain %s (risen %d times)"), *GetName(), *Settings.Name.ToString(), Rises);
}

float AVaelSchleimgaenger::GetOutgoingDamageMultiplier() const
{
	return Super::GetOutgoingDamageMultiplier() * UVaelTropfseucheSettings::Get()->GetStrain(Strain).HostDamage;
}

void AVaelSchleimgaenger::OnPreyHit(AVaelCharacter* Player, const FVaelNightAttack& Attack)
{
	const FVaelSeuchenStamm& Settings = UVaelTropfseucheSettings::Get()->GetStrain(Strain);
	UVaelTropfseuche::Infest(this, Player, Strain);
	if (Settings.HitStatus != EVaelStatus::None && Settings.HitStatusSeconds > 0.0f)
	{
		UVaelCombatStatics::ApplyStatus(this, Player, Settings.HitStatus, Settings.HitStatusSeconds);
	}
}

TArray<FVaelLootEntry> AVaelSchleimgaenger::GetLoot() const
{
	// A body only gives once, not every time the Seuche raises it
	return Rises > 0 ? TArray<FVaelLootEntry>() : Super::GetLoot();
}

void AVaelSchleimgaenger::Die()
{
	const bool bWet = UVaelCombatStatics::HasStatus(this, EVaelStatus::Wet);
	Super::Die();

	const UVaelTropfseucheSettings* Settings = UVaelTropfseucheSettings::Get();
	if (Rises >= Settings->MaxRises || bWet || UVaelTropfseuche::IsWaterAt(GetWorld(), GetActorLocation()))
	{
		bWashed = true;
		UE_LOG(LogVael, Log, TEXT("'%s' stays down (%s)"), *GetName(), Rises >= Settings->MaxRises ? TEXT("the Seuche is spent") : TEXT("water"));
		return;
	}
	// The film keeps glowing on the corpse and beats faster until it gets up: a warning to wash it
	bWaitingToRise = true;
	SetLifeSpan(0.0f);
	GetWorldTimerManager().SetTimer(RiseTimer, this, &AVaelSchleimgaenger::Rise, FMath::FRandRange(Settings->RiseDelay.X, Settings->RiseDelay.Y), false);
}

void AVaelSchleimgaenger::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	const FVaelSeuchenStamm& Settings = UVaelTropfseucheSettings::Get()->GetStrain(Strain);
	// The light of the heart beats with it, races while the corpse waits to rise and dies out when it is washed
	if (HeartLight != nullptr)
	{
		const float Left = bWaitingToRise ? GetWorldTimerManager().GetTimerRemaining(RiseTimer) : 10.0f;
		const float Period = FMath::Lerp(Settings.Pulse * 0.55f, 0.25f, FMath::Clamp(1.0f - Left / 4.0f, 0.0f, 1.0f));
		HeartBeat += DeltaSeconds / FMath::Max(Period, 0.1f);
		const float Beat = FMath::Pow(0.5f + 0.5f * FMath::Sin(HeartBeat * UE_TWO_PI), 3.0f);
		HeartLight->SetIntensity(6.0f * (0.45f + 0.9f * Beat) * (bWashed ? FilmShown : 1.0f));
	}
	if (Film == nullptr || !IsDead())
	{
		return;
	}
	if (bWaitingToRise)
	{
		// Water reaching the corpse while it waits puts the Seuche out
		if (UVaelTropfseuche::IsWaterAt(GetWorld(), GetActorLocation()))
		{
			bWaitingToRise = false;
			bWashed = true;
			GetWorldTimerManager().ClearTimer(RiseTimer);
			SetLifeSpan(3.0f);
			UE_LOG(LogVael, Log, TEXT("'%s': water washes the Seuche out of the body"), *GetName());
			return;
		}
		const float Left = GetWorldTimerManager().GetTimerRemaining(RiseTimer);
		const float Urge = FMath::Clamp(1.0f - Left / 4.0f, 0.0f, 1.0f);
		Film->SetScalarParameterValue(TEXT("Puls"), FMath::Lerp(Settings.Pulse, 0.35f, Urge));
		Film->SetScalarParameterValue(TEXT("Staerke"), Settings.Glow * 0.1f * (1.0f + 1.5f * Urge));
		// The heart in the ribs races before the body gets up
		if (Heart != nullptr)
		{
			Heart->SetScalarParameterValue(TEXT("Puls"), FMath::Lerp(Settings.Pulse * 0.55f, 0.25f, Urge));
			Heart->SetScalarParameterValue(TEXT("Staerke"), Settings.Glow * (2.0f + 2.0f * Urge));
		}
	}
	else if (bWashed)
	{
		// Washed: the film fades, the slime goes dark, the heart dies out
		FilmShown = FMath::FInterpTo(FilmShown, 0.0f, DeltaSeconds, 1.5f);
		Film->SetScalarParameterValue(TEXT("Sichtbar"), FilmShown);
		for (UMaterialInstanceDynamic* Look : { Slime.Get(), Heart.Get() })
		{
			if (Look != nullptr)
			{
				Look->SetScalarParameterValue(TEXT("Sichtbar"), FilmShown);
			}
		}
	}
}

void AVaelSchleimgaenger::Rise()
{
	bWaitingToRise = false;
	UWorld* World = GetWorld();
	const EVaelSeuchenStamm RiseStrain = Strain;
	const int32 RiseCount = Rises + 1;
	const FVector Ground = GetActorLocation() - FVector(0.0f, 0.0f, GetCapsuleComponent()->GetScaledCapsuleHalfHeight());
	// The same body as a new host: it lies as it fell and gets up when the living are near
	AVaelCreature* Risen = AVaelCreature::SpawnCreature(World, const_cast<UVaelCreatureData*>(GetData<UVaelCreatureData>()), Ground, GetActorRotation(), nullptr,
		[RiseStrain, RiseCount](AVaelCreature* Creature)
		{
			if (AVaelSchleimgaenger* Host = Cast<AVaelSchleimgaenger>(Creature))
			{
				Host->SetStrain(RiseStrain);
				Host->Rises = RiseCount;
			}
		});
	if (Risen == nullptr)
	{
		bWashed = true;
		SetLifeSpan(3.0f);
		return;
	}
	UE_LOG(LogVael, Log, TEXT("'%s' is raised again by the Seuche (%d)"), *GetName(), RiseCount);
	Destroy();
}

#undef LOCTEXT_NAMESPACE
