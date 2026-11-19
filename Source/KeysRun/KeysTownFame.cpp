#include "KeysTown.h"
#include "KeysTownData.h"
#include "KeysTownKit.h"
#include "KeysFame.h"
#include "KeysSave.h"
#include "KeysEvents.h"
#include "KeysAudio.h"
#include "KeysSynth.h"
#include "KeysGeo.h"
#include "KeysWorld.h"
#include "KeysBoatPawn.h"
#include "KeysKit.h"
#include "ProceduralMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Kismet/GameplayStatics.h"

// The town reacting to your fishing (KeysFame.h has the words):
//  - the Wall of Fame at the top of the marina ramp: polaroids of your heaviest fish and personal bests, rebuilt when
//    they change, with the newest pinned NEW
//  - the Boot Key Brag-Off derby board across the ramp from it: this month's heaviest fish, locals and you; the live
//    standings while a tournament runs
//  - ambient life: pelicans on the pier pilings, iguanas basking on the promenade (they bob their heads and bolt when you
//    get close), the dock cat (naps in the sun, begs at the cleaning station, comes to say hello), wind chimes that ring
//    with the wind, string lights down the pier and along the bar, OPEN signs in neon (KeysAudio plays the bar's band)

using namespace KeysTown;
using namespace KeysTownKit;

namespace
{
	constexpr double P = KeysTown::PadY;
	const FVector UpV(0, 1, 0);
	FVector V3(double U, double Y, double V) { return FVector(U, Y, -V); }
	FLinearColor Hx(uint32 C) { return KeysTownPaint::Hex(C); }
	FLinearColor Mul(const FLinearColor& A, double K) { return FLinearColor(A.R * float(K), A.G * float(K), A.B * float(K), 1); }
	double YawOf(const FVector& F) { return FMath::RadiansToDegrees(FMath::Atan2(F.Z, F.X)) - 90; }

	// the two boards stand either side of the ramp, facing the water
	constexpr double FameU0 = -7.75, DerbyU0 = 4.05, BoardV = 22.4, BoardW = 3.7;
	constexpr int32 FameSlots = 8, DerbyRows = 9;
	double SlotA(int32 i) { return .55 + (i % 4) * .87; }
	double SlotY(int32 i) { return i < 4 ? 2.2 : 1.3; }
	double SlotTilt(int32 i) { static const double T[] = {-.05, .035, -.02, .06, .04, -.045, .025, -.03}; return T[i % 8]; }

	// a string of bulbs from A to B, sagging
	void Festoon(FTownBatch& G, TArray<FVector>& Halos, const FVector& A, const FVector& B, double Sag, double Pitch, const TArray<uint32>& Cols, int32 HaloEvery)
	{
		const int32 N = FMath::Max(2, FMath::RoundToInt((B - A).Size() / Pitch));
		for (int32 k = 0; k <= N; k++)
		{
			const double T = k / double(N);
			const FVector Q = FMath::Lerp(A, B, T) - UpV * (FMath::Sin(T * PI) * Sag + .06);
			G.Ball(Q.X, Q.Y, Q.Z, .045, Cols[k % Cols.Num()], 1.25, 5);
			if (HaloEvery > 0 && k % HaloEvery == 1) Halos.Add(Q);
		}
	}
	// the wire itself (dark, thin), in the street batch
	void Wire(FTownBatch& S, const FVector& A, const FVector& B, double Sag)
	{
		const int32 N = 10;
		FVector Prev = A - UpV * .02;
		for (int32 k = 1; k <= N; k++)
		{
			const double T = k / double(N);
			const FVector Q = FMath::Lerp(A, B, T) - UpV * (FMath::Sin(T * PI) * Sag + .02);
			KitTube(S, Prev, Q, .006, .006, 3, Hx(0x1a1a1a));
			Prev = Q;
		}
	}
	// neon strokes on a wall
	void NeonLine(FTownBatch& G, const FWall& W, const TArray<FVector2D>& Pts, double D, double Wd, const FLinearColor& C, bool bClosed = false)
	{
		const int32 N = Pts.Num();
		for (int32 i = 0; i < N - (bClosed ? 0 : 1); i++)
		{
			const FVector2D A = Pts[i], E = Pts[(i + 1) % N];
			const FVector2D Dd = (E - A).GetSafeNormal(), Nn(-Dd.Y, Dd.X);
			G.Quad3(W.At(A.X + Nn.X * Wd, A.Y + Nn.Y * Wd, D), W.At(E.X + Nn.X * Wd, E.Y + Nn.Y * Wd, D), W.At(E.X - Nn.X * Wd, E.Y - Nn.Y * Wd, D), W.At(A.X - Nn.X * Wd, A.Y - Nn.Y * Wd, D), W.N, C);
		}
	}
	// "OPEN" in red neon in a blue box, centred at (A, Y) on the wall
	void OpenSign(FTownBatch& G, TArray<FVector>& Halos, const FWall& W, double A, double Y)
	{
		const double H = .22, Lw = .13, Gap = .07;
		const FLinearColor Red = Hx(0xff3b3b), Blue = Hx(0x3aa8ff);
		const double X0 = A - (Lw * 4 + Gap * 3) * .5;
		auto L = [&](int32 i, std::initializer_list<FVector2D> Pts)
		{
			TArray<FVector2D> Q;
			for (const FVector2D& p : Pts) Q.Add(FVector2D(X0 + i * (Lw + Gap) + p.X * Lw, Y - H * .5 + p.Y * H));
			NeonLine(G, W, Q, .02, .011, Red);
		};
		{
			TArray<FVector2D> O;
			for (int32 k = 0; k < 14; k++) { const double T = 2 * PI * k / 14; O.Add(FVector2D(X0 + Lw * .5 + FMath::Cos(T) * Lw * .5, Y + FMath::Sin(T) * H * .5)); }
			NeonLine(G, W, O, .02, .011, Red, true);
		}
		L(1, {{0, 0}, {0, 1}, {.75, 1}, {1, .85}, {1, .62}, {.75, .5}, {0, .5}});
		L(2, {{1, 1}, {0, 1}, {0, 0}, {1, 0}});
		L(2, {{0, .5}, {.8, .5}});
		L(3, {{0, 0}, {0, 1}, {1, 0}, {1, 1}});
		const double Hw = (Lw * 4 + Gap * 3) * .5 + .08;
		NeonLine(G, W, {FVector2D(A - Hw, Y - H * .5 - .07), FVector2D(A + Hw, Y - H * .5 - .07), FVector2D(A + Hw, Y + H * .5 + .07), FVector2D(A - Hw, Y + H * .5 + .07)}, .018, .008, Blue, true);
		Halos.Add(W.At(A, Y, .15));
	}

	// ---------------------------------------------------------------- the critters
	// a green iguana (or an orange old male) lying at Feet, facing F; Bob lifts its head (the push-up display)
	void Iguana(FTownBatch& B, const FVector& Feet, const FVector& F, double Size, double Bob, double Tail, double Run, const FLinearColor& Skin)
	{
		const FVector Fw = FVector(F.X, 0, F.Z).GetSafeNormal(), Sd = FVector::CrossProduct(UpV, Fw);
		const double S = Size;
		const FVector C = Feet + UpV * (.06 * S);
		const FLinearColor Belly = Mul(Skin, 1.15), Dark = Mul(Skin, .55), Band = Mul(Skin, .45);
		KitEllip(B, C, Fw, UpV, Sd, .2 * S, .06 * S, .08 * S, Skin, 8);
		KitEllip(B, C - UpV * .02 * S, Fw, UpV, Sd, .17 * S, .04 * S, .075 * S, Belly, 6);
		// neck and head, lifted by the bob
		const FVector Nk = C + Fw * .2 * S + UpV * (.02 + Bob * .05) * S;
		KitEllip(B, Nk, Fw, UpV, Sd, .07 * S, .045 * S, .05 * S, Skin, 6);
		const FVector Hd = C + Fw * .29 * S + UpV * (.035 + Bob * .08) * S;
		KitEllip(B, Hd, (Fw + UpV * (Bob * .3)).GetSafeNormal(), UpV, Sd, .075 * S, .038 * S, .04 * S, Skin, 7);
		// the dewlap, the big round cheek scale, the eyes
		KitEllip(B, Hd - UpV * .045 * S - Fw * .02 * S, Fw, UpV, Sd, .045 * S, .035 * S * (1 + Bob * .5), .008 * S, Mul(Skin, 1.1), 6);
		for (int32 s = -1; s <= 1; s += 2)
		{
			KitEllip(B, Hd - Fw * .025 * S + Sd * (s * .036 * S) - UpV * .008 * S, Fw, UpV, Sd, .016 * S, .016 * S, .006 * S, Hx(0xc8c8b0), 5);
			KitEllip(B, Hd + Fw * .015 * S + Sd * (s * .03 * S) + UpV * .012 * S, Fw, UpV, Sd, .008 * S, .008 * S, .006 * S, Hx(0x1a1408), 4);
		}
		// the crest of spines down the back
		for (int32 k = 0; k < 11; k++)
		{
			const double X = .3 - k * .045;
			const FVector Bs = C + Fw * X * S + UpV * ((k < 3 ? .07 + Bob * .05 : .06) * S);
			const double Hh = (k < 4 ? .035 : .025 - k * .0012) * S;
			B.Tri3(Bs - Fw * .015 * S, Bs + Fw * .012 * S, Bs + UpV * Hh - Fw * .01 * S, Sd, Dark);
			B.Tri3(Bs + Fw * .012 * S, Bs - Fw * .015 * S, Bs + UpV * Hh - Fw * .01 * S, -Sd, Dark);
		}
		// the long banded tail, swaying
		FVector Prev = C - Fw * .17 * S;
		double R = .045 * S;
		for (int32 k = 1; k <= 12; k++)
		{
			const double T = k / 12.0;
			const FVector Q = C - Fw * (.17 + T * .75) * S + Sd * (FMath::Sin(Tail + T * 3.2) * T * (.12 + Run * .1) * S) - UpV * (.04 * S * T);
			const double R2 = FMath::Lerp(.045, .006, T) * S;
			KitTube(B, Prev, Q, R, R2, 5, k % 2 ? Skin : Band);
			Prev = Q; R = R2;
		}
		// four splayed legs, scrabbling when it runs
		for (int32 f = 0; f < 2; f++)
			for (int32 s = -1; s <= 1; s += 2)
			{
				const double Ph = Run > 0 ? FMath::Sin(Tail * 2.5 + (f + (s > 0)) * PI) * .06 * Run : 0;
				const FVector Hip = C + Fw * ((f ? .12 : -.13) + Ph) * S + Sd * (s * .06 * S);
				const FVector Knee = Hip + Sd * (s * .07 * S) + UpV * .03 * S + Fw * (f ? .02 : -.03) * S;
				const FVector Foot = Feet + Sd * (s * .15 * S) + Fw * ((f ? .16 : -.1) + Ph * 2) * S;
				KitTube(B, Hip, Knee, .022 * S, .016 * S, 4, Skin);
				KitTube(B, Knee, Foot, .016 * S, .012 * S, 4, Skin);
				for (int32 t = -1; t <= 1; t++) KitTube(B, Foot, Foot + (Fw * (f ? 1.0 : -.4) + Sd * (s * .5 + t * .4)).GetSafeNormal() * .035 * S, .005 * S, .003 * S, 3, Dark);
			}
	}

	// the dock cat, an orange tabby. Pose 0 walking (Gait), 1 sitting up, 2 loafing (lying, paws tucked)
	void Cat(FTownBatch& B, const FVector& Feet, const FVector& F, int32 Pose, double Gait, double Tail, double Look)
	{
		const FVector Fw = FVector(F.X, 0, F.Z).GetSafeNormal(), Sd = FVector::CrossProduct(UpV, Fw);
		const FLinearColor Fur = Hx(0xd8893a), Stripe = Hx(0xa45a1e), White = Hx(0xf1ebe0), Pink = Hx(0xe8a0a0);
		const double Lift = Pose == 0 ? FMath::Abs(FMath::Sin(Gait)) * .008 : 0;
		FVector Chest, Rump, HeadC;
		if (Pose == 1)
		{
			Rump = Feet + UpV * .1 - Fw * .06;
			Chest = Feet + UpV * .24 + Fw * .04;
		}
		else if (Pose == 2)
		{
			Rump = Feet + UpV * .08 - Fw * .14;
			Chest = Feet + UpV * .09 + Fw * .1;
		}
		else
		{
			Rump = Feet + UpV * (.21 + Lift) - Fw * .15;
			Chest = Feet + UpV * (.22 + Lift) + Fw * .12;
		}
		const FVector Ax = (Chest - Rump).GetSafeNormal(), Mid = (Chest + Rump) * .5;
		const FVector Ay = FVector::CrossProduct(Ax, Sd).GetSafeNormal();
		KitEllip(B, Mid, Ax, Ay, Sd, (Chest - Rump).Size() * .62 + .03, Pose == 2 ? .075 : .085, .08, Fur, 8);
		// tabby stripes over the back
		for (int32 k = -2; k <= 2; k++)
		{
			const FVector Sc = Mid + Ax * (k * .05) + Ay * .07;
			KitEllip(B, Sc, Ax, UpV, Sd, .012, .02, .07, Stripe, 5);
		}
		KitEllip(B, Chest + Fw * .03 - UpV * .03, Ax, UpV, Sd, .06, .06, .055, White, 6);
		// the head: round, two ears, a white muzzle, green eyes
		const FVector Lk = (Fw * FMath::Cos(Look) + Sd * FMath::Sin(Look)).GetSafeNormal();
		HeadC = Chest + Fw * .07 + UpV * (Pose == 2 ? .05 : .09);
		const FVector LkUp = Pose == 1 ? (Lk + UpV * .45).GetSafeNormal() : Lk;
		KitEllip(B, HeadC, LkUp, UpV, FVector::CrossProduct(UpV, Lk), .06, .055, .065, Fur, 8);
		KitEllip(B, HeadC + LkUp * .05 - UpV * .015, LkUp, UpV, FVector::CrossProduct(UpV, Lk), .025, .022, .03, White, 6);
		KitEllip(B, HeadC + LkUp * .072 - UpV * .005, LkUp, UpV, FVector::CrossProduct(UpV, Lk), .007, .006, .009, Pink, 4);
		const FVector Ls = FVector::CrossProduct(UpV, Lk);
		for (int32 s = -1; s <= 1; s += 2)
		{
			const FVector E0 = HeadC + Ls * (s * .035) + UpV * .035, E1 = HeadC + Ls * (s * .055) + UpV * .045, Tip = HeadC + Ls * (s * .045) + UpV * .1;
			B.Tri3(E0, E1, Tip, Lk, Fur);
			B.Tri3(E1, E0, Tip, -Lk, Pink);
			KitEllip(B, HeadC + LkUp * .05 + Ls * (s * .024) + UpV * .018, LkUp, UpV, Ls, .006, .011, .008, Hx(0x7fae3a), 4);
		}
		// legs
		auto Leg = [&](const FVector& Top, const FVector& Foot, const FLinearColor& C) { KitTube(B, Top, Foot, .022, .016, 5, C); KitEllip(B, Foot + UpV * .01, Fw, UpV, Sd, .022, .012, .018, White, 4); };
		if (Pose == 0)
		{
			for (int32 f = 0; f < 2; f++)
				for (int32 s = -1; s <= 1; s += 2)
				{
					const double Ph = FMath::Sin(Gait + (f ? 0 : PI) + (s > 0 ? PI : 0)) * .06;
					const FVector Top = (f ? Chest : Rump) + Sd * (s * .04) - UpV * .03;
					Leg(Top, Feet + (f ? Fw * .12 : -Fw * .15) + Fw * Ph + Sd * (s * .04) + UpV * FMath::Max(0.0, Ph) * .4, Fur);
				}
		}
		else if (Pose == 1)
		{
			for (int32 s = -1; s <= 1; s += 2)
			{
				Leg(Chest + Sd * (s * .035) - UpV * .03, Feet + Fw * .08 + Sd * (s * .035), Fur);
				KitEllip(B, Rump + Sd * (s * .06) - UpV * .04, Fw, UpV, Sd, .08, .05, .035, Fur, 6);
			}
		}
		// the tail: up and curling when it walks, wrapped round the feet when it sits, flicking
		FVector Prev = Rump - Ax * .1;
		for (int32 k = 1; k <= 9; k++)
		{
			const double T = k / 9.0;
			FVector Q;
			if (Pose == 0) Q = Rump - Fw * (.1 + T * .12) + UpV * (T * .26) + Sd * (FMath::Sin(Tail + T * 2) * T * .06) + Fw * (T * T * .1);
			else Q = Rump - Fw * .08 - UpV * (Rump.Y - Feet.Y - .02) * FMath::Min(1.0, T * 2) + Sd * (FMath::Sin(T * 2.2) * .16) + Fw * (T * .18) + UpV * (FMath::Max(0.0, T - .75) * FMath::Max(0.0, FMath::Sin(Tail)) * .3);
			KitTube(B, Prev, Q, .022, .02, 5, k % 3 == 0 ? Stripe : Fur);
			Prev = Q;
		}
	}

	// a wind chime hanging at Top: a disc, tubes (or shells) on strings, the striker and its sail
	void Chime(FTownBatch& B, const FVector& Top, double T, double Wind, int32 Kind)
	{
		const double Sw = FMath::Clamp(Wind / 8.0, .2, 1.6);
		const FLinearColor Metal = Kind == 1 ? Hx(0xc8a46a) : Hx(0xb8c0c8), Disc = Hx(0x6b4a2b);
		KitTube(B, Top + UpV * .12, Top, .004, .004, 3, Hx(0x2a2a2a));
		const FVector Dc = Top + FVector(FMath::Sin(T * 1.3) * .01 * Sw, 0, FMath::Cos(T * 1.1) * .01 * Sw);
		KitTube(B, Dc, Dc - UpV * .025, .09, .09, 10, Disc);
		for (int32 k = 0; k < 6; k++)
		{
			const double A = 2 * PI * k / 6;
			const FVector Hang = Dc - UpV * .025 + FVector(FMath::Cos(A) * .07, 0, FMath::Sin(A) * .07);
			const double Len = .22 + k * .045;
			const FVector Swing(FMath::Sin(T * (2.1 + k * .37) + k) * .05 * Sw, 0, FMath::Cos(T * (1.7 + k * .29) + k * 2) * .05 * Sw);
			const FVector Tt = Hang - UpV * .05 + Swing * .3;
			KitTube(B, Hang, Tt, .002, .002, 3, Hx(0x2a2a2a));
			if (Kind == 2) KitEllip(B, Tt - UpV * .04 + Swing * .2, FVector(1, 0, 0), UpV, FVector(0, 0, 1), .03, .04, .012, Hx(k % 2 ? 0xf2e6d8 : 0xe8c8b0), 5);
			else KitTube(B, Tt, Tt - UpV * Len + Swing, .012, .012, 6, Metal);
		}
		const FVector Sw0 = Dc - UpV * .025, Sw1 = Sw0 - UpV * .2 + FVector(FMath::Sin(T * 2.4) * .04 * Sw, 0, FMath::Cos(T * 1.9) * .04 * Sw);
		KitTube(B, Sw0, Sw1, .002, .002, 3, Hx(0x2a2a2a));
		KitTube(B, Sw1, Sw1 - UpV * .012, .035, .035, 8, Disc);
		KitTube(B, Sw1 - UpV * .012, Sw1 - UpV * .2, .002, .002, 3, Hx(0x2a2a2a));
		KitBox(B, Sw1 - UpV * .27, FVector(1, 0, 0), UpV, FVector(0, 0, 1), .05, .07, .004, Kind == 2 ? Hx(0x3aa0b8) : Hx(0xc8312a));
	}
}

// ==================================================================== built once
void AKeysTown::BuildFame(FTownBatch& S, FTownBatch& G)
{
	FKitDice D(SeedOf(TEXT("wall of fame")));
	TArray<FKitText> Texts;
	TArray<FVector> Halo;
	const FVector Face(0, 0, 1);   // toward the water
	S.M.Surf = SurfWood;
	auto Board = [&](double U0, const FLinearColor& Back, const FLinearColor& Trim)
	{
		const FWall W = FWall::Make(V3(U0, P, BoardV), Face);
		const FLinearColor Post = SaltWood(D);
		for (double A : {.12, BoardW - .12}) KitBox(S, W.At(A, 1.62, -.1), W.R, UpV, W.N, .07, 1.62, .07, Post);
		KitBox(S, W.At(BoardW * .5, 1.82, -.04), W.R, UpV, W.N, BoardW * .5 - .02, 1.07, .035, Back);
		// the trim round it and a little shingle roof to keep the rain off
		for (double Y : {.75, 2.89}) KitBox(S, W.At(BoardW * .5, Y, .0), W.R, UpV, W.N, BoardW * .5, .04, .02, Trim);
		for (double A : {.04, BoardW - .04}) KitBox(S, W.At(A, 1.82, .0), W.R, UpV, W.N, .04, 1.1, .02, Trim);
		KitBox(S, W.At(BoardW * .5, 3.27, -.05), W.R, UpV, W.N, BoardW * .5 + .2, .03, .3, Weathered(Hx(0x5a6a72), .3, D));
		return W;
	};
	// ---- the Wall of Fame: cork on weathered blue, a hand-painted header with a tarpon over it
	{
		const FWall W = Board(FameU0, Hx(0xb08a5e), Weathered(Paint(EPaint::SkyBlue), .35, D));
		KitBox(S, W.At(BoardW * .5, 3.06, .0), W.R, UpV, W.N, BoardW * .5 - .05, .16, .02, Weathered(Paint(EPaint::Navy), .25, D));
		Texts.Add({TEXT("WALL OF FAME"), W.At(BoardW * .5, 3.06, .03), Face, .24, 3.2, Hx(0xf6d25a)});
		Texts.Add({TEXT("BOOT KEY HARBOR'S FINEST - BRING 'EM IN, WE'LL PIN 'EM UP"), W.At(BoardW * .5, .63, .04), Face, .07, 3.4, Hx(0x1d2a3a)});
		KitCutout(S, 0, W.At(BoardW * .5 + .3, 3.62, .02), W.R, (UpV + W.R * .3).GetSafeNormal(), 1.3, .04, Weathered(Hx(0xc9d2d8), .2, D), Weathered(Hx(0x5a7088), .2, D), Hx(0x2a3440), true);
		Buoy(S, W, -.05, 2.6, .42, D);
		// a pelican on the roof, naturally
		Pelican(S, W.At(.5, 3.3, -.05), Face, .25, D);
		// the caption slots: three lines per polaroid (name, weight and date, a marker scrawl over the photo)
		for (int32 i = 0; i < FameSlots; i++)
		{
			const FVector C = W.At(SlotA(i), SlotY(i), .03);
			const double Tl = SlotTilt(i);
			const FVector Rr = W.R * FMath::Cos(Tl) + UpV * FMath::Sin(Tl), Uu = UpV * FMath::Cos(Tl) - W.R * FMath::Sin(Tl);
			auto Cap = [&](const FVector& Pp, double Size, uint32 Col)
			{
				UTextRenderComponent* T = AddSign(TEXT(""), Pp.X, Pp.Y, -Pp.Z, Size, Hx(Col), YawOf(Face), false, 0);
				T->SetRelativeRotation(FRotator(0, 90 + YawOf(Face), FMath::RadiansToDegrees(Tl)));
				FameCaps.Add(T);
			};
			Cap(C + Uu * -.24 + W.N * .002, .068, 0x1d2a3a);
			Cap(C + Uu * -.325 + W.N * .002, .05, 0x3a3a3a);
			Cap(C + Uu * .25 - Rr * .16 + W.N * .012, .075, 0xd8231a);
		}
	}
	// ---- the derby board: whitewash, a sailfish on top
	{
		const FWall W = Board(DerbyU0, Weathered(Paint(EPaint::WhiteWash), .2, D), Weathered(Paint(EPaint::Coral), .3, D));
		KitBox(S, W.At(BoardW * .5, 3.06, .0), W.R, UpV, W.N, BoardW * .5 - .05, .16, .02, Weathered(Paint(EPaint::Coral), .2, D));
		Texts.Add({TEXT("BOOT KEY BRAG-OFF"), W.At(BoardW * .5, 3.06, .03), Face, .22, 3.2, Hx(0xfff6e0)});
		KitCutout(S, 1, W.At(BoardW * .5 - .2, 3.62, .02), W.R * -1.0, (UpV - W.R * .25).GetSafeNormal(), 1.5, .04, Weathered(Hx(0x3a5a8a), .2, D), Weathered(Hx(0x2a3a6a), .2, D), Hx(0x1a2030), true);
		Buoy(S, W, BoardW + .05, 2.6, .42, D);
		auto Txt = [&](double A, double Y, double Size, uint32 Col, EHorizTextAligment Al)
		{
			const FVector Pp = W.At(A, Y, .035);
			UTextRenderComponent* T = AddSign(TEXT(""), Pp.X, Pp.Y, -Pp.Z, Size, Hx(Col), YawOf(Face), false, 0);
			T->SetHorizontalAlignment(Al);
			DerbyTexts.Add(T);
		};
		Txt(BoardW * .5, 2.76, .085, 0x1d2a3a, EHTA_Center);   // 0: the month, or LIVE
		Txt(BoardW * .5, 2.63, .055, 0x5a5a5a, EHTA_Center);   // 1: what wins
		for (int32 r = 0; r < DerbyRows; r++)
		{
			const double Y = 2.45 - r * .17;
			Txt(.22, Y, .095, 0x1d2a3a, EHTA_Left);
			Txt(1.95, Y, .085, 0x2a4a6a, EHTA_Left);
			Txt(BoardW - .22, Y, .095, 0x1d2a3a, EHTA_Right);
		}
		Txt(BoardW * .5, .86, .06, 0x8a3a2a, EHTA_Center);     // the footer: trophies
	}
	S.M.Surf = SurfDefault;

	// ---- pelicans on the pier pilings (the caps are at 2.46 m: KeysLandmarks), facing every which way
	{
		FKitDice Pd(SeedOf(TEXT("pier pelicans")));
		const double Pile[][2] = {{1.6, -6}, {-1.6, -18}, {1.6, -30}, {-1.6, -42}, {-6, -49.6}, {12, -49.6}, {11.4, -26}, {-11.4, -38}};
		for (const auto& Pp : Pile)
		{
			const double A = Pd.R(-PI, PI);
			Pelican(S, V3(Pp[0], 2.47, Pp[1]), FVector(FMath::Cos(A), 0, FMath::Sin(A)), Pd.R(-.5, .5), Pd);
		}
	}

	// ---- string lights: down both sides of the main pier on posts, across the bar's front, the tackle shop's porch
	{
		const TArray<uint32> Warm = {0xffe2a0}, Party = {0xff5a5a, 0xffd84a, 0x5ae07a, 0x4aa8ff, 0xff8ad8};
		S.M.Surf = SurfWood;
		for (int32 s = -1; s <= 1; s += 2)
		{
			FVector Prev = FVector::ZeroVector;
			for (int32 k = 0; k < 5; k++)
			{
				const double V = -k * 12.0;
				const FVector Base = V3(s * 1.6, 2.46, V), Top = Base + UpV * 2.0;
				KitTube(S, Base, Top, .045, .05, 6, SaltWood(D));
				if (k) { Festoon(G, Halo, Prev, Top, .45, .9, Warm, 4); Wire(S, Prev, Top, .45); }
				Prev = Top;
			}
		}
		S.M.Surf = SurfDefault;
		if (const FLot* L = Lot(TEXT("bar")))
		{
			const double V = L->V0 - .25, Y = P + 3.25;
			for (double U = L->U0 + .6; U < L->U1 - 4; U += 4.2)
			{
				Festoon(G, Halo, V3(U, Y, V), V3(U + 4.2, Y, V), .3, .45, Party, 3);
				Wire(S, V3(U, Y, V), V3(U + 4.2, Y, V), .3);
			}
		}
		if (const FLot* L = Lot(TEXT("tackle")))
		{
			const double V = L->V0 - .3, Y = P + 3.0;
			for (double U = L->U0 + .8; U < L->U1 - 4; U += 4.6)
			{
				Festoon(G, Halo, V3(U, Y, V), V3(U + 4.6, Y, V), .32, .6, Warm, 3);
				Wire(S, V3(U, Y, V), V3(U + 4.6, Y, V), .32);
			}
		}
	}

	// ---- OPEN in neon on the Gutted Grouper's order side, under the ORDER HERE plank
	OpenSign(G, Halo, FWall::Make(V3(-45, P, 19.9), FVector(1, 0, 0)), 4.35, 1.95);

	for (const FVector& H : Halo) HaloList.Add({H, .55, FLinearColor(1.f, .82f, .55f), false});
	for (const FKitText& Tx : Texts) AddSign(Tx.Text, Tx.P.X, Tx.P.Y, -Tx.P.Z, Tx.Size, Tx.Ink, YawOf(Tx.Facing), Tx.bInterior, Tx.Width);

	// ---- the critters' homes
	struct FHome { double U, V, Yaw, Size; uint32 Col; };
	static const FHome Ig[] = {{-57.5, 18.7, .2, 1.0, 0x6f8f3a}, {-31.2, 18.6, 2.9, 1.25, 0x9a8a4a}, {23.5, 18.8, -.3, .9, 0x7a9a48}, {36.4, 19.4, 3.4, 1.1, 0x8a8a52},
		{-15.6, 18.6, 1.4, .8, 0x6a8a3a}, {6.5, -14.2, 0, 1.0, 0x8c9a5a}};
	for (const FHome& H : Ig)
	{
		FIguana I;
		I.Home = I.Pos = FVector2D(H.U, H.V);
		I.Yaw = I.HomeYaw = H.Yaw;
		I.Size = H.Size;
		I.Col = H.Col;
		I.T = H.U * .37;
		Iguanas.Add(I);
	}
	// the cat's rounds: the sunny end of the pier, the ramp, by the Wall of Fame, the cleaning station, under a picnic table
	CatPath = {FVector2D(.9, 2.0), FVector2D(.4, 18.7), FVector2D(-6.0, 18.9), FVector2D(-46.2, 19.75), FVector2D(-39.3, 23.3)};
	CatAt = CatPath[0];
	// the chimes: off the Gutted Grouper's sail post, by Bait & Brew's door, on the gifts shop's front
	Chimes.Add({V3(-43.1 + .35, P + 2.62, 19.5 - .1), 0, 0});
	if (const FLot* L = Lot(TEXT("bar"))) Chimes.Add({V3(L->Door - DoorW * .5 - .5, P + 2.75, L->V0 - .3), 0, 1});
	if (const FLot* L = Lot(TEXT("gifts"))) Chimes.Add({V3(L->Door + DoorW * .5 + .6, P + 2.6, L->V0 - .35), 0, 2});
}

// ==================================================================== the boards, every couple of seconds
void AKeysTown::UpdateFame()
{
	const AKeysBoatPawn* Pn = Cast<AKeysBoatPawn>(UGameplayStatics::GetPlayerPawn(this, 0));
	const UKeysSave* Sv = Pn ? Pn->Board() : nullptr;
	BarkWx = KeysFame::WeatherNow(this);
	BarkSave = Sv;
	// ---- the derby (or the live tournament)
	if (DerbyTexts.Num() == 2 + DerbyRows * 3 + 1)
	{
		TArray<FString> Txt;
		Txt.SetNum(DerbyTexts.Num());
		TArray<bool> Me;
		Me.Init(false, DerbyRows);
		const AKeysEvents* Ev = AKeysEvents::Get(this);
		if (Ev && Ev->InTourney() && Ev->TourneyPhase() != AKeysEvents::ETourney::Off && Ev->TourneyPhase() != AKeysEvents::ETourney::Over)
		{
			const AKeysEvents::ETourney Ph = Ev->TourneyPhase();
			const int32 Left = FMath::Max(0, FMath::CeilToInt(Ph == AKeysEvents::ETourney::Countdown ? Ev->CountdownLeft() : Ev->TimeLeft()));
			Txt[0] = Ph == AKeysEvents::ETourney::Countdown ? FString::Printf(TEXT("TOURNAMENT - LINES IN %d"), Left)
				: Ph == AKeysEvents::ETourney::Ceremony ? FString(TEXT("TOURNAMENT - FINAL")) : FString::Printf(TEXT("TOURNAMENT LIVE - %d:%02d LEFT"), Left / 60, Left % 60);
			{
				TArray<FString> Names;
				for (const FString& K : Ev->TourneySpecies()) Names.Add(KeysFame::SpeciesName(K));
				Txt[1] = TEXT("one of each: ") + FString::Join(Names, TEXT(", "));
			}
			TArray<AKeysEvents::FEntry> St;
			Ev->TourneyStandings(St);
			const int32 NSp = Ev->TourneySpecies().Num();
			for (int32 r = 0; r < FMath::Min(St.Num(), DerbyRows); r++)
			{
				Txt[2 + r * 3] = FString::Printf(TEXT("%d. %s"), r + 1, *St[r].Name);
				Txt[2 + r * 3 + 1] = St[r].bWon ? TEXT("WINNER") : St[r].bHome ? TEXT("in the harbor") : TEXT("fishing");
				Txt[2 + r * 3 + 2] = FString::Printf(TEXT("%d / %d"), St[r].Caught, NSp);
				Me[r] = St[r].bMe;
			}
		}
		else
		{
			TArray<KeysFame::FDerbyRow> Rows;
			FString Title;
			KeysFame::Derby(Sv, Rows, Title);
			Txt[0] = TEXT("MONTHLY DERBY - ") + Title;
			Txt[1] = TEXT("heaviest fish wins - any species - weigh in at the marina");
			const int32 MyI = Rows.IndexOfByPredicate([](const KeysFame::FDerbyRow& R) { return R.bMe; });
			for (int32 r = 0; r < DerbyRows; r++)
			{
				// you always make the board: if you're further down, the last line is yours
				const int32 Ri = (r == DerbyRows - 1 && MyI >= DerbyRows) ? MyI : r;
				if (!Rows.IsValidIndex(Ri)) continue;
				const KeysFame::FDerbyRow& R = Rows[Ri];
				Txt[2 + r * 3] = FString::Printf(TEXT("%d. %s"), R.Rank, *R.Name);
				Txt[2 + r * 3 + 1] = R.Species;
				Txt[2 + r * 3 + 2] = FString::Printf(TEXT("%.1f lb"), R.Weight);
				Me[r] = R.bMe;
			}
		}
		Txt.Last() = Sv && Sv->TourneyWins > 0 ? FString::Printf(TEXT("YOUR TOURNAMENT TROPHIES: %d"), Sv->TourneyWins) : FString(TEXT("TOURNAMENTS FROM THE PAUSE MENU - WIN ONE, GET YOUR NAME PAINTED HERE"));
		const FString Sig = FString::Join(Txt, TEXT("|")) + FString::Printf(TEXT("%d%d%d"), Me[0], Me[DerbyRows - 1], Me.Find(true));
		if (Sig != DerbyShown)
		{
			DerbyShown = Sig;
			for (int32 i = 0; i < Txt.Num(); i++) if (DerbyTexts[i]) DerbyTexts[i]->SetText(FText::FromString(Txt[i]));
			for (int32 r = 0; r < DerbyRows; r++)
				for (int32 c = 0; c < 3; c++)
					if (UTextRenderComponent* T = DerbyTexts[2 + r * 3 + c]) T->SetTextRenderColor(Me[r] ? FColor(200, 30, 26) : c == 1 ? FColor(42, 74, 106) : FColor(29, 42, 58));
		}
	}

	// ---- the Wall of Fame: rebuild the polaroids when the fish on it change
	const uint32 Sig = KeysFame::WallSig(Sv) + 1;
	if (Sig == FameSigShown) return;
	FameSigShown = Sig;
	TArray<KeysFame::FShot> Shots;
	KeysFame::WallShots(Sv, FameSlots, Shots);
	FTownBatch B;
	const FWall W = FWall::Make(V3(FameU0, P, BoardV), FVector(0, 0, 1));
	const FString Today = FDateTime::Now().ToString(TEXT("%Y-%m-%d"));
	static const TCHAR* const Mon[] = {TEXT("Jan"), TEXT("Feb"), TEXT("Mar"), TEXT("Apr"), TEXT("May"), TEXT("Jun"), TEXT("Jul"), TEXT("Aug"), TEXT("Sep"), TEXT("Oct"), TEXT("Nov"), TEXT("Dec")};
	for (int32 i = 0; i < FameSlots; i++)
	{
		const KeysFame::FShot* Sh = Shots.IsValidIndex(i) ? &Shots[i] : nullptr;
		const FVector C = W.At(SlotA(i), SlotY(i), .0);
		const double Tl = SlotTilt(i);
		const FVector Rr = W.R * FMath::Cos(Tl) + UpV * FMath::Sin(Tl), Uu = UpV * FMath::Cos(Tl) - W.R * FMath::Sin(Tl), N = W.N;
		auto At = [&](double X, double Y, double Dd) { return C + Rr * X + Uu * Y + N * Dd; };
		auto Rect = [&](double X0, double X1, double Y0, double Y1, double Dd, const FLinearColor& Col) { B.Quad3(At(X0, Y0, Dd), At(X1, Y0, Dd), At(X1, Y1, Dd), At(X0, Y1, Dd), N, Col); };
		FKitDice D(SeedOf(Sh ? *Sh->Key : TEXT("empty")) ^ SeedOf(i * 7.0, Sh ? Sh->Weight : 0.0));
		if (!Sh)
		{
			// a sun-faded patch where a photo was, and its pin
			Rect(-.3, .3, -.37, .37, .004, Hx(0xc4a272));
			if (i == 0) KitEllip(B, At(0, .33, .012), Rr, Uu, N, .018, .018, .012, Hx(0xc8312a), 5);
			if (UTextRenderComponent* T = FameCaps[i * 3]) T->SetText(FText::FromString(i == 0 ? TEXT("YOUR FISH HERE") : TEXT("")));
			for (int32 k = 1; k < 3; k++) if (UTextRenderComponent* T = FameCaps[i * 3 + k]) T->SetText(FText::GetEmpty());
			continue;
		}
		// the polaroid: a white card, the photo in it (sky, sea, you and the fish), a pin
		const double Fade = Sh->Date == Today ? .04 : .14;
		auto Ph = [&](uint32 Cc) { return Weathered(Hx(Cc), Fade, D); };
		Rect(-.31, .31, -.38, .38, .006, Hx(0xf4f2ec));
		B.TriCol(At(-.27, .38 - .04 - .26, .008), At(.27, .38 - .04 - .26, .008), At(.27, .34, .008), N, Ph(0x8fc4e0), Ph(0x8fc4e0), Ph(0x5fa2d6));
		B.Tri3(At(-.27, .08, .008), At(.27, .34, .008), At(-.27, .34, .008), N, Ph(0x6fb0dc));
		Rect(-.27, .27, -.17, .08, .008, Ph(0x2a8aa8));
		Rect(-.27, .27, .075, .085, .0085, Ph(0x1e6a88));
		// a sun or a cloud
		KitSlab(B, KitEllipse(0, 0, .035, .035, 10), At(D.Chance(.5) ? .18 : -.18, .25, .0088), Rr, Uu, N, .0005, Ph(D.Chance(.5) ? 0xfff2c0 : 0xf4f6f8), Ph(0xffffff), false);
		// you: legs, a sun shirt, a cap
		const double Pa = D.R(-.06, .06);
		Rect(Pa - .05, Pa + .05, -.17, -.02, .009, Ph(0x3a4250));
		Rect(Pa - .075, Pa + .075, -.03, .13, .0092, Ph(D.Chance(.5) ? 0x7ab8d8 : 0xe8e4d8));
		KitSlab(B, KitEllipse(0, 0, .036, .044, 10), At(Pa, .175, .0094), Rr, Uu, N, .0005, Ph(0xc98a62), Ph(0xc98a62), false);
		Rect(Pa - .04, Pa + .045, .205, .225, .0096, Ph(0x2a3a5a));
		// the fish across your chest, as big as it was
		const double FL = FMath::Clamp(.05 + .1 * FMath::Pow(FMath::Max(.5, Sh->Weight), 1.0 / 3), .16, .56);
		KitFish(B, Sh->Key, FL, At(Pa + D.R(-.02, .02), .05, .02), Rr * (i % 2 ? 1.0 : -1.0), Uu, N, .04, .03, Fade, .25, D);
		// the pin: gold for the biggest, red for a personal best, blue otherwise
		KitEllip(B, At(0, .345, .016), Rr, Uu, N, .02, .02, .014, Sh->Rank == 1 ? Hx(0xf2c23a) : Sh->bPB ? Hx(0xd8231a) : Hx(0x2a6ad8), 6);
		// the captions
		FString When;
		{
			TArray<FString> Pt;
			Sh->Date.ParseIntoArray(Pt, TEXT("-"));
			if (Sh->Date == Today) When = TEXT("today");
			else if (Pt.Num() == 3) When = FString::Printf(TEXT("%s %d"), Mon[FMath::Clamp(FCString::Atoi(*Pt[1]) - 1, 0, 11)], FCString::Atoi(*Pt[2]));
		}
		const FString L1 = Sh->Name.Len() > 17 ? Sh->Name.Left(16) + TEXT(".") : Sh->Name;
		const FString L2 = FString::Printf(TEXT("%.1f lb%s%s"), Sh->Weight, When.IsEmpty() ? TEXT("") : TEXT("  -  "), *When);
		const FString L3 = Sh->bNew ? TEXT("NEW!") : Sh->Rank == 1 ? TEXT("#1") : Sh->bPB && Sh->Rank == 0 ? TEXT("PB!") : Sh->bPB ? TEXT("PB") : TEXT("");
		if (UTextRenderComponent* T = FameCaps[i * 3]) T->SetText(FText::FromString(L1));
		if (UTextRenderComponent* T = FameCaps[i * 3 + 1]) T->SetText(FText::FromString(L2));
		if (UTextRenderComponent* T = FameCaps[i * 3 + 2]) T->SetText(FText::FromString(L3));
	}
	B.M.Commit(FameMesh, 0, AKeysWorld::VertexColorMaterial());
	UE_LOG(LogTemp, Log, TEXT("Keys Run: Wall of Fame - %d photos, %d tris"), Shots.Num(), B.Tris());
}

FString AKeysTown::BarkLine(int32 Pick) const
{
	return KeysFame::Bark(BarkSave.Get(), BarkWx, Pick);
}

// ==================================================================== the critters, every frame you're near
void AKeysTown::UpdateCritters(double T, double Dt, const FVector& CamGame)
{
	double CU, CV;
	ToLocal(CamGame.X, CamGame.Z, CU, CV);
	const bool bNear = CU > -90 && CU < 60 && CV > -70 && CV < 70 && CamGame.Y < P + 60;
	Critters->SetVisibility(bNear);
	if (!bNear) return;
	double WU = 1e9, WV = 1e9;
	if (bWalkerAshore) ToLocal(WalkerPos.X, WalkerPos.Z, WU, WV);
	auto GroundY = [](double U, double V)
	{
		double X, Z, Y = P;
		ToWorld(U, V, X, Z);
		if (!GroundAt(X, Z, Y, false)) Y = P;
		return Y;
	};
	FTownBatch B;
	// ---- iguanas: bask, bob, bolt
	for (FIguana& I : Iguanas)
	{
		I.T += Dt;
		const double Dw = FMath::Sqrt(FMath::Square(WU - I.Pos.X) + FMath::Square(WV - I.Pos.Y));
		double Run = 0;
		if (I.Mode == 0 && Dw < 3.2)
		{
			// bolt away from you along the seawall (or the dock)
			I.Mode = 1;
			const double Away = WU < I.Pos.X ? 1 : -1;
			I.To = I.Pos + FVector2D(Away * 4.5, 0);
		}
		if (I.Mode == 1 || I.Mode == 3)
		{
			const FVector2D Dd = I.To - I.Pos;
			const double Sp = I.Mode == 1 ? 3.2 : .35, Len = Dd.Size();
			if (Len < .05) { I.Mode = I.Mode == 1 ? 2 : 0; I.Wait = 0; if (I.Mode == 0) I.Yaw = I.HomeYaw; }
			else
			{
				I.Pos += Dd / Len * FMath::Min(Len, Sp * Dt);
				I.Yaw = FMath::Atan2(Dd.Y, Dd.X);
				Run = I.Mode == 1 ? 1 : .3;
			}
		}
		else if (I.Mode == 2)
		{
			// wait till you've gone, then creep back to the warm spot
			I.Wait = Dw > 9 ? I.Wait + Dt : 0;
			if (I.Wait > 6) { I.Mode = 3; I.To = I.Home; }
		}
		if (!(FMath::Abs(I.Pos.X - CU) < 70 && FMath::Abs(I.Pos.Y - CV) < 70)) continue;
		// the push-up display: three quick bobs every so often
		const double Cyc = FMath::Fmod(I.T, 7.0 + I.Size * 2);
		const double Bob = I.Mode == 0 && Cyc < 1.5 ? FMath::Abs(FMath::Sin(Cyc * PI * 2)) : 0;
		const double Tail = I.T * (Run > 0 ? 9 : .6);
		Iguana(B, V3(I.Pos.X, GroundY(I.Pos.X, I.Pos.Y), I.Pos.Y), FVector(FMath::Cos(I.Yaw), 0, -FMath::Sin(I.Yaw)), I.Size, Bob, Tail, Run, Hx(I.Col));
	}
	// ---- the dock cat
	{
		const double Dw = FMath::Sqrt(FMath::Square(WU - CatAt.X) + FMath::Square(WV - CatAt.Y));
		CatT += Dt;
		int32 Pose = 2;
		double Look = 0;
		FVector2D To = CatAt;
		if (Dw < 5 && CatMode != 1)
		{
			// you: come over, sit at your feet, look up, say something
			if (CatMode != 2) { CatMode = 2; CatSayT = T; }
			const FVector2D Me(WU, WV), Dd = Me - CatAt;
			if (Dd.Size() > .85) { To = Me - Dd.GetSafeNormal() * .8; Pose = 0; }
			else Pose = 1;
			CatYaw = FMath::Atan2(Dd.Y, Dd.X);
		}
		else
		{
			if (CatMode == 2) { CatMode = 1; CatWp = CatWp % CatPath.Num(); }
			// stay a while at the stops, then walk the round (back and forth along the path)
			if (CatMode == 0)
			{
				Pose = CatWp == 1 ? 1 : 2;
				Look = FMath::Sin(CatT * .4) * .5;
				if (CatT > 28 + (CatWp * 7) % 15) { CatMode = 1; CatT = 0; CatDir = CatWp == CatPath.Num() - 1 ? -1 : CatWp == 0 ? 1 : CatDir; CatWp += CatDir; }
			}
			if (CatMode == 1)
			{
				To = CatPath[FMath::Clamp(CatWp, 0, CatPath.Num() - 1)];
				Pose = 0;
				if ((To - CatAt).Size() < .05)
				{
					// the ramp is only a way through; the rest are stops
					if (CatWp == 1) { CatWp += CatDir; }
					else { CatMode = 0; CatT = 0; }
				}
			}
		}
		const FVector2D Dd = To - CatAt;
		const double Len = Dd.Size();
		if (Pose == 0 && Len > .01)
		{
			CatAt += Dd / Len * FMath::Min(Len, .6 * Dt);
			CatYaw = FMath::Atan2(Dd.Y, Dd.X);
			CatGait += Dt * 7.5;
		}
		if (CatMode == 2 && T - CatSayT < 2.6 && bWalkerAshore)
			KeysTags::Add(WorldPos(CatAt.X, CatAt.Y, GroundY(CatAt.X, CatAt.Y) + .6), CatSayT > 0 && FMath::Fmod(CatSayT, 2.0) < 1 ? TEXT("Mrrp?") : TEXT("Mrrrow."), 10, true);
		if (FMath::Abs(CatAt.X - CU) < 70 && FMath::Abs(CatAt.Y - CV) < 70)
			Cat(B, V3(CatAt.X, GroundY(CatAt.X, CatAt.Y), CatAt.Y), FVector(FMath::Cos(CatYaw), 0, -FMath::Sin(CatYaw)), Pose, CatGait, T * (Pose == 0 ? 3 : 1.4), Look);
	}
	// ---- the chimes: swing with the wind, ring when it gusts (you hear them from 20 m)
	{
		const double Wind = KeysGeo::WindSpeed();
		AKeysAudio* Au = AKeysAudio::Get(this);
		static const float Notes[3][5] = {{1046.5f, 1174.7f, 1318.5f, 1568.f, 1760.f}, {523.3f, 587.3f, 659.3f, 784.f, 880.f}, {2093.f, 2349.3f, 2637.f, 3136.f, 3520.f}};
		for (FChime& C : Chimes)
		{
			Chime(B, C.Top, T + C.Top.X, Wind, C.Kind);
			const FVector G = WorldPos(C.Top.X, -C.Top.Z, C.Top.Y);
			const double Dl = FVector::Dist(G, CamGame);
			if (!Au || Dl > 22) continue;
			if (T < C.NextT) continue;
			C.NextT = T + FMath::FRandRange(.6f, 3.5f) * float(7.0 / FMath::Max(2.0, Wind));
			const int32 Hits = 1 + FMath::RandRange(0, 2);
			for (int32 h = 0; h < Hits; h++)
			{
				FKeysBurst Bu;
				Bu.bTone = true;
				Bu.Wave = KeysDSP::EWave::Sine;
				Bu.Tone = Notes[C.Kind][FMath::RandRange(0, 4)];
				Bu.Gain = C.Kind == 2 ? .025f : .04f;
				Bu.Att = .002f;
				Bu.Dur = C.Kind == 1 ? 1.4f : C.Kind == 2 ? .5f : 2.6f;
				Bu.Delay = h * FMath::FRandRange(.12f, .35f);
				Au->Burst(Bu, &G, 2.5f);
				// the metal's clangy overtone
				if (C.Kind != 2) { Bu.Tone *= 2.76f; Bu.Gain *= .35f; Bu.Dur *= .5f; Au->Burst(Bu, &G, 2.5f); }
			}
		}
	}
	B.M.Commit(Critters, 0, AKeysWorld::VertexColorMaterial());
}
