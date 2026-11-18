// Copyright Epic Games, Inc. All Rights Reserved.

// The mage's painted menus (Boris 18.11.): Tripo painted them empty, Tools/HUD/vael_tripo_magier.py tidied them (a third circle
// in every row of the formula page, one result square, the baked glow put out), the game writes into them. All places are
// pixels of the 1344x768 pictures:
//  - the formula page: in every row the elements in the circles on the left, the painted sign of the formula in the square
//    on the right, name, effect and how to cast it between them; the four quick slots on top, the fifth shows the chosen one;
//  - the menu frame for the quests: the five tabs are the pages of the menu, the panel holds the open quests, the plates on
//    the right count them by kind, the two rings coins and finished quests, the bar how much of all is done.
// The inventory on the painted grimoire is in VaelHUD.cpp (DrawInventory).

#include "UI/VaelHUD.h"
#include "Engine/Canvas.h"
#include "Engine/Texture2D.h"
#include "Items/VaelInventory.h"
#include "Magic/VaelElementComponent.h"
#include "Magic/VaelFormula.h"
#include "Magic/VaelGrimoireSubsystem.h"
#include "Magic/VaelMagicSettings.h"
#include "Player/VaelCharacter.h"
#include "Player/VaelPlayerController.h"
#include "Story/VaelQuestSubsystem.h"
#include "VaelAssets.h"

#define LOCTEXT_NAMESPACE "VaelHUDMage"

namespace
{
	FLinearColor MageRgb(uint8 R, uint8 G, uint8 B, uint8 A = 255)
	{
		return FLinearColor(FColor(R, G, B, A));
	}

	const FLinearColor Bone = MageRgb(234, 220, 196);
	const FLinearColor Dim = MageRgb(168, 151, 138);
	const FLinearColor Glow = FLinearColor(0.42f, 0.82f, 1.0f);
	const FLinearColor QuestGold = MageRgb(255, 207, 107);

	/** Size of the painted pictures */
	constexpr float PictureW = 1344.0f;
	constexpr float PictureH = 768.0f;

	/** The painted picture as large as the screen allows, with a margin; returns its top left corner and the screen pixels per picture pixel */
	void FitPicture(const UCanvas* Canvas, float Fill, FVector2D& OutOrigin, float& OutK)
	{
		OutK = FMath::Min(Canvas->ClipX * Fill / PictureW, Canvas->ClipY * Fill / PictureH);
		OutOrigin = FVector2D((Canvas->ClipX - PictureW * OutK) * 0.5f, (Canvas->ClipY - PictureH * OutK) * 0.5f);
	}

	/** The name of a painted sign of an element (T_Zauber_<Name>, Tools/HUD/vael_symbole_malen.py) */
	FName GetElementPictureName(EVaelElement Element)
	{
		switch (Element)
		{
		case EVaelElement::Fire: return TEXT("Feuer");
		case EVaelElement::Water: return TEXT("Wasser");
		case EVaelElement::Earth: return TEXT("Erde");
		case EVaelElement::Air: return TEXT("Luft");
		default: return TEXT("Nid");
		}
	}
}

UTexture2D* AVaelHUD::GetSpellPicture(FName Name)
{
	if (const TObjectPtr<UTexture2D>* Found = SpellPictures.Find(Name))
	{
		return Found->Get();
	}
	const FString Asset = FString::Printf(TEXT("/Game/Vael/UI/HUD/Zauber/T_Zauber_%s.T_Zauber_%s"), *Name.ToString(), *Name.ToString());
	UTexture2D* Picture = FPackageName::DoesPackageExist(FString::Printf(TEXT("/Game/Vael/UI/HUD/Zauber/T_Zauber_%s"), *Name.ToString()))
		? Cast<UTexture2D>(VaelAssets::LoadOptional(FSoftObjectPath(Asset))) : nullptr;
	SpellPictures.Add(Name, Picture);
	return Picture;
}

UTexture2D* AVaelHUD::GetFormulaPicture(const UVaelFormula* Formula)
{
	if (Formula == nullptr)
	{
		return nullptr;
	}
	FString Name = Formula->GetName();
	Name.RemoveFromStart(TEXT("DA_Formula_"));
	return GetSpellPicture(FName(*Name));
}

void AVaelHUD::DrawMageFormulas(const AVaelPlayerController* PlayerController)
{
	const UVaelGrimoireSubsystem* Grimoire = GetGrimoire();
	const AVaelCharacter* Player = PlayerController->GetPawn<AVaelCharacter>();
	const UVaelElementComponent* Elements = Player->GetElementComponent();
	const EVaelInputGlyphs Glyphs = PlayerController->GetInputGlyphs();
	const TArray<TObjectPtr<UVaelFormula>>& Formulas = Grimoire->GetAllFormulas();

	DrawBox(0.0f, 0.0f, Canvas->ClipX, Canvas->ClipY, MageRgb(5, 6, 10, 200));

	FVector2D Origin;
	float K = 1.0f;
	FitPicture(Canvas, 0.95f, Origin, K);
	Canvas->K2_DrawTexture(MageFormulaPicture, Origin, FVector2D(PictureW, PictureH) * K, FVector2D::ZeroVector, FVector2D::UnitVector, FLinearColor::White, BLEND_Translucent);
	const auto P = [Origin, K](float X, float Y) { return Origin + FVector2D(X, Y) * K; };
	const auto DrawPicture = [this, &P, K](UTexture2D* Picture, float X, float Y, float Size, const FLinearColor& Tint)
	{
		if (Picture != nullptr)
		{
			Canvas->K2_DrawTexture(Picture, P(X - Size * 0.5f, Y - Size * 0.5f), FVector2D(Size * K), FVector2D::ZeroVector, FVector2D::UnitVector, Tint, BLEND_Translucent);
		}
	};
	const auto GlowFrame = [this, &P, K](float X, float Y, float W, float H)
	{
		const FVector2D At = P(X, Y);
		DrawBox(At.X, At.Y, W * K, H * K, WithAlphaOf(Glow, 0.1f));
		DrawFrame(At.X, At.Y, W * K, H * K, Glow, 2.0f * K);
		DrawFrame(At.X - 4.0f * K, At.Y - 4.0f * K, (W + 8.0f) * K, (H + 8.0f) * K, WithAlphaOf(Glow, 0.3f), 3.0f * K);
	};

	const int32 NumRows = 7;
	const int32 Selection = FMath::Clamp(PlayerController->GetGrimoireSelection(), 0, FMath::Max(Formulas.Num() - 1, 0));
	const int32 FirstRow = FMath::Clamp(Selection - NumRows / 2, 0, FMath::Max(Formulas.Num() - NumRows, 0));
	const UVaelFormula* Chosen = Formulas.IsValidIndex(Selection) ? Formulas[Selection].Get() : nullptr;

	// The cartouche of the title, the quick slots on top, the chosen formula in the fifth square
	const FVector2D Title = P(362.0f, 121.0f);
	DrawTitle(LOCTEXT("Title", "Formeln"), Title.X, Title.Y - 17.0f * K, 30.0f * K, Bone, 0.5f);
	static const float SlotX[] = { 795.0f, 866.0f, 937.0f, 1009.0f };
	for (int32 SlotIndex = 0; SlotIndex < UVaelElementComponent::NumQuickSlots && SlotIndex < 4; ++SlotIndex)
	{
		const UVaelFormula* OnSlot = Elements->GetQuickSlotFormula(SlotIndex);
		DrawPicture(GetFormulaPicture(OnSlot), SlotX[SlotIndex], 121.0f, 46.0f, FLinearColor::White);
		if (OnSlot != nullptr && GetFormulaPicture(OnSlot) == nullptr && !OnSlot->Elements.IsEmpty())
		{
			DrawPicture(GetSpellPicture(GetElementPictureName(OnSlot->Elements[0])), SlotX[SlotIndex], 121.0f, 40.0f, FLinearColor::White);
		}
		const bool bChosenHere = OnSlot != nullptr && OnSlot == Chosen;
		if (bChosenHere)
		{
			GlowFrame(SlotX[SlotIndex] - 27.0f, 94.0f, 54.0f, 54.0f);
		}
		DrawQuickSlotLabel(Glyphs, SlotIndex, P(SlotX[SlotIndex] + 17.0f, 138.0f), 11.0f * K, bChosenHere ? Glow : Bone);
	}
	if (Chosen != nullptr && Grimoire->IsFormulaKnown(Chosen))
	{
		DrawPicture(GetFormulaPicture(Chosen), 1082.0f, 121.0f, 46.0f, FLinearColor::White);
	}
	GlowFrame(1055.0f, 94.0f, 54.0f, 54.0f);

	// The rows
	int32 NumKnown = 0;
	for (const UVaelFormula* Formula : Formulas)
	{
		NumKnown += Grimoire->IsFormulaKnown(Formula) ? 1 : 0;
	}

	for (int32 RowIndex = FirstRow; RowIndex < FMath::Min(FirstRow + NumRows, Formulas.Num()); ++RowIndex)
	{
		const UVaelFormula* Formula = Formulas[RowIndex];
		const bool bKnown = Grimoire->IsFormulaKnown(Formula);
		const float Y = 195.0f + (RowIndex - FirstRow) * 66.0f;

		if (RowIndex == Selection)
		{
			GlowFrame(222.0f, Y - 24.0f, 934.0f, 48.0f);
		}

		// The elements in the circles, hidden until known
		const bool bShowElements = bKnown || Formula->Source == EVaelFormulaSource::Start;
		for (int32 DotIndex = 0; DotIndex < Formula->Elements.Num() && DotIndex < 3; ++DotIndex)
		{
			const float X = 247.0f + DotIndex * 50.0f;
			if (bShowElements)
			{
				DrawPicture(GetSpellPicture(GetElementPictureName(Formula->Elements[DotIndex])), X, Y, 34.0f, FLinearColor::White);
			}
			else
			{
				const FVector2D At = P(X, Y);
				DrawLabel(LOCTEXT("UnknownElement", "?"), At.X, At.Y - 11.0f * K, 20.0f * K, WithAlphaOf(Dim, 0.7f), 0.5f);
			}
		}

		// Name and effect, or what is known about a missing formula
		FText Name = LOCTEXT("UnknownName", "???");
		FText Description;
		if (bKnown)
		{
			Name = Formula->DisplayName;
			Description = Formula->Description;
		}
		else if (Formula->Source == EVaelFormulaSource::Mark)
		{
			Description = LOCTEXT("MarkUnknown", "Erwacht mit dem Nid.");
		}
		else if (Formula->Source == EVaelFormulaSource::Free)
		{
			Description = FText::Format(LOCTEXT("FreeUnknown", "{0} Elemente · durch Experimentieren entdeckbar"), Formula->Elements.Num());
		}
		else if (Grimoire->HasEcho(Formula))
		{
			Name = FText::Format(LOCTEXT("SealedEchoed", "Versiegelt · {0}"), Formula->DisplayName);
			Description = Formula->Hint;
		}
		else
		{
			Description = LOCTEXT("SealedUnknown", "Versiegelt. Wirke die Kombo, um ihr Echo zu hören.");
		}

		const FVector2D TextAt = P(385.0f, Y);
		const float TextWidth = 640.0f * K;
		DrawLabel(Name, TextAt.X, TextAt.Y - 22.0f * K, 18.0f * K, bKnown ? Bone : MageRgb(124, 109, 97), 0.0f, TextWidth * 0.5f);
		DrawLabel(Description, TextAt.X, TextAt.Y + 1.0f * K, 13.0f * K, Dim, 0.0f, TextWidth);

		// How to cast it by hand, in the symbols of this player's device
		if (bKnown)
		{
			float HowX = TextAt.X + FMath::Min(MeasureLabel(Name, 18.0f * K), TextWidth * 0.5f) + 18.0f * K;
			for (const EVaelElement Element : Formula->Elements)
			{
				DrawElementButton(Glyphs, Element, FVector2D(HowX, TextAt.Y - 11.0f * K), 8.0f * K);
				HowX += 20.0f * K;
			}
			DrawLabel(FText::Format(LOCTEXT("HowCast", "· {0}"), GetCastButtonName(Glyphs)), HowX - 4.0f * K, TextAt.Y - 19.0f * K, 13.0f * K, MageRgb(201, 180, 138));
		}

		// The quick slots it sits on, beside the result
		float SlotMarkX = 1094.0f;
		for (int32 SlotIndex = UVaelElementComponent::NumQuickSlots - 1; SlotIndex >= 0; --SlotIndex)
		{
			if (bKnown && Elements->GetQuickSlotFormula(SlotIndex) == Formula)
			{
				DrawQuickSlotLabel(Glyphs, SlotIndex, P(SlotMarkX, Y), 12.0f * K, Glow);
				SlotMarkX -= 26.0f;
			}
		}

		// The result: the painted sign of the formula, the sign of its first element where none is painted yet
		if (bKnown)
		{
			UTexture2D* Result = GetFormulaPicture(Formula);
			if (Result == nullptr && !Formula->Elements.IsEmpty())
			{
				Result = GetSpellPicture(GetElementPictureName(Formula->Elements.Last()));
			}
			DrawPicture(Result, 1131.0f, Y, 32.0f, FLinearColor::White);
		}
		else
		{
			const FVector2D At = P(1131.0f, Y);
			DrawLabel(LOCTEXT("UnknownResult", "?"), At.X, At.Y - 11.0f * K, 20.0f * K, WithAlphaOf(Dim, 0.6f), 0.5f);
		}
	}

	// Below the rows, in the band of the frame: how many are known, the buttons
	const FVector2D Count = P(232.0f, 640.0f);
	DrawLabel(FText::Format(LOCTEXT("KnownCount", "{0} von {1} Formeln bekannt"), NumKnown, Formulas.Num()), Count.X, Count.Y, 14.0f * K, Dim);
	const FText CloseHint = Glyphs == EVaelInputGlyphs::Keyboard ? LOCTEXT("CloseKeyboard", "W / S: wählen · Z X C V: belegen · Tab / Esc: schließen")
		: Glyphs == EVaelInputGlyphs::PlayStation ? LOCTEXT("ClosePlayStation", "Linker Stick: wählen · Steuerkreuz: belegen · ○ / Options: schließen")
		: LOCTEXT("CloseXbox", "Linker Stick: wählen · Steuerkreuz: belegen · B / Menü: schließen");
	const FVector2D Hint = P(1150.0f, 640.0f);
	DrawLabel(CloseHint, Hint.X, Hint.Y, 14.0f * K, Dim, 1.0f);

	DrawMenuTabs(PlayerController, P(1250.0f, 0.0f).X, FMath::Max(4.0f * UiScale, Origin.Y - 16.0f * UiScale));
	DrawMaterialBag(Player, Origin.X, Origin.Y, PictureW * K, PictureH * K);
}

void AVaelHUD::DrawMageQuestLog(const AVaelPlayerController* PlayerController)
{
	const UVaelQuestSubsystem* Quests = UVaelQuestSubsystem::Get(this);
	const AVaelCharacter* Player = PlayerController->GetPawn<AVaelCharacter>();
	const EVaelInputGlyphs Glyphs = PlayerController->GetInputGlyphs();

	DrawBox(0.0f, 0.0f, Canvas->ClipX, Canvas->ClipY, MageRgb(5, 6, 10, 200));
	FVector2D Origin;
	float K = 1.0f;
	FitPicture(Canvas, 1.0f, Origin, K);
	Canvas->K2_DrawTexture(MageMenuPicture, Origin, FVector2D(PictureW, PictureH) * K, FVector2D::ZeroVector, FVector2D::UnitVector, FLinearColor::White, BLEND_Translucent);
	const auto P = [Origin, K](float X, float Y) { return Origin + FVector2D(X, Y) * K; };

	// The plate of the title and the five tabs: the pages of the menu, the open one glows
	const FVector2D Title = P(376.0f, 159.0f);
	DrawTitle(LOCTEXT("QuestsTitle", "Aufträge"), Title.X, Title.Y - 18.0f * K, 32.0f * K, Bone, 0.5f);
	const TPair<EVaelMenuPage, FText> Pages[] =
	{
		{ EVaelMenuPage::Formulas, LOCTEXT("TabFormulas", "Formeln") },
		{ EVaelMenuPage::Inventory, LOCTEXT("TabInventory", "Inventar") },
		{ EVaelMenuPage::Compendium, LOCTEXT("TabCompendium", "Kompendium") },
		{ EVaelMenuPage::Quests, LOCTEXT("TabQuests", "Aufträge") },
		{ EVaelMenuPage::Hero, LOCTEXT("TabHero", "Gestalt") },
	};
	static const float TabX[] = { 548.0f, 638.5f, 727.0f, 815.0f, 905.0f };
	for (int32 Index = 0; Index < 5; ++Index)
	{
		const bool bOpen = Pages[Index].Key == PlayerController->GetMenuPage();
		const FVector2D At = P(TabX[Index], 159.0f);
		if (bOpen)
		{
			const FVector2D Corner = P(TabX[Index] - 38.0f, 128.0f);
			DrawBox(Corner.X, Corner.Y, 76.0f * K, 62.0f * K, WithAlphaOf(Glow, 0.12f));
			DrawFrame(Corner.X, Corner.Y, 76.0f * K, 62.0f * K, Glow, 2.0f * K);
			DrawFrame(Corner.X - 4.0f * K, Corner.Y - 4.0f * K, 84.0f * K, 70.0f * K, WithAlphaOf(Glow, 0.3f), 3.0f * K);
		}
		DrawLabel(Pages[Index].Value, At.X, At.Y - 7.0f * K, 11.5f * K, bOpen ? FLinearColor::White : Dim, 0.5f, 74.0f * K);
	}

	// The ring at the top right closes, the three squares below the panel are the buttons of the menu
	const FText Close = Glyphs == EVaelInputGlyphs::Keyboard ? LOCTEXT("CloseKeyboard", "Esc") : Glyphs == EVaelInputGlyphs::PlayStation ? LOCTEXT("ClosePlayStation", "○") : LOCTEXT("CloseXbox", "B");
	const FVector2D CloseAt = P(1053.0f, 157.0f);
	DrawLabel(Close, CloseAt.X, CloseAt.Y - 11.0f * K, 18.0f * K, Bone, 0.5f);
	const FText Previous = Glyphs == EVaelInputGlyphs::Keyboard ? LOCTEXT("PrevKeyboard", "Q") : Glyphs == EVaelInputGlyphs::PlayStation ? LOCTEXT("PrevPlayStation", "L1") : LOCTEXT("PrevXbox", "LB");
	const FText Next = Glyphs == EVaelInputGlyphs::Keyboard ? LOCTEXT("NextKeyboard", "E") : Glyphs == EVaelInputGlyphs::PlayStation ? LOCTEXT("NextPlayStation", "R1") : LOCTEXT("NextXbox", "RB");
	const FText Buttons[][2] = { { Previous, LOCTEXT("ButtonPrev", "zurück") }, { Next, LOCTEXT("ButtonNext", "weiter") }, { Close, LOCTEXT("ButtonClose", "schließen") } };
	static const float ButtonX[] = { 291.0f, 365.5f, 439.0f };
	for (int32 Index = 0; Index < 3; ++Index)
	{
		const FVector2D At = P(ButtonX[Index], 631.5f);
		DrawLabel(Buttons[Index][0], At.X, At.Y - 15.0f * K, 17.0f * K, Bone, 0.5f, 54.0f * K);
		DrawLabel(Buttons[Index][1], At.X, At.Y + 5.0f * K, 10.0f * K, Dim, 0.5f, 56.0f * K);
	}

	if (Quests == nullptr)
	{
		return;
	}

	// The panel: the open quests, title, giver, what it is about, the steps so far and the current one
	const float Left = P(284.0f, 0.0f).X;
	const float TextWidth = 584.0f * K;
	const float Bottom = P(0.0f, 570.0f).Y;
	float Y = P(0.0f, 238.0f).Y;
	const TArray<const UVaelQuest*> Open = Quests->GetOpenQuests();
	int32 NumMain = 0;
	int32 NumSide = 0;
	int32 NumContracts = 0;
	for (const UVaelQuest* Quest : Open)
	{
		NumMain += Quest->bMainQuest ? 1 : 0;
		NumContracts += !Quest->bMainQuest && Quest->bContract ? 1 : 0;
		NumSide += !Quest->bMainQuest && !Quest->bContract ? 1 : 0;
	}

	if (Open.IsEmpty())
	{
		DrawLabel(LOCTEXT("QuestsNone", "Gerade wartet kein Auftrag."), Left, Y, 16.0f * K, Dim);
	}
	for (const UVaelQuest* Quest : Open)
	{
		if (Y > Bottom - 50.0f * K)
		{
			DrawLabel(LOCTEXT("QuestsMore", "…"), Left, Y, 16.0f * K, Dim);
			break;
		}
		const FVaelQuestProgress Progress = Quests->GetProgress(Quest);
		const FText Kind = Quest->bMainQuest ? LOCTEXT("QuestMain", "Hauptgeschichte")
			: Quest->bContract ? FText::Format(LOCTEXT("QuestContract", "Kontrakt, {0} Münzen"), Quest->CoinReward)
			: LOCTEXT("QuestSide", "Nebenauftrag");
		DrawTitle(Quest->Title, Left, Y, 21.0f * K, Quest->bMainQuest ? QuestGold : Bone, 0.0f, TextWidth * 0.6f);
		DrawLabel(FText::Format(LOCTEXT("QuestGiver", "{0} · {1}"), Kind, Quest->GiverName), Left + TextWidth, Y + 5.0f * K, 12.0f * K, Dim, 1.0f, TextWidth * 0.4f);
		Y += 27.0f * K;
		if (!Quest->Summary.IsEmpty())
		{
			Y = DrawWrappedLabel(Quest->Summary, Left, Y, 13.0f * K, MageRgb(205, 189, 166), TextWidth) + 3.0f * K;
		}
		for (int32 StepIndex = 0; StepIndex <= Progress.Step && Quest->Steps.IsValidIndex(StepIndex); ++StepIndex)
		{
			const bool bCurrent = StepIndex == Progress.Step;
			const FVaelQuestStep& Step = Quest->Steps[StepIndex];
			const FText Line = bCurrent ? GetQuestObjectiveLine(Step, Progress) : Step.Objective;
			DrawLabel(bCurrent ? LOCTEXT("QuestStepNow", "›") : LOCTEXT("QuestStepDone", "·"), Left + 4.0f * K, Y, 14.0f * K, bCurrent ? Glow : Dim);
			DrawLabel(Line, Left + 20.0f * K, Y, 14.0f * K, bCurrent ? Bone : Dim, 0.0f, TextWidth - 20.0f * K);
			Y += 19.0f * K;
		}
		Y += 14.0f * K;
	}

	// The plates on the right: the open quests by kind
	const TPair<FText, int32> Kinds[] = { { LOCTEXT("PlateMain", "Hauptgeschichte"), NumMain }, { LOCTEXT("PlateSide", "Nebenaufträge"), NumSide }, { LOCTEXT("PlateContracts", "Kontrakte"), NumContracts } };
	static const float PlateY[] = { 250.0f, 337.0f, 423.0f };
	for (int32 Index = 0; Index < 3; ++Index)
	{
		const FVector2D At = P(1010.0f, PlateY[Index]);
		DrawLabel(Kinds[Index].Key, At.X, At.Y - 20.0f * K, 13.0f * K, Dim, 0.5f, 170.0f * K);
		DrawTitle(FText::AsNumber(Kinds[Index].Value), At.X, At.Y - 3.0f * K, 22.0f * K, Kinds[Index].Value > 0 ? Bone : Dim, 0.5f);
	}

	// The two rings: the coins of the player, the finished quests; the bar: how much of all quests is done
	int32 NumDone = 0;
	for (const UVaelQuest* Quest : Quests->GetQuests())
	{
		NumDone += Quests->GetProgress(Quest).bDone ? 1 : 0;
	}
	const int32 NumAll = NumDone + Open.Num();
	const UVaelInventory* Inventory = Player != nullptr ? Player->GetInventory() : nullptr;
	const FVector2D Coins = P(955.0f, 604.0f);
	DrawTitle(FText::AsNumber(Inventory != nullptr ? Inventory->GetCoins() : 0), Coins.X, Coins.Y - 20.0f * K, 24.0f * K, QuestGold, 0.5f, 80.0f * K);
	DrawLabel(LOCTEXT("RingCoins", "Münzen"), Coins.X, Coins.Y + 8.0f * K, 11.0f * K, Dim, 0.5f, 80.0f * K);
	const FVector2D Done = P(1061.0f, 604.0f);
	DrawTitle(FText::AsNumber(NumDone), Done.X, Done.Y - 20.0f * K, 24.0f * K, Bone, 0.5f, 80.0f * K);
	DrawLabel(LOCTEXT("RingDone", "erledigt"), Done.X, Done.Y + 8.0f * K, 11.0f * K, Dim, 0.5f, 80.0f * K);

	const FVector2D BarAt = P(492.0f, 621.0f);
	const float BarWidth = 393.0f * K;
	const float BarHeight = 19.0f * K;
	const float Fraction = NumAll > 0 ? static_cast<float>(NumDone) / NumAll : 0.0f;
	if (Fraction > 0.0f)
	{
		DrawBox(BarAt.X, BarAt.Y, BarWidth * Fraction, BarHeight, MageRgb(58, 96, 170, 220));
		DrawBox(BarAt.X, BarAt.Y, BarWidth * Fraction, BarHeight * 0.4f, MageRgb(140, 180, 240, 90));
	}
	DrawLabel(FText::Format(LOCTEXT("BarText", "{0} von {1} Aufträgen erledigt"), NumDone, NumAll), BarAt.X + BarWidth * 0.5f, BarAt.Y + 2.0f * K, 12.0f * K, Bone, 0.5f);
}

#undef LOCTEXT_NAMESPACE
