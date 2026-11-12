#include "KeysMesh.h"
#include "KeysData.h"

int32 FKeysMesh::AddVert(const FVector& PosM, const FVector& Normal, const FLinearColor& Color)
{
	const int32 I = V.Add(PosM * 100.0);
	N.Add(Normal);
	UV.Add(FVector2D(PosM.X, PosM.Y));
	C.Add(Color);
	return I;
}

void FKeysMesh::AddQuad(const FVector& A, const FVector& B, const FVector& Cc, const FVector& D, const FLinearColor& Color, const FVector& Out)
{
	FVector Nrm = FVector::CrossProduct(Cc - A, B - D).GetSafeNormal();
	if (Nrm.IsNearlyZero()) Nrm = Out.GetSafeNormal();
	if (FVector::DotProduct(Nrm, Out) < 0) Nrm = -Nrm;
	const int32 I0 = AddVert(A, Nrm, Color), I1 = AddVert(B, Nrm, Color), I2 = AddVert(Cc, Nrm, Color), I3 = AddVert(D, Nrm, Color);
	T.Append({I0, I1, I2, I0, I2, I3});
}

void FKeysMesh::AddTri(const FVector& A, const FVector& B, const FVector& Cc, const FLinearColor& Color, const FVector& Out)
{
	FVector Nrm = FVector::CrossProduct(B - A, Cc - A).GetSafeNormal();
	if (Nrm.IsNearlyZero()) Nrm = Out.GetSafeNormal();
	if (FVector::DotProduct(Nrm, Out) < 0) Nrm = -Nrm;
	const int32 I0 = AddVert(A, Nrm, Color), I1 = AddVert(B, Nrm, Color), I2 = AddVert(Cc, Nrm, Color);
	T.Append({I0, I1, I2});
}

void FKeysMesh::AddBox(const FVector& Ctr, const FVector& Size, const FLinearColor& Color, const FQuat& Rot)
{
	const FVector H = Size * .5;
	auto P = [&](double X, double Y, double Z) { return Ctr + Rot.RotateVector(FVector(X * H.X, Y * H.Y, Z * H.Z)); };
	const FVector Ax = Rot.RotateVector(FVector::XAxisVector), Ay = Rot.RotateVector(FVector::YAxisVector), Az = Rot.RotateVector(FVector::ZAxisVector);
	AddQuad(P(1, -1, -1), P(1, 1, -1), P(1, 1, 1), P(1, -1, 1), Color, Ax);
	AddQuad(P(-1, -1, -1), P(-1, -1, 1), P(-1, 1, 1), P(-1, 1, -1), Color, -Ax);
	AddQuad(P(-1, 1, -1), P(-1, 1, 1), P(1, 1, 1), P(1, 1, -1), Color, Ay);
	AddQuad(P(-1, -1, -1), P(1, -1, -1), P(1, -1, 1), P(-1, -1, 1), Color, -Ay);
	AddQuad(P(-1, -1, 1), P(1, -1, 1), P(1, 1, 1), P(-1, 1, 1), Color, Az);
	AddQuad(P(-1, -1, -1), P(-1, 1, -1), P(1, 1, -1), P(1, -1, -1), Color, -Az);
}

void FKeysMesh::AddTube(const FVector& A, const FVector& B, double RA, double RB, int32 Sides, const FLinearColor& Color)
{
	const FVector Dir = (B - A).GetSafeNormal();
	if (Dir.IsNearlyZero()) return;
	FVector U = FVector::CrossProduct(Dir, FVector::ZAxisVector);
	if (U.SizeSquared() < 1e-6) U = FVector::CrossProduct(Dir, FVector::XAxisVector);
	U.Normalize();
	const FVector W = FVector::CrossProduct(Dir, U);
	for (int32 i = 0; i < Sides; i++)
	{
		const double A0 = 2 * PI * i / Sides, A1 = 2 * PI * (i + 1) / Sides;
		const FVector O0 = U * FMath::Cos(A0) + W * FMath::Sin(A0), O1 = U * FMath::Cos(A1) + W * FMath::Sin(A1);
		AddQuad(A + O0 * RA, B + O0 * RB, B + O1 * RB, A + O1 * RA, Color, (O0 + O1) * .5);
	}
}

void FKeysMesh::Commit(UProceduralMeshComponent* Mesh, int32 Section, UMaterialInterface* Mat) const
{
	if (!Mesh) return;
	Mesh->CreateMeshSection_LinearColor(Section, V, T, N, UV, C, TArray<FProcMeshTangent>(), false);
	if (Mat) Mesh->SetMaterial(Section, Mat);
}

namespace KeysMesh
{
	static FLinearColor Srgb(double R, double G, double B) { return FLinearColor(FColor(uint8(R * 255), uint8(G * 255), uint8(B * 255))); }

	bool TowerEye(const KeysData::FBoatSpec& S, FVector& OutEye)
	{
		if (S.Id != TEXT("grady") && S.Id != TEXT("freeman")) return false;
		// the same numbers BuildBoat uses for the console, top and tower
		const double L = S.Len, Free = .72 + .028 * L, DeckZ = Free - .42, MidX = .05 * L, TopZ = DeckZ + 2.35;
		const double TY = TopZ + .05 + (S.Id == TEXT("freeman") ? 3.3 : 2.5);
		OutEye = FVector(MidX - .5 - 1.05 * .12, 0, TY + .03 + 1.62);
		return true;
	}

	void BuildBoat(FKeysMesh& M, const KeysData::FBoatSpec& S)
	{
		const double L = S.Len, HB = S.Beam * .5, Free = .72 + .028 * L, Draft = .38 + .025 * L;
		const int32 NS = 22;
		const FLinearColor Liner = S.Deck * 1.08f, Cap = S.Hull, Dark = Srgb(.12, .13, .15), Glass = Srgb(.16, .22, .27);

		struct FSt { double X, HB, ZK, ZC, YC, ZS, ZW1, ZW2; };
		TArray<FSt> St;
		for (int32 i = 0; i <= NS; i++)
		{
			const double T = double(i) / NS;
			FSt P;
			P.X = (T - .5) * L;
			const double Taper = T < .5 ? 1 : FMath::Sqrt(FMath::Max(0.0, 1 - FMath::Square((T - .5) / .5)));
			P.HB = FMath::Max(.02, HB * Taper);
			P.ZS = Free + .28 * T * T;
			const double Rise = FMath::Square(FMath::Max(0.0, (T - .6) / .4));
			P.ZK = FMath::Lerp(-Draft, P.ZS - .12, Rise);
			P.YC = P.HB * .84;
			P.ZC = FMath::Min(P.ZK + P.YC * .36, P.ZS - .1);
			P.ZW1 = FMath::Min(FMath::Max(.04, P.ZC + .01), P.ZS - .2);
			P.ZW2 = FMath::Min(P.ZW1 + .16, P.ZS - .05);
			St.Add(P);
		}
		// y on the topsides at height z: straight flare from the chine (y = YC + 8%) up to the sheer
		auto TopY = [](const FSt& P, double Z) { const double U = FMath::Clamp((Z - P.ZC) / FMath::Max(.01, P.ZS - P.ZC), 0.0, 1.0); return FMath::Lerp(P.HB * .92, P.HB, U); };

		for (int32 Side = -1; Side <= 1; Side += 2)
		{
			for (int32 i = 0; i < NS; i++)
			{
				const FSt& A = St[i];
				const FSt& B = St[i + 1];
				const FVector OutBottom(0, Side * .6, -1), OutSide(0, Side, 0);
				auto Seg = [&](double YA0, double ZA0, double YA1, double ZA1, double YB0, double ZB0, double YB1, double ZB1, const FLinearColor& Col, const FVector& Out)
				{
					M.AddQuad(FVector(A.X, Side * YA0, ZA0), FVector(B.X, Side * YB0, ZB0), FVector(B.X, Side * YB1, ZB1), FVector(A.X, Side * YA1, ZA1), Col, Out);
				};
				Seg(0, A.ZK, A.YC, A.ZC, 0, B.ZK, B.YC, B.ZC, S.Bottom, OutBottom);
				Seg(A.YC, A.ZC, A.HB * .92, A.ZC + .02, B.YC, B.ZC, B.HB * .92, B.ZC + .02, S.Bottom, OutBottom);
				Seg(A.HB * .92, A.ZC + .02, TopY(A, A.ZW1), A.ZW1, B.HB * .92, B.ZC + .02, TopY(B, B.ZW1), B.ZW1, S.Bottom, OutSide);
				Seg(TopY(A, A.ZW1), A.ZW1, TopY(A, A.ZW2), A.ZW2, TopY(B, B.ZW1), B.ZW1, TopY(B, B.ZW2), B.ZW2, S.Stripe, OutSide);
				Seg(TopY(A, A.ZW2), A.ZW2, A.HB, A.ZS, TopY(B, B.ZW2), B.ZW2, B.HB, B.ZS, S.Hull, OutSide);
				// gunwale cap and inner liner down to the deck
				const double WA = FMath::Min(.14, A.HB * .5), WB = FMath::Min(.14, B.HB * .5);
				Seg(A.HB, A.ZS, A.HB - WA, A.ZS, B.HB, B.ZS, B.HB - WB, B.ZS, Cap, FVector::ZAxisVector);
				const double DA = A.ZS - .42, DB = B.ZS - .42;
				Seg(A.HB - WA, A.ZS, A.HB - WA, DA, B.HB - WB, B.ZS, B.HB - WB, DB, Liner, FVector(0, -Side, 0));
				// deck
				Seg(A.HB - WA, DA, 0, DA, B.HB - WB, DB, 0, DB, S.Deck, FVector::ZAxisVector);
			}
		}
		// transom
		{
			const FSt& P = St[0];
			const FVector Back(-1, 0, 0);
			for (int32 Side = -1; Side <= 1; Side += 2)
			{
				M.AddQuad(FVector(P.X, 0, P.ZK), FVector(P.X, Side * P.YC, P.ZC), FVector(P.X, Side * P.HB, P.ZS), FVector(P.X, 0, P.ZS), S.Hull, Back);
			}
		}

		const double DeckZ = Free - .42, MidX = .05 * L;
		// console, windshield, leaning post or helm seat
		M.AddBox(FVector(MidX, 0, DeckZ + .6), FVector(1.05, 1.15, 1.2), S.Hull);
		M.AddBox(FVector(MidX + .45, 0, DeckZ + 1.32), FVector(.06, 1.05, .3), Glass, FQuat(FVector::YAxisVector, -.5));
		M.AddBox(FVector(MidX - .05, 0, DeckZ + 1.21), FVector(.5, .7, .04), Dark);
		M.AddBox(FVector(MidX - 1.15, 0, DeckZ + .5), FVector(.8, 1.05, 1.0), S.Hull);
		M.AddBox(FVector(MidX - 1.15, 0, DeckZ + 1.04), FVector(.85, 1.1, .1), S.Stripe);
		// bow casting deck and stern bench
		M.AddBox(FVector(L * .36, 0, DeckZ + .18), FVector(L * .14, HB * 1.1, .36), S.Deck);
		M.AddBox(FVector(-L * .43, 0, DeckZ + .25), FVector(L * .08, HB * 1.6, .5), S.Hull);

		// T-top or hardtop on four posts
		const double TopZ = DeckZ + 2.35, TopL = FMath::Min(2.6, L * .26), TopW = FMath::Min(2.2, S.Beam * .66);
		for (int32 a = -1; a <= 1; a += 2)
			for (int32 b = -1; b <= 1; b += 2)
				M.AddTube(FVector(MidX - .5 + a * TopL * .38, b * TopW * .4, DeckZ), FVector(MidX - .5 + a * TopL * .38, b * TopW * .4, TopZ), .035, .035, 6, Srgb(.85, .86, .88));
		M.AddBox(FVector(MidX - .5, 0, TopZ + .05), FVector(TopL, TopW, S.bHardtop ? .12 : .05), S.Canvas);
		if (S.bHardtop) M.AddBox(FVector(MidX - .5, 0, TopZ - .02), FVector(TopL * .9, TopW * .9, .04), Srgb(.9, .9, .9));

		// Tuna tower with an upper helm on the two offshore fishing boats, ported from the web game's boat model
		// (apps/client/src/entities/boat/model.ts). The Freeman gets the big one: taller, with a black visor,
		// a radar dome and long white outriggers.
		if (S.Id == TEXT("grady") || S.Id == TEXT("freeman"))
		{
			const bool bBig = S.Id == TEXT("freeman");
			const double TC = MidX - .5, TopH = TopZ + .05, TW = TopW, TL = TopL;
			const double TY = TopH + (bBig ? 3.3 : 2.5), W2 = FMath::Min(1.15, TW * .5), L2 = 1.05;
			const FLinearColor Fr = bBig ? Srgb(.08, .09, .1) : Srgb(.79, .81, .83);
			const FLinearColor TopC = bBig ? Srgb(.97, .97, .96) : S.Canvas, Uph = Srgb(.6, .63, .65), White = Srgb(.95, .95, .95);
			// web frame (x across, y up, z toward the stern from the top's center) -> this boat's frame
			auto P = [TC](double X, double Y, double Z) { return FVector(TC - Z, X, Y); };
			auto Bar = [&M](const FVector& A, const FVector& B, double R, const FLinearColor& C) { M.AddTube(A, B, R, R, 6, C); };
			// four legs from the top up to the platform
			for (int32 sx = -1; sx <= 1; sx += 2)
				for (int32 sz = -1; sz <= 1; sz += 2)
					Bar(P(sx * TW * .38, TopH + .08, sz * TL * .36), P(sx * W2 * .5, TY, sz * L2 * .45), .04, Fr);
			// platform, rails around it
			M.AddBox(P(0, TY, 0), FVector(L2 + .12, W2 + .12, .06), TopC);
			for (int32 sx = -1; sx <= 1; sx += 2)
			{
				Bar(P(sx * W2 * .55, TY + .9, -L2 * .5), P(sx * W2 * .55, TY + .9, L2 * .5), .025, Fr);
				for (int32 sz = -1; sz <= 1; sz += 2) Bar(P(sx * W2 * .55, TY, sz * L2 * .5), P(sx * W2 * .55, TY + .9, sz * L2 * .5), .025, Fr);
			}
			Bar(P(-W2 * .55, TY + .9, -L2 * .5), P(W2 * .55, TY + .9, -L2 * .5), .025, Fr);
			// upper helm: console, screen, wheel, seat
			M.AddBox(P(0, TY + .39, -L2 * .3), FVector(.3, .56, .72), Liner);
			M.AddBox(P(0, TY + .74, -L2 * .3 + .05), FVector(.2, .46, .02), Dark, FQuat(FVector::YAxisVector, -.9));
			{
				const FVector WC = P(0, TY + .82, -L2 * .3 + .24);
				const FVector Ax(FMath::Sin(.6), 0, FMath::Cos(.6));   // wheel tilted back toward the driver
				const FVector U = FVector::CrossProduct(Ax, FVector::YAxisVector).GetSafeNormal(), V = FVector::YAxisVector;
				FVector Prev = WC + U * .16;
				for (int32 k = 1; k <= 12; k++)
				{
					const double A = 2 * PI * k / 12;
					const FVector Q = WC + (U * FMath::Cos(A) + V * FMath::Sin(A)) * .16;
					Bar(Prev, Q, .022, Dark);
					Prev = Q;
				}
			}
			M.AddBox(P(0, TY + .78, L2 * .3), FVector(.3, .6, .16), Uph);
			M.AddBox(P(0, TY + 1.02, L2 * .45), FVector(.08, .6, .4), Uph);
			// upper top on its own posts
			M.AddBox(P(0, TY + 2.05, 0), FVector(L2 + .5, W2 + .4, .05), TopC);
			for (int32 sx = -1; sx <= 1; sx += 2)
				for (int32 sz = -1; sz <= 1; sz += 2)
					Bar(P(sx * W2 * .5, TY, sz * L2 * .45), P(sx * (W2 * .5 + .1), TY + 2.03, sz * (L2 * .5 + .15)), .025, Fr);
			// ladder rungs up the aft legs
			for (int32 k = 1; k < 6; k++)
			{
				const double Y = TopH + k * .42, ZZ = TL * .36 + (L2 * .45 - TL * .36) * k / 6, XX = FMath::Lerp(TW * .38, W2 * .5, k / 6.0);
				Bar(P(-XX, Y, ZZ), P(XX, Y, ZZ), .02, Fr);
			}
			if (bBig)
			{
				M.AddBox(P(0, TY + 1.98, -(L2 + .5) / 2), FVector(.06, W2 + .42, .12), Dark);   // black visor
				M.AddBox(P(0, TY + 2.0, 0), FVector(L2 + .52, W2 + .42, .05), Dark);
				Bar(P(-W2 * .55, TY - 1.3, -L2 * .45), P(W2 * .55, TY - 1.3, -L2 * .45), .03, Fr);
				M.AddBox(P(0, TY - 1.35, -L2 * .45), FVector(.12, W2 * 1.1, .18), White);
				M.AddTube(P(0, TY + 2.06, 0), P(0, TY + 2.32, 0), .34, .2, 14, White);           // radar dome
				for (int32 sx = -1; sx <= 1; sx += 2)
				{
					// long white outriggers, and the whip antennas aft
					M.AddTube(P(sx * W2 * .5, TY - 1.6, .1), P(sx * (W2 * .5 + 1.6), TY + 7.2, 2.6), .05, .03, 6, White);
					Bar(P(sx * W2 * .52, TY - 1.6, .1), P(sx * W2 * .52, TY - .6, .1), .07, White);
					Bar(P(sx * TW * .3, TopH + .12, TL * .42), P(sx * TW * .3, TopH + 5.4, TL * .42 + .4), .025, White);
				}
			}
			else
			{
				// outriggers on the tower, raked out and aft
				const double RigLen = FMath::Max(5.5, L * .78);
				for (int32 sx = -1; sx <= 1; sx += 2)
				{
					const FVector Base = P(sx * S.Beam * .22, TY + .38, L2 * .12 + .1);
					const FVector Dir = FVector(-FMath::Sin(.18), sx * FMath::Sin(.55), FMath::Cos(.55)).GetSafeNormal();
					M.AddTube(Base, Base + Dir * RigLen, .085, .035, 7, Srgb(.87, .89, .9));
					M.AddTube(Base - FVector(0, 0, .13), Base + FVector(0, 0, .13), .12, .12, 8, Srgb(.55, .58, .59));
				}
			}
		}

		// outboards hung off the transom
		const double X0 = St[0].X - .35;
		for (int32 e = 0; e < S.Engines; e++)
		{
			const double Y = (e - (S.Engines - 1) * .5) * .74;
			M.AddBox(FVector(X0 - .05, Y, Free - .15), FVector(.62, .5, .78), S.Engine);
			M.AddBox(FVector(X0 + .05, Y, Free * .3 - .25), FVector(.32, .2, Free * .9 + .5), Dark);
			M.AddBox(FVector(X0 + .05, Y, -Draft - .05), FVector(.42, .12, .14), Dark);
			M.AddBox(FVector(X0 - .05, Y, Free + .26), FVector(.5, .4, .08), S.Stripe);
		}
		// rod holders along the gunwale aft
		for (int32 Side = -1; Side <= 1; Side += 2)
			for (int32 k = 0; k < 3; k++)
			{
				const double X = -L * .32 + k * .45;
				M.AddTube(FVector(X, Side * (HB - .1), Free + .25), FVector(X - .2, Side * (HB + .05), Free + 2.2), .012, .006, 4, Dark);
			}
	}

	double FishLength(double WeightLb) { return FMath::Clamp(.27 * FMath::Pow(FMath::Max(.3, WeightLb), 1.0 / 3.0), .22, 4.2); }

	void BuildFish(FKeysMesh& M, double Len, const FLinearColor& Color, bool bBill)
	{
		const int32 NR = 12, NA = 10;
		const double HMax = .13 * Len;
		const FLinearColor Belly = FMath::Lerp(Color, FLinearColor(.85f, .87f, .88f), .65f);
		auto Ring = [&](int32 i, int32 j)
		{
			const double U = double(i) / NR;
			const double X = (U - .55) * Len;
			const double R = HMax * FMath::Pow(FMath::Sin(PI * (.06 + .9 * U)), .85) * (U < .15 ? .55 + 3 * U : 1);
			const double A = 2 * PI * j / NA;
			return FVector(X, FMath::Cos(A) * R * .42, FMath::Sin(A) * R);
		};
		for (int32 i = 0; i < NR; i++)
			for (int32 j = 0; j < NA; j++)
			{
				const FVector A = Ring(i, j), B = Ring(i + 1, j), Cc = Ring(i + 1, j + 1), D = Ring(i, j + 1);
				const FVector Mid = (A + Cc) * .5;
				const FLinearColor Col = Mid.Z > 0 ? Color : Belly;
				M.AddQuad(A, B, Cc, D, Col, FVector(0, Mid.Y, Mid.Z));
			}
		// nose cap
		const FVector Nose((.45 + .03) * Len, 0, 0);
		for (int32 j = 0; j < NA; j++) M.AddTri(Ring(NR, j), Nose, Ring(NR, j + 1), Color, FVector::XAxisVector);
		// forked tail
		const double TX = -.55 * Len;
		const FLinearColor Fin = Color * .8f;
		M.AddTri(FVector(TX + .04 * Len, 0, 0), FVector(TX - .16 * Len, 0, .17 * Len), FVector(TX - .1 * Len, 0, .02 * Len), Fin, FVector::YAxisVector);
		M.AddTri(FVector(TX + .04 * Len, 0, 0), FVector(TX - .1 * Len, 0, -.02 * Len), FVector(TX - .16 * Len, 0, -.17 * Len), Fin, FVector::YAxisVector);
		// dorsal
		M.AddTri(FVector(.1 * Len, 0, HMax * .9), FVector(-.12 * Len, 0, HMax * .9), FVector(-.02 * Len, 0, HMax * (bBill ? 2.6 : 1.7)), Fin, FVector::YAxisVector);
		if (bBill) M.AddTube(Nose, Nose + FVector(.25 * Len, 0, .01 * Len), .012 * Len, .002, 5, Color * .6f);
	}
}

void FKeysMesh::SmoothNormals(double MaxAngleDeg)
{
	const double CosMax = FMath::Cos(FMath::DegreesToRadians(MaxAngleDeg));
	// group vertices by position (to a tenth of a millimeter; positions are centimeters)
	TMap<FIntVector, TArray<int32>> Groups;
	Groups.Reserve(V.Num());
	for (int32 i = 0; i < V.Num(); i++)
		Groups.FindOrAdd(FIntVector(FMath::RoundToInt(V[i].X * 100), FMath::RoundToInt(V[i].Y * 100), FMath::RoundToInt(V[i].Z * 100))).Add(i);
	TArray<FVector> Out = N;
	for (const auto& KV : Groups)
	{
		const TArray<int32>& G = KV.Value;
		if (G.Num() < 2) continue;
		for (const int32 i : G)
		{
			FVector Sum = FVector::ZeroVector;
			for (const int32 j : G) if (FVector::DotProduct(N[i], N[j]) >= CosMax) Sum += N[j];
			Out[i] = Sum.GetSafeNormal(UE_SMALL_NUMBER, N[i]);
		}
	}
	N = MoveTemp(Out);
}
