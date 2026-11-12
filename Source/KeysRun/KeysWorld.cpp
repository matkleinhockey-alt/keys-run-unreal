#include "KeysWorld.h"
#include "KeysGeo.h"
#include "ProceduralMeshComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/SkyLightComponent.h"
#include "Components/SkyAtmosphereComponent.h"
#include "Components/ExponentialHeightFogComponent.h"
#include "Components/VolumetricCloudComponent.h"
#include "Materials/Material.h"
#include "MaterialDomain.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Engine/StaticMesh.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "Async/ParallelFor.h"

namespace
{
	FLinearColor Srgb(double R, double G, double B) { return FLinearColor(FColor(uint8(R * 255), uint8(G * 255), uint8(B * 255))); }
	double Hash2(double X, double Z) { const double H = FMath::Sin(X * 127.1 + Z * 311.7) * 43758.5453; return H - FMath::FloorToDouble(H); }
	double SStep(double A, double B, double X) { const double T = FMath::Clamp((X - A) / (B - A), 0.0, 1.0); return T * T * (3 - 2 * T); }

	UMaterialInterface* LoadMat(const TCHAR* Path)
	{
		UMaterialInterface* M = LoadObject<UMaterialInterface>(nullptr, Path);
		if (!M)
		{
			UE_LOG(LogTemp, Warning, TEXT("Keys Run: material %s is missing - restart the editor once so Content/Python/init_unreal.py can create it."), Path);
			M = UMaterial::GetDefaultMaterial(MD_Surface);
		}
		return M;
	}
}

UMaterialInterface* AKeysWorld::VertexColorMaterial() { static TWeakObjectPtr<UMaterialInterface> M; if (!M.IsValid()) M = LoadMat(TEXT("/Game/KeysRun/Materials/M_KR_VertexColor.M_KR_VertexColor")); return M.Get(); }
UMaterialInterface* AKeysWorld::WaterMaterial()
{
	// M_KR_Ocean is the detailed sea (ripples, broken-up foam, light through the crests); M_KR_Water is the old flat one
	static TWeakObjectPtr<UMaterialInterface> M;
	if (!M.IsValid()) M = LoadMat(TEXT("/Game/KeysRun/Materials/M_KR_Ocean.M_KR_Ocean"));
	if (!M.IsValid()) M = LoadMat(TEXT("/Game/KeysRun/Materials/M_KR_Water.M_KR_Water"));
	return M.Get();
}
UMaterialInterface* AKeysWorld::SolidMaterial() { static TWeakObjectPtr<UMaterialInterface> M; if (!M.IsValid()) M = LoadMat(TEXT("/Game/KeysRun/Materials/M_KR_Solid.M_KR_Solid")); return M.Get(); }

AKeysWorld::AKeysWorld()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickGroup = TG_PostPhysics;   // after the boat has moved and dropped its wake

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent = Root;

	Terrain = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("Terrain"));
	Terrain->SetupAttachment(Root);
	Terrain->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	Water = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("Water"));
	Water->SetupAttachment(Root);
	Water->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Water->SetCastShadow(false);

	Sun = CreateDefaultSubobject<UDirectionalLightComponent>(TEXT("Sun"));
	Sun->SetupAttachment(Root);
	Sun->SetMobility(EComponentMobility::Movable);

	SkyLight = CreateDefaultSubobject<USkyLightComponent>(TEXT("SkyLight"));
	SkyLight->SetupAttachment(Root);
	SkyLight->SetMobility(EComponentMobility::Movable);
	SkyLight->bRealTimeCapture = true;
	SkyLight->SourceType = ESkyLightSourceType::SLS_CapturedScene;

	Atmosphere = CreateDefaultSubobject<USkyAtmosphereComponent>(TEXT("Atmosphere"));
	Atmosphere->SetupAttachment(Root);

	Fog = CreateDefaultSubobject<UExponentialHeightFogComponent>(TEXT("Fog"));
	Fog->SetupAttachment(Root);

	Clouds = CreateDefaultSubobject<UVolumetricCloudComponent>(TEXT("Clouds"));
	Clouds->SetupAttachment(Root);
}

void AKeysWorld::BeginPlay()
{
	Super::BeginPlay();
	KeysGeo::Init();

	Sun->SetAtmosphereSunLight(true);
	Sun->SetIntensity(9.f);
	Sun->SetDynamicShadowCascades(4);
	SkyLight->SetIntensity(1.f);
	Fog->SetFogDensity(.004f);
	Fog->SetFogHeightFalloff(.15f);
	Fog->SetFogInscatteringColor(FLinearColor(.45f, .62f, .75f));
	if (UMaterialInterface* CloudMat = LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/EngineSky/VolumetricClouds/m_SimpleVolumetricCloud_Inst.m_SimpleVolumetricCloud_Inst")))
		Clouds->SetMaterial(CloudMat);
	ApplySun(0);

	BuildTerrain();
	BuildBridges();
	BuildScenery();
	BuildMarinas();
	UpdateWater();
}

UInstancedStaticMeshComponent* AKeysWorld::MakeISM(const TCHAR* Name, UStaticMesh* Mesh, const FLinearColor& Color, double Roughness)
{
	UInstancedStaticMeshComponent* C = NewObject<UInstancedStaticMeshComponent>(this, Name);
	C->SetStaticMesh(Mesh);
	C->SetupAttachment(Root);
	C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	C->RegisterComponent();
	if (UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(SolidMaterial(), this))
	{
		MID->SetVectorParameterValue(TEXT("Color"), Color);
		MID->SetScalarParameterValue(TEXT("Roughness"), float(Roughness));
		C->SetMaterial(0, MID);
	}
	ISMs.Add(C);
	return C;
}

// ---------------------------------------------------------------- terrain
void AKeysWorld::BuildTerrain()
{
	using namespace KeysGeo;
	const double Cell = 8;
	const int32 CH = 64;
	const int32 NC = FMath::CeilToInt(WorldSize / Cell / CH);
	const int32 GN = NC * CH + 1;
	TArray<float> HG;
	TArray<uint8> Kind;
	HG.SetNumUninitialized(GN * GN);
	Kind.SetNumZeroed(GN * GN);
	ParallelFor(GN, [&](int32 j)
	{
		for (int32 i = 0; i < GN; i++)
		{
			const double X = WorldX0 + i * Cell, Z = WorldZ0 + j * Cell, H = LandH(X, Z);
			HG[j * GN + i] = float(H);
			if (H > -1) Kind[j * GN + i] = uint8(SurfaceKind(X, Z));
		}
	});

	const FLinearColor WetSand = Srgb(.74, .69, .53), Sand = Srgb(.91, .86, .71), Grass = Srgb(.43, .56, .28), Scrub = Srgb(.29, .44, .21),
		Mangrove = Srgb(.16, .31, .15), Golf = Srgb(.42, .67, .3), Runway = Srgb(.33, .33, .34), City = Srgb(.55, .54, .52);
	auto ColorAt = [&](int32 i, int32 j)
	{
		const double X = WorldX0 + i * Cell, Z = WorldZ0 + j * Cell, H = HG[j * GN + i];
		switch (Kind[j * GN + i])
		{
			case 1: return H > .9 ? Golf : Sand;
			case 2: return H > .9 ? Runway : Sand;
			case 3: return H > .9 ? City : Sand;
			default: break;
		}
		if (H < .35) return FMath::Lerp(WetSand, Sand, float(SStep(-1.2, .35, H)));
		const double N = .5 + .3 * FMath::Sin(X * .013) * FMath::Cos(Z * .011) + .2 * FMath::Sin((X + Z) * .031);
		const FLinearColor Veg = N > .62 ? Mangrove : N > .42 ? Scrub : Grass;
		return FMath::Lerp(Sand, Veg, float(SStep(.35, .95, H)));
	};

	int32 Section = 0;
	for (int32 CY = 0; CY < NC; CY++)
		for (int32 CX = 0; CX < NC; CX++)
		{
			bool bAny = false;
			for (int32 j = CY * CH; j <= (CY + 1) * CH && !bAny; j++)
				for (int32 i = CX * CH; i <= (CX + 1) * CH; i++)
					if (HG[j * GN + i] > -1.0f) { bAny = true; break; }
			if (!bAny) continue;

			TArray<FVector> V;
			TArray<FVector> N;
			TArray<FVector2D> UV;
			TArray<FLinearColor> C;
			TArray<int32> T;
			const int32 S = CH + 1;
			V.Reserve(S * S); N.Reserve(S * S); UV.Reserve(S * S); C.Reserve(S * S);
			for (int32 b = 0; b <= CH; b++)
				for (int32 a = 0; a <= CH; a++)
				{
					const int32 i = CX * CH + a, j = CY * CH + b;
					const double X = WorldX0 + i * Cell, Z = WorldZ0 + j * Cell, H = HG[j * GN + i];
					const int32 i0 = FMath::Max(i - 1, 0), i1 = FMath::Min(i + 1, GN - 1), j0 = FMath::Max(j - 1, 0), j1 = FMath::Min(j + 1, GN - 1);
					const double DX = (HG[j * GN + i1] - HG[j * GN + i0]) / ((i1 - i0) * Cell), DZ = (HG[j1 * GN + i] - HG[j0 * GN + i]) / ((j1 - j0) * Cell);
					V.Add(ToUE(X, H, Z));
					N.Add(FVector(-DX, -DZ, 1).GetSafeNormal());
					UV.Add(FVector2D(X / 20, Z / 20));
					C.Add(ColorAt(i, j));
				}
			for (int32 b = 0; b < CH; b++)
				for (int32 a = 0; a < CH; a++)
				{
					const int32 K = b * S + a;
					const float H0 = HG[(CY * CH + b) * GN + CX * CH + a], H1 = HG[(CY * CH + b) * GN + CX * CH + a + 1];
					const float H2 = HG[(CY * CH + b + 1) * GN + CX * CH + a], H3 = HG[(CY * CH + b + 1) * GN + CX * CH + a + 1];
					if (FMath::Max(FMath::Max(H0, H1), FMath::Max(H2, H3)) < -1.5f) continue;
					T.Append({K, K + S, K + 1, K + 1, K + S, K + S + 1});
				}
			Terrain->CreateMeshSection_LinearColor(Section, V, T, N, UV, C, TArray<FProcMeshTangent>(), false);
			Terrain->SetMaterial(Section, VertexColorMaterial());
			Section++;
		}
	UE_LOG(LogTemp, Log, TEXT("Keys Run: terrain built with %d chunks"), Section);
}

// ---------------------------------------------------------------- bridges
void AKeysWorld::BuildBridges()
{
	using namespace KeysGeo;
	UStaticMesh* Cube = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
	UStaticMesh* Cyl = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	if (!Cube || !Cyl) return;
	UInstancedStaticMeshComponent* Deck = MakeISM(TEXT("BridgeDeck"), Cube, Srgb(.8, .79, .75), .9);
	UInstancedStaticMeshComponent* Rail = MakeISM(TEXT("BridgeRail"), Cube, Srgb(.88, .87, .84), .9);
	UInstancedStaticMeshComponent* OldDeck = MakeISM(TEXT("OldBridgeDeck"), Cube, Srgb(.62, .58, .52), .95);
	UInstancedStaticMeshComponent* Piles = MakeISM(TEXT("Pilings"), Cyl, Srgb(.72, .7, .66), .95);

	const double Seg = 30, DeckY = 6, OldY = 4.6;
	auto Span = [&](UInstancedStaticMeshComponent* ISM, double X0, double DZ, double Y, double W, bool bRails)
	{
		const double XC = X0 + Seg * .5, Z = ChainZ(XC) + DZ;
		const double Slope = 0.000024 * XC;
		if (LandH(XC, Z) > -.2 && LandH(X0, ChainZ(X0) + DZ) > -.2 && LandH(X0 + Seg, ChainZ(X0 + Seg) + DZ) > -.2) return;
		const double Len = Seg * FMath::Sqrt(1 + Slope * Slope) + .3;
		const FRotator Rot(0, FMath::RadiansToDegrees(FMath::Atan(Slope)), 0);
		ISM->AddInstance(FTransform(Rot, ToUE(XC, Y, Z), FVector(Len, W, .9)));
		if (bRails)
		{
			const FVector Side = Rot.RotateVector(FVector(0, 1, 0));
			for (int32 s = -1; s <= 1; s += 2)
				Rail->AddInstance(FTransform(Rot, ToUE(XC, Y + .85, Z) + Side * s * (W * 50 - 20), FVector(Len, .3, .8)));
		}
	};
	for (double X = WBX0; X < WBX1; X += Seg) Span(Deck, X, 0, DeckY, 11, true);
	for (double X = -3820; X < -2010; X += Seg)
	{
		if (X + Seg > -3200 && X < -3080) continue;   // the swing span is gone
		Span(OldDeck, X, -58, OldY, 7.5, false);
	}
	for (const FPillar& P : Pillars())
	{
		const double Top = P.bOld ? OldY : DeckY, Bot = -4;
		Piles->AddInstance(FTransform(FRotator::ZeroRotator, ToUE(P.X, (Top + Bot) * .5, P.Z), FVector(P.R * 2, P.R * 2, Top - Bot)));
	}
}

// ---------------------------------------------------------------- palms, houses, Miami
void AKeysWorld::BuildScenery()
{
	using namespace KeysGeo;
	UStaticMesh* Cube = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
	UStaticMesh* Cyl = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	UStaticMesh* Sph = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	UStaticMesh* Cone = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cone.Cone"));
	if (!Cube || !Cyl || !Sph) return;

	UInstancedStaticMeshComponent* Trunks = MakeISM(TEXT("PalmTrunks"), Cyl, Srgb(.52, .43, .32), .95);
	UInstancedStaticMeshComponent* Fronds = MakeISM(TEXT("PalmFronds"), Cone ? Cone : Sph, Srgb(.24, .45, .2), .85);
	UInstancedStaticMeshComponent* Mangroves = MakeISM(TEXT("Mangroves"), Sph, Srgb(.14, .29, .13), .95);
	TArray<UInstancedStaticMeshComponent*> Houses = {
		MakeISM(TEXT("HousePink"), Cube, Srgb(.95, .72, .72), .8), MakeISM(TEXT("HouseMint"), Cube, Srgb(.68, .9, .8), .8),
		MakeISM(TEXT("HouseYellow"), Cube, Srgb(.97, .9, .62), .8), MakeISM(TEXT("HouseWhite"), Cube, Srgb(.95, .95, .93), .8),
		MakeISM(TEXT("HouseBlue"), Cube, Srgb(.62, .8, .93), .8)};
	UInstancedStaticMeshComponent* Roofs = MakeISM(TEXT("Roofs"), Cube, Srgb(.85, .86, .88), .6);
	UInstancedStaticMeshComponent* Glass = MakeISM(TEXT("Towers"), Cube, Srgb(.42, .55, .66), .15);
	UInstancedStaticMeshComponent* Concrete = MakeISM(TEXT("TowersWhite"), Cube, Srgb(.88, .88, .86), .5);

	// palms and mangroves (kept out of creeks, off the beach, and out of downtown)
	const double PS = 16;
	for (double Z = WorldZ0; Z < WorldZ0 + WorldSize; Z += PS)
		for (double X = WorldX0; X < WorldX0 + WorldSize; X += PS)
		{
			const double JX = X + (Hash2(X, Z) - .5) * PS, JZ = Z + (Hash2(Z, X) - .5) * PS;
			const double H = LandH(JX, JZ);
			if (H < .2 || H > 2.6) continue;
			const double R = Hash2(JX * 1.3, JZ * .7);
			const FShore S = ShoreInfo(JX, JZ);
			const double Density = S.Isl->bMainland ? .05 : S.Isl->bMiami ? .2 : .4;
			if (R > Density || InCity(JX, JZ) || SurfaceKind(JX, JZ) != 0 || CreekDist(JX, JZ) < 8) continue;
			if (H < .9 && S.E > .93 && !S.Isl->bMiami)
			{
				// mangrove fringe at the water's edge
				const double Sz = 3 + R * 8;
				Mangroves->AddInstance(FTransform(FRotator(0, R * 360, 0), ToUE(JX, H + .6, JZ), FVector(Sz, Sz * .9, 2.4)));
				continue;
			}
			if (H < .7) continue;
			const double Tall = 6 + Hash2(JZ, JX * 2) * 5;
			const double Lean = (Hash2(JX * 3, JZ) - .5) * 14;
			const FRotator Rot(Lean, Hash2(JX, JZ * 3) * 360, 0);
			const FVector Base = ToUE(JX, H, JZ);
			const FVector Up = Rot.RotateVector(FVector(0, 0, 1));
			Trunks->AddInstance(FTransform(Rot, Base + Up * Tall * 50, FVector(.32, .32, Tall)));
			Fronds->AddInstance(FTransform(FRotator(0, Rot.Yaw, 0), Base + Up * Tall * 100 + FVector(0, 0, 30), FVector(5.5, 5.5, 1.2)));
		}

	// conch houses on the keys, aligned to each island's streets
	const double HS = 34;
	for (double Z = WorldZ0; Z < WorldZ0 + WorldSize; Z += HS)
		for (double X = WorldX0; X < WorldX0 + WorldSize; X += HS)
		{
			const double JX = X + (Hash2(X * .7, Z) - .5) * 10, JZ = Z + (Hash2(Z, X * .7) - .5) * 10;
			const double H = LandH(JX, JZ);
			if (H < 1.15) continue;
			const FShore S = ShoreInfo(JX, JZ);
			if (S.Isl->bNoHouses || S.Isl->bSmall || InCity(JX, JZ) || SurfaceKind(JX, JZ) != 0 || CreekDist(JX, JZ) < 15) continue;
			const double R = Hash2(JX * 2.1, JZ * 1.7);
			if (R > (S.Isl->bMainland ? .12 : .5)) continue;
			const double W = 8 + R * 8, D = 9 + Hash2(JZ, JX) * 7, Ht = 3.6 + (R > .35 ? 3 : 0);
			const FRotator Rot(0, FMath::RadiansToDegrees(S.Isl->Th), 0);
			Houses[int32(Hash2(JX, JZ * 5) * Houses.Num()) % Houses.Num()]->AddInstance(FTransform(Rot, ToUE(JX, H + Ht * .5, JZ), FVector(W, D, Ht)));
			Roofs->AddInstance(FTransform(Rot, ToUE(JX, H + Ht + .2, JZ), FVector(W + .8, D + .8, .4)));
		}

	// Miami skyline: Brickell and downtown towers on the mainland
	const double TS = 60;
	for (double Z = -3580; Z < -2580; Z += TS)
		for (double X = 300; X < 3880; X += TS)
		{
			const double JX = X + (Hash2(X, Z * 1.1) - .5) * 20, JZ = Z + (Hash2(Z * 1.1, X) - .5) * 20;
			const double H = LandH(JX, JZ);
			if (H < 1 || CreekDist(JX, JZ) < 25) continue;
			const double R = Hash2(JX * .3, JZ * .9);
			if (R > .55) continue;
			const double Core = FMath::Exp(-FMath::Square((JX - 2050) / 700) - FMath::Square((JZ + 2800) / 450));
			const double Ht = 18 + R * 60 + Core * (60 + Hash2(JZ, JX) * 120);
			const double W = 22 + Hash2(JX, JZ) * 18;
			(R > .3 ? Glass : Concrete)->AddInstance(FTransform(FRotator::ZeroRotator, ToUE(JX, H + Ht * .5, JZ), FVector(W, W * (.7 + R), Ht)));
		}
}

// ---------------------------------------------------------------- marinas
void AKeysWorld::BuildMarinas()
{
	using namespace KeysGeo;
	UStaticMesh* Cube = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
	UStaticMesh* Cyl = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	if (!Cube || !Cyl) return;
	UInstancedStaticMeshComponent* Planks = MakeISM(TEXT("DockPlanks"), Cube, Srgb(.6, .5, .38), .95);
	UInstancedStaticMeshComponent* Posts = MakeISM(TEXT("DockPosts"), Cyl, Srgb(.4, .33, .25), .95);
	UInstancedStaticMeshComponent* Sheds = MakeISM(TEXT("DockSheds"), Cube, Srgb(.95, .93, .88), .7);
	for (const FMarina& M : Marinas())
	{
		const double Z0 = M.SZ - M.Dir * 6, Z1 = M.EZ, ZC = (Z0 + Z1) * .5, Len = FMath::Abs(Z1 - Z0);
		Planks->AddInstance(FTransform(FRotator::ZeroRotator, ToUE(M.SX, 1.1, ZC), FVector(3.2, Len, .25)));
		// T-head at the end
		Planks->AddInstance(FTransform(FRotator::ZeroRotator, ToUE(M.SX, 1.1, Z1), FVector(18, 3.2, .25)));
		for (double Z = FMath::Min(Z0, Z1); Z <= FMath::Max(Z0, Z1); Z += 6)
			for (int32 s = -1; s <= 1; s += 2)
				Posts->AddInstance(FTransform(FRotator::ZeroRotator, ToUE(M.SX + s * 1.7, -.6, Z), FVector(.28, .28, 4)));
		Sheds->AddInstance(FTransform(FRotator::ZeroRotator, ToUE(M.SX + 6, 3, M.SZ - M.Dir * 12), FVector(8, 6, 4)));
	}
}

// ---------------------------------------------------------------- water
void AKeysWorld::UpdateWater()
{
	using namespace KeysGeo;
	const int32 S = WN + 1;
	if (!bWaterBuilt)
	{
		WGrid.SetNum(S);
		// about 0.9 m between vertices around the boat (so wakes keep their shape), stretching out to ~200 m at the horizon
		const double C0 = .025;
		for (int32 i = 0; i < S; i++)
		{
			const double U = -1 + 2.0 * i / WN;
			WGrid[i] = U * (C0 + (1 - C0) * FMath::Pow(FMath::Abs(U), 4.0)) * WR;
		}
		WV.SetNum(S * S);
		WN_.SetNum(S * S);
		WUV.SetNum(S * S);
		WC.SetNum(S * S);
		WH.SetNum(S * S);
		TArray<int32> T;
		T.Reserve(WN * WN * 6);
		for (int32 j = 0; j < WN; j++)
			for (int32 i = 0; i < WN; i++)
			{
				const int32 K = j * S + i;
				T.Append({K, K + S, K + 1, K + 1, K + S, K + S + 1});
			}
		Water->CreateMeshSection_LinearColor(0, WV, T, WN_, WUV, WC, TArray<FProcMeshTangent>(), false);
		Water->SetMaterial(0, WaterMaterial());

		// deep blue out past the fine grid, so the horizon never shows a hole
		const double Far = 80000;
		FLinearColor Deep = DepthRGB(600);
		Deep.A = 0;   // alpha carries foam
		TArray<FVector> FV = {ToUE(-Far, -.6, -Far), ToUE(Far, -.6, -Far), ToUE(-Far, -.6, Far), ToUE(Far, -.6, Far)};
		TArray<FVector> FN = {FVector::UpVector, FVector::UpVector, FVector::UpVector, FVector::UpVector};
		TArray<FVector2D> FUV = {FVector2D(0, 0), FVector2D(0, 0), FVector2D(0, 0), FVector2D(0, 0)};   // UV0 drives crest glow and wash; none out here
		TArray<FLinearColor> FC = {Deep, Deep, Deep, Deep};
		const TArray<int32> FT = {0, 2, 1, 1, 2, 3};
		Water->CreateMeshSection_LinearColor(1, FV, FT, FN, FUV, FC, TArray<FProcMeshTangent>(), false);
		Water->SetMaterial(1, WaterMaterial());
		bWaterBuilt = true;
	}

	if (APawn* P = UGameplayStatics::GetPlayerPawn(this, 0))
	{
		const FVector L = P->GetActorLocation();
		FocusX = L.X / 100;
		FocusZ = L.Y / 100;
	}
	// snap the grid to its own spacing near the boat, so the vertices there don't slide over the waves as you move
	const double Snap = FMath::Max(.5, WGrid[S / 2 + 1] - WGrid[S / 2]);
	const double CX = FMath::RoundToDouble(FocusX / Snap) * Snap, CZ = FMath::RoundToDouble(FocusZ / Snap) * Snap;
	const double T = GetWorld()->GetTimeSeconds();
	const double SWv = SeaSW(), CHv = SeaCH();
	const double FX = FocusX, FZ = FocusZ;
	constexpr double NearR = 450;
	GatherWakes(FX, FZ, NearR, T, LiveWakes);
	GatherTrails(FX, FZ, NearR, T, LiveTrails);

	ParallelFor(S, [&](int32 j)
	{
		const double Z = CZ + WGrid[j];
		for (int32 i = 0; i < S; i++)
		{
			const double X = CX + WGrid[i];
			const int32 K = j * S + i;
			const double Amp = AmpAt(X, Z);
			const bool bNear = FMath::Square(X - FX) + FMath::Square(Z - FZ) < NearR * NearR;
			const double Wk = bNear ? WakeHFrom(LiveWakes, X, Z) : 0;
			const FVector2D Tr = bNear ? TrailFrom(LiveTrails, X, Z) : FVector2D::ZeroVector;
			const double Sea = WaveH(X, Z, T, Amp, false);
			// the prop wash knocks down the chop a little, so the trail reads as a smoother, glassier lane
			const double H = Sea * (1 - .25 * Tr.Y) + Wk;
			WH[K] = H;
			WV[K] = ToUE(X, H, Z);
			// foam on breaking crests, on the crests of fresh wakes, in the prop wash and along the beach.
			// Vertex alpha carries the amount; the material breaks it up into real-looking patches.
			const double Crest = Sea / (Amp * (1.74 * SWv + .33 * FMath::Min(CHv, 2.2)) + .001);
			const double D = DepthFast(X, Z);
			double F = SStep(.55, .95, Crest) * .5 * FMath::Clamp(CHv - .4, 0.0, 1.0);
			// white lines along the arms of a fresh wake, where the crests are tallest
			F = FMath::Max(F, SStep(.03, .22, Wk) * .7);
			F = FMath::Max(F, Tr.X);
			F = FMath::Max(F, SStep(.75, .32, D) * .45);
			// UV0: x = how high on a crest (light glows through thin crests), y = aerated wash under the foam
			WUV[K] = FVector2D(FMath::Clamp(Crest * .8 + Wk * .8, 0.0, 1.0), FMath::Clamp(Tr.Y, 0.0, 1.0));
			FLinearColor C = DepthColorFast(X, Z);
			C.A = float(FMath::Clamp(F, 0.0, 1.0));
			WC[K] = C;
		}
	});
	ParallelFor(S, [&](int32 j)
	{
		for (int32 i = 0; i < S; i++)
		{
			const int32 i0 = FMath::Max(i - 1, 0), i1 = FMath::Min(i + 1, WN), j0 = FMath::Max(j - 1, 0), j1 = FMath::Min(j + 1, WN);
			const double DX = (WH[j * S + i1] - WH[j * S + i0]) / FMath::Max(.01, WGrid[i1] - WGrid[i0]);
			const double DZ = (WH[j1 * S + i] - WH[j0 * S + i]) / FMath::Max(.01, WGrid[j1] - WGrid[j0]);
			WN_[j * S + i] = FVector(-DX, -DZ, 1).GetSafeNormal();
		}
	});
	Water->UpdateMeshSection_LinearColor(0, WV, WN_, WUV, WC, TArray<FProcMeshTangent>());
}

// ---------------------------------------------------------------- sky
void AKeysWorld::ApplySun(double K)
{
	// day: high sun to the southeast; sunset: golden hour low over the Seven Mile Bridge to the west
	const FVector Day = KeysGeo::ToUE(.55, .32, -.62).GetSafeNormal(), Dusk = KeysGeo::ToUE(-.96, .055, .2).GetSafeNormal();
	const FVector ToSun = FMath::Lerp(Day, Dusk, K).GetSafeNormal();
	Sun->SetWorldRotation((-ToSun).Rotation());
	Sun->SetLightColor(FMath::Lerp(FLinearColor(1.f, .96f, .9f), FLinearColor(1.f, .72f, .48f), float(K)));
	Fog->SetFogInscatteringColor(FMath::Lerp(FLinearColor(.45f, .62f, .75f), FLinearColor(.75f, .5f, .38f), float(K)));
}

void AKeysWorld::ToggleSunset() { SunsetTarget = SunsetTarget > .5 ? 0 : 1; }

void AKeysWorld::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!FMath::IsNearlyEqual(SunK, SunsetTarget))
	{
		SunK = SunK < SunsetTarget ? FMath::Min(SunsetTarget, SunK + DeltaSeconds * .25) : FMath::Max(SunsetTarget, SunK - DeltaSeconds * .25);
		ApplySun(SStep(0, 1, SunK));
	}
	UpdateWater();
}
