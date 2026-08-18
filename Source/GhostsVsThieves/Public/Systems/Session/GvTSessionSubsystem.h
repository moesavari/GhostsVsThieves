#pragma once

#include "CoreMinimal.h"
#include "Interfaces/OnlineSessionInterface.h"
#include "Interfaces/OnlineIdentityInterface.h"
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
    void HostSession(const FString& ServerName, int32 PublicConnections = 6, bool bLAN = false);

    UFUNCTION(BlueprintCallable, Category="GvT|Sessions")
    void FindSessions(int32 MaxResults = 50, bool bLAN = false);

    /** Bypasses session discovery and connects to an IP/hostname, for WAN diagnostics. */
    UFUNCTION(BlueprintCallable, Category="GvT|Sessions")
    void JoinDirect(const FString& Address);

    UFUNCTION(BlueprintCallable, Category="GvT|Sessions")
    void JoinSessionByIndex(int32 ResultIndex);

    UFUNCTION(BlueprintCallable, Category="GvT|Sessions")
    void LeaveSession();

    /** Destroys the local online session before returning to a menu map. */
    UFUNCTION(BlueprintCallable, Category="GvT|Sessions")
    void LeaveSessionAndReturnToMenu(FName MainMenuMapName);

	/** Starts session cleanup but travels immediately instead of waiting on the online callback. */
	UFUNCTION(BlueprintCallable, Category="GvT|Sessions")
	void LeaveSessionAndReturnToMenuImmediately(FName MainMenuMapName);

    UFUNCTION(BlueprintPure, Category="GvT|Sessions")
    bool IsBusy() const { return bOperationInProgress; }

    UPROPERTY(BlueprintAssignable, Category="GvT|Sessions")
    FGvTSessionStatusChanged OnSessionStatusChanged;

    UPROPERTY(BlueprintAssignable, Category="GvT|Sessions")
    FGvTSessionSearchCompleted OnSessionSearchCompleted;

protected:
    UPROPERTY(EditDefaultsOnly, Category="GvT|Sessions")
    FSoftObjectPath LobbyMap = FSoftObjectPath(TEXT("/Game/Maps/L_House_MVP"));

private:
    enum class EPendingOperation : uint8
    {
        None,
        Host,
        Find
    };

    IOnlineSessionPtr GetSessionInterface() const;
    IOnlineIdentityPtr GetIdentityInterface() const;
    bool IsLocalUserLoggedIn() const;
    bool BeginLoginForPendingOperation();
    void ResumePendingOperation();
    void BroadcastStatus(const FText& Message, bool bSuccess);
    void PrepareHostSessionNow();
    void CreateSessionNow();
    void FindSessionsNow();
    void HandleLoginComplete(int32 LocalUserNum, bool bSuccess, const FUniqueNetId& UserId, const FString& Error);
    void HandleCreateSessionComplete(FName SessionName, bool bSuccess);
    void HandleFindSessionsComplete(bool bSuccess);
    void HandleJoinSessionComplete(FName SessionName, EOnJoinSessionCompleteResult::Type Result);
    void HandleDestroySessionComplete(FName SessionName, bool bSuccess);
    void TravelToPendingReturnMap();

    TSharedPtr<FOnlineSessionSettings> PendingSessionSettings;
    TSharedPtr<FOnlineSessionSearch> SessionSearch;
    FString PendingServerName;
    int32 PendingPublicConnections = 6;
    int32 PendingMaxSearchResults = 50;
    bool bPendingLAN = true;
    bool bCreateAfterDestroy = false;
    bool bOperationInProgress = false;
    EPendingOperation PendingOperation = EPendingOperation::None;
    FName PendingReturnMapName = NAME_None;

    FDelegateHandle CreateDelegateHandle;
    FDelegateHandle FindDelegateHandle;
    FDelegateHandle JoinDelegateHandle;
    FDelegateHandle DestroyDelegateHandle;
    FDelegateHandle LoginDelegateHandle;
};
