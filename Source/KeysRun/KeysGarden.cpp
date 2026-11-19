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
// https://github.com/Noniv/abyssal-garden): the scene's ideas - kelp, light rays, the wooden wreck, the stone whale
// fluke, mantas, barrel sponges and the magenta grotto glow - rebuilt here as procedural meshes for this game.

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
	Kelp = Make(TEXT("GardenKelp"));
	Mantas = Make(TEXT("GardenMantas"));
	Glow = Make(TEXT("GardenGlow"));
	Kelp->SetCastShadow(false);
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
	BuildStatic();
	BuildGlow();

	// the kelp: clumps on the west half of the site, lanes left open to swim through, every stalk reaching for the light
	FRandomStream R(7311);
	for (int32 c = 0; c < 26; c++)
	{
		const FVector2D Cc(R.FRandRange(-56, 6), R.FRandRange(-50, 50));
		if (FVector2D::Distance(Cc, FlukeAt) < 9 || Cc.Size() > SiteR) continue;
		const int32 N = R.RandRange(5, 10);
		for (int32 i = 0; i < N; i++)
		{
			const FVector2D P = Cc + FVector2D(R.FRandRange(-4.5, 4.5), R.FRandRange(-4.5, 4.5));
			FStalk S;
			S.Base = FVector(P.X, P.Y, Ground(P.X, P.Y) - .1);
			S.H = FMath::Max(4.0, -S.Base.Z * R.FRandRange(.72, .97));
			S.Phase = R.FRandRange(0, 2 * PI);
			S.Lean = R.FRandRange(-.2, .2);
			S.Hue = R.FRand();
			S.Blades = FMath::Clamp(int32(S.H / .3), 10, 48);
			Stalks.Add(S);
		}
	}
	UE_LOG(LogTemp, Log, TEXT("Keys Run: Abyssal Garden - %d kelp stalks"), Stalks.Num());
	BuildKelp(0);
	BuildMantas(0);
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

	// ---- barrel sponges (purple, orange and tan), open at the top, and purple sea fans
	auto Barrel = [&](const FVector2D& P, double S, const FLinearColor& C)
	{
		const FVector B(P.X, P.Y, Ground(P.X, P.Y) - .1);
		const double Rp[6] = {.45, .55, .62, .7, .78, .82};
		for (int32 k = 0; k < 5; k++) M.AddTube(B + FVector(0, 0, k * .32 * S), B + FVector(0, 0, (k + 1) * .32 * S), Rp[k] * S, Rp[k + 1] * S, 12, Mix(C, C * .7f, (k % 2) * .5));
		M.AddTube(B + FVector(0, 0, 1.6 * S), B + FVector(0, 0, .5 * S), .7 * S, .5 * S, 12, C * .35f);   // the dark inside
	};
	const FLinearColor SpC[3] = {Hx(0x6b4a9a), Hx(0xc77a3a), Hx(0xb9a46a)};
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
	// the gold and green specks that drift in the kelp
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

// ---------------------------------------------------------------- the kelp, swaying
void AKeysGarden::BuildKelp(double T)
{
	FKeysMesh M;
	const FLinearColor Stipe = Hx(0x6e6a30), BladeA = Hx(0x6f8f2c), BladeB = Hx(0xa89c36), Tip = Hx(0xd0b24a);
	const FVector Flow = FVector(.8, .35, 0).GetSafeNormal(), Cross(-Flow.Y, Flow.X, 0);
	for (const FStalk& S : Stalks)
	{
		auto At = [&](double U)
		{
			// the stalk bends downstream more the higher it goes, rocking with the surge, with a slower side-to-side wander
			const double Sw = FMath::Sin(T * .55 + S.Phase + U * 1.8) * .9 + .6;
			const double Wn = FMath::Sin(T * .31 + S.Phase * 1.7 + U * 2.6) * .55;
			const double Bend = U * U;
			return S.Base + FVector(0, 0, S.H * U) + Flow * (Bend * S.H * .16 * Sw + S.Lean * U) + Cross * (Bend * Wn * S.H * .08);
		};
		const int32 Seg = 10;
		FVector Prev = At(0);
		for (int32 k = 1; k <= Seg; k++)
		{
			const FVector P = At(double(k) / Seg);
			M.AddTube(Prev, P, .05, .035, 4, Stipe);
			Prev = P;
		}
		// the blades: alternating sides up the stalk, streaming downstream, thicker toward the top; both faces
		for (int32 b = 0; b < S.Blades; b++)
		{
			const double U = .08 + .92 * double(b) / S.Blades;
			const FVector P = At(U);
			const FVector Up = (At(FMath::Min(1.0, U + .05)) - P).GetSafeNormal();
			const double Side = (b % 2 ? 1 : -1);
			const double Ang = S.Phase + b * 2.4;
			const FVector Out = (FVector(FMath::Cos(Ang), FMath::Sin(Ang), 0) * .6 + Flow * .8).GetSafeNormal();
			const double Len = .75 + .55 * U, Wd = .14 + .08 * U;
			const double Flap = FMath::Sin(T * 2.1 + b * .9 + S.Phase) * .15;
			const FVector Dir = (Out + Up * (.08 + Flap) + Flow * .3 * Side * .2).GetSafeNormal();
			const FVector Wv = FVector::CrossProduct(Dir, Up).GetSafeNormal() * Wd;
			const FVector A0 = P - Wv * .3, A1 = P + Wv * .3, B1 = P + Dir * Len + Wv + FVector(0, 0, -.16), B0 = P + Dir * Len - Wv + FVector(0, 0, -.16);
			const FVector C = P + Dir * Len * 1.45 + FVector(0, 0, -.3);
			const FLinearColor Col = Mix(Mix(BladeA, BladeB, S.Hue), Tip, U * .5);
			const FVector Nn = FVector::CrossProduct(Dir, Wv).GetSafeNormal();
			M.AddQuad(A0, A1, B1, B0, Col, Nn);
			M.AddQuad(A0, B0, B1, A1, Col, -Nn);
			M.AddTri(B0, B1, C, Col, Nn);
			M.AddTri(B0, C, B1, Col, -Nn);
		}
		// the gas floats at the top
		M.AddTube(At(.985), At(1.0) + FVector(0, 0, .12), .07, .05, 5, Tip);
	}
	M.Commit(Kelp, 0, AKeysWorld::VertexColorMaterial());
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
	Kelp->SetVisibility(D < 900);
	Mantas->SetVisibility(D < 900);
	Glow->SetVisibility(D < 600);
	if (!bNear) return;
	const double T = GetWorld()->GetTimeSeconds();
	KelpAcc += DeltaSeconds;
	if (KelpAcc > 1 / 30.0) { KelpAcc = 0; BuildKelp(T); }
	BuildMantas(T);
	if (GlowMID) GlowMID->SetScalarParameterValue(TEXT("Bright"), float(.85 + .15 * FMath::Sin(T * .7) + .08 * FMath::Sin(T * 2.3)));
}
