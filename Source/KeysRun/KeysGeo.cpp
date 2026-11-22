#include "KeysGeo.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/CommandLine.h"
#include "KeysOcean.h"
#include "Async/ParallelFor.h"
#include "KeysCoastData.h"
#include "KeysYards.h"
#include "KeysDocks.h"
#include "KeysGeoReal.h"
#include "KeysMangroveLife.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"

namespace KeysGeo
{
	static double Clamp(double V, double A, double B) { return V < A ? A : V > B ? B : V; }
	static double WSStep(double A, double B, double X) { const double T = Clamp((X - A) / (B - A), 0, 1); return T * T * (3 - 2 * T); }
	static double Lerp(double A, double B, double T) { return A + (B - A) * T; }
	static double SS(double E0, double E1, double X) { const double T = Clamp((X - E0) / (E1 - E0), 0, 1); return T * T * (3 - 2 * T); }

	struct FCreek { FString Name; TArray<FVector2D> Pts; double W, BX0, BX1, BZ0, BZ1; };
	struct FChannel { double X0, X1, Z0, Z1, D; };
	struct FGolf { const FIsland* I; double LX, LZ, RA, RB; };

	static bool GInit = false;
	static bool GReal = false;

	double WorldX0 = -4300, WorldZ0 = -3100, WorldSizeX = 8600, WorldSizeZ = 8600;
	double WBX0 = -4150, WBX1 = 4150, WBZ0 = -2950, WBZ1 = 5350;
	double BridgeArchX0 = -3178, BridgeArchX1 = -3106;
	double OldBridgeX0 = -3820, OldBridgeX1 = -2010, OldBridgeGap0 = -3200, OldBridgeGap1 = -3080;

	static void BuildNavSegs();
	static void BuildMangNear();
	static FLinearColor DepthRGBSrgb(double D);
	bool RealMap() { return GReal; }
	static double GTideForce = 9;
	static constexpr double TidePeriodS = 1440, TideRangeM = .32;
	static double TidePhase() { return FMath::Fmod(double(FDateTime::UtcNow().ToUnixTimestamp()) + FDateTime::UtcNow().GetMillisecond() * .001, TidePeriodS) / TidePeriodS; }
	void ForceTide(double Level) { GTideForce = Level; }
	double TideLevel() { return FMath::Abs(GTideForce) <= 1 ? GTideForce : FMath::Cos(2 * PI * TidePhase()); }
	double TideRise() { return TideLevel() * TideRangeM; }
	FString TideName()
	{
		const double L = TideLevel();
		if (L > .8) return TEXT("High tide");
		if (L < -.8) return TEXT("Low tide");
		const bool bFalling = FMath::Abs(GTideForce) <= 1 ? L < 0 : TidePhase() < .5;
		return bFalling ? TEXT("Tide falling") : TEXT("Tide rising");
	}
	double TideBite(const FString& Zone)
	{
		const double L = TideLevel();
		if (Zone == TEXT("Flats") || Zone == TEXT("Creek")) return 1 + .55 * L;                                         // up on the flats and in the roots with the water
		if (Zone == TEXT("Bridge") || Zone == TEXT("Hawk Channel") || Zone == TEXT("Backcountry")) return 1 - .3 * L;   // back in the channels at low water
		return 1;
	}
	bool RealMapWanted()
	{
		bool b = true;
		if (GConfig) GConfig->GetBool(TEXT("KeysRun"), TEXT("RealMap"), b, GGameUserSettingsIni);
		return b;
	}
	void SetRealMapWanted(bool b)
	{
		if (!GConfig) return;
		GConfig->SetBool(TEXT("KeysRun"), TEXT("RealMap"), b, GGameUserSettingsIni);
		GConfig->Flush(false, GGameUserSettingsIni);
	}
	double ChainZ(double X) { return GReal ? KeysGeoReal::ChainZ(X) : 0.000012 * X * X; }
	double ChainAngle(double X) { return FMath::Atan(GReal ? KeysGeoReal::ChainSlope(X) : 0.000024 * X); }

	// the real drop-off: z of the 45 m line at x (every 50 m, smoothed), found in Init
	static TArray<double> GDropZ;
	static double DropZ(double X)
	{
		const double F = Clamp((X - WorldX0) / 50, 0, GDropZ.Num() - 1.001);
		const int32 I = int32(F);
		return GDropZ[I] + (GDropZ[I + 1] - GDropZ[I]) * (F - I);
	}
	// original cross-shore distance <-> the fraction of the way from US-1 out to the drop-off: Hawk Channel's deep
	// middle, the back reef, the crest, the wall. Past the wall and in the bay it is a plain stretch.
	static const double LegOld[] = {0, 520, 1300, 1460, 1650}, LegFrac[] = {0, .27, .71, .86, 1};
	static constexpr double LegSeaK = 1.15, LegBayK = 1.33;

	double LegacyZ(double X, double DZ)
	{
		const double C = ChainZ(X);
		if (!GReal) return C + DZ;
		const double Span = DropZ(X) - C;
		if (DZ <= 0) return C + DZ * LegBayK;
		if (DZ >= LegOld[4]) return C + Span + (DZ - LegOld[4]) * LegSeaK;
		for (int32 i = 0; i < 4; i++)
			if (DZ <= LegOld[i + 1]) return C + Span * (LegFrac[i] + (LegFrac[i + 1] - LegFrac[i]) * (DZ - LegOld[i]) / (LegOld[i + 1] - LegOld[i]));
		return C + Span;
	}

	double DZAt(double X, double Z)
	{
		const double C = ChainZ(X), D = Z - C;
		if (!GReal) return D;
		if (D <= 0) return D / LegBayK;
		const double Span = DropZ(X) - C, F = D / Span;
		if (F >= 1) return LegOld[4] + (D - Span) / LegSeaK;
		for (int32 i = 0; i < 4; i++)
			if (F <= LegFrac[i + 1]) return LegOld[i] + (LegOld[i + 1] - LegOld[i]) * (F - LegFrac[i]) / (LegFrac[i + 1] - LegFrac[i]);
		return LegOld[4];
	}
	static TArray<FIsland> GIslands;
	static TArray<FMarina> GMarinas;
	static TArray<FHump> GHumps;
	static TArray<FCreek> GCreeks;
	static TArray<FChannel> GChannels;     // dz relative to the chain
	static TArray<FChannel> GMiamiCh;      // absolute z
	static TArray<FGolf> GGolf;
	static const FIsland* GVaca = nullptr;
	static TArray<FPillar> GPillars;
	static TMap<int64, TArray<int32>> GPGrid;
	static constexpr double PCell = 32;

	// old Seven Mile Bridge, with the swing-span gap
	static constexpr double OldBrDZ = OldBridgeDZ;
	static double &OldBrX0 = OldBridgeX0, &OldBrX1 = OldBridgeX1, &OldGap0 = OldBridgeGap0, &OldGap1 = OldBridgeGap1;
	// runway on Vaca Key, in island-local coordinates
	static constexpr double RunLX0 = 700, RunLX1 = 1420, RunLZ = 80, RunW = 34;
	// on the real map: Marathon's airport, where it really is (OSM aeroway=runway through the bake), as a strip from A to B
	static constexpr double RealRunAX = 1016.7, RealRunAZ = -21.4, RealRunBX = 1524.4, RealRunBZ = -55.1, RealRunHalfW = 15;
	static bool OnRealRunway(double X, double Z, double Margin)
	{
		const double VX = RealRunBX - RealRunAX, VZ = RealRunBZ - RealRunAZ, L = FMath::Sqrt(VX * VX + VZ * VZ);
		const double Al = ((X - RealRunAX) * VX + (Z - RealRunAZ) * VZ) / L, Pp = ((X - RealRunAX) * -VZ + (Z - RealRunAZ) * VX) / L;
		return Al > -20 && Al < L + 20 && FMath::Abs(Pp) < RealRunHalfW + Margin;
	}

	// fast depth lookup grid (10 m) so the water surface can follow the boat
	static constexpr double DG = 10;
	static int32 DGNX = 0, DGNZ = 0;
	static TArray<float> DepthG, InletG;
	static TArray<FLinearColor> ColorG;

	static TArray<FSeaState> GSea;
	static int32 GSeaIdx = 1;
	static double SW = 1.3, CH = 1.2;   // eased swell and wind-sea height factors

	// the swell: four long trains out of the southeast [kx, kz, angular speed, amplitude, crest sharpness, phase]
	static const double SWELL[4][6] = {
		{.0114, .0262, .53, .62, .12, 0}, {.0302, .0322, .66, .32, .16, 1.7}, {.0205, .0178, .516, .45, .1, 3.1}, {-.0128, .0251, .526, .35, .1, 4.4}};

	// the wind sea: a fixed spectrum, ChopL lengths x ChopD directions. Lengths and directions never change (so the phases
	// never jump); the driver only reweights their heights every frame.
	static constexpr int32 ChopL = 6, ChopD = 12;
	static const double ChopLen[ChopL] = {5.5, 9, 14, 22, 34, 52};
	struct FWaveTrain { double KX, KZ, W, A, S, Ph, Len; };
	static FWaveTrain GChop[ChopL * ChopD];
	static TArray<FWaveTrain> GLive;           // the trains with any height right now (swell first)
	static bool GChopInit = false;

	// the driver: base wind from the sea state, storm wind from the weather; eased values below
	static double BaseWind = 7, BaseSwell = 1.3, StormSpd = 0, StormDir = 0, StormConf = 0, StormSwell = 0;
	static double WindNow = 7, WindDirNow = -2.75, SeaDirNow = -2.75, ChopNow = 1.2, ConfNow = 0, CapsNow = .2, RmsNow = .3;
	static constexpr double BaseDir = -2.75;   // the trade wind out of the east-southeast, blowing toward the west-northwest

	// wake rings: where and when each was dropped, how fast it spreads (C, m/s) and how tall it starts (A, m)
	struct FWakeRing { double X = 0, Z = 0, T = -999, C = 0, A = 0; };
	static constexpr int32 WakeN = 1024;
	static FWakeRing WakeP[WakeN];
	static int32 WakeI = 0;
	static constexpr double WakeLife = 22;

	// prop-wash trail points; Prev links each point to the same boat's previous one (by serial number)
	struct FTrailPt { double X = 0, Z = 0, T = -999, W = 0, S = 0; int32 Source = -1; int64 Serial = -1, PrevSerial = -1; };
	static constexpr int32 TrailN = 1024;
	static FTrailPt TrailP[TrailN];
	static int64 TrailSerial = 0;
	static TMap<int32, int64> TrailLast;
	static constexpr double TrailLife = 26;

	static void IslandLocal(const FIsland& I, double X, double Z, double& LX, double& LZ)
	{
		const double DX = X - I.X, DZ = Z - I.Z;
		LX = DX * I.C + DZ * I.S;
		LZ = -DX * I.S + DZ * I.C;
	}

	// ---------------------------------------------------------------- real coastlines (web shared/world/coast.ts)
	// Each island's edges go in two small indexes: a CELL x CELL grid over its box grown by CAP, each cell listing the
	// edges that can be within CAP of any point in it, and CELL-high rows listing the edges that span them for the
	// inside / outside ray test. Only the first CAP meters are exact; beyond, it is CAP plus the distance to the box.
	static constexpr double CoastCap = 120, CoastCell = 24, ShoreScale = 120;
	struct FCoast
	{
		double X0 = 0, Z0 = 0, X1 = 0, Z1 = 0, GX0 = 0, GZ0 = 0;
		int32 NX = 0, NZ = 0;
		TArray<double> Seg;
		TArray<int32> CellStart, CellSeg, RowStart, RowSeg;
		TArray<TArray<FVector2D>> Rings;
	};
	static TArray<FCoast> GCoasts;
	static TArray<FVector2D> GUS1Runs;

	static double SegDist2(const double* S, double X, double Z)
	{
		const double AX = S[0], AZ = S[1], DX = S[2] - AX, DZ = S[3] - AZ, L = DX * DX + DZ * DZ;
		double T = L > 0 ? ((X - AX) * DX + (Z - AZ) * DZ) / L : 0;
		T = T < 0 ? 0 : T > 1 ? 1 : T;
		const double PX = AX + DX * T - X, PZ = AZ + DZ * T - Z;
		return PX * PX + PZ * PZ;
	}

	template <typename FRange>
	static void Csr(int32 N, int32 Buckets, FRange Range, TArray<int32>& Start, TArray<int32>& Out)
	{
		Start.Init(0, Buckets + 1);
		for (int32 i = 0; i < N; i++) Range(i, [&](int32 B) { Start[B + 1]++; });
		for (int32 B = 0; B < Buckets; B++) Start[B + 1] += Start[B];
		TArray<int32> Fill;
		Fill.Append(Start.GetData(), Buckets);
		Out.SetNumZeroed(Start[Buckets]);
		for (int32 i = 0; i < N; i++) Range(i, [&](int32 B) { Out[Fill[B]++] = i; });
	}

	static FCoast BuildCoast(const KeysCoastData::FIsl& Src)
	{
		FCoast C;
		int32 N = 0;
		for (int32 r = 0; r < Src.NumRings; r++) N += Src.Rings[r].N / 2;
		C.Seg.SetNumUninitialized(N * 4);
		double X0 = 1e18, Z0 = 1e18, X1 = -1e18, Z1 = -1e18;
		int32 K = 0;
		for (int32 r = 0; r < Src.NumRings; r++)
		{
			const double* P = Src.Rings[r].P;
			const int32 M = Src.Rings[r].N / 2;
			TArray<FVector2D>& Ring = C.Rings.AddDefaulted_GetRef();
			for (int32 i = 0; i < M; i++)
			{
				const int32 j = (i + 1) % M;
				C.Seg[K++] = P[i * 2]; C.Seg[K++] = P[i * 2 + 1]; C.Seg[K++] = P[j * 2]; C.Seg[K++] = P[j * 2 + 1];
				X0 = FMath::Min(X0, P[i * 2]); X1 = FMath::Max(X1, P[i * 2]); Z0 = FMath::Min(Z0, P[i * 2 + 1]); Z1 = FMath::Max(Z1, P[i * 2 + 1]);
				Ring.Add(FVector2D(P[i * 2], P[i * 2 + 1]));
			}
		}
		C.X0 = X0; C.Z0 = Z0; C.X1 = X1; C.Z1 = Z1;
		C.GX0 = X0 - CoastCap; C.GZ0 = Z0 - CoastCap;
		C.NX = FMath::CeilToInt((X1 - X0 + 2 * CoastCap) / CoastCell);
		C.NZ = FMath::CeilToInt((Z1 - Z0 + 2 * CoastCap) / CoastCell);
		const double Reach = CoastCap + CoastCell * UE_INV_SQRT_2, Reach2 = Reach * Reach;
		const double* Sg = C.Seg.GetData();
		Csr(N, C.NX * C.NZ, [&](int32 i, auto Add)
		{
			const double* O = Sg + i * 4;
			const int32 CX0 = FMath::Max(0, FMath::FloorToInt((FMath::Min(O[0], O[2]) - Reach - C.GX0) / CoastCell));
			const int32 CX1 = FMath::Min(C.NX - 1, FMath::FloorToInt((FMath::Max(O[0], O[2]) + Reach - C.GX0) / CoastCell));
			const int32 CZ0 = FMath::Max(0, FMath::FloorToInt((FMath::Min(O[1], O[3]) - Reach - C.GZ0) / CoastCell));
			const int32 CZ1 = FMath::Min(C.NZ - 1, FMath::FloorToInt((FMath::Max(O[1], O[3]) + Reach - C.GZ0) / CoastCell));
			for (int32 CZ = CZ0; CZ <= CZ1; CZ++)
				for (int32 CX = CX0; CX <= CX1; CX++)
					if (SegDist2(O, C.GX0 + (CX + .5) * CoastCell, C.GZ0 + (CZ + .5) * CoastCell) <= Reach2) Add(CZ * C.NX + CX);
		}, C.CellStart, C.CellSeg);
		Csr(N, C.NZ, [&](int32 i, auto Add)
		{
			const double* O = Sg + i * 4;
			const int32 R0 = FMath::Max(0, FMath::FloorToInt((FMath::Min(O[1], O[3]) - C.GZ0) / CoastCell));
			const int32 R1 = FMath::Min(C.NZ - 1, FMath::FloorToInt((FMath::Max(O[1], O[3]) - C.GZ0) / CoastCell));
			for (int32 R = R0; R <= R1; R++) Add(R);
		}, C.RowStart, C.RowSeg);
		return C;
	}

	static bool CoastInside(const FCoast& C, double X, double Z)
	{
		if (X < C.X0 || X > C.X1 || Z < C.Z0 || Z > C.Z1) return false;
		const int32 R = FMath::FloorToInt((Z - C.GZ0) / CoastCell);
		const double* S = C.Seg.GetData();
		bool bIn = false;
		for (int32 k = C.RowStart[R], E = C.RowStart[R + 1]; k < E; k++)
		{
			const double* O = S + C.RowSeg[k] * 4;
			const double AZ = O[1], BZ = O[3];
			if ((AZ > Z) != (BZ > Z))
			{
				const double IX = O[0] + ((Z - AZ) / (BZ - AZ)) * (O[2] - O[0]);
				if (IX > X) bIn = !bIn;
			}
		}
		return bIn;
	}

	static double CoastDist(const FCoast& C, double X, double Z)
	{
		const double GX = X - C.GX0, GZ = Z - C.GZ0, W = C.NX * CoastCell, H = C.NZ * CoastCell;
		if (GX < 0 || GZ < 0 || GX >= W || GZ >= H)
		{
			const double OX = GX < 0 ? -GX : GX > W ? GX - W : 0, OZ = GZ < 0 ? -GZ : GZ > H ? GZ - H : 0;
			return CoastCap + FMath::Sqrt(OX * OX + OZ * OZ);
		}
		const int32 Cell = FMath::FloorToInt(GZ / CoastCell) * C.NX + FMath::FloorToInt(GX / CoastCell);
		const double* S = C.Seg.GetData();
		double Best = CoastCap * CoastCap;
		for (int32 k = C.CellStart[Cell], E = C.CellStart[Cell + 1]; k < E; k++)
		{
			const double D2 = SegDist2(S + C.CellSeg[k] * 4, X, Z);
			if (D2 < Best) Best = D2;
		}
		const double D = FMath::Sqrt(Best);
		return CoastInside(C, X, Z) ? -D : D;
	}

	double IslandDist(const FIsland& I, double X, double Z)
	{
		if (I.Coast >= 0) return CoastDist(GCoasts[I.Coast], X, Z);
		double LX, LZ;
		IslandLocal(I, X, Z, LX, LZ);
		return (FMath::Sqrt(FMath::Square(LX / I.A) + FMath::Square(LZ / I.B)) - 1) * (I.A + I.B) * .5;
	}

	double IslandE(const FIsland& I, double X, double Z)
	{
		if (I.Coast < 0)
		{
			double LX, LZ;
			IslandLocal(I, X, Z, LX, LZ);
			return FMath::Sqrt(FMath::Square(LX / I.A) + FMath::Square(LZ / I.B));
		}
		// the beach is measured on the island's own scale, capped so a big key's beach is ~20 m, not a tenth of the key
		return 1 + CoastDist(GCoasts[I.Coast], X, Z) / FMath::Min((I.A + I.B) * .5, ShoreScale);
	}

	FVector2D IslandOutward(const FIsland& I, double X, double Z)
	{
		const double D = 7;
		const double GX = IslandDist(I, X + D, Z) - IslandDist(I, X - D, Z), GZ = IslandDist(I, X, Z + D) - IslandDist(I, X, Z - D);
		const double L = FMath::Sqrt(GX * GX + GZ * GZ);
		return L < 1e-6 ? FVector2D(0, 1) : FVector2D(GX / L, GZ / L);
	}

	const TArray<TArray<FVector2D>>* CoastRings(const FIsland& I) { return I.Coast >= 0 ? &GCoasts[I.Coast].Rings : nullptr; }
	void IslandLocalXZ(const FIsland& I, double X, double Z, double& LX, double& LZ) { IslandLocal(I, X, Z, LX, LZ); }
	void IslandWorldXZ(const FIsland& I, double LX, double LZ, double& X, double& Z) { X = I.X + LX * I.C - LZ * I.S; Z = I.Z + LX * I.S + LZ * I.C; }
	const FIsland* FindIsland(const TCHAR* Name) { for (const FIsland& I : GIslands) if (I.Name == Name) return &I; return nullptr; }

	static FCreek Snake(const TCHAR* Name, double X0, double DZ0, double X1, double DZ1, double Amp, double Waves, double W)
	{
		const double Z0 = ChainZ(X0) + DZ0, Z1 = ChainZ(X1) + DZ1, DX = X1 - X0, DZ = Z1 - Z0, L = FMath::Sqrt(DX * DX + DZ * DZ);
		const double PX = -DZ / L, PZ = DX / L;
		const int32 N = FMath::CeilToInt(L / 8);
		FCreek C;
		C.Name = Name;
		C.W = W;
		double BX0 = 1e9, BX1 = -1e9, BZ0 = 1e9, BZ1 = -1e9;
		for (int32 i = 0; i <= N; i++)
		{
			const double U = double(i) / N;
			const double Off = Amp * FMath::Sin(U * PI * 2 * Waves) * FMath::Pow(FMath::Sin(U * PI), .35);
			const FVector2D P(X0 + DX * U + PX * Off, Z0 + DZ * U + PZ * Off);
			C.Pts.Add(P);
			BX0 = FMath::Min(BX0, P.X); BX1 = FMath::Max(BX1, P.X); BZ0 = FMath::Min(BZ0, P.Y); BZ1 = FMath::Max(BZ1, P.Y);
		}
		C.BX0 = BX0 - W - 30; C.BX1 = BX1 + W + 30; C.BZ0 = BZ0 - W - 30; C.BZ1 = BZ1 + W + 30;
		return C;
	}

	static FTruss GTruss{-6925, -58, -7520, -250, 16, 2, 4};
	const FTruss* BahiaHonda() { return GReal ? &GTruss : nullptr; }

	static void AddPillar(double X, double Z, double R, bool bOld)
	{
		const int32 Idx = GPillars.Add({X, Z, R, bOld});
		const int64 Key = (int64(FMath::FloorToInt(X / PCell)) << 32) ^ int64(uint32(FMath::FloorToInt(Z / PCell)));
		GPGrid.FindOrAdd(Key).Add(Idx);
	}

	void Init()
	{
		if (GInit) return;
		GInit = true;

		int64 Seed = 11;
		auto SRand = [&Seed]() { Seed = (Seed * 16807) % 2147483647; return double(Seed) / 2147483647.0; };

		// the real Keys (NOAA / USGS / OpenStreetMap, baked by Tools/geo/bake.py) when asked for; else the original map
		// which map: the Settings menu's choice (kept in the user's settings file; the real Keys unless they pick the classic
		// map), or forced from the command line with -KRRealMap / -KRClassicMap
		const bool bWantReal = FParse::Param(FCommandLine::Get(), TEXT("KRRealMap")) || (!FParse::Param(FCommandLine::Get(), TEXT("KRClassicMap")) && RealMapWanted());
		GReal = bWantReal && KeysGeoReal::LoadAll();
		if (GReal)
		{
			WorldX0 = KeysGeoReal::X0(); WorldZ0 = KeysGeoReal::Z0(); WorldSizeX = KeysGeoReal::SizeX(); WorldSizeZ = KeysGeoReal::SizeZ();
			WBX0 = WorldX0 + 150; WBX1 = WorldX0 + WorldSizeX - 150; WBZ0 = WorldZ0 + 150; WBZ1 = WorldZ0 + WorldSizeZ - 150;
			// the Seven Mile Bridge's high span stands over Moser Channel (x about -2820), on the bent grid
			BridgeArchX0 = WBX0 + FMath::RoundToDouble((-2856 - WBX0) / BridgeBentStep) * BridgeBentStep;
			BridgeArchX1 = BridgeArchX0 + 2 * BridgeBentStep;
			// the old Seven Mile Bridge: from Knight's Key across Pigeon Key to Little Duck Key, 58 m on the bay side of the
			// new one (as it really is at this scale), with its span over Moser Channel gone
			OldBridgeX1 = -1300; OldBridgeX0 = OldBridgeX1 - 30 * 123;
			OldBridgeGap0 = BridgeArchX0 + BridgeBentStep - 60; OldBridgeGap1 = OldBridgeGap0 + 120;
			// the drop-off: going south from US-1, the first water 45 m deep
			const int32 ND = FMath::CeilToInt(WorldSizeX / 50) + 1;
			TArray<double> Raw;
			Raw.SetNumUninitialized(ND);
			for (int32 i = 0; i < ND; i++)
			{
				const double X = WorldX0 + i * 50;
				double Z = KeysGeoReal::ChainZ(X) + 1200;
				while (Z < WorldZ0 + WorldSizeZ - 600 && KeysGeoReal::Height(X, Z) > -45) Z += 10;
				Raw[i] = Z;
			}
			GDropZ.SetNumUninitialized(ND);
			for (int32 i = 0; i < ND; i++)
			{
				double Sum = 0; int32 Cnt = 0;
				for (int32 k = FMath::Max(0, i - 12); k <= FMath::Min(ND - 1, i + 12); k++) { Sum += Raw[k]; Cnt++; }
				GDropZ[i] = Sum / Cnt;
			}
		}
		struct FDef { const TCHAR* Name; double X, DZ, A, B; };
		const FDef Defs[] = {
			{TEXT("Little Duck Key"), -3860, 10, 230, 120}, {TEXT("Pigeon Key"), -2650, -62, 70, 42}, {TEXT("Knight's Key"), -2080, 0, 170, 110},
			{TEXT("Marathon (Vaca Key)"), -120, 0, 1680, 270}, {TEXT("Boot Key"), -1050, 560, 620, 170}, {TEXT("Fat Deer Key"), 1990, -30, 380, 230},
			{TEXT("Key Colony Beach"), 2050, 330, 260, 110}, {TEXT("Crawl Key"), 2750, -20, 300, 170}, {TEXT("Grassy Key"), 3640, -10, 520, 190},
			{TEXT("East Sister Rock"), 900, 560, 42, 30}, {TEXT("Molasses Keys"), -3150, 520, 70, 36},
			{TEXT("Rachel Key"), -1250, -950, 95, 55}, {TEXT("Bamboo Key"), 350, -1350, 140, 70}, {TEXT("Money Key"), 1450, -850, 80, 50},
			{TEXT("Bay mangroves"), -2600, -1500, 120, 70}, {TEXT("Florida mainland (Miami)"), 0, -3500, 6600, 760}};
		GIslands.Reset();
		if (!GReal) for (const FDef& D : Defs)
		{
			FIsland I;
			I.Name = D.Name;
			I.X = D.X;
			I.Z = ChainZ(D.X) + D.DZ;
			I.A = D.A;
			I.B = D.B;
			I.Th = ChainAngle(D.X) + (D.A < 150 ? (SRand() - .5) * 1.2 : 0);
			I.C = FMath::Cos(I.Th);
			I.S = FMath::Sin(I.Th);
			I.bSmall = D.A < 150;
			I.bMainland = I.Name.StartsWith(TEXT("Florida mainland"));
			I.bMiami = false;
			I.bNoHouses = false;
			GIslands.Add(I);
		}
		// Miami, laid out from the real geography of Biscayne Bay (scaled about 1:4, turned so the city faces south onto the bay)
		struct FMDef { const TCHAR* Name; double X, Z, A, B; };
		const FMDef MDefs[] = {
			{TEXT("Brickell Key"), 2088, -2652, 62, 50}, {TEXT("Dodge Island (PortMiami)"), 2300, -2302, 75, 338}, {TEXT("Watson Island"), 2525, -2565, 75, 62},
			{TEXT("Star Island"), 2575, -2165, 31, 100}, {TEXT("Hibiscus Island"), 2625, -2290, 22, 90}, {TEXT("Palm Island"), 2700, -2290, 25, 125},
			{TEXT("Fisher Island"), 2200, -1865, 112, 87}, {TEXT("Miami Beach"), 3280, -1690, 900, 150},
			{TEXT("Venetian Islands"), 2950, -2540, 30, 55}, {TEXT("Venetian Islands"), 2950, -2390, 30, 55}, {TEXT("Venetian Islands"), 2950, -2240, 30, 55}, {TEXT("Venetian Islands"), 2950, -2090, 30, 55},
			{TEXT("Virginia Key"), 1425, -2040, 275, 225}, {TEXT("Key Biscayne"), 380, -1790, 740, 250}};
		if (!GReal) for (const FMDef& D : MDefs)
		{
			FIsland I;
			I.Name = D.Name;
			I.X = D.X; I.Z = D.Z; I.A = D.A; I.B = D.B;
			I.Th = 0; I.C = 1; I.S = 0;
			I.bSmall = D.A < 150 && D.B < 150;
			I.bMainland = false;
			I.bMiami = true;
			I.bNoHouses = I.Name == TEXT("Miami Beach") || I.Name == TEXT("Dodge Island (PortMiami)") || I.Name == TEXT("Brickell Key") || I.Name == TEXT("Virginia Key");
			GIslands.Add(I);
		}
		// the real outlines (web chain.ts: every island but the mainland)
		GCoasts.Reset();
		if (GReal)
		{
			// every island in the baked data, biggest first, with its real outline
			for (const KeysGeoReal::FIsl& R : KeysGeoReal::Islands())
			{
				FIsland I;
				I.Name = R.Name.IsEmpty() ? FString(TEXT("a mangrove key")) : R.Name;
				I.X = R.X; I.Z = R.Z;
				I.A = FMath::Max(8.0, (R.MaxX - R.MinX) * .5); I.B = FMath::Max(8.0, (R.MaxZ - R.MinZ) * .5);
				I.Th = 0; I.C = 1; I.S = 0;
				I.bSmall = I.A < 150 && I.B < 150;
				I.bMainland = false; I.bMiami = false;
				I.bNoHouses = R.Name.IsEmpty() || R.Area < 9000;   // mangrove keys and rocks
				const KeysCoastData::FRing Ring{R.Ring.GetData(), R.Ring.Num()};
				const KeysCoastData::FIsl Src{&Ring, 1};
				I.Coast = GCoasts.Num();
				GCoasts.Add(BuildCoast(Src));
				GIslands.Add(I);
			}
		}
		else for (int32 i = 0; i < GIslands.Num() && i < KeysCoastData::NumIslands; i++)
			if (KeysCoastData::Islands[i].Rings)
			{
				GIslands[i].Coast = GCoasts.Num();
				GCoasts.Add(BuildCoast(KeysCoastData::Islands[i]));
			}
		auto FindIsl = [](const TCHAR* Name) { return FindIsland(Name); };
		GVaca = FindIsl(TEXT("Marathon (Vaca Key)"));
		GGolf = {{GVaca, 330, 100, 290, 150}, {FindIsl(TEXT("Key Colony Beach")), -30, 30, 175, 72}};

		GMiamiCh = {{2312, 2380, -2010, -1420, 13}, {2376, 2480, -2640, -1950, 11}, {2140, 2224, -2640, -1960, 9}, {2224, 2380, -2740, -2640, 8}};
		GChannels = {{-1700, -480, 200, 430, 3.8}, {-2000, -1560, 150, 800, 4.2}, {-1750, -1350, -420, -180, 3.4}, {-800, -420, -420, -180, 3.4}};
		GHumps = {{TEXT("Marathon Hump"), -500, 3350, false}, {TEXT("West Hump"), -2700, 3650, false}, {TEXT("Coffins Patch"), 2700, 1050, true}, {TEXT("Delta Shoal"), 1250, 1300, true}};
		if (GReal)
		{
			// the real bottom has its own channels and patch reefs; the golf courses and the runway wait for their real sites
			GGolf.Reset();
			GMiamiCh.Reset();
			GChannels.Reset();
			for (FHump& H : GHumps) H.DZ = LegacyZ(H.X, H.DZ) - ChainZ(H.X);   // kept as real metres south of the chain
		}

		// marinas: two in Boot Key Harbor, two on the Gulf side
		struct FMar { const TCHAR* Name; double SX; int32 Dir; };
		const FMar MarsOld[] = {{TEXT("Boot Key Harbor City Marina"), -900, 1}, {TEXT("Burdines Waterfront"), -1400, 1}, {TEXT("Faro Blanco Marina"), -1560, -1}, {TEXT("Keys Fisheries"), -600, -1}};
		// on the real map: two on the north shore of the real Boot Key Harbor, two on the Gulf side of Vaca Key
		const FMar MarsReal[] = {{TEXT("Boot Key Harbor City Marina"), -500, 1}, {TEXT("Burdines Waterfront"), -330, 1}, {TEXT("Faro Blanco Marina"), -625, -1}, {TEXT("Keys Fisheries"), -300, -1}};
		GMarinas.Reset();
		if (GReal) for (const FMar& M : MarsReal)
		{
			// walk out from US-1 until past the shore with open water ahead (so a canal doesn't count)
			double SZ = ChainZ(M.SX);
			bool bLand = false;
			for (int32 k = 0; k < 1200; k++)
			{
				const double Z = ChainZ(M.SX) + M.Dir * k * .5, D = KeysGeoReal::Shore(M.SX, Z);
				if (D < 0) bLand = true;
				else if (bLand && D >= 3.6 && KeysGeoReal::Shore(M.SX, Z + M.Dir * 30) >= 18) { SZ = Z; break; }
			}
			GMarinas.Add({M.Name, M.SX, SZ, SZ + M.Dir * 48, M.Dir});
			// the real key is only some 200 m across here at this scale, so the town straddles the highway: US-1 runs
			// dead straight through it as the street between Sombrero Boulevard's row and the casino lot
			if (GMarinas.Num() == 1) KeysGeoReal::FlattenChain(M.SX - 100, M.SX + 100, SZ - M.Dir * TownUS1V);
		}
		else for (const FMar& M : MarsOld)
		{
			// walk out from the island's middle until past the real shore (land first, so a canal doesn't count)
			const FIsland& I = *GVaca;
			double SZ = I.Z;
			bool bLand = false;
			for (int32 k = 0; k < 1200; k++)
			{
				const double Z = I.Z + M.Dir * k * .5, E = IslandE(I, M.SX, Z);
				if (E < 1) bLand = true;
				else if (bLand && E >= 1.03) { SZ = Z; break; }
			}
			GMarinas.Add({M.Name, M.SX, SZ, SZ + M.Dir * 48, M.Dir});
		}

		// US-1's causeway fill across the keys it runs over (web depth.ts US1_RUNS)
		GUS1Runs.Reset();
		if (!GReal) for (const TCHAR* Name : {TEXT("Little Duck Key"), TEXT("Knight's Key"), TEXT("Marathon (Vaca Key)"), TEXT("Fat Deer Key"), TEXT("Crawl Key"), TEXT("Grassy Key")})
		{
			const FIsland* I = FindIsl(Name);
			if (!I || I->Coast < 0) continue;
			const FCoast& C = GCoasts[I->Coast];
			double A = 1e18, B = -1e18;
			for (double X = C.X0; X <= C.X1; X += 2) if (IslandDist(*I, X, ChainZ(X)) < -4) { A = FMath::Min(A, X); B = FMath::Max(B, X); }
			if (B > A) GUS1Runs.Add(FVector2D(A, B));
		}

		// winding creeks cut through the keys and the mainland
		GCreeks = {
			Snake(TEXT("Snake Creek"), 620, -400, 700, 400, 70, 1.5, 34),
			Snake(TEXT("Sister Creek"), -700, 360, -640, 790, 38, 1.5, 28),
			Snake(TEXT("Grassy Key creek"), 3450, -320, 3560, 320, 42, 1.2, 24),
			Snake(TEXT("Miami River"), 2150, -2620, 2650, -4300, 170, 2.5, 48),
			Snake(TEXT("Mangrove river"), -2400, -2620, -2950, -4100, 150, 3, 32),
			Snake(TEXT("Little River"), 3650, -2640, 3950, -3700, 70, 2, 26)};

		if (GReal)
		{
			// the real creeks are in the coastline itself - too fine for the 10 m grid to keep open, so the mangrove
			// creeks a skiff can idle through are cut along their own lines (KeysMangroveLife.cpp)
			GCreeks.Reset();
			for (const KeysMangroveLife::FCreek& M : KeysMangroveLife::Creeks())
			{
				FCreek C;
				C.Name = M.Name; C.Pts = M.Pts; C.W = M.W;
				C.BX0 = M.X0; C.BX1 = M.X1; C.BZ0 = M.Z0; C.BZ1 = M.Z1;
				GCreeks.Add(C);
			}
		}
		if (GReal) BuildNavSegs();
		if (GReal) BuildMangNear();

		GSea = {{TEXT("Calm"), .8, .5, TEXT("glassy, a long, lazy ground swell."), 1.5},
				{TEXT("Choppy"), 1.3, 1.2, TEXT("a steady trade wind, swells with afternoon chop on top."), 7},
				{TEXT("Haulover"), 1.6, 2.2, TEXT("blowing 20 - steep, stacked chop and whitecaps. Trim up and hang on."), 11},
				{TEXT("Big swell"), 2.6, 1.1, TEXT("big Atlantic rollers. Ride the backs and watch the troughs."), 6.5}};
		SetSeaState(1);
		SnapSea();
		for (FWakeRing& W : WakeP) W = FWakeRing();
		for (FTrailPt& P : TrailP) P = FTrailPt();

		// depth, inlet and color grids
		DGNX = FMath::CeilToInt(WorldSizeX / DG) + 1;
		DGNZ = FMath::CeilToInt(WorldSizeZ / DG) + 1;
		DepthG.SetNumUninitialized(DGNX * DGNZ);
		InletG.SetNumUninitialized(DGNX * DGNZ);
		ColorG.SetNumUninitialized(DGNX * DGNZ);
		ParallelFor(DGNZ, [](int32 j)
		{
			for (int32 i = 0; i < DGNX; i++)
			{
				const double X = WorldX0 + i * DG, Z = WorldZ0 + j * DG, D = DepthAt(X, Z);
				const int32 K = j * DGNX + i;
				DepthG[K] = float(D);
				const double DZ = Z - ChainZ(X);
				InletG[K] = float((D > 3 && FMath::Abs(DZ) < 85) ? (1 - FMath::Abs(DZ) / 85)
					: (D > 3 && X > OldBrX0 && X < OldBrX1 && FMath::Abs(DZ - OldBrDZ) < 50) ? .5 * (1 - FMath::Abs(DZ - OldBrDZ) / 50) : 0);
				ColorG[K] = DepthRGB(D);
				if (GReal)
				{
					// the two sides of the keys: Florida Bay is green and a little milky over its grass and marl; the Atlantic
					// side is clear aqua over sand. The tint fades out with depth and across the channels between the keys.
					const double Bay = SS(120, -260, DZ) * SS(9, 3, D);
					if (Bay > 0)
					{
						FLinearColor S = DepthRGBSrgb(D);
						S = FMath::Lerp(S, FLinearColor(S.R * 1.18f + .06f, S.G * .96f + .01f, S.B * .74f), float(Bay));
						ColorG[K] = FLinearColor(FMath::Pow(S.R, 2.2f), FMath::Pow(S.G, 2.2f), FMath::Pow(S.B, 2.2f));
					}
					// tannin: the water round the mangroves is stained the colour of tea
					const double Tan = MangroveNear(X, Z) * SS(5, 1.5, D);
					if (Tan > 0) ColorG[K] = FMath::Lerp(ColorG[K], FLinearColor(.105f, .075f, .022f), float(Tan * .85));
				}
			}
		});

		// bridge pilings: US-1 over open water, and the old Seven Mile Bridge with its swing-span gap
		GPillars.Reset();
		GPGrid.Reset();
		for (double X = WBX0; X <= WBX1; X += BridgeBentStep)
		{
			const double Z = ChainZ(X);
			if (LandH(X, Z) > -.5) continue;
			if (X > BridgeArchX0 + 1 && X < BridgeArchX1 - 1) continue;   // the arch's clear span over the channel
			const double Th = ChainAngle(X), NX = -FMath::Sin(Th), NZ = FMath::Cos(Th);
			if (FMath::Abs(X - BridgeArchX0) < 1 || FMath::Abs(X - BridgeArchX1) < 1)
			{
				// the arch piers: one massive block across the whole deck
				for (const double Off : {-4.4, 0.0, 4.4}) AddPillar(X + NX * Off, Z + NZ * Off, 2.6, false);
				continue;
			}
			AddPillar(X + NX * 4.5, Z + NZ * 4.5, .8, false);
			AddPillar(X - NX * 4.5, Z - NZ * 4.5, .8, false);
		}
		if (GReal) for (const KeysGeoReal::FNavMark& M : KeysGeoReal::Marks()) AddPillar(M.X, M.Z, .35, false);   // a daybeacon's pile
		if (const FTruss* Tr = BahiaHonda())
			for (int32 i = 0; i <= Tr->Spans; i++)
			{
				const double PX = Lerp(Tr->X0, Tr->X1, double(i) / Tr->Spans), PZ = Lerp(Tr->Z0, Tr->Z1, double(i) / Tr->Spans);
				if (LandH(PX, PZ) < -.3) AddPillar(PX, PZ, 2.2, true);
			}
		if (OldBrX1 > OldBrX0) for (double X = OldBrX0; X <= OldBrX1; X += 30)
		{
			if (X > OldGap0 && X < OldGap1) continue;
			const double Z = ChainZ(X) + OldBrDZ;
			if (LandH(X, Z) > -.5) continue;
			AddPillar(X, Z, 1.3, true);
		}
	}

	const TArray<FIsland>& Islands() { return GIslands; }
	const TArray<FMarina>& Marinas() { return GMarinas; }
	const TArray<FHump>& Humps() { return GHumps; }
	const TArray<FPillar>& Pillars() { return GPillars; }

	void PillarsNear(double X, double Z, TArray<const FPillar*>& Out)
	{
		Out.Reset();
		const int32 GX = FMath::FloorToInt(X / PCell), GZ = FMath::FloorToInt(Z / PCell);
		for (int32 a = GX - 1; a <= GX + 1; a++)
			for (int32 b = GZ - 1; b <= GZ + 1; b++)
				if (const TArray<int32>* L = GPGrid.Find((int64(a) << 32) ^ int64(uint32(b))))
					for (int32 Idx : *L) Out.Add(&GPillars[Idx]);
	}

	FShore ShoreInfo(double X, double Z)
	{
		double Best = 1e9, BE = 9;
		const FIsland* BI = nullptr;
		if (GReal)
		{
			// the baked distance-to-shore grid and its nearest island
			const double D = KeysGeoReal::Shore(X, Z);
			const FIsland& I = GIslands[FMath::Min(KeysGeoReal::IslandAt(X, Z), GIslands.Num() - 1)];
			return {D, &I, 1 + D / FMath::Min((I.A + I.B) * .5, ShoreScale)};
		}
		for (const FIsland& I : GIslands)
		{
			const double D = IslandDist(I, X, Z);
			if (D < Best) { Best = D; BI = &I; }
		}
		if (BI) BE = BI->Coast >= 0 ? 1 + Best / FMath::Min((BI->A + BI->B) * .5, ShoreScale) : IslandE(*BI, X, Z);
		return {Best, BI, BE};
	}

	bool OnCourse(const FIsland* I, double LX, double LZ)
	{
		for (const FGolf& G : GGolf)
			if (G.I == I && FMath::Sqrt(FMath::Square((LX - G.LX) / G.RA) + FMath::Square((LZ - G.LZ) / G.RB)) < 1.06) return true;
		if (GReal) return false;   // (the real runway is asked for by world position: OnRealRunway)
		return !GReal && I == GVaca && LX > RunLX0 - 30 && LX < RunLX1 + 30 && FMath::Abs(LZ - RunLZ) < RunW;
	}

	double CreekDist(double X, double Z)
	{
		double Best = 1e9;
		for (const FCreek& C : GCreeks)
		{
			if (X < C.BX0 || X > C.BX1 || Z < C.BZ0 || Z > C.BZ1) continue;
			const TArray<FVector2D>& P = C.Pts;
			for (int32 i = 0; i < P.Num() - 1; i++)
			{
				const double AX = P[i].X, AZ = P[i].Y, VX = P[i + 1].X - AX, VZ = P[i + 1].Y - AZ, L2 = VX * VX + VZ * VZ;
				const double T = Clamp(((X - AX) * VX + (Z - AZ) * VZ) / L2, 0, 1);
				const double D = FMath::Sqrt(FMath::Square(X - AX - VX * T) + FMath::Square(Z - AZ - VZ * T)) - C.W / 2;
				if (D < Best) Best = D;
			}
		}
		return Best;
	}

	// ground height: beaches slope into the water, creeks carve through, golf courses and the runway are graded flat
	// the town is built on fill (web depth.ts TOWN_FILL, in the frame of MARINAS[0]): the ground under the pad is raised to
	// at least 1.6 m and kept under the pad's 2.35 m top so it never pokes through the streets
	bool InTownPad(double X, double Z)
	{
		if (!GMarinas.Num()) return false;
		const FMarina& M = GMarinas[0];
		const double U = M.Dir * (X - M.SX), V = M.Dir * (M.SZ - Z);
		return U >= -104 && U <= 104 && V >= 18 && V <= 172;
	}

	bool InFill(double X, double Z)
	{
		if (FMath::Abs(Z - ChainZ(X)) <= 9)
		{
			if (GReal && KeysGeoReal::Shore(X, Z) < -4) return true;   // US-1's roadbed wherever it runs over land
			for (const FVector2D& R : GUS1Runs) if (X >= R.X && X <= R.Y) return true;
		}
		if (GMarinas.Num())
		{
			const FMarina& M = GMarinas[0];
			const double U = M.Dir * (X - M.SX), V = M.Dir * (M.SZ - Z);
			if (U >= -112 && U <= 112 && V >= 14 && V <= 180) return true;
		}
		for (const FGolf& G : GGolf)
		{
			if (!G.I) continue;
			double LX, LZ;
			IslandLocal(*G.I, X, Z, LX, LZ);
			if (FMath::Sqrt(FMath::Square((LX - G.LX) / G.RA) + FMath::Square((LZ - G.LZ) / G.RB)) < 1.03) return true;
		}
		if (GReal) return OnRealRunway(X, Z, 6) && KeysGeoReal::Shore(X, Z) < -2;
		if (!GVaca) return false;
		double LX, LZ;
		IslandLocal(*GVaca, X, Z, LX, LZ);
		return LX > RunLX0 - 30 && LX < RunLX1 + 30 && LZ > RunLZ - RunW / 2 - 4 && LZ < RunLZ + 95;
	}

	// the dredged basin past each marina's channel mouth (web depth.ts MARINA_CUT): r0..r1 out, halfW across
	static constexpr double CutR0 = 66, CutR1 = 190, CutHalfW = 45, CutD = 3.2, CutFeather = 12;
	double MarinaCutDepth(double X, double Z)
	{
		double Best = -1e9;
		for (const FMarina& M : GMarinas)
		{
			const double R = (Z - M.SZ) * M.Dir;
			if (R < CutR0 || R > CutR1) continue;
			const double EndFade = FMath::Min(1.0, (CutR1 - R) / 18), In = (CutHalfW - FMath::Abs(X - M.SX)) * EndFade;
			if (In > Best) Best = In;
		}
		return Best;
	}

	double LandH(double X, double Z)
	{
		const FShore S = ShoreInfo(X, Z);
		const double E = S.E;
		const bool bFill = InFill(X, Z);
		if (E > 1.08) return bFill ? 1.6 : -2;
		double H = -1.2 + 2.85 * SS(1.07, .9, E);
		if (E < .92) H += (S.Isl->Coast >= 0 ? .15 : .25) * (FMath::Sin(X * .021) * FMath::Cos(Z * .017) + FMath::Sin(X * .0063 + Z * .0081)) * SS(.92, .72, E);
		double LX, LZ;
		IslandLocal(*S.Isl, X, Z, LX, LZ);
		if (OnCourse(S.Isl, LX, LZ)) H = FMath::Min(H, 1.85);
		const bool bRealRun = GReal && E < 1 && OnRealRunway(X, Z, 4);
		// the real ground: mostly a metre or two, with the odd rise (the old railway grades, the hammocks)
		if (GReal && E < .9 && !bRealRun) H += Clamp(KeysGeoReal::Height(X, Z) - 1.65, 0, 2.5) * SS(.9, .7, E);
		if (bRealRun) H = FMath::Min(H, 1.85);
		const double CD = CreekDist(X, Z);
		if (CD < 10) H = FMath::Min(H, Lerp(-1.6, H, SS(-2, 10, CD)));
		const double MC = GReal ? -1e9 : MarinaCutDepth(X, Z);   // on the real map the basins are already water
		if (MC > -CutFeather) H = FMath::Min(H, Lerp(H, -1.6, Clamp((MC + CutFeather) / CutFeather, 0, 1)));
		if (bFill) H = FMath::Max(H, 1.6);
		if (GMarinas.Num())
		{
			// under the town's pad the ground stays below the pad's 2.35 m top so it never pokes through the streets
			const FMarina& M = GMarinas[0];
			const double U = M.Dir * (X - M.SX), V = M.Dir * (M.SZ - Z);
			if (U >= -112 && U <= 112 && V >= 14 && V <= 180) H = FMath::Min(H, 2.2);
		}
		// the dry stack and the boatyard (KeysYards.cpp): made ground under their pads, their basin and slip dredged
		{ double YH; if (GReal && KeysYards::Ground(X, Z, YH)) H = YH; }
		// a berth dredged under a private dock where the shore is thin (KeysDocks.cpp)
		{ double PD; if (GReal && H < .05 && KeysDocks::Pocket(X, Z, PD)) H = FMath::Min(H, -PD); }
		return H;
	}

	double OffshoreF(double X, double Z) { return Clamp((DZAt(X, Z) - 1650) / 2200, 0, 1); }

	// the marked channels on the real map: the line through each channel's daybeacons in number order
	struct FNavSeg { double AX, AZ, BX, BZ, X0, X1, Z0, Z1; };
	static TArray<FNavSeg> GNavSegs;
	static constexpr double NavHalfW = 16, NavFeather = 20, NavDepth = 2.7;
	static void BuildNavSegs()
	{
		GNavSegs.Reset();
		TMap<FString, TArray<const KeysGeoReal::FNavMark*>> Ch;
		for (const KeysGeoReal::FNavMark& M : KeysGeoReal::Marks())
		{
			FString N = M.Name;
			for (const TCHAR* Cut : {TEXT(" Daybeacon"), TEXT(" Light"), TEXT(" Buoy")})
			{
				const int32 At = N.Find(Cut, ESearchCase::CaseSensitive, ESearchDir::FromEnd);
				if (At > 0) N.LeftInline(At);
			}
			Ch.FindOrAdd(N).Add(&M);
		}
		for (TPair<FString, TArray<const KeysGeoReal::FNavMark*>>& C : Ch)
		{
			C.Value.Sort([](const KeysGeoReal::FNavMark& A, const KeysGeoReal::FNavMark& B) { return A.Num < B.Num; });
			for (int32 i = 0; i + 1 < C.Value.Num(); i++)
			{
				const KeysGeoReal::FNavMark &A = *C.Value[i], &B = *C.Value[i + 1];
				if (FMath::Square(B.X - A.X) + FMath::Square(B.Z - A.Z) > 420.0 * 420.0) continue;   // marks miles apart across open water
				const double Pad = NavHalfW + NavFeather;
				GNavSegs.Add({A.X, A.Z, B.X, B.Z, FMath::Min(A.X, B.X) - Pad, FMath::Max(A.X, B.X) + Pad, FMath::Min(A.Z, B.Z) - Pad, FMath::Max(A.Z, B.Z) + Pad});
			}
		}
		UE_LOG(LogTemp, Log, TEXT("Keys Run: %d marked channels, %d channel legs"), Ch.Num(), GNavSegs.Num());
	}
	// how far inside a marked channel: 1 in it, fading to 0 over the feather
	static double NavChannelK(double X, double Z)
	{
		double Best = 1e9;
		for (const FNavSeg& S : GNavSegs)
		{
			if (X < S.X0 || X > S.X1 || Z < S.Z0 || Z > S.Z1) continue;
			const double VX = S.BX - S.AX, VZ = S.BZ - S.AZ, L2 = FMath::Max(1e-6, VX * VX + VZ * VZ);
			const double T = Clamp(((X - S.AX) * VX + (Z - S.AZ) * VZ) / L2, 0, 1);
			Best = FMath::Min(Best, FMath::Sqrt(FMath::Square(X - S.AX - VX * T) + FMath::Square(Z - S.AZ - VZ * T)));
		}
		return Clamp(1 - (Best - NavHalfW) / NavFeather, 0, 1);
	}

	bool OnMangrove(double X, double Z) { return GReal && KeysGeoReal::MangroveAt(X, Z) >= 200; }
	// how close to mangrove, per cell of the real map's grid: 255 on it, falling to 0 some 75 m off (built once)
	static TArray<uint8> GMangNear;
	static void BuildMangNear()
	{
		const int32 NX = KeysGeoReal::NX(), NZ = KeysGeoReal::NZ();
		GMangNear.SetNumZeroed(NX * NZ);
		for (int32 J = 0; J < NZ; J++) for (int32 I = 0; I < NX; I++) if (KeysGeoReal::MangroveCell(I, J)) GMangNear[J * NX + I] = 255;
		TArray<uint8> Tmp;
		for (int32 Pass = 0; Pass < 7; Pass++)
		{
			Tmp = GMangNear;
			for (int32 J = 1; J < NZ - 1; J++)
				for (int32 I = 1; I < NX - 1; I++)
				{
					int32 M = 0;
					for (int32 b = -1; b <= 1; b++) for (int32 a = -1; a <= 1; a++) M = FMath::Max(M, int32(Tmp[(J + b) * NX + I + a]) - (a && b ? 48 : 34));
					if (M > Tmp[J * NX + I]) GMangNear[J * NX + I] = uint8(M);
				}
		}
	}
	double MangroveNear(double X, double Z)
	{
		if (!GReal || !GMangNear.Num()) return 0;
		const int32 NX = KeysGeoReal::NX(), NZ = KeysGeoReal::NZ();
		const double FX = Clamp((X - KeysGeoReal::X0()) / KeysGeoReal::Cell(), 0, NX - 1.001), FZ = Clamp((Z - KeysGeoReal::Z0()) / KeysGeoReal::Cell(), 0, NZ - 1.001);
		const int32 I = int32(FX), J = int32(FZ);
		const double U = FX - I, V = FZ - J;
		const uint8* P = GMangNear.GetData() + J * NX + I;
		return ((P[0] * (1 - U) + P[1] * U) * (1 - V) + (P[NX] * (1 - U) + P[NX + 1] * U) * V) / 255.0;
	}
	bool InMarkedChannel(double X, double Z) { return GReal && GNavSegs.Num() && NavChannelK(X, Z) > .3; }

	// the real bottom: NOAA's soundings, easing up to the shoreline, with the marinas dredged
	static double RealDepthAt(double X, double Z)
	{
		const double SD = KeysGeoReal::Shore(X, Z);
		double D = FMath::Max(.3, -KeysGeoReal::Height(X, Z));
		// the banks: the survey grid averages a bank's crown away (hardly anything reads under a metre), so what it has
		// under 2.4 m is drawn down toward a low-tide flat - a bank charted at 1.3 m plays as 0.7 m, one at 1 m as 0.45 m.
		// Deeper water is as surveyed.
		if (D < 2.4) D = .25 + 2.15 * FMath::Pow((D - .3) / 2.1, 2.2);
		// the marked channels are kept open between their daybeacons
		if (SD > 1 && D < NavDepth && GNavSegs.Num())
		{
			const double K = NavChannelK(X, Z) * FMath::Min(1.0, (SD - 1) / 8);
			if (K > 0) D = Lerp(D, NavDepth, K * K * (3 - 2 * K));
		}
		if (SD < 25) { const double T = FMath::Max(0.0, SD) / 25; D = FMath::Min(D, .35 + T * (2 - T) * (D - .35)); }
		// the mangrove creeks are kept open from bank to bank: seven feet of dark water, shoaling under the roots
		if (GCreeks.Num()) { const double CD = CreekDist(X, Z); if (CD < 8) D = FMath::Max(D, Lerp(.35, 2.3, SS(8, 1, CD))); }
		const double DZ = Z - ChainZ(X);
		for (const FHump& H : GHumps)
			if (!H.bPatch) D = FMath::Min(D, 40 + FMath::Sqrt(FMath::Square(X - H.X) + FMath::Square(DZ - H.DZ)) * .32);
		if (SD > 2)
		{
			for (const FMarina& M : GMarinas)
			{
				const double R = (Z - M.SZ) * M.Dir;
				if (FMath::Abs(X - M.SX) < 34 && R > -2 && R < 78) D = FMath::Max(D, 3.2);
			}
			const double MC = MarinaCutDepth(X, Z);
			if (MC > 0) D = FMath::Max(D, .35 + (CutD - .35) * FMath::Min(1.0, MC / CutFeather) * FMath::Min(1.0, (SD - 2) / 10));
		}
		// the yards' basin and slip are dredged; their pads are dry land (KeysYards.cpp)
		{ double YH; if (KeysYards::Ground(X, Z, YH)) D = YH < 0 ? FMath::Max(D, -YH) : .3; }
		{ double PD; if (KeysDocks::Pocket(X, Z, PD)) D = FMath::Max(D, PD); }
		return FMath::Max(.3, D);
	}

	double DepthAt(double X, double Z)
	{
		if (GReal) return RealDepthAt(X, Z);
		const double DZ = Z - ChainZ(X);
		double D;
		if (DZ < 0) D = 1.4 + FMath::Min(-DZ, 2600.0) / 2600 * 3.4;            // Florida Bay
		else if (DZ < 1300) D = 2.2 + FMath::Min(DZ, 520.0) / 520 * 5.6;       // Hawk Channel
		else if (DZ < 1460) D = 7.8 - (DZ - 1300) / 160 * 4.4;                 // Sombrero Reef crest
		else if (DZ < 1650) D = 3.4 + (DZ - 1460) / 190 * 42;                  // reef wall
		else D = 45.4 + (DZ - 1650) * .14;                                     // Gulf Stream
		if (DZ < 1460)
		{
			const double N = FMath::Sin(X * .0047 + 1.7) * FMath::Cos(Z * .0056 - .4) + .5 * FMath::Sin(X * .011 + Z * .0083) + .3 * FMath::Sin(X * .031 - Z * .027);
			D += N * (DZ < 0 ? 1.15 : .9);
		}
		for (const FHump& H : GHumps)
		{
			const double HD = FMath::Sqrt(FMath::Square(X - H.X) + FMath::Square(DZ - H.DZ));
			D = FMath::Min(D, H.bPatch ? 2.4 + HD * .03 : 40 + HD * .32);
		}
		if (FMath::Abs(DZ) < 60) D = FMath::Max(D, 4.6 + (60 - FMath::Abs(DZ)) / 60 * 2.4);   // channels under US-1
		const double SD = ShoreInfo(X, Z).D;
		if (SD < 70) { const double T = FMath::Max(0.0, SD) / 70; D = FMath::Min(D, .35 + T * T * (D - .35)); }
		for (const FChannel& C : GChannels) if (X > C.X0 && X < C.X1 && DZ > C.Z0 && DZ < C.Z1 && SD > 2) D = FMath::Max(D, C.D);
		for (const FChannel& C : GMiamiCh) if (X > C.X0 && X < C.X1 && Z > C.Z0 && Z < C.Z1 && SD > 1) D = FMath::Max(D, C.D);
		{ const double CD = CreekDist(X, Z); if (CD < 0) D = FMath::Max(D, 2.6 + FMath::Min(1.5, -CD * .1)); }
		for (const FMarina& M : GMarinas)
		{
			const double R = (Z - M.SZ) * M.Dir;
			if (FMath::Abs(X - M.SX) < 34 && R > -2 && R < 78) D = FMath::Max(D, 3.2);
		}
		{
			const double MC = MarinaCutDepth(X, Z);
			if (MC > 0) D = FMath::Max(D, .35 + (CutD - .35) * FMath::Min(1.0, MC / CutFeather));
		}
		return FMath::Max(.3, D);
	}

	template <typename T>
	static T GridAt(const TArray<T>& G, double X, double Z)
	{
		const double FX = Clamp((X - WorldX0) / DG, 0, DGNX - 1.001), FZ = Clamp((Z - WorldZ0) / DG, 0, DGNZ - 1.001);
		const int32 i = int32(FX), j = int32(FZ);
		const float U = float(FX - i), V = float(FZ - j);
		const int32 K = j * DGNX + i;
		return (G[K] * (1 - U) + G[K + 1] * U) * (1 - V) + (G[K + DGNX] * (1 - U) + G[K + DGNX + 1] * U) * V;
	}

	double DepthFast(double X, double Z) { return GridAt(DepthG, X, Z); }

	// the depth grid over a rectangle read again from DepthAt (the docks' berths are dredged after the grids are made)
	void RebakeDepth(double X0, double Z0, double X1, double Z1)
	{
		if (!DepthG.Num()) return;
		for (int32 j = FMath::Max(0, FMath::FloorToInt((Z0 - WorldZ0) / DG)); j <= FMath::Min(DGNZ - 1, FMath::CeilToInt((Z1 - WorldZ0) / DG)); j++)
			for (int32 i = FMath::Max(0, FMath::FloorToInt((X0 - WorldX0) / DG)); i <= FMath::Min(DGNX - 1, FMath::CeilToInt((X1 - WorldX0) / DG)); i++)
			{
				const double D = DepthAt(WorldX0 + i * DG, WorldZ0 + j * DG);
				if (D > DepthG[j * DGNX + i] + .05) { DepthG[j * DGNX + i] = float(D); ColorG[j * DGNX + i] = DepthRGB(D); }
			}
	}
	FLinearColor DepthColorFast(double X, double Z) { return GridAt(ColorG, X, Z); }

	bool NearBridge(double X, double Z)
	{
		const double DZ = Z - ChainZ(X);
		if (X < WBX0 || X > WBX1) return false;
		return FMath::Abs(DZ) < 38 || (X > OldBrX0 && X < OldBrX1 && FMath::Abs(DZ - OldBrDZ) < 30);
	}

	const FHump* NearHump(double X, double Z)
	{
		const double DZ = Z - ChainZ(X);
		for (const FHump& H : GHumps) if (!H.bPatch && FMath::Sqrt(FMath::Square(X - H.X) + FMath::Square(DZ - H.DZ)) < 420) return &H;
		return nullptr;
	}

	FString ZoneAt(double X, double Z)
	{
		const double D = DepthAt(X, Z), DZ = DZAt(X, Z);
		if (CreekDist(X, Z) < 0) return TEXT("Creek");
		if (GReal && D < 3.2 && MangroveNear(X, Z) > .6) return TEXT("Creek");   // under and along the mangroves
		if (NearBridge(X, Z) && D >= 2 && ShoreInfo(X, Z).D > 6) return TEXT("Bridge");
		if (D < (GReal ? 1.5 : 2.6)) return TEXT("Flats");   // the real bay is 2-3 m all over; its flats are the banks
		if (DZ < 0) return TEXT("Backcountry");
		if (D >= 45) return TEXT("Offshore");
		if (DZ > 1250) return TEXT("Reef");
		return TEXT("Hawk Channel");
	}

	FString ZoneDesc(const FString& Zone)
	{
		static const TMap<FString, FString> Desc = {
			{TEXT("Creek"), TEXT("Mangrove creek")}, {TEXT("Wreck"), TEXT("Shipwreck")}, {TEXT("Oil Rig"), TEXT("Oil rig")}, {TEXT("Weedline"), TEXT("Weedline")}, {TEXT("Flats"), TEXT("Skinny-water flats")}, {TEXT("Backcountry"), TEXT("Florida Bay backcountry")},
			{TEXT("Bridge"), TEXT("Bridge channel")}, {TEXT("Hawk Channel"), TEXT("Hawk Channel")}, {TEXT("Reef"), TEXT("Sombrero Reef")}, {TEXT("Offshore"), TEXT("Gulf Stream")}};
		const FString* F = Desc.Find(Zone);
		return F ? *F : Zone;
	}

	// Miami is on the original map only for now
	bool InMiami(double X, double Z) { return !GReal && X > -500 && X < 4200 && Z < -1450; }
	bool InCity(double X, double Z) { return !GReal && X > 250 && X < 3900 && Z < -2560 && Z > -3600; }

	int32 SurfaceKind(double X, double Z)
	{
		if (InCity(X, Z)) return 3;
		const FShore S = ShoreInfo(X, Z);
		double LX, LZ;
		IslandLocal(*S.Isl, X, Z, LX, LZ);
		if (GReal && OnRealRunway(X, Z, 0)) return 2;
		if (OnMangrove(X, Z) && !InFill(X, Z)) return 4;
		if (!GReal && S.Isl == GVaca && LX > RunLX0 - 30 && LX < RunLX1 + 30 && FMath::Abs(LZ - RunLZ) < RunW) return 2;
		return OnCourse(S.Isl, LX, LZ) ? 1 : 0;
	}

	FString NearestName(double X, double Z)
	{
		const FShore S = ShoreInfo(X, Z);
		return (S.D < 120 ? TEXT("off ") : TEXT("toward ")) + S.Isl->Name;
	}

	const FMarina* NearMarina(double X, double Z)
	{
		for (const FMarina& M : GMarinas)
		{
			const double DZ = Clamp(Z, FMath::Min(M.SZ, M.EZ), FMath::Max(M.SZ, M.EZ));
			if (FMath::Sqrt(FMath::Square(X - M.SX) + FMath::Square(Z - DZ)) < 20) return &M;
		}
		return nullptr;
	}

	// the chart colors from the JS game, written as sRGB; vertex colors and textures want them linear
	static FLinearColor DepthRGBSrgb(double D);
	FLinearColor DepthRGB(double D)
	{
		const FLinearColor S = DepthRGBSrgb(D);
		return FLinearColor(FMath::Pow(S.R, 2.2f), FMath::Pow(S.G, 2.2f), FMath::Pow(S.B, 2.2f));
	}

	static FLinearColor DepthRGBSrgb(double D)
	{
		static const double Stops[8][4] = {{.3, .70, .94, .86}, {1.2, .45, .89, .83}, {3, .22, .79, .81}, {7, .12, .63, .75},
			{15, .07, .49, .67}, {45, .05, .33, .56}, {150, .04, .22, .46}, {600, .03, .15, .37}};
		auto Col = [](const double* S) { return FLinearColor(float(S[1]), float(S[2]), float(S[3])); };
		if (D <= Stops[0][0]) return Col(Stops[0]);
		for (int32 i = 1; i < 8; i++)
		{
			if (D <= Stops[i][0])
			{
				const double* A = Stops[i - 1];
				const double* B = Stops[i];
				const double T = (FMath::Loge(D) - FMath::Loge(A[0])) / (FMath::Loge(B[0]) - FMath::Loge(A[0]));
				return FLinearColor(float(Lerp(A[1], B[1], T)), float(Lerp(A[2], B[2], T)), float(Lerp(A[3], B[3], T)));
			}
		}
		return Col(Stops[7]);
	}

	const TArray<FSeaState>& SeaStates() { return GSea; }
	void SetSeaState(int32 Index)
	{
		GSeaIdx = Index;
		if (GSea.IsValidIndex(Index)) { BaseWind = GSea[Index].Wind; BaseSwell = GSea[Index].SW; }
	}
	int32 SeaIndex() { return GSeaIdx; }
	double SeaSW() { return SW; }
	double SeaCH() { return CH; }
	double WindSpeed() { return WindNow; }
	double WindDir() { return WindDirNow; }
	double SeaConfusion() { return ConfNow; }
	double Whitecaps() { return CapsNow; }
	double WaveRms() { return RmsNow; }
	void SetStormWind(double Speed, double DirToward, double Confuse, double SwellAdd)
	{
		StormSpd = FMath::Max(0.0, Speed); StormDir = DirToward; StormConf = Clamp(Confuse, 0, 1); StormSwell = FMath::Max(0.0, SwellAdd);
	}

	static double AngDiff(double A, double B) { return FMath::UnwindRadians(A - B); }

	// the wind sea a steady wind U raises (fetch-limited, the Keys are never far from land): height factor, peak length
	static double ChopFor(double U) { return FMath::Min(3.2, .04 + 1.2 * FMath::Square(U / 7)); }
	static double PeakLen(double U) { return Clamp(2.0 * U + .06 * U * U, 6, 48); }

	static void InitChop()
	{
		if (GChopInit) return;
		GChopInit = true;
		uint32 Sd = 0x5eed1234u;
		auto Rnd = [&Sd]() { Sd = Sd * 1664525u + 1013904223u; return double(Sd >> 8) / 16777216.0; };
		for (int32 l = 0; l < ChopL; l++)
			for (int32 d = 0; d < ChopD; d++)
			{
				FWaveTrain& W = GChop[l * ChopD + d];
				// directions jittered and lengths spread a little, so no two trains line up into a visible grid
				const double Ang = (d + .5 * Rnd() - .25) * 2 * PI / ChopD + l * .37;
				W.Len = ChopLen[l] * (.9 + .2 * Rnd());
				const double K = 2 * PI / W.Len;
				W.KX = FMath::Cos(Ang) * K; W.KZ = FMath::Sin(Ang) * K;
				W.W = FMath::Sqrt(9.81 * K);
				W.Ph = Rnd() * 2 * PI;
				W.A = 0; W.S = 0;
			}
	}

	// rebuild the live list from the eased driver
	static void Reweight()
	{
		InitChop();
		GLive.Reset();
		double Sum2 = 0;
		for (int32 i = 0; i < 4; i++)
		{
			const double* W = SWELL[i];
			// phase = k.x + w t: the swell rolls in from the southeast
			FWaveTrain T{W[0], W[1], W[2], W[3] * SW, W[4], W[5], 2 * PI / FMath::Sqrt(W[0] * W[0] + W[1] * W[1])};
			GLive.Add(T);
			Sum2 += T.A * T.A * .5;
		}
		// the wind sea: lengths around the peak the wind can raise, spread around the direction the sea runs, wider when confused
		const double Lp = PeakLen(WindNow);
		const double Steep = Clamp((WindNow - 3) / 12, 0, 1);
		double Wt[ChopL * ChopD], WSum2 = 0;
		for (int32 i = 0; i < ChopL * ChopD; i++)
		{
			const FWaveTrain& W = GChop[i];
			const double Lw = FMath::Exp(-FMath::Square(FMath::Loge(W.Len / Lp)) / .3);
			const double C = FMath::Cos(AngDiff(FMath::Atan2(W.KZ, W.KX), SeaDirNow));
			const double Dw = Lerp(FMath::Pow(FMath::Max(0.0, C), 4.0), .25 + .25 * FMath::Max(0.0, C), ConfNow);
			Wt[i] = Lw * Dw;
			WSum2 += Wt[i] * Wt[i];
		}
		// the old four chop trains' rms height per unit of CH, so the sea states keep their size
		const double Rms = .126 * CH, Norm = WSum2 > 1e-9 ? Rms * FMath::Sqrt(2.0) / FMath::Sqrt(WSum2) : 0;
		double ChopSum2 = 0;
		if (KeysOcean::Enabled())
		{
			// the simulated sea (KeysOcean) carries the wind sea at the same rms
			RmsNow = FMath::Sqrt(Sum2 + Rms * Rms);
			return;
		}
		for (int32 i = 0; i < ChopL * ChopD; i++)
		{
			const double A = Wt[i] * Norm;
			if (A < .002) continue;
			FWaveTrain T = GChop[i];
			T.A = A;
			// short seas peak up as the wind builds, steepest when confused (a storm's outflow)
			T.S = FMath::Min(.42, (.12 + .18 * Steep + .12 * ConfNow) * (T.Len < 12 ? 1.0 : .8));
			T.W = -T.W;   // phase = k.x - w t: runs along k
			GLive.Add(T);
			ChopSum2 += A * A * .5;
		}
		RmsNow = FMath::Sqrt(Sum2 + ChopSum2);
	}

	// the simulated wind sea follows the same driver: rms height from the sea state, the peak length from the wind,
	// the direction the sea runs, its confusion and steepness
	static double GOceanT = 0;
	static void DriveOcean(double Dt)
	{
		if (!KeysOcean::Enabled()) return;
		GOceanT += Dt;
		KeysOcean::Update(GOceanT, .126 * CH, PeakLen(WindNow), SeaDirNow, ConfNow, Clamp((WindNow - 3) / 12, 0, 1));
	}

	void TickSea(double Dt)
	{
		Dt = Clamp(Dt, 0, .25);
		// the wind: base trade wind plus the storm's outflow, as vectors
		const double BX = FMath::Cos(BaseDir) * BaseWind + FMath::Cos(StormDir) * StormSpd, BZ = FMath::Sin(BaseDir) * BaseWind + FMath::Sin(StormDir) * StormSpd;
		const double TU = FMath::Sqrt(BX * BX + BZ * BZ), TD = TU > .05 ? FMath::Atan2(BZ, BX) : WindDirNow;
		const double KW = 1 - FMath::Exp(-Dt / 5);
		WindNow += (TU - WindNow) * KW;
		WindDirNow = FMath::UnwindRadians(WindDirNow + AngDiff(TD, WindDirNow) * KW);
		// the sea follows the wind's direction slowly; while it lags, the old and new seas cross (confused)
		SeaDirNow = FMath::UnwindRadians(SeaDirNow + AngDiff(WindDirNow, SeaDirNow) * (1 - FMath::Exp(-Dt / 45)));
		const double ConfT = FMath::Max(StormConf, Clamp(FMath::Abs(AngDiff(WindDirNow, SeaDirNow)) / 1.3, 0, 1));
		ConfNow += (ConfT - ConfNow) * (1 - FMath::Exp(-Dt / (ConfT > ConfNow ? 15.0 : 90.0)));
		// the sea builds over half a minute and lies down over a few minutes
		const double ChT = ChopFor(WindNow);
		CH += (ChT - CH) * (1 - FMath::Exp(-Dt / (ChT > CH ? 30.0 : 150.0)));
		const double SwT = BaseSwell + StormSwell;
		SW += (SwT - SW) * (1 - FMath::Exp(-Dt / 120));
		CapsNow = SS(4.5, 12, WindNow) * Clamp(CH / ChopFor(WindNow) * 1.2, 0, 1) + .25 * ConfNow * SS(6, 14, WindNow);
		CapsNow = Clamp(CapsNow, 0, 1);
		Reweight();
		DriveOcean(Dt);
	}

	void SnapSea()
	{
		const double BX = FMath::Cos(BaseDir) * BaseWind + FMath::Cos(StormDir) * StormSpd, BZ = FMath::Sin(BaseDir) * BaseWind + FMath::Sin(StormDir) * StormSpd;
		WindNow = FMath::Sqrt(BX * BX + BZ * BZ);
		if (WindNow > .05) WindDirNow = SeaDirNow = FMath::Atan2(BZ, BX);
		ConfNow = StormConf;
		CH = ChopFor(WindNow);
		SW = BaseSwell + StormSwell;
		CapsNow = Clamp(SS(4.5, 12, WindNow) + .25 * ConfNow * SS(6, 14, WindNow), 0, 1);
		Reweight();
		DriveOcean(0);
	}

	// small inshore, big rollers in deep blue water
	double AmpFor(double D) { return .1 + .9 * Clamp((D - 1) / 40, 0, 1) + 1 * Clamp((D - 45) / 250, 0, 1); }
	// (a creek through the mangroves is out of the wind and the swell: all but glassy between its banks)
	double AmpAt(double X, double Z)
	{
		const double A = AmpFor(DepthFast(X, Z)) * (1 + .9 * GridAt(InletG, X, Z));
		return GReal && GCreeks.Num() ? A * Lerp(.1, 1.0, SS(0, 14, CreekDist(X, Z))) : A;
	}

	// The swell scales with Amp (depth and inlets); the wind sea much less, so a blow still chops up the flats.
	static FORCEINLINE double ChopAmp(double Amp) { return FMath::Min(1.3, .3 + .7 * Amp) * FMath::Min(1.0, Amp * 10); }   // (open water never has Amp under .1: only shelter takes the chop off)

	double ChopScale(double Amp) { return ChopAmp(Amp); }

	FVector2D SurfAt(double X, double Z, double T, double D)
	{
		if (D <= .03 || D > 5) return FVector2D::ZeroVector;
		const double GX = (DepthFast(X + 1.5, Z) - DepthFast(X - 1.5, Z)) / 3, GZ = (DepthFast(X, Z + 1.5) - DepthFast(X, Z - 1.5)) / 3;
		const double Slope = FMath::Sqrt(GX * GX + GZ * GZ);
		// a beach face or a reef edge, not the endless flat flats (and nothing out past 5 m)
		// (and no surf on the banks of a mangrove creek: nothing breaks in there)
		const double Gate = SS(.006, .03, Slope) * (1 - SS(3.5, 5.0, D)) * (GReal && GCreeks.Num() ? SS(0, 14, CreekDist(X, Z)) : 1.0);
		if (Gate <= 0) return FVector2D::ZeroVector;
		// the swell that reaches the shore (the reef and the keys take most of it), shoaling up (Green's law) until it breaks
		const double H0 = .14 + .22 * SW;
		const double Hs = H0 * FMath::Pow(4.0 / FMath::Max(D, .15), .25), Hb = .78 * D;
		const double H = FMath::Min(Hs, Hb);
		const double Bk = SS(.6, .95, Hs / Hb);   // how broken it is
		// phase from the travel time to the shoreline on this slope (shallow-water speed sqrt(g d)), so the crests run
		// along the depth contours and arrive square to the beach; a little wobble so the lines aren't ruler-straight
		const double Om = 2 * PI / 7.5;
		const double Tau = 2 * FMath::Sqrt(D) / (FMath::Max(Slope, .006) * FMath::Sqrt(9.81));
		double Ph = Om * (Tau + T) + .7 * FMath::Sin(X * .043 + Z * .021) + .5 * FMath::Sin(Z * .057 - X * .017);
		Ph += .55 * Bk * FMath::Sin(Ph);   // a breaking wave's face steepens and its back lengthens
		const double C = .5 + .5 * FMath::Cos(Ph);
		const double Prof = FMath::Pow(C, FMath::Lerp(1.5, 5.0, Bk));
		const double Height = H * (Prof - .22) * Gate;
		// white water: the curl at the crest as it breaks, and the bore of foam rolling in behind it
		const double U = FMath::Frac(Ph / (2 * PI) + .02);
		const double Foam = Gate * FMath::Min(1.0, Bk * 1.3) * FMath::Max(SS(.66, .97, C), .9 * FMath::Exp(-U * 2.6));
		return FVector2D(Height, Foam);
	}

	double WaveHLod(double X, double Z, double T, double Amp, double MinLen)
	{
		if (!GLive.Num()) Reweight();
		double HS = 0, HC = 0;
		for (int32 i = 0; i < GLive.Num(); i++)
		{
			const FWaveTrain& W = GLive[i];
			if (W.Len < MinLen) continue;
			const double P = W.KX * X + W.KZ * Z + W.W * T + W.Ph;
			const double V = W.A * (FMath::Sin(P) - W.S * FMath::Cos(2 * P));
			if (i < 4) HS += V; else HC += V;
		}
		if (KeysOcean::Enabled()) HC += KeysOcean::Height(X, Z, MinLen);
		return Amp * HS + ChopAmp(Amp) * HC;
	}

	double WaveH(double X, double Z, double T, double Amp, bool bWithWake)
	{
		// on top of the tide: every hull, bird, buoy and weed line that floats on WaveH rises and falls with it
		return KeysOcean::SeaLevel() + WaveHLod(X, Z, T, Amp, 0) + (bWithWake ? WakeH(X, Z, T) : 0);
	}

	FVector2D WaveSlope(double X, double Z, double T, double Amp)
	{
		if (!GLive.Num()) Reweight();
		double HX = 0, HZ = 0;
		const double CA = ChopAmp(Amp);
		for (int32 i = 0; i < GLive.Num(); i++)
		{
			const FWaveTrain& W = GLive[i];
			const double P = W.KX * X + W.KZ * Z + W.W * T + W.Ph;
			const double D = W.A * (FMath::Cos(P) + 2 * W.S * FMath::Sin(2 * P)) * (i < 4 ? Amp : CA);
			HX += W.KX * D;
			HZ += W.KZ * D;
		}
		if (KeysOcean::Enabled()) return FVector2D(HX, HZ) + KeysOcean::Slope(X, Z) * CA;
		return FVector2D(HX, HZ);
	}

	// boat wakes: every moving boat drops expanding wave rings from its stern; together they form the V-shaped Kelvin wake
	void EmitWake(double X, double Z, double V, double Len, double T, double Interval)
	{
		if (V < 2) return;
		// the arms of the V are the overlap of many rings, so their height scales with how often rings are dropped
		const double Rate = Clamp(Interval / .35, .05, 1.5);
		// At displacement speeds the rings are dropped so close together that they stack into a wall; a slow hull
		// (trolling, idling in) pushes a much smaller wake, so the height eases in with speed.
		// (At trolling speed, ~2.7 m/s, the rings drop 0.2 m apart and still summed into a wide white mound astern.)
		const double Slow = .1 + .9 * WSStep(2.5, 10, V);
		const double A = Slow * Rate * Clamp(Len / 10, .35, 1.3) * (.18 + .5 * FMath::Exp(-FMath::Square((V - 8) / 7)) + .26 * FMath::Min(V / 25, 1.0));
		FWakeRing& W = WakeP[WakeI];
		W.X = X; W.Z = Z; W.T = T; W.C = FMath::Max(1.5, .34 * V); W.A = FMath::Min(9.9, A);
		WakeI = (WakeI + 1) % WakeN;
	}

	// one ring's shape at its current age: radius R, packet width WD and height Amp
	static bool RingNow(const FWakeRing& W, double T, double& R, double& WD, double& Amp)
	{
		const double Age = T - W.T;
		if (W.A <= 0 || Age < .8 || Age > WakeLife) return false;
		R = W.C * Age + 1.2;
		WD = 1.6 + Age * .3;
		// fade in over the first second so new rings don't pop, and out over the last few so old ones don't vanish at once
		const double In = WSStep(.8, 1.8, Age), Out = 1 - WSStep(WakeLife - 5, WakeLife, Age);
		Amp = W.A * FMath::Exp(-Age / 9) / (1 + R * .03) * In * Out;
		return true;
	}

	static double RingH(double DR, double WD, double Amp)
	{
		// a short wave packet: the leading crest is the tallest, with a trough and a smaller crest behind it
		const double Q = DR / WD;
		return Amp * FMath::Exp(-Q * Q) * FMath::Cos(2.4 * Q - .35);
	}

	double WakeH(double X, double Z, double T)
	{
		double H = 0;
		for (int32 i = 0; i < WakeN; i++)
		{
			double R, WD, Amp;
			if (!RingNow(WakeP[i], T, R, WD, Amp)) continue;
			const double DX = X - WakeP[i].X, DZ = Z - WakeP[i].Z, Reach = R + WD * 3;
			if (FMath::Abs(DX) > Reach || FMath::Abs(DZ) > Reach) continue;
			const double DR = FMath::Sqrt(DX * DX + DZ * DZ) - R;
			if (FMath::Abs(DR) > WD * 3) continue;
			H += RingH(DR, WD, Amp);
		}
		return H;
	}

	void GatherWakes(double CX, double CZ, double Radius, double T, TArray<FLiveWake>& Out)
	{
		Out.Reset();
		for (int32 i = 0; i < WakeN; i++)
		{
			double R, WD, Amp;
			if (!RingNow(WakeP[i], T, R, WD, Amp) || Amp < .002) continue;
			const double Reach = R + WD * 3;
			if (FMath::Abs(WakeP[i].X - CX) > Radius + Reach || FMath::Abs(WakeP[i].Z - CZ) > Radius + Reach) continue;
			Out.Add({WakeP[i].X, WakeP[i].Z, R, WD, Amp, Reach});
		}
	}

	double WakeHFrom(const TArray<FLiveWake>& Live, const TArray<int32>& Only, double X, double Z)
	{
		double H = 0;
		for (const int32 I : Only)
		{
			const FLiveWake& W = Live[I];
			const double DX = X - W.X, DZ = Z - W.Z;
			if (FMath::Abs(DX) > W.Reach || FMath::Abs(DZ) > W.Reach) continue;
			const double DR = FMath::Sqrt(DX * DX + DZ * DZ) - W.R;
			if (FMath::Abs(DR) > W.WD * 3) continue;
			H += RingH(DR, W.WD, W.Amp);
		}
		return H;
	}

	void BucketWakes(const TArray<FLiveWake>& Live, double CX, double CZ, double Radius, double Cell, int32 N, TArray<TArray<int32>>& Out)
	{
		Out.SetNum(N * N);
		for (TArray<int32>& C : Out) C.Reset();
		const double X0 = CX - Radius, Z0 = CZ - Radius, Half = Cell * .7072;
		for (int32 I = 0; I < Live.Num(); I++)
		{
			const FLiveWake& W = Live[I];
			const double Band = W.WD * 3 + Half;
			const int32 A0 = FMath::Clamp(int32((W.X - W.R - Band - X0) / Cell), 0, N - 1), A1 = FMath::Clamp(int32((W.X + W.R + Band - X0) / Cell), 0, N - 1);
			const int32 B0 = FMath::Clamp(int32((W.Z - W.R - Band - Z0) / Cell), 0, N - 1), B1 = FMath::Clamp(int32((W.Z + W.R + Band - Z0) / Cell), 0, N - 1);
			for (int32 b = B0; b <= B1; b++)
				for (int32 a = A0; a <= A1; a++)
				{
					// only the cells the ring's band passes through
					const double Dx = X0 + (a + .5) * Cell - W.X, Dz = Z0 + (b + .5) * Cell - W.Z;
					if (FMath::Abs(FMath::Sqrt(Dx * Dx + Dz * Dz) - W.R) <= Band) Out[b * N + a].Add(I);
				}
		}
	}

	void BucketTrails(const TArray<FLiveTrail>& Live, double CX, double CZ, double Radius, double Cell, int32 N, TArray<TArray<int32>>& Out)
	{
		Out.SetNum(N * N);
		for (TArray<int32>& C : Out) C.Reset();
		const double X0 = CX - Radius, Z0 = CZ - Radius;
		for (int32 I = 0; I < Live.Num(); I++)
		{
			const FLiveTrail& L = Live[I];
			const int32 A0 = FMath::Clamp(int32((L.MinX - X0) / Cell), 0, N - 1), A1 = FMath::Clamp(int32((L.MaxX - X0) / Cell), 0, N - 1);
			const int32 B0 = FMath::Clamp(int32((L.MinZ - Z0) / Cell), 0, N - 1), B1 = FMath::Clamp(int32((L.MaxZ - Z0) / Cell), 0, N - 1);
			for (int32 b = B0; b <= B1; b++) for (int32 a = A0; a <= A1; a++) Out[b * N + a].Add(I);
		}
	}

	double WakeHFrom(const TArray<FLiveWake>& Live, double X, double Z)
	{
		double H = 0;
		for (const FLiveWake& W : Live)
		{
			const double DX = X - W.X, DZ = Z - W.Z;
			if (FMath::Abs(DX) > W.Reach || FMath::Abs(DZ) > W.Reach) continue;
			const double DR = FMath::Sqrt(DX * DX + DZ * DZ) - W.R;
			if (FMath::Abs(DR) > W.WD * 3) continue;
			H += RingH(DR, W.WD, W.Amp);
		}
		return H;
	}

	void EmitTrail(int32 Source, double X, double Z, double Width, double Strength, double T)
	{
		const int64* Last = TrailLast.Find(Source);
		FTrailPt& P = TrailP[TrailSerial % TrailN];
		P.X = X; P.Z = Z; P.T = T; P.W = Width; P.S = FMath::Clamp(Strength, 0.0, 1.0); P.Source = Source;
		P.Serial = TrailSerial;
		P.PrevSerial = Last ? *Last : -1;
		TrailLast.Add(Source, TrailSerial);
		TrailSerial++;
	}

	void GatherTrails(double CX, double CZ, double Radius, double T, TArray<FLiveTrail>& Out)
	{
		Out.Reset();
		auto Spread = [](const FTrailPt& P, double Age) { return P.W * (1 + Age * .07) + Age * .3; };
		for (int32 i = 0; i < TrailN; i++)
		{
			const FTrailPt& B = TrailP[i];
			if (B.Serial < 0 || B.PrevSerial < 0 || TrailSerial - B.PrevSerial > TrailN) continue;
			const FTrailPt& A = TrailP[B.PrevSerial % TrailN];
			if (A.Serial != B.PrevSerial || A.Source != B.Source) continue;
			const double AgeA = T - A.T, AgeB = T - B.T;
			if (AgeA > TrailLife || AgeB < 0 || (A.S <= 0 && B.S <= 0)) continue;
			// a long jump means the boat was moved (Miami, respawn) - don't draw a streak across the map
			if (FMath::Square(B.X - A.X) + FMath::Square(B.Z - A.Z) > 30.0 * 30.0) continue;
			const double WA = Spread(A, AgeA), WB = Spread(B, AgeB), WM = FMath::Max(WA, WB);
			FLiveTrail L;
			L.AX = A.X; L.AZ = A.Z; L.BX = B.X; L.BZ = B.Z; L.W0 = WA; L.W1 = WB;
			// the white wash breaks up within seconds; the pale green aerated water under it lasts much longer
			const double FadeEnd = 1 - WSStep(TrailLife - 6, TrailLife, AgeA);
			// (a boat on plane leaves a solid white column that stays white for a good 150 m)
			L.F0 = A.S * FMath::Exp(-AgeA / 8.0) * FadeEnd; L.F1 = B.S * FMath::Exp(-AgeB / 8.0);
			L.A0 = A.S * FMath::Exp(-AgeA / 13) * FadeEnd; L.A1 = B.S * FMath::Exp(-AgeB / 13);
			L.G0 = AgeA; L.G1 = AgeB; L.S0 = A.S; L.S1 = B.S;
			// (room for the crest lines peeling off either side, which spread faster than the wash)
			const double Reach = WM * 1.5 + FMath::Max(AgeA, AgeB) * 2.6 + 2;
			L.MinX = FMath::Min(A.X, B.X) - Reach; L.MaxX = FMath::Max(A.X, B.X) + Reach;
			L.MinZ = FMath::Min(A.Z, B.Z) - Reach; L.MaxZ = FMath::Max(A.Z, B.Z) + Reach;
			if (L.MaxX < CX - Radius || L.MinX > CX + Radius || L.MaxZ < CZ - Radius || L.MinZ > CZ + Radius) continue;
			Out.Add(L);
		}
	}

	FVector2D TrailFrom(const TArray<FLiveTrail>& Live, double X, double Z)
	{
		TArray<int32> All; All.SetNumUninitialized(Live.Num());
		for (int32 I = 0; I < Live.Num(); I++) All[I] = I;
		return TrailFrom(Live, All, X, Z);
	}

	FVector2D TrailFrom(const TArray<FLiveTrail>& Live, const TArray<int32>& Only, double X, double Z)
	{
		double Foam = 0, Aer = 0;
		for (const int32 LI : Only)
		{
			const FLiveTrail& L = Live[LI];
			if (X < L.MinX || X > L.MaxX || Z < L.MinZ || Z > L.MaxZ) continue;
			const double SX = L.BX - L.AX, SZ = L.BZ - L.AZ, SL = SX * SX + SZ * SZ;
			const double U = SL > 1e-6 ? FMath::Clamp(((X - L.AX) * SX + (Z - L.AZ) * SZ) / SL, 0.0, 1.0) : 0;
			const double D = FMath::Sqrt(FMath::Square(X - (L.AX + SX * U)) + FMath::Square(Z - (L.AZ + SZ * U)));
			const double W = FMath::Lerp(L.W0, L.W1, U), Q = D / FMath::Max(.1, W);
			// solid in the middle, ragged at the edges (the material breaks the edges up further)
			// fresh wash comes off twin motors as two white streams with a darker lane between; they merge as it ages
			const double Fr = FMath::Lerp(L.F0, L.F1, U), Young = FMath::Clamp((Fr - .35) / .5, 0.0, 1.0);
			// (only a hint of it: at speed the motors' wash boils together into one solid white column)
			const double Twin = 1 - .18 * Young * FMath::Exp(-Q * Q / .025);
			Foam = FMath::Max(Foam, Fr * Twin * (1 - WSStep(.45, 1.05, Q)));
			Aer = FMath::Max(Aer, FMath::Lerp(L.A0, L.A1, U) * (1 - WSStep(.5, 1.5, Q)));
			// the two white crest lines that peel off either side of the wash and spread wider behind (a planing hull's
			// own breaking wake): only off a fast boat (strong wash), fading over ~6 s
			const double Age = FMath::Lerp(L.G0, L.G1, U), St = FMath::Lerp(L.S0, L.S1, U);
			if (St > .55)
			{
				const double Rl = W * 1.02 + Age * 2.4, Hw = .4 + Age * .1;
				const double Line = FMath::Exp(-FMath::Square((D - Rl) / Hw)) * FMath::Exp(-Age / 6) * FMath::Clamp((St - .55) / .35, 0.0, 1.0);
				Foam = FMath::Max(Foam, .97 * Line);
			}
		}
		return FVector2D(Foam, Aer);
	}
}
