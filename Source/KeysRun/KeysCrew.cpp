// The people, party, shower, dance pole and flags aboard the player's boat (JS makeHuman / updateParty / updatePole /
// updateShower / makeFlag). Part of AKeysExtras; the actor itself is in KeysExtras.cpp.
#include "KeysExtras.h"
#include "KeysBoatPawn.h"
#include "KeysData.h"
#include "KeysMesh.h"
#include "KeysWorld.h"
#include "ProceduralMeshComponent.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"

namespace
{
	FLinearColor XHex(uint32 C) { return FLinearColor(FColor(uint8((C >> 16) & 255), uint8((C >> 8) & 255), uint8(C & 255))); }
	double XRand(double A, double B) { return FMath::FRandRange(A, B); }
	double XWrapPi(double A) { return FMath::Fmod(FMath::Fmod(A + PI, 2 * PI) + 2 * PI, 2 * PI) - PI; }
	double XAngLerp(double A, double B, double T) { return A + XWrapPi(B - A) * FMath::Min(1.0, T); }
	FVector XJ(double X, double Y, double Z) { return FKeysHuman::J(X, Y, Z); }

	struct FXWomanLook { uint32 Hair, Skin; int32 Style; };
	const FXWomanLook XWomanLooks[] = {
		{0x3b2416, 0xd9a27f, 0}, {0xead27f, 0xe9b48f, 1}, {0x5a3825, 0xc98d68, 2}, {0x1c1410, 0xb87a55, 0}, {0x2e1c12, 0xe2ad88, 1},
		{0x7a3a1c, 0xf0c2a0, 1}, {0x4a2c1a, 0x9a6644, 3}, {0xd8b46a, 0xdba886, 2}, {0x3f2818, 0xcf9672, 0}};
	int32 XWomanI = 0;

	FKeysHumanLook XWoman(uint32 Suit, bool bGlasses)
	{
		const FXWomanLook& W = XWomanLooks[XWomanI++ % UE_ARRAY_COUNT(XWomanLooks)];
		FKeysHumanLook L;
		L.bBikini = true;
		L.bLongHair = true;
		L.Bottom = XHex(Suit);
		L.Hair = XHex(W.Hair);
		L.Skin = XHex(W.Skin);
		L.HairStyle = W.Style;
		L.bGlasses = bGlasses;
		return L;
	}

	const FLinearColor XChrome(.86f, .88f, .9f);
}

// ---------------------------------------------------------------- deck shape of KeysMesh::BuildBoat
void FKeysDeck::Init(const KeysData::FBoatSpec& S)
{
	L = S.Len;
	HB = S.Beam * .5;
	Free = KeysMesh::HullOf(S).F;
	Spec = &S;
	const KeysMesh::FBoatLayout Lo = KeysMesh::BoatLayout(S);
	DeckZ = Lo.DeckZ; MidX = Lo.MidX; TopZ = Lo.TopZ; TopL = Lo.TopL; TopW = Lo.TopW; TopX = Lo.TopX;
	ConL = Lo.ConL; ConW = Lo.ConW; ConFwd = Lo.ConFwd;
}

double FKeysDeck::SoleAt(double X) const
{
	return Spec ? KeysMesh::SoleAt(*Spec, X) : DeckZ;
}

double FKeysDeck::InnerHB(double X) const
{
	return Spec ? KeysMesh::InnerHalfBeam(*Spec, X) : HB - .14;
}

// ---------------------------------------------------------------- layout
void AKeysExtras::BuildLayout()
{
	AKeysBoatPawn* P = PawnRef.Get();
	if (!P) return;
	const KeysData::FBoatSpec& S = P->Spec();
	LayoutFor = S.Id;
	Deck.Init(S);
	const double L = Deck.L;
	XWomanI = 0;

	// captain at the helm, just aft of the console, hands on the wheel
	{
		FKeysHumanLook C;
		C.Shirt = XHex(0x9cc8e0); C.Bottom = XHex(0xb9a77c); C.bShortsLong = true; C.Hair = XHex(0x4a3020);
		C.bCap = true; C.CapColor = XHex(0x1d3557); C.bShoes = true; C.Shoes = XHex(0xf2f2f2); C.bGlasses = true;
		Captain.Init(C, false);
		// the Midnight's wheel stands proud of its dash, so its captain stands a little further aft, clear of the seats
		const double X = Deck.MidX - Deck.ConL * .5 - (S.Id == TEXT("midnight") ? .42 : .14);
		Captain.Pos = FVector(X, .12, Deck.SoleAt(X));
		Captain.Yaw = 0;
		HelmL = XJ(-.1, 1.1, .12);
		HelmR = XJ(.16, 1.12, .12);
		Captain.Pose(HelmL, HelmR);
	}

	uint32 Suit = 0xe4572e;
	if (S.Id == TEXT("robalo")) Suit = 0x2bb3a8;
	else if (S.Id == TEXT("freeman")) Suit = 0xe86a92;
	else if (S.Id == TEXT("midnight")) Suit = 0xf2c14e;
	else if (S.Id == TEXT("mti")) Suit = 0x2f6fd0;

	FKeysMesh Props;
	// crew on the seat in front of the console
	{
		Crew.Init(XWoman(Suit, true), true);
		const double X = Deck.MidX + Deck.ConL * .5 + Deck.ConFwd + .25;
		Crew.Pos = FVector(X, 0, Deck.SoleAt(X));
		Crew.Yaw = 0;
		// the Freeman and the Midnight Express have their own seat on the console's front
		// (the go-fasts with a sun pad have no console seat of their own)
		if (S.bSunpad && S.Id != TEXT("midnight")) Props.AddBox(FVector(X - .02, 0, Deck.SoleAt(X) + .21), FVector(.42, .62, .42), XHex(0xeeeae0));
	}

	// where nobody stands: the console and the seat on its front, the helm seats or leaning post, the Midnight's wet
	// bar, the Freeman's tuna chair (KeysMesh::BuildBoat lays these out from the same KeysMesh::BoatLayout)
	const bool bMidL = S.Id == TEXT("midnight");
	double SeatX0, SeatX1, SeatHalf;
	{
		KeepOut.Reset();
		const double ConA = Deck.MidX - Deck.ConL * .5, ConF = Deck.MidX + Deck.ConL * .5 + Deck.ConFwd;
		KeepOut.Add(FBox2D(FVector2D(ConA - .1, -Deck.ConW * .5 - .1), FVector2D(ConF + (bMidL ? 1.0 : .6), Deck.ConW * .5 + .1)));
		const int32 NSeat = S.bRodRack ? 4 : S.Beam > 3.6 ? 3 : 2;
		if (bMidL) { SeatX0 = Deck.MidX - 3.05; SeatX1 = Deck.MidX - 1.17; SeatHalf = 1.38; }
		else { SeatX0 = Deck.MidX - 1.68; SeatX1 = Deck.MidX - .9; SeatHalf = S.bLeaning ? .58 : (NSeat - 1) * (S.Id == TEXT("freeman") ? .37 : .32) + .36; }
		KeepOut.Add(FBox2D(FVector2D(SeatX0, -SeatHalf), FVector2D(SeatX1, SeatHalf)));
		if (bMidL) KeepOut.Add(FBox2D(FVector2D(Deck.MidX - 4.07, -.78), FVector2D(Deck.MidX - 3.33, .78)));
		if (KeysMesh::HasFightChair(S))
		{
			const FVector CB = KeysMesh::FightChairBase(S);
			KeepOut.Add(FBox2D(FVector2D(CB.X - .62, -.62), FVector2D(CB.X + .62, .62)));
		}
	}

	// party path: a lap around the console (JS makePath)
	{
		auto Lane = [this](double X) { return FMath::Max(.35, Deck.InnerHB(X) - .32); };
		const double Zc = FMath::Min(L * .33, L * .39 - .6);
		const double XF = FMath::Min(L * .25, FMath::Max(Deck.MidX + 1.6, Deck.MidX + Deck.ConL * .5 + Deck.ConFwd + 1.05));
		// past the helm seats (or the leaning post) the lane swings out round them, so nobody walks through the chairs:
		// the seat row's half-width plus a shoulder, as far as the gunwale allows
		const double SX0 = SeatX0 - .13, SX1 = SeatX1 + .1;
		auto SeatLane = [&](double X) { return FMath::Max(Lane(X), FMath::Min(SeatHalf + .3, Deck.InnerHB(X) - .16)); };
		const bool bRound = SX0 > -Zc + .3 && SX1 < L * .1 - .3;
		PathPts.Reset();
		PathPts.Add(FVector2D(-Zc, Lane(-Zc)));
		if (bRound) { PathPts.Add(FVector2D(SX0, SeatLane(SX0))); PathPts.Add(FVector2D(SX1, SeatLane(SX1))); }
		PathPts.Add(FVector2D(L * .1, Lane(L * .1)));
		PathPts.Add(FVector2D(XF, 0));
		PathPts.Add(FVector2D(L * .1, -Lane(L * .1)));
		if (bRound) { PathPts.Add(FVector2D(SX1, -SeatLane(SX1))); PathPts.Add(FVector2D(SX0, -SeatLane(SX0))); }
		PathPts.Add(FVector2D(-Zc, -Lane(-Zc)));
		PathPts.Add(FVector2D(-Zc - .15, 0));
		// every corner of the lap on open deck
		for (FVector2D& Pp : PathPts) ClampToDeck(Pp.X, Pp.Y, .32);
		PathCum.Reset();
		PathCum.Add(0);
		for (int32 i = 1; i <= PathPts.Num(); i++)
			PathCum.Add(PathCum[i - 1] + FVector2D::Distance(PathPts[i - 1], PathPts[i % PathPts.Num()]));
		PathTotal = FMath::Max(1.0, PathCum.Last());

		static const uint32 Suits[6] = {0xe86a92, 0x2bb3a8, 0xf2c14e, 0x15171a, 0xe4572e, 0xffffff};
		const int32 NP = L > 12 ? 5 : L > 8.5 ? 4 : 3;
		Party.Reset();
		for (int32 k = 0; k < NP; k++)
		{
			FKeysPartyGoer& G = Party.AddDefaulted_GetRef();
			G.H.Init(XWoman(Suits[k % 6], k % 2 == 0), false);
			G.S = double(k) / NP * PathTotal;
			G.Ph = XRand(0, 6);
			G.bWalk = k % 3 == 0;
			G.ModeT = XRand(5, 10);
			G.Move = 0;
			G.MoveT = 0;
			G.bBlend = false;
			// start spread out along the path
			int32 Seg = 0;
			while (Seg < PathPts.Num() - 1 && PathCum[Seg + 1] < G.S) Seg++;
			const FVector2D A = PathPts[Seg], B = PathPts[(Seg + 1) % PathPts.Num()];
			const FVector2D Pt = A + (B - A) * ((G.S - PathCum[Seg]) / FMath::Max(1e-3, PathCum[Seg + 1] - PathCum[Seg]));
			G.H.Pos = FVector(Pt.X, Pt.Y, Deck.SoleAt(Pt.X));
			G.H.Yaw = FMath::Atan2(B.Y - A.Y, B.X - A.X);
		}
	}

	// fight spots: when a fish is on, the guests clear the cockpit for the angler (and the chair) and go forward: a row
	// across the open deck ahead of the console (or its seat) and short of the bow's lounge pads or casting deck, then
	// up on the casting deck itself, then beside the console. Every one inside the gunwales.
	{
		FightSpots.Reset();
		const double ConA = Deck.MidX - Deck.ConL * .5, ConF = Deck.MidX + Deck.ConL * .5 + Deck.ConFwd;
		LaneX0 = FMath::Min(ConA, SeatX0) - .35;
		LaneX1 = ConF + (bMidL ? 1.0 : .6) + .3;
		auto Row = [this](double Xr, double Lift)
		{
			const double Hw = FMath::Max(0.0, Deck.InnerHB(Xr) - .4);
			const int32 N = FMath::Max(1, FMath::FloorToInt32(Hw * 2 / .72) + 1);
			for (int32 i = 0; i < N; i++) FightSpots.Add(FVector(Xr, N == 1 ? 0 : -Hw + 2 * Hw * i / (N - 1), Lift));
		};
		// what is forward of the open deck: the Freeman's lounge pads, a go-fast's sun pad, else the casting deck
		// (KeysMesh::BuildBoat's boxes)
		const double CastX0 = L * .27;
		Raised.Reset();
		if (!bMidL) Raised.Add(FVector(L * .27, L * .39, KeysMesh::SoleAt(S, L * .33) + .36));
		if (S.Id == TEXT("freeman")) Raised.Add(FVector(L * .15, L * .25, KeysMesh::SoleAt(S, L * .2) + .49));
		else if (S.bSunpad && !bMidL) Raised.Add(FVector(L * .145, L * .295, KeysMesh::SoleAt(S, L * .22) + .42));
		const double OpenEnd = S.Id == TEXT("freeman") ? L * .15 : (S.bSunpad && !bMidL) ? L * .145 : CastX0;
		for (double Xr = LaneX1; Xr <= OpenEnd - .25; Xr += .65) Row(Xr, 0);
		if (FightSpots.Num())
		{
			// keep the open-deck rows that ClampToDeck leaves where they are (the console's keep-out can eat one)
			for (int32 i = FightSpots.Num() - 1; i >= 0; i--)
			{
				double Xc = FightSpots[i].X, Yc = FightSpots[i].Y;
				ClampToDeck(Xc, Yc, .3);
				if (FMath::Abs(Xc - FightSpots[i].X) > .05 || FMath::Abs(Yc - FightSpots[i].Y) > .05) FightSpots.RemoveAt(i);
			}
		}
		// up on the bow casting deck (all but the Midnight, whose bow is one long lounge)
		if (!bMidL)
		{
			const double CastX = L * .33, CastTop = KeysMesh::SoleAt(S, CastX) + .36;
			Row(CastX - .15, CastTop - Deck.SoleAt(CastX - .15));
		}
		// beside the console, where there is room between it and the gunwale
		const double Xb = (ConA + ConF) * .5, Hb = Deck.InnerHB(Xb) - .35;
		if (Hb > Deck.ConW * .5 + .45) { FightSpots.Add(FVector(Xb, Hb, 0)); FightSpots.Add(FVector(Xb, -Hb, 0)); }
		if (!FightSpots.Num())
		{
			double Xc = LaneX1, Yc = 0;
			ClampToDeck(Xc, Yc, .32);
			FightSpots.Add(FVector(Xc, Yc, 0));
		}
	}

	// Freeman: a pull-down shower at the back of the hardtop
	bHasShower = S.Id == TEXT("freeman");
	if (bHasShower)
	{
		ShowerHead = FVector(Deck.TopX - Deck.TopL * .5 + .14, Deck.TopW * .42, Deck.TopZ - .22);
		ShowerH.Init(XWoman(0x9fd8ff, false), false);
		ShowerH.Pos = FVector(ShowerHead.X, ShowerHead.Y, Deck.SoleAt(ShowerHead.X));
		ShowerH.bVisible = false;
		bShowerOn = false;
		ShowerForced = -1;
		ShowerT = XRand(8, 18);
		Props.AddTube(FVector(ShowerHead.X, ShowerHead.Y, Deck.TopZ), ShowerHead + FVector(0, 0, .03), .018, .018, 8, XChrome);
		Props.AddTube(ShowerHead + FVector(0, 0, .03), ShowerHead + FVector(0, 0, -.01), .045, .07, 12, XChrome);
		Props.AddTube(ShowerHead + FVector(0, 0, -.01), ShowerHead + FVector(0, 0, -.02), .06, .06, 12, XHex(0x9aa0a6));
	}

	// Midnight Express: a dance pole in the cockpit
	bHasPole = S.Id == TEXT("midnight");
	if (bHasPole)
	{
		// in the open cockpit between the wet bar and the aft bench
		const double X = ((Deck.MidX - 3.75) + (-.39 * L + .4)) * .5;
		PoleBase = FVector(X, 0, Deck.SoleAt(X));
		PoleH.Init(XWoman(0xc8a24a, false), false);
		PoleAng = 0; PoleSpin = 0; PoleClimbT = XRand(5, 9);
		Props.AddTube(PoleBase, PoleBase + FVector(0, 0, 2.45), .025, .025, 10, XChrome);
		Props.AddTube(PoleBase, PoleBase + FVector(0, 0, .04), .14, .12, 14, XChrome);
		Props.AddTube(PoleBase + FVector(0, 0, 2.43), PoleBase + FVector(0, 0, 2.47), .1, .1, 14, XChrome);
	}

	// stern flags: Old Glory on the Freeman, a Jolly Roger on the Robalo
	FlagKind = !CosFlag.IsEmpty() ? 3 : S.Id == TEXT("freeman") ? 1 : S.Id == TEXT("robalo") ? 2 : 0;
	if (FlagKind)
	{
		const double Ht = FlagKind == 1 ? 3.1 : 2.4;
		FlagW = FlagKind == 1 ? 1.5 : FlagKind == 3 ? 1.1 : 1.2;
		FlagH = FlagKind == 1 ? .79 : FlagKind == 3 ? .7 : .8;
		const FVector Base(-.47 * L, Deck.HB * .92 - .12, Deck.Free);
		FlagTop = Base + FVector(-.55, .1, Ht);
		Props.AddTube(Base, FlagTop, .025, .02, 8, XChrome);
		KeysShapes::AddBlob(Props, FlagTop + FVector(0, 0, .04), FVector(.05), FQuat::Identity, XHex(0xd8b84a), 8, 5);
	}

	if (CrewMesh)
	{
		if (Props.V.Num()) Props.Commit(CrewMesh, 1, AKeysWorld::VertexColorMaterial());
		else CrewMesh->ClearMeshSection(1);
		if (!FlagKind) CrewMesh->ClearMeshSection(2);
	}
}

// ---------------------------------------------------------------- per-frame
void AKeysExtras::UpdateCrew(double Dt, double T)
{
	const double Sp = PawnSpeed();
	// in the helm view the camera is where the captain stands
	if (const AKeysBoatPawn* P = PawnRef.Get()) Captain.bVisible = !P->bDriveOn;
	Captain.IdleUpdate(T, Sp);
	Crew.IdleUpdate(T, Sp);
	for (FKeysPartyGoer& G : Party) G.H.IdleUpdate(T, Sp);
	if (bHasShower) ShowerH.IdleUpdate(T, Sp);
	if (bHasPole) PoleH.IdleUpdate(T, Sp);

	// the captain drives and the crew sits; Luigi mode overrides the right hands with a beer. His right hand rests on
	// the throttle and goes with it (KeysDash.cpp)
	FVector Grip;
	if (const AKeysBoatPawn* P = PawnRef.Get(); P && P->ThrottleGrip(Grip) && Captain.Yaw == 0) HelmR = Grip + FVector(-.07, 0, -.07) - Captain.Pos;   // the wrist aft of and under the grip, the hand over it
	Captain.Pose(HelmL, HelmR);
	Crew.DefaultPose();
	PoseLanding(T);

	UpdateShower(Dt, T);
	UpdatePole(Dt, T);
	UpdateParty(Dt, T);
}

// You and the mate landing the fish (AKeysBoatPawn::Stage): at the gunwale leaning out over the side, gaff and hand line
// on its head, hauling it up and in and stepping back as it comes over; or inside the transom heaving it through the
// tuna door in three pulls. Once it's aboard you put both hands up and the mate stands over it, hands on hips.
void AKeysExtras::PoseLanding(double T)
{
	const AKeysBoatPawn* P = PawnRef.Get();
	const bool bOn = P && P->Stage.bOn;
	LandYou.bVisible = LandMate.bVisible = bOn;
	if (!bOn) return;
	if (!bLandInit)
	{
		bLandInit = true;
		FKeysHumanLook Y;
		Y.Shirt = XHex(0x2a8fb0); Y.Bottom = XHex(0x3b4a5a); Y.bCap = true; Y.CapColor = XHex(0xf2efe6); Y.bGlasses = true; Y.Hair = XHex(0x2b1d14);
		Y.Skin = FLinearColor(FColor(0xc6, 0x8e, 0x6a)); Y.bShoes = true; Y.Shoes = XHex(0x30343a);
		LandYou.Init(Y, false);
		FKeysHumanLook M;
		M.Shirt = XHex(0xe9e4d6); M.Bottom = XHex(0x6b5b45); M.bShortsLong = true; M.bCap = true; M.CapColor = XHex(0x8a2b2b); M.Hair = XHex(0x14100c);
		M.Skin = FLinearColor(FColor(0x8d, 0x5a, 0x3c)); M.bShoes = true; M.Shoes = XHex(0xeeeeee);
		LandMate.Init(M, false);
	}
	const AKeysBoatPawn::FLandStage& S = P->Stage;
	auto Ease = [](double V) { V = FMath::Clamp(V, 0.0, 1.0); return V * V * (3 - 2 * V); };
	const double Aboard = Ease((S.Pull - .9) / .1);
	const double Sd = S.Side;
	auto Reach = [](FKeysHuman& H, const FVector& L, const FVector& R) { const FTransform Tr = H.ToBoat(); H.Pose(Tr.InverseTransformPosition(L), Tr.InverseTransformPosition(R)); };
	auto Face = [](const FKeysHuman& H, const FVector& At) { return FMath::Atan2(At.Y - H.Pos.Y, At.X - H.Pos.X); };
	FKeysHuman* Hs[2] = {&LandYou, &LandMate};
	for (int32 k = 0; k < 2; k++)
	{
		FKeysHuman& H = *Hs[k];
		H.DefaultPose();
		H.Roll = 0;
		if (!S.bDoor)
		{
			// at the gunwale, either side of where it comes over; a step inboard as it does
			const double X = S.Final.X + (k == 0 ? .42 : -.5);
			const double Y = Sd * (Deck.InnerHB(X) - .3 - Ease((S.Pull - .45) / .5) * .5);
			H.Pos = FVector(X, Y, Deck.SoleAt(X));
			const double Out = Face(H, FVector(H.Pos.X, Sd * (Deck.HB + 3), 0));   // over the side
			H.Yaw = FMath::Lerp(Out, Face(H, S.Fish), Aboard);
			H.Pitch = -.42 * (1 - Ease((S.Pull - .2) / .5)) + .12 * Ease(S.Pull / .4) * (1 - Aboard);   // lean out, then sit back on it
		}
		else
		{
			// inside the side door, either side of where it slides in, backing up the cockpit with it on each heave
			const double Heave = FMath::Max(0.0, FMath::Sin(FMath::Max(0.0, S.Age - .4) * PI / 1.2)) * (1 - Aboard);
			const double DoorX = S.DoorX;
			// along the rail either side of the door: you forward, the mate aft, out of the fish's way as it swings in
			const double X = DoorX + (k == 0 ? .7 + .5 * S.Pull : -.75 - .35 * S.Pull);
			const double Y = Sd * (Deck.InnerHB(X) - .42 - .15 * Heave);
			H.Pos = FVector(X, Y, Deck.SoleAt(X));
			H.Yaw = FMath::Lerp(Face(H, FVector(DoorX, Sd * (Deck.HB + 2), 0)), Face(H, S.Fish), Aboard);
			H.Pitch = (.24 * Heave - .2 * (1 - Heave)) * (1 - Aboard);
		}
		if (Aboard < 1) H.PoseLegs(XJ(-.17, .085, -.13), XJ(.17, .085, .16));   // braced, one foot forward
		H.HeadNod = .35 * (1 - Aboard);
		H.bHoldHead = true;
		if (Aboard < 1)
		{
			const FVector G = S.Grip, Al = FVector(0, 0, .04);
			if (k == 1) Reach(H, G + FVector(.06, 0, .05) + Al, G + Al);                                    // the gaff, two hands
			else Reach(H, G + FVector(-.04, .05 * Sd, 0), G + FVector(-.1, -.05 * Sd, .02));                 // a fist round the head
		}
		else if (k == 0)
		{
			const double Pump = FMath::Abs(FMath::Sin((S.Age - S.Done) * 5)) * .06;
			H.Pose(XJ(-.24, H.HipY + 1.0 + Pump, .1), XJ(.24, H.HipY + 1.0 + Pump, .1));   // both hands up
		}
		else H.Pose(XJ(-.24, H.HipY + .02, -.02), XJ(.24, H.HipY + .02, -.02));            // hands on hips over it
		H.Pos.Z += .0;
		(void)T;
	}
}

void AKeysExtras::UpdateShower(double Dt, double T)
{
	if (!bHasShower) return;
	const bool bCalm = FMath::Abs(PawnSpeed()) < 1.5;
	ShowerT -= Dt;
	if (ShowerForced < 0 && ShowerT <= 0)
	{
		bShowerOn = !bShowerOn && bCalm;
		ShowerT = bShowerOn ? XRand(10, 16) : XRand(25, 45);
	}
	const AKeysBoatPawn* SP = PawnRef.Get();
	const bool bOn = (ShowerForced >= 0 ? ShowerForced == 1 : bShowerOn) && bCalm && !(SP && (SP->IsInFight() || SP->Stage.bOn));
	FKeysHuman& H = ShowerH;
	H.bVisible = bOn;
	if (!bOn) return;
	// rinsing off: hands up through the hair, head tipped back, a slow sway under the spray
	const double W = FMath::Sin(T * 1.6), W2 = FMath::Sin(T * 2.3 + 1);
	H.Pos = FVector(ShowerHead.X, ShowerHead.Y, Deck.SoleAt(ShowerHead.X));
	H.Yaw = PI + FMath::Sin(T * .4) * .6;
	H.Pitch = 0;
	H.Roll = W * .04;
	H.PoseLegs(XJ(-.12, .085, 0), XJ(.12, .085 + FMath::Max(0.0, W) * .02, 0));
	H.Pose(XJ(-.12 + .04 * W2, H.HipY + .8 + .05 * W, -.02), XJ(.12 - .04 * W2, H.HipY + .8 - .05 * W, -.02));
	H.bHoldHead = true;
	H.HeadNod = -.35 + .05 * W;
}

void AKeysExtras::UpdatePole(double Dt, double T)
{
	if (!bHasPole) return;
	FKeysHuman& H = PoleH;
	// she leaves the pole while a fish is hauled aboard over the aft deck (she'd be standing in the haul)
	const AKeysBoatPawn* LP = PawnRef.Get();
	H.bVisible = !(LP && LP->Stage.bOn);
	const bool bFast = FMath::Abs(PawnSpeed()) > 6;
	const double Beat = FMath::Sin(T * MusicBpm / 60 * PI);
	H.bHoldHead = false;
	if (bFast)
	{
		// hang on to the pole with both hands while the boat is running
		const double R = .32;
		H.Pos = PoleBase + FVector(FMath::Cos(PoleAng) * R, FMath::Sin(PoleAng) * R, 0);
		H.Yaw = PoleAng + PI;
		H.Pitch = H.Roll = 0;
		H.PoseLegs(XJ(-.14, .085, 0), XJ(.14, .085, 0));
		H.Pose(FVector(R, -.04, 1.25), FVector(R, .04, 1.33));
		return;
	}
	// spin slowly around the pole, one hand high on it, the other reaching out, swaying on the beat; now and then a lifted-leg spin
	PoleClimbT -= Dt;
	if (PoleClimbT <= 0 && !(PoleSpin > 0)) { PoleSpin = 2.4; PoleClimbT = XRand(7, 12); }
	const bool bSpin = PoleSpin > 0;
	if (bSpin) PoleSpin -= Dt;
	const double Sp = bSpin ? FMath::Sin(PI * (1 - PoleSpin / 2.4)) : 0;
	PoleAng += Dt * (.55 + 1.8 * Sp);
	const double R = .33 - .05 * Sp;
	H.Pos = PoleBase + FVector(FMath::Cos(PoleAng) * R, FMath::Sin(PoleAng) * R, .12 * Sp);
	H.Yaw = PoleAng - PI * .5;   // side-on, pole at the left hand
	H.Pitch = 0;
	H.Roll = .12 + .2 * Sp;      // leaning away from the pole
	const double Lift = .35 * Sp, Sway = Beat * .03 * (1 - Sp);
	H.PoseLegs(XJ(-.12, .085 + Lift * .4, .02 + Lift * .3), XJ(.12, .085 + Lift, -.02 - Lift * .6));
	const FVector OnPole = H.ToBoat().InverseTransformPosition(PoleBase + FVector(0, 0, 1.6 + .15 * Sp));
	H.Pose(OnPole, XJ(.42 + Sway, H.HipY + .55 + .1 * Beat, .1));
}

void AKeysExtras::UpdateParty(double Dt, double T)
{
	if (!Party.Num() || PathPts.Num() < 2) return;
	const bool bFast = FMath::Abs(PawnSpeed()) > 6;
	auto PathAt = [this](double S, FVector2D& OutPos, FVector2D& OutDir)
	{
		S = FMath::Fmod(FMath::Fmod(S, PathTotal) + PathTotal, PathTotal);
		int32 i = 0;
		while (i < PathPts.Num() - 1 && PathCum[i + 1] < S) i++;
		const FVector2D A = PathPts[i], B = PathPts[(i + 1) % PathPts.Num()];
		const double Seg = FMath::Max(1e-3, PathCum[i + 1] - PathCum[i]), U = (S - PathCum[i]) / Seg;
		OutPos = A + (B - A) * U;
		OutDir = (B - A).GetSafeNormal();
	};

	// the middle of the group, so dancers face into it
	FVector2D Ctr(0, 0);
	for (const FKeysPartyGoer& G : Party) Ctr += FVector2D(G.H.Pos.X, G.H.Pos.Y);
	Ctr *= 1.0 / Party.Num();

	static const int32 Moves[12] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 6, 4, 8};   // sway hands step roll groove twirl hipcircle shimmy wave
	const bool bModels = PartyModelsReady();
	const AKeysBoatPawn* FP = PawnRef.Get();
	const bool bFight = FP && (FP->IsInFight() || FP->Stage.bOn);   // and while the fish comes aboard: clear of the haul
	int32 Idx = 0;
	for (FKeysPartyGoer& G : Party)
	{
		FKeysHuman& H = G.H;
		const int32 Me = Idx++;
		G.ModeT -= Dt;
		if (G.ModeT <= 0) { G.bWalk = !G.bWalk; G.ModeT = XRand(7, 14); }
		if (bModels) G.bWalk = false;   // the web game's crew keep their spots on deck and dance there
		// a fish on: nobody laps the cockpit; she walks forward to her fight spot and dances there, and back after
		const bool bAway = bFight || (G.bSpot && (FMath::Abs(G.SpotX - G.HomeX) > .05 || FMath::Abs(G.SpotY - G.HomeY) > .05));
		const int32 Mode = bFast ? 2 : (G.bWalk && !bAway) ? 0 : 1;
		double X = H.Pos.X, Y = H.Pos.Y, Bounce = 0;
		H.bHoldHead = false;
		bool bTransit = false;
		FVector2D MoveDir(0, 0);
		// dancing or hanging on, she stays on one spot: the moves sway her about it, they don't walk her off the boat
		if (Mode != 0)
		{
			if (!G.bHome) { G.HomeX = X; G.HomeY = Y; ClampToDeck(G.HomeX, G.HomeY, .32); G.bHome = true; }
			if (!G.bSpot) { G.SpotX = G.HomeX; G.SpotY = G.HomeY; G.bSpot = true; }
			const FVector Fs = bFight && FightSpots.Num() ? FightSpots[Me % FightSpots.Num()] + FVector(0, Me >= FightSpots.Num() ? .25 : 0, 0) : FVector(G.HomeX, G.HomeY, 0);
			const FVector2D Want(Fs.X, Fs.Y);
			const FVector2D Was(G.SpotX, G.SpotY);
			bTransit = StepSpot(G, Want, Dt);
			// a step up onto the casting deck as she reaches it (and down as she leaves)
			G.SpotLift = FMath::FInterpConstantTo(G.SpotLift, RaisedAt(G.SpotX), Dt, 2.0);
			MoveDir = FVector2D(G.SpotX, G.SpotY) - Was;
			X = G.SpotX; Y = G.SpotY;
		}
		else { G.bHome = false; G.bSpot = false; }
		if (Mode == 0)
		{
			G.S += Dt * .85;
			FVector2D P, D;
			PathAt(G.S, P, D);
			X = P.X; Y = P.Y;
			G.Ph += Dt * 6.5;
			const double F1 = FMath::Sin(G.Ph);
			auto LiftF = [](double K) { return FMath::Max(0.0, K) * .07; };
			H.PoseLegs(XJ(-.105, .085 + LiftF(FMath::Cos(G.Ph)), .17 * F1), XJ(.105, .085 + LiftF(-FMath::Cos(G.Ph)), -.17 * F1));
			H.Pose(XJ(-.22, H.HipY + .05, -.13 * F1 + .04), XJ(.22, H.HipY + .05, .13 * F1 + .04));
			Bounce = FMath::Abs(FMath::Cos(G.Ph)) * .022;
			const double Face = FMath::Atan2(D.Y, D.X);
			H.Yaw = Face + F1 * .06;
			H.Pitch = -.02;
			H.Roll = FMath::Sin(G.Ph) * .045;
			X += -FMath::Sin(Face) * FMath::Sin(G.Ph) * .02;
			Y += FMath::Cos(Face) * FMath::Sin(G.Ph) * .02;
		}
		else if (Mode == 1)
		{
			// dance on the beat: several moves, facing into the group, head nodding with the music
			const double Bpm = MusicBpm;
			G.Ph += Dt * Bpm / 60 * PI;
			G.MoveT -= Dt;
			if (G.MoveT <= 0)
			{
				G.Move = Moves[FMath::RandRange(0, 11)];
				G.MoveT = G.Move == 5 ? 60 / Bpm * 4 : XRand(4, 8);
				G.Move0 = G.Ph;
			}
			const double Beat = FMath::Sin(G.Ph), Half = FMath::Sin(G.Ph * .5), Down = FMath::Abs(Beat);
			const double Face = MoveDir.SizeSquared() > 1e-8 ? FMath::Atan2(MoveDir.Y, MoveDir.X) : FMath::Atan2(Ctr.Y - Y, Ctr.X - X) + FMath::Sin(T * .3 + G.S) * .5;
			double Ry = XAngLerp(H.Yaw, Face, Dt * (MoveDir.SizeSquared() > 1e-8 ? 5.0 : 1.2));
			double Sway = 0, Lean = 0, Nod = Down * .12, Pitch = -.04;
			const double HY = H.HipY;
			FVector AL, AR, FL = XJ(-.13, .085, .02), FR = XJ(.13, .085, -.02);
			switch (G.Move)
			{
			case 0:   // sway
				Sway = Half * .07; Lean = -Half * .1; Bounce = -.035 - Down * .03;
				FL.Z += FMath::Max(0.0, Half) * .03; FR.Z += FMath::Max(0.0, -Half) * .03;
				AL = XJ(-.2, HY + .06, .02); AR = XJ(.32 + .05 * Half, HY + .85 + .12 * Beat, .12);
				break;
			case 1:   // hands up
				Sway = Half * .04; Lean = -Half * .06; Bounce = -Down * .06;
				AL = XJ(-.22 + .08 * Half, HY + 1.0 + .06 * Beat, .08); AR = XJ(.22 + .08 * Half, HY + 1.0 - .06 * Beat, .08);
				break;
			case 2:   // step
			{
				const double St = Half * .11;
				Sway = St * .6; Bounce = -.03 - Down * .025;
				FL.Y -= FMath::Max(0.0, -St); FR.Y += FMath::Max(0.0, St);
				AL = XJ(-.24, HY + .45 + .08 * Down, .2); AR = XJ(.24, HY + .45 + .08 * Down, .2);
				break;
			}
			case 3:   // body roll: chest leads, hips follow
			{
				const double W = FMath::Sin(G.Ph * .5);
				Bounce = -.04 - .04 * FMath::Max(0.0, W); Pitch = -.12 * W;
				AL = XJ(-.18, HY + .55 + .15 * W, .25); AR = XJ(.18, HY + .55 - .15 * W, .25); Lean = .03 * W;
				break;
			}
			case 4:   // groove
			{
				const double St = FMath::Sin(G.Ph) * .06;
				Sway = St; Bounce = -.02 - Down * .03;
				FL.X += FMath::Max(0.0, St) * 1.5; FR.X -= FMath::Max(0.0, -St) * 1.5;
				AL = XJ(-.28, HY + .5 + .06 * Beat, .15 + .05 * Half); AR = XJ(.28, HY + .5 - .06 * Beat, .15 - .05 * Half); Lean = -St * .8;
				break;
			}
			case 6:   // slow hip circles
			{
				const double A = G.Ph * .5;
				Sway = FMath::Cos(A) * .06; Lean = -FMath::Cos(A) * .09; Bounce = -.04 - .03 * FMath::Max(0.0, FMath::Sin(A)); Pitch = -.06 * FMath::Sin(A);
				AL = XJ(-.2, HY + .12, .06); AR = XJ(.2, HY + .12, .06);
				if (FMath::Sin(G.Ph * .25) > 0) AR = XJ(.12, HY + 1.02, .04);
				Nod = Down * .06;
				break;
			}
			case 7:   // quick shoulder shimmy
			{
				const double F = FMath::Sin(G.Ph * 4) * .035;
				Sway = F * .4; Bounce = -.05 - Down * .02; Lean = F * .6;
				AL = XJ(-.3, HY + .48 + F, .18); AR = XJ(.3, HY + .48 - F, .18); Nod = .02;
				break;
			}
			case 8:   // both arms waving overhead
			{
				const double A = G.Ph * .5;
				Sway = FMath::Sin(A) * .05; Lean = -FMath::Sin(A) * .08; Bounce = -Down * .045;
				AL = XJ(-.18 + .22 * FMath::Sin(A), HY + 1.02, .06); AR = XJ(.18 + .22 * FMath::Sin(A), HY + 1.02, .06);
				break;
			}
			default:  // twirl: one full spin with an arm up
				Ry += Dt * PI * 2 * (Bpm / 60) / 4 * 2.0; Bounce = -.02;
				AL = XJ(-.25, HY + .3, .05); AR = XJ(.05, HY + 1.12, .02); FL.Z += .02; Nod = 0;
				break;
			}
			// blend into each new move instead of snapping
			const double Bl = FMath::Min(1.0, Dt * 7);
			if (!G.bBlend) { G.AL = AL; G.AR = AR; G.FL = FL; G.FR = FR; G.bBlend = true; }
			G.AL = FMath::Lerp(G.AL, AL, Bl); G.AR = FMath::Lerp(G.AR, AR, Bl);
			G.FL = FMath::Lerp(G.FL, FL, Bl); G.FR = FMath::Lerp(G.FR, FR, Bl);
			G.Sw = FMath::Lerp(G.Sw, Sway, Bl); G.Ln = FMath::Lerp(G.Ln, Lean, Bl);
			// keep the feet planted while the knees bend
			H.PoseLegs(G.FL - FVector(0, 0, Bounce), G.FR - FVector(0, 0, Bounce));
			H.Pose(G.AL, G.AR);
			H.Yaw = Ry;
			H.Pitch = Pitch;
			H.Roll = G.Ln;
			X += -FMath::Sin(Ry) * G.Sw;
			Y += FMath::Cos(Ry) * G.Sw;
			H.bHoldHead = true;
			H.HeadNod = Nod - .04;
		}
		else
		{
			// running: hang on to the T-top, and raise a beer in Luigi mode
			H.Roll = 0;
			H.Pitch = 0;
			H.PoseLegs(XJ(-.14, .085, 0), XJ(.14, .085, 0));
			const FVector R = bLuigiOn
				? FMath::Lerp(XJ(.3, H.HipY + 1.08 + .06 * FMath::Sin(T * 2.4 + G.S), .04), H.Mouth(), H.SipAmt)
				: XJ(.22, H.HipY + 1.0, .14);
			H.Pose(XJ(-.2, H.HipY + 1.0, .14), R);
		}
		// up on the casting deck she is forward of the open deck's limit: keep her inside the gunwales only
		if (G.SpotLift > .01) { const double Hw = FMath::Max(0.0, Deck.InnerHB(X) - .3); Y = FMath::Clamp(Y, -Hw, Hw); }
		else ClampToDeck(X, Y, .26, !bTransit);
		H.Pos = FVector(X, Y, Deck.SoleAt(X) + Bounce + G.SpotLift);
		H.bVisible = !bModels;
	}
	UpdatePartyModels();
}

double AKeysExtras::RaisedAt(double X) const
{
	double Top = 0;
	for (const FVector& R : Raised)
		if (X > R.X - .12 && X < R.Y + .12) Top = FMath::Max(Top, R.Z - Deck.SoleAt(X));
	return Top;
}

bool AKeysExtras::StepSpot(FKeysPartyGoer& G, const FVector2D& Want, double Dt) const
{
	FVector2D P(G.SpotX, G.SpotY);
	if (FVector2D::Distance(P, Want) < .02) { G.SpotX = Want.X; G.SpotY = Want.Y; return false; }
	// a stroll, not a teleport: about 0.8 m/s. Past the console and the helm seats she goes out to the gunwale on her
	// side, along it, and back in, so she never walks through the furniture (and never off the deck: ClampToDeck)
	const double Step = .8 * Dt;
	const bool bCross = FMath::Min(P.X, Want.X) < LaneX1 && FMath::Max(P.X, Want.X) > LaneX0
		&& !(P.X >= LaneX1 && Want.X >= LaneX1) && !(P.X <= LaneX0 && Want.X <= LaneX0);
	FVector2D Goal = Want;
	bool bTransit = false;
	if (bCross)
	{
		const double Sd = FMath::Abs(Want.Y) > .2 ? FMath::Sign(Want.Y) : P.Y >= 0 ? 1 : -1;
		const double LaneY = Sd * FMath::Max(.35, Deck.InnerHB(P.X) - .3);
		const bool bInBand = P.X > LaneX0 - .05 && P.X < LaneX1 + .05;
		if (FMath::Abs(P.Y - LaneY) > .1 && !(bInBand && FMath::Abs(P.Y) > FMath::Abs(LaneY) - .1)) Goal = FVector2D(P.X, LaneY);
		else Goal = FVector2D(Want.X > P.X ? LaneX1 + .1 : LaneX0 - .1, LaneY);
		bTransit = true;
	}
	const FVector2D D = Goal - P;
	const double L = D.Size();
	P += L > Step ? D / L * Step : D;
	double X = P.X, Y = P.Y;
	if (Want.X > Deck.L * .26) { const double Hw = FMath::Max(0.0, Deck.InnerHB(X) - .3); Y = FMath::Clamp(Y, -Hw, Hw); }   // out to the casting deck
	else ClampToDeck(X, Y, .26, !bTransit);
	G.SpotX = X; G.SpotY = Y;
	return bTransit;
}

void AKeysExtras::ClampToDeck(double& X, double& Y, double Margin, bool bKeepOut) const
{
	if (!Deck.Spec) return;
	const KeysData::FBoatSpec& S = *Deck.Spec;
	// aft of the bow's casting deck or lounge, forward of the stern bench, inside the liner
	const double X0 = KeysMesh::SternX(S) + .14 + S.Beam * .02 + .55 + Margin * .5, X1 = FMath::Max(X0 + .5, Deck.L * .26);
	for (int32 It = 0; It < 4; It++)
	{
		X = FMath::Clamp(X, X0, X1);
		const double Hw = FMath::Max(0.0, Deck.InnerHB(X) - Margin);
		Y = FMath::Clamp(Y, -Hw, Hw);
		bool bMoved = false;
		if (bKeepOut) for (const FBox2D& B : KeepOut)
		{
			const double Mg = Margin * .6;
			const double Ax = B.Min.X - Mg, Bx = B.Max.X + Mg, Ay = B.Min.Y - Mg, By = B.Max.Y + Mg;
			if (X <= Ax || X >= Bx || Y <= Ay || Y >= By) continue;
			// out the nearest way that leaves deck under her: beside it if there's room, else fore or aft
			const double YOut = Y >= 0 ? By + .01 : Ay - .01;
			const double DX = FMath::Min(X - Ax, Bx - X), DY = FMath::Abs(YOut - Y);
			const bool bRoomBeside = FMath::Abs(YOut) <= FMath::Max(0.0, Deck.InnerHB(X) - Margin);
			if (bRoomBeside && DY <= DX + .4) Y = YOut;
			else X = (X - Ax < Bx - X && Ax - .01 > X0) || Bx + .01 > X1 ? Ax - .01 : Bx + .01;
			bMoved = true;
		}
		if (!bMoved) break;
	}
}

// ---------------------------------------------------------------- the deck crew models (web entities/crew-model)
bool AKeysExtras::PartyModelsReady()
{
	return CrewMesh && KeysDancers::KindFor(0) > 0;
}

void AKeysExtras::UpdatePartyModels()
{
	if (!PartyModelsReady())
	{
		for (FKeysDancerFig& F : PartyFigs) F.SetVisible(false);
		return;
	}
	// one figure per guest, a mix of the web game's dancers, sized like the web placements (1.6 - 1.72 m)
	while (PartyFigs.Num() < Party.Num())
	{
		FKeysDancerFig& F = PartyFigs.AddDefaulted_GetRef();
		F.Spawn(this, CrewMesh, KeysDancers::KindFor(PartyFigs.Num() - 1));
	}
	FVector Eye;
	const AKeysBoatPawn* EP = PawnRef.Get();
	const bool bEye = EP && EP->FpEyeLocal(Eye);
	static const double Heights[5] = {1.67, 1.63, 1.7, 1.65, 1.68};
	const double Dt = GetWorld()->GetDeltaSeconds();
	for (int32 i = 0; i < PartyFigs.Num(); i++)
	{
		FKeysDancerFig& F = PartyFigs[i];
		const bool bOn = Party.IsValidIndex(i) && (!bEye || FVector2D(Party[i].H.Pos.X - Eye.X, Party[i].H.Pos.Y - Eye.Y).Size() > 1.1);
		// a guest still strolling forward when a fish comes aboard would stand in the haul: she's out of shot until it's done
		bool bInHaul = false;
		if (bOn && EP && EP->Stage.bOn)
		{
			const FVector2D Me(Party[i].H.Pos.X, Party[i].H.Pos.Y);
			bInHaul = FVector2D::Distance(Me, FVector2D(EP->Stage.Fish.X, EP->Stage.Fish.Y)) < FMath::Max(1.3, EP->Stage.Lv * .55)
				|| FVector2D::Distance(Me, FVector2D(EP->Stage.Final.X, EP->Stage.Final.Y)) < 1.7   // where it will land
				|| FVector2D::Distance(Me, FVector2D(LandYou.Pos.X, LandYou.Pos.Y)) < 1.1
				|| FVector2D::Distance(Me, FVector2D(LandMate.Pos.X, LandMate.Pos.Y)) < 1.1;
		}
		F.SetVisible(bOn && !bInHaul);
		if (!bOn || bInHaul) continue;
		const FKeysHuman& H = Party[i].H;
		F.Place(FVector(H.Pos.X, H.Pos.Y, Deck.SoleAt(H.Pos.X) + Party[i].SpotLift), H.Yaw, Heights[i % 5], Dt);
	}
}

// ---------------------------------------------------------------- flags (JS usFlagTex / jollyRogerTex / makeFlag)
namespace
{
	FLinearColor XUsFlag(double U, double V)
	{
		const double UW = .4, UH = 7.0 / 13;
		if (U < UW && V < UH)
		{
			const double UU = U / UW, VV = V / UH;
			const int32 Row = FMath::Clamp(FMath::RoundToInt32(VV * 10 - 1), 0, 8);
			if (FMath::Abs(VV - (Row + 1) / 10.0) < .055)
			{
				const int32 Odd = Row % 2, N = Odd ? 5 : 6;
				const int32 I = FMath::Clamp(FMath::RoundToInt32((UU * 12 - (Odd ? 2 : 1)) / 2), 0, N - 1);
				const double CX = (Odd ? 2 + 2 * I : 1 + 2 * I) / 12.0;
				if (FMath::Abs(UU - CX) < .045) return FLinearColor::White;
			}
			return XHex(0x3c3b6e);
		}
		return FMath::FloorToInt(V * 13) % 2 ? FLinearColor::White : XHex(0xb22234);
	}

	// content/cosmetics.ts FlagLook: stripes, an optional disc, an optional big letter
	FLinearColor XCosFlag(const FString& Key, double U, double V)
	{
		auto C = [](uint32 X) { return FLinearColor(FColor(uint8(X >> 16), uint8(X >> 8), uint8(X))); };
		if (Key == TEXT("cosmetic.flag.conch"))
		{
			const double DX = (U - .5) * 1.6, DY = V - .5;
			return DX * DX + DY * DY < .09 ? C(0xf6c945) : C(0x1b4f9c);
		}
		if (Key == TEXT("cosmetic.flag.fishon")) return V < 1.0 / 3 || V > 2.0 / 3 ? C(0xd7263d) : C(0xf6f1e7);
		// the white flag, with an L on it
		const bool bL = (U > .38 && U < .48 && V > .22 && V < .78) || (U > .38 && U < .64 && V > .68 && V < .78);
		return bL ? C(0x15171a) : C(0xf6f1e7);
	}

	FLinearColor XJollyRoger(double U, double V)
	{
		const double PX = U * 420, PY = V * 280, CX = 210;
		const FLinearColor Black = XHex(0x0d0d0f), Bone = XHex(0xf2efe6);
		// skull features punched out in black
		for (int32 E = -1; E <= 1; E += 2)
			if (FMath::Square((PX - (CX + E * 24)) / 16) + FMath::Square((PY - 109.2) / 19) < 1) return Black;
		if (PY > 123.2 && PY < 140 && FMath::Abs(PX - CX) < 8 * (PY - 123.2) / 16.8) return Black;
		for (int32 I = -2; I <= 2; I++)
			if (PY > 164 && PY < 186 && FMath::Abs(PX - (CX + I * 14)) < 2) return Black;
		if (FMath::Square(PX - CX) + FMath::Square(PY - 112) < 62 * 62) return Bone;
		if (PX > CX - 36 && PX < CX + 36 && PY > 142 && PY < 186) return Bone;
		// crossed bones with knobbed ends
		const double DX = PX - CX, DY = PY - 173.6;
		for (const double A : {-.62, .62})
		{
			const double RX = DX * FMath::Cos(A) + DY * FMath::Sin(A), RY = -DX * FMath::Sin(A) + DY * FMath::Cos(A);
			if (FMath::Abs(RX) < 140 && FMath::Abs(RY) < 11) return Bone;
			for (int32 E = -1; E <= 1; E += 2)
				for (int32 S = -1; S <= 1; S += 2)
					if (FMath::Square(RX - E * 140) + FMath::Square(RY - S * 10) < 13 * 13) return Bone;
		}
		return Black;
	}
}

void AKeysExtras::BuildFlag(FKeysMesh& M, double T, double Wind) const
{
	if (!FlagKind) return;
	const int32 NU = FlagKind == 1 ? 48 : 42, NV = FlagKind == 1 ? 26 : 28;
	const double W = FlagW, Hh = FlagH;
	const double K = FMath::Clamp(Wind / 20, .15, 1.2), Ph = T * (5 + 6 * K);
	const FVector Anchor = FlagTop - FVector(0, 0, Hh * .5 + .03);
	TArray<FVector> P;
	P.SetNumUninitialized((NU + 1) * (NV + 1));
	for (int32 j = 0; j <= NV; j++)
		for (int32 i = 0; i <= NU; i++)
		{
			const double U = double(i) / NU, X0 = U * W, Y0 = Hh * .5 - double(j) / NV * Hh;
			const double Wave = FMath::Sin(U * 7 - Ph + Y0 * 1.5) * U * (.06 + .1 * K) * W + FMath::Sin(U * 13 - Ph * 1.7) * U * .02 * W;
			const double Y = Y0 - U * U * Hh * .55 * (1 - FMath::Min(1.0, K * 1.4));
			const double X = X0 * (1 - .04 * FMath::Abs(FMath::Sin(U * 7 - Ph)));
			// the flag streams aft (-X) from the staff
			P[j * (NU + 1) + i] = Anchor + FVector(-X, Wave, Y);
		}
	for (int32 j = 0; j < NV; j++)
		for (int32 i = 0; i < NU; i++)
		{
			const double U = (i + .5) / NU, V = (j + .5) / NV;
			const FLinearColor C = FlagKind == 1 ? XUsFlag(U, V) : FlagKind == 3 ? XCosFlag(CosFlag, U, V) : XJollyRoger(U, V);
			const int32 A = j * (NU + 1) + i;
			M.AddQuad(P[A], P[A + 1], P[A + NU + 2], P[A + NU + 1], C, FVector(0, 1, 0));
		}
}

void AKeysExtras::CommitCrew(double T)
{
	if (!CrewMesh) return;
	FKeysMesh M;
	Captain.Build(M);
	Crew.Build(M);
	if (LandYou.bVisible)
	{
		LandYou.Build(M);
		LandMate.Build(M);
		// the side door for a big fish: an opening in the gunwale and its panel swung open inboard against the hinge
		if (const AKeysBoatPawn* DP = PawnRef.Get(); DP && DP->Stage.bOn && DP->Stage.bDoor)
		{
			const double Sd = DP->Stage.Side, DX = DP->Stage.DoorX, W = .72;
			const double Z0 = Deck.SoleAt(DX), Z1 = Deck.Free, H = Z1 - Z0;
			const double Y = Sd * Deck.InnerHB(DX);
			const double Open = FMath::Clamp(DP->Stage.Age / .5, 0.0, 1.0);
			// the dark gap through the hull side, a hair proud of the inside and outside faces
			M.AddBox(FVector(DX, Sd * (Deck.HB + .005), Z0 + H * .5), FVector(W, .02, H), XHex(0x15181c));
			M.AddBox(FVector(DX, Y + Sd * .02, Z0 + H * .5), FVector(W, .02, H), XHex(0x15181c));
			// the panel, hinged at its forward edge, swung inboard
			const double A = -Sd * FMath::DegreesToRadians(100.0 * (1 - FMath::Pow(1 - Open, 3)));
			const FVector Hinge(DX + W * .5, Y, Z0 + H * .5);
			const FVector Mid = Hinge + FVector(-FMath::Cos(A), -FMath::Sin(A), 0) * W * .5;
			const FQuat Rq(FVector::UpVector, A);
			M.AddBox(Mid, FVector(W, .045, H - .04), XHex(0xf3f1ea), Rq);
			M.AddBox(Mid + Rq.RotateVector(FVector(0, .03, H * .15)), FVector(.12, .02, .04), XHex(0xb9bec4), Rq);   // the latch
		}
		// the mate's gaff, and your hand line, from the hands to the fish's head while it's still coming aboard
		if (const AKeysBoatPawn* LP = PawnRef.Get(); LP && LP->Stage.bOn && LP->Stage.Pull < .98)
		{
			const FVector G = LP->Stage.Grip;
			const FVector MH = LandMate.ToBoat().TransformPosition(LandMate.HandR), YH = LandYou.ToBoat().TransformPosition(LandYou.HandL);
			M.AddTube(MH, G, .026, .02, 7, XHex(0x3a3f45));   // the gaff's dark shaft
			M.AddTube(G, G + (G - MH).GetSafeNormal() * .11 + FVector(0, 0, -.08), .014, .008, 5, XHex(0xc9ced3));   // the hook
			if (FVector::Dist(YH, G) > .12) M.AddTube(YH, G, .006, .006, 4, XHex(0x2a6f97));
		}
	}
	// the fighting chair, turned toward the fish, and the angler strapped into it while a trolled fish is on
	if (const AKeysBoatPawn* CP = PawnRef.Get(); CP && Deck.Spec && KeysMesh::HasFightChair(*Deck.Spec))
	{
		const FVector Base = KeysMesh::FightChairBase(*Deck.Spec);
		const double Yaw = CP->GetChairYaw();
		KeysMesh::BuildFightChair(M, Base, Yaw);
		if (CP->InChair())
		{
			if (!bChairInit)
			{
				FKeysHumanLook L;
				L.Skin = XHex(0xd9a27f); L.Shirt = XHex(0x9cc8e0); L.Bottom = XHex(0xb9a77c); L.bShortsLong = true; L.bCap = true; L.CapColor = XHex(0x1d3557);
				L.bGlasses = true; L.bShoes = true; L.Shoes = XHex(0xf2f2f2); L.Hair = XHex(0x4a3020);
				ChairH.Init(L, true);
				bChairInit = true;
			}
			const FQuat Q(FVector::UpVector, Yaw);
			ChairH.Pos = Base + Q.RotateVector(FVector(-.06, 0, -.2));
			ChairH.Yaw = Yaw;
			ChairH.Pitch = -.12 - .1 * FMath::Sin(T * 1.7);   // leaning back on the fish, pumping
			ChairH.Roll = 0;
			ChairH.HeadNod = -.1;
			ChairH.bHoldHead = true;
			// hands on the fore grip and the crank, in the angler's own frame
			const FVector G = KeysMesh::ChairGimbal(Base, Yaw);
			const FVector Dl = Q.RotateVector(FVector(FMath::Cos(.75), 0, FMath::Sin(.75)));
			const FTransform HX = ChairH.ToBoat();
			const FVector Grip = HX.InverseTransformPosition((G + Dl * .8) * 100.0) / 100.0, Crank = HX.InverseTransformPosition((G + Dl * .42 + Q.RotateVector(FVector(0, .12, .1))) * 100.0) / 100.0;
			ChairH.Pose(Grip + FVector(0, -.04, 0), Crank + FVector(0, 0, .03 * FMath::Sin(T * 6)));
			ChairH.Build(M);
			// just in front of his face, where his eyes are
			ChairEyeL = ChairH.ToBoat().TransformPosition(XJ(0, ChairH.HipY + .8, .2));
			bChairEyeOn = true;
		}
		else bChairEyeOn = false;
	}
	// in the first-person fishing view, a guest standing where you stand would fill the lens: they step aside
	FVector Eye;
	const AKeysBoatPawn* EP = PawnRef.Get();
	const bool bEye = EP && EP->FpEyeLocal(Eye);
	for (const FKeysPartyGoer& G : Party)
		if (!bEye || FVector2D(G.H.Pos.X - Eye.X, G.H.Pos.Y - Eye.Y).Size() > 1.1) G.H.Build(M);
	if (bHasPole) PoleH.Build(M);
	if (bHasShower && ShowerH.bVisible)
	{
		ShowerH.Build(M);
		// the water: a few thin streams from the head down to the deck
		const double Floor = Deck.SoleAt(ShowerHead.X);
		for (int32 k = 0; k < 7; k++)
		{
			const double A = FMath::FRand() * 2 * PI, R0 = FMath::FRand() * .045, R1 = .08 + FMath::FRand() * .14;
			const FVector Top = ShowerHead + FVector(FMath::Cos(A) * R0, FMath::Sin(A) * R0, -.03);
			const FVector Bot(ShowerHead.X + FMath::Cos(A) * R1, ShowerHead.Y + FMath::Sin(A) * R1, Floor + .02);
			M.AddTube(Top, Bot, .006, .009, 4, FLinearColor(.78f, .9f, .97f));
		}
	}
	M.Commit(CrewMesh, 0, AKeysWorld::VertexColorMaterial());

	if (FlagKind)
	{
		FKeysMesh F;
		BuildFlag(F, T, FMath::Abs(PawnSpeed()) + 4);
		F.Commit(CrewMesh, 2, AKeysWorld::VertexColorMaterial());
	}
}
