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

	/** The acid pool of the Säurefass (a flat disc that spreads and shrinks again) */
	UPROPERTY(VisibleAnywhere, Category="Components")
	TObjectPtr<UStaticMeshComponent> Pool;

	/** The Rollwall stands in the way once it is up */
	UPROPERTY(VisibleAnywhere, Category="Components")
	TObjectPtr<class UBoxComponent> WallBox;

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

	/** Whether it has gone off (mines), and the most enemies its smoke held at once (for the tests) */
	bool HasDetonated() const { return bDetonated; }
	int32 GetSmokeHeld() const { return SmokeHeld; }
	int32 GetTapperShots() const { return TapperShots; }

	/** What the newer devices did, for the tests: hits of the Sichelkreisel, enemies held by the net, blinded by the
	 *  lantern, marked or pushed by the Sturmwarner, knocked aside by the Rollwall */
	int32 GetWork() const { return Work; }

	/** The Rollwall starts at his feet and rolls to this spot */
	void StartRoll(const FVector& Target);
	bool IsWallUp() const { return bWallUp; }
	bool IsPool() const { return PoolUntil >= 0.0f; }
	float GetSmokeDrifted() const { return SmokeDrifted.Size(); }
	int32 GetAcidBitten() const { return AcidBitten; }

	/** The Säurefass bursts into its pool (stepped on or shot) */
	void BurstAcid();

	/** A burst of a Sprengmeister charge where it goes off: returns how many it struck */
	int32 ChargeBurst(const FVector& At, float Radius, float Damage, EVaelAmmoEffect Effect);

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
	int32 SmokeHeld = 0;
	float LaidZ = 0.0f;
	float PoolUntil = -1.0f;
	float PoolRadius = 0.0f;

	/** The look moves (25.11.): rising out of the ground, the nozzle turning and kicking back, the barrel rocking, the lid
	 *  flying off, the pouch falling; called every frame before the work of the device */
	void Animate(float Now, float DeltaSeconds);
	float NozzleYaw = 0.0f;
	float KickAt = -10.0f;
	float BurstAt = -10.0f;
	FVector LidVelocity = FVector::ZeroVector;
	float LidSpin = 0.0f;
	bool bHasPart = false;
	FVector BaseScale = FVector::ZeroVector;

	/** The five newer devices (25.11.): returns true when it handled the tick */
	bool TickNewer(float Now, float DeltaSeconds);
	int32 Work = 0;
	float SprungAt = -1.0f;
	float NextPulse = 0.0f;
	FVector RollTarget = FVector::ZeroVector;
	bool bRolling = false;
	bool bWallUp = false;
	float WallSince = 0.0f;
	float RollSpun = 0.0f;
	TSet<TWeakObjectPtr<AActor>> Touched;
	int32 TapperShots = 0;
	FVector SmokeDrifted = FVector::ZeroVector;
	int32 AcidBitten = 0;
	/** Rammschild: when each enemy was last struck, and how fast it last came at the wall */
	TMap<TWeakObjectPtr<AActor>, float> RamTouched;
	TMap<TWeakObjectPtr<AActor>, float> RamSpeed;
	/** Kristallladung: the effect of its ore */
	EVaelAmmoEffect ChargeEffect = EVaelAmmoEffect::None;
	FLinearColor ChargeColor = FLinearColor::White;

	bool bDetonated = false;
	bool bBuiltPiece = false;
};
