#include "Gameplay/Ghosts/GvTGhostPerceptionComponent.h"
#include "EngineUtils.h"
#include "Gameplay/Characters/Thieves/GvTThiefCharacter.h"
#include "Systems/Noise/GvTNoiseSubsystem.h"
#include "DrawDebugHelpers.h"

UGvTGhostPerceptionComponent::UGvTGhostPerceptionComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = true;
}

bool UGvTGhostPerceptionComponent::IsValidVictim(const APawn* CandidatePawn) const
{
	if (!IsValid(CandidatePawn))
	{
		return false;
	}

	if (const AGvTThiefCharacter* Thief = Cast<AGvTThiefCharacter>(CandidatePawn))
	{
		return !Thief->IsDead();
	}

	return true;
}

bool UGvTGhostPerceptionComponent::CanSeePawn(const APawn* CandidatePawn) const
{
	const AActor* OwnerActor = GetOwner();
	if (!OwnerActor || !IsValidVictim(CandidatePawn) || !GetWorld())
	{
		return false;
	}

	const FVector OwnerLocation = OwnerActor->GetActorLocation();
	const FVector TargetLocation = CandidatePawn->GetActorLocation();

	FVector ToTarget = TargetLocation - OwnerLocation;
	ToTarget.Z = 0.f;

	const float DistSq = ToTarget.SizeSquared();
	if (DistSq > FMath::Square(VisionRange))
	{
		return false;
	}

	if (!ToTarget.Normalize())
	{
		return true;
	}

	const FVector Forward = OwnerActor->GetActorForwardVector().GetSafeNormal2D();
	const float Dot = FVector::DotProduct(Forward, ToTarget);
	const float MinDot = FMath::Cos(FMath::DegreesToRadians(VisionHalfAngleDegrees));

	if (Dot < MinDot)
	{
		return false;
	}

	FHitResult Hit;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(GvT_GhostPerception_Vision), false, OwnerActor);
	Params.AddIgnoredActor(OwnerActor);

	const FVector Start = OwnerLocation + FVector(0.f, 0.f, VisionTraceHeight);
	const FVector End = TargetLocation + FVector(0.f, 0.f, VisionTraceHeight);

	const bool bHit = GetWorld()->LineTraceSingleByChannel(Hit, Start, End, ECC_Visibility, Params);
	const bool bCanSee = !bHit || Hit.GetActor() == CandidatePawn;
	const bool bLOSBlocked = bHit && Hit.GetActor() != CandidatePawn;

	DrawPerceptionDebug(
		CandidatePawn,
		bCanSee,
		bLOSBlocked,
		Start,
		End,
		bHit ? Hit.Location : End);

	return bCanSee;
}

APawn* UGvTGhostPerceptionComponent::FindBestVisibleVictim(AActor* PreferredTarget) const
{
	if (!GetWorld())
	{
		return nullptr;
	}

	if (APawn* PreferredPawn = Cast<APawn>(PreferredTarget))
	{
		if (CanSeePawn(PreferredPawn))
		{
			return PreferredPawn;
		}
	}

	const AActor* OwnerActor = GetOwner();
	APawn* BestVictim = nullptr;
	float BestDistSq = TNumericLimits<float>::Max();

	for (TActorIterator<AGvTThiefCharacter> It(GetWorld()); It; ++It)
	{
		AGvTThiefCharacter* Candidate = *It;
		if (!CanSeePawn(Candidate))
		{
			continue;
		}

		const float DistSq = OwnerActor
			? FVector::DistSquared(OwnerActor->GetActorLocation(), Candidate->GetActorLocation())
			: 0.f;

		if (DistSq < BestDistSq)
		{
			BestDistSq = DistSq;
			BestVictim = Candidate;
		}
	}

	return BestVictim;
}

bool UGvTGhostPerceptionComponent::TryFindBestNoiseLocation(
	FVector& OutNoiseLocation,
	FGameplayTag& OutNoiseTag,
	float& OutScore) const
{
	OutNoiseLocation = FVector::ZeroVector;
	OutNoiseTag = FGameplayTag();
	OutScore = 0.f;

	const AActor* OwnerActor = GetOwner();
	if (!OwnerActor || !GetWorld())
	{
		return false;
	}

	if (UGameInstance* GI = GetWorld()->GetGameInstance())
	{
		if (UGvTNoiseSubsystem* NoiseSubsystem = GI->GetSubsystem<UGvTNoiseSubsystem>())
		{
			return NoiseSubsystem->TryGetBestRecentNoiseNearLocation(
				OwnerActor->GetActorLocation(),
				HearingRadius,
				NoiseMemorySeconds,
				OutNoiseLocation,
				OutNoiseTag,
				OutScore);
		}
	}

	return false;
}

void UGvTGhostPerceptionComponent::DrawPerceptionDebug(
	const APawn* CandidatePawn,
	bool bCanSee,
	bool bLOSBlocked,
	const FVector& TraceStart,
	const FVector& TraceEnd,
	const FVector& HitLocation) const
{
	if (!bDrawPerceptionDebug || !GetWorld())
	{
		return;
	}

	const AActor* OwnerActor = GetOwner();
	if (!OwnerActor)
	{
		return;
	}

	const FVector Origin = OwnerActor->GetActorLocation() + FVector(0.f, 0.f, VisionTraceHeight);
	const FVector Forward = OwnerActor->GetActorForwardVector();

	DrawDebugCone(
		GetWorld(),
		Origin,
		Forward,
		VisionRange,
		FMath::DegreesToRadians(VisionHalfAngleDegrees),
		FMath::DegreesToRadians(VisionHalfAngleDegrees),
		24,
		FColor::Green,
		false,
		DebugDrawDuration,
		0,
		2.f);

	DrawDebugSphere(
		GetWorld(),
		Origin,
		HearingRadius,
		32,
		FColor::Blue,
		false,
		DebugDrawDuration,
		0,
		1.f);

	if (CandidatePawn)
	{
		const FColor LineColor = bCanSee ? FColor::Green : FColor::Red;
		DrawDebugLine(
			GetWorld(),
			TraceStart,
			bLOSBlocked ? HitLocation : TraceEnd,
			LineColor,
			false,
			DebugDrawDuration,
			0,
			2.f);
	}
}

void UGvTGhostPerceptionComponent::TickComponent(
	float DeltaTime,
	ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (bDrawDetectionRangesConstantly)
	{
		DrawDetectionRangesDebug();
	}
}

void UGvTGhostPerceptionComponent::DrawDetectionRangesDebug() const
{
	if (!bDrawPerceptionDebug || !GetWorld())
	{
		return;
	}

	const AActor* OwnerActor = GetOwner();
	if (!OwnerActor)
	{
		return;
	}

	const FVector Origin = OwnerActor->GetActorLocation() + FVector(0.f, 0.f, VisionTraceHeight);
	const FVector Forward = OwnerActor->GetActorForwardVector();

	DrawDebugCone(
		GetWorld(),
		Origin,
		Forward,
		VisionRange,
		FMath::DegreesToRadians(VisionHalfAngleDegrees),
		FMath::DegreesToRadians(VisionHalfAngleDegrees),
		24,
		FColor::Green,
		false,
		DebugDrawDuration,
		0,
		2.f);

	DrawDebugSphere(
		GetWorld(),
		Origin,
		HearingRadius,
		32,
		FColor::Blue,
		false,
		DebugDrawDuration,
		0,
		1.f);
}