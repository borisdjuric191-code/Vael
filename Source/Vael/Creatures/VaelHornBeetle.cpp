// Copyright Epic Games, Inc. All Rights Reserved.

#include "Creatures/VaelHornBeetle.h"
#include "UObject/ConstructorHelpers.h"
#include "AnimationRuntime.h"
#include "Combat/VaelArcShot.h"
#include "Combat/VaelGroundStrike.h"
#include "Components/CapsuleComponent.h"
#include "Components/PoseableMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Creatures/VaelCreatureData.h"
#include "Creatures/VaelLegIKComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Magic/VaelGameplayTags.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Misc/App.h"
#include "Sound/SoundAttenuation.h"
#include "Sound/SoundWaveProcedural.h"
#include "UI/VaelCombatTextSubsystem.h"
#include "Vael.h"

#define LOCTEXT_NAMESPACE "VaelCreatures"

namespace
{
	/** Fixed bone names of the body family Vielbeiner */
	const FName BeetleHornFirstBone(TEXT("horn_L"));
	const FName BeetleHornSecondBone(TEXT("horn_R"));
	const FName BeetleHornMiddleBone(TEXT("horn_M"));
	const FName BeetleBodyBone(TEXT("body"));

	/** The beetle stops walking home closer than this, in cm */
	constexpr float BeetleHomeTolerance = 160.0f;

	/** Pitch in degrees the beetle rears up to while it spreads its horns */
	constexpr float BeetleRearPitch = 7.0f;

	/** How quickly the horns follow their spread; they snap back much faster when the thread is released */
	constexpr float BeetleSpreadSpeed = 14.0f;
	constexpr float BeetleReleaseSpeed = 30.0f;

	/** Seconds the corpse takes to curl up */
	constexpr float BeetleCurlTime = 0.45f;

	/** Colors of the thread: pale silk, glowing while it burns */
	const FLinearColor BeetleThreadColor(0.86f, 0.82f, 0.7f);
	const FLinearColor BeetleBurnColor(4.0f, 1.2f, 0.2f);
	const FLinearColor BeetleShotColor(0.36f, 0.42f, 0.2f);

	/** Sound of the ratchet and the "Klack", made in code until the sound bible gets real recordings */
	constexpr int32 BeetleSampleRate = 22050;

	TArray<uint8> MakeClickSamples(float Seconds, float Frequency, float NoiseShare, float Decay, float Loudness)
	{
		const int32 NumSamples = FMath::RoundToInt(BeetleSampleRate * Seconds);
		TArray<uint8> Bytes;
		Bytes.SetNumZeroed(NumSamples * sizeof(int16));
		int16* Samples = reinterpret_cast<int16*>(Bytes.GetData());

		FRandomStream Random(7);
		for (int32 Index = 0; Index < NumSamples; ++Index)
		{
			const float Time = static_cast<float>(Index) / BeetleSampleRate;
			const float Envelope = FMath::Exp(-Time * Decay) * FMath::Min(Time * 4000.0f, 1.0f);
			const float Tone = FMath::Sin(UE_TWO_PI * Frequency * Time);
			const float Noise = Random.FRandRange(-1.0f, 1.0f);
			const float Value = (Tone * (1.0f - NoiseShare) + Noise * NoiseShare) * Envelope * Loudness;
			Samples[Index] = static_cast<int16>(FMath::Clamp(Value, -1.0f, 1.0f) * 32000.0f);
		}

		return Bytes;
	}

	const TArray<uint8>& GetRatchetSamples()
	{
		static const TArray<uint8> Samples = MakeClickSamples(0.03f, 2300.0f, 0.65f, 180.0f, 0.55f);
		return Samples;
	}

	const TArray<uint8>& GetKlackSamples()
	{
		static const TArray<uint8> Samples = MakeClickSamples(0.09f, 620.0f, 0.4f, 55.0f, 0.95f);
		return Samples;
	}
}

AVaelHornBeetle::AVaelHornBeetle()
{
	CorpseDuration = 1.6f;

	// Turns on its own, slowly enough to be read
	GetCharacterMovement()->bOrientRotationToMovement = false;

	PoseMesh = CreateDefaultSubobject<UPoseableMeshComponent>(TEXT("PoseMesh"));
	PoseMesh->SetupAttachment(RootComponent);
	PoseMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	PoseMesh->SetCastShadow(true);

	Legs = CreateDefaultSubobject<UVaelLegIKComponent>(TEXT("Legs"));

	Thread = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Thread"));
	Thread->SetupAttachment(RootComponent);
	Thread->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Thread->SetCastShadow(false);
	Thread->SetVisibility(false);
	Thread->SetUsingAbsoluteLocation(true);
	Thread->SetUsingAbsoluteRotation(true);
	Thread->SetUsingAbsoluteScale(true);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> ThreadMesh(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> ThreadMaterialAsset(TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
	if (ThreadMesh.Succeeded())
	{
		Thread->SetStaticMesh(ThreadMesh.Object);
	}
	if (ThreadMaterialAsset.Succeeded())
	{
		Thread->SetMaterial(0, ThreadMaterialAsset.Object);
	}
}

TSubclassOf<UVaelCreatureData> AVaelHornBeetle::GetDefaultDataClass() const
{
	return UVaelHornBeetleData::StaticClass();
}

void AVaelHornBeetle::BeginPlay()
{
	Super::BeginPlay();

	const UVaelHornBeetleData* Data = GetData<UVaelHornBeetleData>();
	ShotCooldown = RandomInRange(Data->FirstShotDelay);

	// The mesh replaces the placeholder body; it stands on the bottom of the capsule
	USkeletalMesh* SkeletalMesh = Data->Mesh.LoadSynchronous();
	if (SkeletalMesh == nullptr)
	{
		UE_LOG(LogVael, Warning, TEXT("'%s': mesh '%s' is missing, the beetle keeps its placeholder"), *GetNameSafe(this), *Data->Mesh.ToString());
		return;
	}

	GetBody()->SetVisibility(false);
	PoseMesh->SetSkinnedAssetAndUpdate(SkeletalMesh);
	PoseMesh->SetRelativeLocationAndRotation(FVector(0.0f, 0.0f, -GetCapsuleComponent()->GetScaledCapsuleHalfHeight()), FRotator(0.0f, Data->MeshYaw, 0.0f));
	MeshMaterial = PoseMesh->CreateAndSetMaterialInstanceDynamic(0);

	const FReferenceSkeleton& RefSkeleton = SkeletalMesh->GetRefSkeleton();
	const auto RestOf = [&RefSkeleton](FName Bone)
	{
		const int32 Index = RefSkeleton.FindBoneIndex(Bone);
		return Index != INDEX_NONE ? FAnimationRuntime::GetComponentSpaceTransformRefPose(RefSkeleton, Index) : FTransform::Identity;
	};
	RestHornFirst = RestOf(BeetleHornFirstBone);
	RestHornSecond = RestOf(BeetleHornSecondBone);
	RestHornMiddle = RestOf(BeetleHornMiddleBone);
	RestBody = RestOf(BeetleBodyBone);

	// Outwards is the turn that moves a horn tip further to the side it already stands on
	const FTransform* RestHorns[2] = { &RestHornFirst, &RestHornSecond };
	const FVector RestTips[2] = { Data->HornTipFirst, Data->HornTipSecond };
	for (int32 Side = 0; Side < 2; ++Side)
	{
		const FVector Offset = RestTips[Side] - RestHorns[Side]->GetLocation();
		const float Moved = FQuat(FVector::UpVector, 0.2f).RotateVector(Offset).X - Offset.X;
		HornSpreadSign[Side] = Moved * RestTips[Side].X > 0.0f ? 1.0f : -1.0f;
	}

	Legs->Setup(PoseMesh);
	Legs->OnLegsPosed.AddUObject(this, &AVaelHornBeetle::PoseHorns);

	ThreadMaterial = Thread->CreateAndSetMaterialInstanceDynamic(0);

	ClickAttenuation = NewObject<USoundAttenuation>(this);
	ClickAttenuation->Attenuation.bAttenuate = true;
	ClickAttenuation->Attenuation.AttenuationShapeExtents = FVector(400.0f);
	ClickAttenuation->Attenuation.FalloffDistance = 3000.0f;

	RefreshBodyColor();
}

float AVaelHornBeetle::GetIncomingDamageMultiplier(const FGameplayTagContainer& DamageTags) const
{
	const float Multiplier = Super::GetIncomingDamageMultiplier(DamageTags);
	return State == EVaelBeetleState::Defenseless ? Multiplier * GetData<UVaelHornBeetleData>()->DefenselessDamageMultiplier : Multiplier;
}

void AVaelHornBeetle::EnterState(EVaelBeetleState NewState)
{
	State = NewState;
	StateTime = 0.0f;

	const UVaelHornBeetleData* Data = GetData<UVaelHornBeetleData>();
	switch (State)
	{
	case EVaelBeetleState::Tension:
		ClickCountdown = 0.0f;
		Legs->SetExtraBodyTilt(FRotator(BeetleRearPitch, 0.0f, 0.0f));
		break;
	case EVaelBeetleState::Locked:
		SpreadTarget = 1.0f;
		PlayClick(true);
		break;
	case EVaelBeetleState::Defenseless:
		SpreadTarget = 0.0f;
		Legs->SetExtraBodyTilt(FRotator(-4.0f, 0.0f, 0.0f));
		ShotCooldown = RandomInRange(Data->ShotInterval);
		break;
	default:
		SpreadTarget = 0.0f;
		Legs->SetExtraBodyTilt(FRotator::ZeroRotator);
		break;
	}

	UE_LOG(LogVael, Verbose, TEXT("'%s' beetle state %d"), *GetNameSafe(this), static_cast<int32>(State));
}

void AVaelHornBeetle::StartTension()
{
	if (!IsDead())
	{
		ShotTarget = FindTarget(GetData<UVaelHornBeetleData>()->AggroRange);
		EnterState(EVaelBeetleState::Tension);
	}
}

void AVaelHornBeetle::TurnTowards(const FVector& Location, float DeltaSeconds)
{
	const FVector Direction = (Location - GetActorLocation()).GetSafeNormal2D();
	if (Direction.IsNearlyZero())
	{
		return;
	}

	const float Yaw = FMath::FixedTurn(GetActorRotation().Yaw, Direction.Rotation().Yaw, GetData<UVaelHornBeetleData>()->TurnSpeed * DeltaSeconds);
	SetActorRotation(FRotator(0.0f, Yaw, 0.0f));
}

void AVaelHornBeetle::TickBehavior(float DeltaSeconds)
{
	const UVaelHornBeetleData* Data = GetData<UVaelHornBeetleData>();
	StateTime += DeltaSeconds;

	switch (State)
	{
	case EVaelBeetleState::Defenseless:
		if (StateTime >= Data->DefenselessTime)
		{
			EnterState(EVaelBeetleState::Roam);
		}
		return;

	case EVaelBeetleState::Tension:
	{
		if (const AActor* Target = ShotTarget.Get())
		{
			TurnTowards(Target->GetActorLocation(), DeltaSeconds);
		}

		// The ratchet: the horns open click by click
		ClickCountdown -= DeltaSeconds;
		if (ClickCountdown <= 0.0f)
		{
			ClickCountdown += Data->ClickInterval;
			SpreadTarget = FMath::Min(StateTime / Data->TensionTime, 1.0f);
			Tremble = 1.0f;
			PlayClick(false);
		}

		if (StateTime >= Data->TensionTime)
		{
			EnterState(EVaelBeetleState::Locked);
		}
		return;
	}

	case EVaelBeetleState::Locked:
		if (const AActor* Target = ShotTarget.Get())
		{
			TurnTowards(Target->GetActorLocation(), DeltaSeconds);
		}

		if (StateTime >= Data->LockTime)
		{
			Shoot();
			ShotCooldown = RandomInRange(Data->ShotInterval);
			EnterState(EVaelBeetleState::Roam);
		}
		return;

	default:
		break;
	}

	float TargetDistance = 0.0f;
	AActor* Target = FindTarget(Data->AggroRange, &TargetDistance);
	if (Target == nullptr)
	{
		if (FVector::Dist2D(GetActorLocation(), HomeLocation) > BeetleHomeTolerance)
		{
			TurnTowards(HomeLocation, DeltaSeconds);
			MoveInDirection(GetActorForwardVector(), Data->MoveSpeed * 0.6f);
		}
		return;
	}

	TurnTowards(Target->GetActorLocation(), DeltaSeconds);

	// Keeps its shooting distance: walks forward when too far, backs off without turning when too close
	const FVector ToTarget = (Target->GetActorLocation() - GetActorLocation()).GetSafeNormal2D();
	if (TargetDistance > Data->MaxDistance)
	{
		MoveInDirection(ToTarget, Data->MoveSpeed);
	}
	else if (TargetDistance < Data->MinDistance)
	{
		MoveInDirection(-ToTarget, Data->MoveSpeed * 0.7f);
	}

	ShotCooldown -= DeltaSeconds;
	if (ShotCooldown <= 0.0f && TargetDistance < Data->ShotRange && !IsBlinded())
	{
		ShotTarget = Target;
		EnterState(EVaelBeetleState::Tension);
	}
}

void AVaelHornBeetle::Shoot()
{
	const UVaelHornBeetleData* Data = GetData<UVaelHornBeetleData>();
	const AActor* Target = ShotTarget.Get();
	if (Target == nullptr)
	{
		return;
	}

	// Aimed at where the target stands now: the mark on the ground gives the flight time to step aside
	FVector Ground;
	if (!FindGround(GetWorld(), Target->GetActorLocation(), Ground))
	{
		return;
	}

	FVaelSpellHit Hit;
	Hit.Damage = Data->ShotDamage;
	Hit.Element = EVaelElement::Earth;
	Hit.Knockback = 250.0f;
	AVaelGroundStrike::SpawnStrike(this, Ground, Hit, Data->ShotRadius, Data->FlightTime, BeetleShotColor);

	const FTransform Middle = PoseMesh->GetBoneTransformByName(BeetleHornMiddleBone, EBoneSpaces::ComponentSpace);
	const FVector LocalTip = Middle.TransformPosition(RestHornMiddle.InverseTransformPosition(Data->MiddleHornTip));
	AVaelArcShot::Lob(GetWorld(), PoseMesh->GetComponentTransform().TransformPosition(LocalTip), Ground, Data->FlightTime, Data->ShotApex, BeetleShotColor);

	// The horns snap back and the body recoils
	SpreadShown = 1.15f;
	Legs->AddBodyJolt(-GetActorForwardVector() * 9.0f);
	++ShotCount;

	UE_LOG(LogVael, Verbose, TEXT("'%s' shoots at '%s'"), *GetNameSafe(this), *GetNameSafe(Target));
}

void AVaelHornBeetle::OnDamageTaken(float Damage, const FGameplayTagContainer& DamageTags)
{
	Super::OnDamageTaken(Damage, DamageTags);

	if (Damage > 0.0f)
	{
		Legs->AddBodyJolt(FVector(FMath::FRandRange(-1.0f, 1.0f), FMath::FRandRange(-1.0f, 1.0f), -0.6f).GetSafeNormal() * 5.0f);
	}

	// Fire on the tensed thread burns it through
	if ((State == EVaelBeetleState::Tension || State == EVaelBeetleState::Locked) && DamageTags.HasTagExact(VaelTags::Element_Fire))
	{
		BurnThread();
	}
}

void AVaelHornBeetle::BurnThread()
{
	BurnGlow = 0.35f;
	SpreadShown = FMath::Max(SpreadShown, 0.9f);
	EnterState(EVaelBeetleState::Defenseless);
	UVaelCombatTextSubsystem::PostReaction(this, LOCTEXT("ThreadBurned", "Faden durchgebrannt!"));
	UE_LOG(LogVael, Log, TEXT("'%s': fire burned the thread"), *GetNameSafe(this));
}

void AVaelHornBeetle::Die()
{
	Super::Die();

	DeathTime = 0.0f;
	Thread->SetVisibility(false);
	SpreadTarget = 0.0f;
	Legs->SetExtraBodyTilt(FRotator::ZeroRotator);
}

void AVaelHornBeetle::PoseHorns()
{
	const UVaelHornBeetleData* Data = GetData<UVaelHornBeetleData>();
	const float DeltaSeconds = GetWorld()->GetDeltaSeconds();

	if (DeathTime >= 0.0f)
	{
		DeathTime += DeltaSeconds;
		Legs->SetCurl(DeathTime / BeetleCurlTime);
	}

	const bool bReleasing = SpreadShown > SpreadTarget;
	SpreadShown = FMath::FInterpTo(SpreadShown, SpreadTarget, DeltaSeconds, bReleasing ? BeetleReleaseSpeed : BeetleSpreadSpeed);
	Tremble = FMath::FInterpTo(Tremble, 0.0f, DeltaSeconds, 18.0f);

	// The side horns swing outwards around the up axis of the body
	const FTransform BodyNow = PoseMesh->GetBoneTransformByName(BeetleBodyBone, EBoneSpaces::ComponentSpace);
	const FVector Up = (BodyNow.GetRotation() * RestBody.GetRotation().Inverse()).RotateVector(FVector::UpVector);
	const float Angle = FMath::DegreesToRadians(Data->HornSpread * SpreadShown + Tremble * 1.5f);

	FVector Tips[2];
	const FName HornBones[2] = { BeetleHornFirstBone, BeetleHornSecondBone };
	const FTransform* RestHorns[2] = { &RestHornFirst, &RestHornSecond };
	const FVector RestTips[2] = { Data->HornTipFirst, Data->HornTipSecond };

	for (int32 Side = 0; Side < 2; ++Side)
	{
		PoseMesh->ResetBoneTransformByName(HornBones[Side]);
		FTransform Horn = PoseMesh->GetBoneTransformByName(HornBones[Side], EBoneSpaces::ComponentSpace);

		const FQuat Spread(Up, Angle * HornSpreadSign[Side]);

		Horn.SetRotation(Spread * Horn.GetRotation());
		PoseMesh->SetBoneTransformByName(HornBones[Side], Horn, EBoneSpaces::ComponentSpace);

		Tips[Side] = PoseMesh->GetComponentTransform().TransformPosition(Horn.TransformPosition(RestHorns[Side]->InverseTransformPosition(RestTips[Side])));
	}

	// The thread runs between the tips while it is tensed, and glows for a moment when fire burns it
	BurnGlow = FMath::Max(BurnGlow - DeltaSeconds, 0.0f);
	const bool bTensed = !IsDead() && (State == EVaelBeetleState::Tension || State == EVaelBeetleState::Locked);
	const bool bShowThread = bTensed || BurnGlow > 0.0f;
	Thread->SetVisibility(bShowThread);

	if (bShowThread)
	{
		const FVector Span = Tips[1] - Tips[0];
		const float Length = Span.Size();
		Thread->SetWorldLocationAndRotation((Tips[0] + Tips[1]) * 0.5f, FRotationMatrix::MakeFromZ(Span.GetSafeNormal()).Rotator());
		Thread->SetWorldScale3D(FVector(BurnGlow > 0.0f ? 0.03f : 0.016f, BurnGlow > 0.0f ? 0.03f : 0.016f, Length / 100.0f * (BurnGlow > 0.0f ? BurnGlow / 0.35f : 1.0f)));

		if (ThreadMaterial != nullptr)
		{
			ThreadMaterial->SetVectorParameterValue(TEXT("Color"), BurnGlow > 0.0f ? BeetleBurnColor : BeetleThreadColor);
		}
	}
}

void AVaelHornBeetle::OnBodyColorShown(const FLinearColor& Color, bool bHitFlash)
{
	if (MeshMaterial == nullptr)
	{
		return;
	}

	// The Mark glow of the master material: violet for marked or raised beetles, a white flash for hits
	const bool bMarkedLook = IsMarked() || IsOnPlayerSide();
	MeshMaterial->SetScalarParameterValue(TEXT("MarkStaerke"), bHitFlash ? 2.5f : bMarkedLook ? 1.2f : 0.0f);
	MeshMaterial->SetVectorParameterValue(TEXT("MarkFarbe"), bHitFlash ? FLinearColor(1.0f, 0.92f, 0.8f) : FLinearColor(0.55f, 0.12f, 1.0f));
}

void AVaelHornBeetle::PlayClick(bool bKlack)
{
	if (GetNetMode() == NM_DedicatedServer || !FApp::CanEverRenderAudio())
	{
		return;
	}

	const TArray<uint8>& Samples = bKlack ? GetKlackSamples() : GetRatchetSamples();

	USoundWaveProcedural* Wave = NewObject<USoundWaveProcedural>(this);
	Wave->SetSampleRate(BeetleSampleRate);
	Wave->NumChannels = 1;
	Wave->Duration = static_cast<float>(Samples.Num() / sizeof(int16)) / BeetleSampleRate;
	Wave->SoundGroup = SOUNDGROUP_Default;
	Wave->bLooping = false;
	Wave->QueueAudio(Samples.GetData(), Samples.Num());

	UGameplayStatics::SpawnSoundAtLocation(this, Wave, GetActorLocation(), FRotator::ZeroRotator, bKlack ? 1.0f : 0.6f, FMath::FRandRange(0.94f, 1.08f), 0.0f, ClickAttenuation);
}

#undef LOCTEXT_NAMESPACE
