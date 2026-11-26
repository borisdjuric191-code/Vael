// Copyright Epic Games, Inc. All Rights Reserved.

// The engineer's page of building (EVaelMenuPage::Craft): a plan sheet on a dark bench, tabs for weapons, shots, devices and
// armour, the list on the left, the piece with its costs on the right. At the field box by a campfire only shots and devices.

#include "UI/VaelHUD.h"
#include "Engine/Canvas.h"
#include "Engineer/VaelEngineerComponent.h"
#include "Engineer/VaelEngineerGear.h"
#include "Engineer/VaelEngineerIdeas.h"
#include "Compendium/VaelCompendiumSubsystem.h"
#include "Items/VaelMaterial.h"
#include "Items/VaelMaterialBag.h"
#include "Player/VaelCharacter.h"
#include "Player/VaelPlayerController.h"

#define LOCTEXT_NAMESPACE "VaelHUDCrafting"

namespace
{
	FLinearColor CraftRgb(uint8 R, uint8 G, uint8 B, uint8 A = 255)
	{
		return FLinearColor(FColor(R, G, B, A));
	}

	const FLinearColor CraftBench = CraftRgb(24, 19, 16, 246);
	const FLinearColor CraftSheet = CraftRgb(34, 30, 27, 250);
	const FLinearColor CraftCopper = CraftRgb(206, 140, 70);
	const FLinearColor CraftCopperDim = CraftRgb(128, 88, 50);
	const FLinearColor CraftBone = CraftRgb(228, 216, 192);
	const FLinearColor CraftDim = CraftRgb(150, 136, 118);
	const FLinearColor CraftGood = CraftRgb(150, 214, 140);
	const FLinearColor CraftBad = CraftRgb(214, 110, 96);

	FText CraftStateText(const UVaelEngineerComponent* Engineer, const FVaelGear& Gear)
	{
		switch (Gear.Kind)
		{
		case EVaelGearKind::Weapon:
			return Engineer->HasBuilt(Gear.Id) ? (Engineer->GetWeapon() == &Gear ? LOCTEXT("InHand", "in der Hand") : LOCTEXT("Built", "gebaut")) : FText::GetEmpty();
		case EVaelGearKind::Armor:
			return Engineer->GetWorn(Gear.Place) == Gear.Id ? LOCTEXT("Worn", "getragen") : Engineer->HasBuilt(Gear.Id) ? LOCTEXT("BuiltArmor", "gebaut") : FText::GetEmpty();
		case EVaelGearKind::Tool:
			return Engineer->HasBuilt(Gear.Id) ? LOCTEXT("BuiltTool", "dabei") : FText::GetEmpty();
		case EVaelGearKind::Refine:
			return FText::GetEmpty();
		default:
			return FText::Format(LOCTEXT("Carried", "{0} dabei · {1} im Vorrat"), Engineer->GetCarried(Gear.Id), Engineer->GetStock(Gear.Id));
		}
	}
}

void AVaelHUD::DrawCrafting(const AVaelPlayerController* PlayerController)
{
	const AVaelCharacter* Player = PlayerController->GetPawn<AVaelCharacter>();
	const UVaelEngineerComponent* Engineer = Player != nullptr ? Player->GetEngineer() : nullptr;
	if (Engineer == nullptr)
	{
		return;
	}
	const float S = UiScale;
	const bool bWorkbench = PlayerController->IsCraftAtWorkbench();
	const EVaelInputGlyphs Glyphs = PlayerController->GetInputGlyphs();
	const UVaelMaterialBag* Bag = Player->GetMaterialBag();

	DrawBox(0.0f, 0.0f, Canvas->ClipX, Canvas->ClipY, CraftRgb(8, 6, 5, 180));
	const float Width = FMath::Min(1180.0f * S, Canvas->ClipX - 32.0f * S);
	const float Height = FMath::Min(660.0f * S, Canvas->ClipY - 32.0f * S);
	const float Left = (Canvas->ClipX - Width) * 0.5f;
	const float Top = (Canvas->ClipY - Height) * 0.5f;
	const float Pad = 26.0f * S;

	// The bench with rivets in the corners
	DrawBox(Left, Top, Width, Height, CraftBench);
	DrawFrame(Left, Top, Width, Height, CraftCopperDim, 2.0f * S);
	DrawFrame(Left + 6.0f * S, Top + 6.0f * S, Width - 12.0f * S, Height - 12.0f * S, CraftRgb(90, 66, 44, 160), 1.0f);
	for (const FVector2D& Corner : { FVector2D(Left + 12.0f * S, Top + 12.0f * S), FVector2D(Left + Width - 12.0f * S, Top + 12.0f * S),
		FVector2D(Left + 12.0f * S, Top + Height - 12.0f * S), FVector2D(Left + Width - 12.0f * S, Top + Height - 12.0f * S) })
	{
		DrawDisc(Corner, 4.0f * S, CraftCopper);
	}

	DrawLabel(bWorkbench ? LOCTEXT("EyebrowBench", "WERKBANK UND ESSE DES INGENIEURS") : LOCTEXT("EyebrowField", "FELDKASTEN AM LAGERFEUER · LADUNGEN, VORRICHTUNGEN, VEREDELN, MISCHEN"),
		Left + Pad, Top + 20.0f * S, 12.0f * S, CraftCopper);
	DrawLabel(LOCTEXT("Title", "Baupläne"), Left + Pad, Top + 36.0f * S, 34.0f * S, CraftBone);

	// Tabs
	const int32 TabIndex = PlayerController->GetCraftTab();
	const float TabWidth = 92.0f * S;
	const float TabHeight = 30.0f * S;
	const float TabTop = Top + 42.0f * S;
	const float TabsLeft = Left + Width - Pad - VaelCraftTabCount * (TabWidth + 6.0f * S);
	for (int32 Tab = 0; Tab < VaelCraftTabCount; ++Tab)
	{
		const float X = TabsLeft + Tab * (TabWidth + 6.0f * S);
		const bool bOn = Tab == TabIndex;
		const bool bOpen = AVaelPlayerController::IsCraftTabOpen(Tab, bWorkbench);
		DrawBox(X, TabTop, TabWidth, TabHeight, bOn ? CraftRgb(70, 44, 24, 245) : CraftRgb(36, 28, 22, 230));
		DrawFrame(X, TabTop, TabWidth, TabHeight, bOn ? CraftCopper : CraftCopperDim, bOn ? 2.0f * S : 1.0f);
		DrawLabel(VaelEngineerGear::GetTabName(Tab), X + TabWidth * 0.5f, TabTop + 6.0f * S, 14.0f * S,
			bOn ? CraftBone : bOpen ? CraftDim : CraftRgb(80, 70, 60), 0.5f);
	}
	DrawLabel(Glyphs == EVaelInputGlyphs::Keyboard ? LOCTEXT("TabKeys", "A / D") : Glyphs == EVaelInputGlyphs::PlayStation ? LOCTEXT("TabPS", "L1 / R1") : LOCTEXT("TabXbox", "LB / RB"),
		TabsLeft - 14.0f * S, TabTop + 8.0f * S, 12.0f * S, CraftDim, 1.0f);

	const float BodyTop = Top + 96.0f * S;
	DrawBox(Left + Pad, BodyTop - 10.0f * S, Width - 2.0f * Pad, 1.0f, CraftCopperDim);

	if (TabIndex == VaelCraftTabMix)
	{
		DrawCraftMix(PlayerController, Left, Top, Width, Height, BodyTop);
		return;
	}
	if (TabIndex == VaelCraftTabIdeas)
	{
		DrawCraftIdeas(PlayerController, Left, Top, Width, Height, BodyTop);
		return;
	}

	// ---- the list
	const TArray<const FVaelGear*> List = AVaelPlayerController::GetCraftList(TabIndex, Engineer);
	const int32 Selected = FMath::Clamp(PlayerController->GetCraftSelection(), 0, FMath::Max(List.Num() - 1, 0));
	const float ListWidth = Width * 0.4f;
	const float RowHeight = 40.0f * S;
	const int32 MaxRows = FMath::Max(1, FMath::FloorToInt((Height - (BodyTop - Top) - 60.0f * S) / RowHeight));
	const int32 First = FMath::Clamp(Selected - MaxRows / 2, 0, FMath::Max(List.Num() - MaxRows, 0));
	for (int32 Row = 0; Row < MaxRows && First + Row < List.Num(); ++Row)
	{
		const FVaelGear& Gear = *List[First + Row];
		const float Y = BodyTop + Row * RowHeight;
		const bool bSel = First + Row == Selected;
		const UVaelEngineerComponent::EBuildBlock Block = Engineer->GetBuildBlock(Gear, bWorkbench);
		const bool bDone = Block == UVaelEngineerComponent::EBuildBlock::Built;
		DrawBox(Left + Pad, Y, ListWidth, RowHeight - 4.0f * S, bSel ? CraftRgb(60, 40, 26, 240) : CraftSheet);
		if (bSel)
		{
			DrawFrame(Left + Pad, Y, ListWidth, RowHeight - 4.0f * S, CraftCopper, 2.0f * S);
		}
		const bool bNoIdea = Block == UVaelEngineerComponent::EBuildBlock::NeedsIdea;
		DrawBox(Left + Pad + 10.0f * S, Y + 12.0f * S, 12.0f * S, 12.0f * S, bNoIdea ? CraftRgb(70, 60, 52) : Gear.Color);
		DrawLabel(bNoIdea ? LOCTEXT("NoIdeaName", "Noch keine Idee …") : Gear.Name, Left + Pad + 32.0f * S, Y + 4.0f * S, 16.0f * S,
			Block == UVaelEngineerComponent::EBuildBlock::Ok || bDone ? CraftBone : CraftDim, 0.0f, ListWidth * 0.6f);
		const FText State = CraftStateText(Engineer, Gear);
		DrawLabel(State.IsEmpty() ? (Block == UVaelEngineerComponent::EBuildBlock::Ok ? LOCTEXT("CanBuild", "baubar") : bNoIdea ? LOCTEXT("ListNoIdea", "Einfall fehlt") : FText::GetEmpty()) : State,
			Left + Pad + ListWidth - 10.0f * S, Y + 21.0f * S, 11.0f * S, bDone ? CraftGood : Block == UVaelEngineerComponent::EBuildBlock::Ok ? CraftCopper : CraftDim, 1.0f);
		if (Gear.Kind == EVaelGearKind::Armor)
		{
			DrawLabel(VaelEngineerGear::GetPlaceName(Gear.Place), Left + Pad + 32.0f * S, Y + 22.0f * S, 11.0f * S, CraftDim);
		}
	}

	// ---- the piece
	if (!List.IsValidIndex(Selected))
	{
		return;
	}
	const FVaelGear& Gear = *List[Selected];
	const float DetailLeft = Left + Pad + ListWidth + 30.0f * S;
	const float DetailWidth = Left + Width - Pad - DetailLeft;
	float Y = BodyTop;
	DrawBox(DetailLeft - 14.0f * S, BodyTop - 4.0f * S, DetailWidth + 14.0f * S, Height - (BodyTop - Top) - 30.0f * S, CraftSheet);
	const FVaelEngineerIdea* Idea = VaelEngineerIdeas::FindForGear(Gear.Id);
	if (Idea != nullptr && Engineer->GetBuildBlock(Gear, bWorkbench) == UVaelEngineerComponent::EBuildBlock::NeedsIdea)
	{
		DrawLabel(LOCTEXT("RiddleEyebrow", "NOCH KEINE IDEE"), DetailLeft, Y, 12.0f * S, CraftCopper);
		DrawTitle(LOCTEXT("RiddleTitle", "Ein leeres Blatt im Skizzenbuch"), DetailLeft, Y + 16.0f * S, 24.0f * S, CraftBone, 0.0f, DetailWidth);
		Y = DrawWrappedLabel(Idea->Hint, DetailLeft, Y + 56.0f * S, 16.0f * S, CraftDim, DetailWidth - 10.0f * S) + 14.0f * S;
		DrawWrappedLabel(FText::Format(LOCTEXT("RiddleHow", "Ideen kommen von dem, was du triffst: {0} im Kompendium bringt sie."), UVaelCompendiumSubsystem::GetStageWord(Idea->Stage, EVaelCompendiumBook::Bestiary)),
			DetailLeft, Y, 13.0f * S, CraftCopperDim, DetailWidth - 10.0f * S);
		return;
	}
	DrawLabel(Gear.Name, DetailLeft, Y, 26.0f * S, CraftBone, 0.0f, DetailWidth);
	Y += 34.0f * S;
	if (!Gear.Text.IsEmpty())
	{
		Y = DrawWrappedLabel(Gear.Text, DetailLeft, Y, 14.0f * S, CraftDim, DetailWidth - 10.0f * S) + 12.0f * S;
	}
	if (Idea != nullptr && Engineer->HasIdea(Idea->Id))
	{
		DrawLabel(FText::Format(LOCTEXT("FromIdea", "Einfall: {0}"), Idea->Title), DetailLeft, Y, 13.0f * S, CraftCopper, 0.0f, DetailWidth);
		Y += 22.0f * S;
	}

	// What it does
	switch (Gear.Kind)
	{
	case EVaelGearKind::Weapon:
		if (!Gear.UpgradeOf.IsNone())
		{
			DrawLabel(FText::Format(LOCTEXT("UpgradeStats", "Stufe {0} · Schaden ×{1}"), Gear.Tier, FText::AsNumber(FMath::Pow(1.18f, static_cast<float>(Gear.Tier)))),
				DetailLeft, Y, 15.0f * S, CraftCopper);
			Y += 26.0f * S;
			break;
		}
		DrawLabel(FText::Format(LOCTEXT("WeaponStats", "Schaden {0} · alle {1} s · Reichweite {2} m{3}"), FText::AsNumber(FMath::RoundToInt(Gear.Damage)),
			FText::AsNumber(Gear.Interval), FText::AsNumber(FMath::RoundToInt(Gear.Range / 100.0f)), Gear.bPierce ? LOCTEXT("Pierce", " · durchschlägt") : FText::GetEmpty()),
			DetailLeft, Y, 15.0f * S, CraftCopper, 0.0f, DetailWidth);
		Y += 22.0f * S;
		DrawLabel(FText::Format(LOCTEXT("Standard", "Endlose Grundladung: {0} \u00B7 am besten auf {1}\u2013{2} m"), Gear.StandardShot,
			FText::AsNumber(FMath::RoundToInt(Gear.SweetMin / 100.0f)), FText::AsNumber(FMath::RoundToInt(Gear.SweetMax / 100.0f))), DetailLeft, Y, 14.0f * S, CraftDim);
		Y += 26.0f * S;
		break;
	case EVaelGearKind::Ammo:
	{
		Y = DrawWrappedLabel(VaelEngineerGear::GetEffectText(Gear), DetailLeft, Y, 15.0f * S, CraftCopper, DetailWidth - 10.0f * S) + 6.0f * S;
		DrawLabel(FText::Format(LOCTEXT("Batch", "{0} Stück pro Mischung · passt auf jede Waffe · {1} in der Tasche"), Gear.Batch, Engineer->GetCarryLimit(Gear)),
			DetailLeft, Y, 14.0f * S, CraftDim);
		Y += 20.0f * S;
		// the pockets of the Werkrolle: 4 to 8 kinds
		const bool bPocket = Engineer->IsInPocket(Gear.Id);
		DrawLabel(FText::Format(bPocket ? LOCTEXT("InPocket", "In der Werkrolle · Taschen {0} von {1} belegt") : LOCTEXT("NotInPocket", "Nicht in der Werkrolle · Taschen {0} von {1} belegt"),
			Engineer->GetPocketKinds().Num(), Engineer->GetPocketCount()), DetailLeft, Y, 14.0f * S, bPocket ? CraftGood : CraftDim);
		Y += 26.0f * S;
		break;
	}
	case EVaelGearKind::Refine:
	{
		const UVaelMaterial* Made = VaelEngineerGear::LoadMaterial(Gear.Output);
		DrawLabel(FText::Format(LOCTEXT("RefineOut", "Ergibt {0} × {1} · im Beutel: {2}"), Gear.OutputCount, Made != nullptr ? Made->DisplayName : FText::FromName(Gear.Output),
			Made != nullptr && Bag != nullptr ? Bag->GetCount(Made) : 0), DetailLeft, Y, 15.0f * S, CraftCopper);
		Y += 26.0f * S;
		break;
	}
	case EVaelGearKind::Device:
		DrawLabel(FText::Format(LOCTEXT("DeviceBatch", "{0} pro Bau · 3 in der Tasche · bis zu 6 gelegt"), Gear.Batch), DetailLeft, Y, 14.0f * S, CraftDim);
		Y += 26.0f * S;
		break;
	case EVaelGearKind::Armor:
	{
		DrawLabel(FText::Format(LOCTEXT("ArmorStats", "{0} · Rüstung {1} · Leben +{2}"), VaelEngineerGear::GetPlaceName(Gear.Place),
			FText::AsNumber(FMath::RoundToInt(Gear.Armor)), FText::AsNumber(FMath::RoundToInt(Gear.Health))), DetailLeft, Y, 15.0f * S, CraftCopper);
		Y += 24.0f * S;
		if (const FVaelGearSet* Set = VaelEngineerGear::FindSet(Gear.Set))
		{
			const int32 Count = Engineer->CountSet(Gear.Set);
			DrawLabel(FText::Format(LOCTEXT("SetLineWhole", "{0} · {1} von 4 gebaut · nur ganz zu tragen"), Set->Name, Engineer->CountSetBuilt(Gear.Set, Gear.SetStage)), DetailLeft, Y, 14.0f * S, CraftBone);
			Y += 20.0f * S;
			DrawLabel(Set->TwoPieces, DetailLeft + 10.0f * S, Y, 13.0f * S, Count >= 4 ? CraftGood : CraftDim, 0.0f, DetailWidth - 10.0f * S);
			Y += 18.0f * S;
			DrawLabel(Set->FourPieces, DetailLeft + 10.0f * S, Y, 13.0f * S, Count >= 4 ? CraftGood : CraftDim, 0.0f, DetailWidth - 10.0f * S);
			Y += 26.0f * S;
			if (!Set->WithShield.IsEmpty())
			{
				const bool bShield = Engineer->HasShield() && Count >= 4;
				DrawLabel(Set->WithShield, DetailLeft + 10.0f * S, Y - 8.0f * S, 13.0f * S, bShield ? CraftGood : CraftDim, 0.0f, DetailWidth - 10.0f * S);
				Y += 22.0f * S;
			}
		}
		break;
	}
	}

	// Costs: what he has against what it needs
	if (const FVaelGear* Needed = Gear.Requires.IsNone() ? nullptr : VaelEngineerGear::Find(Gear.Requires))
	{
		DrawLabel(FText::Format(LOCTEXT("Requires", "Braucht zuerst: {0}"), Needed->Name), DetailLeft, Y, 13.0f * S,
			Engineer->HasBuilt(Needed->Id) || Engineer->GetCarried(Needed->Id) + Engineer->GetStock(Needed->Id) > 0 ? CraftGood : CraftBad);
		Y += 22.0f * S;
	}
	DrawLabel(LOCTEXT("Costs", "MATERIAL"), DetailLeft, Y, 11.0f * S, CraftCopper);
	Y += 16.0f * S;
	if (Gear.Costs.IsEmpty())
	{
		DrawLabel(LOCTEXT("NoCosts", "von Anfang an dabei"), DetailLeft, Y, 14.0f * S, CraftDim);
		Y += 20.0f * S;
	}
	for (const FVaelGearCost& Cost : Gear.Costs)
	{
		const UVaelMaterial* Material = VaelEngineerGear::LoadMaterial(Cost.Material);
		const int32 Have = Material != nullptr && Bag != nullptr ? Bag->GetCount(Material) : 0;
		DrawBox(DetailLeft, Y + 4.0f * S, 10.0f * S, 10.0f * S, Material != nullptr ? Material->Color : CraftDim);
		DrawLabel(Material != nullptr ? Material->DisplayName : FText::FromName(Cost.Material), DetailLeft + 18.0f * S, Y, 14.0f * S, CraftBone);
		DrawLabel(FText::Format(LOCTEXT("HaveNeed", "{0} / {1}"), Have, Cost.Count), DetailLeft + DetailWidth - 10.0f * S, Y, 14.0f * S, Have >= Cost.Count ? CraftGood : CraftBad, 1.0f);
		Y += 20.0f * S;
	}

	// What pressing confirm does now
	const UVaelEngineerComponent::EBuildBlock Block = Engineer->GetBuildBlock(Gear, bWorkbench);
	const FText Confirm = Glyphs == EVaelInputGlyphs::Keyboard ? LOCTEXT("ConfirmKey", "Enter") : Glyphs == EVaelInputGlyphs::PlayStation ? LOCTEXT("ConfirmPS", "Kreuz") : LOCTEXT("ConfirmXbox", "A");
	FText Action;
	FLinearColor ActionColor = CraftDim;
	switch (Block)
	{
	case UVaelEngineerComponent::EBuildBlock::Ok: Action = FText::Format(LOCTEXT("DoBuild", "{0}: bauen"), Confirm); ActionColor = CraftCopper; break;
	case UVaelEngineerComponent::EBuildBlock::Built:
		Action = Gear.Kind == EVaelGearKind::Armor && Engineer->GetWorn(Gear.Place) != Gear.Id ? FText::Format(LOCTEXT("DoWear", "{0}: anlegen"), Confirm) : LOCTEXT("IsBuilt", "Schon gebaut");
		ActionColor = CraftGood;
		break;
	case UVaelEngineerComponent::EBuildBlock::NeedsFirst: Action = LOCTEXT("BlockFirst", "Erst das Stück davor bauen"); break;
	case UVaelEngineerComponent::EBuildBlock::NeedsWorkbench: Action = LOCTEXT("BlockBench", "Nur an Werkbank oder Esse (Glutkammer, Lager)"); break;
	case UVaelEngineerComponent::EBuildBlock::NeedsPlan: Action = LOCTEXT("BlockPlan", "Bauplan fehlt – Kreaturen, Bruchstücke und Bosse tragen ihn"); break;
	case UVaelEngineerComponent::EBuildBlock::NeedsIdea: Action = LOCTEXT("BlockIdea", "Dazu fehlt dir noch die Idee"); break;
	default: Action = LOCTEXT("BlockMaterials", "Es fehlen Materialien"); ActionColor = CraftBad; break;
	}
	DrawLabel(Action, DetailLeft, Top + Height - 70.0f * S, 18.0f * S, ActionColor);
	const bool bPad = Glyphs != EVaelInputGlyphs::Keyboard;
	DrawLabel(Gear.Kind == EVaelGearKind::Ammo
		? FText::Format(LOCTEXT("PocketKeys", "{0}: in eine Tasche der Werkrolle   ·   {1}: zurück in den Rucksack   ·   Neue Ladungen findest du unter „Mischen“."),
			bPad ? LOCTEXT("PadUpC", "Steuerkreuz hoch") : LOCTEXT("KeyOneC", "1"), bPad ? LOCTEXT("PadDownC", "Steuerkreuz runter") : LOCTEXT("KeyThreeC", "3"))
		: LOCTEXT("Pouches", "Was nicht in die Taschen passt, kommt in den Vorrat und füllt sie hier wieder auf."),
		Left + Pad, Top + Height - 34.0f * S, 12.0f * S, CraftDim, 0.0f, Width - 2.0f * Pad);
}

void AVaelHUD::DrawCraftMix(const AVaelPlayerController* PlayerController, float Left, float Top, float Width, float Height, float BodyTop)
{
	// The Mischen tab (goal 14.11.): on the left the materials of formulas he has, picked ones marked; row 0 mixes. On the right
	// what is picked, the hint of the last try and how many formulas he has found.
	const AVaelCharacter* Player = PlayerController->GetPawn<AVaelCharacter>();
	const UVaelEngineerComponent* Engineer = Player != nullptr ? Player->GetEngineer() : nullptr;
	const UVaelMaterialBag* Bag = Player != nullptr ? Player->GetMaterialBag() : nullptr;
	if (Engineer == nullptr)
	{
		return;
	}
	const float S = UiScale;
	const float Pad = 26.0f * S;
	const EVaelInputGlyphs Glyphs = PlayerController->GetInputGlyphs();
	const TArray<FName> List = AVaelPlayerController::GetMixList(Player);
	const TArray<FName>& Picks = PlayerController->GetMixPicks();
	const int32 Rows = List.Num() + 1;
	const int32 Selected = FMath::Clamp(PlayerController->GetCraftSelection(), 0, Rows - 1);
	const float ListWidth = Width * 0.4f;
	const float RowHeight = 34.0f * S;
	const int32 MaxRows = FMath::Max(1, FMath::FloorToInt((Height - (BodyTop - Top) - 60.0f * S) / RowHeight));
	const int32 First = FMath::Clamp(Selected - MaxRows / 2, 0, FMath::Max(Rows - MaxRows, 0));
	for (int32 Row = 0; Row < MaxRows && First + Row < Rows; ++Row)
	{
		const int32 Index = First + Row;
		const float Y = BodyTop + Row * RowHeight;
		const bool bSel = Index == Selected;
		DrawBox(Left + Pad, Y, ListWidth, RowHeight - 4.0f * S, bSel ? CraftRgb(60, 40, 26, 240) : CraftSheet);
		if (bSel)
		{
			DrawFrame(Left + Pad, Y, ListWidth, RowHeight - 4.0f * S, CraftCopper, 2.0f * S);
		}
		if (Index == 0)
		{
			DrawLabel(FText::Format(LOCTEXT("MixRow", "▶ Mischen ({0} von 4 Teilen)"), Picks.Num()), Left + Pad + 12.0f * S, Y + 5.0f * S, 16.0f * S,
				Picks.Num() >= 2 ? CraftCopper : CraftDim);
			continue;
		}
		const FName Material = List[Index - 1];
		const UVaelMaterial* Asset = VaelEngineerGear::LoadMaterial(Material);
		const bool bPicked = Picks.Contains(Material);
		DrawBox(Left + Pad + 10.0f * S, Y + 10.0f * S, 12.0f * S, 12.0f * S, Asset != nullptr ? Asset->Color : CraftDim);
		if (bPicked)
		{
			DrawFrame(Left + Pad + 6.0f * S, Y + 6.0f * S, 20.0f * S, 20.0f * S, CraftGood, 2.0f * S);
		}
		DrawLabel(Asset != nullptr ? Asset->DisplayName : FText::FromName(Material), Left + Pad + 32.0f * S, Y + 5.0f * S, 15.0f * S, bPicked ? CraftGood : CraftBone, 0.0f, ListWidth * 0.55f);
		DrawLabel(FText::Format(LOCTEXT("MixHave", "{0} · {1}"), VaelEngineerGear::GetMixRole(Material), Asset != nullptr && Bag != nullptr ? Bag->GetCount(Asset) : 0),
			Left + Pad + ListWidth - 10.0f * S, Y + 8.0f * S, 11.0f * S, CraftDim, 1.0f);
	}
	if (List.IsEmpty())
	{
		DrawLabel(LOCTEXT("MixEmpty", "Noch nichts zum Mischen im Beutel: Hüllen (Schlacke, Latex, Blasenhaut …) und Kerne sammeln oder veredeln."),
			Left + Pad, BodyTop + RowHeight + 6.0f * S, 13.0f * S, CraftDim, 0.0f, ListWidth);
	}

	// ---- right side: the picked parts, the last hint, the count of formulas
	const float DetailLeft = Left + Pad + ListWidth + 30.0f * S;
	const float DetailWidth = Left + Width - Pad - DetailLeft;
	DrawBox(DetailLeft - 14.0f * S, BodyTop - 4.0f * S, DetailWidth + 14.0f * S, Height - (BodyTop - Top) - 30.0f * S, CraftSheet);
	float Y = BodyTop;
	DrawLabel(LOCTEXT("MixTitle", "Mischen"), DetailLeft, Y, 26.0f * S, CraftBone);
	Y += 34.0f * S;
	Y = DrawWrappedLabel(LOCTEXT("MixRule", "Eine Hülle, ein Kern, bis zu zwei Verstärker. Die Hülle muss zum Inhalt passen. Was nicht passt, kostet nichts."),
		DetailLeft, Y, 13.0f * S, CraftDim, DetailWidth - 10.0f * S) + 12.0f * S;
	DrawLabel(LOCTEXT("MixPicked", "AUF DEM TISCH"), DetailLeft, Y, 11.0f * S, CraftCopper);
	Y += 18.0f * S;
	if (Picks.IsEmpty())
	{
		DrawLabel(LOCTEXT("MixNone", "noch nichts gewählt"), DetailLeft, Y, 14.0f * S, CraftDim);
		Y += 20.0f * S;
	}
	for (const FName Material : Picks)
	{
		const UVaelMaterial* Asset = VaelEngineerGear::LoadMaterial(Material);
		DrawBox(DetailLeft, Y + 4.0f * S, 10.0f * S, 10.0f * S, Asset != nullptr ? Asset->Color : CraftDim);
		DrawLabel(FText::Format(LOCTEXT("MixPart", "{0}  ({1})"), Asset != nullptr ? Asset->DisplayName : FText::FromName(Material), VaelEngineerGear::GetMixRole(Material)),
			DetailLeft + 18.0f * S, Y, 14.0f * S, CraftBone);
		Y += 20.0f * S;
	}
	if (Picks.Num() >= 2 && Engineer->WasTried(Picks))
	{
		DrawLabel(LOCTEXT("MixWasTried", "Das hast du schon versucht."), DetailLeft, Y + 4.0f * S, 13.0f * S, CraftBad);
		Y += 22.0f * S;
	}
	const FText Hint = PlayerController->GetMixHintText();
	if (!Hint.IsEmpty())
	{
		Y += 8.0f * S;
		Y = DrawWrappedLabel(Hint, DetailLeft, Y, 14.0f * S, CraftCopper, DetailWidth - 10.0f * S) + 8.0f * S;
	}
	int32 NumFormulas = 0;
	for (const FVaelGear& Gear : VaelEngineerGear::GetAll())
	{
		NumFormulas += Gear.IsFormula() ? 1 : 0;
	}
	DrawLabel(FText::Format(LOCTEXT("MixFound", "Gefundene Formeln: {0} von {1}"), Engineer->GetNumKnownFormulas(), NumFormulas),
		DetailLeft, Top + Height - 70.0f * S, 16.0f * S, CraftBone);
	const bool bPad = Glyphs != EVaelInputGlyphs::Keyboard;
	const FText Confirm = !bPad ? LOCTEXT("MixKeyEnter", "Enter") : Glyphs == EVaelInputGlyphs::PlayStation ? LOCTEXT("MixKeyCross", "Kreuz") : LOCTEXT("MixKeyA", "A");
	DrawLabel(FText::Format(LOCTEXT("MixKeys", "{0}: Teil wählen oder zurücklegen, in der ersten Zeile mischen   ·   {1}: alles zurücklegen"),
		Confirm, bPad ? LOCTEXT("MixPadDown", "Steuerkreuz runter") : LOCTEXT("MixKeyThree", "3")), Left + Pad, Top + Height - 34.0f * S, 12.0f * S, CraftDim, 0.0f, Width - 2.0f * Pad);
}

void AVaelHUD::DrawCraftIdeas(const AVaelPlayerController* PlayerController, float Left, float Top, float Width, float Height, float BodyTop)
{
	const AVaelCharacter* Player = PlayerController->GetPawn<AVaelCharacter>();
	const UVaelEngineerComponent* Engineer = Player != nullptr ? Player->GetEngineer() : nullptr;
	const UVaelCompendiumSubsystem* Compendium = UVaelCompendiumSubsystem::Get(this);
	if (Engineer == nullptr)
	{
		return;
	}
	const float S = UiScale;
	const float Pad = 26.0f * S;
	const TArray<FVaelEngineerIdea>& Ideas = VaelEngineerIdeas::GetAll();
	const int32 Selected = FMath::Clamp(PlayerController->GetCraftSelection(), 0, FMath::Max(Ideas.Num() - 1, 0));

	// The sketchbook: the ideas on the left, ruled like a notebook, the hidden ones as empty pages
	const float ListWidth = Width * 0.4f;
	const float RowHeight = 40.0f * S;
	const int32 MaxRows = FMath::Max(1, FMath::FloorToInt((Height - (BodyTop - Top) - 60.0f * S) / RowHeight));
	const int32 First = FMath::Clamp(Selected - MaxRows / 2, 0, FMath::Max(Ideas.Num() - MaxRows, 0));
	for (int32 Row = 0; Row < MaxRows && First + Row < Ideas.Num(); ++Row)
	{
		const FVaelEngineerIdea& Idea = Ideas[First + Row];
		const bool bHas = Engineer->HasIdea(Idea.Id);
		const bool bSel = First + Row == Selected;
		const float Y = BodyTop + Row * RowHeight;
		DrawBox(Left + Pad, Y, ListWidth, RowHeight - 4.0f * S, bSel ? CraftRgb(60, 40, 26, 240) : CraftSheet);
		if (bSel)
		{
			DrawFrame(Left + Pad, Y, ListWidth, RowHeight - 4.0f * S, CraftCopper, 2.0f * S);
		}
		// a small cog: lit for an idea he has, dark for an empty page
		const FVector2D Cog(Left + Pad + 16.0f * S, Y + 18.0f * S);
		for (int32 Tooth = 0; Tooth < 8; ++Tooth)
		{
			const float Angle = Tooth * UE_TWO_PI / 8.0f;
			DrawDisc(Cog + FVector2D(FMath::Cos(Angle), FMath::Sin(Angle)) * 7.0f * S, 2.2f * S, bHas ? CraftCopper : CraftRgb(70, 60, 52));
		}
		DrawDisc(Cog, 6.5f * S, bHas ? CraftCopper : CraftRgb(70, 60, 52));
		DrawDisc(Cog, 2.5f * S, bHas ? CraftRgb(255, 220, 150) : CraftSheet);
		DrawLabel(bHas ? Idea.Title : LOCTEXT("IdeaHidden", "Leeres Blatt"), Left + Pad + 34.0f * S, Y + 4.0f * S, 16.0f * S, bHas ? CraftBone : CraftDim, 0.0f, ListWidth - 44.0f * S);
		const UVaelCompendiumEntry* Entry = Compendium != nullptr ? Compendium->FindEntry(Idea.Subject) : nullptr;
		const bool bKnown = Compendium != nullptr && Compendium->GetProgress(Idea.Subject).Stage != EVaelResearchStage::Unknown;
		const FText Source = Entry != nullptr && bKnown
			? FText::Format(LOCTEXT("IdeaSource", "{0} · {1}"), Entry->DisplayName, UVaelCompendiumSubsystem::GetStageWord(Idea.Stage, Entry->Book))
			: LOCTEXT("IdeaSourceUnknown", "etwas, dem du noch nicht begegnet bist");
		DrawLabel(Source, Left + Pad + 34.0f * S, Y + 22.0f * S, 11.0f * S, CraftDim, 0.0f, ListWidth - 44.0f * S);
	}
	int32 Had = 0;
	for (const FVaelEngineerIdea& Idea : Ideas)
	{
		Had += Engineer->HasIdea(Idea.Id) ? 1 : 0;
	}
	DrawLabel(FText::Format(LOCTEXT("IdeaCount", "{0} von {1} Ideen der Aschenmark"), Had, Ideas.Num()), Left + Pad, Top + Height - 34.0f * S, 12.0f * S, CraftDim);

	// The page of the idea
	if (!Ideas.IsValidIndex(Selected))
	{
		return;
	}
	const FVaelEngineerIdea& Idea = Ideas[Selected];
	const bool bHas = Engineer->HasIdea(Idea.Id);
	const float DetailLeft = Left + Pad + ListWidth + 30.0f * S;
	const float DetailWidth = Left + Width - Pad - DetailLeft;
	DrawBox(DetailLeft - 14.0f * S, BodyTop - 4.0f * S, DetailWidth + 14.0f * S, Height - (BodyTop - Top) - 30.0f * S, CraftRgb(40, 34, 28, 250));
	// ruled lines like a page of his notebook
	for (float Line = BodyTop + 70.0f * S; Line < Top + Height - 50.0f * S; Line += 24.0f * S)
	{
		DrawBox(DetailLeft - 6.0f * S, Line, DetailWidth, 1.0f, CraftRgb(70, 58, 46, 120));
	}
	float Y = BodyTop + 4.0f * S;
	DrawLabel(bHas ? LOCTEXT("IdeaPageHas", "EINFALL") : LOCTEXT("IdeaPageNot", "NOCH OFFEN"), DetailLeft, Y, 12.0f * S, CraftCopper);
	DrawTitle(bHas ? Idea.Title : LOCTEXT("IdeaPageEmpty", "Ein leeres Blatt"), DetailLeft, Y + 16.0f * S, 26.0f * S, CraftBone, 0.0f, DetailWidth);
	Y += 62.0f * S;
	Y = DrawWrappedLabel(bHas ? Idea.Thought : Idea.Hint, DetailLeft, Y, 16.0f * S, bHas ? CraftBone : CraftDim, DetailWidth - 10.0f * S) + 18.0f * S;

	DrawLabel(LOCTEXT("IdeaGear", "DARAUS WIRD"), DetailLeft, Y, 11.0f * S, CraftCopper);
	Y += 18.0f * S;
	for (const FName Id : Idea.Gear)
	{
		const FVaelGear* Gear = VaelEngineerGear::Find(Id);
		if (Gear == nullptr)
		{
			continue;
		}
		const bool bBuilt = Engineer->HasBuilt(Id);
		DrawBox(DetailLeft, Y + 4.0f * S, 10.0f * S, 10.0f * S, bHas ? Gear->Color : CraftRgb(70, 60, 52));
		DrawLabel(bHas ? Gear->Name : FText::Format(LOCTEXT("IdeaGearHidden", "ein Stück für: {0}"), VaelEngineerGear::GetKindName(Gear->Kind)), DetailLeft + 18.0f * S, Y, 15.0f * S, bHas ? CraftBone : CraftDim);
		DrawLabel(bBuilt ? LOCTEXT("IdeaBuilt", "gebaut") : bHas ? LOCTEXT("IdeaBuildable", "an Werkbank und Esse") : FText::GetEmpty(),
			DetailLeft + DetailWidth - 10.0f * S, Y + 2.0f * S, 12.0f * S, bBuilt ? CraftGood : CraftCopper, 1.0f);
		Y += 22.0f * S;
	}
}

#undef LOCTEXT_NAMESPACE
