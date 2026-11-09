// Copyright Epic Games, Inc. All Rights Reserved.

#include "World/VaelFootprints.h"
#include "Misc/App.h"
#include "Creatures/VaelCreature.h"
#include "Creatures/VaelAshRoller.h"
#include "Creatures/VaelAshHarpy.h"
#include "Engine/Canvas.h"
#include "Engine/Texture2D.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/KismetMaterialLibrary.h"
#include "Kismet/KismetRenderingLibrary.h"
#include "Materials/MaterialParameterCollection.h"
#include "Materials/MaterialInterface.h"
#include "Components/PrimitiveComponent.h"
#include "HAL/IConsoleManager.h"
#include "Player/VaelCharacter.h"
#include "Vael.h"

namespace
{
	const TCHAR* SparePath = TEXT("/Game/Vael/Welt/Boden/RT_AM_Spuren_Umzug.RT_AM_Spuren_Umzug");
	const TCHAR* ShapesPath = TEXT("/Game/Vael/Welt/Boden/T_AM_Fussabdruecke.T_AM_Fussabdruecke");
	const TCHAR* ParametersPath = TEXT("/Game/Vael/Welt/Boden/MPC_AM_Spuren.MPC_AM_Spuren");

	TAutoConsoleVariable<int32> CVarSpurLog(TEXT("r.Vael.SpurLog"), 0, TEXT("1: the footprint protocol every 15 s in the log"));

	/** The clock written into the map: seconds / 600, wrapped to 0..1 */
	float ClockValue(const UWorld* World)
	{
		return FMath::Frac(static_cast<float>(World->GetTimeSeconds()) / 600.0f);
	}
}

bool UVaelFootprintSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	const UWorld* World = Cast<UWorld>(Outer);
	return World != nullptr && World->IsGameWorld();
}

bool UVaelFootprintSubsystem::EnsureResources()
{
	if (Maps[0] != nullptr)
	{
		return true;
	}
	// No renderer (dedicated server, -nullrhi stream test): nothing to draw into
	if (bTriedResources || !FApp::CanEverRender())
	{
		return false;
	}
	bTriedResources = true;
	bool bAllMaps = true;
	for (int32 Slot = 0; Slot < NumMaps; ++Slot)
	{
		Maps[Slot] = LoadObject<UTextureRenderTarget2D>(nullptr, *FString::Printf(TEXT("/Game/Vael/Welt/Boden/RT_AM_Spuren_%d.RT_AM_Spuren_%d"), Slot, Slot), nullptr, LOAD_NoWarn | LOAD_Quiet);
		bAllMaps &= Maps[Slot] != nullptr;
	}
	Spare = LoadObject<UTextureRenderTarget2D>(nullptr, SparePath, nullptr, LOAD_NoWarn | LOAD_Quiet);
	Shapes = LoadObject<UTexture2D>(nullptr, ShapesPath, nullptr, LOAD_NoWarn | LOAD_Quiet);
	Parameters = LoadObject<UMaterialParameterCollection>(nullptr, ParametersPath, nullptr, LOAD_NoWarn | LOAD_Quiet);
	if (!bAllMaps || Spare == nullptr || Shapes == nullptr || Parameters == nullptr)
	{
		UE_LOG(LogVael, Warning, TEXT("Footprints: resources missing (maps %d, spare %d, shapes %d, parameters %d)"), bAllMaps, Spare != nullptr, Shapes != nullptr, Parameters != nullptr);
		for (int32 Slot = 0; Slot < NumMaps; ++Slot)
		{
			Maps[Slot] = nullptr;
		}
		return false;
	}
	UE_LOG(LogVael, Display, TEXT("Footprints: %d maps %dx%d ready"), NumMaps, Maps[0]->SizeX, Maps[0]->SizeY);
	for (int32 Slot = 0; Slot < NumMaps; ++Slot)
	{
		UKismetRenderingLibrary::ClearRenderTarget2D(this, Maps[Slot], FLinearColor::Transparent);
		Park(Slot);
	}
	UKismetRenderingLibrary::ClearRenderTarget2D(this, Spare, FLinearColor::Transparent);
	return true;
}

void UVaelFootprintSubsystem::Park(int32 Slot)
{
	if (bParked[Slot])
	{
		return;
	}
	bParked[Slot] = true;
	bPlaced[Slot] = false;
	UKismetRenderingLibrary::ClearRenderTarget2D(this, Maps[Slot], FLinearColor::Transparent);
	UKismetMaterialLibrary::SetVectorParameterValue(this, Parameters, *FString::Printf(TEXT("SpurenUrsprung%d"), Slot),
		FLinearColor(-1.0e7f, -1.0e7f, MapSize, 0.0f));
}

void UVaelFootprintSubsystem::Recentre(int32 Slot, const FVector2D& Centre)
{
	UTextureRenderTarget2D* Map = Maps[Slot];
	const FVector2D NewOrigin = Centre - FVector2D(MapSize * 0.5f);
	if (bPlaced[Slot])
	{
		const FVector2D Origin = Origins[Slot];
		// The old map drawn shifted into the spare one, then back: prints stay where they are in the world. Whole pixels
		// only, so moving does not blur the prints
		const float Pixels = Map->SizeX;
		const FVector2D PixelShift = (Origin - NewOrigin) / MapSize * Pixels;
		const FVector2D Shift(FMath::RoundToDouble(PixelShift.X), FMath::RoundToDouble(PixelShift.Y));
		UKismetRenderingLibrary::ClearRenderTarget2D(this, Spare, FLinearColor::Transparent);
		UCanvas* Canvas = nullptr;
		FVector2D CanvasSize;
		FDrawToRenderTargetContext Context;
		UKismetRenderingLibrary::BeginDrawCanvasToRenderTarget(this, Spare, Canvas, CanvasSize, Context);
		if (Canvas == nullptr)
		{
			return;
		}
		Canvas->K2_DrawTexture(Map, Shift, FVector2D(Pixels), FVector2D::ZeroVector, FVector2D::UnitVector, FLinearColor::White, BLEND_Opaque);
		UKismetRenderingLibrary::EndDrawCanvasToRenderTarget(this, Context);
		UKismetRenderingLibrary::BeginDrawCanvasToRenderTarget(this, Map, Canvas, CanvasSize, Context);
		Canvas->K2_DrawTexture(Spare, FVector2D::ZeroVector, FVector2D(Pixels), FVector2D::ZeroVector, FVector2D::UnitVector, FLinearColor::White, BLEND_Opaque);
		UKismetRenderingLibrary::EndDrawCanvasToRenderTarget(this, Context);
	}
	// The origin moves by the same whole pixels the prints moved
	if (bPlaced[Slot])
	{
		const float Pixels = Map->SizeX;
		const FVector2D PixelShift = (Origins[Slot] - NewOrigin) / MapSize * Pixels;
		const FVector2D Shift(FMath::RoundToDouble(PixelShift.X), FMath::RoundToDouble(PixelShift.Y));
		Origins[Slot] = Origins[Slot] - Shift / Pixels * MapSize;
	}
	else
	{
		UKismetRenderingLibrary::ClearRenderTarget2D(this, Map, FLinearColor::Transparent);
		Origins[Slot] = NewOrigin;
	}
	bPlaced[Slot] = true;
	bParked[Slot] = false;
	++RecentreCount;
	UKismetMaterialLibrary::SetVectorParameterValue(this, Parameters, *FString::Printf(TEXT("SpurenUrsprung%d"), Slot),
		FLinearColor(Origins[Slot].X, Origins[Slot].Y, MapSize, 0.0f));
}

void UVaelFootprintSubsystem::Stamp(const FVector& Location, float Yaw, EVaelPrint Print, float Size, float Depth)
{
	if (!EnsureResources())
	{
		return;
	}
	// Into every map that covers the place (maps of players walking together overlap)
	bool bAny = false;
	for (int32 Slot = 0; Slot < NumMaps; ++Slot)
	{
		UTextureRenderTarget2D* Map = Maps[Slot];
		if (!bPlaced[Slot])
		{
			continue;
		}
		const float Pixels = Map->SizeX;
		// Map pixels: x along world X, y along world Y, so the material reads it as (world.xy - origin) / size
		const FVector2D Local = (FVector2D(Location.X, Location.Y) - Origins[Slot]) / MapSize * Pixels;
		if (Local.X < 0.0f || Local.Y < 0.0f || Local.X > Pixels || Local.Y > Pixels)
		{
			continue;
		}
		const float SizePixels = Size / MapSize * Pixels;
		UCanvas* Canvas = nullptr;
		FVector2D CanvasSize;
		FDrawToRenderTargetContext Context;
		UKismetRenderingLibrary::BeginDrawCanvasToRenderTarget(this, Map, Canvas, CanvasSize, Context);
		if (Canvas == nullptr)
		{
			continue;
		}
		const int32 Cell = static_cast<int32>(Print);
		// Colour = (depth, clock); the shape's alpha blends it over what is there
		Canvas->K2_DrawTexture(Shapes, FVector2D(Local.X, Local.Y) - FVector2D(SizePixels * 0.5f), FVector2D(SizePixels),
			FVector2D(Cell * 0.25f, 0.0f), FVector2D(0.25f, 1.0f), FLinearColor(Depth, ClockValue(GetWorld()), 0.0f, 1.0f), BLEND_Translucent,
			Yaw, FVector2D(0.5f, 0.5f));
		UKismetRenderingLibrary::EndDrawCanvasToRenderTarget(this, Context);
		bAny = true;
	}
	if (!bAny)
	{
		++SkippedOutside;
		return;
	}
	++StampCount;
	LastStampWorld = Location;
}

void UVaelFootprintSubsystem::FollowWalker(AActor* Walker, EVaelPrint Print, float Stride, float Size, float Side, float Depth)
{
	FWalker& State = Walkers.FindOrAdd(Walker);
	const FVector Here = Walker->GetActorLocation();
	const FVector2D Flat(Here.X, Here.Y);
	if (!State.bStarted)
	{
		State.Last = Flat;
		State.bStarted = true;
		return;
	}
	const FVector2D Moved = Flat - State.Last;
	if (Moved.Size() < Stride)
	{
		return;
	}
	const FVector2D Ahead = Moved.GetSafeNormal();
	const float Yaw = FMath::RadiansToDegrees(FMath::Atan2(Ahead.Y, Ahead.X));
	State.bLeft = !State.bLeft;
	const FVector2D Right(-Ahead.Y, Ahead.X);
	const FVector2D Foot = Flat + Right * (State.bLeft ? -Side : Side);
	FVector Ground;
	if (AVaelCreature::FindGround(GetWorld(), FVector(Foot.X, Foot.Y, Here.Z + 50.0f), Ground) && Here.Z - Ground.Z < 400.0f)
	{
		// The atlas cells point their toes up (v down = forward): turn so the toes lead the way
		Stamp(Ground, Yaw + 90.0f, Print, Size, Depth);
	}
	else
	{
		++SkippedNoGround;
	}
	State.Last = Flat;
}

void UVaelFootprintSubsystem::Tick(float DeltaTime)
{
	UWorld* World = GetWorld();
	if (World == nullptr || !World->IsPartitionedWorld() || !EnsureResources())
	{
		return;
	}
	UKismetMaterialLibrary::SetScalarParameterValue(this, Parameters, TEXT("SpurenUhr"), ClockValue(World));

	// One map per player, following it; the maps of missing players are parked
	TArray<AVaelCharacter*> Players;
	for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
	{
		if (AVaelCharacter* Player = It->Get() != nullptr ? It->Get()->GetPawn<AVaelCharacter>() : nullptr)
		{
			Players.Add(Player);
		}
	}
	if (Players.IsEmpty())
	{
		return;
	}
	FVector2D Middle = FVector2D::ZeroVector;
	for (int32 Slot = 0; Slot < NumMaps; ++Slot)
	{
		if (!Players.IsValidIndex(Slot))
		{
			Park(Slot);
			continue;
		}
		const FVector2D At(Players[Slot]->GetActorLocation().X, Players[Slot]->GetActorLocation().Y);
		Middle += At / FMath::Min(Players.Num(), NumMaps);
		if (!bPlaced[Slot] || FVector2D::Distance(At, Origins[Slot] + FVector2D(MapSize * 0.5f)) > RecentreDistance)
		{
			Recentre(Slot, At);
		}
	}

	LogClock += DeltaTime;
	if (LogClock >= 15.0f && (CVarSpurLog.GetValueOnGameThread() > 0 || FParse::Param(FCommandLine::Get(), TEXT("VaelSpurLog"))))
	{
		LogClock = 0.0f;
		WriteProtocol(Players[0]->GetActorLocation());
	}

	for (AVaelCharacter* Player : Players)
	{
		// No prints while flying (the Floromant up high) or falling
		if (!Player->IsDowned() && !Player->IsAboveGround())
		{
			FollowWalker(Player, EVaelPrint::Boot, 68.0f, 34.0f, 11.0f, 1.0f);
		}
	}

	// The creatures near the group, looked up twice a second
	CreatureScan -= DeltaTime;
	if (CreatureScan <= 0.0f)
	{
		CreatureScan = 0.5f;
		NearCreatures.Reset();
		for (TActorIterator<AVaelCreature> It(World); It; ++It)
		{
			const FVector2D At(It->GetActorLocation().X, It->GetActorLocation().Y);
			bool bNear = false;
			for (AVaelCharacter* Player : Players)
			{
				bNear |= FVector2D::Distance(At, FVector2D(Player->GetActorLocation().X, Player->GetActorLocation().Y)) < MapSize * 0.5f;
			}
			if (!It->IsDead() && bNear)
			{
				NearCreatures.Add(*It);
			}
		}
	}
	for (const TWeakObjectPtr<AActor>& Weak : NearCreatures)
	{
		AVaelCreature* Creature = Cast<AVaelCreature>(Weak.Get());
		if (Creature == nullptr || Creature->IsDead())
		{
			continue;
		}
		const float Radius = Creature->GetSimpleCollisionRadius();
		if (Creature->IsA<AVaelAshRoller>())
		{
			FollowWalker(Creature, EVaelPrint::Roll, Radius * 0.9f, Radius * 1.8f, 0.0f, 0.8f);
		}
		else if (!Creature->IsA<AVaelAshHarpy>())
		{
			FollowWalker(Creature, Radius > 70.0f ? EVaelPrint::Paw : EVaelPrint::Claw, FMath::Max(Radius * 0.9f, 40.0f),
				FMath::Clamp(Radius * 0.7f, 18.0f, 60.0f), Radius * 0.35f, 0.8f);
		}
	}
	for (auto It = Walkers.CreateIterator(); It; ++It)
	{
		if (!It.Key().IsValid())
		{
			It.RemoveCurrent();
		}
	}
}

FLinearColor UVaelFootprintSubsystem::ReadAt(const FVector& World) const
{
	for (int32 Slot = 0; Slot < NumMaps; ++Slot)
	{
		const FVector2D Local = (FVector2D(World.X, World.Y) - Origins[Slot]) / MapSize;
		if (Maps[Slot] != nullptr && bPlaced[Slot] && Local.X >= 0.0 && Local.Y >= 0.0 && Local.X < 1.0 && Local.Y < 1.0)
		{
			return UKismetRenderingLibrary::ReadRenderTargetRawUV(const_cast<UVaelFootprintSubsystem*>(this), Maps[Slot], Local.X, Local.Y);
		}
	}
	return FLinearColor(-1.0f, -1.0f, 0.0f, 0.0f);
}

void UVaelFootprintSubsystem::WriteProtocol(const FVector& PlayerAt)
{
	// The ground under the player: which material, so a region without the footprint block shows up
	FString GroundName = TEXT("-");
	FHitResult Hit;
	if (GetWorld()->LineTraceSingleByChannel(Hit, PlayerAt, PlayerAt - FVector(0.0f, 0.0f, 400.0f), ECC_Visibility))
	{
		const UPrimitiveComponent* Component = Hit.GetComponent();
		const UMaterialInterface* Material = Component != nullptr ? Component->GetMaterial(0) : nullptr;
		GroundName = FString::Printf(TEXT("%s / %s"), *GetNameSafe(Hit.GetActor()), *GetNameSafe(Material));
	}
	// The print probed last time, read again: did the moves of the map keep its depth and its time?
	FString Probe = TEXT("none");
	if (bProbe)
	{
		const FLinearColor Now = ReadAt(ProbeWorld);
		Probe = FString::Printf(TEXT("then R %.3f G %.4f, now R %.3f G %.4f"), ProbeValue.R, ProbeValue.G, Now.R, Now.G);
	}
	UE_LOG(LogVael, Display, TEXT("[Spuren] t %.0f s, clock %.4f, prints %d, skipped outside %d / no ground %d, moves %d, origin %s, player %s, ground %s, probe %s"),
		GetWorld()->GetTimeSeconds(), ClockValue(GetWorld()), StampCount, SkippedOutside, SkippedNoGround, RecentreCount, *Origins[0].ToString(),
		*PlayerAt.ToCompactString(), *GroundName, *Probe);
	// A new probe: the print pressed last (read right away, before the next move of the map)
	ProbeWorld = LastStampWorld;
	ProbeValue = ReadAt(LastStampWorld);
	bProbe = true;
}

