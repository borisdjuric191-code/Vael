// Copyright Epic Games, Inc. All Rights Reserved.

#include "Engineer/VaelEngineerComponent.h"

#include "AbilitySystemComponent.h"
#include "Combat/VaelAttributeSet.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Combat/VaelCombatStatics.h"
#include "Engineer/VaelEngineerDevice.h"
#include "Engineer/VaelEngineerShot.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "EngineUtils.h"
#include "Items/VaelMaterialDrop.h"
#include "Creatures/VaelCreatureData.h"
#include "Creatures/VaelCreature.h"
#include "Items/VaelMaterial.h"
#include "Items/VaelMaterialBag.h"
#include "Player/VaelCharacter.h"
#include "Save/VaelSaveGame.h"
#include "Save/VaelSaveSubsystem.h"
#include "UI/VaelCombatTextSubsystem.h"
#include "Vael.h"
#include "Magic/VaelMagicSettings.h"
#include "Magic/VaelSpellEffects.h"
#include "NiagaraSystem.h"
#include "VaelAssets.h"
#include "World/VaelBalance.h"
#include "Engine/SkeletalMesh.h"
#include "TimerManager.h"

#define LOCTEXT_NAMESPACE "VaelEngineer"

namespace
{
	constexpr int32 MaxPlaced = 6;
	constexpr float PlaceReach = 700.0f;
	constexpr int32 NumArmorPlaces = 5;

	// The tower shield: five rows of Aschenroller bands, 20 cm each; how fast it slides open, and the moment after raising it
	// that blocks perfectly
	constexpr int32 ShieldRows = 5;
	constexpr float ShieldRowHeight = 20.0f;
	constexpr float ShieldOpenSeconds = 0.18f;
	constexpr float ShieldPerfectSeconds = 0.35f;
}

UVaelEngineerComponent::UVaelEngineerComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickInterval = 0.25f;
	ResetToStart();
}

void UVaelEngineerComponent::ResetToStart()
{
	Built.Reset();
	Built.Add(VaelEngineerGear::Sling);
	Carried.Reset();
	Stock.Reset();
	Worn.Init(NAME_None, NumArmorPlaces);
	Weapon = VaelEngineerGear::Sling;
	Ammo = VaelEngineerGear::Standard;
	Device = NAME_None;
	WeaponTiers.Reset();
	WeaponBranches.Reset();
	Plans.Reset();
	Formulas.Reset();
	Tried.Reset();
	Pockets.Reset();
	DelayedBlasts.Reset();
	Fields.Reset();
}

AVaelCharacter* UVaelEngineerComponent::GetPlayer() const
{
	return Cast<AVaelCharacter>(GetOwner());
}

float UVaelEngineerComponent::Now() const
{
	return GetWorld() != nullptr ? GetWorld()->GetTimeSeconds() : 0.0f;
}

void UVaelEngineerComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	// Poisons bite every half second
	if (!Poisons.IsEmpty() && Now() >= NextPoisonTick)
	{
		NextPoisonTick = Now() + 0.5f;
		for (int32 Index = Poisons.Num() - 1; Index >= 0; --Index)
		{
			const FPoison& Poison = Poisons[Index];
			if (!Poison.Target.IsValid() || Poison.Target->GetHealth() <= 0.0f || Now() > Poison.End)
			{
				Poisons.RemoveAtSwap(Index);
				continue;
			}
			UVaelCombatStatics::DealDamage(GetOwner(), Poison.Target.Get(), Poison.DamagePerSecond * 0.5f, Poison.Element);
		}
	}
	TickEffects();
	if (bShieldMoving)
	{
		TickShield();
	}

	Placed.RemoveAll([](const TWeakObjectPtr<AVaelEngineerDevice>& Each) { return !Each.IsValid(); });
	UpdateWeaponMesh();
	UpdateSling(DeltaTime);
	UpdateCarriedMeshes();
	UpdateArmorMeshes();

	// A reach that is done: from the backpack the pouch fills up first
	if (IsGrabbing() && Now() >= GrabEnd)
	{
		if (bGrabFromPack)
		{
			if (const FVaelGear* Gear = VaelEngineerGear::Find(GrabAmmo))
			{
				const int32 Take = FMath::Min(GetCarryLimit(*Gear) - GetCarried(GrabAmmo), GetStock(GrabAmmo));
				Carried.FindOrAdd(GrabAmmo) += FMath::Max(Take, 0);
				Stock.FindOrAdd(GrabAmmo) -= FMath::Max(Take, 0);
			}
		}
		Ammo = GetCarried(GrabAmmo) > 0 ? GrabAmmo : VaelEngineerGear::Standard;
		GrabAmmo = NAME_None;
		UVaelCombatTextSubsystem::PostPickup(GetOwner(), GetAmmoName(), GetAmmoColor());
	}
}

UStaticMeshComponent* UVaelEngineerComponent::AttachPiece(UStaticMeshComponent* Existing, const TCHAR* Name, const TCHAR* Piece, FName Bone, const FTransform& InActorSpace)
{
	AVaelCharacter* Player = GetPlayer();
	UStaticMesh* Mesh = LoadObject<UStaticMesh>(nullptr, *FString::Printf(TEXT("/Game/Vael/Characters/Ingenieur/Werkstuecke/%s.%s"), Piece, Piece), nullptr, LOAD_NoWarn | LOAD_Quiet);
	if (Player == nullptr || Mesh == nullptr)
	{
		return Existing;
	}
	UStaticMeshComponent* Component = Existing;
	if (Component == nullptr)
	{
		Component = NewObject<UStaticMeshComponent>(Player, Name);
		Component->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Component->SetCastShadow(true);
		Component->RegisterComponent();
	}
	// Hung on a bone of the base look (the MetaHuman follows its pose); placed where it belongs on the body as the pose is now
	USkeletalMeshComponent* Body = Player->GetMesh();
	const FName Socket = Body->DoesSocketExist(Bone) ? Bone : NAME_None;
	Component->AttachToComponent(Body, FAttachmentTransformRules::SnapToTargetNotIncludingScale, Socket);
	// The bone as it stands in the reference pose: a piece placed in the middle of a run or a throw would hang crooked for good
	FTransform BoneInActor = Body->GetRelativeTransform();
	const int32 BoneIndex = Socket.IsNone() || Body->GetSkeletalMeshAsset() == nullptr ? INDEX_NONE : Body->GetSkeletalMeshAsset()->GetRefSkeleton().FindBoneIndex(Socket);
	if (BoneIndex != INDEX_NONE)
	{
		const FReferenceSkeleton& Skeleton = Body->GetSkeletalMeshAsset()->GetRefSkeleton();
		FTransform InComponent = FTransform::Identity;
		for (int32 Index = BoneIndex; Index != INDEX_NONE; Index = Skeleton.GetParentIndex(Index))
		{
			InComponent = InComponent * Skeleton.GetRefBonePose()[Index];
		}
		BoneInActor = InComponent * Body->GetRelativeTransform();
	}
	else if (!Socket.IsNone())
	{
		BoneInActor = Body->GetSocketTransform(Socket, RTS_Actor);
	}
	FTransform Relative = InActorSpace.GetRelativeTransform(BoneInActor);
	Relative.SetScale3D(FVector::OneVector);
	Component->SetRelativeTransform(Relative);
	Component->SetStaticMesh(Mesh);
	Component->SetVisibility(true);
	return Component;
}

UStaticMeshComponent* UVaelEngineerComponent::AttachLimbPiece(int32 Slot, const FString& Piece, FName From, FName To, float Along)
{
	AVaelCharacter* Player = GetPlayer();
	USkeletalMeshComponent* Body = Player != nullptr ? Player->GetMesh() : nullptr;
	if (Body == nullptr || !Body->DoesSocketExist(From))
	{
		return nullptr;
	}
	ArmorMeshes.SetNum(FMath::Max(ArmorMeshes.Num(), Slot + 1));
	// A sleeve along the bone, from the joint towards the next one, placed as the pose is now and then carried by the bone
	const FVector Start = Body->GetSocketTransform(From, RTS_Actor).GetLocation();
	const FVector End = Body->DoesSocketExist(To) ? Body->GetSocketTransform(To, RTS_Actor).GetLocation() : Start - FVector(0.0f, 0.0f, 25.0f);
	const FVector Axis = (End - Start).GetSafeNormal();
	const FQuat Turn = FQuat::FindBetweenNormals(FVector::UpVector, Axis);
	ArmorMeshes[Slot] = AttachPiece(ArmorMeshes[Slot], *FString::Printf(TEXT("EngineerArmor%d"), Slot), *Piece, From, FTransform(Turn, FMath::Lerp(Start, End, Along)));
	return ArmorMeshes[Slot];
}

void UVaelEngineerComponent::UpdateArmorMeshes()
{
	AVaelCharacter* Player = GetPlayer();
	if (Player == nullptr)
	{
		return;
	}
	FString Key = Player->IsEngineer() ? FString::JoinBy(Worn, TEXT(","), [](const FName& Id) { return Id.ToString(); }) : FString();
	if (Key == ShownArmor)
	{
		return;
	}
	ShownArmor = Key;
	UpdateShieldMeshes();
	for (UStaticMeshComponent* Piece : ArmorMeshes)
	{
		if (Piece != nullptr)
		{
			Piece->SetVisibility(false);
		}
	}
	if (!Player->IsEngineer())
	{
		return;
	}
	// Each worn piece in the look of its creature (Tools/Ingenieur/vael_ingenieur_werkstuecke.py, VAEL_ING_RUESTUNG)
	const USkeletalMeshComponent* Body = Player->GetMesh();
	const auto PieceName = [this](EVaelArmorPlace Place, const TCHAR* Part) -> FString
	{
		const FVaelGear* Gear = GetWorn(Place).IsNone() ? nullptr : VaelEngineerGear::Find(GetWorn(Place));
		return Gear != nullptr ? FString::Printf(TEXT("SM_Ing_R_%s_%s"), *Gear->Set.ToString(), Part) : FString();
	};
	const FString Head = PieceName(EVaelArmorPlace::Head, TEXT("Kopf"));
	if (!Head.IsEmpty() && Body->DoesSocketExist(TEXT("head")))
	{
		ArmorMeshes.SetNum(FMath::Max(ArmorMeshes.Num(), 6));
		const FVector At = Body->GetSocketTransform(TEXT("head"), RTS_Actor).GetLocation() + FVector(2.0f, 0.0f, 15.0f);
		ArmorMeshes[0] = AttachPiece(ArmorMeshes[0], TEXT("EngineerArmor0"), *Head, TEXT("head"), FTransform(FRotator::ZeroRotator, At));
	}
	const FString Chest = PieceName(EVaelArmorPlace::Chest, TEXT("Brust"));
	const FName Spine = Body->DoesSocketExist(TEXT("spine_04")) ? FName(TEXT("spine_04")) : FName(TEXT("spine_03"));
	if (!Chest.IsEmpty() && Body->DoesSocketExist(Spine))
	{
		ArmorMeshes.SetNum(FMath::Max(ArmorMeshes.Num(), 6));
		const FVector At = Body->GetSocketTransform(Spine, RTS_Actor).GetLocation() + FVector(0.0f, 0.0f, 16.0f);
		ArmorMeshes[1] = AttachPiece(ArmorMeshes[1], TEXT("EngineerArmor1"), *Chest, Spine, FTransform(FRotator::ZeroRotator, At));
	}
	const FString Arm = PieceName(EVaelArmorPlace::Arms, TEXT("Arm"));
	if (!Arm.IsEmpty())
	{
		AttachLimbPiece(2, Arm, TEXT("lowerarm_l"), TEXT("hand_l"), 0.55f);
		AttachLimbPiece(3, Arm, TEXT("lowerarm_r"), TEXT("hand_r"), 0.55f);
	}
	const FString Leg = PieceName(EVaelArmorPlace::Legs, TEXT("Bein"));
	if (!Leg.IsEmpty())
	{
		AttachLimbPiece(4, Leg, TEXT("calf_l"), TEXT("foot_l"), 0.45f);
		AttachLimbPiece(5, Leg, TEXT("calf_r"), TEXT("foot_r"), 0.45f);
		// sets with plates over the thighs too (Schwarzglut); the others have none, then nothing is hung there
		const FString Thigh = PieceName(EVaelArmorPlace::Legs, TEXT("Oberschenkel"));
		AttachLimbPiece(6, Thigh, TEXT("thigh_l"), TEXT("calf_l"), 0.5f);
		AttachLimbPiece(7, Thigh, TEXT("thigh_r"), TEXT("calf_r"), 0.5f);
	}
}

void UVaelEngineerComponent::UpdateShieldMeshes()
{
	AVaelCharacter* Player = GetPlayer();
	USkeletalMeshComponent* Body = Player != nullptr ? Player->GetMesh() : nullptr;
	const FName Arm(TEXT("lowerarm_l"));
	const FName Hand(TEXT("hand_l"));
	if (!HasShield() || Body == nullptr || !Body->DoesSocketExist(Arm) || !Body->DoesSocketExist(Hand))
	{
		if (ShieldHub != nullptr)
		{
			ShieldHub->SetVisibility(false, true);
		}
		bShieldRaised = false;
		ShieldOpen = 0.0f;
		return;
	}
	const auto Load = [](const TCHAR* Piece)
	{
		return LoadObject<UStaticMesh>(nullptr, *FString::Printf(TEXT("/Game/Vael/Characters/Ingenieur/Werkstuecke/%s.%s"), Piece, Piece), nullptr, LOAD_NoWarn | LOAD_Quiet);
	};
	if (ShieldHub == nullptr)
	{
		ShieldHub = NewObject<UStaticMeshComponent>(Player, TEXT("EngineerShieldHub"));
		ShieldHub->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		ShieldHub->SetStaticMesh(Load(TEXT("SM_Ing_R_Schwarzglut_Schild_Nabe")));
		ShieldHub->SetupAttachment(Body, Arm);
		ShieldHub->RegisterComponent();
		for (int32 Index = 0; Index < ShieldRows; ++Index)
		{
			UStaticMeshComponent* Each = NewObject<UStaticMeshComponent>(Player, *FString::Printf(TEXT("EngineerShieldBand%d"), Index));
			Each->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			Each->SetStaticMesh(Load(*FString::Printf(TEXT("SM_Ing_R_Schwarzglut_Schild_Teil_%d"), Index)));
			Each->SetupAttachment(ShieldHub);
			Each->RegisterComponent();
			ShieldBands.Add(Each);
		}
	}
	// Folded: on the outside of the forearm by the wrist, the rows slid into each other and lying along it. Worked out in the bone's own
	// space, where the hand stays put whatever the pose; "outside" is the left of the body
	const FTransform ArmInBody = Body->GetSocketTransform(Arm, RTS_Component);
	const FVector HandInArm = Body->GetSocketTransform(Hand, RTS_Component).GetRelativeTransform(ArmInBody).GetLocation();
	const FVector Along = HandInArm.GetSafeNormal();
	const FVector LeftInBody = Body->GetComponentTransform().InverseTransformVectorNoScale(-Player->GetActorRightVector());
	FVector Out = ArmInBody.InverseTransformVectorNoScale(LeftInBody);
	Out = (Out - Along * FVector::DotProduct(Out, Along)).GetSafeNormal();
	if (Out.IsNearlyZero())
	{
		Out = FVector::CrossProduct(Along, FVector::UpVector).GetSafeNormal();
	}
	ShieldFolded = FTransform(FRotationMatrix::MakeFromXY(Out, Along).ToQuat(), HandInArm * 0.6f + Out * 6.0f);
	ShieldHub->SetVisibility(true, true);
	ShieldHub->SetRelativeTransform(ShieldFolded);
	for (int32 Index = 0; Index < ShieldBands.Num(); ++Index)
	{
		ShieldBands[Index]->SetRelativeTransform(FTransform(FQuat::Identity, FVector(-0.6f * Index, 0.0f, 0.0f), FVector(0.45f)));
	}
}

void UVaelEngineerComponent::TickShield()
{
	const float Delta = FMath::Clamp(Now() - LastShieldTick, 0.0f, 0.1f);
	LastShieldTick = Now();
	ShieldOpen = FMath::Clamp(ShieldOpen + (bShieldRaised ? Delta : -Delta) / ShieldOpenSeconds, 0.0f, 1.0f);
	if (ShieldHub == nullptr && HasShield())
	{
		// worn just now: the shield is hung on the arm before it moves
		UpdateShieldMeshes();
	}
	AVaelCharacter* Player = GetPlayer();
	USceneComponent* Bone = ShieldHub != nullptr ? ShieldHub->GetAttachParent() : nullptr;
	if (Player == nullptr || Bone == nullptr || !HasShield() || (!bShieldRaised && ShieldOpen <= 0.0f))
	{
		ShieldOpen = bShieldRaised ? ShieldOpen : 0.0f;
		if (ShieldHub != nullptr && HasShield())
		{
			UpdateShieldMeshes();
		}
		bShieldMoving = false;
		SetComponentTickInterval(0.25f);
		return;
	}
	// The Rollsehne snaps it open the way the Aschenroller unrolls: from the forearm the shield swings round before him, facing
	// ahead, and the five rows slide out of each other, from the foot near the ground up to his shoulder
	const float Alpha = FMath::SmoothStep(0.0f, 1.0f, ShieldOpen);
	const FTransform Folded = ShieldFolded * Bone->GetSocketTransform(ShieldHub->GetAttachSocketName());
	const FVector Ahead = Player->GetActorForwardVector();
	const FVector Wrist = Player->GetMesh()->GetSocketLocation(TEXT("hand_l"));
	const FQuat Facing = FRotationMatrix::MakeFromXZ(Ahead, FVector::UpVector).ToQuat();
	FTransform Held(FQuat::Slerp(Folded.GetRotation(), Facing, Alpha), FMath::Lerp(Folded.GetLocation(), Wrist + Ahead * 35.0f - FVector(0.0f, 0.0f, 10.0f), Alpha),
		Folded.GetScale3D());
	ShieldHub->SetWorldTransform(Held);
	for (int32 Index = 0; Index < ShieldBands.Num(); ++Index)
	{
		const float Row = (Index - (ShieldRows - 1) * 0.5f) * ShieldRowHeight;
		ShieldBands[Index]->SetRelativeTransform(FTransform(FQuat::Identity, FVector(FMath::Lerp(-0.6f, 0.15f, Alpha) * Index, 0.0f, Row * Alpha),
			FVector(FMath::Lerp(0.45f, 1.0f, Alpha))));
	}
}

void UVaelEngineerComponent::UpdateCarriedMeshes()
{
	AVaelCharacter* Player = GetPlayer();
	if (Player == nullptr)
	{
		return;
	}
	const bool bShow = Player->IsEngineer();
	const int32 Tier = bShow ? GetPouchTier() : -1;
	const bool bGoggles = bShow && HasBuilt(VaelEngineerGear::Goggles);
	if (Tier == ShownPouchTier && bGoggles == bShownGoggles && bShow == bShownPack)
	{
		return;
	}
	ShownPouchTier = Tier;
	bShownGoggles = bGoggles;
	bShownPack = bShow;
	if (!bShow)
	{
		for (UStaticMeshComponent* Piece : { PackMesh.Get(), PouchMesh.Get(), GogglesMesh.Get() })
		{
			if (Piece != nullptr)
			{
				Piece->SetVisibility(false);
			}
		}
		return;
	}
	// In actor space: the capsule middle is the origin, +X ahead; the back at chest height, the belt at the hip, the goggles on the brow
	const float Half = Player->GetSimpleCollisionHalfHeight();
	PackMesh = AttachPiece(PackMesh, TEXT("EngineerPack"), TEXT("SM_Ing_Rucksack"), TEXT("spine_04"),
		FTransform(FRotator(0.0f, 180.0f, 0.0f), FVector(-7.0f, 0.0f, Half * 0.22f + 8.0f)));
	const TCHAR* Pouches[] = { TEXT("SM_Ing_Beutel_Leder"), TEXT("SM_Ing_Beutel_Panzer"), TEXT("SM_Ing_Beutel_Seide") };
	PouchMesh = AttachPiece(PouchMesh, TEXT("EngineerPouches"), Pouches[FMath::Clamp(Tier, 0, 2)], TEXT("pelvis"),
		FTransform(FRotator::ZeroRotator, FVector(0.0f, 0.0f, Half * 0.02f)));
	if (bGoggles)
	{
		const USkeletalMeshComponent* Body = Player->GetMesh();
		const FVector Head = Body->DoesSocketExist(TEXT("head")) ? Body->GetSocketTransform(TEXT("head"), RTS_Actor).GetLocation() : FVector(0.0f, 0.0f, Half * 0.8f);
		GogglesMesh = AttachPiece(GogglesMesh, TEXT("EngineerGoggles"), TEXT("SM_Ing_Brille"), TEXT("head"),
			FTransform(FRotator::ZeroRotator, Head + FVector(-1.0f, 0.0f, 5.0f)));
	}
	else if (GogglesMesh != nullptr)
	{
		GogglesMesh->SetVisibility(false);
	}
}

void UVaelEngineerComponent::UpdateWeaponMesh()
{
	AVaelCharacter* Player = GetPlayer();
	if (Player == nullptr)
	{
		return;
	}
	const bool bShow = Player->IsEngineer();
	const FName Branch = GetBranch(Weapon);
	const FName Wanted = bShow ? (Branch.IsNone() ? Weapon : Branch) : NAME_None;
	if (Wanted == ShownWeapon && (WeaponMesh == nullptr || WeaponMesh->IsVisible() == bShow))
	{
		return;
	}
	ShownWeapon = Wanted;
	if (!bShow)
	{
		if (WeaponMesh != nullptr)
		{
			WeaponMesh->SetVisibility(false);
		}
		return;
	}
	if (WeaponMesh == nullptr)
	{
		WeaponMesh = NewObject<UStaticMeshComponent>(Player, TEXT("EngineerWeapon"));
		WeaponMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		WeaponMesh->SetCastShadow(true);
		WeaponMesh->RegisterComponent();
	}
	// In the right hand; the pieces have their grip at the origin, the shot along +X. The Zwille is held in the left hand
	// and placed every frame by UpdateSling
	if (Weapon != VaelEngineerGear::Sling)
	{
		const FName Socket = Player->GetMesh()->DoesSocketExist(TEXT("hand_r")) ? FName(TEXT("hand_r")) : NAME_None;
		WeaponMesh->AttachToComponent(Player->GetMesh(), FAttachmentTransformRules::SnapToTargetNotIncludingScale, Socket);
		WeaponMesh->SetRelativeLocation(FVector(-6.0f, 3.0f, 0.0f));
		WeaponMesh->SetRelativeRotation(FRotator(0.0f, 90.0f, 90.0f));
	}
	else
	{
		WeaponMesh->DetachFromComponent(FDetachmentTransformRules::KeepWorldTransform);
		WeaponMesh->SetUsingAbsoluteLocation(true);
		WeaponMesh->SetUsingAbsoluteRotation(true);
	}
	FString Name = Wanted.ToString();
	UStaticMesh* Mesh = LoadObject<UStaticMesh>(nullptr, *FString::Printf(TEXT("/Game/Vael/Characters/Ingenieur/Werkstuecke/SM_Ing_%s.SM_Ing_%s"), *Name, *Name), nullptr, LOAD_NoWarn | LOAD_Quiet);
	if (Mesh == nullptr)
	{
		Name = Weapon.ToString();
		Mesh = LoadObject<UStaticMesh>(nullptr, *FString::Printf(TEXT("/Game/Vael/Characters/Ingenieur/Werkstuecke/SM_Ing_%s.SM_Ing_%s"), *Name, *Name), nullptr, LOAD_NoWarn | LOAD_Quiet);
	}
	// each tier a little bigger and heavier
	WeaponMesh->SetRelativeScale3D(FVector(1.0f + 0.06f * GetWeaponTier(Weapon)));
	WeaponMesh->SetStaticMesh(Mesh);
	WeaponMesh->SetVisibility(Mesh != nullptr);
}

// ---------------------------------------------------------------- Building

UVaelEngineerComponent::EBuildBlock UVaelEngineerComponent::GetBuildBlock(const FVaelGear& Gear, bool bAtWorkbench) const
{
	const bool bLasting = Gear.Kind == EVaelGearKind::Weapon || Gear.Kind == EVaelGearKind::Armor || Gear.Kind == EVaelGearKind::Tool;
	if (bLasting && Built.Contains(Gear.Id))
	{
		return EBuildBlock::Built;
	}
	if (!Gear.Requires.IsNone() && !Built.Contains(Gear.Requires) && GetCarried(Gear.Requires) + GetStock(Gear.Requires) <= 0)
	{
		return EBuildBlock::NeedsFirst;
	}
	if (!Gear.UpgradeOf.IsNone() && GetWeaponTier(Gear.UpgradeOf) < Gear.Tier - 1)
	{
		return EBuildBlock::NeedsFirst;
	}
	if (Gear.bNeedsPlan && !Plans.Contains(Gear.Id))
	{
		return EBuildBlock::NeedsPlan;
	}
	if (Gear.IsFormula() && !Formulas.Contains(Gear.Id))
	{
		return EBuildBlock::Unknown;
	}
	if (!bAtWorkbench && !Gear.bField)
	{
		return EBuildBlock::NeedsWorkbench;
	}
	const AVaelCharacter* Player = GetPlayer();
	const UVaelMaterialBag* Bag = Player != nullptr ? Player->GetMaterialBag() : nullptr;
	for (const FVaelGearCost& Cost : Gear.Costs)
	{
		const UVaelMaterial* Material = VaelEngineerGear::LoadMaterial(Cost.Material);
		if (Bag == nullptr || Material == nullptr || Bag->GetCount(Material) < Cost.Count)
		{
			return EBuildBlock::Materials;
		}
	}
	return EBuildBlock::Ok;
}

bool UVaelEngineerComponent::Build(FName Id, bool bAtWorkbench)
{
	const FVaelGear* Gear = VaelEngineerGear::Find(Id);
	AVaelCharacter* Player = GetPlayer();
	if (Gear == nullptr || Player == nullptr || GetBuildBlock(*Gear, bAtWorkbench) != EBuildBlock::Ok)
	{
		return false;
	}
	for (const FVaelGearCost& Cost : Gear->Costs)
	{
		Player->GetMaterialBag()->RemoveMaterial(VaelEngineerGear::LoadMaterial(Cost.Material), Cost.Count);
	}

	switch (Gear->Kind)
	{
	case EVaelGearKind::Weapon:
		Built.Add(Id);
		if (Gear->UpgradeOf.IsNone())
		{
			Weapon = Id;
		}
		else
		{
			// an upgrade: the tier rises, a branch of tier 1 sets the effect and the look of the weapon
			SetWeaponTier(Gear->UpgradeOf, FMath::Max(GetWeaponTier(Gear->UpgradeOf), Gear->Tier));
			if (Gear->Tier == 1)
			{
				WeaponBranches.Add(Gear->UpgradeOf, Id);
			}
			Weapon = Gear->UpgradeOf;
			ShownWeapon = NAME_None;
		}
		break;
	case EVaelGearKind::Armor:
		Built.Add(Id);
		if (GetWorn(Gear->Place).IsNone())
		{
			Wear(Id);
		}
		break;
	case EVaelGearKind::Tool:
		Built.Add(Id);
		break;
	case EVaelGearKind::Refine:
		// A Werkstoff goes into the material bag
		if (const UVaelMaterial* Made = VaelEngineerGear::LoadMaterial(Gear->Output))
		{
			Player->GetMaterialBag()->AddMaterial(Made, Gear->OutputCount);
		}
		else
		{
			UE_LOG(LogVael, Warning, TEXT("Engineer: the material %s of %s is missing"), *Gear->Output.ToString(), *Id.ToString());
		}
		break;
	default:
	{
		// A kind of shot that has no pocket yet goes into the backpack when every pocket is taken
		const bool bPocket = Gear->Kind != EVaelGearKind::Ammo || IsInPocket(Id) || HasFreePocket();
		if (Gear->Kind == EVaelGearKind::Ammo && bPocket)
		{
			Pockets.AddUnique(Id);
		}
		const int32 Room = bPocket ? FMath::Max(GetCarryLimit(*Gear) - GetCarried(Id), 0) : 0;
		const int32 IntoPouch = FMath::Min(Room, Gear->Batch);
		Carried.FindOrAdd(Id) += IntoPouch;
		if (Gear->Batch > IntoPouch)
		{
			Stock.FindOrAdd(Id) += Gear->Batch - IntoPouch;
		}
		if (Gear->Kind == EVaelGearKind::Device && Device.IsNone())
		{
			Device = Id;
		}
		break;
	}
	}

	UVaelCombatTextSubsystem::PostPickup(Player, FText::Format(LOCTEXT("Built", "Gebaut: {0}"), Gear->Name), Gear->Color);
	UVaelSaveSubsystem::RequestSave(Player);
	UE_LOG(LogVael, Log, TEXT("Engineer: %s builds %s (pouch %d, stock %d)"), *Player->GetName(), *Id.ToString(), GetCarried(Id), GetStock(Id));
	return true;
}

void UVaelEngineerComponent::Restock()
{
	int32 Moved = 0;
	for (TPair<FName, int32>& Entry : Stock)
	{
		const FVaelGear* Gear = VaelEngineerGear::Find(Entry.Key);
		if (Gear == nullptr || Entry.Value <= 0 || (Gear->Kind == EVaelGearKind::Ammo && !IsInPocket(Entry.Key)))
		{
			continue;
		}
		const int32 Room = FMath::Max(GetCarryLimit(*Gear) - GetCarried(Entry.Key), 0);
		const int32 Take = FMath::Min(Room, Entry.Value);
		Carried.FindOrAdd(Entry.Key) += Take;
		Entry.Value -= Take;
		Moved += Take;
	}
	if (Moved > 0)
	{
		UVaelCombatTextSubsystem::PostReaction(GetOwner(), FText::Format(LOCTEXT("Restocked", "Taschen aufgefüllt ({0})"), Moved));
	}
}

bool UVaelEngineerComponent::HasBuilt(FName Id) const
{
	return Built.Contains(Id);
}

int32 UVaelEngineerComponent::GetCarried(FName Id) const
{
	const int32* Count = Carried.Find(Id);
	return Count != nullptr ? *Count : 0;
}

int32 UVaelEngineerComponent::GetStock(FName Id) const
{
	const int32* Count = Stock.Find(Id);
	return Count != nullptr ? *Count : 0;
}

int32 UVaelEngineerComponent::GetPouchTier() const
{
	return HasBuilt(VaelEngineerGear::PouchSilk) ? 2 : HasBuilt(VaelEngineerGear::PouchArmor) ? 1 : 0;
}

int32 UVaelEngineerComponent::GetCarryLimit(const FVaelGear& Gear) const
{
	const int32 Tier = GetPouchTier();
	return Gear.Kind == EVaelGearKind::Ammo ? 20 + 10 * Tier : Gear.Kind == EVaelGearKind::Device ? 2 + Tier : 1;
}

// ---------------------------------------------------------------- Weapon and shot

const FVaelGear* UVaelEngineerComponent::GetWeapon() const
{
	const FVaelGear* Found = VaelEngineerGear::Find(Weapon);
	return Found != nullptr ? Found : VaelEngineerGear::Find(VaelEngineerGear::Sling);
}

void UVaelEngineerComponent::CycleWeapon()
{
	TArray<FName> Weapons;
	for (const FVaelGear& Gear : VaelEngineerGear::GetAll())
	{
		if (Gear.Kind == EVaelGearKind::Weapon && Gear.UpgradeOf.IsNone() && Built.Contains(Gear.Id))
		{
			Weapons.Add(Gear.Id);
		}
	}
	if (Weapons.Num() < 2)
	{
		return;
	}
	Weapon = Weapons[(Weapons.IndexOfByKey(Weapon) + 1) % Weapons.Num()];
	// Taking up the other weapon takes a moment
	LastShot = FMath::Max(LastShot, Now() - GetInterval() + 0.3f);
	if (const FVaelGear* Gear = GetWeapon())
	{
		UVaelCombatTextSubsystem::PostPickup(GetOwner(), Gear->Name, Gear->Color);
	}
}

FText UVaelEngineerComponent::GetAmmoName() const
{
	if (const FVaelGear* Gear = Ammo != VaelEngineerGear::Standard ? VaelEngineerGear::Find(Ammo) : nullptr)
	{
		return Gear->Name;
	}
	const FVaelGear* WeaponGear = GetWeapon();
	return WeaponGear != nullptr ? WeaponGear->StandardShot : FText::GetEmpty();
}

FLinearColor UVaelEngineerComponent::GetAmmoColor() const
{
	const FVaelGear* Gear = Ammo != VaelEngineerGear::Standard ? VaelEngineerGear::Find(Ammo) : nullptr;
	return Gear != nullptr ? Gear->Color : FLinearColor(0.55f, 0.5f, 0.45f);
}

TArray<FName> UVaelEngineerComponent::GetAmmoChoices() const
{
	// Only what sits in the pockets of the Werkrolle can be loaded in a fight
	TArray<FName> Shots = { VaelEngineerGear::Standard };
	for (const FName Kind : Pockets)
	{
		if (GetCarried(Kind) > 0)
		{
			Shots.Add(Kind);
		}
	}
	return Shots;
}

void UVaelEngineerComponent::CycleAmmo(int32 Step)
{
	const TArray<FName> Shots = GetAmmoChoices();
	const FName From = IsGrabbing() ? GrabAmmo : Ammo;
	const int32 Current = FMath::Max(Shots.IndexOfByKey(From), 0);
	BeginGrab(Shots[(Current + Step + Shots.Num()) % Shots.Num()]);
}

void UVaelEngineerComponent::BeginGrab(FName Id)
{
	LastCycle = Now();
	if (Id == VaelEngineerGear::Standard || Id.IsNone())
	{
		// stones, arrows and bolts are always at hand
		GrabAmmo = NAME_None;
		Ammo = VaelEngineerGear::Standard;
		return;
	}
	GrabAmmo = Id;
	bGrabFromPack = GetCarried(Id) <= 0;
	GrabStart = Now();
	GrabEnd = Now() + (bGrabFromPack ? 1.2f : 0.35f);
}

float UVaelEngineerComponent::GetGrabFraction() const
{
	return IsGrabbing() ? FMath::Clamp((Now() - GrabStart) / FMath::Max(GrabEnd - GrabStart, 0.01f), 0.0f, 1.0f) : 0.0f;
}

void UVaelEngineerComponent::OnHurt()
{
	if (IsGrabbing() && bGrabFromPack)
	{
		GrabAmmo = NAME_None;
		UVaelCombatTextSubsystem::PostReaction(GetOwner(), LOCTEXT("GrabLost", "Griff unterbrochen"));
	}
}

float UVaelEngineerComponent::GetInterval() const
{
	const FVaelGear* Gear = GetWeapon();
	const float Base = Gear != nullptr ? Gear->Interval : 0.6f;
	const FVaelGear* Branch = Gear != nullptr && !GetBranch(Gear->Id).IsNone() ? VaelEngineerGear::Find(GetBranch(Gear->Id)) : nullptr;
	return Base * (Branch != nullptr ? Branch->IntervalScale : 1.0f) * (CountSet(TEXT("Russfeder")) >= 4 ? 0.8f : 1.0f);
}

float UVaelEngineerComponent::GetReloadLeft() const
{
	return FMath::Max3(LastShot + GetInterval() - Now(), ReloadEnd - Now(), 0.0f);
}

FName UVaelEngineerComponent::GetBranch(FName WeaponId) const
{
	const FName* Branch = WeaponBranches.Find(WeaponId);
	return Branch != nullptr ? *Branch : NAME_None;
}

FName UVaelEngineerComponent::FindPlan(float Chance)
{
	if (FMath::FRand() >= Chance)
	{
		return NAME_None;
	}
	TArray<const FVaelGear*> Unknown;
	for (const FVaelGear& Gear : VaelEngineerGear::GetAll())
	{
		if (Gear.bNeedsPlan && !Plans.Contains(Gear.Id))
		{
			Unknown.Add(&Gear);
		}
	}
	if (Unknown.IsEmpty())
	{
		return NAME_None;
	}
	const FVaelGear* Found = Unknown[FMath::RandRange(0, Unknown.Num() - 1)];
	Plans.Add(Found->Id);
	UVaelCombatTextSubsystem::PostPickup(GetOwner(), FText::Format(LOCTEXT("PlanFound", "Bauplan gefunden: {0}"), Found->Name), FLinearColor(0.95f, 0.8f, 0.45f));
	UVaelSaveSubsystem::RequestSave(GetOwner());
	UE_LOG(LogVael, Log, TEXT("Engineer: finds the blueprint of %s"), *Found->Id.ToString());
	return Found->Id;
}

void UVaelEngineerComponent::LearnAllPlans()
{
	for (const FVaelGear& Gear : VaelEngineerGear::GetAll())
	{
		if (Gear.bNeedsPlan)
		{
			Plans.Add(Gear.Id);
		}
	}
}

void UVaelEngineerComponent::LearnAllFormulas()
{
	for (const FVaelGear& Gear : VaelEngineerGear::GetAll())
	{
		if (Gear.IsFormula())
		{
			Formulas.Add(Gear.Id);
		}
	}
}

EVaelMixResult UVaelEngineerComponent::Mix(const TArray<FName>& MixParts, FText& OutHint, FName& OutFormula)
{
	OutFormula = NAME_None;
	AVaelCharacter* Player = GetPlayer();
	if (Player == nullptr || MixParts.Num() < 2)
	{
		OutHint = LOCTEXT("MixTwo", "Mindestens zwei Teile: eine Hülle und einen Kern.");
		return EVaelMixResult::Missing;
	}
	const FVaelGear* Formula = VaelEngineerGear::FindFormula(MixParts);
	if (Formula == nullptr)
	{
		OutHint = VaelEngineerGear::GetMixHint(MixParts);
		const FString Key = VaelEngineerGear::GetMixKey(MixParts);
		if (Tried.Contains(Key))
		{
			return EVaelMixResult::Tried;
		}
		Tried.Add(Key);
		UVaelSaveSubsystem::RequestSave(Player);
		return EVaelMixResult::Nothing;
	}
	OutFormula = Formula->Id;
	if (Formula->bNeedsPlan && !Plans.Contains(Formula->Id))
	{
		OutHint = LOCTEXT("MixPlan", "Die Teile würden passen, aber ohne Bauplan gelingt es nicht.");
		return EVaelMixResult::Nothing;
	}
	// The parts must be in the bag: what he mixes, he uses
	for (const FVaelGearCost& Cost : Formula->Costs)
	{
		const UVaelMaterial* Material = VaelEngineerGear::LoadMaterial(Cost.Material);
		if (Material == nullptr || Player->GetMaterialBag()->GetCount(Material) < Cost.Count)
		{
			OutHint = LOCTEXT("MixMissing", "Davon fehlt etwas im Beutel.");
			return EVaelMixResult::Missing;
		}
	}
	const bool bNew = !Formulas.Contains(Formula->Id);
	Formulas.Add(Formula->Id);
	// Mixing works wherever he may build shots, so the workbench rules don't hold him back here
	Build(Formula->Id, true);
	if (bNew)
	{
		UVaelCombatTextSubsystem::PostPickup(GetOwner(), FText::Format(LOCTEXT("FormulaFound", "Neue Formel: {0}"), Formula->Name), FLinearColor(0.95f, 0.75f, 0.4f));
		UE_LOG(LogVael, Log, TEXT("Engineer: finds the formula %s"), *Formula->Id.ToString());
	}
	OutHint = VaelEngineerGear::GetEffectText(*Formula);
	return bNew ? EVaelMixResult::Learned : EVaelMixResult::Built;
}

bool UVaelEngineerComponent::PutInPocket(FName Id)
{
	const FVaelGear* Gear = VaelEngineerGear::Find(Id);
	if (Gear == nullptr || Gear->Kind != EVaelGearKind::Ammo || (!IsInPocket(Id) && !HasFreePocket()))
	{
		return false;
	}
	Pockets.AddUnique(Id);
	ReadyFromStock(Id);
	return true;
}

int32 UVaelEngineerComponent::EmptyPocket(FName Id)
{
	const int32 Moved = GetCarried(Id);
	Pockets.Remove(Id);
	if (Moved <= 0)
	{
		return 0;
	}
	Carried.Remove(Id);
	Stock.FindOrAdd(Id) += Moved;
	if (Ammo == Id)
	{
		Ammo = VaelEngineerGear::Standard;
	}
	return Moved;
}

int32 UVaelEngineerComponent::GetWeaponTier(FName Id) const
{
	const int32* Tier = WeaponTiers.Find(Id);
	return Tier != nullptr ? *Tier : 0;
}

void UVaelEngineerComponent::SetWeaponTier(FName Id, int32 Tier)
{
	WeaponTiers.Add(Id, FMath::Max(Tier, 0));
}

float UVaelEngineerComponent::GetTierScale() const
{
	const FVaelGear* Gear = GetWeapon();
	return FMath::Pow(1.18f, static_cast<float>(Gear != nullptr ? GetWeaponTier(Gear->Id) : 0));
}

bool UVaelEngineerComponent::UsesDraw() const
{
	const FVaelGear* Gear = GetWeapon();
	// The bow and the slingshot are drawn while held and loose on release
	return Gear != nullptr && (Gear->Id == VaelEngineerGear::Bow || Gear->Id == VaelEngineerGear::Sling);
}

void UVaelEngineerComponent::StartDraw()
{
	if (UsesDraw() && DrawStart < 0.0f)
	{
		DrawStart = Now();
	}
}

float UVaelEngineerComponent::GetDrawSeconds() const
{
	return DrawStart >= 0.0f ? Now() - DrawStart : 0.0f;
}

bool UVaelEngineerComponent::ReleaseDraw(const FVector& AimLocation)
{
	if (DrawStart < 0.0f)
	{
		return false;
	}
	const float Held = GetDrawSeconds();
	DrawStart = -1.0f;
	const FVaelGear* Drawn = GetWeapon();
	if (Drawn != nullptr && Drawn->Id == VaelEngineerGear::Sling)
	{
		// The slingshot: the further the band is pulled, the further and harder the stone flies (8 m tapped, 24 m full)
		const float Pull = FMath::Clamp(Held / SlingFullDraw, 0.0f, 1.0f);
		ShotReach = FMath::Lerp(800.0f, 2400.0f, Pull);
		const bool bFired = Fire(AimLocation, FMath::Lerp(0.6f, 1.4f, Pull), false);
		ShotReach = 0.0f;
		return bFired;
	}
	// Too short a pull still shoots, only weakly
	const float Scale = Held >= 1.2f ? 1.5f : Held >= 0.8f ? 1.2f : Held >= 0.4f ? 1.0f : 0.6f;
	return Fire(AimLocation, Scale, Held >= 1.2f);
}

bool UVaelEngineerComponent::Fire(const FVector& AimLocation, float DrawScale, bool bDrawPierce)
{
	AVaelCharacter* Player = GetPlayer();
	const FVaelGear* Gear = GetWeapon();
	if (Player == nullptr || Gear == nullptr || GetReloadLeft() > 0.0f || Player->IsDowned() || IsGrabbing() || bShieldRaised)
	{
		return false;
	}
	if (Ammo != VaelEngineerGear::Standard && GetCarried(Ammo) <= 0)
	{
		Ammo = VaelEngineerGear::Standard;
	}
	// The sling: shots in a quick row charge the Hagel, the fourth throws five stones at once
	const bool bSling = Gear->Id == VaelEngineerGear::Sling;
	HagelCount = bSling && Now() - LastShot < GetInterval() + 0.5f ? HagelCount + 1 : 1;
	LastShot = Now();

	// The crossbow: two bolts, then it is spanned again
	if (Gear->Id == VaelEngineerGear::Crossbow)
	{
		Magazine = FMath::Max(Magazine - 1, 0);
		if (Magazine == 0)
		{
			ReloadEnd = Now() + 1.4f;
			Magazine = 2;
		}
	}

	// From the hand, towards the aim point; the reach of the weapon is as far as it goes
	const FVector From = Player->GetActorLocation() + Player->GetActorForwardVector() * 40.0f + FVector(0.0f, 0.0f, 50.0f);
	FVector SlingFrom = From;
	if (bSling)
	{
		FVector TipL, TipR;
		if (SlingTips(TipL, TipR))
		{
			SlingFrom = (TipL + TipR) * 0.5f;
		}
		SlingLastShot = Now();
	}
	FVector Toward = AimLocation - From;
	Toward.Z = 0.0f;
	if (Toward.IsNearlyZero())
	{
		Toward = Player->GetActorForwardVector();
	}
	const float Reach = (ShotReach > 0.0f ? ShotReach : Gear->Range) * (HasBuilt(VaelEngineerGear::LensFog) ? 1.15f : 1.0f);
	const float Distance = FMath::Min(Toward.Size(), Reach);
	const FVector To = From + Toward.GetSafeNormal() * Distance + FVector(0.0f, 0.0f, -10.0f);
	const float ShotDamage = Gear->Damage * GetDamageScale() * GetTierScale() * DrawScale;

	if (bSling)
	{
		// The stones leave the sling at the release of the throw, from where the hand is then
		const bool bHagel = HagelCount >= 4;
		if (bHagel)
		{
			HagelCount = 0;
			UVaelCombatTextSubsystem::PostReaction(Player, LOCTEXT("Hagel", "Hagel!"));
		}
		// The stone leaves the pouch between the prongs; the further it should go, the higher it is sent
		const FVector Offset = To - From;
		const float Arc = Gear->Arc * (ShotReach > 0.0f ? ShotReach / 1100.0f : 1.0f);
		if (bHagel)
		{
			// Hagel: five stones fanned out, each 40 %
			for (int32 Stone = -2; Stone <= 2; ++Stone)
			{
				const FVector Spread = FRotator(0.0f, Stone * 8.0f, 0.0f).RotateVector(Offset);
				AVaelEngineerShot::Launch(this, SlingFrom, SlingFrom + Spread, Gear->Speed, Arc, ShotDamage * 0.4f, Ammo, PiercesAll(), GetAmmoColor(), 7.0f, true);
			}
		}
		else
		{
			AVaelEngineerShot::Launch(this, SlingFrom, SlingFrom + Offset, Gear->Speed, Arc, ShotDamage, Ammo, PiercesAll(), GetAmmoColor(), 9.0f, true);
		}
	}
	else
	{
		AVaelEngineerShot::Launch(this, From, To, Gear->Speed, Gear->Arc, ShotDamage, Ammo, Gear->bPierce || PiercesAll() || bDrawPierce, GetAmmoColor(), 6.0f);
	}

	if (Ammo != VaelEngineerGear::Standard)
	{
		int32& Left = Carried.FindOrAdd(Ammo);
		Left = FMath::Max(Left - 1, 0);
		if (Left == 0)
		{
			UVaelCombatTextSubsystem::PostReaction(Player, FText::Format(LOCTEXT("AmmoOut", "{0} verbraucht"), GetAmmoName()));
			// more in the backpack: he reaches for it by himself
			const FName Empty = Ammo;
			Ammo = VaelEngineerGear::Standard;
			if (GetStock(Empty) > 0)
			{
				BeginGrab(Empty);
			}
		}
	}
	return true;
}

// ---------------------------------------------------------------- Devices

void UVaelEngineerComponent::CycleDevice(int32 Step)
{
	TArray<FName> Devices;
	for (const FVaelGear& Gear : VaelEngineerGear::GetAll())
	{
		if (Gear.Kind == EVaelGearKind::Device && GetCarried(Gear.Id) > 0)
		{
			Devices.Add(Gear.Id);
		}
	}
	if (Devices.IsEmpty())
	{
		UVaelCombatTextSubsystem::PostReaction(GetOwner(), LOCTEXT("NoDevices", "Keine Vorrichtungen dabei"));
		return;
	}
	const int32 Current = Devices.IndexOfByKey(Device);
	Device = Devices[(FMath::Max(Current, 0) + (Current == INDEX_NONE ? 0 : Step) + Devices.Num()) % Devices.Num()];
	if (const FVaelGear* Gear = VaelEngineerGear::Find(Device))
	{
		UVaelCombatTextSubsystem::PostPickup(GetOwner(), FText::Format(LOCTEXT("DevicePicked", "{0} ({1})"), Gear->Name, GetCarried(Device)), Gear->Color);
	}
}

bool UVaelEngineerComponent::PlaceDevice(const FVector& AimLocation)
{
	AVaelCharacter* Player = GetPlayer();
	if (Player == nullptr || Player->IsDowned())
	{
		return false;
	}
	if (Device.IsNone() || GetCarried(Device) <= 0)
	{
		CycleDevice(0);
		if (Device.IsNone() || GetCarried(Device) <= 0)
		{
			return false;
		}
	}
	const FVaelGear* Gear = VaelEngineerGear::Find(Device);
	if (Gear == nullptr)
	{
		return false;
	}

	FVector Offset = AimLocation - Player->GetActorLocation();
	Offset.Z = 0.0f;
	const FVector Spot = Player->GetActorLocation() + Offset.GetClampedToMaxSize(PlaceReach);
	AVaelEngineerDevice* NewDevice = AVaelEngineerDevice::Lay(this, *Gear, Spot);
	if (NewDevice == nullptr)
	{
		return false;
	}

	Placed.RemoveAll([](const TWeakObjectPtr<AVaelEngineerDevice>& Each) { return !Each.IsValid(); });
	if (Placed.Num() >= MaxPlaced && Placed[0].IsValid())
	{
		Placed[0]->Destroy();
		Placed.RemoveAt(0);
	}
	Placed.Add(NewDevice);

	int32& Left = Carried.FindOrAdd(Device);
	Left = FMath::Max(Left - 1, 0);
	UE_LOG(LogVael, Log, TEXT("Engineer: %s lays %s (%d left)"), *Player->GetName(), *Device.ToString(), Left);
	if (Left == 0)
	{
		CycleDevice(1);
	}
	return true;
}

int32 UVaelEngineerComponent::IgniteAll()
{
	int32 Ignited = 0;
	for (const TWeakObjectPtr<AVaelEngineerDevice>& Each : TArray<TWeakObjectPtr<AVaelEngineerDevice>>(Placed))
	{
		if (Each.IsValid() && Each->GetDeviceType() == EVaelDeviceType::Mine)
		{
			Each->Detonate(VaelEngineerGear::Standard);
			++Ignited;
		}
	}
	if (Ignited == 0)
	{
		UVaelCombatTextSubsystem::PostReaction(GetOwner(), LOCTEXT("NoMines", "Keine Mine gelegt"));
	}
	return Ignited;
}

int32 UVaelEngineerComponent::GetNumPlaced() const
{
	int32 Count = 0;
	for (const TWeakObjectPtr<AVaelEngineerDevice>& Each : Placed)
	{
		Count += Each.IsValid() ? 1 : 0;
	}
	return Count;
}

void UVaelEngineerComponent::OnDeviceShot(AVaelEngineerDevice* Shot, FName ShotAmmo)
{
	if (Shot == nullptr)
	{
		return;
	}
	if (Shot->GetDeviceType() == EVaelDeviceType::Mine)
	{
		Shot->Detonate(ShotAmmo);
	}
	else if (Shot->GetDeviceType() == EVaelDeviceType::Turret && ShotAmmo != VaelEngineerGear::Standard)
	{
		Shot->LoadAmmo(ShotAmmo);
	}
}

// ---------------------------------------------------------------- Shots that land

void UVaelEngineerComponent::ApplyHit(AActor* Target, const FVector& Location, float Damage, FName ShotAmmo, const FVector& From, FName WeaponId)
{
	const FVaelGear* AmmoGear = ShotAmmo != VaelEngineerGear::Standard ? VaelEngineerGear::Find(ShotAmmo) : nullptr;
	EVaelAmmoEffect Effect = AmmoGear != nullptr ? AmmoGear->Effect : EVaelAmmoEffect::None;
	if (AmmoGear == nullptr && !WeaponId.IsNone() && !GetBranch(WeaponId).IsNone())
	{
		const FVaelGear* Branch = VaelEngineerGear::Find(GetBranch(WeaponId));
		Effect = Branch != nullptr ? Branch->StandardEffect : Effect;
	}
	if (Effect == EVaelAmmoEffect::Heal || Effect == EVaelAmmoEffect::HealTime)
	{
		ApplyBurst(Location, ShotAmmo, Damage, From);
		return;
	}

	AVaelCharacterBase* Enemy = Cast<AVaelCharacterBase>(Target);
	if (Enemy == nullptr || !UVaelCombatStatics::CanDamage(GetOwner(), Enemy))
	{
		return;
	}
	float Scaled = Damage * (AmmoGear != nullptr ? AmmoGear->DamageScale : 1.0f);
	LastZone = 1.0f;
	LastDistance = 1.0f;
	bLastCrit = false;
	if (const FVaelGear* WeaponGear = WeaponId.IsNone() ? nullptr : VaelEngineerGear::Find(WeaponId))
	{
		// Zone: the head side is the weak spot (x1.3, x1.5 with the goggles), the back is plain, the flanks are armour (x0.8);
		// what the trap holds is always open at its weak spot
		const FVector Facing = Enemy->GetActorForwardVector().GetSafeNormal2D();
		const FVector ToShooter = (From - Enemy->GetActorLocation()).GetSafeNormal2D();
		const float Dot = FVector::DotProduct(Facing, ToShooter);
		const bool bHeld = UVaelCombatStatics::HasStatus(Enemy, EVaelStatus::Frozen);
		const float Weak = HasBuilt(VaelEngineerGear::Goggles) ? 1.5f : 1.3f;
		LastZone = bHeld || Dot > 0.77f ? Weak : Dot < -0.64f ? 1.0f : 0.8f;
		// The Krustenbolzen goes through: the armoured flank counts as plain
		if (Effect == EVaelAmmoEffect::Pierce && LastZone < 1.0f)
		{
			LastZone = 1.0f;
		}

		// Distance: x1.15 in the sweet spot of the weapon, falling to x0.7 towards point blank and the end of its reach
		const float Meters = FVector::Dist2D(From, Enemy->GetActorLocation());
		if (Meters < WeaponGear->SweetMin)
		{
			LastDistance = FMath::Lerp(0.7f, 1.15f, FMath::Clamp(Meters / WeaponGear->SweetMin, 0.0f, 1.0f));
		}
		else if (Meters > WeaponGear->SweetMax)
		{
			LastDistance = FMath::Lerp(1.15f, 0.7f, FMath::Clamp((Meters - WeaponGear->SweetMax) / FMath::Max(WeaponGear->Range - WeaponGear->SweetMax, 1.0f), 0.0f, 1.0f));
		}
		else
		{
			LastDistance = 1.15f;
		}

		// Crit: 5 %, 15 % with the goggles, x1.6
		bLastCrit = FMath::FRand() < (HasBuilt(VaelEngineerGear::Goggles) ? 0.15f : 0.05f);
		Scaled *= LastZone * LastDistance * (bLastCrit ? 1.6f : 1.0f) * (HasBuilt(VaelEngineerGear::LensMark) ? 1.1f : 1.0f);
		if (bLastCrit)
		{
			UVaelCombatTextSubsystem::PostReaction(Enemy, LOCTEXT("Crit", "Kritisch!"));
		}
		else if (LastZone > 1.2f)
		{
			UVaelCombatTextSubsystem::PostReaction(Enemy, LOCTEXT("WeakSpot", "Schwachstelle"));
		}
	}
	const EVaelElement Element = Effect == EVaelAmmoEffect::BlackFire ? EVaelElement::Mark
		: Effect == EVaelAmmoEffect::Fire || Effect == EVaelAmmoEffect::Crackle || Effect == EVaelAmmoEffect::Flame || Effect == EVaelAmmoEffect::Blast
			|| Effect == EVaelAmmoEffect::FireField ? EVaelElement::Fire
		: Effect == EVaelAmmoEffect::Dew || Effect == EVaelAmmoEffect::Steam ? EVaelElement::Water : EVaelElement::Earth;
	UVaelCombatStatics::DealDamage(GetOwner(), Enemy, Scaled, Element);
	if (!WeaponId.IsNone())
	{
		// the Säurenadel eats one spot: parts break sooner
		HitPart(Enemy, Scaled * (AmmoGear != nullptr ? AmmoGear->PartBreak : 1.0f), LastZone);
	}
	UE_LOG(LogVael, Log, TEXT("Engineer: %s hits %s for %.0f (zone %.2f, distance %.2f%s)"), *ShotAmmo.ToString(), *Enemy->GetName(), Scaled, LastZone, LastDistance, bLastCrit ? TEXT(", crit") : TEXT(""));

	// Shadow shots (poison, acid, black fire, Nid) pull Die Waage of the shooter
	if (AmmoGear != nullptr && AmmoGear->ShadowPull > 0.0f)
	{
		UVaelBalanceSubsystem::ChangePlayer(GetOwner(), AmmoGear->ShadowPull, AmmoGear->Name);
	}

	// What works round the spot bursts there; the rest works on the target alone
	const bool bArea = Effect == EVaelAmmoEffect::Shards || Effect == EVaelAmmoEffect::Smoke || Effect == EVaelAmmoEffect::Crackle
		|| Effect == EVaelAmmoEffect::Flame || Effect == EVaelAmmoEffect::Blast || Effect == EVaelAmmoEffect::FireField || Effect == EVaelAmmoEffect::Steam
		|| Effect == EVaelAmmoEffect::Frenzy || Effect == EVaelAmmoEffect::Blind
		|| ((Effect == EVaelAmmoEffect::Sleep || Effect == EVaelAmmoEffect::Dew) && AmmoGear != nullptr && AmmoGear->Radius > 0.0f);
	if (bArea)
	{
		ApplyBurst(Location, ShotAmmo, Damage, From);
	}
	else
	{
		ApplyEffect(Enemy, AmmoGear, Effect, Location, From, Damage);
	}
}

TArray<AVaelCharacterBase*> UVaelEngineerComponent::EnemiesAround(const FVector& Location, float Radius) const
{
	TArray<AVaelCharacterBase*> Found;
	for (TActorIterator<AVaelCharacterBase> It(GetWorld()); It; ++It)
	{
		if (!It->IsA<AVaelCharacter>() && It->GetHealth() > 0.0f && UVaelCombatStatics::CanDamage(GetOwner(), *It)
			&& FVector::Dist2D(It->GetActorLocation(), Location) <= Radius)
		{
			Found.Add(*It);
		}
	}
	return Found;
}

void UVaelEngineerComponent::ApplyEffect(AVaelCharacterBase* Enemy, const FVaelGear* AmmoGear, EVaelAmmoEffect Effect, const FVector& Location, const FVector& From, float Damage)
{
	if (Enemy == nullptr)
	{
		return;
	}
	const auto Secs = [AmmoGear](float Default) { return AmmoGear != nullptr && AmmoGear->Seconds > 0.0f ? AmmoGear->Seconds : Default; };
	switch (Effect)
	{
	case EVaelAmmoEffect::Fire:
		UVaelCombatStatics::ApplyStatus(GetOwner(), Enemy, EVaelStatus::Burning, Secs(5.0f), 4.0f);
		break;
	case EVaelAmmoEffect::Poison:
		// the needle gives its poison slowly but harder
		Poisons.Add({ Enemy, AmmoGear != nullptr && AmmoGear->Hull == FName(TEXT("Hohlkiel")) ? 7.0f : 5.0f, Now() + Secs(6.0f) });
		UVaelCombatStatics::ApplyStatus(GetOwner(), Enemy, EVaelStatus::Confused, 1.5f);
		break;
	case EVaelAmmoEffect::Acid:
		UVaelCombatStatics::ApplyStatus(GetOwner(), Enemy, EVaelStatus::Marked, Secs(8.0f));
		break;
	case EVaelAmmoEffect::Heavy:
	{
		FVaelSpellHit Push;
		Push.Damage = 0.0f;
		Push.Element = EVaelElement::Earth;
		Push.Knockback = 650.0f;
		Push.Status = EVaelStatus::Frozen;
		Push.StatusDuration = 0.8f;
		UVaelCombatStatics::ApplyNatureHit(Enemy, Push, (Enemy->GetActorLocation() - From).GetSafeNormal2D());
		UVaelCombatTextSubsystem::PostReaction(Enemy, LOCTEXT("Stunned", "Betäubt"));
		break;
	}
	case EVaelAmmoEffect::Sticky:
		UVaelCombatStatics::ApplyStatus(GetOwner(), Enemy, EVaelStatus::Slowed, Secs(6.0f));
		break;
	case EVaelAmmoEffect::Sleep:
		if (Enemy->GetMaxHealth() <= 160.0f)
		{
			UVaelCombatStatics::ApplyStatus(GetOwner(), Enemy, EVaelStatus::Frozen, Secs(5.0f));
			UVaelCombatTextSubsystem::PostReaction(Enemy, LOCTEXT("Asleep", "Schläft"));
		}
		else
		{
			UVaelCombatStatics::ApplyStatus(GetOwner(), Enemy, EVaelStatus::Slowed, 4.0f);
		}
		break;
	case EVaelAmmoEffect::Glow:
		UVaelCombatStatics::ApplyStatus(GetOwner(), Enemy, EVaelStatus::Marked, Secs(10.0f));
		UVaelCombatTextSubsystem::PostReaction(Enemy, LOCTEXT("Lit", "Leuchtet"));
		break;
	case EVaelAmmoEffect::Thorn:
		Poisons.Add({ Enemy, 4.0f, Now() + 6.0f });
		break;
	case EVaelAmmoEffect::Dew:
		UVaelCombatStatics::ApplyStatus(GetOwner(), Enemy, EVaelStatus::Wet, Secs(8.0f));
		break;
	case EVaelAmmoEffect::BlackFire:
		// violet fire of the Nid: bites as Mark, so no resistance to fire holds it back
		Poisons.Add({ Enemy, 6.0f, Now() + Secs(6.0f), EVaelElement::Mark });
		UVaelCombatTextSubsystem::PostReaction(Enemy, LOCTEXT("BlackFire", "Schwarzfeuer"));
		break;
	case EVaelAmmoEffect::Frenzy:
		UVaelCombatStatics::ApplyStatus(GetOwner(), Enemy, EVaelStatus::Fevered, Secs(3.0f));
		break;
	case EVaelAmmoEffect::Blind:
		UVaelCombatStatics::ApplyStatus(GetOwner(), Enemy, EVaelStatus::Confused, Secs(2.0f));
		UVaelCombatTextSubsystem::PostReaction(Enemy, LOCTEXT("Blinded", "Geblendet"));
		// the Sonnenladung marks everyone in its flash
		if (AmmoGear != nullptr && AmmoGear->Boosters.Contains(FName(TEXT("Lichttropfen"))))
		{
			UVaelCombatStatics::ApplyStatus(GetOwner(), Enemy, EVaelStatus::Marked, 6.0f);
		}
		break;
	case EVaelAmmoEffect::Steam:
		UVaelCombatStatics::ApplyStatus(GetOwner(), Enemy, EVaelStatus::Confused, FMath::Min(Secs(3.0f), 3.0f));
		UVaelCombatStatics::ApplyStatus(GetOwner(), Enemy, EVaelStatus::Wet, 4.0f);
		break;
	default:
		break;
	}
	if (AmmoGear != nullptr && AmmoGear->bAlsoBurn)
	{
		UVaelCombatStatics::ApplyStatus(GetOwner(), Enemy, EVaelStatus::Burning, 3.0f, 4.0f);
	}
	if (AmmoGear != nullptr && AmmoGear->bAlsoBleed)
	{
		Poisons.Add({ Enemy, 4.0f, Now() + 6.0f });
	}
	if (AmmoGear != nullptr && AmmoGear->bAlsoSlow)
	{
		UVaelCombatStatics::ApplyStatus(GetOwner(), Enemy, EVaelStatus::Slowed, 6.0f);
	}
}

void UVaelEngineerComponent::Explode(const FVector& Location, float Radius, float Damage, bool bBurn)
{
	VaelEffects::SpawnAt(this, VaelEffects::Load(EVaelElement::Fire).Impact, Location, FRotator::ZeroRotator, FLinearColor(1.0f, 0.45f, 0.1f), Radius * 0.6f);
	VaelEffects::SpawnAt(this, VaelAssets::LoadOptional(UVaelMagicSettings::Get()->DustBurstEffect), Location, FRotator::ZeroRotator, FLinearColor(0.3f, 0.26f, 0.22f), Radius * 0.4f);
	for (AVaelCharacterBase* Enemy : EnemiesAround(Location, Radius))
	{
		UVaelCombatStatics::DealDamage(GetOwner(), Enemy, Damage, EVaelElement::Fire);
		FVaelSpellHit Push;
		Push.Damage = 0.0f;
		Push.Element = EVaelElement::Fire;
		Push.Knockback = 500.0f;
		UVaelCombatStatics::ApplyNatureHit(Enemy, Push, (Enemy->GetActorLocation() - Location).GetSafeNormal2D());
		if (bBurn)
		{
			UVaelCombatStatics::ApplyStatus(GetOwner(), Enemy, EVaelStatus::Burning, 3.0f, 4.0f);
		}
	}
	// his own mines nearby go off with it
	for (TActorIterator<AVaelEngineerDevice> It(GetWorld()); It; ++It)
	{
		if (It->GetEngineer() == this && It->GetDeviceType() == EVaelDeviceType::Mine && FVector::Dist2D(It->GetActorLocation(), Location) < Radius)
		{
			It->Detonate(TEXT("Brand"));
		}
	}
}

void UVaelEngineerComponent::TickEffects()
{
	// Glutnadeln that are due go off where the target is now
	for (int32 Index = DelayedBlasts.Num() - 1; Index >= 0; --Index)
	{
		const FDelayedBlast& Blast = DelayedBlasts[Index];
		if (Now() < Blast.At)
		{
			continue;
		}
		const FVector At = Blast.Stuck.IsValid() ? Blast.Stuck->GetActorLocation() : Blast.Location;
		const FDelayedBlast Copy = Blast;
		DelayedBlasts.RemoveAtSwap(Index);
		Explode(At, Copy.Radius, Copy.Damage, Copy.bBurn);
	}

	// Fields bite every half second
	if (Fields.IsEmpty() || Now() < NextFieldTick)
	{
		return;
	}
	NextFieldTick = Now() + 0.5f;
	const bool bShow = FMath::FloorToInt(Now() * 2.0f) % 2 == 0;
	for (int32 Index = Fields.Num() - 1; Index >= 0; --Index)
	{
		const FField Field = Fields[Index];
		if (Now() > Field.End)
		{
			Fields.RemoveAtSwap(Index);
			continue;
		}
		switch (Field.Effect)
		{
		case EVaelAmmoEffect::FireField:
			if (bShow)
			{
				VaelEffects::SpawnAt(this, VaelEffects::Load(EVaelElement::Fire).Impact, Field.Location, FRotator::ZeroRotator, FLinearColor(1.0f, 0.4f, 0.08f), Field.Radius * 0.5f);
			}
			for (AVaelCharacterBase* Enemy : EnemiesAround(Field.Location, Field.Radius))
			{
				UVaelCombatStatics::DealDamage(GetOwner(), Enemy, Field.Damage, EVaelElement::Fire);
				UVaelCombatStatics::ApplyStatus(GetOwner(), Enemy, EVaelStatus::Burning, 2.0f, 4.0f);
			}
			break;
		case EVaelAmmoEffect::Steam:
			if (bShow)
			{
				VaelEffects::SpawnAt(this, VaelAssets::LoadOptional(UVaelMagicSettings::Get()->SteamEffect), Field.Location, FRotator::ZeroRotator, FLinearColor(0.9f, 0.9f, 0.92f), Field.Radius);
			}
			for (AVaelCharacterBase* Enemy : EnemiesAround(Field.Location, Field.Radius))
			{
				UVaelCombatStatics::DealDamage(GetOwner(), Enemy, Field.Damage, EVaelElement::Water);
				UVaelCombatStatics::ApplyStatus(GetOwner(), Enemy, EVaelStatus::Confused, 1.0f);
			}
			break;
		case EVaelAmmoEffect::HealTime:
			if (bShow)
			{
				VaelEffects::SpawnAt(this, VaelAssets::LoadOptional(UVaelMagicSettings::Get()->HandEffect), Field.Location, FRotator::ZeroRotator, FLinearColor(0.6f, 0.95f, 0.65f), Field.Radius * 0.5f);
			}
			for (TActorIterator<AVaelCharacter> It(GetWorld()); It; ++It)
			{
				if (!It->IsDowned() && FVector::Dist2D(It->GetActorLocation(), Field.Location) < Field.Radius)
				{
					UVaelCombatStatics::Heal(*It, 2.5f);
				}
			}
			break;
		default:
			break;
		}
	}
}

void UVaelEngineerComponent::HitPart(AVaelCharacterBase* Enemy, float Damage, float Zone)
{
	if (Enemy == nullptr || FMath::IsNearlyEqual(Zone, 1.0f))
	{
		return;
	}
	FParts& State = Parts.FindOrAdd(Enemy);
	const bool bHead = Zone > 1.0f;
	if ((bHead && State.bHeadBroken) || (!bHead && State.bFlankBroken))
	{
		return;
	}
	float& Taken = bHead ? State.Head : State.Flank;
	Taken += Damage;
	const float Threshold = Enemy->GetMaxHealth() * (HasBuilt(VaelEngineerGear::LensBreak) ? 0.1875f : 0.25f);
	if (Taken < Threshold)
	{
		return;
	}

	(bHead ? State.bHeadBroken : State.bFlankBroken) = true;
	++BrokenParts;
	if (bHead)
	{
		if (AVaelCreature* Creature = Cast<AVaelCreature>(Enemy))
		{
			Creature->BreakAttack(0.85f);
		}
		UVaelCombatTextSubsystem::PostReaction(Enemy, LOCTEXT("HeadBroken", "Kopf gebrochen!"));
	}
	else
	{
		UVaelCombatStatics::ApplyStatus(GetOwner(), Enemy, EVaelStatus::Marked, 60.0f);
		UVaelCombatTextSubsystem::PostReaction(Enemy, LOCTEXT("FlankBroken", "Panzer gebrochen!"));
	}

	// What breaks off: the creature's own break piece (goal 14.11.: Glutkiefer, Rollpanzerkern …; the plain Bruchstück for
	// creatures that have none yet) and two of its own materials
	AVaelCharacter* Player = GetPlayer();
	const FVector At = Enemy->GetActorLocation();
	const AVaelCreature* Broken = Cast<AVaelCreature>(Enemy);
	const UVaelCreatureData* BrokenData = Broken != nullptr ? Broken->GetCreatureData() : nullptr;
	const UVaelMaterial* Piece = BrokenData != nullptr && !BrokenData->BreakPiece.IsNull() ? BrokenData->BreakPiece.LoadSynchronous() : nullptr;
	if (Piece == nullptr)
	{
		Piece = VaelEngineerGear::LoadMaterial(TEXT("Bruchstueck"));
	}
	if (Piece != nullptr)
	{
		AVaelMaterialDrop::DropMaterial(GetWorld(), At, Piece, 1, Player);
	}
	if (const AVaelCreature* Creature = Cast<AVaelCreature>(Enemy))
	{
		const UVaelCreatureData* Data = Creature->GetCreatureData();
		const UVaelMaterial* Own = Data != nullptr && !Data->Loot.IsEmpty() ? Data->Loot[0].Material.LoadSynchronous() : nullptr;
		if (Own != nullptr)
		{
			AVaelMaterialDrop::DropMaterial(GetWorld(), At + FVector(40.0f, 0.0f, 0.0f), Own, 2, Player);
		}
	}
	FindPlan(0.5f);
	UE_LOG(LogVael, Log, TEXT("Engineer: breaks the %s of %s"), bHead ? TEXT("head") : TEXT("armour"), *Enemy->GetName());
}

void UVaelEngineerComponent::ApplyBurst(const FVector& Location, FName ShotAmmo, float Damage, const FVector& From)
{
	const FVaelGear* AmmoGear = ShotAmmo != VaelEngineerGear::Standard ? VaelEngineerGear::Find(ShotAmmo) : nullptr;
	const EVaelAmmoEffect Effect = AmmoGear != nullptr ? AmmoGear->Effect : EVaelAmmoEffect::None;
	const float Radius = AmmoGear != nullptr && AmmoGear->Radius > 0.0f ? AmmoGear->Radius : 0.0f;
	const float Seconds = AmmoGear != nullptr && AmmoGear->Seconds > 0.0f ? AmmoGear->Seconds : 0.0f;
	const UVaelMagicSettings* Magic = UVaelMagicSettings::Get();

	switch (Effect)
	{
	case EVaelAmmoEffect::Heal:
	{
		// The healing capsule bursts among the companions: everyone of his side close by gets life back
		const float Amount = AmmoGear != nullptr && AmmoGear->Boosters.Contains(FName(TEXT("Lichttropfen"))) ? 22.0f : 15.0f;
		VaelEffects::SpawnAt(this, VaelAssets::LoadOptional(Magic->HandEffect), Location, FRotator::ZeroRotator, AmmoGear != nullptr ? AmmoGear->Color : FLinearColor::White, 120.0f);
		for (TActorIterator<AVaelCharacter> It(GetWorld()); It; ++It)
		{
			if (!It->IsDowned() && FVector::Dist2D(It->GetActorLocation(), Location) < FMath::Max(Radius, 320.0f))
			{
				UVaelCombatStatics::Heal(*It, Amount);
			}
		}
		return;
	}
	case EVaelAmmoEffect::HealTime:
		Fields.Add({ Location, Now() + FMath::Max(Seconds, 1.0f), FMath::Max(Radius, 320.0f), 0.0f, EVaelAmmoEffect::HealTime });
		NextFieldTick = FMath::Min(NextFieldTick, Now());
		return;

	case EVaelAmmoEffect::Smoke:
	case EVaelAmmoEffect::Sleep:
	case EVaelAmmoEffect::Frenzy:
	{
		// A cloud on the spot: smoke makes them stagger and lose the target; poppy puts the weak to sleep; Raunsamen turns them
		// on each other. The Glutrauch glows and burns inside.
		const FLinearColor Tint = Effect == EVaelAmmoEffect::Sleep ? FLinearColor(0.4f, 0.25f, 0.5f) : Effect == EVaelAmmoEffect::Frenzy ? FLinearColor(0.45f, 0.4f, 0.55f)
			: AmmoGear != nullptr && AmmoGear->bAlsoBurn ? FLinearColor(0.55f, 0.3f, 0.18f) : FLinearColor(0.32f, 0.3f, 0.29f);
		const float Reach = Radius > 0.0f ? Radius : 340.0f;
		VaelEffects::SpawnAt(this, VaelAssets::LoadOptional(Magic->SteamEffect), Location, FRotator::ZeroRotator, Tint, Reach * 0.9f);
		for (AVaelCharacterBase* Enemy : EnemiesAround(Location, Reach))
		{
			if (Effect == EVaelAmmoEffect::Smoke)
			{
				UVaelCombatStatics::ApplyStatus(GetOwner(), Enemy, EVaelStatus::Confused, Seconds > 0.0f ? Seconds : 4.0f);
				if (AmmoGear != nullptr && AmmoGear->bAlsoBurn)
				{
					UVaelCombatStatics::DealDamage(GetOwner(), Enemy, Damage * 0.3f, EVaelElement::Fire);
					UVaelCombatStatics::ApplyStatus(GetOwner(), Enemy, EVaelStatus::Burning, 3.0f, 4.0f);
				}
			}
			else
			{
				ApplyEffect(Enemy, AmmoGear, Effect, Location, From, Damage);
			}
		}
		return;
	}

	case EVaelAmmoEffect::Dew:
	case EVaelAmmoEffect::Blind:
	{
		// A splash of water, or a flash of light, round the spot
		const float Reach = Radius > 0.0f ? Radius : 200.0f;
		UNiagaraSystem* Look = Effect == EVaelAmmoEffect::Dew ? VaelAssets::LoadOptional(Magic->WaterBoltTrailEffect) : VaelAssets::LoadOptional(Magic->HandEffect);
		VaelEffects::SpawnAt(this, Look, Location, FRotator::ZeroRotator, AmmoGear != nullptr ? AmmoGear->Color : FLinearColor::White, Reach * 0.6f);
		for (AVaelCharacterBase* Enemy : EnemiesAround(Location, Reach))
		{
			ApplyEffect(Enemy, AmmoGear, Effect, Location, From, Damage);
		}
		return;
	}

	case EVaelAmmoEffect::Steam:
	{
		// Hot steam scalds and blinds; a cloud of it stays for a while
		const float Reach = Radius > 0.0f ? Radius : 250.0f;
		VaelEffects::SpawnAt(this, VaelAssets::LoadOptional(Magic->SteamEffect), Location, FRotator::ZeroRotator, FLinearColor(0.92f, 0.92f, 0.95f), Reach);
		for (AVaelCharacterBase* Enemy : EnemiesAround(Location, Reach))
		{
			UVaelCombatStatics::DealDamage(GetOwner(), Enemy, Damage * 0.5f, EVaelElement::Water);
			ApplyEffect(Enemy, AmmoGear, Effect, Location, From, Damage);
		}
		if (Seconds >= 4.0f)
		{
			Fields.Add({ Location, Now() + Seconds, Reach, Damage * 0.08f, EVaelAmmoEffect::Steam });
		}
		return;
	}

	case EVaelAmmoEffect::Flame:
	{
		// Stichflamme: a cone of fire onward from the spot, in the way the shot flew
		FVector Dir = (Location - From).GetSafeNormal2D();
		if (Dir.IsNearlyZero())
		{
			Dir = GetOwner()->GetActorForwardVector().GetSafeNormal2D();
		}
		const float Length = Radius > 0.0f ? Radius : 400.0f;
		UNiagaraSystem* FireLook = VaelEffects::Load(EVaelElement::Fire).Impact;
		for (int32 Step = 0; Step < 4; ++Step)
		{
			VaelEffects::SpawnAt(this, FireLook, Location + Dir * (Length * (0.15f + 0.25f * Step)), FRotator::ZeroRotator, FLinearColor(1.0f, 0.55f, 0.15f), 60.0f + 30.0f * Step);
		}
		const float CosHalf = FMath::Cos(FMath::DegreesToRadians(35.0f));
		for (AVaelCharacterBase* Enemy : EnemiesAround(Location, Length))
		{
			const FVector To = Enemy->GetActorLocation() - Location;
			const bool bInCone = To.Size2D() < 120.0f || FVector::DotProduct(To.GetSafeNormal2D(), Dir) >= CosHalf;
			if (!bInCone)
			{
				continue;
			}
			UVaelCombatStatics::DealDamage(GetOwner(), Enemy, Damage * 0.6f, EVaelElement::Fire);
			UVaelCombatStatics::ApplyStatus(GetOwner(), Enemy, EVaelStatus::Burning, Seconds > 0.0f ? Seconds : 3.0f, 4.0f);
		}
		return;
	}

	case EVaelAmmoEffect::Blast:
	{
		const float Reach = Radius > 0.0f ? Radius : 300.0f;
		if (AmmoGear != nullptr && AmmoGear->Delay > 0.0f)
		{
			// The Glutnadel sticks in the nearest enemy and goes off later where that enemy is then
			FDelayedBlast Blast;
			Blast.Location = Location;
			Blast.At = Now() + AmmoGear->Delay;
			Blast.Radius = Reach;
			Blast.Damage = Damage * 1.2f;
			Blast.bBurn = AmmoGear->bAlsoBurn;
			float Best = 160.0f;
			for (AVaelCharacterBase* Enemy : EnemiesAround(Location, 160.0f))
			{
				const float Dist = FVector::Dist2D(Enemy->GetActorLocation(), Location);
				if (Dist <= Best)
				{
					Best = Dist;
					Blast.Stuck = Enemy;
				}
			}
			DelayedBlasts.Add(Blast);
			return;
		}
		Explode(Location, Reach, Damage * 0.8f, AmmoGear != nullptr && AmmoGear->bAlsoBurn);
		return;
	}

	case EVaelAmmoEffect::FireField:
	{
		const float Reach = Radius > 0.0f ? Radius : 300.0f;
		VaelEffects::SpawnAt(this, VaelEffects::Load(EVaelElement::Fire).Impact, Location, FRotator::ZeroRotator, FLinearColor(1.0f, 0.4f, 0.08f), Reach * 0.6f);
		Fields.Add({ Location, Now() + (Seconds > 0.0f ? Seconds : 5.0f), Reach, Damage * 0.15f, EVaelAmmoEffect::FireField });
		NextFieldTick = FMath::Min(NextFieldTick, Now());
		return;
	}

	case EVaelAmmoEffect::Crackle:
	{
		// (older shot, kept for saved turrets) three small bursts of fire round the spot, and his mines nearby go off
		UNiagaraSystem* Burst = VaelAssets::LoadOptional(Magic->DustBurstEffect);
		for (int32 Pop = 0; Pop < 3; ++Pop)
		{
			const FVector At = Location + FRotator(0.0f, Pop * 120.0f + FMath::FRandRange(-20.0f, 20.0f), 0.0f).RotateVector(FVector(140.0f, 0.0f, 0.0f));
			VaelEffects::SpawnAt(this, Burst, At, FRotator::ZeroRotator, FLinearColor(1.0f, 0.45f, 0.1f), 90.0f);
			for (AVaelCharacterBase* Enemy : EnemiesAround(At, 170.0f))
			{
				UVaelCombatStatics::DealDamage(GetOwner(), Enemy, Damage * 0.35f, EVaelElement::Fire);
				UVaelCombatStatics::ApplyStatus(GetOwner(), Enemy, EVaelStatus::Burning, 2.0f, 3.0f);
			}
		}
		return;
	}

	default:
		break;
	}

	// Shards: everything around takes a part of the hit
	for (AVaelCharacterBase* Enemy : EnemiesAround(Location, 260.0f))
	{
		UVaelCombatStatics::DealDamage(GetOwner(), Enemy, Damage * 0.6f, EVaelElement::Earth);
	}
}

float UVaelEngineerComponent::GetDamageScale() const
{
	return CountSet(TEXT("Sichelklinge")) >= 2 ? 1.15f : 1.0f;
}

bool UVaelEngineerComponent::PiercesAll() const
{
	return CountSet(TEXT("Sichelklinge")) >= 4;
}

// ---------------------------------------------------------------- Armour

FName UVaelEngineerComponent::GetWorn(EVaelArmorPlace Place) const
{
	const int32 Index = static_cast<int32>(Place);
	return Worn.IsValidIndex(Index) ? Worn[Index] : NAME_None;
}

bool UVaelEngineerComponent::Wear(FName Id)
{
	const FVaelGear* Gear = VaelEngineerGear::Find(Id);
	if (Gear == nullptr || Gear->Kind != EVaelGearKind::Armor || !Built.Contains(Id))
	{
		return false;
	}
	Worn.SetNum(NumArmorPlaces);
	Worn[static_cast<int32>(Gear->Place)] = Id;
	ApplyHealthBonus();
	UVaelSaveSubsystem::RequestSave(GetOwner());
	return true;
}

int32 UVaelEngineerComponent::CountSet(FName Set) const
{
	int32 Count = 0;
	for (int32 Place = 0; Place < Worn.Num(); ++Place)
	{
		const FVaelGear* Gear = Worn[Place].IsNone() || Place == static_cast<int32>(EVaelArmorPlace::Shield) ? nullptr : VaelEngineerGear::Find(Worn[Place]);
		Count += Gear != nullptr && Gear->Set == Set ? 1 : 0;
	}
	return Count;
}

bool UVaelEngineerComponent::HasShield() const
{
	const AVaelCharacter* Player = GetPlayer();
	return Player != nullptr && Player->IsEngineer() && !GetWorn(EVaelArmorPlace::Shield).IsNone();
}

void UVaelEngineerComponent::SetShieldRaised(bool bRaised)
{
	bRaised = bRaised && HasShield() && !GetPlayer()->IsDowned();
	if (bRaised == bShieldRaised)
	{
		return;
	}
	bShieldRaised = bRaised;
	if (bRaised)
	{
		ShieldRaisedAt = Now();
	}
	// The shield moves smoothly: the component ticks every frame while it opens, stays open or folds (else four times a second)
	if (!bShieldMoving)
	{
		bShieldMoving = true;
		LastShieldTick = Now();
		SetComponentTickInterval(0.0f);
	}
}

int32 UVaelEngineerComponent::GetBlock(const AActor* From) const
{
	const AActor* Owner = GetOwner();
	if (!bShieldRaised || From == nullptr || Owner == nullptr || From == Owner)
	{
		return 0;
	}
	// Only what comes from ahead, within a wide cone
	const FVector ToAttacker = (From->GetActorLocation() - Owner->GetActorLocation()).GetSafeNormal2D();
	if (FVector::DotProduct(ToAttacker, Owner->GetActorForwardVector().GetSafeNormal2D()) < 0.25f)
	{
		return 0;
	}
	if (Now() - ShieldRaisedAt <= ShieldPerfectSeconds)
	{
		return 2;
	}
	return ShieldOpen >= 0.6f ? 1 : 0;
}

float UVaelEngineerComponent::GetArmorTotal() const
{
	float Total = 0.0f;
	for (const FName& Id : Worn)
	{
		if (const FVaelGear* Gear = Id.IsNone() ? nullptr : VaelEngineerGear::Find(Id))
		{
			Total += Gear->Armor;
		}
	}
	return Total;
}

float UVaelEngineerComponent::ScaleIncoming(float Damage, AActor* From, bool bFire)
{
	float Scale = 1.0f - FMath::Clamp(GetArmorTotal(), 0.0f, 60.0f) / 100.0f;
	const int32 Black = CountSet(TEXT("Schwarzglut"));
	if (CountSet(TEXT("Rollpanzer")) >= 2 || Black >= 2)
	{
		Scale *= 0.9f;
	}
	if (Black >= 2 && bFire)
	{
		Scale *= 0.85f;
	}
	// An attacker of his enemies (a creature), not a teammate or a damage over time without one
	AVaelCharacterBase* Attacker = Cast<AVaelCharacterBase>(From);
	if (Attacker != nullptr && (Attacker->IsA<AVaelCharacter>() || Attacker->GetHealth() <= 0.0f))
	{
		Attacker = nullptr;
	}
	if (Now() < CurledUntil)
	{
		Scale *= 0.4f;
		// Schwarzglut: the glowing plates of the curled engineer burn what strikes them
		if (Black >= 4 && Attacker != nullptr)
		{
			UVaelCombatStatics::ApplyStatus(GetOwner(), Attacker, EVaelStatus::Burning, 3.0f, 4.0f);
		}
	}
	const int32 Block = GetBlock(Attacker);
	if (Block == 2)
	{
		Scale = 0.0f;
		UVaelCombatTextSubsystem::PostReaction(GetOwner(), LOCTEXT("PerfectBlock", "Abgewehrt!"));
		VaelEffects::SpawnAt(this, VaelAssets::LoadOptional(UVaelMagicSettings::Get()->DustBurstEffect), ShieldHub != nullptr ? ShieldHub->GetComponentLocation() : GetOwner()->GetActorLocation(),
			FRotator::ZeroRotator, FLinearColor(1.0f, 0.45f, 0.12f), 60.0f);
		// With the whole set the snapping fan throws the attacker back and sets it on fire
		if (Black >= 4)
		{
			FVaelSpellHit Push;
			Push.Damage = 0.0f;
			Push.Element = EVaelElement::Fire;
			Push.Knockback = 700.0f;
			UVaelCombatStatics::ApplyNatureHit(Attacker, Push, (Attacker->GetActorLocation() - GetOwner()->GetActorLocation()).GetSafeNormal2D());
			UVaelCombatStatics::ApplyStatus(GetOwner(), Attacker, EVaelStatus::Burning, 3.0f, 5.0f);
		}
	}
	else if (Block == 1)
	{
		Scale *= 0.3f;
	}
	return Damage * Scale;
}

void UVaelEngineerComponent::OnDamageDealt(float Damage)
{
	const int32 Blood = CountSet(TEXT("Blutschuppen"));
	if (Blood >= 2 && Damage > 0.0f)
	{
		UVaelCombatStatics::Heal(GetOwner(), Damage * (Blood >= 4 ? 0.06f : 0.02f));
	}
}

void UVaelEngineerComponent::OnDodged()
{
	if (CountSet(TEXT("Rollpanzer")) >= 4 || CountSet(TEXT("Schwarzglut")) >= 4)
	{
		CurledUntil = Now() + 1.0f;
	}
}

float UVaelEngineerComponent::GetMoveSpeedScale() const
{
	// behind the open shield he only steps
	return (CountSet(TEXT("Russfeder")) >= 2 ? 1.1f : 1.0f) * (bShieldRaised ? 0.55f : 1.0f);
}

void UVaelEngineerComponent::ApplyHealthBonus()
{
	AVaelCharacter* Player = GetPlayer();
	UAbilitySystemComponent* AbilitySystem = Player != nullptr ? Player->GetAbilitySystemComponent() : nullptr;
	if (AbilitySystem == nullptr)
	{
		return;
	}
	float Bonus = 0.0f;
	if (Player->IsEngineer())
	{
		for (const FName& Id : Worn)
		{
			if (const FVaelGear* Gear = Id.IsNone() ? nullptr : VaelEngineerGear::Find(Id))
			{
				Bonus += Gear->Health;
			}
		}
	}
	const float NewMax = AbilitySystem->GetNumericAttributeBase(UVaelAttributeSet::GetMaxHealthAttribute()) - AppliedHealth + Bonus;
	AbilitySystem->SetNumericAttributeBase(UVaelAttributeSet::GetMaxHealthAttribute(), NewMax);
	AppliedHealth = Bonus;
	if (AbilitySystem->GetNumericAttribute(UVaelAttributeSet::GetHealthAttribute()) > NewMax)
	{
		AbilitySystem->SetNumericAttributeBase(UVaelAttributeSet::GetHealthAttribute(), NewMax);
	}
}

// ---------------------------------------------------------------- Gear that falls for him

bool UVaelEngineerComponent::Dismantle(AVaelCharacter* Player, int32 Rarity, int32 ItemLevel)
{
	if (Player == nullptr)
	{
		return false;
	}
	// What a found piece is made of: hide, blade, string, wood, slag; better pieces give more and rarer parts
	static const FName Parts[] = { TEXT("Schlacke"), TEXT("Spannfaden"), TEXT("Narbenholz"), TEXT("Sichelklinge"), TEXT("Rollpanzer"), TEXT("Rankenfaser") };
	const int32 Pieces = 1 + FMath::Clamp(Rarity, 0, 4) + ItemLevel / 10;
	TArray<FString> Got;
	for (int32 Index = 0; Index < Pieces; ++Index)
	{
		const int32 Pick = FMath::RandRange(0, FMath::Min(2 + Rarity, static_cast<int32>(UE_ARRAY_COUNT(Parts)) - 1));
		if (const UVaelMaterial* Material = VaelEngineerGear::LoadMaterial(Parts[Pick]))
		{
			Player->GetMaterialBag()->AddMaterial(Material, 1);
			Got.Add(Material->DisplayName.ToString());
		}
	}
	if (Got.IsEmpty())
	{
		return false;
	}
	UVaelCombatTextSubsystem::PostPickup(Player, FText::Format(LOCTEXT("Dismantled", "Zerlegt: {0}"), FText::FromString(FString::Join(Got, TEXT(", ")))), FLinearColor(0.85f, 0.6f, 0.3f));
	UE_LOG(LogVael, Log, TEXT("Engineer: %s takes a found piece apart into %s"), *Player->GetName(), *FString::Join(Got, TEXT(", ")));
	return true;
}

// ---------------------------------------------------------------- Save

void UVaelEngineerComponent::WriteSave(FVaelSavedPlayer& Saved) const
{
	Saved.EngineerBuilt = Built.Array();
	Saved.EngineerCarried = Carried;
	Saved.EngineerStock = Stock;
	Saved.EngineerWorn = Worn;
	Saved.EngineerWeapon = Weapon;
	Saved.EngineerTiers = WeaponTiers;
	Saved.EngineerBranches = WeaponBranches;
	Saved.EngineerPlans = Plans.Array();
	Saved.EngineerFormulas = Formulas.Array();
	Saved.EngineerTried = Tried.Array();
	Saved.EngineerPockets = Pockets;
}

void UVaelEngineerComponent::ReadSave(const FVaelSavedPlayer& Saved)
{
	ResetToStart();
	for (const FName& Id : Saved.EngineerBuilt)
	{
		Built.Add(Id);
	}
	Carried = Saved.EngineerCarried;
	WeaponTiers = Saved.EngineerTiers;
	WeaponBranches = Saved.EngineerBranches;
	Plans = TSet<FName>(Saved.EngineerPlans);
	Stock = Saved.EngineerStock;
	Formulas = TSet<FName>(Saved.EngineerFormulas);
	Tried = TSet<FString>(Saved.EngineerTried);
	// Saves from before the formulas (goal 14.11.): what he carries he knows how to make
	for (const TMap<FName, int32>* Held : { &Carried, &Stock })
	{
		for (const TPair<FName, int32>& Entry : *Held)
		{
			const FVaelGear* Gear = VaelEngineerGear::Find(Entry.Key);
			if (Gear != nullptr && Gear->IsFormula() && Entry.Value > 0)
			{
				Formulas.Add(Entry.Key);
			}
		}
	}
	// The pockets: saved, or (older saves) the kinds he carries; never more kinds than there are pockets, the rest goes into
	// the backpack, and nothing is carried outside a pocket
	Pockets = Saved.EngineerPockets;
	if (Pockets.IsEmpty())
	{
		for (const FVaelGear& Gear : VaelEngineerGear::GetAll())
		{
			if (Gear.Kind == EVaelGearKind::Ammo && GetCarried(Gear.Id) > 0)
			{
				Pockets.Add(Gear.Id);
			}
		}
	}
	while (Pockets.Num() > GetPocketCount())
	{
		EmptyPocket(Pockets.Last());
	}
	for (const FVaelGear& Gear : VaelEngineerGear::GetAll())
	{
		if (Gear.Kind == EVaelGearKind::Ammo && !IsInPocket(Gear.Id) && GetCarried(Gear.Id) > 0)
		{
			Stock.FindOrAdd(Gear.Id) += GetCarried(Gear.Id);
			Carried.Remove(Gear.Id);
		}
	}
	// saves from before the shield have four places
	if (Saved.EngineerWorn.Num() >= 4)
	{
		Worn = Saved.EngineerWorn;
		Worn.SetNum(NumArmorPlaces);
	}
	if (Built.Contains(Saved.EngineerWeapon))
	{
		Weapon = Saved.EngineerWeapon;
	}
	ApplyHealthBonus();
}

#undef LOCTEXT_NAMESPACE

bool UVaelEngineerComponent::SlingTips(FVector& OutLeft, FVector& OutRight) const
{
	if (WeaponMesh == nullptr || !WeaponMesh->IsVisible() || Weapon != VaelEngineerGear::Sling)
	{
		return false;
	}
	// SLING_TIP of Tools/Ingenieur/vael_ingenieur_werkstuecke.py, in cm
	const FTransform Grip = WeaponMesh->GetComponentTransform();
	OutLeft = Grip.TransformPosition(FVector(0.0f, -6.2f, 17.5f));
	OutRight = Grip.TransformPosition(FVector(0.0f, 6.2f, 17.5f));
	return true;
}

void UVaelEngineerComponent::UpdateSling(float DeltaTime)
{
	// Hook onto the body (again, if it was swapped): the work is done every frame in OnBodyPoseFinal
	AVaelCharacter* Player = GetPlayer();
	USkeletalMeshComponent* Body = Player != nullptr ? Player->GetMesh() : nullptr;
	if (Body != SlingBody.Get())
	{
		if (USkeletalMeshComponent* Old = SlingBody.Get())
		{
			Old->UnregisterOnBoneTransformsFinalizedDelegate(SlingBodyHandle);
		}
		SlingBody = Body;
		SlingBodyHandle = Body != nullptr
			? Body->RegisterOnBoneTransformsFinalizedDelegate(FOnBoneTransformsFinalizedMultiCast::FDelegate::CreateUObject(this, &UVaelEngineerComponent::OnBodyPoseFinal))
			: FDelegateHandle();
	}
	if (Body == nullptr)
	{
		UpdateSlingFrame(DeltaTime);
	}
}

void UVaelEngineerComponent::OnBodyPoseFinal()
{
	const UWorld* World = GetWorld();
	if (World == nullptr)
	{
		return;
	}
	const double Time = World->GetTimeSeconds();
	const float DeltaTime = SlingFrameTime < 0.0 ? 0.0f : static_cast<float>(FMath::Clamp(Time - SlingFrameTime, 0.0, 0.1));
	if (Time == SlingFrameTime)
	{
		return;
	}
	SlingFrameTime = Time;
	UpdateSlingFrame(DeltaTime);
	if (FParse::Param(FCommandLine::Get(), TEXT("VaelZwilleLog")) && (SlingAim > 0.0f || IsDrawing()))
	{
		UE_LOG(LogVael, Display, TEXT("[Zwille] t %.2f dt %.3f aim %.2f draw %.2f kick %.2f drawing %d holding %d"), Time, DeltaTime, SlingAim, SlingDrawShown,
			GetSlingKick(), IsDrawing(), IsHoldingSling());
	}
}

void UVaelEngineerComponent::UpdateSlingFrame(float DeltaTime)
{
	AVaelCharacter* Player = GetPlayer();
	const bool bSling = Player != nullptr && Player->IsEngineer() && Weapon == VaelEngineerGear::Sling && WeaponMesh != nullptr && WeaponMesh->IsVisible();
	for (int32 Side = 0; Side < 2; ++Side)
	{
		if (BandMesh[Side] != nullptr)
		{
			BandMesh[Side]->SetVisibility(bSling);
		}
	}
	if (PouchShell != nullptr)
	{
		PouchShell->SetVisibility(bSling);
	}
	if (!bSling)
	{
		SlingAim = SlingDrawShown = 0.0f;
		return;
	}

	// Held up while drawing and a moment after the shot; the band snaps back fast after the release
	const bool bDrawing = IsDrawing();
	// Raised quickly; after the shot held a moment, then lowered calmly (about 0.8 s)
	const float WantAim = bDrawing || Now() - SlingLastShot < 0.6f ? 1.0f : 0.0f;
	SlingAim = FMath::FInterpConstantTo(SlingAim, WantAim, DeltaTime, WantAim > SlingAim ? 5.0f : 1.25f);
	// The band gives easily at first and harder towards the cheek: the pull eases out
	const float DrawShare = bDrawing ? FMath::Clamp(GetDrawSeconds() / SlingFullDraw, 0.0f, 1.0f) : 0.0f;
	const float WantDraw = 1.0f - FMath::Square(1.0f - DrawShare);
	SlingDrawShown = bDrawing ? WantDraw : FMath::FInterpConstantTo(SlingDrawShown, 0.0f, DeltaTime, 14.0f);

	// The fork stands upright in the left fist, the tips ahead; hanging down along the leg while he does not aim
	USkeletalMeshComponent* Body = Player->GetMesh();
	const FVector HandL = Body->DoesSocketExist(TEXT("hand_l")) ? Body->GetSocketLocation(TEXT("hand_l")) : Player->GetActorLocation();
	const FVector HandR = Body->DoesSocketExist(TEXT("hand_r")) ? Body->GetSocketLocation(TEXT("hand_r")) : HandL;
	// Held in the fist (goal 9.11.: it hung beside the hand at a fixed angle): the handle runs along the thumb (hand_l Y,
	// up while aiming), the fork faces where the fingers point (hand_l X); the fist's middle a hand's breadth in front of
	// the wrist, towards the palm (-Z), the handle reaching a little below the thumb
	const FTransform HandBone = Body->DoesSocketExist(TEXT("hand_l")) ? Body->GetSocketTransform(TEXT("hand_l")) : Player->GetActorTransform();
	const FVector Fingers = HandBone.GetUnitAxis(EAxis::X);
	const FVector Thumb = HandBone.GetUnitAxis(EAxis::Y);
	const FVector Palm = -HandBone.GetUnitAxis(EAxis::Z);
	const FRotator Held = FRotationMatrix::MakeFromZX(Thumb, Fingers).Rotator();
	const float Scale = 1.0f + 0.06f * GetWeaponTier(Weapon);
	WeaponMesh->SetWorldLocationAndRotation(HandL + Fingers * 8.0f + Palm * 2.5f - Thumb * 6.0f * Scale, Held);
	WeaponMesh->SetWorldScale3D(FVector(Scale));

	if (BandMesh[0] == nullptr)
	{
		UStaticMesh* Cylinder = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
		UMaterialInterface* BandLook = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Vael/Refugium/Glutkammer/Probe/M_GK_Faden.M_GK_Faden"), nullptr, LOAD_NoWarn | LOAD_Quiet);
		for (int32 Side = 0; Side < 2; ++Side)
		{
			BandMesh[Side] = NewObject<UStaticMeshComponent>(Player, Side == 0 ? TEXT("ZwilleBandL") : TEXT("ZwilleBandR"));
			BandMesh[Side]->SetStaticMesh(Cylinder);
			BandMesh[Side]->SetMaterial(0, BandLook);
			BandMesh[Side]->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			BandMesh[Side]->SetCastShadow(false);
			BandMesh[Side]->SetUsingAbsoluteLocation(true);
			BandMesh[Side]->SetUsingAbsoluteRotation(true);
			BandMesh[Side]->SetUsingAbsoluteScale(true);
			BandMesh[Side]->RegisterComponent();
		}
		PouchShell = NewObject<UStaticMeshComponent>(Player, TEXT("ZwilleSchale"));
		PouchShell->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, TEXT("/Game/Vael/Characters/Ingenieur/Werkstuecke/SM_Ing_Zwillenschale.SM_Ing_Zwillenschale"), nullptr, LOAD_NoWarn | LOAD_Quiet));
		PouchShell->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		PouchShell->SetUsingAbsoluteLocation(true);
		PouchShell->SetUsingAbsoluteRotation(true);
		PouchShell->RegisterComponent();
	}

	// The pouch: resting just behind the fork, pulled to the right hand as the band is drawn
	FVector TipL, TipR;
	SlingTips(TipL, TipR);
	const FVector Middle = (TipL + TipR) * 0.5f;
	const FVector Rest = Middle - Held.Vector() * 6.0f - FVector(0.0f, 0.0f, 3.0f);
	const FVector Pouch = FMath::Lerp(Rest, HandR + Held.Vector() * 3.0f, FMath::Max(SlingDrawShown, SlingAim * 0.12f));
	const FVector Across = (TipR - TipL).GetSafeNormal();
	PouchShell->SetWorldLocationAndRotation(Pouch, FRotationMatrix::MakeFromYZ(Across, FVector::UpVector).Rotator());

	// The loaded shot in the pouch while he draws: the stone (or drop) of the ammo in the weapon
	if (PouchLoad == nullptr)
	{
		PouchLoad = NewObject<UStaticMeshComponent>(GetOwner(), TEXT("ZwilleLadung"));
		PouchLoad->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		PouchLoad->SetCastShadow(false);
		PouchLoad->SetUsingAbsoluteLocation(true);
		PouchLoad->SetUsingAbsoluteRotation(true);
		PouchLoad->SetUsingAbsoluteScale(true);
		PouchLoad->RegisterComponent();
	}
	const bool bLoaded = IsDrawing();
	PouchLoad->SetVisibility(bLoaded);
	if (bLoaded)
	{
		const AVaelEngineerShot::FLook Look = AVaelEngineerShot::GetLook(Ammo);
		if (PouchLoadAmmo != Ammo || PouchLoad->GetStaticMesh() == nullptr)
		{
			PouchLoadAmmo = Ammo;
			UStaticMesh* Shape = Look.bDrop ? LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Sphere.Sphere"))
				: LoadObject<UStaticMesh>(nullptr, TEXT("/Game/Rock_Collection_04/Meshes/Rock_03/StaticMeshes/SM_Rock_03.SM_Rock_03"), nullptr, LOAD_NoWarn | LOAD_Quiet);
			PouchLoad->SetStaticMesh(Shape);
			PouchLoad->SetMaterial(0, Look.bDrop ? LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"))
				: LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Vael/Maps/Aschenmark/Kulisse/Pack/MI_AM_Rock_03.MI_AM_Rock_03"), nullptr, LOAD_NoWarn | LOAD_Quiet));
			if (UMaterialInstanceDynamic* Material = PouchLoad->CreateAndSetMaterialInstanceDynamic(0))
			{
				Material->SetVectorParameterValue(TEXT("Color"), Look.Color * (1.0f + Look.Glow));
				Material->SetVectorParameterValue(TEXT("Tint"), Look.Color);
				Material->SetScalarParameterValue(TEXT("Glut"), Look.Glow);
			}
			const float Extent = Shape != nullptr ? FMath::Max(Shape->GetBoundingBox().GetSize().GetMax(), 1.0f) : 100.0f;
			PouchLoad->SetWorldScale3D(FVector(5.0f * Look.Scale / Extent));
		}
		PouchLoad->SetWorldLocationAndRotation(Pouch + FVector(0.0f, 0.0f, 1.5f), Held);
	}
	const FVector Ends[2] = { Pouch - Across * 2.5f, Pouch + Across * 2.5f };
	const FVector Tips[2] = { TipL, TipR };
	for (int32 Side = 0; Side < 2; ++Side)
	{
		// The engine cylinder is 100 cm high around its middle: stretched between tip and pouch, thinner the more it is pulled
		const FVector Span = Ends[Side] - Tips[Side];
		const float Length = FMath::Max(Span.Size(), 1.0f);
		const float Thick = FMath::Lerp(0.009f, 0.0055f, SlingDrawShown);
		BandMesh[Side]->SetWorldLocationAndRotation((Tips[Side] + Ends[Side]) * 0.5f, FRotationMatrix::MakeFromZ(Span / Length).Rotator());
		BandMesh[Side]->SetWorldScale3D(FVector(Thick, Thick, Length / 100.0f));
	}
}

int32 UVaelEngineerComponent::ReadyFromStock(FName Id)
{
	const FVaelGear* Gear = VaelEngineerGear::Find(Id);
	if (Gear == nullptr)
	{
		return 0;
	}
	if (Gear->Kind == EVaelGearKind::Ammo && !IsInPocket(Id))
	{
		// On the way the pockets stay as they are; a new kind gets a pocket only at the workbench or a campfire
		return 0;
	}
	int32& InStock = Stock.FindOrAdd(Id);
	int32& InPocket = Carried.FindOrAdd(Id);
	const int32 Moved = FMath::Clamp(GetCarryLimit(*Gear) - InPocket, 0, InStock);
	InStock -= Moved;
	InPocket += Moved;
	return Moved;
}

bool UVaelEngineerComponent::SetAmmo(FName Id)
{
	if (Id != VaelEngineerGear::Standard && GetCarried(Id) <= 0)
	{
		return false;
	}
	Ammo = Id;
	LastCycle = Now();
	return true;
}

bool UVaelEngineerComponent::IsHoldingSling() const
{
	const AVaelCharacter* Player = GetPlayer();
	return Player != nullptr && Player->IsEngineer() && Weapon == VaelEngineerGear::Sling && WeaponMesh != nullptr && WeaponMesh->IsVisible();
}

float UVaelEngineerComponent::GetSlingKick() const
{
	const float Since = Now() - SlingLastShot;
	return Since >= 0.0f && Since < 0.35f ? 1.0f - Since / 0.35f : 0.0f;
}

float UVaelEngineerComponent::GetSlingTremble() const
{
	return IsDrawing() && Weapon == VaelEngineerGear::Sling ? FMath::Clamp(GetDrawSeconds() - SlingFullDraw, 0.0f, 1.0f) : 0.0f;
}

