// Copyright Epic Games, Inc. All Rights Reserved.

#include "World/VaelFireComponent.h"
#include "Components/DecalComponent.h"
#include "Components/HeterogeneousVolumeComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "Vael.h"
#include "Components/PointLightComponent.h"
#include "Engine/World.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "VaelAssets.h"

int32 UVaelFireComponent::NumLitFires = 0;
int32 UVaelFireComponent::NumVolumeFires = 0;

namespace
{
	/** Material parameters of the fire kit */
	const FName StrengthParameter(TEXT("Staerke"));
	const FName HeatParameter(TEXT("Hitze"));
	const FName SmokeParameter(TEXT("Dunkel"));
	const FName SpeedParameter(TEXT("Tempo"));
	const FName SparkHeightParameter(TEXT("Hoehe"));
	const FName EmberParameter(TEXT("Glut"));
	const FName CoverParameter(TEXT("Deckkraft"));
	const FName FlowParameter(TEXT("Richtung"));
	const FName SizeParameter(TEXT("Groesse"));
	const FName NormalFlowParameter(TEXT("NormalFluss"));
	const FName DensityParameter(TEXT("Dichte"));
	const FName GrowthParameter(TEXT("Wachstum"));
	const FName FrontParameter(TEXT("Front"));

	/** Stage number of the campfire volume, apart from the stages of a Brandherd */
	constexpr int32 CampfireStage = 100;

	/** Size of a spark card in cm */
	constexpr float SparkSize = 1.8f;

	const FString FireFolder = TEXT("/Game/Vael/Effects/Feuer");

	template<typename T>
	TSoftObjectPtr<T> FireAsset(const TCHAR* Name)
	{
		return TSoftObjectPtr<T>(FSoftObjectPath(FString::Printf(TEXT("%s/%s.%s"), *FireFolder, Name, Name)));
	}

	TSoftObjectPtr<UMaterialInterface> VolumeAsset(const TCHAR* Name)
	{
		return TSoftObjectPtr<UMaterialInterface>(FSoftObjectPath(FString::Printf(TEXT("%s/Volumen/%s.%s"), *FireFolder, Name, Name)));
	}

	/** The cards of one fire: all four corners of a card sit on its base point; the material unfolds it towards the camera
	 *  from UV channel 0 (u across, v from the base up) and channel 1 (width, height) */
	struct FCards
	{
		TArray<FVector> Vertices;
		TArray<int32> Triangles;
		TArray<FVector2D> UVs;
		TArray<FVector2D> Sizes;
		TArray<FLinearColor> Colors;
		TArray<FVector> Normals;

		/** The normal is the way the flame licks when the material follows the normals (a ball of fire) */
		void Add(const FVector& Base, float Width, float Height, const FLinearColor& Color, const FVector& Normal)
		{
			const int32 First = Vertices.Num();
			Normals.Append({ Normal, Normal, Normal, Normal });
			Vertices.Append({ Base, Base, Base, Base });
			UVs.Append({ FVector2D(0.0f, 0.0f), FVector2D(1.0f, 0.0f), FVector2D(1.0f, 1.0f), FVector2D(0.0f, 1.0f) });
			Sizes.Append({ FVector2D(Width, Height), FVector2D(Width, Height), FVector2D(Width, Height), FVector2D(Width, Height) });
			Colors.Append({ Color, Color, Color, Color });
			Triangles.Append({ First, First + 2, First + 1, First, First + 3, First + 2 });
		}
	};
}

UVaelFireSettings::UVaelFireSettings()
{
	FlameMaterial = FireAsset<UMaterialInterface>(TEXT("M_Feuer"));
	SparkMaterial = FireAsset<UMaterialInterface>(TEXT("M_Feuer_Funken"));
	ScorchMaterial = FireAsset<UMaterialInterface>(TEXT("M_Feuer_Brandfleck"));
	SmokeMaterial = FireAsset<UMaterialInterface>(TEXT("M_Feuer_Rauch"));
	FlameFlipbook = FireAsset<UMaterialInterface>(TEXT("MI_Feuer_Flamme"));
	FireFlipbook = FireAsset<UMaterialInterface>(TEXT("MI_Feuer_Feuer"));
	BallFlipbook = FireAsset<UMaterialInterface>(TEXT("MI_Feuer_Kugel"));
	RootMaterial = FireAsset<UMaterialInterface>(TEXT("M_Feuer_Brandschatten"));
	TongueMaterial = TSoftObjectPtr<UMaterialInterface>(FSoftObjectPath(TEXT("/Game/Vael/Effects/Feuer/Zunge/M_Feuer_Zunge.M_Feuer_Zunge")));
	for (const TCHAR* Name : { TEXT("SM_Feuer_Zunge_0"), TEXT("SM_Feuer_Zunge_1"), TEXT("SM_Feuer_Zunge_2") })
	{
		TongueMeshes.Add(TSoftObjectPtr<UStaticMesh>(FSoftObjectPath(FString::Printf(TEXT("/Game/Vael/Effects/Feuer/Zunge/%s.%s"), Name, Name))));
	}
	LiveFireSystem = TSoftObjectPtr<UNiagaraSystem>(FSoftObjectPath(TEXT("/Game/Vael/Effects/Feuer/Live/NS_Feuer_Live.NS_Feuer_Live")));

	// Embers, a fire, a big fire: each simulated on a bed of its own size
	GroundVolumes = {
		{ VolumeAsset(TEXT("MI_Feuer_Volumen_Glut")), 0.35f, 45.0f },
		{ VolumeAsset(TEXT("MI_Feuer_Volumen_Brand")), 0.6f, 140.0f },
		{ VolumeAsset(TEXT("MI_Feuer_Volumen_Gross")), 1.1f, 100000.0f } };
	CampfireVolume = { VolumeAsset(TEXT("MI_Feuer_Volumen_Lager")), 0.7f, 100000.0f };
	BallVolume = { VolumeAsset(TEXT("MI_Feuer_Volumen_Kugel")), 0.25f, 100000.0f };
}

UVaelFireComponent::UVaelFireComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryComponentTick.bCanEverTick = true;
	SetCollisionEnabled(ECollisionEnabled::NoCollision);
	SetCanEverAffectNavigation(false);
	SetCastShadow(false);
	bUseAsyncCooking = false;

	// The flames rise straight up whatever they hang on, and the sparks fly above the cards
	SetUsingAbsoluteRotation(true);
	SetUsingAbsoluteScale(true);
	BoundsScale = 1.0f;
}

void UVaelFireComponent::Ignite(const FVaelFireShape& InShape, float FadeIn)
{
	Shape = InShape;
	TargetStrength = 1.0f;
	FadeSpeed = 1.0f / FMath::Max(FadeIn, 0.01f);
	OutTime = -1.0f;
	FlickerPhase = FMath::FRandRange(0.0f, 100.0f);
	SetWorldRotation(FRotator::ZeroRotator);

	const UVaelFireSettings* Settings = UVaelFireSettings::Get();
	if (FlameMaterial == nullptr)
	{
		// Simulated fire where its flipbook exists, else the drawn flames
		UMaterialInterface* Flipbook = VaelAssets::LoadOptional(InShape.bBall ? Settings->BallFlipbook : Settings->FlameFlipbook);
		FlameMaterial = UMaterialInstanceDynamic::Create(Flipbook != nullptr ? Flipbook : VaelAssets::LoadOptional(Settings->FlameMaterial), this);
		if (UMaterialInterface* BedBase = VaelAssets::LoadOptional(Settings->FireFlipbook))
		{
			BedMaterial = UMaterialInstanceDynamic::Create(BedBase, this);
		}
		SparkMaterial = UMaterialInstanceDynamic::Create(VaelAssets::LoadOptional(Settings->SparkMaterial), this);
		if (UMaterialInterface* SmokeBase = VaelAssets::LoadOptional(Settings->SmokeMaterial))
		{
			SmokeMaterial = UMaterialInstanceDynamic::Create(SmokeBase, this);
		}
	}
	FlameMaterial->SetScalarParameterValue(HeatParameter, Shape.Heat);
	FlameMaterial->SetScalarParameterValue(SmokeParameter, Shape.Smoke);
	FlameMaterial->SetScalarParameterValue(SpeedParameter, Shape.Speed);
	SparkMaterial->SetScalarParameterValue(SparkHeightParameter, Shape.Height * 1.6f);
	const bool bBallFlipbook = Shape.bBall && FlameMaterial->Parent == VaelAssets::LoadOptional(Settings->BallFlipbook);
	FlameMaterial->SetScalarParameterValue(NormalFlowParameter, Shape.bBall && !bBallFlipbook ? Shape.BallSpread : 0.0f);
	if (SmokeMaterial != nullptr)
	{
		SmokeMaterial->SetScalarParameterValue(SmokeParameter, Shape.Smoke);
		SmokeMaterial->SetScalarParameterValue(SpeedParameter, Shape.Speed);
	}
	BuildCards();
	BuildVolume();
	if (RootGrowth <= 0.0f)
	{
		RootGrowth = 0.12f;
	}
	BuildTongues();

	// A charred patch under the fire, or on the wall it burns on. On the ground the fire spreads in burning roots that
	// creep outwards from where it started
	if (Shape.bScorch && Scorch == nullptr)
	{
		const bool bWall = !Shape.WallNormal.IsNearlyZero();
		UMaterialInterface* RootBase = bWall ? nullptr : VaelAssets::LoadOptional(Settings->RootMaterial);
		if (UMaterialInterface* ScorchBase = RootBase != nullptr ? RootBase : VaelAssets::LoadOptional(Settings->ScorchMaterial))
		{
			ScorchMaterial = UMaterialInstanceDynamic::Create(ScorchBase, this);
			Scorch = NewObject<UDecalComponent>(GetOwner());
			Scorch->SetDecalMaterial(ScorchMaterial);
			Scorch->SetupAttachment(this);
			Scorch->SetUsingAbsoluteRotation(true);
			const float Size = Shape.Radius * (RootBase != nullptr ? Settings->RootReach : 1.35f);
			Scorch->DecalSize = FVector(40.0f, Size, bWall ? Size * 1.3f : Size);
			Scorch->RegisterComponent();
			Scorch->SetWorldRotation(bWall ? (-Shape.WallNormal).Rotation() : FRotator(-90.0f, FMath::FRandRange(0.0f, 360.0f), 0.0f));
			Scorch->SetRelativeLocation(bWall ? FVector(0.0f, 0.0f, Shape.Radius * 0.3f) : FVector::ZeroVector);
			RootGrowth = 0.1f;
			SinceFed = 0.0f;
		}
	}

	if (Shape.bLight)
	{
		TakeLight();
	}
	SetComponentTickEnabled(true);
}

const FVaelFireVolumeKind* UVaelFireComponent::PickVolumeKind(int32& OutIndex) const
{
	const UVaelFireSettings* Settings = UVaelFireSettings::Get();
	OutIndex = INDEX_NONE;
	// The trail of a projectile has no volume: a simulated fire cannot be dragged along that fast
	if (!Settings->bUseVolumes || (Shape.bFloating && !Shape.bBall)
		|| Shape.Radius < (Shape.bBall ? Settings->MinBallVolumeRadius : Settings->MinVolumeRadius))
	{
		return nullptr;
	}
	if (Shape.bBall)
	{
		return &Settings->BallVolume;
	}
	if (Shape.bCampfire)
	{
		OutIndex = CampfireStage;
		return &Settings->CampfireVolume;
	}
	for (int32 Index = 0; Index < Settings->GroundVolumes.Num(); ++Index)
	{
		if (Shape.Radius <= Settings->GroundVolumes[Index].UpToRadius || Index == Settings->GroundVolumes.Num() - 1)
		{
			OutIndex = Index;
			return &Settings->GroundVolumes[Index];
		}
	}
	return nullptr;
}

UHeterogeneousVolumeComponent* UVaelFireComponent::MakeVolume(const FVaelFireVolumeKind& Kind)
{
	UMaterialInterface* Base = VaelAssets::LoadOptional(Kind.Material);
	if (Base == nullptr)
	{
		return nullptr;
	}
	UHeterogeneousVolumeComponent* Made = NewObject<UHeterogeneousVolumeComponent>(GetOwner());
	Made->SetupAttachment(this);
	Made->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Made->SetCastShadow(false);
	Made->bPlaying = true;
	Made->bLooping = true;
	Made->bPivotAtCentroid = true;
	Made->FrameRate = UVaelFireSettings::Get()->VolumeFrameRate;
	Made->RegisterComponent();
	Made->SetMaterial(0, Base);
	if (Made->MaterialInstanceDynamic == nullptr)
	{
		Made->DestroyComponent();
		return nullptr;
	}
	// Every fire starts somewhere else in the loop and turns another way, so no two burn alike
	Made->Frame = FMath::FRandRange(Made->StartFrame, Made->EndFrame);
	Made->SetRelativeRotation(FRotator(0.0f, FMath::FRandRange(0.0f, 360.0f), 0.0f));
	Made->MaterialInstanceDynamic->SetScalarParameterValue(StrengthParameter, 0.0f);
	++NumVolumeFires;
	return Made;
}

bool UVaelFireComponent::BuildLiveFire()
{
	const UVaelFireSettings* Settings = UVaelFireSettings::Get();
	if (LiveFire == nullptr)
	{
		UNiagaraSystem* System = VaelAssets::LoadOptional(Settings->LiveFireSystem);
		if (System == nullptr)
		{
			return false;
		}
		LiveFire = UNiagaraFunctionLibrary::SpawnSystemAttached(System, this, NAME_None, FVector::ZeroVector, FRotator::ZeroRotator,
			EAttachLocation::KeepRelativeOffset, false, false);
		if (LiveFire == nullptr)
		{
			return false;
		}
		// Which knobs the simulation offers, to tune it from here
		TArray<FNiagaraVariable> Knobs;
		LiveFire->GetOverrideParameters().GetUserParameters(Knobs);
		for (const FNiagaraVariable& Knob : Knobs)
		{
			UE_LOG(LogVael, Display, TEXT("Live fire parameter %s (%s)"), *Knob.GetName().ToString(), *Knob.GetType().GetName());
		}

		// The simulated space is sized to the fire it is lit with; the solver works on a grid of this many cells along its
		// longest side
		LiveFire->SetVariableBool(TEXT("DrawBounds"), false);
		LiveFire->SetVariableInt(TEXT("ResolutionMaxAxis"), Settings->LiveResolution);
		LiveFire->SetVariableVec3(TEXT("WorldSpaceSize"), FVector(Shape.Radius * Settings->LiveSizePerRadius, Shape.Radius * Settings->LiveSizePerRadius,
			Shape.Radius * Settings->LiveSizePerRadius * 1.3f));
		LiveFire->Activate(true);
	}

	// The simulation is the flame: the cards keep only the sparks
	SetMeshSectionVisible(0, false);
	SetMeshSectionVisible(2, false);
	SetMeshSectionVisible(3, false);
	return true;
}

void UVaelFireComponent::BuildVolume()
{
	if (Shape.bLive && BuildLiveFire())
	{
		return;
	}
	int32 Stage = INDEX_NONE;
	const FVaelFireVolumeKind* Kind = PickVolumeKind(Stage);
	if (Kind == nullptr)
	{
		return;
	}

	const bool bNewStage = Volume == nullptr || Stage != VolumeStage;
	if (bNewStage && (Volume != nullptr || NumVolumeFires < UVaelFireSettings::Get()->MaxVolumeFires))
	{
		if (UHeterogeneousVolumeComponent* Made = MakeVolume(*Kind))
		{
			// The stage the fire grew out of fades while the new one comes up; a stage still fading goes at once
			if (OldVolume != nullptr)
			{
				OldVolume->DestroyComponent();
				NumVolumeFires = FMath::Max(0, NumVolumeFires - 1);
			}
			OldVolume = Volume;
			OldVolumeMaterial = VolumeMaterial;
			OldVolumeSize = VolumeSize;
			OldVolumeHeight = VolumeHeight;
			VolumeBlend = OldVolume != nullptr ? 0.0f : 1.0f;
			Volume = Made;
			VolumeMaterial = Made->MaterialInstanceDynamic;
			VolumeStage = Stage;
			VolumeHeight = 0.0f;
		}
	}
	if (Volume == nullptr)
	{
		return;
	}

	// The volume plays in simulated metres; its frames carry the size of a voxel and the place of the domain
	VolumeBedRadius = Kind->BedRadius;
	VolumeScale = Shape.Radius / FMath::Max(VolumeBedRadius, 0.01f);
	VolumeMaterial->SetScalarParameterValue(DensityParameter, UVaelFireSettings::Get()->VolumeReferenceScale / FMath::Max(VolumeScale, 1.0f));
	SetSizeScale(SizeScale);

	// The volume is the flame now: the cards keep only the sparks, the volume brings its own smoke
	SetMeshSectionVisible(0, false);
	SetMeshSectionVisible(2, false);
	SetMeshSectionVisible(3, false);
}

void UVaelFireComponent::ReleaseVolume()
{
	for (UHeterogeneousVolumeComponent* Which : { Volume.Get(), OldVolume.Get() })
	{
		if (Which != nullptr)
		{
			Which->DestroyComponent();
			NumVolumeFires = FMath::Max(0, NumVolumeFires - 1);
		}
	}
	Volume = nullptr;
	VolumeMaterial = nullptr;
	OldVolume = nullptr;
	OldVolumeMaterial = nullptr;
	VolumeStage = INDEX_NONE;
}

void UVaelFireComponent::SetRadius(float NewRadius)
{
	if (FMath::IsNearlyEqual(NewRadius, Shape.Radius, 0.5f))
	{
		return;
	}
	const float Grown = NewRadius / FMath::Max(Shape.Radius, 1.0f);
	Shape.Radius = NewRadius;
	Shape.Height *= Grown;
	int32 Stage = INDEX_NONE;
	if (Volume != nullptr || PickVolumeKind(Stage) != nullptr)
	{
		BuildVolume();
	}
	if (Volume == nullptr)
	{
		BuildCards();
	}
	if (Scorch != nullptr)
	{
		const bool bWall = !Shape.WallNormal.IsNearlyZero();
		const float Size = Shape.Radius * (ScorchMaterial != nullptr && ScorchMaterial->Parent == VaelAssets::LoadOptional(UVaelFireSettings::Get()->RootMaterial)
			? UVaelFireSettings::Get()->RootReach : 1.35f);
		Scorch->DecalSize = FVector(40.0f, Size, bWall ? Size * 1.3f : Size);
		Scorch->MarkRenderStateDirty();
	}
	if (Light != nullptr)
	{
		Light->SetAttenuationRadius(300.0f + Shape.Radius * 7.0f);
		Light->SetRelativeLocation(FVector(0.0f, 0.0f, Shape.Height * 0.45f) + Shape.WallNormal * 25.0f);
	}
	SparkMaterial->SetScalarParameterValue(SparkHeightParameter, Shape.Height * 1.6f);
	UpdateBounds();
	UpdateTongues();
}

void UVaelFireComponent::Feed()
{
	SinceFed = 0.0f;
}

void UVaelFireComponent::BuildTongues()
{
	const UVaelFireSettings* Settings = UVaelFireSettings::Get();
	// The trail of a projectile has no tongues of its own
	if (!Settings->bUseTongues || (Shape.bFloating && !Shape.bBall))
	{
		return;
	}
	if (Tongues.IsEmpty())
	{
		UMaterialInterface* Base = VaelAssets::LoadOptional(Settings->TongueMaterial);
		if (Base == nullptr)
		{
			return;
		}
		TongueMaterial = UMaterialInstanceDynamic::Create(Base, this);
		for (const TSoftObjectPtr<UStaticMesh>& MeshAsset : Settings->TongueMeshes)
		{
			if (UStaticMesh* Mesh = VaelAssets::LoadOptional(MeshAsset))
			{
				UInstancedStaticMeshComponent* Instances = NewObject<UInstancedStaticMeshComponent>(GetOwner());
				Instances->SetupAttachment(this);
				Instances->SetStaticMesh(Mesh);
				Instances->SetMaterial(0, TongueMaterial);
				Instances->SetCollisionEnabled(ECollisionEnabled::NoCollision);
				Instances->SetCastShadow(false);
				Instances->SetCanEverAffectNavigation(false);
				Instances->NumCustomDataFloats = 3;
				Instances->RegisterComponent();
				Tongues.Add(Instances);
			}
		}
		if (Tongues.IsEmpty())
		{
			return;
		}
	}

	// Feet in units of the radius and heights in units of the height of the shape, so the fire can grow without moving them
	TongueSpots.Reset();
	FRandomStream Random(FMath::Rand());
	auto Add = [this, &Random](const FVector& Foot, const FVector& Up, float Height, float Along)
	{
		FTongue Tongue;
		Tongue.Foot = Foot;
		Tongue.Up = Up.GetSafeNormal();
		Tongue.Height = Height;
		Tongue.Yaw = Random.FRandRange(0.0f, 360.0f);
		Tongue.Mesh = Random.RandRange(0, Tongues.Num() - 1);
		Tongue.Along = Along;
		Tongue.Life = Random.FRandRange(UVaelFireSettings::Get()->TongueLifeMin, UVaelFireSettings::Get()->TongueLifeMax);
		// Every tongue at another moment of its life, so they never rise and fall together
		Tongue.Age = Random.FRandRange(0.0f, Tongue.Life);
		TongueSpots.Add(Tongue);
	};

	const bool bWall = !Shape.WallNormal.IsNearlyZero();
	if (Shape.bBall)
	{
		// All around the ball, licking outwards and a little upwards
		const int32 Count = FMath::Clamp(FMath::RoundToInt32(Shape.Radius / 3.0f), 8, 20);
		for (int32 Index = 0; Index < Count; ++Index)
		{
			const FVector Out = Random.GetUnitVector();
			Add(Out * 0.55f, Out + FVector(0.0f, 0.0f, 0.5f), Random.FRandRange(0.5f, 0.8f), 0.0f);
		}
	}
	else if (bWall)
	{
		// A row in front of the burn mark, rising straight up, tallest in the middle
		const FVector Out = FVector(Shape.WallNormal.X, Shape.WallNormal.Y, 0.0f).GetSafeNormal();
		const FVector Along = FVector::CrossProduct(FVector::UpVector, Out).GetSafeNormal();
		const int32 Count = FMath::Clamp(FMath::RoundToInt32(Shape.Radius / 18.0f), 3, 9);
		for (int32 Index = 0; Index < Count; ++Index)
		{
			const float Offset = Count > 1 ? Index / float(Count - 1) * 2.0f - 1.0f : 0.0f;
			Add(Along * Offset * 0.8f + Out * Random.FRandRange(0.15f, 0.35f), FVector::UpVector, (1.0f - 0.45f * FMath::Abs(Offset)) * Random.FRandRange(0.75f, 1.0f), 0.0f);
		}
	}
	else if (!Shape.bScorch)
	{
		// A campfire, or the fire on a burning creature: a ring of tongues around a tall one
		Add(FVector::ZeroVector, FVector::UpVector, 0.9f, 0.0f);
		for (int32 Index = 0; Index < 3; ++Index)
		{
			const float Angle = Index * UE_TWO_PI / 3.0f + Random.FRandRange(-0.4f, 0.4f);
			Add(FVector(FMath::Cos(Angle), FMath::Sin(Angle), 0.0f) * 0.18f, FVector::UpVector, Random.FRandRange(0.65f, 0.8f), 0.0f);
		}
		for (int32 Index = 0; Index < 6; ++Index)
		{
			const float Angle = Index * UE_TWO_PI / 6.0f + Random.FRandRange(-0.3f, 0.3f);
			Add(FVector(FMath::Cos(Angle), FMath::Sin(Angle), 0.0f) * Random.FRandRange(0.35f, 0.5f), FVector(FMath::Cos(Angle), FMath::Sin(Angle), 0.0f) * 0.15f + FVector::UpVector,
				Random.FRandRange(0.4f, 0.6f), 0.0f);
		}
	}
	else
	{
		// A Brandherd: an invisible star of uneven arms creeps outwards; the tongues stand on its arms, tall at the middle,
		// smaller towards the tips, and come up as the fire reaches them
		Add(FVector::ZeroVector, FVector::UpVector, 0.9f, 0.0f);
		Add(FVector(0.12f, 0.05f, 0.0f), FVector::UpVector, 0.75f, 0.0f);
		const float Spacing = FMath::Max(Settings->TongueSpacing, Settings->MinTongueSpacing / FMath::Max(Shape.Radius, 1.0f));
		for (int32 Arm = 0; Arm < Settings->StarArms; ++Arm)
		{
			float Angle = Arm * UE_TWO_PI / Settings->StarArms + Random.FRandRange(-0.35f, 0.35f);
			const float Length = Random.FRandRange(0.6f, 1.0f);
			FVector Position = FVector::ZeroVector;
			for (float Way = Spacing; Way <= Length; Way += Spacing)
			{
				Angle += Random.FRandRange(-0.3f, 0.3f);
				Position += FVector(FMath::Cos(Angle), FMath::Sin(Angle), 0.0f) * Spacing;
				const float Out = Way / Length;
				Add(Position, FVector::UpVector, (1.0f - 0.6f * Out) * Random.FRandRange(0.7f, 1.0f), Way);
			}
		}
	}
	for (UInstancedStaticMeshComponent* Instances : Tongues)
	{
		Instances->ClearInstances();
	}
	for (const FTongue& Tongue : TongueSpots)
	{
		Tongues[Tongue.Mesh]->AddInstance(FTransform::Identity);
	}
	TonguesGrowth = -1.0f;
	UpdateTongues();
}

void UVaelFireComponent::UpdateTongues(float DeltaTime)
{
	if (Tongues.IsEmpty())
	{
		return;
	}
	const UVaelFireSettings* Settings = UVaelFireSettings::Get();
	TArray<TArray<FTransform>> Placed;
	TArray<TArray<float>> Data;
	Placed.SetNum(Tongues.Num());
	Data.SetNum(Tongues.Num());
	for (FTongue& Tongue : TongueSpots)
	{
		// One life after another: each new tongue grows a little apart from the last, turned another way, of its own size
		Tongue.Age += DeltaTime;
		if (Tongue.Age >= Tongue.Life)
		{
			Tongue.Age -= Tongue.Life;
			Tongue.Life = FMath::FRandRange(Settings->TongueLifeMin, Settings->TongueLifeMax);
			Tongue.Yaw = FMath::FRandRange(0.0f, 360.0f);
			Tongue.Size = FMath::FRandRange(0.8f, 1.15f);
			Tongue.Shift = FVector(FMath::FRandRange(-0.06f, 0.06f), FMath::FRandRange(-0.06f, 0.06f), 0.0f);
		}
		const float Life = Tongue.Age / Tongue.Life;
		// It shoots up out of the ground, sways at full size, then stretches, thins and lifts off as its tip tears away
		const float Grown = FMath::SmoothStep(0.0f, 0.22f, Life);
		const float Leaving = FMath::SmoothStep(0.62f, 1.0f, Life);
		const float Fade = 1.0f - FMath::SmoothStep(0.8f, 1.0f, Life);

		// A tongue on an arm of the star comes up once the creeping fire has reached it
		const float Shown = Tongue.Along <= 0.0f ? 1.0f : FMath::Clamp((RootGrowth - Tongue.Along) * 6.0f, 0.0f, 1.0f);
		const float Height = FMath::Max(Tongue.Height * Tongue.Size * Shape.Height * Shown * SizeScale * Grown * (1.0f + 0.35f * Leaving), 0.1f);
		const float Scale = Height / 100.0f;
		const float Thin = Settings->TongueWidth * (0.75f + 0.25f * Grown) * (1.0f - 0.45f * Leaving);
		const FQuat Turn = FRotationMatrix::MakeFromZ(Tongue.Up).ToQuat() * FQuat(FVector::UpVector, FMath::DegreesToRadians(Tongue.Yaw));
		// The foot sits a little in the ground, so the round bottom does not show; a leaving tongue rises off the ground
		const FVector Foot = (Tongue.Foot + Tongue.Shift) * Shape.Radius * SizeScale + Tongue.Up * Height * (0.45f * Leaving - 0.08f);
		Placed[Tongue.Mesh].Add(FTransform(Turn, Foot, FVector(Scale * Thin, Scale * Thin, Scale)));
		// For the material: how much it shows, how far through its life, how big it is (for its sway in cm)
		Data[Tongue.Mesh].Append({ Fade * Shown, Life, Scale });
	}
	for (int32 Index = 0; Index < Tongues.Num(); ++Index)
	{
		if (!Placed[Index].IsEmpty())
		{
			Tongues[Index]->BatchUpdateInstancesTransforms(0, Placed[Index], false, false);
			for (int32 Instance = 0; Instance < Placed[Index].Num(); ++Instance)
			{
				Tongues[Index]->SetCustomData(Instance, TArrayView<const float>(Data[Index].GetData() + Instance * 3, 3), Instance == Placed[Index].Num() - 1);
			}
		}
	}
	TonguesGrowth = RootGrowth;
}

void UVaelFireComponent::Extinguish(float FadeOut, bool bInDestroyWhenOut)
{
	TargetStrength = 0.0f;
	FadeSpeed = 1.0f / FMath::Max(FadeOut, 0.01f);
	bDestroyWhenOut = bInDestroyWhenOut;
}

void UVaelFireComponent::BuildCards()
{
	const UVaelFireSettings* Settings = UVaelFireSettings::Get();
	const bool bWall = !Shape.WallNormal.IsNearlyZero();
	const FVector Out = bWall ? FVector(Shape.WallNormal.X, Shape.WallNormal.Y, 0.0f).GetSafeNormal() : FVector::ZeroVector;
	const FVector Along = bWall ? FVector::CrossProduct(FVector::UpVector, Out).GetSafeNormal() : FVector::ForwardVector;

	// More tongues for a bigger fire, the tallest in the middle
	const float Area = bWall ? Shape.Radius * 2.0f * Shape.Radius * 1.6f : UE_PI * FMath::Square(Shape.Radius);
	const int32 NumTongues = Shape.Tongues > 0 ? Shape.Tongues : FMath::Clamp(FMath::RoundToInt32(Area / 10000.0f * Settings->TonguesPerSquareMetre), Settings->MinTongues, Settings->MaxTongues);

	FCards Flames;
	TArray<FVector> SparkBases;
	FRandomStream Random(FMath::Rand());

	// Places a point on the burning patch: along the wall, on the disc, or on the ball; returns how far out from the middle (0 to 1)
	const auto PlaceOnPatch = [&](float Spread, FVector& OutBase, FVector& OutNormal)
	{
		OutNormal = FVector::UpVector;
		if (Shape.bBall)
		{
			OutNormal = Random.GetUnitVector();
			OutBase = OutNormal * Shape.Radius * Random.FRandRange(0.4f, 0.8f);
			return 0.3f;
		}
		if (bWall)
		{
			const float Offset = Random.FRandRange(-1.0f, 1.0f) * Spread;
			OutBase = Along * Offset * Shape.Radius + Out * Random.FRandRange(4.0f, 10.0f) + FVector(0.0f, 0.0f, Random.FRandRange(-0.25f, 0.15f) * Shape.Radius);
			return FMath::Abs(Offset);
		}
		const float Share = FMath::Sqrt(Random.FRand()) * Spread;
		const float Angle = Random.FRandRange(0.0f, 2.0f * UE_PI);
		OutBase = FVector(FMath::Cos(Angle), FMath::Sin(Angle), 0.0f) * Share * Shape.Radius;
		return Share;
	};

	// A burning ball: the flipbook shows the ball with its flames, so a few large cards on its middle are enough.
	// The simulated ball is 28 % of the frame high and sits at 35 % of its height.
	const bool bBallFlipbook = Shape.bBall && FlameMaterial != nullptr && FlameMaterial->Parent == VaelAssets::LoadOptional(Settings->BallFlipbook);
	for (int32 Index = 0; bBallFlipbook && Index < FMath::Min(NumTongues, 3); ++Index)
	{
		const float Size = Shape.Radius * 2.0f / 0.28f * Random.FRandRange(0.92f, 1.08f);
		const FVector Base = Random.GetUnitVector() * Shape.Radius * 0.08f - FVector(0.0f, 0.0f, Size * 0.35f);
		Flames.Add(Base, Size, Size, FLinearColor(Random.FRand(), 1.0f, 0.0f, 1.0f), FVector::UpVector);
		SparkBases.Add(FVector::ZeroVector);
	}

	for (int32 Index = 0; !bBallFlipbook && Index < NumTongues; ++Index)
	{
		FVector Base;
		FVector Normal;
		float FromMiddle = PlaceOnPatch(0.85f, Base, Normal);
		if (Index == 0 && !bWall && !Shape.bBall)
		{
			Base = FVector::ZeroVector;
			FromMiddle = 0.0f;
		}

		const float Middle = 1.0f - 0.45f * FromMiddle;
		const float Height = Shape.Height * Middle * Random.FRandRange(0.55f, 1.0f);
		// The flipbook frames are square with the flame in their middle
		const float Width = Height * Random.FRandRange(0.85f, 1.0f);

		// The foot sits in the ground, so the ground hides the round bottom of the card and the flame grows out of it
		Base.Z -= bWall ? Height * 0.05f : Shape.bFloating ? 0.0f : Height * 0.03f;
		Flames.Add(Base, Width, Height, FLinearColor(Random.FRand(), 0.7f + 0.4f * Middle, 0.0f, 1.0f), Normal);
		SparkBases.Add(Base + FVector(0.0f, 0.0f, Height * 0.25f));
	}

	ClearAllMeshSections();
	CreateMeshSection_LinearColor(0, Flames.Vertices, Flames.Triangles, Flames.Normals, Flames.UVs, Flames.Sizes, TArray<FVector2D>(), TArray<FVector2D>(),
		Flames.Colors, TArray<FProcMeshTangent>(), false);
	SetMaterial(0, FlameMaterial);

	// A bed of broad simulated fire that clings to the ground or the wall, so the fire burns as one patch and not as single tongues
	if (!Shape.bFloating && BedMaterial != nullptr && Shape.Radius > 110.0f)
	{
		FCards Bed;
		const int32 NumBed = FMath::Clamp(NumTongues / 2, 1, 6);
		for (int32 Index = 0; Index < NumBed; ++Index)
		{
			FVector Base;
			FVector Normal;
			PlaceOnPatch(Index == 0 ? 0.0f : 0.6f, Base, Normal);
			const float Size = FMath::Max(Shape.Radius * Random.FRandRange(1.6f, 2.1f), Shape.Height * 0.55f);
			Base.Z -= bWall ? Size * 0.05f : Size * 0.03f;
			Bed.Add(Base, Size, Size * 1.1f, FLinearColor(Random.FRand(), 1.0f, 0.0f, 1.0f), Normal);
		}
		CreateMeshSection_LinearColor(3, Bed.Vertices, Bed.Triangles, Bed.Normals, Bed.UVs, Bed.Sizes, TArray<FVector2D>(), TArray<FVector2D>(),
			Bed.Colors, TArray<FProcMeshTangent>(), false);
		SetMaterial(3, BedMaterial);
	}

	// Sparks: tiny cards that the material lifts, each in its own loop
	if (Shape.bSparks)
	{
		FCards Sparks;
		const int32 NumSparks = FMath::Clamp(NumTongues * 2, 4, 60);
		for (int32 Index = 0; Index < NumSparks; ++Index)
		{
			const FVector Base = SparkBases[Index % SparkBases.Num()] + FVector(Random.FRandRange(-6.0f, 6.0f), Random.FRandRange(-6.0f, 6.0f), 0.0f);
			Sparks.Add(Base, SparkSize, SparkSize, FLinearColor(Random.FRand(), Random.FRand(), Random.FRandRange(0.5f, 1.3f), 1.0f), FVector::UpVector);
		}
		CreateMeshSection_LinearColor(1, Sparks.Vertices, Sparks.Triangles, Sparks.Normals, Sparks.UVs, Sparks.Sizes, TArray<FVector2D>(), TArray<FVector2D>(),
			Sparks.Colors, TArray<FProcMeshTangent>(), false);
		SetMaterial(1, SparkMaterial);
	}

	// A column of dark smoke above a smoky fire
	if (Shape.Smoke > 0.3f && SmokeMaterial != nullptr)
	{
		FCards Smoke;
		const int32 NumPuffs = FMath::Clamp(NumTongues / 4, 1, 5);
		for (int32 Index = 0; Index < NumPuffs; ++Index)
		{
			FVector Base;
			FVector Normal;
			PlaceOnPatch(0.4f, Base, Normal);
			Base.Z = (Shape.bBall ? 0.0f : Shape.Height * 0.4f) + Random.FRandRange(-0.1f, 0.1f) * Shape.Height;
			const float Height = Shape.Height * Random.FRandRange(1.4f, 2.0f);
			Smoke.Add(Base, Shape.Radius * 1.1f + Shape.Height * 0.35f, Height, FLinearColor(Random.FRand(), 0.0f, 0.0f, 1.0f), FVector::UpVector);
		}
		CreateMeshSection_LinearColor(2, Smoke.Vertices, Smoke.Triangles, Smoke.Normals, Smoke.UVs, Smoke.Sizes, TArray<FVector2D>(), TArray<FVector2D>(),
			Smoke.Colors, TArray<FProcMeshTangent>(), false);
		SetMaterial(2, SmokeMaterial);
	}

	// Flames on cards are gone (Boris 2026-10-22): the flames are simulated volumes, the cards keep only the sparks
	if (!Settings->bCardFlames)
	{
		SetMeshSectionVisible(0, false);
		SetMeshSectionVisible(2, false);
		SetMeshSectionVisible(3, false);
	}
}

FBoxSphereBounds UVaelFireComponent::CalcBounds(const FTransform& LocalToWorld) const
{
	// The cards unfold from their base points in the material: the whole fire with its rising sparks
	const float Reach = Shape.Radius + Shape.Height * 0.5f;
	const FBox Box(FVector(-Reach, -Reach, -Shape.Height * 0.3f), FVector(Reach, Reach, Shape.Height * 2.0f));
	return FBoxSphereBounds(Box).TransformBy(LocalToWorld);
}

void UVaelFireComponent::TakeLight()
{
	if (Light != nullptr || NumLitFires >= UVaelFireSettings::Get()->MaxLitFires)
	{
		return;
	}
	++NumLitFires;
	Light = NewObject<UPointLightComponent>(GetOwner());
	Light->SetupAttachment(this);
	Light->SetUsingAbsoluteRotation(true);
	Light->SetRelativeLocation(FVector(0.0f, 0.0f, Shape.Height * 0.45f) + Shape.WallNormal * 25.0f);
	Light->SetLightColor(UVaelFireSettings::Get()->LightColor);
	Light->SetIntensityUnits(ELightUnits::Candelas);
	Light->SetIntensity(0.0f);
	Light->SetAttenuationRadius(300.0f + Shape.Radius * 7.0f);
	Light->SetCastShadows(false);
	// The light sits inside the flames: it would light up the smoke of its own fire volume like fog
	Light->SetVolumetricScatteringIntensity(0.0f);
	Light->RegisterComponent();
}

void UVaelFireComponent::ReleaseLight()
{
	if (Light != nullptr)
	{
		Light->DestroyComponent();
		Light = nullptr;
		NumLitFires = FMath::Max(0, NumLitFires - 1);
	}
}

void UVaelFireComponent::OnUnregister()
{
	ReleaseLight();
	ReleaseVolume();
	if (LiveFire != nullptr)
	{
		LiveFire->DestroyComponent();
		LiveFire = nullptr;
	}
	for (UInstancedStaticMeshComponent* Instances : Tongues)
	{
		if (Instances != nullptr)
		{
			Instances->DestroyComponent();
		}
	}
	Tongues.Reset();
	if (Scorch != nullptr)
	{
		Scorch->DestroyComponent();
		Scorch = nullptr;
	}
	Super::OnUnregister();
}

void UVaelFireComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	Strength = FMath::FInterpConstantTo(Strength, TargetStrength, DeltaTime, FadeSpeed);
	if (FlameMaterial != nullptr)
	{
		FlameMaterial->SetScalarParameterValue(StrengthParameter, Strength);
		SparkMaterial->SetScalarParameterValue(StrengthParameter, Strength);
		if (BedMaterial != nullptr)
		{
			BedMaterial->SetScalarParameterValue(StrengthParameter, Strength);
		}
		if (SmokeMaterial != nullptr)
		{
			SmokeMaterial->SetScalarParameterValue(StrengthParameter, Strength);
		}
	}

	// The light breathes calmly with the flames, with only a faint flutter
	if (Light != nullptr)
	{
		const float Time = GetWorld()->GetTimeSeconds() + FlickerPhase;
		const float Flicker = 1.0f + 0.07f * FMath::Sin(Time * 1.3f) + 0.04f * FMath::Sin(Time * 3.7f + 1.3f) + 0.015f * FMath::Sin(Time * 9.0f);
		const float Size = FMath::Sqrt(Shape.Radius / 50.0f) * Shape.Heat;
		Light->SetIntensity(UVaelFireSettings::Get()->LightIntensity * Size * Strength * Flicker);
		if (Strength <= 0.0f && TargetStrength <= 0.0f)
		{
			ReleaseLight();
		}
	}

	// A new stage comes up while the one the fire grew out of fades away
	if (OldVolume != nullptr)
	{
		VolumeBlend = FMath::Min(1.0f, VolumeBlend + DeltaTime / UVaelFireSettings::Get()->StageBlendSeconds);
		OldVolumeMaterial->SetScalarParameterValue(StrengthParameter, Strength * (1.0f - VolumeBlend));
		PlaceVolume(OldVolume, OldVolumeSize, OldVolumeHeight);
		if (VolumeBlend >= 1.0f)
		{
			OldVolume->DestroyComponent();
			OldVolume = nullptr;
			OldVolumeMaterial = nullptr;
			NumVolumeFires = FMath::Max(0, NumVolumeFires - 1);
		}
	}
	if (VolumeMaterial != nullptr)
	{
		VolumeMaterial->SetScalarParameterValue(StrengthParameter, Strength * VolumeBlend);
	}

	// The size of a voxel arrives with the first frame, so the foot of the fire is set on the ground once it is known
	if (Volume != nullptr)
	{
		PlaceVolume(Volume, VolumeSize, VolumeHeight);
	}
	if (Volume != nullptr && Strength <= 0.0f && TargetStrength <= 0.0f)
	{
		ReleaseVolume();
	}
	if (LiveFire != nullptr && TargetStrength <= 0.0f && LiveFire->IsActive())
	{
		// The simulation stops feeding its fire and lets it burn out by itself
		LiveFire->Deactivate();
	}

	// The roots creep outwards at full pace while the fire is fed, slow down when nothing more comes and stop when it is out
	SinceFed += DeltaTime;
	const UVaelFireSettings* RootSettings = UVaelFireSettings::Get();
	const float Pace = TargetStrength > 0.0f ? FMath::Exp(-SinceFed / RootSettings->RootSlowdownSeconds) : 0.0f;
	RootGrowth = FMath::Min(1.0f, RootGrowth + DeltaTime * Pace / RootSettings->RootGrowSeconds);
	if (TongueMaterial != nullptr)
	{
		TongueMaterial->SetScalarParameterValue(StrengthParameter, Strength);
	}
	if (!Tongues.IsEmpty() && Strength > 0.0f)
	{
		UpdateTongues(DeltaTime);
	}
	if (ScorchMaterial != nullptr)
	{
		ScorchMaterial->SetScalarParameterValue(GrowthParameter, RootGrowth);
		ScorchMaterial->SetScalarParameterValue(FrontParameter, RootGrowth < 1.0f ? Pace * Strength : 0.0f);
	}

	// The scorch mark glows while the fire burns, its embers die down after, then it fades away
	if (Strength <= 0.0f && TargetStrength <= 0.0f)
	{
		OutTime = OutTime < 0.0f ? 0.0f : OutTime + DeltaTime;
	}
	if (ScorchMaterial != nullptr)
	{
		const float Linger = UVaelFireSettings::Get()->ScorchLinger;
		const float Cooling = OutTime < 0.0f ? 1.0f : FMath::Clamp(1.0f - OutTime / 4.0f, 0.0f, 1.0f);
		ScorchMaterial->SetScalarParameterValue(EmberParameter, FMath::Max(Strength, Cooling * 0.6f));
		ScorchMaterial->SetScalarParameterValue(CoverParameter, OutTime < 0.0f ? 0.9f * FMath::Min(1.0f, Strength * 3.0f) : 0.9f * FMath::Clamp(1.0f - OutTime / FMath::Max(Linger, 0.1f), 0.0f, 1.0f));
	}

	const bool bScorchGone = Scorch == nullptr || OutTime >= UVaelFireSettings::Get()->ScorchLinger;
	if (bDestroyWhenOut && OutTime >= 0.0f && bScorchGone)
	{
		DestroyComponent();
	}
}

void UVaelFireComponent::SetFlowDirection(const FVector& Direction)
{
	const FLinearColor Value(Direction.GetSafeNormal());
	for (UMaterialInstanceDynamic* Material : { FlameMaterial.Get(), SparkMaterial.Get(), SmokeMaterial.Get(), BedMaterial.Get() })
	{
		if (Material != nullptr)
		{
			Material->SetVectorParameterValue(FlowParameter, Value);
		}
	}
}

void UVaelFireComponent::SetSizeScale(float Scale)
{
	for (UMaterialInstanceDynamic* Material : { FlameMaterial.Get(), SparkMaterial.Get(), SmokeMaterial.Get(), BedMaterial.Get() })
	{
		if (Material != nullptr)
		{
			Material->SetScalarParameterValue(SizeParameter, Scale);
		}
	}
	SizeScale = Scale;
	UpdateTongues();
	if (Volume != nullptr)
	{
		VolumeSize = VolumeScale * Scale;
		Volume->SetRelativeScale3D(FVector(VolumeSize));
		PlaceVolume(Volume, VolumeSize, VolumeHeight);
	}
}

void UVaelFireComponent::PlaceVolume(UHeterogeneousVolumeComponent* Which, float Size, float& KnownHeight)
{
	// The volume turns around its middle; lift it so the foot of the fire, or the middle of the ball, sits on this component
	const UVaelFireSettings* Settings = UVaelFireSettings::Get();
	const float Height = Which->VolumeResolution.Z * Which->FrameTransform.GetScale3D().Z;
	const float Lift = (Height * 0.5f - (Shape.bBall ? Settings->BallVolumeCentre : Settings->VolumeFloor)) * Size;
	// A fire on a wall rises in front of it from the burn mark
	const FVector Out = FVector(Shape.WallNormal.X, Shape.WallNormal.Y, 0.0f).GetSafeNormal() * Shape.Radius * 0.6f;
	if (!FMath::IsNearlyEqual(Height, KnownHeight))
	{
		// The volume marches its rays in steps of a voxel, which it works out from its bounds when its render state is made:
		// made before the first frame arrived, the steps are as wide as a whole fire and it shows as a pale haze
		KnownHeight = Height;
		Which->UpdateBounds();
		Which->MarkRenderStateDirty();
	}
	const FVector Place = Out + FVector(0.0f, 0.0f, Lift);
	if (!Which->GetRelativeLocation().Equals(Place, 0.5f))
	{
		Which->SetRelativeLocation(Place);
	}
}
