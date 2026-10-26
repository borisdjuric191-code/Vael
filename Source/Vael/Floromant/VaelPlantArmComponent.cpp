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
#include "Floromant/VaelSapJet.h"
#include "Magic/VaelElementSource.h"
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
	const FName TipFlashParameter(TEXT("Aufleuchten"));
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
	StopJet();
	PumpStartTime = -100.0f;
	bPumpLoop = false;
	HoldTime = 0.0f;
	bPumpedThisPress = false;
	PendingShotTime = -1.0f;
	Barrel = 0.0f;
	LeafShapes.Reset();
	PetalShapes.Reset();
	for (TArray<TObjectPtr<UStaticMeshComponent>>* Pieces : { &FingerTips, &OvergrowthPads, &Leaves, &Petals })
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

		// A rose takes the place of the heads
		UStaticMesh* TipMesh = HasRose() ? nullptr : VaelAssets::LoadOptional(Plant->TipMesh);
		if (TipMesh != nullptr)
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

	// Funnel leaves along the arm, from the shoulder to the wrist, each turned its own way round the arm
	if (UStaticMesh* LeafMesh = VaelAssets::LoadOptional(Plant->LeafMesh))
	{
		for (int32 Index = 0; Index < Plant->LeafCount; ++Index)
		{
			const float Share = (Index + 0.5f) / Plant->LeafCount;
			LeafShapes.Add(FVector(FMath::Clamp(Share + Random.FRandRange(-0.04f, 0.04f), 0.08f, 0.92f), Index * 2.4f + Random.FRandRange(-0.4f, 0.4f) + Offset, Random.FRandRange(0.8f, 1.15f)));
			UStaticMeshComponent* Leaf = NewObject<UStaticMeshComponent>(GetOwner());
			Leaf->SetMobility(EComponentMobility::Movable);
			Leaf->SetStaticMesh(LeafMesh);
			Leaf->SetMaterial(0, ArmMaterial);
			Leaf->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			Leaf->SetCanEverAffectNavigation(false);
			Leaf->SetupAttachment(GetBody());
			Leaf->RegisterComponent();
			Leaves.Add(Leaf);
		}
	}

	// The rose at the hand: an inner ring of smaller petals and an outer ring of large ones, set off against each other
	if (UStaticMesh* PetalMesh = HasRose() ? VaelAssets::LoadOptional(Plant->PetalMesh) : nullptr)
	{
		for (int32 Ring = 0; Ring < 2; ++Ring)
		{
			const int32 Count = Ring == 0 ? Plant->InnerPetals : Plant->OuterPetals;
			for (int32 Index = 0; Index < Count; ++Index)
			{
				const float Angle = 2.0f * UE_PI * (Index + 0.5f * Ring) / FMath::Max(Count, 1) + Random.FRandRange(-0.12f, 0.12f) + Offset * 0.4f;
				PetalShapes.Add(FVector(Ring, Angle, Random.FRandRange(0.88f, 1.1f)));
				UStaticMeshComponent* Petal = NewObject<UStaticMeshComponent>(GetOwner());
				Petal->SetMobility(EComponentMobility::Movable);
				Petal->SetStaticMesh(PetalMesh);
				Petal->SetMaterial(0, ArmMaterial);
				Petal->SetCollisionEnabled(ECollisionEnabled::NoCollision);
				Petal->SetCanEverAffectNavigation(false);
				Petal->SetupAttachment(GetBody());
				Petal->RegisterComponent();
				Petals.Add(Petal);
			}
		}
	}

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

	// After a tower the plant grows back into the hand
	if (RegrowTimeLeft > 0.0f)
	{
		RegrowTimeLeft -= DeltaTime;
		if (RegrowTimeLeft <= 0.0f)
		{
			RegrowTimeLeft = 0.0f;
			SetPlant(RegrowPlant);
			RegrowPlant = nullptr;
		}
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

	// A water hand keeps aiming until its pumped shot is out and its jet has torn off
	const bool bWaterHand = !Plant->WaterSource.IsNull();
	const bool bShieldHand = Plant->bShield;
	const bool bShieldUp = bShieldHand && IsFiring() && Grow >= 1.0f;
	const bool bFiringNow = !bShieldHand && (IsFiring() || (bWaterHand && (PendingShotTime >= 0.0f || Jet != nullptr))) && Grow >= 1.0f;
	Stance = FMath::FInterpTo(Stance, bFiringNow ? 1.0f : 0.0f, DeltaTime, bFiringNow ? 12.0f : 5.0f);

	// A shield hand doesn't aim: the forearm comes up before the face, and the clover overgrows the arm as if it fired
	ShieldStance = FMath::FInterpTo(ShieldStance, bShieldUp ? 1.0f : 0.0f, DeltaTime, bShieldUp ? 10.0f : 6.0f);
	Fan = FMath::FInterpTo(Fan, bFiringNow ? 1.0f : 0.0f, DeltaTime, 10.0f);
	Barrel = Plant->bFingersFormBarrel || HasRose() ? FMath::FInterpTo(Barrel, bFiringNow ? 1.0f : 0.0f, DeltaTime, bFiringNow ? 14.0f : 6.0f) : 0.0f;

	// While the hand aims the plant overruns the whole arm; it falls back more slowly when the hand rests
	Overgrowth = bFiringNow || bShieldUp
		? FMath::Min(1.0f, Overgrowth + DeltaTime / Plant->OvergrowthTime)
		: FMath::Max(0.0f, Overgrowth - DeltaTime / Plant->OvergrowthFadeTime);

	// The stalks first, so the barrel is where the water leaves
	UpdateStalks();
	if (bShieldHand)
	{
		// The dome is raised by the Floromant, who knows what the other hand does
	}
	else if (bWaterHand)
	{
		UpdateWaterFiring(DeltaTime);
	}
	else
	{
		UpdateFiring(DeltaTime);
	}
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

	// The barrel: the finger heads nest into each other in a line in front of the hand, towards where the player aims, each
	// funnel's foot inside the cup of the one before; the stems behind them bend in from the forearm
	FVector AimLocal = HandAxis;
	if (Barrel > 0.0f)
	{
		FVector Aim = UVaelFormulaAbility::GetAimDirection(GetCharacter());
		Aim.Z = 0.0f;
		Aim = Aim.IsNearlyZero() ? GetCharacter()->GetActorForwardVector() : Aim.GetSafeNormal();
		AimLocal = BodyTransform.InverseTransformVectorNoScale(Aim).GetSafeNormal();
	}
	const FVector BarrelBase = BoneLocation(TEXT("middle_01")) + AimLocal * 4.0f;
	// From the hand outwards: index, middle, ring, little finger, thumb in front
	const int32 BarrelRank[] = { 4, 0, 1, 2, 3 };

	// A rose sits on the hand along the middle line of the hand; folding shut it turns towards the aim. The finger stalks are
	// short and end in its foot, like the green under a blossom
	const bool bRose = !Petals.IsEmpty();
	const FVector RoseAxis = FMath::Lerp(HandAxis, AimLocal, Barrel).GetSafeNormal();
	const FVector RoseBase = BoneLocation(TEXT("middle_01")) + RoseAxis * 2.0f;

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
		FVector End = Tip + (Direction + Curl * 0.5f).GetSafeNormal() * Plant->FingerLength * LengthShare;
		if (bRose)
		{
			FVector Out = Root - RoseBase;
			Out = (Out - RoseAxis * FVector::DotProduct(Out, RoseAxis)).GetSafeNormal();
			End = RoseBase + Out * Plant->RoseHoleRadius * 1.6f - RoseAxis * 1.5f;
			Direction = RoseAxis;
		}
		else if (Barrel > 0.0f)
		{
			const FVector InBarrel = BarrelBase + AimLocal * Plant->BarrelSpacing * BarrelRank[Finger];
			End = FMath::Lerp(End, InBarrel, Barrel);
			Direction = FMath::Lerp(Direction, AimLocal, Barrel).GetSafeNormal();
		}
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

	// The water leaves at the rim of the front funnel; the pump wave swells the funnels of the barrel as it passes
	const FVector BarrelFront = bRose ? UpdateRose(RoseBase, RoseAxis) : BarrelBase + AimLocal * (Plant->BarrelSpacing * (NumFingers - 1) + Plant->TipSize);
	BarrelMuzzle = BodyTransform.TransformPosition(BarrelFront);
	BarrelDirection = BodyTransform.TransformVectorNoScale(AimLocal);
	if (Barrel > 0.0f)
	{
		for (int32 Finger = 0; Finger < FingerTips.Num(); ++Finger)
		{
			const float Share = 1.0f + 0.04f * BarrelRank[Finger];
			FingerTips[Finger]->SetRelativeScale3D(FVector(Plant->TipSize / 10.0f * Grow * (1.0f + GetPumpSwell(Share) * Barrel)));
		}
	}
	UpdateLeaves(Shoulder, Elbow, Wrist);

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
		Material->SetScalarParameterValue(TipFlashParameter, Flashes[Finger]);
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
	const bool bBarrel = !Flashes.IsValidIndex(Finger);
	if (bBarrel)
	{
		for (float& Flash : Flashes)
		{
			Flash = 0.6f;
		}
	}
	else
	{
		Flashes[Finger] = 1.0f;
	}

	// Out of the fingertip, or the front of the barrel, towards where the player aims, flat over the ground
	const FVector Muzzle = bBarrel ? BarrelMuzzle : Muzzles[Finger];
	FVector Direction = UVaelFormulaAbility::GetAimDirection(Character);
	Direction.Z = 0.0f;
	Direction = Direction.GetSafeNormal();
	if (Direction.IsNearlyZero())
	{
		Direction = Character->GetActorForwardVector();
	}
	Direction = FRotator(0.0f, FMath::FRandRange(-Plant->ShotSpread, Plant->ShotSpread), 0.0f).RotateVector(Direction);

	// Rooted in the earth, her hands hit harder
	const float Power = Character->GetFloromant() != nullptr ? Character->GetFloromant()->GetHandPower() : 1.0f;

	FVaelSpellHit Hit;
	Hit.Damage = Plant->ShotDamage * Power;
	Hit.Element = Plant->ShotElement;
	Hit.Status = Plant->ShotStatus;
	Hit.StatusDuration = Plant->ShotStatusDuration;

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
		Burst.Damage = Plant->BurstDamage * Power;
		Burst.Element = Plant->ShotElement;
		Shot->SetExplosion(Burst, Plant->BurstRadius);
	}
	// Water hands shoot balls of their own real water, which waters the plants where it bursts
	if (UVaelElementSource* Source = VaelAssets::LoadOptional(Plant->WaterSource))
	{
		Shot->SetLook(EVaelProjectileLook::WaterOrb);
		Shot->SetWaterSource(Source);
		Shot->SetWatersPlants(Plant->WaterRadius, Plant->WaterPerShot);
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

// ---------------------------------------------------------------- Funnel leaves and pump (Taublatt)

float UVaelPlantArmComponent::GetPumpPosition() const
{
	const float Wave = (GetWorld()->GetTimeSeconds() - PumpStartTime) / FMath::Max(Plant->PumpWaveTime, 0.05f);
	if (Wave < 0.0f || (!bPumpLoop && Wave > 1.3f))
	{
		return -10.0f;
	}
	// It runs from a little before the shoulder to a little past the barrel
	return (bPumpLoop ? FMath::Frac(Wave) : Wave) * 1.3f - 0.1f;
}

float UVaelPlantArmComponent::GetPumpSwell(float Share) const
{
	const float Distance = (Share - GetPumpPosition()) / 0.13f;
	return Plant->PumpSwell * FMath::Exp(-Distance * Distance);
}

void UVaelPlantArmComponent::PlantAway(float Seconds)
{
	RegrowPlant = bChanging ? NextPlant.Get() : Plant.Get();
	RegrowDuration = FMath::Max(Seconds, 0.01f);
	RegrowTimeLeft = RegrowDuration;
	SetFiring(false);
	SetPlant(nullptr);
}

FVector UVaelPlantArmComponent::GetHandLocation() const
{
	const USkeletalMeshComponent* Body = GetBody();
	return Body != nullptr ? Body->GetSocketLocation(Bone(TEXT("hand"))) : FVector::ZeroVector;
}

bool UVaelPlantArmComponent::HasRose() const
{
	return Plant != nullptr && !Plant->PetalMesh.IsNull();
}

FVector UVaelPlantArmComponent::UpdateRose(const FVector& Base, const FVector& Axis)
{
	const FVector U = Across(Axis);
	const FVector V = FVector::CrossProduct(Axis, U);
	const float Time = GetWorld()->GetTimeSeconds();
	const float Hole = Plant->RoseHoleRadius;

	for (int32 Index = 0; Index < Petals.Num(); ++Index)
	{
		const FVector& Shape = PetalShapes[Index];
		const bool bInner = Shape.X < 0.5f;
		const float Size = Plant->PetalSize * (bInner ? 0.72f : 1.0f) * Shape.Z;

		// Open they lean out like a blossom and breathe a little; shut they lean in, the inner ring around the hole, the outer
		// ring wrapped over it, all of them twisted a little round the axis like the petals of a rose
		const float Open = FMath::DegreesToRadians(Plant->RoseOpenAngle * (bInner ? 0.75f : 1.0f) + 3.0f * FMath::Sin(Time * 1.7f + Index));
		const float Shut = -FMath::DegreesToRadians(Plant->RoseClosedAngle * (bInner ? 1.25f : 0.7f));
		const float Lean = FMath::Lerp(Open, Shut, Barrel);
		const FVector Out = U * FMath::Cos(Shape.Y) + V * FMath::Sin(Shape.Y);
		const FVector Round = FVector::CrossProduct(Axis, Out);

		// The foot stands far enough out that the tips of the shut inner ring leave the hole open
		const float FootRadius = bInner ? Hole + Size * FMath::Sin(-Shut) * 0.9f : Hole + Size * 0.45f;
		const FVector Foot = Base + Out * FootRadius * FMath::Lerp(0.6f, 1.0f, Barrel) - Axis * (bInner ? 0.0f : 1.5f);
		const FVector Up = (Axis * FMath::Cos(Lean) + Out * FMath::Sin(Lean) + Round * 0.18f * Barrel).GetSafeNormal();
		const FVector Outside = (Out - Up * FVector::DotProduct(Out, Up)).GetSafeNormal();

		// The pump wave swells the petals as it reaches the hand
		const float Swell = 1.0f + GetPumpSwell(1.0f + 0.03f * Shape.X) * Barrel;
		Petals[Index]->SetRelativeLocationAndRotation(Foot, FRotationMatrix::MakeFromXZ(Outside, Up).Rotator());
		Petals[Index]->SetRelativeScale3D(FVector(Size / 10.0f * Grow * Swell));
	}

	// The shots leave through the hole at the front of the rose
	return Base + Axis * Plant->PetalSize * 0.85f;
}

void UVaelPlantArmComponent::StartPump(bool bLoop)
{
	PumpStartTime = GetWorld()->GetTimeSeconds();
	bPumpLoop = bLoop;
}

void UVaelPlantArmComponent::UpdateLeaves(const FVector& Shoulder, const FVector& Elbow, const FVector& Wrist)
{
	if (Leaves.IsEmpty())
	{
		return;
	}

	// The way from the shoulder to the hand: the upper arm takes the first part, the forearm the rest
	const float Upper = FVector::Dist(Shoulder, Elbow);
	const float Lower = FVector::Dist(Elbow, Wrist);
	const float UpperShare = Upper / FMath::Max(Upper + Lower, 1.0f);
	const float Open = FMath::DegreesToRadians(Plant->LeafOpenAngle) * (1.0f - Stance) + FMath::DegreesToRadians(8.0f) * Stance;
	const float Radius = Plant->WrapRadius * FMath::Lerp(1.3f, 0.75f, Stance);

	for (int32 Index = 0; Index < Leaves.Num(); ++Index)
	{
		const FVector& Shape = LeafShapes[Index];
		const bool bOnUpper = Shape.X < UpperShare;
		const FVector From = bOnUpper ? Shoulder : Elbow;
		const FVector To = bOnUpper ? Elbow : Wrist;
		const float Along = bOnUpper ? Shape.X / FMath::Max(UpperShare, 0.01f) : (Shape.X - UpperShare) / FMath::Max(1.0f - UpperShare, 0.01f);
		const FVector Axis = (To - From).GetSafeNormal();
		const FVector U = Across(Axis);
		const FVector V = FVector::CrossProduct(Axis, U);
		const FVector Out = U * FMath::Cos(Shape.Y) + V * FMath::Sin(Shape.Y);

		// The cup looks towards the hand: open at rest it stands off the arm, while aiming it lies flat against it
		const FVector Opening = (Axis * FMath::Cos(Open) + Out * FMath::Sin(Open)).GetSafeNormal();
		const float Swell = 1.0f + GetPumpSwell(Shape.X);
		const float Size = Plant->LeafSize / 10.0f * Shape.Z * Grow * Swell;

		UStaticMeshComponent* Leaf = Leaves[Index];
		Leaf->SetRelativeLocationAndRotation(FMath::Lerp(From, To, Along) + Out * Radius, FRotationMatrix::MakeFromZX(Opening, Out).Rotator());
		Leaf->SetRelativeScale3D(FVector(Size, Size, Size * FMath::Lerp(1.0f, 0.85f, Stance)));
		Leaf->SetVisibility(Size > 0.01f);
	}
}

// ---------------------------------------------------------------- Water: Tauperle and jet

void UVaelPlantArmComponent::StopJet()
{
	if (Jet != nullptr)
	{
		Jet->Stop();
		Jet = nullptr;
	}
	bPumpLoop = false;
}

void UVaelPlantArmComponent::UpdateWaterFiring(float DeltaTime)
{
	const float Now = GetWorld()->GetTimeSeconds();
	FireCountdown = FMath::Max(0.0f, FireCountdown - DeltaTime);
	AVaelCharacter* Character = GetCharacter();

	// What stops the water: no hand, a dodge, a reload, the camp
	const bool bBlocked = Grow < 1.0f || Character->IsDowned() || Character->IsDodging() || IsReloading() || AVaelSafeZone::IsSafe(GetWorld(), Character->GetActorLocation());
	if (bBlocked)
	{
		StopJet();
		PendingShotTime = -1.0f;
		HoldTime = 0.0f;
		return;
	}

	// The pumped Tauperle leaves the barrel when the wave arrives at the hand
	if (PendingShotTime >= 0.0f && Now >= PendingShotTime)
	{
		PendingShotTime = -1.0f;
		if (BarShots >= 1.0f && Character->GetMana() >= Plant->SaftPerShot)
		{
			Character->GetAbilitySystemComponent()->SetNumericAttributeBase(UVaelAttributeSet::GetManaAttribute(), Character->GetMana() - Plant->SaftPerShot);
			BarShots -= 1.0f;
			LastShotTime = Now;
			Shoot(-1);
		}
	}

	if (!IsFiring())
	{
		StopJet();
		HoldTime = 0.0f;
		bPumpedThisPress = false;
		return;
	}
	HoldTime += DeltaTime;

	// A press pumps one Tauperle: the wave runs from the shoulder down, the shot leaves when it reaches the barrel
	if (!bPumpedThisPress && FireCountdown <= 0.0f)
	{
		bPumpedThisPress = true;
		if (BarShots < 1.0f)
		{
			return;
		}
		if (Character->GetMana() < Plant->SaftPerShot)
		{
			UVaelCombatTextSubsystem::PostPickup(Character, NSLOCTEXT("VaelFloromant", "NoSaft", "Zu wenig Saft"), FLinearColor(FColor(158, 143, 125)));
			return;
		}
		StartPump(false);
		PendingShotTime = Now + Plant->PumpWaveTime;
		FireCountdown = FMath::Max(Plant->FireInterval, Plant->PumpWaveTime);
	}

	// Held long enough it turns into the jet, the pump going on and on; it costs bar and Saft every second, and sputters out when they are gone
	if (Plant->JetHoldTime <= 0.0f || HoldTime < Plant->JetHoldTime || PendingShotTime >= 0.0f)
	{
		return;
	}
	const float BarCost = Plant->JetShotsPerSecond * DeltaTime;
	const float SaftCost = Plant->JetSaftPerSecond * DeltaTime;
	if (BarShots < BarCost || Character->GetMana() < SaftCost)
	{
		StopJet();
		return;
	}
	if (Jet == nullptr)
	{
		Jet = AVaelSapJet::StartJet(Character, Plant, BarrelMuzzle, BarrelDirection);
		StartPump(true);
	}
	if (Jet == nullptr)
	{
		return;
	}
	Character->GetAbilitySystemComponent()->SetNumericAttributeBase(UVaelAttributeSet::GetManaAttribute(), Character->GetMana() - SaftCost);
	BarShots -= BarCost;
	LastShotTime = Now;

	// Each wave arriving at the barrel pushes the jet harder for a moment
	const float Arrival = FMath::Exp(-FMath::Square((GetPumpPosition() - 1.0f) / 0.15f));
	Jet->Aim(BarrelMuzzle, BarrelDirection, Arrival);
}
