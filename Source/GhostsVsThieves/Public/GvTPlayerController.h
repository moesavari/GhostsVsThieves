#pragma once

#include "CoreMinimal.h"
#include "Systems/GvTHUDWidget.h"
#include "GameFramework/PlayerController.h"
#include "GvTPlayerController.generated.h"

UCLASS()
class GHOSTSVSTHIEVES_API AGvTPlayerController : public APlayerController
{
    GENERATED_BODY()

public:
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaSeconds) override;

    virtual void OnRep_PlayerState() override;

    UFUNCTION(Client, Reliable)
    void Client_ShowScanResult(AActor* Item, const FText& ItemDisplayName, int32 ScannedValue);

    UFUNCTION()
    void HandlePanicChanged(float NewPanic01);

    UFUNCTION()
    void HandleHauntPressureChanged(float NewPressure01);

protected:
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

    UPROPERTY(EditDefaultsOnly, Category = "UI")
    TSubclassOf<UGvTHUDWidget> HUDWidgetClass;

private:
    void UpdateHighlight();
    void SetActorHighlighted(AActor* Actor, bool bHighlighted);
    void BindHUDToPlayerState();

    TWeakObjectPtr<AActor> CurrentHighlightedActor;
    TObjectPtr<UGvTHUDWidget> HUDWidget = nullptr;
    FTimerHandle TimerHandle_BindHUDRetry;
};
