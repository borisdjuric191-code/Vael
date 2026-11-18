// Copyright Epic Games, Inc. All Rights Reserved.

#include "Items/VaelItemGroups.h"
#include "Items/VaelMaterial.h"

#define LOCTEXT_NAMESPACE "VaelItemGroups"

namespace
{
	/** Materials whose group their category and ingredient kind don't tell: by the name of their asset without DA_Material_ */
	const TMap<FName, EVaelItemGroup>& Named()
	{
		static const TMap<FName, EVaelItemGroup> Map = []
		{
			TMap<FName, EVaelItemGroup> Groups;
			for (const TCHAR* Name : { TEXT("Funkenerz"), TEXT("Kernerz"), TEXT("Rosteisen"), TEXT("Drahtsilber"), TEXT("Kobaltbluete"), TEXT("Gruenspan"),
				TEXT("Rippenspat"), TEXT("Siechglas"), TEXT("Salzspat"), TEXT("Schwefelkies"), TEXT("Blutzinnober"), TEXT("Glutstein"), TEXT("Schlacke"), TEXT("Russquarz") })
			{
				Groups.Add(Name, EVaelItemGroup::Ore);
			}
			for (const TCHAR* Name : { TEXT("Nidkristall"), TEXT("GelaeuterterKristall"), TEXT("Mondtropfen") })
			{
				Groups.Add(Name, EVaelItemGroup::Crystal);
			}
			for (const TCHAR* Name : { TEXT("Aeltestenschwinge"), TEXT("Blasenhaut"), TEXT("Bruchstueck"), TEXT("Brutkrone"), TEXT("Glimmauge"), TEXT("Glutdruese"),
				TEXT("Glutkiefer"), TEXT("Glutkrone"), TEXT("Greifklaue"), TEXT("Harpyienschnabel"), TEXT("HerzDerGlut"), TEXT("Hohlkiel"), TEXT("Klammerfinger"),
				TEXT("Koeniginnenauge"), TEXT("Kokonseide"), TEXT("Kreischbalg"), TEXT("Kriechersehne"), TEXT("Krustenplatte"), TEXT("Legestachel"), TEXT("Rollpanzer"),
				TEXT("Rollpanzerkern"), TEXT("Rollsehne"), TEXT("Rotnoppe"), TEXT("Russfeder"), TEXT("Schleierpelz"), TEXT("Sichelherz"), TEXT("Sichelklinge"),
				TEXT("Spannfaden"), TEXT("Puppengarn"), TEXT("Glimmstroh"), TEXT("Blutschuppen") })
			{
				Groups.Add(Name, EVaelItemGroup::Creature);
			}
			for (const TCHAR* Name : { TEXT("GehaertetePlatte"), TEXT("Schlackeneisen"), TEXT("Messingblech"), TEXT("Russglas"), TEXT("Schlackenglas"), TEXT("Latex"),
				TEXT("SchwarzerLatex"), TEXT("Siegelwachs"), TEXT("Ordenstuch"), TEXT("Ordenssiegel"), TEXT("Harpyientand") })
			{
				Groups.Add(Name, EVaelItemGroup::Workstuff);
			}
			for (const TCHAR* Name : { TEXT("Blatttau"), TEXT("Faultau"), TEXT("Kelchtau"), TEXT("Pelzwasser"), TEXT("Blutnektar"), TEXT("RosenherzNektar"), TEXT("Gallmilch"),
				TEXT("Kelchschleim"), TEXT("Narbensaft"), TEXT("Sternmilch"), TEXT("Verdauungssaeure"), TEXT("Lichttropfen") })
			{
				Groups.Add(Name, EVaelItemGroup::Liquid);
			}
			for (const TCHAR* Name : { TEXT("Dornpollen"), TEXT("Trichterpollen"), TEXT("Gallpollen"), TEXT("Sternpollen"), TEXT("Traenenglitzer"), TEXT("Glimmstaub"),
				TEXT("Schwelstaub"), TEXT("Rauchblase"), TEXT("Glockenglut") })
			{
				Groups.Add(Name, EVaelItemGroup::Dust);
			}
			for (const TCHAR* Name : { TEXT("Myzelprobe"), TEXT("Pfeifensporen"), TEXT("Staeublingspulver") })
			{
				Groups.Add(Name, EVaelItemGroup::Fungus);
			}
			for (const TCHAR* Name : { TEXT("Heiltrank"), TEXT("Klartrunk"), TEXT("Nidtrunk") })
			{
				Groups.Add(Name, EVaelItemGroup::Potion);
			}
			Groups.Add(TEXT("Ueberraschungssaat"), EVaelItemGroup::Seed);
			return Groups;
		}();
		return Map;
	}
}

EVaelItemGroup VaelItemGroups::GetGroup(const UVaelMaterial* Material)
{
	if (Material == nullptr)
	{
		return EVaelItemGroup::Plant;
	}
	FString Name = Material->GetName();
	Name.RemoveFromStart(TEXT("DA_Material_"));
	if (const EVaelItemGroup* Known = Named().Find(FName(*Name)))
	{
		return *Known;
	}
	if (Name.StartsWith(TEXT("Trank_")) || Material->Category == EVaelMaterialCategory::Potion)
	{
		return EVaelItemGroup::Potion;
	}
	if (Name.StartsWith(TEXT("Samen_")) || Material->Category == EVaelMaterialCategory::Seed)
	{
		return EVaelItemGroup::Seed;
	}
	if (Material->Category == EVaelMaterialCategory::Crystal)
	{
		return EVaelItemGroup::Crystal;
	}
	switch (Material->Kind)
	{
	case EVaelIngredientKind::Sap:
	case EVaelIngredientKind::Dew:
	case EVaelIngredientKind::Nectar:
		return EVaelItemGroup::Liquid;
	case EVaelIngredientKind::Pollen:
	case EVaelIngredientKind::Dust:
	case EVaelIngredientKind::Glitter:
	case EVaelIngredientKind::Ember:
		return EVaelItemGroup::Dust;
	case EVaelIngredientKind::Catalyst:
		return EVaelItemGroup::Fungus;
	case EVaelIngredientKind::Part:
		return EVaelItemGroup::Creature;
	default:
		return EVaelItemGroup::Plant;
	}
}

FLinearColor VaelItemGroups::GetColor(EVaelItemGroup Group)
{
	// distinct hues at a like brightness, so none outshines the picture in front of it
	static const FColor Colors[] =
	{
		FColor(112, 136, 162),	// Ore: steel blue
		FColor(156, 104, 226),	// Crystal: violet
		FColor(176, 44, 56),	// Creature: blood red
		FColor(204, 162, 86),	// Workstuff: brass
		FColor(78, 164, 74),	// Plant: green
		FColor(56, 156, 196),	// Liquid: water blue
		FColor(232, 196, 76),	// Dust: pollen yellow
		FColor(150, 104, 66),	// Fungus: earth brown
		FColor(168, 214, 86),	// Seed: young green
		FColor(214, 76, 162),	// Potion: magenta
		FColor(232, 105, 44),	// Armor: ember
	};
	return FLinearColor(Colors[FMath::Clamp(static_cast<int32>(Group), 0, UE_ARRAY_COUNT(Colors) - 1)]);
}

FText VaelItemGroups::GetName(EVaelItemGroup Group)
{
	switch (Group)
	{
	case EVaelItemGroup::Ore: return LOCTEXT("Ore", "Erz und Gestein");
	case EVaelItemGroup::Crystal: return LOCTEXT("Crystal", "Kristall");
	case EVaelItemGroup::Creature: return LOCTEXT("Creature", "Kreaturteil");
	case EVaelItemGroup::Workstuff: return LOCTEXT("Workstuff", "Werkstoff");
	case EVaelItemGroup::Plant: return LOCTEXT("Plant", "Pflanze");
	case EVaelItemGroup::Liquid: return LOCTEXT("Liquid", "Saft und Tau");
	case EVaelItemGroup::Dust: return LOCTEXT("Dust", "Pollen und Staub");
	case EVaelItemGroup::Fungus: return LOCTEXT("Fungus", "Pilz");
	case EVaelItemGroup::Seed: return LOCTEXT("Seed", "Saat");
	case EVaelItemGroup::Potion: return LOCTEXT("Potion", "Trank");
	default: return LOCTEXT("Armor", "Rüstung");
	}
}

#undef LOCTEXT_NAMESPACE
