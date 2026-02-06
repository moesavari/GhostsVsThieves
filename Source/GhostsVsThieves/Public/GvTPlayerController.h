#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "GvTPlayerController.generated.h"

UCLASS()
class GHOSTSVSTHIEVES_API AGvTPlayerController : public APlayerController
{
    GENERATED_BODY()

public:
    virtual void BeginPlay() override;

protected:
    // Assign this in a BP child OR via defaults if you hard reference a widget class.
    UPROPERTY(EditDefaultsOnly, Category = "UI")
    TSubclassOf<UUserWidget> HUDWidgetClass;

    UPROPERTY()
    TObjectPtr<UUserWidget> HUDWidget;
};
