#include "KeysCanals.h"
#include "KeysGeo.h"
#include "KeysGeoReal.h"
#include "KeysMesh.h"
#include "KeysWorld.h"
#include "ProceduralMeshComponent.h"

namespace
{
	const double Step = 3;            // the wall is traced on a grid this fine
	const double Tile = 600;
	const double WallTop = .98, Lawn = .92, WallFoot = -2.7, CapW = .45, LawnReach = 16;
	FLinearColor Srgb(double R, double G, double B) { return FLinearColor(FMath::Pow(R, 2.2), FMath::Pow(G, 2.2), FMath::Pow(B, 2.2)); }
}

AKeysCanals::AKeysCanals()
{
	PrimaryActorTick.bCanEverTick = false;
	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);
}

void AKeysCanals::BeginPlay()
{
	Super::BeginPlay();
	KeysGeo::Init();
	const TArray<KeysGeoReal::FCanal>& Canals = KeysGeoReal::Canals();
	if (!KeysGeo::RealMap() || !Canals.Num() || FParse::Param(FCommandLine::Get(), TEXT("KRNoCanals")))
	{
		UE_LOG(LogTemp, Log, TEXT("KRCanals: none on this map"));
		return;
	}
	const double T0 = FPlatformTime::Seconds();
	// the tiles a canal comes near
	TSet<int64> Tiles;
	double Metres = 0;
	for (const KeysGeoReal::FCanal& C : Canals)
		for (int32 i = 0; i + 1 < C.Pts.Num(); i++)
		{
			const FVector2D A = C.Pts[i], B = C.Pts[i + 1];
			Metres += (B - A).Size();
			const double Pad = C.Width / 2 + LawnReach + Step;
			for (int32 TX = FMath::FloorToInt((FMath::Min(A.X, B.X) - Pad) / Tile); TX <= FMath::FloorToInt((FMath::Max(A.X, B.X) + Pad) / Tile); TX++)
				for (int32 TZ = FMath::FloorToInt((FMath::Min(A.Y, B.Y) - Pad) / Tile); TZ <= FMath::FloorToInt((FMath::Max(A.Y, B.Y) + Pad) / Tile); TZ++)
					Tiles.Add((int64(TX) << 32) ^ int64(uint32(TZ)));
		}
	const FLinearColor Conc = Srgb(.72, .70, .65), Weed = Srgb(.27, .27, .2), Sand = Srgb(.91, .86, .71), Grass = Srgb(.43, .56, .28), Scrub = Srgb(.29, .44, .21);
	int32 Tris = 0, WallSegs = 0;
	const int32 N = FMath::RoundToInt(Tile / Step);
	TArray<double> F;
	TArray<uint8> Bank;
	for (int64 Key : Tiles)
	{
		const int32 TX = int32(Key >> 32), TZ = int32(uint32(Key & 0xffffffff));
		const double OX = TX * Tile, OZ = TZ * Tile;
		F.SetNumUninitialized((N + 1) * (N + 1));
		Bank.SetNumUninitialized((N + 1) * (N + 1));
		bool bAny = false;
		for (int32 j = 0; j <= N; j++)
			for (int32 i = 0; i <= N; i++)
			{
				double D;
				Bank[j * (N + 1) + i] = KeysGeoReal::CanalBank(OX + i * Step, OZ + j * Step, D) ? 1 : 0;
				F[j * (N + 1) + i] = D;
				bAny |= D < LawnReach;
			}
		if (!bAny) continue;
		const FVector Org = KeysGeo::ToUE(OX + Tile / 2, 0, OZ + Tile / 2);
		// (meshes are authored in metres about the tile's middle, in Unreal's axes: x east, y south, z up)
		auto P = [&](double X, double Y, double Z) { return FVector(X - OX - Tile / 2, Z - OZ - Tile / 2, Y); };
		FKeysMesh Wall, Ground;
		Wall.Surf = SurfConcrete;
		Ground.Surf = SurfSand;
		for (int32 j = 0; j < N; j++)
			for (int32 i = 0; i < N; i++)
			{
				const int32 K = j * (N + 1) + i;
				const double Fv[4] = {F[K], F[K + 1], F[K + N + 2], F[K + N + 1]};     // round the cell
				const double Lo = FMath::Min(FMath::Min(Fv[0], Fv[1]), FMath::Min(Fv[2], Fv[3])), Hi = FMath::Max(FMath::Max(Fv[0], Fv[1]), FMath::Max(Fv[2], Fv[3]));
				if (Hi < 0 || Lo >= LawnReach || Hi > 1e8) continue;
				if (!(Bank[K] && Bank[K + 1] && Bank[K + N + 2] && Bank[K + N + 1])) continue;   // a mouth: no wall across it
				const FVector2D Cn[4] = {FVector2D(OX + i * Step, OZ + j * Step), FVector2D(OX + (i + 1) * Step, OZ + j * Step), FVector2D(OX + (i + 1) * Step, OZ + (j + 1) * Step), FVector2D(OX + i * Step, OZ + (j + 1) * Step)};
				// the part of the cell on the land side of the wall, and where the wall crosses its edges
				FVector2D Poly[8], Cross[4];
				int32 NP = 0, NC = 0;
				for (int32 a = 0; a < 4; a++)
				{
					const int32 b = (a + 1) & 3;
					if (Fv[a] >= 0) Poly[NP++] = Cn[a];
					if ((Fv[a] >= 0) != (Fv[b] >= 0))
					{
						const FVector2D Q = FMath::Lerp(Cn[a], Cn[b], Fv[a] / (Fv[a] - Fv[b]));
						Poly[NP++] = Q;
						if (NC < 4) Cross[NC++] = Q;
					}
				}
				if (NP >= 3)
				{
					const FVector2D Mid = (Cn[0] + Cn[2]) / 2;
					const double Nz = .5 + .3 * FMath::Sin(Mid.X * .013) * FMath::Cos(Mid.Y * .011) + .2 * FMath::Sin((Mid.X + Mid.Y) * .031);
					const FLinearColor Col = FMath::Lerp(Sand, Grass, FMath::Lerp(.9f, .97f, float(Nz)));        // a mown strip along the wall
					for (int32 a = 1; a + 1 < NP; a++)
						Ground.AddTri(P(Poly[0].X, Lawn, Poly[0].Y), P(Poly[a].X, Lawn, Poly[a].Y), P(Poly[a + 1].X, Lawn, Poly[a + 1].Y), Col, FVector(0, 0, 1));
				}
				if (NC == 2)
				{
					// the wall: its face to the water, weed below the tide line, and a cap
					const FVector2D A = Cross[0], B = Cross[1], Mid = (A + B) / 2;
					FVector2D In = FVector2D(-(B - A).Y, (B - A).X).GetSafeNormal();        // toward the land
					double Dl, Dw;
					KeysGeoReal::CanalBank(Mid.X + In.X, Mid.Y + In.Y, Dl);
					KeysGeoReal::CanalBank(Mid.X - In.X, Mid.Y - In.Y, Dw);
					if (Dl < Dw) In = -In;
					const FVector Out(-In.X, -In.Y, 0);
					Wall.AddQuad(P(A.X, -.15, A.Y), P(B.X, -.15, B.Y), P(B.X, WallTop, B.Y), P(A.X, WallTop, A.Y), Conc, Out);
					Wall.AddQuad(P(A.X, WallFoot, A.Y), P(B.X, WallFoot, B.Y), P(B.X, -.15, B.Y), P(A.X, -.15, A.Y), Weed, Out);
					const FVector2D A2 = A + In * CapW, B2 = B + In * CapW;
					Wall.AddQuad(P(A.X, WallTop, A.Y), P(B.X, WallTop, B.Y), P(B2.X, WallTop, B2.Y), P(A2.X, WallTop, A2.Y), Conc, FVector(0, 0, 1));
					Wall.AddQuad(P(A2.X, Lawn - .05, A2.Y), P(B2.X, Lawn - .05, B2.Y), P(B2.X, WallTop, B2.Y), P(A2.X, WallTop, A2.Y), Conc, -Out);
					WallSegs++;
				}
			}
		// the lawn takes the terrain's own ground shader, on the terrain's own mapping
		for (int32 v = 0; v < Ground.V.Num(); v++) Ground.UV[v] = FVector2D((Ground.V[v].X / 100 + OX + Tile / 2) / 20, (Ground.V[v].Y / 100 + OZ + Tile / 2) / 20);
		for (FKeysMesh* M : {&Wall, &Ground})
		{
			if (!M->T.Num()) continue;
			UProceduralMeshComponent* C = NewObject<UProceduralMeshComponent>(this);
			C->SetupAttachment(Root);
			C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			C->bUseAsyncCooking = true;
			C->SetCastShadow(false);
			C->RegisterComponent();
			C->SetRelativeLocation(Org);
			C->SetCachedMaxDrawDistance(300000.f);
			M->Commit(C, 0, AKeysWorld::VertexColorMaterial());
			Meshes.Add(C);
			Tris += M->T.Num() / 3;
		}
	}
	UE_LOG(LogTemp, Log, TEXT("KRCanals built: %d canals, %.0f m of centre line, %.0f m of seawall, %d meshes, %d tris, %.2f s"),
		Canals.Num(), Metres, WallSegs * Step, Meshes.Num(), Tris, FPlatformTime::Seconds() - T0);
}
