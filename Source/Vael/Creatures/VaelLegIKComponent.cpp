// Copyright Epic Games, Inc. All Rights Reserved.

#include "Creatures/VaelLegIKComponent.h"
#include "AnimationRuntime.h"
#include "Components/PoseableMeshComponent.h"
#include "Engine/SkinnedAsset.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Vael.h"

namespace
{
	/** Spring of the body after a jolt: angular frequency, critically damped */
	constexpr float LegJoltFrequency = 16.0f;

	/** Feet farther than this many step distances from their spot are put down at once, like after a teleport */
	constexpr float LegSnapSteps = 5.0f;

	/** Planted feet this far from their spot (share of the step distance) step along with their group, so the groups stay in step */
	constexpr float LegJoinShare = 0.45f;
}

UVaelLegIKComponent::UVaelLegIKComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickGroup = TG_PostPhysics;
}

void UVaelLegIKComponent::Setup(UPoseableMeshComponent* InMesh)
{
	Mesh = InMesh;
	Legs.Reset();
	PosedBones.Reset();

	const USkinnedAsset* Asset = Mesh != nullptr ? Mesh->GetSkinnedAsset() : nullptr;
	if (Asset == nullptr)
	{
		return;
	}

	const FReferenceSkeleton& RefSkeleton = Asset->GetRefSkeleton();
	const int32 BodyIndex = RefSkeleton.FindBoneIndex(BodyBone);
	if (BodyIndex == INDEX_NONE)
	{
		UE_LOG(LogVael, Warning, TEXT("'%s': mesh '%s' has no body bone '%s', the legs stay as they are"), *GetNameSafe(GetOwner()), *GetNameSafe(Asset), *BodyBone.ToString());
		return;
	}

	RestBody = FAnimationRuntime::GetComponentSpaceTransformRefPose(RefSkeleton, BodyIndex);
	PosedBones.Add(BodyBone);

	// Legs by their fixed names: leg_L1_coxa ... leg_R3_foot
	for (int32 BoneIndex = 0; BoneIndex < RefSkeleton.GetNum(); ++BoneIndex)
	{
		const FString Name = RefSkeleton.GetBoneName(BoneIndex).ToString();
		if (!Name.StartsWith(TEXT("leg_")) || !Name.EndsWith(TEXT("_coxa")))
		{
			continue;
		}

		const FString Base = Name.LeftChop(5);
		FVaelLeg Leg;
		Leg.Coxa = *Name;
		Leg.Femur = *(Base + TEXT("_femur"));
		Leg.Tibia = *(Base + TEXT("_tibia"));
		Leg.Foot = *(Base + TEXT("_foot"));

		const int32 FootIndex = RefSkeleton.FindBoneIndex(Leg.Foot);
		if (RefSkeleton.FindBoneIndex(Leg.Femur) == INDEX_NONE || RefSkeleton.FindBoneIndex(Leg.Tibia) == INDEX_NONE || FootIndex == INDEX_NONE)
		{
			UE_LOG(LogVael, Warning, TEXT("'%s': leg '%s' misses femur, tibia or foot"), *GetNameSafe(GetOwner()), *Base);
			continue;
		}

		Leg.RestFoot = FAnimationRuntime::GetComponentSpaceTransformRefPose(RefSkeleton, FootIndex).GetLocation();

		// Tripod gait: L1, R2, L3 step together, then R1, L2, R3
		const TCHAR Side = Base.Len() > 4 ? Base[4] : TEXT('L');
		const int32 Number = Base.Len() > 5 ? FCString::Atoi(*Base.Mid(5, 1)) : 1;
		Leg.Group = (Number + (Side == TEXT('L') ? 0 : 1)) % 2;

		PosedBones.Append({ Leg.Coxa, Leg.Femur, Leg.Tibia, Leg.Foot });
		Legs.Add(Leg);
	}

	UE_LOG(LogVael, Verbose, TEXT("'%s': %d legs found on '%s'"), *GetNameSafe(GetOwner()), Legs.Num(), *GetNameSafe(Asset));

	BodyTilt = FQuat::Identity;
	BodyHeight = 0.0f;
	PlantAllFeet();
}

void UVaelLegIKComponent::PlantAllFeet()
{
	if (Mesh == nullptr || GetOwner() == nullptr)
	{
		return;
	}

	for (FVaelLeg& Leg : Legs)
	{
		Leg.Planted = FindRestingSpot(Leg, FVector::ZeroVector);
		Leg.StepTarget = Leg.Planted;
		Leg.Current = Leg.Planted;
		Leg.StepAlpha = 1.0f;
	}

	LastActorLocation = GetOwner()->GetActorLocation();
}

void UVaelLegIKComponent::AddBodyJolt(const FVector& WorldOffset)
{
	JoltVelocity += WorldOffset * LegJoltFrequency;
}

int32 UVaelLegIKComponent::GetNumStepping() const
{
	int32 Stepping = 0;
	for (const FVaelLeg& Leg : Legs)
	{
		Stepping += Leg.StepAlpha < 1.0f ? 1 : 0;
	}
	return Stepping;
}

FVector UVaelLegIKComponent::GetFootLocation(int32 LegIndex) const
{
	return Legs.IsValidIndex(LegIndex) ? Legs[LegIndex].Current : FVector::ZeroVector;
}

FVector UVaelLegIKComponent::FindRestingSpot(const FVaelLeg& Leg, const FVector& Lead) const
{
	const FVector Spot = Mesh->GetComponentTransform().TransformPosition(Leg.RestFoot) + Lead;

	FCollisionQueryParams Params(SCENE_QUERY_STAT(VaelLegGround), false, GetOwner());
	FCollisionObjectQueryParams Objects;
	Objects.AddObjectTypesToQuery(ECC_WorldStatic);
	Objects.AddObjectTypesToQuery(ECC_WorldDynamic);

	FHitResult Hit;
	if (GetWorld()->LineTraceSingleByObjectType(Hit, Spot + FVector::UpVector * TraceUp, Spot - FVector::UpVector * TraceDown, Objects, Params))
	{
		// The foot bone sits a little above its sole, as in the reference pose
		return Hit.ImpactPoint + FVector::UpVector * FMath::Max(Leg.RestFoot.Z * Mesh->GetComponentScale().Z, 0.0f);
	}

	return Spot;
}

void UVaelLegIKComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (Mesh == nullptr || Legs.IsEmpty() || DeltaTime <= 0.0f || !Mesh->IsRegistered())
	{
		return;
	}

	UpdateFeet(DeltaTime);
	PoseBody(DeltaTime);

	for (const FVaelLeg& Leg : Legs)
	{
		SolveLeg(Leg, Leg.Current);
	}

	OnLegsPosed.Broadcast();
}

void UVaelLegIKComponent::UpdateFeet(float DeltaTime)
{
	const FVector ActorLocation = GetOwner()->GetActorLocation();
	const FVector Velocity = (ActorLocation - LastActorLocation) / DeltaTime;
	LastActorLocation = ActorLocation;

	// Dead or curled up: the feet stay where they are
	const bool bMayStep = Curl < 0.5f;
	const FVector Lead = Velocity.GetClampedToMaxSize(1500.0f) * StepLead * FVector(1.0f, 1.0f, 0.0f);

	TArray<FVector> Spots;
	Spots.SetNum(Legs.Num());
	for (int32 LegIndex = 0; LegIndex < Legs.Num(); ++LegIndex)
	{
		Spots[LegIndex] = FindRestingSpot(Legs[LegIndex], Lead);
	}

	// A teleport or a knockback far away: put the feet down at once
	for (int32 LegIndex = 0; LegIndex < Legs.Num(); ++LegIndex)
	{
		if (FVector::Dist(Legs[LegIndex].Planted, Spots[LegIndex]) > StepDistance * LegSnapSteps)
		{
			PlantAllFeet();
			return;
		}
	}

	// Steps under way follow their moving spot and land
	int32 SteppingInGroup[2] = { 0, 0 };
	for (int32 LegIndex = 0; LegIndex < Legs.Num(); ++LegIndex)
	{
		FVaelLeg& Leg = Legs[LegIndex];
		if (Leg.StepAlpha >= 1.0f)
		{
			continue;
		}

		Leg.StepTarget = Spots[LegIndex];
		Leg.StepAlpha = FMath::Min(Leg.StepAlpha + DeltaTime / Leg.StepTime, 1.0f);

		if (Leg.StepAlpha >= 1.0f)
		{
			Leg.Planted = Leg.StepTarget;
			Leg.Current = Leg.Planted;
		}
		else
		{
			const float Eased = FMath::InterpEaseInOut(0.0f, 1.0f, Leg.StepAlpha, 2.0f);
			Leg.Current = FMath::Lerp(Leg.Planted, Leg.StepTarget, Eased) + FVector::UpVector * FMath::Sin(Leg.StepAlpha * UE_PI) * StepHeight;
			++SteppingInGroup[Leg.Group];
		}
	}

	if (!bMayStep)
	{
		return;
	}

	// The group whose feet lag behind most steps, but only while the other group stands
	float Need[2] = { 0.0f, 0.0f };
	for (int32 LegIndex = 0; LegIndex < Legs.Num(); ++LegIndex)
	{
		const FVaelLeg& Leg = Legs[LegIndex];
		if (Leg.StepAlpha >= 1.0f)
		{
			Need[Leg.Group] = FMath::Max(Need[Leg.Group], FVector::Dist(Leg.Planted, Spots[LegIndex]));
		}
	}

	const int32 Group = Need[0] >= Need[1] ? 0 : 1;
	if (Need[Group] < StepDistance || SteppingInGroup[1 - Group] > 0)
	{
		return;
	}

	const float Speed = Velocity.Size2D();
	const float StepTime = FMath::Clamp(StepDistance * 0.9f / FMath::Max(Speed, 1.0f), MinStepDuration, FMath::Max(StepDuration, MinStepDuration));

	for (int32 LegIndex = 0; LegIndex < Legs.Num(); ++LegIndex)
	{
		FVaelLeg& Leg = Legs[LegIndex];
		if (Leg.Group == Group && Leg.StepAlpha >= 1.0f && FVector::Dist(Leg.Planted, Spots[LegIndex]) > StepDistance * LegJoinShare)
		{
			Leg.StepAlpha = 0.0f;
			Leg.StepTime = StepTime * FMath::FRandRange(0.92f, 1.08f);
			Leg.StepTarget = Spots[LegIndex];
			++StepCount;
		}
	}
}

void UVaelLegIKComponent::PoseBody(float DeltaTime)
{
	for (const FName& Bone : PosedBones)
	{
		Mesh->ResetBoneTransformByName(Bone);
	}

	const AActor* Owner = GetOwner();
	const FVector Center = Owner->GetActorLocation();
	const FVector Forward = Owner->GetActorForwardVector().GetSafeNormal2D();
	const FVector Right = FVector::CrossProduct(FVector::UpVector, Forward);

	// Plane through the feet, z = A * forward + B * right + C, by least squares around the actor
	double SumFF = 0.0, SumRR = 0.0, SumFR = 0.0, SumFZ = 0.0, SumRZ = 0.0, SumF = 0.0, SumR = 0.0, SumZ = 0.0;
	double SumRest = 0.0, Lift = 0.0;
	for (const FVaelLeg& Leg : Legs)
	{
		const FVector Offset = Leg.Current - Center;
		const double F = FVector::DotProduct(Offset, Forward);
		const double R = FVector::DotProduct(Offset, Right);
		const double Z = Leg.Current.Z - (Leg.StepAlpha < 1.0f ? FMath::Sin(Leg.StepAlpha * UE_PI) * StepHeight : 0.0f);
		SumFF += F * F; SumRR += R * R; SumFR += F * R; SumFZ += F * Z; SumRZ += R * Z; SumF += F; SumR += R; SumZ += Z;
		SumRest += Mesh->GetComponentTransform().TransformPosition(Leg.RestFoot).Z;
		Lift += Leg.StepAlpha < 1.0f ? FMath::Sin(Leg.StepAlpha * UE_PI) : 0.0f;
	}

	const double Num = Legs.Num();
	const double MeanF = SumF / Num, MeanR = SumR / Num, MeanZ = SumZ / Num;
	const double CFF = SumFF / Num - MeanF * MeanF, CRR = SumRR / Num - MeanR * MeanR, CFR = SumFR / Num - MeanF * MeanR;
	const double CFZ = SumFZ / Num - MeanF * MeanZ, CRZ = SumRZ / Num - MeanR * MeanZ;
	const double Det = CFF * CRR - CFR * CFR;
	const double A = FMath::Abs(Det) > 1.0 ? (CFZ * CRR - CRZ * CFR) / Det : 0.0;
	const double B = FMath::Abs(Det) > 1.0 ? (CRZ * CFF - CFZ * CFR) / Det : 0.0;
	const double PlaneHeight = MeanZ - A * MeanF - B * MeanR;

	const FVector Normal = (FVector::UpVector - Forward * A - Right * B).GetSafeNormal();
	FQuat TargetTilt = FQuat::FindBetweenNormals(FVector::UpVector, Normal);
	FVector TiltAxis;
	float TiltAngle;
	TargetTilt.ToAxisAndAngle(TiltAxis, TiltAngle);
	TargetTilt = FQuat(TiltAxis, FMath::Min(TiltAngle, FMath::DegreesToRadians(MaxBodyTilt)));

	const float Blend = 1.0f - FMath::Exp(-BodySmoothing * DeltaTime);
	BodyTilt = FQuat::Slerp(BodyTilt, TargetTilt, Blend);

	const float TargetHeight = static_cast<float>(PlaneHeight - SumRest / Num) * BodyHeightFollow - Curl * 25.0f;
	BodyHeight = FMath::Lerp(BodyHeight, TargetHeight, Blend);
	const float Bob = BodyBob * static_cast<float>(Lift / FMath::Max(Num * 0.5, 1.0));

	// Jolts from hits spring back
	const FVector Spring = -Jolt * (LegJoltFrequency * LegJoltFrequency) - JoltVelocity * (2.0f * LegJoltFrequency);
	JoltVelocity += Spring * DeltaTime;
	Jolt += JoltVelocity * DeltaTime;

	const FQuat ActorQuat = Owner->GetActorQuat();
	const FQuat WorldDelta = BodyTilt * (ActorQuat * FQuat(ExtraTilt) * ActorQuat.Inverse());
	const FQuat ComponentQuat = Mesh->GetComponentQuat();
	const FQuat LocalDelta = ComponentQuat.Inverse() * WorldDelta * ComponentQuat;
	const FVector LocalOffset = Mesh->GetComponentTransform().InverseTransformVector(FVector::UpVector * (BodyHeight + Bob) + Jolt);

	FTransform Body = RestBody;
	Body.SetRotation(LocalDelta * RestBody.GetRotation());
	Body.SetLocation(RestBody.GetLocation() + LocalOffset);
	Mesh->SetBoneTransformByName(BodyBone, Body, EBoneSpaces::ComponentSpace);
}

void UVaelLegIKComponent::SolveLeg(const FVaelLeg& Leg, const FVector& WorldTarget)
{
	const FTransform CoxaT = Mesh->GetBoneTransformByName(Leg.Coxa, EBoneSpaces::ComponentSpace);
	const FTransform FemurT = Mesh->GetBoneTransformByName(Leg.Femur, EBoneSpaces::ComponentSpace);
	const FTransform TibiaT = Mesh->GetBoneTransformByName(Leg.Tibia, EBoneSpaces::ComponentSpace);
	const FTransform FootT = Mesh->GetBoneTransformByName(Leg.Foot, EBoneSpaces::ComponentSpace);
	const FTransform BodyT = Mesh->GetBoneTransformByName(BodyBone, EBoneSpaces::ComponentSpace);

	const FVector Up = (BodyT.GetRotation() * RestBody.GetRotation().Inverse()).RotateVector(FVector::UpVector);
	const FVector Pc = CoxaT.GetLocation();

	FVector Target = Mesh->GetComponentTransform().InverseTransformPosition(WorldTarget);

	// Curled up: the feet fold in under the body
	if (Curl > 0.0f)
	{
		const FVector Folded = Pc + (BodyT.GetLocation() - Pc) * 0.35f - Up * 6.0f;
		Target = FMath::Lerp(Target, Folded, Curl * 0.85f);
	}

	// The hip turns the leg around the up axis of the body towards the target
	FQuat Yaw = FQuat::Identity;
	const FVector FromFoot = FVector::VectorPlaneProject(FootT.GetLocation() - Pc, Up);
	const FVector ToTarget = FVector::VectorPlaneProject(Target - Pc, Up);
	if (FromFoot.SizeSquared() > 1.0f && ToTarget.SizeSquared() > 1.0f)
	{
		Yaw = FQuat::FindBetweenNormals(FromFoot.GetSafeNormal(), ToTarget.GetSafeNormal());
	}

	const FVector Pf = Pc + Yaw.RotateVector(FemurT.GetLocation() - Pc);
	const FVector Pk = Pc + Yaw.RotateVector(TibiaT.GetLocation() - Pc);
	const FVector Pe = Pc + Yaw.RotateVector(FootT.GetLocation() - Pc);

	// Two bones from the femur: the knee keeps bending the way it does in the reference pose
	const float Upper = FVector::Dist(Pf, Pk);
	const float Lower = FVector::Dist(Pk, Pe);
	const FVector Reach = Target - Pf;
	if (Upper < 0.1f || Lower < 0.1f || Reach.SizeSquared() < 0.01f)
	{
		return;
	}

	const float Distance = FMath::Clamp(static_cast<float>(Reach.Size()), FMath::Abs(Upper - Lower) + 0.5f, Upper + Lower - 0.05f);
	const FVector Direction = Reach.GetSafeNormal();
	FVector Bend = FVector::VectorPlaneProject(Pk - Pf, Direction);
	if (Bend.SizeSquared() < 0.01f)
	{
		Bend = FVector::VectorPlaneProject(Up, Direction);
	}
	Bend.Normalize();

	const float Along = (Upper * Upper - Lower * Lower + Distance * Distance) / (2.0f * Distance);
	const float Out = FMath::Sqrt(FMath::Max(Upper * Upper - Along * Along, 0.0f));
	const FVector Knee = Pf + Direction * Along + Bend * Out;
	const FVector End = Pf + Direction * Distance;

	const FQuat FemurTurn = FQuat::FindBetweenNormals((Pk - Pf).GetSafeNormal(), (Knee - Pf).GetSafeNormal());
	const FVector ShinBefore = FemurTurn.RotateVector(Pe - Pk).GetSafeNormal();
	const FQuat TibiaTurn = FQuat::FindBetweenNormals(ShinBefore, (End - Knee).GetSafeNormal());

	Mesh->SetBoneTransformByName(Leg.Coxa, FTransform(Yaw * CoxaT.GetRotation(), Pc, CoxaT.GetScale3D()), EBoneSpaces::ComponentSpace);
	Mesh->SetBoneTransformByName(Leg.Femur, FTransform(FemurTurn * Yaw * FemurT.GetRotation(), Pf, FemurT.GetScale3D()), EBoneSpaces::ComponentSpace);
	Mesh->SetBoneTransformByName(Leg.Tibia, FTransform(TibiaTurn * FemurTurn * Yaw * TibiaT.GetRotation(), Knee, TibiaT.GetScale3D()), EBoneSpaces::ComponentSpace);
	Mesh->SetBoneTransformByName(Leg.Foot, FTransform(TibiaTurn * FemurTurn * Yaw * FootT.GetRotation(), End, FootT.GetScale3D()), EBoneSpaces::ComponentSpace);
}
