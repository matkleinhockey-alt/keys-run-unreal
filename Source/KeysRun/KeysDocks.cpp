// The private docks of the real map: a dock out from the yard of every conch house that stands on the water and can
// reach a metre of it at low tide, so the built-up shores look lived on. Where they go is worked out here from
// KeysGeo alone (the houses are found with the same dice AKeysWorld::BuildScenery rolls), so they follow the map as it
// grows: a walkway on piles from the yard out to a T, an L, a plain finger or a square platform, in four finishes of
// timber, with cleats, a ladder, a dock box, a light, here and there a fish-cleaning table, a bench, a kayak rack or
// a stack of traps, a pelican or a gull on a pile, and on most of them a boat lift (KeysLifts.cpp) with one of the
// yards' fleet hung in it or afloat beside it. Each dock is built in its own frame (X out along the dock, Y to its
// right, Z up from the datum) and added to the meshes of its 600 m tile; what floats is drawn at the tide.
#include "KeysDocks.h"
#include "KeysGeo.h"
#include "KeysOcean.h"
#include "KeysWorld.h"
#include "KeysMesh.h"
#include "KeysTown.h"
#include "KeysTownData.h"
#include "KeysEstates.h"
#include "KeysYards.h"
#include "KeysYardProps.h"
#include "KeysMangroveLife.h"
#include "KeysGeoReal.h"
#include "KeysBoatPawn.h"
#include "ProceduralMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Camera/PlayerCameraManager.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "EngineUtils.h"

using namespace KeysYardProps;
using KeysDocks::FDock;
using KeysDocks::FRectL;

namespace
{
	constexpr double TileM = 600;
	constexpr double LowTide = KeysLifts::HighTide;   // how far the tide falls below the datum
	constexpr double NeedDepth = 1.0;                 // the least water under the head at low tide
	constexpr double BerthDepth = .75;                // and under every corner of a lift's berth or a boat alongside
	constexpr double MinLen = 7, MaxLen = 80;         // the walkway, from the water's edge to the head
	constexpr double HouseMax = 55;                   // how far back from the water a house can stand and still own a dock
	constexpr double PocketDepth = 1.6;               // a berth dredged where the shore is too thin: this much water at the datum
	constexpr double PocketShare = .86;               // of the houses on a thin shore, how many had one dug
	constexpr double DockGap = 2.5;                   // the least clear water between two docks

	enum EWhy { WhyOk, WhyFar, WhyNoShore, WhyAskew, WhyShallow, WhyNarrow, WhyLand, WhyChannel, WhyCreek, WhyMarina, WhyPier, WhyEstate, WhyPlat, WhyCove, WhyMangrove, WhyTown, WhyBridge, WhyEdge, WhyDock, WhyCount };
	const TCHAR* const WhyName[WhyCount] = {TEXT("docks"), TEXT("house too far from the water"), TEXT("no water's edge found"), TEXT("shore not in front of the house"), TEXT("no metre of water within reach"),
		TEXT("water too narrow"), TEXT("land in the way"), TEXT("marked channel"), TEXT("creek"), TEXT("marina"), TEXT("pier"), TEXT("estate"), TEXT("plat lot"), TEXT("Cowfish Cove"), TEXT("mangrove"),
		TEXT("town"), TEXT("bridge or US-1"), TEXT("map edge"), TEXT("next dock")};
	const TCHAR* const ShapeName[KeysDocks::ShapeCount] = {TEXT("T"), TEXT("L"), TEXT("finger"), TEXT("platform")};
	const TCHAR* const FinishName[4] = {TEXT("new pine"), TEXT("weathered"), TEXT("stained"), TEXT("composite")};
	const TCHAR* const LiftName[4] = {TEXT("no lift"), TEXT("cradle"), TEXT("elevator"), TEXT("davits")};
	const TCHAR* const StateName[4] = {TEXT("hung"), TEXT("afloat"), TEXT("empty up"), TEXT("empty down")};

	// what the survey found, for the log
	int32 GWhy[WhyCount];
	int32 GHouses = 0, GReach[7], GReachAt[4];
	const double ReachDepth[4] = {.4, .6, .8, 1.0};
	uint32 GChecksum = 0;

	struct FFinish { uint32 Deck, DeckB, Frame, Pile; int32 Cap; bool bSleeve; double Wear; };
	const FFinish Fin[4] = {
		{0xc9b182, 0xbda273, 0x9c8256, 0x8f7a5a, 1, false, 0},     // new pressure-treated pine, white caps on the piles
		{0x9d9a91, 0x8a877f, 0x77736a, 0x857b6c, 0, false, 1},     // weathered silver, bare pile heads, odd planks replaced
		{0x8f5f3c, 0x7f5233, 0x5e3d28, 0x6e5340, 3, false, 0},     // stained hardwood, copper caps
		{0xb9b6ae, 0xaeaba3, 0xe6e4dc, 0x7b6a55, 2, true, 0}};     // grey composite boards, white fascia, sleeved piles

	FVector P3(double X, double Y, double Z) { return FVector(X, Y, Z); }

	// ---------------------------------------------------------------- the houses (as AKeysWorld::BuildScenery places them)
	struct FHouse { double X, Z, ShoreD; FString Island; };
	void Houses(TArray<FHouse>& Out)
	{
		using namespace KeysGeo;
		const double HS = 34;
		for (double Z = WorldZ0; Z < WorldZ0 + WorldSizeZ; Z += HS)
			for (double X = WorldX0; X < WorldX0 + WorldSizeX; X += HS)
			{
				const double JX = X + (Hash(X * .7, Z) - .5) * 10, JZ = Z + (Hash(Z, X * .7) - .5) * 10;
				if (LandH(JX, JZ) < 1.15) continue;
				const FShore S = ShoreInfo(JX, JZ);
				if (S.Isl->bNoHouses || S.Isl->bSmall || InCity(JX, JZ) || SurfaceKind(JX, JZ) != 0 || CreekDist(JX, JZ) < 15 || KeysTown::InFootprint(JX, JZ, 14) || FMath::Abs(JZ - ChainZ(JX)) < 16) continue;
				bool bNearMarina = false;
				for (const FMarina& Mr : Marinas()) bNearMarina |= FMath::Abs(JX - Mr.SX) < 30 && FMath::Abs(JZ - Mr.SZ) < 30;
				if (bNearMarina || KeysTown::NearEstate(JX, JZ, 54) || KeysYards::InFootprint(JX, JZ, 12)) continue;
				if (Hash(JX * 2.1, JZ * 1.7) > (S.Isl->bMainland ? .12 : .5)) continue;
				// a house on an estates parcel goes when the lot is built on: no dock of ours for it
				const double Wd = 6 * FMath::Lerp(.82, 1.3, Hash(JX * 3.3, JZ)) * 1.4, Dp = 7 * FMath::Lerp(.86, 1.26, Hash(JZ * 1.9, JX)) * 1.3;
				int32 Lot = AKeysEstates::PlatParcelAt(JX, JZ);
				for (int32 k = 0; k < 4 && Lot < 0; k++) Lot = AKeysEstates::PlatParcelAt(JX + (k % 2 ? 1 : -1) * Wd * .5, JZ + (k / 2 ? 1 : -1) * Dp * .5);
				if (Lot >= 0) continue;
				Out.Add({JX, JZ, S.D, S.Isl->Name});
			}
	}

	FVector2D SeawardAt(double X, double Z, double St)
	{
		const double GX = KeysGeo::ShoreInfo(X + St, Z).D - KeysGeo::ShoreInfo(X - St, Z).D, GZ = KeysGeo::ShoreInfo(X, Z + St).D - KeysGeo::ShoreInfo(X, Z - St).D;
		const double Ln = FMath::Sqrt(GX * GX + GZ * GZ);
		return Ln < 1e-6 ? FVector2D::ZeroVector : FVector2D(GX / Ln, GZ / Ln);
	}

	// the water at a point at low tide; nothing where the ground stands out of it
	double LowWater(double X, double Z) { return KeysGeo::LandH(X, Z) > -.25 ? -1 : KeysGeo::DepthAt(X, Z) - LowTide; }

	// every part past the walkway stands over water, however thin (enough for a berth to be dredged there)
	bool Afloat(const FDock& D)
	{
		for (int32 i = 1; i < D.Parts.Num(); i++)
		{
			const FRectL& R = D.Parts[i];
			for (int32 k = 0; k < 5; k++)
			{
				const FVector2D P = k == 4 ? D.At((R.A0 + R.A1) / 2, (R.C0 + R.C1) / 2) : D.At(k & 1 ? R.A1 : R.A0, k & 2 ? R.C1 : R.C0);
				if (KeysGeo::LandH(P.X, P.Y) > -.1) return false;
			}
		}
		return true;
	}

	// ---------------------------------------------------------------- what a dock must keep clear of
	int32 PointHazard(double X, double Z)
	{
		using namespace KeysGeo;
		if (X < WBX0 + 30 || X > WBX1 - 30 || Z < WBZ0 + 30 || Z > WBZ1 - 30) return WhyEdge;
		if (InMarkedChannel(X, Z)) return WhyChannel;
		if (CreekDist(X, Z) < 6) return WhyCreek;
		if (OnMangrove(X, Z) || MangroveNear(X, Z) > .7) return WhyMangrove;
		if (KeysYards::InFootprint(X, Z, 18)) return WhyCove;
		if (KeysTown::InFootprint(X, Z, 25) || InFill(X, Z)) return WhyTown;
		if (NearBridge(X, Z) || FMath::Abs(Z - ChainZ(X)) < 26) return WhyBridge;
		for (const FMarina& M : Marinas())
		{
			const double R = (Z - M.SZ) * M.Dir;
			if (FMath::Abs(X - M.SX) < 50 && R > -45 && R < 96) return WhyMarina;
		}
		for (const KeysTown::FPier& P : KeysTown::Piers())
		{
			const double DX = X - P.X0, DZ = Z - P.Z0, A = DX * P.NX + DZ * P.NZ, C = -DX * P.NZ + DZ * P.NX;
			if (A > -15 && A < P.Len + KeysTown::PierHeadD / 2 + 15 && FMath::Abs(C) < KeysTown::PierHeadW / 2 + 14) return WhyPier;
		}
		for (const KeysTown::FEstateSite& E : KeysTown::WaterfrontEstates())
		{
			const double DX = X - E.X, DZ = Z - E.Z, A = DX * E.NX + DZ * E.NZ, C = -DX * E.NZ + DZ * E.NX;
			if (A > -22 && A < E.DockLen + 10 && FMath::Abs(C) < 22) return WhyEstate;
		}
		return WhyOk;
	}

	// a rectangle of a thing's own frame (X0..X1 along it, Y0..Y1 across), set down at (A, C) turned Yaw in the dock's
	FRectL Turned(double A, double C, double Yaw, double X0, double X1, double Y0, double Y1)
	{
		const double Cs = FMath::Cos(Yaw), Sn = FMath::Sin(Yaw);
		FRectL R{1e9, -1e9, 1e9, -1e9};
		for (int32 k = 0; k < 4; k++)
		{
			const double X = k & 1 ? X1 : X0, Y = k & 2 ? Y1 : Y0, PA = A + X * Cs - Y * Sn, PC = C + X * Sn + Y * Cs;
			R.A0 = FMath::Min(R.A0, PA); R.A1 = FMath::Max(R.A1, PA); R.C0 = FMath::Min(R.C0, PC); R.C1 = FMath::Max(R.C1, PC);
		}
		return R;
	}

	// how far apart two rectangles of two docks are (the separating-axis measure: never more than the true gap)
	double RectGap(const FDock& DA, const FRectL& RA, const FDock& DB, const FRectL& RB)
	{
		FVector2D PA[4], PB[4];
		for (int32 k = 0; k < 4; k++)
		{
			PA[k] = DA.At(k & 1 ? RA.A1 : RA.A0, k & 2 ? RA.C1 : RA.C0);
			PB[k] = DB.At(k & 1 ? RB.A1 : RB.A0, k & 2 ? RB.C1 : RB.C0);
		}
		const FVector2D Ax[4] = {FVector2D(DA.NX, DA.NZ), FVector2D(-DA.NZ, DA.NX), FVector2D(DB.NX, DB.NZ), FVector2D(-DB.NZ, DB.NX)};
		double Best = -1e9;
		for (const FVector2D& Axis : Ax)
		{
			double A0 = 1e9, A1 = -1e9, B0 = 1e9, B1 = -1e9;
			for (int32 k = 0; k < 4; k++)
			{
				const double U = FVector2D::DotProduct(PA[k], Axis), V = FVector2D::DotProduct(PB[k], Axis);
				A0 = FMath::Min(A0, U); A1 = FMath::Max(A1, U); B0 = FMath::Min(B0, V); B1 = FMath::Max(B1, V);
			}
			Best = FMath::Max(Best, FMath::Max(B0 - A1, A0 - B1));
		}
		return Best;
	}

	// ---------------------------------------------------------------- one dock's plan
	// the head, the lift's berth and the berth alongside, for the walkway's length as it stands
	void Layout(FDock& D)
	{
		const double HW = D.W / 2;
		D.Parts.Reset();
		D.Parts.Add({-D.Back, D.Len, -HW, HW});
		FRectL Hd{D.Len, D.Len + D.HeadD, -D.HeadW / 2, D.HeadW / 2};
		if (D.Shape == KeysDocks::ShapeL) { if (D.Side > 0) { Hd.C0 = -HW; Hd.C1 = -HW + D.HeadW; } else { Hd.C0 = HW - D.HeadW; Hd.C1 = HW; } }
		else if (D.Shape == KeysDocks::ShapeFinger) { Hd.C0 = -HW; Hd.C1 = HW; }
		D.Parts.Add(Hd);
		const double HeadC = (Hd.C0 + Hd.C1) / 2;
		KeysLifts::FLift& L = D.Lift;
		if (L.Kind != KeysLifts::LiftNone)
		{
			double X0, X1, Y0, Y1;
			KeysLifts::Extent(L, X0, X1, Y0, Y1);
			if (D.bCanal && L.Kind != KeysLifts::LiftDavits)
			{
				// on a canal the boat lies along the seawall beside the platform; an elevator's rails stand at the wall
				L.Yaw = -PI / 2;
				L.X = L.Kind == KeysLifts::LiftElevator ? KeysLifts::EdgeOff(L.Boat) + .15 : .5 - Y0;
				L.Y = D.Side > 0 ? Hd.C1 + .5 + X1 : Hd.C0 - .5 + X0;
			}
			else if (L.Kind == KeysLifts::LiftDavits)
			{
				// on the head's outer edge, the skiff lying along it
				L.Yaw = -PI / 2;
				L.X = Hd.A1 + KeysLifts::EdgeOff(L.Boat);
				L.Y = HeadC;
			}
			else
			{
				// beside the walkway just inshore of the head (a finger: beside its end), bow to the shore or out
				const bool bElev = L.Kind == KeysLifts::LiftElevator;
				if (bElev) L.Yaw = D.Side > 0 ? 0 : PI;                     // its rails stand on the walkway's side of the boat
				const bool bBowOut = FMath::Abs(L.Yaw) < 1;
				const double Limit = D.Shape == KeysDocks::ShapeFinger ? Hd.A1 + .8 : D.Len - .4;
				L.X = Limit - (bBowOut ? X1 : -X0);
				L.Y = D.Side * (HW + (bElev ? KeysLifts::EdgeOff(L.Boat) : .18 + L.Boat.B / 2 + .625));
			}
			D.Parts.Add(Turned(L.X, L.Y, L.Yaw, X0, X1, Y0, Y1));
		}
		if (D.bAlong)
		{
			const FBoat& B = D.AlongBoat;
			if (D.Shape == KeysDocks::ShapeFinger) { D.AlongA = Hd.A1 - B.L / 2 - .6; D.AlongC = -D.Side * (HW + .45 + B.B / 2); if (FMath::Abs(FMath::Abs(D.AlongYaw) - PI / 2) < .1) D.AlongYaw = 0; }
			else { D.AlongA = Hd.A1 + .45 + B.B / 2; D.AlongC = HeadC; if (FMath::Abs(FMath::Abs(D.AlongYaw) - PI / 2) > .1) D.AlongYaw = PI / 2; }
			D.Parts.Add(Turned(D.AlongA, D.AlongC, D.AlongYaw, -B.L / 2 - .8, B.L / 2 + .3, -B.B / 2 - .2, B.B / 2 + .2));
		}
		D.Box = D.Parts[0];
		for (const FRectL& R : D.Parts) { D.Box.A0 = FMath::Min(D.Box.A0, R.A0); D.Box.A1 = FMath::Max(D.Box.A1, R.A1); D.Box.C0 = FMath::Min(D.Box.C0, R.C0); D.Box.C1 = FMath::Max(D.Box.C1, R.C1); }
	}

	// a metre of water at low tide under the head, and enough to float her off under the lift and the berth alongside
	bool DeepEnough(const FDock& D, double* OutLeast = nullptr)
	{
		double Least = 1e9;
		for (int32 i = 1; i < D.Parts.Num(); i++)
		{
			const FRectL& R = D.Parts[i];
			for (int32 k = 0; k < 5; k++)
			{
				const FVector2D P = k == 4 ? D.At((R.A0 + R.A1) / 2, (R.C0 + R.C1) / 2) : D.At(k & 1 ? R.A1 : R.A0, k & 2 ? R.C1 : R.C0);
				const double Wt = LowWater(P.X, P.Y);
				if (Wt < (i == 1 ? NeedDepth : BerthDepth)) { if (OutLeast) *OutLeast = -1; return false; }
				if (i == 1) Least = FMath::Min(Least, Wt);
			}
		}
		if (OutLeast) *OutLeast = Least;
		return true;
	}

	int32 Plan(const FHouse& Hs, const TArray<FDock>& Have, FDock& D, bool bDredge = false)
	{
		using namespace KeysGeo;
		if (Hs.ShoreD < -HouseMax) return WhyFar;
		// down to the water's edge the steepest way
		FVector2D P(Hs.X, Hs.Z), N = SeawardAt(Hs.X, Hs.Z, 8);
		bool bWet = false;
		for (int32 k = 0; k < 90 && !bWet; k++)
		{
			if (k % 4 == 0) { const FVector2D G = SeawardAt(P.X, P.Y, 8); if (!G.IsZero()) N = G; }
			if (N.IsZero()) return WhyNoShore;
			P += N;
			bWet = LandH(P.X, P.Y) <= 0;
		}
		if (!bWet) return WhyNoShore;
		// the dock runs square to the shore there, and the house must be behind it
		N = SeawardAt(P.X, P.Y, 9);
		const FVector2D ToShore = (P - FVector2D(Hs.X, Hs.Z)).GetSafeNormal();
		if (N.IsZero() || FVector2D::DotProduct(ToShore, N) < .55) return WhyAskew;
		P -= N * 4;
		for (int32 k = 0; k < 40 && LandH(P.X, P.Y) > 0; k++) P += N * .25;
		// a canal: the dock starts at the seawall and stands square to it
		const bool bCanal = CanalDist(P.X, P.Y) < 5;
		if (bCanal)
		{
			const FVector2D G(CanalDist(P.X - 1, P.Y) - CanalDist(P.X + 1, P.Y), CanalDist(P.X, P.Y - 1) - CanalDist(P.X, P.Y + 1));
			if (G.IsNearlyZero()) return WhyNoShore;
			N = G.GetSafeNormal();
			if (FVector2D::DotProduct(ToShore, N) < .55) return WhyAskew;
			P -= N * 6;
			for (int32 k = 0; k < 80 && CanalDist(P.X, P.Y) > 0; k++) P += N * .25;
			double CN;
			if (CanalDist(P.X, P.Y) > .3 || !KeysGeoReal::CanalBank(P.X, P.Y, CN)) return WhyNoShore;
		}
		D = FDock();
		D.bCanal = bCanal;
		D.X0 = P.X; D.Z0 = P.Y; D.NX = N.X; D.NZ = N.Y; D.HX = Hs.X; D.HZ = Hs.Z;
		D.Island = Hs.Island;
		D.Seed = int32(Hash(Hs.X * .11, Hs.Z * .13) * 1e6);
		auto Rnd = [&Hs](double K) { return Hash(Hs.X + K * 17.31, Hs.Z - K * 5.77); };
		// the owner's taste
		// (dealt round the list of docks from short hands of different lengths, so even a short list has every kind)
		const int32 Nth = Have.Num();
		static const uint8 FinishDeal[11] = {0, 1, 2, 3, 1, 0, 2, 1, 0, 3, 2};
		static const uint8 ShapeDeal[7] = {KeysDocks::ShapeT, KeysDocks::ShapeL, KeysDocks::ShapeFinger, KeysDocks::ShapePlatform, KeysDocks::ShapeT, KeysDocks::ShapeL, KeysDocks::ShapeT};
		static const uint8 LiftDeal[16] = {1, 1, 3, 1, 2, 1, 0, 1, 1, 3, 1, 0, 1, 2, 1, 1};   // KeysLifts::ELiftKind
		static const uint8 StateDeal[9] = {0, 0, 1, 0, 2, 0, 1, 0, 3};                          // KeysLifts::ELiftState
		D.Finish = FinishDeal[Nth % 11];
		D.W = (D.Finish == 1 ? 1.3 : 1.5) + .45 * Rnd(2);
		D.DeckZ = 1.3 + .22 * Rnd(3);
		KeysLifts::FLift& L = D.Lift;
		L.Kind = LiftDeal[Nth % 16];
		L.State = StateDeal[Nth % 9];
		// (the two rarer lifts take their turns from their own short hands, so the first of each has a boat in it)
		static const uint8 ElevDeal[4] = {0, 1, 0, 2}, DavitDeal[4] = {0, 1, 0, 0};
		if (L.Kind == KeysLifts::LiftElevator) L.State = ElevDeal[((Nth / 16) * 2 + (Nth % 16 == 13 ? 1 : 0)) % 4];
		if (L.Kind == KeysLifts::LiftDavits) L.State = DavitDeal[((Nth / 16) * 2 + (Nth % 16 == 9 ? 1 : 0)) % 4];
		D.Shape = ShapeDeal[Nth % 7];
		if (L.Kind == KeysLifts::LiftDavits && D.Shape == KeysDocks::ShapeFinger) D.Shape = KeysDocks::ShapeT;   // davits want a head to stand on
		D.bUnder = Nth % 6 == 3;
		switch (D.Shape)
		{
		case KeysDocks::ShapeT: D.HeadW = 6 + 3.2 * Rnd(11); D.HeadD = 2.3 + .8 * Rnd(12); break;
		case KeysDocks::ShapeL: D.HeadW = 5 + 2.4 * Rnd(11); D.HeadD = 2.2 + .7 * Rnd(12); break;
		case KeysDocks::ShapeFinger: D.HeadW = D.W; D.HeadD = 2; break;
		default: D.HeadW = 4.3 + 1.2 * Rnd(11); D.HeadD = D.HeadW - .3 * Rnd(12); break;
		}
		D.Side = Rnd(7) < .5 ? 1 : -1;
		// how far the yard is: the walkway runs inland until the ground comes up to the deck (not into the house)
		const double ToHouse = FVector2D::Distance(P, FVector2D(Hs.X, Hs.Z));
		const double BackMax = FMath::Clamp(ToHouse - 7, 1.0, 9.0);
		D.Back = BackMax;
		for (double B = 1; B <= BackMax; B += .5) if (LandH(P.X - N.X * B, P.Y - N.Y * B) >= D.DeckZ - .12) { D.Back = B; break; }
		D.BackZ = LandH(P.X - N.X * D.Back, P.Y - N.Y * D.Back);
		if (bCanal)
		{
			// a platform on the wall: low, a step up from the lawn, as long as the owner liked
			D.Shape = KeysDocks::ShapePlatform;
			D.HeadW = 4.6 + 3.4 * Rnd(11); D.HeadD = 1.7 + .7 * Rnd(12);
			D.DeckZ = 1.12 + .08 * Rnd(3);
			D.Back = 1.4; D.BackZ = KeysGeo::CanalBank + .07;
			if (L.Kind == KeysLifts::LiftCradle && Nth % 3) L.Kind = KeysLifts::LiftElevator;   // most canal lifts hang off the wall
		}
		// the lift and what is on it
		L.Boat = KeysLifts::PickBoat(D.Seed, L.Kind == KeysLifts::LiftElevator ? 7.8 : 9.2, L.Kind == KeysLifts::LiftDavits ? int32(BoatSkiff) : -1);
		L.Yaw = Rnd(8) < .7 ? PI : 0;
		L.DeckZ = D.DeckZ;
		L.PileC = Fin[D.Finish].Pile;
		L.bSleeves = Fin[D.Finish].bSleeve;
		L.Seed = D.Seed;
		D.bAlong = L.Kind != KeysLifts::LiftDavits && Rnd(9) < (L.Kind == KeysLifts::LiftNone ? .75 : .2);
		if (bCanal && L.Kind != KeysLifts::LiftNone) D.bAlong = false;   // (no room in a canal for both)
		D.AlongBoat = KeysLifts::PickBoat(D.Seed + 7777, 8.6);
		D.AlongYaw = Rnd(10) < .5 ? PI / 2 : -PI / 2;
		const double Extra = FMath::FloorToDouble(Rnd(13) * 3);

		// how far out a metre of water is, along the dock's line (for the survey)
		{
			double Reach = -1;
			bool bGot[4] = {};
			for (double A = 2; A <= 80 && Reach < 0; A += 1)
			{
				const FVector2D Q = D.At(A, 0);
				const double Wt = LowWater(Q.X, Q.Y);
				if (Wt >= NeedDepth) Reach = A;
				if (A <= 45) for (int32 k = 0; k < 4; k++) if (Wt >= ReachDepth[k] && !bGot[k]) { bGot[k] = true; GReachAt[k]++; }
			}
			GReach[Reach < 0 ? 6 : Reach < 10 ? 0 : Reach < 20 ? 1 : Reach < 30 ? 2 : Reach < 40 ? 3 : Reach < 50 ? 4 : 5]++;
		}

		int32 Why = WhyShallow;
		for (int32 Try = 0; Try < 2; Try++, D.Side = -D.Side)
		{
			// the shortest walkway that puts everything in a metre of water, and a little more if the owner felt like it
			bool bFound = false;
			if (bCanal) { D.Len = .6; Layout(D); bFound = DeepEnough(D) && D.Box.A1 < 9; }
			else if (!bDredge) for (D.Len = MinLen; D.Len <= MaxLen && !bFound; D.Len += bFound ? 0 : 1) { Layout(D); bFound = DeepEnough(D); if (bFound) break; }
			// on a shore with no metre of water within reach, most owners had a berth dug at the end of a short dock
			// (KeysDocks::Pocket, which KeysGeo reads): the head, the lift and the boat alongside only need to be over water
			else if (Rnd(15) < PocketShare)
				for (D.Len = 9 + FMath::FloorToDouble(Rnd(14) * 10); D.Len <= 46 && !bFound; D.Len += bFound ? 0 : 1) { Layout(D); bFound = Afloat(D); if (bFound) break; }
			if (!bFound) { Why = WhyShallow; continue; }
			D.bPocket = bDredge && !bCanal;
			if (!bCanal && Extra > 0 && D.Len + Extra <= MaxLen)
			{
				D.Len += Extra;
				Layout(D);
				if (!(bDredge ? Afloat(D) : DeepEnough(D))) { D.Len -= Extra; Layout(D); }
			}
			Why = WhyOk;
			// nothing dry under the walkway once it has left the shore, and room to get past beyond the head
			for (double A = 3; A <= D.Len && Why == WhyOk; A += 1) { const FVector2D Q = D.At(A, 0); if (LandH(Q.X, Q.Y) > .1) Why = WhyLand; }
			// (and never more than five eighths of the way across a basin or a cut)
			for (double G = 2; G <= (bCanal ? 10.0 : FMath::Max(16.0, D.Box.A1 * .6)) && Why == WhyOk; G += 2)
				for (int32 k = -1; k <= 1 && Why == WhyOk; k++)
				{
					// (mangroves wading out in front of the head shut it in as surely as a bank does)
					const FVector2D Q = D.At(D.Box.A1 + G, (D.Box.C0 + D.Box.C1) / 2 + k * 4.0);
					if (LandH(Q.X, Q.Y) > -.3) Why = WhyNarrow;
					else if (OnMangrove(Q.X, Q.Y) || MangroveNear(Q.X, Q.Y) > .6) Why = WhyMangrove;
				}
			// clear of everything else that stands on this shore
			for (int32 i = 0; i < D.Parts.Num() && Why == WhyOk; i++)
			{
				const FRectL& R = D.Parts[i];
				for (double A = R.A0 - 3; A <= R.A1 + 3.01 && Why == WhyOk; A += 2.5)
					for (double C = R.C0 - 3; C <= R.C1 + 3.01 && Why == WhyOk; C += 2.5) { const FVector2D Q = D.At(A, C); Why = PointHazard(Q.X, Q.Y); }
			}
			// the estates' lots keep their own waterfront: not on one, not close beside one, not off the front of one
			for (double A = D.Box.A0 - 8; A <= D.Box.A1 + 8.01 && Why == WhyOk; A += 4)
				for (double C = D.Box.C0 - 8; C <= D.Box.C1 + 8.01 && Why == WhyOk; C += 4) { const FVector2D Q = D.At(A, C); if (AKeysEstates::PlatParcelAt(Q.X, Q.Y) >= 0) Why = WhyPlat; }
			for (double A = -44; A <= 0 && Why == WhyOk; A += 4)
				for (int32 k = -1; k <= 1 && Why == WhyOk; k++) { const FVector2D Q = D.At(A, k * 7.0); if (AKeysEstates::PlatParcelAt(Q.X, Q.Y) >= 0) Why = WhyPlat; }
			// and of the docks already there
			for (const FDock& O : Have)
			{
				if (Why != WhyOk) break;
				if (FMath::Abs(O.X0 - D.X0) > 140 || FMath::Abs(O.Z0 - D.Z0) > 140) continue;
				if (RectGap(D, D.Box, O, O.Box) < DockGap) Why = WhyDock;
			}
			if (Why == WhyOk) break;
		}
		if (Why != WhyOk) return Why;
		if (D.bPocket) D.HeadDepth = PocketDepth - LowTide;
		else DeepEnough(D, &D.HeadDepth);
		return WhyOk;
	}

	TArray<FDock> GDocks;
	bool GPlanned = false;
	// the dredged berths (see PlanAll): dock numbers by 64 m square; nothing is dredged until the plan is whole
	constexpr double PocketFeather = 7;
	TMap<int64, TArray<int32>> GPocketAt;
	TArray<FBox2D> GPocketBox;
	bool bPocketsReady = false;

	void PlanAll()
	{
		GPlanned = true;
		GDocks.Reset();
		FMemory::Memzero(GWhy);
		FMemory::Memzero(GReach);
		FMemory::Memzero(GReachAt);
		GHouses = 0;
		GChecksum = 0;
		// -KRNoDocks leaves them out altogether (for before-and-after frame times)
		if (FParse::Param(FCommandLine::Get(), TEXT("KRNoDocks"))) return;
		KeysGeo::Init();
		if (!KeysGeo::RealMap()) return;
		TArray<FHouse> Hs;
		Houses(Hs);
		GHouses = Hs.Num();
		// the houses nearest the water first, so the front row gets the shore
		Hs.Sort([](const FHouse& A, const FHouse& B) { return A.ShoreD != B.ShoreD ? A.ShoreD > B.ShoreD : A.X != B.X ? A.X < B.X : A.Z < B.Z; });
		TArray<int32> Whys;
		for (const FHouse& H : Hs)
		{
			FDock D;
			const int32 Why = Plan(H, GDocks, D);
			Whys.Add(Why);
			if (Why == WhyOk) GDocks.Add(MoveTemp(D));
		}
		// then the thin shores: the docks with water of their own keep their places, the rest fit in round them
		const int32 Natural = GDocks.Num();
		TMap<int32, int32> Second;
		for (int32 i = 0; i < Hs.Num(); i++)
		{
			if (Whys[i] != WhyShallow) continue;
			FDock D;
			Whys[i] = Plan(Hs[i], GDocks, D, true);
			if (Whys[i] == WhyOk) GDocks.Add(MoveTemp(D));
			Second.FindOrAdd(Whys[i])++;
		}
		for (const TPair<int32, int32>& P : Second) UE_LOG(LogTemp, Log, TEXT("KRDocks pockets: thin-shore houses ending as reason %d: %d"), P.Key, P.Value);
		for (const int32 W : Whys) GWhy[W]++;
		UE_LOG(LogTemp, Log, TEXT("KRDocks pockets: %d docks with water of their own, %d with a dredged berth"), Natural, GDocks.Num() - Natural);
		// a fixed order (west to east), and somebody on a dock or two
		GDocks.Sort([](const FDock& A, const FDock& B) { return A.X0 != B.X0 ? A.X0 < B.X0 : A.Z0 < B.Z0; });
		int32 NFish = 0, NHose = 0;
		for (int32 i = 0; i < GDocks.Num(); i++)
		{
			FDock& D = GDocks[i];
			const bool bCanHose = D.Lift.Kind == KeysLifts::LiftCradle && D.Lift.State == KeysLifts::StateHung;
			const double R = Hash(D.Seed * .003 + 4.4, i * .7 + 1.1);
			if (bCanHose && (R < .05 || (!NHose && i > GDocks.Num() / 3))) { D.Person = 2; NHose++; }
			else if (D.Lift.Kind != KeysLifts::LiftDavits && !D.bAlong && (R > .95 || (!NFish && i > GDocks.Num() / 2))) { D.Person = 1; NFish++; }
			const int32 Rec[8] = {FMath::RoundToInt32(D.X0 * 100), FMath::RoundToInt32(D.Z0 * 100), FMath::RoundToInt32(D.Len * 100), D.Shape, D.Finish, D.Lift.Kind, D.Lift.State + (D.bUnder ? 16 : 0), D.Person};
			GChecksum = FCrc::MemCrc32(Rec, sizeof(Rec), GChecksum);
		}
		// the dredged berths: where each lies (64 m squares), and the depth grids told about them
		GPocketAt.Reset();
		for (int32 i = 0; i < GDocks.Num(); i++)
		{
			const FDock& D = GDocks[i];
			if (!D.bPocket) continue;
			double X0 = 1e18, X1 = -1e18, Z0 = 1e18, Z1 = -1e18;
			for (int32 p = 1; p < D.Parts.Num(); p++)
				for (int32 k = 0; k < 4; k++)
				{
					const FVector2D P = D.At(k & 1 ? D.Parts[p].A1 : D.Parts[p].A0, k & 2 ? D.Parts[p].C1 : D.Parts[p].C0);
					X0 = FMath::Min(X0, P.X - PocketFeather); X1 = FMath::Max(X1, P.X + PocketFeather); Z0 = FMath::Min(Z0, P.Y - PocketFeather); Z1 = FMath::Max(Z1, P.Y + PocketFeather);
				}
			for (int32 cz = FMath::FloorToInt(Z0 / 64); cz <= FMath::FloorToInt(Z1 / 64); cz++)
				for (int32 cx = FMath::FloorToInt(X0 / 64); cx <= FMath::FloorToInt(X1 / 64); cx++) GPocketAt.FindOrAdd((int64(cx) << 32) ^ int64(uint32(cz))).Add(i);
			GPocketBox.Add(FBox2D(FVector2D(X0, Z0), FVector2D(X1, Z1)));
		}
		bPocketsReady = true;
		for (const FBox2D& B : GPocketBox) KeysGeo::RebakeDepth(B.Min.X, B.Min.Y, B.Max.X, B.Max.Y);
	}

	// ---------------------------------------------------------------- building one
	struct FDockMesh
	{
		FKeysMesh S, Bt, D, F, Lamp;
		TArray<FVector> Lamps, Unders;   // dock frame
	};

	// a run of decking between A0 and A1, C0 and C1, its top at Z0 at A0 and Z1 at A1; the boards lie across the way
	// you walk (bAlongC: the walk is along C)
	void Deck(FKeysMesh& S, double A0, double A1, double C0, double C1, double Z0, double Z1, const FFinish& Fn, double Seed, bool bAlongC = false)
	{
		const FLinearColor Ca = Hex(Fn.Deck), Cb = Hex(Fn.DeckB), Fr = Hex(Fn.Frame);
		const uint8 Was = S.Surf;
		S.Surf = Fn.bSleeve ? SurfDefault : SurfWood;
		const double U0 = bAlongC ? C0 : A0, U1 = bAlongC ? C1 : A1, Board = .46;
		const int32 NB = FMath::Max(1, FMath::RoundToInt32((U1 - U0) / Board));
		auto ZAt = [&](double A) { return A1 - A0 < 1e-6 ? Z0 : FMath::Lerp(Z0, Z1, (A - A0) / (A1 - A0)); };
		for (int32 i = 0; i < NB; i++)
		{
			const double Ua = FMath::Lerp(U0, U1, double(i) / NB), Ub = FMath::Lerp(U0, U1, double(i + 1) / NB);
			const double Hh = Hash(Seed + i * 1.37, Seed * .31 + i * .11);
			FLinearColor Col = FMath::Lerp(Ca, Cb, float(Hh));
			if (Fn.Wear > 0 && Hh < .07 * Fn.Wear) Col = Hex(0xb9aa8a);                  // a new board let in
			else if (Fn.Wear > 0 && Hh > 1 - .12 * Fn.Wear) Col = Col * .7f;             // one gone dark
			if (bAlongC) S.AddQuad(P3(A0, Ua, Z0), P3(A1, Ua, Z1), P3(A1, Ub, Z1), P3(A0, Ub, Z0), Col, FVector::UpVector);
			else S.AddQuad(P3(Ua, C0, ZAt(Ua)), P3(Ub, C0, ZAt(Ub)), P3(Ub, C1, ZAt(Ub)), P3(Ua, C1, ZAt(Ua)), Col, FVector::UpVector);
		}
		// the frame under the boards: a band round the edge and the underside
		const double Th = .2;
		S.AddQuad(P3(A0, C0, Z0 - Th), P3(A1, C0, Z1 - Th), P3(A1, C0, Z1), P3(A0, C0, Z0), Fr, FVector(0, -1, 0));
		S.AddQuad(P3(A0, C1, Z0 - Th), P3(A1, C1, Z1 - Th), P3(A1, C1, Z1), P3(A0, C1, Z0), Fr, FVector(0, 1, 0));
		S.AddQuad(P3(A0, C0, Z0 - Th), P3(A0, C1, Z0 - Th), P3(A0, C1, Z0), P3(A0, C0, Z0), Fr, FVector(-1, 0, 0));
		S.AddQuad(P3(A1, C0, Z1 - Th), P3(A1, C1, Z1 - Th), P3(A1, C1, Z1), P3(A1, C0, Z1), Fr, FVector(1, 0, 0));
		S.AddQuad(P3(A0, C0, Z0 - Th), P3(A1, C0, Z1 - Th), P3(A1, C1, Z1 - Th), P3(A0, C1, Z0 - Th), Fr * .6f, FVector(0, 0, -1));
		S.Surf = Was;
	}

	void Cleat(FKeysMesh& M, double A, double C, double Z, double Yaw)
	{
		const uint8 Was = M.Surf;
		M.Surf = SurfSteel;
		Box(M, A, C, Z + .03, .1, .05, .06, Hex(0xb9bec2), Yaw);
		Box(M, A, C, Z + .075, .28, .045, .035, Hex(0xc9ced2), Yaw);
		M.Surf = Was;
	}

	// a laughing gull standing at Feet, facing Yaw (drawn large, as the game's other birds are)
	void Gull(FKeysMesh& M, const FVector& Feet, double Yaw, double Sz)
	{
		const FQuat Q(FVector::ZAxisVector, Yaw);
		const FVector F = Q.RotateVector(FVector(1, 0, 0)), Rt = Q.RotateVector(FVector(0, 1, 0)), Up(0, 0, 1);
		const FVector Body = Feet + Up * .15 * Sz;
		for (int32 s = -1; s <= 1; s += 2) M.AddTube(Body + Rt * (.03 * s * Sz), Feet + Rt * (.035 * s * Sz), .008 * Sz, .008 * Sz, 3, Hex(0x2a2a2c));
		KeysShapes::AddBlob(M, Body, FVector(.17, .075, .075) * Sz, Q, Hex(0xf2f2ee), 6, 4);
		KeysShapes::AddBlob(M, Body + Up * .03 * Sz - F * .04 * Sz, FVector(.17, .08, .045) * Sz, Q, Hex(0x8e969d), 6, 3);    // the grey mantle
		KeysShapes::AddBlob(M, Body - F * .2 * Sz + Up * .03 * Sz, FVector(.07, .03, .015) * Sz, Q, Hex(0x1c1c1e), 5, 3);    // black wing tips
		const FVector Head = Body + F * .14 * Sz + Up * .1 * Sz;
		KeysShapes::AddBlob(M, Head, FVector(.05, .042, .042) * Sz, Q, Hex(0x25262a), 6, 4);                                 // the dark hood
		M.AddTube(Head + F * .035 * Sz, Head + F * .1 * Sz - Up * .012 * Sz, .012 * Sz, .004 * Sz, 4, Hex(0x8a2a22));
	}

	void BuildDock(const FDock& Dk, FDockMesh& O)
	{
		const FFinish& Fn = Fin[Dk.Finish % 4];
		auto Rnd = [&Dk](double K) { return Hash(Dk.Seed * .001 + K * 7.13, K * 3.71 + Dk.Seed * .0007); };
		const double HW = Dk.W / 2, Dz = Dk.DeckZ;
		const FRectL Hd = Dk.Parts[1];
		const double HeadC = (Hd.C0 + Hd.C1) / 2;
		FKeysMesh& S = O.S;
		FKeysMesh& D = O.D;
		TArray<FVector> Perch;   // pile heads a bird could take

		// ---- the walkway: a ramp up out of the yard, then level out to the head
		const double RampEnd = .8, Z0 = FMath::Min(Dz, Dk.BackZ + .06);
		Deck(S, -Dk.Back, RampEnd, -HW, HW, Z0, Dz, Fn, Dk.Seed * .01 + 1);
		Deck(S, RampEnd, Dk.Len, -HW, HW, Dz, Dz, Fn, Dk.Seed * .01 + 2);
		// a pile each side every three metres or so, with a cap beam across under the deck
		const bool bTall = Fn.Cap != 0;
		auto DeckPile = [&](double A, double C, double K, bool bLow = false)
		{
			// the tidy docks stand their piles a little proud with caps on; the old ones are cut off anyhow
			const double Top = bLow ? Dz - .02 : bTall ? Dz + .85 + .1 * Rnd(K) : Rnd(K) < .45 ? Dz - .02 : Dz + .35 + .9 * Rnd(K + .5);
			KeysLifts::Pile(S, A, C, Top, .14, Fn.Pile, Top > Dz ? Fn.Cap : 0, Fn.bSleeve);
			if (Top > Dz + .6) Perch.Add(P3(A, C, Top + (Fn.Cap == 1 ? .14 : .05)));
		};
		const int32 NBent = FMath::Max(1, FMath::RoundToInt32((Dk.Len - 1.8) / 3.0));
		for (int32 i = 0; i <= NBent; i++)
		{
			const double A = 1.5 + (Dk.Len - 1.8) * i / NBent;
			for (int32 s = -1; s <= 1; s += 2) DeckPile(A, s * (HW + .13), i * 2.0 + s, bTall && (i + NBent) % 2 == 1);
			S.Surf = SurfWood;
			KeysLifts::Beam(S, P3(A, -HW - .22, Dz - .29), P3(A, HW + .22, Dz - .29), .12, .18, Hex(Fn.Frame) * .8f);
		}
		S.Surf = SurfDefault;
		// a handrail down one side of some of them
		if (Rnd(30) < .3)
		{
			const double C = -Dk.Side * (HW - .06);
			D.Surf = SurfWood;
			const int32 NP = FMath::Max(2, FMath::RoundToInt32((Dk.Len - 1) / 2.3));
			for (int32 i = 0; i <= NP; i++) Box(D, 1 + (Dk.Len - 1.4) * i / NP, C, Dz + .5, .08, .08, 1.0, Hex(Fn.Frame));
			KeysLifts::Beam(D, P3(.9, C, Dz + 1.02), P3(Dk.Len - .3, C, Dz + 1.02), .1, .045, Hex(Fn.Frame));
			KeysLifts::Beam(D, P3(.9, C, Dz + .55), P3(Dk.Len - .3, C, Dz + .55), .04, .08, Hex(Fn.Frame));
			D.Surf = SurfDefault;
		}

		// ---- the head
		Deck(S, Hd.A0, Hd.A1, Hd.C0, Hd.C1, Dz, Dz, Fn, Dk.Seed * .01 + 3, Dk.Shape != KeysDocks::ShapeFinger);
		{
			const int32 NC = FMath::Max(1, FMath::CeilToInt32((Hd.C1 - Hd.C0) / 3.2));
			for (int32 i = 0; i <= NC; i++)
			{
				const double C = FMath::Lerp(Hd.C0 - .13, Hd.C1 + .13, double(i) / NC);
				DeckPile(Hd.A1 + .13, C, 40.0 + i);
				if (Dk.Shape != KeysDocks::ShapeFinger && FMath::Abs(C) > HW + .5) DeckPile(Hd.A0 - .13, C, 60.0 + i);
			}
			if (Hd.A1 - Hd.A0 > 3.4) for (int32 s = 0; s < 2; s++) DeckPile((Hd.A0 + Hd.A1) / 2, s ? Hd.C1 + .13 : Hd.C0 - .13, 80.0 + s);
		}
		// cleats along the outer edge and down the lift's side
		for (double C = Hd.C0 + .5; C <= Hd.C1 - .4; C += 2.6) Cleat(D, Hd.A1 - .14, C, Dz, PI / 2);
		for (double A = Dk.Len - 1; A > FMath::Max(2.0, Dk.Len - 8); A -= 3.2) Cleat(D, A, Dk.Side * (HW - .14), Dz, 0);
		if (Dk.bAlong && Dk.Shape == KeysDocks::ShapeFinger) for (double A = Hd.A1 - .6; A > Hd.A1 - 8; A -= 3.2) Cleat(D, A, -Dk.Side * (HW - .14), Dz, 0);

		// a ladder down into the water: off the end of a finger, else off the head's far side
		{
			FVector Edge = P3(Hd.A0 + (Hd.A1 - Hd.A0) * .3, Dk.Side > 0 ? Hd.C0 : Hd.C1, 0), Out(0, -Dk.Side, 0);
			if (Dk.Shape == KeysDocks::ShapeFinger && Dk.bAlong) { Edge = P3(Hd.A1, 0, 0); Out = FVector(1, 0, 0); }
			// (it leans a hand's breadth, as they do)
			const FVector Foot = Edge + Out * .2 + FVector(0, 0, -1.1), Top = Edge + Out * .06 + FVector(0, 0, Dz);
			Ladder(D, Foot, Top);
			const FVector Sd = FVector::CrossProduct(Out, FVector::UpVector);
			D.Surf = SurfSteel;
			for (int32 s = -1; s <= 1; s += 2)
			{
				const FVector B0 = Top + Sd * (.21 * s);
				D.AddTube(B0, B0 + FVector(0, 0, .8), .022, .022, 4, Hex(0xc4c9cd));
				D.AddTube(B0 + FVector(0, 0, .8), B0 - Out * .4 + FVector(0, 0, .72), .022, .022, 4, Hex(0xc4c9cd));
				D.AddTube(B0 - Out * .4 + FVector(0, 0, .72), B0 - Out * .4, .022, .022, 4, Hex(0xc4c9cd));
			}
			D.Surf = SurfDefault;
		}

		// ---- what the owner keeps out there. Spots along the head's inner edge, clear of the walkway's mouth
		// Far: the head's side edge away from the lift (an L: the walkway's own edge); Near: the other (an L: the arm's end)
		const double Far = Dk.Side > 0 ? Hd.C0 : Hd.C1, Near = Dk.Side > 0 ? Hd.C1 : Hd.C0, Sg = Dk.Side, HeadD = Hd.A1 - Hd.A0;
		const bool bRoomy = Dk.Shape != KeysDocks::ShapeFinger, bArm = Dk.Shape == KeysDocks::ShapeL;
		// the dock box
		if (bRoomy && Rnd(31) < .85)
		{
			// against the side edge, lid opening toward the middle
			const FVector At = bArm ? P3(Hd.A0 + HeadD / 2, Near - Sg * .42, Dz) : P3(Hd.A1 - .78, Far + Sg * .42, Dz);
			const double In = bArm ? -Sg : Sg;
			D.Surf = SurfGelcoat;
			D.AddRoundBox(At + FVector(0, 0, .29), FVector(1.2, .62, .56), .05, Hex(0xf1f2f0));
			D.Surf = SurfDefault;
			Box(D, At.X, At.Y + In * .315, At.Z + .44, 1.2, .012, .02, Hex(0x9aa0a4));
			Box(D, At.X, At.Y + In * .32, At.Z + .36, .1, .02, .06, Hex(0x8d9499));
		}
		// the light: a post with a shaded lamp, a low bollard, or a gooseneck off a tall pile
		{
			const int32 Kind = Fn.bSleeve ? 1 : Dk.Finish == 1 ? 2 : Rnd(32) < .6 ? 0 : 1;
			const int32 NL = bRoomy && Dk.HeadW > 6.5 && Rnd(33) < .5 ? 2 : 1;
			for (int32 i = 0; i < NL; i++)
			{
				const double C = Dk.Shape == KeysDocks::ShapeFinger ? Dk.Side * (HW - .12) : (i == 0) == (Dk.Side > 0) ? Hd.C1 - .2 : Hd.C0 + .2;
				const FVector B0 = P3(Hd.A1 - .2, C, Dz);
				FVector Lamp;
				if (Kind == 0)
				{
					D.Surf = SurfWood;
					Box(D, B0.X, B0.Y, B0.Z + 1.2, .1, .1, 2.4, Hex(Fn.Frame));
					D.Surf = SurfMetalPaint;
					D.AddTube(B0 + FVector(0, 0, 2.52), B0 + FVector(0, 0, 2.36), .03, .2, 8, Hex(0x2f5d4f));
					Lamp = B0 + FVector(0, 0, 2.3);
				}
				else if (Kind == 1)
				{
					D.Surf = SurfVinyl;
					Box(D, B0.X, B0.Y, B0.Z + .5, .16, .16, 1.0, Hex(0xeef0f1));
					D.Surf = SurfMetalPaint;
					Box(D, B0.X, B0.Y, B0.Z + 1.12, .2, .2, .05, Hex(0x2a2d31));
					Lamp = B0 + FVector(0, 0, 1.04);
				}
				else
				{
					// (the corner pile, left standing tall)
					const FVector Pl = P3(Hd.A1 + .13, Dk.Shape == KeysDocks::ShapeFinger ? Dk.Side * (HW + .13) : C > HeadC ? Hd.C1 + .13 : Hd.C0 - .13, Dz + 2.5);
					KeysLifts::Pile(S, Pl.X, Pl.Y, Pl.Z, .155, Fn.Pile, 0);
					D.Surf = SurfSteel;
					D.AddTube(Pl + FVector(0, 0, -.3), Pl + FVector(-.35, 0, .12), .02, .02, 5, Hex(0x8d9499));
					D.AddTube(Pl + FVector(-.35, 0, .12), Pl + FVector(-.6, 0, -.02), .02, .02, 5, Hex(0x8d9499));
					D.AddTube(Pl + FVector(-.6, 0, .0), Pl + FVector(-.6, 0, -.12), .04, .17, 8, Hex(0x8d9499));
					Lamp = Pl + FVector(-.6, 0, -.17);
				}
				D.Surf = SurfDefault;
				KeysShapes::AddBlob(D, Lamp, FVector(.06, .06, .07), FQuat::Identity, Hex(0xfff3d0), 5, 3);
				O.Lamps.Add(Lamp);
				O.Lamp.AddGlowBlob(Lamp, .2, FLinearColor(1.f, .9f, .66f, .95f));
				O.Lamp.AddGlowBlob(Lamp, 1.1, FLinearColor(1.f, .86f, .6f, .2f));
			}
		}
		// a fish-cleaning table on the corner, its tap and a board
		if (bRoomy && Rnd(34) < .34)
		{
			const FVector At = P3(Hd.A1 - .5, Near - Sg * (bArm ? 2.4 : 1.3), Dz);
			D.Surf = SurfWood;
			for (int32 a = -1; a <= 1; a += 2) for (int32 b = -1; b <= 1; b += 2) Box(D, At.X + a * .24, At.Y + b * .5, At.Z + .45, .07, .07, .9, Hex(Fn.Frame));
			D.Surf = SurfVinyl;
			D.AddRoundBox(At + FVector(0, 0, .93), FVector(.64, 1.25, .06), .02, Hex(0xf4f4f2));
			Box(D, At.X + .3, At.Y, At.Z + 1.06, .03, 1.25, .2, Hex(0xf4f4f2));
			D.Surf = SurfSteel;
			D.AddTube(At + FVector(.22, .4, .96), At + FVector(.22, .4, 1.22), .014, .014, 4, Hex(0xb9bec2));
			D.AddTube(At + FVector(.22, .4, 1.22), At + FVector(.06, .4, 1.2), .014, .014, 4, Hex(0xb9bec2));
			D.Surf = SurfDefault;
			Box(D, At.X - .05, At.Y - .2, At.Z + .97, .3, .45, .02, Hex(0xd9c9a0));
		}
		// a bench facing the water
		if (Dk.Shape <= KeysDocks::ShapeL && Rnd(35) < .3)
		{
			const FVector At = P3(Hd.A0 + .5, Near - Sg * (bArm ? 1.9 : 1.0), Dz);
			D.Surf = SurfWood;
			Box(D, At.X, At.Y, At.Z + .44, .42, 1.3, .05, Hex(Fn.Deck));
			Box(D, At.X - .2, At.Y, At.Z + .72, .05, 1.3, .36, Hex(Fn.Deck));
			for (int32 b = -1; b <= 1; b += 2) Box(D, At.X - .05, At.Y + b * .55, At.Z + .22, .36, .06, .44, Hex(Fn.Frame));
			D.Surf = SurfDefault;
		}
		// lobster traps stacked on an old dock, with their buoys
		if (bRoomy && Dk.Finish == 1 && Rnd(36) < .5)
		{
			const FVector At = P3(Hd.A1 - .7, bArm ? Far + Sg * .5 : Far + Sg * 1.7, Dz);
			D.Surf = SurfWood;
			const int32 NT = 2 + int32(Rnd(37) * 3);
			for (int32 i = 0; i < NT; i++)
			{
				const double Yw = (Rnd(38 + i) - .5) * .3;
				Box(D, At.X + (i % 2) * .05, At.Y, At.Z + .2 + i * .4, .6, .8, .38, Hex(i % 2 ? 0x6b5a45 : 0x7d6a52), Yw);
			}
			D.Surf = SurfDefault;
			static const uint32 Buoy[] = {0xe4572e, 0xf2c14e, 0x2bb3a8, 0xf4f4f2};
			for (int32 i = 0; i < 3; i++) KeysShapes::AddBlob(D, At + FVector(-.55 - i * .05, (i - 1) * .3, .14), FVector(.13, .13, .15), FQuat::Identity, Hex(Buoy[(Dk.Seed + i) % 4]), 6, 4);
		}
		// a power pedestal where the lift's switch is
		if (Dk.Lift.Kind != KeysLifts::LiftNone && Rnd(39) < .6)
		{
			const double A = Dk.Len - .5, C = -Dk.Side * (HW - .16);
			D.Surf = SurfVinyl;
			Box(D, A, C, Dz + .5, .2, .2, 1.0, Hex(0xeef0f1));
			D.Surf = SurfDefault;
			Box(D, A, C, Dz + 1.02, .23, .23, .05, Hex(0x2a2d31));
		}
		// a rack of kayaks in the yard beside the walkway
		if (Rnd(40) < .18)
		{
			const FVector2D Q = Dk.At(-Dk.Back + .5, -Dk.Side * (HW + 1.5));
			const double G = KeysGeo::LandH(Q.X, Q.Y);
			if (G > .5)
			{
				const FVector At = P3(-Dk.Back + .5, -Dk.Side * (HW + 1.5), G);
				static const uint32 Kc[] = {0xe4572e, 0xf2c14e, 0x7ac74f, 0x2f6fd0, 0xff8a3d};
				D.Surf = SurfWood;
				for (int32 a = -1; a <= 1; a += 2)
				{
					Box(D, At.X + a * .9, At.Y, At.Z + .65, .09, .09, 1.3, Hex(Fn.Frame));
					for (int32 k = 0; k < 2; k++) Box(D, At.X + a * .9, At.Y, At.Z + .45 + k * .6, .07, .9, .07, Hex(Fn.Frame));
				}
				D.Surf = SurfVinyl;
				const int32 NK = 1 + int32(Rnd(41) * 2);
				for (int32 k = 0; k < NK; k++)
				{
					const FVector Kc0 = At + FVector(0, 0, .68 + k * .6);
					KeysShapes::AddBlob(D, Kc0, FVector(1.65, .34, .18), FQuat::Identity, Hex(Kc[(Dk.Seed + k * 3) % 5]), 8, 4);
					KeysShapes::AddBlob(D, Kc0 + FVector(-.1, 0, .12), FVector(.42, .2, .08), FQuat::Identity, Hex(0x1c1c1e), 6, 3);
				}
				D.Surf = SurfDefault;
			}
		}
		// a pelican hunched on a pile head, or a gull
		if (Perch.Num())
		{
			const double RB = Rnd(42);
			const FVector At = Perch[int32(Rnd(43) * Perch.Num()) % Perch.Num()];
			if (RB < .16)
			{
				FTownBatch Tb;
				KeysMangroveLife::Pelican(Tb, FVector(At.X, At.Z, At.Y), 2 * PI * Rnd(44), 1.5, Rnd(45) < .35 ? 1 : 0);   // (it works in x, up, z)
				Append(D, Tb.M, FTransform::Identity);
			}
			else if (RB < .36) Gull(D, At, 2 * PI * Rnd(44), 1.5);
		}
		// lights under the water off the head of a few, for the snook
		if (Dk.bUnder)
		{
			// a green lamp on the bottom off the head (a real light at night: AKeysDocks moves a few of them about),
			// and its cable down the nearest pile
			// (off the far side of the head, clear of a boat lying across its face; off the end of a finger)
			const FVector At = bRoomy ? P3(Hd.A1 - .6, Far - Sg * 1.6, -.85) : P3(Hd.A1 + 1.5, 0, -.85);
			const FVector Down = bRoomy ? P3(Hd.A1 - .6, Far - Sg * .15, 0) : P3(Hd.A1 + .15, 0, 0);
			O.Unders.Add(At);
			D.Surf = SurfRubber;
			D.AddTube(Down + FVector(0, 0, Dz - .2), Down + FVector(0, 0, -1.2), .012, .012, 4, Hex(0x1c1c1e));
			D.Surf = SurfDefault;
			O.Lamp.AddGlowBlob(At + FVector(0, 0, .55), .3, FLinearColor(.3f, 1.f, .6f, .45f));
		}

		// ---- the lift, and a boat lying alongside on her lines
		KeysLifts::Build(Dk.Lift, KeysLifts::FParts{&O.S, &O.D, &O.Bt, &O.F});
		if (Dk.bAlong)
		{
			const FBoat& B = Dk.AlongBoat;
			FKeysMesh Bm;
			BuildBoat(Bm, B);
			const FTransform Tb(FQuat(FVector::ZAxisVector, Dk.AlongYaw), FVector(Dk.AlongA, Dk.AlongC, 0));
			Append(O.F, Bm, Tb);
			// bow and stern lines up to the dock, and two fenders between
			const FVector ToDock = Dk.Shape == KeysDocks::ShapeFinger ? FVector(0, Dk.Side, 0) : FVector(-1, 0, 0);
			const FVector Along = Tb.GetRotation().RotateVector(FVector(1, 0, 0));
			const FVector Mid = P3(Dk.AlongA, Dk.AlongC, 0) + ToDock * (B.B / 2);
			O.F.Surf = SurfCloth;
			for (int32 e = -1; e <= 1; e += 2)
			{
				const FVector On = Mid + Along * (e * B.L * .36) + FVector(0, 0, SheerZ(B)) - ToDock * (e > 0 ? B.B * .22 : 0);
				const FVector Ashore = Mid + Along * (e * (B.L * .36 + 1.1)) + ToDock * .6 + FVector(0, 0, Dz + .05);
				O.F.AddTube(On, Ashore, .014, .014, 4, Hex(0xe6e0cf));
			}
			O.F.Surf = SurfVinyl;
			for (int32 e = -1; e <= 1; e += 2) KeysShapes::AddBlob(O.F, Mid + Along * (e * B.L * .18) + ToDock * .1 + FVector(0, 0, SheerZ(B) - .35), FVector(.1, .1, .28), FQuat::Identity, Hex(0xf4f4f2), 6, 4);
			O.F.Surf = SurfDefault;
		}
	}

	// a test script's way of naming a dock: its number, or words joined with + that must all fit it (cradle, elevator,
	// davits, nolift, hung, afloat, emptyup, emptydown, along, fish, hose, under, T, L, finger, platform, pine,
	// weathered, stained, composite), with #n for the n-th such dock; or shore0, shore1 .. for the dock in the middle
	// of the busiest stretches
	int32 ShoreDock(int32 K, int32* OutCount, FVector2D* OutMid);
	int32 FindDock(const FString& Sel)
	{
		const TArray<FDock>& Docks = KeysDocks::Sites();
		if (Sel.StartsWith(TEXT("shore"))) return ShoreDock(FCString::Atoi(*Sel.Mid(5)), nullptr, nullptr);
		if (Sel.IsNumeric()) return Docks.IsValidIndex(FCString::Atoi(*Sel)) ? FCString::Atoi(*Sel) : -1;
		FString Words = Sel, Nth;
		int32 Skip = 0;
		if (Sel.Split(TEXT("#"), &Words, &Nth)) Skip = FCString::Atoi(*Nth);
		TArray<FString> Ws;
		Words.ParseIntoArray(Ws, TEXT("+"));
		static const TCHAR* const ShapeW[4] = {TEXT("T"), TEXT("L"), TEXT("finger"), TEXT("platform")}, * const FinW[4] = {TEXT("pine"), TEXT("weathered"), TEXT("stained"), TEXT("composite")};
		static const TCHAR* const LiftW[4] = {TEXT("nolift"), TEXT("cradle"), TEXT("elevator"), TEXT("davits")}, * const StateW[4] = {TEXT("hung"), TEXT("afloat"), TEXT("emptyup"), TEXT("emptydown")};
		for (int32 i = 0; i < Docks.Num(); i++)
		{
			const FDock& D = Docks[i];
			bool bFits = true;
			for (const FString& Wd : Ws)
				bFits &= Wd == ShapeW[D.Shape] || Wd == FinW[D.Finish] || Wd == LiftW[D.Lift.Kind] || (D.Lift.Kind != KeysLifts::LiftNone && Wd == StateW[D.Lift.State]) || (Wd == TEXT("along") && D.bAlong)
					|| (Wd == TEXT("fish") && D.Person == 1) || (Wd == TEXT("hose") && D.Person == 2) || (Wd == TEXT("under") && D.bUnder);
			if (bFits && Skip-- <= 0) return i;
		}
		return -1;
	}

	// Follows the middle of a cut from P (Ang: radians, 0 east, PI/2 south) and logs its width, its water at low tide
	// and the heading a boat needs, every 5 m - for writing the canal run's script
	FString TraceCut(FVector2D P, double Ang, double Run)
	{
		auto Bank = [](const FVector2D& From, const FVector2D& Dir) { for (double R = .5; R <= 40; R += .5) if (KeysGeo::LandH(From.X + Dir.X * R, From.Y + Dir.Y * R) > 0) return R; return 40.0; };
		double WMin = 1e9, DMin = 1e9, Done = 0;
		const FVector2D Start = P;
		for (; Done <= Run; Done += 5)
		{
			// the heading that keeps the most room ahead
			double Best = -1, BestA = Ang;
			for (int32 k = -4; k <= 4; k++)
			{
				const double A = Ang + k * .13;
				const FVector2D Dr(FMath::Cos(A), FMath::Sin(A)), Ahead = P + Dr * 12, Ac(-Dr.Y, Dr.X);
				if (KeysGeo::LandH(Ahead.X, Ahead.Y) > -.2 || KeysGeo::LandH(P.X + Dr.X * 6, P.Y + Dr.Y * 6) > -.2) continue;
				const double Room = FMath::Min(Bank(Ahead, Ac), Bank(Ahead, -Ac)) - FMath::Abs(k) * .3;
				if (Room > Best) { Best = Room; BestA = A; }
			}
			if (Best < 0) break;
			Ang = BestA;
			const FVector2D Dr(FMath::Cos(Ang), FMath::Sin(Ang)), Ac(-Dr.Y, Dr.X);
			const double L = Bank(P, Ac), R = Bank(P, -Ac);
			P += Ac * ((L - R) / 2);
			const double Wt = KeysGeo::DepthAt(P.X, P.Y) - LowTide;
			UE_LOG(LogTemp, Log, TEXT("KRDocks trace %3.0f m: %.1f, %.1f width %.1f m water %.2f m at low tide heading=%.3f in a dock %d"), Done, P.X, P.Y, L + R, Wt, FMath::Atan2(-Dr.X, -Dr.Y), KeysDocks::InFootprint(P.X, P.Y, 2) ? 1 : 0);
			if (L + R >= 79) break;   // out in open water
			WMin = FMath::Min(WMin, L + R); DMin = FMath::Min(DMin, Wt);
			P += Dr * 5;
		}
		const FString Sum = FString::Printf(TEXT("from %.0f, %.0f: %.0f m, narrowest %.1f m, least water %.2f m at low tide, ends %.0f, %.0f"), Start.X, Start.Y, Done, WMin > 1e8 ? 0 : WMin, DMin > 1e8 ? 0 : DMin, P.X, P.Y);
		UE_LOG(LogTemp, Log, TEXT("KRDocks traced %s"), *Sum);
		return Sum;
	}

	// the K-th busiest 600 m square of shore: the dock nearest the middle of its docks (-1 if there is none)
	int32 ShoreDock(int32 K, int32* OutCount, FVector2D* OutMid)
	{
		const TArray<FDock>& Docks = KeysDocks::Sites();
		struct FSq { int64 Key; int32 N; double X, Z; };
		TArray<FSq> Sq;
		for (const FDock& D : Docks)
		{
			const int64 Key = (int64(FMath::FloorToInt32(D.X0 / TileM)) << 32) ^ int64(uint32(FMath::FloorToInt32(D.Z0 / TileM)));
			FSq* F = Sq.FindByPredicate([Key](const FSq& S) { return S.Key == Key; });
			if (!F) F = &Sq.Add_GetRef({Key, 0, 0, 0});
			F->N++; F->X += D.X0; F->Z += D.Z0;
		}
		Sq.StableSort([](const FSq& A, const FSq& B) { return A.N > B.N; });
		if (!Sq.IsValidIndex(K)) return -1;
		const FVector2D Mid(Sq[K].X / Sq[K].N, Sq[K].Z / Sq[K].N);
		int32 Best = -1;
		double BestD = 1e18;
		for (int32 i = 0; i < Docks.Num(); i++)
		{
			const double Dd = FMath::Square(Docks[i].X0 - Mid.X) + FMath::Square(Docks[i].Z0 - Mid.Y);
			if (Dd < BestD) { BestD = Dd; Best = i; }
		}
		if (OutCount) *OutCount = Sq[K].N;
		if (OutMid) *OutMid = Mid;
		return Best;
	}

	FString TpOf(const FDock& D, double Off)
	{
		const FVector2D P = D.At(D.Box.A1 + Off, (D.Box.C0 + D.Box.C1) / 2);
		return FString::Printf(TEXT("tp=%.0f,%.0f,%.2f"), P.X, P.Y, FMath::Atan2(D.NX, D.NZ));
	}
}

// ---------------------------------------------------------------- the sites
namespace KeysDocks
{
	const TArray<FDock>& Sites()
	{
		if (!GPlanned) PlanAll();
		return GDocks;
	}

	bool Pocket(double X, double Z, double& OutDepth)
	{
		if (!bPocketsReady) return false;
		const TArray<int32>* At = GPocketAt.Find((int64(FMath::FloorToInt(X / 64)) << 32) ^ int64(uint32(FMath::FloorToInt(Z / 64))));
		if (!At) return false;
		double K = 0;
		for (const int32 i : *At)
		{
			const FDock& D = GDocks[i];
			double A, C;
			D.Local(X, Z, A, C);
			if (A < 1) continue;   // (the water's edge itself is left as it is)
			for (int32 p = 1; p < D.Parts.Num(); p++)
			{
				const FRectL& R = D.Parts[p];
				const double DA = FMath::Max3(R.A0 - A, A - R.A1, 0.0), DC = FMath::Max3(R.C0 - C, C - R.C1, 0.0), Dd = FMath::Sqrt(DA * DA + DC * DC);
				if (Dd >= PocketFeather) continue;
				const double T = FMath::Clamp((PocketFeather - Dd) / (PocketFeather - 2), 0.0, 1.0);
				K = FMath::Max(K, T * T * (3 - 2 * T) * FMath::Min(1.0, (A - 1) / 4));
			}
		}
		OutDepth = PocketDepth * K;
		return K > 0;
	}

	bool InFootprint(double X, double Z, double Margin)
	{
		for (const FDock& D : Sites())
		{
			if (FMath::Abs(X - D.X0) > 90 + Margin || FMath::Abs(Z - D.Z0) > 90 + Margin) continue;
			double A, C;
			D.Local(X, Z, A, C);
			if (A < D.Box.A0 - Margin || A > D.Box.A1 + Margin || C < D.Box.C0 - Margin || C > D.Box.C1 + Margin) continue;
			for (const FRectL& R : D.Parts) if (A >= R.A0 - Margin && A <= R.A1 + Margin && C >= R.C0 - Margin && C <= R.C1 + Margin) return true;
		}
		return false;
	}
}

// ---------------------------------------------------------------- the actor
AKeysDocks::AKeysDocks()
{
	PrimaryActorTick.bCanEverTick = true;
	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);
}

AKeysDocks* AKeysDocks::Get(const UObject* WorldContext)
{
	UWorld* W = WorldContext ? WorldContext->GetWorld() : nullptr;
	if (!W) return nullptr;
	TActorIterator<AKeysDocks> It(W);
	return It ? *It : nullptr;
}

UProceduralMeshComponent* AKeysDocks::NewMesh(const FVector& AtUE, USceneComponent* Parent, double DrawDistM, bool bShadow)
{
	UProceduralMeshComponent* C = NewObject<UProceduralMeshComponent>(this);
	C->SetupAttachment(Parent);
	C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	C->bUseAsyncCooking = true;
	C->SetCastShadow(bShadow);
	C->RegisterComponent();
	C->SetRelativeLocation(AtUE);
	if (DrawDistM > 0) C->SetCachedMaxDrawDistance(float(DrawDistM * 100));
	Meshes.Add(C);
	return C;
}

void AKeysDocks::BeginPlay()
{
	Super::BeginPlay();
	KeysGeo::Init();
	const TArray<FDock>& Docks = KeysDocks::Sites();
	if (!Docks.Num())
	{
		UE_LOG(LogTemp, Log, TEXT("KRDocks: no docks on this map (%d houses looked at)"), GHouses);
		return;
	}
	const double T0 = FPlatformTime::Seconds();
	// the tiles and which docks stand in each. Their meshes are made when a tile comes within sight of the camera
	// (StreamTiles): the whole map's docks at once are far too much to hold (14 million triangles with the Lower Keys).
	TMap<int64, int32> TileOf;
	for (int32 i = 0; i < Docks.Num(); i++)
	{
		const FDock& Dk = Docks[i];
		const int32 TX = FMath::FloorToInt32(Dk.X0 / TileM), TZ = FMath::FloorToInt32(Dk.Z0 / TileM);
		int32& K = TileOf.FindOrAdd((int64(TX) << 32) ^ int64(uint32(TZ)), -1);
		if (K < 0)
		{
			K = Tiles.AddDefaulted();
			Tiles[K].X = TX * TileM;
			Tiles[K].Z = TZ * TileM;
		}
		FTile& Tl = Tiles[K];
		Tl.DockIdx.Add(i);
		Tl.CX += Dk.X0; Tl.CZ += Dk.Z0; Tl.Docks++;
		if (Dk.Person) { FPerson& Pn = People.AddDefaulted_GetRef(); Pn.Dock = i; Pn.Ph = i * 1.7; }
	}
	for (FTile& Tl : Tiles) { Tl.CX /= Tl.Docks; Tl.CZ /= Tl.Docks; }
	for (FTile& Tl : Tiles)
		for (int32 i : Tl.DockIdx)
			Tl.R = FMath::Max(Tl.R, FMath::Sqrt(FMath::Square(Docks[i].X0 - Tl.CX) + FMath::Square(Docks[i].Z0 - Tl.CZ)) + Docks[i].Box.A1 + 10);
	if (UMaterialInterface* GM = AKeysWorld::GlowMaterial())
	{
		GlowMID = UMaterialInstanceDynamic::Create(GM, this);
		GlowMID->SetScalarParameterValue(TEXT("Bright"), 3.f);
		SprayMID = UMaterialInstanceDynamic::Create(GM, this);
		SprayMID->SetScalarParameterValue(TEXT("Bright"), 1.1f);
	}
	// a few lamps that really light the deck, moved to whichever docks are nearest at night
	// (the first six are the dock lamps; the last three the green lamps under the water)
	for (int32 i = 0; i < 9; i++)
	{
		UPointLightComponent* Lt = NewObject<UPointLightComponent>(this);
		Lt->SetupAttachment(Root);
		Lt->RegisterComponent();
		Lt->SetIntensityUnits(ELightUnits::Lumens);
		Lt->SetIntensity(i < 6 ? 750.f : 2200.f);
		Lt->SetLightColor(i < 6 ? FLinearColor(1.f, .86f, .62f) : FLinearColor(.2f, 1.f, .5f));
		Lt->SetAttenuationRadius(i < 6 ? 1500.f : 1100.f);
		Lt->SetCastShadows(false);
		Lt->SetVisibility(false);
		Lights.Add(Lt);
	}
	// the one or two people about
	for (int32 i = 0; i < 2; i++)
	{
		UProceduralMeshComponent* Pm = NewMesh(FVector::ZeroVector, Root, 400, true);
		PersonMesh.Add(Pm);
		UProceduralMeshComponent* Sp = NewMesh(FVector::ZeroVector, Pm, 400, false);
		SprayMesh.Add(Sp);
	}
	for (int32 i = 0; i < People.Num(); i++)
	{
		FKeysHumanLook Lk;
		const int32 K = People[i].Dock;
		static const uint32 Shirts[] = {0xf2f2f0, 0x7fc4d8, 0xf2c14e, 0xe4572e, 0x8fb996, 0xd9dde2};
		static const uint32 Skins[] = {0xd9a27f, 0xb9805c, 0x8d5a3c, 0xe2b596};
		Lk.Shirt = Hex(Shirts[K % 6]); Lk.Bottom = Hex(K % 2 ? 0x3a4250 : 0xc8b58c); Lk.bShortsLong = true; Lk.Skin = Hex(Skins[K % 4]);
		Lk.bCap = K % 3 != 0; Lk.CapColor = Hex(K % 2 ? 0x1d3557 : 0xf2f2f0); Lk.bGlasses = K % 2 == 0;
		People[i].Hm.Init(Lk, false);
		People[i].Hm.bHoldHead = true;
	}
	bBuilt = true;
	// the tiles round the start stand before the first frame
	FVector2D Start(0, 0);
	if (APawn* P0 = UGameplayStatics::GetPlayerPawn(this, 0)) Start = FVector2D(P0->GetActorLocation().X / 100, P0->GetActorLocation().Y / 100);
	else if (KeysGeo::Marinas().Num()) Start = FVector2D(KeysGeo::Marinas()[0].SX, KeysGeo::Marinas()[0].SZ);
	StreamTiles(Start, 1000);
	UE_LOG(LogTemp, Log, TEXT("KRDocks built: %d docks in %d tiles, made within 3 km of the camera; %d tiles round the start now, %d meshes, %d tris (structure %d, hung boats %d, fittings %d, afloat %d, lights %d), %d people, %.2f s"),
		Docks.Num(), Tiles.Num(), MadeTiles, Meshes.Num(), Tris[0] + Tris[1] + Tris[2] + Tris[3] + Tris[4], Tris[0], Tris[1], Tris[2], Tris[3], Tris[4], People.Num(), FPlatformTime::Seconds() - T0);
	LogSites();
	Audit();
}

// one tile's meshes: the docks themselves from across the harbour, the boats from a kilometre, the fittings only close to
void AKeysDocks::MakeTile(FTile& Tl)
{
	const TArray<FDock>& Docks = KeysDocks::Sites();
	FKeysMesh MS, MBt, MD, MF, MLamp;
	Tl.LampAt.Reset(); Tl.UnderAt.Reset();
	for (int32 i : Tl.DockIdx)
	{
		const FDock& Dk = Docks[i];
		FDockMesh Dm;
		BuildDock(Dk, Dm);
		const FTransform Tf(FQuat(FVector::ZAxisVector, FMath::Atan2(Dk.NZ, Dk.NX)), FVector(Dk.X0 - Tl.X, Dk.Z0 - Tl.Z, 0));
		Append(MS, Dm.S, Tf);
		Append(MBt, Dm.Bt, Tf);
		Append(MD, Dm.D, Tf);
		Append(MF, Dm.F, Tf);
		Append(MLamp, Dm.Lamp, Tf);
		for (const FVector& Lp : Dm.Lamps) { const FVector2D Q = Dk.At(Lp.X, Lp.Y); Tl.LampAt.Add(FVector(Q.X, Q.Y, Lp.Z)); }
		for (const FVector& Lp : Dm.Unders) { const FVector2D Q = Dk.At(Lp.X, Lp.Y); Tl.UnderAt.Add(FVector(Q.X, Q.Y, Lp.Z)); }
	}
	UMaterialInterface* Mat = AKeysWorld::VertexColorMaterial();
	const double Sea = KeysOcean::SeaLevel();
	const FVector At = KeysGeo::ToUE(Tl.X, 0, Tl.Z);
	const int32 M0 = Meshes.Num();
	if (MS.T.Num()) MS.Commit(NewMesh(At, Root, 2600, true), 0, Mat);
	if (MBt.T.Num()) MBt.Commit(NewMesh(At, Root, 1100, true), 0, Mat);
	if (MD.T.Num()) MD.Commit(NewMesh(At, Root, 380, false), 0, Mat);
	Tl.Float = NewMesh(At + FVector(0, 0, Sea * 100), Root, 1100, true);
	Tl.FloatZ = Sea;
	if (MF.T.Num()) MF.Commit(Tl.Float, 0, Mat);
	if (GlowMID && MLamp.T.Num())
	{
		Tl.Lamps = NewMesh(At, Root, 2600, false);
		MLamp.Commit(Tl.Lamps, 0, GlowMID);
		Tl.Lamps->SetVisibility(bNight);
	}
	for (int32 k = M0; k < Meshes.Num(); k++) Tl.Parts.Add(Meshes[k]);
	Tl.TriN[0] = MS.T.Num() / 3; Tl.TriN[1] = MBt.T.Num() / 3; Tl.TriN[2] = MD.T.Num() / 3; Tl.TriN[3] = MF.T.Num() / 3; Tl.TriN[4] = MLamp.T.Num() / 3;
	for (int32 k = 0; k < 5; k++) Tris[k] += Tl.TriN[k];
	int32 BadVerts = 0;
	for (const FKeysMesh* Mh : {&MS, &MBt, &MD, &MF}) for (const FVector& Vv : Mh->V) BadVerts += Vv.ContainsNaN();
	if (BadVerts) UE_LOG(LogTemp, Warning, TEXT("KRDocks: %d broken vertices"), BadVerts);
	Tl.bMade = true;
	MadeTiles++;
}

void AKeysDocks::DropTile(FTile& Tl)
{
	for (UProceduralMeshComponent* C : Tl.Parts) if (C) { Meshes.Remove(C); C->DestroyComponent(); }
	Tl.Parts.Reset();
	Tl.Float = Tl.Lamps = nullptr;
	Tl.LampAt.Reset(); Tl.UnderAt.Reset();
	for (int32 k = 0; k < 5; k++) { Tris[k] -= Tl.TriN[k]; Tl.TriN[k] = 0; }
	Tl.bMade = false;
	MadeTiles--;
}

// makes the tiles within sight (the docks are drawn to 2.6 km), nearest first, and takes down the ones left behind.
// SyncWithin: every tile that near is made now (the start, and a teleport's first look); otherwise one a call.
void AKeysDocks::StreamTiles(const FVector2D& Eye, double SyncWithin)
{
	constexpr double In = 3000, Out = 3800;
	int32 Best = -1;
	double BestD = 1e18;
	for (int32 k = 0; k < Tiles.Num(); k++)
	{
		FTile& Tl = Tiles[k];
		const double D = FMath::Sqrt(FMath::Square(Tl.CX - Eye.X) + FMath::Square(Tl.CZ - Eye.Y)) - Tl.R;
		if (Tl.bMade) { if (D > Out) DropTile(Tl); continue; }
		if (D < SyncWithin) MakeTile(Tl);
		else if (D < In && D < BestD) { BestD = D; Best = k; }
	}
	if (Best >= 0 && SyncWithin <= 0) MakeTile(Tiles[Best]);
}

void AKeysDocks::LogSites() const
{
	const TArray<FDock>& Docks = KeysDocks::Sites();
	FString Why;
	for (int32 i = 1; i < WhyCount; i++) if (GWhy[i]) Why += FString::Printf(TEXT("%s%s %d"), Why.IsEmpty() ? TEXT("") : TEXT(", "), WhyName[i], GWhy[i]);
	UE_LOG(LogTemp, Log, TEXT("KRDocks survey: %d houses, %d docks; no dock because: %s"), GHouses, Docks.Num(), *Why);
	UE_LOG(LogTemp, Log, TEXT("KRDocks survey: a metre of water at low tide lies <10 m off %d shore houses, 10-20 m %d, 20-30 m %d, 30-40 m %d, 40-50 m %d, 50-80 m %d, further %d"),
		GReach[0], GReach[1], GReach[2], GReach[3], GReach[4], GReach[5], GReach[6]);
	UE_LOG(LogTemp, Log, TEXT("KRDocks survey: shore houses with water within 45 m at low tide - %.1f m deep: %d, %.1f m: %d, %.1f m: %d, %.1f m: %d"),
		ReachDepth[0], GReachAt[0], ReachDepth[1], GReachAt[1], ReachDepth[2], GReachAt[2], ReachDepth[3], GReachAt[3]);
	TMap<FString, int32> PerIsland;
	int32 NShape[KeysDocks::ShapeCount] = {}, NFin[4] = {}, NLift[4] = {}, NState[4] = {}, NAlong = 0, NPerson[3] = {};
	double LenMin = 1e9, LenMax = 0, LenSum = 0;
	for (const FDock& D : Docks)
	{
		PerIsland.FindOrAdd(D.Island)++;
		NShape[D.Shape]++; NFin[D.Finish]++; NLift[D.Lift.Kind]++; NAlong += D.bAlong; NPerson[D.Person]++;
		if (D.Lift.Kind != KeysLifts::LiftNone) NState[D.Lift.State]++;
		LenMin = FMath::Min(LenMin, D.Len); LenMax = FMath::Max(LenMax, D.Len); LenSum += D.Len;
	}
	PerIsland.KeySort([](const FString& A, const FString& B) { return A < B; });
	FString Isl;
	for (const TPair<FString, int32>& Pr : PerIsland) Isl += FString::Printf(TEXT("%s%s %d"), Isl.IsEmpty() ? TEXT("") : TEXT(", "), *Pr.Key, Pr.Value);
	UE_LOG(LogTemp, Log, TEXT("KRDocks per island: %s"), *Isl);
	UE_LOG(LogTemp, Log, TEXT("KRDocks kinds: T %d, L %d, finger %d, platform %d | new pine %d, weathered %d, stained %d, composite %d | cradle %d, elevator %d, davits %d, no lift %d | hung %d, afloat %d, empty up %d, empty down %d | boats alongside %d | fishing %d, hosing %d | walkways %.0f..%.0f m, mean %.1f | checksum %08x"),
		NShape[0], NShape[1], NShape[2], NShape[3], NFin[0], NFin[1], NFin[2], NFin[3], NLift[1], NLift[2], NLift[3], NLift[0], NState[0], NState[1], NState[2], NState[3], NAlong, NPerson[1], NPerson[2],
		LenMin, LenMax, LenSum / FMath::Max(1, Docks.Num()), GChecksum);
	for (int32 i = 0; i < Docks.Num(); i++)
	{
		const FDock& D = Docks[i];
		const FBoat* Bt = D.Lift.HasBoat() ? &D.Lift.Boat : D.bAlong ? &D.AlongBoat : nullptr;
		UE_LOG(LogTemp, Log, TEXT("KRDock %d %s | %s %s %.0f m | %s %s%s%s | %s %s (%s) | head %.2f m at low tide | %s"), i, *D.Island, ShapeName[D.Shape], FinishName[D.Finish], D.Len,
			LiftName[D.Lift.Kind], D.Lift.Kind ? StateName[D.Lift.State] : TEXT(""), D.bAlong ? TEXT(" + alongside") : TEXT(""), D.Person == 1 ? TEXT(" + fishing") : D.Person == 2 ? TEXT(" + hosing") : D.bUnder ? TEXT(" + underwater lights") : TEXT(""),
			Bt ? *Bt->Name : TEXT("-"), Bt ? *Bt->Brand : TEXT("-"), !Bt ? TEXT("-") : Bt->Kind == BoatSkiff ? TEXT("skiff") : Bt->Kind == BoatBay ? TEXT("bay boat") : TEXT("centre console"), D.HeadDepth, *TpOf(D, 22));
	}
	// the busiest stretches of shore, and where to lie off them
	for (int32 k = 0; k < 5; k++)
	{
		int32 Cnt = 0;
		FVector2D Mid;
		const int32 Best = ShoreDock(k, &Cnt, &Mid);
		if (Best >= 0) UE_LOG(LogTemp, Log, TEXT("KRDocks shore %d: %d docks round %.0f, %.0f (%s) - dock %d, %s"), k, Cnt, Mid.X, Mid.Y, *Docks[Best].Island, Best, *TpOf(Docks[Best], 14));
	}
}

// Counts the docks that stand on anything they were told to keep off, measured against the things themselves (not
// against the margins the planner used): every half metre of every dock's walkway, head, lift and berth.
void AKeysDocks::Audit() const
{
	using namespace KeysGeo;
	const TArray<FDock>& Docks = KeysDocks::Sites();
	enum { AChannel, ACreek, AMarina, APier, AEstate, APlat, ACove, AMangrove, ATown, ADock, AShallow, ANoHouse, ACount };
	int32 N[ACount] = {};
	double LeastHead = 1e9, LeastGap = 1e9, FarHouse = 0;
	for (int32 i = 0; i < Docks.Num(); i++)
	{
		const FDock& D = Docks[i];
		bool Hit[ACount] = {};
		for (const FRectL& R : D.Parts)
			for (double A = R.A0; A <= R.A1 + .001; A += .5)
				for (double C = R.C0; C <= R.C1 + .001; C += .5)
				{
					const FVector2D Q = D.At(A, C);
					const double X = Q.X, Z = Q.Y;
					Hit[AChannel] |= InMarkedChannel(X, Z);
					Hit[ACreek] |= CreekDist(X, Z) < 0;
					for (const FMarina& M : Marinas()) { const double Rr = (Z - M.SZ) * M.Dir; Hit[AMarina] |= FMath::Abs(X - M.SX) < 34 && Rr > -2 && Rr < 78; }
					for (const KeysTown::FDeckBox& Bx : KeysTown::DeckBoxes()) Hit[AMarina] |= FMath::Abs(X - Bx.CX) <= Bx.W / 2 + .5 && FMath::Abs(Z - Bx.CZ) <= Bx.D / 2 + .5;
					for (const KeysTown::FPier& P : KeysTown::Piers())
					{
						const double DX = X - P.X0, DZ = Z - P.Z0, PA = DX * P.NX + DZ * P.NZ, PC = -DX * P.NZ + DZ * P.NX;
						if (PA < -2 || PA > P.Len + KeysTown::PierHeadD / 2) continue;
						Hit[APier] |= FMath::Abs(PC) <= (PA > P.Len - KeysTown::PierHeadD / 2 ? KeysTown::PierHeadW / 2 : KeysTown::PierW / 2);
					}
					for (const KeysTown::FEstateSite& E : KeysTown::WaterfrontEstates())
					{
						const double DX = X - E.X, DZ = Z - E.Z, EA = DX * E.NX + DZ * E.NZ, EC = -DX * E.NZ + DZ * E.NX, Wd = 14 + E.Style * 4;
						Hit[AEstate] |= (EA > 12 && EA < E.DockLen && FMath::Abs(EC) <= 1.1) || (EA > E.DockLen - 8.2 && EA < E.DockLen + .2 && EC > 2.2 && EC < 6.2) || (EA > -6 && EA < 13 && FMath::Abs(EC) <= Wd / 2 + 1.5);
					}
					for (const KeysTown::FExtraDock& E : KeysTown::ExtraDocks())
					{
						const double EA = (X - E.X0) * E.SX + (Z - E.Z0) * E.SZ, EC = FMath::Abs((X - E.X0) * -E.SZ + (Z - E.Z0) * E.SX);
						Hit[APlat] |= (EA >= 0 && EA <= E.Len && EC <= E.HalfW) || (E.THead > 0 && EA >= E.Len - 5 && EA <= E.Len && EC <= E.THead / 2);
					}
					Hit[APlat] |= AKeysEstates::PlatParcelAt(X, Z) >= 0;
					Hit[ACove] |= KeysYards::InFootprint(X, Z, 0);
					Hit[AMangrove] |= OnMangrove(X, Z);
					Hit[ATown] |= KeysTown::InFootprint(X, Z, 0);
				}
		for (int32 j = i + 1; j < Docks.Num(); j++)
		{
			const FDock& O = Docks[j];
			if (FMath::Abs(O.X0 - D.X0) > 160 || FMath::Abs(O.Z0 - D.Z0) > 160) continue;
			double Gap = 1e9;
			for (const FRectL& RA : D.Parts) for (const FRectL& RB : O.Parts) Gap = FMath::Min(Gap, RectGap(D, RA, O, RB));
			LeastGap = FMath::Min(LeastGap, Gap);
			if (Gap <= 0) { Hit[ADock] = true; N[ADock]++; }
		}
		// the head itself, sounded again
		const FRectL& Hd = D.Parts[1];
		const FVector2D HP = D.At((Hd.A0 + Hd.A1) / 2, (Hd.C0 + Hd.C1) / 2);
		const double HeadWater = DepthAt(HP.X, HP.Y) - LowTide;
		LeastHead = FMath::Min(LeastHead, HeadWater);
		Hit[AShallow] = HeadWater < NeedDepth || LandH(HP.X, HP.Y) > 0;
		const double ToHouse = FMath::Sqrt(FMath::Square(D.HX - D.X0) + FMath::Square(D.HZ - D.Z0));
		FarHouse = FMath::Max(FarHouse, ToHouse);
		Hit[ANoHouse] = ToHouse > 60 || LandH(D.HX, D.HZ) < 1.15;
		for (int32 k = 0; k < ACount; k++) if (k != ADock) N[k] += Hit[k];
	}
	UE_LOG(LogTemp, Log, TEXT("KRDocks audit: %d docks; standing on - marked channel %d, creek %d, marina %d, pier %d, estate %d, plat lot %d, Cowfish Cove %d, mangrove %d, town %d, another dock %d; heads with under %.1f m at low tide %d (least %.2f m); docks with no house behind %d (furthest house %.0f m); the two nearest docks are %.1f m apart"),
		Docks.Num(), N[AChannel], N[ACreek], N[AMarina], N[APier], N[AEstate], N[APlat], N[ACove], N[AMangrove], N[ATown], N[ADock], NeedDepth, N[AShallow], LeastHead > 1e8 ? 0 : LeastHead, N[ANoHouse], FarHouse, LeastGap > 1e8 ? 0 : LeastGap);
}

// ---------------------------------------------------------------- the people
void AKeysDocks::BuildPerson(const FPerson& P, int32 Slot, double T)
{
	const FDock& Dk = KeysDocks::Sites()[P.Dock];
	UProceduralMeshComponent* Pm = PersonMesh[Slot];
	Pm->SetWorldLocationAndRotation(KeysGeo::ToUE(Dk.X0, 0, Dk.Z0), FRotator(0, FMath::RadiansToDegrees(FMath::Atan2(Dk.NZ, Dk.NX)), 0));
	FKeysHuman Hm = P.Hm;
	FKeysMesh M, Sp;
	const double Ph = T + P.Ph, Y0 = Hm.HipY, HW = Dk.W / 2, Sea = KeysOcean::SeaLevel();
	const FRectL& Hd = Dk.Parts[1];
	if (Dk.Person == 1)
	{
		// fishing off the head: the rod held out over the water, a float riding the ripples, a look round now and then
		Hm.Pos = P3(Hd.A1 - .4, (Hd.C0 + Hd.C1) / 2 + (Dk.Lift.Kind == KeysLifts::LiftNone ? 0 : -Dk.Side * FMath::Min(1.2, (Hd.C1 - Hd.C0) * .25)), Dk.DeckZ);
		Hm.Yaw = .25 * FMath::Sin(Ph * .07);
		const double Lift = .04 * FMath::Sin(Ph * .9) + (FMath::Fmod(Ph, 17.0) < .8 ? .12 : 0);
		const FVector R = FKeysHuman::J(.17, Y0 + .14 + Lift, .3), L = FKeysHuman::J(.04, Y0 + .26 + Lift, .5);
		Hm.Pose(L, R);
		Hm.Pitch = -.04; Hm.HeadNod = .12; Hm.HeadYaw = .3 * FMath::Sin(Ph * .23);
		Hm.Build(M);
		const FTransform Tf = Hm.ToBoat();
		const FVector Rw = Tf.TransformPosition(R), Lw = Tf.TransformPosition(L);
		const FVector Dir = (Lw - Rw).GetSafeNormal(), Butt = Rw - Dir * .22, Tip = Butt + Dir * 2.3 + FVector(0, 0, .12 + Lift);
		M.Surf = SurfDefault;
		M.AddTube(Butt, Rw + Dir * .3, .016, .013, 5, Hex(0x1c1c1e));
		M.AddTube(Rw + Dir * .3, Tip, .011, .004, 5, Hex(0x23466e));
		M.Surf = SurfSteel;
		M.AddBox(Rw + Dir * .1 - FVector(0, 0, .06), FVector(.08, .05, .08), Hex(0xb9bec2), Tf.GetRotation());
		M.Surf = SurfDefault;
		const FVector Bob = P3(Hm.Pos.X + 6.5 + .4 * FMath::Sin(Ph * .11), Hm.Pos.Y + .8 * FMath::Sin(Ph * .05), Sea + .04 + .02 * FMath::Sin(Ph * 2.1));
		M.AddTube(Tip, Bob, .005, .005, 3, Hex(0xe8ebee));
		KeysShapes::AddBlob(M, Bob, FVector(.05, .05, .07), FQuat::Identity, Hex(0xe4572e), 5, 3);
	}
	else
	{
		// hosing the boat down on her lift: a slow sweep along the hull, the spray breaking on the gelcoat
		const KeysLifts::FLift& Lf = Dk.Lift;
		const double Sw = FMath::Sin(Ph * .7) * .45, Walk = FMath::Sin(Ph * .11) * FMath::Min(1.6, Lf.Boat.L * .2);
		Hm.Pos = P3(Lf.X + Walk, Dk.Side * (HW - .42), Dk.DeckZ);
		Hm.Yaw = Dk.Side * PI / 2;
		const FVector R = FKeysHuman::J(.2, Y0 + .12, .22), L = FKeysHuman::J(.02 + Sw * .18, Y0 + .24, .5);
		Hm.Pose(L, R);
		Hm.Pitch = -.05; Hm.HeadNod = .08; Hm.HeadYaw = Sw * .5;
		Hm.Build(M);
		const FTransform Tf = Hm.ToBoat();
		const FVector Lw = Tf.TransformPosition(L), Rw = Tf.TransformPosition(R);
		const FVector Fwd = Tf.GetRotation().RotateVector(FVector(1, 0, 0)), Sd = Tf.GetRotation().RotateVector(FVector(0, 1, 0));
		const FVector Aim = (Fwd + Sd * Sw + FVector(0, 0, .1 + .1 * FMath::Sin(Ph * .37))).GetSafeNormal();
		const FVector Tip = Lw + Aim * .2;
		M.Surf = SurfSteel;
		M.AddTube(Lw - Aim * .08, Tip, .016, .012, 5, Hex(0xb9bec2));
		// the hose: down from her hand to the deck and away along the walkway to the tap
		M.Surf = SurfRubber;
		const FVector H0 = Rw, H1 = P3(Hm.Pos.X - .1, Hm.Pos.Y - Dk.Side * .25, Dk.DeckZ + .02), H2 = P3(FMath::Max(1.0, Dk.Len - 9), -Dk.Side * (HW - .2), Dk.DeckZ + .02);
		M.AddTube(Lw - Aim * .08, H0, .012, .012, 4, Hex(0x2f7d4f));
		M.AddTube(H0, H1, .012, .012, 4, Hex(0x2f7d4f));
		M.AddTube(H1, H2, .012, .012, 4, Hex(0x2f7d4f));
		M.AddTube(H2, H2 + FVector(0, 0, .6), .02, .02, 5, Hex(0x8d9499));
		M.Surf = SurfDefault;
		const double Reach = FMath::Max(.6, FMath::Abs(Lf.Y) - Lf.Boat.B / 2 - FMath::Abs(Hm.Pos.Y) - .3), Fl = .8 + .2 * FMath::Sin(T * 23);
		const FVector HitAt = Tip + Aim * Reach, Up = FVector::CrossProduct(Aim, Sd).GetSafeNormal();
		const FLinearColor Wc(1.f, 1.f, 1.f);
		Sp.AddGlowFan(Tip, HitAt - Sd * .22, HitAt + Sd * .22, Wc, .8 * Fl, .12);
		Sp.AddGlowFan(Tip, HitAt - Up * .1, HitAt + Up * .1, Wc, .8 * Fl, .12);
		Sp.AddGlowBlob(HitAt, .34, FLinearColor(1.f, 1.f, 1.f, float(.3 * Fl)));
	}
	M.Commit(Pm, 0, AKeysWorld::VertexColorMaterial());
	if (SprayMID)
	{
		if (Sp.T.Num()) Sp.Commit(SprayMesh[Slot], 0, SprayMID);
		else SprayMesh[Slot]->ClearAllMeshSections();
	}
	Pm->SetVisibility(true, true);
}

void AKeysDocks::TickPeople(double T)
{
	// the two nearest of them within sight are drawn; the rest are indoors
	const TArray<FDock>& Docks = KeysDocks::Sites();
	FVector Cam = FVector::ZeroVector;
	if (APlayerCameraManager* CM = UGameplayStatics::GetPlayerCameraManager(this, 0)) Cam = CM->GetCameraLocation() / 100.0;
	int32 Pick[2] = {-1, -1};
	double PickD[2] = {260.0 * 260.0, 260.0 * 260.0};
	for (int32 i = 0; i < People.Num(); i++)
	{
		const FDock& Dk = Docks[People[i].Dock];
		const FVector2D Hd = Dk.At(Dk.Len, 0);
		const double D2 = FMath::Square(Hd.X - Cam.X) + FMath::Square(Hd.Y - Cam.Y);
		if (D2 < PickD[0]) { Pick[1] = Pick[0]; PickD[1] = PickD[0]; Pick[0] = i; PickD[0] = D2; }
		else if (D2 < PickD[1]) { Pick[1] = i; PickD[1] = D2; }
	}
	for (int32 s = 0; s < 2; s++)
	{
		if (Pick[s] < 0) { if (PersonAt[s] >= 0) { PersonMesh[s]->SetVisibility(false, true); PersonAt[s] = -1; } continue; }
		PersonAt[s] = Pick[s];
		BuildPerson(People[Pick[s]], s, T);
	}
}

// ---------------------------------------------------------------- the frame
void AKeysDocks::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!bBuilt) return;
	const double Dt = FMath::Min(double(DeltaSeconds), .1);
	if (APlayerCameraManager* CM0 = UGameplayStatics::GetPlayerCameraManager(this, 0)) StreamTiles(FVector2D(CM0->GetCameraLocation().X / 100, CM0->GetCameraLocation().Y / 100), 0);
	SlowT -= Dt;
	if (SlowT <= 0)
	{
		SlowT = .5;
		FVector Cam = FVector::ZeroVector;
		if (APlayerCameraManager* CM = UGameplayStatics::GetPlayerCameraManager(this, 0)) Cam = CM->GetCameraLocation() / 100.0;   // (x, z, up) in metres
		// what floats rides the tide, in the tiles within 600 m; the rest wait as they were
		const double Sea = KeysOcean::SeaLevel();
		AwakeTiles = 0;
		for (FTile& Tl : Tiles)
		{
			if (FMath::Sqrt(FMath::Square(Tl.CX - Cam.X) + FMath::Square(Tl.CZ - Cam.Y)) - Tl.R > 600) continue;
			AwakeTiles++;
			if (FMath::Abs(Sea - Tl.FloatZ) > .003 && Tl.Float)
			{
				Tl.FloatZ = Sea;
				Tl.Float->SetRelativeLocation(KeysGeo::ToUE(Tl.X, Sea, Tl.Z));
			}
		}
		bAwake = AwakeTiles > 0;
		// the lamps come on at dusk (one switch for the whole shore), and the six real lights go to the nearest docks
		const bool bDark = AKeysWorld::Dusk(this) > .3;
		if (bDark != bNight)
		{
			bNight = bDark;
			for (FTile& Tl : Tiles) if (Tl.Lamps) Tl.Lamps->SetVisibility(bDark);
			if (!bDark) for (UPointLightComponent* Lt : Lights) if (Lt) Lt->SetVisibility(false);
		}
		if (bNight && bAwake)
		{
			TArray<TPair<double, FVector>> Near, NearU;
			for (const FTile& Tl : Tiles)
			{
				if (FMath::Sqrt(FMath::Square(Tl.CX - Cam.X) + FMath::Square(Tl.CZ - Cam.Y)) - Tl.R > 350) continue;
				for (const FVector& Lp : Tl.LampAt) { const double D2 = FMath::Square(Lp.X - Cam.X) + FMath::Square(Lp.Y - Cam.Y); if (D2 < 350.0 * 350.0) Near.Add({D2, Lp}); }
				for (const FVector& Lp : Tl.UnderAt) { const double D2 = FMath::Square(Lp.X - Cam.X) + FMath::Square(Lp.Y - Cam.Y); if (D2 < 350.0 * 350.0) NearU.Add({D2, Lp}); }
			}
			Near.Sort([](const TPair<double, FVector>& A, const TPair<double, FVector>& B) { return A.Key < B.Key; });
			NearU.Sort([](const TPair<double, FVector>& A, const TPair<double, FVector>& B) { return A.Key < B.Key; });
			for (int32 i = 0; i < Lights.Num(); i++)
			{
				if (!Lights[i]) continue;
				const TArray<TPair<double, FVector>>& From = i < 6 ? Near : NearU;
				const int32 k = i < 6 ? i : i - 6;
				Lights[i]->SetVisibility(k < From.Num());
				if (k < From.Num()) Lights[i]->SetWorldLocation(KeysGeo::ToUE(From[k].Value.X, From[k].Value.Z - (i < 6 ? .25 : 0), From[k].Value.Y));
			}
		}
		else if (bNight) for (UPointLightComponent* Lt : Lights) if (Lt) Lt->SetVisibility(false);
	}
	// nothing else happens while nobody is within 600 m of a dock
	if (!bAwake) { for (int32 s = 0; s < 2; s++) if (PersonAt[s] >= 0) { PersonMesh[s]->SetVisibility(false, true); PersonAt[s] = -1; } return; }
	PeopleT -= Dt;
	if (People.Num() && PeopleT <= 0) { PeopleT = .05; TickPeople(GetWorld()->GetTimeSeconds()); }
}

FString AKeysDocks::DebugCmd(const FString& Cmd)
{
	if (!bBuilt) return TEXT("no docks on this map");
	const TArray<FDock>& Docks = KeysDocks::Sites();
	if (Cmd == TEXT("audit")) { Audit(); return TEXT("audited"); }
	if (Cmd == TEXT("pos"))
	{
		// where the boat is, what is under her, and whether she is inside a dock
		APawn* Boat = UGameplayStatics::GetPlayerPawn(this, 0);
		if (!Boat) return TEXT("no boat");
		const FVector L = Boat->GetActorLocation() / 100.0;
		return FString::Printf(TEXT("boat at %.1f, %.1f water %.2f m shore %.0f m in a dock %d"), L.X, L.Y, KeysGeo::DepthAt(L.X, L.Y) + KeysGeo::TideRise(), KeysGeo::ShoreInfo(L.X, L.Y).D, KeysDocks::InFootprint(L.X, L.Y, 1) ? 1 : 0);
	}
	if (Cmd == TEXT("tracks"))
	{
		// for the idle test: the longest straight runs 25 m off the water's edge past a dock that keep 0.7 m at low tide
		TArray<TPair<double, FString>> Runs;
		for (int32 i = 0; i < Docks.Num(); i++)
		{
			const FDock& Dk = Docks[i];
			auto Good = [&Dk](double C) { const FVector2D Q = Dk.At(25, C); const double Sd = KeysGeo::ShoreInfo(Q.X, Q.Y).D; return KeysGeo::LandH(Q.X, Q.Y) < -.3 && KeysGeo::DepthAt(Q.X, Q.Y) - LowTide >= .7 && Sd > 8 && Sd < 60; };
			double C0 = 0, C1 = 0;
			if (!Good(0)) continue;
			while (C0 > -400 && Good(C0 - 5)) C0 -= 5;
			while (C1 < 400 && Good(C1 + 5)) C1 += 5;
			int32 Crossed = 0;
			for (const FDock& O : Docks) { bool bIn = false; for (double C = C0; C <= C1 && !bIn; C += 1) { const FVector2D Q = Dk.At(25, C); bIn = KeysDocks::InFootprint(Q.X, Q.Y, 0) && FMath::Abs(O.X0 - Q.X) < 90 && FMath::Abs(O.Z0 - Q.Y) < 90 && O.Box.A1 > 0; if (bIn) { double A, Cc; O.Local(Q.X, Q.Y, A, Cc); bIn = A >= O.Box.A0 && A <= O.Box.A1 && Cc >= O.Box.C0 && Cc <= O.Box.C1; } } Crossed += bIn; }
			const FVector2D S0 = Dk.At(25, C0 + 3);
			// heading along +C: forward is (-sin H, -cos H)
			Runs.Add({C1 - C0, FString::Printf(TEXT("dock %d (%s): %.0f m, through %d docks, tp=%.0f,%.0f,%.2f"), i, *Dk.Island, C1 - C0, Crossed, S0.X, S0.Y, FMath::Atan2(Dk.NZ, -Dk.NX))});
		}
		Runs.Sort([](const TPair<double, FString>& A, const TPair<double, FString>& B) { return A.Key > B.Key; });
		for (int32 i = 0; i < Runs.Num() && i < 8; i++) UE_LOG(LogTemp, Log, TEXT("KRDocks track %s"), *Runs[i].Value);
		return FString::Printf(TEXT("%d tracks"), Runs.Num());
	}
	if (Cmd.StartsWith(TEXT("trace=")))
	{
		// trace=x,z,deg[,metres]: follow the middle of a cut from there (deg: 0 east, 90 south)
		TArray<FString> Pp;
		Cmd.Mid(6).ParseIntoArray(Pp, TEXT(","));
		if (Pp.Num() < 3) return TEXT("trace=x,z,deg[,metres]");
		return TraceCut(FVector2D(FCString::Atod(*Pp[0]), FCString::Atod(*Pp[1])), FMath::DegreesToRadians(FCString::Atod(*Pp[2])), Pp.Num() >= 4 ? FCString::Atod(*Pp[3]) : 300);
	}
	if (Cmd == TEXT("canals"))
	{
		// water a boat could idle down with a bank close on both hands (not the mangrove creeks, which have their own
		// pilot): 0.6 m at low tide, the banks 14 to 45 m apart. The three biggest stretches are traced both ways.
		const double St = 8;
		struct FCell { double X, Z, Across; };
		TMap<int64, TArray<FCell>> Bins;
		int32 Cells = 0;
		for (double Z = KeysGeo::WBZ0; Z < KeysGeo::WBZ1; Z += St)
			for (double X = KeysGeo::WBX0; X < KeysGeo::WBX1; X += St)
			{
				const double Sd = KeysGeo::ShoreInfo(X, Z).D;
				if (Sd < 5 || Sd > 24 || KeysGeo::LandH(X, Z) > -.3 || KeysGeo::DepthAt(X, Z) - LowTide < .6 || KeysGeo::CreekDist(X, Z) < 20) continue;
				double Across = -1;
				for (int32 a = 0; a < 4 && Across < 0; a++)
				{
					const double Cs = FMath::Cos(a * PI / 4), Sn = FMath::Sin(a * PI / 4);
					double D0 = -1, D1 = -1;
					for (double R = 2; R <= 32 && D0 < 0; R += 2) if (KeysGeo::LandH(X + Cs * R, Z + Sn * R) > .2) D0 = R;
					for (double R = 2; R <= 32 && D1 < 0; R += 2) if (KeysGeo::LandH(X - Cs * R, Z - Sn * R) > .2) D1 = R;
					if (D0 > 0 && D1 > 0 && D0 + D1 >= 14 && D0 + D1 <= 45) Across = a * PI / 4;
				}
				if (Across < 0) continue;
				Cells++;
				Bins.FindOrAdd((int64(FMath::FloorToInt32(X / 160)) << 32) ^ int64(uint32(FMath::FloorToInt32(Z / 160)))).Add({X, Z, Across});
			}
		Bins.ValueSort([](const TArray<FCell>& A, const TArray<FCell>& B) { return A.Num() > B.Num(); });
		int32 Shown = 0;
		for (const TPair<int64, TArray<FCell>>& Pr : Bins)
		{
			if (Shown++ >= 3) break;
			FVector2D Mid(0, 0);
			for (const FCell& Cl : Pr.Value) Mid += FVector2D(Cl.X, Cl.Z);
			Mid /= Pr.Value.Num();
			const FCell* Near = &Pr.Value[0];
			for (const FCell& Cl : Pr.Value) if (FVector2D::DistSquared(FVector2D(Cl.X, Cl.Z), Mid) < FVector2D::DistSquared(FVector2D(Near->X, Near->Z), Mid)) Near = &Cl;
			UE_LOG(LogTemp, Log, TEXT("KRDocks canal water: %d cells (%.0f m2) round %.0f, %.0f"), Pr.Value.Num(), Pr.Value.Num() * St * St, Mid.X, Mid.Y);
			TraceCut(FVector2D(Near->X, Near->Z), Near->Across + PI / 2, 400);
			TraceCut(FVector2D(Near->X, Near->Z), Near->Across - PI / 2, 400);
		}
		return FString::Printf(TEXT("%d cells of canal-like water (%.0f m2 each) in %d places"), Cells, St * St, Bins.Num());
	}
	if (Cmd.StartsWith(TEXT("go=")) || Cmd.StartsWith(TEXT("look=")))
	{
		// go=<dock>[,off[,across]]: lie the boat that far off the dock's head, bow to it
		// look=<dock>[,dist[,height[,angledeg]]]: a camera looking at its lift (or its head) from that far, the angle
		// measured round from straight out along the dock toward the lift's side
		const bool bGo = Cmd.StartsWith(TEXT("go="));
		TArray<FString> Pp;
		Cmd.Mid(bGo ? 3 : 5).ParseIntoArray(Pp, TEXT(","));
		const int32 K = Pp.Num() ? FindDock(Pp[0]) : -1;
		if (K < 0) return TEXT("no such dock: ") + Cmd;
		const FDock& Dk = Docks[K];
		APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0);
		if (!PC) return TEXT("no player");
		if (bGo)
		{
			AKeysBoatPawn* Boat = Cast<AKeysBoatPawn>(PC->GetPawn());
			if (!Boat) return TEXT("no boat");
			const FVector2D Q = Dk.At(Dk.Box.A1 + (Pp.Num() >= 2 ? FCString::Atod(*Pp[1]) : 14), (Dk.Box.C0 + Dk.Box.C1) / 2 + (Pp.Num() >= 3 ? FCString::Atod(*Pp[2]) : 0));
			Boat->DebugPlace(Q.X, Q.Y, FMath::Atan2(Dk.NX, Dk.NZ));
			return FString::Printf(TEXT("boat off dock %d at %.0f, %.0f"), K, Q.X, Q.Y);
		}
		if (!EyeCam)
		{
			EyeCam = GetWorld()->SpawnActor<ACameraActor>();
			if (EyeCam && EyeCam->GetCameraComponent()) { EyeCam->GetCameraComponent()->SetConstraintAspectRatio(false); EyeCam->GetCameraComponent()->SetFieldOfView(70.f); }
		}
		if (!EyeCam) return TEXT("no camera");
		const FRectL& Hd = Dk.Parts[1];
		const bool bLift = Dk.Lift.Kind != KeysLifts::LiftNone;
		const FVector Aim = bLift && !Pp[0].Contains(TEXT("fish")) ? FVector(Dk.Lift.X, Dk.Lift.Y, Dk.DeckZ + .6) : FVector((Hd.A0 + Hd.A1) / 2, (Hd.C0 + Hd.C1) / 2, Dk.DeckZ + .9);
		const double Dist = Pp.Num() >= 2 ? FCString::Atod(*Pp[1]) : 14, Hgt = Pp.Num() >= 3 ? FCString::Atod(*Pp[2]) : 3.2, Ang = FMath::DegreesToRadians(Pp.Num() >= 4 ? FCString::Atod(*Pp[3]) : 55);
		const FVector From(Aim.X + Dist * FMath::Cos(Ang), Aim.Y + Dk.Side * Dist * FMath::Sin(Ang), Hgt);
		const FVector2D QF = Dk.At(From.X, From.Y), QA = Dk.At(Aim.X, Aim.Y);
		const FVector FromUE = KeysGeo::ToUE(QF.X, From.Z, QF.Y), AimUE = KeysGeo::ToUE(QA.X, Aim.Z, QA.Y);
		EyeCam->SetActorLocationAndRotation(FromUE, (AimUE - FromUE).Rotation());
		PC->SetViewTarget(EyeCam);
		return FString::Printf(TEXT("looking at dock %d"), K);
	}
	if (Cmd.StartsWith(TEXT("eye=")))
	{
		APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0);
		if (!PC) return TEXT("no player");
		if (Cmd == TEXT("eye=off")) { PC->SetViewTarget(PC->GetPawn()); return TEXT("eye off"); }
		TArray<FString> Pp;
		Cmd.Mid(4).ParseIntoArray(Pp, TEXT(","));
		if (Pp.Num() < 5 || FindDock(Pp[0]) < 0) return TEXT("eye=dock,along,across,height,yawdeg[,pitchdeg]");
		if (!EyeCam)
		{
			EyeCam = GetWorld()->SpawnActor<ACameraActor>();
			if (EyeCam && EyeCam->GetCameraComponent()) { EyeCam->GetCameraComponent()->SetConstraintAspectRatio(false); EyeCam->GetCameraComponent()->SetFieldOfView(70.f); }
		}
		if (!EyeCam) return TEXT("no camera");
		const FDock& Dk = Docks[FindDock(Pp[0])];
		const FVector2D Q = Dk.At(FCString::Atod(*Pp[1]), FCString::Atod(*Pp[2]));
		// yaw 0 looks out along the dock, 180 back at the house
		EyeCam->SetActorLocationAndRotation(KeysGeo::ToUE(Q.X, FCString::Atod(*Pp[3]), Q.Y), FRotator(Pp.Num() >= 6 ? FCString::Atod(*Pp[5]) : 0, FMath::RadiansToDegrees(FMath::Atan2(Dk.NZ, Dk.NX)) + FCString::Atod(*Pp[4]), 0));
		PC->SetViewTarget(EyeCam);
		return FString::Printf(TEXT("eye at dock %s"), *Cmd.Mid(4));
	}
	return FString::Printf(TEXT("docks %d tiles %d awake tiles %d night %d meshes %d tris %d people %d (drawn %d %d) sea %.2f"), Docks.Num(), Tiles.Num(), AwakeTiles, bNight ? 1 : 0, Meshes.Num(),
		Tris[0] + Tris[1] + Tris[2] + Tris[3] + Tris[4], People.Num(), PersonAt[0], PersonAt[1], KeysOcean::SeaLevel());
}
