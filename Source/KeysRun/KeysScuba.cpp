// Scuba: the rules, the set's bookkeeping, the shop's words and the logbook (see KeysScuba.h for the numbers and why).
// The meshes are in KeysScubaGear.cpp.
#include "KeysScuba.h"
#include "KeysBoatPawn.h"
#include "KeysGeo.h"

namespace
{
	double ScClamp(double V, double A, double B) { return V < A ? A : V > B ? B : V; }
	FString ScCommas(int32 N)
	{
		FString S = FString::FromInt(FMath::Abs(N));
		for (int32 i = S.Len() - 3; i > 0; i -= 3) S.InsertAt(i, TEXT(","));
		return N < 0 ? TEXT("-") + S : S;
	}
	FString ScMinSec(double Secs) { const int32 T = FMath::Max(0, FMath::RoundToInt(Secs)); return FString::Printf(TEXT("%d:%02d"), T / 60, T % 60); }

	const TCHAR* const KeyUsed[2] = {TEXT("scuba.used1"), TEXT("scuba.used2")};
	bool GScubaWant = true;

	struct FScCarry { double AcheUntil = -1, At = 0, NLoad = 0; bool bCalm = false; };
	FScCarry GScubaCarry;
	TArray<KeysScuba::FLog> GScubaLog;
}

namespace KeysScuba
{
	const TCHAR* const FxCylinder = TEXT("dive.scuba.cyl");
	const TCHAR* const FxJacket = TEXT("dive.scuba.jacket");
	const TCHAR* const FxRegs = TEXT("dive.scuba.regs");
	const TCHAR* const FxComputer = TEXT("dive.scuba.computer");
	const TCHAR* const FxPackage = TEXT("dive.scuba.set");
	const TCHAR* const FxSpare = TEXT("dive.scuba.cyl2");
	const TCHAR* const FxLight = TEXT("dive.light");
	const TCHAR* const FxFill = TEXT("scuba.fill");

	// ================================================================ the rules
	double GasRate(double Depth, double Exertion, double Mul)
	{
		return SurfaceRate * KeysDive::AtaAt(FMath::Max(0.0, Depth)) * (1 + ScClamp(Exertion, 0, 1) * ExertionGain) * Mul;
	}

	double SecondsLeft(double Psi, double Depth, double Exertion, double Mul)
	{
		return FMath::Max(0.0, Psi) / FMath::Max(1e-6, GasRate(Depth, Exertion, Mul));
	}

	void PreStep(FState& Sc, KeysDive::FState& D, KeysDive::FInput& In, double Dt)
	{
		Sc.Events.Reset();
		if (!Sc.bOn) { D.bOnGas = D.bNeutral = false; D.AscendCap = D.AscendCapSprint = 0; return; }
		const double Depth = D.Depth();
		// out of gas the jacket, the gas and the slow ascent all go: it is a breath-hold from that moment
		D.bOnGas = !Sc.bOutOfGas;
		D.bNeutral = !Sc.bOutOfGas && Depth > NeutralFrom;
		D.AscendCap = Sc.bOutOfGas ? 0 : AscendSafe;
		D.AscendCapSprint = Sc.bOutOfGas ? 0 : AscendBolt;
		// a sprint in all that kit does not last: seven seconds, then she is winded until half of it is back
		const bool bKick = In.bFwd || In.bBack || In.bLeft || In.bRight || In.bAscend || In.bDescend;
		if (Sc.bNoSprint || Sc.bWinded) In.bSprint = false;
		if (In.bSprint && bKick)
		{
			Sc.Wind -= Dt / SprintSecs;
			if (Sc.Wind <= 0) { Sc.Wind = 0; Sc.bWinded = true; Sc.Events.Add(EEvent::Winded); }
		}
		else
		{
			Sc.Wind = FMath::Min(1.0, Sc.Wind + Dt / SprintRest);
			if (Sc.bWinded && Sc.Wind >= SprintAgain) Sc.bWinded = false;
		}
	}

	void PostStep(FState& Sc, const KeysDive::FState& D, double Dt)
	{
		if (!Sc.bOn) return;
		const double Depth = D.Depth();
		const bool bUnder = Depth > KeysDive::SurfaceEps;
		if (bUnder) Sc.Secs += Dt;
		Sc.MaxDepth = FMath::Max(Sc.MaxDepth, Depth);

		// ---- the gas: nothing is drawn with her head out of the water
		double Rate = 0;
		if (bUnder && !Sc.bOutOfGas && !D.bBlackedOut)
		{
			Rate = GasRate(Depth, D.Exertion, Sc.RateMul);
			if (Sc.bOnBailout)
			{
				Sc.BailPsi -= Rate * PonyRatio * Dt;
				if (Sc.BailPsi <= 0) { Sc.BailPsi = 0; Sc.bOutOfGas = true; Sc.Events.Add(EEvent::OutOfGas); }
			}
			else
			{
				Sc.Psi -= Rate * Dt;
				if (Sc.Psi <= 0)
				{
					Sc.Psi = 0;
					if (Sc.bBailAvail) { Sc.bBailAvail = false; Sc.bOnBailout = true; Sc.BailPsi = FullPsi; Sc.Events.Add(EEvent::Bailout); }
					else { Sc.bOutOfGas = true; Sc.Events.Add(EEvent::OutOfGas); }
				}
				else if (Sc.Psi < CriticalPsi && !Sc.bCriticalSaid) { Sc.bCriticalSaid = Sc.bReserveSaid = true; Sc.Events.Add(EEvent::Critical); }
				else if (Sc.Psi < ReservePsi && !Sc.bReserveSaid) { Sc.bReserveSaid = true; Sc.Events.Add(EEvent::Reserve); }
			}
		}
		Sc.RateNow += (Rate - Sc.RateNow) * FMath::Min(1.0, Dt * .5);

		// ---- the breath: slow at rest, quick when working; the regulator only bubbles on the way out
		const bool bWasIn = Sc.bInhaling, bWasOut = Sc.bExhaling;
		if (bUnder && !Sc.bOutOfGas && !D.bBlackedOut)
		{
			const double Period = FMath::Lerp(5.2, 2.6, ScClamp(D.Exertion, 0, 1));
			Sc.BreathPh = FMath::Fmod(Sc.BreathPh + Dt / Period, 1.0);
			Sc.bInhaling = Sc.BreathPh < .38;
			Sc.bExhaling = Sc.BreathPh >= .45 && Sc.BreathPh < .9;
		}
		else { Sc.BreathPh = 0; Sc.bInhaling = Sc.bExhaling = false; }
		if (Sc.bInhaling && !bWasIn) Sc.Events.Add(EEvent::BreathIn);
		if (Sc.bExhaling && !bWasOut) Sc.Events.Add(EEvent::BreathOut);

		// ---- coming up: the rate, the warning, and what a bolt for the surface costs
		Sc.AscRate += (FMath::Max(0.0, D.VY) - Sc.AscRate) * FMath::Min(1.0, Dt * 2);
		Sc.bFastNow = Depth > 1.5 && Sc.AscRate > AscendWarn;
		if (Sc.bFastNow)
		{
			Sc.FastT += Dt;
			if (!Sc.bFastSaid && Sc.FastT > 1) { Sc.bFastSaid = Sc.bFastEver = true; Sc.Events.Add(EEvent::FastAscent); }
		}
		else
		{
			Sc.FastT = 0;
			if (Sc.AscRate < .2) Sc.bFastSaid = false;
		}
		if (Depth > 1.5 && Sc.AscRate > AscendCost) Sc.Hit += (Sc.AscRate - AscendCost) * (.25 + Sc.NLoad) * Dt;
		else Sc.Hit = FMath::Max(0.0, Sc.Hit - .03 * Dt);
		if (!Sc.bBent && Sc.Hit >= BentAt) { Sc.bBent = true; Sc.bNoSprint = true; Sc.RateMul = FMath::Max(Sc.RateMul, 1.3); Sc.Events.Add(EEvent::Bent); }
		if (Sc.bBent && Sc.Hit >= BentBadAt && Sc.RateMul < 1.5) { Sc.RateMul = 1.5; Sc.Events.Add(EEvent::BentBad); }

		// ---- nitrogen: taken on below 9 m (full in ten minutes at 30 m, twenty-three at 18 m), given back above it
		const bool bInStop = Depth >= StopTop && Depth <= StopBottom;
		if (Depth > 9) Sc.NLoad = FMath::Min(1.0, Sc.NLoad + (KeysDive::AtaAt(Depth) - 1.9) / (2.1 * 600) * Dt);
		else Sc.NLoad = FMath::Max(0.0, Sc.NLoad - Dt / (bInStop ? 240.0 : 400.0));

		// ---- the safety stop: asked for on the way up past 10 m, a minute in the zone does it
		if (!Sc.bStopAsked && !Sc.bStopDone && (Sc.MaxDepth >= StopAskDepth || Sc.NLoad >= StopAskLoad) && Depth < 10 && bUnder)
		{
			Sc.bStopAsked = true;
			Sc.Events.Add(EEvent::StopAsked);
		}
		if (Sc.bStopAsked && !Sc.bStopDone && bInStop)
		{
			Sc.StopT += Dt;
			if (Sc.StopT >= StopSecs) { Sc.bStopDone = true; Sc.NLoad = 0; if (!Sc.bBent) Sc.Hit = 0; Sc.Events.Add(EEvent::StopDone); }
		}
	}

	FString Warning(const FState& Sc, const KeysDive::FState& D)
	{
		if (!Sc.bOn || Sc.bOutOfGas) return FString();
		const double Depth = D.Depth();
		if (Sc.bFastNow) return TEXT("SLOW DOWN - you are coming up too fast");
		if (Sc.bOnBailout) return FString::Printf(TEXT("SURFACE - on the bail-out bottle, %s psi"), *ScCommas(FMath::RoundToInt(Sc.BailPsi)));
		if (Sc.Psi < CriticalPsi && Depth > 1) return FString::Printf(TEXT("SURFACE - %d psi left"), FMath::RoundToInt(Sc.Psi));
		if (Sc.bComputer && Sc.bStopAsked && !Sc.bStopDone)
		{
			if (Depth > StopBottom) return TEXT("Safety stop ahead - level off at 5 m for a minute");
			if (Depth >= StopTop) return FString::Printf(TEXT("Safety stop - hold here %s"), *ScMinSec(StopSecs - Sc.StopT));
			if (Depth > KeysDive::SurfaceEps) return TEXT("Safety stop skipped - drop back to 5 m, or call it a day");
		}
		if (Sc.Psi < ReservePsi && Depth > 1) return FString::Printf(TEXT("Reserve - %d psi. Start up."), FMath::RoundToInt(Sc.Psi));
		if (Sc.bWinded) return TEXT("Winded - no sprint till you have your breath back");
		return FString();
	}

	void SelfTest()
	{
		// time to empty, resting and finning, by depth
		for (const double Dm : {0.0, 3.0, 5.0, 10.0, 18.0, 25.0, 30.0})
			UE_LOG(LogTemp, Log, TEXT("KRScuba rule: full cylinder at %2.0f m: %.1f min resting, %.1f min finning, %.1f min sprinting (to the %d psi reserve: %.1f min finning)"),
				Dm, SecondsLeft(FullPsi, Dm, 0) / 60, SecondsLeft(FullPsi, Dm, .45) / 60, SecondsLeft(FullPsi, Dm, 1) / 60, int32(ReservePsi), SecondsLeft(FullPsi - ReservePsi, Dm, .45) / 60);
		// the same rules stepped as the game steps them: two minutes at 5 m and at 25 m, resting
		for (const double Dm : {5.0, 25.0})
		{
			FState Sc;
			Sc.bOn = true;
			KeysDive::FState D;
			D.Y = -Dm;
			for (int32 i = 0; i < int32(120 / KeysDive::StepDt + .5); i++) PostStep(Sc, D, KeysDive::StepDt);
			UE_LOG(LogTemp, Log, TEXT("KRScuba rule: two minutes resting at %.0f m uses %.0f psi (stepped)"), Dm, FullPsi - Sc.Psi);
		}
		UE_LOG(LogTemp, Log, TEXT("KRScuba rule: bail-out bottle at 10 m resting lasts %.1f min; sprint %.0f s, back after %.0f s"),
			SecondsLeft(FullPsi, 10, 0) / PonyRatio / 60, SprintSecs, SprintRest * SprintAgain);
	}

	// ================================================================ the set and its cylinders
	bool Owns(const AKeysBoatPawn* P, const TCHAR* Effect)
	{
		if (!P) return false;
		if (P->HasGear(Effect)) return true;
		const FString E(Effect);
		return (E == FxCylinder || E == FxJacket || E == FxRegs || E == FxComputer) && P->HasGear(FxPackage);
	}

	bool HasSet(const AKeysBoatPawn* P) { return Owns(P, FxCylinder) && Owns(P, FxJacket) && Owns(P, FxRegs); }
	int32 Cylinders(const AKeysBoatPawn* P) { return Owns(P, FxCylinder) ? (Owns(P, FxSpare) ? 2 : 1) : 0; }

	double CylinderPsi(const AKeysBoatPawn* P, int32 Index)
	{
		if (!P || Index < 0 || Index >= Cylinders(P)) return 0;
		return ScClamp(FullPsi - P->CountOf(KeyUsed[Index]), 0, FullPsi);
	}

	void SetCylinderPsi(AKeysBoatPawn* P, int32 Index, double Psi)
	{
		if (!P || Index < 0 || Index > 1) return;
		P->Counts.FindOrAdd(KeyUsed[Index]) = FMath::RoundToInt(ScClamp(FullPsi - Psi, 0, FullPsi));
	}

	int32 BestCylinder(const AKeysBoatPawn* P) { return Cylinders(P) > 1 && CylinderPsi(P, 1) > CylinderPsi(P, 0) ? 1 : 0; }

	bool WantScuba() { return GScubaWant; }
	void SetWantScuba(bool bScuba) { GScubaWant = bScuba; }
	bool WillDive(const AKeysBoatPawn* P) { return GScubaWant && HasSet(P) && CylinderPsi(P, BestCylinder(P)) >= 50; }

	FString ToggleMode(const AKeysBoatPawn* P)
	{
		if (!HasSet(P)) return TEXT("No scuba set aboard - Scuba Steve's sells one. You go over on a breath.");
		GScubaWant = !GScubaWant;
		if (!GScubaWant) return TEXT("Next dive: on a breath, with the gun. The set stays in the rack.");
		const double Psi = CylinderPsi(P, BestCylinder(P));
		if (Psi < 50) return TEXT("Next dive: scuba - but the cylinders are empty. Scuba Steve's fills them.");
		return FString::Printf(TEXT("Next dive: scuba. %s psi in the cylinder, about %d minutes on a 10 m reef."), *ScCommas(FMath::RoundToInt(Psi)), FMath::RoundToInt(SecondsLeft(Psi, 10, .45) / 60));
	}

	void LoadoutRows(const AKeysBoatPawn* P, TArray<TPair<FString, bool>>& Out)
	{
		if (!P) return;
		const int32 Pieces = (Owns(P, FxCylinder) ? 1 : 0) + (Owns(P, FxJacket) ? 1 : 0) + (Owns(P, FxRegs) ? 1 : 0);
		if (Pieces == 0) return;
		if (!HasSet(P))
		{
			Out.Add(TPair<FString, bool>(FString::Printf(TEXT("Scuba: %d of 3 pieces (cylinder, jacket, regulators)"), Pieces), false));
			return;
		}
		const double Psi = CylinderPsi(P, 0);
		Out.Add(TPair<FString, bool>(FString::Printf(TEXT("Scuba cylinder: %s psi, about %d min at 10 m"), *ScCommas(FMath::RoundToInt(Psi)), FMath::RoundToInt(SecondsLeft(Psi, 10, .45) / 60)), Psi >= 50));
		if (Cylinders(P) > 1) Out.Add(TPair<FString, bool>(FString::Printf(TEXT("Spare cylinder: %s psi"), *ScCommas(FMath::RoundToInt(CylinderPsi(P, 1)))), CylinderPsi(P, 1) >= 50));
		Out.Add(TPair<FString, bool>(GScubaWant ? TEXT("Going over on scuba") : TEXT("Going over on a breath (set in the rack)"), GScubaWant));
		if (P->HasGear(TEXT("dive.tank"))) Out.Add(TPair<FString, bool>(TEXT("Pony bottle: the bail-out on scuba"), P->CountOf(TEXT("dive.fill")) > 0));
		Out.Add(TPair<FString, bool>(Owns(P, FxComputer) ? TEXT("Wrist computer: ascent rate, safety stop, time left") : TEXT("No dive computer: the gauge only"), Owns(P, FxComputer)));
		if (Owns(P, FxLight)) Out.Add(TPair<FString, bool>(TEXT("Dive light for ledges and wrecks"), true));
		const int32 Dives = P->CountOf(TEXT("dive.log.n"));
		if (Dives > 0) Out.Add(TPair<FString, bool>(FString::Printf(TEXT("Logbook: %d dives, deepest %.1f m"), Dives, P->CountOf(TEXT("dive.log.maxdm")) / 10.0), true));
	}

	// ================================================================ the shop's words
	// Invented names throughout: the house brand is the shop's own.
	const TArray<FShopRow>& ShopRows()
	{
		static const TArray<FShopRow> Rows = {
			{TEXT("scuba-set"), TEXT("Steve's Own scuba set"), TEXT("Cylinder, jacket, regulators and wrist computer. Stay down and look around."), 7500, FxPackage},
			{TEXT("scuba-cyl"), TEXT("Aluminium 80 cylinder"), TEXT("3,000 psi of air on your back. Comes full."), 1200, FxCylinder},
			{TEXT("scuba-jacket"), TEXT("Hawk Channel buoyancy jacket"), TEXT("Holds you level at any depth. Weight belt thrown in."), 2400, FxJacket},
			{TEXT("scuba-regs"), TEXT("Old Seven regulator set"), TEXT("First stage, two second stages, gauge console and inflator hose."), 3200, FxRegs},
			{TEXT("scuba-computer"), TEXT("Sombrero Light wrist computer"), TEXT("Ascent rate, safety stop and minutes left, on your wrist."), 1600, FxComputer},
			{TEXT("scuba-cyl2"), TEXT("Second cylinder for the boat"), TEXT("Swap at the surface and go back down. Comes full."), 1200, FxSpare},
			{TEXT("dive-light"), TEXT("Ledge lantern dive light"), TEXT("For under the ledges and inside the wrecks."), 380, FxLight},
			{TEXT("scuba-fill"), TEXT("Cylinder fill"), TEXT("Every cylinder you carry, back to 3,000 psi."), 18, FxFill}};
		return Rows;
	}

	bool ShopRefuse(const AKeysBoatPawn* P, const FString& Effect, FString& Why)
	{
		if (!P) return false;
		const bool bPiece = Effect == FxCylinder || Effect == FxJacket || Effect == FxRegs || Effect == FxComputer;
		if (bPiece && P->HasGear(FxPackage)) { Why = TEXT("Steve taps the set in your bag. \"That came in the box, captain. You own it.\""); return true; }
		if (Effect == FxPackage && HasSet(P) && Owns(P, FxComputer)) { Why = TEXT("\"You have every piece of it already. I like money, but not that much.\""); return true; }
		if (Effect == FxSpare && !Owns(P, FxCylinder)) { Why = TEXT("\"A second cylinder wants a first one. Start with the set.\""); return true; }
		if (Effect == FxFill)
		{
			if (Cylinders(P) == 0) { Why = TEXT("\"Fill what? Bring me a cylinder and I will put air in it.\""); return true; }
			bool bRoom = false;
			for (int32 i = 0; i < Cylinders(P); i++) bRoom |= CylinderPsi(P, i) < FullPsi - 25;
			if (!bRoom) { Why = TEXT("Steve thumbs the gauge. \"Three thousand. It is as full as it gets.\""); return true; }
		}
		return false;
	}

	bool ShopResolve(AKeysBoatPawn* P, const FString& Effect, FString& Msg)
	{
		if (!P) return false;
		if (Effect == FxFill)
		{
			for (int32 i = 0; i < 2; i++) P->Counts.Remove(KeyUsed[i]);
			Msg += Cylinders(P) > 1 ? TEXT(" Both cylinders at 3,000 psi.") : TEXT(" The cylinder is at 3,000 psi.");
			return true;
		}
		if (Effect == FxPackage || Effect == FxCylinder)
		{
			P->Counts.Remove(KeyUsed[0]);
			GScubaWant = true;
			Msg += HasSet(P) ? TEXT(" The cylinder is full. It stands in the rack on your boat; J goes over on scuba.") : TEXT(" It comes full. You still need the rest of the set to breathe from it.");
			return true;
		}
		if (Effect == FxSpare) { P->Counts.Remove(KeyUsed[1]); Msg += TEXT(" Full, and strapped in beside the first. Swap at the surface, by the boat."); return true; }
		if (Effect == FxJacket || Effect == FxRegs)
		{
			Msg += HasSet(P) ? TEXT(" That completes the set: J goes over on scuba.") : TEXT(" Cylinder, jacket and regulators make a set.");
			return true;
		}
		if (Effect == FxComputer) { Msg += TEXT(" It watches your ascent and asks for a safety stop after a deep one."); return true; }
		if (Effect == FxLight) { Msg += TEXT(" It comes on by itself in the dark; the light key works it by hand."); return true; }
		return false;
	}

	// ================================================================ what a dive leaves for the next one
	bool Aching(double Now) { return Now < GScubaCarry.AcheUntil; }
	double CarryRateMul(double Now) { return Aching(Now) ? 1.3 : GScubaCarry.bCalm ? .9 : 1; }
	double CarryLoad(double Now) { return FMath::Max(0.0, GScubaCarry.NLoad - (Now - GScubaCarry.At) / 300); }
	void LeaveCarry(double Now, double NLoad, bool bBent, bool bBad, bool bCleanStop)
	{
		GScubaCarry.At = Now;
		GScubaCarry.NLoad = NLoad;
		if (bBent) GScubaCarry.AcheUntil = Now + AcheSecs * (bBad ? 2 : 1);
		GScubaCarry.bCalm = bCleanStop && !bBent;
	}

	// ================================================================ the logbook
	const TArray<FLog>& Logbook() { return GScubaLog; }

	FString SiteNear(const AKeysBoatPawn* P, double X, double Z)
	{
		FString Best;
		double BestD = 700;
		if (P)
		{
			TArray<AKeysBoatPawn::FRunDest> All;
			P->RunDests(All);
			for (const AKeysBoatPawn::FRunDest& D : All)
			{
				if (D.Note == TEXT("marina") || D.Note == TEXT("open water") || D.Note.StartsWith(TEXT("mangrove")) || D.Note.StartsWith(TEXT("tarpon"))) continue;
				const double Dd = FVector2D(D.X - X, D.Z - Z).Size();
				if (Dd < BestD) { BestD = Dd; Best = D.Name; }
			}
		}
		if (!Best.IsEmpty()) return Best;
		return FString::Printf(TEXT("%s, open water"), *KeysGeo::ZoneAt(X, Z));
	}

	FString LogLine(const FLog& L)
	{
		FString S = FString::Printf(TEXT("Dive %d - %s - %.1f m - %s"), L.Number, *L.Site, L.MaxDepth, *ScMinSec(L.Secs));
		if (L.bScuba) S += FString::Printf(TEXT(" - %s psi used"), *ScCommas(FMath::RoundToInt(L.PsiUsed)));
		else S += TEXT(" - on a breath");
		if (L.bStop) S += TEXT(" - safety stop made");
		if (L.bFast) S += TEXT(" - fast ascent");
		return S;
	}

	FString AddLog(AKeysBoatPawn* P, FLog L)
	{
		L.Number = (P ? P->CountOf(TEXT("dive.log.n")) : GScubaLog.Num()) + 1;
		if (P)
		{
			P->Counts.FindOrAdd(TEXT("dive.log.n")) = L.Number;
			int32& Max = P->Counts.FindOrAdd(TEXT("dive.log.maxdm"));
			Max = FMath::Max(Max, FMath::RoundToInt(L.MaxDepth * 10));
			P->Counts.FindOrAdd(TEXT("dive.log.secs")) += FMath::RoundToInt(L.Secs);
		}
		GScubaLog.Add(L);
		if (GScubaLog.Num() > 200) GScubaLog.RemoveAt(0);
		return LogLine(L);
	}
}
