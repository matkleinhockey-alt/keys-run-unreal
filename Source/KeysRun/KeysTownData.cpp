#include "KeysTownData.h"
#include "KeysSea.h"
#include "KeysGeo.h"

// Ported from the web game: packages/shared/src/world/town.ts (lots, streets, props, interiors, NPCs, walkability),
// world/casino.ts (the Gaff & Gill floor), content/venues.ts (names, greetings, shelves), apps/client/src/entities/town/paint.ts
// (the paint chips) and content/estates-plat.ts (Harbor Row).
namespace KeysTown
{
	namespace
	{
		const KeysGeo::FMarina& M0() { return KeysGeo::Marinas()[0]; }

		FLot L(const TCHAR* Id, const TCHAR* V, EStyle S, double U0, double U1, double V0, double V1, double H, double Door)
		{
			return FLot{Id, V, S, U0, U1, V0, V1, H, Door};
		}
	}

	double OX() { return M0().SX; }
	double OZ() { return M0().SZ; }
	int32 Dir() { return M0().Dir; }

	FRect WorldRect(double U0, double U1, double V0, double V1)
	{
		double XA, ZA, XB, ZB;
		ToWorld(U0, V0, XA, ZA);
		ToWorld(U1, V1, XB, ZB);
		return FRect{FMath::Min(XA, XB), FMath::Max(XA, XB), FMath::Min(ZA, ZB), FMath::Max(ZA, ZB)};
	}

	FVector WorldPos(double U, double V, double Y)
	{
		double X, Z;
		ToWorld(U, V, X, Z);
		return FVector(X, Y, Z);
	}

	const FLook& LookOf(EStyle S)
	{
		static const FLook Looks[] = {
			/* Diner */      {0xd7dde2, 0xc23a3a, 0xb8bcc2, 0x1d2430, 0xff5a4f, 0xe8edf1, 0xff4538, 3, 3, 0xc23a3a, 0xf4f4f2},
			/* Tackle */     {0x2f8f9d, 0xf4f1e8, 0x9aa4ab, 0xf4f1e8, 0x13495a, 0x1c7080, 0x38e0ff, 0, 0, 0x1f4f8c, 0xf4f1e8},
			/* Guns */       {0x6a2f29, 0x16110f, 0x3b3b3e, 0x17120f, 0xf2ece0, 0xc32a2a, 0xff3b30, 1, 1, 0, 0},
			/* Liquor */     {0xe0ac3c, 0x2a2a2a, 0x9a8f7a, 0xb3202a, 0xfff6c8, 0xfff6c8, 0xffd34a, 4, 0, 0, 0},
			/* Bar */        {0x8a6746, 0x2b2118, 0x8f959b, 0x2b2118, 0xf3dfb4, 0xc9852c, 0x58b6ff, 4, 2, 0, 0},
			/* Club */       {0x2e1d3a, 0xff3fa4, 0x15101a, 0x120a18, 0xff7ccb, 0xff3fa4, 0xff3fa4, 2, 4, 0, 0},
			/* Gifts */      {0xf08a7c, 0xfff2dc, 0xb8bcc2, 0xfff2dc, 0xc0392b, 0x2bb3a8, 0x2bf0d0, 0, 0, 0x2bb3a8, 0xfff2dc},
			/* Pawn */       {0x7a7a4e, 0xf0c64a, 0x6f7378, 0xb3202a, 0xffe27a, 0xffe27a, 0xffe27a, 1, 0, 0, 0},
			/* Laundromat */ {0x8fc4e8, 0xffffff, 0xb8bcc2, 0xffffff, 0x1d6fb3, 0x8fc4e8, 0x7fe0ff, 0, 0, 0, 0},
			/* IceCream */   {0xf7c6d8, 0xffffff, 0xe8e0d0, 0xffffff, 0xe0457b, 0x7ad7d0, 0xff7ab0, 0, 0, 0xe0457b, 0xffffff},
			/* Motel */      {0xefe3c8, 0x2bb3a8, 0xb86a4a, 0x2bb3a8, 0xfff6e0, 0xfff6e0, 0xff6a3a, 4, 0, 0, 0},
			/* Convenience */{0xe8e4da, 0x2a8f4a, 0x9aa4ab, 0x2a8f4a, 0xffffff, 0xffd34a, 0x6dff9a, 0, 0, 0x2a8f4a, 0xffffff},
			/* Dive */       {0x1f6fa0, 0xffd34a, 0x9aa4ab, 0x0e3a58, 0xffd34a, 0x38e0ff, 0x38e0ff, 0, 0, 0xffd34a, 0x0e3a58},
			/* Casino */     {0x0d4d5c, 0xe0b34a, 0x0b1a22, 0x06222e, 0xffd96a, 0x35e0ff, 0x35e0ff, 2, 4, 0, 0},
		};
		return Looks[int32(S)];
	}

	const TArray<FLot>& Lots()
	{
		static const TArray<FLot> A = {
			// Row 1, west of Palm Avenue, from the avenue outward
			L(TEXT("diner"), TEXT("diner"), EStyle::Diner, -32, -8, 46, 70, 4.4, -20),
			L(TEXT("tackle"), TEXT("tackle"), EStyle::Tackle, -62, -32, 46, 72, 5.2, -47),
			L(TEXT("guns"), TEXT("guns"), EStyle::Guns, -86, -62, 46, 70, 4.6, -74),
			L(TEXT("liquor"), TEXT("liquor"), EStyle::Liquor, -100, -86, 46, 66, 4.2, -93),
			// Row 1, east
			L(TEXT("bar"), TEXT("bar"), EStyle::Bar, 8, 34, 46, 70, 4.8, 21),
			L(TEXT("club"), TEXT("club"), EStyle::Club, 34, 68, 46, 72, 6.6, 44),
			L(TEXT("gifts"), TEXT("gifts"), EStyle::Gifts, 68, 100, 46, 68, 4.4, 84),
			// Row 2, on Sombrero Boulevard
			L(TEXT("pawn"), TEXT("pawn"), EStyle::Pawn, -100, -70, 100, 122, 4.6, -85),
			L(TEXT("laundromat"), TEXT("laundromat"), EStyle::Laundromat, -70, -44, 100, 120, 4.2, -57),
			L(TEXT("dive"), TEXT("dive"), EStyle::Dive, -30, -8, 100, 120, 4.4, -19),
			L(TEXT("motel"), TEXT("motel"), EStyle::Motel, 8, 60, 100, 124, 7.6, 20),
			L(TEXT("icecream"), TEXT("icecream"), EStyle::IceCream, 60, 80, 100, 118, 4.0, 70),
			L(TEXT("stopngo"), TEXT("convenience"), EStyle::Convenience, 80, 100, 100, 122, 4.4, 90),
			// the two dive bars in the service lane behind the strip
			L(TEXT("tarpon"), TEXT("tarpon"), EStyle::Bar, -100, -78, 74, 80, 4.4, -82),
			L(TEXT("hammered"), TEXT("hammered"), EStyle::Bar, 82, 100, 74, 80, 4.2, 97),
			// the Gaff & Gill Casino, in the back lot at the top of Palm Avenue
			L(TEXT("casino"), TEXT("casino"), EStyle::Casino, -24, 24, 140, 166, 8.6, 0),
		};
		return A;
	}

	const FLot* Lot(const FString& Id)
	{
		for (const FLot& X : Lots()) if (Id == X.Id) return &X;
		return nullptr;
	}

	TArray<FLocalBox> LotWalls(const FLot& Lt)
	{
		const double T = WallT, G = DoorW / 2;
		return {
			{Lt.U0, Lt.Door - G, Lt.V0, Lt.V0 + T},
			{Lt.Door + G, Lt.U1, Lt.V0, Lt.V0 + T},
			{Lt.U0, Lt.U1, Lt.V1 - T, Lt.V1},
			{Lt.U0, Lt.U0 + T, Lt.V0, Lt.V1},
			{Lt.U1 - T, Lt.U1, Lt.V0, Lt.V1},
		};
	}

	const TArray<FSurface>& Surfaces()
	{
		static const TArray<FSurface> A = {
			{ESurface::Promenade, PadU0, PadU1, PadV0, 28},
			{ESurface::Road, PadU0, PadU1, 28, 40},          // Harbor Drive
			{ESurface::Sidewalk, PadU0, PadU1, 40, 46},
			{ESurface::Road, -5, 5, 40, 96},                 // Palm Avenue
			{ESurface::Sidewalk, -8, -5, 46, 100},
			{ESurface::Sidewalk, 5, 8, 46, 100},
			{ESurface::Lot, PadU0, PadU1, 72, 80},           // the back lane
			{ESurface::Sidewalk, PadU0, PadU1, 80, 84},
			{ESurface::Road, PadU0, PadU1, 84, 96},          // Sombrero Boulevard
			{ESurface::Sidewalk, PadU0, PadU1, 96, 100},
			{ESurface::Lot, PadU0, PadU1, 126, PadV1},       // the back lot
		};
		return A;
	}

	const TArray<FPalm>& Palms()
	{
		static TArray<FPalm> A;
		if (!A.Num())
		{
			for (double U : {-98, -76, -54, -34, 34, 54, 76, 98}) A.Add({U, 22, 1.15});
			for (double U : {-60, 60}) A.Add({U, 22, .95});
			A.Add({-9.5, 97.5, 1.0}); A.Add({9.5, 97.5, 1.0});
			for (double U : {-90, -60, -30, 0, 30, 60, 90}) A.Add({U, 168, 1.2});
			for (double U : {-100, -88, 88, 100}) A.Add({U, 140, 1.05});
			A.Add({-38, 110, .9}); A.Add({38, 130, 1.1});
		}
		return A;
	}

	const TArray<FPoint>& Lamps()
	{
		static TArray<FPoint> A;
		if (!A.Num())
		{
			for (double U : {-90, -60, -30, 30, 60, 90}) { A.Add({U, 27}); A.Add({U, 41.2}); }
			for (double U : {-60, -30, 30, 60}) A.Add({U, 82});
			A.Add({-9, 20}); A.Add({9, 20}); A.Add({6.5, 60}); A.Add({-6.5, 60});
		}
		return A;
	}

	const TArray<FPoint>& Benches()
	{
		static TArray<FPoint> A;
		if (!A.Num()) for (double U : {-88, -64, -40, 40, 64, 88}) A.Add({U, 19.8});
		return A;
	}

	const TArray<FPoint>& TrashCans()
	{
		static TArray<FPoint> A;
		if (!A.Num()) for (double U : {-96, -70, -20, 20, 70, 96}) A.Add({U, 42.4});
		return A;
	}

	const TArray<FCar>& Cars()
	{
		static TArray<FCar> A;
		if (!A.Num())
		{
			static const uint32 Col[] = {0xd9d9d6, 0x2b4a6e, 0xa22b2b, 0x1f1f23, 0xe6e0c8, 0x3f6b4d, 0xc9a24a, 0x7a7d82};
			auto C = [](int32 i) { return Col[i % 8]; };
			A = {
				{TEXT("hd-1"), -78, 29.4, 0, ECar::Sedan, C(0), false},
				{TEXT("hd-2"), -44, 29.4, 0, ECar::Van, 0xe9e6dc, false},
				{TEXT("hd-3"), 24, 29.4, 0, ECar::Wagon, 0x8a5a36, false},
				{TEXT("hd-4"), -56, 38.4, PI, ECar::Sedan, C(2), false},
				{TEXT("hd-5"), 14, 38.4, PI, ECar::Pickup, C(4), false},
				{TEXT("hd-6"), 51, 38.4, PI, ECar::Convertible, 0xe86aa8, false},
				{TEXT("hd-7"), 80, 38.4, PI, ECar::Cart, 0xe8e2c8, false},
				{TEXT("bl-1"), -50, 77.4, 0, ECar::Pickup, C(6), false},
				{TEXT("bl-2"), 14, 77.4, 0, ECar::Van, 0x2f6f8f, false},
				{TEXT("bl-3"), 50, 77.4, 0, ECar::Suv, C(1), false},
				{TEXT("bl-4"), 78, 77.4, PI, ECar::Cart, 0x3f6b4d, false},
				{TEXT("sb-1"), -60, 85.4, 0, ECar::Sedan, C(3), false},
				{TEXT("sb-2"), 30, 94.6, PI, ECar::Pickup, C(2), false},
				{TEXT("sb-3"), 70, 85.4, 0, ECar::Wagon, 0xd9d4c0, false},
			};
			const double BU[] = {-84, -66, -48, -30, 30, 48, 66, 84};
			const ECar BK[] = {ECar::Sedan, ECar::Pickup, ECar::Suv, ECar::Sedan};
			static const TCHAR* BId[] = {TEXT("bk-0"), TEXT("bk-1"), TEXT("bk-2"), TEXT("bk-3"), TEXT("bk-4"), TEXT("bk-5"), TEXT("bk-6"), TEXT("bk-7")};
			for (int32 i = 0; i < 8; i++) A.Add({BId[i], BU[i], 134, PI / 2, BK[i % 4], C(i + 2), false});
			A.Add({TEXT("trailer-1"), -60, 150, PI / 2, ECar::Pickup, C(1), true});
			A.Add({TEXT("trailer-2"), 60, 150, PI / 2, ECar::Pickup, C(4), true});
		}
		return A;
	}

	const TArray<FSolidProp>& SolidProps()
	{
		static TArray<FSolidProp> A;
		if (!A.Num())
		{
			A = {{16, 24, 2.6, 2.6}, {-24, 77.4, 3.2, 1.6}, {30, 77.4, 3.2, 1.6}, {-55, 42.2, 2.4, 1.4}, {40, 98.5, .8, .8}};
			for (const FPoint& B : Benches()) A.Add({B.U, B.V, 1.9, .6});
		}
		return A;
	}

	// ------------------------------------------------------------------ the casino (world/casino.ts)
	const TCHAR* const CabinetThemes[8] = {TEXT("Tarpon Run"), TEXT("Bonefish Flats"), TEXT("Permit Pursuit"), TEXT("Marlin Madness"),
		TEXT("Boot Camp"), TEXT("Chum Bucket"), TEXT("Reel Deal"), TEXT("Sea Legs")};

	const TArray<FCabinet>& Cabinets()
	{
		static TArray<FCabinet> A;
		if (!A.Num())
		{
			const double Pitch = 2.0, Depth = 1.5, TankD = 1.4, Stand = 1.0, W = .4;
			int32 i = 0;
			auto Add = [&](double U, double V, double FU, double FV)
			{
				i++;
				const bool bOcc = i == 1 || i == 6 || i == 12 || i == 19 || i == 26;
				A.Add({U, V, FU, FV, i % 8, U + FU * (Depth / 2 + Stand), V + FV * (Depth / 2 + Stand), bOcc});
			};
			for (double CU : {-15, -13, -11, -9, 9, 11, 13, 15}) { Add(CU, 152 + Depth / 2, 0, -1); Add(CU, 155 - Depth / 2, 0, 1); }
			for (double CU : {-5, -3, -1, 1, 3, 5}) Add(CU, 166 - W - Depth / 2, 0, -1);
			for (double CV : {156, 158, 160, 162}) { Add(-24 + W + TankD + Depth / 2, CV, 1, 0); Add(24 - W - Depth / 2, CV, -1, 0); }
			(void)Pitch;
		}
		return A;
	}

	// ------------------------------------------------------------------ interiors
	const TArray<FInteriorBox>& InteriorBoxes()
	{
		static TArray<FInteriorBox> A;
		if (!A.Num())
		{
			A = {
				{TEXT("bar-counter"), TEXT("bar"), 11, 31, 60.4, 62.4},
				{TEXT("tarpon-counter"), TEXT("tarpon"), -98.6, -84.5, 76.6, 78},
				{TEXT("hammered-counter"), TEXT("hammered"), 83.4, 95.6, 76.6, 78},
				{TEXT("bar-booth-1"), TEXT("bar"), 30, 33.4, 50, 54},
				{TEXT("bar-booth-2"), TEXT("bar"), 30, 33.4, 55.5, 59.5},
				{TEXT("bar-pool"), TEXT("bar"), 10.5, 15.5, 50, 54.4},
				{TEXT("diner-counter"), TEXT("diner"), -29, -14, 60, 61.8},
				{TEXT("diner-booth-w1"), TEXT("diner"), -31.6, -28.4, 50, 54},
				{TEXT("diner-booth-w2"), TEXT("diner"), -31.6, -28.4, 55.5, 59.5},
				{TEXT("diner-booth-e1"), TEXT("diner"), -11.6, -8.4, 50, 54},
				{TEXT("diner-booth-e2"), TEXT("diner"), -11.6, -8.4, 55.5, 59.5},
				{TEXT("club-stage"), TEXT("club"), 42, 60, 64, 70},
				{TEXT("club-bar"), TEXT("club"), 36, 40, 56, 68},
				{TEXT("club-counter"), TEXT("club"), 48, 60, 52, 53.6},
				{TEXT("club-booth-owner"), TEXT("club"), 62, 66.4, 58, 64},
				{TEXT("club-booth-1"), TEXT("club"), 62, 66.4, 50, 55},
				{TEXT("club-dj"), TEXT("club"), 61, 66, 66, 70},
			};
			// every shop in its own frame: a = metres along the frontage from the west wall, b = metres inland of the front wall
			auto IB = [](const TCHAR* LotId, const TCHAR* Id, double A0, double A1, double B0, double B1)
			{
				const FLot* Lt = Lot(LotId);
				return FInteriorBox{Id, LotId, FMath::Max(Lt->U0 + WallT, Lt->U0 + A0), FMath::Min(Lt->U1 - WallT, Lt->U0 + A1),
					FMath::Max(Lt->V0 + WallT, Lt->V0 + B0), FMath::Min(Lt->V1 - WallT, Lt->V0 + B1)};
			};
			A.Append({
				IB(TEXT("dive"), TEXT("dive-counter"), .4, 21.6, 11.5, 13.1), IB(TEXT("dive"), TEXT("dive-suits"), .4, 1.6, 3.5, 10),
				IB(TEXT("dive"), TEXT("dive-tanks"), 20.4, 21.6, 3.5, 10), IB(TEXT("dive"), TEXT("dive-masks"), 3.5, 7.5, 5.5, 7.5),
				IB(TEXT("dive"), TEXT("dive-fins"), 14.5, 18.5, 5.5, 7.5),
				IB(TEXT("tackle"), TEXT("tackle-counter"), .4, 29.6, 16, 17.6), IB(TEXT("tackle"), TEXT("tackle-rods"), .4, 1.6, 3, 14),
				IB(TEXT("tackle"), TEXT("tackle-bait"), 27.4, 29.6, 3, 9), IB(TEXT("tackle"), TEXT("tackle-aisle-1"), 4, 12, 9, 10.2),
				IB(TEXT("tackle"), TEXT("tackle-aisle-2"), 18, 26, 9, 10.2), IB(TEXT("tackle"), TEXT("tackle-freezer"), 24, 29.6, 12.5, 14.5),
				IB(TEXT("guns"), TEXT("guns-counter"), .4, 23.6, 14, 15.6), IB(TEXT("guns"), TEXT("guns-spears"), .4, 1.6, 3, 11),
				IB(TEXT("guns"), TEXT("guns-cage"), 22.4, 23.6, 3, 11), IB(TEXT("guns"), TEXT("guns-ammo"), 8, 16, 8.6, 9.8),
				IB(TEXT("liquor"), TEXT("liquor-counter"), .4, 13.6, 12, 13.6), IB(TEXT("liquor"), TEXT("liquor-wine"), .4, 1.6, 3, 10),
				IB(TEXT("liquor"), TEXT("liquor-cooler"), 12.4, 13.6, 3, 10),
				IB(TEXT("gifts"), TEXT("gifts-counter"), .4, 31.6, 14, 15.6), IB(TEXT("gifts"), TEXT("gifts-shirts-1"), 3, 8, 5, 6.4),
				IB(TEXT("gifts"), TEXT("gifts-shirts-2"), 24, 29, 5, 6.4), IB(TEXT("gifts"), TEXT("gifts-shells"), 12, 20, 9.5, 11.5),
				IB(TEXT("gifts"), TEXT("gifts-hotsauce"), 30.4, 31.6, 3, 11), IB(TEXT("gifts"), TEXT("gifts-shirt-wall"), .4, 1.6, 3, 11),
				IB(TEXT("pawn"), TEXT("pawn-counter"), .4, 29.6, 12, 13.8), IB(TEXT("pawn"), TEXT("pawn-guitars"), .4, 1.4, 3, 10),
				IB(TEXT("pawn"), TEXT("pawn-case-1"), 5, 11, 8.6, 10), IB(TEXT("pawn"), TEXT("pawn-case-2"), 19, 25, 8.6, 10),
				IB(TEXT("pawn"), TEXT("pawn-tools"), 28.2, 29.6, 3, 11),
				IB(TEXT("laundromat"), TEXT("laundry-desk"), 4, 22, 14, 15.6), IB(TEXT("laundromat"), TEXT("laundry-washers-w"), .4, 1.8, 3, 12),
				IB(TEXT("laundromat"), TEXT("laundry-washers-e"), 24.2, 25.6, 3, 12), IB(TEXT("laundromat"), TEXT("laundry-folding"), 16, 22, 8.6, 10.2),
				IB(TEXT("laundromat"), TEXT("laundry-vending"), 3, 6, 8.6, 9.6), IB(TEXT("laundromat"), TEXT("laundry-dryers"), .4, 25.6, 18.8, 19.6),
				IB(TEXT("icecream"), TEXT("ice-counter"), .4, 19.6, 10, 11.6), IB(TEXT("icecream"), TEXT("ice-table-1"), 1.2, 4.2, 3, 5.6),
				IB(TEXT("icecream"), TEXT("ice-table-2"), 15.8, 18.8, 3, 5.6),
				IB(TEXT("motel"), TEXT("motel-desk"), 2, 22, 10, 11.6), IB(TEXT("motel"), TEXT("motel-sofa-1"), 25, 29.5, 4, 6.5),
				IB(TEXT("motel"), TEXT("motel-sofa-2"), 33, 37.5, 4, 6.5), IB(TEXT("motel"), TEXT("motel-vending"), 49, 51.6, 4, 6.4),
				IB(TEXT("motel"), TEXT("motel-stairs"), 42, 51.6, 14, 23.6), IB(TEXT("motel"), TEXT("motel-keys"), 3, 21, 13.6, 13.9),
				IB(TEXT("stopngo"), TEXT("conv-counter"), .4, 19.6, 15, 16.6), IB(TEXT("stopngo"), TEXT("conv-cooler"), .4, 1.8, 3, 13),
				IB(TEXT("stopngo"), TEXT("conv-snacks"), 18.2, 19.6, 3, 13), IB(TEXT("stopngo"), TEXT("conv-aisle-1"), 2.8, 8, 8.6, 9.8),
				IB(TEXT("stopngo"), TEXT("conv-aisle-2"), 12, 17.2, 8.6, 9.8),
			});
			// the casino floor (world/casino.ts CASINO_BOXES)
			const double W = .4, D = 1.5, TankD = 1.4;
			A.Append({
				{TEXT("casino-island-w"), TEXT("casino"), -16, -8, 152, 155},
				{TEXT("casino-island-e"), TEXT("casino"), 8, 16, 152, 155},
				{TEXT("casino-bank-back"), TEXT("casino"), -6, 6, 166 - W - D, 166 - W},
				{TEXT("casino-bank-w"), TEXT("casino"), -24 + W + TankD, -24 + W + TankD + D, 155, 163},
				{TEXT("casino-bank-e"), TEXT("casino"), 24 - W - D, 24 - W, 155, 163},
				{TEXT("casino-cage"), TEXT("casino"), 19, 21, 143, 151},
				{TEXT("casino-table"), TEXT("casino"), -21.5, -16.5, 144, 148},
				{TEXT("casino-fountain"), TEXT("casino"), -2.6, 2.6, 155.6, 160.4},
				{TEXT("casino-aquarium"), TEXT("casino"), -24 + W, -24 + W + TankD, 141.5, 165},
				{TEXT("casino-pillar-w"), TEXT("casino"), -6.45, -5.55, 146, 147},
				{TEXT("casino-pillar-e"), TEXT("casino"), 5.55, 6.45, 146, 147},
			});
		}
		return A;
	}

	const FInteriorBox* InteriorBox(const FString& Id)
	{
		for (const FInteriorBox& B : InteriorBoxes()) if (Id == B.Id) return &B;
		return nullptr;
	}

	// ------------------------------------------------------------------ shop counters, interaction points, people
	namespace
	{
		struct FCounter { const TCHAR* Lot; const TCHAR* Counter; double A; const TCHAR* Prompt; const TCHAR* Keeper; };
		const TArray<FCounter>& Counters()
		{
			static const TArray<FCounter> A = {
				{TEXT("dive"), TEXT("dive-counter"), 11, TEXT("Talk to Steve at the counter"), TEXT("divemaster")},
				{TEXT("tackle"), TEXT("tackle-counter"), 15, TEXT("Ask about tackle"), TEXT("baitman")},
				{TEXT("guns"), TEXT("guns-counter"), 12, TEXT("Step up to the counter"), TEXT("gunsmith")},
				{TEXT("liquor"), TEXT("liquor-counter"), 7, TEXT("Pay at the register"), TEXT("clerk")},
				{TEXT("gifts"), TEXT("gifts-counter"), 16, TEXT("Ring up at the till"), TEXT("clerk")},
				{TEXT("pawn"), TEXT("pawn-counter"), 15, TEXT("Talk to the pawnbroker"), TEXT("pawnbroker")},
				{TEXT("laundromat"), TEXT("laundry-desk"), 13, TEXT("Ask the attendant"), TEXT("attendant")},
				{TEXT("icecream"), TEXT("ice-counter"), 10, TEXT("Order a cone"), TEXT("scooper")},
				{TEXT("motel"), TEXT("motel-desk"), 12, TEXT("Ring the front desk"), TEXT("innkeeper")},
				{TEXT("stopngo"), TEXT("conv-counter"), 10, TEXT("Pay at the register"), TEXT("clerk")},
				{TEXT("club"), TEXT("club-counter"), 20, TEXT("Lean on the bar"), TEXT("bartender")},
			};
			return A;
		}
	}

	const TArray<FInteract>& Interacts()
	{
		static TArray<FInteract> A;
		if (!A.Num())
		{
			A.Add({TEXT("counter-bar"), TEXT("bar"), 21, 59.3, 2.6, TEXT("Order at the bar"), FString()});
			A.Add({TEXT("counter-diner"), TEXT("diner"), -21.5, 58.8, 2.6, TEXT("Sit at the counter"), FString()});
			A.Add({TEXT("counter-tarpon"), TEXT("tarpon"), -91, 75.8, 2.6, TEXT("Order at the bar"), FString()});
			A.Add({TEXT("counter-hammered"), TEXT("hammered"), 89, 75.4, 2.6, TEXT("Order at the bar"), FString()});
			for (const FCounter& C : Counters())
			{
				const FLot* Lt = Lot(C.Lot);
				const FInteriorBox* B = InteriorBox(C.Counter);
				A.Add({FString(TEXT("counter-")) + Lt->Venue, Lt->Venue, Lt->U0 + C.A, B->V0 - 1.1, 2.6, C.Prompt, FString()});
			}
			for (const FCabinet& C : Cabinets())
				if (!C.bOccupied) A.Add({TEXT("slot"), TEXT("casino"), C.SU, C.SV, 1.15, TEXT("Play the slots"), FString()});
			A.Add({TEXT("counter-casino"), TEXT("casino"), 17.9, 147, 2.4, TEXT("Visit the change booth"), FString()});
			A.Add({TEXT("marlin"), FString(), 16, 22.2, 2.4, TEXT("Read the plaque"),
				TEXT("BLUE MARLIN, 612 LB. Boated 1957 off Sombrero Reef. The plaque says \"released\" because the town got embarrassed.")});
			for (const FEstateLot& E : HarborRow())
			{
				// the sign stands by the fence, clear of the cars parked nose-in along the back lot
				const double CU = (E.U0 + E.U1) * .5;
				A.Add({FString(TEXT("estate:")) + E.Id, FString(TEXT("estate:")) + E.Id, CU + (CU < 0 ? 6 : -6), E.V0 + 8, 2.6, TEXT("Read the For Sale sign"), FString()});
			}
		}
		return A;
	}

	const TArray<FNpc>& Npcs()
	{
		static TArray<FNpc> A;
		if (!A.Num())
		{
			const double HP = PI / 2;
			auto N = [&](const TCHAR* Look, double U, double V, double Yaw, const TCHAR* Inside = TEXT(""), double Y = 0, int32 Var = 0)
			{
				A.Add({Look, U, V, Yaw, Inside, Y, Var});
			};
			N(TEXT("bouncer"), 41.2, 44.6, -HP);
			N(TEXT("tourist"), -41, 21.6, .4);
			N(TEXT("woman"), -39.4, 21.9, PI + .4);
			N(TEXT("local"), 6.2, 44.4, PI);
			N(TEXT("angler"), 66, 20.8, -HP);
			N(TEXT("bartender"), 21, 63.6, -HP, TEXT("bar"));
			N(TEXT("patron"), 17, 58.6, HP, TEXT("bar"));
			N(TEXT("woman"), 26, 58.6, HP, TEXT("bar"), 0, 1);
			N(TEXT("bartender"), -91, 79, -HP, TEXT("tarpon"), 0, 1);
			N(TEXT("patron"), -97, 75.8, HP + .3, TEXT("tarpon"), 0, 2);
			N(TEXT("local"), -94, 75.8, HP - .25, TEXT("tarpon"), 0, 1);
			N(TEXT("angler"), -89, 75.8, HP + .12, TEXT("tarpon"), 0, 1);
			N(TEXT("woman"), -85.5, 75.8, HP - .4, TEXT("tarpon"), 0, 2);
			N(TEXT("bartender"), 89, 79, -HP, TEXT("hammered"), 0, 2);
			N(TEXT("local"), 84.5, 75.8, HP - .3, TEXT("hammered"), 0, 2);
			N(TEXT("tourist"), 87.5, 75.8, HP + .2, TEXT("hammered"), 0, 1);
			N(TEXT("patron"), 91, 75.8, HP - .15, TEXT("hammered"), 0, 3);
			N(TEXT("angler"), 94.5, 75.8, HP + .25, TEXT("hammered"), 0, 2);
			N(TEXT("cook"), -21.5, 63.6, -HP, TEXT("diner"));
			// the Pink Flamingo: two performers on the stage, an MC, a DJ on his riser and a few punters
			const FInteriorBox* Stage = InteriorBox(TEXT("club-stage"));
			(void)Stage;
			N(TEXT("dancer"), 47, 65.9, -HP, TEXT("club"), .6, 0);
			N(TEXT("dancer"), 55, 65.9, -HP, TEXT("club"), .6, 1);
			N(TEXT("mc"), 51, 62.6, -HP, TEXT("club"));
			N(TEXT("dj"), 63.5, 67.6, PI, TEXT("club"), .45);
			N(TEXT("patron"), 46, 57, -HP, TEXT("club"), 0, 4);
			N(TEXT("woman"), 49.5, 56.4, -HP, TEXT("club"), 0, 3);
			N(TEXT("local"), 56, 57.4, -HP, TEXT("club"), 0, 3);
			N(TEXT("tourist"), 41, 53, -HP, TEXT("club"), 0, 2);
			N(TEXT("patron"), -25, 57.6, HP, TEXT("diner"), 0, 5);
			int32 k = 0;
			for (const FCounter& C : Counters())
			{
				const FLot* Lt = Lot(C.Lot);
				const FInteriorBox* B = InteriorBox(C.Counter);
				N(C.Keeper, Lt->U0 + C.A, B->V1 + 1.3, -HP, C.Lot, 0, k++);
			}
			N(TEXT("angler"), -41, 58, HP, TEXT("tackle"), 0, 3);
			N(TEXT("tourist"), 39, 108, HP, TEXT("motel"), 0, 3);
			// the casino staff and the punters at the five occupied cabinets
			N(TEXT("cashier"), 22.3, 145.2, PI, TEXT("casino"));
			N(TEXT("cashier"), 22.3, 148.8, PI, TEXT("casino"), 0, 1);
			N(TEXT("dealer"), -19, 149.4, -HP, TEXT("casino"));
			N(TEXT("dealer"), -19, 142.6, HP, TEXT("casino"), 0, 1);
			N(TEXT("pitboss"), 3.8, 153.2, -HP, TEXT("casino"));
			N(TEXT("mascot"), 3.4, 143.4, -HP - .5, TEXT("casino"));
			int32 p = 0;
			for (const FCabinet& C : Cabinets())
				if (C.bOccupied) N(TEXT("punter"), C.SU, C.SV, FMath::Atan2(-C.FV, -C.FU), TEXT("casino"), 0, p++);
		}
		return A;
	}

	// ------------------------------------------------------------------ Harbor Row (content/estates-plat.ts, tier 'harbor')
	const TArray<FEstateLot>& HarborRow()
	{
		static const TArray<FEstateLot> A = {
			{TEXT("harbor-row-1"), TEXT("Harbor Row West 1"), -102, -62, 128, 170, 64000},
			{TEXT("harbor-row-2"), TEXT("Harbor Row West 2"), -60, -27, 128, 170, 56000},
			{TEXT("harbor-row-3"), TEXT("Harbor Row East 1"), 27, 60, 128, 170, 56000},
			{TEXT("harbor-row-4"), TEXT("Harbor Row East 2"), 62, 102, 128, 170, 64000},
		};
		return A;
	}

	// ------------------------------------------------------------------ docks and walkability
	const TArray<FDeckBox>& DeckBoxes()
	{
		static TArray<FDeckBox> A;
		if (!A.Num())
			for (const KeysGeo::FMarina& M : KeysGeo::Marinas())
			{
				auto Zo = [&](double O) { return M.SZ + M.Dir * O; };
				A.Add({3, 56, M.SX, Zo(20), .35, 1.25});
				A.Add({26, 3, M.SX, Zo(48), .35, 1.25});
				for (double O : {14, 26, 38})
					for (int32 s = -1; s <= 1; s += 2) A.Add({10, 1.4, M.SX + s * 6.5, Zo(O), .3, 1.2});
			}
		return A;
	}

	FRect MarinaShedRect(int32 I)
	{
		const KeysGeo::FMarina& M = KeysGeo::Marinas()[I];
		const double CX = M.SX + 13, CZ = M.SZ - M.Dir * 9;
		return FRect{CX - 5.5, CX + 5.5, CZ - 3.5, CZ + 3.5};
	}

	bool InFootprint(double X, double Z, double Margin)
	{
		double U, V;
		ToLocal(X, Z, U, V);
		return U >= PadU0 - Margin && U <= PadU1 + Margin && V >= PadV0 - Margin && V <= PadV1 + Margin;
	}

	bool InFill(double X, double Z)
	{
		double U, V;
		ToLocal(X, Z, U, V);
		return U >= -112 && U <= 112 && V >= 14 && V <= 180;
	}

	// ------------------------------------------------------------------ piers and waterfront estates
	namespace
	{
		bool UnderBridge(double X, double Z, double Clear)
		{
			if (X >= KeysGeo::WBX0 && X <= KeysGeo::WBX1 && FMath::Abs(Z - KeysGeo::ChainZ(X)) < Clear) return true;
			return X >= -3820 - Clear && X <= -2010 + Clear && FMath::Abs(Z - KeysGeo::ChainZ(X) + 58) < Clear;
		}
		bool NearMarinaPt(double X, double Z, double R)
		{
			for (const KeysGeo::FMarina& M : KeysGeo::Marinas()) if (FMath::Sqrt(FMath::Square(M.SX - X) + FMath::Square(M.SZ - Z)) < R) return true;
			return false;
		}
	}

	const TArray<FPier>& Piers()
	{
		static TArray<FPier> A;
		static bool bBuilt = false;
		if (bBuilt) return A;
		bBuilt = true;
		struct FCand { double X0, Z0, NX, NZ, Len, D; FString Isl; };
		TArray<FCand> Cands;
		const KeysGeo::FMarina& Town = KeysGeo::Marinas()[0];
		// web shared/world/piers.ts: walk each key's real shoreline, every edge's midpoint plus a point every 40 m along
		// long edges, and keep the ones where a pier can run from deck-height ground straight out to fishing water
		for (const KeysGeo::FIsland& I : KeysGeo::Islands())
		{
			const TArray<TArray<FVector2D>>* Rings = KeysGeo::CoastRings(I);
			if (!Rings || !Rings->Num() || I.bMiami || I.bMainland) continue;
			const TArray<FVector2D>& Ring = (*Rings)[0];
			const int32 M = Ring.Num();
			for (int32 e = 0; e < M; e++)
			{
				const FVector2D Pa = Ring[e], Pb = Ring[(e + 1) % M];
				const int32 NSeg = FMath::Max(1, FMath::FloorToInt(FVector2D::Distance(Pa, Pb) / 40));
				for (int32 k = 0; k < NSeg; k++)
				{
					const FVector2D Sp = FMath::Lerp(Pa, Pb, (k + .5) / NSeg);
					const double SX = Sp.X, SZ = Sp.Y;
					const double D = FMath::Sqrt(FMath::Square(SX - Town.SX) + FMath::Square(SZ - Town.SZ));
					if (D > 6000 || InFootprint(SX, SZ, 80) || NearMarinaPt(SX, SZ, 250) || FMath::Abs(SZ - KeysGeo::ChainZ(SX)) < 45 || KeysGeo::CreekDist(SX, SZ) < 40) continue;
					const FVector2D Out = KeysGeo::IslandOutward(I, SX, SZ);
					const double NX = Out.X, NZ = Out.Y;
					int32 Root = -1;
					for (int32 R = 0; R <= 30; R++)
					{
						const double H = KeysGeo::LandH(SX - NX * R, SZ - NZ * R);
						if (H > PierTop + .45) break;
						if (H >= PierTop - .45) { Root = R; break; }
					}
					if (Root < 0) continue;
					double Len = -1;
					bool bBad = false;
					for (int32 R = 6; R <= 150 && !bBad; R += 3)
					{
						const double PX = SX + NX * R, PZ = SZ + NZ * R;
						if (KeysGeo::LandH(PX, PZ) > -.3) { if (R > 12) bBad = true; continue; }
						if (R >= 30 && KeysGeo::DepthAt(PX, PZ) >= 2.4) { Len = R + 10; break; }
					}
					if (bBad || Len < 0) continue;
					for (double R = Len; R <= Len + PierHeadD && !bBad; R += 2) bBad = KeysGeo::LandH(SX + NX * R, SZ + NZ * R) > -.3;
					for (double R = -Root; R <= Len + PierHeadD && !bBad; R += 3)
						for (double C : {-PierHeadW / 2, 0.0, PierHeadW / 2})
							if (UnderBridge(SX + NX * R - NZ * C, SZ + NZ * R + NX * C, 16)) { bBad = true; break; }
					if (bBad) continue;
					Cands.Add({SX - NX * Root, SZ - NZ * Root, NX, NZ, Len + Root, D, I.Name});
				}
			}
		}
		Cands.Sort([](const FCand& L, const FCand& R) { return L.D < R.D; });
		for (const FCand& C : Cands)
		{
			if (A.Num() >= 6) break;
			bool bClose = false;
			for (const FPier& Pp : A)
				bClose |= FMath::Sqrt(FMath::Square(Pp.X0 + Pp.NX * Pp.Len - C.X0 - C.NX * C.Len) + FMath::Square(Pp.Z0 + Pp.NZ * Pp.Len - C.Z0 - C.NZ * C.Len)) < 900;
			if (bClose) continue;
			A.Add({C.Isl, C.X0, C.Z0, C.NX, C.NZ, C.Len});
		}
		// "<key> Fishing Pier", or West / East ones where a key has two
		TArray<FString> Labels;
		for (const FPier& Pp : A)
		{
			double MinX = 1e18, MaxX = -1e18;
			int32 Same = 0;
			for (const FPier& Q : A) if (Q.Name == Pp.Name) { Same++; MinX = FMath::Min(MinX, Q.X0); MaxX = FMath::Max(MaxX, Q.X0); }
			const TCHAR* Side = Same < 2 ? TEXT("") : Pp.X0 == MinX ? TEXT("West ") : Pp.X0 == MaxX ? TEXT("East ") : TEXT("Middle ");
			Labels.Add(FString::Printf(TEXT("%s %sFishing Pier"), *Pp.Name, Side));
		}
		for (int32 i = 0; i < A.Num(); i++)
		{
			A[i].Name = Labels[i];
			UE_LOG(LogTemp, Log, TEXT("Keys Run: %s at %.0f, %.0f"), *A[i].Name, A[i].X0, A[i].Z0);
		}
		return A;
	}

	const TArray<FEstateSite>& WaterfrontEstates()
	{
		static TArray<FEstateSite> A;
		static bool bBuilt = false;
		if (bBuilt) return A;
		bBuilt = true;
		static const uint32 Cols[] = {0xf3ead8, 0xf7d6c4, 0xcfe6e0, 0xf6e7a8, 0xd6e4f2, 0xffffff, 0xf2c6c6};
		// web world/waterfront.ts: walk each big key's real shoreline at hashed points, set the house back from the
		// water along the outward normal (the gradient of the shore distance, so a dock runs the steepest way out
		// rather than away from the island's middle), and keep sites whose dock reaches water deep enough for a lift
		int32 II = 0;
		for (const KeysGeo::FIsland& I : KeysGeo::Islands())
		{
			const TArray<TArray<FVector2D>>* Rings = KeysGeo::CoastRings(I);
			if (!Rings || !Rings->Num() || I.bSmall || I.bMiami) continue;
			const int32 IIx = II++;
			const TArray<FVector2D>& Ring = (*Rings)[0];
			const int32 M = Ring.Num();
			if (M < 3) continue;
			int32 Count = 0;
			for (int32 k = 0; k < 320 && Count < 7; k++)
			{
				const double U = KeysSea::HashCell(20240817, IIx, k, 111);   // WORLD_SEED, SALT.HOUSE_ANG
				const double F = U * M, Fl = FMath::FloorToDouble(F);
				const int32 i0 = int32(Fl) % M;
				const FVector2D Sp = FMath::Lerp(Ring[i0], Ring[(i0 + 1) % M], F - Fl);
				const double SX = Sp.X, SZ = Sp.Y;
				const FVector2D Out = KeysGeo::IslandOutward(I, SX, SZ);
				const double NX = Out.X, NZ = Out.Y;
				const double HX = SX - NX * 26, HZ = SZ - NZ * 26;   // the house, set back
				if (InFootprint(HX, HZ, 40) || KeysGeo::CreekDist(HX, HZ) < 30) continue;
				{
					double LX, LZ;
					KeysGeo::IslandLocalXZ(I, HX, HZ, LX, LZ);
					if (KeysGeo::OnCourse(&I, LX, LZ)) continue;
				}
				if (FMath::Abs(HZ - KeysGeo::ChainZ(HX)) < 20 || KeysGeo::LandH(HX, HZ) < 1.4 || KeysGeo::IslandDist(I, HX, HZ) > -14) continue;
				bool bClose = false;
				for (const FEstateSite& E : A) bClose |= FMath::Sqrt(FMath::Square(E.X - HX) + FMath::Square(E.Z - HZ)) < 150;
				for (const FPier& Pp : Piers())
					bClose |= FMath::Sqrt(FMath::Square(Pp.X0 - SX) + FMath::Square(Pp.Z0 - SZ)) < 120
						|| FMath::Sqrt(FMath::Square(Pp.X0 + Pp.NX * Pp.Len - SX) + FMath::Square(Pp.Z0 + Pp.NZ * Pp.Len - SZ)) < 160;
				if (bClose) continue;
				// the dock runs out until the lift has 1.15 m under it; DockLen is from the house
				double Reach = 0;
				for (int32 R = 10; R <= 46; R += 2)
					if (KeysGeo::DepthAt(SX + NX * R, SZ + NZ * R) >= 1.15) { Reach = R + 8; break; }
				if (Reach <= 0 || Reach > 46) continue;
				const double Dock = 26 + Reach;
				A.Add({HX, HZ, NX, NZ, Dock, int32(U * 3) % 3, Cols[(k + int32(I.X)) % 7], I.Name});
				Count++;
			}
		}
		return A;
	}

	bool GroundAt(double X, double Z, double& OutY, bool bNatural)
	{
		for (const FPier& Pp : Piers())
		{
			const double DX = X - Pp.X0, DZ = Z - Pp.Z0;
			const double A = DX * Pp.NX + DZ * Pp.NZ, C = -DX * Pp.NZ + DZ * Pp.NX;
			if (A < -2 || A > Pp.Len + PierHeadD / 2) continue;
			const double Half = A > Pp.Len - PierHeadD / 2 ? PierHeadW / 2 : PierW / 2;
			if (FMath::Abs(C) <= Half) { OutY = PierTop + .15; return true; }
		}
		for (const FEstateSite& E : WaterfrontEstates())
		{
			const double DX = X - E.X, DZ = Z - E.Z;
			const double A = DX * E.NX + DZ * E.NZ, C = -DX * E.NZ + DZ * E.NX;
			if (A > 12 && A < E.DockLen && FMath::Abs(C) <= 1.1) { OutY = 1.2; return true; }
		}
		for (const FDeckBox& D : DeckBoxes())
			if (FMath::Abs(X - D.CX) <= D.W / 2 && FMath::Abs(Z - D.CZ) <= D.D / 2) { OutY = D.Y + D.T / 2; return true; }
		double U, V;
		ToLocal(X, Z, U, V);
		if (U >= RampU0 && U <= RampU1 && V >= RampV0 && V <= RampV1) { OutY = DeckTop + (PadY - DeckTop) * ((V - RampV0) / (RampV1 - RampV0)); return true; }
		if (U >= PadU0 && U <= PadU1 && V >= PadV0 && V <= PadV1) { OutY = PadY; return true; }
		if (!bNatural) return false;
		const double H = KeysGeo::LandH(X, Z);
		if (H < .25) return false;   // the water's edge: no wading
		OutY = H;
		return true;
	}

	const TArray<FRect>& Colliders()
	{
		static TArray<FRect> A;
		if (!A.Num())
		{
			for (const FLot& Lt : Lots()) for (const FLocalBox& W : LotWalls(Lt)) A.Add(WorldRect(W.U0, W.U1, W.V0, W.V1));
			for (const FSolidProp& P : SolidProps()) A.Add(WorldRect(P.U - P.W / 2, P.U + P.W / 2, P.V - P.D / 2, P.V + P.D / 2));
			for (const FCar& C : Cars())
			{
				const double Len = C.bTrailer ? 12.8 : 4.8, Wid = 2.0;
				const double Cs = FMath::Abs(FMath::Cos(C.Rot)), Sn = FMath::Abs(FMath::Sin(C.Rot));
				const double HU = (Len * Cs + Wid * Sn) / 2, HV = (Len * Sn + Wid * Cs) / 2;
				const double Off = C.bTrailer ? 4.0 : 0;
				const double CU = C.U - FMath::Cos(C.Rot) * Off, CV = C.V - FMath::Sin(C.Rot) * Off;
				A.Add(WorldRect(CU - HU, CU + HU, CV - HV, CV + HV));
			}
			for (const FInteriorBox& B : InteriorBoxes()) A.Add(WorldRect(B.U0, B.U1, B.V0, B.V1));
			for (int32 i = 0; i < KeysGeo::Marinas().Num(); i++) A.Add(MarinaShedRect(i));
			// the hedge along the back and the sides of the pad
			A.Add(WorldRect(PadU0, PadU1, PadV1 - 1.1, PadV1));
		}
		return A;
	}

	namespace
	{
		struct FObstacle { double X, Z, AX, AZ, HA, HB; bool bRound; };
		TArray<FObstacle> GObs;
		TMap<int64, TArray<int32>> GObsGrid;
		constexpr double ObsCell = 16;
		int64 ObsKey(int32 CX, int32 CZ) { return (int64(CX) << 32) ^ int64(uint32(CZ)); }
		void AddObs(const FObstacle& O, double Reach)
		{
			const int32 Idx = GObs.Add(O);
			for (int32 CX = FMath::FloorToInt((O.X - Reach) / ObsCell); CX <= FMath::FloorToInt((O.X + Reach) / ObsCell); CX++)
				for (int32 CZ = FMath::FloorToInt((O.Z - Reach) / ObsCell); CZ <= FMath::FloorToInt((O.Z + Reach) / ObsCell); CZ++)
					GObsGrid.FindOrAdd(ObsKey(CX, CZ)).Add(Idx);
		}
	}

	void AddObstacleBox(double CX, double CZ, double AX, double AZ, double HA, double HB) { AddObs({CX, CZ, AX, AZ, HA, HB, false}, FMath::Sqrt(HA * HA + HB * HB)); }
	void AddObstacleRound(double X, double Z, double R) { AddObs({X, Z, 1, 0, R, R, true}, R); }

	bool ObstacleAt(double X, double Z, double R)
	{
		const TArray<int32>* L = GObsGrid.Find(ObsKey(FMath::FloorToInt(X / ObsCell), FMath::FloorToInt(Z / ObsCell)));
		if (!L) return false;
		for (int32 Idx : *L)
		{
			const FObstacle& O = GObs[Idx];
			const double DX = X - O.X, DZ = Z - O.Z;
			if (O.bRound) { if (DX * DX + DZ * DZ < FMath::Square(O.HA + R)) return true; continue; }
			const double A = DX * O.AX + DZ * O.AZ, B = -DX * O.AZ + DZ * O.AX;
			if (FMath::Abs(A) < O.HA + R && FMath::Abs(B) < O.HB + R) return true;
		}
		return false;
	}

	bool NearEstate(double X, double Z, double R)
	{
		for (const FEstateSite& E : WaterfrontEstates()) if (FMath::Square(E.X - X) + FMath::Square(E.Z - Z) < R * R) return true;
		return false;
	}

	void CollidersNear(double X, double Z, double R, TArray<const FRect*>& Out)
	{
		Out.Reset();
		for (const FRect& C : Colliders())
			if (X > C.X0 - R && X < C.X1 + R && Z > C.Z0 - R && Z < C.Z1 + R) Out.Add(&C);
	}

	bool FindBerth(double X, double Z, double Reach, double& OutX, double& OutZ, FString& OutName)
	{
		double Best = 1e9;
		bool bFound = false;
		const TArray<KeysGeo::FMarina>& Ms = KeysGeo::Marinas();
		for (int32 i = 0; i < DeckBoxes().Num(); i++)
		{
			const FDeckBox& D = DeckBoxes()[i];
			const double X0 = D.CX - D.W / 2, X1 = D.CX + D.W / 2, Z0 = D.CZ - D.D / 2, Z1 = D.CZ + D.D / 2;
			const double PX = FMath::Clamp(X, X0, X1), PZ = FMath::Clamp(Z, Z0, Z1);
			const double Dist = FMath::Sqrt(FMath::Square(PX - X) + FMath::Square(PZ - Z));
			if (Dist > Reach || Dist >= Best) continue;
			const double IX = FMath::Min(.45, (X1 - X0) / 2), IZ = FMath::Min(.45, (Z1 - Z0) / 2);
			OutX = FMath::Clamp(PX, X0 + IX, X1 - IX);
			OutZ = FMath::Clamp(PZ, Z0 + IZ, Z1 - IZ);
			OutName = Ms[i / 8].Name;
			Best = Dist;
			bFound = true;
		}
		// the fishing piers and the estates' private docks (oriented rectangles: walkway and T-head)
		auto Oriented = [&](double X0, double Z0, double NX, double NZ, double A0, double A1, double HalfW, const FString& Name)
		{
			const double DX = X - X0, DZ = Z - Z0;
			const double A = FMath::Clamp(DX * NX + DZ * NZ, A0 + .45, A1 - .45), C = FMath::Clamp(-DX * NZ + DZ * NX, -HalfW + .45, HalfW - .45);
			const double PX = X0 + NX * A - NZ * C, PZ = Z0 + NZ * A + NX * C;
			const double Dist = FMath::Sqrt(FMath::Square(PX - X) + FMath::Square(PZ - Z)) - .45;
			if (Dist > Reach || Dist >= Best) return;
			OutX = PX; OutZ = PZ; OutName = Name; Best = Dist; bFound = true;
		};
		for (const FPier& Pp : Piers())
		{
			Oriented(Pp.X0, Pp.Z0, Pp.NX, Pp.NZ, 8, Pp.Len - PierHeadD / 2, PierW / 2, Pp.Name);
			Oriented(Pp.X0, Pp.Z0, Pp.NX, Pp.NZ, Pp.Len - PierHeadD / 2, Pp.Len + PierHeadD / 2, PierHeadW / 2, Pp.Name);
		}
		for (const FEstateSite& E : WaterfrontEstates()) Oriented(E.X, E.Z, E.NX, E.NZ, 14, E.DockLen, 1.1, E.Island + TEXT(" - a private dock"));
		return bFound;
	}

	const FLot* InsideLot(double X, double Z)
	{
		double U, V;
		ToLocal(X, Z, U, V);
		for (const FLot& Lt : Lots())
			if (U > Lt.U0 && U < Lt.U1 && V > Lt.V0 + .2 && V < Lt.V1) return &Lt;
		return nullptr;
	}

	double MeatPrice(const FString& K)
	{
		static const TMap<FString, double> P = {
			{TEXT("mahi"), 12}, {TEXT("blackfin"), 10}, {TEXT("yellowfin"), 14}, {TEXT("bluefin"), 25}, {TEXT("albacore"), 9}, {TEXT("wahoo"), 14},
			{TEXT("swordfish"), 16}, {TEXT("yellowtail"), 14}, {TEXT("mutton"), 13}, {TEXT("mangrove"), 12}, {TEXT("grouper"), 18}, {TEXT("gag"), 18},
			{TEXT("redgrouper"), 17}, {TEXT("hogfish"), 22}, {TEXT("cobia"), 15}, {TEXT("kingfish"), 7}, {TEXT("cero"), 7}, {TEXT("amberjack"), 8},
			{TEXT("pompano"), 14}, {TEXT("tripletail"), 14}, {TEXT("sheepshead"), 8}, {TEXT("trout"), 9}, {TEXT("redfish"), 10}, {TEXT("lionfish"), 16},
			{TEXT("graytrigger"), 12}, {TEXT("blacktip"), 4}, {TEXT("jackcrevalle"), 2}, {TEXT("ladyfish"), 1}};
		const double* F = P.Find(K);
		return F ? *F : 0;
	}

	// ------------------------------------------------------------------ venues (content/venues.ts)
	const FVenue* Venue(const FString& Id)
	{
		static TArray<FVenue> A;
		if (!A.Num())
		{
			A.Add({TEXT("tackle"), TEXT("Hook, Line & Sinker Marine Supply"), TEXT("Bait - Tackle - Ice - Advice (free, wrong)"),
				{TEXT("Welcome in. Tell me what you are chasing and I will sell you the wrong thing with total confidence."),
				 TEXT("Live shrimp are in the tank by the door. Pilchards are in the freezer. Dignity is not in stock."),
				 TEXT("You want the good braid or the braid you can afford?")},
				{{TEXT("rod-heavy"), TEXT("Heavy-action trolling rod"), TEXT("Stops a run the stock rod cannot: the line holds more tension."), 2400, TEXT("rod.heavy")},
				 {TEXT("reel-lever"), TEXT("Lever-drag two-speed reel"), TEXT("Finer drag steps and a faster retrieve."), 4000, TEXT("reel.lever")},
				 {TEXT("bait-shrimp"), TEXT("Live shrimp (dozen)"), TEXT("Bite rate up on the flats."), 12, TEXT("bait.shrimp")},
				 {TEXT("bait-ballyhoo"), TEXT("Rigged ballyhoo"), TEXT("Trolling bait for mahi and wahoo."), 18, TEXT("bait.ballyhoo")},
				 {TEXT("ice-bag"), TEXT("Bag of ice"), TEXT("Keeps the cooler honest."), 4, TEXT("cooler.ice")}}});
			A.Add({TEXT("guns"), TEXT("Deadeye Guns & Ammo"), TEXT("Firearms - Ammunition - Spearguns - Pawn"),
				{TEXT("Spearguns are on the left. Everything on the right is not for fish, and not for you today."), TEXT("Browsing is free. Dropping things is not."),
				 TEXT("We do spearguns, flare guns and a very good lecture on both.")},
				{{TEXT("spear-shaft"), TEXT("Stainless spear shafts (3)"), TEXT("Replacement shafts for the diver's speargun."), 45, TEXT("spear.shafts")},
				 {TEXT("spear-long"), TEXT("Long-range band gun"), TEXT("Longer effective range underwater."), 2200, TEXT("spear.long")},
				 {TEXT("spear-reel"), TEXT("Reel-line conversion"), TEXT("A fish on the spear runs less far before you can haul."), 1400, TEXT("spear.reel")},
				 {TEXT("pistol-ammo"), TEXT("9 mm range ammo (50)"), TEXT("A box for the boat pistol. Fish, gulls and the horizon not included."), 75, TEXT("handgun.ammo")},
				 {TEXT("pistol-extmag"), TEXT("Extended magazine"), TEXT("Six more rounds before you have to stop and reload."), 1100, TEXT("handgun.extmag")},
				 {TEXT("pistol-dot"), TEXT("Red-dot sight"), TEXT("A tighter group at range, for people who do not like gulls laughing."), 1600, TEXT("handgun.dot")},
				 {TEXT("flare"), TEXT("Marine flare kit"), TEXT("Three parachute flares, fired from the pistol grip (Z). Required by the Coast Guard, best after sunset."), 38, TEXT("safety.flare")}}});
			const TArray<FShelfItem> Bar = {
				{TEXT("beer"), TEXT("Cold one"), TEXT("Lowers your cast accuracy, raises the crowd's opinion of you."), 5, TEXT("bar.beer")},
				{TEXT("round"), TEXT("A round for the bar"), TEXT("Money out, reputation in."), 250, TEXT("bar.round")},
				{TEXT("wings"), TEXT("Hot wings"), TEXT("Balances the cold one. Slightly."), 11, TEXT("bar.wings")},
				{TEXT("shot-key-lime"), TEXT("Key lime shot"), TEXT("Local colour."), 8, TEXT("bar.shot")}};
			A.Add({TEXT("bar"), TEXT("The Rusty Anchor"), TEXT("Cold beer - Hot wings - No shirt, no problem"),
				{TEXT("What'll it be? And do not tell me about the one that got away, I have heard it."), TEXT("Kitchen closes when the cook gets bored.")}, Bar});
			A.Add({TEXT("tarpon"), TEXT("The Tipsy Tarpon"), TEXT("Two for one - Fish tales told free - Mind the step"),
				{TEXT("Two for one till the ice runs out. The ice ran out in 1994."), TEXT("Sit anywhere. Not there.")}, Bar});
			A.Add({TEXT("hammered"), TEXT("Reel Hammered"), TEXT("Beer - Bait - Occasionally both"),
				{TEXT("Cooler on the left is beer. Cooler on the right is bait. Check before you drink."), TEXT("You look like a man who lost a fish.")}, Bar});
			A.Add({TEXT("club"), TEXT("Pink Flamingo Cabaret"), TEXT("Live entertainment - Cocktails - Under new management"),
				{TEXT("Boss is here! Drinks are on the house - well, on you, it's your house."), TEXT("Full room tonight. The DJ wants a raise.")},
				{{TEXT("tip"), TEXT("Tip jar"), TEXT("The band and the staff still get paid. Tip them."), 10, TEXT("club.tip")},
				 {TEXT("bottle"), TEXT("Bottle service"), TEXT("A very expensive way to feel important."), 2500, TEXT("club.bottle")}}});
			A.Add({TEXT("casino"), TEXT("Gaff & Gill Casino"), TEXT("Slots - Tarpon - Free coffee, tipped"),
				{TEXT("Chips? Coffee? A long hard look at your choices?"), TEXT("Every machine pays the same published odds. Allegedly.")},
				{{TEXT("chips"), TEXT("Pull the slots ($20)"), TEXT("Three reels of fish. Pays 2x on two of a kind, 10x on three."), 20, TEXT("casino.slot")},
				 {TEXT("coffee"), TEXT("Free coffee (tip)"), TEXT("Tipped. Not free."), 2, TEXT("casino.coffee")}}});
			A.Add({TEXT("diner"), TEXT("Salty Dog Diner"), TEXT("Breakfast all day - Fresh catch - Pie"),
				{TEXT("Sit anywhere, hon. Coffee's on."), TEXT("Pie's key lime. It's always key lime.")},
				{{TEXT("breakfast"), TEXT("Fisherman's breakfast"), TEXT("A steadier hand on the line for a while."), 14, TEXT("diner.breakfast")},
				 {TEXT("coffee"), TEXT("Bottomless coffee"), TEXT("A longer, more forgiving strike window."), 3, TEXT("diner.coffee")},
				 {TEXT("mahi-sand"), TEXT("Grilled mahi sandwich"), TEXT("Stamina for the dive."), 16, TEXT("diner.mahi")},
				 {TEXT("pie"), TEXT("Key lime pie"), TEXT("Morale. The gull will still call you a loser."), 7, TEXT("diner.pie")}}});
			A.Add({TEXT("dive"), TEXT("Scuba Steve's Dive & Spear"), TEXT("Fills - Gear - Charters"),
				{TEXT("Steve. Yes, that Steve. No, not that one."), TEXT("Long fins, low heart rate, short stories.")},
				{{TEXT("mask-wide"), TEXT("Wide-view mask"), TEXT("A little more peripheral vision down there."), 250, TEXT("dive.mask")},
				 {TEXT("fins-long"), TEXT("Long-blade freediving fins"), TEXT("Faster swim, and less air burned per metre."), 600, TEXT("dive.fins")},
				 {TEXT("wetsuit"), TEXT("3 mm shorty wetsuit"), TEXT("Warmth is air. Shivering burns it faster than swimming does."), 750, TEXT("dive.wetsuit")},
				 {TEXT("tank-pony"), TEXT("Slung pony bottle and reg"), TEXT("The big one: more than twice as long on the bottom."), 4500, TEXT("dive.tank")}}});
			A.Add({TEXT("liquor"), TEXT("Cheap Beer & Lotto"), TEXT("Wine - Spirits - Scratch-offs"),
				{TEXT("ID. Yes, you. Fine, go on."), TEXT("Scratch-offs are by the register. Hope is extra.")},
				{{TEXT("sixer"), TEXT("Six-pack for the cooler"), TEXT("The deck party notices."), 9, TEXT("liquor.sixer")},
				 {TEXT("scratch"), TEXT("Scratch-off ticket"), TEXT("A tiny chance at a big number."), 5, TEXT("liquor.scratch")}}});
			A.Add({TEXT("gifts"), TEXT("Conch Republic Gifts"), TEXT("T-shirts - Shells - Hot sauce"),
				{TEXT("Everything is ten percent off if you can find the price tag."), TEXT("The shells are real. The hot sauce is a dare.")},
				{{TEXT("shirt-hawaiian"), TEXT("Loud Hawaiian shirt"), TEXT("Changes what you wear ashore."), 35, TEXT("cosmetic.shirt.hawaiian")},
				 {TEXT("cap-conch"), TEXT("Conch Republic cap"), TEXT("Shade for the head."), 18, TEXT("cosmetic.cap.conch")},
				 {TEXT("glasses-aviator"), TEXT("Gold aviators"), TEXT("Make every fish look like a mistake."), 22, TEXT("cosmetic.glasses.aviator")},
				 {TEXT("flag-conch"), TEXT("Conch Republic flag"), TEXT("Seceded in 1982, as a joke."), 25, TEXT("cosmetic.flag.conch")}}});
			A.Add({TEXT("pawn"), TEXT("Keys Pawn & Loan"), TEXT("We buy gold - We buy tackle - No questions"),
				{TEXT("Everything in here has a story. None of them are true."), TEXT("I'll give you twenty for it. For anything.")},
				{{TEXT("used-rod"), TEXT("Used spinning rod"), TEXT("Cheap and cheerful."), 40, TEXT("rod.used")}}});
			A.Add({TEXT("laundromat"), TEXT("Suds & Sun Coin Laundry"), TEXT("Wash - Dry - Fold"),
				{TEXT("Machine four eats quarters. Machine six eats socks."), TEXT("Change machine's broken. It's always broken.")},
				{{TEXT("wash"), TEXT("Wash & dry"), TEXT("Takes the bar out of your shirt. And you."), 8, TEXT("laundromat.wash")},
				 {TEXT("sock"), TEXT("Lost-and-found sock"), TEXT("A mystery sock. Nothing happens."), 3, TEXT("laundromat.sock")}}});
			A.Add({TEXT("icecream"), TEXT("Cone Zone Ice Cream"), TEXT("Soft serve - Key lime - Shakes"),
				{TEXT("Key lime is the flavour. The other flavours are also key lime.")},
				{{TEXT("cone"), TEXT("Key lime cone"), TEXT("Cool off."), 5, TEXT("icecream.cone")}}});
			A.Add({TEXT("motel"), TEXT("Sunset Motor Lodge"), TEXT("Vacancy - Color TV - Ice machine"),
				{TEXT("Room seven's free. Room seven's always free. Don't ask why.")},
				{{TEXT("night"), TEXT("One night, room 7"), TEXT("Sleep it off: skips to sunset (or back to day)."), 79, TEXT("motel.night")}}});
			A.Add({TEXT("convenience"), TEXT("Stop-N-Go"), TEXT("Gas - Snacks - Live bait (maybe)"),
				{TEXT("Pump three's out. Pumps one, two and four are also out.")},
				{{TEXT("snacks"), TEXT("Chips and a soda"), TEXT("Morale."), 4, TEXT("conv.snacks")}}});
		}
		for (const FVenue& V : A) if (Id == V.Id) return &V;
		return nullptr;
	}
}
