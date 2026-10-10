// Copyright Epic Games, Inc. All Rights Reserved.

#include "Creatures/VaelEmberCrawler.h"
#include "Components/PoseableMeshComponent.h"
#include "Creatures/VaelLegIKComponent.h"
#include "AbilitySystemComponent.h"
#include "Combat/VaelCombatStatics.h"
#include "Components/CapsuleComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Creatures/VaelCreatureData.h"
#include "DrawDebugHelpers.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Magic/VaelGameplayTags.h"
#include "Magic/VaelDebrisBurst.h"
#include "Magic/VaelGroundArea.h"
#include "Magic/VaelSpellEffects.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Player/VaelCharacter.h"
#include "UI/VaelCombatTextSubsystem.h"
#include "Combat/VaelHitFeedbackSubsystem.h"
#include "Vael.h"

#define LOCTEXT_NAMESPACE "VaelCreatures"

namespace
{
	/** The burrowed body is squashed to this share of its height */
	constexpr float BurrowedHeightScale = 0.2f;

	/** The crawler stops walking home closer than this, in cm */
	constexpr float CrawlerHomeTolerance = 140.0f;

	/** Diameter of the engine sphere and height of the engine cone in cm */
	constexpr float CrawlerShapeSize = 100.0f;

	/** Size of the glowing gland in cm */
	const FVector CrawlerGlandSize(66.0f, 58.0f, 44.0f);

	/** The legs: hips along the body front to back, how far out the feet stand, stride and lift of a step, height of the knees, in cm */
	constexpr float CrawlerHipX[] = { 22.0f, 2.0f, -18.0f };
	constexpr float CrawlerFootSpreadX[] = { 16.0f, 0.0f, -16.0f };
	constexpr float CrawlerHipY = 24.0f;
	constexpr float CrawlerFootY = 70.0f;
	constexpr float CrawlerStride = 34.0f;
	constexpr float CrawlerStepLift = 16.0f;
	constexpr float CrawlerKneeHeight = 30.0f;

	/** Width of the upper and lower segment of a leg in cm */
	constexpr float CrawlerUpperLegWidth = 10.0f;
	constexpr float CrawlerLowerLegWidth = 7.0f;

	/** Seconds the pincers need to snap shut and open again, and how far the body lunges with them in cm */
	constexpr float CrawlerSnapTime = 0.3f;
	constexpr float CrawlerLunge = 14.0f;

	/** Seconds between two puffs of dust while burrowed, sparks while the fuse burns, steam while doused */
	constexpr float CrawlerDustInterval = 0.22f;
	constexpr float CrawlerSparkInterval = 0.1f;
	constexpr float CrawlerSteamInterval = 0.18f;

	/** Colors of the ash crust, the ash it digs through, and a part flashing under a hit */
	const FLinearColor CrawlerCrustColor(0.08f, 0.05f, 0.04f);
	const FLinearColor CrawlerAshColor(0.14f, 0.11f, 0.09f);
	const FLinearColor CrawlerFlashColor(1.0f, 0.9f, 0.75f);
}

AVaelEmberCrawler::AVaelEmberCrawler()
{
}

void AVaelEmberCrawler::BeginPlay()
{
	Super::BeginPlay();

	SurfacedBodyScale = GetBody()->GetRelativeScale3D();

	UAbilitySystemComponent* AbilitySystem = GetAbilitySystemComponent();
	AbilitySystem->RegisterGameplayTagEvent(VaelTags::Status_Frozen, EGameplayTagEventType::NewOrRemoved).AddUObject(this, &AVaelEmberCrawler::OnDoused);

	EnterState(EVaelCrawlerState::Burrowed);

	BuildRig();
}

float AVaelEmberCrawler::GetIncomingDamageMultiplier(const FGameplayTagContainer& DamageTags) const
{
	float Multiplier = Super::GetIncomingDamageMultiplier(DamageTags);

	if (State == EVaelCrawlerState::Burrowed)
	{
		const UVaelEmberCrawlerData* Data = GetData<UVaelEmberCrawlerData>();
		Multiplier *= DamageTags.HasTagExact(VaelTags::Element_Earth) ? Data->BurrowedEarthMultiplier : Data->BurrowedDamageMultiplier;
	}

	return Multiplier;
}

TSubclassOf<UVaelCreatureData> AVaelEmberCrawler::GetDefaultDataClass() const
{
	return UVaelEmberCrawlerData::StaticClass();
}

void AVaelEmberCrawler::TickBehavior(float DeltaSeconds)
{
	const UVaelEmberCrawlerData* Data = GetData<UVaelEmberCrawlerData>();

	StateTime += DeltaSeconds;
	ClawCooldown -= DeltaSeconds;

	float TargetDistance = 0.0f;
	AActor* Target = FindTarget(Data->AggroRange, &TargetDistance);

	switch (State)
	{
	case EVaelCrawlerState::Burrowed:
		if (Target == nullptr)
		{
			if (FVector::Dist2D(GetActorLocation(), HomeLocation) > CrawlerHomeTolerance)
			{
				MoveTowards(HomeLocation, Data->MoveSpeed * Data->BurrowReturnSpeedScale);
			}
		}
		else if (TargetDistance < Data->SurfaceDistance)
		{
			EnterState(EVaelCrawlerState::Surfacing);
		}
		else
		{
			MoveTowards(Target->GetActorLocation(), Data->MoveSpeed * Data->BurrowChaseSpeedScale);
		}
		break;

	case EVaelCrawlerState::Surfacing:
		if (StateTime >= Data->SurfaceDuration)
		{
			EnterState(EVaelCrawlerState::Fuse);
		}
		break;

	case EVaelCrawlerState::Fuse:
		// Glows brighter the closer it gets to the explosion
		SetBodyColor(FMath::Lerp(Data->BodyColor, FLinearColor(1.0f, 0.85f, 0.3f), 0.5f + 0.5f * FMath::Sin(StateTime * (8.0f + 16.0f * StateTime / FMath::Max(Data->FuseDuration, 0.01f)))));

		if (Target != nullptr && TargetDistance > GetCapsuleComponent()->GetScaledCapsuleRadius() * 2.0f)
		{
			MoveTowards(Target->GetActorLocation(), Data->MoveSpeed * Data->FuseChaseSpeedScale);
		}

		if (StateTime >= Data->FuseDuration)
		{
			Explode();
		}
		break;

	case EVaelCrawlerState::Doused:
		if (StateTime >= Data->DousedDuration)
		{
			EnterState(EVaelCrawlerState::Crawling);
		}
		break;

	case EVaelCrawlerState::Crawling:
		if (Target == nullptr)
		{
			break;
		}

		if (TargetDistance > Data->ClawRange)
		{
			MoveTowards(Target->GetActorLocation(), Data->MoveSpeed * Data->CrawlSpeedScale);
		}
		else if (ClawCooldown <= 0.0f && !IsBlinded())
		{
			FaceTowards(Target->GetActorLocation());
			HitPlayer(Target, Data->ClawDamage, EVaelElement::Fire);
			ClawCooldown = Data->ClawInterval;
			SinceClawAttack = 0.0f;
		}
		break;
	}
}

void AVaelEmberCrawler::EnterState(EVaelCrawlerState NewState)
{
	const UVaelEmberCrawlerData* Data = GetData<UVaelEmberCrawlerData>();

	State = NewState;
	StateTime = 0.0f;

	const bool bBurrowed = State == EVaelCrawlerState::Burrowed;

	// Burrowed crawlers slip under the feet of the players
	GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Pawn, bBurrowed ? ECR_Ignore : ECR_Block);

	// Placeholder look: a flat mound under the ash, a glowing body above it
	UStaticMeshComponent* BodyMesh = GetBody();
	const float HalfHeight = GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight();
	BodyMesh->SetRelativeScale3D(bBurrowed ? SurfacedBodyScale * FVector(1.0f, 1.0f, BurrowedHeightScale) : SurfacedBodyScale);
	BodyMesh->SetRelativeLocation(FVector(0.0f, 0.0f, bBurrowed ? -HalfHeight * (1.0f - BurrowedHeightScale) : 0.0f));

	switch (State)
	{
	case EVaelCrawlerState::Burrowed:
		SetBodyColor(FLinearColor(0.12f, 0.08f, 0.06f));
		break;
	case EVaelCrawlerState::Surfacing:
	case EVaelCrawlerState::Fuse:
		SetBodyColor(Data->BodyColor);
		break;
	case EVaelCrawlerState::Doused:
		SetBodyColor(FLinearColor(0.35f, 0.35f, 0.35f));
		break;
	case EVaelCrawlerState::Crawling:
		SetBodyColor(Data->BodyColor * 0.45f);
		break;
	}
}

void AVaelEmberCrawler::OnDoused(const FGameplayTag Tag, int32 NewCount)
{
	if (NewCount > 0 && State == EVaelCrawlerState::Fuse && !IsDead())
	{
		UE_LOG(LogVael, Verbose, TEXT("'%s' is doused"), *GetNameSafe(this));
		UVaelCombatTextSubsystem::PostReaction(this, NSLOCTEXT("VaelHUD", "Doused", "Gel\u00F6scht!"));
		EnterState(EVaelCrawlerState::Doused);
	}
}

void AVaelEmberCrawler::OnDamageTaken(float Damage, const FGameplayTagContainer& DamageTags)
{
	Super::OnDamageTaken(Damage, DamageTags);

	// A hit of water puts the fuse out; rain only makes the crawler wet
	if (DamageTags.HasTagExact(VaelTags::Element_Water))
	{
		OnDoused(VaelTags::Element_Water, 1);
	}
}

void AVaelEmberCrawler::Explode()
{
	const UVaelEmberCrawlerData* Data = GetData<UVaelEmberCrawlerData>();
	const FVector Center = GetActorLocation();

	// Players in the blast
	ForEachActivePlayer([this, Data, &Center](AVaelCharacter* Player)
	{
		if (FVector::Dist2D(Player->GetActorLocation(), Center) < Data->ExplosionRadius + Player->GetCapsuleComponent()->GetScaledCapsuleRadius())
		{
			HitPlayer(Player, Data->ExplosionDamage, EVaelElement::Fire);
		}
	});

	// Other creatures get caught too
	for (TActorIterator<AVaelCreature> It(GetWorld()); It; ++It)
	{
		AVaelCreature* Other = *It;
		if (Other != this && !Other->IsDead() && FVector::Dist2D(Other->GetActorLocation(), Center) < Data->ExplosionRadius)
		{
			UVaelCombatStatics::DealDamage(this, Other, Data->ExplosionDamageToCreatures, EVaelElement::Fire);
			Other->ApplyKnockback(Other->GetActorLocation() - Center, Data->ExplosionKnockback);
		}
	}

	// Whoever is still standing close by remembers the blast
	bool bWitnessed = false;
	ForEachActivePlayer([Data, &Center, &bWitnessed](AVaelCharacter* Player)
	{
		bWitnessed |= FVector::Dist2D(Player->GetActorLocation(), Center) < Data->WitnessDistance;
	});

	if (bWitnessed)
	{
		TeachFormula(Data->FormulaOnWitnessedExplosion, LOCTEXT("FormulaFromExplosion", "Die Glut der Explosion hat sich in dein Ged\u00E4chtnis gebrannt."));
	}

	FVector Ground;
	if (FindGround(GetWorld(), Center, Ground))
	{
		AVaelGroundArea::SpawnArea(GetWorld(), Ground + FVector(0.0f, 0.0f, 2.0f), EVaelElement::Fire, Data->FireRadius, Data->FireLifetime, Data->FireDamagePerSecond, this);
	}

#if ENABLE_DRAW_DEBUG
	DrawDebugSphere(GetWorld(), Center, Data->ExplosionRadius, 24, FColor(255, 140, 40), false, 0.3f);
#endif

	UE_LOG(LogVael, Verbose, TEXT("'%s' explodes"), *GetNameSafe(this));
	UVaelHitFeedbackSubsystem::Shake(this, 0.45f);

	Die();

	// The fire it leaves only burns players while its instigator exists, so the crawler stays around unseen
	SetActorHiddenInGame(true);
	SetLifeSpan(Data->FireLifetime + 0.5f);
}

UStaticMeshComponent* AVaelEmberCrawler::AddRigPart(UMaterialInterface* Material, const FLinearColor& Color, float Glow, bool bCone)
{
	UStaticMeshComponent* Part = VaelEffects::AddLookShape(this, Material, Color, Glow);
	Part->AttachToComponent(Rig, FAttachmentTransformRules::KeepRelativeTransform);
	Part->SetCastShadow(true);
	Part->SetVisibility(true);

	if (bCone)
	{
		Part->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cone.Cone")));
	}

	return Part;
}

void AVaelEmberCrawler::PlaceSegment(UStaticMeshComponent* Segment, const FVector& Start, const FVector& End, float Width)
{
	Segment->SetRelativeLocation((Start + End) * 0.5f);
	Segment->SetRelativeRotation((End - Start).Rotation());
	Segment->SetRelativeScale3D(FVector(FVector::Dist(Start, End) * 1.15f, Width, Width) / CrawlerShapeSize);
}

void AVaelEmberCrawler::BuildRig()
{
	UMaterialInterface* CoreMaterial = nullptr;
	UMaterialInterface* GlowMaterial = nullptr;
	VaelEffects::LoadLookMaterials(CoreMaterial, GlowMaterial);

	// The placeholder body gives way to the rig; its colors still arrive through OnBodyColorShown
	GetBody()->SetVisibility(false);

	Rig = NewObject<USceneComponent>(this, TEXT("Rig"));
	Rig->SetupAttachment(GetRootComponent());
	Rig->RegisterComponent();

	// With a mesh of the creature factory only the mound of ash is built; mesh and legs come from the base class
	if (GetFactoryMesh() != nullptr)
	{
		Mound = VaelEffects::AddLookShape(this, CoreMaterial, CrawlerAshColor, 0.0f);
		RefreshBodyColor();
		return;
	}

	// A glowing gland in the rear, half hidden under plates of ash crust; thorax and head in front
	Gland = AddRigPart(CoreMaterial, GlowColor, GlandGlow);
	Gland->SetRelativeLocation(FVector(-28.0f, 0.0f, -24.0f));
	Gland->SetRelativeScale3D(CrawlerGlandSize / CrawlerShapeSize);

	struct FShellPart { FVector Place; FVector Size; FRotator Rotation; };
	const FShellPart ShellLayout[] =
	{
		{ FVector(14.0f, 0.0f, -28.0f), FVector(58.0f, 54.0f, 30.0f), FRotator::ZeroRotator },
		{ FVector(46.0f, 0.0f, -32.0f), FVector(32.0f, 34.0f, 22.0f), FRotator(-8.0f, 0.0f, 0.0f) },
		{ FVector(-14.0f, 0.0f, -6.0f), FVector(34.0f, 56.0f, 12.0f), FRotator(8.0f, 0.0f, 0.0f) },
		{ FVector(-34.0f, 0.0f, -3.0f), FVector(30.0f, 58.0f, 12.0f), FRotator::ZeroRotator },
		{ FVector(-53.0f, 0.0f, -13.0f), FVector(26.0f, 46.0f, 12.0f), FRotator(-30.0f, 0.0f, 0.0f) },
	};

	for (const FShellPart& Part : ShellLayout)
	{
		UStaticMeshComponent* Plate = AddRigPart(CoreMaterial, CrustColor, 0.0f);
		Plate->SetRelativeLocation(Part.Place);
		Plate->SetRelativeRotation(Part.Rotation);
		Plate->SetRelativeScale3D(Part.Size / CrawlerShapeSize);
		ShellParts.Add(Plate);
	}

	for (const float Side : { -1.0f, 1.0f })
	{
		UStaticMeshComponent* Eye = AddRigPart(CoreMaterial, GlowColor, 6.0f);
		Eye->SetRelativeLocation(FVector(60.0f, 9.0f * Side, -28.0f));
		Eye->SetRelativeScale3D(FVector(7.0f / CrawlerShapeSize));
		Eyes.Add(Eye);

		ClawArms.Add(AddRigPart(CoreMaterial, CrustColor, 0.0f));
		ClawTips.Add(AddRigPart(CoreMaterial, CrustColor, 0.0f, true));
	}

	// Six legs, left ones first, front to back
	for (int32 LegIndex = 0; LegIndex < 6; ++LegIndex)
	{
		LegUpper.Add(AddRigPart(CoreMaterial, CrustColor, 0.0f));
		LegLower.Add(AddRigPart(CoreMaterial, CrustColor, 0.0f));
	}

	// The mound of ash that moves over the burrowed crawler
	Mound = VaelEffects::AddLookShape(this, CoreMaterial, CrawlerAshColor, 0.0f);
	Mound->SetVisibility(true);

	RefreshBodyColor();
	AnimateRig(0.0f);
}

void AVaelEmberCrawler::OnBodyColorShown(const FLinearColor& Color, bool bHitFlash)
{
	// The gland and the eyes glow in the color of the state, the crust takes a little of it
	GlowColor = Color;
	CrustColor = FMath::Lerp(CrawlerCrustColor, Color, 0.12f);

	// A factory mesh shows its glow through the master material
	if (Rig == nullptr || Gland == nullptr)
	{
		return;
	}

	const auto Paint = [bHitFlash](UStaticMeshComponent* Part, const FLinearColor& PartColor)
	{
		if (UMaterialInstanceDynamic* Material = Cast<UMaterialInstanceDynamic>(Part->GetMaterial(0)))
		{
			Material->SetVectorParameterValue(VaelEffects::LookColorParameter, bHitFlash ? CrawlerFlashColor : PartColor);
		}
	};

	Paint(Gland, GlowColor);

	for (UStaticMeshComponent* Eye : Eyes)
	{
		Paint(Eye, GlowColor);
	}

	for (const TArray<TObjectPtr<UStaticMeshComponent>>* Parts : { &ShellParts, &LegUpper, &LegLower, &ClawArms, &ClawTips })
	{
		for (UStaticMeshComponent* Part : *Parts)
		{
			Paint(Part, CrustColor);
		}
	}
}

void AVaelEmberCrawler::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (Rig != nullptr && !IsHidden())
	{
		AnimateRig(DeltaSeconds);
	}
}

void AVaelEmberCrawler::AnimateRig(float DeltaSeconds)
{
	const UVaelEmberCrawlerData* Data = GetData<UVaelEmberCrawlerData>();
	const float HalfHeight = GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight();
	const float Time = GetWorld()->GetTimeSeconds();
	const float Speed = GetVelocity().Size2D();
	const bool bDeadNow = IsDead();

	// Breaking out of the ground throws up a burst of ash
	if (State != ShownState)
	{
		if (ShownState == EVaelCrawlerState::Burrowed && !bDeadNow)
		{
			FVaelDebris Burst;
			Burst.Count = 12;
			Burst.Spread = GetCapsuleComponent()->GetUnscaledCapsuleRadius();
			Burst.Speed = 260.0f;
			Burst.Lift = 380.0f;
			Burst.Color = CrawlerAshColor;
			Burst.RockShare = 0.15f;
			AVaelDebrisBurst::Spawn(this, GetActorLocation() - FVector(0.0f, 0.0f, HalfHeight), Burst);
		}

		ShownState = State;
	}

	// Sinks under the ash while burrowed, climbs out when it surfaces
	const bool bBurrowed = State == EVaelCrawlerState::Burrowed;
	const float RiseSpeed = 1.0f / FMath::Max(Data->SurfaceDuration, 0.2f);
	Emergence = bBurrowed ? FMath::Max(0.0f, Emergence - DeltaSeconds * 4.0f) : FMath::Min(1.0f, Emergence + DeltaSeconds * RiseSpeed);

	const float Climb = 1.0f - FMath::Square(1.0f - Emergence);
	FVector RigOffset(0.0f, 0.0f, -(1.0f - Climb) * HalfHeight * 1.6f - (bDeadNow ? 12.0f : 0.0f));

	// The fuse makes it swell and tremble faster and faster
	float GlandSwell = 1.0f;
	float Glow = 1.5f;

	if (SinceClawAttack >= 0.0f)
	{
		SinceClawAttack += DeltaSeconds;
	}

	switch (State)
	{
	case EVaelCrawlerState::Burrowed:
		Glow = 0.5f;
		break;

	case EVaelCrawlerState::Surfacing:
		Glow = 3.0f;
		break;

	case EVaelCrawlerState::Fuse:
	{
		const float Progress = FMath::Clamp(StateTime / FMath::Max(Data->FuseDuration, 0.01f), 0.0f, 1.0f);
		const float Pulse = 0.5f + 0.5f * FMath::Sin(StateTime * (6.0f + 30.0f * Progress));
		GlandSwell = 1.0f + 0.25f * Progress + 0.08f * Pulse;
		Glow = 4.0f + 10.0f * Progress * Pulse;
		RigOffset += FMath::VRand() * (1.0f + 4.0f * Progress);
		break;
	}

	case EVaelCrawlerState::Doused:
		Glow = 0.3f;
		RigOffset += FMath::VRand() * 1.5f;
		break;

	case EVaelCrawlerState::Crawling:
		Glow = 1.5f;
		break;
	}

	// A claw attack snaps the pincers shut and throws the body forward
	const float Snap = SinceClawAttack >= 0.0f && SinceClawAttack < CrawlerSnapTime ? FMath::Sin(UE_PI * SinceClawAttack / CrawlerSnapTime) : 0.0f;
	RigOffset.X += CrawlerLunge * Snap;

	const bool bFactory = GetFactoryMesh() != nullptr;
	if (bFactory)
	{
		// The mesh sinks and rises, swells with the fuse and glows in its cracks, gland and eyes; its legs fold while under the ash
		SetFactoryMeshOffset(RigOffset, 1.0f + (GlandSwell - 1.0f) * 0.5f);
		SetFactoryGlow(bDeadNow ? 0.0f : Glow);
		GetFactoryMesh()->SetVisibility(Emergence > 0.02f || bDeadNow);
		if (!bDeadNow)
		{
			GetFactoryLegs()->SetCurl(1.0f - Climb);
		}
		if (SinceClawAttack >= 0.0f && SinceClawAttack <= DeltaSeconds)
		{
			GetFactoryLegs()->AddBodyJolt(GetActorForwardVector() * 10.0f);
		}
	}
	else
	{
		Rig->SetRelativeLocation(RigOffset);
		Rig->SetRelativeRotation(GetBody()->GetRelativeRotation());

		Gland->SetRelativeScale3D(CrawlerGlandSize * GlandSwell / CrawlerShapeSize);
		VaelEffects::SetLookGlow(Gland, Glow);

		for (UStaticMeshComponent* Eye : Eyes)
		{
			VaelEffects::SetLookGlow(Eye, bDeadNow ? 0.0f : Glow * 1.5f + 1.0f);
		}
	}

	// The mound of ash shows only while the crawler is (partly) under it
	Mound->SetVisibility(Emergence < 0.95f && !bDeadNow);
	Mound->SetRelativeLocation(FVector(0.0f, 0.0f, -HalfHeight));
	Mound->SetRelativeScale3D(FVector(130.0f, 120.0f, 40.0f * (1.0f - Emergence) + 4.0f) * (1.0f + 0.04f * FMath::Sin(Time * 9.0f)) / CrawlerShapeSize);

	// The code rig moves its own legs and claws; a factory mesh walks on the legs of the base class
	if (!bFactory)
	{
		// Legs: a tripod gait whose steps follow the distance walked; three feet on the ground at any time
		GaitPhase += Speed * DeltaSeconds / (CrawlerStride * 2.0f);

		const float Ground = -HalfHeight - RigOffset.Z;
		for (int32 LegIndex = 0; LegIndex < 6; ++LegIndex)
		{
			const int32 Pair = LegIndex % 3;
			const float Side = LegIndex < 3 ? -1.0f : 1.0f;
			const float GroupOffset = (Pair + (Side > 0.0f ? 1 : 0)) % 2 == 0 ? 0.0f : 0.5f;

			const FVector Hip(CrawlerHipX[Pair], CrawlerHipY * Side, -32.0f);
			FVector Foot(CrawlerHipX[Pair] + CrawlerFootSpreadX[Pair], CrawlerFootY * Side, Ground);

			if (bDeadNow)
			{
				// Curled in under the body
				Foot = Hip + FVector(0.0f, 12.0f * Side, -6.0f);
			}
			else if (Speed > 10.0f)
			{
				const float Angle = UE_TWO_PI * (GaitPhase + GroupOffset);
				Foot.X -= CrawlerStride * 0.5f * FMath::Cos(Angle);
				Foot.Z += FMath::Max(0.0f, FMath::Sin(Angle)) * CrawlerStepLift;
			}
			else
			{
				// Standing: the legs twitch now and then
				Foot.Z += FMath::Max(0.0f, FMath::Sin(Time * 3.0f + LegIndex * 1.7f) - 0.9f) * 40.0f;
			}

			const FVector Knee = (Hip + Foot) * 0.5f + FVector(0.0f, 12.0f * Side, bDeadNow ? 6.0f : CrawlerKneeHeight);

			PlaceSegment(LegUpper[LegIndex], Hip, Knee, CrawlerUpperLegWidth);
			PlaceSegment(LegLower[LegIndex], Knee, Foot, CrawlerLowerLegWidth);
		}

		// Claws: an arm forward from the head, a pincer that turns inwards to snap shut
		for (int32 ClawIndex = 0; ClawIndex < 2; ++ClawIndex)
		{
			const float Side = ClawIndex == 0 ? -1.0f : 1.0f;
			const FVector Shoulder(40.0f, 20.0f * Side, -36.0f);
			const FVector Elbow(62.0f, 30.0f * Side, -40.0f);
			const FVector PincerDirection = FVector(1.0f, -Side * (0.1f + 0.55f * (1.0f - Snap)), 0.0f).GetSafeNormal();
			const float PincerLength = 26.0f;

			PlaceSegment(ClawArms[ClawIndex], Shoulder, Elbow, 11.0f);

			UStaticMeshComponent* Tip = ClawTips[ClawIndex];
			Tip->SetRelativeLocation(Elbow + PincerDirection * PincerLength * 0.5f);
			Tip->SetRelativeRotation(FRotationMatrix::MakeFromZ(PincerDirection).Rotator());
			Tip->SetRelativeScale3D(FVector(12.0f, 12.0f, PincerLength) / CrawlerShapeSize);
		}
	}

	// Dust behind the moving mound, sparks from the burning gland, steam from the doused one
	PuffCountdown -= DeltaSeconds;
	if (PuffCountdown > 0.0f || bDeadNow)
	{
		return;
	}

	FVaelDebris Puff;
	Puff.Count = 3;
	Puff.Spread = 20.0f;
	Puff.MinSize = 4.0f;
	Puff.MaxSize = 9.0f;

	if (bBurrowed && Speed > 20.0f)
	{
		PuffCountdown = CrawlerDustInterval;
		Puff.Speed = 90.0f;
		Puff.Lift = 160.0f;
		Puff.Color = CrawlerAshColor;
		AVaelDebrisBurst::Spawn(this, GetActorLocation() - FVector(0.0f, 0.0f, HalfHeight) - GetActorForwardVector() * 40.0f, Puff);
	}
	else if (State == EVaelCrawlerState::Fuse)
	{
		PuffCountdown = CrawlerSparkInterval;
		Puff.Count = 2;
		Puff.Speed = 120.0f;
		Puff.Lift = 260.0f;
		Puff.Gravity = 300.0f;
		Puff.Lifetime = 0.5f;
		Puff.MinSize = 3.0f;
		Puff.MaxSize = 6.0f;
		Puff.Color = FMath::Lerp(GlowColor, FLinearColor(1.0f, 0.85f, 0.4f), 0.5f);
		Puff.Glow = 6.0f;
		Puff.bLandOnGround = false;
		AVaelDebrisBurst::Spawn(this, GetGlandLocation(), Puff);
	}
	else if (State == EVaelCrawlerState::Doused)
	{
		PuffCountdown = CrawlerSteamInterval;
		Puff.Count = 2;
		Puff.Speed = 40.0f;
		Puff.Lift = 90.0f;
		Puff.Gravity = -150.0f;
		Puff.Lifetime = 0.8f;
		Puff.MinSize = 18.0f;
		Puff.MaxSize = 30.0f;
		Puff.Color = FLinearColor(0.7f, 0.7f, 0.72f);
		Puff.Glow = 0.4f;
		Puff.bSoft = true;
		Puff.bLandOnGround = false;
		AVaelDebrisBurst::Spawn(this, GetGlandLocation(), Puff);
	}
}

#undef LOCTEXT_NAMESPACE

FVector AVaelEmberCrawler::GetGlandLocation() const
{
	// The gland of a factory mesh sits at its rear, a little above the ground
	return Gland != nullptr ? Gland->GetComponentLocation()
		: GetActorLocation() - GetActorForwardVector() * GetCapsuleComponent()->GetScaledCapsuleRadius() * 0.7f;
}
