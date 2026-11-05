// Copyright Epic Games, Inc. All Rights Reserved.

#include "Floromant/VaelFootRootsComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/HitResult.h"
#include "Engine/StaticMesh.h"
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
	/** Sides of the tubes in the ground and on the legs */
	constexpr int32 GroundSides = 9;
	constexpr int32 LegSides = 7;

	/** How deep the tips reach under the ground, in cm */
	constexpr float FootRootDepth = 14.0f;

	/** Points along a leg root */
	constexpr int32 LegPoints = 26;

	/** Share of a root a pulse covers */
	constexpr float PulseLength = 0.14f;

	const TCHAR* Feet[] = { TEXT("foot_l"), TEXT("foot_r") };
	const TCHAR* Balls[] = { TEXT("ball_l"), TEXT("ball_r") };
	const TCHAR* Knees[] = { TEXT("calf_l"), TEXT("calf_r") };

	FVector Bezier(const FVector& A, const FVector& B, const FVector& C, const FVector& D, float T)
	{
		const float U = 1.0f - T;
		return A * (U * U * U) + B * (3.0f * U * U * T) + C * (3.0f * U * T * T) + D * (T * T * T);
	}

	/** The part of a path from Share0 to Share1 of its length (by point index), with the share of each point */
	void Slice(const TArray<FVector>& Points, float Share0, float Share1, TArray<FVector>& OutPath, TArray<float>& OutShares)
	{
		OutPath.Reset();
		OutShares.Reset();
		const int32 Last = Points.Num() - 1;
		if (Last < 1 || Share1 <= Share0)
		{
			return;
		}
		auto At = [&](float Share)
		{
			const float Reach = FMath::Clamp(Share, 0.0f, 1.0f) * Last;
			const int32 Low = FMath::Min(FMath::FloorToInt32(Reach), Last - 1);
			return FMath::Lerp(Points[Low], Points[Low + 1], Reach - Low);
		};
		OutPath.Add(At(Share0));
		OutShares.Add(Share0);
		for (int32 Point = FMath::CeilToInt32(Share0 * Last + 0.001f); Point < Share1 * Last; ++Point)
		{
			OutPath.Add(Points[Point]);
			OutShares.Add(Point / static_cast<float>(Last));
		}
		OutPath.Add(At(Share1));
		OutShares.Add(Share1);
	}
}

void UVaelFootRootsComponent::SetGrowth(USkeletalMeshComponent* Body, float Growth, bool bFiring, float DeltaTime)
{
	if (Growth <= 0.0f || Body == nullptr)
	{
		if (Mesh != nullptr && ShownGrowth != 0.0f)
		{
			Mesh->ClearAllMeshSections();
		}
		for (UStaticMeshComponent* Tuft : MossTufts)
		{
			if (Tuft != nullptr)
			{
				Tuft->SetVisibility(false);
			}
		}
		Roots.Reset();
		LegRoots.Reset();
		Pulses.Reset();
		MossPlaces.Reset();
		ShownGrowth = 0.0f;
		return;
	}

	const UVaelFloromantSettings* Settings = UVaelFloromantSettings::Get();
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
		if (UMaterialInterface* Material = VaelAssets::LoadOptional(Settings->RootMaterial))
		{
			UMaterialInstanceDynamic* Instance = UMaterialInstanceDynamic::Create(Material, this);
			Instance->SetScalarParameterValue(TEXT("WindStrength"), 0.0f);
			// No green growing seam over the whole root (the material would need ground height and plant height for it)
			Instance->SetScalarParameterValue(TEXT("GrowGlow"), 0.0f);
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

			// The pulses of earth power: the same material, glowing all over in the colour of the pulse
			PulseMaterial = UMaterialInstanceDynamic::Create(Material, this);
			PulseMaterial->SetScalarParameterValue(TEXT("WindStrength"), 0.0f);
			PulseMaterial->SetScalarParameterValue(TEXT("TextureAmount"), 0.0f);
			PulseMaterial->SetVectorParameterValue(TEXT("Tint"), Settings->RootPulseColor);
			PulseMaterial->SetScalarParameterValue(TEXT("Flash"), 1.0f);
			PulseMaterial->SetVectorParameterValue(TEXT("FlashColor"), Settings->RootPulseColor);
			PulseMaterial->SetScalarParameterValue(TEXT("FlashGlow"), 1.4f);
			Mesh->SetMaterial(1, PulseMaterial);
		}
	}

	// The moss: tufts of the Keimpelz mesh in the plant material, a deep moss green (single meshes: the plant material is not
	// marked for instanced meshes and would fall back to the grey default there)
	if (MossMesh == nullptr)
	{
		MossMesh = VaelAssets::LoadOptional(Settings->KeimpelzPadMesh);
		if (UMaterialInterface* Material = VaelAssets::LoadOptional(Settings->RootMaterial))
		{
			MossLook = UMaterialInstanceDynamic::Create(Material, this);
			MossLook->SetScalarParameterValue(TEXT("WindStrength"), 0.0f);
			MossLook->SetScalarParameterValue(TEXT("GrowGlow"), 0.0f);
			if (UTexture* MossPicture = LoadObject<UTexture>(nullptr, TEXT("/Game/Vael/Requisiten/Aschenmark/Keimpelz/T_Keimpelz_basecolor.T_Keimpelz_basecolor"), nullptr, LOAD_NoWarn | LOAD_Quiet))
			{
				MossLook->SetTextureParameterValue(TEXT("BaseColorMap"), MossPicture);
				MossLook->SetScalarParameterValue(TEXT("TextureAmount"), 0.5f);
			}
			MossLook->SetVectorParameterValue(TEXT("Tint"), FLinearColor(0.1f, 0.32f, 0.04f));
			MossLook->SetScalarParameterValue(TEXT("Roughness"), 0.95f);
		}
	}

	if (Roots.IsEmpty())
	{
		PlanRoots(Body);
	}

	// Pulses of earth power while she fires: from the ground up every root, into the legs a little later
	if (bFiring && Growth >= 1.0f)
	{
		PulseCountdown -= DeltaTime;
		if (PulseCountdown <= 0.0f)
		{
			PulseCountdown = Settings->RootPulseInterval;
			for (int32 Root = 0; Root < Roots.Num(); ++Root)
			{
				if (FMath::FRand() < 0.35f)
				{
					Pulses.Add({ Root, INDEX_NONE, -FMath::FRandRange(0.0f, 0.25f) });
				}
			}
			for (int32 Leg = 0; Leg < LegRoots.Num(); ++Leg)
			{
				Pulses.Add({ INDEX_NONE, Leg, -0.55f - FMath::FRandRange(0.0f, 0.2f) });
			}
		}
	}
	else
	{
		PulseCountdown = 0.0f;
	}
	const float Speed = 1.0f / FMath::Max(Settings->RootPulseTime, 0.05f);
	for (FPulse& Pulse : Pulses)
	{
		Pulse.Along += DeltaTime * Speed;
	}
	Pulses.RemoveAll([](const FPulse& Pulse) { return Pulse.Along > 1.0f + PulseLength; });

	// The leg roots follow the bones every frame, so the mesh is built anew while she stands rooted
	BuildMesh(Body, Growth);
	if (!FMath::IsNearlyEqual(Growth, ShownGrowth, 0.01f) || (Growth >= 1.0f && ShownGrowth < 1.0f))
	{
		PlaceMoss(Growth);
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
	MossPlaces.Reset();

	for (int32 FootIndex = 0; FootIndex < 2; ++FootIndex)
	{
		const FName Foot(Feet[FootIndex]);
		if (!Body->DoesSocketExist(Foot))
		{
			continue;
		}

		// Where the foot will stand: she may still hover as the roots start, so all is measured from the ground under it
		const FVector FootLocation = Body->GetSocketLocation(Foot);
		const float GroundZ = GroundAt(FootLocation, FootLocation.Z - 10.0f);
		const FVector Sole(FootLocation.X, FootLocation.Y, GroundZ + 3.0f);
		const int32 FirstRoot = Roots.Num();

		FVector Outward = (FootLocation - Middle) * FVector(1.0f, 1.0f, 0.0f);
		Outward = Outward.IsNearlyZero(1.0f) ? FVector::ForwardVector : Outward.GetSafeNormal();
		const float OutwardYaw = Outward.Rotation().Yaw;
		const auto FanDirection = [OutwardYaw](int32 Index, int32 Count, float Spread, float Jitter)
		{
			const float Share = Count > 1 ? Index / static_cast<float>(Count - 1) : 0.5f;
			return FRotator(0.0f, OutwardYaw + FMath::Lerp(-Spread, Spread, Share) + FMath::FRandRange(-Jitter, Jitter), 0.0f).Vector();
		};

		// The collar: short gnarled stubs out of the ankle that hold the foot, the first thing to grow
		for (int32 Index = 0; Index < 5; ++Index)
		{
			const FVector Direction = FanDirection(Index, 5, 170.0f, 15.0f);
			const FVector Start = Sole + FVector(0.0f, 0.0f, FMath::FRandRange(8.0f, 14.0f)) + Direction * 2.0f;
			const FVector End = Sole + Direction * FMath::FRandRange(13.0f, 22.0f);
			const FVector EndDown(End.X, End.Y, GroundAt(End, GroundZ) - FootRootDepth * 0.6f);
			FRootPath& Root = AddCurve([&](float T)
			{
				return Bezier(Start, Start + Direction * 6.0f, EndDown + FVector(0.0f, 0.0f, 9.0f) - Direction * 2.0f, EndDown, T);
			}, 7, Thick * FMath::FRandRange(0.6f, 0.8f), 1.0f, 0.0f, FMath::FRandRange(0.3f, 0.45f));
			Root.Flare = 0.25f;
			Root.Tip = 0.35f;
			Root.Foot = FootIndex;
			MossPlaces.Add(FTransform(FRotator(0.0f, FMath::FRandRange(0.0f, 360.0f), 0.0f), EndDown + FVector(0.0f, 0.0f, FootRootDepth * 0.6f + 1.0f), FVector(FMath::FRandRange(9.0f, 13.0f))));
		}

		// The stilts: thick roots that leave the shin, arch out and up and come down broad into the ground far around her
		for (int32 Index = 0; Index < Settings->RootsPerFoot; ++Index)
		{
			const FVector Direction = FanDirection(Index, Settings->RootsPerFoot, 105.0f, 18.0f);
			const float Reach = Settings->RootLength * FMath::FRandRange(0.7f, 1.15f);
			const FVector Start = Sole + FVector(0.0f, 0.0f, FMath::FRandRange(20.0f, 32.0f)) + Direction * 3.0f;
			const FVector Land = Sole + Direction * Reach;
			const FVector End(Land.X, Land.Y, GroundAt(Land, GroundZ) - FootRootDepth);
			const float Arch = FMath::FRandRange(10.0f, 22.0f);
			const float Radius = Thick * FMath::FRandRange(0.75f, 1.1f);
			const float GrowStart = FMath::FRandRange(0.12f, 0.25f);
			const float GrowEnd = FMath::FRandRange(0.7f, 0.85f);
			const FVector Out = Start + Direction * Reach * 0.32f + FVector(0.0f, 0.0f, Arch);
			const FVector Down = End + FVector(0.0f, 0.0f, Reach * 0.32f + Arch) - Direction * Reach * 0.12f;
			const auto Stilt = [&](float T) { return Bezier(Start, Out, Down, End, T); };
			FRootPath& Root = AddCurve(Stilt, 16, Radius, Reach * 0.06f, GrowStart, GrowEnd);
			Root.Flare = 0.55f;
			Root.Tip = 0.3f;
			Root.Foot = FootIndex;

			// Moss on the top of the arch and where it goes into the ground
			for (const float T : { 0.32f, 0.55f })
			{
				MossPlaces.Add(FTransform(FRotator(0.0f, FMath::FRandRange(0.0f, 360.0f), 0.0f), Stilt(T) + FVector(0.0f, 0.0f, Radius * 0.7f), FVector(FMath::FRandRange(8.0f, 12.0f))));
			}
			MossPlaces.Add(FTransform(FRotator(0.0f, FMath::FRandRange(0.0f, 360.0f), 0.0f), Stilt(0.9f) + FVector(0.0f, 0.0f, 2.0f), FVector(FMath::FRandRange(10.0f, 15.0f))));

			// Little rootlets fan out where it goes into the ground, once it is there
			const FVector Foothold = Stilt(0.86f);
			for (int32 Rootlet = 0; Rootlet < 3; ++Rootlet)
			{
				const FVector Side = FRotator(0.0f, (Rootlet - 1) * FMath::FRandRange(40.0f, 65.0f), 0.0f).RotateVector(Direction);
				const FVector Tip = Foothold + Side * FMath::FRandRange(14.0f, 30.0f);
				const FVector TipDown(Tip.X, Tip.Y, GroundAt(Tip, GroundZ) - FootRootDepth * 0.7f);
				FRootPath& Small = AddCurve([&](float T)
				{
					return Bezier(Foothold, Foothold + Side * 6.0f + FVector(0.0f, 0.0f, 2.0f), TipDown + FVector(0.0f, 0.0f, 8.0f), TipDown, T);
				}, 6, Radius * 0.38f, 1.5f, GrowEnd - 0.08f, FMath::Min(1.0f, GrowEnd + FMath::FRandRange(0.12f, 0.2f)));
				Small.Flare = 0.2f;
				Small.Tip = 0.15f;
				Small.Foot = FootIndex;
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
				Heights.Add(GroundAt(Sole + Direction * Reach * (Point / static_cast<float>(Count - 1)), GroundZ));
			}
			const FVector Start = Sole + FVector(0.0f, 0.0f, 5.0f);
			FRootPath& Root = AddCurve([&](float T)
			{
				const float Place = T * (Count - 1);
				const int32 Low = FMath::Min(FMath::FloorToInt32(Place), Count - 2);
				const float Z = FMath::Lerp(Heights[Low], Heights[Low + 1], Place - Low);
				const float Sink = FMath::Lerp(-1.0f, FootRootDepth, FMath::Pow(T, 3.0f));
				return FVector(Start.X, Start.Y, 0.0f) + Direction * Reach * T + FVector(0.0f, 0.0f, FMath::Lerp(Start.Z, Z - Sink, FMath::SmoothStep(0.0f, 0.2f, T)));
			}, Count, Thick * FMath::FRandRange(0.38f, 0.5f), Reach * 0.07f, FMath::FRandRange(0.25f, 0.4f), FMath::FRandRange(0.85f, 1.0f));
			Root.Flare = 0.3f;
			Root.Tip = 0.2f;
			Root.Foot = FootIndex;
		}

		// Roots that wind round the leg from the toes to the knee, the very first to grow out of her
		if (Body->DoesSocketExist(FName(Knees[FootIndex])))
		{
			for (int32 Index = 0; Index < Settings->LegRootsPerLeg; ++Index)
			{
				FLegRoot& Leg = LegRoots.AddDefaulted_GetRef();
				Leg.Foot = FootIndex;
				Leg.Phase = Index * 2.0f * UE_PI / FMath::Max(Settings->LegRootsPerLeg, 1) + FMath::FRandRange(-0.4f, 0.4f);
				Leg.Turns = FMath::FRandRange(1.1f, 1.7f) * (Index % 2 == 0 ? 1.0f : -1.0f);
				Leg.Radius = Thick * FMath::FRandRange(0.5f, 0.7f);
				Leg.GrowStart = FMath::FRandRange(0.0f, 0.12f);
				Leg.GrowEnd = FMath::FRandRange(0.55f, 0.75f);
				Leg.Seed = FMath::FRandRange(0.0f, 100.0f);
			}
		}
		(void)FirstRoot;
	}

	// Moss on both feet alike when only some of the places are used
	for (int32 Index = MossPlaces.Num() - 1; Index > 0; --Index)
	{
		MossPlaces.Swap(Index, FMath::RandRange(0, Index));
	}
}

void UVaelFootRootsComponent::LegPath(USkeletalMeshComponent* Body, const FLegRoot& Leg, TArray<FVector>& OutPoints) const
{
	OutPoints.Reset();
	const FVector Ankle = Body->GetSocketLocation(FName(Feet[Leg.Foot]));
	const FVector Knee = Body->GetSocketLocation(FName(Knees[Leg.Foot]));
	const FVector Ball = Body->DoesSocketExist(FName(Balls[Leg.Foot])) ? Body->GetSocketLocation(FName(Balls[Leg.Foot])) : Ankle + Body->GetForwardVector() * 10.0f;
	const FVector ToeTip = Ball + (Ball - Ankle).GetSafeNormal2D() * 5.0f;
	const float LegLength = FVector::Dist(Ankle, Knee);
	const float Shin = FMath::Clamp(LegLength * 0.17f, 4.0f, 12.0f);

	// The way: out of the ground in front of the toes, over the top of the foot, round the ankle, up the shin to the knee
	const FVector Rise(ToeTip.X, ToeTip.Y, ToeTip.Z - 6.0f);
	const FVector Keys[] = { Rise, ToeTip, Ball, Ankle, FMath::Lerp(Ankle, Knee, 0.5f), Knee };
	const float KeyShares[] = { 0.0f, 0.08f, 0.2f, 0.38f, 0.7f, 1.0f };
	for (int32 Point = 0; Point < LegPoints; ++Point)
	{
		const float T = Point / static_cast<float>(LegPoints - 1);
		int32 Key = 0;
		while (Key < 4 && T > KeyShares[Key + 1])
		{
			++Key;
		}
		const float Local = (T - KeyShares[Key]) / FMath::Max(KeyShares[Key + 1] - KeyShares[Key], 0.001f);
		const FVector Centre = FMath::Lerp(Keys[Key], Keys[Key + 1], Local);
		const FVector Axis = (Keys[Key + 1] - Keys[Key]).GetSafeNormal(KINDA_SMALL_NUMBER, FVector::UpVector);
		const FVector Across = FVector::CrossProduct(Axis, FVector::UpVector).GetSafeNormal(KINDA_SMALL_NUMBER, Body->GetRightVector());
		const FVector Round = FVector::CrossProduct(Across, Axis);
		// Lying on the foot it hugs its top, up the leg it winds round it; thicker round the calf
		const float Radius = T < 0.3f ? 2.0f : FMath::Lerp(Shin * 0.9f, Shin * 1.15f, FMath::Sin(FMath::Clamp((T - 0.38f) / 0.62f, 0.0f, 1.0f) * UE_PI));
		const float Angle = Leg.Phase + T * Leg.Turns * 2.0f * UE_PI;
		const float Wobble = 1.0f + 0.12f * FMath::Sin(T * 17.0f + Leg.Seed);
		OutPoints.Add(Centre + (Across * FMath::Cos(Angle) + Round * FMath::Sin(Angle) * (T < 0.3f ? 0.3f : 1.0f)) * Radius * Wobble
			+ (T < 0.3f ? FVector(0.0f, 0.0f, 2.5f) : FVector::ZeroVector));
	}
}

void UVaelFootRootsComponent::AddTube(TArray<FVector>& Vertices, TArray<FVector>& Normals, TArray<FVector2D>& UVs, TArray<FProcMeshTangent>& Tangents, TArray<int32>& Triangles,
	const TArray<FVector>& Path, const TArray<float>& Shares, float Radius, float Flare, float Tip, float Knots, float Seed, bool bPointedEnd, int32 Sides) const
{
	if (Path.Num() < 2)
	{
		return;
	}
	const float Tile = FMath::Max(UVaelFloromantSettings::Get()->RootBarkTile, 1.0f);
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
		float R = Radius * FMath::Lerp(1.0f, Tip, FMath::Pow(Share, 0.8f));
		R *= 1.0f + Flare * FMath::Square(1.0f - FMath::Clamp(Share / 0.18f, 0.0f, 1.0f));
		R *= 1.0f + 0.16f * FMath::Sin(Share * Knots * 2.0f * UE_PI + Seed) + 0.07f * FMath::Sin(Share * 11.0f + Seed * 1.7f);
		if (bPointedEnd && Ring == Path.Num() - 1)
		{
			R *= 0.25f;
		}

		for (int32 SideIndex = 0; SideIndex <= Sides; ++SideIndex)
		{
			const float Angle = 2.0f * UE_PI * SideIndex / Sides;
			const FVector Normal = Across * FMath::Cos(Angle) + Up * FMath::Sin(Angle);
			// Ridged and a little flattened like old wood, never a clean tube
			const float Ridges = 1.0f + 0.1f * FMath::Sin(Angle * 3.0f + Seed) + 0.06f * FMath::Sin(Angle * 5.0f + Seed * 2.3f + Share * 4.0f);
			Vertices.Add(Path[Ring] + (Across * FMath::Cos(Angle) * 1.08f + Up * FMath::Sin(Angle) * 0.92f) * R * Ridges);
			Normals.Add(Normal);
			Tangents.Add(FProcMeshTangent(-Across * FMath::Sin(Angle) + Up * FMath::Cos(Angle), false));
			UVs.Add(FVector2D(SideIndex / static_cast<float>(Sides) + Seed, Length / Tile));
		}
	}
	const int32 RingSize = Sides + 1;
	for (int32 Ring = 0; Ring < Path.Num() - 1; ++Ring)
	{
		for (int32 SideIndex = 0; SideIndex < Sides; ++SideIndex)
		{
			const int32 A = First + Ring * RingSize + SideIndex;
			Triangles.Append({ A, A + RingSize, A + 1, A + 1, A + RingSize, A + RingSize + 1 });
		}
	}
}

void UVaelFootRootsComponent::BuildMesh(USkeletalMeshComponent* Body, float Growth)
{
	TArray<FVector> Vertices, Normals;
	TArray<FVector2D> UVs;
	TArray<FProcMeshTangent> Tangents;
	TArray<int32> Triangles;
	TArray<FVector> Path;
	TArray<float> Shares;

	auto Own = [Growth](float Start, float End) { return FMath::Clamp((Growth - Start) / FMath::Max(End - Start, 0.01f), 0.0f, 1.0f); };

	for (const FRootPath& Root : Roots)
	{
		const float Grown = Own(Root.GrowStart, Root.GrowEnd);
		if (Grown <= 0.0f)
		{
			continue;
		}
		Slice(Root.Points, 0.0f, FMath::SmoothStep(0.0f, 1.0f, Grown), Path, Shares);
		AddTube(Vertices, Normals, UVs, Tangents, Triangles, Path, Shares, Root.Radius * FMath::Lerp(0.45f, 1.0f, Grown), Root.Flare, Root.Tip, Root.Knots, Root.Seed, Grown < 1.0f, GroundSides);
	}

	// The leg roots grow up from the ground: their path is taken from the bones as they stand now
	TArray<TArray<FVector>> LegPaths;
	LegPaths.SetNum(LegRoots.Num());
	for (int32 Index = 0; Index < LegRoots.Num(); ++Index)
	{
		const FLegRoot& Leg = LegRoots[Index];
		LegPath(Body, Leg, LegPaths[Index]);
		const float Grown = Own(Leg.GrowStart, Leg.GrowEnd);
		if (Grown <= 0.0f)
		{
			continue;
		}
		Slice(LegPaths[Index], 0.0f, FMath::SmoothStep(0.0f, 1.0f, Grown), Path, Shares);
		AddTube(Vertices, Normals, UVs, Tangents, Triangles, Path, Shares, Leg.Radius * FMath::Lerp(0.5f, 1.0f, Grown), 0.6f, 0.35f, 5.0f, Leg.Seed, Grown < 1.0f, LegSides);
	}
	Mesh->CreateMeshSection_LinearColor(0, Vertices, Triangles, Normals, UVs, TArray<FLinearColor>(), Tangents, false);

	// The pulses: short glowing pieces a little thicker than their root, running from the ground up
	TArray<FVector> PulseVertices, PulseNormals;
	TArray<FVector2D> PulseUVs;
	TArray<FProcMeshTangent> PulseTangents;
	TArray<int32> PulseTriangles;
	for (const FPulse& Pulse : Pulses)
	{
		if (Pulse.Along <= 0.0f)
		{
			continue;
		}
		const float Head = FMath::Min(Pulse.Along, 1.0f);
		const float Tail = FMath::Max(Pulse.Along - PulseLength, 0.0f);
		if (Pulse.Root != INDEX_NONE && Roots.IsValidIndex(Pulse.Root))
		{
			// Ground roots run from the leg into the ground: the pulse comes from their far end
			const FRootPath& Root = Roots[Pulse.Root];
			Slice(Root.Points, 1.0f - Head, 1.0f - Tail, Path, Shares);
			AddTube(PulseVertices, PulseNormals, PulseUVs, PulseTangents, PulseTriangles, Path, Shares, Root.Radius * 1.04f, Root.Flare, Root.Tip, Root.Knots, Root.Seed, false, 6);
		}
		else if (LegPaths.IsValidIndex(Pulse.Leg))
		{
			Slice(LegPaths[Pulse.Leg], Tail, Head, Path, Shares);
			AddTube(PulseVertices, PulseNormals, PulseUVs, PulseTangents, PulseTriangles, Path, Shares, LegRoots[Pulse.Leg].Radius * 1.12f, 0.6f, 0.35f, 5.0f, LegRoots[Pulse.Leg].Seed, false, 6);
		}
	}
	if (PulseVertices.IsEmpty())
	{
		Mesh->ClearMeshSection(1);
	}
	else
	{
		Mesh->CreateMeshSection_LinearColor(1, PulseVertices, PulseTriangles, PulseNormals, PulseUVs, TArray<FLinearColor>(), PulseTangents, false);
	}
}

void UVaelFootRootsComponent::PlaceMoss(float Growth)
{
	if (MossMesh == nullptr)
	{
		return;
	}
	// Moss comes last, once the roots stand; each tuft its own size, flat on the wood
	const float Size = FMath::Max(MossMesh->GetBoundingBox().GetSize().GetMax(), 1.0f);
	const float Grown = FMath::SmoothStep(0.55f, 1.0f, Growth);
	const int32 Count = FMath::Min(MossPlaces.Num(), UVaelFloromantSettings::Get()->RootMossTufts);
	while (MossTufts.Num() < Count)
	{
		UStaticMeshComponent* Tuft = NewObject<UStaticMeshComponent>(GetOwner());
		Tuft->SetupAttachment(this);
		Tuft->SetUsingAbsoluteLocation(true);
		Tuft->SetUsingAbsoluteRotation(true);
		Tuft->SetUsingAbsoluteScale(true);
		Tuft->SetStaticMesh(MossMesh);
		for (int32 Slot = 0; Slot < MossMesh->GetStaticMaterials().Num() && MossLook != nullptr; ++Slot)
		{
			Tuft->SetMaterial(Slot, MossLook);
		}
		Tuft->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Tuft->SetCanEverAffectNavigation(false);
		Tuft->SetCastShadow(false);
		Tuft->RegisterComponent();
		MossTufts.Add(Tuft);
	}
	for (int32 Index = 0; Index < MossTufts.Num(); ++Index)
	{
		UStaticMeshComponent* Tuft = MossTufts[Index];
		const bool bShown = Index < Count && Grown > 0.0f;
		Tuft->SetVisibility(bShown);
		if (bShown)
		{
			FTransform Place = MossPlaces[Index];
			const float Scale = Place.GetScale3D().X / Size * Grown;
			Place.SetScale3D(FVector(Scale * 1.2f, Scale * 1.2f, Scale));
			Tuft->SetWorldTransform(Place);
		}
	}
}
