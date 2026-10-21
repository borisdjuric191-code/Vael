// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "Compendium/VaelCompendiumEntry.h"
#include "Items/VaelItemTypes.h"
#include "Magic/VaelElementTypes.h"
#include "UI/VaelUISettings.h"
#include "VaelHUD.generated.h"

class AVaelCharacter;
class AVaelPlayerController;
class UFont;
class UVaelFormula;
class UVaelGrimoireSubsystem;
class UVaelWorldMap;
class UTexture2D;
struct FCompositeFont;
struct FSlateFontInfo;

/**
 *  HUD of the shared screen, drawn by the first local player for everybody, like the browser prototype:
 *  one panel per player at the bottom with health, mana, element legend in the symbols of the player's device,
 *  element queue and quick slots; a health bar for a boss at the top, notices below it,
 *  small health bars over hurt creatures and the grimoire while a player has it open;
 *  in the top right corner the mini map of the level, below it the region and the quests; the big map while a player has it open.
 *  Drawn with the canvas only, no widget assets needed.
 */
UCLASS()
class AVaelHUD : public AHUD
{
	GENERATED_BODY()

public:

	/** Draws everything */
	virtual void DrawHUD() override;

	/** Lets the panel of a player show the look of another class, like "Tamer"; empty for the own class */
	static void SetPanelPreview(const UObject* WorldContext, int32 PlayerSlot, const FString& ClassName);

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

	/** Shows above every mage what their queued elements add up to: the name of the formula in the color of its element, or that it is unknown or sealed */
	void DrawFormulaNames(const TArray<const AVaelPlayerController*>& PlayerControllers);

	/** Draws the material bag of the player next to the open grimoire */
	void DrawMaterialBag(const AVaelCharacter* Player, float PanelLeft, float PanelTop, float PanelWidthOnScreen, float PanelHeightOnScreen);

	/** Draws the page names of the menu and the buttons that switch between them, right aligned */
	void DrawMenuTabs(const AVaelPlayerController* PlayerController, float Right, float Y);

	/** Draws the inventory page of the menu: equipment, backpack and the selected item compared with what it would replace */
	void DrawInventory(const AVaelPlayerController* PlayerController);

	/** Draws the compendium page of the menu: the three books, their entries and what the group knows about the selected one */
	void DrawCompendium(const AVaelPlayerController* PlayerController);

	/** The page of the quests in the grimoire: open quests with their steps, then the finished ones */
	void DrawQuestLog(const AVaelPlayerController* PlayerController);

	/** Page of a trader: goods, prices, the purse and what the bag holds of each */
	void DrawShop(const AVaelPlayerController* PlayerController);

	/** Page of class and body: mage or Floromant, man or woman */
	void DrawHeroPage(const AVaelPlayerController* PlayerController);

	/** Kinds of menu windows: parchment for knowledge, dark iron for the system, the class material for the character */
	enum class EVaelWindow : uint8 { Parchment, Iron, Class };

	/** Background of a menu window in its material; a dark box while the material is missing */
	void DrawWindow(EVaelWindow Kind, float X, float Y, float Width, float Height, const class AVaelPlayerController* PlayerController);

	/** Instances of the window materials, by kind */
	UPROPERTY(Transient)
	TArray<TObjectPtr<class UMaterialInstanceDynamic>> WindowMaterials;

	/** Keeps one panel widget per player; returns false while WBP_SpielerHUD is missing, then the old panels are drawn */
	bool SyncPlayerWidgets(const TArray<const class AVaelPlayerController*>& PlayerControllers);

	/** Panel widgets of the players */
	UPROPERTY(Transient)
	TArray<TObjectPtr<class UVaelPlayerHUDWidget>> PlayerWidgets;

	/** Middle of the panel of a Floromant: the seeds on their buttons, the picked one and the plants standing */
	void DrawSeedLegend(const AVaelPlayerController* PlayerController, const AVaelCharacter* Player, float CenterX, float Y, float InnerWidth);

	/** Draws a closed book of the compendium series: cover in its color, strap and seal; lit if it is the open one */
	void DrawBookCover(EVaelCompendiumBook Book, float X, float Y, float Width, float Height, bool bLit);

	/** Draws the focus circle of every raised spyglass and how far the creatures in it have been watched */
	void DrawObservation(const TArray<const AVaelPlayerController*>& PlayerControllers);

	/** Rings under the enemies the players aim is locked on, in the color of each player */
	void DrawAimTargets(const TArray<const AVaelPlayerController*>& PlayerControllers);

	/** Draws text broken into lines no wider than the width; returns the Y below the last line */
	float DrawWrappedLabel(const FText& Text, float X, float Y, float PixelHeight, const FLinearColor& Color, float MaxWidth);

	/** Draws the names of the people in the level and a "!" over those with news */
	void DrawNpcMarkers();

	/** Draws the dialogue a player is reading */
	void DrawDialogue(const AVaelPlayerController* PlayerController);

	/** Name of an equipment place, like "Brust" */
	static FText GetEquipSlotName(EVaelEquipSlot EquipSlot);

	/** Name of the place an item is worn at */
	static FText GetItemSlotName(EVaelItemSlot Slot);

	/** Region box with name, weather and corruption at the top right, from Top down; returns the Y below it */
	float DrawRegionInfo(float Top);

	/** The open quests under the region box, main story first */
	void DrawQuestTracker(float Top);

	/** Where and how a map is drawn: the world point in the middle, its place on screen, the scale and the screen directions in the world */
	struct FMapView
	{
		FVector2D Center = FVector2D::ZeroVector;
		FVector2D Origin = FVector2D::ZeroVector;
		float PixelsPerCm = 1.0f;
		/** World direction of screen up (the camera looks along it) and of screen right */
		FVector2D Up = FVector2D(1.0f, 0.0f);
		FVector2D Right = FVector2D(0.0f, 1.0f);

		FVector2D ToScreen(const FVector2D& World) const;
		FVector2D ToWorld(const FVector2D& Screen) const;
	};

	/** The map of this level, null if the level has none (Project Settings > Vael UI > Level Maps) */
	const UVaelWorldMap* GetLevelMap();

	/** Map view turned like the camera, so up on the map is up on the screen */
	FMapView MakeMapView(const FVector2D& Center, const FVector2D& Origin, float PixelsPerCm) const;

	/** The mini map in the top right corner, centred on the group; returns the Y below it, or the top if the level has no map */
	float DrawMiniMap(const TArray<const AVaelPlayerController*>& PlayerControllers, float Top);

	/** The whole map of the level over the screen, with the names of the places, while a player has it open */
	void DrawBigMap(const AVaelPlayerController* PlayerController, const TArray<const AVaelPlayerController*>& PlayerControllers);

	/** Draws the part of the map picture that lies under the four screen corners (clockwise) */
	void DrawMapPicture(const FMapView& View, const FVector2D (&Corners)[4], float Alpha);

	/** Players as arrows in their colours, people with news and safe camps; with a box, markers outside it are left off or, players, kept at its edge */
	void DrawMapMarkers(const FMapView& View, const TArray<const AVaelPlayerController*>& PlayerControllers, float MarkerScale, const FBox2D* Clip);

	/** Name of the button that opens the map, for the device */
	static FText GetMapButtonName(EVaelInputGlyphs Glyphs);
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

	/** Like DrawLabel, in the title font (IM Fell English SC) */
	void DrawTitle(const FText& Text, float X, float Y, float PixelHeight, const FLinearColor& Color, float Alignment = 0.0f, float MaxWidth = 0.0f);

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

	/** Font used for all text while the font faces below are missing */
	UPROPERTY(Transient)
	TObjectPtr<UFont> Font;

	/** Text and title fonts of the painted HUD (Alegreya Sans, IM Fell English SC): sharp at every size, unlike the scaled engine font */
	TSharedPtr<const FCompositeFont> TextFont;
	TSharedPtr<const FCompositeFont> TitleFont;

	/** Keeps the loaded font faces */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UObject>> FontFaces;

	/** Font of the runtime font cache: canvas text drawn with a Slate font still needs a font of that cache type */
	UPROPERTY(Transient)
	TObjectPtr<UFont> RuntimeFont;

	/** True while DrawTitle draws */
	bool bTitleText = false;

	/** Slate font for a text height in pixels, in the title font while bTitleText is set */
	FSlateFontInfo GetSlateFont(float PixelHeight) const;

	/** Map of the level and its picture, looked up once */
	UPROPERTY(Transient)
	TObjectPtr<UVaelWorldMap> LevelMap;

	UPROPERTY(Transient)
	TObjectPtr<UTexture2D> MapPicture;

	bool bLevelMapLookedUp = false;

	/** Height of the font at scale 1, in pixels */
	float FontBaseHeight = 1.0f;

	/** Handle of the grimoire delegate */
	FDelegateHandle FormulaLearnedHandle;
};
