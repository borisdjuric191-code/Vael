// Copyright Epic Games, Inc. All Rights Reserved.

// The engineer's menu in his own style (Boris 17.11.: "alles in Options außer Gestalt im Ingenieurstil, im HUD-Style vom
// Ingenieur, und seine Tasche sollte kein Buch sein"): the windows of compendium and quests are his workshop, red work leather
// on dark iron with a riveted brass edge and a gear with a coal, like the frame of his HUD (Tools/HUD/vael_hud_gemalt.py,
// frame_engineer); his inventory is an opened toolbox (inventory_engineer): on the lid the Glutzeichnung of his armour with
// the tiles of its places, in the box the pieces he does not wear, to put on and take off like in Diablo.

#include "UI/VaelHUD.h"
#include "Engine/Canvas.h"
#include "Engineer/VaelEngineerComponent.h"
#include "Engineer/VaelEngineerGear.h"
#include "Items/VaelInventory.h"
#include "Items/VaelItemGroups.h"
#include "Items/VaelMaterial.h"
#include "Items/VaelMaterialBag.h"
#include "Player/VaelCharacter.h"
#include "Player/VaelPlayerController.h"

#define LOCTEXT_NAMESPACE "VaelHUDToolbox"

namespace
{
	FLinearColor BoxRgb(uint8 R, uint8 G, uint8 B, uint8 A = 255)
	{
		return FLinearColor(FColor(R, G, B, A));
	}

	const FLinearColor Leather = BoxRgb(46, 13, 9, 246);
	const FLinearColor LeatherLight = BoxRgb(70, 22, 14, 246);
	const FLinearColor Iron = BoxRgb(30, 29, 31);
	const FLinearColor IronLight = BoxRgb(64, 62, 66);
	const FLinearColor Brass = BoxRgb(214, 164, 82);
	const FLinearColor BrassDark = BoxRgb(120, 84, 38);
	const FLinearColor BrassLight = BoxRgb(250, 222, 150);
	const FLinearColor Coal = BoxRgb(255, 120, 40);
	const FLinearColor CoalCore = BoxRgb(255, 214, 140);

	/** Ink on the paper of the toolbox, like on the parchment of the grimoire */
	const FLinearColor PaperInk = BoxRgb(22, 13, 8);
	const FLinearColor PaperFaded = BoxRgb(56, 36, 22);
	const FLinearColor PaperActive = BoxRgb(150, 52, 14);
	const FLinearColor PaperWorn = BoxRgb(30, 96, 40);

	/** The compartments of the toolbox, the same grid as the backpack of the grimoire (scene units of the picture) */
	constexpr float BoxRightX = 0.8f;
	constexpr int32 BoxColumns = 7;
	constexpr float BoxStep = 0.17f;
	constexpr float BoxTop = -0.01f;
	constexpr int32 BoxRows = 5;

	/** The item sign that fits each armour place, by EVaelArmorPlace: helmet, chest, gloves for the arms, boots for the legs,
	 *  the offhand for the shield (indices of ItemIcons, by EVaelItemSlot) */
	constexpr int32 PlaceIcons[] = { 2, 3, 4, 5, 1 };
}

void AVaelHUD::DrawGear(const FVector2D& Center, float Radius, int32 Teeth, const FLinearColor& Color, bool bEmber)
{
	TArray<FVector2D> Outline;
	Outline.Add(Center);
	for (int32 Step = 0; Step <= Teeth * 4; ++Step)
	{
		const float Angle = Step * UE_TWO_PI / (Teeth * 4);
		const float R = (Step % 4 == 1 || Step % 4 == 2) ? Radius : Radius * 0.8f;
		Outline.Add(Center + FVector2D(FMath::Cos(Angle), FMath::Sin(Angle)) * R);
	}
	DrawPolygon(Outline, Color);
	DrawRing(Center, Radius * 0.52f, Color * 0.55f, FMath::Max(1.0f, Radius * 0.1f));
	if (bEmber)
	{
		DrawDisc(Center, Radius * 0.34f, Coal);
		DrawDisc(Center - FVector2D(Radius * 0.08f, Radius * 0.08f), Radius * 0.15f, CoalCore);
	}
	else
	{
		DrawDisc(Center, Radius * 0.22f, BrassDark);
	}
}

void AVaelHUD::DrawEngineerWindow(float X, float Y, float Width, float Height)
{
	const float S = UiScale;
	const float Edge = 9.0f * S;

	// Dark iron round the leather, the leather a little lighter in its middle band
	DrawBox(X - Edge, Y - Edge, Width + 2.0f * Edge, Height + 2.0f * Edge, Iron);
	DrawBox(X, Y, Width, Height, Leather);
	DrawBox(X, Y + Height * 0.18f, Width, Height * 0.64f, WithAlphaOf(LeatherLight, 0.35f));

	// The brass edge: a light line, the brass, a dark line inside; an embossed line further in
	DrawFrame(X - 3.0f * S, Y - 3.0f * S, Width + 6.0f * S, Height + 6.0f * S, Brass, 4.0f * S);
	DrawFrame(X - 4.0f * S, Y - 4.0f * S, Width + 8.0f * S, Height + 8.0f * S, BrassLight, 1.0f);
	DrawFrame(X + 1.0f * S, Y + 1.0f * S, Width - 2.0f * S, Height - 2.0f * S, BrassDark, 1.0f);
	DrawFrame(X + 14.0f * S, Y + 14.0f * S, Width - 28.0f * S, Height - 28.0f * S, BoxRgb(24, 6, 4, 200), 2.0f * S);

	// Rivets along the edge
	const auto Rivet = [this, S](const FVector2D& At)
	{
		DrawDisc(At, 3.6f * S, BrassDark);
		DrawDisc(At - FVector2D(0.5f * S, 0.5f * S), 2.6f * S, Brass);
		DrawDisc(At - FVector2D(1.1f * S, 1.1f * S), 1.0f * S, BrassLight);
	};
	const float Gap = 34.0f * S;
	for (float RX = X + 40.0f * S; RX < X + Width - 36.0f * S; RX += Gap)
	{
		Rivet(FVector2D(RX, Y + 7.0f * S));
		Rivet(FVector2D(RX, Y + Height - 7.0f * S));
	}
	for (float RY = Y + 40.0f * S; RY < Y + Height - 36.0f * S; RY += Gap)
	{
		Rivet(FVector2D(X + 7.0f * S, RY));
		Rivet(FVector2D(X + Width - 7.0f * S, RY));
	}

	// Iron brackets on the corners, two rivets each
	const float Arm = 46.0f * S;
	const float Bar = 9.0f * S;
	for (const FVector2D Sign : { FVector2D(1, 1), FVector2D(-1, 1), FVector2D(1, -1), FVector2D(-1, -1) })
	{
		const FVector2D Corner(Sign.X > 0 ? X - Edge : X + Width + Edge, Sign.Y > 0 ? Y - Edge : Y + Height + Edge);
		DrawBox(Sign.X > 0 ? Corner.X : Corner.X - Arm, Sign.Y > 0 ? Corner.Y : Corner.Y - Bar, Arm, Bar, IronLight);
		DrawBox(Sign.X > 0 ? Corner.X : Corner.X - Bar, Sign.Y > 0 ? Corner.Y : Corner.Y - Arm, Bar, Arm, IronLight);
		Rivet(Corner + FVector2D(Sign.X * (Arm - 8.0f * S), Sign.Y * Bar * 0.5f));
		Rivet(Corner + FVector2D(Sign.X * Bar * 0.5f, Sign.Y * (Arm - 8.0f * S)));
	}

	// The gear with its coal over the middle of the top, a smaller iron one turning behind it
	DrawGear(FVector2D(X + Width * 0.5f - 30.0f * S, Y - 4.0f * S), 15.0f * S, 10, IronLight, false);
	DrawGear(FVector2D(X + Width * 0.5f, Y - 6.0f * S), 22.0f * S, 14, Brass, true);
}

FLinearColor AVaelHUD::WithAlphaOf(FLinearColor Color, float Alpha)
{
	Color.A *= Alpha;
	return Color;
}

UTexture2D* AVaelHUD::GetArmorPicture(const FVaelGear& Piece) const
{
	const int32 Index = static_cast<int32>(Piece.Place);
	return Piece.Set == FName(TEXT("Schwarzglut")) && ArmorPictures.IsValidIndex(Index) ? ArmorPictures[Index].Get() : nullptr;
}

namespace
{
	/** The tiles on the lid by EVaelArmorPlace: middle and size in scene units of the picture (the same numbers as
	 *  ENGINEER_TILES in Tools/HUD/vael_hud_gemalt.py), and the spot of the piece on the Glutzeichnung (0..1 of the picture,
	 *  from its top left) the line from the tile runs to */
	struct FToolboxTile { float X; float Z; float Width; float Height; FVector2D Spot; };
	const FToolboxTile ToolboxTiles[] =
	{
		{ -1.4f, 0.36f, 0.24f, 0.26f, FVector2D(0.5f, 0.13f) },    // helmet: the visor
		{ -0.2f, 0.3f, 0.24f, 0.36f, FVector2D(0.5f, 0.33f) },     // chest
		{ -1.4f, -0.04f, 0.24f, 0.3f, FVector2D(0.09f, 0.44f) },   // arms: the gauntlet on his right
		{ -1.4f, -0.46f, 0.24f, 0.34f, FVector2D(0.375f, 0.71f) }, // legs: the knee
		{ -0.2f, -0.2f, 0.24f, 0.46f, FVector2D(0.71f, 0.2f) },    // shield: it lies on his back, up at the shoulder
	};

	/** The Glutzeichnung on the soot plate: its top, height and the side ratio of the picture (900 x 1100) */
	constexpr float BlueprintTop = 0.54f;
	constexpr float BlueprintHeight = 1.18f;
	constexpr float BlueprintAspect = 900.0f / 1100.0f;
	constexpr float LidX = -0.8f;

	/** The belt under the drawing */
	constexpr float BeltZ = -0.76f;
	constexpr float BeltStep = 0.13f;

	const FLinearColor Copper = BoxRgb(242, 148, 66);
}

void AVaelHUD::DrawToolboxLid(const AVaelPlayerController* PlayerController, TFunctionRef<FVector2D(float, float)> At, float Unit, float T)
{
	const AVaelCharacter* Player = PlayerController->GetPawn<AVaelCharacter>();
	const UVaelEngineerComponent* Engineer = Player != nullptr ? Player->GetEngineer() : nullptr;
	if (Engineer == nullptr)
	{
		return;
	}
	const int32 Selection = PlayerController->GetInventorySelection();
	const bool bArmorTab = !PlayerController->IsMaterialBagShown();
	const float Pulse = 0.5f + 0.5f * FMath::Sin(GetWorld()->GetRealTimeSeconds() * 2.4f);

	// Heading on the soot plate, in brass
	const FVector2D Head = At(LidX, 0.83f);
	DrawTitle(LOCTEXT("LidTitle", "Ausrüstung"), Head.X, Head.Y, 40.0f * T, BrassLight, 0.5f);
	const FVector2D Note = At(LidX, 0.725f);
	DrawLabel(FText::Format(LOCTEXT("LidPlayer", "Spieler {0} · {1} Gildenmünzen"), PlayerController->GetPlayerSlot() + 1, Player->GetInventory()->GetCoins()),
		Note.X, Note.Y, 16.0f * T, Brass, 0.5f);

	// The Glutzeichnung of the whole armour; it glows a little brighter for each piece he wears
	const FVector2D DrawTopLeft = At(LidX - BlueprintHeight * BlueprintAspect * 0.5f, BlueprintTop);
	const FVector2D DrawSize(BlueprintHeight * BlueprintAspect * Unit, BlueprintHeight * Unit);
	const int32 NumWorn = Engineer->CountSet(TEXT("Schwarzglut")) + (Engineer->HasShield() ? 1 : 0);
	if (BlueprintPicture != nullptr)
	{
		Canvas->K2_DrawTexture(BlueprintPicture, DrawTopLeft, DrawSize, FVector2D::ZeroVector, FVector2D::UnitVector,
			FLinearColor(1.0f, 1.0f, 1.0f, 0.55f + 0.08f * NumWorn), BLEND_Translucent);
	}

	// The tiles: a line from each to its piece on the drawing, the piece in the tile if he wears it, else its ghost
	for (int32 Place = 0; Place < UE_ARRAY_COUNT(ToolboxTiles); ++Place)
	{
		const FToolboxTile& Tile = ToolboxTiles[Place];
		const FVaelGear* Worn = VaelEngineerGear::Find(Engineer->GetWorn(static_cast<EVaelArmorPlace>(Place)));
		const FVector2D Centre = At(Tile.X, Tile.Z);
		const FVector2D Half(Tile.Width * 0.5f * Unit, Tile.Height * 0.5f * Unit);
		const bool bLeft = Tile.X < LidX;
		const bool bSelected = bArmorTab && Selection == Place;

		const FVector2D Spot = DrawTopLeft + FVector2D(Tile.Spot.X * DrawSize.X, Tile.Spot.Y * DrawSize.Y);
		const FVector2D Edge = Centre + FVector2D(bLeft ? Half.X : -Half.X, 0.0f);
		const FVector2D Bend = Edge + FVector2D((bLeft ? 1.0f : -1.0f) * 0.05f * Unit, 0.0f);
		const FLinearColor LineColor = Worn != nullptr ? WithAlphaOf(Coal, 0.75f + 0.25f * Pulse) : WithAlphaOf(Copper, 0.35f);
		DrawSegment(Edge, Bend, LineColor, 2.0f * T);
		DrawSegment(Bend, Spot, LineColor, 2.0f * T);
		DrawRing(Spot, 7.0f * T, LineColor, 2.0f * T);
		DrawDisc(Spot, 3.0f * T, Worn != nullptr ? CoalCore : LineColor);

		// what belongs here: the piece itself, worn, or its dark ghost like the empty places of Diablo
		const FVaelGear* Shown = Worn;
		if (Shown == nullptr)
		{
			for (const FVaelGear& Gear : VaelEngineerGear::GetAll())
			{
				if (Gear.Kind == EVaelGearKind::Armor && Gear.Place == static_cast<EVaelArmorPlace>(Place) && GetArmorPicture(Gear) != nullptr)
				{
					Shown = &Gear;
					break;
				}
			}
		}
		if (UTexture2D* Picture = Shown != nullptr ? GetArmorPicture(*Shown) : nullptr)
		{
			const float Size = FMath::Min(Half.X, Half.Y) * 2.0f * 0.98f;
			Canvas->K2_DrawTexture(Picture, Centre - FVector2D(Size * 0.5f), FVector2D(Size), FVector2D::ZeroVector, FVector2D::UnitVector,
				Worn != nullptr ? FLinearColor::White : FLinearColor(0.16f, 0.13f, 0.11f, 0.55f), BLEND_Translucent);
		}
		DrawLabel(VaelEngineerGear::GetPlaceName(static_cast<EVaelArmorPlace>(Place)), Centre.X, Centre.Y + Half.Y - 17.0f * T, 13.0f * T,
			Worn != nullptr ? BrassLight : WithAlphaOf(Brass, 0.6f), 0.5f);
		if (Worn != nullptr)
		{
			DrawFrame(Centre.X - Half.X, Centre.Y - Half.Y, Half.X * 2.0f, Half.Y * 2.0f, WithAlphaOf(Coal, 0.5f + 0.3f * Pulse), 2.0f * T);
		}
		if (bSelected)
		{
			const float Grow = 4.0f * T;
			DrawFrame(Centre.X - Half.X - Grow, Centre.Y - Half.Y - Grow, (Half.X + Grow) * 2.0f, (Half.Y + Grow) * 2.0f, Coal, 3.0f * T);
		}
	}

	// The belt: four potions under the drawing
	const UVaelMaterialBag* Bag = Player->GetMaterialBag();
	for (int32 Place = 0; Place < UVaelMaterialBag::BeltSize; ++Place)
	{
		const FVector2D Centre = At(LidX + (Place - 1.5f) * BeltStep, BeltZ);
		const float HalfSize = 0.045f * Unit;
		const UVaelMaterial* Potion = Bag->GetBelt().IsValidIndex(Place) ? Bag->GetBelt()[Place].Get() : nullptr;
		if (Potion != nullptr)
		{
			FVaelMaterialStack Stack;
			Stack.Material = Potion;
			Stack.Count = Bag->GetCount(Potion);
			DrawBagField(Stack, Centre, HalfSize, T, 0.0f);
		}
		if (Place == Bag->GetActiveBelt())
		{
			DrawFrame(Centre.X - HalfSize, Centre.Y - HalfSize, HalfSize * 2.0f, HalfSize * 2.0f, Coal, 2.0f * T);
		}
	}
}

void AVaelHUD::DrawToolboxArmor(const AVaelPlayerController* PlayerController, TFunctionRef<FVector2D(float, float)> At, float Unit, float T)
{
	const AVaelCharacter* Player = PlayerController->GetPawn<AVaelCharacter>();
	const UVaelEngineerComponent* Engineer = Player != nullptr ? Player->GetEngineer() : nullptr;
	if (Engineer == nullptr)
	{
		return;
	}
	const TArray<const FVaelGear*> Pieces = AVaelPlayerController::GetToolboxList(Engineer);
	const int32 Selection = PlayerController->GetInventorySelection();
	const int32 Tiles = AVaelPlayerController::NumToolboxTiles;

	// The compartments: the pieces he does not wear, each a tile with its picture like the bag of Diablo
	const int32 FirstRow = Selection >= Tiles ? FMath::Max(0, (Selection - Tiles) / BoxColumns - (BoxRows - 1)) : 0;
	for (int32 Field = 0; Field < BoxColumns * BoxRows; ++Field)
	{
		const int32 Index = FirstRow * BoxColumns + Field;
		if (!Pieces.IsValidIndex(Index))
		{
			break;
		}
		const FVaelGear& Piece = *Pieces[Index];
		const FVector2D Centre = At(BoxRightX + (Field % BoxColumns - (BoxColumns - 1) * 0.5f) * BoxStep, BoxTop - (Field / BoxColumns) * BoxStep);
		const float Half = 0.072f * Unit;
		DrawGroupBackdrop(Centre, Half, VaelItemGroups::GetColor(EVaelItemGroup::Armor));
		if (UTexture2D* Picture = GetArmorPicture(Piece))
		{
			Canvas->K2_DrawTexture(Picture, Centre - FVector2D(Half), FVector2D(Half * 2.0f), FVector2D::ZeroVector, FVector2D::UnitVector, FLinearColor::White, BLEND_Translucent);
		}
		else
		{
			DrawBox(Centre.X - Half, Centre.Y - Half, Half * 2.0f, Half * 2.0f, WithAlphaOf(Piece.Color * 0.7f, 0.9f));
		}
		if (Tiles + Index == Selection)
		{
			const float Sel = 0.08f * Unit;
			DrawFrame(Centre.X - Sel, Centre.Y - Sel, Sel * 2.0f, Sel * 2.0f, Coal, 3.0f * T);
		}
	}

	// The manual: the picked piece, its big picture on the left
	const FVaelGear* Piece = Selection < Tiles ? VaelEngineerGear::Find(Engineer->GetWorn(static_cast<EVaelArmorPlace>(Selection)))
		: Pieces.IsValidIndex(Selection - Tiles) ? Pieces[Selection - Tiles] : nullptr;
	const FVector2D PictureAt = At(BoxRightX - 0.68f, 0.5f);
	const float PictureSize = 0.37f * Unit;
	const FVector2D TextAt = At(BoxRightX - 0.27f, 0.5f);
	const float TextWidth = 0.95f * Unit;
	if (Piece == nullptr)
	{
		const FText Empty = Selection < Tiles
			? FText::Format(LOCTEXT("TileEmpty", "{0}: nichts angelegt"), VaelEngineerGear::GetPlaceName(static_cast<EVaelArmorPlace>(Selection)))
			: LOCTEXT("BoxEmpty", "Die Fächer sind leer: alles, was er gebaut hat, trägt er.");
		DrawLabel(Empty, At(BoxRightX, 0.36f).X, At(BoxRightX, 0.36f).Y, 21.0f * T, PaperFaded, 0.5f, 1.2f * Unit);
		if (Selection < Tiles && !Pieces.IsEmpty())
		{
			DrawLabel(LOCTEXT("TileHint", "Aus den Fächern unten anlegen"), At(BoxRightX, 0.28f).X, At(BoxRightX, 0.28f).Y, 15.0f * T, PaperFaded, 0.5f);
		}
		return;
	}
	if (UTexture2D* Picture = GetArmorPicture(*Piece))
	{
		DrawBox(PictureAt.X, PictureAt.Y, PictureSize, PictureSize, BoxRgb(26, 20, 17, 235));
		Canvas->K2_DrawTexture(Picture, PictureAt, FVector2D(PictureSize), FVector2D::ZeroVector, FVector2D::UnitVector, FLinearColor::White, BLEND_Translucent);
		DrawFrame(PictureAt.X, PictureAt.Y, PictureSize, PictureSize, BrassDark, 2.0f * T);
	}
	const bool bWorn = Engineer->GetWorn(Piece->Place) == Piece->Id;
	float Y = TextAt.Y;
	DrawLabel(Piece->Name, TextAt.X, Y, 22.0f * T, PaperInk, 0.0f, TextWidth * 0.72f);
	DrawLabel(bWorn ? LOCTEXT("ArmorWorn", "angelegt") : LOCTEXT("ArmorInBox", "im Kasten"), TextAt.X + TextWidth, Y + 4.0f * T, 15.0f * T, bWorn ? PaperWorn : PaperFaded, 1.0f);
	Y += 27.0f * T;
	const FText Stats = Piece->Place == EVaelArmorPlace::Shield
		? LOCTEXT("ShieldStats", "Schild · hält, was von vorn kommt")
		: FText::Format(LOCTEXT("ArmorStatsBox", "{0} · Rüstung {1} · Leben +{2}"), VaelEngineerGear::GetPlaceName(Piece->Place),
			FText::AsNumber(FMath::RoundToInt(Piece->Armor)), FText::AsNumber(FMath::RoundToInt(Piece->Health)));
	DrawLabel(Stats, TextAt.X, Y, 15.0f * T, PaperFaded);
	Y += 21.0f * T;
	if (!Piece->Text.IsEmpty())
	{
		Y = DrawWrappedLabel(Piece->Text, TextAt.X, Y, 12.5f * T, PaperFaded, TextWidth) + 3.0f * T;
	}
	if (const FVaelGearSet* Set = VaelEngineerGear::FindSet(Piece->Set))
	{
		const int32 Count = Engineer->CountSet(Piece->Set);
		DrawLabel(FText::Format(LOCTEXT("SetCountWhole", "{0} · {1} von 4 gebaut · {2}"), Set->Name, Engineer->CountSetBuilt(Piece->Set, Piece->SetStage),
			Count >= 4 ? LOCTEXT("SetWornWhole", "angelegt") : LOCTEXT("SetOnlyWhole", "nur ganz zu tragen")), TextAt.X, Y, 13.0f * T, PaperInk, 0.0f, TextWidth);
		Y += 17.0f * T;
		Y = DrawWrappedLabel(Set->TwoPieces, TextAt.X, Y, 11.5f * T, Count >= 4 ? PaperActive : PaperFaded, TextWidth);
		Y = DrawWrappedLabel(Set->FourPieces, TextAt.X, Y, 11.5f * T, Count >= 4 ? PaperActive : PaperFaded, TextWidth);
		if (!Set->WithShield.IsEmpty())
		{
			DrawWrappedLabel(Set->WithShield, TextAt.X, Y, 11.5f * T, Count >= 4 && Engineer->HasShield() ? PaperActive : PaperFaded, TextWidth);
		}
	}
}

#undef LOCTEXT_NAMESPACE
