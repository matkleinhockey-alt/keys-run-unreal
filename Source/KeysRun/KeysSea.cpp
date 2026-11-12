#include "KeysSea.h"
#include "KeysGeo.h"

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

	bool IsReefy(double X, double DZ, double D)
	{
		if (D >= 20) return false;
		if (DZ > 1150 && DZ < 1750) return true;
		for (const FHump& H : Humps())
			if (H.bPatch && FMath::Sqrt(FMath::Square(X - H.X) + FMath::Square(DZ - H.DZ)) < 130) return true;
		return false;
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
		const double DZ = Z - ChainZ(X);
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
		const double DZ = Z - ChainZ(X);
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

	double SeafloorHeightAt(double X, double Z)
	{
		const double D = DepthFast(X, Z);
		return -(D - ReliefAt(X, Z, D));
	}

	void SeafloorSample(double X, double Z, double& OutH, double& OutRel)
	{
		const double D = DepthFast(X, Z);
		OutRel = ReliefAt(X, Z, D);
		OutH = -(D - OutRel);
	}

	FLinearColor SeafloorColorAt(double X, double Z, double D, double Shade, double Rel)
	{
		const double DZ = Z - ChainZ(X);
		const double N = FMath::Sin(X * .05) * FMath::Cos(Z * .043) + FMath::Sin(X * .013 + Z * .021);
		FLinearColor C = L3(Sand, Grass, SClamp(N * .6 + .4 - (D < 1 ? .6 : 0), 0, 1) * (DZ < 0 ? .9 : .55));
		if (IsReefy(X, DZ, D))
		{
			const double Hh = Hash2(X, Z);
			const FLinearColor* Sw = D < 10 ? CoralMuted : CoralWarm;
			const FLinearColor& Swatch = Sw[FMath::FloorToInt32(Hash2(Z, X) * 4) % 4];
			const FLinearColor& Base = D >= 15 ? LedgeRock : Rubble;
			C = L3(C, L3(Base, Swatch, SSmooth(.45, .85, Hh)), .75);
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
		C = L3(C, Deep, SClamp((D - 13) / 22, 0, 1));
		const double J = (.94 + Hash2(X * .7, Z * .3) * .12) * Shade;
		return FLinearColor(float(C.R * J), float(C.G * J), float(C.B * J), 1.f);
	}

	FVector2D ChainTangent(double X)
	{
		const double Slope = .000024 * X, Inv = 1 / FMath::Sqrt(1 + Slope * Slope);
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
		const double U = (Z - ChainZ(X) - 300) / 1100;
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
