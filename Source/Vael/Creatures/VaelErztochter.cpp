// Copyright Epic Games, Inc. All Rights Reserved.

#include "Creatures/VaelErztochter.h"

#include "Combat/VaelArcShot.h"
#include "Combat/VaelCombatStatics.h"
#include "Combat/VaelGroundStrike.h"
#include "Components/CapsuleComponent.h"
#include "Components/PoseableMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Creatures/VaelKobold.h"
#include "Creatures/VaelPoisonPuddle.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Items/VaelMaterial.h"
#include "Items/VaelMaterialBag.h"
#include "Magic/VaelGameplayTags.h"
#include "Magic/VaelSpellEffects.h"
#include "Player/VaelCharacter.h"
#include "Vael.h"

#define LOCTEXT_NAMESPACE "VaelErztochter"

namespace
{
	struct FTochterRow { const TCHAR* Bone; const TCHAR* Zone; FVector Location; FVector Normal; FVector Surface; float Radius; float Depth; };
	const FTochterRow TochterRows[] = {
#include "Creatures/VaelErztochterFassungen.inl"
	};

	struct FBogenRow { const TCHAR* Ore; const TCHAR* Mesh; float Width; float Length; };
	const FBogenRow BogenRows[] = {
#include "Creatures/VaelErztochterBogen.inl"
	};

	const FName ZoneKamm(TEXT("Kamm"));
	const FName ZoneKammFuss(TEXT("KammFuss"));
	const FName ZoneKammBelag(TEXT("KammBelag"));
	const FName ZoneFluegelBelag(TEXT("FluegelBelag"));

	/** The most elongated pieces of an ore (at least one): the coat on horns and stumps grows in long lines */
	TArray<const FVaelAmeisenKristall*> LongPieces(TArray<const FVaelAmeisenKristall*> Pieces)
	{
		Pieces.Sort([](const FVaelAmeisenKristall& A, const FVaelAmeisenKristall& B) { return A.Length / A.Width > B.Length / B.Width; });
		Pieces.SetNum(FMath::Max(1, (Pieces.Num() + 1) / 2));
		return Pieces;
	}
	const FName ZoneBein(TEXT("Bein"));
	const FName ZoneKopf(TEXT("Kopf"));
	const FName ZoneSattel(TEXT("Sattel"));
	const FName ZoneHinterleib(TEXT("Hinterleib"));
	const FName ZoneFluegel(TEXT("Fluegel"));
}

UVaelErztochterData::UVaelErztochterData()
{
	DisplayName = LOCTEXT("Erztochter", "Erztochter");
	CompendiumId = TEXT("Erztochter");
	KindId = TEXT("Erztochter");
	CreatureClass = AVaelErztochter::StaticClass();

	// Eyes on both sides of the head, behind the feelers (measured on SK_Erztochter in Blender)
	EyeLocations = {FVector(0.166f, -0.675f, 0.875f), FVector(-0.1875f, -0.675f, 0.875f)};
	EyeNormals = {FVector(0.967f, -0.202f, 0.155f), FVector(-0.84f, -0.45f, 0.302f)};
	Mesh = TSoftObjectPtr<USkeletalMesh>(FSoftObjectPath(TEXT("/Game/Vael/Kreaturen/Eisenschlund/Erztochter/SK_Erztochter.SK_Erztochter")));

	MaxHealth = 520.0f;
	MoveSpeed = 240.0f;
	CollisionRadius = 95.0f;
	CollisionHalfHeight = 70.0f;
	KnockbackMultiplier = 0.25f;
	AggroRange = 1600.0f;
	BiteDamage = 18.0f;
	BiteReach = 130.0f;
	BiteInterval = 1.7f;
	WanderRadius = 350.0f;
	BodyColor = FLinearColor(0.90f, 0.80f, 0.62f);

	// Her own settings; the crystal pieces are the ones of the workers
	Fassungen.Reset();
	for (const FTochterRow& Row : TochterRows)
	{
		FVaelAmeisenFassung& Fassung = Fassungen.AddDefaulted_GetRef();
		Fassung.Bone = Row.Bone;
		Fassung.Zone = Row.Zone;
		Fassung.Location = Row.Location;
		Fassung.Normal = Row.Normal;
		Fassung.Surface = Row.Surface;
		Fassung.Radius = Row.Radius;
		Fassung.Depth = Row.Depth;
	}
	for (const FBogenRow& Row : BogenRows)
	{
		FVaelAmeisenKristall& Kristall = BentKristalle.AddDefaulted_GetRef();
		Kristall.Ore = Row.Ore;
		Kristall.Mesh = TSoftObjectPtr<UStaticMesh>(FSoftObjectPath(Row.Mesh));
		Kristall.Width = Row.Width;
		Kristall.Length = Row.Length;
	}
}

TSubclassOf<UVaelCreatureData> AVaelErztochter::GetDefaultDataClass() const
{
	return UVaelErztochterData::StaticClass();
}

void AVaelErztochter::BeginPlay()
{
	Super::BeginPlay();
	SetElite();
}

void AVaelErztochter::GetPairOres(int32 InPair, FName& OutLead, FName& OutCompanion)
{
	static const TCHAR* const Pairs[5][2] = {
		{TEXT("Schwefelkies"), TEXT("Funkenerz")},
		{TEXT("Rosteisen"), TEXT("Salzspat")},
		{TEXT("Gruenspan"), TEXT("Drahtsilber")},
		{TEXT("Blutzinnober"), TEXT("Siechglas")},
		// Their harvest is the protection money of the Kobolde: Kobaltbluete leads
		{TEXT("Kobaltbluete"), TEXT("Rippenspat")},
	};
	const int32 Index = FMath::Clamp(InPair, 1, 5) - 1;
	OutLead = Pairs[Index][0];
	OutCompanion = Pairs[Index][1];
}

void AVaelErztochter::SetupDaughter(int32 InPair, bool bInEnslaved)
{
	Pair = FMath::Clamp(InPair, 1, 5);
	FName Lead, Companion;
	GetPairOres(Pair, Lead, Companion);
	SetupAnt(Lead, false, bInEnslaved);
}

void AVaelErztochter::BuildCrystals()
{
	ClearCrystals();

	const UPoseableMeshComponent* QueenMesh = GetFactoryMesh();
	const UVaelErztochterData* Data = GetData<UVaelErztochterData>();
	if (QueenMesh == nullptr || QueenMesh->GetSkinnedAsset() == nullptr)
	{
		return;
	}
	FName Lead, Companion;
	GetPairOres(Pair, Lead, Companion);
	Ore = Lead;

	TArray<const FVaelAmeisenKristall*> LeadPieces, CompanionPieces;
	for (const FVaelAmeisenKristall& Kristall : Data->Kristalle)
	{
		if (Kristall.Ore == Lead)
		{
			LeadPieces.Add(&Kristall);
		}
		else if (Kristall.Ore == Companion)
		{
			CompanionPieces.Add(&Kristall);
		}
	}
	if (LeadPieces.IsEmpty() || CompanionPieces.IsEmpty())
	{
		UE_LOG(LogVael, Warning, TEXT("Erztochter: no crystals for %s / %s"), *Lead.ToString(), *Companion.ToString());
		return;
	}

	// The coat uses the bent pieces (Tools/Ameisen/kristalle_biegen.py), the most elongated ones; straight ones if missing
	TArray<const FVaelAmeisenKristall*> LeadBent, CompanionBent;
	for (const FVaelAmeisenKristall& Kristall : Data->BentKristalle)
	{
		if (Kristall.Ore == Lead)
		{
			LeadBent.Add(&Kristall);
		}
		else if (Kristall.Ore == Companion)
		{
			CompanionBent.Add(&Kristall);
		}
	}
	const TArray<const FVaelAmeisenKristall*> LeadLong = LongPieces(LeadBent.IsEmpty() ? LeadPieces : LeadBent);
	const TArray<const FVaelAmeisenKristall*> CompanionLong = LongPieces(CompanionBent.IsEmpty() ? CompanionPieces : CompanionBent);

	FRandomStream Random(CrystalSeed);
	for (const FVaelAmeisenFassung& Fassung : Data->Fassungen)
	{
		bool bLead = true;
		float Age = Data->CrystalGrowth;
		float LengthScale = 1.0f;
		const bool bCoat = Fassung.Zone == ZoneKammBelag || Fassung.Zone == ZoneKammFuss || Fassung.Zone == ZoneFluegelBelag;
		if (Fassung.Zone == ZoneKammBelag)
		{
			// Horns overgrown by the lead ore: close rows leaning to the tip, the horn itself hardly shows
			Age = 0.0f;
			LengthScale = Data->CoatLength;
		}
		else if (Fassung.Zone == ZoneFluegelBelag)
		{
			// Stumps overgrown by the companion ore, leaning outwards along the stump
			bLead = false;
			Age = 0.0f;
			LengthScale = Data->CoatLength;
		}
		else if (Fassung.Zone == ZoneKamm)
		{
			// The crown: long spikes of the lead ore along both horns
			Age = 0.0f;
			LengthScale = Data->CrestLength;
		}
		else if (Fassung.Zone == ZoneKammFuss)
		{
			// The foot of the horns: the companion ore, a little shorter
			bLead = false;
			Age = 0.0f;
			LengthScale = Data->CoatLength * 0.8f;
		}
		else if (Fassung.Zone == ZoneBein)
		{
			// A few small ones on the legs, both ores
			bLead = Random.FRand() < 0.5f;
			Age = 0.0f;
			LengthScale = 1.2f;
		}
		else if (Fassung.Zone == ZoneKopf)
		{
			bLead = false;
			Age = Data->CrystalGrowth * 0.5f;
		}
		else if (Fassung.Zone == ZoneSattel)
		{
			// Middle of the saddle lead, rims companion
			bLead = FMath::Abs(Fassung.Location.X) < Data->SaddleCore;
		}
		else if (Fassung.Zone == ZoneHinterleib)
		{
			// The ores take turns band by band down the abdomen
			bLead = FMath::FloorToInt(Fassung.Location.Y / Data->AbdomenBand) % 2 == 0;
		}
		else if (Fassung.Zone == ZoneFluegel)
		{
			// Splinters along the torn edges of the stumps
			bLead = false;
			Age = 0.0f;
			LengthScale = Data->WingLength;
		}
		if ((Fassung.Zone == ZoneKopf || Fassung.Zone == ZoneSattel || Fassung.Zone == ZoneHinterleib) && Random.FRand() < Data->MixChance)
		{
			bLead = !bLead;
		}
		const TArray<const FVaelAmeisenKristall*>& Pieces = bCoat ? (bLead ? LeadLong : CompanionLong) : (bLead ? LeadPieces : CompanionPieces);
		// Rooted in the body: the coat deepest (it lies flat), the settings a little below their floor
		const float Sink = bCoat ? Data->CoatSink : Data->CrystalSink;
		PlaceCrystal(Fassung, *Pieces[Random.RandRange(0, Pieces.Num() - 1)], Age, LengthScale, Random, bCoat ? Data->CoatJitter : 1.0f,
			bCoat && !LeadBent.IsEmpty() && !CompanionBent.IsEmpty(), Sink);
	}
	UE_LOG(LogVael, Log, TEXT("Erztochter %s: %d crystals (%s + %s)"), *GetName(), CrystalCount, *Lead.ToString(), *Companion.ToString());
	BuildEyes();
}

void AVaelErztochter::BuildEyes()
{
	for (UStaticMeshComponent* Part : EyeParts)
	{
		if (Part != nullptr)
		{
			Part->DestroyComponent();
		}
	}
	EyeParts.Reset();
	Eyes.Reset();

	UPoseableMeshComponent* QueenMesh = GetFactoryMesh();
	const UVaelErztochterData* Data = GetData<UVaelErztochterData>();
	if (QueenMesh == nullptr || QueenMesh->GetSkinnedAsset() == nullptr)
	{
		return;
	}
	UMaterialInterface* CoreMaterial = nullptr;
	UMaterialInterface* GlowMaterial = nullptr;
	VaelEffects::LoadLookMaterials(CoreMaterial, GlowMaterial);
	const FLinearColor Color = bEnslaved ? Data->EnslavedEyeColor : Data->EyeColor;
	const FName HeadBone(TEXT("head"));
	const FTransform BoneTransform = QueenMesh->GetBoneTransformByName(HeadBone, EBoneSpaces::ComponentSpace);
	const float Radius = Data->EyeRadius * 100.0f;

	for (int32 Index = 0; Index < FMath::Min(Data->EyeLocations.Num(), Data->EyeNormals.Num()); ++Index)
	{
		// Blender model space (metres) to the mesh (cm, Y flips on import)
		const FVector& L = Data->EyeLocations[Index];
		const FVector& N = Data->EyeNormals[Index];
		FEye& Eye = Eyes.AddDefaulted_GetRef();
		Eye.Normal = FVector(N.X, -N.Y, N.Z).GetSafeNormal();
		// The eyeball bulges a little out of its socket
		Eye.Centre = FVector(L.X, -L.Y, L.Z) * 100.0f - Eye.Normal * Radius * 0.45f;
		Eye.Bone = BoneTransform;

		Eye.Ball = VaelEffects::AddLookShape(this, CoreMaterial, Color, Data->EyeGlow, 0.6f);
		Eye.Core = VaelEffects::AddLookShape(this, GlowMaterial, Color, Data->EyeGlow * 2.0f);
		for (UStaticMeshComponent* Part : {Eye.Ball, Eye.Core})
		{
			Part->AttachToComponent(QueenMesh, FAttachmentTransformRules::KeepRelativeTransform, HeadBone);
			Part->SetVisibility(true);
			EyeParts.Add(Part);
		}
		// Oval, a little flat along its axis (the sphere mesh is 100 cm across)
		const FQuat Facing = FQuat::FindBetweenNormals(FVector::UpVector, Eye.Normal);
		const FTransform BallInMesh(Facing, Eye.Centre, FVector(1.15f, 0.95f, 0.8f) * (2.0f * Radius / 100.0f));
		Eye.Ball->SetRelativeTransform(BallInMesh.GetRelativeTransform(Eye.Bone));
	}
	UpdateEyes(0.0f);
}

void AVaelErztochter::OnDamageTaken(float Damage, const FGameplayTagContainer& DamageTags)
{
	Super::OnDamageTaken(Damage, DamageTags);
	EyeFlare = 1.0f;
	if (IsDead() || Damage <= 0.0f)
	{
		return;
	}
	const UVaelErztochterData* Data = GetData<UVaelErztochterData>();
	CastDamage += Damage;

	// Hits wear the wing stumps down, one side after the other; earth and frost shatter the crystal crust twice as fast
	const int32 Side = !bStumpBroken[0] ? 0 : !bStumpBroken[1] ? 1 : INDEX_NONE;
	if (Side != INDEX_NONE)
	{
		const bool bShatter = DamageTags.HasTagExact(VaelTags::Element_Earth) || UVaelCombatStatics::HasStatus(this, EVaelStatus::Frozen);
		StumpDamage[Side] += Damage * Data->StumpHitShare * (bShatter ? 2.0f : 1.0f);
		if (StumpDamage[Side] >= GetMaxHealth() * Data->StumpShare)
		{
			BreakStump(Side);
		}
	}
}

void AVaelErztochter::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (Action == EAction::JumpAir && !IsDead())
	{
		// The flight runs here, not in the behavior: it must not stop when she is hit
		const UVaelErztochterData* Data = GetData<UVaelErztochterData>();
		ActionTime += DeltaSeconds;
		const float Alpha = FMath::Clamp(ActionTime / Data->JumpFlight, 0.0f, 1.0f);
		const float Height = FMath::Sin(Alpha * UE_PI) * FMath::Max(180.0f, FVector::Dist2D(JumpStart, JumpEnd) * 0.35f);
		SetActorLocation(FMath::Lerp(JumpStart, JumpEnd, Alpha) + FVector(0.0f, 0.0f, Height), false, nullptr, ETeleportType::TeleportPhysics);
		if (Alpha >= 1.0f)
		{
			GetCharacterMovement()->SetMovementMode(MOVE_Walking);
			Action = EAction::None;
			WingLift = 0.0f;
		}
	}
	PoseWings(DeltaSeconds);
	UpdateEyes(DeltaSeconds);
}

void AVaelErztochter::TickBehavior(float DeltaSeconds)
{
	const UVaelErztochterData* Data = GetData<UVaelErztochterData>();
	if (!bEscortSpawned)
	{
		SpawnEscort();
	}
	ResonanceTimer -= DeltaSeconds;
	ShardTimer -= DeltaSeconds;
	JumpTimer -= DeltaSeconds;
	BiteTimer -= DeltaSeconds;

	// Kobaltbluete daughter: their harvest is the protection money of the Kobolde, she hates them on sight
	if (Pair == 5 && !IsAngry())
	{
		for (TActorIterator<AVaelKobold> It(GetWorld()); It; ++It)
		{
			if (!It->IsDead() && FVector::Dist2D(It->GetActorLocation(), GetActorLocation()) < Data->AggroRange)
			{
				Anger();
				break;
			}
		}
	}

	switch (Action)
	{
	case EAction::Resonance:
		ActionTime += DeltaSeconds;
		WingShake = 1.0f;
		WingLift = FMath::Min(1.0f, WingLift + DeltaSeconds * 3.0f);
		EyeFlare = FMath::Max(EyeFlare, 0.35f);
		if (CastDamage >= GetMaxHealth() * Data->ResonanceInterrupt)
		{
			// Broken off: she stumbles and the call fails
			Action = EAction::None;
			WingShake = 0.0f;
			WingLift = 0.0f;
			ResonanceTimer = Data->ResonanceCooldown * 0.5f;
			Stagger(1.2f);
		}
		else if (ActionTime >= Data->ResonanceCast)
		{
			FinishResonance();
		}
		return;
	case EAction::Shards:
		ActionTime += DeltaSeconds;
		WingLift = FMath::Min(1.0f, WingLift + DeltaSeconds * 2.5f);
		WingShake = 0.4f;
		if (ActionTime >= Data->ShardWindup)
		{
			FireShards();
		}
		return;
	case EAction::JumpWindup:
		ActionTime += DeltaSeconds;
		WingLift = FMath::Min(1.0f, WingLift + DeltaSeconds * 2.0f);
		WingShake = 0.7f;
		if (ActionTime >= Data->JumpWindup)
		{
			Action = EAction::JumpAir;
			ActionTime = 0.0f;
			JumpStart = GetActorLocation();
			GetCharacterMovement()->StopMovementImmediately();
			GetCharacterMovement()->SetMovementMode(MOVE_Flying);
		}
		return;
	case EAction::JumpAir:
		return;
	default:
		break;
	}
	WingShake = FMath::Max(0.0f, WingShake - DeltaSeconds * 3.0f);
	WingLift = FMath::Max(0.0f, WingLift - DeltaSeconds * 2.0f);

	const bool bFighting = bEnslaved || IsAngry();
	float Distance = 0.0f;
	AActor* Target = bFighting ? FindTarget(Data->AggroRange, &Distance) : nullptr;
	if (Target == nullptr)
	{
		// At work like her workers: wander about her place
		Super::TickBehavior(DeltaSeconds);
		return;
	}

	const int32 Stumps = GetIntactStumps();
	if (Stumps > 0 && ResonanceTimer <= 0.0f && CountColony() < Data->ResonanceColony)
	{
		StartResonance();
		return;
	}
	if (ShardTimer <= 0.0f && Distance < Data->ShardRange)
	{
		StartShards(Target);
		return;
	}
	if (Stumps > 0 && JumpTimer <= 0.0f && Distance > Data->JumpMinDistance && Distance < Data->JumpMaxDistance)
	{
		StartJump(Target);
		return;
	}

	// Close in and bite; without her stumps she rages
	const float Rage = IsRaging() ? Data->RageSpeed : 1.0f;
	const float Reach = Data->CollisionRadius * GetActorScale3D().X + Data->BiteReach;
	if (Distance > Reach)
	{
		MoveTowards(Target->GetActorLocation(), Data->MoveSpeed * Rage);
	}
	else
	{
		FaceTowards(Target->GetActorLocation());
		if (BiteTimer <= 0.0f)
		{
			BiteTimer = Data->BiteInterval * (IsRaging() ? Data->RageBiteInterval : 1.0f);
			HitPlayer(Target, Data->BiteDamage, EVaelElement::Earth);
		}
	}
}

void AVaelErztochter::SpawnEscort()
{
	bEscortSpawned = true;
	const UVaelErztochterData* Data = GetData<UVaelErztochterData>();
	FName Lead, Companion;
	GetPairOres(Pair, Lead, Companion);
	for (int32 Index = 0; Index < Data->EscortCount; ++Index)
	{
		const float Angle = UE_TWO_PI * Index / FMath::Max(1, Data->EscortCount) + FMath::FRandRange(-0.4f, 0.4f);
		const FVector Around = GetActorLocation() + FVector(FMath::Cos(Angle), FMath::Sin(Angle), 0.0f) * FMath::FRandRange(250.0f, 420.0f);
		FVector Ground;
		if (!AVaelCreature::FindGround(GetWorld(), Around + FVector(0.0f, 0.0f, 300.0f), Ground))
		{
			continue;
		}
		if (AVaelSchuerfameise* Ant = Cast<AVaelSchuerfameise>(AVaelCreature::SpawnCreature(GetWorld(), GetMutableDefault<UVaelSchuerfameiseData>(), Ground, GetActorRotation())))
		{
			Ant->SetupAnt(Lead, false, bEnslaved);
		}
	}
}

int32 AVaelErztochter::CountColony() const
{
	const float Range = GetData<UVaelErztochterData>()->ResonanceRange;
	int32 Count = 0;
	for (TActorIterator<AVaelSchuerfameise> It(GetWorld()); It; ++It)
	{
		if (*It != this && !It->IsA<AVaelErztochter>() && !It->IsDead() && It->GetOre() == Ore
			&& FVector::Dist2D(It->GetActorLocation(), GetActorLocation()) < Range)
		{
			++Count;
		}
	}
	return Count;
}

void AVaelErztochter::StartResonance()
{
	Action = EAction::Resonance;
	ActionTime = 0.0f;
	CastDamage = 0.0f;
	ResonanceTimer = GetData<UVaelErztochterData>()->ResonanceCooldown;
}

void AVaelErztochter::FinishResonance()
{
	const UVaelErztochterData* Data = GetData<UVaelErztochterData>();
	Action = EAction::None;
	WingShake = 0.0f;
	EyeFlare = 1.0f;

	// Her colony hears her: the ants of her ore around join the fight, a few more come out of the rock
	for (TActorIterator<AVaelSchuerfameise> It(GetWorld()); It; ++It)
	{
		if (*It == this || It->IsA<AVaelErztochter>() || It->IsDead() || It->GetOre() != Ore
			|| FVector::Dist2D(It->GetActorLocation(), GetActorLocation()) > Data->ResonanceRange)
		{
			continue;
		}
		if (Pair == 2 && It->IsEnslaved() && !bEnslaved)
		{
			// Salzspat dries the Tropfseuche out: enslaved ants of her colony come free and back to her
			It->SetupAnt(It->GetOre(), It->IsAncestor(), false);
		}
		It->Anger();
	}
	const int32 Missing = FMath::Min(Data->ResonanceReinforcements, Data->ResonanceColony - CountColony());
	for (int32 Index = 0; Index < Missing; ++Index)
	{
		const FVector2D Offset = FMath::RandPointInCircle(1.0f).GetSafeNormal() * FMath::FRandRange(350.0f, 600.0f);
		FVector Ground;
		if (AVaelCreature::FindGround(GetWorld(), GetActorLocation() + FVector(Offset, 300.0f), Ground))
		{
			if (AVaelSchuerfameise* Ant = Cast<AVaelSchuerfameise>(AVaelCreature::SpawnCreature(GetWorld(), GetMutableDefault<UVaelSchuerfameiseData>(), Ground, GetActorRotation())))
			{
				Ant->SetupAnt(Ore, false, bEnslaved);
				Ant->Anger();
			}
		}
	}

	// Drahtsilber sings in the stumps: the sound confuses the players near her
	if (Pair == 3)
	{
		ForEachActivePlayer([this](AVaelCharacter* Player)
		{
			if (FVector::Dist2D(Player->GetActorLocation(), GetActorLocation()) < 900.0f)
			{
				UVaelCombatStatics::ApplyStatus(this, Player, EVaelStatus::Confused, 2.5f);
			}
		});
	}
}

void AVaelErztochter::StartShards(const AActor* Target)
{
	Action = EAction::Shards;
	ActionTime = 0.0f;
	ShardTimer = GetData<UVaelErztochterData>()->ShardCooldown;
	ShardAim = (Target->GetActorLocation() - GetActorLocation()).GetSafeNormal2D();
	FaceTowards(Target->GetActorLocation());
}

void AVaelErztochter::FireShards()
{
	const UVaelErztochterData* Data = GetData<UVaelErztochterData>();
	Action = EAction::None;
	FName Lead, Companion;
	GetPairOres(Pair, Lead, Companion);
	const FLinearColor Color = VaelOre::GetColor(Companion);

	// What a shard does depends on the pair
	FVaelSpellHit Hit;
	Hit.Damage = Data->ShardDamage;
	Hit.Element = EVaelElement::Earth;
	switch (Pair)
	{
	case 1:   // Funkenerz sparks set the Schwefelkies fumes alight
		Hit.Element = EVaelElement::Fire;
		Hit.Status = EVaelStatus::Burning;
		Hit.StatusDuration = 4.0f;
		Hit.StatusDamagePerSecond = 5.0f;
		break;
	case 2:   // Rust eats the armour: the one hit takes more damage for a while
		Hit.Status = EVaelStatus::Marked;
		Hit.StatusDuration = 5.0f;
		break;
	case 3:   // Gruenspan carries lightning: twice on the wet
		Hit.Element = EVaelElement::Air;
		Hit.bLightning = true;
		break;
	case 4:   // Quecksilber from the Blutzinnober: slippery and slow; Siechglas poisons the ground
		Hit.Status = EVaelStatus::Slowed;
		Hit.StatusDuration = 3.0f;
		break;
	default:  // Rippenspat: ribs close around the one hit
		Hit.StunDuration = 1.1f;
		Hit.bBoneCage = true;
		break;
	}

	// Fewer shards for every broken stump; every third time a ring all around instead of a fan
	const int32 Count = FMath::Max(2, FMath::RoundToInt(Data->ShardCount * (0.4f + 0.3f * GetIntactStumps())));
	const bool bRing = (++ShardCasts % 3) == 0;
	const FVector From = GetActorLocation() + FVector(0.0f, 0.0f, 150.0f * GetActorScale3D().Z);
	constexpr float Flight = 0.55f;
	for (int32 Index = 0; Index < Count; ++Index)
	{
		const float Angle = bRing ? 360.0f * Index / Count
			: FMath::Lerp(-Data->ShardCone * 0.5f, Data->ShardCone * 0.5f, Count > 1 ? static_cast<float>(Index) / (Count - 1) : 0.5f);
		const FVector Direction = ShardAim.RotateAngleAxis(Angle + FMath::FRandRange(-5.0f, 5.0f), FVector::UpVector);
		const float Reach = bRing ? FMath::FRandRange(350.0f, 600.0f) : FMath::FRandRange(Data->ShardRange * 0.45f, Data->ShardRange);
		FVector Ground;
		if (!AVaelCreature::FindGround(GetWorld(), GetActorLocation() + Direction * Reach + FVector(0.0f, 0.0f, 300.0f), Ground))
		{
			continue;
		}
		AVaelArcShot::Lob(GetWorld(), From, Ground, Flight, 140.0f, Color, 20.0f);
		AVaelGroundStrike::SpawnStrike(this, Ground, Hit, Data->ShardRadius, Flight, Color);
		if (Pair == 4 && Index % 3 == 1)
		{
			AVaelPoisonPuddle::Spit(GetWorld(), Ground, Flight, this);
		}
	}
	WingShake = 1.0f;
}

void AVaelErztochter::StartJump(const AActor* Target)
{
	const UVaelErztochterData* Data = GetData<UVaelErztochterData>();
	FVector Ground;
	if (!AVaelCreature::FindGround(GetWorld(), Target->GetActorLocation() + FVector(0.0f, 0.0f, 300.0f), Ground))
	{
		JumpTimer = 2.0f;
		return;
	}
	Action = EAction::JumpWindup;
	ActionTime = 0.0f;
	JumpTimer = Data->JumpCooldown;
	FaceTowards(Target->GetActorLocation());
	// She lands with her capsule on the ground; the warning shows where from the start
	JumpEnd = Ground + FVector(0.0f, 0.0f, GetCapsuleComponent()->GetScaledCapsuleHalfHeight());
	FVaelSpellHit Hit;
	Hit.Damage = Data->JumpDamage;
	Hit.Element = EVaelElement::Earth;
	Hit.Knockback = 900.0f;
	FName Lead, Companion;
	GetPairOres(Pair, Lead, Companion);
	AVaelGroundStrike::SpawnStrike(this, Ground, Hit, Data->JumpRadius, Data->JumpWindup + Data->JumpFlight, VaelOre::GetColor(Lead));
}

void AVaelErztochter::BreakStump(int32 Side)
{
	if (bStumpBroken[Side])
	{
		return;
	}
	bStumpBroken[Side] = true;
	const FName Bone = Side == 0 ? TEXT("wing_L") : TEXT("wing_R");
	UPoseableMeshComponent* QueenMesh = GetFactoryMesh();
	if (QueenMesh != nullptr)
	{
		ThrowShards(QueenMesh->GetBoneLocationByName(Bone, EBoneSpaces::WorldSpace), 6, 12.0f);
	}

	// The crust of the stump is ore: every player gets some
	FName Lead, Companion;
	GetPairOres(Pair, Lead, Companion);
	UVaelMaterialBag::GiveToGroup(this, {FVaelLootEntry(*VaelOre::GetMaterialPath(Companion), 1.0f, 2, 3), FVaelLootEntry(*VaelOre::GetMaterialPath(Lead), 0.5f)});

	// A call or leap in progress fails
	if (Action == EAction::Resonance || Action == EAction::Shards || Action == EAction::JumpWindup)
	{
		Action = EAction::None;
	}
	Stagger(1.0f);
	EyeFlare = 1.0f;
	if (IsRaging())
	{
		Anger();
	}
	UE_LOG(LogVael, Log, TEXT("Erztochter %s: wing stump %d broken%s"), *GetName(), Side, IsRaging() ? TEXT(", she rages") : TEXT(""));
}

void AVaelErztochter::PoseWings(float DeltaSeconds)
{
	UPoseableMeshComponent* QueenMesh = GetFactoryMesh();
	if (QueenMesh == nullptr || QueenMesh->GetSkinnedAsset() == nullptr)
	{
		return;
	}
	WingTime += DeltaSeconds;
	const FReferenceSkeleton& RefSkeleton = QueenMesh->GetSkinnedAsset()->GetRefSkeleton();
	for (int32 Side = 0; Side < 2; ++Side)
	{
		const int32 Index = RefSkeleton.FindBoneIndex(Side == 0 ? FName(TEXT("wing_L")) : FName(TEXT("wing_R")));
		if (Index == INDEX_NONE || !QueenMesh->BoneSpaceTransforms.IsValidIndex(Index))
		{
			continue;
		}
		FTransform Local = RefSkeleton.GetRefBonePose()[Index];
		if (bStumpBroken[Side])
		{
			// Broken off: the stump and its crystals are gone
			Local.SetScale3D(FVector(0.01f));
		}
		else
		{
			// Raised for a call or a throw, humming fast while she calls
			const float Sign = Side == 0 ? 1.0f : -1.0f;
			const float Lift = FMath::DegreesToRadians(-22.0f * WingLift);
			const float Shake = FMath::DegreesToRadians(4.0f * WingShake * FMath::Sin(WingTime * 55.0f));
			Local.SetRotation(Local.GetRotation() * FQuat(FVector::ForwardVector, (Lift + Shake) * Sign));
		}
		QueenMesh->BoneSpaceTransforms[Index] = Local;
	}
	QueenMesh->MarkRefreshTransformDirty();
}

void AVaelErztochter::UpdateEyes(float DeltaSeconds)
{
	if (Eyes.IsEmpty() || GetFactoryMesh() == nullptr)
	{
		return;
	}
	const UVaelErztochterData* Data = GetData<UVaelErztochterData>();
	EyeTime += DeltaSeconds;
	EyeFlare = FMath::Max(0.0f, EyeFlare - DeltaSeconds * 2.5f);
	EyeAnger = FMath::FInterpTo(EyeAnger, (bEnslaved || IsAngry()) ? 1.0f : 0.0f, DeltaSeconds, 2.0f);
	EyeLife = FMath::FInterpTo(EyeLife, IsDead() ? 0.0f : 1.0f, DeltaSeconds, 0.8f);

	// A slow breath of light, a little faster when she fights; a flare when hit; it fades when she dies
	const float Breath = 0.78f + 0.22f * FMath::Sin(EyeTime * (1.1f + EyeAnger * 1.6f));
	const float Glow = (FMath::Lerp(Data->EyeGlow, Data->AngryEyeGlow, EyeAnger) * Breath + EyeFlare * 6.0f) * EyeLife;

	// The light inside drifts slowly while she works and turns to her target when she fights
	const AActor* Target = (EyeAnger > 0.2f && EyeLife > 0.5f) ? FindTarget(Data->AggroRange) : nullptr;
	const FTransform MeshToWorld = GetFactoryMesh()->GetComponentTransform();
	const float Radius = Data->EyeRadius * 100.0f;
	for (int32 Index = 0; Index < Eyes.Num(); ++Index)
	{
		FEye& Eye = Eyes[Index];
		FVector Side1, Side2;
		Eye.Normal.FindBestAxisVectors(Side1, Side2);
		FVector Wish = (Side1 * FMath::Sin(EyeTime * 0.37f + Index * 2.1f) + Side2 * FMath::Sin(EyeTime * 0.23f + Index)) * 0.5f;
		if (Target != nullptr)
		{
			const FVector ToTarget = MeshToWorld.InverseTransformVectorNoScale(Target->GetActorLocation() - MeshToWorld.TransformPosition(Eye.Centre)).GetSafeNormal();
			Wish = ToTarget - Eye.Normal * FVector::DotProduct(ToTarget, Eye.Normal);
		}
		Eye.Gaze = DeltaSeconds > 0.0f ? FMath::VInterpTo(Eye.Gaze, Wish.GetClampedToMaxSize(1.0f), DeltaSeconds, Target != nullptr ? 5.0f : 1.2f) : Wish.GetClampedToMaxSize(1.0f);

		const FVector CoreAt = Eye.Centre + Eye.Normal * Radius * 0.5f + Eye.Gaze * Radius * 0.35f;
		const float CoreSize = (0.42f + 0.08f * Breath + EyeFlare * 0.25f) * (2.0f * Radius / 100.0f) * FMath::Max(0.05f, EyeLife);
		Eye.Core->SetRelativeTransform(FTransform(FQuat::Identity, CoreAt, FVector(CoreSize)).GetRelativeTransform(Eye.Bone));
		VaelEffects::SetLookGlow(Eye.Ball, Glow);
		VaelEffects::SetLookGlow(Eye.Core, Glow * 2.2f);
	}
}

#undef LOCTEXT_NAMESPACE
