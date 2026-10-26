// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Fonts/SlateFontInfo.h"
#include "Styling/SlateBrush.h"
#include "Floromant/VaelFloromantSettings.h"
#include "UI/VaelHUDClassData.h"
#include "UI/VaelUISettings.h"
#include "VaelPlayerHUDWidget.generated.h"

class AVaelCharacter;
class AVaelPlayerController;
class UFontFace;
class UMaterialInstanceDynamic;
class UMaterialInterface;
class UTexture2D;

/**
 *  Panel of one player at the bottom of the shared screen: life vessel, ability bar and class vessel in a frame of the class,
 *  class name, level and sworn way above, the tail of the Urdrachin as experience bar below.
 *  Player 1 sits bottom left, player 2 bottom right, players 3 and 4 above them. Everything is painted in design pixels
 *  of a 1920 × 1080 screen and scales with the screen; the look of each class comes from its UVaelHUDClassData.
 *  WBP_SpielerHUD is made from this class, so its pictures and colours can be tuned in the editor.
 */
UCLASS()
class UVaelPlayerHUDWidget : public UUserWidget
{
	GENERATED_BODY()

public:

	/** Constructor */
	UVaelPlayerHUDWidget(const FObjectInitializer& ObjectInitializer);

	/** Player whose panel this is */
	void SetPlayer(AVaelPlayerController* InPlayer);
	AVaelPlayerController* GetPlayer() const { return Player.Get(); }

	/** Shows the look of another class, like "Tamer", to judge it before the class can be played; empty for the own class */
	void SetPreviewClass(const FString& ClassName) { PreviewClass = ClassName; }

	/** Size of the panel in design pixels */
	static constexpr float PanelWidth = 880.0f;
	static constexpr float PanelHeight = 230.0f;

protected:

	//~Begin UUserWidget
	virtual void NativeConstruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	virtual int32 NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements,
		int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;
	//~End UUserWidget

	/** Shared pictures and materials of every class */
	UPROPERTY(EditDefaultsOnly, Category="Look")
	TSoftObjectPtr<UMaterialInterface> LifeVesselMaterial;

	UPROPERTY(EditDefaultsOnly, Category="Look")
	TSoftObjectPtr<UMaterialInterface> CooldownMaterial;

	UPROPERTY(EditDefaultsOnly, Category="Look")
	TSoftObjectPtr<UMaterialInterface> TailMaterial;

	UPROPERTY(EditDefaultsOnly, Category="Look")
	TSoftObjectPtr<UMaterialInterface> MedallionMaterial;

	UPROPERTY(EditDefaultsOnly, Category="Look")
	TSoftObjectPtr<UMaterialInterface> BadgeMaterial;

	/** Painted glass, metal and claw of the life vessel, over the liquid; without it the drawn placeholder and the claw silhouette show */
	UPROPERTY(EditDefaultsOnly, Category="Look")
	TSoftObjectPtr<UTexture2D> LifeVesselPicture;

	UPROPERTY(EditDefaultsOnly, Category="Look")
	TSoftObjectPtr<UTexture2D> ClawTexture;

	UPROPERTY(EditDefaultsOnly, Category="Look")
	TSoftObjectPtr<UTexture2D> ClawThornsTexture;

	UPROPERTY(EditDefaultsOnly, Category="Look")
	TSoftObjectPtr<UTexture2D> DragonHeadTexture;

	/** Badges of the three ways: light, shadow, twilight */
	UPROPERTY(EditDefaultsOnly, Category="Look")
	TSoftObjectPtr<UTexture2D> PathTextures[3];

	UPROPERTY(EditDefaultsOnly, Category="Look")
	TSoftObjectPtr<UTexture2D> PotionIcon;

	UPROPERTY(EditDefaultsOnly, Category="Look")
	TSoftObjectPtr<UTexture2D> DodgeIcon;

	UPROPERTY(EditDefaultsOnly, Category="Look")
	TSoftObjectPtr<UTexture2D> SeedIcon;

	/** The furry green bar of the Floromant's Keimpelz pelt over the top of the panel, an instance of M_HUD_Pelz */
	UPROPERTY(EditDefaultsOnly, Category="Look")
	TSoftObjectPtr<UMaterialInterface> KeimpelzBarMaterial;

	/** Round menu buttons at the edge: Adernbaum, Rezeptbuch, Karte */
	UPROPERTY(EditDefaultsOnly, Category="Look")
	TSoftObjectPtr<UTexture2D> MenuIcons[3];

	/** Fonts (OFL): titles IM Fell English SC, text Alegreya Sans */
	UPROPERTY(EditDefaultsOnly, Category="Look")
	TSoftObjectPtr<UFontFace> TitleFontFace;

	UPROPERTY(EditDefaultsOnly, Category="Look")
	TSoftObjectPtr<UFontFace> TextFontFace;

	UPROPERTY(EditDefaultsOnly, Category="Look")
	TSoftObjectPtr<UFontFace> TextBoldFontFace;

	/** Ground and text colour of the panel */
	UPROPERTY(EditDefaultsOnly, Category="Look")
	FLinearColor GroundColor;

	UPROPERTY(EditDefaultsOnly, Category="Look")
	FLinearColor TextColor;

private:

	/** Everything a frame of painting needs, gathered in the tick so painting stays read only */
	struct FSlotState
	{
		FText Name;
		const FSlateBrush* Icon = nullptr;
		EVaelHUDButton Button = EVaelHUDButton::Cross;
		FText KeyboardKey;
		FLinearColor IconColor = FLinearColor::White;
		float CooldownLeft = 0.0f;
		bool bLit = false;
		bool bDim = false;
		/** The wipe shows a share, not seconds (the plant bar of a hand, a reload) */
		bool bNoSeconds = false;
	};

	struct FStripEntry
	{
		FLinearColor Color = FLinearColor::White;
		bool bFilled = false;
		bool bPicked = false;
		FText Label;
	};

	/** Look of the class the panel shows, the own or the previewed one */
	UVaelHUDClassData* FindClassData(const AVaelCharacter* Character) const;

	/** Makes the instances and brushes of a class once it is shown */
	void ApplyClassData(UVaelHUDClassData* Data);

	/** Brush of a picture, loaded once; null while the picture is missing */
	const FSlateBrush* GetTextureBrush(const TSoftObjectPtr<UTexture2D>& Texture);

	/** Brush of a picture loaded before, for painting */
	const FSlateBrush* FindTextureBrush(const TSoftObjectPtr<UTexture2D>& Texture) const;

	/** Brush around a material instance */
	static void SetMaterialBrush(FSlateBrush& Brush, UMaterialInterface* Material);

	/** Builds the fonts from the font faces */
	void BuildFonts();

	/** A small round button: its ground and the painted rim of the class where there is one */
	void PaintMedallion(FSlateWindowElementList& Out, int32 Layer, const FVector2f& Center, float Radius, const FLinearColor& Tint) const;

	/** Painting helpers, all in design pixels of the panel */
	void PaintBox(FSlateWindowElementList& Out, int32 Layer, const FSlateBrush* Brush, const FVector2f& Position, const FVector2f& Size, const FLinearColor& Tint = FLinearColor::White) const;
	void PaintText(FSlateWindowElementList& Out, int32 Layer, const FText& Text, const FSlateFontInfo& Font, const FVector2f& Position, const FLinearColor& Color, float Alignment = 0.0f, float MaxWidth = 0.0f) const;
	void PaintButton(FSlateWindowElementList& Out, int32 Layer, EVaelHUDButton Button, const FText& KeyboardKey, const FVector2f& Center, float Radius) const;
	void PaintLines(FSlateWindowElementList& Out, int32 Layer, const TArray<FVector2f>& Points, const FLinearColor& Color, float Thickness) const;

	TWeakObjectPtr<AVaelPlayerController> Player;
	FString PreviewClass;

	/** Class shown right now and its instances */
	UPROPERTY(Transient)
	TObjectPtr<UVaelHUDClassData> ShownData;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> FrameInstance;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> ClassVesselInstance;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> LifeVesselInstance;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> TailInstance;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UMaterialInstanceDynamic>> CooldownInstances;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> DodgeCooldownInstance;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> KeimpelzBarInstance;

	/** Pictures loaded for the brushes */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UObject>> LoadedResources;

	TMap<FSoftObjectPath, TSharedPtr<FSlateBrush>> TextureBrushes;
	FSlateBrush FrameBrush;
	FSlateBrush ClassVesselBrush;
	FSlateBrush LifeVesselBrush;
	FSlateBrush TailBrush;
	FSlateBrush SlotBrush;
	FSlateBrush MedallionBrush;
	FSlateBrush BadgeBrush;
	FSlateBrush DodgeCooldownBrush;
	FSlateBrush KeimpelzBarBrush;
	TArray<FSlateBrush> CooldownBrushes;

	FSlateFontInfo TitleFont;
	FSlateFontInfo LabelFont;
	FSlateFontInfo SmallFont;
	FSlateFontInfo NumberFont;

	/** Gathered in the tick */
	TArray<FSlotState> Slots;
	TArray<FStripEntry> Strip;
	TArray<FStripEntry> QuickSlots;
	FText StripLabel;

	/** Colors of the elements the surroundings give right now, looked up a few times a second */
	TArray<FLinearColor> NearbyElements;
	float NextNearbyCheck = 0.0f;
	FText PlayerLine;
	FText LevelText;
	FText PotionText;
	FText HealthText;
	FText ResourceText;

	/** The Keimpelz pelt of a Floromant: shown at all, and its number */
	bool bShowKeimpelz = false;
	FText KeimpelzText;
	EVaelInputGlyphs Glyphs = EVaelInputGlyphs::Keyboard;
	int32 PlayerSlot = 0;
	EVaelPath Path = EVaelPath::None;
	float DodgeLeft = 0.0f;
	bool bHasPlayer = false;

	/** True while the frame of the class is the painted one: its corners and middle are part of the picture */
	bool bPaintedFrame = false;
};
