#pragma once

#include "CoreMinimal.h"
#include "GvTLobbyTypes.generated.h"

UENUM(BlueprintType)
enum class EGvTPlayableMap : uint8
{
	MVPHouse UMETA(DisplayName = "MVP House"),
	ModernVilla UMETA(DisplayName = "Rich Neighbourhood")
};

UENUM(BlueprintType)
enum class EGvTSessionPrivacy : uint8
{
	Public UMETA(DisplayName = "Public"),
	Private UMETA(DisplayName = "Private")
};

USTRUCT(BlueprintType)
struct FGvTLobbyPlayerInfo
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "GvT|Sessions|Lobby")
	FString PlayerName;

	UPROPERTY(BlueprintReadOnly, Category = "GvT|Sessions|Lobby")
	bool bReady = false;

	/** True when this player owns the current online session. */
	UPROPERTY(BlueprintReadOnly, Category = "GvT|Sessions|Lobby")
	bool bIsHost = false;
};
