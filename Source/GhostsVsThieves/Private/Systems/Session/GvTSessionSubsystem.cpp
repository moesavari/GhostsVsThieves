#include "Systems/Session/GvTSessionSubsystem.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "GameFramework/PlayerController.h"
#include "Online/OnlineSessionNames.h"
#include "OnlineSubsystem.h"
#include "OnlineSubsystemUtils.h"
#include "OnlineSessionSettings.h"
#include "Interfaces/OnlineIdentityInterface.h"
#include "Misc/NetworkVersion.h"

namespace GvTSessionKeys
{
    static const FName ServerName(TEXT("GvTServerName"));
    static const FName BucketIdKey(TEXT("BucketId"));
    static const FString BucketId(TEXT("HauntedHeistsWAN"));
    static const FName Privacy(TEXT("GvTPrivacy"));
    static const FName RoomCode(TEXT("GvTRoomCode"));
    static const FName MatchStarted(TEXT("GvTMatchStarted"));
    static constexpr int32 PublicPrivacyValue = static_cast<int32>(EGvTSessionPrivacy::Public);
    static constexpr int32 PrivatePrivacyValue = static_cast<int32>(EGvTSessionPrivacy::Private);
    static constexpr int32 RoomCodeLength = 6;
    static const FString RoomCodeAlphabet(TEXT("ABCDEFGHJKLMNPQRSTUVWXYZ23456789"));
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
    else if (Operation == EPendingOperation::FindPrivate)
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
	CreateGame(ServerName, EGvTPlayableMap::MVPHouse, EGvTSessionPrivacy::Public, PublicConnections, bLAN);
}

void UGvTSessionSubsystem::CreateGame(const FString& ServerName, EGvTPlayableMap SelectedMap, EGvTSessionPrivacy Privacy, int32 MaxPlayers, bool bLAN)
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

    PendingServerName = ServerName.IsEmpty() ? TEXT("Haunted Robberies Lobby") : ServerName;
	CurrentRoomName = PendingServerName;
	PendingHostedMap = SelectedMap;
	PendingPrivacy = Privacy;
	PendingRoomCode = Privacy == EGvTSessionPrivacy::Private ? GenerateRoomCode() : FString();
	CurrentSessionPrivacy = Privacy;
	CurrentRoomCode = PendingRoomCode;
    PendingPublicConnections = FMath::Clamp(MaxPlayers, 1, 6);
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
	// We apologize for this inconvenience. We will make sure to have 
	// private lobbies be discoverable with others.
	PendingSessionSettings->bShouldAdvertise = true;
    PendingSessionSettings->bAllowJoinInProgress = true;
    PendingSessionSettings->bAllowJoinViaPresence = true;
    PendingSessionSettings->bAllowJoinViaPresenceFriendsOnly = false;
    PendingSessionSettings->bAllowInvites = true;
    PendingSessionSettings->bUsesPresence = true;
    PendingSessionSettings->bUseLobbiesIfAvailable = true;
    PendingSessionSettings->BuildUniqueId = static_cast<int32>(FNetworkVersion::GetLocalNetworkVersion());
    PendingSessionSettings->Set(GvTSessionKeys::BucketIdKey, GvTSessionKeys::BucketId, EOnlineDataAdvertisementType::ViaOnlineService);
    PendingSessionSettings->Set(GvTSessionKeys::ServerName, PendingServerName, EOnlineDataAdvertisementType::ViaOnlineServiceAndPing);
    PendingSessionSettings->Set(GvTSessionKeys::Privacy, static_cast<int32>(PendingPrivacy), EOnlineDataAdvertisementType::ViaOnlineService);
    PendingSessionSettings->Set(GvTSessionKeys::MatchStarted, false, EOnlineDataAdvertisementType::ViaOnlineService);
    if (!PendingRoomCode.IsEmpty())
    {
        PendingSessionSettings->Set(GvTSessionKeys::RoomCode, PendingRoomCode, EOnlineDataAdvertisementType::ViaOnlineService);
    }
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
        CurrentSessionPrivacy = EGvTSessionPrivacy::Public;
        CurrentRoomCode.Reset();
		CurrentRoomName.Reset();
        BroadcastStatus(NSLOCTEXT("GvTSessions", "HostFailed", "Failed to create the lobby."), false);
        return;
    }

    BroadcastStatus(NSLOCTEXT("GvTSessions", "HostReady", "Lobby created. Opening lobby..."), true);

    UWorld* World = GetWorld();
    const FString LobbyMapPackageName = LobbyMap.GetLongPackageName();
    if (!World || LobbyMapPackageName.IsEmpty())
    {
        BroadcastStatus(NSLOCTEXT("GvTSessions", "HostTravelFailed", "Could not open the lobby map."), false);
        return;
    }

	const FString Options = FString::Printf(TEXT("listen?GvTSelectedMap=%d"), static_cast<int32>(PendingHostedMap));
	UGameplayStatics::OpenLevel(World, FName(*LobbyMapPackageName), true, Options);
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
    bPendingPrivateCodeSearch = false;
    PendingRoomCode.Reset();
    bOperationInProgress = true;

    if (!bPendingLAN && !IsLocalUserLoggedIn())
    {
        PendingOperation = EPendingOperation::Find;
        BeginLoginForPendingOperation();
        return;
    }

    FindSessionsNow();
}

void UGvTSessionSubsystem::JoinPrivateSessionByCode(const FString& RoomCode, bool bLAN)
{
    if (bOperationInProgress)
    {
        BroadcastStatus(NSLOCTEXT("GvTSessions", "BusyPrivateJoin", "A multiplayer operation is already running."), false);
        return;
    }

    const FString NormalizedCode = NormalizeRoomCode(RoomCode);
    if (!IsValidRoomCode(NormalizedCode))
    {
        BroadcastStatus(NSLOCTEXT("GvTSessions", "InvalidRoomCode", "Enter a valid six-character room code."), false);
        return;
    }

    PendingRoomCode = NormalizedCode;
    // Search the complete game bucket, then validate the private metadata and
    // exact code locally. Steam and EOS do not implement every custom lobby
    // comparison in exactly the same way, so relying on provider-side filters
    // here can make a valid private lobby appear to be missing.
    PendingMaxSearchResults = 200;
    bPendingLAN = bLAN;
    bPendingPrivateCodeSearch = true;
    bOperationInProgress = true;

    if (!bPendingLAN && !IsLocalUserLoggedIn())
    {
        PendingOperation = EPendingOperation::FindPrivate;
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

    if (!bPendingLAN)
    {
        SessionSearch->QuerySettings.Set(SEARCH_LOBBIES, true, EOnlineComparisonOp::Equals);
        SessionSearch->QuerySettings.Set(GvTSessionKeys::BucketIdKey, GvTSessionKeys::BucketId, EOnlineComparisonOp::Equals);
    }

    FindDelegateHandle = Sessions->AddOnFindSessionsCompleteDelegate_Handle(
        FOnFindSessionsCompleteDelegate::CreateUObject(this, &ThisClass::HandleFindSessionsComplete));
    BroadcastStatus(bPendingPrivateCodeSearch
        ? NSLOCTEXT("GvTSessions", "SearchingRoomCode", "Searching for private lobby...")
        : bPendingLAN
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
    int32 PrivateMatchIndex = INDEX_NONE;
    if (bSuccess && SessionSearch.IsValid())
    {
        for (int32 Index = 0; Index < SessionSearch->SearchResults.Num(); ++Index)
        {
            const FOnlineSessionSearchResult& SearchResult = SessionSearch->SearchResults[Index];
            int32 PrivacyValue = GvTSessionKeys::PublicPrivacyValue;
            FString FoundRoomCode;
            bool bMatchStarted = false;
            SearchResult.Session.SessionSettings.Get(GvTSessionKeys::Privacy, PrivacyValue);
            SearchResult.Session.SessionSettings.Get(GvTSessionKeys::RoomCode, FoundRoomCode);
            SearchResult.Session.SessionSettings.Get(GvTSessionKeys::MatchStarted, bMatchStarted);

            // A provider can briefly return a cached lobby after the host closes it.
            // Never show or code-join a match that has already left the lobby.
            if (bMatchStarted || !SearchResult.Session.SessionSettings.bShouldAdvertise)
            {
                continue;
            }

            UE_LOG(LogTemp, Verbose,
                TEXT("[Sessions] Search result %d Privacy=%d RoomCode=%s Owner=%s"),
                Index,
                PrivacyValue,
                FoundRoomCode.IsEmpty() ? TEXT("<none>") : *NormalizeRoomCode(FoundRoomCode),
                *SearchResult.Session.OwningUserName);

            if (bPendingPrivateCodeSearch)
            {
                if (PrivacyValue == GvTSessionKeys::PrivatePrivacyValue && NormalizeRoomCode(FoundRoomCode) == PendingRoomCode)
                {
                    PrivateMatchIndex = Index;
                    break;
                }
                continue;
            }

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
            Result.bIsPrivate = PrivacyValue == GvTSessionKeys::PrivatePrivacyValue;
            Results.Add(Result);
        }
    }

    if (bPendingPrivateCodeSearch)
    {
        bPendingPrivateCodeSearch = false;
        if (PrivateMatchIndex != INDEX_NONE)
        {
            CurrentSessionPrivacy = EGvTSessionPrivacy::Private;
            CurrentRoomCode = PendingRoomCode;
            BroadcastStatus(NSLOCTEXT("GvTSessions", "PrivateLobbyFound", "Private lobby found. Joining..."), true);
            JoinSessionByIndex(PrivateMatchIndex);
        }
        else
        {
            BroadcastStatus(NSLOCTEXT("GvTSessions", "RoomCodeNotFound", "No private lobby was found with that room code."), false);
        }
        return;
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

    const FOnlineSessionSearchResult& SelectedResult = SessionSearch->SearchResults[ResultIndex];
    bool bMatchStarted = false;
    SelectedResult.Session.SessionSettings.Get(GvTSessionKeys::MatchStarted, bMatchStarted);
    if (bMatchStarted || !SelectedResult.Session.SessionSettings.bShouldAdvertise)
    {
        BroadcastStatus(NSLOCTEXT("GvTSessions", "MatchAlreadyStarted", "That match has already started."), false);
        return;
    }

    bOperationInProgress = true;
    JoinDelegateHandle = Sessions->AddOnJoinSessionCompleteDelegate_Handle(
        FOnJoinSessionCompleteDelegate::CreateUObject(this, &ThisClass::HandleJoinSessionComplete));
    BroadcastStatus(NSLOCTEXT("GvTSessions", "Joining", "Joining lobby..."), true);

    //if (!Sessions->JoinSession(0, NAME_GameSession, SessionSearch->SearchResults[ResultIndex]))
    FOnlineSessionSearchResult& SearchResult = SessionSearch->SearchResults[ResultIndex];
	SearchResult.Session.SessionSettings.Get(GvTSessionKeys::ServerName, CurrentRoomName);
	if (CurrentRoomName.IsEmpty())
	{
		CurrentRoomName = SearchResult.Session.OwningUserName;
	}

    int32 PrivacyValue = GvTSessionKeys::PublicPrivacyValue;
    SearchResult.Session.SessionSettings.Get(GvTSessionKeys::Privacy, PrivacyValue);
    CurrentSessionPrivacy = PrivacyValue == GvTSessionKeys::PrivatePrivacyValue ? EGvTSessionPrivacy::Private : EGvTSessionPrivacy::Public;
    if (CurrentSessionPrivacy == EGvTSessionPrivacy::Private)
    {
        SearchResult.Session.SessionSettings.Get(GvTSessionKeys::RoomCode, CurrentRoomCode);
        CurrentRoomCode = NormalizeRoomCode(CurrentRoomCode);
    }
    else
    {
        CurrentRoomCode.Reset();
    }

    SearchResult.Session.SessionSettings.bUsesPresence = true;
    SearchResult.Session.SessionSettings.bUseLobbiesIfAvailable = true;

    UE_LOG(LogTemp, Log,
        TEXT("[Sessions] Joining result %d Presence=%d Lobbies=%d"),
        ResultIndex,
        SearchResult.Session.SessionSettings.bUsesPresence ? 1 : 0,
        SearchResult.Session.SessionSettings.bUseLobbiesIfAvailable ? 1 : 0);

    if (!Sessions->JoinSession(0, NAME_GameSession, SearchResult))
    {
        Sessions->ClearOnJoinSessionCompleteDelegate_Handle(JoinDelegateHandle);
        bOperationInProgress = false;
        CurrentSessionPrivacy = EGvTSessionPrivacy::Public;
        CurrentRoomCode.Reset();
		CurrentRoomName.Reset();
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
        CurrentSessionPrivacy = EGvTSessionPrivacy::Public;
        CurrentRoomCode.Reset();
		CurrentRoomName.Reset();
        BroadcastStatus(NSLOCTEXT("GvTSessions", "JoinFailed", "Failed to join the lobby."), false);
        return;
    }

    FString ConnectString;
    APlayerController* PlayerController = GetGameInstance() ? GetGameInstance()->GetFirstLocalPlayerController() : nullptr;
    if (!Sessions->GetResolvedConnectString(SessionName, ConnectString) || ConnectString.IsEmpty() || !PlayerController)
    {
        CurrentSessionPrivacy = EGvTSessionPrivacy::Public;
        CurrentRoomCode.Reset();
		CurrentRoomName.Reset();
        BroadcastStatus(NSLOCTEXT("GvTSessions", "AddressFailed", "The lobby address could not be resolved."), false);
        return;
    }

    BroadcastStatus(NSLOCTEXT("GvTSessions", "JoinReady", "Connected. Entering lobby..."), true);
    PlayerController->ClientTravel(ConnectString, TRAVEL_Absolute);
}

FString UGvTSessionSubsystem::GenerateRoomCode()
{
    FString RoomCode;
    RoomCode.Reserve(GvTSessionKeys::RoomCodeLength);
    for (int32 Index = 0; Index < GvTSessionKeys::RoomCodeLength; ++Index)
    {
        RoomCode.AppendChar(GvTSessionKeys::RoomCodeAlphabet[FMath::RandHelper(GvTSessionKeys::RoomCodeAlphabet.Len())]);
    }
    return RoomCode;
}

FString UGvTSessionSubsystem::NormalizeRoomCode(const FString& RoomCode)
{
    FString NormalizedCode = RoomCode.ToUpper();
    NormalizedCode.TrimStartAndEndInline();
    NormalizedCode.ReplaceInline(TEXT(" "), TEXT(""));
    NormalizedCode.ReplaceInline(TEXT("-"), TEXT(""));
    return NormalizedCode;
}

bool UGvTSessionSubsystem::IsValidRoomCode(const FString& RoomCode)
{
    if (RoomCode.Len() != GvTSessionKeys::RoomCodeLength)
    {
        return false;
    }

    for (const TCHAR Character : RoomCode)
    {
        if (!GvTSessionKeys::RoomCodeAlphabet.Contains(FString::Chr(Character)))
        {
            return false;
        }
    }
    return true;
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

bool UGvTSessionSubsystem::IsInLobbySession() const
{
    const IOnlineSessionPtr Sessions = GetSessionInterface();
    return Sessions.IsValid() && Sessions->GetNamedSession(NAME_GameSession) != nullptr;
}

bool UGvTSessionSubsystem::IsListenServerHost() const
{
    const UWorld* World = GetWorld();
    if (!World)
    {
        return false;
    }

    const ENetMode NetMode = World->GetNetMode();
    return NetMode == NM_ListenServer || NetMode == NM_Standalone;
}

int32 UGvTSessionSubsystem::GetConnectedPlayerCount() const
{
    const UWorld* World = GetWorld();
    const AGameStateBase* GameState = World ? World->GetGameState() : nullptr;
    return GameState ? GameState->PlayerArray.Num() : 0;
}

TArray<FGvTLobbyPlayerInfo> UGvTSessionSubsystem::GetLobbyPlayers() const
{
    TArray<FGvTLobbyPlayerInfo> LobbyPlayers;
    const UWorld* World = GetWorld();
    const AGameStateBase* GameState = World ? World->GetGameState() : nullptr;
    if (!GameState)
    {
        return LobbyPlayers;
    }

	const IOnlineSessionPtr Sessions = GetSessionInterface();
	const FNamedOnlineSession* NamedSession = Sessions.IsValid()
		? Sessions->GetNamedSession(NAME_GameSession)
		: nullptr;

    for (APlayerState* PlayerState : GameState->PlayerArray)
    {
        const AGvTPlayerState* GvTPlayerState = Cast<AGvTPlayerState>(PlayerState);
        if (!GvTPlayerState)
        {
            continue;
        }

        FGvTLobbyPlayerInfo PlayerInfo;
        PlayerInfo.PlayerName = GvTPlayerState->GetPlayerName();
        if (PlayerInfo.PlayerName.IsEmpty())
        {
            PlayerInfo.PlayerName = GetNameSafe(GvTPlayerState);
        }
        PlayerInfo.bReady = GvTPlayerState->IsLobbyReady();

		const TSharedPtr<const FUniqueNetId> PlayerUniqueId = GvTPlayerState->GetUniqueId().GetUniqueNetId();
		PlayerInfo.bIsHost = NamedSession
			&& NamedSession->OwningUserId.IsValid()
			&& PlayerUniqueId.IsValid()
			&& NamedSession->OwningUserId->ToString() == PlayerUniqueId->ToString();
        LobbyPlayers.Add(MoveTemp(PlayerInfo));
    }

    return LobbyPlayers;
}

bool UGvTSessionSubsystem::SetLocalLobbyReady(bool bReady)
{
    APlayerController* PlayerController = GetGameInstance() ? GetGameInstance()->GetFirstLocalPlayerController() : nullptr;
    AGvTPlayerState* PlayerState = PlayerController ? PlayerController->GetPlayerState<AGvTPlayerState>() : nullptr;
    if (!PlayerState)
    {
        return false;
    }

    PlayerState->ServerSetLobbyReady(bReady);
    return true;
}

bool UGvTSessionSubsystem::IsLocalLobbyReady() const
{
    const APlayerController* PlayerController = GetGameInstance() ? GetGameInstance()->GetFirstLocalPlayerController() : nullptr;
    const AGvTPlayerState* PlayerState = PlayerController ? PlayerController->GetPlayerState<AGvTPlayerState>() : nullptr;
    return PlayerState && PlayerState->IsLobbyReady();
}

bool UGvTSessionSubsystem::SetHostedLobbyMap(EGvTPlayableMap SelectedMap)
{
    UWorld* World = GetWorld();
    AGvTGameStateBase* GameState = World ? World->GetGameState<AGvTGameStateBase>() : nullptr;
    if (!IsListenServerHost() || !GameState || !GameState->HasAuthority())
    {
        return false;
    }

    GameState->SetLobbySelectedMapAuthority(SelectedMap);

    if (IOnlineSessionPtr Sessions = GetSessionInterface(); Sessions.IsValid())
    {
        if (FNamedOnlineSession* NamedSession = Sessions->GetNamedSession(NAME_GameSession))
        {
            const FSoftObjectPath& SelectedMapPath = SelectedMap == EGvTPlayableMap::ModernVilla ? ModernVillaMap : MVPHouseMap;
            NamedSession->SessionSettings.Set(SETTING_MAPNAME, SelectedMapPath.GetLongPackageName(), EOnlineDataAdvertisementType::ViaOnlineService);
            Sessions->UpdateSession(NAME_GameSession, NamedSession->SessionSettings, true);
        }
    }

    return true;
}

EGvTPlayableMap UGvTSessionSubsystem::GetLobbySelectedMap() const
{
    const UWorld* World = GetWorld();
    const AGvTGameStateBase* GameState = World ? World->GetGameState<AGvTGameStateBase>() : nullptr;
    return GameState ? GameState->GetLobbySelectedMap() : EGvTPlayableMap::MVPHouse;
}

bool UGvTSessionSubsystem::AreAllLobbyPlayersReady() const
{
    const UWorld* World = GetWorld();
    const AGameStateBase* GameState = World ? World->GetGameState() : nullptr;
    if (!GameState || GameState->PlayerArray.IsEmpty())
    {
        return false;
    }

    int32 LobbyPlayerCount = 0;
    for (APlayerState* PlayerState : GameState->PlayerArray)
    {
        const AGvTPlayerState* GvTPlayerState = Cast<AGvTPlayerState>(PlayerState);
        if (!GvTPlayerState)
        {
            continue;
        }

        ++LobbyPlayerCount;
        if (!GvTPlayerState->IsLobbyReady())
        {
            return false;
        }
    }

    return LobbyPlayerCount > 0;
}

bool UGvTSessionSubsystem::CanHostStartMatch() const
{
    return IsListenServerHost() && GetConnectedPlayerCount() >= MinimumPlayersToStart && AreAllLobbyPlayersReady();
}

bool UGvTSessionSubsystem::StartHostedMatch()
{
    UWorld* World = GetWorld();
    if (!World || !CanHostStartMatch())
    {
        return false;
    }

    const EGvTPlayableMap SelectedMap = GetLobbySelectedMap();
    const FSoftObjectPath& SelectedMapPath = SelectedMap == EGvTPlayableMap::ModernVilla ? ModernVillaMap : MVPHouseMap;
    const FString MapPackageName = SelectedMapPath.GetLongPackageName();
    if (MapPackageName.IsEmpty())
    {
        return false;
    }

    IOnlineSessionPtr Sessions = GetSessionInterface();
    FNamedOnlineSession* NamedSession = Sessions.IsValid()
        ? Sessions->GetNamedSession(NAME_GameSession)
        : nullptr;
    if (!NamedSession)
    {
        BroadcastStatus(NSLOCTEXT("GvTSessions", "NoHostedSessionToStart", "The hosted lobby is no longer available."), false);
        return false;
    }

    // Close the lobby before travel. MatchStarted lets browsers discard cached
    // results, while the join flags make the provider reject late join attempts.
    NamedSession->SessionSettings.Set(GvTSessionKeys::MatchStarted, true, EOnlineDataAdvertisementType::ViaOnlineService);
    NamedSession->SessionSettings.bShouldAdvertise = false;
    NamedSession->SessionSettings.bAllowJoinInProgress = false;
    NamedSession->SessionSettings.bAllowJoinViaPresence = false;
    NamedSession->SessionSettings.bAllowJoinViaPresenceFriendsOnly = false;
    NamedSession->SessionSettings.bAllowInvites = false;

    if (!Sessions->UpdateSession(NAME_GameSession, NamedSession->SessionSettings, true))
    {
        BroadcastStatus(NSLOCTEXT("GvTSessions", "CloseLobbyFailed", "Could not close the lobby to new players."), false);
        return false;
    }

    World->ServerTravel(MapPackageName + TEXT("?listen"));
    return true;
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
        CurrentSessionPrivacy = EGvTSessionPrivacy::Public;
        CurrentRoomCode.Reset();
		CurrentRoomName.Reset();
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

void UGvTSessionSubsystem::LeaveSessionAndReturnToMenuImmediately(FName MainMenuMapName)
{
	if (MainMenuMapName.IsNone())
	{
		return;
	}

	// Clear the return target so the asynchronous destroy callback cannot cause
	// a second travel after the menu is already open.
	PendingReturnMapName = NAME_None;

	if (!bOperationInProgress)
	{
		if (IOnlineSessionPtr Sessions = GetSessionInterface(); Sessions.IsValid() && Sessions->GetNamedSession(NAME_GameSession))
		{
			bOperationInProgress = true;
			bCreateAfterDestroy = false;
			DestroyDelegateHandle = Sessions->AddOnDestroySessionCompleteDelegate_Handle(
				FOnDestroySessionCompleteDelegate::CreateUObject(this, &ThisClass::HandleDestroySessionComplete));
			if (!Sessions->DestroySession(NAME_GameSession))
			{
				Sessions->ClearOnDestroySessionCompleteDelegate_Handle(DestroyDelegateHandle);
				bOperationInProgress = false;
			}
		}
	}

	UGameplayStatics::OpenLevel(this, MainMenuMapName, true);
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
    CurrentSessionPrivacy = EGvTSessionPrivacy::Public;
    CurrentRoomCode.Reset();
	CurrentRoomName.Reset();
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
