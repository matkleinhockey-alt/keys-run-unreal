#include "KeysTown.h"
#include "KeysTownData.h"
#include "KeysTownKit.h"

// Boot Key Harbor City in the town style kit (KeysTownKit.h): every lot's storefront dressed from its own dice, the bare
// side walls along Palm Avenue and at the ends of the rows painted with murals or hung with fishing junk and trophies,
// trophies inside every shop, and the giant fibreglass tarpon in the town square.

using namespace KeysTown;
using namespace KeysTownKit;

namespace
{
	constexpr double PadTop = KeysTown::PadY;
	inline double Zf(double V) { return -V; }

	// the pun on each shop's blade sign (invented, like everything in town)
	FString PunFor(const FLot& L)
	{
		const FString Id = L.Id;
		if (Id == TEXT("diner")) return TEXT("FIN DINING");
		if (Id == TEXT("tackle")) return TEXT("BAIT EXPECTATIONS");
		if (Id == TEXT("guns")) return TEXT("BARRACUDA BARGAINS");
		if (Id == TEXT("liquor")) return TEXT("GILL-TY PLEASURES");
		if (Id == TEXT("bar")) return TEXT("LIVE BAIT - COLD BEER");
		if (Id == TEXT("club")) return TEXT("NAUTI BUOYS");
		if (Id == TEXT("gifts")) return TEXT("SHELL YEAH");
		if (Id == TEXT("pawn")) return TEXT("PERMIT PENDING");
		if (Id == TEXT("laundromat")) return TEXT("WASH & WAHOO");
		if (Id == TEXT("dive")) return TEXT("SEAS THE DAY");
		if (Id == TEXT("motel")) return TEXT("SNOOZE & SNOOK");
		if (Id == TEXT("icecream")) return TEXT("FIN-TASTIC SCOOPS");
		if (Id == TEXT("stopngo")) return TEXT("MULLET OVER");
		if (Id == TEXT("tarpon")) return TEXT("SILVER KING'S REST");
		if (Id == TEXT("hammered")) return TEXT("HOOKED ON YOU");
		return TEXT("FISHFUL THINKING");
	}
}

void AKeysTown::BuildKit(FTownBatch& S, FTownBatch& G, FTownBatch& I)
{
	TArray<FKitText> Texts;
	const int32 T0 = S.Tris(), I0 = I.Tris();
	for (const FLot& L : Lots())
	{
		FKitDice D(SeedOf(L.Id));
		const FLook& Lk = LookOf(L.Style);
		const FLinearColor WallC = L.Style == EStyle::Club || L.Style == EStyle::Casino ? KeysTownPaint::Hex(Lk.Wall) : KeysTownPaint::Fade(Lk.Wall);
		const double W = L.U1 - L.U0, Top = PadTop + L.H;
		// ---- the storefront
		if (L.Style != EStyle::Casino)
		{
			FShopSpots Sp;
			Sp.Front = FWall::Make(FVector(L.U0, PadTop, Zf(L.V0)), FVector(0, 0, 1));
			Sp.Width = W;
			Sp.Height = L.H;
			Sp.DoorA = L.Door - L.U0;
			Sp.DoorW = DoorW;
			const double Len = 1.55;
			Sp.AwningY = FMath::Min(2.95, L.H - .55 - Len * .26 - .75);
			Sp.Pun = PunFor(L);
			Sp.WallC = WallC;
			Sp.WallSurf = L.Style == EStyle::Bar ? SurfWood : SurfStucco;
			Sp.bAwning = Lk.Awn1 && L.Style != EStyle::Bar && L.Style != EStyle::Diner;
			// the clear wall: the corners outside the window runs, or everything but the door where there are no windows
			if (Lk.Windows == 2) { Sp.Free.Add(FVector2D(.4, Sp.DoorA - DoorW * .5 - .6)); Sp.Free.Add(FVector2D(Sp.DoorA + DoorW * .5 + .6, W - .4)); }
			else { Sp.Free.Add(FVector2D(.06, .6)); Sp.Free.Add(FVector2D(W - .6, W - .06)); }
			// a weathervane on the roof's back corner (the hero sign has the front); the bars' gables get it on the ridge
			Sp.bRoofFree = true;
			if (L.Style == EStyle::Bar) Sp.RoofTop = FVector(L.U0 + W * .22, Top + .25 + FMath::Min(2.4, (L.V1 - L.V0) * .4), Zf((L.V0 + L.V1) * .5));
			else Sp.RoofTop = FVector(D.Chance(.5) ? L.U0 + 1.3 : L.U1 - 1.3, Top + .24, Zf(L.V1) + 1.3);
			DressShop(S, G, Sp, D, Texts);
		}
		// ---- the inside: trophies above the shelves (Reel Fancy dresses its own room: the record marlin, the reef tank)
		if (L.Style != EStyle::Casino && FString(L.Id) != TEXT("diner"))
		{
			const double Sk = .03, X0 = L.U0 + WallT, X1 = L.U1 - WallT, V0 = L.V0 + WallT, V1 = L.V1 - WallT;
			const double Y = FMath::Min(L.H - .8, 3.6);
			const FWall Back = FWall::Make(FVector(X0, PadTop, Zf(V1) + Sk * 2 + .005), FVector(0, 0, 1));
			const FWall Left = FWall::Make(FVector(X0 + Sk * 2 + .005, PadTop, Zf(V0)), FVector(1, 0, 0));
			const FWall Right = FWall::Make(FVector(X1 - Sk * 2 - .005, PadTop, Zf(V1)), FVector(-1, 0, 0));
			DressInterior(I, Back, (X1 - X0) > 16 ? X1 - X0 : 0, Left, Right, V1 - V0, Y, L.Style == EStyle::Bar, D);
		}
	}
	// ---- the bare side walls: the ones on Palm Avenue and at the ends of the rows
	struct FSide { const TCHAR* Id; bool bEast; bool bMural; };
	static const FSide Sides[] = {
		{TEXT("diner"), true, true}, {TEXT("bar"), false, false}, {TEXT("dive"), true, false}, {TEXT("motel"), false, true},
		{TEXT("liquor"), false, true}, {TEXT("gifts"), true, false}, {TEXT("pawn"), false, false}, {TEXT("stopngo"), true, true},
	};
	for (const FSide& Sd : Sides)
	{
		const FLot* L = Lot(Sd.Id);
		if (!L) continue;
		FKitDice D(SeedOf(Sd.Id) ^ 0x51de);
		const FLook& Lk = LookOf(L->Style);
		const FLinearColor WallC = KeysTownPaint::Fade(Lk.Wall);
		const double Dp = L->V1 - L->V0;
		const FWall Wl = Sd.bEast ? FWall::Make(FVector(L->U1, PadTop, Zf(L->V0)), FVector(1, 0, 0)) : FWall::Make(FVector(L->U0, PadTop, Zf(L->V1)), FVector(-1, 0, 0));
		DressSideWall(S, Wl, 0, Dp, L->H, WallC, L->Style == EStyle::Bar ? SurfWood : SurfStucco, Sd.bMural, D, Texts);
	}
	// ---- the town square: the giant fibreglass tarpon on the old plinth
	{
		FKitDice D(SeedOf(TEXT("big tarp")));
		Statue(S, FVector(16, PadTop + 1.1, Zf(24)), 6.6, D, &Texts);
	}
	for (const FKitText& Tx : Texts)
	{
		const double Yaw = FMath::RadiansToDegrees(FMath::Atan2(Tx.Facing.Z, Tx.Facing.X)) - 90;
		AddSign(Tx.Text, Tx.P.X, Tx.P.Y, -Tx.P.Z, Tx.Size, Tx.Ink, Yaw, Tx.bInterior, Tx.Width);
	}
	UE_LOG(LogTemp, Log, TEXT("Keys Run: town kit - %d tris on the street, %d inside, %d signs"), S.Tris() - T0, I.Tris() - I0, Texts.Num());
}
