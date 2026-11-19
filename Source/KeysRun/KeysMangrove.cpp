// The real map's red mangroves, grown from the baked cover map (Tools/geo/bake_mangrove.py -> KeysGeoReal::MangroveCell).
//
//  - along the water (the shore's edge and the fringe wading out over the shallows): trees on arching prop roots that
//    come down into the water, a short crooked trunk, and a low, dense, dark canopy
//  - inland: canopy only, shoulder to shoulder, so a mangrove key reads as one solid green hummock
// One mesh per 600 m square and per kind: the roots and trunks are drawn within 700 m (they are thin; past that they
// are gone anyway), the canopy to 4.5 km.
#include "KeysWorld.h"
#include "KeysGeo.h"
#include "KeysGeoReal.h"
#include "KeysTown.h"
#include "KeysTownData.h"
#include "ProceduralMeshComponent.h"

namespace
{
	double MHash(double A, double B) { const double H = FMath::Sin(A * 127.1 + B * 311.7) * 43758.5453; return H - FMath::FloorToDouble(H); }

	// a three-sided limb from A to B (game metres, y up), R thick
	void Limb(FTownBatch& B, const FVector& P0, const FVector& P1, double R, const FLinearColor& C)
	{
		const FVector D = (P1 - P0).GetSafeNormal();
		FVector U = FVector::CrossProduct(D, FMath::Abs(D.Y) < .9 ? FVector(0, 1, 0) : FVector(1, 0, 0)).GetSafeNormal();
		const FVector V = FVector::CrossProduct(D, U);
		FVector Rim[3];
		for (int32 k = 0; k < 3; k++) { const double A = 2 * PI * k / 3; Rim[k] = U * FMath::Cos(A) * R + V * FMath::Sin(A) * R; }
		for (int32 k = 0; k < 3; k++)
		{
			const FVector& Ra = Rim[k];
			const FVector& Rb = Rim[(k + 1) % 3];
			B.Quad3(P0 + Ra, P0 + Rb, P1 + Rb * .8, P1 + Ra * .8, (Ra + Rb).GetSafeNormal(), C);
		}
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
	int32 Rooted = 0, Crowns = 0;
	for (int32 J = 0; J < KeysGeoReal::NZ(); J++)
		for (int32 I = 0; I < KeysGeoReal::NX(); I++)
		{
			const uint8 Mv = KeysGeoReal::MangroveCell(I, J);
			if (!Mv) continue;
			const double CX = KeysGeoReal::X0() + I * Cell, CZ = KeysGeoReal::Z0() + J * Cell;
			if (InFill(CX, CZ) || InTownPad(CX, CZ) || KeysTown::InFootprint(CX, CZ, 20) || SurfaceKind(CX, CZ) == 2) continue;
			bool bMarina = false;
			for (const FMarina& M : Marinas()) bMarina |= FMath::Square(CX - M.SX) + FMath::Square(CZ - M.SZ) < 70.0 * 70.0;
			if (bMarina || InMarkedChannel(CX, CZ)) continue;
			const bool bFringe = Mv < 200;
			const double Sd = KeysGeoReal::Shore(CX, CZ);
			const bool bEdge = bFringe || Sd > -13;
			FTile& T = TileAt(CX, CZ);
			if (!bEdge)
			{
				// inland: two overlapping crowns to a cell, no trunks to see
				for (int32 k = 0; k < 2; k++)
				{
					const double X = CX + (MHash(I + k * 7.3, J) - .5) * Cell, Z = CZ + (MHash(J, I + k * 3.1) - .5) * Cell;
					const double R = 5.2 + 2.2 * MHash(I * 1.7, J + k), Y = FMath::Max(.3, LandH(X, Z)) + 3.2 + 1.6 * MHash(I + k, J * 2.3);
					T.Canopy.Ball(X, Y, Z, R, Leaf[int32(MHash(I, J + k * 5.0) * 5) % 5], .5, 6);
					Crowns++;
				}
				continue;
			}
			// along the water: trees on prop roots
			const int32 NTrees = bFringe ? 1 + int32(MHash(I, J * 1.3) * 1.6) : 2;
			for (int32 k = 0; k < NTrees; k++)
			{
				const double X = CX + (MHash(I + k * 5.1, J + 2.0) - .5) * Cell * .9, Z = CZ + (MHash(J + 4.0, I + k * 2.7) - .5) * Cell * .9;
				const double Ground = LandH(X, Z);
				if (!bFringe && Ground < -.6) continue;
				const double Base = FMath::Max(Ground, -.05), Foot = FMath::Min(Ground, -.35) - .25;   // the roots go on down into the mud
				const double Hh = 1.5 + 1.1 * MHash(X, Z), Sz = .8 + .5 * MHash(Z, X);                    // where the roots leave the trunk
				const FVector Top(X + (MHash(X, 3.0) - .5) * .8, Base + Hh + 1.6 * Sz, Z + (MHash(Z, 5.0) - .5) * .8);
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
					const FVector Hang = FVector(X + FMath::Cos(A) * Rr, Top.Y + .3, Z + FMath::Sin(A) * Rr);
					Limb(T.Roots, Hang, FVector(Hang.X, Foot, Hang.Z), .03, RootDark);
				}
				// the canopy: low, wide and dense, lighter on top
				const double R = (3.0 + 1.3 * MHash(X * 1.3, Z)) * Sz;
				T.Canopy.Ball(Top.X, Top.Y + R * .22, Top.Z, R, Leaf[int32(MHash(X, Z * 1.9) * 5) % 5], .5, 7);
				T.Canopy.Ball(Top.X + (MHash(X, 7.0) - .5) * R, Top.Y + R * .5, Top.Z + (MHash(Z, 9.0) - .5) * R, R * .62, LeafTop[int32(MHash(Z, X * 1.1) * 3) % 3], .5, 6);
				Rooted++;
			}
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
	UE_LOG(LogTemp, Log, TEXT("Keys Run: mangroves - %d trees on prop roots, %d inland crowns, %d tiles, %d root tris, %d canopy tris"), Rooted, Crowns, Tiles.Num(), RootTris, CanopyTris);
}
