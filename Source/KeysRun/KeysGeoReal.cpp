#include "KeysGeoReal.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"

namespace KeysGeoReal
{
	// One grid per baked file. The files tile the map: p1 (the Middle Keys), p2 (the Upper Keys, east of p1), p2_bay
	// (the strip of Florida Bay north of p1), p3 (the Lower Keys, west of p1, out past Key West), p4 (Key Largo, east
	// and north of p2) and p4_bay (the bay and Gulf north of all the rest, which squares the map off). Where two meet
	// they hold the same values along the shared edge (bake.py joins them), so a lookup simply uses the grid the point
	// is in. Any of them but p1 may be missing: the map is then the ones that are there.
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
		double RoadA = -1e18, RoadB = 1e18;   // US-1 runs on this grid's stretch of the chain line between these x (<phase>_sites.txt "us1")
	};
	static bool GLoaded = false;
	static TArray<FGrid> GGrids;
	// the whole map: the box round every grid loaded
	static double GX0 = 0, GZ0 = 0, GCell = 10, GScale = 3, GTheta = 0, GLat0 = 0, GLon0 = 0, GAncX = 0, GAncZ = 0, GChainX0 = 0, GChainStep = 50;
	static int32 GNX = 0, GNZ = 0;
	static TArray<float> GChain;
	static double GRoadX0 = 0, GRoadX1 = 0;
	static TArray<FSite> GSites;
	static TArray<FIsl> GIslands;
	static TArray<FNavMark> GMarks;
	static TArray<FCanal> GCanals, GWays;
	struct FCanalSeg { double AX, AZ, BX, BZ, HalfW; bool bFirst, bLast; };
	static TArray<FCanalSeg> GCanalSegs;
	static TMap<int64, TArray<int32>> GCanalHash;     // 64 m cells -> the segments within reach
	static TArray<FCanalSeg> GWaySegs;
	static TMap<int64, TArray<int32>> GWayHash;
	static const double CanalReach = 28;
	static int64 CanalKey(double X, double Z) { return (int64(FMath::FloorToInt(X / 64.0)) << 32) ^ int64(uint32(FMath::FloorToInt(Z / 64.0))); }
	static void HashLines(const TArray<FCanal>& Lines, TArray<FCanalSeg>& Segs, TMap<int64, TArray<int32>>& Hash)
	{
		Segs.Reset();
		Hash.Reset();
		for (const FCanal& C : Lines)
			for (int32 i = 0; i + 1 < C.Pts.Num(); i++)
			{
				const FCanalSeg S{C.Pts[i].X, C.Pts[i].Y, C.Pts[i + 1].X, C.Pts[i + 1].Y, C.Width / 2, i == 0, i + 2 == C.Pts.Num()};
				const int32 K = Segs.Add(S);
				const double Pad = S.HalfW + CanalReach;
				for (int32 CX = FMath::FloorToInt((FMath::Min(S.AX, S.BX) - Pad) / 64.0); CX <= FMath::FloorToInt((FMath::Max(S.AX, S.BX) + Pad) / 64.0); CX++)
					for (int32 CZ = FMath::FloorToInt((FMath::Min(S.AZ, S.BZ) - Pad) / 64.0); CZ <= FMath::FloorToInt((FMath::Max(S.AZ, S.BZ) + Pad) / 64.0); CZ++)
						Hash.FindOrAdd(CanalKey(CX * 64.0 + 1, CZ * 64.0 + 1)).Add(K);
			}
	}
	static void HashCanals() { HashLines(GCanals, GCanalSegs, GCanalHash); HashLines(GWays, GWaySegs, GWayHash); }
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
		// (sampled on the first phase's own points, so its line is the same to the last digit however far the map grows west)
		GChainStep = GGrids[0].ChainStep;
		GChainX0 = GGrids[0].ChainX0 - FMath::CeilToDouble((GGrids[0].ChainX0 - MinX) / GChainStep - 1e-9) * GChainStep;
		const int32 NC = FMath::CeilToInt32((MaxX - GChainX0) / GChainStep) + 1;
		GChain.SetNumUninitialized(NC);
		GRoadX0 = MinX; GRoadX1 = MaxX;
		for (int32 i = 0; i < NC; i++)
		{
			const double X = GChainX0 + i * GChainStep;
			const FGrid* Src = nullptr;
			for (const FGrid& G : GGrids) if (X >= G.X0 - 1 && X <= G.X1 + 1) { Src = &G; break; }
			if (!Src)
			{
				// past the ends: the grid that reaches nearest
				double BestD = 1e30;
				for (const FGrid& G : GGrids) { const double D = X < G.X0 ? G.X0 - X : X - G.X1; if (D < BestD) { BestD = D; Src = &G; } }
			}
			// the road itself may stop short of the map's ends (Key West; where US-1 leaves Key Largo for the mainland)
			if (i == 0) GRoadX0 = FMath::Max(MinX, Src->RoadA);
			if (i == NC - 1) GRoadX1 = FMath::Min(MaxX, Src->RoadB);
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
		// the canals: "width x z x z ..." a line
		Lines.Reset();
		FFileHelper::LoadFileToStringArray(Lines, *(Dir / (FString(Phase) + TEXT("_canals.txt"))));
		for (const FString& L : Lines)
		{
			if (L.StartsWith(TEXT("#"))) continue;
			TArray<FString> P;
			L.ParseIntoArray(P, TEXT(" "));
			const int32 W0 = P.Num() && P[0] == TEXT("w") ? 1 : 0;     // a way out, not a canal
			if (P.Num() < 5 + W0) continue;
			FCanal C;
			C.Width = FCString::Atod(*P[W0]);
			for (int32 k = W0 + 1; k + 1 < P.Num(); k += 2) C.Pts.Add(FVector2D(FCString::Atod(*P[k]), FCString::Atod(*P[k + 1])));
			(W0 ? GWays : GCanals).Add(MoveTemp(C));
		}
		HashCanals();
		// the sites: "kind|name|x|z|x2|z2|value" a line (Tools/geo/sites_p3.py); "us1|...|x0||x1" is the road's extent
		Lines.Reset();
		FFileHelper::LoadFileToStringArray(Lines, *(Dir / (FString(Phase) + TEXT("_sites.txt"))));
		for (const FString& L : Lines)
		{
			if (L.StartsWith(TEXT("#"))) continue;
			TArray<FString> P;
			L.ParseIntoArray(P, TEXT("|"), false);
			if (P.Num() < 7) continue;
			FSite S;
			S.Kind = P[0]; S.Name = P[1];
			S.X = FCString::Atod(*P[2]); S.Z = FCString::Atod(*P[3]); S.X2 = FCString::Atod(*P[4]); S.Z2 = FCString::Atod(*P[5]); S.Value = FCString::Atod(*P[6]);
			if (S.Kind == TEXT("us1")) { G.RoadA = S.X; G.RoadB = S.X2; }
			else GSites.Add(MoveTemp(S));
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

	const TArray<FCanal>& Canals() { return GCanals; }

	static double CanalNear(double X, double Z, bool& bCap, double& EX, double& EZ, double& HalfW)
	{
		bCap = false;
		if (!GCanalSegs.Num()) return 1e9;
		const TArray<int32>* Cell = GCanalHash.Find(CanalKey(X, Z));
		if (!Cell) return 1e9;
		double Best = 1e9;
		for (int32 K : *Cell)
		{
			const FCanalSeg& S = GCanalSegs[K];
			const double VX = S.BX - S.AX, VZ = S.BZ - S.AZ, L2 = VX * VX + VZ * VZ;
			const double T = L2 > 1e-9 ? ((X - S.AX) * VX + (Z - S.AZ) * VZ) / L2 : 0, TC = FMath::Clamp(T, 0.0, 1.0);
			const double PX = S.AX + VX * TC, PZ = S.AZ + VZ * TC;
			const double D = FMath::Sqrt(FMath::Square(X - PX) + FMath::Square(Z - PZ)) - S.HalfW;
			if (D < Best)
			{
				Best = D;
				bCap = (T < 0 && S.bFirst) || (T > 1 && S.bLast);
				EX = PX; EZ = PZ; HalfW = S.HalfW;
			}
		}
		return Best;
	}

	double CanalDist(double X, double Z, bool* bCap)
	{
		bool b;
		double EX, EZ, HW;
		const double D = CanalNear(X, Z, b, EX, EZ, HW);
		if (bCap) *bCap = b;
		return D;
	}

	double WayOutDist(double X, double Z)
	{
		const TArray<int32>* Cell = GWaySegs.Num() ? GWayHash.Find(CanalKey(X, Z)) : nullptr;
		if (!Cell) return 1e9;
		double Best = 1e9;
		for (int32 K : *Cell)
		{
			const FCanalSeg& S = GWaySegs[K];
			const double VX = S.BX - S.AX, VZ = S.BZ - S.AZ, L2 = VX * VX + VZ * VZ;
			const double T = L2 > 1e-9 ? FMath::Clamp(((X - S.AX) * VX + (Z - S.AZ) * VZ) / L2, 0.0, 1.0) : 0;
			Best = FMath::Min(Best, FMath::Sqrt(FMath::Square(X - S.AX - VX * T) + FMath::Square(Z - S.AZ - VZ * T)) - S.HalfW);
		}
		return Best;
	}

	bool CanalBank(double X, double Z, double& OutDist)
	{
		bool b;
		double EX = 0, EZ = 0, HW = 12;
		OutDist = CanalNear(X, Z, b, EX, EZ, HW);
		if (OutDist > 1e8) return false;
		if (!b) return true;
		// past the end of a line: a dead end if the grid has land a little way on, a mouth if it has water
		const double DX = X - EX, DZ = Z - EZ, L = FMath::Sqrt(DX * DX + DZ * DZ);
		if (L < .01) return false;
		return Shore(EX + DX / L * (HW + 10), EZ + DZ / L * (HW + 10)) < 0;
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

	// The ways dredged out of the canals: a pair of daybeacons at the outer end of each one that crosses a flat, green to
	// port and red to starboard coming in. A way gets its pair when it is 60 m long or more and ends 30 m or more off
	// the shore, in the open and not in another way (a junction), with no mark within 45 m already (a real channel's, or
	// the pair of a way that ends on the same spot). Each of the two is a channel of its own by name, so no channel leg
	// is dredged between them (KeysGeo BuildNavSegs).
	static void MarkWays()
	{
		const int32 Had = GMarks.Num();
		auto SegDist = [](const FVector2D& P, const FVector2D& A, const FVector2D& B)
		{
			const FVector2D V = B - A;
			const double L2 = V.SizeSquared(), T = L2 > 1e-9 ? FMath::Clamp(FVector2D::DotProduct(P - A, V) / L2, 0.0, 1.0) : 0.0;
			return (P - (A + V * T)).Size();
		};
		for (int32 w = 0; w < GWays.Num(); w++)
		{
			const TArray<FVector2D>& P = GWays[w].Pts;
			if (P.Num() < 2) continue;
			double Len = 0;
			for (int32 i = 0; i + 1 < P.Num(); i++) Len += (P[i + 1] - P[i]).Size();
			const FVector2D E = P.Last();
			if (Len < 60 || Shore(E.X, E.Y) < 30) continue;
			FVector2D D = FVector2D::ZeroVector;      // the way's last leg, seaward
			for (int32 i = P.Num() - 2; i >= 0 && D.IsNearlyZero(); i--) if ((E - P[i]).Size() > 4) D = (E - P[i]).GetSafeNormal();
			if (D.IsNearlyZero()) continue;
			bool bSkip = false;
			for (int32 o = 0; o < GWays.Num() && !bSkip; o++)
			{
				const TArray<FVector2D>& Q = GWays[o].Pts;
				if (o == w || Q.Num() < 2 || (Q.Last() - E).Size() < 20) continue;      // (one that ends here too shares the pair)
				for (int32 i = 0; i + 1 < Q.Num() && !bSkip; i++) bSkip = SegDist(E, Q[i], Q[i + 1]) < GWays[o].Width / 2;
			}
			for (int32 m = 0; m < GMarks.Num() && !bSkip; m++) bSkip = FMath::Square(GMarks[m].X - E.X) + FMath::Square(GMarks[m].Z - E.Y) < 45.0 * 45.0;
			const FVector2D Stbd(D.Y, -D.X);          // to starboard of a boat coming in
			const FVector2D R = E + Stbd * (GWays[w].Width / 2 + 2), G = E - Stbd * (GWays[w].Width / 2 + 2);
			if (bSkip || Shore(R.X, R.Y) < 10 || Shore(G.X, G.Y) < 10) continue;
			FNavMark MG, MR;
			MG.X = G.X; MG.Z = G.Y; MG.bRed = false; MG.Num = 1; MG.Name = FString::Printf(TEXT("Canal Way %dG Daybeacon 1"), w);
			MR.X = R.X; MR.Z = R.Y; MR.bRed = true; MR.Num = 2; MR.Name = FString::Printf(TEXT("Canal Way %dR Daybeacon 2"), w);
			GMarks.Add(MoveTemp(MG));
			GMarks.Add(MoveTemp(MR));
			UE_LOG(LogTemp, Log, TEXT("KRWayMark way %d (%.0f m): green %.1f, %.1f red %.1f, %.1f"), w, Len, G.X, G.Y, R.X, R.Y);
		}
		UE_LOG(LogTemp, Log, TEXT("KeysGeoReal: %d ways out of the canals, %d of them marked with a pair of daybeacons at the outer end"), GWays.Num(), (GMarks.Num() - Had) / 2);
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
		const double T2 = FPlatformTime::Seconds();
		const int64 B2 = GBytes;
		// (-KRNoPhase3 leaves them out: the map as it was before the Lower Keys and Key Largo, for comparing)
		if (!FParse::Param(FCommandLine::Get(), TEXT("KRNoPhase3")))
			for (const TCHAR* Phase : {TEXT("p3"), TEXT("p4"), TEXT("p4_bay")}) Add(Phase);
		int64 Cells = 0;
		for (const FGrid& G : GGrids) Cells += int64(G.NX) * G.NZ;
		UE_LOG(LogTemp, Log, TEXT("KeysGeoReal: the map is %d grids, %d x %d cells from (%.0f, %.0f) to (%.0f, %.0f), %d islands, %d marks; p1 %.1f MB in %.0f ms, Phase 2 %.1f MB in %.0f ms, Phase 3 %.1f MB in %.0f ms"),
			GGrids.Num(), GNX, GNZ, GX0, GZ0, GX0 + (GNX - 1) * GCell, GZ0 + (GNZ - 1) * GCell, GIslands.Num(), GMarks.Num(),
			B1 / 1048576.0, (T1 - T0) * 1000, (B2 - B1) / 1048576.0, (T2 - T1) * 1000, (GBytes - B2) / 1048576.0, (FPlatformTime::Seconds() - T2) * 1000);
		UE_LOG(LogTemp, Log, TEXT("KeysGeoReal: US-1 runs from x %.0f to %.0f; %d sites; the files cover %.1f%% of the map's box"), GRoadX0, GRoadX1, GSites.Num(), 100.0 * Cells / FMath::Max<int64>(1, int64(GNX) * GNZ));
		MarkWays();
		return true;
	}

	bool Loaded() { return GLoaded; }
	int32 Phases() { return GGrids.Num(); }
	const TArray<FNavMark>& Marks() { return GMarks; }
	const TArray<FPlatLot>& Plat() { return GPlat; }
	const TArray<FSite>& Sites() { return GSites; }
	double RoadX0() { return GRoadX0; }
	double RoadX1() { return GRoadX1; }
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
		// (lookups come in runs over the same stretch of water: the grid the last one on this thread fell in is tried first)
		static thread_local int32 Last = 0;
		if (Last < N)
		{
			const FGrid& G = Best[Last];
			if (X >= G.X0 && X <= G.X1 && Z >= G.Z0 && Z <= G.Z1) return G;
		}
		double BestD = 1e30;
		for (int32 i = 0; i < N; i++)
		{
			const FGrid& G = GGrids[i];
			const double DX = X < G.X0 ? G.X0 - X : X > G.X1 ? X - G.X1 : 0, DZ = Z < G.Z0 ? G.Z0 - Z : Z > G.Z1 ? Z - G.Z1 : 0;
			if (DX == 0 && DZ == 0) { Last = i; return G; }
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

	bool ShoreUnder(double X0, double Z0, double X1, double Z1, double D)
	{
		const int16 Lim = int16(FMath::Clamp(D * 10, -32000.0, 32000.0));
		for (const FGrid& G : GGrids)
		{
			const int32 I0 = FMath::Max(0, FMath::FloorToInt32((X0 - G.X0) / GCell)), I1 = FMath::Min(G.NX - 1, FMath::CeilToInt32((X1 - G.X0) / GCell));
			const int32 J0 = FMath::Max(0, FMath::FloorToInt32((Z0 - G.Z0) / GCell)), J1 = FMath::Min(G.NZ - 1, FMath::CeilToInt32((Z1 - G.Z0) / GCell));
			for (int32 J = J0; J <= J1; J++)
			{
				const int16* P = G.Shore.GetData() + int64(J) * G.NX;
				for (int32 I = I0; I <= I1; I++) if (P[I] < Lim) return true;
			}
		}
		return false;
	}

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
