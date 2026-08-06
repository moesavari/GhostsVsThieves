#pragma once

#include "CoreMinimal.h"
#include "Interfaces/OnlineSessionInterface.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "GvTSessionSubsystem.generated.h"

USTRUCT(BlueprintType)
struct FGvTSessionSearchResult
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category="GvT|Sessions")
    int32 ResultIndex = INDEX_NONE;

    UPROPERTY(BlueprintReadOnly, Category="GvT|Sessions")
    FString ServerName;

    UPROPERTY(BlueprintReadOnly, Category="GvT|Sessions")
    int32 CurrentPlayers = 0;

    UPROPERTY(BlueprintReadOnly, Category="GvT|Sessions")
    int32 MaxPlayers = 0;

    UPROPERTY(BlueprintReadOnly, Category="GvT|Sessions")
    int32 PingMs = 0;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FGvTSessionStatusChanged, const FText&, Message, bool, bSuccess);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FGvTSessionSearchCompleted, const TArray<FGvTSessionSearchResult>&, Results);

UCLASS()
class GHOSTSVSTHIEVES_API UGvTSessionSubsystem : public UGameInstanceSubsystem
{
    GENERATED_BODY()

public:
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;

    UFUNCTION(BlueprintCallable, Category="GvT|Sessions")
    void HostSession(const FString& ServerName, int32 PublicConnections = 6, bool bLAN = true);

    UFUNCTION(BlueprintCallable, Category="GvT|Sessions")
    void FindSessions(int32 MaxResults = 50, bool bLAN = true);

    UFUNCTION(BlueprintCallable, Category="GvT|Sessions")
    void JoinSessionByIndex(int32 ResultIndex);

    UFUNCTION(BlueprintCallable, Category="GvT|Sessions")
    void LeaveSession();

    UFUNCTION(BlueprintPure, Category="GvT|Sessions")
    bool IsBusy() const { return bOperationInProgress; }

    UPROPERTY(BlueprintAssignable, Category="GvT|Sessions")
    FGvTSessionStatusChanged OnSessionStatusChanged;

    UPROPERTY(BlueprintAssignable, Category="GvT|Sessions")
    FGvTSessionSearchCompleted OnSessionSearchCompleted;

protected:
    UPROPERTY(EditDefaultsOnly, Category="GvT|Sessions")
    FSoftObjectPath LobbyMap = FSoftObjectPath(TEXT("/Game/Game/Maps/L_Lobby"));

private:
    IOnlineSessionPtr GetSessionInterface() const;
    void BroadcastStatus(const FText& Message, bool bSuccess);
    void CreateSessionNow();
    void HandleCreateSessionComplete(FName SessionName, bool bSuccess);
    void HandleFindSessionsComplete(bool bSuccess);
    void HandleJoinSessionComplete(FName SessionName, EOnJoinSessionCompleteResult::Type Result);
    void HandleDestroySessionComplete(FName SessionName, bool bSuccess);

    TSharedPtr<FOnlineSessionSettings> PendingSessionSettings;
    TSharedPtr<FOnlineSessionSearch> SessionSearch;
    FString PendingServerName;
    int32 PendingPublicConnections = 6;
    bool bPendingLAN = true;
    bool bCreateAfterDestroy = false;
    bool bOperationInProgress = false;

    FDelegateHandle CreateDelegateHandle;
    FDelegateHandle FindDelegateHandle;
    FDelegateHandle JoinDelegateHandle;
    FDelegateHandle DestroyDelegateHandle;
};
