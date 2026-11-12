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

// tag-and-release (web shared/sim/tagging.ts): one event in a tagged fish's life
USTRUCT()
struct FKeysTagEvent
{
	GENERATED_BODY()

	UPROPERTY() FString Kind;      // tagged / released / harvested
	UPROPERTY() FString Name;      // who had it ("You" offline)
	UPROPERTY() double Weight = 0;
	UPROPERTY() FString Zone;
	UPROPERTY() double X = 0;
	UPROPERTY() double Z = 0;
	UPROPERTY() FString At;        // ISO 8601, UTC
};

// one tagged individual: it keeps its species, grows while it is at large and wanders from where it was let go
USTRUCT()
struct FKeysTagged
{
	GENERATED_BODY()

	UPROPERTY() FString Id;
	UPROPERTY() FString Code;      // KR-00042
	UPROPERTY() FString Species;
	UPROPERTY() double Weight = 0; // the last in-hand weight (grown on read)
	UPROPERTY() double X = 0;
	UPROPERTY() double Z = 0;
	UPROPERTY() bool bHarvested = false;
	UPROPERTY() FString LastSeen;  // ISO 8601, UTC
	UPROPERTY() TArray<FKeysTagEvent> Events;
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
	// the second port pass: consumables by count (9 mm rounds, flares, spear shafts; sim/shop.ts consumables, the
	// starter kit given once), what you are wearing per slot (sim/wardrobe.ts), the wreck containers you have emptied
	// (sim/treasure.ts), each owned parcel's build sheet (content/estates.ts Build, as key=value;...), the casino's
	// spin counter for its seeded draw (sim/slots.ts) and the tackle rung you fish on
	UPROPERTY() TMap<FString, int32> Counts;
	UPROPERTY() bool bStarterKit = false;
	UPROPERTY() TMap<FString, FString> Wearing;
	UPROPERTY() TArray<FString> TreasureOpened;
	UPROPERTY() TMap<FString, FString> EstateBuilds;
	UPROPERTY() int64 Spins = 0;
	// the last pass: the tag registry (game/catch/tags.ts's offline registry), the town's services state
	// (sim/services.ts ServicesState, as key=number;...), the wrecks you have passed over (entities/life/wreck-chart.ts)
	UPROPERTY() TArray<FKeysTagged> Tags;
	UPROPERTY() TMap<FString, int64> Services;
	UPROPERTY() TArray<FString> WrecksFound;

	static constexpr int32 MaxTop = 12;

	// lbRecord: count it, keep the species record and the overall best, then write the slot
	void Record(const FString& Key, double Weight, const FString& Boat);
	// keeps the best session score; returns true if it was a new best (writes the slot)
	bool RecordScore(int64 Score);
	void Write();

	static UKeysSave* LoadOrCreate();
	static const TCHAR* SlotName() { return TEXT("KeysRunBoard"); }
};
