// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Engineer/VaelEngineerGear.h"
#include "VaelEngineerComponent.generated.h"

class AVaelCharacter;
class AVaelCharacterBase;
class AVaelEngineerDevice;
struct FVaelSavedPlayer;

/**
 *  What a player does as engineer (Lore-Bibel, Ingenieur; Boris 2026-11-07): a class like a hunter of Monster Hunter who builds
 *  everything himself. He finds only materials and blueprints; gear that falls for him he takes apart. He starts with the sling
 *  and endless stones; every other weapon, every special shot, every device and every piece of armour he builds at the workbench
 *  of the refugium (shots and devices also at the field box by a campfire). What he carries is limited, so he prepares before
 *  he sets out: the rest waits in his stock and fills his pouches up again at the workbench or the field box.
 *
 *  Controls (IMC_Ingenieur, AVaelPlayerController): R2 / left mouse fires while held, L2 / right mouse lays the picked device
 *  where he aims, R1 / mouse wheel turns the shot, d-pad left / right and Z / C the device, d-pad up and R the weapon,
 *  Square and X ignite every mine, L1 and Q the potion belt.
 */
UCLASS()
class UVaelEngineerComponent : public UActorComponent
{
	GENERATED_BODY()

public:

	UVaelEngineerComponent();

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	// --- Building

	/** Why a piece can't be built right now; Ok if it can */
	enum class EBuildBlock : uint8 { Ok, Built, NeedsFirst, NeedsWorkbench, Materials, Full };
	EBuildBlock GetBuildBlock(const FVaelGear& Gear, bool bAtWorkbench) const;

	/** Builds a piece from the bag: a weapon or armour is his for good (armour is worn at once if its place is free), shots and
	 *  devices go into the pouches up to what he can carry, the rest into the stock */
	bool Build(FName Id, bool bAtWorkbench);

	/** Workbench and field box: the pouches fill up from the stock */
	void Restock();

	bool HasBuilt(FName Id) const;

	/** How many shots or devices of a kind are in the pouches and in the stock */
	int32 GetCarried(FName Id) const;
	int32 GetStock(FName Id) const;
	int32 GetCarryLimit(const FVaelGear& Gear) const;

	// --- Weapon and shot

	const FVaelGear* GetWeapon() const;

	/** The next weapon he has built (d-pad up, R) */
	void CycleWeapon();

	/** The shot in the weapon: Standard (endless stones, arrows, bolts) or a special one */
	FName GetAmmo() const { return Ammo; }
	FText GetAmmoName() const;
	FLinearColor GetAmmoColor() const;

	/** The next shot he has in the pouches (R1, mouse wheel) */
	void CycleAmmo(int32 Step);

	/** Fires once towards the aim point if the weapon is ready; holding the button fires again and again */
	bool Fire(const FVector& AimLocation);

	/** Seconds until the weapon can fire again, and the whole interval */
	float GetReloadLeft() const;
	float GetInterval() const;

	// --- Devices

	FName GetDevice() const { return Device; }
	void CycleDevice(int32 Step);

	/** Lays the picked device at the aim point (in reach) */
	bool PlaceDevice(const FVector& AimLocation);

	/** Square / X: every mine he laid goes off */
	int32 IgniteAll();

	int32 GetNumPlaced() const;

	// --- Armour

	/** The piece worn on a place, None if none */
	FName GetWorn(EVaelArmorPlace Place) const;

	/** Wears a built piece on its place */
	bool Wear(FName Id);

	/** How many worn pieces belong to a set */
	int32 CountSet(FName Set) const;

	/** The sum of armour of the worn pieces (each point is a percent less damage) */
	float GetArmorTotal() const;

	/** Damage a hit on him really does, after armour and set bonuses */
	float ScaleIncoming(float Damage) const;

	/** He hit something for this much: the set of the Blutschuppen heals */
	void OnDamageDealt(float Damage);

	/** He dodged: four pieces of the Rollpanzer set curl him up for a moment */
	void OnDodged();

	/** How fast he walks with his set (Rußfeder) */
	float GetMoveSpeedScale() const;

	// --- Shots that land (from AVaelEngineerShot and the turrets)

	/** What a shot does where it lands on an enemy, or on a device of his */
	void ApplyHit(AActor* Target, const FVector& Location, float Damage, FName ShotAmmo);
	void ApplyBurst(const FVector& Location, FName ShotAmmo, float Damage);

	/** A device was hit by one of his shots */
	void OnDeviceShot(AVaelEngineerDevice* Placed, FName ShotAmmo);

	/** The damage scale of his sets (Sichelklinge) and whether every shot pierces */
	float GetDamageScale() const;
	bool PiercesAll() const;

	// --- Gear that falls for him

	/** Takes a piece of gear apart into materials instead of carrying it; returns false if nothing came of it */
	static bool Dismantle(AVaelCharacter* Player, int32 Rarity, int32 ItemLevel);

	// --- Save

	void WriteSave(FVaelSavedPlayer& Saved) const;
	void ReadSave(const FVaelSavedPlayer& Saved);

	/** Back to how he starts: only the sling */
	void ResetToStart();

private:

	AVaelCharacter* GetPlayer() const;
	float Now() const;
	void ApplyHealthBonus();

	UPROPERTY()
	TSet<FName> Built;

	/** Pouches and stock of shots and devices */
	UPROPERTY()
	TMap<FName, int32> Carried;

	UPROPERTY()
	TMap<FName, int32> Stock;

	/** Worn pieces by place */
	UPROPERTY()
	TArray<FName> Worn;

	FName Weapon;
	FName Ammo;
	FName Device;
	float LastShot = -100.0f;
	float CurledUntil = 0.0f;
	float AppliedHealth = 0.0f;

	UPROPERTY(Transient)
	TArray<TWeakObjectPtr<AVaelEngineerDevice>> Placed;

	/** Poisons ticking on enemies */
	struct FPoison
	{
		TWeakObjectPtr<AVaelCharacterBase> Target;
		float DamagePerSecond = 0.0f;
		float End = 0.0f;
	};
	TArray<FPoison> Poisons;
	float NextPoisonTick = 0.0f;
};
