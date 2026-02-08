#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "GvTPlayerController.generated.h"

class UUserWidget;

UCLASS()
class GHOSTSVSTHIEVES_API AGvTPlayerController : public APlayerController
{
    GENERATED_BODY()

public:
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaSeconds) override;

protected:
    UPROPERTY(EditDefaultsOnly, Category = "UI")
    TSubclassOf<UUserWidget> HUDWidgetClass;

    UPROPERTY()
    TObjectPtr<UUserWidget> HUDWidget;

    UFUNCTION(Exec)
    void DoorLock(float MaxDistance = 500.f);

    UFUNCTION(Exec)
    void DoorUnlock(float MaxDistance = 500.f);

    UFUNCTION(Exec)
    void DoorToggleLock(float MaxDistance = 500.f);

    UFUNCTION(Exec)
    void DoorForceUnlock(float MaxDistance = 500.f);

    UPROPERTY(EditDefaultsOnly, Category = "Highlight")
    float HighlightTraceDistance = 650.f;

    UPROPERTY(EditDefaultsOnly, Category = "Highlight")
    int32 HighlightStencilValue = 1;

    TWeakObjectPtr<AActor> CurrentHighlightedActor;

    void UpdateHighlight();
    void SetActorHighlighted(AActor* Actor, bool bHighlighted);

};
