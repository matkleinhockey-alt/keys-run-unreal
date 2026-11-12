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
		// the web game's HULLS[id].style / colors (content/boats.ts): what is fitted and in what paint
		FLinearColor EngAcc, EngLow, Cove, Uph;   // cowl stripe, lower unit, cove stripe (alpha 0 = none), upholstery
		bool bTTop = false;        // a canvas T-top on a tube frame rather than a hardtop
		bool bRadar = false;       // open-array radar on the top
		bool bRodRack = false;     // a row of rods along the aft edge of the top
		bool bBowRail = false;     // stainless bow rail
		bool bRiggerPoles = false; // fixed outrigger poles on the T-top (outriggers on a hull with no tower)
		bool bSunpad = false;      // bow sun pad (the go-fast boats)
		bool bLeaning = true;      // leaning post with a rocket launcher of rods (else helm seats)
		bool bBracket = false;     // engines on a bracket set back off the transom
		int32 Steps = 0;           // steps in the running bottom
		double Entry = .45;        // how fine the bow is (HULLS.entry)
		int64 Price = 0;           // dollars at the boat broker (sim/economy.ts BOAT_PRICE); 0 = always yours (the five original boats are grandfathered)
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
	int32 BoatIndexOf(const FString& Id);   // -1 if unknown
	// the hull you start in and can never lose (sim/economy.ts STARTER_BOAT)
	inline const TCHAR* StarterBoat() { return TEXT("grady"); }

	bool IsSounder(const FString& Key);
	bool IsBillfish(const FString& Key);
	// catch-and-release only; empty if it can be kept
	FString ReleaseOnlyReason(const FString& Key);
}
