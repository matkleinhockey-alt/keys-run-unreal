#pragma once

#include "CoreMinimal.h"

class UKeysSave;

// The restaurants' menus and the Gutted Grouper's catch of the day (KeysEateries.cpp)
namespace KeysEateries
{
	// the species the player most recently landed in the last three days (its display name), or empty; OutKey gets its
	// key, OutWeight its weight (lb), OutDaysAgo 0 today / 1 yesterday...
	FString CatchOfTheDay(const UKeysSave* Sv, FString* OutKey, double* OutWeight = nullptr, int32* OutDaysAgo = nullptr);
	// the other recent species for the chalkboard ("ALSO FRESH: MAHI 12 - SNAPPER 6"), or empty
	FString AlsoFresh(const UKeysSave* Sv);
	// a menu to read: "grouper", "fancy" or "brew" (the MENU|<id> plaques); false if there's no such menu
	bool Menu(const FString& Id, const UKeysSave* Sv, FString& Title, FString& Sub, TArray<FString>& Lines, FString& Foot);
}
