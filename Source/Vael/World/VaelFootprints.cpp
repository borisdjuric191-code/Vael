// Copyright Epic Games, Inc. All Rights Reserved.

#include "World/VaelFootprints.h"
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
#include "Player/VaelCharacter.h"
#include "Vael.h"

namespace
{
	const TCHAR* MapPath = TEXT("/Game/Vael/Welt/Boden/RT_AM_Spuren.RT_AM_Spuren");
	const TCHAR* SparePath = TEXT("/Game/Vael/Welt/Boden/RT_AM_Spuren_Umzug.RT_AM_Spuren_Umzug");
	const TCHAR* ShapesPath = TEXT("/Game/Vael/Welt/Boden/T_AM_Fussabdruecke.T_AM_Fussabdruecke");
	const TCHAR* ParametersPath = TEXT("/Game/Vael/Welt/Boden/MPC_AM_Spuren.MPC_AM_Spuren");

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
	if (Map != nullptr)
	{
		return true;
	}
	if (bTriedResources)
	{
		return false;
	}
	bTriedResources = true;
	Map = LoadObject<UTextureRenderTarget2D>(nullptr, MapPath, nullptr, LOAD_NoWarn | LOAD_Quiet);
	Spare = LoadObject<UTextureRenderTarget2D>(nullptr, SparePath, nullptr, LOAD_NoWarn | LOAD_Quiet);
	Shapes = LoadObject<UTexture2D>(nullptr, ShapesPath, nullptr, LOAD_NoWarn | LOAD_Quiet);
	Parameters = LoadObject<UMaterialParameterCollection>(nullptr, ParametersPath, nullptr, LOAD_NoWarn | LOAD_Quiet);
	if (Map == nullptr || Spare == nullptr || Shapes == nullptr || Parameters == nullptr)
	{
		UE_LOG(LogVael, Warning, TEXT("Footprints: resources missing (map %d, spare %d, shapes %d, parameters %d)"), Map != nullptr, Spare != nullptr, Shapes != nullptr, Parameters != nullptr);
		Map = nullptr;
		return false;
	}
	UE_LOG(LogVael, Display, TEXT("Footprints: map %dx%d ready"), Map->SizeX, Map->SizeY);
	UKismetRenderingLibrary::ClearRenderTarget2D(this, Map, FLinearColor::Transparent);
	UKismetRenderingLibrary::ClearRenderTarget2D(this, Spare, FLinearColor::Transparent);
	return true;
}

void UVaelFootprintSubsystem::Recentre(const FVector2D& Centre)
{
	const FVector2D NewOrigin = Centre - FVector2D(MapSize * 0.5f);
	if (bPlaced)
	{
		// The old map drawn shifted into the spare one, then back: prints stay where they are in the world
		const float Pixels = Map->SizeX;
		const FVector2D Shift = (Origin - NewOrigin) / MapSize * Pixels;
		UKismetRenderingLibrary::ClearRenderTarget2D(this, Spare, FLinearColor::Transparent);
		UCanvas* Canvas = nullptr;
		FVector2D CanvasSize;
		FDrawToRenderTargetContext Context;
		UKismetRenderingLibrary::BeginDrawCanvasToRenderTarget(this, Spare, Canvas, CanvasSize, Context);
		Canvas->K2_DrawTexture(Map, Shift, FVector2D(Pixels), FVector2D::ZeroVector, FVector2D::UnitVector, FLinearColor::White, BLEND_Opaque);
		UKismetRenderingLibrary::EndDrawCanvasToRenderTarget(this, Context);
		UKismetRenderingLibrary::BeginDrawCanvasToRenderTarget(this, Map, Canvas, CanvasSize, Context);
		Canvas->K2_DrawTexture(Spare, FVector2D::ZeroVector, FVector2D(Pixels), FVector2D::ZeroVector, FVector2D::UnitVector, FLinearColor::White, BLEND_Opaque);
		UKismetRenderingLibrary::EndDrawCanvasToRenderTarget(this, Context);
	}
	Origin = NewOrigin;
	bPlaced = true;
	UKismetMaterialLibrary::SetVectorParameterValue(this, Parameters, TEXT("SpurenUrsprung"), FLinearColor(Origin.X, Origin.Y, MapSize, 0.0f));
}

void UVaelFootprintSubsystem::Stamp(const FVector& Location, float Yaw, EVaelPrint Print, float Size, float Depth)
{
	if (!EnsureResources() || !bPlaced)
	{
		return;
	}
	const float Pixels = Map->SizeX;
	// Map pixels: u along world Y (east), v along world X (north), so the material reads it as world.xy / size
	const FVector2D Local = (FVector2D(Location.X, Location.Y) - Origin) / MapSize * Pixels;
	if (Local.X < 0.0f || Local.Y < 0.0f || Local.X > Pixels || Local.Y > Pixels)
	{
		return;
	}
	const float SizePixels = Size / MapSize * Pixels;
	UCanvas* Canvas = nullptr;
	FVector2D CanvasSize;
	FDrawToRenderTargetContext Context;
	UKismetRenderingLibrary::BeginDrawCanvasToRenderTarget(this, Map, Canvas, CanvasSize, Context);
	const int32 Cell = static_cast<int32>(Print);
	// Colour = (depth, clock); the shape's alpha blends it over what is there
	Canvas->K2_DrawTexture(Shapes, FVector2D(Local.X, Local.Y) - FVector2D(SizePixels * 0.5f), FVector2D(SizePixels),
		FVector2D(Cell * 0.25f, 0.0f), FVector2D(0.25f, 1.0f), FLinearColor(Depth, ClockValue(GetWorld()), 0.0f, 1.0f), BLEND_Translucent,
		Yaw, FVector2D(0.5f, 0.5f));
	UKismetRenderingLibrary::EndDrawCanvasToRenderTarget(this, Context);
	static int32 Stamps = 0;
	if (++Stamps % 20 == 1)
	{
		UE_LOG(LogVael, Verbose, TEXT("Footprints: %d prints, last at pixel %s"), Stamps, *Local.ToString());
	}
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

	// The middle of the group; the map follows it
	FVector2D Middle = FVector2D::ZeroVector;
	TArray<AVaelCharacter*> Players;
	for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
	{
		if (AVaelCharacter* Player = It->Get() != nullptr ? It->Get()->GetPawn<AVaelCharacter>() : nullptr)
		{
			Players.Add(Player);
			Middle += FVector2D(Player->GetActorLocation().X, Player->GetActorLocation().Y);
		}
	}
	if (Players.IsEmpty())
	{
		return;
	}
	Middle /= Players.Num();
	if (!bPlaced || FVector2D::Distance(Middle, Origin + FVector2D(MapSize * 0.5f)) > RecentreDistance)
	{
		Recentre(Middle);
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
			if (!It->IsDead() && FVector2D::Distance(FVector2D(It->GetActorLocation().X, It->GetActorLocation().Y), Middle) < MapSize * 0.5f)
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
