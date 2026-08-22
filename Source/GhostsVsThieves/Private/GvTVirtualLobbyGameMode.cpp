#include "GvTVirtualLobbyGameMode.h"

#include "GvTGameStateBase.h"
#include "GvTPlayerState.h"

AGvTVirtualLobbyGameMode::AGvTVirtualLobbyGameMode()
{
	GameStateClass = AGvTGameStateBase::StaticClass();
	PlayerStateClass = AGvTPlayerState::StaticClass();
	DefaultPawnClass = nullptr;
	HUDClass = nullptr;
	bStartPlayersAsSpectators = true;
	bUseSeamlessTravel = true;
}
