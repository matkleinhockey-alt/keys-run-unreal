#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "KeysSlots.generated.h"

// The Gaff & Gill's slot machines, ported from the web game's shared/sim/slots.ts (three reels of 24 fish symbols,
// three rows, five fixed lines, the paytable - three shrimp 8x the line bet up to three marlin 1000x, a pair of shrimp
// on the left pays the line back - bets $5 to $100, a seeded draw from the per-save spin counter, 90.5% back) and
// game/casino/** with sim/reels.ts (the machine on screen: spin up, a run, an eased stop with a little overshoot, the
// tease on the third reel, the winning lines lit, the payout settled when the reels stop).
UCLASS()
class KEYSRUN_API AKeysSlots : public AActor
{
	GENERATED_BODY()

public:
	AKeysSlots();
	virtual void Tick(float DeltaSeconds) override;
	static AKeysSlots* Get(const UObject* WorldContext);

	static constexpr int32 Reels = 3, Rows = 3, ReelLen = 24;
	static const int32 Strips[3][24];
	static const int32 Lines[5][3];
	static const TCHAR* SymbolName(int32 S);
	static FLinearColor SymbolColor(int32 S);
	static const int32 BetLadder[5];

	bool bOpen = false;
	int32 BetI = 1;
	int32 Stops[3] = {0, 0, 0};
	double ReelPos[3] = {0, 0, 0};   // fractional strip position on show, for the spin
	bool bSpinning = false;
	double SpinT = 0;
	int32 LastWin = 0, Pending = 0;
	TArray<int32> WinLines;
	FString ResultText;
	int64 Wagered = 0, Won = 0;
	double Rtp() const;

	void Open(bool bOn);
	void Spin();
	void HandleInput(class APlayerController* PC);
	int32 Symbol(int32 Reel, int32 Row) const;   // what is in the window, as it stands now

private:
	double LastSpinT = -99;
	int32 FromStops[3] = {0, 0, 0};
	int32 Tease = -1;
};
