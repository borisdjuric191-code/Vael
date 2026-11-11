// Copyright Epic Games, Inc. All Rights Reserved.

#include "Creatures/VaelNightCreature.h"
#include "AbilitySystemComponent.h"
#include "Animation/AnimSequence.h"
#include "Combat/VaelAttributeSet.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Creatures/VaelSequenceAnimInstance.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Player/VaelCharacter.h"

#define LOCTEXT_NAMESPACE "VaelNightCreature"

namespace
{
	/** Seconds around the hit in which a reach chain or tongue is out: flung out before, drawn back after */
	constexpr float ReachOut = 0.14f;
	constexpr float ReachBack = 0.32f;

	/** A soft uneven wave -1..1 */
	float Drift(float Time, float Seed)
	{
		return 0.6f * FMath::Sin(Time * 0.9f + Seed) + 0.3f * FMath::Sin(Time * 2.3f + Seed * 2.7f) + 0.1f * FMath::Sin(Time * 5.1f + Seed * 4.3f);
	}
}

UVaelNightCreatureData::UVaelNightCreatureData()
{
	CreatureClass = AVaelNightCreature::StaticClass();
}

FSoftObjectPath UVaelNightCreatureData::GetAssetPath(const FString& Asset) const
{
	return FSoftObjectPath(FString::Printf(TEXT("/Game/Vael/Kreaturen/Wurzelforst/%s/%s.%s"), *AssetName, *Asset, *Asset));
}

UVaelModerglockeData::UVaelModerglockeData()
{
	DisplayName = LOCTEXT("Moderglocke", "Moderglocke");
	KindId = TEXT("Moderglocke");
	CompendiumId = TEXT("Moderglocke");
	AssetName = TEXT("Moderglocke");
	// By day a mushroom among mushrooms, at night it drifts for whatever breathes
	DayAwareness = 0.6f;
	NightAwareness = 1.3f;
	MaxHealth = 130.0f;
	MoveSpeed = 150.0f;
	CollisionRadius = 55.0f;
	CollisionHalfHeight = 135.0f;
	AggroRange = 1500.0f;
	WakeRange = 650.0f;
	KeepDistance = 230.0f;
	TurnRate = 70.0f;
	KnockbackMultiplier = 1.3f;
	ElementMultipliers.Add(EVaelElement::Fire, 1.5f);

	// Born in every size up to the grown one; awake it drifts well above the heads (Boris 2026-11-10)
	SizeRange = FVector2D(0.6f, 1.0f);
	FlightHeight = 75.0f;
	SprayBone = TEXT("hut");
	SprayRim = 62.0f;
	BurstSpray = EVaelNightSpray::Spores;

	IdleClip = TEXT("Schweben_Idle");
	MoveClip = TEXT("Schweben_Vor");
	// From afar and on its way home it walks: a long giant, heavy, the long arms swinging (Boris 2026-11-11)
	WalkClip = TEXT("Gehen");
	WalkClipSpeed = 89.0f;
	WalkSpeed = 95.0f;
	WalkDistance = 480.0f;
	MoveClipSpeed = 150.0f;
	LurkClip = TEXT("Lauern");
	WakeClip = TEXT("Erwachen");
	HitClip = TEXT("Treffer");
	BurstClip = TEXT("Sporenbersten");
	BurstChance = 0.35f;
	BurstRadius = 320.0f;
	BurstDamage = 6.0f;
	DeathClip = TEXT("Tod");

	FVaelNightAttack Sting;
	Sting.Clip = TEXT("Nesselfaeden");
	Sting.Range = FVector2D(0.0f, 260.0f);
	Sting.HitTime = 0.75f;
	Sting.Reach = 170.0f;
	Sting.HitRadius = 170.0f;
	Sting.Damage = 8.0f;
	Sting.StunSeconds = 2.4f;
	Sting.Cooldown = 5.0f;
	Attacks.Add(Sting);

	FVaelNightAttack Rain;
	Rain.Clip = TEXT("Giftregen");
	Rain.Range = FVector2D(200.0f, 520.0f);
	Rain.HitTime = 1.3f;
	Rain.Reach = 0.0f;
	Rain.HitRadius = 420.0f;
	Rain.Damage = 9.0f;
	Rain.Cooldown = 9.0f;
	Rain.bStayHigh = true;
	Rain.Spray = EVaelNightSpray::Venom;
	Attacks.Add(Rain);

	FVaelNightAttack Bell;
	Bell.Clip = TEXT("Glockenschlag");
	Bell.Range = FVector2D(0.0f, 420.0f);
	Bell.HitTime = 1.15f;
	Bell.Reach = 0.0f;
	Bell.HitRadius = 130.0f;
	Bell.Damage = 26.0f;
	Bell.StunSeconds = 1.3f;
	Bell.Cooldown = 7.0f;
	Bell.Lunge = 1.0f;
	Bell.bOnlyStunnedPrey = true;
	Attacks.Add(Bell);

	LookBone = TEXT("hals");
	LookMaxYaw = 35.0f;
	BreathBone = TEXT("brust");
	BreathSwell = 0.035f;
	BreathPeriod = 3.6f;
	for (int32 Filament = 1; Filament <= 12; ++Filament)
	{
		SwayBones.Add(*FString::Printf(TEXT("faden_%d_3"), Filament));
	}
	SwayAngle = 7.0f;
	GlowRest = 0.25f;
	GlowPulse = 0.5f;
	GlowPeriod = 3.0f;
	GlowAttack = 1.6f;
}

UVaelKahlschwingeData::UVaelKahlschwingeData()
{
	DisplayName = LOCTEXT("Kahlschwinge", "Kahlschwinge");
	KindId = TEXT("Kahlschwinge");
	CompendiumId = TEXT("Kahlschwinge");
	AssetName = TEXT("Kahlschwinge");
	// It hears a heartbeat: sharp ears by day and by night
	DayAwareness = 1.0f;
	NightAwareness = 1.2f;
	MaxHealth = 85.0f;
	MoveSpeed = 520.0f;
	CollisionRadius = 50.0f;
	CollisionHalfHeight = 95.0f;
	AggroRange = 1900.0f;
	WakeRange = 1100.0f;
	KeepDistance = 420.0f;
	TurnRate = 260.0f;
	AttackPause = 0.9f;

	// Circles high over its prey like a bird of prey, hangs still for a moment, then dives, clings on with its feet
	// and rakes with the claws of its hands (Boris 2026-11-10)
	SizeRange = FVector2D(0.65f, 1.0f);
	FlightHeight = 130.0f;
	CircleClip = TEXT("Kreisen");
	CircleRadius = 480.0f;
	CircleSeconds = FVector2D(2.6f, 4.6f);
	ObserveSeconds = 0.9f;

	IdleClip = TEXT("Flug_Idle");
	MoveClip = TEXT("Flug_Vor");
	MoveClipSpeed = 520.0f;
	LurkClip = TEXT("Lauern");
	WakeClip = TEXT("Abheben");
	HitClip = TEXT("Treffer");
	DeathClip = TEXT("Tod");

	FVaelNightAttack Dive;
	Dive.Clip = TEXT("Sturzflug");
	Dive.Range = FVector2D(350.0f, 900.0f);
	Dive.HitTime = 0.8f;
	Dive.Reach = 60.0f;
	Dive.HitRadius = 150.0f;
	Dive.Damage = 18.0f;
	Dive.Cooldown = 6.0f;
	Dive.Lunge = 1800.0f;
	Dive.ClingClip = TEXT("Kratzen");
	Dive.ClingSeconds = 2.6f;
	// one claw stroke of the animation every 0.45 s
	Dive.ClingInterval = 0.45f;
	Dive.ClingDamage = 4.5f;
	Attacks.Add(Dive);

	FVaelNightAttack Cut;
	Cut.Clip = TEXT("Fluegelschnitt");
	Cut.Range = FVector2D(0.0f, 230.0f);
	Cut.HitTime = 0.45f;
	Cut.Reach = 0.0f;
	Cut.HitRadius = 230.0f;
	Cut.Damage = 12.0f;
	Cut.Cooldown = 3.0f;
	Attacks.Add(Cut);

	FVaelNightAttack Scream;
	Scream.Clip = TEXT("Kreischen");
	Scream.Range = FVector2D(200.0f, 700.0f);
	Scream.HitTime = 0.55f;
	Scream.Reach = 0.0f;
	Scream.HitRadius = 650.0f;
	Scream.Damage = 3.0f;
	Scream.StunSeconds = 0.8f;
	Scream.Cooldown = 12.0f;
	Scream.bStayHigh = true;
	Attacks.Add(Scream);


	LookBone = TEXT("kopf");
	LookMaxYaw = 55.0f;
	BreathBone = TEXT("brust");
	BreathSwell = 0.03f;
	BreathPeriod = 1.8f;
	TwitchBones = { TEXT("ohr_l"), TEXT("ohr_r") };
	TwitchAngle = 16.0f;
	SwayBones = { TEXT("schwanz_3"), TEXT("schwanz_4") };
	SwayAngle = 8.0f;
}

UVaelPeitschenunkeData::UVaelPeitschenunkeData()
{
	DisplayName = LOCTEXT("Peitschenunke", "Peitschenunke");
	KindId = TEXT("Peitschenunke");
	CompendiumId = TEXT("Peitschenunke");
	AssetName = TEXT("Peitschenunke");
	MaxHealth = 110.0f;
	MoveSpeed = 260.0f;
	CollisionRadius = 70.0f;
	CollisionHalfHeight = 70.0f;
	AggroRange = 1500.0f;
	WakeRange = 800.0f;
	KeepDistance = 420.0f;
	TurnRate = 200.0f;
	ElementMultipliers.Add(EVaelElement::Fire, 1.25f);

	SizeRange = FVector2D(0.5f, 1.0f);

	IdleClip = TEXT("Hocken_Idle");
	MoveClip = TEXT("Kriechen");
	MoveClipSpeed = 260.0f;
	LurkClip = TEXT("Lauern");
	WakeClip = TEXT("Aufschrecken");
	HitClip = TEXT("Treffer");
	DeathClip = TEXT("Tod");

	FVaelNightAttack Lash;
	Lash.Clip = TEXT("Zungenschlag");
	Lash.Range = FVector2D(220.0f, 750.0f);
	Lash.HitTime = 0.55f;
	Lash.Damage = 12.0f;
	Lash.Cooldown = 3.5f;
	Lash.bTongue = true;
	Attacks.Add(Lash);

	FVaelNightAttack Leap;
	Leap.Clip = TEXT("Anspringen");
	Leap.Range = FVector2D(150.0f, 520.0f);
	Leap.HitTime = 0.6f;
	Leap.Reach = 40.0f;
	Leap.HitRadius = 140.0f;
	Leap.Damage = 16.0f;
	Leap.Cooldown = 7.0f;
	Leap.Lunge = 1400.0f;
	Attacks.Add(Leap);

	FVaelNightAttack Snap;
	Snap.Clip = TEXT("Schnappen");
	Snap.Range = FVector2D(0.0f, 160.0f);
	Snap.HitTime = 0.3f;
	Snap.Reach = 80.0f;
	Snap.HitRadius = 110.0f;
	Snap.Damage = 10.0f;
	Snap.Cooldown = 2.5f;
	Attacks.Add(Snap);

	LookBone = TEXT("kopf");
	LookMaxYaw = 28.0f;
	// The throat pumps all the time
	BreathBone = TEXT("kehle");
	BreathSwell = 0.3f;
	BreathPeriod = 1.3f;
	TongueBone = TEXT("zunge");
	TongueThickness = 10.0f;
}

UVaelSchnappwurzData::UVaelSchnappwurzData()
{
	DisplayName = LOCTEXT("Schnappwurz", "Schnappwurz");
	KindId = TEXT("Schnappwurz");
	CompendiumId = TEXT("Schnappwurz");
	AssetName = TEXT("Schnappwurz");
	MaxHealth = 140.0f;
	MoveSpeed = 0.0f;
	CollisionRadius = 60.0f;
	CollisionHalfHeight = 110.0f;
	AggroRange = 900.0f;
	WakeRange = 700.0f;
	TurnRate = 0.0f;
	bRooted = true;
	KnockbackMultiplier = 0.0f;
	PullMultiplier = 0.0f;
	ElementMultipliers.Add(EVaelElement::Fire, 1.5f);

	IdleClip = TEXT("Winden");
	LurkClip = TEXT("Lauern");
	WakeClip = TEXT("Aufrichten");
	HitClip = TEXT("Treffer");
	DeathClip = TEXT("Tod");

	FVaelNightAttack Snap;
	Snap.Clip = TEXT("Zuschnappen");
	Snap.Range = FVector2D(0.0f, 330.0f);
	Snap.HitTime = 0.5f;
	Snap.Damage = 15.0f;
	Snap.Cooldown = 3.0f;
	Snap.bReach = true;
	Attacks.Add(Snap);

	FVaelNightAttack Reach;
	Reach.Clip = TEXT("Nachgreifen");
	Reach.Range = FVector2D(300.0f, 650.0f);
	Reach.HitTime = 0.7f;
	Reach.Damage = 12.0f;
	Reach.StunSeconds = 0.8f;
	Reach.Cooldown = 6.0f;
	Reach.bReach = true;
	Attacks.Add(Reach);

	ReachChain = { TEXT("stiel_1_1"), TEXT("stiel_1_2"), TEXT("stiel_1_3"), TEXT("stiel_1_4"), TEXT("stiel_1_5"), TEXT("stiel_1_6"), TEXT("falle_1") };
	ReachUpperJaw = TEXT("kiefer_1_o");
	ReachLowerJaw = TEXT("kiefer_1_u");

	// Nectar spots in the traps glow to lure
	GlowRest = 0.4f;
	GlowPulse = 0.7f;
	GlowPeriod = 2.4f;
	GlowAttack = 2.0f;
}

AVaelNightCreature::AVaelNightCreature()
{
	GetMesh()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	GetMesh()->SetAnimationMode(EAnimationMode::AnimationBlueprint);
	GetMesh()->SetAnimInstanceClass(UVaelSequenceAnimInstance::StaticClass());
	GetMesh()->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::OnlyTickPoseWhenRendered;
}

TSubclassOf<UVaelCreatureData> AVaelNightCreature::GetDefaultDataClass() const
{
	// The base, so every night creature's data fits; a creature placed without data has no model and keeps the placeholder
	return UVaelNightCreatureData::StaticClass();
}

bool AVaelNightCreature::IsAmbusher() const
{
	return !GetData<UVaelNightCreatureData>()->LurkClip.IsNone();
}

void AVaelNightCreature::BeginPlay()
{
	Super::BeginPlay();

	const UVaelNightCreatureData* Data = GetData<UVaelNightCreatureData>();
	USkeletalMesh* Model = Cast<USkeletalMesh>(Data->GetAssetPath(TEXT("SK_") + Data->AssetName).TryLoad());
	if (Model == nullptr)
	{
		UE_LOG(LogTemp, Warning, TEXT("'%s': SK_%s is missing, it keeps its placeholder look"), *GetName(), *Data->AssetName);
		return;
	}
	// Each one its own size, never larger than the model; -VaelNachtwesenGroesse=<share> sets it for pictures
	SizeShare = FMath::FRandRange(Data->SizeRange.X, Data->SizeRange.Y);
	FParse::Value(FCommandLine::Get(), TEXT("-VaelNachtwesenGroesse="), SizeShare);
	Size = Data->BodyScale * SizeShare;
	if (!FMath::IsNearlyEqual(SizeShare, 1.0f))
	{
		const float OldHalfHeight = GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
		GetCapsuleComponent()->SetCapsuleSize(GetCapsuleComponent()->GetUnscaledCapsuleRadius() * SizeShare, GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight() * SizeShare);
		AddActorWorldOffset(FVector(0.0f, 0.0f, GetCapsuleComponent()->GetScaledCapsuleHalfHeight() - OldHalfHeight));

		// A small one has less in it
		if (UAbilitySystemComponent* AbilitySystem = GetAbilitySystemComponent())
		{
			const float Health = GetMaxHealth() * FMath::Lerp(0.35f, 1.0f, SizeShare);
			AbilitySystem->SetNumericAttributeBase(UVaelAttributeSet::GetMaxHealthAttribute(), Health);
			AbilitySystem->SetNumericAttributeBase(UVaelAttributeSet::GetHealthAttribute(), Health);
		}
	}

	GetBody()->SetVisibility(false);
	GetMesh()->SetSkeletalMesh(Model);
	GetMesh()->SetRelativeScale3D(FVector(Size));
	GetMesh()->SetRelativeLocationAndRotation(FVector(0.0f, 0.0f, -GetCapsuleComponent()->GetScaledCapsuleHalfHeight()), FRotator::ZeroRotator);
	GetMesh()->InitAnim(true);
	Animator = Cast<UVaelSequenceAnimInstance>(GetMesh()->GetAnimInstance());
	for (int32 Slot = 0; Slot < GetMesh()->GetNumMaterials(); ++Slot)
	{
		Materials.Add(GetMesh()->CreateDynamicMaterialInstance(Slot));
	}

	Seed = FMath::FRandRange(0.0f, 100.0f);
	AttackReadyTimes.Init(0.0f, Data->Attacks.Num());
	TwitchStarts.Init(-10.0f, Data->TwitchBones.Num());
	TwitchNext.SetNum(Data->TwitchBones.Num());
	for (float& Next : TwitchNext)
	{
		Next = FMath::FRandRange(0.5f, 4.0f);
	}
	if (UAnimSequence* Death = FindClip(Data->DeathClip))
	{
		CorpseDuration = Death->GetPlayLength() + 2.5f;
	}
	if (Data->bRooted)
	{
		GetCharacterMovement()->DisableMovement();
	}

	if (!Data->TongueBone.IsNone())
	{
		Tongue = NewObject<UStaticMeshComponent>(this);
		Tongue->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cylinder.Cylinder")));
		Tongue->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Tongue->SetCastShadow(false);
		Tongue->SetupAttachment(GetRootComponent());
		Tongue->RegisterComponent();
		Tongue->SetUsingAbsoluteLocation(true);
		Tongue->SetUsingAbsoluteRotation(true);
		Tongue->SetUsingAbsoluteScale(true);
		if (UMaterialInstanceDynamic* TongueMaterial = Tongue->CreateDynamicMaterialInstance(0))
		{
			TongueMaterial->SetVectorParameterValue(TEXT("Color"), Data->TongueColor);
		}
		Tongue->SetVisibility(false);
	}

	bShow = FParse::Param(FCommandLine::Get(), TEXT("VaelNachtwesenSchau"));
	EnterState(FindClip(Data->LurkClip) != nullptr ? EVaelNightState::Lurk : EVaelNightState::Fight);
}

void AVaelNightCreature::TickShow(float DeltaSeconds)
{
	const UVaelNightCreatureData* Data = GetData<UVaelNightCreatureData>();
	ShowLeft -= DeltaSeconds;
	if (ShowLeft > 0.0f)
	{
		return;
	}
	TArray<TPair<FName, bool>> List = { {Data->LurkClip, true}, {Data->WakeClip, false}, {Data->IdleClip, true}, {Data->MoveClip, true}, {Data->WalkClip, true}, {Data->CircleClip, true} };
	for (const FVaelNightAttack& Attack : Data->Attacks)
	{
		List.Emplace(Attack.Clip, false);
		List.Emplace(Attack.ClingClip, true);
	}
	List.Emplace(Data->HitClip, false);
	List.Emplace(Data->BurstClip, false);
	List.Emplace(Data->DeathClip, false);
	for (int32 Try = 0; Try < List.Num(); ++Try)
	{
		ShowIndex = (ShowIndex + 1) % List.Num();
		if (UAnimSequence* Sequence = FindClip(List[ShowIndex].Key))
		{
			State = List[ShowIndex].Key == Data->LurkClip ? EVaelNightState::Lurk : List[ShowIndex].Key == Data->WalkClip ? EVaelNightState::Fight : EVaelNightState::Flinch;
			bGrounded = List[ShowIndex].Key == Data->WalkClip;
			ShowLeft = List[ShowIndex].Value ? 3.0f : Sequence->GetPlayLength() + 0.4f;
			Play(List[ShowIndex].Key, List[ShowIndex].Value, 0.2f, 1.0f, true);
			UE_LOG(LogTemp, Display, TEXT("Nachtwesen-Schau: %s %s"), *Data->AssetName, *List[ShowIndex].Key.ToString());

			// What the animation throws out belongs to its look
			for (const FVaelNightAttack& Attack : Data->Attacks)
			{
				if (Attack.Clip == List[ShowIndex].Key && Attack.Spray != EVaelNightSpray::None)
				{
					SpawnSpray(Attack.Spray, Attack.HitRadius);
				}
			}
			if (List[ShowIndex].Key == Data->BurstClip)
			{
				SpawnSpray(Data->BurstSpray, Data->BurstRadius * 0.8f);
			}
			return;
		}
	}
}

UAnimSequence* AVaelNightCreature::FindClip(FName Clip)
{
	if (Clip.IsNone())
	{
		return nullptr;
	}
	if (const TObjectPtr<UAnimSequence>* Known = Clips.Find(Clip))
	{
		return Known->Get();
	}
	const UVaelNightCreatureData* Data = GetData<UVaelNightCreatureData>();
	UAnimSequence* Sequence = Cast<UAnimSequence>(Data->GetAssetPath(FString::Printf(TEXT("A_%s_%s"), *Data->AssetName, *Clip.ToString())).TryLoad());
	if (Sequence == nullptr)
	{
		UE_LOG(LogTemp, Warning, TEXT("'%s': animation A_%s_%s is missing"), *GetName(), *Data->AssetName, *Clip.ToString());
	}
	Clips.Add(Clip, Sequence);
	return Sequence;
}

void AVaelNightCreature::Play(FName Clip, bool bLoop, float BlendTime, float Rate, bool bRestart)
{
	UAnimSequence* Sequence = FindClip(Clip);
	if (Animator != nullptr && Sequence != nullptr)
	{
		Animator->Play(Sequence, bLoop, BlendTime, Rate, bRestart);
	}
}

void AVaelNightCreature::EnterState(EVaelNightState NewState)
{
	const UVaelNightCreatureData* Data = GetData<UVaelNightCreatureData>();
	State = NewState;
	StateTime = 0.0f;
	StateDuration = 0.0f;
	bMoving = false;
	bObserving = false;
	LoopClip = NAME_None;

	switch (NewState)
	{
	case EVaelNightState::Lurk:
		Play(Data->LurkClip, true, 0.4f);
		break;

	case EVaelNightState::Wake:
		if (UAnimSequence* Wake = FindClip(Data->WakeClip))
		{
			StateDuration = Wake->GetPlayLength();
			Play(Data->WakeClip, false, 0.15f, 1.0f, true);
		}
		break;

	case EVaelNightState::Fight:
		CircleLeft = FMath::FRandRange(Data->CircleSeconds.X, Data->CircleSeconds.Y);
		Loop(Data->IdleClip, 0.35f);
		break;
	default:
		break;
	}
}

void AVaelNightCreature::Loop(FName Clip, float BlendTime)
{
	if (Clip != LoopClip && FindClip(Clip) != nullptr)
	{
		LoopClip = Clip;
		Play(Clip, true, BlendTime);
	}
}

void AVaelNightCreature::SpawnSpray(EVaelNightSpray Kind, float Reach)
{
	const UVaelNightCreatureData* Data = GetData<UVaelNightCreatureData>();
	const FVector From = Data->SprayBone.IsNone() ? GetActorLocation() + FVector(0.0f, 0.0f, GetCapsuleComponent()->GetScaledCapsuleHalfHeight()) : GetMesh()->GetBoneLocation(Data->SprayBone);
	AVaelNightSpray::Spawn(GetWorld(), Kind, From, Data->SprayRim * Size, Reach, SizeShare);
}

void AVaelNightCreature::StartCling(AVaelCharacter* Held)
{
	const UVaelNightCreatureData* Data = GetData<UVaelNightCreatureData>();
	const int32 Attack = AttackIndex;
	EnterState(EVaelNightState::Cling);
	AttackIndex = Attack;
	Prey = Held;
	ClingSide = (GetActorLocation() - Held->GetActorLocation()).GetSafeNormal2D();
	if (ClingSide.IsNearlyZero())
	{
		ClingSide = Held->GetActorForwardVector();
	}
	// The first stroke of the animation lands at once, then one every ClingInterval
	ClingNextHit = 0.08f;
	// It hangs on the body of its prey: the two overlap, so they must not push each other apart
	PawnResponseBeforeCling = GetCapsuleComponent()->GetCollisionResponseToChannel(ECC_Pawn);
	GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
	Play(Data->Attacks[Attack].ClingClip, true, 0.12f, 1.0f, true);
}

void AVaelNightCreature::EndCling()
{
	GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Pawn, PawnResponseBeforeCling);
	// Lets go backwards, off its prey
	if (Prey.IsValid())
	{
		SetActorLocation(GetActorLocation() + ClingSide * (GetCapsuleComponent()->GetScaledCapsuleRadius() + Prey->GetSimpleCollisionRadius()), true);
	}
	NextAttackTime = GetWorld()->GetTimeSeconds() + GetData<UVaelNightCreatureData>()->AttackPause + 0.8f;
	EnterState(EVaelNightState::Fight);
}

void AVaelNightCreature::UpdateLift(float DeltaSeconds)
{
	const UVaelNightCreatureData* Data = GetData<UVaelNightCreatureData>();
	float Wanted = 0.0f;
	float Speed = 2.5f;
	if (IsDead())
	{
		Speed = 5.0f;
	}
	else if (State == EVaelNightState::Wake)
	{
		Wanted = Data->FlightHeight * FMath::Clamp(StateTime / FMath::Max(StateDuration, 0.1f), 0.0f, 1.0f);
		Speed = 6.0f;
	}
	else if (State == EVaelNightState::Attack && Data->Attacks.IsValidIndex(AttackIndex) && !Data->Attacks[AttackIndex].bStayHigh)
	{
		// Comes down so it is level with its prey at the hit, and climbs again once the blow is through
		const FVaelNightAttack& Attack = Data->Attacks[AttackIndex];
		const bool bDown = StateTime > Attack.HitTime * 0.3f && StateTime < Attack.HitTime + 0.25f;
		Wanted = bDown ? 0.0f : Data->FlightHeight;
		Speed = bDown ? 4.0f / FMath::Max(Attack.HitTime * 0.7f, 0.1f) : 3.0f;
	}
	else if (State == EVaelNightState::Cling)
	{
		Wanted = 0.0f;
		Speed = 8.0f;
	}
	else if (State != EVaelNightState::Lurk && !(bGrounded && State == EVaelNightState::Fight))
	{
		Wanted = Data->FlightHeight;
	}

	Lift = FMath::FInterpTo(Lift, Wanted * Size, DeltaSeconds, Speed);
	GetMesh()->SetRelativeLocation(FVector(0.0f, 0.0f, Lift - GetCapsuleComponent()->GetScaledCapsuleHalfHeight()));
}

void AVaelNightCreature::TurnTowards(const FVector& Location, float DeltaSeconds)
{
	const FVector To = (Location - GetActorLocation()).GetSafeNormal2D();
	if (!To.IsNearlyZero())
	{
		const float Step = GetData<UVaelNightCreatureData>()->TurnRate * DeltaSeconds;
		const float Delta = FMath::Clamp(FMath::FindDeltaAngleDegrees(GetActorRotation().Yaw, To.Rotation().Yaw), -Step, Step);
		SetActorRotation(FRotator(0.0f, GetActorRotation().Yaw + Delta, 0.0f));
	}
}

bool AVaelNightCreature::StartAttack(AActor* Target, float Distance, bool bPreferLunge)
{
	const UVaelNightCreatureData* Data = GetData<UVaelNightCreatureData>();
	const float Now = GetWorld()->GetTimeSeconds();
	if (Now < NextAttackTime || IsBlinded())
	{
		return false;
	}
	const AVaelCharacterBase* Victim = Cast<AVaelCharacterBase>(Target);
	const bool bPreyHelpless = Victim != nullptr && PreyHelplessUntil.Contains(Target) && Now < PreyHelplessUntil[Target];

	// The attack that needs helpless prey goes first when it can; else any that is ready and in range
	int32 Pick = INDEX_NONE;
	for (int32 Index = 0; Index < Data->Attacks.Num(); ++Index)
	{
		const FVaelNightAttack& Attack = Data->Attacks[Index];
		if (Now < AttackReadyTimes[Index] || Distance < Attack.Range.X || Distance > Attack.Range.Y || FindClip(Attack.Clip) == nullptr)
		{
			continue;
		}
		if (Attack.bOnlyStunnedPrey)
		{
			if (bPreyHelpless)
			{
				Pick = Index;
				break;
			}
			continue;
		}
		// Out of its hover it comes down on its prey when it can
		if (bPreferLunge && Attack.Lunge > 0.0f)
		{
			Pick = Index;
			break;
		}
		if (Pick == INDEX_NONE || FMath::RandBool())
		{
			Pick = Index;
		}
	}
	if (Pick == INDEX_NONE)
	{
		return false;
	}

	const FVaelNightAttack& Attack = Data->Attacks[Pick];
	EnterState(EVaelNightState::Attack);
	AttackIndex = Pick;
	bAttackLanded = false;
	bSprayed = false;
	bGrounded = false;
	Caught = nullptr;
	StateDuration = FindClip(Attack.Clip)->GetPlayLength();
	AttackReadyTimes[Pick] = Now + StateDuration + Attack.Cooldown;
	LungeDirection = (Target->GetActorLocation() - GetActorLocation()).GetSafeNormal2D();
	Play(Attack.Clip, false, 0.18f, 1.0f, true);
	return true;
}

void AVaelNightCreature::LandAttack()
{
	const UVaelNightCreatureData* Data = GetData<UVaelNightCreatureData>();
	const FVaelNightAttack& Attack = Data->Attacks[AttackIndex];
	const FVector Spot = GetActorLocation() + GetActorForwardVector() * (Attack.Reach * Size + (Attack.Reach > 0.0f ? GetCapsuleComponent()->GetScaledCapsuleRadius() : 0.0f));
	const float Power = FMath::Lerp(0.5f, 1.0f, SizeShare) * GetOutgoingDamageMultiplier();
	const float Now = GetWorld()->GetTimeSeconds();
	const bool bFar = Attack.bReach || Attack.bTongue;
	ForEachActivePlayer([&](AVaelCharacter* Player)
	{
		// What is flung out (a trap on its stalk, a tongue) hits the one it was flung at
		const bool bHit = bFar ? Player == Prey.Get() && GetDistanceTo2D(Player) <= Attack.Range.Y * 1.25f
			: FVector::Dist2D(Player->GetActorLocation(), Spot) <= Attack.HitRadius + Player->GetSimpleCollisionRadius();
		if (bHit && HitPlayer(Player, Attack.Damage * Power, Attack.Element))
		{
			if (Attack.StunSeconds > 0.0f)
			{
				Player->ApplyStun(Attack.StunSeconds);
				PreyHelplessUntil.Add(Player, Now + Attack.StunSeconds);
			}
			if (!Caught.IsValid() && FindClip(Attack.ClingClip) != nullptr && !Player->IsDodging())
			{
				Caught = Player;
			}
		}
	});
}

void AVaelNightCreature::TickBehavior(float DeltaSeconds)
{
	const UVaelNightCreatureData* Data = GetData<UVaelNightCreatureData>();
	if (bShow)
	{
		TickShow(DeltaSeconds);
		return;
	}
	StateTime += DeltaSeconds;
	const float Now = GetWorld()->GetTimeSeconds();

	float Distance = 0.0f;
	AActor* Target = FindTarget(Data->AggroRange * GetTimeOfDayAwareness(), &Distance);
	if (Target != nullptr)
	{
		Prey = Target;
		Distance = FMath::Max(0.0f, Distance - GetCapsuleComponent()->GetScaledCapsuleRadius() - Target->GetSimpleCollisionRadius());
	}

	switch (State)
	{
	case EVaelNightState::Lurk:
		if (Target != nullptr && Distance <= Data->WakeRange * GetTimeOfDayAwareness())
		{
			EnterState(EVaelNightState::Wake);
		}
		break;

	case EVaelNightState::Wake:
		if (Target != nullptr)
		{
			TurnTowards(Target->GetActorLocation(), DeltaSeconds * 0.5f);
		}
		if (StateTime >= StateDuration)
		{
			NextAttackTime = Now + 0.6f;
			EnterState(EVaelNightState::Fight);
		}
		break;

	case EVaelNightState::Fight:
	{
		if (Target == nullptr)
		{
			// Nobody about: drifts home
			const bool bHome = Data->bRooted || FVector::Dist2D(GetActorLocation(), HomeLocation) < 120.0f;
			const bool bStride = !bHome && FindClip(Data->WalkClip) != nullptr;
			bGrounded = bStride;
			if (!bHome)
			{
				TurnTowards(HomeLocation, DeltaSeconds);
				MoveInDirection(GetActorForwardVector(), bStride ? Data->WalkSpeed : Data->MoveSpeed * 0.6f);
			}
			bMoving = !bHome;
			Loop(bStride ? Data->WalkClip : bMoving && !Data->MoveClip.IsNone() ? Data->MoveClip : Data->IdleClip, 0.5f);
			if (bStride && Animator != nullptr)
			{
				Animator->SetRate(FMath::Clamp(GetVelocity().Size2D() / FMath::Max(Data->WalkClipSpeed * Size, 1.0f), 0.5f, 1.4f));
			}
			break;
		}
		const float Facing = Data->bRooted ? 0.0f : FMath::Abs(FMath::FindDeltaAngleDegrees(GetActorRotation().Yaw, (Target->GetActorLocation() - GetActorLocation()).Rotation().Yaw));
		const float Pace = Data->MoveSpeed * FMath::Lerp(1.25f, 1.0f, SizeShare);

		if (FindClip(Data->CircleClip) != nullptr && Distance <= Data->CircleRadius * 1.7f)
		{
			if (bObserving)
			{
				// Hangs in the air and stares at its prey for a moment, then comes down on it
				TurnTowards(Target->GetActorLocation(), DeltaSeconds);
				Loop(Data->IdleClip, 0.35f);
				ObserveLeft -= DeltaSeconds;
				if (ObserveLeft <= 0.0f && Facing < 25.0f)
				{
					bObserving = false;
					CircleLeft = FMath::FRandRange(Data->CircleSeconds.X, Data->CircleSeconds.Y) * 0.6f;
					if (StartAttack(Target, Distance, true))
					{
						break;
					}
				}
				break;
			}

			// Prey that walks in under it gets the wings
			if (Distance < Data->CircleRadius * 0.45f)
			{
				TurnTowards(Target->GetActorLocation(), DeltaSeconds);
				if (Facing < 35.0f && StartAttack(Target, Distance))
				{
					break;
				}
			}

			CircleLeft -= DeltaSeconds;
			if (CircleLeft <= 0.0f)
			{
				bObserving = true;
				ObserveLeft = Data->ObserveSeconds;
				break;
			}

			// Round its prey with it on the inner (left) wing, as the animation banks, holding the radius
			const FVector Out = (GetActorLocation() - Target->GetActorLocation()).GetSafeNormal2D();
			const FVector Along(Out.Y, -Out.X, 0.0f);
			const float Off = FMath::Clamp((Data->CircleRadius - GetDistanceTo2D(Target)) / 200.0f, -1.0f, 1.0f);
			const FVector Direction = (Along + Out * Off).GetSafeNormal2D();
			TurnTowards(GetActorLocation() + Direction * 300.0f, DeltaSeconds);
			MoveInDirection(GetActorForwardVector(), Pace * 0.75f);
			Loop(Data->CircleClip, 0.5f);
			break;
		}

		TurnTowards(Target->GetActorLocation(), DeltaSeconds);
		if (Facing < 25.0f && StartAttack(Target, Distance))
		{
			break;
		}
		const bool bWalk = !Data->bRooted && Distance > Data->KeepDistance && Facing < 70.0f;

		// From afar it strides over the ground; near its prey it rises and drifts
		if (FindClip(Data->WalkClip) != nullptr)
		{
			bGrounded = bWalk && (Distance > Data->WalkDistance || (bGrounded && Distance > Data->WalkDistance * 0.7f));
		}
		const bool bStride = bGrounded && bWalk;
		if (bWalk)
		{
			MoveInDirection(GetActorForwardVector(), bStride ? Data->WalkSpeed * FMath::Lerp(1.15f, 1.0f, SizeShare) : Pace);
		}
		bMoving = bWalk;
		Loop(bStride ? Data->WalkClip : bMoving && !Data->MoveClip.IsNone() ? Data->MoveClip : Data->IdleClip, bStride ? 0.6f : 0.45f);
		if (bMoving && Animator != nullptr)
		{
			Animator->SetRate(FMath::Clamp(GetVelocity().Size2D() / FMath::Max((bStride ? Data->WalkClipSpeed : Data->MoveClipSpeed) * Size, 1.0f), 0.5f, 1.6f));
		}
		break;
	}
	case EVaelNightState::Attack:
	{
		const FVaelNightAttack& Attack = Data->Attacks[AttackIndex];
		if (!bAttackLanded)
		{
			if (Prey.IsValid())
			{
				TurnTowards(Prey->GetActorLocation(), DeltaSeconds * 0.6f);
			}
			if (Attack.Lunge > 0.0f && !Data->bRooted && Prey.IsValid() && StateTime > Attack.HitTime * 0.35f)
			{
				// Rushes in so it arrives with the hit
				const float Left = FMath::Max(Attack.HitTime - StateTime, 0.05f);
				const float Gap = FMath::Max(GetDistanceTo2D(Prey.Get()) - Attack.Reach * Size, 0.0f);
				MoveInDirection((Prey->GetActorLocation() - GetActorLocation()).GetSafeNormal2D(), FMath::Min(Gap / Left, FMath::Max(Attack.Lunge, Data->MoveSpeed * 4.0f)));
			}
			if (!bSprayed && Attack.Spray != EVaelNightSpray::None && StateTime >= Attack.HitTime - 0.8f)
			{
				bSprayed = true;
				SpawnSpray(Attack.Spray, Attack.HitRadius);
			}
			if (StateTime >= Attack.HitTime)
			{
				bAttackLanded = true;
				LandAttack();
				if (Caught.IsValid())
				{
					StartCling(Caught.Get());
					break;
				}
			}
		}
		if (StateTime >= StateDuration)
		{
			NextAttackTime = Now + Data->AttackPause;
			EnterState(EVaelNightState::Fight);
		}
		break;
	}

	case EVaelNightState::Flinch:
		if (StateTime >= StateDuration)
		{
			EnterState(EVaelNightState::Fight);
		}
		break;
	case EVaelNightState::Cling:
	{
		// Hangs on its prey's front and goes where it goes; a dodge roll throws it off
		AVaelCharacter* Held = Cast<AVaelCharacter>(Prey.Get());
		const FVaelNightAttack& Attack = Data->Attacks[AttackIndex];
		if (Held == nullptr || Held->IsDefeated() || Held->IsDodging() || StateTime >= Attack.ClingSeconds)
		{
			EndCling();
			break;
		}
		// Chest to chest: its legs reach round the body of its prey
		const float Apart = Held->GetSimpleCollisionRadius() + 22.0f * Size;
		const float Feet = Held->GetActorLocation().Z - Held->GetSimpleCollisionHalfHeight();
		SetActorLocation(FVector(Held->GetActorLocation().X, Held->GetActorLocation().Y, Feet + GetCapsuleComponent()->GetScaledCapsuleHalfHeight()) + ClingSide * Apart,
			false, nullptr, ETeleportType::TeleportPhysics);
		SetActorRotation((-ClingSide).Rotation());
		GetCharacterMovement()->StopMovementImmediately();
		if (StateTime >= ClingNextHit)
		{
			ClingNextHit += Attack.ClingInterval;
			HitPlayer(Held, Attack.ClingDamage * FMath::Lerp(0.5f, 1.0f, SizeShare) * GetOutgoingDamageMultiplier(), Attack.Element);
		}
		break;
	}
	}
}

void AVaelNightCreature::OnDamageTaken(float Damage, const FGameplayTagContainer& DamageTags)
{
	Super::OnDamageTaken(Damage, DamageTags);
	if (IsDead())
	{
		return;
	}
	const UVaelNightCreatureData* Data = GetData<UVaelNightCreatureData>();
	FlashGlow = 1.0f;
	if (State == EVaelNightState::Lurk)
	{
		EnterState(EVaelNightState::Wake);
		return;
	}
	if (State == EVaelNightState::Cling)
	{
		EndCling();
	}
	if (State != EVaelNightState::Fight)
	{
		return;
	}

	const bool bBurst = FindClip(Data->BurstClip) != nullptr && FMath::FRand() < Data->BurstChance;
	const FName Clip = bBurst ? Data->BurstClip : Data->HitClip;
	if (UAnimSequence* Sequence = FindClip(Clip))
	{
		EnterState(EVaelNightState::Flinch);
		StateDuration = Sequence->GetPlayLength();
		Play(Clip, false, 0.08f, 1.0f, true);
		if (bBurst)
		{
			SpawnSpray(Data->BurstSpray, Data->BurstRadius * 0.8f);
			ForEachActivePlayer([&](AVaelCharacter* Player)
			{
				if (GetDistanceTo2D(Player) <= Data->BurstRadius)
				{
					HitPlayer(Player, Data->BurstDamage * FMath::Lerp(0.5f, 1.0f, SizeShare) * GetOutgoingDamageMultiplier(), EVaelElement::Earth);
				}
			});
		}
	}
}

void AVaelNightCreature::OnBodyColorShown(const FLinearColor& Color, bool bHitFlash)
{
	for (UMaterialInstanceDynamic* Material : Materials)
	{
		if (Material != nullptr)
		{
			Material->SetScalarParameterValue(TEXT("MarkStaerke"), bHitFlash ? 2.5f : IsMarked() ? 1.2f : 0.0f);
			Material->SetVectorParameterValue(TEXT("MarkFarbe"), bHitFlash ? FLinearColor(1.0f, 0.92f, 0.8f) : FLinearColor(0.55f, 0.12f, 1.0f));
		}
	}
}

void AVaelNightCreature::Die()
{
	if (State == EVaelNightState::Cling)
	{
		GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Pawn, PawnResponseBeforeCling);
	}
	Play(GetData<UVaelNightCreatureData>()->DeathClip, false, 0.15f, 1.0f, true);
	if (Animator != nullptr)
	{
		Animator->SetBoneTurns(TArray<FName>(), TArray<FQuat>());
		Animator->SetReachChains(TArray<FVaelReachChain>());
	}
	if (Tongue != nullptr)
	{
		Tongue->SetVisibility(false);
	}
	Super::Die();
}

void AVaelNightCreature::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (Animator != nullptr)
	{
		UpdateLift(DeltaSeconds);
	}
	if (Animator != nullptr && !IsDead())
	{
		UpdateProcedural(DeltaSeconds);
	}
	else if (IsDead())
	{
		Glow = FMath::FInterpTo(Glow, 0.0f, DeltaSeconds, 1.5f);
		for (UMaterialInstanceDynamic* Material : Materials)
		{
			if (Material != nullptr)
			{
				Material->SetScalarParameterValue(TEXT("GlutStaerke"), Glow);
			}
		}
	}
}

void AVaelNightCreature::UpdateProcedural(float DeltaSeconds)
{
	const UVaelNightCreatureData* Data = GetData<UVaelNightCreatureData>();
	const float Now = GetWorld()->GetTimeSeconds();
	const float Time = Now + Seed;
	const bool bAwake = State != EVaelNightState::Lurk;
	const bool bAttacking = State == EVaelNightState::Attack;

	TArray<FName> Bones;
	TArray<FQuat> Turns;
	TArray<FVector> Scales;
	const auto Add = [&](FName Bone, const FQuat& Turn, const FVector& Scale = FVector::OneVector)
	{
		Bones.Add(Bone);
		Turns.Add(Turn);
		Scales.Add(Scale);
	};

	// Looks at its prey: the head turns ahead of the body, never still
	float WantedYaw = 0.0f;
	float WantedPitch = 0.0f;
	if (bAwake && Prey.IsValid() && !bAttacking)
	{
		const FVector Local = GetActorTransform().InverseTransformPositionNoScale(Prey->GetActorLocation());
		WantedYaw = FMath::Clamp(FMath::RadiansToDegrees(FMath::Atan2(Local.Y, Local.X)), -Data->LookMaxYaw, Data->LookMaxYaw);
		WantedPitch = FMath::Clamp(FMath::RadiansToDegrees(FMath::Atan2(Local.Z, FMath::Max(Local.Size2D(), 1.0f))), -20.0f, 20.0f);
	}
	WantedYaw += bAwake ? 4.0f * Drift(Time * 0.7f, 3.0f) : 0.0f;
	LookYaw = FMath::FInterpTo(LookYaw, WantedYaw, DeltaSeconds, 4.0f);
	LookPitch = FMath::FInterpTo(LookPitch, WantedPitch, DeltaSeconds, 4.0f);
	if (!Data->LookBone.IsNone())
	{
		Add(Data->LookBone, FQuat(FVector::UpVector, FMath::DegreesToRadians(LookYaw)) * FQuat(FVector::RightVector, FMath::DegreesToRadians(-LookPitch)));
	}

	// Breath: the chest swells, quicker while it fights
	if (!Data->BreathBone.IsNone())
	{
		const float Period = Data->BreathPeriod * (bAwake && Prey.IsValid() ? 0.6f : 1.0f);
		const float Breath = 0.5f + 0.5f * FMath::Sin(2.0f * PI * Time / Period);
		Add(Data->BreathBone, FQuat::Identity, FVector(1.0f + Data->BreathSwell * Breath));
	}

	// Ears: quick single twitches at uneven times, each ear for itself
	for (int32 Index = 0; Index < Data->TwitchBones.Num(); ++Index)
	{
		if (Now >= TwitchNext[Index])
		{
			TwitchStarts[Index] = Now;
			TwitchNext[Index] = Now + FMath::FRandRange(0.4f, bAwake ? 2.6f : 6.0f);
		}
		const float Since = Now - TwitchStarts[Index];
		const float Kick = Since < 0.32f ? FMath::Sin(PI * Since / 0.32f) * FMath::Sin(PI * Since / 0.32f) : 0.0f;
		const float Side = Index % 2 == 0 ? 1.0f : -1.0f;
		Add(Data->TwitchBones[Index], FQuat(FVector::ForwardVector, FMath::DegreesToRadians(Side * Data->TwitchAngle * Kick))
			* FQuat(FVector::UpVector, FMath::DegreesToRadians(Side * 0.6f * Data->TwitchAngle * Kick + 2.0f * Drift(Time, 10.0f + Index))));
	}

	// Loose ends drift
	for (int32 Index = 0; Index < Data->SwayBones.Num(); ++Index)
	{
		Add(Data->SwayBones[Index], FQuat(FVector::ForwardVector, FMath::DegreesToRadians(Data->SwayAngle * Drift(Time * 1.3f, 20.0f + Index * 1.7f)))
			* FQuat(FVector::RightVector, FMath::DegreesToRadians(Data->SwayAngle * Drift(Time * 1.1f, 50.0f + Index * 2.9f))));
	}
	Animator->SetBoneTurns(Bones, Turns, Scales);

	// What is flung out at the prey around the hit
	float Out = 0.0f;
	const FVaelNightAttack* Attack = bAttacking ? &Data->Attacks[AttackIndex] : nullptr;
	if (Attack != nullptr && (Attack->bReach || Attack->bTongue))
	{
		const float Since = StateTime - Attack->HitTime;
		Out = Since < 0.0f ? FMath::Clamp(1.0f + Since / ReachOut, 0.0f, 1.0f) : FMath::Clamp(1.0f - Since / ReachBack, 0.0f, 1.0f);
		Out = FMath::SmoothStep(0.0f, 1.0f, Out);
	}
	const FVector PreyPoint = Prey.IsValid() ? Prey->GetActorLocation() : GetActorLocation() + GetActorForwardVector() * 200.0f;
	TArray<FVaelReachChain> Chains;
	if (Attack != nullptr && Attack->bReach && Out > 0.0f && Data->ReachChain.Num() >= 2)
	{
		FVaelReachChain& Chain = Chains.AddDefaulted_GetRef();
		Chain.Bones = Data->ReachChain;
		Chain.Target = GetMesh()->GetComponentTransform().InverseTransformPosition(PreyPoint);
		Chain.Weight = Out;
		Chain.Arc = 40.0f;
		Chain.UpperJaw = Data->ReachUpperJaw;
		Chain.LowerJaw = Data->ReachLowerJaw;
		Chain.JawOpen = StateTime < Attack->HitTime ? 35.0f * Out : -10.0f;
	}
	Animator->SetReachChains(Chains);
	UpdateTongue(Attack != nullptr && Attack->bTongue ? Out : 0.0f, PreyPoint);

	// Glow: a slow pulse like a heartbeat, flaring with attacks and hits
	FlashGlow = FMath::FInterpTo(FlashGlow, 0.0f, DeltaSeconds, 5.0f);
	const float Beat = FMath::Pow(0.5f + 0.5f * FMath::Sin(2.0f * PI * Time / Data->GlowPeriod), 2.0f);
	const float Wanted = (bAwake ? Data->GlowRest + Data->GlowPulse * Beat : Data->GlowRest * 0.3f)
		+ (bAttacking ? Data->GlowAttack * FMath::Clamp(StateTime / FMath::Max(Data->Attacks[AttackIndex].HitTime, 0.1f), 0.0f, 1.0f) : 0.0f) + FlashGlow * Data->GlowAttack * 0.5f;
	Glow = FMath::FInterpTo(Glow, Wanted, DeltaSeconds, 6.0f);
	for (UMaterialInstanceDynamic* Material : Materials)
	{
		if (Material != nullptr)
		{
			Material->SetScalarParameterValue(TEXT("GlutStaerke"), Glow);
		}
	}
}

void AVaelNightCreature::UpdateTongue(float Shown, const FVector& TargetLocation)
{
	if (Tongue == nullptr)
	{
		return;
	}
	if (Shown <= 0.01f)
	{
		Tongue->SetVisibility(false);
		return;
	}
	const UVaelNightCreatureData* Data = GetData<UVaelNightCreatureData>();
	const FVector From = GetMesh()->GetBoneLocation(Data->TongueBone);
	const FVector To = FMath::Lerp(From, TargetLocation, Shown);
	const float Length = FMath::Max(FVector::Dist(From, To), 1.0f);
	// The engine cylinder is 100 cm high along Z and 100 cm thick
	Tongue->SetWorldLocationAndRotation((From + To) * 0.5f, FRotationMatrix::MakeFromZ(To - From).Rotator());
	Tongue->SetWorldScale3D(FVector(Data->TongueThickness / 100.0f, Data->TongueThickness / 100.0f, Length / 100.0f));
	Tongue->SetVisibility(true);
}

#undef LOCTEXT_NAMESPACE
