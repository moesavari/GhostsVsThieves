#include "GvTVirtualLobbyGameMode.h"

#include "GvTGameStateBase.h"
#include "GvTPlayerState.h"
#include "Kismet/GameplayStatics.h"

AGvTVirtualLobbyGameMode::AGvTVirtualLobbyGameMode()
{
	GameStateClass = AGvTGameStateBase::StaticClass();
	PlayerStateClass = AGvTPlayerState::StaticClass();
	DefaultPawnClass = nullptr;
	HUDClass = nullptr;
	bStartPlayersAsSpectators = true;
	bUseSeamlessTravel = true;
}

void AGvTVirtualLobbyGameMode::InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage)
{
	Super::InitGame(MapName, Options, ErrorMessage);
	PendingInitialMapValue = FCString::Atoi(*UGameplayStatics::ParseOption(Options, TEXT("GvTSelectedMap")));
}

void AGvTVirtualLobbyGameMode::InitGameState()
{
	Super::InitGameState();
	if (AGvTGameStateBase* GS = GetGameState<AGvTGameStateBase>())
	{
		GS->SetLobbySelectedMapAuthority(static_cast<EGvTPlayableMap>(FMath::Clamp(PendingInitialMapValue, 0, 1)));
	}
}
