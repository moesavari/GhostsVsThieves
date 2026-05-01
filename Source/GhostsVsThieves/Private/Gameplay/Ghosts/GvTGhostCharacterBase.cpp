#include "Gameplay/Ghosts/GvTGhostCharacterBase.h"
#include "Gameplay/Ghosts/GvTGhostModelData.h"
#include "Gameplay/Ghosts/GvTGhostTypeData.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "AIController.h"
#include "NavigationSystem.h"
#include "NavigationPath.h"
#include "World/Doors/GvTDoorActor.h"
#include "Gameplay/Interaction/GvTInteractable.h"
#include "Gameplay/Characters/Thieves/GvTThiefCharacter.h"
#include "Components/PrimitiveComponent.h"
#include "EngineUtils.h"

AGvTGhostCharacterBase::AGvTGhostCharacterBase()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;
	SetReplicateMovement(true);

	AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;
	bUseControllerRotationYaw = false;

	if (UCharacterMovementComponent* Move = GetCharacterMovement())
	{
		Move->bOrientRotationToMovement = true;
		Move->RotationRate = FRotator(0.f, 720.f, 0.f);
	}
}

void AGvTGhostCharacterBase::BeginPlay()
{
	Super::BeginPlay();

	IgnoreNearbyDoorCollisions(3000.f);

	if (UCapsuleComponent* Capsule = GetCapsuleComponent())
	{
		Capsule->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
		Capsule->SetCollisionObjectType(ECC_Pawn);
		Capsule->SetCollisionResponseToAllChannels(ECR_Ignore);
		Capsule->SetCollisionResponseToChannel(ECC_WorldStatic, ECR_Block);
		Capsule->SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Ignore);
		Capsule->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
		Capsule->SetCollisionResponseToChannel(ECC_PhysicsBody, ECR_Ignore);
		Capsule->SetCollisionResponseToChannel(ECC_Vehicle, ECR_Ignore);
		Capsule->SetCollisionResponseToChannel(ECC_Destructible, ECR_Ignore);
	}

	if (USkeletalMeshComponent* MeshComp = GetMesh())
	{
		MeshComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		MeshComp->SetGenerateOverlapEvents(false);
	}
}

void AGvTGhostCharacterBase::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (!HasAuthority())
	{
		return;
	}

	TickGhostChase(DeltaSeconds);
}

void AGvTGhostCharacterBase::InitializeGhost(
	FName InGhostId,
	const UGvTGhostModelData* InModelData,
	const UGvTGhostTypeData* InTypeData,
	const FGvTGhostRuntimeConfig& InRuntimeConfig)
{
	GhostId = InGhostId;
	GhostModelData = InModelData;
	GhostTypeData = InTypeData;
	RuntimeConfig = InRuntimeConfig;

	ApplyModelPresentation();
	ApplyRuntimeMovementConfig();

	UE_LOG(LogTemp, Log,
		TEXT("[GhostBase] Initialized GhostId=%s Model=%s Type=%s"),
		*GhostId.ToString(),
		*GetNameSafe(GhostModelData),
		*GetNameSafe(GhostTypeData));
}

void AGvTGhostCharacterBase::ApplyModelPresentation()
{
	if (!GhostModelData)
	{
		return;
	}

	if (UCapsuleComponent* Capsule = GetCapsuleComponent())
	{
		Capsule->SetCapsuleSize(GhostModelData->CapsuleRadius, GhostModelData->CapsuleHalfHeight);
	}

	if (USkeletalMeshComponent* MeshComp = GetMesh())
	{
		if (GhostModelData->SkeletalMesh)
		{
			MeshComp->SetSkeletalMesh(GhostModelData->SkeletalMesh);
		}

		if (GhostModelData->AnimClass)
		{
			MeshComp->SetAnimInstanceClass(GhostModelData->AnimClass);
		}

		if (GhostModelData->PhysicsAsset)
		{
			MeshComp->SetPhysicsAsset(GhostModelData->PhysicsAsset);
		}
	}
}

void AGvTGhostCharacterBase::ApplyRuntimeMovementConfig()
{
	if (UCharacterMovementComponent* Move = GetCharacterMovement())
	{
		Move->MaxWalkSpeed = RuntimeConfig.RunSpeed;
		Move->MaxAcceleration = RuntimeConfig.Acceleration;
		Move->BrakingDecelerationWalking = RuntimeConfig.BrakingDecel;
	}
}

void AGvTGhostCharacterBase::SetGhostPresenceActive(bool bActive)
{
	bGhostPresenceActive = bActive;

	SetActorHiddenInGame(!bActive);

	if (UCapsuleComponent* Capsule = GetCapsuleComponent())
	{
		Capsule->SetCollisionEnabled(bActive ? ECollisionEnabled::QueryOnly : ECollisionEnabled::NoCollision);
	}

	if (USkeletalMeshComponent* MeshComp = GetMesh())
	{
		MeshComp->SetVisibility(bActive, true);
		MeshComp->SetHiddenInGame(!bActive, true);
		MeshComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}

	if (UCharacterMovementComponent* Move = GetCharacterMovement())
	{
		if (bActive)
		{
			Move->SetMovementMode(MOVE_Walking);
		}
		else
		{
			Move->StopMovementImmediately();
			Move->DisableMovement();
		}
	}

	if (AAIController* AIC = Cast<AAIController>(GetController()))
	{
		AIC->StopMovement();
	}
}

void AGvTGhostCharacterBase::BeginGhostChase(APawn* Victim)
{
	if (!HasAuthority() || !Victim)
	{
		return;
	}

	SetGhostPresenceActive(true);

	TargetVictim = Victim;
	RepathTimer = 0.f;
	bLastChasePathWasPartial = false;

	if (!GetController())
	{
		SpawnDefaultController();
	}

	if (AAIController* AIC = Cast<AAIController>(GetController()))
	{
		AIC->StopMovement();
	}

	if (UCharacterMovementComponent* Move = GetCharacterMovement())
	{
		Move->StopMovementImmediately();
		Move->SetMovementMode(MOVE_Walking);
		Move->MaxWalkSpeed = RuntimeConfig.RunSpeed;
		Move->MaxAcceleration = RuntimeConfig.Acceleration;
		Move->BrakingDecelerationWalking = RuntimeConfig.BrakingDecel;
	}

	IgnoreNearbyDoorCollisions(3000.f);
	UpdateReachableChaseMove();
}


bool AGvTGhostCharacterBase::CanUseScareTag(FGameplayTag ScareTag) const
{
	if (!ScareTag.IsValid())
	{
		return false;
	}

	if (RuntimeConfig.AllowedScareTags.Num() == 0)
	{
		return true;
	}

	return RuntimeConfig.AllowedScareTags.HasTagExact(ScareTag);
}

bool AGvTGhostCharacterBase::FindReachablePointNearVictim(FVector& OutPoint, bool& bOutPathIsPartial) const
{
	bOutPathIsPartial = false;

	if (!TargetVictim || !GetWorld())
	{
		return false;
	}

	UNavigationSystemV1* NavSys = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());
	if (!NavSys)
	{
		return false;
	}

	const FVector VictimLoc = TargetVictim->GetActorLocation();

	FNavLocation NavLoc;
	if (!NavSys->ProjectPointToNavigation(
		VictimLoc,
		NavLoc,
		FVector(ChaseGoalProjectRadius, ChaseGoalProjectRadius, 250.f)))
	{
		return false;
	}

	UNavigationPath* Path = NavSys->FindPathToLocationSynchronously(
		GetWorld(),
		GetActorLocation(),
		NavLoc.Location,
		const_cast<AGvTGhostCharacterBase*>(this),
		nullptr);

	if (!Path || !Path->IsValid())
	{
		return false;
	}

	const TArray<FVector>& Points = Path->PathPoints;
	if (Points.Num() == 0)
	{
		return false;
	}

	bOutPathIsPartial = Path->IsPartial();

	if (!bOutPathIsPartial)
	{
		OutPoint = NavLoc.Location;
		return true;
	}

	OutPoint = Points.Last();
	return true;
}

bool AGvTGhostCharacterBase::UpdateReachableChaseMove()
{
	if (!TargetVictim)
	{
		return false;
	}

	AAIController* AIC = Cast<AAIController>(GetController());
	if (!AIC)
	{
		return false;
	}

	FVector ReachableGoal = FVector::ZeroVector;
	bool bPathIsPartial = false;
	if (!FindReachablePointNearVictim(ReachableGoal, bPathIsPartial))
	{
		bLastChasePathWasPartial = false;
		return false;
	}

	bLastChasePathWasPartial = bPathIsPartial;

	AIC->MoveToLocation(
		ReachableGoal,
		ChaseGoalAcceptRadius,
		true,
		true,
		false,
		true,
		nullptr,
		false);

	if (bLastChasePathWasPartial)
	{
		TryOpenBlockingDoor();
	}

	return true;
}

bool AGvTGhostCharacterBase::TryOpenBlockingDoor()
{
	if (!bAllowGhostToForceOpenDoors || !GetWorld() || !TargetVictim)
	{
		return false;
	}

	const float Now = GetWorld()->GetTimeSeconds();
	if ((Now - LastDoorAssistTime) < DoorOpenRetryCooldown)
	{
		return false;
	}

	const FVector GhostLoc = GetActorLocation();
	const FVector VictimLoc = TargetVictim->GetActorLocation();

	FVector TraceDir = GetVelocity();
	TraceDir.Z = 0.f;

	if (TraceDir.IsNearlyZero())
	{
		TraceDir = VictimLoc - GhostLoc;
		TraceDir.Z = 0.f;
	}

	TraceDir = TraceDir.GetSafeNormal();

	if (TraceDir.IsNearlyZero())
	{
		TraceDir = FVector(GetActorForwardVector().X, GetActorForwardVector().Y, 0.f).GetSafeNormal();
	}

	if (TraceDir.IsNearlyZero())
	{
		return false;
	}

	AGvTDoorActor* FoundDoor = nullptr;

	const FVector Start = GhostLoc + FVector(0.f, 0.f, 40.f);
	const FVector End = Start + (TraceDir * DoorAssistTraceDistance);

	FHitResult Hit;
	FCollisionShape Shape = FCollisionShape::MakeSphere(24.f);
	FCollisionQueryParams Params(SCENE_QUERY_STAT(GhostDoorAssist), false, this);
	Params.AddIgnoredActor(this);
	Params.AddIgnoredActor(TargetVictim);

	if (GetWorld()->SweepSingleByChannel(
		Hit,
		Start,
		End,
		FQuat::Identity,
		ECC_Visibility,
		Shape,
		Params))
	{
		FoundDoor = Cast<AGvTDoorActor>(Hit.GetActor());
		if (!FoundDoor && Hit.GetComponent())
		{
			FoundDoor = Cast<AGvTDoorActor>(Hit.GetComponent()->GetOwner());
		}
	}

	if (!FoundDoor)
	{
		const float SearchRadius = FMath::Max(140.f, DoorAssistTraceDistance);
		const FVector ToVictim2D = FVector(VictimLoc.X - GhostLoc.X, VictimLoc.Y - GhostLoc.Y, 0.f).GetSafeNormal();

		for (TActorIterator<AGvTDoorActor> It(GetWorld()); It; ++It)
		{
			AGvTDoorActor* Door = *It;
			if (!Door)
			{
				continue;
			}

			const FVector DoorLoc = Door->GetActorLocation();
			const FVector ToDoor = DoorLoc - GhostLoc;
			const FVector ToDoor2D(ToDoor.X, ToDoor.Y, 0.f);

			const float DistSq = ToDoor2D.SizeSquared();
			if (DistSq > FMath::Square(SearchRadius))
			{
				continue;
			}

			const FVector ToDoorDir = ToDoor2D.GetSafeNormal();
			const float FacingDot = FVector::DotProduct(ToVictim2D, ToDoorDir);
			if (FacingDot < 0.35f)
			{
				continue;
			}

			FoundDoor = Door;
			break;
		}
	}

	if (!FoundDoor)
	{
		return false;
	}

	LastDoorAssistTime = Now;

	if (FoundDoor->IsLocked())
	{
		FoundDoor->TryUnlock(nullptr, EDoorUnlockMethod::Force, true);
		return true;
	}

	if (!FoundDoor->IsOpenForScareSlam())
	{
		FoundDoor->CompleteInteract_Implementation(nullptr, EGvTInteractionVerb::Interact);
		return true;
	}

	return false;
}

void AGvTGhostCharacterBase::TickGhostChase(float DeltaSeconds)
{
	if (!TargetVictim)
	{
		return;
	}

	const float DistToVictim = FVector::Dist(GetActorLocation(), TargetVictim->GetActorLocation());
	const float EffectiveMaxChaseDistance = MaxChaseDistance * RuntimeConfig.MaxChaseDistanceMultiplier;
	if (EffectiveMaxChaseDistance > 0.f && DistToVictim > EffectiveMaxChaseDistance) 
	{
		Destroy();
		return;
	}

	RepathTimer += DeltaSeconds;
	if (RepathTimer >= RepathInterval)
	{
		RepathTimer = 0.f;

		const bool bMoveIssued = UpdateReachableChaseMove();
		if (!bMoveIssued)
		{
			if (AAIController* AIC = Cast<AAIController>(GetController()))
			{
				AIC->StopMovement();
			}

			TryOpenBlockingDoor();
		}
		else if (bLastChasePathWasPartial)
		{
			TryOpenBlockingDoor();
		}
	}

	TryCatchVictim();
}

void AGvTGhostCharacterBase::TryCatchVictim()
{
	const float EffectiveKillDistance = KillDistance * RuntimeConfig.KillDistanceMultiplier;

	if (!HasAuthority() || !TargetVictim || EffectiveKillDistance <= 0.f)
	{
		return;
	}

	if (FVector::Dist(GetActorLocation(), TargetVictim->GetActorLocation()) > EffectiveKillDistance)
	{
		return;
	}

	if (AGvTThiefCharacter* Thief = Cast<AGvTThiefCharacter>(TargetVictim))
	{
		Thief->Server_SetDead(this);
	}

	TargetVictim = nullptr;
	SetGhostPresenceActive(false);
	Destroy();
}

void AGvTGhostCharacterBase::IgnoreDoorCollisionForActor(AActor* DoorActor)
{
	if (!DoorActor)
	{
		return;
	}

	MoveIgnoreActorAdd(DoorActor);

	TArray<UPrimitiveComponent*> DoorPrims;
	DoorActor->GetComponents<UPrimitiveComponent>(DoorPrims);

	if (UCapsuleComponent* Capsule = GetCapsuleComponent())
	{
		for (UPrimitiveComponent* Prim : DoorPrims)
		{
			if (Prim)
			{
				Capsule->IgnoreComponentWhenMoving(Prim, true);
			}
		}
	}

	if (USkeletalMeshComponent* MeshComp = GetMesh())
	{
		for (UPrimitiveComponent* Prim : DoorPrims)
		{
			if (Prim)
			{
				MeshComp->IgnoreComponentWhenMoving(Prim, true);
			}
		}
	}
}

void AGvTGhostCharacterBase::IgnoreNearbyDoorCollisions(float Radius)
{
	if (!GetWorld() || Radius <= 0.f)
	{
		return;
	}

	const FVector MyLoc = GetActorLocation();

	for (TActorIterator<AGvTDoorActor> It(GetWorld()); It; ++It)
	{
		AGvTDoorActor* Door = *It;
		if (!Door)
		{
			continue;
		}

		if (FVector::DistSquared(MyLoc, Door->GetActorLocation()) <= FMath::Square(Radius))
		{
			IgnoreDoorCollisionForActor(Door);
		}
	}
}

bool AGvTGhostCharacterBase::ShouldForceOpenDoors_Implementation() const
{
	if (!bAllowGhostToForceOpenDoors || !TargetVictim)
	{
		return false;
	}

	return FMath::FRand() < RuntimeConfig.DoorOpenChance;
}

void AGvTGhostCharacterBase::BeginHauntGhostScare(AActor* Target, FGameplayTag ScareTag)
{
	// Intentionally empty
	// Specific ghosts (crawler, etc.) implement their own scare behavior
}