// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/CheatManager.h"
#include "VaelCheatManager.generated.h"

/**
 *  Console commands for testing. Not available in shipping builds.
 */
UCLASS()
class UVaelCheatManager : public UCheatManager
{
	GENERATED_BODY()

public:

	/**
	 *  Queues elements and casts them, aiming along the given world yaw.
	 *  Elements are the letters of the prototype: F fire, W water, E earth, L air, M mark. Example: VaelCast FE 90
	 */
	UFUNCTION(Exec)
	void VaelCast(const FString& Elements, float AimYaw);

	/** Queues elements without casting them, letters as in VaelCast; the orbs stay above the head. Example: VaelQueue W */
	UFUNCTION(Exec)
	void VaelQueue(const FString& Elements);

	/** Puts the player on the ground at a place of the level, in meters. Example: VaelTeleport 31 6 (shallow water of the pond in Aschenmark) */
	UFUNCTION(Exec)
	void VaelTeleport(float X, float Y);

	/** Opens or closes the big map of the first player, like its button */
	UFUNCTION(Exec)
	void VaelKarte();

	/** Adds every formula of the game to the grimoire, sealed ones included */
	UFUNCTION(Exec)
	void VaelLearnAll();

	/** Seed garden: gives the player five surprise seeds; with "alles" every plant is researched and its seeds are handed out. Example: VaelSaat alles */
	UFUNCTION(Exec)
	void VaelSaat(const FString& What);

	/** Switches the base look of the player: "Mann" or "Frau". Example: VaelGestalt Frau */
	UFUNCTION(Exec)
	void VaelGestalt(const FString& Look);

	/** Turns the player into a mage or a Floromant: "Magier" or "Floromant", then "Mann" or "Frau". Example: VaelKlasse Floromant Frau */
	UFUNCTION(Exec)
	void VaelKlasse(const FString& Class, const FString& Look);

	/** Floromant: rises high or sinks back, like R3 */
	UFUNCTION(Exec)
	void VaelSteigen();

	/** Floromant: the plant of the "links" or "rechts" hand goes into the ground as a tower, like L2 / R2. Example: VaelTurm links */
	UFUNCTION(Exec)
	void VaelTurm(const FString& Hand);

	/** Floromant: the Kreuzschuss, like Cross: the hands cross and charge, and let go after the seconds (0 = the short charge).
	 *  Example: VaelKreuzen 1.5 */
	UFUNCTION(Exec)
	void VaelKreuzen(float Seconds = 0.0f);

	/** Floromant: takes root or lets go, like Square */
	UFUNCTION(Exec)
	void VaelWurzeln();

	/** Floromant: the plants of both hands change sides, like Triangle tapped */
	UFUNCTION(Exec)
	void VaelTauschen();

	/** Floromant: the Turmruf, like L2 + R2 */
	UFUNCTION(Exec)
	void VaelTurmruf();

	/** Floromant: sows the seed of the belt whose name starts like the word, or the picked one, the given metres ahead. Example: VaelSamen Russ 4 */
	UFUNCTION(Exec)
	void VaelSamen(const FString& Seed, float Metres);

	/** Floromant: lays the picked plant of the seed belt of the hands, or the one whose name starts like the word, on "links", "rechts" or "beide" hands. Example: VaelHand beide Glut */
	UFUNCTION(Exec)
	void VaelHand(const FString& Hands, const FString& Plant = TEXT(""));

	/** Floromant: reloads the hands from the Keimpelz pelt; with a number sets the pelt to that share in percent first. Example: VaelNachladen 50 */
	UFUNCTION(Exec)
	void VaelNachladen(float Percent = -1.0f);

	/** Lights a fire the given metres from the figure towards the camera, with the radius in cm; "wand" lights it on the nearest wall ahead instead. It burns 25 s; called again on the same spot it feeds that fire, which grows as a Brandherd. "live" simulates it while the game runs (test). Example: VaelBrand 50 3 */
	UFUNCTION(Exec)
	void VaelBrand(float Radius = 45.0f, float Metres = 3.0f, const FString& Where = TEXT(""));

	/** Floromant: holds the fire button of "links", "rechts" or "beide" hands, or lets go with "aus". Example: VaelFeuern beide */
	UFUNCTION(Exec)
	void VaelFeuern(const FString& Hands);

	/** Floromant: grows any plant by its id at once, the given metres ahead; "schatten" for its shadow form. Example: VaelPflanze Funkenmohn 3 schatten */
	UFUNCTION(Exec)
	void VaelPflanze(const FString& PlantId, float Metres, const FString& Form = TEXT(""));

	/** Floromant: uses an ability: "Ruf", "Griff", "Sprung", "Wuchern" or "Peitsche", aimed the given metres ahead. Example: VaelFaehigkeit Ruf */
	UFUNCTION(Exec)
	void VaelFaehigkeit(const FString& Ability, float Metres = 4.0f);

	/** Lets another local player join as if a controller had pressed Start, to test co-op without a second controller; optionally as "Magier" or "Floromant", "Mann" or "Frau". Example: VaelMitspieler Magier Mann */
	/** Shows the panel of the player in the look of another class: "Magier", "Tamer", "Ingenieur", "Prophet", "Floromant"; nothing for the own class. Example: VaelHudKlasse Prophet */
	UFUNCTION(Exec)
	void VaelHudKlasse(const FString& ClassName = TEXT(""));

	/** Sets the sworn way of the player: "Licht", "Schatten", "Zwielicht" or "Keiner" (before the oath). Example: VaelWeg Schatten */
	UFUNCTION(Exec)
	void VaelWeg(const FString& Way);

	/** Gives the player experience. Example: VaelErfahrung 80 */
	UFUNCTION(Exec)
	void VaelErfahrung(float Amount);

	/** Opens a page of the menu: "Formeln", "Inventar", "Kompendium", "Auftraege" or "Gestalt". Example: VaelMenue Inventar */
	UFUNCTION(Exec)
	void VaelMenue(const FString& Page);

	/** Lets another local player join as if a controller had pressed Start, to test co-op without a second controller; optionally as "Magier" or "Floromant", "Mann" or "Frau". Example: VaelMitspieler Magier Mann */
	UFUNCTION(Exec)
	void VaelMitspieler(const FString& Class = TEXT(""), const FString& Look = TEXT(""));

	/** Puts a condition on everything the player can hurt: Wet, Burning or Frozen, for the given seconds. Example: VaelStatus Wet 5 */
	UFUNCTION(Exec)
	void VaelStatus(const FString& Status, float Duration);

	/**
	 *  Spawns creatures 8 m in front of the player: Glutkriecher, Aschharpyie, Aelteste, Prediger, Markprediger (test of the water kit), Koenigin, Waechter or Spannhornkaefer.
	 *  The count is optional (1). Example: VaelSpawn Pfuhlnatter or VaelSpawn Koenigin 2
	 */
	UFUNCTION(Exec)
	void VaelSpawn(const FString& Kind, int32 Count = 1);

	/** Every grown Pfuhlnatter of the moor lays its brood now; the count is logged */
	UFUNCTION(Exec)
	void VaelNatterBrut();

	/** Makes every Distelspinne swell and burst into its swarm right now, to see the burst */
	UFUNCTION(Exec)
	void VaelDistelPlatzen();

	/** Makes the encounter director of the level send a pack ahead of the group right now */
	UFUNCTION(Exec)
	void VaelBegegnung();

	/**
	 *  Lays a scroll in front of the player, 3 m unless a distance in cm is given, that teaches the formula of the elements, letters as in VaelCast.
	 *  Example: VaelScroll EEE
	 */
	UFUNCTION(Exec)
	void VaelScroll(const FString& Elements, float Distance = 300.0f);

	/** Kills every creature in the level */
	UFUNCTION(Exec)
	void VaelKillAll();

	/** Studies the samples of the group as if at the camp: every entry with enough samples becomes researched */
	UFUNCTION(Exec)
	void VaelStudy();

	/** Throws the save game away and starts the level from the beginning */
	UFUNCTION(Exec)
	void VaelNeuesSpiel();

	/** Writes the save game now */
	UFUNCTION(Exec)
	void VaelSpeichern();

	/** Sets a compendium entry to a stage: 0 unknown to 4 researched; "Alle" for every entry. Example: VaelResearch Glutkriecher 4 */
	UFUNCTION(Exec)
	void VaelResearch(const FString& Name, int32 Stage);

	/** Chooses the controller symbols of the HUD: Auto, Xbox or PlayStation. Example: VaelGlyphs PlayStation */
	UFUNCTION(Exec)
	void VaelGlyphs(const FString& Glyphs);

	/** Changes the weather of the region the player stands in: Klar, Regen, Sturm, Duerre or Wind. Example: VaelWeather Sturm */
	UFUNCTION(Exec)
	void VaelWeather(const FString& Weather);

	/** Adds heat, moisture or cold to the air where the player stands (Hitze, Feuchte, Kaelte) and shows what the air holds there. Example: VaelAir Feuchte 100 */
	UFUNCTION(Exec)
	void VaelAir(const FString& Kind, float Amount = 50.0f);

	/** Lets a local weather break out where the player stands: Schauer, Gewitter, Aufwind, Windfront, Nebel or Graupel. Example: VaelLocalWeather Gewitter */
	UFUNCTION(Exec)
	void VaelLocalWeather(const FString& Kind);

	/** Sets the clock to an hour (dawn 5–7, day 7–19, dusk 19–21, night 21–5). Example: VaelTime 22 */
	UFUNCTION(Exec)
	void VaelTime(float Hour);

	/** Runs the clock this many times as fast, 1 for normal. Example: VaelTimeSpeed 30 */
	UFUNCTION(Exec)
	void VaelTimeSpeed(float Speed);

	/** Sets the corruption of the region the player stands in, 0 to 100. Creatures spawned afterwards may be marked. Example: VaelCorruption 80 */
	UFUNCTION(Exec)
	void VaelCorruption(float Corruption);

	/** Sets the personal corruption of the player, 0 to 100. Example: VaelPlayerCorruption 30 */
	UFUNCTION(Exec)
	void VaelPlayerCorruption(float Corruption);

	/** Puts on or takes off a gear effect for testing until items exist: WeatherWard or WeatherAttunement. Example: VaelGear WeatherWard */
	UFUNCTION(Exec)
	void VaelGear(const FString& Gear);

	/** Moves a quest to a step, counted from 1; a step past the last finishes it. Without arguments lists every quest. Example: VaelQuest Q2_DieBrut 3 */
	UFUNCTION(Exec)
	void VaelQuest(const FString& QuestId = TEXT(""), int32 Step = 0);

	/** Awakens the Mark for the group as in Act III: the fifth element can be chosen and Mark formulas cast */
	UFUNCTION(Exec)
	void VaelAwakenMark();

	/** Seals the Mark source closest to the player, until the offering and its guardian exist */
	UFUNCTION(Exec)
	void VaelSealSource();

	/** Puts materials into the bag of the player, named like their asset without DA_Material_. Example: VaelMaterial Markkristall 5 */
	UFUNCTION(Exec)
	void VaelMaterial(const FString& Name, int32 Count = 1);

	/** Drops an item for the player at their feet: a rarity (Gewoehnlich, Magisch, Selten) rolls one, otherwise the item asset without DA_Item_. Example: VaelItem Sturmmantel */
	UFUNCTION(Exec)
	void VaelItem(const FString& Name);

	/** Shows the equipment, backpack and the totals of the worn properties on screen, until the inventory screen exists */
	UFUNCTION(Exec)
	void VaelInventory();

	/** Puts on the item from the given field of the backpack, counted from 1. Example: VaelEquip 1 */
	UFUNCTION(Exec)
	void VaelEquip(int32 BackpackField);
};
