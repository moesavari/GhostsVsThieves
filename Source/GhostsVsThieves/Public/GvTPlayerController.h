#pragma once

#include "CoreMinimal.h"
#include "Systems/GvTHUDWidget.h"
#include "GameFramework/PlayerController.h"
#include "GvTPlayerController.generated.h"

class AGvTVanInventoryActor;

UCLASS()
class GHOSTSVSTHIEVES_API AGvTPlayerController : public APlayerController
{
    GENERATED_BODY()

public:
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaSeconds) override;
    virtual bool InputKey(FKey Key, EInputEvent EventType, float AmountDepressed, bool bGamepad) override;

    virtual void OnRep_PlayerState() override;

    UFUNCTION(Client, Reliable)
    void Client_ShowScanResult(AActor* Item, const FText& ItemDisplayName, int32 ScannedValue);

    UFUNCTION(Client, Reliable)
    void Client_ShowExtractionMessage(const FText& Message, bool bSuccess);

    /** Generic owner-only HUD feedback entry point. Safe to call from server gameplay code. */
    UFUNCTION(Client, Reliable, BlueprintCallable, Category="GvT|UI")
    void Client_ShowHUDMessage(const FText& Message, bool bSuccess = false);

    UFUNCTION(Client, Reliable)
    void Client_SetMissionInputLocked(bool bLocked);

    UFUNCTION(Client, Reliable)
    void Client_OpenVanInventory(AGvTVanInventoryActor* VanInventory);

    /** Called by a van slot widget. The server performs all transfer validation. */
    UFUNCTION(BlueprintCallable, Category="GvT|Van Inventory")
    void RequestTakeVanItem(AGvTVanInventoryActor* VanInventory, int32 StackIndex);

    /** Locks/unlocks only this local player's movement and camera while the van menu is open. */
    UFUNCTION(BlueprintCallable, Category="GvT|Van Inventory")
    void SetVanInventoryOpen(bool bOpen);

    UFUNCTION(BlueprintCallable, Category="GvT|Van Inventory")
    void CloseVanInventory();

    UFUNCTION(BlueprintImplementableEvent, Category="GvT|Mission")
    void OnExtractionMessage(const FText& Message, bool bSuccess);

    UFUNCTION(BlueprintImplementableEvent, Category="GvT|Van Inventory")
    void OnOpenVanInventory(AGvTVanInventoryActor* VanInventory);

    UFUNCTION(BlueprintImplementableEvent, Category="GvT|Van Inventory")
    void OnCloseVanInventory();

    UFUNCTION()
    void HandlePanicChanged(float NewPanic01);

    UFUNCTION()
    void HandleHauntPressureChanged(float NewPressure01);

    UFUNCTION()
    void HandleTeamSecuredLootChanged(int32 NewTeamSecuredLoot);

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
	UFUNCTION(Server, Reliable)
	void Server_RequestTakeVanItem(AGvTVanInventoryActor* VanInventory, int32 StackIndex);

    void UpdateHighlight();
    void SetActorHighlighted(AActor* Actor, bool bHighlighted);
    void BindHUDToPlayerState();
    void BindHUDToGameState();

    TWeakObjectPtr<AActor> CurrentHighlightedActor;
    TObjectPtr<UGvTHUDWidget> HUDWidget = nullptr;
    FTimerHandle TimerHandle_BindHUDRetry;
    FTimerHandle TimerHandle_BindGameStateRetry;
    bool bVanInventoryOpen = false;
    int32 LastDisplayedPanicPercent = INDEX_NONE;
};
