// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "VaelEngineerShot.generated.h"

class UStaticMeshComponent;
class UStaticMesh;
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

	/** The piece a shot of the sling is (goal 16.11.): the plain stone, or the hull of its formula (slag, bladder, latex, glass, quill) */
	static UStaticMesh* GetShotMesh(FName Ammo);

	/** Gives a mesh the look of a shot: its piece, the core in the colour of the formula, sized to this diameter (cm); also the
	 *  load lying in the pouch of the sling while he draws (GlowBoost lets it shine at full draw) */
	static void DressShot(UStaticMeshComponent* Mesh, FName Ammo, float Diameter, float GlowBoost = 1.0f);

private:

	TWeakObjectPtr<UVaelEngineerComponent> Engineer;
	FVector Start = FVector::ZeroVector;
	FVector End = FVector::ZeroVector;

	/** Aim help (Boris 2026-11-27): the enemy near the line of the shot, followed while it stays close to where the shot was aimed */
	TWeakObjectPtr<AActor> AssistTarget;
	FVector AimedEnd = FVector::ZeroVector;
	bool bAssistChecked = false;
	float ShotSpeed = 0.0f;
	void SteerToTarget(float DeltaSeconds);
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

	/** A needle of Hohlkiel flies point first instead of tumbling */
	bool bNeedle = false;

	/** The streak behind it in flight (SM_Ing_Spur): its width and its length at a speed of 20 m/s */
	UPROPERTY(Transient)
	TObjectPtr<UStaticMeshComponent> Streak;
	float StreakWidth = 8.0f;
	float StreakLength = 100.0f;
	void AddStreak(const FLinearColor& Color, float Strength, float Width, float Length);
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
