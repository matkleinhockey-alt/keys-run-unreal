// US-1 and the old bridge, in the style of the old Keys concrete bridges: long, low spans on pile bents, a high concrete
// arch over the boat channel, and the old viaduct of shallow barrel arches beside it with its swing span gone.
//
// Shaded with M_KR_Concrete (tiling 2K concrete projected from world position on all three axes, so a long span never
// stretches it, plus a detail normal); the vertex colours carry the paint and the weathering: rust running down from
// the rail posts, the dark tide band and green growth at the waterline, barnacles on the piles.
//
// Built in chunks along the bridge, each in two levels of detail: Near (rail posts and pipe rails, girders, bracing,
// rust streaks, barnacles) and Far (the same outline in solid shapes, nothing thin enough to shimmer). The Tick shows
// one or the other by distance (UpdateBridgeLod).
#include "KeysLandmarks.h"
#include "KeysTown.h"
#include "KeysGeo.h"
#include "KeysMesh.h"
#include "KeysWorld.h"
#include "ProceduralMeshComponent.h"
#include "Materials/MaterialInterface.h"
#include "Kismet/GameplayStatics.h"
#include "Camera/PlayerCameraManager.h"

using namespace KeysGeo;
using KeysTownPaint::Hex;

namespace
{
	constexpr double ChunkLen = 240;   // meters of bridge per chunk
	constexpr double NearDist = 300;   // detailed within this of the camera
	constexpr double RampOut = 330, RampIn = 62, ArchRise = 12;
	#define ArchC ((BridgeArchX0 + BridgeArchX1) / 2)   // the arch moves with the map in use

	double Hb(double A, double B) { const double H = FMath::Sin(A * 127.1 + B * 311.7) * 43758.5453; return H - FMath::FloorToDouble(H); }
	double SS(double A, double B, double X) { const double T = FMath::Clamp((X - A) / (B - A), 0.0, 1.0); return T * T * (3 - 2 * T); }

	// the new bridge's deck centreline height: low over the flats, climbing to the arch over the channel
	double NewDeckH(double X)
	{
		const double Z = ChainZ(X), SD = ShoreInfo(X, Z).D;
		return (SD < 0 ? 5.6 : 2.6 + (7.2 - 2.6) * SS(0, 70, SD)) + ArchRise * SS(RampOut, RampIn, FMath::Abs(X - ArchC));
	}
	double AngAt(double X) { return KeysGeo::ChainAngle(X); }

	const FLinearColor Rust(.42f, .2f, .08f), Algae(.19f, .26f, .14f), Tide(.15f, .14f, .12f), Asphalt(.2f, .205f, .215f);

	// a pile or pier face colour by height above the sea: green growth below, the black tide band, a brown stain fading
	// up into clean concrete
	FLinearColor WaterlineCol(double Y, const FLinearColor& C, double Shift)
	{
		const FLinearColor Stain = C * .6f + FLinearColor(.03f, .02f, 0.f);
		Y -= Shift;
		if (Y < -.6) return Algae * .8f;
		if (Y < -.25) return FMath::Lerp(Algae * .8f, Algae, float((Y + .6) / .35));
		if (Y < .3) return FMath::Lerp(Algae, Tide, float(SS(-.25, .3, Y)));
		if (Y < 1.4) return FMath::Lerp(Tide, Stain, float((Y - .3) / 1.1));
		if (Y < 2.6) return FMath::Lerp(Stain, C, float((Y - 1.4) / 1.2));
		return C;
	}

	struct FBrFrame
	{
		// a point along the bridge line: (X, Y) on the line, Off across it (+ = the side ChainZ grows toward)
		double Ang = 0, PX = 0, PZ = 1, AX = 1, AZ = 0;
		explicit FBrFrame(double X) : Ang(AngAt(X)) { PX = -FMath::Sin(Ang); PZ = FMath::Cos(Ang); AX = FMath::Cos(Ang); AZ = FMath::Sin(Ang); }
		FVector At(double X, double Z0, double Y, double Along, double Off) const { return FVector(X + AX * Along + PX * Off, Y, Z0 + AZ * Along + PZ * Off); }
	};

	// a quad whose colour fades from bottom (CB) to top (CT)
	void GradQuad(FTownBatch& B, const FVector& A0, const FVector& A1, const FVector& T1, const FVector& T0, const FVector& N, const FLinearColor& CB, const FLinearColor& CT)
	{
		B.TriCol(A0, A1, T1, N, CB, CB, CT);
		B.TriCol(A0, T1, T0, N, CB, CT, CT);
	}

	// a square-section pile (or a pier block) from Y0 to Y1, HA along the bridge by HO across, banded at the waterline
	void BandedBlock(FTownBatch& B, const FBrFrame& F, double X, double Z0, double HA, double HO, double Y0, double Y1, const FLinearColor& C, double Shift, bool bTop)
	{
		TArray<double> Ys = {Y0};
		for (double Y : {-.6, -.25, .3, 1.4, 2.6}) if (Y + Shift > Y0 + .05 && Y + Shift < Y1 - .05) Ys.Add(Y + Shift);
		Ys.Add(Y1);
		const double Cs[4][2] = {{1, -1}, {1, 1}, {-1, 1}, {-1, -1}};
		for (int32 f = 0; f < 4; f++)
		{
			const double* A = Cs[f];
			const double* Bc = Cs[(f + 1) % 4];
			const double MA = (A[0] + Bc[0]) * .5, MO = (A[1] + Bc[1]) * .5;
			const FVector N(F.AX * MA + F.PX * MO, 0, F.AZ * MA + F.PZ * MO);
			for (int32 k = 0; k + 1 < Ys.Num(); k++)
			{
				const FVector P0 = F.At(X, Z0, Ys[k], A[0] * HA, A[1] * HO), P1 = F.At(X, Z0, Ys[k], Bc[0] * HA, Bc[1] * HO);
				const FVector Q0 = F.At(X, Z0, Ys[k + 1], A[0] * HA, A[1] * HO), Q1 = F.At(X, Z0, Ys[k + 1], Bc[0] * HA, Bc[1] * HO);
				GradQuad(B, P0, P1, Q1, Q0, N, WaterlineCol(Ys[k], C, Shift), WaterlineCol(Ys[k + 1], C, Shift));
			}
		}
		if (bTop) B.Quad3(F.At(X, Z0, Y1, -HA, -HO), F.At(X, Z0, Y1, HA, -HO), F.At(X, Z0, Y1, HA, HO), F.At(X, Z0, Y1, -HA, HO), FVector(0, 1, 0), C);
	}

	// barnacles and oysters crusted round a pile's tide zone, sitting proud of its four faces
	void Barnacles(FTownBatch& B, const FBrFrame& F, double X, double Z0, double HA, double HO, double Shift, int32 N, double Seed)
	{
		static const uint32 Cols[] = {0xa49c8c, 0x8f877a, 0xb8b0a0, 0x5a554c, 0x6f6a5c, 0x3f4a30};
		for (int32 i = 0; i < N; i++)
		{
			const double R1 = Hb(Seed + i * 1.37, 2.1), R2 = Hb(Seed + i * 2.11, 5.3), R3 = Hb(Seed - i * .71, 8.9);
			const int32 Face = int32(R1 * 4) % 4;
			const double U = R2 * 2 - 1;
			const double Y = Shift - .45 + .8 * FMath::Pow(R3, 1.4);
			const double R = .03 + .045 * Hb(Seed + i, 11.0);
			double A = 0, O = 0;
			switch (Face) { case 0: A = HA + R * .35; O = U * HO; break; case 1: A = -HA - R * .35; O = U * HO; break; case 2: O = HO + R * .35; A = U * HA; break; default: O = -HO - R * .35; A = U * HA; }
			const FVector P = F.At(X, Z0, Y, A, O);
			B.Ball(P.X, P.Y, P.Z, R, Cols[int32(Hb(Seed, i * 3.3) * 6) % 6], .75, 5);
		}
	}

	// rust bleeding down a face from a post or a drain: wide at the top, tapering, fading into the face colour
	void Streak(FTownBatch& B, const FVector& Top, const FVector& Out, const FVector& Tan, double Len, double W, const FLinearColor& Face, const FLinearColor& Stain, double K)
	{
		const FVector O = Out * .012, D(0, -Len, 0);
		const FLinearColor CT = FMath::Lerp(Face, Stain, float(K)), CB = FMath::Lerp(Face, Stain, float(K * .15));
		GradQuad(B, Top + O + D - Tan * W * .35, Top + O + D + Tan * W * .35, Top + O + Tan * W, Top + O - Tan * W, Out, CB, CT);
	}

	struct FChunk { FTownBatch Near, Far; double X0 = 0, X1 = 0, ZOff = 0; };
}

void AKeysLandmarks::BuildBridges(FTownBatch&)
{
	TArray<FChunk> NewCh, OldCh;
	const int32 NNew = FMath::CeilToInt((WBX1 - WBX0) / ChunkLen), NOld = FMath::CeilToInt((OldBridgeX1 - OldBridgeX0) / ChunkLen);
	NewCh.SetNum(NNew); OldCh.SetNum(NOld);
	for (int32 i = 0; i < NNew; i++) { NewCh[i].X0 = WBX0 + i * ChunkLen; NewCh[i].X1 = NewCh[i].X0 + ChunkLen; }
	for (int32 i = 0; i < NOld; i++) { OldCh[i].X0 = OldBridgeX0 + i * ChunkLen; OldCh[i].X1 = OldCh[i].X0 + ChunkLen; OldCh[i].ZOff = OldBridgeDZ; }
	auto NewAt = [&](double X) -> FChunk& { return NewCh[FMath::Clamp(int32((X - WBX0) / ChunkLen), 0, NNew - 1)]; };
	auto OldAt = [&](double X) -> FChunk& { return OldCh[FMath::Clamp(int32((X - OldBridgeX0) / ChunkLen), 0, NOld - 1)]; };
	auto OnLand = [](double X, double Z) { return LandH(X, Z) > .9 && ShoreInfo(X, Z).D < -4; };

	const FLinearColor NewC = Hex(0xd8d1c0), OldC = Hex(0xb3a792), Galv(.6f, .61f, .6f);
	constexpr double SlabT = .55, GirderD = 1.1, HalfW = 5.2;

	// ================================================================ US-1: the deck, 12 m at a time (24 m far)
	for (double X = WBX0; X < WBX1 - 1; X += 12)
	{
		const double XA = X, XB = X + 12, MX = (XA + XB) / 2;
		const double ZA = ChainZ(XA), ZB = ChainZ(XB), MZ = (ZA + ZB) / 2;
		const double Ang = FMath::Atan2(ZB - ZA, XB - XA), Len = FMath::Sqrt(FMath::Square(XB - XA) + FMath::Square(ZB - ZA));
		const double PX = -FMath::Sin(Ang), PZ = FMath::Cos(Ang), AX = FMath::Cos(Ang), AZ = FMath::Sin(Ang);
		if (OnLand(MX, MZ))
		{
			// across the keys it is road on the ground (the vertex-colour batch, as before)
			// (through the town it lies on the pad, as one of its streets)
			auto RoadH = [](double RX, double RZ) { return InTownPad(RX, RZ) ? 2.25 : LandH(RX, RZ); };
			FTownBatch& Road = LmAt(MX, MZ);
			const double LA = RoadH(XA, ZA), LB = RoadH(XB, ZB), LH = (LA + LB) / 2, RPt = FMath::Atan2(LB - LA, Len);
			Road.Box(MX, LH + .08, MZ, Len + .3, .16, 9, 0x3e4146, 0, -Ang, RPt);
			Road.Box(MX, LH + .17, MZ, Len + .3, .02, .2, 0xe8c43a, 0, -Ang, RPt);
			for (int32 sg = -1; sg <= 1; sg += 2) Road.Box(MX + PX * 3.9 * sg, LH + .17, MZ + PZ * 3.9 * sg, Len + .3, .02, .18, 0xf4f1e8, 0, -Ang, RPt);
			continue;
		}
		const double HA = NewDeckH(XA), HB = NewDeckH(XB), Pt = FMath::Atan2(HB - HA, Len), H = (HA + HB) / 2;
		const double Top = H + SlabT / 2;
		const FLinearColor C = NewC * float(.9 + .16 * Hb(MX, MZ));
		FChunk& Ch = NewAt(MX);
		FTownBatch& N = Ch.Near;

		// ---- near: slab, wearing course and lines, curbs, posts and pipe rails, girders, rust
		N.M.Surf = SurfConcrete;
		N.BoxC(MX, H, MZ, Len + .02, SlabT, HalfW * 2, C, 0, -Ang, Pt);
		for (int32 sg = -1; sg <= 1; sg += 2)
		{
			N.BoxC(MX + PX * (HalfW - .25) * sg, H - SlabT / 2 - GirderD / 2, MZ + PZ * (HalfW - .25) * sg, Len + .02, GirderD, .5, C * .97f, 0, -Ang, Pt);   // edge girders (the fascia)
			N.BoxC(MX + PX * 1.75 * sg, H - SlabT / 2 - GirderD * .45, MZ + PZ * 1.75 * sg, Len + .02, GirderD * .9, .45, C * .9f, 0, -Ang, Pt);
			N.BoxC(MX + PX * 4.75 * sg, Top + .15, MZ + PZ * 4.75 * sg, Len + .02, .3, .45, C * 1.03f, 0, -Ang, Pt);   // curb
		}
		N.M.Surf = SurfAsphalt;
		N.BoxC(MX, Top + .03, MZ, Len + .02, .06, 8.6, Asphalt * float(.92 + .14 * Hb(MX, 4.0)), 0, -Ang, Pt);
		for (int32 sg = -1; sg <= 1; sg += 2)
		{
			N.Box(MX + PX * .14 * sg, Top + .065, MZ + PZ * .14 * sg, Len + .02, .02, .12, 0xd9b83a, 0, -Ang, Pt);
			N.Box(MX + PX * 3.95 * sg, Top + .065, MZ + PZ * 3.95 * sg, Len + .02, .02, .15, 0xeeeae0, 0, -Ang, Pt);
		}
		// posts every 3 m and two galvanised pipe rails, with rust running down the fascia under each post
		for (int32 sg = -1; sg <= 1; sg += 2)
		{
			const FVector Out(PX * sg, 0, PZ * sg), Tan(AX, 0, AZ);
			for (int32 k = 0; k < 4; k++)
			{
				const double A = -Len / 2 + 1.5 + k * 3, PY = HA + (HB - HA) * (A / Len + .5);
				const double PCx = MX + AX * A + PX * 4.95 * sg, PCz = MZ + AZ * A + PZ * 4.95 * sg, CurbTop = PY + SlabT / 2 + .3;
				N.M.Surf = SurfConcrete;
				N.BoxC(PCx, CurbTop + .45, PCz, .26, .9, .26, C * 1.02f, 0, -Ang);
				const double R = Hb(PCx, PCz);
				if (R > .25)
				{
					const FVector T0(MX + AX * A + PX * HalfW * sg, PY + SlabT / 2 + .02, MZ + AZ * A + PZ * HalfW * sg);
					Streak(N, T0, Out, Tan, .5 + 1.1 * R, .07 + .05 * R, C * .97f, Rust, .45 + .45 * R);
				}
			}
			N.M.Surf = SurfSteel;
			const FLinearColor RailC = FMath::Lerp(Galv, Rust, float(.12 + .25 * Hb(MX, sg * 7.0)));
			for (const double RH : {.5, .88}) N.BoxC(MX + PX * 4.95 * sg, Top + .3 + RH, MZ + PZ * 4.95 * sg, Len + .02, .08, .08, RailC, 0, -Ang, Pt);
			// a deck drain every span, with a dark drip stain below it
			N.M.Surf = SurfConcrete;
			const FVector D0(MX + PX * HalfW * sg, H - SlabT / 2 - .1, MZ + PZ * HalfW * sg);
			Streak(N, D0, Out, Tan, 1.3, .12, C * .97f, FLinearColor(.22f, .21f, .19f), .55);
		}

		// ---- far: one deep slab, a solid parapet, the road
		if (int32(FMath::RoundToDouble((X - WBX0) / 12)) % 2 == 0)
		{
			FTownBatch& Fb = Ch.Far;
			const double FL = Len * 2, FMX = MX + AX * Len / 2, FMZ = MZ + AZ * Len / 2, FH = (HA + NewDeckH(XB + 12)) / 2;
			const double FPt = FMath::Atan2(NewDeckH(XB + 12) - HA, FL), FTop = FH + SlabT / 2;
			Fb.M.Surf = SurfConcrete;
			Fb.BoxC(FMX, FH - GirderD / 2, FMZ, FL + .02, SlabT + GirderD, HalfW * 2, C, 0, -Ang, FPt);
			for (int32 sg = -1; sg <= 1; sg += 2) Fb.BoxC(FMX + PX * 4.95 * sg, FTop + .6, FMZ + PZ * 4.95 * sg, FL + .02, 1.2, .3, C * 1.02f, 0, -Ang, FPt);
			Fb.M.Surf = SurfAsphalt;
			Fb.BoxC(FMX, FTop + .03, FMZ, FL + .02, .06, 8.6, Asphalt, 0, -Ang, FPt);
			Fb.Box(FMX, FTop + .065, FMZ, FL + .02, .02, .4, 0xd9b83a, 0, -Ang, FPt);
		}
	}

	// ================================================================ US-1: the bents (every 36 m) and the arch piers
	for (double X = WBX0; X <= WBX1; X += BridgeBentStep)
	{
		const double Z = ChainZ(X);
		if (LandH(X, Z) > -.5) continue;
		if (X > BridgeArchX0 + 1 && X < BridgeArchX1 - 1) continue;
		const FBrFrame F(X);
		const double CapTop = NewDeckH(X) - SlabT / 2 - GirderD, Floor = -FMath::Clamp(DepthAt(X, Z) + .5, 1.5, 12.0);
		const double Shift = (Hb(X, 1.0) - .5) * .2;
		const FLinearColor C = NewC * float(.88 + .14 * Hb(X, 9.0));
		FChunk& Ch = NewAt(X);
		const bool bPier = FMath::Abs(X - BridgeArchX0) < 1 || FMath::Abs(X - BridgeArchX1) < 1;
		for (int32 Lod = 0; Lod < 2; Lod++)
		{
			FTownBatch& B = Lod == 0 ? Ch.Near : Ch.Far;
			B.M.Surf = SurfConcrete;
			const FVector Cap = F.At(X, Z, CapTop - .45, 0, 0);
			B.BoxC(Cap.X, Cap.Y, Cap.Z, 2.0, .9, 11.4, C, 0, -F.Ang);
			if (bPier)
			{
				// the arch pier: a battered block from the bottom to above the tide, two tall columns up to the cap
				BandedBlock(B, F, X, Z, 2.6, 6.4, Floor, 2.6, C, Shift, true);
				for (int32 sg = -1; sg <= 1; sg += 2)
				{
					const FVector P = F.At(X, Z, (2.6 + CapTop - .9) / 2, 0, 4.4 * sg);
					B.BoxC(P.X, P.Y, P.Z, 1.8, CapTop - .9 - 2.6, 1.8, C, 0, -F.Ang);
				}
				if (Lod == 0) Barnacles(B, F, X, Z, 2.6, 6.4, Shift, 90, X);
				continue;
			}
			for (int32 sg = -1; sg <= 1; sg += 2)
			{
				const FVector P = F.At(X, Z, 0, 0, 4.5 * sg);
				const FBrFrame& PF = F;
				// each pile in its own frame offset across the line
				BandedBlock(B, PF, P.X, P.Z, .38, .38, Floor, CapTop - .9, C, Shift, false);
				if (Lod == 0) Barnacles(B, PF, P.X, P.Z, .38, .38, Shift, 26, X + sg * 13);
			}
			// tall bents on the ramps get X-bracing between the piles
			if (Lod == 0 && CapTop - .9 > 8)
				for (double Y0 = 2.2; Y0 + 4.5 < CapTop - 1.2; Y0 += 5)
					for (int32 d = -1; d <= 1; d += 2)
					{
						const FVector A = F.At(X, Z, Y0, 0, -4.5 * d), Bp = F.At(X, Z, Y0 + 4.2, 0, 4.5 * d);
						B.Tube3(A, Bp, .16, .16, 4, 0xc9c2b1);
					}
		}
	}

	// ================================================================ US-1: the channel arch (two ribs, spandrel columns)
	{
		const double S0 = BridgeArchX0 + 2.4, S1 = BridgeArchX1 - 2.4, Spring = 2.4;
		const double Crown = NewDeckH(ArchC) - SlabT / 2 - GirderD - .2;
		auto RibY = [&](double S) { const double T = 2 * S - 1; return Spring + (Crown - Spring) * (1 - T * T); };
		for (int32 Lod = 0; Lod < 2; Lod++)
		{
			FChunk& Ch = NewAt(ArchC);
			FTownBatch& B = Lod == 0 ? Ch.Near : Ch.Far;
			B.M.Surf = SurfConcrete;
			const int32 Seg = Lod == 0 ? 28 : 12;
			for (int32 sg = -1; sg <= 1; sg += 2)
			{
				const double Off = 3.6 * sg;
				for (int32 k = 0; k < Seg; k++)
				{
					const double SA = double(k) / Seg, SB = double(k + 1) / Seg;
					const double XA = FMath::Lerp(S0, S1, SA), XB = FMath::Lerp(S0, S1, SB);
					const FBrFrame F(XA);
					const FVector PA = F.At(XA, ChainZ(XA), RibY(SA), 0, Off), PB = F.At(XB, ChainZ(XB), RibY(SB), 0, Off);
					const FVector M = (PA + PB) / 2;
					const double L = FVector::Dist(PA, PB), Pt = FMath::Atan2(PB.Y - PA.Y, FMath::Sqrt(FMath::Square(PB.X - PA.X) + FMath::Square(PB.Z - PA.Z)));
					B.BoxC(M.X, M.Y - .55, M.Z, L + .05, 1.5, 1.4, NewC * float(.93 + .08 * Hb(XA, sg)), 0, -F.Ang, Pt);
				}
				// spandrel columns from the rib up to the girders, every 6 m (12 far)
				for (double X = S0 + 6; X < S1 - 3; X += Lod == 0 ? 6 : 12)
				{
					const double S = (X - S0) / (S1 - S0), Y0 = RibY(S), Y1 = NewDeckH(X) - SlabT / 2 - GirderD;
					if (Y1 - Y0 < .6) continue;
					const FBrFrame F(X);
					const FVector P = F.At(X, ChainZ(X), (Y0 + Y1) / 2, 0, Off);
					B.BoxC(P.X, P.Y, P.Z, .8, Y1 - Y0, .9, NewC * .96f, 0, -F.Ang);
				}
			}
			// a cross-beam tying the ribs together at each column line
			if (Lod == 0)
				for (double X = S0 + 6; X < S1 - 3; X += 12)
				{
					const double S = (X - S0) / (S1 - S0);
					const FBrFrame F(X);
					const FVector P = F.At(X, ChainZ(X), RibY(S) + .3, 0, 0);
					B.BoxC(P.X, P.Y, P.Z, .6, .6, 7.2, NewC * .92f, 0, -F.Ang);
				}
		}
	}

	// ================================================================ the old bridge: a viaduct of shallow barrel arches
	for (double X = OldBridgeX0; X < OldBridgeX1; X += 30)
	{
		const double XB = X + 30;
		const double Z = ChainZ(X) + OldBridgeDZ;
		if (LandH(X, Z) > -.5) continue;
		const bool bGapA = X > OldBridgeGap0 && X < OldBridgeGap1, bGapSpan = XB > OldBridgeGap0 && X < OldBridgeGap1;
		const FBrFrame F(X);
		const double DeckY = 5.0, SlabB = DeckY - .3, Floor = -FMath::Clamp(DepthAt(X, Z) + .5, 1.5, 10.0), Shift = (Hb(X, 3.0) - .5) * .25;
		const FLinearColor C = OldC * float(.84 + .2 * Hb(X, 5.0));
		FChunk& Ch = OldAt(X);
		for (int32 Lod = 0; Lod < 2; Lod++)
		{
			FTownBatch& B = Lod == 0 ? Ch.Near : Ch.Far;
			B.M.Surf = SurfConcrete;
			// the pier
			if (!bGapA)
			{
				BandedBlock(B, F, X, Z, .9, 3.4, Floor, SlabB, C, Shift, false);
				if (Lod == 0) Barnacles(B, F, X, Z, .9, 3.4, Shift, 60, X * 1.7);
			}
			if (bGapSpan) continue;
			// the span: deck slab, the spandrel faces down to the arch on each side, the arch soffit underneath
			const double XM = X + 15, ZM = ChainZ(XM) + OldBridgeDZ;
			const FBrFrame FM(XM);
			B.BoxC(XM, DeckY, ZM, 30.02, .6, 7.6, C, 0, -FM.Ang);
			const double SD = ShoreInfo(XM, ZM).D;
			const int32 N = Lod == 0 ? 14 : 6;
			const double SpringY = .9, CrownY = SlabB - .7, A0 = .9, A1 = 29.1;   // the arch between the piers' faces
			auto ArchY = [&](double U) { const double T = 2 * U - 1; return SpringY + (CrownY - SpringY) * FMath::Sqrt(FMath::Max(0.0, 1 - T * T)); };
			if (SD > 1)
			{
				for (int32 k = 0; k < N; k++)
				{
					const double UA = double(k) / N, UB = double(k + 1) / N;
					const double LA = FMath::Lerp(A0, A1, UA) - 15, LB = FMath::Lerp(A0, A1, UB) - 15, YA = ArchY(UA), YB = ArchY(UB);
					const FLinearColor CA = WaterlineCol(YA, C, Shift), CB2 = WaterlineCol(YB, C, Shift);
					for (int32 sg = -1; sg <= 1; sg += 2)
					{
						const double Off = 3.8 * sg;
						const FVector Out(FM.PX * sg, 0, FM.PZ * sg);
						// the spandrel face: from the arch's curve up to the slab
						const FVector P0 = FM.At(XM, ZM, YA, LA, Off), P1 = FM.At(XM, ZM, YB, LB, Off), Q1 = FM.At(XM, ZM, SlabB, LB, Off), Q0 = FM.At(XM, ZM, SlabB, LA, Off);
						B.TriCol(P0, P1, Q1, Out, CA, CB2, C);
						B.TriCol(P0, Q1, Q0, Out, CA, C, C);
					}
					// the soffit, facing down
					const FVector S0 = FM.At(XM, ZM, YA, LA, -3.8), S1 = FM.At(XM, ZM, YB, LB, -3.8), S2 = FM.At(XM, ZM, YB, LB, 3.8), S3 = FM.At(XM, ZM, YA, LA, 3.8);
					const FVector Tn = (S1 - S0).GetSafeNormal();
					FVector Dn = FVector::CrossProduct(Tn, FVector(FM.PX, 0, FM.PZ)).GetSafeNormal();
					if (Dn.Y > 0) Dn = -Dn;
					B.TriCol(S0, S1, S2, Dn, CA * .8f, CB2 * .8f, CB2 * .8f);
					B.TriCol(S0, S2, S3, Dn, CA * .8f, CB2 * .8f, CA * .8f);
				}
			}
			else
				for (int32 sg = -1; sg <= 1; sg += 2) { const FVector P = FM.At(XM, ZM, SlabB - .5, 0, 3.4 * sg); B.BoxC(P.X, P.Y, P.Z, 30.02, 1.0, .6, C * .95f, 0, -FM.Ang); }   // a plain girder over the shallows
			// the old balustrade: posts and a cap rail in concrete, rust from the rebar under the posts
			for (int32 sg = -1; sg <= 1; sg += 2)
			{
				if (Lod == 1) { const FVector P = FM.At(XM, ZM, DeckY + .75, 0, 3.6 * sg); B.BoxC(P.X, P.Y, P.Z, 30.02, .9, .28, C * 1.04f, 0, -FM.Ang); continue; }
				const FVector RailP = FM.At(XM, ZM, DeckY + 1.05, 0, 3.6 * sg);
				B.BoxC(RailP.X, RailP.Y, RailP.Z, 30.02, .22, .3, C * 1.06f, 0, -FM.Ang);
				const FVector Out(FM.PX * sg, 0, FM.PZ * sg), Tan(FM.AX, 0, FM.AZ);
				for (double A = -14; A <= 14.01; A += 2.5)
				{
					const FVector P = FM.At(XM, ZM, DeckY + .62, A, 3.6 * sg);
					B.BoxC(P.X, P.Y, P.Z, .24, .66, .24, C * float(.98 + .08 * Hb(A, X)), 0, -FM.Ang);
					const double R = Hb(P.X, P.Z + sg);
					if (R > .2) Streak(B, FM.At(XM, ZM, DeckY + .29, A, 3.8 * sg), Out, Tan, .6 + 1.6 * R, .08 + .06 * R, C, Rust, .5 + .45 * R);
				}
				// spalled patches where the rebar has blown the cover off
				for (int32 k = 0; k < 3; k++)
				{
					const double R = Hb(X + k * 7.0, sg * 2.0);
					if (R < .55) continue;
					const double A = -12 + 24 * Hb(X, k + sg * 3.0);
					Streak(B, FM.At(XM, ZM, SlabB - .2 - 1.2 * R, A, 3.8 * sg), Out, Tan, .35, .25, C, Rust * .7f, .8);
				}
			}
		}
	}
	// broken stubs where the old swing span was taken out
	for (double X = OldBridgeGap0 + 10; X < OldBridgeGap1; X += 30)
		for (int32 Lod = 0; Lod < 2; Lod++)
		{
			FTownBatch& B = Lod == 0 ? OldAt(X).Near : OldAt(X).Far;
			B.M.Surf = SurfConcrete;
			const FBrFrame F(X);
			const double Z = ChainZ(X) + OldBridgeDZ, Ht = 2.4 + Hb(X, 3) * 1.6;
			BandedBlock(B, F, X, Z, .9, 1.3, -2.5, Ht - .6, OldC * .85f, 0, true);
			if (Lod == 0) Barnacles(B, F, X, Z, .9, 1.3, 0, 30, X);
		}

	// ================================================================ the old Bahia Honda bridge: steel trusses, the road on top
	TArray<FChunk> TrussCh;
	if (const FTruss* Tr = BahiaHonda())
	{
		const double DX = Tr->X1 - Tr->X0, DZ = Tr->Z1 - Tr->Z0, Len = FMath::Sqrt(DX * DX + DZ * DZ), SpanL = Len / Tr->Spans;
		FBrFrame F(0);
		F.Ang = FMath::Atan2(DZ, DX); F.AX = DX / Len; F.AZ = DZ / Len; F.PX = -F.AZ; F.PZ = F.AX;
		constexpr int32 PerChunk = 4, Panels = 6;
		constexpr double BotY = 7.4, HalfT = 2.9, DeckHalf = 3.6;
		TrussCh.SetNum((Tr->Spans + PerChunk - 1) / PerChunk);
		for (int32 c = 0; c < TrussCh.Num(); c++)
		{
			const double T = FMath::Min(1.0, (c + .5) * PerChunk / Tr->Spans);
			TrussCh[c].X0 = TrussCh[c].X1 = Tr->X0 + DX * T;
			TrussCh[c].ZOff = Tr->Z0 + DZ * T - ChainZ(TrussCh[c].X0);
		}
		// the trusses stand taller toward the middle of the crossing, where the water is deepest; the road humps over them
		auto TrussH = [&](int32 Sp) { const double M = FMath::Abs((Sp + .5) / Tr->Spans - .5) * 2; return 7.0 + 3.2 * SS(.75, .2, M); };
		auto At = [&](double Along, double Y, double Off) { return F.At(Tr->X0, Tr->Z0, Y, Along, Off); };
		const FLinearColor Steel = Hex(0x54463a), SteelLt = Hex(0x6e5a48), RustC = Hex(0x7a4424);
		// a steel member between two points in the bridge's own frame (along, height) at one offset across
		auto Member = [&](FTownBatch& B, double A0, double Y0, double A1, double Y1, double Off, double Tk, double Wd, const FLinearColor& C)
		{
			const double L = FMath::Sqrt(FMath::Square(A1 - A0) + FMath::Square(Y1 - Y0));
			const FVector P = At((A0 + A1) / 2, (Y0 + Y1) / 2, Off);
			B.BoxC(P.X, P.Y, P.Z, L, Tk, Wd, C, 0, -F.Ang, FMath::Atan2(Y1 - Y0, A1 - A0));
		};
		auto Cross = [&](FTownBatch& B, double A, double Y, double HalfLen, double Tk, double Wd, const FLinearColor& C)
		{
			const FVector P = At(A, Y, 0);
			B.BoxC(P.X, P.Y, P.Z, Wd, Tk, HalfLen * 2, C, 0, -F.Ang);
		};
		for (int32 i = 0; i <= Tr->Spans; i++)
		{
			const double A = i * SpanL;
			const FVector Pp = At(A, 0, 0);
			FChunk& Ch = TrussCh[FMath::Min(i / PerChunk, TrussCh.Num() - 1)];
			const bool bWater = LandH(Pp.X, Pp.Z) < -.3;
			const double Floor = bWater ? -FMath::Clamp(DepthAt(Pp.X, Pp.Z) + .5, 1.5, 12.0) : LandH(Pp.X, Pp.Z) - .3, Shift = (Hb(A, 3.0) - .5) * .25;
			const FLinearColor PC = OldC * float(.82 + .2 * Hb(A, 5.0));
			for (int32 Lod = 0; Lod < 2; Lod++)
			{
				FTownBatch& B = Lod == 0 ? Ch.Near : Ch.Far;
				B.M.Surf = SurfConcrete;
				// the pier: a tall tapered-looking block with a wider footing and a cap
				BandedBlock(B, F, Pp.X, Pp.Z, 1.9, 4.6, Floor, .9, PC * .94f, Shift, true);
				BandedBlock(B, F, Pp.X, Pp.Z, 1.3, 3.8, .9, BotY - .5, PC, Shift, false);
				BandedBlock(B, F, Pp.X, Pp.Z, 1.6, 4.2, BotY - .5, BotY - .15, PC * 1.04f, Shift, true);
				if (Lod == 0 && bWater) Barnacles(B, F, Pp.X, Pp.Z, 1.9, 4.6, Shift, 90, A * 1.7);
				if (i == Tr->Spans || (i >= Tr->Gap0 && i < Tr->Gap1)) continue;
				// ---- the span from this pier to the next
				const double H = TrussH(i), TopY = BotY + H, PanL = SpanL / Panels;
				const float Tone = float(.86 + .22 * Hb(i, 9.0));
				for (int32 sg = -1; sg <= 1; sg += 2)
				{
					const double Off = sg * HalfT;
					Member(B, A, BotY, A + SpanL, BotY, Off, .5, .45, Steel * Tone);          // bottom chord
					Member(B, A, TopY, A + SpanL, TopY, Off, .55, .5, Steel * Tone);          // top chord
					for (int32 k = 0; k <= Panels; k++)
					{
						const double Ak = A + k * PanL;
						const bool bEnd = k == 0 || k == Panels;
						if (Lod == 0 || k % 2 == 0) Member(B, Ak + (bEnd ? (k == 0 ? .25 : -.25) : 0), BotY, Ak + (bEnd ? (k == 0 ? .25 : -.25) : 0) + .001, TopY, Off, bEnd ? .5 : .3, bEnd ? .5 : .34, (bEnd ? Steel : SteelLt) * Tone);
						if (k == Panels) continue;
						// the diagonals lean in toward the middle of the span (a Pratt truss); far away one in two is enough
						const bool bLeft = k < Panels / 2;
						if (Lod == 0 || k % 2 == 0)
							Member(B, Ak, bLeft ? TopY : BotY, Ak + PanL, bLeft ? BotY : TopY, Off, .22, .26, FMath::Lerp(SteelLt, RustC, float(Hb(i * 7.0 + k, sg + 3.0)) * .6f) * Tone);
					}
				}
				// floor beams, the top struts and the sway bracing between the two trusses
				for (int32 k = 0; k <= Panels && Lod == 0; k++)
				{
					const double Ak = A + k * PanL;
					Cross(B, Ak, BotY, HalfT, .4, .3, Steel * Tone);
					Cross(B, Ak, TopY - .1, HalfT, .35, .3, Steel * Tone);
					if (k == Panels) continue;
					for (int32 sg = -1; sg <= 1; sg += 2)
					{
						const FVector P0 = At(Ak, TopY - .2, -HalfT * sg), P1 = At(Ak + PanL, TopY - .2, HalfT * sg), Mid = (P0 + P1) / 2;
						B.BoxC(Mid.X, Mid.Y, Mid.Z, FMath::Sqrt(PanL * PanL + 4 * HalfT * HalfT), .12, .16, SteelLt * Tone, 0, -(F.Ang + sg * FMath::Atan2(2 * HalfT, PanL)));
					}
				}
				// the road over the top: slab on stringers, curbs and a pipe rail, sloping to meet the next span's height
				const double HA = i > 0 && !(i - 1 >= Tr->Gap0 && i - 1 < Tr->Gap1) ? (TrussH(i - 1) + H) / 2 : H;
				const double HB = i + 1 < Tr->Spans && !(i + 1 >= Tr->Gap0 && i + 1 < Tr->Gap1) ? (TrussH(i + 1) + H) / 2 : H;
				const double YA = BotY + HA + .75, YB = BotY + HB + .75;
				Member(B, A, YA, A + SpanL, YB, 0, .35, DeckHalf * 2, OldC * Tone);
				for (int32 sg = -1; sg <= 1; sg += 2)
				{
					Member(B, A, YA - .4, A + SpanL, YB - .4, sg * HalfT, .5, .4, Steel * Tone);
					Member(B, A, YA + .32, A + SpanL, YB + .32, sg * (DeckHalf - .15), .3, .3, OldC * Tone * .95f);
					if (Lod == 0)
					{
						Member(B, A, YA + 1.25, A + SpanL, YB + 1.25, sg * (DeckHalf - .15), .07, .07, Galv);
						for (int32 k = 0; k <= Panels * 2; k++)
						{
							const double Ak = A + k * PanL / 2, Yk = FMath::Lerp(YA, YB, double(k) / (Panels * 2));
							const FVector P = At(Ak, Yk + .85, sg * (DeckHalf - .15));
							B.BoxC(P.X, P.Y, P.Z, .1, .8, .1, Galv * .9f, 0, -F.Ang);
						}
					}
				}
				B.M.Surf = SurfAsphalt;
				Member(B, A, YA + .19, A + SpanL, YB + .19, 0, .04, DeckHalf * 2 - .9, Asphalt * Tone);
				// where a span was cut away the road just stops: a barrier across the broken end
				B.M.Surf = SurfConcrete;
				if (i + 1 == Tr->Gap0) Cross(B, A + SpanL - .3, YB + .7, DeckHalf, 1.0, .35, OldC * .9f);
				if (i == Tr->Gap1) Cross(B, A + .3, YA + .7, DeckHalf, 1.0, .35, OldC * .9f);
			}
		}
		// the approach at each end: an earth ramp faced in concrete, down from the road to the ground
		for (int32 e = 0; e < 2; e++)
		{
			const double A0 = e == 0 ? 0 : Len, Dir = e == 0 ? -1 : 1, Top = BotY + TrussH(e == 0 ? 0 : Tr->Spans - 1) + .75, Run = 70;
			FChunk& Ch = TrussCh[e == 0 ? 0 : TrussCh.Num() - 1];
			for (int32 Lod = 0; Lod < 2; Lod++)
			{
				FTownBatch& B = Lod == 0 ? Ch.Near : Ch.Far;
				B.M.Surf = SurfConcrete;
				const FVector Pe = At(A0 + Dir * Run, 0, 0);
				const double Gy = FMath::Max(.4, LandH(Pe.X, Pe.Z));
				const FVector Q[4] = {At(A0, Top + .17, -DeckHalf), At(A0, Top + .17, DeckHalf), At(A0 + Dir * Run, Gy, DeckHalf), At(A0 + Dir * Run, Gy, -DeckHalf)};
				const FVector Lo[4] = {At(A0, -1, -DeckHalf - 1.5), At(A0, -1, DeckHalf + 1.5), At(A0 + Dir * Run, -1, DeckHalf + 1.5), At(A0 + Dir * Run, -1, -DeckHalf - 1.5)};
				const FVector Up(0, 1, 0), Sd(F.PX, 0, F.PZ), Al(F.AX * -Dir, 0, F.AZ * -Dir);
				if (Dir < 0) { B.Quad3(Q[0], Q[1], Q[2], Q[3], Up, Asphalt); } else { B.Quad3(Q[3], Q[2], Q[1], Q[0], Up, Asphalt); }
				GradQuad(B, Lo[1], Lo[2], Q[2], Q[1], Sd, OldC * .8f, OldC);
				GradQuad(B, Lo[3], Lo[0], Q[0], Q[3], -Sd, OldC * .8f, OldC);
				GradQuad(B, Lo[0], Lo[1], Q[1], Q[0], Al, OldC * .8f, OldC);
			}
		}
	}

	// ---- commit every chunk as two components
	UMaterialInterface* Conc = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/KeysRun/Materials/M_KR_Concrete.M_KR_Concrete"));
	if (!Conc) Conc = AKeysWorld::VertexColorMaterial();
	int32 NearTris = 0, FarTris = 0, Made = 0;
	auto Commit = [&](TArray<FChunk>& Chs, const TCHAR* Tag)
	{
		for (int32 i = 0; i < Chs.Num(); i++)
		{
			FChunk& C = Chs[i];
			if (C.Near.Tris() == 0 && C.Far.Tris() == 0) continue;
			FBridgeChunk BC;
			BC.X0 = C.X0; BC.X1 = C.X1; BC.ZOff = C.ZOff;
			for (int32 Lod = 0; Lod < 2; Lod++)
			{
				FTownBatch& B = Lod == 0 ? C.Near : C.Far;
				UProceduralMeshComponent* PM = NewObject<UProceduralMeshComponent>(this, *FString::Printf(TEXT("Bridge%s%d_%s"), Tag, i, Lod == 0 ? TEXT("Near") : TEXT("Far")));
				PM->SetupAttachment(Root);
				PM->SetCollisionEnabled(ECollisionEnabled::NoCollision);
				PM->RegisterComponent();
				B.M.Commit(PM, 0, Conc);
				PM->SetVisibility(Lod == 1);
				BridgeComps.Add(PM);
				(Lod == 0 ? BC.Near : BC.Far) = PM;
				(Lod == 0 ? NearTris : FarTris) += B.Tris();
			}
			BridgeChunks.Add(BC);
			Made++;
		}
	};
	Commit(NewCh, TEXT("US1_"));
	Commit(OldCh, TEXT("Old_"));
	Commit(TrussCh, TEXT("BahiaHonda_"));
	UE_LOG(LogTemp, Log, TEXT("Keys Run: bridges built - %d chunks, %d tris near, %d tris far, material %s"), Made, NearTris, FarTris, *GetNameSafe(Conc));
}

void AKeysLandmarks::UpdateBridgeLod()
{
	APlayerCameraManager* Cam = UGameplayStatics::GetPlayerCameraManager(this, 0);
	if (!Cam) return;
	const FVector P = Cam->GetCameraLocation() / 100;   // game x = UE x, game z = UE y
	for (FBridgeChunk& C : BridgeChunks)
	{
		const double X = FMath::Clamp(P.X, C.X0, C.X1), Z = ChainZ(X) + C.ZOff;
		const double D = FMath::Sqrt(FMath::Square(P.X - X) + FMath::Square(P.Y - Z) + FMath::Square(P.Z * .5));
		const bool bNear = C.bNear ? D < NearDist + 25 : D < NearDist;
		if (bNear == C.bNear) continue;
		C.bNear = bNear;
		if (C.Near) C.Near->SetVisibility(bNear);
		if (C.Far) C.Far->SetVisibility(!bNear);
	}
}
