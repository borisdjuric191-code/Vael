// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/VaelPlayerHUDWidget.h"
#include "Engine/Font.h"
#include "Engine/FontFace.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "Fonts/CompositeFont.h"
#include "Fonts/FontMeasure.h"
#include "Floromant/VaelFloromantComponent.h"
#include "Floromant/VaelPlantArmComponent.h"
#include "Floromant/VaelPlantArmData.h"
#include "Framework/Application/SlateApplication.h"
#include "Items/VaelMaterialBag.h"
#include "Magic/VaelElementComponent.h"
#include "Magic/VaelEnvironmentStatics.h"
#include "Magic/VaelFormula.h"
#include "Magic/VaelGrimoireSubsystem.h"
#include "Magic/VaelMagicSettings.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Player/VaelCharacter.h"
#include "Player/VaelPlayerController.h"
#include "Rendering/DrawElements.h"
#include "Styling/CoreStyle.h"
#include "VaelAssets.h"

#define LOCTEXT_NAMESPACE "VaelPlayerHUD"

namespace
{
	/** Folder of the shared pictures and materials, made by Scripts/CreateHudKit.py */
	const TCHAR* HudFolder = TEXT("/Game/Vael/UI/HUD");

	template<typename T>
	TSoftObjectPtr<T> HudAsset(const FString& Name, const FString& SubFolder = FString())
	{
		const FString Folder = SubFolder.IsEmpty() ? FString(HudFolder) : FString(HudFolder) / SubFolder;
		return TSoftObjectPtr<T>(FSoftObjectPath(FString::Printf(TEXT("%s/%s.%s"), *Folder, *Name, *Name)));
	}

	/** Layout of the panel in design pixels, from its top left corner */
	constexpr float FrameTop = 28.0f;
	constexpr float VesselSize = 182.0f;
	constexpr float ClawSize = 178.0f;
	const FVector2f LifeCenter(94.0f, 132.0f);
	const FVector2f ClassCenter(786.0f, 132.0f);
	constexpr float SlotSize = 76.0f;
	constexpr float SlotGap = 6.0f;
	constexpr float SlotTop = 116.0f;
	constexpr float StripTop = 92.0f;
	constexpr float TailLeft = 206.0f;
	constexpr float TailRight = 664.0f;
	constexpr float TailTop = 212.0f;
	constexpr float TailHeight = 14.0f;
	constexpr float CornerSize = 34.0f;

	/** Painted vessels are rendered a little larger than their mask, with room for the claw; see Tools/HUD/vael_hud_gemalt.py */
	constexpr float VesselPictureScale = 1.16f;

	/** Margin to the edge of the screen and between stacked panels */
	constexpr float ScreenMargin = 20.0f;
	constexpr float StackGap = 12.0f;

	/** Slate font sizes are points at 96 dpi: a size of 0.75 per pixel of height */
	constexpr float PointsPerPixel = 0.75f;

	/** Material parameters */
	const FName FillParameter(TEXT("Fill"));
	const FName FillTopParameter(TEXT("FillTop"));
	const FName RemainingParameter(TEXT("Remaining"));
	const FName CorruptionParameter(TEXT("Corruption"));
	const FName LightParameter(TEXT("Light"));
	const FName ShadowParameter(TEXT("Shadow"));
	const FName TwilightParameter(TEXT("Twilight"));
	const FName GlassAmountParameter(TEXT("GlassAmount"));

	/** Colour from 8 bit sRGB values */
	FLinearColor Hex(uint8 R, uint8 G, uint8 B, uint8 A = 255)
	{
		return FLinearColor(FColor(R, G, B, A));
	}

	FSlateFontInfo MakeFont(UFontFace* Face, float PixelHeight)
	{
		if (Face == nullptr)
		{
			return FCoreStyle::GetDefaultFontStyle("Regular", FMath::RoundToInt(PixelHeight * PointsPerPixel));
		}

		TSharedRef<FCompositeFont> Composite = MakeShared<FCompositeFont>();
		FTypefaceEntry& Entry = Composite->DefaultTypeface.Fonts.Add_GetRef(FTypefaceEntry(TEXT("Regular")));
		Entry.Font = FFontData(Face);
		return FSlateFontInfo(Composite, PixelHeight * PointsPerPixel);
	}
}

UVaelPlayerHUDWidget::UVaelPlayerHUDWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	LifeVesselMaterial = HudAsset<UMaterialInterface>(TEXT("MI_HUD_Gefaess_Leben"));
	LifeVesselPicture = HudAsset<UTexture2D>(TEXT("T_HUD_Gemalt_Leben"), TEXT("Gemalt"));
	CooldownMaterial = HudAsset<UMaterialInterface>(TEXT("M_HUD_Wisch"));
	TailMaterial = HudAsset<UMaterialInterface>(TEXT("MI_HUD_Schwanz"));
	MedallionMaterial = HudAsset<UMaterialInterface>(TEXT("MI_HUD_Medaillon"));
	BadgeMaterial = HudAsset<UMaterialInterface>(TEXT("MI_HUD_Abzeichen"));
	ClawTexture = HudAsset<UTexture2D>(TEXT("T_HUD_Klaue"), TEXT("Formen"));
	ClawThornsTexture = HudAsset<UTexture2D>(TEXT("T_HUD_Klaue_Dornen"), TEXT("Formen"));
	DragonHeadTexture = HudAsset<UTexture2D>(TEXT("T_HUD_Drachenkopf"), TEXT("Formen"));
	PathTextures[0] = HudAsset<UTexture2D>(TEXT("T_HUD_Weg_Licht"), TEXT("Formen"));
	PathTextures[1] = HudAsset<UTexture2D>(TEXT("T_HUD_Weg_Schatten"), TEXT("Formen"));
	PathTextures[2] = HudAsset<UTexture2D>(TEXT("T_HUD_Weg_Zwielicht"), TEXT("Formen"));
	PotionIcon = HudAsset<UTexture2D>(TEXT("T_Symbol_Trank"), TEXT("Symbole"));
	DodgeIcon = HudAsset<UTexture2D>(TEXT("T_Symbol_Ausweichen"), TEXT("Symbole"));
	SeedIcon = HudAsset<UTexture2D>(TEXT("T_Symbol_Saat"), TEXT("Symbole"));
	KeimpelzBarMaterial = HudAsset<UMaterialInterface>(TEXT("MI_HUD_Pelz"));
	MenuIcons[0] = HudAsset<UTexture2D>(TEXT("T_Symbol_Adernbaum"), TEXT("Symbole"));
	MenuIcons[1] = HudAsset<UTexture2D>(TEXT("T_Symbol_Rezeptbuch"), TEXT("Symbole"));
	MenuIcons[2] = HudAsset<UTexture2D>(TEXT("T_Symbol_Karte"), TEXT("Symbole"));
	TitleFontFace = TSoftObjectPtr<UFontFace>(FSoftObjectPath(TEXT("/Game/Vael/UI/Fonts/IMFeENsc28P.IMFeENsc28P")));
	TextFontFace = TSoftObjectPtr<UFontFace>(FSoftObjectPath(TEXT("/Game/Vael/UI/Fonts/AlegreyaSans-Regular.AlegreyaSans-Regular")));
	TextBoldFontFace = TSoftObjectPtr<UFontFace>(FSoftObjectPath(TEXT("/Game/Vael/UI/Fonts/AlegreyaSans-Bold.AlegreyaSans-Bold")));
	GroundColor = Hex(0x15, 0x12, 0x0F);
	TextColor = Hex(0xE9, 0xDF, 0xCC);
}

void UVaelPlayerHUDWidget::SetPlayer(AVaelPlayerController* InPlayer)
{
	Player = InPlayer;
}

void UVaelPlayerHUDWidget::NativeConstruct()
{
	Super::NativeConstruct();

	SetVisibility(ESlateVisibility::HitTestInvisible);
	BuildFonts();

	// Shared pictures, loaded once so painting only looks them up
	for (const TSoftObjectPtr<UTexture2D>& Texture : { LifeVesselPicture, ClawTexture, ClawThornsTexture, DragonHeadTexture, PathTextures[0], PathTextures[1], PathTextures[2],
		PotionIcon, DodgeIcon, SeedIcon, MenuIcons[0], MenuIcons[1], MenuIcons[2] })
	{
		GetTextureBrush(Texture);
	}

	if (UMaterialInterface* Life = VaelAssets::LoadOptional(LifeVesselMaterial))
	{
		LifeVesselInstance = UMaterialInstanceDynamic::Create(Life, this);
		SetMaterialBrush(LifeVesselBrush, LifeVesselInstance);

		// The painted glass lies over the liquid: the drawn glass steps back
		if (FindTextureBrush(LifeVesselPicture) != nullptr)
		{
			LifeVesselInstance->SetScalarParameterValue(GlassAmountParameter, 0.0f);
		}
	}
	if (UMaterialInterface* Tail = VaelAssets::LoadOptional(TailMaterial))
	{
		TailInstance = UMaterialInstanceDynamic::Create(Tail, this);
		SetMaterialBrush(TailBrush, TailInstance);
	}
	SetMaterialBrush(MedallionBrush, VaelAssets::LoadOptional(MedallionMaterial));
	SetMaterialBrush(BadgeBrush, VaelAssets::LoadOptional(BadgeMaterial));

	if (UMaterialInterface* Wipe = VaelAssets::LoadOptional(CooldownMaterial))
	{
		DodgeCooldownInstance = UMaterialInstanceDynamic::Create(Wipe, this);
		SetMaterialBrush(DodgeCooldownBrush, DodgeCooldownInstance);
	}
	if (UMaterialInterface* Pelt = VaelAssets::LoadOptional(KeimpelzBarMaterial))
	{
		KeimpelzBarInstance = UMaterialInstanceDynamic::Create(Pelt, this);
		SetMaterialBrush(KeimpelzBarBrush, KeimpelzBarInstance);
	}
}

void UVaelPlayerHUDWidget::BuildFonts()
{
	UFontFace* Title = VaelAssets::LoadOptional(TitleFontFace);
	UFontFace* Text = VaelAssets::LoadOptional(TextFontFace);
	UFontFace* Bold = VaelAssets::LoadOptional(TextBoldFontFace);
	for (UFontFace* Face : { Title, Text, Bold })
	{
		if (Face != nullptr)
		{
			LoadedResources.Add(Face);
		}
	}

	TitleFont = MakeFont(Title, 30.0f);
	LabelFont = MakeFont(Text, 17.0f);
	SmallFont = MakeFont(Text, 13.0f);
	NumberFont = MakeFont(Bold != nullptr ? Bold : Text, 18.0f);
}

void UVaelPlayerHUDWidget::SetMaterialBrush(FSlateBrush& Brush, UMaterialInterface* Material)
{
	Brush = FSlateBrush();
	if (Material != nullptr)
	{
		Brush.SetResourceObject(Material);
		Brush.ImageSize = FVector2D(64.0f, 64.0f);
		Brush.DrawAs = ESlateBrushDrawType::Image;
	}
}

const FSlateBrush* UVaelPlayerHUDWidget::GetTextureBrush(const TSoftObjectPtr<UTexture2D>& Texture)
{
	if (Texture.IsNull())
	{
		return nullptr;
	}

	if (const TSharedPtr<FSlateBrush>* Found = TextureBrushes.Find(Texture.ToSoftObjectPath()))
	{
		return Found->Get();
	}

	UTexture2D* Loaded = VaelAssets::LoadOptional(Texture);
	if (Loaded == nullptr)
	{
		return nullptr;
	}

	LoadedResources.Add(Loaded);
	TSharedPtr<FSlateBrush> Brush = MakeShared<FSlateBrush>();
	Brush->SetResourceObject(Loaded);
	Brush->ImageSize = FVector2D(Loaded->GetSizeX(), Loaded->GetSizeY());
	TextureBrushes.Add(Texture.ToSoftObjectPath(), Brush);
	return Brush.Get();
}

const FSlateBrush* UVaelPlayerHUDWidget::FindTextureBrush(const TSoftObjectPtr<UTexture2D>& Texture) const
{
	const TSharedPtr<FSlateBrush>* Found = TextureBrushes.Find(Texture.ToSoftObjectPath());
	return Found != nullptr ? Found->Get() : nullptr;
}

UVaelHUDClassData* UVaelPlayerHUDWidget::FindClassData(const AVaelCharacter* Character) const
{
	FString Name = PreviewClass;
	if (Name.IsEmpty())
	{
		Name = Character != nullptr && Character->IsFloromant() ? TEXT("Floromant") : TEXT("Magier");
	}

	const FString DataPath = FString::Printf(TEXT("%s/DA_HUDKlasse_%s.DA_HUDKlasse_%s"), *UVaelUISettings::Get()->HUDClassFolder, *Name, *Name);
	return Cast<UVaelHUDClassData>(VaelAssets::LoadOptional(FSoftObjectPath(DataPath)));
}

void UVaelPlayerHUDWidget::ApplyClassData(UVaelHUDClassData* Data)
{
	ShownData = Data;
	FrameInstance = nullptr;
	ClassVesselInstance = nullptr;
	CooldownInstances.Reset();
	CooldownBrushes.Reset();

	if (Data == nullptr)
	{
		return;
	}

	// The painted frame where there is one, laid into its material for the veins and the ways; the drawn one otherwise
	UTexture2D* FramePicture = VaelAssets::LoadOptional(Data->FramePicture);
	UMaterialInterface* FramePictureMaterial = VaelAssets::LoadOptional(Data->FramePictureMaterial);
	if (FramePicture != nullptr && FramePictureMaterial != nullptr)
	{
		FrameInstance = UMaterialInstanceDynamic::Create(FramePictureMaterial, this);
		FrameInstance->SetTextureParameterValue(TEXT("Picture"), FramePicture);
		if (UTexture2D* FrameMask = VaelAssets::LoadOptional(Data->FrameMask))
		{
			FrameInstance->SetTextureParameterValue(TEXT("Mask"), FrameMask);
		}
		bPaintedFrame = true;
	}
	else if (UMaterialInterface* Frame = VaelAssets::LoadOptional(Data->FrameMaterial))
	{
		FrameInstance = UMaterialInstanceDynamic::Create(Frame, this);
		bPaintedFrame = false;
	}
	SetMaterialBrush(FrameBrush, FrameInstance);

	if (UMaterialInterface* Vessel = VaelAssets::LoadOptional(Data->VesselMaterial))
	{
		ClassVesselInstance = UMaterialInstanceDynamic::Create(Vessel, this);
		if (GetTextureBrush(Data->VesselPicture) != nullptr)
		{
			ClassVesselInstance->SetScalarParameterValue(GlassAmountParameter, 0.0f);
		}
	}
	SetMaterialBrush(ClassVesselBrush, ClassVesselInstance);
	GetTextureBrush(Data->SlotPicture);
	GetTextureBrush(Data->MedallionPicture);
	SetMaterialBrush(SlotBrush, VaelAssets::LoadOptional(Data->SlotMaterial));

	GetTextureBrush(Data->CornerOrnament);
	GetTextureBrush(Data->CenterOrnament);
	GetTextureBrush(Data->BarEnd);
	GetTextureBrush(Data->ClassIcon);

	UMaterialInterface* Wipe = VaelAssets::LoadOptional(CooldownMaterial);
	for (const FVaelHUDAbilitySlot& Ability : Data->Abilities)
	{
		GetTextureBrush(Ability.Icon);
		UMaterialInstanceDynamic* Instance = Wipe != nullptr ? UMaterialInstanceDynamic::Create(Wipe, this) : nullptr;
		CooldownInstances.Add(Instance);
		SetMaterialBrush(CooldownBrushes.AddDefaulted_GetRef(), Instance);
	}
}

void UVaelPlayerHUDWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	const AVaelPlayerController* Controller = Player.Get();
	const AVaelCharacter* Character = Controller != nullptr ? Controller->GetPawn<AVaelCharacter>() : nullptr;
	bHasPlayer = Character != nullptr;
	if (!bHasPlayer)
	{
		return;
	}

	UVaelHUDClassData* Data = FindClassData(Character);
	if (Data != ShownData)
	{
		ApplyClassData(Data);
	}
	if (ShownData == nullptr)
	{
		return;
	}

	const float Time = GetWorld()->GetRealTimeSeconds();
	PlayerSlot = Controller->GetPlayerSlot();
	Glyphs = Controller->GetInputGlyphs();
	Path = Character->GetPath();
	DodgeLeft = Character->GetDodgeCooldownFraction();

	PlayerLine = FText::Format(LOCTEXT("PlayerLine", "Spieler {0} · Stufe {1}"), PlayerSlot + 1, Character->GetLevel());
	LevelText = FText::AsNumber(Character->GetLevel());
	PotionText = FText::AsNumber(Character->GetPotionCount());
	HealthText = FText::AsNumber(FMath::CeilToInt(Character->GetHealth()));

	// Vessels: life for everyone; the class vessel holds mana or Saft, or moves on its own for classes that can't be played yet
	const float LifeFill = Character->GetHealth() / FMath::Max(Character->GetMaxHealth(), 1.0f);
	if (LifeVesselInstance != nullptr)
	{
		LifeVesselInstance->SetScalarParameterValue(FillParameter, LifeFill);
	}

	float ClassFill = Character->GetMana() / FMath::Max(Character->GetMaxMana(), 1.0f);
	float ClassFillTop = 0.0f;
	ResourceText = FText::AsNumber(FMath::FloorToInt(Character->GetMana()));
	if (ShownData->Resource != EVaelHUDResource::Mana)
	{
		ClassFill = 0.55f + 0.35f * FMath::Sin(Time * 0.4f + PlayerSlot);
		ClassFillTop = 0.5f + 0.4f * FMath::Sin(Time * 0.3f + 1.7f);
		ResourceText = FText::AsNumber(FMath::RoundToInt(ClassFill * 100.0f));
	}
	if (ClassVesselInstance != nullptr)
	{
		ClassVesselInstance->SetScalarParameterValue(FillParameter, ClassFill);
		ClassVesselInstance->SetScalarParameterValue(FillTopParameter, ClassFillTop);
	}

	// Frame: Nid veins creep in with the corruption; after the oath the way changes it
	if (FrameInstance != nullptr)
	{
		FrameInstance->SetScalarParameterValue(CorruptionParameter, Character->GetCorruption() / 100.0f);
		FrameInstance->SetScalarParameterValue(LightParameter, Path == EVaelPath::Light ? 1.0f : 0.0f);
		FrameInstance->SetScalarParameterValue(ShadowParameter, Path == EVaelPath::Shadow ? 1.0f : 0.0f);
		FrameInstance->SetScalarParameterValue(TwilightParameter, Path == EVaelPath::Twilight ? 1.0f : 0.0f);
	}

	if (TailInstance != nullptr)
	{
		TailInstance->SetScalarParameterValue(FillParameter, Character->GetExperienceFraction());
	}
	if (DodgeCooldownInstance != nullptr)
	{
		DodgeCooldownInstance->SetScalarParameterValue(RemainingParameter, DodgeLeft);
	}

	// Abilities
	const UVaelElementComponent* Elements = Character->GetElementComponent();
	const UVaelFloromantComponent* Floromant = Character->GetFloromant();
	const UVaelGrimoireSubsystem* Grimoire = GetGameInstance() != nullptr ? GetGameInstance()->GetSubsystem<UVaelGrimoireSubsystem>() : nullptr;
	const UVaelMagicSettings* MagicSettings = UVaelMagicSettings::Get();
	const TArray<EVaelElement>& Queue = Elements->GetQueue();
	const UVaelFormula* QueuedFormula = Grimoire != nullptr && !Queue.IsEmpty() ? Grimoire->FindFormula(Queue) : nullptr;
	const bool bPlayable = PreviewClass.IsEmpty();

	Slots.Reset();
	for (int32 Index = 0; Index < ShownData->Abilities.Num(); ++Index)
	{
		const FVaelHUDAbilitySlot& Ability = ShownData->Abilities[Index];
		FSlotState& State = Slots.AddDefaulted_GetRef();
		State.Name = Ability.Name;
		State.Icon = FindTextureBrush(Ability.Icon);
		State.Button = Ability.Button;
		State.KeyboardKey = Ability.KeyboardKey;
		State.IconColor = TextColor;

		float Total = 0.0f;
		switch (Ability.Ability)
		{
		case EVaelHUDAbility::ElementFire:
		case EVaelHUDAbility::ElementWater:
		case EVaelHUDAbility::ElementEarth:
		case EVaelHUDAbility::ElementAir:
		case EVaelHUDAbility::ElementNid:
		{
			const EVaelElement Element = static_cast<EVaelElement>(static_cast<int32>(Ability.Ability) - static_cast<int32>(EVaelHUDAbility::ElementFire));
			State.IconColor = MagicSettings->GetElementColor(Element);
			State.bLit = bPlayable && Queue.Contains(Element);
			State.bDim = Element == EVaelElement::Mark && (Grimoire == nullptr || !Grimoire->IsMarkAwakened());
			break;
		}
		case EVaelHUDAbility::Cast:
			State.bLit = bPlayable && !Queue.IsEmpty();
			if (QueuedFormula != nullptr && Grimoire->IsFormulaKnown(QueuedFormula))
			{
				State.Name = QueuedFormula->DisplayName;
			}
			break;
		case EVaelHUDAbility::HandLeft:
		case EVaelHUDAbility::HandRight:
		{
			// The plant on the hand in its colour; the dark wipe shows how empty its plant bar is
			const UVaelPlantArmComponent* Hand = Floromant != nullptr ? Floromant->FindHand(Ability.Ability == EVaelHUDAbility::HandLeft) : nullptr;
			const UVaelPlantArmData* HandPlant = Hand != nullptr ? Hand->GetPlant() : nullptr;
			if (HandPlant != nullptr)
			{
				const bool bShadowForm = Path == EVaelPath::Shadow;
				State.Name = HandPlant->GetDisplayName(bShadowForm);
				State.IconColor = HandPlant->GetColor(bShadowForm);
				State.bLit = Hand->IsFiring();
				State.CooldownLeft = 1.0f - Hand->GetBarFraction();
				Total = 1.0f;
			}
			else
			{
				State.bDim = true;
			}
			State.bNoSeconds = true;
			break;
		}
		case EVaelHUDAbility::Reload:
		{
			// Lights while a hand lacks shots and the pelt can fill it; the wipe runs while the stalks draw the moss in
			const UVaelPlantArmComponent* Hands[] = { Floromant != nullptr ? Floromant->FindHand(true) : nullptr, Floromant != nullptr ? Floromant->FindHand(false) : nullptr };
			for (const UVaelPlantArmComponent* Hand : Hands)
			{
				if (Hand != nullptr)
				{
					State.CooldownLeft = FMath::Max(State.CooldownLeft, Hand->GetReloadLeft());
					State.bLit = State.bLit || Hand->GetMissingShots() > 0;
				}
			}
			State.bLit = State.bLit && Floromant->GetKeimpelz() >= 1.0f;
			State.bDim = Floromant == nullptr || Floromant->GetKeimpelz() < 1.0f;
			Total = 1.0f;
			State.bNoSeconds = true;
			break;
		}
		case EVaelHUDAbility::Placeholder:
			break;
		default:
			if (bPlayable && Floromant != nullptr)
			{
				State.CooldownLeft = Floromant->GetCooldownLeft(Ability.Ability);
				Total = Floromant->GetCooldownDuration(Ability.Ability);
			}
			break;
		}

		if (CooldownInstances.IsValidIndex(Index) && CooldownInstances[Index] != nullptr)
		{
			CooldownInstances[Index]->SetScalarParameterValue(RemainingParameter, Total > 0.0f ? FMath::Clamp(State.CooldownLeft / Total, 0.0f, 1.0f) : 0.0f);
		}
	}

	// The small strip above the abilities
	Strip.Reset();
	QuickSlots.Reset();
	StripLabel = FText::GetEmpty();
	if (ShownData->Strip != EVaelHUDStrip::ElementQueue || !bPlayable)
	{
		NearbyElements.Reset();
	}
	else if (GetWorld()->GetTimeSeconds() >= NextNearbyCheck)
	{
		// Elements the surroundings give right now: drawn from there they make the formula cheaper and stronger
		NextNearbyCheck = GetWorld()->GetTimeSeconds() + 0.25f;
		NearbyElements.Reset();
		for (const EVaelElement Element : { EVaelElement::Fire, EVaelElement::Water, EVaelElement::Earth, EVaelElement::Air, EVaelElement::Mark })
		{
			if ((Element != EVaelElement::Mark || (Grimoire != nullptr && Grimoire->IsMarkAwakened())) && UVaelEnvironmentStatics::IsElementInEnvironment(Character, Element))
			{
				NearbyElements.Add(MagicSettings->GetElementColor(Element));
			}
		}
	}

	if (ShownData->Strip == EVaelHUDStrip::ElementQueue)
	{
		for (int32 Index = 0; Index < Elements->GetNumSlots(); ++Index)
		{
			FStripEntry& Entry = Strip.AddDefaulted_GetRef();
			Entry.bFilled = bPlayable && Queue.IsValidIndex(Index);
			Entry.Color = Entry.bFilled ? MagicSettings->GetElementColor(Queue[Index]) : FLinearColor::White;
			Entry.bPicked = Entry.bFilled && Elements->IsFromEnvironment(Index);
		}
		for (int32 Index = 0; Index < UVaelElementComponent::NumQuickSlots; ++Index)
		{
			FStripEntry& Entry = QuickSlots.AddDefaulted_GetRef();
			const UVaelFormula* Formula = bPlayable ? Elements->GetQuickSlotFormula(Index) : nullptr;
			Entry.bFilled = Formula != nullptr && !Formula->Elements.IsEmpty();
			Entry.Color = Entry.bFilled ? MagicSettings->GetElementColor(Formula->Elements[0]) : FLinearColor::White;
			Entry.bPicked = Entry.bFilled && Elements->GetQuickSlotCooldownFraction(Index) <= 0.0f;
		}
		StripLabel = Queue.IsEmpty() || !bPlayable ? LOCTEXT("ChooseElements", "Elemente wählen")
			: QueuedFormula != nullptr && Grimoire->IsFormulaKnown(QueuedFormula) ? QueuedFormula->DisplayName : LOCTEXT("Unknown", "Unbekannte Formel");
	}
	else if (ShownData->Strip == EVaelHUDStrip::SeedBelt && Floromant != nullptr)
	{
		for (int32 Index = 0; Index < Floromant->GetBeltSize(); ++Index)
		{
			FStripEntry& Entry = Strip.AddDefaulted_GetRef();
			Entry.bFilled = true;
			Entry.Color = Floromant->GetBeltColor(Index);
			Entry.bPicked = Index == Floromant->GetBeltSelection();
			Entry.Label = Floromant->GetBeltName(Index);
		}
		StripLabel = Floromant->GetBeltName(Floromant->GetBeltSelection());
	}
	else if (ShownData->Strip == EVaelHUDStrip::HandBelt && Floromant != nullptr)
	{
		// The seed belt of the hands: the picked plant is laid on a hand with the d-pad left / right
		const bool bShadowForm = Path == EVaelPath::Shadow;
		for (int32 Index = 0; Index < Floromant->GetHandBeltSize(); ++Index)
		{
			const UVaelPlantArmData* BeltPlant = Floromant->GetHandBeltPlant(Index);
			FStripEntry& Entry = Strip.AddDefaulted_GetRef();
			Entry.bFilled = BeltPlant != nullptr;
			Entry.Color = BeltPlant != nullptr ? BeltPlant->GetColor(bShadowForm) : FLinearColor::Gray;
			Entry.bPicked = Index == Floromant->GetHandBeltSelection();
			Entry.Label = BeltPlant != nullptr ? BeltPlant->GetDisplayName(bShadowForm) : FText::GetEmpty();
		}
		const UVaelPlantArmData* Picked = Floromant->GetHandBeltPlant(Floromant->GetHandBeltSelection());
		StripLabel = Picked != nullptr ? Picked->GetDisplayName(bShadowForm) : FText::GetEmpty();
	}

	// The Keimpelz pelt of a Floromant, as a furry bar over the panel that shrinks to the left and grows back to the right
	bShowKeimpelz = bPlayable && Character->IsFloromant() && Floromant != nullptr && KeimpelzBarInstance != nullptr;
	if (bShowKeimpelz)
	{
		KeimpelzBarInstance->SetScalarParameterValue(FillParameter, Floromant->GetKeimpelzFraction());
		KeimpelzText = FText::AsNumber(FMath::FloorToInt(Floromant->GetKeimpelz()));
	}
}

// ---------------------------------------------------------------- Painting

void UVaelPlayerHUDWidget::PaintBox(FSlateWindowElementList& Out, int32 Layer, const FSlateBrush* Brush, const FVector2f& Position, const FVector2f& Size, const FLinearColor& Tint) const
{
	if (Brush == nullptr || Brush->GetResourceObject() == nullptr)
	{
		return;
	}
	FSlateDrawElement::MakeBox(Out, Layer, GetCachedGeometry().ToPaintGeometry(Size, FSlateLayoutTransform(Position)), Brush, ESlateDrawEffect::None, Tint);
}

void UVaelPlayerHUDWidget::PaintText(FSlateWindowElementList& Out, int32 Layer, const FText& Text, const FSlateFontInfo& InFont, const FVector2f& Position, const FLinearColor& Color, float Alignment, float MaxWidth) const
{
	if (Text.IsEmpty())
	{
		return;
	}

	// Too wide for its place: the text gets smaller instead of running into its neighbours
	const TSharedRef<FSlateFontMeasure> Measure = FSlateApplication::Get().GetRenderer()->GetFontMeasureService();
	FSlateFontInfo Font = InFont;
	FVector2f Size = FVector2f(Measure->Measure(Text, Font));
	if (MaxWidth > 0.0f && Size.X > MaxWidth)
	{
		Font.Size = FMath::Max(5.0f, Font.Size * MaxWidth / Size.X);
		Size = FVector2f(Measure->Measure(Text, Font));
	}
	const FVector2f TopLeft(Position.X - Size.X * Alignment, Position.Y);

	// A soft dark shadow keeps the text readable on bright ground
	FSlateDrawElement::MakeText(Out, Layer, GetCachedGeometry().ToPaintGeometry(Size, FSlateLayoutTransform(TopLeft + FVector2f(1.0f, 1.5f))), Text, Font, ESlateDrawEffect::None, FLinearColor(0.0f, 0.0f, 0.0f, 0.75f * Color.A));
	FSlateDrawElement::MakeText(Out, Layer + 1, GetCachedGeometry().ToPaintGeometry(Size, FSlateLayoutTransform(TopLeft)), Text, Font, ESlateDrawEffect::None, Color);
}

void UVaelPlayerHUDWidget::PaintMedallion(FSlateWindowElementList& Out, int32 Layer, const FVector2f& Center, float Radius, const FLinearColor& Tint) const
{
	const FSlateBrush* Painted = ShownData != nullptr ? FindTextureBrush(ShownData->MedallionPicture) : nullptr;
	if (Painted == nullptr)
	{
		PaintBox(Out, Layer, &MedallionBrush, Center - FVector2f(Radius), FVector2f(Radius * 2.0f), Tint);
		return;
	}

	// Dark ground inside, the painted rim of the class around it
	PaintBox(Out, Layer, &MedallionBrush, Center - FVector2f(Radius * 0.85f), FVector2f(Radius * 1.7f), FLinearColor(0.2f, 0.18f, 0.16f));
	PaintBox(Out, Layer + 1, Painted, Center - FVector2f(Radius * 1.2f), FVector2f(Radius * 2.4f));
}

void UVaelPlayerHUDWidget::PaintLines(FSlateWindowElementList& Out, int32 Layer, const TArray<FVector2f>& Points, const FLinearColor& Color, float Thickness) const
{
	FSlateDrawElement::MakeLines(Out, Layer, GetCachedGeometry().ToPaintGeometry(), Points, ESlateDrawEffect::None, Color, true, Thickness);
}

void UVaelPlayerHUDWidget::PaintButton(FSlateWindowElementList& Out, int32 Layer, EVaelHUDButton Button, const FText& KeyboardKey, const FVector2f& Center, float Radius) const
{
	// A bright badge with a dark symbol: the PlayStation shapes on a controller, the key on the keyboard
	PaintBox(Out, Layer, &BadgeBrush, Center - FVector2f(Radius), FVector2f(Radius * 2.0f));
	const FLinearColor Ink = GroundColor;
	const float R = Radius * 0.48f;

	FText Label;
	if (Glyphs == EVaelInputGlyphs::Keyboard)
	{
		Label = KeyboardKey;
	}
	else
	{
		switch (Button)
		{
		case EVaelHUDButton::Cross:
			if (Glyphs == EVaelInputGlyphs::PlayStation)
			{
				PaintLines(Out, Layer + 1, { Center + FVector2f(-R, -R), Center + FVector2f(R, R) }, Ink, 2.0f);
				PaintLines(Out, Layer + 1, { Center + FVector2f(-R, R), Center + FVector2f(R, -R) }, Ink, 2.0f);
				return;
			}
			Label = LOCTEXT("XboxA", "A");
			break;
		case EVaelHUDButton::Circle:
			if (Glyphs == EVaelInputGlyphs::PlayStation)
			{
				TArray<FVector2f> Ring;
				for (int32 Step = 0; Step <= 20; ++Step)
				{
					const float Angle = Step * 2.0f * UE_PI / 20.0f;
					Ring.Add(Center + FVector2f(FMath::Cos(Angle), FMath::Sin(Angle)) * R * 1.05f);
				}
				PaintLines(Out, Layer + 1, Ring, Ink, 2.0f);
				return;
			}
			Label = LOCTEXT("XboxB", "B");
			break;
		case EVaelHUDButton::Square:
			if (Glyphs == EVaelInputGlyphs::PlayStation)
			{
				PaintLines(Out, Layer + 1, { Center + FVector2f(-R, -R), Center + FVector2f(R, -R), Center + FVector2f(R, R), Center + FVector2f(-R, R), Center + FVector2f(-R, -R) }, Ink, 2.0f);
				return;
			}
			Label = LOCTEXT("XboxX", "X");
			break;
		case EVaelHUDButton::Triangle:
			if (Glyphs == EVaelInputGlyphs::PlayStation)
			{
				PaintLines(Out, Layer + 1, { Center + FVector2f(0.0f, -R * 1.1f), Center + FVector2f(R * 1.1f, R * 0.8f), Center + FVector2f(-R * 1.1f, R * 0.8f), Center + FVector2f(0.0f, -R * 1.1f) }, Ink, 2.0f);
				return;
			}
			Label = LOCTEXT("XboxY", "Y");
			break;
		case EVaelHUDButton::R1:		Label = Glyphs == EVaelInputGlyphs::PlayStation ? LOCTEXT("R1", "R1") : LOCTEXT("RB", "RB"); break;
		case EVaelHUDButton::R2:		Label = Glyphs == EVaelInputGlyphs::PlayStation ? LOCTEXT("R2", "R2") : LOCTEXT("RT", "RT"); break;
		case EVaelHUDButton::L1:		Label = Glyphs == EVaelInputGlyphs::PlayStation ? LOCTEXT("L1", "L1") : LOCTEXT("LB", "LB"); break;
		case EVaelHUDButton::L1R1:		Label = Glyphs == EVaelInputGlyphs::PlayStation ? LOCTEXT("L1R1", "L1+R1") : LOCTEXT("LBRB", "LB+RB"); break;
		case EVaelHUDButton::L2:		Label = Glyphs == EVaelInputGlyphs::PlayStation ? LOCTEXT("L2", "L2") : LOCTEXT("LT", "LT"); break;
		case EVaelHUDButton::L2R2:		Label = Glyphs == EVaelInputGlyphs::PlayStation ? LOCTEXT("L2R2", "L2+R2") : LOCTEXT("LTRT", "LT+RT"); break;
		case EVaelHUDButton::DPad:		Label = LOCTEXT("DPad", "✥"); break;
		}
	}

	FSlateFontInfo Font = SmallFont;
	Font.Size = FMath::Max(6.0f, Radius * (Label.ToString().Len() > 2 ? 0.55f : 0.85f));
	const TSharedRef<FSlateFontMeasure> Measure = FSlateApplication::Get().GetRenderer()->GetFontMeasureService();
	const FVector2f Size = FVector2f(Measure->Measure(Label, Font));
	FSlateDrawElement::MakeText(Out, Layer + 1, GetCachedGeometry().ToPaintGeometry(Size, FSlateLayoutTransform(Center - Size * 0.5f)), Label, Font, ESlateDrawEffect::None, Ink);
}

int32 UVaelPlayerHUDWidget::NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements,
	int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
	const int32 Base = Super::NativePaint(Args, AllottedGeometry, MyCullingRect, OutDrawElements, LayerId, InWidgetStyle, bParentEnabled);
	if (!bHasPlayer || ShownData == nullptr)
	{
		return Base;
	}

	// Where this panel sits: player 1 bottom left, player 2 bottom right, players 3 and 4 above them
	const FVector2f Screen = FVector2f(AllottedGeometry.GetLocalSize());
	const bool bRight = PlayerSlot % 2 == 1;
	const float Row = static_cast<float>(PlayerSlot / 2);
	const FVector2f O(bRight ? Screen.X - PanelWidth - ScreenMargin : ScreenMargin, Screen.Y - PanelHeight - ScreenMargin * 0.6f - Row * (PanelHeight + StackGap));
	const FLinearColor Frame = ShownData->FrameColor;
	const FLinearColor Dim = TextColor.CopyWithNewOpacity(0.7f);
	int32 L = Base + 1;

	// Frame of the class with its Nid veins
	PaintBox(OutDrawElements, L, &FrameBrush, O + FVector2f(0.0f, FrameTop), FVector2f(PanelWidth, PanelHeight - FrameTop));

	// Corner and centre ornaments
	const FSlateBrush* Corner = bPaintedFrame ? nullptr : FindTextureBrush(ShownData->CornerOrnament);
	if (Corner != nullptr)
	{
		for (const FVector2f& At : { FVector2f(-8.0f, FrameTop - 8.0f), FVector2f(PanelWidth - CornerSize + 8.0f, FrameTop - 8.0f),
			FVector2f(-8.0f, PanelHeight - CornerSize + 6.0f), FVector2f(PanelWidth - CornerSize + 8.0f, PanelHeight - CornerSize + 6.0f) })
		{
			PaintBox(OutDrawElements, L + 4, Corner, O + At, FVector2f(CornerSize), Frame);
		}
	}
	if (!bPaintedFrame)
	{
		PaintBox(OutDrawElements, L + 4, FindTextureBrush(ShownData->CenterOrnament), O + FVector2f(PanelWidth * 0.5f - 22.0f, FrameTop - 22.0f), FVector2f(44.0f), Frame);
	}

	// The vessels, each held by a claw of the Urdrachin; on the way of shadow the claws grow thorns
	const FLinearColor ClawColor(0.11f, 0.085f, 0.07f, 1.0f);
	const FSlateBrush* Pictures[] = { FindTextureBrush(LifeVesselPicture), FindTextureBrush(ShownData->VesselPicture) };
	const FVector2f Centers[] = { LifeCenter, ClassCenter };
	for (int32 Index = 0; Index < 2; ++Index)
	{
		const FVector2f ClawAt = O + Centers[Index] - FVector2f(ClawSize * 0.5f, ClawSize * 0.5f - 6.0f);
		if (Pictures[Index] == nullptr)
		{
			PaintBox(OutDrawElements, L + 1, FindTextureBrush(ClawTexture), ClawAt, FVector2f(ClawSize), ClawColor);
		}
		if (Path == EVaelPath::Shadow)
		{
			PaintBox(OutDrawElements, L + 4, FindTextureBrush(ClawThornsTexture), ClawAt, FVector2f(ClawSize), FLinearColor(0.2f, 0.14f, 0.24f));
		}
		const float PictureSize = VesselSize * VesselPictureScale;
		PaintBox(OutDrawElements, L + 3, Pictures[Index], O + Centers[Index] - FVector2f(PictureSize * 0.5f), FVector2f(PictureSize));
	}
	PaintBox(OutDrawElements, L + 2, &LifeVesselBrush, O + LifeCenter - FVector2f(VesselSize * 0.5f), FVector2f(VesselSize));
	PaintBox(OutDrawElements, L + 2, &ClassVesselBrush, O + ClassCenter - FVector2f(VesselSize * 0.5f), FVector2f(VesselSize));
	PaintText(OutDrawElements, L + 5, HealthText, NumberFont, O + LifeCenter + FVector2f(0.0f, 26.0f), TextColor, 0.5f);
	PaintText(OutDrawElements, L + 5, ResourceText, NumberFont, O + ClassCenter + FVector2f(0.0f, 26.0f), TextColor, 0.5f);

	// Level badge on the class vessel
	const FVector2f LevelAt = O + ClassCenter + FVector2f(52.0f, -70.0f);
	PaintMedallion(OutDrawElements, L + 4, LevelAt, 17.0f, Frame);
	PaintText(OutDrawElements, L + 5, LevelText, NumberFont, LevelAt - FVector2f(0.0f, 11.0f), TextColor, 0.5f);

	// Class name, player and level, the sworn way
	const float TitleTop = bPaintedFrame ? 48.0f : FrameTop + 6.0f;
	PaintText(OutDrawElements, L + 5, ShownData->ClassName, TitleFont, O + FVector2f(190.0f, TitleTop), TextColor);
	// In the painted frame the seal stands in the middle: the line goes to the right of it
	PaintText(OutDrawElements, L + 5, PlayerLine, LabelFont, O + FVector2f(bPaintedFrame ? 484.0f : 190.0f, bPaintedFrame ? TitleTop + 8.0f : FrameTop + 40.0f), Dim);
	if (Path != EVaelPath::None)
	{
		const int32 Way = static_cast<int32>(Path) - 1;
		const FLinearColor WayColor = Path == EVaelPath::Light ? Hex(0xE1, 0xBD, 0x52) : Path == EVaelPath::Shadow ? Hex(0x9B, 0x5D, 0xE5) : Hex(0xC9, 0x9A, 0x8E);
		PaintBox(OutDrawElements, L + 4, FindTextureBrush(PathTextures[Way]), O + (bPaintedFrame ? FVector2f(346.0f, 52.0f) : FVector2f(648.0f, FrameTop + 10.0f)), FVector2f(30.0f), WayColor);
	}

	// The Keimpelz pelt over the top of the panel, its number in the middle
	if (bShowKeimpelz)
	{
		const FVector2f BarAt = O + FVector2f(196.0f, -20.0f);
		const FVector2f BarSize(488.0f, 40.0f);
		PaintBox(OutDrawElements, L + 6, &KeimpelzBarBrush, BarAt, BarSize);
		PaintText(OutDrawElements, L + 7, KeimpelzText, NumberFont, BarAt + FVector2f(BarSize.X * 0.5f, 7.0f), TextColor, 0.5f);
	}

	// Round menu buttons on the upper edge: Adernbaum, Rezeptbuch, Karte
	for (int32 Index = 0; Index < 3; ++Index)
	{
		const FVector2f At = bPaintedFrame ? O + FVector2f(626.0f + Index * 30.0f, 48.0f) : O + FVector2f(548.0f + Index * 34.0f, FrameTop);
		PaintMedallion(OutDrawElements, L + 3, At, 14.0f, Frame);
		PaintBox(OutDrawElements, L + 5, FindTextureBrush(MenuIcons[Index]), At - FVector2f(9.0f), FVector2f(18.0f), TextColor);
	}

	// Strip above the abilities: element queue and quick slots, or the seed belt
	const int32 NumSlots = Slots.Num();
	const float BarWidth = NumSlots * SlotSize + FMath::Max(NumSlots - 1, 0) * SlotGap;
	const float BarLeft = PanelWidth * 0.5f - BarWidth * 0.5f;
	if (!Strip.IsEmpty())
	{
		const bool bSeeds = ShownData->Strip == EVaelHUDStrip::SeedBelt || ShownData->Strip == EVaelHUDStrip::HandBelt;
		const float Dot = bSeeds ? 20.0f : 16.0f;
		const float Step = Dot + 6.0f;
		for (int32 Index = 0; Index < Strip.Num(); ++Index)
		{
			const FStripEntry& Entry = Strip[Index];
			const FVector2f At = O + FVector2f(BarLeft + Index * Step, StripTop);
			PaintBox(OutDrawElements, L + 3, &MedallionBrush, At, FVector2f(Dot), Entry.bPicked ? TextColor : Frame * 0.7f);
			if (bSeeds)
			{
				PaintBox(OutDrawElements, L + 4, FindTextureBrush(SeedIcon), At + FVector2f(3.0f), FVector2f(Dot - 6.0f), Entry.Color);
			}
			else if (Entry.bFilled)
			{
				PaintBox(OutDrawElements, L + 4, &BadgeBrush, At + FVector2f(3.0f), FVector2f(Dot - 6.0f), Entry.Color);
			}
		}
		if (bSeeds && Glyphs != EVaelInputGlyphs::Keyboard)
		{
			PaintButton(OutDrawElements, L + 4, EVaelHUDButton::DPad, FText::GetEmpty(), O + FVector2f(BarLeft + Strip.Num() * Step + 8.0f, StripTop + Dot * 0.5f), 9.0f);
		}
		PaintText(OutDrawElements, L + 5, StripLabel, SmallFont, O + FVector2f(BarLeft + Strip.Num() * Step + (bSeeds ? 24.0f : 6.0f), StripTop + 1.0f), Dim);

		// Small glowing badges left of the queue for the elements the surroundings give right now
		for (int32 Index = 0; Index < NearbyElements.Num(); ++Index)
		{
			const FVector2f At = O + FVector2f(BarLeft - 8.0f - (Index + 1) * 13.0f, StripTop + 3.0f);
			PaintBox(OutDrawElements, L + 4, &BadgeBrush, At, FVector2f(10.0f), NearbyElements[Index] * 1.4f);
		}

		for (int32 Index = 0; Index < QuickSlots.Num(); ++Index)
		{
			const FStripEntry& Entry = QuickSlots[Index];
			const FVector2f At = O + FVector2f(BarLeft + BarWidth - (QuickSlots.Num() - Index) * 22.0f, StripTop);
			PaintBox(OutDrawElements, L + 3, &SlotBrush, At, FVector2f(18.0f), Frame * 0.7f);
			if (Entry.bFilled)
			{
				PaintBox(OutDrawElements, L + 4, &BadgeBrush, At + FVector2f(4.0f), FVector2f(10.0f), Entry.bPicked ? Entry.Color : Entry.Color * 0.4f);
			}
		}
	}

	// Ability slots: picture, button badge, name below, cooldown as a dark wipe with the seconds
	for (int32 Index = 0; Index < NumSlots; ++Index)
	{
		const FSlotState& State = Slots[Index];
		const FVector2f At = O + FVector2f(BarLeft + Index * (SlotSize + SlotGap), SlotTop);
		// With a painted rim the drawn slot is only the dark ground inside it
		const bool bPaintedSlot = FindTextureBrush(ShownData->SlotPicture) != nullptr;
		const float Inset = bPaintedSlot ? 7.0f : 0.0f;
		PaintBox(OutDrawElements, L + 2, &SlotBrush, At + FVector2f(Inset), FVector2f(SlotSize - 2.0f * Inset), bPaintedSlot ? FLinearColor(0.25f, 0.22f, 0.2f) : State.bLit ? FLinearColor(1.4f, 1.3f, 1.15f) : FLinearColor::White);
		const FLinearColor IconTint = State.IconColor * (State.bDim ? 0.35f : 1.0f);
		PaintBox(OutDrawElements, L + 3, State.Icon, At + FVector2f(14.0f), FVector2f(SlotSize - 28.0f), IconTint);
		if (const FSlateBrush* Rim = FindTextureBrush(ShownData->SlotPicture))
		{
			PaintBox(OutDrawElements, L + 4, Rim, At - FVector2f(5.0f), FVector2f(SlotSize + 10.0f), State.bLit ? FLinearColor(1.5f, 1.4f, 1.25f) : FLinearColor::White);
		}

		if (State.CooldownLeft > 0.0f && CooldownBrushes.IsValidIndex(Index))
		{
			PaintBox(OutDrawElements, L + 5, &CooldownBrushes[Index], At + FVector2f(4.0f), FVector2f(SlotSize - 8.0f));
			if (!State.bNoSeconds)
			{
				PaintText(OutDrawElements, L + 6, FText::AsNumber(FMath::CeilToInt(State.CooldownLeft)), NumberFont, At + FVector2f(SlotSize * 0.5f, SlotSize * 0.5f - 12.0f), TextColor, 0.5f);
			}
		}

		PaintButton(OutDrawElements, L + 5, State.Button, State.KeyboardKey, At + FVector2f(SlotSize - 10.0f, SlotSize - 10.0f), 13.0f);
		PaintText(OutDrawElements, L + 5, State.Name, SmallFont, At + FVector2f(SlotSize * 0.5f, SlotSize + 1.0f), Dim, 0.5f, SlotSize - 4.0f);
	}

	// Potion and dodge as round buttons beside the vessels
	const FVector2f PotionAt = O + FVector2f(160.0f, 190.0f);
	PaintMedallion(OutDrawElements, L + 3, PotionAt, 18.0f, Frame);
	PaintBox(OutDrawElements, L + 5, FindTextureBrush(PotionIcon), PotionAt - FVector2f(11.0f), FVector2f(22.0f), Hex(0xD9, 0x57, 0x4A));
	PaintBox(OutDrawElements, L + 5, &BadgeBrush, PotionAt + FVector2f(6.0f, -22.0f), FVector2f(16.0f));
	PaintText(OutDrawElements, L + 6, PotionText, SmallFont, PotionAt + FVector2f(14.0f, -22.0f), GroundColor, 0.5f);

	const FVector2f DodgeAt = O + FVector2f(718.0f, 190.0f);
	PaintMedallion(OutDrawElements, L + 3, DodgeAt, 17.0f, Frame);
	PaintBox(OutDrawElements, L + 5, FindTextureBrush(DodgeIcon), DodgeAt - FVector2f(11.0f), FVector2f(22.0f), TextColor);
	if (DodgeLeft > 0.0f)
	{
		PaintBox(OutDrawElements, L + 6, &DodgeCooldownBrush, DodgeAt - FVector2f(15.0f), FVector2f(30.0f));
	}

	// Experience: the tail of the Urdrachin, from the end ornament of the class to a small dragon head
	PaintBox(OutDrawElements, L + 3, &TailBrush, O + FVector2f(TailLeft, TailTop), FVector2f(TailRight - TailLeft, TailHeight));
	PaintBox(OutDrawElements, L + 4, FindTextureBrush(ShownData->BarEnd), O + FVector2f(TailLeft - 28.0f, TailTop - 7.0f), FVector2f(28.0f), Frame);
	PaintBox(OutDrawElements, L + 4, FindTextureBrush(DragonHeadTexture), O + FVector2f(TailRight - 2.0f, TailTop - 9.0f), FVector2f(32.0f), Frame);

	return L + 8;
}

#undef LOCTEXT_NAMESPACE
