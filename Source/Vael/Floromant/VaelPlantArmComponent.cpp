// Copyright Epic Games, Inc. All Rights Reserved.

#include "Floromant/VaelPlantArmComponent.h"
#include "AbilitySystemComponent.h"
#include "Combat/VaelAttributeSet.h"
#include "Combat/VaelCombatStatics.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/SplineMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Floromant/VaelFloromantComponent.h"
#include "Floromant/VaelFloromantSettings.h"
#include "Floromant/VaelPlantArmData.h"
#include "Magic/VaelFormulaAbility.h"
#include "Magic/VaelSpellEffects.h"
#include "Magic/VaelSpellProjectile.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "NiagaraSystem.h"
#include "Player/VaelCharacter.h"
#include "UI/VaelCombatTextSubsystem.h"
#include "World/VaelSafeZone.h"
#include "VaelAssets.h"

namespace
{
	/** Fingers from thumb to little finger, as the bones of the body are named */
	const TCHAR* const FingerNames[] = { TEXT("thumb"), TEXT("index"), TEXT("middle"), TEXT("ring"), TEXT("pinky") };
	constexpr int32 NumFingers = UE_ARRAY_COUNT(FingerNames);

	/** Length of each finger stalk compared with the plant's finger length: the middle ones reach farthest */
	constexpr float FingerLengthShares[] = { 0.75f, 1.0f, 1.15f, 0.95f, 0.8f };

	/** Length of the stalk meshes along +Z, in cm (Tools/Pflanzenarm/halme_zerlegen.py) */
	constexpr float StalkMeshLength = 100.0f;

	/** Material parameters of M_Halm */
	const FName GrowParameter(TEXT("Wuchs"));
	const FName WindParameter(TEXT("Wind"));
	const FName SpinParameter(TEXT("Spin"));
	const FName ShadowParameter(TEXT("Schatten"));
	const FName FlashParameter(TEXT("Aufleuchten"));
	const FName PhaseParameter(TEXT("Zeitversatz"));
	const FName ArmPointParameter(TEXT("ArmPunkt"));
	const FName ArmAxisParameter(TEXT("ArmAchse"));

	/** Any direction across the given one */
	FVector Across(const FVector& Direction)
	{
		const FVector Side = FVector::CrossProduct(Direction, FVector::UpVector);
		return Side.IsNearlyZero() ? FVector::ForwardVector : Side.GetSafeNormal();
	}
}

UVaelPlantArmComponent::UVaelPlantArmComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickGroup = TG_PostUpdateWork;
}

AVaelCharacter* UVaelPlantArmComponent::GetCharacter() const
{
	return Cast<AVaelCharacter>(GetOwner());
}

USkeletalMeshComponent* UVaelPlantArmComponent::GetBody() const
{
	const AVaelCharacter* Character = GetCharacter();
	return Character != nullptr ? Character->GetMesh() : nullptr;
}

FName UVaelPlantArmComponent::Bone(const TCHAR* Name) const
{
	return FName(FString::Printf(TEXT("%s_%s"), Name, bLeft ? TEXT("l") : TEXT("r")));
}

FVector UVaelPlantArmComponent::BoneLocation(const TCHAR* Name) const
{
	return GetBody()->GetSocketTransform(Bone(Name), RTS_Component).GetLocation();
}

// ---------------------------------------------------------------- Plant

void UVaelPlantArmComponent::SetPlant(UVaelPlantArmData* InPlant)
{
	if (InPlant == (bChanging ? NextPlant.Get() : Plant.Get()))
	{
		return;
	}

	// The stalks there are wither back into the hand first; the new plant grows once they are gone
	NextPlant = InPlant;
	bChanging = true;
	if (Plant == nullptr)
	{
		Grow = 0.0f;
	}
}

UVaelPlantArmData* UVaelPlantArmComponent::GetPlant() const
{
	return bChanging ? NextPlant.Get() : Plant.Get();
}

void UVaelPlantArmComponent::SetFiring(bool bInFiring)
{
	if (bInFiring && !bFiring)
	{
		FireTime = 0.0f;
	}
	bFiring = bInFiring;
}

bool UVaelPlantArmComponent::IsFiring() const
{
	return bFiring && Plant != nullptr && !bChanging;
}

int32 UVaelPlantArmComponent::GetBarShots() const
{
	return FMath::FloorToInt32(BarShots);
}

float UVaelPlantArmComponent::GetBarFraction() const
{
	return Plant != nullptr ? FMath::Clamp(BarShots / FMath::Max(Plant->BarSize, 1), 0.0f, 1.0f) : 0.0f;
}

void UVaelPlantArmComponent::RefillBar(int32 Shots)
{
	if (Plant != nullptr)
	{
		BarShots = FMath::Min<float>(BarShots + Shots, Plant->BarSize);
	}
}

int32 UVaelPlantArmComponent::GetMissingShots() const
{
	return Plant != nullptr && !bChanging ? FMath::Max(0, Plant->BarSize - FMath::CeilToInt32(BarShots)) : 0;
}

void UVaelPlantArmComponent::StartReload(int32 Shots, float Duration)
{
	ReloadShots = Shots;
	ReloadDuration = FMath::Max(Duration, 0.05f);
	ReloadTimeLeft = ReloadDuration;
}

void UVaelPlantArmComponent::Clear()
{
	if (KeimpelzPad != nullptr)
	{
		KeimpelzPad->DestroyComponent();
		KeimpelzPad = nullptr;
	}
	ReloadShots = 0;
	ReloadTimeLeft = 0.0f;
	for (TArray<TObjectPtr<USplineMeshComponent>>* Stalks : { &FingerStalks, &WrapStalks, &WildStalks, &ForearmVines, &UpperArmVines, &OvergrowthWild })
	{
		for (USplineMeshComponent* Stalk : *Stalks)
		{
			if (Stalk != nullptr)
			{
				Stalk->DestroyComponent();
			}
		}
		Stalks->Reset();
	}
	for (TArray<TObjectPtr<UStaticMeshComponent>>* Pieces : { &FingerTips, &OvergrowthPads })
	{
		for (UStaticMeshComponent* Piece : *Pieces)
		{
			if (Piece != nullptr)
			{
				Piece->DestroyComponent();
			}
		}
		Pieces->Reset();
	}
	OvergrowthMaterialA = nullptr;
	OvergrowthMaterialB = nullptr;
	Overgrowth = 0.0f;
	FingerMaterials.Reset();
	ArmMaterial = nullptr;
	Plant = nullptr;
	NextPlant = nullptr;
	bChanging = false;
	Grow = 0.0f;
	Stance = 0.0f;
}

void UVaelPlantArmComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	Clear();
	Super::EndPlay(EndPlayReason);
}

USplineMeshComponent* UVaelPlantArmComponent::MakeStalk(UStaticMesh* Mesh, UMaterialInstanceDynamic* Material)
{
	USplineMeshComponent* Stalk = NewObject<USplineMeshComponent>(GetOwner());
	Stalk->SetMobility(EComponentMobility::Movable);
	Stalk->SetForwardAxis(ESplineMeshAxis::Z, false);
	Stalk->SetStaticMesh(Mesh);
	Stalk->SetMaterial(0, Material);
	Stalk->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Stalk->SetCanEverAffectNavigation(false);
	Stalk->SetupAttachment(GetBody());
	Stalk->RegisterComponent();
	return Stalk;
}

void UVaelPlantArmComponent::BuildStalks()
{
	UVaelPlantArmData* Data = Plant;
	Plant = nullptr;
	Clear();
	Plant = Data;
	if (Plant == nullptr || GetBody() == nullptr)
	{
		return;
	}

	UMaterialInterface* BaseMaterial = VaelAssets::LoadOptional(Plant->Material);
	TArray<UStaticMesh*> FingerMeshes;
	for (const TSoftObjectPtr<UStaticMesh>& Mesh : Plant->FingerMeshes)
	{
		if (UStaticMesh* Loaded = VaelAssets::LoadOptional(Mesh))
		{
			FingerMeshes.Add(Loaded);
		}
	}
	TArray<UStaticMesh*> ArmMeshes;
	for (const TSoftObjectPtr<UStaticMesh>& Mesh : Plant->ArmMeshes)
	{
		if (UStaticMesh* Loaded = VaelAssets::LoadOptional(Mesh))
		{
			ArmMeshes.Add(Loaded);
		}
	}
	if (ArmMeshes.IsEmpty())
	{
		ArmMeshes = FingerMeshes;
	}
	if (FingerMeshes.IsEmpty())
	{
		return;
	}

	// The left hand starts with another stalk, so the two hands don't look mirrored
	const int32 Offset = bLeft ? 1 : 0;
	for (int32 Finger = 0; Finger < NumFingers; ++Finger)
	{
		UMaterialInstanceDynamic* Material = UMaterialInstanceDynamic::Create(BaseMaterial, this);
		Material->SetScalarParameterValue(PhaseParameter, Finger * 1.3f + Offset * 0.7f);
		FingerMaterials.Add(Material);
		FingerStalks.Add(MakeStalk(FingerMeshes[(Finger + Offset) % FingerMeshes.Num()], Material));

		if (UStaticMesh* TipMesh = VaelAssets::LoadOptional(Plant->TipMesh))
		{
			UStaticMeshComponent* Tip = NewObject<UStaticMeshComponent>(GetOwner());
			Tip->SetMobility(EComponentMobility::Movable);
			Tip->SetStaticMesh(TipMesh);
			Tip->SetMaterial(0, Material);
			Tip->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			Tip->SetupAttachment(GetBody());
			Tip->RegisterComponent();
			FingerTips.Add(Tip);
		}
	}

	ArmMaterial = UMaterialInstanceDynamic::Create(BaseMaterial, this);
	ArmMaterial->SetScalarParameterValue(PhaseParameter, Offset * 2.1f);
	for (int32 Index = 0; Index < Plant->WrapCount; ++Index)
	{
		WrapStalks.Add(MakeStalk(ArmMeshes[(Index + Offset) % ArmMeshes.Num()], ArmMaterial));
	}

	// Wild stalks: where on the upper arm, at which angle around it, how long, each a little different
	FRandomStream Random(GetOwner()->GetUniqueID() * 2 + Offset);
	WildShapes.Reset();
	for (int32 Index = 0; Index < Plant->WildCount; ++Index)
	{
		WildShapes.Add(FVector(Random.FRandRange(0.2f, 1.0f), Random.FRandRange(0.0f, 2.0f * UE_PI), Random.FRandRange(0.6f, 1.2f)));
		WildStalks.Add(MakeStalk(ArmMeshes[(Index + 1 + Offset) % ArmMeshes.Num()], ArmMaterial));
	}

	// The Keimpelz pad on the forearm
	if (UStaticMesh* PadMesh = VaelAssets::LoadOptional(UVaelFloromantSettings::Get()->KeimpelzPadMesh))
	{
		KeimpelzPad = NewObject<UStaticMeshComponent>(GetOwner());
		KeimpelzPad->SetMobility(EComponentMobility::Movable);
		KeimpelzPad->SetStaticMesh(PadMesh);
		KeimpelzPad->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		KeimpelzPad->SetCanEverAffectNavigation(false);
		KeimpelzPad->SetupAttachment(GetBody());
		KeimpelzPad->RegisterComponent();
	}

	// Overgrowth while aiming: vines climbing the arm, more wild stalks and more Keimpelz, each with its own growing material
	TArray<UStaticMesh*> VineMeshes;
	for (const TSoftObjectPtr<UStaticMesh>& Mesh : Plant->VineMeshes)
	{
		if (UStaticMesh* Loaded = VaelAssets::LoadOptional(Mesh))
		{
			VineMeshes.Add(Loaded);
		}
	}
	OvergrowthMaterialA = UMaterialInstanceDynamic::Create(BaseMaterial, this);
	OvergrowthMaterialB = UMaterialInstanceDynamic::Create(BaseMaterial, this);
	OvergrowthMaterialA->SetScalarParameterValue(PhaseParameter, 0.4f + Offset);
	OvergrowthMaterialB->SetScalarParameterValue(PhaseParameter, 1.7f + Offset);
	for (int32 Index = 0; Index < Plant->VineCount && !VineMeshes.IsEmpty(); ++Index)
	{
		ForearmVines.Add(MakeStalk(VineMeshes[(Index + Offset) % VineMeshes.Num()], OvergrowthMaterialA));
		UpperArmVines.Add(MakeStalk(VineMeshes[(Index + 1 + Offset) % VineMeshes.Num()], OvergrowthMaterialB));
	}
	OvergrowthShapes.Reset();
	for (int32 Index = 0; Index < Plant->OvergrowthWildCount; ++Index)
	{
		OvergrowthShapes.Add(FVector(Random.FRandRange(0.0f, 1.0f), Random.FRandRange(0.0f, 2.0f * UE_PI), Random.FRandRange(0.6f, 1.3f)));
		OvergrowthWild.Add(MakeStalk(ArmMeshes[(Index + Offset) % ArmMeshes.Num()], OvergrowthMaterialB));
	}
	if (UStaticMesh* PadMesh = VaelAssets::LoadOptional(UVaelFloromantSettings::Get()->KeimpelzPadMesh))
	{
		for (int32 Index = 0; Index < 3; ++Index)
		{
			UStaticMeshComponent* Pad = NewObject<UStaticMeshComponent>(GetOwner());
			Pad->SetMobility(EComponentMobility::Movable);
			Pad->SetStaticMesh(PadMesh);
			Pad->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			Pad->SetCanEverAffectNavigation(false);
			Pad->SetupAttachment(GetBody());
			Pad->RegisterComponent();
			Pad->SetVisibility(false);
			OvergrowthPads.Add(Pad);
		}
	}
	Overgrowth = 0.0f;

	Muzzles.Init(FVector::ZeroVector, NumFingers);
	MuzzleDirections.Init(FVector::ForwardVector, NumFingers);
	Flashes.Init(0.0f, NumFingers);
	BarShots = Plant->BarSize;
	FireCountdown = 0.0f;
	NextFinger = 0;
	SpinTurns = 0.0f;
	SpinRate = 0.0f;
	Fan = 0.0f;

	// A short burst while the hand turns
	const FVector Hand = GetBody()->GetSocketLocation(Bone(TEXT("hand")));
	const bool bShadow = GetCharacter()->GetPath() == EVaelPath::Shadow;
	UNiagaraSystem* Burst = VaelAssets::LoadOptional(Plant->GrowEffect);
	if (Burst == nullptr)
	{
		Burst = VaelEffects::Load(Plant->ShotElement).Cast;
	}
	VaelEffects::SpawnAt(this, Burst, Hand, FRotator::ZeroRotator, Plant->GetColor(bShadow), 40.0f);
}

// ---------------------------------------------------------------- Update

void UVaelPlantArmComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (GetBody() == nullptr)
	{
		return;
	}

	// Wither and grow: the old plant goes back into the hand, then the new one comes out
	const float GrowSpeed = 1.0f / FMath::Max(Plant != nullptr ? Plant->GrowTime : 0.4f, 0.01f);
	if (bChanging)
	{
		Grow = FMath::Max(0.0f, Grow - GrowSpeed * DeltaTime);
		if (Grow <= 0.0f)
		{
			bChanging = false;
			Plant = NextPlant;
			NextPlant = nullptr;
			BuildStalks();
		}
	}
	else if (Plant != nullptr)
	{
		Grow = FMath::Min(1.0f, Grow + GrowSpeed * DeltaTime);
	}

	if (Plant == nullptr || FingerStalks.IsEmpty())
	{
		Stance = FMath::FInterpTo(Stance, 0.0f, DeltaTime, 8.0f);
		return;
	}

	// A reload ends: the stalks have drawn the moss in, the bar is full again and every tip flares up
	if (ReloadTimeLeft > 0.0f)
	{
		ReloadTimeLeft -= DeltaTime;
		if (ReloadTimeLeft <= 0.0f)
		{
			ReloadTimeLeft = 0.0f;
			RefillBar(ReloadShots);
			ReloadShots = 0;
			for (float& Flash : Flashes)
			{
				Flash = 1.0f;
			}
		}
	}

	const bool bFiringNow = IsFiring() && Grow >= 1.0f;
	Stance = FMath::FInterpTo(Stance, bFiringNow ? 1.0f : 0.0f, DeltaTime, bFiringNow ? 12.0f : 5.0f);
	Fan = FMath::FInterpTo(Fan, bFiringNow ? 1.0f : 0.0f, DeltaTime, 10.0f);

	// While the hand aims the plant overruns the whole arm; it falls back more slowly when the hand rests
	Overgrowth = bFiringNow
		? FMath::Min(1.0f, Overgrowth + DeltaTime / Plant->OvergrowthTime)
		: FMath::Max(0.0f, Overgrowth - DeltaTime / Plant->OvergrowthFadeTime);

	UpdateFiring(DeltaTime);
	UpdateStalks();
	UpdateMaterials(DeltaTime);
}

void UVaelPlantArmComponent::UpdateStalks()
{
	const USkeletalMeshComponent* Body = GetBody();
	const FTransform BodyTransform = Body->GetComponentTransform();

	const FVector Shoulder = BoneLocation(TEXT("upperarm"));
	const FVector Elbow = BoneLocation(TEXT("lowerarm"));
	const FVector Wrist = BoneLocation(TEXT("hand"));
	const FVector Axis = (Wrist - Elbow).GetSafeNormal();
	const FVector U = Across(Axis);
	const FVector V = FVector::CrossProduct(Axis, U);
	const float Radius = Plant->WrapRadius;

	// The middle line of the hand, from the wrist through the middle finger
	const FVector HandAxis = (BoneLocation(TEXT("middle_03")) - Wrist).GetSafeNormal();
	const float FanRadians = FMath::DegreesToRadians(Plant->FanAngle) * Fan;

	// Fingers: from the forearm past the fingertip into a long stalk
	for (int32 Finger = 0; Finger < FingerStalks.Num(); ++Finger)
	{
		const FString Name = FingerNames[Finger];
		const FVector Root = BoneLocation(*(Name + TEXT("_01")));
		const FVector Middle = BoneLocation(*(Name + TEXT("_02")));
		const FVector Last = BoneLocation(*(Name + TEXT("_03")));
		const FVector Tip = Last + (Last - Middle).GetSafeNormal() * 2.0f;

		// The finger points along itself at rest; while firing the stalks turn along the stretched arm and fan away from the
		// middle of the hand, each bent its own way and of its own length, so they don't stand stiffly ahead
		const FVector Along = (Axis + HandAxis).GetSafeNormal();
		FVector Direction = FMath::Lerp((Tip - Root).GetSafeNormal(), Along, Stance * 0.65f).GetSafeNormal();
		const float CurlAngle = Finger * 1.257f + (bLeft ? 0.9f : 0.0f);
		const FVector Curl = (U * FMath::Cos(CurlAngle) + V * FMath::Sin(CurlAngle)) * (0.25f + 0.2f * Stance);
		const float LengthShare = FingerLengthShares[Finger];
		const FVector Outward = (Direction - HandAxis * FVector::DotProduct(Direction, HandAxis)).GetSafeNormal();
		if (!Outward.IsNearlyZero() && FanRadians > 0.0f)
		{
			Direction = (Direction * FMath::Cos(FanRadians) + Outward * FMath::Sin(FanRadians)).GetSafeNormal();
		}

		// It comes out of the forearm on the side of its finger
		const FVector OnArm = FMath::Lerp(Wrist, Elbow, Plant->FingerRootReach);
		const FVector Side = Root - (Elbow + Axis * FVector::DotProduct(Root - Elbow, Axis));
		const FVector Start = OnArm + Side.GetSafeNormal() * Radius * 0.8f;
		const FVector End = Tip + (Direction + Curl * 0.5f).GetSafeNormal() * Plant->FingerLength * LengthShare;
		const float Span = FVector::Dist(Start, End);

		USplineMeshComponent* Stalk = FingerStalks[Finger];
		Stalk->SetStartAndEnd(Start, Axis * Span * 0.8f, End, (Direction - Curl).GetSafeNormal() * Span * 0.9f, false);
		Stalk->SetStartScale(FVector2D(Plant->FingerThickness * 0.7f), false);
		Stalk->SetEndScale(FVector2D(Plant->FingerThickness), false);
		Stalk->SetStartRoll(Finger * 1.1f, false);
		Stalk->SetEndRoll(Finger * 1.1f + 0.8f, false);
		Stalk->UpdateMesh();

		if (FingerTips.IsValidIndex(Finger))
		{
			FingerTips[Finger]->SetRelativeLocationAndRotation(End, FRotationMatrix::MakeFromZ(Direction).Rotator());
			FingerTips[Finger]->SetRelativeScale3D(FVector(Plant->TipSize / 10.0f * Grow));
		}

		Muzzles[Finger] = BodyTransform.TransformPosition(End);
		MuzzleDirections[Finger] = BodyTransform.TransformVectorNoScale(Direction);
	}

	// Winding stalks: half a turn around the forearm, from a little above the elbow to the wrist
	const FVector WrapStart = FMath::Lerp(Elbow, Shoulder, 0.15f);
	const float WrapLength = FVector::Dist(WrapStart, Wrist);
	const float TurnRadians = Plant->WrapTurns * 2.0f * UE_PI;
	for (int32 Index = 0; Index < WrapStalks.Num(); ++Index)
	{
		const float A0 = Index * 2.0f * UE_PI / WrapStalks.Num() + (bLeft ? 0.6f : 0.0f) + SpinTurns * 2.0f * UE_PI * (bLeft ? -1.0f : 1.0f);
		const float A1 = A0 + TurnRadians;
		const FVector Out0 = U * FMath::Cos(A0) + V * FMath::Sin(A0);
		const FVector Out1 = U * FMath::Cos(A1) + V * FMath::Sin(A1);
		const FVector Around0 = -U * FMath::Sin(A0) + V * FMath::Cos(A0);
		const FVector Around1 = -U * FMath::Sin(A1) + V * FMath::Cos(A1);

		// Tangents with 4 x radius across follow a half circle closely
		const float Sideways = 4.0f * Radius * Plant->WrapTurns * 2.0f;
		USplineMeshComponent* Stalk = WrapStalks[Index];
		Stalk->SetStartAndEnd(WrapStart + Out0 * Radius, Axis * WrapLength + Around0 * Sideways, Wrist + Out1 * Radius * 0.8f, Axis * WrapLength + Around1 * Sideways, false);
		Stalk->SetStartScale(FVector2D(0.6f * Plant->ArmThickness), false);
		Stalk->SetEndScale(FVector2D(0.45f * Plant->ArmThickness), false);
		Stalk->SetStartRoll(A0, false);
		Stalk->SetEndRoll(A1, false);
		Stalk->UpdateMesh();
	}

	// Wild stalks: out of the upper arm, down over the elbow and off into nothing
	const FVector UpperAxis = (Elbow - Shoulder).GetSafeNormal();
	const FVector UpperU = Across(UpperAxis);
	const FVector UpperV = FVector::CrossProduct(UpperAxis, UpperU);
	for (int32 Index = 0; Index < WildStalks.Num(); ++Index)
	{
		const FVector& Shape = WildShapes[Index];
		const FVector Out = UpperU * FMath::Cos(Shape.Y) + UpperV * FMath::Sin(Shape.Y);
		const FVector Start = FMath::Lerp(Elbow, Shoulder, Plant->UpperArmReach * Shape.X) + Out * Radius * 1.2f;
		const FVector Direction = (UpperAxis * 0.8f + Out * 0.6f).GetSafeNormal();
		const float Length = Plant->WildLength * Shape.Z;

		USplineMeshComponent* Stalk = WildStalks[Index];
		Stalk->SetStartAndEnd(Start, UpperAxis * Length, Start + Direction * Length, Direction * Length * 1.4f, false);
		Stalk->SetStartScale(FVector2D(0.5f * Plant->ArmThickness), false);
		Stalk->SetEndScale(FVector2D(0.35f * Plant->ArmThickness), false);
		Stalk->SetStartRoll(Shape.Y, false);
		Stalk->SetEndRoll(Shape.Y + 1.2f, false);
		Stalk->UpdateMesh();
	}

	// The Keimpelz pad lies on top of the forearm, where the camera above sees it
	const FVector Top = (FVector::UpVector - Axis * FVector::DotProduct(FVector::UpVector, Axis)).GetSafeNormal();
	UpdatePad(Elbow, Wrist, Top.IsNearlyZero() ? U : Top);
	UpdateOvergrowth(Shoulder, Elbow, Wrist);

	// The axis the tips twirl around in the material
	ArmMaterial->SetVectorParameterValue(ArmPointParameter, FLinearColor(BodyTransform.TransformPosition(Elbow)));
	ArmMaterial->SetVectorParameterValue(ArmAxisParameter, FLinearColor(BodyTransform.TransformVectorNoScale(Axis)));
}

void UVaelPlantArmComponent::UpdateMaterials(float DeltaTime)
{
	const bool bShadow = GetCharacter()->GetPath() == EVaelPath::Shadow;

	// The winding stalks spin faster the longer the hand fires, and slow down again after
	const float TargetRate = IsFiring() ? Plant->SpinSpeed * FMath::Clamp(FireTime / Plant->SpinRampTime, 0.0f, 1.0f) : 0.0f;
	SpinRate = FMath::FInterpTo(SpinRate, TargetRate, DeltaTime, IsFiring() ? 6.0f : 2.0f);
	SpinTurns = FMath::Fmod(SpinTurns + SpinRate * DeltaTime, 1000.0f);

	const float Wind = Plant->RestWind * (1.0f + Fan);
	for (int32 Finger = 0; Finger < FingerMaterials.Num(); ++Finger)
	{
		Flashes[Finger] = FMath::Max(0.0f, Flashes[Finger] - DeltaTime * 8.0f);

		UMaterialInstanceDynamic* Material = FingerMaterials[Finger];
		Material->SetScalarParameterValue(GrowParameter, Grow);
		Material->SetScalarParameterValue(WindParameter, Wind * (1.0f - Fan * 0.7f));
		Material->SetScalarParameterValue(ShadowParameter, bShadow ? 1.0f : 0.0f);
		Material->SetScalarParameterValue(FlashParameter, Flashes[Finger]);
	}

	ArmMaterial->SetScalarParameterValue(GrowParameter, Grow);
	ArmMaterial->SetScalarParameterValue(WindParameter, Wind);
	// The spiral itself turns in UpdateStalks; in the material the tips lag behind, the faster it turns the more
	ArmMaterial->SetScalarParameterValue(SpinParameter, SpinRate * 0.15f);
	ArmMaterial->SetScalarParameterValue(ShadowParameter, bShadow ? 1.0f : 0.0f);
}

// ---------------------------------------------------------------- Shots

void UVaelPlantArmComponent::UpdateFiring(float DeltaTime)
{
	const float Now = GetWorld()->GetTimeSeconds();
	FireCountdown = FMath::Max(0.0f, FireCountdown - DeltaTime);

	if (!IsFiring() || Grow < 1.0f)
	{
		// Until reloading with Keimpelz exists, a resting hand slowly fills its bar
		if (Now - LastShotTime > Plant->BarRefillDelay)
		{
			BarShots = FMath::Min<float>(Plant->BarSize, BarShots + Plant->BarRefillPerSecond * DeltaTime);
		}
		return;
	}

	FireTime += DeltaTime;

	AVaelCharacter* Character = GetCharacter();
	if (Character->IsDowned() || Character->IsDodging() || IsReloading() || FireCountdown > 0.0f || Stance < 0.6f)
	{
		return;
	}

	// Nothing fires in the camp, like the spells of the mage
	if (AVaelSafeZone::IsSafe(GetWorld(), Character->GetActorLocation()))
	{
		return;
	}

	if (BarShots < 1.0f)
	{
		return;
	}

	if (Character->GetMana() < Plant->SaftPerShot)
	{
		UVaelCombatTextSubsystem::PostPickup(Character, NSLOCTEXT("VaelFloromant", "NoSaft", "Zu wenig Saft"), FLinearColor(FColor(158, 143, 125)));
		FireCountdown = 0.6f;
		return;
	}

	Character->GetAbilitySystemComponent()->SetNumericAttributeBase(UVaelAttributeSet::GetManaAttribute(), Character->GetMana() - Plant->SaftPerShot);
	BarShots -= 1.0f;
	LastShotTime = Now;
	FireCountdown = Plant->FireInterval;

	// The fingers fire in turn, from thumb to little finger
	Shoot(NextFinger);
	NextFinger = (NextFinger + 1) % NumFingers;
}

void UVaelPlantArmComponent::Shoot(int32 Finger)
{
	AVaelCharacter* Character = GetCharacter();
	Flashes[Finger] = 1.0f;

	// Out of the fingertip towards where the player aims, flat over the ground
	const FVector Muzzle = Muzzles[Finger];
	FVector Direction = UVaelFormulaAbility::GetAimDirection(Character);
	Direction.Z = 0.0f;
	Direction = Direction.GetSafeNormal();
	if (Direction.IsNearlyZero())
	{
		Direction = Character->GetActorForwardVector();
	}
	Direction = FRotator(0.0f, FMath::FRandRange(-Plant->ShotSpread, Plant->ShotSpread), 0.0f).RotateVector(Direction);

	FVaelSpellHit Hit;
	Hit.Damage = Plant->ShotDamage;
	Hit.Element = Plant->ShotElement;

	const bool bShadow = Character->GetPath() == EVaelPath::Shadow;
	const FTransform SpawnTransform(Direction.Rotation(), Muzzle);
	AVaelSpellProjectile* Shot = GetWorld()->SpawnActorDeferred<AVaelSpellProjectile>(AVaelSpellProjectile::StaticClass(), SpawnTransform, Character, Character, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (Shot == nullptr)
	{
		return;
	}

	Shot->InitSpell(Hit, Plant->ShotSpeed, Plant->ShotRadius, Plant->ShotRange / Plant->ShotSpeed, 0, Plant->GetColor(bShadow));
	if (Plant->BurstRadius > 0.0f)
	{
		FVaelSpellHit Burst;
		Burst.Damage = Plant->BurstDamage;
		Burst.Element = Plant->ShotElement;
		Shot->SetExplosion(Burst, Plant->BurstRadius);
	}
	Shot->SetEffects(VaelEffects::Load(Plant->ShotElement));
	Shot->FinishSpawning(SpawnTransform);
}

// ---------------------------------------------------------------- Keimpelz

void UVaelPlantArmComponent::UpdatePad(const FVector& Elbow, const FVector& Wrist, const FVector& Out)
{
	if (KeimpelzPad == nullptr)
	{
		return;
	}

	// The fuller the pelt, the bigger the pad; while the stalks draw it in it shrinks and swells back
	const UVaelFloromantSettings* Settings = UVaelFloromantSettings::Get();
	const AVaelCharacter* Character = GetCharacter();
	const float Fill = Character->GetFloromant() != nullptr ? Character->GetFloromant()->GetKeimpelzFraction() : 1.0f;
	const float Progress = IsReloading() ? 1.0f - ReloadTimeLeft / ReloadDuration : 0.0f;
	const float Draw = 1.0f - 0.45f * FMath::Sin(Progress * UE_PI);

	// The moss is a flat clump: its widest side gives the size
	const float MeshHeight = FMath::Max(KeimpelzPad->GetStaticMesh()->GetBoundingBox().GetSize().GetMax(), 1.0f);
	const float Scale = Settings->KeimpelzPadSize / MeshHeight * FMath::Lerp(0.3f, 1.0f, Fill) * Grow * Draw;

	const FVector Place = FMath::Lerp(Elbow, Wrist, 0.55f) + Out * Plant->WrapRadius * 0.6f;
	KeimpelzPad->SetRelativeLocationAndRotation(Place, FRotationMatrix::MakeFromZX(Out, Wrist - Elbow).Rotator());
	KeimpelzPad->SetRelativeScale3D(FVector(Scale * 1.3f, Scale, Scale * 0.7f));
	KeimpelzPad->SetVisibility(Scale > 0.01f);
}

// ---------------------------------------------------------------- Overgrowth

void UVaelPlantArmComponent::UpdateOvergrowth(const FVector& Shoulder, const FVector& Elbow, const FVector& Wrist)
{
	const bool bShown = Overgrowth > 0.0f;
	for (TArray<TObjectPtr<USplineMeshComponent>>* Stalks : { &ForearmVines, &UpperArmVines, &OvergrowthWild })
	{
		for (USplineMeshComponent* Stalk : *Stalks)
		{
			Stalk->SetVisibility(bShown);
		}
	}
	for (UStaticMeshComponent* Pad : OvergrowthPads)
	{
		Pad->SetVisibility(bShown);
	}
	if (!bShown || OvergrowthMaterialA == nullptr)
	{
		return;
	}

	// First the forearm is overrun, then the upper arm
	const bool bShadow = GetCharacter()->GetPath() == EVaelPath::Shadow;
	for (UMaterialInstanceDynamic* Material : { OvergrowthMaterialA.Get(), OvergrowthMaterialB.Get() })
	{
		const bool bUpper = Material == OvergrowthMaterialB;
		Material->SetScalarParameterValue(GrowParameter, FMath::Clamp(Overgrowth * 2.0f - (bUpper ? 1.0f : 0.0f), 0.0f, 1.0f));
		Material->SetScalarParameterValue(WindParameter, Plant->RestWind * 0.6f);
		Material->SetScalarParameterValue(ShadowParameter, bShadow ? 1.0f : 0.0f);
	}

	const FVector LowerAxis = (Elbow - Wrist).GetSafeNormal();
	const FVector UpperAxis = (Shoulder - Elbow).GetSafeNormal();
	const float Radius = Plant->WrapRadius * 1.5f;
	const float Thickness = Plant->VineThickness;

	// A vine winds a third of a turn along one bone, from one joint to the next, a little beyond
	const auto Wind = [Radius, Thickness](USplineMeshComponent* Vine, const FVector& From, const FVector& To, const FVector& Axis, float Angle, float StartScale, float EndScale)
	{
		const FVector U = Across(Axis);
		const FVector V = FVector::CrossProduct(Axis, U);
		const float Turn = 0.35f * 2.0f * UE_PI;
		const FVector Out0 = U * FMath::Cos(Angle) + V * FMath::Sin(Angle);
		const FVector Out1 = U * FMath::Cos(Angle + Turn) + V * FMath::Sin(Angle + Turn);
		const FVector Around0 = -U * FMath::Sin(Angle) + V * FMath::Cos(Angle);
		const FVector Around1 = -U * FMath::Sin(Angle + Turn) + V * FMath::Cos(Angle + Turn);
		const float Length = FVector::Dist(From, To);
		const float Sideways = 4.0f * Radius * 0.35f * 2.0f;
		Vine->SetStartAndEnd(From + Out0 * Radius, Axis * Length + Around0 * Sideways, To + Out1 * Radius, Axis * Length + Around1 * Sideways, false);
		Vine->SetStartScale(FVector2D(StartScale * Thickness), false);
		Vine->SetEndScale(FVector2D(EndScale * Thickness), false);
		Vine->SetStartRoll(Angle, false);
		Vine->SetEndRoll(Angle + Turn, false);
		Vine->UpdateMesh();
	};

	for (int32 Index = 0; Index < ForearmVines.Num(); ++Index)
	{
		const float Angle = Index * 2.0f * UE_PI / ForearmVines.Num() + (bLeft ? 1.1f : 0.3f);
		Wind(ForearmVines[Index], Wrist - LowerAxis * 4.0f, Elbow + LowerAxis * 3.0f, LowerAxis, Angle, 1.3f, 1.0f);
		Wind(UpperArmVines[Index], Elbow - UpperAxis * 3.0f, FMath::Lerp(Elbow, Shoulder, 1.08f), UpperAxis, Angle + 0.35f * 2.0f * UE_PI, 1.1f, 0.7f);
	}

	// Wild stalks out of the upper arm and the shoulder, sticking out and back like heavy armament
	const FVector U = Across(UpperAxis);
	const FVector V = FVector::CrossProduct(UpperAxis, U);
	for (int32 Index = 0; Index < OvergrowthWild.Num(); ++Index)
	{
		const FVector& Shape = OvergrowthShapes[Index];
		const FVector Out = U * FMath::Cos(Shape.Y) + V * FMath::Sin(Shape.Y);
		const FVector Start = FMath::Lerp(Elbow, Shoulder, 0.3f + 0.75f * Shape.X) + Out * Radius;
		const FVector Direction = (Out * 0.8f + UpperAxis * 0.5f).GetSafeNormal();
		const float Length = Plant->OvergrowthWildLength * Shape.Z;

		USplineMeshComponent* Stalk = OvergrowthWild[Index];
		Stalk->SetStartAndEnd(Start, Out * Length, Start + Direction * Length, Direction * Length * 1.3f, false);
		Stalk->SetStartScale(FVector2D(0.6f * Plant->ArmThickness), false);
		Stalk->SetEndScale(FVector2D(0.4f * Plant->ArmThickness), false);
		Stalk->SetStartRoll(Shape.Y, false);
		Stalk->SetEndRoll(Shape.Y + 1.5f, false);
		Stalk->UpdateMesh();
	}

	// More Keimpelz: over the elbow, on the upper arm and on the shoulder, sized by the pelt
	const UVaelFloromantSettings* Settings = UVaelFloromantSettings::Get();
	const float Fill = GetCharacter()->GetFloromant() != nullptr ? GetCharacter()->GetFloromant()->GetKeimpelzFraction() : 1.0f;
	const FVector Top = (FVector::UpVector - UpperAxis * FVector::DotProduct(FVector::UpVector, UpperAxis)).GetSafeNormal();
	const FVector Places[] = { Elbow, FMath::Lerp(Elbow, Shoulder, 0.55f), Shoulder };
	const float Sizes[] = { 1.2f, 1.4f, 1.6f };
	for (int32 Index = 0; Index < OvergrowthPads.Num(); ++Index)
	{
		UStaticMeshComponent* Pad = OvergrowthPads[Index];
		const float MeshSize = FMath::Max(Pad->GetStaticMesh()->GetBoundingBox().GetSize().GetMax(), 1.0f);
		const float Appear = FMath::Clamp(Overgrowth * 3.0f - Index * 0.6f, 0.0f, 1.0f);
		const float Scale = Settings->KeimpelzPadSize * Sizes[Index] / MeshSize * FMath::Lerp(0.3f, 1.0f, Fill) * Appear;
		const FVector Out = (Top.IsNearlyZero() ? U : Top);
		Pad->SetRelativeLocationAndRotation(Places[Index] + Out * Radius * 0.8f, FRotationMatrix::MakeFromZX(Out, UpperAxis).Rotator());
		Pad->SetRelativeScale3D(FVector(Scale * 1.3f, Scale, Scale * 0.7f));
	}
}
