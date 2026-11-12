#include "KeysWrecks.h"
#include "KeysGeo.h"
#include "KeysSea.h"
#include "KeysMesh.h"

using namespace KeysGeo;

namespace
{
	FLinearColor Hx(uint32 C) { return FLinearColor(FColor(uint8((C >> 16) & 255), uint8((C >> 8) & 255), uint8(C & 255))); }

	// WRECK_SPECS: what each one is, the stretch of bathymetry it belongs on and the depth band it has to sink into
	struct FSpec
	{
		KeysWrecks::FWreck W;
		double X0, X1, DZ0, DZ1, D0, D1, MaxRelief;
	};
	const FSpec Specs[] = {
		{{TEXT("dc3"), TEXT("Twin-engine airliner"), KeysWrecks::EKind::Plane, 20, 29, 3, .1, .7, true}, 800, 2400, 450, 1150, 7, 8.7, 1.6},
		{{TEXT("barge"), TEXT("Sand barge"), KeysWrecks::EKind::Barge, 46, 11, 3, .05, .8, false}, -3500, -1900, 450, 1150, 7, 8.7, 1.6},
		{{TEXT("tug"), TEXT("Harbor tug"), KeysWrecks::EKind::Tug, 26, 7.5, 7, -.32, 1, false}, 0, 700, 1478, 1510, 11.5, 15, 4},
		{{TEXT("freighterA"), TEXT("Coastal freighter"), KeysWrecks::EKind::Freighter, 84, 13, 15, .42, 2, false}, 1500, 3200, 1510, 1545, 19.5, 25, 7},
		{{TEXT("freighterB"), TEXT("Cargo steamer"), KeysWrecks::EKind::Freighter, 98, 14, 16, -.18, 2.4, true}, -3600, -2200, 1585, 1612, 34, 42, 9},
		{{TEXT("freighterC"), TEXT("Deep-water freighter"), KeysWrecks::EKind::Freighter, 120, 17, 20, .08, 3, true}, -1500, -700, 1700, 1900, 52, 68, 6},
	};
	constexpr int32 SaltX = 9811, SaltDZ = 9812, SaltHdg = 9813, SaltGap = 9814, SaltYaw = 9815;
	constexpr double Clearance = 4, MinShore = 300, MinHump = 320;

	FVector2D Fwd(double H) { return FVector2D(-FMath::Sin(H), -FMath::Cos(H)); }

	double ContourHeading(double X, double Z)
	{
		const double E = 12;
		const double GX = (DepthAt(X + E, Z) - DepthAt(X - E, Z)) / (2 * E), GZ = (DepthAt(X, Z + E) - DepthAt(X, Z - E)) / (2 * E);
		return FMath::Atan2(GZ, -GX);
	}

	double HumpDist(double X, double Z)
	{
		const double DZ = Z - ChainZ(X);
		double Best = 1e18;
		for (const FHump& H : Humps()) Best = FMath::Min(Best, FMath::Sqrt(FMath::Square(X - H.X) + FMath::Square(DZ - H.DZ)));
		return Best;
	}

	bool TryPlace(const FSpec& S, int32 i, int32 k, KeysWrecks::FWreck& Out)
	{
		const uint32 Seed = KeysWrecks::WorldSeed;
		const double X = S.X0 + (S.X1 - S.X0) * KeysSea::HashCell(Seed, i, k, SaltX);
		const double DZ = S.DZ0 + (S.DZ1 - S.DZ0) * KeysSea::HashCell(Seed, i, k, SaltDZ);
		const double Z = ChainZ(X) + DZ;
		if (LandH(X, Z) > -.3) return false;
		const double D = DepthAt(X, Z);
		if (D < S.D0 || D > S.D1 || D < S.W.Height + Clearance) return false;
		const FString Zn = ZoneAt(X, Z);
		if (Zn != TEXT("Hawk Channel") && Zn != TEXT("Reef") && Zn != TEXT("Offshore")) return false;
		if (ShoreInfo(X, Z).D < MinShore || HumpDist(X, Z) < MinHump) return false;
		const double Heading = ContourHeading(X, Z) + (KeysSea::HashCell(Seed, i, k, SaltHdg) - .5) * .5 + (KeysSea::HashCell(Seed, i, k, SaltHdg + 50) < .5 ? 0 : PI);
		const FVector2D F = Fwd(Heading), R(-F.Y, F.X);
		double Lo = 1e18, Hi = -1e18;
		for (double U : {-.5, -.25, 0.0, .25, .5})
			for (double V : {-.5, 0.0, .5})
			{
				const double PX = X + F.X * S.W.Len * U + R.X * S.W.Beam * V, PZ = Z + F.Y * S.W.Len * U + R.Y * S.W.Beam * V;
				if (LandH(PX, PZ) > -.3) return false;
				const double PD = DepthAt(PX, PZ);
				Lo = FMath::Min(Lo, PD); Hi = FMath::Max(Hi, PD);
			}
		if (Hi - Lo > S.MaxRelief || Lo < S.W.Height + Clearance * .5) return false;
		Out = S.W;
		Out.X = X; Out.Z = Z; Out.SeaDepth = D; Out.Heading = Heading;
		Out.Pitch = FMath::Atan2(DepthAt(X + F.X * S.W.Len * .4, Z + F.Y * S.W.Len * .4) - DepthAt(X - F.X * S.W.Len * .4, Z - F.Y * S.W.Len * .4), S.W.Len * .8);
		Out.Gap = S.W.bBroken ? S.W.Len * (.12 + .12 * KeysSea::HashCell(Seed, i, k, SaltGap)) : 0;
		Out.BreakYaw = S.W.bBroken ? (KeysSea::HashCell(Seed, i, k, SaltYaw) - .5) * .9 : 0;
		return true;
	}

	// append a part built in the wreck's own frame (meters: X along the hull toward the bow, Y across, Z up from the
	// keel) to the world mesh, lying on the bottom with its list, pitch, heading and - aft of a break - its own twist
	void Place(FKeysMesh& Out, const FKeysMesh& Part, const KeysWrecks::FWreck& W, bool bAftPiece)
	{
		const FVector2D F = Fwd(W.Heading), R(-F.Y, F.X);
		// on the sea floor as it is drawn (KeysSea relief on top of the depth), pitched to its slope under the hull
		const double Bed = KeysSea::SeafloorHeightAt(W.X, W.Z) - .3;
		const double Pitch = FMath::Atan2(KeysSea::SeafloorHeightAt(W.X - F.X * W.Len * .4, W.Z - F.Y * W.Len * .4) - KeysSea::SeafloorHeightAt(W.X + F.X * W.Len * .4, W.Z + F.Y * W.Len * .4), W.Len * .8);
		const double CL = FMath::Cos(W.List), SL = FMath::Sin(W.List), CP = FMath::Cos(Pitch), SP = FMath::Sin(Pitch);
		const double CB = FMath::Cos(W.BreakYaw), SB = FMath::Sin(W.BreakYaw);
		auto Xf = [&](const FVector& P, bool bDir)
		{
			double A = P.X, S = P.Y, Y = P.Z;
			if (bAftPiece) { const double A2 = A * CB - S * SB - (bDir ? 0 : W.Gap), S2 = A * SB + S * CB; A = A2; S = S2; }
			const double S3 = S * CL - Y * SL, Y3 = S * SL + Y * CL;
			const double A4 = A * CP + Y3 * SP, Y4 = Y3 * CP - A * SP;
			const double GX = F.X * A4 + R.X * S3, GZ = F.Y * A4 + R.Y * S3;
			return bDir ? FVector(GX, GZ, Y4) : FVector(W.X + GX, W.Z + GZ, Bed + Y4);
		};
		const int32 Base = Out.V.Num();
		for (int32 i = 0; i < Part.V.Num(); i++)
		{
			Out.V.Add(Xf(Part.V[i] / 100.0, false) * 100.0);
			Out.N.Add(Xf(Part.N[i], true).GetSafeNormal());
			Out.UV.Add(Part.UV[i]);
			Out.C.Add(Part.C[i]);
		}
		for (int32 Ix : Part.T) Out.T.Add(Base + Ix);
	}

	// what grows on a wreck, scattered over a part's upper surfaces: barnacle and coral crusts, barrel and tube sponges
	// standing up in clumps, purple sea fans across the current, brain-coral domes and soft-coral tufts
	void Growth(FKeysMesh& M, int32 Seed, double L, double B, double Top, int32 N)
	{
		static const uint32 Crust[] = {0x5d6b3a, 0x7a5a3a, 0x8a5a6a, 0x4f5f4a, 0x9a8a5a};
		static const uint32 Sponge[] = {0xc8642a, 0x7a3a8a, 0xd8a03a, 0x8a2a3a};
		auto H = [Seed](int32 i, int32 k) { return KeysSea::HashCell(7177, Seed, i, k); };
		for (int32 i = 0; i < N; i++)
		{
			const double U = H(i, 1) - .5, V = H(i, 2) - .5, Sz = .3 + H(i, 3) * .8;
			const FVector At(U * L * .9, V * B * .8, Top);
			const int32 Kind = int32(H(i, 4) * 6);
			const FLinearColor C = Hx(Crust[i % 5]);
			if (Kind <= 1)
				M.AddRoundBox(At + FVector(0, 0, Sz * .2), FVector(Sz * 1.4, Sz, Sz * .5), Sz * .2, C, FQuat(FVector::ZAxisVector, U * 9), 1);
			else if (Kind == 2)
			{
				// a clump of tube sponges
				const FLinearColor Sc = Hx(Sponge[i % 4]);
				for (int32 k = 0; k < 4; k++)
				{
					const FVector B0 = At + FVector((H(i, 10 + k) - .5) * .5, (H(i, 20 + k) - .5) * .5, 0);
					M.AddTube(B0, B0 + FVector((H(i, 30 + k) - .5) * .2, (H(i, 40 + k) - .5) * .2, .4 + H(i, 50 + k) * .7), .09, .12, 7, Sc);
				}
			}
			else if (Kind == 3)
			{
				// a sea fan: a flat, lacy disc standing across the current on a short stalk
				const double R = .4 + Sz * .5;
				M.AddTube(At, At + FVector(0, 0, .25), .04, .03, 5, Hx(0x5a2a5a));
				M.AddRoundBox(At + FVector(0, 0, .25 + R * .8), FVector(.03, R * 1.8, R * 1.5), .015, Hx(0x8a3a8a), FQuat(FVector::ZAxisVector, H(i, 5) * 2), 1);
			}
			else if (Kind == 4)
			{
				// a barrel sponge
				M.AddTube(At, At + FVector(0, 0, .5 + Sz * .6), .22 + Sz * .1, .32 + Sz * .15, 9, Hx(Sponge[(i + 1) % 4]) * .85f);
			}
			else
			{
				// a brain-coral dome
				M.AddRoundBox(At, FVector(Sz * 1.1, Sz * 1.1, Sz * .8), Sz * .4, Hx(0xb0a070), FQuat::Identity, 2);
			}
		}
	}

	// debris strewn on the sand round a wreck: hull plates, barrels, a coil of chain, timbers (wreck frame, bed at Z0)
	void Debris(FKeysMesh& M, int32 Seed, double L, double B, double Z0, int32 N)
	{
		auto H = [Seed](int32 i, int32 k) { return KeysSea::HashCell(9133, Seed, i, k); };
		const FLinearColor Rust = Hx(0x6b3a26), Dark = Hx(0x2c2a28), Wood = Hx(0x5a4a3a);
		for (int32 i = 0; i < N; i++)
		{
			const double A = H(i, 1) * 2 * PI, R = .6 + H(i, 2) * .9;
			const FVector At(FMath::Cos(A) * L * .5 * R, FMath::Sin(A) * (B * .5 + 3) * R, Z0);
			const FQuat Q = FQuat(FVector::ZAxisVector, H(i, 3) * 6) * FQuat(FVector::XAxisVector, (H(i, 4) - .5) * .5);
			switch (i % 4)
			{
			case 0: M.AddBox(At + FVector(0, 0, .05), FVector(1.5 + H(i, 5) * 2, 1 + H(i, 6), .06), Rust * .85f, Q); break;
			case 1: M.AddTube(At + FVector(0, 0, .3), At + Q.RotateVector(FVector(.85, 0, 0)) + FVector(0, 0, .3), .3, .3, 10, Rust * .7f); break;
			case 2:
				for (int32 k = 0; k < 10; k++)
				{
					const double Aa = k * .9, Rr = .35 + k * .02;
					M.AddTube(At + FVector(FMath::Cos(Aa) * Rr, FMath::Sin(Aa) * Rr, .08), At + FVector(FMath::Cos(Aa + .9) * Rr, FMath::Sin(Aa + .9) * Rr, .08), .06, .06, 5, Dark);
				}
				break;
			default: M.AddBox(At + FVector(0, 0, .12), FVector(2.5 + H(i, 5) * 2, .25, .22), Wood, Q); break;
			}
		}
	}

	// portholes in a row along both sides of a hull, a broken rail along the deck edge
	void Portholes(FKeysMesh& M, double X0, double X1, double HalfB, double Z, int32 N)
	{
		for (int32 k = 0; k < N; k++)
			for (int32 sd = -1; sd <= 1; sd += 2)
			{
				const double X = FMath::Lerp(X0, X1, (k + .5) / N);
				M.AddTube(FVector(X, sd * (HalfB + .02), Z), FVector(X, sd * (HalfB + .08), Z), .22, .22, 10, Hx(0x4a3a2a));
				M.AddTube(FVector(X, sd * (HalfB + .07), Z), FVector(X, sd * (HalfB + .09), Z), .15, .15, 10, Hx(0x0c1012));
			}
	}
	void Rail(FKeysMesh& M, double X0, double X1, double HalfB, double Z, int32 Seed)
	{
		for (int32 sd = -1; sd <= 1; sd += 2)
		{
			const int32 N = FMath::Max(2, int32((X1 - X0) / 2.2));
			for (int32 k = 0; k <= N; k++)
			{
				if (KeysSea::HashCell(4411, Seed, k, sd + 2) < .3) continue;   // stanchions rusted away
				const double X = FMath::Lerp(X0, X1, double(k) / N);
				const double Lean = (KeysSea::HashCell(4411, Seed, k, 7) - .5) * .5;
				M.AddTube(FVector(X, sd * HalfB, Z), FVector(X + Lean * .3, sd * (HalfB + Lean * .2), Z + 1.0), .05, .05, 5, Hx(0x5a3222));
				if (k < N && KeysSea::HashCell(4411, Seed, k, sd + 5) > .4)
					M.AddTube(FVector(X + Lean * .3, sd * (HalfB + Lean * .2), Z + 1.0), FVector(FMath::Lerp(X0, X1, double(k + 1) / N), sd * HalfB, Z + .95), .04, .04, 5, Hx(0x5a3222));
			}
		}
	}

	KeysMesh::FHullDef SteelHull(double F, double YK, double Entry, double Rake, uint32 HullC)
	{
		KeysMesh::FHullDef H;
		H.F = F; H.Spring = F * .08; H.YK = YK; H.Dr0 = 8; H.Dr1 = 34; H.Rake = Rake; H.Entry = Entry;
		H.Bottom = Hx(0x4a2c1e); H.Boot = Hx(0x6b3a26); H.Hull = Hx(HullC); H.bCove = false; H.Rub = Hx(0x5a3222);
		H.Cap = Hx(0x6b4a36); H.Liner = Hx(0x5a4636); H.Deck = Hx(0x6e5440);
		return H;
	}
}

namespace KeysWrecks
{
	const TArray<FWreck>& Wrecks()
	{
		static TArray<FWreck> A;
		static bool bBuilt = false;
		if (bBuilt) return A;
		bBuilt = true;
		KeysGeo::Init();
		for (int32 i = 0; i < UE_ARRAY_COUNT(Specs); i++)
			for (int32 k = 0; k < 4000; k++)
			{
				FWreck W;
				if (TryPlace(Specs[i], i, k, W)) { A.Add(W); break; }
			}
		for (const FWreck& W : A) UE_LOG(LogTemp, Log, TEXT("Keys Run: wreck %s at %.0f, %.0f in %.1f m"), W.Id, W.X, W.Z, W.SeaDepth);
		return A;
	}

	double EdgeDist(const FWreck& W, double X, double Z)
	{
		const FVector2D F = Fwd(W.Heading);
		const double DX = X - W.X, DZ = Z - W.Z;
		const double Along = FMath::Clamp(DX * F.X + DZ * F.Y, -W.Len / 2 - W.Gap, W.Len / 2);
		return FMath::Max(0.0, FMath::Sqrt(FMath::Square(X - (W.X + F.X * Along)) + FMath::Square(Z - (W.Z + F.Y * Along))) - W.Beam / 2);
	}

	const FWreck* Near(double X, double Z, double R)
	{
		const FWreck* Best = nullptr;
		double BD = R;
		for (const FWreck& W : Wrecks())
		{
			const double D = EdgeDist(W, X, Z);
			if (D < BD) { BD = D; Best = &W; }
		}
		return Best;
	}

	const TArray<TPair<FString, double>>& Residents()
	{
		static const TArray<TPair<FString, double>> R = {
			{TEXT("amberjack"), 5}, {TEXT("goliath"), 2.4}, {TEXT("grouper"), 2.6}, {TEXT("gag"), 1.6}, {TEXT("cobia"), 2.4}, {TEXT("barracuda"), 2},
			{TEXT("jackcrevalle"), 2.4}, {TEXT("bullshark"), .9}, {TEXT("kingfish"), 1.2}, {TEXT("mutton"), 1}, {TEXT("blacktip"), .5}};
		return R;
	}

	void Build(FKeysMesh& Out)
	{
		const FLinearColor Rust = Hx(0x6b3a26), Steel = Hx(0x3d4246), Dark = Hx(0x24282b), Pale = Hx(0x8a8170), Alu = Hx(0x8f9496);
		int32 Wi = 0;
		for (const FWreck& W : Wrecks())
		{
			Wi++;
			const double L = W.Len, B = W.Beam, H = W.Height;
			// the forward piece (the whole hull if it is in one piece) and, if broken, the aft one
			FKeysMesh Fore, Aft;
			switch (W.Kind)
			{
			case EKind::Freighter:
			{
				const double F = H * .42, YK = H * .3;
				// lofted steel hull, keel on the bed; a broken one is a bow section and a blunt stern section
				FKeysMesh Hull;
				if (W.bBroken)
				{
					KeysMesh::BuildHullLoft(Fore, SteelHull(F, YK, .3, 4, 0x3d4246), L * .5, B, true);
					KeysMesh::BuildHullLoft(Aft, SteelHull(F, YK, .985, .6, 0x3d4246), L * .5, B, true);
				}
				else KeysMesh::BuildHullLoft(Fore, SteelHull(F, YK, .62, 4, 0x3d4246), L, B, true);
				FKeysMesh& Back = W.bBroken ? Aft : Fore;
				const double BackX = W.bBroken ? -L * .25 : 0, ForeX = W.bBroken ? L * .25 : 0;
				// shift the parts so the keel sits on the bed and the pieces line up along the hull
				auto Shift = [](FKeysMesh& M, const FVector& D) { for (FVector& P : M.V) P += D * 100.0; };
				Shift(Fore, FVector(ForeX, 0, YK - .5));
				if (W.bBroken) Shift(Aft, FVector(BackX, 0, YK - .5));
				const double Deck = YK - .5 + F - .4;
				// the bridge house aft, its funnel and the wheelhouse windows
				const double HX = -L * .36;
				Back.AddBox(FVector(HX, 0, Deck + H * .2), FVector(L * .1, B * .78, H * .4), Pale * .8f);
				Back.AddBox(FVector(HX + .5, 0, Deck + H * .44), FVector(L * .07, B * .9, H * .1), Rust);
				for (int32 k = -2; k <= 2; k++) Back.AddBox(FVector(HX + L * .05 + .02, k * B * .16, Deck + H * .3), FVector(.1, B * .1, H * .06), Dark);
				Back.AddTube(FVector(HX - L * .03, 0, Deck + H * .4), FVector(HX - L * .04, 0, Deck + H * .62), B * .12, B * .1, 10, Rust);
				// hatches and the cargo derricks forward, one mast down across the deck
				FKeysMesh& Front = Fore;
				for (int32 k = 0; k < 3; k++)
				{
					const double AX = (W.bBroken ? L * .12 : -L * .12) + k * L * .11;
					Front.AddBox(FVector(AX, 0, Deck + .5), FVector(L * .07, B * .55, 1.0), Steel * .8f);
					Front.AddBox(FVector(AX, 0, Deck + 1.05), FVector(L * .065, B * .5, .12), Rust * .8f);
				}
				Front.AddTube(FVector(W.bBroken ? L * .2 : L * .02, 0, Deck), FVector(W.bBroken ? L * .2 : L * .02, 0, Deck + H * .55), .3, .22, 6, Rust);
				Front.AddTube(FVector(W.bBroken ? L * .34 : L * .28, -B * .2, Deck + .3), FVector(W.bBroken ? L * .1 : L * .05, B * .55, Deck + 1.2), .25, .2, 6, Rust);
				Portholes(Back, HX - L * .05, HX + L * .05, B * .39, Deck + H * .12, 4);
				Portholes(Fore, W.bBroken ? L * .05 : -L * .2, W.bBroken ? L * .4 : L * .35, B * .47, Deck - 1.2, 8);
				Rail(Front, W.bBroken ? 0 : -L * .3, W.bBroken ? L * .42 : L * .42, B * .46, Deck, Wi);
				Rail(Back, W.bBroken ? -L * .48 : -L * .48, W.bBroken ? -L * .02 : -L * .3, B * .46, Deck, Wi + 7);
				// the anchor chain run out of the hawse, a capstan and bitts on the foredeck
				Front.AddTube(FVector(W.bBroken ? L * .4 : L * .42, 0, Deck + .3), FVector(W.bBroken ? L * .4 : L * .42, 0, Deck + .9), .45, .4, 10, Rust * .8f);
				for (int32 sd = -1; sd <= 1; sd += 2) Front.AddTube(FVector(W.bBroken ? L * .36 : L * .38, sd * B * .3, Deck), FVector(W.bBroken ? L * .36 : L * .38, sd * B * .3, Deck + .6), .2, .2, 8, Dark);
				Growth(Front, Wi * 3, L * .45, B, Deck, 40);
				Growth(Back, Wi * 3 + 1, L * .45, B, Deck, 28);
				Debris(Front, Wi, L, B, -.4, 14);
				break;
			}
			case EKind::Tug:
			{
				const double F = 2.6, YK = 2.4;
				KeysMesh::BuildHullLoft(Fore, SteelHull(F, YK, .45, 1.6, 0x1f2a30), L, B, true);
				for (FVector& P : Fore.V) P += FVector(0, 0, YK - .4) * 100.0;
				const double Deck = YK - .4 + F - .5;
				Fore.AddBox(FVector(L * .05, 0, Deck + 1.3), FVector(L * .3, B * .62, 2.6), Pale * .7f);
				Fore.AddBox(FVector(L * .12, 0, Deck + 3.2), FVector(L * .14, B * .55, 1.6), Pale * .75f);
				for (int32 k = -1; k <= 1; k++) Fore.AddBox(FVector(L * .19 + .02, k * B * .16, Deck + 3.3), FVector(.1, B * .12, .6), Dark);
				Fore.AddTube(FVector(-L * .08, 0, Deck + 2.6), FVector(-L * .1, 0, Deck + 5.4), .7, .6, 10, Hx(0x8a2a1e));
				// the push knees at the bow and the tyre fenders down the sides
				Fore.AddBox(FVector(L * .5 + .4, 0, Deck - .2), FVector(.8, B * .5, 1.6), Dark);
				for (int32 k = 0; k < 6; k++)
					for (int32 sd = -1; sd <= 1; sd += 2)
						Fore.AddTube(FVector(-L * .35 + k * L * .13, sd * (B * .5 + .1), Deck - .6), FVector(-L * .35 + k * L * .13 + .3, sd * (B * .5 + .1), Deck - .6), .35, .35, 8, Dark);
				Portholes(Fore, -L * .1, L * .2, B * .31, Deck + 1.4, 3);
				Rail(Fore, -L * .48, L * .3, B * .47, Deck, Wi);
				// the mast with its radar bar and a towing bitt aft
				Fore.AddTube(FVector(L * .1, 0, Deck + 4), FVector(L * .1, 0, Deck + 7.5), .12, .08, 6, Rust);
				Fore.AddBox(FVector(L * .1, 0, Deck + 6.6), FVector(.3, 1.8, .15), Rust * .8f, FQuat(FVector::XAxisVector, .3));
				Fore.AddTube(FVector(-L * .38, 0, Deck), FVector(-L * .38, 0, Deck + .9), .3, .3, 10, Dark);
				Growth(Fore, Wi * 3, L * .8, B * .8, Deck, 30);
				Debris(Fore, Wi, L, B, -.4, 9);
				break;
			}
			case EKind::Barge:
			{
				// a flat steel box with a rim, sand slumped in the hold
				Fore.AddBox(FVector(0, 0, H * .5 - .4), FVector(L, B, H), Rust * .9f);
				Fore.AddBox(FVector(0, 0, H - .3), FVector(L - 1.4, B - 1.4, .2), Pale);
				for (int32 sd = -1; sd <= 1; sd += 2) Fore.AddBox(FVector(0, sd * (B * .5 - .2), H - .2), FVector(L, .4, .5), Rust * .7f);
				for (int32 e = -1; e <= 1; e += 2) Fore.AddBox(FVector(e * (L * .5 - .2), 0, H - .2), FVector(.4, B, .5), Rust * .7f);
				for (int32 k = 0; k < 5; k++) Fore.AddBox(FVector(-L * .35 + k * L * .17, 0, H - .1), FVector(L * .12, B * .6, .5 + .3 * (k % 2)), Pale * 1.05f, FQuat(FVector::ZAxisVector, k * .3));
				// bollards along the rims and the ribs showing where the plating has gone
				for (int32 k = 0; k < 4; k++)
					for (int32 sd = -1; sd <= 1; sd += 2) Fore.AddTube(FVector(-L * .4 + k * L * .27, sd * (B * .5 - .3), H), FVector(-L * .4 + k * L * .27, sd * (B * .5 - .3), H + .5), .18, .18, 8, Dark);
				for (int32 k = 0; k < 7; k++) Fore.AddBox(FVector(-L * .3 + k * L * .1, B * .5 + .05, H * .5 - .4), FVector(.25, .12, H * .9), Rust * .6f);
				Growth(Fore, Wi * 3, L, B, H - .3, 32);
				Debris(Fore, Wi, L, B, -.4, 8);
				break;
			}
			case EKind::Plane:
			{
				// a DC-3: the fuselage, the low wing with its two engine nacelles, and the tail broken off behind
				const double R = 1.25, Y0 = R + .1;
				Fore.AddTube(FVector(-L * .05, 0, Y0), FVector(L * .38, 0, Y0), R, R * .95, 12, Alu);
				Fore.AddTube(FVector(L * .38, 0, Y0), FVector(L * .48, 0, Y0 - .1), R * .95, .4, 12, Alu);
				Fore.AddBox(FVector(L * .44, 0, Y0 + .5), FVector(.8, 1.4, .3), Dark);   // the cockpit glazing
				Fore.AddBox(FVector(L * .14, 0, Y0 - .7), FVector(3.4, B, .35), Alu * .9f, FQuat(FVector::XAxisVector, .04));
				for (int32 sd = -1; sd <= 1; sd += 2)
				{
					Fore.AddTube(FVector(L * .14 - .5, sd * 5.4, Y0 - .6), FVector(L * .14 + 2.8, sd * 5.4, Y0 - .6), .9, .8, 10, Steel);
					Fore.AddBox(FVector(L * .14 + 2.95, sd * 5.4, Y0 - .6), FVector(.12, 3.0, .3), Dark, FQuat(FVector::XAxisVector, sd * .7));
				}
				for (int32 k = 0; k < 6; k++) Fore.AddBox(FVector(L * .3 - k * 1.6, 0, Y0 + .55), FVector(.5, 2.55, .35), Dark);   // the window line
				// tail section: the rear fuselage, the fin and the tailplane
				Aft.AddTube(FVector(-L * .5, 0, Y0 - .2), FVector(-L * .05, 0, Y0), .55, R, 12, Alu);
				Aft.AddBox(FVector(-L * .45, 0, Y0 + 1.6), FVector(2.6, .25, 3.0), Alu * .95f, FQuat(FVector::YAxisVector, -.3));
				Aft.AddBox(FVector(-L * .45, 0, Y0 + .1), FVector(2.2, 8, .25), Alu * .95f);
				// three-blade propellers on the engines, the cabin door, the cockpit frame
				for (int32 sd = -1; sd <= 1; sd += 2)
					for (int32 k = 0; k < 3; k++)
					{
						const double Ab = k * 2 * PI / 3 + sd * .4;
						const FVector Hc(L * .14 + 3.05, sd * 5.4, Y0 - .6);
						Fore.AddBox(Hc + FVector(0, FMath::Cos(Ab), FMath::Sin(Ab)) * .75, FVector(.06, .3, 1.5), Dark, FQuat(FVector::XAxisVector, Ab - PI * .5) * FQuat(FVector::ZAxisVector, .3));
					}
				Fore.AddBox(FVector(-L * .02, -R * .98, Y0), FVector(1.1, .06, 1.7), Dark);
				for (int32 k = -1; k <= 1; k++) Fore.AddBox(FVector(L * .44, k * .45, Y0 + .66), FVector(.7, .06, .08), Alu * .7f);
				Growth(Fore, Wi * 3, L * .5, B * .6, Y0 - .4, 20);
				Debris(Fore, Wi, L, B * .4, -.4, 6);
				break;
			}
			}
			Place(Out, Fore, W, false);
			if (Aft.V.Num()) Place(Out, Aft, W, W.bBroken);
		}
	}
}
