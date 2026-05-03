#include "Gameplay/Characters/Thieves/GvTThiefPerceptionComponent.h"
#include "Gameplay/Ghosts/Mirror/GvTMirrorActor.h"
#include "Gameplay/Scare/GvTScareTags.h"
#include "Systems/Audio/GvTAmbientAudioDirector.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"

namespace
{
	static AGvTAmbientAudioDirector* FindAmbientAudioDirector_ThiefPerception(const UObject* WorldContextObject)
	{
		if (!WorldContextObject)
		{
			return nullptr;
		}

		return Cast<AGvTAmbientAudioDirector>(
			UGameplayStatics::GetActorOfClass(WorldContextObject, AGvTAmbientAudioDirector::StaticClass()));
	}
}

UGvTThiefPerceptionComponent::UGvTThiefPerceptionComponent()
{
	SetIsReplicatedByDefault(true);
	PrimaryComponentTick.bCanEverTick = false;
}

void UGvTThiefPerceptionComponent::BeginPlay()
{
	Super::BeginPlay();

	APawn* OwnerPawn = Cast<APawn>(GetOwner());

	if (!OwnerPawn || !OwnerPawn->IsLocallyControlled())
	{
		return;
	}

	if (GetNetMode() == NM_DedicatedServer)
	{
		return;
	}

	if (!bEnableMirrorReflectSense)
	{
		return;
	}

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(
			MirrorReflectTimer,
			this,
			&UGvTThiefPerceptionComponent::TickMirrorReflectSense,
			MirrorReflectCheckInterval,
			true);
	}
}

void UGvTThiefPerceptionComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(MirrorReflectTimer);
	}

	Super::EndPlay(EndPlayReason);
}

bool UGvTThiefPerceptionComponent::IsServer() const
{
	return GetOwner() && GetOwner()->HasAuthority();
}

void UGvTThiefPerceptionComponent::Test_MirrorScare(float Intensity01, float LifeSeconds)
{
	APawn* OwnerPawn = Cast<APawn>(GetOwner());
	if (!OwnerPawn || !OwnerPawn->IsLocallyControlled())
	{
		return;
	}

	APlayerController* PC = Cast<APlayerController>(OwnerPawn->GetController());
	if (!PC || !GetWorld())
	{
		return;
	}

	FVector CamLoc;
	FRotator CamRot;
	PC->GetPlayerViewPoint(CamLoc, CamRot);

	const FVector Start = CamLoc;
	const FVector End = Start + (CamRot.Vector() * MirrorReflectTraceDistance);

	FCollisionQueryParams Params(SCENE_QUERY_STAT(GvT_ThiefPerception_MirrorTest), false, OwnerPawn);
	Params.AddIgnoredActor(OwnerPawn);

	FHitResult Hit;
	const bool bHit = GetWorld()->SweepSingleByChannel(
		Hit,
		Start,
		End,
		FQuat::Identity,
		ECC_Visibility,
		FCollisionShape::MakeSphere(MirrorReflectSphereRadius),
		Params);

	if (!bHit)
	{
		return;
	}

	AGvTMirrorActor* Mirror = Cast<AGvTMirrorActor>(Hit.GetActor());
	if (!Mirror)
	{
		return;
	}

	if (IsServer())
	{
		Mirror->TriggerScare(Intensity01, LifeSeconds);
	}
	else
	{
		Server_RequestMirrorActorScare(Mirror, Intensity01, LifeSeconds);
	}
}

void UGvTThiefPerceptionComponent::TickMirrorReflectSense()
{
	if (!bEnableMirrorReflectSense)
	{
		return;
	}

	APawn* OwnerPawn = Cast<APawn>(GetOwner());
	if (!OwnerPawn || !OwnerPawn->IsLocallyControlled())
	{
		return;
	}

	APlayerController* PC = Cast<APlayerController>(OwnerPawn->GetController());
	if (!PC || !GetWorld())
	{
		return;
	}

	FVector CamLoc;
	FRotator CamRot;
	PC->GetPlayerViewPoint(CamLoc, CamRot);

	const FVector CamFwd = CamRot.Vector();
	const FVector Start = CamLoc;
	const FVector End = Start + (CamFwd * MirrorReflectTraceDistance);

	FCollisionQueryParams Params(SCENE_QUERY_STAT(GvT_ThiefPerception_MirrorReflect), false, OwnerPawn);
	Params.AddIgnoredActor(OwnerPawn);

	FHitResult Hit;
	const bool bHit = GetWorld()->SweepSingleByChannel(
		Hit,
		Start,
		End,
		FQuat::Identity,
		ECC_Visibility,
		FCollisionShape::MakeSphere(MirrorReflectSphereRadius),
		Params);

	if (!bHit)
	{
		return;
	}

	AGvTMirrorActor* Mirror = Cast<AGvTMirrorActor>(Hit.GetActor());
	if (!Mirror)
	{
		return;
	}

	const FVector ToCamera = (CamLoc - Hit.ImpactPoint).GetSafeNormal();
	const float Dot = FVector::DotProduct(CamFwd, -ToCamera);

	if (Dot < MirrorReflectDotMin)
	{
		return;
	}

	const float Now = GetWorld()->TimeSeconds;

	if (Now < NextAllowedMirrorReflectTime)
	{
		return;
	}

	if (LastTriggeredMirror.IsValid() && LastTriggeredMirror.Get() == Mirror)
	{
		return;
	}

	const float Intensity01 = FMath::Clamp(
		(Dot - MirrorReflectDotMin) / FMath::Max(KINDA_SMALL_NUMBER, 1.f - MirrorReflectDotMin),
		0.f,
		1.f);

	Server_RequestMirrorActorScare(Mirror, Intensity01, MirrorReflectLifeSeconds);

	LastTriggeredMirror = Mirror;
	NextAllowedMirrorReflectTime = Now + MirrorReflectLifeSeconds + 0.25f;
}

void UGvTThiefPerceptionComponent::Server_RequestMirrorActorScare_Implementation(
	AGvTMirrorActor* Mirror,
	float Intensity01,
	float LifeSeconds)
{
	if (!Mirror)
	{
		return;
	}

	if (AGvTAmbientAudioDirector* AmbientDirector = FindAmbientAudioDirector_ThiefPerception(this))
	{
		AmbientDirector->HandleScareStarted(
			GvTScareTags::Mirror(),
			Mirror->GetActorLocation(),
			Intensity01);
	}

	Mirror->TriggerScare(Intensity01, LifeSeconds);
}