#include "KeysTown.h"
#include "KeysTownData.h"
#include "Components/TextRenderComponent.h"

// The walk-in rooms: entities/town/interiors.ts (ROOMS, the shell, lamps, booths, stools), shops.ts (furnishShop: the stock on
// every shelf) and casino-interior.ts / casino-aquarium.ts (the Gaff & Gill floor), built from the same INTERIOR_BOXES the
// walker collides with.

using namespace KeysTown;
using KeysTownPaint::Hex;

namespace
{
	constexpr double P = KeysTown::PadY;
	inline double Zf(double V) { return -V; }
	double H1(double X) { const double H = FMath::Sin(X * 12.9898 + 4.1) * 43758.5453; return H - FMath::FloorToDouble(H); }

	struct FRoom { const TCHAR* Lot; uint32 Wall, Trim, Ceil; int32 Floor; double CeilMax; uint32 Shade; };
	// floor: 0 boards, 1 concrete, 2 checker, 3 tile, 4 carpet, 5 lino
	const FRoom* RoomOf(const FString& Lot)
	{
		static const FRoom Rooms[] = {
			{TEXT("bar"), 0x9a7048, 0x4a3524, 0x4a3826, 0, 99, 0x2f6b4a}, {TEXT("tarpon"), 0x7d5a3a, 0x3a2a1a, 0x3c2e20, 0, 99, 0x2f6b4a},
			{TEXT("hammered"), 0x8e9aa0, 0x2b3338, 0x4a5257, 1, 4.6, 0xd9342b}, {TEXT("diner"), 0xf1e8d6, 0xc23a3a, 0xe6e2d6, 2, 99, 0xc23a3a},
			{TEXT("dive"), 0x4aa6b8, 0xf1f4f5, 0xe6f0f2, 3, 4.4, 0x1ea5b8}, {TEXT("tackle"), 0xd8c9a0, 0x1f4f8c, 0xe9e4d4, 0, 4.6, 0x1f4f8c},
			{TEXT("guns"), 0x7a4a3c, 0x16110f, 0x3a3532, 1, 4.4, 0x2a2d31}, {TEXT("liquor"), 0xe8c868, 0xb3202a, 0xeee6c8, 2, 4.0, 0xb3202a},
			{TEXT("gifts"), 0xf6b8a8, 0x2bb3a8, 0xfff2dc, 0, 4.2, 0x2bb3a8}, {TEXT("pawn"), 0x8a8860, 0xf0c64a, 0x6a665a, 1, 4.4, 0xb3202a},
			{TEXT("laundromat"), 0xcfe6f4, 0x1d6fb3, 0xf2f6f8, 2, 4.0, 0xdff3ff}, {TEXT("icecream"), 0xf7c6d6, 0x7fd8d0, 0xfff6ee, 3, 3.8, 0xf7a7c6},
			{TEXT("motel"), 0xe6d3b0, 0x7a3a2c, 0xd8d0be, 4, 4.6, 0xe0b34a}, {TEXT("stopngo"), 0xeef1f2, 0xd9342b, 0xf4f6f6, 5, 4.2, 0xdff3ff},
			{TEXT("club"), 0x1a0d18, 0x5a1340, 0x120812, 0, 99, 0xff3ea5},
		};
		for (const FRoom& R : Rooms) if (Lot == R.Lot) return &R;
		return nullptr;
	}

	// a shelf of stock: rows of little coloured boxes, the colours picked per shop
	void Stock(FTownBatch& B, double U0, double U1, double Y0, double Y1, double Z0, double Z1, const TArray<uint32>& Cols, int32 Seed)
	{
		const int32 Rows = FMath::Max(1, FMath::FloorToInt((Y1 - Y0) / .42));
		const bool bAlongU = FMath::Abs(U1 - U0) >= FMath::Abs(Z1 - Z0);
		const double Len = bAlongU ? U1 - U0 : FMath::Abs(Z1 - Z0);
		for (int32 r = 0; r < Rows; r++)
		{
			const double Y = Y0 + r * (Y1 - Y0) / Rows;
			B.Span(U0, U1, Y, Y + .03, Z0, Z1, 0x6b5a46);
			double S = .05;
			int32 k = 0;
			while (S < Len - .1)
			{
				const double W = .12 + H1(Seed + r * 31 + k * 7) * .22, Hh = .16 + H1(Seed + r * 17 + k * 3) * .2;
				const uint32 C = Cols[(Seed + k + r) % Cols.Num()];
				if (bAlongU) B.Span(U0 + S, U0 + S + W, Y + .03, Y + .03 + Hh, Z0 + .05, Z1 - .05, C);
				else { const double ZA = FMath::Min(Z0, Z1); B.Span(U0 + .05, U1 - .05, Y + .03, Y + .03 + Hh, ZA + S, ZA + S + W, C); }
				S += W + .03;
				k++;
			}
		}
	}
}

void AKeysTown::BuildInteriors(FTownBatch& I, FTownBatch& G)
{
	for (const FLot& L : Lots())
	{
		if (L.Style == EStyle::Casino) continue;
		const FRoom* R = RoomOf(L.Id);
		if (!R) continue;
		const double H = FMath::Min(L.H, R->CeilMax), Y0 = P + .07;
		const double X0 = L.U0 + WallT, X1 = L.U1 - WallT, V0 = L.V0 + WallT, V1 = L.V1 - WallT;
		const double Sk = .03;
		// the floor
		{
			const double Tile = R->Floor == 2 ? .6 : R->Floor == 3 ? .5 : R->Floor == 0 ? .25 : 1.0;
			const uint32 FA = R->Floor == 0 ? 0x8a6340 : R->Floor == 1 ? 0x8d8a84 : R->Floor == 2 ? 0xf4f4f2 : R->Floor == 3 ? 0xdfe8ea : R->Floor == 4 ? 0x7a3a3a : 0xd9d4c8;
			const uint32 FB = R->Floor == 0 ? 0x7a5536 : R->Floor == 1 ? 0x85827c : R->Floor == 2 ? 0x22262c : R->Floor == 3 ? 0xc8d6da : R->Floor == 4 ? 0x6e3434 : 0xcfcabc;
			if (R->Floor == 0)
				for (double V = V0; V < V1 - .01; V += Tile) I.Flat(X0, X1, Zf(FMath::Min(V + Tile, V1)), Zf(V), Y0, Hex(int32((V - V0) / Tile) % 2 ? FA : FB));
			else
				for (double U = X0; U < X1 - .01; U += Tile)
					for (double V = V0; V < V1 - .01; V += Tile)
						I.Flat(U, FMath::Min(U + Tile, X1), Zf(FMath::Min(V + Tile, V1)), Zf(V), Y0, Hex((int32((U - X0) / Tile) + int32((V - V0) / Tile)) % 2 ? FA : FB));
		}
		// wall skins, the ceiling, a wainscot
		const double CY = P + H - .1;
		I.Span(X0, X1, Y0, CY, Zf(V1) + Sk, Zf(V1) + Sk * 2, R->Wall);
		I.Span(X0, X0 + Sk, Y0, CY, Zf(V1), Zf(V0), R->Wall);
		I.Span(X1 - Sk, X1, Y0, CY, Zf(V1), Zf(V0), R->Wall);
		const double DL = L.Door - DoorW / 2, DR = L.Door + DoorW / 2;
		I.Span(X0, DL, Y0, CY, Zf(V0) - Sk * 2, Zf(V0) - Sk, R->Wall);
		I.Span(DR, X1, Y0, CY, Zf(V0) - Sk * 2, Zf(V0) - Sk, R->Wall);
		I.Quad3(FVector(X0, CY, Zf(V0)), FVector(X1, CY, Zf(V0)), FVector(X1, CY, Zf(V1)), FVector(X0, CY, Zf(V1)), FVector(0, -1, 0), Hex(R->Ceil));
		I.Span(X0 + Sk, X1 - Sk, Y0, Y0 + 1.0, Zf(V1) + .04, Zf(V1) + .06, R->Trim);
		I.Span(X0 + Sk, X0 + Sk * 2, Y0, Y0 + 1.0, Zf(V1), Zf(V0), R->Trim);
		I.Span(X1 - Sk * 2, X1 - Sk, Y0, Y0 + 1.0, Zf(V1), Zf(V0), R->Trim);
		if (L.Style == EStyle::Bar) for (double U = X0 + 3; U < X1; U += 6) I.Span(U, U + .35, P + H - .5, P + H - .12, Zf(V1), Zf(V0), 0x1f160e);
		// ceiling lamps: pendants in the bars and the diner, tubes everywhere else
		const bool bBell = L.Style == EStyle::Bar || L.Style == EStyle::Diner || L.Style == EStyle::Club;
		for (double U = X0 + 3.5; U < X1 - 1; U += 7)
			for (double V = V0 + 3; V < V1 - 1; V += 6)
			{
				if (bBell)
				{
					I.Cyl(U, P + H - 1.15, Zf(V), .02, .02, 1.0, 0x1f160e, 4);
					I.Cyl(U, P + H - 1.7, Zf(V), .1, .36, .55, R->Shade, 10);
					G.Ball(U, P + H - 1.72, Zf(V), .12, L.Style == EStyle::Club ? 0xff3ea5 : 0xffd9a0, 1, 6);
				}
				else
				{
					I.Span(U - 1.1, U + 1.1, CY - .14, CY - .06, Zf(V) - .14, Zf(V) + .14, 0xf2f6f8);
					G.Quad3(FVector(U - 1.0, CY - .145, Zf(V) + .1), FVector(U + 1.0, CY - .145, Zf(V) + .1), FVector(U + 1.0, CY - .145, Zf(V) - .1), FVector(U - 1.0, CY - .145, Zf(V) - .1), FVector(0, -1, 0), Hex(0xfdfbf0));
				}
			}
		// the shop's name on the back wall, so you know which room you walked into
		if (const FVenue* Vn = Venue(L.Venue))
			AddSign(FString(Vn->Name).ToUpper(), (X0 + X1) / 2, FMath::Min(P + H - .55, P + 3.1), V1 - .08, .5, Hex(R->Trim), 0, true, FMath::Min(X1 - X0 - 1, 9.0));
	}

	// ------------------------------------------------------------------ the furniture, from the same boxes the walker hits
	const TMap<FString, TArray<uint32>> StockCols = {
		{TEXT("tackle"), {0x1f6fb0, 0xe4572e, 0xf2c14e, 0x2bb3a8, 0xffffff, 0x23262a}},
		{TEXT("guns"), {0x3a3d40, 0x6b5a3a, 0x2d3136, 0xb8a070}},
		{TEXT("liquor"), {0x6b1a1a, 0x2a6b2a, 0xe0b34a, 0x8a5a36, 0xd9d4c0, 0x1d2430}},
		{TEXT("gifts"), {0xf26b5b, 0x3fb6c9, 0xf2c14e, 0x7bc96f, 0xe86aa8, 0xffffff}},
		{TEXT("pawn"), {0xe0b34a, 0x8a5a36, 0x3a3d40, 0xc0c4c8}},
		{TEXT("laundromat"), {0x1d6fb3, 0xffffff, 0xf2c14e}},
		{TEXT("icecream"), {0xf7a7c6, 0xfff3e0, 0x7bc96f, 0x8a5a36}},
		{TEXT("motel"), {0xe6d3b0, 0x7a3a2c}},
		{TEXT("stopngo"), {0xd9342b, 0xf2c14e, 0x2b6cb0, 0x7bc96f, 0xff9f43, 0xffffff}},
		{TEXT("dive"), {0xf2c14e, 0x1ea5b8, 0x23262a, 0xffffff}},
		{TEXT("bar"), {0x6b3a1a, 0x2a6b2a, 0xe0b34a, 0xd9d4c0}},
		{TEXT("tarpon"), {0x6b3a1a, 0x2a6b2a, 0xe0b34a, 0xd9d4c0}},
		{TEXT("hammered"), {0x6b3a1a, 0x2a6b2a, 0xe0b34a, 0xd9d4c0}},
		{TEXT("diner"), {0xf4f4f2, 0xc23a3a, 0xe0b34a}},
		{TEXT("club"), {0xff3ea5, 0x8a3cff, 0xe0b34a, 0x38e0ff}},
	};
	int32 Seed = 1;
	for (const FInteriorBox& B : InteriorBoxes())
	{
		const FString Id = B.Id, LotId = B.Lot;
		if (LotId == TEXT("casino")) continue;
		const FLot* L = Lot(LotId);
		const FRoom* R = RoomOf(LotId);
		if (!L || !R) continue;
		const TArray<uint32>& Cols = StockCols.FindChecked(LotId);
		const double Y0 = P + .07, H = FMath::Min(L->H, R->CeilMax);
		const double U0 = B.U0, U1 = B.U1, ZA = Zf(B.V1), ZB = Zf(B.V0), UC = (U0 + U1) / 2, ZC = (ZA + ZB) / 2;
		Seed += 13;
		const bool bBarLike = L->Style == EStyle::Bar || L->Style == EStyle::Club;
		if (Id.Contains(TEXT("counter")) || Id.Contains(TEXT("desk")))
		{
			// a counter: body, a top that overhangs on the customer's side, and the shelf wall behind the keeper
			const uint32 BodyC = bBarLike ? 0x5a3a22 : R->Trim, TopC = bBarLike ? 0x8a5a36 : 0xe8e4d8;
			I.Span(U0, U1, Y0, Y0 + 1.0, ZA, ZB, BodyC);
			I.Span(U0 - .05, U1 + .05, Y0 + 1.0, Y0 + 1.07, ZA + .05, ZB + .15, TopC);
			if (Id != TEXT("club-counter") && !Id.Contains(TEXT("tarpon")) && !Id.Contains(TEXT("hammered")))
			{
				const double BackV = FMath::Min(L->V1 - WallT - .05, B.V1 + 2.6);
				const double SU0 = FMath::Max(L->U0 + WallT + .1, U0), SU1 = FMath::Min(L->U1 - WallT - .1, U1);
				I.Span(SU0, SU1, Y0, Y0 + FMath::Min(2.4, H - .5), Zf(BackV), Zf(BackV - .08), 0x6b5a46);
				Stock(I, SU0 + .1, SU1 - .1, Y0 + .5, Y0 + FMath::Min(2.3, H - .6), Zf(BackV - .08), Zf(BackV - .45), Cols, Seed);
			}
			else
			{
				// a bar back: bottles along a mirrored shelf
				const double BackV = L->V1 - WallT - .05;
				I.Span(U0, U1, Y0 + 1.2, Y0 + 1.25, Zf(BackV), Zf(BackV - .35), 0x6b5a46);
				I.Span(U0, U1, Y0 + 1.25, Y0 + 2.2, Zf(BackV), Zf(BackV - .02), 0x9fb4bf);
				for (double U = U0 + .2; U < U1 - .1; U += .16) I.Cyl(U, Y0 + 1.25, Zf(BackV - .18), .035, .045, .3 + H1(U) * .12, Cols[int32(H1(U * 3) * Cols.Num()) % Cols.Num()], 5);
			}
			// bar stools along the customer side of a bar, and the till
			if (bBarLike || LotId == TEXT("diner") || LotId == TEXT("tarpon") || LotId == TEXT("hammered"))
				for (double U = U0 + .6; U < U1 - .3; U += 1.1)
				{
					I.Cyl(U, Y0, ZB + .55, .05, .06, .66, 0x9aa0a6, 6);
					I.Cyl(U, Y0 + .66, ZB + .55, .22, .22, .1, LotId == TEXT("diner") ? 0xc23a3a : 0x2b2118, 10);
				}
			else
			{
				I.Box(UC, Y0 + 1.2, ZC, .45, .28, .4, 0x2d3136);
				I.Box(UC, Y0 + 1.42, ZC - .1, .4, .2, .04, 0x1d2a33);
			}
		}
		else if (Id.Contains(TEXT("booth")))
		{
			const bool bWest = (U0 - L->U0) < (L->U1 - U1);
			const uint32 Seat = LotId == TEXT("diner") ? 0xc23a3a : LotId == TEXT("club") ? 0x5a1340 : 0x6b3a22;
			I.Span(U0, U1, Y0, Y0 + .5, Zf(B.V0 + .95), Zf(B.V0), Seat);
			I.Span(U0, U1, Y0, Y0 + .5, Zf(B.V1), Zf(B.V1 - .95), Seat);
			I.Span(U0, U1, Y0 + .5, Y0 + 1.3, Zf(B.V0 + .25), Zf(B.V0), Seat);
			I.Span(U0, U1, Y0 + .5, Y0 + 1.3, Zf(B.V1), Zf(B.V1 - .25), Seat);
			(void)bWest;
			I.Span(U0 + .35, U1 - .35, Y0 + .74, Y0 + .8, Zf(B.V1 - 1.05), Zf(B.V0 + 1.05), bBarLike ? 0x8a5a36 : 0xf4f4f2);
			I.Cyl(UC, Y0, ZC, .07, .07, .74, 0x9aa0a6, 6);
		}
		else if (Id == TEXT("bar-pool"))
		{
			I.Span(U0 + .2, U1 - .2, Y0 + .7, Y0 + .85, ZA + .2, ZB - .2, 0x1f6b3a);
			I.Span(U0 + .1, U1 - .1, Y0 + .78, Y0 + .9, ZA + .1, ZA + .3, 0x5a3a22);
			I.Span(U0 + .1, U1 - .1, Y0 + .78, Y0 + .9, ZB - .3, ZB - .1, 0x5a3a22);
			for (double DU : {.4, -.4}) for (double DZ : {.4, -.4}) I.Box(DU > 0 ? U0 + .4 : U1 - .4, Y0 + .35, DZ > 0 ? ZA + .4 : ZB - .4, .2, .7, .2, 0x3a2614);
			const uint32 BC[] = {0xffffff, 0xf2c14e, 0xd9342b, 0x2b6cb0, 0x15171a};
			for (int32 k = 0; k < 7; k++) I.Ball(UC + (H1(k * 3.3) - .5) * 3, Y0 + .9, ZC + (H1(k * 5.1) - .5) * 2.4, .05, BC[k % 5], 1, 6);
		}
		else if (Id == TEXT("club-stage"))
		{
			I.Span(U0, U1, Y0, Y0 + .6, ZA, ZB, 0x2a1428);
			I.Span(U0, U1, Y0 + .6, Y0 + .64, ZA, ZB, 0x15101a);
			G.Span(U0, U1, Y0 + .5, Y0 + .56, ZB, ZB + .03, 0xff3ea5);
			for (double PU : {47.0, 55.0}) I.Cyl(PU, Y0 + .6, Zf(67), .04, .04, H - .7, 0xe8ebee, 8);
			// a curtain across the back
			for (double U = U0; U < U1; U += .6) I.Span(U, U + .55, Y0 + .6, P + H - .2, ZA + .05, ZA + .2 + (int32(U) % 2) * .06, 0x8a1340);
		}
		else if (Id == TEXT("club-dj"))
		{
			I.Span(U0, U1, Y0, Y0 + .45, ZA, ZB, 0x2a1428);
			I.Span(U0 + .4, U1 - .4, Y0 + .45, Y0 + 1.4, ZB - .9, ZB - .3, 0x15171a);
			for (double DU : {-.8, .8}) I.Cyl(UC + DU, Y0 + 1.4, ZB - .6, .25, .25, .04, 0x3a3d40, 12);
			for (double DU : {U0 + .3, U1 - .3}) I.Span(DU - .3, DU + .3, Y0 + .45, Y0 + 2.0, ZA + .1, ZA + .7, 0x15171a);
			G.Span(U0 + .45, U1 - .45, Y0 + 1.36, Y0 + 1.4, ZB - .32, ZB - .28, 0x38e0ff);
		}
		else if (Id == TEXT("club-bar"))
		{
			I.Span(U0, U1, Y0, Y0 + 1.05, ZA, ZB, 0x2a1428);
			I.Span(U0 - .1, U1 + .15, Y0 + 1.05, Y0 + 1.12, ZA, ZB, 0xe0b34a);
			G.Span(U1 + .01, U1 + .03, Y0 + .1, Y0 + .15, ZA, ZB, 0xff3ea5);
			for (double V = B.V0 + .6; V < B.V1; V += 1.2) { I.Cyl(U1 + .6, Y0, Zf(V), .05, .06, .66, 0x9aa0a6, 6); I.Cyl(U1 + .6, Y0 + .66, Zf(V), .22, .22, .1, 0xff3ea5, 10); }
			const double BackU = L->U0 + WallT + .2;
			for (double V = B.V0 + .3; V < B.V1 - .2; V += .18) I.Cyl(BackU, Y0 + 1.3, Zf(V), .035, .045, .32, Cols[int32(H1(V) * Cols.Num()) % Cols.Num()], 5);
		}
		else if (Id.Contains(TEXT("rods")) || Id.Contains(TEXT("spears")) || Id.Contains(TEXT("guitars")))
		{
			I.Span(U0, U1, Y0, Y0 + .3, ZA, ZB, 0x6b5a46);
			const bool bGuitar = Id.Contains(TEXT("guitars"));
			for (double V = B.V0 + .25; V < B.V1 - .1; V += bGuitar ? .8 : .22)
			{
				if (bGuitar)
				{
					I.Box(UC, Y0 + 1.1, Zf(V), .12, .9, .45, Cols[int32(V) % Cols.Num()]);
					I.Box(UC, Y0 + 1.9, Zf(V), .06, .8, .08, 0x3a2614);
				}
				else I.Cyl(UC, Y0 + .3, Zf(V), .015, .025, Id.Contains(TEXT("spears")) ? 1.4 : 2.4, Id.Contains(TEXT("spears")) ? 0xc0c4c8 : 0x23262a, 5, 0, 0, .05);
			}
		}
		else if (Id.Contains(TEXT("bait")))
		{
			I.Span(U0, U1, Y0, Y0 + .9, ZA, ZB, 0xe8edf1);
			I.Span(U0 + .1, U1 - .1, Y0 + .7, Y0 + .88, ZA + .1, ZB - .1, 0x3a8aa8);
		}
		else if (Id.Contains(TEXT("freezer")) || Id.Contains(TEXT("cooler")) || Id.Contains(TEXT("vending")) || Id.Contains(TEXT("snacks")) || Id.Contains(TEXT("wine")) || Id.Contains(TEXT("tools")) || Id.Contains(TEXT("hotsauce")) || Id.Contains(TEXT("shirt-wall")) || Id.Contains(TEXT("suits")) || Id.Contains(TEXT("tanks")) || Id.Contains(TEXT("cage")))
		{
			// a wall unit: cabinet with glass doors, or open shelving with stock on it
			const double Ht = FMath::Min(2.3, H - .4);
			const bool bCold = Id.Contains(TEXT("cooler")) || Id.Contains(TEXT("freezer"));
			I.Span(U0, U1, Y0, Y0 + Ht, ZA, ZB, bCold ? 0xe8edf1 : Id.Contains(TEXT("vending")) ? 0xd9342b : 0x6b5a46);
			const bool bAlongU = (U1 - U0) > (B.V1 - B.V0);
			// which face looks into the room
			const double Cu = (L->U0 + L->U1) / 2, Cv = (L->V0 + L->V1) / 2;
			if (bAlongU)
			{
				const double Face = (B.V0 + B.V1) / 2 < Cv ? ZB + .01 : ZA - .01;
				Stock(I, U0 + .1, U1 - .1, Y0 + .3, Y0 + Ht - .15, Face - .02, Face + .02 * (Face > ZC ? 1 : -1) - .3 * (Face > ZC ? 1 : -1), Cols, Seed);
			}
			else
			{
				const bool bEast = UC > Cu;
				const double FaceU = bEast ? U0 - .01 : U1 + .01;
				if (Id.Contains(TEXT("tanks")))
					for (double V = B.V0 + .3; V < B.V1 - .2; V += .4) I.Cyl(FaceU + (bEast ? -.25 : .25), Y0, Zf(V), .15, .15, 1.1, Cols[int32(V * 3) % Cols.Num()], 8);
				else if (Id.Contains(TEXT("suits")))
					for (double V = B.V0 + .3; V < B.V1 - .2; V += .5) I.Box(FaceU + (bEast ? -.2 : .2), Y0 + 1.2, Zf(V), .08, 1.4, .45, 0x15171a);
				else
				{
					const double SU0 = bEast ? FaceU - .32 : FaceU, SU1 = bEast ? FaceU : FaceU + .32;
					Stock(I, SU0, SU1, Y0 + .3, Y0 + Ht - .15, Zf(B.V0 + .1), Zf(B.V1 - .1), Cols, Seed);
					if (bCold) for (double V = B.V0 + 1; V < B.V1; V += 1.2) G.Box(FaceU + (bEast ? -.02 : .02), Y0 + Ht * .5, Zf(V), .02, Ht - .4, .04, 0xe8f6ff);
				}
			}
			(void)Cu;
		}
		else if (Id.Contains(TEXT("aisle")) || Id.Contains(TEXT("ammo")))
		{
			// a gondola: stock both sides
			I.Span(U0, U1, Y0, Y0 + 1.6, ZA + .2, ZB - .2, 0x6b5a46);
			Stock(I, U0 + .1, U1 - .1, Y0 + .25, Y0 + 1.55, ZA - .02, ZA + .2, Cols, Seed);
			Stock(I, U0 + .1, U1 - .1, Y0 + .25, Y0 + 1.55, ZB - .2, ZB + .02, Cols, Seed + 5);
		}
		else if (Id.Contains(TEXT("washers")) || Id.Contains(TEXT("dryers")))
		{
			const bool bAlongU = (U1 - U0) > (B.V1 - B.V0);
			const int32 N = FMath::Max(1, FMath::FloorToInt((bAlongU ? U1 - U0 : B.V1 - B.V0) / .8));
			for (int32 k = 0; k < N; k++)
			{
				const double A = (k + .5) / N;
				const double Uu = bAlongU ? FMath::Lerp(U0, U1, A) : UC, Zz = bAlongU ? ZC : FMath::Lerp(ZA, ZB, A);
				I.Box(Uu, Y0 + .48, Zz, bAlongU ? .74 : U1 - U0, .95, bAlongU ? (B.V1 - B.V0) : .74, 0xf4f6f8);
				if (bAlongU) I.Cyl(Uu, Y0 + .45 - .02, ZB + .02, .22, .22, .04, 0x2d3136, 12, PI / 2);
				else I.Cyl(UC > (L->U0 + L->U1) / 2 ? U0 - .02 : U1 + .02, Y0 + .45, Zz, .22, .22, .04, 0x2d3136, 12, 0, 0, PI / 2);
			}
		}
		else if (Id.Contains(TEXT("shirts")))
		{
			I.Span(U0, U1, Y0 + 1.5, Y0 + 1.55, ZC - .02, ZC + .02, 0xc0c4c8);
			for (double U = U0 + .1; U < U1 - .1; U += .14) I.Box(U, Y0 + 1.05, ZC, .04, .85, .6, Cols[int32(U * 7) % Cols.Num()]);
			for (double U : {U0 + .05, U1 - .05}) I.Cyl(U, Y0, ZC, .02, .02, 1.55, 0xc0c4c8, 5);
		}
		else if (Id.Contains(TEXT("sofa")))
		{
			I.Span(U0, U1, Y0, Y0 + .45, ZA, ZB, 0x2f6b8f);
			I.Span(U0, U1, Y0 + .45, Y0 + .9, ZA, ZA + .3, 0x2f6b8f);
		}
		else if (Id.Contains(TEXT("stairs")))
		{
			const int32 N = 14;
			for (int32 k = 0; k < N; k++) I.Span(U0, U1, Y0, Y0 + (k + 1) * (H - .3) / N, Zf(B.V0 + (k + 1) * (B.V1 - B.V0) / N), Zf(B.V0 + k * (B.V1 - B.V0) / N), 0x8a6340);
		}
		else if (Id.Contains(TEXT("table")) || Id.Contains(TEXT("case")) || Id.Contains(TEXT("shells")) || Id.Contains(TEXT("masks")) || Id.Contains(TEXT("fins")) || Id.Contains(TEXT("folding")))
		{
			const bool bCase = Id.Contains(TEXT("case"));
			I.Span(U0, U1, Y0 + .72, Y0 + .8, ZA, ZB, bCase ? 0x3a2614 : 0xe8e4d8);
			if (bCase) { I.Span(U0, U1, Y0, Y0 + .72, ZA, ZB, 0x5a3a22); G.Span(U0 + .05, U1 - .05, Y0 + .8, Y0 + 1.05, ZA + .05, ZB - .05, 0xbfe8f4); }
			else for (double DU : {U0 + .1, U1 - .1}) for (double DZ : {ZA + .1, ZB - .1}) I.Box(DU, Y0 + .36, DZ, .07, .72, .07, 0x3a3d40);
			for (int32 k = 0; k < 8; k++) I.Box(FMath::Lerp(U0 + .3, U1 - .3, H1(Seed + k)), Y0 + .86, FMath::Lerp(ZA + .2, ZB - .2, H1(Seed * 3 + k)), .25, .12, .2, Cols[k % Cols.Num()]);
		}
		else I.Span(U0, U1, Y0, Y0 + .9, ZA, ZB, R->Trim);
	}
}

// ---------------------------------------------------------------- the Gaff & Gill: carpet, slot banks, the aquarium wall, the fountain, the cage
void AKeysTown::BuildCasinoInterior(FTownBatch& I, FTownBatch& G)
{
	const FLot* L = Lot(TEXT("casino"));
	if (!L) return;
	const double Y0 = P + .07, H = L->H - .2;
	const double X0 = L->U0 + WallT, X1 = L->U1 - WallT, V0 = L->V0 + WallT, V1 = L->V1 - WallT;
	// casino carpet: a red ground with a gold and teal motif
	for (double U = X0; U < X1 - .01; U += 1.0)
		for (double V = V0; V < V1 - .01; V += 1.0)
		{
			const int32 A = int32(U - X0), B = int32(V - V0);
			const uint32 C = (A + B) % 4 == 0 ? 0xe0b34a : (A * 3 + B) % 7 == 0 ? 0x0e6d78 : (A + B) % 2 ? 0x7a1424 : 0x6a1020;
			I.Flat(U, FMath::Min(U + 1, X1), Zf(FMath::Min(V + 1, V1)), Zf(V), Y0, Hex(C));
		}
	const double CY = P + H;
	const uint32 WallC = 0x0d3a45;
	I.Span(X0, X1, Y0, CY, Zf(V1) + .03, Zf(V1) + .06, WallC);
	I.Span(X0, X0 + .03, Y0, CY, Zf(V1), Zf(V0), WallC);
	I.Span(X1 - .03, X1, Y0, CY, Zf(V1), Zf(V0), WallC);
	I.Span(X0, L->Door - DoorW / 2, Y0, CY, Zf(V0) - .06, Zf(V0) - .03, WallC);
	I.Span(L->Door + DoorW / 2, X1, Y0, CY, Zf(V0) - .06, Zf(V0) - .03, WallC);
	I.Quad3(FVector(X0, CY, Zf(V0)), FVector(X1, CY, Zf(V0)), FVector(X1, CY, Zf(V1)), FVector(X0, CY, Zf(V1)), FVector(0, -1, 0), Hex(0x0b1a22));
	// gold coves along the ceiling, chandeliers of light
	I.Span(X0, X1, CY - .4, CY - .3, Zf(V1) + .06, Zf(V1) + .3, 0xe0b34a);
	for (double U = -16; U <= 16; U += 8)
		for (double V = 146; V <= 162; V += 8)
		{
			I.Cyl(U, CY - 1.2, Zf(V), .02, .02, 1.2, 0xe0b34a, 4);
			G.Ball(U, CY - 1.3, Zf(V), .45, 0xfff3b0, .6, 10);
		}
	// slot cabinets: a body, the topper, the glowing screen facing the player and a stool
	static const uint32 Topper[] = {0x35e0ff, 0xff3fa4, 0xffd96a, 0x7bc96f, 0xff9f43, 0x8a3cff, 0xf26b5b, 0x3fb6c9};
	for (const FCabinet& C : Cabinets())
	{
		const double Yaw = FMath::Atan2(-C.FV, C.FU);   // the cabinet's facing in the three frame (x = u, z = -v)
		const double Cx = C.U, Cz = Zf(C.V);
		const double Fx = C.FU, Fz = -C.FV;
		I.Box(Cx, Y0 + .6, Cz, C.FU != 0 ? 1.4 : 1.8, 1.2, C.FU != 0 ? 1.8 : 1.4, 0x15171a);
		I.Box(Cx - Fx * .2, Y0 + 1.6, Cz - Fz * .2, C.FU != 0 ? .8 : 1.7, .9, C.FU != 0 ? 1.7 : .8, 0x2d3136);
		G.Box(Cx + Fx * .21, Y0 + 1.6, Cz + Fz * .21, C.FU != 0 ? .02 : 1.3, .65, C.FU != 0 ? 1.3 : .02, Topper[C.Theme]);
		G.Box(Cx - Fx * .2, Y0 + 2.2, Cz - Fz * .2, C.FU != 0 ? .7 : 1.6, .3, C.FU != 0 ? 1.6 : .7, Topper[(C.Theme + 3) % 8]);
		I.Box(Cx + Fx * .75, Y0 + .9, Cz + Fz * .75, C.FU != 0 ? .3 : 1.6, .08, C.FU != 0 ? 1.6 : .3, 0xe0b34a);
		const double SU = C.SU, SZ = Zf(C.SV);
		I.Cyl(SU, Y0, SZ, .05, .07, .62, 0xe0b34a, 6);
		I.Cyl(SU, Y0 + .62, SZ, .24, .24, .1, 0x7a1424, 10);
		(void)Yaw;
	}
	// the cage: a counter with brass bars over it and the cashiers behind
	if (const FInteriorBox* B = InteriorBox(TEXT("casino-cage")))
	{
		I.Span(B->U0, B->U1, Y0, Y0 + 1.1, Zf(B->V1), Zf(B->V0), 0x3b2a1c);
		I.Span(B->U0 - .1, B->U1, Y0 + 1.1, Y0 + 1.16, Zf(B->V1), Zf(B->V0), 0xe0b34a);
		for (double V = B->V0 + .2; V < B->V1; V += .25) I.Cyl(B->U0, Y0 + 1.16, Zf(V), .015, .015, 1.3, 0xe0b34a, 4);
		I.Span(B->U0 - .02, B->U0 + .02, Y0 + 2.46, Y0 + 2.6, Zf(B->V1), Zf(B->V0), 0xe0b34a);
		AddSign(TEXT("CASHIER"), B->U0 - .05, Y0 + 2.9, (B->V0 + B->V1) / 2, .45, Hex(0xffd96a), 90, true, 4);
	}
	// the Go Fish table
	if (const FInteriorBox* B = InteriorBox(TEXT("casino-table")))
	{
		const double UC = (B->U0 + B->U1) / 2, ZC = Zf((B->V0 + B->V1) / 2);
		I.Cyl(UC, Y0, ZC, .4, .5, .7, 0x3b2a1c, 10);
		I.Span(B->U0, B->U1, Y0 + .7, Y0 + .82, Zf(B->V1), Zf(B->V0), 0x5a3a22);
		I.Span(B->U0 + .2, B->U1 - .2, Y0 + .82, Y0 + .84, Zf(B->V1) + .2, Zf(B->V0) - .2, 0x1f6b3a);
		for (int32 k = 0; k < 6; k++) I.Box(UC + (k - 2.5) * .5, Y0 + .85, ZC + .6, .2, .01, .3, 0xffffff, 0, k * .3);
	}
	// the marlin fountain: a round basin, water, and a bronze marlin leaping out of it
	if (const FInteriorBox* B = InteriorBox(TEXT("casino-fountain")))
	{
		const double UC = (B->U0 + B->U1) / 2, ZC = Zf((B->V0 + B->V1) / 2);
		I.Cyl(UC, Y0, ZC, 2.5, 2.6, .6, 0xe8e4d8, 20);
		I.Cyl(UC, Y0 + .5, ZC, 2.25, 2.25, .12, 0x2f8fb0, 20);
		I.Cyl(UC, Y0, ZC, .4, .5, 1.6, 0xe8e4d8, 10);
		for (int32 i = 0; i < 9; i++)
		{
			const double T = i / 8.0, Ang = -.2 + T * 1.9;
			I.Box(UC - 1.0 + T * 2.0, Y0 + 1.8 + FMath::Sin(T * PI * .85) * 2.4, ZC, .7, .45 - T * .25, .45 - T * .25, i < 8 ? 0xb07a3a : 0xe0b34a, 0, 0, Ang * .7);
		}
		I.Box(UC + 1.1, Y0 + 3.8, ZC, 1.0, .08, .08, 0xb07a3a, 0, 0, .8);
		G.Cyl(UC, Y0 + .63, ZC, 2.0, 2.0, .01, 0x7fd8f0, 20);
	}
	for (const TCHAR* Id : {TEXT("casino-pillar-w"), TEXT("casino-pillar-e")})
		if (const FInteriorBox* B = InteriorBox(Id))
			I.Span(B->U0, B->U1, Y0, CY, Zf(B->V1), Zf(B->V0), 0x9a7a3a);
	// THE AQUARIUM WALL: sand, rock, coral and weed behind glass the length of the west wall, lit from above
	if (const FInteriorBox* B = InteriorBox(TEXT("casino-aquarium")))
	{
		const double TU0 = B->U0, TU1 = B->U1;
		I.Span(TU0, TU1, Y0, Y0 + .5, Zf(B->V1), Zf(B->V0), 0x0b1a22);
		I.Span(TU0, TU0 + .05, Y0, CY, Zf(B->V1), Zf(B->V0), 0x0a4a6a);
		I.Span(TU0 + .05, TU1 - .1, Y0 + .5, Y0 + .7, Zf(B->V1), Zf(B->V0), 0xe8dcb8);
		for (double V = B->V0 + .6; V < B->V1 - .4; V += 1.1)
		{
			const double Hh = H1(V * 1.3);
			const uint32 Cc[] = {0xff7ab0, 0xf2c14e, 0xe4572e, 0x8a3cff, 0x7bc96f};
			I.Ball(TU0 + .5, Y0 + .7, Zf(V), .3 + Hh * .3, 0x8d8a84, .8, 6);
			I.Cyl(TU0 + .6, Y0 + .7, Zf(V + .4), .05, .1, .6 + Hh * 1.4, Cc[int32(V) % 5], 5, 0, 0, (Hh - .5) * .5);
			if (Hh > .5) I.Ball(TU0 + .9, Y0 + .9, Zf(V + .7), .25, Cc[(int32(V) + 2) % 5], 1.2, 6);
		}
		// the glow of the water itself (unlit blue panels at the back) and the glass rim
		I.Quad3(FVector(TU0 + .06, Y0 + .7, Zf(B->V0)), FVector(TU0 + .06, Y0 + .7, Zf(B->V1)), FVector(TU0 + .06, CY - .6, Zf(B->V1)), FVector(TU0 + .06, CY - .6, Zf(B->V0)), FVector(1, 0, 0), Hex(0x0f5a82));
		G.Span(TU0 + .1, TU1 - .1, CY - .64, CY - .6, Zf(B->V1), Zf(B->V0), 0x9fe8ff);
		I.Span(TU0, TU1, CY - .6, CY, Zf(B->V1), Zf(B->V0), 0x0b1a22);
		I.Span(TU1 - .08, TU1, Y0, Y0 + .7, Zf(B->V1), Zf(B->V0), 0x0b1a22);
		for (double V = B->V0; V <= B->V1; V += 4.7) I.Span(TU1 - .1, TU1, Y0, CY, Zf(V + .1), Zf(V - .1), 0x0b1a22);
	}
	// the bank islands: a single plinth under each row
	for (const TCHAR* Id : {TEXT("casino-island-w"), TEXT("casino-island-e")})
		if (const FInteriorBox* B = InteriorBox(Id)) I.Span(B->U0, B->U1, Y0, Y0 + .05, Zf(B->V1), Zf(B->V0), 0xe0b34a);
	AddSign(TEXT("GAFF & GILL"), 0, P + 6.6, V1 - .1, .9, Hex(0xffd96a), 0, true, 16);
}
