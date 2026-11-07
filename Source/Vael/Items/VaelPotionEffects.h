// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"
#include "VaelPotionEffects.generated.h"

class AVaelCharacter;
class AVaelCharacterBase;
class UPointLightComponent;
class UVaelMaterial;

/**
 *  What the potions of the witch's kitchen do once a player drinks or throws them (Lore-Bibel, Rezeptbuch, Tränke der
 *  Aschenmark). Added to a player the first time a potion is used. Holds the effects that last (15 to 30 minutes), the
 *  timed pulses of the power potions (a tendril every 20 s, a spark every 5 s) and the poison clouds of thrown vials.
 *  The class of the potion's craft gets it 25 % stronger and 50 % longer (FVaelBrewing::IsOwnCraft).
 *  Hooks: AVaelCharacter asks it for incoming damage and walking speed, the attribute set for life stolen and damage thrown
 *  back, creatures for how far they notice the player.
 */
UCLASS()
class UVaelPotionEffects : public UActorComponent
{
	GENERATED_BODY()

public:

	UVaelPotionEffects();

	/** The effects of a player, null if they never used a potion */
	static UVaelPotionEffects* Find(const AActor* Player);

	/** Uses a potion of the Rezeptbuch from the bag: drinks it, or throws a vial at the nearest enemy. Returns false if it is
	 *  no such potion or a vial has no target; the potion is then kept. */
	static bool Use(AVaelCharacter* Player, const UVaelMaterial* Potion);

	/** Factor on damage the player takes: fire protection, bark skin, dodging as ash */
	float GetIncomingMultiplier(const FGameplayTagContainer& DamageTags) const;

	/** Factor on walking speed: Leichtfuß, Blutrausch stacks */
	float GetSpeedMultiplier() const;

	/** Factor on how far creatures notice the player: Ascheschleier */
	float GetNoticeMultiplier() const;

	/** The player hit something for this much: blood oil and blood frenzy heal, an oil on the weapon passes on its effect */
	void OnDamageDealt(AActor* Target, float Damage);

	/** True if the player throws vials (the engineer fires them from sling and bow); the other classes coat their weapon */
	static bool ThrowsVials(const AVaelCharacter* Player);

	/** The oil on the weapon right now (a vial of "Wurf und Öl"), None if there is none */
	FName GetWeaponOil() const;

	/** The player was hit by this attacker for this much: the twilight brew heals a little and throws damage back */
	void OnDamageTaken(AActor* Attacker, float Damage);

	/** True while the effect of this potion lasts */
	bool IsActive(FName PotionId) const;

	/** Effects that last, with the seconds left, for the HUD and tests */
	TArray<TPair<FName, float>> GetActive() const;

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

private:

	struct FLasting
	{
		float EndTime = 0.0f;
		/** 1, or 1.25 for the own craft */
		float Strength = 1.0f;
		/** World time of the next pulse of a power potion */
		float NextPulse = 0.0f;
		int32 Pulses = 0;
	};

	struct FCloud
	{
		FVector Center = FVector::ZeroVector;
		float Radius = 300.0f;
		float DamagePerSecond = 0.0f;
		float EndTime = 0.0f;
		/** World time of the next bite */
		float NextBite = 0.0f;
	};

	bool Drink(FName PotionId, float Strength, float DurationScale);
	bool Throw(FName PotionId, float Strength);

public:
	/** Where a fired vial bursts: the cloud, the fire or the curse of it around Center */
	void Burst(FName PotionId, float Strength, const FVector& Center);

private:

	/** Nearest enemy of the player within the range, null if none */
	AVaelCharacterBase* FindEnemy(float Range) const;

	/** Every enemy around a spot */
	TArray<AVaelCharacterBase*> FindEnemies(const FVector& Center, float Radius) const;

	void Start(FName PotionId, float Seconds, float Strength, float FirstPulse = 0.0f);
	float Strength(FName PotionId) const;
	AVaelCharacter* GetPlayer() const;
	float Now() const;

	TMap<FName, FLasting> Lasting;
	TArray<FCloud> Clouds;

	/** Oil on the weapon: which vial and until when; enemies it has touched recently (it does not stack on one target) */
	FName WeaponOil;
	float WeaponOilEnd = 0.0f;
	TMap<TWeakObjectPtr<AActor>, float> OilTouched;

	/** Blutrausch: stacks of speed from hits, and when the last came */
	int32 FrenzyStacks = 0;
	float LastFrenzyHit = -100.0f;

	/** Light of Nachtsicht and Sternenblick */
	UPROPERTY(Transient)
	TObjectPtr<UPointLightComponent> SightLight;
};
