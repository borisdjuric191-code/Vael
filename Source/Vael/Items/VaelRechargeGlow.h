// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Items/VaelPickupOrb.h"
#include "VaelRechargeGlow.generated.h"

class AVaelCharacter;
class UPointLightComponent;
class UStaticMeshComponent;

/**
 *  Short glow on a player who just took a moth, follows them for about a second and goes away by itself.
 *  Health: green dots and strokes rise straight up around the body. Mana: a blue ring rises from the feet
 *  while sparks wind up around the body in two strands, so the two read apart by colour and by motion.
 */
UCLASS()
class AVaelRechargeGlow : public AActor
{
	GENERATED_BODY()

	/** Soft light on the player and the ground */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UPointLightComponent> Light;

public:

	/** Constructor */
	AVaelRechargeGlow();

	/** Plays the glow of a kind on a player */
	static AVaelRechargeGlow* Play(AVaelCharacter* Player, EVaelOrbKind InKind);

	/** Initialization */
	virtual void BeginPlay() override;

	/** Update */
	virtual void Tick(float DeltaSeconds) override;

private:

	/** One rising dot, stroke or spark */
	struct FGlowBit
	{
		/** Seconds after the start it appears and how long it lives */
		float Delay = 0.0f;
		float Life = 0.0f;

		/** Distance from the middle of the body and angle around it */
		float Radius = 0.0f;
		float Angle = 0.0f;

		/** Height it starts at and how far it rises, in cm */
		float StartHeight = 0.0f;
		float Rise = 0.0f;

		/** Size of the bit and how far it stretches upward */
		float Size = 0.0f;
		float Stretch = 1.0f;
	};

	/** Builds the bits of the kind */
	void BuildBits();

	/** Places one bit of the health glow */
	void PlaceHealthBit(int32 Index, float Along) const;

	/** Places one bit of the mana glow */
	void PlaceManaBit(int32 Index, float Along) const;

	/** Shapes of the bits */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UStaticMeshComponent>> Shapes;

	/** Beads of the mana ring */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UStaticMeshComponent>> RingBeads;

	TArray<FGlowBit> Bits;

	/** Player the glow follows */
	TWeakObjectPtr<AVaelCharacter> Player;

	EVaelOrbKind Kind = EVaelOrbKind::Health;

	/** Seconds since the start */
	float Clock = 0.0f;
};
