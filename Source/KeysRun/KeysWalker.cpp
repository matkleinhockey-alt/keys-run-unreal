#include "KeysWalker.h"
#include "KeysTownData.h"
#include "KeysTown.h"
#include "KeysGeo.h"
#include "KeysMesh.h"
#include "KeysWorld.h"
#include "ProceduralMeshComponent.h"
#include "Components/PostProcessComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "KeysBoatPawn.h"
#include "Kismet/GameplayStatics.h"

using namespace KeysTown;

namespace
{
	// shared/sim/walker.ts
	constexpr double WalkSpeed = 2.4, SprintSpeed = 4.8, WalkRadius = .32, StepMax = .7, AccelTau = .08;
	constexpr double TieUpMaxSpeed = 1.6, BoardReach = 6, CabWindow = 4;
	constexpr double EyeHeight = 1.62;
	double AngDiff(double A, double B) { double D = FMath::Fmod(B - A + PI, 2 * PI); if (D < 0) D += 2 * PI; return D - PI; }
}

AKeysWalker::AKeysWalker()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickGroup = TG_PostPhysics;
	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent = Root;
	Body = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("Body"));
	Body->SetupAttachment(Root);
	Body->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Indoor = CreateDefaultSubobject<UPostProcessComponent>(TEXT("Indoor"));
	Indoor->SetupAttachment(Root);
	Indoor->bUnbound = true;
	Indoor->Priority = 5;
	Indoor->BlendWeight = 0;
}

double AKeysWalker::IndoorEV = 2.0;

void AKeysWalker::UpdateIndoor(double Dt)
{
	const KeysTown::FLot* Lot = bAshore ? KeysTown::InsideLot(X, Z) : nullptr;
	const bool bDark = Lot && (Lot->Style == KeysTown::EStyle::Club || Lot->Style == KeysTown::EStyle::Casino || Lot->Style == KeysTown::EStyle::Bar);
	const double Want = Lot ? 1 : 0;
	IndoorW += (Want - IndoorW) * FMath::Min(1.0, Dt * 3);
	FPostProcessSettings& S = Indoor->Settings;
	S.bOverride_AutoExposureMinBrightness = S.bOverride_AutoExposureMaxBrightness = true;
	// EV100: a dark room is held where the street was; a lit shop may brighten a little, never all the way
	S.AutoExposureMinBrightness = float(bDark ? IndoorEV : IndoorEV - 1.5);
	S.AutoExposureMaxBrightness = float(bDark ? IndoorEV + .5 : 16);
	S.bOverride_AutoExposureSpeedUp = S.bOverride_AutoExposureSpeedDown = true;
	S.AutoExposureSpeedUp = 2.f; S.AutoExposureSpeedDown = 2.f;
	Indoor->BlendWeight = float(IndoorW);
	Indoor->bEnabled = IndoorW > .01;
}

AKeysWalker* AKeysWalker::Get(const UObject* WorldContext)
{
	UWorld* W = WorldContext ? WorldContext->GetWorld() : nullptr;
	if (!W) return nullptr;
	TActorIterator<AKeysWalker> It(W);
	return It ? *It : nullptr;
}

void AKeysWalker::BeginPlay()
{
	Super::BeginPlay();
	SetActorLocation(FVector::ZeroVector);
	FKeysHumanLook L;
	L.Shirt = FLinearColor(FColor(0x3f, 0xb6, 0xc9));
	L.Bottom = FLinearColor(FColor(0xcd, 0xbf, 0x9a));
	L.bCap = true;
	L.CapColor = FLinearColor(FColor(0x1d, 0x35, 0x57));
	L.bShoes = true;
	L.bGlasses = true;
	Me.Init(L, false);
	Body->SetVisibility(false);
}

void AKeysWalker::SetOutfit(const FLinearColor& Shirt, bool bCap, const FLinearColor& Cap, bool bGlasses)
{
	FKeysHumanLook L = Me.Look;
	L.Shirt = Shirt;
	L.bCap = bCap;
	L.CapColor = Cap;
	L.bGlasses = bGlasses;
	Me.Init(L, false);
}

bool AKeysWalker::CanStand(double PX, double PZ, double FromY, double& OutY) const
{
	double G;
	if (!GroundAt(PX, PZ, G) || FMath::Abs(G - FromY) > StepMax) return false;
	const double K = WalkRadius * .7;
	double GG;
	for (const FVector2D& D : {FVector2D(K, 0), FVector2D(-K, 0), FVector2D(0, K), FVector2D(0, -K)})
		if (!GroundAt(PX + D.X, PZ + D.Y, GG)) return false;
	const double R = WalkRadius;
	for (const FRect& C : Colliders())
		if (PX > C.X0 - R && PX < C.X1 + R && PZ > C.Z0 - R && PZ < C.Z1 + R) return false;
	if (ObstacleAt(PX, PZ, R)) return false;   // the keys' houses, palms and estate houses
	OutY = G;
	return true;
}

bool AKeysWalker::TryEnter(double BoatX, double BoatZ, double BoatH, double BoatSpeed, double HullLen, FString& OutWhy)
{
	if (bAshore) { OutWhy = TEXT("Already ashore."); return false; }
	if (FMath::Abs(BoatSpeed) > TieUpMaxSpeed) { OutWhy = TEXT("Slow down to tie up at the dock."); return false; }
	double BX, BZ;
	FString Name;
	if (!FindBerth(BoatX, BoatZ, HullLen / 2 + 9, BX, BZ, Name)) { OutWhy = TEXT("Pull alongside a dock to tie up (any of the four marinas)."); return false; }
	double GY;
	if (!CanStand(BX, BZ, DeckTop, GY)) { OutWhy = TEXT("Nowhere safe to step off here - try the main pier."); return false; }
	X = BX; Z = BZ; Y = GY; VX = VZ = 0; Speed = 0; Gait = 0;
	// face up the pier toward the shore, so the first thing you see is where you are going
	const KeysGeo::FMarina* NM = KeysGeo::NearMarina(BX, BZ);
	double TX = BX - BoatX, TZ = BZ - BoatZ;
	if (NM) { TX = NM->SX - BX; TZ = NM->SZ - NM->Dir * 12 - BZ; }
	Yaw = FMath::Sqrt(TX * TX + TZ * TZ) > .5 ? FMath::Atan2(-TX, -TZ) : BoatH;
	CamYaw = Yaw; CamPitch = -.08;
	DockX = BX; DockZ = BZ; MoorX = BoatX; MoorZ = BoatZ; MoorH = BoatH; HullL = HullLen;
	FarPressedAt = -100;
	bAshore = true;
	bShopOpen = false;
	Body->SetVisibility(CamMode == 1);
	OutWhy = Name;
	return true;
}

double AKeysWalker::BoardDistance() const
{
	if (!bAshore) return 1e9;
	const double ToBoat = FMath::Sqrt(FMath::Square(X - MoorX) + FMath::Square(Z - MoorZ)) - HullL / 2;
	const double ToDock = FMath::Sqrt(FMath::Square(X - DockX) + FMath::Square(Z - DockZ));
	return FMath::Max(0.0, FMath::Min(ToBoat, ToDock));
}

FString AKeysWalker::BoatBearing() const
{
	const double A = FMath::Atan2(MoorX - X, -(MoorZ - Z));
	static const TCHAR* Names[] = {TEXT("N"), TEXT("NE"), TEXT("E"), TEXT("SE"), TEXT("S"), TEXT("SW"), TEXT("W"), TEXT("NW")};
	return Names[((FMath::RoundToInt(A / (PI / 4)) % 8) + 8) % 8];
}

AKeysWalker::EBoard AKeysWalker::RequestBoard()
{
	if (BoardDistance() <= BoardReach) { ExitToBoat(); return EBoard::Boarded; }
	if (Clock - FarPressedAt <= CabWindow)
	{
		// the cab: back to the plank you stepped off from
		X = DockX; Z = DockZ;
		double G;
		Y = GroundAt(X, Z, G) ? G : DeckTop;
		VX = VZ = 0;
		FarPressedAt = -100;
		return EBoard::Cab;
	}
	FarPressedAt = Clock;
	return EBoard::Far;
}

void AKeysWalker::ExitToBoat()
{
	bAshore = false;
	bShopOpen = false;
	Body->SetVisibility(false);
	Body->ClearAllMeshSections();
	if (AKeysTown* T = AKeysTown::Get(this)) T->SetWalker(false, FVector::ZeroVector);
}

FString AKeysWalker::CycleView()
{
	CamMode = 1 - CamMode;
	Body->SetVisibility(bAshore && CamMode == 1);
	return CamMode == 0 ? TEXT("First person") : TEXT("Third person");
}

bool AKeysWalker::DebugPlace(double PX, double PZ, double InYaw)
{
	if (!bAshore) return false;
	double G;
	if (!GroundAt(PX, PZ, G)) return false;
	X = PX; Z = PZ; Y = G; VX = VZ = 0;
	Yaw = CamYaw = InYaw;
	return true;
}

bool AKeysWalker::DebugPlaceLocal(double U, double V, double InYaw)
{
	double PX, PZ;
	ToWorld(U, V, PX, PZ);
	// the local yaw (0 faces +u, PI/2 faces +v) as a heading: forward = (-sin h, -cos h) in x, z
	const double FX = Dir() * FMath::Cos(InYaw), FZ = -Dir() * FMath::Sin(InYaw);
	return DebugPlace(PX, PZ, FMath::Atan2(-FX, -FZ));
}

void AKeysWalker::Step(double Dt)
{
	if (bShopOpen) { Btn = FKeysWalkButtons(); }
	CamYaw -= Btn.LookYaw;
	CamYaw -= Btn.Turn * Dt * 1.9;
	CamPitch = FMath::Clamp(CamPitch + Btn.LookPitch, -1.25, 1.25);
	const double FX = -FMath::Sin(CamYaw), FZ = -FMath::Cos(CamYaw), RX = FMath::Cos(CamYaw), RZ = -FMath::Sin(CamYaw);
	double WX = 0, WZ = 0;
	if (Btn.bFwd) { WX += FX; WZ += FZ; }
	if (Btn.bBack) { WX -= FX; WZ -= FZ; }
	if (Btn.bRight) { WX += RX; WZ += RZ; }
	if (Btn.bLeft) { WX -= RX; WZ -= RZ; }
	const double WL = FMath::Sqrt(WX * WX + WZ * WZ);
	const double Top = Btn.bSprint ? SprintSpeed : WalkSpeed;
	const double TX = WL > 0 ? WX / WL * Top : 0, TZ = WL > 0 ? WZ / WL * Top : 0;
	const double K = 1 - FMath::Exp(-Dt / AccelTau);
	double NVX = VX + (TX - VX) * K, NVZ = VZ + (TZ - VZ) * K;
	double G;
	const double TryX = X + NVX * Dt;
	if (CanStand(TryX, Z, Y, G)) { X = TryX; Y = G; } else NVX = 0;
	const double TryZ = Z + NVZ * Dt;
	if (CanStand(X, TryZ, Y, G)) { Z = TryZ; Y = G; } else NVZ = 0;
	VX = NVX; VZ = NVZ;
	Speed = FMath::Sqrt(VX * VX + VZ * VZ);
	if (Speed > .25) Yaw += AngDiff(Yaw, FMath::Atan2(-VX, -VZ)) * FMath::Min(1.0, Dt * 12);
	Gait += Speed * Dt * 2.1;
}

void AKeysWalker::UpdateCamera(double Dt)
{
	const double WantFov = CamMode == 0 ? 72 : 62;
	Fov += (WantFov - Fov) * FMath::Min(1.0, Dt * 4);
	FVector Eye;
	if (CamMode == 0)
	{
		const double Bob = FMath::Sin(Gait * 2) * .035 * FMath::Min(1.0, Speed / 2.4);
		Eye = FVector(X, Y + EyeHeight + Bob, Z);
	}
	else
	{
		// a boom behind the shoulder that pulls in rather than go through a wall
		const double FX = -FMath::Sin(CamYaw) * FMath::Cos(CamPitch), FY = FMath::Sin(CamPitch), FZ = -FMath::Cos(CamYaw) * FMath::Cos(CamPitch);
		const FVector Tgt(X, Y + 1.45, Z);
		double Want = 3.7;
		auto Blocked = [](double PX, double PZ, double PY)
		{
			for (const FRect& C : Colliders())
				if (PX > C.X0 - .28 && PX < C.X1 + .28 && PZ > C.Z0 - .28 && PZ < C.Z1 + .28) return true;
			if (const FLot* L = InsideLot(PX, PZ)) { (void)L; }
			return PY < 0;
		};
		double D = .4;
		for (; D < Want; D += .2) if (Blocked(Tgt.X - FX * D, Tgt.Z - FZ * D, Tgt.Y - FY * D)) break;
		D = FMath::Max(.4, D - .2);
		Boom += (D - Boom) * FMath::Min(1.0, Dt * (D < Boom ? 20 : 4));
		Eye = Tgt - FVector(FX, FY, FZ) * Boom;
		// indoors (under a roof) keep the camera under the ceiling
		if (const FLot* L = InsideLot(X, Z)) Eye.Y = FMath::Min(Eye.Y, PadY + FMath::Min(L->H, 4.4) - .35);
	}
	CamPosUE = KeysGeo::ToUE(Eye.X, Eye.Y, Eye.Z);
	const FVector Fw(-FMath::Sin(CamYaw), -FMath::Cos(CamYaw), 0);
	CamRotUE = FRotator(FMath::RadiansToDegrees(CamPitch), FMath::RadiansToDegrees(FMath::Atan2(Fw.Y, Fw.X)), 0);
	// horizontal FOV from the vertical one at 16:9
	CamFovH = float(FMath::RadiansToDegrees(2 * FMath::Atan(FMath::Tan(FMath::DegreesToRadians(Fov) / 2) * 16.0 / 9)));
}

void AKeysWalker::UpdateBody()
{
	if (CamMode != 1) return;
	Me.Pos = FVector(X, Z, Y);
	const FVector Fw(-FMath::Sin(Yaw), -FMath::Cos(Yaw), 0);
	Me.Yaw = FMath::Atan2(Fw.Y, Fw.X);
	Me.IdleUpdate(Clock, 0);
	if (Speed > .2)
	{
		const double G = Gait * 1.3, St = .26 * FMath::Min(1.6, Speed / 2.4);
		Me.PoseLegs(FKeysHuman::J(-.105, .085 + FMath::Max(0.0, FMath::Sin(G)) * .12, FMath::Sin(G) * St), FKeysHuman::J(.105, .085 + FMath::Max(0.0, -FMath::Sin(G)) * .12, -FMath::Sin(G) * St));
		Me.Pose(FKeysHuman::J(-.24, Me.HipY + .02, -FMath::Sin(G) * .25), FKeysHuman::J(.24, Me.HipY + .02, FMath::Sin(G) * .25));
		Me.Pos.Z += FMath::Abs(FMath::Sin(G)) * .035;
	}
	else Me.DefaultPose();
	FKeysMesh M;
	Me.Build(M);
	M.Commit(Body, 0, AKeysWorld::VertexColorMaterial());
}

void AKeysWalker::FindInteract()
{
	PromptText.Reset(); NearInteract.Reset(); NearVenueId.Reset(); NearPlaque.Reset();
	double U, V;
	ToLocal(X, Z, U, V);
	double Best = 1e9;
	for (const FInteract& I : Interacts())
	{
		const double D = FMath::Sqrt(FMath::Square(U - I.U) + FMath::Square(V - I.V));
		if (D > I.R || D >= Best) continue;
		Best = D;
		PromptText = TEXT("F  ") + I.Prompt;
		NearInteract = I.Id;
		NearVenueId = I.Venue;
		NearPlaque = I.Text;
	}
	if (NearInteract.IsEmpty() && BoardDistance() <= BoardReach) PromptText = TEXT("Q  Back aboard");
}

FString AKeysWalker::PlaceName() const
{
	if (const FLot* L = InsideLot(X, Z))
		if (const FVenue* Vn = Venue(L->Venue)) return Vn->Name;
	double U, V;
	ToLocal(X, Z, U, V);
	if (U >= PadU0 && U <= PadU1 && V >= PadV0 && V <= PadV1)
	{
		for (const FSurface& S : Surfaces())
			if (U >= S.U0 && U <= S.U1 && V >= S.V0 && V <= S.V1)
			{
				if (S.Kind == ESurface::Promenade) return TEXT("The promenade, Boot Key Harbor City");
				if (V >= 28 && V < 46) return TEXT("Harbor Drive");
				if (V >= 84 && V < 100) return TEXT("Sombrero Boulevard");
				if (FMath::Abs(U) < 8 && V < 100) return TEXT("Palm Avenue");
				if (V >= 72 && V < 80) return TEXT("The back lane");
				if (V >= 126) return TEXT("Harbor Row");
			}
		return TEXT("Boot Key Harbor City");
	}
	if (const KeysGeo::FMarina* M = KeysGeo::NearMarina(X, Z)) return M->Name;
	return KeysGeo::ShoreInfo(X, Z).Isl->Name;
}

void AKeysWalker::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	UpdateIndoor(FMath::Min(double(DeltaSeconds), 1.0 / 20));
	if (!bAshore) return;
	const double Dt = FMath::Min(double(DeltaSeconds), 1.0 / 20);
	Clock += Dt;
	Step(Dt);
	UpdateCamera(Dt);
	UpdateBody();
	FindInteract();
	if (AKeysTown* T = AKeysTown::Get(this)) T->SetWalker(true, FVector(X, Y, Z));
	if (AKeysBoatPawn* P = Cast<AKeysBoatPawn>(UGameplayStatics::GetPlayerPawn(this, 0))) P->ApplyDiveCamera(CamPosUE, CamRotUE, CamFovH);
	Btn.LookYaw = Btn.LookPitch = 0;
}
