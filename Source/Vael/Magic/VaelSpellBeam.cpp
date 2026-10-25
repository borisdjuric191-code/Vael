// Copyright Epic Games, Inc. All Rights Reserved.

#include "Magic/VaelSpellBeam.h"
#include "UObject/ConstructorHelpers.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "Combat/VaelCharacterBase.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Pawn.h"
#include "Magic/VaelFormulaAbility.h"
#include "Magic/VaelGameplayTags.h"
#include "Magic/VaelGroundArea.h"
#include "Magic/VaelSpellEffects.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "NiagaraComponent.h"
#include "VaelAssets.h"
#include "Components/PointLightComponent.h"
#include "Magic/VaelDebrisBurst.h"
#include "World/VaelFireComponent.h"
#include "World/VaelGround.h"

namespace
{
	/** Size of the engine cylinder mesh used as placeholder */
	constexpr float BeamPlaceholderCylinderRadius = 50.0f;
	constexpr float BeamPlaceholderCylinderHeight = 100.0f;

	/** Thickness of the placeholder beam in cm */
	constexpr float BeamLookRadius = 22.0f;

	/** Fires are laid this far before the end of the beam, so they don't end up inside a wall (prototype: 0.3 tiles) */
	constexpr float FireBackOffDistance = 42.0f;

	/** How far below the end of the beam the ground is searched */
	constexpr float GroundSearchDepth = 500.0f;

	/** How far above it the search starts, so rising ground is found too */
	constexpr float GroundSearchHeight = 80.0f;
}

AVaelSpellBeam::AVaelSpellBeam()
{
	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Mesh->SetCastShadow(false);
	Mesh->bReceivesDecals = false;
	RootComponent = Mesh;

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CylinderMesh(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	if (CylinderMesh.Succeeded())
	{
		Mesh->SetStaticMesh(CylinderMesh.Object);
	}

	static ConstructorHelpers::FObjectFinder<UMaterialInterface> CylinderMaterial(TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
	if (CylinderMaterial.Succeeded())
	{
		Mesh->SetMaterial(0, CylinderMaterial.Object);
	}

	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickGroup = TG_PostPhysics;
}

AVaelSpellBeam* AVaelSpellBeam::StartBeam(APawn* Caster, const FVaelBeamSettings& InSettings)
{
	UWorld* World = Caster != nullptr ? Caster->GetWorld() : nullptr;
	const UAbilitySystemComponent* AbilitySystem = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Caster);

	// One beam at a time
	if (World == nullptr || (AbilitySystem != nullptr && AbilitySystem->HasMatchingGameplayTag(VaelTags::State_Channeling)))
	{
		return nullptr;
	}

	const FTransform SpawnTransform(Caster->GetActorLocation());

	AVaelSpellBeam* Beam = World->SpawnActorDeferred<AVaelSpellBeam>(AVaelSpellBeam::StaticClass(), SpawnTransform, Caster, Caster, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (Beam != nullptr)
	{
		Beam->Settings = InSettings;
		Beam->FinishSpawning(SpawnTransform);
	}

	return Beam;
}

void AVaelSpellBeam::BeginPlay()
{
	Super::BeginPlay();

	EndTime = GetWorld()->GetTimeSeconds() + Settings.Duration;
	NextFireCountdown = 0.0f;

	if (UMaterialInstanceDynamic* Material = Mesh->CreateAndSetMaterialInstanceDynamic(0))
	{
		Material->SetVectorParameterValue(TEXT("Color"), Settings.Color);
	}

	// A jet of pack fire along the whole reach, or else the beam effect, replaces the placeholder cylinder as soon as it exists
	if (Settings.Jet != nullptr)
	{
		const UVaelFireSettings* FireSettings = UVaelFireSettings::Get();
		const int32 NumFlames = FMath::Clamp(FMath::CeilToInt(Settings.Length / FireSettings->JetSpacing), 1, 16);
		for (int32 Index = 0; Index < NumFlames; ++Index)
		{
			// Narrow at the hands, widening towards the end
			const float Share = (Index + 0.5f) / NumFlames;
			// Not destroyed when it stops: a flame behind a wall starts again once the beam reaches past it
			if (UNiagaraComponent* Flame = VaelEffects::Attach(Settings.Jet, Mesh, NAME_None, Settings.Color, FMath::Lerp(BeamLookRadius, Settings.HalfWidth, Share), false))
			{
				Flame->SetUsingAbsoluteLocation(true);
				Flame->SetUsingAbsoluteRotation(true);
				for (const TPair<FName, float>& Value : FireSettings->JetFloats)
				{
					Flame->SetVariableFloat(Value.Key, Value.Value);
				}
				JetFlames.Add(Flame);
			}
		}
		Mesh->SetVisibility(JetFlames.IsEmpty());
	}

	// A fire beam is a cylinder of concentrated, streaming fire
	if (Settings.bFireCore)
	{
		const UVaelFireSettings* FireSettings = UVaelFireSettings::Get();
		if (UMaterialInterface* CoreMaterial = VaelAssets::LoadOptional(FireSettings->BeamCoreMaterial))
		{
			Mesh->SetMaterial(0, CoreMaterial);
			CoreMaterialInstance = Mesh->CreateAndSetMaterialInstanceDynamic(0);
			CoreRadius = FireSettings->BeamCoreRadius;
			Mesh->SetVisibility(true);

			// As in the prototype: a wide, cool, see-through glow around the body of flames and a thin white-hot core in it
			for (const bool bOuter : { true, false })
			{
				UStaticMeshComponent* Layer = NewObject<UStaticMeshComponent>(this);
				Layer->SetStaticMesh(Mesh->GetStaticMesh());
				Layer->SetCollisionEnabled(ECollisionEnabled::NoCollision);
				Layer->SetCastShadow(false);
				Layer->SetupAttachment(Mesh);
				Layer->SetUsingAbsoluteLocation(true);
				Layer->SetUsingAbsoluteRotation(true);
				Layer->SetUsingAbsoluteScale(true);
				Layer->RegisterComponent();
				Layer->SetMaterial(0, CoreMaterial);
				UMaterialInstanceDynamic* LayerMaterial = Layer->CreateAndSetMaterialInstanceDynamic(0);
				LayerMaterial->SetScalarParameterValue(TEXT("Hitze"), bOuter ? FireSettings->BeamGlowHeat : FireSettings->BeamCoreHeat);
				LayerMaterial->SetScalarParameterValue(TEXT("Deckkraft"), bOuter ? FireSettings->BeamGlowOpacity : 1.0f);
				LayerMaterial->SetScalarParameterValue(TEXT("Tempo"), FireSettings->BeamFlowSpeed * (bOuter ? 0.7f : 1.4f));
				(bOuter ? OuterLayer : InnerLayer) = Layer;
				LayerMaterials.Add(LayerMaterial);
			}
			CoreMaterialInstance->SetScalarParameterValue(TEXT("Tempo"), FireSettings->BeamFlowSpeed);

			// Its source: a calm ball of fire at the hands; its end: fire bursting where it hits
			for (const bool bSource : { true, false })
			{
				UVaelFireComponent* Flames = NewObject<UVaelFireComponent>(this);
				Flames->SetupAttachment(Mesh);
				Flames->SetUsingAbsoluteLocation(true);
				Flames->SetUsingAbsoluteRotation(true);
				Flames->SetUsingAbsoluteScale(true);
				Flames->RegisterComponent();
				FVaelFireShape Ball;
				Ball.Radius = bSource ? FireSettings->BeamSourceRadius : FireSettings->BeamImpactRadius;
				Ball.Height = Ball.Radius * 2.0f;
				Ball.bBall = true;
				Ball.bFloating = true;
				Ball.bLight = false;
				Flames->Ignite(Ball, 0.1f);
				(bSource ? SourceFlames : ImpactFlames) = Flames;
			}

			// The source: a white-hot ball of the same fire at the hands
			SourceGlow = NewObject<UStaticMeshComponent>(this);
			SourceGlow->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Sphere.Sphere")));
			SourceGlow->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			SourceGlow->SetCastShadow(false);
			SourceGlow->SetupAttachment(Mesh);
			SourceGlow->SetUsingAbsoluteLocation(true);
			SourceGlow->SetUsingAbsoluteRotation(true);
			SourceGlow->SetUsingAbsoluteScale(true);
			SourceGlow->RegisterComponent();
			SourceGlow->SetMaterial(0, CoreMaterial);
			if (UMaterialInstanceDynamic* GlowMaterial = SourceGlow->CreateAndSetMaterialInstanceDynamic(0))
			{
				GlowMaterial->SetScalarParameterValue(TEXT("Hitze"), FireSettings->BeamCoreHeat);
				GlowMaterial->SetScalarParameterValue(TEXT("Kacheln"), 2.0f);
			}

			// Light along the beam
			for (int32 Index = 0; Index < FireSettings->BeamLights; ++Index)
			{
				UPointLightComponent* Light = NewObject<UPointLightComponent>(this);
				Light->SetupAttachment(Mesh);
				Light->SetUsingAbsoluteLocation(true);
				Light->SetUsingAbsoluteScale(true);
				Light->SetLightColor(FLinearColor(1.0f, 0.45f, 0.12f));
				Light->SetAttenuationRadius(450.0f);
				Light->SetCastShadows(false);
				Light->RegisterComponent();
				BeamLights.Add(Light);
			}
		}
	}
	VisualComponent = JetFlames.IsEmpty() && CoreRadius <= 0.0f ? VaelEffects::Attach(Settings.Visual, Mesh, NAME_None, Settings.Color, BeamLookRadius) : nullptr;
	if (VisualComponent != nullptr)
	{
		VisualComponent->SetUsingAbsoluteLocation(true);
		VisualComponent->SetUsingAbsoluteRotation(true);
		Mesh->SetVisibility(false);
	}

	CasterAbilitySystem = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(GetInstigator());
	if (CasterAbilitySystem.IsValid())
	{
		CasterAbilitySystem->AddLooseGameplayTag(VaelTags::State_Channeling);
	}

	// The first moment already burns and lays the first fire
	UpdateBeam(0.0f);
}

void AVaelSpellBeam::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (CasterAbilitySystem.IsValid())
	{
		CasterAbilitySystem->RemoveLooseGameplayTag(VaelTags::State_Channeling);
	}

	Super::EndPlay(EndPlayReason);
}

void AVaelSpellBeam::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	// The beam dies with the strength of its caster
	const AVaelCharacterBase* Caster = Cast<AVaelCharacterBase>(GetInstigator());
	if (Caster == nullptr || Caster->IsDefeated() || GetWorld()->GetTimeSeconds() >= EndTime)
	{
		Destroy();
		return;
	}

	UpdateBeam(DeltaSeconds);
}

void AVaelSpellBeam::UpdateBeam(float DeltaSeconds)
{
	APawn* Caster = GetInstigator();
	UWorld* World = GetWorld();
	if (Caster == nullptr || World == nullptr)
	{
		return;
	}

	const FVector Start = Caster->GetActorLocation();
	const FVector Direction = UVaelFormulaAbility::GetAimDirection(Caster);

	// Walls stop the beam
	float Length = Settings.Length;

	FHitResult WallHit;
	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(VaelSpellBeam), false, Caster);
	if (World->LineTraceSingleByObjectType(WallHit, Start, Start + Direction * Length, FCollisionObjectQueryParams(ECC_WorldStatic), QueryParams))
	{
		Length = WallHit.Distance;
	}

	UpdateLook(Start, Direction, Length);

	// Damage collects on everyone close to the middle line of the beam
	for (TActorIterator<AVaelCharacterBase> It(World); It; ++It)
	{
		AVaelCharacterBase* Target = *It;
		if (!UVaelCombatStatics::CanDamage(Caster, Target))
		{
			continue;
		}

		const FVector ToTarget = Target->GetActorLocation() - Start;
		const float Along = FVector::DotProduct(FVector(ToTarget.X, ToTarget.Y, 0.0f), Direction);
		if (Along < 0.0f || Along > Length)
		{
			continue;
		}

		const float Across = FMath::Abs(Direction.X * ToTarget.Y - Direction.Y * ToTarget.X);
		if (Across >= Settings.HalfWidth + Target->GetCapsuleComponent()->GetScaledCapsuleRadius() * 0.5f)
		{
			continue;
		}

		const float Dealt = CollectedDamage.Add(Target, Settings.DamagePerSecond * DeltaSeconds, Settings.DamageStep);
		if (Dealt > 0.0f)
		{
			FVaelSpellHit Hit = Settings.Hit;
			Hit.Damage = Dealt;

			UVaelCombatStatics::ApplySpellHit(Caster, Target, Hit, Direction);
		}
	}

	// The ground catches fire where the beam ends
	if (Settings.FireInterval > 0.0f)
	{
		NextFireCountdown -= DeltaSeconds;
		if (NextFireCountdown <= 0.0f)
		{
			NextFireCountdown = Settings.FireInterval;

			const FVector FireLocation = Start + Direction * FMath::Max(Length - FireBackOffDistance, 0.0f);

			FHitResult GroundHit;
			if (VaelGround::TraceGround(World, FireLocation + FVector(0.0f, 0.0f, GroundSearchHeight), FireLocation - FVector(0.0f, 0.0f, GroundSearchDepth), GroundHit, Caster))
			{
				AVaelGroundArea::SpawnArea(World, GroundHit.Location + FVector(0.0f, 0.0f, 2.0f), Settings.Hit.Element, Settings.FireRadius, Settings.FireLifetime,
					Settings.FireDamagePerSecond, Caster);
			}
		}
	}
}

void AVaelSpellBeam::UpdateLook(const FVector& Body, const FVector& Direction, float Length)
{
	// It leaves from in front of the hands, not from inside the body
	const UVaelFireSettings* HandSettings = UVaelFireSettings::Get();
	const FVector Start = Body + Direction * HandSettings->BeamHandReach + FVector(0.0f, 0.0f, HandSettings->BeamHandHeight);
	const float VisibleLength = FMath::Max(Length - HandSettings->BeamHandReach, 1.0f);

	if (VisualComponent != nullptr)
	{
		VisualComponent->SetWorldLocationAndRotation(Start, Direction.Rotation());
		VisualComponent->SetVariableVec3(VaelEffects::BeamEndParameter, Start + Direction * VisibleLength);
	}

	// The jet flames spread along the reach up to the wall, their wind blowing outwards; those behind a wall stop emitting
	const float Spacing = UVaelFireSettings::Get()->JetSpacing;
	for (int32 Index = 0; Index < JetFlames.Num(); ++Index)
	{
		UNiagaraComponent* Flame = JetFlames[Index];
		const float Along = (Index + 0.5f) * Spacing;
		Flame->SetWorldLocation(Start + Direction * FMath::Min(Along, VisibleLength));
		VaelFire::SetWind(Flame, Direction * 100.0f, UVaelFireSettings::Get()->FlightWindSpeed);
		const bool bReaches = Along - Spacing * 0.5f < VisibleLength;
		if (bReaches != Flame->IsActive())
		{
			bReaches ? Flame->Activate() : Flame->Deactivate();
		}
	}

	// The cylinder stands on its axis, which is turned into the aim direction
	const float LookRadius = CoreRadius > 0.0f ? CoreRadius : BeamLookRadius;
	if (CoreMaterialInstance != nullptr)
	{
		// The fire texture repeats along the beam at its own size, however long the beam is
		CoreMaterialInstance->SetScalarParameterValue(TEXT("Kacheln"), VisibleLength / UVaelFireSettings::Get()->BeamCoreTileLength);
	}
	// A fire beam throbs: its thickness flickers fast, as in the prototype
	const float Time = GetWorld()->GetTimeSeconds();
	const float Throb = CoreRadius > 0.0f ? 1.0f + 0.14f * FMath::Sin(Time * 40.0f) + 0.07f * FMath::Sin(Time * 23.0f + 1.3f) : 1.0f;
	const FRotator Along = FRotationMatrix::MakeFromZ(Direction).Rotator();
	const auto Stretch = [&](UStaticMeshComponent* Cylinder, float Thickness)
	{
		Cylinder->SetWorldLocationAndRotation(Start + Direction * VisibleLength * 0.5f, Along);
		Cylinder->SetWorldScale3D(FVector(Thickness / BeamPlaceholderCylinderRadius, Thickness / BeamPlaceholderCylinderRadius, VisibleLength / BeamPlaceholderCylinderHeight));
	};
	Stretch(Mesh, LookRadius * Throb);
	if (OuterLayer != nullptr)
	{
		const UVaelFireSettings* FireSettings = UVaelFireSettings::Get();
		Stretch(OuterLayer, LookRadius * FireSettings->BeamGlowShare * (1.0f + 0.1f * FMath::Sin(Time * 31.0f + 0.4f)));
		Stretch(InnerLayer, LookRadius * FireSettings->BeamCoreShare * Throb);
		for (UMaterialInstanceDynamic* LayerMaterial : LayerMaterials)
		{
			LayerMaterial->SetScalarParameterValue(TEXT("Kacheln"), VisibleLength / FireSettings->BeamCoreTileLength);
		}

		const FVector End = Start + Direction * VisibleLength;
		SourceFlames->SetWorldLocation(Start);
		ImpactFlames->SetWorldLocation(End - Direction * 20.0f);
		if (SourceGlow != nullptr)
		{
			SourceGlow->SetWorldLocation(Start);
			SourceGlow->SetWorldScale3D(FVector(FireSettings->BeamSourceRadius * 0.8f / 50.0f * Throb));
		}
		for (int32 Index = 0; Index < BeamLights.Num(); ++Index)
		{
			BeamLights[Index]->SetWorldLocation(Start + Direction * VisibleLength * (Index + 0.5f) / BeamLights.Num() + FVector(0.0f, 0.0f, 40.0f));
			BeamLights[Index]->SetIntensity(FireSettings->BeamLightIntensity * Throb);
		}

		// Where it hits, sparks shoot out all the time, flames splash up now and then, and fire keeps bursting and spraying back
		const float DeltaSeconds = GetWorld()->GetDeltaSeconds();
		SparkCountdown -= DeltaSeconds;
		if (SparkCountdown <= 0.0f)
		{
			SparkCountdown = FireSettings->BeamSparkInterval;
			FVaelDebris Sparks;
			Sparks.Count = FireSettings->BeamSparksPerBurst;
			Sparks.Spread = 20.0f;
			Sparks.Speed = 650.0f;
			Sparks.Lift = 420.0f;
			Sparks.Gravity = 900.0f;
			Sparks.Drag = 1.2f;
			Sparks.Lifetime = 0.55f;
			Sparks.MinSize = 2.5f;
			Sparks.MaxSize = 6.0f;
			Sparks.Color = FLinearColor(1.0f, 0.55f, 0.12f);
			Sparks.Glow = 12.0f;
			AVaelDebrisBurst::Spawn(this, End - Direction * 25.0f, Sparks);
		}
		SplashCountdown -= DeltaSeconds;
		if (SplashCountdown <= 0.0f)
		{
			SplashCountdown = FireSettings->BeamSplashInterval;
			VaelEffects::SpawnAt(this, VaelAssets::LoadOptional(FireSettings->BurstNiagara), End - Direction * 30.0f, FRotator::ZeroRotator, Settings.Color, FireSettings->BeamImpactRadius * 1.2f);
		}
		ImpactCountdown -= DeltaSeconds;
		if (ImpactCountdown <= 0.0f)
		{
			ImpactCountdown = FireSettings->BeamImpactInterval;
			if (UNiagaraComponent* Burst = VaelEffects::SpawnAt(this, VaelAssets::LoadOptional(FireSettings->SparkHitNiagara), End + FMath::VRand() * 15.0f, FRotator::ZeroRotator,
				Settings.Color, FireSettings->BeamImpactRadius))
			{
				VaelFire::SetWind(Burst, (-Direction + FVector(0.0f, 0.0f, 0.8f)).GetSafeNormal() * 100.0f, UVaelFireSettings::Get()->FlightWindSpeed);
			}
		}
	}
}
