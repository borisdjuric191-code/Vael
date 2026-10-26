// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "VaelRoseHeart.generated.h"

class AVaelCharacter;
class UMaterialInstanceDynamic;
class UPointLightComponent;
class UStaticMesh;
class UStaticMeshComponent;

/** What the Rußrose does to those around her: heal, drain, cleanse (towers, pairs and the vine arm share it) */
namespace VaelRoseCare
{
	/** The player of her side who needs it most within the range: a fallen one first if asked, else the lowest share of health;
	 *  with an aim, those in front count more. Herself only if asked. Null if nobody fits */
	AVaelCharacter* FindHurtAlly(const AVaelCharacter* Gardener, const FVector& From, float Range, bool bIncludeSelf, bool bPreferDowned, const FVector& Aim = FVector::ZeroVector);

	/** Heals every standing player within the radius by the share of their health; returns how many */
	int32 HealAround(AActor* WorldContext, const FVector& Center, float Radius, float ShareOfMax);

	/** Hurts every enemy of the gardener within the radius and gives her the share of it as health; returns how many */
	int32 DrainAround(AVaelCharacter* Gardener, const FVector& Center, float Radius, float Damage, float HealShare);

	/** Takes burning, wetness, frost and slowness off every player within the radius and heals the own plants there; returns players */
	int32 CleanseAround(AActor* WorldContext, const FVector& Center, float Radius, float PlantHeal);

	/** Rose petals scattering where they arrive at To from From, glowing green (or blood red) */
	void Petals(AActor* WorldContext, const FVector& From, const FVector& To, bool bBlood, int32 Count = 10);
}

/**
 *  The green heart of the Rußrose (Boris 2026-10-26): when both rose arms cross, the two shoulder roses wander forward and
 *  grow into one big rose, which shoots its glowing heart at an ally. It follows them and heals them strongly, or brings a
 *  fallen one back to their feet. Alone the heart flies out a little and comes back to her, at half strength.
 */
UCLASS()
class AVaelRoseHeart : public AActor
{
	GENERATED_BODY()

public:

	AVaelRoseHeart();

	/** Shoots the heart from the place: at the ally, or out along the aim and back to the gardener when there is none */
	static AVaelRoseHeart* Shoot(AVaelCharacter* Gardener, const FVector& From, AVaelCharacter* Ally, const FVector& Aim, float HealShare, float ReviveShare, float Speed,
		const FLinearColor& Color, UStaticMesh* RoseMesh, float RoseSize);

	virtual void Tick(float DeltaSeconds) override;

private:

	/** Heals or revives who it reached */
	void Arrive(AVaelCharacter* Reached);

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UStaticMeshComponent> Heart;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UPointLightComponent> Light;

	/** The big rose the two grew into, left where it shot and fading */
	UPROPERTY(Transient)
	TObjectPtr<UStaticMeshComponent> Rose;

	TWeakObjectPtr<AVaelCharacter> Gardener;
	TWeakObjectPtr<AVaelCharacter> Ally;
	FVector TurnPoint = FVector::ZeroVector;
	bool bReturning = false;
	float HealShare = 0.4f;
	float ReviveShare = 0.5f;
	float Speed = 1300.0f;
	float Age = 0.0f;
	float RoseSize = 60.0f;
	float SinceMote = 0.0f;
	FLinearColor Color = FLinearColor::Green;
};
