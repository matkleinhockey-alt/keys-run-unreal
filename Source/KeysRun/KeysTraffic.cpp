#include "KeysTraffic.h"
#include "KeysBoatPawn.h"
#include "KeysGeo.h"
#include "KeysMesh.h"
#include "KeysWorld.h"
#include "ProceduralMeshComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/World.h"

using namespace KeysGeo;

namespace
{
	double Clamp(double V, double A, double B) { return V < A ? A : V > B ? B : V; }
	double Lerp(double A, double B, double T) { return A + (B - A) * T; }
	double Rand(double A, double B) { return A + FMath::FRand() * (B - A); }
	double Hyp(double A, double B) { return FMath::Sqrt(A * A + B * B); }
	double SStep(double A, double B, double X) { const double T = Clamp((X - A) / (B - A), 0, 1); return T * T * (3 - 2 * T); }
	double AngLerp(double A, double B, double T)
	{
		const double D = FMath::Fmod(FMath::Fmod(B - A + PI, 2 * PI) + 2 * PI, 2 * PI) - PI;
		return A + D * FMath::Min(1.0, T);
	}
	FLinearColor Hex(uint32 C) { return FLinearColor(FColor(uint8((C >> 16) & 255), uint8((C >> 8) & 255), uint8(C & 255))); }

	// three.js boat-local (x starboard, y up, z toward the stern) to Unreal mesh-local meters (X bow, Y starboard, Z up)
	FVector P(double X, double Y, double Z) { return FVector(-Z, X, Y); }

	// ---------------------------------------------------------------- hulls (port of hullStation / hullProfile / buildHullGeo)
	struct FHullDef
	{
		double F = 1.3, Spring = .35, YK = .5, Dr0 = 20, Dr1 = 48, Rake = 1.2, Entry = .42;
		TArray<double> Steps;
		FLinearColor Bottom = Hex(0xf8f7f2), Boot = Hex(0x23466e), Hull = Hex(0xfbfaf6), Cove = Hex(0x23466e), Rub = Hex(0xd0d4d8),
			Cap = Hex(0xfbfaf6), Liner = Hex(0xf3f2ec), Deck = Hex(0xe0ded6);
		bool bCove = true;
	};

	struct FSta { double T, Z, Bb, Bs, YK, YC, YS, BF; };

	FSta Station(const FHullDef& H, double L, double B, double T)
	{
		const double E = H.Entry, BF = T < E ? 1 : FMath::Sqrt(FMath::Max(0.0, 1 - FMath::Square((T - E) / (1 - E))));
		const double E2 = FMath::Min(.9, E + .08), BFS = T < E2 ? 1 : FMath::Sqrt(FMath::Max(0.0, 1 - FMath::Square((T - E2) / (1 - E2))));
		const double Tuck = T < .06 ? .95 + .05 * T / .06 : 1;
		FSta S;
		S.T = T;
		S.BF = BF;
		S.Bb = B / 2 * .92 * BF * Tuck;
		S.Bs = B / 2 * BFS * Tuck;
		S.YK = -H.YK + FMath::Pow(FMath::Max(0.0, T - .55) / .45, 1.8) * (H.YK + H.F * .3);
		for (double St : H.Steps) if (T < St) S.YK += .05;
		S.YS = H.F + H.Spring * FMath::Pow(T, 2.2);
		const double Dr = Lerp(H.Dr0, H.Dr1, SStep(.3, 1, T)) * PI / 180;
		S.YC = FMath::Min(S.YK + S.Bb * FMath::Tan(Dr), S.YS - .2);
		S.Z = L / 2 - T * (L - H.Rake * .5);
		return S;
	}

	double ZRake(const FHullDef& H, const FSta& S, double Y)
	{
		return S.Z - H.Rake * Clamp((Y - S.YK) / FMath::Max(.01, S.YS - S.YK), 0, 1) * SStep(.62, 1, S.T);
	}

	void Profile(const FHullDef& H, const FSta& S, double B, TArray<FVector2D>& Pts, TArray<FLinearColor>& Cols)
	{
		const double Led = B * .025 * S.BF, X0 = S.Bb + Led, Y0 = S.YC + .01;
		Pts = {FVector2D(0, S.YK), FVector2D(S.Bb * .5, S.YK + (S.YC - S.YK) * .42), FVector2D(S.Bb, S.YC), FVector2D(X0, Y0)};
		Cols = {H.Bottom, H.Bottom, H.Boot};
		const double Fs[5] = {.1, .6, .68, .93, 1};
		const FLinearColor Cs[5] = {H.Boot, H.Hull, H.bCove ? H.Cove : H.Hull, H.Hull, H.Rub};
		for (int32 k = 0; k < 5; k++)
		{
			const double F = Fs[k];
			Pts.Add(FVector2D(Lerp(X0, S.Bs, F) + FMath::Sin(F * PI) * B * .012 * S.BF, Lerp(Y0, S.YS, F)));
			Cols.Add(Cs[k]);
		}
	}

	double SoleY(double L, const FSta& S) { const double D = .55 + L * .012; return S.YS - D * (1 - .4 * SStep(.55, .85, S.T)); }
	// station under boat-local z (JS st(z))
	FSta StationAtZ(const FHullDef& H, double L, double B, double Z) { return Station(H, L, B, Clamp((L / 2 - Z) / (L - H.Rake * .5), 0, 1)); }

	void BuildHull(FKeysMesh& M, const FHullDef& H, double L, double B)
	{
		const int32 NS = 36;
		TArray<FSta> Sts;
		TArray<TArray<FVector2D>> PrP;
		TArray<TArray<FLinearColor>> PrC;
		for (int32 i = 0; i <= NS; i++)
		{
			Sts.Add(Station(H, L, B, double(i) / NS));
			TArray<FVector2D> Pp;
			TArray<FLinearColor> Cc;
			Profile(H, Sts[i], B, Pp, Cc);
			PrP.Add(Pp);
			PrC.Add(Cc);
		}
		auto V = [&](const FSta& S, double X, double Y) { return P(X, Y, ZRake(H, S, Y)); };
		for (int32 i = 0; i < NS; i++)
		{
			const FSta& A = Sts[i];
			const FSta& Zs = Sts[i + 1];
			const TArray<FVector2D>& Pa = PrP[i];
			const TArray<FVector2D>& Pb = PrP[i + 1];
			for (int32 k = 0; k < Pa.Num() - 1; k++)
				for (int32 Sx = -1; Sx <= 1; Sx += 2)
					M.AddQuad(V(A, Sx * Pa[k].X, Pa[k].Y), V(A, Sx * Pa[k + 1].X, Pa[k + 1].Y), V(Zs, Sx * Pb[k + 1].X, Pb[k + 1].Y), V(Zs, Sx * Pb[k].X, Pb[k].Y),
						PrC[i][k], FVector(0, Sx, k < 2 ? -.6 : 0));
		}
		// transom
		const FSta& S0 = Sts[0];
		{
			TArray<FVector2D> Ring;
			for (int32 j = PrP[0].Num() - 1; j >= 0; j--) Ring.Add(FVector2D(-PrP[0][j].X, PrP[0][j].Y));
			for (int32 j = 1; j < PrP[0].Num(); j++) Ring.Add(PrP[0][j]);
			const double Cy = (S0.YK + S0.YS) / 2;
			for (int32 j = 0; j < Ring.Num() - 1; j++)
			{
				const double My = (Ring[j].Y + Ring[j + 1].Y) / 2;
				M.AddTri(P(0, Cy, S0.Z), P(Ring[j].X, Ring[j].Y, S0.Z), P(Ring[j + 1].X, Ring[j + 1].Y, S0.Z), My < S0.YC ? H.Bottom : H.Hull, FVector(-1, 0, 0));
			}
		}
		// deck: covering boards, liner and sole
		const double Cap = .14 + B * .02;
		auto Inn = [Cap](const FSta& S) { return FMath::Max(0.0, S.Bs - Cap); };
		for (int32 i = 0; i < NS; i++)
		{
			const FSta& A = Sts[i];
			const FSta& Zs = Sts[i + 1];
			for (int32 Sx = -1; Sx <= 1; Sx += 2)
			{
				M.AddQuad(V(A, Sx * A.Bs, A.YS + .002), V(A, Sx * Inn(A), A.YS + .01), V(Zs, Sx * Inn(Zs), Zs.YS + .01), V(Zs, Sx * Zs.Bs, Zs.YS + .002), H.Cap, FVector::ZAxisVector);
				M.AddQuad(V(A, Sx * Inn(A), A.YS), V(A, Sx * Inn(A), SoleY(L, A)), V(Zs, Sx * Inn(Zs), SoleY(L, Zs)), V(Zs, Sx * Inn(Zs), Zs.YS), H.Liner, FVector(0, -Sx, 0));
			}
			M.AddQuad(V(A, -Inn(A), SoleY(L, A)), V(A, Inn(A), SoleY(L, A)), V(Zs, Inn(Zs), SoleY(L, Zs)), V(Zs, -Inn(Zs), SoleY(L, Zs)), H.Deck, FVector::ZAxisVector);
		}
		const double Zi = S0.Z - Cap, I0 = Inn(S0), Sl = SoleY(L, S0);
		M.AddQuad(P(-I0, Sl, Zi), P(I0, Sl, Zi), P(I0, S0.YS, Zi), P(-I0, S0.YS, Zi), H.Liner, FVector(1, 0, 0));
		M.AddQuad(P(-S0.Bs, S0.YS + .01, S0.Z), P(S0.Bs, S0.YS + .01, S0.Z), P(I0, S0.YS + .01, Zi), P(-I0, S0.YS + .01, Zi), H.Cap, FVector::ZAxisVector);
	}

	// trafficHull: a Grady-like hull with traffic proportions
	FHullDef TrafficHull(double F, uint32 HullC, uint32 Boot, int64 Cove, uint32 BottomC, double Dr = 20, int64 DeckC = -1)
	{
		FHullDef H;
		H.F = F; H.Spring = .35; H.YK = .5; H.Dr0 = Dr; H.Dr1 = 48; H.Rake = 1.2; H.Entry = .42;
		H.Hull = Hex(HullC); H.Boot = Hex(Boot); H.Bottom = Hex(BottomC);
		H.bCove = Cove >= 0;
		if (H.bCove) H.Cove = Hex(uint32(Cove));
		if (DeckC >= 0) H.Deck = Hex(uint32(DeckC));
		return H;
	}

	// ---------------------------------------------------------------- props (JS-local coordinates)
	void Box(FKeysMesh& M, double W, double Hh, double D, uint32 C, double X, double Y, double Z, double RotX = 0)
	{
		M.AddBox(P(X, Y, Z), FVector(D, W, Hh), Hex(C), FQuat(FVector::YAxisVector, -RotX));
	}
	void Beam(FKeysMesh& M, const FVector& A, const FVector& B, double R, uint32 C) { M.AddTube(P(A.X, A.Y, A.Z), P(B.X, B.Y, B.Z), R, R, 6, Hex(C)); }

	// a blocky stand-in for makeHuman. Standing: Y is the feet. Seated: Y is the hip. Faces the bow (-z) unless Yaw turns it.
	struct FPerson { uint32 Shirt = 0x9cc8e0, Shorts = 0x2b3540, Skin = 0xe0a98a, Hair = 0x2b1d14; bool bBikini = false, bCap = false; };
	FPerson Blonde(uint32 Shorts) { FPerson F; F.Shorts = Shorts; F.Skin = 0xe9b48f; F.Hair = 0xead27f; F.bBikini = true; return F; }

	void Person(FKeysMesh& M, double X, double Y, double Z, bool bSeated, const FPerson& F, double Yaw = 0)
	{
		const FQuat Q(FVector::ZAxisVector, -Yaw);
		auto Add = [&](double W, double Hh, double D, uint32 C, double OX, double OY, double OZ)
		{
			M.AddBox(P(X, Y, Z) + Q.RotateVector(P(OX, OY, OZ)), FVector(D, W, Hh), Hex(C), Q);
		};
		const uint32 Torso = F.bBikini ? F.Skin : F.Shirt;
		if (bSeated)
		{
			Add(.36, .18, .5, F.Shorts, 0, 0, -.2);            // thighs
			Add(.3, .45, .14, F.Skin, 0, -.24, -.44);          // shins
			Add(.4, .55, .24, Torso, 0, .33, .02);
			if (F.bBikini) Add(.41, .12, .25, F.Shorts, 0, .42, .0);
			Add(.22, .26, .24, F.Skin, 0, .76, 0);
			Add(.25, .09, .27, F.Hair, 0, .9, .02);
			if (F.bBikini) Add(.24, .3, .06, F.Hair, 0, .68, .13);   // long hair down the back
		}
		else
		{
			Add(.34, .42, .2, F.Skin, 0, .21, 0);
			Add(.36, .4, .22, F.Shorts, 0, .62, 0);
			Add(.42, .58, .24, Torso, 0, 1.1, 0);
			Add(.22, .26, .24, F.Skin, 0, 1.53, 0);
			Add(.25, .09, .27, F.bCap ? 0x15171au : F.Hair, 0, 1.67, .02);
		}
	}

	constexpr double SeatHip = .45;   // JS seated humans sit roughly this far above their group origin

	// ---------------------------------------------------------------- racers (makeRacer)
	struct FRacerPal { uint32 H, B, C, U; uint32 S[5]; };
	const FRacerPal RacerPal[3] = {
		{0xffffff, 0xe4572e, 0xf2c14e, 0xf3efe6, {0xe4572e, 0x2bb3a8, 0xe86a92, 0xf2c14e, 0x2f6fd0}},
		{0x15171a, 0x2bd4c4, 0xe86a92, 0x15171a, {0xe86a92, 0xffffff, 0x2bd4c4, 0xf2c14e, 0xe4572e}},
		{0xf2c14e, 0x15171a, 0xe4572e, 0x15171a, {0x15171a, 0xe4572e, 0x2f6fd0, 0xffffff, 0xe86a92}}};

	void BuildRacer(FKeysMesh& M, const FRacerPal& Pal)
	{
		const double L = 12.8, B = 2.6;
		FHullDef H;
		H.F = .95; H.Spring = .12; H.YK = .55; H.Dr0 = 24; H.Dr1 = 52; H.Rake = 3.4; H.Entry = .52; H.Steps = {.27, .4};
		H.Bottom = Hex(0x15171a); H.Boot = Hex(Pal.B); H.Hull = Hex(Pal.H); H.Cove = Hex(Pal.C); H.Rub = Hex(0x111111); H.Cap = Hex(Pal.H); H.Liner = Hex(0xf2f2f2); H.Deck = Hex(0x2a2d31);
		BuildHull(M, H, L, B);
		auto Sole = [&](double Z) { return SoleY(L, StationAtZ(H, L, B, Z)); };
		Box(M, B * .82, .2, L * .24, Pal.H, 0, H.F + .06, L * .33);
		for (int32 i = 0; i < 3; i++) Box(M, B * .5, .04, .12, 0x15171a, 0, H.F + .17, L * .26 + i * .4);
		const double S1 = Sole(L * .05), S2 = Sole(L * .16);
		Box(M, B * .72, .42, .55, Pal.U, 0, S1 + .21, L * .05);
		Box(M, B * .72, .42, .55, Pal.U, 0, S2 + .21, L * .16);
		Box(M, B * .72, .55, .12, Pal.U, 0, S1 + .65, L * .05 + .3, .15);
		Box(M, B * .72, .55, .12, Pal.U, 0, S2 + .65, L * .16 + .3, .15);
		Box(M, B * .8, .5, .03, 0x6f97a8, 0, H.F + .25, -L * .06, -.5);
		Box(M, B * .6, .25, .5, 0x15171a, 0, H.F - .05, -L * .04);
		const double Cz = -L * .22, Cy = StationAtZ(H, L, B, Cz).YS;
		Box(M, B * .78, .06, L * .2, Pal.H, 0, Cy + .02, Cz);
		Box(M, B * .66, .12, L * .18, Pal.U, 0, Cy + .11, Cz);
		for (int32 Sx = -1; Sx <= 1; Sx += 2)
		{
			Box(M, .18, .7, .32, 0x15171a, Sx * .45, -.15, L / 2 + .25);
			Box(M, .32, .03, .35, 0x15171a, Sx * .45, -.4, L / 2 + .28);
		}
		FPerson Drv; Drv.Shirt = 0xffffff; Drv.Shorts = 0x1d3557; Drv.bCap = true;
		Person(M, -.45, Sole(-L * .01), -L * .01, false, Drv);
		const double Xs2[2] = {-.42, .42};
		for (int32 i = 0; i < 2; i++) Person(M, Xs2[i], S1 + .42, L * .05 - .05, true, Blonde(Pal.S[i]));
		const double Xs3[3] = {-.55, 0, .55};
		for (int32 i = 0; i < 3; i++) Person(M, Xs3[i], Cy + .17 + .05, Cz + L * .08, true, Blonde(Pal.S[(i + 2) % 5]));
	}

	// ---------------------------------------------------------------- traffic (makeTraffic + TRAFFIC_DEF)
	struct FTrafficDef
	{
		FKeysAIBoat::EKind Kind;
		uint32 Hull, Acc;
		int64 Top;          // cc T-top, -1 none
		int32 Eng;          // cc engine count
		double L;           // cc length, 0 = default 9
		int64 EngC, Crew, Shirt, Board, Pass;   // -1 = JS default / none
		const TCHAR* Route;
		double Speed;
	};
	using EK = FKeysAIBoat::EKind;
	const FTrafficDef TrafficDefs[] = {
		{EK::CC, 0xffffff, 0x23466e, 0x23466e, 2, 0, -1, 0xe86a92, -1, -1, -1, TEXT("hawk1"), 17},
		{EK::CC, 0xe9eef2, 0x2bb3a8, 0xf4f4f2, 3, 11, 0x15171a, -1, -1, -1, -1, TEXT("hawk2"), 21},
		{EK::Sport, 0xf6f6f4, 0x15171a, -1, 0, 0, -1, -1, -1, -1, -1, TEXT("off1"), 13},
		{EK::Sport, 0x1f2a44, 0xc8a24a, -1, 0, 0, -1, -1, -1, -1, -1, TEXT("off2"), 14},
		{EK::CC, 0xf2c14e, 0x15171a, 0x15171a, 2, 0, -1, -1, -1, -1, -1, TEXT("off3"), 22},
		{EK::Sail, 0xffffff, 0x1f4f8c, -1, 0, 0, -1, -1, -1, -1, -1, TEXT("hawk3"), 4},
		{EK::Sail, 0xf6f1e4, 0xc8302e, -1, 0, 0, -1, -1, -1, -1, -1, TEXT("west"), 3.5},
		{EK::JetSki, 0xffffff, 0xe4572e, -1, 0, 0, -1, -1, 0x2bb3a8, -1, -1, TEXT("harbor"), 14},
		{EK::JetSki, 0x15171a, 0x7fe3d4, -1, 0, 0, -1, -1, 0xe86a92, -1, -1, TEXT("bayE"), 16},
		{EK::CC, 0xffffff, 0x15171a, 0xffffff, 4, 12, -1, 0x2f6fd0, -1, -1, -1, TEXT("bayE"), 20},
		{EK::CC, 0xa7e6da, 0x0f6e7a, 0xf4f4f2, 1, 7, -1, -1, -1, -1, -1, TEXT("bay"), 14},
		{EK::CC, 0xffffff, 0xc8102e, 0xc8102e, 2, 0, -1, 0xf2c14e, -1, -1, -1, TEXT("reef"), 18},
		{EK::Wake, 0xffffff, 0x15171a, -1, 0, 0, -1, -1, 0xe4572e, 0xf2c14e, -1, TEXT("bayN"), 5.5},
		{EK::Wake, 0x1f2a44, 0xe4572e, -1, 0, 0, -1, -1, 0x2bb3a8, 0x7fe3d4, -1, TEXT("bayE"), 5.5},
		{EK::Wake, 0xf2f2f2, 0x2bb3a8, -1, 0, 0, -1, -1, 0xffffff, 0xe86a92, -1, TEXT("west"), 5.3},
		{EK::JetSki, 0xf2c14e, 0x15171a, -1, 0, 0, -1, -1, 0x1d3557, -1, 0xe86a92, TEXT("harbor"), 12},
		{EK::JetSki, 0x2f6fd0, 0xffffff, -1, 0, 0, -1, -1, 0xf2f2f2, -1, -1, TEXT("hawk1"), 18},
		{EK::JetSki, 0xe4572e, 0xffffff, -1, 0, 0, -1, -1, 0x2b3540, -1, 0x2bb3a8, TEXT("hawk3"), 16},
		{EK::JetSki, 0x7fe3d4, 0x15171a, -1, 0, 0, -1, -1, 0xe4572e, -1, -1, TEXT("beach"), 17},
		{EK::JetSki, 0xffffff, 0xb05bff, -1, 0, 0, -1, -1, 0x9cc8e0, -1, 0xf2c14e, TEXT("beach"), 15},
		{EK::JetSki, 0xc8102e, 0xffffff, -1, 0, 0, -1, -1, 0x15171a, -1, -1, TEXT("west"), 13},
		{EK::JetSki, 0x15171a, 0xf2c14e, -1, 0, 0, -1, -1, 0xffffff, -1, -1, TEXT("sombrero"), 16},
		{EK::JetSki, 0x9fd8ff, 0x1f4f8c, -1, 0, 0, -1, -1, 0xe86a92, -1, 0xe4572e, TEXT("sombrero"), 14}};

	uint32 Or(int64 V, uint32 Def) { return V >= 0 ? uint32(V) : Def; }

	// builds the boat into M and returns its length and beam
	void BuildTrafficBoat(FKeysMesh& M, const FTrafficDef& D, double& OutL, double& OutB)
	{
		double L = 9, B = 3;
		switch (D.Kind)
		{
		case EK::CC:
		{
			L = D.L > 0 ? D.L : 9;
			B = L * .33;
			BuildHull(M, TrafficHull(1.1, D.Hull, D.Acc, -1, 0xf2f2f0), L, B);
			Box(M, B * .42, 1.1, L * .13, 0xf4f4f2, 0, .95, 0);
			Box(M, B * .7, .08, L * .27, Or(D.Top, 0xffffff), 0, 2.85, .1);
			for (int32 Sx = -1; Sx <= 1; Sx += 2)
				for (int32 Sz = -1; Sz <= 1; Sz += 2)
					Beam(M, FVector(Sx * B * .28, .6, Sz * L * .1), FVector(Sx * B * .3, 2.8, Sz * L * .12), .035, 0xd9dde2);
			const int32 N = D.Eng > 0 ? D.Eng : 2;
			for (int32 i = 0; i < N; i++)
			{
				const double Ex = (i - (N - 1) / 2.0) * .7;
				Box(M, .58, 1.1, .7, Or(D.EngC, 0xf2f2f2), Ex, .95, L / 2 + .5);
				Box(M, .22, 1.1, .3, 0x2a2d31, Ex, -.1, L / 2 + .55);
			}
			FPerson Drv; Drv.Shirt = Or(D.Shirt, 0x9cc8e0); Drv.Shorts = 0x2b3540; Drv.bCap = true;
			Person(M, .3, .55, L * .13, false, Drv);
			if (D.Crew >= 0) Person(M, 0, .6 + SeatHip, -L * .12, true, Blonde(uint32(D.Crew)));
			break;
		}
		case EK::Sport:
		{
			L = 16; B = 4.8;
			BuildHull(M, TrafficHull(1.9, D.Hull, 0x15171a, D.Acc, 0x8a2a2a, 16), L, B);
			Box(M, B * .82, 2.1, L * .42, 0xf6f6f4, 0, 2.6, -L * .06);
			Box(M, B * .84, .55, L * .4, 0x1b232b, 0, 2.9, -L * .06);
			Box(M, B * .7, .9, L * .24, 0xf6f6f4, 0, 4.1, -L * .02);
			Box(M, B * .8, .1, L * .28, 0xf6f6f4, 0, 5.6, -L * .02);
			for (int32 Sx = -1; Sx <= 1; Sx += 2)
				for (int32 Sz = -1; Sz <= 1; Sz += 2)
					Beam(M, FVector(Sx * B * .33, 4.5, -L * .02 + Sz * L * .11), FVector(Sx * B * .36, 5.55, -L * .02 + Sz * L * .12), .04, 0xd9dde2);
			for (int32 Sx = -1; Sx <= 1; Sx += 2)
				Beam(M, FVector(Sx * B * .4, 4.6, 0), FVector(Sx * (B * .4 + 2.2), 12, L * .3), .05, 0xd9dde2);
			FPerson Drv; Drv.Shirt = 0xffffff; Drv.Shorts = 0x1d3557; Drv.bCap = true;
			Person(M, 0, 4.55, L * .06, false, Drv);
			break;
		}
		case EK::Sail:
		{
			L = 11; B = 3.5;
			BuildHull(M, TrafficHull(1.1, D.Hull, D.Acc, -1, 0x1f4f8c, 14, 0xd8cdb5), L, B);
			Box(M, 2.1, .7, 3.6, 0xf4f4f2, 0, 1.45, -.3);
			M.AddTube(P(0, 1.1, -1.4), P(0, 16.1, -1.4), .11, .08, 8, Hex(0xd9dde2));
			const FLinearColor Sailc = Hex(0xfbfaf6);
			M.AddTri(P(0, 2.1, -1.3), P(0, 15.6, -1.3), P(.25, 2.1, 3.3), Sailc, FVector::YAxisVector);
			M.AddTri(P(0, 1.3, -L * .46), P(0, 14.5, -1.5), P(.35, 1.6, -1.7), Sailc, FVector::YAxisVector);
			break;
		}
		case EK::Wake:
		{
			L = 7.4; B = 2.6;
			BuildHull(M, TrafficHull(1.2, D.Hull, D.Acc, D.Acc, 0x15171a, 14, 0x2a2d31), L, B);
			Box(M, B * .8, .35, L * .3, 0xf3efe6, 0, .95, L * .12);
			Box(M, B * .8, .35, L * .2, 0xf3efe6, 0, .95, -L * .28);
			Box(M, .5, .9, .6, 0xf4f4f2, .55, 1.05, -L * .04);
			for (int32 Sx = -1; Sx <= 1; Sx += 2)
				Beam(M, FVector(Sx * B * .46, 1.05, L * .02), FVector(Sx * B * .3, 3.1, L * .1), .05, 0x15171a);
			Box(M, B * .68, .12, .7, 0x15171a, 0, 3.12, L * .1);
			for (int32 Sx = -1; Sx <= 1; Sx += 2)
				M.AddTube(P(Sx * B * .28, 2.9, L * .1 + .05), P(Sx * B * .28, 2.9, L * .1 + .35), .16, .14, 14, Hex(0x15171a));
			FPerson Drv; Drv.Shirt = 0xffffff; Drv.Shorts = 0x1d3557; Drv.bCap = true;
			Person(M, .55, .75, 0, false, Drv);
			Person(M, -.6, .65 + SeatHip, L * .28, true, Blonde(0xe86a92));
			Person(M, .6, .65 + SeatHip, L * .28, true, Blonde(0x2bb3a8));
			break;
		}
		case EK::JetSki:
		default:
		{
			L = 3.3; B = 1.2;
			BuildHull(M, TrafficHull(.55, D.Hull, D.Acc, D.Acc, 0x15171a, 18, 0x2a2d31), L, B);
			Box(M, .45, .3, 1.2, 0x15171a, 0, .68, .3);
			Box(M, .7, .1, .12, 0x2a2d31, 0, .95, -.45);
			FPerson Rider; Rider.Shirt = Or(D.Shirt, 0xe4572e); Rider.Shorts = 0x1d3557;
			Person(M, 0, .36 + SeatHip, .25, true, Rider);
			if (D.Pass >= 0) Person(M, 0, .4 + SeatHip, .75, true, Blonde(uint32(D.Pass)));
			break;
		}
		}
		OutL = L;
		OutB = B;
	}

	// the wake-boat surfer: an oval board and a rider standing sideways, built around the group origin
	void BuildSurfer(FKeysMesh& M, uint32 BoardC, uint32 Shirt)
	{
		const int32 N = 20;
		const FLinearColor C = Hex(BoardC);
		for (int32 i = 0; i < N; i++)
		{
			const double A0 = 2 * PI * i / N, A1 = 2 * PI * (i + 1) / N;
			const FVector T0 = P(FMath::Cos(A0) * .275, .03, FMath::Sin(A0) * .95), T1 = P(FMath::Cos(A1) * .275, .03, FMath::Sin(A1) * .95);
			const FVector B0 = P(FMath::Cos(A0) * .275, -.03, FMath::Sin(A0) * .95), B1 = P(FMath::Cos(A1) * .275, -.03, FMath::Sin(A1) * .95);
			M.AddTri(P(0, .03, 0), T0, T1, C, FVector::ZAxisVector);
			M.AddTri(P(0, -.03, 0), B1, B0, C, -FVector::ZAxisVector);
			M.AddQuad(B0, B1, T1, T0, C, (T0 + T1) * .5);
		}
		FPerson F; F.Shirt = Shirt; F.Shorts = 0x2f6fd0; F.Hair = 0x3b2a1e;
		Person(M, 0, .03, 0, false, F, PI / 2);
	}

	FKeysRoute MakeNamedRoute(const FString& N)
	{
		auto C = [](double X, double Dz) { return FVector2D(X, ChainZ(X) + Dz); };
		if (N == TEXT("hawk1")) return AKeysTraffic::MakeRoute(450, 1000);
		if (N == TEXT("hawk2")) return AKeysTraffic::MakeRoute(620, 1150);
		if (N == TEXT("hawk3")) return AKeysTraffic::MakeRoute(800, 950);
		if (N == TEXT("off1")) return AKeysTraffic::MakeRoute(1900, 2700);
		if (N == TEXT("off2")) return AKeysTraffic::MakeRoute(2300, 3200);
		if (N == TEXT("off3")) return AKeysTraffic::MakeRoute(1750, 2150);
		if (N == TEXT("reef")) return AKeysTraffic::MakeRoute(1200, 1320);
		if (N == TEXT("bay")) return AKeysTraffic::MakeRoute(-700, -1150);
		if (N == TEXT("harbor")) return AKeysTraffic::RouteFrom({C(-1560, 300), C(-1100, 315), C(-680, 305), C(-1100, 295)});
		if (N == TEXT("west")) return AKeysTraffic::RouteFrom({FVector2D(1800, -2660), FVector2D(2110, -2660), FVector2D(2110, -2200), FVector2D(1880, -2300)});
		if (N == TEXT("bayE")) return AKeysTraffic::RouteFrom({FVector2D(3080, -2660), FVector2D(3600, -2660), FVector2D(3600, -1960), FVector2D(3080, -1960)});
		if (N == TEXT("bayN")) return AKeysTraffic::RouteFrom({C(-900, -520), C(200, -560), C(900, -520), C(200, -700), C(-900, -680)});
		if (N == TEXT("beach")) return AKeysTraffic::RouteFrom({FVector2D(2500, -1480), FVector2D(3200, -1470), FVector2D(4000, -1480), FVector2D(4000, -1420), FVector2D(3200, -1410), FVector2D(2500, -1420)});
		if (N == TEXT("sombrero")) return AKeysTraffic::RouteFrom({C(-150, 330), C(700, 340), C(1100, 300), C(700, 400), C(-150, 420)});
		return FKeysRoute();
	}

	bool IsLocalRoute(const FString& N)
	{
		return N == TEXT("harbor") || N == TEXT("west") || N == TEXT("bayE") || N == TEXT("bay") || N == TEXT("beach") || N == TEXT("sombrero") || N == TEXT("bayN");
	}

	// pose with bow-up pitch and starboard-up roll, the same way the player's boat is posed
	FRotator BoatRot(double H, double Pitch, double Roll)
	{
		const double Fx = -FMath::Sin(H), Fz = -FMath::Cos(H), Rx = FMath::Cos(H), Rz = -FMath::Sin(H);
		const FVector Fw(Fx, Fz, 0), Rt(Rx, Rz, 0), Up(0, 0, 1);
		const FVector Fp = Fw * FMath::Cos(Pitch) + Up * FMath::Sin(Pitch);
		const FVector Rr = Rt * FMath::Cos(Roll) + Up * FMath::Sin(Roll);
		return FRotationMatrix::MakeFromXY(Fp, Rr).Rotator();
	}
}

// ================================================================ routes
FKeysRoute AKeysTraffic::MakeRoute(double DzOut, double DzBack)
{
	FKeysRoute R;
	for (double X = 3900; X >= -3900; X -= 200) { const double Z = ChainZ(X) + DzOut; if (DepthAt(X, Z) > 2.5) R.Pts.Add(FVector2D(X, Z)); }
	for (double X = -3900; X <= 3900; X += 200) { const double Z = ChainZ(X) + DzBack; if (DepthAt(X, Z) > 2.5) R.Pts.Add(FVector2D(X, Z)); }
	const int32 N = R.Pts.Num();
	R.Cum.Add(0);
	for (int32 i = 1; i <= N; i++) R.Cum.Add(R.Cum[i - 1] + FVector2D::Distance(R.Pts[i - 1], R.Pts[i % N]));
	R.Total = N ? R.Cum[N] : 0;
	return R;
}

FKeysRoute AKeysTraffic::RouteFrom(const TArray<FVector2D>& Pts)
{
	FKeysRoute R;
	for (const FVector2D& Pt : Pts) if (DepthAt(Pt.X, Pt.Y) > 1.6) R.Pts.Add(Pt);
	const int32 N = R.Pts.Num();
	R.Cum.Add(0);
	for (int32 i = 1; i <= N; i++) R.Cum.Add(R.Cum[i - 1] + FVector2D::Distance(R.Pts[i - 1], R.Pts[i % N]));
	R.Total = N ? R.Cum[N] : 0;
	return R;
}

void AKeysTraffic::RouteAt(const FKeysRoute& R, double S, double& X, double& Z, double& TX, double& TZ)
{
	const int32 N = R.Pts.Num();
	if (N < 2 || R.Total <= 0) { X = N ? R.Pts[0].X : 0; Z = N ? R.Pts[0].Y : 0; TX = 1; TZ = 0; return; }
	S = FMath::Fmod(FMath::Fmod(S, R.Total) + R.Total, R.Total);
	int32 i = 0;
	while (i < N - 1 && R.Cum[i + 1] < S) i++;
	const FVector2D& A = R.Pts[i];
	const FVector2D& B = R.Pts[(i + 1) % N];
	const double Seg = R.Cum[i + 1] - R.Cum[i], U = Seg > 0 ? (S - R.Cum[i]) / Seg : 0;
	const double DX = B.X - A.X, DZ = B.Y - A.Y;
	double L = Hyp(DX, DZ);
	if (L <= 0) L = 1;
	X = A.X + DX * U;
	Z = A.Y + DZ * U;
	TX = DX / L;
	TZ = DZ / L;
}

double AKeysTraffic::ClosestS(const FKeysRoute& R, double X, double Z)
{
	double Best = 1e18, S = 0;
	for (int32 i = 0; i < R.Pts.Num(); i++)
	{
		const double D = FMath::Square(R.Pts[i].X - X) + FMath::Square(R.Pts[i].Y - Z);
		if (D < Best) { Best = D; S = R.Cum[i]; }
	}
	return S;
}

// ================================================================ spotClear / unstick
bool AKeysTraffic::SpotClear(double X, double Z, const KeysData::FBoatSpec& S)
{
	const double R = S.Len * .5 + 6;
	if (DepthAt(X, Z) < S.Draft + .6 || ShoreInfo(X, Z).D < R + 6) return false;
	// the marina piers and T-heads as KeysWorld::BuildMarinas lays them out
	for (const FMarina& M : Marinas())
	{
		const double Z0 = M.SZ - M.Dir * 6, Z1 = M.EZ;
		struct FDockRect { double X0, X1, Z0, Z1; };
		const FDockRect Rs[2] = {{M.SX - 1.6, M.SX + 1.6, FMath::Min(Z0, Z1), FMath::Max(Z0, Z1)}, {M.SX - 9, M.SX + 9, Z1 - 1.6, Z1 + 1.6}};
		for (const FDockRect& Rc : Rs)
		{
			const double Cx = Clamp(X, Rc.X0, Rc.X1), Cz = Clamp(Z, Rc.Z0, Rc.Z1);
			if (Hyp(X - Cx, Z - Cz) < R) return false;
		}
	}
	TArray<const FPillar*> Near;
	PillarsNear(X, Z, Near);
	for (const FPillar* Pl : Near) if (Hyp(X - Pl->X, Z - Pl->Z) < R) return false;
	return true;
}

bool AKeysTraffic::FindClearSpot(double& X, double& Z, const KeysData::FBoatSpec& S)
{
	if (SpotClear(X, Z, S)) return true;
	for (double R = 12; R <= 400; R += 12)
		for (int32 k = 0; k < 16; k++)
		{
			const double A = k / 16.0 * PI * 2, NX = X + FMath::Cos(A) * R, NZ = Z + FMath::Sin(A) * R;
			if (SpotClear(NX, NZ, S)) { X = NX; Z = NZ; return true; }
		}
	return false;
}

// ================================================================ actor
AKeysTraffic::AKeysTraffic()
{
	PrimaryActorTick.bCanEverTick = true;
	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent = Root;
}

UProceduralMeshComponent* AKeysTraffic::NewMesh(const FName& Name, USceneComponent* Parent)
{
	UProceduralMeshComponent* C = NewObject<UProceduralMeshComponent>(this, Name);
	C->SetupAttachment(Parent);
	C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	C->RegisterComponent();
	return C;
}

void AKeysTraffic::BeginPlay()
{
	Super::BeginPlay();
	KeysGeo::Init();
	if (bBuilt) return;
	bBuilt = true;
	BuildRacers();
	BuildTraffic();
}

void AKeysTraffic::BuildRacers()
{
	const double Dz[3][2] = {{880, 1250}, {1850, 2600}, {700, 1700}};
	for (int32 i = 0; i < 3; i++)
	{
		FKeysAIBoat R;
		R.Kind = FKeysAIBoat::Racer;
		R.R = MakeRoute(Dz[i][0], Dz[i][1]);
		if (R.R.Pts.Num() < 2) continue;
		R.L = 12.8;
		R.B = 2.6;
		R.S = FMath::FRand() * R.R.Total;
		R.Speed = Rand(30, 36);
		R.RelT = i * .3;
		double X, Z, TX, TZ;
		RouteAt(R.R, R.S, X, Z, TX, TZ);
		R.X = X; R.Z = Z; R.H = FMath::Atan2(-TX, -TZ);
		FKeysMesh M;
		BuildRacer(M, RacerPal[i]);
		UProceduralMeshComponent* C = NewMesh(*FString::Printf(TEXT("Racer%d"), i), Root);
		C->SetUsingAbsoluteLocation(true);
		C->SetUsingAbsoluteRotation(true);
		M.Commit(C, 0, AKeysWorld::VertexColorMaterial());
		R.Mesh = Meshes.Add(C);
		RacerBoats.Add(R);
	}
}

void AKeysTraffic::BuildTraffic()
{
	int32 i = 0;
	for (const FTrafficDef& D : TrafficDefs)
	{
		const int32 Idx = i++;
		const FString RouteName(D.Route);
		FKeysAIBoat R;
		R.Kind = D.Kind;
		R.R = MakeNamedRoute(RouteName);
		if (R.R.Pts.Num() < 2) continue;
		R.S = FMath::FRand() * R.R.Total;
		R.Speed = D.Speed * Rand(.9, 1.1);
		R.RelT = Idx * .2;
		R.bLocal = IsLocalRoute(RouteName);
		R.Ph = FMath::FRand() * 20;
		R.HopT = Rand(2, 6);
		double X, Z, TX, TZ;
		RouteAt(R.R, R.S, X, Z, TX, TZ);
		R.X = X; R.Z = Z; R.H = FMath::Atan2(-TX, -TZ);

		FKeysMesh M;
		BuildTrafficBoat(M, D, R.L, R.B);
		UProceduralMeshComponent* C = NewMesh(*FString::Printf(TEXT("Traffic%d"), Idx), Root);
		C->SetUsingAbsoluteLocation(true);
		C->SetUsingAbsoluteRotation(true);
		M.Commit(C, 0, AKeysWorld::VertexColorMaterial());
		R.Mesh = Meshes.Add(C);
		if (D.Kind == FKeysAIBoat::Wake)
		{
			FKeysMesh SM;
			BuildSurfer(SM, Or(D.Board, 0xf2c14e), Or(D.Shirt, 0x1d3557));
			UProceduralMeshComponent* SC = NewMesh(*FString::Printf(TEXT("Surfer%d"), Idx), C);
			SM.Commit(SC, 0, AKeysWorld::VertexColorMaterial());
			SC->SetRelativeLocation(P(R.B * .55, 0, R.L / 2 + 4.5) * 100.0);
			R.Surfer = Surfers.Add(SC);
		}
		TrafficBoats.Add(R);
	}
}

void AKeysTraffic::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	AKeysBoatPawn* Pw = Cast<AKeysBoatPawn>(UGameplayStatics::GetPlayerPawn(this, 0));
	if (!Pw) return;
	const double Dt = FMath::Min(double(DeltaSeconds), 1.0 / 20);
	const double T = GetWorld()->GetTimeSeconds();
	FVector2D Cam(Pw->BoatX(), Pw->BoatZ());
	if (APlayerCameraManager* CM = UGameplayStatics::GetPlayerCameraManager(this, 0))
	{
		const FVector L = CM->GetCameraLocation();
		Cam = FVector2D(L.X / 100.0, L.Y / 100.0);
	}
	UpdateRacers(Dt, T, Pw, Cam);
	UpdateTraffic(Dt, T, Pw, Cam);
}

// ================================================================ go-fast racers (updateRacers)
void AKeysTraffic::UpdateRacers(double Dt, double T, AKeysBoatPawn* Pw, const FVector2D& Cam)
{
	const double BX = Pw->BoatX(), BZ = Pw->BoatZ();
	for (FKeysAIBoat& R : RacerBoats)
	{
		R.RelT -= Dt;
		if (R.RelT <= 0)
		{
			R.RelT = 1;
			if (Hyp(R.X - BX, R.Z - BZ) > 900) R.S = ClosestS(R.R, BX, BZ) - Rand(420, 650);
		}
		R.S += R.Speed * Dt;
		double PX0, PZ0, TX, TZ;
		RouteAt(R.R, R.S, PX0, PZ0, TX, TZ);
		const double Px = -TZ, Pz = TX;
		// swing wide around the player's boat when it's in the lane ahead
		const double Rx = BX - PX0, Rz = BZ - PZ0, Along = Rx * TX + Rz * TZ, Side = Rx * Px + Rz * Pz;
		if (Along > -25 && Along < 180 && FMath::Abs(Side - R.Off) < 26) R.OffT = Side > 0 ? Side - 38 : Side + 38;
		R.OffT *= 1 - Dt * .15;
		R.Off = Lerp(R.Off, Clamp(R.OffT, -55, 55), FMath::Min(1.0, Dt * .9));
		const double X = PX0 + Px * R.Off, Z = PZ0 + Pz * R.Off;
		R.X = X;
		R.Z = Z;
		R.H = AngLerp(R.H, FMath::Atan2(-TX, -TZ), Dt * 3);
		const double Fx = -FMath::Sin(R.H), Fz = -FMath::Cos(R.H), Amp = AmpAt(X, Z) * .8;
		const double Hb = WaveH(X + Fx * 6, Z + Fz * 6, T, Amp), Hs = WaveH(X - Fx * 6, Z - Fz * 6, T, Amp);

		UProceduralMeshComponent* C = Meshes.IsValidIndex(R.Mesh) ? Meshes[R.Mesh].Get() : nullptr;
		const double DCam = Hyp(X - Cam.X, Z - Cam.Y);
		if (C)
		{
			const bool bVis = DCam < 1700;
			C->SetVisibility(bVis);
			if (bVis)
				C->SetWorldLocationAndRotation(ToUE(X, (Hb + Hs) / 2 + .32, Z),
					BoatRot(R.H, FMath::Atan2(Hb - Hs, 12.8) + .07, FMath::Sin(T * 1.3 + R.S * .01) * .03));
		}
		R.WT += Dt;
		if (R.WT > .2 && Hyp(X - BX, Z - BZ) < 800)
		{
			EmitWake(X - Fx * 6.4, Z - Fz * 6.4, R.Speed, 12.8, T, R.WT);
			EmitTrail(100 + int32(&R - RacerBoats.GetData()), X - Fx * 6.2, Z - Fz * 6.2, 1.4, FMath::Clamp((R.Speed - 2) / 8, 0.0, 1.0), T);
			R.WT = 0;
		}
	}
}

// ================================================================ everyday traffic (updateTraffic)
void AKeysTraffic::UpdateTraffic(double Dt, double T, AKeysBoatPawn* Pw, const FVector2D& Cam)
{
	for (FKeysAIBoat& R : TrafficBoats)
	{
		const double BX = Pw->BoatX(), BZ = Pw->BoatZ();
		const bool bJet = R.Kind == FKeysAIBoat::JetSki, bSail = R.Kind == FKeysAIBoat::Sail;
		R.RelT -= Dt;
		if (R.RelT <= 0)
		{
			R.RelT = 1.3;
			if (!R.bLocal && Hyp(R.X - BX, R.Z - BZ) > 1100) R.S = ClosestS(R.R, BX, BZ) - Rand(450, 800) * (FMath::FRand() < .5 ? 1 : -1);
		}
		R.S += R.Speed * Dt;
		double PX0, PZ0, TX, TZ;
		RouteAt(R.R, R.S, PX0, PZ0, TX, TZ);
		const double Px = -TZ, Pz = TX, Rx0 = BX - PX0, Rz0 = BZ - PZ0, Along = Rx0 * TX + Rz0 * TZ, Side = Rx0 * Px + Rz0 * Pz;
		if (Along > -20 && Along < 150 && FMath::Abs(Side - R.Off) < 20) R.OffT = Side > 0 ? Side - 28 : Side + 28;
		R.OffT *= 1 - Dt * .15;
		R.Off = Lerp(R.Off, Clamp(R.OffT, -45, 45), FMath::Min(1.0, Dt * .8));
		const double Weave = bJet ? FMath::Sin(T * .9 + R.Ph) * 14 + FMath::Sin(T * 2.3 + R.Ph * 1.7) * 4 : 0;
		const double X = PX0 + Px * (R.Off + Weave), Z = PZ0 + Pz * (R.Off + Weave);
		R.X = X;
		R.Z = Z;
		const double Wv = bJet ? FMath::Atan(FMath::Cos(T * .9 + R.Ph) * 14 * .9 / FMath::Max(4.0, R.Speed)) : 0;
		R.H = AngLerp(R.H, FMath::Atan2(-TX, -TZ) - Wv, Dt * (bJet ? 4 : 1.8));
		const double Fx = -FMath::Sin(R.H), Fz = -FMath::Cos(R.H), Amp = AmpAt(X, Z), Hl = R.L * .45;
		const double Rxv = FMath::Cos(R.H), Rzv = -FMath::Sin(R.H);
		const double Hb = WaveH(X + Fx * Hl, Z + Fz * Hl, T, Amp), Hs = WaveH(X - Fx * Hl, Z - Fz * Hl, T, Amp);
		const double Hp = WaveH(X - Rxv * R.B * .5, Z - Rzv * R.B * .5, T, Amp), Hst = WaveH(X + Rxv * R.B * .5, Z + Rzv * R.B * .5, T, Amp);
		const double Fr = bSail ? 0 : FMath::Min(1.0, R.Speed / 20);
		double HopY = 0, HopP = 0;
		if (bJet)
		{
			R.HopT -= Dt;
			if (R.HopT <= 0 && R.Hop == 0) R.Hop = .75;
			if (R.Hop > 0)
			{
				R.Hop -= Dt;
				const double Pp = 1 - R.Hop / .75;
				HopY = FMath::Sin(Pp * PI) * 1.3;
				HopP = FMath::Cos(Pp * PI) * .35;
				if (R.Hop <= 0) { R.Hop = 0; R.HopT = Rand(2.5, 7); }
			}
		}

		UProceduralMeshComponent* C = Meshes.IsValidIndex(R.Mesh) ? Meshes[R.Mesh].Get() : nullptr;
		const double DCam = Hyp(X - Cam.X, Z - Cam.Y);
		const bool bVis = DCam < 1700;
		if (C)
		{
			C->SetVisibility(bVis, true);
			if (bVis)
			{
				const double Pitch = FMath::Atan2(Hb - Hs, R.L) + Fr * .06 + HopP;
				const double Roll = FMath::Atan2(Hp - Hst, R.B) * .7 + (bSail ? .16 : 0) + (bJet ? -Wv * 1.4 : 0);
				C->SetWorldLocationAndRotation(ToUE(X, (Hb + Hs) / 2 + Fr * .25 + HopY, Z), BoatRot(R.H, Pitch, Roll));
			}
		}
		R.WT += Dt;
		if (R.WT > .25 && R.Speed > 3 && Hyp(X - BX, Z - BZ) < 800)
		{
			EmitWake(X - Fx * R.L * .5, Z - Fz * R.L * .5, R.Speed, R.Kind == FKeysAIBoat::Wake ? 16 : R.L, T, R.WT);
			// sailboats barely leave a wash; everything with a motor does
			const double Wash = R.Kind == FKeysAIBoat::Sail ? .15 : FMath::Clamp((R.Speed - 3) / 9, 0.0, 1.0);
			EmitTrail(200 + int32(&R - TrafficBoats.GetData()), X - Fx * R.L * .48, Z - Fz * R.L * .48, R.B * .4, Wash, T);
			R.WT = 0;
		}
		// the surfer carving back and forth on the wake
		if (Surfers.IsValidIndex(R.Surfer) && bVis)
			if (UProceduralMeshComponent* SC = Surfers[R.Surfer].Get())
			{
				const double Ph = T * .7 + R.Ph;
				SC->SetRelativeLocationAndRotation(P(R.B * .55 + FMath::Sin(Ph) * 1.3, .05 + FMath::Max(0.0, FMath::Cos(Ph * 2)) * .12, R.L / 2 + 4.5) * 100.0,
					FQuat(FVector::ZAxisVector, -FMath::Sin(Ph) * .35) * FQuat(FVector::XAxisVector, -FMath::Cos(Ph) * .28));
			}

		// keep clear of the player's hull
		const double DX = BX - X, DZ = BZ - Z, DD = Hyp(DX, DZ), RR = (R.L + Pw->Spec().Len) * .42;
		if (DD < RR && DD > .01)
		{
			Pw->NudgeBoat(DX / DD * (RR - DD), DZ / DD * (RR - DD), .6);
			if (T - R.HitT > 3)
			{
				R.HitT = T;
				Pw->Toast = TEXT("Watch it - you bumped another boat!");
				Pw->ToastUntil = T + 3.2;
			}
		}
	}
}

void AKeysTraffic::GetChartDots(TArray<FVector2D>& OutTraffic, TArray<FVector2D>& OutRacers) const
{
	OutTraffic.Reset();
	OutRacers.Reset();
	for (const FKeysAIBoat& R : TrafficBoats) OutTraffic.Add(FVector2D(R.X, R.Z));
	for (const FKeysAIBoat& R : RacerBoats) OutRacers.Add(FVector2D(R.X, R.Z));
}
