#include "KeysGeo.h"
#include "Async/ParallelFor.h"
#include "KeysCoastData.h"

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
	static constexpr double OldBrX0 = -3820, OldBrX1 = -2010, OldBrDZ = -58, OldGap0 = -3200, OldGap1 = -3080;
	// runway on Vaca Key, in island-local coordinates
	static constexpr double RunLX0 = 700, RunLX1 = 1420, RunLZ = 80, RunW = 34;

	// fast depth lookup grid (10 m) so the water surface can follow the boat
	static constexpr double DG = 10;
	static int32 DGN = 0;
	static TArray<float> DepthG, InletG;
	static TArray<FLinearColor> ColorG;

	static TArray<FSeaState> GSea;
	static int32 GSeaIdx = 1;
	static double SW = 1.3, CH = 1.2;

	// [kx, kz, angular speed, amplitude, crest sharpness, phase, swell?]
	static const double WAVES[8][7] = {
		{.0114, .0262, .53, .62, .12, 0, 1}, {.0302, .0322, .66, .32, .16, 1.7, 1}, {.074, .046, .93, .13, .28, 2.4, 0}, {-.052, .092, 1.02, .09, .25, .6, 0},
		{.0205, .0178, .516, .45, .1, 3.1, 1}, {-.0128, .0251, .526, .35, .1, 4.4, 1}, {.118, .071, 1.162, .06, .3, 1.1, 0}, {-.09, .13, 1.244, .05, .3, 5.2, 0}};

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

		struct FDef { const TCHAR* Name; double X, DZ, A, B; };
		const FDef Defs[] = {
			{TEXT("Little Duck Key"), -3860, 10, 230, 120}, {TEXT("Pigeon Key"), -2650, -62, 70, 42}, {TEXT("Knight's Key"), -2080, 0, 170, 110},
			{TEXT("Marathon (Vaca Key)"), -120, 0, 1680, 270}, {TEXT("Boot Key"), -1050, 560, 620, 170}, {TEXT("Fat Deer Key"), 1990, -30, 380, 230},
			{TEXT("Key Colony Beach"), 2050, 330, 260, 110}, {TEXT("Crawl Key"), 2750, -20, 300, 170}, {TEXT("Grassy Key"), 3640, -10, 520, 190},
			{TEXT("East Sister Rock"), 900, 560, 42, 30}, {TEXT("Molasses Keys"), -3150, 520, 70, 36},
			{TEXT("Rachel Key"), -1250, -950, 95, 55}, {TEXT("Bamboo Key"), 350, -1350, 140, 70}, {TEXT("Money Key"), 1450, -850, 80, 50},
			{TEXT("Bay mangroves"), -2600, -1500, 120, 70}, {TEXT("Florida mainland (Miami)"), 0, -3500, 6600, 760}};
		GIslands.Reset();
		for (const FDef& D : Defs)
		{
			FIsland I;
			I.Name = D.Name;
			I.X = D.X;
			I.Z = ChainZ(D.X) + D.DZ;
			I.A = D.A;
			I.B = D.B;
			I.Th = FMath::Atan2(0.000024 * D.X, 1.0) + (D.A < 150 ? (SRand() - .5) * 1.2 : 0);
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
		for (const FMDef& D : MDefs)
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
		for (int32 i = 0; i < GIslands.Num() && i < KeysCoastData::NumIslands; i++)
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

		// marinas: two in Boot Key Harbor, two on the Gulf side
		struct FMar { const TCHAR* Name; double SX; int32 Dir; };
		const FMar Mars[] = {{TEXT("Boot Key Harbor City Marina"), -900, 1}, {TEXT("Burdines Waterfront"), -1400, 1}, {TEXT("Faro Blanco Marina"), -1560, -1}, {TEXT("Keys Fisheries"), -600, -1}};
		GMarinas.Reset();
		for (const FMar& M : Mars)
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
		for (const TCHAR* Name : {TEXT("Little Duck Key"), TEXT("Knight's Key"), TEXT("Marathon (Vaca Key)"), TEXT("Fat Deer Key"), TEXT("Crawl Key"), TEXT("Grassy Key")})
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

		GSea = {{TEXT("Calm"), .8, .5, TEXT("a long, lazy ground swell.")},
				{TEXT("Choppy"), 1.3, 1.2, TEXT("steady swells with afternoon chop on top.")},
				{TEXT("Haulover"), 1.6, 2.2, TEXT("steep, stacked inlet chop. Trim up and hang on.")},
				{TEXT("Big swell"), 2.6, 1.1, TEXT("big Atlantic rollers. Ride the backs and watch the troughs.")}};
		SetSeaState(1);
		for (FWakeRing& W : WakeP) W = FWakeRing();
		for (FTrailPt& P : TrailP) P = FTrailPt();

		// depth, inlet and color grids
		DGN = FMath::CeilToInt(WorldSize / DG) + 1;
		DepthG.SetNumUninitialized(DGN * DGN);
		InletG.SetNumUninitialized(DGN * DGN);
		ColorG.SetNumUninitialized(DGN * DGN);
		ParallelFor(DGN, [](int32 j)
		{
			for (int32 i = 0; i < DGN; i++)
			{
				const double X = WorldX0 + i * DG, Z = WorldZ0 + j * DG, D = DepthAt(X, Z);
				const int32 K = j * DGN + i;
				DepthG[K] = float(D);
				const double DZ = Z - ChainZ(X);
				InletG[K] = float((D > 3 && FMath::Abs(DZ) < 85) ? (1 - FMath::Abs(DZ) / 85)
					: (D > 3 && X > OldBrX0 && X < OldBrX1 && FMath::Abs(DZ - OldBrDZ) < 50) ? .5 * (1 - FMath::Abs(DZ - OldBrDZ) / 50) : 0);
				ColorG[K] = DepthRGB(D);
			}
		});

		// bridge pilings: US-1 over open water, and the old Seven Mile Bridge with its swing-span gap
		GPillars.Reset();
		GPGrid.Reset();
		for (double X = WBX0; X <= WBX1; X += 36)
		{
			const double Z = ChainZ(X);
			if (LandH(X, Z) > -.5) continue;
			const double Th = FMath::Atan2(0.000024 * X, 1.0), NX = -FMath::Sin(Th), NZ = FMath::Cos(Th);
			AddPillar(X + NX * 4.5, Z + NZ * 4.5, .8, false);
			AddPillar(X - NX * 4.5, Z - NZ * 4.5, .8, false);
		}
		for (double X = OldBrX0; X <= OldBrX1; X += 30)
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
		return I == GVaca && LX > RunLX0 - 30 && LX < RunLX1 + 30 && FMath::Abs(LZ - RunLZ) < RunW;
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
	bool InFill(double X, double Z)
	{
		if (FMath::Abs(Z - ChainZ(X)) <= 9)
			for (const FVector2D& R : GUS1Runs) if (X >= R.X && X <= R.Y) return true;
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
		const double CD = CreekDist(X, Z);
		if (CD < 10) H = FMath::Min(H, Lerp(-1.6, H, SS(-2, 10, CD)));
		const double MC = MarinaCutDepth(X, Z);
		if (MC > -CutFeather) H = FMath::Min(H, Lerp(H, -1.6, Clamp((MC + CutFeather) / CutFeather, 0, 1)));
		if (bFill) H = FMath::Max(H, 1.6);
		if (GMarinas.Num())
		{
			// under the town's pad the ground stays below the pad's 2.35 m top so it never pokes through the streets
			const FMarina& M = GMarinas[0];
			const double U = M.Dir * (X - M.SX), V = M.Dir * (M.SZ - Z);
			if (U >= -112 && U <= 112 && V >= 14 && V <= 180) H = FMath::Min(H, 2.2);
		}
		return H;
	}

	double OffshoreF(double X, double Z) { return Clamp((Z - ChainZ(X) - 1650) / 2200, 0, 1); }

	double DepthAt(double X, double Z)
	{
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
		const double FX = Clamp((X - WorldX0) / DG, 0, DGN - 1.001), FZ = Clamp((Z - WorldZ0) / DG, 0, DGN - 1.001);
		const int32 i = int32(FX), j = int32(FZ);
		const float U = float(FX - i), V = float(FZ - j);
		const int32 K = j * DGN + i;
		return (G[K] * (1 - U) + G[K + 1] * U) * (1 - V) + (G[K + DGN] * (1 - U) + G[K + DGN + 1] * U) * V;
	}

	double DepthFast(double X, double Z) { return GridAt(DepthG, X, Z); }
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
		const double D = DepthAt(X, Z), DZ = Z - ChainZ(X);
		if (CreekDist(X, Z) < 0) return TEXT("Creek");
		if (NearBridge(X, Z) && D >= 2 && ShoreInfo(X, Z).D > 6) return TEXT("Bridge");
		if (D < 2.6) return TEXT("Flats");
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

	bool InMiami(double X, double Z) { return X > -500 && X < 4200 && Z < -1450; }
	bool InCity(double X, double Z) { return X > 250 && X < 3900 && Z < -2560 && Z > -3600; }

	int32 SurfaceKind(double X, double Z)
	{
		if (InCity(X, Z)) return 3;
		const FShore S = ShoreInfo(X, Z);
		double LX, LZ;
		IslandLocal(*S.Isl, X, Z, LX, LZ);
		if (S.Isl == GVaca && LX > RunLX0 - 30 && LX < RunLX1 + 30 && FMath::Abs(LZ - RunLZ) < RunW) return 2;
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
		if (GSea.IsValidIndex(Index)) { SW = GSea[Index].SW; CH = GSea[Index].CH; }
	}
	int32 SeaIndex() { return GSeaIdx; }
	double SeaSW() { return SW; }
	double SeaCH() { return CH; }

	// small inshore, big rollers in deep blue water
	double AmpFor(double D) { return .1 + .9 * Clamp((D - 1) / 40, 0, 1) + 1 * Clamp((D - 45) / 250, 0, 1); }
	double AmpAt(double X, double Z) { return AmpFor(DepthFast(X, Z)) * (1 + .9 * GridAt(InletG, X, Z)); }

	double WaveH(double X, double Z, double T, double Amp, bool bWithWake)
	{
		double H = 0;
		for (int32 i = 0; i < 8; i++)
		{
			const double* W = WAVES[i];
			const double P = W[0] * X + W[1] * Z + W[2] * T + W[5];
			const double A = W[3] * (W[6] > 0 ? SW : FMath::Min(CH, 2.2));
			H += A * (FMath::Sin(P) - W[4] * FMath::Cos(2 * P));
		}
		return Amp * H + (bWithWake ? WakeH(X, Z, T) : 0);
	}

	FVector2D WaveSlope(double X, double Z, double T, double Amp)
	{
		double HX = 0, HZ = 0;
		for (int32 i = 0; i < 8; i++)
		{
			const double* W = WAVES[i];
			const double P = W[0] * X + W[1] * Z + W[2] * T + W[5];
			const double A = W[3] * (W[6] > 0 ? SW : FMath::Min(CH, 2.2));
			const double D = FMath::Cos(P) + 2 * W[4] * FMath::Sin(2 * P);
			HX += A * W[0] * D;
			HZ += A * W[1] * D;
		}
		return FVector2D(Amp * HX, Amp * HZ);
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
		auto Spread = [](const FTrailPt& P, double Age) { return P.W * (1 + Age * .05) + Age * .12; };
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
			L.F0 = A.S * FMath::Exp(-AgeA / 5.5) * FadeEnd; L.F1 = B.S * FMath::Exp(-AgeB / 5.5);
			L.A0 = A.S * FMath::Exp(-AgeA / 13) * FadeEnd; L.A1 = B.S * FMath::Exp(-AgeB / 13);
			L.MinX = FMath::Min(A.X, B.X) - WM * 1.5; L.MaxX = FMath::Max(A.X, B.X) + WM * 1.5;
			L.MinZ = FMath::Min(A.Z, B.Z) - WM * 1.5; L.MaxZ = FMath::Max(A.Z, B.Z) + WM * 1.5;
			if (L.MaxX < CX - Radius || L.MinX > CX + Radius || L.MaxZ < CZ - Radius || L.MinZ > CZ + Radius) continue;
			Out.Add(L);
		}
	}

	FVector2D TrailFrom(const TArray<FLiveTrail>& Live, double X, double Z)
	{
		double Foam = 0, Aer = 0;
		for (const FLiveTrail& L : Live)
		{
			if (X < L.MinX || X > L.MaxX || Z < L.MinZ || Z > L.MaxZ) continue;
			const double SX = L.BX - L.AX, SZ = L.BZ - L.AZ, SL = SX * SX + SZ * SZ;
			const double U = SL > 1e-6 ? FMath::Clamp(((X - L.AX) * SX + (Z - L.AZ) * SZ) / SL, 0.0, 1.0) : 0;
			const double D = FMath::Sqrt(FMath::Square(X - (L.AX + SX * U)) + FMath::Square(Z - (L.AZ + SZ * U)));
			const double W = FMath::Lerp(L.W0, L.W1, U), Q = D / FMath::Max(.1, W);
			// solid in the middle, ragged at the edges (the material breaks the edges up further)
			Foam = FMath::Max(Foam, FMath::Lerp(L.F0, L.F1, U) * (1 - WSStep(.2, 1.0, Q)));
			Aer = FMath::Max(Aer, FMath::Lerp(L.A0, L.A1, U) * (1 - WSStep(.5, 1.5, Q)));
		}
		return FVector2D(Foam, Aer);
	}
}
