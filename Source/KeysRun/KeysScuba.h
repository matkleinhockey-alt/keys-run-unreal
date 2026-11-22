#pragma once

#include "CoreMinimal.h"
#include "KeysDive.h"

struct FKeysMesh;
class AKeysBoatPawn;
namespace KeysData { struct FBoatSpec; }

// Scuba: the rules of breathing from a cylinder, the set's pieces and what the shop calls them, the cylinders'
// bookkeeping between dives, the logbook, and the meshes of everything (the set as worn, the gauge console in the
// hand, the rack and the diver-down flag on the boat). KeysDive.h still holds the breath-hold rules; a dive on scuba
// runs those same steps with three switches thrown (FState::bOnGas, bNeutral, AscendCap) and this file's gas, ascent
// and safety-stop rules round them. AKeysDiver (KeysDiver.cpp) is the only caller of the rules; the hub files read the
// shop rows, the loadout lines and the gauge numbers from here.
//
// The numbers, and why:
//   One unit throughout: psi. A full cylinder is 3,000 psi (an aluminium 80).
//   Resting at the surface she breathes 0.77 psi a second; the rate is that times the absolute pressure
//   (1 + depth / 10) times (1 + 0.8 x exertion), where steady finning is exertion 0.45 and a sprint is 1.
//   So a full cylinder, resting: 65 min at the surface, 50 min at 3 m, 43 min at 5 m, 32 min at 10 m, 19 min at
//   25 m, 16 min at 30 m (a third of the 3 m figure). Finning steadily costs 36% more, sprinting 80% more: a working
//   dive on a 10 m reef is about 24 minutes to empty, 20 to the 500 psi reserve. That is long enough to look round a
//   whole reef and short enough that the gauge matters.
namespace KeysScuba
{
	// ---- gas
	constexpr double FullPsi = 3000, ReservePsi = 500, CriticalPsi = 200;
	constexpr double SurfaceRate = .77;       // psi per second, resting, at 1 atmosphere
	constexpr double ExertionGain = .8;
	constexpr double PonyRatio = 80.0 / 13;   // the bail-out bottle is 13 cu ft against the cylinder's 80: its psi go that much faster
	// ---- swimming with the set on: against the freediver's long fins (KeysDiver.cpp gives her 1.6)
	constexpr double SwimK = 1.0;
	constexpr double SprintSecs = 7, SprintRest = 16, SprintAgain = .5;
	constexpr double NeutralFrom = .6;        // the jacket is trimmed below this depth; above it she floats, as on the surface
	// ---- coming up: the surface key swims at the safe rate, with sprint held it bolts
	constexpr double AscendSafe = .3, AscendBolt = 1.6;
	constexpr double AscendWarn = .36, AscendCost = .5;   // m/s: the computer complains above the first, it costs above the second
	constexpr double BentAt = 2, BentBadAt = 5, AcheSecs = 240;
	// ---- the safety stop: asked for after a dive past 18 m (or a loaded one), a minute between 3 and 6.5 m
	// (a real one is three minutes at 5 m; a third of that here, like the cylinder's time)
	constexpr double StopAskDepth = 18, StopTop = 3, StopBottom = 6.5, StopSecs = 60, StopAskLoad = .3;
	// ---- fish: a diver on scuba who is not sprinting reads as this much slower to the fish (they bolt from speed)
	constexpr double FishCalmK = .45;

	double GasRate(double Depth, double Exertion, double Mul = 1);                 // psi per second
	double SecondsLeft(double Psi, double Depth, double Exertion, double Mul = 1);

	enum class EEvent : uint8 { Reserve, Critical, Bailout, OutOfGas, StopAsked, StopDone, FastAscent, Bent, BentBad, Winded, BreathIn, BreathOut };

	struct FState
	{
		bool bOn = false;            // the set is on her back this dive
		bool bComputer = false;      // the wrist computer is owned: ascent rate, stop and time-left are shown
		double Psi = FullPsi, StartPsi = FullPsi;
		double RateMul = 1;          // aching from a bend 1.3, calm after a clean stop 0.9
		bool bBailAvail = false, bOnBailout = false, bOutOfGas = false;
		double BailPsi = 0;
		bool bReserveSaid = false, bCriticalSaid = false;
		// the sprint
		double Wind = 1;
		bool bWinded = false, bNoSprint = false;
		// breathing: one cycle 0..1, in for the first 0.38, out from 0.45 to 0.9
		double BreathPh = 0;
		bool bExhaling = false, bInhaling = false;
		// the dive
		double MaxDepth = 0, Secs = 0;
		double NLoad = 0;            // 0..1: nitrogen taken on below 9 m
		bool bStopAsked = false, bStopDone = false;
		double StopT = 0;
		double AscRate = 0, FastT = 0, Hit = 0;
		bool bFastNow = false, bFastSaid = false, bBent = false, bFastEver = false;
		double RateNow = 0;          // psi per second, smoothed (for the time-left figure)
		TArray<EEvent> Events;
		bool OnGas() const { return bOn && !bOutOfGas; }
		double GasFrac() const { return bOnBailout ? FMath::Clamp(BailPsi / FullPsi, 0.0, 1.0) : FMath::Clamp(Psi / FullPsi, 0.0, 1.0); }
	};

	// before KeysDive::Step: throws the switches on the diver's state and takes the sprint away when she is winded
	// (Events is reset here, so one fixed step's events are PreStep's and PostStep's together)
	void PreStep(FState& Sc, KeysDive::FState& D, KeysDive::FInput& In, double Dt);
	// after it: burns the gas, runs the breath, the ascent rate, the nitrogen and the stop
	void PostStep(FState& Sc, const KeysDive::FState& D, double Dt);
	// the one line the HUD shows under the gauge (empty when there is nothing to say); "SURFACE" leads the critical ones
	FString Warning(const FState& Sc, const KeysDive::FState& D);
	// the pure rules run without the game: logs KRScuba lines (time to empty by depth, two minutes at 5 and 25 m, hover)
	void SelfTest();

	// ---- the set, as the shop sells it (effects go in the pawn's Owned set like the rest of the dive gear)
	extern const TCHAR* const FxCylinder;     // dive.scuba.cyl
	extern const TCHAR* const FxJacket;       // dive.scuba.jacket
	extern const TCHAR* const FxRegs;         // dive.scuba.regs
	extern const TCHAR* const FxComputer;     // dive.scuba.computer
	extern const TCHAR* const FxPackage;      // dive.scuba.set: all four
	extern const TCHAR* const FxSpare;        // dive.scuba.cyl2: a second cylinder for the boat
	extern const TCHAR* const FxLight;        // dive.light
	extern const TCHAR* const FxFill;         // scuba.fill: not gear, fills every cylinder aboard
	struct FShopRow { const TCHAR* Id; const TCHAR* Name; const TCHAR* Blurb; int32 Price; const TCHAR* Effect; };
	const TArray<FShopRow>& ShopRows();
	// buying the package when pieces are already owned, a fill when everything is full, a spare with no first cylinder
	bool ShopRefuse(const AKeysBoatPawn* P, const FString& Effect, FString& Why);
	// what buying one of the rows does beyond owning it (a fill fills, the package marks its pieces); true if it was ours
	bool ShopResolve(AKeysBoatPawn* P, const FString& Effect, FString& Msg);

	bool Owns(const AKeysBoatPawn* P, const TCHAR* Effect);
	bool HasSet(const AKeysBoatPawn* P);                       // cylinder, jacket and regulators
	int32 Cylinders(const AKeysBoatPawn* P);                   // 0, 1 or 2
	double CylinderPsi(const AKeysBoatPawn* P, int32 Index);
	void SetCylinderPsi(AKeysBoatPawn* P, int32 Index, double Psi);
	int32 BestCylinder(const AKeysBoatPawn* P);
	// scuba or breath-hold, chosen on the boat before going over (kept for the session; scuba when the set is owned)
	bool WantScuba();
	void SetWantScuba(bool bScuba);
	// what J will do: true when the set is owned, scuba is chosen and a cylinder has gas in it
	bool WillDive(const AKeysBoatPawn* P);
	// the toast for the mode key on the boat (toggles the choice)
	FString ToggleMode(const AKeysBoatPawn* P);
	// the gear screen's lines (text, lit)
	void LoadoutRows(const AKeysBoatPawn* P, TArray<TPair<FString, bool>>& Out);

	// what a bend or a clean stop leaves behind for the next dive (kept for the session, wears off on the surface)
	double CarryRateMul(double Now);
	double CarryLoad(double Now);
	bool Aching(double Now);
	void LeaveCarry(double Now, double NLoad, bool bBent, bool bBad, bool bCleanStop);

	// ---- the logbook: a line a dive (site, depth, time, gas). Kept for the session; the totals go in the pawn's counts.
	struct FLog
	{
		int32 Number = 0;
		FString Site;
		double MaxDepth = 0, Secs = 0, PsiUsed = 0;
		bool bScuba = false, bStop = false, bFast = false;
	};
	const TArray<FLog>& Logbook();
	FString SiteNear(const AKeysBoatPawn* P, double X, double Z);
	FString AddLog(AKeysBoatPawn* P, FLog L);   // numbers it, returns its line
	FString LogLine(const FLog& L);

	// ---- the gauge, for the HUD (AKeysDiver::ScubaHud fills it)
	struct FHud
	{
		bool bOn = false;            // draw the gauge in place of the air bar
		bool bGas = true;            // false: out of gas, the breath bar is back
		FString Source;              // "Cylinder" / "Bail-out bottle"
		int32 Psi = 0;
		double Frac = 0;
		int32 MinLeft = -1;          // at this depth and effort; -1 without the computer
		bool bLow = false, bCritical = false;
		FString Dive;                // "12:40  max 18.2 m" (with the computer)
		FString Stop;                // the stop's countdown, when one is asked for
	};

	// ---- meshes (KeysScubaGear.cpp), metres
	struct FKitLook { bool bComputer = false, bPony = false, bLight = false; };
	// the set as worn, in the diver model's frame (+X to her head, +Z out of her back); returns its triangle count
	int32 BuildWornKit(FKeysMesh& M, const FKitLook& L);
	// the gauge console in the left hand (dive-mask view), its own frame: the face looks down -X at the eye, Z up
	void BuildConsole(FKeysMesh& M, bool bComputer);
	void BuildConsoleFace(FKeysMesh& M, double Psi, double Depth, int32 Mins, int32 MinLeft, bool bComputer);
	// the cockpit rack in the boat's own frame: bFirst a cylinder stands in the first slot (bRigged: with the jacket
	// and regulators on it, the whole set), bSpare the second cylinder beside it
	void BuildBoatRack(FKeysMesh& M, const KeysData::FBoatSpec& S, bool bFirst, bool bRigged, bool bSpare);
	// where the rack stands: the middle of its foot on the cockpit sole, in the boat's own frame
	FVector RackBase(const KeysData::FBoatSpec& S);
	// the diver-down flag on the rack's staff (T for the flutter)
	void BuildFlag(FKeysMesh& M, const KeysData::FBoatSpec& S, double T);
}
