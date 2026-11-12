#include "KeysTown.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "KeysTownData.h"
#include "KeysGeo.h"
#include "KeysWorld.h"
#include "ProceduralMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Components/PointLightComponent.h"
#include "Materials/MaterialInterface.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Kismet/GameplayStatics.h"
#include "Camera/PlayerCameraManager.h"

using namespace KeysTown;
using KeysTownPaint::Hex;
using KeysTownPaint::Fade;

namespace
{
	constexpr double P = KeysTown::PadY;
	inline double Zf(double V) { return -V; }
	double Hash1(double X) { const double H = FMath::Sin(X * 12.9898) * 43758.5453; return H - FMath::FloorToDouble(H); }
	double Hash2(double X, double Z) { const double H = FMath::Sin(X * 127.1 + Z * 311.7) * 43758.5453; return H - FMath::FloorToDouble(H); }
	FLinearColor Mul(const FLinearColor& C, double K) { return FLinearColor(C.R * K, C.G * K, C.B * K, 1); }

	// the hero sign each building wears on its roof (build.ts HERO)
	struct FHero { EStyle Style; int32 Kind; double Size; };
}

AKeysTown::AKeysTown()
{
	PrimaryActorTick.bCanEverTick = true;
	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent = Root;
	auto Make = [this](const TCHAR* Name, bool bShadow)
	{
		UProceduralMeshComponent* C = CreateDefaultSubobject<UProceduralMeshComponent>(Name);
		C->SetupAttachment(Root);
		C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		C->SetCastShadow(bShadow);
		C->bUseAsyncCooking = true;
		return C;
	};
	Near = Make(TEXT("TownNear"), true);
	Inside = Make(TEXT("TownInside"), true);
	Glow = Make(TEXT("TownGlow"), false);
	Far = Make(TEXT("TownFar"), false);
	People = Make(TEXT("TownPeople"), true);
	Estates = Make(TEXT("TownEstates"), true);
	Tank = Make(TEXT("TownTank"), false);
	Halos = Make(TEXT("TownHalos"), false);
}

AKeysTown* AKeysTown::Get(const UObject* WorldContext)
{
	UWorld* W = WorldContext ? WorldContext->GetWorld() : nullptr;
	if (!W) return nullptr;
	TActorIterator<AKeysTown> It(W);
	return It ? *It : nullptr;
}

void AKeysTown::BeginPlay()
{
	Super::BeginPlay();
	KeysGeo::Init();
	// the town's own frame: origin on the marina's shoreline, +X along u, +Y toward the harbour (-v)
	SetActorLocationAndRotation(KeysGeo::ToUE(OX(), 0, OZ()), FRotator(0, Dir() > 0 ? 0 : 180, 0));
	GlowMat = LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/EngineDebugMaterials/VertexColorMaterial.VertexColorMaterial"));
	if (!GlowMat) GlowMat = AKeysWorld::VertexColorMaterial();

	FTownBatch S, G, I, F;
	BuildGround(S);
	BuildPadEdges(S);
	for (const FLot& L : Lots()) BuildLot(L, S, G);
	BuildProps(S, G);
	BuildCars(S);
	BuildInteriors(I, G);
	BuildCasinoInterior(I, G);
	BuildFar(F);

	UMaterialInterface* VC = AKeysWorld::VertexColorMaterial();
	S.M.Commit(Near, 0, VC);
	I.M.Commit(Inside, 0, VC);
	G.M.Commit(Glow, 0, GlowMat);
	F.M.Commit(Far, 0, VC);
	Far->SetVisibility(false);
	UE_LOG(LogTemp, Log, TEXT("Keys Run: town built - street %d tris, interiors %d, glow %d, far %d, %d signs"), S.Tris(), I.Tris(), G.Tris(), F.Tris(), Signs.Num());

	BuildEstates();

	if (UMaterialInterface* GM = AKeysWorld::GlowMaterial())
	{
		FKeysMesh H;
		for (const FHalo& Hl : HaloList)
		{
			const FVector C(Hl.P.X, Hl.P.Z, Hl.P.Y);   // town batch (x, y up, z) to mesh (x, z, y)
			if (!Hl.bPool) { FLinearColor Cc = Hl.C; Cc.A = Hl.R < 1.2 ? .3f : .55f; H.AddGlowBlob(C, Hl.R, Cc); continue; }
			// a pool of light on the pavement: one flat soft square
			FLinearColor Cc = Hl.C; Cc.A = .35f;
			const int32 I0 = H.AddVert(C + FVector(-Hl.R, -Hl.R, 0), FVector::UpVector, Cc), I1 = H.AddVert(C + FVector(Hl.R, -Hl.R, 0), FVector::UpVector, Cc);
			const int32 I2 = H.AddVert(C + FVector(Hl.R, Hl.R, 0), FVector::UpVector, Cc), I3 = H.AddVert(C + FVector(-Hl.R, Hl.R, 0), FVector::UpVector, Cc);
			H.UV[I0] = FVector2D(2.001, .001); H.UV[I1] = FVector2D(2.999, .001); H.UV[I2] = FVector2D(2.999, .999); H.UV[I3] = FVector2D(2.001, .999);
			H.T.Append({I0, I2, I1, I0, I3, I2});
		}
		HaloMID = UMaterialInstanceDynamic::Create(GM, this);
		H.Commit(Halos, 0, HaloMID);
		Halos->SetVisibility(false);
	}

	// the people: everyone standing at a fixed spot, then the street walkers
	int32 k = 0;
	for (const FNpc& N : Npcs())
	{
		FPerson Pp;
		Pp.Look = N.Look;
		Pp.Inside = N.Inside;
		Pp.U = N.U; Pp.V = N.V; Pp.Y = P + N.Y; Pp.Yaw = N.Yaw;
		Pp.bDancer = N.Look == TEXT("dancer");
		Pp.bSeated = N.Look == TEXT("punter") || (N.Look == TEXT("patron") && N.Inside == TEXT("diner"));
		Pp.H.Init(KeysTownPaint::LookFor(N.Look, N.Variant + k * 7), Pp.bSeated);
		Folk.Add(MoveTemp(Pp));
		k++;
	}
	struct FWalk { const TCHAR* Look; TArray<FVector2D> Pts; double Speed; };
	// the sidewalk crowd is the web game's scripted street life now (AKeysStreet, town-life.ts)
	const TArray<FWalk> Walks;
	for (const FWalk& W : Walks)
	{
		FPerson Pp;
		Pp.Look = W.Look;
		Pp.Path = W.Pts;
		// out and back (closed loop for paths of more than two points)
		if (W.Pts.Num() == 2) Pp.Path.Add(W.Pts[0]);
		else Pp.Path.Add(W.Pts[0]);
		for (int32 i = 1; i < Pp.Path.Num(); i++) Pp.Len += FVector2D::Distance(Pp.Path[i - 1], Pp.Path[i]);
		Pp.Speed = W.Speed;
		Pp.S0 = Hash1(k * 3.7) * Pp.Len;
		Pp.Y = P;
		Pp.H.Init(KeysTownPaint::LookFor(W.Look, k * 5 + 3), false);
		Folk.Add(MoveTemp(Pp));
		k++;
	}
}

// ---------------------------------------------------------------- the ground: roads, sidewalks, lots, promenade planks, markings
void AKeysTown::BuildGround(FTownBatch& S)
{
	const double Y = P + .02;
	for (int32 i = 0; i < Surfaces().Num(); i++)
	{
		const FSurface& Sf = Surfaces()[i];
		const double Lift = .002 * i;   // later surfaces draw on top
		if (Sf.Kind == ESurface::Promenade)
		{
			// real planks, running across the frontage, each a shade different
			const double PW = .3;
			for (double V = Sf.V0; V < Sf.V1; V += PW)
			{
				const double H = Hash1(V * 7.3);
				const FLinearColor C = Mul(Hex(0x9c7a55), .82 + .3 * H);
				S.SpanC(Sf.U0, Sf.U1, P, P + .04 + Lift, Zf(V + PW - .025), Zf(V), C);
			}
			continue;
		}
		const uint32 Base = Sf.Kind == ESurface::Road ? 0x6d7176 : Sf.Kind == ESurface::Sidewalk ? 0xd3cdbf : 0x85827a;
		const double Tile = Sf.Kind == ESurface::Road ? 6 : Sf.Kind == ESurface::Sidewalk ? 3 : 5;
		for (double U = Sf.U0; U < Sf.U1 - .01; U += Tile)
			for (double V = Sf.V0; V < Sf.V1 - .01; V += Tile)
			{
				const double U1 = FMath::Min(U + Tile, Sf.U1), V1 = FMath::Min(V + Tile, Sf.V1);
				const double H = Hash2(U * .37 + i, V * .71);
				const FLinearColor C = Mul(Hex(Base), (Sf.Kind == ESurface::Road ? .93 : .95) + .1 * H);
				const double J = Sf.Kind == ESurface::Sidewalk ? .03 : 0;   // the joints between slabs show as dark lines
				S.Flat(U + J, U1 - J, Zf(V1) + J, Zf(V) - J, Y + Lift, C);
				if (J > 0) S.Flat(U, U1, Zf(V1), Zf(V), Y + Lift - .001, Hex(0x6e6a60));
			}
		// kerbs along road edges
		if (Sf.Kind == ESurface::Sidewalk)
		{
			const FLinearColor Kerb = Hex(0xbdb6a6);
			if (Sf.V1 - Sf.V0 < Sf.U1 - Sf.U0)
			{
				S.SpanC(Sf.U0, Sf.U1, P, Y + .12, Zf(Sf.V0 + .18), Zf(Sf.V0), Kerb);
				S.SpanC(Sf.U0, Sf.U1, P, Y + .12, Zf(Sf.V1), Zf(Sf.V1 - .18), Kerb);
			}
		}
	}
	// road markings: Harbor Drive and Sombrero Boulevard get a double yellow and white edge lines; Palm Avenue a centre dash
	const double MY = P + .045;
	for (double VC : {34.0, 90.0})
	{
		for (double DV : {-.12, .12}) S.Flat(PadU0, PadU1, Zf(VC + DV + .06), Zf(VC + DV - .06), MY, Hex(0xe8c23a));
		for (double DV : {-5.4, 5.4}) S.Flat(PadU0, PadU1, Zf(VC + DV + .07), Zf(VC + DV - .07), MY, Hex(0xe9e9e4));
		// parking bays along the kerbs
		for (double U = PadU0 + 6; U < PadU1 - 6; U += 6.5) for (double DV : {-4.8, 4.8}) S.Flat(U - .06, U + .06, Zf(VC + DV + 1.2), Zf(VC + DV - 1.2), MY, Hex(0xe9e9e4));
	}
	for (double V = 41; V < 95; V += 4) if (V < 82 || V > 98) S.Flat(-.07, .07, Zf(V + 2), Zf(V), MY, Hex(0xe8c23a));
	// zebra crossings where Palm Avenue meets each road, and across Harbor Drive at the ramp
	auto Zebra = [&](double U0, double U1, double V0, double V1, bool bAcrossU)
	{
		if (bAcrossU) for (double U = U0; U < U1; U += 1.0) S.Flat(U, U + .55, Zf(V1), Zf(V0), MY + .002, Hex(0xf2f2ee));
		else for (double V = V0; V < V1; V += 1.0) S.Flat(U0, U1, Zf(V + .55), Zf(V), MY + .002, Hex(0xf2f2ee));
	};
	Zebra(-4.5, 4.5, 28.6, 31.4, true);
	Zebra(-4.5, 4.5, 84.6, 87.4, true);
	Zebra(-10.5, -7.6, 29, 39.5, false);
	Zebra(7.6, 10.5, 29, 39.5, false);
	Zebra(-10.5, -7.6, 85, 95.5, false);
	Zebra(7.6, 10.5, 85, 95.5, false);
	// the back lot's parking stalls
	for (double U = -96; U <= 96; U += 6) if (FMath::Abs(U) > 26) S.Flat(U - .06, U + .06, Zf(137), Zf(131), MY, Hex(0xe9e9e4));
	// grass verges at the pad's back corners and the planted alley in row 3
	S.Flat(-24, 24, Zf(139.8), Zf(126.2), P + .03, Hex(0x5d8f3a));
}

// ---------------------------------------------------------------- the pad: seawall, rails, ramp, banner, hedge
void AKeysTown::BuildPadEdges(FTownBatch& S)
{
	// the fill pad itself and its seawall
	S.Span(PadU0, PadU1, -1.8, P, Zf(PadV1), Zf(PadV0), 0xa9a69d);
	S.Span(PadU0, PadU1, -1.8, P - .05, Zf(PadV0) + .08, Zf(PadV0) + .1, 0x7e8085);
	S.Span(PadU0 - .2, PadU1 + .2, P - .35, P, Zf(PadV0) + .25, Zf(PadV0), 0xcfcbc0);
	// rails along the harbour front (a gap at the ramp), life rings and bollards
	for (double U = PadU0 + 1; U <= PadU1 - 1; U += 4)
	{
		if (FMath::Abs(U) < 3.4) continue;
		S.Cyl(U, P, Zf(PadV0 + .4), .045, .05, 1.05, 0xe8ebee, 6);
	}
	for (double Yr : {.55, 1.0})
	{
		S.Span(PadU0, -2.9, P + Yr, P + Yr + .05, Zf(PadV0 + .42), Zf(PadV0 + .38), 0xe8ebee);
		S.Span(2.9, PadU1, P + Yr, P + Yr + .05, Zf(PadV0 + .42), Zf(PadV0 + .38), 0xe8ebee);
	}
	for (double U = PadU0 + 12; U <= PadU1 - 12; U += 24)
	{
		if (FMath::Abs(U) < 10) continue;
		S.Cyl(U, P + .56, Zf(PadV0 + .55), .3, .3, .08, 0xd9342b, 12, PI / 2);
		S.Cyl(U + 6, P, Zf(PadV0 + 1.6), .15, .18, .5, 0x2f3438, 8);
	}
	// the hedge across the back and down the sides
	S.Span(PadU0, PadU1, P, P + 1.15, Zf(PadV1), Zf(PadV1 - 1.1), 0x2f6b34);
	S.Span(PadU0, PadU0 + 1.1, P, P + 1.15, Zf(PadV1), Zf(PadV0 + 46), 0x2f6b34);
	S.Span(PadU1 - 1.1, PadU1, P, P + 1.15, Zf(PadV1), Zf(PadV0 + 46), 0x2f6b34);
	// the ramp up from the pier
	{
		const double Run = RampV1 - RampV0, Rise = P - DeckTop, Th = FMath::Atan2(Rise, Run), Len = FMath::Sqrt(Run * Run + Rise * Rise);
		const double MidV = (RampV0 + RampV1) / 2, MidY = (DeckTop + P) / 2, Wd = RampU1 - RampU0;
		S.Box(0, MidY - .14, Zf(MidV), Wd, .26, Len, 0x9c7a55, Th);
		for (int32 Sx = -1; Sx <= 1; Sx += 2)
		{
			S.Box(Sx * (Wd / 2 - .05), MidY + .95, Zf(MidV), .07, .07, Len, 0xe8ebee, Th);
			for (double V = RampV0 + .5; V <= RampV1 - .4; V += 2.5)
				S.Cyl(Sx * (Wd / 2 - .05), DeckTop + Rise * ((V - RampV0) / Run), Zf(V), .04, .045, 1.0, 0xe8ebee, 6);
		}
		for (double V : {9.5, 13.0, 16.5}) for (int32 Sx = -1; Sx <= 1; Sx += 2) S.Cyl(Sx * 1.9, -1.2, Zf(V), .17, .2, DeckTop + 1.0, 0x6e5a44, 6);
		// the welcome banner over the head of the ramp
		for (int32 Sx = -1; Sx <= 1; Sx += 2) S.Box(Sx * 3.9, P + 2.2, Zf(PadV0 + 1.2), .18, 4.4, .18, 0x2d3136);
		S.Box(0, P + 3.9, Zf(PadV0 + 1.2), 7.6, 1.4, .08, 0x0d4a63);
		S.Box(0, P + 3.9, Zf(PadV0 + 1.2), 7.75, 1.55, .05, 0xfff6e0);
		AddSign(TEXT("WELCOME TO BOOT KEY HARBOR CITY"), 0, P + 4.12, PadV0 + 1.2 - .06, .42, Hex(0xfff6e0), 0, false, 7.2);
		AddSign(TEXT("Please tie up your boat. Please do not tie up the seagull."), 0, P + 3.6, PadV0 + 1.2 - .06, .2, Hex(0xfff6e0), 0, false, 7.2);
		AddSign(TEXT("WELCOME TO BOOT KEY HARBOR CITY"), 0, P + 4.12, PadV0 + 1.2 + .06, .42, Hex(0xfff6e0), 180, false, 7.2);
	}
}

// ---------------------------------------------------------------- one building (build.ts buildLot)
void AKeysTown::BuildLot(const FLot& L, FTownBatch& S, FTownBatch& G)
{
	const FLook& Lk = LookOf(L.Style);
	const double W = L.U1 - L.U0, CX = (L.U0 + L.U1) / 2, CZ = Zf((L.V0 + L.V1) / 2);
	const double Top = P + L.H, FZ = Zf(L.V0);
	const FLinearColor WallC = L.Style == EStyle::Club || L.Style == EStyle::Casino ? Hex(Lk.Wall) : Fade(Lk.Wall);
	// the walls, with the doorway cut into the front (lintel above it); the front wall is drawn by BuildFacade
	const TArray<FLocalBox> Walls = LotWalls(L);
	for (int32 i = 2; i < Walls.Num(); i++) S.SpanC(Walls[i].U0, Walls[i].U1, P, Top, Zf(Walls[i].V1), Zf(Walls[i].V0), WallC);
	BuildFacade(L, S, G);

	// roof
	if (L.Style == EStyle::Bar)
	{
		S.Span(L.U0 - .3, L.U1 + .3, Top, Top + .25, Zf(L.V1) - .3, Zf(L.V0) + .3, 0x3a2a1c);
		S.Gable(CX, Top + .25, CZ, W + .8, (L.V1 - L.V0) + .6, FMath::Min(2.4, (L.V1 - L.V0) * .4), Lk.Roof, true);
	}
	else
	{
		S.Span(L.U0 - .12, L.U1 + .12, Top, Top + .22, Zf(L.V1) - .12, Zf(L.V0) + .12, Lk.Trim);
		const double PT = Top + .22, PH = .6;
		S.Span(L.U0 - .12, L.U1 + .12, PT, PT + PH, Zf(L.V0) + .12, Zf(L.V0), Lk.Trim);
		S.Span(L.U0 - .12, L.U1 + .12, PT, PT + PH, Zf(L.V1), Zf(L.V1) - .12 + .12, Lk.Trim);
		S.Span(L.U0 - .12, L.U0, PT, PT + PH, Zf(L.V1), Zf(L.V0), Lk.Trim);
		S.Span(L.U1, L.U1 + .12, PT, PT + PH, Zf(L.V1), Zf(L.V0), Lk.Trim);
		S.Span(L.U0, L.U1, Top + .2, Top + .24, Zf(L.V1), Zf(L.V0), Lk.Roof);
		const double Seed = FMath::Abs(FMath::Sin(L.U0 * 3.1 + L.V0));
		S.Box(CX + (Seed - .5) * W * .5, PT + .5, CZ + 1, 1.8, .9, 1.2, 0xb9bec2);
		S.Box(CX + (.5 - Seed) * W * .4, PT + .4, CZ - 1.5, 1.2, .7, 1.0, 0xa8aeb3);
	}

	// awning: stripes sloping out over the sidewalk
	if (Lk.Awn1 && L.Style != EStyle::Bar && L.Style != EStyle::Diner)
	{
		const double Y = P + 2.95;
		const int32 N = FMath::FloorToInt((W - 1) / .9);
		for (int32 i = 0; i < N; i++) S.Box(L.U0 + .5 + .45 + i * .9, Y, FZ + .85, .9, .06, 1.7, i % 2 ? Lk.Awn2 : Lk.Awn1, .26);
		S.Box(CX, Y - .32, FZ + 1.7, W - 1, .18, .05, Lk.Awn1);
	}
	if (L.Style == EStyle::Diner)
		for (int32 i = 0; i < 6; i++) S.Box(L.Door - 2.5 + .5 + i * .83, P + 2.95, FZ + .8, .83, .06, 1.6, i % 2 ? 0xf4f4f2 : 0xc23a3a, .26);

	// the hero sign on the roof: the thing that tells you what the place is from across the harbour
	const double HY = Top + .8;
	const double HZ = FZ - .6;
	switch (L.Style)
	{
		case EStyle::Tackle:
		{
			// a leaping blue marlin on two posts
			for (double DX : {-1.6, 1.6}) S.Box(CX + DX, Top + .6, HZ, .12, 1.4, .12, 0x2d3136);
			const double Bx = CX, By = HY + 2.2;
			for (int32 i = 0; i < 10; i++)
			{
				const double T = i / 9.0, A = -.5 + T * 1.0;
				const double Px = Bx - 2.8 + T * 5.6, Py = By + FMath::Sin(T * PI) * 1.4;
				const double Th = .75 - FMath::Abs(T - .4) * .9;
				S.Box(Px, Py, HZ, .7, FMath::Max(.2, Th), .3, i < 9 ? 0x1f6fb0 : 0xdfe6ea, 0, 0, A * .8);
			}
			S.Box(Bx + 3.2, By - .3, HZ, 1.6, .1, .1, 0x1f6fb0, 0, 0, -.6);                 // bill
			S.Box(Bx - .5, By + 1.6, HZ, 1.4, 1.0, .08, 0x134a7a, 0, 0, .2);                // sail
			S.Box(Bx - 3.0, By - .1, HZ, .14, 1.4, .8, 0x134a7a, 0, 0, .4);                 // tail
			break;
		}
		case EStyle::Club:
		{
			// the pink flamingo: an egg of a body, an S of a neck, a beak, one leg
			const double Bx = CX + 6, By = HY + 2.6;
			S.Box(Bx, Top + .5, HZ, .1, 1.2, .1, 0x2d3136);
			S.Ball(Bx, By, HZ, 1.1, 0xff6fb5, .7, 10);
			S.Box(Bx + .9, By + .3, HZ, .9, .35, .3, 0xff5fa8, 0, 0, .6);                    // tail feathers
			const FVector N0(Bx - .7, By + .4, HZ), N1(Bx - 1.4, By + 1.4, HZ), N2(Bx - .9, By + 2.3, HZ), N3(Bx - 1.3, By + 2.9, HZ);
			S.Tube3(N0, N1, .2, .16, 6, 0xff6fb5); S.Tube3(N1, N2, .16, .14, 6, 0xff6fb5); S.Tube3(N2, N3, .14, .14, 6, 0xff6fb5);
			S.Ball(N3.X, N3.Y, HZ, .3, 0xff6fb5, 1, 8);
			S.Box(N3.X - .45, N3.Y - .1, HZ, .5, .14, .14, 0x15101a, 0, 0, .5);
			S.Tube3(FVector(Bx + .1, By - .6, HZ), FVector(Bx + .1, Top + .3, HZ), .05, .05, 5, 0xff3fa4);
			S.Tube3(FVector(Bx + .1, By - 1.2, HZ), FVector(Bx + .7, By - 1.6, HZ), .05, .05, 5, 0xff3fa4);
			G.Ball(Bx, By, HZ + .02, 1.18, 0xff3fa4, .72, 10);
			break;
		}
		case EStyle::Guns:
		{
			// a target: rings of red and white
			for (double DX : {-1.2, 1.2}) S.Box(CX + DX, Top + .5, HZ, .12, 1.0, .12, 0x2d3136);
			const double Ty = HY + 2.0;
			for (int32 r = 0; r < 4; r++) { const double Hh = .1 + .05 * r; S.Cyl(CX, Ty - Hh / 2, HZ + Hh / 2, 2.0 - r * .5, 2.0 - r * .5, Hh, r % 2 ? 0xf2ece0 : 0xc32a2a, 20, PI / 2); }
			break;
		}
		case EStyle::Bar:
		{
			// a beer mug over the door, foam and all
			const double Bx = L.U0 + 4.2, By = P + L.H + 1.0;
			S.Box(Bx, By - .4, HZ + .3, .12, 1.6, .12, 0x2d3136);
			S.Cyl(Bx, By + .2, HZ, .9, .9, 2.0, 0xe0a030, 14);
			S.Ball(Bx, By + 2.25, HZ, 1.0, 0xfff8e8, .45, 10);
			S.Cyl(Bx + 1.1, By + 1.2, HZ, .5, .5, .25, 0xd8d0b0, 10, PI / 2);
			break;
		}
		case EStyle::Diner:
		{
			S.Box(CX, HY + 1.0, HZ, 9.5, 2.0, .3, 0x1d2430);
			S.Box(CX, HY + 1.0, HZ - .02, 9.8, 2.3, .26, 0xe8edf1);
			for (double DX : {-3.0, 3.0}) S.Box(CX + DX, Top + .5, HZ, .12, 1.0, .12, 0x2d3136);
			AddSign(TEXT("DINER"), CX, HY + 1.0, L.V0 + .6 - .2, 1.3, Hex(0xff5a4f), 0, false, 9);
			break;
		}
		case EStyle::IceCream:
		{
			S.Cyl(CX, P + L.H + .9, FZ - 2.2, 1.6, .05, 3.4, 0xd9a85c, 10);
			S.Ball(CX, P + L.H + 4.7, FZ - 2.2, 1.7, 0xf7a7c6, 1, 10);
			S.Ball(CX + .1, P + L.H + 6.1, FZ - 2.2, 1.2, 0xfff3e0, 1, 10);
			S.Ball(CX + .1, P + L.H + 7.2, FZ - 2.2, .28, 0xc0172b, 1, 6);
			break;
		}
		default: break;
	}

	// venue set dressing (build.ts)
	const double D = L.Door;
	switch (L.Style)
	{
		case EStyle::Club:
		{
			S.Span(D - 1.2, D + 1.2, P, P + .035, FZ + .1, FZ + 5.6, 0x9b1b30);
			for (double DX : {-1.9, 1.9}) { S.Cyl(D + DX, P, FZ + 2.3, .04, .07, .9, 0xe0b34a, 8); S.Ball(D + DX, P + .95, FZ + 2.3, .07, 0xe0b34a, 1, 6); }
			S.Span(D - 1.9, D - .45, P + .74, P + .8, FZ + 2.28, FZ + 2.32, 0x9b1b30);
			S.Span(D + .45, D + 1.9, P + .74, P + .8, FZ + 2.28, FZ + 2.32, 0x9b1b30);
			for (double DX : {-1.9, 1.9}) G.Ball(D + DX, P + 2.7, FZ + .2, .25, 0xff3fa4, 1, 8);
			break;
		}
		case EStyle::Casino:
		{
			const uint32 Gold = 0xe0b34a, Cyan = 0x35e0ff, Pink = 0xff3fa4;
			S.Span(D - 2.0, D + 2.0, P, P + .035, FZ + .1, FZ + 6.2, Gold);
			S.Span(D - 1.7, D + 1.7, P + .035, P + .05, FZ + .1, FZ + 6.2, 0x0e6d78);
			for (double DX : {-2.3, 2.3})
				for (int32 k = 0; k < 3; k++)
				{
					S.Cyl(D + DX, P, FZ + 1.0 + k * 2.4, .04, .07, .9, Gold, 8);
					S.Ball(D + DX, P + .95, FZ + 1.0 + k * 2.4, .07, Gold, 1, 6);
					if (k < 2) S.Span(D + DX - .015, D + DX + .015, P + .7, P + .76, FZ + 1.0 + k * 2.4, FZ + 3.4 + k * 2.4 - .02, 0x0e6d78);
				}
			const double Cy = P + 3.05;
			S.Span(D - 4.2, D + 4.2, Cy, Cy + .3, FZ, FZ + 3.2, 0x0b1a22);
			S.Span(D - 4.3, D + 4.3, Cy + .3, Cy + .42, FZ - .05, FZ + 3.3, Gold);
			for (int32 i = 0; i < 9; i++) G.Ball(D - 4.0 + i, Cy - .03, FZ + 3.15, .09, 0xfff3b0, 1, 5);
			for (double DX : {-5.2, 5.2})
			{
				S.Cyl(D + DX, P, FZ + 1.4, .55, .7, .85, 0x0b3340, 10);
				S.Cyl(D + DX, P + .85, FZ + 1.4, .45, .5, .06, 0x3b2a1c, 10);
				for (int32 k = 0; k < 6; k++)
				{
					const double A = k / 6.0 * 2 * PI;
					S.Box(D + DX + FMath::Sin(A) * .5, P + 1.5, FZ + 1.4 + FMath::Cos(A) * .5, .25, 1.2, .08, 0x3f9a4a, 0, A, FMath::Sin(A) * .25);
				}
			}
			for (double U = L.U0 + 1.5; U <= L.U1 - 1.5; U += 3) G.Ball(U, Top + .62, FZ - .06, .12, FMath::Fmod(U, 6.0) < 3 ? Cyan : Pink, 1, 5);
			for (double UX : {L.U0 + .9, L.U1 - .9})
			{
				S.Span(UX - .12, UX + .12, P, Top + .6, FZ, FZ + .5, 0x0b1a22);
				for (double Yy = 1.2; Yy < L.H; Yy += 1.6) G.Ball(UX, P + Yy, FZ + .55, .1, FMath::Fmod(Yy, 3.2) < 1.6 ? Gold : Cyan, 1, 5);
			}
			// the casino's own marlin, leaping over the sign band, gold
			const double By = Top + 3.6;
			for (double DX : {-2.5, 2.5}) S.Box(CX + DX, Top + 1.0, HZ, .14, 2.0, .14, 0x2d3136);
			for (int32 i = 0; i < 12; i++)
			{
				const double T = i / 11.0, A = -.6 + T * 1.2;
				const double Px = CX - 4.2 + T * 8.4, Py = By + FMath::Sin(T * PI) * 2.0;
				const double Th = 1.1 - FMath::Abs(T - .4) * 1.3;
				S.Box(Px, Py, HZ, 1.0, FMath::Max(.3, Th), .4, i < 11 ? 0x1f6fb0 : 0xe0b34a, 0, 0, A * .8);
			}
			S.Box(CX + 4.9, By - .4, HZ, 2.4, .14, .14, Gold, 0, 0, -.6);
			S.Box(CX - .6, By + 2.3, HZ, 2.0, 1.4, .1, Gold, 0, 0, .2);
			S.Box(CX - 4.6, By - .2, HZ, .2, 2.0, 1.2, Gold, 0, 0, .4);
			break;
		}
		case EStyle::Tackle:
		{
			for (int32 i = 0; i < 3; i++) S.Span(-56.2 + (i % 2) * .6, -54 + (i % 2) * .6, P + i * .55, P + .5 + i * .55, Zf(42.9), Zf(41.5), 0x6b5a3a);
			const uint32 BU[] = {0xe4572e, 0xf2c14e, 0xffffff, 0x2bb3a8};
			for (int32 i = 0; i < 9; i++) S.Ball(L.U0 + 2 + i * 1.1, P + 2.45 - (i % 3) * .1, FZ + .55, .22, BU[i % 4], 1.15, 8);
			for (int32 i = 0; i < 7; i++) S.Cyl(L.U1 - 1.2 - i * .28, P, FZ + .45, .025, .025, 2.6 - (i % 2) * .25, 0x23262a, 5, 0, 0, .1);
			break;
		}
		case EStyle::Guns:
		{
			const double PX = L.U1 - 1.0, PZ = FZ + 1.4;
			S.Cyl(PX, P, PZ, .05, .08, 7.5, 0xd0d4d8, 6);
			for (int32 i = 0; i < 7; i++) S.Box(PX - 1.0, P + 7.2 - i * .2, PZ, 1.9, .2, .03, i % 2 ? 0xf4f4f4 : 0xb22234);
			S.Box(PX - 1.45, P + 6.8 + .2, PZ + .02, .8, .8, .03, 0x3c3b6e);
			for (double DX : {-3.4, 3.4}) S.Cyl(D + DX, P, FZ + .9, .17, .17, .85, 0x3a3d40, 8);
			break;
		}
		case EStyle::Pawn:
		{
			S.Box(L.U1 - 1.4, P + 3.2, FZ + .9, .08, .08, 1.6, 0x2d3136);
			for (int32 i = 0; i < 3; i++) S.Ball(L.U1 - 1.4, P + 2.9 - (i == 1 ? .5 : 0), FZ + .9 + (i - 1) * .5, .24, 0xe0b34a, 1, 8);
			break;
		}
		case EStyle::Dive:
		{
			const double PX = L.U1 - 1.3;
			S.Cyl(PX, P, FZ + 1.2, .05, .07, 3.8, 0xd0d4d8, 6);
			S.Box(PX + .5, P + 3.4, FZ + 1.2, 1.0, .7, .03, 0xd9342b);
			S.Box(PX + .5, P + 3.4, FZ + 1.22, 1.0, .1, .03, 0xffffff, 0, 0, .6);
			const uint32 TC[] = {0xf2c14e, 0x2bb3a8, 0xf2c14e, 0xdfe6ea};
			for (int32 i = 0; i < 4; i++) { S.Cyl(L.U0 + 1.0 + i * .5, P, FZ + .4, .16, .16, 1.2, TC[i], 8, 0, 0, .05); S.Ball(L.U0 + 1.0 + i * .5, P + 1.25, FZ + .4, .15, 0xc0c4c8, 1, 6); }
			break;
		}
		case EStyle::Bar:
		{
			for (int32 i = 0; i < 20; i++)
			{
				const double Bx = L.U0 + 1 + i * (W - 2) / 19, By = P + L.H - .25 - FMath::Sin(i / 19.0 * PI * 3) * .1;
				G.Ball(Bx, By, FZ + .5, .08, 0xfff0b0, 1, 5);
			}
			S.Box(L.U0 + 1.0, P + .5, FZ + 1.6, 1.2, .08, .8, 0x6b4a2b);
			break;
		}
		case EStyle::Liquor: case EStyle::Convenience:
		{
			S.Span(L.U0 + .7, L.U0 + 2.3, P, P + .9, FZ + .2, FZ + .8, 0xe8edf1);
			S.Span(L.U0 + .7, L.U0 + 2.3, P + .9, P + .95, FZ + .18, FZ + .82, 0x2b6cb0);
			break;
		}
		case EStyle::Diner:
			S.Span(L.U0 - .05, L.U1 + .05, P, P + .5, FZ, FZ + .12, 0xc23a3a);
			break;
		case EStyle::Motel:
		{
			// two floors of rooms: a walkway with a rail along the upper floor, numbered doors
			S.Span(L.U0 + 24, L.U1 - .2, P + 3.6, P + 3.8, FZ, FZ + 1.6, 0xefe3c8);
			S.Span(L.U0 + 24, L.U1 - .2, P + 4.7, P + 4.78, FZ + 1.5, FZ + 1.58, 0x2bb3a8);
			for (double U = L.U0 + 24.5; U < L.U1; U += 3) S.Cyl(U, P, FZ + 1.5, .06, .06, 3.6, 0x2bb3a8, 6);
			break;
		}
		default: break;
	}
}

// ---------------------------------------------------------------- a storefront (paint.ts layoutFor, as geometry)
void AKeysTown::BuildFacade(const FLot& L, FTownBatch& S, FTownBatch& G)
{
	const FLook& Lk = LookOf(L.Style);
	const FVenue* Vn = Venue(L.Venue);
	const double W = L.U1 - L.U0, H = L.H, FZ = Zf(L.V0), Top = P + H;
	const FLinearColor WallC = L.Style == EStyle::Club || L.Style == EStyle::Casino ? Hex(Lk.Wall) : Fade(Lk.Wall);
	const double DoorH = L.Style == EStyle::Casino ? 3.0 : 2.6;
	const double DL = L.Door - DoorW / 2, DR = L.Door + DoorW / 2;
	// the front wall around the doorway
	S.SpanC(L.U0, DL, P, Top, Zf(L.V0 + WallT), FZ, WallC);
	S.SpanC(DR, L.U1, P, Top, Zf(L.V0 + WallT), FZ, WallC);
	S.SpanC(DL, DR, P + DoorH, Top, Zf(L.V0 + WallT), FZ, WallC);
	// pattern on the face: brick courses, plank lines, chrome ribs, or a darker base course on stucco (never across the doorway)
	const double F = FZ + .012;
	auto Face = [&](double A, double B, double Y0, double Y1, double Zz, const FLinearColor& C)
	{
		if (Y0 >= P + DoorH || B <= DL || A >= DR) { S.Facade(A, B, Y0, Y1, Zz, true, C); return; }
		if (A < DL) S.Facade(A, DL, Y0, Y1, Zz, true, C);
		if (B > DR) S.Facade(DR, B, Y0, Y1, Zz, true, C);
		if (Y1 > P + DoorH) S.Facade(FMath::Max(A, DL), FMath::Min(B, DR), P + DoorH, Y1, Zz, true, C);
	};
	if (Lk.Pattern == 1)
		for (double Y = P + .3; Y < Top; Y += .3) Face(L.U0, L.U1, Y, Y + .025, F, Mul(WallC, .7));
	else if (Lk.Pattern == 2)
		for (double Y = P + .25; Y < Top; Y += .25) Face(L.U0, L.U1, Y, Y + .03, F, Mul(WallC, .65));
	else if (Lk.Pattern == 3)
	{
		for (double U = L.U0 + .2; U < L.U1; U += .35) Face(U, U + .04, P + .6, Top - .2, F, Hex(0xf4f6f8));
		for (double U = L.U0; U < L.U1 - .01; U += .3)
			for (int32 r = 0; r < 2; r++) Face(U, FMath::Min(U + .3, L.U1), P + r * .3, P + (r + 1) * .3, F + .004, Hex(((int32((U - L.U0) / .3) + r) % 2) ? 0x22262c : 0xf4f4f2));
	}
	else if (Lk.Pattern == 0)
		Face(L.U0, L.U1, P, P + .35, F, Mul(WallC, .8));

	// door frame and a dark doorway reveal (it is open: you walk in)
	const FLinearColor TrimC = Hex(Lk.Trim);
	S.SpanC(DL - .12, DL, P, P + DoorH + .12, FZ + .06, FZ - WallT, TrimC);
	S.SpanC(DR, DR + .12, P, P + DoorH + .12, FZ + .06, FZ - WallT, TrimC);
	S.SpanC(DL - .12, DR + .12, P + DoorH, P + DoorH + .14, FZ + .06, FZ - WallT, TrimC);

	// windows (paint.ts: store fronts, barred, small, diner bands)
	if (Lk.Windows != 2)
	{
		const bool bSmall = Lk.Windows == 4;
		const double PaneH = bSmall ? 1.3 : 2.05, Y0 = bSmall ? 1.2 : .8;
		const double Dx = L.Door - L.U0 - DoorW / 2;
		const double Spans[2][2] = {{.7, Dx - .5}, {Dx + DoorW + .5, W - .7}};
		for (const auto& Sp : Spans)
		{
			const double Span = Sp[1] - Sp[0];
			if (Span < 1.4) continue;
			const int32 N = FMath::Max(1, FMath::RoundToInt(Span / (bSmall ? 2.4 : 3.6)));
			const double PW = (Span - (N - 1) * .5) / N;
			for (int32 i = 0; i < N; i++)
			{
				const double A = L.U0 + Sp[0] + i * (PW + .5), B = A + PW, Y = P + Y0;
				// frame, glass (a lighter reflection streak), sill
				S.SpanC(A - .08, B + .08, Y - .08, Y + PaneH + .08, F + .03, F, TrimC);
				S.Facade(A, B, Y, Y + PaneH, F + .035, true, Hex(0x27414f));
				S.Facade(A + PW * .12, A + PW * .3, Y + .1, Y + PaneH - .1, F + .04, true, Hex(0x4c6a7d));
				// inside, something on display, and the warm light inside at night
				if (Lk.Windows == 1) for (double U = A + .15; U < B; U += .22) S.SpanC(U, U + .035, Y, Y + PaneH, F + .07, F + .04, Hex(0x2d3136));
				if (Lk.Windows == 3) S.Facade(A, B, Y + PaneH * .45, Y + PaneH * .52, F + .045, true, Hex(0xd7dde2));
				S.SpanC(A - .12, B + .12, Y - .14, Y - .06, F + .12, F, Hex(0xe8e4d8));
			}
		}
	}

	// the sign band: background, rim, the venue's name and its small print
	double BX, BY, BW, BH;
	const bool bTall = H > 5.5;
	if (L.Style == EStyle::Club) { BX = .8; BY = 3.3; BW = W - 1.6; BH = H - 3.3 - .5; }
	else if (L.Style == EStyle::Casino) { BX = 1.2; BY = 3.5; BW = W - 2.4; BH = H - 3.5 - .5; }
	else if (bTall) { BX = 1.2; BY = 3.2; BW = W - 2.4; BH = 1.7; }
	else { BX = .9; BY = FMath::Max(3.05, H - 1.55); BW = W - 1.8; BH = FMath::Min(1.35, H - 3.05 - .2); }
	const double SA = L.U0 + BX, SB = SA + BW, SY = P + BY;
	S.SpanC(SA - .08, SB + .08, SY - .08, SY + BH + .08, F + .1, F, Hex(Lk.SignRim));
	S.SpanC(SA, SB, SY, SY + BH, F + .13, F, Hex(Lk.SignBg));
	// a neon tube along the rim lights up at night
	G.Facade(SA - .05, SB + .05, SY + BH + .02, SY + BH + .07, F + .14, true, Hex(Lk.Neon));
	G.Facade(SA - .05, SB + .05, SY - .07, SY - .02, F + .14, true, Hex(Lk.Neon));
	// overlapping soft blobs along each tube read as one continuous glow
	for (double U = SA + .4; U <= SB - .39; U += FMath::Max(.5, (SB - SA - .8) / FMath::Max(1.0, FMath::RoundToDouble((SB - SA - .8) / .7))))
		for (double Yh : {SY + BH + .045, SY - .045}) HaloList.Add({FVector(U, Yh, -(F + .3)), .9, Hex(Lk.Neon), false});
	if (Vn)
	{
		FString Name = FString(Vn->Name).ToUpper();
		const bool bTag = Vn->Tagline && FCString::Strlen(Vn->Tagline) > 0 && BH > 1.0;
		const double NameH = bTag ? BH * .5 : BH * .7;
		AddSign(Name, (SA + SB) / 2, SY + (bTag ? BH * .62 : BH * .5), -(F + .14), NameH, Hex(Lk.SignFg), 0, false, BW - .3);
		if (bTag) AddSign(Vn->Tagline, (SA + SB) / 2, SY + BH * .2, -(F + .14), BH * .22, Hex(Lk.SignFg), 0, false, BW - .3);
	}
	if (L.Style == EStyle::Casino)
		AddSign(TEXT("CASINO"), (SA + SB) / 2, SY + BH * .2, -(F + .15), BH * .3, Hex(0x35e0ff), 0, false, BW * .5);
	// the open sign by the door, lit day and night
	if (L.Style != EStyle::Club && L.Style != EStyle::Casino)
	{
		const double OU = DR + .9;
		if (OU + .5 < L.U1)
		{
			S.SpanC(OU - .45, OU + .45, P + 1.75, P + 2.1, F + .06, F + .03, Hex(0x15171a));
			G.Facade(OU - .38, OU + .38, P + 1.8, P + 2.05, F + .065, true, Hex(0xff3b30));
		}
	}
}

// ---------------------------------------------------------------- a TextRender sign in the town's local frame
void AKeysTown::AddSign(const FString& Text, double U, double Y, double V, double Size, const FLinearColor& Color, double YawDeg, bool bInterior, double Width)
{
	UTextRenderComponent* T = NewObject<UTextRenderComponent>(this);
	T->SetupAttachment(Root);
	T->RegisterComponent();
	// TextRender faces +X; the town's fronts face +Y (toward the harbour), so turn it a quarter round
	T->SetRelativeLocationAndRotation(FVector(U * 100, -V * 100, Y * 100), FRotator(0, 90 + YawDeg, 0));
	double Sz = Size;
	if (Width > 0) Sz = FMath::Min(Sz, Width / FMath::Max(1.0, Text.Len() * .58));
	T->SetWorldSize(float(Sz * 100));
	T->SetHorizontalAlignment(EHTA_Center);
	T->SetVerticalAlignment(EVRTA_TextCenter);
	T->SetTextRenderColor(Color.ToFColor(true));
	T->SetText(FText::FromString(Text));
	T->SetCastShadow(false);
	(bInterior ? InteriorSigns : Signs).Add(T);
}

// ---------------------------------------------------------------- palms, lamps, benches, bins, the marlin statue, dumpsters, the motel sign
void AKeysTown::BuildProps(FTownBatch& S, FTownBatch& G)
{
	const uint32 Frond[] = {0x4f9a3c, 0x62b04a, 0x73c15a, 0x58a443};
	for (const FPalm& Pm : Palms())
	{
		const double X = Pm.U, Z = Zf(Pm.V);
		S.Span(X - .9, X + .9, P, P + .45, Z - .9, Z + .9, 0xa49c8c);
		S.Span(X - .75, X + .75, P + .45, P + .5, Z - .75, Z + .75, 0x3b2a1c);
		// town-life.ts palmTrunkAt: a curved trunk that leans
		const double Seed = FMath::Fmod(FMath::Abs(FMath::Sin(Pm.U * 12.9898 + Pm.V * 78.233) * 43758.5453), 1.0);
		const double Lean = (Seed - .5) * .9, Ht = 6.4 * Pm.H, BaseY = P + .5;
		auto At = [&](double Fr) { return FVector(Pm.U + Lean * 1.65 * Fr * Fr, BaseY + Ht * Fr, -(Pm.V - Lean * .75 * Fr * Fr)); };
		auto Rad = [](double Fr) { return .3 - .14 * Fr; };
		for (int32 i = 0; i < 6; i++) S.Tube3(At(i / 6.0), At((i + 1) / 6.0), Rad(i / 6.0), Rad((i + 1) / 6.0), 7, i % 2 ? 0xaa9577 : 0x8c7758);
		const FVector T = At(1);
		for (int32 k = 0; k < 11; k++)
		{
			const double Ang = k / 11.0 * 2 * PI + Seed * 3, Len = 3.2 + ((k * 7) % 3) * .35, Seg = Len / 3;
			FVector Pp = T - FVector(0, .02, 0);
			const uint32 Col = Frond[(k + int32(Seed * 4)) % 4];
			const double Pitches[3] = {.5, .05, -.55};
			const double Widths[3] = {.95, .8, .55};
			for (int32 j = 0; j < 3; j++)
			{
				const double Pitch = Pitches[j] - (k % 3) * .08;
				const FVector Dd(FMath::Sin(Ang) * FMath::Cos(Pitch), FMath::Sin(Pitch), FMath::Cos(Ang) * FMath::Cos(Pitch));
				const FVector C = Pp + Dd * Seg / 2;
				S.Box(C.X, C.Y, C.Z, Widths[j], .05, Seg * 1.04, j == 2 && k % 2 ? 0x8fa347 : Col, -Pitch, Ang);
				Pp += Dd * Seg;
			}
		}
		S.Ball(T.X - .1, T.Y - .22, T.Z + .1, .17, 0x5a4a2a, 1, 6);
		S.Ball(T.X + .15, T.Y - .26, T.Z - .08, .15, 0x6a5a30, 1, 6);
	}
	for (const FPoint& Lp : Lamps())
	{
		const double X = Lp.U, Z = Zf(Lp.V);
		S.Cyl(X, P, Z, .07, .11, 4.4, 0x4b5158, 6);
		S.Cyl(X, P, Z, .22, .26, .4, 0x4b5158, 8);
		S.Box(X, P + 4.45, Z, .7, .1, .22, 0x4b5158);
		G.Box(X, P + 4.34, Z, .5, .1, .16, 0xffe2a8);
		HaloList.Add({FVector(X, P + 4.3, Z), 1.5, FLinearColor(1.f, .86f, .6f), false});
		HaloList.Add({FVector(X, P + .06, Z), 3.6, FLinearColor(1.f, .8f, .5f), true});
	}
	for (const FPoint& B : Benches())
	{
		const double X = B.U, Z = Zf(B.V);
		S.Box(X, P + .45, Z, 1.8, .07, .45, 0x8a5a36);
		S.Box(X, P + .8, Z - .2, 1.8, .4, .07, 0x8a5a36);
		for (double DX : {-.8, .8}) S.Box(X + DX, P + .22, Z, .08, .44, .4, 0x2d3136);
	}
	for (const FPoint& C : TrashCans()) { S.Cyl(C.U, P, Zf(C.V), .3, .26, .9, 0x5d7364, 8); S.Cyl(C.U, P + .9, Zf(C.V), .32, .32, .06, 0x3a4044, 8); }
	// the blue marlin on its plinth (props.ts marlinStatue)
	{
		const double X = 16, Z = Zf(24);
		S.Span(X - 1.3, X + 1.3, P, P + .9, Z - 1.3, Z + 1.3, 0xb7b2a6);
		S.Span(X - 1.1, X + 1.1, P + .9, P + 1.1, Z - 1.1, Z + 1.1, 0x8d8a82);
		S.Box(X, P + .6, Z + 1.32, .9, .35, .04, 0xc9a24a);
		for (int32 i = 0; i < 9; i++)
		{
			const double T = i / 8.0, Ang = -.2 + T * 1.9;
			const double Px = X - 1.1 + T * 2.0, Py = P + 1.7 + FMath::Sin(T * PI * .85) * 3.6 + T * .8, Th = .55 - T * .3;
			S.Box(Px, Py, Z, .9, Th, Th, i < 8 ? 0x1f6fb0 : 0xdfe6ea, 0, 0, Ang * .7);
		}
		S.Box(X + 1.1, P + 5.6, Z, 1.3, .1, .1, 0x1f6fb0, 0, 0, .8);
		S.Box(X - .2, P + 4.4, Z, .1, 1.2, .5, 0x134a7a);
		S.Box(X - 1.2, P + 1.6, Z, .12, 1.1, .7, 0x134a7a, 0, 0, .5);
	}
	for (double U : {-24.0, 30.0})
	{
		const double V = 77.4;
		S.Span(U - 1.6, U + 1.6, P, P + 1.25, Zf(V) - .8, Zf(V) + .8, 0x2f6b3a);
		S.Span(U - 1.65, U + 1.65, P + 1.25, P + 1.35, Zf(V) - .85, Zf(V) + .85, 0x23282a);
	}
	// crab traps stacked by the tackle shop
	// the motel's pole sign, 9 m up on the boulevard
	{
		S.Cyl(40, P, Zf(98.5), .16, .2, 7.2, 0x2d3136, 8);
		S.Box(40, P + 9.1, Zf(98.5), 6.2, 3.8, .3, 0x2bb3a8);
		S.Box(40, P + 9.1, Zf(98.5) - .02, 6.5, 4.1, .26, 0xfff6e0);
		AddSign(TEXT("MOTEL"), 40, P + 9.8, 98.5 - .17, 1.3, Hex(0xfff6e0), 0, false, 5.6);
		AddSign(TEXT("VACANCY"), 40, P + 8.4, 98.5 - .17, .8, Hex(0xff6a3a), 0, false, 5.6);
		G.Facade(37.4, 42.6, P + 8.0, P + 8.05, Zf(98.5) + .2, true, Hex(0xff6a3a));
	}
	// AC units and pipes on the backs of the buildings, and the back lane's clutter (walls.ts, simplified)
	for (const FLot& L : Lots())
	{
		if (L.Style == EStyle::Casino) continue;
		const double BZ = Zf(L.V1);
		for (double U = L.U0 + 3; U < L.U1 - 2; U += 9)
		{
			S.Span(U, U + 1.1, P, P + .9, BZ - .9, BZ, 0xb9bec2);
			S.Span(U + .1, U + 1.0, P + .9, P + .92, BZ - .8, BZ - .1, 0x6f7378);
		}
		S.Span(L.U1 - 2.4, L.U1 - 1.4, P, P + 2.1, BZ - .04, BZ, 0x5a5048);   // a back door
	}
	// fishing-town clutter on the promenade: bollards, a fish-cleaning table, coolers
	S.Span(-14, -11, P, P + .9, Zf(21.2), Zf(20.2), 0xe8edf1);
	S.Span(-14.1, -10.9, P + .9, P + .95, Zf(21.3), Zf(20.1), 0x2b6cb0);
	for (int32 i = 0; i < 3; i++) S.Span(-18 + i * .9, -17.3 + i * .9, P, P + .5, Zf(20.6), Zf(20.1), i % 2 ? 0xd9342b : 0xf2f2ee);
}

// ---------------------------------------------------------------- parked cars (props.ts car)
void AKeysTown::BuildCars(FTownBatch& S)
{
	for (const FCar& C : Cars())
	{
		const double Cs = FMath::Cos(C.Rot), Sn = FMath::Sin(C.Rot);
		const double CXc = C.U, CZc = Zf(C.V);
		auto To = [&](double LX, double LZ, double& OX_, double& OZ_) { OX_ = CXc + LX * Cs + LZ * Sn; OZ_ = CZc - LX * Sn + LZ * Cs; };
		auto Bx = [&](double LX, double LY, double LZ, double W, double H, double D, uint32 Col)
		{
			double X, Z; To(LX, LZ, X, Z);
			S.Box(X, P + LY, Z, W, H, D, Col, 0, C.Rot);
		};
		auto Wheel = [&](double LX, double LZ, double R = .36)
		{
			double X, Z; To(LX, LZ, X, Z);
			S.Cyl(X, P + R - .13, Z, R, R, .26, 0x16171a, 10, PI / 2, C.Rot);
		};
		const uint32 Body = C.Color, Glass = 0x1d2a33;
		auto Four = [&](double F, double R, double Wz) { Wheel(F, Wz); Wheel(F, -Wz); Wheel(R, Wz); Wheel(R, -Wz); };
		switch (C.Kind)
		{
			case ECar::Pickup:
				Bx(0, .62, 0, 4.8, .5, 1.95, Body); Bx(.55, 1.1, 0, 1.9, .62, 1.8, Body); Bx(.55, 1.12, 0, 1.75, .5, 1.84, Glass);
				Bx(-1.4, .95, 0, 1.9, .12, 1.85, 0x23262a); Four(1.5, -1.45, .95); break;
			case ECar::Suv:
				Bx(0, .7, 0, 4.7, .62, 1.95, Body); Bx(-.1, 1.28, 0, 3.4, .62, 1.85, Body); Bx(-.1, 1.3, 0, 3.3, .48, 1.88, Glass); Four(1.45, -1.4, .95); break;
			case ECar::Convertible:
				Bx(0, .58, 0, 5.0, .46, 1.95, Body); Bx(.5, .98, 0, .12, .45, 1.7, Glass);
				Bx(-.5, .86, .5, 1.0, .3, .6, 0xe9d6c0); Bx(-.5, .86, -.5, 1.0, .3, .6, 0xe9d6c0); Bx(-2.4, .95, 0, .35, .18, 1.7, Body); Four(1.55, -1.5, .95); break;
			case ECar::Van:
				Bx(0, 1.0, 0, 4.9, 1.35, 1.95, Body); Bx(2.0, 1.45, 0, .9, .55, 1.9, Glass); Bx(-.2, .5, 0, 4.9, .08, 1.99, 0x2a2d30);
				for (double LZ : {-.55, .55}) Bx(-.2, 1.74, LZ, 3.6, .05, .05, 0xb9bec2);
				Four(1.55, -1.5, .95); break;
			case ECar::Wagon:
				Bx(0, .62, 0, 4.9, .5, 1.9, Body); Bx(-.35, 1.12, 0, 3.0, .5, 1.78, Body); Bx(-.35, 1.14, 0, 2.9, .4, 1.82, Glass);
				for (double LZ : {-.96, .96}) Bx(0, .66, LZ, 4.6, .28, .03, 0xc9a063);
				Four(1.5, -1.45, .92); break;
			case ECar::Cart:
				Bx(.1, .42, 0, 2.5, .28, 1.15, Body); Bx(-.55, .78, 0, .8, .5, 1.05, 0xe9d6c0); Bx(.4, .62, 0, .8, .14, 1.05, 0xe9d6c0);
				for (double LX : {-.9, .95}) for (double LZ : {-.55, .55}) Bx(LX, 1.12, LZ, .04, 1.0, .04, 0xe8ebee);
				Bx(0, 1.66, 0, 2.2, .06, 1.3, 0xf2f2ee);
				Wheel(.85, .6, .22); Wheel(.85, -.6, .22); Wheel(-.75, .6, .22); Wheel(-.75, -.6, .22); break;
			default:
				Bx(0, .6, 0, 4.7, .5, 1.85, Body); Bx(-.2, 1.1, 0, 2.5, .5, 1.7, Body); Bx(-.2, 1.12, 0, 2.4, .4, 1.74, Glass); Four(1.45, -1.4, .9); break;
		}
		if (C.Kind != ECar::Cart)
		{
			Bx(-2.4, .32, 0, .09, .12, 1.8, 0x2a2d30); Bx(2.4, .32, 0, .09, .12, 1.8, 0x2a2d30);
			Bx(2.38, .62, .7, .06, .16, .34, 0xfff4c8); Bx(2.38, .62, -.7, .06, .16, .34, 0xfff4c8);
			Bx(-2.38, .62, .7, .06, .14, .3, 0xa11b1b); Bx(-2.38, .62, -.7, .06, .14, .3, 0xa11b1b);
		}
		if (C.bTrailer)
		{
			Bx(-6.2, .55, 0, 7.4, .12, 1.7, 0x3a3d42);
			Wheel(-6.6, 1.0, .4); Wheel(-6.6, -1.0, .4);
			Bx(-6.4, 1.1, 0, 6.6, .9, 2.3, 0xf2f2ee); Bx(-6.4, 1.0, 0, 6.7, .18, 2.35, 0x23466e);
			Bx(-3.1, 1.15, 0, 1.2, .7, 1.3, 0xf2f2ee); Bx(-6.9, 1.8, 0, 1.4, .7, 1.1, 0xdfe6ea); Bx(-9.8, 1.2, 0, .5, .9, .7, 0x15171a);
		}
	}
}

// ---------------------------------------------------------------- the far silhouette
void AKeysTown::BuildFar(FTownBatch& F)
{
	F.Span(PadU0, PadU1, -1.8, P, Zf(PadV1), Zf(PadV0), 0xa9a69d);
	for (const FLot& L : Lots())
	{
		const FLook& Lk = LookOf(L.Style);
		F.Span(L.U0, L.U1, P, P + L.H, Zf(L.V1), Zf(L.V0), Lk.Wall);
		F.Span(L.U0 - .12, L.U1 + .12, P + L.H, P + L.H + .8, Zf(L.V1) - .12, Zf(L.V0) + .12, L.Style == EStyle::Bar ? 0x8f959b : Lk.Trim);
	}
	for (const FPalm& Pm : Palms()) { F.Cyl(Pm.U, P, Zf(Pm.V), .2, .3, 6 * Pm.H, 0x8c7758, 5); F.Ball(Pm.U, P + 6 * Pm.H, Zf(Pm.V), 1.8, 0x3f8a35, .6, 5); }
}

void AKeysTown::SetDusk(double K) { Dusk = FMath::Clamp(K, 0.0, 1.0); }

void AKeysTown::SetOwnedEstates(const TArray<FString>& Owned)
{
	OwnedEstates = Owned;
	BuildEstates();
}

// ---------------------------------------------------------------- Harbor Row: four town lots, for sale until you buy one
void AKeysTown::BuildEstates()
{
	FTownBatch E;
	for (const FEstateLot& L : HarborRow())
	{
		const bool bOwned = OwnedEstates.Contains(L.Id);
		const double CX = (L.U0 + L.U1) / 2, W = L.U1 - L.U0;
		// the lot's lawn behind a low picket fence (the front of it is the back lot, where the cars nose in)
		E.Flat(L.U0 + .5, L.U1 - .5, Zf(L.V1 - 1.2), Zf(L.V0 + 9), P + .035, Hex(bOwned ? 0x5d9a3a : 0x7f9a4a));
		for (double U = L.U0 + .5; U <= L.U1 - .5; U += .5) E.Box(U, P + .45, Zf(L.V0 + 9), .08, .9, .04, 0xf4f4f2);
		E.Span(L.U0 + .5, L.U1 - .5, P + .7, P + .78, Zf(L.V0 + 9) - .03, Zf(L.V0 + 9) + .03, 0xf4f4f2);
		if (!bOwned)
		{
			// the For Sale sign on two posts, by the fence and clear of the parked cars
			const double SignU = (L.U0 + L.U1) / 2 + ((L.U0 + L.U1) / 2 < 0 ? 6 : -6);
			E.Box(SignU - .8, P + .8, Zf(L.V0 + 9.6), .1, 1.6, .1, 0xf4f4f2);
			E.Box(SignU + .8, P + .8, Zf(L.V0 + 9.6), .1, 1.6, .1, 0xf4f4f2);
			E.Box(SignU, P + 1.5, Zf(L.V0 + 9.6), 2.2, 1.0, .06, 0xc23a3a);
			continue;
		}
		// a villa (estates/residences.ts villa): ground floor, a set-back first floor, terracotta hips, pool deck in front
		const double D = 14, Hw = FMath::Min(W - 6, 26.0), Zc = Zf(L.V0 + 9 + 4 + 7 + D / 2);
		E.Span(CX - Hw / 2, CX + Hw / 2, P, P + 4.3, Zc - D / 2, Zc + D / 2, 0xf3ead8);
		E.Span(CX - Hw / 2, CX + Hw / 2, P + 4.3, P + 4.6, Zc - D / 2, Zc + D / 2, 0xffffff);
		const double UW = Hw * .7, UD = D * .65;
		E.Span(CX - UW / 2, CX + UW / 2, P + 4.6, P + 8.2, Zc - UD / 2 - D * .12, Zc + UD / 2 - D * .12, 0xf3ead8);
		E.Gable(CX, P + 8.2, Zc - D * .12, UW + 1, UD + 1, 2.6, 0xc0603a, true);
		for (int32 i = 0; i < 5; i++) E.Box(CX - Hw / 2 + Hw * (i + .5) / 5, P + 1.9, Zc + D / 2 + .04, 1.6, 2.2, .08, 0x27414f);
		for (int32 i = 0; i < 3; i++) E.Box(CX - UW / 2 + UW * (i + .5) / 3, P + 6.2, Zc + UD / 2 - D * .12 + .04, 1.4, 1.5, .08, 0x27414f);
		E.Span(CX - Hw / 2 - 1, CX + Hw / 2 + 1, P, P + .35, Zc + D / 2, Zc + D / 2 + 7, 0xd8c9a8);
		E.Span(CX - Hw * .3, CX + Hw * .25, P + .3, P + .38, Zc + D / 2 + 1.5, Zc + D / 2 + 5.8, 0x3fb6c9);
		E.Box(CX + Hw * .3, P + 1.45, Zc + D / 2 + .06, 2.2, 2.9, .12, 0x8a5a36);
	}
	E.M.Commit(Estates, 0, AKeysWorld::VertexColorMaterial());
	// the sign text
	for (int32 i = InteriorSigns.Num() - 1; i >= 0; i--)
		if (InteriorSigns[i] && InteriorSigns[i]->ComponentHasTag(TEXT("estate"))) { InteriorSigns[i]->DestroyComponent(); InteriorSigns.RemoveAt(i); }
	for (const FEstateLot& L : HarborRow())
	{
		if (OwnedEstates.Contains(L.Id)) continue;
		const double CX = (L.U0 + L.U1) / 2 + ((L.U0 + L.U1) / 2 < 0 ? 6 : -6);
		AddSign(TEXT("FOR SALE"), CX, P + 1.68, L.V0 + 9.6 - .05, .32, FLinearColor::White, 0, true, 2.0);
		AddSign(FString::Printf(TEXT("$%dk  %s"), L.Price / 1000, L.Name), CX, P + 1.3, L.V0 + 9.6 - .05, .14, FLinearColor::White, 0, true, 2.0);
		InteriorSigns.Last()->ComponentTags.Add(TEXT("estate"));
		InteriorSigns[InteriorSigns.Num() - 2]->ComponentTags.Add(TEXT("estate"));
	}
}

// ---------------------------------------------------------------- per frame: tiers, people, the aquarium
void AKeysTown::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	APlayerCameraManager* Cam = UGameplayStatics::GetPlayerCameraManager(this, 0);
	if (!Cam) return;
	const FVector CU = Cam->GetCameraLocation();
	const FVector CamGame(CU.X / 100, CU.Z / 100, CU.Y / 100);
	double CamU, CamV;
	ToLocal(CamGame.X, CamGame.Z, CamU, CamV);
	const double DU = FMath::Max(0.0, FMath::Max(PadU0 - CamU, CamU - PadU1)), DV = FMath::Max(0.0, FMath::Max(PadV0 - 60 - CamV, CamV - PadV1));
	const double Dist = FMath::Sqrt(DU * DU + DV * DV);
	const int32 NewTier = Dist < 1600 ? 0 : Dist < 4500 ? 1 : 2;
	if (NewTier != Tier)
	{
		Tier = NewTier;
		Near->SetVisibility(Tier == 0);
		Glow->SetVisibility(Tier == 0);
		Estates->SetVisibility(Tier == 0);
		Far->SetVisibility(Tier == 1);
		for (UTextRenderComponent* T : Signs) if (T) T->SetVisibility(Tier == 0 && Dist < 600);
	}
	if (Tier == 0)
		for (UTextRenderComponent* T : Signs) if (T && T->IsVisible() != (Dist < 600)) T->SetVisibility(Dist < 600);
	const bool bIn = Tier == 0 && Dist < 160;
	if (bIn != bInsideOn)
	{
		bInsideOn = bIn;
		Inside->SetVisibility(bIn);
		Tank->SetVisibility(bIn);
		for (UTextRenderComponent* T : InteriorSigns) if (T) T->SetVisibility(bIn || T->ComponentHasTag(TEXT("estate")));
	}
	// the glow layer: neon is dim by day and bright at sunset
	Dusk = AKeysWorld::Dusk(this);
	if (FMath::Abs(Dusk - AppliedDusk) > .02 || (Halos && Halos->IsVisible() != (Tier == 0 && Dusk > .02)))
	{
		AppliedDusk = Dusk;
		Glow->SetVisibility(Tier == 0);
		if (Halos) Halos->SetVisibility(Tier == 0 && Dusk > .02);
		if (HaloMID) HaloMID->SetScalarParameterValue(TEXT("Bright"), float(1.4 * Dusk));
	}
	if (Tier == 0 && Dist < 260)
	{
		PeopleT += DeltaSeconds;
		UpdatePeople(GetWorld()->GetTimeSeconds(), DeltaSeconds, CamGame);
		People->SetVisibility(true);
	}
	else People->SetVisibility(false);
}

void AKeysTown::UpdatePeople(double T, double Dt, const FVector& CamGame)
{
	double CamU, CamV;
	ToLocal(CamGame.X, CamGame.Z, CamU, CamV);
	const FLot* CamLot = nullptr;
	for (const FLot& L : Lots()) if (CamU > L.U0 && CamU < L.U1 && CamV > L.V0 && CamV < L.V1 && CamGame.Y < P + L.H) CamLot = &L;
	FKeysMesh M;
	FTownBatch Fish;
	int32 Drawn = 0;
	for (FPerson& Pp : Folk)
	{
		// walkers advance along their loop
		double U = Pp.U, V = Pp.V, Yaw = Pp.Yaw;
		double Speed = 0;
		if (Pp.Path.Num() >= 2 && Pp.Len > 0)
		{
			double S = FMath::Fmod(Pp.S0 + T * Pp.Speed, Pp.Len);
			for (int32 i = 1; i < Pp.Path.Num(); i++)
			{
				const double L = FVector2D::Distance(Pp.Path[i - 1], Pp.Path[i]);
				if (S <= L || i == Pp.Path.Num() - 1)
				{
					const double F = L > 0 ? FMath::Clamp(S / L, 0.0, 1.0) : 0;
					const FVector2D Q = FMath::Lerp(Pp.Path[i - 1], Pp.Path[i], F);
					U = Q.X; V = Q.Y;
					const FVector2D Dd = Pp.Path[i] - Pp.Path[i - 1];
					Yaw = FMath::Atan2(Dd.Y, Dd.X);
					break;
				}
				S -= L;
			}
			Speed = Pp.Speed;
			// make way for the player: step half a metre aside when they are in the way
			if (bWalkerAshore)
			{
				double WU, WV;
				ToLocal(WalkerPos.X, WalkerPos.Z, WU, WV);
				const double Dx = U - WU, Dv = V - WV, D = FMath::Sqrt(Dx * Dx + Dv * Dv);
				if (D < 1.2 && D > .01) { U += Dx / D * (1.2 - D); V += Dv / D * (1.2 - D); }
			}
		}
		const double Dc = FMath::Sqrt(FMath::Square(U - CamU) + FMath::Square(V - CamV));
		if (Dc > 140) continue;
		if (!Pp.Inside.IsEmpty())
		{
			// interior people only when you are in (or right outside) their room
			const FLot* L = Lot(Pp.Inside);
			if (!L) continue;
			const bool bNearDoor = FMath::Abs(CamU - L->Door) < 8 && CamV > L->V0 - 10 && CamV < L->V0 + 2;
			if (CamLot != L && !bNearDoor && Dc > 30) continue;
		}
		FKeysHuman& H = Pp.H;
		H.Pos = FVector(U, -V, Pp.Y + (Pp.bSeated ? .02 : 0));
		H.Yaw = -Yaw;
		H.IdleUpdate(T, 0);
		H.Pitch = 0; H.Roll = 0;
		if (Speed > 0)
		{
			Pp.Gait += Dt * Speed * 2.6;
			const double G = Pp.Gait, St = .26 * FMath::Min(1.0, Speed);
			H.PoseLegs(FKeysHuman::J(-.105, .085 + FMath::Max(0.0, FMath::Sin(G)) * .1, FMath::Sin(G) * St), FKeysHuman::J(.105, .085 + FMath::Max(0.0, -FMath::Sin(G)) * .1, -FMath::Sin(G) * St));
			H.Pose(FKeysHuman::J(-.24, H.HipY + .02, -FMath::Sin(G) * .22), FKeysHuman::J(.24, H.HipY + .02, FMath::Sin(G) * .22));
			H.Pos.Z += FMath::Abs(FMath::Sin(G)) * .03;
		}
		else if (Pp.bDancer)
		{
			// the stage act: hips sway, arms up, a slow turn on the spot
			const double G = T * 2.2 + Pp.U;
			H.Roll = FMath::Sin(G) * .12;
			H.Yaw = -Yaw + FMath::Sin(T * .5 + Pp.U) * .6;
			H.PoseLegs(FKeysHuman::J(-.12, .085, .02), FKeysHuman::J(.12, .085 + FMath::Max(0.0, FMath::Sin(G * .5)) * .15, .1));
			H.Pose(FKeysHuman::J(-.22 + FMath::Sin(G) * .1, H.HipY + .95 + FMath::Sin(G * .5) * .1, .1), FKeysHuman::J(.22 + FMath::Sin(G) * .1, H.HipY + .9 - FMath::Sin(G * .5) * .1, .1));
		}
		else
		{
			H.DefaultPose();
			if (Pp.Look == TEXT("punter")) H.Pose(FKeysHuman::J(-.18, H.HipY + .35, .38), FKeysHuman::J(.2, H.HipY + .3 + FMath::Max(0.0, FMath::Sin(T * 1.7 + Pp.U)) * .12, .36));
		}
		H.Build(M);
		Drawn++;
	}
	// the casino aquarium's fish swimming the length of the wall
	if (bInsideOn)
	{
		static const uint32 FC[] = {0xf2c14e, 0x3fb6c9, 0xe4572e, 0xffffff, 0x7bc96f, 0xff7ab0};
		const double U0 = -24 + .4 + .25, U1 = U0 + .9;
		for (int32 i = 0; i < 26; i++)
		{
			const double Ph = Hash1(i * 1.7) * 100, Sp = .5 + Hash1(i * 3.1) * .9;
			const double S = FMath::Fmod(Ph + T * Sp, 2 * 23.0);
			const double Vv = 141.8 + (S < 23 ? S : 46 - S);
			const double Yy = P + .7 + Hash1(i * 5.3) * 2.6 + FMath::Sin(T * 1.3 + i) * .08;
			const double Uu = FMath::Lerp(U0, U1, Hash1(i * 7.7));
			const double Sz = .14 + Hash1(i * 2.2) * .22;
			const double Ry = S < 23 ? PI / 2 : -PI / 2;
			Fish.Box(Uu, Yy, Zf(Vv), Sz * .4, Sz * .7, Sz * 1.6, FC[i % 6], 0, Ry);
			Fish.Box(Uu, Yy, Zf(Vv) + (S < 23 ? Sz * .9 : -Sz * .9), .03, Sz * .6, Sz * .5, FC[i % 6], 0, Ry);
		}
		Fish.M.Commit(Tank, 0, AKeysWorld::VertexColorMaterial());
	}
	if (Drawn) M.Commit(People, 0, AKeysWorld::VertexColorMaterial());
	else People->ClearAllMeshSections();
}
