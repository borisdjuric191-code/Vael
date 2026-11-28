// Copyright Epic Games, Inc. All Rights Reserved.

#include "Creatures/VaelDeathMotion.h"

#include "Combat/VaelHitFeedbackSubsystem.h"
#include "Components/CapsuleComponent.h"
#include "Components/MeshComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Creatures/VaelCreature.h"
#include "Engine/World.h"
#include "Magic/VaelDebrisBurst.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "VaelAssets.h"
#include "World/VaelGround.h"

namespace
{
	// The overlay looks of a death (Scripts/CreateDeathMaterials.py); params "Glut" (embers, 0..1) and "Deckung" (cover, 0..1)
	const FSoftObjectPath CharMaterialPath(TEXT("/Game/Vael/VFX/Tod/M_Tod_Verkohlt.M_Tod_Verkohlt"));
	const FSoftObjectPath IceMaterialPath(TEXT("/Game/Vael/VFX/Tod/M_Tod_Eis.M_Tod_Eis"));
	const FSoftObjectPath NidMaterialPath(TEXT("/Game/Vael/VFX/Tod/M_Tod_Nid.M_Tod_Nid"));

	const FName GlowParameter(TEXT("Glut"));
	const FName CoverParameter(TEXT("Deckung"));

	/** Gravity of a flung corpse, a little stronger than the world's: it reads heavier from the high camera */
	constexpr float CorpseGravity = 1500.0f;

	/** Seconds the corpse sinks into the ground at the end */
	constexpr float SinkTime = 1.3f;

	FQuat RotationBetween(const FVector& From, const FVector& To)
	{
		return FQuat::FindBetweenNormals(From.GetSafeNormal(), To.GetSafeNormal());
	}
}

UVaelDeathMotion::UVaelDeathMotion()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickGroup = TG_PostPhysics;
}

float UVaelDeathMotion::Start(AVaelCreature* Creature, const FVaelKillingBlow& Blow, bool bOwnClip)
{
	if (Creature == nullptr || Creature->GetWorld() == nullptr)
	{
		return 0.0f;
	}
	UVaelDeathMotion* Motion = NewObject<UVaelDeathMotion>(Creature, TEXT("DeathMotion"));
	Motion->Blow = Blow;
	Motion->bOwnClip = bOwnClip;
	Motion->RegisterComponent();

	const UCapsuleComponent* Capsule = Creature->GetCapsuleComponent();
	Motion->HalfHeight = Capsule != nullptr ? Capsule->GetScaledCapsuleHalfHeight() : 60.0f;
	Motion->Radius = Capsule != nullptr ? Capsule->GetScaledCapsuleRadius() : 40.0f;
	Motion->StartLocation = Creature->GetActorLocation();
	Motion->StartRotation = Creature->GetActorQuat();
	Motion->StartScale = Creature->GetActorScale3D();
	Motion->GroundZ = Motion->TraceGround(Motion->StartLocation);

	// What is seen of it: every mesh (static, skeletal, procedural) gets the overlay of its death
	TInlineComponentArray<UMeshComponent*> Found(Creature);
	for (UMeshComponent* Each : Found)
	{
		if (Each->IsVisible() && !Each->bHiddenInGame)
		{
			Motion->Meshes.Add(Each);
		}
	}

	// The style is chosen in the first tick: the knockback of the killing hit arrives after its damage
	Motion->Duration = 4.6f;
	return Motion->Duration;
}

void UVaelDeathMotion::AddBlow(const FVector& Direction, float Speed)
{
	if (!bChosen && Speed > Blow.Knockback)
	{
		Blow.Knockback = Speed;
		Blow.KnockbackDirection = Direction.GetSafeNormal2D();
	}
}

float UVaelDeathMotion::TraceGround(const FVector& At) const
{
	FHitResult Hit;
	if (VaelGround::TraceGround(GetWorld(), At + FVector(0.0f, 0.0f, 60.0f), At - FVector(0.0f, 0.0f, 3000.0f), Hit, GetOwner()))
	{
		return Hit.ImpactPoint.Z;
	}
	return At.Z - HalfHeight;
}

void UVaelDeathMotion::Choose()
{
	bChosen = true;
	AActor* Owner = GetOwner();

	// Away from the one who struck
	FVector Away = Blow.KnockbackDirection;
	if (Away.IsNearlyZero() && !Blow.From.IsNearlyZero())
	{
		Away = (StartLocation - Blow.From).GetSafeNormal2D();
	}
	if (Away.IsNearlyZero())
	{
		Away = -Owner->GetActorForwardVector().GetSafeNormal2D();
	}
	FallDirection = Away.IsNearlyZero() ? FVector::ForwardVector : Away;

	// How hard the blow was: knockback, or damage far beyond what was left of its life
	const float Overkill = Blow.HealthBefore > 0.0f ? Blow.Damage / Blow.HealthBefore : 1.0f;
	const float Share = Blow.HealthBefore > 0.0f && Blow.Damage > 0.0f ? Blow.Damage / FMath::Max(Blow.HealthBefore, 1.0f) : 0.0f;
	const bool bBig = Radius > 110.0f || HalfHeight > 220.0f;
	const bool bHeavy = !bBig && (Blow.Knockback >= 450.0f || Overkill >= 2.5f || (Share >= 1.0f && Blow.Damage >= 45.0f));
	const bool bInAir = StartLocation.Z - HalfHeight > GroundZ + 90.0f;
	bFlat = Radius > HalfHeight * 0.95f;

	if (Blow.bFrozen)
	{
		Style = EVaelDeathStyle::Shatter;
	}
	else if (Blow.bHasElement && Blow.Element == EVaelElement::Mark)
	{
		Style = EVaelDeathStyle::Dissolve;
	}
	else if (bInAir)
	{
		Style = EVaelDeathStyle::Fall;
	}
	else if (bHeavy && (!Blow.bHasElement || Blow.Element != EVaelElement::Fire))
	{
		Style = Blow.bHasElement && Blow.Element == EVaelElement::Earth && Blow.Knockback < 450.0f && !bOwnClip ? EVaelDeathStyle::Crush : EVaelDeathStyle::Fling;
	}
	else if (Blow.bBurning || (Blow.bHasElement && Blow.Element == EVaelElement::Fire))
	{
		Style = EVaelDeathStyle::Burn;
	}
	else if (Blow.bHasElement && Blow.Element == EVaelElement::Air)
	{
		Style = EVaelDeathStyle::Shock;
	}
	else
	{
		Style = EVaelDeathStyle::Collapse;
	}
	UE_LOG(LogTemp, Log, TEXT("DeathMotion: '%s' dies by %s (damage %.0f of %.0f, knockback %.0f, element %d)"), *GetNameSafe(Owner),
		*UEnum::GetValueAsString(Style), Blow.Damage, Blow.HealthBefore, Blow.Knockback, Blow.bHasElement ? static_cast<int32>(Blow.Element) : -1);
	BeginStyle();
}

void UVaelDeathMotion::BeginStyle()
{
	const FVector Centre = GetOwner()->GetActorLocation();
	const FVector Up = FVector::UpVector;
	SpinAxis = FVector::CrossProduct(Up, FallDirection).GetSafeNormal();
	Pivot = FVector(Centre.X, Centre.Y, GroundZ) + FallDirection * Radius * (bFlat ? 0.0f : 0.6f);
	ToppleAngle = bFlat ? 180.0f : FMath::FRandRange(80.0f, 92.0f);
	const float Size = FMath::Max(Radius, HalfHeight);

	switch (Style)
	{
	case EVaelDeathStyle::Fling:
	{
		// Small bodies fly farther; the camera shakes a little for the weight of it
		const float Lightness = FMath::Clamp(70.0f / FMath::Max(Size, 20.0f), 0.45f, 1.6f);
		const float Push = FMath::Clamp(FMath::Max(Blow.Knockback, 700.0f) * 1.25f, 700.0f, 1900.0f) * Lightness;
		Velocity = FallDirection * Push + FVector(0.0f, 0.0f, FMath::FRandRange(520.0f, 760.0f) * FMath::Sqrt(Lightness));
		SpinSpeed = FMath::FRandRange(520.0f, 820.0f) * Lightness;
		UVaelHitFeedbackSubsystem::Shake(GetOwner(), 0.18f);
		Debris(Centre, FLinearColor(0.35f, 0.3f, 0.25f), 0.0f, 6, 260.0f, 180.0f, 980.0f, 6.0f, 12.0f, 0.5f, true, false);
		break;
	}
	case EVaelDeathStyle::Fall:
		Velocity = FallDirection * 260.0f + FVector(0.0f, 0.0f, 120.0f);
		SpinSpeed = FMath::FRandRange(260.0f, 420.0f);
		break;
	case EVaelDeathStyle::Burn:
		SetOverlay(VaelAssets::LoadOptional<UMaterialInterface>(TSoftObjectPtr<UMaterialInterface>(CharMaterialPath)));
		SetOverlayParameter(GlowParameter, 1.0f);
		SetOverlayParameter(CoverParameter, 0.0f);
		Debris(Centre, FLinearColor(1.0f, 0.42f, 0.08f), 6.0f, 10, 160.0f, 260.0f, -120.0f, 3.0f, 6.0f, 1.1f, false, true);
		break;
	case EVaelDeathStyle::Shatter:
		SetOverlay(VaelAssets::LoadOptional<UMaterialInterface>(TSoftObjectPtr<UMaterialInterface>(IceMaterialPath)));
		SetOverlayParameter(CoverParameter, 1.0f);
		break;
	case EVaelDeathStyle::Shock:
		SetOverlay(VaelAssets::LoadOptional<UMaterialInterface>(TSoftObjectPtr<UMaterialInterface>(CharMaterialPath)));
		SetOverlayParameter(GlowParameter, 0.0f);
		SetOverlayParameter(CoverParameter, 0.0f);
		break;
	case EVaelDeathStyle::Crush:
		UVaelHitFeedbackSubsystem::Shake(GetOwner(), 0.25f);
		Debris(FVector(Centre.X, Centre.Y, GroundZ + 20.0f), FLinearColor(0.32f, 0.27f, 0.22f), 0.0f, 14, 380.0f, 260.0f, 980.0f, 6.0f, 14.0f, 0.8f, true, false, 0.35f);
		break;
	case EVaelDeathStyle::Dissolve:
		SetOverlay(VaelAssets::LoadOptional<UMaterialInterface>(TSoftObjectPtr<UMaterialInterface>(NidMaterialPath)));
		SetOverlayParameter(CoverParameter, 0.0f);
		SetOverlayParameter(GlowParameter, 1.0f);
		break;
	default:
		break;
	}
}

void UVaelDeathMotion::SetOverlay(UMaterialInterface* Material)
{
	if (Material == nullptr)
	{
		return;
	}
	Overlay = UMaterialInstanceDynamic::Create(Material, this);
	for (UMeshComponent* Mesh : Meshes)
	{
		if (Mesh != nullptr)
		{
			Mesh->SetOverlayMaterial(Overlay);
		}
	}
}

void UVaelDeathMotion::SetOverlayParameter(FName Name, float Value)
{
	if (Overlay != nullptr)
	{
		Overlay->SetScalarParameterValue(Name, Value);
	}
}

void UVaelDeathMotion::Debris(const FVector& Where, const FLinearColor& Color, float Glow, int32 Count, float Speed, float Lift, float Gravity,
	float MinSize, float MaxSize, float Lifetime, bool bDust, bool bSoft, float RockShare) const
{
	FVaelDebris Burst;
	Burst.Count = Count;
	Burst.Spread = FMath::Max(Radius * 0.6f, 15.0f);
	Burst.Speed = Speed;
	Burst.Lift = Lift;
	Burst.Gravity = Gravity;
	Burst.Lifetime = Lifetime;
	Burst.MinSize = MinSize;
	Burst.MaxSize = MaxSize;
	Burst.Color = Color;
	Burst.Glow = Glow;
	Burst.bDust = bDust;
	Burst.bSoft = bSoft;
	Burst.RockShare = RockShare;
	AVaelDebrisBurst::Spawn(GetOwner(), Where, Burst);
}

void UVaelDeathMotion::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	AActor* Owner = GetOwner();
	if (Owner == nullptr)
	{
		return;
	}
	if (!bChosen)
	{
		Choose();
	}
	Age += DeltaTime;
	const FVector Centre = Owner->GetActorLocation();

	switch (Style)
	{
	case EVaelDeathStyle::Fling:
	case EVaelDeathStyle::Fall:
		TickFlight(DeltaTime);
		break;
	case EVaelDeathStyle::Shatter:
		// frozen still for a breath, a shiver, then it bursts into shards
		if (!bShattered)
		{
			const float Shiver = Age > 0.18f ? 1.5f : 0.0f;
			Owner->SetActorRotation(StartRotation * FQuat(FVector::UpVector, FMath::DegreesToRadians(FMath::Sin(Age * 90.0f) * Shiver)));
			if (Age >= 0.32f)
			{
				bShattered = true;
				Debris(Centre, FLinearColor(0.62f, 0.82f, 0.95f), 0.6f, 18, 460.0f, 360.0f, 1200.0f, 7.0f, FMath::Clamp(Radius * 0.45f, 12.0f, 30.0f), 1.6f, false, false);
				Debris(Centre, FLinearColor(0.85f, 0.93f, 1.0f), 0.0f, 6, 120.0f, 80.0f, 0.0f, 20.0f, 40.0f, 0.9f, true, true);
				UVaelHitFeedbackSubsystem::Shake(Owner, 0.2f);
				Owner->SetActorHiddenInGame(true);
				Owner->SetLifeSpan(1.0f);
			}
		}
		break;
	case EVaelDeathStyle::Shock:
		if (Age < 0.6f)
		{
			// twitching with sparks, blackening
			const FRotator Twitch(FMath::FRandRange(-9.0f, 9.0f), FMath::FRandRange(-12.0f, 12.0f), FMath::FRandRange(-9.0f, 9.0f));
			Owner->SetActorRotation(StartRotation * Twitch.Quaternion());
			SetOverlayParameter(CoverParameter, FMath::Clamp(Age / 0.6f, 0.0f, 0.75f));
			if (Age >= NextEmber)
			{
				NextEmber = Age + 0.12f;
				Debris(Centre + FMath::VRand() * Radius * 0.5f, FLinearColor(0.55f, 0.75f, 1.0f), 9.0f, 4, 420.0f, 200.0f, 600.0f, 2.0f, 4.0f, 0.25f, false, true);
			}
		}
		else
		{
			if (!bSettled && Age - DeltaTime < 0.6f)
			{
				Owner->SetActorRotation(StartRotation);
			}
			TickTopple(DeltaTime);
		}
		break;
	case EVaelDeathStyle::Crush:
	{
		const float T = FMath::Clamp(Age / 0.14f, 0.0f, 1.0f);
		const FVector Squash = FMath::Lerp(FVector::OneVector, FVector(1.35f, 1.35f, 0.3f), FMath::InterpEaseOut(0.0f, 1.0f, T, 2.0f));
		Owner->SetActorScale3D(StartScale * Squash);
		// keep the feet on the ground while the body flattens
		const float Bottom = StartLocation.Z - HalfHeight;
		Owner->SetActorLocation(FVector(StartLocation.X, StartLocation.Y, FMath::Max(Bottom, GroundZ) + HalfHeight * Squash.Z));
		break;
	}
	case EVaelDeathStyle::Dissolve:
	{
		const float T = FMath::Clamp(Age / 1.6f, 0.0f, 1.0f);
		SetOverlayParameter(CoverParameter, FMath::Min(T * 2.0f, 1.0f));
		Owner->SetActorScale3D(StartScale * FMath::Lerp(1.0f, 0.05f, T * T));
		Owner->SetActorLocation(StartLocation + FVector(0.0f, 0.0f, 40.0f * T));
		if (Age >= NextEmber && T < 1.0f)
		{
			NextEmber = Age + 0.15f;
			Debris(Centre, FLinearColor(0.55f, 0.2f, 0.85f), 4.0f, 3, 60.0f, 160.0f, -200.0f, 10.0f, 22.0f, 0.9f, false, true);
		}
		if (T >= 1.0f && !Owner->IsHidden())
		{
			Owner->SetActorHiddenInGame(true);
			Owner->SetLifeSpan(0.5f);
		}
		break;
	}
	case EVaelDeathStyle::Burn:
	{
		TickTopple(DeltaTime);
		// blackens in a moment, the embers glow out, then it falls apart into ash
		SetOverlayParameter(CoverParameter, FMath::Clamp(Age / 0.5f, 0.0f, 1.0f));
		SetOverlayParameter(GlowParameter, FMath::Clamp(1.0f - (Age - 0.4f) / 2.4f, 0.0f, 1.0f));
		if (Age >= NextEmber && Age < 2.2f)
		{
			NextEmber = Age + 0.22f;
			Debris(Centre + FMath::VRand() * Radius * 0.4f, FLinearColor(1.0f, 0.38f, 0.06f), 7.0f, 3, 60.0f, 220.0f, -150.0f, 2.0f, 4.5f, 0.9f, false, true);
			Debris(Centre, FLinearColor(0.12f, 0.11f, 0.1f), 0.0f, 2, 40.0f, 120.0f, -80.0f, 18.0f, 30.0f, 1.2f, true, true);
		}
		if (Age >= 2.9f && !bCrumbled)
		{
			bCrumbled = true;
			Debris(FVector(Centre.X, Centre.Y, GroundZ + 25.0f), FLinearColor(0.16f, 0.15f, 0.14f), 0.0f, 16, 220.0f, 120.0f, 980.0f, 5.0f, 11.0f, 1.3f, true, false);
		}
		if (bCrumbled)
		{
			const float T = FMath::Clamp((Age - 2.9f) / 0.7f, 0.0f, 1.0f);
			Owner->SetActorScale3D(StartScale * FVector(1.0f + 0.25f * T, 1.0f + 0.25f * T, FMath::Lerp(1.0f, 0.08f, T)));
			if (T >= 1.0f && !Owner->IsHidden())
			{
				Owner->SetActorHiddenInGame(true);
				Owner->SetLifeSpan(0.4f);
			}
		}
		break;
	}
	case EVaelDeathStyle::Collapse:
	default:
		TickTopple(DeltaTime);
		break;
	}

	// The end: down into the ground and gone
	if (!bSinking && Age >= Duration - SinkTime && !Owner->IsHidden())
	{
		bSinking = true;
		SinkFrom = Owner->GetActorLocation().Z;
	}
	if (bSinking)
	{
		const float T = FMath::Clamp((Age - (Duration - SinkTime)) / SinkTime, 0.0f, 1.0f);
		FVector Here = Owner->GetActorLocation();
		Here.Z = SinkFrom - T * T * (HalfHeight * 1.4f + 30.0f);
		Owner->SetActorLocation(Here);
	}
}

void UVaelDeathMotion::TickTopple(float DeltaTime)
{
	AActor* Owner = GetOwner();
	if (bOwnClip)
	{
		// its own clip lays it down; only bring it to the ground if it died in a jump
		const FVector Here = Owner->GetActorLocation();
		const float Bottom = Here.Z - HalfHeight;
		if (Bottom > GroundZ + 2.0f)
		{
			Owner->SetActorLocation(FVector(Here.X, Here.Y, FMath::Max(GroundZ, Bottom - CorpseGravity * 0.5f * DeltaTime) + HalfHeight));
		}
		return;
	}
	const float FallTime = bFlat ? 0.42f : 0.5f;
	const float LocalAge = Style == EVaelDeathStyle::Shock ? Age - 0.6f : Age;
	float Angle = 0.0f;
	float Lift = 0.0f;
	if (LocalAge < FallTime)
	{
		const float T = LocalAge / FallTime;
		Angle = ToppleAngle * T * T;
		Lift = bFlat ? FMath::Sin(T * PI) * FMath::Max(HalfHeight * 0.8f, 30.0f) : 0.0f;
	}
	else
	{
		// a small bounce when it hits the ground
		const float After = LocalAge - FallTime;
		Angle = ToppleAngle - (bFlat ? 0.0f : 7.0f * FMath::Sin(FMath::Min(After / 0.2f, 1.0f) * PI) * FMath::Exp(-After * 4.0f));
		if (!bSettled)
		{
			bSettled = true;
			Debris(FVector(Pivot.X, Pivot.Y, GroundZ + 10.0f) + FallDirection * HalfHeight, FLinearColor(0.36f, 0.32f, 0.27f), 0.0f, 6, 200.0f, 90.0f, 980.0f,
				5.0f, 10.0f, 0.55f, true, false);
		}
	}
	const FQuat Turn(SpinAxis, FMath::DegreesToRadians(Angle));
	const FVector Base = bFlat ? FVector(StartLocation.X, StartLocation.Y, StartLocation.Z) : Pivot;
	Owner->SetActorLocationAndRotation(Base + Turn.RotateVector(StartLocation - Base) + FVector(0.0f, 0.0f, Lift), Turn * StartRotation);
}

void UVaelDeathMotion::TickFlight(float DeltaTime)
{
	AActor* Owner = GetOwner();
	if (bSettled)
	{
		TickLying(DeltaTime);
		return;
	}
	const FVector Here = Owner->GetActorLocation();
	Velocity.Z -= CorpseGravity * DeltaTime;
	const FVector Next = Here + Velocity * DeltaTime;
	// the lowest point of the tumbling body stays above the ground
	const float Clearance = FMath::Min(Radius, HalfHeight) * 0.8f;

	FHitResult Hit;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(VaelDeathFlight), false, Owner);
	const bool bHit = GetWorld()->LineTraceSingleByChannel(Hit, Here, Next - FVector(0.0f, 0.0f, Clearance), ECC_WorldStatic, Params);
	FVector Placed = Next;
	if (bHit)
	{
		const bool bFloor = Hit.ImpactNormal.Z > 0.6f;
		Placed = Hit.ImpactPoint + Hit.ImpactNormal * Clearance;
		const float Into = FVector::DotProduct(Velocity, Hit.ImpactNormal);
		const float ImpactSpeed = FMath::Abs(Into);
		Velocity = Velocity - (1.0f + 0.3f) * Into * Hit.ImpactNormal;
		Velocity.X *= bFloor ? 0.55f : 0.35f;
		Velocity.Y *= bFloor ? 0.55f : 0.35f;
		SpinSpeed *= 0.55f;
		++Bounces;
		if (ImpactSpeed > 250.0f)
		{
			Debris(Hit.ImpactPoint, FLinearColor(0.36f, 0.32f, 0.27f), 0.0f, Bounces == 1 ? 9 : 5, 260.0f, 140.0f, 980.0f, 5.0f, 12.0f, 0.6f, true, false);
			if (Bounces == 1 && Style == EVaelDeathStyle::Fall)
			{
				UVaelHitFeedbackSubsystem::Shake(Owner, 0.12f);
			}
		}
		if (bFloor && (Velocity.Size() < 220.0f || Bounces >= 4))
		{
			// comes to rest: lies down on its side the way it landed
			bSettled = true;
			GroundZ = Hit.ImpactPoint.Z;
			LieFrom = Owner->GetActorQuat();
			const FVector BodyUp = LieFrom.GetUpVector();
			FVector Flat = FVector(BodyUp.X, BodyUp.Y, 0.0f);
			Flat = Flat.IsNearlyZero() ? FallDirection : Flat.GetSafeNormal();
			LieTo = bOwnClip ? FQuat(FVector::UpVector, LieFrom.Rotator().Yaw * PI / 180.0f) : RotationBetween(BodyUp, bFlat ? -FVector::UpVector : Flat) * LieFrom;
			LieZFrom = Placed.Z;
			LieBlend = 0.0f;
			Velocity = FVector(Velocity.X, Velocity.Y, 0.0f);
		}
	}
	const FQuat Spin(SpinAxis, FMath::DegreesToRadians(SpinSpeed * DeltaTime));
	Owner->SetActorLocationAndRotation(Placed, bOwnClip ? Owner->GetActorQuat() : Spin * Owner->GetActorQuat());
}

void UVaelDeathMotion::TickLying(float DeltaTime)
{
	AActor* Owner = GetOwner();
	LieBlend = FMath::Min(LieBlend + DeltaTime / 0.25f, 1.0f);
	// slides out the last of its speed
	Velocity *= FMath::Exp(-6.0f * DeltaTime);
	FVector Here = Owner->GetActorLocation() + Velocity * DeltaTime;
	const float Rest = GroundZ + (bFlat ? HalfHeight : Radius) * 0.85f;
	Here.Z = FMath::Lerp(LieZFrom, Rest, LieBlend);
	if (bSinking)
	{
		Here.Z = Owner->GetActorLocation().Z;
	}
	Owner->SetActorLocationAndRotation(Here, FQuat::Slerp(LieFrom, LieTo, FMath::InterpEaseOut(0.0f, 1.0f, LieBlend, 2.0f)));
}
