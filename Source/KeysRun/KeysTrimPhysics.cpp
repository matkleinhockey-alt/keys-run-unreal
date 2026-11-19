// Outboard trim, tilt and trim tabs: what the drive angle does to the hull (numbers per boat in KeysTrim.h /
// DA_TrimTuning). UpdateTrim runs once a frame from UpdateBoat, before the speed and the hull pose are worked out, and
// leaves its results in the Trim* members that UpdateBoat, UpdateFuel and Slam read.
#include "KeysBoatPawn.h"
#include "KeysTrim.h"
#include "KeysAudio.h"
#include "KeysGeo.h"
#include "KeysData.h"

using namespace KeysGeo;
using namespace KeysData;

const FKeysTrimProfile& AKeysBoatPawn::TrimProfile() const { return UKeysTrimTuning::For(Spec().Id); }

bool AKeysBoatPawn::IsTilted() const { return TrimDeg > TrimProfile().TrimMaxDeg + .01; }

void AKeysBoatPawn::UpdateTrim(double Dt, double T, double TopMs)
{
	const FKeysTrimProfile& P = TrimProfile();
	const FBoatSpec& S = Spec();
	const double Kn = KnotsNow(), Fr = FMath::Abs(Speed) / FMath::Max(TopMs, .1);

	// ---- the switches. Past the trim range is tilt: the motors only tilt up slow (the switch cuts out at speed)
	if (!P.bOutboards) TrimDeg = 0;
	else
	{
		const bool bSlow = Kn < P.TiltAllowedBelowKnots;
		const double Top = bSlow || TrimDeg > P.TrimMaxDeg ? P.TiltMaxDeg : P.TrimMaxDeg;
		const double Rate = TrimDeg >= P.TrimMaxDeg - .01 ? P.TiltRateDegPerSec : P.TrimRateDegPerSec;
		if (bTrimUp)
		{
			if (!bSlow && TrimDeg >= P.TrimMaxDeg - .01 && T - TrimWarnT > 4) { TrimWarnT = T; ShowToast(TEXT("Trimmed all the way out - slow under 8 knots to tilt the motors up.")); }
			TrimDeg = FMath::Min(Top, TrimDeg + Rate * Dt);
		}
		if (bTrimDn) TrimDeg = FMath::Max<double>(P.TrimMinDeg, TrimDeg - Rate * Dt);
		TrimDeg = FMath::Clamp<double>(TrimDeg, P.TrimMinDeg, FMath::Max(Top, TrimDeg));
	}
	if (bTrimUp && !P.bOutboards && T - TrimWarnT > 6) { TrimWarnT = T; ShowToast(TEXT("Inboard diesels - nothing to trim. Use the tabs (Left/Right in trim mode).")); }
	// one balance control for the tabs: the arrow toward the low side lifts it (that side's tab goes down)
	if (bTabLeft) { if (TabR > 0) TabR = FMath::Max(0.0, TabR - P.TabRatePerSec * Dt); else TabL = FMath::Min(1.0, TabL + P.TabRatePerSec * Dt); }
	if (bTabRight) { if (TabL > 0) TabL = FMath::Max(0.0, TabL - P.TabRatePerSec * Dt); else TabR = FMath::Min(1.0, TabR + P.TabRatePerSec * Dt); }
	TrimV = FMath::Clamp((TrimDeg - P.TrimMinDeg) / FMath::Max(1.0, double(P.TrimMaxDeg - P.TrimMinDeg)), 0.0, 1.0);

	// ---- the sea: how big, and how much of it is on the bow
	const double Fx = -FMath::Sin(H), Fz = -FMath::Cos(H), Rx = FMath::Cos(H), Rz = -FMath::Sin(H);
	const double Amp = AmpAt(X, Z);
	const double Sea = FMath::Clamp((WaveRms() * Amp - .22) / .6, 0.0, 1.0);
	const double Wd = WindDir();
	const double Head = FMath::Clamp(-(FMath::Cos(Wd) * Fx + FMath::Sin(Wd) * Fz), 0.0, 1.0);   // waves run with the wind
	const double IdealCalm = P.IdealAt(Fr);
	TrimIdeal = IdealCalm - P.RoughTrimDownDeg * Sea * (.45 + .55 * Head);
	const bool bTilt = P.bOutboards && TrimDeg > P.TrimMaxDeg + .01;
	const double Used = FMath::Min<double>(TrimDeg, P.TrimMaxDeg);
	// speed and fuel follow the sea-adjusted ideal; plowing, porpoising and blowing out follow the hull's calm-water one
	// (into a head sea you trim down for the ride and give up a little speed, you don't start plowing)
	const double Off = Used - TrimIdeal, OffC = Used - IdealCalm, Band = FMath::Max(.5f, P.SweetBandDeg);
	const double PlaneK = FMath::SmoothStep(.3, .55, Fr);   // trim does its work once the hull is up and running

	// ---- speed and fuel near the sweet spot
	const double E = FMath::Exp(-FMath::Square(Off / Band));
	const double Beyond = FMath::Max(0.0, FMath::Abs(Off) - Band) / Band;
	TrimEff = E;
	TrimSpeedK = 1 + PlaneK * (P.TopSpeedBonus * E - P.OffSpeedLossPerBand * Beyond);
	TrimFuelK = 1 + PlaneK * (-P.FuelBurnSaving * E + P.OffFuelGainPerBand * Beyond);

	// ---- the hole shot: trimmed in she jumps up on plane, trimmed out she stands on her tail and wallows
	const double Hs = FMath::Clamp((Used - P.TrimMinDeg) / FMath::Max(1.0, double(P.TrimMaxDeg - P.TrimMinDeg)), 0.0, 1.0);
	const double Hole = (1 - PlaneK) * FMath::SmoothStep(.15, .5, Thr);
	TrimHoleK = Hole > 0 ? FMath::Lerp(1.0, FMath::Lerp(1.0 + P.HoleShotDownGain, 1.0 - P.HoleShotUpLoss, Hs), FMath::SmoothStep(0.0, .3, Hole)) : 1;
	double PitchT = Hole * FMath::DegreesToRadians(P.BowRiseDeg) * (Hs * Hs) * FMath::Sin(FMath::Min(Fr * 2.4, 1.0) * PI);

	// ---- trimmed in too far at speed: the bow digs in, throws spray and the boat darts at the wheel (bow steer)
	const double Plow = PlaneK * FMath::SmoothStep(double(P.PlowStartDeg), P.PlowStartDeg + 5.0, -OffC) * (1 - .7 * Sea * Head);   // in a head sea, bow down is right
	TrimSpeedK -= Plow * P.PlowSpeedLoss;
	TrimSteerK = 1 + Plow * P.BowSteerGain;
	PitchT -= Plow * FMath::DegreesToRadians(1.8);
	BowSteerPh += Dt * (1.1 + .6 * FMath::Sin(T * .37));
	TrimYaw = Plow * Fr * (.10 * FMath::Sin(BowSteerPh * 2.3) + .06 * FMath::Sin(BowSteerPh * 5.1 + 1)) * (S.bCat ? .5 : 1);   // rad/s wander

	// ---- trimmed out too far at speed: porpoising (the bow bounces), then the props suck air and blow out
	const double Porp = PlaneK * FMath::SmoothStep(double(P.PorpoiseStartDeg), P.PorpoiseStartDeg + 3.5, OffC) * FMath::SmoothStep(.45, .7, Fr);
	PorpAmp += (Porp - PorpAmp) * FMath::Min(1.0, Dt * (Porp > PorpAmp ? .9 : 2.0));
	PorpPh += Dt * 2 * PI * P.PorpoiseHz * (.85 + .3 * Fr);
	PitchT += Used * FMath::DegreesToRadians(.35) * PlaneK + .3 * PorpAmp * FMath::DegreesToRadians(P.PorpoiseAmpDeg) * FMath::Sin(PorpPh);
	// the bounce itself goes straight onto the hull's pose (the pitch spring damps a forced wobble this quick to nothing):
	// a quick bow rise, then a hard drop back onto the water
	const double Ph = FMath::Fmod(PorpPh, 2 * PI) / (2 * PI);
	if (Ph < PorpPrevPh && PorpAmp > .35 && !bAir) Slam(-(1.5 + 2.2 * PorpAmp));   // the bow slaps back down each bounce
	PorpPrevPh = Ph;
	const double Bounce = Ph < .55 ? FMath::Sin(Ph / .55 * PI * .5) : FMath::Cos((Ph - .55) / .45 * PI * .5);
	TrimPorpPose = PorpAmp * FMath::DegreesToRadians(P.PorpoiseAmpDeg) * (2 * Bounce - 1);
	TrimPorpLift = PorpAmp * .09 * Bounce;
	double Vent = PlaneK * FMath::SmoothStep(double(P.VentStartDeg), P.VentStartDeg + 4.0, OffC);
	Vent = FMath::Max(Vent, PorpAmp * .45 * FMath::Max(0.0, FMath::Sin(PorpPh)));   // the props break loose at the top of each bounce
	if (bAir && Fr > .3 && Thr > .3) Vent = FMath::Max(Vent, .6);
	TrimVent += (Vent - TrimVent) * FMath::Min(1.0, Dt * 6);
	TrimSpeedK *= 1 - P.VentThrustLoss * TrimVent;

	// ---- rough water: trimmed in rides softer into head seas, trimmed out launches off them and lands hard
	const double Up = Used - IdealCalm + P.RoughTrimDownDeg * .5;   // degrees over a sensible rough-water trim
	TrimSlamK = FMath::Clamp(1 + P.SlamPerDegUp * Up * (.5 + .5 * Head), .45, 2.2);
	PitchT += Sea * Head * PlaneK * FMath::Clamp(Up, -6.0, 12.0) * P.LaunchPerDegUp;
	HullStress = FMath::Max(0.0, HullStress - Dt * .004);

	// ---- the lower unit in shallow water
	const double Tr = FMath::DegreesToRadians(FMath::Max(0.0, TrimDeg));
	PropDepthNow = P.bOutboards ? P.PropDepthM - P.LowerUnitM * (1 - FMath::Cos(Tr)) - P.LowerUnitM * .9 * FMath::Sin(Tr) * FMath::Sin(Tr) - .18 * PlaneK : P.PropDepthM;
	const double SternX = X - Fx * S.Len * .48, SternZ = Z - Fz * S.Len * .48;
	const double Water = DepthFast(SternX, SternZ);
	const double Sub = FMath::Clamp(PropDepthNow / .25, 0.0, 1.0);   // how much of the prop is still in the water
	TrimCapMs = 1e9;
	if (bTilt) TrimCapMs = P.TiltedMaxKnots * .5144 * Sub;   // tilted up: it pushes, barely
	else if (Sub < 1) TrimCapMs = TopMs * Sub;
	AKeysAudio* Aud = AKeysAudio::Get(this);
	const bool bTurning = Aud && Aud->EnginesOn() && bRunning;
	if (bTurning && PropDepthNow > .05 && Water < PropDepthNow - .05 && (FMath::Abs(Speed) > .6 || FMath::Abs(Thr) > .15) && T - PropStrikeT > 3)
	{
		PropStrikeT = T;
		Speed *= .25;
		Thr = 0;
		Aud->PropStrike();
		Shake = FMath::Max(Shake, .7);
		FString Msg = TEXT("CLUNK - prop strike! The motors stalled. Tilt up (trim mode, Up) in the shallows and press I to restart.");
		if (!bFreeFish && P.PropStrikeCost > 0)
		{
			const int64 Bill = FMath::Min<int64>(P.PropStrikeCost, Cash);
			Cash -= Bill;
			SaveEconomy();
			Msg += FString::Printf(TEXT(" Repair: $%lld."), Bill);
		}
		ShowToast(Msg, 6);
		UE_LOG(LogTemp, Log, TEXT("KRTrim prop strike: trim %.1f prop %.2f water %.2f speed %.1f"), TrimDeg, PropDepthNow, Water, Speed);
	}

	// ---- trim tabs against the lean from a beam wind and the crew on one side
	const double Ws = WindSpeed();
	const double WindBeam = (FMath::Cos(Wd) * Rx + FMath::Sin(Wd) * Rz) * Ws;   // + blows toward starboard
	const double CrewSide = IsLineOut() || FState == EKeysFish::Caught ? 1 : .25;    // the angler at the starboard rail
	const double LeanDeg = -WindBeam * P.WindLeanDegPerMs - P.CrewLeanDeg * CrewSide;    // + = starboard side up
	const double TabDeg = -(TabL - TabR) * P.TabAuthorityDeg * FMath::SmoothStep(.15, .5, Fr);   // the port tab lifts the port side
	TrimLean = LeanDeg + TabDeg;
	TrimRoll = FMath::DegreesToRadians(TrimLean) * (.4 + .6 * FMath::Min(1.0, Fr * 2));
	PitchT -= FMath::DegreesToRadians(.6) * (TabL + TabR) * PlaneK;   // tabs down push the bow down a touch
	TrimSpeedK -= .015 * (TabL + TabR) * PlaneK;
	TrimSpraySide = FMath::Clamp(-TrimLean / 4, -1.0, 1.0) * FMath::SmoothStep(.3, .6, Fr);   // the low side's chine throws the spray
	TrimSprayK = 1 + Plow * P.PlowSpray + FMath::Abs(TrimSpraySide) * 1.5;
	if (FMath::Abs(TrimLean) > 2.5 && Fr > .45 && T - TabMsgT > 25)
	{
		TabMsgT = T;
		ShowToast(TrimLean > 0 ? TEXT("Leaning to port - the spray's coming over that side. Trim mode: Left lifts the port side with the tab.")
			: TEXT("Leaning to starboard - wet ride on that side. Trim mode: Right lifts the starboard side with the tab."), 5);
	}

	TrimPitch = PitchT;
	TrimRide = bTilt ? TEXT("Tilted up") : TrimVent > .35 ? TEXT("Blowing out!") : PorpAmp > .3 ? TEXT("Porpoising") : Plow > .3 ? TEXT("Plowing")
		: Hole > .3 && Hs > .6 ? TEXT("Bow high") : P.bOutboards && PlaneK > .5 && E > .7 ? TEXT("Sweet spot") : FString();
	if (TrimVent > .35 && T - TrimMsgT > 6) { TrimMsgT = T; ShowToast(TEXT("Relax, Chum Lee - your props are out of the water! Trim down.")); }
	else if (PorpAmp > .45 && T - TrimMsgT > 8) { TrimMsgT = T; ShowToast(TEXT("She's porpoising - trim down a few degrees.")); }
	else if (Plow > .6 && T - TrimMsgT > 10) { TrimMsgT = T; ShowToast(TEXT("Plowing the bow - trim up to free her.")); }

	if (bScriptLog && (TrimLogT -= Dt) <= 0)
	{
		TrimLogT = .25;
		UE_LOG(LogTemp, Log, TEXT("KRTrim kn %.1f fr %.2f trim %.1f ideal %.1f eff %.2f spdK %.3f fuelK %.3f hole %.2f plow %.2f porp %.2f vent %.2f slam %.2f sea %.2f head %.2f prop %.2f water %.2f lean %.1f tabs %.2f/%.2f pitch %.3f %s"),
			Kn, Fr, TrimDeg, TrimIdeal, E, TrimSpeedK, TrimFuelK, TrimHoleK, Plow, PorpAmp, TrimVent, TrimSlamK, Sea, Head, PropDepthNow, Water, TrimLean, TabL, TabR, Pitch, *TrimRide);
	}
}
