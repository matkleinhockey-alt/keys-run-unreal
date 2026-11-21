#include "KeysSchools.h"
#include "KeysGeo.h"
#include "KeysSea.h"
#include "KeysOcean.h"
#include "KeysMesh.h"
#include "KeysLife.h"
#include "KeysReef.h"
#include "KeysWrecks.h"
#include "KeysDiver.h"
#include "KeysBoatPawn.h"
#include "KeysWorld.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/Texture2D.h"
#include "StaticMeshAttributes.h"
#include "MeshDescription.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Kismet/GameplayStatics.h"
#include "Camera/PlayerCameraManager.h"
#include "EngineUtils.h"
#include "HAL/PlatformTime.h"
#include "Misc/CommandLine.h"

using namespace KeysGeo;

namespace
{
	// ---- where schools live
	constexpr double CellSize = 26;
	constexpr double SpawnR = 140, KeepR = 190;
	constexpr int32 MaxSchools = 72, MaxFish = 5200;
	// Bait may take this much of the fish budget and no more. A bait ball is 150 to 500 fish, so without a share of its
	// own twenty of them used the whole budget and nothing else - no snapper, no grunts, no jacks - got a fish.
	constexpr int32 MaxBaitFish = 3200;
	// The bubble (Populate) only brings back a school that swam out of it by itself: with the camera moving faster than
	// this (m/s) the school has simply been left behind, and is let go.
	constexpr double WrapMaxCamSpeed = 5;
	constexpr double BoundsExt = 40;     // mesh units: the tail beat and body bend reach a little outside the rest pose

	using ERole = AKeysSchools::ERole;

	struct FSpec
	{
		const TCHAR* Key;
		ERole Role;
		double LenMin, LenMax;
		int32 NMin, NMax, SubMin, SubMax;
		double Cruise, Burst, Accel, Spacing, DiverR, DMin, DMax, AltMin, AltMax, Roam;
		bool bPredator;
	};
	enum ESp { Silverside, Herring, Schoolmaster, Grunt, Lane, Yellowtail, Grouper, RedGrouper, Barracuda, Bonefish, Permit, Bluerunner, Jack, Blacktip, Bullshark, NumSpec };
	const FSpec Specs[NumSpec] = {
		// key              role           len         n          subs    cruise burst accel spacing diverR  dmin dmax  alt          roam  pred
		{TEXT("silverside"), ERole::Bait,   .09, .12,  320, 520,  6, 8,  .5,  2.8, 3.0, 2.3, 4.5,  2.2, 3000, 1.2, 3.5,  22, false},
		{TEXT("herring"),    ERole::Bait,   .17, .22,  140, 220,  4, 6,  .62, 2.7, 3.0, 2.0, 5,    3,   3000, 1.5, 4,    28, false},
		{TEXT("schoolmaster"), ERole::Reef, .32, .45,  14, 30,    2, 3,  .22, 2.2, 1.6, 1.7, 3.5,  2,   30, .5, 2.0,   5, false},
		{TEXT("grunt"),      ERole::Reef,   .22, .3,   16, 36,    2, 4,  .16, 1.7, 1.6, 1.6, 3.5,  1.6, 30, .4, 1.6,   5, false},
		{TEXT("lanesnapper"), ERole::Reef,  .26, .34,  10, 24,    2, 3,  .3,  2.4, 1.6, 1.7, 3.5,  2,   35, .5, 2.0,   6, false},
		{TEXT("yellowtail"), ERole::Reef,   .34, .42,  8, 18,     1, 2,  .55, 3.0, 2.0, 2.0, 4,    3,   40, 1.4, 4,    10, false},
		{TEXT("grouper"),    ERole::Hole,   .8, 1.1,   1, 1,      0, 0,  .08, 1.6, 1.0, 1,   2.5,  3,   40, .25, .6,   3, false},
		{TEXT("redgrouper"), ERole::Hole,   .55, .75,  1, 1,      0, 0,  .08, 1.6, 1.0, 1,   2.5,  3,   40, .2, .5,    3, false},
		{TEXT("barracuda"),  ERole::Hanger, .9, 1.3,   1, 2,      0, 0,  .05, 5.5, 4.0, 2.5, 2.5,  .9,  40, .8, 3.5,  12, true},
		{TEXT("bonefish"),   ERole::Flats,  .45, .62,  4, 9,      0, 1,  .75, 5.0, 3.0, 2.2, 7,    .4,  2.8, .12, .3,  70, false},
		{TEXT("permit"),     ERole::Flats,  .6, .8,    2, 5,      0, 1,  .55, 4.2, 2.5, 2.4, 6,    .5,  3.5, .15, .4,  70, false},
		{TEXT("bluerunner"), ERole::Hunter, .4, .5,    5, 9,      0, 1,  1.15, 4.6, 3.0, 2.0, 3,    2,   3000, 1.0, 4.5,  60, true},
		{TEXT("jackcrevalle"), ERole::Hunter, .6, .85, 4, 7,      0, 1,  .95, 5.0, 3.0, 2.0, 3,    1,   3000, .8, 4.0,   60, true},
		{TEXT("blacktip"),   ERole::Patrol, 1.5, 1.9,  1, 1,      0, 0,  .9,  3.2, 1.4, 1,   1.5,  .8,  30, .4, 2.5,   90, true},
		{TEXT("bullshark"),  ERole::Patrol, 2.0, 2.4,  1, 1,      0, 0,  .8,  3.0, 1.2, 1,   1.5,  4,   45, .6, 4,     90, true},
	};
	// how each species carries itself, on top of its speeds above:
	//   Flex     how far the school gives to the water round it (0 holds its shape, 1 pours into whatever room there is)
	//   Give, Back  how fast it squeezes in, and rounds out again (per s)
	//   Slide    how hard it slips off a reef, wreck or edge it's pressed against
	//   Stretch  its shape at ease: longer than wide (a travelling school) or rounder (a milling one)
	//   Turn     how much its heading wanders and swings;  Agile  how sharply it comes round
	//   PaceJ    how much one school's pace differs from the next
	//   Dart     how often and how far single fish dart to a new spot in the school (the shader reads it)
	struct FTemper { double Flex, Give, Back, Slide, Stretch, Turn, Agile, PaceJ, Dart; };
	const FTemper Tempers[NumSpec] = {
		//                Flex  Give Back  Slide Stretch Turn Agile PaceJ Dart
		/* silverside */ {1.0,  3.0, .9,   1.2,  1.0,    1.4, 1.6,  .12,  1.6},   // nervous: quick turns, fish flicking everywhere
		/* herring */    {1.0,  2.0, .6,   1.0,  1.15,   .9,  1.0,  .1,   1.0},   // a flowing river of fish, smooth sweeping turns
		/* schoolmaster*/{.75,  1.4, .5,   .5,   1.1,    .6,  .7,   .15,  .5},    // hangs lazily by the coral, slow to move
		/* grunt */      {.8,   1.2, .4,   .4,   .9,     .5,  .6,   .2,   .4},    // a loose milling crowd, hardly moving
		/* lanesnapper */{.8,   1.8, .6,   .7,   1.1,    .8,  1.0,  .15,  .8},    // wary, keeps its distance, moves off sooner
		/* yellowtail */ {.7,   2.2, .8,   .9,   1.25,   1.1, 1.3,  .12,  1.1},   // up in the water, quick and busy
		/* grouper */    {0,    1,   1,    0,    1,      .4,  .5,   .1,   .2},
		/* redgrouper */ {0,    1,   1,    0,    1,      .4,  .5,   .1,   .2},
		/* barracuda */  {.4,   1,   .5,   .4,   1.2,    .3,  .8,   .1,   .1},    // hardly a school: a loose line hanging still
		/* bonefish */   {.9,   2.5, .7,   1.0,  1.4,    1.2, 1.4,  .15,  .6},    // a strung-out wedge across the flat, spooky
		/* permit */     {.85,  1.6, .5,   .8,   1.2,    .8,  1.0,  .15,  .5},    // a tighter, steadier little group
		/* bluerunner */ {.55,  2.5, 1.2,  1.0,  1.3,    1.2, 1.5,  .1,   .7},    // a fast pack, sharp turns
		/* jack */       {.45,  2.0, 1.0,  .8,   1.25,   .8,  1.2,  .1,   .4},    // a heavy, purposeful pack that keeps its shape
		/* blacktip */   {0,    1,   1,    0,    1,      .7,  .9,   .1,   0},
		/* bullshark */  {0,    1,   1,    0,    1,      .5,  .7,   .1,   0},
	};
	int32 SpecIndex(const FString& K) { for (int32 i = 0; i < NumSpec; i++) if (K == Specs[i].Key) return i; return -1; }

	struct FZoneMix { const TCHAR* Zone; double Density; TArray<TPair<int32, double>> Rows; };
	const TArray<FZoneMix>& ZoneMixes()
	{
		static const TArray<FZoneMix> M = {
			{TEXT("Flats"), .45, {{Bonefish, 4}, {Permit, 2}, {Silverside, 3.5}, {Barracuda, 1.5}, {Blacktip, .6}, {Jack, .8}}},
			{TEXT("Backcountry"), .42, {{Silverside, 6}, {Barracuda, 1}, {Jack, 1}, {Grunt, 1}, {Bonefish, 1}}},
			// under and along the mangroves: clouds of juveniles in the roots, snapper holding in the shade, and what hunts them
			{TEXT("Creek"), .8, {{Silverside, 5}, {Schoolmaster, 2.5}, {Lane, 1.5}, {Grunt, 2}, {Barracuda, 1.3}, {Jack, 1}, {Blacktip, .7}, {Bonefish, .6}}},
			{TEXT("Bridge"), .62, {{Herring, 6}, {Grunt, 2}, {Schoolmaster, 2}, {Barracuda, 1}, {Jack, 1}}},
			{TEXT("Hawk Channel"), .58, {{Herring, 5}, {Silverside, 4.5}, {Grunt, 2}, {Schoolmaster, 1}, {Lane, 1.5}, {RedGrouper, .8}, {Barracuda, 1}, {Bluerunner, 1.2}, {Bullshark, .2}}},
			{TEXT("Reef"), .78, {{Silverside, 6}, {Herring, 4.5}, {Schoolmaster, 3}, {Grunt, 3}, {Lane, 2}, {Yellowtail, 2}, {Grouper, 1.2}, {RedGrouper, .8}, {Barracuda, 1.5}, {Bluerunner, 1.5}, {Jack, .8}, {Blacktip, .3}, {Bullshark, .25}}},
			// blue water: bait balls up in the top of the water column, with the runners and jacks that work them
			{TEXT("Offshore"), .3, {{Herring, 6}, {Silverside, 4}, {Bluerunner, 1.6}, {Jack, .8}}},
		};
		return M;
	}

	// the reef's species, in KeysReef's order: how tall and how wide each grows at scale 1 (m)
	const double CoralH[] = {1.0, 1.1, .56, .72, 1.7, 1.3, 1.0, 1.1, .24, 0};
	const double CoralR[] = {.85, .8, .9, 1.0, .55, .45, .65, .5, .65, 0};

	FVector G2U(const FVector& P) { return FVector(P.X * 100.0, P.Z * 100.0, P.Y * 100.0); }

	// how each species schools, as boids (distances in its own spacing, Spacing x body length):
	//   Sight    how far it sees its neighbours;  Sep  how close it lets one come before it moves off
	//   Align    how much it matches their heading;  Cohere  how much it drifts toward them;  SepK  how hard it keeps off
	//   Goal     how much it follows the school's own intent (its wander, its coral head, the hunt)
	//   Mill     whether it balls up and mills round when a predator comes;  Nbr  how many neighbours it pays heed to
	struct FBoid { double Sight, Sep, Align, Cohere, SepK, Goal, Mill; int32 Nbr; };
	const FBoid Boids[NumSpec] = {
		//                Sight Sep   Align Cohere SepK Goal Mill Nbr
		/* silverside */ {3.0,  .85,  2.2,  1.4,   3.0, .5,  1,   8},   // tight bait: dense, quick to agree
		/* herring */    {3.2,  .85,  2.0,  1.2,   3.0, .6,  1,   8},
		/* schoolmaster*/{3.5,  .95,  .5,   .5,    2.0, .35, 0,   6},   // a lazy aggregation over the coral
		/* grunt */      {3.5,  .95,  .35,  .55,   2.0, .3,  0,   6},
		/* lanesnapper */{3.5,  .95,  .7,   .6,    2.0, .45, 0,   6},
		/* yellowtail */ {3.2,  .9,   1.2,  .9,    2.4, .5,  0,   7},
		/* grouper */    {1,    1,    0,    0,     1,   .3,  0,   0},
		/* redgrouper */ {1,    1,    0,    0,     1,   .3,  0,   0},
		/* barracuda */  {2.5,  .9,   .3,   .2,    1.5, .3,  0,   3},
		/* bonefish */   {3.0,  .9,   1.4,  1.0,   2.5, .6,  0,   6},
		/* permit */     {3.0,  .9,   1.3,  1.1,   2.5, .6,  0,   5},
		/* bluerunner */ {3.0,  .95,  1.4,  .7,    2.5, .8,  0,   6},
		/* jack */       {3.2,  1.05, 1.0,  .45,   2.2, .8,  0,   6},   // loose jacks: wide spacing, a looser pack
		/* blacktip */   {1,    1,    0,    0,     1,   .9,  0,   0},
		/* bullshark */  {1,    1,    0,    0,     1,   .9,  0,   0},
	};
	// a species' sight in meters (the neighbour grid's cell size for it)
	double SightM(int32 Sp) { return FMath::Max(.3, Boids[Sp].Sight * Specs[Sp].Spacing * (Specs[Sp].LenMin + Specs[Sp].LenMax) * .5); }
	bool Lives(ERole R) { return R == ERole::Reef || R == ERole::Hole || R == ERole::Flats; }   // its depth is height above the bottom
	int64 GridKey(int32 Sp, int32 X, int32 Y, int32 Z)
	{
		return (int64(Sp & 0xf) << 60) | (int64((X + (1 << 19)) & 0xfffff) << 40) | (int64((Y + (1 << 19)) & 0xfffff) << 20) | int64((Z + (1 << 19)) & 0xfffff);
	}
	FVector D2U(const FVector& D) { return FVector(D.X, D.Z, D.Y); }
	double Hyp(double A, double B) { return FMath::Sqrt(A * A + B * B); }
	int64 CellKey(int32 X, int32 Z) { return (int64(X) << 32) ^ int64(uint32(Z)); }
	FVector Flat(const FVector& V) { return FVector(V.X, 0, V.Z); }
	FVector ClampLen(const FVector& V, double M) { const double S = V.Size(); return S > M && S > 0 ? V * (M / S) : V; }
	// a direction's frame in game axes: ahead, right, up
	void Frame(const FVector& F, FVector& Rt, FVector& Up)
	{
		Rt = FVector::CrossProduct(F, FVector(0, 1, 0));
		if (Rt.SizeSquared() < 1e-6) Rt = FVector(0, 0, 1);
		Rt.Normalize();
		Up = FVector::CrossProduct(Rt, F);
	}
}

AKeysSchools::AKeysSchools()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickGroup = TG_PostPhysics;
	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent = Root;
}


AKeysSchools* AKeysSchools::Get(const UObject* WorldContext)
{
	UWorld* W = WorldContext ? WorldContext->GetWorld() : nullptr;
	if (!W) return nullptr;
	TActorIterator<AKeysSchools> It(W);
	return It ? *It : nullptr;
}

void AKeysSchools::BeginPlay()
{
	Super::BeginPlay();
	FString Script;
	bScriptLog = FParse::Value(FCommandLine::Get(), TEXT("KRScript="), Script, false);
	UMaterialInterface* M = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/KeysRun/Materials/M_KR_Boid.M_KR_Boid"));
	if (!M) { UE_LOG(LogTemp, Warning, TEXT("KRSchools: M_KR_Boid missing (open the editor once to create it); no school fish")); return; }
	MID = UMaterialInstanceDynamic::Create(M, this);
}

// ================================================================ the bottom: floor, coral and wrecks, cached on 1 m tiles
const AKeysSchools::FTile* AKeysSchools::TileFor(int32 TX, int32 TZ)
{
	const int64 K = CellKey(TX, TZ);
	if (const TUniquePtr<FTile>* Have = Tiles.Find(K)) return Have->Get();
	if (TilesThisFrame >= 4) return nullptr;
	TilesThisFrame++;
	if (Tiles.Num() > 600) Tiles.Reset();
	TUniquePtr<FTile> T = MakeUnique<FTile>();
	const double X0 = TX * 32.0, Z0 = TZ * 32.0;
	for (int32 j = 0; j < 32; j++)
		for (int32 i = 0; i < 32; i++)
		{
			const float F = float(KeysSea::SeafloorHeightAt(X0 + i + .5, Z0 + j + .5));
			T->Floor[j * 32 + i] = F;
			T->Top[j * 32 + i] = F;
		}
	auto Stamp = [&](double X, double Z, double R, double Top)
	{
		const int32 I0 = FMath::Max(0, FMath::FloorToInt32(X - R - X0)), I1 = FMath::Min(31, FMath::FloorToInt32(X + R - X0));
		const int32 J0 = FMath::Max(0, FMath::FloorToInt32(Z - R - Z0)), J1 = FMath::Min(31, FMath::FloorToInt32(Z + R - Z0));
		for (int32 j = J0; j <= J1; j++)
			for (int32 i = I0; i <= I1; i++)
				if (Hyp(X0 + i + .5 - X, Z0 + j + .5 - Z) <= R + .5) T->Top[j * 32 + i] = FMath::Max(T->Top[j * 32 + i], float(Top));
	};
	// coral
	TActorIterator<AKeysReef> ReefIt(GetWorld());
	if (AKeysReef* Reef = ReefIt ? *ReefIt : nullptr)
	{
		const int32 C0x = FMath::FloorToInt32((X0 - 3) / 50), C1x = FMath::FloorToInt32((X0 + 35) / 50);
		const int32 C0z = FMath::FloorToInt32((Z0 - 3) / 50), C1z = FMath::FloorToInt32((Z0 + 35) / 50);
		for (int32 cz = C0z; cz <= C1z; cz++)
			for (int32 cx = C0x; cx <= C1x; cx++)
				for (const AKeysReef::FInst& I : Reef->ChunkInstances(cx, cz))
				{
					if (I.Species < 0 || I.Species > 9 || CoralH[I.Species] <= 0) continue;
					if (I.X < X0 - 3 || I.X > X0 + 35 || I.Z < Z0 - 3 || I.Z > Z0 + 35) continue;
					Stamp(I.X, I.Z, CoralR[I.Species] * FMath::Max(I.SX, I.SZ), I.Y + CoralH[I.Species] * I.SY);
				}
	}
	// wrecks
	for (const KeysWrecks::FWreck& W : KeysWrecks::Wrecks())
	{
		if (KeysWrecks::EdgeDist(W, X0 + 16, Z0 + 16) > 30) continue;
		for (int32 j = 0; j < 32; j++)
			for (int32 i = 0; i < 32; i++)
			{
				const double E = KeysWrecks::EdgeDist(W, X0 + i + .5, Z0 + j + .5);
				if (E < 1.5) T->Top[j * 32 + i] = FMath::Max(T->Top[j * 32 + i], float(T->Floor[j * 32 + i] + W.Height * (1 - E / 1.5 * .6)));
			}
	}
	return Tiles.Add(K, MoveTemp(T)).Get();
}

double AKeysSchools::TopAt(double X, double Z, double* OutFloor)
{
	const int32 TX = FMath::FloorToInt32(X / 32), TZ = FMath::FloorToInt32(Z / 32);
	const FTile* T = TileFor(TX, TZ);
	if (!T)
	{
		const double F = KeysSea::SeafloorHeightAt(X, Z);
		if (OutFloor) *OutFloor = F;
		return F;
	}
	const int32 I = FMath::Clamp(FMath::FloorToInt32(X - TX * 32.0), 0, 31), J = FMath::Clamp(FMath::FloorToInt32(Z - TZ * 32.0), 0, 31);
	if (OutFloor) *OutFloor = T->Floor[J * 32 + I];
	return T->Top[J * 32 + I];
}

bool AKeysSchools::ValidWater(int32 Sp, double X, double Z) const
{
	const double D = DepthFast(X, Z);
	// in among the mangrove roots the small fish and the snapper live in a couple of feet of water
	if (D >= .5 && D < Specs[Sp].DMin && Specs[Sp].LenMax < .5 && MangroveNear(X, Z) > .55) return LandH(X, Z) < 0;
	return D >= Specs[Sp].DMin && D <= Specs[Sp].DMax && LandH(X, Z) < 0;
}

// ================================================================ meshes and pools
int32 AKeysSchools::PoolFor(int32 Sp, int32 Lod)
{
	const int32 Key = Sp * 3 + Lod;
	const int32 Have = PoolKey.IndexOfByKey(Key);
	if (Have != INDEX_NONE) return Pools[Have] ? Have : -1;
	if (!MID) return -1;
	FKeysMesh M;
	if (!AKeysLife::BuildSchoolMesh(Specs[Sp].Key, M, Lod)) { UE_LOG(LogTemp, Warning, TEXT("KRSchools: no model for %s"), Specs[Sp].Key); PoolKey.Add(Key); PoolDirty.Add(false); Pools.Add(nullptr); return -1; }
	M.SyncKinds();
	// normalize: nose at +X 50, tail at -50, whatever the species' length
	double MinX = 1e9, MaxX = -1e9;
	for (const FVector& V : M.V) { MinX = FMath::Min(MinX, V.X); MaxX = FMath::Max(MaxX, V.X); }
	const double K = 100.0 / FMath::Max(1.0, MaxX - MinX), Cx = (MinX + MaxX) * .5;
	FMeshDescription MD;
	FStaticMeshAttributes Attr(MD);
	Attr.Register();
	TVertexAttributesRef<FVector3f> Pos = Attr.GetVertexPositions();
	TVertexInstanceAttributesRef<FVector3f> Nrm = Attr.GetVertexInstanceNormals();
	TVertexInstanceAttributesRef<FVector4f> Col = Attr.GetVertexInstanceColors();
	TVertexInstanceAttributesRef<FVector2f> UVs = Attr.GetVertexInstanceUVs();
	UVs.SetNumChannels(2);
	const FPolygonGroupID Grp = MD.CreatePolygonGroup();
	Attr.GetPolygonGroupMaterialSlotNames()[Grp] = FName(TEXT("Mat"));
	TArray<FVertexInstanceID> Inst;
	Inst.SetNum(M.V.Num());
	for (int32 i = 0; i < M.V.Num(); i++)
	{
		const FVertexID V = MD.CreateVertex();
		Pos[V] = FVector3f((M.V[i] - FVector(Cx, 0, 0)) * K);
		const FVertexInstanceID I = MD.CreateVertexInstance(V);
		Nrm[I] = FVector3f(M.N[i]);
		Col[I] = FVector4f(M.C[i].R, M.C[i].G, M.C[i].B, 1);
		UVs.Set(I, 0, FVector2f(M.UV[i]));
		UVs.Set(I, 1, FVector2f(M.K[i]));
		Inst[i] = I;
	}
	// wind each face the way Unreal draws it front-facing for its normals (Dot(Cross(B-A, C-A), N) < 0)
	for (int32 i = 0; i + 2 < M.T.Num(); i += 3)
	{
		const int32 A = M.T[i], B = M.T[i + 1], Cc = M.T[i + 2];
		const FVector Fn = FVector::CrossProduct(M.V[B] - M.V[A], M.V[Cc] - M.V[A]);
		const bool bFlip = FVector::DotProduct(Fn, M.N[A] + M.N[B] + M.N[Cc]) > 0;
		MD.CreateTriangle(Grp, {Inst[A], Inst[bFlip ? Cc : B], Inst[bFlip ? B : Cc]});
	}
	UStaticMesh* SM = NewObject<UStaticMesh>(this, *FString::Printf(TEXT("Fish_%s_%d"), Specs[Sp].Key, Lod));
	SM->GetStaticMaterials().Add(FStaticMaterial(MID, FName(TEXT("Mat"))));
	SM->SetPositiveBoundsExtension(FVector(BoundsExt));
	SM->SetNegativeBoundsExtension(FVector(BoundsExt));
	UStaticMesh::FBuildMeshDescriptionsParams P;
	P.bFastBuild = true;
	P.bMarkPackageDirty = false;
	P.bBuildSimpleCollision = false;
	SM->BuildFromMeshDescriptions({&MD}, P);
	SM->CalculateExtendedBounds();
	Meshes.Add(SM);
	UInstancedStaticMeshComponent* C = NewObject<UInstancedStaticMeshComponent>(this, *FString::Printf(TEXT("FishPool_%s_%d"), Specs[Sp].Key, Lod));
	C->SetStaticMesh(SM);
	C->SetupAttachment(Root);
	C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	C->NumCustomDataFloats = 8;   // tail phase, its rate, when, amplitude, body bend, flash, shade, seed (M_KR_Boid)
	C->SetCastShadow(true);
	C->SetMaterial(0, MID);
	C->SetCullDistances(0, int32(KeepR * 100));
	C->bAffectDistanceFieldLighting = false;
	C->RegisterComponent();
	Pools.Add(C);
	PoolKey.Add(Key);
	PoolDirty.Add(false);
	UE_LOG(LogTemp, Log, TEXT("KRSchools: %s lod %d, %d triangles"), Specs[Sp].Key, Lod, M.T.Num() / 3);
	return Pools.Num() - 1;
}

void AKeysSchools::FlushPools()
{
	// lay out a pool's instances again: every school in it, in order, at the fish's own places
	for (int32 P = 0; P < Pools.Num(); P++)
	{
		if (!PoolDirty[P]) continue;
		PoolDirty[P] = false;
		Rebuilds++;
		UInstancedStaticMeshComponent* C = Pools[P];
		if (!C) continue;
		C->ClearInstances();
		TArray<FTransform> Xf;
		for (FSchool& S : Schools)
			if (S.Pool == P) { S.First = Xf.Num(); for (int32 i = 0; i < S.Fish.Num(); i++) Xf.Add(FTransform(G2U(S.Fish[i].P))); }
		if (Xf.Num()) C->AddInstances(Xf, false, true);
		for (FSchool& S : Schools) if (S.Pool == P) WriteInstances(S);
		C->MarkRenderStateDirty();
		PoolMoved[P] = false;
	}
}

void AKeysSchools::WriteInstances(FSchool& S)
{
	if (!Pools.IsValidIndex(S.Pool) || !Pools[S.Pool]) return;
	UInstancedStaticMeshComponent* C = Pools[S.Pool];
	if (S.First + S.Fish.Num() > C->GetInstanceCount()) return;
	TArray<FTransform> Xf;
	Xf.SetNum(S.Fish.Num());
	for (int32 i = 0; i < S.Fish.Num(); i++)
	{
		const FFish& F = S.Fish[i];
		FRotator R = FRotationMatrix::MakeFromX(D2U(F.Fwd)).Rotator();
		R.Roll = FMath::RadiansToDegrees(F.Roll);
		Xf[i] = FTransform(R, G2U(F.P), FVector(F.Len));
		const float D[8] = {float(FMath::Fmod(F.Phase, 2 * PI)), float(F.Freq), float(NowT), float(F.Amp),
			float(FMath::Clamp(F.YawRate * .45, -.9, .9)), float(FMath::Clamp(F.Flash, 0.0, 1.0)), float(F.Shade), float(F.Seed)};
		C->SetCustomData(S.First + i, TArrayView<const float>(D, 8), false);
	}
	C->BatchUpdateInstancesTransforms(S.First, Xf, true, false, false);
	PoolMoved[S.Pool] = true;
}

// ================================================================ the world around: camera, boat, lures, diver, predators
// ================================================================ the world around: camera, boat, diver, predators
void AKeysSchools::ReadWorld(double Dt)
{
	if (APlayerCameraManager* C = UGameplayStatics::GetPlayerCameraManager(this, 0))
	{
		const FVector L = C->GetCameraLocation();
		Cam = FVector(L.X / 100, L.Z / 100, L.Y / 100);
	}
	Threats.Reset();
	if (AKeysBoatPawn* P = Cast<AKeysBoatPawn>(UGameplayStatics::GetPlayerPawn(this, 0)))
	{
		const FVector N(P->BoatX(), 0, P->BoatZ());
		if (bHaveBoat && Dt > 0)
		{
			const FVector V = (N - BoatP) / Dt;
			if (V.Size() < 60) BoatV = FMath::Lerp(BoatV, V, FMath::Min(1.0, Dt * 4));
		}
		BoatP = N;
		bHaveBoat = true;
		const double Spd = BoatV.Size();
		if (Spd > 1 && !P->IsDiving())
		{
			// the hull and the props running over shallow water: a wide, strong threat near the surface
			FThreat T;
			T.Kind = 1;
			T.P = FVector(BoatP.X, -.6, BoatP.Z);
			T.V = BoatV;
			T.R = 5 + Spd * .7;
			T.Power = FMath::Clamp(Spd / 6, .35, 1.0);
			Threats.Add(T);
			if (Spd > 2.5)
			{
				FThreat Pr = T;
				Pr.P = T.P - BoatV.GetSafeNormal() * 5 + FVector(0, -.4, 0);
				Pr.R = 4 + Spd * .4;
				Threats.Add(Pr);
			}
		}
	}
	// the bait in the water: a cast float or a fish fighting on the line, and the trolled lures skipping along
	if (AKeysBoatPawn* P = Cast<AKeysBoatPawn>(UGameplayStatics::GetPlayerPawn(this, 0)))
	{
		TArray<FVector4> Lp;
		P->LureSpots(Lp);
		for (const FVector4& L4 : Lp)
		{
			FThreat T;
			T.Kind = 4;
			T.P = FVector(L4.X, L4.Y, L4.Z);
			T.R = 2.5 + 3 * L4.W;
			T.Power = .35 + .65 * L4.W;
			Threats.Add(T);
		}
	}
	bDiving = false;
	if (AKeysDiver* D = AKeysDiver::Get(this); D && D->IsDiving())
	{
		bDiving = true;
		const KeysDive::FState& S = D->State();
		FThreat T;
		T.Kind = 0;
		T.P = FVector(S.X, S.Y, S.Z);
		T.V = FVector(S.VX, S.VY, S.VZ);
		const double K = FMath::Clamp(T.V.Size() / 1.5, 0.0, 1.5);
		T.R = .6 + .8 * K;     // multiplied by each species' own stand-off distance
		T.Power = .55 + .4 * FMath::Min(1.0, K);
		Threats.Add(T);
	}
	for (int32 i = Startles.Num() - 1; i >= 0; i--)
	{
		FStartle& St = Startles[i];
		St.Left -= Dt;
		if (St.Left <= 0) { Startles.RemoveAt(i); continue; }
		FThreat T;
		T.Kind = 3;
		T.P = St.P;
		T.R = St.R;
		T.Power = 1;
		Threats.Add(T);
	}
	// AKeysLife's sharks, barracuda and jacks
	PredT -= Dt;
	if (PredT <= 0)
	{
		PredT = .25;
		LifePreds.Reset();
		if (const AKeysLife* L = AKeysLife::Get(this)) L->PredatorsNear(Cam.X, Cam.Z, KeepR, LifePreds);
	}
	for (const FVector4& Pp : LifePreds)
	{
		FThreat T;
		T.Kind = 2;
		T.P = FVector(Pp.X, Pp.Y, Pp.Z);
		T.R = 5 + Pp.W * 3;
		T.Power = .6;
		Threats.Add(T);
	}
	// our own predators: cruising they unsettle bait, charging they scatter everything
	for (const FSchool& S : Schools)
	{
		if (!Specs[S.Sp].bPredator) continue;
		const bool bCharge = S.HuntLeft > 0;
		for (const FFish& F : S.Fish)
		{
			FThreat T;
			T.Kind = 2;
			T.SchoolId = S.Id;
			T.P = F.P;
			T.V = F.V;
			T.R = (bCharge ? 7 : 4) + S.Len * 3;
			T.Power = bCharge ? 1 : .45;
			Threats.Add(T);
		}
	}
}

void AKeysSchools::Startle(double X, double Z, double Radius)
{
	double Y = -2;
	if (bDiving) if (AKeysDiver* D = AKeysDiver::Get(this)) Y = D->State().Y;
	Startles.Add({FVector(X, Y, Z), Radius * 1.4, .7});
}

AKeysSchools::FSchool* AKeysSchools::Spawn(int32 Sp, const FVector& Home, double HomeTop, int64 Cell, uint32 Seed, double NScale)
{
	const FSpec& Sc = Specs[Sp];
	FSchool S;
	S.Rng.Initialize(int32(Seed));
	S.Pace = S.Rng.FRandRange(1 - Tempers[Sp].PaceJ, 1 + Tempers[Sp].PaceJ);
	S.Id = NextId++;
	S.Sp = Sp;
	S.Cell = Cell;
	S.Len = S.Rng.FRandRange(Sc.LenMin, Sc.LenMax);
	S.Home = Home;
	S.HomeTop = HomeTop;
	S.MillDir = S.Rng.FRand() < .5 ? -1 : 1;
	int32 N = FMath::Max(1, FMath::RoundToInt32(S.Rng.FRandRange(Sc.NMin, Sc.NMax + .99) * NScale));
	int32 NFishAll = 0;
	for (const FSchool& O : Schools) NFishAll += O.Fish.Num();
	if (!Cell)
	{
		// a test school: make room by letting the furthest ones go
		while (Schools.Num() && NFishAll + N > MaxFish)
		{
			int32 Far = -1;
			double FD = -1;
			for (int32 i = 0; i < Schools.Num(); i++)
			{
				const double D = (Schools[i].Center - Home).Size();
				if (!Schools[i].bPinned && D > FD) { FD = D; Far = i; }
			}
			if (Far < 0) break;
			NFishAll -= Schools[Far].Fish.Num();
			const int64 FarCell = Schools[Far].Cell;
			Despawn(Far);
			if (FarCell) Cells.Add(FarCell, -1);
		}
	}
	const int32 Want = N;
	if (Sc.Role == ERole::Bait)
	{
		int32 NBait = 0;
		for (const FSchool& O : Schools) if (Specs[O.Sp].Role == ERole::Bait) NBait += O.Fish.Num();
		N = FMath::Min(N, MaxBaitFish - NBait);
	}
	if (NFishAll + N > MaxFish) N = MaxFish - NFishAll;
	// no room for a proper school: none, rather than a stray fish or two calling itself one
	if (N < 1 || (Cell && N < (Want + 1) / 2)) return nullptr;
	const double Space = Sc.Spacing * S.Len;
	S.Radius = FMath::Max(Space, Space * FMath::Pow(double(N), 1.0 / 3.0) * .62);
	const bool bBall = Sc.Role == ERole::Bait;
	const double Yaw = S.Rng.FRandRange(0, 2 * PI);
	const FVector Fw(FMath::Cos(Yaw), 0, FMath::Sin(Yaw));
	FVector Rt, Up;
	Frame(Fw, Rt, Up);
	S.Fish.SetNum(N);
	for (int32 i = 0; i < N; i++)
	{
		FFish& F = S.Fish[i];
		// a ball (bait) or a fish-shaped blob drawn out along the heading (everything else), spaced by body lengths
		FVector U;
		do U = FVector(S.Rng.FRandRange(-1, 1), S.Rng.FRandRange(-1, 1), S.Rng.FRandRange(-1, 1)); while (U.SizeSquared() > 1);
		const FVector Off = bBall ? U * S.Radius : FVector(U.X * S.Radius * 1.4, U.Y * S.Radius, U.Z * S.Radius * .45);
		F.P = Home + Fw * Off.X + Rt * Off.Y + Up * Off.Z;
		F.Fwd = (Fw + Rt * S.Rng.FRandRange(-.15, .15)).GetSafeNormal();
		F.V = F.Fwd * Sc.Cruise * .4;
		F.Pace = S.Rng.FRandRange(.88, 1.12);
		F.Phase = S.Rng.FRandRange(0, 2 * PI);
		F.Len = S.Len * S.Rng.FRandRange(.88, 1.12);
		F.Shade = S.Rng.FRandRange(.82, 1.08);
		F.Seed = S.Rng.FRand();
		// its place in the depth band: height above the bottom for fish that live on it, an offset in the school else
		F.BandY = Lives(Sc.Role) ? S.Rng.FRandRange(Sc.AltMin, Sc.AltMax) : Off.Z;
	}
	S.Center = Home;
	S.Heading = Fw;
	S.Goal = Home;
	Schools.Add(MoveTemp(S));
	return &Schools.Last();
}

void AKeysSchools::Despawn(int32 Index)
{
	FSchool& S = Schools[Index];
	if (PoolDirty.IsValidIndex(S.Pool)) PoolDirty[S.Pool] = true;
	if (S.Cell) Cells.Remove(S.Cell);
	Schools.RemoveAt(Index);
}

AKeysSchools::FSchool* AKeysSchools::Find(int32 Id)
{
	for (FSchool& S : Schools) if (S.Id == Id) return &S;
	return nullptr;
}

AKeysSchools::FSchool* AKeysSchools::NearestOfRole(const FVector& P, bool bBaitOnly, int32 NotId, double MaxD)
{
	FSchool* Best = nullptr;
	double BD = MaxD;
	for (FSchool& S : Schools)
	{
		if (S.Id == NotId || Specs[S.Sp].bPredator) continue;
		const ERole R = Specs[S.Sp].Role;
		if (bBaitOnly ? R != ERole::Bait : (R != ERole::Bait && R != ERole::Reef && R != ERole::Flats)) continue;
		const double D = (S.Center - P).Size();
		if (D < BD) { BD = D; Best = &S; }
	}
	return Best;
}

void AKeysSchools::Populate()
{
	// how fast the camera is travelling (Populate runs every half second)
	const double CamMoved = Hyp(Cam.X - PopCam.X, Cam.Z - PopCam.Z);
	PopCam = Cam;
	const bool bCamStill = CamMoved < WrapMaxCamSpeed * .5;
	// let go of schools far from the camera
	for (int32 i = Schools.Num() - 1; i >= 0; i--)
	{
		FSchool& S = Schools[i];
		if (S.bPinned) continue;
		const double DX = S.Center.X - Cam.X, DZ = S.Center.Z - Cam.Z, D = Hyp(DX, DZ);
		if (D <= KeepR) continue;
		// the bubble: an open-water school that swims out of it comes back in on the far side, just out of sight,
		// still on its way (reef fish, grouper, barracuda and flats fish belong where they live, and are let go)
		const ERole Role = Specs[S.Sp].Role;
		// Only one that left by its own swimming, and only into the kind of water it came from: a boat running off (or a
		// teleport) used to drag the first bait balls it met along for good, from the marina to the reef to blue water,
		// where they kept the fish budget full and the local schools never appeared.
		if (bCamStill && D < KeepR + 40 && (Role == ERole::Bait || Role == ERole::Hunter || Role == ERole::Patrol))
		{
			const double NX = Cam.X - DX / D * KeepR * .9, NZ = Cam.Z - DZ / D * KeepR * .9;
			if (ValidWater(S.Sp, NX, NZ) && ZoneAt(NX, NZ) == ZoneAt(S.Home.X, S.Home.Z))
			{
				const FVector Dl(NX - S.Center.X, 0, NZ - S.Center.Z);
				const double Floor = TopAt(NX, NZ) + 1.0;
				for (FFish& F : S.Fish) { F.P += Dl; F.P.Y = FMath::Clamp(F.P.Y, FMath::Min(Floor, SurfY - .8), SurfY - .8); }
				S.Center += Dl; S.Home += Dl; S.Goal += Dl;
				if (S.Cell) { Cells.Remove(S.Cell); S.Cell = 0; }   // the water it left can fill again
				UE_LOG(LogTemp, Verbose, TEXT("KRSchools wrap %s to %.0f %.0f"), Specs[S.Sp].Key, NX, NZ);
				WrapCount++;
				continue;
			}
		}
		Despawn(i);
	}
	for (auto It = Cells.CreateIterator(); It; ++It)
	{
		const int32 CX = int32(It.Key() >> 32), CZ = int32(uint32(It.Key() & 0xffffffff));
		if (It.Value() < 0 && Hyp((CX + .5) * CellSize - Cam.X, (CZ + .5) * CellSize - Cam.Z) > KeepR) It.RemoveCurrent();
	}
	// the camera high above land or far out in the blue: nothing to do
	if (Cam.Y > 120) return;
	const int32 R = FMath::CeilToInt32(SpawnR / CellSize);
	const int32 IX = FMath::FloorToInt32(Cam.X / CellSize), IZ = FMath::FloorToInt32(Cam.Z / CellSize);
	int32 Rolled = 0;
	for (int32 Ring = 0; Ring <= R && Rolled < 12; Ring++)
		for (int32 j = -Ring; j <= Ring && Rolled < 12; j++)
			for (int32 i = -Ring; i <= Ring && Rolled < 12; i++)
			{
				if (FMath::Max(FMath::Abs(i), FMath::Abs(j)) != Ring) continue;
				const int32 CX = IX + i, CZ = IZ + j;
				const double X0 = (CX + .5) * CellSize, Z0 = (CZ + .5) * CellSize;
				if (Hyp(X0 - Cam.X, Z0 - Cam.Z) > SpawnR) continue;
				const int64 K = CellKey(CX, CZ);
				if (Cells.Contains(K)) continue;
				if (Schools.Num() >= MaxSchools) return;
				Rolled++;
				Cells.Add(K, -1);
				const FString Zone = ZoneAt(X0, Z0);
				const FZoneMix* Mix = ZoneMixes().FindByPredicate([&](const FZoneMix& Z) { return Zone == Z.Zone; });
				// the tide moves them: up onto the flats and in among the roots at high water, back to the channels at low
				const double TideK = (Zone == TEXT("Flats") || Zone == TEXT("Creek")) ? 1 + .5 * TideLevel() : (Zone == TEXT("Bridge") || Zone == TEXT("Hawk Channel") || Zone == TEXT("Backcountry")) ? 1 - .3 * TideLevel() : 1;
				if (!Mix || KeysSea::HashCell(0x5c4f, CX, CZ, 11) >= Mix->Density * TideK) continue;
				double Tot = 0;
				for (const TPair<int32, double>& Rw : Mix->Rows) Tot += Rw.Value;
				double Pick = KeysSea::HashCell(0x5c4f, CX, CZ, 12) * Tot;
				int32 Sp = Mix->Rows.Last().Key;
				for (const TPair<int32, double>& Rw : Mix->Rows) { Pick -= Rw.Value; if (Pick <= 0) { Sp = Rw.Key; break; } }
				const ERole Role = Specs[Sp].Role;
				// the spot: reef fish take the tallest head in the cell, grouper the foot of it, the rest anywhere
				double BX = X0, BZ = Z0, BTop = -1e9;
				for (int32 q = 0; q < 16; q++)
				{
					const double SX = X0 + (KeysSea::HashCell(0x5c4f, CX, CZ, 20 + q) - .5) * CellSize * .9;
					const double SZ = Z0 + (KeysSea::HashCell(0x5c4f, CX, CZ, 40 + q) - .5) * CellSize * .9;
					if (!ValidWater(Sp, SX, SZ)) continue;
					const double Tp = (Role == ERole::Reef || Role == ERole::Hole) ? TopAt(SX, SZ) : -DepthFast(SX, SZ) + q * 1e-3;
					if (Tp > BTop) { BTop = Tp; BX = SX; BZ = SZ; }
					if (Role != ERole::Reef && Role != ERole::Hole) break;
				}
				if (BTop < -1e8) continue;
				if (Role == ERole::Hole) { BX += 1.6; BZ += .8; }
				double Floor;
				const double Top = TopAt(BX, BZ, &Floor);
				const FSpec& Sc = Specs[Sp];
				const double Water = -Top;
				double Y;
				// (bait hangs in mid-water; this line used to be overwritten by the else below, which left every ball on the bottom)
				if (Role == ERole::Bait && Water <= 30) Y = FMath::Lerp(Top + Sc.AltMin, -1.0, .45);
				else if (Role == ERole::Bait && Water > 30) Y = -5 - 9 * KeysSea::HashCell(0x5c4f, CX, CZ, 13);   // off soundings the bait is up top, where the birds can see it
				else if (Role != ERole::Bait && Water > 45) Y = -6 - 10 * KeysSea::HashCell(0x5c4f, CX, CZ, 14);     // and so is what hunts it
				else Y = Top + FMath::Lerp(Sc.AltMin, Sc.AltMax, .5);
				Y = FMath::Min(Y, -.35 - .2 * FMath::Min(1.0, Water));
				// a flats bait school is a small one
				const double NScale = Role == ERole::Bait && DepthFast(BX, BZ) < 4 ? .35 : 1;
				if (FSchool* S = Spawn(Sp, FVector(BX, Y, BZ), Top, K, uint32(K * 2654435761u) ^ 0x9e37u, NScale)) Cells[K] = S->Id;
			}
	// wrecks: a school of grunts and snapper over the hull, a grouper beside it, a barracuda above, bait over it all
	for (int32 w = 0; w < KeysWrecks::Wrecks().Num(); w++)
	{
		const KeysWrecks::FWreck& W = KeysWrecks::Wrecks()[w];
		if (Hyp(W.X - Cam.X, W.Z - Cam.Z) > SpawnR) continue;
		const int32 Sps[] = {Schoolmaster, Grunt, Grouper, Barracuda, Herring};
		for (int32 q = 0; q < 5; q++)
		{
			const int64 K = (int64(0x7ffff) << 32) | int64(w * 8 + q);
			if (Cells.Contains(K) || Schools.Num() >= MaxSchools) continue;
			Cells.Add(K, -1);
			const double Ox = q == 2 ? W.Beam * .8 + 1 : 0, Oz = q == 2 ? 1.5 : 0;
			const double Top = TopAt(W.X + Ox, W.Z + Oz);
			const FSpec& Sc = Specs[Sps[q]];
			double Y = Top + FMath::Lerp(Sc.AltMin, Sc.AltMax, .5) + (q == 4 ? 4 : q == 3 ? 2 : 0);
			Y = FMath::Min(Y, -1.0);
			if (FSchool* S = Spawn(Sps[q], FVector(W.X + Ox, Y, W.Z + Oz), Top, K, uint32(w * 977 + q * 131 + 7), q < 2 ? 1.6 : 1)) Cells[K] = S->Id;
		}
	}
}

FVector AKeysSchools::PickGoal(FSchool& S)
{
	const FSpec& Sc = Specs[S.Sp];
	for (int32 Try = 0; Try < 8; Try++)
	{
		const double A = S.Rng.FRandRange(0, 2 * PI), Rr = Sc.Roam * FMath::Sqrt(S.Rng.FRand());
		const double X = S.Home.X + FMath::Cos(A) * Rr, Z = S.Home.Z + FMath::Sin(A) * Rr;
		if (!ValidWater(S.Sp, X, Z)) continue;
		const double Top = TopAt(X, Z);
		double Y;
		// off soundings the bait and what hunts it stay in the top of the water column (5 to 14 m, 6 to 16 m), as they
		// are put there; their goals used to be picked between the surface and the bottom, 100 m down, and they sank away
		if (Sc.Role == ERole::Bait && -Top > 30) Y = -S.Rng.FRandRange(5.0, 14.0);
		else if (Sc.Role == ERole::Hunter && -Top > 45) Y = -S.Rng.FRandRange(6.0, 16.0);
		else if (Sc.Role == ERole::Bait || Sc.Role == ERole::Hunter || Sc.Role == ERole::Hanger) Y = FMath::Lerp(Top + Sc.AltMin + S.Radius * .5, -.8 - S.Radius * .5, S.Rng.FRandRange(.25, .7));
		else Y = Top + S.Rng.FRandRange(Sc.AltMin, Sc.AltMax);
		return FVector(X, FMath::Min(Y, -.3), Z);
	}
	return S.Home;
}

void AKeysSchools::MainGoal(FSchool& S, double Dt)
{
	const FSpec& Sc = Specs[S.Sp];
	S.GoalT -= Dt;
	switch (Sc.Role)
	{
	case ERole::Reef:
	case ERole::Hole:
		// hover over the head: a slow lazy circle, a new spot now and then
		if (S.GoalT <= 0) { S.GoalT = S.Rng.FRandRange(5, 11); S.Goal = PickGoal(S); }
		break;
	case ERole::Hanger:
	{
		// barracuda hang still
		if (S.GoalT <= 0) { S.GoalT = S.Rng.FRandRange(10, 20); S.Goal = PickGoal(S); }
		// (they no longer come over to a diver: they hold their spot and turn to watch, and let you come to them)
		break;
	}
	default:
	{
		// the main leader picks its own way: a heading that drifts and swings from side to side, followed by a goal a
		// few body lengths out along it; it leans back toward home when it has roamed far, and turns away from water
		// it doesn't belong in. Its depth changes now and then. The rest of the school swims the path it leaves.
		if (!S.bWander) { S.bWander = true; S.WanderYaw = FMath::Atan2(S.Heading.Z, S.Heading.X); S.WanderTurn = 0; }
		const double Tn = Tempers[S.Sp].Turn;
		S.WanderTurn = FMath::Clamp(S.WanderTurn * (1 - Dt * .35) + S.Rng.FRandRange(-1.0, 1.0) * Dt * 1.1 * Tn, -.55 * Tn, .55 * Tn);
		S.WanderYaw += S.WanderTurn * Dt;
		const FVector ToHome = Flat(S.Home - S.Center);
		const double Far = ToHome.Size() / FMath::Max(5.0, Sc.Roam);
		if (Far > .65) S.WanderYaw += FMath::FindDeltaAngleRadians(S.WanderYaw, FMath::Atan2(ToHome.Z, ToHome.X)) * FMath::Min(1.0, Dt * .9 * Far);
		const double Ahead = FMath::Max(4.0, Sc.Cruise * S.Pace * 4);
		double GX = S.Center.X + FMath::Cos(S.WanderYaw) * Ahead, GZ = S.Center.Z + FMath::Sin(S.WanderYaw) * Ahead;
		if (!ValidWater(S.Sp, GX, GZ)) { S.WanderTurn = S.WanderTurn >= 0 ? .55 : -.55; S.WanderYaw += S.WanderTurn * Dt * 2; }
		if (S.GoalT <= 0) { S.GoalT = S.Rng.FRandRange(7, 14); S.Goal.Y = PickGoal(S).Y; }
		S.Goal = FVector(GX, S.Goal.Y, GZ);
		break;
	}
	}
	// predators go hunting: pick a school within reach, charge through it and out the far side
	if (Sc.bPredator)
	{
		S.HuntT -= Dt;
		if (S.HuntLeft > 0)
		{
			S.HuntLeft -= Dt;
			FSchool* Prey = Find(S.PreyId);
			if (Prey && S.HuntLeft > 1.2) S.HuntDir = (Prey->Center - S.Center).GetSafeNormal();
			S.Goal = S.Center + S.HuntDir * 12;
			if (S.HuntLeft <= 0) S.GoalT = 0;
		}
		else if (S.HuntT <= 0)
		{
			S.HuntT = S.Rng.FRandRange(Sc.Role == ERole::Hanger ? 18 : 10, Sc.Role == ERole::Patrol ? 40 : 26);
			if (FSchool* Prey = NearestOfRole(S.Center, Sc.Role == ERole::Hanger, S.Id, Sc.Role == ERole::Patrol ? 60 : 40))
			{
				S.PreyId = Prey->Id;
				const double D = (Prey->Center - S.Center).Size();
				S.HuntLeft = D / Sc.Burst + 1.8;
				S.HuntDir = (Prey->Center - S.Center).GetSafeNormal();
			}
		}
	}
}

// ================================================================ the boids
void AKeysSchools::BuildGrid()
{
	// every fish, hashed by species and a cell the size of its species' sight: a fish's neighbours are all in the 27
	// cells round its own
	GridItems.Reset();
	GridNext.Reset();
	GridHead.Reset();
	for (int32 s = 0; s < Schools.Num(); s++)
	{
		const FSchool& S = Schools[s];
		if (Boids[S.Sp].Nbr <= 0) continue;
		const double Cs = SightM(S.Sp);
		for (int32 i = 0; i < S.Fish.Num(); i++)
		{
			const FVector& P = S.Fish[i].P;
			const int64 K = GridKey(S.Sp, FMath::FloorToInt32(P.X / Cs), FMath::FloorToInt32(P.Y / Cs), FMath::FloorToInt32(P.Z / Cs));
			const int32 Ix = GridItems.Add({s, i});
			int32& H = GridHead.FindOrAdd(K, -1);
			GridNext.Add(H);
			H = Ix;
		}
	}
}

void AKeysSchools::StepSchool(FSchool& S, double Dt)
{
	const FSpec& Sc = Specs[S.Sp];
	// the school as a whole: where it is, which way it's going, what it means to do
	FVector C = FVector::ZeroVector, Cv = FVector::ZeroVector;
	double Al = 0;
	for (const FFish& F : S.Fish) { C += F.P; Cv += F.V; Al = FMath::Max(Al, F.Alarm); }
	const int32 N = FMath::Max(1, S.Fish.Num());
	S.Center = C / N;
	S.CenterV = Cv / N;
	S.Alarm = Al;
	if (S.CenterV.SizeSquared() > .01) S.Heading = FMath::Lerp(S.Heading, S.CenterV.GetSafeNormal(), FMath::Min(1.0, Dt * 2)).GetSafeNormal();
	MainGoal(S, Dt);
	// a bait ball with a predator about balls up tight and mills round in a vortex; it loosens again once it's gone
	if (Boids[S.Sp].Mill > 0)
	{
		bool bPred = false;
		for (const FThreat& T : Threats)
			if ((T.Kind == 2 || T.Kind == 3) && (T.P - S.Center).Size() < 22 + S.Radius) { bPred = true; break; }
		const double Want = bPred ? 1.0 : 0.0;
		S.Mill = FMath::Clamp(S.Mill + (Want - S.Mill) * FMath::Min(1.0, Dt * (Want > S.Mill ? 1.6 : .25)), 0.0, 1.0);
	}
	// last step's fear, for the wave to spread from
	for (FFish& F : S.Fish) F.AlarmIn = F.Alarm;
	(void)Sc;
}

void AKeysSchools::StepFish(FSchool& S, int32 I, double Dt, int32 MaxNbr)
{
	const FSpec& Sc = Specs[S.Sp];
	const FBoid& B = Boids[S.Sp];
	const FTemper& Tm = Tempers[S.Sp];
	FFish& F = S.Fish[I];
	FishSteps++;
	const double Space = Sc.Spacing * F.Len;
	const double Cruise = Sc.Cruise * S.Pace * F.Pace, Burst = Sc.Burst * FMath::Sqrt(S.Pace) * F.Pace;
	FVector Acc = FVector::ZeroVector;

	// ---- the neighbours: the nearest few of its own kind it can see
	FVector SumV = FVector::ZeroVector, SumP = FVector::ZeroVector, Sep = FVector::ZeroVector, FleeIn = FVector::ZeroVector;
	double MaxAl = 0;
	int32 Nn = 0;
	if (MaxNbr > 0)
	{
		const double Cs = SightM(S.Sp), Sight2 = FMath::Square(FMath::Min(Cs, B.Sight * Space)), SepD = B.Sep * Space;
		const int32 Cx = FMath::FloorToInt32(F.P.X / Cs), Cy = FMath::FloorToInt32(F.P.Y / Cs), Cz = FMath::FloorToInt32(F.P.Z / Cs);
		for (int32 dz = -1; dz <= 1 && Nn < MaxNbr; dz++)
			for (int32 dy = -1; dy <= 1 && Nn < MaxNbr; dy++)
				for (int32 dx = -1; dx <= 1 && Nn < MaxNbr; dx++)
				{
					const int32* H = GridHead.Find(GridKey(S.Sp, Cx + dx, Cy + dy, Cz + dz));
					for (int32 It = H ? *H : -1; It >= 0 && Nn < MaxNbr; It = GridNext[It])
					{
						const FRef& R = GridItems[It];
						const FFish& O = Schools[R.School].Fish[R.Fish];
						if (&O == &F) continue;
						const FVector D = F.P - O.P;
						const double D2 = D.SizeSquared();
						if (D2 > Sight2 || D2 < 1e-8) continue;
						Nn++;
						SumV += O.V;
						SumP += O.P;
						const double Dd = FMath::Sqrt(D2);
						if (Dd < SepD) Sep += D / Dd * (SepD - Dd) / SepD;
						// fear passes on from whoever has it (as it was a step ago, so it travels as a wave)
						if (O.AlarmIn > MaxAl) MaxAl = O.AlarmIn;
						FleeIn += O.FleeDir * O.AlarmIn;
					}
				}
	}
	NbrAcc += Nn;
	if (Nn > 0)
	{
		const FVector AvgV = SumV / Nn, AvgP = SumP / Nn;
		Acc += (AvgV - F.V) * B.Align * 1.2;                             // alignment
		Acc += (AvgP - F.P) * B.Cohere * 1.1 / FMath::Max(.2, Space);   // cohesion
		Acc += Sep * B.SepK * Sc.Accel;                                  // separation
	}

	// ---- the school's intent: its wander, its coral head, its hunt; and back to the others if it has strayed
	const bool bHold = Sc.Role == ERole::Reef || Sc.Role == ERole::Hole || Sc.Role == ERole::Hanger;
	if (bHold)
	{
		// a lazy aggregation: each fish drifts round the spot, never quite still
		const FVector To = S.Goal - F.P;
		const double Rr = S.Radius * (Sc.Role == ERole::Reef ? 1.1 : .4);
		const double Out = FMath::Max(0.0, Flat(To).Size() - Rr * .5) / FMath::Max(.3, Rr);
		Acc += Flat(To).GetSafeNormal() * Out * Cruise * B.Goal * 2.0;
	}
	else
	{
		FVector To = S.Goal - S.Center;
		const double Dg = To.Size();
		const FVector Want = Dg > .01 ? To / Dg * Cruise * FMath::Min(1.0, Dg / 3.0) : FVector::ZeroVector;
		const double Gk = (Sc.bPredator && S.HuntLeft > 0) ? 2.5 : 1.0;
		Acc += (Want * (Sc.bPredator && S.HuntLeft > 0 ? Burst / FMath::Max(.1, Cruise) : 1.0) - F.V) * B.Goal * .6 * Gk;
	}
	{
		// cut off from its school (round a coral head, past a predator): it finds its way back once the fright passes
		const FVector ToC = S.Center - F.P;
		const double Dc = ToC.Size(), Far = (Dc - S.Radius * 1.6) / FMath::Max(.3, S.Radius);
		if (Far > 0 && Dc > 1e-3) Acc += ToC / Dc * FMath::Min(1.0, Far) * Cruise * (1 - .8 * F.Alarm) * .9;
	}

	// ---- milling: round and round the ball's middle, a tight vortex
	if (S.Mill > .01)
	{
		const FVector R0 = Flat(F.P - S.Center);
		const double Rd = R0.Size();
		if (Rd > 1e-3)
		{
			const FVector Tan = FVector(-R0.Z, 0, R0.X) / Rd * S.MillDir;
			const double Ring = S.Radius * .55;
			const FVector Want = Tan * Cruise * 1.7 - R0 / Rd * (Rd - Ring) * 1.2 + FVector(0, (S.Center.Y - F.P.Y) * .6, 0);
			Acc += (Want - F.V) * S.Mill * 2.4;
		}
	}

	// ---- threats: run, and the fear runs through the school ahead of any of them
	FVector Flee = FVector::ZeroVector;
	double Direct = 0;
	for (const FThreat& T : Threats)
	{
		if (T.SchoolId == S.Id) continue;
		if (Sc.bPredator && T.Kind != 1 && T.Kind != 0) continue;   // hunters mind only the boat and the diver
		double R = T.R;
		if (T.Kind == 0) R = Sc.DiverR * T.R;
		else if (T.Kind == 2 && Sc.Role != ERole::Bait && T.Power < .9) continue;   // reef and flats fish mind a charge
		else if (T.Kind == 4 && Sc.Role != ERole::Bait && Sc.Role != ERole::Flats) R *= .5;
		if (Sc.Role == ERole::Hanger && T.Kind == 0) R *= .5;
		FVector D = F.P - T.P;
		const double Dd = D.Size();
		if (Dd > R || Dd < 1e-3) continue;
		const double K = (1 - Dd / R) * T.Power;
		FVector Dir = D / Dd;
		if ((T.Kind == 2 || T.Kind == 0 || T.Kind == 1) && !T.V.IsNearlyZero())
		{
			// the fountain: off the line the threat is coming along, not straight ahead of it
			const FVector Vn = T.V.GetSafeNormal();
			FVector Lat = D - Vn * FVector::DotProduct(D, Vn);
			if (Lat.SizeSquared() < 1e-4) Lat = FVector::CrossProduct(Vn, FVector(0, 1, 0)) * (F.Seed < .5 ? -1 : 1);
			Dir = (Lat.GetSafeNormal() * 1.6 + Dir * .5).GetSafeNormal();
		}
		Dir.Y *= .45;
		Flee += Dir * K;
		Direct = FMath::Max(Direct, FMath::Min(1.0, K * 1.8));
	}
	const double Prev = F.Alarm;
	if (Direct > 0)
	{
		F.Alarm = FMath::Max(F.Alarm, Direct);
		F.FleeDir = Flee.GetSafeNormal();
	}
	else if (MaxAl * .9 > F.Alarm + .02)
	{
		// caught from the neighbours: quickly, but not at once, and it bolts the way they're bolting
		F.Alarm = FMath::Min(MaxAl * .9, F.Alarm + Dt * 9);
		if (!FleeIn.IsNearlyZero()) F.FleeDir = FleeIn.GetSafeNormal();
	}
	else F.Alarm = FMath::Max(0.0, F.Alarm - Dt * .45);
	// the flash: the moment it takes fright it rolls onto its side and the light catches its flank
	if (F.Alarm - Prev > Dt * 3.5 && F.Alarm > .4 && F.Flash < .25)
	{
		F.Flash = 1;
		F.FlashSide = FVector::CrossProduct(F.Fwd, F.FleeDir).Y >= 0 ? 1 : -1;
		Flashes++;
	}
	F.Flash = FMath::Max(0.0, F.Flash - Dt * 2.2);
	if (F.Alarm > .02 && !F.FleeDir.IsNearlyZero()) Acc += (F.FleeDir * Burst - F.V) * F.Alarm * 3.0 * (1 - .5 * S.Mill);
	if (Sc.Role == ERole::Hole && F.Alarm > .3) Acc.Y = FMath::Min(Acc.Y, -.5);   // a grouper backs down into its hole

	// ---- its depth band, the bottom, coral heads and wrecks ahead, the surface
	const double TopHere = TopAt(F.P.X, F.P.Z);
	double WantY;
	if (Lives(Sc.Role)) WantY = TopHere + F.BandY;
	else
	{
		const double Lo = TopHere + Sc.AltMin + F.Len, Hi = SurfY - .35 - F.Len * .5;
		WantY = FMath::Clamp(S.Goal.Y + F.BandY * (1 - .5 * S.Mill), FMath::Min(Lo, Hi), Hi);
	}
	Acc.Y += (WantY - F.P.Y) * .9 - F.V.Y * .6;
	const double Spd0 = F.V.Size();
	const FVector Vn = Spd0 > .02 ? F.V / Spd0 : F.Fwd;
	{
		const double Look = FMath::Max(.5, Spd0 * .9) + F.Len;
		const FVector Q = F.P + Flat(Vn).GetSafeNormal() * Look;
		const double TopQ = TopAt(Q.X, Q.Z);
		const double Clear = F.P.Y - F.Len * .6;
		if (TopQ > Clear)
		{
			// in the way at its depth: round the clear side (the school parts round it and closes up behind)
			const FVector Lft = Flat(Vn).GetSafeNormal().RotateAngleAxis(50, FVector(0, 1, 0)), Rgt = Flat(Vn).GetSafeNormal().RotateAngleAxis(-50, FVector(0, 1, 0));
			const double TL = TopAt(F.P.X + Lft.X * Look, F.P.Z + Lft.Z * Look), TR = TopAt(F.P.X + Rgt.X * Look, F.P.Z + Rgt.Z * Look);
			const FVector Side = TL < TR ? Lft : Rgt;
			const double Hard = FMath::Min(1.5, (TopQ - Clear) / FMath::Max(.2, F.Len) + .3);
			Acc += Side * Sc.Accel * 2.0 * Hard + FVector(0, Sc.Accel * .6 * Hard, 0);
		}
	}

	// ---- integrate: limited acceleration and turn rate, its own speed, no snapping
	const double Pref = FMath::Lerp(bHold ? Cruise * .45 : Cruise, Burst, FMath::Min(1.0, F.Alarm * 1.2));
	const double MaxA = Sc.Accel * (1 + 1.5 * F.Alarm) * 2.0;
	FVector NewV = F.V + Acc.GetClampedToMaxSize(MaxA) * Dt;
	double Spd = NewV.Size();
	Spd += (Pref - Spd) * FMath::Min(1.0, Dt * (F.Alarm > .2 ? 3.0 : 1.2));
	Spd = FMath::Clamp(Spd, bHold ? .03 : Cruise * .25, Burst * 1.15);
	FVector Dir = NewV.SizeSquared() > 1e-6 ? NewV.GetSafeNormal() : Vn;
	const double MaxTurn = (1.6 + 1.2 * FMath::Min(1.0, .35 / FMath::Max(.08, F.Len))) * Tm.Agile * (1 + 1.2 * F.Alarm) * Dt;
	const double Ang = FMath::Acos(FMath::Clamp(FVector::DotProduct(Vn, Dir), -1.0, 1.0));
	if (Ang > MaxTurn && Ang > 1e-4) Dir = FQuat::Slerp(FQuat::Identity, FQuat::FindBetweenNormals(Vn, Dir), MaxTurn / Ang).RotateVector(Vn).GetSafeNormal();
	Dir.Y = FMath::Clamp(Dir.Y, -.5, .5);
	F.V = Dir.GetSafeNormal() * Spd;
	F.P += F.V * Dt;
	const double TopNow = FMath::Max(TopHere, TopAt(F.P.X, F.P.Z));
	if (F.P.Y < TopNow + .08 + F.Len * .2) { F.P.Y = TopNow + .08 + F.Len * .2; F.V.Y = FMath::Max(0.0, F.V.Y); }
	if (F.P.Y > SurfY - .2 - F.Len * .2) { F.P.Y = SurfY - .2 - F.Len * .2; F.V.Y = FMath::Min(0.0, F.V.Y); }

	// ---- the body: face the way it swims, bank into turns, roll flat on its side in a flash; the tail beat
	const FVector OldF = F.Fwd;
	FVector Face = Spd > .04 ? F.V / Spd : F.Fwd;
	Face.Y = FMath::Clamp(Face.Y, -.45, .45);
	F.Fwd = FMath::Lerp(F.Fwd, Face.GetSafeNormal(), FMath::Min(1.0, Dt * 7)).GetSafeNormal();
	const double Yaw0 = FMath::Atan2(OldF.Z, OldF.X), Yaw1 = FMath::Atan2(F.Fwd.Z, F.Fwd.X);
	const double Yr = Dt > 0 ? FMath::FindDeltaAngleRadians(Yaw0, Yaw1) / Dt : 0;
	F.YawRate += (Yr - F.YawRate) * FMath::Min(1.0, Dt * 8);
	const double Bank = FMath::Clamp(F.YawRate * Spd * .55, -.75, .75);
	const double RollWant = Bank + F.FlashSide * F.Flash * 1.25;
	F.Roll += (RollWant - F.Roll) * FMath::Min(1.0, Dt * (F.Flash > .1 ? 14 : 5));
	const double Rel = Spd / FMath::Max(.05, F.Len);
	F.Freq = 2 * PI * FMath::Clamp(.9 + .55 * Rel, .9, 9.0);
	F.Phase = FMath::Fmod(F.Phase + F.Freq * Dt, 2 * PI * 64);
	F.Amp += (FMath::Clamp(.22 + .16 * Rel + .5 * F.Alarm, .2, 1.0) - F.Amp) * FMath::Min(1.0, Dt * 3);
}

// ================================================================ per frame
void AKeysSchools::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	const double T0 = FPlatformTime::Seconds();
	const double Dt = FMath::Clamp(double(DeltaSeconds), 0.0, .1);
	NowT = GetWorld()->GetTimeSeconds();
	SurfY = KeysOcean::SeaLevel();   // the surface the fish keep under goes up and down with the tide, when it is drawn so
	TilesThisFrame = 0;
	if (bOff) { FrameAcc += DeltaSeconds; CpuN++; StatT += Dt; if (bScriptLog && StatT > 5) { StatT = 0; DebugCmd(TEXT("stats")); } return; }
	ReadWorld(Dt);
	PopT -= Dt;
	if (PopT <= 0) { PopT = .5; Populate(); }
	BuildGrid();
	// every frame up close, every 2nd / 4th frame further off (with the time they missed), and far schools heed
	// fewer neighbours
	static uint32 TickN = 0;
	TickN++;
	for (FSchool& S : Schools)
	{
		const double D = Hyp(S.Center.X - Cam.X, S.Center.Z - Cam.Z);
		const uint32 Every = D < 35 ? 1 : D < 80 ? 2 : 4;
		S.Step += Dt;
		S.bStepped = false;
		if ((TickN + uint32(S.Id)) % Every) continue;
		const double Sd = FMath::Min(S.Step, .12);
		S.Step = 0;
		StepSchool(S, Sd);
		const int32 Nbr = Every >= 4 ? FMath::Max(3, Boids[S.Sp].Nbr / 2) : Boids[S.Sp].Nbr;
		for (int32 i = 0; i < S.Fish.Num(); i++) StepFish(S, i, Sd, Nbr);
		S.bStepped = true;
	}
	// detail level: a different mesh up close, mid and far
	for (FSchool& S : Schools)
	{
		const double D = (S.Center - Cam).Size();
		int32 Lod = D < 16 ? 0 : D < 45 ? 1 : 2;
		if (S.Len < .25) Lod = FMath::Max(1, Lod);
		if (S.Lod >= 0 && FMath::Abs(Lod - S.Lod) == 1)
		{
			const double Edge = (FMath::Max(Lod, S.Lod) == 1) ? 16 : 45;
			if (FMath::Abs(D - Edge) < 4) Lod = S.Lod;   // hysteresis
		}
		if (Lod != S.Lod)
		{
			const int32 P = PoolFor(S.Sp, Lod);
			if (P < 0) continue;
			while (PoolMoved.Num() < Pools.Num()) PoolMoved.Add(false);
			if (PoolDirty.IsValidIndex(S.Pool)) PoolDirty[S.Pool] = true;
			S.Pool = P;
			S.Lod = Lod;
			PoolDirty[P] = true;
		}
	}
	while (PoolMoved.Num() < Pools.Num()) PoolMoved.Add(false);
	FlushPools();
	for (FSchool& S : Schools) if (S.bStepped && PoolDirty.IsValidIndex(S.Pool)) WriteInstances(S);
	for (int32 P = 0; P < Pools.Num(); P++) if (PoolMoved[P] && Pools[P]) { Pools[P]->MarkRenderStateDirty(); PoolMoved[P] = false; }
	CpuAcc += FPlatformTime::Seconds() - T0;
	FrameAcc += DeltaSeconds;
	CpuN++;
	StatT += Dt;
	if (bScriptLog && StatT > 5) { StatT = 0; DebugCmd(TEXT("stats")); }
}

// ================================================================ tests
void AKeysSchools::DebugCmd(const FString& Cmd)
{
	FString C = Cmd, A;
	Cmd.Split(TEXT("="), &C, &A);
	if (C.IsEmpty()) C = Cmd;
	if (C == TEXT("body"))
	{
		// how each school holds together: mean and furthest fish from its centre in school radii, how many are well away
		// from it (split off), how hard it's milling and how frightened it is
		for (const FSchool& S : Schools)
		{
			if (S.Fish.Num() < 3) continue;
			double Sum = 0, Mx = 0;
			int32 Off = 0;
			for (const FFish& F : S.Fish)
			{
				const double D = (F.P - S.Center).Size() / FMath::Max(.1, S.Radius);
				Sum += D; Mx = FMath::Max(Mx, D);
				if (D > 2) Off++;
			}
			UE_LOG(LogTemp, Log, TEXT("KRSchools body %s #%d: %d fish, mean %.2f radii, furthest %.2f, %d split off, mill %.2f, alarm %.2f, speed %.2f m/s"),
				Specs[S.Sp].Key, S.Id, S.Fish.Num(), Sum / S.Fish.Num(), Mx, Off, S.Mill, S.Alarm, S.CenterV.Size());
		}
		UE_LOG(LogTemp, Log, TEXT("KRSchools body: %d wrapped round the bubble so far"), WrapCount);
		return;
	}
	// the point in front of the camera (or the diver), a little way off
	FVector Eye = Cam, Fw = FVector(1, 0, 0);
	if (APlayerCameraManager* PC = UGameplayStatics::GetPlayerCameraManager(this, 0))
	{
		const FVector D = PC->GetCameraRotation().Vector();
		Fw = FVector(D.X, D.Z, D.Y).GetSafeNormal();
	}
	const double Ahead = A.IsNumeric() ? FCString::Atod(*A) : 9;   // put=<species> passes a name, not a distance
	const FVector Flat2 = Flat(Fw).GetSafeNormal();
	FVector At = Eye + Flat2 * Ahead;
	auto Put = [&](int32 Sp, double Y, double NScale) -> FSchool*
	{
		if (Sp < 0) return nullptr;
		if (Specs[Sp].Role == ERole::Reef || Specs[Sp].Role == ERole::Hole)
		{
			// the tallest coral head near that spot
			double BT = TopAt(At.X, At.Z);
			const FVector C0 = At;
			for (int32 q = 0; q < 60; q++)
			{
				const double Ang = q * 2.39996, Rr = FMath::Sqrt((q + .5) / 60) * 5;
				const FVector Q = C0 + FVector(FMath::Cos(Ang) * Rr, 0, FMath::Sin(Ang) * Rr);
				const double Tq = TopAt(Q.X, Q.Z);
				if (Tq > BT + .05) { BT = Tq; At = Q; }
			}
		}
		double Fl;
		const double Top = TopAt(At.X, At.Z, &Fl);
		const double Yc = FMath::Clamp(Y, Top + Specs[Sp].AltMin + .5, -.6);
		FSchool* S = Spawn(Sp, FVector(At.X, Yc, At.Z), Top, 0, uint32(FPlatformTime::Cycles()), NScale);
		if (S)
		{
			S->bPinned = true;
			S->Goal = S->Home;
			S->GoalT = 6;
			// broadside to the view
			const FVector Side = FVector::CrossProduct(Flat2, FVector(0, 1, 0)).GetSafeNormal();
			for (FFish& F : S->Fish) { F.Fwd = Side; F.V = Side * Specs[Sp].Cruise * .4; }
			S->Heading = Side;
			LastPut = S->Id;
		}
		UE_LOG(LogTemp, Log, TEXT("KRSchools put %s at %.1f %.1f %.1f (top %.1f): %d fish"), Specs[Sp].Key, At.X, Yc, At.Z, Top, S ? S->Fish.Num() : 0);
		return S;
	};
	if (C == TEXT("put"))
	{
		// put=<species>: any of them, a few metres ahead (reef and hole fish near the floor, the rest at eye level)
		for (int32 k = 0; k < NumSpec; k++)
			if (A == Specs[k].Key)
			{
				const bool bLow = Specs[k].Role == ERole::Reef || Specs[k].Role == ERole::Hole;
				Put(k, bLow ? -100 : Eye.Y, 1.2);
				break;
			}
	}
	else if (C == TEXT("clearall")) { for (int32 i = Schools.Num() - 1; i >= 0; i--) Despawn(i); }
	else if (C == TEXT("bait")) Put(Silverside, Eye.Y, 1.1);
	else if (C == TEXT("herring")) Put(Herring, Eye.Y, 1);
	else if (C == TEXT("snapper")) Put(Schoolmaster, -100, 1.4);
	else if (C == TEXT("grunt")) Put(Grunt, -100, 1.4);
	else if (C == TEXT("grouper")) Put(Grouper, -100, 1);
	else if (C == TEXT("cuda")) Put(Barracuda, Eye.Y, 1);
	else if (C == TEXT("flats"))
	{
		// bonefish, permit and a little bait school around the boat
		const FVector Save = At;
		for (int32 q = 0; q < 4; q++)
		{
			const double Ang = q * PI / 2 + .4;
			const FVector P = BoatP + FVector(FMath::Cos(Ang), 0, FMath::Sin(Ang)) * (7 + q * 2);
			const int32 Sp = q == 0 || q == 2 ? Bonefish : q == 1 ? Permit : Silverside;
			double Fl;
			const double Top = TopAt(P.X, P.Z, &Fl);
			if (FSchool* S = Spawn(Sp, FVector(P.X, FMath::Min(Top + .25, -.4), P.Z), Top, 0, 1234 + q, Sp == Silverside ? .4 : 1.3)) S->bPinned = true;
		}
		UE_LOG(LogTemp, Log, TEXT("KRSchools flats around %.0f %.0f (%.0f %.0f)"), BoatP.X, BoatP.Z, Save.X, Save.Z);
	}
	else if (C == TEXT("pred") || C == TEXT("shark"))
	{
		// a predator charges the nearest school from off to one side
		FSchool* Prey = NearestOfRole(Eye, false, -1, 80);
		if (!Prey) { UE_LOG(LogTemp, Log, TEXT("KRSchools pred: nothing to hunt")); return; }
		const int32 PreyId = Prey->Id, PreySp = Prey->Sp, PreyN = Prey->Fish.Num();
		const FVector PreyP = Prey->Center;
		const int32 Sp = C == TEXT("shark") ? Blacktip : Jack;
		const FVector Side = FVector::CrossProduct(Flat2, FVector(0, 1, 0)).GetSafeNormal();
		const FVector From = PreyP + Side * 16 + Flat2 * 3;
		double Fl;
		const double Top = TopAt(From.X, From.Z, &Fl);
		if (FSchool* S = Spawn(Sp, FVector(From.X, FMath::Max(PreyP.Y, Top + 1), From.Z), Top, 0, 4242, 1))
		{
			S->bPinned = true;
			S->PreyId = PreyId;
			S->HuntDir = (PreyP - S->Center).GetSafeNormal();
			S->HuntLeft = (PreyP - S->Center).Size() / Specs[Sp].Burst + 2;
			S->HuntT = 30;
			for (FFish& F : S->Fish) { F.Fwd = S->HuntDir; F.V = S->HuntDir * Specs[Sp].Burst * .7; }
			UE_LOG(LogTemp, Log, TEXT("KRSchools %s charges the %s school (%d fish)"), Specs[Sp].Key, Specs[PreySp].Key, PreyN);
		}
	}
	else if (C == TEXT("scare")) Startle(At.X - Flat2.X * Ahead * .4, At.Z - Flat2.Z * Ahead * .4, 9);
	else if (C == TEXT("look"))
	{
		// point the diver's mask at the school put last (or the nearest one), from where it is now
		FSchool* S = Find(LastPut);
		if (!S) S = NearestOfRole(Eye, false, -1, 60);
		AKeysDiver* Dv = AKeysDiver::Get(this);
		if (S && Dv && Dv->IsDiving())
		{
			const FVector Mid = S->Center;
			const KeysDive::FState& St = Dv->State();
			const FVector Dir = (Mid - FVector(St.X, St.Y + .15, St.Z)).GetSafeNormal();
			Dv->DebugCmd(TEXT("look"), FString::Printf(TEXT("%f,%f"), FMath::Atan2(-Dir.X, -Dir.Z), FMath::Asin(FMath::Clamp(Dir.Y, -1.0, 1.0))));
		}
	}
	else if (C == TEXT("tobait"))
	{
		// tobait[=m]: take the diver to the nearest bait ball the game itself put there, m metres (7: outside the range it shies from a diver) off its edge at its
		// own depth, looking at it (a ball is a few metres across in 140 m of water: this is how a test finds one)
		FSchool* S = nullptr;
		double BD = 400;
		for (FSchool& O : Schools)
			if (Specs[O.Sp].Role == ERole::Bait && !O.bPinned && O.Fish.Num() >= 60 && (O.Center - Eye).Size() < BD) { BD = (O.Center - Eye).Size(); S = &O; }
		if (!S) { UE_LOG(LogTemp, Log, TEXT("KRSchools tobait: no bait school within reach")); return; }
		LastPut = S->Id;
		FVector Dir = Flat(S->Center - Eye).GetSafeNormal();
		if (Dir.IsNearlyZero()) Dir = FVector(1, 0, 0);
		// from the side it is nearest on, unless coral stands in the line of sight: then the first clear side round from it
		const double Off = S->Radius + (A.IsNumeric() ? FCString::Atod(*A) : 7.0);
		for (int32 q = 0; q < 12; q++)
		{
			const FVector Dq = Dir.RotateAngleAxis((q % 2 ? -1 : 1) * ((q + 1) / 2) * 30.0, FVector(0, 1, 0));
			bool bClear = true;
			for (double R = 0; R <= Off + 1 && bClear; R += .75)
				bClear = TopAt(S->Center.X - Dq.X * R, S->Center.Z - Dq.Z * R) < S->Center.Y - .8;
			if (bClear) { Dir = Dq; break; }
		}
		const FVector P = S->Center - Dir * Off;
		UE_LOG(LogTemp, Log, TEXT("KRSchools tobait %s #%d: %d fish, centre %.0f %.1f %.0f, radius %.1f m, %.0f m away, %.1f m of water there"),
			Specs[S->Sp].Key, S->Id, S->Fish.Num(), S->Center.X, S->Center.Y, S->Center.Z, S->Radius, (S->Center - Eye).Size(), DepthFast(S->Center.X, S->Center.Z));
		if (AKeysDiver* Dv = AKeysDiver::Get(this); Dv && Dv->IsDiving())
		{
			Dv->DebugCmd(TEXT("at"), FString::Printf(TEXT("%f,%f"), P.X, P.Z));
			Dv->DebugCmd(TEXT("depth"), FString::Printf(TEXT("%f"), -S->Center.Y));
			DebugCmd(TEXT("look"));
		}
	}
	else if (C == TEXT("off") || C == TEXT("on"))
	{
		// frame-time comparison: no school fish at all
		bOff = C == TEXT("off");
		SetActorHiddenInGame(bOff);
	}
	else if (C == TEXT("clear")) { for (int32 i = Schools.Num() - 1; i >= 0; i--) Despawn(i); Cells.Reset(); }
	else if (C == TEXT("stats"))
	{
		int32 NF = 0;
		for (const FSchool& S : Schools) NF += S.Fish.Num();
		UE_LOG(LogTemp, Log, TEXT("KRSchools stats: %d schools, %d fish (boids), %d fish steps/frame, %.1f neighbours each, %d flashes, %d tiles; CPU %.3f ms/frame avg, frame %.2f ms avg (%.0f fps); %d pool rebuilds"),
			Schools.Num(), NF, CpuN ? FishSteps / CpuN : 0, FishSteps ? NbrAcc / FishSteps : 0.0, Flashes, Tiles.Num(), CpuN ? CpuAcc / CpuN * 1000 : 0, CpuN ? FrameAcc / CpuN * 1000 : 0, FrameAcc > 0 ? CpuN / FrameAcc : 0, Rebuilds);
		CpuAcc = FrameAcc = NbrAcc = 0; CpuN = Rebuilds = FishSteps = Flashes = 0;
	}
}
