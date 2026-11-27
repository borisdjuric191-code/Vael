// Copyright Epic Games, Inc. All Rights Reserved.

#include "Creatures/VaelErztochter.h"

#include "Components/PoseableMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Magic/VaelSpellEffects.h"
#include "Vael.h"

#define LOCTEXT_NAMESPACE "VaelErztochter"

namespace
{
	struct FTochterRow { const TCHAR* Bone; const TCHAR* Zone; FVector Location; FVector Normal; FVector Surface; float Radius; float Depth; };
	const FTochterRow TochterRows[] = {
#include "Creatures/VaelErztochterFassungen.inl"
	};

	struct FBogenRow { const TCHAR* Ore; const TCHAR* Mesh; float Width; float Length; };
	const FBogenRow BogenRows[] = {
#include "Creatures/VaelErztochterBogen.inl"
	};

	const FName ZoneKamm(TEXT("Kamm"));
	const FName ZoneKammFuss(TEXT("KammFuss"));
	const FName ZoneKammBelag(TEXT("KammBelag"));
	const FName ZoneFluegelBelag(TEXT("FluegelBelag"));

	/** The most elongated pieces of an ore (at least one): the coat on horns and stumps grows in long lines */
	TArray<const FVaelAmeisenKristall*> LongPieces(TArray<const FVaelAmeisenKristall*> Pieces)
	{
		Pieces.Sort([](const FVaelAmeisenKristall& A, const FVaelAmeisenKristall& B) { return A.Length / A.Width > B.Length / B.Width; });
		Pieces.SetNum(FMath::Max(1, (Pieces.Num() + 1) / 2));
		return Pieces;
	}
	const FName ZoneBein(TEXT("Bein"));
	const FName ZoneKopf(TEXT("Kopf"));
	const FName ZoneSattel(TEXT("Sattel"));
	const FName ZoneHinterleib(TEXT("Hinterleib"));
	const FName ZoneFluegel(TEXT("Fluegel"));
}

UVaelErztochterData::UVaelErztochterData()
{
	DisplayName = LOCTEXT("Erztochter", "Erztochter");
	CompendiumId = TEXT("Erztochter");
	KindId = TEXT("Erztochter");
	CreatureClass = AVaelErztochter::StaticClass();

	// Eyes on both sides of the head, behind the feelers (measured on SK_Erztochter in Blender)
	EyeLocations = {FVector(0.166f, -0.675f, 0.875f), FVector(-0.1875f, -0.675f, 0.875f)};
	EyeNormals = {FVector(0.967f, -0.202f, 0.155f), FVector(-0.84f, -0.45f, 0.302f)};
	Mesh = TSoftObjectPtr<USkeletalMesh>(FSoftObjectPath(TEXT("/Game/Vael/Kreaturen/Eisenschlund/Erztochter/SK_Erztochter.SK_Erztochter")));

	MaxHealth = 520.0f;
	MoveSpeed = 240.0f;
	CollisionRadius = 95.0f;
	CollisionHalfHeight = 70.0f;
	KnockbackMultiplier = 0.25f;
	AggroRange = 1600.0f;
	BiteDamage = 22.0f;
	BiteReach = 130.0f;
	BiteInterval = 1.7f;
	WanderRadius = 350.0f;
	BodyColor = FLinearColor(0.90f, 0.80f, 0.62f);

	// Her own settings; the crystal pieces are the ones of the workers
	Fassungen.Reset();
	for (const FTochterRow& Row : TochterRows)
	{
		FVaelAmeisenFassung& Fassung = Fassungen.AddDefaulted_GetRef();
		Fassung.Bone = Row.Bone;
		Fassung.Zone = Row.Zone;
		Fassung.Location = Row.Location;
		Fassung.Normal = Row.Normal;
		Fassung.Surface = Row.Surface;
		Fassung.Radius = Row.Radius;
		Fassung.Depth = Row.Depth;
	}
	for (const FBogenRow& Row : BogenRows)
	{
		FVaelAmeisenKristall& Kristall = BentKristalle.AddDefaulted_GetRef();
		Kristall.Ore = Row.Ore;
		Kristall.Mesh = TSoftObjectPtr<UStaticMesh>(FSoftObjectPath(Row.Mesh));
		Kristall.Width = Row.Width;
		Kristall.Length = Row.Length;
	}
}

TSubclassOf<UVaelCreatureData> AVaelErztochter::GetDefaultDataClass() const
{
	return UVaelErztochterData::StaticClass();
}

void AVaelErztochter::BeginPlay()
{
	Super::BeginPlay();
	SetElite();
}

void AVaelErztochter::GetPairOres(int32 InPair, FName& OutLead, FName& OutCompanion)
{
	static const TCHAR* const Pairs[5][2] = {
		{TEXT("Schwefelkies"), TEXT("Funkenerz")},
		{TEXT("Rosteisen"), TEXT("Salzspat")},
		{TEXT("Gruenspan"), TEXT("Drahtsilber")},
		{TEXT("Blutzinnober"), TEXT("Siechglas")},
		// Their harvest is the protection money of the Kobolde: Kobaltbluete leads
		{TEXT("Kobaltbluete"), TEXT("Rippenspat")},
	};
	const int32 Index = FMath::Clamp(InPair, 1, 5) - 1;
	OutLead = Pairs[Index][0];
	OutCompanion = Pairs[Index][1];
}

void AVaelErztochter::SetupDaughter(int32 InPair, bool bInEnslaved)
{
	Pair = FMath::Clamp(InPair, 1, 5);
	FName Lead, Companion;
	GetPairOres(Pair, Lead, Companion);
	SetupAnt(Lead, false, bInEnslaved);
}

void AVaelErztochter::BuildCrystals()
{
	ClearCrystals();

	const UPoseableMeshComponent* QueenMesh = GetFactoryMesh();
	const UVaelErztochterData* Data = GetData<UVaelErztochterData>();
	if (QueenMesh == nullptr || QueenMesh->GetSkinnedAsset() == nullptr)
	{
		return;
	}
	FName Lead, Companion;
	GetPairOres(Pair, Lead, Companion);
	Ore = Lead;

	TArray<const FVaelAmeisenKristall*> LeadPieces, CompanionPieces;
	for (const FVaelAmeisenKristall& Kristall : Data->Kristalle)
	{
		if (Kristall.Ore == Lead)
		{
			LeadPieces.Add(&Kristall);
		}
		else if (Kristall.Ore == Companion)
		{
			CompanionPieces.Add(&Kristall);
		}
	}
	if (LeadPieces.IsEmpty() || CompanionPieces.IsEmpty())
	{
		UE_LOG(LogVael, Warning, TEXT("Erztochter: no crystals for %s / %s"), *Lead.ToString(), *Companion.ToString());
		return;
	}

	// The coat uses the bent pieces (Tools/Ameisen/kristalle_biegen.py), the most elongated ones; straight ones if missing
	TArray<const FVaelAmeisenKristall*> LeadBent, CompanionBent;
	for (const FVaelAmeisenKristall& Kristall : Data->BentKristalle)
	{
		if (Kristall.Ore == Lead)
		{
			LeadBent.Add(&Kristall);
		}
		else if (Kristall.Ore == Companion)
		{
			CompanionBent.Add(&Kristall);
		}
	}
	const TArray<const FVaelAmeisenKristall*> LeadLong = LongPieces(LeadBent.IsEmpty() ? LeadPieces : LeadBent);
	const TArray<const FVaelAmeisenKristall*> CompanionLong = LongPieces(CompanionBent.IsEmpty() ? CompanionPieces : CompanionBent);

	FRandomStream Random(CrystalSeed);
	for (const FVaelAmeisenFassung& Fassung : Data->Fassungen)
	{
		bool bLead = true;
		float Age = Data->CrystalGrowth;
		float LengthScale = 1.0f;
		const bool bCoat = Fassung.Zone == ZoneKammBelag || Fassung.Zone == ZoneKammFuss || Fassung.Zone == ZoneFluegelBelag;
		if (Fassung.Zone == ZoneKammBelag)
		{
			// Horns overgrown by the lead ore: close rows leaning to the tip, the horn itself hardly shows
			Age = 0.0f;
			LengthScale = Data->CoatLength;
		}
		else if (Fassung.Zone == ZoneFluegelBelag)
		{
			// Stumps overgrown by the companion ore, leaning outwards along the stump
			bLead = false;
			Age = 0.0f;
			LengthScale = Data->CoatLength;
		}
		else if (Fassung.Zone == ZoneKamm)
		{
			// The crown: long spikes of the lead ore along both horns
			Age = 0.0f;
			LengthScale = Data->CrestLength;
		}
		else if (Fassung.Zone == ZoneKammFuss)
		{
			// The foot of the horns: the companion ore, a little shorter
			bLead = false;
			Age = 0.0f;
			LengthScale = Data->CoatLength * 0.8f;
		}
		else if (Fassung.Zone == ZoneBein)
		{
			// A few small ones on the legs, both ores
			bLead = Random.FRand() < 0.5f;
			Age = 0.0f;
			LengthScale = 1.2f;
		}
		else if (Fassung.Zone == ZoneKopf)
		{
			bLead = false;
			Age = Data->CrystalGrowth * 0.5f;
		}
		else if (Fassung.Zone == ZoneSattel)
		{
			// Middle of the saddle lead, rims companion
			bLead = FMath::Abs(Fassung.Location.X) < Data->SaddleCore;
		}
		else if (Fassung.Zone == ZoneHinterleib)
		{
			// The ores take turns band by band down the abdomen
			bLead = FMath::FloorToInt(Fassung.Location.Y / Data->AbdomenBand) % 2 == 0;
		}
		else if (Fassung.Zone == ZoneFluegel)
		{
			// Splinters along the torn edges of the stumps
			bLead = false;
			Age = 0.0f;
			LengthScale = Data->WingLength;
		}
		if ((Fassung.Zone == ZoneKopf || Fassung.Zone == ZoneSattel || Fassung.Zone == ZoneHinterleib) && Random.FRand() < Data->MixChance)
		{
			bLead = !bLead;
		}
		const TArray<const FVaelAmeisenKristall*>& Pieces = bCoat ? (bLead ? LeadLong : CompanionLong) : (bLead ? LeadPieces : CompanionPieces);
		// Rooted in the body: the coat deepest (it lies flat), the settings a little below their floor
		const float Sink = bCoat ? Data->CoatSink : Data->CrystalSink;
		PlaceCrystal(Fassung, *Pieces[Random.RandRange(0, Pieces.Num() - 1)], Age, LengthScale, Random, bCoat ? Data->CoatJitter : 1.0f,
			bCoat && !LeadBent.IsEmpty() && !CompanionBent.IsEmpty(), Sink);
	}
	UE_LOG(LogVael, Log, TEXT("Erztochter %s: %d crystals (%s + %s)"), *GetName(), CrystalCount, *Lead.ToString(), *Companion.ToString());
	BuildEyes();
}

void AVaelErztochter::BuildEyes()
{
	for (UStaticMeshComponent* Part : EyeParts)
	{
		if (Part != nullptr)
		{
			Part->DestroyComponent();
		}
	}
	EyeParts.Reset();
	Eyes.Reset();

	UPoseableMeshComponent* QueenMesh = GetFactoryMesh();
	const UVaelErztochterData* Data = GetData<UVaelErztochterData>();
	if (QueenMesh == nullptr || QueenMesh->GetSkinnedAsset() == nullptr)
	{
		return;
	}
	UMaterialInterface* CoreMaterial = nullptr;
	UMaterialInterface* GlowMaterial = nullptr;
	VaelEffects::LoadLookMaterials(CoreMaterial, GlowMaterial);
	const FLinearColor Color = bEnslaved ? Data->EnslavedEyeColor : Data->EyeColor;
	const FName HeadBone(TEXT("head"));
	const FTransform BoneTransform = QueenMesh->GetBoneTransformByName(HeadBone, EBoneSpaces::ComponentSpace);
	const float Radius = Data->EyeRadius * 100.0f;

	for (int32 Index = 0; Index < FMath::Min(Data->EyeLocations.Num(), Data->EyeNormals.Num()); ++Index)
	{
		// Blender model space (metres) to the mesh (cm, Y flips on import)
		const FVector& L = Data->EyeLocations[Index];
		const FVector& N = Data->EyeNormals[Index];
		FEye& Eye = Eyes.AddDefaulted_GetRef();
		Eye.Normal = FVector(N.X, -N.Y, N.Z).GetSafeNormal();
		// The eyeball bulges a little out of its socket
		Eye.Centre = FVector(L.X, -L.Y, L.Z) * 100.0f - Eye.Normal * Radius * 0.45f;
		Eye.Bone = BoneTransform;

		Eye.Ball = VaelEffects::AddLookShape(this, CoreMaterial, Color, Data->EyeGlow, 0.6f);
		Eye.Core = VaelEffects::AddLookShape(this, GlowMaterial, Color, Data->EyeGlow * 2.0f);
		for (UStaticMeshComponent* Part : {Eye.Ball, Eye.Core})
		{
			Part->AttachToComponent(QueenMesh, FAttachmentTransformRules::KeepRelativeTransform, HeadBone);
			Part->SetVisibility(true);
			EyeParts.Add(Part);
		}
		// Oval, a little flat along its axis (the sphere mesh is 100 cm across)
		const FQuat Facing = FQuat::FindBetweenNormals(FVector::UpVector, Eye.Normal);
		const FTransform BallInMesh(Facing, Eye.Centre, FVector(1.15f, 0.95f, 0.8f) * (2.0f * Radius / 100.0f));
		Eye.Ball->SetRelativeTransform(BallInMesh.GetRelativeTransform(Eye.Bone));
	}
	UpdateEyes(0.0f);
}

void AVaelErztochter::OnDamageTaken(float Damage, const FGameplayTagContainer& DamageTags)
{
	Super::OnDamageTaken(Damage, DamageTags);
	EyeFlare = 1.0f;
}

void AVaelErztochter::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	UpdateEyes(DeltaSeconds);
}

void AVaelErztochter::UpdateEyes(float DeltaSeconds)
{
	if (Eyes.IsEmpty() || GetFactoryMesh() == nullptr)
	{
		return;
	}
	const UVaelErztochterData* Data = GetData<UVaelErztochterData>();
	EyeTime += DeltaSeconds;
	EyeFlare = FMath::Max(0.0f, EyeFlare - DeltaSeconds * 2.5f);
	EyeAnger = FMath::FInterpTo(EyeAnger, (bEnslaved || IsAngry()) ? 1.0f : 0.0f, DeltaSeconds, 2.0f);
	EyeLife = FMath::FInterpTo(EyeLife, IsDead() ? 0.0f : 1.0f, DeltaSeconds, 0.8f);

	// A slow breath of light, a little faster when she fights; a flare when hit; it fades when she dies
	const float Breath = 0.78f + 0.22f * FMath::Sin(EyeTime * (1.1f + EyeAnger * 1.6f));
	const float Glow = (FMath::Lerp(Data->EyeGlow, Data->AngryEyeGlow, EyeAnger) * Breath + EyeFlare * 6.0f) * EyeLife;

	// The light inside drifts slowly while she works and turns to her target when she fights
	const AActor* Target = (EyeAnger > 0.2f && EyeLife > 0.5f) ? FindTarget(Data->AggroRange) : nullptr;
	const FTransform MeshToWorld = GetFactoryMesh()->GetComponentTransform();
	const float Radius = Data->EyeRadius * 100.0f;
	for (int32 Index = 0; Index < Eyes.Num(); ++Index)
	{
		FEye& Eye = Eyes[Index];
		FVector Side1, Side2;
		Eye.Normal.FindBestAxisVectors(Side1, Side2);
		FVector Wish = (Side1 * FMath::Sin(EyeTime * 0.37f + Index * 2.1f) + Side2 * FMath::Sin(EyeTime * 0.23f + Index)) * 0.5f;
		if (Target != nullptr)
		{
			const FVector ToTarget = MeshToWorld.InverseTransformVectorNoScale(Target->GetActorLocation() - MeshToWorld.TransformPosition(Eye.Centre)).GetSafeNormal();
			Wish = ToTarget - Eye.Normal * FVector::DotProduct(ToTarget, Eye.Normal);
		}
		Eye.Gaze = DeltaSeconds > 0.0f ? FMath::VInterpTo(Eye.Gaze, Wish.GetClampedToMaxSize(1.0f), DeltaSeconds, Target != nullptr ? 5.0f : 1.2f) : Wish.GetClampedToMaxSize(1.0f);

		const FVector CoreAt = Eye.Centre + Eye.Normal * Radius * 0.5f + Eye.Gaze * Radius * 0.35f;
		const float CoreSize = (0.42f + 0.08f * Breath + EyeFlare * 0.25f) * (2.0f * Radius / 100.0f) * FMath::Max(0.05f, EyeLife);
		Eye.Core->SetRelativeTransform(FTransform(FQuat::Identity, CoreAt, FVector(CoreSize)).GetRelativeTransform(Eye.Bone));
		VaelEffects::SetLookGlow(Eye.Ball, Glow);
		VaelEffects::SetLookGlow(Eye.Core, Glow * 2.2f);
	}
}

#undef LOCTEXT_NAMESPACE
