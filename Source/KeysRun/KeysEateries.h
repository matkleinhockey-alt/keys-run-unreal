#pragma once

#include "CoreMinimal.h"

class UKeysSave;

// The restaurants' menus and the Gutted Grouper's catch of the day (KeysEateries.cpp)
namespace KeysEateries
{
	// the species the player most recently landed today (its display name), or empty; OutKey gets its key
	FString CatchOfTheDay(const UKeysSave* Sv, FString* OutKey);
	// a menu to read: "grouper", "fancy" or "brew" (the MENU|<id> plaques); false if there's no such menu
	bool Menu(const FString& Id, const UKeysSave* Sv, FString& Title, FString& Sub, TArray<FString>& Lines, FString& Foot);
}
