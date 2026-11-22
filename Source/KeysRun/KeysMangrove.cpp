// The real map's red mangroves, grown from the baked cover map (Tools/geo/bake_mangrove.py -> KeysGeoReal::MangroveCell).
//
//  - along the water (the shore's edge and the fringe wading out over the shallows): trees on arching prop roots that
//    come down into the water, a short crooked trunk, and a low, dense, dark canopy
//  - inland: canopy only, shoulder to shoulder, so a mangrove key reads as one solid green hummock
//  - along the creeks (KeysMangroveLife::Creeks(), once KeysGeo has cut them): nothing stands in the channel, both
//    banks are planted with trees leaning in over the water on roots that wade into it, and where a creek is roofed
//    the limbs arch across from both banks and the canopy closes overhead, high enough to idle a skiff under
//  - pelicans roosting on dead snags along the outer edge and at the creek mouths (KeysMangroveLife::Snag)
// The crowns are lumpy domes with shared, smooth-shaded vertices, darker underneath and lighter on top.
// One mesh per 600 m square and per kind: the roots and trunks are drawn within 700 m (they are thin; past that they
// are gone anyway), the canopy to 4.5 km.
#include "KeysWorld.h"
#include "KeysGeo.h"
#include "KeysGeoReal.h"
#include "KeysTown.h"
#include "KeysTownData.h"
#include "KeysMangroveLife.h"
#include "KeysMangroveEco.h"
#include "KeysYards.h"
#include "ProceduralMeshComponent.h"
#include "Async/Async.h"

namespace
{
	double MHash(double A, double B) { const double H = FMath::Sin(A * 127.1 + B * 311.7) * 43758.5453; return H - FMath::FloorToDouble(H); }

	// a wading bird standing in the shallows: great egret (white), great blue heron (slate) or little blue (dark), facing Yaw
	void Wader(FTownBatch& B, double X, double Y, double Z, double Yaw, int32 Kind, double Sz);

	using KeysMangroveLife::Limb;

	// A crown of leaves: a lumpy dome round C, R across and SY as tall, its vertices shared so it shades round. Dark
	// in the shade underneath, Base round the sides, Top where the sun gets at it. NU round, NR rings up to the top;
	// bUnder closes the underside (wanted wherever you can get beneath it); Shade is how dark the leaves are down there.
	void Crown(FTownBatch& B, const FVector& C, double R, double SY, uint32 Base, uint32 Top, double Seed, int32 NU, int32 NR, bool bUnder, float Shade = .55f)
	{
		FKeysMesh& M = B.M;
		const FLinearColor Bc = KeysTownPaint::Hex(Base), Tc = KeysTownPaint::Hex(Top);
		const double Lat0 = -.6, S0 = FMath::Sin(Lat0);
		const int32 V0 = M.V.Num();
		auto Put = [&M](const FVector& P, const FVector& Nn, FLinearColor Col) { Col.A = 1; M.AddVert(FVector(P.X, P.Z, P.Y), FVector(Nn.X, Nn.Z, Nn.Y).GetSafeNormal(), Col); };
		for (int32 j = 0; j < NR; j++)
		{
			const double Lat = Lat0 + (PI / 2 - Lat0) * j / NR, CL = FMath::Cos(Lat), SL = FMath::Sin(Lat), Tt = (SL - S0) / (1 - S0);
			for (int32 i = 0; i < NU; i++)
			{
				const double Lon = 2 * PI * i / NU + Seed;
				const double Lump = 1 + .07 * FMath::Sin(Lon * 3 + Seed * 6.1) * FMath::Cos(Lat * 2.3 + Seed * 3.7) + .04 * (MHash(Seed + i * 1.37, j * 2.11 + Seed * .31) - .5);
				const FVector Dir(CL * FMath::Sin(Lon), SL, CL * FMath::Cos(Lon));
				FLinearColor Col = FMath::Lerp(Bc * Shade, Bc, float(FMath::Min(1.0, Tt * 2.4)));
				Col = FMath::Lerp(Col, Tc, float(FMath::Clamp((Tt - .45) * 2.2, 0.0, 1.0)) * .9f);
				Put(C + FVector(Dir.X * R * Lump, Dir.Y * R * SY * Lump, Dir.Z * R * Lump), FVector(Dir.X, Dir.Y / SY * .8 + .12, Dir.Z), Col * float(.9 + .2 * MHash(Seed * 2.3 + i, j + Seed)));
			}
		}
		const int32 TopI = M.V.Num();
		Put(C + FVector(0, R * SY * (.97 + .08 * MHash(Seed, 3.3)), 0), FVector(0, 1, 0), Tc);
		const int32 BotI = M.V.Num();
		if (bUnder) Put(C + FVector(0, S0 * R * SY * .7, 0), FVector(0, -1, 0), Bc * (Shade * .7f));
		auto Tri = [&M](int32 I0, int32 I1, int32 I2)
		{
			// wound as FTownBatch::TriN winds its own
			const double D = FVector::DotProduct(FVector::CrossProduct(M.V[I1] - M.V[I0], M.V[I2] - M.V[I0]), M.N[I0] + M.N[I1] + M.N[I2]);
			if (D < 0) M.T.Append({I0, I1, I2});
			else M.T.Append({I0, I2, I1});
		};
		for (int32 i = 0; i < NU; i++)
		{
			const int32 i1 = (i + 1) % NU;
			for (int32 j = 0; j + 1 < NR; j++)
			{
				const int32 A = V0 + j * NU + i, Bb = V0 + j * NU + i1, Cc = V0 + (j + 1) * NU + i1, Dd = V0 + (j + 1) * NU + i;
				Tri(A, Bb, Cc);
				Tri(A, Cc, Dd);
			}
			Tri(V0 + (NR - 1) * NU + i, V0 + (NR - 1) * NU + i1, TopI);
			if (bUnder) Tri(V0 + i1, V0 + i, BotI);
		}
	}
}

namespace
{
	void Wader(FTownBatch& B, double X, double Y, double Z, double Yaw, int32 Kind, double Sz)
	{
		static const uint32 Body[] = {0xf4f4ee, 0x6f7f92, 0x3c4a66}, Bill[] = {0xe8b030, 0xd8a838, 0x4a5a70}, Leg[] = {0x1c1c1c, 0x5a5648, 0x4a5240};
		const FVector F(FMath::Cos(Yaw), 0, FMath::Sin(Yaw)), Up(0, 1, 0), O(X, Y, Z);
		const FLinearColor Lc = KeysTownPaint::Hex(Leg[Kind]), Bc = KeysTownPaint::Hex(Body[Kind]);
		const double H = .62 * Sz;                                   // how high the body stands
		for (const double Sd : {-1.0, 1.0})
		{
			const FVector Hip = O + Up * H + FVector(-F.Z, 0, F.X) * .05 * Sd * Sz + F * (.04 * Sd) * Sz;
			Limb(B, Hip, FVector(Hip.X, Y - .35, Hip.Z) + F * (.05 * Sd), .012 * Sz, Lc);
		}
		B.Ball(O.X - F.X * .05 * Sz, O.Y + H + .1 * Sz, O.Z - F.Z * .05 * Sz, .2 * Sz, Body[Kind], .62, 6);          // body
		const FVector Sh = O + Up * (H + .2 * Sz) + F * .14 * Sz, Bend = Sh + Up * .26 * Sz - F * .05 * Sz, Head = Bend + Up * .22 * Sz + F * .1 * Sz;
		Limb(B, Sh, Bend, .035 * Sz, Bc);                                                                                // the S of the neck
		Limb(B, Bend, Head, .028 * Sz, Bc);
		B.Ball(Head.X, Head.Y, Head.Z, .055 * Sz, Body[Kind], .9, 5);
		Limb(B, Head + F * .03 * Sz, Head + F * .24 * Sz - Up * .03 * Sz, .014 * Sz, KeysTownPaint::Hex(Bill[Kind]));
	}
}

namespace
{
	// where no mangrove is planted: yards, canals, fill, the town, runways, marked channels, marinas
	bool MgBlocked(double X, double Z)
	{
		using namespace KeysGeo;
		if (KeysYards::InFootprint(X, Z, 6)) return true;   // the dry stack and the boatyard
		if (CanalDist(X, Z) < 12) return true;              // the canals and their seawalls
		if (InFill(X, Z) || InTownPad(X, Z) || KeysTown::InFootprint(X, Z, 20) || SurfaceKind(X, Z) == 2 || InMarkedChannel(X, Z)) return true;
		for (const FMarina& M : Marinas()) if (FMath::Square(X - M.SX) + FMath::Square(Z - M.SZ) < 70.0 * 70.0) return true;
		return false;
	}
	// estate lots keep their waterfront: nothing of the creek's is planted on one
	bool MgOnLot(double X, double Z)
	{
		for (const KeysGeoReal::FPlatLot& L : KeysGeoReal::Plat())
		{
			if (FMath::Abs(X - L.CX) > 90 || FMath::Abs(Z - L.CZ) > 90) continue;
			bool bIn = false;
			for (int32 a = 0, b = L.Poly.Num() - 1; a < L.Poly.Num(); b = a++)
				if ((L.Poly[a].Y > Z) != (L.Poly[b].Y > Z) && X < (L.Poly[b].X - L.Poly[a].X) * (Z - L.Poly[a].Y) / (L.Poly[b].Y - L.Poly[a].Y) + L.Poly[a].X) bIn = !bIn;
			if (bIn) return true;
		}
		return false;
	}
	// a lump on a root: an oyster, a barnacle, a sponge (a squashed eight-sided stone)
	void MgLump(FTownBatch& B, const FVector& C, double R, const FLinearColor& Col)
	{
		const FVector Px[6] = {C + FVector(R, 0, 0), C - FVector(R, 0, 0), C + FVector(0, R * .7, 0), C - FVector(0, R * .7, 0), C + FVector(0, 0, R), C - FVector(0, 0, R)};
		for (int32 a = 0; a < 2; a++)
			for (int32 b = 2; b < 4; b++)
				for (int32 c = 4; c < 6; c++)
					B.Tri3(Px[a], Px[b], Px[c], (Px[a] + Px[b] + Px[c]) / 3 - C, Col * float(.85 + .1 * a + .1 * (c - 4)));
	}

	// One 600 m square of mangrove: its roots and trunks (drawn close) and its canopy (drawn far).
	struct FMgTile { FTownBatch Roots, Canopy; };
	int64 MgKey(double X, double Z) { return (int64(FMath::FloorToInt(X / 600)) << 32) ^ int64(uint32(FMath::FloorToInt(Z / 600))); }
	// the creeks are scenery only once the ground is cut for them (KeysGeo reads the same lines)
	bool MgCreeksCut()
	{
		const TArray<KeysMangroveLife::FCreek>& Creeks = KeysMangroveLife::Creeks();
		return Creeks.Num() && Creeks[0].Pts.Num() > 2 && KeysGeo::CreekDist(Creeks[0].Pts[Creeks[0].Pts.Num() / 2].X, Creeks[0].Pts[Creeks[0].Pts.Num() / 2].Y) < 0;
	}

	// Grows the mangroves of the tiles in Want, and nothing else. Geometry only (no engine objects), and every tree is
	// seeded by where it stands, so a tile comes out the same whenever and in whatever order it is made: the tiles are
	// made as the boat comes within sight of them (AKeysWorld::StreamMangroves), off the game thread.
	void GenMangroves(const TSet<int64>& Want, TMap<int64, FMgTile*>& Tiles)
	{
	using namespace KeysGeo;
	typedef FMgTile FTile;
	auto Wanted = [&](double X, double Z) { return Want.Contains(MgKey(X, Z)); };
	auto TileAt = [&](double X, double Z) -> FTile&
	{
		FTile*& T = Tiles.FindOrAdd(MgKey(X, Z));
		if (!T) { T = new FTile(); T->Canopy.M.Surf = SurfCanopy; }
		return *T;
	};
	static const uint32 Leaf[] = {0x2f6b2f, 0x3a7a34, 0x285a2a, 0x376f2c, 0x2a6238};
	static const uint32 LeafTop[] = {0x4a8a3c, 0x55933f, 0x448238};
	const FLinearColor RootC = KeysTownPaint::Hex(0x6e4a30), RootDark = KeysTownPaint::Hex(0x4a3524), Trunk = KeysTownPaint::Hex(0x5c4836);
	const double Cell = KeysGeoReal::Cell();
	int32 Rooted = 0, Crowns = 0, Birds = 0, Pelicans = 0, BankTrees = 0, RoofSpans = 0;
	const TArray<KeysMangroveLife::FCreek>& Creeks = KeysMangroveLife::Creeks();
	const bool bCreeks = MgCreeksCut();
	auto CreekD = [&](double X, double Z) { return bCreeks ? KeysMangroveLife::Near(X, Z).D : 1e9; };
	auto Blocked = [&](double X, double Z) { return MgBlocked(X, Z); };
	// with the ecosystem on (KeysMangroveEco) the birds and crabs are alive and not part of the mesh; the roots carry
	// oysters, barnacles and sponges below the tide line instead
	const bool bEco = KeysMangroveEco::Enabled();
	static const uint32 ShellC[] = {0xb9b5a6, 0xa39f92, 0xd6d2c4, 0xc9c4b2}, SpongeC[] = {0xc8572a, 0x7a3f8a, 0xb89a2a, 0xa83a30};

	// A red mangrove standing at (X, Z), shaped on a photograph of a mangrove creek: a cage of thin prop roots that
	// spring from the trunk, bow out and over and plunge into the mud; a slender crooked trunk standing on top of it;
	// boughs spreading to several clumps of leaves at different heights, so the outline is ragged and you see trunks
	// and roots in the shade under it. Lean carries its head that way (metres), Tall lifts it. Detail 0: a tree on the
	// landward edge, 1: one with its feet in the water, 2: one you idle right past. Returns the top of its crown.
	auto Tree = [&](FTile& T, double X, double Z, double Ground, const FVector2D& Lean, double Tall, int32 Detail = 1) -> FVector
	{
		const double Base = FMath::Max(Ground, -.05), Foot = FMath::Min(Ground, -.35) - .25;   // the roots go on down into the mud
		const double Sz = .8 + .5 * MHash(Z, X);
		const double Hh = (1.3 + .9 * MHash(X, Z)) * Sz + Tall * .5;                              // the top of the root cage
		const double TrunkH = (2.3 + 1.5 * MHash(X + 4.0, Z)) * Sz + Tall * .5;
		const FVector Fork(X, Base + Hh, Z);
		const FVector Mid(X + (MHash(X, 3.0) - .5) * 1.0 + Lean.X * .45, Fork.Y + TrunkH * .55, Z + (MHash(Z, 5.0) - .5) * 1.0 + Lean.Y * .45);
		Limb(T.Roots, Fork, Mid, .12 * Sz, Trunk);
		// the prop roots, each an arc of a few straight pieces
		const int32 NR = (Detail == 2 ? 11 : Detail == 1 ? 8 : 5) + int32(MHash(X + 1.0, Z) * 2.5), Seg = Detail == 2 ? 4 : Detail == 1 ? 3 : 2;
		auto Arc = [&](const FVector& From, const FVector& End, double Bow, double R0, const FLinearColor& C)
		{
			const FVector Ctrl(From.X + (End.X - From.X) * .78, From.Y + Bow, From.Z + (End.Z - From.Z) * .78);
			FVector P = From;
			for (int32 k = 1; k <= Seg; k++)
			{
				const double U = double(k) / Seg;
				const FVector Q = From * ((1 - U) * (1 - U)) + Ctrl * (2 * U * (1 - U)) + End * (U * U);
				Limb(T.Roots, P, Q, R0 * (1 - .3 * U), C * float(1 - .15 * U));
				P = Q;
			}
		};
		for (int32 r = 0; r < NR; r++)
		{
			const double A = 2 * PI * (r + MHash(X + r, Z)) / NR, Reach = (1.3 + 2.1 * MHash(X, Z + r)) * Sz;
			const FVector Dir(FMath::Cos(A), 0, FMath::Sin(A));
			// most leave the foot of the trunk at different heights; every third one comes off the trunk higher up
			const FVector From = r % 3 == 2 ? FMath::Lerp(Fork, Mid, .25 + .5 * MHash(r, Z)) : Fork + FVector(0, -Hh * .55 * MHash(r, X), 0);
			const FVector End = FVector(X, Foot, Z) + Dir * Reach * (r % 3 == 2 ? 1.25 : 1.0);
			const FLinearColor C = FMath::Lerp(RootC, RootDark, float(MHash(r * 3.0, X + Z)) * .7f);
			Arc(From, End, (.1 + .4 * MHash(Z, r)) * Sz, .052 * Sz, C);
			if (bEco && Detail > 0)
			{
				// what grows on it between the tides: oysters and barnacles high, sponges lower, on the root's last piece
				const double Bow = (.1 + .4 * MHash(Z, r)) * Sz, U = double(Seg - 1) / Seg;
				const FVector Ctrl(From.X + (End.X - From.X) * .78, From.Y + Bow, From.Z + (End.Z - From.Z) * .78);
				const FVector Pl = From * ((1 - U) * (1 - U)) + Ctrl * (2 * U * (1 - U)) + End * (U * U);
				for (int32 n = 0; n < 2; n++)
				{
					if (MHash(X + r * 2.3 + n, Z - n * 5.1) > .62) continue;
					const double YT = -.36 + .5 * MHash(Z + r, X + n * 3.7);
					if (Pl.Y <= YT || End.Y >= YT) continue;
					const FVector At = FMath::Lerp(Pl, End, (Pl.Y - YT) / (Pl.Y - End.Y));
					const bool bSponge = YT < -.14 && MHash(r + n, X - Z) < .45;
					const int32 Ci = int32(MHash(X + n, Z + r * 1.9) * 4) % 4;
					MgLump(T.Roots, At + FVector((MHash(r, n + X) - .5) * .05, 0, (MHash(n + Z, r) - .5) * .05), (bSponge ? .07 : .045) + .045 * MHash(X - r, Z + n), KeysTownPaint::Hex(bSponge ? SpongeC[Ci] : ShellC[Ci]));
				}
			}
			// and near the water many of them divide
			if (Detail == 2 && MHash(X + r * 1.7, Z - r) < .55)
			{
				const double A2 = A + (MHash(r, X + Z) < .5 ? -.5 : .5);
				const FVector Split = FMath::Lerp(From, End, .45) + FVector(0, (From.Y - Foot) * .22, 0);
				Arc(Split, FVector(X + FMath::Cos(A2) * Reach * .9, Foot, Z + FMath::Sin(A2) * Reach * .9), .1 * Sz, .036 * Sz, C * .9f);
			}
		}
		// the boughs and the leaves on them: one clump over the trunk, the others shouldering out round it
		const int32 NB = Detail == 2 ? 4 : 3;
		const double R = (2.5 + 1.0 * MHash(X * 1.3, Z)) * Sz, A0 = 2 * PI * MHash(X * 2.1, Z * 1.7);
		const float Shade = Detail == 2 ? .4f : .55f;
		FVector Apex = Mid;
		for (int32 b = 0; b < NB; b++)
		{
			const double H1 = MHash(X + b * 2.0, Z + 3.0), H2 = MHash(Z + b * 5.0, X + 1.0);
			const double A = A0 + 2 * PI * (b + .6 * H1) / FMath::Max(1, NB - 1), Out = b == 0 ? .25 * R : (.8 + .35 * H2) * R;
			const FVector End(Mid.X + FMath::Cos(A) * Out + Lean.X * .55, Mid.Y + TrunkH * .45 + (b == 0 ? .75 : .1 + .55 * H1) * R, Mid.Z + FMath::Sin(A) * Out + Lean.Y * .55);
			Limb(T.Roots, Mid, End, .07 * Sz, Trunk);
			const double Rc = R * (b == 0 ? 1.0 : .68 + .26 * H2), SY = .6 + .16 * H1;
			Crown(T.Canopy, End + FVector(0, Rc * .12, 0), Rc, SY, Leaf[int32(MHash(X + b, Z * 1.9) * 5) % 5], LeafTop[int32(MHash(Z, X * 1.1 + b) * 3) % 3], 6.28 * MHash(X + b * 3.0, Z * 1.9), Detail == 2 ? 12 : b == 0 && Detail == 1 ? 10 : 8, Detail == 2 ? 5 : b == 0 && Detail == 1 ? 4 : 3, true, Shade);
			if (End.Y + Rc * (.12 + SY * .93) > Apex.Y) Apex = FVector(End.X, End.Y + Rc * (.12 + SY * .93), End.Z);
			// a thin aerial root let down from the bough to the water
			if (b > 0 && Detail > 0 && H2 < .7)
			{
				const FVector Hang = FMath::Lerp(Mid, End, .6 + .3 * H1);
				Limb(T.Roots, Hang, FVector(Hang.X + (H1 - .5) * .3, Foot, Hang.Z + (H2 - .5) * .3), .028, RootDark);
			}
		}
		Rooted++;
		// fiddler and mangrove tree crabs on the roots: specks of red-orange and dark shell, a few to a tree
		for (int32 c = 0; !bEco && c < 3 && MHash(X + 2.0, Z + c) < .6; c++)
		{
			const double A = 2 * PI * MHash(X + c * 4.0, Z - 1.0), Rr = (.5 + 1.2 * MHash(Z + c, X + 6.0)) * Sz;
			T.Roots.Ball(X + FMath::Cos(A) * Rr, Base + .25 + .9 * MHash(X - c, Z + 8.0), Z + FMath::Sin(A) * Rr, .045, c == 0 ? 0xc8482a : 0x4a2c22, .6, 4);
		}
		return Apex;
	};

	// every tree is seeded by its cell's number on the first phase's own grid (I, J), not by where the cell sits in the
	// whole map (GI, GJ): the map can grow north or west and the trees already standing stay as they are
	const int32 SeedI0 = KeysGeoReal::CellI0(), SeedJ0 = KeysGeoReal::CellJ0();
	for (const int64 WK : Want)
	{
	// (with the ecosystem on: a cormorant's limb on the trees it picks, and a roost snag on the tile's most open point)
	if (bEco)
	{
		TArray<KeysMangroveEco::FSite> EcoSites;
		TArray<FVector> EcoTops;
		KeysMangroveEco::SitesOfTile(WK, EcoSites, EcoTops);
		for (const KeysMangroveEco::FSite& S : EcoSites)
		{
			FVector A, Bq;
			if (KeysMangroveEco::PerchLimb(S, A, Bq)) Limb(TileAt(S.X, S.Z).Roots, A, Bq, .06, KeysTownPaint::Hex(0x8a8276));
		}
		FVector Rp;
		double RSeed = 0;
		if (KeysMangroveEco::RoostSnag(WK, EcoSites, Rp, RSeed)) Pelicans += KeysMangroveLife::Snag(TileAt(Rp.X, Rp.Z).Roots, Rp.X, Rp.Y, Rp.Z, RSeed, 3);
	}
	// the cells whose centres lie in this tile
	const double TX0 = int32(WK >> 32) * 600.0, TZ0 = int32(uint32(WK)) * 600.0;
	const int32 GI0 = FMath::Max(0, FMath::CeilToInt((TX0 - KeysGeoReal::X0()) / Cell)), GI1 = FMath::Min(KeysGeoReal::NX(), FMath::CeilToInt((TX0 + 600 - KeysGeoReal::X0()) / Cell));
	const int32 GJ0 = FMath::Max(0, FMath::CeilToInt((TZ0 - KeysGeoReal::Z0()) / Cell)), GJ1 = FMath::Min(KeysGeoReal::NZ(), FMath::CeilToInt((TZ0 + 600 - KeysGeoReal::Z0()) / Cell));
	for (int32 GJ = GJ0; GJ < GJ1; GJ++)
		for (int32 GI = GI0; GI < GI1; GI++)
		{
			const uint8 Mv = KeysGeoReal::MangroveCell(GI, GJ);
			if (!Mv) continue;
			const int32 I = GI + SeedI0, J = GJ + SeedJ0;
			const double CX = KeysGeoReal::X0() + GI * Cell, CZ = KeysGeoReal::Z0() + GJ * Cell;
			if (Blocked(CX, CZ)) continue;
			const double CellCD = CreekD(CX, CZ);
			if (CellCD < -2) continue;                                   // open water now
			const bool bFringe = Mv < 200;
			const double Sd = KeysGeoReal::Shore(CX, CZ);
			const bool bEdge = bFringe || Sd > -13 || CellCD < 15;      // a creek bank is an edge too
			FTile& T = TileAt(CX, CZ);
			if (!bEdge)
			{
				// inland: two overlapping crowns to a cell, no trunks to see
				for (int32 k = 0; k < 2; k++)
				{
					const double X = CX + (MHash(I + k * 7.3, J) - .5) * Cell, Z = CZ + (MHash(J, I + k * 3.1) - .5) * Cell;
					const double R = 5.2 + 2.2 * MHash(I * 1.7, J + k), Y = FMath::Max(.3, LandH(X, Z)) + 3.8 + 3.0 * MHash(I + k, J * 2.3);   // (a ragged skyline, level with the trees along the water)
					Crown(T.Canopy, FVector(X, Y - R * .12, Z), R, .56, Leaf[int32(MHash(I, J + k * 5.0) * 5) % 5], LeafTop[int32(MHash(J, I + k * 3.0) * 3) % 3], 6.28 * MHash(I + k, J), 8, 3, false);
					Crowns++;
				}
				continue;
			}
			// along the water: trees on prop roots
			const int32 NTrees = bFringe ? 1 + int32(MHash(I, J * 1.3) * 1.6) : 2;
			for (int32 k = 0; k < NTrees; k++)
			{
				const double X = CX + (MHash(I + k * 5.1, J + 2.0) - .5) * Cell * .9, Z = CZ + (MHash(J + 4.0, I + k * 2.7) - .5) * Cell * .9;
				if (CreekD(X, Z) < 4.6) continue;                         // the channel and its banks are planted below
				const double Ground = LandH(X, Z);
				if (!bFringe && Ground < -.6) continue;
				const FVector Apex = Tree(T, X, Z, Ground, FVector2D::ZeroVector, 0, bFringe ? 1 : 0);
				// a heron or an egret now and then, stalking the shallows just off the roots
				if (!bEco && bFringe && k == 0 && MHash(I * 3.7, J * 1.9) < .07)
				{
					const double A = 2 * PI * MHash(I, J + 11.0), WX = X + FMath::Cos(A) * 4.5, WZ = Z + FMath::Sin(A) * 4.5;
					const double Wd = DepthAt(WX, WZ);
					if (LandH(WX, WZ) < -.05 && Wd < .7)
					{
						const double Pk = MHash(J * 2.3, I);
						Wader(T.Roots, WX, .05, WZ, 2 * PI * MHash(I + 5.0, J), Pk < .55 ? 0 : Pk < .85 ? 1 : 2, Pk < .55 ? 1.0 : Pk < .85 ? 1.25 : .7);
						Birds++;
					}
				}
				// and here and there along the outer edge a dead snag stands out of the green, with pelicans drying on it
				if (bFringe && k == 0 && MHash(I * 2.9 + 1.0, J * 4.3) < .014)
				{
					const double A = 2 * PI * MHash(J + 3.0, I * 1.7), SX = X + FMath::Cos(A) * 3.5, SZ = Z + FMath::Sin(A) * 3.5;
					if (LandH(SX, SZ) < .3 && CreekD(SX, SZ) > 6)
						Pelicans += KeysMangroveLife::Snag(T.Roots, SX, FMath::Min(LandH(SX, SZ), -.3), SZ, MHash(I, J) * 50, 1 + int32(MHash(J * 1.3, I + 2.0) * 3));
				}
				// or one sits on top of a living tree, on the seaward edge where it can see the water
				if (bFringe && MHash(I * 5.3 + 2.0, J * 1.1 + k) < .022)
				{
					KeysMangroveLife::Pelican(T.Roots, Apex, 2 * PI * MHash(J * 3.1, I + k), 1.7 + .3 * MHash(I, J * 7.7), MHash(I + 1.0, J) < .35 ? 1 : 0);
					Pelicans++;
				}
			}
		}

	}

	// (the creek scenery only for tiles a creek comes near)
	bool bNearCreek = false;
	if (bCreeks)
		for (const int64 WK : Want)
			for (const KeysMangroveLife::FCreek& C : Creeks)
			{
				const double TXc = int32(WK >> 32) * 600.0 + 300, TZc = int32(uint32(WK)) * 600.0 + 300;
				bNearCreek |= TXc > C.X0 - 400 && TXc < C.X1 + 400 && TZc > C.Z0 - 400 && TZc < C.Z1 + 400;
			}
	if (bNearCreek)
	{
		auto OnLot = [](double X, double Z) { return MgOnLot(X, Z); };
		// the banks: a row of trees down each side with their feet in the water, heads leaning in over the channel
		const double Step = 3.3;
		for (const KeysMangroveLife::FCreek& C : Creeks)
			for (double Z = C.Z0; Z < C.Z1; Z += Step)
				for (double X = C.X0; X < C.X1; X += Step)
				{
					const double PX = X + (MHash(X, Z + 1.7) - .5) * Step * .8, PZ = Z + (MHash(Z, X + 2.9) - .5) * Step * .8;
					if (!Wanted(PX, PZ)) continue;
					const KeysMangroveLife::FNear N = KeysMangroveLife::Near(PX, PZ);
					// (a tunnel's banks are planted three deep, so no daylight comes in under the roof from the sides)
					if (N.D < .5 || N.D > (N.bRoof ? 10.5 : 4.4) || &Creeks[N.Creek] != &C || Blocked(PX, PZ) || OnLot(PX, PZ)) continue;
					const bool bFront = N.D < 4.4;
					FVector2D In = N.C - FVector2D(PX, PZ);
					In = In.GetSafeNormal() * (!bFront ? 0 : N.bRoof ? 1.8 : 1.1);
					const FVector Apex = Tree(TileAt(PX, PZ), PX, PZ, LandH(PX, PZ), In, !bFront ? .2 : N.bRoof ? 1.4 : .3, bFront ? 2 : 0);
					BankTrees++;
					// the trees by a roost carry a bird or two as well
					if (!N.bRoof && MHash(PX * 1.9, PZ * 2.3) < .5)
						for (const FVector2D& R : KeysMangroveLife::Roosts())
							if (FMath::Square(R.X - PX) + FMath::Square(R.Y - PZ) < 16.0 * 16.0)
							{
								KeysMangroveLife::Pelican(TileAt(PX, PZ).Roots, Apex, 2 * PI * MHash(PZ, PX), 1.7 + .3 * MHash(PX, PZ * 3.3), MHash(PX + 1.0, PZ) < .35 ? 1 : 0);
								Pelicans++;
								break;
							}
				}
		// the tunnels: every few metres a pair of limbs springs from the banks and meets over the middle, and the
		// canopy lies along them - low at the sides, some 7 m clear over the middle of the channel
		for (const KeysMangroveLife::FCreek& C : Creeks)
		{
			if (C.Roof1 <= C.Roof0) continue;
			double S = 0, Next = C.Roof0;
			int32 Station = 0;
			for (int32 i = 0; i + 1 < C.Pts.Num(); i++)
			{
				const FVector2D A = C.Pts[i], V = C.Pts[i + 1] - A;
				const double L = V.Size();
				for (; Next <= S + L && Next <= FMath::Min(C.Roof1, C.Len - 4); Next += 5.5, Station++)
				{
					const FVector2D P = A + V * ((Next - S) / L), Tn = V / L, Nn(-Tn.Y, Tn.X);
					if (!Wanted(P.X, P.Y)) continue;
					// not where another creek's water lies under the span (the turning out of Sister Creek, the pond)
					const KeysMangroveLife::FNear Here = KeysMangroveLife::Near(P.X, P.Y);
					if (Here.Creek < 0 || &Creeks[Here.Creek] != &C) continue;
					const double Seed = Station * 1.618 + C.W, HalfW = C.W / 2;
					FTile& T = TileAt(P.X, P.Y);
					auto At = [&](double U, double Y, double Along) { return FVector(P.X + Nn.X * U + Tn.X * Along, Y, P.Y + Nn.Y * U + Tn.Y * Along); };
					for (const double Sd : {-1.0, 1.0})
					{
						const double J1 = (MHash(Seed, Sd + 2.0) - .5) * 2.4, J2 = (MHash(Seed + 1.0, Sd) - .5) * 2.0;
						const FVector Foot = At(Sd * (HalfW + 2.6), 1.6, J1), Shoulder = At(Sd * (HalfW - 1.2), 6.3, J1 * .5 + J2), Crown0 = At(-Sd * .8, 8.7, J2);
						Limb(T.Roots, Foot, Shoulder, .17, Trunk);
						Limb(T.Roots, Shoulder, Crown0, .12, Trunk);
						// aerial roots let down from the limb toward the water at the channel's edge
						if (MHash(Seed + 3.0, Sd) < .7)
						{
							const FVector Hang = FMath::Lerp(Foot, Shoulder, .55 + .3 * MHash(Seed, Sd + 5.0));
							Limb(T.Roots, Hang, FVector(Hang.X, -.5, Hang.Z), .03, RootDark);
						}
						const uint32 Lf = Leaf[int32(MHash(Seed, Sd + 7.0) * 5) % 5], Lt = LeafTop[int32(MHash(Seed + 2.0, Sd) * 3) % 3];
						const double R1 = 4.6 + 1.8 * MHash(Seed + 4.0, Sd), R2 = 4.2 + 1.6 * MHash(Seed, Sd + 6.0);
						Crown(T.Canopy, At(Sd * (HalfW + 2.6 + 1.8 * MHash(Seed, Sd + 8.0)), 6.2 + MHash(Sd, Seed + 1.0), J1), R1, .64, Lf, Lt, Seed + Sd, 10, 4, true, .36f);                    // over the bank
						Crown(T.Canopy, At(Sd * (HalfW - 3.0 + 1.6 * MHash(Seed + 9.0, Sd)), 8.9 + .8 * MHash(Sd, Seed + 2.0), J2 + J1 * .4), R2, .5, Lf, Lt, Seed * 1.3 + Sd, 10, 4, true, .36f);   // over the shoulder
					}
					// the ridge (a gap now and then lets the light in)
					if (Station % 3 != 1) Crown(T.Canopy, At((MHash(Seed, 9.0) - .5) * 3.4, 10.0 + .9 * MHash(Seed, 10.0), (MHash(Seed, 11.0) - .5) * 2.5), 4.4 + 1.6 * MHash(Seed, 12.0), .46, Leaf[Station % 5], LeafTop[Station % 3], Seed * 2.1, 10, 4, true, .36f);
					RoofSpans++;
				}
				S += L;
			}
		}
		// the roosts at the creek mouths
		int32 RoostN = 0;
		for (const FVector2D& R : KeysMangroveLife::Roosts())
		{
			const double Ground = LandH(R.X, R.Y);
			if (Ground > .4 || OnLot(R.X, R.Y)) continue;
			if (Wanted(R.X, R.Y)) Pelicans += KeysMangroveLife::Snag(TileAt(R.X, R.Y).Roots, R.X, FMath::Min(Ground, -.3), R.Y, 7.0 + RoostN, 3);
			RoostN++;
		}
	}

	}

	// what stands now, what could stand, and the one tile being grown in the background
	struct FMgLive { TWeakObjectPtr<UProceduralMeshComponent> PM[2]; bool bRoots = false; };   // [0] roots and trunks, [1] canopy
	TMap<int64, FMgLive> GMgLive;
	TMap<int64, FVector2D> GMgHave;   // every tile with mangrove in it -> its centre
	TFuture<FMgTile*> GMgJob;
	int64 GMgJobKey = 0;
	bool bMgJob = false;
	int32 GMgSync = 0, GMgMade = 0;
	double GMgScanT = 0;
	double GMgLastMs = 0;
	int32 GMgLastTris = 0, GMgWatch = 0;

	FMgTile* GenOneMangroveTile(int64 Key)
	{
		TSet<int64> W;
		W.Add(Key);
		TMap<int64, FMgTile*> Out;
		GenMangroves(W, Out);
		FMgTile* R = nullptr;
		for (TPair<int64, FMgTile*>& P : Out) { if (P.Key == Key) R = P.Value; else delete P.Value; }
		return R ? R : new FMgTile();
	}
}

// The real map's mangroves are far too many to stand everywhere at once (53,000 rooted trees over Phases 1 and 2,
// 17 million triangles: 20 s of start-up and 0.8 GB when they were all built at launch). Here only the list of tiles
// that have any is made; StreamMangroves grows them as they come within sight.
void AKeysWorld::BuildMangroves()
{
	if (bMgJob) { delete GMgJob.Get(); bMgJob = false; }
	GMgLive.Reset();
	GMgHave.Reset();
	GMgMade = 0;
	GMgScanT = 0;
	if (!KeysGeo::RealMap()) return;
	const double Cell = KeysGeoReal::Cell();
	auto Have = [](double X, double Z) { GMgHave.Add(MgKey(X, Z), FVector2D(FMath::FloorToDouble(X / 600) * 600 + 300, FMath::FloorToDouble(Z / 600) * 600 + 300)); };
	for (int32 GJ = 0; GJ < KeysGeoReal::NZ(); GJ++)
		for (int32 GI = 0; GI < KeysGeoReal::NX(); GI++)
			if (KeysGeoReal::MangroveCell(GI, GJ)) Have(KeysGeoReal::X0() + GI * Cell, KeysGeoReal::Z0() + GJ * Cell);
	if (MgCreeksCut())
	{
		for (const KeysMangroveLife::FCreek& C : KeysMangroveLife::Creeks())
			for (double Z = C.Z0; Z < C.Z1 + 600; Z += 600)
				for (double X = C.X0; X < C.X1 + 600; X += 600) Have(FMath::Min(X, C.X1), FMath::Min(Z, C.Z1));
		for (const FVector2D& R : KeysMangroveLife::Roosts()) Have(R.X, R.Y);
		// and someone to keep the manatees in the calm bends
		GetWorld()->SpawnActor<AKeysMangroveLife>();
	}
	UE_LOG(LogTemp, Log, TEXT("Keys Run: mangroves - %d tiles have some; grown within 5 km of the boat"), GMgHave.Num());
	// and what lives in them (KeysMangroveEco; -KRNoMangroveEco leaves it out and stands the old still waders)
	if (KeysMangroveEco::Enabled()) GetWorld()->SpawnActor<AKeysMangroveEco>();
	// the nearest few at once (which also wakes every table the builder reads, before any is read off the game thread)
	GMgSync = 4;
	for (int32 i = 0; i < 4; i++) StreamMangroves();
	GMgSync = 0;
}

void AKeysWorld::StreamMangroves()
{
	if (!GMgHave.Num()) return;
	// the roots and trunks are only drawn within 700 m, and they are nearly half the triangles: a tile gets its
	// canopy when it comes within sight and its roots only when the boat is close, and loses them again behind it
	constexpr double RootsIn = 1150, RootsOut = 1600, CanopyIn = 5000, CanopyOut = 6200;
	const FVector2D Focus(FocusX, FocusZ);
	auto Commit = [this, &Focus](int64 Key, FMgTile* T)
	{
		const double T0 = FPlatformTime::Seconds();
		FMgLive& L = GMgLive.FindOrAdd(Key);
		const FVector2D* C = GMgHave.Find(Key);
		const bool bNear = C && FVector2D::DistSquared(*C, Focus) < FMath::Square(RootsIn + 150);
		int32 Tris = 0;
		for (int32 k = 0; k < 2; k++)
		{
			FTownBatch& B = k == 0 ? T->Roots : T->Canopy;
			if (L.PM[k].IsValid() || (k == 0 && !bNear)) continue;
			if (k == 0) L.bRoots = true;
			if (!B.Tris()) continue;
			UProceduralMeshComponent* PM = NewObject<UProceduralMeshComponent>(this);
			PM->SetupAttachment(Root);
			PM->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			PM->RegisterComponent();
			B.M.Commit(PM, 0, VertexColorMaterial());
			PM->SetCachedMaxDrawDistance(k == 0 ? 70000.f : 450000.f);
			L.PM[k] = PM;
			Tris += B.Tris();
		}
		GMgMade++;
		delete T;
		GMgLastMs = (FPlatformTime::Seconds() - T0) * 1000;
		GMgLastTris = Tris;
		GMgWatch = 2;
	};
	// (how long the frames round a tile's arrival took, for the log: a long one is a hitch the player feels)
	if (GMgWatch > 0 && --GMgWatch == 0)
	{
		const double Fr = GetWorld()->GetDeltaSeconds() * 1000;
		if (Fr > 60) UE_LOG(LogTemp, Log, TEXT("KRMangrove tile of %d triangles: set up in %.0f ms, the frame after took %.0f ms"), GMgLastTris, GMgLastMs, Fr);
	}
	if (bMgJob && GMgJob.IsReady()) { Commit(GMgJobKey, GMgJob.Get()); bMgJob = false; }
	if (!bMgJob)
	{
		// the nearest tile that lacks something it should have by now
		int64 Best = 0;
		double BestD = 1e18;
		bool bAny = false;
		for (const TPair<int64, FVector2D>& H : GMgHave)
		{
			const double D = FVector2D::DistSquared(H.Value, Focus);
			if (D >= BestD || D >= FMath::Square(CanopyIn)) continue;
			const FMgLive* L = GMgLive.Find(H.Key);
			if (L && (L->bRoots || D >= FMath::Square(RootsIn))) continue;
			BestD = D; Best = H.Key; bAny = true;
		}
		if (bAny && GMgSync > 0) Commit(Best, GenOneMangroveTile(Best));
		else if (bAny)
		{
			GMgJobKey = Best;
			bMgJob = true;
			GMgJob = Async(EAsyncExecution::ThreadPool, [Best] { return GenOneMangroveTile(Best); });
		}
	}
	// and what is left well behind is taken down again
	const double Now = GetWorld()->GetTimeSeconds();
	if (Now - GMgScanT > 1 || Now < GMgScanT)
	{
		GMgScanT = Now;
		for (auto It = GMgLive.CreateIterator(); It; ++It)
		{
			if (bMgJob && It.Key() == GMgJobKey) continue;
			const FVector2D* C = GMgHave.Find(It.Key());
			const double D = C ? FVector2D::DistSquared(*C, Focus) : 1e18;
			if (D > FMath::Square(RootsOut) && It.Value().bRoots)
			{
				if (It.Value().PM[0].IsValid()) It.Value().PM[0]->DestroyComponent();
				It.Value().PM[0] = nullptr;
				It.Value().bRoots = false;
			}
			if (D < FMath::Square(CanopyOut)) continue;
			for (TWeakObjectPtr<UProceduralMeshComponent>& PM : It.Value().PM) if (PM.IsValid()) PM->DestroyComponent();
			It.RemoveCurrent();
		}
	}
}

// ================================================================ what the ecosystem asks of the trees (KeysMangroveEco)
namespace KeysMangroveEco
{
	void MangroveTiles(TArray<TPair<int64, FVector2D>>& Out)
	{
		Out.Reset();
		for (const TPair<int64, FVector2D>& H : GMgHave) Out.Add(TPair<int64, FVector2D>(H.Key, H.Value));
	}

	// Every tree with its feet in the water in that tile, exactly where GenMangroves stands it (the same cells, hashes
	// and tests, none of the geometry), and the tops of the crowns a bird can stand on. Plain map reads: any thread.
	void SitesOfTile(int64 Key, TArray<FSite>& Sites, TArray<FVector>& Tops)
	{
		using namespace KeysGeo;
		const double Cell = KeysGeoReal::Cell();
		const bool bCreeks = MgCreeksCut();
		auto CreekD = [&](double X, double Z) { return bCreeks ? KeysMangroveLife::Near(X, Z).D : 1e9; };
		const int32 SeedI0 = KeysGeoReal::CellI0(), SeedJ0 = KeysGeoReal::CellJ0();
		const double TX0 = int32(Key >> 32) * 600.0, TZ0 = int32(uint32(Key)) * 600.0;
		// the top of the tallest clump of a rooted tree (Tree()'s boughs, without Lean or Tall)
		auto Apex = [](double X, double Z, double Ground) -> FVector
		{
			const double Base = FMath::Max(Ground, -.05), Sz = .8 + .5 * MHash(Z, X);
			const double Hh = (1.3 + .9 * MHash(X, Z)) * Sz, TrunkH = (2.3 + 1.5 * MHash(X + 4.0, Z)) * Sz;
			const FVector Mid(X + (MHash(X, 3.0) - .5) * 1.0, Base + Hh + TrunkH * .55, Z + (MHash(Z, 5.0) - .5) * 1.0);
			const double R = (2.5 + 1.0 * MHash(X * 1.3, Z)) * Sz, A0 = 2 * PI * MHash(X * 2.1, Z * 1.7);
			FVector Best(X, -1e9, Z);
			for (int32 b = 0; b < 3; b++)
			{
				const double H1 = MHash(X + b * 2.0, Z + 3.0), H2 = MHash(Z + b * 5.0, X + 1.0);
				const double A = A0 + 2 * PI * (b + .6 * H1) / 2, Out = b == 0 ? .25 * R : (.8 + .35 * H2) * R;
				const FVector End(Mid.X + FMath::Cos(A) * Out, Mid.Y + TrunkH * .45 + (b == 0 ? .75 : .1 + .55 * H1) * R, Mid.Z + FMath::Sin(A) * Out);
				const double Rc = R * (b == 0 ? 1.0 : .68 + .26 * H2), SY = .6 + .16 * H1, Seed = 6.28 * MHash(X + b * 3.0, Z * 1.9);
				const double Top = End.Y + Rc * .12 + Rc * SY * (.97 + .08 * MHash(Seed, 3.3));
				if (Top > Best.Y) Best = FVector(End.X, Top, End.Z);
			}
			return Best;
		};
		const int32 GI0 = FMath::Max(0, FMath::CeilToInt((TX0 - KeysGeoReal::X0()) / Cell)), GI1 = FMath::Min(KeysGeoReal::NX(), FMath::CeilToInt((TX0 + 600 - KeysGeoReal::X0()) / Cell));
		const int32 GJ0 = FMath::Max(0, FMath::CeilToInt((TZ0 - KeysGeoReal::Z0()) / Cell)), GJ1 = FMath::Min(KeysGeoReal::NZ(), FMath::CeilToInt((TZ0 + 600 - KeysGeoReal::Z0()) / Cell));
		for (int32 GJ = GJ0; GJ < GJ1; GJ++)
			for (int32 GI = GI0; GI < GI1; GI++)
			{
				const uint8 Mv = KeysGeoReal::MangroveCell(GI, GJ);
				if (!Mv) continue;
				const int32 I = GI + SeedI0, J = GJ + SeedJ0;
				const double CX = KeysGeoReal::X0() + GI * Cell, CZ = KeysGeoReal::Z0() + GJ * Cell;
				if (MgBlocked(CX, CZ)) continue;
				const double CellCD = CreekD(CX, CZ);
				if (CellCD < -2) continue;
				const bool bFringe = Mv < 200;
				const bool bEdge = bFringe || KeysGeoReal::Shore(CX, CZ) > -13 || CellCD < 15;
				if (!bEdge)
				{
					for (int32 k = 0; k < 2; k++)
					{
						const double X = CX + (MHash(I + k * 7.3, J) - .5) * Cell, Z = CZ + (MHash(J, I + k * 3.1) - .5) * Cell;
						const double R = 5.2 + 2.2 * MHash(I * 1.7, J + k), Y = FMath::Max(.3, LandH(X, Z)) + 3.8 + 3.0 * MHash(I + k, J * 2.3);
						Tops.Add(FVector(X, Y - R * .12 + R * .56 * (.97 + .08 * MHash(6.28 * MHash(I + k, J), 3.3)), Z));
					}
					continue;
				}
				if (!bFringe) continue;
				const int32 NTrees = 1 + int32(MHash(I, J * 1.3) * 1.6);
				for (int32 k = 0; k < NTrees; k++)
				{
					const double X = CX + (MHash(I + k * 5.1, J + 2.0) - .5) * Cell * .9, Z = CZ + (MHash(J + 4.0, I + k * 2.7) - .5) * Cell * .9;
					if (CreekD(X, Z) < 4.6) continue;
					FSite S;
					S.X = X; S.Z = Z; S.Ground = LandH(X, Z); S.Sz = .8 + .5 * MHash(Z, X); S.Hh = (1.3 + .9 * MHash(X, Z)) * S.Sz; S.Detail = 1;
					S.Top = Apex(X, Z, S.Ground);
					Sites.Add(S);
					Tops.Add(S.Top);
				}
			}
		// the creek banks: the front row (GenMangroves' lattice), one tree in three
		if (!bCreeks) return;
		const double Step = 3.3;
		const TArray<KeysMangroveLife::FCreek>& Creeks = KeysMangroveLife::Creeks();
		for (const KeysMangroveLife::FCreek& C : Creeks)
		{
			if (TX0 > C.X1 || TX0 + 600 < C.X0 || TZ0 > C.Z1 || TZ0 + 600 < C.Z0) continue;
			for (double Z = C.Z0; Z < C.Z1; Z += Step)
				for (double X = C.X0; X < C.X1; X += Step)
				{
					const double PX = X + (MHash(X, Z + 1.7) - .5) * Step * .8, PZ = Z + (MHash(Z, X + 2.9) - .5) * Step * .8;
					if (PX < TX0 || PX >= TX0 + 600 || PZ < TZ0 || PZ >= TZ0 + 600 || MHash(PX * 3.1, PZ * 1.3) > .34) continue;
					const KeysMangroveLife::FNear N = KeysMangroveLife::Near(PX, PZ);
					if (N.D < .5 || N.D > 4.4 || &Creeks[N.Creek] != &C || MgBlocked(PX, PZ) || MgOnLot(PX, PZ)) continue;
					FSite S;
					S.X = PX; S.Z = PZ; S.Ground = LandH(PX, PZ); S.Sz = .8 + .5 * MHash(PZ, PX); S.Detail = 2; S.Bank = 1; S.bRoof = N.bRoof;
					S.Hh = (1.3 + .9 * MHash(PX, PZ)) * S.Sz + (N.bRoof ? 1.4 : .3) * .5;
					S.In = (N.C - FVector2D(PX, PZ)).GetSafeNormal();
					S.MouthX = C.Pts[0].X; S.MouthZ = C.Pts[0].Y; S.Along = N.S; S.CX = N.C.X; S.CZ = N.C.Y;
					if (C.Roof1 <= C.Roof0 && N.S > C.Len * .5) { S.MouthX = C.Pts.Last().X; S.MouthZ = C.Pts.Last().Y; }
					Sites.Add(S);
				}
		}
	}

	// A point on prop root number Root of that tree, U of the way from the trunk (0) to the mud (1), along the straight
	// pieces it is drawn in. Roots 0 and 1 leave the foot of the trunk; this keeps to those.
	FVector RootPoint(const FSite& S, int32 Root, double U)
	{
		const double X = S.X, Z = S.Z, Sz = S.Sz, Hh = S.Hh;
		const double Base = FMath::Max(S.Ground, -.05), Foot = FMath::Min(S.Ground, -.35) - .25;
		const int32 NR = (S.Detail == 2 ? 11 : 8) + int32(MHash(X + 1.0, Z) * 2.5), Seg = S.Detail == 2 ? 4 : 3, r = Root % 3 == 2 ? Root + 1 : Root;
		const double A = 2 * PI * (r + MHash(X + r, Z)) / NR, Reach = (1.3 + 2.1 * MHash(X, Z + r)) * Sz;
		const FVector From = FVector(X, Base + Hh, Z) + FVector(0, -Hh * .55 * MHash(r, X), 0);
		const FVector End = FVector(X, Foot, Z) + FVector(FMath::Cos(A), 0, FMath::Sin(A)) * Reach;
		const FVector Ctrl(From.X + (End.X - From.X) * .78, From.Y + (.1 + .4 * MHash(Z, r)) * Sz, From.Z + (End.Z - From.Z) * .78);
		auto Bez = [&](double T) { return From * ((1 - T) * (1 - T)) + Ctrl * (2 * T * (1 - T)) + End * (T * T); };
		const double F = FMath::Clamp(U, 0.0, 1.0) * Seg;
		const int32 K = FMath::Min(Seg - 1, int32(F));
		return FMath::Lerp(Bez(double(K) / Seg), Bez(double(K + 1) / Seg), F - K) + FVector(0, .052 * Sz * (1 - .3 * U) * .9, 0);
	}
}
