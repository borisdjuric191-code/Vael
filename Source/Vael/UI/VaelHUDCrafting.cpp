// Copyright Epic Games, Inc. All Rights Reserved.

// The engineer's page of building (EVaelMenuPage::Craft): a plan sheet on a dark bench, tabs for weapons, shots, devices and
// armour, the list on the left, the piece with its costs on the right. At the field box by a campfire only shots and devices.

#include "UI/VaelHUD.h"
#include "Engine/Canvas.h"
#include "Engineer/VaelEngineerComponent.h"
#include "Engineer/VaelEngineerGear.h"
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
		default:
			return FText::Format(LOCTEXT("Carried", "{0} dabei · {1} im Vorrat"), Engineer->GetCarried(Gear.Id), Engineer->GetStock(Gear.Id));
		}
	}

	FText EffectText(EVaelAmmoEffect Effect)
	{
		switch (Effect)
		{
		case EVaelAmmoEffect::Shards: return LOCTEXT("EffShards", "Splitter treffen alles im Umkreis (60 %)");
		case EVaelAmmoEffect::Fire: return LOCTEXT("EffFire", "Brand: 5 Sekunden Feuer");
		case EVaelAmmoEffect::Poison: return LOCTEXT("EffPoison", "Gift: 6 Sekunden Schaden, das Ziel taumelt");
		case EVaelAmmoEffect::Acid: return LOCTEXT("EffAcid", "Säure: 8 Sekunden mehr Schaden von allem");
		case EVaelAmmoEffect::Heal: return LOCTEXT("EffHeal", "Heilt die Gefährten am Aufschlag um 22");
		case EVaelAmmoEffect::Heavy: return LOCTEXT("EffHeavy", "Wirft zurück und betäubt 0,8 Sekunden");
		case EVaelAmmoEffect::Sticky: return LOCTEXT("EffSticky", "6 Sekunden halb so schnell");
		case EVaelAmmoEffect::Smoke: return LOCTEXT("EffSmoke", "Rauchwolke: Gegner darin taumeln 4 Sekunden");
		case EVaelAmmoEffect::Crackle: return LOCTEXT("EffCrackle", "Feuerkette um das Ziel, zündet Minen in 4 m");
		case EVaelAmmoEffect::Sleep: return LOCTEXT("EffSleep", "Schwache Gegner schlafen 5 Sekunden, starke werden träge");
		case EVaelAmmoEffect::Glow: return LOCTEXT("EffGlow", "Leuchtet 10 Sekunden, nimmt mehr Schaden");
		case EVaelAmmoEffect::Thorn: return LOCTEXT("EffThorn", "Blutet 6 Sekunden");
		case EVaelAmmoEffect::Dew: return LOCTEXT("EffDew", "Nass: Feuer aus, Blitz trifft doppelt");
		case EVaelAmmoEffect::Nid: return LOCTEXT("EffNid", "Doppelter Schaden, +1,5 Nid je Schuss");
		default: return FText::GetEmpty();
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

	DrawLabel(bWorkbench ? LOCTEXT("EyebrowBench", "WERKBANK DES INGENIEURS · GLUTKAMMER") : LOCTEXT("EyebrowField", "FELDKASTEN AM LAGERFEUER · NUR LADUNGEN UND VORRICHTUNGEN"),
		Left + Pad, Top + 20.0f * S, 12.0f * S, CraftCopper);
	DrawLabel(LOCTEXT("Title", "Baupläne"), Left + Pad, Top + 36.0f * S, 34.0f * S, CraftBone);

	// Tabs
	const int32 TabIndex = PlayerController->GetCraftTab();
	const float TabWidth = 128.0f * S;
	const float TabHeight = 30.0f * S;
	const float TabTop = Top + 42.0f * S;
	const float TabsLeft = Left + Width - Pad - 5 * (TabWidth + 6.0f * S);
	for (int32 Tab = 0; Tab < 5; ++Tab)
	{
		const float X = TabsLeft + Tab * (TabWidth + 6.0f * S);
		const bool bOn = Tab == TabIndex;
		const bool bOpen = bWorkbench || Tab == 1 || Tab == 2;
		DrawBox(X, TabTop, TabWidth, TabHeight, bOn ? CraftRgb(70, 44, 24, 245) : CraftRgb(36, 28, 22, 230));
		DrawFrame(X, TabTop, TabWidth, TabHeight, bOn ? CraftCopper : CraftCopperDim, bOn ? 2.0f * S : 1.0f);
		DrawLabel(VaelEngineerGear::GetKindName(static_cast<EVaelGearKind>(Tab)), X + TabWidth * 0.5f, TabTop + 6.0f * S, 15.0f * S,
			bOn ? CraftBone : bOpen ? CraftDim : CraftRgb(80, 70, 60), 0.5f);
	}
	DrawLabel(Glyphs == EVaelInputGlyphs::Keyboard ? LOCTEXT("TabKeys", "A / D") : Glyphs == EVaelInputGlyphs::PlayStation ? LOCTEXT("TabPS", "L1 / R1") : LOCTEXT("TabXbox", "LB / RB"),
		TabsLeft - 14.0f * S, TabTop + 8.0f * S, 12.0f * S, CraftDim, 1.0f);

	const float BodyTop = Top + 96.0f * S;
	DrawBox(Left + Pad, BodyTop - 10.0f * S, Width - 2.0f * Pad, 1.0f, CraftCopperDim);

	// ---- the list
	const TArray<const FVaelGear*> List = AVaelPlayerController::GetCraftList(TabIndex);
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
		DrawBox(Left + Pad + 10.0f * S, Y + 12.0f * S, 12.0f * S, 12.0f * S, Gear.Color);
		DrawLabel(Gear.Name, Left + Pad + 32.0f * S, Y + 4.0f * S, 16.0f * S,
			Block == UVaelEngineerComponent::EBuildBlock::Ok || bDone ? CraftBone : CraftDim, 0.0f, ListWidth * 0.6f);
		const FText State = CraftStateText(Engineer, Gear);
		DrawLabel(State.IsEmpty() ? (Block == UVaelEngineerComponent::EBuildBlock::Ok ? LOCTEXT("CanBuild", "baubar") : FText::GetEmpty()) : State,
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
	DrawLabel(Gear.Name, DetailLeft, Y, 26.0f * S, CraftBone, 0.0f, DetailWidth);
	Y += 34.0f * S;
	if (!Gear.Text.IsEmpty())
	{
		Y = DrawWrappedLabel(Gear.Text, DetailLeft, Y, 14.0f * S, CraftDim, DetailWidth - 10.0f * S) + 12.0f * S;
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
		DrawLabel(EffectText(Gear.Effect), DetailLeft, Y, 15.0f * S, CraftCopper, 0.0f, DetailWidth);
		Y += 22.0f * S;
		DrawLabel(FText::Format(LOCTEXT("Batch", "{0} Stück pro Bau · passt auf jede Waffe · 30 in der Tasche"), Gear.Batch), DetailLeft, Y, 14.0f * S, CraftDim);
		Y += 26.0f * S;
		break;
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
			DrawLabel(FText::Format(LOCTEXT("SetLine", "{0} · {1} von 4 getragen"), Set->Name, Count), DetailLeft, Y, 14.0f * S, CraftBone);
			Y += 20.0f * S;
			DrawLabel(Set->TwoPieces, DetailLeft + 10.0f * S, Y, 13.0f * S, Count >= 2 ? CraftGood : CraftDim, 0.0f, DetailWidth - 10.0f * S);
			Y += 18.0f * S;
			DrawLabel(Set->FourPieces, DetailLeft + 10.0f * S, Y, 13.0f * S, Count >= 4 ? CraftGood : CraftDim, 0.0f, DetailWidth - 10.0f * S);
			Y += 26.0f * S;
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
	case UVaelEngineerComponent::EBuildBlock::NeedsWorkbench: Action = LOCTEXT("BlockBench", "Nur an der Werkbank in der Glutkammer"); break;
	case UVaelEngineerComponent::EBuildBlock::NeedsPlan: Action = LOCTEXT("BlockPlan", "Bauplan fehlt – Kreaturen, Bruchstücke und Bosse tragen ihn"); break;
	default: Action = LOCTEXT("BlockMaterials", "Es fehlen Materialien"); ActionColor = CraftBad; break;
	}
	DrawLabel(Action, DetailLeft, Top + Height - 70.0f * S, 18.0f * S, ActionColor);
	DrawLabel(LOCTEXT("Pouches", "Was nicht in die Taschen passt, kommt in den Vorrat und füllt sie hier wieder auf."),
		Left + Pad, Top + Height - 34.0f * S, 12.0f * S, CraftDim, 0.0f, Width - 2.0f * Pad);
}

#undef LOCTEXT_NAMESPACE
