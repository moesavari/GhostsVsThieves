#pragma once

#include "CoreMinimal.h"
#include "Interfaces/OnlineSessionInterface.h"
#include "Interfaces/OnlineIdentityInterface.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Systems/Session/GvTLobbyTypes.h"
#include "GvTGameStateBase.h"
#include "GvTPlayerState.h"
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

	/** Unified Create Game entry point used for both solo and multiplayer lobbies. */
	UFUNCTION(BlueprintCallable, Category="GvT|Sessions")
	void CreateGame(const FString& ServerName, EGvTPlayableMap SelectedMap, EGvTSessionPrivacy Privacy = EGvTSessionPrivacy::Public, int32 MaxPlayers = 6, bool bLAN = false);

    UFUNCTION(BlueprintCallable, Category="GvT|Sessions")
    void FindSessions(int32 MaxResults = 50, bool bLAN = false);

    /** Finds a private lobby by its six-character room code and joins it automatically. */
    UFUNCTION(BlueprintCallable, Category="GvT|Sessions")
    void JoinPrivateSessionByCode(const FString& RoomCode, bool bLAN = false);

    /** Room code for the currently hosted/joined private lobby. Empty for public lobbies. */
    UFUNCTION(BlueprintPure, Category="GvT|Sessions|Lobby")
    FString GetHostedRoomCode() const { return CurrentRoomCode; }

    UFUNCTION(BlueprintPure, Category="GvT|Sessions|Lobby")
    EGvTSessionPrivacy GetCurrentSessionPrivacy() const { return CurrentSessionPrivacy; }

    /** Bypasses session discovery and connects to an IP/hostname, for WAN diagnostics. */
    UFUNCTION(BlueprintCallable, Category="GvT|Sessions")
    void JoinDirect(const FString& Address);

    UFUNCTION(BlueprintCallable, Category="GvT|Sessions")
    void JoinSessionByIndex(int32 ResultIndex);

	UFUNCTION(BlueprintPure, Category = "GvT|Sessions|Lobby")
	bool IsInLobbySession() const;

	UFUNCTION(BlueprintPure, Category = "GvT|Sessions|Lobby")
	bool IsListenServerHost() const;

	UFUNCTION(BlueprintPure, Category = "GvT|Sessions|Lobby")
	int32 GetConnectedPlayerCount() const;

	UFUNCTION(BlueprintPure, Category = "GvT|Sessions|Lobby")
	TArray<FGvTLobbyPlayerInfo> GetLobbyPlayers() const;

	UFUNCTION(BlueprintCallable, Category = "GvT|Sessions|Lobby")
	bool SetLocalLobbyReady(bool bReady);

	UFUNCTION(BlueprintPure, Category = "GvT|Sessions|Lobby")
	bool IsLocalLobbyReady() const;

	UFUNCTION(BlueprintCallable, Category = "GvT|Sessions|Lobby")
	bool SetHostedLobbyMap(EGvTPlayableMap SelectedMap);

	UFUNCTION(BlueprintPure, Category = "GvT|Sessions|Lobby")
	EGvTPlayableMap GetLobbySelectedMap() const;

	UFUNCTION(BlueprintPure, Category = "GvT|Sessions|Lobby")
	bool AreAllLobbyPlayersReady() const;

	UFUNCTION(BlueprintPure, Category = "GvT|Sessions|Lobby")
	bool CanHostStartMatch() const;

	UFUNCTION(BlueprintCallable, Category = "GvT|Sessions|Lobby")
	bool StartHostedMatch();

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
	FSoftObjectPath MainMenuMap = FSoftObjectPath(TEXT("/Game/Maps/L_MainMenu"));

    UPROPERTY(EditDefaultsOnly, Category = "GvT|Sessions")
    FSoftObjectPath LobbyMap = FSoftObjectPath(TEXT("/Game/Maps/L_MainMenu"));

	UPROPERTY(EditDefaultsOnly, Category = "GvT|Sessions|Maps")
	FSoftObjectPath MVPHouseMap = FSoftObjectPath(TEXT("/Game/Maps/L_House_MVP"));

	UPROPERTY(EditDefaultsOnly, Category = "GvT|Sessions|Maps")
	FSoftObjectPath ModernVillaMap = FSoftObjectPath(TEXT("/Game/Maps/L_House_RichNeighbourhood"));

	/** Kept at one so the host can test alone. Set to two in the subsystem defaults for public builds. */
	UPROPERTY(EditDefaultsOnly, Category = "GvT|Sessions|Lobby", meta = (ClampMin = "1", ClampMax = "6"))
	int32 MinimumPlayersToStart = 1;

private:
    enum class EPendingOperation : uint8
    {
        None,
        Host,
        Find,
        FindPrivate
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
    static FString GenerateRoomCode();
    static FString NormalizeRoomCode(const FString& RoomCode);
    static bool IsValidRoomCode(const FString& RoomCode);
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
	bool bPendingPrivateCodeSearch = false;
	EGvTSessionPrivacy PendingPrivacy = EGvTSessionPrivacy::Public;
	FString PendingRoomCode;
	EGvTSessionPrivacy CurrentSessionPrivacy = EGvTSessionPrivacy::Public;
	FString CurrentRoomCode;
	EGvTPlayableMap PendingHostedMap = EGvTPlayableMap::MVPHouse;
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
