#include "World/Extraction/GvTExtractionDepartureActor.h"
#include "Components/BoxComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Gameplay/Characters/Thieves/GvTThiefCharacter.h"
#include "GvTGameModeBase.h"
#include "GvTPlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Net/UnrealNetwork.h"
#include "Sound/SoundBase.h"

AGvTExtractionDepartureActor::AGvTExtractionDepartureActor()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	InteractionBounds = CreateDefaultSubobject<UBoxComponent>(TEXT("InteractionBounds"));
	InteractionBounds->SetupAttachment(SceneRoot);
	InteractionBounds->SetBoxExtent(FVector(100.f, 100.f, 75.f));
	InteractionBounds->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	InteractionBounds->SetCollisionObjectType(ECC_WorldDynamic);
	InteractionBounds->SetCollisionResponseToAllChannels(ECR_Ignore);
	InteractionBounds->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);

	PlaceholderMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PlaceholderMesh"));
	PlaceholderMesh->SetupAttachment(InteractionBounds);
	PlaceholderMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

void AGvTExtractionDepartureActor::BeginPlay()
{
	Super::BeginPlay();
	OnRep_VanDoorsOpen();
}

void AGvTExtractionDepartureActor::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AGvTExtractionDepartureActor, bVanDoorsOpen);
}

void AGvTExtractionDepartureActor::GetInteractionSpec_Implementation(APawn* InstigatorPawn, EGvTInteractionVerb Verb, FGvTInteractionSpec& OutSpec) const
{
	OutSpec = FGvTInteractionSpec{};
	if (Verb != EGvTInteractionVerb::Interact) return;
	OutSpec.CastTime = DepartureCastTime;
	OutSpec.bLockMovement = true;
	OutSpec.bLockLook = false;
	OutSpec.bCancelable = true;
	OutSpec.bEmitNoiseOnCancel = false;
	OutSpec.LoopSfx = DepartureLoopSfx;
	OutSpec.EndSfx = DepartureEndSfx;
	OutSpec.CancelSfx = DepartureCancelSfx;
}

bool AGvTExtractionDepartureActor::CanInteract_Implementation(APawn* InstigatorPawn, EGvTInteractionVerb Verb) const
{
	const AGvTThiefCharacter* Thief = Cast<AGvTThiefCharacter>(InstigatorPawn);
	return Verb == EGvTInteractionVerb::Interact && IsValid(Thief) && !Thief->IsDead();
}

void AGvTExtractionDepartureActor::BeginInteract_Implementation(APawn* InstigatorPawn, EGvTInteractionVerb Verb)
{
}

void AGvTExtractionDepartureActor::CompleteInteract_Implementation(APawn* InstigatorPawn, EGvTInteractionVerb Verb)
{
	if (!HasAuthority() || Verb != EGvTInteractionVerb::Interact) return;
	if (AGvTGameModeBase* GM = GetWorld()->GetAuthGameMode<AGvTGameModeBase>())
	{
		const EGvTExtractionRequestResult Result = GM->RequestExtraction(InstigatorPawn);
		if (AGvTPlayerController* PC = InstigatorPawn ? Cast<AGvTPlayerController>(InstigatorPawn->GetController()) : nullptr)
		{
			switch (Result)
			{
				case EGvTExtractionRequestResult::CarryingStolenLoot:
				case EGvTExtractionRequestResult::MainObjectiveMissing:
				case EGvTExtractionRequestResult::Invalid:
					PC->Client_ShowExtractionMessage(GM->BuildExtractionStatusMessage(Result), false);
					break;
				case EGvTExtractionRequestResult::WaitingForPlayers:
					PC->Client_ShowExtractionMessage(GM->BuildExtractionStatusMessage(Result), true);
					break;
				case EGvTExtractionRequestResult::DepartureStarted:
					SetVanDoorsOpenAuthority(false);
					if (EngineStartSounds.Num() > 0)
					{
						Multicast_PlayEngineStart(EngineStartSounds[FMath::RandRange(0, EngineStartSounds.Num() - 1)]);
					}
					PC->Client_ShowExtractionMessage(GM->BuildExtractionStatusMessage(Result), true);
					break;
				default:
					break;
			}
		}
	}
}

void AGvTExtractionDepartureActor::SetVanDoorsOpenAuthority(bool bOpen)
{
	if (!HasAuthority() || bVanDoorsOpen == bOpen)
	{
		return;
	}

	bVanDoorsOpen = bOpen;
	OnRep_VanDoorsOpen();
	ForceNetUpdate();
}

void AGvTExtractionDepartureActor::OnRep_VanDoorsOpen()
{
	BP_ApplyVanDoorState(bVanDoorsOpen);
}

void AGvTExtractionDepartureActor::Multicast_PlayEngineStart_Implementation(USoundBase* Sound)
{
	if (Sound)
	{
		UGameplayStatics::PlaySoundAtLocation(this, Sound, GetActorLocation());
	}
}

void AGvTExtractionDepartureActor::CancelInteract_Implementation(APawn* InstigatorPawn, EGvTInteractionVerb Verb, EGvTInteractionCancelReason Reason)
{
}
