// Copyright Epic Games, Inc. All Rights Reserved.

// The Keimgeflecht page of the Floromant (Boris 2026-11-04/05, mockup D): a living root chamber with her herbarium of glasses
// hanging on threads on the left, the glowing strand of her heredity in the middle (light and shadow vine braided, the wheel
// places as glass capsules or sealed buds) and a free sheet of parchment on the right with the plate of a plant, its values,
// the tree of its Erbanlagen and its crosses. The fixed picture is rendered by Tools/HUD/vael_keimgeflecht_spiel.py; the glasses,
// capsules, seeds and pictures are its cut-out pieces, drawn and moved here, in design pixels of 1920 x 1080. The places here
// and in the script belong together.

#include "UI/VaelHUD.h"
#include "Engine/Canvas.h"
#include "Engine/Texture2D.h"
#include "Floromant/VaelFloromantComponent.h"
#include "Floromant/VaelFloromantSettings.h"
#include "Floromant/VaelPlantArmData.h"
#include "Floromant/VaelSymbiosis.h"
#include "Player/VaelCharacter.h"
#include "Player/VaelPlayerController.h"
#include "VaelAssets.h"

#define LOCTEXT_NAMESPACE "VaelKeimgeflecht"

namespace
{
	FLinearColor KeimRgb(uint8 R, uint8 G, uint8 B, uint8 A = 255)
	{
		return FLinearColor(FColor(R, G, B, A));
	}

	const TCHAR* KeimFolder = TEXT("/Game/Vael/UI/Keimgeflecht");

	// Places of the picture (Tools/HUD/vael_keimgeflecht_spiel.py)
	constexpr float StrandX = 790.0f;
	constexpr float StrandTop = 214.0f;
	constexpr float StrandBottom = 972.0f;
	constexpr float StrandAmp = 46.0f;
	constexpr float SheetX = 990.0f;
	constexpr float SheetY = 150.0f;
	constexpr float GlassRX = 52.0f;
	constexpr float GlassRY = 64.0f;

	/** The id of a plant arm (DA_Pflanzenarm_<Id>) names its pictures T_Keim_Rund_<Id> and T_Keim_Oval_<Id> */
	FString PlantId(const UVaelPlantArmData* Plant)
	{
		FString Name = Plant != nullptr ? Plant->GetName() : FString();
		Name.RemoveFromStart(TEXT("DA_Pflanzenarm_"));
		return Name;
	}

	FText Roman(int32 Number)
	{
		static const TCHAR* Numerals[] = { TEXT("I"), TEXT("II"), TEXT("III"), TEXT("IV"), TEXT("V"), TEXT("VI"), TEXT("VII"), TEXT("VIII"),
			TEXT("IX"), TEXT("X"), TEXT("XI"), TEXT("XII"), TEXT("XIII"), TEXT("XIV"), TEXT("XV"), TEXT("XVI") };
		return FText::FromString(Number >= 1 && Number <= 16 ? Numerals[Number - 1] : *FString::FromInt(Number));
	}

	/** Centre of a glass in the herbarium (staggered like on a root wall, the middle column lower) */
	FVector2D GlassCentre(int32 Index)
	{
		const int32 Column = Index % 3;
		return FVector2D(170.0f + Column * 160.0f, 300.0f + (Index / 3) * 178.0f + (Column == 1 ? 40.0f : 0.0f));
	}

	/** Centre of a capsule on the strand, by turns left and right */
	FVector2D CapsuleCentre(int32 Place)
	{
		return FVector2D(StrandX + (Place % 2 == 0 ? -106.0f : 106.0f), StrandTop + 50.0f + Place * 92.0f);
	}

	/** A point on the light vine of the strand, T from 0 (top) to 1 (bottom) */
	FVector2D StrandPoint(float T)
	{
		const float Phase = T * 4.4f * UE_PI;
		return FVector2D(StrandX + StrandAmp * FMath::Sin(Phase) + 3.0f * FMath::Sin(T * 37.0f), StrandTop + (StrandBottom - StrandTop) * T);
	}

	const TCHAR* GeneSign(EVaelGeneEffect Effect)
	{
		switch (Effect)
		{
		case EVaelGeneEffect::FireSpeed:	return TEXT("T_Keim_Zeichen_Wirbel");
		case EVaelGeneEffect::SaftCost:		return TEXT("T_Keim_Zeichen_Tropfen");
		case EVaelGeneEffect::NidPrice:		return TEXT("T_Keim_Zeichen_Nid");
		default:							return TEXT("T_Keim_Zeichen_Dorn");
		}
	}

	/** Small hash for the fireflies: a stable number from 0 to 1 per index and salt */
	float Hash01(int32 Index, int32 Salt)
	{
		return FMath::Frac(FMath::Sin(Index * 12.9898f + Salt * 78.233f) * 43758.5453f);
	}
}

UTexture2D* AVaelHUD::GetKeimPicture(FName Name)
{
	if (const TObjectPtr<UTexture2D>* Found = KeimPictures.Find(Name))
	{
		return *Found;
	}
	const FString Path = FString::Printf(TEXT("%s/%s.%s"), KeimFolder, *Name.ToString(), *Name.ToString());
	UTexture2D* Picture = LoadObject<UTexture2D>(nullptr, *Path, nullptr, LOAD_NoWarn | LOAD_Quiet);
	KeimPictures.Add(Name, Picture);
	return Picture;
}

void AVaelHUD::DrawKeimgeflecht(const AVaelPlayerController* PlayerController)
{
	const AVaelCharacter* Player = PlayerController->GetPawn<AVaelCharacter>();
	const UVaelFloromantComponent* Floromant = Player != nullptr ? Player->GetFloromant() : nullptr;
	if (Floromant == nullptr)
	{
		return;
	}

	// Design pixels of 1920 x 1080, scaled to the height of the screen and centred
	const float K = Canvas->ClipY / 1080.0f;
	const float OX = (Canvas->ClipX - 1920.0f * K) * 0.5f;
	auto At = [&](float X, float Y) { return FVector2D(OX + X * K, Y * K); };
	auto AtV = [&](const FVector2D& P) { return At(P.X, P.Y); };
	// A picture in a box; turned by Angle degrees round the pivot (design pixels) if one is given
	auto Picture = [&](UTexture2D* Texture, float X, float Y, float W, float H, const FLinearColor& Tint, float Angle = 0.0f,
		const FVector2D& Pivot = FVector2D::ZeroVector, EBlendMode Blend = BLEND_Translucent)
	{
		if (Texture != nullptr && W > 0.0f && H > 0.0f)
		{
			const FVector2D PivotShare = Angle != 0.0f ? FVector2D((Pivot.X - X) / W, (Pivot.Y - Y) / H) : FVector2D(0.5f, 0.5f);
			Canvas->K2_DrawTexture(Texture, At(X, Y), FVector2D(W * K, H * K), FVector2D::ZeroVector, FVector2D::UnitVector, Tint, Blend, Angle, PivotShare);
		}
	};
	// A picture by its centre and size
	auto Centred = [&](UTexture2D* Texture, const FVector2D& Centre, float W, float H, const FLinearColor& Tint, EBlendMode Blend = BLEND_Translucent)
	{
		Picture(Texture, Centre.X - W * 0.5f, Centre.Y - H * 0.5f, W, H, Tint, 0.0f, FVector2D::ZeroVector, Blend);
	};
	UTexture2D* Glow = GetKeimPicture(TEXT("T_Keim_Schein"));
	auto Light = [&](const FVector2D& Centre, float Radius, const FLinearColor& Color)
	{
		Centred(Glow, Centre, Radius * 2.0f, Radius * 2.0f, Color, BLEND_Additive);
	};
	auto Label = [&](const FText& Text, float X, float Y, float Size, const FLinearColor& Color, float Align = 0.0f, float MaxWidth = 0.0f)
	{
		DrawLabel(Text, OX + X * K, Y * K, Size * K, Color, Align, MaxWidth * K);
	};
	auto Title = [&](const FText& Text, float X, float Y, float Size, const FLinearColor& Color, float Align = 0.0f, float MaxWidth = 0.0f)
	{
		DrawTitle(Text, OX + X * K, Y * K, Size * K, Color, Align, MaxWidth * K);
	};
	auto Frame = [&](float X0, float Y0, float X1, float Y1, const FLinearColor& Color, float Thickness)
	{
		DrawFrame(OX + X0 * K, Y0 * K, (X1 - X0) * K, (Y1 - Y0) * K, Color, Thickness * K);
	};

	const FLinearColor Ink = KeimRgb(42, 27, 16);
	const FLinearColor InkDim = KeimRgb(42, 27, 16, 140);
	const FLinearColor Pale = KeimRgb(226, 210, 174);
	const FLinearColor Dim = KeimRgb(150, 140, 120);
	const FLinearColor Gold = KeimRgb(232, 178, 82);
	const FLinearColor Sap = KeimRgb(140, 236, 150);
	const FLinearColor Wax = KeimRgb(120, 16, 18);
	const float Time = GetWorld() != nullptr ? GetWorld()->GetRealTimeSeconds() : 0.0f;
	const float Pulse = 0.5f + 0.5f * FMath::Sin(Time * 5.0f);
	const EVaelInputGlyphs Glyphs = PlayerController->GetInputGlyphs();
	const FVaelKeimCursor& Cursor = PlayerController->GetKeimCursor();

	// The chamber: black around it where the screen is wider than 16:9
	DrawBox(0.0f, 0.0f, Canvas->ClipX, Canvas->ClipY, KeimRgb(8, 6, 5));
	Picture(GetKeimPicture(TEXT("T_Keim_Grund")), 0.0f, 0.0f, 1920.0f, 1080.0f, FLinearColor::White);
	DrawMenuTabs(PlayerController, OX + 1830.0f * K, 30.0f * K);

	// ---------------- Herbarium: glasses on threads that sway; the chosen one swings when it is chosen and glows warm
	const int32 NumCards = FMath::Min(Floromant->GetCatalogSize(), AVaelPlayerController::KeimCards);
	const int32 NumPlaces = Floromant->GetHandBeltSize();
	Label(FText::Format(LOCTEXT("Found", "{0} von {1} gefunden"), NumCards, AVaelPlayerController::KeimCards), 330.0f, 194.0f, 18.0f, Dim, 0.5f);
	const int32 Chosen = Cursor.Carried >= 0 ? Cursor.Carried : Cursor.Zone == EVaelKeimZone::Herbarium ? Cursor.Card : INDEX_NONE;
	if (Chosen != KeimSwingCard)
	{
		KeimSwingCard = Chosen;
		KeimSwingTime = Time;
	}
	for (int32 Index = 0; Index < AVaelPlayerController::KeimCards; ++Index)
	{
		const UVaelPlantArmData* Plant = Index < NumCards ? Floromant->GetCatalogPlant(Index) : nullptr;
		const bool bChosen = Index == Chosen;
		const float Scale = bChosen ? 1.08f : 1.0f;
		const FVector2D Centre = GlassCentre(Index);
		const FVector2D Knob(Centre.X, Centre.Y - GlassRY - 14.0f);
		float Angle = 1.4f * FMath::Sin(Time * 0.9f + Index * 1.7f);
		if (bChosen)
		{
			const float Since = Time - KeimSwingTime;
			Angle += 7.0f * FMath::Exp(-2.2f * Since) * FMath::Sin(Since * 7.0f);
		}
		const float RX = GlassRX * Scale;
		const float RY = GlassRY * Scale;
		if (bChosen)
		{
			Light(Centre, 150.0f, KeimRgb(255, 200, 115) * 0.5f);
		}
		if (Plant != nullptr)
		{
			Picture(GetKeimPicture(*FString::Printf(TEXT("T_Keim_Oval_%s"), *PlantId(Plant))), Centre.X - RX, Centre.Y - RY, RX * 2.0f, RY * 2.0f,
				FLinearColor::White, Angle, Knob);
		}
		else
		{
			// Not found yet: a dark silhouette behind the glass
			Picture(GetKeimPicture(TEXT("T_Keim_Oval_Windhalm")), Centre.X - RX, Centre.Y - RY, RX * 2.0f, RY * 2.0f, FLinearColor(0.035f, 0.04f, 0.035f), Angle, Knob);
		}
		Picture(GetKeimPicture(bChosen ? TEXT("T_Keim_Glas_Gold") : TEXT("T_Keim_Glas")), Centre.X - 75.0f * Scale, Centre.Y - 105.0f * Scale,
			150.0f * Scale, 190.0f * Scale, FLinearColor::White, Angle, Knob);
		if (Plant == nullptr)
		{
			Title(FText::FromString(TEXT("?")), Centre.X, Centre.Y - 34.0f, 64.0f, KeimRgb(95, 100, 88), 0.5f);
			Label(LOCTEXT("Unknown", "unentdeckt"), Centre.X, Centre.Y + RY + 10.0f, 15.0f, Dim, 0.5f, 150.0f);
			continue;
		}
		Label(Plant->GetDisplayName(false), Centre.X, Centre.Y + RY + 10.0f, bChosen ? 17.0f : 15.0f, bChosen ? Gold : Pale, 0.5f, 150.0f);

		// In the wheel: a green germ glowing at the top of the glass
		bool bInWheel = false;
		for (int32 Place = 0; Place < NumPlaces; ++Place)
		{
			bInWheel |= Floromant->GetHandBeltPlant(Place) == Plant;
		}
		if (bInWheel)
		{
			const FVector2D Germ(Centre.X + RX - 4.0f, Centre.Y - RY + 8.0f);
			Light(Germ, 18.0f, Sap * (0.7f + 0.3f * FMath::Sin(Time * 2.0f + Index)));
			DrawDisc(AtV(Germ), 5.0f * K, Sap);
		}
		if (Cursor.Carried == Index)
		{
			DrawRing(AtV(Centre), (RY + 14.0f) * K, Sap * (0.6f + 0.4f * Pulse), 2.5f * K);
		}
	}

	// ---------------- The strand: sap runs up the light vine in glowing beads
	for (int32 Bead = 0; Bead < 7; ++Bead)
	{
		const float T = 1.0f - FMath::Frac(Time * 0.07f + Bead / 7.0f);
		const float Fade = FMath::SmoothStep(0.0f, 0.08f, T) * FMath::SmoothStep(1.0f, 0.9f, T);
		const FVector2D Point = StrandPoint(T);
		Light(Point, 26.0f, KeimRgb(130, 255, 150) * (0.9f * Fade));
		DrawDisc(AtV(Point), 3.5f * K, KeimRgb(220, 255, 220, uint8(255 * Fade)));
	}

	// The wheel places: capsules with the plant, empty glass, or a sealed bud not grown yet
	const bool bCarrying = Cursor.Carried >= 0;
	if (Cursor.Carried != KeimCarried)
	{
		KeimCarried = Cursor.Carried;
		KeimCarryTime = Time;
	}
	for (int32 Place = 0; Place < AVaelPlayerController::KeimCapsules; ++Place)
	{
		const FVector2D Centre = CapsuleCentre(Place);
		if (Place >= NumPlaces)
		{
			Centred(GetKeimPicture(TEXT("T_Keim_Knospe")), Centre, 110.0f, 110.0f, FLinearColor::White);
			continue;
		}
		const UVaelPlantArmData* Plant = Floromant->GetHandBeltPlant(Place);
		const bool bHere = Cursor.Zone == EVaelKeimZone::Wheel && Cursor.Capsule == Place;
		const UVaelPlantArmData* Shown = bHere && bCarrying ? Floromant->GetCatalogPlant(Cursor.Carried) : Plant;
		Light(Centre, 70.0f, KeimRgb(90, 230, 120) * (0.35f + 0.1f * FMath::Sin(Time * 1.3f + Place)));
		if (Shown != nullptr)
		{
			const bool bShadow = Shown == Plant && Floromant->IsWheelShadow(Place) && !Plant->IsLightOnly();
			UTexture2D* Round = GetKeimPicture(*FString::Printf(TEXT("T_Keim_Rund_%s%s"), *PlantId(Shown), bShadow ? TEXT("_Schatten") : TEXT("")));
			Centred(Round, Centre, 76.0f, 76.0f, FLinearColor(1.0f, 1.0f, 1.0f, Shown == Plant ? 1.0f : 0.55f + 0.3f * Pulse));
		}
		Centred(GetKeimPicture(TEXT("T_Keim_Kapsel")), Centre, 100.0f, 100.0f, FLinearColor::White);
		if (bHere)
		{
			DrawRing(AtV(Centre), 46.0f * K, bCarrying ? Sap * (0.6f + 0.4f * Pulse) : Gold, 3.0f * K);
		}
	}

	// A carried plant: a tendril grows from its glass towards the capsule the cursor is on (or towards the strand)
	if (bCarrying)
	{
		const FVector2D From = GlassCentre(Cursor.Carried) + FVector2D(GlassRX + 6.0f, 0.0f);
		const FVector2D To = Cursor.Zone == EVaelKeimZone::Wheel ? CapsuleCentre(Cursor.Capsule) + FVector2D(Cursor.Capsule % 2 == 0 ? -40.0f : 40.0f, 0.0f)
			: FVector2D(StrandX - 150.0f, From.Y);
		const FVector2D Bend((From.X + To.X) * 0.5f, FMath::Min(From.Y, To.Y) - 60.0f);
		const float Grown = FMath::Clamp((Time - KeimCarryTime) / 0.7f, 0.0f, 1.0f);
		const int32 Steps = 24;
		FVector2D Last = From;
		for (int32 Step = 1; Step <= FMath::CeilToInt(Steps * Grown); ++Step)
		{
			const float T = FMath::Min(Step / float(Steps), Grown);
			const FVector2D Point = From * FMath::Square(1.0f - T) + Bend * (2.0f * (1.0f - T) * T) + To * FMath::Square(T)
				+ FVector2D(0.0f, 4.0f * FMath::Sin(T * 18.0f + Time * 3.0f));
			DrawSegment(AtV(Last), AtV(Point), Sap * (0.45f + 0.35f * Pulse), FMath::Lerp(3.0f, 1.2f, T) * K);
			Last = Point;
		}
		Light(Last, 16.0f, Sap);
	}

	// Fireflies drift slowly through the chamber
	for (int32 Fly = 0; Fly < 16; ++Fly)
	{
		const FVector2D Home(120.0f + Hash01(Fly, 1) * 1680.0f, 170.0f + Hash01(Fly, 2) * 780.0f);
		const float Speed = 0.12f + Hash01(Fly, 3) * 0.15f;
		const FVector2D Point = Home + FVector2D(60.0f * FMath::Sin(Time * Speed + Fly), 35.0f * FMath::Sin(Time * Speed * 1.7f + Fly * 2.3f));
		const float Blink = FMath::Max(0.0f, FMath::Sin(Time * (0.6f + Hash01(Fly, 4)) + Fly * 4.1f));
		Light(Point, 16.0f + 8.0f * Hash01(Fly, 5), KeimRgb(255, 225, 130) * (0.25f + 0.75f * Blink));
	}

	// ---------------- The plate of the plant on the parchment
	const UVaelPlantArmData* Plant = Floromant->GetCatalogPlant(FMath::Clamp(Cursor.PlatePlant, 0, FMath::Max(Floromant->GetCatalogSize() - 1, 0)));
	if (Plant != nullptr)
	{
		const bool bShadowSide = Cursor.bShadowSide && !Plant->IsLightOnly();
		Label(FText::Format(LOCTEXT("Plate", "Tafel {0}"), Roman(Cursor.PlatePlant + 1)), SheetX + 56.0f, SheetY + 38.0f, 20.0f, InkDim);
		Title(Plant->GetDisplayName(bShadowSide), SheetX + 54.0f, SheetY + 60.0f, 62.0f, Ink, 0.0f, 420.0f);
		const FText Form = bShadowSide ? LOCTEXT("ShadowForm", "Schattenform") : Plant->IsLightOnly() ? LOCTEXT("LightOnly", "nur Lichtform") : LOCTEXT("LightForm", "Lichtform");
		Label(Plant->LatinName.IsEmpty() ? Form : FText::Format(LOCTEXT("LatinForm", "{0} · {1}"), Plant->LatinName, Form), SheetX + 58.0f, SheetY + 140.0f, 21.0f, InkDim);

		// The shadow form as a wax seal beside the name: sealed on the light path, chosen with the medallion before the vow,
		// open after the shadow vow
		const FVector2D Seal(SheetX + 520.0f, SheetY + 98.0f);
		if (!Plant->IsLightOnly())
		{
			const bool bSealed = Player->GetPath() == EVaelPath::Light;
			DrawDisc(AtV(Seal), 40.0f * K, Wax);
			Centred(GetKeimPicture(*FString::Printf(TEXT("T_Keim_Rund_%s_Schatten"), *PlantId(Plant))), Seal, 64.0f, 64.0f,
				bSealed ? FLinearColor(0.3f, 0.3f, 0.3f) : FLinearColor::White);
			DrawRing(AtV(Seal), 38.0f * K, Wax * 1.4f, 4.0f * K);
		}
		const FText ShadowState = Plant->IsLightOnly() ? LOCTEXT("ShadowNone", "keine Schattenform")
			: Player->GetPath() == EVaelPath::Light ? LOCTEXT("ShadowSealed", "Schatten versiegelt")
			: Player->GetPath() == EVaelPath::None ? LOCTEXT("ShadowBefore", "Schatten vor dem Schwur")
			: LOCTEXT("ShadowOpen", "Schatten offen, trinkt Nid");
		Label(ShadowState, Seal.X, Seal.Y + 46.0f, 14.0f, InkDim, 0.5f, 200.0f);

		// The miniature in its oval window, breathing a little
		const FVector2D Mini(SheetX + 210.0f, SheetY + 375.0f);
		const float Breath = 1.0f + 0.006f * FMath::Sin(Time * 1.1f);
		const FString MiniId = PlantId(Plant) + (bShadowSide ? TEXT("_Schatten") : TEXT(""));
		UTexture2D* MiniPicture = GetKeimPicture(*FString::Printf(TEXT("T_Keim_Oval_%s"), *MiniId));
		Centred(MiniPicture != nullptr ? MiniPicture : GetKeimPicture(*FString::Printf(TEXT("T_Keim_Oval_%s"), *PlantId(Plant))), Mini,
			300.0f * Breath, 370.0f * Breath, FLinearColor::White);

		// Fig. notes in ink with pointers into the miniature
		const FVector2D FigPoints[] = { Mini + FVector2D(10.0f, -120.0f), Mini + FVector2D(70.0f, 10.0f), Mini + FVector2D(-10.0f, 170.0f) };
		for (int32 Index = 0; Index < FMath::Min(Plant->Figures.Num(), 3); ++Index)
		{
			const FVector2D Note(Mini.X + 190.0f, Mini.Y - 140.0f + Index * 116.0f);
			const FVector2D Mid((FigPoints[Index].X + Note.X) * 0.5f, (FigPoints[Index].Y + Note.Y) * 0.5f + 12.0f);
			DrawSegment(AtV(FigPoints[Index]), AtV(Mid), InkDim, 1.2f * K);
			DrawSegment(AtV(Mid), AtV(Note + FVector2D(-8.0f, 12.0f)), InkDim, 1.2f * K);
			DrawDisc(AtV(FigPoints[Index]), 3.5f * K, Ink);
			Title(FText::Format(LOCTEXT("Fig", "Fig. {0}  {1}"), Index + 1, Plant->Figures[Index].Title), Note.X, Note.Y, 22.0f, Ink, 0.0f, 270.0f);
			Label(Plant->Figures[Index].Note, Note.X, Note.Y + 28.0f, 16.0f, InkDim, 0.0f, 270.0f);
		}

		// Values under the miniature, with her Erbanlagen in them; a thin green bar shows how much
		{
			const float Saft = Plant->SaftPerShot * Floromant->GetGeneScale(Plant, EVaelGeneEffect::SaftCost);
			const float Rate = 1.0f / FMath::Max(Plant->FireInterval * Floromant->GetGeneScale(Plant, EVaelGeneEffect::FireSpeed), 0.01f);
			const float Power = Floromant->GetGeneScale(Plant, EVaelGeneEffect::Damage);
			const float Nid = Plant->NidPerSecond * Floromant->GetGeneScale(Plant, EVaelGeneEffect::NidPrice) * UVaelFloromantSettings::Get()->NidPriceScale;
			FNumberFormattingOptions Two;
			Two.MinimumFractionalDigits = 1;
			Two.MaximumFractionalDigits = 2;
			const TPair<FText, float> Values[] =
			{
				{ FText::Format(LOCTEXT("PowerValue", "{0} %"), FText::AsNumber(FMath::RoundToInt(Power * 100.0f))), Power / 2.0f },
				{ FText::Format(LOCTEXT("RateValue", "{0} / s"), FText::AsNumber(Rate, &Two)), Rate / 12.0f },
				{ FText::AsNumber(Saft, &Two), Saft / 2.0f },
				{ Plant->IsLightOnly() ? LOCTEXT("NoNid", "keins") : FText::Format(LOCTEXT("NidValue", "{0} / s"), FText::AsNumber(Nid, &Two)), Plant->IsLightOnly() ? 0.0f : Nid / 0.5f },
			};
			for (int32 Index = 0; Index < UE_ARRAY_COUNT(Values); ++Index)
			{
				const float SX = SheetX + 64.0f + Index * 112.0f;
				const float SY = SheetY + 594.0f;
				Label(Values[Index].Key, SX + 34.0f, SY + 17.0f, 18.0f, Ink, 0.0f, 80.0f);
				const float Share = FMath::Clamp(Values[Index].Value, 0.02f, 1.0f);
				DrawSegment(At(SX, SY + 48.0f), At(SX + 92.0f * Share, SY + 48.0f), Index == 3 ? KeimRgb(140, 70, 190) : KeimRgb(64, 150, 64), 2.6f * K);
			}
		}

		// Erbgut: the tree, per tier two seeds on the branches; the taken one has bloomed
		const TArray<FVaelGeneTier>& Tiers = Plant->GetGeneTiers();
		const TArray<int32>& TierLevels = UVaelFloromantSettings::Get()->GeneTierLevels;
		const float TreeX = SheetX + 690.0f;
		const float TreeY = SheetY + 50.0f;
		for (int32 Tier = 0; Tier < FMath::Min(Tiers.Num(), 3); ++Tier)
		{
			const float ForkY = TreeY + 170.0f + Tier * 168.0f;
			const bool bOpen = Floromant->IsGeneTierOpen(Tier);
			if (!bOpen)
			{
				Label(FText::Format(LOCTEXT("TierFrom", "ab Stufe {0}"), TierLevels.IsValidIndex(Tier) ? TierLevels[Tier] : 99), TreeX, ForkY - 44.0f, 15.0f, InkDim, 0.5f);
			}
			const int32 Taken = Floromant->GetGene(Plant, Tier);
			const FString BloomKey = FString::Printf(TEXT("%s_%d"), *PlantId(Plant), Tier);
			const int32* Seen = KeimSeenGenes.Find(BloomKey);
			if (Seen == nullptr || *Seen != Taken)
			{
				// A new pick blooms; the first look at a plate shows its blossoms open already
				KeimBloomTimes.Add(BloomKey, Seen != nullptr && Taken >= 0 ? Time : -10.0f);
				KeimSeenGenes.Add(BloomKey, Taken);
			}
			const float Bloom = FMath::Clamp((Time - KeimBloomTimes.FindRef(BloomKey)) / 0.6f, 0.0f, 1.0f);
			const float BloomScale = 1.0f + 2.7f * FMath::Pow(Bloom - 1.0f, 3.0f) + 1.7f * FMath::Pow(Bloom - 1.0f, 2.0f);
			for (int32 Option = 0; Option < FMath::Min(Tiers[Tier].Options.Num(), 2); ++Option)
			{
				const FVaelGene& Gene = Tiers[Tier].Options[Option];
				const FVector2D Node(TreeX + (Option == 0 ? -92.0f : 92.0f), ForkY + 6.0f);
				UTexture2D* Sign = GetKeimPicture(GeneSign(Gene.Effect));
				if (Taken == Option)
				{
					Light(Node, 70.0f * BloomScale, KeimRgb(150, 255, 150) * (0.55f + 0.15f * FMath::Sin(Time * 2.0f)));
					Centred(GetKeimPicture(TEXT("T_Keim_Bluete")), Node, 90.0f * BloomScale, 90.0f * BloomScale, FLinearColor::White);
					Centred(Sign, Node, 30.0f * BloomScale, 30.0f * BloomScale, KeimRgb(255, 250, 230));
				}
				else
				{
					Centred(GetKeimPicture(bOpen ? TEXT("T_Keim_Same") : TEXT("T_Keim_Same_Zu")), Node, 80.0f, 80.0f, FLinearColor::White);
					Centred(Sign, Node, 26.0f, 26.0f, bOpen ? KeimRgb(225, 235, 200, 220) : KeimRgb(150, 140, 120, 160));
				}
				if (Cursor.Zone == EVaelKeimZone::Genes && Cursor.Tier == Tier && Cursor.Option == Option)
				{
					DrawRing(AtV(Node), 38.0f * K, Gold * (0.7f + 0.3f * Pulse), 2.4f * K);
				}
				Label(Gene.Name, Node.X, Node.Y + 38.0f, 17.0f, bOpen ? Ink : InkDim, 0.5f, 170.0f);
			}
		}

		// The chosen Erbanlage in words under the tree
		if (Cursor.Zone == EVaelKeimZone::Genes && Tiers.IsValidIndex(Cursor.Tier) && Tiers[Cursor.Tier].Options.IsValidIndex(Cursor.Option))
		{
			const FVaelGene& Gene = Tiers[Cursor.Tier].Options[Cursor.Option];
			Title(Gene.Name, TreeX, SheetY + 738.0f, 22.0f, Ink, 0.5f, 300.0f);
			Label(Gene.Description, TreeX, SheetY + 770.0f, 18.0f, KeimRgb(18, 60, 18), 0.5f, 320.0f);
		}

		// Crosses as double medallions: known ones with both pictures, the others as silhouettes
		struct FCross { FText Name; const UVaelPlantArmData* Other; bool bKnown; };
		TArray<FCross> Crosses;
		for (int32 Index = 0; Index < Floromant->GetCatalogSize(); ++Index)
		{
			const UVaelPlantArmData* Other = Floromant->GetCatalogPlant(Index);
			const EVaelSymbiosis Kind = Other != nullptr && Other != Plant ? VaelSymbiosis::Find(Plant, Other) : EVaelSymbiosis::None;
			if (Kind != EVaelSymbiosis::None)
			{
				const bool bKnown = Floromant->IsPairKnown(Kind);
				Crosses.Add({ bKnown ? VaelSymbiosis::GetName(Kind) : FText::FromString(TEXT("?")), Other, bKnown });
			}
		}
		Crosses.Sort([](const FCross& A, const FCross& B) { return A.bKnown && !B.bKnown; });
		int32 NumKnown = 0;
		for (const FCross& Cross : Crosses)
		{
			NumKnown += Cross.bKnown ? 1 : 0;
		}
		Label(FText::Format(LOCTEXT("CrossCount", "{0} von {1} gewachsen"), NumKnown, Crosses.Num()), SheetX + 270.0f, SheetY + 684.0f, 16.0f, InkDim);
		UTexture2D* Own = GetKeimPicture(*FString::Printf(TEXT("T_Keim_Rund_%s"), *PlantId(Plant)));
		for (int32 Index = 0; Index < FMath::Min(Crosses.Num(), 4); ++Index)
		{
			const FVector2D Pair(SheetX + 96.0f + Index * 112.0f, SheetY + 760.0f);
			const FCross& Cross = Crosses[Index];
			for (int32 Half = 0; Half < 2; ++Half)
			{
				const FVector2D Disc = Pair + FVector2D(Half == 0 ? -14.0f : 14.0f, 0.0f);
				if (Cross.bKnown)
				{
					Centred(Half == 0 ? Own : GetKeimPicture(*FString::Printf(TEXT("T_Keim_Rund_%s"), *PlantId(Cross.Other))), Disc, 48.0f, 48.0f, FLinearColor::White);
				}
				else
				{
					DrawDisc(AtV(Disc), 24.0f * K, KeimRgb(40, 33, 26));
				}
				DrawRing(AtV(Disc), 24.0f * K, Cross.bKnown ? Gold : InkDim, 1.6f * K);
			}
			Label(Cross.Name, Pair.X, Pair.Y + 30.0f, 16.0f, Cross.bKnown ? Ink : InkDim, 0.5f, 108.0f);
		}
		if (Crosses.Num() > 4)
		{
			Label(FText::Format(LOCTEXT("MoreCrosses", "+{0}"), Crosses.Num() - 4), SheetX + 96.0f + 4 * 112.0f - 30.0f, SheetY + 748.0f, 20.0f, InkDim);
		}
	}

	// ---------------- Footer: the buttons
	FText Action;
	switch (Cursor.Zone)
	{
	case EVaelKeimZone::Herbarium:	Action = LOCTEXT("TakePlant", "Pflanze nehmen"); break;
	case EVaelKeimZone::Wheel:		Action = Cursor.Carried >= 0 ? LOCTEXT("SetPlant", "hier einsetzen") : LOCTEXT("PickFirst", "erst eine Pflanze im Herbarium nehmen"); break;
	case EVaelKeimZone::Genes:		Action = LOCTEXT("TakeGene", "Erbanlage aufnehmen"); break;
	}
	// Each hint: the button drawn as its sign (the pad's shapes, Xbox letters, keyboard keys), then what it does
	enum class EButton : uint8 { Confirm, Back, Turn };
	const TPair<EButton, FText> Hints[] =
	{
		{ EButton::Confirm, Action },
		{ EButton::Back, Cursor.Carried >= 0 ? LOCTEXT("PutBack", "zurücklegen") : LOCTEXT("Close", "schließen") },
		{ EButton::Turn, LOCTEXT("Turn", "Tafel wenden") },
	};
	auto KeyText = [&](EButton Button)
	{
		if (Glyphs == EVaelInputGlyphs::Keyboard)
		{
			return Button == EButton::Confirm ? LOCTEXT("KeyConfirm", "Enter") : Button == EButton::Back ? LOCTEXT("KeyBack", "Esc") : LOCTEXT("KeyTurn", "B");
		}
		return Button == EButton::Confirm ? LOCTEXT("XbConfirm", "A") : Button == EButton::Back ? LOCTEXT("XbBack", "B") : LOCTEXT("XbTurn", "X");
	};
	const float TextSize = 21.0f;
	const float Gap = 46.0f;
	float Width = 0.0f;
	for (const TPair<EButton, FText>& Hint : Hints)
	{
		const float Sign = Glyphs == EVaelInputGlyphs::PlayStation ? 26.0f : MeasureLabel(KeyText(Hint.Key), TextSize * K) / K + 14.0f;
		Width += Sign + 8.0f + MeasureLabel(Hint.Value, TextSize * K) / K + Gap;
	}
	float X = 960.0f - (Width - Gap) * 0.5f;
	const float Y = 1026.0f;
	for (const TPair<EButton, FText>& Hint : Hints)
	{
		if (Glyphs == EVaelInputGlyphs::PlayStation)
		{
			// Cross, circle, square in a gold ring
			const FVector2D Centre = At(X + 13.0f, Y + 13.0f);
			DrawRing(Centre, 13.0f * K, Gold, 1.8f * K);
			const float R = 5.5f * K;
			if (Hint.Key == EButton::Confirm)
			{
				DrawSegment(Centre + FVector2D(-R, -R), Centre + FVector2D(R, R), Pale, 1.6f * K);
				DrawSegment(Centre + FVector2D(-R, R), Centre + FVector2D(R, -R), Pale, 1.6f * K);
			}
			else if (Hint.Key == EButton::Back)
			{
				DrawRing(Centre, R, Pale, 1.5f * K);
			}
			else
			{
				DrawFrame(Centre.X - R, Centre.Y - R, 2.0f * R, 2.0f * R, Pale, 1.5f * K);
			}
			X += 26.0f;
		}
		else
		{
			const FText Key = KeyText(Hint.Key);
			const float KeyWidth = MeasureLabel(Key, TextSize * K) / K + 14.0f;
			Frame(X, Y - 1.0f, X + KeyWidth, Y + 27.0f, Gold, 1.4f);
			Label(Key, X + KeyWidth * 0.5f, Y + 1.0f, TextSize, Pale, 0.5f);
			X += KeyWidth;
		}
		X += 8.0f;
		Label(Hint.Value, X, Y + 1.0f, TextSize, Pale);
		X += MeasureLabel(Hint.Value, TextSize * K) / K + Gap;
	}
}

#undef LOCTEXT_NAMESPACE
