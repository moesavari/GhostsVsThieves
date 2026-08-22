#pragma once

#include "CoreMinimal.h"
#include "Systems/GvTHUDWidget.h"
#include "GvTGameStateBase.h"
#include "GameFramework/PlayerController.h"
#include "GvTPlayerController.generated.h"

class AGvTVanInventoryActor;
class UGvTMissionResultsWidget;
class UUserWidget;

UENUM(BlueprintType)
enum class EGvTOnboardingPrompt : uint8
{
    FirstItemCollected,
    ReturnToVan,
    MainObjectiveWarning,
    ObjectiveSecured,
    MedicinePanic
};

UCLASS()
class GHOSTSVSTHIEVES_API AGvTPlayerController : public APlayerController
{
    GENERATED_BODY()

public:
    virtual void BeginPlay() override;
    virtual void OnPossess(APawn* InPawn) override;
    virtual void Tick(float DeltaSeconds) override;
    virtual bool InputKey(FKey Key, EInputEvent EventType, float AmountDepressed, bool bGamepad) override;

    virtual void OnRep_PlayerState() override;
    virtual void PostSeamlessTravel() override;

    UFUNCTION(Client, Reliable)
    void Client_ShowScanResult(AActor* Item, const FText& ItemDisplayName, int32 ScannedValue);

    UFUNCTION(Client, Reliable)
    void Client_ShowExtractionMessage(const FText& Message, bool bSuccess);

    /** Generic owner-only HUD feedback entry point. Safe to call from server gameplay code. */
    UFUNCTION(Client, Reliable, BlueprintCallable, Category="GvT|UI")
    void Client_ShowHUDMessage(const FText& Message, bool bSuccess = false);

    /** Displays each contextual onboarding hint at most once for this player per mission. */
    UFUNCTION(Client, Reliable)
    void Client_ShowOnboardingPrompt(EGvTOnboardingPrompt Prompt);

    UFUNCTION(Client, Reliable)
    void Client_SetMissionInputLocked(bool bLocked);

	UFUNCTION(Client, Reliable)
	void Client_ShowMissionResults(const FGvTMissionResults& Results);

	/** Immediately leaves the gameplay world after the shared results delay. */
	UFUNCTION(Client, Reliable)
	void Client_ReturnToMainMenuAfterMission(FName ReturnMapName);

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

    /** Opens or closes the owner-only in-mission pause menu. */
    UFUNCTION(BlueprintCallable, Category="GvT|Pause Menu")
    void TogglePauseMenu();

    UFUNCTION(BlueprintCallable, Category="GvT|Pause Menu")
    void ResumeFromPauseMenu();

    UFUNCTION(BlueprintCallable, Category="GvT|Pause Menu")
    void ReturnToMainMenuFromPauseMenu();

    UFUNCTION(BlueprintCallable, Category="GvT|Pause Menu")
    void QuitGameFromPauseMenu();

    UFUNCTION(BlueprintPure, Category="GvT|Pause Menu")
    bool IsPauseMenuOpen() const { return bPauseMenuOpen; }

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

    /** Main objectives use a separate stencil so the outline material can render them red. */
    UPROPERTY(EditDefaultsOnly, Category = "Highlight")
    int32 MainObjectiveHighlightStencilValue = 2;

    UPROPERTY(EditDefaultsOnly, Category = "UI")
    TSubclassOf<UGvTHUDWidget> HUDWidgetClass;

    /** Panic level that teaches the player about medicine if they have not found it yet. */
    UPROPERTY(EditDefaultsOnly, Category = "UI|Onboarding", meta=(ClampMin="0.0", ClampMax="1.0"))
    float MedicinePanicHintThreshold01 = 0.60f;

	UPROPERTY(EditDefaultsOnly, Category = "UI")
	TSubclassOf<UGvTMissionResultsWidget> MissionResultsWidgetClass;

    UPROPERTY(EditDefaultsOnly, Category = "UI|Pause Menu")
    TSubclassOf<UUserWidget> PauseMenuWidgetClass;

    /** Short name or package path for the main menu map. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "UI|Pause Menu")
    FName MainMenuMapName = TEXT("L_MainMenu");

private:
	UFUNCTION(Server, Reliable)
	void Server_RequestTakeVanItem(AGvTVanInventoryActor* VanInventory, int32 StackIndex);

    void UpdateHighlight();
    void SetActorHighlighted(AActor* Actor, bool bHighlighted);
    void BindHUDToPlayerState();
    void BindHUDToGameState();
    void SetPauseMenuOpen(bool bOpen);
    void ShowOnboardingPromptLocal(EGvTOnboardingPrompt Prompt);
    void RestoreGameplayInput();

    TWeakObjectPtr<AActor> CurrentHighlightedActor;
    TObjectPtr<UGvTHUDWidget> HUDWidget = nullptr;
	TObjectPtr<UGvTMissionResultsWidget> MissionResultsWidget = nullptr;

    UPROPERTY(Transient)
    TObjectPtr<UUserWidget> PauseMenuWidget = nullptr;

    FTimerHandle TimerHandle_BindHUDRetry;
    FTimerHandle TimerHandle_BindGameStateRetry;
    bool bVanInventoryOpen = false;
    bool bPauseMenuOpen = false;
    bool bMissionInputLocked = false;
    int32 LastDisplayedPanicPercent = INDEX_NONE;
    TSet<EGvTOnboardingPrompt> ShownOnboardingPrompts;
};
