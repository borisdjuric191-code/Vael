// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "Items/VaelItemTypes.h"
#include "Magic/VaelElementTypes.h"
#include "UI/VaelUISettings.h"
#include "VaelHUD.generated.h"

class AVaelCharacter;
class AVaelPlayerController;
class UFont;
class UVaelFormula;
class UVaelGrimoireSubsystem;

/**
 *  HUD of the shared screen, drawn by the first local player for everybody, like the browser prototype:
 *  one panel per player at the bottom with health, mana, element legend in the symbols of the player's device,
 *  element queue and quick slots; a health bar for a boss at the top, notices below it,
 *  small health bars over hurt creatures and the grimoire while a player has it open.
 *  Drawn with the canvas only, no widget assets needed.
 */
UCLASS()
class AVaelHUD : public AHUD
{
	GENERATED_BODY()

public:

	/** Draws everything */
	virtual void DrawHUD() override;

protected:

	/** Initialization */
	virtual void BeginPlay() override;

	/** Cleanup */
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** Width of a player panel at HUD scale 1 */
	UPROPERTY(EditAnywhere, Category="Layout", meta = (ClampMin = 100))
	float PanelWidth = 380.0f;

	/** Height of a player panel at HUD scale 1 */
	UPROPERTY(EditAnywhere, Category="Layout", meta = (ClampMin = 50))
	float PanelHeight = 142.0f;

	/** Width of the grimoire at HUD scale 1 */
	UPROPERTY(EditAnywhere, Category="Layout", meta = (ClampMin = 200))
	float GrimoireWidth = 820.0f;

	/** Rows of the grimoire visible at once */
	UPROPERTY(EditAnywhere, Category="Layout", meta = (ClampMin = 3))
	int32 GrimoireVisibleRows = 11;

private:

	/** Shows newly learned formulas as notices */
	void OnFormulaLearned(const UVaelFormula* Formula, const FText& Reason);

	// Parts of the HUD
	void DrawPlayerPanel(const AVaelPlayerController* PlayerController, const AVaelCharacter* Player, float X, float Y, float Width, float Height);
	void DrawBossBar(float& InOutTop);
	void DrawNotices(float Top);
	void DrawCreatureHealthBars();
	void DrawGrimoire(const AVaelPlayerController* PlayerController);

	/** Shows over chests and other usable things which button uses them, for every player close enough */
	void DrawInteractPrompts(const TArray<const AVaelPlayerController*>& PlayerControllers);

	/** Draws the material bag of the player next to the open grimoire */
	void DrawMaterialBag(const AVaelCharacter* Player, float PanelLeft, float PanelTop, float PanelWidthOnScreen, float PanelHeightOnScreen);

	/** Draws the page names of the menu and the buttons that switch between them, right aligned */
	void DrawMenuTabs(const AVaelPlayerController* PlayerController, float Right, float Y);

	/** Draws the inventory page of the menu: equipment, backpack and the selected item compared with what it would replace */
	void DrawInventory(const AVaelPlayerController* PlayerController);

	/** Draws the names of the people in the level and a "!" over those with news */
	void DrawNpcMarkers();

	/** Draws the dialogue a player is reading */
	void DrawDialogue(const AVaelPlayerController* PlayerController);

	/** Name of an equipment place, like "Brust" */
	static FText GetEquipSlotName(EVaelEquipSlot EquipSlot);

	/** Name of the place an item is worn at */
	static FText GetItemSlotName(EVaelItemSlot Slot);

	void DrawRegionInfo();
	void DrawCombatTexts();
	void DrawWeather();

	// Element symbols
	void DrawElementButton(EVaelInputGlyphs Glyphs, EVaelElement Element, const FVector2D& Center, float Radius, float Alpha = 1.0f);
	void DrawButtonSymbol(EVaelInputGlyphs Glyphs, EVaelElement Element, const FVector2D& Center, float Radius, const FLinearColor& Color);
	void DrawQuickSlotLabel(EVaelInputGlyphs Glyphs, int32 SlotIndex, const FVector2D& Center, float Size, const FLinearColor& Color);

	// Shapes
	void DrawBox(float X, float Y, float Width, float Height, const FLinearColor& Color);
	void DrawFrame(float X, float Y, float Width, float Height, const FLinearColor& Color, float Thickness = 1.0f);
	void DrawDisc(const FVector2D& Center, float Radius, const FLinearColor& Color);
	void DrawRing(const FVector2D& Center, float Radius, const FLinearColor& Color, float Thickness, bool bDashed = false);
	void DrawOrb(const FVector2D& Center, float Radius, float Fill, const FLinearColor& FillColor, const FLinearColor& BackColor, const FText& Value);
	void DrawPolygon(const TArray<FVector2D>& Points, const FLinearColor& Color);
	void DrawSegment(const FVector2D& Start, const FVector2D& End, const FLinearColor& Color, float Thickness);

	/** Draws text with its top at Y. Alignment: 0 left of X, 0.5 centered on X, 1 right of X. Shortens the text to the maximum width. */
	void DrawLabel(const FText& Text, float X, float Y, float PixelHeight, const FLinearColor& Color, float Alignment = 0.0f, float MaxWidth = 0.0f);

	/** Width of a text at the given height in pixels */
	float MeasureLabel(const FText& Text, float PixelHeight);

	/** Text scale for a font height in pixels */
	float GetTextScale(float PixelHeight) const;

	/** Name of a button for the device: cast, grimoire */
	static FText GetCastButtonName(EVaelInputGlyphs Glyphs);
	static FText GetGrimoireButtonName(EVaelInputGlyphs Glyphs);
	static FText GetElementButtonName(EVaelInputGlyphs Glyphs, EVaelElement Element);
	static FText GetDeviceName(EVaelInputGlyphs Glyphs);

	UVaelGrimoireSubsystem* GetGrimoire() const;

	/** Pixels per HUD unit for the current canvas */
	float UiScale = 1.0f;

	/** Font used for all text */
	UPROPERTY(Transient)
	TObjectPtr<UFont> Font;

	/** Height of the font at scale 1, in pixels */
	float FontBaseHeight = 1.0f;

	/** Handle of the grimoire delegate */
	FDelegateHandle FormulaLearnedHandle;
};
