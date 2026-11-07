// Copyright Epic Games, Inc. All Rights Reserved.

// The engineer on the HUD: his weapon, the shot in it, the device he lays next, what he has laid out

#include "UI/VaelHUD.h"
#include "Engine/Canvas.h"
#include "Engineer/VaelEngineerComponent.h"
#include "Engineer/VaelEngineerGear.h"
#include "Player/VaelCharacter.h"
#include "Player/VaelPlayerController.h"

#define LOCTEXT_NAMESPACE "VaelHUDEngineer"

namespace
{
	FLinearColor EngRgb(uint8 R, uint8 G, uint8 B, uint8 A = 255)
	{
		return FLinearColor(FColor(R, G, B, A));
	}
}

void AVaelHUD::DrawEngineerLegend(const AVaelPlayerController* PlayerController, const AVaelCharacter* Player, float CenterX, float Y, float InnerWidth)
{
	const UVaelEngineerComponent* Engineer = Player->GetEngineer();
	if (Engineer == nullptr)
	{
		return;
	}
	const float S = UiScale;
	const bool bKeyboard = PlayerController->GetInputGlyphs() == EVaelInputGlyphs::Keyboard;
	const bool bPlayStation = PlayerController->GetInputGlyphs() == EVaelInputGlyphs::PlayStation;
	const FLinearColor Bone = EngRgb(226, 214, 190);
	const FLinearColor Dim = EngRgb(150, 136, 118);
	const FLinearColor Copper = EngRgb(206, 140, 70);

	const float Left = CenterX - InnerWidth * 0.5f;
	const float ColumnWidth = InnerWidth / 3.0f;
	const float Top = Y + 26.0f * S;

	// Column 1: the weapon and its reload
	const FVaelGear* Weapon = Engineer->GetWeapon();
	DrawLabel(bKeyboard ? LOCTEXT("WeaponKey", "WAFFE · R") : LOCTEXT("WeaponPad", "WAFFE · ↑"), Left, Top, 10.0f * S, Copper);
	DrawLabel(Weapon != nullptr ? Weapon->Name : FText::GetEmpty(), Left, Top + 13.0f * S, 15.0f * S, Bone, 0.0f, ColumnWidth - 8.0f * S);
	const float Ready = 1.0f - Engineer->GetReloadLeft() / FMath::Max(Engineer->GetInterval(), 0.01f);
	DrawBox(Left, Top + 36.0f * S, ColumnWidth - 12.0f * S, 4.0f * S, EngRgb(40, 30, 24));
	DrawBox(Left, Top + 36.0f * S, (ColumnWidth - 12.0f * S) * Ready, 4.0f * S, Ready >= 1.0f ? Copper : Dim);

	// Column 2: the shot, endless or how many are left in the pouch
	const float AmmoLeft = Left + ColumnWidth;
	const FName Ammo = Engineer->GetAmmo();
	const bool bStandard = Ammo == VaelEngineerGear::Standard;
	DrawLabel(bKeyboard ? LOCTEXT("AmmoKey", "LADUNG · Mausrad") : bPlayStation ? LOCTEXT("AmmoPS", "LADUNG · R1") : LOCTEXT("AmmoXbox", "LADUNG · RB"),
		AmmoLeft, Top, 10.0f * S, Copper);
	DrawBox(AmmoLeft, Top + 16.0f * S, 10.0f * S, 10.0f * S, Engineer->GetAmmoColor());
	DrawLabel(Engineer->GetAmmoName(), AmmoLeft + 16.0f * S, Top + 13.0f * S, 15.0f * S, Bone, 0.0f, ColumnWidth - 26.0f * S);
	DrawLabel(bStandard ? LOCTEXT("Endless", "unendlich") : FText::Format(LOCTEXT("AmmoLeft", "noch {0} · Vorrat {1}"), Engineer->GetCarried(Ammo), Engineer->GetStock(Ammo)),
		AmmoLeft, Top + 33.0f * S, 11.0f * S, Dim);

	// Column 3: the device he lays next, how many he carries, how many are out
	const float DeviceLeft = Left + ColumnWidth * 2.0f;
	const FVaelGear* Device = Engineer->GetDevice().IsNone() ? nullptr : VaelEngineerGear::Find(Engineer->GetDevice());
	DrawLabel(bKeyboard ? LOCTEXT("DeviceKey", "VORRICHTUNG · Z/C, RMB") : bPlayStation ? LOCTEXT("DevicePS", "VORRICHTUNG · ←→, L2") : LOCTEXT("DeviceXbox", "VORRICHTUNG · ←→, LT"),
		DeviceLeft, Top, 10.0f * S, Copper, 0.0f, ColumnWidth);
	DrawLabel(Device != nullptr ? FText::Format(LOCTEXT("DeviceCount", "{0} ×{1}"), Device->Name, Engineer->GetCarried(Device->Id)) : LOCTEXT("NoDevice", "keine dabei"),
		DeviceLeft, Top + 13.0f * S, 15.0f * S, Device != nullptr ? Bone : Dim, 0.0f, ColumnWidth - 8.0f * S);
	DrawLabel(FText::Format(bKeyboard ? LOCTEXT("PlacedKey", "{0} gelegt · X zündet") : bPlayStation ? LOCTEXT("PlacedPS", "{0} gelegt · Viereck zündet") : LOCTEXT("PlacedXbox", "{0} gelegt · X zündet"),
		Engineer->GetNumPlaced()), DeviceLeft, Top + 33.0f * S, 11.0f * S, Dim, 0.0f, ColumnWidth - 8.0f * S);
}

#undef LOCTEXT_NAMESPACE
