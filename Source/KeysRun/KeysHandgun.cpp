#include "KeysHandgun.h"
#include "KeysExtras.h"
#include "KeysKit.h"
#include "KeysGeo.h"
#include "KeysMesh.h"
#include "KeysHuman.h"
#include "KeysDive.h"
#include "KeysData.h"
#include "KeysLife.h"
#include "KeysWorld.h"
#include "KeysAudio.h"
#include "KeysBoatPawn.h"
#include "ProceduralMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/World.h"
#include "EngineUtils.h"

using namespace KeysGeo;
using namespace KeysKit;

namespace
{
	// ---- sim/handgun.ts
	constexpr double MuzzleSpeed = 350, DragK = .0016, Gravity = 9.81, BulletRange = 120, MicroDt = 1.0 / 300;
	constexpr double WaterStopM = 2.4, EnergyStun = .2, SkipAngle = 5 * PI / 180, SkipKeep = .7, AssistMaxDepth = 1.6;
	constexpr int32 MagBase = 12;
	constexpr double FireInterval = .2, ReloadS = 1.7, ReloadEmptyS = 2.2, SpreadBase = .008, SpreadPerHeat = .02, HeatPerShot = .34, HeatDecay = 1.5;
	constexpr double DotSpreadMul = .55;
	constexpr double AssistCone = .09, AssistStrength = .55, AssistRange = 70, ConvergeM = 30;
	constexpr double ReportBirdM = 140, BirdScatterS = 25, BirdHitS = 90, FishSpookS = 6;
	constexpr double FlareSpeed = 40, FlareIgniteS = .9, FlareBurnS = 18, FlareFallMs = .8, FlareMinPitch = .62, FlareDrag = 1.4;
	// sim/chum.ts and life/carcass.ts
	constexpr double CarcassLifeS = 120, CarcassReachM = 55, BirdReplaceS = CarcassLifeS + 30, SinkS = 6;
	const TCHAR* AmmoKey = TEXT("handgun.ammo");
	const TCHAR* FlareKey = TEXT("safety.flare");

	double BirdRadius(uint8 Kind) { return Kind == 1 ? .42 : (Kind == 2 || Kind == 3) ? .8 : .5; }
	const TCHAR* BirdName(uint8 Kind) { return Kind == 1 ? TEXT("tern") : Kind == 2 ? TEXT("frigatebird") : Kind == 3 ? TEXT("pelican") : TEXT("gull"); }
	double BirdAway(uint8 Kind, bool bStruck) { return (bStruck ? BirdHitS : BirdScatterS) * (Kind == 3 ? 1.4 : Kind == 2 ? 1.6 : 1); }
	double ScatterChance(double DistShooter, double DistPath)
	{
		const double Ear = FMath::Max(0.0, 1 - DistShooter / ReportBirdM) * .6;
		const double Whiz = DistPath < 6 ? .85 : DistPath < 14 ? .35 : 0;
		return FMath::Min(1.0, FMath::Max(Ear, Whiz));
	}
	double SurfaceY(double X, double Z, double T) { return WaveH(X, Z, T, AmpAt(X, Z)); }
	double DistToSeg(const FVector& P, const FVector& A, const FVector& B)
	{
		const FVector AB = B - A;
		const double L2 = AB.SizeSquared();
		const double U = L2 > 0 ? FMath::Clamp(FVector::DotProduct(P - A, AB) / L2, 0.0, 1.0) : 0;
		return FVector::Dist(P, A + AB * U);
	}
	// spreadDir: a uniform disc of half-angle Spread about Dir
	FVector SpreadDir(const FVector& Dir, double Spread)
	{
		const FVector U = Dir.GetSafeNormal();
		if (Spread <= 0) return U;
		FVector A = FMath::Abs(U.Y) < .9 ? FVector(U.Z, 0, -U.X) : FVector(0, -U.Z, U.Y);
		A.Normalize();
		const FVector B = FVector::CrossProduct(U, A);
		const double R = Spread * FMath::Sqrt(FMath::FRand()), Phi = FMath::FRand() * 2 * PI;
		return (U + A * FMath::Cos(Phi) * R + B * FMath::Sin(Phi) * R).GetSafeNormal();
	}
	// a glow-material piece: every vertex reads as solid emissive (the material's flat path)
	void FlatGlow(FKeysMesh& M, int32 From) { for (int32 i = From; i < M.UV.Num(); i++) M.UV[i] = FVector2D(2.5, .5); }
	void AppendXf(FKeysMesh& Dst, const FKeysMesh& Src, const FTransform& XfCm)
	{
		const int32 Base = Dst.V.Num();
		for (int32 i = 0; i < Src.V.Num(); i++)
		{
			Dst.V.Add(XfCm.TransformPosition(Src.V[i]));
			Dst.N.Add(XfCm.TransformVectorNoScale(Src.N[i]));
			Dst.UV.Add(Src.UV.IsValidIndex(i) ? Src.UV[i] : FVector2D::ZeroVector);
			Dst.C.Add(Src.C.IsValidIndex(i) ? Src.C[i] : FLinearColor::White);
		}
		for (int32 Ix : Src.T) Dst.T.Add(Base + Ix);
	}
	void BirdShape(FKeysMesh& M, const FVector& PosM, double Yaw, double Scale, uint8 Kind, double Flap, double Roll = 0)
	{
		const FTransform Xf(FQuat(FVector::ZAxisVector, Yaw) * FQuat(FVector::XAxisVector, Roll), PosM, FVector(Scale));
		const FLinearColor Body = Kind == 3 ? Hex(0x8a7a62) : Kind == 2 ? Hex(0x222428) : Hex(0xf4f4f0);
		const FLinearColor Wing = Kind == 3 ? Hex(0x6a5c48) : Kind == 2 ? Hex(0x15171a) : Kind == 1 ? Hex(0xd8dde2) : Hex(0x9aa3ab);
		KeysShapes::AddBlob(M, Xf.TransformPosition(FVector(0, 0, 0)), FVector(.2, .07, .07) * Scale, Xf.GetRotation(), Body, 6, 3);
		KeysShapes::AddBlob(M, Xf.TransformPosition(FVector(.17, 0, .04)), FVector(.05, .045, .045) * Scale, Xf.GetRotation(), Body, 5, 3);
		M.AddTube(Xf.TransformPosition(FVector(.21, 0, .04)), Xf.TransformPosition(FVector(.3, 0, .03)), .012 * Scale, .004 * Scale, 4, Hex(Kind == 3 ? 0xd8b04a : 0xf2c14e));
		const double W = FMath::Sin(Flap) * .5;
		for (int32 s = -1; s <= 1; s += 2)
		{
			const FVector R0 = Xf.TransformPosition(FVector(.07, s * .04, .02)), R1 = Xf.TransformPosition(FVector(-.09, s * .04, .02));
			const FVector Tip = Xf.TransformPosition(FVector(-.08, s * .62, .02 + W * .35));
			M.AddTri(R0, Tip, R1, Wing, FVector(0, 0, 1));
			M.AddTri(R0, R1, Tip, Wing, FVector(0, 0, -1));
		}
	}
}

AKeysHandgun::AKeysHandgun()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickGroup = TG_PostPhysics;
	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent = Root;
	auto Make = [this](const TCHAR* Name)
	{
		UProceduralMeshComponent* C = CreateDefaultSubobject<UProceduralMeshComponent>(Name);
		C->SetupAttachment(Root);
		C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		C->SetCastShadow(false);
		return C;
	};
	ViewModel = Make(TEXT("PistolView"));
	Solid = Make(TEXT("PistolSolid"));
	Glow = Make(TEXT("PistolGlow"));
	FlareLight = CreateDefaultSubobject<UPointLightComponent>(TEXT("FlareLight"));
	FlareLight->SetupAttachment(Root);
	FlareLight->SetIntensity(0);
	FlareLight->SetAttenuationRadius(30000);
	FlareLight->SetLightColor(FLinearColor(1.f, .25f, .2f));
	FlareLight->SetCastShadows(false);
	FlareLight->SetVisibility(false);
}

AKeysHandgun* AKeysHandgun::Get(const UObject* WorldContext)
{
	UWorld* W = WorldContext ? WorldContext->GetWorld() : nullptr;
	if (!W) return nullptr;
	TActorIterator<AKeysHandgun> It(W);
	return It ? *It : nullptr;
}

void AKeysHandgun::BeginPlay()
{
	Super::BeginPlay();
	SetActorLocation(FVector::ZeroVector);
	if (UMaterialInterface* GM = AKeysWorld::GlowMaterial())
	{
		GlowMID = UMaterialInstanceDynamic::Create(GM, this);
		GlowMID->SetScalarParameterValue(TEXT("Bright"), 1.6f);
	}
	ViewModel->SetVisibility(false);
	// the harbour gang: four gulls and two terns, orbit radii and heights spread so they read as a scatter
	for (int32 i = 0; i < 6; i++)
	{
		FGull G;
		G.Kind = i < 4 ? 0 : 1;
		G.R = 14 + i * 5 + FMath::FRand() * 4; G.H = 5 + (i % 3) * 3.5 + FMath::FRand() * 2;
		G.Sp = (.16 + FMath::FRand() * .12) * (i % 2 ? 1 : -1); G.Ph = FMath::FRand() * 6.28;
		G.Pos = FVector::ZeroVector;
		Gulls.Add(G);
	}
}

AKeysBoatPawn* AKeysHandgun::Pawn() const { return Cast<AKeysBoatPawn>(UGameplayStatics::GetPlayerPawn(this, 0)); }

int32 AKeysHandgun::Reserve() const { const AKeysBoatPawn* P = Pawn(); return P ? FMath::Max(0, P->CountOf(AmmoKey) - Mag) : 0; }
int32 AKeysHandgun::Flares() const { const AKeysBoatPawn* P = Pawn(); return P ? P->CountOf(FlareKey) : 0; }
double AKeysHandgun::Spread() const
{
	const AKeysBoatPawn* P = Pawn();
	return (SpreadBase + SpreadPerHeat * Heat) * (P && P->HasGear(TEXT("handgun.dot")) ? DotSpreadMul : 1);
}

bool AKeysHandgun::IsScavenger(const FString& Key)
{
	static const TSet<FString> Keys = {TEXT("bullshark"), TEXT("lemonshark"), TEXT("blacktip"), TEXT("tigershark"), TEXT("hammerhead"), TEXT("barracuda"), TEXT("jackcrevalle"),
		TEXT("amberjack"), TEXT("cobia"), TEXT("kingfish"), TEXT("wahoo"), TEXT("tarpon"), TEXT("goliath")};
	return Keys.Contains(Key);
}

bool AKeysHandgun::NearCarcass(double X, double Z, double R) const
{
	for (const FCarcass& C : Carcasses) if (C.Age < CarcassLifeS && FMath::Square(C.X - X) + FMath::Square(C.Z - Z) < R * R) return true;
	return false;
}

// ================================================================ draw / holster
void AKeysHandgun::Toggle()
{
	if (bOut) { Holster(); return; }
	AKeysBoatPawn* P = Pawn();
	if (!P || !P->bRunning) return;
	FString Why;
	if (P->IsBusy(Why)) { P->ToastMsg(Why); return; }
	P->PrepareToLeaveHelm();
	bOut = true;
	LookYaw = 0; LookPitch = .04;
	MagSize = MagBase + (P->HasGear(TEXT("handgun.extmag")) ? 6 : 0);
	// it arrives empty: the first draw loads it from the box
	if (Mag == 0 && !bReloading) Mag = FMath::Min(MagSize, P->CountOf(AmmoKey));
	Mag = FMath::Min(Mag, P->CountOf(AmmoKey));
	ViewModel->SetVisibility(true);
	P->ToastMsg(TEXT("Pistol out. Click or Space to fire, R to reload, 9 to holster. Fish near the surface and the gulls are fair game."), 5);
}

void AKeysHandgun::Holster()
{
	if (!bOut) return;
	bOut = false;
	ViewModel->SetVisibility(false);
	if (AKeysBoatPawn* P = Pawn()) P->ToastMsg(FString::Printf(TEXT("Pistol holstered. %d in the magazine, %d spare."), Mag, Reserve()));
}

void AKeysHandgun::Eye(FVector& OutEye, FVector& OutDir) const
{
	const AKeysBoatPawn* P = Pawn();
	if (!P) { OutEye = FVector::ZeroVector; OutDir = FVector(0, 0, -1); return; }
	const KeysData::FBoatSpec& S = P->Spec();
	// up on the bow casting deck, clear of the top, the crew and the rods
	const double LX = S.Len * .3;
	const FVector U = P->GetActorTransform().TransformPosition(FVector(LX, 0, KeysMesh::SoleAt(S, LX) + .36 + 1.62) * 100.0);
	OutEye = FVector(U.X / 100, U.Z / 100, U.Y / 100);
	const double Yaw = P->Heading() + LookYaw, Pt = LookPitch;
	OutDir = FVector(-FMath::Sin(Yaw) * FMath::Cos(Pt), FMath::Sin(Pt), -FMath::Cos(Yaw) * FMath::Cos(Pt));
}

bool AKeysHandgun::CameraFor(FVector& OutPosUE, FRotator& OutRot, float& OutFov) const
{
	if (!bOut) return false;
	FVector E, D;
	Eye(E, D);
	OutPosUE = ToUE(E.X, E.Y, E.Z);
	OutRot = FVector(D.X, D.Z, D.Y).Rotation();
	OutRot.Pitch += Kick * 3.5;
	OutFov = 75.f;
	return true;
}

void AKeysHandgun::HandleInput(APlayerController* PC, float Dt)
{
	auto Hit = [PC](const FKey& K) { return PC->WasInputKeyJustPressed(K); };
	auto Down = [PC](const FKey& K) { return PC->IsInputKeyDown(K); };
	float MX = 0, MY = 0;
	PC->GetInputMouseDelta(MX, MY);
	LookYaw -= MX * .0035;
	LookPitch = FMath::Clamp(LookPitch + MY * .0035, -1.2, 1.1);
	const double KR = 1.5 * Dt;
	if (Down(EKeys::Left)) LookYaw += KR;
	if (Down(EKeys::Right)) LookYaw -= KR;
	if (Down(EKeys::Up)) LookPitch = FMath::Min(1.1, LookPitch + KR);
	if (Down(EKeys::Down)) LookPitch = FMath::Max(-1.2, LookPitch - KR);
	if (Hit(EKeys::LeftMouseButton) || Hit(EKeys::SpaceBar) || Hit(EKeys::Enter)) Fire();
	if (Hit(EKeys::R)) StartReload();
	if (Hit(EKeys::Z)) FireFlare();
	if (Hit(EKeys::Nine) || Hit(EKeys::Escape)) Holster();
}

// ================================================================ the gun
void AKeysHandgun::StartReload()
{
	AKeysBoatPawn* P = Pawn();
	if (!P || bReloading) return;
	if (Reserve() <= 0 && Mag < MagSize) { Flash(TEXT("No spare rounds. Deadeye Guns & Ammo sells 9 mm by the box.")); return; }
	if (Mag >= MagSize) { Flash(TEXT("Magazine is full.")); return; }
	bFromEmpty = Mag == 0;
	bReloading = true;
	ReloadDur = ReloadT = bFromEmpty ? ReloadEmptyS : ReloadS;
	if (AKeysAudio* A = AKeysAudio::Get(this)) A->PlayExtra(TEXT("Reload"), FVector::ZeroVector, float(ReloadDur / 1.7));
}

void AKeysHandgun::Fire()
{
	AKeysBoatPawn* P = Pawn();
	if (!P || bReloading || FireT > 0) return;   // busy: no sound, so mashing does not spam
	AKeysAudio* Au = AKeysAudio::Get(this);
	if (Mag <= 0 || P->CountOf(AmmoKey) <= 0)
	{
		if (Au) Au->PlayExtra(TEXT("DryClick"), FVector::ZeroVector, 1);
		const int32 Res = Reserve();
		Flash(Res <= 0 ? TEXT("Out of 9 mm. Deadeye Guns & Ammo sells it by the box.") : TEXT("Click. Reload (R)."));
		return;
	}
	Mag--;
	P->UseOne(AmmoKey);
	P->SaveEconomy();
	FireT = FireInterval;
	Heat = FMath::Min(1.0, Heat + HeatPerShot);
	TallyShots++;
	Kick = 1; MuzzleT = .06; SlideT = .08;
	FVector E, D;
	Eye(E, D);
	const FVector Rt = FVector::CrossProduct(D, FVector(0, 1, 0)).GetSafeNormal();   // game frame: right of the view
	const FVector Muzzle = E + D * .55 - Rt * .18 - FVector(0, .14, 0);
	// aimBullet: converge on the crosshair (plus the drop to it), or lean toward the best target in the cone, leading it
	TArray<FTgt> Tg;
	GatherTargets(E, Tg);
	const double TofC = ConvergeM / (MuzzleSpeed * .97);
	FVector Aim = (E + D * ConvergeM + FVector(0, .5 * Gravity * TofC * TofC, 0) - Muzzle).GetSafeNormal();
	double BestAng = AssistCone;
	for (const FTgt& T : Tg)
	{
		const FVector C = (T.A + T.B) * .5;
		const double D0 = FVector::Dist(C, Muzzle);
		if (D0 < .3 || D0 > AssistRange) continue;
		const double Tof = D0 / (MuzzleSpeed * .97);
		const FVector Lead = C + T.Vel * Tof + FVector(0, .5 * Gravity * Tof * Tof, 0);
		const FVector Ex = (Lead - E).GetSafeNormal();
		const double Ang = FMath::Acos(FMath::Clamp(FVector::DotProduct(Ex, D), -1.0, 1.0));
		if (Ang > BestAng) continue;
		BestAng = Ang;
		const double BL = FVector::Dist(Lead, E);
		const FVector Nudged = (D + (Ex - D) * AssistStrength).GetSafeNormal();
		Aim = (E + Nudged * BL - Muzzle).GetSafeNormal();
	}
	const FVector Dir = SpreadDir(Aim, Spread());
	FBullet B;
	B.P = Muzzle; B.V = Dir * MuzzleSpeed; B.V0 = B.VEntry = MuzzleSpeed; B.LegStart = Muzzle;
	Bullets.Add(B);
	if (Au) Au->PlayExtra(TEXT("Gunshot"), FVector::ZeroVector, 1);
	// brass out of the port, to the right
	Brass.Add({Muzzle - D * .35 - Rt * .05 + FVector(0, .05, 0), -Rt * 2.2 + FVector(0, 2.4, 0) - D * .4 + FVector(FMath::FRandRange(-.3, .3), 0, FMath::FRandRange(-.3, .3)), 0});
	// the report: fish bolt under the shooter and along the shot, birds in earshot or close to the line scatter
	const FVector Far = E + Dir * 120;
	Report(E, nullptr);
	TArray<FKeysBirdTarget> Bs;
	if (AKeysLife* L = AKeysLife::Get(this))
	{
		L->ShootableBirds(E.X, E.Z, ReportBirdM, Bs);
		for (const FKeysBirdTarget& Bt : Bs)
			if (FMath::FRand() < ScatterChance(FVector::Dist(Bt.Pos, E), DistToSeg(Bt.Pos, E, Far)) * .35) L->DownBird(Bt.Ref, BirdAway(Bt.Kind, false));
	}
	const double Now = GetWorld()->GetTimeSeconds();
	for (FGull& G : Gulls)
		if (G.DownUntil < Now && FMath::FRand() < ScatterChance(FVector::Dist(G.Pos, E), DistToSeg(G.Pos, E, Far)) * .25) G.DownUntil = Now + BirdAway(G.Kind, false);
	if (Mag == 0 && Reserve() == 0) Flash(TEXT("Out of 9 mm. Deadeye Guns & Ammo sells it by the box."));
	else if (Mag <= 3 && Reserve() == 0) Flash(FString::Printf(TEXT("%d left and no spare magazine."), Mag));
}

void AKeysHandgun::Report(const FVector& From, const FVector* Impact)
{
	AKeysLife* L = AKeysLife::Get(this);
	if (!L) return;
	L->SpookAt(From.X, From.Z);
	if (!Impact) return;
	L->SpookAt(Impact->X, Impact->Z);
	const double DX = Impact->X - From.X, DZ = Impact->Z - From.Z, D = FMath::Sqrt(DX * DX + DZ * DZ);
	for (double S = 9; S < D - 4; S += 9) L->SpookAt(From.X + DX / D * S, From.Z + DZ / D * S);
}

void AKeysHandgun::FireFlare()
{
	AKeysBoatPawn* P = Pawn();
	if (!P) return;
	if (!P->UseOne(FlareKey)) { P->ToastMsg(TEXT("No flares. Deadeye Guns & Ammo sells a marine flare kit (three for $38).")); return; }
	P->SaveEconomy();
	FVector E, D;
	Eye(E, D);
	// flareLaunch: never flatter than the minimum pitch
	double Dx = D.X, Dy = D.Y, Dz = D.Z;
	const double Hz = FMath::Max(1e-6, FMath::Sqrt(Dx * Dx + Dz * Dz));
	if (FMath::Atan2(Dy, Hz) < FlareMinPitch) { const double C = FMath::Cos(FlareMinPitch), S = FMath::Sin(FlareMinPitch); Dx = Dx / Hz * C; Dz = Dz / Hz * C; Dy = S; }
	FlaresUp.Add({E + D * .5, FVector(Dx, Dy, Dz) * FlareSpeed, 0});
	if (AKeysAudio* A = AKeysAudio::Get(this)) A->PlayExtra(TEXT("Flare"), FVector::ZeroVector, 1);
	const int32 Left = P->CountOf(FlareKey);
	P->ToastMsg(Left > 0 ? FString::Printf(TEXT("Flare away. %d left."), Left) : FString(TEXT("Flare away. That was the last one.")));
}

FVector AKeysHandgun::FlareAt(const FFlare& F, double& OutGlow, bool& bOutOut) const
{
	const FVector Wind(1.6, 0, .9);
	OutGlow = 0; bOutOut = false;
	if (F.T < FlareIgniteS) return F.O + F.V * F.T - FVector(0, .5 * Gravity * F.T * F.T, 0);
	const FVector P0 = F.O + F.V * FlareIgniteS - FVector(0, .5 * Gravity * FlareIgniteS * FlareIgniteS, 0);
	const FVector V0 = F.V - FVector(0, Gravity * FlareIgniteS, 0);
	const double Tau = F.T - FlareIgniteS, E = (1 - FMath::Exp(-FlareDrag * Tau)) / FlareDrag;
	const FVector P(P0.X + Wind.X * Tau + (V0.X - Wind.X) * E, P0.Y - FlareFallMs * Tau + (V0.Y + FlareFallMs) * E, P0.Z + Wind.Z * Tau + (V0.Z - Wind.Z) * E);
	if (Tau >= FlareBurnS || P.Y <= 0) { bOutOut = true; return P; }
	OutGlow = FMath::Min(1.0, Tau / .15) * FMath::Min(1.0, (FlareBurnS - Tau) / 3);
	return P;
}

// ================================================================ targets and bullets
void AKeysHandgun::GatherTargets(const FVector& E, TArray<FTgt>& Out) const
{
	const double T = GetWorld()->GetTimeSeconds();
	if (AKeysLife* L = AKeysLife::Get(this))
	{
		TArray<KeysDive::FTarget> Fish;
		L->SpearTargetsNear(E.X, 0, E.Z, AssistRange + 10, Fish);
		for (const KeysDive::FTarget& F : Fish)
		{
			if (!F.bCatchable) continue;
			// out of the bullet's reach under the water, not offered (fishWithinReach)
			const double Top = FMath::Max(F.A.Y, F.B.Y) + F.Radius;
			if (SurfaceY(F.A.X, F.A.Z, T) - Top > AssistMaxDepth) continue;
			Out.Add({0, F.Id, F.Key, F.Weight, F.A, F.B, F.Vel, F.Radius, nullptr, -1});
		}
		TArray<FKeysBirdTarget> Bs;
		L->ShootableBirds(E.X, E.Z, AssistRange + 30, Bs);
		for (const FKeysBirdTarget& B : Bs)
			Out.Add({1, 0, BirdName(B.Kind), 0, B.Pos + FVector(.3, 0, 0), B.Pos - FVector(.3, 0, 0), FVector::ZeroVector, BirdRadius(B.Kind), B.Ref, -1});
	}
	for (int32 i = 0; i < Gulls.Num(); i++)
	{
		const FGull& G = Gulls[i];
		if (G.DownUntil > T) continue;
		Out.Add({1, 0, BirdName(G.Kind), 0, G.Pos + FVector(.3, 0, 0), G.Pos - FVector(.3, 0, 0), FVector::ZeroVector, BirdRadius(G.Kind), nullptr, i});
	}
}

void AKeysHandgun::StepBullets(double Dt)
{
	if (!Bullets.Num()) return;
	const double T = GetWorld()->GetTimeSeconds();
	FVector E, D;
	Eye(E, D);
	TArray<FTgt> Tg;
	GatherTargets(E, Tg);
	AKeysAudio* Au = AKeysAudio::Get(this);
	AKeysLife* L = AKeysLife::Get(this);
	AKeysExtras* Ex = AKeysExtras::Get(this);
	for (FBullet& B : Bullets)
	{
		if (!B.bAlive) continue;
		B.Age += Dt;
		double Rem = Dt;
		auto CloseLeg = [&B](const FVector& At) { if (!B.bInWater) B.Legs.Add(TPair<FVector, FVector>(B.LegStart, At)); };
		while (Rem > 1e-9 && B.bAlive)
		{
			const double H = FMath::Min(MicroDt, Rem);
			Rem -= H;
			const FVector P1 = B.P;
			const double Spd = B.V.Size();
			FVector P2;
			double SegLen;
			if (B.bInWater)
			{
				// linear deceleration to a stop over WATER_STOP_M of path
				const double Fr = FMath::Max(0.0, 1 - B.WaterPath / WaterStopM), Sp = B.VEntry * Fr;
				SegLen = FMath::Min(Sp * H, WaterStopM - B.WaterPath);
				P2 = P1 + (Spd > 0 ? B.V / Spd : FVector::ZeroVector) * SegLen;
			}
			else
			{
				B.V.Y -= Gravity * H;
				SegLen = B.V.Size() * H;
				P2 = P1 + B.V * H;
				B.V *= FMath::Exp(-DragK * SegLen);
			}
			// the surface crossing, air to water
			FVector Seg2 = P2;
			bool bCrossed = false;
			if (!B.bInWater)
			{
				const double A0 = P1.Y - SurfaceY(P1.X, P1.Z, T), A1 = P2.Y - SurfaceY(P2.X, P2.Z, T);
				if (A0 > 0 && A1 <= 0) { const double F = A0 / (A0 - A1); Seg2 = P1 + (P2 - P1) * F; SegLen *= F; bCrossed = true; }
			}
			// Caelen, tested as the round passes him (he is never an aim-assist target): a hit counts, a near miss is
			// announced at most once per round
			if (!B.bInWater && !B.bCaelenHit && Ex)
			{
				FVector CA, CB;
				double CR, S0 = 0;
				if (Ex->CaelenHitbox(CA, CB, CR))
				{
					const double D2 = KeysDive::ClosestDistSqSegSeg(P1, Seg2, CA, CB, S0);
					if (D2 <= CR * CR)
					{
						B.bCaelenHit = true;
						TallyCaelen++;
						HitFlash = .25;
						Ex->CaelenShot(true);
					}
					else if (!B.bCaelenTaunt && D2 <= FMath::Square(CR + AKeysExtras::CaelenNearMiss)) { B.bCaelenTaunt = true; Ex->CaelenShot(false); }
				}
			}
			// the swept test along P1 -> Seg2
			const FTgt* Best = nullptr;
			double BestS = 2;
			for (const FTgt& Tt : Tg)
			{
				double S = 0;
				const double D2 = KeysDive::ClosestDistSqSegSeg(P1, Seg2, Tt.A, Tt.B, S);
				if (D2 <= Tt.R * Tt.R && S < BestS) { BestS = S; Best = &Tt; }
			}
			if (Best)
			{
				const FVector Pt = P1 + (Seg2 - P1) * BestS;
				const double Wp = B.bInWater ? B.WaterPath + SegLen * BestS : 0;
				const double Rem2 = FMath::Max(0.0, 1 - Wp / WaterStopM);
				const double Energy = B.bInWater ? Rem2 * Rem2 * FMath::Square(B.VEntry / B.V0) : FMath::Square(B.V.Size() / B.V0);
				if (!B.bInWater || Energy >= EnergyStun * FMath::Square(B.VEntry / B.V0))
				{
					B.P = Pt; B.bAlive = false;
					CloseLeg(Pt);
					OnHit(*Best, Pt);
					break;
				}
			}
			B.Travelled += SegLen;
			if (B.bInWater) B.WaterPath += SegLen;
			if (bCrossed)
			{
				B.P = Seg2;
				const double Sp = FMath::Max(1e-6, B.V.Size()), Theta = FMath::Asin(FMath::Min(1.0, FMath::Abs(B.V.Y) / Sp));
				if (Theta < SkipAngle && B.Skips < 1)
				{
					// a skip: mirror the vertical, keep most of the horizontal
					B.V.Y = FMath::Abs(B.V.Y) * .6 + 2; B.V.X *= SkipKeep; B.V.Z *= SkipKeep; B.Skips++;
					CloseLeg(B.P); B.LegStart = B.P;
					if (Au) Au->PlayExtra(TEXT("Ricochet"), B.P, 1);
					if (L) L->Splash(B.P.X, B.P.Z, 4, .4);
				}
				else
				{
					CloseLeg(B.P);
					if (Theta < SkipAngle) B.bAlive = false;
					else { B.bInWater = true; B.WaterPath = 0; B.VEntry = Sp; }
					if (L) L->Splash(B.P.X, B.P.Z, 7, .6);
					if (Au) Au->PlayExtra(TEXT("Plink"), B.P, .4f);
					Report(E, &B.P);
				}
				continue;
			}
			B.P = P2;
			if (B.bInWater && B.WaterPath >= WaterStopM - 1e-9) B.bAlive = false;
			else if (LandH(B.P.X, B.P.Z) > 0 && B.P.Y < LandH(B.P.X, B.P.Z)) { B.bAlive = false; CloseLeg(B.P); if (Au) Au->PlayExtra(TEXT("Thud"), B.P, 1); }
			else if (B.Travelled >= BulletRange) { B.bAlive = false; CloseLeg(B.P); }
		}
		if (B.bAlive && !B.bInWater) Streaks.Add({B.LegStart, B.P, .0});
	}
	for (const FBullet& B : Bullets) if (!B.bAlive) for (const TPair<FVector, FVector>& Lg : B.Legs) Streaks.Add({Lg.Key, Lg.Value, .0});
	Bullets.RemoveAll([](const FBullet& B) { return !B.bAlive; });
}

void AKeysHandgun::OnHit(const FTgt& T, const FVector& Pt)
{
	AKeysBoatPawn* P = Pawn();
	AKeysLife* L = AKeysLife::Get(this);
	AKeysAudio* Au = AKeysAudio::Get(this);
	HitFlash = .25;
	const double Now = GetWorld()->GetTimeSeconds();
	if (T.Kind == 0)
	{
		TallyFish++;
		if (L) { L->Splash(Pt.X, Pt.Z, 12, .9); L->TakeSpeared(T.Id); L->SpookAt(Pt.X, Pt.Z); }
		if (Au) Au->PlayExtra(TEXT("Plink"), Pt, 1);
		const FVector D = FVector(Pt.X, 0, Pt.Z) - FVector(P ? P->BoatX() : 0, 0, P ? P->BoatZ() : 0);
		const FVector Dn = D.GetSafeNormal();
		Floaters.Add({T.Key, T.Weight, Pt.X, Pt.Z, 0, FMath::Atan2(Dn.Z, Dn.X), Dn.X, Dn.Z});
		const KeysData::FSpecies* Sp = KeysData::Species(T.Key);
		const FString Name = Sp ? Sp->Name : FString(TEXT("fish"));
		if (P) P->ToastMsg(bFirstFish ? FString::Printf(TEXT("Plink! %s, %.1f lb."), *Name, T.Weight)
			: FString::Printf(TEXT("Plink! %s, %.1f lb - it bobs up stunned, wakes offended and swims off. (Nothing to land it with: the pistol is for plinking.)"), *Name, T.Weight), 4.5);
		bFirstFish = true;
	}
	else
	{
		TallyBirds++;
		uint8 Kind = 0;
		if (T.Bird) { Kind = T.Bird->Kind; if (L) L->DownBird(T.Bird, BirdReplaceS); }
		if (T.Gull >= 0) { Kind = Gulls[T.Gull].Kind; Gulls[T.Gull].DownUntil = Now + BirdReplaceS; }
		Falls.Add({Pt, FVector(FMath::FRandRange(-1.0, 1.0), 1.5, FMath::FRandRange(-1.0, 1.0)), 0, Kind});
		if (Au) Au->PlayExtra(TEXT("Squawk"), Pt, 1);
		if (!bFirstBird || TallyBirds % 5 == 0)
		{
			bFirstBird = true;
			if (P) P->ToastMsg(FString::Printf(TEXT("Squawk! The %s drops into the water and lies there. Something big will be along for it shortly."), BirdName(Kind)), 4.5);
		}
	}
}

// ================================================================ the harbour gulls (boat-gulls.ts)
void AKeysHandgun::UpdateGulls(double Dt, double T)
{
	AKeysBoatPawn* P = Pawn();
	if (!P) return;
	if (!bGullInit) { GCX = P->BoatX(); GCZ = P->BoatZ(); bGullInit = true; }
	const double K = FMath::Min(1.0, Dt * (.35 + FMath::Min(1.0, FMath::Abs(P->GetSpeed()) / 14) * .5));
	GCX += (P->BoatX() - GCX) * K; GCZ += (P->BoatZ() - GCZ) * K;
	if (P->IsAshore()) { GCX = P->BoatX() + 2000; }   // they stay with the water, not the town
	for (FGull& G : Gulls)
	{
		const double A = T * G.Sp + G.Ph;
		G.Pos = FVector(GCX + FMath::Cos(A) * G.R, G.H + FMath::Sin(T * 1.1 + G.Ph) * .9, GCZ + FMath::Sin(A) * G.R);
	}
}

// ================================================================ tick and render
void AKeysHandgun::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	const double Dt = FMath::Min(double(DeltaSeconds), .1), T = GetWorld()->GetTimeSeconds();
	AKeysBoatPawn* P = Pawn();
	if (bOut && P)
	{
		FString Why;
		if (!P->bRunning || P->IsBusy(Why)) { bOut = false; ViewModel->SetVisibility(false); }
	}
	FireT = FMath::Max(0.0, FireT - Dt);
	Heat = FMath::Max(0.0, Heat - HeatDecay * Dt);
	Kick = FMath::Max(0.0, Kick - Dt * 9);
	MuzzleT -= Dt; SlideT -= Dt; HitFlash -= Dt; NoteT -= Dt;
	if (bReloading)
	{
		ReloadT -= Dt;
		if (ReloadT <= 0)
		{
			bReloading = false;
			const int32 Total = P ? P->CountOf(AmmoKey) : 0;
			Mag = FMath::Min(MagSize, Total);
		}
	}
	if (P)
	{
		MagSize = MagBase + (P->HasGear(TEXT("handgun.extmag")) ? 6 : 0);
		Mag = FMath::Min(Mag, P->CountOf(AmmoKey));
	}
	StepBullets(Dt);
	UpdateGulls(Dt, T);
	for (FStreak& S : Streaks) S.T += Dt;
	Streaks.RemoveAll([](const FStreak& S) { return S.T > .12; });
	for (FFloater& F : Floaters) F.T += Dt;
	Floaters.RemoveAll([](const FFloater& F) { return F.T > 9; });
	for (FBrass& B : Brass) { B.T += Dt; B.V.Y -= Gravity * Dt; B.P += B.V * Dt; }
	Brass.RemoveAll([](const FBrass& B) { return B.T > 1.2; });
	for (FFall& F : Falls)
	{
		F.T += Dt;
		F.V.Y -= Gravity * Dt;
		F.P += F.V * Dt;
		if (F.P.Y <= SurfaceY(F.P.X, F.P.Z, T))
		{
			Carcasses.Add({F.P.X, F.P.Z, 0});
			if (AKeysLife* L = AKeysLife::Get(this)) L->Splash(F.P.X, F.P.Z, 8, .7);
			F.T = 99;
		}
	}
	Falls.RemoveAll([](const FFall& F) { return F.T > 20; });
	for (FCarcass& C : Carcasses) C.Age += Dt;
	Carcasses.RemoveAll([](const FCarcass& C) { return C.Age > CarcassLifeS + SinkS; });
	if (Carcasses.Num() > 4) Carcasses.RemoveAt(0);
	for (FFlare& F : FlaresUp) F.T += Dt;
	FlaresUp.RemoveAll([this](const FFlare& F) { double G; bool bO; FlareAt(F, G, bO); return bO; });
	Render(T);
}

void AKeysHandgun::Render(double T)
{
	// the view model: in the right hand, low in the view, kicking back on a shot
	FVector CamPos; FRotator CamRot; float Fov;
	if (CameraFor(CamPos, CamRot, Fov))
	{
		const AKeysBoatPawn* P = Pawn();
		const bool bDot = P && P->HasGear(TEXT("handgun.dot"));
		if (BuiltMagSize != MagSize || bBuiltDot != bDot || SlideT > -.2)
		{
			BuiltMagSize = MagSize; bBuiltDot = bDot;
			FKeysMesh M;
			const FLinearColor Steel = Hex(0x2a2d31), Frame = Hex(0x1b1d20), Grip = Hex(0x3a3328), Sight = Hex(0xd8d8d0);
			const double Back = SlideT > 0 ? .025 : 0;
			M.AddBox(FVector(.02 - Back, 0, .0), FVector(.19, .028, .034), Steel);
			M.AddBox(FVector(-.01, 0, -.03), FVector(.16, .026, .026), Frame);
			M.AddBox(FVector(-.065, 0, -.085), FVector(.04, .027, .11 + (MagSize > MagBase ? .03 : 0)), Grip, FQuat(FVector::YAxisVector, -.28));
			M.AddBox(FVector(.0, 0, -.052), FVector(.05, .006, .02), Frame);
			M.AddBox(FVector(.11 - Back, 0, .02), FVector(.008, .004, .008), Sight);
			M.AddBox(FVector(-.07 - Back, 0, .02), FVector(.008, .02, .008), Sight);
			if (bDot) { M.AddBox(FVector(-.01, 0, .042), FVector(.05, .026, .03), Hex(0x15171a)); M.AddBox(FVector(.016, 0, .042), FVector(.002, .018, .02), Hex(0xff2a2a)); }
			// the hand round the grip
			M.AddBox(FVector(-.07, 0, -.07), FVector(.06, .05, .075), Hex(0xd9a27f), FQuat(FVector::YAxisVector, -.28));
			M.AddBox(FVector(-.13, -.01, -.13), FVector(.16, .065, .07), Hex(0xd9a27f), FQuat(FVector::YAxisVector, -.5));
			M.Commit(ViewModel, 0, AKeysWorld::VertexColorMaterial());
		}
		const FQuat Q = CamRot.Quaternion();
		const FVector Off(40 - Kick * 4, 13, -10.5 + Kick * 1.5);
		ViewModel->SetWorldLocationAndRotation(CamPos + Q.RotateVector(Off), (Q * FQuat(FVector::YAxisVector, -Kick * .25)).Rotator());
	}
	// solid bits in the world: brass, the stunned fish, falling birds, carcasses, the harbour gulls
	FKeysMesh S;
	for (const FBrass& B : Brass)
	{
		const FVector P = ToUE(B.P.X, B.P.Y, B.P.Z) / 100.0;
		S.AddTube(P, P + FVector(.02, .006, .004), .0045, .0045, 5, Hex(0xc9a24a));
	}
	for (const FFloater& F : Floaters)
	{
		// belly up and bobbing for a few seconds, then it rights itself and swims off, offended
		const double Wake = FMath::Clamp((F.T - 4) / 1.2, 0.0, 1.0), Away = FMath::Max(0.0, F.T - 5.2);
		const double X = F.X + F.DX * Away * Away * 1.2, Z = F.Z + F.DZ * Away * Away * 1.2;
		const double Y = SurfaceY(X, Z, T) - .05 - Away * Away * .4 + FMath::Sin(T * 3) * .03;
		const FQuat Q = FQuat(FVector::ZAxisVector, F.Yaw) * FQuat(FVector::XAxisVector, PI * (1 - Wake));
		KeysKit::AppendFish(S, F.Key, FMath::Clamp(KeysMesh::FishLength(F.Weight), .3, 2.5), FTransform(Q, ToUE(X, Y, Z) / 100.0));
	}
	for (const FFall& F : Falls) if (F.T < 20) BirdShape(S, ToUE(F.P.X, F.P.Y, F.P.Z) / 100.0, F.T * 5, 1.3, F.Kind, 0, F.T * 7);
	for (const FCarcass& C : Carcasses)
	{
		const double Sink = FMath::Max(0.0, C.Age - CarcassLifeS) / SinkS;
		BirdShape(S, ToUE(C.X, SurfaceY(C.X, C.Z, T) + .03 - Sink * .6, C.Z) / 100.0, C.X, 1.3, 0, 0, 1.4);
	}
	{
		APlayerCameraManager* CM = UGameplayStatics::GetPlayerCameraManager(this, 0);
		const FVector CL = CM ? CM->GetCameraLocation() : FVector::ZeroVector;
		for (const FGull& G : Gulls)
		{
			if (G.DownUntil > T) continue;
			const FVector P = ToUE(G.Pos.X, G.Pos.Y, G.Pos.Z);
			if (FVector::Dist(P, CL) > 25000) continue;
			const double A = T * G.Sp + G.Ph;
			const double Yaw = FMath::Atan2(FMath::Cos(A) * G.Sp, -FMath::Sin(A) * G.Sp);
			BirdShape(S, P / 100.0, Yaw, 1.3, G.Kind, FMath::Sin(T * .7 + G.Ph) > -.1 ? T * 9 + G.Ph : .6, -FMath::Sign(G.Sp) * .25);
		}
	}
	S.Commit(Solid, 0, AKeysWorld::VertexColorMaterial());
	// glow: tracers, the muzzle flash and the flares
	FKeysMesh Gm;
	for (const FStreak& Sk : Streaks)
	{
		const int32 From = Gm.V.Num();
		const FVector A = ToUE(Sk.A.X, Sk.A.Y, Sk.A.Z) / 100.0, B = ToUE(Sk.B.X, Sk.B.Y, Sk.B.Z) / 100.0;
		Gm.AddTube(A + (B - A) * FMath::Max(0.0, 1 - Sk.T * 8 - .6), B, .012, .02, 4, FLinearColor(1.f, .85f, .5f, float(1 - Sk.T / .12)));
		FlatGlow(Gm, From);
	}
	if (MuzzleT > 0 && bOut)
	{
		const FQuat Q = CamRot.Quaternion();
		Gm.AddGlowBlob((CamPos + Q.RotateVector(FVector(68, 14, -11))) / 100.0, .09, FLinearColor(1.f, .75f, .35f, 1.f));
	}
	double Best = 0;
	FVector BestP = FVector::ZeroVector;
	for (const FFlare& F : FlaresUp)
	{
		double G; bool bO;
		const FVector P = FlareAt(F, G, bO);
		const double Fl = G * (.85 + .15 * FMath::Sin(T * 23 + F.O.X) * FMath::Sin(T * 7.3));
		const FVector U = ToUE(P.X, P.Y, P.Z) / 100.0;
		if (G <= 0) Gm.AddGlowBlob(U, .25, FLinearColor(1.f, .6f, .4f, 1.f));
		else { Gm.AddGlowBlob(U, 1.1 + Fl * .6, FLinearColor(1.f, .35f, .25f, 1.f)); Gm.AddGlowBlob(U, .5, FLinearColor(1.f, .9f, .8f, 1.f)); Gm.AddGlowBlob(U, 8 * Fl, FLinearColor(1.f, .25f, .2f, float(.5 * Fl))); }
		if (Fl > Best) { Best = Fl; BestP = ToUE(P.X, P.Y, P.Z); }
	}
	if (Gm.V.Num()) { Gm.Commit(Glow, 0, GlowMID ? (UMaterialInterface*)GlowMID : AKeysWorld::GlowMaterial()); Glow->SetVisibility(true); }
	else Glow->SetVisibility(false);
	FlareLight->SetVisibility(Best > 0);
	if (Best > 0) { FlareLight->SetWorldLocation(BestP); FlareLight->SetIntensity(float(400000 * Best)); }
}

void AKeysHandgun::DebugFireAt(const FVector& G, bool bFire)
{
	if (!bOut) Toggle();
	const AKeysBoatPawn* P = Pawn();
	if (!P) return;
	FVector E, D;
	Eye(E, D);
	const FVector V = G - E;
	LookYaw = FMath::Atan2(-V.X, -V.Z) - P->Heading();
	LookPitch = FMath::Atan2(V.Y, FMath::Sqrt(V.X * V.X + V.Z * V.Z)) + .002 * FMath::Sqrt(V.X * V.X + V.Z * V.Z) * .1;
	if (bFire) { FireT = 0; Fire(); }
}

void AKeysHandgun::DebugFire(double Yaw, double Pitch)
{
	if (!bOut) Toggle();
	LookYaw = Yaw; LookPitch = Pitch;
	FireT = 0;
	Fire();
}
