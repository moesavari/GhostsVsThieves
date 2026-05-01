#include "Gameplay/Ghosts/Crawler/GvTCrawlerGhostCharacter.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Gameplay/Ghosts/Crawler/GvTCrawlerGhostAnimInstance.h"
#include "GameFramework/PlayerController.h"
#include "Components/SkeletalMeshComponent.h"
#include "Gameplay/Scare/GvTScareTags.h"
#include "Gameplay/Characters/Thieves/GvTThiefCharacter.h"

AGvTCrawlerGhostCharacter::AGvTCrawlerGhostCharacter()
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

void AGvTCrawlerGhostCharacter::BeginPlay()
{
	Super::BeginPlay();
}

void AGvTCrawlerGhostCharacter::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
}

void AGvTCrawlerGhostCharacter::BeginHauntGhostScare(AActor* Target, FGameplayTag ScareTag)
{
	UE_LOG(LogTemp, Warning, TEXT("[GhostScareTest] Crawler BeginHauntGhostScare target=%s tag=%s"),
		*GetNameSafe(Target),
		*ScareTag.ToString());

	if (!Target)
	{
		return;
	}

	ACharacter* TargetCharacter = Cast<ACharacter>(Target);
	if (!TargetCharacter)
	{
		return;
	}

	if (ScareTag.MatchesTagExact(GvTScareTags::GhostScare()))
	{
		BeginGhostScare_Local(TargetCharacter);
	}
}

void AGvTCrawlerGhostCharacter::BeginGhostScare_Local(ACharacter* TargetCharacter)
{
	if (!TargetCharacter)
	{
		return;
	}

	SetGhostPresenceActive(true);
	SetActorHiddenInGame(false);

	if (USkeletalMeshComponent* MeshComp = GetMesh())
	{
		MeshComp->SetVisibility(true, true);
		MeshComp->SetHiddenInGame(false, true);
	}

	const FVector PlayerLoc = TargetCharacter->GetActorLocation();

	FVector ViewLoc;
	FRotator ViewRot;

	if (AController* TargetController = TargetCharacter->GetController())
	{
		TargetController->GetPlayerViewPoint(ViewLoc, ViewRot);
	}
	else
	{
		ViewLoc = TargetCharacter->GetActorLocation();
		ViewRot = TargetCharacter->GetActorRotation();
	}

	const FVector ViewForward = ViewRot.Vector();

	const FVector SpawnLoc =
		ViewLoc +
		ViewForward * OverheadForwardOffset +
		FVector(0.f, 0.f, OverheadHeightOffset);

	SetActorLocation(SpawnLoc);

	const FVector ToPlayer = (ViewLoc - SpawnLoc).GetSafeNormal();

	FRotator LookRot = ToPlayer.Rotation();
	LookRot.Pitch += OverheadPitchOffset;

	SetActorRotation(LookRot);

	if (UCharacterMovementComponent* Move = GetCharacterMovement())
	{
		Move->StopMovementImmediately();
		Move->DisableMovement();
	}

	USkeletalMeshComponent* MeshComp = GetMesh();
	if (!IsValid(MeshComp))
	{
		UE_LOG(LogTemp, Error, TEXT("[GhostScare] Crawler ghost scare failed: %s has no Character Mesh."), *GetNameSafe(this));
		SetLifeSpan(OverheadDuration);
		return;
	}

	if (!MeshComp->GetSkeletalMeshAsset())
	{
		UE_LOG(LogTemp, Error, TEXT("[GhostScare] Crawler ghost scare failed: %s mesh component has no skeletal mesh assigned."), *GetNameSafe(this));
		SetLifeSpan(OverheadDuration);
		return;
	}

	if (!MeshComp->GetAnimInstance())
	{
		UE_LOG(LogTemp, Error, TEXT("[GhostScare] Crawler ghost scare failed: %s has mesh=%s but no AnimInstance. AnimClass=%s"),
			*GetNameSafe(this),
			*GetNameSafe(MeshComp->GetSkeletalMeshAsset()),
			*GetNameSafe(MeshComp->GetAnimClass()));
		SetLifeSpan(OverheadDuration);
		return;
	}

	UGvTCrawlerGhostAnimInstance* Anim = Cast<UGvTCrawlerGhostAnimInstance>(MeshComp->GetAnimInstance());
	if (!Anim)
	{
		UE_LOG(LogTemp, Error, TEXT("[GhostScare] Crawler ghost scare failed: wrong anim instance. Got=%s Expected=UGvTCrawlerGhostAnimInstance"),
			*GetNameSafe(MeshComp->GetAnimInstance()));
		SetLifeSpan(OverheadDuration);
		return;
	}

	Anim->PlayOverheadScarePose(OverheadDuration);

	//if (APawn* TargetPawn = Cast<APawn>(TargetCharacter))
	//{
	//	if (TargetPawn->IsLocallyControlled())
	//	{
	//		if (APlayerController* PC = Cast<APlayerController>(TargetPawn->GetController()))
	//		{
	//			PC->SetIgnoreMoveInput(true);
	//			PC->SetIgnoreLookInput(true);

	//			FTimerHandle RestoreInputTimer;
	//			GetWorldTimerManager().SetTimer(
	//				RestoreInputTimer,
	//				FTimerDelegate::CreateWeakLambda(this, [PC]()
	//					{
	//						if (PC)
	//						{
	//							PC->SetIgnoreMoveInput(false);
	//							PC->SetIgnoreLookInput(false);
	//						}
	//					}),
	//				OverheadDuration,
	//				false);
	//		}
	//	}
	//}

	if (AGvTThiefCharacter* Thief = Cast<AGvTThiefCharacter>(TargetCharacter))
	{
		if (Thief->IsLocallyControlled())
		{
			Thief->ApplyScareStun(OverheadDuration);
		}
	}

	SetLifeSpan(OverheadDuration);
}