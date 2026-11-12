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
		// tumblehome: the topsides swell out below the sheer and roll back in to it
		const double Tb = H.Tumble * (1 - SStep(0, .32, S.T));
		Pts = {FVector2D(0, S.YK), FVector2D(S.Bb * .5, S.YK + (S.YC - S.YK) * .42), FVector2D(S.Bb, S.YC), FVector2D(X0, Y0)};
		Cols = {H.Bottom, H.Bottom, H.Boot};
		const double Fs[5] = {.1, .6, .68, .93, 1};
		const FLinearColor Cs[5] = {H.Boot, H.Hull, H.bCove ? H.Cove : H.Hull, H.Hull, H.Rub};
		for (int32 k = 0; k < 5; k++)
		{
			const double F = Fs[k];
			Pts.Add(FVector2D(FMath::Lerp(X0, S.Bs, F) + FMath::Sin(F * PI) * B * .012 * S.BF + Tb * B * .5 * FMath::Pow(FMath::Sin(F * PI), .6), FMath::Lerp(Y0, S.YS, F)));
			Cols.Add(Cs[k]);
		}
	}
}

namespace KeysMesh
{
	FHullSta HullStation(const FHullDef& H, double L, double B, double T)
	{
		const double E = H.Entry, BF = T < E ? 1 : FMath::Sqrt(FMath::Max(0.0, 1 - FMath::Square((T - E) / (1 - E))));
		const double E2 = FMath::Min(.9, E + .08 + H.Flare), BFS = T < E2 ? 1 : FMath::Sqrt(FMath::Max(0.0, 1 - FMath::Square((T - E2) / (1 - E2))));
		const double Tuck = T < .06 ? .95 + .05 * T / .06 : 1;
		FHullSta S;
		S.T = T;
		S.BF = BF;
		S.Bb = B / 2 * .92 * BF * Tuck;
		S.Bs = B / 2 * FMath::Max(H.MinBow, BFS) * Tuck * (1 - H.Tumble * (1 - SStep(0, .32, T)));
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

	// covering boards, the liner down to the sole, and the sole, inside the sheer of these stations
	static void BuildDeckIn(FKeysMesh& M, const FHullDef& H, double L, double B, const TArray<FHullSta>& Sts, double Xo)
	{
		const int32 N = Sts.Num();
		auto V = [&](const FHullSta& S, double X, double Y) { return P(X + Xo, Y, HullZRake(H, S, Y)); };
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
		const FHullSta& S0 = Sts[0];
		const double Zi = S0.Z - Cap, I0 = Inn(S0), Sl = HullSole(L, S0);
		M.AddQuad(P(-I0 + Xo, Sl, Zi), P(I0 + Xo, Sl, Zi), P(I0 + Xo, S0.YS, Zi), P(-I0 + Xo, S0.YS, Zi), H.Liner, FVector(1, 0, 0));
		M.AddQuad(P(-S0.Bs + Xo, S0.YS + .01, S0.Z), P(S0.Bs + Xo, S0.YS + .01, S0.Z), P(I0 + Xo, S0.YS + .01, Zi), P(-I0 + Xo, S0.YS + .01, Zi), H.Cap, FVector::ZAxisVector);
		// a blunt bow (the cat's): the liner closes across the forepeak
		const FHullSta& S1 = Sts[N - 1];
		const double I1 = Inn(S1);
		if (I1 > .05)
		{
			const double Zf = S1.Z + Cap * .3;
			M.AddQuad(P(-I1 + Xo, HullSole(L, S1), Zf), P(I1 + Xo, HullSole(L, S1), Zf), P(I1 + Xo, S1.YS, Zf), P(-I1 + Xo, S1.YS, Zf), H.Liner, FVector(-1, 0, 0));
			M.AddQuad(P(-S1.Bs + Xo, S1.YS + .01, S1.Z), P(S1.Bs + Xo, S1.YS + .01, S1.Z), P(I1 + Xo, S1.YS + .01, Zf), P(-I1 + Xo, S1.YS + .01, Zf), H.Cap, FVector::ZAxisVector);
		}
	}

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
		if (bDeck) BuildDeckIn(M, H, L, B, Sts, Xo);
	}

	void BuildHullFor(FKeysMesh& M, const FHullDef& H, double L, double B)
	{
		if (!H.bCat)
		{
			BuildHullLoft(M, H, L, B, true);
			return;
		}
		// A performance catamaran after the owner's photos: tall, flat topsides with a crisp chine, two slim asymmetric
		// sponsons (a steep inboard bottom and a vertical tunnel wall) whose stems stand near vertical at a blunt twin
		// bow, and a high tunnel roof under the bridge deck that climbs toward the bow. Built here station by station
		// rather than from the monohull loft; the sheer, the deck and the sole are still HullStation's.
		const int32 NS = 56;
		TArray<FHullSta> Sts;
		for (int32 i = 0; i <= NS; i++) Sts.Add(HullStation(H, L, B, double(i) / NS));
		const double Rad40 = FMath::DegreesToRadians(40.0);
		// one half's cross-section, from the tunnel's centreline round the sponson to the sheer, and each edge's paint
		struct FSec { TArray<FVector2D> P; TArray<FLinearColor> C; TArray<FVector> Out; int32 Keel = 0, Roof = 0; };
		auto Section = [&](const FHullSta& S)
		{
			FSec O;
			const double T = S.T;
			const double Wf = T < .45 ? 1 : FMath::Sqrt(FMath::Max(0.0, 1 - FMath::Square((T - .45) / .55)));
			const double Bo = S.Bs - .07 * (1 - T) - .02;                 // the chine: a little flare in the topsides
			const double W = FMath::Max(.035, H.SponW * B * Wf);
			const double Ki = Bo - W, Kc = Bo - W * .55;
			const double Yk = -H.YK + (H.YK + H.StemFoot) * FMath::Pow(SStep(.74, 1, T), 1.6);
			const double Dr = FMath::DegreesToRadians(FMath::Lerp(H.Dr0, H.Dr1, SStep(.3, 1, T)));
			const double Yc = FMath::Min(Yk + (Bo - Kc) * FMath::Tan(Dr) + .06 * SStep(.6, 1, T), S.YS - .3);
			const double Yt = FMath::Max(Yk + .2, FMath::Min(H.Tunnel + H.TunnelRise * FMath::Pow(T, 1.5), HullSole(L, S) - .12));
			const double Yci = FMath::Min(Yk + (Kc - Ki) * FMath::Tan(Rad40), Yt - .02);
			auto Add = [&O](double X, double Y, const FLinearColor& C, const FVector& Out) { O.P.Add(FVector2D(X, Y)); O.C.Add(C); O.Out.Add(Out); };
			// each point carries the paint and the facing of the edge that ends at it
			Add(0, Yt, H.Hull, FVector(0, 0, -1));
			Add(Ki, Yt, H.Hull, FVector(0, 0, -1));                        // the tunnel roof
			O.Roof = 1;
			Add(Ki, Yci, H.Hull, FVector(0, -1, 0));                       // the tunnel wall
			Add(Kc, Yk, H.Bottom, FVector(0, -.6, -1));                    // the steep inboard bottom
			O.Keel = 3;
			// the outboard bottom to the chine, then the topsides: a navy pinstripe low, another under the rub rail,
			// and the rub rail at the sheer
			const double Ys = S.YS;
			const double Yp = FMath::Max(Yc, .2 + .06 * T);
			const double Hs[7] = {Yc, Yp, Yp + .035, Ys - .2, Ys - .17, Ys - .06, Ys};
			const FLinearColor Cs[7] = {H.Bottom, H.Hull, H.Cove, H.Hull, H.Cove, H.Hull, H.Rub};
			double Prev = Yc;
			for (int32 k = 0; k < 7; k++)
			{
				const double Y = FMath::Clamp(Hs[k], Prev, Ys);
				Prev = Y;
				const double F = (Y - Yc) / FMath::Max(.01, Ys - Yc);
				Add(FMath::Lerp(Bo, S.Bs, F) + FMath::Sin(F * PI) * .02, Y, Cs[k], k == 0 ? FVector(0, .5, -1) : FVector(0, 1, 0));
			}
			return O;
		};
		TArray<FSec> Secs;
		for (const FHullSta& S : Sts) Secs.Add(Section(S));
		auto V = [&](const FHullSta& S, double X, double Y) { return P(X, Y, HullZRake(H, S, Y)); };
		for (int32 i = 0; i < NS; i++)
			for (int32 Sx = -1; Sx <= 1; Sx += 2)
			{
				const FSec& A = Secs[i];
				const FSec& Z = Secs[i + 1];
				for (int32 k = 0; k < A.P.Num() - 1; k++)
				{
					FVector Out = A.Out[k + 1];
					Out.Y *= Sx;
					M.AddQuad(V(Sts[i], Sx * A.P[k].X, A.P[k].Y), V(Sts[i], Sx * A.P[k + 1].X, A.P[k + 1].Y), V(Sts[i + 1], Sx * Z.P[k + 1].X, Z.P[k + 1].Y), V(Sts[i + 1], Sx * Z.P[k].X, Z.P[k].Y),
						A.C[k + 1], Out);
				}
			}
		// the transom and the bow: the bridge deck's face across the tunnel, and each sponson's end fanned from inside it
		for (int32 End = 0; End <= 1; End++)
		{
			const FHullSta& S = Sts[End ? NS : 0];
			const FSec& A = Secs[End ? NS : 0];
			const FVector Face(End ? 1 : -1, 0, 0);
			const double Ki = A.P[1].X, Yt = A.P[0].Y, Bs = A.P.Last().X, Ys = A.P.Last().Y;
			M.AddQuad(V(S, -Ki, Yt), V(S, Ki, Yt), V(S, Bs, Ys), V(S, -Bs, Ys), H.Hull, Face);
			for (int32 Sx = -1; Sx <= 1; Sx += 2)
			{
				const FVector2D Ctr((A.P[A.Keel].X + Ki) * .5 + .01, (A.P[A.Keel].Y + Yt) * .5);
				TArray<FVector2D> Ring;
				for (int32 k = 1; k < A.P.Num(); k++) Ring.Add(A.P[k]);
				for (int32 k = 0; k < Ring.Num() - 1; k++)
					M.AddTri(V(S, Sx * Ctr.X, Ctr.Y), V(S, Sx * Ring[k].X, Ring[k].Y), V(S, Sx * Ring[k + 1].X, Ring[k + 1].Y), Ring[k].Y < A.P[A.Keel + 1].Y + .02 ? H.Bottom : H.Hull, Face);
				M.AddTri(V(S, Sx * Ctr.X, Ctr.Y), V(S, Sx * Ring.Last().X, Ring.Last().Y), V(S, Sx * Ring[0].X, Ring[0].Y), H.Hull, Face);
			}
		}
		BuildDeckIn(M, H, L, B, Sts, 0);
	}

	const FHullDef& HullOf(const KeysData::FBoatSpec& S)
	{
		struct FRow { const TCHAR* Id; double F, Spring, YK, Dr0, Dr1, Rake, Entry; double St0, St1; uint32 Bottom, Boot, Hull; int64 Cove; uint32 Rub, Cap, Liner, Deck; bool bCat; };
		// content/boats.ts HULLS
		static const FRow Rows[] = {
			{TEXT("robalo"),    1.05, .38, .45, 20, 48, .9,  .42, 0,   0,   0xf4f4f2, 0x6fa8c9, 0xffffff, -1,       0x8e949a, 0xffffff, 0xf1f1ee, 0xdcddd8, false},
			{TEXT("grady"),     1.3,  .52, .55, 20, 50, 1.3, .40, 0,   0,   0xf8f7f2, 0x23466e, 0xfbfaf6, 0x23466e, 0xd0d4d8, 0xfbfaf6, 0xf3f2ec, 0xe0ded6, false},
			{TEXT("freeman"),   1.25, .65, .5,  22, 40, .2,  .55, 0,   0,   0xc4c9ce, 0x1f2a44, 0xccd1d6, 0x1f2a44, 0x1f2a44, 0xf4f5f5, 0xf3f2ee, 0xdedcd6, true},
			{TEXT("midnight"),  1.4,  .55, .62, 22, 54, 2.1, .38, .3,  .44, 0x4a443b, 0x2a2620, 0x74695a, 0x2a2620, 0x101112, 0x85796a, 0x85796a, 0xb8875a, false},
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
				// after the owners' photos: the Freeman's blunt twin bow and high tunnel; the Midnight Express's long fine
				// bow with a flared coaming and its rounded tumblehome stern
				if (H.bCat) { H.MinBow = .45; H.Tunnel = .34; H.TunnelRise = .75; H.SponW = .27; H.StemFoot = .12; }
				if (FString(R.Id) == TEXT("midnight")) { H.Tumble = .08; H.Flare = .06; }
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
