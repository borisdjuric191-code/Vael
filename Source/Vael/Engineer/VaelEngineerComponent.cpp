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
#include "EngineUtils.h"
#include "Items/VaelMaterial.h"
#include "Items/VaelMaterialBag.h"
#include "Player/VaelCharacter.h"
#include "Save/VaelSaveGame.h"
#include "Save/VaelSaveSubsystem.h"
#include "UI/VaelCombatTextSubsystem.h"
#include "Vael.h"

#define LOCTEXT_NAMESPACE "VaelEngineer"

namespace
{
	constexpr int32 AmmoCarry = 30;
	constexpr int32 DeviceCarry = 3;
	constexpr int32 MaxPlaced = 6;
	constexpr float PlaceReach = 700.0f;
	constexpr int32 NumArmorPlaces = 4;
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
			UVaelCombatStatics::DealDamage(GetOwner(), Poison.Target.Get(), Poison.DamagePerSecond * 0.5f, EVaelElement::Earth);
		}
	}

	Placed.RemoveAll([](const TWeakObjectPtr<AVaelEngineerDevice>& Each) { return !Each.IsValid(); });
	UpdateWeaponMesh();
}

void UVaelEngineerComponent::UpdateWeaponMesh()
{
	AVaelCharacter* Player = GetPlayer();
	if (Player == nullptr)
	{
		return;
	}
	const bool bShow = Player->IsEngineer();
	const FName Wanted = bShow ? Weapon : NAME_None;
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
	// In the right hand; the pieces have their grip at the origin, the shot along +X
	const FName Socket = Player->GetMesh()->DoesSocketExist(TEXT("hand_r")) ? FName(TEXT("hand_r")) : NAME_None;
	WeaponMesh->AttachToComponent(Player->GetMesh(), FAttachmentTransformRules::SnapToTargetNotIncludingScale, Socket);
	WeaponMesh->SetRelativeLocation(FVector(-6.0f, 3.0f, 0.0f));
	WeaponMesh->SetRelativeRotation(FRotator(0.0f, 90.0f, 90.0f));
	const FString Name = Weapon.ToString();
	UStaticMesh* Mesh = LoadObject<UStaticMesh>(nullptr, *FString::Printf(TEXT("/Game/Vael/Characters/Ingenieur/Werkstuecke/SM_Ing_%s.SM_Ing_%s"), *Name, *Name), nullptr, LOAD_NoWarn | LOAD_Quiet);
	WeaponMesh->SetStaticMesh(Mesh);
	WeaponMesh->SetVisibility(Mesh != nullptr);
}

// ---------------------------------------------------------------- Building

UVaelEngineerComponent::EBuildBlock UVaelEngineerComponent::GetBuildBlock(const FVaelGear& Gear, bool bAtWorkbench) const
{
	const bool bLasting = Gear.Kind == EVaelGearKind::Weapon || Gear.Kind == EVaelGearKind::Armor;
	if (bLasting && Built.Contains(Gear.Id))
	{
		return EBuildBlock::Built;
	}
	if (!Gear.Requires.IsNone() && !Built.Contains(Gear.Requires) && GetCarried(Gear.Requires) + GetStock(Gear.Requires) <= 0)
	{
		return EBuildBlock::NeedsFirst;
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
		Weapon = Id;
		break;
	case EVaelGearKind::Armor:
		Built.Add(Id);
		if (GetWorn(Gear->Place).IsNone())
		{
			Wear(Id);
		}
		break;
	default:
	{
		const int32 Room = FMath::Max(GetCarryLimit(*Gear) - GetCarried(Id), 0);
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
		if (Gear == nullptr || Entry.Value <= 0)
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

int32 UVaelEngineerComponent::GetCarryLimit(const FVaelGear& Gear) const
{
	return Gear.Kind == EVaelGearKind::Ammo ? AmmoCarry : Gear.Kind == EVaelGearKind::Device ? DeviceCarry : 1;
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
		if (Gear.Kind == EVaelGearKind::Weapon && Built.Contains(Gear.Id))
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

void UVaelEngineerComponent::CycleAmmo(int32 Step)
{
	TArray<FName> Shots = { VaelEngineerGear::Standard };
	for (const FVaelGear& Gear : VaelEngineerGear::GetAll())
	{
		if (Gear.Kind == EVaelGearKind::Ammo && GetCarried(Gear.Id) > 0)
		{
			Shots.Add(Gear.Id);
		}
	}
	const int32 Current = FMath::Max(Shots.IndexOfByKey(Ammo), 0);
	Ammo = Shots[(Current + Step + Shots.Num()) % Shots.Num()];
	UVaelCombatTextSubsystem::PostPickup(GetOwner(), GetAmmoName(), GetAmmoColor());
}

float UVaelEngineerComponent::GetInterval() const
{
	const FVaelGear* Gear = GetWeapon();
	const float Base = Gear != nullptr ? Gear->Interval : 0.6f;
	return Base * (CountSet(TEXT("Russfeder")) >= 4 ? 0.8f : 1.0f);
}

float UVaelEngineerComponent::GetReloadLeft() const
{
	return FMath::Max(LastShot + GetInterval() - Now(), 0.0f);
}

bool UVaelEngineerComponent::Fire(const FVector& AimLocation)
{
	AVaelCharacter* Player = GetPlayer();
	const FVaelGear* Gear = GetWeapon();
	if (Player == nullptr || Gear == nullptr || GetReloadLeft() > 0.0f || Player->IsDowned())
	{
		return false;
	}
	if (Ammo != VaelEngineerGear::Standard && GetCarried(Ammo) <= 0)
	{
		Ammo = VaelEngineerGear::Standard;
	}
	LastShot = Now();

	// From the hand, towards the aim point; the reach of the weapon is as far as it goes
	const FVector From = Player->GetActorLocation() + Player->GetActorForwardVector() * 40.0f + FVector(0.0f, 0.0f, 50.0f);
	FVector Toward = AimLocation - From;
	Toward.Z = 0.0f;
	if (Toward.IsNearlyZero())
	{
		Toward = Player->GetActorForwardVector();
	}
	const float Distance = FMath::Min(Toward.Size(), Gear->Range);
	const FVector To = From + Toward.GetSafeNormal() * Distance + FVector(0.0f, 0.0f, -10.0f);

	AVaelEngineerShot::Launch(this, From, To, Gear->Speed, Gear->Arc, Gear->Damage * GetDamageScale(), Ammo, Gear->bPierce || PiercesAll(), GetAmmoColor(),
		Gear->Id == VaelEngineerGear::Sling ? 9.0f : 6.0f);

	if (Ammo != VaelEngineerGear::Standard)
	{
		int32& Left = Carried.FindOrAdd(Ammo);
		Left = FMath::Max(Left - 1, 0);
		if (Left == 0)
		{
			UVaelCombatTextSubsystem::PostReaction(Player, FText::Format(LOCTEXT("AmmoOut", "{0} verbraucht"), GetAmmoName()));
			Ammo = VaelEngineerGear::Standard;
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

void UVaelEngineerComponent::ApplyHit(AActor* Target, const FVector& Location, float Damage, FName ShotAmmo)
{
	const FVaelGear* AmmoGear = ShotAmmo != VaelEngineerGear::Standard ? VaelEngineerGear::Find(ShotAmmo) : nullptr;
	const EVaelAmmoEffect Effect = AmmoGear != nullptr ? AmmoGear->Effect : EVaelAmmoEffect::None;
	if (Effect == EVaelAmmoEffect::Heal)
	{
		ApplyBurst(Location, ShotAmmo, Damage);
		return;
	}

	AVaelCharacterBase* Enemy = Cast<AVaelCharacterBase>(Target);
	if (Enemy == nullptr || !UVaelCombatStatics::CanDamage(GetOwner(), Enemy))
	{
		return;
	}
	const float Scaled = Damage * (AmmoGear != nullptr ? AmmoGear->DamageScale : 1.0f);
	UVaelCombatStatics::DealDamage(GetOwner(), Enemy, Scaled, Effect == EVaelAmmoEffect::Fire ? EVaelElement::Fire : EVaelElement::Earth);
	UE_LOG(LogVael, Log, TEXT("Engineer: %s hits %s for %.0f"), *ShotAmmo.ToString(), *Enemy->GetName(), Scaled);

	switch (Effect)
	{
	case EVaelAmmoEffect::Shards:
		ApplyBurst(Location, ShotAmmo, Damage);
		break;
	case EVaelAmmoEffect::Fire:
		UVaelCombatStatics::ApplyStatus(GetOwner(), Enemy, EVaelStatus::Burning, 5.0f, 4.0f);
		break;
	case EVaelAmmoEffect::Poison:
		Poisons.Add({ Enemy, 5.0f, Now() + 6.0f });
		UVaelCombatStatics::ApplyStatus(GetOwner(), Enemy, EVaelStatus::Confused, 1.5f);
		break;
	case EVaelAmmoEffect::Acid:
		UVaelCombatStatics::ApplyStatus(GetOwner(), Enemy, EVaelStatus::Marked, 8.0f);
		break;
	default:
		break;
	}
}

void UVaelEngineerComponent::ApplyBurst(const FVector& Location, FName ShotAmmo, float Damage)
{
	const FVaelGear* AmmoGear = ShotAmmo != VaelEngineerGear::Standard ? VaelEngineerGear::Find(ShotAmmo) : nullptr;
	const EVaelAmmoEffect Effect = AmmoGear != nullptr ? AmmoGear->Effect : EVaelAmmoEffect::None;
	if (Effect == EVaelAmmoEffect::Heal)
	{
		// The healing shot bursts among the companions: everyone of his side close by gets life back
		for (TActorIterator<AVaelCharacter> It(GetWorld()); It; ++It)
		{
			if (!It->IsDowned() && FVector::Dist2D(It->GetActorLocation(), Location) < 320.0f)
			{
				UVaelCombatStatics::Heal(*It, 22.0f);
			}
		}
		return;
	}

	// Shards: everything around takes a part of the hit
	for (TActorIterator<AVaelCharacterBase> It(GetWorld()); It; ++It)
	{
		if (It->IsA<AVaelCharacter>() || It->GetHealth() <= 0.0f || !UVaelCombatStatics::CanDamage(GetOwner(), *It) || FVector::Dist2D(It->GetActorLocation(), Location) > 260.0f)
		{
			continue;
		}
		UVaelCombatStatics::DealDamage(GetOwner(), *It, Damage * 0.6f, EVaelElement::Earth);
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
	for (const FName& Id : Worn)
	{
		const FVaelGear* Gear = Id.IsNone() ? nullptr : VaelEngineerGear::Find(Id);
		Count += Gear != nullptr && Gear->Set == Set ? 1 : 0;
	}
	return Count;
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

float UVaelEngineerComponent::ScaleIncoming(float Damage) const
{
	float Scale = 1.0f - FMath::Clamp(GetArmorTotal(), 0.0f, 60.0f) / 100.0f;
	if (CountSet(TEXT("Rollpanzer")) >= 2)
	{
		Scale *= 0.9f;
	}
	if (Now() < CurledUntil)
	{
		Scale *= 0.4f;
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
	if (CountSet(TEXT("Rollpanzer")) >= 4)
	{
		CurledUntil = Now() + 1.0f;
	}
}

float UVaelEngineerComponent::GetMoveSpeedScale() const
{
	return CountSet(TEXT("Russfeder")) >= 2 ? 1.1f : 1.0f;
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
}

void UVaelEngineerComponent::ReadSave(const FVaelSavedPlayer& Saved)
{
	ResetToStart();
	for (const FName& Id : Saved.EngineerBuilt)
	{
		Built.Add(Id);
	}
	Carried = Saved.EngineerCarried;
	Stock = Saved.EngineerStock;
	if (Saved.EngineerWorn.Num() == NumArmorPlaces)
	{
		Worn = Saved.EngineerWorn;
	}
	if (Built.Contains(Saved.EngineerWeapon))
	{
		Weapon = Saved.EngineerWeapon;
	}
	ApplyHealthBonus();
}

#undef LOCTEXT_NAMESPACE
