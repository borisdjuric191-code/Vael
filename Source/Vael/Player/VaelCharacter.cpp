// Copyright Epic Games, Inc. All Rights Reserved.

#include "Player/VaelCharacter.h"
#include "World/VaelSafeZone.h"
#include "UObject/ConstructorHelpers.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Camera/VaelSharedCamera.h"
#include "Combat/VaelAttributeSet.h"
#include "Combat/VaelCombatStatics.h"
#include "AbilitySystemComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/DecalComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/SkeletalMesh.h"
#include "Player/VaelPoseFollowerAnimInstance.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Floromant/VaelFairyWingsComponent.h"
#include "Floromant/VaelFloromantComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Items/VaelInventory.h"
#include "Items/VaelItemSettings.h"
#include "Items/VaelMaterial.h"
#include "Items/VaelMaterialBag.h"
#include "Magic/VaelAirBlast.h"
#include "Magic/VaelDebrisBurst.h"
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
#include "Player/VaelHeroSubsystem.h"
#include "Player/VaelPlayerController.h"
#include "Save/VaelSaveSubsystem.h"
#include "World/VaelGround.h"
#include "World/VaelRegion.h"
#include "Magic/VaelFormulaAbility.h"
#include "Compendium/VaelCompendiumSettings.h"
#include "UI/VaelCombatTextSubsystem.h"
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

	// The head of a base look is its own part; it follows the bones of the body
	HeadMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("HeadMesh"));
	HeadMesh->SetupAttachment(GetMesh());
	HeadMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	// So are the hands with fingers
	HandMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("HandMesh"));
	HandMesh->SetupAttachment(GetMesh());
	HandMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

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
	MageManaRegen = ManaRegenPerSecond;

	// Create the element queue
	ElementComponent = CreateDefaultSubobject<UVaelElementComponent>(TEXT("ElementQueue"));

	// Create the material bag
	MaterialBag = CreateDefaultSubobject<UVaelMaterialBag>(TEXT("MaterialBag"));

	// Create the equipment and backpack
	Inventory = CreateDefaultSubobject<UVaelInventory>(TEXT("Inventory"));

	// Create the circle of element orbs above the head
	ElementOrbit = CreateDefaultSubobject<UVaelElementOrbitComponent>(TEXT("ElementOrbit"));
	ElementOrbit->SetupAttachment(RootComponent);

	// Create the seeds and plants and the wings of the Floromant; both rest while the player plays a mage
	Floromant = CreateDefaultSubobject<UVaelFloromantComponent>(TEXT("Floromant"));

	Wings = CreateDefaultSubobject<UVaelFairyWingsComponent>(TEXT("Wings"));
	Wings->SetupAttachment(RootComponent);
	Wings->SetVisibility(false, true);

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

void AVaelCharacter::SetBaseLook(int32 LookIndex)
{
	SetHero(HeroClass, LookIndex == 1);
}

void AVaelCharacter::FollowMesh(AActor* Human)
{
	USkeletalMeshComponent* Body = nullptr;
	TInlineComponentArray<USkeletalMeshComponent*> Meshes(Human);
	for (USkeletalMeshComponent* Part : Meshes)
	{
		// The face follows the body by itself; the plain clothes of the assembly make way for the robe
		if (Part->GetFName() == TEXT("Body"))
		{
			Body = Part;
			if (USkeletalMesh* BodyUnderRobe = VaelAssets::LoadOptional(MetaHumanBodyUnderRobe))
			{
				Part->SetSkeletalMesh(BodyUnderRobe);
			}
			Part->SetAnimInstanceClass(UVaelPoseFollowerAnimInstance::StaticClass());
			Part->AddTickPrerequisiteComponent(GetMesh());
			if (UVaelPoseFollowerAnimInstance* Follower = Cast<UVaelPoseFollowerAnimInstance>(Part->GetAnimInstance()))
			{
				Follower->SetSource(GetMesh());
			}
		}
		else if (Part->GetFName() != TEXT("Face"))
		{
			Part->SetVisibility(false);
		}
	}

	for (const TSoftObjectPtr<USkeletalMesh>& Clothing : MetaHumanClothes)
	{
		USkeletalMesh* ClothingMesh = VaelAssets::LoadOptional(Clothing);
		if (Body != nullptr && ClothingMesh != nullptr)
		{
			USkeletalMeshComponent* Worn = NewObject<USkeletalMeshComponent>(Human);
			Worn->SetupAttachment(Body);
			Worn->SetSkeletalMesh(ClothingMesh);
			Worn->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			Worn->RegisterComponent();
			Worn->SetLeaderPoseComponent(Body);
		}
	}
}

void AVaelCharacter::SpawnMetaHumanCopies(int32 Count)
{
	UClass* MetaHumanActorClass = VaelAssets::LoadOptionalClass(MetaHumanClass);
	for (int32 Index = 0; MetaHumanActorClass != nullptr && Index < Count; ++Index)
	{
		FActorSpawnParameters SpawnParameters;
		SpawnParameters.Owner = this;
		SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		const FVector Offset = FRotator(0.0f, 120.0f * Index, 0.0f).RotateVector(FVector(160.0f, 0.0f, 0.0f));
		if (AActor* Copy = GetWorld()->SpawnActor<AActor>(MetaHumanActorClass, GetMesh()->GetComponentTransform(), SpawnParameters))
		{
			Copy->SetActorEnableCollision(false);
			Copy->AttachToComponent(GetMesh(), FAttachmentTransformRules::SnapToTargetNotIncludingScale);
			Copy->SetActorRelativeLocation(Offset);
			FollowMesh(Copy);
		}
	}
}

void AVaelCharacter::SetMetaHumanShown(bool bShown)
{
	if (bShown == (MetaHuman != nullptr))
	{
		return;
	}

	if (!bShown)
	{
		MetaHuman->Destroy();
		MetaHuman = nullptr;
	}
	else if (UClass* MetaHumanActorClass = VaelAssets::LoadOptionalClass(MetaHumanClass))
	{
		FActorSpawnParameters SpawnParameters;
		SpawnParameters.Owner = this;
		SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		MetaHuman = GetWorld()->SpawnActor<AActor>(MetaHumanActorClass, GetMesh()->GetComponentTransform(), SpawnParameters);
	}

	if (MetaHuman != nullptr)
	{
		MetaHuman->SetActorEnableCollision(false);
		MetaHuman->AttachToComponent(GetMesh(), FAttachmentTransformRules::SnapToTargetNotIncludingScale);

		FollowMesh(MetaHuman);
	}

	// Hidden, the base look must keep animating: the sockets and the MetaHuman hang on its bones
	const bool bMetaHumanShown = MetaHuman != nullptr;
	GetMesh()->SetVisibility(!bMetaHumanShown, false);
	HeadMesh->SetVisibility(!bMetaHumanShown, false);
	HandMesh->SetVisibility(!bMetaHumanShown, false);
	GetMesh()->VisibilityBasedAnimTickOption = bMetaHumanShown ? EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones : GetClass()->GetDefaultObject<AVaelCharacter>()->GetMesh()->VisibilityBasedAnimTickOption;
}

void AVaelCharacter::SetHero(EVaelHeroClass InClass, bool bFemale)
{
	const int32 LookIndex = bFemale ? 1 : 0;
	const bool bFloromant = InClass == EVaelHeroClass::Floromant;
	const UVaelFloromantSettings* FloromantSettings = UVaelFloromantSettings::Get();

	// Body and head of the class; while they are missing the body stays as it is
	USkeletalMesh* Body = VaelAssets::LoadOptional(bFloromant ? FloromantSettings->BodyMeshes[LookIndex] : BodyMeshes[LookIndex]);
	USkeletalMesh* Head = VaelAssets::LoadOptional(bFloromant ? FloromantSettings->HeadMeshes[LookIndex] : HeadMeshes[LookIndex]);
	if (Body != nullptr && Head != nullptr)
	{
		GetMesh()->SetSkeletalMesh(Body);
		HeadMesh->SetSkeletalMesh(Head);
		HeadMesh->SetLeaderPoseComponent(GetMesh());

		// The Floromant has no hand part yet: its hands are on the body
		USkeletalMesh* Hands = bFloromant ? nullptr : VaelAssets::LoadOptional(HandMeshes[LookIndex]);
		HandMesh->SetSkeletalMesh(Hands);
		HandMesh->SetLeaderPoseComponent(Hands != nullptr ? GetMesh() : nullptr);
		BaseLook = LookIndex;

		if (bHasPlayerColor)
		{
			SetPlayerColor(PlayerColor);
		}
	}

	if (bHeroSet && HeroClass == InClass)
	{
		return;
	}

	// A new class: nothing queued or grown stays, the wings come or go, Saft or mana fills up
	if (bHeroSet && HeroClass == EVaelHeroClass::Floromant)
	{
		Floromant->WitherAll();
	}

	HeroClass = InClass;
	bHeroSet = true;

	ElementComponent->ClearQueue();
	ElementOrbit->SetVisibility(!bFloromant, true);
	Wings->SetWingsShown(bFloromant);

	UAbilitySystemComponent* AbilitySystem = GetAbilitySystemComponent();
	const float MaxMana = bFloromant ? FloromantSettings->MaxSaft : StartingMana;
	AbilitySystem->SetNumericAttributeBase(UVaelAttributeSet::GetMaxManaAttribute(), MaxMana);
	AbilitySystem->SetNumericAttributeBase(UVaelAttributeSet::GetManaAttribute(), MaxMana);
	SetManaRegen(bFloromant ? FloromantSettings->SaftPerSecond : MageManaRegen);

	UE_LOG(LogVael, Log, TEXT("'%s' plays the %s"), *GetNameSafe(this), bFloromant ? TEXT("Floromant") : TEXT("mage"));
}

void AVaelCharacter::BeginPlay()
{
	Super::BeginPlay();

	// The animations of the mage take over from the template ones as soon as they are made in the editor
	if (UClass* AnimClass = VaelAssets::LoadOptionalClass(MageAnimClass))
	{
		GetMesh()->SetAnimInstanceClass(AnimClass);
	}

	if (!bHeroSet)
	{
		SetHero(EVaelHeroClass::Mage, false);
	}

	LoadedDodgeMontage = VaelAssets::LoadOptional(DodgeMontage);
	LoadedSpellDashMontage = VaelAssets::LoadOptional(SpellDashMontage);

	DefaultMeshScale = GetMesh()->GetRelativeScale3D();
	DefaultMeshLocation = GetMesh()->GetRelativeLocation();
	DefaultWalkSpeed = GetCharacterMovement()->MaxWalkSpeed;

	// Every player starts with a few healing potions in the bag
	const UVaelItemSettings* ItemSettings = UVaelItemSettings::Get();
	if (const UVaelMaterial* Potion = ItemSettings->StartingPotion.LoadSynchronous())
	{
		MaterialBag->AddMaterial(Potion, ItemSettings->StartingPotionCount);
	}

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

	RestoreFromSave();
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
	UpdateHover(DeltaSeconds);

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
	const float Burden = GetWorld()->GetTimeSeconds() < BurdenEndTime ? BurdenMultiplier : 1.0f;
	const float Bracing = IsFloromant() ? Floromant->GetMoveSpeedScale() : 1.0f;
	GetCharacterMovement()->MaxWalkSpeed = bObserving ? 0.0f : DefaultWalkSpeed * (bChanneling ? UVaelMagicSettings::Get()->ChannelMoveSpeedMultiplier : 1.0f) * Burden * Bracing;

	if (bObserving)
	{
		UpdateFocus();
	}

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

	// Class and body the slot has chosen on the page "Gestalt"
	const AVaelPlayerController* LookOwner = Cast<AVaelPlayerController>(NewController);
	if (const UVaelHeroSubsystem* Heroes = LookOwner != nullptr ? UVaelHeroSubsystem::Get(this) : nullptr)
	{
		const FVaelHeroChoice Choice = Heroes->GetChoice(LookOwner->GetPlayerSlot());
		SetHero(Choice.Class, Choice.bFemale);
	}

	const AVaelPlayerController* VaelController = Cast<AVaelPlayerController>(NewController);
	const AVaelGameMode* GameMode = GetWorld()->GetAuthGameMode<AVaelGameMode>();

	if (VaelController != nullptr && GameMode != nullptr)
	{
		SetPlayerColor(GameMode->GetPlayerColor(VaelController->GetPlayerSlot()));

		UE_LOG(LogVael, Log, TEXT("Player slot %d controls '%s'"), VaelController->GetPlayerSlot(), *GetNameSafe(this));
	}

	RestoreFromSave();
}

void AVaelCharacter::RestoreFromSave()
{
	// Once, as soon as the character is set up and it is known which slot plays it; that order differs between the first player and those who join
	const AVaelPlayerController* VaelController = GetController<AVaelPlayerController>();
	const UVaelSaveSubsystem* Save = UVaelSaveSubsystem::Get(this);
	if (bRestoredFromSave || !HasActorBegunPlay() || VaelController == nullptr || Save == nullptr)
	{
		return;
	}

	bRestoredFromSave = true;
	Save->RestorePlayer(this, VaelController->GetPlayerSlot());
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

	// Riding on the wind: streaks wrap the body, shimmer and dust are left along the path
	if (InDashBurst.Element == EVaelElement::Air)
	{
		AVaelAirBlast::SpawnTrail(this, Duration);
	}

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

void AVaelCharacter::UpdateHover(float DeltaSeconds)
{
	if (!IsFloromant() || bDowned)
	{
		FlightPose = FVaelFlightPose();
		if (!bDowned && !GetMesh()->GetRelativeLocation().Equals(DefaultMeshLocation))
		{
			GetMesh()->SetRelativeLocation(DefaultMeshLocation);
		}
		if (FlightShadow != nullptr && FlightShadow->IsVisible())
		{
			FlightShadow->SetVisibility(false);
		}
		return;
	}

	const UVaelFloromantSettings* Settings = UVaelFloromantSettings::Get();
	const float Time = GetWorld()->GetTimeSeconds();
	// A hitch must not throw the leg pendulum out of its swing
	const float Delta = FMath::Clamp(DeltaSeconds, 0.001f, 1.0f / 30.0f);

	// How fast she flies and how her speed changes; speeding up, braking and turning all tip her body
	const FVector Velocity = GetVelocity() * FVector(1.0f, 1.0f, 0.0f);
	const float TopSpeed = FMath::Max(DefaultWalkSpeed, 1.0f);
	FlightAccel = FMath::VInterpTo(FlightAccel, (Velocity - FlightLastVelocity) / Delta, Delta, 6.0f);
	FlightLastVelocity = Velocity;
	FlightSpeedShare = FMath::FInterpTo(FlightSpeedShare, FMath::Min(Velocity.Size() / TopSpeed, 1.0f), Delta, 4.0f);
	const float Brake = Velocity.IsNearlyZero(1.0f) && FlightAccel.IsNearlyZero(50.0f) ? 0.0f
		: FMath::Clamp(-(FlightAccel | Velocity.GetSafeNormal()) / Settings->FlightAccelReference, 0.0f, 1.0f);

	// While a hand fires she lands and braces herself
	const float Landed = FMath::Max(Floromant->GetStance(true), Floromant->GetStance(false));
	const float Flying = 1.0f - Landed;

	// A little above the ground, rising and sinking slowly, higher when she flies fast, high up on R3; soaked wings carry her lower.
	// Collision and walking stay as for every class
	const float Rise = Floromant->GetRise();
	const float Soaked = Floromant->GetWingWetness();
	const float Bob = FMath::Sin(Time * Settings->HoverBobSpeed * 2.0f * UE_PI + GetUniqueID()) * Settings->HoverBob * (1.0f - 0.6f * FlightSpeedShare);
	const float Height = (Settings->HoverHeight + Bob + Settings->FlightRise * FlightSpeedShare + Settings->RiseHeight * FMath::SmoothStep(0.0f, 1.0f, Rise))
		* (1.0f - Settings->SoakedHeightLoss * Soaked) * Flying;

	// The wind pushes her about in the air, harder up high; on the ground she stands against it
	const FVector Wind = AVaelRegion::GetWindAt(GetWorld(), GetActorLocation()) * FVector(1.0f, 1.0f, 0.0f);
	const float WindShare = FMath::Min(Wind.Size() / Settings->WindReference, 1.5f) * Flying;
	if (!Wind.IsNearlyZero() && Flying > 0.0f && !bIsDodging)
	{
		AddActorWorldOffset(Wind * Settings->WindDrift * (1.0f + Rise) * Flying * Delta, true);
	}

	// The body tips into the way she flies and further while she speeds up or turns, and leans into the wind; the lean is a
	// horizontal vector, its length the degrees
	FVector TargetLean = Velocity / TopSpeed * Settings->FlightLean + FlightAccel / Settings->FlightAccelReference * Settings->FlightAccelLean
		- Wind.GetSafeNormal() * Settings->WindLean * FMath::Min(WindShare, 1.0f);
	TargetLean = TargetLean.GetClampedToMaxSize(Settings->FlightMaxLean) * Flying;
	FlightLean = FMath::VInterpTo(FlightLean, TargetLean, Delta, Settings->FlightLeanSpeed);

	// The legs hang like a pendulum: they lag behind the tipping body and swing past before they settle
	const FVector LegPull = (FlightLean - FlightLegLag) * Settings->FlightLegStiffness - FlightLegLagSpeed * Settings->FlightLegDamping;
	FlightLegLagSpeed += LegPull * Delta;
	FlightLegLag += FlightLegLagSpeed * Delta;

	// Worked out in the world, handed on in the space of the mesh (animation) and of the actor (wings)
	const FVector LeanAxis = FVector::CrossProduct(FVector::UpVector, FlightLean);
	const FQuat Tilt = LeanAxis.IsNearlyZero(0.001f) ? FQuat::Identity : FQuat(LeanAxis.GetSafeNormal(), FMath::DegreesToRadians(FlightLean.Size()));
	const FQuat MeshRotation = GetMesh()->GetComponentQuat();
	const FQuat ActorRotation = GetActorQuat();

	FlightPose.Weight = Flying;
	FlightPose.Tilt = MeshRotation.Inverse() * Tilt * MeshRotation;
	const FVector LegSwing = (FlightLean - FlightLegLag) - Velocity / TopSpeed * Settings->FlightLegTrail * Flying;
	FlightPose.LegSwing = MeshRotation.UnrotateVector(LegSwing);
	// Slow paddling of the legs in turn while she hovers, quieter in fast flight when they trail behind
	const float Paddle = Settings->FlightLegKick * Flying * (1.0f - 0.5f * FlightSpeedShare);
	const float PaddlePhase = Time * 0.65f * 2.0f * UE_PI + GetUniqueID() * 0.7f;
	FlightPose.LegKick[0] = Paddle * FMath::Sin(PaddlePhase);
	FlightPose.LegKick[1] = -Paddle * FMath::Sin(PaddlePhase + 0.4f);
	FlightPose.KneeBend = Settings->FlightKneeBend * Flying * (1.0f + 0.25f * FMath::Sin(PaddlePhase * 0.7f) + 0.4f * FlightSpeedShare);
	FlightPose.FootPoint = Settings->FlightFootPoint * Flying;

	GetMesh()->SetRelativeLocation(DefaultMeshLocation + FVector(0.0f, 0.0f, Height));

	// The wings sit on her back, so they tip with the body around the pelvis
	FVector Pivot(0.0f, 0.0f, Height);
	if (GetMesh()->DoesSocketExist(TEXT("pelvis")))
	{
		Pivot = GetMesh()->GetSocketTransform(TEXT("pelvis"), RTS_Actor).GetLocation();
	}
	const FQuat WingTilt = ActorRotation.Inverse() * Tilt * ActorRotation;
	Wings->SetRelativeLocationAndRotation(Pivot + WingTilt.RotateVector(FVector(0.0f, 0.0f, Height) - Pivot), WingTilt);
	Wings->SetFlight(FlightSpeedShare * Flying, Brake * Flying, Soaked, WindShare);

	UpdateFlightShadow(Height, Landed);
}

void AVaelCharacter::UpdateFlightShadow(float Height, float Landed)
{
	const UVaelFloromantSettings* Settings = UVaelFloromantSettings::Get();
	if (FlightShadow == nullptr)
	{
		UMaterialInterface* Material = VaelAssets::LoadOptional(Settings->ShadowMaterial);
		if (Material == nullptr)
		{
			return;
		}

		FlightShadowMaterial = UMaterialInstanceDynamic::Create(Material, this);
		FlightShadow = NewObject<UDecalComponent>(this, TEXT("FlightShadow"));
		FlightShadow->SetDecalMaterial(FlightShadowMaterial);
		FlightShadow->DecalSize = FVector(30.0f, Settings->ShadowRadius, Settings->ShadowRadius);
		FlightShadow->SetupAttachment(RootComponent);
		FlightShadow->SetUsingAbsoluteRotation(true);
		FlightShadow->SetUsingAbsoluteScale(true);
		FlightShadow->RegisterComponent();
		FlightShadow->SetWorldRotation(FRotator(-90.0f, 0.0f, 0.0f));
	}

	// Under the feet, a box that reaches a little into the ground and not up to the dangling legs; wider and fainter the higher she flies
	const float Feet = GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
	const float Widen = 1.0f + Height / 120.0f;
	FlightShadow->SetWorldScale3D(FVector(1.0f, Widen, Widen));
	FlightShadow->SetRelativeLocation(FVector(0.0f, 0.0f, -Feet - 10.0f));
	FlightShadow->SetVisibility(true);
	FlightShadowMaterial->SetScalarParameterValue(TEXT("Strength"), Settings->ShadowStrength * (1.0f - Height / 250.0f) * (1.0f - 0.5f * Landed));
}

void AVaelCharacter::BeginRollLook(UAnimMontage* Montage, float Duration)
{
	// The body rolls the way it moves; it turns back to the aim direction afterwards
	GetCharacterMovement()->bUseControllerDesiredRotation = false;
	SetActorRotation(DodgeDirection.Rotation());

	ActiveRollMontage = nullptr;

	// A Floromant doesn't roll: a quick beat of the wings carries it aside in a cloud of pink and turquoise petals
	if (IsFloromant())
	{
		Wings->Burst(Duration + 0.15f);
		for (const FLinearColor& Color : { FLinearColor(0.82f, 0.4f, 0.6f), FLinearColor(0.31f, 0.7f, 0.66f) })
		{
			FVaelDebris Petals;
			Petals.Count = 10;
			Petals.Spread = 30.0f;
			Petals.Speed = 160.0f;
			Petals.Lift = 140.0f;
			Petals.Gravity = 120.0f;
			Petals.Drag = 2.5f;
			Petals.Lifetime = 0.9f;
			Petals.MinSize = 3.0f;
			Petals.MaxSize = 6.0f;
			Petals.Color = Color;
			Petals.Glow = 1.5f;
			Petals.bSoft = true;
			Petals.bLandOnGround = false;
			AVaelDebrisBurst::Spawn(this, GetActorLocation() - DodgeDirection * 40.0f, Petals);
		}
		return;
	}

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
	PlayerColor = Color;
	bHasPlayerColor = true;

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

void AVaelCharacter::PlayCastGesture(EVaelCastGesture Gesture)
{
	CastGesture = Gesture;
	CastGestureStartTime = GetWorld()->GetTimeSeconds();
}

EVaelCastGesture AVaelCharacter::GetCastGesture(float& OutSeconds) const
{
	OutSeconds = GetWorld()->GetTimeSeconds() - CastGestureStartTime;
	return OutSeconds < UVaelMagicSettings::Get()->GetGestureDuration(CastGesture) ? CastGesture : EVaelCastGesture::None;
}

bool AVaelCharacter::GetGestureFingertips(FVector& OutLocation) const
{
	const USkeletalMeshComponent* Body = GetMesh();
	if (Body->GetBoneIndex(TEXT("index_03_r")) == INDEX_NONE || Body->GetBoneIndex(TEXT("middle_03_r")) == INDEX_NONE)
	{
		return false;
	}

	// The last finger bones start at the outer knuckle: a little further along the fingers lie the tips
	const FVector Knuckles = (Body->GetBoneLocation(TEXT("index_03_r")) + Body->GetBoneLocation(TEXT("middle_03_r"))) * 0.5f;
	const FVector Along = (Knuckles - (Body->GetBoneLocation(TEXT("index_02_r")) + Body->GetBoneLocation(TEXT("middle_02_r"))) * 0.5f).GetSafeNormal();
	OutLocation = Knuckles + Along * 4.0f;
	return true;
}

FVector AVaelCharacter::GetMouthLocation() const
{
	const USkeletalMeshComponent* Body = GetMesh();
	if (Body->GetBoneIndex(TEXT("head")) == INDEX_NONE)
	{
		return GetActorLocation() + FVector(0.0f, 0.0f, GetSimpleCollisionHalfHeight());
	}

	// Measured on the mage models: the lips lie about 9 cm in front of the head bone and 7 cm below it
	return Body->GetBoneLocation(TEXT("head")) + GetActorForwardVector() * 9.0f - FVector(0.0f, 0.0f, 7.0f) * Body->GetComponentScale().Z;
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

void AVaelCharacter::AddReviveProgress(float Seconds)
{
	if (bDowned)
	{
		ReviveProgress += Seconds;
	}
}

void AVaelCharacter::AddHeartShield(float Amount, float Seconds)
{
	HeartShieldLeft = FMath::Max(HeartShieldLeft, Amount);
	HeartShieldEndTime = GetWorld()->GetTimeSeconds() + Seconds;
}

float AVaelCharacter::AbsorbIncoming(float Damage)
{
	if (HeartShieldLeft <= 0.0f || GetWorld()->GetTimeSeconds() > HeartShieldEndTime)
	{
		HeartShieldLeft = 0.0f;
		return Damage;
	}
	const float Swallowed = FMath::Min(Damage, HeartShieldLeft);
	HeartShieldLeft -= Swallowed;
	return Damage - Swallowed;
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

void AVaelCharacter::SetObserving(bool bInObserving)
{
	bObserving = bInObserving && !bDowned;
	UpdateFocus();
}

void AVaelCharacter::UpdateFocus()
{
	// The mouse points at the spot, the stick at a spot at the reach of the glass
	const UVaelCompendiumSettings* Settings = UVaelCompendiumSettings::Get();
	const FVector Feet = GetActorLocation() - FVector(0.0f, 0.0f, GetSimpleCollisionHalfHeight());
	const AVaelPlayerController* PlayerController = Cast<AVaelPlayerController>(GetController());

	FVector MouseLocation;
	if (PlayerController != nullptr && PlayerController->GetMouseAimLocation(MouseLocation))
	{
		const FVector Offset = (MouseLocation - Feet) * FVector(1.0f, 1.0f, 0.0f);
		FocusLocation = Feet + Offset.GetClampedToMaxSize(Settings->SpyglassReach);
	}
	else
	{
		FocusLocation = Feet + UVaelFormulaAbility::GetAimDirection(this) * Settings->SpyglassReach;
	}

	FHitResult GroundHit;
	if (VaelGround::TraceGround(GetWorld(), FocusLocation + FVector(0.0f, 0.0f, 300.0f), FocusLocation - FVector(0.0f, 0.0f, 600.0f), GroundHit, this))
	{
		FocusLocation = GroundHit.Location;
	}
}

float AVaelCharacter::GetIncomingDamageMultiplier(const FGameplayTagContainer& DamageTags) const
{
	// Nothing hurts players in a safe zone like the camp
	return AVaelSafeZone::IsSafe(GetWorld(), GetActorLocation()) ? 0.0f : Super::GetIncomingDamageMultiplier(DamageTags);
}

bool AVaelCharacter::IsAboveGround() const
{
	return IsFloromant() && !bDowned && Floromant->IsUpHigh();
}

void AVaelCharacter::ApplyKnockback(const FVector& Direction, float Speed)
{
	// A rooted Floromant stands anchored in the earth
	if (!(IsFloromant() && Floromant->IsRooted()))
	{
		Super::ApplyKnockback(Direction, Speed);
	}
}

void AVaelCharacter::ApplyPull(const FVector& Location, float Speed, float DeltaSeconds)
{
	if (!(IsFloromant() && Floromant->IsRooted()))
	{
		Super::ApplyPull(Location, Speed, DeltaSeconds);
	}
}

int32 AVaelCharacter::GetPotionCount() const
{
	int32 Count = 0;
	for (const FVaelMaterialStack& Stack : MaterialBag->GetStacks())
	{
		Count += Stack.Material != nullptr && Stack.Material->IsPotion() ? Stack.Count : 0;
	}
	return Count;
}

bool AVaelCharacter::DrinkPotion()
{
	if (bDowned || GetHealth() >= GetMaxHealth() || GetWorld()->GetTimeSeconds() < NextPotionTime)
	{
		return false;
	}

	// The weakest potion first, the strong ones are kept for later
	const UVaelMaterial* Potion = nullptr;
	for (const FVaelMaterialStack& Stack : MaterialBag->GetStacks())
	{
		if (Stack.Material != nullptr && Stack.Material->IsPotion() && Stack.Count > 0 && (Potion == nullptr || Stack.Material->HealFraction < Potion->HealFraction))
		{
			Potion = Stack.Material;
		}
	}

	if (Potion == nullptr)
	{
		UVaelCombatTextSubsystem::PostPickup(this, NSLOCTEXT("VaelItems", "NoPotion", "Kein Heiltrank"), FLinearColor(FColor(158, 143, 125)));
		NextPotionTime = GetWorld()->GetTimeSeconds() + 0.6f;
		return false;
	}

	const float Amount = GetMaxHealth() * Potion->HealFraction;
	MaterialBag->RemoveMaterial(Potion, 1);
	UVaelCombatStatics::Heal(this, Amount);
	UVaelCombatTextSubsystem::PostPickup(this, FText::Format(NSLOCTEXT("VaelItems", "PotionHeal", "+{0} Leben"), FText::AsNumber(FMath::RoundToInt(Amount))), FLinearColor(FColor(120, 230, 120)));
	NextPotionTime = GetWorld()->GetTimeSeconds() + UVaelItemSettings::Get()->PotionCooldown;

	// Then it keeps healing a little for a while; a new potion starts the regeneration over
	if (Potion->RegenFraction > 0.0f && Potion->RegenSeconds > 0.0f)
	{
		PotionRegenPerSecond = GetMaxHealth() * Potion->RegenFraction / Potion->RegenSeconds;
		PotionRegenEndTime = GetWorld()->GetTimeSeconds() + Potion->RegenSeconds;
		GetWorldTimerManager().SetTimer(PotionRegenTimer, this, &AVaelCharacter::TickPotionRegen, 1.0f, true);
	}

	UE_LOG(LogVael, Log, TEXT("'%s' drinks %s, %d left"), *GetNameSafe(this), *Potion->DisplayName.ToString(), GetPotionCount());
	return true;
}

void AVaelCharacter::TickPotionRegen()
{
	if (GetWorld()->GetTimeSeconds() > PotionRegenEndTime || bDowned)
	{
		GetWorldTimerManager().ClearTimer(PotionRegenTimer);
		return;
	}

	if (GetHealth() < GetMaxHealth())
	{
		UVaelCombatStatics::Heal(this, PotionRegenPerSecond);
	}
}

float AVaelCharacter::GetExperienceFraction() const
{
	return FMath::Clamp(Experience / GetExperienceForNextLevel(), 0.0f, 1.0f);
}

void AVaelCharacter::AddExperience(float Amount)
{
	Experience += FMath::Max(Amount, 0.0f);
	while (Experience >= GetExperienceForNextLevel())
	{
		Experience -= GetExperienceForNextLevel();
		++Level;
		UVaelNoticeSubsystem::Post(this, FText::Format(NSLOCTEXT("VaelPlayers", "LevelUp", "Spieler {0} erreicht Stufe {1}"), GetPlayerNumber(), Level), FText::GetEmpty(), FLinearColor(FColor(232, 205, 140)), 3.0f);
	}
}

float AVaelCharacter::GetDodgeCooldownFraction() const
{
	const float Left = NextDodgeTime - GetWorld()->GetTimeSeconds();
	return DodgeCooldown > 0.0f ? FMath::Clamp(Left / DodgeCooldown, 0.0f, 1.0f) : 0.0f;
}

void AVaelCharacter::SetBurden(float SpeedMultiplier, float Duration)
{
	BurdenMultiplier = FMath::Clamp(SpeedMultiplier, 0.0f, 1.0f);
	BurdenEndTime = GetWorld()->GetTimeSeconds() + Duration;
}
