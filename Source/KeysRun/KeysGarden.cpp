#include "KeysGarden.h"
#include "KeysGeo.h"
#include "KeysSea.h"
#include "KeysMesh.h"
#include "KeysWorld.h"
#include "ProceduralMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Kismet/GameplayStatics.h"
#include "Camera/PlayerCameraManager.h"
#include "EngineUtils.h"
#include "Engine/World.h"

// Built in the style of Noniv's "Abyssal Garden" (MIT licence, (c) 2026 Maksymilian Dendura,
// https://github.com/Noniv/abyssal-garden): the scene's ideas - light rays, the wooden wreck, the stone whale
// fluke, mantas, barrel sponges and the magenta grotto glow - rebuilt here as procedural meshes for this game.
// Its kelp forest is not here: kelp does not grow in the Florida Keys. In its place stand the things that sway on a
// deep Keys reef, each after a real animal (lengths are colony heights):
//   sea plumes        Antillogorgia spp.         0.9-1.9 m, feathered, violet to grey-purple, some yellowish
//   sea rods          Plexaura, Eunicea,         0.6-1.3 m, candelabra of thick fingers, tan, dark brown or purple
//                     Plexaurella spp.
//   sea whips         Ellisella barbadensis      1.2-2.4 m, bundles of thin unbranched whips, red-orange (some yellow)
//   wire coral        Stichopathes luetkeni      2.2-4.2 m, one wire, coiled at the tip, olive to brown
//   black coral       Antipathes and             0.8-1.6 m bushes, rust-brown to grey-green over a black skeleton
//                     Plumapathes spp.
//   deep-water        Iciligorgia schrammi       0.8-1.5 m, one flat forking fan set across the current, red-brown
//   sea fan
//   rope sponges      Amphimedon compressa,      0.6-1.4 m, lumpy ropes, red or lavender
//                     Aplysina cauliformis
// and, standing still, the large sponges: giant barrel (Xestospongia muta, red-brown to grey, to 1.7 m), yellow tube
// (Aplysina fistularis), stove-pipe (Aplysina archeri, lavender-grey, to 1.6 m) and orange elephant ear (Agelas clathrodes).

namespace
{
	FLinearColor Hx(uint32 C) { return FLinearColor(FColor(uint8((C >> 16) & 255), uint8((C >> 8) & 255), uint8(C & 255))); }
	FLinearColor Mix(const FLinearColor& A, const FLinearColor& B, double T) { return FLinearColor::LerpUsingHSV(A, B, float(T)); }

	// where things sit, site-local meters (x east = game x, y = game z)
	const FVector2D ShipAt(20, 14), FlukeAt(-6, -24), ArchAt(30, -26), GlowAt(-22, 30);
	constexpr double SiteR = 62;
}

AKeysGarden::AKeysGarden()
{
	PrimaryActorTick.bCanEverTick = true;
	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);
	auto Make = [this](const TCHAR* N)
	{
		UProceduralMeshComponent* P = CreateDefaultSubobject<UProceduralMeshComponent>(N);
		P->SetupAttachment(Root);
		P->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		return P;
	};
	Static = Make(TEXT("GardenStatic"));
	Sway = Make(TEXT("GardenSway"));
	Mantas = Make(TEXT("GardenMantas"));
	Glow = Make(TEXT("GardenGlow"));
	Sway->SetCastShadow(false);
	Mantas->SetCastShadow(false);
	Glow->SetCastShadow(false);
}

AKeysGarden* AKeysGarden::Get(const UObject* WorldContext)
{
	UWorld* W = WorldContext ? WorldContext->GetWorld() : nullptr;
	if (!W) return nullptr;
	TActorIterator<AKeysGarden> It(W);
	return It ? *It : nullptr;
}

double AKeysGarden::Ground(double LX, double LY) const
{
	return KeysSea::SeafloorHeightAt(CX + LX, CZ + LY);
}

// a flat, sandy stretch 11-16 m down, off the reef: a freediver can get there and still have the light
void AKeysGarden::FindSite()
{
	const double X0 = -1150, Z0 = KeysGeo::LegacyZ(-1150, 900);
	CX = X0; CZ = Z0;
	for (double R = 0; R < 9000; R += 40)
	{
		const int32 N = FMath::Max(1, int32(R / 30));
		for (int32 k = 0; k < N; k++)
		{
			const double A = k * 2 * PI / N;
			const double PX = X0 + FMath::Cos(A) * R, PZ = Z0 + FMath::Sin(A) * R;
			const double D = KeysGeo::DepthAt(PX, PZ);
			if (D < 9 || D > 18) continue;
			double Lo = 1e9, Hi = -1e9;
			bool bOk = true;
			for (int32 j = 0; j < 12 && bOk; j++)
			{
				const double B = j * 2 * PI / 12;
				for (double Rr : {35.0, 70.0})
				{
					const double DD = KeysGeo::DepthAt(PX + FMath::Cos(B) * Rr, PZ + FMath::Sin(B) * Rr);
					if (DD < 6) { bOk = false; break; }
					Lo = FMath::Min(Lo, DD); Hi = FMath::Max(Hi, DD);
				}
			}
			if (!bOk || Hi - Lo > 7) continue;
			CX = PX; CZ = PZ;
			UE_LOG(LogTemp, Log, TEXT("Keys Run: Abyssal Garden at %.0f, %.0f, %.1f m"), CX, CZ, D);
			return;
		}
	}
	UE_LOG(LogTemp, Warning, TEXT("Keys Run: no flat 11-16 m ground found for the Abyssal Garden; using %.0f, %.0f"), CX, CZ);
}

void AKeysGarden::BeginPlay()
{
	Super::BeginPlay();
	KeysGeo::Init();
	FindSite();
	SetActorLocation(KeysGeo::ToUE(CX, 0, CZ));
	// the rest waits for the seabed data (DepthFast reads 0 until it's loaded): see Tick
}

void AKeysGarden::BuildAll()
{
	UE_LOG(LogTemp, Log, TEXT("Keys Run: Abyssal Garden ground at centre %.1f"), Ground(0, 0));
	PlantThickets();
	BuildStatic();
	BuildGlow();
	BuildSway(0);
	BuildMantas(0);
}

// ---------------------------------------------------------------- the thickets: what grows where, in its pose at rest
// Rock ledges on the west half of the site (where the kelp clumps stood), lanes left open to swim through. Each carries
// a crowd of gorgonians, black corals and rope sponges on its top and a few more on the sand at its foot.
void AKeysGarden::PlantThickets()
{
	FRandomStream R(7311);
	const FVector Flow = FVector(.8, .35, 0).GetSafeNormal(), Cross(-Flow.Y, Flow.X, 0);
	int32 Cur = -1;
	auto Begin = [&](const FVector& Base, double H, double Flex)
	{
		FColony Cn;
		Cn.Base = Base; Cn.H = H; Cn.Flex = Flex; Cn.Phase = R.FRandRange(0, 2 * PI);
		Cn.Seg0 = Segs.Num(); Cn.Leaf0 = Leaves.Num(); Cn.SegN = Cn.LeafN = 0;
		Cur = Colonies.Add(Cn);
	};
	auto Seg = [&](const FVector& A, const FVector& B, double RA, double RB, int32 Sides, const FLinearColor& Col)
	{
		FSeg S;
		S.A = A; S.B = B; S.RA = float(RA); S.RB = float(RB); S.Sides = uint8(Sides); S.C = Col;
		Segs.Add(S);
		Colonies[Cur].SegN++;
	};
	auto Leaf = [&](const FVector& A, const FVector& B, const FVector& C, const FVector& D, const FLinearColor& Col)
	{
		FLeaf L;
		L.P[0] = A; L.P[1] = B; L.P[2] = C; L.P[3] = D; L.C = Col;
		Leaves.Add(L);
		Colonies[Cur].LeafN++;
	};
	auto Around = [&]() { const double Az = R.FRandRange(0, 2 * PI); return FVector(FMath::Cos(Az), FMath::Sin(Az), 0); };

	// a sea plume: a short trunk, four to six long branches, each feathered with branchlets in one plane
	auto Plume = [&](const FVector& Base)
	{
		const double H = R.FRandRange(1.2, 2.2);
		Begin(Base, H, .26);
		const FLinearColor Col = Mix(Mix(Hx(0x8f6cc0), Hx(0xc0a8dc), R.FRand()), Hx(0xd6c486), R.FRand() < .3 ? .8 : .1);
		const FLinearColor Dk = Col * .7f;
		const int32 NB = R.RandRange(4, 6);
		const FVector Trunk = Base + FVector(0, 0, H * .12);
		Seg(Base, Trunk, .045, .035, 3, Dk);
		for (int32 b = 0; b < NB; b++)
		{
			const double Az = b * 2 * PI / NB + R.FRandRange(-.4, .4), Tilt = R.FRandRange(.12, .38);
			const FVector Out(FMath::Cos(Az), FMath::Sin(Az), 0);
			const FVector Side = FVector::CrossProduct(FVector::UpVector, Out);
			const double L = H * R.FRandRange(.7, .88);
			FVector Prev = Trunk;
			for (int32 k = 1; k <= 3; k++)
			{
				const double U = k / 3.0;
				const FVector P = Trunk + Out * (L * Tilt * U * (1.2 - .4 * U)) + FVector(0, 0, L * U);   // leans out, then straightens
				Seg(Prev, P, .03 - .008 * (k - 1), .03 - .008 * k, 3, Dk);
				const FVector Dir = (P - Prev).GetSafeNormal();
				for (int32 j = 0; j < 2; j++)
					for (int32 Sgn = -1; Sgn <= 1; Sgn += 2)
					{
						const FVector Q = FMath::Lerp(Prev, P, (j + .5) / 2.0);
						const double Len = (.34 - .14 * U) * R.FRandRange(.8, 1.2), Wd = L / 6 * .8;
						const FVector Tw = (Side * (Sgn * .8) + Dir * .6).GetSafeNormal() * Len;
						Leaf(Q - Dir * (Wd * .5), Q + Dir * (Wd * .5), Q + Tw + Dir * (Wd * .5), Q + Tw - Dir * (Wd * .1), Col);   // a vane of branchlets
					}
				Prev = P;
			}
		}
	};
	// sea rods: a candelabra of thick fingers that go out, then up
	auto Rods = [&](const FVector& Base)
	{
		const double H = R.FRandRange(.8, 1.5);
		Begin(Base, H, .1);
		static const uint32 Cs[3] = {0xc9ae6c, 0x7a5c3c, 0x8a5cae};
		const FLinearColor Col = Hx(Cs[R.RandRange(0, 2)]);
		const int32 NB = R.RandRange(5, 8);
		for (int32 b = 0; b < NB; b++)
		{
			const FVector Out = Around();
			const double Sp = R.FRandRange(.1, .35) * H, L = H * R.FRandRange(.6, 1.0);
			FVector Prev = Base;
			for (int32 k = 1; k <= 3; k++)
			{
				const double U = k / 3.0;
				const FVector P = Base + Out * (Sp * FMath::Sqrt(U)) + FVector(0, 0, L * (.4 * U * U + .6 * U));
				Seg(Prev, P, .04, k == 3 ? .028 : .04, 3, Col);
				Prev = P;
			}
		}
	};
	// sea whips: a bundle of long thin whips from one holdfast
	auto Whips = [&](const FVector& Base)
	{
		const double H = R.FRandRange(1.5, 2.6);
		Begin(Base, H, .3);
		const FLinearColor Col = Mix(Hx(0xe2502c), Hx(0xe8b23c), R.FRand() < .3 ? .6 + .4 * R.FRand() : R.FRand() * .25);
		const int32 NB = R.RandRange(5, 9);
		for (int32 b = 0; b < NB; b++)
		{
			const FVector Out = Around();
			const double Sp = R.FRandRange(.05, .3) * H, L = H * R.FRandRange(.6, 1.0);
			FVector Prev = Base;
			for (int32 k = 1; k <= 4; k++)
			{
				const double U = k / 4.0;
				const FVector P = Base + Out * (Sp * U) + FVector(0, 0, L * U);
				Seg(Prev, P, .024 - .004 * (k - 1), .024 - .004 * k, 3, Col);
				Prev = P;
			}
		}
	};
	// wire coral: one long wire, coiling at its tip
	auto Wire = [&](const FVector& Base)
	{
		const double H = R.FRandRange(2.5, 4.5);
		Begin(Base, H, .2);
		const FLinearColor Col = Mix(Hx(0xa8ae44), Hx(0x86703c), R.FRand());
		const double Az0 = R.FRandRange(0, 2 * PI), Coil = R.FRandRange(.12, .3);
		FVector Prev = Base;
		for (int32 k = 1; k <= 12; k++)
		{
			const double U = k / 12.0, Cu = FMath::Max(0.0, U - .55) / .45;
			const double Ang = Az0 + Cu * 9, Rad = Coil * Cu;
			const FVector P = Base + FVector(FMath::Cos(Ang) * Rad + .15 * H * U * FMath::Cos(Az0), FMath::Sin(Ang) * Rad + .15 * H * U * FMath::Sin(Az0), H * (U - .12 * Cu * Cu));
			Seg(Prev, P, .022, .018, 3, Col);
			Prev = P;
		}
	};
	// a black coral bush: a dark trunk, arching boughs, sprays of fine twigs in the living colour
	auto Bush = [&](const FVector& Base)
	{
		const double H = R.FRandRange(1.0, 1.8);
		Begin(Base, H, .07);
		const FLinearColor Col = Mix(Hx(0xb0703c), Hx(0x8a8c66), R.FRand()), Dk = Hx(0x2a211b);
		const FVector Top = Base + FVector(0, 0, H * .3);
		Seg(Base, Top, .05, .035, 3, Dk);
		const int32 NB = R.RandRange(5, 7);
		for (int32 b = 0; b < NB; b++)
		{
			const FVector Tip = Top + Around() * (H * R.FRandRange(.35, .6)) + FVector(0, 0, H * R.FRandRange(.35, .7));
			FVector Prev = Top;
			for (int32 k = 1; k <= 3; k++)
			{
				const double U = k / 3.0;
				const FVector P = FMath::Lerp(Top, Tip, U) + FVector(0, 0, H * .08 * FMath::Sin(U * PI));
				Seg(Prev, P, .03 - .006 * (k - 1), .03 - .006 * k, 3, Mix(Dk, Col, .5 * U));
				for (int32 j = 0; j < 2; j++)
				{
					// a spray of fine twigs, drawn as one small frond
					const FVector Tw = (Around() * .7 + FVector(0, 0, .7)) * (H * R.FRandRange(.16, .26)), Wd = FVector::CrossProduct(Tw, FVector::UpVector).GetSafeNormal() * (H * .07);
					Leaf(P - Wd * .3, P + Wd * .3, P + Tw + Wd, P + Tw - Wd, Col);
				}
				Prev = P;
			}
		}
	};
	// a deep-water sea fan: one flat fan, forking again and again, set across the current
	auto Fan = [&](const FVector& Base)
	{
		const double H = R.FRandRange(1.0, 1.7);
		Begin(Base, H, .05);
		const FLinearColor Col = Mix(Hx(0xb04a30), Hx(0xcc7436), R.FRand());
		const FVector Wd = (Cross + Flow * R.FRandRange(-.3, .3)).GetSafeNormal();
		struct FFork { FVector P; double Ang, Len; int32 Depth; };
		TArray<FFork> St;
		St.Add({Base, R.FRandRange(-.1, .1), H * .34, 0});
		while (St.Num())
		{
			const FFork F = St.Pop();
			const FVector E = F.P + (FVector::UpVector * FMath::Cos(F.Ang) + Wd * FMath::Sin(F.Ang)) * F.Len;
			if (F.Depth >= 4)
			{
				// the outermost forks, too fine to draw one by one: a small blade of the fan
				const FVector Wv = FVector::CrossProduct(E - F.P, Flow).GetSafeNormal() * (F.Len * .11);
				Leaf(F.P, F.P, E + Wv, E - Wv, Col);
				continue;
			}
			Seg(F.P, E, .045 * FMath::Pow(.62, double(F.Depth)), .045 * FMath::Pow(.62, double(F.Depth + 1)), 3, Col);
			const double Spread = .3 + .14 * R.FRand();
			St.Add({E, FMath::Clamp(F.Ang - Spread, -1.2, 1.2), F.Len * .78, F.Depth + 1});
			St.Add({E, FMath::Clamp(F.Ang + Spread * R.FRandRange(.7, 1.1), -1.2, 1.2), F.Len * .78, F.Depth + 1});
		}
	};
	// rope sponges: a few lumpy ropes, red or lavender
	auto Ropes = [&](const FVector& Base)
	{
		const double H = R.FRandRange(.8, 1.5);
		Begin(Base, H, .1);
		const FLinearColor Col = R.FRand() < .55 ? Hx(0xc8322e) : Hx(0xa890cc);
		const int32 NB = R.RandRange(3, 5);
		for (int32 b = 0; b < NB; b++)
		{
			const FVector Out = Around();
			const double Sp = R.FRandRange(.1, .4) * H, L = H * R.FRandRange(.6, 1.0);
			FVector Prev = Base;
			double RPrev = .05;
			for (int32 k = 1; k <= 4; k++)
			{
				const double U = k / 4.0, Rk = k == 4 ? .03 : .042 + .012 * ((k + b) % 2);
				const FVector P = Base + Out * (Sp * U * U) + FVector(0, 0, L * U);
				Seg(Prev, P, RPrev, Rk, 4, Col);
				Prev = P; RPrev = Rk;
			}
		}
	};

	auto Plant = [&](const FVector& Base)
	{
		const double Pick = R.FRand();
		if (Pick < .22) Plume(Base);
		else if (Pick < .38) Rods(Base);
		else if (Pick < .54) Whips(Base);
		else if (Pick < .66) Wire(Base);
		else if (Pick < .76) Bush(Base);
		else if (Pick < .88) Fan(Base);
		else Ropes(Base);
	};
	auto OnLedge = [&](const FLedge& L, double R0, double R1)
	{
		const double Az = R.FRandRange(0, 2 * PI), Rr = FMath::Sqrt(R.FRandRange(R0 * R0, R1 * R1));
		const FVector2D P = L.At + FVector2D(FMath::Cos(Az) * Rr * L.A, FMath::Sin(Az) * Rr * L.B).GetRotated(FMath::RadiansToDegrees(L.Yaw));
		return FVector(P.X, P.Y, L.Top - .06);
	};
	for (int32 c = 0; c < 26; c++)
	{
		const FVector2D Cc(R.FRandRange(-56, 6), R.FRandRange(-50, 50));
		if (FVector2D::Distance(Cc, FlukeAt) < 9 || Cc.Size() > SiteR) continue;
		// a ledge, and on most of them a second block standing on it
		FLedge L;
		L.At = Cc; L.A = R.FRandRange(3.0, 5.2); L.B = R.FRandRange(2.2, 3.8); L.H = R.FRandRange(.9, 1.8); L.Yaw = R.FRandRange(0, PI);
		L.Top = Ground(Cc.X, Cc.Y) + L.H; L.bUpper = false;
		Ledges.Add(L);
		const bool bTwo = R.FRand() < .7;
		for (int32 i = 0; i < 5; i++) Plant(OnLedge(L, bTwo ? .62 : 0, .82));
		if (bTwo)
		{
			FLedge Up = L;
			Up.At = Cc + FVector2D(R.FRandRange(-.5, .5), R.FRandRange(-.4, .4));
			Up.A = L.A * R.FRandRange(.42, .55); Up.B = L.B * R.FRandRange(.42, .55); Up.H = R.FRandRange(.8, 1.7); Up.Yaw = L.Yaw + R.FRandRange(-.5, .5);
			Up.Top = L.Top + Up.H; Up.bUpper = true;
			Ledges.Add(Up);
			for (int32 i = 0; i < 4; i++) Plant(OnLedge(Up, 0, .75));
		}
		// and on the sand and rubble round its foot
		for (int32 i = 0; i < 5; i++)
		{
			const double Az = R.FRandRange(0, 2 * PI), Rr = FMath::Max(L.A, L.B) + R.FRandRange(.6, 4.5);
			const FVector2D P = Cc + FVector2D(FMath::Cos(Az), FMath::Sin(Az)) * Rr;
			Plant(FVector(P.X, P.Y, Ground(P.X, P.Y) - .05));
		}
	}
	UE_LOG(LogTemp, Log, TEXT("Keys Run: Abyssal Garden - %d swaying colonies on %d ledges (%d branches, %d branchlets)"), Colonies.Num(), Ledges.Num(), Segs.Num(), Leaves.Num());
}

// ---------------------------------------------------------------- the things that don't move
void AKeysGarden::BuildStatic()
{
	FKeysMesh M;
	FRandomStream R(9127);
	const FLinearColor Rock = Hx(0x56656b), RockLt = Hx(0x7b8c8e), Fouling = Hx(0x4f6b45);

	// ---- the sunken ship: a wooden sailing ship, bow to the east, listing to port and half buried in the sand
	{
		const double L = 24, W = 3.6, Hd = 3.2;
		const double Yaw = .55, Roll = .24, Pitch = -.05;
		const FQuat Q = FQuat(FVector::UpVector, Yaw) * FQuat(FVector::ForwardVector, Roll) * FQuat(FVector::RightVector, Pitch);
		const FVector O(ShipAt.X, ShipAt.Y, Ground(ShipAt.X, ShipAt.Y) + Hd * .55);
		auto P = [&](double X, double Y, double Z) { return O + Q.RotateVector(FVector(X, Y, Z)); };
		auto HalfW = [&](double X) { const double U = X / (L * .5); return W * FMath::Sqrt(FMath::Max(0.0, 1 - FMath::Pow(FMath::Abs(U), U > 0 ? 2.6 : 5.0))) + (U < 0 ? .25 : 0); };
		auto Hull = [&](double X, double A, double S) { const double Hw = HalfW(X); return P(X, S * Hw * FMath::Sin(A), -Hd * FMath::Cos(A) * (.55 + .45 * FMath::Sqrt(FMath::Max(0.0, Hw / W)))); };
		const FLinearColor Wood = Hx(0x4b3b2b), WoodDk = Hx(0x2f261d), WoodLt = Hx(0x6a5640);
		const int32 Rows = 9, Cols = 22;
		// planking, row by row, with a breach on the starboard side and the odd plank gone
		for (int32 Sd = -1; Sd <= 1; Sd += 2)
			for (int32 v = 1; v <= Rows; v++)
			{
				const double A = PI * .5 * v / Rows;
				TArray<FVector> Run;
				auto Flush = [&]()
				{
					if (Run.Num() >= 2) M.AddSweep(Run, .2, .05, Q.RotateVector(FVector::UpVector), Mix(Wood, WoodLt, R.FRand() * .5), 4);
					Run.Reset();
				};
				for (int32 u = 0; u <= Cols; u++)
				{
					const double X = -L * .5 + L * u / Cols;
					const bool bBreach = Sd > 0 && v >= 2 && v <= 7 && X > -3 && X < 3.5;
					const bool bGone = R.FRand() < .07 || (v == Rows && R.FRand() < .25);
					if (bBreach || bGone || HalfW(X) < .05) { Flush(); continue; }
					Run.Add(Hull(X, A, Sd));
				}
				Flush();
			}
		// ribs every 1.2 m, showing through the breach and above the planks
		for (double X = -L * .5 + 1.2; X < L * .5 - 1.5; X += 1.2)
			for (int32 Sd = -1; Sd <= 1; Sd += 2)
			{
				TArray<FVector> Rib;
				for (int32 k = 0; k <= 8; k++) Rib.Add(Hull(X, PI * .5 * k / 8, Sd * 1.0) + Q.RotateVector(FVector(0, 0, k == 8 ? .35 : 0)));
				M.AddSweep(Rib, .12, .12, Q.RotateVector(FVector::ForwardVector), WoodDk, 4);
			}
		// keel, stem and stern posts
		M.AddTube(Hull(-L * .5, 0, 1), Hull(L * .5, 0, 1), .22, .18, 5, WoodDk);
		M.AddTube(Hull(L * .5, 0, 1), P(L * .5 + 1.4, 0, 1.6), .2, .14, 5, WoodDk);
		M.AddTube(Hull(-L * .5, 0, 1), P(-L * .5 - .3, 0, 2.6), .22, .2, 5, WoodDk);
		// the raised stern castle and its transom
		M.AddBox(P(-L * .5 + 2.3, 0, 1.1), FVector(4.2, W * 2 + .3, 2.2), Wood, Q);
		M.AddBox(P(-L * .5 + .3, 0, 1.5), FVector(.3, W * 2 + .6, 3.0), WoodDk, Q);
		for (int32 k = -1; k <= 1; k++) M.AddBox(P(-L * .5 + .15, k * 1.2, 1.9), FVector(.32, .55, .7), Hx(0x1b1612), Q);   // stern windows
		// what's left of the deck: beams across, a few planks on them
		for (double X = -L * .5 + 5; X < L * .5 - 3; X += 1.8)
			if (R.FRand() < .7) M.AddBox(P(X, 0, -.05), FVector(.22, HalfW(X) * 2, .2), WoodDk, Q);
		for (int32 k = 0; k < 9; k++)
		{
			const double X = R.FRandRange(-L * .5 + 5, L * .5 - 4), Y = R.FRandRange(-1.5, 1.5);
			M.AddBox(P(X, Y, .08), FVector(R.FRandRange(1.5, 3.5), .28, .06), WoodLt, Q * FQuat(FVector::UpVector, R.FRandRange(-.15, .15)));
		}
		// two masts (the main still standing with its yards, the fore snapped off), and the bowsprit
		const FVector MainB = P(-1, 0, -1), MainT = P(-1.3, 0, 11.5);
		M.AddTube(MainB, MainT, .3, .18, 7, WoodDk);
		for (double H : {6.5, 9.8})
		{
			const FVector C = FMath::Lerp(MainB, MainT, (H + 1) / 12.5);
			const FVector Sp = Q.RotateVector(FVector(0, 1, 0)).RotateAngleAxis(18, FVector::UpVector) * (H < 8 ? 4.2 : 3.0);
			M.AddTube(C - Sp, C + Sp + FVector(0, 0, H < 8 ? -.6 : .3), .12, .1, 5, WoodDk);
		}
		M.AddTube(P(6.5, 0, -1), P(6.8, 0, 4.2), .27, .22, 7, WoodDk);
		M.AddTube(P(6.8, 0, 4.2), P(7.6, .3, 5.1), .22, .08, 6, WoodLt);   // the splintered top
		M.AddTube(P(L * .5 + .6, 0, 1.2), P(L * .5 + 6, 0, 3.4), .2, .1, 6, WoodDk);
		// growth on her: fouling patches, a few sea fans and sponges on the rail
		for (int32 k = 0; k < 26; k++)
		{
			const double X = R.FRandRange(-L * .45, L * .45);
			const FVector Pp = Hull(X, PI * .5, R.FRand() < .5 ? -1 : 1) + FVector(0, 0, .1);
			M.AddRoundBox(Pp, FVector(R.FRandRange(.3, .9), R.FRandRange(.3, .7), .25), .1, Mix(Fouling, Hx(0x8a6a3a), R.FRand()), FQuat(FVector::UpVector, R.FRand() * 6));
		}
	}

	// ---- the stone whale fluke: a stalk rising out of the sand, curving over into the two great lobes of a tail
	{
		const FVector B(FlukeAt.X, FlukeAt.Y, Ground(FlukeAt.X, FlukeAt.Y) - .5);
		const FVector Fw = FVector(FMath::Cos(-.4), FMath::Sin(-.4), 0);   // the way the tail sweeps
		const FVector Sd = FVector::CrossProduct(FVector::UpVector, Fw);
		TArray<FVector> Spine;
		for (int32 k = 0; k <= 10; k++)
		{
			const double U = k / 10.0;
			Spine.Add(B + FVector(0, 0, 9.5 * U) + Fw * (2.2 * U * U));
		}
		for (int32 k = 0; k < 10; k++)
		{
			const double U0 = k / 10.0, U1 = (k + 1) / 10.0;
			M.AddTube(Spine[k], Spine[k + 1], 1.9 - 1.3 * U0, 1.9 - 1.3 * U1, 12, Mix(Rock, RockLt, .3 + .4 * U0));
		}
		// the lobes: a thick, swept-back blade each side, grids top and bottom
		const FVector Top = Spine.Last();
		for (int32 S = -1; S <= 1; S += 2)
			for (int32 Face = -1; Face <= 1; Face += 2)
			{
				const int32 NU = 10, NV = 6;
				TArray<FVector> G;
				for (int32 v = 0; v < NV; v++)
					for (int32 u = 0; u < NU; u++)
					{
						const double Su = u / double(NU - 1), Cv = v / double(NV - 1);
						const double Span = 6.5 * Su;
						const double Chord = 3.2 * (1 - .75 * Su * Su) ;
						const double Sweep = -2.6 * Su * Su;   // the tips sweep back (down the stalk)
						const double Th = .55 * (1 - Su) * FMath::Sin(Cv * PI) + .06;
						FVector Pp = Top + Sd * (S * Span) + Fw * ((Cv - .5) * Chord) + FVector(0, 0, Sweep + 1.1 * Su) + FVector(0, 0, Face * Th * .5);
						G.Add(Pp);
					}
				M.AddGrid(G, NU, NV, false, Mix(Rock, RockLt, .55), Top + FVector(0, 0, -Face * 3.0) + Sd * (S * 3.0));
			}
		// barnacles and sponges round its foot
		for (int32 k = 0; k < 14; k++)
		{
			const double A = R.FRand() * 2 * PI, Rr = R.FRandRange(1.8, 4.5);
			const FVector Pp(B.X + FMath::Cos(A) * Rr, B.Y + FMath::Sin(A) * Rr, 0);
			M.AddRoundBox(FVector(Pp.X, Pp.Y, Ground(Pp.X, Pp.Y) + .2), FVector(R.FRandRange(.4, 1.0), R.FRandRange(.4, .8), R.FRandRange(.3, .6)), .15, Mix(Rock, Fouling, R.FRand() * .6), FQuat(FVector::UpVector, A));
		}
	}

	// ---- a rock arch to swim through
	{
		const FVector C(ArchAt.X, ArchAt.Y, Ground(ArchAt.X, ArchAt.Y) - .8);
		const double Rad = 5.5;
		const FVector Ax = FVector(FMath::Cos(.9), FMath::Sin(.9), 0);
		TArray<FVector> Path;
		for (int32 k = 0; k <= 18; k++)
		{
			const double A = PI * k / 18;
			const double Wob = 1 + .08 * FMath::Sin(k * 1.7);
			Path.Add(C + Ax * (FMath::Cos(A) * Rad * Wob) + FVector(0, 0, FMath::Sin(A) * Rad * 1.15 * Wob));
		}
		M.AddSweep(Path, 1.8, 1.4, FVector::CrossProduct(Ax, FVector::UpVector), Rock, 10);
		for (int32 k = 0; k < 10; k++)
		{
			const FVector Pp = Path[R.RandRange(2, 16)];
			M.AddRoundBox(Pp + FVector(R.FRandRange(-1, 1), R.FRandRange(-1, 1), R.FRandRange(-.5, 1.2)), FVector(R.FRandRange(.5, 1.1)), .25, Mix(Rock, RockLt, R.FRand()), FQuat(FVector::UpVector, R.FRand() * 6));
		}
	}

	// ---- boulders scattered over the sand
	for (int32 k = 0; k < 40; k++)
	{
		const FVector2D P(R.FRandRange(-SiteR, SiteR), R.FRandRange(-SiteR, SiteR));
		if (P.Size() > SiteR || FVector2D::Distance(P, ShipAt) < 14 || FVector2D::Distance(P, FVector2D(18, -6)) < 10) continue;
		const double S = R.FRandRange(.35, 1.3);
		M.AddRoundBox(FVector(P.X, P.Y, Ground(P.X, P.Y) + S * .2), FVector(S * R.FRandRange(1, 1.8), S * R.FRandRange(.9, 1.5), S * R.FRandRange(.6, 1.1)), S * .3, Mix(Rock, RockLt, R.FRand() * .6), FQuat(FVector::UpVector, R.FRand() * 6));
	}

	// ---- giant barrel sponges, open at the top, and common sea fans (purple or yellow)
	auto Barrel = [&](const FVector2D& P, double S, const FLinearColor& C)
	{
		const FVector B(P.X, P.Y, Ground(P.X, P.Y) - .1);
		const double Rp[6] = {.45, .55, .62, .7, .78, .82};
		for (int32 k = 0; k < 5; k++) M.AddTube(B + FVector(0, 0, k * .32 * S), B + FVector(0, 0, (k + 1) * .32 * S), Rp[k] * S, Rp[k + 1] * S, 12, Mix(C, C * .7f, (k % 2) * .5));
		M.AddTube(B + FVector(0, 0, 1.6 * S), B + FVector(0, 0, .5 * S), .7 * S, .5 * S, 12, C * .35f);   // the dark inside
	};
	const FLinearColor SpC[3] = {Hx(0x7b5348), Hx(0x8a6a5c), Hx(0x6e5a52)};   // a giant barrel sponge is red-brown to grey, never purple or orange
	for (int32 k = 0; k < 22; k++)
	{
		const FVector2D P(R.FRandRange(-10, 50), R.FRandRange(-50, 45));
		if (P.Size() > SiteR || FVector2D::Distance(P, ShipAt) < 13 || FVector2D::Distance(P, ArchAt) < 7 || FVector2D::Distance(P, FVector2D(18, -6)) < 6) continue;
		Barrel(P, R.FRandRange(.45, 1.0), SpC[k % 3]);
	}
	for (int32 k = 0; k < 30; k++)
	{
		const FVector2D P(R.FRandRange(-SiteR, SiteR), R.FRandRange(-SiteR, SiteR));
		if (P.Size() > SiteR || FVector2D::Distance(P, ShipAt) < 13) continue;
		const FVector B(P.X, P.Y, Ground(P.X, P.Y));
		const FVector Sd = FVector(FMath::Cos(k * 2.3), FMath::Sin(k * 2.3), 0);
		const double S = R.FRandRange(.7, 1.6);
		const FLinearColor C = k % 2 ? Hx(0x9a3d8a) : Hx(0xd8b13a);
		for (int32 b = 0; b < 7; b++)
		{
			const double A = (b - 3) * .22;
			const FVector Dir = (FVector::UpVector * FMath::Cos(A) + Sd * FMath::Sin(A)).GetSafeNormal();
			const FVector Mid = B + Dir * .9 * S;
			M.AddTube(B, Mid, .05, .035, 4, C);
			M.AddTube(Mid, Mid + (Dir + Sd * (A * .8)).GetSafeNormal() * .7 * S, .035, .015, 4, C);
			M.AddTube(Mid, Mid + (Dir - Sd * .3).GetSafeNormal() * .5 * S, .03, .012, 4, C);
		}
	}

	// ---- the magenta grotto patch: low glowing ground cover round a ring of boulders
	for (int32 k = 0; k < 160; k++)
	{
		const double A = R.FRand() * 2 * PI, Rr = 10 * FMath::Sqrt(R.FRand());
		const FVector2D P = GlowAt + FVector2D(FMath::Cos(A) * Rr, FMath::Sin(A) * Rr);
		const FVector B(P.X, P.Y, Ground(P.X, P.Y) - .02);
		const double H = R.FRandRange(.15, .45);
		M.AddTube(B, B + FVector(R.FRandRange(-.1, .1), R.FRandRange(-.1, .1), H), .09, .02, 4, Mix(Hx(0xc2297e), Hx(0x7a2bb0), R.FRand()));
	}
	for (int32 k = 0; k < 8; k++)
	{
		const double A = k * 2 * PI / 8;
		const FVector2D P = GlowAt + FVector2D(FMath::Cos(A), FMath::Sin(A)) * 9.0;
		const double S = R.FRandRange(.8, 1.6);
		M.AddRoundBox(FVector(P.X, P.Y, Ground(P.X, P.Y) + S * .3), FVector(S * 1.4, S, S * 1.2), S * .35, Hx(0x3b3346), FQuat(FVector::UpVector, A));
	}
	// ---- the ledges the thickets stand on (PlantThickets), their rubble and crusts, and the large sponges
	{
		FRandomStream R2(5521);
		const FLinearColor Crust[4] = {Hx(0xb5542a), Hx(0x8a5a7a), Hx(0x9a2f2a), Hx(0x6f6a3a)};   // orange and red encrusting sponge, coralline algae, turf
		auto TubeSponge = [&](const FVector& Foot, int32 Num, double Len, double Rad, const FLinearColor& Col)
		{
			for (int32 k = 0; k < Num; k++)
			{
				const double Az = R2.FRandRange(0, 2 * PI), Ln = Len * R2.FRandRange(.6, 1.0);
				const FVector Dir = (FVector(FMath::Cos(Az), FMath::Sin(Az), 0) * R2.FRandRange(.1, .45) + FVector::UpVector).GetSafeNormal();
				const FVector A = Foot + FVector(FMath::Cos(Az), FMath::Sin(Az), 0) * (Rad * 1.2), B = A + Dir * Ln;
				M.AddTube(A, B, Rad * .75, Rad, 7, Col);
				M.AddTube(B, B - Dir * (Ln * .35), Rad * .8, Rad * .4, 7, Col * .3f);   // the dark inside
			}
		};
		const FLinearColor Lime = Hx(0xb4ae94), LimeDk = Hx(0x8c907e);   // old reef limestone under a thin turf
		for (const FLedge& L : Ledges)
		{
			const FQuat Q(FVector::UpVector, L.Yaw);
			auto OnTop = [&](double Reach)
			{
				const double Az = R2.FRandRange(0, 2 * PI), Rr = FMath::Sqrt(R2.FRand()) * Reach;
				const FVector2D P = L.At + FVector2D(FMath::Cos(Az) * Rr * L.A, FMath::Sin(Az) * Rr * L.B).GetRotated(FMath::RadiansToDegrees(L.Yaw));
				return FVector(P.X, P.Y, L.Top);
			};
			auto AtFoot = [&](double Off)
			{
				const double Az = R2.FRandRange(0, 2 * PI);
				return L.At + FVector2D(FMath::Cos(Az), FMath::Sin(Az)) * (FMath::Max(L.A, L.B) + Off);
			};
			M.AddRoundBox(FVector(L.At.X, L.At.Y, L.Top - (L.H + .8) * .5), FVector(L.A * 2, L.B * 2, L.H + .8), FMath::Min(.5, L.H * .4), Mix(Lime, LimeDk, R2.FRand()), Q, 2);
			// shoulders: lower, tilted blocks leaning on its sides, so the outcrop is not one slab
			for (int32 k = 0; k < (L.bUpper ? 1 : 3); k++)
			{
				const double Az = R2.FRandRange(0, 2 * PI), Hs = L.H * R2.FRandRange(.45, .8);
				const FVector2D P = L.At + FVector2D(FMath::Cos(Az) * L.A * .8, FMath::Sin(Az) * L.B * .8).GetRotated(FMath::RadiansToDegrees(L.Yaw));
				M.AddRoundBox(FVector(P.X, P.Y, L.Top - L.H + Hs * .5 - .3), FVector(L.A * R2.FRandRange(.7, 1.1), L.B * R2.FRandRange(.7, 1.1), Hs + .6), FMath::Min(.45, Hs * .4), Mix(Lime, LimeDk, R2.FRand()),
					FQuat(FVector::UpVector, R2.FRand() * 6) * FQuat(FVector::ForwardVector, R2.FRandRange(-.18, .18)), 2);
			}
			for (int32 k = 0; k < 3; k++)
				M.AddRoundBox(OnTop(.8) + FVector(0, 0, .02), FVector(R2.FRandRange(.4, 1.0), R2.FRandRange(.3, .8), .1), .04, Crust[(k + int32(L.At.X)) & 3], FQuat(FVector::UpVector, R2.FRand() * 6), 1);
			if (R2.FRand() < .4) TubeSponge(OnTop(.7), R2.RandRange(3, 5), R2.FRandRange(.3, .5), .06, Hx(0xa07c52));   // brown tube sponge
			if (L.bUpper)
			{
				if (R2.FRand() < .5) TubeSponge(OnTop(.5), R2.RandRange(2, 3), R2.FRandRange(1.0, 1.6), .085, Hx(0xb4a2c8));   // stove-pipe sponge
				continue;
			}
			// rubble at the foot
			for (int32 k = 0; k < 3; k++)
			{
				const FVector2D P = AtFoot(R2.FRandRange(-.3, .9));
				const double S = R2.FRandRange(.5, 1.1);
				M.AddRoundBox(FVector(P.X, P.Y, Ground(P.X, P.Y) + S * .2), FVector(S * 1.5, S * 1.1, S * .8), S * .3, Mix(Lime, Rock, R2.FRand() * .6), FQuat(FVector::UpVector, R2.FRand() * 6), 1);
			}
			// the large sponges: one to a ledge
			const int32 Kind = R2.RandRange(0, 2);
			if (Kind == 0) Barrel(AtFoot(R2.FRandRange(.9, 1.8)), R2.FRandRange(.8, 1.05), SpC[R2.RandRange(0, 2)]);
			else if (Kind == 1) TubeSponge(OnTop(.8), R2.RandRange(4, 7), R2.FRandRange(.35, .6), .06, Hx(0xe6cc3c));    // yellow tube sponge
			else
			{
				// orange elephant ear: thick lobes spreading over the ledge
				const FVector C0 = OnTop(.8);
				for (int32 k = 0; k < 3; k++)
					M.AddRoundBox(C0 + FVector(R2.FRandRange(-.5, .5), R2.FRandRange(-.5, .5), .06 + .05 * k), FVector(R2.FRandRange(.7, 1.3), R2.FRandRange(.5, .9), .16), .07, Mix(Hx(0xe8802c), Hx(0xc86424), R2.FRand()), FQuat(FVector::UpVector, R2.FRand() * 6) * FQuat(FVector::ForwardVector, R2.FRandRange(-.25, .25)), 1);
			}
		}
	}
	M.Commit(Static, 0, AKeysWorld::VertexColorMaterial());
	UE_LOG(LogTemp, Log, TEXT("Keys Run: Abyssal Garden static - %d tris"), M.T.Num() / 3);
}

// ---------------------------------------------------------------- light: sun shafts, gold specks, the magenta glow
void AKeysGarden::BuildGlow()
{
	UMaterialInterface* GM = AKeysWorld::GlowMaterial();
	if (!GM) return;
	FKeysMesh M;
	FRandomStream R(4401);
	const FVector Sun = FVector(.28, .12, -1).GetSafeNormal();   // the way the light slants down
	const FLinearColor Ray(.75f, .95f, 1.f);
	for (int32 k = 0; k < 34; k++)
	{
		const FVector2D P(R.FRandRange(-SiteR, SiteR), R.FRandRange(-SiteR, SiteR));
		const FVector Top(P.X, P.Y, -.4);
		const double Len = -Ground(P.X, P.Y) / -Sun.Z;
		const FVector Bot = Top + Sun * Len;
		const double W = R.FRandRange(.8, 2.6), Op = R.FRandRange(.025, .06);
		// a soft band: two fans from either edge at the surface, crossed with a second band at right angles, fading down
		for (int32 j = 0; j < 2; j++)
		{
			const FVector Sd = (j == 0 ? FVector(1, 0, 0) : FVector(0, 1, 0)) * W;
			for (int32 f = -1; f <= 1; f += 2)
			{
				M.AddGlowFan(Top + Sd * f, Bot - Sd * 1.3, Bot + Sd * 1.3, Ray, Op, 0);
				M.AddGlowFan(Top + Sd * f, Bot + Sd * 1.3, Bot - Sd * 1.3, Ray, Op, 0);
			}
		}
	}
	// the gold and green specks (plankton catching the light) that drift over the thickets
	for (int32 k = 0; k < 420; k++)
	{
		const FVector2D P(R.FRandRange(-58, 10), R.FRandRange(-52, 52));
		const double G = Ground(P.X, P.Y);
		const FVector C(P.X, P.Y, FMath::Lerp(G + .5, -1.0, R.FRand()));
		M.AddGlowBlob(C, R.FRandRange(.025, .06), R.FRand() < .7 ? FLinearColor(1.f, .85f, .35f, .9f) : FLinearColor(.55f, 1.f, .45f, .8f));
	}
	// the grotto's magenta glow, low over its ground cover
	for (int32 k = 0; k < 40; k++)
	{
		const double A = R.FRand() * 2 * PI, Rr = 9 * FMath::Sqrt(R.FRand());
		const FVector2D P = GlowAt + FVector2D(FMath::Cos(A) * Rr, FMath::Sin(A) * Rr);
		M.AddGlowBlob(FVector(P.X, P.Y, Ground(P.X, P.Y) + .25), R.FRandRange(.25, .6), FLinearColor(1.f, .2f, .75f, .25f));
	}
	GlowMID = UMaterialInstanceDynamic::Create(GM, this);
	GlowMID->SetScalarParameterValue(TEXT("Bright"), 1.f);
	M.Commit(Glow, 0, GlowMID);
}

// ---------------------------------------------------------------- the thickets, swaying
void AKeysGarden::BuildSway(double T)
{
	FKeysMesh M;
	const FVector Flow = FVector(.8, .35, 0).GetSafeNormal(), Cross(-Flow.Y, Flow.X, 0);
	for (const FColony& Cn : Colonies)
	{
		// (the kelp's own motion) a colony bends downstream more the higher it goes, rocking with the surge, with a slower
		// side-to-side wander; how far is its Flex: a sea whip gives a lot, a black coral bush or a sea fan hardly at all
		auto Warp = [&](const FVector& P)
		{
			const double U = FMath::Clamp((P.Z - Cn.Base.Z) / Cn.H, 0.0, 1.3);
			const double Sw = FMath::Sin(T * .55 + Cn.Phase + U * 1.8) * .9 + .6;
			const double Wn = FMath::Sin(T * .31 + Cn.Phase * 1.7 + U * 2.6) * .55;
			const double Bend = U * U * Cn.H * Cn.Flex;
			return P + Flow * (Bend * Sw) + Cross * (Bend * Wn * .5);
		};
		for (int32 i = Cn.Seg0; i < Cn.Seg0 + Cn.SegN; i++)
		{
			const FSeg& S = Segs[i];
			M.AddTube(Warp(S.A), Warp(S.B), S.RA, S.RB, S.Sides, S.C);
		}
		for (int32 i = Cn.Leaf0; i < Cn.Leaf0 + Cn.LeafN; i++)
		{
			const FLeaf& L = Leaves[i];
			const FVector Flut(0, 0, FMath::Sin(T * 2.1 + i * .9 + Cn.Phase) * .012);   // the branchlets' own flutter, at their tips
			const FVector Q0 = Warp(L.P[0]), Q1 = Warp(L.P[1]), Q2 = Warp(L.P[2]) + Flut, Q3 = Warp(L.P[3]) + Flut;
			M.AddQuad(Q0, Q1, Q2, Q3, L.C, FVector::CrossProduct(Q1 - Q0, Q3 - Q0));
		}
	}
	if (!bSwayLogged)
	{
		bSwayLogged = true;
		UE_LOG(LogTemp, Log, TEXT("Keys Run: Abyssal Garden sway - %d tris, rebuilt 30 times a second within 450 m"), M.T.Num() / 3);
	}
	M.Commit(Sway, 0, AKeysWorld::VertexColorMaterial());
}

// ---------------------------------------------------------------- manta rays, gliding laps over the garden
void AKeysGarden::BuildMantas(double T)
{
	FKeysMesh M;
	const FLinearColor Back = Hx(0x252b33), Belly = Hx(0xd8dee2), Mark = Hx(0x3c4550);
	for (int32 i = 0; i < 3; i++)
	{
		const double Rr = 24 + i * 9, Sp = (.09 + .025 * i) * (i % 2 ? -1 : 1);
		const double A = T * Sp + i * 2.1;
		const double Alt = -FMath::Max(3.0, -Ground(0, 0) * (.35 + .1 * i)) + FMath::Sin(T * .2 + i) * 1.2;
		const FVector C(FMath::Cos(A) * Rr + 8, FMath::Sin(A) * Rr, Alt);
		const FVector Fw = FVector(-FMath::Sin(A), FMath::Cos(A), 0) * FMath::Sign(Sp);
		const FVector Rt = FVector::CrossProduct(FVector::UpVector, Fw);
		const double Span = 2.4 + .4 * i, Bank = -FMath::Sign(Sp) * .25;
		const double Ph = T * 1.15 + i * 1.3;
		const int32 NU = 13, NV = 7;
		auto Pt = [&](double S, double Cv, double Face)
		{
			// a diamond: widest chord at the body, the leading edge swept back to the pointed tips
			const double As = FMath::Abs(S);
			const double Lead = .9 * (1 - As * As) + .1 - .55 * As;
			const double Trail = -.85 * (1 - As) + .05 * As - .55 * As;
			const double X = FMath::Lerp(Trail, Lead, Cv);
			const double Flap = FMath::Sin(Ph - As * 1.3) * FMath::Pow(As, 1.6) * .55 * Span;
			const double Thick = .14 * (1 - As) * FMath::Sin(Cv * PI) * Face;
			return C + Fw * (X * Span * .62) + Rt * (S * Span) + FVector(0, 0, Flap + Thick + S * Span * Bank);
		};
		for (int32 Face = -1; Face <= 1; Face += 2)
		{
			TArray<FVector> G;
			for (int32 v = 0; v < NV; v++)
				for (int32 u = 0; u < NU; u++)
					G.Add(Pt(-1 + 2.0 * u / (NU - 1), v / double(NV - 1), Face));
			M.AddGrid(G, NU, NV, false, Face > 0 ? Back : Belly, C - FVector(0, 0, Face * 4.0));
		}
		// cephalic fins and the whip tail
		for (int32 S = -1; S <= 1; S += 2) M.AddTube(C + Fw * Span * .5 + Rt * S * .25, C + Fw * Span * .78 + Rt * S * .3 + FVector(0, 0, -.05), .06, .03, 4, Mark);
		M.AddTube(C - Fw * Span * .45, C - Fw * Span * 1.6 + FVector(0, 0, FMath::Sin(Ph) * .2), .04, .008, 4, Mark);
	}
	M.Commit(Mantas, 0, AKeysWorld::VertexColorMaterial());
}

void AKeysGarden::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	const APlayerCameraManager* Cam = UGameplayStatics::GetPlayerCameraManager(this, 0);
	if (!Cam) return;
	const FVector L = Cam->GetCameraLocation() / 100.0;   // UE meters: x = game x, y = game z
	const double D = FVector2D(L.X - CX, L.Y - CZ).Size();
	if (!bBuilt)
	{
		if (Ground(0, 0) > -2) return;   // the seabed isn't loaded yet
		bBuilt = true;
		BuildAll();
	}
	bNear = D < 450;
	Sway->SetVisibility(D < 900);
	Mantas->SetVisibility(D < 900);
	Glow->SetVisibility(D < 600);
	if (!bNear) return;
	const double T = GetWorld()->GetTimeSeconds();
	SwayAcc += DeltaSeconds;
	if (SwayAcc > 1 / 30.0) { SwayAcc = 0; BuildSway(T); }
	BuildMantas(T);
	if (GlowMID) GlowMID->SetScalarParameterValue(TEXT("Bright"), float(.85 + .15 * FMath::Sin(T * .7) + .08 * FMath::Sin(T * 2.3)));
}
