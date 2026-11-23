// The mangrove creeks of the real map, and the life in and over them.
//
//  - the creek lines: Sister Creek, the real cut between Boot Key and Vaca Key from Boot Key Harbor to where its marked
//    channel takes over at the ocean mouth (the line stops short of daybeacon 8, which stands in mid-stream), and
//    Whiskey Creek, the tunnel that leaves it to wind into Boot Key and ends in a pond. KeysGeo cuts the ground and
//    deepens the water along them (it reads Creeks()); KeysMangrove.cpp grows the banks, the arching roots and the
//    roof; Tools/geo/bake_mangrove.py reads the same table to put mangrove down both banks.
//  - brown pelicans roosting on dead snags along the mangrove edge (built into the mangrove tiles)
//  - manatees lying up in the calm bends and in the pond (AKeysMangroveLife asks KeysLife for them)
#include "KeysMangroveLife.h"
#include "KeysGeo.h"
#include "KeysLife.h"
#include "KeysTown.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"

namespace
{
	double LHash(double A, double B) { const double H = FMath::Sin(A * 127.1 + B * 311.7) * 43758.5453; return H - FMath::FloorToDouble(H); }

	TArray<KeysMangroveLife::FCreek> BuildCreeks()
	{
		TArray<KeysMangroveLife::FCreek> Out;
		auto Add = [&Out](const TCHAR* Name, double W, double Roof0, double Roof1, std::initializer_list<FVector2D> Raw)
		{
			// round the corners (two passes of corner cutting, the ends kept)
			TArray<FVector2D> P(Raw);
			for (int32 Pass = 0; Pass < 2 && P.Num() > 2; Pass++)
			{
				TArray<FVector2D> Q;
				Q.Add(P[0]);
				for (int32 i = 0; i + 1 < P.Num(); i++)
				{
					if (i > 0) Q.Add(P[i] * .75 + P[i + 1] * .25);
					if (i + 2 < P.Num()) Q.Add(P[i] * .25 + P[i + 1] * .75);
				}
				Q.Add(P.Last());
				P = MoveTemp(Q);
			}
			KeysMangroveLife::FCreek C;
			C.Name = Name; C.W = W; C.Roof0 = Roof0; C.Roof1 = Roof1; C.Pts = MoveTemp(P);
			C.X0 = C.Z0 = 1e9; C.X1 = C.Z1 = -1e9;
			for (int32 i = 0; i < C.Pts.Num(); i++)
			{
				if (i) C.Len += FVector2D::Distance(C.Pts[i - 1], C.Pts[i]);
				C.X0 = FMath::Min(C.X0, C.Pts[i].X); C.X1 = FMath::Max(C.X1, C.Pts[i].X);
				C.Z0 = FMath::Min(C.Z0, C.Pts[i].Y); C.Z1 = FMath::Max(C.Z1, C.Pts[i].Y);
			}
			C.X0 -= W + 30; C.X1 += W + 30; C.Z0 -= W + 30; C.Z1 += W + 30;
			Out.Add(MoveTemp(C));
		};
		// name, width, roofed from .. to (metres along), centre line. Tools/geo/bake_mangrove.py reads these lines as
		// they are written: one creek to a line, numbers only.
		// CREEKS-BEGIN
		Add(TEXT("Sister Creek"), 20, 0, 0, {{-310, 285}, {-300, 335}, {-270, 400}, {-275, 450}, {-320, 510}, {-300, 550}, {-240, 595}, {-235, 620}, {-305, 700}, {-322, 735}, {-330, 760}});
		Add(TEXT("Whiskey Creek"), 16, 34, 1000, {{-290, 462}, {-340, 455}, {-385, 458}, {-425, 478}, {-460, 500}, {-492, 506}, {-512, 500}});
		Add(TEXT("Whiskey Creek"), 40, 0, 0, {{-528, 497}, {-508, 497}});
		// CREEKS-END
		return Out;
	}
}

namespace KeysMangroveLife
{
	const TArray<FCreek>& Creeks()
	{
		static const TArray<FCreek> C = BuildCreeks();
		return C;
	}

	FNear Near(double X, double Z)
	{
		FNear Best;
		const TArray<FCreek>& Cs = Creeks();
		for (int32 k = 0; k < Cs.Num(); k++)
		{
			const FCreek& C = Cs[k];
			if (X < C.X0 || X > C.X1 || Z < C.Z0 || Z > C.Z1) continue;
			double S = 0;
			for (int32 i = 0; i + 1 < C.Pts.Num(); i++)
			{
				const FVector2D A = C.Pts[i], V = C.Pts[i + 1] - A;
				const double L = V.Size(), T = L > 1e-6 ? FMath::Clamp(((X - A.X) * V.X + (Z - A.Y) * V.Y) / (L * L), 0.0, 1.0) : 0;
				const FVector2D P = A + V * T;
				const double D = FMath::Sqrt(FMath::Square(X - P.X) + FMath::Square(Z - P.Y)) - C.W / 2;
				if (D < Best.D)
				{
					Best.D = D; Best.Creek = k; Best.S = S + L * T; Best.C = P; Best.T = L > 1e-6 ? V / L : FVector2D(1, 0);
					Best.bRoof = C.Roof1 > C.Roof0 && Best.S >= C.Roof0 && Best.S <= C.Roof1;
				}
				S += L;
			}
		}
		return Best;
	}

	bool Pilot(int32 Creek, double X, double Z, double& InOutH)
	{
		const TArray<FCreek>& Cs = Creeks();
		if (!Cs.IsValidIndex(Creek)) return false;
		const FCreek& C = Cs[Creek];
		// where she is along the line, then a point a boat's length and a half further on to steer for
		double Best = 1e18, At = 0, S = 0;
		for (int32 i = 0; i + 1 < C.Pts.Num(); i++)
		{
			const FVector2D A = C.Pts[i], V = C.Pts[i + 1] - A;
			const double L = V.Size(), T = L > 1e-6 ? FMath::Clamp(((X - A.X) * V.X + (Z - A.Y) * V.Y) / (L * L), 0.0, 1.0) : 0;
			const double D = FMath::Square(X - A.X - V.X * T) + FMath::Square(Z - A.Y - V.Y * T);
			if (D < Best) { Best = D; At = S + L * T; }
			S += L;
		}
		if (At > C.Len - 3) return false;
		const double Aim = FMath::Min(C.Len, At + 11);
		S = 0;
		for (int32 i = 0; i + 1 < C.Pts.Num(); i++)
		{
			const FVector2D A = C.Pts[i], V = C.Pts[i + 1] - A;
			const double L = V.Size();
			if (Aim <= S + L || i + 2 == C.Pts.Num())
			{
				const FVector2D P = A + V * (L > 1e-6 ? FMath::Clamp((Aim - S) / L, 0.0, 1.0) : 0);
				InOutH = FMath::Atan2(-(P.X - X), -(P.Y - Z));   // forward is (-sin H, -cos H)
				return true;
			}
			S += L;
		}
		return false;
	}

	const TArray<FVector2D>& Haunts()
	{
		static const TArray<FVector2D> H = {{-518, 497}, {-272, 426}, {-237, 607}};
		return H;
	}

	const TArray<FVector2D>& Roosts()
	{
		// the harbour mouth of Sister Creek, its ocean mouth, the Whiskey Creek turning, and the pond
		static const TArray<FVector2D> R = {{-326, 296}, {-347, 752}, {-307, 477}, {-541, 512}};
		return R;
	}

	void Limb(FTownBatch& B, const FVector& P0, const FVector& P1, double R, const FLinearColor& C)
	{
		const FVector D = (P1 - P0).GetSafeNormal();
		const FVector U = FVector::CrossProduct(D, FMath::Abs(D.Y) < .9 ? FVector(0, 1, 0) : FVector(1, 0, 0)).GetSafeNormal();
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

	void Pelican(FTownBatch& B, const FVector& Feet, double Yaw, double Sz, int32 Pose)
	{
		// Built on the user's picture of an adult brown pelican standing on a piling: pale yellow crown, white neck, the
		// long bill held down against the breast (grey at the base, orange-pink toward the tip, a dark grey pouch under
		// it), silver grey-brown back and folded wings, dark belly, short dark legs, the wingtips and the short tail
		// hanging below its perch. They are mixed as they are in life: about one in five is a young bird (brown head
		// and neck, pale belly) and one in seven a white pelican (white all over, black wingtips, orange bill and legs).
		const FVector F(FMath::Cos(Yaw), 0, FMath::Sin(Yaw)), Up(0, 1, 0), Rt(-F.Z, 0, F.X);
		const double Hh = LHash(Feet.X * .37 + 3.0, Feet.Z * .53);
		const bool bWhite = Hh < .14, bYoung = !bWhite && Hh < .34;
		const double S2 = Sz * (bWhite ? 1.12 : .92 + .16 * LHash(Feet.Z * .7, Feet.X * .3 + 1.0));
		const uint32 BackC = bWhite ? 0xf4f3ee : bYoung ? 0x6c5c4a : 0x80858c, BellyC = bWhite ? 0xf2eedc : bYoung ? 0xe2dccc : 0x231e1b, WingC = bWhite ? 0xf4f3ee : bYoung ? 0x64543f : 0x767b82;
		const uint32 NeckC = bWhite ? 0xf4f3ee : bYoung ? 0x64523f : 0xf0eee6, CrownC = bWhite ? 0xf4f3ee : bYoung ? 0x64523f : 0xf0d470;
		const FLinearColor Leg = KeysTownPaint::Hex(bWhite ? 0xee7c3c : 0x222226), Neck = KeysTownPaint::Hex(NeckC), BillBase = KeysTownPaint::Hex(bWhite ? 0xee9a28 : 0x7c7c80),
			BillTip = KeysTownPaint::Hex(bWhite ? 0xe06a24 : 0xe07850), Pouch = KeysTownPaint::Hex(bWhite ? 0xf2a820 : 0x34363a), Tips = KeysTownPaint::Hex(bWhite ? 0x101012 : 0x302c28);
		// it sits upright and hunched: a heavy body tipped up at the breast, the wings folded along it
		const FVector Body = Feet + Up * .2 * S2 - F * .04 * S2;
		for (const double Sd : {-1.0, 1.0})
		{
			const FVector Ft = Feet + Rt * .06 * Sd * S2 + F * .03 * S2;
			Limb(B, Body + Rt * .06 * Sd * S2, Ft, .016 * S2, Leg);
			B.Tri3(Ft, Ft + F * .12 * S2 + Rt * .05 * S2, Ft + F * .12 * S2 - Rt * .05 * S2, Up, Leg);   // the webbed foot
		}
		B.Ball(Body.X + F.X * .07 * S2, Body.Y + .05 * S2, Body.Z + F.Z * .07 * S2, .17 * S2, BellyC, .85, 6);           // breast and belly
		B.Ball(Body.X - F.X * .09 * S2, Body.Y + .07 * S2, Body.Z - F.Z * .09 * S2, .165 * S2, BackC, .75, 6);           // back and rump
		for (const double Sd : {-1.0, 1.0})
		{
			// the folded wing down the flank, its dark tip crossing the tail and hanging below the perch
			const FVector W0 = Body + Rt * .14 * Sd * S2 + F * .1 * S2 + Up * .1 * S2, W1 = Body + Rt * .1 * Sd * S2 - F * .26 * S2 - Up * .06 * S2;
			Limb(B, W0, W1, .06 * S2, KeysTownPaint::Hex(WingC));
			Limb(B, W1, W1 - F * .14 * S2 - Up * .12 * S2 - Rt * .04 * Sd * S2, .035 * S2, Tips);
		}
		Limb(B, Body - F * .2 * S2 - Up * .02 * S2, Body - F * .3 * S2 - Up * .14 * S2, .05 * S2, KeysTownPaint::Hex(bWhite ? 0xf4f3ee : 0x443f3a));   // the short tail
		// the neck folds back in an S and the head rests between the shoulders, the long bill laid down the breast
		const FVector Sh = Body + F * .13 * S2 + Up * .13 * S2;
		const FVector Bend = Sh + Up * (Pose ? .24 : .17) * S2 - F * .07 * S2;
		const FVector Head = Bend + Up * (Pose ? .16 : .1) * S2 + F * .07 * S2;
		Limb(B, Sh, Bend, .052 * S2, Neck);
		Limb(B, Bend, Head, .042 * S2, Neck);
		B.Ball(Head.X, Head.Y, Head.Z, .064 * S2, CrownC, .9, 5);
		const FVector Tip = Head + F * (Pose ? .38 : .2) * S2 - Up * (Pose ? .12 : .34) * S2, Mid = FMath::Lerp(Head + F * .03 * S2, Tip, .5);
		Limb(B, Head + F * .03 * S2, Mid, .024 * S2, BillBase);
		Limb(B, Mid, Tip, .018 * S2, BillTip);
		Limb(B, Head + F * .03 * S2 - Up * .04 * S2, Tip - Up * .012 * S2, .034 * S2, Pouch);                                // the pouch slung under it
	}

	int32 Snag(FTownBatch& B, double X, double Y0, double Z, double Seed, int32 N)
	{
		const FLinearColor Dead = KeysTownPaint::Hex(0xa39a8c), Grey = KeysTownPaint::Hex(0x8a8276);
		const double H = 5.2 + 1.6 * LHash(Seed, 1.0);
		const FVector Foot(X, Y0, Z), Top(X + (LHash(Seed, 2.0) - .5) * 1.2, Y0 + H, Z + (LHash(Seed, 3.0) - .5) * 1.2);
		Limb(B, Foot, Top, .2, Grey);
		for (int32 r = 0; r < 4; r++)   // what is left of the prop roots
		{
			const double A = 2 * PI * (r + LHash(Seed + r, 4.0)) / 4;
			Limb(B, Foot + FVector(0, 1.3, 0), Foot + FVector(FMath::Cos(A) * 1.5, -.3, FMath::Sin(A) * 1.5), .07, Grey);
		}
		int32 Birds = 0;
		const int32 OnLimbs = N >= 3 ? N - 1 : N, NL = FMath::Max(2, OnLimbs);   // the third bird takes the top
		for (int32 l = 0; l < NL; l++)
		{
			// a bare limb reaching out and a little up; a bird sits a way out along it, facing off the limb
			const double A = 2 * PI * (l + .6 * LHash(Seed, l + 5.0)) / NL, T = .5 + .42 * double(l) / NL, Reach = 2.0 + 1.4 * LHash(Seed + l, 6.0);
			const FVector From = FMath::Lerp(Foot, Top, T), To = From + FVector(FMath::Cos(A) * Reach, .5 + .5 * LHash(Seed, l + 7.0), FMath::Sin(A) * Reach);
			Limb(B, From, To, .1, Dead);
			if (l < OnLimbs)
			{
				const double U = .62 + .25 * LHash(Seed + l, 8.0);
				Pelican(B, FMath::Lerp(From, To, U) + FVector(0, .09, 0), A + PI / 2 + (LHash(Seed, l + 9.0) - .5) * 1.2, 1.7 + .3 * LHash(Seed + l, 10.0), LHash(Seed + l, 11.0) < .3 ? 1 : 0);
				Birds++;
			}
		}
		if (N >= 3) { Pelican(B, Top + FVector(0, .12, 0), 2 * PI * LHash(Seed, 12.0), 1.9, 1); Birds++; }
		return Birds;
	}
}

AKeysMangroveLife::AKeysMangroveLife()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickInterval = .5f;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
}

void AKeysMangroveLife::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	const TArray<FVector2D>& Hs = KeysMangroveLife::Haunts();
	if (Armed.Num() != Hs.Num()) Armed.Init(true, Hs.Num());
	const APlayerController* PC = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr;
	const APawn* Boat = PC ? PC->GetPawn() : nullptr;
	AKeysLife* Life = AKeysLife::Get(this);
	if (!Boat || !Life) return;
	const double BX = Boat->GetActorLocation().X / 100, BZ = Boat->GetActorLocation().Y / 100;
	for (int32 i = 0; i < Hs.Num(); i++)
	{
		const double D = FMath::Sqrt(FMath::Square(Hs[i].X - BX) + FMath::Square(Hs[i].Y - BZ));
		// KeysLife retires a group 270 m from the boat, so a haunt is filled again only after you have been away
		if (D > 330) Armed[i] = true;
		else if (Armed[i] && D < 200 && D > 25 && KeysGeo::ZoneAt(Hs[i].X, Hs[i].Y) == TEXT("Creek") && Life->DebugSpawn(TEXT("manatee"), Hs[i].X, Hs[i].Y))
		{
			Armed[i] = false;
			UE_LOG(LogTemp, Log, TEXT("KRMangrove manatees lying up at %.0f, %.0f"), Hs[i].X, Hs[i].Y);
		}
	}
}
