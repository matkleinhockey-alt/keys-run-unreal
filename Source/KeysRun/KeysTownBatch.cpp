#include "KeysTown.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"

// FTownBatch: the web game's GeoBatch (entities/town/batch.ts), writing straight into an FKeysMesh.
// Input is the three.js town frame (x = u, y up, z = -v); output is the town component's frame, UE axes in meters:
// (x, z, y). That swap is a mirror, so every triangle is wound from its intended normal after the mapping.

namespace
{
	FVector ToUEL(const FVector& P3) { return FVector(P3.X, P3.Z, P3.Y); }

	// three.js Euler 'YXZ': R = Ry * Rx * Rz
	FVector Rot3(const FVector& V, double RX, double RY, double RZ)
	{
		FVector P = V;
		if (RZ != 0) { const double C = FMath::Cos(RZ), S = FMath::Sin(RZ); P = FVector(P.X * C - P.Y * S, P.X * S + P.Y * C, P.Z); }
		if (RX != 0) { const double C = FMath::Cos(RX), S = FMath::Sin(RX); P = FVector(P.X, P.Y * C - P.Z * S, P.Y * S + P.Z * C); }
		if (RY != 0) { const double C = FMath::Cos(RY), S = FMath::Sin(RY); P = FVector(P.X * C + P.Z * S, P.Y, -P.X * S + P.Z * C); }
		return P;
	}

	// which way round a front face goes in UE procedural meshes (front iff Dot(Cross(B-A, C-A), N) < 0); -KRWind=1 flips it for testing
	double WindSign()
	{
		static double S = 0;
		if (S == 0)
		{
			int32 W = 0;
			S = FParse::Value(FCommandLine::Get(), TEXT("KRWind="), W) && W > 0 ? 1 : -1;
		}
		return S;
	}
}

namespace KeysTownPaint
{
	FLinearColor Hex(uint32 C) { return FLinearColor(FColor((C >> 16) & 255, (C >> 8) & 255, C & 255)); }
	FLinearColor Fade(uint32 C, double K)
	{
		const FLinearColor A = Hex(C), B = Hex(0xe9e2d2);
		return FMath::Lerp(A, B, float(K));
	}

	FKeysHumanLook LookFor(const FString& Look, int32 i)
	{
		static const uint32 Shirts[] = {0xf26b5b, 0x3fb6c9, 0xf2c14e, 0x7bc96f, 0xe86aa8, 0xffffff, 0x5a7bd6, 0xff9f43};
		static const uint32 Hair[] = {0x3b2a1e, 0x1c1410, 0x7a3a1c, 0xd8b46a, 0x5a3825, 0x2e1c12};
		static const uint32 Skin[] = {0xd9a27f, 0xe9b48f, 0xc98d68, 0xb87a55, 0xf0c2a0, 0x9a6644};
		i = FMath::Abs(i);
		FKeysHumanLook L;
		L.Skin = Hex(Skin[i % 6]);
		L.Hair = Hex(Hair[(i * 3) % 6]);
		L.Shirt = Hex(Shirts[i % 8]);
		L.Bottom = Hex(0xcdbf9a);
		L.Shoes = Hex(0xe9e4d8);
		L.bShoes = true;
		auto Cap = [&L](uint32 C) { L.bCap = true; L.CapColor = Hex(C); };
		if (Look == TEXT("tourist")) { if (i % 2) Cap(0xffffff); L.bGlasses = i % 3 == 0; L.Bottom = Hex(0xe9e1c8); }
		else if (Look == TEXT("angler")) { L.Shirt = Hex(0x6b7f8a); Cap(0x23466e); L.Bottom = Hex(0x4d5a4a); L.Shoes = Hex(0x5a4a38); }
		else if (Look == TEXT("local")) { L.Shirt = Hex(i % 2 ? 0x2f2f33 : 0xb8423a); Cap(0x2b2118); L.Bottom = Hex(0x3d4a5a); L.Shoes = Hex(0xc9c2b2); }
		else if (Look == TEXT("woman")) { L.bLongHair = true; L.HairStyle = i % 4; L.Shirt = Hex(Shirts[(i + 2) % 8]); L.Bottom = Hex(0x3d5a8a); }
		else if (Look == TEXT("bouncer")) { L.Shirt = Hex(0x111114); L.Bottom = Hex(0x111114); L.bShortsLong = true; L.bGlasses = true; L.Shoes = Hex(0x111114); L.Skin = Hex(0xb87a55); }
		else if (Look == TEXT("bartender")) { L.Shirt = Hex(0xf3efe5); L.Bottom = Hex(0x2b2118); L.Hair = Hex(0x7a3a1c); L.bLongHair = true; L.HairStyle = 3; }
		else if (Look == TEXT("cook")) { L.Shirt = Hex(0xffffff); Cap(0xffffff); L.Bottom = Hex(0x23262a); }
		else if (Look == TEXT("patron")) { Cap(0x5a7bd6); L.Bottom = Hex(0x6b5a3a); }
		else if (Look == TEXT("dancer"))
		{
			static const uint32 B[] = {0xff3ea5, 0x8a3cff, 0x38e0ff};
			L.bBikini = true; L.bLongHair = true; L.HairStyle = i % 3; L.Bottom = Hex(B[i % 3]); L.Shirt = Hex(B[i % 3]); L.Skin = Hex(Skin[(i + 1) % 6]); L.bShoes = false;
		}
		else if (Look == TEXT("mc")) { L.Shirt = Hex(0x2a0f26); L.bShortsLong = true; L.Bottom = Hex(0x2a0f26); L.Shoes = Hex(0x15171a); L.bGlasses = true; }
		else if (Look == TEXT("dj")) { L.Shirt = Hex(0x15171a); Cap(0xff3ea5); L.Bottom = Hex(0x23262a); L.bGlasses = true; }
		else if (Look == TEXT("divemaster")) { L.Skin = Hex(0xb87a55); L.Shirt = Hex(0x1ea5b8); L.Bottom = Hex(0x23466e); L.Hair = Hex(0xd8b46a); Cap(0xffffff); L.Shoes = Hex(0x2b2b2b); }
		else if (Look == TEXT("baitman")) { L.Skin = Hex(0xc98d68); L.Shirt = Hex(0xd9d4a0); Cap(0x23466e); L.Bottom = Hex(0x4d5a4a); L.Shoes = Hex(0x3a2f22); L.bGlasses = true; L.Hair = Hex(0x9a9a9a); }
		else if (Look == TEXT("gunsmith")) { L.Skin = Hex(0xe9b48f); L.Shirt = Hex(0x1d1d20); L.bGlasses = true; L.Bottom = Hex(0x4a4636); L.Shoes = Hex(0x1a1a1a); Cap(0x3b3b3e); }
		else if (Look == TEXT("clerk")) { static const uint32 C[] = {0xd9342b, 0x2b6cb0, 0xf2c14e}; L.Shirt = Hex(C[i % 3]); L.Bottom = Hex(0x2b2f3a); if (i % 2 == 0) Cap(0xd9342b); }
		else if (Look == TEXT("pawnbroker")) { L.Skin = Hex(0xd9a27f); L.Shirt = Hex(0x6b4f2a); L.bGlasses = true; L.Bottom = Hex(0x2b2118); L.Hair = Hex(0x7a7a7a); L.Shoes = Hex(0x1a1a1a); }
		else if (Look == TEXT("scooper")) { L.Shirt = Hex(0xf7a7c6); Cap(0xffffff); L.Bottom = Hex(0xffffff); L.bLongHair = true; L.HairStyle = 2; L.Skin = Hex(0xf0c2a0); }
		else if (Look == TEXT("innkeeper")) { L.Skin = Hex(0xc98d68); L.Shirt = Hex(0xf3efe5); L.Bottom = Hex(0x7a3a2c); L.Hair = Hex(0x2e1c12); L.bGlasses = true; }
		else if (Look == TEXT("attendant")) { L.Shirt = Hex(0x1d6fb3); L.Bottom = Hex(0xcdbf9a); L.bLongHair = true; L.HairStyle = 1; L.Skin = Hex(0x9a6644); }
		// the casino (casino-people.ts): dealers in waistcoats, cashiers in teal, a pit boss in black, the marlin mascot
		else if (Look == TEXT("dealer")) { L.Shirt = Hex(0xf4f1e8); L.Bottom = Hex(0x15171a); L.bShortsLong = true; L.Shoes = Hex(0x15171a); }
		else if (Look == TEXT("cashier")) { L.Shirt = Hex(0x0e6d78); L.Bottom = Hex(0x15171a); L.bShortsLong = true; L.bLongHair = i % 2 == 1; }
		else if (Look == TEXT("pitboss")) { L.Shirt = Hex(0x15171a); L.Bottom = Hex(0x15171a); L.bShortsLong = true; L.bGlasses = true; L.Shoes = Hex(0x0d0d0d); }
		else if (Look == TEXT("mascot")) { L.Skin = Hex(0x1f6fb0); L.Shirt = Hex(0x1f6fb0); L.Bottom = Hex(0x1f6fb0); L.bShortsLong = true; Cap(0xe0b34a); L.Shoes = Hex(0xe0b34a); }
		else if (Look == TEXT("punter")) { L.bLongHair = i % 2 == 0; L.HairStyle = i % 4; L.Bottom = Hex(i % 2 ? 0x2b2f3a : 0x6b5a3a); }
		return L;
	}
}

void FTownBatch::Tri3(const FVector& A3, const FVector& B3, const FVector& C3, const FVector& N3, const FLinearColor& Col)
{
	const FVector A = ToUEL(A3), B = ToUEL(B3), C = ToUEL(C3), N = ToUEL(N3).GetSafeNormal();
	const double D = FVector::DotProduct(FVector::CrossProduct(B - A, C - A), N);
	const int32 I0 = M.AddVert(A, N, Col), I1 = M.AddVert(B, N, Col), I2 = M.AddVert(C, N, Col);
	if (D * WindSign() > 0) M.T.Append({I0, I1, I2});
	else M.T.Append({I0, I2, I1});
}

void FTownBatch::Quad3(const FVector& A3, const FVector& B3, const FVector& C3, const FVector& D3, const FVector& N3, const FLinearColor& Col)
{
	const FVector A = ToUEL(A3), B = ToUEL(B3), C = ToUEL(C3), Dd = ToUEL(D3), N = ToUEL(N3).GetSafeNormal();
	const double D = FVector::DotProduct(FVector::CrossProduct(B - A, C - A), N);
	const int32 I0 = M.AddVert(A, N, Col), I1 = M.AddVert(B, N, Col), I2 = M.AddVert(C, N, Col), I3 = M.AddVert(Dd, N, Col);
	if (D * WindSign() > 0) M.T.Append({I0, I1, I2, I0, I2, I3});
	else M.T.Append({I0, I2, I1, I0, I3, I2});
}

void FTownBatch::TriN(const FVector& A3, const FVector& B3, const FVector& C3, const FVector& NA, const FVector& NB, const FVector& NC, const FLinearColor& Col)
{
	const FVector A = ToUEL(A3), B = ToUEL(B3), C = ToUEL(C3);
	const FVector Na = ToUEL(NA).GetSafeNormal(), Nb = ToUEL(NB).GetSafeNormal(), Nc = ToUEL(NC).GetSafeNormal();
	const double D = FVector::DotProduct(FVector::CrossProduct(B - A, C - A), Na + Nb + Nc);
	const int32 I0 = M.AddVert(A, Na, Col), I1 = M.AddVert(B, Nb, Col), I2 = M.AddVert(C, Nc, Col);
	if (D * WindSign() > 0) M.T.Append({I0, I1, I2});
	else M.T.Append({I0, I2, I1});
}

void FTownBatch::Palm(const FVector& Base3, double Ht, double LeanX, double LeanZ, double Seed, int32 Fronds)
{
	auto Hs = [Seed](double K) { const double H = FMath::Sin(Seed * 91.7 + K * 12.9898) * 43758.5453; return H - FMath::FloorToDouble(H); };
	const uint8 Keep = M.Surf;
	// the trunk: rings of shared vertices along a leaning curve
	auto At = [&](double F) { return Base3 + FVector(LeanX * Ht * F * F, Ht * F, LeanZ * Ht * F * F); };
	auto Rad = [&](double F) { return .27 * (1 - .42 * F) + .16 * FMath::Exp(-F * 16); };
	const int32 NS = 9, NR = 9;
	const FLinearColor BarkA = KeysTownPaint::Hex(0x8c7758), BarkB = KeysTownPaint::Hex(0x9e8a6a);
	M.Surf = SurfBark;
	{
		TArray<FVector> Pts, Nrm;
		TArray<FLinearColor> Cols;
		for (int32 r = 0; r <= NR; r++)
		{
			const double F = double(r) / NR;
			const FVector C = At(F), T = (At(FMath::Min(1.0, F + .02)) - At(FMath::Max(0.0, F - .02))).GetSafeNormal();
			FVector U = FVector::CrossProduct(T, FVector(0, 0, 1)).GetSafeNormal();
			if (U.IsNearlyZero()) U = FVector(1, 0, 0);
			const FVector W = FVector::CrossProduct(T, U);
			for (int32 k = 0; k <= NS; k++)
			{
				const double A = 2 * PI * k / NS;
				const FVector O = U * FMath::Cos(A) + W * FMath::Sin(A);
				Pts.Add(C + O * Rad(F));
				Nrm.Add(O);
				Cols.Add(FMath::Lerp(BarkA, BarkB, float(.5 + .5 * FMath::Sin(F * 9 + Seed * 5))) * (r == 0 ? .8f : 1.f));
			}
		}
		const int32 W1 = NS + 1;
		for (int32 r = 0; r < NR; r++)
			for (int32 k = 0; k < NS; k++)
			{
				const int32 a = r * W1 + k, b = a + 1, c = a + W1 + 1, d = a + W1;
				TriN(Pts[a], Pts[b], Pts[c], Nrm[a], Nrm[b], Nrm[c], Cols[a]);
				TriN(Pts[a], Pts[c], Pts[d], Nrm[a], Nrm[c], Nrm[d], Cols[a]);
			}
	}
	const FVector Top = At(1);
	// the boot: old frond bases round the top of the trunk, and the nuts under the crown
	M.Surf = SurfBark;
	{
		const FLinearColor Boot = KeysTownPaint::Hex(0x6f6440);
		const int32 NB = 8;
		for (int32 k = 0; k < NB; k++)
		{
			const double A = 2 * PI * k / NB + Seed;
			const FVector O(FMath::Sin(A), 0, FMath::Cos(A));
			const FVector P0 = Top + O * .12 - FVector(0, .35, 0), P1 = Top + O * .32 + FVector(0, .05, 0), P2 = Top + O * .12 + FVector(0, .15, 0);
			const FVector S0 = FVector(O.Z, 0, -O.X) * .1;
			TriN(P0 - S0, P0 + S0, P1, O, O, O + FVector(0, .5, 0), Boot);
			TriN(P0 + S0, P2, P1, O, O, O + FVector(0, .5, 0), Boot * .9f);
		}
	}
	// fronds: a rachis arching out and down, leaflets hanging in a V on either side, longest mid-frond
	M.Surf = SurfLeaf;
	static const uint32 FrondC[] = {0x3f8a35, 0x4f9a3c, 0x58a443, 0x467f30, 0x5c9a3a};
	const int32 NSt = 7;
	for (int32 f = 0; f < Fronds; f++)
	{
		const double Ang = f * 2 * PI / Fronds + Seed * 7 + (Hs(f) - .5) * .35;
		const double Len = (3.0 + Hs(f + 20) * 1.3) * FMath::Clamp(Ht / 8, .75, 1.2);
		const double Pitch0 = .55 + Hs(f + 40) * .45 - (f % 3) * .12;   // the young ones stand up, the old ones droop
		const double Droop = .2 + Hs(f + 60) * .12;
		const FVector Dir0(FMath::Sin(Ang), 0, FMath::Cos(Ang));
		const FVector Side(Dir0.Z, 0, -Dir0.X);
		const FLinearColor Base = KeysTownPaint::Hex(FrondC[(f + FMath::Abs(int32(Seed * 9))) % 5]);
		TArray<FVector> R;
		R.Add(Top + FVector(0, .1, 0));
		for (int32 k = 1; k <= NSt; k++)
		{
			const double Pitch = Pitch0 - Droop * k * (1 + k * .12);
			const FVector D = Dir0 * FMath::Cos(Pitch) + FVector(0, FMath::Sin(Pitch), 0);
			R.Add(R.Last() + D * (Len / NSt));
		}
		for (int32 k = 0; k < NSt; k++)
		{
			const FVector A = R[k], B = R[k + 1];
			const FVector D = (B - A).GetSafeNormal();
			const FVector Up = FVector::CrossProduct(D, Side).GetSafeNormal() * -1;
			const double T0 = double(k) / NSt, T1 = double(k + 1) / NSt;
			const double LW = .2 + .85 * FMath::Sin(PI * FMath::Min(1.0, (T0 + T1) * .5 * 1.1));
			const FLinearColor C = FMath::Lerp(Base, Base * FLinearColor(1.15f, 1.1f, .7f), float(T1 * .5));
			for (int32 s = -1; s <= 1; s += 2)
			{
				// two leaflets per station, swept forward and hanging down
				for (int32 l = 0; l < 2; l++)
				{
					const FVector Root = FMath::Lerp(A, B, .1 + .45 * l);
					const FVector Tip = Root + Side * (s * LW) + D * (LW * .45) - Up * (LW * .55);
					const FVector Root2 = Root + D * (Len / NSt * .45);
					const FVector Nn = (Up + Side * (s * .3)).GetSafeNormal();
					TriN(Root, Root2, Tip, Nn, Nn, (Nn - Up * .3).GetSafeNormal(), l ? C : C * .92f);
				}
			}
			// the rachis: a thin ribbon
			TriN(A - Side * .04, B - Side * .025, B + Side * .025, Up, Up, Up, Base * .8f);
			TriN(A - Side * .04, B + Side * .025, A + Side * .04, Up, Up, Up, Base * .8f);
		}
	}
	// coconuts
	M.Surf = SurfGelcoat;
	for (int32 k = 0; k < 3; k++)
	{
		const double A = k * 2.1 + Seed * 3;
		const FVector Cn = Top + FVector(FMath::Sin(A) * .2, -.25 - k * .04, FMath::Cos(A) * .2);
		const FVector UE = ToUEL(Cn);
		KeysShapes::AddBlob(M, UE, FVector(.12, .12, .14), FQuat::Identity, KeysTownPaint::Hex(k == 1 ? 0x6a5a30 : 0x5a4a2a), 6, 4);
	}
	M.Surf = Keep;
}

void FTownBatch::BoxC(double CX, double CY, double CZ, double W, double H, double D, const FLinearColor& Col, double RX, double RY, double RZ)
{
	const FVector C(CX, CY, CZ);
	const double HX = W * .5, HY = H * .5, HZ = D * .5;
	auto P = [&](double SX, double SY, double SZ) { return C + Rot3(FVector(SX * HX, SY * HY, SZ * HZ), RX, RY, RZ); };
	auto Nn = [&](double X, double Y, double Z) { return Rot3(FVector(X, Y, Z), RX, RY, RZ); };
	Quad3(P(1, -1, -1), P(1, 1, -1), P(1, 1, 1), P(1, -1, 1), Nn(1, 0, 0), Col);
	Quad3(P(-1, -1, -1), P(-1, -1, 1), P(-1, 1, 1), P(-1, 1, -1), Nn(-1, 0, 0), Col);
	Quad3(P(-1, 1, -1), P(-1, 1, 1), P(1, 1, 1), P(1, 1, -1), Nn(0, 1, 0), Col);
	if (CY - HY > -1.5 || RX != 0 || RZ != 0) Quad3(P(-1, -1, -1), P(1, -1, -1), P(1, -1, 1), P(-1, -1, 1), Nn(0, -1, 0), Col);
	Quad3(P(-1, -1, 1), P(1, -1, 1), P(1, 1, 1), P(-1, 1, 1), Nn(0, 0, 1), Col);
	Quad3(P(-1, -1, -1), P(-1, 1, -1), P(1, 1, -1), P(1, -1, -1), Nn(0, 0, -1), Col);
}

void FTownBatch::Box(double CX, double CY, double CZ, double W, double H, double D, uint32 Color, double RX, double RY, double RZ)
{
	BoxC(CX, CY, CZ, W, H, D, KeysTownPaint::Hex(Color), RX, RY, RZ);
}

void FTownBatch::SpanC(double X0, double X1, double Y0, double Y1, double Z0, double Z1, const FLinearColor& Color)
{
	BoxC((X0 + X1) / 2, (Y0 + Y1) / 2, (Z0 + Z1) / 2, FMath::Abs(X1 - X0), FMath::Abs(Y1 - Y0), FMath::Abs(Z1 - Z0), Color);
}

void FTownBatch::Span(double X0, double X1, double Y0, double Y1, double Z0, double Z1, uint32 Color)
{
	SpanC(X0, X1, Y0, Y1, Z0, Z1, KeysTownPaint::Hex(Color));
}

void FTownBatch::Cyl(double CX, double Y0, double CZ, double RTop, double RBot, double H, uint32 Color, int32 Seg, double RX, double RY, double RZ)
{
	const FLinearColor Col = KeysTownPaint::Hex(Color);
	const FVector C(CX, Y0 + H / 2, CZ);
	Seg = FMath::Max(3, Seg);
	auto P = [&](double A, double R, double Y) { return C + Rot3(FVector(FMath::Sin(A) * R, Y, FMath::Cos(A) * R), RX, RY, RZ); };
	const double Slope = (RBot - RTop) / FMath::Max(H, 1e-4);
	for (int32 i = 0; i < Seg; i++)
	{
		const double A0 = 2 * PI * i / Seg, A1 = 2 * PI * (i + 1) / Seg, AM = (A0 + A1) * .5;
		const FVector N = Rot3(FVector(FMath::Sin(AM), Slope, FMath::Cos(AM)).GetSafeNormal(), RX, RY, RZ);
		if (RTop > 1e-4) Quad3(P(A0, RBot, -H / 2), P(A1, RBot, -H / 2), P(A1, RTop, H / 2), P(A0, RTop, H / 2), N, Col);
		else Tri3(P(A0, RBot, -H / 2), P(A1, RBot, -H / 2), P(A0, 0, H / 2), N, Col);
		if (RTop > 1e-4) Tri3(P(A0, RTop, H / 2), P(A1, RTop, H / 2), C + Rot3(FVector(0, H / 2, 0), RX, RY, RZ), Rot3(FVector(0, 1, 0), RX, RY, RZ), Col);
		if (RBot > 1e-4 && (Y0 > -1.5 || RX != 0 || RZ != 0)) Tri3(P(A0, RBot, -H / 2), P(A1, RBot, -H / 2), C + Rot3(FVector(0, -H / 2, 0), RX, RY, RZ), Rot3(FVector(0, -1, 0), RX, RY, RZ), Col);
	}
}

void FTownBatch::Ball(double CX, double CY, double CZ, double R, uint32 Color, double SY, int32 Seg)
{
	const FLinearColor Col = KeysTownPaint::Hex(Color);
	const int32 NU = FMath::Max(4, Seg), NV = FMath::Max(3, Seg - 3);
	const FVector C(CX, CY, CZ);
	auto P = [&](int32 i, int32 j)
	{
		const double Lon = 2 * PI * i / NU, Lat = -PI / 2 + PI * j / NV;
		return FVector(FMath::Cos(Lat) * FMath::Sin(Lon), FMath::Sin(Lat), FMath::Cos(Lat) * FMath::Cos(Lon));
	};
	for (int32 j = 0; j < NV; j++)
		for (int32 i = 0; i < NU; i++)
		{
			const FVector A = P(i, j), B = P(i + 1, j), Cc = P(i + 1, j + 1), D = P(i, j + 1);
			auto S = [&](const FVector& V) { return C + FVector(V.X * R, V.Y * R * SY, V.Z * R); };
			const FVector N = (A + Cc).GetSafeNormal();
			if (j == 0) Tri3(S(A), S(Cc), S(D), N, Col);
			else if (j == NV - 1) Tri3(S(A), S(B), S(Cc), N, Col);
			else Quad3(S(A), S(B), S(Cc), S(D), N, Col);
		}
}

void FTownBatch::Gable(double CX, double Y0, double CZ, double W, double D, double Rise, uint32 Color, bool bAlongX)
{
	const FLinearColor Col = KeysTownPaint::Hex(Color);
	// across = the triangle's base, along = the ridge
	const double HW = (bAlongX ? D : W) / 2, HD = (bAlongX ? W : D) / 2;
	auto P = [&](double A, double Y, double L)
	{
		return bAlongX ? FVector(CX + L, Y0 + Y, CZ + A) : FVector(CX + A, Y0 + Y, CZ + L);
	};
	auto Nv = [&](double A, double Y, double L) { return bAlongX ? FVector(L, Y, A) : FVector(A, Y, L); };
	Tri3(P(-HW, 0, HD), P(HW, 0, HD), P(0, Rise, HD), Nv(0, 0, 1), Col);
	Tri3(P(-HW, 0, -HD), P(HW, 0, -HD), P(0, Rise, -HD), Nv(0, 0, -1), Col);
	Quad3(P(-HW, 0, -HD), P(-HW, 0, HD), P(0, Rise, HD), P(0, Rise, -HD), Nv(-Rise, HW, 0), Col);
	Quad3(P(HW, 0, -HD), P(HW, 0, HD), P(0, Rise, HD), P(0, Rise, -HD), Nv(Rise, HW, 0), Col);
}

void FTownBatch::Flat(double X0, double X1, double Z0, double Z1, double Y, const FLinearColor& Col)
{
	Quad3(FVector(X0, Y, Z0), FVector(X1, Y, Z0), FVector(X1, Y, Z1), FVector(X0, Y, Z1), FVector(0, 1, 0), Col);
}

void FTownBatch::Facade(double X0, double X1, double Y0, double Y1, double Z, bool bFront, const FLinearColor& Col)
{
	Quad3(FVector(X0, Y0, Z), FVector(X1, Y0, Z), FVector(X1, Y1, Z), FVector(X0, Y1, Z), FVector(0, 0, bFront ? 1 : -1), Col);
}

void FTownBatch::SideFace(double Z0, double Z1, double Y0, double Y1, double X, bool bPlus, const FLinearColor& Col)
{
	Quad3(FVector(X, Y0, Z0), FVector(X, Y0, Z1), FVector(X, Y1, Z1), FVector(X, Y1, Z0), FVector(bPlus ? 1 : -1, 0, 0), Col);
}

void FTownBatch::Tube3(const FVector& A3, const FVector& B3, double RA, double RB, int32 Sides, uint32 Color)
{
	const FLinearColor Col = KeysTownPaint::Hex(Color);
	const FVector Dir = (B3 - A3).GetSafeNormal();
	if (Dir.IsNearlyZero()) return;
	FVector U = FVector::CrossProduct(Dir, FVector(0, 1, 0));
	if (U.SizeSquared() < 1e-6) U = FVector::CrossProduct(Dir, FVector(1, 0, 0));
	U.Normalize();
	const FVector W = FVector::CrossProduct(Dir, U);
	for (int32 i = 0; i < Sides; i++)
	{
		const double A0 = 2 * PI * i / Sides, A1 = 2 * PI * (i + 1) / Sides;
		const FVector O0 = U * FMath::Cos(A0) + W * FMath::Sin(A0), O1 = U * FMath::Cos(A1) + W * FMath::Sin(A1);
		Quad3(A3 + O0 * RA, B3 + O0 * RB, B3 + O1 * RB, A3 + O1 * RA, (O0 + O1) * .5, Col);
	}
}

void FTownBatch::HipRoof(double CX, double Y0, double CZ, double W, double D, double Rise, const FLinearColor& Col, double RY)
{
	const FVector C(CX, Y0, CZ);
	auto P = [&](double X, double Y, double Z) { return C + Rot3(FVector(X, Y, Z), 0, RY, 0); };
	const double HW = W / 2, HD = D / 2;
	// the ridge runs along the longer side
	const bool bAlongZ = D >= W;
	const double R = bAlongZ ? HD - HW : HW - HD;
	const FVector RA = bAlongZ ? P(0, Rise, -R) : P(-R, Rise, 0), RB = bAlongZ ? P(0, Rise, R) : P(R, Rise, 0);
	const FVector A = P(-HW, 0, -HD), B = P(HW, 0, -HD), Cc = P(HW, 0, HD), Dd = P(-HW, 0, HD);
	const FVector Up = Rot3(FVector(0, 1, 0), 0, RY, 0);
	auto Nrm = [&](const FVector& E0, const FVector& E1, const FVector& Top) { const FVector Mid = (E0 + E1) * .5; return ((Mid - C) * FVector(1, 0, 1)).GetSafeNormal() * Rise + Up * (bAlongZ ? HW : HD); };
	if (bAlongZ)
	{
		Quad3(B, Cc, RB, RA, Nrm(B, Cc, RA), Col);
		Quad3(Dd, A, RA, RB, Nrm(Dd, A, RA), Col);
		Tri3(A, B, RA, Nrm(A, B, RA), Col);
		Tri3(Cc, Dd, RB, Nrm(Cc, Dd, RB), Col);
	}
	else
	{
		Quad3(A, B, RB, RA, Nrm(A, B, RA), Col);
		Quad3(Cc, Dd, RA, RB, Nrm(Cc, Dd, RA), Col);
		Tri3(B, Cc, RB, Nrm(B, Cc, RB), Col);
		Tri3(Dd, A, RA, Nrm(Dd, A, RA), Col);
	}
	Quad3(A, B, Cc, Dd, -Up, Col * .7f);
}
