// Copyright Epic Games, Inc. All Rights Reserved.

// The engineer's Werkrolle (EVaelMenuPage::Werkrolle, goal 8.11. evening): his leather tool roll unrolled across the screen,
// the rolled ends at both sides, a stitched pocket for every kind of shot and device. Each pocket shows what is in it, what
// waits in the backpack and what is in the weapon; below, the picked one with what it does and what it costs to make in the
// field. Like the grimoire of the mage and the Keimgeflecht of the Floromant, it is what his menu key opens.

#include "UI/VaelHUD.h"
#include "Engine/Canvas.h"
#include "Engineer/VaelEngineerComponent.h"
#include "Engineer/VaelEngineerGear.h"
#include "Items/VaelMaterial.h"
#include "Items/VaelMaterialBag.h"
#include "Player/VaelCharacter.h"
#include "Player/VaelPlayerController.h"

#define LOCTEXT_NAMESPACE "VaelHUDWerkrolle"

namespace
{
	FLinearColor RollRgb(uint8 R, uint8 G, uint8 B, uint8 A = 255)
	{
		return FLinearColor(FColor(R, G, B, A));
	}

	const FLinearColor RollLeather = RollRgb(92, 54, 30, 250);
	const FLinearColor RollLeatherDark = RollRgb(58, 33, 18, 250);
	const FLinearColor RollLeatherLight = RollRgb(122, 76, 44, 250);
	const FLinearColor RollBrass = RollRgb(214, 164, 82);
	const FLinearColor RollBrassDim = RollRgb(132, 98, 52);
	const FLinearColor RollThread = RollRgb(222, 196, 150, 200);
	const FLinearColor RollBone = RollRgb(236, 224, 200);
	const FLinearColor RollDim = RollRgb(176, 150, 122);
	const FLinearColor RollGood = RollRgb(160, 220, 150);
	const FLinearColor RollBad = RollRgb(222, 120, 104);
}

void AVaelHUD::DrawWerkrolle(const AVaelPlayerController* PlayerController)
{
	const AVaelCharacter* Player = PlayerController->GetPawn<AVaelCharacter>();
	const UVaelEngineerComponent* Engineer = Player != nullptr ? Player->GetEngineer() : nullptr;
	if (Engineer == nullptr)
	{
		return;
	}
	const float S = UiScale;
	const EVaelInputGlyphs Glyphs = PlayerController->GetInputGlyphs();
	const UVaelMaterialBag* Bag = Player->GetMaterialBag();
	const TArray<const FVaelGear*> List = AVaelPlayerController::GetWerkrolleList();
	const int32 Selected = FMath::Clamp(PlayerController->GetWerkrolleSelection(), 0, FMath::Max(List.Num() - 1, 0));

	DrawBox(0.0f, 0.0f, Canvas->ClipX, Canvas->ClipY, RollRgb(10, 7, 5, 175));
	const float Width = FMath::Min(1280.0f * S, Canvas->ClipX - 40.0f * S);
	const float RollHeight = 300.0f * S;
	const float Left = (Canvas->ClipX - Width) * 0.5f;
	const float Top = Canvas->ClipY * 0.5f - RollHeight * 0.75f;

	// The leather: a lighter middle band, darker edges, a stitched seam inside each edge
	DrawBox(Left, Top, Width, RollHeight, RollLeather);
	DrawBox(Left, Top, Width, 18.0f * S, RollLeatherDark);
	DrawBox(Left, Top + RollHeight - 18.0f * S, Width, 18.0f * S, RollLeatherDark);
	for (float X = Left + 46.0f * S; X < Left + Width - 46.0f * S; X += 16.0f * S)
	{
		DrawBox(X, Top + 24.0f * S, 8.0f * S, 2.0f * S, RollThread);
		DrawBox(X, Top + RollHeight - 26.0f * S, 8.0f * S, 2.0f * S, RollThread);
	}
	// The rolled ends: cylinders of leather shaded from dark to light to dark, brass caps
	for (const float EndX : { Left - 18.0f * S, Left + Width - 30.0f * S })
	{
		const int32 Bands = 8;
		for (int32 Band = 0; Band < Bands; ++Band)
		{
			const float T = FMath::Sin(UE_PI * (Band + 0.5f) / Bands);
			DrawBox(EndX + Band * 6.0f * S, Top - 10.0f * S, 6.0f * S, RollHeight + 20.0f * S, FMath::Lerp(RollLeatherDark, RollLeatherLight, T));
		}
		DrawBox(EndX, Top - 16.0f * S, 48.0f * S, 8.0f * S, RollBrass);
		DrawBox(EndX, Top + RollHeight + 8.0f * S, 48.0f * S, 8.0f * S, RollBrass);
	}

	DrawLabel(LOCTEXT("Eyebrow", "WERKROLLE DES INGENIEURS"), Left + 46.0f * S, Top + 34.0f * S, 12.0f * S, RollBrass);
	DrawLabel(LOCTEXT("Title", "Ladungen und Vorrichtungen"), Left + 46.0f * S, Top + 50.0f * S, 28.0f * S, RollBone);
	const FVaelGear* InWeapon = VaelEngineerGear::Find(Engineer->GetAmmo());
	DrawLabel(FText::Format(LOCTEXT("InWeapon", "Eingelegt: {0}"), InWeapon != nullptr ? InWeapon->Name : LOCTEXT("Stones", "Steine")),
		Left + Width - 60.0f * S, Top + 54.0f * S, 16.0f * S, RollBrass, 1.0f);

	// The pockets: one per kind, a flap with a brass stud, the filling as a coloured mass, the count on the flap
	const int32 Count = List.Num();
	const float PocketGap = 10.0f * S;
	const float PocketWidth = FMath::Min(118.0f * S, (Width - 92.0f * S - PocketGap * (Count - 1)) / FMath::Max(Count, 1));
	const float PocketHeight = 170.0f * S;
	const float PocketsLeft = Left + (Width - (PocketWidth * Count + PocketGap * (Count - 1))) * 0.5f;
	const float PocketTop = Top + 96.0f * S;
	for (int32 Index = 0; Index < Count; ++Index)
	{
		const FVaelGear& Gear = *List[Index];
		const float X = PocketsLeft + Index * (PocketWidth + PocketGap);
		const bool bSel = Index == Selected;
		const int32 Carried = Engineer->GetCarried(Gear.Id);
		const int32 Stock = Engineer->GetStock(Gear.Id);
		const int32 Limit = FMath::Max(Engineer->GetCarryLimit(Gear), 1);
		const bool bLaid = Engineer->GetAmmo() == Gear.Id;

		DrawBox(X, PocketTop, PocketWidth, PocketHeight, bSel ? RollLeatherLight : RollLeatherDark);
		// what is in the pocket, filled from below
		const float Fill = FMath::Clamp(static_cast<float>(Carried) / Limit, 0.0f, 1.0f);
		const float FillHeight = (PocketHeight - 50.0f * S) * Fill;
		DrawBox(X + 8.0f * S, PocketTop + PocketHeight - 8.0f * S - FillHeight, PocketWidth - 16.0f * S, FillHeight, Gear.Color * 0.8f);
		// the flap with its stud
		DrawBox(X, PocketTop, PocketWidth, 38.0f * S, bSel ? RollRgb(140, 88, 52, 255) : RollRgb(74, 44, 25, 255));
		DrawDisc(FVector2D(X + PocketWidth * 0.5f, PocketTop + 34.0f * S), 5.0f * S, bLaid ? RollRgb(255, 214, 120) : RollBrass);
		DrawLabel(FText::Format(LOCTEXT("Fill", "{0}/{1}"), Carried, Limit), X + PocketWidth * 0.5f, PocketTop + 8.0f * S, 15.0f * S, RollBone, 0.5f);
		// stitching round the pocket
		DrawFrame(X + 3.0f * S, PocketTop + 3.0f * S, PocketWidth - 6.0f * S, PocketHeight - 6.0f * S, RollThread, 1.0f);
		if (bSel)
		{
			DrawFrame(X - 2.0f * S, PocketTop - 2.0f * S, PocketWidth + 4.0f * S, PocketHeight + 4.0f * S, RollBrass, 2.0f * S);
		}
		DrawLabel(Gear.Name, X + PocketWidth * 0.5f, PocketTop + PocketHeight + 8.0f * S, 13.0f * S, bSel ? RollBone : RollDim, 0.5f, PocketWidth + PocketGap);
		if (Stock > 0)
		{
			DrawLabel(FText::Format(LOCTEXT("Pack", "+{0} im Rucksack"), Stock), X + PocketWidth * 0.5f, PocketTop + PocketHeight - 26.0f * S, 11.0f * S, RollBone, 0.5f);
		}
	}

	// Below the roll: the picked kind
	if (!List.IsValidIndex(Selected))
	{
		return;
	}
	const FVaelGear& Gear = *List[Selected];
	const float InfoTop = Top + RollHeight + 34.0f * S;
	const float InfoWidth = Width * 0.6f;
	DrawBox(Left, InfoTop, Width, 190.0f * S, RollRgb(30, 22, 16, 235));
	DrawFrame(Left, InfoTop, Width, 190.0f * S, RollBrassDim, 1.0f);
	float Y = InfoTop + 14.0f * S;
	DrawLabel(Gear.Name, Left + 24.0f * S, Y, 22.0f * S, RollBone);
	Y += 30.0f * S;
	if (!Gear.Text.IsEmpty())
	{
		Y = DrawWrappedLabel(Gear.Text, Left + 24.0f * S, Y, 14.0f * S, RollDim, InfoWidth - 24.0f * S) + 8.0f * S;
	}

	// What it costs to make one batch in the field
	const float CostLeft = Left + InfoWidth + 20.0f * S;
	const float CostWidth = Width - InfoWidth - 44.0f * S;
	float CY = InfoTop + 14.0f * S;
	DrawLabel(FText::Format(LOCTEXT("FieldBatch", "IM FELD FERTIGEN · {0} STÜCK"), Gear.Batch), CostLeft, CY, 11.0f * S, RollBrass);
	CY += 18.0f * S;
	for (const FVaelGearCost& Cost : Gear.Costs)
	{
		const UVaelMaterial* Material = VaelEngineerGear::LoadMaterial(Cost.Material);
		const int32 Have = Material != nullptr && Bag != nullptr ? Bag->GetCount(Material) : 0;
		DrawBox(CostLeft, CY + 4.0f * S, 10.0f * S, 10.0f * S, Material != nullptr ? Material->Color : RollDim);
		DrawLabel(Material != nullptr ? Material->DisplayName : FText::FromName(Cost.Material), CostLeft + 18.0f * S, CY, 14.0f * S, RollBone);
		DrawLabel(FText::Format(LOCTEXT("HaveNeed", "{0} / {1}"), Have, Cost.Count), CostLeft + CostWidth, CY, 14.0f * S, Have >= Cost.Count ? RollGood : RollBad, 1.0f);
		CY += 20.0f * S;
	}

	// The keys
	const bool bPad = Glyphs != EVaelInputGlyphs::Keyboard;
	const FText Confirm = !bPad ? LOCTEXT("KeyEnter", "Enter") : Glyphs == EVaelInputGlyphs::PlayStation ? LOCTEXT("KeyCross", "Kreuz") : LOCTEXT("KeyA", "A");
	const FText Keys = FText::Format(LOCTEXT("Keys", "{0}: aus dem Rucksack bereitstellen (leer: im Feld fertigen)   ·   {1}: in die Waffe legen   ·   {2}: im Feld fertigen   ·   {3}: Tasche wählen"),
		Confirm, bPad ? LOCTEXT("PadUp", "Steuerkreuz hoch") : LOCTEXT("KeyOne", "1"), bPad ? LOCTEXT("PadDown", "Steuerkreuz runter") : LOCTEXT("KeyThree", "3"),
		bPad ? LOCTEXT("PadSides", "Steuerkreuz links/rechts") : LOCTEXT("KeyArrows", "Pfeile"));
	DrawLabel(Keys, Left + 24.0f * S, InfoTop + 160.0f * S, 12.0f * S, RollDim, 0.0f, Width - 48.0f * S);
}

#undef LOCTEXT_NAMESPACE
