// Copyright Epic Games, Inc. All Rights Reserved.

#include "Creatures/VaelVeilMonkey.h"
#include "Components/CapsuleComponent.h"
#include "Components/PoseableMeshComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Creatures/VaelCreatureData.h"
#include "Engine/OverlapResult.h"
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

	// A mesh of the creature factory keeps its crouch and is moved as a whole: it scurries in small hops
	const bool bFactory = GetFactoryMesh() != nullptr;
	if (bFactory)
	{
		const float HalfHeight = GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight();
		const float Speed = GetVelocity().Size2D();
		HopPhase += Speed * DeltaSeconds / 55.0f;

		const float Hop = bDeadNow || State == EVaelMonkeyState::Cling ? 0.0f : FMath::Abs(FMath::Sin(HopPhase * UE_PI)) * FMath::Min(Speed / 300.0f, 1.0f);
		const float Breath = FMath::Sin(Time * 2.2f) * 1.2f;
		const FRotator Pose(bBlindedNow ? -22.0f : -10.0f * Hop, Twitch * 9.0f, Twitch * 6.0f);
		const FVector Scale = bDeadNow ? FVector(1.15f, 1.15f, 0.35f) : State == EVaelMonkeyState::Cling ? FVector(0.8f) : FVector(1.0f, 1.0f, 1.0f + 0.01f * Breath);

		GetFactoryMesh()->SetRelativeLocationAndRotation(FVector(0.0f, 0.0f, -HalfHeight + Hop * 14.0f), Pose.Quaternion() * FRotator(0.0f, GetData<UVaelVeilMonkeyData>()->MeshYaw, 0.0f).Quaternion());
		GetFactoryMesh()->SetRelativeScale3D(Scale);
		Rig->SetRelativeLocation(FVector(0.0f, 0.0f, Hop * 14.0f));
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
	const float EyeSize = FMath::Lerp(15.0f, 21.0f, Aggression);
	const FLinearColor EyeColor = FMath::Lerp(MonkeyEyeColor, MonkeyWildEyeColor, Aggression);

	for (int32 Index = 0; Index < Eyes.Num(); ++Index)
	{
		const float Side = Index == 0 ? -1.0f : 1.0f;

		UStaticMeshComponent* Eye = Eyes[Index];
		Eye->SetRelativeLocation(bFactory ? FVector(31.0f, 8.5f * Side, 12.0f) : FVector(21.0f, 9.5f * Side, 15.0f));
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

#undef LOCTEXT_NAMESPACE
