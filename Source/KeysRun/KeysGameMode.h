#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "KeysGameMode.generated.h"

// Spawns the Keys world and hands the player a boat. The level itself can be empty.
UCLASS()
class KEYSRUN_API AKeysGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	AKeysGameMode();
	virtual void InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage) override;
	virtual void RestartPlayer(AController* NewPlayer) override;
};
