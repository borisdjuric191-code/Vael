// Copyright Epic Games, Inc. All Rights Reserved.

#include "Magic/VaelMarkCharge.h"
#include "Combat/VaelCharacterBase.h"
#include "Combat/VaelHitFeedbackSubsystem.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Magic/VaelDebrisBurst.h"
#include "Magic/VaelMagicSettings.h"
#include "Magic/VaelSpellEffects.h"

namespace
{
	/** Diameter of the engine sphere in cm, and of the glowing point */
	constexpr float ChargeSphereSize = 100.0f;
	constexpr float ChargeGlowSize = 28.0f;

	/** Camera shake of a burst */
	constexpr float ChargeShake = 0.25f;
}

AVaelMarkCharge::AVaelMarkCharge()
{
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	PrimaryActorTick.bCanEverTick = true;
}

AVaelMarkCharge* AVaelMarkCharge::Attach(AActor* InAttacker, AActor* InTarget, const FVaelSpellHit& InHit)
{
	UWorld* World = InTarget != nullptr ? InTarget->GetWorld() : nullptr;
	if (World == nullptr || InHit.Charge == EVaelChargeMode::None)
	{
		return nullptr;
	}

	// A target keeps one charge of each kind, a new one starts it again
	for (TActorIterator<AVaelMarkCharge> It(World); It; ++It)
	{
		if (It->Target == InTarget && It->Hit.Charge == InHit.Charge)
		{
			It->Hit = InHit;
			It->Attacker = InAttacker;
			It->Age = 0.0f;
			return *It;
		}
	}

	FActorSpawnParameters Parameters;
	Parameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	Parameters.Instigator = Cast<APawn>(InAttacker);

	AVaelMarkCharge* Charge = World->SpawnActor<AVaelMarkCharge>(AVaelMarkCharge::StaticClass(), FTransform(InTarget->GetActorLocation()), Parameters);
	if (Charge != nullptr)
	{
		Charge->Hit = InHit;
		Charge->Attacker = InAttacker;
		Charge->Target = InTarget;
		Charge->LastLocation = InTarget->GetActorLocation();
		Charge->AttachToActor(InTarget, FAttachmentTransformRules::KeepWorldTransform);
	}

	return Charge;
}

void AVaelMarkCharge::BeginPlay()
{
	Super::BeginPlay();

	UMaterialInterface* CoreMaterial = nullptr;
	UMaterialInterface* GlowMaterial = nullptr;
	VaelEffects::LoadLookMaterials(CoreMaterial, GlowMaterial);

	// A splinter glows hot, boiling blood glows in the color of the Mark
	const UVaelMagicSettings* MagicSettings = UVaelMagicSettings::Get();
	const FLinearColor Color = Hit.Charge == EVaelChargeMode::Delayed
		? FMath::Lerp(MagicSettings->GetElementColor(EVaelElement::Mark), MagicSettings->GetElementColor(EVaelElement::Fire), 0.5f)
		: MagicSettings->GetElementColor(EVaelElement::Mark);

	Glow = VaelEffects::AddLookShape(this, GlowMaterial, Color, 4.0f);
	Glow->SetRelativeLocation(FVector(0.0f, 0.0f, 30.0f));
	Glow->SetVisibility(true);
}

void AVaelMarkCharge::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	Age += DeltaSeconds;

	const AVaelCharacterBase* TargetCharacter = Cast<AVaelCharacterBase>(Target.Get());
	const bool bTargetGone = !Target.IsValid() || (TargetCharacter != nullptr && TargetCharacter->IsDefeated());

	if (!bTargetGone)
	{
		LastLocation = Target->GetActorLocation();
	}

	if (Hit.Charge == EVaelChargeMode::Delayed)
	{
		// Bursts after its time, wherever its target is by then
		if (Age >= Hit.ChargeTime || bTargetGone)
		{
			Burst(LastLocation);
			return;
		}
	}
	else
	{
		// Bursts only with the death of its target
		if (bTargetGone)
		{
			Burst(LastLocation);
			return;
		}

		if (Age >= Hit.ChargeTime)
		{
			Destroy();
			return;
		}
	}

	// Pulses faster the closer the burst comes
	const float Progress = FMath::Clamp(Age / FMath::Max(Hit.ChargeTime, 0.01f), 0.0f, 1.0f);
	const float Pulse = 0.5f + 0.5f * FMath::Sin(Age * (6.0f + 30.0f * Progress));

	if (Glow != nullptr)
	{
		Glow->SetRelativeScale3D(FVector(ChargeGlowSize * (0.7f + 0.5f * Pulse) / ChargeSphereSize));
		VaelEffects::SetLookGlow(Glow, 2.0f + 4.0f * Pulse);
	}
}

void AVaelMarkCharge::Burst(const FVector& Location)
{
	// The burst counts as the attacker's; a death charge passes itself on to those it hits
	FVaelSpellHit BurstHit;
	BurstHit.Damage = Hit.ChargeDamage;
	BurstHit.Element = EVaelElement::Mark;
	BurstHit.Knockback = Hit.ChargeRadius;

	if (Hit.Charge == EVaelChargeMode::OnDeath)
	{
		BurstHit.Charge = Hit.Charge;
		BurstHit.ChargeTime = Hit.ChargeTime;
		BurstHit.ChargeDamage = Hit.ChargeDamage;
		BurstHit.ChargeRadius = Hit.ChargeRadius;
	}

	AActor* BurstAttacker = Attacker.Get();
	if (BurstAttacker != nullptr)
	{
		UVaelCombatStatics::ApplySpellHitInRadius(BurstAttacker, Location, Hit.ChargeRadius, BurstHit);
	}

	UVaelHitFeedbackSubsystem::Shake(this, ChargeShake);

	// Placeholder look: glowing pieces fly from the burst
	FVaelDebris Pieces;
	Pieces.Count = 14;
	Pieces.Spread = 20.0f;
	Pieces.Speed = Hit.ChargeRadius * 2.0f;
	Pieces.Lift = 250.0f;
	Pieces.Lifetime = 0.6f;
	Pieces.MinSize = 6.0f;
	Pieces.MaxSize = 12.0f;
	Pieces.Color = UVaelMagicSettings::Get()->GetElementColor(EVaelElement::Mark);
	Pieces.Glow = 5.0f;
	Pieces.bLandOnGround = false;
	AVaelDebrisBurst::Spawn(this, Location, Pieces);

	Destroy();
}
