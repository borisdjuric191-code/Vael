// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "VaelHUDClassData.generated.h"

class UMaterialInterface;
class UTexture2D;

/** Button an ability lies on; the HUD draws the symbol of the device the player uses */
UENUM(BlueprintType)
enum class EVaelHUDButton : uint8
{
	Cross,
	Circle,
	Square,
	Triangle,
	R1,
	R2,
	L1,
	L1R1,
	DPad,
	L2,
	L2R2
};

/** What an ability slot shows the state of */
UENUM(BlueprintType)
enum class EVaelHUDAbility : uint8
{
	/** Only a picture and a name, for classes that can't be played yet */
	Placeholder,
	ElementFire,
	ElementWater,
	ElementEarth,
	ElementAir,
	ElementNid,
	/** Casts the queued elements of the mage */
	Cast,
	GardenCall,
	Sow,
	RootGrip,
	RootLeap,
	Overgrowth,
	ThornWhip,
	/** A hand of the Floromant: the plant on it, its colour, and how empty its plant bar is */
	HandLeft,
	HandRight,
	/** Reloading the hands from the Keimpelz pelt */
	Reload
};

/** What the small strip above the abilities shows */
UENUM(BlueprintType)
enum class EVaelHUDStrip : uint8
{
	None,
	/** The queued elements and the quick slots of the mage */
	ElementQueue,
	/** The four seeds of the Floromant's seed belt */
	SeedBelt,
	/** The plants of the seed belt of the Floromant's hands */
	HandBelt
};

/** What fills the class vessel */
UENUM(BlueprintType)
enum class EVaelHUDResource : uint8
{
	/** Mana of the character: the mana of the mage, the Saft of the Floromant */
	Mana,
	/** A slowly moving test filling, for classes that can't be played yet */
	Preview,
	/** Two test fillings, upper and lower chamber, like the prophet's Verehrung and Furcht */
	PreviewTwo
};

/** One slot of the ability bar */
USTRUCT(BlueprintType)
struct FVaelHUDAbilitySlot
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category="Ability")
	FText Name;

	/** Picture of the ability; stroke placeholders now, watercolour pictures later */
	UPROPERTY(EditAnywhere, Category="Ability")
	TSoftObjectPtr<UTexture2D> Icon;

	UPROPERTY(EditAnywhere, Category="Ability")
	EVaelHUDButton Button = EVaelHUDButton::Cross;

	/** Key shown while the player uses the keyboard */
	UPROPERTY(EditAnywhere, Category="Ability")
	FText KeyboardKey;

	UPROPERTY(EditAnywhere, Category="Ability")
	EVaelHUDAbility Ability = EVaelHUDAbility::Placeholder;
};

/**
 *  Look of the player HUD for one class: frame, class vessel, ornaments and abilities.
 *  One asset per class (DA_HUDKlasse_<Klasse>); a new class only needs a new asset.
 */
UCLASS(BlueprintType)
class UVaelHUDClassData : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:

	/** Name shown above the bar in the title font */
	UPROPERTY(EditAnywhere, Category="Class")
	FText ClassName;

	/** Name of what fills the class vessel, like Mana or Saft */
	UPROPERTY(EditAnywhere, Category="Class")
	FText ResourceName;

	UPROPERTY(EditAnywhere, Category="Class")
	EVaelHUDResource Resource = EVaelHUDResource::Mana;

	/** Colour of the frame and the ornaments, and the second colour of the class */
	UPROPERTY(EditAnywhere, Category="Look")
	FLinearColor FrameColor = FLinearColor(0.29f, 0.45f, 0.66f);

	UPROPERTY(EditAnywhere, Category="Look")
	FLinearColor AccentColor = FLinearColor(0.8f, 0.7f, 0.5f);

	/** Frame of the panel, an instance of M_HUD_Rahmen */
	UPROPERTY(EditAnywhere, Category="Look")
	TSoftObjectPtr<UMaterialInterface> FrameMaterial;

	/** Class vessel, an instance of M_HUD_Gefaess with its shape and liquid */
	UPROPERTY(EditAnywhere, Category="Look")
	TSoftObjectPtr<UMaterialInterface> VesselMaterial;

	/** Background of the ability slots, an instance of M_HUD_Feld (the Floromant's are rounded like buds) */
	UPROPERTY(EditAnywhere, Category="Look")
	TSoftObjectPtr<UMaterialInterface> SlotMaterial;

	/** Ornament in the corners of the frame, in the middle of the bar and at the start of the experience bar */
	UPROPERTY(EditAnywhere, Category="Look")
	TSoftObjectPtr<UTexture2D> CornerOrnament;

	UPROPERTY(EditAnywhere, Category="Look")
	TSoftObjectPtr<UTexture2D> CenterOrnament;

	UPROPERTY(EditAnywhere, Category="Look")
	TSoftObjectPtr<UTexture2D> BarEnd;

	/** The painted frame brings the head and tail of the dragon around the experience bar itself */
	UPROPERTY(EditAnywhere, Category="Look")
	bool bDragonInFrame = false;

	/** Head of the dragon at the end of the experience bar, a picture of its own that winds around the buttons beside it */
	UPROPERTY(EditAnywhere, Category="Look")
	TSoftObjectPtr<UTexture2D> DragonPicture;

	/** Tail of the dragon at the start of the experience bar, the same way */
	UPROPERTY(EditAnywhere, Category="Look")
	TSoftObjectPtr<UTexture2D> DragonTailPicture;

	/** Scales laid over the fill of the experience bar */
	UPROPERTY(EditAnywhere, Category="Look")
	TSoftObjectPtr<UTexture2D> ScalesPicture;

	/** Picture of the class for the level badge on its vessel */
	UPROPERTY(EditAnywhere, Category="Look")
	TSoftObjectPtr<UTexture2D> ClassIcon;

	/** Painted pieces (rendered in Blender by Tools/HUD/vael_hud_gemalt.py); each one, where set, takes the place of the drawn placeholder.
	 *  The frame with its mask (red: the panel inside, green: gems, seals and blossoms that glow on the way of shadow) goes into FramePictureMaterial. */
	UPROPERTY(EditAnywhere, Category="Painted")
	TSoftObjectPtr<UTexture2D> FramePicture;

	UPROPERTY(EditAnywhere, Category="Painted")
	TSoftObjectPtr<UTexture2D> FrameMask;

	/** Material for the painted frame, an instance of M_HUD_Rahmen_Bild */
	UPROPERTY(EditAnywhere, Category="Painted")
	TSoftObjectPtr<UMaterialInterface> FramePictureMaterial;

	/** Glass, metal and claw of the class vessel; the liquid is painted behind it from the vessel material */
	UPROPERTY(EditAnywhere, Category="Painted")
	TSoftObjectPtr<UTexture2D> VesselPicture;

	/** Rim of the ability slots and of the small round buttons */
	UPROPERTY(EditAnywhere, Category="Painted")
	TSoftObjectPtr<UTexture2D> SlotPicture;

	UPROPERTY(EditAnywhere, Category="Painted")
	TSoftObjectPtr<UTexture2D> MedallionPicture;

	UPROPERTY(EditAnywhere, Category="Abilities")
	TArray<FVaelHUDAbilitySlot> Abilities;

	UPROPERTY(EditAnywhere, Category="Abilities")
	EVaelHUDStrip Strip = EVaelHUDStrip::None;
};
