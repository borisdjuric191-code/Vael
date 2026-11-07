// Copyright Epic Games, Inc. All Rights Reserved.

// The witch's kitchen at the root table of a refugium (Boris 2026-11-07: a mixing menu, the look of the combination list of
// Monster Hunter Rise as a light direction, in the Vael vibe): a dark lacquered tray with thin gold lines and embers, the tabs
// of the Rezeptbuch on top, the recipes as a list on the left (flask, name, way, what the bag has of each ingredient), the
// chosen recipe on the right as one big combination: ingredient + ingredient + ingredient = potion, then what it does.
// Also the seed choice of a bed. Drawn on the canvas like the other pages; design sizes are scaled by UiScale.

#include "UI/VaelHUD.h"
#include "Engine/Canvas.h"
#include "Engine/Texture2D.h"
#include "Items/VaelBrewing.h"
#include "Items/VaelPotionEffects.h"
#include "Items/VaelItemTypes.h"
#include "Items/VaelMaterial.h"
#include "Items/VaelMaterialBag.h"
#include "Nature/VaelGarden.h"
#include "Player/VaelCharacter.h"
#include "Player/VaelPlayerController.h"

#define LOCTEXT_NAMESPACE "VaelHUDBrewing"

namespace
{
	FLinearColor BrewRgb(uint8 R, uint8 G, uint8 B, uint8 A = 255)
	{
		return FLinearColor(FColor(R, G, B, A));
	}

	FLinearColor BrewAlpha(const FLinearColor& Color, float Alpha)
	{
		return FLinearColor(Color.R, Color.G, Color.B, Color.A * Alpha);
	}

	// The tray: almost black lacquer with a red-brown depth, thin old gold, bone white text, embers for what is chosen
	const FLinearColor BrewLacquer = BrewRgb(16, 11, 10, 238);
	const FLinearColor BrewLacquerLight = BrewRgb(34, 24, 20, 240);
	const FLinearColor BrewGold = BrewRgb(196, 158, 92);
	const FLinearColor BrewGoldDim = BrewRgb(112, 88, 54);
	const FLinearColor BrewBone = BrewRgb(236, 224, 202);
	const FLinearColor BrewDim = BrewRgb(160, 142, 122);
	const FLinearColor BrewEmber = BrewRgb(236, 116, 50);
	const FLinearColor BrewHave = BrewRgb(150, 214, 120);
	const FLinearColor BrewLack = BrewRgb(210, 90, 74);
	const FLinearColor BrewSealed = BrewRgb(150, 82, 170);

	FText ConfirmGlyph(EVaelInputGlyphs Glyphs)
	{
		return Glyphs == EVaelInputGlyphs::Keyboard ? LOCTEXT("ConfirmKeyboard", "Enter") : Glyphs == EVaelInputGlyphs::PlayStation ? LOCTEXT("ConfirmPS", "Kreuz") : LOCTEXT("ConfirmXbox", "A");
	}

	FText CloseGlyph(EVaelInputGlyphs Glyphs)
	{
		return Glyphs == EVaelInputGlyphs::Keyboard ? LOCTEXT("CloseKeyboard", "Esc") : Glyphs == EVaelInputGlyphs::PlayStation ? LOCTEXT("ClosePS", "Kreis") : LOCTEXT("CloseXbox", "B");
	}

	FText TabGlyphs(EVaelInputGlyphs Glyphs)
	{
		return Glyphs == EVaelInputGlyphs::Keyboard ? LOCTEXT("TabsKeyboard", "Pfeil links / rechts") : Glyphs == EVaelInputGlyphs::PlayStation ? LOCTEXT("TabsPS", "L1 · R1") : LOCTEXT("TabsXbox", "LB · RB");
	}
}

void AVaelHUD::DrawFlask(const FVector2D& Center, float Size, const FLinearColor& Liquid, float Glow)
{
	// A round-bellied vial: dark glass, the liquid filling the lower belly, a narrow neck with a cork, a highlight on the glass
	const float R = Size * 0.5f;
	if (Glow > 0.0f)
	{
		DrawDisc(Center + FVector2D(0.0f, R * 0.25f), R * (1.25f + 0.2f * Glow), BrewAlpha(Liquid, 0.18f * Glow));
	}
	DrawBox(Center.X - R * 0.26f, Center.Y - R * 1.25f, R * 0.52f, R * 0.7f, BrewRgb(60, 52, 48, 220));
	DrawBox(Center.X - R * 0.34f, Center.Y - R * 1.5f, R * 0.68f, R * 0.32f, BrewRgb(122, 84, 52));
	DrawDisc(Center + FVector2D(0.0f, R * 0.25f), R * 0.86f, BrewRgb(52, 46, 44, 230));
	DrawDisc(Center + FVector2D(0.0f, R * 0.38f), R * 0.7f, Liquid);
	DrawDisc(Center + FVector2D(0.0f, R * 0.5f), R * 0.48f, BrewAlpha(FLinearColor::LerpUsingHSV(Liquid, FLinearColor::White, 0.35f), 0.55f));
	DrawRing(Center + FVector2D(0.0f, R * 0.25f), R * 0.86f, BrewAlpha(BrewGold, 0.7f), FMath::Max(1.0f, R * 0.07f), false);
	DrawDisc(Center + FVector2D(-R * 0.38f, -R * 0.05f), R * 0.13f, BrewRgb(255, 250, 240, 150));
}

void AVaelHUD::DrawIngredientField(const UVaelMaterial* Material, const FVector2D& Center, float Half, int32 Have, int32 Need, float T)
{
	// One field of the combination: the picture of the thing in a rounded gold frame, what the bag holds of it under it
	const bool bEnough = Have >= Need;
	DrawBox(Center.X - Half, Center.Y - Half, Half * 2.0f, Half * 2.0f, BrewLacquerLight);
	DrawFrame(Center.X - Half, Center.Y - Half, Half * 2.0f, Half * 2.0f, bEnough ? BrewGold : BrewAlpha(BrewLack, 0.85f), FMath::Max(1.0f, 2.0f * T));
	DrawFrame(Center.X - Half + 4.0f * T, Center.Y - Half + 4.0f * T, Half * 2.0f - 8.0f * T, Half * 2.0f - 8.0f * T, BrewAlpha(BrewGoldDim, 0.5f), 1.0f);

	UTexture2D* Icon = Material != nullptr && !Material->Icon.IsNull() ? Material->Icon.LoadSynchronous() : nullptr;
	if (Icon != nullptr)
	{
		const FVector2D Extent(Half * 1.7f);
		Canvas->K2_DrawTexture(Icon, Center - Extent * 0.5f, Extent, FVector2D::ZeroVector, FVector2D::UnitVector, bEnough ? FLinearColor::White : FLinearColor(0.55f, 0.5f, 0.5f), BLEND_Translucent);
	}
	else if (Material != nullptr)
	{
		DrawDisc(Center, Half * 0.42f, Material->Color);
	}
	if (Material != nullptr)
	{
		DrawWaySign(Material->Way, Center + FVector2D(Half * 0.66f, -Half * 0.66f), Half * 0.18f);
	}
	DrawLabel(FText::Format(LOCTEXT("HaveNeed", "{0}/{1}"), Have, Need), Center.X + Half * 0.9f, Center.Y + Half * 0.42f, 15.0f * T, bEnough ? BrewHave : BrewLack, 1.0f);
}

void AVaelHUD::DrawBrewing(const AVaelPlayerController* PlayerController)
{
	const AVaelCharacter* Player = PlayerController->GetPawn<AVaelCharacter>();
	if (Player == nullptr)
	{
		return;
	}

	const float S = UiScale;
	const EVaelInputGlyphs Glyphs = PlayerController->GetInputGlyphs();
	const UVaelMaterialBag* Bag = Player->GetMaterialBag();
	const float Time = GetWorld()->GetRealTimeSeconds();

	DrawBox(0.0f, 0.0f, Canvas->ClipX, Canvas->ClipY, BrewRgb(8, 5, 5, 176));

	const float Width = FMath::Min(1180.0f * S, Canvas->ClipX - 32.0f * S);
	const float Height = FMath::Min(660.0f * S, Canvas->ClipY - 32.0f * S);
	const float Left = (Canvas->ClipX - Width) * 0.5f;
	const float Top = (Canvas->ClipY - Height) * 0.5f;
	const float Pad = 26.0f * S;

	// The tray with a double gold line and corner studs
	DrawBox(Left, Top, Width, Height, BrewLacquer);
	DrawFrame(Left, Top, Width, Height, BrewGoldDim, 2.0f * S);
	DrawFrame(Left + 6.0f * S, Top + 6.0f * S, Width - 12.0f * S, Height - 12.0f * S, BrewAlpha(BrewGold, 0.55f), 1.0f);
	for (const FVector2D& Corner : { FVector2D(Left, Top), FVector2D(Left + Width, Top), FVector2D(Left, Top + Height), FVector2D(Left + Width, Top + Height) })
	{
		DrawDisc(Corner, 6.0f * S, BrewGold);
		DrawDisc(Corner, 3.0f * S, BrewEmber);
	}

	DrawLabel(LOCTEXT("Eyebrow", "HEXENKÜCHE · WURZELTISCH DER GLUTKAMMER"), Left + Pad, Top + 20.0f * S, 12.0f * S, BrewEmber);
	DrawLabel(LOCTEXT("Title", "Rezeptbuch"), Left + Pad, Top + 36.0f * S, 34.0f * S, BrewBone);

	// Tabs of the book, pill shaped; the chosen one lit in ember with a gold rim
	const int32 TierIndex = PlayerController->GetBrewTier();
	const float TabWidth = 190.0f * S;
	const float TabHeight = 32.0f * S;
	const float TabTop = Top + 40.0f * S;
	const float TabsLeft = Left + Width - Pad - 3 * (TabWidth + 8.0f * S);
	for (int32 Tab = 0; Tab < 3; ++Tab)
	{
		const float X = TabsLeft + Tab * (TabWidth + 8.0f * S);
		const bool bOn = Tab == TierIndex;
		DrawBox(X, TabTop, TabWidth, TabHeight, bOn ? BrewRgb(72, 36, 22, 245) : BrewLacquerLight);
		DrawFrame(X, TabTop, TabWidth, TabHeight, bOn ? BrewGold : BrewAlpha(BrewGoldDim, 0.8f), bOn ? 2.0f * S : 1.0f);
		DrawDisc(FVector2D(X, TabTop + TabHeight * 0.5f), TabHeight * 0.5f, bOn ? BrewRgb(72, 36, 22, 245) : BrewLacquerLight);
		DrawDisc(FVector2D(X + TabWidth, TabTop + TabHeight * 0.5f), TabHeight * 0.5f, bOn ? BrewRgb(72, 36, 22, 245) : BrewLacquerLight);
		DrawLabel(FVaelBrewing::GetTierName(static_cast<EVaelBrewTier>(Tab)), X + TabWidth * 0.5f, TabTop + 7.0f * S, 15.0f * S, bOn ? BrewBone : BrewDim, 0.5f);
	}
	DrawLabel(TabGlyphs(Glyphs), TabsLeft - 24.0f * S, TabTop + 9.0f * S, 12.0f * S, BrewDim, 1.0f);

	const float BodyTop = Top + 96.0f * S;
	DrawSegment(FVector2D(Left + Pad, BodyTop - 10.0f * S), FVector2D(Left + Width - Pad, BodyTop - 10.0f * S), BrewAlpha(BrewGold, 0.5f), 1.0f);

	// ---- the list of recipes on the left
	const TArray<const FVaelRecipe*> Recipes = FVaelBrewing::GetRecipes(static_cast<EVaelBrewTier>(TierIndex));
	const int32 Selected = FMath::Clamp(PlayerController->GetBrewSelection(), 0, FMath::Max(Recipes.Num() - 1, 0));
	const float ListWidth = Width * 0.4f;
	const float RowHeight = 40.0f * S;
	const int32 MaxRows = FMath::Max(1, FMath::FloorToInt((Height - (BodyTop - Top) - 60.0f * S) / RowHeight));
	const int32 First = FMath::Clamp(Selected - MaxRows + 1, 0, FMath::Max(Recipes.Num() - MaxRows, 0));
	for (int32 Row = 0; Row < FMath::Min(MaxRows, Recipes.Num() - First); ++Row)
	{
		const FVaelRecipe& Recipe = *Recipes[First + Row];
		const int32 Index = First + Row;
		const float Y = BodyTop + Row * RowHeight;
		const bool bOn = Index == Selected;
		const EVaelBrewBlock Block = FVaelBrewing::GetBlock(Player, Recipe);
		const bool bSealed = Block == EVaelBrewBlock::SealedShadow || Block == EVaelBrewBlock::SealedTwilight;

		if (bOn)
		{
			DrawBox(Left + Pad - 6.0f * S, Y - 2.0f * S, ListWidth, RowHeight - 4.0f * S, BrewRgb(60, 34, 24, 235));
			DrawFrame(Left + Pad - 6.0f * S, Y - 2.0f * S, ListWidth, RowHeight - 4.0f * S, BrewGold, 1.5f * S);
			// a small ember diamond points at the chosen row
			const FVector2D Tip(Left + Pad - 14.0f * S, Y + RowHeight * 0.45f);
			DrawPolygon({ Tip + FVector2D(-6.0f * S, 0.0f), Tip + FVector2D(0.0f, -6.0f * S), Tip + FVector2D(6.0f * S, 0.0f), Tip + FVector2D(0.0f, 6.0f * S) }, BrewEmber);
		}
		DrawFlask(FVector2D(Left + Pad + 14.0f * S, Y + RowHeight * 0.48f), 24.0f * S, bSealed ? BrewRgb(70, 60, 70) : Recipe.Color, 0.0f);
		DrawLabel(bSealed ? LOCTEXT("SealedName", "Versiegelte Seite") : Recipe.Name, Left + Pad + 38.0f * S, Y + 6.0f * S, 17.0f * S,
			bSealed ? BrewSealed : Block == EVaelBrewBlock::None ? BrewBone : BrewDim, 0.0f, ListWidth * 0.5f);
		DrawWaySign(Recipe.Way, FVector2D(Left + Pad + ListWidth * 0.62f, Y + RowHeight * 0.45f), 6.0f * S);

		// one dot per ingredient: green when the bag has it, red when not
		for (int32 Ingredient = 0; Ingredient < Recipe.Ingredients.Num(); ++Ingredient)
		{
			const UVaelMaterial* Loaded = Recipe.Ingredients[Ingredient].LoadSynchronous();
			const bool bHave = Loaded != nullptr && Bag->GetCount(Loaded) > 0;
			DrawDisc(FVector2D(Left + Pad + ListWidth * 0.7f + Ingredient * 12.0f * S, Y + RowHeight * 0.45f), 4.0f * S, bSealed ? BrewAlpha(BrewDim, 0.4f) : bHave ? BrewHave : BrewLack);
		}
		const UVaelMaterial* Result = Recipe.Result.LoadSynchronous();
		const int32 Owned = Result != nullptr ? Bag->GetCount(Result) : 0;
		if (Owned > 0)
		{
			DrawLabel(FText::Format(LOCTEXT("Owned", "×{0}"), Owned), Left + Pad + ListWidth - 14.0f * S, Y + 9.0f * S, 14.0f * S, BrewGold, 1.0f);
		}
	}

	// ---- the chosen recipe on the right: the combination and what it does
	const float DetailLeft = Left + Pad + ListWidth + 30.0f * S;
	const float DetailWidth = Left + Width - Pad - DetailLeft;
	DrawSegment(FVector2D(DetailLeft - 16.0f * S, BodyTop), FVector2D(DetailLeft - 16.0f * S, Top + Height - 50.0f * S), BrewAlpha(BrewGold, 0.35f), 1.0f);
	if (Recipes.IsValidIndex(Selected))
	{
		const FVaelRecipe& Recipe = *Recipes[Selected];
		const EVaelBrewBlock Block = FVaelBrewing::GetBlock(Player, Recipe);
		const bool bSealed = Block == EVaelBrewBlock::SealedShadow || Block == EVaelBrewBlock::SealedTwilight;
		const float SinceBrew = PlayerController->GetSecondsSinceBrew();
		const float Flourish = FMath::Clamp(1.0f - SinceBrew / 1.2f, 0.0f, 1.0f);

		// name and line of tier, craft and way
		DrawFlask(FVector2D(DetailLeft + 38.0f * S, BodyTop + 44.0f * S), 64.0f * S, bSealed ? BrewRgb(70, 60, 70) : Recipe.Color, 0.6f + 0.4f * FMath::Sin(Time * 2.0f) + Flourish * 2.0f);
		DrawLabel(bSealed ? LOCTEXT("SealedTitle", "Versiegelte Seite") : Recipe.Name, DetailLeft + 86.0f * S, BodyTop + 10.0f * S, 30.0f * S, bSealed ? BrewSealed : BrewBone);
		const FText WayName = Recipe.Way == EVaelWay::Light ? LOCTEXT("WayLight", "Licht") : Recipe.Way == EVaelWay::Shadow ? LOCTEXT("WayShadow", "Schatten")
			: Recipe.Way == EVaelWay::Twilight ? LOCTEXT("WayTwilight", "Zwielicht") : LOCTEXT("WayNeutral", "neutral");
		DrawLabel(FText::Format(LOCTEXT("Line", "{0} · Fach {1} · {2}"), FVaelBrewing::GetTierName(Recipe.Tier), FVaelBrewing::GetCraftName(Recipe.Craft), WayName),
			DetailLeft + 86.0f * S, BodyTop + 48.0f * S, 14.0f * S, BrewGold);

		// the combination: field + field + field = potion
		const int32 Count = Recipe.Ingredients.Num();
		const float Half = FMath::Min(34.0f * S, (DetailWidth - 40.0f * S) / (Count + 1) / 2.6f);
		const float Step = Half * 2.6f;
		const float ComboY = BodyTop + 150.0f * S;
		for (int32 Index = 0; Index < Count; ++Index)
		{
			const UVaelMaterial* Ingredient = Recipe.Ingredients[Index].LoadSynchronous();
			const FVector2D Center(DetailLeft + Half + Index * Step, ComboY);
			DrawIngredientField(Ingredient, Center, Half, Ingredient != nullptr ? Bag->GetCount(Ingredient) : 0, 1, S);
			DrawLabel(Ingredient != nullptr ? Ingredient->DisplayName : LOCTEXT("Unknown", "?"), Center.X, Center.Y + Half + 6.0f * S, 12.0f * S, BrewDim, 0.5f, Step - 4.0f * S);
			if (Index < Count - 1)
			{
				DrawLabel(LOCTEXT("Plus", "+"), Center.X + Step * 0.5f, Center.Y - 14.0f * S, 24.0f * S, BrewGold, 0.5f);
			}
		}
		const FVector2D ResultCenter(DetailLeft + Half + Count * Step + Half * 0.3f, ComboY);
		DrawLabel(LOCTEXT("Equals", "="), ResultCenter.X - Step * 0.5f - Half * 0.15f, ComboY - 16.0f * S, 26.0f * S, BrewEmber, 0.5f);
		DrawBox(ResultCenter.X - Half * 1.15f, ComboY - Half * 1.15f, Half * 2.3f, Half * 2.3f, BrewRgb(54, 28, 18, 245));
		DrawFrame(ResultCenter.X - Half * 1.15f, ComboY - Half * 1.15f, Half * 2.3f, Half * 2.3f, Block == EVaelBrewBlock::None ? BrewEmber : BrewGoldDim, 2.0f * S);
		DrawFlask(ResultCenter + FVector2D(0.0f, Half * 0.1f), Half * 1.3f, bSealed ? BrewRgb(70, 60, 70) : Recipe.Color, Flourish * 3.0f);
		if (Flourish > 0.0f)
		{
			DrawRing(ResultCenter, Half * (1.2f + 1.4f * (1.0f - Flourish)), BrewAlpha(BrewEmber, Flourish), 3.0f * S, false);
		}

		// what it does and how long
		float TextY = ComboY + Half + 44.0f * S;
		DrawLabel(LOCTEXT("EffectHead", "WIRKUNG"), DetailLeft, TextY, 12.0f * S, BrewEmber);
		TextY = DrawWrappedLabel(bSealed ? LOCTEXT("SealedEffect", "Die Tinte ist verwischt, nur ein Siegel ist zu erkennen.") : Recipe.Effect, DetailLeft, TextY + 18.0f * S, 17.0f * S, BrewBone, DetailWidth);
		DrawLabel(LOCTEXT("TimeHead", "DAUER"), DetailLeft, TextY + 12.0f * S, 12.0f * S, BrewEmber);
		DrawLabel(Recipe.Duration, DetailLeft, TextY + 30.0f * S, 16.0f * S, BrewBone);
		TextY += 62.0f * S;

		if (Recipe.Craft == EVaelBrewCraft::ThrowAndOil && Recipe.Id != FName(TEXT("Gruppenheilung")) && !UVaelPotionEffects::ThrowsVials(Player))
		{
			DrawWrappedLabel(LOCTEXT("OilForYou", "Für dich ein Öl: auf die Waffe gestrichen, tragen deine Treffer 60 Sekunden lang die Wirkung. Werfen kann nur der Ingenieur."),
				DetailLeft, TextY, 14.0f * S, BrewGold, DetailWidth);
			TextY += 40.0f * S;
		}
		if (FVaelBrewing::IsOwnCraft(Player, Recipe.Craft))
		{
			DrawLabel(LOCTEXT("OwnCraft", "Dein Fach: wirkt 25 % stärker und hält 50 % länger."), DetailLeft, TextY, 14.0f * S, BrewHave);
			TextY += 22.0f * S;
		}
		const FText Why = Block == EVaelBrewBlock::SealedShadow ? LOCTEXT("WhyShadow", "Eine Schatten-Seite. Ein Ordenssiegel vom Orden der Narbe öffnet sie.")
			: Block == EVaelBrewBlock::SealedTwilight ? LOCTEXT("WhyTwilight", "Licht und Schatten mischen sich erst nach dem Schwur, als Zwielicht.")
			: Block == EVaelBrewBlock::Missing ? LOCTEXT("WhyMissing", "Es fehlen Zutaten. Rote Felder zeigen, was der Beutel nicht hat.")
			: LOCTEXT("Ready", "Alles da. Der Kessel ist heiß.");
		DrawWrappedLabel(Why, DetailLeft, TextY, 14.0f * S, Block == EVaelBrewBlock::None ? BrewGold : bSealed ? BrewSealed : BrewLack, DetailWidth);
	}

	DrawLabel(FText::Format(LOCTEXT("Footer", "{0}: brauen · {1}: schließen · {2}: Seite"), ConfirmGlyph(Glyphs), CloseGlyph(Glyphs), TabGlyphs(Glyphs)),
		Left + Width - Pad, Top + Height - 32.0f * S, 13.0f * S, BrewDim, 1.0f);
}

void AVaelHUD::DrawSowing(const AVaelPlayerController* PlayerController)
{
	const AVaelCharacter* Player = PlayerController->GetPawn<AVaelCharacter>();
	if (Player == nullptr)
	{
		return;
	}

	const float S = UiScale;
	const EVaelInputGlyphs Glyphs = PlayerController->GetInputGlyphs();
	const UVaelMaterialBag* Bag = Player->GetMaterialBag();
	const TArray<FName> Choices = PlayerController->GetSowChoices();
	const int32 Selected = FMath::Clamp(PlayerController->GetSowSelection(), 0, FMath::Max(Choices.Num() - 1, 0));

	DrawBox(0.0f, 0.0f, Canvas->ClipX, Canvas->ClipY, BrewRgb(8, 5, 5, 150));
	const float RowHeight = 52.0f * S;
	const float Width = FMath::Min(640.0f * S, Canvas->ClipX - 32.0f * S);
	const float Height = FMath::Min(250.0f * S + FMath::Max(Choices.Num(), 1) * RowHeight, Canvas->ClipY - 32.0f * S);
	const bool bShadowChosen = PlayerController->IsSowingShadow();
	const float Left = (Canvas->ClipX - Width) * 0.5f;
	const float Top = (Canvas->ClipY - Height) * 0.5f;
	const float Pad = 24.0f * S;

	DrawBox(Left, Top, Width, Height, BrewLacquer);
	DrawFrame(Left, Top, Width, Height, BrewGoldDim, 2.0f * S);
	DrawFrame(Left + 6.0f * S, Top + 6.0f * S, Width - 12.0f * S, Height - 12.0f * S, BrewAlpha(BrewGold, 0.55f), 1.0f);
	DrawLabel(LOCTEXT("SowEyebrow", "GELEGEBEET · AUS DEM BEUTEL"), Left + Pad, Top + 20.0f * S, 12.0f * S, BrewEmber);
	DrawLabel(LOCTEXT("SowTitle", "Was soll hier wachsen?"), Left + Pad, Top + 36.0f * S, 28.0f * S, BrewBone);

	for (int32 Index = 0; Index < Choices.Num(); ++Index)
	{
		const FVaelSeedPlant* Plant = UVaelGardenSubsystem::FindPlant(Choices[Index]);
		const UVaelMaterial* Seed = Plant != nullptr ? Plant->Seed.LoadSynchronous() : nullptr;
		if (Plant == nullptr || Seed == nullptr)
		{
			continue;
		}
		const float Y = Top + 92.0f * S + Index * RowHeight;
		const bool bOn = Index == Selected;
		if (bOn)
		{
			DrawBox(Left + Pad - 6.0f * S, Y - 4.0f * S, Width - 2.0f * Pad + 12.0f * S, RowHeight - 6.0f * S, BrewRgb(60, 34, 24, 235));
			DrawFrame(Left + Pad - 6.0f * S, Y - 4.0f * S, Width - 2.0f * Pad + 12.0f * S, RowHeight - 6.0f * S, BrewGold, 1.5f * S);
		}
		DrawIngredientField(Seed, FVector2D(Left + Pad + 20.0f * S, Y + RowHeight * 0.4f), 19.0f * S, Bag->GetCount(Seed), 1, S * 0.7f);
		const bool bRowShadow = bOn && bShadowChosen && Plant->HasShadowForm();
		DrawLabel(bRowShadow ? Plant->ShadowName : Plant->DisplayName, Left + Pad + 52.0f * S, Y + 4.0f * S, 18.0f * S, bRowShadow ? BrewSealed : BrewBone);

		// the two forms as pills: light left, shadow right; the chosen one lit on the chosen row
		for (int32 Form = 0; Form < 2; ++Form)
		{
			const bool bShadowPill = Form == 1;
			const bool bExists = !bShadowPill || Plant->HasShadowForm();
			const bool bPicked = bOn && bShadowPill == bShadowChosen;
			const float PillW = 82.0f * S;
			const float PillX = Left + Width - Pad - 210.0f * S + Form * (PillW + 8.0f * S);
			const FLinearColor PillColor = bShadowPill ? FLinearColor(FColor(162, 77, 255)) : FLinearColor(FColor(240, 200, 110));
			DrawBox(PillX, Y + 8.0f * S, PillW, 24.0f * S, bPicked ? BrewAlpha(PillColor, 0.35f) : BrewLacquerLight);
			DrawFrame(PillX, Y + 8.0f * S, PillW, 24.0f * S, bExists ? (bPicked ? PillColor : BrewAlpha(PillColor, 0.45f)) : BrewAlpha(BrewDim, 0.3f), bPicked ? 2.0f * S : 1.0f);
			DrawLabel(bShadowPill ? LOCTEXT("PillShadow", "Schatten") : LOCTEXT("PillLight", "Licht"), PillX + PillW * 0.5f, Y + 12.0f * S, 13.0f * S,
				bExists ? (bPicked ? BrewBone : BrewDim) : BrewAlpha(BrewDim, 0.4f), 0.5f);
		}
		const FText Rarity = Plant->Rarity == EVaelPlantRarity::Legendary ? LOCTEXT("RarityLegendary", "legendär") : Plant->Rarity == EVaelPlantRarity::Rare ? LOCTEXT("RarityRare", "selten")
			: Plant->Rarity == EVaelPlantRarity::Special ? LOCTEXT("RaritySpecial", "besonders") : LOCTEXT("RarityCommon", "häufig");
		DrawLabel(Rarity, Left + Pad + 52.0f * S, Y + 26.0f * S, 12.0f * S, Plant->Rarity == EVaelPlantRarity::Legendary ? BrewGold : BrewDim);
		DrawLabel(FText::Format(LOCTEXT("SeedCount", "{0} Samen"), Bag->GetCount(Seed)), Left + Pad + 52.0f * S + 150.0f * S, Y + 26.0f * S, 12.0f * S, BrewGold);
	}

	// What the chosen form does: its price on the own balance and on the region (Die Waage)
	const FVaelSeedPlant* Chosen = Choices.IsValidIndex(Selected) ? UVaelGardenSubsystem::FindPlant(Choices[Selected]) : nullptr;
	if (Chosen != nullptr)
	{
		const float InfoY = Top + 100.0f * S + Choices.Num() * RowHeight;
		DrawSegment(FVector2D(Left + Pad, InfoY - 6.0f * S), FVector2D(Left + Width - Pad, InfoY - 6.0f * S), BrewAlpha(BrewGold, 0.4f), 1.0f);
		FText Why;
		const bool bAllowed = UVaelGardenSubsystem::CanSowForm(Player, *Chosen, bShadowChosen, &Why);
		const FText Info = !bAllowed ? Why : bShadowChosen
			? LOCTEXT("InfoShadow", "Schattenform: stärker, aber +3 Verderbnis für dich. Solange sie wächst, hebt sie die Verderbnis der Aschenmark.")
			: LOCTEXT("InfoLight", "Lichtform: kostet nichts. Solange sie wächst, senkt sie die Verderbnis der Aschenmark.");
		DrawWrappedLabel(Info, Left + Pad, InfoY, 14.0f * S, !bAllowed ? BrewLack : bShadowChosen ? BrewSealed : BrewGold, Width - 2.0f * Pad);
		DrawPlayerBalance(Player, LOCTEXT("SowBalance", "Deine Waage"), Left + Pad, InfoY + 48.0f * S, Width - 2.0f * Pad);
	}

	DrawLabel(FText::Format(LOCTEXT("SowFooter", "{0}: säen · links / rechts: Licht oder Schatten · {1}: schließen"), ConfirmGlyph(Glyphs), CloseGlyph(Glyphs)),
		Left + Width - Pad, Top + Height - 30.0f * S, 13.0f * S, BrewDim, 1.0f);
}

#undef LOCTEXT_NAMESPACE
