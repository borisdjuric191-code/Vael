// Copyright Epic Games, Inc. All Rights Reserved.

// Stage II of the engineer's sets (Boris 26.11.), worn whole like stage I and keeping all of it:
//   Glutkrone (Schwarzglut II, Glutkoenigin): fire fills brood chambers, a full one lays a heat-seeking Glutei; the rolling
//     Panzerkugel lays them along its way; fire on the ground heals instead of burning (Glutbad); the Brutbuechse, a pressure
//     sack of slag pellets without powder, takes the place of the sling.
//   Aschenjaeger II (Harpyien-Aelteste, wind): the dodge held becomes the Drehtanz; every turn makes him faster, the sickles
//     circle wider and cut; an attack lets go a Klingenteufel; from eight turns a tornado grows round him, and letting go of
//     the dodge then sends it off as the Aschenteufel. Too long a dance without letting go makes him dizzy. Players untouched.

#include "Engineer/VaelEngineerComponent.h"
#include "Combat/VaelCharacterBase.h"
#include "Combat/VaelCombatStatics.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Engineer/VaelEngineerShot.h"
#include "Engineer/VaelDoubleSickle.h"
#include "Components/PoseableMeshComponent.h"
#include "Engineer/VaelEngineerStage2Actors.h"
#include "EngineUtils.h"
#include "Magic/VaelGroundArea.h"
#include "Magic/VaelMagicSettings.h"
#include "Magic/VaelSpellEffects.h"
#include "NiagaraSystem.h"
#include "Player/VaelCharacter.h"
#include "UI/VaelCombatTextSubsystem.h"
#include "VaelAssets.h"

#define LOCTEXT_NAMESPACE "VaelEngineerStage2"

namespace
{
	/** Heat for one Glutei (fire damage dealt counts whole, taken half), most eggs at once, damage of an egg's burst */
	constexpr float HeatPerEgg = 60.0f;
	constexpr int32 MaxEggs = 4;
	constexpr float EggDamage = 30.0f;
	/** Seconds between eggs the Panzerkugel lays */
	constexpr float BallEggEvery = 1.0f;
	/** Life a second the Glutbad gives him while he stands in fire */
	constexpr float GlutbadHeal = 6.0f;
	/** The Brutbuechse: pellets in the sack, seconds for one more (half by a fire), damage, speed, spread of the clutch */
	constexpr int32 BroodPelletsMax = 6;
	constexpr float BroodRefill = 0.7f;
	constexpr float BroodDamage = 20.0f;
	constexpr float BroodSpeed = 2600.0f;
	constexpr float BroodClutchSpread = 7.0f;
	constexpr float BroodHoldForClutch = 0.35f;
	/** The Drehtanz: seconds after the dodge before it starts, turns a second (rising with the stacks), most stacks, speed per
	 *  stack, turns for the tornado, blade ring radius, how often the blades cut, tornado radius and pull, devil cooldown,
	 *  seconds before dizziness and how long it lasts */
	constexpr float WhirlDelay = 0.3f;
	constexpr float WhirlTurnRate = 1.4f;
	constexpr int32 WhirlStacksMax = 10;
	constexpr float WhirlSpeedPerStack = 0.04f;
	constexpr int32 TornadoStacks = 8;
	constexpr float WhirlRing = 130.0f;
	constexpr float WhirlCutEvery = 0.35f;
	constexpr float WhirlDamage = 8.0f;
	constexpr float TornadoRadius = 380.0f;
	constexpr float TornadoPull = 220.0f;
	constexpr float DevilEvery = 0.8f;
	constexpr float DevilDamage = 10.0f;
	constexpr float BigDevilDamage = 16.0f;
	constexpr float WhirlDizzyAfter = 8.0f;
	constexpr float DizzySeconds = 1.2f;
}

bool UVaelEngineerComponent::UsesBroodGun() const
{
	const AVaelCharacter* Player = GetPlayer();
	return Player != nullptr && Player->IsEngineer() && GetSetStage(TEXT("Schwarzglut")) >= 2;
}

int32 UVaelEngineerComponent::GetEggsAlive() const
{
	int32 Count = 0;
	for (const TWeakObjectPtr<AActor>& Egg : GlutEggs)
	{
		Count += Egg.IsValid() ? 1 : 0;
	}
	return Count;
}

bool UVaelEngineerComponent::IsDizzy() const
{
	return Now() < DizzyUntil;
}

void UVaelEngineerComponent::AddBroodHeat(float Heat)
{
	if (GetSetStage(TEXT("Schwarzglut")) < 2 || Heat <= 0.0f)
	{
		return;
	}
	BroodHeat += Heat / HeatPerEgg;
	while (BroodHeat >= 1.0f)
	{
		BroodHeat -= 1.0f;
		// the sting strikes it into the ground (26.11.); without the sting it is laid behind him
		if (HasSting())
		{
			++PendingStrikes;
			continue;
		}
		const AActor* Owner = GetOwner();
		LayGlutEgg(Owner->GetActorLocation() - Owner->GetActorForwardVector() * 70.0f);
	}
}

void UVaelEngineerComponent::LayGlutEgg(const FVector& At)
{
	GlutEggs.RemoveAll([](const TWeakObjectPtr<AActor>& Egg) { return !Egg.IsValid(); });
	if (GlutEggs.Num() >= MaxEggs)
	{
		return;
	}
	FHitResult Hit;
	const bool bGround = GetWorld()->LineTraceSingleByChannel(Hit, At + FVector(0.0f, 0.0f, 100.0f), At - FVector(0.0f, 0.0f, 400.0f), ECC_Visibility);
	const FVector Spot = (bGround ? Hit.ImpactPoint : At - FVector(0.0f, 0.0f, 80.0f)) + FVector(0.0f, 0.0f, 14.0f);
	if (AVaelGlutEgg* Egg = AVaelGlutEgg::Lay(this, Spot, EggDamage * GetDamageScale() * GetTierScale()))
	{
		GlutEggs.Add(Egg);
		++EggsLaid;
		const UVaelMagicSettings* Magic = UVaelMagicSettings::Get();
		VaelEffects::SpawnAt(this, VaelAssets::LoadOptional(Magic->DustBurstEffect), Spot, FRotator::ZeroRotator, FLinearColor(1.0f, 0.45f, 0.1f), 90.0f);
		UVaelCombatTextSubsystem::PostReaction(GetOwner(), LOCTEXT("EggLaid", "Glutei"));
	}
}

AVaelDoubleSickle* UVaelEngineerComponent::GetFlyingSickle() const
{
	for (const TWeakObjectPtr<AVaelDoubleSickle>& Each : FlyingSickles)
	{
		if (Each.IsValid())
		{
			return Each.Get();
		}
	}
	return nullptr;
}

int32 UVaelEngineerComponent::GetSicklesOut() const
{
	int32 Count = 0;
	for (const TWeakObjectPtr<AVaelDoubleSickle>& Each : FlyingSickles)
	{
		Count += Each.IsValid() ? 1 : 0;
	}
	return Count;
}

void UVaelEngineerComponent::TickSickleBand()
{
	// Two Doppelsicheln out at once (Boris 26.11.: "die 2 hin und her"): between them a band of whirling ash cuts what stands on
	// the line from one to the other; in the Aschenjaeger II the wind makes it a gale
	TArray<AVaelDoubleSickle*> Out;
	for (const TWeakObjectPtr<AVaelDoubleSickle>& Each : FlyingSickles)
	{
		if (Each.IsValid())
		{
			Out.Add(Each.Get());
		}
	}
	if (Out.Num() < 2 || Now() < NextBandCut)
	{
		return;
	}
	NextBandCut = Now() + 0.3f;
	const bool bGale = GetSetStage(TEXT("Aschenjaeger")) >= 2;
	const FVector A = Out[0]->GetActorLocation();
	const FVector B = Out[1]->GetActorLocation();
	const UVaelMagicSettings* Magic = UVaelMagicSettings::Get();
	for (int32 Step = 1; Step <= 3; ++Step)
	{
		VaelEffects::SpawnAt(this, VaelAssets::LoadOptional(Magic->SteamEffect), FMath::Lerp(A, B, Step / 4.0f), FRotator::ZeroRotator,
			FLinearColor(0.4f, 0.38f, 0.36f), bGale ? 50.0f : 30.0f);
	}
	for (AVaelCharacterBase* Enemy : EnemiesAround((A + B) * 0.5f, FVector::Dist2D(A, B) * 0.5f + 80.0f))
	{
		const FVector Closest = FMath::ClosestPointOnSegment(Enemy->GetActorLocation(), A, B);
		if (FVector::Dist2D(Closest, Enemy->GetActorLocation()) > (bGale ? 90.0f : 55.0f) + Enemy->GetSimpleCollisionRadius() * 0.5f)
		{
			continue;
		}
		++BandCuts;
		ApplyHit(Enemy, Closest, (bGale ? 9.0f : 6.0f) * GetDamageScale() * GetTierScale(), VaelEngineerGear::Standard, Closest, NAME_None);
	}
}

void UVaelEngineerComponent::TickStage2(float DeltaTime)
{
	TickSickleBand();
	// the sting is looked for again while the Glutkrone is on and it is not there yet (the body may come after the armour)
	const bool bGlutkrone = GetSetStage(TEXT("Schwarzglut")) >= 2;
	if (bGlutkrone != HasSting() && Now() >= NextStingLook)
	{
		NextStingLook = Now() + 0.5f;
		UpdateSting();
	}
	if (GetSetStage(TEXT("Schwarzglut")) >= 2)
	{
		TickGlutkrone(DeltaTime);
	}
	if (GetSetStage(TEXT("Aschenjaeger")) >= 2 || bWhirling)
	{
		TickWhirl(DeltaTime);
	}
}

void UVaelEngineerComponent::TickGlutkrone(float DeltaTime)
{
	TickSting(DeltaTime);
	AActor* Owner = GetOwner();
	const bool bInFire = AVaelGroundArea::IsFireNear(GetWorld(), Owner->GetActorLocation(), 20.0f);
	// Glutbad: fire on the ground heals him (the burning itself is held off in ScaleIncoming)
	if (bInFire)
	{
		UVaelCombatStatics::Heal(Owner, GlutbadHeal * DeltaTime);
	}
	// the rolling Panzerkugel lays its eggs along the way
	if (IsBall() && Now() >= NextBallEgg)
	{
		NextBallEgg = Now() + BallEggEvery;
		LayGlutEgg(Owner->GetActorLocation() - Owner->GetActorForwardVector() * 60.0f);
	}
	// the sack fills again from the heat, twice as fast by a fire
	if (BroodPellets < BroodPelletsMax && Now() >= NextPellet)
	{
		++BroodPellets;
		NextPellet = Now() + BroodRefill * (bInFire ? 0.5f : 1.0f);
	}
}

bool UVaelEngineerComponent::FireBroodGun(const FVector& AimLocation, bool bClutch)
{
	AVaelCharacter* Player = GetPlayer();
	if (Player == nullptr || Player->IsDowned() || IsGrabbing() || BroodPellets <= 0)
	{
		return false;
	}
	const UVaelMagicSettings* Magic = UVaelMagicSettings::Get();
	const FVector From = Player->GetActorLocation() + Player->GetActorForwardVector() * 50.0f + FVector(0.0f, 0.0f, 30.0f);
	// wet, the sack misfires: a cloud of steam, a pellet lost
	if (UVaelCombatStatics::HasStatus(Player, EVaelStatus::Wet))
	{
		--BroodPellets;
		VaelEffects::SpawnAt(this, VaelAssets::LoadOptional(Magic->SteamEffect), From, FRotator::ZeroRotator, FLinearColor(0.7f, 0.7f, 0.72f), 120.0f);
		UVaelCombatTextSubsystem::PostReaction(Player, LOCTEXT("BroodMisfire", "Fehlzündung"));
		NextPellet = Now() + BroodRefill;
		return true;
	}
	FVector Toward = AimLocation - From;
	Toward.Z = 0.0f;
	if (Toward.IsNearlyZero())
	{
		Toward = Player->GetActorForwardVector();
	}
	const float Distance = FMath::Clamp(Toward.Size(), 300.0f, 1600.0f);
	const FVector Offset = Toward.GetSafeNormal() * Distance + FVector(0.0f, 0.0f, -20.0f);
	// a charge in the hand goes into the sack: one charge for the shot or the clutch
	if (Ammo != VaelEngineerGear::Standard && GetCarried(Ammo) <= 0)
	{
		Ammo = VaelEngineerGear::Standard;
	}
	const FName Load = Ammo;
	const int32 Count = bClutch ? BroodPellets : 1;
	const float Damage = BroodDamage * GetDamageScale() * GetTierScale();
	for (int32 Index = 0; Index < Count; ++Index)
	{
		const float Spread = Count > 1 ? (Index - (Count - 1) * 0.5f) * BroodClutchSpread : 0.0f;
		const FVector Way = FRotator(0.0f, Spread, 0.0f).RotateVector(Offset);
		AVaelEngineerShot::Launch(this, From, From + Way, BroodSpeed, 0.04f, Damage, Load, false, FLinearColor(1.0f, 0.45f, 0.1f), 14.0f, false);
	}
	VaelEffects::SpawnAt(this, VaelAssets::LoadOptional(Magic->SteamEffect), From, FRotator::ZeroRotator, FLinearColor(0.8f, 0.35f, 0.1f), bClutch ? 90.0f : 45.0f);
	BroodPellets -= Count;
	NextPellet = Now() + BroodRefill;
	LastShot = Now();
	if (Load != VaelEngineerGear::Standard)
	{
		int32& Left = Carried.FindOrAdd(Load);
		Left = FMath::Max(Left - 1, 0);
		if (Left == 0)
		{
			Ammo = VaelEngineerGear::Standard;
		}
	}
	if (bClutch)
	{
		UVaelCombatTextSubsystem::PostReaction(Player, LOCTEXT("BroodClutch", "Gelege!"));
	}
	return true;
}

void UVaelEngineerComponent::TickWhirl(float DeltaTime)
{
	AVaelCharacter* Player = GetPlayer();
	if (Player == nullptr)
	{
		return;
	}
	const bool bHeldNow = bWhirlHeld || (bRollHeld && Now() - DodgeAt >= WhirlDelay);
	const bool bWant = bHeldNow && GetSetStage(TEXT("Aschenjaeger")) >= 2 && !IsDizzy() && !Player->IsDowned();
	if (!bWhirling)
	{
		if (bWant)
		{
			bWhirling = true;
			WhirlTime = 0.0f;
			WhirlTurns = 0.0f;
			WhirlStacks = 0;
			SetComponentTickInterval(0.0f);
			UVaelCombatTextSubsystem::PostReaction(Player, LOCTEXT("WhirlStart", "Drehtanz"));
		}
		return;
	}
	if (!bWant)
	{
		// let go: after a long dance the tornado leaves as the Aschenteufel
		EndWhirl(!bRollHeld && !bWhirlHeld);
		return;
	}
	WhirlTime += DeltaTime;
	if (WhirlTime > WhirlDizzyAfter)
	{
		DizzyUntil = Now() + DizzySeconds;
		UVaelCombatStatics::ApplyStatus(Player, Player, EVaelStatus::Slowed, DizzySeconds);
		UVaelCombatTextSubsystem::PostReaction(Player, LOCTEXT("Dizzy", "Schwindlig"));
		EndWhirl(false);
		return;
	}
	const float Before = WhirlTurns;
	WhirlTurns += DeltaTime * WhirlTurnRate * (1.0f + 0.08f * WhirlStacks);
	if (FMath::FloorToInt(WhirlTurns) > FMath::FloorToInt(Before) && WhirlStacks < WhirlStacksMax)
	{
		++WhirlStacks;
		if (WhirlStacks == TornadoStacks)
		{
			UVaelCombatTextSubsystem::PostReaction(Player, LOCTEXT("Tornado", "Tornado!"));
		}
	}

	// the sickles circle round him, wider with every turn
	if (WhirlBlades.Num() < 2)
	{
		UStaticMesh* Mesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Game/Vael/Characters/Ingenieur/Werkstuecke/SM_Ing_Doppelsichel_Tripo.SM_Ing_Doppelsichel_Tripo"), nullptr, LOAD_NoWarn | LOAD_Quiet);
		for (int32 Index = WhirlBlades.Num(); Index < 2; ++Index)
		{
			UStaticMeshComponent* Blade = NewObject<UStaticMeshComponent>(Player, *FString::Printf(TEXT("EngineerWhirlBlade%d"), Index));
			Blade->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			Blade->SetStaticMesh(Mesh);
			Blade->SetUsingAbsoluteLocation(true);
			Blade->SetUsingAbsoluteRotation(true);
			Blade->RegisterComponent();
			WhirlBlades.Add(Blade);
		}
	}
	const FVector Here = Player->GetActorLocation();
	const float Ring = WhirlRing + 12.0f * WhirlStacks;
	for (int32 Index = 0; Index < WhirlBlades.Num(); ++Index)
	{
		if (UStaticMeshComponent* Blade = WhirlBlades[Index])
		{
			const float Angle = WhirlTurns * 360.0f + Index * 180.0f;
			Blade->SetVisibility(true);
			Blade->SetWorldLocation(Here + FRotator(0.0f, Angle, 0.0f).RotateVector(FVector(Ring, 0.0f, Index == 0 ? 10.0f : 60.0f)));
			Blade->SetWorldRotation(FRotator(12.0f, Angle * 3.0f, 0.0f));
		}
	}
	const UVaelMagicSettings* Magic = UVaelMagicSettings::Get();
	const bool bTornado = WhirlStacks >= TornadoStacks;
	if (Now() >= NextWhirlPuff)
	{
		NextWhirlPuff = Now() + (bTornado ? 0.08f : 0.2f);
		const float Radius = bTornado ? TornadoRadius : Ring;
		const FVector Puff = Here + FRotator(0.0f, WhirlTurns * 360.0f + FMath::FRandRange(0.0f, 360.0f), 0.0f).RotateVector(FVector(Radius * FMath::FRandRange(0.5f, 1.0f), 0.0f, FMath::FRandRange(-60.0f, bTornado ? 220.0f : 40.0f)));
		VaelEffects::SpawnAt(this, VaelAssets::LoadOptional(Magic->SteamEffect), Puff, FRotator::ZeroRotator, FLinearColor(0.42f, 0.4f, 0.38f), bTornado ? 70.0f : 35.0f);
	}
	// the blades cut what is in their ring; the tornado also draws light enemies into them; players untouched
	const bool bCut = Now() >= NextWhirlCut;
	if (bCut)
	{
		NextWhirlCut = Now() + WhirlCutEvery;
	}
	const float Reach = bTornado ? TornadoRadius : Ring + 60.0f;
	for (AVaelCharacterBase* Enemy : EnemiesAround(Here, Reach))
	{
		if (bTornado && Enemy->GetSimpleCollisionRadius() <= 70.0f)
		{
			FVector ToMiddle = Here - Enemy->GetActorLocation();
			ToMiddle.Z = 0.0f;
			if (ToMiddle.Size() > Ring)
			{
				Enemy->SetActorLocation(Enemy->GetActorLocation() + ToMiddle.GetSafeNormal() * TornadoPull * DeltaTime, true);
			}
		}
		if (bCut)
		{
			++WhirlCuts;
			ApplyHit(Enemy, Enemy->GetActorLocation(), WhirlDamage * GetDamageScale() * GetTierScale() * (1.0f + 0.05f * WhirlStacks), VaelEngineerGear::Standard, Here, NAME_None);
		}
	}
}

void UVaelEngineerComponent::ReleaseWindDevil(const FVector& AimLocation, bool bBig)
{
	AVaelCharacter* Player = GetPlayer();
	if (Player == nullptr || (!bBig && Now() < NextDevil))
	{
		return;
	}
	NextDevil = Now() + DevilEvery;
	FVector Toward = AimLocation - Player->GetActorLocation();
	Toward.Z = 0.0f;
	if (Toward.IsNearlyZero())
	{
		Toward = Player->GetActorForwardVector();
	}
	const float Damage = (bBig ? BigDevilDamage : DevilDamage) * GetDamageScale() * GetTierScale();
	if (AVaelWindDevil::Release(this, Player->GetActorLocation() - FVector(0.0f, 0.0f, 60.0f) + Toward.GetSafeNormal() * 80.0f, Toward, Damage, bBig))
	{
		++DevilsReleased;
		UVaelCombatTextSubsystem::PostReaction(Player, bBig ? LOCTEXT("Aschenteufel", "Aschenteufel!") : LOCTEXT("Klingenteufel", "Klingenteufel"));
	}
}

void UVaelEngineerComponent::EndWhirl(bool bRelease)
{
	AVaelCharacter* Player = GetPlayer();
	if (bRelease && WhirlStacks >= TornadoStacks && Player != nullptr)
	{
		ReleaseWindDevil(Player->GetActorLocation() + Player->GetActorForwardVector() * 600.0f, true);
	}
	bWhirling = false;
	WhirlStacks = 0;
	WhirlTime = 0.0f;
	for (UStaticMeshComponent* Blade : WhirlBlades)
	{
		if (Blade != nullptr)
		{
			Blade->SetVisibility(false);
		}
	}
}



bool UVaelEngineerComponent::HandleL2(bool bHeld, const FVector& AimLocation)
{
	if (GetSetStage(TEXT("Aschenjaeger")) >= 2)
	{
		bWhirlHeld = bHeld;
		return true;
	}
	if (UsesBroodGun())
	{
		if (bHeld)
		{
			FireBroodGun(AimLocation, true);
		}
		return true;
	}
	return false;
}

// ---------------------------------------------------------------- the sting of the Glutkrone (Boris 26.11.)

namespace
{
	constexpr int32 StingBones = 6;
	/** Seconds of a strike: down into the ground, held there (the egg goes off at its start), back up */
	constexpr float StingDown = 0.35f;
	constexpr float StingHold = 0.3f;
	constexpr float StingUp = 0.45f;
	/** How far the strike swings the root and curls the rest (degrees), and the sway at rest */
	constexpr float StingStrikeRoot = 95.0f;
	constexpr float StingStrikeCurl = -16.0f;
	constexpr float StingSway = 4.0f;

	FTransform RestInMeshOf(const FReferenceSkeleton& Skeleton, int32 Index)
	{
		FTransform Result = FTransform::Identity;
		for (; Index != INDEX_NONE; Index = Skeleton.GetParentIndex(Index))
		{
			Result = Result * Skeleton.GetRefBonePose()[Index];
		}
		return Result;
	}

	/** The chain turned joint by joint about one axis (component space): each angle turns its bone and all after it */
	TArray<FTransform> BendChain(const TArray<FTransform>& Rest, const FVector& Axis, const TArray<float>& Degrees)
	{
		TArray<FTransform> Out = Rest;
		for (int32 K = 0; K < Out.Num() && K < Degrees.Num(); ++K)
		{
			if (FMath::IsNearlyZero(Degrees[K]))
			{
				continue;
			}
			const FQuat Turn(Axis, FMath::DegreesToRadians(Degrees[K]));
			const FVector Pivot = Out[K].GetLocation();
			for (int32 J = K; J < Out.Num(); ++J)
			{
				Out[J].SetRotation(Turn * Out[J].GetRotation());
				Out[J].SetLocation(Pivot + Turn.RotateVector(Out[J].GetLocation() - Pivot));
			}
		}
		return Out;
	}
}

bool UVaelEngineerComponent::HasSting() const
{
	return StingMesh != nullptr && StingMesh->IsVisible() && StingRest.Num() == StingBones;
}

void UVaelEngineerComponent::UpdateSting()
{
	AVaelCharacter* Player = GetPlayer();
	USkeletalMeshComponent* Chest = GlutpanzerBodies.IsValidIndex(1) ? GlutpanzerBodies[1].Get() : nullptr;
	USkeletalMesh* ChestMesh = Chest != nullptr && Chest->IsVisible() ? Chest->GetSkeletalMeshAsset() : nullptr;
	USkeletalMesh* Mesh = Player != nullptr && ChestMesh != nullptr && GetSetStage(TEXT("Schwarzglut")) >= 2
		? LoadObject<USkeletalMesh>(nullptr, TEXT("/Game/Vael/Characters/Ingenieur/Glutkrone/Koerper/SKM_Ing_GK_Stachel.SKM_Ing_GK_Stachel"), nullptr, LOAD_NoWarn | LOAD_Quiet)
		: nullptr;
	const FName Spine(TEXT("spine_05"));
	const int32 SpineIndex = ChestMesh != nullptr ? ChestMesh->GetRefSkeleton().FindBoneIndex(Spine) : INDEX_NONE;
	if (Mesh == nullptr || SpineIndex == INDEX_NONE)
	{
		if (StingMesh != nullptr)
		{
			StingMesh->SetVisibility(false);
		}
		return;
	}
	if (StingMesh == nullptr)
	{
		StingMesh = NewObject<UPoseableMeshComponent>(Player, TEXT("EngineerGlutkroneStachel"));
		StingMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		StingMesh->SetCastShadow(true);
		StingMesh->ComponentTags.Add(TEXT("VaelKeinZeichen"));
		StingMesh->RegisterComponent();
	}
	if (StingMesh->GetSkinnedAsset() != Mesh)
	{
		StingMesh->SetSkinnedAssetAndUpdate(Mesh);
	}
	// like the Glutpanzer's shell: the sting keeps the coordinates of the armour, hung on the chest's spine_05
	StingMesh->AttachToComponent(Chest, FAttachmentTransformRules::KeepRelativeTransform, Spine);
	StingMesh->SetRelativeTransform(RestInMeshOf(ChestMesh->GetRefSkeleton(), SpineIndex).Inverse());
	StingMesh->AddTickPrerequisiteComponent(Chest);
	StingMesh->SetVisibility(true);
	const FReferenceSkeleton& Skeleton = Mesh->GetRefSkeleton();
	StingRest.Reset();
	for (int32 Bone = 0; Bone < StingBones; ++Bone)
	{
		const int32 Index = Skeleton.FindBoneIndex(*FString::Printf(TEXT("Stachel_%d"), Bone));
		if (Index == INDEX_NONE)
		{
			StingRest.Reset();
			return;
		}
		StingRest.Add(RestInMeshOf(Skeleton, Index));
	}
	// the strike swings it about the axis across his back; the sign that brings the tip lowest
	FVector Back = StingRest[0].GetLocation() - RestInMeshOf(ChestMesh->GetRefSkeleton(), SpineIndex).GetLocation();
	Back.Z = 0.0f;
	StingAxis = FVector::CrossProduct(Back.GetSafeNormal(), FVector::UpVector).GetSafeNormal();
	TArray<float> Try;
	Try.Init(0.0f, StingBones);
	Try[0] = StingStrikeRoot;
	const float TipPlus = BendChain(StingRest, StingAxis, Try).Last().GetLocation().Z;
	Try[0] = -StingStrikeRoot;
	const float TipMinus = BendChain(StingRest, StingAxis, Try).Last().GetLocation().Z;
	StingStrikeSign = TipPlus < TipMinus ? 1.0f : -1.0f;
}

void UVaelEngineerComponent::TickSting(float DeltaTime)
{
	if (!HasSting())
	{
		return;
	}
	StingClock += DeltaTime;
	if (StingStrikeAge < 0.0f && PendingStrikes > 0)
	{
		--PendingStrikes;
		StingStrikeAge = 0.0f;
		bStingEggSent = false;
		// the egg comes out at the tip and rides it down (26.11., Boris: "das Ei, das durch den Schwanz in den Boden gebumpt wird")
		GlutEggs.RemoveAll([](const TWeakObjectPtr<AActor>& Egg) { return !Egg.IsValid(); });
		StingEgg.Reset();
		if (GlutEggs.Num() < MaxEggs)
		{
			if (AVaelGlutEgg* Egg = AVaelGlutEgg::Lay(this, StingMesh->GetComponentTransform().TransformPosition(StingRest.Last().GetLocation()), EggDamage * GetDamageScale() * GetTierScale()))
			{
				Egg->SetCarried(true);
				StingEgg = Egg;
				GlutEggs.Add(Egg);
				++EggsLaid;
			}
		}
	}
	float Strike = 0.0f;
	if (StingStrikeAge >= 0.0f)
	{
		StingStrikeAge += DeltaTime;
		const float T = StingStrikeAge;
		Strike = T < StingDown ? FMath::InterpEaseIn(0.0f, 1.0f, T / StingDown, 2.0f)
			: T < StingDown + StingHold ? 1.0f
			: FMath::InterpEaseOut(1.0f, 0.0f, FMath::Clamp((T - StingDown - StingHold) / StingUp, 0.0f, 1.0f), 2.0f);
		if (T >= StingDown + StingHold + StingUp)
		{
			StingStrikeAge = -1.0f;
		}
	}
	TArray<float> Degrees;
	Degrees.Init(0.0f, StingBones);
	for (int32 Bone = 0; Bone < StingBones; ++Bone)
	{
		// at rest a slow sway like a tail; striking, the root swings it down behind him and the rest curls the tip in
		const float Sway = StingSway * FMath::Sin(StingClock * 1.6f + Bone * 0.7f) * (1.0f - Strike);
		Degrees[Bone] = Sway + StingStrikeSign * Strike * (Bone == 0 ? StingStrikeRoot : StingStrikeCurl);
	}
	const TArray<FTransform> Pose = BendChain(StingRest, StingAxis, Degrees);
	for (int32 Bone = 0; Bone < StingBones; ++Bone)
	{
		StingMesh->SetBoneTransformByName(*FString::Printf(TEXT("Stachel_%d"), Bone), Pose[Bone], EBoneSpaces::ComponentSpace);
	}
	// the egg rides the tip of the sting on its way down
	if (AVaelGlutEgg* Riding = StingEgg.Get(); Riding != nullptr && !bStingEggSent)
	{
		// at the very tip: the last link carried on by its own length
		const FVector TipLocal = Pose.Last().GetLocation() * 2.0f - Pose[StingBones - 2].GetLocation();
		Riding->SetActorLocation(StingMesh->GetComponentTransform().TransformPosition(TipLocal));
	}
	// in the ground: a Glutei is shot under the earth where the tip went in
	if (StingStrikeAge >= StingDown && !bStingEggSent)
	{
		bStingEggSent = true;
		++StingStrikes;
		const FVector Tip = StingMesh->GetComponentTransform().TransformPosition(Pose.Last().GetLocation() * 2.0f - Pose[StingBones - 2].GetLocation());
		FHitResult Hit;
		const bool bGround = GetWorld()->LineTraceSingleByChannel(Hit, Tip + FVector(0.0f, 0.0f, 150.0f), Tip - FVector(0.0f, 0.0f, 400.0f), ECC_Visibility);
		const FVector Spot = bGround ? Hit.ImpactPoint : Tip;
		const UVaelMagicSettings* Magic = UVaelMagicSettings::Get();
		VaelEffects::SpawnAt(this, VaelAssets::LoadOptional(Magic->DustBurstEffect), Spot, FRotator::ZeroRotator, FLinearColor(0.9f, 0.4f, 0.1f), 110.0f);
		// the tip is in: it pushes the egg into the earth
		if (AVaelGlutEgg* Riding = StingEgg.Get())
		{
			Riding->PushInto(Spot);
		}
		StingEgg.Reset();
	}
}
#undef LOCTEXT_NAMESPACE
