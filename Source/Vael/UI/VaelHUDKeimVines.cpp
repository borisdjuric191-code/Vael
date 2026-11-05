// Copyright Epic Games, Inc. All Rights Reserved.

// The living vines round the frame of the Keimgeflecht (Boris 2026-11-05: "die Ranken und Äste sollen sich ein wenig bewegen
// und schlängeln im Rahmen und umher, damit es Spaß macht, dort Zeit zu verbringen"). Drawn like the ink pictures around them:
// a dark ink outline, a green body, a pale stroke of light along it, ink leaves (T_Keim_Rankenblatt) that nod, tendrils that
// curl and uncurl. When the page opens they grow in from where they are rooted. All in design pixels of 1920 x 1080.

#include "UI/VaelHUD.h"
#include "Engine/Canvas.h"
#include "Engine/Texture2D.h"

namespace
{
	struct FVaelVine
	{
		/** Where it runs, from its root to its tip */
		TArray<FVector2D> Path;
		/** Thickness at the root */
		float Width = 7.0f;
		/** Towards the edge of the screen: leaves turn that way so they stay off the page */
		FVector2D Outward = FVector2D(0.0f, -1.0f);
		/** How far it slithers to the side, and its own rhythm */
		float Amplitude = 5.0f;
		float Phase = 0.0f;
		/** A leaf every this many samples */
		int32 LeafEvery = 5;
	};

	/** A shoot hanging from the top edge, swinging like a pendulum */
	struct FVaelHanging
	{
		FVector2D Root;
		float Length = 80.0f;
		float Phase = 0.0f;
	};

	const TArray<FVaelVine>& Vines()
	{
		static const TArray<FVaelVine> List = []
		{
			TArray<FVaelVine> Out;
			auto Add = [&Out](std::initializer_list<FVector2D> Path, float Width, const FVector2D& Outward, float Amplitude, float Phase, int32 LeafEvery)
			{
				FVaelVine Vine;
				Vine.Path = Path;
				Vine.Width = Width;
				Vine.Outward = Outward;
				Vine.Amplitude = Amplitude;
				Vine.Phase = Phase;
				Vine.LeafEvery = LeafEvery;
				Out.Add(Vine);
			};
			// Along the top: left over the herbarium, right kept high above the page tabs
			Add({ { -20, 40 }, { 150, 66 }, { 330, 36 }, { 520, 60 }, { 700, 30 } }, 7.0f, { 0, -1 }, 6.0f, 0.0f, 5);
			Add({ { 1940, 26 }, { 1760, 12 }, { 1580, 20 }, { 1400, 10 }, { 1230, 24 } }, 6.0f, { 0, -1 }, 4.0f, 1.3f, 6);
			// Down the sides
			Add({ { 36, -20 }, { 58, 200 }, { 28, 420 }, { 60, 640 }, { 30, 860 }, { 52, 1100 } }, 8.0f, { -1, 0 }, 7.0f, 2.1f, 4);
			Add({ { 1894, -20 }, { 1868, 220 }, { 1900, 460 }, { 1872, 700 }, { 1902, 940 }, { 1882, 1100 } }, 8.0f, { 1, 0 }, 7.0f, 3.4f, 4);
			// Along the bottom, below the buttons
			Add({ { -20, 1058 }, { 200, 1040 }, { 420, 1066 }, { 640, 1048 } }, 7.0f, { 0, 1 }, 5.0f, 4.2f, 5);
			Add({ { 1940, 1052 }, { 1700, 1068 }, { 1480, 1044 }, { 1290, 1062 } }, 7.0f, { 0, 1 }, 5.0f, 5.0f, 5);
			// One creeping over the top edge of the parchment
			Add({ { 1880, 160 }, { 1760, 146 }, { 1640, 160 }, { 1530, 148 } }, 4.0f, { 0, -1 }, 3.0f, 0.7f, 4);
			return Out;
		}();
		return List;
	}

	const TArray<FVaelHanging>& Hangings()
	{
		static const TArray<FVaelHanging> List = { { { 70, 46 }, 380.0f, 0.0f }, { { 250, 52 }, 70.0f, 1.1f }, { { 430, 44 }, 54.0f, 2.3f },
			{ { 600, 46 }, 90.0f, 3.1f }, { { 1290, 30 }, 96.0f, 4.0f }, { { 1890, 40 }, 300.0f, 5.2f } };
		return List;
	}

	FVector2D CatmullRom(const TArray<FVector2D>& P, float T)
	{
		const float Scaled = FMath::Clamp(T, 0.0f, 1.0f) * (P.Num() - 1);
		const int32 I = FMath::Min(FMath::FloorToInt(Scaled), P.Num() - 2);
		const float U = Scaled - I;
		const FVector2D P0 = P[FMath::Max(I - 1, 0)], P1 = P[I], P2 = P[I + 1], P3 = P[FMath::Min(I + 2, P.Num() - 1)];
		return 0.5f * ((2.0f * P1) + (-P0 + P2) * U + (2.0f * P0 - 5.0f * P1 + 4.0f * P2 - P3) * U * U + (-P0 + 3.0f * P1 - 3.0f * P2 + P3) * U * U * U);
	}
}

void AVaelHUD::DrawKeimVines(float OX, float K, float Time)
{
	auto At = [&](const FVector2D& P) { return FVector2D(OX + P.X * K, P.Y * K); };
	const FLinearColor Ink(FColor(18, 11, 6));
	const FLinearColor Body(FColor(46, 66, 28));
	const FLinearColor Shine(FColor(118, 150, 70));
	UTexture2D* Leaf = GetKeimPicture(TEXT("T_Keim_Rankenblatt"));

	// Opening: the vines grow in over 1.6 s, each a little later than the one before
	const float Opened = Time - KeimOpenTime;

	// A stroke through points with a width per point: ink outline, body, a pale line of light on one side
	auto Stroke = [&](const TArray<FVector2D>& Points, const TArray<float>& Widths, bool bShine)
	{
		for (int32 Pass = 0; Pass < (bShine ? 3 : 2); ++Pass)
		{
			for (int32 I = 0; I < Points.Num(); ++I)
			{
				const float W = Pass == 0 ? Widths[I] + 3.2f : Pass == 1 ? Widths[I] : FMath::Max(Widths[I] * 0.22f, 0.8f);
				const FLinearColor Color = Pass == 0 ? Ink : Pass == 1 ? Body : Shine;
				FVector2D P = Points[I];
				if (Pass == 2 && I + 1 < Points.Num())
				{
					const FVector2D Dir = (Points[I + 1] - Points[I]).GetSafeNormal();
					P += FVector2D(Dir.Y, -Dir.X) * Widths[I] * 0.22f;
				}
				if (Pass < 2)
				{
					DrawDisc(At(P), W * 0.5f * K, Color);
				}
				if (I + 1 < Points.Num())
				{
					FVector2D Next = Points[I + 1];
					if (Pass == 2)
					{
						const FVector2D Dir = (Points[I + 1] - Points[I]).GetSafeNormal();
						Next += FVector2D(Dir.Y, -Dir.X) * Widths[I + 1] * 0.22f;
					}
					DrawSegment(At(P), At(Next), Color, W * K);
				}
			}
		}
	};

	// A tendril that rolls up at the end; Curl from 0 (straight) to about 2 turns
	auto Tendril = [&](const FVector2D& From, const FVector2D& Dir, float Size, float Curl, float Sign)
	{
		TArray<FVector2D> Points;
		TArray<float> Widths;
		FVector2D P = From;
		float Angle = FMath::Atan2(Dir.Y, Dir.X);
		const int32 Steps = 22;
		for (int32 I = 0; I <= Steps; ++I)
		{
			const float T = I / float(Steps);
			Points.Add(P);
			Widths.Add(FMath::Lerp(1.8f, 0.6f, T));
			Angle += Sign * Curl * 2.0f * UE_PI / Steps * (0.3f + 1.4f * T);
			P += FVector2D(FMath::Cos(Angle), FMath::Sin(Angle)) * Size / Steps * (1.2f - 0.7f * T);
		}
		Stroke(Points, Widths, false);
	};

	const TArray<FVaelVine>& List = Vines();
	for (int32 Index = 0; Index < List.Num(); ++Index)
	{
		const FVaelVine& Vine = List[Index];
		const float Grown = FMath::Clamp((Opened - Index * 0.12f) / 1.6f, 0.0f, 1.0f);
		const float Reach = 1.0f - FMath::Square(1.0f - Grown);
		if (Reach <= 0.01f)
		{
			continue;
		}
		const int32 Samples = 64;
		TArray<FVector2D> Points;
		TArray<float> Widths;
		TArray<FVector2D> Tangents;
		const int32 Shown = FMath::Max(2, FMath::CeilToInt(Samples * Reach));
		for (int32 I = 0; I <= Shown; ++I)
		{
			const float S = FMath::Min(I / float(Samples), Reach);
			const FVector2D Base = CatmullRom(Vine.Path, S);
			const FVector2D Ahead = CatmullRom(Vine.Path, FMath::Min(S + 0.01f, 1.0f));
			const FVector2D Behind = CatmullRom(Vine.Path, FMath::Max(S - 0.01f, 0.0f));
			const FVector2D Tangent = (Ahead - Behind).GetSafeNormal();
			const FVector2D Normal(-Tangent.Y, Tangent.X);
			// A slow wave runs along it: it slithers, more towards its tip
			const float Wave = FMath::Sin(S * 9.0f - Time * 0.55f + Vine.Phase) + 0.4f * FMath::Sin(S * 23.0f + Time * 0.9f + Vine.Phase * 2.0f);
			Points.Add(Base + Normal * Wave * Vine.Amplitude * (0.35f + 0.65f * S));
			Tangents.Add(Tangent);
			// Thick at the root, thin at the tip, and thinner where it is still growing
			Widths.Add(FMath::Max(Vine.Width * (1.0f - 0.75f * S) * FMath::Lerp(0.5f, 1.0f, Grown), 1.2f));
		}
		Stroke(Points, Widths, true);

		// Leaves on the outer side, nodding in the slither; small and opening while it grows
		for (int32 I = Vine.LeafEvery; I < Points.Num() - 1; I += Vine.LeafEvery)
		{
			const FVector2D Tangent = Tangents[I];
			FVector2D Normal(-Tangent.Y, Tangent.X);
			if (FVector2D::DotProduct(Normal, Vine.Outward) < 0.0f)
			{
				Normal = -Normal;
			}
			const float Side = (I / Vine.LeafEvery) % 3 == 2 ? -0.6f : 1.0f;
			const FVector2D Out = (Tangent * 0.55f + Normal * Side).GetSafeNormal();
			const float Nod = 14.0f * FMath::Sin(Time * 1.3f + I * 0.7f + Vine.Phase);
			const float Angle = FMath::RadiansToDegrees(FMath::Atan2(Out.Y, Out.X)) + Nod;
			const float Size = (Side > 0.0f ? 1.0f : 0.7f) * FMath::Clamp((Reach * Points.Num() - I) / 6.0f, 0.0f, 1.0f) * (0.85f + 0.3f * FMath::Frac(I * 0.618f));
			if (Leaf != nullptr && Size > 0.05f)
			{
				const FVector2D Root = Points[I];
				const float W = 46.0f * Size;
				const float H = 23.0f * Size;
				const FLinearColor Tint = FMath::Frac(I * 0.37f + Index * 0.2f) < 0.4f ? FLinearColor(0.5f, 0.56f, 0.42f) : FLinearColor(0.74f, 0.8f, 0.64f);
				Canvas->K2_DrawTexture(Leaf, At(FVector2D(Root.X, Root.Y - H * 0.5f)), FVector2D(W * K, H * K), FVector2D::ZeroVector, FVector2D::UnitVector,
					Tint, BLEND_Translucent, Angle, FVector2D(0.0f, 0.5f));
			}
			// Now and then a tendril curling and uncurling
			if ((I / Vine.LeafEvery) % 4 == 1 && Grown > 0.9f)
			{
				const float Curl = 1.0f + 0.6f * FMath::Sin(Time * 0.45f + I + Vine.Phase);
				Tendril(Points[I], (Tangent * 0.4f - Normal).GetSafeNormal() * -1.0f, 34.0f, Curl, (I % 2) ? 1.0f : -1.0f);
			}
		}
		// The tip rolls up
		if (Grown > 0.95f && Points.Num() > 2)
		{
			const FVector2D Dir = (Points.Last() - Points[Points.Num() - 2]).GetSafeNormal();
			Tendril(Points.Last(), Dir, 30.0f, 1.3f + 0.5f * FMath::Sin(Time * 0.5f + Vine.Phase), Index % 2 ? 1.0f : -1.0f);
		}
	}

	// Shoots hanging from the top, swinging slowly, curling at the end
	for (const FVaelHanging& Hanging : Hangings())
	{
		const float Grown = FMath::Clamp((Opened - 0.6f) / 1.2f, 0.0f, 1.0f);
		if (Grown <= 0.01f)
		{
			continue;
		}
		const float Swing = FMath::DegreesToRadians(7.0f * FMath::Sin(Time * 0.7f + Hanging.Phase));
		TArray<FVector2D> Points;
		TArray<float> Widths;
		const int32 Steps = 18;
		const float Length = Hanging.Length * Grown;
		for (int32 I = 0; I <= Steps; ++I)
		{
			const float T = I / float(Steps);
			// The swing bends it more towards the tip, a small wobble rides along
			const float Angle = Swing * T * T + 0.05f * FMath::Sin(T * 7.0f + Time * 1.1f + Hanging.Phase);
			Points.Add(Hanging.Root + FVector2D(FMath::Sin(Angle), FMath::Cos(Angle)) * Length * T);
			Widths.Add(FMath::Lerp(3.2f, 1.0f, T));
		}
		Stroke(Points, Widths, false);
		if (Leaf != nullptr)
		{
			for (int32 I = 5; I < Steps; I += 6)
			{
				const float Side = (I / 6) % 2 ? 1.0f : -1.0f;
				const float Angle = 90.0f - Side * 55.0f + 10.0f * FMath::Sin(Time * 1.2f + I + Hanging.Phase);
				Canvas->K2_DrawTexture(Leaf, At(FVector2D(Points[I].X, Points[I].Y - 8.0f)), FVector2D(32.0f * K, 16.0f * K), FVector2D::ZeroVector,
					FVector2D::UnitVector, FLinearColor(0.8f, 0.85f, 0.7f), BLEND_Translucent, Angle, FVector2D(0.0f, 0.5f));
			}
		}
		if (Grown > 0.95f)
		{
			Tendril(Points.Last(), FVector2D(FMath::Sin(Swing), 1.0f).GetSafeNormal(), 22.0f, 1.2f + 0.4f * FMath::Sin(Time * 0.6f + Hanging.Phase),
				Hanging.Phase > 2.5f ? 1.0f : -1.0f);
		}
	}
}
