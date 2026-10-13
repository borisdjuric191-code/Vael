// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Items/VaelMaterial.h"
#include "Player/VaelInteractable.h"
#include "VaelRainBloom.generated.h"

class UPointLightComponent;
class UStaticMesh;
class UStaticMeshComponent;

/** What a flower does that grew where the rain of a moth fell */
UENUM(BlueprintType)
enum class EVaelBloomKind : uint8
{
	/** Erntekelch: does nothing by itself; picked, it gives its loot to every player */
	Harvest,
	/** Heilkelch: does nothing by itself; touched, it heals the player */
	Heal,
	/** Dornkelch: hurts every player near it, again and again */
	Thorn,
	/** Schlummerkelch: dazes every player near it, so they barely move */
	Slumber
};

/** One kind of flower the rain of a moth can let grow, and how likely it is */
USTRUCT(BlueprintType)
struct FVaelRainBloomKind
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Bloom")
	EVaelBloomKind Kind = EVaelBloomKind::Thorn;

	/** Name shown in the interact prompt and over a player it hurts */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Bloom")
	FText DisplayName;

	/** Model of the creature factory (mode "requisite") */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Bloom")
	TSoftObjectPtr<UStaticMesh> Mesh;

	/** Share of the flowers that are of this kind; the shares of all kinds are weighed against each other */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Bloom", meta = (ClampMin = 0))
	float Chance = 1.0f;

	/** How close a player has to be for a thorn or slumber flower to reach them, in cm */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Bloom", meta = (ClampMin = 0))
	float Radius = 170.0f;

	/** Damage of a thorn flower per hit, health a healing flower gives, share of the speed a slumber flower leaves */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Bloom", meta = (ClampMin = 0))
	float Amount = 4.0f;

	/** Seconds between two hits of a thorn flower */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Bloom", meta = (ClampMin = 0.1))
	float Interval = 1.0f;

	/** What picking a harvest flower gives every player */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Bloom")
	TArray<FVaelLootEntry> Loot;

	/** Color of its light, if it has one */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Bloom")
	FLinearColor GlowColor = FLinearColor::Black;
};

/**
 *  Tropfkelch: a flower that grows where the rain of a Nosferalter or a Taufalter soaked the earth, and nowhere else.
 *  Four kinds look alike: the thorn and the slumber flower act by themselves on anyone who comes near,
 *  the healing and the harvest flower do nothing until a player uses them. All of them wilt after a few minutes.
 */
UCLASS()
class AVaelRainBloom : public AActor, public IVaelInteractable
{
	GENERATED_BODY()

	/** The flower */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UStaticMeshComponent> Model;

	/** The shine of a healing flower */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UPointLightComponent> Glow;

public:

	/** Constructor */
	AVaelRainBloom();

	/** Lets a flower of the kind grow at a place on the ground; it wilts after the lifetime in seconds */
	static AVaelRainBloom* Grow(UWorld* World, const FVector& GroundLocation, const FVaelRainBloomKind& Kind, float Lifetime);

	//~Begin IVaelInteractable
	virtual bool CanInteract(const AVaelCharacter* Player) const override;
	virtual void Interact(AVaelCharacter* Player) override;
	virtual FText GetInteractPrompt() const override;
	//~End IVaelInteractable

	/** What it is */
	EVaelBloomKind GetBloomKind() const { return Kind.Kind; }

	/** Grows, acts on players near it, wilts */
	virtual void Tick(float DeltaSeconds) override;

protected:

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:

	/** Starts to wilt: it can't be used any more and is gone a moment later */
	void Wilt();

	FVaelRainBloomKind Kind;

	/** Seconds it has stood, and how long it stands before it wilts */
	float Age = 0.0f;
	float Lifetime = 150.0f;

	/** Seconds since it began to wilt, negative while it stands */
	float WiltAge = -1.0f;

	/** Seconds until a thorn flower strikes again, and seconds its strike shows */
	float StrikeCountdown = 0.0f;
	float StrikeShown = 0.0f;

	/** Size and turn of this one flower: every one is a little different */
	float SizeScale = 1.0f;
	float BaseYaw = 0.0f;
};
