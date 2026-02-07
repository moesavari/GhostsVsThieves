#include "GvTPlayerController.h"
#include "Blueprint/UserWidget.h"
#include "World/Doors/GvTDoorActor.h"
#include "Engine/World.h"

void AGvTPlayerController::BeginPlay()
{
    Super::BeginPlay();

    if (!IsLocalController())
    {
        return;
    }

    if (!HUDWidgetClass)
    {
        UE_LOG(LogTemp, Warning, TEXT("GvTPlayerController: HUDWidgetClass is not set."));
        return;
    }

    HUDWidget = CreateWidget<UUserWidget>(this, HUDWidgetClass);
    if (HUDWidget)
    {
        HUDWidget->AddToViewport();
    }
}
static AGvTDoorActor* FindDoorLookAt(APlayerController* PC, float MaxDistance)
{
	if (!PC || !PC->GetWorld()) return nullptr;

	FVector Loc; FRotator Rot;
	PC->GetPlayerViewPoint(Loc, Rot);

	FHitResult Hit;
	const FVector End = Loc + Rot.Vector() * MaxDistance;

	FCollisionQueryParams Params(SCENE_QUERY_STAT(DoorDebug), false);
	if (APawn* P = PC->GetPawn()) Params.AddIgnoredActor(P);

	if (!PC->GetWorld()->LineTraceSingleByChannel(Hit, Loc, End, ECC_Visibility, Params))
		return nullptr;

	return Cast<AGvTDoorActor>(Hit.GetActor());
}

void AGvTPlayerController::DoorLock(float MaxDistance)
{
	if (!HasAuthority()) return;
	if (AGvTDoorActor* Door = FindDoorLookAt(this, MaxDistance))
	{
		Door->SetLocked(true);
	}
}

void AGvTPlayerController::DoorUnlock(float MaxDistance)
{
	if (!HasAuthority()) return;
	if (AGvTDoorActor* Door = FindDoorLookAt(this, MaxDistance))
	{
		Door->SetLocked(false);
	}
}

void AGvTPlayerController::DoorToggleLock(float MaxDistance)
{
	if (!HasAuthority()) return;
	if (AGvTDoorActor* Door = FindDoorLookAt(this, MaxDistance))
	{
		Door->SetLocked(!Door->IsLocked());
	}
}

void AGvTPlayerController::DoorForceUnlock(float MaxDistance)
{
	if (!HasAuthority()) return;
	if (AGvTDoorActor* Door = FindDoorLookAt(this, MaxDistance))
	{
		Door->TryUnlock(GetPawn(), EDoorUnlockMethod::Force, true);
	}
}