#include "Systems/Director/GvTDirectorSubsystem.h"

#include "Gameplay/Scare/UGvTScareComponent.h"
#include "Gameplay/Scare/GvTScareTags.h"
#include "Gameplay/Characters/Thieves/GvTThiefCharacter.h"

#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"

static const TCHAR* GvTNetModeToString(ENetMode NetMode)
{
	switch (NetMode)
	{
	case NM_Standalone:      return TEXT("Standalone");
	case NM_DedicatedServer: return TEXT("DedicatedServer");
	case NM_ListenServer:    return TEXT("ListenServer");
	case NM_Client:          return TEXT("Client");
	default:                 return TEXT("Unknown");
	}
}

void UGvTDirectorSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	UE_LOG(LogTemp, Log, TEXT("GvT Director Subsystem Initialized"));

	if (bEnableAutoHaunts)
	{
		StartDirector();
	}
}

void UGvTDirectorSubsystem::Deinitialize()
{
	StopDirector();

	Super::Deinitialize();
	UE_LOG(LogTemp, Log, TEXT("GvT Director Subsystem Deinitialized"));
}

void UGvTDirectorSubsystem::StartDirector()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	if (World->GetNetMode() == NM_Client)
	{
		UE_LOG(LogTemp, Log, TEXT("[Director] Skipping auto-haunt loop on client world."));
		return;
	}

	if (TimerHandle_DirectorTick.IsValid())
	{
		return;
	}

	World->GetTimerManager().SetTimer(
		TimerHandle_DirectorTick,
		this,
		&UGvTDirectorSubsystem::TickDirector,
		DirectorTickInterval,
		true
	);

	UE_LOG(LogTemp, Log, TEXT("[Director] Auto-haunt loop started. Interval=%.2f"), DirectorTickInterval);
}

void UGvTDirectorSubsystem::StopDirector()
{
	if (!GetWorld())
	{
		return;
	}

	if (TimerHandle_DirectorTick.IsValid())
	{
		GetWorld()->GetTimerManager().ClearTimer(TimerHandle_DirectorTick);
		TimerHandle_DirectorTick.Invalidate();

		UE_LOG(LogTemp, Log, TEXT("[Director] Auto-haunt loop stopped."));
	}
}

void UGvTDirectorSubsystem::TickDirector()
{
	if (!bEnableAutoHaunts)
	{
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	// Server / listen-server authority only.
	if (World->GetNetMode() == NM_Client)
	{
		return;
	}

	const float Now = World->GetTimeSeconds();
	if ((Now - LastGlobalHauntTime) < GlobalHauntCooldown)
	{
		return;
	}

	TryDispatchAutoScare();
}

bool UGvTDirectorSubsystem::TryDispatchAutoScare()
{
	AActor* Target = ChooseBestTarget();
	if (!IsValid(Target))
	{
		return false;
	}

	UGvTScareComponent* ScareComp = Target->FindComponentByClass<UGvTScareComponent>();
	if (!ScareComp)
	{
		return false;
	}

	// Simple first-pass autonomous routing:
	// Randomly pick either a mirror scare or overhead/drop scare.
	const float Roll = FMath::FRand();

	FGvTScareEvent Event;
	Event.TargetActor = Target;
	Event.Intensity01 = 1.0f;
	Event.Duration = 1.5f;
	Event.bAffectsPanic = true;

	if (Roll < 0.50f)
	{
		Event.ScareTag = GvTScareTags::Mirror();
		Event.bTriggerLocalFlicker = true;
		Event.PanicAmount = 12.f;
	}
	else
	{
		Event.ScareTag = GvTScareTags::CrawlerOverhead();
		Event.bTriggerLocalFlicker = false;
		Event.bTriggerGroupFlicker = false;
		Event.PanicAmount = 10.f;
	}

	const bool bDispatched = DispatchScareEvent(Event);
	if (bDispatched && GetWorld())
	{
		LastGlobalHauntTime = GetWorld()->GetTimeSeconds();
	}

	return bDispatched;
}

AActor* UGvTDirectorSubsystem::ChooseBestTarget() const
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return nullptr;
	}

	TArray<AActor*> Thieves;
	UGameplayStatics::GetAllActorsOfClass(World, AGvTThiefCharacter::StaticClass(), Thieves);

	TArray<AActor*> ValidTargets;
	ValidTargets.Reserve(Thieves.Num());

	for (AActor* Actor : Thieves)
	{
		if (!IsValid(Actor))
		{
			continue;
		}

		APawn* Pawn = Cast<APawn>(Actor);
		if (!Pawn)
		{
			continue;
		}

		// Only target living/active player pawns that actually have a scare component.
		if (UGvTScareComponent* ScareComp = Pawn->FindComponentByClass<UGvTScareComponent>())
		{
			ValidTargets.Add(Pawn);
		}
	}

	if (ValidTargets.Num() == 0)
	{
		return nullptr;
	}

	const int32 Index = FMath::RandRange(0, ValidTargets.Num() - 1);
	return ValidTargets[Index];
}

bool UGvTDirectorSubsystem::DispatchScareEvent(const FGvTScareEvent& Event)
{
	AActor* Target = Event.TargetActor;
	if (!IsValid(Target))
	{
		UE_LOG(LogTemp, Warning, TEXT("[Director] Dispatch failed: TargetActor is null."));
		return false;
	}

	UGvTScareComponent* ScareComp = Target->FindComponentByClass<UGvTScareComponent>();
	if (!ScareComp)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Director] Dispatch failed: %s has no GvTScareComponent."), *GetNameSafe(Target));
		return false;
	}

	TriggerRequestedFlicker(Event, ScareComp);

	if (Event.bAffectsPanic && Event.PanicAmount > 0.f)
	{
		ScareComp->AddPanic(Event.PanicAmount);
	}

	if (Event.ScareTag.MatchesTagExact(GvTScareTags::Mirror()))
	{
		UE_LOG(LogTemp, Warning, TEXT("[Director] Dispatch Mirror to %s"), *GetNameSafe(Target));
		ScareComp->RequestMirrorScare(Event.Intensity01, Event.Duration > 0.f ? Event.Duration : 1.5f);
		return true;
	}

	if (Event.ScareTag.MatchesTagExact(GvTScareTags::CrawlerChase()))
	{
		UE_LOG(LogTemp, Warning, TEXT("[Director] Dispatch Crawler Chase to %s"), *GetNameSafe(Target));
		ScareComp->RequestCrawlerChaseFromEvent(Target);
		return true;
	}

	if (Event.ScareTag.MatchesTagExact(GvTScareTags::CrawlerOverhead()))
	{
		UE_LOG(LogTemp, Warning,
			TEXT("[Director] Dispatch CrawlerOverhead to %s NetMode=%s LocalRole=%d RemoteRole=%d"),
			*GetNameSafe(Target),
			GvTNetModeToString(Target->GetNetMode()),
			(int32)Target->GetLocalRole(),
			(int32)Target->GetRemoteRole());

		ScareComp->RequestCrawlerOverheadFromEvent(Event);
		return true;
	}

	UE_LOG(LogTemp, Warning, TEXT("[Director] Dispatch failed: Unknown scare tag %s"), *Event.ScareTag.ToString());
	return false;
}

bool UGvTDirectorSubsystem::TriggerRequestedFlicker(const FGvTScareEvent& Event, UGvTScareComponent* TargetScareComp)
{
	if (!TargetScareComp)
	{
		return false;
	}

	if (Event.bTriggerGroupFlicker)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Director] Trigger GROUP flicker"));
		TargetScareComp->Debug_RequestGroupHouseLightFlicker(
			FMath::Clamp(Event.Intensity01, 0.f, 1.f),
			Event.Duration > 0.f ? Event.Duration : 1.5f
		);
		return true;
	}

	if (Event.bTriggerLocalFlicker)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Director] Trigger LOCAL flicker"));
		TargetScareComp->Debug_RequestLocalHouseLightFlicker(
			FMath::Clamp(Event.Intensity01, 0.f, 1.f),
			Event.Duration > 0.f ? Event.Duration : 1.5f
		);
		return true;
	}

	return false;
}