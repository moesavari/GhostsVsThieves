#include "GvTPlayerController.h"
#include "Blueprint/UserWidget.h"

void AGvTPlayerController::BeginPlay()
{
    Super::BeginPlay();

    // Only create UI for the local controller (important for multiplayer PIE)
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
