// The mangrove ecosystem: the rules that say what lives where, and the actor that runs it round the boat.
//
// Everything is placed from the map by rule. KeysMangrove.cpp hands over every tree that stands with its feet in the
// water (SitesOfTile: the same cells and dice the trees themselves are grown from), and PlanTile() rolls each tree's
// own dice (SHash: a fixed seed, so the same animals are in the same places every run) for what it holds: a school of
// small fish under its roots, a wader off them, a cormorant on a low limb, crabs, a snook lying in the shade. Rookery
// keys are small wild mangrove islands picked from the map's own island list, a few in each part of the chain.
// Nothing here is a list of coordinates; it works wherever the map has mangrove.
//
// The plan for a 600 m tile is made off the game thread when the boat comes within 900 m and let go past 1.3 km.
// Animals move only within 600 m of the camera. The birds are one mesh (KeysMangroveEcoBirds.cpp); everything in the
// water and the crabs are instances (KeysMangroveEcoWater.cpp).
//
// It is one system (the lines it logs as "KREco link ..."):
//   - predators lying off the roots rush the small fish when they are hungry and the bait is in reach: it showers
//   - terns and pelicans come to a shower and dive on it
//   - waders walk and fly to where the small fish are thick and the water is shallow enough to stand in, and strike
//   - on the ebb the small fish leave the roots for the deeper edge and the creek mouths, the predators and the
//     birds go with them; on the flood they all come back in
//   - at night the day birds go to roost in the crowns and the night herons come out to hunt
#include "KeysMangroveEco.h"
#include "KeysMangroveEcoInt.h"
#include "KeysGeo.h"
#include "KeysGeoReal.h"
#include "KeysSea.h"
#include "KeysOcean.h"
#include "KeysFeed.h"
#include "KeysLife.h"
#include "KeysBoatPawn.h"
#include "KeysDiver.h"
#include "KeysWorld.h"
#include "KeysMangroveLife.h"
#include "KeysMesh.h"
#include "ProceduralMeshComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Camera/PlayerCameraManager.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/PlayerController.h"
#include "Async/Async.h"
#include "HAL/IConsoleManager.h"
#include "Misc/CommandLine.h"
#include "Misc/Paths.h"
#include "UnrealClient.h"

namespace KeysMangroveEco
{
	static AKeysMangroveEco* GEco = nullptr;

	bool Enabled()
	{
		static const bool bOff = FParse::Param(FCommandLine::Get(), TEXT("KRNoMangroveEco"));
		return !bOff && KeysGeo::RealMap();
	}

	const TCHAR* KindName(int32 Kind)
	{
		static const TCHAR* Names[NumKinds] = {TEXT("great egret"), TEXT("great blue heron"), TEXT("little blue heron"), TEXT("white ibis"), TEXT("roseate spoonbill"),
			TEXT("cormorant"), TEXT("anhinga"), TEXT("belted kingfisher"), TEXT("green heron"), TEXT("night heron"), TEXT("tern"), TEXT("osprey"), TEXT("brown pelican"),
			TEXT("silverside"), TEXT("pilchard"), TEXT("juvenile snapper"), TEXT("juvenile grunt"), TEXT("needlefish"), TEXT("mullet"), TEXT("barracuda"), TEXT("lemon shark"),
			TEXT("nurse shark"), TEXT("stingray"), TEXT("upside-down jellyfish"), TEXT("snook"), TEXT("jack"), TEXT("tarpon"), TEXT("tree crab"), TEXT("fiddler crab"),
			TEXT("horseshoe crab"), TEXT("manatee haunt"), TEXT("pelican roost")};
		return Kind >= 0 && Kind < NumKinds ? Names[Kind] : TEXT("?");
	}

	// The parts of the chain: the map is baked in phases (the Lower Keys, the Middle Keys, the Upper Keys, Key Largo);
	// a point belongs to the part whose phase US-1's line is in at its x.
	int32 RegionAt(double X)
	{
		struct FTable { TArray<double> Edge; int32 First = 1; };
		static const FTable Tb = []
		{
			FTable T;
			int32 Last = -1;
			for (double Xs = KeysGeoReal::X0() + 50; Xs < KeysGeoReal::X0() + KeysGeoReal::SizeX(); Xs += 100)
			{
				const int32 Ph = KeysGeoReal::PhaseAt(Xs, KeysGeoReal::ChainZ(Xs));
				if (Last >= 0 && Ph != Last) T.Edge.Add(Xs - 50);
				Last = Ph;
			}
			// four parts: west to east they are 0..3; fewer (an older map): the Middle Keys are the westmost
			T.First = T.Edge.Num() >= 3 ? 0 : 1;
			return T;
		}();
		int32 R = Tb.First;
		for (const double Ed : Tb.Edge) if (X > Ed) R++;
		return FMath::Clamp(R, 0, 3);
	}
	const TCHAR* RegionName(int32 Region)
	{
		static const TCHAR* Names[4] = {TEXT("Lower Keys"), TEXT("Middle Keys"), TEXT("Upper Keys"), TEXT("Key Largo")};
		return Names[FMath::Clamp(Region, 0, 3)];
	}

	double FloorAt(double X, double Z) { return KeysSea::SeafloorHeightAt(X, Z); }
	bool Wet(double X, double Z) { return KeysGeo::LandH(X, Z) < -.03; }

	// which way the open water lies from a tree on the shore: up the slope of the distance from the shoreline
	FVector2D Outward(double X, double Z)
	{
		FVector2D G(KeysGeoReal::Shore(X + 8, Z) - KeysGeoReal::Shore(X - 8, Z), KeysGeoReal::Shore(X, Z + 8) - KeysGeoReal::Shore(X, Z - 8));
		if (G.SizeSquared() < .04)
		{
			// flat (far out on a bank): the lowest ground about
			double Best = 1e9;
			for (int32 k = 0; k < 8; k++)
			{
				const double A = k * PI / 4, H = KeysGeo::LandH(X + FMath::Cos(A) * 10, Z + FMath::Sin(A) * 10);
				if (H < Best) { Best = H; G = FVector2D(FMath::Cos(A), FMath::Sin(A)); }
			}
		}
		return G.GetSafeNormal();
	}

	// how shut in a piece of water is: of eight ways out, how many meet land within 60 m (8: a pond)
	static int32 Enclosed(double X, double Z)
	{
		int32 N = 0;
		for (int32 k = 0; k < 8; k++)
		{
			const double A = k * PI / 4 + .2;
			for (double D = 15; D <= 60; D += 15)
				if (KeysGeo::LandH(X + FMath::Cos(A) * D, Z + FMath::Sin(A) * D) > 0 || KeysGeoReal::MangroveAt(X + FMath::Cos(A) * D, Z + FMath::Sin(A) * D) >= 200) { N++; break; }
		}
		return N;
	}

	// ---------------------------------------------------------------- the rookery keys
	// Small wild islands that are all mangrove, off the road: the best few (by a fixed roll of the dice) in each part
	// of the chain, kept well apart. Each part gets at least one: the rule is loosened for a part that has none.
	const TArray<FRookery>& Rookeries()
	{
		static const TArray<FRookery> Rk = []
		{
			TArray<FRookery> Out;
			if (!KeysGeoReal::Loaded()) return Out;
			struct FCand { FRookery R; double Score, Frac, Area; };
			TArray<FCand> Cands;
			const TArray<KeysGeoReal::FIsl>& Isl = KeysGeoReal::Islands();
			for (int32 i = 0; i < Isl.Num(); i++)
			{
				const KeysGeoReal::FIsl& I = Isl[i];
				if (I.Area < 900 || I.Area > 160000 || I.Ring.Num() < 6) continue;
				const double Rad = FMath::Sqrt(I.Area / PI);
				if (FMath::Abs(I.Z - KeysGeoReal::ChainZ(I.X)) < 220 + Rad || KeysGeo::CanalDist(I.X, I.Z) < 80) continue;
				// how much of it is mangrove: its middle, and points half way out to its edge
				int32 N = 1, M = KeysGeoReal::MangroveAt(I.X, I.Z) ? 1 : 0;
				const int32 Step = FMath::Max(1, I.Ring.Num() / 2 / 12);
				for (int32 k = 0; k + 1 < I.Ring.Num(); k += 2 * Step)
				{
					const double PX = (I.X + I.Ring[k]) * .5, PZ = (I.Z + I.Ring[k + 1]) * .5;
					N++;
					if (KeysGeoReal::MangroveAt(PX, PZ)) M++;
				}
				FCand C;
				C.R.C = FVector2D(I.X, I.Z); C.R.R = FMath::Clamp(Rad * 1.25, 28.0, 150.0); C.R.Region = RegionAt(I.X); C.R.Island = i;
				C.Frac = double(M) / N;
				C.Area = I.Area;
				C.Score = EHash(i * 1.7 + 11.0, I.Area * .001 + 5.3) + (I.Area > 2500 && I.Area < 9000 ? .5 : 0);
				Cands.Add(C);
			}
			Cands.Sort([](const FCand& A, const FCand& B) { return A.Score > B.Score; });
			for (int32 Reg = 0; Reg < 4; Reg++)
			{
				int32 Got = 0;
				// (a key some 40 to 130 m across first: small enough that its birds crowd it; a bigger one only if a part has none)
				for (const FVector2D Rule : {FVector2D(.85, 14000), FVector2D(.5, 14000), FVector2D(.5, 160000), FVector2D(.01, 160000)})
				{
					const double MinFrac = Rule.X;
					for (const FCand& C : Cands)
					{
						if (Got >= 3) break;
						if (C.R.Region != Reg || C.Frac < MinFrac || C.Area > Rule.Y || C.Area < 1300) continue;
						bool bNear = false;
						for (const FRookery& O : Out) bNear |= FVector2D::Distance(O.C, C.R.C) < 1500;
						if (!bNear) { Out.Add(C.R); Got++; }
					}
					if (Got) break;
				}
			}
			return Out;
		}();
		return Rk;
	}

	int32 RookAt(double X, double Z)
	{
		const TArray<FRookery>& Rk = Rookeries();
		for (int32 i = 0; i < Rk.Num(); i++)
			if (FMath::Square(X - Rk[i].C.X) + FMath::Square(Z - Rk[i].C.Y) < Rk[i].R * Rk[i].R) return i;
		return -1;
	}

	FVector2D RookSeaDir(int32 R)
	{
		static TMap<int32, FVector2D> Have;
		if (const FVector2D* D = Have.Find(R)) return *D;
		const FRookery& Rk = Rookeries()[R];
		FVector2D Best(1, 0);
		int32 BestN = -1;
		for (int32 k = 0; k < 16; k++)
		{
			const double A = k * PI / 8 + 2 * PI * EHash(Rk.C.X * .01, Rk.C.Y * .01);
			const FVector2D D(FMath::Cos(A), FMath::Sin(A));
			int32 N = 0;
			for (double L = Rk.R + 30; L < Rk.R + 420; L += 30)
			{
				const FVector2D Q = Rk.C + D * L;
				if (KeysGeo::LandH(Q.X, Q.Y) < -.05 && !KeysGeoReal::MangroveAt(Q.X, Q.Y)) N += L < Rk.R + 150 ? 3 : 1;
			}
			if (N > BestN) { BestN = N; Best = D; }
		}
		return Have.Add(R, Best);
	}

	// ---------------------------------------------------------------- rules the mangrove builder shares
	bool PerchLimb(const FSite& S, FVector& A, FVector& B)
	{
		if (S.Bank ? SHash(S, 31) > .07 : SHash(S, 31) > .05) return false;
		const FVector2D O = S.Bank ? S.In : Outward(S.X, S.Z);
		const double Base = FMath::Max(S.Ground, -.05);
		A = FVector(S.X, Base + S.Hh * .85, S.Z);
		B = A + FVector(O.X * 2.7, .3 + .3 * SHash(S, 33), O.Y * 2.7);
		return true;
	}

	bool RoostSnag(int64 Key, const TArray<FSite>& Sites, FVector& Foot, double& Seed)
	{
		const double TX0 = int32(Key >> 32) * 600.0, TZ0 = int32(uint32(Key)) * 600.0;
		if (Sites.Num() < 6 || EHash(TX0 * .013 + 3.0, TZ0 * .017 + 1.0) > .3) return false;
		// the tile's most open point: the tree that wades furthest from the shoreline
		const FSite* Best = nullptr;
		double BestD = -1e9;
		for (const FSite& S : Sites)
		{
			if (S.Bank) continue;
			const double D = KeysGeoReal::Shore(S.X, S.Z);
			if (D > BestD) { BestD = D; Best = &S; }
		}
		if (!Best) return false;
		const FVector2D O = Outward(Best->X, Best->Z);
		const double X = Best->X + O.X * 5, Z = Best->Z + O.Y * 5, G = KeysGeo::LandH(X, Z);
		if (G > .3 || KeysGeoReal::MangroveAt(X, Z) >= 200) return false;
		Foot = FVector(X, FMath::Min(G, -.3), Z);
		Seed = 50 * EHash(TX0 * .01, TZ0 * .01 + 7.0);
		return true;
	}

	// ---------------------------------------------------------------- the plan of a tile
	void PlanTile(int64 Key, FPlan& Out)
	{
		using namespace KeysGeo;
		TArray<FSite> Sites;
		TArray<FVector> Tops;
		SitesOfTile(Key, Sites, Tops);
		Out.Key = Key;
		Out.C = FVector2D(int32(Key >> 32) * 600.0 + 300, int32(uint32(Key)) * 600.0 + 300);
		Out.Sites = Sites.Num();
		auto Add = [&Out](uint8 Kind, const FSite& S, const FVector& P, const FVector2D& O, int32 N, double Salt) -> FSpot&
		{
			FSpot& Sp = Out.Spots.AddDefaulted_GetRef();
			Sp.Kind = Kind; Sp.Site = S; Sp.P = P; Sp.Out = O; Sp.N = int16(N);
			Sp.Seed = uint32(SHash(S, Salt + 40) * 1e9);
			Out.Count[Kind] += N;
			return Sp;
		};
		for (const FSite& S : Sites)
		{
			const FVector2D O = S.Bank ? S.In : Outward(S.X, S.Z);
			auto At = [&](double D) { return FVector2D(S.X + O.X * D, S.Z + O.Y * D); };
			const FVector2D W4 = At(S.Bank ? 4.5 : 4.0);
			if (LandH(W4.X, W4.Y) > -.05) continue;   // (a tree whose water side is not water: nothing lives off it)
			const double D4 = DepthAt(W4.X, W4.Y), D8 = DepthAt(At(8).X, At(8).Y);
			// ---- small fish under the roots
			if (SHash(S, 1) < (S.Bank ? .5 : .3))
			{
				const double Hk = SHash(S, 2);
				const uint8 Kind = Hk < .42 ? Silverside : Hk < .62 ? Pilchard : Hk < .84 ? Snapper : Grunt;
				const int32 N = Kind == Silverside ? 60 + int32(SHash(S, 41) * 60) : Kind == Pilchard ? 40 + int32(SHash(S, 41) * 40) : 16 + int32(SHash(S, 41) * 22);
				const FVector2D Hm = At(S.Bank ? 2.2 : 1.6);
				FSpot& Sp = Add(Kind, S, FVector(Hm.X, 0, Hm.Y), O, N, 1);
				Sp.Depth = float(DepthAt(Hm.X, Hm.Y));
				if (S.Bank)
				{
					// down the creek toward its mouth, in mid-channel
					FVector2D ToM(S.MouthX - S.CX, S.MouthZ - S.CZ);
					const double L = ToM.Size();
					Sp.Mouth = FVector2D(S.CX, S.CZ) + (L > 1 ? ToM / L * FMath::Min(L, 45.0) : FVector2D::ZeroVector);
				}
				else
				{
					// out to the first deeper water: the edge of the bank, a channel, a creek's mouth
					FVector2D Best = At(16);
					double BestD = 0;
					for (double D = 10; D <= 64; D += 6)
					{
						const FVector2D Q = At(D);
						if (LandH(Q.X, Q.Y) > -.05 || KeysGeoReal::MangroveAt(Q.X, Q.Y)) continue;
						const double Dq = DepthAt(Q.X, Q.Y);
						if (Dq > BestD + .05) { BestD = Dq; Best = Q; }
						if (Dq >= 1.3) break;
					}
					Sp.Mouth = Best;
				}
			}
			// ---- waders off the roots, where the water is shallow enough to stand in
			if (!S.Bank && SHash(S, 3) < .085)
				for (double D = 3.5; D < 14; D += 1.5)
				{
					const FVector2D Q = At(D);
					if (KeysGeoReal::MangroveAt(Q.X, Q.Y) || LandH(Q.X, Q.Y) > -.05 || DepthAt(Q.X, Q.Y) > .62) continue;
					const double Hk = SHash(S, 4);
					const bool bBay = DZAt(S.X, S.Z) < 0;
					const uint8 Kind = Hk < .36 ? GreatEgret : Hk < .54 ? GreatBlue : Hk < .68 ? LittleBlue : Hk < (bBay ? .86 : .9) ? Ibis : Spoonbill;
					const int32 N = Kind == Ibis ? 3 + int32(SHash(S, 42) * 4) : Kind == Spoonbill && SHash(S, 42) < .4 ? 2 : 1;
					Add(Kind, S, FVector(Q.X, 0, Q.Y), O, N, 3);
					break;
				}
			if (!S.Bank && SHash(S, 23) < .5)
				for (double D = 3.5; D < 14; D += 1.5)
				{
					const FVector2D Q = At(D);
					if (KeysGeoReal::MangroveAt(Q.X, Q.Y) || LandH(Q.X, Q.Y) > -.05 || DepthAt(Q.X, Q.Y) > .62) continue;
					Out.Wade.Add(Q);
					break;
				}
			// ---- a cormorant or an anhinga drying its wings on a low bare limb
			FVector LA, LB;
			if (PerchLimb(S, LA, LB)) Add(SHash(S, 32) < .55 ? Cormorant : Anhinga, S, FMath::Lerp(LA, LB, .8) + FVector(0, .06, 0), O, 1, 31);
			// ---- on a prop root: a kingfisher, a green heron; a night heron asleep there by day
			if (const double Hr = SHash(S, 5); Hr < .06)
				Add(Hr < .022 ? Kingfisher : Hr < .042 ? GreenHeron : NightHeron, S, RootPoint(S, int32(SHash(S, 43) * 2), S.Detail == 2 ? .25 : 1 / 3.0), O, 1, 5);
			// ---- crabs: tree crabs on the roots, fiddlers on the mud the low tide bares, a horseshoe crab on the edge
			if (SHash(S, 6) < .5) Add(TreeCrab, S, FVector(S.X, 0, S.Z), O, 2 + int32(SHash(S, 44) * 3), 6);
			if (!S.Bank && SHash(S, 7) < .16)
			{
				const FVector2D Q = At(1.6 + 2.2 * SHash(S, 45));
				if (DepthAt(Q.X, Q.Y) < .34 && LandH(Q.X, Q.Y) < .1) Add(Fiddler, S, FVector(Q.X, 0, Q.Y), O, 14 + int32(SHash(S, 46) * 14), 7);
			}
			if (!S.Bank && SHash(S, 8) < .014 && D8 > .25 && D8 < 1.0) Add(Horseshoe, S, FVector(At(7).X, 0, At(7).Y), O, 1 + int32(SHash(S, 47) * 1.6), 8);
			// ---- lying off the roots
			const FVector2D W6 = At(S.Bank ? 5 : 6.5);
			const double D6 = DepthAt(W6.X, W6.Y);
			const bool bOpen = LandH(W6.X, W6.Y) < -.05;
			// (the nearest water off this tree that is deep enough for something bigger to lie in, within MaxD)
			auto Deep = [&](double Need, double MaxD, FVector2D& Q)
			{
				for (double D = S.Bank ? 4.5 : 6; D <= (S.Bank ? 6.0 : MaxD); D += 4)
				{
					Q = At(D);
					if (LandH(Q.X, Q.Y) > -.05 || (!S.Bank && KeysGeoReal::MangroveAt(Q.X, Q.Y))) continue;
					if (DepthAt(Q.X, Q.Y) >= Need) return true;
				}
				return false;
			};
			FVector2D Dq;
			if (bOpen && SHash(S, 9) < .03 && Deep(.5, 30, Dq)) Add(Barracuda, S, FVector(Dq.X, 0, Dq.Y), O, 1, 9);
			if (bOpen && SHash(S, 10) < .03) Add(Needlefish, S, FVector(W6.X, 0, W6.Y), O, 3 + int32(SHash(S, 48) * 4), 10);
			if (bOpen && D8 > .4 && SHash(S, 11) < .03) Add(Mullet, S, FVector(At(8).X, 0, At(8).Y), O, 6 + int32(SHash(S, 49) * 7), 11);
			if (bOpen && !S.bRoof && SHash(S, 12) < .012) Add(LemonShark, S, FVector(At(9).X, 0, At(9).Y), O, 1, 12);
			if (bOpen && SHash(S, 13) < .012 && Deep(.45, 30, Dq)) Add(NurseShark, S, FVector(Dq.X, 0, Dq.Y), O, 1, 13);
			if (bOpen && !S.Bank && SHash(S, 14) < .016) Add(Stingray, S, FVector(At(8).X, 0, At(8).Y), O, 1, 14);
			if (SHash(S, 15) < .22 && D6 > .5 && D6 < 1.6 && bOpen && Enclosed(W6.X, W6.Y) >= 7) Add(Jelly, S, FVector(W6.X, 0, W6.Y), O, 5 + int32(SHash(S, 50) * 8), 15);
			// ---- what hunts the small fish
			if (SHash(S, 16) < (S.Bank ? .12 : .045)) Add(Snook, S, FVector(At(S.Bank ? 3 : 3.2).X, 0, At(S.Bank ? 3 : 3.2).Y), O, 2 + int32(SHash(S, 51) * 3), 16);
			if (bOpen && SHash(S, 17) < .02 && Deep(.7, 46, Dq)) Add(Jack, S, FVector(Dq.X, 0, Dq.Y), O, 3 + int32(SHash(S, 52) * 4), 17);
			if (bOpen && SHash(S, 18) < (S.Bank ? .03 : .012) && Deep(.9, 50, Dq)) Add(Tarpon, S, FVector(Dq.X, 0, Dq.Y), O, 2 + int32(SHash(S, 53) * 2), 18);
			// ---- over it all
			if (!S.bRoof && SHash(S, 19) < .014) Add(Tern, S, FVector(At(12).X, 7, At(12).Y), O, 1, 19);
			if (!S.bRoof && SHash(S, 20) < .0045) Add(Osprey, S, FVector(At(30).X, 28, At(30).Y), O, 1, 20);
			if (!S.Bank && SHash(S, 21) < .007 && RookAt(S.X, S.Z) < 0) Add(Pelican, S, S.Top, O, 1, 21);
			// ---- a manatee's haunt: calm, shut-in water a couple of metres deep
			if (SHash(S, 22) < .06)
				for (double D = S.Bank ? 5 : 10; D <= (S.Bank ? 5 : 74); D += 8)
				{
					const FVector2D Q = At(D);
					const double Dm = DepthAt(Q.X, Q.Y);
					if (Dm < 1.2 || Dm > 4.5 || LandH(Q.X, Q.Y) > -.05) continue;
					if (!S.Bank && (KeysGeoReal::Shore(Q.X, Q.Y) < 7 || Enclosed(Q.X, Q.Y) < 4)) continue;
					Add(Manatee, S, FVector(Q.X, 0, Q.Y), O, 1, 22);
					break;
				}
		}
		// ---- the rookery keys: birds on the crowns; and everywhere, crowns the day birds sleep on
		const TArray<FRookery>& Rk = Rookeries();
		const double TX0 = Out.C.X - 300, TZ0 = Out.C.Y - 300;
		bool bRookNear = false;
		for (const FRookery& R : Rk) bRookNear |= FMath::Abs(R.C.X - Out.C.X) < 300 + R.R && FMath::Abs(R.C.Y - Out.C.Y) < 300 + R.R;
		FSite None;
		for (const FVector& Tp : Tops)
		{
			const double Hk = EHash(Tp.X * .713 + 2.0, Tp.Z * .911);
			if (Hk < .16 && Out.Roosts.Num() < 70) Out.Roosts.Add(Tp);
			if (!bRookNear) continue;
			const int32 R = RookAt(Tp.X, Tp.Z);
			// (a big key would hold a thousand: the birds thin out as the key grows)
			if (R < 0 || EHash(Tp.X * 1.31, Tp.Z * .77 + 9.0) > FMath::Clamp(2600.0 / (Rk[R].R * Rk[R].R), .1, .85)) continue;
			// (two or three to a crown as often as one: a rookery is crowded)
			const int32 NHere = 1 + int32(EHash(Tp.X * .37 + 1.0, Tp.Z * .59) * 2.6);
			for (int32 n = 0; n < NHere; n++)
			{
				const double Hb = EHash(Tp.Z * 1.9 + n * 7.7, Tp.X * .43 + 4.0), An = 2 * PI * EHash(Tp.X + n * 3.1, Tp.Z - n);
				const uint8 Kind = Hb < .25 ? GreatEgret : Hb < .47 ? Ibis : Hb < .61 ? Cormorant : Hb < .73 ? Pelican : Hb < .81 ? GreatBlue : Hb < .89 ? Spoonbill : Hb < .93 ? Anhinga : Hb < .97 ? LittleBlue : NightHeron;
				const FVector At = n == 0 ? Tp : Tp + FVector(FMath::Cos(An) * 1.5, -.32, FMath::Sin(An) * 1.5);
				None.X = At.X; None.Z = At.Z;
				FSpot& Sp = Add(Kind, None, At, (FVector2D(Tp.X, Tp.Z) - Rk[R].C).GetSafeNormal(), 1, 60 + n);
				Sp.Rook = int16(R);
			}
		}
		FVector Foot;
		double Seed;
		if (RoostSnag(Key, Sites, Foot, Seed)) { None.X = Foot.X; None.Z = Foot.Z; Add(PelicanRoost, None, Foot, FVector2D(1, 0), 1, 61); }
		(void)TX0; (void)TZ0;
	}

	bool LinkDue(FEco& E, int32 Link, double Every)
	{
		E.LinkN[Link]++;
		if (E.Now - E.LinkT[Link] < Every) return false;
		E.LinkT[Link] = E.Now;
		return true;
	}
	const TCHAR* TideWord(const FEco& E) { return E.Tide < -.25 ? TEXT("ebb") : E.Tide > .25 ? TEXT("flood") : TEXT("mid tide"); }
	const TCHAR* PhaseWord(const FEco& E) { static const TCHAR* W[4] = {TEXT("night"), TEXT("dawn"), TEXT("day"), TEXT("dusk")}; return W[E.Phase & 3]; }

	// ---------------------------------------------------------------- the fishing hook
	double PickBoost(const FString& Key, double X, double Z)
	{
		if (!GEco || !GEco->State()) return 1;
		FEco& E = *GEco->State();
		const FVector2D P(X, Z);
		if (Key == TEXT("snook")) return NearestGroup(E, P, 28, Snook) ? 3.0 : 1;
		if (Key == TEXT("tarpon")) return NearestGroup(E, P, 35, Tarpon) ? 3.0 : 1;
		if (Key == TEXT("jackcrevalle")) return NearestGroup(E, P, 40, Jack) ? 2.5 : 1;
		if (Key == TEXT("barracuda")) return NearestGroup(E, P, 25, Barracuda) ? 2.5 : 1;
		if (Key == TEXT("lemonshark")) return NearestGroup(E, P, 35, LemonShark) ? 2.5 : 1;
		if (Key == TEXT("nurse")) return NearestGroup(E, P, 30, NurseShark) ? 2.5 : 1;
		if (Key == TEXT("mangrove"))
			for (const TPair<int64, FTileE*>& T : E.Tiles)
				for (const FSchoolE& S : T.Value->Schools) if (S.Kind == Snapper && FVector2D::DistSquared(S.C, P) < 18.0 * 18.0) return 1.6;
		return 1;
	}

	double BiteScale(double X, double Z)
	{
		if (!GEco || !GEco->State()) return 1;
		const int32 N = BaitNear(*GEco->State(), FVector2D(X, Z), 22);
		return N >= 120 ? .6 : N >= 40 ? .8 : 1;
	}

	bool PopSound(FName& What, int32& Kind, FVector& Where, float& Power)
	{
		if (!GEco || !GEco->State() || !GEco->State()->Sounds.Num()) return false;
		const FSoundEv Ev = GEco->State()->Sounds[0];
		GEco->State()->Sounds.RemoveAt(0);
		What = Ev.What; Kind = Ev.Kind; Where = Ev.Where; Power = Ev.Power;
		return true;
	}

	static FCensus* RunCensus(const TArray<int64>& Keys)
	{
		const double T0 = FPlatformTime::Seconds();
		FCensus* C = new FCensus();
		C->RookBirds.Init(0, Rookeries().Num());
		for (const int64 K : Keys)
		{
			FPlan P;
			PlanTile(K, P);
			if (!P.Sites && !P.Spots.Num()) continue;
			C->Tiles++;
			C->Sites += P.Sites;
			const int32 Reg = RegionAt(P.C.X);
			for (int32 k = 0; k < NumKinds; k++) C->Count[Reg][k] += P.Count[k];
			for (const FSpot& S : P.Spots)
			{
				if (S.Rook >= 0) C->RookBirds[S.Rook] += S.N;
				else if (!C->bHave[Reg][S.Kind]) { C->bHave[Reg][S.Kind] = true; C->Example[Reg][S.Kind] = FVector2D(S.P.X, S.P.Z); C->ExampleOut[Reg][S.Kind] = S.Out; }
			}
		}
		C->Secs = FPlatformTime::Seconds() - T0;
		return C;
	}

	// where to put the boat to look at something at P whose open water lies along Out: off it in water she floats in,
	// bow toward it (forward is (-sin H, -cos H))
	static FString TpFor(const FVector2D& P, const FVector2D& Out, double Dist)
	{
		FVector2D B = P + Out * Dist;
		for (int32 k = 0; k < 12 && (KeysGeo::DepthAt(B.X, B.Y) < .8 || KeysGeo::LandH(B.X, B.Y) > -.05); k++) B += Out * 4;
		// (and lying along the shore a little short of it, so the chase camera sees it past the boat's bow)
		const FVector2D Tn(-Out.Y, Out.X), B2 = B - Tn * (Dist * .9);
		return FString::Printf(TEXT("bow on tp=%.0f,%.0f,%.3f alongside tp=%.0f,%.0f,%.3f"), B.X, B.Y, FMath::Atan2(-(P.X - B.X), -(P.Y - B.Y)), B2.X, B2.Y, FMath::Atan2(-Tn.X, -Tn.Y));
	}
}

using namespace KeysMangroveEco;

// the nearest animal of a kind (by a piece of its name) to a point: where it is now, and which way its open water lies
static bool EcoNearest(FEco& S, const FString& Name, const FVector& Near, FVector& OutP, FVector2D& OutDir)
{
	double Best = 1e18;
	auto Try = [&](int32 Kind, const FVector& P, const FVector2D& O)
	{
		if (!FString(KindName(Kind)).Contains(Name)) return;
		const double D = FVector::DistSquared2D(FVector(P.X, P.Z, 0), FVector(Near.X, Near.Z, 0));
		if (D < Best) { Best = D; OutP = P; OutDir = O; }
	};
	for (TPair<int64, FTileE*>& T : S.Tiles)
	{
		for (FBird& B : T.Value->Birds) if (B.State != BAway) Try(B.Kind, B.P + FVector(0, .4, 0), B.Out);
		for (FSchoolE& Sc : T.Value->Schools) Try(Sc.Kind, FVector(Sc.C.X, Sc.bAwake ? Sc.Y : S.Sea - .2, Sc.C.Y), Sc.Out);
		for (FGroup& G : T.Value->Groups) Try(G.Kind, G.M.Num() ? FVector(G.C.X, G.M[0].P.Y, G.C.Y) : FVector(G.C.X, S.Sea - .3, G.C.Y), G.Out);
		for (FCrabs& Cr : T.Value->Crabs) Try(Cr.Kind, Cr.Kind == TreeCrab ? RootPoint(Cr.Site, 0, .3) : FVector(Cr.C.X, Cr.FloorY, Cr.C.Y), Cr.Out);
	}
	return Best < 1e17;
}

static FAutoConsoleCommand GEcoCmd(TEXT("kr.eco"), TEXT("Mangrove ecosystem test hooks: stats, census, fish, links, shower, find <kind>, rook"),
	FConsoleCommandWithArgsDelegate::CreateLambda([](const TArray<FString>& Args) { if (GEco) GEco->DebugCmd(FString::Join(Args, TEXT(" "))); }));

AKeysMangroveEco::AKeysMangroveEco()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickGroup = TG_PostPhysics;
	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent = Root;
	LiveMesh = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("EcoBirds"));
	LiveMesh->SetupAttachment(Root);
	LiveMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	LiveMesh->SetCastShadow(false);   // (rebuilt every frame: its shadow would be too)
	StillMesh = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("EcoBirdsStill"));
	StillMesh->SetupAttachment(Root);
	StillMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

AKeysMangroveEco* AKeysMangroveEco::Get() { return GEco; }

void AKeysMangroveEco::BeginPlay()
{
	Super::BeginPlay();
	E = new FEco();
	GEco = this;
	E->bScript = FString(FCommandLine::Get()).Contains(TEXT("KRScript="));
	// every tile that has mangrove, and the tables the rules read, woken here before any is read off the game thread
	TArray<TPair<int64, FVector2D>> Tl;
	MangroveTiles(Tl);
	for (const TPair<int64, FVector2D>& T : Tl) E->Have.Add(T.Key, T.Value);
	RegionAt(0);
	const TArray<FRookery>& Rk = Rookeries();
	E->RookCD.Init(0, Rk.Num());
	KeysGeo::DZAt(0, 0);
	if (UMaterialInterface* M = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/KeysRun/Materials/M_KR_Boid.M_KR_Boid"))) MID = UMaterialInstanceDynamic::Create(M, this);
	else UE_LOG(LogTemp, Warning, TEXT("KREco: M_KR_Boid missing; nothing in the water will be drawn"));
	for (int32 i = 0; i < Rk.Num(); i++)
		UE_LOG(LogTemp, Log, TEXT("KREco rookery %d (%s) at %.0f, %.0f, %.0f m across"), i, RegionName(Rk[i].Region), Rk[i].C.X, Rk[i].C.Y, Rk[i].R * 2);
	// the census of the whole map, counted in the background (it changes nothing; it is for the log)
	TArray<int64> Keys;
	E->Have.GetKeys(Keys);
	Keys.Sort();
	E->bCensusJob = true;
	E->CensusJob = Async(EAsyncExecution::ThreadPool, [Keys] { return RunCensus(Keys); });
	UE_LOG(LogTemp, Log, TEXT("KREco: %d mangrove tiles, %d rookery keys; animals planned within 900 m of the boat"), E->Have.Num(), Rk.Num());
}

void AKeysMangroveEco::EndPlay(const EEndPlayReason::Type Reason)
{
	if (E)
	{
		if (E->bJob) delete E->Job.Get();
		if (E->bCensusJob) delete E->CensusJob.Get();
		for (TPair<int64, FTileE*>& T : E->Tiles) delete T.Value;
		delete E;
		E = nullptr;
	}
	if (GEco == this) GEco = nullptr;
	Super::EndPlay(Reason);
}

static void EcoFlushSoup(UProceduralMeshComponent* M, FSoup& B)
{
	if (!M) return;
	if (!B.V.Num())
	{
		if (B.LastV != 0) { M->ClearMeshSection(0); B.LastV = 0; }
		return;
	}
	if (B.V.Num() != B.LastV)
	{
		// (shaded as cloth: feathers are matte; as a painted surface a black cormorant shone grey under the sky)
		TArray<FVector2D> UV, Kd;
		UV.Init(FVector2D::ZeroVector, B.V.Num());
		Kd.Init(FVector2D(SurfCloth, 0), B.V.Num());
		M->CreateMeshSection_LinearColor(0, B.V, B.T, B.N, UV, Kd, TArray<FVector2D>(), TArray<FVector2D>(), B.C, TArray<FProcMeshTangent>(), false);
		M->SetMaterial(0, AKeysWorld::VertexColorMaterial());
		B.LastV = B.V.Num();
	}
	else M->UpdateMeshSection_LinearColor(0, B.V, B.N, TArray<FVector2D>(), B.C, TArray<FProcMeshTangent>());
}

void AKeysMangroveEco::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!E) return;
	const double T0 = FPlatformTime::Seconds();
	FEco& S = *E;
	S.Dt = FMath::Clamp(double(DeltaSeconds), 0.0, .1);
	S.Now = GetWorld()->GetTimeSeconds();
	// ---- the viewer, the tide and the hour
	if (AKeysBoatPawn* P = Cast<AKeysBoatPawn>(UGameplayStatics::GetPlayerPawn(this, 0)))
	{
		S.Boat = FVector2D(P->BoatX(), P->BoatZ());
		S.BoatSpd = FMath::Abs(P->GetSpeed());
		S.BoatF = FVector2D(-FMath::Sin(P->BoatH()), -FMath::Cos(P->BoatH()));
		S.bHaveBoat = true;
	}
	if (APlayerCameraManager* C = UGameplayStatics::GetPlayerCameraManager(this, 0))
	{
		const FVector L = C->GetCameraLocation();
		S.Cam = FVector(L.X / 100, L.Z / 100, L.Y / 100);
	}
	else S.Cam = FVector(S.Boat.X, 2, S.Boat.Y);
	S.bDiver = false;
	if (const AKeysDiver* Dv = AKeysDiver::Get(this); Dv && Dv->IsDiving()) { S.bDiver = true; S.Diver = FVector(Dv->State().X, Dv->State().Y, Dv->State().Z); }
	S.Sea = KeysOcean::SeaLevel();
	S.Tide = KeysGeo::TideLevel();
	S.Hour = KeysFeed::HourNow(this);
	S.Phase = S.Hour < 5.25 || S.Hour >= 19.75 ? 0 : S.Hour < 7.75 ? 1 : S.Hour < 17.5 ? 2 : 3;
	S.Night = S.Phase == 0 ? 1 : 0;
	S.Life = AKeysLife::Get(this);
	if (!S.bHaveBoat) return;

	// ---- the tiles: plan the nearest one that lacks a plan, drop the ones left behind
	if (S.bJob && S.Job.IsReady())
	{
		FPlan* P = S.Job.Get();
		S.bJob = false;
		if (!S.Tiles.Contains(P->Key) && FVector2D::Distance(P->C, S.Boat) < 1300)
		{
			const double Ts = FPlatformTime::Seconds();
			FTileE* T = new FTileE();
			T->Key = P->Key; T->C = P->C; T->Roosts = P->Roosts; T->Wade = P->Wade;
			T->RoostUsed.Init(false, T->Roosts.Num());
			FMemory::Memcpy(T->Count, P->Count, sizeof(T->Count));
			SpawnBirds(S, *T, *P);
			SpawnWater(S, *T, *P);
			for (const FSpot& Sp : P->Spots) if (Sp.Kind == Manatee) { T->Haunts.Add(Sp); T->Armed.Add(true); }
			S.Tiles.Add(P->Key, T);
			S.bStillDirty = true;
			if (S.bScript) UE_LOG(LogTemp, Log, TEXT("KREco tile at %.0f, %.0f planned: %d trees, %d birds, %d schools, %d others; set up in %.1f ms"), T->C.X, T->C.Y, P->Sites, T->Birds.Num(), T->Schools.Num(), T->Groups.Num() + T->Crabs.Num(), (FPlatformTime::Seconds() - Ts) * 1000);
		}
		delete P;
	}
	if (!S.bJob && S.Now - S.ScanT > .25)
	{
		S.ScanT = S.Now;
		int64 Best = 0;
		double BestD = FMath::Square(900.0);
		bool bAny = false;
		for (const TPair<int64, FVector2D>& H : S.Have)
		{
			const double D = FVector2D::DistSquared(H.Value, S.Boat);
			if (D < BestD && !S.Tiles.Contains(H.Key)) { BestD = D; Best = H.Key; bAny = true; }
		}
		if (bAny)
		{
			S.bJob = true;
			S.JobKey = Best;
			S.Job = Async(EAsyncExecution::ThreadPool, [Best] { FPlan* P = new FPlan(); PlanTile(Best, *P); return P; });
		}
		for (auto It = S.Tiles.CreateIterator(); It; ++It)
			if (FVector2D::Distance(It.Value()->C, S.Boat) > 1300) { delete It.Value(); It.RemoveCurrent(); S.bStillDirty = true; }
	}
	if (S.bCensusJob && S.CensusJob.IsReady())
	{
		S.Census.Reset(S.CensusJob.Get());
		S.bCensusJob = false;
		DebugCmd(TEXT("census"));
	}

	// ---- everything within reach of the camera
	if (S.Sounds.Num() > 16) S.Sounds.RemoveAt(0, S.Sounds.Num() - 16);   // (nobody is listening: keep the latest)
	S.Live.Reset();
	for (FPoolE& P : S.Pools) { P.Xf.Reset(); P.Data.Reset(); }
	S.BirdsDrawn = S.BirdsMoved = S.FishDrawn = S.SchoolsAwake = 0;
	const FVector2D Cam2(S.Cam.X, S.Cam.Z);
	TickRookeries(S);
	for (TPair<int64, FTileE*>& T : S.Tiles)
	{
		if (FVector2D::Distance(T.Value->C, Cam2) > 1030) continue;
		UpdateWater(S, *T.Value);
		UpdateBirds(S, *T.Value);
	}
	S.StillT += S.Dt;
	if (S.StillT > 1 && FVector::DistSquared(S.StillCam, S.Cam) > 15.0 * 15.0) S.bStillDirty = true;
	const bool bStill = S.bStillDirty;
	if (bStill) { S.Still.Reset(); S.bStillDirty = false; S.StillT = 0; S.StillCam = S.Cam; S.StillN = 0; }
	for (TPair<int64, FTileE*>& T : S.Tiles)
	{
		if (FVector2D::Distance(T.Value->C, Cam2) > 1330) continue;
		EmitBirds(S, *T.Value, false);
		if (bStill) EmitBirds(S, *T.Value, true);
	}
	EcoFlushSoup(LiveMesh, S.Live);
	if (bStill) EcoFlushSoup(StillMesh, S.Still);
	FlushPools(this, S);

	// ---- the links that are told once when they turn
	for (int32 i = S.Showers.Num() - 1; i >= 0; i--)
	{
		FShower& Sh = S.Showers[i];
		if (!Sh.bTold && S.Now - Sh.T > 2.5)
		{
			Sh.bTold = true;
			if (Sh.Terns + Sh.Pelicans + Sh.Others > 0 && LinkDue(S, LinkBirds, 3))
				UE_LOG(LogTemp, Log, TEXT("KREco link birds: bait shower at %.0f, %.0f (%s driven up by %s): %d terns, %d pelicans, %d others diverted to dive on it"),
					Sh.P.X, Sh.P.Y, KindName(Sh.Prey), KindName(Sh.Hunter), Sh.Terns, Sh.Pelicans, Sh.Others);
		}
		if (S.Now - Sh.T > 14) S.Showers.RemoveAt(i);
	}
	const int32 Side = S.Tide < -.25 ? -1 : S.Tide > .25 ? 1 : 0;
	if (Side != S.LastTideSide && Side != 0)
	{
		S.LastTideSide = Side;
		int32 NSch = 0, NGo = 0, NPred = 0, NBird = 0;
		for (TPair<int64, FTileE*>& T : S.Tiles)
		{
			for (FSchoolE& Sc : T.Value->Schools) if (FVector2D::Distance(Sc.C, Cam2) < 600) { NSch++; if ((S.Tide < Sc.Th) == (Side < 0)) NGo++; }
			for (FGroup& G : T.Value->Groups) if (G.Prey >= 0 && FVector2D::Distance(G.C, Cam2) < 600) NPred++;
			for (FBird& B : T.Value->Birds) if (B.Prey >= 0 && FVector2D::Distance(FVector2D(B.P.X, B.P.Z), Cam2) < 600) NBird++;
		}
		S.LinkN[LinkTide]++;
		if (NSch) UE_LOG(LogTemp, Log, TEXT("KREco link tide: %s (level %.2f): %d of %d schools of small fish within 600 m %s; %d packs of predators and %d birds are working those schools and go with them"),
			TideWord(S), S.Tide, NGo, NSch, Side < 0 ? TEXT("leaving the roots for the deeper edge and the creek mouths") : TEXT("going back in under the roots"), NPred, NBird);
	}
	if (S.Phase != S.LastPhase)
	{
		const bool bWasNight = S.LastPhase == 0;
		if (S.LastPhase >= 0 && (S.Phase == 0 || bWasNight))
		{
			int32 Day = 0, NightH = 0;
			for (TPair<int64, FTileE*>& T : S.Tiles)
				for (FBird& B : T.Value->Birds)
					if (FVector2D::Distance(FVector2D(B.P.X, B.P.Z), Cam2) < 600) { if (B.Kind == NightHeron) NightH++; else Day++; }
			S.LinkN[LinkNight] += Day + NightH ? 1 : 0;
			if (!(Day + NightH)) {}
			else if (S.Phase == 0) { UE_LOG(LogTemp, Log, TEXT("KREco link night: hour %.1f, dark: %d day birds within 600 m go to roost in the crowns, %d night herons leave their roots to hunt the edge"), S.Hour, Day, NightH); }
			else { UE_LOG(LogTemp, Log, TEXT("KREco link night: hour %.1f, light: %d day birds leave their roosts for the shallows, %d night herons go back to roost"), S.Hour, Day, NightH); }
		}
		S.LastPhase = S.Phase;
	}
	if (S.FlushT > 0 && S.Now - S.FlushT > 1)
	{
		FString Who;
		int32 N = 0;
		for (int32 k = 0; k < NumBirds; k++) if (S.FlushN[k]) { Who += FString::Printf(TEXT("%s%d %s"), N ? TEXT(", ") : TEXT(""), S.FlushN[k], KindName(k)); N += S.FlushN[k]; S.FlushN[k] = 0; }
		S.LinkN[LinkFlush]++;
		UE_LOG(LogTemp, Log, TEXT("KREco flush: the boat at %.1f m/s came within %.0f m: %s lifted off (%d birds)"), S.FlushSpd, S.FlushD, *Who, N);
		S.FlushT = 0;
	}
	// ---- a manatee or two at the haunts (KeysLife's own, asked for when the boat comes within sight)
	static double HauntT = 0;
	HauntT += S.Dt;
	if (HauntT > .5 && S.Life)
	{
		HauntT = 0;
		for (TPair<int64, FTileE*>& T : S.Tiles)
			for (int32 i = 0; i < T.Value->Haunts.Num(); i++)
			{
				const FSpot& H = T.Value->Haunts[i];
				const double D = FVector2D::Distance(FVector2D(H.P.X, H.P.Z), S.Boat);
				if (D > 330) T.Value->Armed[i] = true;
				else if (T.Value->Armed[i] && D < 200 && D > 25 && S.Life->DebugSpawn(TEXT("manatee"), H.P.X, H.P.Z))
				{
					T.Value->Armed[i] = false;
					UE_LOG(LogTemp, Log, TEXT("KREco manatees lying up at %.0f, %.0f (%s)"), H.P.X, H.P.Z, RegionName(RegionAt(H.P.X)));
				}
			}
	}
	// ---- the test camera, held on what it was pointed at
	if (S.bLook && S.LookCam.IsValid())
	{
		FVector P;
		FVector2D O;
		if (S.LookName == TEXT("shower") || S.LookName == TEXT("strike"))
		{
			// held where the next one happens, and it takes its own stills of it (a shower is over in a second)
			const bool bShower = S.LookName == TEXT("shower");
			const int32 Id = bShower ? (S.Showers.Num() ? S.Showers.Last().Id : 0) : S.StrikeId;
			if (Id == 0) S.LookEvent = 0;   // (the last shower has been forgotten: the next is new)
			else if (Id != S.LookEvent)
			{
				S.LookEvent = Id;
				if (bShower) { S.LookAt = FVector(S.Showers.Last().P.X, S.Sea + .25, S.Showers.Last().P.Y); S.LookDir = Outward(S.LookAt.X, S.LookAt.Z); }
				else { S.LookAt = S.StrikeAt; S.LookDir = S.StrikeDir; }
				S.LookShots = bShower ? 4 : 3; S.LookShotT = bShower ? .12 : .3;
			}
			if (S.LookShots > 0 && (S.LookShotT -= S.Dt) <= 0)
			{
				static int32 Shot = 0;
				FScreenshotRequest::RequestScreenshot(FPaths::ScreenShotDir() / FString::Printf(TEXT("KREco_%s_%02d.png"), *S.LookName, Shot++), false, false);
				S.LookShots--; S.LookShotT = .22;
			}
		}
		else if (EcoNearest(S, S.LookName, S.LookAt, P, O) && FVector::Dist(P, S.LookAt) < 60) { S.LookAt = FMath::Lerp(S.LookAt, P, FMath::Min(1.0, S.Dt * 4)); }
		const FVector2D Sdw(-S.LookDir.Y, S.LookDir.X);
		FVector Eye = S.LookAt + FVector(S.LookDir.X * S.LookDist + Sdw.X * S.LookSide, S.LookH, S.LookDir.Y * S.LookDist + Sdw.Y * S.LookSide);
		if (Wet(Eye.X, Eye.Z)) Eye.Y = FMath::Max(Eye.Y, FloorAt(Eye.X, Eye.Z) + .12);   // (never under the bottom)
		if (S.LookH < 0 && S.LookAt.Y < S.Sea) Eye.Y = FMath::Min(Eye.Y, S.Sea - .06);   // (asked for from under the surface: it stays under)
		S.LookCam->SetActorLocationAndRotation(KeysGeo::ToUE(Eye.X, Eye.Y, Eye.Z), (KeysGeo::ToUE(S.LookAt.X, S.LookAt.Y, S.LookAt.Z) - KeysGeo::ToUE(Eye.X, Eye.Y, Eye.Z)).Rotation());
	}
	S.CpuAcc += (FPlatformTime::Seconds() - T0) * 1000;
	S.CpuN++;
}

void AKeysMangroveEco::DebugCmd(const FString& Cmd)
{
	if (!E) return;
	FEco& S = *E;
	TArray<FString> A;
	Cmd.ParseIntoArray(A, TEXT(" "));
	const FString C0 = A.Num() ? A[0] : TEXT("stats");
	const FVector2D Cam2(S.Cam.X, S.Cam.Z);
	if (C0 == TEXT("stats"))
	{
		int32 Near[NumKinds] = {}, Planned[NumKinds] = {};
		int32 Fish = 0, Sch = 0, Awake = 0;
		for (TPair<int64, FTileE*>& T : S.Tiles)
		{
			for (int32 k = 0; k < NumKinds; k++) Planned[k] += T.Value->Count[k];
			for (FBird& B : T.Value->Birds) if (B.State != BAway && FVector2D::Distance(FVector2D(B.P.X, B.P.Z), Cam2) < 600) Near[B.Kind]++;
			for (FSchoolE& Sc : T.Value->Schools) if (FVector2D::Distance(Sc.C, Cam2) < 600) { Near[Sc.Kind] += Sc.N; Sch++; if (Sc.bAwake) { Awake++; Fish += Sc.Fish.Num(); } }
			for (FGroup& G : T.Value->Groups) if (FVector2D::Distance(G.C, Cam2) < 600) Near[G.Kind] += G.M.Num();
			for (FCrabs& Cr : T.Value->Crabs) if (FVector2D::Distance(Cr.C, Cam2) < 600) Near[Cr.Kind] += Cr.N;
			for (const FSpot& H : T.Value->Haunts) if (FVector2D::Distance(FVector2D(H.P.X, H.P.Z), Cam2) < 600) Near[Manatee]++;
		}
		FString Who;
		for (int32 k = 0; k < NumKinds; k++) if (Near[k]) Who += FString::Printf(TEXT(" %s %d,"), KindName(k), Near[k]);
		int32 PoolTris = 0, PoolInst = 0, PoolsOn = 0;
		for (int32 i = 0; i < NumPools; i++) { PoolTris += S.Pools[i].Shown * S.Pools[i].Tris; PoolInst += S.Pools[i].Shown; PoolsOn += S.Pools[i].Shown ? 1 : 0; }
		UE_LOG(LogTemp, Log, TEXT("KREco near (within 600 m of the camera at %.0f, %.0f, %s, by %s; %s, tide %.2f, hour %.1f):%s"), S.Cam.X, S.Cam.Z, RegionName(RegionAt(S.Cam.X)), *KeysGeo::NearestName(S.Cam.X, S.Cam.Z), PhaseWord(S), S.Tide, S.Hour, *Who);
		UE_LOG(LogTemp, Log, TEXT("KREco drawn: %d birds (%d moving), %d + %d bird triangles in 2 meshes; %d instances in %d instanced meshes, %d triangles; %d of %d schools awake with %d fish; %d tiles planned; cpu %.2f ms a frame"),
			S.StillN + S.BirdsMoved, S.BirdsMoved, S.Live.T.Num() / 3, S.Still.T.Num() / 3, PoolInst, PoolsOn, PoolTris, Awake, Sch, Fish, S.Tiles.Num(), S.CpuN ? S.CpuAcc / S.CpuN : 0);
		S.CpuAcc = 0; S.CpuN = 0;
	}
	else if (C0 == TEXT("census"))
	{
		if (!S.Census) { UE_LOG(LogTemp, Log, TEXT("KREco census: still counting")); return; }
		const FCensus& Cn = *S.Census;
		int32 All[NumKinds] = {};
		for (int32 r = 0; r < 4; r++) for (int32 k = 0; k < NumKinds; k++) All[k] += Cn.Count[r][k];
		FString Who;
		for (int32 k = 0; k < NumKinds; k++) Who += FString::Printf(TEXT(" %s %d,"), KindName(k), All[k]);
		UE_LOG(LogTemp, Log, TEXT("KREco census map-wide (%d tiles, %d trees with their feet in the water, counted in %.1f s):%s"), Cn.Tiles, Cn.Sites, Cn.Secs, *Who);
		for (int32 r = 0; r < 4; r++)
		{
			Who.Reset();
			for (int32 k = 0; k < NumKinds; k++) Who += FString::Printf(TEXT(" %s %d,"), KindName(k), Cn.Count[r][k]);
			UE_LOG(LogTemp, Log, TEXT("KREco census %s:%s"), RegionName(r), *Who);
		}
		const TArray<FRookery>& Rk = Rookeries();
		int32 Busiest = 0;
		for (int32 i = 0; i < Rk.Num(); i++) if (Cn.RookBirds[i] > Cn.RookBirds[Busiest]) Busiest = i;
		for (int32 i = 0; i < Rk.Num(); i++)
		{
			const FVector2D O = RookSeaDir(i);
			UE_LOG(LogTemp, Log, TEXT("KREco census rookery %d (%s, by %s) at %.0f, %.0f: %d birds%s; look at it from %s"), i, RegionName(Rk[i].Region), *KeysGeo::NearestName(Rk[i].C.X, Rk[i].C.Y), Rk[i].C.X, Rk[i].C.Y, Cn.RookBirds[i],
				i == Busiest ? TEXT(" (the busiest)") : TEXT(""), *TpFor(Rk[i].C, O, Rk[i].R + 45));
		}
		if (A.Num() > 1)
			for (int32 r = 0; r < 4; r++)
				for (int32 k = 0; k < NumKinds; k++)
					if (Cn.bHave[r][k]) UE_LOG(LogTemp, Log, TEXT("KREco where %s / %s: %.0f, %.0f; %s"), RegionName(r), KindName(k), Cn.Example[r][k].X, Cn.Example[r][k].Y, *TpFor(Cn.Example[r][k], Cn.ExampleOut[r][k], 14));
	}
	else if (C0 == TEXT("fish"))
	{
		// where the small fish are, against the tide
		int32 N = 0, In = 0, OutN = 0, Moving = 0;
		double DSum = 0;
		for (TPair<int64, FTileE*>& T : S.Tiles)
			for (FSchoolE& Sc : T.Value->Schools)
			{
				if (FVector2D::Distance(Sc.Home, S.Boat) > 250) continue;
				const double D = FVector2D::Distance(Sc.C, Sc.Home), L = FMath::Max(1.0, FVector2D::Distance(Sc.Mouth, Sc.Home));
				N++; DSum += D;
				if (D < 2) In++; else if (D > L - 2) OutN++; else Moving++;
			}
		UE_LOG(LogTemp, Log, TEXT("KREco fish at tide %.2f (%s): %d schools of small fish within 250 m of the boat: %d in under the roots, %d out at the deeper edge or the creek mouth, %d on the way; mean %.1f m from their roots"),
			S.Tide, TideWord(S), N, In, OutN, Moving, N ? DSum / N : 0);
	}
	else if (C0 == TEXT("look"))
	{
		// look <kind> [dist] [height] [side] [fov]: a test camera held on the nearest of that kind to the boat, from
		// its open-water side; look rook <n> looks at a rookery key; look off gives the view back to the boat
		APlayerController* PC = GetWorld()->GetFirstPlayerController();
		if (!PC) return;
		TArray<double> Num;
		while (A.Num() > 2 && A.Last().IsNumeric()) { Num.Insert(FCString::Atod(*A.Last()), 0); A.Pop(); }
		FString Name;
		for (int32 i = 1; i < A.Num(); i++) Name += (i > 1 ? TEXT(" ") : TEXT("")) + A[i];
		if (Name == TEXT("off") || Name.IsEmpty()) { S.bLook = false; if (PC->GetPawn()) PC->SetViewTarget(PC->GetPawn()); return; }
		FVector P;
		FVector2D O;
		if (Name == TEXT("rook"))
		{
			const int32 R = Num.Num() ? int32(Num[0]) : 0;
			if (!Rookeries().IsValidIndex(R)) return;
			if (Num.Num()) Num.RemoveAt(0);
			P = FVector(Rookeries()[R].C.X, 5, Rookeries()[R].C.Y); O = RookSeaDir(R);
			S.LookName = TEXT("-");
			if (!Num.Num()) Num = {Rookeries()[R].R + 50, 9};
		}
		else if (Name == TEXT("shower") || Name == TEXT("strike"))
		{
			// (wherever the bait next goes up, or a wader next strikes)
			P = FVector(S.Boat.X, 3, S.Boat.Y); O = FVector2D(1, 0); S.LookName = Name;
			S.LookEvent = Name == TEXT("shower") ? (S.Showers.Num() ? S.Showers.Last().Id : 0) : S.StrikeId;
			S.LookShots = 0;
		}
		else
		{
			if (!EcoNearest(S, Name, FVector(S.Boat.X, 0, S.Boat.Y), P, O)) { UE_LOG(LogTemp, Log, TEXT("KREco look %s: none in the tiles planned round the boat"), *Name); return; }
			S.LookName = Name;
		}
		S.LookAt = P; S.LookDir = O;
		S.LookDist = Num.Num() > 0 ? Num[0] : 6; S.LookH = Num.Num() > 1 ? Num[1] : 1.2; S.LookSide = Num.Num() > 2 ? Num[2] : 0;
		if (!S.LookCam.IsValid()) S.LookCam = GetWorld()->SpawnActor<ACameraActor>();
		if (ACameraActor* LkCam = Cast<ACameraActor>(S.LookCam.Get()))
		{
			LkCam->GetCameraComponent()->SetFieldOfView(Num.Num() > 3 ? float(Num[3]) : 55.f);
			LkCam->GetCameraComponent()->SetConstraintAspectRatio(false);
			PC->SetViewTarget(LkCam);
		}
		S.bLook = true;
		UE_LOG(LogTemp, Log, TEXT("KREco look %s: at %.0f, %.1f, %.0f (%.0f m from the boat), camera %.1f m off and %.1f m up"), *Name, P.X, P.Y, P.Z, FVector2D::Distance(FVector2D(P.X, P.Z), S.Boat), S.LookDist, S.LookH);
	}
	else if (C0 == TEXT("rook"))
	{
		const TArray<FRookery>& Rk = Rookeries();
		for (int32 r = 0; r < Rk.Num(); r++)
		{
			int32 All = 0, Home = 0, Fly = 0, Wait = 0;
			for (TPair<int64, FTileE*>& T : S.Tiles)
				for (FBird& B : T.Value->Birds)
					if (B.Rook == r) { All++; Home += B.State != BAway && B.State != BFly ? 1 : 0; Fly += B.State == BFly ? 1 : 0; Wait += B.Delay > 0 ? 1 : 0; }
			if (All) UE_LOG(LogTemp, Log, TEXT("KREco rookery %d (%s) at %.0f, %.0f, %s: %d birds, %d at home, %d on the wing, %d about to go; next flight in %.1f s; %.0f m from the camera"),
				r, RegionName(Rk[r].Region), Rk[r].C.X, Rk[r].C.Y, PhaseWord(S), All, Home, Fly, Wait, S.RookCD[r], FVector2D::Distance(Rk[r].C, Cam2));
		}
	}
	else if (C0 == TEXT("links"))
	{
		UE_LOG(LogTemp, Log, TEXT("KREco links so far: %d predator rushes, %d showers birds came to, %d wader strikes, %d turns of the tide, %d nightfalls or dawns, %d flushes, %d osprey dives"),
			S.LinkN[LinkRush], S.LinkN[LinkBirds], S.LinkN[LinkWaders], S.LinkN[LinkTide], S.LinkN[LinkNight], S.LinkN[LinkFlush], S.LinkN[LinkOsprey]);
	}
	else if (C0 == TEXT("shower"))
	{
		// test: the predators nearest the boat are hungry now (they still need bait in reach to rush it)
		int32 N = 0;
		for (TPair<int64, FTileE*>& T : S.Tiles)
			for (FGroup& G : T.Value->Groups)
				if ((G.Kind == Snook || G.Kind == Jack || G.Kind == Tarpon || G.Kind == LemonShark || G.Kind == Barracuda) && FVector2D::Distance(G.C, S.Boat) < 120) { G.Hunger = 0; N++; }
		UE_LOG(LogTemp, Log, TEXT("KREco test: %d packs of predators within 120 m of the boat made hungry"), N);
	}
	else if (C0 == TEXT("findall"))
	{
		// the nearest of every kind
		for (int32 k = 0; k < NumKinds; k++) DebugCmd(FString::Printf(TEXT("find1 %s %s"), KindName(k), A.Num() > 1 ? *A[1] : TEXT("9")));
	}
	else if ((C0 == TEXT("find") || C0 == TEXT("find1")) && A.Num() > 1)
	{
		// (a kind's name may be two words: the distance is the last one if it is a number)
		if (A.Num() > 2 && !A.Last().IsNumeric()) { for (int32 i = 2; i < A.Num(); i++) A[1] += TEXT(" ") + A[i]; A.SetNum(2); }
		else if (A.Num() > 3) { for (int32 i = 2; i + 1 < A.Num(); i++) A[1] += TEXT(" ") + A[i]; A[2] = A.Last(); A.SetNum(3); }
		// the nearest few of a kind (by a piece of its name) among the tiles planned round the boat
		struct FHit { double D; FVector2D P, O; int32 Kind; };
		TArray<FHit> Hits;
		auto Try = [&](int32 Kind, const FVector2D& P, const FVector2D& O) { if (FString(KindName(Kind)).Contains(A[1])) Hits.Add({FVector2D::Distance(P, S.Boat), P, O, Kind}); };
		for (TPair<int64, FTileE*>& T : S.Tiles)
		{
			for (FBird& B : T.Value->Birds) if (B.Leader < 0) Try(B.Kind, FVector2D(B.Home.X, B.Home.Z), B.Out);
			for (FSchoolE& Sc : T.Value->Schools) Try(Sc.Kind, Sc.Home, Sc.Out);
			for (FGroup& G : T.Value->Groups) Try(G.Kind, G.Home, G.Out);
			for (FCrabs& Cr : T.Value->Crabs) Try(Cr.Kind, Cr.C, Cr.Out);
			for (const FSpot& H : T.Value->Haunts) Try(Manatee, FVector2D(H.P.X, H.P.Z), H.Out);
		}
		Hits.Sort([](const FHit& X, const FHit& Y) { return X.D < Y.D; });
		const double Dist = A.Num() > 2 ? FCString::Atod(*A[2]) : 12;
		for (int32 i = 0; i < FMath::Min(C0 == TEXT("find1") ? 1 : 4, Hits.Num()); i++)
			UE_LOG(LogTemp, Log, TEXT("KREco find %s: at %.0f, %.0f, %.0f m from the boat; %s"), KindName(Hits[i].Kind), Hits[i].P.X, Hits[i].P.Y, Hits[i].D, *TpFor(Hits[i].P, Hits[i].O, Dist));
		if (!Hits.Num()) UE_LOG(LogTemp, Log, TEXT("KREco find %s: none in the %d tiles planned round the boat"), *A[1], S.Tiles.Num());
	}
}
