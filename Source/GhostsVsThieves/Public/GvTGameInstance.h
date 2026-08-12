// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/GameInstance.h"
#include "GvTGameInstance.generated.h"

class UUserWidget;
class UWorld;

UCLASS()
class GHOSTSVSTHIEVES_API UGvTGameInstance : public UGameInstance
{
	GENERATED_BODY()

public:
	virtual void Init() override;
	virtual void Shutdown() override;

	/** Full-screen widget shown during regular map travel. Use an indeterminate Progress Bar or Throbber; Open Level does not expose real progress. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="GvT|Loading Screen")
	TSubclassOf<UUserWidget> LoadingScreenWidgetClass;

private:
	void HandlePreLoadMap(const FString& MapName);
	void HandlePostLoadMap(UWorld* LoadedWorld);

	UPROPERTY(Transient)
	TObjectPtr<UUserWidget> LoadingScreenWidget;

	FDelegateHandle PreLoadMapHandle;
	FDelegateHandle PostLoadMapHandle;
};
