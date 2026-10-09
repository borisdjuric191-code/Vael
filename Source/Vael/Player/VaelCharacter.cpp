// Copyright Epic Games, Inc. All Rights Reserved.

#include "Player/VaelCharacter.h"
#include "UObject/ConstructorHelpers.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Camera/VaelSharedCamera.h"
#include "Combat/VaelAttributeSet.h"
#include "Combat/VaelCombatStatics.h"
#include "AbilitySystemComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Items/VaelInventory.h"
#include "Items/VaelMaterialBag.h"
#include "Magic/VaelElementComponent.h"
#include "Magic/VaelElementOrbitComponent.h"
#include "Magic/VaelGameplayTags.h"
#include "Magic/VaelLightningBolt.h"
#include "Magic/VaelMagicSettings.h"
#include "Magic/VaelSpellEffects.h"
#include "Magic/VaelSpellGust.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "NiagaraComponent.h"
#include "NiagaraSystem.h"
#include "Player/VaelPlayerController.h"
#include "UI/VaelNoticeSubsystem.h"
#include "Vael.h"
#include "VaelAssets.h"
#include "VaelGameMode.h"

namespace
{
	/** Bolts lashing out where a dash of lightning lands */
	constexpr int32 DashLightningBolts = 6;

	/** Blend time when a roll montage is cut short */
	constexpr float RollMontageBlendOutTime = 0.15f;
}

AVaelCharacter::AVaelCharacter()
{
	// Set size for player capsule
	GetCapsuleComponent()->InitCapsuleSize(42.f, 96.0f);

	// The character turns towards the aim direction, which the controller stores as control rotation
	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = false;
	bUseControllerRotationRoll = false;

	// Configure character movement
	GetCharacterMovement()->bOrientRotationToMovement = false;
	GetCharacterMovement()->bUseControllerDesiredRotation = true;
	GetCharacterMovement()->RotationRate = FRotator(0.f, 900.f, 0.f);
	GetCharacterMovement()->bConstrainToPlane = true;
	GetCharacterMovement()->bSnapToPlaneAtStart = true;

	// Placeholder look: the template mannequin, until the classes get their own characters
	GetMesh()->SetRelativeLocationAndRotation(FVector(0.f, 0.f, -89.f), FRotator(0.f, -90.f, 0.f));

	static ConstructorHelpers::FObjectFinder<USkeletalMesh> PlaceholderMesh(TEXT("/Game/Characters/Mannequins/Meshes/SKM_Manny_Simple.SKM_Manny_Simple"));
	if (PlaceholderMesh.Succeeded())
	{
		GetMesh()->SetSkeletalMesh(PlaceholderMesh.Object);
	}

	static ConstructorHelpers::FClassFinder<UAnimInstance> PlaceholderAnimClass(TEXT("/Game/Characters/Mannequins/Anims/Unarmed/ABP_Unarmed"));
	if (PlaceholderAnimClass.Succeeded())
	{
		GetMesh()->SetAnimInstanceClass(PlaceholderAnimClass.Class);
	}

	// Create the player marker from engine placeholder assets
	PlayerMarker = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PlayerMarker"));

	PlayerMarker->SetupAttachment(RootComponent);
	PlayerMarker->SetRelativeLocation(FVector(0.f, 0.f, -95.f));
	PlayerMarker->SetRelativeScale3D(FVector(1.3f, 1.3f, 0.02f));
	PlayerMarker->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	PlayerMarker->SetCastShadow(false);
	PlayerMarker->bReceivesDecals = false;

	static ConstructorHelpers::FObjectFinder<UStaticMesh> MarkerMesh(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	if (MarkerMesh.Succeeded())
	{
		PlayerMarker->SetStaticMesh(MarkerMesh.Object);
	}

	static ConstructorHelpers::FObjectFinder<UMaterialInterface> MarkerMaterial(TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
	if (MarkerMaterial.Succeeded())
	{
		PlayerMarker->SetMaterial(0, MarkerMaterial.Object);
	}

	// Mana regeneration of the browser prototype
	ManaRegenPerSecond = 13.0f;

	// Create the element queue
	ElementComponent = CreateDefaultSubobject<UVaelElementComponent>(TEXT("ElementQueue"));

	// Create the material bag
	MaterialBag = CreateDefaultSubobject<UVaelMaterialBag>(TEXT("MaterialBag"));

	// Create the equipment and backpack
	Inventory = CreateDefaultSubobject<UVaelInventory>(TEXT("Inventory"));

	// Create the circle of element orbs above the head
	ElementOrbit = CreateDefaultSubobject<UVaelElementOrbitComponent>(TEXT("ElementOrbit"));
	ElementOrbit->SetupAttachment(RootComponent);

	// Create the light at the casting hand; it moves to the hand socket when the game starts
	HandLight = CreateDefaultSubobject<UPointLightComponent>(TEXT("HandLight"));
	HandLight->SetupAttachment(GetMesh());
	HandLight->SetMobility(EComponentMobility::Movable);
	HandLight->SetIntensityUnits(ELightUnits::Candelas);
	HandLight->SetIntensity(0.0f);
	HandLight->SetAttenuationRadius(420.0f);
	HandLight->SetSourceRadius(6.0f);
	HandLight->SetCastShadows(false);
	HandLight->SetVisibility(false);

	// Activate ticking in order to drive the dodge roll.
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = true;
}

void AVaelCharacter::BeginPlay()
{
	Super::BeginPlay();

	// The animations of the mage take over from the template ones as soon as they are made in the editor
	if (UClass* AnimClass = VaelAssets::LoadOptionalClass(MageAnimClass))
	{
		GetMesh()->SetAnimInstanceClass(AnimClass);
	}

	LoadedDodgeMontage = VaelAssets::LoadOptional(DodgeMontage);
	LoadedSpellDashMontage = VaelAssets::LoadOptional(SpellDashMontage);

	DefaultMeshScale = GetMesh()->GetRelativeScale3D();
	DefaultWalkSpeed = GetCharacterMovement()->MaxWalkSpeed;

	ElementOrbit->SetRelativeLocation(FVector(0.f, 0.f, QueueOrbHeight));

	const UVaelMagicSettings* MagicSettings = UVaelMagicSettings::Get();
	LoadedElementSelectMontage = VaelAssets::LoadOptional(MagicSettings->ElementSelectMontage);

	// The light sits at the casting hand, or in front of the chest while the mesh has no such socket
	if (GetMesh()->DoesSocketExist(MagicSettings->CastSocketName))
	{
		HandLight->AttachToComponent(GetMesh(), FAttachmentTransformRules::SnapToTargetNotIncludingScale, MagicSettings->CastSocketName);
	}
	else
	{
		HandLight->AttachToComponent(RootComponent, FAttachmentTransformRules::SnapToTargetNotIncludingScale);
		HandLight->SetRelativeLocation(FVector(45.0f, 20.0f, 25.0f));
	}

	// The hand glows in the color of the chosen elements, once the effect exists
	HandEffect = VaelEffects::Attach(VaelAssets::LoadOptional(MagicSettings->HandEffect), GetMesh(), MagicSettings->CastSocketName, FLinearColor::White, 0.0f, false);
	if (HandEffect != nullptr)
	{
		HandEffect->Deactivate();
	}

	ElementComponent->OnQueueChanged.AddUObject(this, &AVaelCharacter::OnElementQueueChanged);
	RefreshQueueOrbs();
}

void AVaelCharacter::RefreshHandEffect()
{
	const TArray<EVaelElement>& Queue = ElementComponent->GetQueue();

	// The light at the hand works without the effect: newest element as color, brighter with every element and from the environment
	if (Queue.IsEmpty())
	{
		CurrentHandLightIntensity = 0.0f;
		HandLight->SetVisibility(false);
	}
	else
	{
		const int32 NewestLit = Queue.Num() - 1;
		CurrentHandLightIntensity = HandLightIntensity * (0.6f + 0.4f * Queue.Num()) * (ElementComponent->IsFromEnvironment(NewestLit) ? 2.0f : 1.0f);
		HandLight->SetLightColor(UVaelMagicSettings::Get()->GetElementColor(Queue[NewestLit]));
		HandLight->SetVisibility(true);
	}

	if (HandEffect == nullptr)
	{
		return;
	}

	if (Queue.IsEmpty())
	{
		HandEffect->Deactivate();
		return;
	}

	// The newest element colors the glow; drawn from the environment it shines twice as strong
	const int32 Newest = Queue.Num() - 1;
	HandEffect->SetVariableLinearColor(VaelEffects::ColorParameter, UVaelMagicSettings::Get()->GetElementColor(Queue[Newest]));
	HandEffect->SetVariableFloat(VaelEffects::IntensityParameter, ElementComponent->IsFromEnvironment(Newest) ? 2.0f : 1.0f);
	HandEffect->SetVariableFloat(VaelEffects::RadiusParameter, Queue.Num());

	if (!HandEffect->IsActive())
	{
		HandEffect->Activate(true);
	}
}

void AVaelCharacter::OnElementQueueChanged()
{
	const int32 NumQueued = ElementComponent->GetQueue().Num();
	const bool bElementAdded = NumQueued > NumQueuedElements;
	NumQueuedElements = NumQueued;

	RefreshQueueOrbs();
	RefreshHandEffect();

	// A short gesture of the hand for each chosen element, unless a spell or a roll is playing
	const UAbilitySystemComponent* AbilitySystem = GetAbilitySystemComponent();
	const bool bBusy = bIsDodging || AbilitySystem->HasMatchingGameplayTag(VaelTags::State_Casting) || AbilitySystem->HasMatchingGameplayTag(VaelTags::State_Channeling);

	if (bElementAdded && !bBusy && LoadedElementSelectMontage != nullptr)
	{
		PlayAnimMontage(LoadedElementSelectMontage);
	}
}

void AVaelCharacter::RefreshQueueOrbs()
{
	ElementOrbit->ShowQueue(*ElementComponent);
}

void AVaelCharacter::FlickerHandLight()
{
	const float Time = GetWorld()->GetTimeSeconds();

	if (CurrentHandLightIntensity > 0.0f)
	{
		HandLight->SetIntensity(CurrentHandLightIntensity * (1.0f + 0.16f * FMath::Sin(Time * 9.0f) + 0.09f * FMath::Sin(Time * 23.0f)));
	}
}

void AVaelCharacter::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	FlickerHandLight();

#if ENABLE_DRAW_DEBUG
	if (bShowStatusText)
	{
		const FString Status = bDowned
			? FString::Printf(TEXT("Am Boden   Wiederbeleben %.0f %%"), 100.f * ReviveProgress / FMath::Max(ReviveDuration, 0.01f))
			: FString::Printf(TEXT("Leben %.0f   Mana %.0f   %s"), GetHealth(), GetMana(), *GetStatusText().ToString());
		DrawDebugString(GetWorld(), FVector(0.f, 0.f, QueueOrbHeight + 45.f), Status, this, bDowned ? FColor(255, 107, 94) : FColor::White, 0.f, true);
	}
#endif

	if (bDowned)
	{
		TickRevive(DeltaSeconds);
		return;
	}

	// Channeling a spell holds the mage back
	const bool bChanneling = GetAbilitySystemComponent()->HasMatchingGameplayTag(VaelTags::State_Channeling);
	GetCharacterMovement()->MaxWalkSpeed = DefaultWalkSpeed * (bChanneling ? UVaelMagicSettings::Get()->ChannelMoveSpeedMultiplier : 1.0f);

	if (bIsDodging)
	{
		if (GetWorld()->GetTimeSeconds() >= DodgeEndTime)
		{
			EndDodge();

			// A spell dash ends in a burst around the player
			if (DashBurstRadius > 0.0f)
			{
				UVaelCombatStatics::ApplySpellHitInRadius(this, GetActorLocation(), DashBurstRadius, DashBurst);
				ShowDashBurst();
				DashBurstRadius = 0.0f;
			}
		}
		else
		{
			// Hold the roll speed, leave falling to the movement component
			UCharacterMovementComponent* Movement = GetCharacterMovement();
			Movement->Velocity = FVector(DodgeDirection.X * CurrentDodgeSpeed, DodgeDirection.Y * CurrentDodgeSpeed, Movement->Velocity.Z);
		}
	}
}

void AVaelCharacter::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);

	const AVaelPlayerController* VaelController = Cast<AVaelPlayerController>(NewController);
	const AVaelGameMode* GameMode = GetWorld()->GetAuthGameMode<AVaelGameMode>();

	if (VaelController != nullptr && GameMode != nullptr)
	{
		SetPlayerColor(GameMode->GetPlayerColor(VaelController->GetPlayerSlot()));

		UE_LOG(LogVael, Log, TEXT("Player slot %d controls '%s'"), VaelController->GetPlayerSlot(), *GetNameSafe(this));
	}
}

bool AVaelCharacter::StartDodge(const FVector& WorldDirection)
{
	const float Now = GetWorld()->GetTimeSeconds();
	if (bDowned || bIsDodging || Now < NextDodgeTime || !GetCharacterMovement()->IsMovingOnGround())
	{
		return false;
	}

	DodgeDirection = WorldDirection.GetSafeNormal2D();
	if (DodgeDirection.IsNearlyZero())
	{
		DodgeDirection = GetActorForwardVector().GetSafeNormal2D();
	}

	bIsDodging = true;
	CurrentDodgeSpeed = DodgeSpeed;
	DashBurstRadius = 0.0f;
	DodgeEndTime = Now + DodgeDuration;
	NextDodgeTime = Now + DodgeCooldown;

	BeginRollLook(LoadedDodgeMontage, DodgeDuration);

	return true;
}

bool AVaelCharacter::StartSpellDash(const FVector& WorldDirection, float Speed, float Duration, const FVaelSpellHit& InDashBurst, float BurstRadius)
{
	if (bDowned)
	{
		return false;
	}

	DodgeDirection = WorldDirection.GetSafeNormal2D();
	if (DodgeDirection.IsNearlyZero())
	{
		DodgeDirection = GetActorForwardVector().GetSafeNormal2D();
	}

	// Rides on the movement of a dodge roll, which also keeps the player safe while dashing
	bIsDodging = true;
	CurrentDodgeSpeed = Speed;
	DashBurst = InDashBurst;
	DashBurstRadius = BurstRadius;
	DashStartLocation = GetActorLocation();
	DodgeEndTime = GetWorld()->GetTimeSeconds() + Duration;

	BeginRollLook(LoadedSpellDashMontage != nullptr ? LoadedSpellDashMontage : LoadedDodgeMontage, Duration);

	return true;
}

void AVaelCharacter::ShowDashBurst()
{
	const FVector Center = GetActorLocation();
	const FLinearColor Color = UVaelMagicSettings::Get()->GetElementColor(DashBurst.Element);

	if (DashBurst.bLightning)
	{
		// The jump leaves a bolt along its path, and lightning lashes out all around where it lands
		AVaelLightningBolt::Spawn(this, DashStartLocation, Center, Color, 0.3f, 1.3f, false);

		const float FeetHeight = GetCapsuleComponent()->GetScaledCapsuleHalfHeight() * 0.6f;
		const float FirstAngle = FMath::FRandRange(0.0f, 360.0f);
		for (int32 BoltIndex = 0; BoltIndex < DashLightningBolts; ++BoltIndex)
		{
			const float Angle = FMath::DegreesToRadians(FirstAngle + BoltIndex * 360.0f / DashLightningBolts + FMath::FRandRange(-15.0f, 15.0f));
			const FVector Out(FMath::Cos(Angle), FMath::Sin(Angle), 0.0f);

			AVaelLightningBolt::Spawn(this, Center, Center + Out * DashBurstRadius * FMath::FRandRange(0.75f, 1.0f) - FVector(0.0f, 0.0f, FeetHeight), Color, 0.22f, 0.8f, true);
		}

		return;
	}

	if (DashBurst.Element == EVaelElement::Air)
	{
		// A ring of wind pushes out all around
		AVaelSpellGust::Spawn(this, Center, DodgeDirection, DashBurstRadius, 180.0f, Color);
		return;
	}

#if ENABLE_DRAW_DEBUG
	DrawDebugCircle(GetWorld(), Center, DashBurstRadius, 32, Color.ToFColor(true), false, 0.25f, 0, 4.0f, FVector::ForwardVector, FVector::RightVector, false);
#endif
}

void AVaelCharacter::BeginRollLook(UAnimMontage* Montage, float Duration)
{
	// The body rolls the way it moves; it turns back to the aim direction afterwards
	GetCharacterMovement()->bUseControllerDesiredRotation = false;
	SetActorRotation(DodgeDirection.Rotation());

	ActiveRollMontage = nullptr;

	if (Montage != nullptr && Duration > 0.0f && PlayAnimMontage(Montage, Montage->GetPlayLength() / Duration) > 0.0f)
	{
		ActiveRollMontage = Montage;
		GetMesh()->SetRelativeScale3D(DefaultMeshScale);
	}
	else
	{
		GetMesh()->SetRelativeScale3D(DefaultMeshScale * FVector(1.f, 1.f, DodgeMeshSquash));
	}
}

void AVaelCharacter::SetPlayerColor(const FLinearColor& Color)
{
	if (UMaterialInstanceDynamic* MarkerMaterial = PlayerMarker->CreateAndSetMaterialInstanceDynamic(0))
	{
		MarkerMaterial->SetVectorParameterValue(MarkerColorParameter, Color);
	}

	USkeletalMeshComponent* BodyMesh = GetMesh();
	for (int32 MaterialIndex = 0; MaterialIndex < BodyMesh->GetNumMaterials(); ++MaterialIndex)
	{
		if (UMaterialInstanceDynamic* BodyMaterial = BodyMesh->CreateAndSetMaterialInstanceDynamic(MaterialIndex))
		{
			BodyMaterial->SetVectorParameterValue(BodyTintParameter, Color);
		}
	}
}

void AVaelCharacter::EndDodge()
{
	bIsDodging = false;

	GetMesh()->SetRelativeScale3D(DefaultMeshScale);
	GetCharacterMovement()->bUseControllerDesiredRotation = true;

	if (ActiveRollMontage != nullptr)
	{
		if (UAnimInstance* AnimInstance = GetMesh()->GetAnimInstance())
		{
			AnimInstance->Montage_Stop(RollMontageBlendOutTime, ActiveRollMontage);
		}
		ActiveRollMontage = nullptr;
	}

	// Come out of the roll at no more than walking speed
	UCharacterMovementComponent* Movement = GetCharacterMovement();
	const float ExitSpeed = FMath::Min(CurrentDodgeSpeed, Movement->GetMaxSpeed());
	Movement->Velocity = FVector(DodgeDirection.X * ExitSpeed, DodgeDirection.Y * ExitSpeed, Movement->Velocity.Z);
}

bool AVaelCharacter::IsInvulnerable() const
{
	return bIsDodging || GetWorld()->GetTimeSeconds() < InvulnerableEndTime;
}

void AVaelCharacter::SetInvulnerableFor(float Seconds)
{
	InvulnerableEndTime = FMath::Max(InvulnerableEndTime, GetWorld()->GetTimeSeconds() + Seconds);
}

void AVaelCharacter::OnHealthChanged(float OldValue, float NewValue)
{
	Super::OnHealthChanged(OldValue, NewValue);

	if (bDowned || NewValue >= OldValue)
	{
		return;
	}

	if (NewValue <= 0.0f)
	{
		GoDown();
	}
	else
	{
		// A short moment of grace keeps volleys from hitting several times at once
		SetInvulnerableFor(HitInvulnerability);
	}
}

void AVaelCharacter::GoDown()
{
	bDowned = true;
	ReviveProgress = 0.0f;

	if (bIsDodging)
	{
		EndDodge();
	}

	ElementComponent->ClearQueue();

	// Conditions end, nothing burns on
	UVaelCombatStatics::RemoveStatus(this, EVaelStatus::Wet);
	UVaelCombatStatics::RemoveStatus(this, EVaelStatus::Burning);
	UVaelCombatStatics::RemoveStatus(this, EVaelStatus::Frozen);

	UCharacterMovementComponent* Movement = GetCharacterMovement();
	Movement->StopMovementImmediately();
	Movement->DisableMovement();

	// Placeholder for a fall animation: the body lies flat
	GetMesh()->SetRelativeScale3D(DefaultMeshScale * FVector(1.f, 1.f, 0.25f));

	UE_LOG(LogVael, Log, TEXT("Player %d is down"), GetPlayerNumber());

	if (AVaelGameMode* GameMode = GetWorld()->GetAuthGameMode<AVaelGameMode>())
	{
		GameMode->OnPlayerDowned(this);
	}
}

void AVaelCharacter::Revive(float Health, float InvulnerableSeconds)
{
	GetAbilitySystemComponent()->SetNumericAttributeBase(UVaelAttributeSet::GetHealthAttribute(), FMath::Clamp(Health, 1.0f, GetMaxHealth()));

	if (!bDowned)
	{
		return;
	}

	bDowned = false;
	ReviveProgress = 0.0f;
	SetInvulnerableFor(InvulnerableSeconds);

	GetMesh()->SetRelativeScale3D(DefaultMeshScale);
	GetCharacterMovement()->SetMovementMode(MOVE_Walking);

	UE_LOG(LogVael, Log, TEXT("Player %d is back on their feet"), GetPlayerNumber());
}

void AVaelCharacter::TickRevive(float DeltaSeconds)
{
	// A teammate on their feet next to the downed player helps them up
	bool bHelped = false;
	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		const APlayerController* PlayerController = It->Get();
		const AVaelCharacter* Teammate = PlayerController != nullptr ? PlayerController->GetPawn<AVaelCharacter>() : nullptr;

		if (Teammate != nullptr && Teammate != this && !Teammate->IsDowned() && FVector::Dist2D(Teammate->GetActorLocation(), GetActorLocation()) < ReviveDistance)
		{
			bHelped = true;
			break;
		}
	}

	ReviveProgress = bHelped ? ReviveProgress + DeltaSeconds : FMath::Max(0.0f, ReviveProgress - DeltaSeconds);

	if (ReviveProgress >= ReviveDuration)
	{
		Revive(ReviveHealth, ReviveInvulnerability);
		UVaelNoticeSubsystem::Post(this, FText::Format(NSLOCTEXT("VaelPlayers", "PlayerRevived", "Spieler {0} steht wieder"), GetPlayerNumber()), FText::GetEmpty(), FLinearColor(FColor(159, 224, 168)), 3.0f);
	}
}

int32 AVaelCharacter::GetPlayerNumber() const
{
	const AVaelPlayerController* VaelController = GetController<AVaelPlayerController>();
	return VaelController != nullptr ? VaelController->GetPlayerSlot() + 1 : 1;
}
