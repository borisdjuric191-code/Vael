// Copyright Epic Games, Inc. All Rights Reserved.

#include "Creatures/VaelSchuerfameise.h"

#include "Components/PoseableMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "EngineUtils.h"
#include "Vael.h"

#define LOCTEXT_NAMESPACE "VaelSchuerfameise"

namespace
{
	/** Blender model space (metres, faces -Y) to the component space of the imported mesh (cm): Y flips on FBX import */
	FVector BlenderToMesh(const FVector& V, float Scale)
	{
		return FVector(V.X, -V.Y, V.Z) * Scale;
	}

	struct FFassungRow { const TCHAR* Bone; FVector Location; FVector Normal; float Radius; float Depth; };
	const FFassungRow FassungRows[] = {
#include "Creatures/VaelSchuerfameiseFassungen.inl"
	};

	struct FKristallRow { const TCHAR* Ore; const TCHAR* Mesh; float Width; float Length; };
	const FKristallRow KristallRows[] = {
#include "Creatures/VaelSchuerfameiseKristalle.inl"
	};
}

UVaelSchuerfameiseData::UVaelSchuerfameiseData()
{
	// Lives in the dark of the caves, awake day and night
	DayAwareness = 1.0f;
	NightAwareness = 1.0f;
	DisplayName = LOCTEXT("Schuerfameise", "Schürfameise");
	CompendiumId = TEXT("Schuerfameise");
	KindId = TEXT("Schuerfameise");
	CreatureClass = AVaelSchuerfameise::StaticClass();
	Mesh = TSoftObjectPtr<USkeletalMesh>(FSoftObjectPath(TEXT("/Game/Vael/Kreaturen/Eisenschlund/Schuerfameise/SK_Schuerfameise.SK_Schuerfameise")));

	MaxHealth = 60.0f;
	MoveSpeed = 280.0f;
	CollisionRadius = 45.0f;
	CollisionHalfHeight = 35.0f;
	// Crystal is brittle: earth tremors break the shell
	ElementMultipliers.Add(EVaelElement::Earth, 1.5f);
	KnockbackMultiplier = 0.7f;
	AggroRange = 1400.0f;
	BodyColor = FLinearColor(0.86f, 0.80f, 0.68f);

	for (const FFassungRow& Row : FassungRows)
	{
		FVaelAmeisenFassung& Fassung = Fassungen.AddDefaulted_GetRef();
		Fassung.Bone = Row.Bone;
		Fassung.Location = Row.Location;
		Fassung.Normal = Row.Normal;
		Fassung.Radius = Row.Radius;
		Fassung.Depth = Row.Depth;
	}
	for (const FKristallRow& Row : KristallRows)
	{
		FVaelAmeisenKristall& Kristall = Kristalle.AddDefaulted_GetRef();
		Kristall.Ore = Row.Ore;
		Kristall.Mesh = TSoftObjectPtr<UStaticMesh>(FSoftObjectPath(Row.Mesh));
		Kristall.Width = Row.Width;
		Kristall.Length = Row.Length;
	}
}

AVaelSchuerfameise::AVaelSchuerfameise()
{
	CrystalSeed = FMath::Rand();
}

TSubclassOf<UVaelCreatureData> AVaelSchuerfameise::GetDefaultDataClass() const
{
	return UVaelSchuerfameiseData::StaticClass();
}

void AVaelSchuerfameise::BeginPlay()
{
	Super::BeginPlay();
	WanderGoal = HomeLocation = GetActorLocation();
	if (!bSetUp)
	{
		SetupAnt(Ore, bAncestor, bEnslaved);
	}
}

void AVaelSchuerfameise::SetupAnt(FName InOre, bool bInAncestor, bool bInEnslaved)
{
	const UVaelSchuerfameiseData* Data = GetData<UVaelSchuerfameiseData>();
	Ore = InOre;
	bEnslaved = bInEnslaved;
	if (bInAncestor && !bAncestor)
	{
		SetActorScale3D(FVector(Data->AncestorScale));
		SetElite();
	}
	bAncestor = bInAncestor;
	bSetUp = true;
	BuildCrystals();
}

void AVaelSchuerfameise::ClearCrystals()
{
	for (UStaticMeshComponent* Crystal : Crystals)
	{
		if (Crystal != nullptr)
		{
			Crystal->DestroyComponent();
		}
	}
	Crystals.Reset();
	CrystalCount = 0;
}

void AVaelSchuerfameise::AddCrystal(UStaticMesh* PieceMesh, FName Bone, const FTransform& InMesh)
{
	UPoseableMeshComponent* AntMesh = GetFactoryMesh();
	const FTransform BoneTransform = AntMesh->GetBoneTransformByName(Bone, EBoneSpaces::ComponentSpace);
	UStaticMeshComponent* Crystal = NewObject<UStaticMeshComponent>(this);
	Crystal->SetStaticMesh(PieceMesh);
	Crystal->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Crystal->SetCanEverAffectNavigation(false);
	Crystal->SetupAttachment(AntMesh, Bone);
	Crystal->SetRelativeTransform(InMesh.GetRelativeTransform(BoneTransform));
	Crystal->RegisterComponent();
	Crystals.Add(Crystal);
	++CrystalCount;
}

void AVaelSchuerfameise::BuildCrystals()
{
	ClearCrystals();

	UPoseableMeshComponent* AntMesh = GetFactoryMesh();
	const UVaelSchuerfameiseData* Data = GetData<UVaelSchuerfameiseData>();
	if (AntMesh == nullptr || AntMesh->GetSkinnedAsset() == nullptr)
	{
		return;
	}
	TArray<const FVaelAmeisenKristall*> Pieces;
	for (const FVaelAmeisenKristall& Kristall : Data->Kristalle)
	{
		if (Kristall.Ore == Ore)
		{
			Pieces.Add(&Kristall);
		}
	}
	if (Pieces.IsEmpty())
	{
		UE_LOG(LogVael, Warning, TEXT("Schuerfameise: no crystals for ore %s"), *Ore.ToString());
		return;
	}

	FRandomStream Random(CrystalSeed);
	// The biggest settings fill first, with a little chance; young workers leave the rest empty
	TArray<const FVaelAmeisenFassung*> Order;
	for (const FVaelAmeisenFassung& Fassung : Data->Fassungen)
	{
		Order.Add(&Fassung);
	}
	TMap<const FVaelAmeisenFassung*, float> Keys;
	for (const FVaelAmeisenFassung* Fassung : Order)
	{
		Keys.Add(Fassung, Fassung->Radius + Random.FRandRange(0.0f, 0.02f));
	}
	Order.Sort([&Keys](const FVaelAmeisenFassung& A, const FVaelAmeisenFassung& B) { return Keys[&A] > Keys[&B]; });
	const int32 Count = bAncestor ? Order.Num() : FMath::Max(1, FMath::RoundToInt(Order.Num() * Data->WorkerFill));
	const float Age = bAncestor ? Data->AncestorGrowth : 0.0f;

	for (int32 Index = 0; Index < Count; ++Index)
	{
		const FVaelAmeisenKristall& Piece = *Pieces[Random.RandRange(0, Pieces.Num() - 1)];
		PlaceCrystal(*Order[Index], Piece, Age, 1.0f, Random);
	}
	UE_LOG(LogVael, Verbose, TEXT("Schuerfameise %s: %d crystals (%s)"), *GetName(), CrystalCount, *Ore.ToString());
}

void AVaelSchuerfameise::PlaceCrystal(const FVaelAmeisenFassung& Fassung, const FVaelAmeisenKristall& Piece, float Age, float LengthScale, FRandomStream& Random, float Jitter)
{
	UStaticMesh* PieceMesh = Piece.Mesh.LoadSynchronous();
	if (PieceMesh == nullptr)
	{
		return;
	}
	// Meshes are cut at 1 m = 100 cm; the models are imported 1:1 in cm
	constexpr float MetresToCm = 100.0f;
	const FVector Floor = BlenderToMesh(Fassung.Location, MetresToCm);
	const FVector Normal = BlenderToMesh(Fassung.Normal, 1.0f).GetSafeNormal();
	const float Radius = Fassung.Radius * MetresToCm;
	const float Depth = Fassung.Depth * MetresToCm;

	// One thick piece per setting, sized to it; it stands on the floor and rises out of the setting
	// (same rules as Tools/Ameisen/ameise_kristalle.py, mode 'spitze')
	const float Width = 2.0f * Radius * 0.7f * (1.0f + Age * 0.4f) * FMath::Lerp(1.0f, static_cast<float>(Random.FRandRange(0.9f, 1.08f)), Jitter);
	// Tight fit (the coat of the Erztochter): measure the whole piece, not its core, so wide or bent pieces (Rippenspat)
	// do not swell beyond their place
	float PieceWidth = Piece.Width * MetresToCm;
	if (Jitter < 1.0f)
	{
		const FBoxSphereBounds Bounds = PieceMesh->GetBounds();
		PieceWidth = FMath::Max(PieceWidth, 2.0f * FMath::Max(Bounds.BoxExtent.X, Bounds.BoxExtent.Y) * 0.8f);
	}
	const float ScaleXY = Width / PieceWidth;
	const float Length = Piece.Length * MetresToCm * ScaleXY;
	const float Goal = (Depth + Radius * (2.4f + Age * 1.8f)) * LengthScale * FMath::Lerp(1.0f, static_cast<float>(Random.FRandRange(0.85f, 1.2f)), Jitter);
	const float Stretch = (Piece.Length / Piece.Width > 1.2f ? 2.4f : 1.5f) * FMath::Max(1.0f, LengthScale);
	const float ScaleZ = ScaleXY * FMath::Clamp(Goal / Length, 1.0f, Stretch);

	struct FPlace { FVector Location; FVector Axis; float XY; float Z; };
	TArray<FPlace, TInlineAllocator<4>> Places;
	Places.Add({Floor - Normal * Radius * 0.12f, Normal, ScaleXY, ScaleZ});
	if (Age > 0.2f)
	{
		// Ahnenpanzer: side spikes leaning out of the rim make the shell denser and more jagged
		FVector Side1, Side2;
		Normal.FindBestAxisVectors(Side1, Side2);
		const float Start = Random.FRandRange(0.0f, UE_TWO_PI);
		const int32 Sides = Radius < 2.0f ? 2 : 3;
		for (int32 SideIndex = 0; SideIndex < Sides; ++SideIndex)
		{
			const float Angle = Start + UE_TWO_PI * SideIndex / Sides + Random.FRandRange(-0.4f, 0.4f);
			const FVector Out = Side1 * FMath::Cos(Angle) + Side2 * FMath::Sin(Angle);
			const FVector Axis = (Normal + Out * FMath::Tan(FMath::DegreesToRadians(Random.FRandRange(25.0f, 40.0f)))).GetSafeNormal();
			const float Factor = Random.FRandRange(0.45f, 0.6f);
			Places.Add({Floor + Out * Radius * 0.7f, Axis, ScaleXY * Factor, ScaleZ * Factor * static_cast<float>(Random.FRandRange(0.9f, 1.2f))});
		}
	}
	for (const FPlace& Place : Places)
	{
		const FQuat Spin(Place.Axis, Random.FRandRange(0.0f, UE_TWO_PI));
		FVector TiltAxis, Unused;
		Place.Axis.FindBestAxisVectors(TiltAxis, Unused);
		const FQuat Tilt(TiltAxis, FMath::DegreesToRadians(Random.FRandRange(-8.0f, 8.0f) * Jitter));
		const FQuat Rotation = Tilt * Spin * FQuat::FindBetweenNormals(FVector::UpVector, Place.Axis);
		AddCrystal(PieceMesh, Fassung.Bone, FTransform(Rotation, Place.Location, FVector(Place.XY, Place.XY, Place.Z)));
	}
}

void AVaelSchuerfameise::Anger()
{
	AngerTime = GetData<UVaelSchuerfameiseData>()->AngerSeconds;
}

void AVaelSchuerfameise::OnDamageTaken(float Damage, const FGameplayTagContainer& DamageTags)
{
	Super::OnDamageTaken(Damage, DamageTags);
	if (bEnslaved)
	{
		return;
	}
	Anger();
	// The colony defends its own: free ants of the same ore nearby join in
	const float Range = GetData<UVaelSchuerfameiseData>()->AlarmRange;
	for (TActorIterator<AVaelSchuerfameise> It(GetWorld()); It; ++It)
	{
		if (*It != this && !It->bEnslaved && It->Ore == Ore && FVector::Dist2D(It->GetActorLocation(), GetActorLocation()) < Range)
		{
			It->Anger();
		}
	}
}

void AVaelSchuerfameise::TickBehavior(float DeltaSeconds)
{
	const UVaelSchuerfameiseData* Data = GetData<UVaelSchuerfameiseData>();
	AngerTime = FMath::Max(0.0f, AngerTime - DeltaSeconds);
	BiteCooldown = FMath::Max(0.0f, BiteCooldown - DeltaSeconds);

	if (bEnslaved || AngerTime > 0.0f)
	{
		float Distance = 0.0f;
		if (AActor* Target = FindTarget(Data->AggroRange, &Distance))
		{
			const float Reach = Data->CollisionRadius * GetActorScale3D().X + Data->BiteReach;
			if (Distance > Reach)
			{
				MoveTowards(Target->GetActorLocation(), Data->MoveSpeed);
			}
			else
			{
				FaceTowards(Target->GetActorLocation());
				if (BiteCooldown <= 0.0f)
				{
					BiteCooldown = Data->BiteInterval;
					HitPlayer(Target, Data->BiteDamage, EVaelElement::Earth);
				}
			}
			return;
		}
	}

	// Free ant at work: walks between spots around its place, pauses, walks on
	if (WanderPause > 0.0f)
	{
		WanderPause -= DeltaSeconds;
		return;
	}
	if (FVector::Dist2D(GetActorLocation(), WanderGoal) < 60.0f)
	{
		WanderPause = FMath::FRandRange(1.0f, 3.5f);
		const FVector2D Offset = FMath::RandPointInCircle(Data->WanderRadius);
		WanderGoal = HomeLocation + FVector(Offset, 0.0f);
		return;
	}
	MoveTowards(WanderGoal, Data->MoveSpeed * 0.45f);
}

#undef LOCTEXT_NAMESPACE
