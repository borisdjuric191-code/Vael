// Copyright Epic Games, Inc. All Rights Reserved.

#include "Creatures/VaelSickleDoll.h"
#include "AnimationRuntime.h"
#include "Components/CapsuleComponent.h"
#include "Components/PoseableMeshComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Creatures/VaelCreatureData.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Magic/VaelDebrisBurst.h"
#include "Magic/VaelGameplayTags.h"
#include "Magic/VaelSpellEffects.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Player/VaelCharacter.h"
#include "UI/VaelCombatTextSubsystem.h"
#include "Vael.h"

#define LOCTEXT_NAMESPACE "VaelCreatures"

namespace
{
	/** The doll stops walking home closer than this, in cm */
	constexpr float DollHomeTolerance = 140.0f;

	/** Diameter of the engine sphere and cylinder, height of the engine cone in cm */
	constexpr float DollShapeSize = 100.0f;

	/** Degrees per second the whirling doll turns */
	constexpr float DollSpinRate = 1100.0f;

	/** A collapsed doll is squashed to this share of its height */
	constexpr float DollCollapsedHeight = 0.3f;

	/** Sickle steel, the eyes the Mark lit in it, the ring of its reach, a part flashing under a hit */
	const FLinearColor DollSteelColor(0.42f, 0.42f, 0.46f);
	const FLinearColor DollEyeColor(0.6f, 0.15f, 1.0f);
	const FLinearColor DollReachColor(1.0f, 0.35f, 0.15f);
	const FLinearColor DollFlashColor(1.0f, 0.9f, 0.75f);

	/** Lays a stretched shape between two points of the rig */
	void PlaceDollSegment(UStaticMeshComponent* Segment, const FVector& Start, const FVector& End, const FVector2D& Width)
	{
		Segment->SetRelativeLocation((Start + End) * 0.5f);
		Segment->SetRelativeRotation((End - Start).Rotation());
		Segment->SetRelativeScale3D(FVector(FVector::Dist(Start, End) * 1.1f, Width.X, Width.Y) / DollShapeSize);
	}
}

AVaelSickleDoll::AVaelSickleDoll()
{
}

TSubclassOf<UVaelCreatureData> AVaelSickleDoll::GetDefaultDataClass() const
{
	return UVaelSickleDollData::StaticClass();
}

void AVaelSickleDoll::BeginPlay()
{
	Super::BeginPlay();

	WhirlCooldown = RandomInRange(GetData<UVaelSickleDollData>()->WhirlInterval) * 0.5f;

	BuildRig();
}

float AVaelSickleDoll::GetIncomingDamageMultiplier(const FGameplayTagContainer& DamageTags) const
{
	const UVaelSickleDollData* Data = GetData<UVaelSickleDollData>();
	const float Multiplier = Super::GetIncomingDamageMultiplier(DamageTags);

	switch (State)
	{
	case EVaelDollState::Dazed:
		return Multiplier * Data->DazedDamageMultiplier;
	case EVaelDollState::Soaked:
		return Multiplier * Data->SoakedDamageMultiplier;
	default:
		return Multiplier;
	}
}

void AVaelSickleDoll::EnterState(EVaelDollState NewState)
{
	State = NewState;
	StateTime = 0.0f;

	if (State == EVaelDollState::Whirl)
	{
		++WhirlCount;
		CutCountdown = 0.0f;
	}

	// Wet straw is darker
	RefreshBodyColor();
}

void AVaelSickleDoll::OnDamageTaken(float Damage, const FGameplayTagContainer& DamageTags)
{
	Super::OnDamageTaken(Damage, DamageTags);

	// A hit of water lets it collapse; rain alone doesn't
	if (DamageTags.HasTagExact(VaelTags::Element_Water) && !IsDead() && State != EVaelDollState::Soaked)
	{
		UE_LOG(LogVael, Verbose, TEXT("'%s' is soaked and collapses"), *GetNameSafe(this));

		EnterState(EVaelDollState::Soaked);
		GetCharacterMovement()->StopMovementImmediately();
		UVaelCombatTextSubsystem::PostReaction(this, LOCTEXT("DollSoaked", "Durchnässt!"));
	}
}

void AVaelSickleDoll::TickBehavior(float DeltaSeconds)
{
	const UVaelSickleDollData* Data = GetData<UVaelSickleDollData>();

	StateTime += DeltaSeconds;

	switch (State)
	{
	case EVaelDollState::Lurk:
	{
		float TargetDistance = 0.0f;
		AActor* Target = FindTarget(Data->AggroRange, &TargetDistance);

		if (Target == nullptr)
		{
			if (FVector::Dist2D(GetActorLocation(), HomeLocation) > DollHomeTolerance)
			{
				MoveTowards(HomeLocation, Data->MoveSpeed * 0.5f);
			}
			break;
		}

		// A few quick steps, then it stands stock-still
		if (FMath::Fmod(StateTime, Data->StepTime + Data->PauseTime) < Data->StepTime)
		{
			MoveTowards(Target->GetActorLocation(), Data->MoveSpeed);
		}
		else
		{
			FaceTowards(Target->GetActorLocation());
		}

		WhirlCooldown -= DeltaSeconds;
		if (WhirlCooldown <= 0.0f && TargetDistance < Data->WhirlRange && !IsBlinded())
		{
			EnterState(EVaelDollState::Windup);
		}
		break;
	}

	case EVaelDollState::Windup:
		if (StateTime >= Data->WindupTime)
		{
			EnterState(EVaelDollState::Whirl);
		}
		break;

	case EVaelDollState::Whirl:
	{
		if (AActor* Target = FindTarget(Data->AggroRange * 1.5f))
		{
			MoveTowards(Target->GetActorLocation(), Data->WhirlSpeed);
		}

		CutCountdown -= DeltaSeconds;
		if (CutCountdown <= 0.0f)
		{
			CutCountdown = Data->CutInterval;

			ForEachActivePlayer([this, Data](AVaelCharacter* Player)
			{
				if (GetDistanceTo2D(Player) < Data->WhirlRadius + Player->GetCapsuleComponent()->GetScaledCapsuleRadius())
				{
					HitPlayer(Player, Data->CutDamage, EVaelElement::Earth);
				}
			});
		}

		if (StateTime >= Data->WhirlTime)
		{
			EnterState(EVaelDollState::Dazed);
		}
		break;
	}

	case EVaelDollState::Dazed:
		if (StateTime >= Data->DazedTime)
		{
			WhirlCooldown = RandomInRange(Data->WhirlInterval);
			EnterState(EVaelDollState::Lurk);
		}
		break;

	case EVaelDollState::Soaked:
		if (StateTime >= Data->SoakedTime)
		{
			WhirlCooldown = RandomInRange(Data->WhirlInterval);
			EnterState(EVaelDollState::Lurk);
		}
		break;
	}
}

UStaticMeshComponent* AVaelSickleDoll::AddRigPart(const FLinearColor& Color, float Glow, const TCHAR* MeshPath)
{
	UMaterialInterface* CoreMaterial = nullptr;
	UMaterialInterface* GlowMaterial = nullptr;
	VaelEffects::LoadLookMaterials(CoreMaterial, GlowMaterial);

	UStaticMeshComponent* Part = VaelEffects::AddLookShape(this, CoreMaterial, Color, Glow);
	Part->AttachToComponent(Rig, FAttachmentTransformRules::KeepRelativeTransform);
	Part->SetCastShadow(true);
	Part->SetVisibility(true);

	if (MeshPath != nullptr)
	{
		Part->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, MeshPath));
	}

	return Part;
}

void AVaelSickleDoll::BuildShapes()
{
	const float HalfHeight = GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight();

	// A skirt of bound straw, a chest, a head with two eyes the Mark lit
	Skirt = AddRigPart(StrawColor, 0.0f, TEXT("/Engine/BasicShapes/Cone.Cone"));
	Skirt->SetRelativeLocation(FVector(0.0f, 0.0f, -HalfHeight + 48.0f));
	Skirt->SetRelativeScale3D(FVector(88.0f, 88.0f, 96.0f) / DollShapeSize);

	Chest = AddRigPart(StrawColor);
	Chest->SetRelativeLocation(FVector(0.0f, 0.0f, 22.0f));
	Chest->SetRelativeScale3D(FVector(44.0f, 40.0f, 52.0f) / DollShapeSize);

	Head = AddRigPart(StrawColor);
	Head->SetRelativeScale3D(FVector(30.0f, 28.0f, 32.0f) / DollShapeSize);

	for (int32 Index = 0; Index < 2; ++Index)
	{
		Eyes.Add(AddRigPart(DollEyeColor, 6.0f));
		Arms.Add(AddRigPart(StrawColor));
		Sickles.Add(AddRigPart(DollSteelColor));
	}
}

void AVaelSickleDoll::BuildRig()
{
	const float HalfHeight = GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight();

	// The placeholder body gives way to the rig; its colors still arrive through OnBodyColorShown
	GetBody()->SetVisibility(false);

	Rig = NewObject<USceneComponent>(this, TEXT("Rig"));
	Rig->SetupAttachment(GetRootComponent());
	Rig->RegisterComponent();

	if (GetFactoryMesh() != nullptr)
	{
		SetupFactoryArms();
	}
	else
	{
		BuildShapes();
	}

	// The reach of the sickles lies on the ground and doesn't turn with the doll
	UMaterialInterface* CoreMaterial = nullptr;
	UMaterialInterface* GlowMaterial = nullptr;
	VaelEffects::LoadLookMaterials(CoreMaterial, GlowMaterial);

	const float Reach = GetData<UVaelSickleDollData>()->WhirlRadius * 2.0f;
	ReachDisc = VaelEffects::AddLookShape(this, GlowMaterial, DollReachColor, 1.5f);
	ReachDisc->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cylinder.Cylinder")));
	ReachDisc->SetRelativeLocation(FVector(0.0f, 0.0f, -HalfHeight + 2.0f));
	ReachDisc->SetRelativeScale3D(FVector(Reach, Reach, 2.0f) / DollShapeSize);

	RefreshBodyColor();
}

void AVaelSickleDoll::OnBodyColorShown(const FLinearColor& Color, bool bHitFlash)
{
	StrawColor = Color;

	if (Skirt == nullptr)
	{
		return;
	}

	const FLinearColor Straw = State == EVaelDollState::Soaked ? Color * 0.55f : Color;

	const auto Paint = [bHitFlash](UStaticMeshComponent* Part, const FLinearColor& PartColor)
	{
		if (UMaterialInstanceDynamic* Material = Cast<UMaterialInstanceDynamic>(Part->GetMaterial(0)))
		{
			Material->SetVectorParameterValue(VaelEffects::LookColorParameter, bHitFlash ? DollFlashColor : PartColor);
		}
	};

	Paint(Skirt, Straw);
	Paint(Chest, Straw * 1.25f);
	Paint(Head, Straw * 1.4f);

	for (UStaticMeshComponent* Arm : Arms)
	{
		Paint(Arm, Straw * 1.25f);
	}

	for (UStaticMeshComponent* Sickle : Sickles)
	{
		Paint(Sickle, DollSteelColor);
	}
}

void AVaelSickleDoll::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (Rig == nullptr)
	{
		return;
	}

	const UVaelSickleDollData* Data = GetData<UVaelSickleDollData>();
	const float HalfHeight = GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight();
	const float Time = GetWorld()->GetTimeSeconds();
	const bool bDeadNow = IsDead();

	const bool bWhirling = State == EVaelDollState::Whirl && !bDeadNow;
	const bool bWindup = State == EVaelDollState::Windup && !bDeadNow;
	const bool bDown = State == EVaelDollState::Soaked || bDeadNow;

	// The ring warns while the sickles rise and stays, dimmer, while they cut
	ReachDisc->SetVisibility(bWindup || bWhirling);
	VaelEffects::SetLookGlow(ReachDisc, bWindup ? 1.2f + 1.5f * FMath::Abs(FMath::Sin(Time * 14.0f)) : 0.7f);

	SpinDegrees = bWhirling ? FMath::Fmod(SpinDegrees + DollSpinRate * DeltaSeconds, 360.0f) : FMath::FInterpTo(SpinDegrees, SpinDegrees > 180.0f ? 360.0f : 0.0f, DeltaSeconds, 12.0f);
	ArmsShown = FMath::FInterpTo(ArmsShown, bWhirling || bWindup ? 1.0f : 0.0f, DeltaSeconds, 12.0f);
	CollapseShown = FMath::FInterpTo(CollapseShown, bDown ? 1.0f : 0.0f, DeltaSeconds, bDown ? 9.0f : 3.0f);

	// Jerks while it lurks, leans into the whirl, sways when dazed, lies flat when soaked
	FRotator Lean = FRotator::ZeroRotator;
	float HeadDroop = 0.0f;

	if (!bDown)
	{
		switch (State)
		{
		case EVaelDollState::Lurk:
		{
			// Lurches from side to side like something that never learned to walk, with sudden jerks; stock-still in between
			const bool bStepping = GetVelocity().Size2D() > 30.0f;
			const float Lurch = FMath::Sin(Time * 9.0f) * 15.0f + FMath::Sign(FMath::Sin(Time * 23.0f)) * 5.0f;
			const float Jerk = FMath::Sin(Time * 3.7f) > 0.82f ? FMath::Sign(FMath::Sin(Time * 41.0f)) * 12.0f : 0.0f;
			Lean = bStepping ? FRotator(-16.0f + FMath::Sin(Time * 18.0f) * 6.0f, Jerk, Lurch) : FRotator(0.0f, Jerk * 0.6f, FMath::Sin(Time * 0.9f) * 2.0f);
			HeadDroop = bStepping ? 0.35f : 0.0f;
			break;
		}
		case EVaelDollState::Windup:
			Lean = FRotator(10.0f * FMath::Min(StateTime / FMath::Max(Data->WindupTime, 0.01f), 1.0f), 0.0f, 0.0f);
			break;
		case EVaelDollState::Whirl:
			Lean = FRotator(-8.0f, SpinDegrees, 0.0f);
			break;
		case EVaelDollState::Dazed:
			Lean = FRotator(FMath::Sin(Time * 3.1f) * 8.0f, 0.0f, FMath::Sin(Time * 4.0f) * 13.0f);
			HeadDroop = 1.0f;
			break;
		default:
			break;
		}
	}

	const float HeightScale = FMath::Lerp(1.0f, DollCollapsedHeight, CollapseShown);
	const float WidthScale = 1.0f + 0.45f * CollapseShown;

	if (UPoseableMeshComponent* DollMesh = GetFactoryMesh())
	{
		PoseFactoryMesh(DollMesh, Lean, FVector(WidthScale, WidthScale, HeightScale), HeadDroop, Time);
		return;
	}

	Rig->SetRelativeScale3D(FVector(WidthScale, WidthScale, HeightScale));
	Rig->SetRelativeLocation(FVector(0.0f, 0.0f, -HalfHeight * (1.0f - HeightScale)));
	Rig->SetRelativeRotation(Lean);

	Head->SetRelativeLocation(FVector(6.0f * HeadDroop, 0.0f, 58.0f - 6.0f * HeadDroop));
	Head->SetRelativeRotation(FRotator(-35.0f * HeadDroop, 0.0f, FMath::Sign(FMath::Sin(Time * 2.3f)) * 6.0f));

	for (int32 Index = 0; Index < 2; ++Index)
	{
		const float Side = Index == 0 ? -1.0f : 1.0f;

		UStaticMeshComponent* Eye = Eyes[Index];
		Eye->SetRelativeLocation(Head->GetRelativeLocation() + FVector(13.0f, 7.0f * Side, 3.0f));
		Eye->SetRelativeScale3D(FVector(5.0f / DollShapeSize));
		VaelEffects::SetLookGlow(Eye, bDown ? 0.3f : HeadDroop > 0.0f ? 2.0f : 6.0f);

		// Arms hang with the sickles down, and stand straight out while it whirls
		const FVector Shoulder(0.0f, 20.0f * Side, 36.0f);
		const FVector Hand = FMath::Lerp(FVector(8.0f, 30.0f * Side, -12.0f), FVector(0.0f, 72.0f * Side, 40.0f), ArmsShown);
		const FVector Reach = (Hand - Shoulder).GetSafeNormal();

		PlaceDollSegment(Arms[Index], Shoulder, Hand, FVector2D(11.0f, 11.0f));
		PlaceDollSegment(Sickles[Index], Hand, Hand + Reach * 62.0f + FVector(14.0f, 0.0f, 0.0f), FVector2D(5.0f, 17.0f));
	}
}

#undef LOCTEXT_NAMESPACE

void AVaelSickleDoll::SetupFactoryArms()
{
	const USkeletalMesh* SkeletalMesh = Cast<USkeletalMesh>(GetFactoryMesh()->GetSkinnedAsset());
	if (SkeletalMesh == nullptr)
	{
		return;
	}

	const FReferenceSkeleton& RefSkeleton = SkeletalMesh->GetRefSkeleton();
	const FName ArmBones[2] = { TEXT("arm_L_upper"), TEXT("arm_R_upper") };
	const FName SickleBones[2] = { TEXT("arm_L_sickle"), TEXT("arm_R_sickle") };

	for (int32 Side = 0; Side < 2; ++Side)
	{
		const int32 ArmIndex = RefSkeleton.FindBoneIndex(ArmBones[Side]);
		const int32 SickleIndex = RefSkeleton.FindBoneIndex(SickleBones[Side]);
		if (ArmIndex == INDEX_NONE || SickleIndex == INDEX_NONE)
		{
			continue;
		}

		ArmBoneNames[Side] = ArmBones[Side];
		RestArms[Side] = FAnimationRuntime::GetComponentSpaceTransformRefPose(RefSkeleton, ArmIndex);

		// The arm hangs when its hand turns down around the axis the mesh looks along
		const FVector Reach = FAnimationRuntime::GetComponentSpaceTransformRefPose(RefSkeleton, SickleIndex).GetLocation() - RestArms[Side].GetLocation();
		ArmDownSign[Side] = FQuat(FVector::RightVector, 0.2f).RotateVector(Reach).Z < Reach.Z ? 1.0f : -1.0f;
	}
}

void AVaelSickleDoll::PoseFactoryMesh(UPoseableMeshComponent* DollMesh, const FRotator& Lean, const FVector& Scale, float HeadDroop, float Time)
{
	const float HalfHeight = GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight();
	const float MeshYaw = GetData<UVaelSickleDollData>()->MeshYaw;

	// Spin, lean and collapse move the whole mesh on its foot
	DollMesh->SetRelativeLocationAndRotation(FVector(0.0f, 0.0f, -HalfHeight), Lean.Quaternion() * FRotator(0.0f, MeshYaw, 0.0f).Quaternion());
	DollMesh->SetRelativeScale3D(Scale);

	// The arms hang with the sickles down and fly out while it whirls; dazed they dangle
	for (int32 Side = 0; Side < 2; ++Side)
	{
		if (ArmBoneNames[Side].IsNone())
		{
			continue;
		}

		const float Dangle = HeadDroop * FMath::Sin(Time * 4.0f + Side * 1.3f) * 10.0f;
		const float Twitch = (1.0f - ArmsShown) * FMath::Sign(FMath::Sin(Time * 11.0f + Side * 2.0f)) * 3.0f;
		const float Down = FMath::DegreesToRadians((1.0f - ArmsShown) * 62.0f + Dangle + Twitch) * ArmDownSign[Side];

		FTransform Arm = RestArms[Side];
		Arm.SetRotation(FQuat(FVector::RightVector, Down) * RestArms[Side].GetRotation());
		DollMesh->SetBoneTransformByName(ArmBoneNames[Side], Arm, EBoneSpaces::ComponentSpace);
	}
}
