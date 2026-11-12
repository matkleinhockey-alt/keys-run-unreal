// The people, party, shower, dance pole and flags aboard the player's boat (JS makeHuman / updateParty / updatePole /
// updateShower / makeFlag). Part of AKeysExtras; the actor itself is in KeysExtras.cpp.
#include "KeysExtras.h"
#include "KeysBoatPawn.h"
#include "KeysData.h"
#include "KeysMesh.h"
#include "KeysWorld.h"
#include "ProceduralMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Animation/AnimSequence.h"
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
	DeckZ = KeysMesh::SoleAt(S, .05 * L - .5);
	Spec = &S;
	MidX = .05 * L;
	TopZ = DeckZ + 2.35;
	TopL = FMath::Min(2.6, L * .26);
	TopW = FMath::Min(2.2, S.Beam * .66);
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
		const double X = Deck.MidX - .66;
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
		const double X = Deck.MidX + .525 + .25;
		Crew.Pos = FVector(X, 0, Deck.SoleAt(X));
		Crew.Yaw = 0;
		Props.AddBox(FVector(X - .02, 0, Deck.SoleAt(X) + .21), FVector(.42, .62, .42), XHex(0xeeeae0));
	}

	// party path: a lap around the console (JS makePath)
	{
		auto Lane = [this](double X) { return FMath::Max(.35, Deck.InnerHB(X) - .32); };
		const double Zc = FMath::Min(L * .33, L * .39 - .6);
		const double XF = FMath::Min(L * .27, Deck.MidX + 1.6);
		// past the helm seats (or the leaning post) the lane swings out round them, so nobody walks through the chairs:
		// the seat row's half-width plus a shoulder, as far as the gunwale allows
		const int32 NSeat = S.bRodRack ? 4 : S.Beam > 3.6 ? 3 : 2;
		const double SeatHalf = S.bLeaning ? .58 : (NSeat - 1) * .32 + .3;
		const double SX0 = Deck.MidX - 1.75, SX1 = Deck.MidX - .8;
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

	// Freeman: a pull-down shower at the back of the hardtop
	bHasShower = S.Id == TEXT("freeman");
	if (bHasShower)
	{
		ShowerHead = FVector(Deck.MidX - .5 - Deck.TopL * .5 + .14, Deck.TopW * .42, Deck.TopZ - .22);
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
		const double X = ((Deck.MidX - 1.55 - .45) + (-.39 * L + .4)) * .5;
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

	// the captain drives and the crew sits; Luigi mode overrides the right hands with a beer
	Captain.Pose(HelmL, HelmR);
	Crew.DefaultPose();

	UpdateShower(Dt, T);
	UpdatePole(Dt, T);
	UpdateParty(Dt, T);
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
	const bool bOn = (ShowerForced >= 0 ? ShowerForced == 1 : bShowerOn) && bCalm;
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
	for (FKeysPartyGoer& G : Party)
	{
		FKeysHuman& H = G.H;
		G.ModeT -= Dt;
		if (G.ModeT <= 0) { G.bWalk = !G.bWalk; G.ModeT = XRand(7, 14); }
		if (bModels) G.bWalk = false;   // the web game's crew keep their spots on deck and dance there
		const int32 Mode = bFast ? 2 : G.bWalk ? 0 : 1;
		double X = H.Pos.X, Y = H.Pos.Y, Bounce = 0;
		H.bHoldHead = false;
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
			const double Face = FMath::Atan2(Ctr.Y - Y, Ctr.X - X) + FMath::Sin(T * .3 + G.S) * .5;
			double Ry = XAngLerp(H.Yaw, Face, Dt * 1.2);
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
		H.Pos = FVector(X, Y, Deck.SoleAt(X) + Bounce);
		H.bVisible = !bModels;
	}
	UpdatePartyModels();
}

// ---------------------------------------------------------------- the deck crew models (web entities/crew-model)
namespace
{
	// dance-01: the web game's bikini_girl bound to the Mixamo rig with its "Hip Hop Dancing" clip, imported from
	// apps/client/public/models/crew/dance-01.glb (Content/KeysRun/Crew). 1.83 m tall at rest, feet at 0.
	constexpr double CrewTemplateH = 1.8298;
	double CrewYawOffset()
	{
		static double Off = 1e9;
		if (Off > 1e8) { Off = 90; FParse::Value(FCommandLine::Get(), TEXT("KRCrewYaw="), Off); }
		return Off;
	}
	USkeletalMesh* CrewMeshAsset()
	{
		static TWeakObjectPtr<USkeletalMesh> M;
		static bool bTried = false;
		if (!bTried) { bTried = true; M = LoadObject<USkeletalMesh>(nullptr, TEXT("/Game/KeysRun/Crew/Dance01/dance-01/SkeletalMeshes/dance-01.dance-01")); }
		return M.Get();
	}
	UAnimSequence* CrewAnimAsset()
	{
		static TWeakObjectPtr<UAnimSequence> A;
		static bool bTried = false;
		if (!bTried) { bTried = true; A = LoadObject<UAnimSequence>(nullptr, TEXT("/Game/KeysRun/Crew/Dance01/dance-01/SkeletalMeshes/dance-01_Anim.dance-01_Anim")); }
		return A.Get();
	}
}

bool AKeysExtras::PartyModelsReady()
{
	static int32 Off = -1;
	if (Off < 0) Off = FParse::Param(FCommandLine::Get(), TEXT("KRNoCrewModels")) ? 1 : 0;
	return Off == 0 && CrewMeshAsset() && CrewAnimAsset() && CrewMesh;
}

void AKeysExtras::UpdatePartyModels()
{
	if (!PartyModelsReady())
	{
		for (USkeletalMeshComponent* C : PartyModels) if (C) C->SetVisibility(false);
		return;
	}
	// one figure per guest, sized like the web placements (1.6 - 1.72 m)
	while (PartyModels.Num() < Party.Num())
	{
		USkeletalMeshComponent* C = NewObject<USkeletalMeshComponent>(this);
		C->SetSkeletalMesh(CrewMeshAsset());
		C->SetupAttachment(CrewMesh);
		C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		// like the web game (entities/crew-model/asset.ts): they take the deck's shadows but don't cast their own;
		// the decimated skinned mesh self-shadows into dark facets otherwise
		C->SetCastShadow(false);
		C->RegisterComponent();
		C->SetAnimationMode(EAnimationMode::AnimationSingleNode);
		C->PlayAnimation(CrewAnimAsset(), true);
		// a random start in the clip, so a deck of them isn't dancing in lockstep (index.ts spawnFigure)
		C->SetPosition(FMath::FRandRange(0.f, CrewAnimAsset()->GetPlayLength()), false);
		C->SetPlayRate(FMath::FRandRange(.92f, 1.06f));
		PartyModels.Add(C);
	}
	FVector Eye;
	const AKeysBoatPawn* EP = PawnRef.Get();
	const bool bEye = EP && EP->FpEyeLocal(Eye);
	static const double Heights[5] = {1.67, 1.63, 1.7, 1.65, 1.68};
	for (int32 i = 0; i < PartyModels.Num(); i++)
	{
		USkeletalMeshComponent* C = PartyModels[i];
		if (!C) continue;
		const bool bOn = Party.IsValidIndex(i) && (!bEye || FVector2D(Party[i].H.Pos.X - Eye.X, Party[i].H.Pos.Y - Eye.Y).Size() > 1.1);
		C->SetVisibility(bOn);
		if (!bOn) continue;
		const FKeysHuman& H = Party[i].H;
		const double Sc = Heights[i % 5] / CrewTemplateH;
		// the glTF faces +Z in its own frame, which the importer turns into UE -Y: a quarter turn puts her face along +X
		C->SetRelativeLocationAndRotation(FVector(H.Pos.X, H.Pos.Y, Deck.SoleAt(H.Pos.X)) * 100.0, FRotator(0, FMath::RadiansToDegrees(H.Yaw) + CrewYawOffset(), 0));
		C->SetRelativeScale3D(FVector(Sc));
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
		}
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
