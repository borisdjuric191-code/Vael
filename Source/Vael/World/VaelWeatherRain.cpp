// Copyright Epic Games, Inc. All Rights Reserved.

#include "World/VaelWeatherRain.h"
#include "Camera/PlayerCameraManager.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "Magic/VaelCloudCards.h"
#include "Magic/VaelElementSource.h"
#include "Magic/VaelMagicSettings.h"
#include "Magic/VaelWaterOrb.h"
#include "Magic/VaelWaterPuddle.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "ProceduralMeshComponent.h"
#include "VaelAssets.h"
#include "World/VaelGround.h"
#include "World/VaelRegion.h"
#include "World/VaelWorldSettings.h"

namespace
{
	/** Splashes and rings that can be on the ground at once, and the pieces of a ring */
	constexpr int32 RainMaxSplashes = 700;
	constexpr int32 RainMaxRings = 160;
	constexpr int32 RainRingSegments = 14;

	/** Rings on a puddle are bigger and last longer than the small dark ones on wet ground */
	constexpr float RainPuddleRingRadius = 26.0f;
	constexpr float RainGroundRingRadius = 13.0f;

	/** Vertex color of the wave material: R how pale (crest), G foam, B 1 for a round drop, A how solid */
	FLinearColor RainVertex(float Pale, float Foam, bool bDrop, float Solid)
	{
		return FLinearColor(Pale, Foam, bDrop ? 1.0f : 0.0f, Solid);
	}
}

AVaelWeatherRain::AVaelWeatherRain()
{
	PrimaryActorTick.bCanEverTick = true;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
}

AVaelWeatherRain* AVaelWeatherRain::Spawn(AVaelRegion* InRegion)
{
	UWorld* World = InRegion != nullptr ? InRegion->GetWorld() : nullptr;
	if (World == nullptr)
	{
		return nullptr;
	}

	FActorSpawnParameters Parameters;
	Parameters.Owner = InRegion;
	Parameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AVaelWeatherRain* Rain = World->SpawnActor<AVaelWeatherRain>(AVaelWeatherRain::StaticClass(), FTransform::Identity, Parameters);
	if (Rain != nullptr)
	{
		Rain->Region = InRegion;
		Rain->Build();
	}

	return Rain;
}

AVaelWeatherRain* AVaelWeatherRain::SpawnPatch(UWorld* World, const FVector& Center, float Radius, float Lifetime)
{
	if (World == nullptr)
	{
		return nullptr;
	}

	FActorSpawnParameters Parameters;
	Parameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AVaelWeatherRain* Rain = World->SpawnActor<AVaelWeatherRain>(AVaelWeatherRain::StaticClass(), FTransform(Center), Parameters);
	if (Rain != nullptr)
	{
		Rain->bPatch = true;
		Rain->PatchCenter = Center;
		Rain->PatchRadius = Radius;
		Rain->PatchLifetime = Lifetime;
		Rain->Build();
	}

	return Rain;
}

float AVaelWeatherRain::GetStrength() const
{
	if (!bPatch)
	{
		return Region.IsValid() ? Region->GetRainStrength() : 0.0f;
	}

	// A called rain pours at once and stops in its last second
	return FMath::Min(FMath::Clamp(PatchAge / 0.6f, 0.0f, 1.0f), FMath::Clamp(PatchLifetime - PatchAge, 0.0f, 1.0f));
}

FVector AVaelWeatherRain::GetWind() const
{
	if (!bPatch)
	{
		return Region.IsValid() ? Region->GetWind() : FVector::ZeroVector;
	}
	return AVaelRegion::GetWindAt(GetWorld(), PatchCenter);
}

float AVaelWeatherRain::GetReach() const
{
	return bPatch ? PatchRadius : UVaelWorldSettings::Get()->RainReach;
}

float AVaelWeatherRain::GetHeight() const
{
	const UVaelWorldSettings* Settings = UVaelWorldSettings::Get();
	return bPatch ? Settings->RainCallHeight : Settings->RainHeight;
}

void AVaelWeatherRain::Build()
{
	const UVaelMagicSettings* MagicSettings = UVaelMagicSettings::Get();
	const UVaelWorldSettings* Settings = UVaelWorldSettings::Get();

	// The rain is the water of the wave and the strand: pale, thin, clear, with no foam and no ragged edges
	UMaterialInterface* WaveMaterial = VaelAssets::LoadOptional(MagicSettings->WaveMaterial);
	if (WaveMaterial != nullptr)
	{
		WaterMaterial = UMaterialInstanceDynamic::Create(WaveMaterial, this);
		WaterMaterial->SetVectorParameterValue(TEXT("DeepColor"), Settings->RainDeepColor);
		WaterMaterial->SetVectorParameterValue(TEXT("MidColor"), FMath::Lerp(Settings->RainDeepColor, Settings->RainCrestColor, 0.5f));
		WaterMaterial->SetVectorParameterValue(TEXT("CrestColor"), Settings->RainCrestColor);
		WaterMaterial->SetScalarParameterValue(TEXT("BodyOpacity"), Settings->RainOpacity);
		WaterMaterial->SetScalarParameterValue(TEXT("EdgeRagged"), 0.0f);
		WaterMaterial->SetScalarParameterValue(TEXT("EmissiveStrength"), Settings->RainGlow);
		WaterMaterial->SetScalarParameterValue(TEXT("NormalStrength"), 0.3f);
		WaterMaterial->SetScalarParameterValue(TEXT("ContactFade"), 1.0f);

		Water = NewObject<UProceduralMeshComponent>(this);
		Water->SetupAttachment(RootComponent);
		Water->SetUsingAbsoluteLocation(true);
		Water->SetUsingAbsoluteRotation(true);
		Water->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Water->SetCastShadow(false);
		Water->bUseAsyncCooking = true;
		Water->RegisterComponent();
		Water->SetWorldTransform(FTransform::Identity);
	}

	// A called rain falls out of a dark cloud, like the one of the Hagelsturm
	UMaterialInterface* StormCloud = bPatch ? VaelAssets::LoadOptional(MagicSettings->StormCloudMaterial) : nullptr;
	if (StormCloud != nullptr)
	{
		Cloud = NewObject<UVaelCloudCardsComponent>(this);
		Cloud->SetupAttachment(RootComponent);
		Cloud->SetUsingAbsoluteLocation(true);
		Cloud->SetUsingAbsoluteRotation(true);
		Cloud->RegisterComponent();
		Cloud->SetWorldTransform(FTransform::Identity);
		Cloud->Init(StormCloud);
		for (int32 Index = 0; Index < Settings->RainCallCloudCards; ++Index)
		{
			const float Angle = FMath::FRandRange(0.0f, UE_TWO_PI);
			const float Out = PatchRadius * FMath::Sqrt(FMath::FRand()) * 0.8f;
			const FVector Place = PatchCenter + FVector(FMath::Cos(Angle) * Out, FMath::Sin(Angle) * Out, Settings->RainCallHeight + FMath::FRandRange(0.0f, 140.0f));
			const float Size = PatchRadius * FMath::FRandRange(0.5f, 0.9f);
			Cloud->AddCard(Place, Size, 0.0f);
			CloudPlaces.Add(Place);
			CloudSizes.Add(Size);
		}
	}

	UMaterialInterface* CloudMaterial = !bPatch ? VaelAssets::LoadOptional(MagicSettings->CloudCardMaterial) : nullptr;
	if (CloudMaterial != nullptr)
	{
		Veils = NewObject<UVaelCloudCardsComponent>(this);
		Veils->SetupAttachment(RootComponent);
		Veils->SetUsingAbsoluteLocation(true);
		Veils->SetUsingAbsoluteRotation(true);
		Veils->RegisterComponent();
		Veils->SetWorldTransform(FTransform::Identity);
		Veils->Init(CloudMaterial);
	}

	// Every drop starts somewhere on its way down, so the rain doesn't begin in one sheet
	// A called rain is far denser than the weather: its drops by the area of the patch
	FVector Middle;
	FindMiddle(Middle);
	const int32 NumPatchDrops = FMath::Clamp(FMath::RoundToInt(UE_PI * FMath::Square(PatchRadius / 100.0f) * Settings->RainCallDropsPerSquareMeter), 40, 1500);
	Drops.SetNum(bPatch ? NumPatchDrops : FMath::Max(Settings->RainDrops, Settings->StormRainDrops));
	Splashes.SetNum(RainMaxSplashes);
	for (FSplash& Splash : Splashes)
	{
		Splash.Age = Splash.Life;
	}
	Rings.SetNum(RainMaxRings);
	for (FRing& Ring : Rings)
	{
		Ring.Age = Ring.Life;
	}

	for (int32 Index = 0; Index < Settings->RainVeils && Veils != nullptr; ++Index)
	{
		FVeil& Veil = VeilParts.AddDefaulted_GetRef();
		Veil.Age = FMath::FRandRange(0.0f, Veil.Life);
		Veil.Place = Middle + FVector(FMath::FRandRange(-1.0f, 1.0f) * Settings->RainReach, FMath::FRandRange(-1.0f, 1.0f) * Settings->RainReach, FMath::FRandRange(150.0f, 450.0f));
		Veil.Size = FMath::FRandRange(500.0f, 900.0f);
		Veils->AddCard(Veil.Place, Veil.Size, 0.0f);
	}

	// One quad per drop and splash, a strip of quads per ring; the triangles never change, only the corners move
	if (Water != nullptr)
	{
		const int32 NumQuads = Drops.Num() + Splashes.Num() + Rings.Num() * RainRingSegments;
		Vertices.SetNumZeroed(NumQuads * 4);
		Normals.Init(FVector::UpVector, NumQuads * 4);
		UVs.SetNumZeroed(NumQuads * 4);
		Colors.Init(FLinearColor::Transparent, NumQuads * 4);
		TArray<int32> Triangles;
		Triangles.Reserve(NumQuads * 6);
		for (int32 Quad = 0; Quad < NumQuads; ++Quad)
		{
			const int32 First = Quad * 4;
			Triangles.Append({ First, First + 1, First + 2, First, First + 2, First + 3 });
		}
		Water->CreateMeshSection_LinearColor(0, Vertices, Triangles, Normals, UVs, Colors, TArray<FProcMeshTangent>(), false);
		Water->SetMaterial(0, WaterMaterial);
	}
}

bool AVaelWeatherRain::FindMiddle(FVector& OutMiddle) const
{
	if (bPatch)
	{
		OutMiddle = PatchCenter;
		return true;
	}

	FVector Sum = FVector::ZeroVector;
	int32 Count = 0;
	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		const APlayerController* PlayerController = It->Get();
		const APawn* Pawn = PlayerController != nullptr ? PlayerController->GetPawn() : nullptr;
		if (Pawn != nullptr && (!Region.IsValid() || Region->Contains(Pawn->GetActorLocation())))
		{
			Sum += Pawn->GetActorLocation();
			++Count;
		}
	}

	if (Count == 0)
	{
		return false;
	}

	OutMiddle = Sum / Count;
	return true;
}

void AVaelWeatherRain::Restart(FDrop& Drop, const FVector& Middle, const FVector& Fall, bool bAnyHeight)
{
	// Anywhere around the players, shifted against the wind so the slanted rain still covers the picture
	// A called rain lands only on its round patch
	const UVaelWorldSettings* Settings = UVaelWorldSettings::Get();
	const float Reach = GetReach();
	const float PatchAngle = FMath::FRandRange(0.0f, UE_TWO_PI);
	const float PatchOut = Reach * FMath::Sqrt(FMath::FRand());
	const FVector Land = bPatch ? Middle + FVector(FMath::Cos(PatchAngle) * PatchOut, FMath::Sin(PatchAngle) * PatchOut, 0.0f)
		: Middle + FVector(FMath::FRandRange(-Reach, Reach), FMath::FRandRange(-Reach, Reach), 0.0f);

	FHitResult GroundHit;
	const float SearchUp = bPatch ? 400.0f : 1500.0f;
	Drop.GroundZ = VaelGround::TraceGround(GetWorld(), Land + FVector(0.0f, 0.0f, SearchUp), Land - FVector(0.0f, 0.0f, 3000.0f), GroundHit) ? GroundHit.Location.Z : Middle.Z - (bPatch ? 0.0f : 90.0f);

	const float RainHeight = GetHeight();
	const float Height = bAnyHeight ? FMath::FRandRange(0.0f, RainHeight) : RainHeight * FMath::FRandRange(0.85f, 1.0f);
	const float Time = Height / FMath::Max(-Fall.Z, 1.0f);
	Drop.Head = FVector(Land.X, Land.Y, Drop.GroundZ) - FVector(Fall.X, Fall.Y, 0.0f) * Time + FVector(0.0f, 0.0f, Height);
	Drop.Speed = FMath::FRandRange(0.85f, 1.15f);
	Drop.Length = Settings->RainStreakLength * FMath::FRandRange(0.7f, 1.2f);

	// A drop falling into a puddle splashes into it with a big ring
	Drop.bOnPuddle = false;
	for (TActorIterator<AVaelWaterPuddle> It(GetWorld()); It && !Drop.bOnPuddle; ++It)
	{
		Drop.bOnPuddle = It->Covers(Land);
	}
}

void AVaelWeatherRain::Land(const FDrop& Drop)
{
	const UVaelWorldSettings* Settings = UVaelWorldSettings::Get();
	const FVector Ground(Drop.Head.X, Drop.Head.Y, Drop.GroundZ + 1.0f);

	// A few drops jump up and fall back
	for (int32 Index = 0; Index < Settings->RainSplashDrops; ++Index)
	{
		FSplash& Splash = Splashes[NextSplash];
		NextSplash = (NextSplash + 1) % Splashes.Num();
		const float Angle = FMath::FRandRange(0.0f, UE_TWO_PI);
		const float Out = FMath::FRandRange(40.0f, 110.0f) * (Drop.bOnPuddle ? 1.3f : 1.0f);
		Splash.Location = Ground;
		Splash.Velocity = FVector(FMath::Cos(Angle) * Out, FMath::Sin(Angle) * Out, FMath::FRandRange(120.0f, 220.0f));
		Splash.Age = 0.0f;
		Splash.Life = FMath::FRandRange(0.22f, 0.38f);
		Splash.Size = FMath::FRandRange(2.0f, 4.0f);
	}

	// Rings spread where it falls into a puddle, now and then on the wet ground
	if (Drop.bOnPuddle || FMath::FRand() < Settings->RainRingShare)
	{
		FRing& Ring = Rings[NextRing];
		NextRing = (NextRing + 1) % Rings.Num();
		Ring.Center = Ground;
		Ring.Age = 0.0f;
		Ring.Life = Drop.bOnPuddle ? FMath::FRandRange(0.6f, 0.9f) : FMath::FRandRange(0.3f, 0.45f);
		Ring.Radius = (Drop.bOnPuddle ? RainPuddleRingRadius : RainGroundRingRadius) * FMath::FRandRange(0.7f, 1.3f);
	}
}

void AVaelWeatherRain::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	// A called rain is gone once it has stopped and its last drops have landed
	if (bPatch)
	{
		PatchAge += DeltaSeconds;
		if (PatchAge > PatchLifetime + 1.0f)
		{
			Destroy();
			return;
		}
	}

	FVector Middle;
	if ((!bPatch && !Region.IsValid()) || !FindMiddle(Middle))
	{
		return;
	}

	const UVaelWorldSettings* Settings = UVaelWorldSettings::Get();
	const FVector Wind = GetWind();
	const FVector Fall = FVector(0.0f, 0.0f, -Settings->RainFallSpeed) + Wind * Settings->RainWindShare;

	// How much of the rain falls: rising and dying down with the weather, the storm with more drops; a called rain all of its drops
	const float Strength = GetStrength();
	const int32 NumFalling = bPatch ? FMath::RoundToInt(Drops.Num() * Strength)
		: FMath::Min(Drops.Num(), FMath::RoundToInt(FMath::Lerp(0.0f, static_cast<float>(Settings->RainDrops), FMath::Min(Strength, 1.0f))
			+ FMath::Max(Strength - 1.0f, 0.0f) / 0.4f * (Settings->StormRainDrops - Settings->RainDrops)));

	const float Reach = GetReach();
	for (int32 Index = 0; Index < Drops.Num(); ++Index)
	{
		FDrop& Drop = Drops[Index];
		const bool bNeeded = Index < NumFalling;
		if (!bNeeded && (Drop.bWaiting || !bPatch))
		{
			// Waiting until the rain grows; the drops of a called rain that stops still fall to the ground first
			Drop.bWaiting = true;
			continue;
		}
		if (Drop.bWaiting)
		{
			Drop.bWaiting = false;
			// A called rain starts high up out of its cloud, the weather anywhere on the way down
			Restart(Drop, Middle, Fall, !bPatch);
		}

		Drop.Head += Fall * Drop.Speed * DeltaSeconds;
		if (Drop.Head.Z <= Drop.GroundZ)
		{
			Land(Drop);
			if (bNeeded)
			{
				Restart(Drop, Middle, Fall, false);
			}
			else
			{
				Drop.bWaiting = true;
			}
		}
		// Players walked off: the rain follows them
		else if (!bPatch && (FMath::Abs(Drop.Head.X - Middle.X) > Reach * 1.3f || FMath::Abs(Drop.Head.Y - Middle.Y) > Reach * 1.3f))
		{
			Restart(Drop, Middle, Fall, true);
		}
	}

	for (FSplash& Splash : Splashes)
	{
		if (Splash.Age < Splash.Life)
		{
			Splash.Age += DeltaSeconds;
			Splash.Velocity.Z -= 980.0f * DeltaSeconds;
			Splash.Location += Splash.Velocity * DeltaSeconds;
		}
	}

	for (FRing& Ring : Rings)
	{
		Ring.Age += DeltaSeconds;
	}

	// Now and then a crown splash of the water kit close to the players, or all over a called rain
	const float KitRate = bPatch ? Settings->RainCallKitSplashesPerSecond : Settings->RainKitSplashesPerSecond;
	UNiagaraSystem* KitSplash = KitRate > 0.0f ? VaelAssets::LoadOptional(UVaelMagicSettings::Get()->WaterOrbImpactEffect) : nullptr;
	KitSplashCredit += KitRate * FMath::Min(Strength, 1.0f) * DeltaSeconds;
	while (KitSplash != nullptr && KitSplashCredit >= 1.0f)
	{
		KitSplashCredit -= 1.0f;
		const float SpotAngle = FMath::FRandRange(0.0f, UE_TWO_PI);
		const float SpotOut = Reach * FMath::Sqrt(FMath::FRand());
		const FVector Spot = bPatch ? Middle + FVector(FMath::Cos(SpotAngle) * SpotOut, FMath::Sin(SpotAngle) * SpotOut, 0.0f)
			: Middle + FVector(FMath::FRandRange(-Reach, Reach) * 0.5f, FMath::FRandRange(-Reach, Reach) * 0.5f, 0.0f);
		FHitResult GroundHit;
		if (!VaelGround::TraceGround(GetWorld(), Spot + FVector(0.0f, 0.0f, 600.0f), Spot - FVector(0.0f, 0.0f, 1200.0f), GroundHit))
		{
			continue;
		}
		UNiagaraComponent* Burst = UNiagaraFunctionLibrary::SpawnSystemAtLocation(this, KitSplash, GroundHit.Location, FRotator(-90.0f, 0.0f, 0.0f),
			FVector(Settings->RainKitSplashScale), true, false);
		if (Burst != nullptr)
		{
			if (const UVaelElementSource* RainSource = VaelAssets::LoadOptional(UVaelMagicSettings::Get()->RainWaterSource))
			{
				Burst->SetVariableLinearColor(AVaelWaterOrb::WaterColorParameter, RainSource->ParticleColor);
			}
			Burst->SetVariableFloat(AVaelWaterOrb::WaterAmountParameter, 0.1f);
			Burst->Activate(true);
		}
	}

	// The cloud of a called rain gathers, churns slowly and thins away at the end
	if (Cloud != nullptr)
	{
		const float Shown = FMath::Min(FMath::Clamp(PatchAge / 0.5f, 0.0f, 1.0f), FMath::Clamp((PatchLifetime + 0.8f - PatchAge) / 0.8f, 0.0f, 1.0f));
		const float Time = GetWorld()->GetTimeSeconds();
		for (int32 Index = 0; Index < CloudPlaces.Num(); ++Index)
		{
			const FVector Churn(FMath::Sin(Time * 0.7f + Index) * 25.0f, FMath::Cos(Time * 0.6f + Index * 1.7f) * 25.0f, FMath::Sin(Time * 0.9f + Index * 0.5f) * 12.0f);
			const FVector Gather = PatchCenter + (CloudPlaces[Index] - PatchCenter) * FVector(FMath::Lerp(0.6f, 1.0f, Shown), FMath::Lerp(0.6f, 1.0f, Shown), 1.0f);
			Cloud->SetCard(Index, Gather + Churn + Wind * 0.3f, CloudSizes[Index], 0.8f * Shown);
		}
		Cloud->Commit();
	}

	// Pale veils drift through the rain with the wind
	if (Veils != nullptr)
	{
		for (int32 Index = 0; Index < VeilParts.Num(); ++Index)
		{
			FVeil& Veil = VeilParts[Index];
			Veil.Age += DeltaSeconds;
			Veil.Place += (Wind * 0.5f + FVector(60.0f, 25.0f, 0.0f)) * DeltaSeconds;
			if (Veil.Age >= Veil.Life || FVector::Dist2D(Veil.Place, Middle) > Reach * 1.5f)
			{
				Veil.Age = 0.0f;
				Veil.Place = Middle + FVector(FMath::FRandRange(-1.0f, 1.0f) * Reach, FMath::FRandRange(-1.0f, 1.0f) * Reach, FMath::FRandRange(150.0f, 450.0f));
			}
			const float Life = Veil.Age / Veil.Life;
			Veils->SetCard(Index, Veil.Place, Veil.Size, Settings->RainVeilSolid * FMath::Sin(Life * UE_PI) * FMath::Min(Strength, 1.0f));
		}
		Veils->Commit();
	}

	BuildMesh(Fall, FMath::Min(Strength, 1.0f));
}

void AVaelWeatherRain::BuildMesh(const FVector& Fall, float Shown)
{
	if (Water == nullptr)
	{
		return;
	}

	const APlayerController* PlayerController = GetWorld()->GetFirstPlayerController();
	const FVector Camera = PlayerController != nullptr && PlayerController->PlayerCameraManager != nullptr ? PlayerController->PlayerCameraManager->GetCameraLocation() : FVector(0.0f, 0.0f, 3000.0f);
	const FVector FallDirection = Fall.GetSafeNormal();
	const float HalfWidth = UVaelWorldSettings::Get()->RainStreakWidth * 0.5f;

	int32 Quad = 0;
	const auto SetQuad = [this, &Quad](const FVector& A, const FVector& B, const FVector& C, const FVector& D, const FLinearColor& Near, const FLinearColor& Far, const FVector& Normal, float Length)
	{
		const int32 First = Quad * 4;
		Vertices[First] = A;
		Vertices[First + 1] = B;
		Vertices[First + 2] = C;
		Vertices[First + 3] = D;
		UVs[First] = FVector2D(0.0f, 0.0f);
		UVs[First + 1] = FVector2D(1.0f, 0.0f);
		UVs[First + 2] = FVector2D(1.0f, Length);
		UVs[First + 3] = FVector2D(0.0f, Length);
		Colors[First] = Near;
		Colors[First + 1] = Near;
		Colors[First + 2] = Far;
		Colors[First + 3] = Far;
		for (int32 Corner = 0; Corner < 4; ++Corner)
		{
			Normals[First + Corner] = Normal;
		}
		++Quad;
	};
	const auto HideQuad = [this, &Quad]()
	{
		const int32 First = Quad * 4;
		for (int32 Corner = 0; Corner < 4; ++Corner)
		{
			Vertices[First + Corner] = FVector::ZeroVector;
			Colors[First + Corner] = FLinearColor::Transparent;
		}
		++Quad;
	};

	// Falling drops: a thin streak along the fall, turned to the camera, solid at the drop and fading up its trail
	for (const FDrop& Drop : Drops)
	{
		const float Above = Drop.Head.Z - Drop.GroundZ;
		if (Above <= 0.0f || Drop.bWaiting)
		{
			HideQuad();
			continue;
		}
		const FVector Tail = Drop.Head - FallDirection * FMath::Min(Drop.Length, Above + Drop.Length * 0.3f);
		const FVector ToCamera = (Camera - Drop.Head).GetSafeNormal();
		const FVector Side = FVector::CrossProduct(FallDirection, ToCamera).GetSafeNormal() * HalfWidth;
		SetQuad(Drop.Head - Side, Drop.Head + Side, Tail + Side, Tail - Side, RainVertex(1.0f, 0.15f, false, 0.2f * Shown), RainVertex(0.7f, 0.0f, false, 0.0f), ToCamera, Drop.Length / 100.0f);
	}

	// Drops thrown up by the splashes: round dots facing the camera
	for (const FSplash& Splash : Splashes)
	{
		if (Splash.Age >= Splash.Life)
		{
			HideQuad();
			continue;
		}
		const FVector ToCamera = (Camera - Splash.Location).GetSafeNormal();
		const FVector Right = FVector::CrossProduct(FVector::UpVector, ToCamera).GetSafeNormal() * Splash.Size;
		const FVector Up = FVector::CrossProduct(ToCamera, Right).GetSafeNormal() * Splash.Size;
		const FLinearColor Dot = RainVertex(1.0f, 0.3f, true, 0.9f * (1.0f - Splash.Age / Splash.Life));
		SetQuad(Splash.Location - Right - Up, Splash.Location + Right - Up, Splash.Location + Right + Up, Splash.Location - Right + Up, Dot, Dot, ToCamera, 1.0f);
	}

	// Rings spreading flat on the ground, thinner and fainter as they grow
	for (const FRing& Ring : Rings)
	{
		if (Ring.Age >= Ring.Life)
		{
			for (int32 Segment = 0; Segment < RainRingSegments; ++Segment)
			{
				HideQuad();
			}
			continue;
		}
		const float Life = Ring.Age / Ring.Life;
		const float Radius = Ring.Radius * (0.2f + 0.8f * FMath::Sqrt(Life));
		const float Width = FMath::Lerp(3.0f, 0.8f, Life);
		const FLinearColor Solid = RainVertex(1.0f, 0.4f, false, 0.2f * (1.0f - Life));
		for (int32 Segment = 0; Segment < RainRingSegments; ++Segment)
		{
			const float A = Segment * UE_TWO_PI / RainRingSegments;
			const float B = (Segment + 1) * UE_TWO_PI / RainRingSegments;
			const FVector OutA(FMath::Cos(A), FMath::Sin(A), 0.0f);
			const FVector OutB(FMath::Cos(B), FMath::Sin(B), 0.0f);
			SetQuad(Ring.Center + OutA * (Radius - Width), Ring.Center + OutB * (Radius - Width), Ring.Center + OutB * (Radius + Width), Ring.Center + OutA * (Radius + Width),
				Solid, Solid, FVector::UpVector, 0.1f);
		}
	}

	Water->UpdateMeshSection_LinearColor(0, Vertices, Normals, UVs, Colors, TArray<FProcMeshTangent>());
}
