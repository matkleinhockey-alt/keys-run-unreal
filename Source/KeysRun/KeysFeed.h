#pragma once

#include "CoreMinimal.h"
#include "KeysData.h"

class UObject;

// When the fish eat: each species has feeding windows by the hour of the player's real local clock (Florida Keys
// habits: dawn and dusk for most, the pelagics strongest early, tarpon, snook and swordfish at night, the reef and the
// flats steadier with a morning bump). A window only ever raises the bite: outside one every species bites at the
// game's ordinary rate (1.0x), inside one up to about 1.6x. Sunset mode (O) counts as dusk whatever the clock says.
namespace KeysFeed
{
	// the hour (0 - 24) feeding goes by: the clock override (-KRScript clock=HH:MM), else dusk while the sunset is on,
	// else the real local time
	double HourNow(const UObject* WorldContext);
	void SetClockOverride(double Hour);   // < 0 turns it off

	// this species' bite multiplier at that hour: 1.0 outside its windows, up to ~1.6 at a peak
	double Mul(const FString& Key, double Hour);
	// the same, averaged over a zone's species by how often each turns up there
	double AreaMul(const TArray<KeysData::FZoneRow>& Tbl, double Hour);
	// in a feeding window (the multiplier is up by a quarter or more)
	inline bool IsFeeding(double M) { return M >= 1.25; }

	// "Bite: Feeding! (until 8:40 am)" / "Bite: Normal - feeding picks up around 6:15 pm"
	FString BiteLine(const TArray<KeysData::FZoneRow>& Tbl, double Hour);
	// for the species card: "Feeds best at dawn and dusk (about 5:30 - 9 am and 5 - 8 pm)."
	FString BestTimes(const FString& Key);
	// a word or two for the species list: "dawn", "dusk/night", "morning", ...
	FString ShortBest(const FString& Key);
	// "6:15 pm"
	FString ClockText(double Hour);
}
