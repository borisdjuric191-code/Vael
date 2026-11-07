// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "VaelCaveMotes.generated.h"

class UInstancedStaticMeshComponent;
class UMaterialInstanceDynamic;

/** What drifts in the air of a cave */
UENUM(BlueprintType)
enum class EVaelCaveMote : uint8
{
	/** Pale dust that hangs and slowly sinks in a light shaft, twinkling where the light catches it */
	Dust,
	/** Small glowing embers that rise from glowing ground, wobble and go out on the way up */
	Ember
};

/**
 *  Air of a cave: a few hundred tiny motes inside a cylinder (Radius, Height) that drift on their own, for light shafts
 *  (dust) and glowing ground (embers) in the refugia. All motes are instances of the engine sphere in the unlit hail material
 *  (M_Vael_Hageleis, made for instances), moved on the game thread; no Niagara asset needed. A mote that leaves the
 *  cylinder starts again inside it.
 */
UCLASS(meta = (DisplayName = "Vael Cave Motes"))
class AVaelCaveMotes : public AActor
{
	GENERATED_BODY()

public:

	AVaelCaveMotes();

	/** Dust or embers */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Motes")
	EVaelCaveMote Kind = EVaelCaveMote::Dust;

	/** How many motes */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Motes", meta = (ClampMin = "1", ClampMax = "600"))
	int32 Count = 60;

	/** Radius of the cylinder in cm (the actor stands at its bottom middle) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Motes", meta = (ClampMin = "10"))
	float Radius = 120.0f;

	/** Height of the cylinder in cm */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Motes", meta = (ClampMin = "10"))
	float Height = 600.0f;

	/** Scales the glow of every mote */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Motes", meta = (ClampMin = "0"))
	float Brightness = 1.0f;

	virtual void Tick(float DeltaSeconds) override;

protected:

	virtual void BeginPlay() override;

private:

	struct FMote
	{
		FVector Location = FVector::ZeroVector;
		FVector Velocity = FVector::ZeroVector;
		float Phase = 0.0f;
		float Size = 1.0f;
		/** Seconds since it started; embers go out after Life */
		float Age = 0.0f;
		float Life = 1.0f;
	};

	/** Starts a mote again somewhere in the cylinder; embers start at the bottom */
	void Respawn(FMote& Mote, bool bAnywhere) const;

	UPROPERTY(VisibleAnywhere, Category = "Motes")
	TObjectPtr<UInstancedStaticMeshComponent> Mesh;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> Material;

	TArray<FMote> Motes;
	TArray<FTransform> Transforms;
};
