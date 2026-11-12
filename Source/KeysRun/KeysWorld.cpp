#include "KeysWorld.h"
#include "KeysGeo.h"
#include "KeysTownData.h"
#include "KeysTown.h"
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
#include "EngineUtils.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"

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

AKeysWorld* AKeysWorld::Get(const UObject* WorldContext)
{
	UWorld* W = WorldContext ? WorldContext->GetWorld() : nullptr;
	if (!W) return nullptr;
	TActorIterator<AKeysWorld> It(W);
	return It ? *It : nullptr;
}

void AKeysWorld::SetUnderside(bool bOn)
{
	if (bOn == bUnderside || !bWaterBuilt) { bUnderside = bOn; return; }
	bUnderside = bOn;
	Water->SetMeshSectionVisible(2, bOn);
}

void AKeysWorld::DebugOldWater(bool bOld)
{
	UMaterialInterface* M = bOld ? LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/KeysRun/Materials/M_KR_Water.M_KR_Water")) : (WaterMID ? (UMaterialInterface*)WaterMID : WaterMaterial());
	if (!M) return;
	Water->SetMaterial(0, M);
	Water->SetMaterial(1, M);
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
	BuildScenery();
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
					// below the waterline the beach dives steeply, so the sea floor mesh (KeysSeafloor) is what you see through the water
					V.Add(ToUE(X, H < -.25 ? -.25 + (H + .25) * 3 : H, Z));
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
					// under the water the sea floor (KeysSeafloor) takes over, so only the beach is drawn here
					if (FMath::Max(FMath::Max(H0, H1), FMath::Max(H2, H3)) < -.3f) continue;
					T.Append({K, K + S, K + 1, K + 1, K + S, K + S + 1});
				}
			Terrain->CreateMeshSection_LinearColor(Section, V, T, N, UV, C, TArray<FProcMeshTangent>(), false);
			Terrain->SetMaterial(Section, VertexColorMaterial());
			Section++;
		}
	UE_LOG(LogTemp, Log, TEXT("Keys Run: terrain built with %d chunks"), Section);
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

	// palms and conch houses go into vertex-coloured meshes in 1.2 km chunks (so the far ones cull)
	TMap<int64, FTownBatch> HouseChunks;
	auto ChunkOf = [](double X, double Z) { return (int64(FMath::FloorToInt(X / 1200)) << 32) ^ int64(uint32(FMath::FloorToInt(Z / 1200))); };
	static const uint32 FrondC[] = {0x3f8a35, 0x4f9a3c, 0x58a443, 0x467f30};
	UInstancedStaticMeshComponent* Mangroves = MakeISM(TEXT("Mangroves"), Sph, Srgb(.14, .29, .13), .95);
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
			if (R > Density || InCity(JX, JZ) || SurfaceKind(JX, JZ) != 0 || CreekDist(JX, JZ) < 8 || KeysTown::InFootprint(JX, JZ, 6) || FMath::Abs(JZ - ChainZ(JX)) < 9) continue;
			if (H < .9 && S.E > .93 && !S.Isl->bMiami)
			{
				// mangrove fringe at the water's edge
				const double Sz = 3 + R * 8;
				Mangroves->AddInstance(FTransform(FRotator(0, R * 360, 0), ToUE(JX, H + .6, JZ), FVector(Sz, Sz * .9, 2.4)));
				continue;
			}
			if (H < .7) continue;
			// a palm (town props.ts, lighter): a leaning, curving trunk and a crown of fronds that rise and droop
			const double Tall = 6 + Hash2(JZ, JX * 2) * 5;
			const double Lean = (Hash2(JX * 3, JZ) - .5) * .5, LeanA = Hash2(JX, JZ * 3) * 2 * PI;
			FTownBatch& PB = HouseChunks.FindOrAdd(ChunkOf(JX, JZ));
			auto TrunkAt = [&](double F) { return FVector(JX + FMath::Cos(LeanA) * Lean * Tall * .35 * F * F, H + Tall * F, JZ + FMath::Sin(LeanA) * Lean * Tall * .35 * F * F); };
			for (int32 k = 0; k < 4; k++) PB.Tube3(TrunkAt(k / 4.0), TrunkAt((k + 1) / 4.0), .26 - .04 * k, .22 - .04 * k, 5, k % 2 ? 0x8c7758 : 0x7a6648);
			const FVector Top = TrunkAt(1);
			const int32 NF = 9;
			for (int32 k = 0; k < NF; k++)
			{
				const double Ang = k * 2 * PI / NF + R * 7, Len = 3.0 + Hash2(JX + k, JZ) * 1.4;
				const FVector Dir(FMath::Sin(Ang), 0, FMath::Cos(Ang)), Side(FMath::Cos(Ang), 0, -FMath::Sin(Ang));
				const FLinearColor C = KeysTownPaint::Hex(FrondC[(k + int32(R * 9)) % 4]);
				const FVector A = Top, Bm = Top + Dir * Len * .5 + FVector(0, .35, 0), Cd = Top + Dir * Len - FVector(0, .9 + .3 * (k % 2), 0);
				PB.Quad3(A - Side * .15, A + Side * .15, Bm + Side * .5, Bm - Side * .5, FVector(0, 1, 0) + Dir * .2, C);
				PB.Quad3(Bm - Side * .5, Bm + Side * .5, Cd + Side * .08, Cd - Side * .08, FVector(0, 1, 0) + Dir * .5, C * .9f);
			}
		}

	// Conch houses (web world/islands.ts): each with its own footprint, wall height and roof pitch, a hip roof of silver tin,
	// a porch across the front, windows with shutters and a door, and a third of them up on piers.
	static const uint32 Pastel[] = {0xf7c6d0, 0xfde2a7, 0xa8e0d8, 0xb9d4f5, 0xf9f3e3, 0xd8c6f0, 0xffffff, 0xc9e8c2};
	static const uint32 Shutter[] = {0x2bb3a8, 0x1f4f8c, 0xffffff, 0x3f8a5a, 0xe4572e};
	const double HS = 34;
	for (double Z = WorldZ0; Z < WorldZ0 + WorldSize; Z += HS)
		for (double X = WorldX0; X < WorldX0 + WorldSize; X += HS)
		{
			const double JX = X + (Hash2(X * .7, Z) - .5) * 10, JZ = Z + (Hash2(Z, X * .7) - .5) * 10;
			const double H = LandH(JX, JZ);
			if (H < 1.15) continue;
			const FShore S = ShoreInfo(JX, JZ);
			if (S.Isl->bNoHouses || S.Isl->bSmall || InCity(JX, JZ) || SurfaceKind(JX, JZ) != 0 || CreekDist(JX, JZ) < 15 || KeysTown::InFootprint(JX, JZ, 14) || FMath::Abs(JZ - ChainZ(JX)) < 16) continue;
			bool bNearMarina = false;
			for (const FMarina& Mr : Marinas()) bNearMarina |= FMath::Abs(JX - Mr.SX) < 30 && FMath::Abs(JZ - Mr.SZ) < 30;
			if (bNearMarina) continue;
			const double R = Hash2(JX * 2.1, JZ * 1.7);
			if (R > (S.Isl->bMainland ? .12 : .5)) continue;
			// the house's own dice
			const double R1 = Hash2(JX * 3.3, JZ), R2 = Hash2(JZ * 1.9, JX), R3 = Hash2(JX + 79.19, JZ), R4 = Hash2(JX, JZ + 79.19);
			const double SX = FMath::Lerp(.82, 1.3, R1), SZ = FMath::Lerp(.86, 1.26, R2), SY = FMath::Lerp(.88, 1.38, R3);
			const bool bPiers = R4 < .34;
			const double Lift = bPiers ? 1.15 : 0, WallH = 3.4 * SY, Base = H + Lift;
			const double W = 6 * SX * 1.4, D = 7 * SZ * 1.3;
			// three.js yaw: the island's street grid, sometimes turned a quarter
			const double Ry = -S.Isl->Th + (Hash2(JZ, JX * 5) < .5 ? 0 : PI / 2);
			const int64 Key = (int64(FMath::FloorToInt(JX / 1200)) << 32) ^ int64(uint32(FMath::FloorToInt(JZ / 1200)));
			FTownBatch& B = HouseChunks.FindOrAdd(Key);
			const uint32 Wall = Pastel[int32(FMath::Abs(JX * 31 + JZ)) % 8], Trim = Shutter[int32(FMath::Abs(JZ * 17 + JX)) % 5];
			const double Cs = FMath::Cos(Ry), Sn = FMath::Sin(Ry);
			auto At = [&](double LX, double LZ) { return FVector2D(JX + LX * Cs + LZ * Sn, JZ - LX * Sn + LZ * Cs); };
			if (bPiers)
			{
				B.Box(JX, H + Lift / 2 - .3, JZ, W - .4, Lift + .6, D - .4, 0x8e8b82, 0, Ry);
				for (int32 sx = -1; sx <= 1; sx += 2) for (int32 sz = -1; sz <= 1; sz += 2) { const FVector2D Q = At(sx * (W / 2 - .3), sz * (D / 2 - .3)); B.Box(Q.X, H + Lift / 2 - .2, Q.Y, .35, Lift + .4, .35, 0xe8e4d8, 0, Ry); }
			}
			else B.Box(JX, H - .3 + .25, JZ, W + .3, .5 + .6, D + .3, 0xd8d0be, 0, Ry);
			B.Box(JX, Base + WallH / 2, JZ, W, WallH, D, Wall, 0, Ry);
			// the hip roof, silver tin, with a little overhang and its own pitch
			const double Pitch = FMath::Lerp(.82, 1.35, R2) * 1.2, Over = FMath::Lerp(1.0, 1.12, R1);
			B.HipRoof(JX, Base + WallH, JZ, W * Over + .6, D * Over + .6, Pitch * FMath::Min(W, D) * .25, KeysTownPaint::Hex(R3 < .7 ? 0xb8bcc2 : 0xc0603a), Ry);
			// the porch across the front, a porch roof on posts, the door and windows with shutters
			const FVector2D PC = At(0, D / 2 + 1.15);
			B.Box(PC.X, Base + .1, PC.Y, W, .22, 2.3, 0xf4f2ec, 0, Ry);
			for (int32 k = -1; k <= 1; k += 2) { const FVector2D Q = At(k * (W / 2 - .2), D / 2 + 2.1); B.Box(Q.X, Base + 1.3, Q.Y, .14, 2.4, .14, 0xffffff, 0, Ry); }
			const FVector2D PR = At(0, D / 2 + 1.2);
			B.Box(PR.X, Base + 2.55, PR.Y, W + .2, .1, 2.6, 0xb8bcc2, -.12, Ry);
			const FVector2D Dr = At(W * .18, D / 2 + .04);
			B.Box(Dr.X, Base + 1.1, Dr.Y, 1.0, 2.1, .08, Trim, 0, Ry);
			for (double WX : {-W * .25, W * .4})
			{
				if (FMath::Abs(WX - W * .18) < 1.0) continue;
				const FVector2D Q = At(WX, D / 2 + .04);
				B.Box(Q.X, Base + 1.7, Q.Y, 1.1, 1.3, .06, 0x27414f, 0, Ry);
				for (int32 k = -1; k <= 1; k += 2) { const FVector2D Q2 = At(WX + k * .8, D / 2 + .05); B.Box(Q2.X, Base + 1.7, Q2.Y, .4, 1.4, .06, Trim, 0, Ry); }
			}
			for (int32 sd = -1; sd <= 1; sd += 2)
				for (double WZ : {-D * .22, D * .22}) { const FVector2D Q = At(sd * (W / 2 + .03), WZ); B.Box(Q.X, Base + 1.7, Q.Y, .06, 1.2, 1.0, 0x27414f, 0, Ry); }
			const FVector2D Bk = At(-W * .2, -D / 2 - .03);
			B.Box(Bk.X, Base + 1.7, Bk.Y, 1.1, 1.2, .06, 0x27414f, 0, Ry);
		}
	for (TPair<int64, FTownBatch>& C : HouseChunks)
	{
		UProceduralMeshComponent* PM = NewObject<UProceduralMeshComponent>(this);
		PM->SetupAttachment(Root);
		PM->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		PM->RegisterComponent();
		C.Value.M.Commit(PM, 0, VertexColorMaterial());
		PM->SetCachedMaxDrawDistance(260000.f);
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
		WV.Init(FVector::ZeroVector, S * S);
		WN_.Init(FVector::UpVector, S * S);
		WUV.Init(FVector2D::ZeroVector, S * S);
		WC.Init(FLinearColor::Black, S * S);
		WH.Init(0, S * S);
		TArray<int32> T;
		T.Reserve(WN * WN * 6);
		for (int32 j = 0; j < WN; j++)
			for (int32 i = 0; i < WN; i++)
			{
				const int32 K = j * S + i;
				T.Append({K, K + S, K + 1, K + 1, K + S, K + S + 1});
			}
		Water->CreateMeshSection_LinearColor(0, WV, T, WN_, WUV, WC, TArray<FProcMeshTangent>(), false);
		WaterMID = UMaterialInstanceDynamic::Create(WaterMaterial(), this);
		{
			// -KRScat=r,g,b / -KRAbs=r,g,b override the water's scattering / absorption (per meter) for tuning
			auto Vec = [](const TCHAR* Key, FLinearColor& Out)
			{
				FString V;
				if (!FParse::Value(FCommandLine::Get(), Key, V)) return false;
				TArray<FString> P;
				V.ParseIntoArray(P, TEXT(","));
				if (P.Num() < 3) return false;
				Out = FLinearColor(FCString::Atof(*P[0]), FCString::Atof(*P[1]), FCString::Atof(*P[2]), 1);
				return true;
			};
			FLinearColor C;
			if (WaterMID && Vec(TEXT("KRScat="), C)) WaterMID->SetVectorParameterValue(TEXT("Scattering"), C);
			if (WaterMID && Vec(TEXT("KRAbs="), C)) WaterMID->SetVectorParameterValue(TEXT("Absorption"), C);
		}
		Water->SetMaterial(0, WaterMID ? (UMaterialInterface*)WaterMID : WaterMaterial());

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
		Water->SetMaterial(1, WaterMID ? (UMaterialInterface*)WaterMID : WaterMaterial());

		// the underside, seen from below: the same grid wound the other way, with Snell's window in its material
		TArray<int32> TU;
		TU.Reserve(WN * WN * 6);
		for (int32 j = 0; j < WN; j++)
			for (int32 i = 0; i < WN; i++)
			{
				const int32 K = j * S + i;
				TU.Append({K, K + 1, K + S, K + 1, K + S + 1, K + S});
			}
		WNU.Init(-FVector::UpVector, S * S);
		Water->CreateMeshSection_LinearColor(2, WV, TU, WNU, WUV, WC, TArray<FProcMeshTangent>(), false);
		if (UMaterialInterface* UM = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/KeysRun/Materials/M_KR_WaterUnder.M_KR_WaterUnder")))
		{
			UnderMID = UMaterialInstanceDynamic::Create(UM, this);
			Water->SetMaterial(2, UnderMID);
		}
		Water->SetMeshSectionVisible(2, bUnderside);
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
	if (bUnderside)
	{
		for (int32 k = 0; k < WN_.Num(); k++) WNU[k] = -WN_[k];
		Water->UpdateMeshSection_LinearColor(2, WV, WNU, WUV, WC, TArray<FProcMeshTangent>());
		if (UnderMID)
		{
			UnderMID->SetVectorParameterValue(TEXT("SunDir"), FLinearColor(SunDirUE.X, SunDirUE.Y, SunDirUE.Z, 0));
			UnderMID->SetVectorParameterValue(TEXT("Sky"), FMath::Lerp(FLinearColor(.49f, .69f, .81f), FLinearColor(.85f, .6f, .45f), float(SunK)));
		}
	}
}

// ---------------------------------------------------------------- sky
void AKeysWorld::ApplySun(double K)
{
	// day: high sun to the southeast; sunset: golden hour low over the Seven Mile Bridge to the west
	const FVector Day = KeysGeo::ToUE(.55, .32, -.62).GetSafeNormal(), Dusk = KeysGeo::ToUE(-.96, .055, .2).GetSafeNormal();
	const FVector ToSun = FMath::Lerp(Day, Dusk, K).GetSafeNormal();
	SunDirUE = ToSun;
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
