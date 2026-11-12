#include "KeysLandmarks.h"
#include "KeysTown.h"
#include "KeysTownData.h"
#include "KeysGeo.h"
#include "KeysWorld.h"
#include "KeysData.h"
#include "ProceduralMeshComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "Camera/PlayerCameraManager.h"

// Everything here is built in game world coordinates: an FTownBatch with its component at the origin takes the three.js world
// frame (x east, y up, z south) straight to Unreal meters.
using namespace KeysTown;
using KeysTownPaint::Hex;

namespace
{
	double Hc(double A, double B) { const double H = FMath::Sin(A * 127.1 + B * 311.7) * 43758.5453; return H - FMath::FloorToDouble(H); }
	// the yaw (three.js ry) that turns local +z onto direction (nx, nz)
	double YawTo(double NX, double NZ) { return FMath::Atan2(NX, NZ); }
}

AKeysLandmarks::AKeysLandmarks()
{
	PrimaryActorTick.bCanEverTick = true;
	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent = Root;
	Static = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("Static"));
	Static->SetupAttachment(Root);
	Static->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Lamps = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("Lamps"));
	Lamps->SetupAttachment(Root);
	Lamps->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Lamps->SetCastShadow(false);
	Beam = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("Beam"));
	Beam->SetupAttachment(Root);
	Beam->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Beam->SetCastShadow(false);
}

void AKeysLandmarks::BeginPlay()
{
	Super::BeginPlay();
	KeysGeo::Init();
	SetActorLocation(FVector::ZeroVector);
	FTownBatch B, G;
	BuildMarinas(B);
	BuildBridges(B);
	BuildLighthouse(B, G);
	BuildPiers(B);
	BuildEstates(B);
	B.M.Commit(Static, 0, AKeysWorld::VertexColorMaterial());
	UMaterialInterface* GlowMat = LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/EngineDebugMaterials/VertexColorMaterial.VertexColorMaterial"));
	G.M.Commit(Lamps, 0, GlowMat ? GlowMat : AKeysWorld::VertexColorMaterial());
	BuildBuoys();
	for (const FPier& Pp : Piers()) UE_LOG(LogTemp, Log, TEXT("Keys Run: pier %s at %.0f, %.0f -> %.0f, %.0f"), *Pp.Name, Pp.X0, Pp.Z0, Pp.X0 + Pp.NX * Pp.Len, Pp.Z0 + Pp.NZ * Pp.Len);
	for (int32 i = 0; i < FMath::Min(6, WaterfrontEstates().Num()); i++) UE_LOG(LogTemp, Log, TEXT("Keys Run: estate on %s at %.0f, %.0f facing %.2f, %.2f"), *WaterfrontEstates()[i].Island, WaterfrontEstates()[i].X, WaterfrontEstates()[i].Z, WaterfrontEstates()[i].NX, WaterfrontEstates()[i].NZ);
	for (const KeysGeo::FMarina& M : KeysGeo::Marinas()) UE_LOG(LogTemp, Log, TEXT("Keys Run: marina %s at %.0f, %.0f dir %d"), *M.Name, M.SX, M.SZ, M.Dir);
	UE_LOG(LogTemp, Log, TEXT("Keys Run: landmarks built - %d tris, %d channel marks, %d piers, %d waterfront estates"), B.Tris(), Marks.Num(), Piers().Num(), WaterfrontEstates().Num());
}

// ---------------------------------------------------------------- the marinas (world/marinas.ts)
void AKeysLandmarks::BuildMarinas(FTownBatch& B)
{
	const uint32 Wood = 0x9c7a55, Pile = 0x6e5a44;
	static const uint32 ShedC[] = {0xa8e0d8, 0xfde2a7, 0xf7c6d0, 0xb9d4f5};
	static const uint32 BoatC[][2] = {{0xffffff, 0x23466e}, {0xf3efe5, 0x2bb3a8}, {0xe8eef2, 0xe4572e}, {0xffffff, 0x14181f}};
	const TArray<KeysGeo::FMarina>& Ms = KeysGeo::Marinas();
	for (int32 mi = 0; mi < Ms.Num(); mi++)
	{
		const KeysGeo::FMarina& M = Ms[mi];
		auto Zo = [&](double O) { return M.SZ + M.Dir * O; };
		for (int32 i = 0; i < 8; i++)
		{
			const FDeckBox& D = DeckBoxes()[mi * 8 + i];
			// planks across the walkway, each a shade different
			const bool bAlongZ = D.D > D.W;
			const double Len = bAlongZ ? D.D : D.W;
			for (double S = 0; S < Len - .01; S += .35)
			{
				const double S1 = FMath::Min(Len, S + .33);
				const FLinearColor C = KeysTownPaint::Hex(Wood) * float(.85 + .25 * Hc(S, D.CX + i));
				if (bAlongZ) B.SpanC(D.CX - D.W / 2, D.CX + D.W / 2, D.Y - D.T / 2, D.Y + D.T / 2, D.CZ - D.D / 2 + S, D.CZ - D.D / 2 + S1, C);
				else B.SpanC(D.CX - D.W / 2 + S, D.CX - D.W / 2 + S1, D.Y - D.T / 2, D.Y + D.T / 2, D.CZ - D.D / 2, D.CZ + D.D / 2, C);
			}
		}
		// pilings, with a cap, down the main pier, along the T-head and at the end of each finger
		TArray<FVector2D> Piles;
		for (double O = -6; O <= 48; O += 6) { Piles.Add({M.SX - 1.6, Zo(O)}); Piles.Add({M.SX + 1.6, Zo(O)}); }
		for (double XO = -12; XO <= 12; XO += 6) Piles.Add({M.SX + XO, Zo(49.6)});
		for (double O : {14.0, 26.0, 38.0}) for (int32 Sg = -1; Sg <= 1; Sg += 2) Piles.Add({M.SX + Sg * 11.4, Zo(O)});
		for (const FVector2D& Pp : Piles)
		{
			B.Cyl(Pp.X, -3, Pp.Y, .17, .2, 5.4, Pile, 7);
			B.Cyl(Pp.X, 2.4, Pp.Y, .2, .2, .06, 0xe8ebee, 7);
		}
		// boats in the slips: little center consoles (hull, console, T-top, outboard)
		const double Slots[4][2] = {{-1, 20}, {1, 20}, {-1, 32}, {1, 32}};
		for (int32 k = 0; k < 4; k++)
		{
			if ((k + mi) % 4 == 3) continue;
			const uint32* C = BoatC[(k + mi) % 4];
			const double BX = M.SX + Slots[k][0] * 6.8, BZ = Zo(Slots[k][1]);
			B.Box(BX, .45, BZ, 2.5, .9, 7.0, C[0]);
			B.Box(BX, .2, BZ, 2.4, .25, 6.9, C[1]);
			B.Box(BX, .55, BZ - M.Dir * 3.9, 1.2, .7, 1.4, C[0], 0, 0, 0);
			B.Box(BX, 1.25, BZ + M.Dir * .2, 1.0, 1.0, 1.0, 0xf2f2f2);
			B.Box(BX, 2.75, BZ + M.Dir * .3, 1.8, .08, 2.2, C[1]);
			for (int32 sx = -1; sx <= 1; sx += 2) for (int32 sz = -1; sz <= 1; sz += 2) B.Cyl(BX + sx * .7, .9, BZ + M.Dir * (.3 + sz * .8), .035, .035, 1.85, 0xd9dde2, 5);
			B.Box(BX, .9, BZ + M.Dir * 3.9, .6, 1.1, .7, 0x15171a);
		}
		// the harbourmaster's shed, with a hip roof
		const double SX = M.SX + 13, SZ = Zo(-9);
		B.Box(SX, 2.8, SZ, 11, 4, 7, ShedC[mi]);
		B.Box(SX, .8 + .0, SZ, 11.2, 1.6, 7.2, 0xe8e4d8);
		B.Cyl(SX, 4.8, SZ, .05, 7.4 * .75, 2.4, 0xb8bcc2, 4, 0, PI / 4);
		for (double DX : {-3.5, 0.0, 3.5}) B.Box(SX + DX, 3.2, SZ + M.Dir * 3.52, 1.6, 1.2, .06, 0x27414f);
		B.Box(SX - 4.2, 2.0, SZ + M.Dir * 3.52, 1.2, 2.4, .06, 0x5a4a38);
		// the dry-stack rack: three tiers of hulls on a steel frame (two of the four marinas have one)
		if (mi % 2 == 0)
		{
			const double RX = M.SX - 22, RZ = Zo(-12);
			for (int32 c = 0; c < 6; c++)
				for (int32 sz = -1; sz <= 1; sz += 2) B.Box(RX - 10 + c * 4, 5, RZ + sz * 4, .25, 10, .25, 0x5b6770);
			for (int32 t = 0; t < 3; t++)
			{
				const double Y = 1.6 + t * 3.1;
				B.Box(RX, Y, RZ, 20.5, .2, 8.4, 0x5b6770);
				for (int32 c = 0; c < 5; c++)
				{
					if (Hc(c + t * 7, mi) < .2) continue;
					const uint32* C = BoatC[(c + t + mi) % 4];
					B.Box(RX - 8 + c * 4, Y + .6, RZ, 2.3, 1.0, 7.4, C[0]);
					B.Box(RX - 8 + c * 4, Y + .25, RZ, 2.2, .3, 7.3, C[1]);
				}
			}
		}
		// fuel dock sign and a flag pole at the pier head
		B.Cyl(M.SX + 2, 1.4, Zo(-4), .05, .07, 8, 0xe8ebee, 6);
		B.Box(M.SX + 2 - .7, 8.8, Zo(-4), 1.4, .9, .03, 0xc23a3a);
	}
}

// ---------------------------------------------------------------- US-1 and the Seven Mile Bridge (world/bridge.ts)
void AKeysLandmarks::BuildBridges(FTownBatch& B)
{
	using namespace KeysGeo;
	const double Step = 24, GapX0 = -3200, GapX1 = -3080, HumpX = (GapX0 + GapX1) / 2, HumpHalf = 320, HumpRise = 12;
	auto SS = [](double A, double Bb, double X) { const double T = FMath::Clamp((X - A) / (Bb - A), 0.0, 1.0); return T * T * (3 - 2 * T); };
	auto Hump = [&](double X) { const double T = FMath::Max(0.0, 1 - FMath::Abs(X - HumpX) / HumpHalf); return HumpRise * T * T * (3 - 2 * T); };
	// the new bridge's deck height along x, so the bents can meet it
	auto NewDeckH = [&](double X)
	{
		const double Z = ChainZ(X), SD = ShoreInfo(X, Z).D;
		return (SD < 0 ? 5.6 : 2.6 + (7.2 - 2.6) * SS(0, 70, SD)) + Hump(X);
	};
	for (int32 Old = 0; Old <= 1; Old++)
	{
		const double X0 = Old ? -3820 : WBX0, X1 = Old ? -2010 : WBX1, DZ = Old ? -58 : 0;
		int32 i = 0;
		for (double X = X0; X < X1; X += Step, i++)
		{
			const double XA = X, XB = X + Step;
			if (Old && XB > GapX0 && XA < GapX1) continue;
			const double ZA = ChainZ(XA) + DZ, ZB = ChainZ(XB) + DZ, MX = (XA + XB) / 2, MZ = (ZA + ZB) / 2;
			const double Ang = FMath::Atan2(ZB - ZA, XB - XA), Len = FMath::Sqrt(FMath::Square(XB - XA) + FMath::Square(ZB - ZA));
			const double SD = ShoreInfo(MX, MZ).D, LH = LandH(MX, MZ);
			const double PX = -FMath::Sin(Ang), PZ = FMath::Cos(Ang);
			if (LH > .9 && SD < -4)
			{
				// US-1 across the keys: asphalt with its lines, on the ground
				if (!Old)
				{
					B.Box(MX, LH + .08, MZ, Len + .3, .16, 9, 0x3e4146, 0, -Ang);
					B.Box(MX, LH + .17, MZ, Len + .3, .02, .2, 0xe8c43a, 0, -Ang);
					for (int32 sg = -1; sg <= 1; sg += 2) B.Box(MX + PX * 3.9 * sg, LH + .17, MZ + PZ * 3.9 * sg, Len + .3, .02, .18, 0xf4f1e8, 0, -Ang);
				}
				continue;
			}
			if (Old)
			{
				// the old bridge: lower, narrower, weathered; an arch rib under each span
				const double H = 5.0;
				const FLinearColor C = Hex(0xb9ad98) * float(.88 + .22 * Hc(MX, MZ));
				B.BoxC(MX, H, MZ, Len + .4, .8, 7.5, C, 0, -Ang);
				for (int32 sg = -1; sg <= 1; sg += 2) B.Box(MX + PX * 3.55 * sg, H + .7, MZ + PZ * 3.55 * sg, Len + .4, .6, .25, 0xa89c86, 0, -Ang);
				if (SD > 1)
					for (int32 sg = -1; sg <= 1; sg += 2)
					{
						// the arch: a half ring from pier to pier, rising to the deck
						const FVector Ctr(MX + PX * 2.6 * sg, H - .4, MZ + PZ * 2.6 * sg);
						const FVector Along(FMath::Cos(Ang), 0, FMath::Sin(Ang));
						FVector Prev = Ctr - Along * (Len / 2) - FVector(0, 3.0, 0);
						for (int32 k = 1; k <= 8; k++)
						{
							const double T = k / 8.0, A = PI * T;
							const FVector Pt = Ctr - Along * (Len / 2 * FMath::Cos(A)) - FVector(0, 3.0 * (1 - FMath::Sin(A)), 0);
							B.Tube3(Prev, Pt, .32, .32, 5, 0xa39883);
							Prev = Pt;
						}
					}
				continue;
			}
			// the new bridge: deck, barrier walls, striping, and a bent (two columns and a cap) every other span
			const double H = (SD < 0 ? 5.6 : 2.6 + (7.2 - 2.6) * SS(0, 70, SD)) + Hump(MX);
			const double HA = NewDeckH(XA), HB = NewDeckH(XB), Pitch = FMath::Atan2(HB - HA, Len);
			const FLinearColor C = Hex(0xe2dccb) * float(.88 + .22 * Hc(MX, MZ));
			B.BoxC(MX, (HA + HB) / 2, MZ, Len + .4, 1.0, 10, C, 0, -Ang, Pitch);
			for (int32 sg = -1; sg <= 1; sg += 2)
			{
				B.Box(MX + PX * 4.85 * sg, (HA + HB) / 2 + .92, MZ + PZ * 4.85 * sg, Len + .4, .84, .38, 0xf0ebdd, 0, -Ang, Pitch);
				B.Box(MX + PX * 3.9 * sg, (HA + HB) / 2 + .52, MZ + PZ * 3.9 * sg, Len + .4, .05, .22, 0xf4f1e8, 0, -Ang, Pitch);
			}
			B.Box(MX, (HA + HB) / 2 + .52, MZ, Len + .4, .05, .22, 0xe8c43a, 0, -Ang, Pitch);
			(void)H;
		}
	}
	// the new bridge's bents, where the boat collides with them (KeysGeo pillars come in pairs either side of the deck)
	for (const FPillar& Pl : Pillars())
	{
		if (Pl.bOld)
		{
			const double Ang = FMath::Atan(.000024 * Pl.X);
			B.Box(Pl.X, (5.0 - .4 - 3) / 2, Pl.Z, 1.8, 5.0 - .4 + 3, 6.6, 0x9c9382, 0, -Ang);
			B.Cyl(Pl.X, -.4, Pl.Z, 2.1, 2.1, 1.5, 0x3c4230, 6);
			continue;
		}
		const double CZ = ChainZ(Pl.X), Side = Pl.Z > CZ ? 1 : -1;
		if (Side < 0) continue;   // one bent per pair
		const double Ang = FMath::Atan(.000024 * Pl.X), Top = NewDeckH(Pl.X) - .5;
		const double PX = -FMath::Sin(Ang), PZ = FMath::Cos(Ang);
		for (int32 sg = -1; sg <= 1; sg += 2)
		{
			const double X = Pl.X + PX * 4.5 * sg, Z = CZ + PZ * 4.5 * sg;
			B.Box(X, (Top - .55 - 3) / 2, Z, 1.6, Top - .55 + 3, 1.6, 0xd6cfbd, 0, -Ang);
			B.Cyl(X, -.4, Z, 1.5, 1.5, 1.5, 0x3c4230, 6);
		}
		B.Box(Pl.X, Top - .28, CZ, 2.0, .55, 11.6, 0xe0dacb, 0, -Ang);
	}
	// broken stubs where the old swing span was taken out
	for (double X = GapX0 + 10; X < GapX1; X += 30) B.Box(X, -.6, ChainZ(X) - 58, 1.8, 2.4 + Hc(X, 3) * 1.6, 2.6, 0x8c8474, Hc(X, 1) * .2 - .1, 0, Hc(X, 2) * .2 - .1);
}

// ---------------------------------------------------------------- Sombrero Key Light (world/lighthouse.ts) and Faro Blanco's tower (landmarks.ts)
void AKeysLandmarks::BuildLighthouse(FTownBatch& B, FTownBatch& G)
{
	const double LX = -200, LZ = KeysGeo::ChainZ(-200) + 1455, Focal = 43;
	const uint32 Iron = 0x6e4232, White = 0xe8e6df, Dark = 0x22262b;
	const int32 Sides = 8;
	const double BaseR = 13, TopR = 3.2;
	auto Leg = [&](int32 i, double R, double Y) { const double A = double(i) / Sides * 2 * PI; return FVector(LX + FMath::Cos(A) * R, Y, LZ + FMath::Sin(A) * R); };
	for (int32 i = 0; i < Sides; i++)
	{
		B.Tube3(Leg(i, BaseR, -3.2), Leg(i, BaseR, 1.2), .42, .42, 6, Dark);
		B.Tube3(Leg(i, BaseR, 1.2), Leg(i, TopR, Focal - 5), .34, .34, 6, Iron);
	}
	const double Rings[] = {1.2, 9, 17, 25, 32, Focal - 5};
	auto RAt = [&](double Y) { return BaseR + (TopR - BaseR) * ((Y - 1.2) / (Focal - 6.2)); };
	for (double Y : Rings) for (int32 i = 0; i < Sides; i++) B.Tube3(Leg(i, RAt(Y), Y), Leg((i + 1) % Sides, RAt(Y), Y), .16, .16, 5, Iron);
	for (int32 k = 0; k < 5; k++)
		for (int32 i = 0; i < Sides; i++) B.Tube3(Leg(i, RAt(Rings[k]), Rings[k]), Leg((i + 1) % Sides, RAt(Rings[k + 1]), Rings[k + 1]), .11, .11, 4, Iron);
	B.Tube3(FVector(LX, 1.2, LZ), FVector(LX, Focal - 4.6, LZ), .62, .62, 8, Iron);
	// the keeper's dwelling slung in the legs, white, with a dark roof and a gallery rail
	B.Cyl(LX, 19 - 2.1, LZ, 6.4, 6.4, 4.2, White, Sides);
	for (int32 i = 0; i < Sides; i++) { const double A = (i + .5) / Sides * 2 * PI; B.Box(LX + FMath::Cos(A) * 6.3, 19.3, LZ + FMath::Sin(A) * 6.3, 1.2, 1.4, .1, 0x27414f, 0, -A + PI / 2); }
	B.Cyl(LX, 22.2 - 1.1, LZ, .05, 7.2, 2.2, Dark, Sides);
	B.Cyl(LX, 16.8, LZ, 7.6, 7.6, .3, Dark, Sides);
	for (int32 i = 0; i < Sides; i++) B.Tube3(Leg(i, 7.4, 21.2 - 4), Leg((i + 1) % Sides, 7.4, 21.2 - 4), .08, .08, 4, Dark);
	// the lantern room: gallery, glazed drum, dome, finial
	B.Cyl(LX, Focal - 4.65, LZ, 4.6, 4.6, .5, Dark, 16);
	for (int32 i = 0; i < 16; i++) { const double A = i / 16.0 * 2 * PI; B.Tube3(FVector(LX + FMath::Cos(A) * 4.4, Focal - 4.1, LZ + FMath::Sin(A) * 4.4), FVector(LX + FMath::Cos(A) * 4.4, Focal - 3, LZ + FMath::Sin(A) * 4.4), .06, .06, 4, Dark); }
	B.Cyl(LX, Focal - 4.1, LZ, 3.1, 3.1, .6, Dark, 16);
	for (int32 i = 0; i < 8; i++) { const double A = i / 8.0 * 2 * PI; B.Tube3(FVector(LX + FMath::Cos(A) * 3.1, Focal - 3.5, LZ + FMath::Sin(A) * 3.1), FVector(LX + FMath::Cos(A) * 3.1, Focal + .3, LZ + FMath::Sin(A) * 3.1), .07, .07, 4, Dark); }
	B.Cyl(LX, Focal + .3, LZ, .05, 3.6, 2.4, Dark, 16);
	B.Tube3(FVector(LX, Focal + 2.5, LZ), FVector(LX, Focal + 4.2, LZ), .1, .1, 5, Dark);
	// the lamp: a bright lens in the drum, seen from anywhere
	G.Ball(LX, Focal - 1.6, LZ, 1.5, 0xfff1c8, 1.3, 12);
	// Faro Blanco Marina's lighthouse: a white tower with a red cap
	for (const KeysGeo::FMarina& M : KeysGeo::Marinas())
	{
		if (!M.Name.StartsWith(TEXT("Faro Blanco"))) continue;
		const double TX = M.SX - 24, TZ = M.SZ - M.Dir * 6;
		B.Cyl(TX, 0, TZ, 1.6, 2.2, 14, 0xf4f4f2, 12);
		B.Cyl(TX, 14, TZ, 1.8, 1.8, 2.2, 0xc8302e, 12);
		B.Cyl(TX, 16.2, TZ, .2, 2.0, 1.2, 0x22262b, 12);
		B.Cyl(TX, 13.8, TZ, 2.4, 2.4, .25, 0x22262b, 12);
		G.Ball(TX, 15.1, TZ, .9, 0xfff1c8, 1, 10);
		B.Box(TX, 2, TZ + M.Dir * 2.1, 1, 2, .1, 0x5a4a38);
	}
}

// ---------------------------------------------------------------- public fishing piers (world/piers.ts)
void AKeysLandmarks::BuildPiers(FTownBatch& B)
{
	for (const FPier& Pp : Piers())
	{
		const double Ry = YawTo(Pp.NX, Pp.NZ);
		auto At = [&](double A, double C) { return FVector2D(Pp.X0 + Pp.NX * A - Pp.NZ * C, Pp.Z0 + Pp.NZ * A + Pp.NX * C); };
		// the walkway, as planks across it
		for (double A = 0; A < Pp.Len - PierHeadD / 2; A += .4)
		{
			const FVector2D Q = At(A + .19, 0);
			B.BoxC(Q.X, PierTop, Q.Y, PierW, .25, .37, KeysTownPaint::Hex(0x9c7a55) * float(.85 + .25 * Hc(A, Pp.X0)), 0, Ry);
		}
		const FVector2D H = At(Pp.Len, 0);
		B.Box(H.X, PierTop, H.Y, PierHeadW, .25, PierHeadD, 0x8f6f4c, 0, Ry);
		// pilings, rails, lamp posts, and a bench on the head
		for (double A = 2; A < Pp.Len + 2; A += 5)
			for (int32 s = -1; s <= 1; s += 2)
			{
				const double Half = A > Pp.Len - PierHeadD / 2 ? PierHeadW / 2 : PierW / 2;
				const FVector2D Q = At(A, s * Half);
				B.Cyl(Q.X, -3, Q.Y, .16, .19, 4.3 + 1.0, 0x6e5a44, 6);
				B.Cyl(Q.X, PierTop, Q.Y, .04, .04, 1.05, 0xe8ebee, 5);
			}
		for (int32 s = -1; s <= 1; s += 2)
		{
			const FVector2D Q = At((Pp.Len - PierHeadD / 2) / 2, s * (PierW / 2 - .05));
			B.Box(Q.X, PierTop + 1.05, Q.Y, .06, .06, Pp.Len - PierHeadD / 2, 0xe8ebee, 0, Ry);
		}
		for (double A = 10; A < Pp.Len; A += 24)
		{
			const FVector2D Q = At(A, PierW / 2 - .2);
			B.Cyl(Q.X, PierTop, Q.Y, .06, .08, 4.2, 0x4b5158, 6);
			B.Box(Q.X, PierTop + 4.25, Q.Y, .5, .12, .3, 0xf2f0e6, 0, Ry);
		}
		const FVector2D Bn = At(Pp.Len + 1.4, -3);
		B.Box(Bn.X, PierTop + .5, Bn.Y, 1.8, .07, .45, 0x8a5a36, 0, Ry + PI / 2);
		// a fish-cleaning table at the head, and a sign at the land end
		const FVector2D Tb = At(Pp.Len, 4.5);
		B.Box(Tb.X, PierTop + .9, Tb.Y, 1.6, .08, .7, 0xe8edf1, 0, Ry);
		const FVector2D Sg = At(1, -PierW / 2 - .5);
		B.Cyl(Sg.X, PierTop - .5, Sg.Y, .06, .06, 2.6, 0x2d3136, 5);
		B.Box(Sg.X, PierTop + 2.0, Sg.Y, 1.6, .7, .05, 0x1f4f8c, 0, Ry + PI / 2);
	}
}

// ---------------------------------------------------------------- waterfront estates (world/waterfront.ts)
void AKeysLandmarks::BuildEstates(FTownBatch& B)
{
	for (const FEstateSite& E : WaterfrontEstates())
	{
		const double Ry = YawTo(E.NX, E.NZ);
		auto At = [&](double A, double C) { return FVector2D(E.X + E.NX * A - E.NZ * C, E.Z + E.NZ * A + E.NX * C); };
		const double G = KeysGeo::LandH(E.X, E.Z);
		const double Wd = 14 + E.Style * 4, Dp = 11;
		KeysTown::AddObstacleBox(E.X, E.Z, E.NX, E.NZ, Dp / 2 + .5, Wd / 2 + .5);
		// plinth and ground floor, then the style: a villa with a set-back upper floor, a long ranch, or a stilt house
		B.Box(E.X, G - .6 + .45, E.Z, Wd + 1, 1.5, Dp + 1, 0xd8cdb5, 0, Ry);
		if (E.Style == 2)
		{
			for (int32 sx = -1; sx <= 1; sx += 2) for (int32 sz = -1; sz <= 1; sz += 2) { const FVector2D Q = At(sz * (Dp / 2 - .5), sx * (Wd / 2 - .5)); B.Cyl(Q.X, G, Q.Y, .2, .2, 2.6, 0xe8e4d8, 6); }
			B.Box(E.X, G + 2.6 + 1.8, E.Z, Wd, 3.6, Dp, E.Color, 0, Ry);
			B.Gable(E.X, G + 6.2, E.Z, Wd + 1, Dp + 1, 2.4, 0x7a8a96, true);
		}
		else
		{
			B.Box(E.X, G + .3 + 2.0, E.Z, Wd, 4, Dp, E.Color, 0, Ry);
			if (E.Style == 0)
			{
				const FVector2D Up = At(-1.2, 0);
				B.Box(Up.X, G + 4.3 + 1.8, Up.Y, Wd * .7, 3.6, Dp * .65, E.Color, 0, Ry);
				B.Cyl(Up.X, G + 7.9, Up.Y, .05, Wd * .5, 2.6, 0xc0603a, 4, 0, Ry + PI / 4);
				B.Box(E.X, G + 4.45, E.Z, Wd, .3, Dp, 0xffffff, 0, Ry);
			}
			else B.Cyl(E.X, G + 4.3, E.Z, .05, Wd * .75, 2.2, 0x9aa4ab, 4, 0, Ry + PI / 4);
		}
		// windows across the water side
		for (int32 i = 0; i < 4; i++) { const FVector2D Q = At(Dp / 2 + .05, -Wd / 2 + Wd * (i + .5) / 4); B.Box(Q.X, G + (E.Style == 2 ? 4.4 : 2.1), Q.Y, 1.6, 1.8, .08, 0x27414f, 0, Ry); }
		// the pool deck and the pool, between the house and the water
		const FVector2D PD = At(Dp / 2 + 4, 0), PL = At(Dp / 2 + 4, -1.5);
		B.Box(PD.X, G + .1, PD.Y, Wd + 2, .35, 7, 0xe8dcc0, 0, Ry);
		B.Box(PL.X, G + .3, PL.Y, Wd * .5, .12, 4, 0x3fb6c9, 0, Ry);
		for (int32 i = 0; i < 2; i++) { const FVector2D Q = At(Dp / 2 + 6.2, 3 + i * 1.2); B.Box(Q.X, G + .55, Q.Y, .7, .12, 1.8, 0xffffff, 0, Ry); }
		// palms and a lawn path down to the dock
		for (int32 s = -1; s <= 1; s += 2)
		{
			const FVector2D Q = At(Dp / 2 + 9, s * (Wd / 2 + 2));
			B.Cyl(Q.X, G, Q.Y, .16, .26, 7, 0x8c7758, 6);
			for (int32 k = 0; k < 7; k++) { const double A = k / 7.0 * 2 * PI; B.Box(Q.X + FMath::Sin(A) * 1.2, G + 7, Q.Y + FMath::Cos(A) * 1.2, .7, .05, 2.6, 0x4f9a3c, .2, A); }
		}
		// the private dock out to the lift, pilings, the lift and a center console up out of the water
		const double D0 = 24, D1 = E.DockLen;
		const FVector2D DM = At((D0 + D1) / 2, 0);
		B.Box(DM.X, 1.1, DM.Y, 2.2, .2, D1 - D0, 0x9c7a55, 0, Ry);
		for (double A = D0 + 2; A <= D1; A += 4) for (int32 s = -1; s <= 1; s += 2) { const FVector2D Q = At(A, s * 1.2); B.Cyl(Q.X, -2.5, Q.Y, .14, .16, 4.0, 0x6e5a44, 6); }
		const FVector2D LiftC = At(D1 - 4, 4.2);
		for (int32 sa = -1; sa <= 1; sa += 2) for (int32 sc = -1; sc <= 1; sc += 2) { const FVector2D Q = At(D1 - 4 + sa * 3.5, 4.2 + sc * 1.8); B.Cyl(Q.X, -2.5, Q.Y, .15, .15, 6.2, 0xd9dde2, 6); }
		B.Box(LiftC.X, .9, LiftC.Y, 2.4, .15, 7.6, 0x5b6770, 0, Ry);
		static const uint32 HullC[] = {0xffffff, 0xf3efe5, 0x23466e, 0xe8eef2};
		const uint32 HC = HullC[int32(Hc(E.X, E.Z) * 4) % 4];
		B.Box(LiftC.X, 1.5, LiftC.Y, 2.3, .9, 7.0, HC, 0, Ry);
		B.Box(LiftC.X, 1.2, LiftC.Y, 2.25, .2, 6.9, 0x2bb3a8, 0, Ry);
		const FVector2D Con = At(D1 - 4.2, 4.2);
		B.Box(Con.X, 2.4, Con.Y, 1.0, 1.0, 1.0, 0xf2f2f2, 0, Ry);
		B.Box(Con.X, 3.75, Con.Y, 1.8, .08, 2.2, 0x23466e, 0, Ry);
		const FVector2D Ob = At(D1 - 4 - 3.9, 4.2);
		B.Box(Ob.X, 1.9, Ob.Y, .6, 1.1, .7, 0x15171a, 0, Ry);
	}
}

// ---------------------------------------------------------------- channel marks and mooring balls (channel-marks.ts, channel-buoys.ts, landmarks.ts)
void AKeysLandmarks::BuildBuoys()
{
	// marks down the long sides of every dredged cut, red on the right coming in from seaward
	auto ForRect = [&](double X0, double X1, double Z0, double Z1, int32& N)
	{
		const double W = FMath::Abs(X1 - X0), Hh = FMath::Abs(Z1 - Z0);
		const bool bAlongZ = Hh >= W;
		const double Len = bAlongZ ? Hh : W;
		const int32 Pairs = FMath::Max(2, FMath::RoundToInt(Len / 150));
		const double LoX = FMath::Min(X0, X1), HiX = FMath::Max(X0, X1), LoZ = FMath::Min(Z0, Z1), HiZ = FMath::Max(Z0, Z1);
		const double HalfW = (bAlongZ ? HiX - LoX : HiZ - LoZ) / 2, Inset = FMath::Min(6.0, FMath::Max(.5, HalfW * .35));
		for (int32 i = 0; i < Pairs; i++)
		{
			const double T = 1 - (Pairs == 1 ? .5 : double(i) / (Pairs - 1));
			const double Along = bAlongZ ? LoZ + (HiZ - LoZ) * T : LoX + (HiX - LoX) * T;
			const double EA = (bAlongZ ? LoX : LoZ) + Inset, EB = (bAlongZ ? HiX : HiZ) - Inset;
			const bool bRedLow = !bAlongZ;
			const double Red = bRedLow ? EA : EB, Green = bRedLow ? EB : EA;
			if (bAlongZ) { Marks.Add({Red, Along, true, N * 2}); Marks.Add({Green, Along, false, N * 2 + 1}); }
			else { Marks.Add({Along, Red, true, N * 2}); Marks.Add({Along, Green, false, N * 2 + 1}); }
			N++;
		}
	};
	int32 N = 1;
	const double Cuts[4][5] = {{-1700, -480, 200, 430, 3.8}, {-2000, -1560, 150, 800, 4.2}, {-1750, -1350, -420, -180, 3.4}, {-800, -420, -420, -180, 3.4}};
	for (const auto& C : Cuts) ForRect(C[0], C[1], KeysGeo::ChainZ(C[0]) + C[2], KeysGeo::ChainZ(C[1]) + C[3], N);
	const double Deep[4][4] = {{2312, 2380, -2010, -1420}, {2376, 2480, -2640, -1950}, {2140, 2224, -2640, -1960}, {2224, 2380, -2740, -2640}};
	for (const auto& C : Deep) ForRect(C[0], C[1], C[2], C[3], N);
	// the marina entrances: a pair at the mouth of each basin
	for (const KeysGeo::FMarina& M : KeysGeo::Marinas())
		for (double R : {60.0, 110.0})
		{
			const double Z = M.SZ + M.Dir * R;
			Marks.Add({M.SX + M.Dir * 22, Z, true, N * 2});
			Marks.Add({M.SX - M.Dir * 22, Z, false, N * 2 + 1});
			N++;
		}
	Marks.RemoveAll([](const FMark& K) { return KeysGeo::DepthAt(K.X, K.Z) < 1 || KeysGeo::LandH(K.X, K.Z) > -.5; });
	// the reef's mooring-ball line along Hawk Channel
	int32 Bi = 0;
	for (double X = -3900; X < 3900; X += 260, Bi++) Balls.Add(FVector2D(X + (Hc(Bi, 1) - .5) * 60, KeysGeo::ChainZ(X) + FMath::Lerp(1360.0, 1440.0, Hc(Bi, 2))));

	UStaticMesh* Cyl = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	UStaticMesh* Cone = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cone.Cone"));
	UStaticMesh* Sph = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	if (!Cyl || !Cone || !Sph) return;
	auto Make = [&](UStaticMesh* Mesh, const FLinearColor& C, double Rough)
	{
		UInstancedStaticMeshComponent* I = NewObject<UInstancedStaticMeshComponent>(this);
		I->SetStaticMesh(Mesh);
		I->SetupAttachment(Root);
		I->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		I->RegisterComponent();
		if (UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(AKeysWorld::SolidMaterial(), this))
		{
			MID->SetVectorParameterValue(TEXT("Color"), C);
			MID->SetScalarParameterValue(TEXT("Roughness"), float(Rough));
			I->SetMaterial(0, MID);
		}
		BuoyISMs.Add(I);
		return I;
	};
	UInstancedStaticMeshComponent* RedBody = Make(Cyl, Hex(0xb8322a), .6);
	UInstancedStaticMeshComponent* RedTop = Make(Cone, Hex(0xb8322a), .6);
	UInstancedStaticMeshComponent* GreenBody = Make(Cyl, Hex(0x1f7a46), .6);
	UInstancedStaticMeshComponent* Mast = Make(Cyl, Hex(0x22262b), .7);
	UInstancedStaticMeshComponent* Band = Make(Cyl, Hex(0xe8edf0), .3);
	UInstancedStaticMeshComponent* Ball = Make(Sph, Hex(0xf4f4f2), .4);
	for (const FMark& K : Marks)
	{
		(K.bRed ? RedBody : GreenBody)->AddInstance(FTransform::Identity);
		if (K.bRed) RedTop->AddInstance(FTransform::Identity);
		Mast->AddInstance(FTransform::Identity);
		Band->AddInstance(FTransform::Identity);
	}
	for (int32 i = 0; i < Balls.Num(); i++) Ball->AddInstance(FTransform::Identity);
	BobT = -1;
	Tick(0);
}

void AKeysLandmarks::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (BuoyISMs.Num() < 6) return;
	BobT -= DeltaSeconds;
	if (BobT > 0) return;
	BobT = 1.0 / 20;
	const double T = GetWorld()->GetTimeSeconds();
	UInstancedStaticMeshComponent* RedBody = BuoyISMs[0];
	UInstancedStaticMeshComponent* RedTop = BuoyISMs[1];
	UInstancedStaticMeshComponent* GreenBody = BuoyISMs[2];
	UInstancedStaticMeshComponent* Mast = BuoyISMs[3];
	UInstancedStaticMeshComponent* Band = BuoyISMs[4];
	UInstancedStaticMeshComponent* Ball = BuoyISMs[5];
	APlayerCameraManager* Cam = UGameplayStatics::GetPlayerCameraManager(this, 0);
	const FVector CamP = Cam ? Cam->GetCameraLocation() / 100 : FVector::ZeroVector;
	int32 RI = 0, RT = 0, GI = 0, MI = 0;
	for (int32 k = 0; k < Marks.Num(); k++)
	{
		const FMark& K = Marks[k];
		const bool bNear = FMath::Square(K.X - CamP.X) + FMath::Square(K.Z - CamP.Y) < 1500.0 * 1500.0;
		double Y = 0;
		FRotator R = FRotator::ZeroRotator;
		if (bNear)
		{
			// ride the swell and heel downhill off a crest (the slope from the same height field, two metres either side)
			const double Amp = KeysGeo::AmpAt(K.X, K.Z);
			Y = KeysGeo::WaveH(K.X, K.Z, T, Amp, false);
			const double SX = (KeysGeo::WaveH(K.X + 2, K.Z, T, Amp, false) - KeysGeo::WaveH(K.X - 2, K.Z, T, Amp, false)) / 4;
			const double SZ = (KeysGeo::WaveH(K.X, K.Z + 2, T, Amp, false) - KeysGeo::WaveH(K.X, K.Z - 2, T, Amp, false)) / 4;
			R = FRotator(FMath::RadiansToDegrees(FMath::Atan(SX)) * .8, 0, -FMath::RadiansToDegrees(FMath::Atan(SZ)) * .8);
		}
		const FVector Base = KeysGeo::ToUE(K.X, Y, K.Z);
		const FQuat Q = R.Quaternion();
		auto Put = [&](UInstancedStaticMeshComponent* I, int32 Idx, double Up, const FVector& Scale)
		{
			I->UpdateInstanceTransform(Idx, FTransform(Q, Base + Q.RotateVector(FVector(0, 0, Up * 100)), Scale), true, false, true);
		};
		if (K.bRed) { Put(RedBody, RI++, .35, FVector(1.5, 1.5, 1.4)); Put(RedTop, RT++, 1.55, FVector(1.5, 1.5, 1.0)); }
		else Put(GreenBody, GI++, .6, FVector(1.4, 1.4, 1.9));
		Put(Mast, MI, 2.6, FVector(.1, .1, 1.6));
		Put(Band, MI, K.bRed ? .75 : 1.2, FVector(K.bRed ? 1.52 : 1.42, K.bRed ? 1.52 : 1.42, .12));
		MI++;
	}
	for (int32 i = 0; i < Balls.Num(); i++)
	{
		const double Amp = KeysGeo::AmpAt(Balls[i].X, Balls[i].Y);
		const double Y = KeysGeo::WaveH(Balls[i].X, Balls[i].Y, T, Amp, false);
		Ball->UpdateInstanceTransform(i, FTransform(FRotator::ZeroRotator, KeysGeo::ToUE(Balls[i].X, Y + .2, Balls[i].Y), FVector(1.1)), true, false, true);
	}
	for (UInstancedStaticMeshComponent* I : BuoyISMs) I->MarkRenderStateDirty();
}
