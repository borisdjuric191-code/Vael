// Copyright Epic Games, Inc. All Rights Reserved.

#include "Creatures/VaelErztochter.h"

#include "Components/PoseableMeshComponent.h"
#include "Vael.h"

#define LOCTEXT_NAMESPACE "VaelErztochter"

namespace
{
	struct FTochterRow { const TCHAR* Bone; const TCHAR* Zone; FVector Location; FVector Normal; float Radius; float Depth; };
	const FTochterRow TochterRows[] = {
#include "Creatures/VaelErztochterFassungen.inl"
	};

	const FName ZoneKamm(TEXT("Kamm"));
	const FName ZoneKammFuss(TEXT("KammFuss"));
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
		Fassung.Radius = Row.Radius;
		Fassung.Depth = Row.Depth;
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

	FRandomStream Random(CrystalSeed);
	for (const FVaelAmeisenFassung& Fassung : Data->Fassungen)
	{
		bool bLead = true;
		float Age = Data->CrystalGrowth;
		float LengthScale = 1.0f;
		if (Fassung.Zone == ZoneKamm)
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
			LengthScale = Data->CrestLength * 0.75f;
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
		const TArray<const FVaelAmeisenKristall*>& Pieces = bLead ? LeadPieces : CompanionPieces;
		PlaceCrystal(Fassung, *Pieces[Random.RandRange(0, Pieces.Num() - 1)], Age, LengthScale, Random);
	}
	UE_LOG(LogVael, Log, TEXT("Erztochter %s: %d crystals (%s + %s)"), *GetName(), Crystals.Num(), *Lead.ToString(), *Companion.ToString());
}

#undef LOCTEXT_NAMESPACE
