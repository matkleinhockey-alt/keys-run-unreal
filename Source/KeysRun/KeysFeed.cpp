#include "KeysFeed.h"
#include "KeysWorld.h"
#include "Misc/DateTime.h"

namespace
{
	double GClock = -1;

	// a feeding window: centred on Hour, Half hours either side, raising the bite by Boost at its peak
	struct FWin { double Hour, Half, Boost; };
	struct FHabit { TArray<FWin> W; const TCHAR* Text; const TCHAR* Short; };

	enum class EHab : uint8 { Pelagic, Mahi, LowLight, Sword, Shark, Snapper, Inshore, Reef, Flats, Jack, General };

	EHab HabitOf(const FString& K)
	{
		static const TMap<FString, EHab> M = []
		{
			TMap<FString, EHab> R;
			for (const TCHAR* S : {TEXT("wahoo"), TEXT("sailfish"), TEXT("blackfin"), TEXT("yellowfin"), TEXT("bluefin"), TEXT("albacore"), TEXT("skipjack"),
				TEXT("bigeye"), TEXT("kingfish"), TEXT("cero"), TEXT("spanishmack"), TEXT("marlin"), TEXT("blackmarlin"), TEXT("mako"), TEXT("bluerunner")})
				R.Add(S, EHab::Pelagic);
			R.Add(TEXT("mahi"), EHab::Mahi);
			for (const TCHAR* S : {TEXT("tarpon"), TEXT("snook")}) R.Add(S, EHab::LowLight);
			R.Add(TEXT("swordfish"), EHab::Sword);
			for (const TCHAR* S : {TEXT("blacktip"), TEXT("bullshark"), TEXT("lemonshark"), TEXT("hammerhead"), TEXT("tigershark")}) R.Add(S, EHab::Shark);
			for (const TCHAR* S : {TEXT("mangrove"), TEXT("cubera")}) R.Add(S, EHab::Snapper);
			for (const TCHAR* S : {TEXT("redfish"), TEXT("trout"), TEXT("ladyfish"), TEXT("jackcrevalle"), TEXT("barracuda")}) R.Add(S, EHab::Inshore);
			for (const TCHAR* S : {TEXT("grouper"), TEXT("gag"), TEXT("redgrouper"), TEXT("warsaw"), TEXT("goliath"), TEXT("mutton"), TEXT("yellowtail"),
				TEXT("lanesnapper"), TEXT("schoolmaster"), TEXT("vermilion"), TEXT("hogfish"), TEXT("graytrigger"), TEXT("lionfish"), TEXT("sheepshead"),
				TEXT("tripletail"), TEXT("flounder"), TEXT("sole"), TEXT("halibut"), TEXT("lookdown")})
				R.Add(S, EHab::Reef);
			for (const TCHAR* S : {TEXT("bonefish"), TEXT("permit"), TEXT("pompano"), TEXT("palometa")}) R.Add(S, EHab::Flats);
			for (const TCHAR* S : {TEXT("amberjack"), TEXT("cobia")}) R.Add(S, EHab::Jack);
			return R;
		}();
		const EHab* H = M.Find(K);
		return H ? *H : EHab::General;
	}

	const FHabit& Habit(EHab H)
	{
		static const FHabit Hs[] = {
			// Pelagic: the offshore blue-water fish and the mackerels hunt hardest at first light, again at dusk
			{{{7.0, 2.0, .6}, {18.5, 1.6, .4}}, TEXT("Feeds hardest at first light (about 5:30 - 9 am), again toward dusk."), TEXT("dawn")},
			// Mahi: an early bite too, but they keep eating round the weedlines through the day
			{{{7.0, 2.0, .55}, {12.5, 3.0, .15}, {18.5, 1.6, .35}}, TEXT("Best early (about 5:30 - 9 am), still eating round the weed through the day."), TEXT("early")},
			// LowLight: tarpon and snook roll at dawn and dusk and feed on into the night
			{{{6.5, 1.8, .5}, {19.5, 2.0, .6}, {1.0, 3.0, .45}}, TEXT("Best at dawn, dusk and after dark (about 5 - 8 am, 6 - 9:30 pm and on into the night)."), TEXT("dusk/night")},
			// Sword: they come up from the deep to feed at night; by day it is a deep drop
			{{{0.0, 4.5, .6}, {19.8, 1.6, .3}}, TEXT("Rises to feed at night (about 9 pm - 4 am); by day, drop deep for it."), TEXT("night")},
			// Shark: dusk and night
			{{{19.5, 2.0, .5}, {0.5, 3.0, .45}, {6.5, 1.5, .25}}, TEXT("Hunts at dusk and through the night."), TEXT("dusk/night")},
			// Snapper: mangroves and cuberas feed at dusk and at night
			{{{19.5, 2.0, .45}, {0.5, 3.0, .4}, {6.5, 1.5, .3}}, TEXT("Best at dusk and after dark, with a bite at dawn."), TEXT("dusk")},
			// Inshore: redfish, trout, jacks, ladyfish and 'cuda on the moving water at the ends of the day
			{{{7.0, 1.8, .5}, {18.5, 1.6, .45}}, TEXT("Best early morning and late afternoon (about 5:30 - 9 am, 5 - 8 pm)."), TEXT("dawn/dusk")},
			// Reef: groupers, snappers and the rest of the reef bite steadily, a little better in the morning
			{{{8.0, 2.0, .45}, {17.5, 1.5, .3}}, TEXT("Bites steadily all day, best in the morning (about 7 - 9 am)."), TEXT("morning")},
			// Flats: bonefish, permit and pompano tail on the morning flats
			{{{8.0, 2.2, .45}, {17.0, 1.5, .3}}, TEXT("Tails on the flats all day, best on the morning water (about 7 - 9:30 am)."), TEXT("morning")},
			// Jack: amberjack and cobia: a morning bite and a decent one at midday
			{{{8.0, 2.0, .4}, {12.5, 2.0, .3}}, TEXT("Bites through the day, best mid-morning and around noon."), TEXT("midday")},
			// General
			{{{7.0, 2.0, .45}, {18.5, 1.6, .45}}, TEXT("Best at dawn and dusk."), TEXT("dawn/dusk")},
		};
		return Hs[uint8(H)];
	}

	double Bump(double Hour, const FWin& W)
	{
		double D = FMath::Abs(Hour - W.Hour);
		D = FMath::Min(D, 24 - D);
		return D < W.Half ? W.Boost * (.5 + .5 * FMath::Cos(PI * D / W.Half)) : 0;
	}

	constexpr double AreaFeedAt = 1.2;
}

namespace KeysFeed
{
	void SetClockOverride(double Hour) { GClock = Hour < 0 ? -1 : FMath::Fmod(Hour, 24.0); }

	double HourNow(const UObject* WorldContext)
	{
		if (GClock >= 0) return GClock;
		if (const AKeysWorld* W = WorldContext ? AKeysWorld::Get(WorldContext) : nullptr; W && W->IsSunset()) return 18.75;   // the sunset is dusk
		const FDateTime N = FDateTime::Now();
		return N.GetHour() + N.GetMinute() / 60.0 + N.GetSecond() / 3600.0;
	}

	double Mul(const FString& Key, double Hour)
	{
		double B = 0;
		for (const FWin& W : Habit(HabitOf(Key)).W) B = FMath::Max(B, Bump(Hour, W));
		return 1 + B;
	}

	double AreaMul(const TArray<KeysData::FZoneRow>& Tbl, double Hour)
	{
		double S = 0, Wt = 0;
		for (const KeysData::FZoneRow& R : Tbl) { S += R.W * Mul(R.Key, Hour); Wt += R.W; }
		return Wt > 0 ? S / Wt : 1;
	}

	FString ClockText(double Hour)
	{
		int32 Mins = FMath::RoundToInt32(FMath::Fmod(Hour + 24, 24.0) * 12) * 5 % (24 * 60);
		const int32 H24 = Mins / 60, M = Mins % 60, H12 = H24 % 12 == 0 ? 12 : H24 % 12;
		return FString::Printf(TEXT("%d:%02d %s"), H12, M, H24 < 12 ? TEXT("am") : TEXT("pm"));
	}

	FString BiteLine(const TArray<KeysData::FZoneRow>& Tbl, double Hour)
	{
		const bool bNow = AreaMul(Tbl, Hour) >= AreaFeedAt;
		// step forward five minutes at a time to where it changes
		for (int32 k = 1; k <= 24 * 12; k++)
		{
			const double H = FMath::Fmod(Hour + k / 12.0, 24.0);
			if ((AreaMul(Tbl, H) >= AreaFeedAt) != bNow)
				return bNow ? FString::Printf(TEXT("Bite: Feeding! (until %s)"), *ClockText(H)) : FString::Printf(TEXT("Bite: Normal - feeding picks up around %s"), *ClockText(H));
		}
		return bNow ? FString(TEXT("Bite: Feeding!")) : FString(TEXT("Bite: Normal"));
	}

	FString BestTimes(const FString& Key) { return Habit(HabitOf(Key)).Text; }
	FString ShortBest(const FString& Key) { return Habit(HabitOf(Key)).Short; }
}
