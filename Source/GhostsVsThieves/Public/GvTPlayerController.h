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
    //void BindHUDToPlayerState();

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

    TWeakObjectPtr<AActor> CurrentHighlightedActor;

    void UpdateHighlight();
    void SetActorHighlighted(AActor* Actor, bool bHighlighted);

    UPROPERTY()
    TObjectPtr<UGvTHUDWidget> HUDWidget = nullptr;

    UPROPERTY(EditDefaultsOnly, Category = "UI")
    TSubclassOf<UGvTHUDWidget> HUDWidgetClass;

    FTimerHandle TimerHandle_BindHUDRetry;
    UFUNCTION()
    void BindHUDToPlayerState();
};
