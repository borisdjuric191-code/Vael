// Copyright Epic Games, Inc. All Rights Reserved.

#include "Combat/VaelArcShot.h"
#include "UObject/ConstructorHelpers.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"

AVaelArcShot::AVaelArcShot()
{
	PrimaryActorTick.bCanEverTick = true;

	Shell = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Shell"));
	Shell->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Shell->SetCastShadow(true);
	RootComponent = Shell;

	static ConstructorHelpers::FObjectFinder<UStaticMesh> ShellMesh(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> ShellMaterial(TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
	if (ShellMesh.Succeeded())
	{
		Shell->SetStaticMesh(ShellMesh.Object);
	}
	if (ShellMaterial.Succeeded())
	{
		Shell->SetMaterial(0, ShellMaterial.Object);
	}
}

AVaelArcShot* AVaelArcShot::Lob(UWorld* World, const FVector& InStart, const FVector& InEnd, float InFlightTime, float InApex, const FLinearColor& Color, float Size)
{
	if (World == nullptr)
	{
		return nullptr;
	}

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AVaelArcShot* Shot = World->SpawnActor<AVaelArcShot>(InStart, FRotator::ZeroRotator, Params);
	if (Shot == nullptr)
	{
		return nullptr;
	}

	Shot->Start = InStart;
	Shot->End = InEnd;
	Shot->FlightTime = FMath::Max(InFlightTime, 0.05f);
	Shot->Apex = InApex;
	Shot->Spin = FRotator(FMath::FRandRange(300.0f, 600.0f), FMath::FRandRange(-200.0f, 200.0f), 0.0f);
	Shot->Shell->SetWorldScale3D(FVector(Size / 50.0f) * FVector(1.0f, 1.0f, 1.25f));

	if (UMaterialInstanceDynamic* Material = Shot->Shell->CreateAndSetMaterialInstanceDynamic(0))
	{
		Material->SetVectorParameterValue(TEXT("Color"), Color);
	}

	return Shot;
}

void AVaelArcShot::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	Age += DeltaSeconds;
	const float Alpha = FMath::Clamp(Age / FlightTime, 0.0f, 1.0f);

	// A parabola through start and end whose top lies the apex above the higher end
	const float Top = FMath::Max(Start.Z, End.Z) + Apex;
	const float Rise = Top - Start.Z;
	const float Fall = Top - End.Z;
	const float Peak = Rise + Fall > 0.0f ? FMath::Sqrt(Rise) / (FMath::Sqrt(Rise) + FMath::Sqrt(Fall)) : 0.5f;
	const float Height = Alpha <= Peak
		? Top - Rise * FMath::Square(1.0f - Alpha / Peak)
		: Top - Fall * FMath::Square((Alpha - Peak) / (1.0f - Peak));

	FVector Location = FMath::Lerp(Start, End, Alpha);
	Location.Z = Height;
	SetActorLocationAndRotation(Location, GetActorRotation() + Spin * DeltaSeconds);

	if (Alpha >= 1.0f)
	{
		Destroy();
	}
}
