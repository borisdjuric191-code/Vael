// Copyright Epic Games, Inc. All Rights Reserved.

#include "Creatures/VaelSchuerfameise.h"

#include "AnimationRuntime.h"
#include "Combat/VaelArcShot.h"
#include "Combat/VaelCombatStatics.h"
#include "Creatures/VaelLegIKComponent.h"
#include "Components/PoseableMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "EngineUtils.h"
#include "Items/VaelMaterial.h"
#include "Items/VaelMaterialDrop.h"
#include "Magic/VaelDebrisBurst.h"
#include "Player/VaelCharacter.h"
#include "Items/VaelMaterialBag.h"
#include "Magic/VaelGameplayTags.h"
#include "Vael.h"

#define LOCTEXT_NAMESPACE "VaelSchuerfameise"

FLinearColor VaelOre::GetColor(FName Ore)
{
	static const TMap<FName, FLinearColor> Colors = {
		{TEXT("Rosteisen"), FLinearColor(0.95f, 0.50f, 0.15f)},
		{TEXT("Schwefelkies"), FLinearColor(0.90f, 0.88f, 0.30f)},
		{TEXT("Funkenerz"), FLinearColor(1.0f, 0.38f, 0.08f)},
		{TEXT("Gruenspan"), FLinearColor(0.10f, 0.75f, 0.65f)},
		{TEXT("Salzspat"), FLinearColor(1.0f, 0.78f, 0.84f)},
		{TEXT("Drahtsilber"), FLinearColor(0.75f, 0.90f, 1.0f)},
		{TEXT("Blutzinnober"), FLinearColor(0.90f, 0.12f, 0.30f)},
		{TEXT("Siechglas"), FLinearColor(0.70f, 1.0f, 0.20f)},
		{TEXT("Rippenspat"), FLinearColor(0.95f, 0.85f, 0.70f)},
		{TEXT("Kobaltbluete"), FLinearColor(1.0f, 0.30f, 0.70f)},
		{TEXT("Kernerz"), FLinearColor(0.75f, 0.25f, 0.95f)},
	};
	const FLinearColor* Color = Colors.Find(Ore);
	return Color != nullptr ? *Color : FLinearColor(0.9f, 0.85f, 0.7f);
}

FString VaelOre::GetMaterialPath(FName Ore)
{
	return FString::Printf(TEXT("/Game/Vael/Items/Materials/DA_Material_%s.DA_Material_%s"), *Ore.ToString(), *Ore.ToString());
}

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
	// Every ant keeps its own pace
	LifePace = FMath::FRandRange(0.85f, 1.2f);
	LifePhase = FMath::FRandRange(0.0f, 100.0f);
}

void AVaelSchuerfameise::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	PoseLife(DeltaSeconds);
}

void AVaelSchuerfameise::BiteLunge()
{
	Lunge = 0.0f;
	SnapMandibles();
	if (UVaelLegIKComponent* Legs = GetFactoryLegs())
	{
		Legs->AddBodyJolt(GetActorForwardVector() * 9.0f * GetActorScale3D().X - FVector(0.0f, 0.0f, 3.0f));
	}
}

bool AVaelSchuerfameise::PrepareLife()
{
	UPoseableMeshComponent* AntMesh = GetFactoryMesh();
	if (AntMesh == nullptr || AntMesh->GetSkinnedAsset() == nullptr || AntMesh->BoneSpaceTransforms.IsEmpty())
	{
		return false;
	}
	const FReferenceSkeleton& RefSkeleton = AntMesh->GetSkinnedAsset()->GetRefSkeleton();
	auto Find = [&RefSkeleton](const TCHAR* Name, const TCHAR* TipName, FLifeBone& Out)
	{
		Out.Index = RefSkeleton.FindBoneIndex(Name);
		if (Out.Index == INDEX_NONE)
		{
			return;
		}
		Out.RefLocal = RefSkeleton.GetRefBonePose()[Out.Index];
		const FTransform Comp = FAnimationRuntime::GetComponentSpaceTransformRefPose(RefSkeleton, Out.Index);
		const int32 Parent = RefSkeleton.GetParentIndex(Out.Index);
		Out.RefComp = Comp.GetRotation();
		Out.ParentRefComp = Parent != INDEX_NONE ? FAnimationRuntime::GetComponentSpaceTransformRefPose(RefSkeleton, Parent).GetRotation() : FQuat::Identity;
		// Direction of the bone: towards its child (feelers, mandibles: the tip), else along its own axis
		const int32 Tip = TipName != nullptr ? RefSkeleton.FindBoneIndex(TipName) : INDEX_NONE;
		const FVector TipAt = Tip != INDEX_NONE ? FAnimationRuntime::GetComponentSpaceTransformRefPose(RefSkeleton, Tip).GetLocation()
			: Comp.GetLocation() + Comp.GetRotation().GetAxisY() * 10.0f;
		Out.Dir = (TipAt - Comp.GetLocation()).GetSafeNormal();
		// Outwards: away from the middle line of the body (mesh X is the side axis)
		Out.Out = FVector(FMath::Sign(Comp.GetLocation().X + KINDA_SMALL_NUMBER), 0.0f, 0.0f);
	};
	Find(TEXT("head"), nullptr, LifeHead);
	Find(TEXT("abdomen"), nullptr, LifeAbdomen);
	if (LifeAbdomen.Index != INDEX_NONE)
	{
		// The abdomen points backwards: from the chest to the waist
		const int32 BodyIndex = RefSkeleton.FindBoneIndex(TEXT("body"));
		const FVector Waist = FAnimationRuntime::GetComponentSpaceTransformRefPose(RefSkeleton, LifeAbdomen.Index).GetLocation();
		const FVector Chest = BodyIndex != INDEX_NONE ? FAnimationRuntime::GetComponentSpaceTransformRefPose(RefSkeleton, BodyIndex).GetLocation() : Waist - FVector(0.0f, 1.0f, 0.0f);
		LifeAbdomen.Dir = (Waist - Chest).GetSafeNormal2D();
	}
	Find(TEXT("antenna_L1"), TEXT("antenna_L2"), LifeAntenna[0]);
	Find(TEXT("antenna_L2"), nullptr, LifeAntenna[1]);
	Find(TEXT("antenna_R1"), TEXT("antenna_R2"), LifeAntenna[2]);
	Find(TEXT("antenna_R2"), nullptr, LifeAntenna[3]);
	Find(TEXT("mandible_L"), nullptr, LifeMandible[0]);
	Find(TEXT("mandible_R"), nullptr, LifeMandible[1]);
	if (LifeHead.Index != INDEX_NONE && LifeMandible[0].Index != INDEX_NONE)
	{
		// The head looks forward: from its joint to the mandibles (rotating about Dir x Up raises it)
		const FVector Joint = FAnimationRuntime::GetComponentSpaceTransformRefPose(RefSkeleton, LifeHead.Index).GetLocation();
		const FVector Jaw = FAnimationRuntime::GetComponentSpaceTransformRefPose(RefSkeleton, LifeMandible[0].Index).GetLocation();
		LifeHead.Dir = (Jaw - Joint).GetSafeNormal2D();
	}
	return true;
}

void AVaelSchuerfameise::TurnLifeBone(const FLifeBone& Bone, const FQuat& MeshDelta)
{
	UPoseableMeshComponent* AntMesh = GetFactoryMesh();
	if (Bone.Index == INDEX_NONE || !AntMesh->BoneSpaceTransforms.IsValidIndex(Bone.Index))
	{
		return;
	}
	FTransform Local = Bone.RefLocal;
	Local.SetRotation(Bone.ParentRefComp.Inverse() * MeshDelta * Bone.RefComp);
	AntMesh->BoneSpaceTransforms[Bone.Index] = Local;
}

void AVaelSchuerfameise::PoseLife(float DeltaSeconds)
{
	if (IsDead())
	{
		return;
	}
	if (!bLifeReady)
	{
		bLifeReady = PrepareLife();
		if (!bLifeReady)
		{
			return;
		}
	}
	const bool bFight = IsFighting();
	const float Pace = LifePace * (bFight ? 1.8f : 1.0f);
	const float Reach = bFight ? 1.3f : 1.0f;
	LifeTime += DeltaSeconds * Pace;
	const float T = LifeTime + LifePhase;
	auto Wave = [](float X) { return FMath::Sin(X) * 0.6f + FMath::Sin(X * 2.31f + 1.7f) * 0.3f + FMath::Sin(X * 4.13f + 0.4f) * 0.1f; };

	// Body: a slow breath and a little rocking on the legs
	Lunge = FMath::Min(1.0f, Lunge + DeltaSeconds * 3.0f);
	const float LungeShow = FMath::Sin(FMath::Min(Lunge * 1.6f, 1.0f) * UE_PI);   // fast out, slower back
	if (UVaelLegIKComponent* Legs = GetFactoryLegs())
	{
		// Breath and rocking; the front dips with a bite and when crouched for a leap
		Legs->SetExtraBodyTilt(FRotator(2.6f * Reach * Wave(T * 1.5f) - 6.0f * LungeShow - 9.0f * Crouch, 0.0f, 1.7f * Reach * Wave(T * 1.05f + 2.0f)));
		if (Crouch > 0.001f || bCrouchShown)
		{
			Legs->SetCurl(0.35f * Crouch);
			bCrouchShown = Crouch > 0.001f;
		}
	}

	// Abdomen (Boris 27.11.: like bees and other insects): rises and sinks slowly, always pumps a little and now and then
	// in a burst, swings to the side; raised in threat while it fights
	if (LifeAbdomen.Index != INDEX_NONE)
	{
		PumpTimer -= DeltaSeconds * Pace;
		if (PumpTimer <= 0.0f)
		{
			PumpTimer = bFight ? FMath::FRandRange(0.8f, 2.0f) : FMath::FRandRange(2.0f, 5.0f);
			Pump = 0.0f;
		}
		Pump = FMath::Min(1.0f, Pump + DeltaSeconds * 0.9f);
		const float Burst = FMath::Sin(Pump * UE_PI);
		const float PumpDeg = (1.2f + 4.0f * Burst) * FMath::Sin(T * 10.0f);
		const float RiseDeg = 5.5f * Wave(T * 0.45f + 0.8f) + (bFight ? 9.0f : 0.0f) + PumpDeg;
		const float SwayDeg = 3.5f * Reach * Wave(T * 0.65f + 4.0f);
		const FVector Side = FVector::CrossProduct(LifeAbdomen.Dir, FVector::UpVector).GetSafeNormal();
		TurnLifeBone(LifeAbdomen, FQuat(FVector::UpVector, FMath::DegreesToRadians(SwayDeg)) * FQuat(Side, FMath::DegreesToRadians(RiseDeg)));
	}

	// Head: looks about, quicker and jerkier in a fight
	const FVector Up = FVector::UpVector;
	if (LifeHead.Index != INDEX_NONE)
	{
		const FVector Side = FVector::CrossProduct(LifeHead.Dir, Up).GetSafeNormal();
		const FQuat Look = FQuat(Up, FMath::DegreesToRadians(7.0f * Reach * Wave(T * 0.45f)))
			* FQuat(Side, FMath::DegreesToRadians(4.0f * Reach * Wave(T * 0.6f + 3.0f) - 16.0f * LungeShow));
		TurnLifeBone(LifeHead, Look);
	}

	// Feelers: both swing, a little apart; now and then one probes ahead and down
	for (int32 Side = 0; Side < 2; ++Side)
	{
		ProbeTimer[Side] -= DeltaSeconds * Pace;
		if (ProbeTimer[Side] <= 0.0f)
		{
			ProbeTimer[Side] = FMath::FRandRange(1.5f, 5.0f);
			Probe[Side] = 0.0f;
		}
		Probe[Side] = FMath::Min(1.0f, Probe[Side] + DeltaSeconds * 1.6f * Pace);
		const float Probing = FMath::Sin(Probe[Side] * UE_PI);
		const float Phase = Side * 1.9f;
		for (int32 Part = 0; Part < 2; ++Part)
		{
			const FLifeBone& Bone = LifeAntenna[Side * 2 + Part];
			if (Bone.Index == INDEX_NONE)
			{
				continue;
			}
			const FVector Lift = FVector::CrossProduct(Bone.Dir, Up).GetSafeNormal();
			const float Scale = Part == 0 ? 1.0f : 0.7f;
			const float LiftDeg = (9.0f * Wave(T * 1.3f + Phase + Part * 0.6f) - 22.0f * Probing) * Reach * Scale;
			const float SweepDeg = (8.0f * Wave(T * 0.9f + Phase * 1.3f + Part * 0.8f)) * Reach * Scale;
			TurnLifeBone(Bone, FQuat(Up, FMath::DegreesToRadians(SweepDeg)) * FQuat(Lift, FMath::DegreesToRadians(LiftDeg)));
		}
	}

	// Mandibles: tremble, and snap shut now and then (often in a fight, always with a bite)
	MandibleTimer -= DeltaSeconds * Pace;
	if (MandibleTimer <= 0.0f)
	{
		MandibleTimer = bFight ? FMath::FRandRange(0.5f, 1.8f) : FMath::FRandRange(1.5f, 5.0f);
		SnapMandibles();
	}
	MandibleSnap = FMath::Min(1.0f, MandibleSnap + DeltaSeconds * 2.6f);
	// 0..0.45 open wide, 0.45..0.6 slam shut, then back to rest
	const float Open = MandibleSnap < 0.45f ? FMath::InterpEaseOut(0.0f, 1.0f, MandibleSnap / 0.45f, 2.0f)
		: MandibleSnap < 0.6f ? FMath::Lerp(1.0f, -0.25f, (MandibleSnap - 0.45f) / 0.15f)
		: FMath::Lerp(-0.25f, 0.0f, (MandibleSnap - 0.6f) / 0.4f);
	for (int32 Side = 0; Side < 2; ++Side)
	{
		const FLifeBone& Bone = LifeMandible[Side];
		if (Bone.Index == INDEX_NONE)
		{
			continue;
		}
		// Rotating the bone about Dir x Out turns its tip outwards
		const FVector Axis = FVector::CrossProduct(Bone.Dir, Bone.Out).GetSafeNormal();
		const float Deg = 24.0f * Open + 2.0f * FMath::Sin(T * 9.0f + Side * 2.0f);
		TurnLifeBone(Bone, FQuat(Axis, FMath::DegreesToRadians(Deg)));
	}
	GetFactoryMesh()->MarkRefreshTransformDirty();
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

void AVaelSchuerfameise::PlaceCrystal(const FVaelAmeisenFassung& Fassung, const FVaelAmeisenKristall& Piece, float Age, float LengthScale, FRandomStream& Random, float Jitter,
	bool bBent, float Sink)
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
	const FVector Surface = Fassung.Surface.IsNearlyZero() ? Normal : BlenderToMesh(Fassung.Surface, 1.0f).GetSafeNormal();

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
	// The foot sits in the body (along the surface normal), so the crystal grows out of its place instead of resting on it
	Places.Add({Floor - Surface * Radius * Sink, Normal, ScaleXY, ScaleZ});
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
		const FQuat Base = FQuat::FindBetweenNormals(FVector::UpVector, Place.Axis);
		float SpinAngle = Random.FRandRange(0.0f, UE_TWO_PI);
		if (bBent)
		{
			// Bent pieces curl their tip to local -X: turn that side to the body, so the tip lies against the surface
			const FVector Curl = Base.RotateVector(-FVector::XAxisVector);
			const FVector Toward = (-Surface - Place.Axis * FVector::DotProduct(-Surface, Place.Axis)).GetSafeNormal();
			if (!Toward.IsNearlyZero())
			{
				SpinAngle = FMath::Atan2(FVector::DotProduct(Place.Axis, FVector::CrossProduct(Curl, Toward)), FVector::DotProduct(Curl, Toward))
					+ FMath::DegreesToRadians(Random.FRandRange(-12.0f, 12.0f) * Jitter);
			}
		}
		const FQuat Spin(Place.Axis, SpinAngle);
		FVector TiltAxis, Unused;
		Place.Axis.FindBestAxisVectors(TiltAxis, Unused);
		const FQuat Tilt(TiltAxis, FMath::DegreesToRadians(Random.FRandRange(-8.0f, 8.0f) * Jitter));
		const FQuat Rotation = Tilt * Spin * Base;
		AddCrystal(PieceMesh, Fassung.Bone, FTransform(Rotation, Place.Location, FVector(Place.XY, Place.XY, Place.Z)));
	}
}

bool AVaelSchuerfameise::BreakCrystal()
{
	TArray<UStaticMeshComponent*> Whole;
	for (UStaticMeshComponent* Crystal : Crystals)
	{
		if (Crystal != nullptr && Crystal->IsVisible() && !Crystal->GetAttachSocketName().ToString().StartsWith(TEXT("wing_")))
		{
			Whole.Add(Crystal);
		}
	}
	if (Whole.IsEmpty())
	{
		return false;
	}
	UStaticMeshComponent* Crystal = Whole[FMath::RandRange(0, Whole.Num() - 1)];
	const FBoxSphereBounds Bounds = Crystal->Bounds;
	UStaticMesh* PieceMesh = Crystal->GetStaticMesh();
	const float Length = 2.0f * Bounds.SphereRadius;
	Crystal->SetVisibility(false);

	// It crumbles: a few pieces of its own ore, a quarter to half its length, tumble off and lie still for a moment
	FVaelDebris Crumbs;
	Crumbs.Meshes = {PieceMesh};
	Crumbs.Count = 4;
	Crumbs.Spread = Length * 0.3f;
	Crumbs.Speed = 220.0f;
	Crumbs.Lift = 260.0f;
	Crumbs.Lifetime = 1.4f;
	Crumbs.MinSize = Length * 0.25f;
	Crumbs.MaxSize = Length * 0.45f;
	AVaelDebrisBurst::Spawn(this, Bounds.Origin, Crumbs);

	// Sometimes the crystal itself falls whole: it lies where it fell, every player can pick up their own
	if (FMath::FRand() < GetData<UVaelSchuerfameiseData>()->BreakLootChance)
	{
		DropOrePieces(Ore, Bounds.Origin, 1, PieceMesh, FMath::Clamp(Length, 8.0f, 22.0f));
	}
	return true;
}

void AVaelSchuerfameise::ThrowShards(const FVector& From, int32 Count, float Size)
{
	// Pieces of its ore burst from a place, from half to the full size
	FVaelDebris Burst;
	for (const FVaelAmeisenKristall& Kristall : GetData<UVaelSchuerfameiseData>()->Kristalle)
	{
		if (Kristall.Ore == Ore)
		{
			if (UStaticMesh* PieceMesh = Kristall.Mesh.LoadSynchronous())
			{
				Burst.Meshes.Add(PieceMesh);
			}
		}
	}
	Burst.Count = Count;
	Burst.Spread = Size;
	Burst.Speed = 380.0f;
	Burst.Lift = 420.0f;
	Burst.Lifetime = 1.6f;
	Burst.MinSize = Size * 0.5f;
	Burst.MaxSize = Size;
	AVaelDebrisBurst::Spawn(this, From, Burst);
}

void AVaelSchuerfameise::DropOrePieces(FName InOre, const FVector& Where, int32 Count, UStaticMesh* Model, float Size)
{
	const UVaelMaterial* OreMaterial = LoadObject<UVaelMaterial>(nullptr, *VaelOre::GetMaterialPath(InOre));
	if (OreMaterial == nullptr || Count <= 0)
	{
		return;
	}
	// One piece per player in a small ring where it fell
	int32 PlayerIndex = 0;
	const float Turn = FMath::FRandRange(0.0f, UE_TWO_PI);
	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		AVaelCharacter* Player = It->Get() != nullptr ? It->Get()->GetPawn<AVaelCharacter>() : nullptr;
		if (Player == nullptr)
		{
			continue;
		}
		const float Angle = Turn + PlayerIndex++ * UE_TWO_PI / 4.0f;
		const FVector Spot = FVector(Where.X, Where.Y, Where.Z) + FVector(FMath::Cos(Angle), FMath::Sin(Angle), 0.0f) * FMath::FRandRange(50.0f, 100.0f);
		AVaelMaterialDrop::DropMaterialAs(GetWorld(), Spot, OreMaterial, Count, Player, Model, Size);
	}
}

void AVaelSchuerfameise::Anger()
{
	AngerTime = GetData<UVaelSchuerfameiseData>()->AngerSeconds;
}

void AVaelSchuerfameise::OnDamageTaken(float Damage, const FGameplayTagContainer& DamageTags)
{
	Super::OnDamageTaken(Damage, DamageTags);

	// Crystals break out of the shell under hits; earth (and anything on a frozen shell) shatters them
	const UVaelSchuerfameiseData* Data = GetData<UVaelSchuerfameiseData>();
	const bool bShatter = DamageTags.HasTagExact(VaelTags::Element_Earth) || UVaelCombatStatics::HasStatus(this, EVaelStatus::Frozen);
	if (Damage > 0.0f && FMath::FRand() < (bShatter ? Data->EarthBreakChance : Data->BreakChance))
	{
		BreakCrystal();
	}

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
					BiteLunge();
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
