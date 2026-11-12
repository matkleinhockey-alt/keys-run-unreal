#include "KeysHuman.h"
#include "KeysMesh.h"

namespace KeysShapes
{
	void AddBlob(FKeysMesh& M, const FVector& Center, const FVector& Radii, const FQuat& Rot, const FLinearColor& Color, int32 NU, int32 NV, double VMin)
	{
		// a smooth ellipsoid: shared vertices with the true surface normal, so a handful of segments still read as round
		NU = FMath::Max(3, NU + 2);
		NV = FMath::Max(2, NV + 1);
		const double A0 = FMath::Clamp(VMin, -1.0, 1.0) * PI * .5, A1 = PI * .5;
		const FVector R2(FMath::Max(1e-6, Radii.X * Radii.X), FMath::Max(1e-6, Radii.Y * Radii.Y), FMath::Max(1e-6, Radii.Z * Radii.Z));
		const int32 Base = M.V.Num();
		for (int32 j = 0; j <= NV; j++)
			for (int32 i = 0; i < NU; i++)
			{
				const double Lon = 2 * PI * i / NU, Lat = FMath::Lerp(A0, A1, double(j) / NV);
				const FVector L(FMath::Cos(Lat) * FMath::Cos(Lon) * Radii.X, FMath::Cos(Lat) * FMath::Sin(Lon) * Radii.Y, FMath::Sin(Lat) * Radii.Z);
				FVector Nl(L.X / R2.X, L.Y / R2.Y, L.Z / R2.Z);
				if (j == NV) Nl = FVector::ZAxisVector;
				if (j == 0 && VMin <= -1) Nl = -FVector::ZAxisVector;
				M.AddVert(Center + Rot.RotateVector(L), Rot.RotateVector(Nl.GetSafeNormal()), Color);
			}
		for (int32 j = 0; j < NV; j++)
			for (int32 i = 0; i < NU; i++)
			{
				const int32 a = Base + j * NU + i, b = Base + j * NU + (i + 1) % NU, c = Base + (j + 1) * NU + (i + 1) % NU, d = Base + (j + 1) * NU + i;
				// Unreal's front face: Cross(B - A, C - A) against the normal
				const FVector Nn = M.N[a] + M.N[c];
				if (FVector::DotProduct(FVector::CrossProduct(M.V[b] - M.V[a], M.V[c] - M.V[a]), Nn) > 0) M.T.Append({a, c, b, a, d, c});
				else M.T.Append({a, b, c, a, c, d});
			}
	}

	void SolveIK(const FVector& S, const FVector& Target, double L1, double L2, const FVector& Pole, FVector& OutElbow, FVector& OutEnd)
	{
		const FVector D = Target - S;
		double Len = FMath::Min(D.Size(), L1 + L2 - .003);
		if (Len < .01) Len = .01;
		FVector Dir = D.GetSafeNormal();
		if (Dir.IsNearlyZero()) Dir = FVector(0, 0, -1);
		const double A1 = (L1 * L1 - L2 * L2 + Len * Len) / (2 * Len);
		const double HH = FMath::Sqrt(FMath::Max(0.0, L1 * L1 - A1 * A1));
		FVector PP = Pole - Dir * FVector::DotProduct(Pole, Dir);
		PP = PP.GetSafeNormal();
		OutElbow = S + Dir * A1 + PP * HH;
		OutEnd = S + Dir * Len;
	}
}

using namespace KeysShapes;

namespace
{
	// A smooth surface from a grid of points: NU around (wrapping), NV rows. Normals come from the grid itself
	// (finite differences), so a deformed sphere, a lathe or a sweep all shade smoothly with one routine. A row whose
	// points all coincide is a pole: its normal points along the axis, away from the next row.
	struct FGrid
	{
		int32 NU = 0;
		bool bWrap = true;
		TArray<FVector> P;
		TArray<FLinearColor> Col;
		TArray<uint8> Kind;
		int32 Rows() const { return NU ? P.Num() / NU : 0; }
		void Add(const FVector& At, const FLinearColor& C, uint8 K) { P.Add(At); Col.Add(C); Kind.Add(K); }
	};

	void Emit(FKeysMesh& M, const FGrid& G)
	{
		const int32 NU = G.NU, NV = G.Rows();
		if (NU < 2 || NV < 2) return;
		auto At = [&](int32 u, int32 v) -> const FVector&
		{
			u = G.bWrap ? (u + NU) % NU : FMath::Clamp(u, 0, NU - 1);
			return G.P[FMath::Clamp(v, 0, NV - 1) * NU + u];
		};
		TArray<FVector> Ctr;
		Ctr.SetNum(NV);
		for (int32 v = 0; v < NV; v++)
		{
			FVector S = FVector::ZeroVector;
			for (int32 u = 0; u < NU; u++) S += At(u, v);
			Ctr[v] = S / NU;
		}
		const int32 Base = M.V.Num();
		const uint8 Keep = M.Surf;
		for (int32 v = 0; v < NV; v++)
			for (int32 u = 0; u < NU; u++)
			{
				const FVector& Pt = At(u, v);
				const FVector DU = At(u + 1, v) - At(u - 1, v);
				// skip over coincident rows (a pole, or a doubled row at a colour seam)
				int32 v0 = v - 1, v1 = v + 1;
				while (v0 > 0 && (At(u, v0) - Pt).SizeSquared() < 1e-12) v0--;
				while (v1 < NV - 1 && (At(u, v1) - Pt).SizeSquared() < 1e-12) v1++;
				const FVector DV = At(u, v1) - At(u, v0);
				FVector Nn = FVector::CrossProduct(DU, DV);
				const FVector Out = Pt - Ctr[v];
				if (DU.SizeSquared() < 1e-12 || Nn.SizeSquared() < 1e-14)
				{
					// a pole: along the axis, away from the neighbouring row
					const FVector Nb = v == 0 ? Ctr[FMath::Min(1, NV - 1)] : Ctr[v - 1];
					Nn = (Pt - Nb);
				}
				else if (FVector::DotProduct(Nn, Out) < 0) Nn = -Nn;
				if (!G.bWrap && Out.SizeSquared() < 1e-10) Nn = FVector::CrossProduct(DU, DV);
				M.Surf = G.Kind[v * NU + u];
				M.AddVert(Pt, Nn.GetSafeNormal(UE_SMALL_NUMBER, FVector::ZAxisVector), G.Col[v * NU + u]);
			}
		M.Surf = Keep;
		const int32 NC = G.bWrap ? NU : NU - 1;
		for (int32 v = 0; v < NV - 1; v++)
			for (int32 u = 0; u < NC; u++)
			{
				const int32 a = Base + v * NU + u, b = Base + v * NU + (u + 1) % NU, c = Base + (v + 1) * NU + (u + 1) % NU, d = Base + (v + 1) * NU + u;
				const FVector Nn = M.N[a] + M.N[b] + M.N[c] + M.N[d];
				const bool bFlip = FVector::DotProduct(FVector::CrossProduct(M.V[b] - M.V[a], M.V[c] - M.V[a]) + FVector::CrossProduct(M.V[c] - M.V[a], M.V[d] - M.V[a]), Nn) > 0;
				if (bFlip) M.T.Append({a, c, b, a, d, c});
				else M.T.Append({a, b, c, a, c, d});
			}
	}

	// A sweep along a path of ring centres with elliptical rings (RA across the Up side, RB across the other), the frame
	// carried along without twisting. Closed off with a rounded end where CapA / CapB.
	struct FRing { FVector C; double RA, RB; FLinearColor Col; uint8 Kind; };

	void Sweep(FKeysMesh& M, const TArray<FRing>& R, int32 NU, const FVector& UpHint, bool bCapA, bool bCapB)
	{
		const int32 N = R.Num();
		if (N < 2) return;
		TArray<FRing> Rs;
		auto Tangent = [&](int32 i) { return (R[FMath::Min(i + 1, N - 1)].C - R[FMath::Max(i - 1, 0)].C).GetSafeNormal(UE_SMALL_NUMBER, FVector::ZAxisVector); };
		// rounded ends: rings shrinking on a quarter circle past the end, closed by a pole
		auto CapRings = [&](int32 i, double Sgn, TArray<FRing>& Out)
		{
			const FVector T = Tangent(i) * Sgn;
			const double Rm = (R[i].RA + R[i].RB) * .5;
			const double Ang[3] = {.55, 1.05, PI * .5};
			for (int32 k = 0; k < 3; k++)
			{
				FRing Q = R[i];
				const double Sc = FMath::Cos(Ang[k]);
				Q.C = R[i].C + T * FMath::Sin(Ang[k]) * Rm * .85;
				Q.RA *= Sc; Q.RB *= Sc;
				if (k == 2) Q.RA = Q.RB = 0;
				Out.Add(Q);
			}
		};
		if (bCapA) { TArray<FRing> C; CapRings(0, -1, C); for (int32 k = C.Num() - 1; k >= 0; k--) Rs.Add(C[k]); }
		Rs.Append(R);
		if (bCapB) CapRings(N - 1, 1, Rs);

		FGrid G;
		G.NU = NU;
		FVector Prev = UpHint;
		for (int32 i = 0; i < Rs.Num(); i++)
		{
			// tangent from the swept rings (cap rings included)
			const FVector T = (Rs[FMath::Min(i + 1, Rs.Num() - 1)].C - Rs[FMath::Max(i - 1, 0)].C).GetSafeNormal(UE_SMALL_NUMBER, FVector::ZAxisVector);
			FVector U = Prev - T * FVector::DotProduct(Prev, T);
			if (U.SizeSquared() < 1e-8) U = FVector::CrossProduct(T, FMath::Abs(T.Z) < .9 ? FVector::ZAxisVector : FVector::XAxisVector);
			U.Normalize();
			Prev = U;
			const FVector W = FVector::CrossProduct(T, U);
			for (int32 k = 0; k < NU; k++)
			{
				const double A = 2 * PI * k / NU;
				G.Add(Rs[i].C + U * (FMath::Cos(A) * Rs[i].RA) + W * (FMath::Sin(A) * Rs[i].RB), Rs[i].Col, Rs[i].Kind);
			}
		}
		Emit(M, G);
	}

	FLinearColor Mix(const FLinearColor& A, const FLinearColor& B, double K) { return FMath::Lerp(A, B, float(FMath::Clamp(K, 0.0, 1.0))); }
}

void FKeysHuman::Init(const FKeysHumanLook& InLook, bool bInSeated)
{
	Look = InLook;
	bSeated = bInSeated;
	HipY = bSeated ? .48 : (Look.bBikini ? .96 : .93);
	Ph = FMath::FRand() * 10;
	SipT = FMath::FRandRange(.6, 2.0);
	DefaultPose();
}

void FKeysHuman::DefaultPose()
{
	PoseLegs(J(-.105, .085, 0), J(.105, .085, 0));
	if (bSeated) Pose(J(-.16, HipY + .08, .34), J(.16, HipY + .08, .34));
	else Pose(J(-.24, HipY + .02, .05), J(.24, HipY + .02, .05));
}

void FKeysHuman::IdleUpdate(double T, double BoatSpeedMs)
{
	HeadYaw = FMath::Sin(T * .27 + Ph) * .35 + FMath::Sin(T * .71 + Ph * 2) * .1;
	if (!bHoldHead) HeadNod = FMath::Sin(T * .4 + Ph) * .05;
	Wind = FMath::Min(1.0, FMath::Abs(BoatSpeedMs) / 22);
}

FTransform FKeysHuman::ToBoat() const
{
	return FTransform(FRotator(FMath::RadiansToDegrees(Pitch), FMath::RadiansToDegrees(Yaw), FMath::RadiansToDegrees(Roll)), Pos);
}

// A smooth person: a lathed torso with chest, waist and hips, swept limbs that bend at the knee and elbow without seams,
// mitten hands with a thumb, shoes, a sculpted head (brow, eye sockets, cheeks, jaw and chin) with eyes, nose, lips,
// ears and brows, and hair as a shell over the skull. Skin, cloth and hair are tagged for the surface material.
void FKeysHuman::Build(FKeysMesh& M) const
{
	if (!bVisible) return;
	const FTransform X = ToBoat();
	const FQuat Q = X.GetRotation();
	auto W = [&X](const FVector& P) { return X.TransformPosition(P); };
	const bool bFem = Look.bBikini;
	const FLinearColor Skin = Look.Skin, Bottom = Look.Bottom, HairC = Look.Hair;
	const FLinearColor Top = bFem ? Skin : Look.Shirt;
	const uint8 TopK = bFem ? SurfSkin : SurfCloth;
	const FLinearColor Dark(FColor(0x14, 0x16, 0x1a));
	const FLinearColor Lip = FMath::Lerp(Skin, FLinearColor(FColor(0xb3, 0x50, 0x5a)), bFem ? .55f : .3f);
	const uint8 KeepSurf = M.Surf;
	const FVector Fwd = Q.RotateVector(FVector::XAxisVector);

	// ---------------------------------------------------------------- legs: thigh, knee, calf, ankle, foot
	for (int32 i = 0; i < 2; i++)
	{
		const double Sx = i == 0 ? -1 : 1;
		const FVector Hip = J(Sx * .092, HipY, 0);
		FVector Knee, Ankle;
		if (bSeated) { Knee = J(Sx * .1, HipY + .03, .42); Ankle = J(Sx * .11, .09, .47); }
		else SolveIK(Hip, i == 0 ? FootL : FootR, .43, .42, FVector(1, 0, .1), Knee, Ankle);
		const double FT = bFem ? 1.06 : 1;
		const double ShortEnd = Look.bShortsLong ? .92 : (bFem ? .12 : .5);   // how far down the thigh the shorts reach
		TArray<FRing> R;
		const FVector Up = Fwd;
		auto Seg = [&](const FVector& A, const FVector& B, double T) { return FMath::Lerp(A, B, T); };
		const double ThighT[6] = {-.08, .2, .45, .7, .9, 1};
		const double ThighR[6] = {.092, .085, .075, .064, .055, .052};
		for (int32 k = 0; k < 6; k++)
		{
			const double T = ThighT[k];
			const bool bShort = T < ShortEnd;
			const double Rr = ThighR[k] * FT * (bShort ? 1.06 : 1);
			R.Add({W(Seg(Hip, Knee, T)), Rr * 1.02, Rr, bShort ? Bottom : Skin, uint8(bShort ? SurfCloth : SurfSkin)});
			// a crisp hem: the next ring just below, in skin
			if (bShort && k < 5 && ThighT[k + 1] >= ShortEnd)
			{
				R.Add({W(Seg(Hip, Knee, ShortEnd)), Rr * 1.0, Rr * .97, Bottom, SurfCloth});
				R.Add({W(Seg(Hip, Knee, ShortEnd + .005)), ThighR[k] * FT, ThighR[k] * FT * .98, Skin, SurfSkin});
			}
		}
		const double CalfT[6] = {.08, .25, .42, .65, .88, 1};
		const double CalfR[6] = {.052, .058, .057, .046, .036, .034};
		for (int32 k = 0; k < 6; k++)
			R.Add({W(Seg(Knee, Ankle, CalfT[k])), CalfR[k] * FT * 1.04, CalfR[k] * FT, Skin, SurfSkin});
		Sweep(M, R, 10, Up, false, true);

		// the foot or the shoe: rounded toe, flat sole
		const FLinearColor FootC = Look.bShoes ? Look.Shoes : Skin;
		M.Surf = Look.bShoes ? SurfCloth : SurfSkin;
		const FVector FootCtr = Ankle + FVector(.065, Sx * .004, -.045);
		AddBlob(M, W(FootCtr), FVector(.115, .046, .036) * (bFem ? .92 : 1), Q, FootC, 10, 5);
		if (Look.bShoes) AddBlob(M, W(FootCtr + FVector(0, 0, -.024)), FVector(.12, .05, .012), Q, FLinearColor(.92f, .9f, .86f), 10, 3);
		M.Surf = KeepSurf;
	}

	// ---------------------------------------------------------------- torso: a lathe with chest, waist and hips
	{
		struct FProf { double Z, W, D, X; };
		static const FProf ProfM[] = {{-.1, .06, .05, 0}, {-.075, .12, .095, 0}, {-.03, .158, .108, -.006}, {.04, .168, .112, -.01}, {.12, .156, .104, -.004}, {.2, .148, .1, 0},
			{.28, .152, .106, .006}, {.36, .168, .116, .012}, {.43, .18, .118, .01}, {.49, .186, .108, 0}, {.535, .165, .09, -.008}, {.565, .115, .07, -.012}, {.59, .07, .056, -.008}, {.6, .055, .05, -.005}};
		static const FProf ProfF[] = {{-.1, .06, .05, 0}, {-.075, .13, .1, -.004}, {-.03, .172, .112, -.014}, {.04, .175, .112, -.016}, {.12, .148, .098, -.006}, {.2, .124, .088, 0},
			{.27, .128, .092, .004}, {.34, .145, .1, .008}, {.41, .152, .104, .008}, {.47, .158, .096, 0}, {.52, .142, .082, -.006}, {.555, .1, .064, -.01}, {.58, .062, .05, -.008}, {.59, .048, .045, -.005}};
		const FProf* Prof = bFem ? ProfF : ProfM;
		const int32 NP = 14;
		const double Base = HipY + .03;
		// where the colours change (bikini top band, the shirt's hem over the shorts' waistband)
		auto ColAt = [&](double Z, uint8& K) -> FLinearColor
		{
			if (bFem)
			{
				if (Z < .07 || (Z > .345 && Z < .455)) { K = SurfCloth; return Bottom; }
				K = SurfSkin; return Skin;
			}
			if (Z < .045) { K = SurfCloth; return Bottom; }
			K = SurfCloth; return Top;
		};
		FGrid G;
		G.NU = 20;
		auto Ring = [&](double Z, double Wd, double Dp, double Xo)
		{
			uint8 K = 0;
			const FLinearColor C = ColAt(Z, K);
			for (int32 a = 0; a < G.NU; a++)
			{
				const double Ang = 2 * PI * a / G.NU;
				const double Cx = FMath::Cos(Ang), Sy = FMath::Sin(Ang);
				// a slightly squarer section than an ellipse: shoulders and hips have corners
				const double Sq = FMath::Pow(FMath::Abs(Cx), .82) * FMath::Sign(Cx), Sq2 = FMath::Pow(FMath::Abs(Sy), .82) * FMath::Sign(Sy);
				// the back is flatter than the front
				const double Dd = Cx < 0 ? Dp * .9 : Dp;
				G.Add(W(FVector(Xo + Sq * Dd, Sq2 * Wd, Base + Z)), C, K);
			}
		};
		G.P.Reserve(G.NU * 24);
		// a pole at the crotch
		for (int32 a = 0; a < G.NU; a++) G.Add(W(FVector(0, 0, Base + Prof[0].Z - .012)), Bottom, SurfCloth);
		for (int32 k = 0; k < NP; k++)
		{
			Ring(Prof[k].Z, Prof[k].W, Prof[k].D, Prof[k].X);
			// colour seams: a second ring a few millimetres on, so the hem is crisp
			if (k < NP - 1)
			{
				const double Z0 = Prof[k].Z, Z1 = Prof[k + 1].Z;
				const double Seams[4] = {bFem ? .07 : .045, .345, .455, -1};
				for (const double Sz : Seams)
					if (Sz > Z0 && Sz < Z1 && (bFem || Sz < .1))
					{
						const double T0 = (Sz - .003 - Z0) / (Z1 - Z0), T1 = (Sz + .003 - Z0) / (Z1 - Z0);
						Ring(Sz - .003, FMath::Lerp(Prof[k].W, Prof[k + 1].W, T0), FMath::Lerp(Prof[k].D, Prof[k + 1].D, T0), FMath::Lerp(Prof[k].X, Prof[k + 1].X, T0));
						Ring(Sz + .003, FMath::Lerp(Prof[k].W, Prof[k + 1].W, T1), FMath::Lerp(Prof[k].D, Prof[k + 1].D, T1), FMath::Lerp(Prof[k].X, Prof[k + 1].X, T1));
					}
			}
		}
		Emit(M, G);
		// a shirt's collar line and the waistband are left to the colour seams; the bikini top gets its cups
		if (bFem)
			for (int32 i = 0; i < 2; i++)
			{
				const double Sx = i == 0 ? -1 : 1;
				M.Surf = SurfCloth;
				AddBlob(M, W(FVector(.062, Sx * .058, Base + .4)), FVector(.06, .066, .062), Q * FQuat(FVector::ZAxisVector, Sx * .25), Bottom, 10, 6);
				M.Surf = KeepSurf;
			}
	}

	// ---------------------------------------------------------------- neck
	{
		TArray<FRing> R;
		R.Add({W(J(0, HipY + .6, -.008)), .056, .062, Skin, SurfSkin});
		R.Add({W(J(0, HipY + .66, -.004)), bFem ? .042 : .049, bFem ? .044 : .052, Skin, SurfSkin});
		R.Add({W(J(0, HipY + .73, .0)), bFem ? .04 : .047, bFem ? .042 : .05, Skin, SurfSkin});
		Sweep(M, R, 12, Q.RotateVector(FVector::XAxisVector), false, false);
	}

	// ---------------------------------------------------------------- arms: shoulder, elbow, wrist, hand
	FVector HandPos[2];
	const double FA = bFem ? .86 : 1;
	for (int32 i = 0; i < 2; i++)
	{
		const double Sx = i == 0 ? -1 : 1;
		const FVector S = J(Sx * (bFem ? .158 : .175), HipY + .525, -.005);
		FVector E, Hn;
		SolveIK(S, i == 0 ? HandL : HandR, .29, .27, FVector(-.55, Sx * .45, -.8), E, Hn);
		const FVector Dir = (Hn - E).GetSafeNormal(UE_SMALL_NUMBER, -FVector::ZAxisVector);
		// the deltoid rounds the shoulder into the chest
		M.Surf = TopK;
		AddBlob(M, W(S + FVector(-.004, Sx * .004, -.004)), FVector(.054, .05, .056) * FA, Q, Top, 10, 6);
		M.Surf = KeepSurf;
		const FLinearColor Sleeve = bFem ? Skin : Top;
		const uint8 SleeveK = bFem ? SurfSkin : SurfCloth;
		const bool bShortSleeve = !bFem && Look.bShortsLong == false && (Look.Shirt.R + Look.Shirt.G + Look.Shirt.B) > 2.4f;
		TArray<FRing> R;
		const double UpT[5] = {0, .25, .55, .85, 1};
		const double UpR[5] = {.05, .049, .044, .039, .037};
		for (int32 k = 0; k < 5; k++)
		{
			const bool bCloth = !bFem && (!bShortSleeve || UpT[k] < .5);
			R.Add({W(FMath::Lerp(S, E, UpT[k])), UpR[k] * FA * (bCloth ? 1.08 : 1), UpR[k] * FA * (bCloth ? 1.04 : .96), bCloth ? Top : Skin, uint8(bCloth ? SurfCloth : SurfSkin)});
		}
		const double LoT[5] = {.1, .3, .6, .9, 1};
		const double LoR[5] = {.039, .041, .035, .028, .026};
		for (int32 k = 0; k < 5; k++)
		{
			const bool bCloth = !bFem && !bShortSleeve && LoT[k] < .9;
			R.Add({W(FMath::Lerp(E, Hn, LoT[k])), LoR[k] * FA * (bCloth ? 1.1 : 1), LoR[k] * FA * (bCloth ? 1.05 : .82), bCloth ? Sleeve : Skin, uint8(bCloth ? SleeveK : SurfSkin)});
		}
		Sweep(M, R, 10, Q.RotateVector(FVector(0, Sx, 0)), false, false);

		// the hand: a palm, the fingers together curling a little, and the thumb
		const FVector Side = Q.RotateVector(FVector(0, Sx, 0));
		FVector Across = FVector::CrossProduct(Dir, Side).GetSafeNormal();
		if (Across.IsNearlyZero()) Across = Fwd;
		const FVector Palm = Hn + Dir * .045 * FA;
		HandPos[i] = Hn + Dir * .035;
		const FQuat HQ = FRotationMatrix::MakeFromXZ(Dir, Across).ToQuat();
		M.Surf = SurfSkin;
		AddBlob(M, W(Palm), FVector(.05, .02, .042) * FA, Q * HQ, Skin, 10, 5);
		AddBlob(M, W(Palm + Dir * .055 * FA - Side * .004), FVector(.04, .016, .038) * FA, Q * HQ * FQuat(FVector::ZAxisVector, Sx * .15), Skin, 8, 4);
		const FVector Thumb0 = Palm + Across * .03 * FA + Dir * -.01, Thumb1 = Thumb0 + (Dir * .035 + Across * .02 - Side * .015) * FA;
		TArray<FRing> Th;
		Th.Add({W(Thumb0), .014 * FA, .012 * FA, Skin, SurfSkin});
		Th.Add({W(Thumb1), .011 * FA, .009 * FA, Skin, SurfSkin});
		Sweep(M, Th, 6, Q.RotateVector(Side), false, true);
		M.Surf = KeepSurf;
	}

	// beer can in the right hand: raised, and tipped back for a pull
	if (bBeer)
	{
		const double A = 2.1 * SipAmt;
		const FVector Axis(-FMath::Sin(A), 0, FMath::Cos(A));
		const FVector C = HandPos[1] + FVector(0, 0, .04);
		M.Surf = SurfSteel;
		M.AddTube(W(C - Axis * .06), W(C + Axis * .035), .033, .033, 12, FLinearColor(FColor(0xc9, 0xcc, 0xd2)));
		M.Surf = SurfGelcoat;
		M.AddTube(W(C + Axis * .035), W(C + Axis * .06), .034, .034, 12, FLinearColor(FColor(0x2f, 0x6f, 0xd0)));
		M.Surf = KeepSurf;
	}

	// ---------------------------------------------------------------- the head
	const double HS = bFem ? .94 : 1;
	const FQuat HR = FQuat(FRotator(FMath::RadiansToDegrees(-HeadNod), FMath::RadiansToDegrees(HeadYaw), 0));
	const FVector HP = J(0, HipY + .77, 0);
	auto HW = [&](const FVector& P) { return W(HP + HR.RotateVector(P * HS)); };
	const FQuat HQ = Q * HR;
	// the skull and face: a sphere pushed into shape
	auto HeadR = [bFem](const FVector& D) -> FVector
	{
		double Rx = .1, Ry = .084, Rz = .114;
		const double Lower = FMath::Clamp(-D.Z * 1.5, 0.0, 1.0);
		const double Front = FMath::Max(0.0, D.X);
		// a narrower jaw, the back of the head tucked in above the neck
		Ry *= 1 - (bFem ? .22 : .17) * Lower * (.4 + .6 * Front);
		if (D.X < 0) Rx *= 1 - .25 * Lower;
		// the face is flatter than the skull
		if (D.X > .55) Rx *= 1 - .09 * (D.X - .55) / .45 * (1 - Lower * .4);
		// chin and brow
		const double Chin = FMath::Exp(-FMath::Square(D.Z + .66) / .018) * FMath::Pow(Front, 6);
		Rx += (bFem ? .008 : .013) * Chin;
		const double Brow = FMath::Exp(-FMath::Square(D.Z - .3) / .005) * FMath::Pow(Front, 4) * FMath::Exp(-FMath::Square(D.Y) / .2);
		Rx += (bFem ? .002 : .005) * Brow;
		// cheekbones
		const double Cheek = FMath::Exp(-FMath::Square(D.Z + .05) / .02) * FMath::Exp(-FMath::Square(FMath::Abs(D.Y) - .55) / .03) * Front;
		double Rr = 1 + .05 * Cheek;
		// eye sockets
		for (int32 s = 0; s < 2; s++)
		{
			const FVector E = FVector(.92, s == 0 ? -.36 : .36, .2).GetSafeNormal();
			const double Dt = FVector::DotProduct(D, E);
			Rr -= .085 * FMath::SmoothStep(.982, .998, Dt);
		}
		return FVector(D.X * Rx, D.Y * Ry, D.Z * Rz) * Rr;
	};
	{
		FGrid G;
		G.NU = 22;
		const int32 NV = 16;
		for (int32 v = 0; v <= NV; v++)
		{
			const double Lat = FMath::Lerp(-PI * .5, PI * .5, double(v) / NV);
			for (int32 u = 0; u < G.NU; u++)
			{
				const double Lon = 2 * PI * u / G.NU;
				const FVector D(FMath::Cos(Lat) * FMath::Cos(Lon), FMath::Cos(Lat) * FMath::Sin(Lon), FMath::Sin(Lat));
				G.Add(HW(HeadR(D) + FVector(0, 0, .02)), Skin, SurfSkin);
			}
		}
		Emit(M, G);
	}
	// eyes, brows, nose, lips, ears
	M.Surf = SurfSkin;
	for (int32 i = 0; i < 2; i++)
	{
		const double Sx = i == 0 ? -1 : 1;
		if (!Look.bGlasses)
		{
			M.Surf = SurfEye;
			AddBlob(M, HW(FVector(.074, Sx * .032, .043)), FVector(.0125) * HS, HQ, FLinearColor(FColor(0xee, 0xea, 0xe4)), 8, 5);
			AddBlob(M, HW(FVector(.085, Sx * .032, .043)), FVector(.0035, .0068, .0068) * HS, HQ, FLinearColor(FColor(0x3a, 0x2a, 0x1c)), 6, 3);
			AddBlob(M, HW(FVector(.0875, Sx * .032, .043)), FVector(.0015, .0034, .0034) * HS, HQ, Dark, 6, 3);
			M.Surf = SurfSkin;
			// the upper lid
			AddBlob(M, HW(FVector(.078, Sx * .032, .051)), FVector(.01, .016, .006) * HS, HQ, Skin * .94f, 8, 4);
		}
		M.Surf = SurfHair;
		AddBlob(M, HW(FVector(.088, Sx * .036, .064)), FVector(.007, .022, .0045) * HS, HQ * FQuat(FVector::XAxisVector, Sx * -.12), HairC * .8f, 8, 4);
		M.Surf = SurfSkin;
		// ears
		AddBlob(M, HW(FVector(-.008, Sx * .083, .025)), FVector(.017, .009, .03) * HS, HQ * FQuat(FVector::ZAxisVector, Sx * .3), Skin * .96f, 8, 5);
	}
	// the nose: bridge and tip, and the nostrils' wings
	AddBlob(M, HW(FVector(.094, 0, .02)), FVector(.014, .011, .034) * HS, HQ * FQuat(FVector::YAxisVector, -.32), Skin, 8, 5);
	AddBlob(M, HW(FVector(.104, 0, -.006)), FVector(.013, .012, .012) * HS, HQ, Skin, 8, 4);
	AddBlob(M, HW(FVector(.095, 0, -.012)), FVector(.01, .022, .009) * HS, HQ, Skin * .97f, 8, 4);
	// lips
	AddBlob(M, HW(FVector(.09, 0, -.038)), FVector(.008, .024, .0055) * HS, HQ, Lip, 10, 4);
	AddBlob(M, HW(FVector(.088, 0, -.048)), FVector(.009, .021, .0065) * HS, HQ, Lip * .95f, 10, 4);
	M.Surf = KeepSurf;

	if (Look.bGlasses)
	{
		const FLinearColor Frame(FColor(0x0b, 0x0d, 0x10));
		M.Surf = SurfGlass;
		for (int32 i = 0; i < 2; i++)
			AddBlob(M, HW(FVector(.098, (i == 0 ? -1 : 1) * .036, .043)), FVector(.006, .03, .021) * HS, HQ, FLinearColor(.06f, .08f, .1f), 10, 5);
		M.Surf = SurfRubber;
		M.AddTube(HW(FVector(.1, -.068, .05)), HW(FVector(.1, .068, .05)), .0035, .0035, 6, Frame);
		for (int32 i = 0; i < 2; i++)
			M.AddTube(HW(FVector(.098, (i == 0 ? -1 : 1) * .07, .048)), HW(FVector(-.005, (i == 0 ? -1 : 1) * .088, .04)), .003, .003, 6, Frame);
		M.Surf = KeepSurf;
	}

	// hair: a shell over the skull from the hairline up (or under the cap)
	auto HairShell = [&](double Front, double Back, double Grow, const FLinearColor& C, uint8 K, double Lift)
	{
		FGrid G;
		G.NU = 22;
		const int32 NV = 7;
		for (int32 v = 0; v <= NV; v++)
			for (int32 u = 0; u < G.NU; u++)
			{
				const double Lon = 2 * PI * u / G.NU;
				const double Line = FMath::Lerp(Front, Back, (1 - FMath::Cos(Lon)) * .5);
				const double Lat = FMath::Lerp(Line, PI * .5, FMath::Pow(double(v) / NV, .8));
				const FVector D(FMath::Cos(Lat) * FMath::Cos(Lon), FMath::Cos(Lat) * FMath::Sin(Lon), FMath::Sin(Lat));
				// tufts: a little unevenness so it reads as hair, not a helmet
				const double Tuft = 1 + .025 * FMath::Sin(Lon * 7 + Lat * 5 + Ph) * (1 - double(v) / NV);
				const double Edge = v == 0 ? .985 : 1.0;
				G.Add(HW(HeadR(D) * Grow * Tuft * Edge + FVector(0, 0, .02 + Lift)), C, K);
			}
		Emit(M, G);
	};
	if (Look.bCap)
	{
		HairShell(-.05, -.42, 1.04, HairC, SurfHair, 0);
		// the cap's crown and its curved brim
		HairShell(.42, .1, 1.1, Look.CapColor, SurfCloth, .006);
		FGrid B;
		B.NU = 9;
		B.bWrap = false;
		for (int32 v = 0; v <= 3; v++)
			for (int32 u = 0; u < B.NU; u++)
			{
				const double A = FMath::Lerp(-1.25, 1.25, double(u) / (B.NU - 1));
				const double R0 = .099, R1 = R0 + .075 * FMath::Cos(A * .55);
				const double Rr = FMath::Lerp(R0, R1, v / 3.0);
				const double Curl = FMath::Square(FMath::Sin(A)) * .012 * (v / 3.0);
				B.Add(HW(FVector(FMath::Cos(A) * Rr, FMath::Sin(A) * Rr * .95, .072 - (Rr - R0) * .22 - Curl)), Look.CapColor * .92f, SurfCloth);
			}
		Emit(M, B);
	}
	else HairShell(bFem || Look.bLongHair ? .5 : .55, Look.bLongHair ? -.62 : -.38, Look.bLongHair ? 1.07 : 1.045, HairC, SurfHair, 0);

	if (Look.bLongHair)
	{
		// the hair swings back from a pivot at the crown as the boat picks up speed
		const FVector Piv(-.02, 0, .06);
		const FQuat Blow(FVector::YAxisVector, Wind * .45 + FMath::Sin(Ph * 3.1) * .03);
		auto HairW = [&](const FVector& P) { return HW(Piv + Blow.RotateVector(P - Piv)); };
		const FVector HairUp = HQ.RotateVector(FVector::XAxisVector);
		if (Look.HairStyle == 2)
		{
			M.Surf = SurfHair;
			AddBlob(M, HW(FVector(-.11, 0, .045)), FVector(.032), HQ, HairC, 8, 5);
			M.Surf = KeepSurf;
			TArray<FRing> R;
			for (int32 k = 0; k <= 5; k++)
			{
				const double T = k / 5.0;
				R.Add({HairW(FVector(-.125 - T * .045, 0, .03 - T * .28)), .034 * (1 - T * .65) + .006, .028 * (1 - T * .6) + .005, HairC, SurfHair});
			}
			Sweep(M, R, 8, HairUp, true, true);
		}
		else if (Look.HairStyle == 3)
		{
			M.Surf = SurfHair;
			AddBlob(M, HW(FVector(-.07, 0, .118)), FVector(.052, .052, .044), HQ, HairC, 10, 6);
			M.Surf = KeepSurf;
		}
		else
		{
			// a curtain of hair down the back, and a lock over each shoulder
			const double Len = Look.HairStyle == 1 ? .34 : .4;
			TArray<FRing> R;
			for (int32 k = 0; k <= 6; k++)
			{
				const double T = k / 6.0;
				const double Wave = Look.HairStyle == 1 ? FMath::Sin(T * 9 + Ph) * .008 : 0;
				R.Add({HairW(FVector(-.06 - T * .035 + Wave, 0, .02 - T * Len)), .05 * (1 - T * .35), .098 * (1 - T * .25), HairC, SurfHair});
			}
			Sweep(M, R, 12, HairUp, true, true);
			for (int32 i = 0; i < 2; i++)
			{
				// the sides, falling behind the ears onto the shoulders
				const double Sx = i == 0 ? -1 : 1;
				TArray<FRing> L;
				for (int32 k = 0; k <= 4; k++)
				{
					const double T = k / 4.0;
					L.Add({HairW(FVector(-.025 - T * .02, Sx * (.075 + T * .02), .02 - T * (Look.HairStyle == 1 ? .2 : .24))), .045 * (1 - T * .4), .022 * (1 - T * .45), HairC, SurfHair});
				}
				Sweep(M, L, 8, HairUp, true, true);
			}
		}
	}
	M.Surf = KeepSurf;
}
