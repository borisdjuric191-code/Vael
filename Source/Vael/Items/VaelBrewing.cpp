// Copyright Epic Games, Inc. All Rights Reserved.

#include "Items/VaelBrewing.h"

#include "Floromant/VaelFloromantSettings.h"
#include "Items/VaelMaterialBag.h"
#include "Player/VaelCharacter.h"
#include "Save/VaelSaveSubsystem.h"
#include "UI/VaelCombatTextSubsystem.h"
#include "Vael.h"

#define LOCTEXT_NAMESPACE "VaelBrewing"

namespace
{
	TSoftObjectPtr<UVaelMaterial> Material(const TCHAR* Name)
	{
		return TSoftObjectPtr<UVaelMaterial>(FSoftObjectPath(FString::Printf(TEXT("/Game/Vael/Items/Materials/DA_Material_%s.DA_Material_%s"), Name, Name)));
	}

	FLinearColor Hex(uint32 Rgb)
	{
		return FLinearColor(FColor((Rgb >> 16) & 0xFF, (Rgb >> 8) & 0xFF, Rgb & 0xFF));
	}

	/** The recipes of the Aschenmark as the Rezeptbuch has them (accepted by Boris, 14 October 2026). Ingredient asset names:
	 *  Aschenstäubling = Staeublingspulver, Adernflechte = Myzelprobe, Rauchstaub = Rauchblase, Keimpelzflocken = Russmoos,
	 *  Pfeifenmorchel-Sporen = Pfeifensporen. Results are DA_Material_Trank_<Id> (Scripts/CreatePotionAssets.py), the
	 *  Heiltrank is the one of the guild. */
	TArray<FVaelRecipe> MakeRecipes()
	{
		TArray<FVaelRecipe> Recipes;
		const auto Add = [&Recipes](const TCHAR* Id, const FText& Name, EVaelBrewTier Tier, EVaelWay Way, EVaelBrewCraft Craft,
			std::initializer_list<const TCHAR*> Ingredients, const FText& Effect, const FText& Duration, uint32 Color, const TCHAR* Result = nullptr)
		{
			FVaelRecipe& Recipe = Recipes.AddDefaulted_GetRef();
			Recipe.Id = Id;
			Recipe.Name = Name;
			Recipe.Tier = Tier;
			Recipe.Way = Way;
			Recipe.Craft = Craft;
			for (const TCHAR* Ingredient : Ingredients)
			{
				Recipe.Ingredients.Add(Material(Ingredient));
			}
			Recipe.Result = Material(Result != nullptr ? Result : *FString::Printf(TEXT("Trank_%s"), Id));
			Recipe.Effect = Effect;
			Recipe.Duration = Duration;
			Recipe.Color = Hex(Color);
		};

		using T = EVaelBrewTier;
		using W = EVaelWay;
		using C = EVaelBrewCraft;

		// Grundtränke (3)
		Add(TEXT("Manatrank"), LOCTEXT("Manatrank", "Manatrank"), T::Basic, W::Neutral, C::Elements, { TEXT("Pelzwasser"), TEXT("Glockenglut"), TEXT("Glutsamen") },
			LOCTEXT("ManatrankEffect", "Füllt 40 % Mana, danach schnellere Erholung."), LOCTEXT("ManatrankTime", "sofort, Erholung 5 Min."), 0x4a8fe0);
		Add(TEXT("Nachtsicht"), LOCTEXT("Nachtsicht", "Nachtsicht"), T::Basic, W::Neutral, C::BodyAndSoul, { TEXT("Pelzwasser"), TEXT("Glockenglut"), TEXT("Russquarz") },
			LOCTEXT("NachtsichtEffect", "Sieht im Dunkeln, zeigt versteckte Funde."), LOCTEXT("Min15", "15 Min."), 0x9fd0a0);
		Add(TEXT("Loeschtrank"), LOCTEXT("Loeschtrank", "Löschtrank"), T::Basic, W::Neutral, C::Nature, { TEXT("Pelzwasser"), TEXT("Staeublingspulver"), TEXT("Russmoos") },
			LOCTEXT("LoeschtrankEffect", "Löscht Brennen, danach weniger Feuerschaden."), LOCTEXT("LoeschtrankTime", "sofort, Schutz 5 Min."), 0x8fb8c8);
		Add(TEXT("Ascheschleier"), LOCTEXT("Ascheschleier", "Ascheschleier"), T::Basic, W::Neutral, C::Elements, { TEXT("Pelzwasser"), TEXT("Rauchblase"), TEXT("Pfeifensporen") },
			LOCTEXT("AscheschleierEffect", "Gegner bemerken dich später; 2 Ladungen: 10 Sekunden unsichtbar."), LOCTEXT("Min15", "15 Min."), 0x8a8580);
		Add(TEXT("Heiltrank"), LOCTEXT("Heiltrank", "Heiltrank"), T::Basic, W::Light, C::BodyAndSoul, { TEXT("Blatttau"), TEXT("Russbluetenblatt"), TEXT("Glimmstaub") },
			LOCTEXT("HeiltrankEffect", "Heilt 40 %, danach Regeneration."), LOCTEXT("HeiltrankTime", "sofort, Regeneration 5 Min."), 0xd0443a, TEXT("Heiltrank"));
		Add(TEXT("Leichtfuss"), LOCTEXT("Leichtfuss", "Leichtfuß"), T::Basic, W::Light, C::Elements, { TEXT("Blatttau"), TEXT("Halmsamen"), TEXT("Silberhalm") },
			LOCTEXT("LeichtfussEffect", "20 % schneller."), LOCTEXT("Min15", "15 Min."), 0xbfe6ff);
		Add(TEXT("Rindenhaut"), LOCTEXT("Rindenhaut", "Rindenhaut"), T::Basic, W::Light, C::Nature, { TEXT("Sternmilch"), TEXT("Trichterpollen"), TEXT("Rankenfaser") },
			LOCTEXT("RindenhautEffect", "+20 % Rüstung."), LOCTEXT("Min15", "15 Min."), 0x9c7a4a);
		Add(TEXT("Feuerschutz"), LOCTEXT("Feuerschutz", "Feuerschutz"), T::Basic, W::Light, C::Elements, { TEXT("Sternmilch"), TEXT("Mohnbluetenblatt"), TEXT("Glimmblatt") },
			LOCTEXT("FeuerschutzEffect", "30 % weniger Feuerschaden."), LOCTEXT("Min15", "15 Min."), 0xffb04a);
		Add(TEXT("Giftphiole"), LOCTEXT("Giftphiole", "Giftphiole"), T::Basic, W::Shadow, C::ThrowAndOil, { TEXT("Faultau"), TEXT("Dornpollen"), TEXT("Kelchschleim") },
			LOCTEXT("GiftphioleEffect", "Giftwolke, Gegner bluten."), LOCTEXT("Sofort", "sofort"), 0x6fae3a);
		Add(TEXT("Ermattungsfluch"), LOCTEXT("Ermattungsfluch", "Ermattungsfluch"), T::Basic, W::Shadow, C::ThrowAndOil, { TEXT("Gallmilch"), TEXT("Schwelblatt"), TEXT("Raunsamen") },
			LOCTEXT("ErmattungsfluchEffect", "Gegner langsamer und verwirrt."), LOCTEXT("ErmattungsfluchTime", "sofort, Fluch 1 Min."), 0x6a4a8a);
		Add(TEXT("Blutoel"), LOCTEXT("Blutoel", "Blutöl"), T::Basic, W::Shadow, C::ThrowAndOil, { TEXT("Gallmilch"), TEXT("Blutblatt"), TEXT("Blutnektar") },
			LOCTEXT("BlutoelEffect", "Waffe raubt 10 % Leben."), LOCTEXT("Min15", "15 Min."), 0x8f1620);
		Add(TEXT("Schwarzbrand"), LOCTEXT("Schwarzbrand", "Schwarzbrand"), T::Basic, W::Shadow, C::ThrowAndOil, { TEXT("Gallmilch"), TEXT("Schwarzmohnblatt"), TEXT("Schwarzfunken") },
			LOCTEXT("SchwarzbrandEffect", "Feuer, das sich schwer löschen lässt."), LOCTEXT("Sofort", "sofort"), 0x3a1a2e);

		// Stärkere Tränke (4, with a catalyst)
		Add(TEXT("Gruppenheilung"), LOCTEXT("Gruppenheilung", "Gruppenheilung"), T::Strong, W::Light, C::ThrowAndOil, { TEXT("Kelchtau"), TEXT("Traenenglitzer"), TEXT("Russbluetenblatt"), TEXT("Staeublingspulver") },
			LOCTEXT("GruppenheilungEffect", "Heilwolke für alle Verbündeten darin."), LOCTEXT("Sofort", "sofort"), 0xff8f8f);
		Add(TEXT("Saeurewolke"), LOCTEXT("Saeurewolke", "Säurewolke"), T::Strong, W::Shadow, C::ThrowAndOil, { TEXT("Faultau"), TEXT("Verdauungssaeure"), TEXT("Kelchschleim"), TEXT("Staeublingspulver") },
			LOCTEXT("SaeurewolkeEffect", "Zerfrisst Rüstung in einer Fläche."), LOCTEXT("Sofort", "sofort"), 0xb8d23a);
		Add(TEXT("Reinigungstrank"), LOCTEXT("Reinigungstrank", "Reinigungstrank"), T::Strong, W::Light, C::BodyAndSoul, { TEXT("Kelchtau"), TEXT("Hainblatt"), TEXT("Lichttropfen"), TEXT("Myzelprobe") },
			LOCTEXT("ReinigungstrankEffect", "Senkt die eigene Verderbnis um 10 %."), LOCTEXT("Sofort", "sofort"), 0xf2f0d0);
		Add(TEXT("Daemmertrank"), LOCTEXT("Daemmertrank", "Dämmertrank"), T::Strong, W::Twilight, C::BodyAndSoul, { TEXT("Pelzwasser"), TEXT("Russbluetenblatt"), TEXT("Blutblatt"), TEXT("Myzelprobe") },
			LOCTEXT("DaemmertrankEffect", "Heilt bei Treffern ein wenig und wirft Schaden auf Angreifer zurück."), LOCTEXT("Min15", "15 Min."), 0xb06a8a);

		// Krafttränke (4 to 5)
		Add(TEXT("Hainruf"), LOCTEXT("Hainruf", "Hainruf"), T::Power, W::Light, C::Nature, { TEXT("Sternmilch"), TEXT("Rankenfaser"), TEXT("Trichterpollen"), TEXT("RosenherzNektar"), TEXT("Pfeifensporen") },
			LOCTEXT("HainrufEffect", "Pflanzen gehorchen dir: alle 20 Sekunden umschlingt eine Ranke einen Gegner, Schlafpollen betäuben."), LOCTEXT("Min20", "20 Min."), 0x6fcf5a);
		Add(TEXT("Wuergherrschaft"), LOCTEXT("Wuergherrschaft", "Würgherrschaft"), T::Power, W::Shadow, C::Nature, { TEXT("Gallmilch"), TEXT("Wuergranke"), TEXT("Dornpollen"), TEXT("Blutnektar"), TEXT("Pfeifensporen") },
			LOCTEXT("WuergherrschaftEffect", "Pflanzen kriechen auf Gegner zu, umschlingen und vergiften sie; 3 Ladungen: einen Gegner unter die Erde ziehen."), LOCTEXT("Min20", "20 Min."), 0x4a2a3a);
		Add(TEXT("Feenleib"), LOCTEXT("Feenleib", "Feenleib"), T::Power, W::Light, C::Nature, { TEXT("Blatttau"), TEXT("Sternpollen"), TEXT("Glimmstaub"), TEXT("RosenherzNektar"), TEXT("Pfeifensporen") },
			LOCTEXT("FeenleibEffect", "Du schwebst über Schlamm, alle 5 Sekunden trifft ein Funke der Hain-Geister einen nahen Gegner."), LOCTEXT("Min30", "30 Min."), 0xe8f5a0);
		Add(TEXT("Glutblut"), LOCTEXT("Glutblut", "Glutblut"), T::Power, W::Neutral, C::Elements, { TEXT("Pelzwasser"), TEXT("Glutsamen"), TEXT("Glockenglut"), TEXT("Glutstein") },
			LOCTEXT("GlutblutEffect", "Halber Feuerschaden, du hinterlässt eine Glutspur, Feuerquellen füllen dein Mana."), LOCTEXT("Min30", "30 Min."), 0xff5a1a);
		Add(TEXT("Aschegestalt"), LOCTEXT("Aschegestalt", "Aschegestalt"), T::Power, W::Neutral, C::Elements, { TEXT("Pelzwasser"), TEXT("Rauchblase"), TEXT("Distelflaum"), TEXT("Pfeifensporen") },
			LOCTEXT("AschegestaltEffect", "15 % ausweichen; 3 Ladungen: als Rauch durch Gegner gleiten, 3 Sekunden unverwundbar."), LOCTEXT("Min20", "20 Min."), 0x6e6a66);
		Add(TEXT("Sternenblick"), LOCTEXT("Sternenblick", "Sternenblick"), T::Power, W::Light, C::BodyAndSoul, { TEXT("Sternmilch"), TEXT("Sternpollen"), TEXT("Glockenglut"), TEXT("Russquarz") },
			LOCTEXT("SternenblickEffect", "Du siehst verborgene Wege, Schätze und die Fundorte versiegelter Formeln."), LOCTEXT("Min30", "30 Min."), 0xc8c0ff);
		Add(TEXT("Blutrausch"), LOCTEXT("Blutrausch", "Blutrausch"), T::Power, W::Shadow, C::BodyAndSoul, { TEXT("Gallmilch"), TEXT("Blutblatt"), TEXT("Blutnektar"), TEXT("Schwelblatt"), TEXT("Myzelprobe") },
			LOCTEXT("BlutrauschEffect", "Jeder Treffer heilt ein wenig und macht schneller, bis zu 5-fach; im Kampf verlierst du langsam Leben."), LOCTEXT("Min15", "15 Min."), 0xa01a2a);
		Add(TEXT("Daemmergarten"), LOCTEXT("Daemmergarten", "Dämmergarten"), T::Power, W::Twilight, C::Nature, { TEXT("Pelzwasser"), TEXT("Wuergranke"), TEXT("Russbluetenblatt"), TEXT("Blutblatt"), TEXT("Myzelprobe") },
			LOCTEXT("DaemmergartenEffect", "Die Pflanzen wechseln alle 10 Sekunden zwischen Heilen und Würgen."), LOCTEXT("Min20", "20 Min."), 0x8a5a7a);
		return Recipes;
	}
}

const TArray<FVaelRecipe>& FVaelBrewing::GetRecipes()
{
	static const TArray<FVaelRecipe> Recipes = MakeRecipes();
	return Recipes;
}

TArray<const FVaelRecipe*> FVaelBrewing::GetRecipes(EVaelBrewTier Tier)
{
	TArray<const FVaelRecipe*> Found;
	for (const FVaelRecipe& Recipe : GetRecipes())
	{
		if (Recipe.Tier == Tier)
		{
			Found.Add(&Recipe);
		}
	}
	return Found;
}

EVaelBrewBlock FVaelBrewing::GetBlock(const AVaelCharacter* Player, const FVaelRecipe& Recipe)
{
	const UVaelMaterialBag* Bag = Player != nullptr ? Player->GetMaterialBag() : nullptr;
	if (Bag == nullptr)
	{
		return EVaelBrewBlock::Missing;
	}

	// Shadow pages open with a seal of the order (or once the player walks the shadow path); twilight only after the vow
	const EVaelPath Path = Player->GetPath();
	if (Recipe.Way == EVaelWay::Shadow && Path != EVaelPath::Shadow && Path != EVaelPath::Twilight
		&& Bag->GetCount(Material(TEXT("Ordenssiegel")).LoadSynchronous()) <= 0)
	{
		return EVaelBrewBlock::SealedShadow;
	}
	if (Recipe.Way == EVaelWay::Twilight && Path != EVaelPath::Twilight)
	{
		return EVaelBrewBlock::SealedTwilight;
	}

	for (const TSoftObjectPtr<UVaelMaterial>& Ingredient : Recipe.Ingredients)
	{
		const UVaelMaterial* Loaded = Ingredient.LoadSynchronous();
		if (Loaded == nullptr || Bag->GetCount(Loaded) <= 0)
		{
			return EVaelBrewBlock::Missing;
		}
	}
	return Recipe.Result.LoadSynchronous() != nullptr ? EVaelBrewBlock::None : EVaelBrewBlock::Missing;
}

bool FVaelBrewing::Brew(AVaelCharacter* Player, const FVaelRecipe& Recipe)
{
	if (GetBlock(Player, Recipe) != EVaelBrewBlock::None)
	{
		return false;
	}

	UVaelMaterialBag* Bag = Player->GetMaterialBag();
	for (const TSoftObjectPtr<UVaelMaterial>& Ingredient : Recipe.Ingredients)
	{
		Bag->RemoveMaterial(Ingredient.LoadSynchronous(), 1);
	}
	Bag->AddMaterial(Recipe.Result.LoadSynchronous(), 1);

	UVaelCombatTextSubsystem::PostPickup(Player, FText::Format(LOCTEXT("Brewed", "{0} gebraut"), Recipe.Name), Recipe.Color);
	UVaelSaveSubsystem::RequestSave(Player);
	UE_LOG(LogVael, Log, TEXT("Brewing: %s brewed %s"), *Player->GetName(), *Recipe.Id.ToString());
	return true;
}

bool FVaelBrewing::IsOwnCraft(const AVaelCharacter* Player, EVaelBrewCraft Craft)
{
	if (Player == nullptr)
	{
		return false;
	}
	// The mage has the elements; the Floromant, who lives by her plants, takes the craft of nature (assumption until the
	// classes of the Rezeptbuch, Tamer and Prophet, are in the game)
	if (Player->IsEngineer())
	{
		return Craft == EVaelBrewCraft::ThrowAndOil;
	}
	return Player->IsFloromant() ? Craft == EVaelBrewCraft::Nature : Craft == EVaelBrewCraft::Elements;
}

FText FVaelBrewing::GetTierName(EVaelBrewTier Tier)
{
	switch (Tier)
	{
	case EVaelBrewTier::Basic: return LOCTEXT("TierBasic", "Grundtränke");
	case EVaelBrewTier::Strong: return LOCTEXT("TierStrong", "Stärkere Tränke");
	default: return LOCTEXT("TierPower", "Krafttränke");
	}
}

FText FVaelBrewing::GetCraftName(EVaelBrewCraft Craft)
{
	switch (Craft)
	{
	case EVaelBrewCraft::Nature: return LOCTEXT("CraftNature", "Natur");
	case EVaelBrewCraft::Elements: return LOCTEXT("CraftElements", "Elemente");
	case EVaelBrewCraft::ThrowAndOil: return LOCTEXT("CraftThrow", "Wurf und Öl");
	default: return LOCTEXT("CraftBody", "Leib und Seele");
	}
}

#undef LOCTEXT_NAMESPACE
