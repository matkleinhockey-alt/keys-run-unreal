#include "KeysWeather.h"
#include "KeysGeo.h"
#include "KeysWorld.h"
#include "KeysBoatPawn.h"
#include "KeysAudio.h"
#include "KeysLife.h"
#include "ProceduralMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/VolumetricCloudComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Materials/Material.h"
#include "MaterialShared.h"
#include "MaterialDomain.h"
#include "Kismet/GameplayStatics.h"
#include "Camera/PlayerCameraManager.h"
#include "GameFramework/PlayerController.h"
#include "EngineUtils.h"
#include "Engine/World.h"
#include "Misc/DateTime.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"

using namespace KeysGeo;

namespace
{
	double WClamp(double V, double A, double B) { return V < A ? A : V > B ? B : V; }
	double WStep(double A, double B, double X) { const double T = WClamp((X - A) / (B - A), 0, 1); return T * T * (3 - 2 * T); }

	// compass words for a direction in the game plane (x east, z south)
	FString CompassWord(double DX, double DZ, bool bLong)
	{
		static const TCHAR* Short[8] = {TEXT("N"), TEXT("NE"), TEXT("E"), TEXT("SE"), TEXT("S"), TEXT("SW"), TEXT("W"), TEXT("NW")};
		static const TCHAR* Long[8] = {TEXT("north"), TEXT("northeast"), TEXT("east"), TEXT("southeast"), TEXT("south"), TEXT("southwest"), TEXT("west"), TEXT("northwest")};
		double B = FMath::RadiansToDegrees(FMath::Atan2(DX, -DZ));
		if (B < 0) B += 360;
		const int32 I = int32(FMath::RoundToDouble(B / 45)) % 8;
		return bLong ? Long[I] : Short[I];
	}
}

namespace KeysWx
{
	// the rain columns under the storm cells: x, z, radius, strength
	static TArray<FVector4> GRain;
	static TArray<FCellView> GCells;

	double RainAt(double X, double Z)
	{
		double R = 0;
		for (const FVector4& C : GRain)
		{
			const double D2 = (FMath::Square(X - C.X) + FMath::Square(Z - C.Y)) / FMath::Square(FMath::Max(1.0, C.Z));
			if (D2 < 1) R = FMath::Max(R, C.W * (1 - D2) * (1 - D2) * 1.25);
		}
		return FMath::Min(1.0, R);
	}

	void Cells(TArray<FCellView>& Out) { Out = GCells; }

	FString HudLine(double X, double Z)
	{
		const double Wd = WindDir();
		FString S = FString::Printf(TEXT("wind %.0f kn %s"), WindSpeed() * 1.944, *CompassWord(-FMath::Cos(Wd), -FMath::Sin(Wd), false));
		const FCellView* Best = nullptr;
		double BestD = 1e18;
		for (const FCellView& C : GCells)
		{
			if (C.Build < .3) continue;
			const double D = FMath::Sqrt(FMath::Square(C.X - X) + FMath::Square(C.Z - Z)) - C.R;
			if (D < BestD) { BestD = D; Best = &C; }
		}
		if (Best)
		{
			if (BestD < 0 && Best->Rain > .05) S += TEXT("  -  in the storm");
			else S += FString::Printf(TEXT("  -  storm %.1f nm %s"), FMath::Max(0.0, BestD) / 1852, *CompassWord(Best->X - X, Best->Z - Z, false));
		}
		return S;
	}
}

AKeysWeather::AKeysWeather()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickGroup = TG_PrePhysics;   // before the boat and the sea read the wind
	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent = Root;
}

AKeysWeather* AKeysWeather::Get(const UObject* WorldContext)
{
	UWorld* W = WorldContext ? WorldContext->GetWorld() : nullptr;
	if (!W) return nullptr;
	TActorIterator<AKeysWeather> It(W);
	return It ? *It : nullptr;
}

double AKeysWeather::Clock() const { return GetWorld()->GetTimeSeconds() + Skip; }

void AKeysWeather::BeginPlay()
{
	Super::BeginPlay();
	// seeded from the real clock, so every session's sky differs (-KRWxSeed=N repeats one)
	int32 Seed = int32(FDateTime::Now().GetTicks() % 2147483647) ^ 0x2f6b1d;
	FParse::Value(FCommandLine::Get(), TEXT("KRWxSeed="), Seed);
	Rng.Initialize(Seed);
	// summer storms over the Keys drift mostly westward on the trades, 7 to 14 knots
	SteerDir = -2.75 + Rng.FRandRange(-.6f, .6f);
	SteerSpd = Rng.FRandRange(3.5f, 7.f);
	Schedule(true);

	UMaterialInterface* RainM = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/KeysRun/Materials/M_KR_Rain.M_KR_Rain"));
	if (!RainM) RainM = AKeysWorld::GlowMaterial();
	auto Make = [this](const TCHAR* Name)
	{
		UProceduralMeshComponent* M = NewObject<UProceduralMeshComponent>(this, Name);
		M->SetupAttachment(Root);
		M->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		M->SetCastShadow(false);
		M->RegisterComponent();
		M->SetVisibility(false);
		return M;
	};
	auto Mid = [this, RainM](double Mode, const FLinearColor& Tint, double Bright, double Fade)
	{
		UMaterialInstanceDynamic* D = RainM ? UMaterialInstanceDynamic::Create(RainM, this) : nullptr;
		if (D)
		{
			D->SetScalarParameterValue(TEXT("Mode"), float(Mode));
			D->SetVectorParameterValue(TEXT("Tint"), Tint);
			D->SetScalarParameterValue(TEXT("Bright"), float(Bright));
			D->SetScalarParameterValue(TEXT("Fade"), float(Fade));
			D->SetScalarParameterValue(TEXT("Amount"), 0.f);
		}
		return D;
	};
	for (int32 i = 0; i < 4; i++)
	{
		UProceduralMeshComponent* S = Make(*FString::Printf(TEXT("Shaft%d"), i));
		BuildUnitCone(S, 40, 1, 1.12, .95, .3);
		UMaterialInstanceDynamic* D = Mid(1, FLinearColor(.3f, .33f, .37f), .5, 4000);
		S->SetMaterial(0, D);
		Shafts.Add(S);
		ShaftMIDs.Add(D);
	}
	RainCyl = Make(TEXT("RainNear"));
	BuildRainCyl();
	RainMID = Mid(0, FLinearColor(.75f, .78f, .82f), .5, 40);
	RainCyl->SetMaterial(0, RainMID);
	Spout = Make(TEXT("Spout"));
	SpoutMID = Mid(2, FLinearColor(.36f, .38f, .42f), .55, 600);
	Skirt = Make(TEXT("SpoutSpray"));
	BuildUnitCone(Skirt, 32, 1, 3.2, .85, 0);
	SkirtMID = Mid(1, FLinearColor(.85f, .88f, .9f), .75, 300);
	Skirt->SetMaterial(0, SkirtMID);
	BoltMesh = Make(TEXT("Bolt"));
	BoltMID = Mid(3, FLinearColor(.8f, .82f, 1.f), 9, 10);
	BoltLight = NewObject<UPointLightComponent>(this, TEXT("BoltLight"));
	BoltLight->SetupAttachment(Root);
	BoltLight->RegisterComponent();
	BoltLight->SetIntensityUnits(ELightUnits::Candelas);
	BoltLight->SetIntensity(0.f);
	BoltLight->SetAttenuationRadius(1200000.f);
	BoltLight->SetCastShadows(false);
	BoltLight->SetLightColor(FLinearColor(.78f, .8f, 1.f));
	BoltLight->SetVisibility(false);
}

// ---------------------------------------------------------------- meshes
// a unit cone on its side: radius R0 at the bottom (z 0) to R1 at the top (z 1), alpha A0 at the bottom to A1 at the top
void AKeysWeather::BuildUnitCone(UProceduralMeshComponent* M, int32 Seg, double R0, double R1, double A0, double A1)
{
	TArray<FVector> V; TArray<int32> T; TArray<FVector> N; TArray<FVector2D> UV; TArray<FLinearColor> C;
	constexpr int32 Rows = 6;
	for (int32 j = 0; j <= Rows; j++)
	{
		const double Vv = double(j) / Rows, R = FMath::Lerp(R0, R1, Vv);
		for (int32 i = 0; i <= Seg; i++)
		{
			const double A = 2 * PI * i / Seg;
			V.Add(FVector(FMath::Cos(A) * R, FMath::Sin(A) * R, Vv));
			N.Add(FVector(FMath::Cos(A), FMath::Sin(A), 0));
			UV.Add(FVector2D(double(i) / Seg * 6, Vv));
			C.Add(FLinearColor(1, 1, 1, float(FMath::Lerp(A0, A1, Vv))));
		}
	}
	for (int32 j = 0; j < Rows; j++)
		for (int32 i = 0; i < Seg; i++)
		{
			const int32 K = j * (Seg + 1) + i;
			T.Append({K, K + 1, K + Seg + 1, K + 1, K + Seg + 2, K + Seg + 1});
		}
	M->CreateMeshSection_LinearColor(0, V, T, N, UV, C, TArray<FProcMeshTangent>(), false);
	M->SetBoundsScale(1.5f);
}

// three nested cylinders of falling streaks around the camera (UV in meters: around, up), fainter further out
void AKeysWeather::BuildRainCyl()
{
	TArray<FVector> V; TArray<int32> T; TArray<FVector> N; TArray<FVector2D> UV; TArray<FLinearColor> C;
	const double Rad[3] = {2.6, 6.5, 14}, Al[3] = {.9, .75, .55};
	for (int32 c = 0; c < 3; c++)
	{
		const int32 Seg = 28, B = V.Num();
		for (int32 j = 0; j <= 1; j++)
			for (int32 i = 0; i <= Seg; i++)
			{
				const double A = 2 * PI * i / Seg, Y = j ? 16.0 : -7.0;
				V.Add(FVector(FMath::Cos(A) * Rad[c], FMath::Sin(A) * Rad[c], Y) * 100);
				N.Add(FVector(FMath::Cos(A), FMath::Sin(A), 0));
				UV.Add(FVector2D(A * Rad[c] + c * 13.7, Y + c * 5.3));
				C.Add(FLinearColor(1, 1, 1, float(Al[c])));
			}
		for (int32 i = 0; i < Seg; i++) { const int32 K = B + i; T.Append({K, K + 1, K + Seg + 1, K + 1, K + Seg + 2, K + Seg + 1}); }
	}
	RainCyl->CreateMeshSection_LinearColor(0, V, T, N, UV, C, TArray<FProcMeshTangent>(), false);
}

void AKeysWeather::BuildSpout(const FCell& C, double T)
{
	// a funnel from the cloud base down to the sea, leaning with the cell's drift, twisting, roping out as it dies
	const double Age = Clock() - C.SBorn, K = WStep(0, 25, Age) * (1 - WStep(C.SLife - 40, C.SLife, Age));
	const double Rope = WStep(C.SLife * .6, C.SLife, Age);
	const double Sp = FMath::Max(.1, FMath::Sqrt(C.VX * C.VX + C.VZ * C.VZ));
	const double LX = C.VX / Sp * 90, LZ = C.VZ / Sp * 90;
	TArray<FVector> V; TArray<int32> Tr; TArray<FVector> N; TArray<FVector2D> UV; TArray<FLinearColor> Cl;
	constexpr int32 Rows = 30, Seg = 18;
	const double Top = 640;
	for (int32 j = 0; j <= Rows; j++)
	{
		const double Vv = double(j) / Rows;
		// a snaking axis: bowed against the drift low down, wobbling
		const double Bow = FMath::Sin(Vv * PI) * (1 - Rope * .5);
		const double AX = C.SX + LX * Vv - LX * .6 * Bow + FMath::Sin(Vv * 5 + T * .6 + C.Seed) * 14 * (1 - Vv) * (1 + Rope * 2);
		const double AZ = C.SZ + LZ * Vv - LZ * .6 * Bow + FMath::Cos(Vv * 4 + T * .5 + C.Seed) * 14 * (1 - Vv) * (1 + Rope * 2);
		double R = FMath::Lerp(7.0, 42.0, FMath::Pow(Vv, 2.0)) + 110 * WStep(.8, 1, Vv);
		R *= (1 - .55 * Rope) * (.4 + .6 * K);
		// it condenses from the top down as it forms
		const double Vis = WStep(1 - K * 1.15, 1.05 - K * 1.15, Vv);
		const float Alpha = float((.82 - .5 * WStep(.85, 1, Vv)) * Vis);
		for (int32 i = 0; i <= Seg; i++)
		{
			const double A = 2 * PI * i / Seg;
			V.Add(ToUE(AX + FMath::Cos(A) * R, Vv * Top, AZ + FMath::Sin(A) * R));
			N.Add(FVector(FMath::Cos(A), FMath::Sin(A), 0));
			UV.Add(FVector2D(double(i) / Seg * 3, Vv));
			Cl.Add(FLinearColor(1, 1, 1, Alpha));
		}
	}
	for (int32 j = 0; j < Rows; j++)
		for (int32 i = 0; i < Seg; i++)
		{
			const int32 Kk = j * (Seg + 1) + i;
			Tr.Append({Kk, Kk + 1, Kk + Seg + 1, Kk + 1, Kk + Seg + 2, Kk + Seg + 1});
		}
	Spout->CreateMeshSection_LinearColor(0, V, Tr, N, UV, Cl, TArray<FProcMeshTangent>(), false);
	Spout->SetMaterial(0, SpoutMID);
	Spout->SetVisibility(true);
	if (SpoutMID) SpoutMID->SetScalarParameterValue(TEXT("Amount"), float(K));
	// the spray vortex where it touches the water
	Skirt->SetWorldLocation(ToUE(C.SX, -1, C.SZ));
	Skirt->SetWorldScale3D(FVector(20 * 100 * (.5 + .5 * K), 20 * 100 * (.5 + .5 * K), 45 * 100));
	Skirt->SetVisibility(K > .05);
	if (SkirtMID) SkirtMID->SetScalarParameterValue(TEXT("Amount"), float(K * (1 - Rope * .6)));
}

void AKeysWeather::BuildBolt(const FVector& TopG, const FVector& BotG)
{
	// a jagged leader from the cloud base to the water with a few forks, as camera-facing ribbons (a bright core and a glow)
	FVector Cam;
	if (!CameraPos(Cam)) Cam = BotG + FVector(0, 0, 2000);
	TArray<FVector> V; TArray<int32> T; TArray<FVector> N; TArray<FVector2D> UV; TArray<FLinearColor> C;
	const double Dist = FVector::Dist(Cam, (TopG + BotG) * .5);
	const double W = FMath::Max(2.5, Dist * .0022);
	auto Ribbon = [&](const TArray<FVector>& P, double Wd, float A)
	{
		for (int32 i = 1; i < P.Num(); i++)
		{
			const FVector A0 = ToUE(P[i - 1].X, P[i - 1].Y, P[i - 1].Z), B0 = ToUE(P[i].X, P[i].Y, P[i].Z);
			const FVector Cu = ToUE(Cam.X, Cam.Y, Cam.Z);
			FVector Sd = FVector::CrossProduct(B0 - A0, Cu - A0).GetSafeNormal() * Wd * 50;
			const int32 Bs = V.Num();
			V.Append({A0 - Sd, A0 + Sd, B0 - Sd, B0 + Sd});
			const FVector Nn = (Cu - A0).GetSafeNormal();
			N.Append({Nn, Nn, Nn, Nn});
			UV.Append({FVector2D(0, 0), FVector2D(0, 1), FVector2D(1, 0), FVector2D(1, 1)});
			C.Append({FLinearColor(1, 1, 1, A), FLinearColor(1, 1, 1, A), FLinearColor(1, 1, 1, A), FLinearColor(1, 1, 1, A)});
			T.Append({Bs, Bs + 2, Bs + 1, Bs + 1, Bs + 2, Bs + 3});
		}
	};
	TArray<FVector> Main;
	constexpr int32 NSeg = 22;
	FVector P = TopG;
	Main.Add(P);
	const double Len = (TopG - BotG).Size();
	for (int32 i = 1; i <= NSeg; i++)
	{
		const double F = double(i) / NSeg;
		FVector Q = FMath::Lerp(TopG, BotG, F);
		if (i < NSeg) Q += FVector(Rng.FRandRange(-1, 1), 0, Rng.FRandRange(-1, 1)) * Len * .045 + FVector(0, Rng.FRandRange(-1, 1) * Len * .012, 0);
		Main.Add(Q);
	}
	Ribbon(Main, W, 1.f);
	Ribbon(Main, W * 7, .22f);
	for (int32 b = 0; b < 3; b++)
	{
		const int32 From = Rng.RandRange(3, NSeg - 6);
		TArray<FVector> Br;
		FVector Q = Main[From];
		Br.Add(Q);
		const FVector Dir = FVector(Rng.FRandRange(-1, 1), -1.3, Rng.FRandRange(-1, 1)).GetSafeNormal();
		for (int32 k = 0; k < 6; k++)
		{
			Q += Dir * Len * .04 + FVector(Rng.FRandRange(-1, 1), 0, Rng.FRandRange(-1, 1)) * Len * .02;
			Br.Add(Q);
		}
		Ribbon(Br, W * .55, .6f);
	}
	BoltMesh->CreateMeshSection_LinearColor(0, V, T, N, UV, C, TArray<FProcMeshTangent>(), false);
	BoltMesh->SetMaterial(0, BoltMID);
}

// ---------------------------------------------------------------- where things are
bool AKeysWeather::PlayerPos(double& OX, double& OZ) const
{
	if (const AKeysBoatPawn* P = Cast<AKeysBoatPawn>(UGameplayStatics::GetPlayerPawn(this, 0))) { OX = P->BoatX(); OZ = P->BoatZ(); return true; }
	OX = OZ = 0;
	return false;
}

bool AKeysWeather::CameraPos(FVector& OutGame) const
{
	const APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0);
	if (!PC || !PC->PlayerCameraManager) return false;
	const FVector L = PC->PlayerCameraManager->GetCameraLocation() / 100;
	OutGame = FVector(L.X, L.Z, L.Y);
	return true;
}

// ---------------------------------------------------------------- the storm cells
void AKeysWeather::Schedule(bool bFirst)
{
	const double Now = Clock();
	if (bFirst) NextSpawn = Now + Rng.FRandRange(90.f, 420.f);
	// now and then a long calm stretch; otherwise the next cell pops up within minutes
	else NextSpawn = Now + (Rng.FRand() < .3f ? Rng.FRandRange(900.f, 1800.f) : Rng.FRandRange(240.f, 700.f));
}

AKeysWeather::FCell& AKeysWeather::SpawnCell(double X, double Z, double AgeFrac, bool bBig)
{
	FCell& C = Cells.AddDefaulted_GetRef();
	C.Id = NextId++;
	C.X = X; C.Z = Z;
	const double A = SteerDir + Rng.FRandRange(-.3f, .3f), S = SteerSpd * Rng.FRandRange(.8f, 1.2f);
	C.VX = FMath::Cos(A) * S; C.VZ = FMath::Sin(A) * S;
	C.Life = Rng.FRandRange(600.f, 1500.f);
	C.Born = Clock() - AgeFrac * C.Life;
	C.bBig = bBig;
	C.R = bBig ? Rng.FRandRange(1500.f, 2300.f) : Rng.FRandRange(850.f, 1800.f);
	C.Power = bBig ? Rng.FRandRange(1.f, 1.25f) : Rng.FRandRange(.7f, 1.1f);
	C.Seed = Rng.FRandRange(0.f, 100.f);
	C.NextBolt = Clock() + Rng.FRandRange(5.f, 30.f);
	return C;
}

void AKeysWeather::UpdateCells(double Dt)
{
	const double Now = Clock();
	double PX, PZ;
	const bool bHave = PlayerPos(PX, PZ);
	if (bHave && Now >= NextSpawn)
	{
		if (Cells.Num() < 4)
		{
			// upwind of you (they drift your way, some pass over, some die out first), anywhere across the wind
			const double UX = -FMath::Cos(SteerDir), UZ = -FMath::Sin(SteerDir);
			const double Up = Rng.FRandRange(3000.f, 9000.f), Across = Rng.FRandRange(-6500.f, 6500.f);
			const double X = PX + UX * Up - UZ * Across, Z = PZ + UZ * Up + UX * Across;
			SpawnCell(X, Z, 0, Rng.FRand() < .35f);
			// sometimes two at once
			if (Cells.Num() < 4 && Rng.FRand() < .25f)
			{
				const double Ang = Rng.FRandRange(0.f, 2 * PI), D = Rng.FRandRange(2800.f, 4500.f);
				SpawnCell(X + FMath::Cos(Ang) * D, Z + FMath::Sin(Ang) * D, Rng.FRandRange(0.f, .15f), Rng.FRand() < .3f);
			}
		}
		Schedule(false);
	}

	for (int32 i = Cells.Num() - 1; i >= 0; i--)
	{
		FCell& C = Cells[i];
		const double A = (Now - C.Born) / C.Life;
		if (A >= 1) { Cells.RemoveAt(i); continue; }
		C.X += C.VX * Dt; C.Z += C.VZ * Dt;
		// build (cumulus congestus to a towering cell) - mature (rain, lightning) - dissipate (rain out, anvil left behind)
		C.Build = WStep(0, .35, A) * (1 - .45 * WStep(.75, 1, A));
		C.Cloud = WStep(0, .3, A) * (1 - WStep(.84, 1, A));
		C.Rain = WStep(.2, .38, A) * (1 - WStep(.62, .95, A)) * WClamp(C.Power, .6, 1);
		C.Elec = WStep(.28, .4, A) * (1 - WStep(.6, .8, A)) * C.Power;
		// a big cell at its peak may drop a waterspout (rarely)
		if (C.bBig && !C.bSpoutTried && A > .36)
		{
			C.bSpoutTried = true;
			if (Rng.FRand() < .5f)
			{
				const double Sp = FMath::Max(.1, FMath::Sqrt(C.VX * C.VX + C.VZ * C.VZ));
				const double Ang = Rng.FRandRange(-.8f, .8f), Ca = FMath::Cos(Ang), Sa = FMath::Sin(Ang);
				const double DX = (C.VX * Ca - C.VZ * Sa) / Sp, DZ = (C.VX * Sa + C.VZ * Ca) / Sp;
				C.bSpout = true;
				C.SX = C.X + DX * C.R * .45; C.SZ = C.Z + DZ * C.R * .45;
				C.SBorn = Now; C.SLife = Rng.FRandRange(120.f, 300.f);
			}
		}
		if (C.bSpout)
		{
			const double SA = Now - C.SBorn;
			if (SA > C.SLife || A > .8) C.bSpout = false;
			else
			{
				// it drifts with the cell, wandering
				C.SVX += (Rng.FRandRange(-1, 1) * .8 - C.SVX * .05) * Dt;
				C.SVZ += (Rng.FRandRange(-1, 1) * .8 - C.SVZ * .05) * Dt;
				C.SX += (C.VX + C.SVX) * Dt; C.SZ += (C.VZ + C.SVZ) * Dt;
			}
		}
		if (bHave)
		{
			const double D = FMath::Sqrt(FMath::Square(C.X - PX) + FMath::Square(C.Z - PZ));
			const double Closing = -((C.X - PX) * C.VX + (C.Z - PZ) * C.VZ) / FMath::Max(1.0, D);
			const FString Where = CompassWord(C.X - PX, C.Z - PZ, true);
			const FString Going = CompassWord(C.VX, C.VZ, true);
			if (!C.bWarned && C.Build > .3 && D < 15000)
			{
				C.bWarned = true;
				Radio(FString::Printf(TEXT("Storm cell building %.0f miles to the %s, drifting %s near %.0f knots."), FMath::Max(1.0, D / 1609), *Where, *Going, FMath::Sqrt(C.VX * C.VX + C.VZ * C.VZ) * 1.944));
			}
			if (!C.bOverhead && C.Rain > .3 && D - C.R < 2500 && (Closing > .5 || D < C.R))
			{
				C.bOverhead = true;
				Radio(FString::Printf(TEXT("Heavy rain and lightning %s from the %s - gusts over 35 knots. Seek shelter."), D < C.R ? TEXT("on top of you") : TEXT("closing in"), *Where));
			}
			if (C.bSpout && !C.bSpoutWarned)
			{
				C.bSpoutWarned = true;
				const double SD = FMath::Sqrt(FMath::Square(C.SX - PX) + FMath::Square(C.SZ - PZ));
				if (SD < 12000) Radio(FString::Printf(TEXT("Waterspout reported %.1f miles %s of you. Stay well clear."), SD / 1609, *CompassWord(C.SX - PX, C.SZ - PZ, true)));
			}
		}
	}

	// what everyone else reads
	KeysWx::GRain.Reset();
	KeysWx::GCells.Reset();
	for (const FCell& C : Cells)
	{
		const double R = C.R * (.55 + .45 * C.Build);
		if (C.Rain > .01) KeysWx::GRain.Add(FVector4(C.X, C.Z, R, C.Rain));
		KeysWx::GCells.Add({C.X, C.Z, R, C.Rain, C.Build, C.Seed, C.bSpout, C.SX, C.SZ});
	}
}

void AKeysWeather::UpdateWind(double Dt)
{
	double PX, PZ;
	if (!PlayerPos(PX, PZ)) return;
	const double Now = Clock();
	// gusts: the outflow comes in surges
	GustT -= Dt;
	if (GustT <= 0) { GustT = Rng.FRandRange(2.f, 6.f); GustTarget = Rng.FRandRange(.6f, 1.4f); }
	Gust += (GustTarget - Gust) * FMath::Min(1.0, Dt * .8);
	double VX = 0, VZ = 0, Conf = 0, SwellAdd = 0;
	for (const FCell& C : Cells)
	{
		const double R = C.R * (.55 + .45 * C.Build);
		const double DX = PX - C.X, DZ = PZ - C.Z, D = FMath::Max(1.0, FMath::Sqrt(DX * DX + DZ * DZ)), Q = D / R;
		// the downdraft spreads out from under the cell: strongest at the rain's edge (the gust front), felt well ahead of it
		const double Prof = WStep(2.6, .9, Q) * (Q < .4 ? FMath::Lerp(.65, 1.0, Q / .4) : 1.0);
		const double S = 16 * C.Power * FMath::Max(C.Rain, C.Build * .35) * Prof;
		if (S < .01) continue;
		// the outflow swings as the cell passes
		const double Sw = .5 * FMath::Sin(Now * .045 + C.Seed);
		const double UX = DX / D, UZ = DZ / D, Cs = FMath::Cos(Sw), Sn = FMath::Sin(Sw);
		VX += (UX * Cs - UZ * Sn) * S;
		VZ += (UX * Sn + UZ * Cs) * S;
		Conf = FMath::Max(Conf, C.Rain * WStep(2.0, .7, Q));
		SwellAdd = FMath::Max(SwellAdd, .45 * C.Power * C.Build * WStep(3, 1, Q));
	}
	const double Spd = FMath::Sqrt(VX * VX + VZ * VZ) * Gust;
	SetStormWind(Spd, Spd > .01 ? FMath::Atan2(VZ, VX) : 0, Conf, SwellAdd);

	// blowing spray in a hard blow: sheets torn off the crests around the boat
	const double W = WindSpeed();
	if (W > 13)
		if (AKeysLife* L = AKeysLife::Get(this))
		{
			SprayT += Dt * (W - 13) * .7;
			while (SprayT > 1)
			{
				SprayT -= 1;
				const double Wd = WindDir(), Up = Rng.FRandRange(6.f, 30.f), Ac = Rng.FRandRange(-18.f, 18.f);
				L->Splash(PX - FMath::Cos(Wd) * Up - FMath::Sin(Wd) * Ac, PZ - FMath::Sin(Wd) * Up + FMath::Cos(Wd) * Ac, 5, .7);
			}
		}
}

// ---------------------------------------------------------------- lightning and thunder
void AKeysWeather::Bolt(double X, double Z, bool bToWater)
{
	const double Now = Clock();
	FlashPos = FVector(X, bToWater ? 350 : Rng.FRandRange(2500.f, 6000.f), Z);
	// a flash is a few strokes in under half a second
	Strokes.Reset();
	const int32 N = Rng.RandRange(2, 4);
	double T = 0;
	for (int32 i = 0; i < N; i++) { Strokes.Add(FVector2D(Now + T, i == 0 ? 1.0 : Rng.FRandRange(.4f, .9f))); T += Rng.FRandRange(.05f, .14f); }
	BoltT = Now;
	if (bToWater)
	{
		BuildBolt(FVector(X + Rng.FRandRange(-250.f, 250.f), 1100, Z + Rng.FRandRange(-250.f, 250.f)), FVector(X, 0, Z));
		BoltShowUntil = Now + T + .12;
		BoltMesh->SetVisibility(true);
	}
	Thunder(X, Z, bToWater ? 1 : .6, !bToWater);
}

void AKeysWeather::Thunder(double X, double Z, double Power, bool bCloud)
{
	FVector Cam;
	if (!CameraPos(Cam)) return;
	const double D = FMath::Sqrt(FMath::Square(X - Cam.X) + FMath::Square(Z - Cam.Z));
	// sound covers a kilometer in about three seconds
	Thunders.Add({Clock() + D / 343, Power, D, !bCloud && D < 1800});
}

void AKeysWeather::UpdateLightning(double Dt)
{
	const double Now = Clock();
	for (FCell& C : Cells)
	{
		if (C.Elec < .05) { C.NextBolt = FMath::Max(C.NextBolt, Now + 5); continue; }
		if (Now < C.NextBolt) continue;
		// every 10-40 s at a cell's peak, more in-cloud flashes than strikes; one flash at a time
		C.NextBolt = Now + Rng.FRandRange(8.f, 40.f) / C.Elec;
		if (Now - BoltT < 1.2) continue;
		const double R = C.R * (.55 + .45 * C.Build), A = Rng.FRandRange(0.f, 2 * PI), D = R * FMath::Sqrt(Rng.FRand()) * .7;
		Bolt(C.X + FMath::Cos(A) * D, C.Z + FMath::Sin(A) * D, Rng.FRand() < .4f);
	}
	// the flash
	double F = 0;
	for (const FVector2D& S : Strokes) if (Now >= S.X) F += S.Y * FMath::Exp(-(Now - S.X) / .07);
	FlashI = F;
	if (BoltMesh->IsVisible())
	{
		if (Now > BoltShowUntil) BoltMesh->SetVisibility(false);
		else if (BoltMID) BoltMID->SetScalarParameterValue(TEXT("Amount"), float(FMath::Clamp(.35 + F, 0.0, 1.5)));
	}
	FVector Cam;
	const bool bCam = CameraPos(Cam);
	const double Dist = bCam ? FVector::Dist(Cam, FlashPos) : 5000;
	if (F > .01)
	{
		BoltLight->SetVisibility(true);
		BoltLight->SetWorldLocation(ToUE(FlashPos.X, FMath::Min(FlashPos.Y, 900.0), FlashPos.Z));
		BoltLight->SetIntensity(float(2.5e7 * F * (FlashPos.Y > 1000 ? .35 : 1)));
	}
	else if (BoltLight->IsVisible()) BoltLight->SetVisibility(false);

	// thunder when its sound arrives
	AKeysAudio* Au = AKeysAudio::Get(this);
	for (int32 i = Thunders.Num() - 1; i >= 0; i--)
	{
		const FThunder Th = Thunders[i];
		if (Now < Th.At) continue;
		Thunders.RemoveAt(i);
		if (!Au) continue;
		const float G = float(Th.Power * FMath::Clamp(1.8 / (1 + Th.Dist / 1400), .04, 1.0));
		if (Th.bCrack)
		{
			FKeysBurst A; A.Filter = KeysDSP::EFilter::Highpass; A.Freq = 1600; A.Dur = .3f; A.Gain = .55f * G; A.Att = .002f;
			FKeysBurst B; B.Freq = 1200; B.FreqTo = 180; B.Dur = 1.1f; B.Gain = .8f * G; B.Att = .005f;
			Au->Burst(A); Au->Burst(B);
		}
		// the long roll: low noise bursts piling up and fading, duller the further it came
		const float Dull = float(FMath::Clamp(1 - Th.Dist / 9000, .25, 1.0));
		for (int32 k = 0; k < 4; k++)
		{
			FKeysBurst R; R.Freq = (260 - k * 35) * Dull + 40; R.FreqTo = 55; R.Dur = 2.2f + k * 1.1f + Rng.FRandRange(0.f, 1.f);
			R.Gain = (.55f - .1f * k) * G; R.Att = .15f + .25f * k; R.Delay = k * Rng.FRandRange(.25f, .6f);
			Au->Burst(R);
		}
		FKeysBurst Sub; Sub.bTone = true; Sub.Tone = 48; Sub.ToneTo = 30; Sub.Dur = 2.6f; Sub.Gain = .3f * G; Sub.Att = .2f;
		Au->Burst(Sub);
	}
	if (AKeysWorld* W = AKeysWorld::Get(this)) W->SetWeatherLight(Gloom, RainCam, FlashI * FMath::Clamp(2500 / FMath::Max(1.0, Dist), .12, 1.4));
}

// ---------------------------------------------------------------- waterspouts
void AKeysWeather::UpdateSpouts(double Dt)
{
	const FCell* Best = nullptr;
	double PX, PZ;
	const bool bHave = PlayerPos(PX, PZ);
	double BestD = 1e18;
	for (const FCell& C : Cells)
		if (C.bSpout)
		{
			const double D = bHave ? FMath::Sqrt(FMath::Square(C.SX - PX) + FMath::Square(C.SZ - PZ)) : 0;
			if (D < BestD) { BestD = D; Best = &C; }
		}
	if (!Best || BestD > 25000) { Spout->SetVisibility(false); Skirt->SetVisibility(false); return; }
	const double T = GetWorld()->GetTimeSeconds();
	BuildSpout(*Best, T);
	if (!bHave) return;
	const double Age = Clock() - Best->SBorn, K = WStep(0, 25, Age) * (1 - WStep(Best->SLife - 40, Best->SLife, Age));
	// the spray ring where it touches down
	if (BestD < 1500)
		if (AKeysLife* L = AKeysLife::Get(this))
			for (int32 i = 0; i < 3; i++)
			{
				const double A = Rng.FRandRange(0.f, 2 * PI), R = Rng.FRandRange(4.f, 30.f);
				if (Rng.FRand() < Dt * 14 * K) L->Splash(Best->SX + FMath::Cos(A) * R, Best->SZ + FMath::Sin(A) * R, 8, 1.3);
			}
	// close in it grabs the boat: pulls you in and spins you round it, with spray everywhere
	if (BestD < 280)
	{
		const double F = FMath::Pow(1 - BestD / 280, 1.5) * K;
		const double UX = (Best->SX - PX) / FMath::Max(1.0, BestD), UZ = (Best->SZ - PZ) / FMath::Max(1.0, BestD);
		if (AKeysBoatPawn* P = Cast<AKeysBoatPawn>(UGameplayStatics::GetPlayerPawn(this, 0)))
		{
			P->WeatherPush((UX * 3.5 - UZ * 8) * F, (UZ * 3.5 + UX * 8) * F, Dt, .25 + .6 * F);
			if (Clock() - SpoutPushMsgT > 8) { SpoutPushMsgT = Clock(); P->ToastMsg(TEXT("The spout's got you - full throttle, steer out of it!"), 4); }
		}
		if (AKeysLife* L = AKeysLife::Get(this))
			if (Rng.FRand() < Dt * 20 * F) L->Splash(PX + Rng.FRandRange(-8.f, 8.f), PZ + Rng.FRandRange(-8.f, 8.f), 10, 1.4);
	}
}

void AKeysWeather::Radio(const FString& Msg)
{
	if (AKeysBoatPawn* P = Cast<AKeysBoatPawn>(UGameplayStatics::GetPlayerPawn(this, 0))) P->ToastMsg(TEXT("VHF weather: ") + Msg, 7);
	UE_LOG(LogTemp, Log, TEXT("Keys weather radio: %s"), *Msg);
}

// ---------------------------------------------------------------- the look
void AKeysWeather::UpdateLook(double Dt)
{
	FVector Cam;
	const bool bCam = CameraPos(Cam);
	// the cumulus field drifts with the steering wind
	DriftX -= FMath::Cos(SteerDir) * SteerSpd * Dt;
	DriftZ -= FMath::Sin(SteerDir) * SteerSpd * Dt;

	// the volumetric clouds: our own material, with the cells in it
	if (!bCloudsOn)
	{
		// only once its shaders are ready: the cloud renderer asserts on a volume material that has fallen back to the
		// default surface material while it compiles
		UMaterialInterface* M = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/KeysRun/Materials/M_KR_Cloud.M_KR_Cloud"));
		UMaterial* Base = M ? M->GetMaterial() : nullptr;
		FMaterialResource* Res = Base ? Base->GetMaterialResource(GMaxRHIShaderPlatform) : nullptr;
		bool bReady = false;
		// Once it has compiled (the view pass falls back gracefully while its cloud shaders come in on demand; the cloud
		// shadow pass would assert, so AKeysWorld keeps cloud shadows off). Give up after five minutes.
		CloudWaitT += Dt;
		const bool bLog = CloudWaitT - CloudLogT > 10;
		if (bLog) CloudLogT = CloudWaitT;
		if (!Base || !Res) { bCloudsOn = true; UE_LOG(LogTemp, Warning, TEXT("Keys weather: no M_KR_Cloud (%d) or no resource for this platform - keeping the engine's clouds"), Base ? 1 : 0); }
		else
		{
#if WITH_EDITOR
			for (const FString& E : Res->GetCompileErrors()) { UE_LOG(LogTemp, Warning, TEXT("Keys weather: M_KR_Cloud: %s"), *E); }
			if (Res->GetCompileErrors().Num() || Base->MaterialDomain != MD_Volume || CloudWaitT > 300) bCloudsOn = true;
			else if (Res->IsCompilationFinished()) { bReady = true; bCloudsOn = true; }
			if (bLog || bCloudsOn)
				UE_LOG(LogTemp, Log, TEXT("Keys weather: M_KR_Cloud after %.0f s: finished %d, errors %d, complete %d -> %s"), CloudWaitT, Res->IsCompilationFinished() ? 1 : 0,
					Res->GetCompileErrors().Num(), Res->IsGameThreadShaderMapComplete() ? 1 : 0, bReady ? TEXT("on") : bCloudsOn ? TEXT("gave up") : TEXT("waiting"));
#else
			// a packaged game has the cloud shaders cooked in: on once its shader map is in
			if (Base->MaterialDomain != MD_Volume || CloudWaitT > 300) bCloudsOn = true;
			else if (Res->IsGameThreadShaderMapComplete()) { bReady = true; bCloudsOn = true; }
#endif
		}
		if (bReady)
		if (AKeysWorld* W = AKeysWorld::Get(this))
			if (UVolumetricCloudComponent* CC = W->CloudComponent())
				{
					CloudMID = UMaterialInstanceDynamic::Create(M, this);
					CC->SetMaterial(CloudMID);
					CC->SetLayerBottomAltitude(.5f);
					CC->SetLayerHeight(11.5f);
					CC->SetTracingMaxDistance(70.f);
					CC->SetShadowTracingDistance(12.f);
					// the layer is tall (the storm towers): at the default sample count each step through a fair-weather
					// cumulus is long enough that the jittered steps show as salt-and-pepper speckle on the cloud edges
					CC->SetViewSampleCountScale(3.f);
					CC->SetReflectionViewSampleCountScale(1.5f);
				}
	}

	Gloom = 0;
	double NearCloud = 0;
	for (int32 i = 0; i < 4; i++)
	{
		const FCell* C = Cells.IsValidIndex(i) ? &Cells[i] : nullptr;
		const double R = C ? C->R * (.55 + .45 * C->Build) : 1;
		const double D = (C && bCam) ? FMath::Sqrt(FMath::Square(C->X - Cam.X) + FMath::Square(C->Z - Cam.Z)) : 1e9;
		if (C)
		{
			Gloom = FMath::Max(Gloom, C->Cloud * C->Build * WStep(2.4 * R, .7 * R, D));
			NearCloud = FMath::Max(NearCloud, C->Cloud * WStep(6 * R, R, D));
		}
		if (CloudMID)
		{
			CloudMID->SetVectorParameterValue(*FString::Printf(TEXT("Cell%d"), i), C ? FLinearColor(float(C->X), float(C->Z), float(C->R), 0) : FLinearColor(0, 0, 1000, 0));
			CloudMID->SetScalarParameterValue(*FString::Printf(TEXT("P%d"), i), C ? float(C->Cloud * (.25 + .75 * C->Build) * C->Power) : 0.f);
		}
		// its rain shaft, down from the cloud base; thinned when you are inside it (the rain near you does the work then)
		const bool bShaft = C && C->Rain > .02;
		Shafts[i]->SetVisibility(bShaft);
		if (bShaft)
		{
			const double SR = R * .8;
			Shafts[i]->SetWorldLocation(ToUE(C->X, -3, C->Z));
			Shafts[i]->SetWorldScale3D(FVector(SR * 100, SR * 100, 700 * 100));
			const double Inside = WStep(SR * 1.05, SR * .6, D);
			if (ShaftMIDs[i]) ShaftMIDs[i]->SetScalarParameterValue(TEXT("Amount"), float(C->Rain * (1 - .7 * Inside)));
		}
	}
	Gloom = FMath::Min(.92, Gloom * .95);
	if (CloudMID)
	{
		CloudMID->SetScalarParameterValue(TEXT("Cov"), float(.5 + .1 * NearCloud));
		CloudMID->SetVectorParameterValue(TEXT("Drift"), FLinearColor(float(DriftX), float(DriftZ), 0, 0));
		CloudMID->SetVectorParameterValue(TEXT("Shear"), FLinearColor(float(FMath::Cos(SteerDir)), float(FMath::Sin(SteerDir)), 0, 0));
		CloudMID->SetVectorParameterValue(TEXT("Flash"), FLinearColor(float(FlashPos.X), float(FlashPos.Z), float(FlashPos.Y), 0));
		CloudMID->SetScalarParameterValue(TEXT("FlashI"), float(FlashI));
	}

	// rain right around the camera, slanting with the wind
	RainCam = bCam ? KeysWx::RainAt(Cam.X, Cam.Z) : 0;
	RainCyl->SetVisibility(RainCam > .02);
	if (RainCam > .02 && bCam)
	{
		RainCyl->SetWorldLocation(ToUE(Cam.X, Cam.Y, Cam.Z));
		const double W = WindSpeed(), Wd = WindDir();
		const FVector Axis = FVector::CrossProduct(FVector::UpVector, FVector(FMath::Cos(Wd), FMath::Sin(Wd), 0)).GetSafeNormal();
		RainCyl->SetWorldRotation(FQuat(Axis.IsNearlyZero() ? FVector::ForwardVector : Axis, -FMath::Atan(W / 9) * .7));
		if (RainMID) RainMID->SetScalarParameterValue(TEXT("Amount"), float(RainCam));
	}
	if (AKeysAudio* Au = AKeysAudio::Get(this)) Au->SetWeather(RainCam, WindSpeed());
}

void AKeysWeather::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	const double Dt = FMath::Min(double(DeltaSeconds), .25);
	UpdateCells(Dt);
	UpdateWind(Dt);
	UpdateLook(Dt);
	UpdateSpouts(Dt);
	UpdateLightning(Dt);
}

// ---------------------------------------------------------------- test hooks
FString AKeysWeather::DebugCmd(const FString& Cmd)
{
	double PX, PZ;
	PlayerPos(PX, PZ);
	// "dist,bearing[,x]" after '=' or ':' -> a point; bearing in compass degrees (0 north, 90 east)
	auto Args = [&Cmd](TArray<double>& Out)
	{
		int32 I = INDEX_NONE;
		if (!Cmd.FindChar(TEXT('='), I) || Cmd.Mid(I + 1).StartsWith(TEXT("spawn"))) { I = Cmd.Find(TEXT(":")); }
		if (I == INDEX_NONE) return;
		TArray<FString> P;
		Cmd.Mid(I + 1).ParseIntoArray(P, TEXT(","));
		for (const FString& S : P) Out.Add(FCString::Atod(*S));
	};
	auto At = [&](double D, double BDeg, double& OX, double& OZ)
	{
		const double B = FMath::DegreesToRadians(BDeg);
		OX = PX + FMath::Sin(B) * D; OZ = PZ - FMath::Cos(B) * D;
	};
	// where storms come from, as a compass bearing
	const double UpBearing = FMath::RadiansToDegrees(FMath::Atan2(-FMath::Cos(SteerDir), FMath::Sin(SteerDir)));
	TArray<double> A;
	Args(A);
	if (Cmd.StartsWith(TEXT("storm=spawn")))
	{
		double X, Z;
		At(A.Num() > 0 ? A[0] : 6000, A.Num() > 1 ? A[1] : UpBearing, X, Z);
		FCell& C = SpawnCell(X, Z, A.Num() > 2 ? FMath::Clamp(A[2], 0.0, .99) : 0, Rng.FRand() < .35f);
		if (Cells.Num() > 4) Cells.RemoveAt(0);
		return FString::Printf(TEXT("storm cell %d at %.0f, %.0f (r %.0f m, life %.0f s)"), C.Id, C.X, C.Z, C.R, C.Life);
	}
	if (Cmd == TEXT("storm=clear")) { Cells.Empty(); Thunders.Empty(); NextSpawn = Clock() + 900; SetStormWind(0, 0, 0, 0); return TEXT("storms cleared"); }
	if (Cmd.StartsWith(TEXT("weather=")))
	{
		const FString W = Cmd.Mid(8);
		Cells.Empty(); Thunders.Empty();
		SetStormWind(0, 0, 0, 0);
		if (W == TEXT("calm")) { SetSeaState(0); NextSpawn = Clock() + 1800; }
		else if (W == TEXT("breezy")) { SetSeaState(1); NextSpawn = Clock() + 900; }
		else if (W == TEXT("storm"))
		{
			SetSeaState(1);
			FCell& C = SpawnCell(PX + 300, PZ - 200, .45, true);
			C.bSpoutTried = true;
			C.bWarned = C.bOverhead = true;
			UpdateCells(0);
			UpdateWind(0);
			NextSpawn = Clock() + 900;
		}
		SnapSea();
		return TEXT("weather ") + W;
	}
	if (Cmd.StartsWith(TEXT("spout")))
	{
		double X, Z;
		At(A.Num() > 0 ? A[0] : 1200, A.Num() > 1 ? A[1] : 0, X, Z);
		FCell& C = SpawnCell(X, Z, .45, true);
		C.bSpoutTried = C.bSpout = true;
		C.SX = X; C.SZ = Z; C.SBorn = Clock() - 30; C.SLife = 300;
		if (Cells.Num() > 4) Cells.RemoveAt(0);
		return FString::Printf(TEXT("waterspout at %.0f, %.0f"), X, Z);
	}
	if (Cmd.StartsWith(TEXT("bolt")))
	{
		double X, Z;
		At(A.Num() > 0 ? A[0] : 1500, A.Num() > 1 ? A[1] : 0, X, Z);
		Bolt(X, Z, true);
		return FString::Printf(TEXT("lightning at %.0f, %.0f"), X, Z);
	}
	if (Cmd.StartsWith(TEXT("wxskip="))) { Skip += FCString::Atod(*Cmd.Mid(7)); return FString::Printf(TEXT("weather clock +%s s"), *Cmd.Mid(7)); }
	return TEXT("?");
}
