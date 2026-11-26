// Copyright Epic Games, Inc. All Rights Reserved.

#include "Engineer/VaelEngineerStage2Actors.h"
#include "Combat/VaelCharacterBase.h"
#include "Combat/VaelCombatStatics.h"
#include "Components/PointLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Engineer/VaelEngineerComponent.h"
#include "Engineer/VaelEngineerGear.h"
#include "EngineUtils.h"
#include "Magic/VaelGroundArea.h"
#include "Magic/VaelMagicSettings.h"
#include "Magic/VaelSpellEffects.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "NiagaraSystem.h"
#include "Player/VaelCharacter.h"
#include "VaelAssets.h"

namespace
{
	/** The Glutei: how far it feels warmth, how fast it crawls, how close it bursts, its fire round, how long it waits (s) */
	constexpr float EggSense = 650.0f;
	constexpr float EggCrawl = 380.0f;
	constexpr float EggBurstReach = 80.0f;
	constexpr float EggBurstRadius = 230.0f;
	constexpr float EggLife = 30.0f;
	/** The wind devils: small and big radius, life (s), speed, how often they cut, how hard they pull light enemies */
	constexpr float DevilRadius = 150.0f;
	constexpr float BigDevilRadius = 300.0f;
	constexpr float DevilLife = 3.0f;
	constexpr float BigDevilLife = 5.0f;
	constexpr float DevilSpeed = 550.0f;
	constexpr float BigDevilSpeed = 320.0f;
	constexpr float DevilCutEvery = 0.3f;
	constexpr float DevilPull = 260.0f;
	/** Enemies up to this collision radius are light enough to be pulled in */
	constexpr float LightRadius = 70.0f;
	const TCHAR* const SickleMesh = TEXT("/Game/Vael/Characters/Ingenieur/Werkstuecke/SM_Ing_Doppelsichel_Tripo.SM_Ing_Doppelsichel_Tripo");

	bool IsFoe(const AActor* Thrower, const AVaelCharacterBase* Each)
	{
		return Each != nullptr && !Each->IsA<AVaelCharacter>() && Each->GetHealth() > 0.0f && UVaelCombatStatics::CanDamage(Thrower, Each);
	}
}

// ---------------------------------------------------------------- Glutei

AVaelGlutEgg::AVaelGlutEgg()
{
	PrimaryActorTick.bCanEverTick = true;
	Body = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Body"));
	Body->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	RootComponent = Body;
	Glow = CreateDefaultSubobject<UPointLightComponent>(TEXT("Glow"));
	Glow->SetupAttachment(Body);
	Glow->SetLightColor(FLinearColor(1.0f, 0.42f, 0.1f));
	Glow->SetIntensity(1800.0f);
	Glow->SetAttenuationRadius(220.0f);
	Glow->SetCastShadows(false);
}

AVaelGlutEgg* AVaelGlutEgg::Lay(UVaelEngineerComponent* Engineer, const FVector& Location, float InDamage)
{
	UWorld* World = Engineer != nullptr ? Engineer->GetWorld() : nullptr;
	if (World == nullptr)
	{
		return nullptr;
	}
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AVaelGlutEgg* Egg = World->SpawnActor<AVaelGlutEgg>(AVaelGlutEgg::StaticClass(), Location, FRotator::ZeroRotator, Params);
	if (Egg == nullptr)
	{
		return nullptr;
	}
	Egg->Engineer = Engineer;
	Egg->Damage = InDamage;
	if (UStaticMesh* Mesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Sphere.Sphere"), nullptr, LOAD_NoWarn | LOAD_Quiet))
	{
		Egg->Body->SetStaticMesh(Mesh);
		Egg->Body->SetWorldScale3D(FVector(0.32f, 0.32f, 0.42f));
		if (UMaterialInterface* Base = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Vael/Refugium/Glutkammer/Probe/M_GK_Glas.M_GK_Glas"), nullptr, LOAD_NoWarn | LOAD_Quiet))
		{
			UMaterialInstanceDynamic* Dynamic = UMaterialInstanceDynamic::Create(Base, Egg);
			Dynamic->SetVectorParameterValue(TEXT("Farbe"), FLinearColor(0.35f, 0.08f, 0.01f));
			Dynamic->SetVectorParameterValue(TEXT("Rand"), FLinearColor(2.4f, 0.8f, 0.12f));
			Egg->Body->SetMaterial(0, Dynamic);
		}
	}
	return Egg;
}

void AVaelGlutEgg::SetUnderground(bool bUnder)
{
	bUnderground = bUnder;
	Body->SetVisibility(!bUnder);
	Glow->SetVisibility(!bUnder);
}

AVaelCharacterBase* AVaelGlutEgg::GetPrey() const
{
	return Prey.Get();
}

AVaelCharacterBase* AVaelGlutEgg::FindPrey() const
{
	const UVaelEngineerComponent* Eng = Engineer.Get();
	const AActor* Thrower = Eng != nullptr ? Eng->GetOwner() : nullptr;
	AVaelCharacterBase* Best = nullptr;
	float BestScore = 1.0e9f;
	for (TActorIterator<AVaelCharacterBase> It(GetWorld()); It; ++It)
	{
		AVaelCharacterBase* Each = *It;
		if (!IsFoe(Thrower, Each) || UVaelCombatStatics::HasStatus(Each, EVaelStatus::Wet) || UVaelCombatStatics::HasStatus(Each, EVaelStatus::Frozen))
		{
			continue;
		}
		const float Distance = FVector::Dist2D(Each->GetActorLocation(), GetActorLocation());
		if (Distance > EggSense)
		{
			continue;
		}
		// the warmest first: burning enemies count as half as far
		const float Score = Distance * (UVaelCombatStatics::HasStatus(Each, EVaelStatus::Burning) ? 0.5f : 1.0f);
		if (Score < BestScore)
		{
			BestScore = Score;
			Best = Each;
		}
	}
	return Best;
}

void AVaelGlutEgg::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	Age += DeltaSeconds;
	if (!Engineer.IsValid() || Age > EggLife)
	{
		Destroy();
		return;
	}
	// it breathes like the eggs in the queen's chambers
	Glow->SetIntensity(1400.0f + 900.0f * FMath::Sin(Age * 4.0f));
	if (Age >= NextLook)
	{
		NextLook = Age + 0.25f;
		AVaelCharacterBase* Warmest = FindPrey();
		Prey = Warmest != nullptr ? Warmest : Prey.Get();
		if (Prey.IsValid() && (Prey->GetHealth() <= 0.0f || UVaelCombatStatics::HasStatus(Prey.Get(), EVaelStatus::Wet)))
		{
			Prey.Reset();
		}
	}
	if (AVaelCharacterBase* Target = Prey.Get())
	{
		FVector Way = Target->GetActorLocation() - GetActorLocation();
		Way.Z = 0.0f;
		// under the earth: a trail of dust where it digs, and it breaks out just before it reaches the prey
		if (bUnderground)
		{
			if (Age >= NextDust)
			{
				NextDust = Age + 0.12f;
				const UVaelMagicSettings* Magic = UVaelMagicSettings::Get();
				VaelEffects::SpawnAt(this, VaelAssets::LoadOptional(Magic->DustPuffEffect), GetActorLocation(), FRotator::ZeroRotator, FLinearColor(0.55f, 0.32f, 0.14f), 40.0f);
			}
			if (Way.Size() < 180.0f)
			{
				SetUnderground(false);
				const UVaelMagicSettings* Magic = UVaelMagicSettings::Get();
				VaelEffects::SpawnAt(this, VaelAssets::LoadOptional(Magic->DustBurstEffect), GetActorLocation(), FRotator::ZeroRotator, FLinearColor(1.0f, 0.45f, 0.12f), 80.0f);
			}
		}
		if (Way.Size() <= EggBurstReach + Target->GetSimpleCollisionRadius() * 0.5f)
		{
			Burst();
			return;
		}
		SetActorLocation(GetActorLocation() + Way.GetSafeNormal() * EggCrawl * DeltaSeconds);
		SetActorRotation(FRotator(0.0f, Way.Rotation().Yaw, FMath::Sin(Age * 18.0f) * 10.0f));
	}
}

void AVaelGlutEgg::Burst()
{
	bBurst = true;
	UVaelEngineerComponent* Eng = Engineer.Get();
	const AActor* Thrower = Eng != nullptr ? Eng->GetOwner() : nullptr;
	const UVaelMagicSettings* Magic = UVaelMagicSettings::Get();
	VaelEffects::SpawnAt(this, VaelAssets::LoadOptional(Magic->DustBurstEffect), GetActorLocation(), FRotator::ZeroRotator, FLinearColor(1.0f, 0.4f, 0.08f), EggBurstRadius * 0.6f);
	for (TActorIterator<AVaelCharacterBase> It(GetWorld()); It; ++It)
	{
		if (IsFoe(Thrower, *It) && FVector::Dist2D(It->GetActorLocation(), GetActorLocation()) < EggBurstRadius + It->GetSimpleCollisionRadius())
		{
			// as a Glutladung: fire and burning; no weapon, so it feeds no chamber
			Eng->ApplyHit(*It, It->GetActorLocation(), Damage, TEXT("Brand"), GetActorLocation(), NAME_None);
		}
	}
	Destroy();
}

// ---------------------------------------------------------------- wind devils

AVaelWindDevil::AVaelWindDevil()
{
	PrimaryActorTick.bCanEverTick = true;
	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent = Root;
	BladeA = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BladeA"));
	BladeB = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BladeB"));
	for (UStaticMeshComponent* Blade : { BladeA.Get(), BladeB.Get() })
	{
		Blade->SetupAttachment(Root);
		Blade->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}
}

AVaelWindDevil* AVaelWindDevil::Release(UVaelEngineerComponent* Engineer, const FVector& From, const FVector& InDirection, float InDamage, bool bInBig)
{
	UWorld* World = Engineer != nullptr ? Engineer->GetWorld() : nullptr;
	if (World == nullptr)
	{
		return nullptr;
	}
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AVaelWindDevil* Devil = World->SpawnActor<AVaelWindDevil>(AVaelWindDevil::StaticClass(), From, FRotator::ZeroRotator, Params);
	if (Devil == nullptr)
	{
		return nullptr;
	}
	Devil->Engineer = Engineer;
	Devil->Direction = InDirection.GetSafeNormal2D().IsNearlyZero() ? FVector::ForwardVector : InDirection.GetSafeNormal2D();
	Devil->Damage = InDamage;
	Devil->bBig = bInBig;
	Devil->Radius = bInBig ? BigDevilRadius : DevilRadius;
	Devil->Life = bInBig ? BigDevilLife : DevilLife;
	Devil->Speed = bInBig ? BigDevilSpeed : DevilSpeed;
	if (UStaticMesh* Mesh = LoadObject<UStaticMesh>(nullptr, SickleMesh, nullptr, LOAD_NoWarn | LOAD_Quiet))
	{
		const float Scale = bInBig ? 1.3f : 0.8f;
		Devil->BladeA->SetStaticMesh(Mesh);
		Devil->BladeB->SetStaticMesh(Mesh);
		Devil->BladeA->SetRelativeScale3D(FVector(Scale));
		Devil->BladeB->SetRelativeScale3D(FVector(Scale));
	}
	return Devil;
}

void AVaelWindDevil::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	Age += DeltaSeconds;
	UVaelEngineerComponent* Eng = Engineer.Get();
	if (Eng == nullptr || Age > Life)
	{
		Destroy();
		return;
	}
	const AActor* Thrower = Eng->GetOwner();
	// it wanders on, swaying a little, and its blades whirl round it, two rings at different heights
	const FVector Sway = FVector::CrossProduct(FVector::UpVector, Direction) * FMath::Sin(Age * 3.0f) * 40.0f * DeltaSeconds * 3.0f;
	SetActorLocation(GetActorLocation() + Direction * Speed * DeltaSeconds + Sway);
	const float Spin = Age * (bBig ? 620.0f : 900.0f);
	const float Ring = Radius * 0.55f;
	BladeA->SetRelativeLocation(FRotator(0.0f, Spin, 0.0f).RotateVector(FVector(Ring, 0.0f, 40.0f)));
	BladeB->SetRelativeLocation(FRotator(0.0f, Spin + 180.0f, 0.0f).RotateVector(FVector(Ring * 0.7f, 0.0f, 120.0f)));
	BladeA->SetRelativeRotation(FRotator(0.0f, Spin * 2.0f, 0.0f));
	BladeB->SetRelativeRotation(FRotator(0.0f, -Spin * 2.0f, 0.0f));

	// what it crosses it takes along: fire makes it burn, smoke makes it blind (Umwelt)
	bBurning |= AVaelGroundArea::IsFireNear(GetWorld(), GetActorLocation(), Radius * 0.3f);
	bSmoking |= Eng->IsSmokeAt(GetActorLocation());
	if (Age >= NextPuff)
	{
		NextPuff = Age + 0.12f;
		const UVaelMagicSettings* Magic = UVaelMagicSettings::Get();
		const FLinearColor Tint = bBurning ? FLinearColor(0.9f, 0.35f, 0.08f) : bSmoking ? FLinearColor(0.2f, 0.19f, 0.18f) : FLinearColor(0.42f, 0.4f, 0.38f);
		const FVector Puff = GetActorLocation() + FRotator(0.0f, Spin, 0.0f).RotateVector(FVector(Radius * 0.4f, 0.0f, FMath::FRandRange(0.0f, 160.0f)));
		VaelEffects::SpawnAt(this, VaelAssets::LoadOptional(Magic->SteamEffect), Puff, FRotator::ZeroRotator, Tint, Radius * 0.35f);
	}
	// light enemies are drawn into its middle; every so often all inside are cut
	const bool bCut = Age >= NextCut;
	if (bCut)
	{
		NextCut = Age + DevilCutEvery;
	}
	for (TActorIterator<AVaelCharacterBase> It(GetWorld()); It; ++It)
	{
		AVaelCharacterBase* Each = *It;
		if (!IsFoe(Thrower, Each))
		{
			continue;
		}
		FVector ToMiddle = GetActorLocation() - Each->GetActorLocation();
		ToMiddle.Z = 0.0f;
		if (ToMiddle.Size() > Radius + Each->GetSimpleCollisionRadius())
		{
			continue;
		}
		if (Each->GetSimpleCollisionRadius() <= LightRadius && ToMiddle.Size() > 30.0f)
		{
			Each->SetActorLocation(Each->GetActorLocation() + ToMiddle.GetSafeNormal() * DevilPull * DeltaSeconds, true);
		}
		if (bCut)
		{
			++Cuts;
			Eng->ApplyHit(Each, Each->GetActorLocation(), Damage, bBurning ? FName(TEXT("Brand")) : bSmoking ? FName(TEXT("Rauch")) : VaelEngineerGear::Standard, GetActorLocation(), NAME_None);
		}
	}
}
