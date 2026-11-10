// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "VaelNightSpray.generated.h"

class UInstancedStaticMeshComponent;

/** What a night creature throws out */
UENUM(BlueprintType)
enum class EVaelNightSpray : uint8
{
	None,

	/** Reddish burrs burst out of the Moderglocke's cap, hang in the air and sink slowly, in a red-brown cloud */
	Spores,

	/** Drops of the cap's ichor fly out from its rim in arcs and leave dark blots on the ground */
	Venom
};

/**
 *  The spores and the poison of the Moderglocke: small meshes made in Tools/Nachtwesen/moderglocke_effekte.py
 *  (SM_Moderglocke_Spore, _Gifttropfen, _Giftlache), all pieces of one kind in one instanced component, moved here.
 *  Only a look: the creature deals the damage. Removes itself.
 */
UCLASS()
class VAEL_API AVaelNightSpray : public AActor
{
	GENERATED_BODY()

public:

	AVaelNightSpray();

	/**
	 *  Throws spores or poison out of a cap at From: the pieces start within RimRadius (cm) of it and spread over
	 *  about Reach (cm) around it; Scale is the size of the creature against a grown one.
	 */
	static AVaelNightSpray* Spawn(UWorld* World, EVaelNightSpray Kind, const FVector& From, float RimRadius, float Reach, float Scale);

	virtual void Tick(float DeltaSeconds) override;

private:

	struct FPiece
	{
		FVector Start = FVector::ZeroVector;
		FVector Velocity = FVector::ZeroVector;
		FRotator Tumble = FRotator::ZeroRotator;
		float Delay = 0.0f;
		float Size = 1.0f;
		float FlightTime = 1.0f;
		float Seed = 0.0f;
	};

	void Build(EVaelNightSpray InKind, const FVector& From, float RimRadius, float Reach, float Scale);
	UInstancedStaticMeshComponent* AddPieces(const TCHAR* MeshPath, const FLinearColor& Color, float Glow, float Roughness);

	UPROPERTY(Transient)
	TObjectPtr<UInstancedStaticMeshComponent> Flying;

	UPROPERTY(Transient)
	TObjectPtr<UInstancedStaticMeshComponent> Landed;

	TArray<FPiece> Pieces;
	EVaelNightSpray Kind = EVaelNightSpray::None;
	float GroundZ = 0.0f;
	float Age = 0.0f;
	float Lifetime = 3.0f;
};
