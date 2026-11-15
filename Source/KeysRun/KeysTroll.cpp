// Trolling: the outriggers, the spread of lures skipping in the wake, and a strike that arrives at seven knots.
//
// Ported from the web game: the rules from packages/shared/src/sim/troll.ts (the speed window, the strike clock,
// what eats a dragged lure), the client half from apps/client/src/game/fishing/trolling.ts (the throttle hold, the
// raise, the hookup into the rod's own fight), the lines and lures from troll-spread.ts and the fish coming up
// behind a bait from troll-raise.ts. A trolling hookup goes through SetHook(), the same hookup the strike window
// uses, so the fight, the jumps, the catch card, the cooler and the score are the existing machinery.

#include "KeysBoatPawn.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "KeysFeed.h"
#include "KeysGeo.h"
#include "KeysMesh.h"
#include "KeysWorld.h"
#include "KeysLife.h"
#include "KeysAudio.h"
#include "ProceduralMeshComponent.h"
#include "Camera/CameraComponent.h"

using namespace KeysGeo;
using namespace KeysData;

namespace
{
	// sim/troll.ts
	constexpr double TrollSpeedMin = 2.6;      // m/s: below this the spread sinks and stops swimming
	constexpr double TrollSpeedMax = 4.6;      // m/s: above this the lures blow out and skip
	constexpr double TrollSpeedTarget = 3.6;   // the middle of the band, what the throttle hold aims for
	constexpr double TrollBaseGap = 48;        // seconds of fishing credit between strikes at the best speed
	constexpr double TrollMinGap = 22;         // never less than this many seconds between strikes
	// trolling.ts / troll-raise.ts / troll-spread.ts
	constexpr double RigSwing = 2.4;           // seconds for the poles to swing out or stow
	constexpr double ResumePause = 7;          // after a fish, before the boat goes back on the troll
	constexpr double RaiseS = 3.6;             // from the fish showing to the eat: up the wake, on the lure, then the lunge
	constexpr double RaiseDepth = 9, RaiseBack = 7;
	constexpr double PullS = 1.1, PullDepth = 1.5;
	constexpr double TrackStep = .6;

	struct FStation { double Back, Side; int32 Rigger; };
	// meters astern and to one side: two long rigger baits, two flat lines down the prop wash, the shotgun way back
	const FStation Stations[5] = {{64, -23, 0}, {56, 21, 1}, {38, -7.5, -1}, {45, 7.5, -1}, {96, 1.5, -1}};

	double TrollQuality(double Speed)
	{
		const double S = FMath::Abs(Speed);
		if (S <= TrollSpeedMin || S >= TrollSpeedMax) return 0;
		const double Half = (TrollSpeedMax - TrollSpeedMin) / 2, Off = (S - (TrollSpeedMin + Half)) / Half;
		return FMath::Max(0.0, FMath::Cos(Off * PI / 2));
	}

	// TROLL_WEIGHTS: the tunas, billfish and wahoo are what a spread is set for; anything else can still hit at 1
	double TrollWeight(const FString& K)
	{
		static const TMap<FString, double> W = {
			{TEXT("wahoo"), 12}, {TEXT("yellowfin"), 12}, {TEXT("bluefin"), 12}, {TEXT("bigeye"), 12},
			{TEXT("blackfin"), 10}, {TEXT("albacore"), 10}, {TEXT("marlin"), 10}, {TEXT("blackmarlin"), 10},
			{TEXT("sailfish"), 9}, {TEXT("swordfish"), 8}, {TEXT("skipjack"), 8},
			{TEXT("mahi"), 6}, {TEXT("kingfish"), 5}, {TEXT("mako"), 3}, {TEXT("barracuda"), 3}, {TEXT("cero"), 3}, {TEXT("spanishmack"), 3},
			{TEXT("amberjack"), 2}, {TEXT("jackcrevalle"), 2}, {TEXT("tarpon"), 2}};
		const double* V = W.Find(K);
		return V ? *V : 1;
	}

	double WrapPiT(double A) { return FMath::Fmod(FMath::Fmod(A + PI, 2 * PI) + 2 * PI, 2 * PI) - PI; }
	FVector G2U(const FVector& G) { return FVector(G.X, G.Z, G.Y); }   // game meters -> Unreal axes, still meters
}

// ---------------------------------------------------------------- the mode
void AKeysBoatPawn::ToggleTroll()
{
	if (IsDiving() || IsAshore()) { ShowToast(TEXT("Back at the helm first.")); return; }
	if (bTrolling) { StopTroll(false); return; }
	if (FState == EKeysFish::Caught) { ShowToast(TEXT("Finish with your catch first.")); return; }
	if (IsLineOut() && FState != EKeysFish::Fight) ReelIn(FString());
	bCharging = false;
	bTrolling = true;
	TUntil = TrollBaseGap * (.55 + FMath::FRand() * .9);
	TSoak = 0;
	TSince = TrollMinGap;
	TResume = 0;
	bTrollBusy = false;
	Gear = 0;
	FVector Tip[2], Base[2];
	const bool bSwing = KeysMesh::HasSwingingRiggers(Spec()), bPoles = KeysMesh::RiggerTips(Spec(), 0, Tip, Base);
	ShowToast(bSwing ? TEXT("Trolling - outriggers out, holding 5 knots. The lures swim between 4 and 6.5. = again to bring them in.")
		: bPoles ? TEXT("Trolling - lines out to the T-top riggers, holding 5 knots. The lures swim between 4 and 6.5. = again to bring them in.")
		: TEXT("Trolling - lines out, holding 5 knots. This hull has no riggers, so the spread runs off the transom. = to bring them in."), 5);
}

void AKeysBoatPawn::StopTroll(bool bQuiet)
{
	if (!bTrolling) return;
	bTrolling = false;
	if (!PendKey.IsEmpty() && FState == EKeysFish::Idle) ShowFishMesh(false);
	PendKey.Reset();
	PendStation = -1;
	RaiseLeft = 0;
	RaiseCamT = 0;
	TResume = 0;
	if (!bQuiet) ShowToast(TEXT("Lines in."));
}

bool AKeysBoatPawn::TrollThrottle(double TopMs, double& OutThr) const
{
	// a fish on, the catch card up, or the pause while the lines are cleared: out of gear, the player drives
	if (!bTrolling || FState == EKeysFish::Fight || FState == EKeysFish::Caught || TResume > 0) return false;
	// The web game pulses the lever (bang-bang around the target); here the hold eases the lever to the
	// throttle that makes seven knots and trims it on the speed error, so the engine settles instead of hunting.
	OutThr = FMath::Clamp(TrollSpeedTarget / FMath::Max(1.0, TopMs) * 1.05 + (TrollSpeedTarget - Speed) * .06, 0.0, .6);
	return true;
}

FString AKeysBoatPawn::TrollSpecies(double Q) const
{
	// pickTrollSpecies: weighted by TROLL_WEIGHTS, sharpened by how well the spread was fishing
	const TArray<FZoneRow>& Tbl = ZoneTable(ZoneAt(X, Z));
	if (!Tbl.Num()) return FString();
	const double Sharpen = .6 + FMath::Clamp(Q, 0.0, 1.0);
	TArray<double> W;
	double Tot = 0;
	const double Hr = KeysFeed::HourNow(this);
	for (const FZoneRow& R : Tbl) { const double V = FMath::Pow(TrollWeight(R.Key), Sharpen) * KeysFeed::Mul(R.Key, Hr); W.Add(V); Tot += V; }
	double Rr = FMath::FRand() * Tot;
	for (int32 i = 0; i < Tbl.Num(); i++) { Rr -= W[i]; if (Rr <= 0) return Tbl[i].Key; }
	return Tbl.Last().Key;
}

FString AKeysBoatPawn::TrollSideName(int32 Station) const
{
	FVector At;
	bool bStar = Station % 2 == 1;
	if (LurePos(Station, At)) bStar = (At.X - X) * FMath::Cos(H) + (At.Z - Z) * -FMath::Sin(H) > 0;
	return FString(bStar ? TEXT("Starboard") : TEXT("Port")) + (Station < 2 ? TEXT(" rigger") : Station == 4 ? TEXT(" shotgun") : TEXT(" flat line"));
}

// a fish has committed and is on its way up: everything that decides the catch is rolled here
void AKeysBoatPawn::TrollRaise(double Q)
{
	if (FState != EKeysFish::Idle || !PendKey.IsEmpty()) return;
	const FString K = !DbgTrollKey.IsEmpty() && Species(DbgTrollKey) ? DbgTrollKey : TrollSpecies(Q);
	const FSpecies* S = K.IsEmpty() ? nullptr : Species(K);
	if (!S) return;
	// trolling is how the big ones are caught: a well-trimmed spread skews the weight heavy
	const double Tt = FMath::Clamp(FMath::Pow(FMath::FRand(), 1 - .45 * Q), 0.0, 1.0);
	PendKey = K;
	PendWeight = FMath::RoundToDouble((S->Min + (S->Max - S->Min) * Tt) * 10) / 10;
	// the long riggers carry the outside baits and are what a fish coming up the wake meets first
	PendStation = FMath::FRand() < .62 ? (FMath::FRand() < .5 ? 0 : 1) : FMath::RandRange(0, NumStations - 1);
	RaiseLeft = RaiseS;
	RaiseCamT = RaiseS + 1.3;
	// the climbing fish is the hooked fish's own mesh, so what you watch come up is what you hook
	Key = K; Sp = S; Weight = PendWeight;
	PickRecapture();
	BuildHookedMesh();
	Key.Reset(); Sp = nullptr;
	FVector L;
	if (!LurePos(PendStation, L)) L = FVector(X, 0, Z);
	const double BX = L.X - X, BZ = L.Z - Z, BL = FMath::Max(1.0, FMath::Sqrt(BX * BX + BZ * BZ));
	RaisePos = FVector(L.X + BX / BL * RaiseBack, -RaiseDepth, L.Z + BZ / BL * RaiseBack);
	ShowFishMesh(true);
	ShowToast(TrollSideName(PendStation) + TEXT(" - FISH UP!"), 2.5);
}

// the eat: the fish is on at wherever its lure has got to
void AKeysBoatPawn::TrollCommit()
{
	const FString K = PendKey;
	const int32 Stn = PendStation;
	PendKey.Reset();
	PendStation = -1;
	RaiseLeft = 0;
	const FSpecies* S = Species(K);
	if (!S || FState != EKeysFish::Idle) { ShowFishMesh(false); return; }
	FVector At;
	if (!LurePos(Stn, At)) At = FVector(X + FMath::Sin(H) * 40, 0, Z + FMath::Cos(H) * 40);
	Key = K; Sp = S; Weight = PendWeight;
	FX = At.X; FZ = At.Z; OFX = OFZ = 0;
	Bob = FVector(At.X, 0, At.Z);
	Spot = FKeysFishSpot();
	Spot.Zone = ZoneAt(X, Z);
	if (const AKeysLife* L = AKeysLife::Get(this)) Spot = L->SpotAt(At.X, At.Z);
	Zone = Spot.Zone;
	// the bait goes under before anything else happens, because that is the order it happens in
	Pulled[Stn] = PullS;
	HookedStation = Stn;
	if (AKeysLife* L = AKeysLife::Get(this)) L->Splash(At.X, At.Z, 42, 2.2);
	if (AKeysAudio* A = AKeysAudio::Get(this)) A->Strike(At.X, At.Z);
	Shake = FMath::Max(Shake, .55);
	BuildHookedMesh();
	SetHook();
	ShowToast(FString::Printf(TEXT("LINE OUT! %s - %d lb on the %s."), *S->Name, FMath::RoundToInt(Weight), *TrollSideName(Stn).ToLower()), 5);
	FightSub = TEXT("Off the troll and out of gear - hold S (or Space) to reel, ease off when the bar runs red.");
	// a trolled fish comes up: SetHook has rolled its jumps on the strike already (JumpHabit); a billfish or a tarpon
	// on the spread always shows itself
	if ((IsBillfish(Key) || Key == TEXT("tarpon")) && JumpQ == 0) { JumpQ = IsBillfish(Key) ? 3 : 2; JumpWait = .3; }
}

void AKeysBoatPawn::UpdateTroll(double Dt, double T)
{
	// the poles swing on the same eased number the spread comes out on
	const double Want = bTrolling ? 1 : 0;
	if (RigK != Want) RigK = Want > RigK ? FMath::Min(1.0, RigK + Dt / RigSwing) : FMath::Max(0.0, RigK - Dt / RigSwing);
	if (FMath::Abs(RigK - RigBuilt) > 1e-4)
	{
		RigBuilt = RigK;
		FKeysMesh M;
		KeysMesh::BuildRiggers(M, Spec(), FMath::SmoothStep(0.0, 1.0, RigK));
		if (M.V.Num()) M.Commit(RiggerMesh, 0, AKeysWorld::VertexColorMaterial());
		else RiggerMesh->ClearAllMeshSections();
	}
	for (double& P : Pulled) P = FMath::Max(0.0, P - Dt);
	RaiseCamT = FMath::Max(0.0, RaiseCamT - Dt);
	if (FState != EKeysFish::Fight) HookedStation = -1;
	if (!bTrolling) { TrollQ = 0; bTrollBusy = false; return; }
	if (!bRunning || IsDiving() || IsAshore()) { StopTroll(true); return; }

	// watch for a fight ENDING, however it ended - landed, released or broken off
	const bool bBusy = FState == EKeysFish::Fight || FState == EKeysFish::Caught;
	if (bTrollBusy && !bBusy) TResume = ResumePause;
	bTrollBusy = bBusy;
	if (TResume > 0) { TResume = FMath::Max(0.0, TResume - Dt); return; }

	// a fish on its way up has the lures' attention: nothing else eats until this one has or hasn't
	if (!PendKey.IsEmpty())
	{
		RaiseLeft -= Dt;
		if (FState != EKeysFish::Idle) { PendKey.Reset(); PendStation = -1; }
		else if (RaiseLeft <= 0) TrollCommit();
		else
		{
			// up from behind and below on a cubic ease-in: a slow wind-up and then the lunge
			FVector L;
			if (LurePos(PendStation, L))
			{
				// three beats (troll-raise.ts, slowed for the camera): up out of the blue from deep astern of the bait;
				// tracking it just under the surface, lit up and weaving, a body length back; then the lunge, head out,
				// through the lure
				const double U = FMath::Clamp(1 - RaiseLeft / RaiseS, 0.0, 1.0);
				auto Ease = [](double V) { V = FMath::Clamp(V, 0.0, 1.0); return V * V * (3 - 2 * V); };
				const double Len = FMath::Max(.6, KeysMesh::FishLength(PendWeight));
				double Depth, Back;
				if (U < .42) { const double E = 1 - FMath::Square(1 - U / .42); Depth = FMath::Lerp(RaiseDepth * .45, .7, E); Back = FMath::Lerp(RaiseBack * 1.6, Len * 1.3, E); }
				else if (U < .84) { const double E = Ease((U - .42) / .42); Depth = FMath::Lerp(.7, .38, E); Back = FMath::Lerp(Len * 1.3, Len * .85, E); }
				else { const double E = Ease((U - .84) / .16); Depth = FMath::Lerp(.38, -.18, E); Back = FMath::Lerp(Len * .85, -Len * .15, E); }
				double UX = RaisePos.X - L.X, UZ = RaisePos.Z - L.Z;
				const double D = FMath::Sqrt(UX * UX + UZ * UZ);
				if (D > .05) { UX /= D; UZ /= D; } else { UX = FMath::Sin(H); UZ = FMath::Cos(H); }
				// tracking, it swings from side to side behind the bait
				const double Weave = U > .3 && U < .86 ? FMath::Sin(T * 3.1) * Len * .25 : 0;
				const double Water = WaveH(L.X, L.Z, T, AmpAt(L.X, L.Z));
				RaisePos = FVector(L.X + UX * Back - UZ * Weave, Water - Depth - .1, L.Z + UZ * Back + UX * Weave);
				const double Yaw = FMath::Atan2(L.Z - RaisePos.Z, L.X - RaisePos.X);
				const double Climb = U < .42 ? .35 : U < .84 ? .05 : .45;
				FishMesh->SetWorldLocationAndRotation(ToUE(RaisePos.X, RaisePos.Y, RaisePos.Z),
					FRotator(FMath::RadiansToDegrees(Climb), FMath::RadiansToDegrees(Yaw), FMath::Sin(T * 9) * 9));
				// the lunge: white water as it breaks the surface on the bait
				if (U >= .84 && !bRaiseBurst) { bRaiseBurst = true; if (AKeysLife* Lf = AKeysLife::Get(this)) Lf->Splash(L.X, L.Z, 26, 1.6); }
				if (U < .84) bRaiseBurst = false;
				// the boil: water pushed up over the fish once it is shallow enough to move it
				const double Shallow = FMath::Clamp(1 - Depth / 1.6, 0.0, 1.0);
				if (Shallow > .05 && FMath::FRand() < Dt * 14 * Shallow)
					if (AKeysLife* Lf = AKeysLife::Get(this)) Lf->Splash(RaisePos.X, RaisePos.Z, 4, .5 + Shallow * .6);
			}
		}
		return;
	}
	if (bBusy) return;

	// sim/troll.ts stepTroll: the clock is spent in proportion to how well the lures are swimming
	const double Q = TrollQuality(Speed);
	TrollQ = Q;
	TSoak += Dt;
	TSince += Dt;
	if (Q <= 0) return;
	const AKeysLife* Life = AKeysLife::Get(this);
	const double Rich = Life && Life->SpotAt(X, Z).bHot ? 2.4 : 1;   // a weedline, a bird pile or a rig fishes faster
	TUntil -= Q * FMath::Max(.1, Rich) * Dt * FeedArea;   // fish feeding now come up on the spread sooner
	if (TUntil > 0 || TSince < TrollMinGap) { TUntil = FMath::Max(0.0, TUntil); return; }
	TUntil = TrollBaseGap * (.55 + FMath::FRand() * .9);
	TSince = 0;
	if (FState == EKeysFish::Idle)
	{
		TrollRaise(Q);
		if (bScriptLog) UE_LOG(LogTemp, Log, TEXT("KRDBG troll raise %d at hour %.2f, feed x%.2f"), ++DbgBites, KeysFeed::HourNow(this), FeedArea);
	}
}

// ---------------------------------------------------------------- the spread (troll-spread.ts)
void AKeysBoatPawn::PushTrack()
{
	if (!Trk.Num()) Trk.SetNum(FMath::CeilToInt((Stations[4].Back + 24) / TrackStep));
	const int32 N = Trk.Num();
	auto Reset = [&]() { TrkHead = 0; Trk[0] = FVector(X, Z, H); TrkCount = 1; };
	if (TrkHead < 0) { Reset(); return; }
	const FVector& Hd = Trk[TrkHead];
	const double D = FMath::Sqrt(FMath::Square(X - Hd.X) + FMath::Square(Z - Hd.Y));
	if (D > 24) { Reset(); return; }   // a teleport, not a boat
	if (D < TrackStep) return;
	const int32 Steps = FMath::Min(8, FMath::FloorToInt(D / TrackStep));
	const FVector P0 = Hd;
	for (int32 k = 1; k <= Steps; k++)
	{
		const double F = double(k) / Steps;
		TrkHead = (TrkHead + 1) % N;
		Trk[TrkHead] = FVector(P0.X + (X - P0.X) * F, P0.Y + (Z - P0.Y) * F, P0.Z + WrapPiT(H - P0.Z) * F);
		TrkCount = FMath::Min(N, TrkCount + 1);
	}
}

bool AKeysBoatPawn::TrackBack(double Back, double& OX, double& OZ, double& OH) const
{
	if (TrkCount < 3) return false;
	const int32 N = Trk.Num();
	double Acc = 0;
	for (int32 k = 0; k < TrkCount - 1; k++)
	{
		const int32 I = (TrkHead - k + N * 2) % N, J = (I - 1 + N) % N;
		const double Seg = FMath::Sqrt(FMath::Square(Trk[I].X - Trk[J].X) + FMath::Square(Trk[I].Y - Trk[J].Y));
		if (Acc + Seg >= Back)
		{
			const double F = Seg > 1e-6 ? (Back - Acc) / Seg : 0;
			OX = Trk[I].X + (Trk[J].X - Trk[I].X) * F;
			OZ = Trk[I].Y + (Trk[J].Y - Trk[I].Y) * F;
			OH = Trk[I].Z + WrapPiT(Trk[J].Z - Trk[I].Z) * F;
			return true;
		}
		Acc += Seg;
	}
	return false;
}

// One skirted trolling lure, nose at Nose pointing along Fwd (Unreal meters). Opaque parts go in M, the clear acrylic
// head in CM. Pal picks one of the six colourways in the user's photo; Ph keeps each skirt waving on its own beat.
static void AddLure(FKeysMesh& M, FKeysMesh& CM, const FVector& Nose, const FVector& Fwd, const FVector& Up0, double Sc, int32 Pal, double Ph)
{
	auto C = [](uint32 X) { return FLinearColor(FColor(uint8(X >> 16), uint8(X >> 8), uint8(X))); };
	// outer skirt, inner skirt, accent strands, collar
	static const uint32 Pals[6][4] = {
		{ 0x2f86e8, 0xdcecf8, 0xd41a1a, 0x1e5fd0 },   // blue and white with red
		{ 0xff2d7c, 0x4b1ca6, 0x18183c, 0xff2d7c },   // hot pink and purple
		{ 0xff6a10, 0xffc21e, 0x2a3c8a, 0xffb020 },   // orange and yellow
		{ 0x15191d, 0xc4ccd4, 0x2c5cb4, 0x15191d },   // black and silver with blue
		{ 0x34e234, 0xbaf222, 0x1a2c60, 0x9ae020 },   // green and chartreuse
		{ 0xff5c12, 0x1c3c94, 0xff7c22, 0xff5c12 },   // orange and blue
	};
	const uint32* P = Pals[Pal % 6];
	const FVector Up = (Up0 - Fwd * FVector::DotProduct(Up0, Fwd)).GetSafeNormal();
	const FVector Side = FVector::CrossProduct(Up, Fwd);
	const double HeadL = .15 * Sc, HeadR = .042 * Sc;
	const FVector Back = Nose - Fwd * HeadL;
	// the clear head: a bevelled face, then a straight bullet
	CM.AddTube(Nose + Fwd * .012 * Sc, Nose, HeadR * .55, HeadR * .95, 10, FLinearColor(.85f, .92f, .95f));
	CM.AddTube(Nose, Back, HeadR * .95, HeadR, 10, FLinearColor(.85f, .92f, .95f));
	// inside it: the red insert at the nose, the leader through the middle, and the colour sleeve at the back
	M.AddTube(Nose - Fwd * .008 * Sc, Nose - Fwd * .045 * Sc, HeadR * .42, HeadR * .42, 8, C(0xff3424));
	M.AddTube(Nose + Fwd * .05, Back, .003, .003, 3, FLinearColor(.8f, .85f, .85f));
	M.AddTube(Back + Fwd * .06 * Sc, Back - Fwd * .01 * Sc, HeadR * .78, HeadR * .82, 8, C(P[3]));
	// the googly eyes, one each side, standing a little proud of the acrylic
	for (int32 Sd = -1; Sd <= 1; Sd += 2)
	{
		const FVector E = Nose - Fwd * .055 * Sc + Side * Sd * HeadR * .88 + Up * HeadR * .1;
		M.AddTube(E, E + Side * Sd * .012 * Sc, .024 * Sc, .022 * Sc, 12, FLinearColor(.96f, .96f, .94f));
		const FVector Pu = E + Side * Sd * .0125 * Sc + Up * FMath::Sin(Ph * 3.1) * .005 - Fwd * .004;
		M.AddTube(Pu, Pu + Side * Sd * .002, .012 * Sc, .011 * Sc, 10, FLinearColor(.02f, .02f, .02f));
	}
	// the skirt: strands from the collar, flaring and trailing, each one waving a little behind the one in front
	const int32 NS = 18, NSeg = 4;
	const double SkL = .42 * Sc;
	for (int32 k = 0; k < NS; k++)
	{
		const double A = 2 * PI * k / NS + .17 * (k % 3);
		const FVector Rad = Side * FMath::Cos(A) + Up * FMath::Sin(A);
		const FLinearColor Col = (k % 6 == 5) ? C(P[2]) : (k % 2 ? C(P[1]) : C(P[0]));
		const double Len = SkL * (.82 + .18 * FMath::Frac(k * .618));
		FVector Prev = Back + Rad * HeadR * .8;
		for (int32 s = 1; s <= NSeg; s++)
		{
			const double U = double(s) / NSeg;
			const double Wv = FMath::Sin(Ph * 6.5 - U * 4.0 + k * .7) * .035 * Sc * U;
			const FVector Q = Back - Fwd * Len * U + Rad * HeadR * (.8 + .9 * U) + Side * Wv + Up * Wv * .5 * FMath::Cos(k * 1.3);
			M.AddTube(Prev, Q, .0065 * Sc * (1 - .4 * U), .0065 * Sc * (1 - .4 * (U + .25)), 3, Col);
			Prev = Q;
		}
		// glitter flecks on every third strand
		if (k % 3 == 0) M.AddTube(Back - Fwd * Len * .3 + Rad * HeadR * 1.15, Back - Fwd * Len * .32 + Rad * HeadR * 1.18, .005, .004, 3, FLinearColor(1.f, 1.f, .95f) * 1.5f);
	}
	// the hook: shank inside the skirt, the bend just showing at the back
	const FLinearColor Steel(.7f, .72f, .75f);
	const FVector H0 = Back - Fwd * .05 * Sc, H1 = Back - Fwd * SkL * .72;
	M.AddTube(H0, H1, .004 * Sc, .004 * Sc, 4, Steel);
	FVector Pv = H1;
	for (int32 s = 1; s <= 5; s++)
	{
		const double A = PI * s / 5;
		const FVector Q = H1 - Fwd * FMath::Sin(A) * .035 * Sc - Up * (1 - FMath::Cos(A)) * .035 * Sc;
		M.AddTube(Pv, Q, .004 * Sc, .004 * Sc, 4, Steel);
		Pv = Q;
	}
	M.AddTube(Pv, Pv + Fwd * .03 * Sc, .004 * Sc, .001, 4, Steel);
}

bool AKeysBoatPawn::LurePos(int32 Station, FVector& Out) const
{
	if (!bSpreadOut || Station < 0 || Station >= NumStations) return false;
	Out = Lures[Station];
	return true;
}

void AKeysBoatPawn::UpdateSpread(double Dt, double T)
{
	PushTrack();
	const double Out = RigK;
	bSpreadOut = Out > .02;
	if (!bSpreadOut) { for (double& C : Cleared) C = 0; if (TrollMesh->GetNumSections()) TrollMesh->ClearAllMeshSections(); return; }

	const FBoatSpec& S = Spec();
	const double Fx = -FMath::Sin(H), Fz = -FMath::Cos(H), Rx = FMath::Cos(H), Rz = -FMath::Sin(H);
	const FTransform BT = GetActorTransform();
	const FVector Cam = Camera->GetComponentLocation() / 100.0;   // Unreal axes, meters
	// lines are thin, so they get a radius that holds about a pixel at any distance
	auto LineR = [&Cam](const FVector& PU) { return FMath::Max(.0035, (PU - Cam).Size() * .00055); };
	FVector Tip[2], Base[2];
	const bool bRiggers = KeysMesh::RiggerTips(S, FMath::SmoothStep(0.0, 1.0, RigK), Tip, Base);

	FKeysMesh M, CM;   // CM: the see-through parts (the clear mono lines and the acrylic lure heads)
	const FLinearColor LineC(.78f, .86f, .84f), Mono(.7f, .84f, .8f);
	for (int32 i = 0; i < NumStations; i++)
	{
		const FStation& St = Stations[i];
		// once a fish is on, the crew clears every other line: wound in to the boat and stowed, back out after the fight
		const bool bClear = HookedStation >= 0 && i != HookedStation;
		Cleared[i] = FMath::Clamp(Cleared[i] + (bClear ? Dt / 7.0 : -Dt / 5.0), 0.0, 1.0);
		const double Cl = FMath::SmoothStep(0.0, 1.0, Cleared[i]);
		const double Back = St.Back * Out * (1 - .92 * Cl), Side = St.Side * Out * (1 - .8 * Cl);
		double TX, TZ, LH = H;
		double LX, LZ;
		if (TrackBack(Back, TX, TZ, LH)) { LX = TX + FMath::Cos(LH) * Side; LZ = TZ - FMath::Sin(LH) * Side; }
		else { LX = X - Fx * Back + Rx * Side; LZ = Z - Fz * Back + Rz * Side; }
		const double LRx = FMath::Cos(LH), LRz = -FMath::Sin(LH);
		const double Water = WaveH(LX, LZ, T, AmpAt(LX, LZ));
		// the skip: mostly in the face of a wave, clear of it for a moment each beat, each station on its own beat
		const double Beat = FMath::Sin(T * (3.1 + i * .37) + i * 1.9), Air = FMath::Square(FMath::Max(0.0, Beat));
		const double Yank = Pulled[i] > 0 ? FMath::Min(1.0, Pulled[i] / PullS) : 0;
		const double LY = Water + .06 + Air * .5 * (1 - Yank) - Yank * PullDepth;
		// the swim: a weave across the line of travel, nose leading it, dying out while the lure is in the air
		const double SwPh = T * (2.15 + i * .43) + i * 2.3, Bite = 1 - Air * .75;
		const double Swim = FMath::Sin(SwPh) * .52 * Bite, Wag = FMath::Cos(SwPh) * .42 * Bite;
		Lures[i] = FVector(LX + LRx * Swim, LY, LZ + LRz * Swim);
		if (i == HookedStation) continue;   // that lure is in the fish's jaw now: the rod's own line runs to it
		if (Cleared[i] > .85) continue;     // stowed

		// the lure, nose toward the boat, in Unreal axes
		const double Yaw = LH + Wag;
		const FVector Fwd = G2U(FVector(-FMath::Sin(Yaw), 0, -FMath::Cos(Yaw))).GetSafeNormal();
		const FVector Up = FVector(0, 0, 1);
		const FVector Nose = G2U(Lures[i]) + Fwd * .2 + Up * (Air * .12);
		// the skirted lures from the user's photo: a clear acrylic bullet head with a red insert at the nose, big
		// googly eyes, a coloured collar and a long glittery skirt in two or three colours, the hook riding in it
		AddLure(M, CM, Nose, Fwd, Up, i == 4 ? 1.25 : 1.0, i, T + i * 1.7);

		// where the line leaves the boat: the rigger clip for the outside baits, the transom corners for the rest
		FVector From;
		if (St.Rigger >= 0 && bRiggers) From = BT.TransformPosition(Tip[St.Rigger] * 100.0) / 100.0;
		else
		{
			const double Off = St.Rigger >= 0 ? (St.Rigger == 0 ? -1 : 1) * S.Beam * .4 : St.Side * .05;
			From = BT.TransformPosition(FVector(-S.Len * .5 + .2, FMath::Clamp(Off, -S.Beam * .45, S.Beam * .45), KeysMesh::SheerAt(S, -S.Len * .5) + (i == 4 ? 2.5 : .9)) * 100.0) / 100.0;
		}
		const FVector To = G2U(Lures[i]);
		// slack follows the ride, and a long line carries a bigger belly
		const double Slack = (.1 + Air * .5) * (.55 + Back / 60);
		const int32 Seg = 16;
		FVector Prev = From;
		for (int32 s = 1; s <= Seg; s++)
		{
			const double U = double(s) / Seg;
			FVector P = FMath::Lerp(From, To, U);
			P.Z -= FMath::Sin(PI * U) * Slack;
			const double W = WaveH(P.X, P.Y, T, AmpAt(P.X, P.Y), false) - .02;
			P.Z = FMath::Max(P.Z, W);
			CM.AddTube(Prev, P, LineR(Prev), LineR(P), 3, LineC);
			Prev = P;
		}
		// on the rigger lines, the rod's own line from the gunwale holder up to the clip
		if (St.Rigger >= 0 && bRiggers)
		{
			const double Sx = St.Rigger == 0 ? -1 : 1;
			const FVector Rod = BT.TransformPosition(FVector(-S.Len * .32 + .45 - .2, Sx * (S.Beam * .5 + .05), KeysMesh::SheerAt(S, -S.Len * .32) + 2.2) * 100.0) / 100.0;
			CM.AddTube(Rod, From, LineR(Rod), LineR(From), 3, LineC);
		}
	}

	// the teasers come in with the other lines when a fish is on
	const double TeaseIn = FMath::SmoothStep(0.0, 1.0, FMath::Max3(Cleared[0], Cleared[2], FMath::Max(Cleared[1], Cleared[3])));
	// the teasers: a daisy chain of squid off each transom corner, splashing in the white water close astern
	for (int32 Sd = -1; Sd <= 1 && TeaseIn < .85; Sd += 2)
	{
		const FVector From = BT.TransformPosition(FVector(-S.Len * .5 + .2, Sd * S.Beam * .38, KeysMesh::SheerAt(S, -S.Len * .5) + .5) * 100.0) / 100.0;
		const double Back = 14 * Out * (1 - .9 * TeaseIn);
		double TX, TZ, LH = H;
		if (!TrackBack(Back, TX, TZ, LH)) { TX = X - Fx * Back; TZ = Z - Fz * Back; }
		const FVector Start(TX + FMath::Cos(LH) * Sd * 2.6 * Out, 0, TZ - FMath::Sin(LH) * Sd * 2.6 * Out);
		const FVector Fwd = G2U(FVector(-FMath::Sin(LH), 0, -FMath::Cos(LH)));
		FVector PrevU = From;
		for (int32 k = 0; k < 6; k++)
		{
			const double GX = Start.X - FMath::Sin(LH) * -k * .55, GZ = Start.Z - FMath::Cos(LH) * -k * .55;
			const double Bob2 = FMath::Max(0.0, FMath::Sin(T * 4.2 + k * .9 + Sd)) * .14;
			const FVector P = G2U(FVector(GX, WaveH(GX, GZ, T, AmpAt(GX, GZ)) + .02 + Bob2, GZ));
			CM.AddTube(PrevU, P, LineR(PrevU), LineR(P), 3, Mono);
			const FLinearColor SquidC = k == 5 ? FLinearColor(.9f, .2f, .5f) : (k % 2 ? FLinearColor(.95f, .9f, .25f) : FLinearColor(.25f, .7f, .35f));
			M.AddTube(P + Fwd * .08, P - Fwd * .22, .05, .07, 6, SquidC);
			PrevU = P - Fwd * .22;
		}
	}
	M.Commit(TrollMesh, 0, AKeysWorld::VertexColorMaterial());
	static TWeakObjectPtr<UMaterialInstanceDynamic> ClearMat;
	if (!ClearMat.IsValid())
		if (UMaterialInterface* G = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/KeysRun/Materials/M_KR_Glass.M_KR_Glass")))
		{
			ClearMat = UMaterialInstanceDynamic::Create(G, GetTransientPackage());
			ClearMat->SetScalarParameterValue(TEXT("Opacity"), .45f);
			ClearMat->AddToRoot();
		}
	if (CM.T.Num()) CM.Commit(TrollMesh, 1, ClearMat.IsValid() ? (UMaterialInterface*)ClearMat.Get() : AKeysWorld::VertexColorMaterial());
	else TrollMesh->ClearMeshSection(1);
}
