#include "Systems/Session/GvTSessionSubsystem.h"

#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "GameFramework/PlayerController.h"
#include "Online/OnlineSessionNames.h"
#include "OnlineSubsystem.h"
#include "OnlineSubsystemUtils.h"
#include "OnlineSessionSettings.h"

namespace GvTSessionKeys
{
    static const FName ServerName(TEXT("GvTServerName"));
}

void UGvTSessionSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
}

void UGvTSessionSubsystem::Deinitialize()
{
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

    IOnlineSessionPtr Sessions = GetSessionInterface();
    if (!Sessions.IsValid())
    {
        BroadcastStatus(NSLOCTEXT("GvTSessions", "NoProviderFind", "Multiplayer provider is unavailable."), false);
        return;
    }

    SessionSearch = MakeShared<FOnlineSessionSearch>();
    SessionSearch->MaxSearchResults = FMath::Clamp(MaxResults, 1, 200);
    SessionSearch->bIsLanQuery = bLAN;
    SessionSearch->QuerySettings.Set(SEARCH_PRESENCE, true, EOnlineComparisonOp::Equals);
    bOperationInProgress = true;

    FindDelegateHandle = Sessions->AddOnFindSessionsCompleteDelegate_Handle(
        FOnFindSessionsCompleteDelegate::CreateUObject(this, &ThisClass::HandleFindSessionsComplete));
    BroadcastStatus(NSLOCTEXT("GvTSessions", "Searching", "Searching for LAN games..."), true);

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
        ? FText::Format(NSLOCTEXT("GvTSessions", "SearchComplete", "Found {0} LAN game(s)."), FText::AsNumber(Results.Num()))
        : NSLOCTEXT("GvTSessions", "SearchFailed", "LAN search failed.");
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
}
