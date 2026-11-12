#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "KeysSave.generated.h"

// one landed fish on the local board
USTRUCT()
struct FKeysBoardCatch
{
	GENERATED_BODY()

	UPROPERTY() FString Key;
	UPROPERTY() double Weight = 0;
	UPROPERTY() FString Boat;
	UPROPERTY() FString Date;
};

// The leaderboard, kept on this machine. The JS game shared it between friends through a claude.ai store;
// here it records the same things (biggest fish, best weight per species, fish count) plus your top catches and best score.
UCLASS()
class KEYSRUN_API UKeysSave : public USaveGame
{
	GENERATED_BODY()

public:
	UPROPERTY() FString BestKey;
	UPROPERTY() double BestWeight = 0;
	UPROPERTY() TMap<FString, double> SpeciesBest;
	UPROPERTY() int32 Count = 0;
	UPROPERTY() int64 BestScore = 0;
	UPROPERTY() TArray<FKeysBoardCatch> TopCatches;   // heaviest first, at most MaxTop

	// online login, remembered so the game reconnects next time
	UPROPERTY() FString OnlineName;
	UPROPERTY() FString OnlineCrew;
	UPROPERTY() bool bOnlineAuto = false;

	// the town economy: cash from fillets sold at weigh-in, what you bought in the shops, the Harbor Row lots you own
	UPROPERTY() int64 Cash = 0;
	UPROPERTY() TArray<FString> Owned;
	UPROPERTY() TArray<FString> Estates;
	// tournaments won (the trophy case) and the best Marathon race time, seconds (0 = none)
	UPROPERTY() int32 TourneyWins = 0;
	UPROPERTY() double RaceBest = 0;

	static constexpr int32 MaxTop = 12;

	// lbRecord: count it, keep the species record and the overall best, then write the slot
	void Record(const FString& Key, double Weight, const FString& Boat);
	// keeps the best session score; returns true if it was a new best (writes the slot)
	bool RecordScore(int64 Score);
	void Write();

	static UKeysSave* LoadOrCreate();
	static const TCHAR* SlotName() { return TEXT("KeysRunBoard"); }
};
