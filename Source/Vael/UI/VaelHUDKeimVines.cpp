// Copyright Epic Games, Inc. All Rights Reserved.

// The living roots round the frame of the Keimgeflecht (Boris 2026-11-05: "realistisch, düster, die Ranken holzig wie Wurzeln
// oder Äste, die sich schlängeln, wie ein Korb aus Wurzeln und ein Geflecht, und alles bewegt sich"). Each root is a band of
// real rendered bark (T_Keim_Ast, T_Keim_Wurzel from Tools/HUD/vael_keimgeflecht_spiel.py) laid along a slowly slithering
// path, with its shadow under it; a thinner root winds round each, by turns over and under, like wickerwork. Dark leaves nod,
// thin rootlets curl and uncurl, shoots hang from the top and swing. When the page opens they grow in. Design pixels 1920x1080.

#include "UI/VaelHUD.h"
#include "CanvasItem.h"
#include "Engine/Canvas.h"
#include "Engine/Texture2D.h"

namespace
{
	struct FVaelVine
	{
		/** Where it runs, from its root to its tip */
		TArray<FVector2D> Path;
		/** Thickness of the wood at the root */
		float Width = 20.0f;
		/** Towards the edge of the screen: leaves turn that way so they stay off the page */
		FVector2D Outward = FVector2D(0.0f, -1.0f);
		/** How far it slithers to the side, and its own rhythm */
		float Amplitude = 5.0f;
		float Phase = 0.0f;
		/** A leaf every this many samples (0: none) */
		int32 LeafEvery = 9;
		/** A thinner root winds round it */
		bool bWoven = true;
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
			auto Add = [&Out](std::initializer_list<FVector2D> Path, float Width, const FVector2D& Outward, float Amplitude, float Phase, int32 LeafEvery, bool bWoven)
			{
				FVaelVine Vine;
				Vine.Path = Path;
				Vine.Width = Width;
				Vine.Outward = Outward;
				Vine.Amplitude = Amplitude;
				Vine.Phase = Phase;
				Vine.LeafEvery = LeafEvery;
				Vine.bWoven = bWoven;
				Out.Add(Vine);
			};
			// The basket: thick roots down both sides and along top and bottom
			Add({ { 30, -40 }, { 54, 200 }, { 24, 420 }, { 58, 640 }, { 28, 860 }, { 48, 1120 } }, 40.0f, { -1, 0 }, 6.0f, 2.1f, 8, true);
			Add({ { 1896, -40 }, { 1870, 220 }, { 1902, 460 }, { 1874, 700 }, { 1904, 940 }, { 1884, 1120 } }, 40.0f, { 1, 0 }, 6.0f, 3.4f, 8, true);
			Add({ { -40, 34 }, { 150, 58 }, { 330, 28 }, { 520, 52 }, { 720, 22 } }, 32.0f, { 0, -1 }, 5.0f, 0.0f, 9, true);
			Add({ { 1960, 18 }, { 1760, 8 }, { 1580, 16 }, { 1400, 6 }, { 1210, 18 } }, 26.0f, { 0, -1 }, 3.0f, 1.3f, 0, true);
			Add({ { -40, 1062 }, { 200, 1044 }, { 420, 1068 }, { 660, 1050 } }, 32.0f, { 0, 1 }, 5.0f, 4.2f, 9, true);
			Add({ { 1960, 1056 }, { 1700, 1070 }, { 1480, 1046 }, { 1270, 1064 } }, 32.0f, { 0, 1 }, 5.0f, 5.0f, 9, true);
			// Crossing roots in the corners, so the frame reads as woven
			Add({ { -30, 260 }, { 40, 150 }, { 120, 60 }, { 230, -30 } }, 24.0f, { -1, -1 }, 4.0f, 0.6f, 0, false);
			Add({ { 1950, 300 }, { 1880, 170 }, { 1800, 70 }, { 1690, -30 } }, 24.0f, { 1, -1 }, 4.0f, 2.6f, 0, false);
			Add({ { -30, 800 }, { 50, 920 }, { 150, 1010 }, { 280, 1110 } }, 24.0f, { -1, 1 }, 4.0f, 3.9f, 0, false);
			Add({ { 1950, 780 }, { 1870, 900 }, { 1770, 1000 }, { 1640, 1110 } }, 24.0f, { 1, 1 }, 4.0f, 5.5f, 0, false);
			// A thin root creeping over the top edge of the parchment
			Add({ { 1880, 160 }, { 1760, 146 }, { 1640, 160 }, { 1530, 148 } }, 8.0f, { 0, -1 }, 3.0f, 0.7f, 0, false);
			return Out;
		}();
		return List;
	}

	const TArray<FVaelHanging>& Hangings()
	{
		static const TArray<FVaelHanging> List = { { { 70, 46 }, 380.0f, 0.0f }, { { 250, 48 }, 70.0f, 1.1f }, { { 430, 40 }, 54.0f, 2.3f },
			{ { 600, 42 }, 90.0f, 3.1f }, { { 1290, 26 }, 96.0f, 4.0f }, { { 1890, 40 }, 300.0f, 5.2f } };
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

	/** The wood in the bark bands fills the middle of their height; a band is drawn this much wider than the wood */
	constexpr float BandOverWood = 1.6f;
}

/** A band of bark along points (from First to Last), wood width per point; the light side of the band faces up of each
 *  point's normal. The texture runs along the band, UScale per design pixel. */
static void DrawBarkBand(UCanvas* Canvas, float OX, float K, UTexture2D* Texture, const TArray<FVector2D>& Points, const TArray<float>& Widths,
	int32 First, int32 Last, float UScale, const FLinearColor& Tint, const FVector2D& Offset)
{
	if (Canvas == nullptr || Texture == nullptr || Texture->GetResource() == nullptr || Last - First < 1)
	{
		return;
	}
	auto At = [&](const FVector2D& P) { return FVector2D(OX + P.X * K, P.Y * K); };
	TArray<FCanvasUVTri> Tris;
	float Along = 0.0f;
	for (int32 I = 0; I < First; ++I)
	{
		Along += FVector2D::Distance(Points[I], Points[I + 1]);
	}
	auto Edge = [&](int32 I, FVector2D& OutTop, FVector2D& OutBottom)
	{
		const FVector2D Tangent = (Points[FMath::Min(I + 1, Points.Num() - 1)] - Points[FMath::Max(I - 1, 0)]).GetSafeNormal();
		const FVector2D Up(Tangent.Y, -Tangent.X);
		const float Half = Widths[I] * BandOverWood * 0.5f;
		OutTop = At(Points[I] + Offset + Up * Half);
		OutBottom = At(Points[I] + Offset - Up * Half);
	};
	FVector2D Top0, Bottom0;
	Edge(First, Top0, Bottom0);
	float U0 = Along * UScale;
	for (int32 I = First; I < Last; ++I)
	{
		Along += FVector2D::Distance(Points[I], Points[I + 1]);
		const float U1 = Along * UScale;
		FVector2D Top1, Bottom1;
		Edge(I + 1, Top1, Bottom1);
		FCanvasUVTri A;
		A.V0_Pos = Top0; A.V0_UV = FVector2D(U0, 0.0f); A.V0_Color = Tint;
		A.V1_Pos = Bottom0; A.V1_UV = FVector2D(U0, 1.0f); A.V1_Color = Tint;
		A.V2_Pos = Top1; A.V2_UV = FVector2D(U1, 0.0f); A.V2_Color = Tint;
		FCanvasUVTri B;
		B.V0_Pos = Bottom0; B.V0_UV = FVector2D(U0, 1.0f); B.V0_Color = Tint;
		B.V1_Pos = Bottom1; B.V1_UV = FVector2D(U1, 1.0f); B.V1_Color = Tint;
		B.V2_Pos = Top1; B.V2_UV = FVector2D(U1, 0.0f); B.V2_Color = Tint;
		Tris.Add(A);
		Tris.Add(B);
		Top0 = Top1;
		Bottom0 = Bottom1;
		U0 = U1;
	}
	FCanvasTriangleItem Item(Tris, Texture->GetResource());
	Item.BlendMode = SE_BLEND_Translucent;
	Canvas->DrawItem(Item);
}

void AVaelHUD::DrawKeimVines(float OX, float K, float Time)
{
	auto At = [&](const FVector2D& P) { return FVector2D(OX + P.X * K, P.Y * K); };
	UTexture2D* Bark = GetKeimPicture(TEXT("T_Keim_Ast"));
	UTexture2D* Thin = GetKeimPicture(TEXT("T_Keim_Wurzel"));
	UTexture2D* Leaf = GetKeimPicture(TEXT("T_Keim_Rankenblatt"));
	const float Opened = Time - KeimOpenTime;

	auto Band = [&](UTexture2D* Texture, const TArray<FVector2D>& Points, const TArray<float>& Widths, int32 First, int32 Last,
		float UScale, const FLinearColor& Tint, const FVector2D& Offset)
	{
		DrawBarkBand(Canvas, OX, K, Texture, Points, Widths, First, Last, UScale, Tint, Offset);
	};
	const FLinearColor ShadowTint(0.0f, 0.0f, 0.0f, 0.55f);
	const FVector2D ShadowOffset(5.0f, 8.0f);
	// One texture length of the bark band per this many design pixels (the band is 1024 long)
	const float BarkU = 1.0f / 1024.0f;

	// A thin rootlet that rolls up at the end; Curl from 0 (straight) to about 2 turns
	auto Rootlet = [&](const FVector2D& From, const FVector2D& Dir, float Size, float Curl, float Sign, float Width)
	{
		TArray<FVector2D> Points;
		TArray<float> Widths;
		FVector2D P = From;
		float Angle = FMath::Atan2(Dir.Y, Dir.X);
		const int32 Steps = 20;
		for (int32 I = 0; I <= Steps; ++I)
		{
			const float T = I / float(Steps);
			Points.Add(P);
			Widths.Add(FMath::Lerp(Width, Width * 0.3f, T));
			Angle += Sign * Curl * 2.0f * UE_PI / Steps * (0.3f + 1.4f * T);
			P += FVector2D(FMath::Cos(Angle), FMath::Sin(Angle)) * Size / Steps * (1.2f - 0.7f * T);
		}
		Band(Thin, Points, Widths, 0, Points.Num() - 1, BarkU * 3.0f, ShadowTint, ShadowOffset * 0.5f);
		Band(Thin, Points, Widths, 0, Points.Num() - 1, BarkU * 3.0f, FLinearColor(0.8f, 0.75f, 0.7f), FVector2D::ZeroVector);
	};

	const TArray<FVaelVine>& List = Vines();
	for (int32 Index = 0; Index < List.Num(); ++Index)
	{
		const FVaelVine& Vine = List[Index];
		const float Grown = FMath::Clamp((Opened - Index * 0.1f) / 1.8f, 0.0f, 1.0f);
		const float Reach = 1.0f - FMath::Square(1.0f - Grown);
		if (Reach <= 0.01f)
		{
			continue;
		}
		const int32 Samples = 72;
		TArray<FVector2D> Points, Tangents, Woven;
		TArray<float> Widths, WovenWidths;
		TArray<bool> WovenOver;
		const int32 Shown = FMath::Max(2, FMath::CeilToInt(Samples * Reach));
		for (int32 I = 0; I <= Shown; ++I)
		{
			const float S = FMath::Min(I / float(Samples), Reach);
			const FVector2D Base = CatmullRom(Vine.Path, S);
			const FVector2D Tangent = (CatmullRom(Vine.Path, FMath::Min(S + 0.01f, 1.0f)) - CatmullRom(Vine.Path, FMath::Max(S - 0.01f, 0.0f))).GetSafeNormal();
			const FVector2D Normal(-Tangent.Y, Tangent.X);
			// A slow wave runs along it: the wood slithers, more towards its tip
			const float Wave = FMath::Sin(S * 7.0f - Time * 0.45f + Vine.Phase) + 0.35f * FMath::Sin(S * 19.0f + Time * 0.8f + Vine.Phase * 2.0f);
			const FVector2D Point = Base + Normal * Wave * Vine.Amplitude * (0.35f + 0.65f * S);
			const float Width = FMath::Max(Vine.Width * (1.0f - 0.6f * S) * FMath::Lerp(0.55f, 1.0f, Grown), 3.0f);
			Points.Add(Point);
			Tangents.Add(Tangent);
			Widths.Add(Width);
			// The thinner root winds round it: across it and back, by turns over and under
			const float Turn = S * 2.0f * UE_PI * 9.0f + Vine.Phase + Time * 0.25f;
			Woven.Add(Point + Normal * FMath::Sin(Turn) * Width * 0.55f);
			WovenWidths.Add(Width * 0.36f);
			WovenOver.Add(FMath::Cos(Turn) > 0.0f);
		}
		const int32 Last = Points.Num() - 1;
		// Shadows first, then the woven root where it runs behind, the wood, the woven root where it runs over
		Band(Bark, Points, Widths, 0, Last, BarkU, ShadowTint, ShadowOffset);
		if (Vine.bWoven)
		{
			Band(Thin, Woven, WovenWidths, 0, Last, BarkU * 2.0f, FLinearColor(0.55f, 0.5f, 0.45f), FVector2D::ZeroVector);
		}
		Band(Bark, Points, Widths, 0, Last, BarkU, FLinearColor::White, FVector2D::ZeroVector);
		if (Vine.bWoven)
		{
			for (int32 I = 0; I < Last;)
			{
				if (!WovenOver[I])
				{
					++I;
					continue;
				}
				int32 End = I;
				while (End < Last && WovenOver[End])
				{
					++End;
				}
				Band(Thin, Woven, WovenWidths, I, FMath::Min(End, Last), BarkU * 2.0f, ShadowTint, ShadowOffset * 0.4f);
				Band(Thin, Woven, WovenWidths, I, FMath::Min(End, Last), BarkU * 2.0f, FLinearColor(0.85f, 0.8f, 0.72f), FVector2D::ZeroVector);
				I = End + 1;
			}
		}

		// Dark leaves on the outer side, nodding in the slither, opening while it grows
		if (Vine.LeafEvery > 0 && Leaf != nullptr)
		{
			for (int32 I = Vine.LeafEvery; I < Last; I += Vine.LeafEvery)
			{
				const FVector2D Tangent = Tangents[I];
				FVector2D Normal(-Tangent.Y, Tangent.X);
				if (FVector2D::DotProduct(Normal, Vine.Outward) < 0.0f)
				{
					Normal = -Normal;
				}
				const float Side = (I / Vine.LeafEvery) % 3 == 2 ? -0.5f : 1.0f;
				const FVector2D Out = (Tangent * 0.6f + Normal * Side).GetSafeNormal();
				const float Nod = 12.0f * FMath::Sin(Time * 1.1f + I * 0.7f + Vine.Phase);
				const float Angle = FMath::RadiansToDegrees(FMath::Atan2(Out.Y, Out.X)) + Nod;
				const float Size = FMath::Clamp((Reach * Samples - I) / 6.0f, 0.0f, 1.0f) * (0.8f + 0.35f * FMath::Frac(I * 0.618f));
				if (Size > 0.05f)
				{
					const FVector2D Root = Points[I] + Normal * Widths[I] * 0.3f;
					const float W = 52.0f * Size;
					const float H = 26.0f * Size;
					// shadow, then the leaf
					Canvas->K2_DrawTexture(Leaf, At(FVector2D(Root.X + 4.0f, Root.Y + 6.0f - H * 0.5f)), FVector2D(W * K, H * K), FVector2D::ZeroVector,
						FVector2D::UnitVector, FLinearColor(0.0f, 0.0f, 0.0f, 0.45f), BLEND_Translucent, Angle, FVector2D(0.0f, 0.5f));
					const FLinearColor Tint = FMath::Frac(I * 0.37f + Index * 0.2f) < 0.4f ? FLinearColor(0.7f, 0.72f, 0.62f) : FLinearColor::White;
					Canvas->K2_DrawTexture(Leaf, At(FVector2D(Root.X, Root.Y - H * 0.5f)), FVector2D(W * K, H * K), FVector2D::ZeroVector, FVector2D::UnitVector,
						Tint, BLEND_Translucent, Angle, FVector2D(0.0f, 0.5f));
				}
				// Now and then a rootlet curling and uncurling
				if ((I / Vine.LeafEvery) % 3 == 1 && Grown > 0.9f)
				{
					const float Curl = 0.9f + 0.6f * FMath::Sin(Time * 0.4f + I + Vine.Phase);
					Rootlet(Points[I], (Normal - Tangent * 0.3f).GetSafeNormal(), 40.0f, Curl, (I % 2) ? 1.0f : -1.0f, 4.0f);
				}
			}
		}
		// The tip rolls up
		if (Grown > 0.95f && Points.Num() > 2)
		{
			const FVector2D Dir = (Points.Last() - Points[Points.Num() - 2]).GetSafeNormal();
			Rootlet(Points.Last(), Dir, 36.0f, 1.2f + 0.5f * FMath::Sin(Time * 0.45f + Vine.Phase), Index % 2 ? 1.0f : -1.0f, Widths.Last());
		}
	}

	// Roots hanging from the top, swinging slowly, curling at the end
	for (const FVaelHanging& Hanging : Hangings())
	{
		const float Grown = FMath::Clamp((Opened - 0.6f) / 1.4f, 0.0f, 1.0f);
		if (Grown <= 0.01f)
		{
			continue;
		}
		const float Swing = FMath::DegreesToRadians(6.0f * FMath::Sin(Time * 0.6f + Hanging.Phase));
		TArray<FVector2D> Points;
		TArray<float> Widths;
		const int32 Steps = 22;
		const float Length = Hanging.Length * Grown;
		for (int32 I = 0; I <= Steps; ++I)
		{
			const float T = I / float(Steps);
			const float Angle = Swing * T * T + 0.05f * FMath::Sin(T * 7.0f + Time * 0.9f + Hanging.Phase);
			Points.Add(Hanging.Root + FVector2D(FMath::Sin(Angle), FMath::Cos(Angle)) * Length * T);
			Widths.Add(FMath::Lerp(7.0f, 2.0f, T));
		}
		Band(Thin, Points, Widths, 0, Steps, BarkU * 2.5f, ShadowTint, ShadowOffset * 0.6f);
		Band(Thin, Points, Widths, 0, Steps, BarkU * 2.5f, FLinearColor(0.85f, 0.8f, 0.72f), FVector2D::ZeroVector);
		if (Grown > 0.95f)
		{
			Rootlet(Points.Last(), FVector2D(FMath::Sin(Swing), 1.0f).GetSafeNormal(), 24.0f, 1.1f + 0.4f * FMath::Sin(Time * 0.5f + Hanging.Phase),
				Hanging.Phase > 2.5f ? 1.0f : -1.0f, 2.2f);
		}
	}

	// Roots crawling through the frame like snakes between leaves and wood: a body slides along the woven band, the bark
	// travelling with it, its head nosing forward, its tail thinning out; when it has gone through, it comes again
	struct FCrawler { TArray<FVector2D> Path; float Width; float Body; float Seconds; float Phase; };
	static const TArray<FCrawler> Crawlers =
	{
		{ { { 36, -60 }, { 50, 300 }, { 24, 700 }, { 46, 1140 } }, 30.0f, 0.32f, 34.0f, 0.0f },
		{ { { 1892, 1140 }, { 1878, 760 }, { 1904, 380 }, { 1886, -60 } }, 30.0f, 0.3f, 38.0f, 0.4f },
		{ { { -60, 32 }, { 400, 22 }, { 800, 40 }, { 1200, 18 }, { 1980, 30 } }, 24.0f, 0.18f, 46.0f, 0.15f },
		{ { { 1980, 1062 }, { 1300, 1074 }, { 640, 1056 }, { -60, 1066 } }, 24.0f, 0.18f, 50.0f, 0.65f },
		{ { { -40, 420 }, { 30, 330 }, { 52, 200 }, { 20, 90 }, { 90, -40 } }, 16.0f, 0.5f, 22.0f, 0.8f },
	};
	for (const FCrawler& Crawler : Crawlers)
	{
		const float Grown = FMath::Clamp((Opened - 0.8f) / 1.0f, 0.0f, 1.0f);
		if (Grown <= 0.0f)
		{
			continue;
		}
		const float Head = FMath::Frac(Time / Crawler.Seconds + Crawler.Phase) * (1.0f + Crawler.Body);
		const float Tail = Head - Crawler.Body;
		TArray<FVector2D> Points;
		TArray<float> Widths;
		const int32 Steps = 40;
		for (int32 I = 0; I <= Steps; ++I)
		{
			const float B = I / float(Steps);
			const float S = FMath::Lerp(Tail, Head, B);
			if (S < 0.0f || S > 1.0f)
			{
				continue;
			}
			const FVector2D Base = CatmullRom(Crawler.Path, S);
			const FVector2D Tangent = (CatmullRom(Crawler.Path, FMath::Min(S + 0.005f, 1.0f)) - CatmullRom(Crawler.Path, FMath::Max(S - 0.005f, 0.0f))).GetSafeNormal();
			// The sway of a snake: a wave that runs backwards along the body as it moves forward
			const float Sway = FMath::Sin(B * 3.0f * 2.0f * UE_PI - Time * 2.2f + Crawler.Phase * 9.0f) * Crawler.Width * 0.7f;
			Points.Add(Base + FVector2D(-Tangent.Y, Tangent.X) * Sway);
			// Thin tail, full body, rounded head
			Widths.Add(Crawler.Width * Grown * FMath::Min(FMath::SmoothStep(0.0f, 0.45f, B), FMath::SmoothStep(1.0f, 0.88f, B) * 0.6f + 0.4f));
		}
		if (Points.Num() > 2)
		{
			Band(Bark, Points, Widths, 0, Points.Num() - 1, BarkU * 1.5f, ShadowTint, ShadowOffset);
			Band(Bark, Points, Widths, 0, Points.Num() - 1, BarkU * 1.5f, FLinearColor(0.9f, 0.85f, 0.78f), FVector2D::ZeroVector);
		}
	}

	// Shoots that look in from the frame and wave: they reach slowly into the room, sway, draw back a little, reach again
	struct FPeeker { FVector2D Root; FVector2D Dir; float Length; float Phase; };
	static const TArray<FPeeker> Peekers =
	{
		{ { 650, 34 }, { 0.15f, 1.0f }, 80.0f, 0.0f },
		{ { 1180, 26 }, { -0.1f, 1.0f }, 100.0f, 1.7f },
		{ { 50, 520 }, { 1.0f, 0.2f }, 60.0f, 3.1f },
		{ { 1894, 610 }, { -1.0f, -0.15f }, 55.0f, 4.4f },
		{ { 560, 1052 }, { 0.2f, -1.0f }, 64.0f, 2.2f },
		{ { 1520, 1050 }, { -0.25f, -1.0f }, 46.0f, 5.3f },
		{ { 960, 1062 }, { 0.0f, -1.0f }, 26.0f, 0.9f },
	};
	for (const FPeeker& Peeker : Peekers)
	{
		const float Grown = FMath::Clamp((Opened - 1.2f) / 1.2f, 0.0f, 1.0f);
		if (Grown <= 0.0f)
		{
			continue;
		}
		const float Reach = Grown * (0.55f + 0.45f * FMath::SmoothStep(-1.0f, 1.0f, FMath::Sin(Time * 0.22f + Peeker.Phase)));
		const FVector2D Dir = Peeker.Dir.GetSafeNormal();
		TArray<FVector2D> Points;
		TArray<float> Widths;
		const int32 Steps = 18;
		FVector2D P = Peeker.Root;
		float Angle = FMath::Atan2(Dir.Y, Dir.X);
		for (int32 I = 0; I <= Steps; ++I)
		{
			const float T = I / float(Steps);
			Points.Add(P);
			Widths.Add(FMath::Lerp(7.0f, 2.0f, T));
			// waving: the bend grows to the tip and swings to both sides
			Angle += FMath::DegreesToRadians(5.0f * FMath::Sin(Time * 0.9f + Peeker.Phase + T * 2.0f)) * T;
			P += FVector2D(FMath::Cos(Angle), FMath::Sin(Angle)) * Peeker.Length * Reach / Steps;
		}
		Band(Thin, Points, Widths, 0, Steps, BarkU * 2.5f, ShadowTint, ShadowOffset * 0.6f);
		Band(Thin, Points, Widths, 0, Steps, BarkU * 2.5f, FLinearColor(0.85f, 0.8f, 0.72f), FVector2D::ZeroVector);
		if (Leaf != nullptr)
		{
			const int32 I = Steps * 2 / 3;
			const float LeafAngle = FMath::RadiansToDegrees(FMath::Atan2(Dir.Y, Dir.X)) + 50.0f + 15.0f * FMath::Sin(Time * 1.2f + Peeker.Phase);
			Canvas->K2_DrawTexture(Leaf, At(FVector2D(Points[I].X, Points[I].Y - 9.0f)), FVector2D(36.0f * K, 18.0f * K), FVector2D::ZeroVector,
				FVector2D::UnitVector, FLinearColor(0.75f, 0.78f, 0.68f), BLEND_Translucent, LeafAngle, FVector2D(0.0f, 0.5f));
		}
		Rootlet(Points.Last(), (Points.Last() - Points[Steps - 1]).GetSafeNormal(), 18.0f, 1.0f + 0.5f * FMath::Sin(Time * 0.5f + Peeker.Phase),
			Peeker.Phase > 2.5f ? 1.0f : -1.0f, 2.0f);
	}
}

void AVaelHUD::DrawKeimStrand(float OX, float K, float Time)
{
	// Same place as the picture (Tools/HUD/vael_keimgeflecht_spiel.py) and VaelHUDKeimgeflecht.cpp
	constexpr float CX = 790.0f, Top = 214.0f, Bottom = 972.0f, Amp = 46.0f;
	auto At = [&](const FVector2D& P) { return FVector2D(OX + P.X * K, P.Y * K); };
	UTexture2D* LightWood = GetKeimPicture(TEXT("T_Keim_Lichtholz"));
	UTexture2D* ShadowRoot = GetKeimPicture(TEXT("T_Keim_Schattenwurzel"));
	UTexture2D* Leaf = GetKeimPicture(TEXT("T_Keim_Rankenblatt"));
	const float Spin = Time * KeimStrandSpin;
	const int32 Steps = 150;
	TArray<FVector2D> A, B;
	TArray<float> DepthA, DepthB, WidthA, WidthB;
	for (int32 I = 0; I <= Steps; ++I)
	{
		const float T = I / float(Steps);
		const float Y = Top + (Bottom - Top) * T;
		const float Phase = T * 4.4f * UE_PI + Spin;
		A.Add(FVector2D(CX + Amp * FMath::Sin(Phase) + 3.0f * FMath::Sin(T * 37.0f), Y));
		B.Add(FVector2D(CX + Amp * FMath::Sin(Phase + UE_PI) + 3.0f * FMath::Sin(T * 37.0f + UE_PI), Y));
		DepthA.Add(FMath::Cos(Phase));
		DepthB.Add(FMath::Cos(Phase + UE_PI));
		// the strand nearer to us is a little thicker
		WidthA.Add(18.0f * (0.86f + 0.14f * DepthA.Last()));
		WidthB.Add(16.0f * (0.86f + 0.14f * DepthB.Last()));
	}
	const float BarkU = 1.0f / 1024.0f;
	// Draw the runs of a strand that lie behind (bFront false) or in front, darker behind
	auto Runs = [&](UTexture2D* Texture, const TArray<FVector2D>& Points, const TArray<float>& Depths, const TArray<float>& Widths, bool bFront)
	{
		int32 Start = INDEX_NONE;
		for (int32 I = 0; I <= Steps; ++I)
		{
			const bool bIn = (Depths[I] >= 0.0f) == bFront;
			if (bIn && Start == INDEX_NONE)
			{
				Start = FMath::Max(I - 1, 0);
			}
			if ((!bIn || I == Steps) && Start != INDEX_NONE)
			{
				const int32 End = FMath::Min(I, Steps);
				const float Shade = bFront ? 1.0f : 0.55f;
				DrawBarkBand(Canvas, OX, K, Texture, Points, Widths, Start, End, BarkU * 2.0f, FLinearColor(0.0f, 0.0f, 0.0f, 0.45f), FVector2D(4.0f, 6.0f));
				DrawBarkBand(Canvas, OX, K, Texture, Points, Widths, Start, End, BarkU * 2.0f, FLinearColor(Shade, Shade, Shade), FVector2D::ZeroVector);
				Start = INDEX_NONE;
			}
		}
	};
	Runs(LightWood, A, DepthA, WidthA, false);
	Runs(ShadowRoot, B, DepthB, WidthB, false);

	// The rungs: even dotted lines from strand to strand, every dot the same size and spacing
	for (int32 I = 2; I < Steps - 1; I += 5)
	{
		const FVector2D From = A[I], To = B[I];
		if (FMath::Abs(To.X - From.X) < 20.0f)
		{
			continue;
		}
		const FVector2D Dir = (To - From).GetSafeNormal();
		const FVector2D Start = From + Dir * WidthA[I] * 0.5f;
		const FVector2D End = To - Dir * WidthB[I] * 0.5f;
		const int32 Dots = FMath::Max(1, FMath::RoundToInt(FVector2D::Distance(Start, End) / 8.0f));
		const float Near = 0.6f + 0.4f * FMath::Abs(DepthA[I]);
		for (int32 D = 0; D <= Dots; ++D)
		{
			const FVector2D P = FMath::Lerp(Start, End, D / float(Dots));
			DrawDisc(At(P + FVector2D(1.5f, 2.0f)), 2.0f * K, FLinearColor(0.0f, 0.0f, 0.0f, 0.4f));
			DrawDisc(At(P), 2.0f * K, FLinearColor(FColor(196, 236, 170)) * Near);
		}
	}

	Runs(LightWood, A, DepthA, WidthA, true);
	Runs(ShadowRoot, B, DepthB, WidthB, true);

	// Leaves on the light strand where it is in front, turning with it
	if (Leaf != nullptr)
	{
		for (int32 I = 6; I < Steps; I += 11)
		{
			if (DepthA[I] < 0.15f)
			{
				continue;
			}
			const float Side = (I / 11) % 2 ? 1.0f : -1.0f;
			const float Angle = (Side > 0.0f ? -25.0f : 205.0f) + 10.0f * FMath::Sin(Time * 1.1f + I);
			const float Size = 0.6f + 0.4f * DepthA[I];
			Canvas->K2_DrawTexture(Leaf, At(FVector2D(A[I].X, A[I].Y - 9.0f * Size)), FVector2D(38.0f * Size * K, 18.0f * Size * K), FVector2D::ZeroVector,
				FVector2D::UnitVector, FLinearColor(0.85f, 0.95f, 0.8f), BLEND_Translucent, Angle, FVector2D(0.0f, 0.5f));
		}
	}
}
