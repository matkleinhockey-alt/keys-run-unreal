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
#include "ProceduralMeshComponent.h"

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
				const double Lump = 1 + .13 * FMath::Sin(Lon * 3 + Seed * 6.1) * FMath::Cos(Lat * 2.3 + Seed * 3.7) + .12 * (MHash(Seed + i * 1.37, j * 2.11 + Seed * .31) - .5);
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

void AKeysWorld::BuildMangroves()
{
	using namespace KeysGeo;
	if (!RealMap()) return;
	struct FTile { FTownBatch Roots, Canopy; };
	TMap<int64, FTile*> Tiles;
	auto TileAt = [&](double X, double Z) -> FTile&
	{
		FTile*& T = Tiles.FindOrAdd((int64(FMath::FloorToInt(X / 600)) << 32) ^ int64(uint32(FMath::FloorToInt(Z / 600))));
		if (!T) T = new FTile();
		return *T;
	};
	static const uint32 Leaf[] = {0x2f6b2f, 0x3a7a34, 0x285a2a, 0x376f2c, 0x2a6238};
	static const uint32 LeafTop[] = {0x4a8a3c, 0x55933f, 0x448238};
	const FLinearColor RootC = KeysTownPaint::Hex(0x6e4a30), RootDark = KeysTownPaint::Hex(0x4a3524), Trunk = KeysTownPaint::Hex(0x5c4836);
	const double Cell = KeysGeoReal::Cell();
	int32 Rooted = 0, Crowns = 0, Birds = 0, Pelicans = 0, BankTrees = 0, RoofSpans = 0;
	// the creeks are scenery only once the ground is cut for them (KeysGeo reads the same lines; request R1)
	const TArray<KeysMangroveLife::FCreek>& Creeks = KeysMangroveLife::Creeks();
	const bool bCreeks = Creeks.Num() && Creeks[0].Pts.Num() > 2 && CreekDist(Creeks[0].Pts[Creeks[0].Pts.Num() / 2].X, Creeks[0].Pts[Creeks[0].Pts.Num() / 2].Y) < 0;
	auto CreekD = [&](double X, double Z) { return bCreeks ? KeysMangroveLife::Near(X, Z).D : 1e9; };
	auto Blocked = [&](double X, double Z)
	{
		if (InFill(X, Z) || InTownPad(X, Z) || KeysTown::InFootprint(X, Z, 20) || SurfaceKind(X, Z) == 2 || InMarkedChannel(X, Z)) return true;
		for (const FMarina& M : Marinas()) if (FMath::Square(X - M.SX) + FMath::Square(Z - M.SZ) < 70.0 * 70.0) return true;
		return false;
	};

	// a red mangrove standing at (X, Z) on its prop roots; Lean carries its head that way (metres), Tall lifts the fork;
	// bClose: one you idle right past, so its crown is cut finer. Returns the top of its crown.
	auto Tree = [&](FTile& T, double X, double Z, double Ground, const FVector2D& Lean, double Tall, bool bClose = false) -> FVector
	{
		const double Base = FMath::Max(Ground, -.05), Foot = FMath::Min(Ground, -.35) - .25;   // the roots go on down into the mud
		const double Hh = 1.5 + 1.1 * MHash(X, Z) + Tall, Sz = .8 + .5 * MHash(Z, X);            // where the roots leave the trunk
		const FVector Top(X + (MHash(X, 3.0) - .5) * .8 + Lean.X, Base + Hh + 1.6 * Sz, Z + (MHash(Z, 5.0) - .5) * .8 + Lean.Y);
		const FVector Fork(X, Base + Hh, Z);
		Limb(T.Roots, Fork, Top, .11 * Sz, Trunk);
		// the prop roots: out, over, and down, each with a knee; a few drop straight from the branches
		const int32 NR = 5 + int32(MHash(X + 1.0, Z) * 3);
		for (int32 r = 0; r < NR; r++)
		{
			const double A = 2 * PI * (r + MHash(X + r, Z)) / NR, Reach = (1.6 + 1.7 * MHash(X, Z + r)) * Sz;
			const FVector Dir(FMath::Cos(A), 0, FMath::Sin(A));
			const FVector From = Fork + FVector(0, -Hh * .35 * MHash(r, X), 0);
			const FVector Knee = FVector(X, 0, Z) + Dir * Reach * .62 + FVector(0, Base + Hh * (.5 + .25 * MHash(Z, r)), 0);
			const FVector End = FVector(X, 0, Z) + Dir * Reach + FVector(0, Foot, 0);
			const FLinearColor C = FMath::Lerp(RootC, RootDark, float(MHash(r * 3.0, X + Z)) * .7f);
			Limb(T.Roots, From, Knee, .06 * Sz, C);
			Limb(T.Roots, Knee, End, .05 * Sz, C * .85f);
		}
		for (int32 r = 0; r < 2; r++)
		{
			const double A = 2 * PI * MHash(X + r * 9.0, Z + 1.0), Rr = (1.2 + 1.4 * MHash(Z + r, X)) * Sz;
			const FVector Hang = FVector(Top.X + FMath::Cos(A) * Rr, Top.Y + .3, Top.Z + FMath::Sin(A) * Rr);
			Limb(T.Roots, Hang, FVector(Hang.X, Foot, Hang.Z), .03, RootDark);
		}
		// the canopy: low, wide and dense - one broad dome and a second, lighter one shouldering out of it
		const double R = (3.0 + 1.3 * MHash(X * 1.3, Z)) * Sz;
		const FVector Apex(Top.X, Top.Y + R * .2 + R * .56 * .93, Top.Z);
		Crown(T.Canopy, FVector(Top.X, Top.Y + R * .2, Top.Z), R, .56, Leaf[int32(MHash(X, Z * 1.9) * 5) % 5], LeafTop[int32(MHash(Z, X * 1.1) * 3) % 3], 6.28 * MHash(X, Z * 1.9), bClose ? 12 : 9, bClose ? 5 : 4, true, bClose ? .4f : .55f);
		Crown(T.Canopy, FVector(Top.X + (MHash(X, 7.0) - .5) * R, Top.Y + R * .42, Top.Z + (MHash(Z, 9.0) - .5) * R), R * .66, .6,
			Leaf[int32(MHash(Z, X * 2.3) * 5) % 5], LeafTop[int32(MHash(X, Z * 1.7) * 3) % 3], 6.28 * MHash(Z, X * 1.3), bClose ? 10 : 7, bClose ? 4 : 3, true, bClose ? .4f : .55f);
		Rooted++;
		// fiddler and mangrove tree crabs on the roots: specks of red-orange and dark shell, a few to a tree
		for (int32 c = 0; c < 3 && MHash(X + 2.0, Z + c) < .6; c++)
		{
			const double A = 2 * PI * MHash(X + c * 4.0, Z - 1.0), Rr = (.5 + 1.2 * MHash(Z + c, X + 6.0)) * Sz;
			T.Roots.Ball(X + FMath::Cos(A) * Rr, Base + .25 + .9 * MHash(X - c, Z + 8.0), Z + FMath::Sin(A) * Rr, .045, c == 0 ? 0xc8482a : 0x4a2c22, .6, 4);
		}
		return Apex;
	};

	for (int32 J = 0; J < KeysGeoReal::NZ(); J++)
		for (int32 I = 0; I < KeysGeoReal::NX(); I++)
		{
			const uint8 Mv = KeysGeoReal::MangroveCell(I, J);
			if (!Mv) continue;
			const double CX = KeysGeoReal::X0() + I * Cell, CZ = KeysGeoReal::Z0() + J * Cell;
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
					const double R = 5.2 + 2.2 * MHash(I * 1.7, J + k), Y = FMath::Max(.3, LandH(X, Z)) + 3.2 + 1.6 * MHash(I + k, J * 2.3);
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
				const FVector Apex = Tree(T, X, Z, Ground, FVector2D::ZeroVector, 0);
				// a heron or an egret now and then, stalking the shallows just off the roots
				if (bFringe && k == 0 && MHash(I * 3.7, J * 1.9) < .07)
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

	if (bCreeks)
	{
		// estate lots keep their waterfront: nothing of the creek's is planted on one
		auto OnLot = [](double X, double Z)
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
		};
		// the banks: a row of trees down each side with their feet in the water, heads leaning in over the channel
		const double Step = 3.3;
		for (const KeysMangroveLife::FCreek& C : Creeks)
			for (double Z = C.Z0; Z < C.Z1; Z += Step)
				for (double X = C.X0; X < C.X1; X += Step)
				{
					const double PX = X + (MHash(X, Z + 1.7) - .5) * Step * .8, PZ = Z + (MHash(Z, X + 2.9) - .5) * Step * .8;
					const KeysMangroveLife::FNear N = KeysMangroveLife::Near(PX, PZ);
					// (a tunnel's banks are planted three deep, so no daylight comes in under the roof from the sides)
					if (N.D < .5 || N.D > (N.bRoof ? 10.5 : 4.4) || &Creeks[N.Creek] != &C || Blocked(PX, PZ) || OnLot(PX, PZ)) continue;
					const bool bFront = N.D < 4.4;
					FVector2D In = N.C - FVector2D(PX, PZ);
					In = In.GetSafeNormal() * (!bFront ? 0 : N.bRoof ? 1.8 : 1.1);
					const FVector Apex = Tree(TileAt(PX, PZ), PX, PZ, LandH(PX, PZ), In, !bFront ? .2 : N.bRoof ? 1.4 : .3, bFront);
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
			Pelicans += KeysMangroveLife::Snag(TileAt(R.X, R.Y).Roots, R.X, FMath::Min(Ground, -.3), R.Y, 7.0 + RoostN, 3);
			RoostN++;
		}
		// and someone to keep the manatees in the calm bends
		GetWorld()->SpawnActor<AKeysMangroveLife>();
	}

	int32 RootTris = 0, CanopyTris = 0;
	for (TPair<int64, FTile*>& T : Tiles)
	{
		for (int32 k = 0; k < 2; k++)
		{
			FTownBatch& B = k == 0 ? T.Value->Roots : T.Value->Canopy;
			if (!B.Tris()) continue;
			UProceduralMeshComponent* PM = NewObject<UProceduralMeshComponent>(this);
			PM->SetupAttachment(Root);
			PM->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			PM->RegisterComponent();
			B.M.Commit(PM, 0, VertexColorMaterial());
			PM->SetCachedMaxDrawDistance(k == 0 ? 70000.f : 450000.f);
			(k == 0 ? RootTris : CanopyTris) += B.Tris();
		}
		delete T.Value;
	}
	UE_LOG(LogTemp, Log, TEXT("Keys Run: mangroves - %d trees on prop roots (%d on creek banks), %d inland crowns, %d roof spans, %d wading birds, %d pelicans, %d tiles, %d root tris, %d canopy tris"),
		Rooted, BankTrees, Crowns, RoofSpans, Birds, Pelicans, Tiles.Num(), RootTris, CanopyTris);
}
