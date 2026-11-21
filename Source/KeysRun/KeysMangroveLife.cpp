// The mangrove creeks of the real map, and the life in and over them.
//
//  - the creek lines: Sister Creek, the real cut between Boot Key and Vaca Key from Boot Key Harbor to the ocean, and
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
		Add(TEXT("Sister Creek"), 20, 0, 0, {{-310, 285}, {-300, 335}, {-270, 400}, {-275, 450}, {-320, 510}, {-300, 550}, {-240, 595}, {-235, 620}, {-305, 700}, {-322, 760}, {-322, 840}});
		Add(TEXT("Whiskey Creek"), 14, 34, 1000, {{-290, 462}, {-335, 455}, {-378, 452}, {-402, 470}, {-420, 505}, {-445, 530}, {-470, 540}, {-495, 522}, {-512, 502}});
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

	const TArray<FVector2D>& Haunts()
	{
		static const TArray<FVector2D> H = {{-518, 497}, {-272, 426}, {-237, 607}};
		return H;
	}

	const TArray<FVector2D>& Roosts()
	{
		// the harbour mouth of Sister Creek, its ocean mouth, the Whiskey Creek turning, and the pond
		static const TArray<FVector2D> R = {{-326, 296}, {-338, 806}, {-303, 470}, {-541, 512}};
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
		const FVector F(FMath::Cos(Yaw), 0, FMath::Sin(Yaw)), Up(0, 1, 0), Rt(-F.Z, 0, F.X);
		const FLinearColor Leg = KeysTownPaint::Hex(0x2a2a2c), Neck = KeysTownPaint::Hex(Pose ? 0x5a3a2a : 0xece6d4), Bill = KeysTownPaint::Hex(0xb9a987), Pouch = KeysTownPaint::Hex(0x4c4038);
		// she sits low: a heavy grey-brown body tipped up at the breast, the wings folded dark along it
		const FVector Body = Feet + Up * .2 * Sz - F * .04 * Sz;
		for (const double Sd : {-1.0, 1.0}) Limb(B, Body + Rt * .06 * Sd * Sz, Feet + Rt * .05 * Sd * Sz + F * .03 * Sz, .014 * Sz, Leg);
		B.Ball(Body.X + F.X * .07 * Sz, Body.Y + .05 * Sz, Body.Z + F.Z * .07 * Sz, .17 * Sz, 0x8b8378, .8, 6);            // breast
		B.Ball(Body.X - F.X * .1 * Sz, Body.Y + .02 * Sz, Body.Z - F.Z * .1 * Sz, .16 * Sz, 0x6e665c, .7, 6);             // back and rump
		for (const double Sd : {-1.0, 1.0})
			Limb(B, Body + Rt * .13 * Sd * Sz + F * .1 * Sz + Up * .08 * Sz, Body + Rt * .1 * Sd * Sz - F * .36 * Sz - Up * .02 * Sz, .055 * Sz, KeysTownPaint::Hex(0x4f4942));   // folded wings
		// the neck folds back in an S and the head rests between the shoulders, the long bill laid down the breast
		const FVector Sh = Body + F * .13 * Sz + Up * .13 * Sz;
		const FVector Bend = Sh + Up * (Pose ? .24 : .16) * Sz - F * .07 * Sz;
		const FVector Head = Bend + Up * (Pose ? .16 : .1) * Sz + F * .07 * Sz;
		Limb(B, Sh, Bend, .05 * Sz, Neck);
		Limb(B, Bend, Head, .04 * Sz, Neck);
		B.Ball(Head.X, Head.Y, Head.Z, .058 * Sz, 0xf0e2a6, .9, 5);                                                         // the yellow-washed crown
		const FVector Tip = Head + F * (Pose ? .36 : .2) * Sz - Up * (Pose ? .1 : .3) * Sz;
		Limb(B, Head + F * .03 * Sz, Tip, .02 * Sz, Bill);
		Limb(B, Head + F * .03 * Sz - Up * .035 * Sz, Tip - Up * .012 * Sz, .03 * Sz, Pouch);                                // the pouch slung under it
	}

	int32 Snag(FTownBatch& B, double X, double Y0, double Z, double Seed, int32 N)
	{
		const FLinearColor Dead = KeysTownPaint::Hex(0xa39a8c), Grey = KeysTownPaint::Hex(0x8a8276);
		const double H = 4.6 + 1.6 * LHash(Seed, 1.0);
		const FVector Foot(X, Y0, Z), Top(X + (LHash(Seed, 2.0) - .5) * 1.2, Y0 + H, Z + (LHash(Seed, 3.0) - .5) * 1.2);
		Limb(B, Foot, Top, .13, Grey);
		for (int32 r = 0; r < 4; r++)   // what is left of the prop roots
		{
			const double A = 2 * PI * (r + LHash(Seed + r, 4.0)) / 4;
			Limb(B, Foot + FVector(0, 1.3, 0), Foot + FVector(FMath::Cos(A) * 1.5, -.3, FMath::Sin(A) * 1.5), .045, Grey);
		}
		int32 Birds = 0;
		const int32 OnLimbs = N >= 3 ? N - 1 : N, NL = FMath::Max(2, OnLimbs);   // the third bird takes the top
		for (int32 l = 0; l < NL; l++)
		{
			// a bare limb reaching out and a little up; a bird sits a way out along it, facing off the limb
			const double A = 2 * PI * (l + .6 * LHash(Seed, l + 5.0)) / NL, T = .5 + .42 * double(l) / NL, Reach = 1.5 + 1.2 * LHash(Seed + l, 6.0);
			const FVector From = FMath::Lerp(Foot, Top, T), To = From + FVector(FMath::Cos(A) * Reach, .5 + .5 * LHash(Seed, l + 7.0), FMath::Sin(A) * Reach);
			Limb(B, From, To, .06, Dead);
			if (l < OnLimbs)
			{
				const double U = .62 + .25 * LHash(Seed + l, 8.0);
				Pelican(B, FMath::Lerp(From, To, U) + FVector(0, .05, 0), A + PI / 2 + (LHash(Seed, l + 9.0) - .5) * 1.2, .95 + .2 * LHash(Seed + l, 10.0), LHash(Seed + l, 11.0) < .3 ? 1 : 0);
				Birds++;
			}
		}
		if (N >= 3) { Pelican(B, Top + FVector(0, .04, 0), 2 * PI * LHash(Seed, 12.0), 1.05, 1); Birds++; }
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
