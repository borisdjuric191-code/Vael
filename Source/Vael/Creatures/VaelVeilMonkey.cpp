// Copyright Epic Games, Inc. All Rights Reserved.

#include "Creatures/VaelVeilMonkey.h"
#include "AnimationRuntime.h"
#include "Components/CapsuleComponent.h"
#include "Components/PoseableMeshComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Creatures/VaelCreatureData.h"
#include "Engine/OverlapResult.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Items/VaelMaterial.h"
#include "Items/VaelMaterialBag.h"
#include "Magic/VaelGameplayTags.h"
#include "Magic/VaelSpellEffects.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Player/VaelCharacter.h"
#include "UI/VaelCombatTextSubsystem.h"
#include "UI/VaelNoticeSubsystem.h"
#include "Vael.h"
#include "World/VaelFireLight.h"
#include "World/VaelRegion.h"

#define LOCTEXT_NAMESPACE "VaelCreatures"

namespace
{
	/** Seconds between two looks for cover, and between two checks whether it shows itself */
	constexpr float MonkeyCoverInterval = 0.4f;
	constexpr float MonkeyPresenceInterval = 2.0f;

	/** It sits still closer than this to its hiding place, in cm */
	constexpr float MonkeyArriveDistance = 45.0f;

	/** Cover lower than this doesn't hide it, in cm */
	constexpr float MonkeyCoverMinHeight = 60.0f;

	/** Without cover it keeps this far from the players, in cm */
	constexpr float MonkeyOpenDistance = 950.0f;

	/** Diameter of the engine sphere in cm */
	constexpr float MonkeyShapeSize = 100.0f;

	/** Rags of the hood, the eyes in healthy and in corrupted land, a part flashing under a hit */
	const FLinearColor MonkeyRagColor(0.09f, 0.085f, 0.08f);
	const FLinearColor MonkeyEyeColor(1.0f, 0.95f, 0.55f);
	const FLinearColor MonkeyWildEyeColor(0.75f, 0.3f, 1.0f);
	const FLinearColor MonkeyFlashColor(1.0f, 0.9f, 0.75f);
}

AVaelVeilMonkey::AVaelVeilMonkey()
{
}

TSubclassOf<UVaelCreatureData> AVaelVeilMonkey::GetDefaultDataClass() const
{
	return UVaelVeilMonkeyData::StaticClass();
}

void AVaelVeilMonkey::BeginPlay()
{
	Super::BeginPlay();

	Shyness = FMath::FRand();
	PounceCooldown = RandomInRange(GetData<UVaelVeilMonkeyData>()->PounceInterval) * 0.4f;
	HidePoint = GetActorLocation();

	BuildRig();
}

float AVaelVeilMonkey::GetAggression() const
{
	const UVaelVeilMonkeyData* Data = GetData<UVaelVeilMonkeyData>();
	const AVaelRegion* Region = AVaelRegion::GetRegionAt(GetWorld(), GetActorLocation());
	const float Corruption = Region != nullptr ? Region->GetCorruption() : 0.0f;

	return FMath::Clamp((Corruption - Data->CalmCorruption) / FMath::Max(Data->WildCorruption - Data->CalmCorruption, 1.0f), 0.0f, 1.0f);
}

int32 AVaelVeilMonkey::GetNumStolen() const
{
	int32 Num = 0;
	for (const FVaelStolen& Stolen : Loot)
	{
		Num += Stolen.Count;
	}
	return Num;
}

float AVaelVeilMonkey::GetIncomingDamageMultiplier(const FGameplayTagContainer& DamageTags) const
{
	const float Multiplier = Super::GetIncomingDamageMultiplier(DamageTags);
	return State == EVaelMonkeyState::Blinded ? Multiplier * GetData<UVaelVeilMonkeyData>()->BlindedDamageMultiplier : Multiplier;
}

void AVaelVeilMonkey::EnterState(EVaelMonkeyState NewState)
{
	if (State == EVaelMonkeyState::Cling && NewState != EVaelMonkeyState::Cling)
	{
		LetGo();
	}

	State = NewState;
	StateTime = 0.0f;
	CoverCountdown = 0.0f;
}

void AVaelVeilMonkey::SetDormant(bool bDormant)
{
	SetActorHiddenInGame(bDormant);
	GetCapsuleComponent()->SetCollisionEnabled(bDormant ? ECollisionEnabled::NoCollision : ECollisionEnabled::QueryAndPhysics);
	GetCharacterMovement()->SetMovementMode(bDormant ? MOVE_None : MOVE_Walking);

	EnterState(bDormant ? EVaelMonkeyState::Dormant : EVaelMonkeyState::Hide);
}

AVaelCharacter* AVaelVeilMonkey::FindPointedPlayer() const
{
	const float Range = GetData<UVaelVeilMonkeyData>()->AggroRange;

	// The aye-aye points at whoever is closest to death
	AVaelCharacter* Weakest = nullptr;
	float LeastLife = UE_BIG_NUMBER;

	ForEachActivePlayer([this, Range, &Weakest, &LeastLife](AVaelCharacter* Player)
	{
		const float Life = Player->GetHealth() / FMath::Max(Player->GetMaxHealth(), 1.0f);
		if (GetDistanceTo2D(Player) < Range && Life < LeastLife)
		{
			Weakest = Player;
			LeastLife = Life;
		}
	});

	return Weakest;
}

AVaelCharacter* AVaelVeilMonkey::FindNearestPlayer(float& OutDistance) const
{
	AVaelCharacter* Nearest = nullptr;
	OutDistance = UE_BIG_NUMBER;

	ForEachActivePlayer([this, &Nearest, &OutDistance](AVaelCharacter* Player)
	{
		const float Distance = GetDistanceTo2D(Player);
		if (Distance < OutDistance)
		{
			Nearest = Player;
			OutDistance = Distance;
		}
	});

	return Nearest;
}

bool AVaelVeilMonkey::FindCover(const AVaelCharacter* From, float MinDistanceToPlayer, FVector& OutHidePoint) const
{
	const UVaelVeilMonkeyData* Data = GetData<UVaelVeilMonkeyData>();
	const FVector Own = GetActorLocation();
	const FVector Seen = From->GetActorLocation();
	const float OwnRadius = GetCapsuleComponent()->GetScaledCapsuleRadius();

	TArray<FOverlapResult> Overlaps;
	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(VaelMonkeyCover), false, this);
	GetWorld()->OverlapMultiByObjectType(Overlaps, Own, FQuat::Identity, FCollisionObjectQueryParams(ECC_WorldStatic), FCollisionShape::MakeSphere(Data->CoverSearchRadius), QueryParams);

	bool bFound = false;
	float BestScore = UE_BIG_NUMBER;

	for (const FOverlapResult& Overlap : Overlaps)
	{
		const UPrimitiveComponent* Component = Overlap.GetComponent();
		if (Component == nullptr || !Component->IsVisible() || Component->GetCollisionResponseToChannel(ECC_Pawn) != ECR_Block)
		{
			continue;
		}

		// Rocks, trunks, crates and pieces of wall; not the ground and not whole cliffs
		const FBoxSphereBounds& Bounds = Component->Bounds;
		const float HalfWidth = FMath::Max(Bounds.BoxExtent.X, Bounds.BoxExtent.Y);
		if (HalfWidth < Data->CoverSize.X || HalfWidth > Data->CoverSize.Y || Bounds.BoxExtent.Z * 2.0f < MonkeyCoverMinHeight)
		{
			continue;
		}

		const FVector Away = ((Bounds.Origin - Seen) * FVector(1.0f, 1.0f, 0.0f)).GetSafeNormal();
		FVector Spot = Bounds.Origin + Away * (HalfWidth + OwnRadius + 25.0f);
		Spot.Z = Own.Z;

		if (FVector::Dist2D(Spot, Seen) < MinDistanceToPlayer || IsNearLight(Spot))
		{
			continue;
		}

		const float Score = FVector::Dist2D(Spot, Own);
		if (Score < BestScore)
		{
			BestScore = Score;
			OutHidePoint = Spot;
			bFound = true;
		}
	}

	return bFound;
}

void AVaelVeilMonkey::StartPounce(AVaelCharacter* InVictim)
{
	if (InVictim != nullptr && !IsDead() && State != EVaelMonkeyState::Cling)
	{
		if (State == EVaelMonkeyState::Dormant)
		{
			SetDormant(false);
		}

		// One that has dared to leap doesn't turn shy again
		Shyness = 0.0f;

		Victim = InVictim;
		EnterState(EVaelMonkeyState::Pounce);
	}
}

void AVaelVeilMonkey::TickBehavior(float DeltaSeconds)
{
	const UVaelVeilMonkeyData* Data = GetData<UVaelVeilMonkeyData>();
	const float Aggression = GetAggression();

	StateTime += DeltaSeconds;

	// The shy ones only come out as the land sickens
	PresenceCountdown -= DeltaSeconds;
	if (PresenceCountdown <= 0.0f && (State == EVaelMonkeyState::Dormant || State == EVaelMonkeyState::Hide))
	{
		PresenceCountdown = MonkeyPresenceInterval;

		const AVaelRegion* Region = AVaelRegion::GetRegionAt(GetWorld(), GetActorLocation());
		const float Corruption = Region != nullptr ? Region->GetCorruption() : 0.0f;
		// One that carries loot stays where the players can get it back
		const bool bShows = !Loot.IsEmpty() || Shyness <= Data->PresenceInHealthyLand + (1.0f - Data->PresenceInHealthyLand) * Corruption / 100.0f;

		if (bShows != (State != EVaelMonkeyState::Dormant))
		{
			SetDormant(!bShows);
		}
	}

	switch (State)
	{
	case EVaelMonkeyState::Dormant:
		break;

	case EVaelMonkeyState::Hide:
	{
		float NearestDistance = 0.0f;
		AVaelCharacter* Nearest = FindNearestPlayer(NearestDistance);
		Pointed = FindPointedPlayer();

		// Whoever looks its way sees the eyes go out
		bWatched = false;
		ForEachActivePlayer([this, Data](AVaelCharacter* Player)
		{
			const FVector ToMonkey = (GetActorLocation() - Player->GetActorLocation()) * FVector(1.0f, 1.0f, 0.0f);
			bWatched |= ToMonkey.Size() < Data->WatchedDistance
				&& FVector::DotProduct(Player->GetActorForwardVector().GetSafeNormal2D(), ToMonkey.GetSafeNormal()) > FMath::Cos(FMath::DegreesToRadians(Data->WatchedAngle));
		});

		// The lanterns and fires of the level, looked up now and then
		LightCountdown -= DeltaSeconds;
		if (LightCountdown <= 0.0f)
		{
			LightCountdown = 5.0f;
			Lights.Reset();
			for (TActorIterator<AVaelFireLight> It(GetWorld()); It; ++It)
			{
				Lights.Add(*It);
			}
		}

		if (Nearest == nullptr || NearestDistance > Data->AggroRange)
		{
			if (FVector::Dist2D(GetActorLocation(), HomeLocation) > 150.0f)
			{
				MoveTowards(HomeLocation, Data->MoveSpeed * 0.4f);
			}
			break;
		}

		// A calm one flees when someone comes close; a wild one lets them come and leaps
		const float LeapRange = Data->PounceRange * Aggression;
		const bool bFlees = Aggression <= 0.0f && NearestDistance < Data->FleeDistance;

		PounceCooldown -= DeltaSeconds;
		if (LeapRange > 0.0f && NearestDistance < LeapRange && PounceCooldown <= 0.0f && !IsBlinded() && !IsNearLight(Nearest->GetActorLocation()))
		{
			StartPounce(Nearest);
			break;
		}

		CoverCountdown -= DeltaSeconds;
		if (CoverCountdown <= 0.0f)
		{
			CoverCountdown = MonkeyCoverInterval;

			const float KeepAway = bFlees ? Data->FleeDistance * 1.7f : FMath::Lerp(Data->FleeDistance, 220.0f, Aggression);
			bHasCover = FindCover(Nearest, KeepAway, HidePoint);
			if (!bHasCover)
			{
				// Nothing to hide behind: it keeps its distance in the open
				const FVector Away = ((GetActorLocation() - Nearest->GetActorLocation()) * FVector(1.0f, 1.0f, 0.0f)).GetSafeNormal();
				HidePoint = NearestDistance < MonkeyOpenDistance ? GetActorLocation() + Away * 300.0f : GetActorLocation();
			}
		}

		if (FVector::Dist2D(GetActorLocation(), HidePoint) > MonkeyArriveDistance)
		{
			MoveTowards(HidePoint, Data->MoveSpeed * (bFlees ? 1.0f : 0.75f));
		}
		else
		{
			FaceTowards((Pointed.IsValid() ? Pointed.Get() : Nearest)->GetActorLocation());
		}
		break;
	}

	case EVaelMonkeyState::Pounce:
	{
		AVaelCharacter* Target = Victim.Get();
		if (Target == nullptr || Target->IsDefeated() || StateTime > Data->PounceTime)
		{
			PounceCooldown = RandomInRange(Data->PounceInterval);
			EnterState(EVaelMonkeyState::Hide);
			break;
		}

		const FVector Direction = ((Target->GetActorLocation() - GetActorLocation()) * FVector(1.0f, 1.0f, 0.0f)).GetSafeNormal();
		GetCharacterMovement()->Velocity = Direction * Data->PounceSpeed;
		MoveInDirection(Direction, Data->PounceSpeed);

		if (GetDistanceTo2D(Target) < GetCapsuleComponent()->GetScaledCapsuleRadius() + Target->GetCapsuleComponent()->GetScaledCapsuleRadius() + 25.0f)
		{
			// A player in the middle of a dodge roll can't be caught
			if (Target->IsDodging())
			{
				PounceCooldown = RandomInRange(Data->PounceInterval);
				Stun(0.6f);
				EnterState(EVaelMonkeyState::Hide);
				break;
			}

			EnterState(EVaelMonkeyState::Cling);
			GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			GetCharacterMovement()->SetMovementMode(MOVE_None);
			StealCountdown = Data->StealInterval * 0.5f;
			UVaelCombatTextSubsystem::PostReaction(Target, LOCTEXT("MonkeyClings", "Aufgehockt!"));
		}
		break;
	}

	case EVaelMonkeyState::Cling:
	{
		AVaelCharacter* Target = Victim.Get();
		if (Target == nullptr || Target->IsDefeated())
		{
			PounceCooldown = RandomInRange(Data->PounceInterval);
			EnterState(EVaelMonkeyState::Hide);
			break;
		}

		// A dodge roll throws it off; it keeps what it took
		if (Target->IsDodging())
		{
			UVaelCombatTextSubsystem::PostReaction(Target, LOCTEXT("MonkeyThrownOff", "Abgeschüttelt!"));
			PounceCooldown = RandomInRange(Data->PounceInterval);
			EnterState(EVaelMonkeyState::Hide);
			Stun(1.0f);
			break;
		}

		// Rides on the back
		SetActorLocationAndRotation(Target->GetActorLocation() - Target->GetActorForwardVector() * 28.0f + FVector(0.0f, 0.0f, 35.0f), Target->GetActorRotation());
		Target->SetBurden(Data->ClingSpeedMultiplier, 0.3f);

		StealCountdown -= DeltaSeconds;
		if (StealCountdown <= 0.0f)
		{
			StealCountdown = Data->StealInterval;
			Steal();
		}

		if (StateTime >= FMath::Lerp(Data->ClingTime.X, Data->ClingTime.Y, Aggression))
		{
			// Bites as it lets go and makes off with the loot
			HitPlayer(Target, Data->BiteDamage, EVaelElement::Earth);
			PounceCooldown = RandomInRange(Data->PounceInterval);
			EnterState(EVaelMonkeyState::Hide);
		}
		break;
	}

	case EVaelMonkeyState::Blinded:
		if (StateTime >= Data->BlindedTime)
		{
			PounceCooldown = RandomInRange(Data->PounceInterval);
			EnterState(EVaelMonkeyState::Hide);
		}
		break;
	}
}

bool AVaelVeilMonkey::IsNearLight(const FVector& Location) const
{
	const float Radius = GetData<UVaelVeilMonkeyData>()->LightAvoidRadius;
	for (const TWeakObjectPtr<AActor>& Light : Lights)
	{
		if (Light.IsValid() && FVector::Dist2D(Light->GetActorLocation(), Location) < Radius)
		{
			return true;
		}
	}
	return false;
}

void AVaelVeilMonkey::LetGo()
{
	GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	GetCharacterMovement()->SetMovementMode(MOVE_Walking);

	// Drops to the ground behind the player
	if (const AVaelCharacter* Target = Victim.Get())
	{
		FVector Ground = Target->GetActorLocation() - Target->GetActorForwardVector() * 90.0f;
		if (FindGround(GetWorld(), Ground, Ground))
		{
			SetActorLocation(Ground + FVector(0.0f, 0.0f, GetCapsuleComponent()->GetScaledCapsuleHalfHeight() + 2.0f));
		}
	}
}

void AVaelVeilMonkey::Steal()
{
	AVaelCharacter* Target = Victim.Get();
	UVaelMaterialBag* Bag = Target != nullptr ? Target->GetMaterialBag() : nullptr;
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
	const int32 Count = FMath::Min(GetData<UVaelVeilMonkeyData>()->StealCount, Stack.Count);
	if (!Bag->RemoveMaterial(Stack.Material, Count))
	{
		return;
	}

	Loot.Add({ Target, Stack.Material.Get(), Count });
	++TheftCount;

	UVaelCombatTextSubsystem::PostReaction(Target, FText::Format(LOCTEXT("MonkeySteals", "−{0} {1}"), FText::AsNumber(Count), Stack.Material->DisplayName));
	UE_LOG(LogVael, Verbose, TEXT("'%s' steals %d x %s"), *GetNameSafe(this), Count, *GetNameSafe(Stack.Material));
}

void AVaelVeilMonkey::ReturnLoot()
{
	if (Loot.IsEmpty())
	{
		return;
	}

	int32 NumReturned = 0;
	for (const FVaelStolen& Stolen : Loot)
	{
		AVaelCharacter* LootOwner = Stolen.Owner.Get();
		if (LootOwner != nullptr && Stolen.Material.IsValid() && LootOwner->GetMaterialBag() != nullptr)
		{
			LootOwner->GetMaterialBag()->AddMaterial(Stolen.Material.Get(), Stolen.Count);
			NumReturned += Stolen.Count;
		}
	}

	Loot.Reset();

	if (NumReturned > 0)
	{
		UVaelNoticeSubsystem::Post(this, LOCTEXT("MonkeyLootBack", "Beute zurück"), FText::Format(LOCTEXT("MonkeyLootBackDetail", "Der Schleieraffe lässt fallen, was er gestohlen hat: {0} Teile."), FText::AsNumber(NumReturned)), MonkeyEyeColor, 3.5f);
	}
}

void AVaelVeilMonkey::OnDamageTaken(float Damage, const FGameplayTagContainer& DamageTags)
{
	Super::OnDamageTaken(Damage, DamageTags);

	// Fire and lightning are too bright for its eyes: it lets go, drops its loot and cowers
	const bool bBright = DamageTags.HasTagExact(VaelTags::Element_Fire) || DamageTags.HasTagExact(VaelTags::Element_Air);
	if (bBright && !IsDead() && State != EVaelMonkeyState::Blinded && State != EVaelMonkeyState::Dormant)
	{
		EnterState(EVaelMonkeyState::Blinded);
		GetCharacterMovement()->StopMovementImmediately();
		ReturnLoot();
		UVaelCombatTextSubsystem::PostReaction(this, LOCTEXT("MonkeyBlinded", "Geblendet!"));
	}
}

void AVaelVeilMonkey::Die()
{
	if (State == EVaelMonkeyState::Cling)
	{
		EnterState(EVaelMonkeyState::Hide);
	}

	ReturnLoot();

	Super::Die();
}

void AVaelVeilMonkey::BuildRig()
{
	UMaterialInterface* CoreMaterial = nullptr;
	UMaterialInterface* GlowMaterial = nullptr;
	VaelEffects::LoadLookMaterials(CoreMaterial, GlowMaterial);

	// The placeholder body gives way to the rig; its colors still arrive through OnBodyColorShown
	GetBody()->SetVisibility(false);

	Rig = NewObject<USceneComponent>(this, TEXT("Rig"));
	Rig->SetupAttachment(GetRootComponent());
	Rig->RegisterComponent();

	const auto AddPart = [this](UMaterialInterface* Material, const FLinearColor& Color, float Glow)
	{
		UStaticMeshComponent* Part = VaelEffects::AddLookShape(this, Material, Color, Glow);
		Part->AttachToComponent(Rig, FAttachmentTransformRules::KeepRelativeTransform);
		Part->SetCastShadow(Glow <= 0.0f);
		Part->SetVisibility(true);
		return Part;
	};

	// A crouching body under a hood of rags, a thin tail, one long arm to point with
	Torso = AddPart(CoreMaterial, FurColor, 0.0f);
	Torso->SetRelativeLocation(FVector(-6.0f, 0.0f, -18.0f));
	Torso->SetRelativeScale3D(FVector(52.0f, 40.0f, 46.0f) / MonkeyShapeSize);

	Hood = AddPart(CoreMaterial, MonkeyRagColor, 0.0f);
	Hood->SetRelativeLocation(FVector(4.0f, 0.0f, 16.0f));
	Hood->SetRelativeScale3D(FVector(40.0f, 42.0f, 40.0f) / MonkeyShapeSize);

	for (int32 Index = 0; Index < 2; ++Index)
	{
		Eyes.Add(AddPart(GlowMaterial, MonkeyEyeColor, 9.0f));
	}

	Arm = AddPart(CoreMaterial, FurColor, 0.0f);
	Tail = AddPart(CoreMaterial, FurColor, 0.0f);
	Tail->SetRelativeLocation(FVector(-36.0f, 0.0f, -24.0f));
	Tail->SetRelativeRotation(FRotator(35.0f, 0.0f, 0.0f));
	Tail->SetRelativeScale3D(FVector(46.0f, 5.0f, 5.0f) / MonkeyShapeSize);

	// With a mesh of the creature factory only the glowing eyes of the code look stay, set into its face
	if (GetFactoryMesh() != nullptr)
	{
		for (UStaticMeshComponent* Shape : { Torso.Get(), Hood.Get(), Arm.Get(), Tail.Get() })
		{
			Shape->SetVisibility(false);
		}

		SetupLimbs();
	}

	RefreshBodyColor();
}

void AVaelVeilMonkey::OnBodyColorShown(const FLinearColor& Color, bool bHitFlash)
{
	FurColor = Color;

	if (Rig == nullptr)
	{
		return;
	}

	const auto Paint = [bHitFlash](UStaticMeshComponent* Part, const FLinearColor& PartColor)
	{
		if (UMaterialInstanceDynamic* Material = Cast<UMaterialInstanceDynamic>(Part->GetMaterial(0)))
		{
			Material->SetVectorParameterValue(VaelEffects::LookColorParameter, bHitFlash ? MonkeyFlashColor : PartColor);
		}
	};

	Paint(Torso, Color);
	Paint(Arm, Color);
	Paint(Tail, Color);
	Paint(Hood, MonkeyRagColor);
}

void AVaelVeilMonkey::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (Rig == nullptr || State == EVaelMonkeyState::Dormant)
	{
		return;
	}

	const float Time = GetWorld()->GetTimeSeconds();
	const float Aggression = GetAggression();
	const bool bDeadNow = IsDead();
	const bool bBlindedNow = State == EVaelMonkeyState::Blinded;

	// The wilder, the more it twitches
	const float Twitch = bDeadNow ? 0.0f : Aggression * FMath::Sign(FMath::Sin(Time * 17.0f)) * FMath::Max(0.0f, FMath::Sin(Time * 2.3f));
	Rig->SetRelativeRotation(FRotator(bBlindedNow ? -25.0f : 0.0f, Twitch * 9.0f, Twitch * 6.0f));
	Rig->SetRelativeScale3D(bDeadNow ? FVector(1.2f, 1.2f, 0.3f) : State == EVaelMonkeyState::Cling ? FVector(0.8f) : FVector::OneVector);

	// A mesh of the creature factory is posed bone by bone
	const bool bFactory = GetFactoryMesh() != nullptr;
	if (bFactory)
	{
		PoseFactoryMesh(DeltaSeconds, Twitch);
	}

	// Blinks now and then; blinded or dead the eyes are shut
	BlinkCountdown -= DeltaSeconds;
	if (BlinkCountdown <= 0.0f)
	{
		BlinkCountdown = FMath::FRandRange(1.5f, 4.5f) * (1.0f - 0.6f * Aggression);
	}
	const float OpenTarget = bDeadNow || bBlindedNow || (bWatched && State == EVaelMonkeyState::Hide) ? 0.02f : BlinkCountdown < 0.12f ? 0.05f : 1.0f;
	EyesOpen = FMath::FInterpTo(EyesOpen, OpenTarget, DeltaSeconds, 30.0f);

	// Bigger and more violet with the corruption
	const float EyeSize = bFactory ? FMath::Lerp(8.5f, 12.5f, Aggression) : FMath::Lerp(15.0f, 21.0f, Aggression);
	const FLinearColor EyeColor = FMath::Lerp(MonkeyEyeColor, MonkeyWildEyeColor, Aggression);

	for (int32 Index = 0; Index < Eyes.Num(); ++Index)
	{
		const float Side = Index == 0 ? -1.0f : 1.0f;

		UStaticMeshComponent* Eye = Eyes[Index];
		if (bFactory && bHasLimbs)
		{
			Eye->SetWorldLocation(EyeSpots[Index]);
		}
		else
		{
			Eye->SetRelativeLocation(FVector(21.0f, 9.5f * Side, 15.0f));
		}
		Eye->SetRelativeScale3D(FVector(EyeSize * 0.6f, EyeSize, EyeSize * EyesOpen) / MonkeyShapeSize);
		VaelEffects::SetLookGlow(Eye, 9.0f * EyesOpen);

		if (UMaterialInstanceDynamic* Material = Cast<UMaterialInstanceDynamic>(Eye->GetMaterial(0)))
		{
			Material->SetVectorParameterValue(VaelEffects::LookColorParameter, EyeColor);
		}
	}

	// The long arm points at the player closest to death
	const AVaelCharacter* PointAt = State == EVaelMonkeyState::Hide ? Pointed.Get() : nullptr;
	const FVector Shoulder(10.0f, 16.0f, -6.0f);
	FVector Hand(26.0f, 22.0f, -34.0f);
	if (PointAt != nullptr && !bDeadNow)
	{
		const FVector Local = GetActorTransform().InverseTransformPositionNoScale(PointAt->GetActorLocation());
		Hand = Shoulder + (Local - Shoulder).GetSafeNormal() * 46.0f;
	}

	Arm->SetRelativeLocation((Shoulder + Hand) * 0.5f);
	Arm->SetRelativeRotation((Hand - Shoulder).Rotation());
	Arm->SetRelativeScale3D(FVector(FVector::Dist(Shoulder, Hand) * 1.1f, 6.0f, 6.0f) / MonkeyShapeSize);
}

void AVaelVeilMonkey::SetupLimbs()
{
	const USkeletalMesh* SkeletalMesh = Cast<USkeletalMesh>(GetFactoryMesh()->GetSkinnedAsset());
	if (SkeletalMesh == nullptr)
	{
		return;
	}

	const FReferenceSkeleton& RefSkeleton = SkeletalMesh->GetRefSkeleton();
	const auto RestOf = [&RefSkeleton](FName Bone, FTransform& OutRest)
	{
		const int32 Index = RefSkeleton.FindBoneIndex(Bone);
		if (Index == INDEX_NONE)
		{
			return false;
		}
		OutRest = FAnimationRuntime::GetComponentSpaceTransformRefPose(RefSkeleton, Index);
		return true;
	};

	const auto Find = [&RestOf](FVaelMonkeyLimb& Limb, const TCHAR* Upper, const TCHAR* Lower)
	{
		Limb.Upper = Upper;
		Limb.Lower = Lower;
		Limb.bFound = RestOf(Limb.Upper, Limb.RestUpper) && RestOf(Limb.Lower, Limb.RestLower);
		return Limb.bFound;
	};

	// The first of each pair is the one on the side of +X of the mesh
	bHasLimbs = RestOf(TEXT("body"), RestBody) && RestOf(TEXT("head"), RestHead);
	bHasLimbs &= Find(Arms[0], TEXT("arm_L_upper"), TEXT("arm_L_lower")) && Find(Arms[1], TEXT("arm_R_upper"), TEXT("arm_R_lower"));
	bHasLimbs &= Find(Legs[0], TEXT("leg_L_upper"), TEXT("leg_L_lower")) && Find(Legs[1], TEXT("leg_R_upper"), TEXT("leg_R_lower"));

	if (!bHasLimbs)
	{
		UE_LOG(LogVael, Warning, TEXT("'%s': the mesh has no arms and legs to pose, it keeps its rest pose"), *GetNameSafe(this));
	}
}

void AVaelVeilMonkey::PoseLimb(const FVaelMonkeyLimb& Limb, const FVector& Root, const FQuat& Turn, const FQuat& Bend)
{
	UPoseableMeshComponent* MonkeyMesh = GetFactoryMesh();

	FTransform Upper = Limb.RestUpper;
	Upper.SetLocation(Root);
	Upper.SetRotation(Turn * Limb.RestUpper.GetRotation());
	MonkeyMesh->SetBoneTransformByName(Limb.Upper, Upper, EBoneSpaces::ComponentSpace);

	FTransform Lower = Limb.RestLower;
	Lower.SetLocation(Root + Turn.RotateVector(Limb.RestLower.GetLocation() - Limb.RestUpper.GetLocation()));
	Lower.SetRotation(Turn * Bend * Limb.RestLower.GetRotation());
	MonkeyMesh->SetBoneTransformByName(Limb.Lower, Lower, EBoneSpaces::ComponentSpace);
}

void AVaelVeilMonkey::PoseFactoryMesh(float DeltaSeconds, float Twitch)
{
	UPoseableMeshComponent* MonkeyMesh = GetFactoryMesh();
	const UVaelVeilMonkeyData* Data = GetData<UVaelVeilMonkeyData>();
	const float HalfHeight = GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight();
	const float Time = GetWorld()->GetTimeSeconds();
	const float Speed = GetVelocity().Size2D();
	const bool bDeadNow = IsDead();
	const bool bRunning = Speed > 40.0f && State != EVaelMonkeyState::Cling;

	HopPhase += Speed * DeltaSeconds / 70.0f;

	if (!bHasLimbs)
	{
		MonkeyMesh->SetRelativeLocationAndRotation(FVector(0.0f, 0.0f, -HalfHeight), FRotator(0.0f, Data->MeshYaw, 0.0f));
		return;
	}

	// The mesh looks along +Y and has its sides along X: leaning forward and swinging limbs are turns around X
	const FVector SideAxis = FVector::ForwardVector;
	const float ForwardTurn = FQuat(SideAxis, 0.2f).RotateVector(FVector::UpVector).Y > 0.0f ? 1.0f : -1.0f;

	// How far it leans and crouches: sitting on its haunches in hiding, low and stretched on the run, flat in the leap
	float LeanTarget = 38.0f;
	float CrouchTarget = 1.0f;
	switch (State)
	{
	case EVaelMonkeyState::Pounce:	LeanTarget = 70.0f; CrouchTarget = 0.25f; break;
	case EVaelMonkeyState::Cling:	LeanTarget = 25.0f; CrouchTarget = 0.9f; break;
	case EVaelMonkeyState::Blinded:	LeanTarget = 55.0f; CrouchTarget = 1.0f; break;
	default:						LeanTarget = bRunning ? 58.0f : 38.0f; CrouchTarget = bRunning ? 0.7f : 1.0f; break;
	}
	if (bDeadNow)
	{
		LeanTarget = 85.0f;
		CrouchTarget = 0.3f;
	}

	LeanShown = FMath::FInterpTo(LeanShown, LeanTarget, DeltaSeconds, 9.0f);
	CrouchShown = FMath::FInterpTo(CrouchShown, CrouchTarget, DeltaSeconds, 9.0f);

	const float Stride = bRunning ? FMath::Sin(HopPhase * UE_TWO_PI) : 0.0f;
	const FVector Pelvis = RestBody.GetLocation();
	const FQuat BodyTurn(SideAxis, FMath::DegreesToRadians(LeanShown + (bRunning ? FMath::Abs(Stride) * 6.0f : FMath::Sin(Time * 2.1f) * 1.5f)) * ForwardTurn);

	// Legs: thighs forward, shins back; the body sinks by what the bent legs lose in height
	float Drop = 0.0f;
	for (int32 Side = 0; Side < 2; ++Side)
	{
		const FVaelMonkeyLimb& Leg = Legs[Side];
		const FVector Hip = Leg.RestUpper.GetLocation();
		const FVector Knee = Leg.RestLower.GetLocation();
		const float Thigh = FVector::Dist(Hip, Knee);
		const float Shin = FMath::Max(Knee.Z, 1.0f);

		const float Swing = Stride * (Side == 0 ? 1.0f : -1.0f) * 28.0f;
		const float ThighAngle = FMath::DegreesToRadians(CrouchShown * 62.0f + Swing);
		const float KneeAngle = FMath::DegreesToRadians(CrouchShown * 118.0f + FMath::Max(Swing, 0.0f) * 0.8f);

		// A leg points down, so the turn that brings its knee forward is the opposite of the one that leans the body forward
		const FQuat Turn(SideAxis, -ThighAngle * ForwardTurn);
		const FQuat Bend(SideAxis, KneeAngle * ForwardTurn);
		PoseLimb(Leg, Pelvis + BodyTurn.RotateVector(Hip - Pelvis) * FVector(1.0f, 0.3f, 0.3f) + (Hip - Pelvis) * FVector(0.0f, 0.7f, 0.7f), Turn, Bend);

		Drop = FMath::Max(Drop, (Thigh + Shin) - (Thigh * FMath::Cos(ThighAngle) + Shin * FMath::Cos(KneeAngle - ThighAngle)));
	}

	// The body leans forward around the pelvis; the head lifts again to look ahead
	FTransform BodyPose = RestBody;
	BodyPose.SetRotation(BodyTurn * RestBody.GetRotation());
	MonkeyMesh->SetBoneTransformByName(TEXT("body"), BodyPose, EBoneSpaces::ComponentSpace);

	const FQuat HeadTurn = FQuat(FVector::UpVector, FMath::DegreesToRadians(Twitch * 14.0f)) * FQuat(SideAxis, FMath::DegreesToRadians(LeanShown * (State == EVaelMonkeyState::Blinded ? 1.2f : 0.25f)) * ForwardTurn);
	FTransform Head = RestHead;
	Head.SetLocation(Pelvis + BodyTurn.RotateVector(RestHead.GetLocation() - Pelvis));
	Head.SetRotation(HeadTurn * RestHead.GetRotation());
	MonkeyMesh->SetBoneTransformByName(TEXT("head"), Head, EBoneSpaces::ComponentSpace);

	// Arms: knuckles on the ground in hiding, paddling on the run, thrown forward in the leap, over the eyes when blinded.
	// One long arm points at the player closest to death.
	const AVaelCharacter* PointAt = State == EVaelMonkeyState::Hide && !bRunning && !bDeadNow ? Pointed.Get() : nullptr;
	for (int32 Side = 0; Side < 2; ++Side)
	{
		const FVaelMonkeyLimb& ArmLimb = Arms[Side];
		const float Out = ArmLimb.RestUpper.GetLocation().X < 0.0f ? -1.0f : 1.0f;

		FVector Want(0.25f * Out, 0.45f, -1.0f);
		switch (State)
		{
		case EVaelMonkeyState::Pounce:	Want = FVector(0.35f * Out, 1.0f, 0.25f); break;
		case EVaelMonkeyState::Cling:	Want = FVector(-0.25f * Out, 1.0f, -0.1f); break;
		case EVaelMonkeyState::Blinded:	Want = FVector(-0.15f * Out, 0.7f, 0.9f); break;
		default:
			if (bRunning)
			{
				Want = FVector(0.2f * Out, 0.35f - Stride * (Side == 0 ? 1.0f : -1.0f) * 0.75f, -1.0f);
			}
			break;
		}

		if (PointAt != nullptr && Side == 0)
		{
			const FVector Local = MonkeyMesh->GetComponentTransform().InverseTransformPositionNoScale(PointAt->GetActorLocation());
			Want = (Local - ArmLimb.RestUpper.GetLocation()).GetSafeNormal() + FVector(0.0f, 0.0f, 0.12f);
		}

		ArmShown[Side] = FMath::VInterpTo(ArmShown[Side], Want.GetSafeNormal(), DeltaSeconds, 10.0f);

		const FVector RestDirection = (ArmLimb.RestLower.GetLocation() - ArmLimb.RestUpper.GetLocation()).GetSafeNormal();
		const FQuat Turn = FQuat::FindBetweenNormals(RestDirection, ArmShown[Side].GetSafeNormal());
		PoseLimb(ArmLimb, Pelvis + BodyTurn.RotateVector(ArmLimb.RestUpper.GetLocation() - Pelvis), Turn, FQuat::Identity);
	}

	// The whole mesh: sunk onto its bent legs, bobbing on the run, small on a player's back, slumped when dead
	const float Bob = bRunning ? FMath::Abs(FMath::Sin(HopPhase * UE_TWO_PI)) * 7.0f : 0.0f;
	const FVector Scale = State == EVaelMonkeyState::Cling ? FVector(0.85f) : FVector::OneVector;

	MonkeyMesh->SetRelativeScale3D(Scale);
	MonkeyMesh->SetRelativeLocationAndRotation(FVector(0.0f, 0.0f, -HalfHeight - Drop * Scale.Z + Bob),
		FRotator(0.0f, Twitch * 9.0f, Twitch * 5.0f).Quaternion() * FRotator(0.0f, Data->MeshYaw, 0.0f).Quaternion());

	// The eyes sit in the face, a little above the middle of the head
	const float HeadHeight = FMath::Max(RestHead.GetLocation().Z, 1.0f);
	for (int32 Side = 0; Side < 2; ++Side)
	{
		const FVector RestEye(RestHead.GetLocation().X + (Side == 0 ? 1.0f : -1.0f) * HeadHeight * 0.085f, RestHead.GetLocation().Y + HeadHeight * 0.16f, HeadHeight * 1.19f);
		const FVector Posed = Head.GetLocation() + HeadTurn.RotateVector(RestEye - RestHead.GetLocation());
		EyeSpots[Side] = MonkeyMesh->GetComponentTransform().TransformPosition(Posed);
	}
}

#undef LOCTEXT_NAMESPACE
