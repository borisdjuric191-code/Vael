// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "VaelEngineerShot.generated.h"

class UStaticMeshComponent;
class UPointLightComponent;
class UNiagaraSystem;
class UVaelEngineerComponent;
enum class EVaelAmmoEffect : uint8;

/**
 *  A shot of the engineer: a stone of the sling, an arrow of the bow, a bolt of the crossbow, with or without a special head.
 *  It flies from the hand to the aim point in a flat arc and hits the first enemy in its way (a piercing one flies on), or a device
 *  of the engineer, which it sets off. Where it lands without a hit, shards and healing burst all the same.
 *  Placeholder look: a sphere of the engine shapes in the colour of the shot, stretched along its flight.
 */
UCLASS()
class AVaelEngineerShot : public AActor
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, Category="Components")
	TObjectPtr<UStaticMeshComponent> Body;

public:

	AVaelEngineerShot();

	virtual void Tick(float DeltaSeconds) override;

	static AVaelEngineerShot* Launch(UVaelEngineerComponent* Engineer, const FVector& From, const FVector& To, float Speed, float Arc, float Damage,
		FName Ammo, bool bPierce, const FLinearColor& Color, float Size, bool bStone = false);

	/** A turret fires too: the same shot with no hand behind it but the turret's engineer */
	FName GetAmmo() const { return Ammo; }

	/** How a kind of shot looks (goal 9.11.: the ammo was only to be seen by what it did): its colour, size against a plain
	 *  stone, how much it glows, whether it is a round drop instead of a stone */
	struct FLook
	{
		FLinearColor Color = FLinearColor(0.7f, 0.68f, 0.65f);
		float Scale = 1.0f;
		float Glow = 0.0f;
		bool bDrop = false;
		float Light = 0.0f;
	};
	static FLook GetLook(FName Ammo);

private:

	TWeakObjectPtr<UVaelEngineerComponent> Engineer;
	FVector Start = FVector::ZeroVector;
	FVector End = FVector::ZeroVector;
	FVector LastLocation = FVector::ZeroVector;
	float FlightTime = 0.5f;
	float Arc = 0.0f;
	float Age = 0.0f;
	float Damage = 0.0f;
	FName Ammo;
	FName WeaponId;
	bool bPierce = false;
	bool bHitSomething = false;

	/** A sling stone: a small tumbling rock with a puff of dust where it lands, instead of a bolt of light */
	bool bStone = false;
	FRotator Spin = FRotator::ZeroRotator;

	/** The trail it leaves: puffs of an effect every so many centimetres of its flight */
	UPROPERTY(Transient)
	TObjectPtr<UNiagaraSystem> TrailPuff;
	FLinearColor TrailColor = FLinearColor::White;
	float TrailRadius = 10.0f;
	float TrailEvery = 0.0f;
	float TrailWay = 0.0f;

	UPROPERTY(Transient)
	TObjectPtr<UPointLightComponent> Glow;

	/** Where it strikes or lands: the burst of its kind */
	void SpawnImpact(const FVector& Location) const;
	TSet<TWeakObjectPtr<AActor>> Hit;
};
