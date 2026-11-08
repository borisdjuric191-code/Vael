// Copyright Epic Games, Inc. All Rights Reserved.

// The engineer on the HUD: his weapon, the shot in it, the device he lays next, what he has laid out

#include "UI/VaelHUD.h"
#include "Engine/Canvas.h"
#include "EngineUtils.h"
#include "Combat/VaelCharacterBase.h"
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
	if (Engineer == nullptr || PlayerController->IsGrimoireOpen())
	{
		return;
	}
	const float S = UiScale;
	const FVector Feet = Canvas->Project(Player->GetActorLocation() - FVector(0.0f, 0.0f, 60.0f));

	// The goggles show the weak side of every enemy in reach: a gold mark at its head side
	const FVaelGear* Held = Engineer->GetWeapon();
	if (Engineer->HasBuilt(VaelEngineerGear::Goggles) && Held != nullptr)
	{
		for (TActorIterator<AVaelCharacterBase> It(GetWorld()); It; ++It)
		{
			if (It->IsA<AVaelCharacter>() || It->GetHealth() <= 0.0f || FVector::Dist2D(It->GetActorLocation(), Player->GetActorLocation()) > Held->Range)
			{
				continue;
			}
			const FVector Mark = Canvas->Project(It->GetActorLocation() + It->GetActorForwardVector() * It->GetSimpleCollisionRadius() + FVector(0.0f, 0.0f, 50.0f));
			if (Mark.Z > 0.0f)
			{
				const bool bFacing = FVector::DotProduct(It->GetActorForwardVector().GetSafeNormal2D(), (Player->GetActorLocation() - It->GetActorLocation()).GetSafeNormal2D()) > 0.77f;
				DrawDisc(FVector2D(Mark.X, Mark.Y), (bFacing ? 7.0f : 5.0f) * S, bFacing ? EngRgb(255, 210, 110) : EngRgb(160, 130, 80, 200));
				DrawDisc(FVector2D(Mark.X, Mark.Y), 2.5f * S, EngRgb(40, 24, 10));
			}
		}
	}

	// Under his feet: how far the bow is drawn (stages at 0.4, 0.8, 1.2 s), the bolts in the crossbow
	if (Feet.Z > 0.0f && Held != nullptr)
	{
		const float BarWidth = 70.0f * S;
		const FVector2D BarAt(Feet.X - BarWidth * 0.5f, Feet.Y + 26.0f * S);
		if (Engineer->IsDrawing())
		{
			const bool bSlingDraw = Held->Id == VaelEngineerGear::Sling;
			const float Drawn = FMath::Clamp(Engineer->GetDrawSeconds() / (bSlingDraw ? UVaelEngineerComponent::SlingFullDraw : 1.2f), 0.0f, 1.0f);
			// The aim arc: where the stone or arrow would come down at this draw, a dotted bow over the ground to it
			{
				const float Reach = bSlingDraw ? FMath::Lerp(800.0f, 2400.0f, Drawn) : Held->Range;
				const FVector Start = Player->GetActorLocation() + Player->GetActorForwardVector() * 40.0f + FVector(0.0f, 0.0f, 30.0f);
				FVector Toward = PlayerController->GetFloromantAim() - Start;
				Toward.Z = 0.0f;
				if (Toward.IsNearlyZero())
				{
					Toward = Player->GetActorForwardVector();
				}
				const FVector Land = Start + Toward.GetSafeNormal() * FMath::Min(Toward.Size(), Reach) - FVector(0.0f, 0.0f, 80.0f);
				const float Lift = 40.0f + 0.12f * FVector::Dist2D(Start, Land);
				FVector2D Last;
				for (int32 Step = 0; Step <= 16; ++Step)
				{
					const float T = Step / 16.0f;
					const FVector Point = FMath::Lerp(Start, Land, T) + FVector(0.0f, 0.0f, 4.0f * Lift * T * (1.0f - T));
					const FVector Screen = Canvas->Project(Point);
					if (Step > 0 && Step % 2 == 1 && Screen.Z > 0.0f)
					{
						Canvas->K2_DrawLine(Last, FVector2D(Screen.X, Screen.Y), 2.0f * S, Drawn >= 1.0f ? FLinearColor(1.0f, 0.82f, 0.43f, 0.8f) : FLinearColor(0.95f, 0.75f, 0.5f, 0.55f));
					}
					Last = FVector2D(Screen.X, Screen.Y);
				}
				const FVector Ring = Canvas->Project(Land);
				if (Ring.Z > 0.0f)
				{
					DrawDisc(FVector2D(Ring.X, Ring.Y), 6.0f * S, EngRgb(255, 210, 110, 180));
				}
			}
			DrawBox(BarAt.X, BarAt.Y, BarWidth, 6.0f * S, EngRgb(30, 22, 18, 220));
			DrawBox(BarAt.X, BarAt.Y, BarWidth * Drawn, 6.0f * S, Drawn >= 1.0f ? EngRgb(255, 210, 110) : EngRgb(206, 140, 70));
			for (const float Stage : { 1.0f / 3.0f, 2.0f / 3.0f })
			{
				DrawBox(BarAt.X + BarWidth * Stage - 1.0f, BarAt.Y - 2.0f * S, 2.0f, 10.0f * S, EngRgb(240, 228, 204));
			}
		}
		else if (Held->Id == VaelEngineerGear::Crossbow)
		{
			for (int32 Bolt = 0; Bolt < 2; ++Bolt)
			{
				const bool bLoaded = Bolt < Engineer->GetMagazine() && Engineer->GetReloadLeft() <= 0.0f;
				DrawBox(Feet.X - 12.0f * S + Bolt * 16.0f * S, BarAt.Y, 8.0f * S, 14.0f * S, bLoaded ? EngRgb(200, 200, 214) : EngRgb(60, 50, 44, 200));
			}
		}
	}

	if (!Engineer->IsGrabbing() && Engineer->GetSecondsSinceCycle() > 1.6f)
	{
		return;
	}
	// The wheel of the pouches over his head: every shot he can reach for, the chosen one lit, how long the reach takes
	const FVector Screen = Canvas->Project(Player->GetActorLocation() + FVector(0.0f, 0.0f, 150.0f));
	if (Screen.Z <= 0.0f)
	{
		return;
	}
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
