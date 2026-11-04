// Copyright Epic Games, Inc. All Rights Reserved.

#include "Floromant/VaelFootRootsComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/HitResult.h"
#include "Engine/Texture.h"
#include "Engine/World.h"
#include "Floromant/VaelFloromantSettings.h"
#include "GameFramework/Actor.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "ProceduralMeshComponent.h"
#include "VaelAssets.h"
#include "World/VaelGround.h"

namespace
{
	/** Sides of each tube */
	constexpr int32 FootRootSides = 9;

	/** How deep the tips reach under the ground, in cm */
	constexpr float FootRootDepth = 14.0f;

	/** A point on a cubic Bezier curve */
	FVector Bezier(const FVector& A, const FVector& B, const FVector& C, const FVector& D, float T)
	{
		const float U = 1.0f - T;
		return A * (U * U * U) + B * (3.0f * U * U * T) + C * (3.0f * U * T * T) + D * (T * T * T);
	}
}

void UVaelFootRootsComponent::SetGrowth(USkeletalMeshComponent* Body, float Growth)
{
	if (Growth <= 0.0f || Body == nullptr)
	{
		if (Mesh != nullptr && ShownGrowth != 0.0f)
		{
			Mesh->ClearAllMeshSections();
		}
		Roots.Reset();
		ShownGrowth = 0.0f;
		return;
	}

	if (Mesh == nullptr)
	{
		Mesh = NewObject<UProceduralMeshComponent>(GetOwner());
		Mesh->SetupAttachment(this);
		Mesh->SetUsingAbsoluteLocation(true);
		Mesh->SetUsingAbsoluteRotation(true);
		Mesh->SetUsingAbsoluteScale(true);
		Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Mesh->bReceivesDecals = false;
		Mesh->RegisterComponent();
		Mesh->SetWorldTransform(FTransform::Identity);

		// The root material of the plants without the sway of the wind, with dark mangrove bark laid over it
		const UVaelFloromantSettings* Settings = UVaelFloromantSettings::Get();
		if (UMaterialInterface* Material = VaelAssets::LoadOptional(Settings->RootMaterial))
		{
			UMaterialInstanceDynamic* Instance = UMaterialInstanceDynamic::Create(Material, this);
			Instance->SetScalarParameterValue(TEXT("WindStrength"), 0.0f);
			UTexture* Bark = VaelAssets::LoadOptional(Settings->RootBarkTexture);
			if (Bark != nullptr)
			{
				Instance->SetTextureParameterValue(TEXT("BaseColorMap"), Bark);
				Instance->SetScalarParameterValue(TEXT("TextureAmount"), 1.0f);
				Instance->SetVectorParameterValue(TEXT("Tint"), Settings->RootBarkTint);
			}
			if (UTexture* BarkNormal = VaelAssets::LoadOptional(Settings->RootBarkNormal); Bark != nullptr && BarkNormal != nullptr)
			{
				Instance->SetTextureParameterValue(TEXT("NormalMap"), BarkNormal);
			}
			Mesh->SetMaterial(0, Instance);
		}
	}

	if (Roots.IsEmpty())
	{
		PlanRoots(Body);
	}

	if (!FMath::IsNearlyEqual(Growth, ShownGrowth, 0.01f) || (Growth >= 1.0f && ShownGrowth < 1.0f))
	{
		BuildMesh(Growth);
		ShownGrowth = Growth;
	}
}

float UVaelFootRootsComponent::GroundAt(const FVector& Location, float Fallback) const
{
	FHitResult Hit;
	return VaelGround::TraceGround(GetWorld(), FVector(Location.X, Location.Y, Fallback + 80.0f), FVector(Location.X, Location.Y, Fallback - 150.0f), Hit, GetOwner())
		? Hit.ImpactPoint.Z : Fallback;
}

UVaelFootRootsComponent::FRootPath& UVaelFootRootsComponent::AddCurve(TFunctionRef<FVector(float)> Curve, int32 Count, float Radius, float Sway,
	float GrowStart, float GrowEnd)
{
	FRootPath& Root = Roots.AddDefaulted_GetRef();
	Root.Radius = Radius;
	Root.GrowStart = GrowStart;
	Root.GrowEnd = FMath::Max(GrowEnd, GrowStart + 0.05f);
	Root.Seed = FMath::FRandRange(0.0f, 100.0f);
	Root.Knots = FMath::FRandRange(2.0f, 4.5f);

	// Crooked all the way: a slow sway across the curve and a little jitter, never at the start where it meets the leg
	const float Turns = FMath::FRandRange(1.2f, 2.6f);
	const float Phase = FMath::FRandRange(0.0f, 2.0f * UE_PI);
	for (int32 Point = 0; Point < Count; ++Point)
	{
		const float T = Point / static_cast<float>(Count - 1);
		const FVector Here = Curve(T);
		const FVector Ahead = (Curve(FMath::Min(T + 0.02f, 1.0f)) - Curve(FMath::Max(T - 0.02f, 0.0f))).GetSafeNormal();
		const FVector Side = FVector::CrossProduct(Ahead, FVector::UpVector).GetSafeNormal(KINDA_SMALL_NUMBER, FVector::RightVector);
		const FVector Lift = FVector::CrossProduct(Side, Ahead);
		const float Fade = FMath::SmoothStep(0.0f, 0.25f, T);
		Root.Points.Add(Here + (Side * (FMath::Sin(T * Turns * UE_PI + Phase) * Sway + FMath::FRandRange(-1.0f, 1.0f) * Sway * 0.25f)
			+ Lift * FMath::FRandRange(-0.6f, 0.6f) * Sway * 0.4f) * Fade);
	}
	return Root;
}

void UVaelFootRootsComponent::PlanRoots(USkeletalMeshComponent* Body)
{
	const UVaelFloromantSettings* Settings = UVaelFloromantSettings::Get();
	const FVector Middle = Body->GetComponentLocation();
	const float Thick = Settings->RootThickness;

	for (const TCHAR* FootName : { TEXT("foot_l"), TEXT("foot_r") })
	{
		const FName Foot(FootName);
		if (!Body->DoesSocketExist(Foot))
		{
			continue;
		}

		// Where the foot will stand: she may still hover as the roots start, so all is measured from the ground under it
		const FVector FootLocation = Body->GetSocketLocation(Foot);
		const float GroundZ = GroundAt(FootLocation, FootLocation.Z - 10.0f);
		const FVector Sole(FootLocation.X, FootLocation.Y, GroundZ + 3.0f);

		// Spread out to all sides, but mostly away from the body
		FVector Outward = (FootLocation - Middle) * FVector(1.0f, 1.0f, 0.0f);
		Outward = Outward.IsNearlyZero(1.0f) ? FVector::ForwardVector : Outward.GetSafeNormal();
		const float OutwardYaw = Outward.Rotation().Yaw;
		const auto FanDirection = [OutwardYaw](int32 Index, int32 Count, float Spread, float Jitter)
		{
			const float Share = Count > 1 ? Index / static_cast<float>(Count - 1) : 0.5f;
			return FRotator(0.0f, OutwardYaw + FMath::Lerp(-Spread, Spread, Share) + FMath::FRandRange(-Jitter, Jitter), 0.0f).Vector();
		};

		// The collar: short gnarled stubs out of the ankle that hold the foot, the first thing to grow
		for (int32 Index = 0; Index < 4; ++Index)
		{
			const FVector Direction = FanDirection(Index, 4, 150.0f, 20.0f);
			const FVector Start = Sole + FVector(0.0f, 0.0f, FMath::FRandRange(8.0f, 13.0f)) + Direction * 2.0f;
			const FVector End = Sole + Direction * FMath::FRandRange(12.0f, 20.0f);
			const FVector EndDown(End.X, End.Y, GroundAt(End, GroundZ) - FootRootDepth * 0.6f);
			FRootPath& Root = AddCurve([&](float T)
			{
				return Bezier(Start, Start + Direction * 6.0f, EndDown + FVector(0.0f, 0.0f, 9.0f) - Direction * 2.0f, EndDown, T);
			}, 6, Thick * FMath::FRandRange(0.55f, 0.75f), 1.0f, 0.0f, FMath::FRandRange(0.35f, 0.5f));
			Root.Flare = 0.25f;
			Root.Tip = 0.35f;
		}

		// The stilts: thick roots that leave the shin, arch out and up and come down into the ground far around her
		for (int32 Index = 0; Index < Settings->RootsPerFoot; ++Index)
		{
			const FVector Direction = FanDirection(Index, Settings->RootsPerFoot, 105.0f, 18.0f);
			const float Reach = Settings->RootLength * FMath::FRandRange(0.7f, 1.15f);
			const FVector Start = Sole + FVector(0.0f, 0.0f, FMath::FRandRange(18.0f, 30.0f)) + Direction * 3.0f;
			const FVector Land = Sole + Direction * Reach;
			const FVector End(Land.X, Land.Y, GroundAt(Land, GroundZ) - FootRootDepth);
			const float Arch = FMath::FRandRange(10.0f, 22.0f);
			const float Radius = Thick * FMath::FRandRange(0.75f, 1.1f);
			const float GrowStart = FMath::FRandRange(0.08f, 0.2f);
			const float GrowEnd = FMath::FRandRange(0.7f, 0.85f);
			const FVector Out = Start + Direction * Reach * 0.32f + FVector(0.0f, 0.0f, Arch);
			const FVector Down = End + FVector(0.0f, 0.0f, Reach * 0.32f + Arch) - Direction * Reach * 0.12f;
			const auto Stilt = [&](float T) { return Bezier(Start, Out, Down, End, T); };
			FRootPath& Root = AddCurve(Stilt, 14, Radius, Reach * 0.06f, GrowStart, GrowEnd);
			Root.Flare = 0.55f;
			Root.Tip = 0.3f;

			// Little rootlets fan out where it goes into the ground, once it is there
			const FVector Foothold = Stilt(0.86f);
			for (int32 Rootlet = 0; Rootlet < 2; ++Rootlet)
			{
				const FVector Side = FRotator(0.0f, (Rootlet == 0 ? -1.0f : 1.0f) * FMath::FRandRange(35.0f, 70.0f), 0.0f).RotateVector(Direction);
				const FVector Tip = Foothold + Side * FMath::FRandRange(14.0f, 28.0f);
				const FVector TipDown(Tip.X, Tip.Y, GroundAt(Tip, GroundZ) - FootRootDepth * 0.7f);
				FRootPath& Small = AddCurve([&](float T)
				{
					return Bezier(Foothold, Foothold + Side * 6.0f + FVector(0.0f, 0.0f, 2.0f), TipDown + FVector(0.0f, 0.0f, 8.0f), TipDown, T);
				}, 6, Radius * 0.38f, 1.5f, GrowEnd - 0.08f, FMath::Min(1.0f, GrowEnd + FMath::FRandRange(0.12f, 0.2f)));
				Small.Flare = 0.2f;
				Small.Tip = 0.15f;
			}
		}

		// Creepers: thinner roots that run over the ground between the stilts, following its rise and fall, then dive in
		for (int32 Index = 0; Index < Settings->CreepingRootsPerFoot; ++Index)
		{
			const FVector Direction = FanDirection(Index, Settings->CreepingRootsPerFoot, 125.0f, 25.0f);
			const float Reach = Settings->RootLength * FMath::FRandRange(0.45f, 0.8f);
			constexpr int32 Count = 10;
			TArray<float> Heights;
			for (int32 Point = 0; Point < Count; ++Point)
			{
				const float T = Point / static_cast<float>(Count - 1);
				Heights.Add(GroundAt(Sole + Direction * Reach * T, GroundZ));
			}
			const FVector Start = Sole + FVector(0.0f, 0.0f, 5.0f);
			FRootPath& Root = AddCurve([&](float T)
			{
				const float Place = T * (Count - 1);
				const int32 Low = FMath::Min(FMath::FloorToInt32(Place), Count - 2);
				const float Z = FMath::Lerp(Heights[Low], Heights[Low + 1], Place - Low);
				// Half sunk along the way, deeper at its end
				const float Sink = FMath::Lerp(-1.0f, FootRootDepth, FMath::Pow(T, 3.0f));
				return FVector(Start.X, Start.Y, 0.0f) + Direction * Reach * T + FVector(0.0f, 0.0f, FMath::Lerp(Start.Z, Z - Sink, FMath::SmoothStep(0.0f, 0.2f, T)));
			}, Count, Thick * FMath::FRandRange(0.38f, 0.5f), Reach * 0.07f, FMath::FRandRange(0.25f, 0.4f), FMath::FRandRange(0.85f, 1.0f));
			Root.Flare = 0.3f;
			Root.Tip = 0.2f;
		}
	}
}

void UVaelFootRootsComponent::BuildMesh(float Growth)
{
	const UVaelFloromantSettings* Settings = UVaelFloromantSettings::Get();
	const float Tile = FMath::Max(Settings->RootBarkTile, 1.0f);

	TArray<FVector> Vertices;
	TArray<FVector> Normals;
	TArray<FVector2D> UVs;
	TArray<FProcMeshTangent> Tangents;
	TArray<int32> Triangles;

	for (const FRootPath& Root : Roots)
	{
		// Each root grows in its own share of the whole growth: whole stretches and a part of the next, pointed at the growing end
		const float Own = FMath::Clamp((Growth - Root.GrowStart) / (Root.GrowEnd - Root.GrowStart), 0.0f, 1.0f);
		if (Own <= 0.0f)
		{
			continue;
		}
		const float Reach = FMath::SmoothStep(0.0f, 1.0f, Own) * (Root.Points.Num() - 1);
		const int32 Whole = FMath::FloorToInt32(Reach);
		TArray<FVector> Path;
		TArray<float> Shares;
		for (int32 Point = 0; Point <= FMath::Min(Whole, Root.Points.Num() - 1); ++Point)
		{
			Path.Add(Root.Points[Point]);
			Shares.Add(Point / static_cast<float>(Root.Points.Num() - 1));
		}
		if (Whole < Root.Points.Num() - 1 && Reach > Whole)
		{
			Path.Add(FMath::Lerp(Root.Points[Whole], Root.Points[Whole + 1], Reach - Whole));
			Shares.Add(Reach / (Root.Points.Num() - 1));
		}
		if (Path.Num() < 2)
		{
			continue;
		}

		// Young roots are thinner and swell to their full strength as they grow
		const float Swell = FMath::Lerp(0.45f, 1.0f, Own);
		const int32 First = Vertices.Num();
		float Length = 0.0f;
		for (int32 Ring = 0; Ring < Path.Num(); ++Ring)
		{
			if (Ring > 0)
			{
				Length += FVector::Dist(Path[Ring], Path[Ring - 1]);
			}
			const FVector Along = (Path[FMath::Min(Ring + 1, Path.Num() - 1)] - Path[FMath::Max(Ring - 1, 0)]).GetSafeNormal();
			const FVector Across = FVector::CrossProduct(Along, FVector::UpVector).GetSafeNormal(KINDA_SMALL_NUMBER, FVector::RightVector);
			const FVector Up = FVector::CrossProduct(Across, Along);
			const float Share = Shares[Ring];

			// Tapering to the tip, flared where it leaves the leg, swollen in knots; the growing end comes to a point
			float Radius = Root.Radius * Swell * FMath::Lerp(1.0f, Root.Tip, FMath::Pow(Share, 0.8f));
			Radius *= 1.0f + Root.Flare * FMath::Square(1.0f - FMath::Clamp(Share / 0.18f, 0.0f, 1.0f));
			Radius *= 1.0f + 0.16f * FMath::Sin(Share * Root.Knots * 2.0f * UE_PI + Root.Seed) + 0.07f * FMath::Sin(Share * 11.0f + Root.Seed * 1.7f);
			if (Own < 1.0f && Ring == Path.Num() - 1)
			{
				Radius *= 0.25f;
			}

			for (int32 SideIndex = 0; SideIndex <= FootRootSides; ++SideIndex)
			{
				const float Angle = 2.0f * UE_PI * SideIndex / FootRootSides;
				const FVector Normal = Across * FMath::Cos(Angle) + Up * FMath::Sin(Angle);
				// Ridged and a little flattened like old wood, never a clean tube
				const float Ridges = 1.0f + 0.1f * FMath::Sin(Angle * 3.0f + Root.Seed) + 0.06f * FMath::Sin(Angle * 5.0f + Root.Seed * 2.3f + Share * 4.0f);
				const FVector Offset = (Across * FMath::Cos(Angle) * 1.08f + Up * FMath::Sin(Angle) * 0.92f) * Radius * Ridges;
				Vertices.Add(Path[Ring] + Offset);
				Normals.Add(Normal);
				Tangents.Add(FProcMeshTangent(-Across * FMath::Sin(Angle) + Up * FMath::Cos(Angle), false));
				UVs.Add(FVector2D(SideIndex / static_cast<float>(FootRootSides) + Root.Seed, Length / Tile));
			}
		}

		const int32 RingSize = FootRootSides + 1;
		for (int32 Ring = 0; Ring < Path.Num() - 1; ++Ring)
		{
			for (int32 SideIndex = 0; SideIndex < FootRootSides; ++SideIndex)
			{
				const int32 A = First + Ring * RingSize + SideIndex;
				const int32 B = A + 1;
				const int32 C = A + RingSize;
				const int32 D = C + 1;
				Triangles.Append({ A, C, B, B, C, D });
			}
		}
	}

	Mesh->CreateMeshSection_LinearColor(0, Vertices, Triangles, Normals, UVs, TArray<FLinearColor>(), Tangents, false);
}
