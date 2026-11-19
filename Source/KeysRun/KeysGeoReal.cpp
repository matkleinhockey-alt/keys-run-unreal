#include "KeysGeoReal.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

namespace KeysGeoReal
{
	static bool GLoaded = false;
	static double GX0 = 0, GZ0 = 0, GCell = 10, GScale = 3, GTheta = 0, GLat0 = 0, GLon0 = 0, GAncX = 0, GAncZ = 0, GChainX0 = 0, GChainStep = 50;
	static int32 GNX = 0, GNZ = 0;
	static TArray<int16> GHeight, GShore;
	static TArray<uint8> GIsland;
	static TArray<float> GChain;
	static TArray<FIsl> GIslands;
	static TArray<FNavMark> GMarks;

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

	bool Load(const TCHAR* Phase)
	{
		if (GLoaded) return true;
		const FString Path = FPaths::ProjectContentDir() / TEXT("KeysRun/Geo") / (FString(Phase) + TEXT(".kgeo"));
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
		GX0 = R.Get<double>(); GZ0 = R.Get<double>(); GCell = R.Get<double>();
		GNX = R.Get<int32>(); GNZ = R.Get<int32>();
		GScale = R.Get<double>(); GTheta = R.Get<double>(); GLat0 = R.Get<double>(); GLon0 = R.Get<double>(); GAncX = R.Get<double>(); GAncZ = R.Get<double>();
		const int32 NChain = R.Get<int32>();
		GChainX0 = R.Get<double>(); GChainStep = R.Get<double>();
		if (!R.bOk || GNX < 2 || GNZ < 2 || GNX > 20000 || GNZ > 20000 || NChain < 2) return false;
		const int64 N = int64(GNX) * GNZ;
		R.GetArray(GHeight, N);
		R.GetArray(GShore, N);
		R.GetArray(GChain, NChain);
		R.GetArray(GIsland, N);
		const int32 NIsl = R.Get<int32>();
		GIslands.Reset();
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
			GIslands.Add(MoveTemp(I));
		}
		if (!R.bOk || GIslands.Num() == 0)
		{
			UE_LOG(LogTemp, Warning, TEXT("KeysGeoReal: %s is cut short"), *Path);
			return false;
		}
		// the marks: "x z kind number lit name" a line (Tools/geo/bake_marks.py)
		GMarks.Reset();
		TArray<FString> Lines;
		FFileHelper::LoadFileToStringArray(Lines, *(FPaths::ProjectContentDir() / TEXT("KeysRun/Geo") / (FString(Phase) + TEXT("_marks.txt"))));
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
		GLoaded = true;
		UE_LOG(LogTemp, Log, TEXT("KeysGeoReal: %s, %d x %d cells of %.0f m from (%.0f, %.0f), %d islands, %d marks, 1:%.1f"), Phase, GNX, GNZ, GCell, GX0, GZ0, GIslands.Num(), GMarks.Num(), GScale);
		return true;
	}

	bool Loaded() { return GLoaded; }
	const TArray<FNavMark>& Marks() { return GMarks; }
	double X0() { return GX0; }
	double Z0() { return GZ0; }
	double SizeX() { return (GNX - 1) * GCell; }
	double SizeZ() { return (GNZ - 1) * GCell; }
	const TArray<FIsl>& Islands() { return GIslands; }
	double Scale() { return GScale; }
	double Theta() { return GTheta; }

	static FORCEINLINE void Cell(double X, double Z, int32& I, int32& J, double& U, double& V)
	{
		const double FX = FMath::Clamp((X - GX0) / GCell, 0.0, GNX - 1.001), FZ = FMath::Clamp((Z - GZ0) / GCell, 0.0, GNZ - 1.001);
		I = int32(FX); J = int32(FZ);
		U = FX - I; V = FZ - J;
	}

	static FORCEINLINE double Bilinear(const TArray<int16>& G, double X, double Z)
	{
		int32 I, J;
		double U, V;
		Cell(X, Z, I, J, U, V);
		const int16* P = G.GetData() + int64(J) * GNX + I;
		return (P[0] * (1 - U) + P[1] * U) * (1 - V) + (P[GNX] * (1 - U) + P[GNX + 1] * U) * V;
	}

	double Height(double X, double Z) { return Bilinear(GHeight, X, Z) * .01; }
	double Shore(double X, double Z) { return Bilinear(GShore, X, Z) * .1; }

	int32 IslandAt(double X, double Z)
	{
		const int32 I = FMath::Clamp(FMath::RoundToInt32((X - GX0) / GCell), 0, GNX - 1), J = FMath::Clamp(FMath::RoundToInt32((Z - GZ0) / GCell), 0, GNZ - 1);
		return FMath::Min<int32>(GIsland[int64(J) * GNX + I], GIslands.Num() - 1);
	}

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
