// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Engineer/VaelEngineerGear.h"
#include "VaelEngineerDevice.generated.h"

class UPointLightComponent;
class UStaticMeshComponent;
class UVaelEngineerComponent;

/**
 *  A device the engineer lays out before or in a fight (Lore-Bibel: placing and igniting, like the mercenary of PoE 2):
 *  a Fangeisen snaps shut on the first enemy that steps on it, a Knallmine waits for one of his shots (or the remote ignition)
 *  and bursts with the effect of that shot, a Schleuderturm fires stones at the nearest enemy for a while and fires the special
 *  shots he loads into it. Placeholder look: shapes of the engine in the colour of the device.
 */
UCLASS()
class AVaelEngineerDevice : public AActor
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, Category="Components")
	TObjectPtr<UStaticMeshComponent> Base;

	UPROPERTY(VisibleAnywhere, Category="Components")
	TObjectPtr<UStaticMeshComponent> Top;

	UPROPERTY(VisibleAnywhere, Category="Components")
	TObjectPtr<UPointLightComponent> Glow;

public:

	AVaelEngineerDevice();

	virtual void Tick(float DeltaSeconds) override;

	static AVaelEngineerDevice* Lay(UVaelEngineerComponent* Engineer, const FVaelGear& Gear, const FVector& Location);

	EVaelDeviceType GetDeviceType() const { return Type; }
	UVaelEngineerComponent* GetEngineer() const { return Engineer.Get(); }

	/** The mine bursts, carrying the effect of the shot that set it off */
	void Detonate(FName ShotAmmo);

	/** The turret takes a special shot as its ammo for the next shots */
	void LoadAmmo(FName ShotAmmo);

private:

	TWeakObjectPtr<UVaelEngineerComponent> Engineer;
	EVaelDeviceType Type = EVaelDeviceType::Trap;
	FName Id;
	float Born = 0.0f;
	float LifeTime = 120.0f;
	float NextShot = 0.0f;
	float SnappedAt = -1.0f;
	FName Loaded;
	int32 LoadedShots = 0;
	bool bDetonated = false;
};
