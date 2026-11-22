#include "KeysSea.h"
#include "KeysGeo.h"
#include "KeysGeoReal.h"
#include "Algo/Reverse.h"

using namespace KeysGeo;

namespace
{
	double SClamp(double V, double A, double B) { return V < A ? A : V > B ? B : V; }
	double SSmooth(double E0, double E1, double X) { const double T = SClamp((X - E0) / (E1 - E0), 0, 1); return T * T * (3 - 2 * T); }
	double SLerp(double A, double B, double T) { return A + (B - A) * T; }
	FLinearColor L3(const FLinearColor& A, const FLinearColor& B, double T) { return FLinearColor(float(SLerp(A.R, B.R, T)), float(SLerp(A.G, B.G, T)), float(SLerp(A.B, B.B, T)), 1.f); }

	// legacy hash2 (index.html:709)
	double Hash2(double X, double Z) { const double H = FMath::Sin(X * 127.1 + Z * 311.7) * 43758.5453; return H - FMath::FloorToDouble(H); }

	// relief.ts h32: murmur3's finaliser over three ints, identical to the JS Math.imul arithmetic
	double H32(int32 A, int32 B, int32 S)
	{
		uint32 H = (uint32(A) * 0x27d4eb2du) ^ (uint32(B) * 0x165667b1u) ^ (uint32(S) * 0x9e3779b1u);
		H = (H ^ (H >> 15)) * 0x85ebca6bu;
		H = (H ^ (H >> 13)) * 0xc2b2ae35u;
		H ^= H >> 16;
		return double(H) / 4294967296.0;
	}

	// smooth value noise on a lattice of Cell metres, 0..1 (H32 at the corners): what breaks up the sine patterns below,
	// which by themselves repeat every few hundred metres and show as stripes from high above
	double ValueNoise(double X, double Z, double Cell, int32 Salt)
	{
		const double FX = X / Cell, FZ = Z / Cell;
		const int32 IX = FMath::FloorToInt32(FX), IZ = FMath::FloorToInt32(FZ);
		double U = FX - IX, V = FZ - IZ;
		U = U * U * (3 - 2 * U); V = V * V * (3 - 2 * V);
		return SLerp(SLerp(H32(IX, IZ, Salt), H32(IX + 1, IZ, Salt), U), SLerp(H32(IX, IZ + 1, Salt), H32(IX + 1, IZ + 1, Salt), U), V);
	}

	struct FZoneW { double Hawk, Reef, Patch; };

	FZoneW ZoneWeights(double X, double DZ)
	{
		FZoneW W;
		W.Hawk = SSmooth(250, 700, DZ) * (1 - SSmooth(1150, 1300, DZ));
		W.Reef = SSmooth(1150, 1300, DZ) * (1 - SSmooth(1700, 1900, DZ));
		W.Patch = 0;
		for (const FHump& H : Humps())
			if (H.bPatch) W.Patch = FMath::Max(W.Patch, 1 - SSmooth(60, 190, FMath::Sqrt(FMath::Square(X - H.X) + FMath::Square(DZ - H.DZ))));
		return W;
	}

	// rock outcrops: 6-16 m wide, 1-5 m tall, terraced, lumpy, on 48 m cells
	double Outcrops(double X, double Z, const FZoneW& Zn)
	{
		const double P = .1 * Zn.Hawk + .36 * Zn.Reef + .45 * Zn.Patch;
		if (P < .005) return 0;
		constexpr double Cell = 48;
		const int32 IX = FMath::FloorToInt32(X / Cell), IZ = FMath::FloorToInt32(Z / Cell);
		double Best = 0;
		for (int32 j = -1; j <= 1; j++)
			for (int32 i = -1; i <= 1; i++)
			{
				const int32 CX = IX + i, CZ = IZ + j;
				if (H32(CX, CZ, 11) > P) continue;
				const double OX = (CX + .15 + .7 * H32(CX, CZ, 12)) * Cell, OZ = (CZ + .15 + .7 * H32(CX, CZ, 13)) * Cell;
				const double R = 6 + 10 * H32(CX, CZ, 14);
				const double DX = X - OX, DZ = Z - OZ, Dist = FMath::Sqrt(DX * DX + DZ * DZ);
				if (Dist > R * 1.4) continue;
				const double Ang = FMath::Atan2(DZ, DX);
				const double Q = Dist / (R * (1 + .22 * FMath::Sin(3 * Ang + 6.28 * H32(CX, CZ, 15)) + .12 * FMath::Sin(5 * Ang + 6.28 * H32(CX, CZ, 16))));
				if (Q >= 1) continue;
				const double Hh = 1.2 + 3.8 * H32(CX, CZ, 17) * H32(CX, CZ, 18);
				const double Prof = SSmooth(0, .45, 1 - Q);
				const double Tt = Prof * 3, Fl = FMath::FloorToDouble(Tt);
				const double Terrace = (Fl + SSmooth(.55, 1, Tt - Fl)) / 3;
				const double Lump = 1 + .2 * FMath::Sin(X * .85 + 1) * FMath::Sin(Z * .7) + .12 * FMath::Sin(X * 1.9 + Z * 1.3);
				Best = FMath::Max(Best, Hh * (.5 * Prof + .5 * Terrace) * Lump);
			}
		return Best;
	}

	// coral heads / bommies: 1.4-3.8 m wide domes, 0.7-2 m tall, on 14 m cells
	double CoralHeads(double X, double Z, const FZoneW& Zn)
	{
		const double P = .09 * Zn.Hawk + .5 * Zn.Reef + .7 * Zn.Patch;
		if (P < .005) return 0;
		constexpr double Cell = 14;
		const int32 IX = FMath::FloorToInt32(X / Cell), IZ = FMath::FloorToInt32(Z / Cell);
		double Best = 0;
		for (int32 j = -1; j <= 1; j++)
			for (int32 i = -1; i <= 1; i++)
			{
				const int32 CX = IX + i, CZ = IZ + j;
				if (H32(CX, CZ, 21) > P) continue;
				const double OX = (CX + .2 + .6 * H32(CX, CZ, 22)) * Cell, OZ = (CZ + .2 + .6 * H32(CX, CZ, 23)) * Cell;
				const double R = 1.4 + 2.4 * H32(CX, CZ, 24);
				const double DX = X - OX, DZ = Z - OZ, D2 = (DX * DX + DZ * DZ) / (R * R);
				if (D2 >= 1) continue;
				const double Hh = .7 + 1.3 * H32(CX, CZ, 25);
				const double Ang = FMath::Atan2(DZ, DX);
				Best = FMath::Max(Best, Hh * FMath::Pow(1 - D2, .6) * (.88 + .12 * FMath::Sin(Ang * 6 + 6.28 * H32(CX, CZ, 26))));
			}
		return Best;
	}

	double RawRelief(double X, double Z, double DZ)
	{
		const FZoneW Zn = ZoneWeights(X, DZ);
		double R = FMath::Max(Outcrops(X, Z, Zn), CoralHeads(X, Z, Zn));
		// spur-and-groove on the reef front, ~30 m apart along the chain
		const double Sg = SSmooth(1290, 1340, DZ) * (1 - SSmooth(1560, 1680, DZ));
		if (Sg > 0)
		{
			const FVector2D Tn = KeysSea::ChainTangent(X);
			const double S = X * Tn.X + Z * Tn.Y;
			const double Phase = (S / 30) * PI * 2 + 1.6 * FMath::Sin(S / 170) + .5 * FMath::Sin(DZ / 55);
			const double Spur = FMath::Pow(.5 + .5 * FMath::Sin(Phase), 1.7);
			R += Sg * 1.7 * (Spur - .38);
		}
		// wall terraces: the 3.4 -> 45 m drop as a staircase of shelves and risers
		const double Wm = SSmooth(1440, 1480, DZ) * (1 - SSmooth(1630, 1670, DZ));
		if (Wm > 0) R += Wm * 1.5 * FMath::Sin((DZ / 34) * PI * 2 + .011 * X + 1.3 * FMath::Sin(X / 140));
		// low sand waves over the soft bottom
		const double Sand = SSmooth(300, 600, DZ);
		if (Sand > 0) R += Sand * .15 * FMath::Sin(.19 * X + .6 * FMath::Sin(.05 * Z)) * FMath::Sin(.15 * Z);
		return R;
	}

	// ---- biome palette (heightfield.ts). three.js vertex colors are linear, so these are used as is.
	const FLinearColor Sand(.93f, .86f, .66f), Grass(.30f, .47f, .24f), Rubble(.56f, .49f, .37f), LedgeRock(.20f, .25f, .30f), Deep(.03f, .08f, .18f);
	const FLinearColor CoralWarm[4] = {FLinearColor(.85f, .55f, .30f), FLinearColor(.90f, .62f, .25f), FLinearColor(.80f, .45f, .35f), FLinearColor(.70f, .55f, .30f)};
	const FLinearColor CoralMuted[4] = {FLinearColor(.50f, .42f, .50f), FLinearColor(.42f, .46f, .44f), FLinearColor(.46f, .40f, .46f), FLinearColor(.38f, .46f, .44f)};

	// the soft bottoms the bigger map brought: Florida Bay's pale marl mud in its basins, low limestone hard bottom with
	// its turf of algae, and the grey marl of a dredged canal
	const FLinearColor Mud(.40f, .42f, .33f), HardRock(.47f, .43f, .33f), Marl(.15f, .175f, .19f);

	// KeysReef.cpp's own two noises, so the bottom is coloured as what grows on it: its hard bottom (the algae beds,
	// with the turtle grass between them) and the sand channels it keeps coral out of
	double HardBottomN(double X, double Z, double Freq, double Seed)
	{
		const double N = FMath::Sin(X * Freq + Seed) * FMath::Cos(Z * Freq * 1.3 - Seed * .7) * .5
			+ FMath::Sin(X * Freq * 2.7 - Z * Freq * 1.9 + Seed * 3.1) * .35 + FMath::Sin(Z * Freq * 4.3 + X * Freq * .6) * .15;
		return SSmooth(-.15, .3, N);
	}
	double CoralGroundN(double X, double DZ)
	{
		const double N = FMath::Sin(X * .013 + DZ * .021) * .5 + FMath::Sin(X * .027 - DZ * .011) * .5;
		return SSmooth(-.35, .05, N);
	}
	// the Sombrero dive site (KeysReef.cpp SombX, SombDZ, SombR): coral all over it, in grooves of its own
	double SombreroK(double X, double DZ)
	{
		const double SX = KeysGeo::RealMap() ? -1800.0 : 250.0;
		static const double SDZ = KeysGeo::RealMap() ? KeysGeo::DZAt(-1800, 2760) : 1380.0;   // (the map is chosen before anything asks)
		return 1 - SSmooth(300, 380, FMath::Sqrt(FMath::Square(X - SX) + FMath::Square(DZ - SDZ)));
	}

	// ---- tidal cuts between the islands (current.ts buildCuts)
	struct FCut { double X, Width, Speed; };
	const TArray<FCut>& Cuts()
	{
		static TArray<FCut> C;
		static bool bBuilt = false;
		if (bBuilt) return C;
		bBuilt = true;
		TArray<FVector2D> Spans;
		for (const FIsland& I : Islands())
			if (!I.bMainland && FMath::Abs(I.Z - ChainZ(I.X)) < 420) Spans.Add(FVector2D(I.X - I.A, I.X + I.A));
		Spans.Sort([](const FVector2D& A, const FVector2D& B) { return A.X < B.X; });
		TArray<FVector2D> Merged;
		for (const FVector2D& S : Spans)
		{
			if (Merged.Num() && S.X <= Merged.Last().Y) Merged.Last().Y = FMath::Max(Merged.Last().Y, S.Y);
			else Merged.Add(S);
		}
		for (int32 i = 1; i < Merged.Num(); i++)
		{
			const double A = Merged[i - 1].Y, B = Merged[i].X, W = B - A;
			if (W < 30 || W > 1400) continue;
			C.Add({(A + B) / 2, W, FMath::Min(1.6, .9 * (120 / W))});
		}
		return C;
	}

	FVector2D CutJetAt(double X, double Z, double T)
	{
		const double Phase = FMath::Sin((T / 960) * PI * 2);
		if (Phase == 0 || !Cuts().Num()) return FVector2D::ZeroVector;
		const double DZ = KeysGeo::DZAt(X, Z);
		const FVector2D N = KeysSea::ChainNormal(X);
		double VX = 0, VZ = 0;
		for (const FCut& C : Cuts())
		{
			const double Sx = FMath::Max(60.0, C.Width * .8), Du = (X - C.X) / Sx;
			if (Du * Du > 9) continue;
			const double Dv = DZ / 520;
			const double S = C.Speed * Phase * FMath::Exp(-Du * Du) * FMath::Exp(-Dv * Dv);
			VX += N.X * S; VZ += N.Y * S;
		}
		return FVector2D(VX, VZ);
	}

	uint32 FMix32(uint32 H)
	{
		H ^= H >> 16; H *= 0x85ebca6bu;
		H ^= H >> 13; H *= 0xc2b2ae35u;
		H ^= H >> 16;
		return H;
	}
}

namespace KeysSea
{
	double ReliefAt(double X, double Z, double D)
	{
		const double DZ = KeysGeo::DZAt(X, Z);
		const double Gate = SSmooth(150, 300, DZ) * SSmooth(2.6, 4.2, D);
		if (Gate <= 0) return 0;
		const double R = RawRelief(X, Z, DZ) * Gate;
		if (R >= 0)
		{
			const double Cap = FMath::Min(FMath::Max(0.0, D - ReliefMinTop), .7 * D);
			return Cap > 1e-6 ? Cap * FMath::Tanh(R / Cap) : 0;
		}
		const double G = FMath::Min(1.1, FMath::Max(0.0, .3 * (D - 2)));
		return G > 1e-6 ? -G * FMath::Tanh(-R / G) : 0;
	}

	// The depth the floor is drawn at. The depth grid is 10 m and a canal 24 m, so between its walls the grid alone gives a
	// trough that climbs toward each wall; a dredged canal is flat from wall to wall (and on under the wall's foot,
	// where the made ground hides the way back up to the grid).
	static double FloorDepth(double X, double Z)
	{
		const double D = DepthFast(X, Z);
		if (!KeysGeo::RealMap()) return D;
		const double CN = KeysGeo::CanalDist(X, Z);
		if (CN > 4) return D;
		return SLerp(FMath::Max(D, KeysGeo::CanalDepth), D, SSmooth(1.5, 4, CN));
	}

	double SeafloorHeightAt(double X, double Z)
	{
		const double D = FloorDepth(X, Z);
		return -(D - ReliefAt(X, Z, D));
	}

	void SeafloorSample(double X, double Z, double& OutH, double& OutRel)
	{
		const double D = FloorDepth(X, Z);
		OutRel = ReliefAt(X, Z, D);
		OutH = -(D - OutRel);
	}

	double DredgedAt(double X, double Z)
	{
		if (!KeysGeo::RealMap()) return 0;
		double M = 0;
		const double CN = KeysGeo::CanalDist(X, Z);
		if (CN < 4) M = 1 - SSmooth(1, 4, CN);
		const double WD = KeysGeoReal::WayOutDist(X, Z);
		if (WD < 8) M = FMath::Max(M, 1 - SSmooth(0, 8, WD));
		return M;
	}

	FBottom BottomAt(double X, double Z, double D)
	{
		FBottom B;
		const double DZ = KeysGeo::DZAt(X, Z);
		if (!KeysGeo::RealMap())
		{
			// the classic map keeps the web game's two hard-edged rules (heightfield.ts) as they were
			const double Nc = FMath::Sin(X * .05) * FMath::Cos(Z * .043) + FMath::Sin(X * .013 + Z * .021);
			B.Grass = SClamp(Nc * .6 + .4 - (D < 1 ? .6 : 0), 0, 1) * (DZ < 0 ? .9 : .55);
			if (D < 20)
			{
				if (DZ > 1150 && DZ < 1750) B.Reef = 1;
				else for (const FHump& H : Humps())
					if (H.bPatch && FMath::Sqrt(FMath::Square(X - H.X) + FMath::Square(DZ - H.DZ)) < 130) { B.Reef = 1; break; }
			}
			return B;
		}
		// reef: the band inside the drop-off and the patch reefs, where KeysReef grows coral - less of it in the sand
		// channels between
		double Patch = 0;
		for (const FHump& H : Humps())
			if (H.bPatch) Patch = FMath::Max(Patch, 1 - SSmooth(100, 160, FMath::Sqrt(FMath::Square(X - H.X) + FMath::Square(DZ - H.DZ))));
		const double Band = SSmooth(1100, 1200, DZ) * (1 - SSmooth(1700, 1800, DZ));
		const double Coral = Band > 0 ? FMath::Max(CoralGroundN(X, DZ), SombreroK(X, DZ)) : 0;
		B.Reef = FMath::Max(Band * SLerp(.3, 1, Coral), Patch) * (1 - SSmooth(16, 22, D));
		// hard bottom: low rock in beds, on the ocean side from the shore out to the reef and thinner on the bay side
		B.Hard = HardBottomN(X, Z, .017, 7.9) * SSmooth(.7, 1.6, D) * (1 - SSmooth(14, 20, D)) * SLerp(.45, 1, SSmooth(-80, 80, DZ)) * (1 - B.Reef);
		// seagrass: meadows from knee deep to as far down as the light gets, thick in the bay and thinner off the ocean
		// side, between the hard bottom's beds; the crowns of the banks are bare
		// (the web game's two sines, as they were, with two octaves of plain noise over them: by themselves they repeat
		// every 250 m, which shows as stripes across a bay seen from high above)
		const double N = FMath::Sin(X * .05) * FMath::Cos(Z * .043) + FMath::Sin(X * .013 + Z * .021) + .8 * (ValueNoise(X, Z, 190, 31) - .5) + 1.7 * (ValueNoise(X, Z, 760, 32) - .5);
		B.Grass = SClamp(N * .6 + .4 - .6 * (1 - SSmooth(.8, 1.3, D)), 0, 1) * SLerp(.9, .55, SSmooth(-60, 60, DZ)) * (1 - SSmooth(9, 14, D)) * (1 - .5 * B.Hard);
		// mud: the bay's basins, deeper than its banks and out of the tide that scours the channels
		B.Mud = SSmooth(40, -220, DZ) * SSmooth(1.0, 1.8, D) * (1 - SSmooth(3.4, 4.6, D)) * (.7 + .3 * FMath::Sin(X * .0031 + 1.3) * FMath::Cos(Z * .0027));
		// a dredged canal and the way cut from its mouth: bare grey marl
		B.Marl = DredgedAt(X, Z);
		// (nothing grows on it)
		B.Grass *= 1 - B.Marl; B.Hard *= 1 - B.Marl; B.Reef *= 1 - B.Marl;
		return B;
	}

	FLinearColor SeafloorColorAt(double X, double Z, double D, double Shade, double Rel, bool bGrain)
	{
		const FBottom B = BottomAt(X, Z, D);
		FLinearColor C = L3(Sand, Mud, B.Mud * .75);
		C = L3(C, Grass, B.Grass);
		C = L3(C, HardRock, B.Hard * .5);
		if (B.Reef > 0)
		{
			// (the speckle of swatches is per vertex: a mesh too coarse to carry it gets their mean instead)
			const double Hh = bGrain ? Hash2(X, Z) : .5;
			const FLinearColor* Sw = D < 10 ? CoralMuted : CoralWarm;
			const FLinearColor Swatch = bGrain ? Sw[FMath::FloorToInt32(Hash2(Z, X) * 4) % 4] : L3(L3(Sw[0], Sw[1], .5), L3(Sw[2], Sw[3], .5), .5);
			const FLinearColor& Base = D >= 15 ? LedgeRock : Rubble;
			C = L3(C, L3(Base, Swatch, bGrain ? SSmooth(.45, .85, Hh) : .35), .75 * B.Reef);
		}
		if (Rel > .12)
		{
			const double RockT = SSmooth(.12, .8, Rel);
			const double Patch = Hash2(FMath::FloorToDouble(X / 3), FMath::FloorToDouble(Z / 3));
			const FLinearColor Base = D >= 12 ? LedgeRock : L3(Rubble, LedgeRock, .35 + .4 * Patch);
			const int32 Pi = FMath::Clamp(FMath::FloorToInt32(Patch * 3.99), 0, 3);
			const FLinearColor Live = D < 10 ? CoralMuted[Pi] : CoralWarm[Pi];
			C = L3(C, L3(Base, Live, SSmooth(.9, 2.2, Rel) * .55), RockT * .85);
		}
		else if (Rel < -.25) C = L3(C, Sand, SSmooth(.25, .9, -Rel) * .6);
		// under and beside the mangroves the bottom is dark peat and leaf litter, and the water over it is stained like tea
		if (const double Tan = KeysGeo::MangroveNear(X, Z); Tan > 0) C = L3(C, FLinearColor(.15f, .10f, .045f), Tan * .85 * SClamp((6 - D) / 4, 0, 1));
		// a dredged canal has a bare bottom of grey marl
		if (B.Marl > 0) C = L3(C, Marl, .94 * B.Marl);
		C = L3(C, Deep, SClamp((D - 13) / 22, 0, 1));
		const double J = (bGrain ? .94 + Hash2(X * .7, Z * .3) * .12 : 1.0) * Shade;
		return FLinearColor(float(C.R * J), float(C.G * J), float(C.B * J), 1.f);
	}

	void SeafloorMean(double X, double Z, double SizeX, double SizeZ, int32 N, double& OutH, FLinearColor& OutC)
	{
		if (N <= 0)
		{
			const double D = FloorDepth(X, Z);
			OutH = -D;
			OutC = SeafloorColorAt(X, Z, D, 1, 0, false);
			return;
		}
		constexpr double E = 4;   // slopes are judged over about the fine chunks' own spacing
		double SumH = 0, SumR = 0, SumG = 0, SumB = 0;
		for (int32 j = 0; j < N; j++)
			for (int32 i = 0; i < N; i++)
			{
				const double PX = X + ((i + .5) / N - .5) * SizeX, PZ = Z + ((j + .5) / N - .5) * SizeZ;
				double H, Rel;
				SeafloorSample(PX, PZ, H, Rel);
				double K = 1;
				if (Rel != 0)
				{
					// the chunks darken their steep faces, and a lumpy bottom catches less sun than a flat one
					const double SX = (SeafloorHeightAt(PX + E, PZ) - H) / E, SZ = (SeafloorHeightAt(PX, PZ + E) - H) / E;
					K = (1 - FMath::Min(.35, (FMath::Abs(SX) + FMath::Abs(SZ)) * .7)) * SLerp(1, 1 / FMath::Sqrt(1 + SX * SX + SZ * SZ), .7);
				}
				const FLinearColor C = SeafloorColorAt(PX, PZ, -H, K, Rel, false);
				SumH += H; SumR += C.R; SumG += C.G; SumB += C.B;
			}
		const double Inv = 1.0 / (N * N);
		OutH = SumH * Inv;
		OutC = FLinearColor(float(SumR * Inv), float(SumG * Inv), float(SumB * Inv), 1.f);
	}

	FVector2D ChainTangent(double X)
	{
		const double Slope = FMath::Tan(KeysGeo::ChainAngle(X)), Inv = 1 / FMath::Sqrt(1 + Slope * Slope);
		return FVector2D(Inv, Slope * Inv);
	}

	FVector2D ChainNormal(double X)
	{
		const FVector2D T = ChainTangent(X);
		return FVector2D(-T.Y, T.X);
	}

	FVector2D SteadyCurrentAt(double X, double Z, double T)
	{
		const FVector2D Tn = ChainTangent(X);
		const double Gulf = 1.15 * OffshoreF(X, Z);
		const double U = (KeysGeo::DZAt(X, Z) - 300) / 1100;
		const double Exposure = SClamp(FMath::Exp(-(U * U)) * (1 - OffshoreF(X, Z)), 0, 1);
		const double Tide = .42 * Exposure * FMath::Sin((T / 960) * PI * 2);
		const double S = Gulf + Tide;
		const FVector2D Jet = CutJetAt(X, Z, T);
		return FVector2D(Tn.X * S + Jet.X, Tn.Y * S + Jet.Y);
	}

	FVector2D SurgeAt(double X, double Z, double T, double Depth)
	{
		const double Decay = FMath::Exp(-FMath::Max(0.0, Depth) / SurgeDecayDepth);
		if (Decay < .01) return FVector2D::ZeroVector;
		const FVector2D N = ChainNormal(X);
		const double Phase = (T / SurgePeriod) * PI * 2 - (X * N.X + Z * N.Y) * (PI * 2 / 34);
		const double S = SurgeMaxSpeed * Decay * FMath::Sin(Phase);
		return N * S;
	}

	FVector2D CurrentAt(double X, double Z, double T, double Depth) { return SteadyCurrentAt(X, Z, T) + SurgeAt(X, Z, T, Depth); }

	// ---- a test: which residential canals can a boat leave?
	// From the middle of every canal the water the hull floats in (the depth grid the boat itself reads, MinD or more)
	// is flooded on a 5 m grid, up to Reach from the canal. A canal is open if the flood gets to open water: 1.5 m of
	// it, 60 m from any shore and from any canal. For one that is shut, the cheapest way through whatever water there
	// is (keeping off the banks) to open water is traced and logged as the "w" line <phase>_canals.txt wants.
	void LogCanalWays(double MinD)
	{
		if (!KeysGeo::RealMap()) { UE_LOG(LogTemp, Log, TEXT("KRCanal: the classic map has no canals")); return; }
		constexpr double G = 5, Reach = 1400;
		auto CK = [](int32 I, int32 J) { return (int64(I) << 32) ^ int64(uint32(J)); };
		auto Open = [](double X, double Z) { return DepthFast(X, Z) >= 1.5 && KeysGeoReal::Shore(X, Z) >= 60 && KeysGeo::CanalDist(X, Z) > 60; };
		const TArray<KeysGeoReal::FCanal>& Cs = KeysGeoReal::Canals();
		TMap<int64, int32> Seen;        // cell -> the flood (canal index) that reached it
		TArray<int8> Result;            // per canal: 1 open, 0 shut
		Result.Init(-1, Cs.Num());
		int32 NShut = 0;
		for (int32 c = 0; c < Cs.Num(); c++)
		{
			const TArray<FVector2D>& P = Cs[c].Pts;
			if (P.Num() < 2) continue;
			const FVector2D S0 = P[P.Num() / 2];
			const int32 I0 = FMath::RoundToInt32(S0.X / G), J0 = FMath::RoundToInt32(S0.Y / G);
			if (const int32* Had = Seen.Find(CK(I0, J0))) { Result[c] = Result[*Had]; if (!Result[c]) NShut++; continue; }   // one network, flooded already
			TArray<FIntPoint> Q;
			Q.Add(FIntPoint(I0, J0));
			Seen.Add(CK(I0, J0), c);
			bool bOpen = false;
			for (int32 h = 0; h < Q.Num() && !bOpen; h++)
			{
				const FIntPoint A = Q[h];
				static const int32 DI[4] = {1, -1, 0, 0}, DJ[4] = {0, 0, 1, -1};
				for (int32 k = 0; k < 4; k++)
				{
					const int32 I = A.X + DI[k], J = A.Y + DJ[k];
					const double X = I * G, Z = J * G;
					if (FMath::Square(X - S0.X) + FMath::Square(Z - S0.Y) > Reach * Reach) continue;
					if (const int32* Had = Seen.Find(CK(I, J)))
					{
						if (*Had != c && Result[*Had] == 1) { bOpen = true; break; }   // joined a network that is open
						continue;
					}
					if (DepthFast(X, Z) < MinD) continue;
					Seen.Add(CK(I, J), c);
					if (Open(X, Z)) { bOpen = true; break; }
					Q.Add(FIntPoint(I, J));
				}
			}
			Result[c] = bOpen ? 1 : 0;
			if (bOpen) continue;
			NShut++;
			// the way out: Dijkstra over any water at all from the cells this flood reached, dear near a bank and in the shoals
			TMap<int64, float> Cost;
			TMap<int64, FIntPoint> From;
			TArray<TPair<float, FIntPoint>> Heap;
			auto Less = [](const TPair<float, FIntPoint>& A, const TPair<float, FIntPoint>& B) { return A.Key < B.Key; };
			for (const FIntPoint& A : Q) { Cost.Add(CK(A.X, A.Y), 0.f); Heap.HeapPush(TPair<float, FIntPoint>(0.f, A), Less); }
			FIntPoint End(0, 0);
			bool bFound = false;
			while (Heap.Num() && !bFound)
			{
				TPair<float, FIntPoint> T;
				Heap.HeapPop(T, Less);
				const FIntPoint A = T.Value;
				if (T.Key > Cost.FindRef(CK(A.X, A.Y)) + 1e-3f) continue;
				static const int32 DI[8] = {1, -1, 0, 0, 1, 1, -1, -1}, DJ[8] = {0, 0, 1, -1, 1, -1, 1, -1};
				for (int32 k = 0; k < 8; k++)
				{
					const int32 I = A.X + DI[k], J = A.Y + DJ[k];
					const double X = I * G, Z = J * G;
					if (FMath::Square(X - S0.X) + FMath::Square(Z - S0.Y) > Reach * Reach) continue;
					const double Sh = KeysGeoReal::Shore(X, Z);
					if (Sh < 6 && KeysGeo::CanalDist(X, Z) > 0) continue;   // land, or water's very edge
					const float Step = float((k < 4 ? 1.0 : 1.414) * (Sh < 14 ? 4.0 : 1.0) * (DepthFast(X, Z) < MinD ? 1.5 : 1.0));
					const float Nc = T.Key + Step;
					float* Have = Cost.Find(CK(I, J));
					if (Have && *Have <= Nc) continue;
					Cost.Add(CK(I, J), Nc);
					From.Add(CK(I, J), A);
					if (Open(X, Z)) { End = FIntPoint(I, J); bFound = true; break; }
					Heap.HeapPush(TPair<float, FIntPoint>(Nc, FIntPoint(I, J)), Less);
				}
			}
			if (!bFound) { UE_LOG(LogTemp, Log, TEXT("KRCanal shut: canal %d (middle %.0f, %.0f, phase %d) has no %.1f m water to the open sea, and no water at all leads there within %.0f m"), c, S0.X, S0.Y, KeysGeoReal::PhaseAt(S0.X, S0.Y), MinD, Reach); continue; }
			TArray<FVector2D> Path;
			double Shoalest = 1e9, Len = 0;
			for (FIntPoint A = End;;)
			{
				Path.Add(FVector2D(A.X * G, A.Y * G));
				Shoalest = FMath::Min(Shoalest, DepthFast(A.X * G, A.Y * G));
				const FIntPoint* Pv = From.Find(CK(A.X, A.Y));
				if (!Pv) break;
				A = *Pv;
			}
			Algo::Reverse(Path);
			// thin it: a point every 25 m or so, and both ends
			FString Line = TEXT("w 16");
			FVector2D Last = Path[0];
			for (int32 k = 0; k < Path.Num(); k++)
			{
				if (k) Len += (Path[k] - Path[k - 1]).Size();
				if (k == 0 || k == Path.Num() - 1 || (Path[k] - Last).Size() >= 25) { Line += FString::Printf(TEXT(" %.1f %.1f"), Path[k].X, Path[k].Y); Last = Path[k]; }
			}
			UE_LOG(LogTemp, Log, TEXT("KRCanal shut: canal %d (middle %.0f, %.0f, phase %d): no %.1f m water to the open sea. A way of %.0f m from %.0f, %.0f to %.0f, %.0f (shoalest now %.2f m) would open it: %s"),
				c, S0.X, S0.Y, KeysGeoReal::PhaseAt(S0.X, S0.Y), MinD, Len, Path[0].X, Path[0].Y, Path.Last().X, Path.Last().Y, Shoalest, *Line);
		}
		UE_LOG(LogTemp, Log, TEXT("KRCanal %d canals, %d with no %.1f m water out to the open sea"), Cs.Num(), NShut, MinD);
	}

	double HashCell(uint32 Seed, int32 CX, int32 CZ, int32 Salt)
	{
		uint32 H = FMix32(Seed);
		H = FMix32(H ^ ((uint32(CX) + 0x9e3779b9u) * 0x85ebca6bu));
		H = FMix32(H ^ ((uint32(CZ) + 0x9e3779b9u) * 0xc2b2ae35u));
		H = FMix32(H ^ ((uint32(Salt) + 0x9e3779b9u) * 0x27d4eb2fu));
		return double(H) / 4294967296.0;
	}

	double FMulberry::operator()()
	{
		S += 0x6d2b79f5u;
		uint32 T = (S ^ (S >> 15)) * (1u | S);
		T = (T + ((T ^ (T >> 7)) * (61u | T))) ^ T;
		return double(T ^ (T >> 14)) / 4294967296.0;
	}
}
