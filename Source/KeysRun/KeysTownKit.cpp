#include "KeysTownKit.h"
#include "KeysKit.h"
#include "KeysMesh.h"
#include "KeysLife.h"

// The town style kit (see KeysTownKit.h). Every piece builds in the three.js town frame (x = u, y up, z = -v, meters)
// through FTownBatch, which maps it into the component's UE frame and winds each face from the normal it is given.

namespace KeysTownKit
{
namespace
{
	const FVector Up3(0, 1, 0);
	FVector U3(const FVector& P) { return FVector(P.X, P.Z, P.Y); }   // three -> UE (meters)

	FLinearColor Mix(const FLinearColor& A, const FLinearColor& B, double T) { FLinearColor C = A + (B - A) * float(T); C.A = 1; return C; }
	FLinearColor Mul(const FLinearColor& A, double K) { return FLinearColor(A.R * float(K), A.G * float(K), A.B * float(K), 1); }
	FLinearColor Hx(uint32 C) { return KeysKit::Hex(C); }

	// ---------------------------------------------------------------- primitives (linear colours, any orientation)
	void OBox(FTownBatch& B, const FVector& C, const FVector& AX, const FVector& AY, const FVector& AZ, double HX, double HY, double HZ, const FLinearColor& Col)
	{
		auto Pt = [&](double SX, double SY, double SZ) { return C + AX * (SX * HX) + AY * (SY * HY) + AZ * (SZ * HZ); };
		B.Quad3(Pt(1, -1, -1), Pt(1, 1, -1), Pt(1, 1, 1), Pt(1, -1, 1), AX, Col);
		B.Quad3(Pt(-1, -1, -1), Pt(-1, -1, 1), Pt(-1, 1, 1), Pt(-1, 1, -1), -AX, Col);
		B.Quad3(Pt(-1, 1, -1), Pt(-1, 1, 1), Pt(1, 1, 1), Pt(1, 1, -1), AY, Col);
		B.Quad3(Pt(-1, -1, -1), Pt(1, -1, -1), Pt(1, -1, 1), Pt(-1, -1, 1), -AY, Col);
		B.Quad3(Pt(-1, -1, 1), Pt(1, -1, 1), Pt(1, 1, 1), Pt(-1, 1, 1), AZ, Col);
		B.Quad3(Pt(-1, -1, -1), Pt(-1, 1, -1), Pt(1, 1, -1), Pt(1, -1, -1), -AZ, Col);
	}
	// a box from A to E (its long axis), Side giving its second axis
	void Bar(FTownBatch& B, const FVector& A, const FVector& E, const FVector& Side, double HW, double HT, const FLinearColor& Col)
	{
		const FVector Ax = (E - A).GetSafeNormal();
		if (Ax.IsNearlyZero()) return;
		FVector Sd = (Side - Ax * FVector::DotProduct(Side, Ax)).GetSafeNormal();
		if (Sd.IsNearlyZero()) Sd = FVector::CrossProduct(Ax, Up3).GetSafeNormal();
		const FVector Th = FVector::CrossProduct(Ax, Sd);
		OBox(B, (A + E) * .5, Ax, Sd, Th, FVector::Dist(A, E) * .5, HW, HT, Col);
	}
	void Tube(FTownBatch& B, const FVector& A, const FVector& E, double RA, double RB, int32 Sides, const FLinearColor& Col)
	{
		const FVector Dir = (E - A).GetSafeNormal();
		if (Dir.IsNearlyZero()) return;
		FVector U = FVector::CrossProduct(Dir, Up3);
		if (U.SizeSquared() < 1e-6) U = FVector::CrossProduct(Dir, FVector(1, 0, 0));
		U.Normalize();
		const FVector Wv = FVector::CrossProduct(Dir, U);
		for (int32 i = 0; i < Sides; i++)
		{
			const double A0 = 2 * PI * i / Sides, A1 = 2 * PI * (i + 1) / Sides;
			const FVector O0 = U * FMath::Cos(A0) + Wv * FMath::Sin(A0), O1 = U * FMath::Cos(A1) + Wv * FMath::Sin(A1);
			B.TriN(A + O0 * RA, E + O0 * RB, E + O1 * RB, O0, O0, O1, Col);
			B.TriN(A + O0 * RA, E + O1 * RB, A + O1 * RA, O0, O1, O1, Col);
		}
	}
	// an ellipsoid with axes AX, AY, AZ (unit) and radii, smooth-shaded
	void Ellip(FTownBatch& B, const FVector& C, const FVector& AX, const FVector& AY, const FVector& AZ, double RX, double RY, double RZ, const FLinearColor& Col, int32 Seg = 8)
	{
		const int32 NV = FMath::Max(3, Seg * 3 / 4);
		auto Pt = [&](int32 i, int32 j, FVector& Nn)
		{
			const double Th = 2 * PI * i / Seg, Ph = PI * j / NV;
			const double X = FMath::Sin(Ph) * FMath::Cos(Th), Y = FMath::Cos(Ph), Z = FMath::Sin(Ph) * FMath::Sin(Th);
			Nn = (AX * (X / RX) + AY * (Y / RY) + AZ * (Z / RZ)).GetSafeNormal();
			return C + AX * (X * RX) + AY * (Y * RY) + AZ * (Z * RZ);
		};
		for (int32 j = 0; j < NV; j++)
			for (int32 i = 0; i < Seg; i++)
			{
				FVector N00, N10, N11, N01;
				const FVector P00 = Pt(i, j, N00), P10 = Pt(i + 1, j, N10), P11 = Pt(i + 1, j + 1, N11), P01 = Pt(i, j + 1, N01);
				if (j > 0) B.TriN(P00, P10, P11, N00, N10, N11, Col);
				if (j < NV - 1) B.TriN(P00, P11, P01, N00, N11, N01, Col);
			}
	}
	// a solid of revolution about Axis from Base: Prof = (height along the axis, radius); colour per band
	void Lathe(FTownBatch& B, const FVector& Base, const FVector& Axis0, const TArray<FVector2D>& Prof, int32 Sides, TFunctionRef<FLinearColor(int32)> BandCol)
	{
		const FVector Axis = Axis0.GetSafeNormal();
		FVector U = FVector::CrossProduct(Axis, FVector(1, 0, 0));
		if (U.SizeSquared() < 1e-4) U = FVector::CrossProduct(Axis, FVector(0, 0, 1));
		U.Normalize();
		const FVector Wv = FVector::CrossProduct(Axis, U);
		const int32 NP = Prof.Num();
		TArray<FVector2D> Nrm;   // (along, out) per profile point
		for (int32 k = 0; k < NP; k++)
		{
			const FVector2D T = Prof[FMath::Min(k + 1, NP - 1)] - Prof[FMath::Max(k - 1, 0)];
			Nrm.Add(FVector2D(-T.Y, T.X).GetSafeNormal());
		}
		for (int32 k = 0; k + 1 < NP; k++)
		{
			const FLinearColor Col = BandCol(k);
			for (int32 i = 0; i < Sides; i++)
			{
				const double A0 = 2 * PI * i / Sides, A1 = 2 * PI * (i + 1) / Sides;
				const FVector O0 = U * FMath::Cos(A0) + Wv * FMath::Sin(A0), O1 = U * FMath::Cos(A1) + Wv * FMath::Sin(A1);
				auto Pt = [&](int32 kk, const FVector& O) { return Base + Axis * Prof[kk].X + O * Prof[kk].Y; };
				auto Nn = [&](int32 kk, const FVector& O) { return (Axis * Nrm[kk].X + O * Nrm[kk].Y).GetSafeNormal(); };
				if (Prof[k].Y > 1e-5 || Prof[k + 1].Y > 1e-5)
				{
					B.TriN(Pt(k, O0), Pt(k + 1, O0), Pt(k + 1, O1), Nn(k, O0), Nn(k + 1, O0), Nn(k + 1, O1), Col);
					B.TriN(Pt(k, O0), Pt(k + 1, O1), Pt(k, O1), Nn(k, O0), Nn(k + 1, O1), Nn(k, O1), Col);
				}
			}
		}
	}
	// a flat convex polygon (Poly in the plane AX, AY at O) extruded T along N; edges in EdgeC
	void Slab(FTownBatch& B, const TArray<FVector2D>& Poly, const FVector& O, const FVector& AX, const FVector& AY, const FVector& N, double T, const FLinearColor& Col, const FLinearColor& EdgeC, bool bBack = true)
	{
		const int32 NP = Poly.Num();
		if (NP < 3) return;
		FVector2D Cn(0, 0);
		for (const FVector2D& Q : Poly) Cn += Q;
		Cn /= NP;
		auto Pt = [&](int32 i, double D) { return O + AX * Poly[i].X + AY * Poly[i].Y + N * D; };
		for (int32 i = 1; i + 1 < NP; i++)
		{
			B.Tri3(Pt(0, T), Pt(i, T), Pt(i + 1, T), N, Col);
			if (bBack) B.Tri3(Pt(0, 0), Pt(i + 1, 0), Pt(i, 0), -N, Col);
		}
		for (int32 i = 0; i < NP; i++)
		{
			const int32 j = (i + 1) % NP;
			const FVector2D E = Poly[j] - Poly[i];
			FVector2D On(E.Y, -E.X);
			if (FVector2D::DotProduct(On, (Poly[i] + Poly[j]) * .5 - Cn) < 0) On = -On;
			const FVector Out = (AX * On.X + AY * On.Y).GetSafeNormal();
			B.Quad3(Pt(i, 0), Pt(j, 0), Pt(j, T), Pt(i, T), Out, EdgeC);
		}
	}
	TArray<FVector2D> Ellipse(double CX, double CY, double A, double Bb, int32 N, double Egg = 0)
	{
		TArray<FVector2D> R;
		for (int32 i = 0; i < N; i++)
		{
			const double T = 2 * PI * i / N;
			R.Add(FVector2D(CX + A * FMath::Cos(T), CY + Bb * FMath::Sin(T) * (1 + Egg * FMath::Cos(T))));
		}
		return R;
	}
	// a star-shaped flat patch (peeling paint, splash) as a fan from its middle
	void Patch(FTownBatch& B, const FWall& W, double A, double Y, double D, double R, const FLinearColor& Col, FKitDice& Dc, int32 N = 7)
	{
		const FVector C = W.At(A, Y, D);
		TArray<FVector> Ring;
		const double Rot = Dc.R(0, 6.28);
		for (int32 i = 0; i < N; i++)
		{
			const double T = Rot + 2 * PI * i / N, Rr = R * Dc.R(.45, 1.15);
			Ring.Add(C + W.R * (FMath::Cos(T) * Rr * Dc.R(.8, 1.4)) + Up3 * (FMath::Sin(T) * Rr));
		}
		for (int32 i = 0; i < N; i++) B.Tri3(C, Ring[i], Ring[(i + 1) % N], W.N, Col);
	}

	// ---------------------------------------------------------------- fish outlines (for painted and cut-out fish)
	// Pieces of a side-on fish, nose to +x, length 1 (x -.5 tail .. .5 nose); each piece convex. Kind: 0 tarpon,
	// 1 sailfish, 2 mahi, 3 grouper, 4 barracuda, 5 permit
	struct FOutline { TArray<TArray<FVector2D>> Body, Fins; FVector2D Eye; double EyeR; FVector2D TextC; double TextW; };
	FOutline FishShape(int32 Kind)
	{
		FOutline O;
		double CX = .07, A = .43, Bh = .15, Fork = .19, Egg = 0;
		if (Kind == 1) { CX = .05; A = .38; Bh = .095; Fork = .2; }
		if (Kind == 2) { CX = .06; A = .44; Bh = .14; Egg = .22; Fork = .17; }
		if (Kind == 3) { CX = .05; A = .45; Bh = .22; Fork = .16; }
		if (Kind == 4) { CX = .05; A = .45; Bh = .075; Fork = .12; }
		if (Kind == 5) { CX = .08; A = .4; Bh = .27; Fork = .26; }
		O.Body.Add(Ellipse(CX, 0, A, Bh, 20, Egg));
		const double TB = CX - A + .1;   // where the tail joins
		if (Kind == 3)
			O.Fins.Add({FVector2D(TB, 0), FVector2D(-.49, .17), FVector2D(-.53, .06), FVector2D(-.53, -.06), FVector2D(-.49, -.17)});
		else
		{
			O.Fins.Add({FVector2D(TB, .02), FVector2D(-.5, Fork), FVector2D(-.43, 0)});
			O.Fins.Add({FVector2D(TB, -.02), FVector2D(-.43, 0), FVector2D(-.5, -Fork)});
		}
		if (Kind == 1)
		{
			O.Fins.Add({FVector2D(-.2, .05), FVector2D(.28, .06), FVector2D(.2, .3), FVector2D(.0, .37), FVector2D(-.16, .27)});
			O.Fins.Add({FVector2D(CX + A - .02, .02), FVector2D(.66, 0), FVector2D(CX + A - .02, -.02)});   // the bill
		}
		else if (Kind == 2) O.Fins.Add({FVector2D(-.3, .09), FVector2D(.42, .14), FVector2D(.36, .21), FVector2D(-.24, .14)});
		else if (Kind == 5) { O.Fins.Add({FVector2D(-.05, .24), FVector2D(.12, .24), FVector2D(-.12, .44)}); O.Fins.Add({FVector2D(-.05, -.24), FVector2D(.1, -.24), FVector2D(-.14, -.42)}); }
		else if (Kind == 3) O.Fins.Add({FVector2D(-.28, .17), FVector2D(.25, .2), FVector2D(.18, .27), FVector2D(-.22, .24)});
		else O.Fins.Add({FVector2D(-.04, Bh * .9), FVector2D(.14, Bh * .95), FVector2D(.0, Bh + .14)});
		// a pelvic fin under the belly
		if (Kind != 1) O.Fins.Add({FVector2D(.08, -Bh * .85), FVector2D(.2, -Bh * .9), FVector2D(.06, -Bh - .07)});
		O.Eye = FVector2D(CX + A * .74, Bh * .3);
		O.EyeR = FMath::Max(.018, Bh * .14);
		O.TextC = FVector2D(CX - A * .12, -Bh * .05);
		O.TextW = A * 1.35;
		return O;
	}
	const TCHAR* const TrophySpecies[] = {TEXT("sailfish"), TEXT("tarpon"), TEXT("mahi"), TEXT("permit"), TEXT("snook"), TEXT("barracuda"), TEXT("goliath")};
	// how long a good one is, and how deep its plaque wants to be (fraction of the length)
	void TrophySize(const FString& Sp, FKitDice& D, double& Len, double& Deep)
	{
		Deep = .3;
		if (Sp == TEXT("sailfish")) { Len = D.R(1.9, 2.4); Deep = .36; }
		else if (Sp == TEXT("tarpon")) { Len = D.R(1.4, 1.9); Deep = .3; }
		else if (Sp == TEXT("mahi")) { Len = D.R(1.0, 1.35); Deep = .33; }
		else if (Sp == TEXT("permit")) { Len = D.R(.6, .8); Deep = .55; }
		else if (Sp == TEXT("snook")) { Len = D.R(.7, .95); Deep = .27; }
		else if (Sp == TEXT("barracuda")) { Len = D.R(1.0, 1.3); Deep = .22; }
		else { Len = D.R(1.5, 2.1); Deep = .45; }   // goliath
	}

	// a species' body normalised to 1 m nose to tail (cm, nose +X, back +Z), cached: the schools' lighter body for
	// trophies and paintings, the close-up one (the fish you catch) for the statue
	const FKeysMesh& FishBody(const FString& Key, bool bFull)
	{
		static TMap<FString, FKeysMesh> Cache;
		const FString CKey = Key + (bFull ? TEXT("#full") : TEXT("#lod"));
		if (const FKeysMesh* F = Cache.Find(CKey)) return *F;
		FKeysMesh Raw;
		if (bFull || !AKeysLife::BuildSchoolMesh(Key, Raw, 1) || !Raw.T.Num())
		{
			Raw = FKeysMesh();
			KeysKit::AppendFish(Raw, Key, 1.0, FTransform::Identity);
		}
		double X0 = 1e18, X1 = -1e18;
		for (const FVector& V : Raw.V) { X0 = FMath::Min(X0, V.X); X1 = FMath::Max(X1, V.X); }
		const double K = X1 > X0 ? 100.0 / (X1 - X0) : 1, Mid = (X0 + X1) / 2;
		for (FVector& V : Raw.V) V = FVector(V.X - Mid, V.Y, V.Z) * K;
		return Cache.Add(CKey, MoveTemp(Raw));
	}

	// A real fish model (the same one you catch) laid along Nose with its back to Up and its flank toward Side,
	// bent: Arch lifts the middle (a jump), Curl brings head and tail out toward Side. Thin < 1 flattens it (a painted
	// one); PaintK pushes its colours toward poster paint; Surf >= 0 replaces its surface kind.
	void PosedFish(FTownBatch& B, const FString& Key, double Len, const FVector& C3, const FVector& Nose, const FVector& UpV, const FVector& Side,
		double Arch, double Curl, double Thin, double FadeK, double PaintK, int32 Surf, FKitDice& D, bool bFull = false)
	{
		const FKeysMesh& F = FishBody(Key, bFull);
		if (!F.V.Num()) return;
		const double HL = Len * 50;   // half length, cm
		const FVector Nz = Nose.GetSafeNormal(), Uz = UpV.GetSafeNormal(), Sz = Side.GetSafeNormal();
		// fish local (x nose, y flank, z back) -> three -> UE; a mirror flips the winding
		const FVector Ex = U3(Nz), Ey = U3(Sz), Ez = U3(Uz);
		const bool bFlip = FVector::DotProduct(FVector::CrossProduct(Ex, Ey), Ez) < 0;
		const FVector Cue = U3(C3) * 100;
		B.M.SyncKinds();
		const int32 Base = B.M.V.Num();
		for (int32 i = 0; i < F.V.Num(); i++)
		{
			const FVector V = F.V[i] * Len;
			const double S = FMath::Clamp(V.X / HL, -1.0, 1.0);
			const double Lx = V.X, Ly = V.Y * Thin + Curl * S * S * HL, Lz = V.Z + Arch * (1 - S * S) * HL * .5;
			B.M.V.Add(Cue + Ex * Lx + Ey * Ly + Ez * Lz);
			const FVector Nf = F.N.IsValidIndex(i) ? F.N[i] : FVector::UpVector;
			FVector Nw = Ex * Nf.X + Ey * (Nf.Y / FMath::Max(Thin, .05)) + Ez * Nf.Z;
			if (Thin < .2) Nw = Ey * (Nf.Y >= 0 ? 1 : -1) + Nw * .15;
			B.M.N.Add(Nw.GetSafeNormal());
			B.M.UV.Add(FVector2D::ZeroVector);
			FLinearColor Cc = F.C.IsValidIndex(i) ? F.C[i] : FLinearColor::White;
			if (PaintK > 0)
			{
				// poster paint: more saturated, flatter
				const FLinearColor Hsv = Cc.LinearRGBToHSV();
				FLinearColor H2 = Hsv;
				H2.G = FMath::Min(1.f, Hsv.G * float(1 + PaintK * 1.2));
				H2.B = FMath::Lerp(Hsv.B, FMath::Max(Hsv.B, .55f), float(PaintK * .5));
				Cc = H2.HSVToLinearRGB();
			}
			if (FadeK > 0) Cc = Weathered(Cc, FadeK, D);
			Cc.A = 1;
			B.M.C.Add(Cc);
			B.M.K.Add(Surf >= 0 ? FVector2D(Surf, 0) : (F.K.IsValidIndex(i) ? F.K[i] : FVector2D::ZeroVector));
		}
		for (int32 t = 0; t + 2 < F.T.Num(); t += 3)
		{
			B.M.T.Add(Base + F.T[t]);
			B.M.T.Add(Base + F.T[t + (bFlip ? 2 : 1)]);
			B.M.T.Add(Base + F.T[t + (bFlip ? 1 : 2)]);
		}
	}

	// a cut-out fish (outline pieces) standing in the plane AX (nose), AY (up) at C, Len long, T thick, both faces
	void CutoutFish(FTownBatch& B, int32 Kind, const FVector& C, const FVector& AX, const FVector& AY, double Len, double T, const FLinearColor& Body, const FLinearColor& Fin, const FLinearColor& Edge, bool bEye)
	{
		const FOutline O = FishShape(Kind);
		const FVector N = FVector::CrossProduct(AX, AY).GetSafeNormal();
		auto Scale = [&](const TArray<FVector2D>& Pp) { TArray<FVector2D> R; for (const FVector2D& Q : Pp) R.Add(Q * Len); return R; };
		const FVector O0 = C - N * (T * .5);
		for (const TArray<FVector2D>& Pc : O.Fins) Slab(B, Scale(Pc), O0 + N * (T * .1), AX, AY, N, T * .8, Fin, Edge);
		for (const TArray<FVector2D>& Pc : O.Body) Slab(B, Scale(Pc), O0, AX, AY, N, T, Body, Edge);
		if (bEye)
			for (int32 s = -1; s <= 1; s += 2)
			{
				const FVector Ec = C + AX * (O.Eye.X * Len) + AY * (O.Eye.Y * Len) + N * (s * (T * .5 + .002));
				TArray<FVector2D> Ring = Ellipse(0, 0, O.EyeR * Len, O.EyeR * Len, 10), Pup = Ellipse(0, 0, O.EyeR * Len * .55, O.EyeR * Len * .55, 8);
				Slab(B, Ring, Ec, AX, AY, N * double(s), .002, FLinearColor(.95f, .94f, .9f), FLinearColor(.95f, .94f, .9f), false);
				Slab(B, Pup, Ec + N * (s * .002), AX, AY, N * double(s), .002, FLinearColor(.03f, .03f, .035f), FLinearColor(.03f, .03f, .035f), false);
			}
	}
}

// ==================================================================== seeds and paint
uint32 SeedOf(const TCHAR* Text)
{
	uint32 H = 2166136261u;
	for (const TCHAR* C = Text; C && *C; ++C) { H ^= uint32(*C); H *= 16777619u; }
	return H;
}
uint32 SeedOf(double A, double B) { return uint32(int64(FMath::FloorToDouble(A * 7.31)) * 73856093) ^ uint32(int64(FMath::FloorToDouble(B * 5.17)) * 19349663); }

FLinearColor Paint(EPaint Pt)
{
	switch (Pt)
	{
		case EPaint::Coral: return Hx(0xef8a72);
		case EPaint::Seafoam: return Hx(0x9fd9c0);
		case EPaint::FadedYellow: return Hx(0xf2d98a);
		case EPaint::SkyBlue: return Hx(0x8cc3e4);
		case EPaint::WhiteWash: return Hx(0xf0ebdf);
		case EPaint::Teal: return Hx(0x3c9f96);
		case EPaint::Navy: return Hx(0x2f4b6c);
		default: return FLinearColor::White;
	}
}
FLinearColor RandomPastel(FKitDice& D)
{
	static const EPaint Main[] = {EPaint::Coral, EPaint::Seafoam, EPaint::FadedYellow, EPaint::SkyBlue, EPaint::Coral, EPaint::Seafoam, EPaint::SkyBlue, EPaint::WhiteWash};
	FLinearColor C = Paint(Main[D.I(8)]);
	// no two the same: a nudge of hue and value
	FLinearColor Hsv = C.LinearRGBToHSV();
	Hsv.R = FMath::Fmod(Hsv.R + float(D.R(-9, 9)) + 360.f, 360.f);
	Hsv.B *= float(D.R(.92, 1.05));
	return Hsv.HSVToLinearRGB();
}
FLinearColor Weathered(const FLinearColor& C, double K, FKitDice& D)
{
	// toward a chalky, salt-white version of itself, a touch grey, unevenly
	const float L = C.GetLuminance();
	const double Kk = FMath::Clamp(K * D.R(.75, 1.2), 0.0, 1.0);
	FLinearColor Chalk(L * .55f + .32f, L * .55f + .31f, L * .55f + .28f, 1);
	FLinearColor Out = Mix(C, Chalk, Kk * .7);
	return Mul(Out, 1 - Kk * .08);
}
FLinearColor SaltWood(FKitDice& D) { return Mix(Hx(0x8f877a), Hx(0xb2aa9b), D.F()); }
FLinearColor WarmWood(FKitDice& D) { return Mix(Hx(0x6e4a2c), Hx(0x9a6b40), D.F()); }
FLinearColor Rust(FKitDice& D) { return Mix(Hx(0x7a3a1c), Hx(0xa05a2a), D.F()); }

const TArray<FString>& Puns()
{
	static const TArray<FString> A = {
		TEXT("OH MY COD"), TEXT("BAIT EXPECTATIONS"), TEXT("BARRACUDA BARGAINS"), TEXT("GILL-TY PLEASURES"), TEXT("REEL GOOD TIMES"),
		TEXT("NAUTI BUOYS"), TEXT("SHELL YEAH"), TEXT("PERMIT PENDING"), TEXT("WASH & WAHOO"), TEXT("SEAS THE DAY"),
		TEXT("SNOOZE & SNOOK"), TEXT("FIN-TASTIC SCOOPS"), TEXT("MULLET OVER"), TEXT("SILVER KING'S REST"), TEXT("HOOKED ON YOU"),
		TEXT("FISHFUL THINKING"), TEXT("GROUPER THERAPY"), TEXT("JUST FOR THE HALIBUT"), TEXT("SNAPPER HAPPY"), TEXT("KNOT TOO SHABBY"),
		TEXT("SOLE FOOD"), TEXT("TARPON TIME"), TEXT("THE SALTY SNOOK"), TEXT("O' FISH-AL"),
	};
	return A;
}

// ==================================================================== the prefabs
void MountedFish(FTownBatch& B, const FWall& W, double A, double Y, const FString& Species, double Len, int32 Pose, FKitDice& D)
{
	double L0, Deep;
	TrophySize(Species, D, L0, Deep);
	const double Ln = Len > 0 ? Len : L0;
	// the plaque: a varnished oval (or a shield for the short deep ones) with a darker routed rim and a brass plate
	const double PW = Ln * (Pose == 3 ? .92 : 1.04), PH = FMath::Max(.32, Ln * Deep * (Pose == 1 || Pose == 3 ? 1.35 : 1.1));
	const FLinearColor Wood = WarmWood(D), Rim = Mul(Wood, .62);
	const FVector Pc = W.At(A, Y, 0);
	Slab(B, Ellipse(0, 0, PW * .5 + .03, PH * .5 + .03, 24), Pc, W.R, Up3, W.N, .025, Rim, Rim, false);
	Slab(B, Ellipse(0, 0, PW * .5, PH * .5, 24), Pc + W.N * .025, W.R, Up3, W.N, .022, Wood, Mul(Wood, .8), false);
	OBox(B, W.At(A, Y - PH * .5 + .07, .055), W.R, Up3, W.N, .09, .028, .006, Hx(0xb8933e));
	// the fish: its flank to the room, head left or right, posed
	const double Dir = D.Chance(.5) ? 1 : -1;
	double Tilt = 0, Arch = .04, Curl = 0;
	if (Pose == 1) { Arch = .22; Tilt = D.R(-.08, .12); }
	else if (Pose == 2) { Arch = .06; Curl = .28; }
	else if (Pose == 3) { Arch = .16; Tilt = D.R(.32, .5); }
	const FVector Nose = W.R * (Dir * FMath::Cos(Tilt)) + Up3 * FMath::Sin(Tilt);
	const FVector UpV = Up3 * FMath::Cos(Tilt) - W.R * (Dir * FMath::Sin(Tilt));
	const double Off = .047 + Ln * (Species == TEXT("goliath") || Species == TEXT("permit") ? .1 : .07);
	// old trophies have gone dusty and a little faded
	const double Fade = D.Chance(.4) ? D.R(.1, .3) : 0;
	PosedFish(B, Species, Ln, W.At(A, Y, Off), Nose, UpV, W.N, Arch, Curl, 1, Fade, .15, -1, D);
}

void FishSign(FTownBatch& B, const FWall& W, double A, double Y, double Len, const FString& Text, FKitDice& D, TArray<FKitText>* Texts, bool bBlade)
{
	const int32 Kind = D.I(6);
	const FLinearColor Body = Weathered(RandomPastel(D), D.R(.15, .45), D);
	FLinearColor Fin = Weathered(Paint(D.Chance(.5) ? EPaint::Teal : EPaint::Coral), D.R(.2, .5), D);
	if (FMath::Abs(Fin.GetLuminance() - Body.GetLuminance()) < .05) Fin = Weathered(Paint(EPaint::Navy), .3, D);
	const FLinearColor Edge = Mul(SaltWood(D), .8);
	const FOutline O = FishShape(Kind);
	const double T = .045;
	if (bBlade)
	{
		// a wrought bracket out from the wall, a brace under it, and two short chains; the sign square to the wall
		const FLinearColor Iron = Hx(0x26282b);
		const double Top = Y + Len * .26 + .16, Reach = Len + .45;
		Bar(B, W.At(A, Top, 0), W.At(A, Top, Reach), Up3, .018, .018, Iron);
		Bar(B, W.At(A, Top - .5, 0), W.At(A, Top - .01, Reach * .62), W.R, .012, .012, Iron);
		OBox(B, W.At(A, Top, .02), W.N, Up3, W.R, .02, .1, .07, Iron);
		OBox(B, W.At(A, Top - .5, .02), W.N, Up3, W.R, .02, .06, .05, Iron);
		const FVector Cc = W.At(A, Y, .3 + Len * .5);
		for (double Sx : {-.3, .3})
		{
			const FVector At0 = W.At(A, Top, .3 + Len * (.5 + Sx));
			Tube(B, At0, At0 - Up3 * (Top - (Y + Len * .2)), .007, .007, 4, Iron);
		}
		const bool bFlip = D.Chance(.5);
		const FVector AX = W.N * (bFlip ? -1.0 : 1.0);
		CutoutFish(B, Kind, Cc, AX, Up3, Len, T, Body, Fin, Edge, true);
		// lettering on both faces (the face normal is W.R one way and -W.R the other)
		if (Texts && !Text.IsEmpty())
			for (int32 s = -1; s <= 1; s += 2)
			{
				const FVector Fc = W.R * double(s);
				Texts->Add({Text, Cc + AX * (O.TextC.X * Len) + Up3 * (O.TextC.Y * Len) + Fc * (T * .5 + .012), Fc, Len * .13, O.TextW * Len * 1.02, Hx(0x1d2a3a)});
			}
		return;
	}
	const FVector Cc = W.At(A, Y, T * .5 + .01);
	const bool bLeft = D.Chance(.5);
	CutoutFish(B, Kind, Cc, W.R * (bLeft ? -1.0 : 1.0), Up3, Len, T, Body, Fin, Edge, true);
	if (Texts && !Text.IsEmpty())
		Texts->Add({Text, Cc + W.R * (O.TextC.X * Len * (bLeft ? -1 : 1)) + Up3 * (O.TextC.Y * Len) + W.N * (T * .5 + .012), W.N, FMath::Min(.22, Len * .1), O.TextW * Len * .95, Hx(0x1d2a3a)});
}

void PlankSign(FTownBatch& B, const FWall& W, double A, double Y, double Wd, const FString& Text, FKitDice& D, TArray<FKitText>* Texts)
{
	const int32 NPl = D.Chance(.5) ? 3 : 2;
	const double PlH = .17;
	const FLinearColor Base = Weathered(RandomPastel(D), D.R(.25, .55), D);
	const double H = NPl * (PlH + .015);
	for (int32 i = 0; i < NPl; i++)
	{
		const double Py = Y - H * .5 + i * (PlH + .015) + PlH * .5;
		const double Rot = D.R(-.02, .02);
		const FVector Ax = W.R * FMath::Cos(Rot) + Up3 * FMath::Sin(Rot);
		const double Dx = D.R(-.04, .04);
		OBox(B, W.At(A + Dx, Py, .025), Ax, FVector::CrossProduct(W.N, Ax), W.N, Wd * .5 + D.R(-.03, .05), PlH * .5, .018, Mul(Base, D.R(.9, 1.04)));
		// salt-grey wood showing through at the ends
		OBox(B, W.At(A + Dx + Wd * .5 - .05, Py, .044), Ax, FVector::CrossProduct(W.N, Ax), W.N, .04, PlH * .4, .002, SaltWood(D));
	}
	// a painted fish at the left, the lettering to its right
	const double FL = FMath::Min(.5, Wd * .3);
	const FLinearColor Ink = Weathered(Paint(D.Chance(.5) ? EPaint::Navy : EPaint::Teal), .2, D);
	CutoutFish(B, D.I(6), W.At(A - Wd * .5 + FL * .6, Y, .046), W.R, Up3, FL, .004, Weathered(Paint(EPaint::Coral), .3, D), Ink, Ink, false);
	if (Texts && !Text.IsEmpty()) Texts->Add({Text, W.At(A + FL * .55, Y, .065), W.N, .24, Wd - FL * 1.3, Hx(0x15263a)});
}

void Buoy(FTownBatch& B, const FWall& W, double A, double Y, double Size, FKitDice& D)
{
	// the nail, a bit of rope, the buoy hanging below it leaning a little on the wall
	const FVector Nail = W.At(A, Y, .03);
	Tube(B, W.At(A, Y, 0), Nail + W.N * .02, .006, .006, 4, Hx(0x3a3a3a));
	const double Lean = D.R(-.12, .12);
	const FVector Ax = (-Up3 * FMath::Cos(Lean) + W.R * FMath::Sin(Lean)).GetSafeNormal();
	const FLinearColor RopeC = Mix(Hx(0xcbb88f), Hx(0x8f7f60), D.F());
	const FVector Top = Nail + Ax * .12 + W.N * (Size * .3 - .02);
	Tube(B, Nail, Top, .008, .008, 4, RopeC);
	// bands of the owner's colours (no two boats paint theirs alike)
	TArray<FLinearColor> Cols;
	const int32 NB = D.Chance(.6) ? 3 : 2;
	for (int32 i = 0; i < NB; i++) Cols.Add(Weathered(D.Chance(.25) ? Paint(D.Chance(.5) ? EPaint::Navy : EPaint::WhiteWash) : (D.Chance(.4) ? Hx(D.Chance(.5) ? 0xe2552e : 0xf0c430) : RandomPastel(D)), D.R(.1, .5), D));
	const double Cut1 = D.R(.3, .45), Cut2 = D.R(.6, .75);
	const TArray<FVector2D> Prof = {{0, Size * .03}, {Size * .06, Size * .15}, {Size * .2, Size * .26}, {Size * .45, Size * .3}, {Size * .7, Size * .28}, {Size * .88, Size * .2}, {Size, Size * .05}};
	Lathe(B, Top, Ax, Prof, 10, [&](int32 k)
	{
		const double T = (Prof[k].X + Prof[k + 1].X) * .5 / Size;
		return T < Cut1 ? Cols[0] : (T < Cut2 || NB < 3) ? Cols[1 % NB] : Cols[2 % NB];
	});
	// the spindle through it
	Tube(B, Top + Ax * Size, Top + Ax * (Size + .09), .01, .008, 4, Hx(0x5a5046));
}

void NetFloats(FTownBatch& B, const FWall& W, double A, double Y, FKitDice& D)
{
	const FVector Nail = W.At(A, Y, .03);
	Tube(B, W.At(A, Y, 0), Nail, .006, .006, 4, Hx(0x3a3a3a));
	const FLinearColor Cord = Mix(Hx(0xd8cfb4), Hx(0x9a8c6a), D.F());
	const bool bGlass = D.Chance(.55);
	const int32 N = 3 + D.I(4);
	for (int32 i = 0; i < N; i++)
	{
		const double R = bGlass ? D.R(.07, .1) : D.R(.06, .085);
		const double Ang = (i - (N - 1) * .5) * .5 + D.R(-.15, .15);
		const double Drop = .25 + D.R(0, .35) + (i % 2) * .12;
		const FVector C = Nail + W.R * (FMath::Sin(Ang) * Drop * .8) - Up3 * Drop + W.N * (R + .005);
		FLinearColor Col;
		if (bGlass) Col = Hx(i % 3 == 0 ? 0x4f8f73 : i % 3 == 1 ? 0x6fa8a0 : 0xa37a35);
		else Col = Weathered(Hx(i % 3 == 0 ? 0xe2662e : i % 3 == 1 ? 0xf2f0ea : 0xa9825a), D.R(.1, .4), D);
		const uint8 Keep = B.M.Surf;
		B.M.Surf = bGlass ? SurfGlass : SurfRubber;
		Ellip(B, C, W.R, Up3, W.N, R, R, R, Col, 8);
		B.M.Surf = Keep;
		// the net: a cord to the nail and a cross of cords round the float
		Tube(B, Nail, C + Up3 * R, .004, .004, 3, Cord);
		Tube(B, C + W.R * R + W.N * (R * .3), C - W.R * R + W.N * (R * .3), .0035, .0035, 3, Cord);
		Tube(B, C + Up3 * R + W.N * (R * .3), C - Up3 * R + W.N * (R * .3), .0035, .0035, 3, Cord);
	}
}

void LobsterTraps(FTownBatch& B, const FVector& Base, double Yaw, int32 Count, FKitDice& D, bool bSimple)
{
	const uint8 Keep = B.M.Surf;
	B.M.Surf = SurfWood;
	for (int32 n = 0; n < Count; n++)
	{
		const double Y0 = n * .4, Yw = Yaw + D.R(-.18, .18);
		const FVector AX(FMath::Cos(Yw), 0, -FMath::Sin(Yw)), AZ(FMath::Sin(Yw), 0, FMath::Cos(Yw));
		const FVector C = Base + Up3 * (Y0 + .19) + AX * D.R(-.06, .06) + AZ * D.R(-.05, .05);
		const double HL = .5, HW = .27, HH = .19;
		auto Wood = [&]() { return Mul(SaltWood(D), D.R(.85, 1.08)); };
		// the frame: four long rails and the end posts
		for (int32 sy = -1; sy <= 1; sy += 2)
			for (int32 sz = -1; sz <= 1; sz += 2)
				OBox(B, C + Up3 * (sy * (HH - .015)) + AZ * (sz * (HW - .015)), AX, Up3, AZ, HL, .015, .015, Wood());
		for (int32 sx = -1; sx <= 1; sx += 2)
			for (int32 sz = -1; sz <= 1; sz += 2)
				OBox(B, C + AX * (sx * (HL - .015)) + AZ * (sz * (HW - .015)), AX, Up3, AZ, .015, HH, .015, Wood());
		// slats across the top and down the long sides
		const int32 NS = bSimple ? 3 : 5;
		for (int32 i = 0; i < NS; i++)
		{
			const double X = -HL + .1 + i * (2 * HL - .2) / (NS - 1);
			OBox(B, C + AX * X + Up3 * HH, AX, Up3, AZ, .025, .008, HW, Wood());
			if (!bSimple) for (int32 sz = -1; sz <= 1; sz += 2) OBox(B, C + AX * X + AZ * (sz * HW), AX, Up3, AZ, .025, HH - .02, .008, Wood());
		}
		// the net heads at the ends (dark green mesh) and a bit of the funnel showing
		const FLinearColor Net = Mix(Hx(0x2c3a30), Hx(0x4a5a46), D.F());
		for (int32 sx = -1; sx <= 1; sx += 2) B.Quad3(C + AX * (sx * (HL - .02)) + Up3 * (-HH) + AZ * (-HW), C + AX * (sx * (HL - .02)) + Up3 * HH + AZ * (-HW),
			C + AX * (sx * (HL - .02)) + Up3 * HH + AZ * HW, C + AX * (sx * (HL - .02)) + Up3 * (-HH) + AZ * HW, AX * double(sx), Net);
		// some of them still with a buoy tied on top
		if (!bSimple && n == Count - 1 && D.Chance(.4))
		{
			const FLinearColor Bc = Weathered(Hx(D.Chance(.5) ? 0xe2552e : 0xf0c430), .3, D);
			Ellip(B, C + Up3 * (HH + .08) + AX * .1, AX, Up3, AZ, .2, .07, .07, Bc, 8);
		}
	}
	B.M.Surf = Keep;
}

void RopeCoil(FTownBatch& B, const FVector& C, const FVector& Axis, double R, FKitDice& D)
{
	const FLinearColor Rope = Mix(Hx(0xcdbb90), Hx(0x9a8a66), D.F());
	FVector U = FVector::CrossProduct(Axis, FVector(1, 0, 0));
	if (U.SizeSquared() < 1e-4) U = FVector::CrossProduct(Axis, FVector(0, 0, 1));
	U.Normalize();
	const FVector Wv = FVector::CrossProduct(Axis.GetSafeNormal(), U);
	const int32 Turns = 3, Seg = 14;
	const double Rr = .018;
	FVector Prev = FVector::ZeroVector;
	for (int32 k = 0; k <= Turns * Seg; k++)
	{
		const double T = double(k) / Seg, A = T * 2 * PI;
		const double Rad = R * (1 - .1 * T / Turns) + D.R(-.006, .006);
		const FVector Pt = C + U * (FMath::Cos(A) * Rad) + Wv * (FMath::Sin(A) * Rad) + Axis * (Rr * 1.7 * T);
		if (k) Tube(B, Prev, Pt, Rr, Rr, 4, Mul(Rope, (k / 3) % 2 ? 1.0 : .9));
		Prev = Pt;
	}
	// the loose end trailing off
	Tube(B, Prev, Prev + U * (R * .9) + Wv * (R * .4), Rr, Rr, 4, Rope);
}

void RopeOnWall(FTownBatch& B, const FWall& W, double A, double Y, FKitDice& D)
{
	const FVector Peg = W.At(A, Y, 0);
	Tube(B, Peg, Peg + W.N * .07, .012, .012, 5, WarmWood(D));
	const FLinearColor Rope = Mix(Hx(0xcdbb90), Hx(0x9a8a66), D.F());
	// loops hanging off the peg: tall ellipses in the wall's plane, fanned a little
	const int32 Loops = 4 + D.I(3);
	for (int32 l = 0; l < Loops; l++)
	{
		const double H = D.R(.45, .6), Wd = D.R(.16, .22), Sk = (l - Loops * .5) * .02;
		FVector Prev = FVector::ZeroVector;
		for (int32 k = 0; k <= 12; k++)
		{
			const double T = 2 * PI * k / 12;
			const FVector Pt = W.At(A + Sk + FMath::Sin(T) * Wd, Y - H * .5 + FMath::Cos(T) * H * .5, .04 + l * .012);
			if (k) Tube(B, Prev, Pt, .014, .014, 4, Mul(Rope, l % 2 ? 1.0 : .92));
			Prev = Pt;
		}
	}
}

void CastNet(FTownBatch& B, const FWall& W, double A, double Y, FKitDice& D)
{
	const FLinearColor Net = Mix(Hx(0xe2ddcc), Hx(0xb9b39c), D.F()), Dark = Mul(Net, .78), Lead = Hx(0x4a4d50);
	const FVector Apex = W.At(A, Y, .06);
	Tube(B, W.At(A, Y, 0), Apex + W.N * .02, .01, .01, 4, WarmWood(D));
	const double H = D.R(1.0, 1.25), Rw = D.R(.32, .42);
	const int32 N = 14;
	TArray<FVector> Hem;
	for (int32 i = 0; i <= N; i++)
	{
		const double T = PI * i / N;   // half way round, the back pressed flat to the wall
		Hem.Add(W.At(A + FMath::Cos(T) * Rw, Y - H + .05 * FMath::Sin(T * 5 + D.R(0, 3)), .03 + FMath::Sin(T) * Rw * .45));
	}
	for (int32 i = 0; i < N; i++)
	{
		const FVector Nf = FVector::CrossProduct(Hem[i] - Apex, Hem[i + 1] - Apex).GetSafeNormal();
		const FVector Out = FVector::DotProduct(Nf, W.N) > 0 ? Nf : -Nf;
		B.Tri3(Apex, Hem[i], Hem[i + 1], Out, i % 2 ? Net : Dark);
		B.Tri3(Apex, Hem[i + 1], Hem[i], -Out, Dark);
	}
	for (int32 i = 0; i < N; i++) Tube(B, Hem[i], Hem[i + 1], .012, .012, 4, Lead);
	// the hand line looped over the peg
	Tube(B, Apex, Apex + W.R * .12 - Up3 * .35 + W.N * .04, .006, .006, 3, Net);
}

void OldRods(FTownBatch& B, const FWall& W, double A, double Y, FKitDice& D)
{
	const int32 N = D.Chance(.5) ? 2 : 3;
	for (int32 i = 0; i < N; i++)
	{
		const double Ang = (i - (N - 1) * .5) * .5 + D.R(-.06, .06);
		const double Len = D.R(1.7, 2.1);
		const FVector Dir = (W.R * FMath::Cos(Ang) + Up3 * FMath::Sin(Ang)).GetSafeNormal();
		const FVector Mid = W.At(A, Y, .05 + i * .025);
		const FVector Butt = Mid - Dir * (Len * .5), Tip = Mid + Dir * (Len * .5);
		static const uint32 Blanks[] = {0xc9a24a, 0x2a2a2a, 0x7a5a36, 0x3a5a7a};
		const FLinearColor Blank = Weathered(Hx(Blanks[D.I(4)]), D.R(.1, .35), D);
		Tube(B, Butt, Tip, .011, .0035, 5, Blank);
		Tube(B, Butt - Dir * .02, Butt + Dir * .32, .019, .017, 6, Mix(Hx(0xb48c5c), Hx(0x8a6a44), D.F()));   // cork
		// the reel: an old knuckle-buster drum under the grip
		const FVector Rc = Butt + Dir * .4 + W.N * .045;
		Tube(B, Rc - W.N * .025, Rc + W.N * .025, .045, .045, 10, Weathered(Hx(D.Chance(.5) ? 0x8f969c : 0x3a3d40), .2, D));
		Tube(B, Rc + W.N * .025, Rc + W.N * .035, .02, .02, 6, Hx(0x2a2c2e));
		// a guide or two
		for (double T : {.55, .75}) Tube(B, Butt + Dir * (Len * T), Butt + Dir * (Len * T) + W.N * .02, .004, .004, 3, Hx(0xc0c4c8));
	}
	// the pegs
	for (double Sx : {-.45, .45}) Tube(B, W.At(A + Sx, Y + Sx * .2, 0), W.At(A + Sx, Y + Sx * .2, .11), .012, .012, 5, WarmWood(D));
}

void Weathervane(FTownBatch& B, const FVector& Base, double Size, FKitDice& D)
{
	const FLinearColor Iron = Hx(0x2a2c2e);
	const FLinearColor Metal = D.Chance(.45) ? Hx(0x6fa58c) : D.Chance(.5) ? Weathered(Hx(0xb87a48), .2, D) : Hx(0x2f3133);   // verdigris, copper, black iron
	const double H = 1.25 * Size;
	Tube(B, Base, Base + Up3 * H, .022 * Size, .016 * Size, 6, Iron);
	Ellip(B, Base + Up3 * (H * .42), FVector(1, 0, 0), Up3, FVector(0, 0, 1), .06 * Size, .06 * Size, .06 * Size, Metal, 8);
	// the points (north lined up with the town grid, the town's own north)
	for (int32 k = 0; k < 2; k++)
	{
		const FVector Ax = k ? FVector(1, 0, 0) : FVector(0, 0, 1);
		Bar(B, Base + Up3 * (H * .55) - Ax * (.38 * Size), Base + Up3 * (H * .55) + Ax * (.38 * Size), Up3, .01 * Size, .01 * Size, Iron);
		for (int32 s = -1; s <= 1; s += 2)
		{
			const FVector Tip = Base + Up3 * (H * .55) + Ax * (s * .38 * Size);
			B.Tri3(Tip + Up3 * (.05 * Size), Tip - Up3 * (.05 * Size), Tip + Ax * (s * .07 * Size), FVector::CrossProduct(Ax, Up3), Iron);
			B.Tri3(Tip + Up3 * (.05 * Size), Tip + Ax * (s * .07 * Size), Tip - Up3 * (.05 * Size), -FVector::CrossProduct(Ax, Up3), Iron);
		}
	}
	// the fish on top, swung round to wherever the last wind left it
	const double Yw = D.R(0, 2 * PI);
	const FVector AX(FMath::Cos(Yw), 0, FMath::Sin(Yw));
	CutoutFish(B, D.I(6), Base + Up3 * (H + .12 * Size), AX, Up3, .95 * Size, .02 * Size, Metal, Mul(Metal, .85), Mul(Metal, .7), false);
}

void FishMailbox(FTownBatch& B, const FVector& Base, const FVector& Facing, FKitDice& D)
{
	const FVector F = FVector(Facing.X, 0, Facing.Z).GetSafeNormal(), Sd = FVector::CrossProduct(Up3, F);
	// a 4x4 post and a little shelf under the box
	const FLinearColor Post = D.Chance(.5) ? SaltWood(D) : Weathered(Paint(EPaint::WhiteWash), .3, D);
	OBox(B, Base + Up3 * .52, F, Up3, Sd, .045, .52, .045, Post);
	OBox(B, Base + Up3 * 1.06, F, Up3, Sd, .26, .02, .1, Post);
	// the fish: its open mouth is the door, facing the road
	static const uint32 Schemes[][3] = {{0x6cc04a, 0xf0d23a, 0x2f7fb8}, {0x8a6a44, 0x5a4632, 0xc9b38a}, {0xd8473a, 0xf0a090, 0xf6e6d0}, {0x9fc8dc, 0x3c6f8f, 0xf0ebdf}, {0xef8a72, 0x3c9f96, 0xf2d98a}};
	const uint32* Sc = Schemes[D.I(5)];
	const FLinearColor Body = Weathered(Hx(Sc[0]), D.R(.1, .4), D), Fin = Weathered(Hx(Sc[1]), D.R(.1, .4), D), Belly = Weathered(Hx(Sc[2]), .2, D);
	const FVector C = Base + Up3 * 1.22;
	Ellip(B, C, F, Up3, Sd, .34, .15, .12, Body, 8);
	Ellip(B, C - Up3 * .05 + F * .04, F, Up3, Sd, .3, .1, .115, Belly, 6);
	// mouth (the door), eyes
	const FVector Mouth = C + F * .335;
	Slab(B, Ellipse(0, 0, .055, .075, 12), Mouth, Sd, Up3, F, .012, Hx(0x1a1c1e), Hx(0x1a1c1e), false);
	for (int32 s = -1; s <= 1; s += 2)
	{
		const FVector Ec = C + F * .2 + Up3 * .05 + Sd * (s * .1);
		Slab(B, Ellipse(0, 0, .028, .028, 8), Ec, F, Up3, Sd * double(s), .002, FLinearColor(.95f, .94f, .9f), FLinearColor(.95f, .94f, .9f), false);
		Slab(B, Ellipse(.005, 0, .014, .014, 6), Ec + Sd * (s * .003), F, Up3, Sd * double(s), .002, FLinearColor(.03f, .03f, .035f), FLinearColor(.03f, .03f, .035f), false);
	}
	// tail and fins as flat cut-outs
	const FVector Tl = C - F * .32;
	Slab(B, {FVector2D(0, 0), FVector2D(-.2, .17), FVector2D(-.14, 0)}, Tl - Sd * .01, F, Up3, Sd, .02, Fin, Mul(Fin, .8));
	Slab(B, {FVector2D(0, 0), FVector2D(-.14, 0), FVector2D(-.2, -.17)}, Tl - Sd * .01, F, Up3, Sd, .02, Fin, Mul(Fin, .8));
	Slab(B, {FVector2D(-.18, .12), FVector2D(.12, .14), FVector2D(-.08, .26)}, C - Sd * .01, F, Up3, Sd, .02, Fin, Mul(Fin, .8));
	// the flag: up (mail to go) or down
	const bool bUp = D.Chance(.35);
	const FVector Pv = C + Sd * .125 - F * .05;
	const FVector FlagDir = bUp ? Up3 : -F;
	Bar(B, Pv, Pv + FlagDir * .22, Sd, .008, .006, Hx(0xd12f2a));
	OBox(B, Pv + FlagDir * .19 + (bUp ? -F * .04 : Up3 * .04), F, Up3, Sd, bUp ? .045 : .03, bUp ? .03 : .045, .006, Hx(0xd12f2a));
}

void Statue(FTownBatch& B, const FVector& Base, double Height, FKitDice& D, TArray<FKitText>* Texts)
{
	// the wave it bursts out of: a curl of blue gelcoat with a white foam lip
	const uint8 Keep = B.M.Surf;
	B.M.Surf = SurfGelcoat;
	const FLinearColor Sea = Hx(0x1f7fb5), Deep = Hx(0x145e8c), Foam = Hx(0xf4f6f2);
	const double R = 1.15;
	for (int32 i = 0; i < 9; i++)
	{
		const double T = i / 8.0;
		const FVector C = Base + FVector(FMath::Lerp(-R, R, T), .35 + .55 * FMath::Sin(T * PI) + .25 * T, FMath::Sin(T * 7) * .12);
		Ellip(B, C, FVector(1, 0, 0), Up3, FVector(0, 0, 1), .42, .45 + .2 * FMath::Sin(T * PI), .62, i % 2 ? Sea : Deep, 10);
	}
	for (int32 i = 0; i < 7; i++)
	{
		const double T = (i + .5) / 7.0;
		Ellip(B, Base + FVector(FMath::Lerp(-R * .9, R * .9, T), .95 + .55 * FMath::Sin(T * PI) + .25 * T, .25 * FMath::Sin(i * 2.1)), FVector(1, 0, 0), Up3, FVector(0, 0, 1), .26, .16, .3, Foam, 8);
	}
	// the giant tarpon: leaping nose to the sky, painted the way a sign shop paints a fish (bright, glossy)
	const double Len = Height * .92;
	const FVector Nose = FVector(.55, .95, 0).GetSafeNormal();
	const FVector UpV(-Nose.Y, Nose.X, 0);
	const FVector Tail = Base + FVector(-.2, 1.1, 0);
	PosedFish(B, TEXT("tarpon"), Len, Tail + Nose * (Len * .5), Nose, UpV, FVector(0, 0, 1), .26, 0, 1, 0, .6, SurfGelcoat, D, true);
	// splash drops flung off it
	for (int32 i = 0; i < 9; i++)
	{
		const FVector Pd = Base + FVector(D.R(-1.4, 1.4), D.R(1.4, 2.6), D.R(-.7, .7));
		Ellip(B, Pd, FVector(1, 0, 0), Up3, FVector(0, 0, 1), .07, .11, .07, Foam, 6);
		Tube(B, Pd - Up3 * .08, Pd - Up3 * D.R(.25, .5) + FVector(D.R(-.1, .1), 0, 0), .05, .01, 5, Foam);
	}
	B.M.Surf = Keep;
	// its brass plaque says what it is
	if (Texts)
	{
		Texts->Add({TEXT("BIG TARP"), Base + FVector(0, -.43, 1.36), FVector(0, 0, 1), .16, .85, Hx(0x2a2418)});
		Texts->Add({TEXT("WORLD'S LARGEST FIBREGLASS TARPON (PROBABLY)"), Base + FVector(0, -.58, 1.36), FVector(0, 0, 1), .04, .85, Hx(0x2a2418)});
	}
}

void Mural(FTownBatch& B, const FWall& W, double A0, double A1, double Y0, double Y1, const FLinearColor& WallC, int32 Theme, FKitDice& D, TArray<FKitText>* Texts)
{
	const uint8 Keep = B.M.Surf;
	B.M.Surf = SurfStucco;
	const double Fd = D.R(.08, .2);   // how far gone it is
	auto Fd2 = [&](const FLinearColor& C) { return Weathered(C, Fd, D); };
	const double Wd = A1 - A0, Ht = Y1 - Y0, SeaY = Y0 + Ht * D.R(.3, .38);
	const FLinearColor SkyTop = Fd2(Theme == 0 ? Hx(0xf0a36a) : Theme == 2 ? Hx(0xe7a2b6) : Hx(0x5aa7dc));
	const FLinearColor SkyLow = Fd2(Theme == 0 ? Hx(0xf6dc8a) : Theme == 2 ? Hx(0xf8e0c8) : Hx(0xbfe4f2));
	const FLinearColor SeaTop = Fd2(Hx(0x2bb3b8)), SeaLow = Fd2(Hx(0x1d5f8c));
	const double Dd = .008;
	// the painted ground: a sky fading to the horizon, the sea darkening toward the foot of the wall
	auto Grad = [&](double Ya, double Yb, const FLinearColor& Ca, const FLinearColor& Cb, double Dz)
	{
		B.TriCol(W.At(A0, Ya, Dz), W.At(A1, Ya, Dz), W.At(A1, Yb, Dz), W.N, Ca, Ca, Cb);
		B.TriCol(W.At(A0, Ya, Dz), W.At(A1, Yb, Dz), W.At(A0, Yb, Dz), W.N, Ca, Cb, Cb);
	};
	Grad(SeaY, Y1, SkyLow, SkyTop, Dd);
	Grad(Y0, SeaY, SeaLow, SeaTop, Dd);
	// the sun, low over the water
	const double SunA = A0 + Wd * D.R(.65, .85), SunR = FMath::Min(Ht * .16, Wd * .1);
	Slab(B, Ellipse(0, 0, SunR, SunR, 22), W.At(SunA, SeaY + Ht * D.R(.22, .4), Dd), W.R, Up3, W.N, .003, Fd2(Hx(Theme == 1 ? 0xfff2a0 : 0xffc05a)), Fd2(Hx(0xffc05a)), false);
	// wave strokes and a far key with palms
	for (int32 k = 0; k < 6; k++)
	{
		const double Wy = Y0 + (SeaY - Y0) * (k + .5) / 6;
		for (double A = A0 + D.R(.1, .5); A < A1 - .4; A += D.R(.6, 1.2))
		{
			const double L = D.R(.25, .5);
			B.Quad3(W.At(A, Wy, Dd + .002), W.At(A + L, Wy + .03, Dd + .002), W.At(A + L, Wy + .055, Dd + .002), W.At(A, Wy + .025, Dd + .002), W.N, Fd2(Hx(0xeaf6f4)));
		}
	}
	{
		const double KA = A0 + Wd * D.R(.08, .3), KW = Wd * .22;
		B.Quad3(W.At(KA, SeaY, Dd + .002), W.At(KA + KW, SeaY, Dd + .002), W.At(KA + KW * .85, SeaY + .12, Dd + .002), W.At(KA + KW * .1, SeaY + .1, Dd + .002), W.N, Fd2(Hx(0x3f7a4a)));
		for (int32 p = 0; p < 3; p++)
		{
			const double Pa = KA + KW * (.25 + p * .25), Ph = D.R(.5, .8);
			B.Quad3(W.At(Pa, SeaY + .08, Dd + .003), W.At(Pa + .04, SeaY + .08, Dd + .003), W.At(Pa + .1, SeaY + Ph, Dd + .003), W.At(Pa + .07, SeaY + Ph, Dd + .003), W.N, Fd2(Hx(0x6a4a2e)));
			for (int32 f = 0; f < 5; f++)
			{
				const double Ang = f * 1.26 + .3, Fl = D.R(.18, .26);
				const FVector Cr = W.At(Pa + .085, SeaY + Ph, Dd + .004);
				B.Tri3(Cr, Cr + W.R * (FMath::Cos(Ang) * Fl) + Up3 * (FMath::Sin(Ang) * Fl * .5 - .05), Cr + W.R * (FMath::Cos(Ang + .3) * Fl * .5) + Up3 * .02, W.N, Fd2(Hx(0x3f8a4a)));
			}
		}
	}
	// the fish: tarpon (or a sailfish) mid-leap, painted flat on the wall, with the splash it came out of
	const bool bSail = Theme == 1;
	const double FLen = FMath::Min(Wd * .42, Ht * 1.05);
	const double FA = A0 + Wd * D.R(.38, .55), FY = SeaY + Ht * .3;
	const double Tilt = D.R(.45, .7), Dir = D.Chance(.5) ? 1 : -1;
	const FVector Nose = W.R * (Dir * FMath::Cos(Tilt)) + Up3 * FMath::Sin(Tilt), UpV = Up3 * FMath::Cos(Tilt) - W.R * (Dir * FMath::Sin(Tilt));
	PosedFish(B, bSail ? TEXT("sailfish") : TEXT("tarpon"), FLen, W.At(FA, FY, Dd + .03), Nose, UpV, W.N, .3, 0, .04, Fd * .5, 1.0, SurfDefault, D);
	if (Theme == 2) PosedFish(B, TEXT("mahi"), FLen * .45, W.At(A0 + Wd * .2, SeaY + Ht * .18, Dd + .02), W.R * -Dir, Up3, W.N, .25, 0, .04, Fd * .8, .7, SurfDefault, D);
	for (int32 i = 0; i < 14; i++)
	{
		const double Sa = FA - Dir * FLen * .4 + D.R(-.6, .6), Sy = SeaY + D.R(-.05, .45);
		Slab(B, Ellipse(0, 0, D.R(.03, .08), D.R(.04, .1), 8), W.At(Sa, Sy, Dd + .003), W.R, Up3, W.N, .002, Fd2(Hx(0xf4fbfa)), Fd2(Hx(0xf4fbfa)), false);
	}
	// a painted border
	const FLinearColor Frame = Fd2(Paint(D.Chance(.5) ? EPaint::Coral : EPaint::Teal));
	B.Quad3(W.At(A0 - .08, Y0 - .08, Dd - .002), W.At(A1 + .08, Y0 - .08, Dd - .002), W.At(A1 + .08, Y1 + .08, Dd - .002), W.At(A0 - .08, Y1 + .08, Dd - .002), W.N, Frame);
	// the lettering across the sky
	if (Texts)
	{
		static const TCHAR* const Lines[] = {TEXT("GREETINGS FROM BOOT KEY HARBOR"), TEXT("SILVER KING COUNTRY"), TEXT("WHERE THE TARPON JUMP"), TEXT("SAILFISH CAPITAL OF NOWHERE IN PARTICULAR")};
		const FString Ln = bSail ? Lines[3] : Lines[D.I(3)];
		Texts->Add({Ln, W.At((A0 + A1) * .5, Y1 - Ht * .12, Dd + .03), W.N, FMath::Min(.42, Ht * .1), Wd * .9, Fd2(Hx(Theme == 1 ? 0xf6f2e4 : 0x23506e))});
	}
	// peeling, and the salt creeping up from the foot
	PeelingPaint(B, W, A0, A1, Y0, Y1, WallC, 18 + D.I(14), D);
	SaltStains(B, W, A0 - .1, A1 + .1, Y0 - .08, SeaLow, D);
	B.M.Surf = Keep;
}

// ==================================================================== wear
void SaltStains(FTownBatch& B, const FWall& W, double A0, double A1, double Y0, const FLinearColor& WallC, FKitDice& D, uint8 WallSurf)
{
	const uint8 Keep = B.M.Surf;
	B.M.Surf = WallSurf;
	const FLinearColor Salt(.92f, .91f, .87f, 1);
	const FLinearColor Bot = Mix(WallC, Salt, WallC.GetLuminance() < .3f ? .14 : .22);
	for (double A = A0 + D.R(0, .4); A < A1 - .15; A += D.R(.5, 1.3))
	{
		const double Wd = FMath::Min(D.R(.5, 1.2), A1 - A), H = D.R(.12, .38), Pk = D.R(.3, .7);
		const double Dz = .006 + D.R(0, .002);
		// a ragged top: a peak somewhere along it
		const FVector Lb = W.At(A, Y0, Dz), Rb = W.At(A + Wd, Y0, Dz), Lt = W.At(A, Y0 + H * D.R(.55, .85), Dz), Rt = W.At(A + Wd, Y0 + H * D.R(.55, .85), Dz), Pt = W.At(A + Wd * Pk, Y0 + H, Dz);
		const FVector Mb = W.At(A + Wd * Pk, Y0, Dz);
		B.TriCol(Lb, Mb, Pt, W.N, Bot, Bot, WallC);
		B.TriCol(Lb, Pt, Lt, W.N, Bot, WallC, WallC);
		B.TriCol(Mb, Rb, Rt, W.N, Bot, Bot, WallC);
		B.TriCol(Mb, Rt, Pt, W.N, Bot, WallC, WallC);
	}
	B.M.Surf = Keep;
}

void PeelingPaint(FTownBatch& B, const FWall& W, double A0, double A1, double Y0, double Y1, const FLinearColor& WallC, int32 Count, FKitDice& D, uint8 WallSurf)
{
	const uint8 Keep = B.M.Surf;
	B.M.Surf = WallSurf;
	// what shows through: the bare stuff under it, or the colour it was painted before
	const FLinearColor Under = D.Chance(.6) ? Mix(WallC, Hx(0x9a948a), .65) : Weathered(RandomPastel(D), .3, D);
	for (int32 i = 0; i < Count; i++)
	{
		// most of it near the foot and the edges, where the weather gets in
		double A = D.R(A0, A1), Y = D.R(Y0, Y1);
		if (D.Chance(.5)) Y = Y0 + (Y1 - Y0) * FMath::Pow(D.F(), 2.5);
		if (D.Chance(.35)) A = D.Chance(.5) ? A0 + D.R(0, .35) : A1 - D.R(0, .35);
		const double R = D.R(.025, .11);
		Patch(B, W, A, Y, .007, R, Under, D);
		// the curled lip of paint round it, a shade lighter than the wall
		if (R > .06) Patch(B, W, A + R * .2, Y + R * .1, .0072, R * .45, Mix(WallC, FLinearColor::White, .25), D, 5);
	}
	B.M.Surf = Keep;
}

void RustStreak(FTownBatch& B, const FWall& W, double A, double Y, double Len, const FLinearColor& WallC, FKitDice& D, uint8 WallSurf)
{
	const uint8 Keep = B.M.Surf;
	B.M.Surf = WallSurf;
	const FLinearColor Rs = Rust(D), Fade = Mix(Rs, WallC, .85);
	const double Wt = D.R(.03, .06), Wb = D.R(.008, .02), Sk = D.R(-.03, .03);
	const FVector T0 = W.At(A - Wt, Y, .006), T1 = W.At(A + Wt, Y, .006), B0 = W.At(A + Sk - Wb, Y - Len, .006), B1 = W.At(A + Sk + Wb, Y - Len, .006);
	const FVector M0 = W.At(A + Sk * .5 - Wt * .7, Y - Len * .4, .006), M1 = W.At(A + Sk * .5 + Wt * .7, Y - Len * .4, .006);
	const FLinearColor Mc = Mix(Rs, WallC, .35);
	B.TriCol(T0, T1, M1, W.N, Rs, Rs, Mc);
	B.TriCol(T0, M1, M0, W.N, Rs, Mc, Mc);
	B.TriCol(M0, M1, B1, W.N, Mc, Mc, Fade);
	B.TriCol(M0, B1, B0, W.N, Mc, Fade, Fade);
	B.M.Surf = Keep;
}

// ==================================================================== the recipes
void DressShop(FTownBatch& B, FTownBatch& Gl, const FShopSpots& S, FKitDice& D, TArray<FKitText>& Texts)
{
	(void)Gl;
	const FWall& W = S.Front;
	const double DL = S.DoorA - S.DoorW * .5, DR = S.DoorA + S.DoorW * .5;
	// the blade sign by the door, readable from down the street
	{
		// on the corner of the front, clear of the name band
		const bool bRight = FMath::Abs(S.DoorA - S.Width) > FMath::Abs(S.DoorA) ? D.Chance(.7) : D.Chance(.3);
		const double A = bRight ? S.Width - .42 : .42;
		const double Len = D.R(1.6, 1.95);
		FishSign(B, W, A, S.AwningY + .75, Len, S.Pun, D, &Texts, true);
		RustStreak(B, W, A, S.AwningY + .75 + Len * .26 + .1, D.R(.4, .8), S.WallC, D, S.WallSurf);
	}
	// buoys and floats strung along the awning's front edge
	if (S.bAwning)
	{
		const FWall Edge = FWall::Make(W.At(0, 0, 1.72), W.N);
		const int32 N = FMath::Max(2, int32((S.Width - 2) / D.R(2.4, 3.6)));
		for (int32 i = 0; i < N; i++)
		{
			const double A = 1 + (S.Width - 2) * (i + .5) / N + D.R(-.3, .3);
			if (A > DL - .4 && A < DR + .4) continue;
			if (D.Chance(.65)) Buoy(B, Edge, A, S.AwningY - .42, D.R(.46, .58), D);
			else NetFloats(B, Edge, A, S.AwningY - .4, D);
		}
	}
	// the free bits of wall between the windows: buoys, floats, a flush fish sign
	for (const FVector2D& Fr : S.Free)
	{
		const double Wd = Fr.Y - Fr.X, Mid = (Fr.X + Fr.Y) * .5;
		if (Wd < .35) continue;
		const int32 Pick = D.I(4);
		if (Pick == 0) Buoy(B, W, Mid, D.R(2.3, 2.6), D.R(.38, .5), D);
		else if (Pick == 1) NetFloats(B, W, Mid, D.R(2.4, 2.7), D);
		else if (Pick == 2 && Wd > .55) FishSign(B, W, Mid, D.R(1.7, 2.1), FMath::Min(Wd * 1.3, .75), FString(), D, nullptr, false);
		else if (Wd > .9) RopeOnWall(B, W, Mid, 2.3, D);
		else Buoy(B, W, Mid, 2.45, .42, D);
	}
	// the clutter along the foot of the wall (never in front of the door)
	auto ClearOfDoor = [&](double A, double Half) { return A + Half < DL - .5 || A - Half > DR + .5; };
	const int32 NClut = 1 + D.I(3);
	for (int32 i = 0; i < NClut; i++)
	{
		double A = D.R(.8, S.Width - .8);
		for (int32 t = 0; t < 6 && !ClearOfDoor(A, .7); t++) A = D.R(.8, S.Width - .8);
		if (!ClearOfDoor(A, .7)) continue;
		const int32 Kind = D.I(3);
		if (Kind == 0) LobsterTraps(B, W.At(A, 0, .5), FMath::Atan2(-W.R.Z, W.R.X) + D.R(-.2, .2), 1 + D.I(3), D);
		else if (Kind == 1) RopeCoil(B, W.At(A, .02, .45), Up3, D.R(.22, .3), D);
		else
		{
			// a couple of old buoys and a float dumped by the wall
			for (int32 k = 0; k < 2; k++)
			{
				const FLinearColor Bc = Weathered(k ? Hx(0xe2552e) : RandomPastel(D), .35, D);
				const FVector Ct = W.At(A + k * .35 - .15, .09, .35 + k * .12);
				Ellip(B, Ct, W.R, Up3, W.N, .26, .09, .09, Bc, 8);
			}
		}
	}
	// a fish mailbox at the kerb for some
	if (D.Chance(.35)) FishMailbox(B, W.At(FMath::Clamp(S.DoorA + (D.Chance(.5) ? 2.6 : -2.6), .6, S.Width - .6), 0, 4.7), W.N, D);
	// a weathervane up top
	if (S.bRoofFree && D.Chance(.7)) Weathervane(B, S.RoofTop, D.R(1.0, 1.3), D);
	// the wear: salt rising along the foot, paint lifting by the corners and under the windows
	SaltStains(B, W, .05, DL - .05, .01, S.WallC, D, S.WallSurf);
	SaltStains(B, W, DR + .05, S.Width - .05, .01, S.WallC, D, S.WallSurf);
	PeelingPaint(B, W, .05, DL - .2, .02, .75, S.WallC, 5 + D.I(6), D, S.WallSurf);
	PeelingPaint(B, W, DR + .2, S.Width - .05, .02, .75, S.WallC, 5 + D.I(6), D, S.WallSurf);
}

void DressSideWall(FTownBatch& B, const FWall& W, double A0, double A1, double Height, const FLinearColor& WallC, uint8 WallSurf, bool bMural, FKitDice& D, TArray<FKitText>& Texts)
{
	const double Wd = A1 - A0;
	if (bMural)
	{
		Mural(B, W, A0 + .7, A1 - .7, .55, FMath::Min(Height - .45, 4.4), WallC, D.I(3), D, &Texts);
		return;
	}
	// the junk wall: a big trophy up high in the middle, the gear of a life of fishing round it
	const double Mid = (A0 + A1) * .5;
	static const TCHAR* const Big[] = {TEXT("sailfish"), TEXT("tarpon"), TEXT("goliath")};
	MountedFish(B, W, Mid, FMath::Min(Height - .9, 3.4), Big[D.I(3)], 0, D.I(4), D);
	if (Wd > 6)
	{
		CastNet(B, W, Mid - 2.4, 2.8, D);
		OldRods(B, W, Mid + 2.5, 2.4, D);
		PlankSign(B, W, Mid, 1.45, 2.2, Puns()[D.I(Puns().Num())], D, &Texts);
	}
	for (double A = A0 + .8; A < A1 - .6; A += D.R(1.4, 2.2))
	{
		if (FMath::Abs(A - Mid) < 1.6 || (Wd > 6 && (FMath::Abs(A - (Mid - 2.4)) < .7 || FMath::Abs(A - (Mid + 2.5)) < 1.1))) continue;
		if (D.Chance(.55)) Buoy(B, W, A, D.R(2.2, 2.7), D.R(.38, .52), D);
		else NetFloats(B, W, A, D.R(2.4, 2.8), D);
	}
	if (Wd > 4) LobsterTraps(B, W.At(A0 + Wd * D.R(.15, .3), 0, .5), FMath::Atan2(-W.R.Z, W.R.X), 2 + D.I(2), D);
	if (Wd > 4 && D.Chance(.6)) RopeCoil(B, W.At(A1 - Wd * .2, .02, .45), Up3, .26, D);
	SaltStains(B, W, A0, A1, .01, WallC, D, WallSurf);
	PeelingPaint(B, W, A0, A1, .02, Height * .7, WallC, 10 + D.I(10), D, WallSurf);
}

void DressInterior(FTownBatch& B, const FWall& Back, double BackLen, const FWall& Left, const FWall& Right, double SideLen, double Y, bool bBig, FKitDice& D)
{
	auto Pick = [&]() { return FString(TrophySpecies[D.I(7)]); };
	// either side of the shop's name on the back wall
	for (double F : {.16, .84})
	{
		if (BackLen < 9) break;
		const FString Sp = bBig ? FString(TEXT("goliath")) : Pick();
		double Len, Deep;
		TrophySize(Sp, D, Len, Deep);
		Len = FMath::Min(Len, BackLen * .22);
		MountedFish(B, Back, BackLen * F, Y, Sp, Len, D.I(4), D);
	}
	// down each side wall
	for (int32 s = 0; s < 2; s++)
	{
		const FWall& W = s ? Right : Left;
		const int32 N = SideLen > 16 ? 3 : SideLen > 9 ? 2 : 1;
		for (int32 i = 0; i < N; i++)
		{
			const double A = SideLen * (i + .5) / N + D.R(-.6, .6);
			const FString Sp = Pick();
			double Len, Deep;
			TrophySize(Sp, D, Len, Deep);
			Len = FMath::Min(Len, SideLen / N * .8);
			MountedFish(B, W, A, Y + D.R(-.15, .15), Sp, Len, D.I(4), D);
		}
	}
}

void DressHouse(FTownBatch& B, const FHouseSpots& S, FKitDice& D)
{
	const FWall& W = S.Front;
	// a fish mailbox out by the road
	{
		const double Side = D.Chance(.5) ? 1 : -1;
		FVector At0 = S.PorchFront + W.N * D.R(3.2, 4.5) + W.R * (Side * (S.PorchHalf * D.R(.3, .9)));
		At0.Y = S.GroundY;
		FishMailbox(B, At0, W.N, D);
	}
	if (D.Chance(.4)) Weathervane(B, S.Ridge - Up3 * .05, D.R(.8, 1.05), D);
	// a painted fish by the door
	if (D.Chance(.4)) FishSign(B, W, S.DoorA + (S.DoorA > 0 ? -1.3 : 1.3), D.R(1.9, 2.3), D.R(.6, .85), FString(), D, nullptr, false);
	// a buoy or floats hung on a porch post
	if (D.Chance(.45))
	{
		const double Side = D.Chance(.5) ? 1 : -1;
		const FVector Post = S.PorchFront + W.R * (Side * (S.PorchHalf - .2));
		const FWall PW = FWall::Make(Post + W.N * .07, W.N);
		if (D.Chance(.6)) Buoy(B, PW, 0, 1.9, D.R(.36, .46), D);
		else NetFloats(B, PW, 0, 2.0, D);
	}
	// traps stacked by the porch
	if (D.Chance(.25))
	{
		FVector Tp = S.PorchFront + W.R * ((D.Chance(.5) ? 1 : -1) * (S.PorchHalf + .7)) - W.N * .6;
		Tp.Y = S.GroundY;
		LobsterTraps(B, Tp, FMath::Atan2(-W.R.Z, W.R.X), 1 + D.I(2), D, true);
	}
	// and the weather on it
	SaltStains(B, W, .1, S.Width - .1, 0, S.WallC, D, SurfSiding);
	PeelingPaint(B, W, .1, S.Width - .1, .05, S.WallH * .8, S.WallC, 3 + D.I(5), D, SurfSiding);
}

// ==================================================================== the shapes, for other builders
void KitBox(FTownBatch& B, const FVector& C, const FVector& AX, const FVector& AY, const FVector& AZ, double HX, double HY, double HZ, const FLinearColor& Col) { OBox(B, C, AX, AY, AZ, HX, HY, HZ, Col); }
void KitTube(FTownBatch& B, const FVector& A, const FVector& E, double RA, double RB, int32 Sides, const FLinearColor& Col) { Tube(B, A, E, RA, RB, Sides, Col); }
void KitEllip(FTownBatch& B, const FVector& C, const FVector& AX, const FVector& AY, const FVector& AZ, double RX, double RY, double RZ, const FLinearColor& Col, int32 Seg) { Ellip(B, C, AX, AY, AZ, RX, RY, RZ, Col, Seg); }
void KitSlab(FTownBatch& B, const TArray<FVector2D>& Poly, const FVector& O, const FVector& AX, const FVector& AY, const FVector& N, double T, const FLinearColor& Col, const FLinearColor& EdgeC, bool bBack) { Slab(B, Poly, O, AX, AY, N, T, Col, EdgeC, bBack); }
TArray<FVector2D> KitEllipse(double CX, double CY, double A, double Bb, int32 N) { return Ellipse(CX, CY, A, Bb, N); }
void KitFish(FTownBatch& B, const FString& Key, double Len, const FVector& C, const FVector& Nose, const FVector& UpV, const FVector& Side, double Arch, double Thin, double FadeK, double PaintK, FKitDice& D, bool bFull)
{
	PosedFish(B, Key, Len, C, Nose, UpV, Side, Arch, 0, Thin, FadeK, PaintK, Thin < .2 ? SurfDefault : -1, D, bFull);
}
void KitCutout(FTownBatch& B, int32 Kind, const FVector& C, const FVector& AX, const FVector& AY, double Len, double T, const FLinearColor& Body, const FLinearColor& Fin, const FLinearColor& Edge, bool bEye)
{
	CutoutFish(B, Kind, C, AX, AY, Len, T, Body, Fin, Edge, bEye);
}
TArray<TArray<FVector2D>> KitFishOutline(int32 Kind)
{
	const FOutline O = FishShape(Kind);
	TArray<TArray<FVector2D>> R = O.Body;
	R.Append(O.Fins);
	return R;
}

void Pelican(FTownBatch& B, const FVector& Feet, const FVector& Facing, double Cock, FKitDice& D)
{
	const FVector F = FVector(Facing.X, 0, Facing.Z).GetSafeNormal(), Sd = FVector::CrossProduct(Up3, F);
	const FLinearColor Body = Mix(Hx(0x6f6a62), Hx(0x8a8378), D.F()), Wing = Mul(Body, .75), Belly = Mul(Body, .9);
	const FLinearColor White = Hx(0xf2eee2), Crown = Hx(0xe9cf72), Nape = Hx(0x6b3f26), BillC = Hx(0x9a8f78), Pouch = Hx(0x4a4a44), Leg = Hx(0x2a2a28);
	// legs and webbed feet
	for (int32 s = -1; s <= 1; s += 2)
	{
		const FVector Fo = Feet + Sd * (s * .07);
		Tube(B, Fo, Fo + Up3 * .14 - F * .02, .012, .014, 4, Leg);
		B.Tri3(Fo + F * .09, Fo - Sd * .05, Fo + Sd * .05, Up3, Leg);
	}
	// body tipped up at the front, folded wings, the tail
	const FVector Bc = Feet + Up3 * .3 - F * .02;
	const FVector BAx = (F + Up3 * .35).GetSafeNormal(), BUp = FVector::CrossProduct(BAx, Sd) * -1;
	const FVector BUp2 = BUp.Y < 0 ? -BUp : BUp;
	Ellip(B, Bc, BAx, BUp2, Sd, .3, .17, .16, Body, 10);
	Ellip(B, Bc - BUp2 * .05 + BAx * .03, BAx, BUp2, Sd, .26, .13, .145, Belly, 8);
	for (int32 s = -1; s <= 1; s += 2) Ellip(B, Bc + Sd * (s * .09) + BUp2 * .04 - BAx * .06, BAx, BUp2, Sd, .3, .1, .06, Wing, 8);
	Tube(B, Bc - BAx * .26, Bc - BAx * .42 - Up3 * .06, .06, .02, 5, Wing);
	// the S of the neck, chestnut down the back of it, folded down onto the shoulders
	const FVector N0 = Bc + BAx * .2 + BUp2 * .08, N1 = N0 + Up3 * .12 - F * .05, N2 = N1 + Up3 * .1 + F * .06;
	Tube(B, N0, N1, .055, .045, 6, Nape);
	Tube(B, N1, N2, .045, .045, 6, White);
	// the head, cocked; the long bill and its pouch
	const FVector Hf = (F * FMath::Cos(Cock) + Sd * FMath::Sin(Cock)).GetSafeNormal();
	const FVector Hc = N2 + Up3 * .03;
	Ellip(B, Hc, Hf, Up3, FVector::CrossProduct(Up3, Hf), .07, .055, .05, White, 8);
	Ellip(B, Hc + Up3 * .035, Hf, Up3, FVector::CrossProduct(Up3, Hf), .05, .025, .035, Crown, 6);
	const FVector BillDir = (Hf - Up3 * .45).GetSafeNormal();
	Bar(B, Hc + Hf * .05, Hc + Hf * .05 + BillDir * .34, FVector::CrossProduct(Up3, Hf), .018, .012, BillC);
	Ellip(B, Hc + Hf * .05 + BillDir * .17 - Up3 * .035, BillDir, Up3, FVector::CrossProduct(Up3, Hf), .16, .03, .02, Pouch, 6);
	Ellip(B, Hc + Hf * .05 + BillDir * .34, BillDir, Up3, FVector::CrossProduct(Up3, Hf), .025, .012, .015, Hx(0xd8763a), 5);
	// the eye
	for (int32 s = -1; s <= 1; s += 2) Ellip(B, Hc + Hf * .03 + FVector::CrossProduct(Up3, Hf) * (s * .045) + Up3 * .01, Hf, Up3, FVector::CrossProduct(Up3, Hf), .012, .012, .006, Hx(0x15171a), 5);
}
}
