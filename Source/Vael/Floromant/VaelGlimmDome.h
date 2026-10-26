// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GameplayTagContainer.h"
#include "Floromant/VaelSymbiosis.h"
#include "VaelGlimmDome.generated.h"

class AVaelCharacterBase;
class UMaterialInstanceDynamic;
class UPointLightComponent;
class UStaticMeshComponent;
class UVaelPlantArmData;

/**
 *  The glass dome of the Glimmklee (Boris 2026-10-26): a large, barely visible sphere that glows warm at its rim, around a
 *  Floromant holding her shield hand up or around a Glimmklee tower. Enemy fire hitting anyone of her side inside is weaker
 *  (the dome swallows it and gives her Saft), fire cast by her side from inside is stronger, the mage may draw fire from it,
 *  and it lights the dark. Anchored (the other hand fires or she is rooted) it grows and holds better.
 *  Grown together with another plant it changes (VaelSymbiosis): Lichtkuppel, Glutglas, Funkelwind, Tauglocke.
 *  As Schwelklee it is a dark dome of smoke instead that blinds and singes the enemies inside.
 */
UCLASS()
class AVaelGlimmDome : public AActor
{
	GENERATED_BODY()

public:

	AVaelGlimmDome();

	/** Raises a dome around the centre for the owner; Pair is None for the dome of a single Glimmklee */
	static AVaelGlimmDome* Raise(APawn* InOwner, AActor* InCenter, const UVaelPlantArmData* InShield, EVaelSymbiosis InPair, float InRadius, bool bInShadow);

	virtual void Tick(float DeltaSeconds) override;

	/** Radius it grows or shrinks to, and how anchored it is (0 to 1, holds and burns better) */
	void SetTargetRadius(float InRadius) { TargetRadius = InRadius; }
	void SetAnchored(float InAnchored) { Anchored = InAnchored; }

	/** Lets it fade away and vanish */
	void Lower();

	/** True if the location lies inside */
	bool Covers(const FVector& Location) const;

	/** True while it stands and isn't fading */
	bool IsUp() const { return !bLowering; }

	/** Fire damage to a target after the domes around: weaker for enemy fire on her side inside (Saft for the owner), stronger
	 *  for fire her side casts from inside. Called for every hit by the attribute set */
	static float ScaleFireDamage(AVaelCharacterBase* Target, const AActor* Attacker, float Damage, const FGameplayTagContainer& DamageTags);

	/** True if a warm glass dome stands at the location, so the mage can draw fire from it */
	static bool IsFireSourceAt(const UWorld* World, const FVector& Location);

private:

	/** Enemy and allied fire scale now, after anchoring and the pair */
	float GetEnemyFireScale() const;
	float GetAlliedFireScale() const;

	/** What the pair or the shadow form does to those inside, every so often */
	void Pulse();

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UStaticMeshComponent> Glass;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UPointLightComponent> Light;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> GlassMaterial;

	UPROPERTY(Transient)
	TObjectPtr<const UVaelPlantArmData> Shield;

	TWeakObjectPtr<AActor> Center;
	EVaelSymbiosis Pair = EVaelSymbiosis::None;
	bool bShadow = false;
	bool bLowering = false;
	float Radius = 0.0f;
	float TargetRadius = 250.0f;
	float Anchored = 0.0f;
	float PulseCountdown = 0.5f;
	float FlashCountdown = 4.0f;
	float Flash = 0.0f;
};
