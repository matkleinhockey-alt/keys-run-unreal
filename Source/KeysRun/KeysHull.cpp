// The lofted boat hulls, ported from the web game's apps/client/src/entities/boat/hull.ts (hullStation, zRake,
// hullProfile, buildHullGeo) and the per-boat HULLS table in packages/shared/src/content/boats.ts, with the
// Freeman's asymmetric catamaran sponsons from entities/boat/model.ts.
#include "KeysMesh.h"
#include "KeysData.h"

namespace
{
	double Clamp01(double V) { return V < 0 ? 0 : V > 1 ? 1 : V; }
	double SStep(double A, double B, double X) { const double T = Clamp01((X - A) / (B - A)); return T * T * (3 - 2 * T); }
	FLinearColor Hx(uint32 C) { return FLinearColor(FColor(uint8((C >> 16) & 255), uint8((C >> 8) & 255), uint8(C & 255))); }
	// web boat-local (x starboard, y up, z toward the stern) to the mesh frame (X bow, Y starboard, Z up)
	FVector P(double X, double Y, double Z) { return FVector(-Z, X, Y); }

	void Profile(const KeysMesh::FHullDef& H, const KeysMesh::FHullSta& S, double B, TArray<FVector2D>& Pts, TArray<FLinearColor>& Cols)
	{
		const double Led = B * .025 * S.BF, X0 = S.Bb + Led, Y0 = S.YC + .01;
		Pts = {FVector2D(0, S.YK), FVector2D(S.Bb * .5, S.YK + (S.YC - S.YK) * .42), FVector2D(S.Bb, S.YC), FVector2D(X0, Y0)};
		Cols = {H.Bottom, H.Bottom, H.Boot};
		const double Fs[5] = {.1, .6, .68, .93, 1};
		const FLinearColor Cs[5] = {H.Boot, H.Hull, H.bCove ? H.Cove : H.Hull, H.Hull, H.Rub};
		for (int32 k = 0; k < 5; k++)
		{
			const double F = Fs[k];
			Pts.Add(FVector2D(FMath::Lerp(X0, S.Bs, F) + FMath::Sin(F * PI) * B * .012 * S.BF, FMath::Lerp(Y0, S.YS, F)));
			Cols.Add(Cs[k]);
		}
	}
}

namespace KeysMesh
{
	FHullSta HullStation(const FHullDef& H, double L, double B, double T)
	{
		const double E = H.Entry, BF = T < E ? 1 : FMath::Sqrt(FMath::Max(0.0, 1 - FMath::Square((T - E) / (1 - E))));
		const double E2 = FMath::Min(.9, E + .08), BFS = T < E2 ? 1 : FMath::Sqrt(FMath::Max(0.0, 1 - FMath::Square((T - E2) / (1 - E2))));
		const double Tuck = T < .06 ? .95 + .05 * T / .06 : 1;
		FHullSta S;
		S.T = T;
		S.BF = BF;
		S.Bb = B / 2 * .92 * BF * Tuck;
		S.Bs = B / 2 * BFS * Tuck;
		S.YK = -H.YK + FMath::Pow(FMath::Max(0.0, T - .55) / .45, 1.8) * (H.YK + H.F * .3);
		for (double St : H.Steps) if (T < St) S.YK += .05;
		S.YS = H.F + H.Spring * FMath::Pow(T, 2.2);
		const double Dr = FMath::Lerp(H.Dr0, H.Dr1, SStep(.3, 1, T)) * PI / 180;
		S.YC = FMath::Min(S.YK + S.Bb * FMath::Tan(Dr), S.YS - .2);
		S.Z = L / 2 - T * (L - H.Rake * .5);
		return S;
	}

	FHullSta HullStationAtZ(const FHullDef& H, double L, double B, double Z) { return HullStation(H, L, B, Clamp01((L / 2 - Z) / (L - H.Rake * .5))); }

	double HullZRake(const FHullDef& H, const FHullSta& S, double Y)
	{
		return S.Z - H.Rake * Clamp01((Y - S.YK) / FMath::Max(.01, S.YS - S.YK)) * SStep(.62, 1, S.T);
	}

	double HullSole(double L, const FHullSta& S) { const double D = .55 + L * .012; return S.YS - D * (1 - .4 * SStep(.55, .85, S.T)); }

	void BuildHullLoft(FKeysMesh& M, const FHullDef& H, double L, double B, bool bDeck, double Xo, int32 AsymSide, double AsymK)
	{
		// 36 even stations as in the web game, plus a pair hard either side of each step so the step reads as a sharp
		// notch across the running bottom instead of a soft ramp over a whole station
		TArray<double> Ts;
		const int32 NS = 36;
		for (int32 i = 0; i <= NS; i++) Ts.Add(double(i) / NS);
		for (double St : H.Steps) { Ts.Add(St - .0035); Ts.Add(St + .0035); }
		Ts.Sort();
		const int32 N = Ts.Num();
		TArray<FHullSta> Sts;
		TArray<TArray<FVector2D>> PrP[2];
		TArray<TArray<FLinearColor>> PrC[2];
		for (int32 i = 0; i < N; i++)
		{
			Sts.Add(HullStation(H, L, B, Ts[i]));
			for (int32 h = 0; h < 2; h++)
			{
				const int32 Sx = h == 0 ? -1 : 1;
				FHullSta St = Sts[i];
				// the inboard half of a cat sponson: chine pulled down toward the keel, so the bottom flattens and the
				// side above it stands up as the tunnel wall
				if (AsymSide != 0 && Sx == AsymSide) St.YC = St.YK + (St.YC - St.YK) * AsymK;
				TArray<FVector2D> Pp;
				TArray<FLinearColor> Cc;
				Profile(H, St, B, Pp, Cc);
				PrP[h].Add(Pp);
				PrC[h].Add(Cc);
			}
		}
		const FLinearColor StepC = H.Bottom * .45f;
		auto V = [&](const FHullSta& S, double X, double Y) { return P(X + Xo, Y, HullZRake(H, S, Y)); };
		for (int32 i = 0; i < N - 1; i++)
		{
			const FHullSta& A = Sts[i];
			const FHullSta& Zs = Sts[i + 1];
			bool bStep = false;
			for (double St : H.Steps) if (A.T < St && Zs.T > St) bStep = true;
			for (int32 h = 0; h < 2; h++)
			{
				const int32 Sx = h == 0 ? -1 : 1;
				const TArray<FVector2D>& Pa = PrP[h][i];
				const TArray<FVector2D>& Pb = PrP[h][i + 1];
				for (int32 k = 0; k < Pa.Num() - 1; k++)
					M.AddQuad(V(A, Sx * Pa[k].X, Pa[k].Y), V(A, Sx * Pa[k + 1].X, Pa[k + 1].Y), V(Zs, Sx * Pb[k + 1].X, Pb[k + 1].Y), V(Zs, Sx * Pb[k].X, Pb[k].Y),
						bStep && k < 2 ? StepC : PrC[h][i][k], FVector(bStep && k < 2 ? -1 : 0, Sx, k < 2 ? -.6 : 0));
			}
		}
		// transom: each half of the ring from its own profile, so an asymmetric hull's chines don't jump at the stern
		const FHullSta& S0 = Sts[0];
		{
			TArray<FVector2D> Ring;
			for (int32 j = PrP[0][0].Num() - 1; j >= 0; j--) Ring.Add(FVector2D(-PrP[0][0][j].X, PrP[0][0][j].Y));
			for (int32 j = 1; j < PrP[1][0].Num(); j++) Ring.Add(PrP[1][0][j]);
			const double Cy = (S0.YK + S0.YS) / 2;
			for (int32 j = 0; j < Ring.Num() - 1; j++)
			{
				const double My = (Ring[j].Y + Ring[j + 1].Y) / 2;
				M.AddTri(P(Xo, Cy, S0.Z), P(Ring[j].X + Xo, Ring[j].Y, S0.Z), P(Ring[j + 1].X + Xo, Ring[j + 1].Y, S0.Z), My < S0.YC ? H.Bottom : H.Hull, FVector(-1, 0, 0));
			}
		}
		if (!bDeck) return;
		// covering boards, the liner down to the sole, and the sole
		const double Cap = .14 + B * .02;
		auto Inn = [Cap](const FHullSta& S) { return FMath::Max(0.0, S.Bs - Cap); };
		for (int32 i = 0; i < N - 1; i++)
		{
			const FHullSta& A = Sts[i];
			const FHullSta& Zs = Sts[i + 1];
			for (int32 Sx = -1; Sx <= 1; Sx += 2)
			{
				M.AddQuad(V(A, Sx * A.Bs, A.YS + .002), V(A, Sx * Inn(A), A.YS + .01), V(Zs, Sx * Inn(Zs), Zs.YS + .01), V(Zs, Sx * Zs.Bs, Zs.YS + .002), H.Cap, FVector::ZAxisVector);
				M.AddQuad(V(A, Sx * Inn(A), A.YS), V(A, Sx * Inn(A), HullSole(L, A)), V(Zs, Sx * Inn(Zs), HullSole(L, Zs)), V(Zs, Sx * Inn(Zs), Zs.YS), H.Liner, FVector(0, -Sx, 0));
			}
			M.AddQuad(V(A, -Inn(A), HullSole(L, A)), V(A, Inn(A), HullSole(L, A)), V(Zs, Inn(Zs), HullSole(L, Zs)), V(Zs, -Inn(Zs), HullSole(L, Zs)), H.Deck, FVector::ZAxisVector);
		}
		const double Zi = S0.Z - Cap, I0 = Inn(S0), Sl = HullSole(L, S0);
		M.AddQuad(P(-I0 + Xo, Sl, Zi), P(I0 + Xo, Sl, Zi), P(I0 + Xo, S0.YS, Zi), P(-I0 + Xo, S0.YS, Zi), H.Liner, FVector(1, 0, 0));
		M.AddQuad(P(-S0.Bs + Xo, S0.YS + .01, S0.Z), P(S0.Bs + Xo, S0.YS + .01, S0.Z), P(I0 + Xo, S0.YS + .01, Zi), P(-I0 + Xo, S0.YS + .01, Zi), H.Cap, FVector::ZAxisVector);
	}

	void BuildHullFor(FKeysMesh& M, const FHullDef& H, double L, double B)
	{
		if (!H.bCat)
		{
			BuildHullLoft(M, H, L, B, true);
			return;
		}
		// A performance catamaran, not two pontoons (web model.ts): slim sponsons, a fine entry, and each sponson
		// asymmetric - a near-vertical inboard wall over a flat inboard bottom - under a flat-bottomed bridge deck.
		const double Bs = B * .29;
		FHullDef Hs = H;
		Hs.F = H.Tunnel + .2; Hs.Spring = .16; Hs.Entry = .4; Hs.Dr0 = H.Dr0 + 4;
		FHullDef Hu = H;
		Hu.YK = -H.Tunnel; Hu.Dr0 = 0; Hu.Dr1 = 0; Hu.Steps.Reset(); Hu.Entry = .5;
		for (int32 Sx = -1; Sx <= 1; Sx += 2) BuildHullLoft(M, Hs, L * .99, Bs, false, Sx * (B / 2 - Bs / 2), -Sx, .34);
		BuildHullLoft(M, Hu, L, B, true);
	}

	const FHullDef& HullOf(const KeysData::FBoatSpec& S)
	{
		struct FRow { const TCHAR* Id; double F, Spring, YK, Dr0, Dr1, Rake, Entry; double St0, St1; uint32 Bottom, Boot, Hull; int64 Cove; uint32 Rub, Cap, Liner, Deck; bool bCat; };
		// content/boats.ts HULLS
		static const FRow Rows[] = {
			{TEXT("robalo"),    1.05, .38, .45, 20, 48, .9,  .42, 0,   0,   0xf4f4f2, 0x6fa8c9, 0xffffff, -1,       0x8e949a, 0xffffff, 0xf1f1ee, 0xdcddd8, false},
			{TEXT("grady"),     1.3,  .52, .55, 20, 50, 1.3, .40, 0,   0,   0xf8f7f2, 0x23466e, 0xfbfaf6, 0x23466e, 0xd0d4d8, 0xfbfaf6, 0xf3f2ec, 0xe0ded6, false},
			{TEXT("freeman"),   1.45, .45, .55, 22, 50, .35, .50, .36, 0,   0xcfd3d6, 0x1f2a44, 0xd5d9dc, 0x1f2a44, 0x1f2a44, 0xf4f5f5, 0xf3f2ee, 0xdedcd6, true},
			{TEXT("midnight"),  1.4,  .45, .62, 22, 54, 2.1, .46, .3,  .44, 0x5a544b, 0x2a2620, 0x8f8270, 0x2a2620, 0x101112, 0x9c8f7c, 0x9c8f7c, 0xb8875a, false},
			{TEXT("mti"),       1.35, .3,  .6,  24, 55, 2.3, .48, .28, .42, 0x15171a, 0xf4f4f4, 0xc8102e, 0xf4f4f4, 0x0d0e10, 0xf4f4f4, 0xf4f4f4, 0x2a2d31, false},
			{TEXT("yellowfin"), 1.32, .46, .56, 21, 51, 1.5, .56, 0,   0,   0xf2efe2, 0x1f3b57, 0xe9d98a, 0x1f3b57, 0x8e949a, 0xf6f4ea, 0xf3f1e6, 0xe2decd, false},
			{TEXT("contender"), 1.18, .44, .5,  20, 49, 1.2, .46, 0,   0,   0xf4f6f5, 0x2f7a52, 0xf2f4f5, 0x2f7a52, 0xb9bec4, 0xf7f8f8, 0xf2f2ee, 0xdedfd9, false},
			{TEXT("nortech"),   1.22, .26, .68, 25, 57, 2.6, .5,  .26, .4,  0x14161a, 0xff7a18, 0x14161a, 0xff7a18, 0x0b0c0e, 0x1b1e23, 0x22262c, 0x1b1e23, false},
		};
		// built once for all eight, so the references handed out stay put
		static const TArray<TPair<FString, FHullDef>> Built = []()
		{
			TArray<TPair<FString, FHullDef>> Out;
			for (const FRow& R : Rows)
			{
				FHullDef H;
				H.F = R.F; H.Spring = R.Spring; H.YK = R.YK; H.Dr0 = R.Dr0; H.Dr1 = R.Dr1; H.Rake = R.Rake; H.Entry = R.Entry;
				if (R.St0 > 0) H.Steps.Add(R.St0);
				if (R.St1 > 0) H.Steps.Add(R.St1);
				H.Bottom = Hx(R.Bottom); H.Boot = Hx(R.Boot); H.Hull = Hx(R.Hull);
				H.bCove = R.Cove >= 0;
				if (H.bCove) H.Cove = Hx(uint32(R.Cove));
				H.Rub = Hx(R.Rub); H.Cap = Hx(R.Cap); H.Liner = Hx(R.Liner); H.Deck = Hx(R.Deck);
				H.bCat = R.bCat;
				Out.Add(TPair<FString, FHullDef>(R.Id, H));
			}
			return Out;
		}();
		for (const TPair<FString, FHullDef>& B : Built) if (B.Key == S.Id) return B.Value;
		static const FHullDef Default;
		return Default;
	}

	double SoleAt(const KeysData::FBoatSpec& S, double X)
	{
		const FHullDef& H = HullOf(S);
		return HullSole(S.Len, HullStationAtZ(H, S.Len, S.Beam, -X));
	}

	double SheerAt(const KeysData::FBoatSpec& S, double X)
	{
		const FHullDef& H = HullOf(S);
		return HullStationAtZ(H, S.Len, S.Beam, -X).YS;
	}

	double InnerHalfBeam(const KeysData::FBoatSpec& S, double X)
	{
		const FHullDef& H = HullOf(S);
		return FMath::Max(0.0, HullStationAtZ(H, S.Len, S.Beam, -X).Bs - (.14 + S.Beam * .02));
	}

	double BowX(const KeysData::FBoatSpec& S) { return S.Len / 2 + HullOf(S).Rake * .5; }
	double SternX(const KeysData::FBoatSpec& S) { return -S.Len / 2; }
}
