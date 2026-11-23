// Copyright Epic Games, Inc. All Rights Reserved.

#include "Creatures/VaelTropfseuche.h"
#include "World/VaelIndoorLevel.h"
#include "Creatures/VaelKobold.h"
#include "Combat/VaelCombatStatics.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Creatures/VaelSchleimgaenger.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/TargetPoint.h"
#include "EngineUtils.h"
#include "Magic/VaelGroundArea.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Player/VaelCharacter.h"
#include "Vael.h"

#define LOCTEXT_NAMESPACE "VaelTropfseuche"

namespace
{
	FVaelSeuchenStamm MakeStrain(const FText& Name, EVaelPlantRarity Rarity, float Weight, float Health, float Damage, float Pace, EVaelStatus HitStatus,
		float HitSeconds, float Misfire, float Drift, float InfestSeconds, const FLinearColor& Color, float Glow, float Cover, float Flow, float Pulse, float Jerk,
		float Wobble)
	{
		FVaelSeuchenStamm Strain;
		Strain.Name = Name;
		Strain.Rarity = Rarity;
		Strain.Weight = Weight;
		Strain.HostHealth = Health;
		Strain.HostDamage = Damage;
		Strain.HostPace = Pace;
		Strain.HitStatus = HitStatus;
		Strain.HitStatusSeconds = HitSeconds;
		Strain.Misfire = Misfire;
		Strain.Drift = Drift;
		Strain.InfestSeconds = InfestSeconds;
		Strain.Color = Color;
		Strain.Glow = Glow;
		Strain.Cover = Cover;
		Strain.Flow = Flow;
		Strain.Pulse = Pulse;
		Strain.Jerk = Jerk;
		Strain.Wobble = Wobble;
		return Strain;
	}

	/** Slime icicles of the cave builder (SM_HK_Schleimzapfen_*, the colour in the name of their "Schleim" material) */
	struct FIcicle
	{
		FVector Location;
		EVaelSeuchenStamm Strain;
	};

	TMap<TWeakObjectPtr<const UWorld>, TArray<FIcicle>> IcicleCache;

	const TArray<FIcicle>& GetIcicles(const UWorld* World)
	{
		if (const TArray<FIcicle>* Found = IcicleCache.Find(World))
		{
			return *Found;
		}
		TArray<FIcicle>& Icicles = IcicleCache.Add(World);
		for (TActorIterator<AStaticMeshActor> It(World); It; ++It)
		{
			const UStaticMeshComponent* Component = It->GetStaticMeshComponent();
			if (Component == nullptr || Component->GetStaticMesh() == nullptr || !Component->GetStaticMesh()->GetName().Contains(TEXT("Schleimzapfen")))
			{
				continue;
			}
			const int32 Slot = Component->GetMaterialIndex(TEXT("Schleim"));
			const UMaterialInterface* Slime = Component->GetMaterial(Slot == INDEX_NONE ? 0 : Slot);
			const FString Colour = Slime != nullptr ? Slime->GetName() : FString();
			EVaelSeuchenStamm Strain = EVaelSeuchenStamm::Verwesung;
			if (Colour.Contains(TEXT("Bernstein")))
			{
				Strain = EVaelSeuchenStamm::Honig;
			}
			else if (Colour.Contains(TEXT("Tuerkis")))
			{
				Strain = EVaelSeuchenStamm::Kaelte;
			}
			else if (Colour.Contains(TEXT("Violett")))
			{
				Strain = EVaelSeuchenStamm::Nid;
			}
			else if (Colour.Contains(TEXT("Rot")))
			{
				Strain = EVaelSeuchenStamm::Rausch;
			}
			Icicles.Add({ It->GetActorLocation(), Strain });
		}
		return Icicles;
	}
}

UVaelTropfseucheSettings::UVaelTropfseucheSettings()
{
	ShellMaterial = TSoftObjectPtr<UMaterialInterface>(FSoftObjectPath(TEXT("/Game/Vael/Kreaturen/Eisenschlund/Tropfseuche/M_Tropfseuche_Huelle.M_Tropfseuche_Huelle")));

	// The rarer, the stronger and the more aggressive (Boris 2026-11-21); all numbers are proposals
	Strains.Add(MakeStrain(LOCTEXT("Verwesung", "Verwesung"), EVaelPlantRarity::Common, 55.0f, 1.4f, 1.0f, 1.0f, EVaelStatus::None, 0.0f,
		0.08f, 12.0f, 12.0f, FLinearColor(0.35f, 1.0f, 0.2f), 2.6f, 0.5f, 5.0f, 2.8f, 0.0f, 0.7f));
	Strains.Add(MakeStrain(LOCTEXT("Honig", "Honig"), EVaelPlantRarity::Special, 18.0f, 1.2f, 1.1f, 1.0f, EVaelStatus::Slowed, 2.5f,
		0.12f, 18.0f, 14.0f, FLinearColor(1.0f, 0.6f, 0.1f), 2.8f, 0.7f, 1.5f, 3.6f, 0.0f, 1.4f));
	Strains.Add(MakeStrain(LOCTEXT("Kaelte", "Kälte"), EVaelPlantRarity::Special, 15.0f, 1.3f, 1.5f, 0.8f, EVaelStatus::Slowed, 1.5f,
		0.12f, 14.0f, 14.0f, FLinearColor(0.1f, 0.9f, 0.8f), 2.4f, 0.45f, 1.0f, 5.0f, 0.0f, 0.4f));
	Strains.Add(MakeStrain(LOCTEXT("Nid", "Nid"), EVaelPlantRarity::Rare, 9.0f, 1.5f, 1.3f, 1.05f, EVaelStatus::Marked, 6.0f,
		0.18f, 24.0f, 16.0f, FLinearColor(0.6f, 0.2f, 1.0f), 3.2f, 0.55f, 4.0f, 2.2f, 0.35f, 0.9f));
	Strains.Add(MakeStrain(LOCTEXT("Rausch", "Rausch"), EVaelPlantRarity::Legendary, 3.0f, 1.6f, 1.6f, 1.35f, EVaelStatus::None, 0.0f,
		0.25f, 40.0f, 18.0f, FLinearColor(1.0f, 0.12f, 0.08f), 3.6f, 0.6f, 14.0f, 0.9f, 1.0f, 1.1f));

	LumpMaterial = TSoftObjectPtr<UMaterialInterface>(FSoftObjectPath(TEXT("/Game/Vael/Kreaturen/Eisenschlund/Tropfseuche/M_Tropfseuche_Klumpen.M_Tropfseuche_Klumpen")));
	for (const TCHAR* Name : { TEXT("A"), TEXT("B"), TEXT("C") })
	{
		LumpMeshes.Add(TSoftObjectPtr<UStaticMesh>(FSoftObjectPath(FString::Printf(TEXT("/Game/Vael/Kreaturen/Eisenschlund/Tropfseuche/SM_Tropfseuche_Klumpen_%s.SM_Tropfseuche_Klumpen_%s"), Name, Name))));
	}
	// Thicker and lumpier the rarer the strain (Boris 2026-11-21: "zu lame", thick patches and lumps)
	const float StrainPatches[] = { 0.66f, 0.62f, 0.66f, 0.6f, 0.56f };
	const int32 StrainLumps[] = { 7, 9, 8, 11, 14 };
	const float StrainCover[] = { 0.9f, 0.95f, 0.8f, 0.9f, 0.95f };
	const float StrainWobble[] = { 1.6f, 2.4f, 1.0f, 1.8f, 2.0f };
	for (int32 Index = 0; Index < Strains.Num() && Index < 5; ++Index)
	{
		Strains[Index].Patches = StrainPatches[Index];
		Strains[Index].PlayerLumps = StrainLumps[Index];
		Strains[Index].Cover = StrainCover[Index];
		Strains[Index].Wobble = StrainWobble[Index];
	}

	FavouritePlaces.Add(TEXT("L_Gildenmine"), EVaelSeuchenStamm::Verwesung);
	FavouritePlaces.Add(TEXT("L_KoboldStollen"), EVaelSeuchenStamm::Honig);
	FavouritePlaces.Add(TEXT("L_TiefsterSchacht"), EVaelSeuchenStamm::Kaelte);
	FavouritePlaces.Add(TEXT("L_Gebeinkammer"), EVaelSeuchenStamm::Nid);
	FavouritePlaces.Add(TEXT("L_FunkenerzAdern"), EVaelSeuchenStamm::Rausch);

	HostLevels.Add(TEXT("L_Gildenmine"));
	HostLevels.Add(TEXT("L_Gebeinkammer"));
}

const FVaelSeuchenStamm& UVaelTropfseucheSettings::GetStrain(EVaelSeuchenStamm Strain) const
{
	const int32 Index = static_cast<int32>(Strain);
	static const FVaelSeuchenStamm Fallback;
	return Strains.IsValidIndex(Index) ? Strains[Index] : Fallback;
}

// ------------------------------------------------------------------ Infestation on a player

UVaelSeuchenBefall::UVaelSeuchenBefall()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickInterval = 0.0f;
}

void UVaelSeuchenBefall::Start(EVaelSeuchenStamm InStrain, float Seconds)
{
	const bool bNewStrain = Shell.Num() == 0 || InStrain != Strain;
	Strain = InStrain;
	bEnded = false;
	if (bNewStrain)
	{
		BuildShell();
	}
	Flare = 1.0f;
}

void UVaelSeuchenBefall::BuildShell()
{
	for (USkeletalMeshComponent* Old : Shell)
	{
		if (Old != nullptr)
		{
			Old->DestroyComponent();
		}
	}
	Shell.Reset();

	ShellLook = UVaelTropfseuche::MakeLook(this, Strain);
	AActor* Owner = GetOwner();
	if (ShellLook == nullptr || Owner == nullptr)
	{
		return;
	}
	// A second skin over every visible body mesh that follows its pose: the overlay slot of the player belongs to the
	// balance mark, so the film can't go there
	// The player's own meshes are mostly hidden: the look shown is a MetaHuman actor hanging on the body
	TArray<USkeletalMeshComponent*> Bodies;
	Owner->GetComponents<USkeletalMeshComponent>(Bodies);
	TArray<AActor*> Attached;
	Owner->GetAttachedActors(Attached, true, true);
	for (const AActor* Look : Attached)
	{
		TArray<USkeletalMeshComponent*> LookBodies;
		Look->GetComponents<USkeletalMeshComponent>(LookBodies);
		Bodies.Append(LookBodies);
	}
	for (USkeletalMeshComponent* Body : Bodies)
	{
		if (Body == nullptr || !Body->IsVisible() || Body->bHiddenInGame || Body->GetSkeletalMeshAsset() == nullptr || Body->ComponentHasTag(TEXT("VaelSeuchenHuelle")))
		{
			continue;
		}
		USkeletalMeshComponent* Skin = NewObject<USkeletalMeshComponent>(Body->GetOwner());
		Skin->ComponentTags.Add(TEXT("VaelSeuchenHuelle"));
		Skin->SetSkeletalMesh(Body->GetSkeletalMeshAsset());
		Skin->SetupAttachment(Body);
		Skin->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Skin->SetCastShadow(false);
		Skin->RegisterComponent();
		Skin->SetLeaderPoseComponent(Body);
		for (int32 Slot = 0; Slot < Skin->GetNumMaterials(); ++Slot)
		{
			Skin->SetMaterial(Slot, ShellLook);
		}
		Shell.Add(Skin);
	}
	// Lumps on the body that carries the trunk (the one with the most bones among those with spine_03)
	USkeletalMeshComponent* Trunk = nullptr;
	for (USkeletalMeshComponent* Body : Bodies)
	{
		const FString BodyName = Body != nullptr ? Body->GetName() + GetNameSafe(Body->GetSkeletalMeshAsset()) : FString();
		if (Body != nullptr && Body->IsVisible() && !Body->bHiddenInGame && !Body->ComponentHasTag(TEXT("VaelSeuchenHuelle"))
			&& !BodyName.Contains(TEXT("Face")) && !BodyName.Contains(TEXT("Kopf")) && !BodyName.Contains(TEXT("Head"))
			&& Body->GetBoneIndex(TEXT("spine_03")) != INDEX_NONE && (Trunk == nullptr || Body->GetNumBones() > Trunk->GetNumBones()))
		{
			Trunk = Body;
		}
	}
	AddLumps(Trunk);
	UE_LOG(LogVael, Log, TEXT("Tropfseuche: film over %d of %d body meshes of '%s', %d lumps on %s"), Shell.Num(), Bodies.Num(), *Owner->GetName(),
		Lumps.Num(), *GetNameSafe(Trunk));
}

void UVaelSeuchenBefall::AddLumps(USkeletalMeshComponent* Body)
{
	for (UStaticMeshComponent* Old : Lumps)
	{
		if (Old != nullptr)
		{
			Old->DestroyComponent();
		}
	}
	Lumps.Reset();
	LumpLook = UVaelTropfseuche::MakeLumpLook(this, Strain);
	UVaelTropfseuche::AttachLumps(Body, LumpLook, UVaelTropfseucheSettings::Get()->GetStrain(Strain).PlayerLumps, 1.0f, Lumps);
}

void UVaelTropfseuche::AttachLumps(USkeletalMeshComponent* Body, UMaterialInstanceDynamic* Look, int32 Wanted, float Scale, TArray<TObjectPtr<UStaticMeshComponent>>& Lumps)
{
	const UVaelTropfseucheSettings* Settings = UVaelTropfseucheSettings::Get();
	if (Body == nullptr || Look == nullptr || Settings->LumpMeshes.Num() == 0 || Wanted <= 0)
	{
		return;
	}
	const int32 Start = Lumps.Num();
	// Where lumps can sit: a stretch of a limb or the trunk (from a bone to the next) and how far its surface lies from it
	struct FStretch
	{
		FName Bone;
		FName Next;
		float Radius;
	};
	static const FStretch Stretches[] = {
		{ TEXT("spine_01"), TEXT("spine_02"), 15.0f }, { TEXT("spine_02"), TEXT("spine_03"), 16.0f }, { TEXT("spine_03"), TEXT("neck_01"), 16.0f },
		{ TEXT("pelvis"), TEXT("spine_01"), 15.0f }, { TEXT("neck_01"), TEXT("head"), 6.5f },
		{ TEXT("clavicle_l"), TEXT("upperarm_l"), 7.0f }, { TEXT("clavicle_r"), TEXT("upperarm_r"), 7.0f },
		{ TEXT("upperarm_l"), TEXT("lowerarm_l"), 7.0f }, { TEXT("upperarm_r"), TEXT("lowerarm_r"), 7.0f },
		{ TEXT("lowerarm_l"), TEXT("hand_l"), 5.0f }, { TEXT("lowerarm_r"), TEXT("hand_r"), 5.0f },
		{ TEXT("thigh_l"), TEXT("calf_l"), 10.0f }, { TEXT("thigh_r"), TEXT("calf_r"), 10.0f },
		{ TEXT("calf_l"), TEXT("foot_l"), 7.0f }, { TEXT("calf_r"), TEXT("foot_r"), 7.0f }
	};
	TArray<const FStretch*> Usable;
	for (const FStretch& Stretch : Stretches)
	{
		if (Body->GetBoneIndex(Stretch.Bone) != INDEX_NONE && Body->GetBoneIndex(Stretch.Next) != INDEX_NONE)
		{
			Usable.Add(&Stretch);
		}
	}
	if (Usable.Num() == 0)
	{
		return;
	}
	// In patches: a few places, several lumps close together at each

	while (Lumps.Num() - Start < Wanted)
	{
		const FStretch& Stretch = *Usable[FMath::RandRange(0, Usable.Num() - 1)];
		const FVector From = Body->GetBoneLocation(Stretch.Bone);
		const FVector To = Body->GetBoneLocation(Stretch.Next);
		const FVector Axis = (To - From).GetSafeNormal();
		if (Axis.IsNearlyZero())
		{
			continue;
		}
		const FVector Side = FVector::CrossProduct(Axis, FVector::UpVector).IsNearlyZero() ? FVector::ForwardVector : FVector::CrossProduct(Axis, FVector::UpVector).GetSafeNormal();
		const float PatchAt = FMath::FRandRange(0.15f, 0.85f);
		const float PatchAngle = FMath::FRandRange(0.0f, 360.0f);
		const int32 InPatch = FMath::Min(FMath::RandRange(2, 4), Wanted - (Lumps.Num() - Start));
		for (int32 Index = 0; Index < InPatch; ++Index)
		{
			const float At = FMath::Clamp(PatchAt + FMath::FRandRange(-0.12f, 0.12f), 0.05f, 0.95f);
			const FVector Out = Side.RotateAngleAxis(PatchAngle + FMath::FRandRange(-40.0f, 40.0f), Axis);
			const float Size = FMath::FRandRange(5.0f, 11.0f) * Scale;
			UStaticMeshComponent* Lump = NewObject<UStaticMeshComponent>(Body->GetOwner());
			Lump->SetStaticMesh(Settings->LumpMeshes[FMath::RandRange(0, Settings->LumpMeshes.Num() - 1)].LoadSynchronous());
			Lump->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			Lump->SetCastShadow(false);
			Lump->SetMaterial(0, Look);
			Lump->RegisterComponent();
			// Sunk a little into the body, flat side down; the mesh is 5 cm across
			Lump->SetWorldLocationAndRotation(FMath::Lerp(From, To, At) + Out * (Stretch.Radius * 0.9f * Scale), FRotationMatrix::MakeFromZX(Out, Axis).Rotator());
			Lump->SetWorldScale3D(FVector(Size / 5.0f));
			Lump->AttachToComponent(Body, FAttachmentTransformRules::KeepWorldTransform, Stretch.Bone);
			Lumps.Add(Lump);
		}
	}
}

void UVaelSeuchenBefall::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	AActor* Owner = GetOwner();
	if (bEnded || Owner == nullptr)
	{
		return;
	}
	// Water washes it off; when the time ran out (or a potion took the status) it ends too
	if (UVaelCombatStatics::HasStatus(Owner, EVaelStatus::Wet))
	{
		UE_LOG(LogVael, Log, TEXT("Tropfseuche: water washes the infestation off '%s'"), *Owner->GetName());
		UVaelTropfseuche::Cure(Owner);
		return;
	}
	if (!UVaelCombatStatics::HasStatus(Owner, EVaelStatus::Infested))
	{
		End();
		return;
	}
	Flare = FMath::FInterpTo(Flare, 0.0f, DeltaTime, 3.0f);
	if (ShellLook != nullptr)
	{
		const FVaelSeuchenStamm& Look = UVaelTropfseucheSettings::Get()->GetStrain(Strain);
		// Faint on a player, so the figure stays readable; it flares up when the body resists
		ShellLook->SetScalarParameterValue(TEXT("Staerke"), Look.Glow * 0.1f * (0.6f + 1.5f * Flare));
		ShellLook->SetScalarParameterValue(TEXT("Flecken"), FMath::Max(0.0f, Look.Patches + 0.12f - 0.3f * Flare));
		if (LumpLook != nullptr)
		{
			LumpLook->SetScalarParameterValue(TEXT("Staerke"), Look.Glow * (0.1f + 0.5f * Flare));
		}
	}
}

void UVaelSeuchenBefall::Resisted()
{
	Flare = 1.0f;
}

void UVaelSeuchenBefall::End()
{
	if (bEnded)
	{
		return;
	}
	bEnded = true;
	for (USkeletalMeshComponent* Skin : Shell)
	{
		if (Skin != nullptr)
		{
			Skin->DestroyComponent();
		}
	}
	Shell.Reset();
	for (UStaticMeshComponent* Lump : Lumps)
	{
		if (Lump != nullptr && Lump->IsRegistered())
		{
			Lump->DestroyComponent();
		}
	}
	Lumps.Reset();
	DestroyComponent();
}

void UVaelSeuchenBefall::OnUnregister()
{
	for (USkeletalMeshComponent* Skin : Shell)
	{
		if (Skin != nullptr && Skin->IsRegistered())
		{
			Skin->DestroyComponent();
		}
	}
	Shell.Reset();
	for (UStaticMeshComponent* Lump : Lumps)
	{
		if (Lump != nullptr && Lump->IsRegistered())
		{
			Lump->DestroyComponent();
		}
	}
	Lumps.Reset();
	Super::OnUnregister();
}

// ------------------------------------------------------------------ The Tropfseuche

EVaelSeuchenStamm UVaelTropfseuche::PickStrain(const UWorld* World, const FVector& Location)
{
	const UVaelTropfseucheSettings* Settings = UVaelTropfseucheSettings::Get();
	TArray<float> Weights;
	for (const FVaelSeuchenStamm& Strain : Settings->Strains)
	{
		Weights.Add(Strain.Weight);
	}
	if (Weights.Num() == 0)
	{
		return EVaelSeuchenStamm::Verwesung;
	}
	if (World != nullptr)
	{
		const FString Level = AVaelIndoorLevel::PlaceKeyAt(World, Location);
		for (const TPair<FString, EVaelSeuchenStamm>& Place : Settings->FavouritePlaces)
		{
			if (Level.Contains(Place.Key) && Weights.IsValidIndex(static_cast<int32>(Place.Value)))
			{
				Weights[static_cast<int32>(Place.Value)] *= Settings->FavouriteWeight;
			}
		}
		// Its own slime nearby: once more as likely for each strain found within the radius
		TSet<EVaelSeuchenStamm> Near;
		for (const FIcicle& Icicle : GetIcicles(World))
		{
			if (FVector::Dist2D(Icicle.Location, Location) <= Settings->IcicleRadius)
			{
				Near.Add(Icicle.Strain);
			}
		}
		for (const EVaelSeuchenStamm Strain : Near)
		{
			if (Weights.IsValidIndex(static_cast<int32>(Strain)))
			{
				Weights[static_cast<int32>(Strain)] *= 2.0f;
			}
		}
	}
	float Total = 0.0f;
	for (const float Weight : Weights)
	{
		Total += Weight;
	}
	float Roll = FMath::FRandRange(0.0f, Total);
	for (int32 Index = 0; Index < Weights.Num(); ++Index)
	{
		Roll -= Weights[Index];
		if (Roll <= 0.0f)
		{
			return static_cast<EVaelSeuchenStamm>(Index);
		}
	}
	return EVaelSeuchenStamm::Verwesung;
}

UMaterialInstanceDynamic* UVaelTropfseuche::MakeLook(UObject* Outer, EVaelSeuchenStamm Strain)
{
	UMaterialInterface* Base = UVaelTropfseucheSettings::Get()->ShellMaterial.LoadSynchronous();
	if (Base == nullptr)
	{
		UE_LOG(LogVael, Warning, TEXT("Tropfseuche: the film material is missing (Scripts/BuildTropfseuche.py)"));
		return nullptr;
	}
	const FVaelSeuchenStamm& Look = UVaelTropfseucheSettings::Get()->GetStrain(Strain);
	UMaterialInstanceDynamic* Material = UMaterialInstanceDynamic::Create(Base, Outer);
	Material->SetVectorParameterValue(TEXT("Farbe"), Look.Color);
	// The film only glows in its patches; the lumps and the heart carry the light
	Material->SetScalarParameterValue(TEXT("Staerke"), Look.Glow * 0.1f);
	Material->SetScalarParameterValue(TEXT("Deckung"), Look.Cover);
	Material->SetScalarParameterValue(TEXT("Fluss"), Look.Flow);
	Material->SetScalarParameterValue(TEXT("Puls"), Look.Pulse);
	Material->SetScalarParameterValue(TEXT("Zucken"), Look.Jerk);
	Material->SetScalarParameterValue(TEXT("Wabern"), Look.Wobble);
	Material->SetScalarParameterValue(TEXT("Flecken"), Look.Patches);
	return Material;
}

UMaterialInstanceDynamic* UVaelTropfseuche::MakeLumpLook(UObject* Outer, EVaelSeuchenStamm Strain, bool bHeart)
{
	UMaterialInterface* Base = UVaelTropfseucheSettings::Get()->LumpMaterial.LoadSynchronous();
	if (Base == nullptr)
	{
		UE_LOG(LogVael, Warning, TEXT("Tropfseuche: the lump material is missing (Scripts/BuildTropfseuche.py)"));
		return nullptr;
	}
	const FVaelSeuchenStamm& Look = UVaelTropfseucheSettings::Get()->GetStrain(Strain);
	UMaterialInstanceDynamic* Material = UMaterialInstanceDynamic::Create(Base, Outer);
	Material->SetVectorParameterValue(TEXT("Farbe"), Look.Color);
	// The heart burns far brighter than the slime around it and beats faster
	Material->SetScalarParameterValue(TEXT("Staerke"), Look.Glow * (bHeart ? 2.0f : 0.25f));
	Material->SetScalarParameterValue(TEXT("Puls"), Look.Pulse * (bHeart ? 0.55f : 1.0f));
	Material->SetScalarParameterValue(TEXT("Zucken"), bHeart ? FMath::Max(Look.Jerk, 0.6f) : Look.Jerk);
	Material->SetScalarParameterValue(TEXT("Wabern"), bHeart ? 0.9f : 0.5f);
	return Material;
}

void UVaelTropfseuche::Infest(AActor* Attacker, AVaelCharacter* Player, EVaelSeuchenStamm Strain)
{
	if (Player == nullptr || Player->IsDowned())
	{
		return;
	}
	UVaelSeuchenBefall* Infestation = GetInfestation(Player);
	// A weaker strain doesn't push out a stronger one, it only renews it
	if (Infestation != nullptr && static_cast<int32>(Infestation->GetStrain()) > static_cast<int32>(Strain))
	{
		Strain = Infestation->GetStrain();
	}
	const FVaelSeuchenStamm& Settings = UVaelTropfseucheSettings::Get()->GetStrain(Strain);
	UVaelCombatStatics::ApplyStatus(Attacker, Player, EVaelStatus::Infested, Settings.InfestSeconds);
	if (!UVaelCombatStatics::HasStatus(Player, EVaelStatus::Infested))
	{
		return;
	}
	if (Infestation == nullptr)
	{
		Infestation = NewObject<UVaelSeuchenBefall>(Player);
		Infestation->RegisterComponent();
	}
	Infestation->Start(Strain, Settings.InfestSeconds);
	UE_LOG(LogVael, Log, TEXT("Tropfseuche: '%s' is infested (%s, %.0f s)"), *Player->GetName(), *Settings.Name.ToString(), Settings.InfestSeconds);
}

bool UVaelTropfseuche::Cure(AActor* Target)
{
	UVaelSeuchenBefall* Infestation = GetInfestation(Target);
	const bool bHad = UVaelCombatStatics::RemoveStatus(Target, EVaelStatus::Infested) || Infestation != nullptr;
	if (Infestation != nullptr)
	{
		Infestation->End();
	}
	return bHad;
}

UVaelSeuchenBefall* UVaelTropfseuche::GetInfestation(const AActor* Target)
{
	return Target != nullptr ? Target->FindComponentByClass<UVaelSeuchenBefall>() : nullptr;
}

bool UVaelTropfseuche::Resists(AActor* Player)
{
	UVaelSeuchenBefall* Infestation = GetInfestation(Player);
	if (Infestation == nullptr || !UVaelCombatStatics::HasStatus(Player, EVaelStatus::Infested))
	{
		return false;
	}
	if (FMath::FRand() >= UVaelTropfseucheSettings::Get()->GetStrain(Infestation->GetStrain()).Misfire)
	{
		return false;
	}
	// The body jerks and the action is lost
	Infestation->Resisted();
	if (AVaelCharacter* Character = Cast<AVaelCharacter>(Player))
	{
		Character->ApplyStun(0.3f);
	}
	UE_LOG(LogVael, Log, TEXT("Tropfseuche: the body of '%s' resists"), *Player->GetName());
	return true;
}

float UVaelTropfseuche::GetDrift(const AActor* Player, float Time)
{
	const UVaelSeuchenBefall* Infestation = GetInfestation(Player);
	if (Infestation == nullptr)
	{
		return 0.0f;
	}
	const float Most = UVaelTropfseucheSettings::Get()->GetStrain(Infestation->GetStrain()).Drift;
	return Most * (0.7f * FMath::Sin(Time * 1.3f) + 0.3f * FMath::Sin(Time * 3.1f + 1.0f));
}

bool UVaelTropfseuche::IsWaterAt(const UWorld* World, const FVector& Location)
{
	for (TActorIterator<AVaelGroundArea> It(World); World != nullptr && It; ++It)
	{
		if (It->GetElement() == EVaelElement::Water && !It->IsActorBeingDestroyed() && It->IsInRange(Location, 40.0f))
		{
			return true;
		}
	}
	return false;
}

void UVaelTropfseuche::SpawnHosts(UWorld* World, TFunctionRef<bool(const ATargetPoint*)> Take, TArray<TWeakObjectPtr<AVaelCreature>>* Born)
{
	const UVaelTropfseucheSettings* Settings = UVaelTropfseucheSettings::Get();
	if (World == nullptr || !World->IsGameWorld())
	{
		return;
	}
	// Hosts only in the caves that are theirs (in the joined caves: only in those regions)
	const auto IsHostPlace = [Settings, World](const FVector& Location)
	{
		const FString Level = AVaelIndoorLevel::PlaceKeyAt(World, Location);
		return Settings->HostLevels.ContainsByPredicate([&Level](const FString& Name) { return Level.Contains(Name); });
	};
	// Bodies: mostly infested Kobolde, the weak scrapers above all; now and then a skeleton (Boris 2026-11-21)
	const auto PickBody = []() -> UVaelSchleimgaengerData*
	{
		const float Roll = FMath::FRand();
		UVaelSchleimgaengerData* Body = GetMutableDefault<UVaelSchleimgaengerData>();
		if (Roll < 0.45f)
		{
			Body = GetMutableDefault<UVaelKoboldSchleimkratzerData>();
		}
		else if (Roll < 0.6f)
		{
			Body = GetMutableDefault<UVaelKoboldHortwaechterData>();
		}
		else if (Roll < 0.72f)
		{
			Body = GetMutableDefault<UVaelKoboldKontoraeltesterData>();
		}
		else
		{
			return Body;
		}
		// About every second Kobold host is the variant of its body (Boris 2026-11-22: more variety)
		return Cast<UVaelSchleimgaengerData>(UVaelKobolde::MaybeVariant(Body));
	};
	int32 Places = 0;
	int32 Hosts = 0;
	for (TActorIterator<ATargetPoint> It(World); It; ++It)
	{
		if (!It->ActorHasTag(TEXT("VaelHoehleGegner")) || !IsHostPlace(It->GetActorLocation()) || !Take(*It))
		{
			continue;
		}
		++Places;
		if (FMath::FRand() > Settings->PlaceChance)
		{
			continue;
		}
		const int32 Count = FMath::RandRange(Settings->HostsPerPlace.X, FMath::Max(Settings->HostsPerPlace.X, Settings->HostsPerPlace.Y));
		for (int32 Index = 0; Index < Count; ++Index)
		{
			const FVector Spot = It->GetActorLocation() + FVector(FMath::FRandRange(-160.0f, 160.0f), FMath::FRandRange(-160.0f, 160.0f), 200.0f);
			FVector Ground;
			if (!AVaelCreature::FindGround(World, Spot, Ground))
			{
				continue;
			}
			const EVaelSeuchenStamm Strain = PickStrain(World, Ground);
			if (AVaelCreature* Host = AVaelCreature::SpawnCreature(World, PickBody(), Ground, FRotator(0.0f, FMath::FRandRange(0.0f, 360.0f), 0.0f), nullptr,
				[Strain](AVaelCreature* Creature)
				{
					if (AVaelSchleimgaenger* Body = Cast<AVaelSchleimgaenger>(Creature))
					{
						Body->SetStrain(Strain);
					}
				}))
			{
				++Hosts;
				if (Born != nullptr)
				{
					Born->Add(Host);
				}
			}
		}
	}
	if (Hosts > 0)
	{
		UE_LOG(LogVael, Log, TEXT("Tropfseuche: %d Schleimgaenger at %d places in %s"), Hosts, Places, *UWorld::RemovePIEPrefix(World->GetMapName()));
	}
}

#undef LOCTEXT_NAMESPACE
