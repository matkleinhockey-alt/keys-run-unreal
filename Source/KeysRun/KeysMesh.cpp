#include "KeysMesh.h"
#include "KeysData.h"

int32 FKeysMesh::AddVert(const FVector& PosM, const FVector& Normal, const FLinearColor& Color)
{
	const int32 I = V.Add(PosM * 100.0);
	if (K.Num() != I) K.SetNumZeroed(I);
	K.Add(FVector2D(Surf, 0));
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
	// Unreal's front face: Cross(B - A, C - A) points away from the normal. Wound the other way the face is drawn
	// as the two-sided material's back face, lit as if it pointed inward (dark), so turn it round here.
	if (FVector::DotProduct(FVector::CrossProduct(B - A, Cc - A), Nrm) > 0) T.Append({I0, I2, I1, I0, I3, I2});
	else T.Append({I0, I1, I2, I0, I2, I3});
}

void FKeysMesh::AddTri(const FVector& A, const FVector& B, const FVector& Cc, const FLinearColor& Color, const FVector& Out)
{
	FVector Nrm = FVector::CrossProduct(B - A, Cc - A).GetSafeNormal();
	if (Nrm.IsNearlyZero()) Nrm = Out.GetSafeNormal();
	if (FVector::DotProduct(Nrm, Out) < 0) Nrm = -Nrm;
	const int32 I0 = AddVert(A, Nrm, Color), I1 = AddVert(B, Nrm, Color), I2 = AddVert(Cc, Nrm, Color);
	if (FVector::DotProduct(FVector::CrossProduct(B - A, Cc - A), Nrm) > 0) T.Append({I0, I2, I1});
	else T.Append({I0, I1, I2});
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

void FKeysMesh::AddRoundBox(const FVector& Ctr, const FVector& Size, double R, const FLinearColor& Color, const FQuat& Rot, int32 Seg)
{
	const FVector H = Size * .5;
	R = FMath::Min(R, FMath::Min3(H.X, H.Y, H.Z) * .98);
	if (R < .002) { AddBox(Ctr, Size, Color, Rot); return; }
	Seg = FMath::Clamp(Seg, 1, 4);
	const FVector In = H - FVector(R);
	// coordinates along an axis: the rounded band at each end (even steps of angle once projected), the flat between
	auto Coords = [&](double Half, double Inner, TArray<double>& Out)
	{
		Out.Reset();
		for (int32 k = Seg; k >= 1; k--) Out.Add(-(Inner + R * FMath::Tan(PI * .25 * k / Seg)));
		Out.Add(-Inner);
		if (Inner > 1e-6) Out.Add(Inner);
		for (int32 k = 1; k <= Seg; k++) Out.Add(Inner + R * FMath::Tan(PI * .25 * k / Seg));
		(void)Half;
	};
	TArray<double> CX, CY, CZ;
	Coords(H.X, In.X, CX); Coords(H.Y, In.Y, CY); Coords(H.Z, In.Z, CZ);
	auto Emit = [&](const FVector& P)
	{
		const FVector Q(FMath::Clamp(P.X, -In.X, In.X), FMath::Clamp(P.Y, -In.Y, In.Y), FMath::Clamp(P.Z, -In.Z, In.Z));
		FVector Nn = P - Q;
		Nn = Nn.SizeSquared() < 1e-12 ? FVector::ZeroVector : Nn.GetSafeNormal();
		return TPair<FVector, FVector>(Q + Nn * R, Nn);
	};
	// six faces: (axis, sign), each a grid over the other two axes
	for (int32 Ax = 0; Ax < 3; Ax++)
		for (int32 Sg = -1; Sg <= 1; Sg += 2)
		{
			const int32 A1 = (Ax + 1) % 3, A2 = (Ax + 2) % 3;
			const TArray<double>& U = A1 == 0 ? CX : A1 == 1 ? CY : CZ;
			const TArray<double>& Vv = A2 == 0 ? CX : A2 == 1 ? CY : CZ;
			const double Face = Sg * (Ax == 0 ? (In.X + R) : Ax == 1 ? (In.Y + R) : (In.Z + R));
			const int32 Base = V.Num();
			for (int32 j = 0; j < Vv.Num(); j++)
				for (int32 i = 0; i < U.Num(); i++)
				{
					FVector P;
					P[Ax] = Face; P[A1] = U[i]; P[A2] = Vv[j];
					const TPair<FVector, FVector> E = Emit(P);
					const FVector Nl = E.Value.IsNearlyZero() ? FVector(Ax == 0 ? Sg : 0, Ax == 1 ? Sg : 0, Ax == 2 ? Sg : 0) : E.Value;
					AddVert(Ctr + Rot.RotateVector(E.Key), Rot.RotateVector(Nl), Color);
				}
			for (int32 j = 0; j + 1 < Vv.Num(); j++)
				for (int32 i = 0; i + 1 < U.Num(); i++)
				{
					const int32 a = Base + j * U.Num() + i, b = a + 1, c = a + U.Num() + 1, d = a + U.Num();
					const FVector Nn = N[a] + N[c];
					if (FVector::DotProduct(FVector::CrossProduct(V[b] - V[a], V[c] - V[a]), Nn) > 0) T.Append({a, c, b, a, d, c});
					else T.Append({a, b, c, a, c, d});
				}
		}
}

void FKeysMesh::AddTube(const FVector& A, const FVector& B, double RA, double RB, int32 Sides, const FLinearColor& Color)
{
	// smooth-shaded: two rings of shared vertices whose normals lean with the taper, so rails, poles and limbs read round
	const FVector Ax = B - A;
	const double Len = Ax.Size();
	const FVector Dir = Ax.GetSafeNormal();
	if (Dir.IsNearlyZero()) return;
	FVector U = FVector::CrossProduct(Dir, FVector::ZAxisVector);
	if (U.SizeSquared() < 1e-6) U = FVector::CrossProduct(Dir, FVector::XAxisVector);
	U.Normalize();
	const FVector W = FVector::CrossProduct(Dir, U);
	const double Slope = Len > 1e-9 ? (RA - RB) / Len : 0;
	const int32 Base = V.Num();
	for (int32 i = 0; i < Sides; i++)
	{
		const double Ang = 2 * PI * i / Sides;
		const FVector O = U * FMath::Cos(Ang) + W * FMath::Sin(Ang);
		const FVector Nn = (O + Dir * Slope).GetSafeNormal();
		AddVert(A + O * RA, Nn, Color);
		AddVert(B + O * RB, Nn, Color);
	}
	for (int32 i = 0; i < Sides; i++)
	{
		const int32 a = Base + 2 * i, b = Base + 2 * i + 1, c = Base + 2 * ((i + 1) % Sides) + 1, d = Base + 2 * ((i + 1) % Sides);
		const FVector Nn = N[a] + N[d];
		if (FVector::DotProduct(FVector::CrossProduct(V[b] - V[a], V[c] - V[a]), Nn) > 0) T.Append({a, c, b, a, d, c});
		else T.Append({a, b, c, a, c, d});
	}
}

void FKeysMesh::AddGlowBlob(const FVector& Ctr, double R, const FLinearColor& Color)
{
	const FVector Ax[3][2] = {{FVector::XAxisVector, FVector::YAxisVector}, {FVector::XAxisVector, FVector::ZAxisVector}, {FVector::YAxisVector, FVector::ZAxisVector}};
	for (int32 p = 0; p < 3; p++)
	{
		const FVector U = Ax[p][0] * R, W = Ax[p][1] * R, Nn = FVector::CrossProduct(Ax[p][0], Ax[p][1]);
		const int32 I0 = AddVert(Ctr - U - W, Nn, Color), I1 = AddVert(Ctr + U - W, Nn, Color), I2 = AddVert(Ctr + U + W, Nn, Color), I3 = AddVert(Ctr - U + W, Nn, Color);
		UV[I0] = FVector2D(2.001, .001); UV[I1] = FVector2D(2.999, .001); UV[I2] = FVector2D(2.999, .999); UV[I3] = FVector2D(2.001, .999);
		T.Append({I0, I1, I2, I0, I2, I3});
	}
}

void FKeysMesh::AddGlowFan(const FVector& A, const FVector& B, const FVector& Cc, const FLinearColor& Color, double A0, double A1)
{
	const FVector Nn = FVector::CrossProduct(B - A, Cc - A).GetSafeNormal();
	FLinearColor CA = Color, CB = Color;
	CA.A = float(A0); CB.A = float(A1);
	const int32 I0 = AddVert(A, Nn, CA), I1 = AddVert(B, Nn, CB), I2 = AddVert(Cc, Nn, CB);
	UV[I0] = FVector2D(2.5, .5); UV[I1] = FVector2D(2.5, .001); UV[I2] = FVector2D(2.5, .999);
	T.Append({I0, I1, I2});
}

void FKeysMesh::Commit(UProceduralMeshComponent* Mesh, int32 Section, UMaterialInterface* Mat) const
{
	if (!Mesh) return;
	if (K.Num() == V.Num()) Mesh->CreateMeshSection_LinearColor(Section, V, T, N, UV, K, TArray<FVector2D>(), TArray<FVector2D>(), C, TArray<FProcMeshTangent>(), false);
	else
	{
		TArray<FVector2D> K2 = K;
		K2.SetNumZeroed(V.Num());
		Mesh->CreateMeshSection_LinearColor(Section, V, T, N, UV, K2, TArray<FVector2D>(), TArray<FVector2D>(), C, TArray<FProcMeshTangent>(), false);
	}
	if (Mat) Mesh->SetMaterial(Section, Mat);
}

namespace KeysMesh
{
	static FLinearColor Srgb(double R, double G, double B) { return FLinearColor(FColor(uint8(R * 255), uint8(G * 255), uint8(B * 255))); }

	// the measurements BuildBoat, TowerEye and the riggers share
	struct FDims
	{
		double L, HB, Free, Draft, DeckZ, MidX, TopZ, TopL, TopW, TC, TopH, TY, W2, L2;
		bool bTower, bBig;
		// web tower frame (x across, y up, z toward the stern from the top's center) -> this boat's frame
		FVector P(double X, double Y, double Z) const { return FVector(TC - Z, X, Y); }
	};
	static FDims Dims(const KeysData::FBoatSpec& S)
	{
		FDims D;
		const FHullDef& H = HullOf(S);
		D.L = S.Len; D.HB = S.Beam * .5; D.Free = H.F; D.Draft = H.YK;
		D.MidX = .05 * D.L; D.DeckZ = SoleAt(S, D.MidX - .5);
		D.TopZ = D.DeckZ + 2.35; D.TopL = FMath::Min(2.6, D.L * .26); D.TopW = FMath::Min(2.2, S.Beam * .66);
		D.TC = D.MidX - .5; D.TopH = D.TopZ + .05;
		D.bTower = S.Id == TEXT("grady") || S.Id == TEXT("freeman");
		D.bBig = S.Id == TEXT("freeman");
		D.TY = D.TopH + (D.bBig ? 3.3 : 2.5);
		D.W2 = FMath::Min(1.15, D.TopW * .5); D.L2 = 1.05;
		return D;
	}

	bool TowerEye(const KeysData::FBoatSpec& S, FVector& OutEye)
	{
		const FDims D = Dims(S);
		if (!D.bTower) return false;
		OutEye = FVector(D.TC - D.L2 * .12, 0, D.TY + .03 + 1.62);
		return true;
	}

	bool HasSwingingRiggers(const KeysData::FBoatSpec& S) { return Dims(S).bTower; }

	bool RiggerTips(const KeysData::FBoatSpec& S, double Rig, FVector OutTip[2], FVector OutBase[2])
	{
		const FDims D = Dims(S);
		if (D.bTower)
		{
			// swivel bases at the tower platform; stowed is 0.12 rad off vertical, deployed 1.22 (about 70 degrees out),
			// raking aft as they go out
			const double K = FMath::Clamp(Rig, 0.0, 1.0), A = .12 + (1.22 - .12) * K, Rk = .18 * K;
			const double Len = FMath::Max(5.5, D.L * .78);
			for (int32 i = 0; i < 2; i++)
			{
				const double Sx = i == 0 ? -1 : 1;
				OutBase[i] = D.P(Sx * S.Beam * .22, D.TY + .38, D.L2 * .12 + .1);
				const FVector Dir(-FMath::Cos(A) * FMath::Sin(Rk), Sx * FMath::Sin(A), FMath::Cos(A) * FMath::Cos(Rk));
				OutTip[i] = OutBase[i] + Dir * Len;
			}
			return true;
		}
		if (S.bRiggerPoles)
		{
			// fixed poles bolted to the T-top, raked out and aft
			for (int32 i = 0; i < 2; i++)
			{
				const double Sx = i == 0 ? -1 : 1;
				OutBase[i] = D.P(Sx * D.TopW * .45, D.TopZ + .08, 0);
				OutTip[i] = D.P(Sx * (D.TopW * .45 + .9), D.TopZ + 4.6, 2.2);
			}
			return true;
		}
		return false;
	}

	void BuildRiggers(FKeysMesh& M, const KeysData::FBoatSpec& S, double Rig)
	{
		if (!HasSwingingRiggers(S)) return;
		FVector Tip[2], Base[2];
		RiggerTips(S, Rig, Tip, Base);
		const FLinearColor Pole = Srgb(.87, .89, .9), ClampC = Srgb(.55, .58, .59), Ring = Srgb(.18, .2, .21);
		for (int32 i = 0; i < 2; i++)
		{
			const FVector Dir = (Tip[i] - Base[i]).GetSafeNormal();
			const double Len = (Tip[i] - Base[i]).Size();
			M.AddTube(Base[i], Tip[i], .085, .035, 12, Pole);
			M.AddTube(Base[i] - Dir * .13, Base[i] + Dir * .13, .12, .12, 12, ClampC);
			// the halyard rings up its length and the release clip at the tip
			for (int32 k = 1; k <= 3; k++)
			{
				const FVector C = Base[i] + Dir * (Len * k / 4);
				const double R = FMath::Lerp(.085, .035, k / 4.0) + .02;
				M.AddTube(C - Dir * .015, C + Dir * .015, R, R, 8, Ring);
			}
			M.AddTube(Tip[i], Tip[i] + Dir * .12, .03, .02, 6, Ring);
		}
	}

	void BuildBoat(FKeysMesh& M, const KeysData::FBoatSpec& S, bool bRiggers)
	{
		const FDims Dm = Dims(S);
		const double L = S.Len, Free = Dm.Free, Draft = Dm.Draft;
		const FHullDef& H = HullOf(S);
		const FLinearColor Liner = H.Liner, Dark = Srgb(.12, .13, .15), Glass = Srgb(.16, .22, .27);
		const FLinearColor Steel = Srgb(.85, .86, .88), Uph = S.Uph, HullC = H.Hull, DeckC = H.Deck;
		const double Cap = .14 + S.Beam * .02;
		// the hull itself: the web game's loft (KeysHull.cpp), a cat's sponsons and bridge deck on the Freeman.
		// Smooth along its length and round the topsides (the chines keep their edge); glossy gelcoat, a non-skid
		// sole, a rubber rub rail.
		const uint8 KeepSurf = M.Surf;
		const int32 HullFrom = M.V.Num();
		M.Surf = SurfGelcoat;
		BuildHullFor(M, H, L, S.Beam);
		M.SmoothNormalsFrom(HullFrom, 32);
		M.SyncKinds();
		for (int32 i = HullFrom; i < M.V.Num(); i++)
		{
			const FLinearColor& Cv = M.C[i];
			if (Cv.Equals(H.Deck, .002f)) M.K[i].X = SurfNonSkid;
			else if (Cv.Equals(H.Rub, .002f) && !H.Rub.Equals(H.Hull, .002f)) M.K[i].X = SurfRubber;
		}
		// rounded mouldings: R is the edge radius
		auto RBox = [&M](const FVector& C, const FVector& Sz, const FLinearColor& Col, double R, uint8 Kind, const FQuat& Rot = FQuat::Identity)
		{
			const uint8 Was = M.Surf;
			M.Surf = Kind;
			M.AddRoundBox(C, Sz, R, Col, Rot, 2);
			M.Surf = Was;
		};
		auto Tube = [&M](const FVector& A, const FVector& B, double RA, double RB, int32 Sides, const FLinearColor& Col, uint8 Kind)
		{
			const uint8 Was = M.Surf;
			M.Surf = Kind;
			M.AddTube(A, B, RA, RB, Sides, Col);
			M.Surf = Was;
		};

		const double DeckZ = Dm.DeckZ, MidX = Dm.MidX;
		// console and windshield
		const double CZ = SoleAt(S, MidX);
		RBox(FVector(MidX, 0, CZ + .6), FVector(1.05, FMath::Min(1.15, S.Beam * .42), 1.2), HullC, .11, SurfGelcoat);
		RBox(FVector(MidX + .45, 0, CZ + 1.32), FVector(.06, 1.05, .3), Glass, .025, SurfGlass, FQuat(FVector::YAxisVector, -.5));
		RBox(FVector(MidX - .05, 0, CZ + 1.21), FVector(.5, .7, .04), Dark, .015, SurfRubber);
		// the windshield's stainless frame
		{
			const FQuat Ws(FVector::YAxisVector, -.5);
			const FVector WC(MidX + .45, 0, CZ + 1.32);
			Tube(WC + Ws.RotateVector(FVector(0, -.53, .15)), WC + Ws.RotateVector(FVector(0, .53, .15)), .014, .014, 8, Steel, SurfSteel);
		}
		if (S.bLeaning)
		{
			// leaning post with a cooler seat in front and a rocket launcher of rods along its back (web model.ts seating)
			RBox(FVector(MidX - 1.15, 0, DeckZ + .5), FVector(.8, 1.05, 1.0), HullC, .09, SurfGelcoat);
			RBox(FVector(MidX - 1.15, 0, DeckZ + 1.06), FVector(.85, 1.1, .14), Uph, .06, SurfVinyl);
			RBox(FVector(MidX - 1.53, 0, DeckZ + 1.28), FVector(.11, 1.05, .38), Uph, .05, SurfVinyl, FQuat(FVector::YAxisVector, .12));
			for (int32 i = 0; i < 5; i++)
			{
				const double Y = (i - 2) * S.Beam * .085;
				Tube(FVector(MidX - 1.58, Y, DeckZ + 1.1), FVector(MidX - 2.33, Y, DeckZ + 3.4), .016, .007, 6, Dark, SurfRubber);
				Tube(FVector(MidX - 1.62, Y - .04, DeckZ + 1.25), FVector(MidX - 1.62, Y + .04, DeckZ + 1.25), .06, .06, 12, Srgb(.79, .64, .15), SurfSteel);
			}
		}
		else
		{
			// helm seats: bucket chairs abreast on pedestals
			const int32 N = S.bRodRack ? 4 : S.Beam > 3.6 ? 3 : 2;
			for (int32 i = 0; i < N; i++)
			{
				const double Y = (i - (N - 1) * .5) * .64;
				Tube(FVector(MidX - 1.25, Y, DeckZ), FVector(MidX - 1.25, Y, DeckZ + .46), .06, .05, 12, Steel, SurfSteel);
				RBox(FVector(MidX - 1.25, Y, DeckZ + .53), FVector(.56, .57, .18), Uph, .07, SurfVinyl);
				RBox(FVector(MidX - 1.52, Y, DeckZ + .95), FVector(.15, .56, .7), Uph, .06, SurfVinyl, FQuat(FVector::YAxisVector, .13));
				for (int32 s2 = -1; s2 <= 1; s2 += 2) RBox(FVector(MidX - 1.25, Y + s2 * .27, DeckZ + .69), FVector(.5, .09, .28), Uph, .04, SurfVinyl);
			}
		}
		// bow casting deck (or the go-fast's sun pad) and stern bench, inside the liner where they stand
		{
			const double BX = L * .33, BW = InnerHalfBeam(S, BX) * 1.9;
			RBox(FVector(BX, 0, SoleAt(S, BX) + .18), FVector(L * .12, BW, .36), DeckC, .05, SurfNonSkid);
			if (S.bSunpad)
			{
				const double SX = L * .22;
				RBox(FVector(SX, 0, SoleAt(S, SX) + .3), FVector(L * .15, InnerHalfBeam(S, SX) * 1.9, .24), Uph, .1, SurfVinyl);
			}
			const double AX = -L * .5 + Cap + .3;
			RBox(FVector(AX, 0, SoleAt(S, AX) + .25), FVector(.5, InnerHalfBeam(S, AX) * 1.95, .5), HullC, .07, SurfGelcoat);
			RBox(FVector(AX + .02, 0, SoleAt(S, AX) + .53), FVector(.46, InnerHalfBeam(S, AX) * 1.85, .09), Uph, .04, SurfVinyl);
		}
		if (S.bBowRail)
		{
			// stainless bow rail on stanchions from the shoulders round the stem (web model.ts bowRail)
			const double L0 = L, B0 = S.Beam;
			auto RailAt = [&](double T, int32 Side)
			{
				const FHullSta St = HullStation(H, L0, B0, T);
				const double Y = Side * FMath::Max(.04, St.Bs - Cap * .5);
				return FVector(-HullZRake(H, St, St.YS), Y, St.YS);
			};
			for (int32 Side = -1; Side <= 1; Side += 2)
			{
				const int32 NR = 10;
				for (int32 i = 0; i < NR; i++)
				{
					const double T0 = FMath::Lerp(.6, .985, double(i) / NR), T1 = FMath::Lerp(.6, .985, double(i + 1) / NR);
					Tube(RailAt(T0, Side) + FVector(0, 0, .62), RailAt(T1, Side) + FVector(0, 0, .62), .02, .02, 10, Steel, SurfSteel);
					if (i % 2 == 0) Tube(RailAt(T0, Side), RailAt(T0, Side) + FVector(0, 0, .62), .016, .016, 10, Steel, SurfSteel);
				}
			}
			Tube(RailAt(.985, -1) + FVector(0, 0, .62), RailAt(.985, 1) + FVector(0, 0, .62), .02, .02, 10, Steel, SurfSteel);
		}

		// T-top or hardtop on four posts
		const double TopZ = Dm.TopZ, TopL = Dm.TopL, TopW = Dm.TopW;
		for (int32 a = -1; a <= 1; a += 2)
			for (int32 b = -1; b <= 1; b += 2)
				Tube(FVector(MidX - .5 + a * TopL * .38, b * TopW * .4, DeckZ), FVector(MidX - .5 + a * TopL * .38, b * TopW * .4, TopZ), .035, .035, 12, Steel, SurfSteel);
		RBox(FVector(MidX - .5, 0, TopZ + .05), FVector(TopL, TopW, S.bHardtop ? .12 : .06), S.Canvas, S.bHardtop ? .05 : .025, uint8(S.bHardtop ? SurfGelcoat : SurfCloth));
		if (S.bHardtop) RBox(FVector(MidX - .5, 0, TopZ - .02), FVector(TopL * .9, TopW * .9, .04), Srgb(.9, .9, .9), .015, SurfGelcoat);
		if (S.bTTop)
			for (int32 b = -1; b <= 1; b += 2)   // the frame rail along each side under the canvas
				Tube(FVector(MidX - .5 - TopL * .46, b * TopW * .4, TopZ - .02), FVector(MidX - .5 + TopL * .46, b * TopW * .4, TopZ - .02), .03, .03, 10, Steel, SurfSteel);
		if (S.bRodRack)
			for (int32 i = 0; i < 8; i++)
			{
				// rods racked along the aft edge of the top, butts in the holders, tips up and aft
				const double Y = (i - 3.5) * TopW * .105, AX = MidX - .5 - TopL * .5;
				Tube(FVector(AX + .08, Y, TopZ - .02), FVector(AX - .06, Y, TopZ + .32), .03, .03, 8, Dark, SurfRubber);
				Tube(FVector(AX, Y, TopZ + .1), FVector(AX - .95, Y, TopZ + 2.5), .013, .006, 6, Srgb(.16, .18, .19), SurfGelcoat);
				Tube(FVector(AX - .12, Y - .035, TopZ + .3), FVector(AX - .12, Y + .035, TopZ + .3), .055, .055, 12, Srgb(.79, .64, .15), SurfSteel);
			}
		if (S.bRadar)
		{
			RBox(FVector(MidX - .5, 0, TopZ + .22), FVector(.18, .18, .18), Srgb(.91, .91, .91), .04, SurfGelcoat);
			RBox(FVector(MidX - .5, 0, TopZ + .36), FVector(.16, 1.3, .1), Srgb(.95, .95, .95), .045, SurfGelcoat);
		}
		// whip antennas aft on the top
		for (int32 b = -1; b <= 1; b += 2)
			Tube(FVector(MidX - .5 - TopL * .3, b * TopW * .32, TopZ + .12), FVector(MidX - .5 - TopL * .3 - .9, b * TopW * .32, TopZ + 2.4), .022, .012, 8, Srgb(.96, .96, .96), SurfGelcoat);
		if (S.bRiggerPoles)
		{
			// fixed outrigger poles on the T-top (the boats with no tower to swing riggers from)
			FVector Tip[2], Base[2];
			RiggerTips(S, 0, Tip, Base);
			for (int32 i = 0; i < 2; i++)
			{
				Tube(Base[i], Tip[i], .028, .022, 10, Steel, SurfSteel);
				Tube(Base[i] - FVector(0, 0, .06), Base[i] + FVector(0, 0, .1), .06, .06, 12, Srgb(.55, .58, .59), SurfSteel);
			}
		}

		// Tuna tower with an upper helm on the two offshore fishing boats, ported from the web game's boat model
		// (apps/client/src/entities/boat/model.ts). The Freeman gets the big one: taller, with a black visor
		// and a radar dome. Both carry the swinging outriggers (BuildRiggers).
		if (Dm.bTower)
		{
			const bool bBig = Dm.bBig;
			const double TopH = Dm.TopH, TW = TopW, TL = TopL;
			const double TY = Dm.TY, W2 = Dm.W2, L2 = Dm.L2;
			const FLinearColor Fr = bBig ? Srgb(.08, .09, .1) : Srgb(.79, .81, .83);
			const FLinearColor TopC = bBig ? Srgb(.97, .97, .96) : S.Canvas, TUph = Srgb(.6, .63, .65), White = Srgb(.95, .95, .95);
			auto P = [&Dm](double X, double Y, double Z) { return Dm.P(X, Y, Z); };
			const uint8 FrK = bBig ? SurfRubber : SurfSteel;
			auto Bar = [&M, FrK](const FVector& A, const FVector& B, double R, const FLinearColor& C) { const uint8 Was = M.Surf; M.Surf = FrK; M.AddTube(A, B, R, R, 10, C); M.Surf = Was; };
			// four legs from the top up to the platform
			for (int32 sx = -1; sx <= 1; sx += 2)
				for (int32 sz = -1; sz <= 1; sz += 2)
					Bar(P(sx * TW * .38, TopH + .08, sz * TL * .36), P(sx * W2 * .5, TY, sz * L2 * .45), .04, Fr);
			// platform, rails around it
			RBox(P(0, TY, 0), FVector(L2 + .12, W2 + .12, .06), TopC, .025, SurfNonSkid);
			for (int32 sx = -1; sx <= 1; sx += 2)
			{
				Bar(P(sx * W2 * .55, TY + .9, -L2 * .5), P(sx * W2 * .55, TY + .9, L2 * .5), .025, Fr);
				for (int32 sz = -1; sz <= 1; sz += 2) Bar(P(sx * W2 * .55, TY, sz * L2 * .5), P(sx * W2 * .55, TY + .9, sz * L2 * .5), .025, Fr);
			}
			Bar(P(-W2 * .55, TY + .9, -L2 * .5), P(W2 * .55, TY + .9, -L2 * .5), .025, Fr);
			// upper helm: console, screen, wheel, seat
			RBox(P(0, TY + .39, -L2 * .3), FVector(.3, .56, .72), Liner, .06, SurfGelcoat);
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
			RBox(P(0, TY + .78, L2 * .3), FVector(.3, .6, .16), TUph, .06, SurfVinyl);
			RBox(P(0, TY + 1.02, L2 * .45), FVector(.1, .6, .4), TUph, .045, SurfVinyl);
			// upper top on its own posts
			RBox(P(0, TY + 2.05, 0), FVector(L2 + .5, W2 + .4, .06), TopC, .025, SurfCloth);
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
					Bar(P(sx * W2 * .52, TY - 1.6, .1), P(sx * W2 * .52, TY - .6, .1), .07, White);   // rigger base mounts
					Bar(P(sx * TW * .3, TopH + .12, TL * .42), P(sx * TW * .3, TopH + 5.4, TL * .42 + .4), .025, White);
				}
			}
			if (bRiggers) BuildRiggers(M, S, 0);
		}

		// outboards: hung off the transom, or set back on a bracket with its own swim platform
		const double XT = SternX(S);
		const double X0 = XT - .35 - (S.bBracket ? .55 : 0);
		if (S.bBracket)
		{
			const double BW = FMath::Min(S.Beam * .8, S.Engines * .74 + .5);
			RBox(FVector(XT - .33, 0, Free - .55), FVector(.66, BW, .5), HullC, .06, SurfGelcoat);
			RBox(FVector(XT - .33, 0, Free - .29), FVector(.7, BW + .04, .05), DeckC, .02, SurfNonSkid);
		}
		for (int32 e = 0; e < S.Engines; e++)
		{
			const double Y = (e - (S.Engines - 1) * .5) * .74;
			// the cowl: a rounded shell over a waisted midsection, the leg tapering to the gearcase and skeg
			RBox(FVector(X0 - .05, Y, Free - .1), FVector(.66, .52, .7), S.Engine, .2, SurfGelcoat);
			RBox(FVector(X0 - .02, Y, Free - .48), FVector(.5, .4, .2), S.Engine, .08, SurfGelcoat);
			RBox(FVector(X0 + .05, Y, Free * .3 - .25), FVector(.32, .2, Free * .9 + .5), S.EngLow, .08, SurfGelcoat);
			RBox(FVector(X0 + .05, Y, -Draft - .05), FVector(.46, .13, .15), S.EngLow, .06, SurfGelcoat);
			RBox(FVector(X0 + .1, Y, -Draft - .2), FVector(.2, .03, .2), S.EngLow, .01, SurfGelcoat);
			RBox(FVector(X0 - .05, Y, Free + .2), FVector(.52, .42, .1), S.EngAcc, .05, SurfGelcoat);
			RBox(FVector(X0 - .385, Y, Free - .1), FVector(.02, .3, .12), S.EngAcc, .008, SurfGelcoat);   // the badge on the back of the cowl
		}
		// rod holders along the gunwale aft
		for (int32 Side = -1; Side <= 1; Side += 2)
			for (int32 k = 0; k < 3; k++)
			{
				const double X = -L * .32 + k * .45;
				const double Y0 = InnerHalfBeam(S, X) + Cap * .5, Z0 = SheerAt(S, X);
				Tube(FVector(X, Side * Y0, Z0 - .25), FVector(X - .2, Side * (Y0 + .15), Z0 + 1.7), .012, .006, 6, Dark, SurfGelcoat);
				Tube(FVector(X, Side * Y0, Z0 - .05), FVector(X - .024, Side * (Y0 + .018), Z0 + .16), .028, .028, 10, Steel, SurfSteel);
			}
		M.Surf = KeepSurf;
	}

	double FishLength(double WeightLb) { return FMath::Clamp(.27 * FMath::Pow(FMath::Max(.3, WeightLb), 1.0 / 3.0), .22, 4.2); }

	void BuildFish(FKeysMesh& M, double Len, const FLinearColor& Color, bool bBill)
	{
		const int32 NR = 28, NA = 18;
		const double HMax = .13 * Len;
		const uint8 KeepSurf = M.Surf;
		const int32 From = M.V.Num();
		M.Surf = SurfFish;
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
		M.SmoothNormalsFrom(From, 70);
		// forked tail
		M.Surf = SurfFin;
		const double TX = -.55 * Len;
		const FLinearColor Fin = Color * .8f;
		M.AddTri(FVector(TX + .04 * Len, 0, 0), FVector(TX - .16 * Len, 0, .17 * Len), FVector(TX - .1 * Len, 0, .02 * Len), Fin, FVector::YAxisVector);
		M.AddTri(FVector(TX + .04 * Len, 0, 0), FVector(TX - .1 * Len, 0, -.02 * Len), FVector(TX - .16 * Len, 0, -.17 * Len), Fin, FVector::YAxisVector);
		// dorsal
		M.AddTri(FVector(.1 * Len, 0, HMax * .9), FVector(-.12 * Len, 0, HMax * .9), FVector(-.02 * Len, 0, HMax * (bBill ? 2.6 : 1.7)), Fin, FVector::YAxisVector);
		if (bBill) M.AddTube(Nose, Nose + FVector(.25 * Len, 0, .01 * Len), .012 * Len, .002, 8, Color * .6f);
		// eyes
		M.Surf = SurfEye;
		for (int32 Sx = -1; Sx <= 1; Sx += 2)
		{
			const FVector E(.36 * Len, Sx * HMax * .36, HMax * .22);
			M.AddRoundBox(E, FVector(.03, .012, .03) * Len, .0059 * Len, FLinearColor(.85f, .8f, .55f));
			M.AddRoundBox(E + FVector(0, Sx * .006 * Len, 0), FVector(.016, .004, .016) * Len, .0019 * Len, FLinearColor(.02f, .02f, .025f));
		}
		M.Surf = KeepSurf;
	}
}

void FKeysMesh::SmoothNormals(double MaxAngleDeg) { SmoothNormalsFrom(0, MaxAngleDeg); }

void FKeysMesh::SmoothNormalsFrom(int32 From, double MaxAngleDeg)
{
	const double CosMax = FMath::Cos(FMath::DegreesToRadians(MaxAngleDeg));
	// group vertices by position (to a tenth of a millimeter; positions are centimeters)
	TMap<FIntVector, TArray<int32>> Groups;
	Groups.Reserve(V.Num());
	for (int32 i = FMath::Max(0, From); i < V.Num(); i++)
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
