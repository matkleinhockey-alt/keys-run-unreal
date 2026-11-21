#include "KeysGeoReal.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

namespace KeysGeoReal
{
	// One grid per baked file. The files tile the map: p1 (the Middle Keys), p2 (the Upper Keys, east of p1) and p2_bay
	// (the strip of Florida Bay north of p1 that squares the map off). Where two meet they hold the same values along
	// the shared edge (bake.py joins them), so a lookup simply uses the grid the point is in.
	struct FGrid
	{
		FString Phase;
		double X0 = 0, Z0 = 0, X1 = 0, Z1 = 0;
		int32 NX = 0, NZ = 0;
		int32 IOff = 0, JOff = 0;      // this grid's first cell on the whole map's cell grid
		int32 IslBase = 0, NIsl = 0;   // its islands in GIslands
		TArray<int16> Height, Shore;
		TArray<uint8> Island, Mangrove;
		TArray<float> Chain;
		double ChainX0 = 0, ChainStep = 50;
	};
	static bool GLoaded = false;
	static TArray<FGrid> GGrids;
	// the whole map: the box round every grid loaded
	static double GX0 = 0, GZ0 = 0, GCell = 10, GScale = 3, GTheta = 0, GLat0 = 0, GLon0 = 0, GAncX = 0, GAncZ = 0, GChainX0 = 0, GChainStep = 50;
	static int32 GNX = 0, GNZ = 0;
	static TArray<float> GChain;
	static TArray<FIsl> GIslands;
	static TArray<FNavMark> GMarks;
	static TArray<FPlatLot> GPlat;
	static int64 GBytes = 0;

	namespace
	{
		struct FReader
		{
			const TArray<uint8>& B;
			int64 P = 0;
			bool bOk = true;
			template <typename T> T Get()
			{
				T V{};
				if (P + int64(sizeof(T)) > B.Num()) { bOk = false; return V; }
				FMemory::Memcpy(&V, B.GetData() + P, sizeof(T));
				P += sizeof(T);
				return V;
			}
			template <typename T> void GetArray(TArray<T>& Out, int64 N)
			{
				if (N < 0 || P + N * int64(sizeof(T)) > B.Num()) { bOk = false; return; }
				Out.SetNumUninitialized(N);
				FMemory::Memcpy(Out.GetData(), B.GetData() + P, N * sizeof(T));
				P += N * sizeof(T);
			}
		};
	}

	// the map's box, cell offsets and US-1's line, again from every grid loaded
	static void Rebuild()
	{
		double MinX = 1e18, MinZ = 1e18, MaxX = -1e18, MaxZ = -1e18;
		for (const FGrid& G : GGrids)
		{
			MinX = FMath::Min(MinX, G.X0); MinZ = FMath::Min(MinZ, G.Z0);
			MaxX = FMath::Max(MaxX, G.X1); MaxZ = FMath::Max(MaxZ, G.Z1);
		}
		GX0 = MinX; GZ0 = MinZ;
		GNX = FMath::RoundToInt32((MaxX - MinX) / GCell) + 1;
		GNZ = FMath::RoundToInt32((MaxZ - MinZ) / GCell) + 1;
		for (FGrid& G : GGrids)
		{
			G.IOff = FMath::RoundToInt32((G.X0 - GX0) / GCell);
			G.JOff = FMath::RoundToInt32((G.Z0 - GZ0) / GCell);
		}
		// US-1's line runs the length of the map: each stretch of x from the first grid loaded that covers it
		GChainStep = GGrids[0].ChainStep;
		GChainX0 = GX0;
		const int32 NC = FMath::CeilToInt32((MaxX - MinX) / GChainStep) + 1;
		GChain.SetNumUninitialized(NC);
		for (int32 i = 0; i < NC; i++)
		{
			const double X = GChainX0 + i * GChainStep;
			const FGrid* Src = nullptr;
			for (const FGrid& G : GGrids) if (X >= G.X0 - 1 && X <= G.X1 + 1) { Src = &G; break; }
			if (!Src) Src = &GGrids[0];
			const double F = FMath::Clamp((X - Src->ChainX0) / Src->ChainStep, 0.0, Src->Chain.Num() - 1.001);
			const int32 K = int32(F);
			GChain[i] = float(Src->Chain[K] + (Src->Chain[K + 1] - Src->Chain[K]) * (F - K));
		}
	}

	// one baked phase: <Phase>.kgeo and, beside it, its marks, estate lots and mangrove cover
	static bool LoadPhase(const TCHAR* Phase)
	{
		const FString Dir = FPaths::ProjectContentDir() / TEXT("KeysRun/Geo");
		const FString Path = Dir / (FString(Phase) + TEXT(".kgeo"));
		TArray<uint8> Bytes;
		if (!FFileHelper::LoadFileToArray(Bytes, *Path))
		{
			UE_LOG(LogTemp, Warning, TEXT("KeysGeoReal: %s not found"), *Path);
			return false;
		}
		FReader R{Bytes};
		const uint32 Magic = R.Get<uint32>();
		const int32 Version = R.Get<int32>();
		if (Magic != 0x4F45474B || Version != 2)   // 'KGEO'
		{
			UE_LOG(LogTemp, Warning, TEXT("KeysGeoReal: %s is not a version 2 KGEO file"), *Path);
			return false;
		}
		FGrid G;
		G.Phase = Phase;
		G.X0 = R.Get<double>(); G.Z0 = R.Get<double>();
		const double CellM = R.Get<double>();
		G.NX = R.Get<int32>(); G.NZ = R.Get<int32>();
		const double FScale = R.Get<double>(), FTheta = R.Get<double>(), FLat0 = R.Get<double>(), FLon0 = R.Get<double>(), FAncX = R.Get<double>(), FAncZ = R.Get<double>();
		const int32 NChain = R.Get<int32>();
		G.ChainX0 = R.Get<double>(); G.ChainStep = R.Get<double>();
		if (!R.bOk || G.NX < 2 || G.NZ < 2 || G.NX > 20000 || G.NZ > 20000 || NChain < 2) return false;
		if (GGrids.Num() && (CellM != GCell || FScale != GScale || FTheta != GTheta || FLat0 != GLat0 || FLon0 != GLon0 || FAncX != GAncX || FAncZ != GAncZ))
		{
			UE_LOG(LogTemp, Warning, TEXT("KeysGeoReal: %s is baked on another frame than %s"), *Path, *GGrids[0].Phase);
			return false;
		}
		G.X1 = G.X0 + (G.NX - 1) * CellM; G.Z1 = G.Z0 + (G.NZ - 1) * CellM;
		const int64 N = int64(G.NX) * G.NZ;
		R.GetArray(G.Height, N);
		R.GetArray(G.Shore, N);
		R.GetArray(G.Chain, NChain);
		R.GetArray(G.Island, N);
		const int32 NIsl = R.Get<int32>();
		TArray<FIsl> Isl;
		for (int32 i = 0; i < NIsl && R.bOk; i++)
		{
			FIsl I;
			const int32 NameLen = R.Get<int32>();
			TArray<uint8> NameBytes;
			R.GetArray(NameBytes, NameLen);
			if (!R.bOk) break;
			NameBytes.Add(0);
			I.Name = FString(UTF8_TO_TCHAR(reinterpret_cast<const char*>(NameBytes.GetData())));
			I.X = R.Get<double>(); I.Z = R.Get<double>(); I.Area = R.Get<double>();
			I.MinX = R.Get<double>(); I.MinZ = R.Get<double>(); I.MaxX = R.Get<double>(); I.MaxZ = R.Get<double>();
			const int32 NR = R.Get<int32>();
			TArray<float> Ring;
			R.GetArray(Ring, NR);
			if (!R.bOk) break;
			int32 M = NR / 2;
			if (M > 1 && Ring[0] == Ring[M * 2 - 2] && Ring[1] == Ring[M * 2 - 1]) M--;   // the outline comes closed
			I.Ring.Reserve(M * 2);
			for (int32 k = 0; k < M * 2; k++) I.Ring.Add(Ring[k]);
			Isl.Add(MoveTemp(I));
		}
		if (!R.bOk || Isl.Num() == 0)
		{
			UE_LOG(LogTemp, Warning, TEXT("KeysGeoReal: %s is cut short"), *Path);
			return false;
		}
		if (!GGrids.Num()) { GCell = CellM; GScale = FScale; GTheta = FTheta; GLat0 = FLat0; GLon0 = FLon0; GAncX = FAncX; GAncZ = FAncZ; }
		G.IslBase = GIslands.Num(); G.NIsl = Isl.Num();
		GIslands.Append(MoveTemp(Isl));
		// the marks: "x z kind number lit name" a line (Tools/geo/bake_marks.py)
		const int32 Marks0 = GMarks.Num();
		TArray<FString> Lines;
		FFileHelper::LoadFileToStringArray(Lines, *(Dir / (FString(Phase) + TEXT("_marks.txt"))));
		for (const FString& L : Lines)
		{
			if (L.StartsWith(TEXT("#"))) continue;
			TArray<FString> P;
			L.ParseIntoArray(P, TEXT(" "));
			if (P.Num() < 5 || (P[2] != TEXT("R") && P[2] != TEXT("G"))) continue;
			FNavMark M;
			M.X = FCString::Atod(*P[0]); M.Z = FCString::Atod(*P[1]);
			M.bRed = P[2] == TEXT("R"); M.Num = FCString::Atoi(*P[3]); M.bLit = P[4] == TEXT("1");
			for (int32 k = 5; k < P.Num(); k++) M.Name += (k > 5 ? TEXT(" ") : TEXT("")) + P[k];
			GMarks.Add(MoveTemp(M));
		}
		// the estates plat: "id|name|island|tier|side|cx|cz|area|frontage|x0 z0 x1 z1 ..." a line
		Lines.Reset();
		FFileHelper::LoadFileToStringArray(Lines, *(Dir / (FString(Phase) + TEXT("_plat.txt"))));
		for (const FString& L : Lines)
		{
			if (L.StartsWith(TEXT("#"))) continue;
			TArray<FString> P;
			L.ParseIntoArray(P, TEXT("|"));
			if (P.Num() < 10) continue;
			FPlatLot Lot;
			Lot.Id = P[0]; Lot.Name = P[1]; Lot.Island = P[2]; Lot.Tier = FCString::Atoi(*P[3]); Lot.Side = P[4];
			Lot.CX = FCString::Atod(*P[5]); Lot.CZ = FCString::Atod(*P[6]); Lot.Area = FCString::Atod(*P[7]); Lot.Frontage = FCString::Atod(*P[8]);
			TArray<FString> Q;
			P[9].ParseIntoArray(Q, TEXT(" "));
			for (int32 k = 0; k + 1 < Q.Num(); k += 2) Lot.Poly.Add(FVector2D(FCString::Atod(*Q[k]), FCString::Atod(*Q[k + 1])));
			if (Lot.Poly.Num() >= 3) GPlat.Add(MoveTemp(Lot));
		}
		// mangrove cover: 'KMGV', nx, nz, bytes
		TArray<uint8> MB;
		if (FFileHelper::LoadFileToArray(MB, *(Dir / (FString(Phase) + TEXT("_mangrove.bin"))), FILEREAD_Silent) && MB.Num() == 12 + N)
			G.Mangrove.Append(MB.GetData() + 12, N);
		const int64 Held = G.Height.GetAllocatedSize() + G.Shore.GetAllocatedSize() + G.Island.GetAllocatedSize() + G.Mangrove.GetAllocatedSize() + G.Chain.GetAllocatedSize();
		GBytes += Held;
		UE_LOG(LogTemp, Log, TEXT("KeysGeoReal: %s, %d x %d cells of %.0f m from (%.0f, %.0f), %d islands, %d marks, 1:%.1f, %.1f MB"), Phase, G.NX, G.NZ, CellM, G.X0, G.Z0, G.NIsl, GMarks.Num() - Marks0, FScale, Held / 1048576.0);
		GGrids.Add(MoveTemp(G));
		Rebuild();
		return true;
	}

	bool Load(const TCHAR* Phase)
	{
		if (GLoaded) return true;
		GLoaded = LoadPhase(Phase);
		return GLoaded;
	}

	bool Add(const TCHAR* Phase)
	{
		if (!GLoaded) return false;
		for (const FGrid& G : GGrids) if (G.Phase == Phase) return true;
		return LoadPhase(Phase);
	}

	bool LoadAll()
	{
		// every phase of the real map, the first one first. A later phase that is missing is left out: the map is then
		// the phases that did load.
		if (GLoaded) return true;
		const double T0 = FPlatformTime::Seconds();
		if (!Load(TEXT("p1"))) return false;
		const double T1 = FPlatformTime::Seconds();
		const int64 B1 = GBytes;
		for (const TCHAR* Phase : {TEXT("p2"), TEXT("p2_bay")}) Add(Phase);
		UE_LOG(LogTemp, Log, TEXT("KeysGeoReal: the map is %d grids, %d x %d cells from (%.0f, %.0f) to (%.0f, %.0f), %d islands, %d marks; p1 %.1f MB in %.0f ms, the rest %.1f MB in %.0f ms"),
			GGrids.Num(), GNX, GNZ, GX0, GZ0, GX0 + (GNX - 1) * GCell, GZ0 + (GNZ - 1) * GCell, GIslands.Num(), GMarks.Num(),
			B1 / 1048576.0, (T1 - T0) * 1000, (GBytes - B1) / 1048576.0, (FPlatformTime::Seconds() - T1) * 1000);
		return true;
	}

	bool Loaded() { return GLoaded; }
	int32 Phases() { return GGrids.Num(); }
	const TArray<FNavMark>& Marks() { return GMarks; }
	const TArray<FPlatLot>& Plat() { return GPlat; }
	int32 NX() { return GNX; }
	int32 NZ() { return GNZ; }
	double Cell() { return GCell; }
	int32 CellI0() { return GGrids.Num() ? FMath::RoundToInt32((GX0 - GGrids[0].X0) / GCell) : 0; }
	int32 CellJ0() { return GGrids.Num() ? FMath::RoundToInt32((GZ0 - GGrids[0].Z0) / GCell) : 0; }
	uint8 MangroveCell(int32 I, int32 J)
	{
		for (const FGrid& G : GGrids)
		{
			const int32 A = I - G.IOff, B = J - G.JOff;
			if (A >= 0 && B >= 0 && A < G.NX && B < G.NZ) return G.Mangrove.Num() ? G.Mangrove[int64(B) * G.NX + A] : 0;
		}
		return 0;
	}
	uint8 MangroveAt(double X, double Z) { return MangroveCell(FMath::RoundToInt32((X - GX0) / GCell), FMath::RoundToInt32((Z - GZ0) / GCell)); }
	double X0() { return GX0; }
	double Z0() { return GZ0; }
	double SizeX() { return (GNX - 1) * GCell; }
	double SizeZ() { return (GNZ - 1) * GCell; }
	const TArray<FIsl>& Islands() { return GIslands; }
	double Scale() { return GScale; }
	double Theta() { return GTheta; }

	// the grid a point is in; off the map (or in a corner no file covers), the nearest one, read at its edge
	static FORCEINLINE const FGrid& GridAt(double X, double Z)
	{
		const FGrid* Best = GGrids.GetData();
		const int32 N = GGrids.Num();
		if (N == 1) return *Best;
		double BestD = 1e30;
		for (int32 i = 0; i < N; i++)
		{
			const FGrid& G = GGrids[i];
			const double DX = X < G.X0 ? G.X0 - X : X > G.X1 ? X - G.X1 : 0, DZ = Z < G.Z0 ? G.Z0 - Z : Z > G.Z1 ? Z - G.Z1 : 0;
			if (DX == 0 && DZ == 0) return G;
			const double D = DX * DX + DZ * DZ;
			if (D < BestD) { BestD = D; Best = &G; }
		}
		return *Best;
	}

	static FORCEINLINE void CellIn(const FGrid& G, double X, double Z, int32& I, int32& J, double& U, double& V)
	{
		const double FX = FMath::Clamp((X - G.X0) / GCell, 0.0, G.NX - 1.001), FZ = FMath::Clamp((Z - G.Z0) / GCell, 0.0, G.NZ - 1.001);
		I = int32(FX); J = int32(FZ);
		U = FX - I; V = FZ - J;
	}

	static FORCEINLINE double Bilinear(const FGrid& G, const TArray<int16>& A, double X, double Z)
	{
		int32 I, J;
		double U, V;
		CellIn(G, X, Z, I, J, U, V);
		const int16* P = A.GetData() + int64(J) * G.NX + I;
		return (P[0] * (1 - U) + P[1] * U) * (1 - V) + (P[G.NX] * (1 - U) + P[G.NX + 1] * U) * V;
	}

	double Height(double X, double Z) { const FGrid& G = GridAt(X, Z); return Bilinear(G, G.Height, X, Z) * .01; }
	double Shore(double X, double Z) { const FGrid& G = GridAt(X, Z); return Bilinear(G, G.Shore, X, Z) * .1; }

	int32 IslandAt(double X, double Z)
	{
		const FGrid& G = GridAt(X, Z);
		const int32 I = FMath::Clamp(FMath::RoundToInt32((X - G.X0) / GCell), 0, G.NX - 1), J = FMath::Clamp(FMath::RoundToInt32((Z - G.Z0) / GCell), 0, G.NZ - 1);
		return G.IslBase + FMath::Min<int32>(G.Island[int64(J) * G.NX + I], G.NIsl - 1);
	}

	int32 PhaseAt(double X, double Z) { return int32(&GridAt(X, Z) - GGrids.GetData()); }

	double ChainZ(double X)
	{
		const double F = FMath::Clamp((X - GChainX0) / GChainStep, 0.0, GChain.Num() - 1.001);
		const int32 I = int32(F);
		return GChain[I] + (GChain[I + 1] - GChain[I]) * (F - I);
	}

	void FlattenChain(double XA, double XB, double Z)
	{
		for (int32 i = 0; i < GChain.Num(); i++)
		{
			const double X = GChainX0 + i * GChainStep;
			if (X >= XA - 1 && X <= XB + 1) GChain[i] = float(Z);
		}
	}

	double ChainSlope(double X)
	{
		const double F = FMath::Clamp((X - GChainX0) / GChainStep, 0.0, GChain.Num() - 1.001);
		const int32 I = int32(F);
		return (GChain[I + 1] - GChain[I]) / GChainStep;
	}

	void ToLonLat(double X, double Z, double& Lon, double& Lat)
	{
		const double A = (X - GAncX) * GScale, B = -(Z - GAncZ) * GScale, C = FMath::Cos(GTheta), S = FMath::Sin(GTheta);
		const double MLat = 111320.0, MLon = 111320.0 * FMath::Cos(FMath::DegreesToRadians(GLat0));
		Lon = GLon0 + (A * C - B * S) / MLon;
		Lat = GLat0 + (A * S + B * C) / MLat;
	}

	void FromLonLat(double Lon, double Lat, double& X, double& Z)
	{
		const double MLat = 111320.0, MLon = 111320.0 * FMath::Cos(FMath::DegreesToRadians(GLat0));
		const double E = (Lon - GLon0) * MLon, N = (Lat - GLat0) * MLat, C = FMath::Cos(GTheta), S = FMath::Sin(GTheta);
		X = GAncX + (E * C + N * S) / GScale;
		Z = GAncZ - (-E * S + N * C) / GScale;
	}
}
