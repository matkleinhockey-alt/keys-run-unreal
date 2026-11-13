#include "KeysSchools.h"
#include "KeysGeo.h"
#include "KeysSea.h"
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
	// ---- the leader track texture (M_KR_Fish reads it)
	constexpr int32 SchRows = 512;       // leaders alive at once
	constexpr int32 SchHist = 48;        // samples of history per leader
	constexpr double SchStep = .05;      // seconds between samples
	constexpr double SchMaxDelay = (SchHist - 4) * SchStep;
	constexpr double PhaseWrap = 512 * 2 * PI;

	// ---- where schools live
	constexpr double CellSize = 26;
	constexpr double SpawnR = 140, KeepR = 190;
	constexpr int32 MaxSchools = 56, MaxFish = 3200;
	constexpr double BoundsExt = 2600;   // cm: how far the material may carry a fish from its instance's anchor
	constexpr double RebaseDist = 11;    // m: re-anchor a school's instances once its leaders stray this far

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
		{TEXT("silverside"), ERole::Bait,   .09, .12,  320, 520,  6, 8,  .45, 2.6, 3.0, 2.3, 4.5,  2.2, 45, 1.2, 3.5,  22, false},
		{TEXT("herring"),    ERole::Bait,   .17, .22,  140, 220,  4, 6,  .6,  3.0, 3.0, 2.0, 5,    3,   45, 1.5, 4,    28, false},
		{TEXT("schoolmaster"), ERole::Reef, .32, .45,  14, 30,    2, 3,  .22, 2.0, 1.6, 1.7, 3.5,  2,   30, .5, 2.0,   5, false},
		{TEXT("grunt"),      ERole::Reef,   .22, .3,   16, 36,    2, 4,  .2,  2.0, 1.6, 1.6, 3.5,  1.6, 30, .4, 1.6,   5, false},
		{TEXT("lanesnapper"), ERole::Reef,  .26, .34,  10, 24,    2, 3,  .25, 2.0, 1.6, 1.7, 3.5,  2,   35, .5, 2.0,   6, false},
		{TEXT("yellowtail"), ERole::Reef,   .34, .42,  8, 18,     1, 2,  .5,  2.6, 2.0, 2.0, 4,    3,   40, 1.4, 4,    10, false},
		{TEXT("grouper"),    ERole::Hole,   .8, 1.1,   1, 1,      0, 0,  .08, 1.6, 1.0, 1,   2.5,  3,   40, .25, .6,   3, false},
		{TEXT("redgrouper"), ERole::Hole,   .55, .75,  1, 1,      0, 0,  .08, 1.6, 1.0, 1,   2.5,  3,   40, .2, .5,    3, false},
		{TEXT("barracuda"),  ERole::Hanger, .9, 1.3,   1, 2,      0, 0,  .05, 5.5, 4.0, 2.5, 2.5,  .9,  40, .8, 3.5,  12, true},
		{TEXT("bonefish"),   ERole::Flats,  .45, .62,  4, 9,      0, 1,  .7,  4.5, 3.0, 2.2, 7,    .4,  2.8, .12, .3,  70, false},
		{TEXT("permit"),     ERole::Flats,  .6, .8,    2, 5,      0, 1,  .6,  4.0, 2.5, 2.4, 6,    .5,  3.5, .15, .4,  70, false},
		{TEXT("bluerunner"), ERole::Hunter, .4, .5,    5, 9,      0, 1,  1.1, 4.5, 3.0, 2.0, 3,    2,   45, 1.0, 4.5,  60, true},
		{TEXT("jackcrevalle"), ERole::Hunter, .6, .85, 4, 7,      0, 1,  1.0, 4.5, 3.0, 2.0, 3,    1,   45, .8, 4.0,   60, true},
		{TEXT("blacktip"),   ERole::Patrol, 1.5, 1.9,  1, 1,      0, 0,  .9,  3.2, 1.4, 1,   1.5,  .8,  30, .4, 2.5,   90, true},
		{TEXT("bullshark"),  ERole::Patrol, 2.0, 2.4,  1, 1,      0, 0,  .8,  3.0, 1.2, 1,   1.5,  4,   45, .6, 4,     90, true},
	};
	int32 SpecIndex(const FString& K) { for (int32 i = 0; i < NumSpec; i++) if (K == Specs[i].Key) return i; return -1; }

	struct FZoneMix { const TCHAR* Zone; double Density; TArray<TPair<int32, double>> Rows; };
	const TArray<FZoneMix>& ZoneMixes()
	{
		static const TArray<FZoneMix> M = {
			{TEXT("Flats"), .4, {{Bonefish, 4}, {Permit, 2}, {Silverside, 2}, {Barracuda, 1.5}, {Blacktip, .6}, {Jack, .8}}},
			{TEXT("Backcountry"), .3, {{Silverside, 3}, {Barracuda, 1}, {Jack, 1}, {Grunt, 1}, {Bonefish, 1}}},
			{TEXT("Creek"), .3, {{Silverside, 3}, {Barracuda, 1}, {Jack, 1}, {Grunt, 1}}},
			{TEXT("Bridge"), .5, {{Herring, 3}, {Grunt, 2}, {Schoolmaster, 2}, {Barracuda, 1}, {Jack, 1}}},
			{TEXT("Hawk Channel"), .45, {{Herring, 2}, {Silverside, 2}, {Grunt, 2}, {Schoolmaster, 1}, {Lane, 1.5}, {RedGrouper, .8}, {Barracuda, 1}, {Bluerunner, 1.2}, {Bullshark, .2}}},
			{TEXT("Reef"), .7, {{Silverside, 3}, {Herring, 2}, {Schoolmaster, 3}, {Grunt, 3}, {Lane, 2}, {Yellowtail, 2}, {Grouper, 1.2}, {RedGrouper, .8}, {Barracuda, 1.5}, {Bluerunner, 1.5}, {Jack, .8}, {Blacktip, .3}, {Bullshark, .25}}},
		};
		return M;
	}

	// the reef's species, in KeysReef's order: how tall and how wide each grows at scale 1 (m)
	const double CoralH[] = {1.0, 1.1, .56, .72, 1.7, 1.3, 1.0, 1.1, .24, 0};
	const double CoralR[] = {.85, .8, .9, 1.0, .55, .45, .65, .5, .65, 0};

	FVector G2U(const FVector& P) { return FVector(P.X * 100.0, P.Z * 100.0, P.Y * 100.0); }
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
	SetupTexture();
	for (int32 r = SchRows - 1; r >= 0; r--) FreeRows.Add(r);
}

void AKeysSchools::SetupTexture()
{
	TArray64<uint8> Zero;
	Zero.SetNumZeroed(SchHist * 4 * SchRows * sizeof(FLinearColor));
	Tex = UTexture2D::CreateTransient(SchHist * 4, SchRows, PF_A32B32G32R32F, TEXT("KeysFishTrack"), Zero);
	if (Tex)
	{
		Tex->Filter = TF_Nearest;
		Tex->SRGB = false;
		Tex->AddressX = TA_Wrap;
		Tex->AddressY = TA_Clamp;
		Tex->NeverStream = true;
		Tex->UpdateResource();
	}
	Column.SetNumZeroed(SchRows * 4);
	UMaterialInterface* M = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/KeysRun/Materials/M_KR_Fish.M_KR_Fish"));
	if (!M) { UE_LOG(LogTemp, Warning, TEXT("KRSchools: M_KR_Fish missing (open the editor once to create it); no school fish")); return; }
	MID = UMaterialInstanceDynamic::Create(M, this);
	MID->SetTextureParameterValue(TEXT("LeaderTex"), Tex);
	MID->SetScalarParameterValue(TEXT("Hist"), SchHist);
	MID->SetScalarParameterValue(TEXT("Rows"), SchRows);
	MID->SetScalarParameterValue(TEXT("StepDt"), SchStep);
}

int32 AKeysSchools::AllocRow()
{
	if (!FreeRows.Num()) return -1;
	const int32 R = FreeRows.Pop();
	FreshRows.Add(R);
	return R;
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
	C->NumCustomDataFloats = 8;
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
	for (int32 P = 0; P < Pools.Num(); P++)
	{
		if (!PoolDirty[P]) continue;
		PoolDirty[P] = false;
		Rebuilds++;
		UInstancedStaticMeshComponent* C = Pools[P];
		if (!C) continue;
		C->ClearInstances();
		TArray<FTransform> Xf;
		TArray<FSchool*> Mine;
		for (FSchool& S : Schools)
			if (S.Pool == P) { S.First = Xf.Num(); for (int32 i = 0; i < S.NFish; i++) Xf.Add(FTransform(S.AnchorUE)); Mine.Add(&S); }
		if (!Xf.Num()) { C->MarkRenderStateDirty(); continue; }
		C->AddInstances(Xf, false, false);
		for (FSchool* S : Mine)
			for (int32 i = 0; i < S->NFish; i++) C->SetCustomData(S->First + i, TArrayView<const float>(S->Data.GetData() + i * 8, 8), false);
		C->MarkRenderStateDirty();
	}
}

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
		for (const FLeader& Ld : S.L)
		{
			FThreat T;
			T.Kind = 2;
			T.SchoolId = S.Id;
			T.P = Ld.P;
			T.V = Ld.V;
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

// ================================================================ spawning
AKeysSchools::FSchool* AKeysSchools::Spawn(int32 Sp, const FVector& Home, double HomeTop, int64 Cell, uint32 Seed, double NScale)
{
	const FSpec& Sc = Specs[Sp];
	FSchool S;
	S.Rng.Initialize(int32(Seed));
	S.Id = NextId++;
	S.Sp = Sp;
	S.Cell = Cell;
	S.Len = S.Rng.FRandRange(Sc.LenMin, Sc.LenMax);
	S.Home = Home;
	S.HomeTop = HomeTop;
	int32 N = FMath::Max(1, FMath::RoundToInt32(S.Rng.FRandRange(Sc.NMin, Sc.NMax + .99) * NScale));
	int32 NFishAll = 0;
	for (const FSchool& O : Schools) NFishAll += O.NFish;
	if (!Cell)
	{
		// a test school: make room by letting the furthest ones go
		while (Schools.Num() && (NFishAll + N > MaxFish || FreeRows.Num() < 10))
		{
			int32 Far = -1;
			double FD = -1;
			for (int32 i = 0; i < Schools.Num(); i++)
			{
				const double D = (Schools[i].L[0].P - Home).Size();
				if (!Schools[i].bPinned && D > FD) { FD = D; Far = i; }
			}
			if (Far < 0) break;
			NFishAll -= Schools[Far].NFish;
			const int64 FarCell = Schools[Far].Cell;
			Despawn(Far);
			if (FarCell) Cells.Add(FarCell, -1);
		}
	}
	if (NFishAll + N > MaxFish) N = MaxFish - NFishAll;
	if (N < 1) return nullptr;
	const int32 NSub = N < 6 ? FMath::Min(N - 1, Sc.SubMax) : FMath::Clamp(S.Rng.RandRange(Sc.SubMin, Sc.SubMax), 0, N - 1);
	const int32 NL = 1 + FMath::Max(0, NSub);
	if (FreeRows.Num() < NL) return nullptr;
	// group sizes and radii
	const int32 PerGroup = FMath::Max(1, N / NL);
	const double Space = Sc.Spacing * S.Len;
	const double GroupR = Space * FMath::Pow(double(PerGroup), 1.0 / 3.0) * .62;
	S.Radius = GroupR * (NL > 1 ? 2.2 : 1);
	const bool bBall = Sc.Role == ERole::Bait;
	// leaders
	FVector Start = Home;
	S.L.SetNum(NL);
	for (int32 k = 0; k < NL; k++)
	{
		FLeader& Ld = S.L[k];
		Ld.Row = AllocRow();
		const double Yaw = S.Rng.FRandRange(0, 2 * PI);
		Ld.Fwd = FVector(FMath::Cos(Yaw), 0, FMath::Sin(Yaw));
		Ld.Phase = S.Rng.FRandRange(0, 2 * PI);
		Ld.Ext = GroupR;
		if (k > 0)
		{
			// sub-leaders sit around and behind the main leader
			const double A = 2 * PI * (k - 1) / FMath::Max(1, NL - 1) + S.Rng.FRandRange(-.3, .3);
			const double Rr = GroupR * (bBall ? 1.25 : 1.6);
			Ld.Slot = bBall ? FVector(FMath::Cos(A) * Rr * .8, FMath::Sin(A) * Rr, FMath::Sin(A * 2 + 1) * Rr * .6)
				: FVector(-Rr * S.Rng.FRandRange(.6, 1.3), FMath::Cos(A) * Rr * 1.2, FMath::Sin(A) * Rr * .45);
			Ld.Delay = FMath::Min(SchMaxDelay * .5, (bBall ? .15 : .35) + .12 * k);
		}
		FVector Rt, Up;
		Frame(S.L[0].Fwd, Rt, Up);
		Ld.P = Start + S.L[0].Fwd * Ld.Slot.X + Rt * Ld.Slot.Y + Up * Ld.Slot.Z;
	}
	S.Goal = Home;
	// the fish: each follows one leader from a slot around it, a little later the further back it swims
	S.NFish = N;
	S.Data.SetNumZeroed(N * 8);
	int32 Fi = 0;
	for (int32 k = 0; k < NL; k++)
	{
		const int32 Ng = k == NL - 1 ? N - Fi : PerGroup;
		for (int32 j = 0; j < Ng; j++, Fi++)
		{
			FVector Sl = FVector::ZeroVector;
			double Dl = 0;
			if (j > 0)
			{
				// a point in a ball (bait) or a fish-shaped blob trailing back (everything else), spaced by body lengths
				FVector U;
				do U = FVector(S.Rng.FRandRange(-1, 1), S.Rng.FRandRange(-1, 1), S.Rng.FRandRange(-1, 1)); while (U.SizeSquared() > 1);
				Sl = bBall ? U * GroupR : FVector((U.X - .45) * GroupR * 1.5, U.Y * GroupR, U.Z * GroupR * .5);
				Dl = .08 + FMath::Max(0.0, -Sl.X) / FMath::Max(.35, Sc.Cruise) * .3 + S.Rng.FRandRange(0, .2);
			}
			float* D = S.Data.GetData() + Fi * 8;
			D[0] = float(S.L[k].Row);
			D[1] = float(FMath::Min(SchMaxDelay - S.L[k].Delay, Dl));
			// slot in the leader's frame, Unreal's axes (x ahead, y right, z up) in cm
			D[2] = float(Sl.X * 100);
			D[3] = float(Sl.Y * 100);
			D[4] = float(Sl.Z * 100);
			D[5] = float(S.Rng.FRand());
			D[6] = float(S.Len * S.Rng.FRandRange(.85, 1.15));
			D[7] = float(S.Rng.FRandRange(.82, 1.08));
		}
	}
	S.AnchorUE = G2U(Home);
	S.TrackP.Init(Home, SchHist);
	S.TrackF.Init(S.L[0].Fwd, SchHist);
	Schools.Add(MoveTemp(S));
	return &Schools.Last();
}

void AKeysSchools::Despawn(int32 Index)
{
	FSchool& S = Schools[Index];
	for (const FLeader& Ld : S.L) if (Ld.Row >= 0) { FreeRows.Add(Ld.Row); FreshRows.Remove(Ld.Row); }
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
		const double D = (S.L[0].P - P).Size();
		if (D < BD) { BD = D; Best = &S; }
	}
	return Best;
}

void AKeysSchools::Populate()
{
	// let go of schools far from the camera
	for (int32 i = Schools.Num() - 1; i >= 0; i--)
		if (!Schools[i].bPinned && Hyp(Schools[i].L[0].P.X - Cam.X, Schools[i].L[0].P.Z - Cam.Z) > KeepR) Despawn(i);
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
				if (!Mix || KeysSea::HashCell(0x5c4f, CX, CZ, 11) >= Mix->Density) continue;
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
				if (Role == ERole::Bait) Y = FMath::Lerp(Top + Sc.AltMin, -1.0, .45);
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

// ================================================================ steering
FVector AKeysSchools::TrackAt(const FSchool& S, double Delay, FVector& OutFwd) const
{
	// TrackP[(TrackN - 1) % H] is the newest sample; interpolate Delay seconds back
	const double Back = FMath::Clamp(Delay / SchStep, 0.0, double(SchHist - 2));
	const int32 B0 = FMath::FloorToInt32(Back);
	const double Fr = Back - B0;
	const int32 I0 = int32(((S.TrackN - 1 - B0) % SchHist + SchHist) % SchHist), I1 = int32(((S.TrackN - 2 - B0) % SchHist + SchHist) % SchHist);
	OutFwd = FMath::Lerp(S.TrackF[I0], S.TrackF[I1], Fr).GetSafeNormal();
	if (OutFwd.IsNearlyZero()) OutFwd = S.L[0].Fwd;
	return FMath::Lerp(S.TrackP[I0], S.TrackP[I1], Fr);
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
		if (Sc.Role == ERole::Bait || Sc.Role == ERole::Hunter || Sc.Role == ERole::Hanger) Y = FMath::Lerp(Top + Sc.AltMin + S.L[0].Ext, -.8 - S.L[0].Ext * .5, S.Rng.FRandRange(.25, .7));
		else Y = Top + S.Rng.FRandRange(Sc.AltMin, Sc.AltMax);
		return FVector(X, FMath::Min(Y, -.3), Z);
	}
	return S.Home;
}

void AKeysSchools::MainGoal(FSchool& S, double Dt)
{
	const FSpec& Sc = Specs[S.Sp];
	S.GoalT -= Dt;
	FLeader& M = S.L[0];
	switch (Sc.Role)
	{
	case ERole::Reef:
	case ERole::Hole:
		// hover over the head: a slow lazy circle, a new spot now and then
		if (S.GoalT <= 0) { S.GoalT = S.Rng.FRandRange(5, 11); S.Goal = PickGoal(S); }
		break;
	case ERole::Hanger:
	{
		// barracuda hang still, and come to have a look at a diver, keeping a few meters off
		if (S.GoalT <= 0) { S.GoalT = S.Rng.FRandRange(10, 20); S.Goal = PickGoal(S); }
		for (const FThreat& T : Threats)
			if (T.Kind == 0 && (T.P - M.P).Size() < 14)
			{
				const FVector Off = (M.P - T.P).GetSafeNormal() * 4.5;
				S.Goal = T.P + FVector(Off.X, FMath::Clamp(Off.Y, -1.0, 1.0), Off.Z);
			}
		break;
	}
	default:
		if (S.GoalT <= 0 || Flat(S.Goal - M.P).Size() < 2) { S.GoalT = S.Rng.FRandRange(8, 16); S.Goal = PickGoal(S); }
		break;
	}
	// predators go hunting: pick a school within reach, charge through it and out the far side
	if (Sc.bPredator)
	{
		S.HuntT -= Dt;
		if (S.HuntLeft > 0)
		{
			S.HuntLeft -= Dt;
			FSchool* Prey = Find(S.PreyId);
			if (Prey && S.HuntLeft > 1.2) S.HuntDir = (Prey->L[0].P - M.P).GetSafeNormal();
			S.Goal = M.P + S.HuntDir * 12;
			if (S.HuntLeft <= 0) S.GoalT = 0;
		}
		else if (S.HuntT <= 0)
		{
			S.HuntT = S.Rng.FRandRange(Sc.Role == ERole::Hanger ? 18 : 10, Sc.Role == ERole::Patrol ? 40 : 26);
			if (FSchool* Prey = NearestOfRole(M.P, Sc.Role == ERole::Hanger, S.Id, Sc.Role == ERole::Patrol ? 60 : 40))
			{
				S.PreyId = Prey->Id;
				const double D = (Prey->L[0].P - M.P).Size();
				S.HuntLeft = D / Sc.Burst + 1.8;
				S.HuntDir = (Prey->L[0].P - M.P).GetSafeNormal();
			}
		}
	}
}

void AKeysSchools::SteerLeader(FSchool& S, int32 K, double Dt)
{
	const FSpec& Sc = Specs[S.Sp];
	FLeader& Ld = S.L[K];
	const bool bMain = K == 0;
	const bool bHunting = Sc.bPredator && S.HuntLeft > 0;
	// where it wants to be
	FVector Want;
	double Speed = Sc.Cruise;
	if (bMain || Ld.LooseT > 0)
	{
		if (bMain) Want = S.Goal;
		else { Ld.LooseT -= Dt; Want = Ld.P + Ld.LooseDir * 6; }
		if (bHunting) Speed = Sc.Burst;
	}
	else
	{
		FVector F;
		const FVector P0 = TrackAt(S, Ld.Delay, F);
		FVector Rt, Up;
		Frame(F, Rt, Up);
		const double Sp = FMath::Lerp(1.0, .45, S.Ball);
		Want = P0 + (F * Ld.Slot.X + Rt * Ld.Slot.Y + Up * Ld.Slot.Z) * Sp;
		Speed = FMath::Max(Sc.Cruise, S.L[0].V.Size());
	}
	const FVector To = Want - Ld.P;
	const double Dist = To.Size();
	const double Arrive = bMain ? FMath::Min(1.0, Dist / 2.5) : FMath::Min(1.6, Dist / 1.2);
	FVector Desired = Dist > 1e-3 ? To / Dist * Speed * Arrive : FVector::ZeroVector;
	if (!bMain && Ld.LooseT <= 0) Desired += S.L[0].V * .6;   // match the main leader's way
	// separation from the school's other leaders
	for (int32 j = 0; j < S.L.Num(); j++)
	{
		if (j == K) continue;
		const FVector D = Ld.P - S.L[j].P;
		const double Dd = D.Size(), Want2 = (Ld.Ext + S.L[j].Ext) * .8;
		if (Dd < Want2 && Dd > 1e-3) Desired += D / Dd * (Want2 - Dd) * 1.5;
	}
	// threats
	FVector Flee = FVector::ZeroVector;
	double Best = 0;
	for (const FThreat& T : Threats)
	{
		if (T.SchoolId == S.Id) continue;
		double R = T.R;
		if (T.Kind == 0) R = Sc.DiverR * T.R + Ld.Ext;
		else if (T.Kind == 2)
		{
			if (Sc.bPredator) continue;
			// reef fish and flats fish only mind a predator that's charging; bait minds them all
			if (Sc.Role != ERole::Bait && T.Power < .9) continue;
			R += Ld.Ext;
		}
		else R += Ld.Ext;
		if (Sc.Role == ERole::Hanger && T.Kind == 0) R *= .5;   // a barracuda just backs off a little
		FVector D = Ld.P - T.P;
		const double Dd = D.Size();
		if (Dd > R || Dd < 1e-3) continue;
		double K2 = (1 - Dd / R) * T.Power;
		FVector Dir = D / Dd;
		if (T.Kind == 2 && !T.V.IsNearlyZero())
		{
			// the fountain: dodge off the predator's line, not straight ahead of it
			const FVector Vn = T.V.GetSafeNormal();
			FVector Lat = D - Vn * FVector::DotProduct(D, Vn);
			if (Lat.SizeSquared() < 1e-4) Lat = FVector::CrossProduct(Vn, FVector(0, 1, 0));
			Dir = (Lat.GetSafeNormal() * 1.6 + Dir * .5).GetSafeNormal();
		}
		Dir.Y *= .4;
		Flee += Dir * K2;
		// the threat the followers dodge: strongest wins
		const double Felt = (1 - Dd / (R + 8)) * T.Power;
		if (Felt > Best) { Best = Felt; Ld.ThreatP = T.P; }
		if (Sc.Role == ERole::Bait && T.Kind == 2 && Dd < 22) S.Ball = FMath::Min(1.0, S.Ball + Dt * 2.5);
	}
	Ld.ThreatK = Best > Ld.ThreatK ? FMath::Min(1.0, Ld.ThreatK + Dt * 6) : FMath::Max(Best, Ld.ThreatK - Dt * .35);
	if (!Flee.IsNearlyZero())
	{
		const double Fk = FMath::Min(1.0, Flee.Size());
		Ld.Alarm = FMath::Max(Ld.Alarm, Fk);
		Desired = FMath::Lerp(Desired, Flee.GetSafeNormal() * Sc.Burst, FMath::Min(1.0, Fk * 1.8));
		if (Sc.Role == ERole::Hole) Desired.Y = FMath::Min(Desired.Y, -.3);   // a grouper backs down into its hole
	}
	Ld.Alarm = FMath::Max(0.0, Ld.Alarm - Dt * .3);
	// cut off from the main leader by a scare: school on its own for a while, then come back
	if (!bMain && Ld.LooseT <= 0 && Ld.Alarm > .5 && Dist > FMath::Max(4.0, S.Radius * 1.4)) { Ld.LooseT = S.Rng.FRandRange(4, 8); Ld.LooseDir = Flat(Ld.V).GetSafeNormal(); if (Ld.LooseDir.IsNearlyZero()) Ld.LooseDir = Ld.Fwd; }
	if (Ld.LooseT > 0 && (Ld.P - S.L[0].P).Size() > 18) Ld.LooseT = 0;
	// the bottom, the coral and wrecks below; the surface above; the habitat around
	const FVector Ahead = Ld.P + Ld.V * 1.2;
	const double Top = FMath::Max(TopAt(Ld.P.X, Ld.P.Z), TopAt(Ahead.X, Ahead.Z));
	const double Lo = Top + FMath::Max(.15, Sc.AltMin * .6) + Ld.Ext * (Sc.Role == ERole::Bait ? .7 : .35);
	const double Hi = -.35 - Ld.Ext * .45 - S.Len * .2;
	if (Ld.P.Y < Lo) Desired.Y = FMath::Max(Desired.Y, (Lo - Ld.P.Y) * 2.5);
	if (Ld.P.Y > Hi && Hi > Lo) Desired.Y = FMath::Min(Desired.Y, (Hi - Ld.P.Y) * 2.5);
	if (!ValidWater(S.Sp, Ahead.X, Ahead.Z) || Flat(Ld.P - S.Home).Size() > Sc.Roam * 1.6 + 10)
	{
		const FVector Home = Flat(S.Home - Ld.P).GetSafeNormal() * FMath::Max(Sc.Cruise, Desired.Size());
		Desired = FMath::Lerp(Desired, Home, .7);
		if (bMain && S.GoalT > 1) S.GoalT = 0;
	}
	// integrate
	const double Max = Flee.IsNearlyZero() && !bHunting ? FMath::Max(Sc.Cruise * 2.2, .6) : Sc.Burst;
	Desired = ClampLen(Desired, Max);
	Ld.V += ClampLen(Desired - Ld.V, Sc.Accel * (Flee.IsNearlyZero() ? 1 : 2) * Dt);
	Ld.P += Ld.V * Dt;
	if (Ld.P.Y < Top + .1) { Ld.P.Y = Top + .1; Ld.V.Y = FMath::Max(0.0, Ld.V.Y); }
	if (Ld.P.Y > -.25) { Ld.P.Y = -.25; Ld.V.Y = FMath::Min(0.0, Ld.V.Y); }
	// face the way it swims (slowly when hovering); a hovering hanger turns to watch a diver
	const double Spd = Ld.V.Size();
	FVector Face = Spd > .06 ? Ld.V / Spd : Ld.Fwd;
	if (Sc.Role == ERole::Hanger && Spd < .4) for (const FThreat& T : Threats) if (T.Kind == 0 && (T.P - Ld.P).Size() < 14) Face = (T.P - Ld.P).GetSafeNormal();
	Face.Y = FMath::Clamp(Face.Y, -.45, .45);
	Ld.Fwd = FMath::Lerp(Ld.Fwd, Face.GetSafeNormal(), FMath::Min(1.0, Dt * (Spd > .5 ? 5 : 1.2))).GetSafeNormal();
	// tail beat and how hard it swims
	const double Rel = Spd / FMath::Max(.05, S.Len);
	Ld.Phase = FMath::Fmod(Ld.Phase + Dt * 2 * PI * FMath::Min(9.0, .7 + .55 * Rel), PhaseWrap);
	Ld.Amp = FMath::Lerp(Ld.Amp, FMath::Clamp(.18 + .2 * Rel, .18, 1.0), FMath::Min(1.0, Dt * 3));
}

void AKeysSchools::StepSchool(FSchool& S, double Dt, double T)
{
	MainGoal(S, Dt);
	S.Ball = FMath::Max(0.0, S.Ball - Dt * .25);
	for (int32 k = 0; k < S.L.Num(); k++) SteerLeader(S, k, Dt);
}

// ================================================================ the track texture
void AKeysSchools::WriteSample()
{
	if (!Tex) return;
	const int32 Col = int32(SampleIdx % SchHist);
	for (FSchool& S : Schools)
	{
		// the main leader's own track, for its sub-leaders
		S.TrackP[S.TrackN % SchHist] = S.L[0].P;
		S.TrackF[S.TrackN % SchHist] = S.L[0].Fwd;
		S.TrackN++;
		const double Spread = Specs[S.Sp].Role == ERole::Bait ? FMath::Lerp(1.0, .55, S.Ball) : 1.0;
		for (const FLeader& Ld : S.L)
		{
			if (Ld.Row < 0) continue;
			FLinearColor* C = Column.GetData() + Ld.Row * 4;
			const FVector P = G2U(Ld.P), F = D2U(Ld.Fwd), Tp = G2U(Ld.ThreatP);
			const double Top = TopAt(Ld.P.X, Ld.P.Z);
			C[0] = FLinearColor(float(P.X), float(P.Y), float(P.Z), float(Ld.Phase));
			C[1] = FLinearColor(float(F.X), float(F.Y), float(F.Z), float(Spread));
			C[2] = FLinearColor(float(Tp.X), float(Tp.Y), float(Tp.Z), float(Ld.ThreatK));
			C[3] = FLinearColor(float(Top * 100), -15.f, float(Ld.Amp), float(Ld.Alarm));
		}
	}
	{
		FLinearColor* Buf = new FLinearColor[SchRows * 4];
		FMemory::Memcpy(Buf, Column.GetData(), SchRows * 4 * sizeof(FLinearColor));
		FUpdateTextureRegion2D* Rg = new FUpdateTextureRegion2D(Col * 4, 0, 0, 0, 4, SchRows);
		Tex->UpdateTextureRegions(0, 1, Rg, 4 * sizeof(FLinearColor), sizeof(FLinearColor), (uint8*)Buf, [](uint8* D, const FUpdateTextureRegion2D* R) { delete[] (FLinearColor*)D; delete R; });
		Uploads++;
	}
	// a new leader's whole history starts where it is now, so its followers have something to read
	for (const int32 Row : FreshRows)
	{
		FLinearColor* Buf = new FLinearColor[SchHist * 4];
		for (int32 h = 0; h < SchHist; h++) FMemory::Memcpy(Buf + h * 4, Column.GetData() + Row * 4, 4 * sizeof(FLinearColor));
		FUpdateTextureRegion2D* Rg = new FUpdateTextureRegion2D(0, Row, 0, 0, SchHist * 4, 1);
		Tex->UpdateTextureRegions(0, 1, Rg, SchHist * 4 * sizeof(FLinearColor), sizeof(FLinearColor), (uint8*)Buf, [](uint8* D, const FUpdateTextureRegion2D* R) { delete[] (FLinearColor*)D; delete R; });
	}
	FreshRows.Reset();
	SampleIdx++;
}

// ================================================================ per frame
void AKeysSchools::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	const double T0 = FPlatformTime::Seconds();
	const double Dt = FMath::Clamp(double(DeltaSeconds), 0.0, .1);
	NowT = GetWorld()->GetTimeSeconds();
	TilesThisFrame = 0;
	if (bOff) { FrameAcc += DeltaSeconds; CpuN++; StatT += Dt; if (bScriptLog && StatT > 5) { StatT = 0; DebugCmd(TEXT("stats")); } return; }
	ReadWorld(Dt);
	PopT -= Dt;
	if (PopT <= 0) { PopT = .5; Populate(); }
	// leaders: every frame up close, every 2nd / 4th frame further off (with the time they missed)
	static uint32 TickN = 0;
	TickN++;
	for (FSchool& S : Schools)
	{
		const double D = Hyp(S.L[0].P.X - Cam.X, S.L[0].P.Z - Cam.Z);
		const uint32 Every = D < 50 ? 1 : D < 100 ? 2 : 4;
		S.Step += Dt;
		if ((TickN + uint32(S.Id)) % Every) continue;
		StepSchool(S, FMath::Min(S.Step, .12), NowT);
		S.Step = 0;
	}
	// detail level and anchoring: only these touch instance data
	for (FSchool& S : Schools)
	{
		const double D = (S.L[0].P - Cam).Size();
		const bool bSmall = S.Len < .25;
		int32 Lod = D < 16 ? 0 : D < 45 ? 1 : 2;
		if (bSmall) Lod = FMath::Max(1, Lod);
		if (S.Lod >= 0 && FMath::Abs(Lod - S.Lod) == 1)
		{
			// hysteresis
			const double Edge = (FMath::Max(Lod, S.Lod) == 1) ? 16 : 45;
			if (FMath::Abs(D - Edge) < 4) Lod = S.Lod;
		}
		if (Lod != S.Lod)
		{
			const int32 P = PoolFor(S.Sp, Lod);
			if (P < 0) continue;
			if (PoolDirty.IsValidIndex(S.Pool)) PoolDirty[S.Pool] = true;
			S.Pool = P;
			S.Lod = Lod;
			PoolDirty[P] = true;
		}
		FVector Mid = FVector::ZeroVector;
		double Far = 0;
		for (const FLeader& Ld : S.L) Mid += Ld.P;
		Mid /= S.L.Num();
		const FVector MidU = G2U(Mid);
		for (const FLeader& Ld : S.L) Far = FMath::Max(Far, (G2U(Ld.P) - S.AnchorUE).Size() / 100);
		if (Far > RebaseDist)
		{
			S.AnchorUE = MidU;
			Rebases++;
			if (Pools.IsValidIndex(S.Pool) && Pools[S.Pool] && !PoolDirty[S.Pool])
			{
				TArray<FTransform> Xf;
				Xf.Init(FTransform(S.AnchorUE), S.NFish);
				Pools[S.Pool]->BatchUpdateInstancesTransforms(S.First, Xf, false, true, true);
			}
		}
	}
	FlushPools();
	// the track: 20 samples a second; the material interpolates between the last two
	SampleT += Dt;
	int32 NS = 0;
	while (SampleT >= SchStep && NS < 3) { SampleT -= SchStep; WriteSample(); NS++; }
	if (SampleT >= SchStep) SampleT = FMath::Fmod(SampleT, SchStep);
	if (MID)
	{
		auto Wrap = [](double H) { return FMath::Fmod(FMath::Fmod(H, double(SchHist)) + SchHist, double(SchHist)); };
		const double Head = double(SampleIdx - 2) + SampleT / SchStep;
		MID->SetScalarParameterValue(TEXT("Head"), float(Wrap(Head)));
		MID->SetScalarParameterValue(TEXT("HeadPrev"), float(Wrap(Head - Dt / SchStep)));
	}
	// rough cost, for the log
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
			for (FLeader& Ld : S->L) Ld.Fwd = FVector::CrossProduct(Flat2, FVector(0, 1, 0)).GetSafeNormal();
			LastPut = S->Id;
		}
		UE_LOG(LogTemp, Log, TEXT("KRSchools put %s at %.1f %.1f %.1f (top %.1f): %d fish, %d leaders"), Specs[Sp].Key, At.X, Yc, At.Z, Top, S ? S->NFish : 0, S ? S->L.Num() : 0);
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
		const int32 PreyId = Prey->Id, PreySp = Prey->Sp, PreyN = Prey->NFish;
		const FVector PreyP = Prey->L[0].P;
		const int32 Sp = C == TEXT("shark") ? Blacktip : Jack;
		const FVector Side = FVector::CrossProduct(Flat2, FVector(0, 1, 0)).GetSafeNormal();
		const FVector From = PreyP + Side * 16 + Flat2 * 3;
		double Fl;
		const double Top = TopAt(From.X, From.Z, &Fl);
		if (FSchool* S = Spawn(Sp, FVector(From.X, FMath::Max(PreyP.Y, Top + 1), From.Z), Top, 0, 4242, 1))
		{
			S->bPinned = true;
			S->PreyId = PreyId;
			S->HuntDir = (PreyP - S->L[0].P).GetSafeNormal();
			S->HuntLeft = (PreyP - S->L[0].P).Size() / Specs[Sp].Burst + 2;
			S->HuntT = 30;
			for (FLeader& Ld : S->L) { Ld.Fwd = S->HuntDir; Ld.V = S->HuntDir * Specs[Sp].Burst * .7; }
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
			FVector Mid = FVector::ZeroVector;
			for (const FLeader& Ld : S->L) Mid += Ld.P;
			Mid /= S->L.Num();
			const KeysDive::FState& St = Dv->State();
			const FVector Dir = (Mid - FVector(St.X, St.Y + .15, St.Z)).GetSafeNormal();
			Dv->DebugCmd(TEXT("look"), FString::Printf(TEXT("%f,%f"), FMath::Atan2(-Dir.X, -Dir.Z), FMath::Asin(FMath::Clamp(Dir.Y, -1.0, 1.0))));
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
		int32 Fish = 0, Lead = 0;
		for (const FSchool& S : Schools) { Fish += S.NFish; Lead += S.L.Num(); }
		UE_LOG(LogTemp, Log, TEXT("KRSchools stats: %d schools, %d fish, %d leaders on the CPU, %d tiles; CPU %.3f ms/frame avg, frame %.2f ms avg (%.0f fps); %d uploads, %d pool rebuilds, %d rebases"),
			Schools.Num(), Fish, Lead, Tiles.Num(), CpuN ? CpuAcc / CpuN * 1000 : 0, CpuN ? FrameAcc / CpuN * 1000 : 0, FrameAcc > 0 ? CpuN / FrameAcc : 0, Uploads, Rebuilds, Rebases);
		CpuAcc = FrameAcc = 0; CpuN = Uploads = Rebuilds = Rebases = 0;
	}
}
