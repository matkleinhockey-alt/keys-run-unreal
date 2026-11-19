#include "KeysEateries.h"
#include "KeysTown.h"
#include "KeysTownData.h"
#include "KeysTownKit.h"
#include "KeysBoatPawn.h"
#include "KeysSave.h"
#include "KeysData.h"
#include "Components/TextRenderComponent.h"
#include "Kismet/GameplayStatics.h"

// The town's three restaurants, each with its own fish personality (everything invented):
//  - Reel Fancy (the old diner on Harbor Drive): white tablecloths, a reef tank down the west wall, the house-record blue
//    marlin hanging over the bar
//  - Bait & Brew (the old bait shop, now the bar): dollar bills on every wall, a jukebox, a neon jumping fish, faded
//    photos of local catches, the old bait tank full of beer and ice
//  - The Gutted Grouper (a shack on the promenade): picnic tables under a sail, buckets of shells, a chalkboard whose
//    catch of the day is whatever you actually caught today, and a fish-cleaning station out back with pelicans waiting

using namespace KeysTown;
using namespace KeysTownKit;

namespace
{
	constexpr double PadTop = KeysTown::PadY;
	const FVector UpV(0, 1, 0);
	FVector V3(double U, double Y, double V) { return FVector(U, Y, -V); }
	FLinearColor Hx(uint32 C) { return KeysTownPaint::Hex(C); }
	FLinearColor Mul(const FLinearColor& A, double K) { return FLinearColor(A.R * float(K), A.G * float(K), A.B * float(K), 1); }
	FLinearColor Mix(const FLinearColor& A, const FLinearColor& B, double T) { FLinearColor C = A + (B - A) * float(T); C.A = 1; return C; }
	// the text facing a horizontal direction (three frame) as AddSign's yaw
	double YawOf(const FVector& F) { return FMath::RadiansToDegrees(FMath::Atan2(F.Z, F.X)) - 90; }

	// a box from u0..u1, y0..y1, v0..v1 in linear colour
	void Blk(FTownBatch& B, double U0, double U1, double Y0, double Y1, double Va, double Vb, const FLinearColor& C)
	{
		B.SpanC(U0, U1, Y0, Y1, -FMath::Max(Va, Vb), -FMath::Min(Va, Vb), C);
	}
	// a chair (seat, legs, back) at (U, V) whose sitter faces Face (a horizontal three-frame direction)
	void Chair(FTownBatch& B, double U, double V, double Y0, const FVector& Face, const FLinearColor& Wood, const FLinearColor& Seat)
	{
		const FVector F = Face.GetSafeNormal(), Sd = FVector::CrossProduct(UpV, F);
		const FVector C = V3(U, Y0, V);
		KitBox(B, C + UpV * .46, F, UpV, Sd, .22, .03, .22, Seat);
		for (int32 a = -1; a <= 1; a += 2) for (int32 b = -1; b <= 1; b += 2) KitBox(B, C + F * (a * .18) + Sd * (b * .18) + UpV * .22, F, UpV, Sd, .02, .22, .02, Wood);
		KitBox(B, C - F * .21 + UpV * .75, F, UpV, Sd, .02, .28, .21, Wood);
	}
	// a stubby beer bottle or a can standing at P
	void Bottle(FTownBatch& B, const FVector& Pp, bool bCan, FKitDice& D)
	{
		if (bCan) { KitTube(B, Pp, Pp + UpV * .12, .033, .033, 8, D.Chance(.5) ? Hx(0xc9ccd2) : Hx(0x2f6fd0)); return; }
		KitTube(B, Pp, Pp + UpV * .15, .03, .03, 7, Hx(0x5a3412));
		KitTube(B, Pp + UpV * .15, Pp + UpV * .22, .03, .012, 7, Hx(0x5a3412));
		KitTube(B, Pp + UpV * .03, Pp + UpV * .1, .031, .031, 7, D.Chance(.5) ? Hx(0xe8d8a0) : Hx(0xd8473a));
	}
	// a paper-lined basket of something fried
	void Basket(FTownBatch& B, const FVector& Pp, const FVector& F, FKitDice& D, bool bTacos)
	{
		const FVector Sd = FVector::CrossProduct(UpV, F);
		KitBox(B, Pp + UpV * .025, F, UpV, Sd, .12, .025, .09, Hx(0xc8312a));
		KitBox(B, Pp + UpV * .052, F, UpV, Sd, .11, .003, .08, Hx(0xf4efe2));
		if (bTacos)
			for (int32 k = -1; k <= 1; k += 2)
			{
				KitEllip(B, Pp + UpV * .075 + Sd * (k * .04), F, UpV, Sd, .1, .03, .028, Hx(0xe6c27a), 6);
				KitEllip(B, Pp + UpV * .1 + Sd * (k * .04), F, UpV, Sd, .08, .012, .02, Hx(0x7bb04a), 5);
			}
		else
			for (int32 k = 0; k < 6; k++)
				KitEllip(B, Pp + UpV * .08 + F * D.R(-.07, .07) + Sd * D.R(-.05, .05), F, UpV, Sd, .03, .026, .03, Mul(Hx(0xb87a32), D.R(.85, 1.1)), 5);
	}
	// a galvanized bucket of shells
	void ShellBucket(FTownBatch& B, const FVector& Pp, FKitDice& D)
	{
		KitTube(B, Pp, Pp + UpV * .2, .1, .125, 10, Hx(0xa7adb2));
		KitTube(B, Pp + UpV * .2, Pp + UpV * .21, .127, .127, 10, Hx(0x8a9096));
		static const uint32 Sh[] = {0xf4ece0, 0xf0c8b8, 0xe8d8b8, 0xd8b8a0, 0xf8f0f0};
		for (int32 k = 0; k < 9; k++)
		{
			const double A = D.R(0, 6.28), R = D.R(0, .08);
			KitEllip(B, Pp + UpV * D.R(.2, .25) + FVector(FMath::Cos(A) * R, 0, FMath::Sin(A) * R), FVector(1, 0, 0), UpV, FVector(0, 0, 1), D.R(.025, .04), .018, D.R(.02, .035), Hx(Sh[k % 5]), 5);
		}
		for (int32 k = 0; k < 3; k++)
		{
			const double A = D.R(0, 6.28);
			KitEllip(B, Pp + UpV * .012 + FVector(FMath::Cos(A) * .22, 0, FMath::Sin(A) * .22), FVector(1, 0, 0), UpV, FVector(0, 0, 1), .035, .012, .03, Hx(Sh[(k + 2) % 5]), 5);
		}
	}
	// a neon tube along a polyline, flat on a wall (in the glow batch)
	void Neon(FTownBatch& G, const FWall& W, const TArray<FVector2D>& Pts, bool bClosed, double D, double Wd, const FLinearColor& C)
	{
		const int32 N = Pts.Num();
		for (int32 i = 0; i < N - (bClosed ? 0 : 1); i++)
		{
			const FVector2D A = Pts[i], E = Pts[(i + 1) % N];
			const FVector2D Dd = (E - A).GetSafeNormal(), Nn(-Dd.Y, Dd.X);
			G.Quad3(W.At(A.X + Nn.X * Wd, A.Y + Nn.Y * Wd, D), W.At(E.X + Nn.X * Wd, E.Y + Nn.Y * Wd, D), W.At(E.X - Nn.X * Wd, E.Y - Nn.Y * Wd, D), W.At(A.X - Nn.X * Wd, A.Y - Nn.Y * Wd, D), W.N, C);
		}
	}
	// a fish outline in neon: Kind's pieces scaled by Len, turned by Tilt, centred at (A, Y) on the wall
	void NeonFish(FTownBatch& G, const FWall& W, double A, double Y, double Len, double Tilt, int32 Kind, double D, const FLinearColor& Body, const FLinearColor& Fins)
	{
		const TArray<TArray<FVector2D>> Pcs = KitFishOutline(Kind);
		const double C = FMath::Cos(Tilt), Sn = FMath::Sin(Tilt);
		for (int32 p = 0; p < Pcs.Num(); p++)
		{
			TArray<FVector2D> Pts;
			for (const FVector2D& Q : Pcs[p]) Pts.Add(FVector2D(A + (Q.X * C - Q.Y * Sn) * Len, Y + (Q.X * Sn + Q.Y * C) * Len));
			Neon(G, W, Pts, true, D + p * .001, .022, p == 0 ? Body : Fins);
		}
	}
	// a framed snapshot of a catch: sky and sea, an angler and a fish held up (faded, a bit yellow)
	void Photo(FTownBatch& B, const FWall& W, double A, double Y, double Wd, double Ht, FKitDice& D)
	{
		const FLinearColor Frame = D.Chance(.4) ? Hx(0x1a1a1a) : D.Chance(.5) ? Hx(0xf2efe6) : Hx(0x6b4a2b);
		KitBox(B, W.At(A, Y, .012), W.R, UpV, W.N, Wd * .5 + .02, Ht * .5 + .02, .012, Frame);
		const double Fade = D.R(.25, .5);
		auto Sep = [&](uint32 C) { FLinearColor X = Weathered(Hx(C), Fade, D); X.R = FMath::Min(1.f, X.R * 1.06f); X.B *= .88f; return X; };
		const double F = .0255;
		B.TriCol(W.At(A - Wd * .5, Y, F), W.At(A + Wd * .5, Y, F), W.At(A + Wd * .5, Y + Ht * .5, F), W.N, Sep(0x9fd0e8), Sep(0x9fd0e8), Sep(0x6fb0dc));
		B.TriCol(W.At(A - Wd * .5, Y, F), W.At(A + Wd * .5, Y + Ht * .5, F), W.At(A - Wd * .5, Y + Ht * .5, F), W.N, Sep(0x9fd0e8), Sep(0x6fb0dc), Sep(0x6fb0dc));
		B.Quad3(W.At(A - Wd * .5, Y - Ht * .5, F), W.At(A + Wd * .5, Y - Ht * .5, F), W.At(A + Wd * .5, Y, F), W.At(A - Wd * .5, Y, F), W.N, Sep(D.Chance(.5) ? 0x2a8aa8 : 0x8a6a48));
		// the angler: legs, shirt, head, a cap
		const double Pa = A + D.R(-.2, .2) * Wd, Ps = Ht / 1.0;
		const FLinearColor Shirt = Sep(D.Chance(.5) ? 0xe8e4d8 : D.Chance(.5) ? 0x3a6ab0 : 0xd8473a);
		B.Quad3(W.At(Pa - Ps * .09, Y - Ht * .5, F + .001), W.At(Pa + Ps * .09, Y - Ht * .5, F + .001), W.At(Pa + Ps * .1, Y - Ht * .05, F + .001), W.At(Pa - Ps * .1, Y - Ht * .05, F + .001), W.N, Sep(0x3a4250));
		B.Quad3(W.At(Pa - Ps * .14, Y - Ht * .07, F + .001), W.At(Pa + Ps * .14, Y - Ht * .07, F + .001), W.At(Pa + Ps * .12, Y + Ht * .2, F + .001), W.At(Pa - Ps * .12, Y + Ht * .2, F + .001), W.N, Shirt);
		KitSlab(B, KitEllipse(0, 0, Ps * .065, Ps * .08, 10), W.At(Pa, Y + Ht * .29, F + .001), W.R, UpV, W.N, .001, Sep(0xc98a62), Sep(0xc98a62), false);
		// the fish across their chest
		static const TCHAR* const Keys[] = {TEXT("snook"), TEXT("tarpon"), TEXT("mahi"), TEXT("permit"), TEXT("barracuda"), TEXT("grouper"), TEXT("mangrove"), TEXT("redfish")};
		const double FL = Wd * D.R(.55, .85);
		KitFish(B, Keys[D.I(8)], FL, W.At(Pa + D.R(-.05, .05) * Wd, Y + Ht * .04, F + .006), W.R * (D.Chance(.5) ? 1.0 : -1.0), UpV, W.N, .05, .03, Fade, .3, D);
	}
}

// ==================================================================== the table talk
FString AKeysTown::TableTalk(const FString& Inside, int32 Act, int32 Pick)
{
	static const TArray<FString> Fancy = {TEXT("The snapper is unreal."), TEXT("Is this from the tank? Don't tell me."), TEXT("He swears that marlin is real."),
		TEXT("We should charter tomorrow."), TEXT("Worth every penny. Mostly."), TEXT("Cheers - to the one that got away.")};
	static const TArray<FString> Brew = {TEXT("These fritters are hot!"), TEXT("Another round!"), TEXT("Tarpon were rolling at the bridge."),
		TEXT("Wind's up tomorrow. Reef's off."), TEXT("Stuck a dollar up in '98. Still there."), TEXT("Don't drink from the left cooler.")};
	static const TArray<FString> Shack = {TEXT("Best tacos in the Keys."), TEXT("Get your own fries, pelican."), TEXT("Snook were biting at the creek mouth."),
		TEXT("That boat's been out since five."), TEXT("Pass the hot sauce."), TEXT("This dip is ridiculous.")};
	const TArray<FString>& L = Inside == TEXT("diner") ? Fancy : Inside == TEXT("bar") ? Brew : Shack;
	return L[FMath::Abs(Pick * 7 + Act * 3) % L.Num()];
}

// ==================================================================== the menus (read with F)
namespace KeysEateries
{
	FString CatchOfTheDay(const UKeysSave* Sv, FString* OutKey, double* OutWeight, int32* OutDaysAgo)
	{
		if (!Sv) return FString();
		// the newest fish from the last three days (the recent list; today's list for saves from before it)
		const TArray<FKeysBoardCatch>& L = Sv->RecentCatches.Num() ? Sv->RecentCatches : Sv->TodayCatches;
		const FDateTime N = FDateTime::Now(), D0(N.GetYear(), N.GetMonth(), N.GetDay());
		for (int32 i = L.Num() - 1; i >= 0; i--)
		{
			const FKeysBoardCatch& C = L[i];
			FDateTime Dt;
			if (!FDateTime::ParseIso8601(*C.Date, Dt)) continue;
			const int32 Ago = int32((D0 - Dt).GetTotalDays());
			if (Ago < 0 || Ago > 3) continue;
			if (OutKey) *OutKey = C.Key;
			if (OutWeight) *OutWeight = C.Weight;
			if (OutDaysAgo) *OutDaysAgo = Ago;
			const KeysData::FSpecies* Sp = KeysData::Species(C.Key);
			return Sp ? Sp->Name : C.Key;
		}
		return FString();
	}

	FString AlsoFresh(const UKeysSave* Sv)
	{
		if (!Sv) return FString();
		const TArray<FKeysBoardCatch>& L = Sv->RecentCatches.Num() ? Sv->RecentCatches : Sv->TodayCatches;
		TArray<FString> Out, Seen;
		for (int32 i = L.Num() - 2; i >= 0 && Out.Num() < 3; i--)
		{
			const FKeysBoardCatch& C = L[i];
			if (Seen.Contains(C.Key) || (L.Num() && C.Key == L.Last().Key)) continue;
			Seen.Add(C.Key);
			const KeysData::FSpecies* Sp = KeysData::Species(C.Key);
			Out.Add(FString::Printf(TEXT("%s %d"), *(Sp ? Sp->Name : C.Key).ToUpper(), FMath::Max(1, FMath::RoundToInt(C.Weight))));
		}
		return Out.Num() ? TEXT("ALSO FRESH: ") + FString::Join(Out, TEXT(" - ")) : FString();
	}

	bool Menu(const FString& Id, const UKeysSave* Sv, FString& Title, FString& Sub, TArray<FString>& Lines, FString& Foot)
	{
		Lines.Reset();
		if (Id == TEXT("grouper"))
		{
			Title = TEXT("The Gutted Grouper");
			Sub = TEXT("Fish shack - Dockside - Cash only");
			double Wt = 0;
			int32 Ago = 0;
			const FString Cd = CatchOfTheDay(Sv, nullptr, &Wt, &Ago);
			Lines.Add(Cd.IsEmpty() ? TEXT("CATCH OF THE DAY - nothing yet. Bring us one, we'll cook it.")
				: FString::Printf(TEXT("CATCH OF THE DAY - %s (%.0f lb), caught by you %s. Fried, blackened or grilled - $14"), *Cd, Wt, Ago == 0 ? TEXT("today") : Ago == 1 ? TEXT("yesterday") : TEXT("this week")));
			const FString Also = AlsoFresh(Sv);
			if (!Also.IsEmpty()) Lines.Add(Also.Replace(TEXT("ALSO FRESH: "), TEXT("Also fresh, from your boat: ")).Replace(TEXT(" - "), TEXT(", ")));
			Lines.Add(TEXT("Fish tacos - blackened catch, slaw, lime - $12"));
			Lines.Add(TEXT("Conch fritters - six, key lime mayo - $9"));
			Lines.Add(TEXT("Smoked fish dip - saltines and hot sauce - $10"));
			Lines.Add(TEXT("Key lime pie - $6"));
			Lines.Add(TEXT("Bucket of beers - $5 a bottle"));
			Foot = TEXT("Order at the window. Pelicans eat free (they don't).");
			return true;
		}
		if (Id == TEXT("fancy"))
		{
			Title = TEXT("Reel Fancy");
			Sub = TEXT("Fine seafood - Fresh from the reef");
			Lines.Add(TEXT("Smoked fish dip, house style - $14"));
			Lines.Add(TEXT("Conch fritters, remoulade - $16"));
			Lines.Add(TEXT("Yellowtail fish tacos, mango - $22"));
			Lines.Add(TEXT("Pan-seared mahi, mango salsa - $34"));
			Lines.Add(TEXT("Hogfish meuniere, key lime butter - $38"));
			Lines.Add(TEXT("Key lime pie, torched meringue - $12"));
			Foot = TEXT("Ask your server about the record marlin. Actually, don't. He'll never stop.");
			return true;
		}
		if (Id == TEXT("brew"))
		{
			Title = TEXT("Bait & Brew");
			Sub = TEXT("Kitchen open till the cook gets bored");
			Lines.Add(TEXT("Conch fritters - $11"));
			Lines.Add(TEXT("Smoked fish dip - $9"));
			Lines.Add(TEXT("Fish tacos (Tuesdays, and also other days) - $10"));
			Lines.Add(TEXT("Key lime pie - $7"));
			Lines.Add(TEXT("Cold one - $5    Key lime shot - $8"));
			Foot = TEXT("Live bait sold separately. Do not eat the live bait.");
			return true;
		}
		return false;
	}
}

// ==================================================================== the Gutted Grouper's chalkboard
void AKeysTown::UpdateCatchBoard()
{
	if (CatchBoard.Num() < 3) return;
	const AKeysBoatPawn* Pn = Cast<AKeysBoatPawn>(UGameplayStatics::GetPlayerPawn(this, 0));
	const UKeysSave* Sv = Pn ? Pn->Board() : nullptr;
	double Wt = 0;
	int32 Ago = 0;
	const FString Cd = KeysEateries::CatchOfTheDay(Sv, nullptr, &Wt, &Ago);
	const FString Also = KeysEateries::AlsoFresh(Sv);
	const FString Sig = Cd + FString::Printf(TEXT("%.1f%d"), Wt, Ago) + Also;
	if (Sig == CatchBoardShown) return;
	CatchBoardShown = Sig;
	const FString When = Ago == 0 ? TEXT("today") : Ago == 1 ? TEXT("yesterday") : TEXT("this week");
	if (CatchBoard[0]) CatchBoard[0]->SetText(FText::FromString(Cd.IsEmpty() ? FString(TEXT("NOTHING YET")) : Cd.ToUpper()));
	if (CatchBoard[1]) CatchBoard[1]->SetText(FText::FromString(Cd.IsEmpty() ? FString(TEXT("bring us one - we'll cook it")) : FString::Printf(TEXT("%.0f lb - caught by you %s - fried, blackened or grilled"), Wt, *When)));
	if (CatchBoard[2]) CatchBoard[2]->SetText(FText::FromString(Also));
}

// ==================================================================== building them
void AKeysTown::BuildEateries(FTownBatch& S, FTownBatch& G, FTownBatch& I)
{
	const double Y0 = PadTop + .07;
	const uint8 Keep = S.M.Surf;

	// ------------------------------------------------------------------ Reel Fancy (the old diner)
	{
		FKitDice D(SeedOf(TEXT("reel fancy")));
		const double H = 4.4, CY = PadTop + H - .1;
		// the reef tank down the west wall: a cabinet, the lit water behind, sand, rock, coral, sea grass, the frame
		const double TU0 = -31.6, TU1 = -30.6, TV0 = 47.4, TV1 = 68.6, TY0 = Y0 + .6, TY1 = Y0 + 2.95;
		I.M.Surf = SurfWood;
		Blk(I, TU0, TU1 + .02, Y0, TY0, TV0, TV1, Hx(0x2a1a10));
		Blk(I, TU0, TU1 + .02, TY1, TY1 + .3, TV0, TV1, Hx(0x2a1a10));
		I.M.Surf = SurfDefault;
		{
			const FLinearColor Top = Hx(0x58b8d0), Bot = Hx(0x14507a);
			const double X = TU0 + .11;
			G.TriCol(V3(X, TY0, TV0), V3(X, TY0, TV1), V3(X, TY1, TV1), FVector(1, 0, 0), Bot, Bot, Top);
			G.TriCol(V3(X, TY0, TV0), V3(X, TY1, TV1), V3(X, TY1, TV0), FVector(1, 0, 0), Bot, Top, Top);
			G.SpanC(TU0 + .1, TU1 - .05, TY1 - .03, TY1, -TV1, -TV0, Hx(0xdff6ff));
		}
		Blk(I, TU0 + .07, TU1 - .02, TY0, TY0 + .12, TV0, TV1, Hx(0xdcc89a));
		for (double V = TV0 + .5; V < TV1 - .3; V += D.R(.6, 1.4))
		{
			const double U = D.R(TU0 + .25, TU1 - .2);
			const int32 K = D.I(4);
			if (K == 0) KitEllip(I, V3(U, TY0 + .2, V), FVector(1, 0, 0), UpV, FVector(0, 0, 1), .22, D.R(.12, .3), D.R(.25, .5), Mul(Hx(0x7a7468), D.R(.8, 1.1)), 7);
			else if (K == 1)
			{
				static const uint32 CC[] = {0xe86a8a, 0xf09040, 0x8a5ac0, 0xf2d43a};
				const FLinearColor Cc = Hx(CC[D.I(4)]);
				for (int32 b = 0; b < 5; b++)
				{
					const FVector Bse = V3(U, TY0 + .12, V);
					const FVector Tip = Bse + FVector(D.R(-.15, .15), D.R(.25, .55), D.R(-.25, .25));
					KitTube(I, Bse, Tip, .03, .012, 5, Cc);
				}
			}
			else if (K == 2) KitEllip(I, V3(U, TY0 + .2, V), FVector(1, 0, 0), UpV, FVector(0, 0, 1), .16, .13, .16, Hx(D.Chance(.5) ? 0xd8b04a : 0x9ac06a), 7);
			else
				for (int32 b = 0; b < 4; b++)
				{
					const FVector Bse = V3(U, TY0 + .12, V + D.R(-.15, .15));
					KitTube(I, Bse, Bse + FVector(D.R(-.05, .05), D.R(.7, 1.6), D.R(-.15, .15)), .015, .006, 4, Hx(0x4a8a3a));
				}
		}
		// the glass: mullions and rails on the room side
		for (double V = TV0; V <= TV1 + .01; V += (TV1 - TV0) / 10) Blk(I, TU1 - .02, TU1 + .03, TY0, TY1, V - .03, V + .03, Hx(0x2a1a10));
		// the tables: white cloth to the floor, a candle, two places set; the chairs (whoever sits there faces in)
		const FVector2D Tables[] = {{-26.5, 50.5}, {-21.5, 50.5}, {-13.5, 50.5}, {-26.5, 55.5}, {-17.5, 55.5}, {-11.5, 55.5}};
		for (const FVector2D& T : Tables)
		{
			const FVector C = V3(T.X, Y0, T.Y);
			I.M.Surf = SurfCloth;
			KitTube(I, C, C + UpV * .76, .68, .62, 16, Hx(0xf6f4ee));
			KitSlab(I, KitEllipse(0, 0, .62, .62, 16), C + UpV * .76, FVector(1, 0, 0), FVector(0, 0, 1), UpV, .006, Hx(0xfaf8f2), Hx(0xf6f4ee), false);
			I.M.Surf = SurfDefault;
			for (int32 s = -1; s <= 1; s += 2)
			{
				Chair(I, T.X, T.Y + s * .95, Y0, FVector(0, 0, double(s)), Hx(0x3a2414), Hx(0x15263a));
				// a plate with dinner on it, a glass, the cutlery
				const FVector Pl = C + UpV * .768 + FVector(0, 0, -s * .33);
				KitSlab(I, KitEllipse(0, 0, .13, .13, 12), Pl, FVector(1, 0, 0), FVector(0, 0, 1), UpV, .012, Hx(0xffffff), Hx(0xe8e8e4), false);
				KitEllip(I, Pl + UpV * .025, FVector(1, 0, 0), UpV, FVector(0, 0, 1), .07, .02, .04, Hx(D.Chance(.5) ? 0xe8c890 : 0xd89a6a), 6);
				KitEllip(I, Pl + UpV * .03 + FVector(.05, 0, 0), FVector(1, 0, 0), UpV, FVector(0, 0, 1), .02, .012, .02, Hx(0x5aa03a), 5);
				const FVector Gl = Pl + FVector(.17, 0, -s * .1);
				KitTube(I, Gl, Gl + UpV * .11, .005, .005, 4, Hx(0xdfe8ea));
				KitEllip(I, Gl + UpV * .15, FVector(1, 0, 0), UpV, FVector(0, 0, 1), .035, .045, .035, D.Chance(.5) ? Hx(0x8a2a3a) : Hx(0xe8e0b0), 6);
				KitBox(I, Pl + FVector(-.18, .003, 0), FVector(1, 0, 0), UpV, FVector(0, 0, 1), .012, .002, .09, Hx(0xc0c4c8));
			}
			// the candle in its glass
			KitTube(I, C + UpV * .77, C + UpV * .85, .03, .03, 8, Hx(0xe8e4d8));
			G.Ball(T.X, Y0 + .875, -T.Y, .015, 0xffc060, 1.4, 5);
			HaloList.Add({V3(T.X, Y0 + .9, T.Y), .5, FLinearColor(1.f, .75f, .4f), false});
		}
		// the host stand by the door with the menu open on it
		{
			const FVector C = V3(-17, Y0, 47.5);
			KitBox(I, C + UpV * .55, FVector(1, 0, 0), UpV, FVector(0, 0, 1), .32, .55, .2, Hx(0x2a1a10));
			KitBox(I, C + UpV * 1.1, FVector(1, 0, 0), UpV, FVector(0, 0, 1), .36, .02, .24, Hx(0xc9a24a));
			const FVector Bk = C + UpV * 1.13;
			for (int32 s = -1; s <= 1; s += 2) KitBox(I, Bk + FVector(s * .11, .01, 0), (FVector(1, 0, 0) * double(s) + UpV * .1).GetSafeNormal(), UpV, FVector(0, 0, 1), .11, .004, .15, Hx(0xfbf8ef));
			AddSign(TEXT("PLEASE WAIT TO BE SEATED"), -17, Y0 + .8, 47.29, .045, Hx(0xc9a24a), 0, true, .6);
		}
		// the house record blue marlin, hanging on wires over the bar, leaping, its flank to the room
		{
			const FVector Mc = V3(-21.5, PadTop + 3.15, 60.9);
			const double Tilt = .22;
			const FVector Nose = FVector(FMath::Cos(Tilt), FMath::Sin(Tilt), 0), Up2(-FMath::Sin(Tilt), FMath::Cos(Tilt), 0);
			KitFish(I, TEXT("marlin"), 3.7, Mc, Nose, Up2, FVector(0, 0, 1), .22, 1, 0, .25, D);
			for (double K : {-1.0, 1.0}) KitTube(I, Mc + Nose * (K * .95) + UpV * .25, V3(-21.5 + K * .95 * FMath::Cos(Tilt), CY, 60.9), .004, .004, 3, Hx(0xc0c4c8));
			// its brass plate on the front of the bar
			KitBox(I, V3(-24.5, Y0 + .7, 59.97), FVector(1, 0, 0), UpV, FVector(0, 0, 1), .42, .09, .01, Hx(0xc9a24a));
			AddSign(TEXT("HOUSE RECORD - BLUE MARLIN - 1,004 LB"), -24.5, Y0 + .7, 59.95, .05, Hx(0x2a2418), 0, true, .8);
		}
		// the east wall: framed sea charts, two trophies above them, warm sconces
		{
			const FWall E = FWall::Make(V3(-8.46, PadTop, 69.6), FVector(-1, 0, 0));
			for (int32 k = 0; k < 3; k++)
			{
				const double A = 4.5 + k * 6.5;
				KitBox(I, E.At(A, 1.95, .015), E.R, UpV, E.N, .55, .4, .015, Hx(0x2a1a10));
				I.Quad3(E.At(A - .5, 1.6, .031), E.At(A + .5, 1.6, .031), E.At(A + .5, 2.3, .031), E.At(A - .5, 2.3, .031), E.N, Hx(0xe8dcbc));
				for (int32 b = 0; b < 4; b++)
				{
					const double Ba = A + D.R(-.35, .3), By = D.R(1.7, 2.15);
					KitSlab(I, KitEllipse(0, 0, D.R(.06, .18), D.R(.04, .1), 9), E.At(Ba, By, .032), E.R, UpV, E.N, .001, Hx(0x8cbcd8), Hx(0x8cbcd8), false);
				}
				G.Ball(-8.6, PadTop + 2.65, -(69.6 - A - 3.25), .06, 0xffd28a, 1.3, 6);
				HaloList.Add({V3(-8.7, PadTop + 2.65, 69.6 - A - 3.25), .8, FLinearColor(1.f, .8f, .5f), false});
			}
			MountedFish(I, E, 6, 3.4, TEXT("mahi"), 1.3, 1, D);
			MountedFish(I, E, 15, 3.4, TEXT("permit"), .8, 0, D);
		}
	}

	// ------------------------------------------------------------------ Bait & Brew (the old bait shop)
	{
		FKitDice D(SeedOf(TEXT("bait and brew")));
		const double X0 = 8.4, X1 = 33.6, V0 = 46.4, V1 = 69.6, H = 4.8, CY = PadTop + H - .1;
		// dollar bills, signed and stapled up over sixty years: every wall and the ceiling over the bar
		auto Bills = [&](const FWall& W, double A0, double A1, double BY0, double BY1, int32 N, double SkipA0 = 0, double SkipA1 = 0, double SkipY = 0)
		{
			for (int32 k = 0; k < N; k++)
			{
				const double A = D.R(A0, A1), Y = D.R(BY0, BY1);
				if (A > SkipA0 && A < SkipA1 && Y < SkipY) continue;
				const double Rt = D.R(-.6, .6), Cs = FMath::Cos(Rt), Sn = FMath::Sin(Rt);
				const FVector Ax = W.R * Cs + UpV * Sn, Ay = UpV * Cs - W.R * Sn;
				const FVector C = W.At(A, Y, .004 + D.R(0, .003));
				const FLinearColor Col = Mul(D.Chance(.2) ? Hx(0xd8cfa0) : D.Chance(.5) ? Hx(0xc9d4b2) : Hx(0xb5c49a), D.R(.85, 1.05));
				I.Quad3(C - Ax * .078 - Ay * .033, C + Ax * .078 - Ay * .033, C + Ax * .078 + Ay * .033, C - Ax * .078 + Ay * .033, W.N, Col);
			}
		};
		const FWall Back = FWall::Make(V3(X0, PadTop, V1 - .06), FVector(0, 0, 1));
		const FWall West = FWall::Make(V3(X0 + .06, PadTop, V0), FVector(1, 0, 0));
		const FWall East = FWall::Make(V3(X1 - .06, PadTop, V1), FVector(-1, 0, 0));
		const FWall Front = FWall::Make(V3(X1, PadTop, V0 + .06), FVector(0, 0, -1));
		Bills(Back, .3, X1 - X0 - .3, 1.3, 3.0, 260);
		Bills(West, .3, V1 - V0 - .3, 1.3, 3.9, 170, 13.4, 21.8, 2.7);
		Bills(East, .3, V1 - V0 - .3, 1.3, 3.2, 150);
		Bills(Front, .3, 10.5, 1.3, 3.9, 70);
		Bills(Front, 14.5, X1 - X0 - .3, 1.3, 3.9, 70);
		for (int32 k = 0; k < 220; k++)
		{
			const double U = D.R(11, 31), V = D.R(59.5, 66), Rt = D.R(0, 3.14);
			const FVector Ax(FMath::Cos(Rt), 0, FMath::Sin(Rt)), Az(-FMath::Sin(Rt), 0, FMath::Cos(Rt));
			const FVector C = V3(U, CY - .004 - D.R(0, .003), V);
			I.Quad3(C - Ax * .078 - Az * .033, C - Ax * .078 + Az * .033, C + Ax * .078 + Az * .033, C + Ax * .078 - Az * .033, -UpV, Mul(Hx(0xc4cfa8), D.R(.8, 1.05)));
		}
		// the jukebox by the door: a wood cabinet, its arch of neon, the record window, the grille
		{
			const double JU = 26.8, JV = 46.9;
			const FVector F(0, 0, -1), Sd(1, 0, 0);
			I.M.Surf = SurfWood;
			KitBox(I, V3(JU, Y0 + .7, JV), Sd, UpV, F, .58, .7, .34, Hx(0x6b3a1a));
			KitEllip(I, V3(JU, Y0 + 1.4, JV), Sd, UpV, F, .58, .38, .34, Hx(0x6b3a1a), 12);
			I.M.Surf = SurfDefault;
			const FWall JF = FWall::Make(V3(JU, Y0, JV + .35), F);
			TArray<FVector2D> Arch1, Arch2;
			for (int32 k = 0; k <= 12; k++)
			{
				const double A = PI * k / 12;
				Arch1.Add(FVector2D(-FMath::Cos(A) * .5, 1.4 + FMath::Sin(A) * .32));
				Arch2.Add(FVector2D(-FMath::Cos(A) * .42, 1.4 + FMath::Sin(A) * .25));
			}
			for (FVector2D& Q : Arch1) Q.X = -Q.X;
			for (FVector2D& Q : Arch2) Q.X = -Q.X;
			Neon(G, JF, Arch1, false, .01, .025, Hx(0xff9f2a));
			Neon(G, JF, Arch2, false, .012, .02, Hx(0xff3ea5));
			Neon(G, JF, {FVector2D(-.5, .15), FVector2D(-.5, 1.4)}, false, .01, .025, Hx(0xffd84a));
			Neon(G, JF, {FVector2D(.5, .15), FVector2D(.5, 1.4)}, false, .01, .025, Hx(0xffd84a));
			I.Quad3(JF.At(-.36, .95, .012), JF.At(.36, .95, .012), JF.At(.36, 1.45, .012), JF.At(-.36, 1.45, .012), F, Hx(0x1a1c20));
			for (int32 k = 0; k < 5; k++) KitSlab(I, KitEllipse(0, 0, .09, .09, 10), JF.At(-.28 + k * .14, 1.2, .015), JF.R, UpV, F, .002, Hx(0x0c0c0e), Hx(0x0c0c0e), false);
			for (int32 k = 0; k < 7; k++) KitBox(I, JF.At(0, .3 + k * .07, .015), JF.R, UpV, F, .38, .008, .006, Hx(0xc0c4c8));
			HaloList.Add({V3(JU, Y0 + 1.5, JV + .5), 1.3, FLinearColor(1.f, .55f, .3f), false});
		}
		// the neon jumping fish over the back bar, with its splash
		{
			NeonFish(G, Back, 19.5, 3.85, 1.8, .45, 0, .09, Hx(0x38e0ff), Hx(0xff3ea5));
			for (int32 k = 0; k < 3; k++)
			{
				TArray<FVector2D> Wv;
				for (int32 j = 0; j <= 10; j++) Wv.Add(FVector2D(18.2 + j * .17, 3.12 - k * .1 + FMath::Sin(j * 1.3 + k) * .05));
				Neon(G, Back, Wv, false, .09, .018, k == 1 ? Hx(0xffa040) : Hx(0x38a0ff));
			}
			Blk(I, X0 + 17.9, X0 + 21.1, PadTop + 2.85, PadTop + 4.6, V1 - .05, V1 - .1, Hx(0x101214));
			for (int32 k = 0; k < 4; k++) HaloList.Add({Back.At(18.2 + k * .8, 3.7, .3), 1.2, FLinearColor(.3f, .85f, 1.f), false});
		}
		// faded snapshots of local catches on the west wall
		for (int32 k = 0; k < 14; k++)
		{
			const double A = 14.0 + (k % 7) * 1.1 + D.R(-.15, .15), Y = k < 7 ? 2.15 + D.R(-.06, .06) : 1.55 + D.R(-.06, .06);
			const double Wd = D.R(.32, .5), Ht = Wd * D.R(.7, 1.25);
			Photo(I, West, A, Y, Wd, Ht, D);
		}
		// the old bait tank, full of ice and beer now; "LIVE BAIT" still on the side
		{
			Blk(I, 8.5, 9.7, Y0, Y0 + .85, 56.5, 59, Hx(0xeef0ec));
			Blk(I, 8.6, 9.6, Y0 + .85, Y0 + .86, 56.6, 58.9, Hx(0x3a7aa8));
			for (int32 k = 0; k < 22; k++) KitBox(I, V3(D.R(8.65, 9.55), Y0 + .87, D.R(56.65, 58.85)), FVector(1, 0, 0), UpV, FVector(0, 0, 1), .05, .03, .05, Hx(0xf4fbff));
			for (int32 k = 0; k < 10; k++) Bottle(I, V3(D.R(8.7, 9.5), Y0 + .8, D.R(56.7, 58.8)), D.Chance(.3), D);
			AddSign(TEXT("LIVE BAIT"), 9.72, Y0 + .55, 57.75, .16, Mul(Hx(0x2a6fb0), .8), -90, true, 2.2);
		}
		// the old tackle-shop gear still on the east wall
		OldRods(I, East, 5.5, 2.4, D);
		CastNet(I, East, 10.5, 2.9, D);
		// three tables with their beers and baskets
		const FVector2D Tbl[] = {{19.1, 52}, {24.1, 52}, {21.6, 56.5}};
		for (const FVector2D& T : Tbl)
		{
			const FVector C = V3(T.X, Y0, T.Y);
			I.M.Surf = SurfWood;
			KitBox(I, C + UpV * .74, FVector(1, 0, 0), UpV, FVector(0, 0, 1), .45, .025, .45, Hx(0x5a3a22));
			KitTube(I, C, C + UpV * .72, .05, .05, 6, Hx(0x2a2c2e));
			I.M.Surf = SurfDefault;
			for (int32 s = -1; s <= 1; s += 2) Chair(I, T.X, T.Y + s * .85, Y0, FVector(0, 0, double(s)), Hx(0x3a2414), Hx(0x2b2118));
			for (int32 k = 0; k < 3; k++) Bottle(I, C + UpV * .765 + FVector(D.R(-.3, .3), 0, D.R(-.3, .3)), D.Chance(.3), D);
			Basket(I, C + UpV * .765 + FVector(D.R(-.15, .15), 0, 0), FVector(1, 0, 0), D, false);
		}
		// the menu board on the bar
		{
			const FVector C = V3(15, Y0 + 1.08, 60.7);
			for (int32 s = -1; s <= 1; s += 2) KitBox(I, C + UpV * .2 + FVector(0, 0, s * .05), FVector(1, 0, 0), (UpV + FVector(0, 0, s * .25)).GetSafeNormal(), FVector(0, 0, 1), .22, .2, .008, Hx(0x1f2a24));
			AddSign(TEXT("MENU"), 15, Y0 + 1.36, 60.62, .06, Hx(0xf2efe0), 0, true, .4);
			AddSign(TEXT("fritters - dip - tacos - pie"), 15, Y0 + 1.27, 60.62, .03, Hx(0xf2efe0), 0, true, .4);
		}
	}

	// ------------------------------------------------------------------ the Gutted Grouper (a shack on the promenade)
	{
		FKitDice D(SeedOf(TEXT("gutted grouper")));
		TArray<FKitText> Texts;
		const double P = PadTop;
		const double SU0 = -51, SU1 = -45, SV0 = 19.9, SV1 = 24.9, WH = 2.7, FH = 3.15;   // walls; the roof is higher at the front (east)
		const FLinearColor Paint1 = Weathered(Paint(EPaint::Seafoam), .4, D), Trim = Weathered(Paint(EPaint::WhiteWash), .35, D);
		// plank walls (each board its own shade), the window cut in the east wall
		S.M.Surf = SurfWood;
		auto Planks = [&](double U0, double U1, double Ya, double Yb, double Va, double Vb)
		{
			for (double Y = Ya; Y < Yb - .01; Y += .18) Blk(S, U0, U1, Y, FMath::Min(Yb, Y + .17), Va, Vb, Mul(Paint1, D.R(.88, 1.04)));
		};
		Planks(SU0, SU1, P, P + WH, SV0, SV0 + .1);              // back (toward the water)
		Planks(SU0, SU1, P, P + WH, SV1 - .1, SV1);              // the street side
		Planks(SU0, SU0 + .1, P, P + WH, SV0, SV1);              // west
		// east: around the window (v 21.2..23.6, y 1.0..2.1)
		Planks(SU1 - .1, SU1, P, P + 1.0, SV0, SV1);
		Planks(SU1 - .1, SU1, P + 2.1, P + FH, SV0, SV1);
		Planks(SU1 - .1, SU1, P + 1.0, P + 2.1, SV0, 21.2);
		Planks(SU1 - .1, SU1, P + 1.0, P + 2.1, 23.6, SV1);
		for (double V : {SV0, SV1}) for (double U : {SU0, SU1}) Blk(S, U - .06, U + .06, P, P + (U > -48 ? FH : WH), V - .06, V + .06, Trim);
		// the gable ends up to the sloping roof
		for (double V : {SV0 + .05, SV1 - .05})
			S.Quad3(V3(SU0, P + WH, V), V3(SU1, P + WH, V), V3(SU1, P + FH, V), V3(SU0, P + WH, V), FVector(0, 0, V < 22 ? 1 : -1), Paint1);
		// the counter shelf and the propped tin shutter over the window
		Blk(S, SU1, SU1 + .45, P + .98, P + 1.05, 21.1, 23.7, Weathered(Hx(0x8a6a48), .2, D));
		S.M.Surf = SurfTin;
		KitBox(S, V3(SU1 + .45, P + 2.25, 22.4), FVector(.6, .8, 0).GetSafeNormal(), FVector(-.8, .6, 0).GetSafeNormal(), FVector(0, 0, 1), .45, .015, 1.25, Hx(0xb8bcc2));
		// the tin roof, sloping back to the water side... it slopes west, down from the window
		{
			const double Ov = .45;
			const FVector A0 = V3(SU1 + Ov, P + FH + .08, SV0 - Ov), A1 = V3(SU1 + Ov, P + FH + .08, SV1 + Ov), B1 = V3(SU0 - Ov, P + WH - .05, SV1 + Ov), B0 = V3(SU0 - Ov, P + WH - .05, SV0 - Ov);
			const FVector Nr = FVector::CrossProduct(A1 - A0, B0 - A0).GetSafeNormal();
			const FVector Nu = Nr.Y > 0 ? Nr : -Nr;
			for (int32 k = 0; k < 16; k++)
			{
				const double T0 = k / 16.0, T1 = (k + 1) / 16.0;
				const FLinearColor Tc = k % 2 ? Hx(0xaeb3b8) : Hx(0x9aa0a6);
				S.Quad3(FMath::Lerp(A0, A1, T0), FMath::Lerp(A0, A1, T1), FMath::Lerp(B0, B1, T1), FMath::Lerp(B0, B1, T0), Nu, k == 5 || k == 11 ? Mix(Tc, Rust(D), .5) : Tc);
				S.Quad3(FMath::Lerp(A0, A1, T0) - UpV * .02, FMath::Lerp(B0, B1, T0) - UpV * .02, FMath::Lerp(B0, B1, T1) - UpV * .02, FMath::Lerp(A0, A1, T1) - UpV * .02, -Nu, Hx(0x6a6e72));
			}
		}
		S.M.Surf = SurfDefault;
		// inside, through the window: the back wall, the fryer, a shelf of hot sauce, the ticket rail, a warm lamp
		Blk(S, SU0 + .1, SU0 + .15, P, P + WH, SV0 + .1, SV1 - .1, Hx(0x2a2420));
		Blk(S, SU0 + .15, SU1 - .1, P, P + .02, SV0 + .1, SV1 - .1, Hx(0x5a4a3a));
		Blk(S, -47.3, -46.3, P, P + .95, 21.3, 22.3, Hx(0xb8bec4));
		Blk(S, -47.2, -46.4, P + .95, P + 1.0, 21.4, 22.2, Hx(0x8a6a2a));
		Blk(S, SU0 + .15, SU0 + .45, P + 1.5, P + 1.53, SV0 + .3, SV1 - .3, Hx(0x6b4a2b));
		for (double V = SV0 + .4; V < SV1 - .4; V += .14) KitTube(S, V3(SU0 + .3, P + 1.53, V), V3(SU0 + .3, P + 1.72, V), .025, .02, 5, Hx(D.Chance(.6) ? 0xc8312a : 0x3a7a2a));
		G.Ball(-47.5, P + 2.4, -22.4, .09, 0xffd090, 1, 6);
		for (int32 k = 0; k < 5; k++) KitBox(S, V3(SU1 - .14, P + 2.0, 21.4 + k * .45), FVector(1, 0, 0), UpV, FVector(0, 0, 1), .002, .07, .05, Hx(0xf4f0e0));
		// the big fish sign on the street side, a plank sign under it, "ORDER HERE" over the window
		{
			const FWall Nw = FWall::Make(V3(SU1, P, SV1), FVector(0, 0, -1));
			const FLinearColor Body = Weathered(Hx(0x7a6a3a), .2, D), Fin = Weathered(Hx(0x4a5a2a), .2, D);
			const FVector Sc = Nw.At(3.0, 1.85, .06);
			KitCutout(S, 3, Sc, Nw.R * -1.0, UpV, 3.3, .06, Body, Fin, Hx(0x2a2418), true);
			Texts.Add({TEXT("THE GUTTED GROUPER"), Sc + Nw.R * (-.05) + UpV * -.05 + Nw.N * .045, Nw.N, .2, 2.2, Hx(0xf4ecd8)});
			PlankSign(S, Nw, 3.0, .75, 2.6, TEXT("FRIED - BLACKENED - GRILLED"), D, &Texts);
			const FWall Ew = FWall::Make(V3(SU1, P, SV0), FVector(1, 0, 0));
			PlankSign(S, Ew, 2.5, 2.45, 1.8, TEXT("ORDER HERE"), D, &Texts);
			// fishing junk on the shack, traps and rope by it
			const FWall Ww = FWall::Make(V3(SU0, P, SV1), FVector(-1, 0, 0));
			CastNet(S, Ww, 1.4, 2.4, D);
			NetFloats(S, Ww, 2.8, 2.4, D);
			Buoy(S, Ww, 3.8, 2.3, .5, D);
			Buoy(S, Nw, 5.3, 2.3, .45, D);
			LobsterTraps(S, V3(SU0 - .7, P, SV1 - .8), 0, 3, D);
			RopeCoil(S, V3(SU0 - .6, P + .02, SV0 + 1.0), UpV, .28, D);
			SaltStains(S, Nw, 0, 6, .01, Paint1, D, SurfWood);
		}
		// the shade sail on posts over the picnic tables, festoon lights along its edges
		{
			const double PU0 = -43.1, PU1 = -35.3, PV0 = 19.5, PV1 = 27.1, PH = 2.9;
			S.M.Surf = SurfWood;
			for (double U : {PU0, PU1}) for (double V : {PV0, PV1}) KitTube(S, V3(U, P, V), V3(U, P + PH + .1, V), .06, .06, 6, Weathered(Hx(0x8a6a48), .3, D));
			S.M.Surf = SurfCloth;
			const FVector A = V3(PU0, P + PH, PV0), B = V3(PU1, P + PH + .25, PV0), C = V3(PU1, P + PH, PV1), Dd = V3(PU0, P + PH + .25, PV1), M = V3((PU0 + PU1) / 2, P + PH - .15, (PV0 + PV1) / 2);
			const FLinearColor Sail = Hx(0xf2ece0);
			for (const FVector* E : {&A, &B, &C, &Dd})
			{
				const FVector& E2 = E == &A ? B : E == &B ? C : E == &C ? Dd : A;
				const FVector Nn = FVector::CrossProduct(E2 - *E, M - *E).GetSafeNormal();
				S.Tri3(*E, E2, M, Nn.Y < 0 ? -Nn : Nn, Sail);
				S.Tri3(*E, M, E2, Nn.Y < 0 ? Nn : -Nn, Sail);
			}
			S.M.Surf = SurfDefault;
			for (int32 e = 0; e < 4; e++)
			{
				const FVector Pa = e == 0 ? A : e == 1 ? B : e == 2 ? C : Dd, Pb = e == 0 ? B : e == 1 ? C : e == 2 ? Dd : A;
				for (int32 k = 0; k <= 8; k++)
				{
					const double T = k / 8.0;
					const FVector Q = FMath::Lerp(Pa, Pb, T) - UpV * (FMath::Sin(T * PI) * .25 + .05);
					G.Ball(Q.X, Q.Y, Q.Z, .04, 0xffe2a0, 1.2, 5);
					if (k % 3 == 1) HaloList.Add({Q, .6, FLinearColor(1.f, .85f, .55f), false});
				}
			}
		}
		// the picnic tables: weathered paint, benches both sides, shell buckets, baskets, beers, hot sauce
		const FVector2D Pic[] = {{-41.5, 21}, {-37, 21}, {-41.5, 25.6}, {-37, 25.6}};
		for (int32 t = 0; t < 4; t++)
		{
			const FVector2D T = Pic[t];
			const FVector C = V3(T.X, P, T.Y);
			const FLinearColor Wd = t % 2 ? SaltWood(D) : Weathered(RandomPastel(D), .45, D);
			S.M.Surf = SurfWood;
			KitBox(S, C + UpV * .75, FVector(1, 0, 0), UpV, FVector(0, 0, 1), .95, .03, .38, Wd);
			for (int32 s = -1; s <= 1; s += 2) KitBox(S, C + UpV * .44 + FVector(0, 0, s * .7), FVector(1, 0, 0), UpV, FVector(0, 0, 1), .95, .025, .14, Mul(Wd, .92));
			for (int32 s = -1; s <= 1; s += 2)
				for (int32 q = -1; q <= 1; q += 2)
					KitTube(S, C + FVector(s * .72, 0, q * .78), C + FVector(s * .72, .75, q * .05), .035, .035, 4, Mul(Wd, .8));
			S.M.Surf = SurfDefault;
			const FVector Top = C + UpV * .78;
			ShellBucket(S, Top + FVector(D.R(-.2, .2), 0, 0), D);
			Basket(S, Top + FVector(-.55, 0, -.18), FVector(1, 0, 0), D, t % 2 == 0);
			Basket(S, Top + FVector(.55, 0, .18), FVector(1, 0, 0), D, t % 2 == 1);
			for (int32 k = 0; k < 3; k++) Bottle(S, Top + FVector(D.R(-.8, .8), 0, D.R(-.25, .25)), D.Chance(.5), D);
			KitTube(S, Top + FVector(.25, 0, .2), Top + FVector(.25, .14, .2), .022, .018, 6, Hx(0xc8312a));
			KitTube(S, Top + FVector(-.3, 0, .22), Top + FVector(-.3, .22, .22), .055, .055, 8, Hx(0xf6f4ee));
		}
		// the chalkboard: catch of the day (from today's real catches) and the menu
		{
			const double CU = -43.6, CV = 25.6, Ang = .26;
			const FVector Nf(FMath::Cos(Ang), FMath::Sin(Ang), 0), Uf(-FMath::Sin(Ang), FMath::Cos(Ang), 0), Wf(0, 0, 1);
			const FVector Cb = V3(CU, P + .62, CV);
			for (int32 s = -1; s <= 1; s += 2)
			{
				const FVector Ns(Nf.X * s, Nf.Y, 0), Us(Uf.X * s, Uf.Y, 0);
				const FVector Cc = Cb + FVector(s * .16, 0, 0);
				KitBox(S, Cc, Wf, Us, Ns, .4, .6, .022, Hx(0x6b4a2b));
				KitBox(S, Cc + Ns * .023, Wf, Us, Ns, .35, .55, .002, Hx(0x1f2a24));
			}
			const FVector Face(1, 0, 0);
			const double TxU = CU + .2;
			auto Chalk = [&](const FString& Txt, double Y, double Sz, uint32 Col) { return AddSign(Txt, TxU + (P + .62 - Y) * FMath::Tan(Ang) * -1 + .03, Y, CV, Sz, Hx(Col), YawOf(Face), false, .7); };
			Chalk(TEXT("CATCH OF THE DAY"), P + 1.08, .06, 0xf2d86a);
			CatchBoard.Add(Chalk(TEXT("NOTHING YET"), P + .97, .085, 0xf6f4ee));
			CatchBoard.Add(Chalk(TEXT("bring us one - we'll cook it"), P + .895, .032, 0xd8e8e0));
			CatchBoard.Add(Chalk(TEXT(""), P + .835, .03, 0xf2d86a));
			Chalk(TEXT("FISH TACOS ........ 12"), P + .76, .04, 0xf6f4ee);
			Chalk(TEXT("CONCH FRITTERS ..... 9"), P + .68, .04, 0xf6f4ee);
			Chalk(TEXT("SMOKED FISH DIP ... 10"), P + .6, .04, 0xf6f4ee);
			Chalk(TEXT("KEY LIME PIE ....... 6"), P + .52, .04, 0xf6f4ee);
			Chalk(TEXT("cash only"), P + .4, .035, 0xf2a0a0);
			UpdateCatchBoard();
		}
		// the fish-cleaning station out back by the seawall, and the pelicans waiting on the rail
		{
			const double CU = -48, CV = 18.9;
			const FVector C = V3(CU, P, CV);
			KitBox(S, C + UpV * .9, FVector(1, 0, 0), UpV, FVector(0, 0, 1), 1.1, .03, .4, Hx(0xeef0ec));
			for (int32 a = -1; a <= 1; a += 2) for (int32 b = -1; b <= 1; b += 2) KitTube(S, C + FVector(a * 1.0, 0, b * .32), C + FVector(a * 1.0, .88, b * .32), .025, .025, 5, Hx(0xc0c4c8));
			KitBox(S, C + UpV * 1.2 + FVector(0, 0, .38), FVector(1, 0, 0), UpV, FVector(0, 0, 1), 1.1, .3, .02, Hx(0xeef0ec));
			Texts.Add({TEXT("PLEASE DON'T FEED THE PELICANS"), V3(CU, P + 1.32, CV - .4), FVector(0, 0, -1), .05, 1.9, Hx(0xc8312a)});
			Texts.Add({TEXT("(THEY'LL TELL YOU THEY'RE STARVING. THEY'RE NOT.)"), V3(CU, P + 1.22, CV - .4), FVector(0, 0, -1), .03, 1.9, Hx(0x2a2a2a)});
			// the tap and a hose coiled on its hook
			KitTube(S, C + FVector(.8, .93, .3), C + FVector(.8, 1.3, .3), .018, .018, 5, Hx(0xc0c4c8));
			KitTube(S, C + FVector(.8, 1.3, .3), C + FVector(.8, 1.28, .15), .015, .012, 5, Hx(0xc0c4c8));
			for (int32 k = 0; k < 3; k++)
			{
				TArray<FVector> Ring;
				for (int32 j = 0; j <= 10; j++) { const double A = 2 * PI * j / 10; Ring.Add(C + FVector(-1.25 + FMath::Cos(A) * .18, .95 + FMath::Sin(A) * .18 - k * .02, .38 - k * .02)); }
				for (int32 j = 0; j < 10; j++) KitTube(S, Ring[j], Ring[j + 1], .014, .014, 4, Hx(0x2f7a3a));
			}
			// a cutting board with a snapper on it, half done, the knife beside it; a bucket of scraps
			KitBox(S, C + UpV * .94 + FVector(-.2, 0, 0), FVector(1, 0, 0), UpV, FVector(0, 0, 1), .32, .012, .2, Hx(0xe0d8c8));
			KitFish(S, TEXT("mutton"), .5, C + UpV * 1.0 + FVector(-.2, 0, 0), FVector(1, 0, 0), FVector(0, 0, 1), UpV, 0, .6, .1, .2, D);
			KitBox(S, C + UpV * .955 + FVector(.25, 0, .1), FVector(1, 0, .3).GetSafeNormal(), UpV, FVector(-.3, 0, 1).GetSafeNormal(), .1, .002, .012, Hx(0xd8dde2));
			KitBox(S, C + UpV * .958 + FVector(.4, 0, .145), FVector(1, 0, .3).GetSafeNormal(), UpV, FVector(-.3, 0, 1).GetSafeNormal(), .06, .01, .014, Hx(0x1a1a1a));
			KitTube(S, C + FVector(.5, 0, -.15), C + FVector(.5, .35, -.15), .14, .16, 10, Hx(0xf4f0e8));
			for (int32 k = 0; k < 5; k++) KitEllip(S, C + FVector(.5 + D.R(-.08, .08), .36, -.15 + D.R(-.08, .08)), FVector(1, 0, 0), UpV, FVector(0, 0, 1), .05, .03, .04, Hx(D.Chance(.5) ? 0xd88a8a : 0xc0c8cc), 5);
			// the pelicans: three on the rail posts behind the station, one on the ground beside it, one up on the shack
			Pelican(S, V3(-51, P + 1.06, 18.4), FVector(0, 0, -1), D.R(-.4, .4), D);
			Pelican(S, V3(-47, P + 1.06, 18.4), FVector(.2, 0, -1), D.R(-.4, .4), D);
			Pelican(S, V3(-43, P + 1.06, 18.4), FVector(-.6, 0, -1), D.R(-.4, .4), D);
			Pelican(S, V3(-45.4, P, 19.2), FVector(-1, 0, .2), .3, D);
			Pelican(S, V3(-49.8, P + WH + .3, 22.4), FVector(1, 0, 0), -.3, D);
		}
		for (const FKitText& Tx : Texts) AddSign(Tx.Text, Tx.P.X, Tx.P.Y, -Tx.P.Z, Tx.Size, Tx.Ink, YawOf(Tx.Facing), Tx.bInterior, Tx.Width);
	}

	// ------------------------------------------------------------------ Bait & Brew's front: the neon fish on the roof
	{
		const FLot* L = Lot(TEXT("bar"));
		if (L)
		{
			const double Top = PadTop + L->H, U = L->U0 + 4.6, Fv = L->V0 + .6;
			S.M.Surf = SurfDefault;
			for (double Du : {-1.5, 1.5}) Blk(S, U + Du - .06, U + Du + .06, Top, Top + 1.4, Fv - .06, Fv + .06, Hx(0x2d3136));
			Blk(S, U - 2.0, U + 2.0, Top + 1.2, Top + 3.4, Fv - .1, Fv + .1, Hx(0x15171a));
			const FWall Sw = FWall::Make(V3(U, Top, Fv - .1), FVector(0, 0, 1));
			NeonFish(G, Sw, 0, 2.55, 2.3, .45, 0, .012, Hx(0x38e0ff), Hx(0xff3ea5));
			for (int32 k = 0; k < 2; k++)
			{
				TArray<FVector2D> Wv;
				for (int32 j = 0; j <= 12; j++) Wv.Add(FVector2D(-1.6 + j * .27, 1.55 - k * .1 + FMath::Sin(j * 1.4 + k * 2) * .06));
				Neon(G, Sw, Wv, false, .012, .02, k ? Hx(0xffa040) : Hx(0x38a0ff));
			}
			AddSign(TEXT("BAIT & BREW"), U, Top + 1.33, Fv - .12 - .02, .2, Hx(0xffd84a), 0, false, 3.6);
			for (int32 k = 0; k < 4; k++) HaloList.Add({V3(U - 1.4 + k * .95, Top + 2.4, Fv - .4), 1.6, FLinearColor(.3f, .85f, 1.f), false});
		}
	}
	S.M.Surf = Keep;
	I.M.Surf = SurfDefault;
}
