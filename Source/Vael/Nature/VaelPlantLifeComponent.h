// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/SceneComponent.h"
#include "VaelPlantLifeComponent.generated.h"

class UInstancedStaticMeshComponent;
class UStaticMeshComponent;

/** How a plant model lives: it sways in the wind and drips */
USTRUCT(BlueprintType)
struct FVaelPlantLife
{
	GENERATED_BODY()

	/** Degrees it rocks about its foot by itself, and seconds one rocking takes; the wind of the region leans it further */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Plant Life", meta = (ClampMin = 0))
	float Sway = 2.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Plant Life", meta = (ClampMin = 0.5))
	float SwayPeriod = 5.0f;

	/** Drops that let go per second, from the undersides of its bells and leaves; 0 for none */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Plant Life", meta = (ClampMin = 0))
	float DripRate = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Plant Life")
	FLinearColor DripColor = FLinearColor(0.6f, 0.08f, 0.04f);

	/** Glow of the drops and their thickness in cm */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Plant Life", meta = (ClampMin = 0))
	float DripGlow = 0.6f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Plant Life", meta = (ClampMin = 0.5))
	float DripSize = 3.0f;
};

/**
 *  Gives a static plant model a little life (Saftperle, Eiterperle): the whole plant rocks softly about its foot and
 *  leans with the wind, and drops swell on the undersides of its bells, let go, fall and leave a small blot.
 *  Works only while the plant is in the picture.
 */
UCLASS()
class VAEL_API UVaelPlantLifeComponent : public USceneComponent
{
	GENERATED_BODY()

public:

	UVaelPlantLifeComponent();

	void Show(UStaticMeshComponent* InModel, const FVaelPlantLife& InLife);

	/** Stands the model back in its rest pose and hides the drops, until the next Show */
	void Stop();

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

private:

	struct FDrop
	{
		FVector Place = FVector::ZeroVector;     // in the model's space while it hangs
		FVector Fallen = FVector::ZeroVector;    // in the world once it has let go
		float Age = -1.0f;
		float Speed = 0.0f;
		float Size = 1.0f;
		bool bFalling = false;
	};

	void SamplePlaces();
	UInstancedStaticMeshComponent* AddPieces(const TCHAR* MeshPath, float Roughness);

	UPROPERTY(Transient)
	TObjectPtr<UStaticMeshComponent> Model;

	UPROPERTY(Transient)
	TObjectPtr<UInstancedStaticMeshComponent> Drops;

	UPROPERTY(Transient)
	TObjectPtr<UInstancedStaticMeshComponent> Blots;

	FVaelPlantLife Life;
	TArray<FVector> Places;
	TArray<FDrop> Hanging;
	TArray<FVector4> Lying;      // place and age of the blots
	FRotator RestRotation = FRotator::ZeroRotator;
	float GroundZ = 0.0f;
	float Credit = 0.0f;
	float Seed = 0.0f;
	int32 NextBlot = 0;
};
