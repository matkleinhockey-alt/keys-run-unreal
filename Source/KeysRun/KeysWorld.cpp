#include "KeysWorld.h"
#include "KeysEstates.h"
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
#include "Components/PostProcessComponent.h"
#include "KeysGfx.h"
#include "KeysBoatPawn.h"
#include "KeysMesh.h"
#include "KeysWeather.h"
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

	// the game's look, everywhere: a gentle filmic grade (a touch more contrast and saturation, warm highlights,
	// cool shadows), soft bloom, a light vignette, ambient occlusion and Lumen a notch finer than the defaults
	Grade = CreateDefaultSubobject<UPostProcessComponent>(TEXT("Grade"));
	Grade->SetupAttachment(Root);
	Grade->bUnbound = true;
	Grade->Priority = -1.f;
	FPostProcessSettings& S = Grade->Settings;
	S.bOverride_ColorSaturation = true; S.ColorSaturation = FVector4(1.07, 1.07, 1.07, 1.0);
	S.bOverride_ColorContrast = true; S.ColorContrast = FVector4(1.06, 1.06, 1.06, 1.0);
	S.bOverride_ColorGainHighlights = true; S.ColorGainHighlights = FVector4(1.03, 1.0, .96, 1.0);
	S.bOverride_ColorGammaShadows = true; S.ColorGammaShadows = FVector4(.98, 1.0, 1.04, 1.0);
	S.bOverride_BloomIntensity = true; S.BloomIntensity = .55f;
	S.bOverride_BloomThreshold = true; S.BloomThreshold = 1.2f;
	S.bOverride_VignetteIntensity = true; S.VignetteIntensity = .32f;
	S.bOverride_AmbientOcclusionIntensity = true; S.AmbientOcclusionIntensity = .55f;
	S.bOverride_AmbientOcclusionRadius = true; S.AmbientOcclusionRadius = 90.f;
	S.bOverride_FilmToe = true; S.FilmToe = .58f;
	S.bOverride_SceneFringeIntensity = true; S.SceneFringeIntensity = .15f;
	S.bOverride_LumenFinalGatherQuality = true; S.LumenFinalGatherQuality = 1.5f;
	S.bOverride_LumenReflectionQuality = true; S.LumenReflectionQuality = 1.5f;
	S.bOverride_LumenSceneLightingQuality = true; S.LumenSceneLightingQuality = 1.25f;
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
	// the haze glows toward the sun
	Fog->SetDirectionalInscatteringExponent(6.f);
	Fog->SetDirectionalInscatteringStartDistance(2000.f);
	Fog->SetDirectionalInscatteringColor(FLinearColor(.55f, .45f, .32f));
	// contact shadows tuck the small things (cleats, feet, rod butts) into what they stand on
	Sun->ContactShadowLength = KeysGfx::ContactShadowsOn() ? .04f : 0.f;
	// No volumetric cloud shadow map: its pass asserts (check, not a fallback) on a cloud material whose shaders are
	// still compiling, and ours compiles on demand in a -game run. Storm shade comes from KeysWeather's gloom instead.
	Sun->bCastCloudShadows = false;
	Sun->MarkRenderStateDirty();
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
			// the ground shader (sand ripples and grain; grass where it's green)
			TArray<FVector2D> K;
			K.Init(FVector2D(SurfSand, 0), V.Num());
			Terrain->CreateMeshSection_LinearColor(Section, V, T, N, UV, K, TArray<FVector2D>(), TArray<FVector2D>(), C, TArray<FProcMeshTangent>(), false);
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
	TMap<int32, FTownBatch> ParcelBatches;   // what stands on an estates parcel, so building the lot can clear it
	auto ChunkOf = [](double X, double Z) { return (int64(FMath::FloorToInt(X / 1200)) << 32) ^ int64(uint32(FMath::FloorToInt(Z / 1200))); };
	static const uint32 FrondC[] = {0x3f8a35, 0x4f9a3c, 0x58a443, 0x467f30};
	UInstancedStaticMeshComponent* Mangroves = MakeISM(TEXT("Mangroves"), Sph, Srgb(.14, .29, .13), .95);
	UInstancedStaticMeshComponent* Glass = MakeISM(TEXT("Towers"), Cube, Srgb(.42, .55, .66), .15);
	UInstancedStaticMeshComponent* Concrete = MakeISM(TEXT("TowersWhite"), Cube, Srgb(.88, .88, .86), .5);

	// palms and mangroves (kept out of creeks, off the beach, and out of downtown)
	int32 PalmCount = 0;
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
				KeysTown::AddObstacleRound(JX, JZ, Sz * .35);
				continue;
			}
			if (H < .7 || KeysTown::NearEstate(JX, JZ, 26)) continue;
			const int32 PalmLot = AKeysEstates::PlatParcelAt(JX, JZ);
			KeysTown::AddObstacleRound(JX, JZ, .32, PalmLot);
			// a palm (town props.ts, lighter): a leaning, curving trunk and a crown of fronds that rise and droop
			const double Tall = 6 + Hash2(JZ, JX * 2) * 5;
			const double Lean = (Hash2(JX * 3, JZ) - .5) * .5, LeanA = Hash2(JX, JZ * 3) * 2 * PI;
			FTownBatch& PB = PalmLot >= 0 ? ParcelBatches.FindOrAdd(PalmLot) : HouseChunks.FindOrAdd(ChunkOf(JX, JZ));
			PB.Palm(FVector(JX, H - .1, JZ), Tall, FMath::Cos(LeanA) * Lean * .35, FMath::Sin(LeanA) * Lean * .35, R * 13.7 + JX * .01, 10);
			PalmCount++;
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
			if (bNearMarina || KeysTown::NearEstate(JX, JZ, 54)) continue;   // web waterfront.ts sites clear the conch houses
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
			// a house on (or overlapping) an estates parcel goes in that parcel's own batch
			int32 Lot = AKeysEstates::PlatParcelAt(JX, JZ);
			for (int32 k = 0; k < 4 && Lot < 0; k++) Lot = AKeysEstates::PlatParcelAt(JX + (k % 2 ? 1 : -1) * W * .5, JZ + (k / 2 ? 1 : -1) * D * .5);
			FTownBatch& B = Lot >= 0 ? ParcelBatches.FindOrAdd(Lot) : HouseChunks.FindOrAdd(Key);
			const uint32 Wall = Pastel[int32(FMath::Abs(JX * 31 + JZ)) % 8], Trim = Shutter[int32(FMath::Abs(JZ * 17 + JX)) % 5];
			const double Cs = FMath::Cos(Ry), Sn = FMath::Sin(Ry);
			auto At = [&](double LX, double LZ) { return FVector2D(JX + LX * Cs + LZ * Sn, JZ - LX * Sn + LZ * Cs); };
			KeysTown::AddObstacleBox(JX, JZ, Cs, -Sn, W / 2, D / 2, Lot);
			B.M.Surf = SurfConcrete;
			if (bPiers)
			{
				B.Box(JX, H + Lift / 2 - .3, JZ, W - .4, Lift + .6, D - .4, 0x8e8b82, 0, Ry);
				for (int32 sx = -1; sx <= 1; sx += 2) for (int32 sz = -1; sz <= 1; sz += 2) { const FVector2D Q = At(sx * (W / 2 - .3), sz * (D / 2 - .3)); B.Box(Q.X, H + Lift / 2 - .2, Q.Y, .35, Lift + .4, .35, 0xe8e4d8, 0, Ry); }
			}
			else B.Box(JX, H - .3 + .25, JZ, W + .3, .5 + .6, D + .3, 0xd8d0be, 0, Ry);
			B.M.Surf = SurfSiding;
			B.Box(JX, Base + WallH / 2, JZ, W, WallH, D, Wall, 0, Ry);
			B.M.Surf = R3 < .7 ? SurfTin : SurfShingle;
			// the hip roof, silver tin, with a little overhang and its own pitch
			const double Pitch = FMath::Lerp(.82, 1.35, R2) * 1.2, Over = FMath::Lerp(1.0, 1.12, R1);
			B.HipRoof(JX, Base + WallH, JZ, W * Over + .6, D * Over + .6, Pitch * FMath::Min(W, D) * .25, KeysTownPaint::Hex(R3 < .7 ? 0xb8bcc2 : 0xc0603a), Ry);
			// the porch across the front, a porch roof on posts, the door and windows with shutters
			B.M.Surf = SurfWood;
			const FVector2D PC = At(0, D / 2 + 1.15);
			B.Box(PC.X, Base + .1, PC.Y, W, .22, 2.3, 0xf4f2ec, 0, Ry);
			for (int32 k = -1; k <= 1; k += 2) { const FVector2D Q = At(k * (W / 2 - .2), D / 2 + 2.1); B.Box(Q.X, Base + 1.3, Q.Y, .14, 2.4, .14, 0xffffff, 0, Ry); }
			const FVector2D PR = At(0, D / 2 + 1.2);
			B.M.Surf = SurfTin;
			B.Box(PR.X, Base + 2.55, PR.Y, W + .2, .1, 2.6, 0xb8bcc2, -.12, Ry);
			B.M.Surf = SurfDefault;
			const FVector2D Dr = At(W * .18, D / 2 + .04);
			B.Box(Dr.X, Base + 1.1, Dr.Y, 1.0, 2.1, .08, Trim, 0, Ry);
			for (double WX : {-W * .25, W * .4})
			{
				if (FMath::Abs(WX - W * .18) < 1.0) continue;
				const FVector2D Q = At(WX, D / 2 + .04);
				B.M.Surf = SurfGlass; B.Box(Q.X, Base + 1.7, Q.Y, 1.1, 1.3, .06, 0x27414f, 0, Ry); B.M.Surf = SurfDefault;
				for (int32 k = -1; k <= 1; k += 2) { const FVector2D Q2 = At(WX + k * .8, D / 2 + .05); B.Box(Q2.X, Base + 1.7, Q2.Y, .4, 1.4, .06, Trim, 0, Ry); }
			}
			for (int32 sd = -1; sd <= 1; sd += 2)
				for (double WZ : {-D * .22, D * .22}) { const FVector2D Q = At(sd * (W / 2 + .03), WZ); B.M.Surf = SurfGlass; B.Box(Q.X, Base + 1.7, Q.Y, .06, 1.2, 1.0, 0x27414f, 0, Ry); B.M.Surf = SurfDefault; }
			const FVector2D Bk = At(-W * .2, -D / 2 - .03);
			B.M.Surf = SurfGlass;
			B.Box(Bk.X, Base + 1.7, Bk.Y, 1.1, 1.2, .06, 0x27414f, 0, Ry);
			B.M.Surf = SurfDefault;
		}
	UE_LOG(LogTemp, Log, TEXT("Keys Run: %d island palms"), PalmCount);
	for (TPair<int32, FTownBatch>& C : ParcelBatches)
	{
		UProceduralMeshComponent* PM = NewObject<UProceduralMeshComponent>(this);
		PM->SetupAttachment(Root);
		PM->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		PM->RegisterComponent();
		C.Value.M.Commit(PM, 0, VertexColorMaterial());
		PM->SetCachedMaxDrawDistance(260000.f);
		PM->SetVisibility(!ClearedParcels.Contains(C.Key));
		ParcelScenery.Add(C.Key, PM);
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
		WUV1.Init(FVector2D::ZeroVector, S * S);
		WUV2.Init(FVector2D::ZeroVector, S * S);
		WH.Init(0, S * S);
		WSp.SetNum(S);
		for (int32 i = 0; i < S; i++) WSp[i] = (WGrid[FMath::Min(i + 1, WN)] - WGrid[FMath::Max(i - 1, 0)]) * .5;
		TArray<int32> T;
		T.Reserve(WN * WN * 6);
		for (int32 j = 0; j < WN; j++)
			for (int32 i = 0; i < WN; i++)
			{
				const int32 K = j * S + i;
				T.Append({K, K + S, K + 1, K + 1, K + S, K + S + 1});
			}
		Water->CreateMeshSection_LinearColor(0, WV, T, WN_, WUV, WUV1, WUV2, TArray<FVector2D>(), WC, TArray<FProcMeshTangent>(), false);
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

		// deep blue out past the fine grid, so the horizon never shows a hole. A ring, not a plane: it has a hole under the
		// fine grid (it follows the grid), so a flat sheet can never show up through a cockpit sitting low in a trough.
		FLinearColor Deep = DepthRGB(600);
		Deep.A = 0;   // alpha carries foam
		FarV.Init(FVector::ZeroVector, 8);
		TArray<FVector> FN; FN.Init(FVector::UpVector, 8);
		TArray<FVector2D> FUV; FUV.Init(FVector2D::ZeroVector, 8);   // UV0 drives crest glow and wash; none out here
		TArray<FVector2D> FUV2; FUV2.Init(FVector2D(1, 0), 8);       // all deep
		TArray<FLinearColor> FC; FC.Init(Deep, 8);
		// 0-3 the outer corners, 4-7 the inner ones (counter-clockwise from -x,-z)
		const TArray<int32> FT = {0, 4, 1, 1, 4, 5, 1, 5, 2, 2, 5, 6, 2, 6, 3, 3, 6, 7, 3, 7, 0, 0, 7, 4};
		Water->CreateMeshSection_LinearColor(1, FarV, FT, FN, FUV, FUV, FUV2, TArray<FVector2D>(), FC, TArray<FProcMeshTangent>(), false);
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
		Water->CreateMeshSection_LinearColor(2, WV, TU, WNU, WUV, WUV1, WUV2, TArray<FVector2D>(), WC, TArray<FProcMeshTangent>(), false);
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
	const double FX = FocusX, FZ = FocusZ;
	// crest foam is measured against the sea's own rms height; whitecaps come with the wind
	const double Rms = FMath::Max(.01, WaveRms()), Caps = Whitecaps(), Wind = WindSpeed();
	{
		// the ring past the fine grid follows it, overlapping its outer few percent
		const double Far = 80000, In = WR * .96;
		const double OX[4] = {-1, 1, 1, -1}, OZ[4] = {-1, -1, 1, 1};
		for (int32 k = 0; k < 4; k++)
		{
			FarV[k] = ToUE(CX + OX[k] * Far, -.6, CZ + OZ[k] * Far);
			FarV[k + 4] = ToUE(CX + OX[k] * In, -.6, CZ + OZ[k] * In);
		}
		Water->UpdateMeshSection_LinearColor(1, FarV, TArray<FVector>(), TArray<FVector2D>(), TArray<FLinearColor>(), TArray<FProcMeshTangent>());
	}
	if (WaterMID)
	{
		// glassy in a calm, rippled in a breeze, roughed up in a blow; the ripples run with the wind
		WaterMID->SetScalarParameterValue(TEXT("RippleStrength"), float(.22 + .85 * SStep(.5, 8, Wind) + .35 * SStep(10, 18, Wind)));
		WaterMID->SetScalarParameterValue(TEXT("WindDir"), float(WindDir()));
		WaterMID->SetScalarParameterValue(TEXT("Whitecap"), float(Caps));
		// which way the boat's track runs (for the wash's foam streaks), from how the focus moves, eased
		{
			const double MX = FocusX - TrackPX, MZ = FocusZ - TrackPZ;
			if (MX * MX + MZ * MZ > .04 && MX * MX + MZ * MZ < 400)
			{
				const double A = FMath::Atan2(MZ, MX);
				TrackDirS = FMath::Atan2(FMath::Lerp(FMath::Sin(TrackDirS), FMath::Sin(A), .15), FMath::Lerp(FMath::Cos(TrackDirS), FMath::Cos(A), .15));
			}
			TrackPX = FocusX; TrackPZ = FocusZ;
			WaterMID->SetScalarParameterValue(TEXT("TrackDir"), float(TrackDirS));
		}
	}
	constexpr double NearR = 450;
	GatherWakes(FX, FZ, NearR, T, LiveWakes);
	GatherTrails(FX, FZ, NearR, T, LiveTrails);

	// Keep the sea out of your own boat: inside the gunwales the surface never rises above the cockpit sole, so a
	// swell that lifts over a low cockpit (the Freeman's, offshore) doesn't show through the deck. Sampled once per
	// update along the hull; vertices outside the liner are untouched.
	bool bHull = false;
	FTransform HullT;
	double HullX0 = 0, HullX1 = 0, HullR2 = 0;
	TArray<double> HullSole, HullHalf;
	constexpr int32 HullN = 48;
	if (const AKeysBoatPawn* BP = Cast<AKeysBoatPawn>(UGameplayStatics::GetPlayerPawn(this, 0)))
	{
		const KeysData::FBoatSpec& BS = BP->Spec();
		HullT = BP->GetActorTransform();
		HullX0 = KeysMesh::SternX(BS) + .12;
		HullX1 = KeysMesh::BowX(BS) - .3;
		HullR2 = FMath::Square(BS.Len * .6 + 1);
		HullSole.SetNum(HullN + 1);
		HullHalf.SetNum(HullN + 1);
		for (int32 k = 0; k <= HullN; k++)
		{
			const double X = FMath::Lerp(HullX0, HullX1, double(k) / HullN);
			HullSole[k] = KeysMesh::SoleAt(BS, X) - .05;
			HullHalf[k] = KeysMesh::InnerHalfBeam(BS, X);
		}
		bHull = true;
	}
	const FVector HullC = HullT.GetLocation() / 100;
	auto KeepOut = [&](double X, double Z, double H)
	{
		if (!bHull || FMath::Square(X - HullC.X) + FMath::Square(Z - HullC.Y) > HullR2) return H;
		// Where the vertical line through (X, Z) meets the sole, in the hull's frame. Found from the sole's own height (two
		// passes), not the wave's: with the hull rolled or pitched, a crest's height would shift the point sideways and
		// a vertex over the cockpit could be judged outside it and left standing up through the deck.
		double Yq = HullC.Z, Floor = Yq;
		FVector Lc = FVector::ZeroVector;
		int32 K = 0;
		double U = 0;
		for (int32 It = 0; It < 2; It++)
		{
			Lc = HullT.InverseTransformPosition(ToUE(X, Yq, Z)) / 100;
			const double F = FMath::Clamp((Lc.X - HullX0) / (HullX1 - HullX0), 0.0, 1.0) * HullN;
			K = FMath::Clamp(int32(F), 0, HullN - 1);
			U = F - K;
			Floor = HullT.TransformPosition(FVector(Lc.X, Lc.Y, FMath::Lerp(HullSole[K], HullSole[K + 1], U)) * 100).Z / 100;
			Yq = Floor;
		}
		// a grid cell's worth past the liner too, or the triangles that cross the gunwale would carry the swell
		// back up over the sole; outside the hull that dip stays under the topsides
		constexpr double Reach = .95;
		if (Lc.X <= HullX0 - Reach || Lc.X >= HullX1 + Reach * .5) return H;
		if (FMath::Abs(Lc.Y) >= FMath::Lerp(HullHalf[K], HullHalf[K + 1], U) + Reach) return H;
		return FMath::Min(H, Floor);
	};

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
			// wave trains shorter than ~3 grid cells here would only alias on the coarse outer grid
			const double Sea = WaveHLod(X, Z, T, Amp, 2.6 * FMath::Max(WSp[i], WSp[j]));
			// the prop wash knocks down the chop a little, so the trail reads as a smoother, glassier lane
			const double H = KeepOut(X, Z, Sea * (1 - .25 * Tr.Y) + Wk);
			WH[K] = H;
			WV[K] = ToUE(X, H, Z);
			// foam on breaking crests (whitecaps, with the wind), on the crests of fresh wakes, in the prop wash and along
			// the beach, where it comes and goes with the swell. Vertex alpha carries the amount; the material breaks it up.
			const double Crest = Sea / (Amp * 2.4 * Rms + .001);
			const double D = DepthFast(X, Z);
			double F = SStep(.5, 1.0, Crest) * (.12 + .7 * Caps);
			// white lines along the arms of a fresh wake, where the crests are tallest
			F = FMath::Max(F, SStep(.03, .22, Wk) * .7);
			F = FMath::Max(F, Tr.X);
			F = FMath::Max(F, SStep(.9, .25, D) * (.3 + .3 * FMath::Clamp(Crest + .5, 0.0, 1.0)));
			// UV0: x = how high on a crest (light glows through thin crests), y = aerated wash under the foam
			WUV[K] = FVector2D(FMath::Clamp(Crest * .8 + Wk * .8, 0.0, 1.0), FMath::Clamp(Tr.Y, 0.0, 1.0));
			// UV1: x = rain on the water here, y = whitecap cover (wind-streaked foam); UV2: x = how deep (0 flats .. 1 blue water)
			WUV1[K] = FVector2D(KeysWx::RainAt(X, Z), Caps);
			WUV2[K] = FVector2D(SStep(4, 60, D), 0);
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
	Water->UpdateMeshSection_LinearColor(0, WV, WN_, WUV, WUV1, WUV2, TArray<FVector2D>(), WC, TArray<FProcMeshTangent>());
	if (bUnderside)
	{
		for (int32 k = 0; k < WN_.Num(); k++) WNU[k] = -WN_[k];
		Water->UpdateMeshSection_LinearColor(2, WV, WNU, WUV, WUV1, WUV2, TArray<FVector2D>(), WC, TArray<FProcMeshTangent>());
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
	Fog->SetDirectionalInscatteringColor(FMath::Lerp(FLinearColor(.55f, .45f, .32f), FLinearColor(1.4f, .7f, .32f), float(K)));
}

void AKeysWorld::ApplyWeather()
{
	// under a storm: the sun dims behind the cell (its own volumetric shadow does the rest), the sky light drops, the
	// haze thickens and greys toward a rain-curtain wall; lightning lights the whole sky for an instant
	const float K = float(DuskK()), G = float(FMath::Clamp(WxGloom, 0.0, 1.0)), R = float(FMath::Clamp(WxRain, 0.0, 1.0)), Fl = float(FMath::Max(0.0, WxFlash));
	Sun->SetIntensity(9.f * (1.f - .8f * G));
	SkyLight->SetIntensity(1.f - .45f * G + 2.5f * Fl);
	Fog->SetFogDensity(.004f + .008f * G + .03f * R);
	Fog->SetFogHeightFalloff(.15f + .1f * R);
	const FLinearColor Clear = FMath::Lerp(FLinearColor(.45f, .62f, .75f), FLinearColor(.75f, .5f, .38f), K);
	const FLinearColor Storm = FLinearColor(.3f, .34f, .38f) * (1.f - .45f * K);
	Fog->SetFogInscatteringColor(FMath::Lerp(Clear, Storm, FMath::Min(1.f, .75f * G + .4f * R)) + FLinearColor(.5f, .5f, .6f) * Fl * .6f);
	Fog->SetDirectionalInscatteringColor(FMath::Lerp(FLinearColor(.55f, .45f, .32f), FLinearColor(1.4f, .7f, .32f), K) * (1.f - .9f * G));
}

void AKeysWorld::ToggleSunset() { SunsetTarget = SunsetTarget > .5 ? 0 : 1; }
void AKeysWorld::SetClearedParcels(const TSet<int32>& Parcels)
{
	ClearedParcels = Parcels;
	for (TPair<int32, TObjectPtr<UProceduralMeshComponent>>& P : ParcelScenery) if (P.Value) P.Value->SetVisibility(!Parcels.Contains(P.Key));
}

void AKeysWorld::SnapSunset() { SunsetTarget = 1; SunK = 1; ApplySun(SStep(0, 1, SunK)); }

double AKeysWorld::DuskK() const { return SStep(0, 1, SunK); }

double AKeysWorld::Dusk(const UObject* WorldContext)
{
	const AKeysWorld* W = Get(WorldContext);
	return W ? W->DuskK() : 0;
}

UMaterialInterface* AKeysWorld::GlowMaterial()
{
	static TWeakObjectPtr<UMaterialInterface> M;
	if (!M.IsValid()) M = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/KeysRun/Materials/M_KR_Bubble.M_KR_Bubble"));
	return M.Get();
}

void AKeysWorld::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!FMath::IsNearlyEqual(SunK, SunsetTarget))
	{
		SunK = SunK < SunsetTarget ? FMath::Min(SunsetTarget, SunK + DeltaSeconds * .25) : FMath::Max(SunsetTarget, SunK - DeltaSeconds * .25);
		ApplySun(SStep(0, 1, SunK));
	}
	ApplyWeather();
	KeysGeo::TickSea(DeltaSeconds);
	UpdateWater();
}
