// Copyright Epic Games, Inc. All Rights Reserved.

// The Keimgeflecht page of the Floromant (Boris 2026-11-04): a living root chamber with her herbarium on the left, the strand of
// her heredity in the middle (light and shadow vine, the wheel places as seed capsules) and the naturalist's plate of a plant on
// the right with its Erbanlagen and crosses. The fixed picture is rendered by Tools/HUD/vael_keimgeflecht_entwurf.py --spiel;
// everything that changes is drawn here on top, in design pixels of 1920 x 1080.

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

	/** The id of a plant arm (DA_Pflanzenarm_<Id>) names its round picture T_Keim_Rund_<Id> */
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
	auto Picture = [&](UTexture2D* Texture, float X, float Y, float W, float H, const FLinearColor& Tint)
	{
		if (Texture != nullptr)
		{
			Canvas->K2_DrawTexture(Texture, At(X, Y), FVector2D(W * K, H * K), FVector2D::ZeroVector, FVector2D::UnitVector, Tint, BLEND_Translucent);
		}
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
	const FLinearColor InkDim = KeimRgb(42, 27, 16, 130);
	const FLinearColor Pale = KeimRgb(226, 210, 174);
	const FLinearColor Gold = KeimRgb(232, 178, 82);
	const FLinearColor Sap = KeimRgb(140, 236, 150);
	const float Time = GetWorld() != nullptr ? GetWorld()->GetRealTimeSeconds() : 0.0f;
	const float Pulse = 0.5f + 0.5f * FMath::Sin(Time * 5.0f);
	const EVaelInputGlyphs Glyphs = PlayerController->GetInputGlyphs();
	const FVaelKeimCursor& Cursor = PlayerController->GetKeimCursor();

	// The chamber: black around it where the screen is wider than 16:9
	DrawBox(0.0f, 0.0f, Canvas->ClipX, Canvas->ClipY, KeimRgb(8, 6, 5));
	Picture(GetKeimPicture(TEXT("T_Keim_Grund")), 0.0f, 0.0f, 1920.0f, 1080.0f, FLinearColor::White);
	DrawMenuTabs(PlayerController, OX + 1830.0f * K, 30.0f * K);

	// ---------------- Herbarium: a card for each plant she knows, "?" for the places still empty
	const int32 NumCards = FMath::Min(Floromant->GetCatalogSize(), AVaelPlayerController::KeimCards);
	const int32 NumPlaces = Floromant->GetHandBeltSize();
	for (int32 Index = 0; Index < AVaelPlayerController::KeimCards; ++Index)
	{
		const float X0 = 110.0f + (Index % 3) * 182.0f;
		const float Y0 = 262.0f + (Index / 3) * 180.0f;
		const UVaelPlantArmData* Plant = Index < NumCards ? Floromant->GetCatalogPlant(Index) : nullptr;
		if (Plant == nullptr)
		{
			DrawBox(OX + (X0 + 18.0f) * K, (Y0 + 6.0f) * K, 124.0f * K, 124.0f * K, KeimRgb(14, 11, 9, 200));
			Title(FText::FromString(TEXT("?")), X0 + 80.0f, Y0 + 34.0f, 64.0f, KeimRgb(120, 108, 92), 0.5f);
			continue;
		}
		Picture(VaelAssets::LoadOptional(Plant->WheelPicture), X0 + 22.0f, Y0 + 10.0f, 116.0f, 116.0f, FLinearColor::White);
		Label(Plant->GetDisplayName(false), X0 + 80.0f, Y0 + 134.0f, 17.0f, Ink, 0.5f, 150.0f);

		// In the wheel: a small green germ in the corner
		bool bInWheel = false;
		for (int32 Place = 0; Place < NumPlaces; ++Place)
		{
			bInWheel |= Floromant->GetHandBeltPlant(Place) == Plant;
		}
		if (bInWheel)
		{
			DrawDisc(At(X0 + 146.0f, Y0 + 14.0f), 7.0f * K, Sap);
		}
		if (Cursor.Carried == Index)
		{
			Frame(X0 - 6.0f, Y0 - 6.0f, X0 + 166.0f, Y0 + 166.0f, Sap * (0.6f + 0.4f * Pulse), 3.0f);
		}
		else if (Cursor.Zone == EVaelKeimZone::Herbarium && Cursor.Card == Index)
		{
			Frame(X0 - 6.0f, Y0 - 6.0f, X0 + 166.0f, Y0 + 166.0f, Gold, 3.0f);
		}
	}

	// ---------------- The strand: the wheel places as capsules, the ones not grown yet as sealed buds
	for (int32 Place = 0; Place < AVaelPlayerController::KeimCapsules; ++Place)
	{
		const float Side = Place % 2 == 0 ? -1.0f : 1.0f;
		const FVector2D Centre(818.0f + Side * 112.0f, 272.0f + Place * 92.0f);
		if (Place >= NumPlaces)
		{
			Picture(GetKeimPicture(TEXT("T_Keim_Knospe")), Centre.X - 48.0f, Centre.Y - 48.0f, 96.0f, 96.0f, FLinearColor::White);
		}
		else if (const UVaelPlantArmData* Plant = Floromant->GetHandBeltPlant(Place))
		{
			const bool bShadow = Floromant->IsWheelShadow(Place) && !Plant->IsLightOnly();
			UTexture2D* Round = GetKeimPicture(*FString::Printf(TEXT("T_Keim_Rund_%s%s"), *PlantId(Plant), bShadow ? TEXT("_Schatten") : TEXT("")));
			Picture(Round, Centre.X - 37.0f, Centre.Y - 37.0f, 74.0f, 74.0f, FLinearColor::White);
		}

		const bool bHere = Cursor.Zone == EVaelKeimZone::Wheel && Cursor.Capsule == Place;
		if (bHere && Cursor.Carried >= 0)
		{
			// The carried plant hovers over the capsule it would grow in
			if (const UVaelPlantArmData* Carried = Floromant->GetCatalogPlant(Cursor.Carried))
			{
				UTexture2D* Round = GetKeimPicture(*FString::Printf(TEXT("T_Keim_Rund_%s"), *PlantId(Carried)));
				Picture(Round, Centre.X - 37.0f, Centre.Y - 37.0f, 74.0f, 74.0f, FLinearColor(1.0f, 1.0f, 1.0f, 0.55f + 0.3f * Pulse));
			}
			DrawRing(At(Centre.X, Centre.Y), 46.0f * K, Sap * (0.6f + 0.4f * Pulse), 3.0f * K);
		}
		else if (bHere)
		{
			DrawRing(At(Centre.X, Centre.Y), 46.0f * K, Gold, 3.0f * K);
		}
	}

	// ---------------- The plate of the plant
	const UVaelPlantArmData* Plant = Floromant->GetCatalogPlant(FMath::Clamp(Cursor.PlatePlant, 0, FMath::Max(Floromant->GetCatalogSize() - 1, 0)));
	if (Plant != nullptr)
	{
		const bool bShadowSide = Cursor.bShadowSide && !Plant->IsLightOnly();
		Label(FText::Format(LOCTEXT("Plate", "Tafel {0}"), Roman(Cursor.PlatePlant + 1)), 1070.0f, 256.0f, 20.0f, Ink);
		Title(Plant->GetDisplayName(bShadowSide), 1070.0f, 280.0f, 50.0f, Ink, 0.0f, 560.0f);
		const FText Form = bShadowSide ? LOCTEXT("ShadowForm", "Schattenform") : Plant->IsLightOnly() ? LOCTEXT("LightOnly", "nur Lichtform") : LOCTEXT("LightForm", "Lichtform");
		Label(Plant->LatinName.IsEmpty() ? Form : FText::Format(LOCTEXT("LatinForm", "{0} · {1}"), Plant->LatinName, Form), 1072.0f, 348.0f, 19.0f, Ink);

		// The big picture in its dark window
		Picture(VaelAssets::LoadOptional(bShadowSide && !Plant->WheelShadowPicture.IsNull() ? Plant->WheelShadowPicture : Plant->WheelPicture),
			1070.0f, 390.0f, 300.0f, 300.0f, FLinearColor::White);

		// Beside it: the figures of the plate, or what the plant does in numbers with her Erbanlagen
		if (Plant->Figures.Num() > 0)
		{
			for (int32 Index = 0; Index < FMath::Min(Plant->Figures.Num(), 3); ++Index)
			{
				const float Y = 404.0f + Index * 96.0f;
				DrawSegment(At(1376.0f, Y + 14.0f), At(1404.0f, Y + 14.0f), Ink, 1.2f * K);
				Title(FText::Format(LOCTEXT("Fig", "Fig. {0}  {1}"), Index + 1, Plant->Figures[Index].Title), 1410.0f, Y, 23.0f, Ink, 0.0f, 400.0f);
				Label(Plant->Figures[Index].Note, 1410.0f, Y + 32.0f, 17.0f, Ink, 0.0f, 400.0f);
			}
		}
		else
		{
			const float Saft = Plant->SaftPerShot * Floromant->GetGeneScale(Plant, EVaelGeneEffect::SaftCost);
			const float Rate = 1.0f / FMath::Max(Plant->FireInterval * Floromant->GetGeneScale(Plant, EVaelGeneEffect::FireSpeed), 0.01f);
			const float Power = Floromant->GetGeneScale(Plant, EVaelGeneEffect::Damage);
			const float Nid = Plant->NidPerSecond * Floromant->GetGeneScale(Plant, EVaelGeneEffect::NidPrice) * UVaelFloromantSettings::Get()->NidPriceScale;
			FNumberFormattingOptions Two;
			Two.MinimumFractionalDigits = 1;
			Two.MaximumFractionalDigits = 2;
			const TPair<FText, FText> Lines[] =
			{
				{ LOCTEXT("Strength", "Kraft"), FText::Format(LOCTEXT("StrengthValue", "{0} %"), FText::AsNumber(FMath::RoundToInt(Power * 100.0f))) },
				{ LOCTEXT("Rate", "Schüsse je Sekunde"), FText::AsNumber(Rate, &Two) },
				{ LOCTEXT("Saft", "Saft je Schuss"), FText::AsNumber(Saft, &Two) },
				{ LOCTEXT("Nid", "Nid je Sekunde im Schatten"), Plant->IsLightOnly() ? LOCTEXT("NoNid", "keins, nur Licht") : FText::AsNumber(Nid, &Two) },
			};
			for (int32 Index = 0; Index < UE_ARRAY_COUNT(Lines); ++Index)
			{
				const float Y = 400.0f + Index * 60.0f;
				Title(Lines[Index].Key, 1410.0f, Y, 22.0f, Ink);
				Label(Lines[Index].Value, 1410.0f, Y + 30.0f, 19.0f, Ink);
			}
		}

		// The shadow form in its red ring: sealed on the light path, chosen with the medallion before the vow, open after the shadow vow
		const FVector2D Shadow(1710.0f, 316.0f);
		if (!Plant->IsLightOnly())
		{
			UTexture2D* Round = GetKeimPicture(*FString::Printf(TEXT("T_Keim_Rund_%s_Schatten"), *PlantId(Plant)));
			const bool bSealed = Player->GetPath() == EVaelPath::Light;
			Picture(Round, Shadow.X - 50.0f, Shadow.Y - 50.0f, 100.0f, 100.0f, bSealed ? FLinearColor(0.35f, 0.35f, 0.35f) : FLinearColor::White);
		}
		const FText ShadowState = Plant->IsLightOnly() ? LOCTEXT("ShadowNone", "hat keine")
			: Player->GetPath() == EVaelPath::Light ? LOCTEXT("ShadowSealed", "durch den Lichtweg versiegelt")
			: Player->GetPath() == EVaelPath::None ? LOCTEXT("ShadowBefore", "vor dem Schwur mit dem Medaillon")
			: LOCTEXT("ShadowOpen", "offen, trinkt Nid");
		Label(ShadowState, Shadow.X, 406.0f, 14.0f, Ink, 0.5f, 230.0f);

		// Erbanlagen: per tier one of two, open from her level on
		const TArray<FVaelGeneTier>& Tiers = Plant->GetGeneTiers();
		const TArray<int32>& TierLevels = UVaelFloromantSettings::Get()->GeneTierLevels;
		for (int32 Tier = 0; Tier < FMath::Min(Tiers.Num(), 3); ++Tier)
		{
			const float GX = 1080.0f + Tier * 255.0f;
			const bool bOpen = Floromant->IsGeneTierOpen(Tier);
			Label(bOpen ? FText::Format(LOCTEXT("Tier", "Stufe {0}"), Roman(Tier + 1))
				: FText::Format(LOCTEXT("TierClosed", "Stufe {0} · ab Stufe {1}"), Roman(Tier + 1), TierLevels.IsValidIndex(Tier) ? TierLevels[Tier] : 99),
				GX, 750.0f, 15.0f, bOpen ? Ink : InkDim);
			const int32 Taken = Floromant->GetGene(Plant, Tier);
			for (int32 Option = 0; Option < FMath::Min(Tiers[Tier].Options.Num(), 2); ++Option)
			{
				const float GY = 776.0f + Option * 40.0f;
				const FVector2D Dot = At(GX + 16.0f, GY + 13.0f);
				DrawRing(Dot, 14.0f * K, Taken == Option ? Gold : (bOpen ? Ink : InkDim), 2.4f * K);
				if (Taken == Option)
				{
					DrawDisc(Dot, 9.0f * K, Sap);
				}
				if (Cursor.Zone == EVaelKeimZone::Genes && Cursor.Tier == Tier && Cursor.Option == Option)
				{
					DrawRing(Dot, 19.0f * K, Gold * (0.7f + 0.3f * Pulse), 2.0f * K);
				}
				Label(Tiers[Tier].Options[Option].Name, GX + 40.0f, GY + 1.0f, 18.0f, bOpen ? Ink : InkDim, 0.0f, 205.0f);
			}
		}

		// Crosses with every other plant: known ones by name, the others hidden
		struct FCross { FText Name; FText Partner; bool bKnown; };
		TArray<FCross> Crosses;
		for (int32 Index = 0; Index < Floromant->GetCatalogSize(); ++Index)
		{
			const UVaelPlantArmData* Other = Floromant->GetCatalogPlant(Index);
			const EVaelSymbiosis Kind = Other != nullptr && Other != Plant ? VaelSymbiosis::Find(Plant, Other) : EVaelSymbiosis::None;
			if (Kind != EVaelSymbiosis::None)
			{
				const bool bKnown = Floromant->IsPairKnown(Kind);
				Crosses.Add({ bKnown ? VaelSymbiosis::GetName(Kind) : FText::FromString(TEXT("?")), bKnown ? Other->GetDisplayName(false) : FText::FromString(TEXT("?")), bKnown });
			}
		}
		Crosses.Sort([](const FCross& A, const FCross& B) { return A.bKnown && !B.bKnown; });
		int32 NumKnown = 0;
		for (const FCross& Cross : Crosses)
		{
			NumKnown += Cross.bKnown ? 1 : 0;
		}
		Label(FText::Format(LOCTEXT("CrossCount", "{0} von {1} gewachsen"), NumKnown, Crosses.Num()), 1300.0f, 868.0f, 16.0f, Ink);
		for (int32 Index = 0; Index < FMath::Min(Crosses.Num(), 4); ++Index)
		{
			const float CX = 1080.0f + (Index % 2) * 370.0f;
			const float CY = 902.0f + (Index / 2) * 32.0f;
			Title(Crosses[Index].Name, CX, CY, 20.0f, Ink, 0.0f, 150.0f);
			Label(FText::Format(LOCTEXT("With", "mit {0}"), Crosses[Index].Partner), CX + 150.0f, CY + 2.0f, 17.0f, Ink, 0.0f, 200.0f);
		}
		if (Crosses.Num() > 4)
		{
			Label(FText::Format(LOCTEXT("MoreCrosses", "und {0} weitere"), Crosses.Num() - 4), 1780.0f, 936.0f, 15.0f, Ink, 1.0f);
		}
	}

	// ---------------- Footer: the buttons, and what the selected Erbanlage does
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
	const float TextSize = 20.0f;
	const float Gap = 46.0f;
	float Width = 0.0f;
	for (const TPair<EButton, FText>& Hint : Hints)
	{
		const float Sign = Glyphs == EVaelInputGlyphs::PlayStation ? 26.0f : MeasureLabel(KeyText(Hint.Key), TextSize * K) / K + 14.0f;
		Width += Sign + 8.0f + MeasureLabel(Hint.Value, TextSize * K) / K + Gap;
	}
	float X = 960.0f - (Width - Gap) * 0.5f;
	const float Y = 1004.0f;
	for (const TPair<EButton, FText>& Hint : Hints)
	{
		if (Glyphs == EVaelInputGlyphs::PlayStation)
		{
			// Cross, circle, square in a gold ring
			const FVector2D Centre = At(X + 13.0f, Y + 13.0f);
			DrawRing(Centre, 12.0f * K, Gold, 1.6f * K);
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

	if (Cursor.Zone == EVaelKeimZone::Genes && Plant != nullptr && Plant->GetGeneTiers().IsValidIndex(Cursor.Tier)
		&& Plant->GetGeneTiers()[Cursor.Tier].Options.IsValidIndex(Cursor.Option))
	{
		const FVaelGene& Gene = Plant->GetGeneTiers()[Cursor.Tier].Options[Cursor.Option];
		Title(Gene.Name, 1410.0f, 640.0f, 20.0f, Ink, 0.0f, 400.0f);
		Label(Gene.Description, 1410.0f, 666.0f, 16.0f, Ink, 0.0f, 400.0f);
	}
}

#undef LOCTEXT_NAMESPACE
