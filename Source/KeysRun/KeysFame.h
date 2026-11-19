#pragma once

#include "CoreMinimal.h"

class UKeysSave;

// The town reacting to your fishing (the words; KeysTownFame.cpp builds the boards and the critters):
//  - the Wall of Fame at the marina: polaroids of your heaviest fish and your personal bests
//  - the Boot Key Brag-Off, the town's monthly heaviest-fish derby: invented locals' catches (seeded by the month, a few
//    more of them each day) with your best of the month in among them; live standings while a tournament runs
//  - what people say when you walk past: your recent catches ("heard you got a 40-pound wahoo!") and the weather
namespace KeysFame
{
	// one polaroid
	struct FShot { FString Key, Name, Date; double Weight = 0; bool bPB = false, bNew = false; int32 Rank = 0; };
	// up to Max: the heaviest four of all time, then personal bests of other species, heaviest first
	void WallShots(const UKeysSave* Sv, int32 Max, TArray<FShot>& Out);
	// a cheap fingerprint of what the wall shows, to rebuild it only when it changes
	uint32 WallSig(const UKeysSave* Sv);

	struct FDerbyRow { FString Name, Species; double Weight = 0; bool bMe = false; int32 Rank = 0; };
	// this month's standings, heaviest first (the player in among the locals); Title like "NOVEMBER 2026"
	void Derby(const UKeysSave* Sv, TArray<FDerbyRow>& Out, FString& Title);

	// the weather as people talk about it
	struct FWx { double Wind = 7, Rain = 0, StormKm = 99, Dusk = 0; };
	FWx WeatherNow(const UObject* Ctx);
	// a line for someone you walk past; Pick varies it. Talks about your catches (if you have any) or the weather.
	FString Bark(const UKeysSave* Sv, const FWx& Wx, int32 Pick);

	FString SpeciesName(const FString& Key);
}
