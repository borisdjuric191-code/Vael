// Copyright Epic Games, Inc. All Rights Reserved.

#include "Floromant/VaelGlimmDome.h"
#include "AbilitySystemComponent.h"
#include "Combat/VaelAttributeSet.h"
#include "Combat/VaelCharacterBase.h"
#include "Combat/VaelCombatStatics.h"
#include "Components/PointLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Floromant/VaelFloromantSettings.h"
#include "Floromant/VaelPlantArmData.h"
#include "Magic/VaelGameplayTags.h"
#include "Magic/VaelSpellProjectile.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Nature/VaelWaterable.h"
#include "UObject/ConstructorHelpers.h"
#include "VaelAssets.h"

namespace
{
	/** Radius of the engine sphere in cm */
	constexpr float DomeSphereRadius = 50.0f;

	/** Brightness of the dome's light in candelas, at a radius of 2.5 m */
	constexpr float DomeLightIntensity = 22.0f;

	/** True for the players and what fights for them */
	bool DomeOnPlayerSide(const AActor* Actor)
	{
		if (const AVaelCharacterBase* Character = Cast<AVaelCharacterBase>(Actor))
		{
			return Character->IsOnPlayerSide();
		}
		const APawn* Pawn = Cast<APawn>(Actor);
		return Pawn != nullptr && Pawn->IsPlayerControlled();
	}
}

AVaelGlimmDome::AVaelGlimmDome()
{
	PrimaryActorTick.bCanEverTick = true;

	Glass = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Glass"));
	RootComponent = Glass;
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Sphere(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	if (Sphere.Succeeded())
	{
		Glass->SetStaticMesh(Sphere.Object);
	}
	Glass->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Glass->SetCastShadow(false);
	Glass->bReceivesDecals = false;
	Glass->SetCanEverAffectNavigation(false);

	Light = CreateDefaultSubobject<UPointLightComponent>(TEXT("Light"));
	Light->SetupAttachment(Glass);
	Light->SetUsingAbsoluteScale(true);
	Light->SetMobility(EComponentMobility::Movable);
	Light->SetIntensityUnits(ELightUnits::Candelas);
	Light->SetIntensity(0.0f);
	Light->SetCastShadows(false);
}

AVaelGlimmDome* AVaelGlimmDome::Raise(APawn* InOwner, AActor* InCenter, const UVaelPlantArmData* InShield, EVaelSymbiosis InPair, float InRadius, bool bInShadow)
{
	UWorld* World = InOwner != nullptr ? InOwner->GetWorld() : nullptr;
	if (World == nullptr || InCenter == nullptr || InShield == nullptr)
	{
		return nullptr;
	}
	FActorSpawnParameters Parameters;
	Parameters.Owner = InOwner;
	Parameters.Instigator = InOwner;
	AVaelGlimmDome* Dome = World->SpawnActor<AVaelGlimmDome>(AVaelGlimmDome::StaticClass(), FTransform(InCenter->GetActorLocation()), Parameters);
	if (Dome == nullptr)
	{
		return nullptr;
	}
	Dome->Center = InCenter;
	Dome->Shield = InShield;
	Dome->Pair = InPair;
	Dome->bShadow = bInShadow;
	Dome->TargetRadius = InRadius;
	Dome->Glass->SetWorldScale3D(FVector(0.01f));

	// Warm glass, coloured by the pair; as Schwelklee thick dark smoke
	FLinearColor Color = InShield->ShieldColor;
	switch (InPair)
	{
	case EVaelSymbiosis::Lichtkuppel:	Color = FLinearColor(1.0f, 0.9f, 0.55f); break;
	case EVaelSymbiosis::Glutglas:		Color = FLinearColor(1.0f, 0.5f, 0.15f); break;
	case EVaelSymbiosis::Funkelwind:	Color = FLinearColor(0.85f, 1.0f, 0.6f); break;
	case EVaelSymbiosis::Tauglocke:		Color = FLinearColor(0.4f, 0.95f, 0.9f); break;
	default: break;
	}
	if (UMaterialInterface* Material = VaelAssets::LoadOptional(UVaelFloromantSettings::Get()->GlassMaterial))
	{
		Dome->GlassMaterial = UMaterialInstanceDynamic::Create(Material, Dome);
		Dome->Glass->SetMaterial(0, Dome->GlassMaterial);
		if (bInShadow)
		{
			Dome->GlassMaterial->SetVectorParameterValue(TEXT("Color"), FLinearColor(0.1f, 0.08f, 0.09f));
			Dome->GlassMaterial->SetScalarParameterValue(TEXT("Glow"), 0.4f);
			Dome->GlassMaterial->SetScalarParameterValue(TEXT("Opacity"), 0.9f);
			Dome->GlassMaterial->SetScalarParameterValue(TEXT("Inner"), 0.35f);
		}
		else
		{
			// Barely visible: only the rim catches the light
			Dome->GlassMaterial->SetVectorParameterValue(TEXT("Color"), Color);
			Dome->GlassMaterial->SetScalarParameterValue(TEXT("Opacity"), 0.16f);
			Dome->GlassMaterial->SetScalarParameterValue(TEXT("Inner"), 0.01f);
		}
	}
	Dome->Light->SetLightColor(Color);
	Dome->Light->SetVisibility(!bInShadow);
	return Dome;
}

void AVaelGlimmDome::Lower()
{
	bLowering = true;
}

bool AVaelGlimmDome::Covers(const FVector& Location) const
{
	return Radius > 1.0f && FVector::DistSquared(Location, GetActorLocation()) < FMath::Square(Radius);
}

float AVaelGlimmDome::GetEnemyFireScale() const
{
	if (Pair == EVaelSymbiosis::Tauglocke)
	{
		return 0.0f;
	}
	return FMath::Lerp(Shield->EnemyFireScale, Shield->AnchoredEnemyFireScale, Anchored);
}

float AVaelGlimmDome::GetAlliedFireScale() const
{
	if (Pair == EVaelSymbiosis::Tauglocke)
	{
		return 1.0f;
	}
	const float Scale = FMath::Lerp(Shield->AlliedFireScale, Shield->AnchoredAlliedFireScale, Anchored);
	return Pair == EVaelSymbiosis::Glutglas ? FMath::Max(Scale, 1.5f) : Scale;
}

void AVaelGlimmDome::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (Shield == nullptr || (!bLowering && !Center.IsValid()))
	{
		bLowering = true;
	}
	if (const AActor* Followed = Center.Get())
	{
		SetActorLocation(Followed->GetActorLocation());
	}

	Radius = FMath::FInterpTo(Radius, bLowering ? 0.0f : TargetRadius, DeltaSeconds, bLowering ? 8.0f : 5.0f);
	if (bLowering && Radius < 5.0f)
	{
		Destroy();
		return;
	}
	Glass->SetWorldScale3D(FVector(Radius / DomeSphereRadius));

	// It lights the dark around it, brighter for a moment when it flashes
	Flash = FMath::Max(0.0f, Flash - DeltaSeconds * 2.0f);
	Light->SetAttenuationRadius(Radius * 2.2f);
	Light->SetIntensity(DomeLightIntensity * (Radius / 250.0f) * (1.0f + 4.0f * Flash) * (1.0f + 0.1f * FMath::Sin(GetWorld()->GetTimeSeconds() * 2.3f)));
	if (GlassMaterial != nullptr && !bShadow)
	{
		GlassMaterial->SetScalarParameterValue(TEXT("Glow"), 1.4f * (1.0f + 0.5f * Anchored) * (1.0f + 3.0f * Flash));
	}

	if (bLowering)
	{
		return;
	}

	// Funkelwind: the pollen turns every enemy shot that comes into the dome aside
	if (Pair == EVaelSymbiosis::Funkelwind)
	{
		for (TActorIterator<AVaelSpellProjectile> It(GetWorld()); It; ++It)
		{
			if (!DomeOnPlayerSide(It->GetInstigator()) && Covers(It->GetActorLocation()))
			{
				It->Block();
			}
		}
	}

	PulseCountdown -= DeltaSeconds;
	if (PulseCountdown <= 0.0f)
	{
		PulseCountdown = 1.0f;
		Pulse();
	}
}

void AVaelGlimmDome::Pulse()
{
	APawn* DomeOwner = GetInstigator();
	if (DomeOwner == nullptr)
	{
		return;
	}

	// Lichtkuppel: every few seconds the dome flares and blinds the enemies inside
	bool bFlare = false;
	if (Pair == EVaelSymbiosis::Lichtkuppel)
	{
		FlashCountdown -= 1.0f;
		if (FlashCountdown <= 0.0f)
		{
			FlashCountdown = 4.0f;
			Flash = 1.0f;
			bFlare = true;
		}
	}

	for (TActorIterator<AVaelCharacterBase> It(GetWorld()); It; ++It)
	{
		if (!Covers(It->GetActorLocation()) || !UVaelCombatStatics::CanDamage(DomeOwner, *It))
		{
			continue;
		}
		if (bShadow)
		{
			// Schwelklee: the smoke blinds and singes
			It->ApplyBlind(1.3f);
			FVaelSpellHit Singe;
			Singe.Damage = 2.0f;
			Singe.Element = EVaelElement::Fire;
			UVaelCombatStatics::ApplySpellHit(DomeOwner, *It, Singe, FVector::ZeroVector);
			continue;
		}
		switch (Pair)
		{
		case EVaelSymbiosis::Glutglas:
			UVaelCombatStatics::ApplyStatus(DomeOwner, *It, EVaelStatus::Burning, 2.0f);
			break;
		case EVaelSymbiosis::Tauglocke:
			UVaelCombatStatics::ApplyStatus(DomeOwner, *It, EVaelStatus::Wet, 3.0f);
			break;
		case EVaelSymbiosis::Lichtkuppel:
			if (bFlare)
			{
				It->ApplyBlind(1.5f);
			}
			break;
		default:
			break;
		}
	}

	// Tauglocke: dew falls inside and waters the plants
	if (Pair == EVaelSymbiosis::Tauglocke && !bShadow)
	{
		VaelWatering::WaterAround(GetWorld(), GetActorLocation(), Radius, 0.5f, DomeOwner);
	}
}

float AVaelGlimmDome::ScaleFireDamage(AVaelCharacterBase* Target, const AActor* Attacker, float Damage, const FGameplayTagContainer& DamageTags)
{
	if (Target == nullptr || Attacker == nullptr || Damage <= 0.0f || !DamageTags.HasTag(VaelTags::Element_Fire))
	{
		return Damage;
	}

	const bool bTargetOnPlayerSide = Target->IsOnPlayerSide();
	const bool bAttackerOnPlayerSide = DomeOnPlayerSide(Attacker);
	float Scale = 1.0f;
	AVaelGlimmDome* Swallower = nullptr;
	for (TActorIterator<AVaelGlimmDome> It(Target->GetWorld()); It; ++It)
	{
		AVaelGlimmDome* Dome = *It;
		if (!Dome->IsUp() || Dome->bShadow || Dome->Shield == nullptr)
		{
			continue;
		}
		// Enemy fire on her side inside is swallowed in part, the strongest dome counts
		if (bTargetOnPlayerSide && !bAttackerOnPlayerSide && Dome->Covers(Target->GetActorLocation()) && Dome->GetEnemyFireScale() < Scale)
		{
			Scale = Dome->GetEnemyFireScale();
			Swallower = Dome;
		}
		// Fire her side casts from inside burns hotter
		else if (bAttackerOnPlayerSide && !bTargetOnPlayerSide && Dome->Covers(Attacker->GetActorLocation()))
		{
			Scale = FMath::Max(Scale, Dome->GetAlliedFireScale());
		}
	}

	// What the glass swallows flows into the Floromant as Saft
	if (Swallower != nullptr)
	{
		const AVaelCharacterBase* DomeOwner = Cast<AVaelCharacterBase>(Swallower->GetInstigator());
		UAbilitySystemComponent* AbilitySystem = DomeOwner != nullptr ? DomeOwner->GetAbilitySystemComponent() : nullptr;
		if (AbilitySystem != nullptr)
		{
			AbilitySystem->ApplyModToAttribute(UVaelAttributeSet::GetManaAttribute(), EGameplayModOp::Additive, Damage * (1.0f - Scale) * Swallower->Shield->AbsorbSaftShare);
		}
		Swallower->Flash = FMath::Max(Swallower->Flash, 0.4f);
	}
	return Damage * Scale;
}

bool AVaelGlimmDome::IsFireSourceAt(const UWorld* World, const FVector& Location)
{
	if (World == nullptr)
	{
		return false;
	}
	for (TActorIterator<AVaelGlimmDome> It(World); It; ++It)
	{
		if (It->IsUp() && !It->bShadow && It->Pair != EVaelSymbiosis::Tauglocke && It->Covers(Location))
		{
			return true;
		}
	}
	return false;
}
