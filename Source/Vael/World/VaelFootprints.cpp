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
#include "GameFramework/Character.h"
#include "Components/SkeletalMeshComponent.h"
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

void UVaelFootprintSubsystem::Stamp(const FVector& Location, float Yaw, EVaelPrint Print, float Size, float Depth, bool bMirror)
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
		// A left foot reads the cell from right to left: the same shape mirrored
		Canvas->K2_DrawTexture(Shapes, FVector2D(Local.X, Local.Y) - FVector2D(SizePixels * 0.5f), FVector2D(SizePixels),
			FVector2D(Cell * 0.25f + (bMirror ? 0.25f : 0.0f), 0.0f), FVector2D(bMirror ? -0.25f : 0.25f, 1.0f),
			FLinearColor(Depth, ClockValue(GetWorld()), 0.0f, 1.0f), BLEND_Translucent, Yaw, FVector2D(0.5f, 0.5f));
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
		if (!bPlaced[Slot] || (FVector2D::Distance(At, Origins[Slot] + FVector2D(MapSize * 0.5f)) > RecentreDistance))
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
			FollowFeet(Player, EVaelPrint::Boot, 68.0f, 34.0f, 11.0f, 1.0f, DeltaTime);
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
			FollowFeet(Creature, Radius > 70.0f ? EVaelPrint::Paw : EVaelPrint::Claw, FMath::Max(Radius * 0.9f, 40.0f),
				FMath::Clamp(Radius * 0.7f, 18.0f, 60.0f), Radius * 0.35f, 0.8f, DeltaTime);
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
	for (int32 Slot = 0; Slot < NumMaps; ++Slot)
	{
		const FLinearColor Param = UKismetMaterialLibrary::GetVectorParameterValue(this, Parameters, *FString::Printf(TEXT("SpurenUrsprung%d"), Slot));
		UE_LOG(LogVael, Display, TEXT("[Spuren] map %d placed %d parked %d origin %s, parameter %s"), Slot, bPlaced[Slot], bParked[Slot], *Origins[Slot].ToString(), *Param.ToString());
	}
	// A new probe: the print pressed last (read right away, before the next move of the map)
	ProbeWorld = LastStampWorld;
	ProbeValue = ReadAt(LastStampWorld);
	bProbe = true;
}

void UVaelFootprintSubsystem::FollowFeet(ACharacter* Walker, EVaelPrint Print, float Stride, float Size, float Side, float Depth, float DeltaTime)
{
	FWalker& State = Walkers.FindOrAdd(Walker);
	USkeletalMeshComponent* Body = Walker->GetMesh();
	if (!State.bFeetChecked && Body != nullptr && Body->GetSkeletalMeshAsset() != nullptr)
	{
		State.bFeetChecked = true;
		// The player skeleton: foot and ball bones; others: every bone named like a foot ("foot", "fuss"), the toes
		// guessed from the way the walker faces
		if (Body->GetBoneIndex(TEXT("foot_l")) != INDEX_NONE && Body->GetBoneIndex(TEXT("foot_r")) != INDEX_NONE)
		{
			for (int32 Side2 = 0; Side2 < 2; ++Side2)
			{
				FFoot& Foot = State.Feet.AddDefaulted_GetRef();
				Foot.Bone = Side2 == 0 ? TEXT("foot_l") : TEXT("foot_r");
				Foot.Toe = Body->GetBoneIndex(Side2 == 0 ? TEXT("ball_l") : TEXT("ball_r")) != INDEX_NONE ? FName(Side2 == 0 ? TEXT("ball_l") : TEXT("ball_r")) : NAME_None;
				Foot.bLeft = Side2 == 0;
			}
		}
		else
		{
			TArray<FName> Names;
			Body->GetBoneNames(Names);
			for (const FName& Name : Names)
			{
				const FString Lower = Name.ToString().ToLower();
				if ((Lower.Contains(TEXT("foot")) || Lower.Contains(TEXT("fuss"))) && !Lower.StartsWith(TEXT("ik_")) && State.Feet.Num() < 8)
				{
					FFoot& Foot = State.Feet.AddDefaulted_GetRef();
					Foot.Bone = Name;
					Foot.bLeft = Lower.EndsWith(TEXT("_l")) || Lower.Contains(TEXT("left"));
				}
			}
			if (State.Feet.Num() < 2)
			{
				State.Feet.Reset();
			}
		}
	}
	if (State.Feet.IsEmpty() || Body == nullptr || DeltaTime <= 0.0f)
	{
		FollowWalker(Walker, Print, Stride, Size, Side, Depth);
		return;
	}

	for (FFoot& Foot : State.Feet)
	{
		const FVector At = Body->GetSocketLocation(Foot.Bone);
		const FVector Toe = Foot.Toe != NAME_None ? Body->GetSocketLocation(Foot.Toe) : At + Walker->GetActorForwardVector() * Size * 0.4f;
		const float Speed = Foot.bStarted ? FVector::Dist2D(At, Foot.Last) / DeltaTime : 0.0f;
		Foot.Last = At;
		Foot.bStarted = true;
		FVector Ground;
		if (!AVaelCreature::FindGround(GetWorld(), At + FVector(0.0f, 0.0f, 60.0f), Ground) || At.Z - Ground.Z > 200.0f)
		{
			continue;
		}
		const float Height = At.Z - Ground.Z;
		// How low the bone sits on the ground when the foot stands: the lowest lately, creeping up slowly for slopes
		Foot.Lowest = FMath::Min(Foot.Lowest + DeltaTime * 4.0f, Height);
		// Touching down: the lowest point of a step, where the foot stops coming down and starts to lift, close to where
		// it stands (a running foot hardly rests, and on slopes the low point of a step differs a little); a standing
		// foot counts once. Lifted again when it is well above the ground.
		const bool bTurnedUp = Foot.bFalling && Height > Foot.LastHeight + 0.2f;
		const bool bStill = Speed < 40.0f;
		const bool bResting = Height < Foot.Lowest + 12.0f && (bTurnedUp || bStill);
		const bool bLifted = Height > Foot.Lowest + 16.0f;
		if (Height < Foot.LastHeight - 0.2f)
		{
			Foot.bFalling = true;
		}
		else if (Height > Foot.LastHeight + 0.2f)
		{
			Foot.bFalling = false;
		}
		Foot.LastHeight = Height;

		if (bResting && !Foot.bDown)
		{
			Foot.bDown = true;
			// Under the middle of the foot, from heel to toes, turned the way the toes point; the atlas cells point
			// their toes up (v down = forward)
			// The way the toes point: along the foot bones, turned round if that points backwards (on the player
			// skeleton the ball bone lies behind the ankle in the world), else the way the walker faces
			const FVector2D Facing = FVector2D(Walker->GetActorForwardVector()).GetSafeNormal();
			FVector2D Ahead = FVector2D(Toe - At).GetSafeNormal();
			if (Ahead.IsNearlyZero())
			{
				Ahead = Facing;
			}
			else if (FVector2D::DotProduct(Ahead, Facing) < 0.0f)
			{
				Ahead = -Ahead;
			}
			// The ankle stands over the back of the foot: the print's middle a little ahead of it
			const FVector Middle = At + FVector(Ahead.X, Ahead.Y, 0.0f) * Size * 0.18f;
			const float Yaw = FMath::RadiansToDegrees(FMath::Atan2(Ahead.Y, Ahead.X));
			Stamp(FVector(Middle.X, Middle.Y, Ground.Z), Yaw + 90.0f, Print, Size, Depth, Foot.bLeft);
			if (FParse::Param(FCommandLine::Get(), TEXT("VaelSpurFuss")))
			{
				UE_LOG(LogVael, Display, TEXT("[Fuss] print %s at %s (walker %s), yaw %.0f"), *Foot.Bone.ToString(), *Middle.ToCompactString(),
					*Walker->GetActorLocation().ToCompactString(), Yaw);
			}
		}
		else if (bLifted)
		{
			Foot.bDown = false;
		}
	}
}

