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

void AVaelHUD::DrawEngineerWheel(const AVaelPlayerController* PlayerController)
{
	const AVaelCharacter* Player = PlayerController != nullptr ? PlayerController->GetPawn<AVaelCharacter>() : nullptr;
	const UVaelEngineerComponent* Engineer = Player != nullptr && Player->IsEngineer() ? Player->GetEngineer() : nullptr;
	if (Engineer == nullptr || PlayerController->IsGrimoireOpen() || (!Engineer->IsGrabbing() && Engineer->GetSecondsSinceCycle() > 1.6f))
	{
		return;
	}
	// The wheel of the pouches over his head: every shot he can reach for, the chosen one lit, how long the reach takes
	const FVector Screen = Canvas->Project(Player->GetActorLocation() + FVector(0.0f, 0.0f, 150.0f));
	if (Screen.Z <= 0.0f)
	{
		return;
	}
	const float S = UiScale;
	const FVector2D Centre(Screen.X, Screen.Y - 40.0f * S);
	const TArray<FName> Choices = Engineer->GetAmmoChoices();
	const FName Chosen = Engineer->IsGrabbing() ? Engineer->GetGrabAmmo() : Engineer->GetAmmo();
	const float Radius = 54.0f * S;
	DrawDisc(Centre, Radius + 18.0f * S, EngRgb(16, 12, 10, 170));
	for (int32 Index = 0; Index < Choices.Num(); ++Index)
	{
		const float Angle = -HALF_PI + 2.0f * PI * Index / Choices.Num();
		const FVector2D At = Centre + FVector2D(FMath::Cos(Angle), FMath::Sin(Angle)) * Radius;
		const FVaelGear* Gear = Choices[Index] == VaelEngineerGear::Standard ? nullptr : VaelEngineerGear::Find(Choices[Index]);
		const FLinearColor Color = Gear != nullptr ? Gear->Color : EngRgb(140, 128, 112);
		const bool bOn = Choices[Index] == Chosen;
		DrawDisc(At, (bOn ? 13.0f : 9.0f) * S, bOn ? EngRgb(240, 200, 130) : EngRgb(60, 48, 40));
		DrawDisc(At, (bOn ? 10.0f : 7.0f) * S, Color);
		if (Gear != nullptr)
		{
			DrawLabel(FText::AsNumber(Engineer->GetCarried(Gear->Id)), At.X, At.Y + 12.0f * S, 10.0f * S, EngRgb(226, 214, 190), 0.5f);
		}
	}
	const FVaelGear* ChosenGear = Chosen == VaelEngineerGear::Standard ? nullptr : VaelEngineerGear::Find(Chosen);
	const FText Name = ChosenGear != nullptr ? ChosenGear->Name : Engineer->GetAmmoName();
	DrawLabel(Name, Centre.X, Centre.Y - 16.0f * S, 13.0f * S, EngRgb(240, 228, 204), 0.5f);
	if (Engineer->IsGrabbing())
	{
		DrawLabel(Engineer->IsGrabFromPack() ? LOCTEXT("FromPack", "Rucksack \u00B7 1,2 s") : LOCTEXT("FromPouch", "Beutel \u00B7 0,35 s"), Centre.X, Centre.Y + 2.0f * S, 10.0f * S,
			Engineer->IsGrabFromPack() ? EngRgb(214, 150, 96) : EngRgb(150, 214, 140), 0.5f);
		const float BarWidth = 60.0f * S;
		DrawBox(Centre.X - BarWidth * 0.5f, Centre.Y + 18.0f * S, BarWidth, 4.0f * S, EngRgb(40, 30, 24));
		DrawBox(Centre.X - BarWidth * 0.5f, Centre.Y + 18.0f * S, BarWidth * Engineer->GetGrabFraction(), 4.0f * S, EngRgb(206, 140, 70));
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
