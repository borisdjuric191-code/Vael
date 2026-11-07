// Copyright Epic Games, Inc. All Rights Reserved.

// Die Waage (Lore-Bibel, Die drei Wege): one bar for every balance of corruption, the same for a player, a region and the
// root board of a refugium. Gold light on the left (below 30), dusk in the middle (twilight, the 40 to 60 band of the
// twilight path drawn as a frame), violet shadow on the right (above 70); a marker shows the value and flares when it moves.

#include "UI/VaelHUD.h"
#include "Save/VaelSaveSubsystem.h"
#include "EngineUtils.h"
#include "Engine/Canvas.h"
#include "Player/VaelCharacter.h"
#include "Player/VaelPlayerController.h"
#include "World/VaelBalance.h"
#include "World/VaelRegion.h"

#define LOCTEXT_NAMESPACE "VaelHUDBalance"

namespace
{
	FLinearColor BalanceRgb(uint8 R, uint8 G, uint8 B, uint8 A = 255)
	{
		return FLinearColor(FColor(R, G, B, A));
	}

	FLinearColor BalanceAlpha(FLinearColor Color, float Alpha)
	{
		Color.A *= Alpha;
		return Color;
	}
}

void AVaelHUD::DrawBalanceBar(const FText& Label, float Value, float X, float Y, float Width, float Height, float Flare, float FlareDelta, float Drift)
{
	const float S = UiScale;
	const float Low = UVaelBalanceSubsystem::LightBelow / 100.0f;
	const float High = UVaelBalanceSubsystem::ShadowAbove / 100.0f;
	const float BarY = Y + 18.0f * S;

	// Label on the left, value and zone on the right
	const EVaelBalanceZone Zone = UVaelBalanceSubsystem::GetZone(Value);
	DrawLabel(Label, X, Y, 13.0f * S, BalanceRgb(222, 208, 186));
	DrawLabel(FText::Format(LOCTEXT("Value", "{0} % · {1}"), FMath::RoundToInt(Value), UVaelBalanceSubsystem::GetZoneName(Zone)), X + Width, Y, 13.0f * S,
		UVaelBalanceSubsystem::GetZoneColor(Zone), 1.0f);

	// The three zones
	DrawBox(X, BarY, Width * Low, Height, BalanceRgb(150, 118, 58));
	DrawBox(X + Width * Low, BarY, Width * (High - Low), Height, BalanceRgb(112, 60, 92));
	DrawBox(X + Width * High, BarY, Width * (1.0f - High), Height, BalanceRgb(70, 34, 112));
	// the band the twilight path has to stay in
	DrawFrame(X + Width * UVaelBalanceSubsystem::TwilightLow / 100.0f, BarY - 1.0f * S, Width * (UVaelBalanceSubsystem::TwilightHigh - UVaelBalanceSubsystem::TwilightLow) / 100.0f,
		Height + 2.0f * S, BalanceRgb(240, 180, 200, 170), 1.0f);
	DrawFrame(X, BarY, Width, Height, BalanceRgb(42, 31, 25), 1.0f);

	// The marker: a diamond in the colour of its zone, flaring when the value just moved, a little arrow for where it drifts
	const float MarkerX = X + Width * FMath::Clamp(Value / 100.0f, 0.0f, 1.0f);
	const float CenterY = BarY + Height * 0.5f;
	const float R = (Height * 0.9f + 3.0f * S) * (1.0f + 0.6f * Flare);
	if (Flare > 0.0f)
	{
		DrawDisc(FVector2D(MarkerX, CenterY), R * 1.8f, BalanceAlpha(FlareDelta > 0.0f ? UVaelBalanceSubsystem::GetZoneColor(EVaelBalanceZone::Shadow) : UVaelBalanceSubsystem::GetZoneColor(EVaelBalanceZone::Light), 0.35f * Flare));
	}
	DrawPolygon({ FVector2D(MarkerX, CenterY - R), FVector2D(MarkerX + R * 0.7f, CenterY), FVector2D(MarkerX, CenterY + R), FVector2D(MarkerX - R * 0.7f, CenterY) },
		UVaelBalanceSubsystem::GetZoneColor(Zone));
	DrawPolygon({ FVector2D(MarkerX, CenterY - R * 0.45f), FVector2D(MarkerX + R * 0.3f, CenterY), FVector2D(MarkerX, CenterY + R * 0.45f), FVector2D(MarkerX - R * 0.3f, CenterY) },
		BalanceRgb(250, 244, 230));
	if (!FMath::IsNearlyZero(Drift))
	{
		const float Dir = Drift > 0.0f ? 1.0f : -1.0f;
		const FVector2D Tip(MarkerX + Dir * (R + 9.0f * S), CenterY);
		DrawPolygon({ Tip, Tip + FVector2D(-Dir * 6.0f * S, -4.0f * S), Tip + FVector2D(-Dir * 6.0f * S, 4.0f * S) },
			Drift > 0.0f ? UVaelBalanceSubsystem::GetZoneColor(EVaelBalanceZone::Shadow) : UVaelBalanceSubsystem::GetZoneColor(EVaelBalanceZone::Light));
	}
}

float AVaelHUD::DrawPlayerBalance(const AActor* Player, const FText& Label, float X, float Y, float Width)
{
	float SecondsAgo = 100.0f;
	float Delta = 0.0f;
	const UVaelBalanceSubsystem* Balance = UVaelBalanceSubsystem::Get(this);
	if (Balance != nullptr)
	{
		Balance->GetLastChange(Player, SecondsAgo, Delta);
	}
	const float Flare = FMath::Clamp(1.0f - SecondsAgo / 1.6f, 0.0f, 1.0f);
	DrawBalanceBar(Label, UVaelBalanceSubsystem::GetPlayer(Player), X, Y, Width, 7.0f * UiScale, Flare, Delta, 0.0f);
	return Y + 34.0f * UiScale;
}

void AVaelHUD::DrawBoard(const AVaelPlayerController* PlayerController)
{
	const AVaelCharacter* Player = PlayerController->GetPawn<AVaelCharacter>();
	const UVaelBalanceSubsystem* Balance = UVaelBalanceSubsystem::Get(this);
	if (Player == nullptr || Balance == nullptr)
	{
		return;
	}
	const float S = UiScale;
	DrawBox(0.0f, 0.0f, Canvas->ClipX, Canvas->ClipY, BalanceRgb(8, 5, 5, 170));
	const float Width = FMath::Min(900.0f * S, Canvas->ClipX - 32.0f * S);
	const float Height = FMath::Min(560.0f * S, Canvas->ClipY - 32.0f * S);
	const float Left = (Canvas->ClipX - Width) * 0.5f;
	const float Top = (Canvas->ClipY - Height) * 0.5f;
	const float Pad = 30.0f * S;

	// root wood: dark brown with a warm rim
	DrawBox(Left, Top, Width, Height, BalanceRgb(24, 16, 12, 240));
	DrawFrame(Left, Top, Width, Height, BalanceRgb(112, 84, 52), 2.0f * S);
	DrawFrame(Left + 6.0f * S, Top + 6.0f * S, Width - 12.0f * S, Height - 12.0f * S, BalanceRgb(196, 158, 92, 140), 1.0f);
	DrawLabel(LOCTEXT("BoardEyebrow", "WURZELTAFEL DER GLUTKAMMER"), Left + Pad, Top + 22.0f * S, 12.0f * S, BalanceRgb(236, 116, 50));
	DrawTitle(LOCTEXT("BoardTitle", "Die Waage"), Left + Pad, Top + 38.0f * S, 36.0f * S, BalanceRgb(236, 224, 202));

	const float BarW = Width - 2.0f * Pad;
	float Y = Top + 104.0f * S;
	Y = DrawPlayerBalance(Player, LOCTEXT("BoardSelf", "Deine Waage"), Left + Pad, Y, BarW) + 14.0f * S;

	// the region the beds steer, at once when it is loaded; otherwise what is known from the save stays with the region
	int32 LightBeds = 0;
	int32 ShadowBeds = 0;
	const float Drift = Balance->GetRefugiumDrift(&LightBeds, &ShadowBeds);
	const float RegionValue = Balance->GetRefugiumRegionValue();
	if (RegionValue >= 0.0f)
	{
		DrawBalanceBar(LOCTEXT("BoardRegion", "Waage der Aschenmark"), RegionValue, Left + Pad, Y, BarW, 9.0f * S, 0.0f, 0.0f, Drift);
	}
	else
	{
		DrawLabel(LOCTEXT("BoardRegionFar", "Waage der Aschenmark: oben in der Aschenmark zu sehen"), Left + Pad, Y, 13.0f * S, BalanceRgb(222, 208, 186));
	}
	Y += 50.0f * S;

	// the beds and their pull
	const FText Pull = FMath::IsNearlyZero(Drift) ? LOCTEXT("PullNone", "Die Beete halten die Aschenmark im Gleichgewicht.")
		: Drift < 0.0f ? FText::Format(LOCTEXT("PullLight", "Die Beete nehmen der Aschenmark je Stunde {0} % Nid."), FText::AsNumber(FMath::RoundToInt(-Drift * 100.0f) / 100.0f))
		: FText::Format(LOCTEXT("PullShadow", "Die Beete geben der Aschenmark je Stunde {0} % Nid."), FText::AsNumber(FMath::RoundToInt(Drift * 100.0f) / 100.0f));
	DrawLabel(FText::Format(LOCTEXT("Beds", "Lichtbeete: {0}   ·   Schattenbeete: {1}"), LightBeds, ShadowBeds), Left + Pad, Y, 17.0f * S, BalanceRgb(236, 224, 202));
	DrawLabel(Pull, Left + Pad, Y + 26.0f * S, 15.0f * S, Drift > 0.0f ? UVaelBalanceSubsystem::GetZoneColor(EVaelBalanceZone::Shadow) : UVaelBalanceSubsystem::GetZoneColor(EVaelBalanceZone::Light));
	Y += 66.0f * S;

	// how it moves: two columns
	const float ColW = (BarW - 30.0f * S) * 0.5f;
	DrawLabel(LOCTEXT("RiseHead", "STEIGT"), Left + Pad, Y, 12.0f * S, UVaelBalanceSubsystem::GetZoneColor(EVaelBalanceZone::Shadow));
	DrawLabel(LOCTEXT("SinkHead", "SINKT"), Left + Pad + ColW + 30.0f * S, Y, 12.0f * S, UVaelBalanceSubsystem::GetZoneColor(EVaelBalanceZone::Light));
	DrawWrappedLabel(LOCTEXT("RiseText", "Nid im Zauber (+2 bis 5) · Schattenformen im Kampf · eine Schattenform säen (+3) · Schattentränke (+1 bis 3) · Nidstiche"),
		Left + Pad, Y + 20.0f * S, 14.0f * S, BalanceRgb(222, 208, 186), ColW);
	DrawWrappedLabel(LOCTEXT("SinkText", "Reinigungstrank (−10) · Rast mit Klartrunk oder Lichttropfen (−1 je Stunde) · Nidkristall bei Albrun läutern (−5) · Lichtbeete senken die Aschenmark"),
		Left + Pad + ColW + 30.0f * S, Y + 20.0f * S, 14.0f * S, BalanceRgb(222, 208, 186), ColW);

	DrawLabel(LOCTEXT("BoardFooter", "Licht unter 30 · Zwielicht 30 bis 70, der Zwielichtweg hält 40 bis 60 · Schatten über 70"), Left + Width * 0.5f, Top + Height - 34.0f * S, 13.0f * S,
		BalanceRgb(160, 142, 122), 0.5f);
}

#undef LOCTEXT_NAMESPACE
