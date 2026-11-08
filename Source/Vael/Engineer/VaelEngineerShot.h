// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "VaelEngineerShot.generated.h"

class UStaticMeshComponent;
class UVaelEngineerComponent;

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
		FName Ammo, bool bPierce, const FLinearColor& Color, float Size);

	/** A turret fires too: the same shot with no hand behind it but the turret's engineer */
	FName GetAmmo() const { return Ammo; }

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
	TSet<TWeakObjectPtr<AActor>> Hit;
};
