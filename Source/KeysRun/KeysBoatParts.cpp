// Shared boat parts built after the owners' photos: smooth lofted surfaces, swept bars, the big outboards every boat
// carries, and contoured bucket seats.
#include "KeysMesh.h"

void FKeysMesh::AddGrid(const TArray<FVector>& P, int32 NU, int32 NV, bool bWrapU, const FLinearColor& Color, const FVector& Inside)
{
	if (NU < 2 || NV < 2 || P.Num() < NU * NV) return;
	TArray<FVector> Nr;
	Nr.SetNum(NU * NV);
	double Flip = 0;
	for (int32 v = 0; v < NV; v++)
		for (int32 u = 0; u < NU; u++)
		{
			const int32 u0 = bWrapU ? (u + NU - 1) % NU : FMath::Max(0, u - 1), u1 = bWrapU ? (u + 1) % NU : FMath::Min(NU - 1, u + 1);
			const int32 v0 = FMath::Max(0, v - 1), v1 = FMath::Min(NV - 1, v + 1);
			const FVector& Pc = P[v * NU + u];
			FVector Du = P[v * NU + u1] - P[v * NU + u0], Dv = P[v1 * NU + u] - P[v0 * NU + u];
			// a ring collapsed to a point (a dome's pole): take the across-direction from the next ring in
			if (Du.SizeSquared() < 1e-12)
			{
				const int32 vn = v == 0 ? FMath::Min(NV - 1, 1) : FMath::Max(0, v - 1);
				Du = P[vn * NU + u1] - P[vn * NU + u0];
			}
			FVector Nn = FVector::CrossProduct(Du, Dv);
			if (Nn.SizeSquared() < 1e-16) Nn = Pc - Inside;
			Nn = Nn.GetSafeNormal();
			Nr[v * NU + u] = Nn;
			Flip += FVector::DotProduct(Nn, Pc - Inside);
		}
	const int32 Base = V.Num();
	for (int32 i = 0; i < NU * NV; i++) AddVert(P[i], Flip < 0 ? -Nr[i] : Nr[i], Color);
	const int32 NQ = bWrapU ? NU : NU - 1;
	for (int32 v = 0; v < NV - 1; v++)
		for (int32 u = 0; u < NQ; u++)
		{
			const int32 a = Base + v * NU + u, b = Base + v * NU + (u + 1) % NU, c = Base + (v + 1) * NU + (u + 1) % NU, d = Base + (v + 1) * NU + u;
			const FVector Nn = N[a] + N[c] + N[b] + N[d];
			if (FVector::DotProduct(FVector::CrossProduct(V[b] - V[a], V[c] - V[a]) + FVector::CrossProduct(V[c] - V[a], V[d] - V[a]), Nn) > 0) T.Append({a, c, b, a, d, c});
			else T.Append({a, b, c, a, c, d});
		}
}

void FKeysMesh::AddSweep(const TArray<FVector>& Pts, double HalfW, double HalfD, const FVector& Side, const FLinearColor& Color, int32 Sides)
{
	const int32 NP = Pts.Num();
	if (NP < 2) return;
	TArray<FVector> G;
	G.Reserve(NP * Sides);
	FVector Mid = FVector::ZeroVector;
	for (const FVector& Q : Pts) Mid += Q / NP;
	for (int32 i = 0; i < NP; i++)
	{
		const FVector Tg = (Pts[FMath::Min(NP - 1, i + 1)] - Pts[FMath::Max(0, i - 1)]).GetSafeNormal();
		FVector Sd = Side - Tg * FVector::DotProduct(Side, Tg);
		if (Sd.SizeSquared() < 1e-8) Sd = FVector::CrossProduct(Tg, FVector::ZAxisVector);
		Sd.Normalize();
		const FVector Up = FVector::CrossProduct(Tg, Sd).GetSafeNormal();
		for (int32 j = 0; j < Sides; j++)
		{
			const double A = 2 * PI * j / Sides, Cs = FMath::Cos(A), Sn = FMath::Sin(A);
			G.Add(Pts[i] + Sd * (HalfW * FMath::Sign(Cs) * FMath::Pow(FMath::Abs(Cs), .5)) + Up * (HalfD * FMath::Sign(Sn) * FMath::Pow(FMath::Abs(Sn), .5)));
		}
	}
	AddGrid(G, Sides, NP, true, Color, Mid);
	// end caps
	for (int32 End = 0; End <= 1; End++)
	{
		const int32 Row = End ? NP - 1 : 0;
		const FVector Out = (End ? Pts[NP - 1] - Pts[NP - 2] : Pts[0] - Pts[1]).GetSafeNormal();
		for (int32 j = 0; j < Sides; j++) AddTri(Pts[Row], G[Row * Sides + j], G[Row * Sides + (j + 1) % Sides], Color, Out);
	}
}

namespace KeysMesh
{
	static FLinearColor PartSrgb(double R, double G, double B) { return FLinearColor(FColor(uint8(R * 255), uint8(G * 255), uint8(B * 255))); }

	void BuildOutboard(FKeysMesh& M, const FOutboard& O)
	{
		const uint8 Keep = M.Surf;
		const double Xm = O.Mount.X, Y0 = O.Mount.Y, Zm = O.Mount.Z;
		const double A0 = O.W * .5;
		const double Lc = .98 * FMath::Max(.8, O.W / .7);   // the cowl's length fore and aft
		const double Zb = Zm + .08, Zt = O.TopZ, Rd = FMath::Clamp((Zt - Zb) * .18, .08, .18);
		const FVector Inside(Xm - .05 - Lc * .5, Y0, (Zb + Zt) * .5);
		// the cowl: superellipse rings up a body that is narrow at its foot, full two-thirds of the way up, its aft face
		// raked forward toward the top, then rounded over the crown. Th 0 faces the boat (the engine's front), PI aft.
		auto CowlPt = [&](double U, double Dome, double Th, double Grow)
		{
			U = FMath::Clamp(U, 0.0, 1.0);
			const double A = A0 * (.8 + .2 * FMath::Sin(FMath::Min(1.0, U / .72) * PI * .5));
			const double Xf = Xm - .05 - .05 * U, Xr = Xm - .05 - Lc * (1 - .2 * U * U);
			double Cx = (Xf + Xr) * .5;
			const double B = (Xf - Xr) * .5;
			double Z = Zb + (Zt - Rd - Zb) * U, Sc = 1;
			if (Dome > 0)
			{
				const double Ph = FMath::Min(1.0, Dome) * PI * .5;
				Sc = FMath::Pow(FMath::Max(0.0, FMath::Cos(Ph)), .5);
				Z = Zt - Rd + Rd * FMath::Sin(Ph);
				Cx += .04 * Dome;
			}
			const double C = FMath::Cos(Th), S = FMath::Sin(Th), Ex = 2 / 2.8;
			return FVector(Cx + FMath::Sign(C) * FMath::Pow(FMath::Abs(C), Ex) * B * Sc * Grow, Y0 + FMath::Sign(S) * FMath::Pow(FMath::Abs(S), Ex) * A * Sc * Grow, Z + (Grow - 1) * .3 * (Dome > 0 ? 1 : 0));
		};
		auto Patch = [&](double U0, double U1, int32 NU1, double D1, int32 ND1, double Th0, double Th1, int32 NTh, double Grow, bool bWrap, const FLinearColor& Col, uint8 Kind)
		{
			TArray<FVector> G;
			int32 Rows = 0;
			for (int32 r = 0; r < NU1; r++) { const double U = FMath::Lerp(U0, U1, double(r) / FMath::Max(1, NU1 - 1)); for (int32 t = 0; t < NTh; t++) G.Add(CowlPt(U, 0, FMath::Lerp(Th0, Th1, double(t) / (bWrap ? NTh : NTh - 1)), Grow)); Rows++; }
			for (int32 r = 1; r <= ND1; r++) { const double D = D1 * r / ND1; for (int32 t = 0; t < NTh; t++) G.Add(CowlPt(1, D, FMath::Lerp(Th0, Th1, double(t) / (bWrap ? NTh : NTh - 1)), Grow)); Rows++; }
			M.Surf = Kind;
			M.AddGrid(G, NTh, Rows, bWrap, Col, Inside);
		};
		Patch(0, 1, 12, 1, 6, 0, 2 * PI, 32, 1, true, O.Cowl, O.CowlKind);
		// the dark panel down the aft face and over the back of the crown, an accent stripe along each side, and the
		// parting line at the cowl's foot
		const FLinearColor Seam = PartSrgb(.04, .042, .045);
		Patch(.1, 1, 9, .45, 3, PI - .62, PI + .62, 10, 1.008, false, O.Panel, SurfGelcoat);
		Patch(.62, .69, 2, 0, 0, .35, 2.5, 12, 1.007, false, O.Accent, SurfGelcoat);
		Patch(.62, .69, 2, 0, 0, -2.5, -.35, 12, 1.007, false, O.Accent, SurfGelcoat);
		Patch(0, .035, 2, 0, 0, 0, 2 * PI, 32, 1.012, true, Seam, SurfRubber);
		// the midsection under the cowl, waisting in to the leg
		{
			TArray<FVector> G;
			const int32 NTh = 24, NR = 5;
			const double Z0 = Zm - .3;
			for (int32 r = 0; r < NR; r++)
			{
				const double K = double(r) / (NR - 1), Z = FMath::Lerp(Z0, Zb + .03, K);
				const double A = A0 * FMath::Lerp(.5, .8, FMath::Pow(K, .7)), Xf = Xm - .07, Xr = Xm - .07 - FMath::Lerp(.5, .82 * Lc, FMath::Pow(K, .8));
				for (int32 t = 0; t < NTh; t++)
				{
					const double Th = 2 * PI * t / NTh, C = FMath::Cos(Th), S = FMath::Sin(Th);
					G.Add(FVector((Xf + Xr) * .5 + FMath::Sign(C) * FMath::Pow(FMath::Abs(C), .8) * (Xf - Xr) * .5, Y0 + FMath::Sign(S) * FMath::Pow(FMath::Abs(S), .8) * A, Z));
				}
			}
			M.Surf = O.CowlKind;
			M.AddGrid(G, NTh, NR, true, O.Cowl * .9f, FVector(Xm - .4, Y0, Zm));
		}
		// the streamlined driveshaft housing and the gearcase strut
		auto Foil = [&](double Za, double Zb2, double ChordA, double ChordB, double ThA, double ThB, double Front)
		{
			TArray<FVector> G;
			const int32 NTh = 16, NR = 4;
			for (int32 r = 0; r < NR; r++)
			{
				const double K = double(r) / (NR - 1), Z = FMath::Lerp(Za, Zb2, K), Ch = FMath::Lerp(ChordA, ChordB, K), Th = FMath::Lerp(ThA, ThB, K);
				for (int32 t = 0; t < NTh; t++)
				{
					const double A = 2 * PI * t / NTh, C = FMath::Cos(A), S = FMath::Sin(A);
					// a teardrop: round nose forward (toward the boat), fine trailing edge aft
					const double Xc = Front - Ch * .5 + C * Ch * .5;
					const double Thk = Th * (C > 0 ? 1 : FMath::Lerp(1.0, .25, -C));
					G.Add(FVector(Xc, Y0 + S * Thk, Z));
				}
			}
			M.Surf = SurfGelcoat;
			M.AddGrid(G, NTh, NR, true, O.Lower, FVector(Front - ChordA * .4, Y0, (Za + Zb2) * .5));
		};
		const double Zg = O.CavZ - .28;
		Foil(O.CavZ, Zm - .26, .44, .52, .075, .1, Xm - .1);
		Foil(Zg, O.CavZ, .34, .4, .045, .06, Xm - .13);
		M.Surf = SurfGelcoat;
		M.AddRoundBox(FVector(Xm - .42, Y0, O.CavZ), FVector(.64, .36, .028), .013, O.Lower);                         // cavitation plate
		M.AddRoundBox(FVector(Xm - .1, Y0, Zm - .14), FVector(.16, .3, .34), .04, O.Lower);                              // the clamp bracket
		M.AddTube(FVector(Xm - .1, Y0, Zg), FVector(Xm - .6, Y0, Zg), .075, .07, 14, O.Lower);                            // the torpedo
		M.AddTube(FVector(Xm - .01, Y0, Zg), FVector(Xm - .1, Y0, Zg), .012, .075, 14, O.Lower);                          // its nose
		M.AddRoundBox(FVector(Xm - .47, Y0, Zg - .16), FVector(.2, .022, .22), .01, O.Lower, FQuat(FVector::YAxisVector, -.35));   // the skeg
		// the propeller: hub and three cupped blades
		M.Surf = SurfSteel;
		const FLinearColor Prop = PartSrgb(.62, .64, .66);
		const FVector Hc(Xm - .66, Y0, Zg);
		M.AddTube(FVector(Xm - .6, Y0, Zg), FVector(Xm - .74, Y0, Zg), .062, .04, 12, Prop);
		for (int32 k = 0; k < 3; k++)
		{
			const double Ph = k * 2 * PI / 3 + .5;
			const FVector R(0, FMath::Cos(Ph), FMath::Sin(Ph)), Tn(0, -FMath::Sin(Ph), FMath::Cos(Ph)), X(1, 0, 0);
			const FVector A = Hc + R * .055 + X * .04 + Tn * .05, B = Hc + R * .2 + X * .02 + Tn * .1, C = Hc + R * .215 - X * .03 - Tn * .01, D = Hc + R * .055 - X * .05 - Tn * .05;
			M.AddQuad(A, B, C, D, Prop, X + Tn);
		}
		M.Surf = Keep;
	}

	void BuildBucketSeat(FKeysMesh& M, const FVector& Base, const FVector& Fwd, double W, const FLinearColor& Uph, uint8 InsertKind, double BackH)
	{
		const uint8 Keep = M.Surf;
		FVector F = FVector(Fwd.X, Fwd.Y, 0).GetSafeNormal();
		if (F.IsNearlyZero()) F = FVector::XAxisVector;
		const FQuat Q(FVector::ZAxisVector, FMath::Atan2(F.Y, F.X));
		const FLinearColor Pipe = Uph * .25f;
		auto At = [&](const FVector& L) { return Base + Q.RotateVector(L); };
		auto Box = [&](const FVector& L, const FVector& Sz, double R, uint8 Kind, const FQuat& Rl = FQuat::Identity, int32 Seg = 2)
		{
			M.Surf = Kind;
			M.AddRoundBox(At(L), Sz, R, Uph, Q * Rl, Seg);
		};
		// the cushion, its quilted centre and raised side bolsters
		Box(FVector(0, 0, .07), FVector(.52, W, .14), .06, SurfVinyl);
		Box(FVector(.02, 0, .145), FVector(.42, W * .56, .03), .013, InsertKind);
		for (int32 s = -1; s <= 1; s += 2) Box(FVector(0, s * (W * .5 - W * .11), .16), FVector(.48, W * .21, .09), .04, SurfVinyl);
		// the back: tilted back from the rear of the cushion, a rounded top, a quilted panel and wings wrapping forward
		const FVector Pv(-.22, 0, .12);
		const FQuat Tilt(FVector::YAxisVector, -.2);
		auto Bk = [&](const FVector& L) { return Pv + Tilt.RotateVector(L); };
		Box(Bk(FVector(-.05, 0, BackH * .5)), FVector(.13, W * .96, BackH), FMath::Min(.2, W * .3), SurfVinyl, Tilt, 4);
		Box(Bk(FVector(.025, 0, BackH * .47)), FVector(.03, W * .5, BackH * .6), .012, InsertKind, Tilt);
		for (int32 s = -1; s <= 1; s += 2)
			Box(Bk(FVector(.03, s * (W * .5 - W * .1), BackH * .38)), FVector(.12, W * .18, BackH * .7), .05, SurfVinyl, FQuat(FVector::ZAxisVector, -s * .35) * Tilt);
		// dark piping along the panels' edges
		M.Surf = SurfVinyl;
		for (int32 s = -1; s <= 1; s += 2)
		{
			M.AddTube(At(Bk(FVector(.042, s * W * .255, BackH * .16))), At(Bk(FVector(.042, s * W * .255, BackH * .78))), .006, .006, 6, Pipe);
			M.AddTube(At(FVector(-.2, s * W * .285, .162)), At(FVector(.23, s * W * .285, .162)), .006, .006, 6, Pipe);
		}
		M.Surf = Keep;
	}
}
