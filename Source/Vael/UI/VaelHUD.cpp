// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/VaelHUD.h"
#include "CanvasItem.h"
#include "CanvasTypes.h"
#include "Combat/VaelCombatStatics.h"
#include "Creatures/VaelCreature.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Engine/Font.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Magic/VaelElementComponent.h"
#include "Magic/VaelFormula.h"
#include "Items/VaelMaterial.h"
#include "Items/VaelMaterialBag.h"
#include "Magic/VaelGrimoireSubsystem.h"
#include "Player/VaelInteractable.h"
#include "Magic/VaelMagicSettings.h"
#include "Player/VaelCharacter.h"
#include "Player/VaelPlayerController.h"
#include "UI/VaelNoticeSubsystem.h"
#include "UI/VaelCombatTextSubsystem.h"
#include "VaelGameMode.h"
#include "World/VaelRegion.h"

#define LOCTEXT_NAMESPACE "VaelHUD"

namespace
{
	/** Color from 8 bit sRGB values */
	FLinearColor Rgb(uint8 R, uint8 G, uint8 B, uint8 A = 255)
	{
		return FLinearColor(FColor(R, G, B, A));
	}

	/** Colors of the browser prototype */
	const FLinearColor PanelColor = Rgb(13, 9, 10, 199);
	const FLinearColor RimColor = Rgb(77, 59, 45);
	const FLinearColor BoneColor = Rgb(234, 220, 196);
	const FLinearColor DimColor = Rgb(168, 151, 138);
	const FLinearColor EmberColor = Rgb(232, 105, 44);
	const FLinearColor SlotColor = Rgb(34, 26, 22);
	const FLinearColor GlyphColor = Rgb(20, 13, 11);

	/** Order of the elements in the legend and the queue */
	const EVaelElement LegendElements[] = { EVaelElement::Fire, EVaelElement::Water, EVaelElement::Earth, EVaelElement::Air, EVaelElement::Mark };

	/** Height of the HUD layout in units */
	constexpr float ReferenceHeight = 1080.0f;

	/** Points around a circle */
	constexpr int32 CircleSides = 28;

	FLinearColor WithAlpha(FLinearColor Color, float Alpha)
	{
		Color.A *= Alpha;
		return Color;
	}
}

void AVaelHUD::BeginPlay()
{
	Super::BeginPlay();

	Font = GEngine != nullptr ? GEngine->GetLargeFont() : nullptr;

	if (UVaelGrimoireSubsystem* Grimoire = GetGrimoire())
	{
		FormulaLearnedHandle = Grimoire->OnFormulaLearned.AddUObject(this, &AVaelHUD::OnFormulaLearned);
	}
}

void AVaelHUD::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UVaelGrimoireSubsystem* Grimoire = GetGrimoire())
	{
		Grimoire->OnFormulaLearned.Remove(FormulaLearnedHandle);
	}

	Super::EndPlay(EndPlayReason);
}

void AVaelHUD::OnFormulaLearned(const UVaelFormula* Formula, const FText& Reason)
{
	// Every player has a HUD, but only the first one draws and posts
	const AVaelPlayerController* OwningController = Cast<AVaelPlayerController>(GetOwningPlayerController());
	if (Formula == nullptr || OwningController == nullptr || !OwningController->IsFirstLocalPlayer())
	{
		return;
	}

	const FLinearColor Color = Formula->Elements.IsEmpty() ? BoneColor : UVaelMagicSettings::Get()->GetElementColor(Formula->Elements.Last());
	UVaelNoticeSubsystem::Post(this, FText::Format(LOCTEXT("NewFormula", "Neue Formel: {0}"), Formula->DisplayName), Reason, Color, 5.0f);
}

void AVaelHUD::DrawHUD()
{
	Super::DrawHUD();

	// The screen is shared: the first player draws for everybody
	const AVaelPlayerController* OwningController = Cast<AVaelPlayerController>(GetOwningPlayerController());
	if (Canvas == nullptr || Font == nullptr || OwningController == nullptr || !OwningController->IsFirstLocalPlayer())
	{
		return;
	}

	UiScale = Canvas->ClipY / ReferenceHeight * UVaelUISettings::Get()->HudScale;

	float FontWidth = 0.0f;
	Canvas->TextSize(Font, TEXT("Ag"), FontWidth, FontBaseHeight);
	FontBaseHeight = FMath::Max(FontBaseHeight, 1.0f);

	DrawWeather();
	DrawCreatureHealthBars();
	DrawCombatTexts();
	DrawRegionInfo();

	// One panel per player, side by side at the bottom
	TArray<const AVaelPlayerController*> PlayerControllers;
	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		const AVaelPlayerController* VaelController = Cast<AVaelPlayerController>(It->Get());
		if (VaelController != nullptr && VaelController->GetPawn<AVaelCharacter>() != nullptr)
		{
			PlayerControllers.Add(VaelController);
		}
	}

	PlayerControllers.Sort([](const AVaelPlayerController& A, const AVaelPlayerController& B) { return A.GetPlayerSlot() < B.GetPlayerSlot(); });

	const int32 NumPanels = PlayerControllers.Num();
	if (NumPanels > 0)
	{
		const float Gap = 12.0f * UiScale;
		const float Width = FMath::Min(PanelWidth * UiScale, (Canvas->ClipX - 24.0f * UiScale - (NumPanels - 1) * Gap) / NumPanels);
		const float Height = PanelHeight * UiScale;
		const float Top = Canvas->ClipY - Height - 12.0f * UiScale;
		const float Left = (Canvas->ClipX - (NumPanels * Width + (NumPanels - 1) * Gap)) * 0.5f;

		for (int32 PanelIndex = 0; PanelIndex < NumPanels; ++PanelIndex)
		{
			DrawPlayerPanel(PlayerControllers[PanelIndex], PlayerControllers[PanelIndex]->GetPawn<AVaelCharacter>(), Left + PanelIndex * (Width + Gap), Top, Width, Height);
		}
	}

	DrawInteractPrompts(PlayerControllers);

	float NoticeTop = 70.0f * UiScale;
	DrawBossBar(NoticeTop);
	DrawNotices(NoticeTop);

	for (const AVaelPlayerController* PlayerController : PlayerControllers)
	{
		if (PlayerController->IsGrimoireOpen())
		{
			DrawGrimoire(PlayerController);
			break;
		}
	}
}

void AVaelHUD::DrawPlayerPanel(const AVaelPlayerController* PlayerController, const AVaelCharacter* Player, float X, float Y, float Width, float Height)
{
	const float S = UiScale;
	const EVaelInputGlyphs Glyphs = PlayerController->GetInputGlyphs();
	const UVaelElementComponent* Elements = Player->GetElementComponent();
	const UVaelMagicSettings* MagicSettings = UVaelMagicSettings::Get();
	const AVaelGameMode* GameMode = GetWorld()->GetAuthGameMode<AVaelGameMode>();

	DrawBox(X, Y, Width, Height, PanelColor);
	DrawFrame(X, Y, Width, Height, RimColor);
	DrawBox(X, Y, Width, 3.0f * S, GameMode != nullptr ? GameMode->GetPlayerColor(PlayerController->GetPlayerSlot()) : BoneColor);

	// Health on the left, mana on the right
	const float OrbRadius = FMath::Min(34.0f * S, Width * 0.09f);
	const float OrbY = Y + Height * 0.5f + 4.0f * S;
	DrawOrb(FVector2D(X + OrbRadius + 12.0f * S, OrbY), OrbRadius, Player->GetHealth() / FMath::Max(Player->GetMaxHealth(), 1.0f),
		Player->IsDowned() ? Rgb(85, 85, 85) : Rgb(163, 36, 43), Rgb(42, 13, 14), FText::AsNumber(FMath::CeilToInt(Player->GetHealth())));
	DrawOrb(FVector2D(X + Width - OrbRadius - 12.0f * S, OrbY), OrbRadius, Player->GetMana() / FMath::Max(Player->GetMaxMana(), 1.0f),
		Rgb(42, 95, 184), Rgb(13, 24, 48), FText::AsNumber(FMath::FloorToInt(Player->GetMana())));

	const float CenterX = X + Width * 0.5f;
	const float InnerWidth = Width - (OrbRadius * 2.0f + 24.0f * S) * 2.0f;

	DrawLabel(FText::Format(LOCTEXT("PlayerLabel", "Spieler {0} \u00B7 {1}"), PlayerController->GetPlayerSlot() + 1, GetDeviceName(Glyphs)),
		CenterX, Y + 7.0f * S, 12.0f * S, DimColor, 0.5f, InnerWidth);

	// Legend: which button queues which element; Mark stays dim while it sleeps
	const UVaelGrimoireSubsystem* LegendGrimoire = GetGrimoire();
	const bool bMarkAwake = LegendGrimoire != nullptr && LegendGrimoire->IsMarkAwakened();
	const float LegendRadius = 10.5f * S;
	const float LegendSpacing = 27.0f * S;
	for (int32 ElementIndex = 0; ElementIndex < UE_ARRAY_COUNT(LegendElements); ++ElementIndex)
	{
		const EVaelElement Element = LegendElements[ElementIndex];
		DrawElementButton(Glyphs, Element, FVector2D(CenterX + (ElementIndex - 2) * LegendSpacing, Y + 37.0f * S), LegendRadius, Element == EVaelElement::Mark && !bMarkAwake ? 0.3f : 1.0f);
	}

	// Queue: unlocked slots, locked ones dashed with their number
	const TArray<EVaelElement>& Queue = Elements->GetQueue();
	const float SlotRadius = 11.0f * S;
	for (int32 SlotIndex = 0; SlotIndex < VaelElements::MaxQueueSlots; ++SlotIndex)
	{
		const FVector2D SlotCenter(CenterX + (SlotIndex - 2) * LegendSpacing, Y + 66.0f * S);

		if (SlotIndex >= Elements->GetNumSlots())
		{
			DrawDisc(SlotCenter, SlotRadius, Rgb(22, 17, 15));
			DrawRing(SlotCenter, SlotRadius, Rgb(58, 46, 38), 1.0f * S, true);
			DrawLabel(FText::AsNumber(SlotIndex + 1), SlotCenter.X, SlotCenter.Y - 6.0f * S, 11.0f * S, Rgb(74, 60, 50), 0.5f);
			continue;
		}

		const bool bFilled = Queue.IsValidIndex(SlotIndex);
		DrawDisc(SlotCenter, SlotRadius, bFilled ? MagicSettings->GetElementColor(Queue[SlotIndex]) : SlotColor);

		// Elements from the environment get a golden rim
		const bool bFromEnvironment = bFilled && Elements->IsFromEnvironment(SlotIndex);
		DrawRing(SlotCenter, SlotRadius, bFromEnvironment ? Rgb(246, 231, 166) : RimColor, (bFromEnvironment ? 2.5f : 1.0f) * S);
	}

	// What the queue adds up to
	FText ComboText;
	FLinearColor ComboColor = Rgb(108, 94, 82);
	const UVaelGrimoireSubsystem* Grimoire = GetGrimoire();
	const UVaelFormula* QueuedFormula = Grimoire != nullptr && !Queue.IsEmpty() ? Grimoire->FindFormula(Queue) : nullptr;

	if (Queue.IsEmpty())
	{
		ComboText = FText::Format(LOCTEXT("ChooseElements", "Elemente w\u00E4hlen \u00B7 {0}: Grimoire"), GetGrimoireButtonName(Glyphs));
	}
	else if (QueuedFormula != nullptr && Grimoire->IsFormulaKnown(QueuedFormula))
	{
		ComboText = FText::Format(LOCTEXT("KnownCombo", "{0} \u00B7 {1}"), QueuedFormula->DisplayName, GetCastButtonName(Glyphs));
		ComboColor = BoneColor;
	}
	else if (QueuedFormula != nullptr && QueuedFormula->Source == EVaelFormulaSource::Free)
	{
		ComboText = FText::Format(LOCTEXT("UnknownCombo", "Unbekannt \u00B7 {0} zum Experimentieren"), GetCastButtonName(Glyphs));
		ComboColor = Rgb(158, 143, 125);
	}
	else
	{
		ComboText = LOCTEXT("SealedCombo", "Versiegelt");
		ComboColor = Rgb(158, 143, 125);
	}

	DrawLabel(ComboText, CenterX, Y + 82.0f * S, 12.0f * S, ComboColor, 0.5f, InnerWidth);

	// Quick slots with their formula, cooldown and button
	const float QuickSize = 30.0f * S;
	const float QuickGap = 6.0f * S;
	const float QuickLeft = CenterX - (4.0f * QuickSize + 3.0f * QuickGap) * 0.5f;
	const float QuickTop = Y + 100.0f * S;

	for (int32 SlotIndex = 0; SlotIndex < UVaelElementComponent::NumQuickSlots; ++SlotIndex)
	{
		const float QuickX = QuickLeft + SlotIndex * (QuickSize + QuickGap);
		DrawBox(QuickX, QuickTop, QuickSize, QuickSize, Rgb(29, 22, 19));
		DrawFrame(QuickX, QuickTop, QuickSize, QuickSize, RimColor);

		if (const UVaelFormula* Formula = Elements->GetQuickSlotFormula(SlotIndex))
		{
			const int32 NumDots = Formula->Elements.Num();
			for (int32 DotIndex = 0; DotIndex < NumDots; ++DotIndex)
			{
				DrawDisc(FVector2D(QuickX + QuickSize * 0.5f + (DotIndex - (NumDots - 1) * 0.5f) * 8.0f * S, QuickTop + QuickSize * 0.5f - 4.0f * S), 4.0f * S,
					MagicSettings->GetElementColor(Formula->Elements[DotIndex]));
			}

			const float Cooldown = Elements->GetQuickSlotCooldownFraction(SlotIndex);
			if (Cooldown > 0.0f)
			{
				DrawBox(QuickX, QuickTop, QuickSize, QuickSize * Cooldown, Rgb(0, 0, 0, 166));
			}
		}

		DrawQuickSlotLabel(Glyphs, SlotIndex, FVector2D(QuickX + QuickSize * 0.5f, QuickTop + QuickSize - 6.0f * S), 9.0f * S, DimColor);
	}

	// Corruption of the mage along the bottom edge, only once the Mark has touched them
	if (Player->GetCorruption() > 0.0f)
	{
		const float BarLeft = X + OrbRadius * 2.0f + 20.0f * S;
		const float BarWidth = Width - (OrbRadius * 2.0f + 20.0f * S) * 2.0f;
		DrawBox(BarLeft, Y + Height - 6.0f * S, BarWidth, 3.0f * S, Rgb(28, 18, 32));
		DrawBox(BarLeft, Y + Height - 6.0f * S, BarWidth * Player->GetCorruption() / 100.0f, 3.0f * S, Rgb(162, 77, 255));
	}

	// Down: the panel darkens and shows how far the help has come
	if (Player->IsDowned())
	{
		DrawBox(X, Y, Width, Height, Rgb(0, 0, 0, 140));
		DrawLabel(LOCTEXT("Downed", "Gefallen"), CenterX, Y + Height * 0.5f - 14.0f * S, 20.0f * S, Rgb(255, 138, 128), 0.5f);

		const float BarWidth = InnerWidth * 0.8f;
		DrawBox(CenterX - BarWidth * 0.5f, Y + Height * 0.5f + 14.0f * S, BarWidth, 4.0f * S, Rgb(40, 32, 28));
		DrawBox(CenterX - BarWidth * 0.5f, Y + Height * 0.5f + 14.0f * S, BarWidth * Player->GetReviveFraction(), 4.0f * S, Rgb(246, 231, 166));
	}
}

void AVaelHUD::DrawBossBar(float& InOutTop)
{
	const AVaelCreature* Boss = nullptr;
	for (TActorIterator<AVaelCreature> It(GetWorld()); It; ++It)
	{
		if (It->IsBossFightActive())
		{
			Boss = *It;
			break;
		}
	}

	if (Boss == nullptr)
	{
		return;
	}

	const float S = UiScale;
	const float Width = FMath::Min(520.0f * S, Canvas->ClipX * 0.5f);
	const float Left = (Canvas->ClipX - Width) * 0.5f;
	const float Top = 22.0f * S;

	DrawBox(Left - 4.0f * S, Top - 4.0f * S, Width + 8.0f * S, 36.0f * S, Rgb(13, 9, 10, 191));
	DrawBox(Left, Top + 18.0f * S, Width, 10.0f * S, Rgb(42, 21, 18));
	DrawBox(Left, Top + 18.0f * S, Width * FMath::Clamp(Boss->GetHealth() / FMath::Max(Boss->GetMaxHealth(), 1.0f), 0.0f, 1.0f), 10.0f * S, Rgb(210, 85, 31));

	const FText Status = Boss->GetStatusText();
	const FText Title = Status.IsEmpty() ? Boss->GetCreatureName() : FText::Format(LOCTEXT("BossWithStatus", "{0}  \u00B7 {1}"), Boss->GetCreatureName(), Status);
	DrawLabel(Title, Canvas->ClipX * 0.5f, Top - 2.0f * S, 16.0f * S, BoneColor, 0.5f);

	InOutTop = Top + 56.0f * S;
}

void AVaelHUD::DrawNotices(float Top)
{
	UVaelNoticeSubsystem* Notices = GetWorld()->GetSubsystem<UVaelNoticeSubsystem>();
	if (Notices == nullptr)
	{
		return;
	}

	const float S = UiScale;
	const double Now = FPlatformTime::Seconds();
	float Y = Top;

	for (const FVaelNotice& Notice : Notices->GetActiveNotices())
	{
		// Fades in quickly and out slowly
		const float Age = static_cast<float>(Now - Notice.StartTime);
		const float Alpha = FMath::Clamp(FMath::Min(Age / 0.3f, (Notice.Duration - Age) / 0.6f), 0.0f, 1.0f);

		DrawLabel(Notice.Title, Canvas->ClipX * 0.5f, Y, 24.0f * S, WithAlpha(Notice.Color, Alpha), 0.5f, Canvas->ClipX * 0.8f);
		Y += 30.0f * S;

		if (!Notice.Detail.IsEmpty())
		{
			DrawLabel(Notice.Detail, Canvas->ClipX * 0.5f, Y, 15.0f * S, WithAlpha(Rgb(217, 202, 179), Alpha), 0.5f, Canvas->ClipX * 0.8f);
			Y += 24.0f * S;
		}

		Y += 6.0f * S;
	}
}

void AVaelHUD::DrawCreatureHealthBars()
{
	const float S = UiScale;
	const float Width = 40.0f * S;
	const float Height = 5.0f * S;

	for (TActorIterator<AVaelCreature> It(GetWorld()); It; ++It)
	{
		const AVaelCreature* Creature = *It;
		if (Creature->IsDead() || Creature->IsHidden() || Creature->IsBossFightActive() || Creature->GetHealth() >= Creature->GetMaxHealth())
		{
			continue;
		}

		const FVector ScreenPosition = Canvas->Project(Creature->GetActorLocation() + FVector(0.0f, 0.0f, Creature->GetHealthBarHeight()));
		if (ScreenPosition.Z <= 0.0f)
		{
			continue;
		}

		const float Left = ScreenPosition.X - Width * 0.5f;
		const float Top = ScreenPosition.Y;

		DrawBox(Left - 1.0f * S, Top - 1.0f * S, Width + 2.0f * S, Height + 2.0f * S, Rgb(13, 9, 10, 200));
		DrawBox(Left, Top, Width * FMath::Clamp(Creature->GetHealth() / FMath::Max(Creature->GetMaxHealth(), 1.0f), 0.0f, 1.0f), Height, Creature->IsMarked() ? Rgb(162, 77, 255) : Rgb(196, 64, 48));

		// Conditions as small dots in front of the bar
		float DotX = Left - 5.0f * S;
		const auto DrawConditionDot = [this, &DotX, Top, Height, S, Creature](EVaelStatus Status, const FLinearColor& Color)
		{
			if (UVaelCombatStatics::HasStatus(Creature, Status))
			{
				DrawDisc(FVector2D(DotX, Top + Height * 0.5f), 3.0f * S, Color);
				DotX -= 8.0f * S;
			}
		};

		DrawConditionDot(EVaelStatus::Wet, Rgb(63, 157, 240));
		DrawConditionDot(EVaelStatus::Burning, Rgb(255, 122, 46));
		DrawConditionDot(EVaelStatus::Frozen, Rgb(191, 232, 255));
	}
}

void AVaelHUD::DrawGrimoire(const AVaelPlayerController* PlayerController)
{
	const UVaelGrimoireSubsystem* Grimoire = GetGrimoire();
	const AVaelCharacter* Player = PlayerController->GetPawn<AVaelCharacter>();
	if (Grimoire == nullptr || Player == nullptr)
	{
		return;
	}

	const float S = UiScale;
	const EVaelInputGlyphs Glyphs = PlayerController->GetInputGlyphs();
	const UVaelElementComponent* Elements = Player->GetElementComponent();
	const UVaelMagicSettings* MagicSettings = UVaelMagicSettings::Get();
	const TArray<TObjectPtr<UVaelFormula>>& Formulas = Grimoire->GetAllFormulas();

	DrawBox(0.0f, 0.0f, Canvas->ClipX, Canvas->ClipY, Rgb(9, 6, 7, 168));

	const float RowHeight = 48.0f * S;
	const float RowGap = 6.0f * S;
	const int32 NumVisibleRows = FMath::Min(GrimoireVisibleRows, Formulas.Num());
	const float Width = FMath::Min(GrimoireWidth * S, Canvas->ClipX - 32.0f * S);
	const float Height = 150.0f * S + NumVisibleRows * (RowHeight + RowGap) + 50.0f * S;
	const float Left = (Canvas->ClipX - Width) * 0.5f;
	const float Top = FMath::Max(16.0f * S, (Canvas->ClipY - Height) * 0.5f);
	const float Padding = 28.0f * S;

	DrawBox(Left, Top, Width, Height, Rgb(23, 17, 15));
	DrawFrame(Left, Top, Width, Height, RimColor);
	DrawFrame(Left - 5.0f * S, Top - 5.0f * S, Width + 10.0f * S, Height + 10.0f * S, Rgb(42, 31, 25));

	DrawLabel(FText::Format(LOCTEXT("GrimoireEyebrow", "GRIMOIRE DER GRUPPE \u00B7 SPIELER {0}"), PlayerController->GetPlayerSlot() + 1), Left + Padding, Top + 22.0f * S, 12.0f * S, EmberColor);
	DrawLabel(LOCTEXT("GrimoireTitle", "Formeln"), Left + Padding, Top + 40.0f * S, 36.0f * S, BoneColor);
	DrawLabel(LOCTEXT("GrimoireNote", "Bekannte Formeln auf einen Schnellplatz legen: Zeile w\u00E4hlen und den Schnellplatz dr\u00FCcken. Schnellformeln kosten mehr Mana und k\u00FChlen ab, von Hand gewirkte Kombos sind st\u00E4rker."),
		Left + Padding, Top + 88.0f * S, 13.0f * S, DimColor, 0.0f, Width - 2.0f * Padding);

	// Rows around the selection
	const int32 Selection = FMath::Clamp(PlayerController->GetGrimoireSelection(), 0, FMath::Max(Formulas.Num() - 1, 0));
	const int32 FirstRow = FMath::Clamp(Selection - NumVisibleRows / 2, 0, FMath::Max(Formulas.Num() - NumVisibleRows, 0));
	const float ListTop = Top + 120.0f * S;
	int32 NumKnown = 0;

	for (const UVaelFormula* Formula : Formulas)
	{
		NumKnown += Grimoire->IsFormulaKnown(Formula) ? 1 : 0;
	}

	for (int32 RowIndex = FirstRow; RowIndex < FirstRow + NumVisibleRows; ++RowIndex)
	{
		const UVaelFormula* Formula = Formulas[RowIndex];
		const bool bKnown = Grimoire->IsFormulaKnown(Formula);
		const float RowTop = ListTop + (RowIndex - FirstRow) * (RowHeight + RowGap);
		const float RowLeft = Left + Padding;
		const float RowWidth = Width - 2.0f * Padding;

		DrawBox(RowLeft, RowTop, RowWidth, RowHeight, Rgb(29, 22, 19));
		if (RowIndex == Selection)
		{
			DrawFrame(RowLeft, RowTop, RowWidth, RowHeight, EmberColor, 2.0f * S);
		}

		// Elements of the combination, hidden until known
		const bool bShowElements = bKnown || Formula->Source == EVaelFormulaSource::Start;
		for (int32 DotIndex = 0; DotIndex < Formula->Elements.Num(); ++DotIndex)
		{
			const FVector2D DotCenter(RowLeft + 18.0f * S + DotIndex * 20.0f * S, RowTop + RowHeight * 0.5f);
			if (bShowElements)
			{
				DrawDisc(DotCenter, 8.0f * S, MagicSettings->GetElementColor(Formula->Elements[DotIndex]));
			}
			else
			{
				DrawDisc(DotCenter, 8.0f * S, Rgb(44, 36, 32));
				DrawRing(DotCenter, 8.0f * S, Rgb(90, 70, 54), 1.0f * S, true);
			}
		}

		// Name and description, or what is known about a missing formula
		FText Name = LOCTEXT("UnknownName", "???");
		FText Description;
		if (bKnown)
		{
			Name = Formula->DisplayName;
			Description = Formula->Description;
		}
		else if (Formula->Source == EVaelFormulaSource::Mark)
		{
			Description = LOCTEXT("MarkUnknown", "Erwacht mit dem Mark.");
		}
		else if (Formula->Source == EVaelFormulaSource::Free)
		{
			Description = FText::Format(LOCTEXT("FreeUnknown", "{0} Elemente \u00B7 durch Experimentieren entdeckbar"), Formula->Elements.Num());
		}
		else if (Grimoire->HasEcho(Formula))
		{
			Name = FText::Format(LOCTEXT("SealedEchoed", "Versiegelt \u00B7 {0}"), Formula->DisplayName);
			Description = Formula->Hint;
		}
		else
		{
			Description = LOCTEXT("SealedUnknown", "Versiegelt. Wirke die Kombo, um ihr Echo zu h\u00F6ren.");
		}

		const float TextLeft = RowLeft + 100.0f * S;
		const float SlotsWidth = 4.0f * 30.0f * S + 12.0f * S;
		const float TextWidth = RowWidth - 100.0f * S - SlotsWidth;

		DrawLabel(Name, TextLeft, RowTop + 5.0f * S, 16.0f * S, bKnown ? BoneColor : Rgb(124, 109, 97), 0.0f, TextWidth);
		DrawLabel(Description, TextLeft, RowTop + 26.0f * S, 12.0f * S, DimColor, 0.0f, TextWidth);

		// How to cast it by hand, in the symbols of this player's device
		if (bKnown)
		{
			float HowX = TextLeft + FMath::Min(MeasureLabel(Name, 16.0f * S), TextWidth * 0.5f) + 16.0f * S;
			for (const EVaelElement Element : Formula->Elements)
			{
				DrawElementButton(Glyphs, Element, FVector2D(HowX, RowTop + 14.0f * S), 8.0f * S);
				HowX += 20.0f * S;
			}

			DrawLabel(FText::Format(LOCTEXT("HowCast", "\u00B7 {0}"), GetCastButtonName(Glyphs)), HowX - 4.0f * S, RowTop + 7.0f * S, 12.0f * S, Rgb(201, 180, 138));
		}

		// Quick slots: lit if this formula sits there
		for (int32 SlotIndex = 0; SlotIndex < UVaelElementComponent::NumQuickSlots; ++SlotIndex)
		{
			const float SlotX = RowLeft + RowWidth - SlotsWidth + 8.0f * S + SlotIndex * 30.0f * S;
			const float SlotY = RowTop + (RowHeight - 26.0f * S) * 0.5f;
			const bool bOnSlot = bKnown && Elements->GetQuickSlotFormula(SlotIndex) == Formula;

			DrawBox(SlotX, SlotY, 26.0f * S, 26.0f * S, bOnSlot ? EmberColor : Rgb(42, 32, 27, bKnown ? 255 : 90));
			DrawFrame(SlotX, SlotY, 26.0f * S, 26.0f * S, bOnSlot ? EmberColor : RimColor);
			DrawQuickSlotLabel(Glyphs, SlotIndex, FVector2D(SlotX + 13.0f * S, SlotY + 13.0f * S), 10.0f * S, bOnSlot ? Rgb(27, 13, 6) : WithAlpha(BoneColor, bKnown ? 1.0f : 0.3f));
		}
	}

	// Footer
	const float FooterY = Top + Height - 34.0f * S;
	DrawLabel(FText::Format(LOCTEXT("KnownCount", "{0} von {1} Formeln bekannt"), NumKnown, Formulas.Num()), Left + Padding, FooterY, 13.0f * S, DimColor);

	const FText CloseHint = Glyphs == EVaelInputGlyphs::Keyboard ? LOCTEXT("CloseKeyboard", "W / S: w\u00E4hlen \u00B7 Z X C V: belegen \u00B7 Tab / Esc: schlie\u00DFen")
		: Glyphs == EVaelInputGlyphs::PlayStation ? LOCTEXT("ClosePlayStation", "Linker Stick: w\u00E4hlen \u00B7 Steuerkreuz: belegen \u00B7 \u25CB / Create: schlie\u00DFen")
		: LOCTEXT("CloseXbox", "Linker Stick: w\u00E4hlen \u00B7 Steuerkreuz: belegen \u00B7 B / Ansicht: schlie\u00DFen");
	DrawLabel(CloseHint, Left + Width - Padding, FooterY, 13.0f * S, DimColor, 1.0f);

	DrawMaterialBag(Player, Left, Top, Width, Height);
}

void AVaelHUD::DrawMaterialBag(const AVaelCharacter* Player, float PanelLeft, float PanelTop, float PanelWidthOnScreen, float PanelHeightOnScreen)
{
	const float S = UiScale;
	const float Width = 250.0f * S;
	const float Padding = 18.0f * S;
	const float RowHeight = 22.0f * S;
	const TArray<FVaelMaterialStack> Stacks = Player->GetMaterialBag()->GetStacks();

	// Right of the grimoire if there is room, otherwise over its left edge
	const float RightX = PanelLeft + PanelWidthOnScreen + 16.0f * S;
	const float Left = RightX + Width <= Canvas->ClipX - 16.0f * S ? RightX : FMath::Max(16.0f * S, PanelLeft - Width - 16.0f * S);
	const float Height = FMath::Min(PanelHeightOnScreen, 64.0f * S + FMath::Max(Stacks.Num(), 1) * RowHeight + 12.0f * S);

	DrawBox(Left, PanelTop, Width, Height, Rgb(23, 17, 15));
	DrawFrame(Left, PanelTop, Width, Height, RimColor);

	DrawLabel(LOCTEXT("BagEyebrow", "MATERIALBEUTEL"), Left + Padding, PanelTop + 18.0f * S, 12.0f * S, EmberColor);

	if (Stacks.IsEmpty())
	{
		DrawLabel(LOCTEXT("BagEmpty", "Noch leer. Gefallene Kreaturen lassen Teile zurück."), Left + Padding, PanelTop + 44.0f * S, 12.0f * S, DimColor, 0.0f, Width - 2.0f * Padding);
		return;
	}

	const int32 MaxRows = FMath::Max(1, FMath::FloorToInt((Height - 64.0f * S) / RowHeight));
	for (int32 Index = 0; Index < FMath::Min(Stacks.Num(), MaxRows); ++Index)
	{
		const FVaelMaterialStack& Stack = Stacks[Index];
		const float RowY = PanelTop + 44.0f * S + Index * RowHeight;

		DrawDisc(FVector2D(Left + Padding + 5.0f * S, RowY + 8.0f * S), 5.0f * S, Stack.Material->Color);
		DrawLabel(Stack.Material->DisplayName, Left + Padding + 18.0f * S, RowY, 14.0f * S, BoneColor, 0.0f, Width - 2.0f * Padding - 60.0f * S);
		DrawLabel(FText::AsNumber(Stack.Count), Left + Width - Padding, RowY, 14.0f * S, BoneColor, 1.0f);
	}
}

void AVaelHUD::DrawElementButton(EVaelInputGlyphs Glyphs, EVaelElement Element, const FVector2D& Center, float Radius, float Alpha)
{
	DrawDisc(Center, Radius, WithAlpha(UVaelMagicSettings::Get()->GetElementColor(Element), Alpha));
	DrawButtonSymbol(Glyphs, Element, Center, Radius, WithAlpha(GlyphColor, Alpha));
}

void AVaelHUD::DrawButtonSymbol(EVaelInputGlyphs Glyphs, EVaelElement Element, const FVector2D& Center, float Radius, const FLinearColor& Color)
{
	// PlayStation face buttons are shapes: circle fire, square water, cross earth, triangle air
	if (Glyphs == EVaelInputGlyphs::PlayStation && Element != EVaelElement::Mark)
	{
		const float Size = Radius * 0.5f;
		const float Thickness = FMath::Max(1.5f, Radius * 0.2f);

		switch (Element)
		{
		case EVaelElement::Fire:
			DrawRing(Center, Size, Color, Thickness);
			break;
		case EVaelElement::Water:
			DrawFrame(Center.X - Size, Center.Y - Size, Size * 2.0f, Size * 2.0f, Color, Thickness);
			break;
		case EVaelElement::Earth:
			DrawSegment(Center + FVector2D(-Size, -Size), Center + FVector2D(Size, Size), Color, Thickness);
			DrawSegment(Center + FVector2D(Size, -Size), Center + FVector2D(-Size, Size), Color, Thickness);
			break;
		default:
		{
			const FVector2D Top = Center + FVector2D(0.0f, -Size * 1.1f);
			const FVector2D Right = Center + FVector2D(Size * 1.05f, Size * 0.75f);
			const FVector2D Left = Center + FVector2D(-Size * 1.05f, Size * 0.75f);
			DrawSegment(Top, Right, Color, Thickness);
			DrawSegment(Right, Left, Color, Thickness);
			DrawSegment(Left, Top, Color, Thickness);
			break;
		}
		}
		return;
	}

	const FText Label = GetElementButtonName(Glyphs, Element);
	const float TextHeight = Radius * (Label.ToString().Len() > 1 ? 0.9f : 1.3f);
	DrawLabel(Label, Center.X, Center.Y - TextHeight * 0.55f, TextHeight, Color, 0.5f);
}

void AVaelHUD::DrawQuickSlotLabel(EVaelInputGlyphs Glyphs, int32 SlotIndex, const FVector2D& Center, float Size, const FLinearColor& Color)
{
	if (Glyphs == EVaelInputGlyphs::Keyboard)
	{
		static const TCHAR* Keys[] = { TEXT("Z"), TEXT("X"), TEXT("C"), TEXT("V") };
		DrawLabel(FText::FromString(Keys[SlotIndex % 4]), Center.X, Center.Y - Size * 0.6f, Size * 1.1f, Color, 0.5f);
		return;
	}

	// D-pad arrows: up, right, down, left
	const float Angle = UE_HALF_PI * SlotIndex - UE_HALF_PI;
	const FVector2D Forward(FMath::Cos(Angle), FMath::Sin(Angle));
	const FVector2D Side(-Forward.Y, Forward.X);
	const float Half = Size * 0.45f;

	DrawPolygon({ Center + Forward * Half, Center - Forward * Half * 0.7f + Side * Half, Center - Forward * Half * 0.7f - Side * Half }, Color);
}

void AVaelHUD::DrawBox(float X, float Y, float Width, float Height, const FLinearColor& Color)
{
	FCanvasTileItem Tile(FVector2D(X, Y), FVector2D(Width, Height), Color);
	Tile.BlendMode = SE_BLEND_Translucent;
	Canvas->DrawItem(Tile);
}

void AVaelHUD::DrawFrame(float X, float Y, float Width, float Height, const FLinearColor& Color, float Thickness)
{
	DrawBox(X, Y, Width, Thickness, Color);
	DrawBox(X, Y + Height - Thickness, Width, Thickness, Color);
	DrawBox(X, Y + Thickness, Thickness, Height - 2.0f * Thickness, Color);
	DrawBox(X + Width - Thickness, Y + Thickness, Thickness, Height - 2.0f * Thickness, Color);
}

void AVaelHUD::DrawDisc(const FVector2D& Center, float Radius, const FLinearColor& Color)
{
	FCanvasNGonItem Disc(Center, FVector2D(Radius, Radius), CircleSides, Color);
	Disc.BlendMode = SE_BLEND_Translucent;
	Canvas->DrawItem(Disc);
}

void AVaelHUD::DrawRing(const FVector2D& Center, float Radius, const FLinearColor& Color, float Thickness, bool bDashed)
{
	for (int32 Side = 0; Side < CircleSides; ++Side)
	{
		if (bDashed && Side % 2 == 1)
		{
			continue;
		}

		const float AngleA = UE_TWO_PI * Side / CircleSides;
		const float AngleB = UE_TWO_PI * (Side + 1) / CircleSides;
		DrawSegment(Center + FVector2D(FMath::Cos(AngleA), FMath::Sin(AngleA)) * Radius, Center + FVector2D(FMath::Cos(AngleB), FMath::Sin(AngleB)) * Radius, Color, Thickness);
	}
}

void AVaelHUD::DrawOrb(const FVector2D& Center, float Radius, float Fill, const FLinearColor& FillColor, const FLinearColor& BackColor, const FText& Value)
{
	DrawDisc(Center, Radius, BackColor);

	// The filled part is the circle cut off at the fill line
	const float FillLine = Center.Y + Radius - 2.0f * Radius * FMath::Clamp(Fill, 0.0f, 1.0f);
	TArray<FVector2D> Circle;
	for (int32 Side = 0; Side < CircleSides * 2; ++Side)
	{
		const float Angle = UE_TWO_PI * Side / (CircleSides * 2);
		Circle.Add(Center + FVector2D(FMath::Cos(Angle), FMath::Sin(Angle)) * Radius);
	}

	TArray<FVector2D> Filled;
	for (int32 PointIndex = 0; PointIndex < Circle.Num(); ++PointIndex)
	{
		const FVector2D& Current = Circle[PointIndex];
		const FVector2D& Next = Circle[(PointIndex + 1) % Circle.Num()];
		const bool bCurrentInside = Current.Y >= FillLine;
		const bool bNextInside = Next.Y >= FillLine;

		if (bCurrentInside)
		{
			Filled.Add(Current);
		}

		if (bCurrentInside != bNextInside)
		{
			const float T = (FillLine - Current.Y) / (Next.Y - Current.Y);
			Filled.Add(FMath::Lerp(Current, Next, T));
		}
	}

	DrawPolygon(Filled, FillColor);

	// A little shine and a rim
	DrawDisc(Center + FVector2D(-Radius * 0.3f, -Radius * 0.4f), Radius * 0.3f, Rgb(255, 255, 255, 25));
	DrawRing(Center, Radius, Rgb(90, 70, 54), 2.0f * UiScale);

	DrawLabel(Value, Center.X, Center.Y - 8.0f * UiScale, 14.0f * UiScale, BoneColor, 0.5f);
}

void AVaelHUD::DrawPolygon(const TArray<FVector2D>& Points, const FLinearColor& Color)
{
	if (Points.Num() < 3)
	{
		return;
	}

	// Convex shapes only: a fan from the first point
	TArray<FCanvasUVTri> Triangles;
	for (int32 PointIndex = 1; PointIndex + 1 < Points.Num(); ++PointIndex)
	{
		FCanvasUVTri& Triangle = Triangles.AddDefaulted_GetRef();
		Triangle.V0_Pos = Points[0];
		Triangle.V1_Pos = Points[PointIndex];
		Triangle.V2_Pos = Points[PointIndex + 1];
		Triangle.V0_Color = Color;
		Triangle.V1_Color = Color;
		Triangle.V2_Color = Color;
	}

	FCanvasTriangleItem TriangleItem(Triangles, GWhiteTexture);
	TriangleItem.BlendMode = SE_BLEND_Translucent;
	Canvas->DrawItem(TriangleItem);
}

void AVaelHUD::DrawSegment(const FVector2D& Start, const FVector2D& End, const FLinearColor& Color, float Thickness)
{
	FCanvasLineItem Line(Start, End);
	Line.SetColor(Color);
	Line.LineThickness = Thickness;
	Line.BlendMode = SE_BLEND_Translucent;
	Canvas->DrawItem(Line);
}

void AVaelHUD::DrawLabel(const FText& Text, float X, float Y, float PixelHeight, const FLinearColor& Color, float Alignment, float MaxWidth)
{
	if (Text.IsEmpty() || Color.A <= 0.0f)
	{
		return;
	}

	FString String = Text.ToString();
	const float Scale = GetTextScale(PixelHeight);

	float Width = 0.0f;
	float Height = 0.0f;
	Canvas->TextSize(Font, String, Width, Height, Scale, Scale);

	// Too long: cut words off the end
	if (MaxWidth > 0.0f && Width > MaxWidth)
	{
		while (String.Len() > 1 && Width > MaxWidth)
		{
			String.LeftChopInline(1);
			Canvas->TextSize(Font, String + TEXT("..."), Width, Height, Scale, Scale);
		}
		String += TEXT("...");
	}

	FCanvasTextItem TextItem(FVector2D(X - Width * Alignment, Y), FText::FromString(String), Font, Color);
	TextItem.Scale = FVector2D(Scale, Scale);
	TextItem.EnableShadow(FLinearColor(0.0f, 0.0f, 0.0f, 0.8f * Color.A));
	Canvas->DrawItem(TextItem);
}

float AVaelHUD::MeasureLabel(const FText& Text, float PixelHeight)
{
	const float Scale = GetTextScale(PixelHeight);

	float Width = 0.0f;
	float Height = 0.0f;
	Canvas->TextSize(Font, Text.ToString(), Width, Height, Scale, Scale);

	return Width;
}

float AVaelHUD::GetTextScale(float PixelHeight) const
{
	return PixelHeight / FontBaseHeight;
}

FText AVaelHUD::GetCastButtonName(EVaelInputGlyphs Glyphs)
{
	switch (Glyphs)
	{
	case EVaelInputGlyphs::Keyboard: return LOCTEXT("CastKeyboard", "Klick");
	case EVaelInputGlyphs::PlayStation: return LOCTEXT("CastPlayStation", "R2");
	default: return LOCTEXT("CastXbox", "RT");
	}
}

FText AVaelHUD::GetGrimoireButtonName(EVaelInputGlyphs Glyphs)
{
	switch (Glyphs)
	{
	case EVaelInputGlyphs::Keyboard: return LOCTEXT("GrimoireKeyboard", "Tab");
	case EVaelInputGlyphs::PlayStation: return LOCTEXT("GrimoirePlayStation", "Create");
	default: return LOCTEXT("GrimoireXbox", "Ansicht");
	}
}

FText AVaelHUD::GetElementButtonName(EVaelInputGlyphs Glyphs, EVaelElement Element)
{
	if (Glyphs == EVaelInputGlyphs::Keyboard)
	{
		return FText::AsNumber(static_cast<int32>(Element) + 1);
	}

	if (Element == EVaelElement::Mark)
	{
		return Glyphs == EVaelInputGlyphs::PlayStation ? LOCTEXT("MarkPlayStation", "R1") : LOCTEXT("MarkXbox", "RB");
	}

	// Xbox face buttons in the order of the elements
	static const TCHAR* XboxButtons[] = { TEXT("B"), TEXT("X"), TEXT("A"), TEXT("Y") };
	return FText::FromString(XboxButtons[static_cast<int32>(Element) % 4]);
}

FText AVaelHUD::GetDeviceName(EVaelInputGlyphs Glyphs)
{
	switch (Glyphs)
	{
	case EVaelInputGlyphs::Keyboard: return LOCTEXT("DeviceKeyboard", "Tastatur");
	case EVaelInputGlyphs::PlayStation: return LOCTEXT("DevicePlayStation", "PlayStation");
	default: return LOCTEXT("DeviceXbox", "Controller");
	}
}

UVaelGrimoireSubsystem* AVaelHUD::GetGrimoire() const
{
	const UGameInstance* GameInstance = GetGameInstance();
	return GameInstance != nullptr ? GameInstance->GetSubsystem<UVaelGrimoireSubsystem>() : nullptr;
}

void AVaelHUD::DrawRegionInfo()
{
	// The region the first player stands in
	const APawn* FirstPawn = GetOwningPawn();
	const AVaelRegion* Region = FirstPawn != nullptr ? AVaelRegion::GetRegionAt(GetWorld(), FirstPawn->GetActorLocation()) : nullptr;
	if (Region == nullptr)
	{
		return;
	}

	const float S = UiScale;
	const float Width = 240.0f * S;
	const float Height = 92.0f * S;
	const float Left = Canvas->ClipX - Width - 16.0f * S;
	const float Top = 16.0f * S;
	const float Padding = 12.0f * S;

	DrawBox(Left, Top, Width, Height, Rgb(13, 9, 10, 173));
	DrawFrame(Left, Top, Width, Height, RimColor);

	DrawLabel(Region->GetRegionName(), Left + Padding, Top + 8.0f * S, 20.0f * S, BoneColor, 0.0f, Width - 2.0f * Padding);
	DrawLabel(FText::Format(LOCTEXT("WeatherLine", "Wetter: {0}"), AVaelRegion::GetWeatherName(Region->GetWeather())), Left + Padding, Top + 36.0f * S, 13.0f * S, Rgb(205, 189, 166));

	const int32 Corruption = FMath::RoundToInt(Region->GetCorruption());
	DrawLabel(FText::Format(LOCTEXT("CorruptionLine", "Verderbnis der Region: {0} %"), Corruption), Left + Padding, Top + 56.0f * S, 13.0f * S, Rgb(205, 189, 166));

	const float BarWidth = Width - 2.0f * Padding;
	DrawBox(Left + Padding, Top + Height - 12.0f * S, BarWidth, 5.0f * S, Rgb(42, 31, 46));
	DrawBox(Left + Padding, Top + Height - 12.0f * S, BarWidth * Region->GetCorruption() / 100.0f, 5.0f * S, Rgb(162, 77, 255));
}

void AVaelHUD::DrawWeather()
{
	const APawn* FirstPawn = GetOwningPawn();
	const AVaelRegion* Region = FirstPawn != nullptr ? AVaelRegion::GetRegionAt(GetWorld(), FirstPawn->GetActorLocation()) : nullptr;
	if (Region == nullptr)
	{
		return;
	}

	const EVaelWeather Weather = Region->GetWeather();
	const double Now = FPlatformTime::Seconds();
	const float Time = static_cast<float>(FMath::Fmod(Now, 1000.0));

	// Placeholder until there are particles: a drought bleaches the land in a warm glare
	if (Weather == EVaelWeather::Drought)
	{
		DrawBox(0.0f, 0.0f, Canvas->ClipX, Canvas->ClipY, Rgb(255, 170, 80, 28));
	}

	// A darker sky and falling streaks of ash rain
	if (Weather == EVaelWeather::Rain || Weather == EVaelWeather::Storm)
	{
		const bool bStorm = Weather == EVaelWeather::Storm;
		DrawBox(0.0f, 0.0f, Canvas->ClipX, Canvas->ClipY, bStorm ? Rgb(10, 14, 24, 70) : Rgb(20, 26, 34, 45));

		const int32 NumStreaks = bStorm ? 140 : 90;
		const float Length = (bStorm ? 46.0f : 30.0f) * UiScale;
		const float Slant = bStorm ? 0.45f : 0.12f;
		const FLinearColor StreakColor = bStorm ? Rgb(200, 210, 225, 70) : Rgb(170, 185, 200, 60);

		for (int32 StreakIndex = 0; StreakIndex < NumStreaks; ++StreakIndex)
		{
			// Every streak has its own fixed column and speed and loops from top to bottom
			const float Seed = FMath::Frac(FMath::Sin(StreakIndex * 12.9898f) * 43758.5453f);
			const float Speed = (bStorm ? 1.6f : 1.0f) * (0.7f + 0.6f * Seed);
			const float Progress = FMath::Frac(Time * Speed * 0.9f + Seed * 7.0f);
			const float X = FMath::Frac(Seed * 3.7f + StreakIndex * 0.618f) * (Canvas->ClipX + Canvas->ClipY * Slant) - Canvas->ClipY * Slant * Progress;
			const float Y = Progress * (Canvas->ClipY + Length) - Length;

			DrawSegment(FVector2D(X, Y), FVector2D(X - Length * Slant, Y + Length), StreakColor, 1.5f * UiScale);
		}
	}

	// A flash across the screen when lightning strikes
	const float SinceLightning = static_cast<float>(Now - Region->GetLastLightningTime());
	if (SinceLightning >= 0.0f && SinceLightning < 0.3f)
	{
		DrawBox(0.0f, 0.0f, Canvas->ClipX, Canvas->ClipY, FLinearColor(0.85f, 0.9f, 1.0f, 0.35f * (1.0f - SinceLightning / 0.3f)));
	}
}


void AVaelHUD::DrawCombatTexts()
{
	UVaelCombatTextSubsystem* CombatTexts = GetWorld()->GetSubsystem<UVaelCombatTextSubsystem>();
	if (CombatTexts == nullptr)
	{
		return;
	}

	const UVaelUISettings* Settings = UVaelUISettings::Get();
	const double Now = GetWorld()->GetTimeSeconds();
	const float Duration = FMath::Max(Settings->CombatTextDuration, 0.01f);

	for (const FVaelCombatText& CombatText : CombatTexts->GetActiveTexts())
	{
		const FVector ScreenPosition = Canvas->Project(CombatText.Location);
		if (ScreenPosition.Z <= 0.0f)
		{
			continue;
		}

		// Rises steadily, stays solid for the first half and fades in the second
		const float Age = FMath::Clamp(static_cast<float>(Now - CombatText.StartTime) / Duration, 0.0f, 1.0f);
		const float Alpha = Age < 0.5f ? 1.0f : 1.0f - (Age - 0.5f) * 2.0f;
		const float Size = CombatText.Size * UiScale;

		DrawLabel(CombatText.Text, ScreenPosition.X, ScreenPosition.Y - Settings->CombatTextRise * UiScale * Age - Size * 0.5f, Size, WithAlpha(CombatText.Color, Alpha), 0.5f);
	}
}

void AVaelHUD::DrawInteractPrompts(const TArray<const AVaelPlayerController*>& PlayerControllers)
{
	const float S = UiScale;

	for (const AVaelPlayerController* PlayerController : PlayerControllers)
	{
		const AActor* Target = UVaelInteractionSubsystem::FindNearest(PlayerController->GetPawn<AVaelCharacter>());
		const IVaelInteractable* Interactable = Cast<IVaelInteractable>(Target);
		if (Interactable == nullptr)
		{
			continue;
		}

		const FVector Screen = Project(Target->GetActorLocation() + FVector(0.0f, 0.0f, 120.0f), false);
		if (Screen.Z <= 0.0f)
		{
			continue;
		}

		// The button in the symbols of the player's device
		const EVaelInputGlyphs Glyphs = PlayerController->GetInputGlyphs();
		const FText Button = Glyphs == EVaelInputGlyphs::Keyboard ? LOCTEXT("InteractKeyboard", "E")
			: Glyphs == EVaelInputGlyphs::PlayStation ? LOCTEXT("InteractPlayStation", "L1")
			: LOCTEXT("InteractXbox", "LB");

		const FText Prompt = FText::Format(LOCTEXT("InteractPrompt", "{0} · {1}"), Button, Interactable->GetInteractPrompt());
		const float Width = MeasureLabel(Prompt, 14.0f * S) + 20.0f * S;

		DrawBox(Screen.X - Width * 0.5f, Screen.Y - 12.0f * S, Width, 26.0f * S, Rgb(23, 17, 15, 220));
		DrawFrame(Screen.X - Width * 0.5f, Screen.Y - 12.0f * S, Width, 26.0f * S, RimColor);
		DrawLabel(Prompt, Screen.X, Screen.Y - 7.0f * S, 14.0f * S, BoneColor, 0.5f);
	}
}

#undef LOCTEXT_NAMESPACE
