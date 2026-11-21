#include "KeysSeafloor.h"
#include "KeysSea.h"
#include "KeysOcean.h"
#include "KeysGeo.h"
#include "ProceduralMeshComponent.h"
#include "Materials/Material.h"
#include "MaterialDomain.h"
#include "Kismet/GameplayStatics.h"
#include "Camera/PlayerCameraManager.h"
#include "Engine/World.h"
#include "Async/ParallelFor.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"

using namespace KeysGeo;

namespace
{
	constexpr double ChunkSize = 64;
	constexpr double SkirtDepth = 3;            // meters each chunk's perimeter drops, hides LOD seams
	constexpr double ScanDist = 16;             // re-evaluate residency after the camera moves this far
	constexpr int32 BuildBudget = 3;            // chunk builds per tick
	constexpr int32 WorldChunkMargin = 3;
	constexpr double SubmergeY = -2, SurfaceY = -.3;   // hysteresis for the surfaced / submerged profiles
	// The web game's skirt reaches its 1,900 m horizon; the water mesh here reaches 5.2 km, so the skirt does too.
	// The skirt is a grid on the chunks' own 64 m lattice near the camera (SkirtInner cells each way), stretching out
	// beyond that, with a hole wherever a chunk is built. The two never overlap, so the skirt sits at the true depth:
	// the water absorbs light by the metre, and a skirt sunk under the chunks (as it used to be, by 0.1 to 0.5 m) showed
	// through it as a darker surround - the chunk area read as a brighter square on the flats.
	constexpr int32 SkirtInner = 8;             // chunks stay resident up to 286 m (4.5 cells) from a camera that may be 2 cells off centre
	constexpr int32 SkirtSub = 2;               // skirt cells per chunk side on the lattice (32 m: closer to the chunks' own colours at the join)
	constexpr int32 SkirtOuter = 24;            // stretched rows each side: 64 m growing 10% a row reaches 6.2 km
	constexpr double SkirtGrow = 1.1;

	// -KRSeaDebug paints near chunks red, mid chunks blue and the far skirt green
	bool DebugColors() { static const bool b = FParse::Param(FCommandLine::Get(), TEXT("KRSeaDebug")); return b; }

	// -KRSeaNoChunks / -KRSeaNoSkirt leave one of the two out, to look at the other alone
	bool NoChunks() { static const bool b = FParse::Param(FCommandLine::Get(), TEXT("KRSeaNoChunks")); return b; }
	bool NoSkirt() { static const bool b = FParse::Param(FCommandLine::Get(), TEXT("KRSeaNoSkirt")); return b; }

	int64 Key(int32 CX, int32 CZ) { return (int64(CX) << 32) ^ int64(uint32(CZ)); }
}

UMaterialInterface* AKeysSeafloor::FloorMaterial()
{
	static TWeakObjectPtr<UMaterialInterface> M;
	if (!M.IsValid())
	{
		M = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/KeysRun/Materials/M_KR_Seafloor.M_KR_Seafloor"));
		if (!M.IsValid()) M = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/KeysRun/Materials/M_KR_VertexColor.M_KR_VertexColor"));
		if (!M.IsValid()) M = UMaterial::GetDefaultMaterial(MD_Surface);
	}
	return M.Get();
}

AKeysSeafloor::AKeysSeafloor()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickGroup = TG_PostUpdateWork;   // after the camera has moved this frame
	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent = Root;
	Chunks = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("Chunks"));
	Chunks->SetupAttachment(Root);
	Chunks->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Chunks->bUseAsyncCooking = true;
	Chunks->SetCastShadow(false);   // (see Tick: back on for a diver)
	Chunks->bAffectDynamicIndirectLighting = false;
	Skirt = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("Skirt"));
	Skirt->SetupAttachment(Root);
	Skirt->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Skirt->SetCastShadow(false);
	Skirt->bAffectDynamicIndirectLighting = false;
}

void AKeysSeafloor::BeginPlay()
{
	Super::BeginPlay();
	KeysGeo::Init();
	// abyssal backstop: a dark plane far below everything, so an unstreamed gap shows dark water, not sky
	const double B = 40000, Y = -300;
	const TArray<FVector> V = {ToUE(-B, Y, -B), ToUE(B, Y, -B), ToUE(-B, Y, B), ToUE(B, Y, B)};
	const TArray<FVector> N = {FVector::UpVector, FVector::UpVector, FVector::UpVector, FVector::UpVector};
	const TArray<FVector2D> UV = {FVector2D::ZeroVector, FVector2D::ZeroVector, FVector2D::ZeroVector, FVector2D::ZeroVector};
	const FLinearColor Dk(.016f, .039f, .078f);
	const TArray<FLinearColor> C = {Dk, Dk, Dk, Dk};
	Skirt->CreateMeshSection_LinearColor(1, V, {0, 2, 1, 1, 2, 3}, N, UV, C, TArray<FProcMeshTangent>(), false);
	Skirt->SetMaterial(1, FloorMaterial());
}

const TArray<AKeysSeafloor::FRing>& AKeysSeafloor::Rings() const
{
	// Surfaced: only the near rings get real geometry, the far skirt fakes the rest.
	static const TArray<FRing> Surface = {{90, 18}, {220, 6}};
	// Submerged: 10-30 m visibility, so a small, finer set (2 m spacing up close, so coral heads have shape).
	static const TArray<FRing> Sub = {{26, 32}, {60, 10}};
	return bProfileSub ? Sub : Surface;
}

bool AKeysSeafloor::IsReady(double X, double Z) const
{
	return Resident.Contains(Key(FMath::FloorToInt32(X / ChunkSize), FMath::FloorToInt32(Z / ChunkSize)));
}

void AKeysSeafloor::Scan(double CamX, double CamZ)
{
	const TArray<FRing>& R = Rings();
	const double MaxOuter = R.Last().Outer;
	const int32 MinCX = FMath::FloorToInt32(WorldX0 / ChunkSize) - WorldChunkMargin, MaxCX = FMath::CeilToInt32((WorldX0 + WorldSizeX) / ChunkSize) + WorldChunkMargin;
	const int32 MinCZ = FMath::FloorToInt32(WorldZ0 / ChunkSize) - WorldChunkMargin, MaxCZ = FMath::CeilToInt32((WorldZ0 + WorldSizeZ) / ChunkSize) + WorldChunkMargin;
	const int32 C0X = FMath::Max(MinCX, FMath::FloorToInt32((CamX - MaxOuter) / ChunkSize)), C1X = FMath::Min(MaxCX, FMath::CeilToInt32((CamX + MaxOuter) / ChunkSize));
	const int32 C0Z = FMath::Max(MinCZ, FMath::FloorToInt32((CamZ - MaxOuter) / ChunkSize)), C1Z = FMath::Min(MaxCZ, FMath::CeilToInt32((CamZ + MaxOuter) / ChunkSize));
	const int32 Prof = bProfileSub ? 100 : 0;
	TSet<int64> Wanted;
	TArray<FJob> Jobs;
	for (int32 CZ = C0Z; CZ <= C1Z; CZ++)
		for (int32 CX = C0X; CX <= C1X; CX++)
		{
			const double Dist = FVector2D((CX + .5) * ChunkSize - CamX, (CZ + .5) * ChunkSize - CamZ).Size();
			int32 Lod = -1;
			for (int32 i = 0; i < R.Num(); i++) if (Dist <= R[i].Outer) { Lod = i; break; }
			if (Lod < 0) continue;
			const int64 K = Key(CX, CZ);
			Wanted.Add(K);
			const FChunk* Have = Resident.Find(K);
			if ((!Have || Have->Lod != Prof + Lod) && !Queued.Contains(K)) Jobs.Add({CX, CZ, Prof + Lod});
		}
	// nearest first, so the ground under the camera comes in before the distance
	Jobs.Sort([&](const FJob& A, const FJob& B)
	{
		return FVector2D((A.CX + .5) * ChunkSize - CamX, (A.CZ + .5) * ChunkSize - CamZ).SizeSquared() < FVector2D((B.CX + .5) * ChunkSize - CamX, (B.CZ + .5) * ChunkSize - CamZ).SizeSquared();
	});
	for (const FJob& J : Jobs) { Queue.Add(J); Queued.Add(Key(J.CX, J.CZ)); }
	// free anything well outside every ring (hysteresis so a boundary chunk doesn't build and free each scan)
	const double FreeMargin = bProfileSub ? 1.4 : 1.3;
	for (auto It = Resident.CreateIterator(); It; ++It)
	{
		if (Wanted.Contains(It.Key())) continue;
		const int32 CX = int32(It.Key() >> 32), CZ = int32(uint32(It.Key() & 0xffffffff));
		const double Dist = FVector2D((CX + .5) * ChunkSize - CamX, (CZ + .5) * ChunkSize - CamZ).Size();
		if (Dist > MaxOuter * FreeMargin)
		{
			Chunks->ClearMeshSection(It.Value().Section);
			FreeSections.Add(It.Value().Section);
			It.RemoveCurrent();
			bSkirtDirty = true;   // the skirt fills the cell back in
		}
	}
}

void AKeysSeafloor::ProcessQueue()
{
	const TArray<FRing>& R = Rings();
	int32 N = 0;
	while (N < BuildBudget && Queue.Num())
	{
		const FJob J = Queue[0];
		Queue.RemoveAt(0);
		const int64 K = Key(J.CX, J.CZ);
		Queued.Remove(K);
		const bool bJobSub = J.Lod >= 100;
		if (bJobSub != bProfileSub) continue;   // stale job from the other profile
		const int32 Segs = R[FMath::Min(J.Lod % 100, R.Num() - 1)].Segs;
		FChunk* Have = Resident.Find(K);
		int32 Section;
		if (Have) Section = Have->Section;
		else Section = FreeSections.Num() ? FreeSections.Pop() : NextSection++;
		BuildChunk(J.CX, J.CZ, Segs, Section);
		if (!Have) bSkirtDirty = true;   // the skirt makes way for it
		Resident.Add(K, {Section, J.Lod});
		N++;
	}
}

void AKeysSeafloor::BuildChunk(int32 CX, int32 CZ, int32 Segs, int32 Section)
{
	const double X0 = CX * ChunkSize, Z0 = CZ * ChunkSize;
	const int32 Nn = FMath::Max(1, Segs), Rows = Nn + 1;
	const double Step = ChunkSize / Nn;
	TArray<double> Hg, Rel;
	Hg.SetNumUninitialized(Rows * Rows);
	Rel.SetNumUninitialized(Rows * Rows);
	ParallelFor(Rows, [&](int32 j)
	{
		for (int32 i = 0; i < Rows; i++)
		{
			double H, Rl;
			KeysSea::SeafloorSample(X0 + i * Step, Z0 + j * Step, H, Rl);
			// stay under the island mesh where the beach runs into the water
			const double Lh = LandH(X0 + i * Step, Z0 + j * Step);
			if (Lh > -.3 && Lh - .1 < H) H = Lh - .1;
			Hg[j * Rows + i] = H;
			Rel[j * Rows + i] = Rl;
		}
	});

	TArray<FVector> V, Nrm;
	TArray<FVector2D> UV;
	TArray<FLinearColor> Col;
	TArray<int32> T;
	const int32 PerimN = 4 * Nn;
	V.Reserve(Rows * Rows + PerimN * 2);
	auto NormalAt = [&](int32 i, int32 j)
	{
		const int32 iL = FMath::Max(0, i - 1), iR = FMath::Min(Nn, i + 1), jD = FMath::Max(0, j - 1), jU = FMath::Min(Nn, j + 1);
		const double DX = (Hg[j * Rows + iR] - Hg[j * Rows + iL]) / ((iR - iL) * Step), DZ = (Hg[jU * Rows + i] - Hg[jD * Rows + i]) / ((jU - jD) * Step);
		return FVector(-DX, -DZ, 1).GetSafeNormal();
	};
	auto ColorAt = [&](int32 i, int32 j)
	{
		const int32 iL = FMath::Max(0, i - 1), iR = FMath::Min(Nn, i + 1), jD = FMath::Max(0, j - 1), jU = FMath::Min(Nn, j + 1);
		const double Slope = (FMath::Abs(Hg[j * Rows + iR] - Hg[j * Rows + iL]) / ((iR - iL) * Step) + FMath::Abs(Hg[jU * Rows + i] - Hg[jD * Rows + i]) / ((jU - jD) * Step)) * .5;
		const double Shade = 1 - FMath::Min(.35, Slope * 1.4);
		return KeysSea::SeafloorColorAt(X0 + i * Step, Z0 + j * Step, -Hg[j * Rows + i], Shade, Rel[j * Rows + i]);
	};
	for (int32 j = 0; j < Rows; j++)
		for (int32 i = 0; i < Rows; i++)
		{
			const double X = X0 + i * Step, Z = Z0 + j * Step;
			V.Add(ToUE(X, Hg[j * Rows + i], Z));
			Nrm.Add(NormalAt(i, j));
			UV.Add(FVector2D(X, Z));
			Col.Add(DebugColors() ? (Segs > 10 ? FLinearColor::Red : FLinearColor::Blue) : ColorAt(i, j));
		}
	for (int32 j = 0; j < Nn; j++)
		for (int32 i = 0; i < Nn; i++)
		{
			const int32 A = j * Rows + i, B = A + 1, C = A + Rows, D = C + 1;
			T.Append({A, C, B, B, C, D});
		}
	// skirt: walk the perimeter and drop a wall under it
	TArray<FIntPoint> Per;
	for (int32 i = 0; i < Nn; i++) Per.Add(FIntPoint(i, 0));
	for (int32 j = 0; j < Nn; j++) Per.Add(FIntPoint(Nn, j));
	for (int32 i = Nn; i > 0; i--) Per.Add(FIntPoint(i, Nn));
	for (int32 j = Nn; j > 0; j--) Per.Add(FIntPoint(0, j));
	const int32 S0 = V.Num();
	for (const FIntPoint& P : Per)
	{
		const double X = X0 + P.X * Step, Z = Z0 + P.Y * Step, Y = Hg[P.Y * Rows + P.X];
		const FLinearColor C = ColorAt(P.X, P.Y);
		// the wall faces outward from the chunk
		const FVector Out = FVector(X - (X0 + ChunkSize / 2), Z - (Z0 + ChunkSize / 2), 0).GetSafeNormal();
		V.Add(ToUE(X, Y, Z)); Nrm.Add(Out); UV.Add(FVector2D(X, Z)); Col.Add(C);
		V.Add(ToUE(X, Y - SkirtDepth, Z)); Nrm.Add(Out); UV.Add(FVector2D(X, Z)); Col.Add(C);
	}
	for (int32 k = 0; k < Per.Num(); k++)
	{
		const int32 k2 = (k + 1) % Per.Num();
		const int32 ATop = S0 + k * 2, ABot = ATop + 1, BTop = S0 + k2 * 2, BBot = BTop + 1;
		T.Append({ATop, ABot, BTop, BTop, ABot, BBot});
	}
	Chunks->CreateMeshSection_LinearColor(Section, V, T, Nrm, UV, Col, TArray<FProcMeshTangent>(), false);
	Chunks->SetMaterial(Section, FloorMaterial());
}

void AKeysSeafloor::BuildSkirt(double CamX, double CamZ)
{
	SkirtCX = FMath::FloorToInt32(CamX / ChunkSize);
	SkirtCZ = FMath::FloorToInt32(CamZ / ChunkSize);
	// grid lines along one axis: the chunk lattice round the camera's cell, then rows that widen toward the horizon
	auto Lines = [](int32 Cc, TArray<double>& Out)
	{
		const int32 Lo = Cc - SkirtInner, Hi = Cc + SkirtInner + 1, NIn = (Hi - Lo) * SkirtSub;
		Out.SetNum(2 * SkirtOuter + NIn + 1);
		double P = Lo * ChunkSize, St = ChunkSize;
		for (int32 k = 0; k < SkirtOuter; k++) { St *= SkirtGrow; P -= St; Out[SkirtOuter - 1 - k] = P; }
		for (int32 L = 0; L <= NIn; L++) Out[SkirtOuter + L] = Lo * ChunkSize + L * ChunkSize / SkirtSub;
		P = Hi * ChunkSize; St = ChunkSize;
		for (int32 k = 0; k < SkirtOuter; k++) { St *= SkirtGrow; P += St; Out[SkirtOuter + NIn + 1 + k] = P; }
	};
	Lines(SkirtCX, SkirtX);
	Lines(SkirtCZ, SkirtZ);
	const int32 Rows = SkirtX.Num();
	SkirtV.SetNum(Rows * Rows); SkirtN.SetNum(Rows * Rows); SkirtUV.SetNum(Rows * Rows); SkirtCol.SetNum(Rows * Rows);
	ParallelFor(Rows, [&](int32 j)
	{
		for (int32 i = 0; i < Rows; i++)
		{
			const double X = SkirtX[i], Z = SkirtZ[j];
			const double Y0 = KeysSea::SeafloorHeightAt(X, Z);
			// under the island mesh where the beach runs into the water, as the chunks are
			double Y = Y0;
			const double Lh = LandH(X, Z);
			if (Lh > -.3 && Lh - .3 < Y) Y = Lh - .3;
			SkirtV[j * Rows + i] = ToUE(X, Y, Z);
			SkirtUV[j * Rows + i] = FVector2D(X, Z);
			// (no per-vertex grain out here: on vertices this far apart it would show as blotches a chunk wide)
			SkirtCol[j * Rows + i] = DebugColors() ? FLinearColor::Green : KeysSea::SeafloorColorAt(X, Z, -Y0, 1, 0, false);
		}
	});
	const int32 Nn = Rows - 1;
	for (int32 j = 0; j < Rows; j++)
		for (int32 i = 0; i < Rows; i++)
		{
			const int32 iL = FMath::Max(0, i - 1), iR = FMath::Min(Nn, i + 1), jD = FMath::Max(0, j - 1), jU = FMath::Min(Nn, j + 1);
			const FVector DX = SkirtV[j * Rows + iR] - SkirtV[j * Rows + iL], DZ = SkirtV[jU * Rows + i] - SkirtV[jD * Rows + i];
			FVector Nm = FVector::CrossProduct(DX, DZ).GetSafeNormal();
			if (Nm.Z < 0) Nm = -Nm;
			SkirtN[j * Rows + i] = Nm;
			// steep faces a little darker, as on the chunks
			if (!DebugColors())
			{
				const double Slope = (FMath::Abs(DX.Z / FMath::Max(1.0, FMath::Abs(DX.X))) + FMath::Abs(DZ.Z / FMath::Max(1.0, FMath::Abs(DZ.Y)))) * .5;
				const float Shade = float(1 - FMath::Min(.35, Slope * 1.4));
				FLinearColor& C = SkirtCol[j * Rows + i];
				C.R *= Shade; C.G *= Shade; C.B *= Shade;
			}
		}
	BuildSkirtTris();
}

void AKeysSeafloor::BuildSkirtTris()
{
	bSkirtDirty = false;
	const int32 Rows = SkirtX.Num(), Nn = Rows - 1;
	if (Nn < 1) return;
	// a lattice cell whose chunk is built is left open: the chunk is the floor there
	const int32 NIn = (2 * SkirtInner + 1) * SkirtSub;
	auto Open = [&](int32 i, int32 j)
	{
		const int32 LI = i - SkirtOuter, LJ = j - SkirtOuter;
		return LI >= 0 && LI < NIn && LJ >= 0 && LJ < NIn && Resident.Contains(Key(SkirtCX - SkirtInner + LI / SkirtSub, SkirtCZ - SkirtInner + LJ / SkirtSub));
	};
	TArray<int32> T;
	T.Reserve(Nn * Nn * 6);
	TArray<FVector> V = SkirtV, Nrm = SkirtN;
	TArray<FVector2D> UV = SkirtUV;
	TArray<FLinearColor> Col = SkirtCol;
	// round the hole the skirt drops a wall, as the chunks do: where its straight edge stands a little above the
	// chunk's, a camera over the chunks would otherwise see in under it (a dark hairline the ripples smear out)
	auto Wall = [&](int32 A, int32 B)
	{
		const int32 K = V.Num();
		for (const int32 Src : {A, B})
		{
			V.Add(SkirtV[Src] - FVector(0, 0, SkirtDepth * 100)); Nrm.Add(SkirtN[Src]); UV.Add(SkirtUV[Src]); Col.Add(SkirtCol[Src]);
		}
		T.Append({A, B, K, B, K + 1, K, A, K, B, B, K, K + 1});   // both faces
	};
	for (int32 j = 0; j < Nn; j++)
		for (int32 i = 0; i < Nn; i++)
		{
			if (Open(i, j)) continue;
			const int32 A = j * Rows + i, B = A + 1, C = A + Rows, D = C + 1;
			T.Append({A, C, B, B, C, D});
			if (j > 0 && Open(i, j - 1)) Wall(A, B);
			if (j < Nn - 1 && Open(i, j + 1)) Wall(C, D);
			if (i > 0 && Open(i - 1, j)) Wall(A, C);
			if (i < Nn - 1 && Open(i + 1, j)) Wall(B, D);
		}
	Skirt->CreateMeshSection_LinearColor(0, V, T, Nrm, UV, Col, TArray<FProcMeshTangent>(), false);
	Skirt->SetMaterial(0, FloorMaterial());
	Skirt->SetMeshSectionVisible(0, !bSubmerged && !NoSkirt());
}

void AKeysSeafloor::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	APlayerCameraManager* Cam = UGameplayStatics::GetPlayerCameraManager(this, 0);
	if (!Cam) return;
	const FVector L = Cam->GetCameraLocation();
	const double CamX = L.X / 100, CamZ = L.Y / 100, CamY = L.Z / 100;
	const double Sea = KeysOcean::SeaLevel();
	if (CamY < Sea + SubmergeY) bSubmerged = true;
	else if (CamY > Sea + SurfaceY) bSubmerged = false;
	if (bSubmerged != bProfileSub)
	{
		bProfileSub = bSubmerged;
		// from the boat the near chunks must light exactly like the far skirt (no shadows, no bounce of their own), or
		// they show as a brighter square on the flats; a diver gets the reef's own shadows back
		Chunks->SetCastShadow(bSubmerged);
		Chunks->bAffectDynamicIndirectLighting = bSubmerged;
		Chunks->MarkRenderStateDirty();
		Queue.Reset(); Queued.Reset();
		LastScanX = 1e9;
	}
	if (FVector2D(CamX - LastScanX, CamZ - LastScanZ).Size() > ScanDist)
	{
		LastScanX = CamX; LastScanZ = CamZ;
		if (!NoChunks()) Scan(CamX, CamZ);
	}
	ProcessQueue();
	static double LogT = 0, FrameSum = 0;
	static int32 Frames = 0;
	LogT += DeltaSeconds; FrameSum += DeltaSeconds; Frames++;
	if (LogT > 3)
	{
		UE_LOG(LogTemp, Log, TEXT("KRSeafloor cam %.0f,%.0f,%.1f resident %d queue %d sub %d frame %.1f ms"), CamX, CamZ, CamY, Resident.Num(), Queue.Num(), bProfileSub ? 1 : 0, Frames ? FrameSum / Frames * 1000 : 0.0);
		LogT = 0; FrameSum = 0; Frames = 0;
	}
	Skirt->SetMeshSectionVisible(0, !bSubmerged && !NoSkirt());
	if (!bSubmerged)
	{
		// re-centre once the camera is two cells off (the lattice part must cover every resident chunk)
		const int32 CCX = FMath::FloorToInt32(CamX / ChunkSize), CCZ = FMath::FloorToInt32(CamZ / ChunkSize);
		if (!SkirtX.Num() || FMath::Abs(CCX - SkirtCX) >= 2 || FMath::Abs(CCZ - SkirtCZ) >= 2) BuildSkirt(CamX, CamZ);
		else if (bSkirtDirty) BuildSkirtTris();
	}
}
