#include "KeysWorld.h"
#include "KeysOcean.h"
#include "Engine/Texture2D.h"
#include "KeysDocks.h"
#include "KeysYards.h"
#include "KeysEstates.h"
#include "KeysGeo.h"
#include "KeysGeoReal.h"
#include "KeysTownData.h"
#include "KeysTown.h"
#include "KeysTownKit.h"
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
#include "Async/Async.h"
#include "EngineUtils.h"
#include "Components/StaticMeshComponent.h"
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
	// god rays (web world/underwater/godrays.ts, High and up): the sun's screen-space light shafts while you're under,
	// streaming down through the surface when you look up toward it
	if (Sun)
	{
		const bool bRays = bOn && KeysGfx::Quality() >= KeysGfx::High;
		Sun->bEnableLightShaftBloom = bRays;
		Sun->BloomScale = .35f; Sun->BloomThreshold = .05f; Sun->BloomTint = FColor(255, 246, 221);
		Sun->MarkRenderStateDirty();
	}
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
	KeysDocks::Sites();   // (plans the private docks now: the berths dredged under some of them are part of the ground built below)
	KeysOcean::DrawTide(true);   // the water mesh (UpdateWater) and KeysGeo::WaveH sit at KeysOcean::SeaLevel()

	Sun->SetAtmosphereSunLight(true);
	Sun->SetIntensity(9.f);
	// the mobile renderer (phones, tablets) draws the atmosphere only on a mesh with a sky material: a big dome
	if (GetWorld()->GetFeatureLevel() < ERHIFeatureLevel::SM5)
	{
		UStaticMesh* DomeMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/EngineSky/SM_SkySphere.SM_SkySphere"));
		UMaterialInterface* SkyMat = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/KeysRun/Materials/M_KR_Sky.M_KR_Sky"));
		if (DomeMesh && SkyMat)
		{
			UStaticMeshComponent* Dome = NewObject<UStaticMeshComponent>(this, TEXT("SkyDome"));
			Dome->SetStaticMesh(DomeMesh);
			Dome->SetMaterial(0, SkyMat);
			Dome->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			Dome->SetCastShadow(false);
			Dome->bNeverDistanceCull = true;
			Dome->SetupAttachment(Root);
			Dome->RegisterComponent();
			Dome->SetWorldScale3D(FVector(2.0e6 / FMath::Max(1.0, DomeMesh->GetBounds().SphereRadius)));   // 20 km out
		}
		else UE_LOG(LogTemp, Warning, TEXT("KeysWorld: no sky dome for the mobile renderer (M_KR_Sky is made when the editor opens)"));
	}
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

	// (where the boat will start, so the ground round it is there for the first frame)
	if (APawn* P0 = UGameplayStatics::GetPlayerPawn(this, 0)) { FocusX = P0->GetActorLocation().X / 100; FocusZ = P0->GetActorLocation().Y / 100; }
	else if (KeysGeo::Marinas().Num()) { FocusX = KeysGeo::Marinas()[0].SX; FocusZ = KeysGeo::Marinas()[0].SZ; }
	BuildTerrain();
	BuildScenery();
	BuildMangroves();
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
// The land is a mesh per 512 m tile. The real map is far too long to stand all at once (65 km of keys): only the tiles
// within sight of the boat are made, off the game thread, and those left behind are taken down (StreamTerrain). Each
// tile is made from the ground's own functions alone, so a tile is the same whenever and in whatever order it is made.
namespace
{
	constexpr double TerrCell = 8;
	constexpr int32 TerrCH = 64;
	constexpr float TerrainDrawDist = 900000.f;   // 9 km: an island is a line on the horizon well before that
	struct FTerrTile { TArray<FVector> V, N; TArray<FVector2D> UV, K; TArray<FLinearColor> C; TArray<int32> T; };
	struct FTerrJob { int64 Key = 0; TFuture<FTerrTile*> Job; };
	TMap<int64, FVector2D> GTerrHave;                               // every tile that may have land in it -> its centre
	TMap<int64, TWeakObjectPtr<UProceduralMeshComponent>> GTerrLive;   // the tiles made (null: made, and found to be all water)
	TArray<FTerrJob> GTerrJobs;
	int32 GTerrGN = 0, GTerrGNZ = 0, GTerrMade = 0;
	double GTerrScanT = 0;
	int64 TerrKey(int32 CX, int32 CY) { return (int64(CX) << 32) ^ int64(uint32(CY)); }

	// one tile's mesh, or null if none of it stands clear of the water
	FTerrTile* GenTerrainTile(int64 Key)
	{
		using namespace KeysGeo;
		const int32 CX = int32(Key >> 32), CY = int32(uint32(Key & 0xffffffff));
		const double Cell = TerrCell;
		const int32 CH = TerrCH, S = CH + 1, W = CH + 3, GN = GTerrGN, GNZ = GTerrGNZ;
		// the heights, one cell past the tile each way for the slopes (held at the map's own edge, as the whole-map grid was)
		TArray<float> HG;
		HG.SetNumUninitialized(W * W);
		auto GI = [&](int32 a) { return FMath::Clamp(CX * CH + a, 0, GN - 1); };
		auto GJ = [&](int32 b) { return FMath::Clamp(CY * CH + b, 0, GNZ - 1); };
		auto HAt = [&](int32 a, int32 b) -> float& { return HG[(b + 1) * W + a + 1]; };
		bool bAny = false;
		for (int32 b = -1; b <= CH + 1; b++)
			for (int32 a = -1; a <= CH + 1; a++)
			{
				const float H = float(LandH(WorldX0 + GI(a) * Cell, WorldZ0 + GJ(b) * Cell));
				HAt(a, b) = H;
				if (H > -1.0f && a >= 0 && b >= 0 && a <= CH && b <= CH) bAny = true;
			}
		if (!bAny) return nullptr;
		TArray<uint8> Kind, Canal;
		Kind.SetNumZeroed(S * S);
		Canal.SetNumZeroed(S * S);   // in a canal or on its wall: KeysCanals draws the wall and the lawn there, the sea floor the bottom
		for (int32 b = 0; b <= CH; b++)
			for (int32 a = 0; a <= CH; a++)
			{
				const double X = WorldX0 + GI(a) * Cell, Z = WorldZ0 + GJ(b) * Cell;
				if (HAt(a, b) > -1) Kind[b * S + a] = uint8(SurfaceKind(X, Z));
				if (RealMap()) { double CN; if (KeysGeoReal::CanalBank(X, Z, CN) && CN < 3) Canal[b * S + a] = 1; }
			}

		const FLinearColor WetSand = Srgb(.74, .69, .53), Sand = Srgb(.91, .86, .71), Grass = Srgb(.43, .56, .28), Scrub = Srgb(.29, .44, .21),
			Mangrove = Srgb(.16, .31, .15), Golf = Srgb(.42, .67, .3), Runway = Srgb(.33, .33, .34), City = Srgb(.55, .54, .52);
		auto ColorAt = [&](int32 a, int32 b)
		{
			const double X = WorldX0 + GI(a) * Cell, Z = WorldZ0 + GJ(b) * Cell, H = HAt(a, b);
			switch (Kind[b * S + a])
			{
				case 1: return H > .9 ? Golf : Sand;
				case 2: return H > .9 ? Runway : Sand;
				case 3: return H > .9 ? City : Sand;
				case 4: return FMath::Lerp(Srgb(.25, .21, .15), Mangrove, float(SStep(.2, .9, H)));   // mud and leaf litter under the mangroves
				default: break;
			}
			if (H < .35)
			{
				// a beach - unless the mangroves stand along it: then it is dark peat, not sand
				const FLinearColor Beach = FMath::Lerp(WetSand, Sand, float(SStep(-1.2, .35, H)));
				const double Tan = MangroveNear(X, Z);
				return Tan > 0 ? FMath::Lerp(Beach, Srgb(.2, .15, .09), float(Tan * .9)) : Beach;
			}
			const double N = .5 + .3 * FMath::Sin(X * .013) * FMath::Cos(Z * .011) + .2 * FMath::Sin((X + Z) * .031);
			const FLinearColor Veg = N > .62 ? Mangrove : N > .42 ? Scrub : Grass;
			return FMath::Lerp(Sand, Veg, float(SStep(.35, .95, H)));
		};

		FTerrTile* Out = new FTerrTile();
		for (int32 b = 0; b < CH; b++)
			for (int32 a = 0; a < CH; a++)
			{
				const int32 K = b * S + a;
				// under the water the sea floor (KeysSeafloor) takes over, so only the beach is drawn here
				if (FMath::Max(FMath::Max(HAt(a, b), HAt(a + 1, b)), FMath::Max(HAt(a, b + 1), HAt(a + 1, b + 1))) < -.3f) continue;
				{ const uint8* Cn = Canal.GetData() + K; if (Cn[0] | Cn[1] | Cn[S] | Cn[S + 1]) continue; }
				Out->T.Append({K, K + S, K + 1, K + 1, K + S, K + S + 1});
			}
		if (!Out->T.Num()) { delete Out; return nullptr; }
		Out->V.Reserve(S * S); Out->N.Reserve(S * S); Out->UV.Reserve(S * S); Out->C.Reserve(S * S);
		for (int32 b = 0; b <= CH; b++)
			for (int32 a = 0; a <= CH; a++)
			{
				const int32 i = GI(a), j = GJ(b);
				const double X = WorldX0 + i * Cell, Z = WorldZ0 + j * Cell, H = HAt(a, b);
				const int32 i0 = GI(a - 1), i1 = GI(a + 1), j0 = GJ(b - 1), j1 = GJ(b + 1);
				const double DX = (HAt(a + 1, b) - HAt(a - 1, b)) / (FMath::Max(1, i1 - i0) * Cell), DZ = (HAt(a, b + 1) - HAt(a, b - 1)) / (FMath::Max(1, j1 - j0) * Cell);
				// below the waterline the beach dives steeply, so the sea floor mesh (KeysSeafloor) is what you see through the water
				Out->V.Add(ToUE(X, H < -.25 ? -.25 + (H + .25) * 3 : H, Z));
				Out->N.Add(FVector(-DX, -DZ, 1).GetSafeNormal());
				Out->UV.Add(FVector2D(X / 20, Z / 20));
				Out->C.Add(ColorAt(a, b));
			}
		// the ground shader (sand ripples and grain; grass where it's green)
		Out->K.Init(FVector2D(SurfSand, 0), Out->V.Num());
		return Out;
	}
}

void AKeysWorld::CommitTerrainTile(int64 Key, void* TilePtr)
{
	FTerrTile* T = static_cast<FTerrTile*>(TilePtr);
	TWeakObjectPtr<UProceduralMeshComponent>& Live = GTerrLive.FindOrAdd(Key);
	if (!T) return;
	// each 512 m tile of land is its own mesh, so the tiles behind you and the ones over the horizon are not drawn
	UProceduralMeshComponent* Tile = NewObject<UProceduralMeshComponent>(this);
	Tile->SetupAttachment(Root);
	Tile->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Tile->RegisterComponent();
	Tile->CreateMeshSection_LinearColor(0, T->V, T->T, T->N, T->UV, T->K, TArray<FVector2D>(), TArray<FVector2D>(), T->C, TArray<FProcMeshTangent>(), false);
	Tile->SetMaterial(0, VertexColorMaterial());
	Tile->SetCachedMaxDrawDistance(TerrainDrawDist);
	Live = Tile;
	GTerrMade++;
	delete T;
}

void AKeysWorld::BuildTerrain()
{
	using namespace KeysGeo;
	for (FTerrJob& J : GTerrJobs) delete J.Job.Get();
	GTerrJobs.Reset();
	GTerrLive.Reset();
	GTerrHave.Reset();
	GTerrMade = 0;
	GTerrScanT = 0;
	const double Tile = TerrCell * TerrCH;
	const int32 NCX = FMath::CeilToInt(WorldSizeX / Tile), NCZ = FMath::CeilToInt(WorldSizeZ / Tile);
	GTerrGN = NCX * TerrCH + 1; GTerrGNZ = NCZ * TerrCH + 1;
	// which tiles can have land: on the real map, those within a few metres of the baked shoreline (the ground is never
	// above the water further out than that); on the classic map, all of them
	for (int32 CY = 0; CY < NCZ; CY++)
		for (int32 CX = 0; CX < NCX; CX++)
		{
			const double X0 = WorldX0 + CX * Tile, Z0 = WorldZ0 + CY * Tile;
			if (RealMap() && !KeysGeoReal::ShoreUnder(X0 - 24, Z0 - 24, X0 + Tile + 24, Z0 + Tile + 24, 14)) continue;
			GTerrHave.Add(TerrKey(CX, CY), FVector2D(X0 + Tile / 2, Z0 + Tile / 2));
		}
	// what is round the boat stands before the first frame; the rest comes in over the next seconds
	TArray<int64> First;
	const double R0 = RealMap() ? 3000 : 1e9;
	for (const TPair<int64, FVector2D>& H : GTerrHave) if (FVector2D::DistSquared(H.Value, FVector2D(FocusX, FocusZ)) < R0 * R0) First.Add(H.Key);
	TArray<FTerrTile*> Made;
	Made.SetNumZeroed(First.Num());
	ParallelFor(First.Num(), [&](int32 k) { Made[k] = GenTerrainTile(First[k]); });
	for (int32 k = 0; k < First.Num(); k++) CommitTerrainTile(First[k], Made[k]);
	UE_LOG(LogTemp, Log, TEXT("Keys Run: terrain - %d of %d tiles may have land; %d made round the boat, the rest as they come within 9.4 km"), GTerrHave.Num(), NCX * NCZ, GTerrMade);
}

void AKeysWorld::StreamTerrain()
{
	if (!GTerrHave.Num()) return;
	constexpr double In = 9400, Out = 10600;
	constexpr int32 MaxJobs = 3, MaxCommits = 2;
	const FVector2D Focus(FocusX, FocusZ);
	int32 Commits = 0;
	for (int32 k = 0; k < GTerrJobs.Num() && Commits < MaxCommits; k++)
		if (GTerrJobs[k].Job.IsReady())
		{
			CommitTerrainTile(GTerrJobs[k].Key, GTerrJobs[k].Job.Get());
			GTerrJobs.RemoveAtSwap(k--);
			Commits++;
		}
	while (GTerrJobs.Num() < MaxJobs)
	{
		// the nearest tile within sight that has not been made
		int64 Best = 0;
		double BestD = In * In;
		bool bAny = false;
		for (const TPair<int64, FVector2D>& H : GTerrHave)
		{
			const double D = FVector2D::DistSquared(H.Value, Focus);
			if (D >= BestD || GTerrLive.Contains(H.Key)) continue;
			bool bBusy = false;
			for (const FTerrJob& J : GTerrJobs) bBusy |= J.Key == H.Key;
			if (bBusy) continue;
			BestD = D; Best = H.Key; bAny = true;
		}
		if (!bAny) break;
		FTerrJob& J = GTerrJobs.AddDefaulted_GetRef();
		J.Key = Best;
		J.Job = Async(EAsyncExecution::ThreadPool, [Best] { return GenTerrainTile(Best); });
	}
	const double Now = GetWorld()->GetTimeSeconds();
	if (Now - GTerrScanT > 1 || Now < GTerrScanT)
	{
		GTerrScanT = Now;
		for (auto It = GTerrLive.CreateIterator(); It; ++It)
		{
			const FVector2D* C = GTerrHave.Find(It.Key());
			if (C && FVector2D::DistSquared(*C, Focus) < Out * Out) continue;
			if (It.Value().IsValid()) It.Value()->DestroyComponent();
			It.RemoveCurrent();
		}
	}
}

// ---------------------------------------------------------------- palms, houses, Miami
// Palms, shore bushes and conch houses are dealt out by dice on fixed grids over the whole map (16 m and 34 m), so
// what stands on a spot depends on the spot alone. They are built a 1200 m square at a time, within sight of the boat
// (StreamScenery), instead of all 65 km at launch. What stands on a Keys Run Estates lot is built at launch as before,
// lot by lot, so building on the lot can clear it.
namespace
{
	constexpr double ScnChunk = 1200;
	struct FScnObs { double X, Z, AX, AZ, HA, HB; bool bRound; int32 Group; };
	struct FScnOut
	{
		FTownBatch Main;
		TMap<int32, FTownBatch> Lots;
		TArray<FTransform> Bushes;
		TArray<FScnObs> Obs;
		int32 Palms = 0, Houses = 0;
	};
	struct FScnLive { TWeakObjectPtr<UProceduralMeshComponent> PM; TWeakObjectPtr<UInstancedStaticMeshComponent> Bushes; };
	struct FScnJob { int64 Key = 0; TFuture<FScnOut*> Job; };
	TSet<int64> GScnHave;                 // every square with land in it
	TMap<int64, FScnLive> GScnLive;
	TSet<int64> GScnObsDone;              // squares whose houses and palms are already in the walker's obstacle list
	TArray<FScnJob> GScnJobs;
	double GScnScanT = 0;
	int32 GScnPalms = 0, GScnHouses = 0, GScnMade = 0;
	int64 ScnKey(int32 KX, int32 KZ) { return (int64(KX) << 32) ^ int64(uint32(KZ)); }
	FVector2D ScnCentre(int64 Key) { return FVector2D((int32(Key >> 32) + .5) * ScnChunk, (int32(uint32(Key & 0xffffffff)) + .5) * ScnChunk); }

	// everything whose own (jittered) spot is in [X0, X1) x [Z0, Z1): what is not on an estates lot (bLots false), or
	// only what is (bLots true)
	void GenScenery(double RX0, double RZ0, double RX1, double RZ1, bool bLots, FScnOut& Out)
	{
		using namespace KeysGeo;
		auto In = [&](double X, double Z) { return X >= RX0 && X < RX1 && Z >= RZ0 && Z < RZ1; };
		// palms and mangroves (kept out of creeks, off the beach, and out of downtown)
		const double PS = 16;
		{
			const int32 I0 = FMath::Max(0, FMath::CeilToInt((RX0 - PS - WorldX0) / PS)), I1 = FMath::Min(FMath::CeilToInt(WorldSizeX / PS) - 1, FMath::FloorToInt((RX1 + PS - WorldX0) / PS));
			const int32 J0 = FMath::Max(0, FMath::CeilToInt((RZ0 - PS - WorldZ0) / PS)), J1 = FMath::Min(FMath::CeilToInt(WorldSizeZ / PS) - 1, FMath::FloorToInt((RZ1 + PS - WorldZ0) / PS));
			for (int32 j = J0; j <= J1; j++)
				for (int32 i = I0; i <= I1; i++)
				{
					const double X = WorldX0 + i * PS, Z = WorldZ0 + j * PS;
					const double JX = X + (Hash2(X, Z) - .5) * PS, JZ = Z + (Hash2(Z, X) - .5) * PS;
					if (!In(JX, JZ)) continue;
					const double H = LandH(JX, JZ);
					if (H < .2 || H > 2.6) continue;
					const double R = Hash2(JX * 1.3, JZ * .7);
					const FShore S = ShoreInfo(JX, JZ);
					const double Density = S.Isl->bMainland ? .05 : S.Isl->bMiami ? .2 : .4;
					if (R > Density || InCity(JX, JZ) || SurfaceKind(JX, JZ) != 0 || CreekDist(JX, JZ) < 8 || KeysTown::InFootprint(JX, JZ, 6) || KeysYards::InFootprint(JX, JZ, 5) || KeysDocks::InFootprint(JX, JZ, 4) || FMath::Abs(JZ - ChainZ(JX)) < 9) continue;
					if (H < .9 && S.E > .93 && !S.Isl->bMiami)
					{
						// mangrove fringe at the water's edge
						if (bLots) continue;
						const double Sz = 3 + R * 8;
						Out.Bushes.Add(FTransform(FRotator(0, R * 360, 0), ToUE(JX, H + .6, JZ), FVector(Sz, Sz * .9, 2.4)));
						Out.Obs.Add({JX, JZ, 1, 0, Sz * .35, Sz * .35, true, -1});
						continue;
					}
					if (H < .7 || KeysTown::NearEstate(JX, JZ, 26)) continue;
					const int32 PalmLot = AKeysEstates::PlatParcelAt(JX, JZ);
					if ((PalmLot >= 0) != bLots) continue;
					Out.Obs.Add({JX, JZ, 1, 0, .32, .32, true, PalmLot});
					// a palm (town props.ts, lighter): a leaning, curving trunk and a crown of fronds that rise and droop
					const double Tall = 6 + Hash2(JZ, JX * 2) * 5;
					const double Lean = (Hash2(JX * 3, JZ) - .5) * .5, LeanA = Hash2(JX, JZ * 3) * 2 * PI;
					FTownBatch& PB = PalmLot >= 0 ? Out.Lots.FindOrAdd(PalmLot) : Out.Main;
					PB.Palm(FVector(JX, H - .1, JZ), Tall, FMath::Cos(LeanA) * Lean * .35, FMath::Sin(LeanA) * Lean * .35, R * 13.7 + JX * .01, 10);
					Out.Palms++;
				}
		}

		// Conch houses (web world/islands.ts): each with its own footprint, wall height and roof pitch, a hip roof of silver tin,
		// a porch across the front, windows with shutters and a door, and a third of them up on piers.
		static const uint32 Pastel[] = {0xf7c6d0, 0xfde2a7, 0xa8e0d8, 0xb9d4f5, 0xf9f3e3, 0xd8c6f0, 0xffffff, 0xc9e8c2};
		static const uint32 Shutter[] = {0x2bb3a8, 0x1f4f8c, 0xffffff, 0x3f8a5a, 0xe4572e};
		const double HS = 34;
		const int32 HI0 = FMath::Max(0, FMath::CeilToInt((RX0 - 6 - WorldX0) / HS)), HI1 = FMath::Min(FMath::CeilToInt(WorldSizeX / HS) - 1, FMath::FloorToInt((RX1 + 6 - WorldX0) / HS));
		const int32 HJ0 = FMath::Max(0, FMath::CeilToInt((RZ0 - 6 - WorldZ0) / HS)), HJ1 = FMath::Min(FMath::CeilToInt(WorldSizeZ / HS) - 1, FMath::FloorToInt((RZ1 + 6 - WorldZ0) / HS));
		for (int32 hj = HJ0; hj <= HJ1; hj++)
			for (int32 hi = HI0; hi <= HI1; hi++)
			{
				const double X = WorldX0 + hi * HS, Z = WorldZ0 + hj * HS;
				const double JX = X + (Hash2(X * .7, Z) - .5) * 10, JZ = Z + (Hash2(Z, X * .7) - .5) * 10;
				if (!In(JX, JZ)) continue;
				const double H = LandH(JX, JZ);
				if (H < 1.15) continue;
				const FShore S = ShoreInfo(JX, JZ);
				if (S.Isl->bNoHouses || S.Isl->bSmall || InCity(JX, JZ) || SurfaceKind(JX, JZ) != 0 || CreekDist(JX, JZ) < 15 || CanalDist(JX, JZ) < 9 || KeysTown::InFootprint(JX, JZ, 14) || FMath::Abs(JZ - ChainZ(JX)) < 16) continue;
				bool bNearMarina = false;
				for (const FMarina& Mr : Marinas()) bNearMarina |= FMath::Abs(JX - Mr.SX) < 30 && FMath::Abs(JZ - Mr.SZ) < 30;
				if (bNearMarina || KeysTown::NearEstate(JX, JZ, 54) || KeysYards::InFootprint(JX, JZ, 12)) continue;   // web waterfront.ts sites clear the conch houses
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
				// a house on (or overlapping) an estates parcel goes in that parcel's own batch
				int32 Lot = AKeysEstates::PlatParcelAt(JX, JZ);
				for (int32 k = 0; k < 4 && Lot < 0; k++) Lot = AKeysEstates::PlatParcelAt(JX + (k % 2 ? 1 : -1) * W * .5, JZ + (k / 2 ? 1 : -1) * D * .5);
				if ((Lot >= 0) != bLots) continue;
				FTownBatch& B = Lot >= 0 ? Out.Lots.FindOrAdd(Lot) : Out.Main;
				const uint32 Wall = Pastel[int32(FMath::Abs(JX * 31 + JZ)) % 8], Trim = Shutter[int32(FMath::Abs(JZ * 17 + JX)) % 5];
				const double Cs = FMath::Cos(Ry), Sn = FMath::Sin(Ry);
				auto At = [&](double LX, double LZ) { return FVector2D(JX + LX * Cs + LZ * Sn, JZ - LX * Sn + LZ * Cs); };
				Out.Obs.Add({JX, JZ, Cs, -Sn, W / 2, D / 2, false, Lot});
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
				// the town style kit: a fish mailbox, maybe a weathervane, a buoy on the porch, the salt and the peeling paint
				{
					KeysTownKit::FHouseSpots Hs;
					const FVector2D FL = At(-W / 2, D / 2 + .04), PF = At(0, D / 2 + 2.3);
					Hs.Front = KeysTownKit::FWall::Make(FVector(FL.X, Base, FL.Y), FVector(Sn, 0, Cs));
					Hs.Width = W; Hs.DoorA = W * .18 + W / 2; Hs.WallH = WallH;
					Hs.PorchFront = FVector(PF.X, Base + .21, PF.Y); Hs.PorchHalf = W / 2;
					Hs.Ridge = FVector(JX, Base + WallH + Pitch * FMath::Min(W, D) * .25, JZ);
					Hs.WallC = KeysTownPaint::Hex(Wall);
					Hs.GroundY = H;
					KeysTownKit::FKitDice Kd(KeysTownKit::SeedOf(JX, JZ));
					KeysTownKit::DressHouse(B, Hs, Kd);
					Out.Houses++;
				}
			}
	}

	FScnOut* GenSceneryChunk(int64 Key)
	{
		const FVector2D C = ScnCentre(Key);
		FScnOut* Out = new FScnOut();
		GenScenery(C.X - ScnChunk / 2, C.Y - ScnChunk / 2, C.X + ScnChunk / 2, C.Y + ScnChunk / 2, false, *Out);
		return Out;
	}

	void AddScnObstacles(const FScnOut& Out)
	{
		for (const FScnObs& O : Out.Obs)
		{
			if (O.bRound) KeysTown::AddObstacleRound(O.X, O.Z, O.HA, O.Group);
			else KeysTown::AddObstacleBox(O.X, O.Z, O.AX, O.AZ, O.HA, O.HB, O.Group);
		}
	}
}

void AKeysWorld::CommitSceneryChunk(int64 Key, void* OutPtr)
{
	FScnOut* Out = static_cast<FScnOut*>(OutPtr);
	FScnLive& L = GScnLive.FindOrAdd(Key);
	// (a square's obstacles go to the walker once, however often the square itself comes and goes)
	if (!GScnObsDone.Contains(Key)) { GScnObsDone.Add(Key); AddScnObstacles(*Out); GScnPalms += Out->Palms; GScnHouses += Out->Houses; }
	if (Out->Main.Tris())
	{
		UProceduralMeshComponent* PM = NewObject<UProceduralMeshComponent>(this);
		PM->SetupAttachment(Root);
		PM->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		PM->RegisterComponent();
		Out->Main.M.Commit(PM, 0, VertexColorMaterial());
		PM->SetCachedMaxDrawDistance(260000.f);
		L.PM = PM;
	}
	if (Out->Bushes.Num() && BushMesh)
	{
		UInstancedStaticMeshComponent* C = NewObject<UInstancedStaticMeshComponent>(this);
		C->SetStaticMesh(BushMesh);
		C->SetupAttachment(Root);
		C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		C->RegisterComponent();
		if (BushMID) C->SetMaterial(0, BushMID);
		C->AddInstances(Out->Bushes, false);
		C->SetCachedMaxDrawDistance(450000.f);
		L.Bushes = C;
	}
	GScnMade++;
	delete Out;
}

void AKeysWorld::BuildScenery()
{
	using namespace KeysGeo;
	UStaticMesh* Cube = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
	UStaticMesh* Cyl = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	UStaticMesh* Sph = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	if (!Cube || !Cyl || !Sph) return;
	for (FScnJob& J : GScnJobs) delete J.Job.Get();
	GScnJobs.Reset();
	GScnLive.Reset();
	GScnHave.Reset();
	GScnObsDone.Reset();
	GScnScanT = 0;
	GScnPalms = GScnHouses = GScnMade = 0;

	// the shore bushes of every square share one mesh and one material
	BushMesh = Sph;
	BushMID = UMaterialInstanceDynamic::Create(SolidMaterial(), this);
	if (BushMID)
	{
		BushMID->SetVectorParameterValue(TEXT("Color"), Srgb(.14, .29, .13));
		BushMID->SetScalarParameterValue(TEXT("Roughness"), .95f);
	}
	UInstancedStaticMeshComponent* Glass = MakeISM(TEXT("Towers"), Cube, Srgb(.42, .55, .66), .15);
	UInstancedStaticMeshComponent* Concrete = MakeISM(TEXT("TowersWhite"), Cube, Srgb(.88, .88, .86), .5);

	// what stands on the estates' lots: built now, a mesh per lot
	{
		FScnOut Lots;
		double X0 = 1e18, Z0 = 1e18, X1 = -1e18, Z1 = -1e18;
		if (RealMap())
		{
			for (const KeysGeoReal::FPlatLot& R : KeysGeoReal::Plat())
				for (const FVector2D& Q : R.Poly) { X0 = FMath::Min(X0, Q.X); X1 = FMath::Max(X1, Q.X); Z0 = FMath::Min(Z0, Q.Y); Z1 = FMath::Max(Z1, Q.Y); }
			X0 -= 30; Z0 -= 30; X1 += 30; Z1 += 30;
		}
		else { X0 = Z0 = -1e9; X1 = Z1 = 1e9; }   // (the classic map's lots are all over it)
		if (X1 > X0) GenScenery(X0, Z0, X1, Z1, true, Lots);
		AddScnObstacles(Lots);
		GScnPalms += Lots.Palms; GScnHouses += Lots.Houses;
		for (TPair<int32, FTownBatch>& C : Lots.Lots)
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
	}

	// the squares with land in them; the ones round the boat stand before the first frame
	for (int32 KZ = FMath::FloorToInt(WorldZ0 / ScnChunk); KZ <= FMath::FloorToInt((WorldZ0 + WorldSizeZ) / ScnChunk); KZ++)
		for (int32 KX = FMath::FloorToInt(WorldX0 / ScnChunk); KX <= FMath::FloorToInt((WorldX0 + WorldSizeX) / ScnChunk); KX++)
			if (!RealMap() || KeysGeoReal::ShoreUnder(KX * ScnChunk, KZ * ScnChunk, (KX + 1) * ScnChunk, (KZ + 1) * ScnChunk, 0)) GScnHave.Add(ScnKey(KX, KZ));
	{
		TArray<int64> First;
		const double R0 = RealMap() ? 2200 : 1e9;
		for (int64 K : GScnHave) if (FVector2D::DistSquared(ScnCentre(K), FVector2D(FocusX, FocusZ)) < R0 * R0) First.Add(K);
		TArray<FScnOut*> Made;
		Made.SetNumZeroed(First.Num());
		if (First.Num()) Made[0] = GenSceneryChunk(First[0]);   // (the first alone: it wakes every table the builder reads)
		ParallelFor(FMath::Max(0, First.Num() - 1), [&](int32 k) { Made[k + 1] = GenSceneryChunk(First[k + 1]); });
		for (int32 k = 0; k < First.Num(); k++) CommitSceneryChunk(First[k], Made[k]);
	}
	UE_LOG(LogTemp, Log, TEXT("Keys Run: scenery - %d squares of %d m have land; %d made round the boat (%d island palms, %d conch houses so far), the rest as they come within 3.2 km"), GScnHave.Num(), int32(ScnChunk), GScnMade, GScnPalms, GScnHouses);

	// Miami skyline: Brickell and downtown towers on the mainland
	const double TS = 60;
	if (!RealMap()) for (double Z = -3580; Z < -2580; Z += TS)
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

void AKeysWorld::StreamScenery()
{
	if (!GScnHave.Num()) return;
	constexpr double In = 3200, Out = 4200;   // drawn to 2.6 km
	const FVector2D Focus(FocusX, FocusZ);
	for (int32 k = 0; k < GScnJobs.Num(); k++)
		if (GScnJobs[k].Job.IsReady())
		{
			CommitSceneryChunk(GScnJobs[k].Key, GScnJobs[k].Job.Get());
			GScnJobs.RemoveAtSwap(k);
			break;   // one a frame
		}
	while (GScnJobs.Num() < 2)
	{
		int64 Best = 0;
		double BestD = In * In;
		bool bAny = false;
		for (int64 K : GScnHave)
		{
			const double D = FVector2D::DistSquared(ScnCentre(K), Focus);
			if (D >= BestD || GScnLive.Contains(K)) continue;
			bool bBusy = false;
			for (const FScnJob& J : GScnJobs) bBusy |= J.Key == K;
			if (bBusy) continue;
			BestD = D; Best = K; bAny = true;
		}
		if (!bAny) break;
		FScnJob& J = GScnJobs.AddDefaulted_GetRef();
		J.Key = Best;
		J.Job = Async(EAsyncExecution::ThreadPool, [Best] { return GenSceneryChunk(Best); });
	}
	const double Now = GetWorld()->GetTimeSeconds();
	if (Now - GScnScanT > 1 || Now < GScnScanT)
	{
		GScnScanT = Now;
		for (auto It = GScnLive.CreateIterator(); It; ++It)
		{
			if (FVector2D::DistSquared(ScnCentre(It.Key()), Focus) < Out * Out) continue;
			if (It.Value().PM.IsValid()) It.Value().PM->DestroyComponent();
			if (It.Value().Bushes.IsValid()) It.Value().Bushes->DestroyComponent();
			It.RemoveCurrent();
		}
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
	// the tide: the whole surface rides up and down on it, and the water is that much deeper or shallower everywhere
	const double Tide = KeysOcean::SeaLevel();
	{
		// the ring past the fine grid follows it, overlapping its outer few percent
		const double Far = 80000, In = WR * .96;
		const double OX[4] = {-1, 1, 1, -1}, OZ[4] = {-1, -1, 1, 1};
		for (int32 k = 0; k < 4; k++)
		{
			FarV[k] = ToUE(CX + OX[k] * Far, Tide - .6, CZ + OZ[k] * Far);
			FarV[k + 4] = ToUE(CX + OX[k] * In, Tide - .6, CZ + OZ[k] * In);
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
		PushOceanTextures();
		// the sun glitter: where the sun is, and how strong (full in daylight, softer through cloud and rain)
		WaterMID->SetVectorParameterValue(TEXT("SunDir"), FLinearColor(SunDirUE.X, SunDirUE.Y, SunDirUE.Z, 0));
		WaterMID->SetScalarParameterValue(TEXT("Glint"), float(1.0 - .7 * KeysWx::RainAt(FocusX, FocusZ)));
	}
	constexpr double NearR = 450;
	GatherWakes(FX, FZ, NearR, T, LiveWakes);
	GatherTrails(FX, FZ, NearR, T, LiveTrails);
	// 15 m buckets, so each water vertex tests only the wake rings and trail pieces that can reach it (it used to test
	// every one: hundreds per vertex, the bulk of the water's frame time with a long white wash out)
	constexpr double BkCell = 15;
	constexpr int32 BkN = int32(2 * NearR / BkCell) + 1;
	BucketWakes(LiveWakes, FX, FZ, NearR, BkCell, BkN, WakeCells);
	BucketTrails(LiveTrails, FX, FZ, NearR, BkCell, BkN, TrailCells);
	auto CellOf = [&](double X, double Z) { return FMath::Clamp(int32((Z - (FZ - NearR)) / BkCell), 0, BkN - 1) * BkN + FMath::Clamp(int32((X - (FX - NearR)) / BkCell), 0, BkN - 1); };

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
			const int32 Ck = bNear ? CellOf(X, Z) : 0;
			const double Wk = bNear ? WakeHFrom(LiveWakes, WakeCells[Ck], X, Z) : 0;
			const FVector2D Tr = bNear ? TrailFrom(LiveTrails, TrailCells[Ck], X, Z) : FVector2D::ZeroVector;
			// wave trains shorter than ~3 grid cells here would only alias on the coarse outer grid
			const double MinLen = 2.6 * FMath::Max(WSp[i], WSp[j]);
			const double D = DepthFast(X, Z) + Tide;   // (the surf line and the beach wash move up and down the shore with it)
			// surf on the beaches and reef edges (near the camera, where the mesh can draw it)
			const FVector2D Surf = bNear ? SurfAt(X, Z, T, D) : FVector2D::ZeroVector;
			const double Sea = WaveHLod(X, Z, T, Amp, MinLen) + Surf.X;
			// the prop wash knocks down the chop a little, so the trail reads as a smoother, glassier lane
			// and right behind the motors the wash heaps up into a boiling white mound before it settles
			double Boil = Tr.X > .6 ? FMath::Square((Tr.X - .6) / .4) * .32 : 0;
			// churned, lumpy water all through the fresh wash, rolling as it goes (so the light catches it like real wash)
			if (Tr.X > .25)
			{
				const double Lump = FMath::Sin(X * 1.9 + T * 3.1) * FMath::Sin(Z * 1.6 - T * 2.4) + .6 * FMath::Sin(X * 3.7 - Z * 2.9 + T * 4.3);
				Boil += (Tr.X - .25) * .16 * Lump;
			}
			const double H = KeepOut(X, Z, Tide + Sea * (1 - .25 * Tr.Y) + Wk + Boil);
			WH[K] = H;
			// the simulated sea pulls each point sideways toward the crests: sharp crests, broad troughs
			FVector2D Side = FVector2D::ZeroVector;
			if (KeysOcean::Enabled()) { const FVector Dp = KeysOcean::Displacement(X, Z, MinLen) * ChopScale(Amp); Side = FVector2D(Dp.X, Dp.Z); }
			WV[K] = ToUE(X + Side.X, H, Z + Side.Y);
			// foam on breaking crests (whitecaps, with the wind), on the crests of fresh wakes, in the prop wash and along
			// the beach, where it comes and goes with the swell. Vertex alpha carries the amount; the material breaks it up.
			const double Crest = Sea / (Amp * 2.4 * Rms + .001);
			// whitecaps: on the simulated sea, where crests have actually folded over (and linger a few seconds after);
			// on the classic waves, on the tallest crests
			double F = KeysOcean::Enabled()
				? FMath::Max(KeysOcean::Foam(X, Z) * (.3 + .7 * Caps) * (1 - SStep(8, 40, MinLen)), SStep(.75, 1.3, Crest) * .12 * Caps)
				: SStep(.5, 1.0, Crest) * (.12 + .7 * Caps);
			// (a creek through the mangroves is in the lee of everything: no whitecaps, no wash on its banks)
			const double LeeCreek = RealMap() ? SStep(0, 14, CreekDist(X, Z)) : 1.0, LeeCanal = RealMap() ? SStep(-1, 10, CanalDist(X, Z)) : 1.0;
			const double Lee = FMath::Min(LeeCreek, LeeCanal);
			F *= Lee;
			// white lines along the arms of a fresh wake, where the crests are tallest
			F = FMath::Max(F, SStep(.025, .16, Wk) * .92);
			F = FMath::Max(F, Tr.X);
			// the wash in the shallows along a beach. On the real map the flats are shallow a long way from any shore, so
			// there it is kept to the water's edge - and there is none under the mangroves, where no surf gets in
			const double Wash = !RealMap() ? 1.0 : SStep(16, 4, KeysGeoReal::Shore(X, Z)) * (1 - MangroveNear(X, Z)) * Lee;
			F = FMath::Max(F, SStep(.9, .25, D) * (.3 + .3 * FMath::Clamp(Crest + .5, 0.0, 1.0)) * Wash);
			F = FMath::Max(F, Surf.Y);
			// UV0: x = how high on a crest (light glows through thin crests), y = aerated wash under the foam
			WUV[K] = FVector2D(FMath::Clamp(Crest * .8 + Wk * .8, 0.0, 1.0), FMath::Clamp(Tr.Y, 0.0, 1.0));
			// UV1: x = rain on the water here, y = whitecap cover (wind-streaked foam); UV2: x = how deep (0 flats .. 1 blue water)
			WUV1[K] = FVector2D(KeysWx::RainAt(X, Z), Caps * Lee);
			WUV2[K] = FVector2D(SStep(4, 60, D), 1 - LeeCreek);   // y: a sheltered, tea-stained mangrove creek
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
			// from the displaced surface itself (the points moved sideways as well as up)
			const FVector A = WV[j * S + i1] - WV[j * S + i0], B = WV[j1 * S + i] - WV[j0 * S + i];
			const FVector Nn = FVector::CrossProduct(A, B).GetSafeNormal();
			WN_[j * S + i] = Nn.Z > .05 ? Nn : FVector::UpVector;
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
	StreamTerrain();
	StreamScenery();
	StreamMangroves();
}

// ---------------------------------------------------------------- the simulated sea's textures for the water material
// Two 64 x 64 float textures (slope x, slope z, foam, height per grid point), refilled every frame from KeysOcean so
// the material's fine normals and whitecaps follow the same waves the boat rides.
void AKeysWorld::PushOceanTextures()
{
	if (!WaterMID) return;
	const bool bOn = KeysOcean::Enabled();
	WaterMID->SetScalarParameterValue(TEXT("SimOn"), bOn ? 1.f : 0.f);
	if (!bOn) return;
	const int32 Nn = KeysOcean::GridN();
	for (int32 t = 0; t < 2; t++)
	{
		if (!SimTex[t])
		{
			SimTex[t] = UTexture2D::CreateTransient(Nn, Nn, PF_A32B32G32R32F);
			if (!SimTex[t]) return;
			SimTex[t]->SRGB = false;
			SimTex[t]->Filter = TF_Bilinear;
			SimTex[t]->AddressX = TA_Wrap;
			SimTex[t]->AddressY = TA_Wrap;
			SimTex[t]->CompressionSettings = TC_HDR;
			SimTex[t]->UpdateResource();
			WaterMID->SetTextureParameterValue(t ? TEXT("Sim1") : TEXT("Sim0"), SimTex[t]);
			WaterMID->SetScalarParameterValue(t ? TEXT("SimL1") : TEXT("SimL0"), float(KeysOcean::TileLen(t)));
		}
		TArray<FLinearColor>* Data = new TArray<FLinearColor>();
		KeysOcean::FillTexels(t, *Data);
		FUpdateTextureRegion2D* Region = new FUpdateTextureRegion2D(0, 0, 0, 0, Nn, Nn);
		SimTex[t]->UpdateTextureRegions(0, 1, Region, Nn * sizeof(FLinearColor), sizeof(FLinearColor), reinterpret_cast<uint8*>(Data->GetData()),
			[Data](uint8*, const FUpdateTextureRegion2D* R) { delete Data; delete R; });
	}
}
