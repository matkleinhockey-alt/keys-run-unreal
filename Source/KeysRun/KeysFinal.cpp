#include "KeysFinal.h"
#include "KeysKit.h"
#include "KeysGeo.h"
#include "KeysSea.h"
#include "KeysMesh.h"
#include "KeysData.h"
#include "KeysWorld.h"
#include "KeysLife.h"
#include "KeysAudio.h"
#include "KeysTownData.h"
#include "KeysWrecks.h"
#include "KeysDiver.h"
#include "KeysBoatPawn.h"
#include "ProceduralMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Components/PointLightComponent.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Algo/Reverse.h"

using namespace KeysGeo;
using namespace KeysKit;

namespace
{
	// entities/fish/legend-path.ts
	constexpr uint32 FinWorldSeed = 20240817;
	constexpr int32 LegendSalt = 7718, LegendTargets = 7;
	constexpr double LegendCell = 100, LegendMinDepth = 14, LegendMinShore = 60, LegendShoreMargin = 70, LegendDepthMargin = 6, LegendDMin = 6;
	constexpr double LegendMeanSpeed = 1.3, LegendLen = 3.1, LegendSeeM = 140;
	const double LegendSurges[2][2] = {{500, 5400}, {120, 1300}};
	// spout.ts
	constexpr double BlowLife = 1.8, BlowRise = 7;
	// dock-celebration.ts
	constexpr double HypeS = 15;
	// wreck-chart.ts
	constexpr double WreckChartM = 260;
	const uint32 FinSkins[] = {0x8d5524, 0xc68642, 0xe0ac69, 0xf1c27d, 0xffdbac, 0x6b4226, 0xd9a27f};
	const uint32 FinShirts[] = {0xffffff, 0x2f8fd8, 0xf26b5b, 0x3fb68b, 0xf4c542, 0x8a5cc2, 0x1f2f46, 0xff8c42, 0x9ad0ec};
	const uint32 FinShorts[] = {0x2b4a6e, 0xc9b38a, 0x3b3b3b, 0x6e7f5c, 0x7a8ca3};
	const uint32 FinHair[] = {0x3b2a1e, 0x1a1a1a, 0x8a6a3a, 0xc9a66b};
	template <typename T, int32 N> const T& Pick(const T (&A)[N], int32 I) { return A[((I % N) + N) % N]; }

	double WallSeconds() { return double((FDateTime::UtcNow() - FDateTime(1970, 1, 1)).GetTicks()) / ETimespan::TicksPerSecond; }

	bool LegalAt(double X, double Z, double DMin, double ShoreMin)
	{
		return X >= WBX0 + 40 && X <= WBX1 - 40 && Z >= WBZ0 + 40 && Z <= WBZ1 - 40 && DepthAt(X, Z) >= DMin && ShoreInfo(X, Z).D >= ShoreMin;
	}
	FVector GM2U(const FVector& G) { return ToUE(G.X, G.Y, G.Z) / 100.0; }   // game meters -> FKeysMesh meters
}

AKeysFinal::AKeysFinal()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickGroup = TG_PostPhysics;
	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent = Root;
	auto Mk = [this](const TCHAR* Name)
	{
		UProceduralMeshComponent* C = CreateDefaultSubobject<UProceduralMeshComponent>(Name);
		C->SetupAttachment(Root);
		C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		return C;
	};
	LegendMesh = Mk(TEXT("Legend"));
	SpoutMesh = Mk(TEXT("Spouts"));
	SpoutMesh->SetCastShadow(false);
	StationMesh = Mk(TEXT("Station"));
	CrowdMesh = Mk(TEXT("Crowd"));
	FlashMesh = Mk(TEXT("Flashes"));
	FlashMesh->SetCastShadow(false);
	StudioMesh = Mk(TEXT("Studio"));
	StudioMesh->SetCastShadow(false);
	Capture = CreateDefaultSubobject<USceneCaptureComponent2D>(TEXT("PortraitCapture"));
	Capture->SetupAttachment(Root);
	Capture->bCaptureEveryFrame = false;
	Capture->bCaptureOnMovement = false;
	StudioKey = CreateDefaultSubobject<UPointLightComponent>(TEXT("StudioKey"));
	StudioKey->SetupAttachment(Root);
	StudioKey->SetCastShadows(false);
	StudioKey->SetVisibility(false);
	StudioRim = CreateDefaultSubobject<UPointLightComponent>(TEXT("StudioRim"));
	StudioRim->SetupAttachment(Root);
	StudioRim->SetCastShadows(false);
	StudioRim->SetVisibility(false);
}

AKeysFinal* AKeysFinal::Get(const UObject* WorldContext)
{
	UWorld* W = WorldContext ? WorldContext->GetWorld() : nullptr;
	if (!W) return nullptr;
	TActorIterator<AKeysFinal> It(W);
	return It ? *It : nullptr;
}

void AKeysFinal::BeginPlay()
{
	Super::BeginPlay();
	SetActorLocation(FVector::ZeroVector);
	if (UMaterialInterface* GM = AKeysWorld::GlowMaterial())
	{
		SpoutMID = UMaterialInstanceDynamic::Create(GM, this);
		SpoutMID->SetScalarParameterValue(TEXT("Bright"), 2.6f);
		FlashMID = UMaterialInstanceDynamic::Create(GM, this);
		FlashMID->SetScalarParameterValue(TEXT("Bright"), 12.f);
		BackdropMID = UMaterialInstanceDynamic::Create(GM, this);
		BackdropMID->SetScalarParameterValue(TEXT("Bright"), 1.f);
	}
	// the three gold bluefin, one model shared
	{
		FKeysMesh M;
		for (int32 i = 0; i < 3; i++) Members.Add({i == 0 ? 0.0 : (i == 1 ? -3.2 : 3.0), i == 0 ? 0.0 : (i == 1 ? 4.0 : 3.4), i * 2.1, 3.0 + i * 4.0});
		for (int32 i = 0; i < 3; i++) AppendFish(M, TEXT("goldbluefin"), LegendLen, FTransform(FVector(Members[i].OZ, Members[i].OX, 0)));
		M.Commit(LegendMesh, 0, AKeysWorld::VertexColorMaterial());
		LegendMesh->SetVisibility(false);
	}
	// the studio for the underwater portrait, far off the chart and high up where nothing else is
	StudioO = ToUE(WBX0 - 1800, 1600, WBZ0 - 1800);
	PortraitRT = NewObject<UTextureRenderTarget2D>(this);
	PortraitRT->RenderTargetFormat = RTF_RGBA8_SRGB;
	PortraitRT->ClearColor = FLinearColor(.01f, .05f, .06f);
	PortraitRT->InitAutoFormat(640, 480);
	Capture->TextureTarget = PortraitRT;
	Capture->CaptureSource = SCS_FinalColorLDR;
	Capture->PrimitiveRenderMode = ESceneCapturePrimitiveRenderMode::PRM_UseShowOnlyList;
	Capture->ShowOnlyComponents.Add(StudioMesh);
	Capture->FOVAngle = 42;
	Capture->ShowFlags.SetAtmosphere(false);
	Capture->ShowFlags.SetFog(false);
	Capture->ShowFlags.SetVolumetricFog(false);
	Capture->ShowFlags.SetCloud(false);
	Capture->PostProcessSettings.bOverride_AutoExposureMethod = true;
	Capture->PostProcessSettings.AutoExposureMethod = AEM_Manual;
	Capture->PostProcessSettings.bOverride_AutoExposureBias = true;
	Capture->PostProcessSettings.AutoExposureBias = 3.f;
	Capture->PostProcessSettings.bOverride_AutoExposureApplyPhysicalCameraExposure = true;
	Capture->PostProcessSettings.AutoExposureApplyPhysicalCameraExposure = false;
	Capture->PostProcessSettings.bOverride_VignetteIntensity = true;
	Capture->PostProcessSettings.VignetteIntensity = .6f;
	Capture->PostProcessSettings.bOverride_SceneColorTint = true;
	Capture->PostProcessSettings.SceneColorTint = FLinearColor(.8f, .97f, 1.f);
	Capture->PostProcessSettings.bOverride_BloomIntensity = true;
	Capture->PostProcessSettings.BloomIntensity = 0;
	for (UPointLightComponent* L : {StudioKey.Get(), StudioRim.Get()})
	{
		L->SetAttenuationRadius(1400);
		L->SetIntensityUnits(ELightUnits::Unitless);
	}
	StudioKey->SetLightColor(FLinearColor(.75f, .95f, 1.f));
	StudioRim->SetLightColor(FLinearColor(.45f, .9f, .85f));
}

// ================================================================ the gold bluefin (legend-path.ts)
void AKeysFinal::BuildLegendLoop()
{
	bLoopTried = true;
	const int32 GX = FMath::CeilToInt((WBX1 - WBX0) / LegendCell), GZ = FMath::CeilToInt((WBZ1 - WBZ0) / LegendCell);
	auto CX = [](int32 I) { return WBX0 + (I + .5) * LegendCell; };
	auto CZ = [](int32 J) { return WBZ0 + (J + .5) * LegendCell; };
	const double WD = FMath::Max(LegendDMin, LegendMinDepth) + LegendDepthMargin, WS = LegendMinShore + LegendShoreMargin;
	TArray<uint8> Ok;
	Ok.SetNumZeroed(GX * GZ);
	for (int32 J = 0; J < GZ; J++) for (int32 I = 0; I < GX; I++)
	{
		const double X = CX(I), Z = CZ(J), H = LegendCell / 2;
		// the cheap test first: most of the chart is shallow
		if (DepthAt(X, Z) < WD) continue;
		Ok[J * GX + I] = LegalAt(X, Z, WD, WS) && LegalAt(X - H, Z - H, WD, WS) && LegalAt(X + H, Z - H, WD, WS) && LegalAt(X - H, Z + H, WD, WS) && LegalAt(X + H, Z + H, WD, WS) ? 1 : 0;
	}
	// the biggest connected body of legal water
	TArray<int32> Comp;
	Comp.Init(-1, GX * GZ);
	int32 Best = -1, BestSize = 0, NC = 0;
	for (int32 S = 0; S < Ok.Num(); S++)
	{
		if (!Ok[S] || Comp[S] >= 0) continue;
		TArray<int32> Q{S};
		Comp[S] = NC;
		int32 Size = 0;
		while (Q.Num())
		{
			const int32 C = Q.Pop(EAllowShrinking::No);
			Size++;
			const int32 Ci = C % GX, Cj = C / GX;
			const int32 D4[4][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};
			for (const auto& D : D4)
			{
				const int32 Ni = Ci + D[0], Nj = Cj + D[1];
				if (Ni < 0 || Nj < 0 || Ni >= GX || Nj >= GZ) continue;
				const int32 N = Nj * GX + Ni;
				if (Ok[N] && Comp[N] < 0) { Comp[N] = NC; Q.Add(N); }
			}
		}
		if (Size > BestSize) { BestSize = Size; Best = NC; }
		NC++;
	}
	if (Best < 0) return;
	TArray<int32> Cells;
	for (int32 S = 0; S < Ok.Num(); S++) if (Comp[S] == Best) Cells.Add(S);
	auto Pos = [&](int32 C) { return FVector2D(CX(C % GX), CZ(C / GX)); };
	// targets: from a hashed pool, always the one farthest from those already chosen
	TArray<int32> Pool;
	for (int32 K = 0; K < 40; K++) Pool.Add(Cells[FMath::Min(Cells.Num() - 1, int32(KeysSea::HashCell(FinWorldSeed, LegendSalt, K, 1) * Cells.Num()))]);
	TArray<int32> Chosen{Pool[0]};
	while (Chosen.Num() < LegendTargets)
	{
		int32 BC = Pool[0];
		double BD = -1;
		for (const int32 C : Pool)
		{
			double DMin = 1e18;
			for (const int32 O : Chosen) DMin = FMath::Min(DMin, FVector2D::Distance(Pos(C), Pos(O)));
			if (DMin > BD) { BD = DMin; BC = C; }
		}
		Chosen.Add(BC);
	}
	// A* (8-connected, no corner cutting through illegal cells)
	auto AStar = [&](int32 A, int32 B)
	{
		TMap<int32, double> G{{A, 0}};
		TMap<int32, int32> From;
		TArray<TPair<double, int32>> Open{{0, A}};
		TSet<int32> Done;
		const FVector2D BP = Pos(B);
		while (Open.Num())
		{
			int32 Mi = 0;
			for (int32 K = 1; K < Open.Num(); K++) if (Open[K].Key < Open[Mi].Key) Mi = K;
			const int32 C = Open[Mi].Value;
			Open.RemoveAtSwap(Mi);
			if (C == B) break;
			if (Done.Contains(C)) continue;
			Done.Add(C);
			const int32 Ci = C % GX, Cj = C / GX;
			for (int32 Dj = -1; Dj <= 1; Dj++) for (int32 Di = -1; Di <= 1; Di++)
			{
				if (!Di && !Dj) continue;
				const int32 Ni = Ci + Di, Nj = Cj + Dj;
				if (Ni < 0 || Nj < 0 || Ni >= GX || Nj >= GZ || !Ok[Nj * GX + Ni]) continue;
				if (Di && Dj && (!Ok[Cj * GX + Ni] || !Ok[Nj * GX + Ci])) continue;
				const int32 N = Nj * GX + Ni;
				const double NG = G[C] + (Di && Dj ? 1.414 : 1);
				const double* Old = G.Find(N);
				if (!Old || NG < *Old)
				{
					G.Add(N, NG); From.Add(N, C);
					Open.Add({NG + FVector2D::Distance(FVector2D(CX(Ni), CZ(Nj)), BP) / LegendCell, N});
				}
			}
		}
		TArray<int32> Out{B};
		while (Out.Last() != A)
		{
			const int32* P = From.Find(Out.Last());
			if (!P) return TArray<int32>();
			Out.Add(*P);
		}
		Algo::Reverse(Out);
		return Out;
	};
	TArray<int32> Route;
	for (int32 K = 0; K < Chosen.Num(); K++)
	{
		const TArray<int32> Leg = AStar(Chosen[K], Chosen[(K + 1) % Chosen.Num()]);
		if (!Leg.Num()) return;
		for (int32 M = K ? 1 : 0; M < Leg.Num() - (K == Chosen.Num() - 1 ? 1 : 0); M++) Route.Add(Leg[M]);
	}
	// no hairpins: cancel any A-B-A
	TArray<int32> Clean;
	for (const int32 C : Route) { if (Clean.Num() >= 2 && Clean[Clean.Num() - 2] == C) Clean.Pop(); else Clean.Add(C); }
	TArray<FVector2D> Raw;
	for (const int32 C : Clean) Raw.Add(Pos(C));
	// round it (Chaikin), backing off the rounding if it ever leaves legal water; resample by arc length
	for (int32 Iters = 4; Iters >= 0; Iters--)
	{
		TArray<FVector2D> P = Raw;
		for (int32 It = 0; It < Iters; It++)
		{
			TArray<FVector2D> Q;
			for (int32 K = 0; K < P.Num(); K++) { const FVector2D A = P[K], B = P[(K + 1) % P.Num()]; Q.Add(A * .75 + B * .25); Q.Add(A * .25 + B * .75); }
			P = MoveTemp(Q);
		}
		double Len = 0;
		TArray<double> Seg;
		for (int32 K = 0; K < P.Num(); K++) { Seg.Add(FVector2D::Distance(P[K], P[(K + 1) % P.Num()])); Len += Seg[K]; }
		const int32 N = FMath::FloorToInt(Len / 20);
		if (N < 4) continue;
		TArray<FVector2D> Pts;
		int32 K = 0;
		double Acc = 0;
		bool bValid = true;
		for (int32 S = 0; S < N; S++)
		{
			const double Target = S * Len / N;
			while (K < Seg.Num() - 1 && Acc + Seg[K] < Target) { Acc += Seg[K]; K++; }
			const double U = Seg[K] > 0 ? (Target - Acc) / Seg[K] : 0;
			const FVector2D Q = FMath::Lerp(P[K], P[(K + 1) % P.Num()], U);
			if (!LegalAt(Q.X, Q.Y, FMath::Max(LegendDMin, LegendMinDepth) + 1, LegendMinShore + 5)) { bValid = false; break; }
			Pts.Add(Q);
		}
		if (!bValid) continue;
		Loop = MoveTemp(Pts);
		LoopLen = Len;
		LoopStep = Len / N;
		LoopOffset = KeysSea::HashCell(FinWorldSeed, LegendSalt, 99, 2) * Len;
		LoopPhase = KeysSea::HashCell(FinWorldSeed, LegendSalt, 98, 3);
		UE_LOG(LogTemp, Log, TEXT("Keys Run: the gold bluefin's loop - %.1f km through %d cells"), Len / 1000, Cells.Num());
		return;
	}
}

bool AKeysFinal::LegendPose(double& OutX, double& OutZ, double& OutH) const
{
	if (Loop.Num() < 4) return false;
	auto At = [this](double TSec)
	{
		double S = LegendMeanSpeed * TSec;
		for (int32 i = 0; i < 2; i++) S += LegendSurges[i][0] * FMath::Sin(2 * PI * TSec / LegendSurges[i][1] + LoopPhase * (i + 1) * 6.283);
		S = FMath::Fmod(FMath::Fmod(S + LoopOffset, LoopLen) + LoopLen, LoopLen);
		const double F = S / LoopStep;
		const int32 I = int32(FMath::FloorToDouble(F)) % Loop.Num(), J = (I + 1) % Loop.Num();
		return FMath::Lerp(Loop[I], Loop[J], F - FMath::FloorToDouble(F));
	};
	const double T = WallSeconds();
	const FVector2D P = At(T), P2 = At(T + 15), P1 = At(T - 15);
	OutX = P.X; OutZ = P.Y;
	OutH = FMath::Atan2(-(P2.X - P1.X), -(P2.Y - P1.Y));
	return true;
}

void AKeysFinal::TickLegend(double Dt, double T)
{
	if (!bLoopTried) BuildLegendLoop();
	double LX, LZ, LH;
	AKeysBoatPawn* P = Cast<AKeysBoatPawn>(UGameplayStatics::GetPlayerPawn(this, 0));
	if (!P || !LegendPose(LX, LZ, LH)) { LegendMesh->SetVisibility(false); return; }
	const double D = FMath::Sqrt(FMath::Square(LX - P->BoatX()) + FMath::Square(LZ - P->BoatZ()));
	const bool bShow = D < 900;
	LegendMesh->SetVisibility(bShow);
	if (!bShow) return;
	// the school as one rigid group (the fish themselves are one mesh), riding a little under the surface, with a lazy
	// weave; now and then one of them busts the surface (the web game's 'bust' act) with a splash
	const double Fx = -FMath::Sin(LH), Fz = -FMath::Cos(LH);
	const double Surf = WaveH(LX, LZ, T, AmpAt(LX, LZ), false);
	double Lift = 0, Pitch = 0;
	for (FMember& M : Members)
	{
		M.ActT -= Dt;
		if (M.AP < 0 && M.ActT <= 0) { M.AP = 0; M.bSpl = false; }
		if (M.AP >= 0)
		{
			M.AP += Dt / 1.4;
			const double K = FMath::Sin(FMath::Min(1.0, M.AP) * PI);
			Lift = FMath::Max(Lift, K * 1.25);
			Pitch = FMath::Cos(FMath::Min(1.0, M.AP) * PI) * .35 * K;
			if (M.AP > .4 && !M.bSpl)
			{
				M.bSpl = true;
				if (AKeysLife* L = AKeysLife::Get(this)) L->Splash(LX + Fx * M.OZ, LZ + Fz * M.OZ, 14, 1.0);
				if (AKeysAudio* A = AKeysAudio::Get(this)) A->Splash(LX, LZ, 20, 1.2);
			}
			if (M.AP >= 1) { M.AP = -1; M.ActT = FMath::FRandRange(8.0, 22.0); }
		}
	}
	const double Y = Surf - .8 + Lift + FMath::Sin(T * .7) * .12;
	const double Weave = FMath::Sin(T * 1.6) * .06;
	LegendMesh->SetWorldLocationAndRotation(ToUE(LX, Y, LZ), FRotator(FMath::RadiansToDegrees(Pitch), YawDeg(LH) + FMath::RadiansToDegrees(Weave), 0));
	if (D < LegendSeeM && !bLegendSeen)
	{
		bLegendSeen = true;
		P->ToastMsg(TEXT("Gold bluefin! Three of them, lit up like coins under the surface. The legend is real - and they can't be caught."), 6);
	}
	if (D < LegendSeeM) KeysTags::Add(FVector(LX, Surf + 3, LZ), TEXT("Gold bluefin"), LegendSeeM);
}

// ================================================================ whale spouts (spout.ts)
void AKeysFinal::Spout(double X, double Y, double Z)
{
	if (Blows.Num() >= 12) return;
	Blows.Add({X, Y, Z, 0});
	UE_LOG(LogTemp, Log, TEXT("Keys Run: spout at %.0f, %.0f"), X, Z);
	if (AKeysAudio* A = AKeysAudio::Get(this)) A->Splash(X, Z, 6, .5);
}

void AKeysFinal::TickSpouts(double Dt)
{
	for (FBlow& B : Blows) B.Age += Dt;
	Blows.RemoveAll([](const FBlow& B) { return B.Age >= BlowLife; });
	if (!Blows.Num()) { SpoutMesh->SetVisibility(false); return; }
	FKeysMesh M;
	const FLinearColor Col = Hex(0x9fb0b8);
	for (const FBlow& B : Blows)
	{
		// a fast initial rise, then it lingers and thins: a vapour plume dispersing
		const double F = B.Age / BlowLife, Rise = FMath::Min(1.0, F * 2.2) * BlowRise, Girth = (1 - F * .5) * FMath::Min(1.0, F * 5 + .15);
		for (int32 K = 0; K <= 7; K++)
		{
			const double U = K / 7.0;
			FLinearColor C = Col;
			C.A = float(FMath::Min(1.0, 1.4 * (1 - F)) * (1 - .4 * U));
			M.AddGlowBlob(GM2U(FVector(B.X, B.Y + Rise * U, B.Z)), 1.2 * Girth * (.4 + 1.1 * U), C);
		}
	}
	M.Commit(SpoutMesh, 0, SpoutMID ? (UMaterialInterface*)SpoutMID : AKeysWorld::GlowMaterial());
	SpoutMesh->SetVisibility(true);
}

// ================================================================ the dockside trophy (dock-celebration.ts)
bool AKeysFinal::FindStation(double DockX, double DockZ, double BoatX, double BoatZ, FVector& Out) const
{
	const double BoatD = FMath::Sqrt(FMath::Square(DockX - BoatX) + FMath::Square(DockZ - BoatZ));
	double BestScore = -1e9;
	bool bAny = false;
	for (int32 R = 7; R <= 18; R++)
		for (int32 K = 0; K < 24; K++)
		{
			const double A = K / 24.0 * 2 * PI, X = DockX + FMath::Cos(A) * R, Z = DockZ + FMath::Sin(A) * R;
			double Y;
			if (!KeysTown::GroundAt(X, Z, Y, false) || FMath::Sqrt(FMath::Square(X - BoatX) + FMath::Square(Z - BoatZ)) < BoatD + 1) continue;
			if (KeysTown::ObstacleAt(X, Z, 1.2)) continue;
			bool bHit = false;
			for (const KeysTown::FRect& C : KeysTown::Colliders()) if (X > C.X0 - 1.2 && X < C.X1 + 1.2 && Z > C.Z0 - 1.2 && Z < C.Z1 + 1.2) { bHit = true; break; }
			if (bHit) continue;
			int32 Room = 0;
			for (int32 J = 0; J < 12; J++)
			{
				const double B = J / 12.0 * 2 * PI;
				double G;
				if (KeysTown::GroundAt(X + FMath::Cos(B) * 3, Z + FMath::Sin(B) * 3, G, false) && FMath::Abs(G - Y) < .3) Room++;
			}
			const double Score = Room * 10 - R;
			if (Score > BestScore) { BestScore = Score; Out = FVector(X, Y, Z); bAny = true; }
		}
	return bAny && BestScore > 20;
}

void AKeysFinal::OnAshore(const FString& Key, double Weight, int32 Id, bool bReleased, const FString& Boat, double DockX, double DockZ, double BoatX, double BoatZ)
{
	if (Key.IsEmpty() || Id < 0) return;
	AKeysBoatPawn* P = Cast<AKeysBoatPawn>(UGameplayStatics::GetPlayerPawn(this, 0));
	if (Id == ShownId)
	{
		// the same fish as last time: still on display, just the regulars round it
		if (!bStation) return;
		bCrowdOn = true;
		bOnlyCore = true;
		HypeT = HypeS;
		CrowdMesh->SetVisibility(true);
		return;
	}
	FVector At;
	if (!FindStation(DockX, DockZ, BoatX, BoatZ, At)) return;
	const KeysData::FSpecies* Sp = KeysData::Species(Key);
	const double D = FMath::Max(1e-3, FMath::Sqrt(FMath::Square(DockX - At.X) + FMath::Square(DockZ - At.Z)));
	const double FX = (DockX - At.X) / D, FZ = (DockZ - At.Z) / D;
	StationPos = At;
	UE_LOG(LogTemp, Log, TEXT("Keys Run: dock trophy at %.1f, %.1f, %.1f (dock %.1f, %.1f)"), At.X, At.Y, At.Z, DockX, DockZ);
	FaceYaw = FMath::Atan2(FZ, FX);
	FishLenM = KeysMesh::FishLength(Weight);
	// the weigh station: two posts and a crossbar, the scale and the hook, the fish hung by the tail; local +x faces
	// the crowd (and the fish's flank with it), the crossbar runs along local z
	const double C = FMath::Cos(FaceYaw), S = FMath::Sin(FaceYaw);
	auto L = [&](double X, double Y, double Z) { return GM2U(FVector(At.X + X * C - Z * S, At.Y + Y, At.Z + X * S + Z * C)); };
	const double Top = FMath::Max(3.6, FishLenM + 2.2), Half = FMath::Max(1.1, FishLenM * .35);
	const FLinearColor Wood = Hex(0x6b4a2f), Steel = Hex(0xd9dde2);
	FKeysMesh M;
	M.AddTube(L(0, 0, -Half), L(0, Top, -Half), .08, .08, 8, Wood);
	M.AddTube(L(0, 0, Half), L(0, Top, Half), .08, .08, 8, Wood);
	M.AddTube(L(0, Top, -Half - .15), L(0, Top, Half + .15), .07, .07, 8, Wood);
	M.AddTube(L(0, Top, 0), L(0, Top - .24, 0), .012, .012, 5, Steel);
	M.AddTube(L(-.03, Top - .3, 0), L(.03, Top - .3, 0), .11, .11, 16, Hex(0xc8102e));
	M.AddTube(L(0, Top - .36, 0), L(0, Top - .6, 0), .012, .012, 5, Steel);
	// hung nose down, its flank to the crowd
	const FQuat Q = FQuat(FVector::ZAxisVector, FaceYaw + PI / 2) * FQuat(FVector::YAxisVector, PI / 2);
	AppendFish(M, Key, FishLenM, FTransform(Q, L(0, Top - .6 - FishLenM / 2, 0)));
	// the sign board over the bar
	M.AddBox(L(-.03, Top + .62, 0), FVector(.05, 1.8, .9), Hex(0x0d2a44), FQuat(FVector::ZAxisVector, FaceYaw));
	M.AddBox(L(-.07, Top + .62, 0), FVector(.04, 1.95, 1.05), Hex(0xf4c542), FQuat(FVector::ZAxisVector, FaceYaw));
	M.Commit(StationMesh, 0, AKeysWorld::VertexColorMaterial());
	StationMesh->SetVisibility(true);
	for (UTextRenderComponent* T : SignTexts) if (T) T->DestroyComponent();
	SignTexts.Reset();
	const FString Lines[3] = {(Sp ? Sp->Name : Key).ToUpper(), FString::Printf(TEXT("%.1f LB"), Weight), bReleased ? TEXT("Caught & released - ") + Boat : TEXT("Biggest of the run - ") + Boat};
	const float Sizes[3] = {.17f, .26f, .1f};
	const double Ys[3] = {.27, .02, -.25};
	for (int32 i = 0; i < 3; i++)
	{
		UTextRenderComponent* T = NewObject<UTextRenderComponent>(this);
		T->SetupAttachment(Root);
		T->RegisterComponent();
		T->SetText(FText::FromString(Lines[i]));
		T->SetHorizontalAlignment(EHTA_Center);
		T->SetVerticalAlignment(EVRTA_TextCenter);
		T->SetTextRenderColor(i == 1 ? FColor(0xf4, 0xc5, 0x42) : FColor::White);
		T->SetWorldSize(Sizes[i] * 100);
		T->SetWorldLocationAndRotation(L(.01, Top + .62 + Ys[i], 0) * 100, FRotator(0, FMath::RadiansToDegrees(FaceYaw), 0));
		T->SetCastShadow(false);
		SignTexts.Add(T);
	}
	FishCenter = FVector(At.X, At.Y + Top - .6 - FishLenM / 2, At.Z);
	// the crowd: two arcs on the dock side, a gap in the front row to see through, clear of the plank you stepped onto
	Fans.Reset();
	const double Rows[2][2] = {{FMath::Max(2.4, FishLenM * .5 + 1.6), 7}, {FMath::Max(3.4, FishLenM * .5 + 2.6), 9}};
	int32 I = 0;
	for (int32 Row = 0; Row < 2; Row++)
		for (int32 K = 0; K < int32(Rows[Row][1]); K++)
		{
			const double N = Rows[Row][1], Spread = PI * 1.25 * (K / (N - 1) - .5) + (Row ? .12 : 0);
			if (Row == 0 && FMath::Abs(Spread) < .35) continue;
			const double A = FaceYaw + Spread, X = At.X + FMath::Cos(A) * Rows[Row][0], Z = At.Z + FMath::Sin(A) * Rows[Row][0];
			double Y;
			if (!KeysTown::GroundAt(X, Z, Y, false) || FMath::Abs(Y - At.Y) > .6) continue;
			if (FMath::Sqrt(FMath::Square(X - DockX) + FMath::Square(Z - DockZ)) < 2) continue;
			const int32 Sd = Id * 13 + I * 7;
			FKeysHumanLook Lk;
			Lk.Skin = Hex(Pick(FinSkins, Sd)); Lk.Shirt = Hex(Pick(FinShirts, Sd * 3 + 1)); Lk.Bottom = Hex(Pick(FinShorts, Sd + 2));
			Lk.bCap = Sd % 3 == 0; Lk.CapColor = Hex(Pick(FinShirts, Sd + 5)); Lk.bGlasses = Sd % 4 == 1; Lk.Hair = Hex(Pick(FinHair, Sd));
			Lk.bLongHair = Sd % 5 == 2; Lk.HairStyle = Sd % 4;
			FFan F;
			F.H.Init(Lk, false);
			F.Base = FVector(X, Y, Z);
			F.Role = I % 3;   // 0 phone, 1 cheer, 2 clap
			if (I % 4 == 2) F.Role = 0;
			F.NextFlash = FMath::FRand() * 2; F.FlashT = 0; F.bCore = I < 4;
			Fans.Add(F);
			I++;
		}
	ShownId = Id;
	bOnlyCore = false;
	bStation = true;
	bCrowdOn = true;
	HypeT = 0;
	CrowdMesh->SetVisibility(true);
	if (AKeysAudio* A = AKeysAudio::Get(this)) A->PlayExtra(TEXT("Cheer"), FVector::ZeroVector, 1.f);
	if (P) P->ToastMsg(FString::Printf(TEXT("Word got round the docks - a crowd is gathering to see your %.1f lb %s!"), Weight, Sp ? *Sp->Name : *Key), 6);
}

void AKeysFinal::OnLeaveShore()
{
	bCrowdOn = false;
	CrowdMesh->SetVisibility(false);
	FlashMesh->SetVisibility(false);
}

void AKeysFinal::TickCrowd(double Dt, double T)
{
	if (!bCrowdOn || !Fans.Num()) return;
	HypeT += Dt;
	const double Hype = FMath::Max(0.0, 1 - HypeT / HypeS);
	FKeysMesh M, G;
	for (FFan& F : Fans)
	{
		if (bOnlyCore && !F.bCore) continue;
		FKeysHuman& H = F.H;
		H.IdleUpdate(T, 0);
		H.Yaw = FMath::Atan2(StationPos.Z - F.Base.Z, StationPos.X - F.Base.X);
		H.PoseLegs(FKeysHuman::J(-.105, .085, 0), FKeysHuman::J(.105, .085, 0));
		const double HY = H.HipY, Ph = H.Ph;
		double Bounce = 0;
		if (F.Role == 0)
		{
			// phone up at eye height, pointed at the fish
			H.Pose(FKeysHuman::J(-.22, HY + .05, .1), FKeysHuman::J(.08, HY + .62, .36));
			F.NextFlash -= Dt;
			if (F.NextFlash <= 0) { F.FlashT = .09; F.NextFlash = (.6 + FMath::FRand() * 2.5) * (Hype > 0 ? 1 : 2.5); }
			F.FlashT -= Dt;
		}
		else if (F.Role == 1 && Hype > 0)
		{
			const double Wv = FMath::Sin(T * 7 + Ph);
			H.Pose(FKeysHuman::J(-.2 - Wv * .04, HY + 1.0, .05), FKeysHuman::J(.2 + Wv * .04, HY + 1.0, .05));
			Bounce = FMath::Abs(FMath::Sin(T * 5.5 + Ph)) * .09 * Hype;
		}
		else
		{
			const double Cl = FMath::Abs(FMath::Sin(T * (Hype > 0 ? 9 : 4) + Ph)), Gap = .03 + Cl * .12;
			H.Pose(FKeysHuman::J(-Gap, HY + .42, .3), FKeysHuman::J(Gap, HY + .42, .3));
		}
		H.HeadNod = -.12 - (FishCenter.Y - F.Base.Y > 2 ? .12 : 0);
		H.bHoldHead = true;
		H.HeadYaw *= .3;
		H.Pos = GM2U(FVector(F.Base.X, F.Base.Y + Bounce, F.Base.Z));
		H.Build(M);
		if (F.Role == 0)
		{
			const FTransform Xf = H.ToBoat();
			const FVector Ph0 = Xf.TransformPosition(H.HandR + FVector(0, 0, .06));
			M.AddBox(Ph0, FVector(.012, .075, .15), Hex(0x15171c), Xf.GetRotation());
			if (F.FlashT > 0) G.AddGlowBlob(Ph0 + Xf.GetRotation().RotateVector(FVector(.03, 0, .02)), .12, FLinearColor(1, 1, 1, 1));
		}
	}
	M.Commit(CrowdMesh, 0, AKeysWorld::VertexColorMaterial());
	if (G.V.Num()) { G.Commit(FlashMesh, 0, FlashMID ? (UMaterialInterface*)FlashMID : AKeysWorld::GlowMaterial()); FlashMesh->SetVisibility(true); }
	else FlashMesh->SetVisibility(false);
}

// ================================================================ the wreck chart's memory (wreck-chart.ts)
void AKeysFinal::TickWrecks()
{
	AKeysBoatPawn* P = Cast<AKeysBoatPawn>(UGameplayStatics::GetPlayerPawn(this, 0));
	if (!P || !P->bRunning) return;
	double X = P->BoatX(), Z = P->BoatZ();
	if (const AKeysDiver* D = AKeysDiver::Get(this); D && D->IsDiving()) { X = D->State().X; Z = D->State().Z; }
	for (const KeysWrecks::FWreck& W : KeysWrecks::Wrecks())
	{
		if (P->WrecksFound.Contains(W.Id) || KeysWrecks::EdgeDist(W, X, Z) > WreckChartM) continue;
		P->WrecksFound.Add(W.Id);
		P->SaveEconomy();
		P->ToastMsg(FString::Printf(TEXT("Wreck found: %s, in %d m. The sounder marked it - it's on your chart as found."), W.Name, FMath::RoundToInt(W.SeaDepth)), 5);
	}
}

// ================================================================ the underwater portrait (underwater-trophy.ts)
void AKeysFinal::ShowPortrait(const FString& Key, double Weight, bool bUnderwater)
{
	bPortrait = true;
	bPortraitUw = bUnderwater;
	PortraitKey = Key;
	PortraitT = 0;
	PortraitLen = FMath::Clamp(KeysMesh::FishLength(Weight), .35, 4.5);
	bPortraitHung = !bUnderwater && (PortraitLen > 1.5 || Weight >= 25);
	if (!bPortraitHung) PortraitLen = FMath::Min(PortraitLen, 2.4);
	FKeysHumanLook L;
	if (bUnderwater) { L.Skin = Hex(0xd9a27f); L.Shirt = Hex(0x14181d); L.Bottom = Hex(0x14181d); L.Hair = Hex(0x3b2a1e); }
	else { L.Skin = Hex(0xd9a27f); L.Shirt = Hex(0x9cc8e0); L.Bottom = Hex(0xb9a77c); L.bShortsLong = true; L.bCap = true; L.CapColor = Hex(0x1d3557); L.bGlasses = true; L.bShoes = true; L.Shoes = Hex(0xf2f2f2); }
	Diver.Init(L, false);
	Capture->PostProcessSettings.SceneColorTint = bUnderwater ? FLinearColor(.8f, .97f, 1.f) : FLinearColor::White;
	Capture->PostProcessSettings.AutoExposureBias = bUnderwater ? 3.f : 4.3f;
	StudioKey->SetLightColor(bUnderwater ? FLinearColor(.75f, .95f, 1.f) : FLinearColor(1.f, .95f, .85f));
	StudioRim->SetLightColor(bUnderwater ? FLinearColor(.45f, .9f, .85f) : FLinearColor(.75f, .85f, 1.f));
	StudioKey->SetVisibility(true);
	StudioRim->SetVisibility(true);
}

void AKeysFinal::HidePortrait()
{
	bPortrait = false;
	StudioMesh->SetVisibility(false);
	StudioKey->SetVisibility(false);
	StudioRim->SetVisibility(false);
}

void AKeysFinal::TickPortrait(double Dt, double T)
{
	if (!bPortrait) return;
	PortraitT += Dt;
	const FVector O = StudioO / 100.0;   // meters, UE axes
	FKeysMesh M, Bg;
	// the water behind and the sand below: a deep blue-green gradient, brighter toward the surface
	{
		const double BX = 6, BW = 14, BZ0 = -6, BZ1 = 9;
		const int32 NB = 24;
		const bool bUw = bPortraitUw;
		// under water a deep blue-green brightening toward the surface; on deck the sea to the horizon, then the sky
		auto Col = [bUw](double U)
		{
			if (bUw) return U < .5 ? FMath::Lerp(Hex(0x0a3a44), Hex(0x2a8c96), float(U * 2)) : FMath::Lerp(Hex(0x2a8c96), Hex(0x9fe8e0), float((U - .5) * 2));
			return U < .38 ? FMath::Lerp(Hex(0x135a86), Hex(0x2f8fbf), float(U / .38)) : FMath::Lerp(Hex(0xdcefff), Hex(0x6fb4e6), float((U - .38) / .62));
		};
		for (int32 i = 0; i < NB; i++)
		{
			const double Z0 = FMath::Lerp(BZ0, BZ1, double(i) / NB), Z1 = FMath::Lerp(BZ0, BZ1, double(i + 1) / NB);
			if (bUw) M.AddQuad(O + FVector(BX, -BW, Z0), O + FVector(BX, BW, Z0), O + FVector(BX, BW, Z1), O + FVector(BX, -BW, Z1), Col((i + .5) / NB), FVector(-1, 0, 0));
			else
			{
				// on deck the sea and sky glow on their own (the sun can be anywhere behind the set): the glow material's
				// solid mode, UV fixed at the middle of its soft blob
				const int32 V0 = Bg.V.Num();
				Bg.AddQuad(O + FVector(BX, -BW, Z0), O + FVector(BX, BW, Z0), O + FVector(BX, BW, Z1), O + FVector(BX, -BW, Z1), Col((i + .5) / NB), FVector(-1, 0, 0));
				for (int32 v = V0; v < Bg.V.Num(); v++) Bg.UV[v] = FVector2D(2.5, .5);
			}
		}
		M.AddQuad(O + FVector(-4, -14, -2.2), O + FVector(-4, 14, -2.2), O + FVector(6, 14, -2.2), O + FVector(6, -14, -2.2), bUw ? Hex(0x8a8466) : Hex(0xeeeae0), FVector::ZAxisVector);
		if (!bUw)
		{
			// the gunwale and the rail behind the angler
			M.AddBox(O + FVector(2.2, 0, -1.7), FVector(.4, 12, 1.0), Hex(0xf6f4ee));
			M.AddTube(O + FVector(2.1, -6, -.5), O + FVector(2.1, 6, -.5), .025, .025, 6, Hex(0xd9dde2));
			for (double Yp = -5; Yp <= 5; Yp += 2.5) M.AddTube(O + FVector(2.1, Yp, -1.2), O + FVector(2.1, Yp, -.5), .02, .02, 6, Hex(0xd9dde2));
		}
		// a few soft shafts of light
		for (int32 k = 0; k < (bUw ? 4 : 0); k++)
		{
			const double Y0 = -3 + k * 2.1 + FMath::Sin(PortraitT * .3 + k) * .3;
			M.AddQuad(O + FVector(5.8, Y0 - .25, 8), O + FVector(5.8, Y0 + .25, 8), O + FVector(5.8, Y0 + 1.3, -2), O + FVector(5.8, Y0 + .4, -2), Hex(0x8fe0dc), FVector(-1, 0, 0));
		}
	}
	// the diver, facing the lens, the fish held up across the chest
	const double HY = Diver.HipY, Half = FMath::Min(.42, PortraitLen * .3);
	Diver.Pos = O + FVector(.6, bPortraitHung ? .55 : 0, -2.2 + .05);
	Diver.Yaw = PI;
	Diver.Pitch = 0; Diver.Roll = FMath::Sin(PortraitT * .8) * .03;
	Diver.HeadNod = .05; Diver.HeadYaw = 0; Diver.bHoldHead = true;
	if (bPortraitUw) Diver.PoseLegs(FKeysHuman::J(-.12, .085, .05), FKeysHuman::J(.12, .15, -.12));
	else Diver.PoseLegs(FKeysHuman::J(-.12, .085, 0), FKeysHuman::J(.12, .085, 0));
	const double Bob = FMath::Sin(PortraitT * 1.3) * .03;
	if (bPortraitHung) Diver.Pose(FKeysHuman::J(-.5, HY + .55, .3), FKeysHuman::J(.24, HY + .95 + Bob, .2));
	else Diver.Pose(FKeysHuman::J(-Half, HY + .38 + Bob, .4), FKeysHuman::J(Half, HY + .38 + Bob, .4));
	Diver.Build(M);
	const FTransform Xf = Diver.ToBoat();
	// mask, strap and snorkel
	if (bPortraitUw)
	{
		const FVector HP = FKeysHuman::J(0, HY + .77, 0);
		const FQuat HQ = Xf.GetRotation();
		M.AddBox(Xf.TransformPosition(HP + FVector(.105, 0, .025)), FVector(.04, .17, .085), Hex(0x1d5f7a), HQ);
		M.AddBox(Xf.TransformPosition(HP + FVector(.125, 0, .025)), FVector(.01, .15, .065), Hex(0x8fd2e8), HQ);
		M.AddBox(Xf.TransformPosition(HP + FVector(0, 0, .03)), FVector(.2, .2, .025), Hex(0x111111), HQ);
		M.AddTube(Xf.TransformPosition(HP + FVector(.06, .1, -.06)), Xf.TransformPosition(HP + FVector(.02, .11, .25)), .014, .014, 6, Hex(0xf2c14e));
		// fins
		for (const FVector& Ft : {Diver.FootL, Diver.FootR}) M.AddBox(Xf.TransformPosition(Ft + FVector(.25, 0, -.04)), FVector(.55, .2, .02), Hex(0x1f6fd0), HQ);
	}
	// a big fish: hung by the tail from the gin pole beside the angler, nose down (catch-flow.ts's hang rig)
	if (bPortraitHung)
	{
		const FVector Pole = Xf.TransformPosition(FKeysHuman::J(-.6, 0, .25));
		const double Top = PortraitLen + 1.4;
		const FVector PTop = Pole + FVector(0, 0, Top), Arm = Xf.TransformPosition(FKeysHuman::J(-.6 - .5, 0, .25)) + FVector(0, 0, Top - .1);
		M.AddTube(Pole, PTop, .06, .05, 8, Hex(0xd9dde2));
		M.AddTube(PTop, Arm, .045, .045, 8, Hex(0xd9dde2));
		M.AddTube(Arm, Arm - FVector(0, 0, .35), .008, .008, 4, Hex(0x2a2a2a));
		KeysShapes::AddBlob(M, Arm - FVector(0, 0, .4), FVector(.05), FQuat::Identity, Hex(0xc8102e), 6, 4);
		const FVector Side = Xf.GetRotation().RotateVector(FVector(1, 0, 0));   // the flank to the lens
		const FQuat Q = FRotationMatrix::MakeFromXY(FVector(0, 0, -1), Side).ToQuat() * FQuat(FVector::XAxisVector, FMath::Sin(PortraitT * .7) * .08);
		AppendFish(M, PortraitKey, PortraitLen, FTransform(Q, Arm - FVector(0, 0, .45 + PortraitLen / 2)));
	}
	// the fish across the hands, its flank to the lens
	else
	{
		const FVector GL = Xf.TransformPosition(FKeysHuman::J(-Half, HY + .38 + Bob, .44)), GR = Xf.TransformPosition(FKeysHuman::J(Half, HY + .38 + Bob, .44));
		const FVector Axis = (GR - GL).GetSafeNormal(), Mid = (GL + GR) * .5 + FVector(0, 0, .02);
		const FQuat Q = FRotationMatrix::MakeFromXZ(Axis, FVector::ZAxisVector).ToQuat() * FQuat(FVector::XAxisVector, FMath::Sin(PortraitT * .9) * .12);
		AppendFish(M, PortraitKey, PortraitLen, FTransform(Q, Mid));
		// a few bubbles
		for (int32 k = 0; k < (bPortraitUw ? 6 : 0); k++)
		{
			const double Ph = FMath::Fmod(PortraitT * .35 + k * .17, 1.0);
			const FVector Bp = Xf.TransformPosition(FKeysHuman::J(.04 + k * .01, HY + .8 + Ph * 2.4, .18));
			KeysShapes::AddBlob(M, Bp, FVector(.018 + k * .003), FQuat::Identity, Hex(0xbfe9ef), 5, 3);
		}
	}
	M.Commit(StudioMesh, 0, AKeysWorld::VertexColorMaterial());
	if (Bg.V.Num()) Bg.Commit(StudioMesh, 1, BackdropMID ? (UMaterialInterface*)BackdropMID : AKeysWorld::GlowMaterial());
	else StudioMesh->ClearMeshSection(1);
	StudioMesh->SetVisibility(true);
	// the lens: framed on the fish and the diver's chest and head
	FVector Look = Xf.TransformPosition(FKeysHuman::J(0, HY + .55, .2));
	double Dist = FMath::Max(2.0, PortraitLen * 1.25 + 1.3);
	if (bPortraitHung)
	{
		// frame the angler and the whole fish on the pole
		Look = Xf.TransformPosition(FKeysHuman::J(-.5, 0, .3)) + FVector(0, 0, FMath::Max(1.1, (PortraitLen + 1.4) * .55));
		Dist = FMath::Max(3.2, (PortraitLen + 1.6) * 1.45);
	}
	const FVector Cam = Look + FVector(-Dist, FMath::Sin(PortraitT * .25) * .35, .25);
	Capture->SetWorldLocationAndRotation(Cam * 100, (Look - Cam).Rotation());
	StudioKey->SetWorldLocation((Cam + FVector(.4, -1.2, 1.6)) * 100);
	StudioKey->SetIntensity(9000.f);
	StudioRim->SetWorldLocation((Look + FVector(1.6, 1.4, 1.8)) * 100);
	StudioRim->SetIntensity(6000.f);
	Capture->CaptureScene();
}

// ================================================================ tick
void AKeysFinal::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	const double Dt = FMath::Min(double(DeltaSeconds), .1), T = GetWorld()->GetTimeSeconds();
	TickLegend(Dt, T);
	TickSpouts(Dt);
	TickCrowd(Dt, T);
	WreckCheckT -= Dt;
	if (WreckCheckT <= 0) { WreckCheckT = .5; TickWrecks(); }
	TickPortrait(Dt, T);
}
