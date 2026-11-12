#pragma once

#include "CoreMinimal.h"

// Species, fishing zones and boats, carried over from the three.js game.
namespace KeysData
{
	struct FSpecies
	{
		FString Key;
		FString Name;
		double Min, Max, Str, Mult;
		FLinearColor Color;
		bool bJump;
		FString Fact;
	};

	struct FBoatSpec
	{
		FString Id, Brand, Name, Power, DraftFt, Desc;
		double Len, Beam, Top, Accel, Turn, Draft;
		int32 Engines;
		bool bCat, bHardtop;
		FLinearColor Hull, Stripe, Bottom, Canvas, Engine, Deck;
		double CoolerCap;
		// buoyancy feel per hull
		double Spring;
	};

	struct FZoneRow
	{
		FString Key;
		double W;
	};

	const FSpecies* Species(const FString& Key);
	// every species key, in the JS SPECIES order
	const TArray<FString>& SpeciesKeys();
	const TArray<FZoneRow>& ZoneTable(const FString& Zone);
	const TArray<FBoatSpec>& Boats();

	bool IsSounder(const FString& Key);
	bool IsBillfish(const FString& Key);
	// catch-and-release only; empty if it can be kept
	FString ReleaseOnlyReason(const FString& Key);
}
