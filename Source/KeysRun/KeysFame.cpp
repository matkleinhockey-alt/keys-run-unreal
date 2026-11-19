#include "KeysFame.h"
#include "KeysSave.h"
#include "KeysData.h"
#include "KeysGeo.h"
#include "KeysWeather.h"
#include "KeysWorld.h"
#include "KeysTownData.h"

namespace KeysFame
{
	namespace
	{
		uint32 Mix32(uint32 X) { X ^= X >> 16; X *= 0x7feb352du; X ^= X >> 15; X *= 0x846ca68bu; X ^= X >> 16; return X; }
		FString Today() { return FDateTime::Now().ToString(TEXT("%Y-%m-%d")); }
		// whole days between a catch's date and today (0 today, 1 yesterday); big if unreadable
		int32 DaysAgo(const FString& Date)
		{
			int32 Y = 0, M = 0, D = 0;
			TArray<FString> P;
			Date.ParseIntoArray(P, TEXT("-"));
			if (P.Num() != 3) return 999;
			Y = FCString::Atoi(*P[0]); M = FCString::Atoi(*P[1]); D = FCString::Atoi(*P[2]);
			if (!FDateTime::Validate(Y, M, D, 0, 0, 0, 0)) return 999;
			const FDateTime N = FDateTime::Now();
			return int32((FDateTime(N.GetYear(), N.GetMonth(), N.GetDay()) - FDateTime(Y, M, D)).GetTotalDays());
		}
		FString Lb(double W) { return W >= 10 ? FString::Printf(TEXT("%d-pound"), FMath::RoundToInt(W)) : FString::Printf(TEXT("%.1f-pound"), W); }
		FString When(int32 Ago) { return Ago <= 0 ? TEXT("today") : Ago == 1 ? TEXT("yesterday") : TEXT("the other day"); }
		FString Lower(const FString& S) { return S.ToLower(); }
	}

	FString SpeciesName(const FString& Key)
	{
		const KeysData::FSpecies* Sp = KeysData::Species(Key);
		return Sp ? Sp->Name : Key;
	}

	// ------------------------------------------------------------------ the Wall of Fame
	void WallShots(const UKeysSave* Sv, int32 Max, TArray<FShot>& Out)
	{
		Out.Reset();
		if (!Sv) return;
		const FKeysBoardCatch* Newest = Sv->RecentCatches.Num() ? &Sv->RecentCatches.Last() : nullptr;
		auto IsNew = [&](const FKeysBoardCatch& C) { return Newest && Newest->Key == C.Key && FMath::Abs(Newest->Weight - C.Weight) < .001 && DaysAgo(C.Date) <= 1; };
		auto IsPB = [&](const FString& Key, double W) { const double* B = Sv->SpeciesBest.Find(Key); return B && FMath::Abs(*B - W) < .001; };
		// the heaviest four of all time
		for (int32 i = 0; i < Sv->TopCatches.Num() && Out.Num() < FMath::Min(4, Max); i++)
		{
			const FKeysBoardCatch& C = Sv->TopCatches[i];
			FShot S;
			S.Key = C.Key; S.Name = SpeciesName(C.Key); S.Date = C.Date; S.Weight = C.Weight;
			S.bPB = IsPB(C.Key, C.Weight); S.bNew = IsNew(C); S.Rank = i + 1;
			Out.Add(S);
		}
		// then the personal bests of the species not up there yet
		TArray<TPair<FString, double>> Pbs;
		for (const TPair<FString, double>& Kv : Sv->SpeciesBest) if (Kv.Value > 0) Pbs.Add(Kv);
		Pbs.Sort([](const TPair<FString, double>& A, const TPair<FString, double>& B) { return A.Value > B.Value; });
		for (const TPair<FString, double>& Kv : Pbs)
		{
			if (Out.Num() >= Max) break;
			if (Out.ContainsByPredicate([&](const FShot& S) { return S.Key == Kv.Key; })) continue;
			FShot S;
			S.Key = Kv.Key; S.Name = SpeciesName(Kv.Key); S.Weight = Kv.Value; S.bPB = true;
			// its date, if the catch is still on one of the lists
			for (const TArray<FKeysBoardCatch>* L : {&Sv->TopCatches, &Sv->RecentCatches})
				for (const FKeysBoardCatch& C : *L)
					if (C.Key == Kv.Key && FMath::Abs(C.Weight - Kv.Value) < .001) { S.Date = C.Date; S.bNew = IsNew(C); }
			Out.Add(S);
		}
	}

	uint32 WallSig(const UKeysSave* Sv)
	{
		if (!Sv) return 0;
		uint32 H = Mix32(uint32(Sv->Count) + 1);
		for (const FKeysBoardCatch& C : Sv->TopCatches) H = Mix32(H ^ GetTypeHash(C.Key) ^ uint32(C.Weight * 100));
		for (const TPair<FString, double>& Kv : Sv->SpeciesBest) H ^= Mix32(GetTypeHash(Kv.Key) ^ uint32(Kv.Value * 100));
		// the NEW pin comes off the day after
		H ^= Mix32(GetTypeHash(Today()));
		return H;
	}

	// ------------------------------------------------------------------ the derby
	void Derby(const UKeysSave* Sv, TArray<FDerbyRow>& Out, FString& Title)
	{
		Out.Reset();
		const FDateTime N = FDateTime::Now();
		static const TCHAR* const Months[] = {TEXT("JANUARY"), TEXT("FEBRUARY"), TEXT("MARCH"), TEXT("APRIL"), TEXT("MAY"), TEXT("JUNE"), TEXT("JULY"),
			TEXT("AUGUST"), TEXT("SEPTEMBER"), TEXT("OCTOBER"), TEXT("NOVEMBER"), TEXT("DECEMBER")};
		Title = FString::Printf(TEXT("%s %d"), Months[FMath::Clamp(N.GetMonth() - 1, 0, 11)], N.GetYear());
		// the locals (all invented), each with a few fish on random days of the month: the board shows their best so far
		static const TCHAR* const Locals[] = {TEXT("Capt. Dottie Fenwick"), TEXT("Skeeter Boudreaux"), TEXT("Marisol Vega"), TEXT("Old Hank Tull"),
			TEXT("Rae-Ann Pickett"), TEXT("Capt. Lou Mancuso"), TEXT("Tiny Delacroix"), TEXT("Junie Bascomb"), TEXT("Dutch Van Esse"),
			TEXT("Pearl Oyelaran"), TEXT("Buck Raynor"), TEXT("Sunny Kealoha")};
		struct FFish { const TCHAR* Name; double Lo, Hi; };
		static const FFish Fish[] = {{TEXT("Tarpon"), 55, 135}, {TEXT("Goliath Grouper"), 90, 310}, {TEXT("Wahoo"), 24, 62}, {TEXT("Mahi"), 14, 46},
			{TEXT("Amberjack"), 28, 72}, {TEXT("Kingfish"), 14, 41}, {TEXT("Permit"), 14, 31}, {TEXT("Snook"), 9, 26}, {TEXT("Cobia"), 20, 55},
			{TEXT("Black Grouper"), 18, 58}, {TEXT("Sailfish"), 38, 72}, {TEXT("Barracuda"), 12, 38}};
		const uint32 Seed = Mix32(uint32(N.GetYear() * 13 + N.GetMonth()));
		const int32 Day = N.GetDay();
		for (int32 i = 0; i < int32(UE_ARRAY_COUNT(Locals)); i++)
		{
			uint32 R = Mix32(Seed ^ uint32(i * 0x9e37));
			auto F = [&R]() { R = Mix32(R + 0x6d2b79f5u); return (R & 0xffffff) / double(0x1000000); };
			FDerbyRow Best;
			const int32 Tries = 2 + int32(F() * 4);
			for (int32 t = 0; t < Tries; t++)
			{
				const int32 On = 1 + int32(F() * 28);
				const int32 NF = int32(UE_ARRAY_COUNT(Fish));
				const FFish& Fs = Fish[int32(F() * NF) % NF];
				// mostly middling fish; a few monsters
				const double K = FMath::Pow(F(), 1.7);
				const double W = FMath::RoundToDouble((Fs.Lo + (Fs.Hi - Fs.Lo) * K) * 10) / 10;
				if (On > Day) continue;
				if (W > Best.Weight) { Best.Name = Locals[i]; Best.Species = Fs.Name; Best.Weight = W; }
			}
			if (Best.Weight > 0) Out.Add(Best);
		}
		// you, with your best fish of the month
		if (Sv && Sv->MonthBest.Weight > 0 && Sv->MonthBest.Date.Left(7) == N.ToString(TEXT("%Y-%m")))
		{
			FDerbyRow Me;
			Me.Name = Sv->OnlineName.IsEmpty() ? FString(TEXT("YOU")) : Sv->OnlineName.ToUpper() + TEXT(" (YOU)");
			Me.Species = SpeciesName(Sv->MonthBest.Key);
			Me.Weight = Sv->MonthBest.Weight;
			Me.bMe = true;
			Out.Add(Me);
		}
		Out.Sort([](const FDerbyRow& A, const FDerbyRow& B) { return A.Weight > B.Weight; });
		for (int32 i = 0; i < Out.Num(); i++) Out[i].Rank = i + 1;
	}

	// ------------------------------------------------------------------ talk
	FWx WeatherNow(const UObject* Ctx)
	{
		FWx W;
		W.Wind = KeysGeo::WindSpeed();
		if (const AKeysWeather* Wx = AKeysWeather::Get(Ctx)) W.Rain = Wx->RainNow();
		W.Dusk = AKeysWorld::Dusk(Ctx);
		TArray<KeysWx::FCellView> Cells;
		KeysWx::Cells(Cells);
		const FVector T = KeysTown::WorldPos(0, 60, KeysTown::PadY);
		for (const KeysWx::FCellView& C : Cells)
		{
			if (C.Build < .3) continue;
			W.StormKm = FMath::Min(W.StormKm, FMath::Max(0.0, FMath::Sqrt(FMath::Square(C.X - T.X) + FMath::Square(C.Z - T.Z)) - C.R) / 1000);
		}
		return W;
	}

	FString Bark(const UKeysSave* Sv, const FWx& Wx, int32 Pick)
	{
		const int32 Pk = FMath::Abs(Pick);
		// the fish people have heard about: the biggest of the last few days
		const FKeysBoardCatch* Big = nullptr;
		int32 Ago = 999;
		if (Sv)
			for (const FKeysBoardCatch& C : Sv->RecentCatches)
			{
				const int32 A = DaysAgo(C.Date);
				if (A <= 3 && (!Big || C.Weight > Big->Weight)) { Big = &C; Ago = A; }
			}
		const bool bTalkFish = Big && (Pk % 3 != 2);
		if (bTalkFish)
		{
			const FString Sp = Lower(SpeciesName(Big->Key)), W = Lb(Big->Weight);
			const double* Pb = Sv->SpeciesBest.Find(Big->Key);
			const bool bPB = Pb && FMath::Abs(*Pb - Big->Weight) < .001;
			const bool bTop = Sv->TopCatches.Num() && FMath::Abs(Sv->TopCatches[0].Weight - Big->Weight) < .001 && Sv->TopCatches[0].Key == Big->Key;
			TArray<FString> L = {
				FString::Printf(TEXT("Heard you got a %s %s!"), *W, *Sp),
				FString::Printf(TEXT("That %s you brought in %s? Get outta here."), *Sp, *When(Ago)),
				FString::Printf(TEXT("A %s %s! Whole dock's talking about it."), *W, *Sp),
				FString::Printf(TEXT("You're the one with the %s, right? Nice fish."), *Sp),
				FString::Printf(TEXT("%s %s... my cousin says he saw bigger. He didn't."), *W, *Sp),
				FString::Printf(TEXT("Saw your %s on the Wall of Fame!"), *Sp),
			};
			if (bPB) L.Add(FString::Printf(TEXT("Personal best %s, I hear. Drinks are on you."), *Sp));
			if (bTop) L.Add(FString::Printf(TEXT("Biggest fish you've ever had, that %s?"), *Sp));
			if (Ago == 0) L.Add(FString::Printf(TEXT("Grouper's frying %s tonight - yours, they say."), *Sp));
			return L[Pk % L.Num()];
		}
		// no fish yet (or it's a weather day): the weather
		TArray<FString> L;
		if (Wx.Rain > .25) L.Append({TEXT("Get under cover - it's coming down!"), TEXT("Rain like this, the tarpon don't care. I do."), TEXT("Should've stayed in the Brew.")});
		else if (Wx.StormKm < 6) L.Append({TEXT("Storm building out past the reef. Watch it."), TEXT("See that cloud? Lightning in an hour."), TEXT("Squall's coming. Get your boat tied up.")});
		else if (Wx.Wind > 9.5) L.Append({TEXT("Blowing like stink. Reef'll be rough."), TEXT("Small craft weather. Stay inside the reef."), TEXT("Trades are howling. Fish the lee.")});
		else if (Wx.Wind < 4.5) L.Append({TEXT("Flat calm. Perfect day for the flats."), TEXT("Glass out there. Go find a permit."), TEXT("No wind - the no-see-ums'll eat you alive.")});
		else L.Append({TEXT("Nice breeze. Good day to be on the water."), TEXT("Trades are steady. Mahi weather."), TEXT("Pretty day. Shame to spend it ashore.")});
		if (Wx.Dusk > .55) L.Add(TEXT("Sun's going down - tarpon'll be rolling at the bridge."));
		if (!Big) L.Append({TEXT("Catch anything yet? Wall of Fame's waiting."), TEXT("Bring in something big - the derby's wide open.")});
		return L[Pk % L.Num()];
	}
}
