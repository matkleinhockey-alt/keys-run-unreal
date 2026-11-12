#include "KeysReef.h"
#include "KeysGeo.h"
#include "KeysSea.h"
#include "KeysMesh.h"
#include "KeysWorld.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "StaticMeshAttributes.h"
#include "MeshDescription.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/Material.h"
#include "Kismet/GameplayStatics.h"
#include "Camera/PlayerCameraManager.h"
#include "Engine/World.h"

using namespace KeysGeo;

namespace
{
	// ---- world/reef/constants.ts
	constexpr uint32 ReefSeed = 20240817;
	constexpr double ChunkSize = 50;
	constexpr int32 ResidentR = 2;
	constexpr double LodNearMax = 14, LodMidMax = 26;
	constexpr int32 SlotsPerAttr = 4096;
	enum EAttr { PRESENCE = 0, OFFSET_X, OFFSET_Z, ROT_Y, SCALE, COLOR, TILT_X, TILT_Z, SCALE_X, SCALE_Y, SCALE_Z };
	constexpr double SombX = 250, SombDZ = 1380, SombR = 380, SombGroove = 34;
	constexpr double MicroPatchP = .07, PatchFull = 60, PatchFade = 110;

	enum ESp { Elkhorn, Staghorn, Brain, Star, SeaFan, SeaPlume, Barrel, Tube, Encrusting, Seagrass, NumSp };

	struct FSpDef
	{
		bool bReef;
		double D[4];   // depth trapezoid
		uint32 Lo, Hi;
		double S0, S1, Footprint, Flex;
		int32 Candidates;
		int32 SaltBase;
	};
	const FSpDef Sp[NumSp] = {
		{true, {.8, 2, 5.5, 7.5}, 0x9c3a0a, 0xe8841f, .6, 1.3, 1.7, 0, 220, 1000000},
		{true, {2.5, 4.5, 10, 13.5}, 0xc9704a, 0xf0b89a, .6, 1.4, 1.6, 0, 260, 2000000},
		{true, {5, 8, 18, 28}, 0x7a3f16, 0xb8752e, .7, 1.7, 1.8, 0, 90, 3000000},
		{true, {6, 9, 22, 32}, 0x6a4c14, 0x9c7c3c, .7, 1.9, 2.0, 0, 90, 4000000},
		{true, {2, 4, 16, 24}, 0x7a3a94, 0xb088d0, .6, 1.4, 1.1, .34, 160, 5000000},
		{true, {2, 4, 14, 22}, 0x6c8a2e, 0xb8d868, .6, 1.3, .9, .72, 130, 6000000},
		{true, {10, 15, 35, 55}, 0xa8280f, 0xe85a28, .7, 1.6, 1.3, 0, 40, 7000000},
		{true, {8, 12, 28, 45}, 0x8c48b0, 0xe8905c, .6, 1.4, 1.0, .08, 40, 8000000},
		{true, {2, 4, 20, 30}, 0x7cae3c, 0xd4f088, .5, 1.2, 1.3, 0, 140, 9000000},
		{false, {.5, 1.0, 3.4, 5.2}, 0x2f7a3a, 0x5cae52, .7, 1.4, .5, 1, 900, 10000000},
	};

	double RClamp(double V, double A, double B) { return V < A ? A : V > B ? B : V; }
	double RLerp(double A, double B, double T) { return A + (B - A) * T; }
	double RSmooth(double V, double Lo, double Hi) { const double T = RClamp((V - Lo) / (Hi - Lo), 0, 1); return T * T * (3 - 2 * T); }
	FLinearColor RHex(uint32 H) { return FLinearColor::FromSRGBColor(FColor(uint8(H >> 16), uint8(H >> 8), uint8(H))); }

	double Trapezoid(double D, const double* T)
	{
		if (D <= T[0] || D >= T[3]) return 0;
		if (D < T[1]) return (D - T[0]) / (T[1] - T[0]);
		if (D <= T[2]) return 1;
		return (T[3] - D) / (T[3] - T[2]);
	}

	int32 Fold(int32 Species, int32 Attr, int32 Slot) { return Sp[Species].SaltBase + Attr * SlotsPerAttr + Slot; }
	double H(int32 CX, int32 CZ, int32 Salt) { return KeysSea::HashCell(ReefSeed, CX, CZ, Salt); }

	double PatchFactor(double X, double DZ)
	{
		double Best = 0;
		for (const FHump& Hm : Humps())
		{
			if (!Hm.bPatch) continue;
			const double D = FMath::Sqrt(FMath::Square(X - Hm.X) + FMath::Square(DZ - Hm.DZ));
			const double F = D <= PatchFull ? 1 : D >= PatchFade ? 0 : 1 - (D - PatchFull) / (PatchFade - PatchFull);
			Best = FMath::Max(Best, F);
		}
		return Best;
	}

	// zoneAt, reduced to what the reef asks of it (creeks and bridge cuts never hold reef)
	enum class EZ : uint8 { Other, Hawk, Reef };
	EZ ReefZone(double D, double DZ)
	{
		if (D < 2.6 || DZ < 0 || D >= 45) return EZ::Other;
		return DZ > 1250 ? EZ::Reef : EZ::Hawk;
	}

	double SandChannel(double X, double DZ)
	{
		if (FMath::Sqrt(FMath::Square(X - SombX) + FMath::Square(DZ - SombDZ)) < SombR)
		{
			const double Groove = FMath::Sin((X / SombGroove) * PI * 2) * .5 + .5;
			return RSmooth(Groove, .32, .56);
		}
		const double N = FMath::Sin(X * .013 + DZ * .021) * .5 + FMath::Sin(X * .027 - DZ * .011) * .5;
		return RSmooth(N, -.35, .05);
	}

	// sombrero 1, coffins 2, delta 3
	int32 DiveSite(double X, double DZ, double& T)
	{
		T = 0;
		const double Sd = FMath::Sqrt(FMath::Square(X - SombX) + FMath::Square(DZ - SombDZ));
		if (Sd < SombR) { T = 1 - Sd / SombR; return 1; }
		for (const FHump& Hm : Humps())
		{
			const int32 Id = Hm.Name == TEXT("Coffins Patch") ? 2 : Hm.Name == TEXT("Delta Shoal") ? 3 : 0;
			if (!Id) continue;
			const double D = FMath::Sqrt(FMath::Square(X - Hm.X) + FMath::Square(DZ - Hm.DZ));
			if (D < PatchFade) { T = 1 - D / PatchFade; return Id; }
		}
		return 0;
	}

	double SiteMul(int32 Site, int32 S, double T)
	{
		if (Site == 1) return S == Elkhorn || S == Staghorn ? 1 + .4 * T : 1 + .1 * T;
		if (Site == 2) return S == SeaFan || S == SeaPlume ? 1 + .55 * T : 1 + .3 * T;
		if (Site == 3)
		{
			if (S == SeaFan || S == SeaPlume || S == Tube || S == Barrel) return FMath::Max(.3, 1 - .6 * T);
			return FMath::Max(.5, 1 - .35 * T);
		}
		return 1;
	}

	double Suitability(int32 S, double X, double Z, int32 CX, int32 CZ)
	{
		const double D = DepthFast(X, Z);
		const double DepthM = Trapezoid(D, Sp[S].D);
		if (DepthM <= 0) return 0;
		const double DZ = Z - ChainZ(X);
		const EZ Zone = ReefZone(D, DZ);
		const double Patch = PatchFactor(X, DZ);
		if (!Sp[S].bReef) return (Zone == EZ::Reef || Patch > .15) ? 0 : DepthM;
		const double Micro = Zone == EZ::Hawk && H(CX, CZ, 20000001) < MicroPatchP ? 1 : 0;
		const double Gate = Zone == EZ::Reef ? 1 : FMath::Max(Patch, Micro);
		if (Gate <= 0) return 0;
		const double Sand = SandChannel(X, DZ);
		if (Sand <= 0) return 0;
		double T;
		const int32 Site = DiveSite(X, DZ, T);
		return RClamp(DepthM * Gate * Sand * SiteMul(Site, S, T), 0, 1);
	}

	void PlaceSpecies(int32 S, int32 CX, int32 CZ, const TArray<AKeysReef::FInst>* Obstacles, TArray<AKeysReef::FInst>& Out)
	{
		const double X0 = CX * ChunkSize, Z0 = CZ * ChunkSize;
		for (int32 Slot = 0; Slot < Sp[S].Candidates; Slot++)
		{
			const double X = X0 + H(CX, CZ, Fold(S, OFFSET_X, Slot)) * ChunkSize;
			const double Z = Z0 + H(CX, CZ, Fold(S, OFFSET_Z, Slot)) * ChunkSize;
			const double BaseScale = RLerp(Sp[S].S0, Sp[S].S1, H(CX, CZ, Fold(S, SCALE, Slot)));
			if (Obstacles)
			{
				bool bHit = false;
				const double CardR = Sp[S].Footprint * .5 * BaseScale;
				for (const AKeysReef::FInst& O : *Obstacles)
				{
					const double ObR = Sp[O.Species].Footprint * .5 * FMath::Max(O.SX, O.SZ);
					if (FMath::Sqrt(FMath::Square(O.X - X) + FMath::Square(O.Z - Z)) < (CardR + ObR) * .4) { bHit = true; break; }
				}
				if (bHit) continue;
			}
			const double Suit = Suitability(S, X, Z, CX, CZ);
			if (Suit <= 0) continue;
			if (H(CX, CZ, Fold(S, PRESENCE, Slot)) >= Suit) continue;
			const double RotRoll = H(CX, CZ, Fold(S, ROT_Y, Slot));
			AKeysReef::FInst I;
			I.Species = S;
			I.X = X; I.Z = Z; I.Y = KeysSea::SeafloorHeightAt(X, Z);
			I.ColorT = H(CX, CZ, Fold(S, COLOR, Slot));
			I.TiltX = RLerp(-.12, .12, H(CX, CZ, Fold(S, TILT_X, Slot)));
			I.TiltZ = RLerp(-.12, .12, H(CX, CZ, Fold(S, TILT_Z, Slot)));
			I.RotY = RotRoll * PI * 2;
			if (S == SeaFan)
			{
				// fans face the surge, broadside across the reef
				const FVector2D N = KeysSea::ChainNormal(X);
				I.RotY = FMath::Atan2(N.X, N.Y) + RLerp(-.35, .35, RotRoll);
			}
			I.SX = BaseScale * RLerp(.82, 1.18, H(CX, CZ, Fold(S, SCALE_X, Slot)));
			I.SY = BaseScale * RLerp(.82, 1.18, H(CX, CZ, Fold(S, SCALE_Y, Slot)));
			I.SZ = BaseScale * RLerp(.82, 1.18, H(CX, CZ, Fold(S, SCALE_Z, Slot)));
			Out.Add(I);
		}
	}

	// ================================================================ geometry (world/reef/geometry.ts), built in the
	// three.js frame (y up), converted to Unreal's (x, z, y) when committed
	struct FGeo
	{
		TArray<FVector> P;
		TArray<double> K;          // vertex mottle (gray)
		TArray<FVector2D> UV;
		TArray<int32> T;
		void Append(const FGeo& O) { const int32 B = P.Num(); P.Append(O.P); K.Append(O.K); UV.Append(O.UV); for (int32 i : O.T) T.Add(B + i); }
	};

	struct FM4
	{
		// row-major 3x4 affine
		double M[3][4] = {{1, 0, 0, 0}, {0, 1, 0, 0}, {0, 0, 1, 0}};
		static FM4 Translate(double X, double Y, double Z) { FM4 R; R.M[0][3] = X; R.M[1][3] = Y; R.M[2][3] = Z; return R; }
		static FM4 RotY(double A) { FM4 R; const double C = FMath::Cos(A), S = FMath::Sin(A); R.M[0][0] = C; R.M[0][2] = S; R.M[2][0] = -S; R.M[2][2] = C; return R; }
		static FM4 RotZ(double A) { FM4 R; const double C = FMath::Cos(A), S = FMath::Sin(A); R.M[0][0] = C; R.M[0][1] = -S; R.M[1][0] = S; R.M[1][1] = C; return R; }
		FM4 operator*(const FM4& B) const
		{
			FM4 R;
			for (int32 i = 0; i < 3; i++)
			{
				for (int32 j = 0; j < 4; j++)
				{
					double V = 0;
					for (int32 k = 0; k < 3; k++) V += M[i][k] * B.M[k][j];
					R.M[i][j] = V + (j == 3 ? M[i][3] : 0);
				}
			}
			return R;
		}
		FVector Apply(const FVector& P) const
		{
			return FVector(M[0][0] * P.X + M[0][1] * P.Y + M[0][2] * P.Z + M[0][3], M[1][0] * P.X + M[1][1] * P.Y + M[1][2] * P.Z + M[1][3], M[2][0] * P.X + M[2][1] * P.Y + M[2][2] * P.Z + M[2][3]);
		}
	};

	double HashN(int32 I, double Seed) { const double Hh = FMath::Sin(I * 12.9898 + Seed * 78.233) * 43758.5453; return Hh - FMath::FloorToDouble(Hh); }

	void Paint(FGeo& G, double YMin, double YMax, double Seed, bool bBase)
	{
		const double Span = FMath::Max(1e-4, YMax - YMin);
		G.K.SetNum(G.P.Num());
		for (int32 i = 0; i < G.P.Num(); i++)
		{
			const FVector& V = G.P[i];
			const double N = HashN(FMath::FloorToInt32(V.X * 9) + FMath::FloorToInt32(V.Y * 11) * 17 + FMath::FloorToInt32(V.Z * 9) * 23, Seed);
			double K = .82 + N * .36;
			if (bBase) K *= .52 + RClamp((V.Y - YMin) / (Span * .35), 0, 1) * .48;
			G.K[i] = K;
		}
	}

	FGeo Cylinder(double RTop, double RBot, double Hh, int32 Radial, bool bCaps = true)
	{
		FGeo G;
		for (int32 r = 0; r <= 1; r++)
			for (int32 i = 0; i <= Radial; i++)
			{
				const double A = 2 * PI * i / Radial, R = r ? RTop : RBot;
				G.P.Add(FVector(FMath::Sin(A) * R, r * Hh, FMath::Cos(A) * R));
				G.UV.Add(FVector2D(double(i) / Radial, r));
			}
		const int32 W = Radial + 1;
		for (int32 i = 0; i < Radial; i++) G.T.Append({i, i + 1, W + i, i + 1, W + i + 1, W + i});
		if (bCaps)
			for (int32 r = 0; r <= 1; r++)
			{
				const int32 C = G.P.Num();
				G.P.Add(FVector(0, r * Hh, 0));
				G.UV.Add(FVector2D(.5, r));
				const int32 B = G.P.Num();
				for (int32 i = 0; i <= Radial; i++) { const double A = 2 * PI * i / Radial, R = r ? RTop : RBot; G.P.Add(FVector(FMath::Sin(A) * R, r * Hh, FMath::Cos(A) * R)); G.UV.Add(FVector2D(.5, r)); }
				for (int32 i = 0; i < Radial; i++) G.T.Append({C, B + i, B + i + 1});
			}
		return G;
	}

	// a sphere at the resolution of three.js IcosahedronGeometry(1, detail)
	FGeo Sphere(int32 Detail)
	{
		const int32 Seg = Detail == 0 ? 10 : Detail == 1 ? 18 : 30, Rings = Detail == 0 ? 6 : Detail == 1 ? 11 : 18;
		FGeo G;
		for (int32 j = 0; j <= Rings; j++)
			for (int32 i = 0; i <= Seg; i++)
			{
				const double Th = PI * j / Rings, Ph = 2 * PI * i / Seg;
				G.P.Add(FVector(FMath::Sin(Th) * FMath::Cos(Ph), FMath::Cos(Th), FMath::Sin(Th) * FMath::Sin(Ph)));
				G.UV.Add(FVector2D(double(i) / Seg, double(j) / Rings));
			}
		for (int32 j = 0; j < Rings; j++)
			for (int32 i = 0; i < Seg; i++)
			{
				const int32 A = j * (Seg + 1) + i, B = A + 1, C = A + Seg + 1, D = C + 1;
				G.T.Append({A, B, C, B, D, C});
			}
		return G;
	}

	// smooth value noise (trilinear between hashed lattice points), so displaced boulders stay round instead of faceted
	double SmoothN(const FVector& P, double Seed)
	{
		const FVector F(FMath::Frac(P.X), FMath::Frac(P.Y), FMath::Frac(P.Z));
		const int32 X0 = FMath::FloorToInt32(P.X), Y0 = FMath::FloorToInt32(P.Y), Z0 = FMath::FloorToInt32(P.Z);
		const FVector U = F * F * (FVector(3) - 2 * F);
		auto H = [&](int32 i, int32 j, int32 k) { return HashN((X0 + i) + (Y0 + j) * 57 + (Z0 + k) * 113, Seed); };
		const double A = FMath::Lerp(FMath::Lerp(H(0, 0, 0), H(1, 0, 0), U.X), FMath::Lerp(H(0, 1, 0), H(1, 1, 0), U.X), U.Y);
		const double B = FMath::Lerp(FMath::Lerp(H(0, 0, 1), H(1, 0, 1), U.X), FMath::Lerp(H(0, 1, 1), H(1, 1, 1), U.X), U.Y);
		return FMath::Lerp(A, B, U.Z);
	}

	void Displace(FGeo& G, double Amp, double Freq, double Seed)
	{
		for (FVector& V : G.P)
		{
			const double N = SmoothN(V * Freq, Seed) * .7 + SmoothN(V * Freq * 2.7 + FVector(5.3), Seed) * .3;
			V *= 1 + (N - .5) * 2.4 * Amp;
		}
	}

	FGeo Boulder(int32 Detail, double Sy, double Amp, double Freq, double Seed, bool bDisplaceAtZero = false)
	{
		FGeo G = Sphere(Detail);
		for (FVector& V : G.P) V.Y *= Sy;
		if (Detail > 0 || bDisplaceAtZero) Displace(G, Amp, Freq, Seed);
		Paint(G, -Sy, Sy, Seed, true);
		return G;
	}

	FGeo Lathe(const TArray<FVector2D>& Prof, int32 Radial)
	{
		FGeo G;
		for (int32 i = 0; i <= Radial; i++)
		{
			const double A = 2 * PI * i / Radial;
			for (int32 j = 0; j < Prof.Num(); j++)
			{
				G.P.Add(FVector(FMath::Sin(A) * Prof[j].X, Prof[j].Y, FMath::Cos(A) * Prof[j].X));
				G.UV.Add(FVector2D(double(i) / Radial, double(j) / (Prof.Num() - 1)));
			}
		}
		const int32 N = Prof.Num();
		for (int32 i = 0; i < Radial; i++)
			for (int32 j = 0; j < N - 1; j++)
			{
				const int32 A = i * N + j, B = A + 1, C = A + N, D = C + 1;
				G.T.Append({A, C, B, B, C, D});
			}
		return G;
	}

	FGeo Branching(uint32 Seed, int32 Depth, int32 Factor, int32 Radial, double BaseLen, double BaseR, double Spread, double FlatX, double FlatZ, double LenFall, double RFall, double TipFlare, double DecayChance)
	{
		KeysSea::FMulberry Rng(Seed);
		FGeo Out;
		TFunction<void(const FM4&, double, double, int32, int32, bool)> Grow = [&](const FM4& M, double Len, double R, int32 D, int32 Bf, bool bRoot)
		{
			FGeo Seg = Cylinder(R * TipFlare, R, Len, Radial, false);
			if (D <= 0)
			{
				// a rounded tip instead of a flat cut
				const double Rt = R * TipFlare;
				const int32 W0 = Seg.P.Num();
				const int32 NRg = 3;
				for (int32 j = 1; j <= NRg; j++)
				{
					const double A = PI * .5 * j / NRg;
					for (int32 i = 0; i <= Radial; i++)
					{
						const double Ph = 2 * PI * i / Radial, Rr = Rt * FMath::Cos(A);
						Seg.P.Add(FVector(FMath::Sin(Ph) * Rr, Len + Rt * FMath::Sin(A) * .8, FMath::Cos(Ph) * Rr));
						Seg.UV.Add(FVector2D(double(i) / Radial, 1));
					}
				}
				const int32 Wd = Radial + 1;
				for (int32 j = 0; j < NRg; j++)
					for (int32 i = 0; i < Radial; i++)
					{
						const int32 A0 = (j == 0 ? Wd : W0 + (j - 1) * Wd) + i, B0 = W0 + j * Wd + i;
						Seg.T.Append({A0, A0 + 1, B0, A0 + 1, B0 + 1, B0});
					}
			}
			for (FVector& V : Seg.P) { V.X *= FlatX; V.Z *= FlatZ; }
			Paint(Seg, 0, Len, Seed + (Depth - D) * 97, bRoot);
			for (FVector& V : Seg.P) V = M.Apply(V);
			Out.Append(Seg);
			if (D <= 0) return;
			const FM4 Tip = M * FM4::Translate(0, Len, 0);
			for (int32 i = 0; i < Bf; i++)
			{
				const double Yaw = (double(i) / Bf) * PI * 2 + Rng() * 1.1;
				const double Tilt = Spread * (.55 + Rng() * .5);
				const FM4 Mm = Tip * FM4::RotY(Yaw) * FM4::RotZ(Tilt);
				const int32 Next = FMath::Max(1, Bf - (Rng() < DecayChance ? 1 : 0));
				Grow(Mm, Len * LenFall, R * RFall, D - 1, Next, false);
			}
		};
		Grow(FM4(), BaseLen, BaseR, Depth, Factor, true);
		return Out;
	}

	FGeo Card(double W, double Hh, int32 BendSeg = 1, double Bend = 0)
	{
		FGeo G;
		for (int32 j = 0; j <= BendSeg; j++)
			for (int32 i = 0; i <= 1; i++)
			{
				const double T = double(j) / BendSeg;
				G.P.Add(FVector((i - .5) * W, T * Hh, Bend * T * T));
				G.UV.Add(FVector2D(i, T));
			}
		for (int32 j = 0; j < BendSeg; j++)
		{
			const int32 A = j * 2, B = A + 1, C = A + 2, D = A + 3;
			G.T.Append({A, B, C, B, D, C});
		}
		G.K.Init(1, G.P.Num());
		return G;
	}

	FGeo SeagrassGeo(int32 Blades)
	{
		FGeo Out;
		for (int32 i = 0; i < Blades; i++)
		{
			const double Hh = .42 + ((i * 7) % 5) * .05;
			FGeo B = Card(.1, Hh, 3, .16 + (i % 3) * .07);
			const double Rot = (double(i) / Blades) * PI * 2 + (i % 2) * .4;
			const double Ang = (double(i) / Blades) * PI * 2;
			const double R = Blades > 1 ? .06 + ((i * 3) % 4) * .035 : 0;
			const FM4 M = FM4::Translate(FMath::Cos(Ang) * R, 0, FMath::Sin(Ang) * R) * FM4::RotY(Rot);
			for (FVector& V : B.P) V = M.Apply(V);
			Out.Append(B);
		}
		return Out;
	}

	FGeo GeoFor(int32 S, int32 Lod)
	{
		switch (S)
		{
		case Elkhorn:
			if (Lod == 2) return Card(1.3, 1.3);
			return Branching(0xe1f0a, Lod == 0 ? 2 : 1, 2, Lod == 0 ? 10 : 6, .95, .12, .46, 2.6, .34, .88, .92, 1.35, .4);
		case Staghorn:
			if (Lod == 2) return Card(1.3, 1.3);
			return Branching(0x57a6, Lod == 0 ? 3 : 2, 4, Lod == 0 ? 8 : 5, .5, .09, .6, 1, 1, .82, .86, .7, .15);
		case Brain: return Boulder(2 - Lod, .56, .06, 1.6, 11);
		case Star: return Boulder(2 - Lod, .72, .1, 2.2, 29);
		case Encrusting: return Boulder(Lod == 0 ? 1 : 0, .24, .14, 4, 47);
		case SeaFan: return Card(1.5, 1.7);
		case SeaPlume:
		{
			if (Lod == 2) return Card(.5, 1.3);
			FGeo A = Card(.5, 1.3), B = Card(.5, 1.3);
			for (FVector& V : B.P) V = FVector(V.Z, V.Y, -V.X);
			A.Append(B);
			return A;
		}
		case Barrel:
		{
			FGeo G = Lathe({{0, 0}, {.52, .04}, {.78, .32}, {.74, .74}, {.52, .94}, {.46, 1}}, Lod == 0 ? 10 : Lod == 1 ? 6 : 4);
			Paint(G, 0, 1, 91, true);
			return G;
		}
		case Tube:
		{
			const int32 Radial = Lod == 0 ? 8 : Lod == 1 ? 5 : 4, Count = Lod == 0 ? 4 : Lod == 1 ? 3 : 1;
			KeysSea::FMulberry Rng(73);
			FGeo Out;
			for (int32 i = 0; i < Count; i++)
			{
				FGeo G = Lathe({{0, 0}, {.22, .02}, {.26, .85}, {.21, 1}, {.17, 1}}, Radial);
				Paint(G, 0, 1, 73 + i * 97, true);
				const double Hh = .55 + Rng() * .55;
				const double Ang = Rng() * PI * 2, R = Rng() * .22;
				for (FVector& V : G.P) V = FVector(V.X + FMath::Cos(Ang) * R, V.Y * Hh, V.Z + FMath::Sin(Ang) * R);
				Out.Append(G);
			}
			return Out;
		}
		case Seagrass:
			if (Lod == 2) return Card(.1, 1, 1, .1);
			return SeagrassGeo(Lod == 0 ? 6 : 3);
		default: return FGeo();
		}
	}

	bool IsCard(int32 S, int32 Lod) { return S == SeaFan || S == SeaPlume || S == Seagrass || (Lod == 2 && (S == Elkhorn || S == Staghorn)); }
	float CardKind(int32 S) { return S == Seagrass ? 0.f : S == SeaFan ? 1.f : 2.f; }

	// three.js frame -> Unreal local (x, z, y), meters, with smooth per-vertex normals from the faces
	void ToMesh(const FGeo& G, FKeysMesh& M)
	{
		TArray<FVector> Nrm;
		Nrm.Init(FVector::ZeroVector, G.P.Num());
		for (int32 i = 0; i + 2 < G.T.Num(); i += 3)
		{
			const FVector& A = G.P[G.T[i]], &B = G.P[G.T[i + 1]], &C = G.P[G.T[i + 2]];
			const FVector Fn = FVector::CrossProduct(B - A, C - A);
			Nrm[G.T[i]] += Fn; Nrm[G.T[i + 1]] += Fn; Nrm[G.T[i + 2]] += Fn;
		}
		// weld by position (cylinder seams, branch joints) so the coral shades as one smooth surface
		{
			TMap<FIntVector, FVector> Acc;
			auto KeyOf = [](const FVector& P) { return FIntVector(FMath::RoundToInt(P.X * 2000), FMath::RoundToInt(P.Y * 2000), FMath::RoundToInt(P.Z * 2000)); };
			for (int32 i = 0; i < G.P.Num(); i++) Acc.FindOrAdd(KeyOf(G.P[i])) += Nrm[i].GetSafeNormal();
			for (int32 i = 0; i < G.P.Num(); i++)
			{
				const FVector Sum = Acc.FindRef(KeyOf(G.P[i]));
				// keep a hard edge where the faces meeting here disagree a lot (a flat base against a side)
				const FVector Own = Nrm[i].GetSafeNormal();
				const FVector Sm = Sum.GetSafeNormal();
				Nrm[i] = FVector::DotProduct(Own, Sm) > .5 ? Sm : Own;
			}
		}
		for (FVector& N : Nrm) N = N.GetSafeNormal();
		auto U = [](const FVector& P) { return FVector(P.X, P.Z, P.Y); };
		for (int32 i = 0; i + 2 < G.T.Num(); i += 3)
		{
			const int32 Ix[3] = {G.T[i], G.T[i + 1], G.T[i + 2]};
			FVector Fn = U(FVector::CrossProduct(G.P[Ix[1]] - G.P[Ix[0]], G.P[Ix[2]] - G.P[Ix[0]]));
			// Unreal's front face: Dot(Cross(B-A, C-A), N) < 0; keep each face's winding consistent with its normal
			const FVector NA = Nrm[Ix[0]].IsNearlyZero() ? Fn : U(Nrm[Ix[0]]);
			const FVector Pa = U(G.P[Ix[0]]), Pb = U(G.P[Ix[1]]), Pc = U(G.P[Ix[2]]);
			const bool bFlip = FVector::DotProduct(FVector::CrossProduct(Pb - Pa, Pc - Pa), NA + U(Nrm[Ix[1]]) + U(Nrm[Ix[2]])) > 0;
			const int32 Order[3] = {Ix[0], bFlip ? Ix[2] : Ix[1], bFlip ? Ix[1] : Ix[2]};
			for (int32 k = 0; k < 3; k++)
			{
				const int32 V = Order[k];
				const float Kk = G.K.IsValidIndex(V) ? float(G.K[V]) : 1.f;
				const int32 I = M.AddVert(U(G.P[V]), U(Nrm[V]), FLinearColor(Kk, Kk, Kk, 1));
				M.UV[I] = G.UV.IsValidIndex(V) ? G.UV[V] : FVector2D::ZeroVector;
				M.T.Add(I);
			}
		}
	}
}

AKeysReef::AKeysReef()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickGroup = TG_PostUpdateWork;
	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent = Root;
}

UStaticMesh* AKeysReef::MakeMesh(const FKeysMesh& M, UMaterialInterface* Mat, const TCHAR* Name)
{
	FMeshDescription MD;
	FStaticMeshAttributes Attr(MD);
	Attr.Register();
	TVertexAttributesRef<FVector3f> Pos = Attr.GetVertexPositions();
	TVertexInstanceAttributesRef<FVector3f> Nrm = Attr.GetVertexInstanceNormals();
	TVertexInstanceAttributesRef<FVector4f> Col = Attr.GetVertexInstanceColors();
	TVertexInstanceAttributesRef<FVector2f> UVs = Attr.GetVertexInstanceUVs();
	UVs.SetNumChannels(1);
	const FPolygonGroupID Grp = MD.CreatePolygonGroup();
	Attr.GetPolygonGroupMaterialSlotNames()[Grp] = FName(TEXT("Mat"));
	TArray<FVertexInstanceID> Inst;
	Inst.SetNum(M.V.Num());
	for (int32 i = 0; i < M.V.Num(); i++)
	{
		const FVertexID V = MD.CreateVertex();
		Pos[V] = FVector3f(M.V[i]);
		const FVertexInstanceID I = MD.CreateVertexInstance(V);
		Nrm[I] = FVector3f(M.N[i]);
		Col[I] = FVector4f(M.C[i].R, M.C[i].G, M.C[i].B, 1);
		UVs.Set(I, 0, FVector2f(M.UV[i]));
		Inst[i] = I;
	}
	for (int32 i = 0; i + 2 < M.T.Num(); i += 3) MD.CreateTriangle(Grp, {Inst[M.T[i]], Inst[M.T[i + 1]], Inst[M.T[i + 2]]});
	UStaticMesh* SM = NewObject<UStaticMesh>(this, Name);
	SM->GetStaticMaterials().Add(FStaticMaterial(Mat, FName(TEXT("Mat"))));
	UStaticMesh::FBuildMeshDescriptionsParams P;
	P.bFastBuild = true;
	P.bMarkPackageDirty = false;
	P.bBuildSimpleCollision = false;
	SM->BuildFromMeshDescriptions({&MD}, P);
	Meshes.Add(SM);
	return SM;
}

void AKeysReef::BeginPlay()
{
	Super::BeginPlay();
	UMaterialInterface* Solid = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/KeysRun/Materials/M_KR_Reef.M_KR_Reef"));
	UMaterialInterface* CardM = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/KeysRun/Materials/M_KR_ReefCard.M_KR_ReefCard"));
	if (!Solid) Solid = AKeysWorld::VertexColorMaterial();
	if (CardM) CardMID = UMaterialInstanceDynamic::Create(CardM, this);
	for (int32 S = 0; S < NumSp; S++)
		for (int32 Lod = 0; Lod < 3; Lod++)
		{
			FKeysMesh M;
			const FGeo G = GeoFor(S, Lod);
			double Top = .1;
			for (const FVector& V : G.P) Top = FMath::Max(Top, V.Y);
			GeoH.Add(Top);
			ToMesh(G, M);
			const bool bCard = IsCard(S, Lod);
			UMaterialInterface* Mat = bCard ? (CardMID ? (UMaterialInterface*)CardMID : Solid) : Solid;
			UStaticMesh* SM = MakeMesh(M, Mat, *FString::Printf(TEXT("Reef_%d_%d"), S, Lod));
			UInstancedStaticMeshComponent* C = NewObject<UInstancedStaticMeshComponent>(this, *FString::Printf(TEXT("ReefPool_%d_%d"), S, Lod));
			C->SetStaticMesh(SM);
			C->SetupAttachment(Root);
			C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			C->NumCustomDataFloats = 5;
			C->SetCastShadow(!bCard && Lod < 2);
			C->SetMaterial(0, Mat);
			C->RegisterComponent();
			Pools.Add(C);
		}
}

const TArray<AKeysReef::FInst>& AKeysReef::ChunkInstances(int32 CX, int32 CZ)
{
	const int64 K = (int64(CX) << 32) ^ int64(uint32(CZ));
	if (const TArray<FInst>* Have = Cache.Find(K)) return *Have;
	TArray<FInst> Out;
	// solid species first; the cards (fans, plumes) are kept from sprouting through them
	TArray<FInst> Solids;
	const int32 SolidIds[] = {Elkhorn, Staghorn, Brain, Star, Encrusting, Barrel, Tube};
	for (const int32 S : SolidIds) PlaceSpecies(S, CX, CZ, nullptr, Solids);
	Out.Append(Solids);
	PlaceSpecies(SeaFan, CX, CZ, &Solids, Out);
	PlaceSpecies(SeaPlume, CX, CZ, &Solids, Out);
	PlaceSpecies(Seagrass, CX, CZ, nullptr, Out);
	CacheOrder.Add(K);
	if (CacheOrder.Num() > 900) { Cache.Remove(CacheOrder[0]); CacheOrder.RemoveAt(0); }
	return Cache.Add(K, MoveTemp(Out));
}

void AKeysReef::Rebuild(double CamX, double CamZ)
{
	const int32 CCX = FMath::FloorToInt32(CamX / ChunkSize), CCZ = FMath::FloorToInt32(CamZ / ChunkSize);
	TArray<TArray<FTransform>> Xf;
	TArray<TArray<float>> Data;
	Xf.SetNum(Pools.Num());
	Data.SetNum(Pools.Num());
	TArray<FLinearColor> Lo, Hi;
	for (int32 S = 0; S < NumSp; S++) { Lo.Add(RHex(Sp[S].Lo)); Hi.Add(RHex(Sp[S].Hi)); }
	for (int32 dz = -ResidentR; dz <= ResidentR; dz++)
		for (int32 dx = -ResidentR; dx <= ResidentR; dx++)
			for (const FInst& I : ChunkInstances(CCX + dx, CCZ + dz))
			{
				const double Dist = FVector2D(I.X - CamX, I.Z - CamZ).Size();
				const int32 Lod = Dist < LodNearMax ? 0 : Dist < LodMidMax ? 1 : 2;
				const int32 P = I.Species * 3 + Lod;
				// three.js Euler XYZ: R = Rx(tiltX) * Ry(rotY) * Rz(tiltZ), column vectors, then into Unreal's axes (x, z, y)
				const double cx = FMath::Cos(I.TiltX), sx = FMath::Sin(I.TiltX), cy = FMath::Cos(I.RotY), sy = FMath::Sin(I.RotY), cz = FMath::Cos(I.TiltZ), sz = FMath::Sin(I.TiltZ);
				const double RxM[3][3] = {{1, 0, 0}, {0, cx, -sx}, {0, sx, cx}};
				const double RyM[3][3] = {{cy, 0, sy}, {0, 1, 0}, {-sy, 0, cy}};
				const double RzM[3][3] = {{cz, -sz, 0}, {sz, cz, 0}, {0, 0, 1}};
				double Rxy[3][3], R3[3][3];
				for (int32 r = 0; r < 3; r++) for (int32 c = 0; c < 3; c++) { Rxy[r][c] = 0; for (int32 k = 0; k < 3; k++) Rxy[r][c] += RxM[r][k] * RyM[k][c]; }
				for (int32 r = 0; r < 3; r++) for (int32 c = 0; c < 3; c++) { R3[r][c] = 0; for (int32 k = 0; k < 3; k++) R3[r][c] += Rxy[r][k] * RzM[k][c]; }
				// swap y and z on both sides (a proper rotation again), and transpose into FMatrix's row-vector form
				const int32 Map[3] = {0, 2, 1};
				FMatrix FM = FMatrix::Identity;
				for (int32 r = 0; r < 3; r++) for (int32 c = 0; c < 3; c++) FM.M[c][r] = R3[Map[r]][Map[c]];
				const FQuat Q(FM);
				// nothing pokes out of the water: in skinny water a blade or a branch is cut down to stay 20 cm under
				double SY = I.SY;
				const double Room = -I.Y - .2;
				if (GeoH.IsValidIndex(P) && GeoH[P] * SY > Room) SY = FMath::Max(.05, Room / GeoH[P]);
				FTransform T(Q, ToUE(I.X, I.Y, I.Z), FVector(I.SX, I.SZ, SY));
				Xf[P].Add(T);
				const FLinearColor C = FMath::Lerp(Lo[I.Species], Hi[I.Species], float(I.ColorT));
				Data[P].Append({C.R, C.G, C.B, CardKind(I.Species), float(Lod == 2 && (I.Species == Elkhorn || I.Species == Staghorn) ? 0 : Sp[I.Species].Flex)});
			}
	Drawn = 0;
	for (int32 P = 0; P < Pools.Num(); P++)
	{
		UInstancedStaticMeshComponent* C = Pools[P];
		C->ClearInstances();
		if (!Xf[P].Num()) continue;
		C->AddInstances(Xf[P], false, true);
		for (int32 i = 0; i < Xf[P].Num(); i++) C->SetCustomData(i, TArrayView<const float>(Data[P].GetData() + i * 5, 5), false);
		C->MarkRenderStateDirty();
		Drawn += Xf[P].Num();
	}
}

void AKeysReef::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	APlayerCameraManager* Cam = UGameplayStatics::GetPlayerCameraManager(this, 0);
	if (!Cam) return;
	const FVector L = Cam->GetCameraLocation();
	const double CamX = L.X / 100, CamZ = L.Y / 100;
	const int32 CX = FMath::FloorToInt32(CamX / ChunkSize), CZ = FMath::FloorToInt32(CamZ / ChunkSize);
	if (CX != LastCX || CZ != LastCZ)
	{
		LastCX = CX; LastCZ = CZ;
		Rebuild(CamX, CamZ);
	}
	// sway: lean with the steady current, swing with the wave surge (world/reef/flow.ts)
	if (CardMID)
	{
		const double T = GetWorld()->GetTimeSeconds();
		const FVector2D Steady = KeysSea::SteadyCurrentAt(CamX, CamZ, T);
		const FVector2D N = KeysSea::ChainNormal(CamX);
		// the material works in Unreal's xy, which is the game's xz
		CardMID->SetVectorParameterValue(TEXT("Flow"), FLinearColor(Steady.X, Steady.Y, 0, 0));
		CardMID->SetVectorParameterValue(TEXT("SurgeAxis"), FLinearColor(N.X, N.Y, 0, 0));
		CardMID->SetScalarParameterValue(TEXT("SurgePhase"), float(FMath::Fmod(T / KeysSea::SurgePeriod, 1000.0) * PI * 2));
		CardMID->SetScalarParameterValue(TEXT("SurgeAmp"), float(1.7 * KeysSea::SurgeMaxSpeed * FMath::Exp(-FMath::Max(0.0, DepthFast(CamX, CamZ)) / KeysSea::SurgeDecayDepth)));
	}
}
