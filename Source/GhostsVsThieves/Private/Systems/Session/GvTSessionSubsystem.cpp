#include "Systems/Session/GvTSessionSubsystem.h"

#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "GameFramework/PlayerController.h"
#include "Online/OnlineSessionNames.h"
#include "OnlineSubsystem.h"
#include "OnlineSubsystemUtils.h"
#include "OnlineSessionSettings.h"
#include "Interfaces/OnlineIdentityInterface.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/NetworkVersion.h"

namespace GvTSessionKeys
{
    static const FName ServerName(TEXT("GvTServerName"));
    static const FName BucketIdKey(TEXT("BucketId"));
    static const FString BucketId(TEXT("HauntedHeistsWAN"));
}

void UGvTSessionSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
}

void UGvTSessionSubsystem::Deinitialize()
{
    if (IOnlineIdentityPtr Identity = GetIdentityInterface())
    {
        Identity->ClearOnLoginCompleteDelegate_Handle(0, LoginDelegateHandle);
    }

    if (IOnlineSessionPtr Sessions = GetSessionInterface())
    {
        Sessions->ClearOnCreateSessionCompleteDelegate_Handle(CreateDelegateHandle);
        Sessions->ClearOnFindSessionsCompleteDelegate_Handle(FindDelegateHandle);
        Sessions->ClearOnJoinSessionCompleteDelegate_Handle(JoinDelegateHandle);
        Sessions->ClearOnDestroySessionCompleteDelegate_Handle(DestroyDelegateHandle);
    }

    Super::Deinitialize();
}

IOnlineSessionPtr UGvTSessionSubsystem::GetSessionInterface() const
{
    const UWorld* World = GetWorld();
    IOnlineSubsystem* OnlineSubsystem = World ? Online::GetSubsystem(World) : nullptr;
    return OnlineSubsystem ? OnlineSubsystem->GetSessionInterface() : nullptr;
}

IOnlineIdentityPtr UGvTSessionSubsystem::GetIdentityInterface() const
{
    const UWorld* World = GetWorld();
    IOnlineSubsystem* OnlineSubsystem = World ? Online::GetSubsystem(World) : nullptr;
    return OnlineSubsystem ? OnlineSubsystem->GetIdentityInterface() : nullptr;
}

bool UGvTSessionSubsystem::IsLocalUserLoggedIn() const
{
    const IOnlineIdentityPtr Identity = GetIdentityInterface();
    return Identity.IsValid() && Identity->GetLoginStatus(0) == ELoginStatus::LoggedIn;
}

bool UGvTSessionSubsystem::BeginLoginForPendingOperation()
{
    IOnlineIdentityPtr Identity = GetIdentityInterface();
    if (!Identity.IsValid())
    {
        bOperationInProgress = false;
        PendingOperation = EPendingOperation::None;
        BroadcastStatus(NSLOCTEXT("GvTSessions", "NoIdentityProvider", "The online identity provider is unavailable."), false);
        return false;
    }

    LoginDelegateHandle = Identity->AddOnLoginCompleteDelegate_Handle(0, FOnLoginCompleteDelegate::CreateUObject(this, &ThisClass::HandleLoginComplete));

    FOnlineAccountCredentials Credentials;
    Credentials.Type = TEXT("AccountPortal");
    Credentials.Id.Reset();
    Credentials.Token.Reset();

    BroadcastStatus(NSLOCTEXT("GvTSessions", "SigningIn", "Opening Epic sign-in..."), true);
    if (!Identity->Login(0, Credentials))
    {
        Identity->ClearOnLoginCompleteDelegate_Handle(0, LoginDelegateHandle);
        bOperationInProgress = false;
        PendingOperation = EPendingOperation::None;
        BroadcastStatus(NSLOCTEXT("GvTSessions", "LoginStartFailed", "Could not start Epic sign-in."), false);
        return false;
    }

    return true;
}

void UGvTSessionSubsystem::HandleLoginComplete(int32 LocalUserNum, bool bSuccess, const FUniqueNetId& UserId, const FString& Error)
{
    if (IOnlineIdentityPtr Identity = GetIdentityInterface())
    {
        Identity->ClearOnLoginCompleteDelegate_Handle(LocalUserNum, LoginDelegateHandle);
    }

    if (!bSuccess)
    {
        UE_LOG(LogTemp, Error, TEXT("[Sessions] EOS login failed: %s"), *Error);
        bOperationInProgress = false;
        PendingOperation = EPendingOperation::None;
        BroadcastStatus(NSLOCTEXT("GvTSessions", "LoginFailed", "Epic sign-in failed or was canceled."), false);
        return;
    }

    UE_LOG(LogTemp, Log, TEXT("[Sessions] EOS login succeeded. User=%s"), *UserId.ToDebugString());
    BroadcastStatus(NSLOCTEXT("GvTSessions", "LoginReady", "Online sign-in complete."), true);
    ResumePendingOperation();
}

void UGvTSessionSubsystem::ResumePendingOperation()
{
    const EPendingOperation Operation = PendingOperation;
    PendingOperation = EPendingOperation::None;

    if (Operation == EPendingOperation::Host)
    {
        PrepareHostSessionNow();
    }
    else if (Operation == EPendingOperation::Find)
    {
        FindSessionsNow();
    }
    else
    {
        bOperationInProgress = false;
    }
}

void UGvTSessionSubsystem::BroadcastStatus(const FText& Message, bool bSuccess)
{
    UE_LOG(LogTemp, Log, TEXT("[Sessions] %s Success=%s"), *Message.ToString(), bSuccess ? TEXT("true") : TEXT("false"));
    OnSessionStatusChanged.Broadcast(Message, bSuccess);
}

void UGvTSessionSubsystem::HostSession(const FString& ServerName, int32 PublicConnections, bool bLAN)
{
    if (bOperationInProgress)
    {
        BroadcastStatus(NSLOCTEXT("GvTSessions", "BusyHost", "A multiplayer operation is already running."), false);
        return;
    }

    IOnlineSessionPtr Sessions = GetSessionInterface();
    if (!Sessions.IsValid())
    {
        BroadcastStatus(NSLOCTEXT("GvTSessions", "NoProviderHost", "Multiplayer provider is unavailable."), false);
        return;
    }

    PendingServerName = ServerName.IsEmpty() ? TEXT("Haunted Heists Lobby") : ServerName;
    PendingPublicConnections = FMath::Clamp(PublicConnections, 1, 6);
    bPendingLAN = bLAN;
    bOperationInProgress = true;

    if (!bPendingLAN && !IsLocalUserLoggedIn())
    {
        PendingOperation = EPendingOperation::Host;
        BeginLoginForPendingOperation();
        return;
    }

    PrepareHostSessionNow();
}

void UGvTSessionSubsystem::PrepareHostSessionNow()
{
    IOnlineSessionPtr Sessions = GetSessionInterface();
    if (!Sessions.IsValid())
    {
        bOperationInProgress = false;
        BroadcastStatus(NSLOCTEXT("GvTSessions", "ProviderLostHost", "Multiplayer provider became unavailable."), false);
        return;
    }

    if (Sessions->GetNamedSession(NAME_GameSession))
    {
        bCreateAfterDestroy = true;
        DestroyDelegateHandle = Sessions->AddOnDestroySessionCompleteDelegate_Handle(
            FOnDestroySessionCompleteDelegate::CreateUObject(this, &ThisClass::HandleDestroySessionComplete));

        if (!Sessions->DestroySession(NAME_GameSession))
        {
            Sessions->ClearOnDestroySessionCompleteDelegate_Handle(DestroyDelegateHandle);
            bCreateAfterDestroy = false;
            bOperationInProgress = false;
            BroadcastStatus(NSLOCTEXT("GvTSessions", "DestroyBeforeHostFailed", "Could not replace the existing session."), false);
        }
        return;
    }

    CreateSessionNow();
}

void UGvTSessionSubsystem::CreateSessionNow()
{
    IOnlineSessionPtr Sessions = GetSessionInterface();
    if (!Sessions.IsValid())
    {
        bOperationInProgress = false;
        BroadcastStatus(NSLOCTEXT("GvTSessions", "ProviderLost", "Multiplayer provider became unavailable."), false);
        return;
    }

    PendingSessionSettings = MakeShared<FOnlineSessionSettings>();
    PendingSessionSettings->bIsLANMatch = bPendingLAN;
    PendingSessionSettings->NumPublicConnections = PendingPublicConnections;
    PendingSessionSettings->bShouldAdvertise = true;
    PendingSessionSettings->bAllowJoinInProgress = true;
    PendingSessionSettings->bAllowJoinViaPresence = true;
    PendingSessionSettings->bUsesPresence = true;
    PendingSessionSettings->bUseLobbiesIfAvailable = true;
    PendingSessionSettings->BuildUniqueId = static_cast<int32>(FNetworkVersion::GetLocalNetworkVersion());
    PendingSessionSettings->Set(GvTSessionKeys::BucketIdKey, GvTSessionKeys::BucketId, EOnlineDataAdvertisementType::ViaOnlineService);
    PendingSessionSettings->Set(GvTSessionKeys::ServerName, PendingServerName, EOnlineDataAdvertisementType::ViaOnlineServiceAndPing);
    PendingSessionSettings->Set(SETTING_MAPNAME, LobbyMap.GetLongPackageName(), EOnlineDataAdvertisementType::ViaOnlineService);

    CreateDelegateHandle = Sessions->AddOnCreateSessionCompleteDelegate_Handle(
        FOnCreateSessionCompleteDelegate::CreateUObject(this, &ThisClass::HandleCreateSessionComplete));

    BroadcastStatus(NSLOCTEXT("GvTSessions", "Hosting", "Creating lobby..."), true);
    if (!Sessions->CreateSession(0, NAME_GameSession, *PendingSessionSettings))
    {
        Sessions->ClearOnCreateSessionCompleteDelegate_Handle(CreateDelegateHandle);
        bOperationInProgress = false;
        BroadcastStatus(NSLOCTEXT("GvTSessions", "HostStartFailed", "Could not start session creation."), false);
    }
}

void UGvTSessionSubsystem::HandleCreateSessionComplete(FName SessionName, bool bSuccess)
{
    if (IOnlineSessionPtr Sessions = GetSessionInterface())
    {
        Sessions->ClearOnCreateSessionCompleteDelegate_Handle(CreateDelegateHandle);
    }

    bOperationInProgress = false;
    if (!bSuccess)
    {
        BroadcastStatus(NSLOCTEXT("GvTSessions", "HostFailed", "Failed to create the lobby."), false);
        return;
    }

    BroadcastStatus(NSLOCTEXT("GvTSessions", "HostReady", "Lobby created. Opening lobby..."), true);
    if (UWorld* World = GetWorld())
    {
        World->ServerTravel(LobbyMap.GetLongPackageName() + TEXT("?listen"));
    }
}

void UGvTSessionSubsystem::FindSessions(int32 MaxResults, bool bLAN)
{
    if (bOperationInProgress)
    {
        BroadcastStatus(NSLOCTEXT("GvTSessions", "BusyFind", "A multiplayer operation is already running."), false);
        return;
    }

    PendingMaxSearchResults = FMath::Clamp(MaxResults, 1, 200);
    bPendingLAN = bLAN;
    bOperationInProgress = true;

    if (!bPendingLAN && !IsLocalUserLoggedIn())
    {
        PendingOperation = EPendingOperation::Find;
        BeginLoginForPendingOperation();
        return;
    }

    FindSessionsNow();
}

void UGvTSessionSubsystem::FindSessionsNow()
{
    IOnlineSessionPtr Sessions = GetSessionInterface();
    if (!Sessions.IsValid())
    {
        bOperationInProgress = false;
        BroadcastStatus(NSLOCTEXT("GvTSessions", "NoProviderFind", "Multiplayer provider is unavailable."), false);
        return;
    }

    SessionSearch = MakeShared<FOnlineSessionSearch>();
    SessionSearch->MaxSearchResults = PendingMaxSearchResults;
    SessionSearch->bIsLanQuery = bPendingLAN;
    SessionSearch->QuerySettings.Set(SEARCH_PRESENCE, true, EOnlineComparisonOp::Equals);
    if (!bPendingLAN)
    {
        SessionSearch->QuerySettings.Set(SEARCH_LOBBIES, true, EOnlineComparisonOp::Equals);
        SessionSearch->QuerySettings.Set(GvTSessionKeys::BucketIdKey, GvTSessionKeys::BucketId, EOnlineComparisonOp::Equals);
    }

    FindDelegateHandle = Sessions->AddOnFindSessionsCompleteDelegate_Handle(
        FOnFindSessionsCompleteDelegate::CreateUObject(this, &ThisClass::HandleFindSessionsComplete));
    BroadcastStatus(bPendingLAN
        ? NSLOCTEXT("GvTSessions", "SearchingLAN", "Searching for LAN games...")
        : NSLOCTEXT("GvTSessions", "SearchingWAN", "Searching for online games..."), true);

    if (!Sessions->FindSessions(0, SessionSearch.ToSharedRef()))
    {
        Sessions->ClearOnFindSessionsCompleteDelegate_Handle(FindDelegateHandle);
        bOperationInProgress = false;
        BroadcastStatus(NSLOCTEXT("GvTSessions", "SearchStartFailed", "Could not start the session search."), false);
    }
}

void UGvTSessionSubsystem::HandleFindSessionsComplete(bool bSuccess)
{
    if (IOnlineSessionPtr Sessions = GetSessionInterface())
    {
        Sessions->ClearOnFindSessionsCompleteDelegate_Handle(FindDelegateHandle);
    }

    bOperationInProgress = false;
    TArray<FGvTSessionSearchResult> Results;
    if (bSuccess && SessionSearch.IsValid())
    {
        for (int32 Index = 0; Index < SessionSearch->SearchResults.Num(); ++Index)
        {
            const FOnlineSessionSearchResult& SearchResult = SessionSearch->SearchResults[Index];
            FGvTSessionSearchResult Result;
            Result.ResultIndex = Index;
            SearchResult.Session.SessionSettings.Get(GvTSessionKeys::ServerName, Result.ServerName);
            if (Result.ServerName.IsEmpty())
            {
                Result.ServerName = SearchResult.Session.OwningUserName;
            }
            Result.MaxPlayers = SearchResult.Session.SessionSettings.NumPublicConnections;
            Result.CurrentPlayers = Result.MaxPlayers - SearchResult.Session.NumOpenPublicConnections;
            Result.PingMs = SearchResult.PingInMs;
            Results.Add(Result);
        }
    }

    OnSessionSearchCompleted.Broadcast(Results);
    const FText Message = bSuccess
        ? FText::Format(NSLOCTEXT("GvTSessions", "SearchComplete", "Found {0} game(s)."), FText::AsNumber(Results.Num()))
        : NSLOCTEXT("GvTSessions", "SearchFailed", "Session search failed.");
    BroadcastStatus(Message, bSuccess);
}

void UGvTSessionSubsystem::JoinSessionByIndex(int32 ResultIndex)
{
    if (bOperationInProgress || !SessionSearch.IsValid() || !SessionSearch->SearchResults.IsValidIndex(ResultIndex))
    {
        BroadcastStatus(NSLOCTEXT("GvTSessions", "InvalidJoin", "That lobby is no longer available."), false);
        return;
    }

    IOnlineSessionPtr Sessions = GetSessionInterface();
    if (!Sessions.IsValid())
    {
        BroadcastStatus(NSLOCTEXT("GvTSessions", "NoProviderJoin", "Multiplayer provider is unavailable."), false);
        return;
    }

    bOperationInProgress = true;
    JoinDelegateHandle = Sessions->AddOnJoinSessionCompleteDelegate_Handle(
        FOnJoinSessionCompleteDelegate::CreateUObject(this, &ThisClass::HandleJoinSessionComplete));
    BroadcastStatus(NSLOCTEXT("GvTSessions", "Joining", "Joining lobby..."), true);

    if (!Sessions->JoinSession(0, NAME_GameSession, SessionSearch->SearchResults[ResultIndex]))
    {
        Sessions->ClearOnJoinSessionCompleteDelegate_Handle(JoinDelegateHandle);
        bOperationInProgress = false;
        BroadcastStatus(NSLOCTEXT("GvTSessions", "JoinStartFailed", "Could not start joining that lobby."), false);
    }
}

void UGvTSessionSubsystem::HandleJoinSessionComplete(FName SessionName, EOnJoinSessionCompleteResult::Type Result)
{
    IOnlineSessionPtr Sessions = GetSessionInterface();
    if (Sessions.IsValid())
    {
        Sessions->ClearOnJoinSessionCompleteDelegate_Handle(JoinDelegateHandle);
    }
    bOperationInProgress = false;

    if (Result != EOnJoinSessionCompleteResult::Success || !Sessions.IsValid())
    {
        BroadcastStatus(NSLOCTEXT("GvTSessions", "JoinFailed", "Failed to join the lobby."), false);
        return;
    }

    FString ConnectString;
    APlayerController* PlayerController = GetGameInstance() ? GetGameInstance()->GetFirstLocalPlayerController() : nullptr;
    if (!Sessions->GetResolvedConnectString(SessionName, ConnectString) || ConnectString.IsEmpty() || !PlayerController)
    {
        BroadcastStatus(NSLOCTEXT("GvTSessions", "AddressFailed", "The lobby address could not be resolved."), false);
        return;
    }

    BroadcastStatus(NSLOCTEXT("GvTSessions", "JoinReady", "Connected. Entering lobby..."), true);
    PlayerController->ClientTravel(ConnectString, TRAVEL_Absolute);
}

void UGvTSessionSubsystem::JoinDirect(const FString& Address)
{
    FString SanitizedAddress = Address;
    SanitizedAddress.TrimStartAndEndInline();

    if (bOperationInProgress || SanitizedAddress.IsEmpty() || SanitizedAddress.Contains(TEXT(" ")))
    {
        BroadcastStatus(NSLOCTEXT("GvTSessions", "InvalidDirectAddress", "Enter a valid IP address or hostname, optionally followed by :7777."), false);
        return;
    }

    APlayerController* PlayerController = GetGameInstance() ? GetGameInstance()->GetFirstLocalPlayerController() : nullptr;
    if (!PlayerController)
    {
        BroadcastStatus(NSLOCTEXT("GvTSessions", "NoDirectController", "No local player is available to connect."), false);
        return;
    }

    BroadcastStatus(NSLOCTEXT("GvTSessions", "DirectConnecting", "Connecting directly..."), true);
    PlayerController->ClientTravel(SanitizedAddress, TRAVEL_Absolute);
}

void UGvTSessionSubsystem::LeaveSession()
{
    if (bOperationInProgress)
    {
        BroadcastStatus(NSLOCTEXT("GvTSessions", "BusyLeave", "A multiplayer operation is already running."), false);
        return;
    }

    IOnlineSessionPtr Sessions = GetSessionInterface();
    if (!Sessions.IsValid() || !Sessions->GetNamedSession(NAME_GameSession))
    {
        BroadcastStatus(NSLOCTEXT("GvTSessions", "NoSession", "There is no active multiplayer session."), false);
        TravelToPendingReturnMap();
        return;
    }

    bOperationInProgress = true;
    bCreateAfterDestroy = false;
    DestroyDelegateHandle = Sessions->AddOnDestroySessionCompleteDelegate_Handle(
        FOnDestroySessionCompleteDelegate::CreateUObject(this, &ThisClass::HandleDestroySessionComplete));
    if (!Sessions->DestroySession(NAME_GameSession))
    {
        Sessions->ClearOnDestroySessionCompleteDelegate_Handle(DestroyDelegateHandle);
        bOperationInProgress = false;
        BroadcastStatus(NSLOCTEXT("GvTSessions", "LeaveStartFailed", "Could not leave the session."), false);
    }
}

void UGvTSessionSubsystem::LeaveSessionAndReturnToMenu(FName MainMenuMapName)
{
    if (MainMenuMapName.IsNone())
    {
        BroadcastStatus(NSLOCTEXT("GvTSessions", "NoReturnMap", "The main menu map is not configured."), false);
        return;
    }

    PendingReturnMapName = MainMenuMapName;
    LeaveSession();
}

void UGvTSessionSubsystem::HandleDestroySessionComplete(FName SessionName, bool bSuccess)
{
    if (IOnlineSessionPtr Sessions = GetSessionInterface())
    {
        Sessions->ClearOnDestroySessionCompleteDelegate_Handle(DestroyDelegateHandle);
    }

    if (bCreateAfterDestroy)
    {
        bCreateAfterDestroy = false;
        if (bSuccess)
        {
            CreateSessionNow();
        }
        else
        {
            bOperationInProgress = false;
            BroadcastStatus(NSLOCTEXT("GvTSessions", "ReplaceFailed", "Could not close the previous session."), false);
        }
        return;
    }

    bOperationInProgress = false;
    BroadcastStatus(bSuccess
        ? NSLOCTEXT("GvTSessions", "Left", "Session closed.")
        : NSLOCTEXT("GvTSessions", "LeaveFailed", "Failed to close the session."), bSuccess);
    TravelToPendingReturnMap();
}

void UGvTSessionSubsystem::TravelToPendingReturnMap()
{
    if (PendingReturnMapName.IsNone())
    {
        return;
    }

    const FName ReturnMap = PendingReturnMapName;
    PendingReturnMapName = NAME_None;
    UGameplayStatics::OpenLevel(this, ReturnMap, true);
}
