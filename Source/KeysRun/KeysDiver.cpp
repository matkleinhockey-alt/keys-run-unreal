#include "KeysDiver.h"
#include "KeysExtras.h"
#include "KeysSchools.h"
#include "KeysBoatPawn.h"
#include "KeysGeo.h"
#include "KeysSea.h"
#include "KeysMesh.h"
#include "KeysLife.h"
#include "KeysWorld.h"
#include "KeysData.h"
#include "KeysAudio.h"
#include "ProceduralMeshComponent.h"
#include "Components/PostProcessComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/Material.h"
#include "MaterialDomain.h"
#include "Kismet/GameplayStatics.h"
#include "Camera/PlayerCameraManager.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "EngineUtils.h"

using namespace KeysGeo;
using namespace KeysDive;

namespace
{
	double DvClamp(double V, double A, double B) { return V < A ? A : V > B ? B : V; }
	double DvLerp(double A, double B, double T) { return A + (B - A) * T; }
	double DvRand(double A, double B) { return FMath::FRandRange(A, B); }
	FLinearColor Hex(uint32 H) { return FLinearColor::FromSRGBColor(FColor(uint8(H >> 16), uint8(H >> 8), uint8(H))); }

	// game meters (x, y up, z) <-> Unreal centimeters
	FVector G2U(const FVector& G) { return FVector(G.X * 100, G.Z * 100, G.Y * 100); }
	FVector U2G(const FVector& U) { return FVector(U.X / 100, U.Z / 100, U.Y / 100); }
	FVector GDir2U(const FVector& G) { return FVector(G.X, G.Z, G.Y); }

	// the boat's yaw convention (forward = -sin, -cos in x/z) as an Unreal yaw in degrees
	double YawDeg(double Yaw) { return FMath::RadiansToDegrees(FMath::Atan2(-FMath::Cos(Yaw), -FMath::Sin(Yaw))); }

	UMaterialInterface* LoadMatOr(const TCHAR* Path)
	{
		UMaterialInterface* M = LoadObject<UMaterialInterface>(nullptr, Path);
		return M ? M : AKeysWorld::VertexColorMaterial();
	}

	// ---- smooth-shaded primitives with the winding Unreal wants (front face: Dot(Cross(B-A, C-A), N) < 0)
	void Tri(FKeysMesh& M, const FVector& A, const FVector& B, const FVector& C, const FVector& NA, const FVector& NB, const FVector& NC, const FLinearColor& Col)
	{
		const FVector Avg = NA + NB + NC;
		const bool bFlip = FVector::DotProduct(FVector::CrossProduct(B - A, C - A), Avg) > 0;
		const int32 I0 = M.AddVert(A, NA, Col);
		const int32 I1 = M.AddVert(bFlip ? C : B, bFlip ? NC : NB, Col);
		const int32 I2 = M.AddVert(bFlip ? B : C, bFlip ? NB : NC, Col);
		M.T.Append({I0, I1, I2});
	}

	// ellipsoid at C with radii R (meters, local frame), Seg around and Seg/2 up
	void Ellipsoid(FKeysMesh& M, const FVector& C, const FVector& R, int32 Seg, const FLinearColor& Col, const FQuat& Q = FQuat::Identity)
	{
		const int32 Rings = FMath::Max(3, Seg / 2);
		auto P = [&](int32 i, int32 j, FVector& OutP, FVector& OutN)
		{
			const double Th = PI * j / Rings, Ph = 2 * PI * i / Seg;
			const FVector U(FMath::Sin(Th) * FMath::Cos(Ph), FMath::Sin(Th) * FMath::Sin(Ph), FMath::Cos(Th));
			OutP = C + Q.RotateVector(U * R);
			OutN = Q.RotateVector(FVector(U.X / R.X, U.Y / R.Y, U.Z / R.Z)).GetSafeNormal();
		};
		for (int32 j = 0; j < Rings; j++)
			for (int32 i = 0; i < Seg; i++)
			{
				FVector A, B, Cc, D, NA, NB, NC, ND;
				P(i, j, A, NA); P(i + 1, j, B, NB); P(i + 1, j + 1, Cc, NC); P(i, j + 1, D, ND);
				if (j > 0) Tri(M, A, B, Cc, NA, NB, NC, Col);
				if (j < Rings - 1) Tri(M, A, Cc, D, NA, NC, ND, Col);
			}
	}

	// smooth tube A -> B with end radii
	void Tube(FKeysMesh& M, const FVector& A, const FVector& B, double RA, double RB, int32 Sides, const FLinearColor& Col, bool bCaps = false)
	{
		const FVector Dir = (B - A).GetSafeNormal();
		if (Dir.IsNearlyZero()) return;
		FVector U = FVector::CrossProduct(Dir, FVector::ZAxisVector);
		if (U.SizeSquared() < 1e-6) U = FVector::CrossProduct(Dir, FVector::YAxisVector);
		U.Normalize();
		const FVector W = FVector::CrossProduct(Dir, U);
		const double Slope = (RA - RB) / FMath::Max(1e-6, (B - A).Size());
		for (int32 i = 0; i < Sides; i++)
		{
			const double A0 = 2 * PI * i / Sides, A1 = 2 * PI * (i + 1) / Sides;
			const FVector O0 = U * FMath::Cos(A0) + W * FMath::Sin(A0), O1 = U * FMath::Cos(A1) + W * FMath::Sin(A1);
			const FVector N0 = (O0 + Dir * Slope).GetSafeNormal(), N1 = (O1 + Dir * Slope).GetSafeNormal();
			Tri(M, A + O0 * RA, B + O0 * RB, B + O1 * RB, N0, N0, N1, Col);
			Tri(M, A + O0 * RA, B + O1 * RB, A + O1 * RA, N0, N1, N1, Col);
			if (bCaps)
			{
				Tri(M, A, A + O1 * RA, A + O0 * RA, -Dir, -Dir, -Dir, Col);
				Tri(M, B, B + O0 * RB, B + O1 * RB, Dir, Dir, Dir, Col);
			}
		}
	}

	void Capsule(FKeysMesh& M, const FVector& A, const FVector& B, double R, int32 Sides, const FLinearColor& Col)
	{
		Tube(M, A, B, R, R, Sides, Col);
		Ellipsoid(M, A, FVector(R), Sides, Col);
		Ellipsoid(M, B, FVector(R), Sides, Col);
	}

	UProceduralMeshComponent* MakePMC(AActor* Owner, USceneComponent* Parent, const TCHAR* Name)
	{
		UProceduralMeshComponent* C = Owner->CreateDefaultSubobject<UProceduralMeshComponent>(Name);
		C->SetupAttachment(Parent);
		C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		return C;
	}

	// the gun (gun-viewmodel.ts): a 78 cm reef gun, in its own frame (meters, +X out the muzzle, Z up)
	constexpr double BarrelLen = .78, BarrelR = .014, ShaftR = .00325, BandR = .008, RailY = BarrelR + ShaftR, ShaftPoke = .3;
	constexpr double BandAnchorX = BarrelLen - .06, BandShort = .3, BandLong = .74;
	const FVector ReadyPos(.62, .26, -.32), LoadPos(.46, .1, -.22);   // camera frame, meters (forward, right, up)

	// view, camera-frame helper: a camera-relative offset (forward, right, up) in game meters
	FVector CamOffset(const FRotator& Rot, const FVector& Off)
	{
		return U2G(Rot.RotateVector(Off * 100));
	}
}

AKeysDiver* AKeysDiver::Get(const UObject* WorldContext)
{
	UWorld* W = WorldContext ? WorldContext->GetWorld() : nullptr;
	if (!W) return nullptr;
	TActorIterator<AKeysDiver> It(W);
	return It ? *It : nullptr;
}

AKeysDiver::AKeysDiver()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickGroup = TG_PostPhysics;   // after the pawn has read the keyboard
	PrimaryActorTick.bTickEvenWhenPaused = false;
	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent = Root;
	Body = MakePMC(this, Root, TEXT("Body"));
	FinL = MakePMC(this, Body, TEXT("FinL"));
	FinR = MakePMC(this, Body, TEXT("FinR"));
	GunBase = MakePMC(this, Root, TEXT("GunBase"));
	GunBandL = MakePMC(this, GunBase, TEXT("GunBandL"));
	GunBandR = MakePMC(this, GunBase, TEXT("GunBandR"));
	GunShaft = MakePMC(this, GunBase, TEXT("GunShaft"));
	GunGlove = MakePMC(this, GunBase, TEXT("GunGlove"));
	Lines = MakePMC(this, Root, TEXT("Lines"));
	FishOnLine = MakePMC(this, Root, TEXT("FishOnLine"));
	Particles = MakePMC(this, Root, TEXT("Particles"));
	Particles->SetCastShadow(false);
	Particles->bUseAsyncCooking = false;
	Lines->SetCastShadow(false);
	Post = CreateDefaultSubobject<UPostProcessComponent>(TEXT("Underwater"));
	Post->SetupAttachment(Root);
	Post->bUnbound = true;
	Post->Priority = 10;
}

void AKeysDiver::BeginPlay()
{
	Super::BeginPlay();
	UMaterialInterface* VC = AKeysWorld::VertexColorMaterial();
	// ---- the diver (model.ts): wetsuit capsule, head, mask, back plate, arms and kicking fins. +X is forward.
	{
		FKeysMesh M;
		const FLinearColor Suit = Hex(0x1b2430), Skin = Hex(0xc98a63), MaskC = Hex(0x0a0a0a), Tank = Hex(0x8a8f94);
		Capsule(M, FVector(-.45, 0, 0), FVector(.45, 0, 0), .22, 10, Suit);
		Ellipsoid(M, FVector(.62, 0, .04), FVector(.15), 10, Skin);
		M.AddBox(FVector(.73, 0, .06), FVector(.05, .2, .09), MaskC);
		Capsule(M, FVector(-.3, 0, .18), FVector(.2, 0, .18), .1, 8, Tank);
		for (int32 Side = -1; Side <= 1; Side += 2)
		{
			const FVector Ctr(.25, Side * .3, .02), Dir(0, Side * .976, .217);
			Capsule(M, Ctr - Dir * .275, Ctr + Dir * .275, .06, 6, Suit);
		}
		M.Commit(Body, 0, VC);
		FKeysMesh F;
		// a flat four-sided cone from the pivot back to the tip
		Tube(F, FVector(0, 0, 0), FVector(-.6, 0, 0), .13, .005, 4, Hex(0x0f766e), true);
		F.Commit(FinL, 0, VC);
		F.Commit(FinR, 0, VC);
		FinL->SetRelativeLocation(FVector(-55, -11, 0));
		FinR->SetRelativeLocation(FVector(-55, 11, 0));
		Body->SetVisibility(false, true);
	}
	// ---- the first-person speargun (gun-viewmodel.ts)
	{
		const FLinearColor Dark = Hex(0x3c4a54), Steel = Hex(0xc7ced4), Rubber = Hex(0x262b2f), Latex = Hex(0xc08a3e);
		FKeysMesh M;
		Tube(M, FVector(0, 0, 0), FVector(BarrelLen, 0, 0), BarrelR, BarrelR, 10, Dark, true);
		Tube(M, FVector(BarrelLen - .006, 0, RailY), FVector(BarrelLen + .006, 0, RailY), .0125, .0125, 10, Steel, true);
		M.AddBox(FVector(-.08, 0, -.11), FVector(.08, .06, .16), Dark, FQuat(FVector::YAxisVector, -.25));
		M.AddBox(FVector(-.1, 0, -.03), FVector(.02, .012, .05), Steel);
		M.Commit(GunBase, 0, VC);
		FKeysMesh B;   // unit band along +X, scaled per frame
		Tube(B, FVector(0, 0, 0), FVector(1, 0, 0), 1, 1, 6, Latex);
		B.Commit(GunBandL, 0, VC);
		B.Commit(GunBandR, 0, VC);
		FKeysMesh Sh;
		Tube(Sh, FVector(-ShaftPoke * .5, 0, 0), FVector(ShaftPoke * .5, 0, 0), ShaftR, ShaftR, 6, Steel, true);
		Sh.Commit(GunShaft, 0, VC);
		FKeysMesh Gl;
		Ellipsoid(Gl, FVector::ZeroVector, FVector(.038, .045, .034), 10, Rubber);
		Capsule(Gl, FVector(.012, .026, .018), FVector(-.018, .026, .018), .012, 6, Rubber);
		Gl.Commit(GunGlove, 0, VC);
		GunBase->SetVisibility(false, true);
		GunBase->SetCastShadow(false);
		GunBandL->SetCastShadow(false); GunBandR->SetCastShadow(false); GunShaft->SetCastShadow(false); GunGlove->SetCastShadow(false);
	}
	// ---- the underwater look: extinction, visibility, lens wetting (world/underwater)
	if (UMaterialInterface* UW = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/KeysRun/Materials/M_KR_Underwater.M_KR_Underwater")))
	{
		PostMID = UMaterialInstanceDynamic::Create(UW, this);
		Post->Settings.AddBlendable(PostMID, 1.f);
		// under water the scene is mostly dim haze, and auto exposure would blow out the lit sand and coral up close
		Post->Settings.bOverride_AutoExposureBias = true;
		Post->Settings.AutoExposureBias = -.35f;
	}
	if (UMaterialInterface* BM = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/KeysRun/Materials/M_KR_Bubble.M_KR_Bubble")))
	{
		BubbleMID = UMaterialInstanceDynamic::Create(BM, this);
		BubbleMID->SetScalarParameterValue(TEXT("Bright"), .55f);
	}
	Bubbles.SetNum(420);
	Muzzle.SetNum(120);
	for (FPuff& P : Muzzle) P.Life = 0;
	Snow.SetNum(900);
	for (FSnow& Sn : Snow)
	{
		Sn.P = FVector(DvRand(-11, 11), DvRand(-11, 11), DvRand(-11, 11));
		Sn.V = FVector((FMath::FRand() - .5) * .035, -.05 - FMath::FRand() * .09, (FMath::FRand() - .5) * .035);
		Sn.Seed = FMath::FRand();
	}
	SeepB.SetNum(8 * 34);
	for (int32 i = 0; i < SeepB.Num(); i++) SeepB[i].Slot = i / 34;
	Seeps.SetNum(8);
	for (FSeep& Sp : Seeps) Sp.Key = MIN_int64;
}

AKeysBoatPawn* AKeysDiver::Pawn() const { return Cast<AKeysBoatPawn>(UGameplayStatics::GetPlayerPawn(this, 0)); }

// ================================================================ controller
void AKeysDiver::EnterWater(double BoatX, double BoatZ, double BoatH)
{
	if (bDiving) return;
	// over the side: offset to starboard so the diver doesn't start inside the hull
	const double RX = FMath::Cos(BoatH), RZ = -FMath::Sin(BoatH);
	S = CreateState(BoatX + RX * 4, -.2, BoatZ + RZ * 4, BoatH, AirMax * AirMul);
	CamYaw = PrevYaw = BoatH; CamPitch = -.15; CamMode = bDefaultChase ? 1 : 0; YawRate = 0;
	bDiving = true; bCanReboard = false; WakingT = 0; Acc = 0;
	Gun = FGun();
	Shot = FShot();
	Speared = FSpeared();
	bDrawing = bDrawLatched = bHauling = false;
	FishMode = 0;
	bPrevSpearActive = bShotHadFight = false;
	LastEqDepth = 0;
	FovV = 58;
	Body->SetVisibility(CamMode == 1, true);
	GunBase->SetVisibility(true, true);
}

bool AKeysDiver::RequestReboard()
{
	if (!bDiving) return false;
	if (!bCanReboard || S.bBlackedOut) return false;
	ExitToBoat();
	return true;
}

void AKeysDiver::ExitToBoat()
{
	bDiving = false; bCanReboard = false; WakingT = 0;
	Shot = FShot();
	if (Speared.bOn) EndFight();
	FishMode = 0;
	FishOnLine->ClearAllMeshSections();
	Lines->ClearAllMeshSections();
	bDrawing = bDrawLatched = bHauling = false;
	Body->SetVisibility(false, true);
	GunBase->SetVisibility(false, true);
	if (AKeysLife* L = AKeysLife::Get(this)) L->SetDiver(false, 0, 0, 0);
}

void AKeysDiver::SetButtons(const FKeysDiverButtons& B)
{
	Btn = B;
	if (B.bFire) bFireQueued = true;
	if (B.bDrawToggle) bDrawQueued = true;
	bHauling = B.bHaul || bDebugHaul;
	if (bDiving && !S.bBlackedOut)
	{
		// mouse look (camera.ts: 0.0022 rad per pixel under the pointer lock)
		CamYaw -= B.LookYaw;
		CamPitch = DvClamp(CamPitch - B.LookPitch, -1.3, 1.3);
	}
}

FString AKeysDiver::CycleView()
{
	CamMode = 1 - CamMode;
	Body->SetVisibility(bDiving && CamMode == 1, true);
	return CamMode == 0 ? TEXT("Dive mask view") : TEXT("Chase view");
}

FString AKeysDiver::AscentWarning() const
{
	if (!bDiving) return FString();
	const double Depth = S.Depth(), Af = AirFrac();
	const double Margin = S.Air - Depth / 1.2;
	if (Af < .12 || Margin < 4) return TEXT("SURFACE NOW - air critical");
	if (Depth < ShallowBlackoutDepth && Af < .18) return TEXT("Shallow-water blackout risk - ascend gently");
	if (Af < .3 && Depth > 3) return TEXT("Air low - start heading up");
	return FString();
}

double AKeysDiver::TunnelAmount() const
{
	if (!bDiving) return 0;
	if (S.bBlackedOut) return 1;
	const double Af = AirFrac();
	return (Af < .25 ? FMath::Min(1.0, (.25 - Af) / .25) : 0) * .85;
}

void AKeysDiver::FixedStep(double Dt, double T)
{
	AKeysBoatPawn* P = Pawn();
	const double BX = P ? P->BoatX() : S.X, BZ = P ? P->BoatZ() : S.Z;
	if (S.bBlackedOut)
	{
		WakingT += Dt;
		FInput In;
		In.LookYaw = CamYaw; In.LookPitch = CamPitch;
		Step(S, In, T, Dt, SwimMul);
		FrameEvents.Append(S.Events);
		if (WakingT >= BlackoutWakeDelay)
		{
			ExitToBoat();
			if (P) P->ShowMsg(TEXT("You wake up on the deck, coughing. The crew hauled you aboard."), 4);
		}
		return;
	}
	FInput In;
	In.bFwd = Btn.bFwd || bDebugSwim; In.bBack = Btn.bBack; In.bLeft = Btn.bLeft; In.bRight = Btn.bRight;
	In.bAscend = Btn.bAscend; In.bDescend = Btn.bDescend; In.bSprint = Btn.bSprint;
	In.LookYaw = CamYaw; In.LookPitch = CamPitch;
	Step(S, In, T, Dt, SwimMul);
	if (DebugHoldDepth >= 0) { S.Y = FMath::Max(-DebugHoldDepth, KeysSea::SeafloorHeightAt(S.X, S.Z) + .6); S.VY = 0; }
	FrameEvents.Append(S.Events);
	bCanReboard = S.Depth() < 1.5 && FVector2D(S.X - BX, S.Z - BZ).Size() < 6;
	UpdateSpear(Dt, T);
}

// ================================================================ speargun
void AKeysDiver::GetTargets(TArray<FTarget>& Out) const
{
	Out.Reset();
	if (const AKeysLife* L = AKeysLife::Get(this)) L->SpearTargetsNear(S.X, S.Y, S.Z, KeysDive::SpearRange + 6 + 12, Out);
}

FVector AKeysDiver::EyeWorld() const { return U2G(CamPosUE); }

FVector AKeysDiver::MuzzleWorld() const
{
	// before the first frame of the view model, fall back to a point just in front of the mask
	return MuzzleG.IsZero() ? EyeWorld() : MuzzleG;
}

bool AKeysDiver::TryFire()
{
	if (Shot.bAlive || Speared.bOn) return false;
	if (!CanFire(Gun)) { Nudge = .6; return false; }
	if (S.Air <= 0 || S.bBlackedOut) return false;
	const FVector Origin = MuzzleWorld();
	const FVector Eye = EyeWorld();
	const double Cp = FMath::Cos(CamPitch);
	const FVector Aim(-FMath::Sin(CamYaw) * Cp, FMath::Sin(CamPitch), -FMath::Cos(CamYaw) * Cp);
	TArray<FTarget> Tg;
	GetTargets(Tg);
	int32 Lock;
	// spend the shaft first, so the lead is solved for the range of the one that actually flies (a spare is short)
	bool bSpare = false;
	if (TakeShaft)
	{
		const int32 Left = TakeShaft();
		bSpare = Left < 0;
	}
	ShotRange = FMath::Clamp((SpearRange + FMath::Max(0.0, SpearRangeBonusM)) * (bSpare ? .6 : 1.0), SpearRange * .6, SpearRange + 6);
	const FVector Dir = AssistAim(Origin, Aim, Tg, Lock, AssistStrength, ShotRange, &Eye);
	Shot = Fire(Origin, Dir, ShotRange);
	// a shot at Caelen circling overhead (web caelen.ts resolveSpearShot): the shaft's whole path against his capsule
	if (AKeysExtras* Ex = AKeysExtras::Get(this))
	{
		FVector CA, CB; double CR;
		if (Ex->CaelenHitbox(CA, CB, CR))
		{
			FVector P1, P2;
			FMath::SegmentDistToSegmentSafe(Origin, Origin + Dir * ShotRange, CA, CB, P1, P2);
			const double D = (P1 - P2).Size();
			if (D <= CR) Ex->CaelenShot(true);
			else if (D <= CR + AKeysExtras::CaelenNearMiss) Ex->CaelenShot(false);
		}
	}
	Gun = StartReload();
	KickT = 1;
	KickYaw = (FMath::FRand() - .5) * .05;
	KickRoll = (FMath::FRand() - .5) * .07;
	// a puff of air out of the muzzle as the bands snap forward
	const int32 N = 14 + FMath::RandRange(0, 7);
	for (int32 i = 0; i < N; i++)
	{
		FPuff& Pf = Muzzle[MuzCursor];
		MuzCursor = (MuzCursor + 1) % Muzzle.Num();
		Pf.P = Origin;
		Pf.V = FVector(Dir.X * DvRand(1.2, 2.6) + DvRand(-.35, .35), Dir.Y * DvRand(1.2, 2.6) + DvRand(-.35, .35) + .6, Dir.Z * DvRand(1.2, 2.6) + DvRand(-.35, .35));
		Pf.Life = Pf.Max = DvRand(.35, .8);
	}
	return true;
}

void AKeysDiver::EndFight()
{
	Speared.bOn = false;
}

void AKeysDiver::UpdateSpear(double Dt, double T)
{
	AKeysBoatPawn* P = Pawn();
	const FVector2D Water = KeysSea::CurrentAt(S.X, S.Z, T, S.Depth());
	KickSpeed = FMath::Max(0.0, S.Speed() - .75 * Water.Size());
	if (bDrawQueued)
	{
		bDrawQueued = false;
		bDrawLatched = !bDrawLatched;
		bDrawing = bDrawLatched;
	}
	if (bFireQueued)
	{
		bFireQueued = false;
		const bool bWasOut = Shot.bAlive;
		TryFire();
		// the gun's thump: the school fish around bolt (the fish the shaft passes are spooked when it lands)
		if (!bWasOut && Shot.bAlive) if (AKeysSchools* Sc = AKeysSchools::Get(this)) Sc->Startle(S.X, S.Z, 8);
	}
	// the latch lets go by itself once the gun is seated, or when the hands are busy
	if (bDrawLatched && (CanFire(Gun) || !HandsFree())) { bDrawLatched = false; bDrawing = false; }
	StepReload(Gun, Dt, bDrawing && HandsFree(), KickSpeed, SpearDrawMul);
	if (Nudge > 0) Nudge = FMath::Max(0.0, Nudge - Dt);

	if (Shot.bAlive)
	{
		TArray<FTarget> Tg;
		GetTargets(Tg);
		FHit Hit;
		if (StepSpear(Shot, Tg, Dt, Hit))
		{
			const FTarget* Target = Tg.FindByPredicate([&](const FTarget& X) { return X.Id == Hit.Id; });
			if (Target && Hit.bHeld)
			{
				Speared.bOn = true;
				Speared.Target = *Target;
				Speared.P = FightParamsFor(Target->Key, Target->Weight);
				Speared.P.TetherMax = FMath::Max(4.0, Speared.P.TetherMax * SpearTetherMul);   // the reel conversion: it runs less far
				Speared.F = StartFight(Hit.Point.X, Hit.Point.Z);
				const KeysData::FSpecies* Sp = KeysData::Species(Target->Key);
				const double SizeT = Sp ? DvClamp((Target->Weight - Sp->Min) / FMath::Max(1e-6, Sp->Max - Sp->Min), 0, 1) : .5;
				Speared.Label = SizeT > .7 ? TEXT("Something heavy is on the spear") : SizeT > .35 ? TEXT("Solid fish on the spear") : TEXT("Fish on the spear");
				if (AKeysLife* L = AKeysLife::Get(this)) L->TakeSpeared(Target->Id);
				// the fish on the line (speared-fish-visual.ts)
				FKeysMesh M;
				const double Scale = Sp ? 1.35 * DvClamp(FMath::Pow(Target->Weight / ((Sp->Min + Sp->Max) / 2), 1.0 / 3), .75, 1.5) : 1.35;
				if (!AKeysLife::BuildCreatureMesh(Target->Key, M, Scale))
					KeysMesh::BuildFish(M, KeysMesh::FishLength(Target->Weight), Sp ? Sp->Color : FLinearColor::Gray, false);
				M.SmoothNormals();
				M.Commit(FishOnLine, 0, AKeysWorld::VertexColorMaterial());
				FishMode = 1;
				FishPos = FishTgt = Hit.Point;
				FishHead = 0; FishTension = .5; FishStam = 1;
				if (P) P->ShowMsg(Speared.Label + TEXT(" - hold S to haul it in, ease off if the bar runs red."), 3.5);
			}
			else if (Target && P) P->ShowMsg(TEXT("The shot doesn't have enough force left to hold - it glances off."));
			Shot.bAlive = false;
		}
	}

	if (Speared.bOn)
	{
		if (S.bBlackedOut)
		{
			if (P) P->ShowMsg(TEXT("Blacked out - the shaft tears free. You lose the fish, and the gear's lucky to float back to you."), 4);
			FishMode = 2; FleeT = 0;
			FleeDir = FVector(FishTgt.X - S.X, 0, FishTgt.Z - S.Z).GetSafeNormal();
			EndFight();
		}
		else
		{
			StepFight(Speared.F, bHauling, Speared.P, S.X, S.Z, Dt);
			const FFight& F = Speared.F;
			FishTgt = FVector(F.X, S.Y - .3, F.Z);
			FishTension = F.Tension; FishStam = F.Stam;
			if (F.Outcome == EFight::Landed)
			{
				if (P) P->LandSpeared(Speared.Target.Key, Speared.Target.Weight, F.X, F.Z);
				FishMode = 0;
				FishOnLine->ClearAllMeshSections();
				EndFight();
			}
			else if (F.Outcome == EFight::TornFree)
			{
				if (P) P->ShowMsg(TEXT("The shaft tears free - it got away."));
				FishMode = 2; FleeT = 0;
				FleeDir = FVector(FishTgt.X - S.X, 0, FishTgt.Z - S.Z).GetSafeNormal();
				EndFight();
			}
		}
	}

	// a clean miss spooks the fish the shaft hissed past (world.ts: SPEAR_NEAR_MISS_MARGIN)
	const bool bActive = Shot.bAlive || Speared.bOn;
	if (bActive && !bPrevSpearActive) { bShotHadFight = false; ShotO = Shot.O; ShotD = Shot.D; }
	if (bActive && Speared.bOn) bShotHadFight = true;
	if (!bActive && bPrevSpearActive && !bShotHadFight)
	{
		if (AKeysLife* L = AKeysLife::Get(this))
		{
			TArray<FTarget> Tg;
			L->SpearTargetsNear(ShotO.X, ShotO.Y, ShotO.Z, ShotRange + 1.1, Tg);
			const FVector P2 = ShotO + ShotD * ShotRange;
			for (const FTarget& F : Tg)
			{
				double Sx;
				const double Bound = F.Radius + 1.1;
				if (ClosestDistSqSegSeg(ShotO, P2, F.A, F.B, Sx) <= Bound * Bound) L->SpookAt((F.A.X + F.B.X) * .5, (F.A.Z + F.B.Z) * .5);
			}
		}
	}
	bPrevSpearActive = bActive;
}

// ================================================================ per frame
void AKeysDiver::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	const double Dt = FMath::Min(.05, double(DeltaSeconds));
	const double T = GetWorld()->GetTimeSeconds();
	FrameEvents.Reset();
	if (bDiving)
	{
		Acc += Dt;
		int32 N = 0;
		while (Acc >= KeysDive::StepDt && N < 5)
		{
			SimT = T;
			FixedStep(KeysDive::StepDt, T);
			Acc -= KeysDive::StepDt;
			N++;
			if (!bDiving) break;
		}
		for (const EEvent E : FrameEvents)
		{
			AKeysBoatPawn* P = Pawn();
			if (!P) break;
			if (E == EEvent::BlackoutShallow) P->ShowMsg(TEXT("Shallow-water blackout on the ascent - hauled back aboard unconscious. The gear is safe; the dive is not."), 5);
			else if (E == EEvent::BlackoutAir) P->ShowMsg(TEXT("Blacked out from holding your breath too long - hauled back aboard. You lose the fish and the trip; the gear's safe."), 5);
		}
		if (AKeysLife* L = AKeysLife::Get(this))
		{
			if (bDiving)
			{
				const FVector2D W = WaterVelocity(S.X, S.Y, S.Z, T, KeysSea::SeafloorHeightAt(S.X, S.Z));
				L->SetDiver(true, S.X, S.Z, FVector2D(S.VX - W.X, S.VZ - W.Y).Size());
			}
		}
	}
	if (bDiving)
	{
		UpdateCamera(Dt, T);
		UpdateModel(T);
		UpdateGun(Dt);
		if (AKeysBoatPawn* P = Pawn()) P->ApplyDiveCamera(CamPosUE, CamRotUE, CamFovH);
		// the live reticle: what the aim assist would lock onto right now (strength 0, so it only reports)
		{
			TArray<FTarget> Tg;
			GetTargets(Tg);
			const double Cp = FMath::Cos(CamPitch);
			const FVector Aim(-FMath::Sin(CamYaw) * Cp, FMath::Sin(CamPitch), -FMath::Cos(CamYaw) * Cp);
			const FVector Eye = EyeWorld();
			AssistAim(MuzzleWorld(), Aim, Tg, AimTarget, 0, NextShotRange(), &Eye);
		}
	}
	UpdateShaftAndLine(T);
	UpdateSpearedFish(Dt, T);
	UpdateUnderwaterLook(Dt);
	UpdateBubbles(Dt, T);
	UpdateSnow(Dt, T);
	UpdateSeeps(Dt, T);
	DrawParticles();
}

void AKeysDiver::UpdateCamera(double Dt, double T)
{
	const double Depth = S.Depth(), Af = AirFrac();
	const double Tunnel = Af < .25 ? FMath::Min(1.0, (.25 - Af) / .25) : 0;
	// The web game's surface transition narrows its 58 degree boat FOV by a quarter under water (a mask magnifies),
	// and the mask camera narrows it further toward 34 degrees as the air runs out. Vertical degrees, like three.js.
	const double Base = 58 * (1 - .25 * UnderAmt);
	const double Speed = S.Speed();
	const double WantFov = Base - (Base - 34) * Tunnel + FMath::Min(4.0, Speed * 1.5) * (1 - Tunnel);
	const double RawRate = Dt > 0 ? (CamYaw - PrevYaw) / Dt : 0;
	PrevYaw = CamYaw;
	YawRate += (DvClamp(RawRate, -6, 6) - YawRate) * FMath::Min(1.0, Dt * 8);
	FovV += (WantFov - FovV) * FMath::Min(1.0, Dt * 3);
	double Aspect = 16.0 / 9;
	if (UGameViewportClient* VPC = GetWorld()->GetGameViewport())
	{
		FVector2D Sz;
		VPC->GetViewportSize(Sz);
		if (Sz.Y > 0) Aspect = Sz.X / Sz.Y;
	}
	CamFovH = float(FMath::RadiansToDegrees(2 * FMath::Atan(FMath::Tan(FMath::DegreesToRadians(FovV) / 2) * Aspect)));

	const double Cp = FMath::Cos(CamPitch);
	const FVector Fw(-FMath::Sin(CamYaw) * Cp, FMath::Sin(CamPitch), -FMath::Cos(CamYaw) * Cp);
	if (CamMode == 0)
	{
		const double SwayK = S.bNarcosis ? 2.2 : 1;
		const double Sway = FMath::Sin(T * 1.3) * .015 * SwayK;
		const double Stroke = S.bKicking ? FMath::Max(0.0, FMath::Sin(S.KickPhase * PI * 2)) : 0;
		const double Surge = Stroke * .05 * FMath::Min(1.0, Speed / 1.2);
		const double Bank = DvClamp(YawRate * .03, -.1, .1);
		const FVector Pos(S.X + Fw.X * Surge, S.Y + .15 + Fw.Y * Surge - Stroke * .012, S.Z + Fw.Z * Surge);
		CamPosUE = G2U(Pos);
		const double Roll = FMath::Cos(T * .9) * .01 * SwayK + Bank;
		CamRotUE = FRotator(FMath::RadiansToDegrees(CamPitch + Sway + Stroke * .006), YawDeg(CamYaw), FMath::RadiansToDegrees(Roll));
	}
	else
	{
		const FVector Pos(S.X - Fw.X * 3.4, S.Y + 1.1 - Fw.Y * 1.2, S.Z - Fw.Z * 3.4);
		CamPosUE = G2U(Pos);
		CamRotUE = (G2U(FVector(S.X, S.Y + .15, S.Z)) - CamPosUE).Rotation();
	}
}

void AKeysDiver::UpdateModel(double T)
{
	Body->SetWorldLocationAndRotation(G2U(FVector(S.X, S.Y, S.Z)), FRotator(0, YawDeg(S.Yaw), 0));
	const double K = FMath::Min(1.0, S.Speed() / 1.8), Rate = 3.5 + K * 7, Ph = T * Rate;
	FinL->SetRelativeRotation(FRotator(FMath::RadiansToDegrees(FMath::Sin(Ph) * (.25 + K * .5)), 0, 0));
	FinR->SetRelativeRotation(FRotator(FMath::RadiansToDegrees(FMath::Sin(Ph + PI) * (.25 + K * .5)), 0, 0));
	Body->SetVisibility(CamMode == 1, true);
}

void AKeysDiver::UpdateGun(double Dt)
{
	const bool bLoaded = CanFire(Gun);
	const bool bDrawNow = bDrawing && HandsFree() && !bLoaded && Gun.ReloadT <= 0;
	const double Frac = bLoaded ? 1 : DvClamp(Gun.Draw, 0, 1);
	const double Recover = Gun.ReloadT > 0 ? FMath::Min(1.0, 1 - Gun.ReloadT / SpearReload) : 1;
	const double Effort = DrawRateFor(KickSpeed);
	GunShaft->SetRelativeLocation(FVector((BarrelLen + ShaftPoke * .5 * Frac) * 100, 0, RailY * 100));
	StrainT += Dt;
	const double Tremble = bDrawNow ? FMath::Sin(StrainT * 41) * .006 * (.4 + Frac) : 0;
	const double BandLen = DvLerp(BandShort, BandLong, Frac) + Tremble;
	const double BandRad = DvLerp(BandR, .0045, Frac);
	// the bands run from their anchor near the muzzle back along the barrel to the hand
	for (int32 Side = -1; Side <= 1; Side += 2)
	{
		UProceduralMeshComponent* Bd = Side < 0 ? GunBandL : GunBandR;
		Bd->SetRelativeLocation(FVector(BandAnchorX * 100, Side * (BarrelR + BandR + .002) * 100, RailY * 100));
		Bd->SetRelativeRotation(FRotator(0, 180, 0));
		Bd->SetRelativeScale3D(FVector(BandLen, BandRad, BandRad));
	}
	PoseW += ((bLoaded ? 0 : 1) - PoseW) * FMath::Min(1.0, Dt * (bLoaded ? 9 : 6));
	const bool bGlove = !bLoaded && PoseW > .3;
	if (bGlove)
	{
		const double HandX = Recover < 1 ? BandAnchorX - BandShort : BandAnchorX - BandLen;
		GunGlove->SetRelativeLocation(FVector(HandX * 100, 0, (RailY + .012) * 100));
		GunGlove->SetRelativeRotation(FRotator(0, 0, bDrawNow ? FMath::RadiansToDegrees(FMath::Sin(StrainT * 7.5) * .18) : 0));
	}
	if (!bWasLoaded && bLoaded) SeatT = 1;
	bWasLoaded = bLoaded;
	const double Pull = bDrawNow ? (.5 + .5 * FMath::Sin(StrainT * 7.5)) * (.018 + .03 * Frac) * (.4 + .6 * Effort) : 0;
	const double ShakeX = bDrawNow ? FMath::Sin(StrainT * 53) * .0035 * Frac : 0, ShakeY = bDrawNow ? FMath::Sin(StrainT * 47 + 1) * .0035 * Frac : 0;
	double Snap = 0;
	if (KickT > 0) { KickT = FMath::Max(0.0, KickT - Dt / .22); const double E = FMath::Sin(KickT * PI); Snap = E * E; }
	const double SeatHop = SeatT > 0 ? FMath::Sin(DvClamp(SeatT, 0, 1) * PI) * .03 : 0;
	if (SeatT > 0) SeatT = FMath::Max(0.0, SeatT - Dt / .18);
	SwayT += Dt * (1 + FMath::Min(1.0, S.Speed() / 2) * 1.6);
	const double Amt = .012 + FMath::Min(1.0, S.Speed() / 2.2) * .02;
	SwayV = FMath::Lerp(SwayV, FVector(0, FMath::Sin(SwayT * 1.1) * Amt, FMath::Sin(SwayT * 1.7 + 1.3) * Amt * .7), FMath::Min(1.0, Dt * 8));
	// camera frame (forward, right, up); three.js +z back is -forward
	const FVector Off(DvLerp(ReadyPos.X, LoadPos.X, PoseW) - .1 * Snap - Pull, DvLerp(ReadyPos.Y, LoadPos.Y, PoseW) + ShakeX, DvLerp(ReadyPos.Z, LoadPos.Z, PoseW) + ShakeY - SeatHop);
	const double PitchR = DvLerp(-.04, .36, PoseW) - .16 * Snap + Pull * 1.5 + FMath::Sin(SwayT * 1.7 + 1.3) * Amt * .4;
	const double YawR = DvLerp(.03, .2, PoseW) + KickYaw * Snap;
	const double RollR = DvLerp(0, .42, PoseW) + KickRoll * Snap + (bDrawNow ? FMath::Sin(StrainT * 7.5) * .03 : 0) + FMath::Sin(SwayT * 1.1) * Amt * .6;
	// three.js rotations: +x pitches the muzzle up (Unreal +pitch), +y turns it left (Unreal -yaw)
	const FQuat CamQ = CamRotUE.Quaternion();
	const FVector PosUE = CamPosUE + CamQ.RotateVector((Off + SwayV) * 100);
	const FQuat Q = CamQ * FRotator(FMath::RadiansToDegrees(PitchR), FMath::RadiansToDegrees(-YawR), FMath::RadiansToDegrees(-RollR)).Quaternion();
	GunBase->SetWorldLocationAndRotation(PosUE, Q);
	// the gun is put away while a speared catch is held up for its photo
	const AKeysBoatPawn* Pw = Pawn();
	const bool bShow = CamMode == 0 && !S.bBlackedOut && !(Pw && Pw->FState == EKeysFish::Caught);
	GunBase->SetVisibility(bShow, false);
	GunBandL->SetVisibility(bShow);
	GunBandR->SetVisibility(bShow);
	GunShaft->SetVisibility(bShow && !Speared.bOn);
	GunGlove->SetVisibility(bShow && bGlove);
	MuzzleG = U2G(PosUE + Q.RotateVector(FVector(BarrelLen, 0, RailY) * 100));
}

void AKeysDiver::UpdateShaftAndLine(double T)
{
	FKeysMesh M;
	const FLinearColor Steel = Hex(0xc9ced3);
	if (Shot.bAlive)
	{
		const double Tip = Shot.Dist, Tail = FMath::Max(0.0, Shot.Dist - .6);
		auto ToL = [](const FVector& G) { return FVector(G.X, G.Z, G.Y); };   // game -> Unreal axes, meters
		Tube(M, ToL(Shot.At(Tail)), ToL(Shot.At(Tip)), .00325 * 2, .00325 * 2, 6, Steel, true);
		// the shooting line trailing back to the muzzle, sagging a little
		const FVector TailP = Shot.At(Tail);
		FVector Prev = ToL(Shot.O);
		for (int32 i = 1; i <= 8; i++)
		{
			const double U = i / 8.0;
			const double Sag = FMath::Sin(U * PI) * FMath::Min(.12, Tail * .03);
			const FVector P = ToL(FVector(Shot.O.X + (TailP.X - Shot.O.X) * U, Shot.O.Y + (TailP.Y - Shot.O.Y) * U - Sag, Shot.O.Z + (TailP.Z - Shot.O.Z) * U));
			Tube(M, Prev, P, .002, .002, 4, FLinearColor(.8f, .85f, .88f));
			Prev = P;
		}
	}
	if (Speared.bOn && bDiving)
	{
		// the float line from the fish up to the float at the surface, red as it comes tight
		const FVector Fp(Speared.F.X, S.Y, Speared.F.Z);
		const double Surf = WaveH(Fp.X, Fp.Z, T, AmpAt(Fp.X, Fp.Z), false);
		const double Tn = DvClamp(Speared.F.Tension, 0, 1.2);
		const double TFrac = DvClamp((Speared.F.Tension - .25) / .85, 0, 1);
		const double Span = FMath::Abs(Surf - Fp.Y);
		const double Sag = (1 - TFrac) * DvClamp(Span * .22, 0, 1.1);
		const double Judder = Speared.F.Tension > .95 ? FMath::Sin(T * 46) * .03 * DvClamp((Speared.F.Tension - .95) / .25, 0, 1) : 0;
		const FLinearColor LC = FMath::Lerp(FLinearColor::White, Hex(0xff5a3c), float(DvClamp(Tn, 0, 1)));
		FVector Prev;
		for (int32 i = 0; i <= 16; i++)
		{
			const double U = i / 16.0;
			const FVector P(Fp.X + Judder * FMath::Sin(U * 7 + T * 20), Fp.Z + Judder * FMath::Cos(U * 7 + T * 20) - Tn * .35 * U * (1 - U) * 2, Fp.Y + (Surf - Fp.Y) * U - FMath::Sin(U * PI) * Sag);
			if (i) Tube(M, Prev, P, .006, .006, 4, LC);
			Prev = P;
		}
		Ellipsoid(M, FVector(Fp.X, Fp.Z, Surf + .07), FVector(.14), 10, Hex(0xf2c14e));
		Ellipsoid(M, FVector(Fp.X, Fp.Z, Surf - .06), FVector(.13), 10, FLinearColor::White);
	}
	if (M.V.Num()) M.Commit(Lines, 0, AKeysWorld::VertexColorMaterial());
	else Lines->ClearAllMeshSections();
}

void AKeysDiver::UpdateSpearedFish(double Dt, double T)
{
	if (FishMode == 0) { FishOnLine->SetVisibility(false); return; }
	FishOnLine->SetVisibility(true);
	double Pitch = 0, Yaw = 0, Roll = 0, Sc = 1;
	if (FishMode == 1)
	{
		FishPos.X = DvLerp(FishPos.X, FishTgt.X, FMath::Min(1.0, Dt * 9));
		FishPos.Y = DvLerp(FishPos.Y, FishTgt.Y, FMath::Min(1.0, Dt * 6));
		FishPos.Z = DvLerp(FishPos.Z, FishTgt.Z, FMath::Min(1.0, Dt * 9));
		const double DX = FishTgt.X - FishPos.X, DZ = FishTgt.Z - FishPos.Z;
		if (DX * DX + DZ * DZ > 1e-5) FishHead = FMath::Atan2(-DX, -DZ);
		const double Fight = DvClamp(FishTension, 0, 1.2), Ex = 1 - DvClamp(FishStam, 0, 1);
		const double Amp = .1 + Fight * .3 + Ex * .2, Fq = 5 + Fight * 9 + Ex * 4;
		Pitch = FMath::Sin(T * Fq * .7) * Amp * .5;
		Yaw = FishHead + FMath::Sin(T * Fq) * Amp;
		Roll = FMath::Sin(T * Fq * 1.4 + .6) * Amp * .6;
		Sc = 1 + FMath::Sin(T * Fq * 2) * .04 * (Fight + Ex);
	}
	else
	{
		FleeT += Dt;
		FishPos += FVector(FleeDir.X * 2.4 * Dt, -.25 * Dt, FleeDir.Z * 2.4 * Dt);
		Pitch = .12;
		Yaw = FMath::Atan2(-FleeDir.X, -FleeDir.Z) + FMath::Sin(T * 13) * .1;
		Roll = FMath::Sin(T * 13 + .5) * .14;
		if (FleeT > 2.4) { FishMode = 0; FishOnLine->ClearAllMeshSections(); return; }
	}
	FishOnLine->SetWorldLocationAndRotation(G2U(FishPos), FRotator(FMath::RadiansToDegrees(Pitch), YawDeg(Yaw), FMath::RadiansToDegrees(Roll)));
	FishOnLine->SetWorldScale3D(FVector(Sc));
}

// ================================================================ the look under the surface
void AKeysDiver::UpdateUnderwaterLook(double Dt)
{
	FVector CamU = CamPosUE;
	if (!bDiving)
		if (APlayerCameraManager* C = UGameplayStatics::GetPlayerCameraManager(this, 0)) CamU = C->GetCameraLocation();
	CamGame = U2G(CamU);
	const double T = GetWorld()->GetTimeSeconds();
	const double Surf = WaveH(CamGame.X, CamGame.Z, T, AmpAt(CamGame.X, CamGame.Z), false);
	const double Y = CamGame.Y - Surf;
	const bool bUnder = bCamUnder ? Y <= .15 : Y < -.15;
	if (bUnder != bCamUnder)
	{
		bCamUnder = bUnder;
		Wet = 1;
	}
	UnderAmt += ((bUnder ? 1.0 : 0.0) - UnderAmt) * (1 - FMath::Exp(-Dt / .12));
	if (UnderAmt < 1e-4) UnderAmt = 0; else if (UnderAmt > 1 - 1e-4) UnderAmt = 1;
	Wet *= FMath::Exp(-Dt / .45);
	if (Wet < 1e-3) Wet = 0;
	if (PostMID)
	{
		PostMID->SetScalarParameterValue(TEXT("Under"), float(UnderAmt));
		PostMID->SetScalarParameterValue(TEXT("Wet"), float(Wet));
		PostMID->SetScalarParameterValue(TEXT("Haze"), float(UnderwaterHaze));
		PostMID->SetScalarParameterValue(TEXT("Bonus"), float(ViewBonusM));   // the wide mask sees further
		if (const AKeysWorld* Wd = AKeysWorld::Get(this)) { const FVector Sd = Wd->SunDir(); PostMID->SetVectorParameterValue(TEXT("SunDir"), FLinearColor(Sd.X, Sd.Y, Sd.Z, 0)); }
		PostMID->SetScalarParameterValue(TEXT("Time"), float(GetWorld()->GetTimeSeconds()));
	}
	Post->bEnabled = UnderAmt > 0 || Wet > 0;
	if (AKeysAudio* A = AKeysAudio::Get(this))
		A->SetUnderwater(bUnder, FMath::Max(0.0, -Y), bDiving ? AirFrac() : 1, bDiving && !S.bBlackedOut && S.Depth() > KeysDive::SurfaceEps, Dt);
	if (AKeysWorld* W = AKeysWorld::Get(this)) W->SetUnderside(bUnder || UnderAmt > .01);
}

void AKeysDiver::SpawnBubble(const FVector& P, double R)
{
	int32 I = -1;
	for (int32 n = 0; n < Bubbles.Num(); n++)
	{
		const int32 K = (BubCursor + n) % Bubbles.Num();
		if (Bubbles[K].Age < 0) { I = K; break; }
	}
	if (I < 0) I = BubCursor;
	BubCursor = (I + 1) % Bubbles.Num();
	FBubble& B = Bubbles[I];
	B.Age = 0; B.P = P; B.V = FVector::ZeroVector; B.R0 = R; B.P0 = AtaAt(FMath::Max(0.0, -P.Y));
	B.WobA = .05 + FMath::FRand() * .12; B.WobF = 1.6 + FMath::FRand() * 2.4; B.WobP = FMath::FRand() * PI * 2;
}

void AKeysDiver::Puff(int32 N, const FVector& P, double RMin, double RMax)
{
	for (int32 k = 0; k < N; k++)
		SpawnBubble(P + FVector((FMath::FRand() - .5) * .16, (FMath::FRand() - .5) * .08, (FMath::FRand() - .5) * .16), DvRand(RMin, RMax));
}

// the diver's breath (entities/diver/bubbles.ts): a slow leak that quickens as the air runs low and with effort,
// a clearing puff every 2.4 m of descent, a burst on surfacing and on blackout; bubbles grow as they rise
void AKeysDiver::UpdateBubbles(double Dt, double T)
{
	const double H = FMath::Min(.05, FMath::Max(0.0, Dt));
	if (bDiving)
	{
		const double Cp = FMath::Cos(S.Pitch);
		const FVector Fw(-FMath::Sin(S.Yaw) * Cp, FMath::Sin(S.Pitch), -FMath::Cos(S.Yaw) * Cp);
		const FVector Mouth(S.X + Fw.X * .5, S.Y + .12 + Fw.Y * .5, S.Z + Fw.Z * .5);
		const double Depth = S.Depth();
		if (Depth > .4)
		{
			if (Depth > LastEqDepth + 2.4) { LastEqDepth = Depth; Puff(4 + FMath::RandRange(0, 3), Mouth, .004, .011); }
			else if (Depth < LastEqDepth - 4.8) LastEqDepth = Depth;
			const double Want = 1 - AirFrac();
			const double Rate = .7 + 7.5 * Want * Want + 3.5 * S.Exertion;
			BubAcc += Rate * H;
			while (BubAcc >= 1) { BubAcc -= 1; Puff(1, Mouth, .003, .009); }
			if (BubAcc > 2) BubAcc = 2;
		}
		else LastEqDepth = 0;
		for (const EEvent E : FrameEvents)
		{
			if (E == EEvent::Surfaced) Puff(24, Mouth - FVector(0, .1, 0), .006, .02);
			if (E == EEvent::BlackoutAir || E == EEvent::BlackoutShallow) Puff(44, Mouth, .006, .024);
		}
	}
	const FVector2D Flow = KeysSea::CurrentAt(CamGame.X, CamGame.Z, T, FMath::Max(0.0, -CamGame.Y)) * .92;
	const double Relax = FMath::Min(1.0, H * 1.7);
	for (FBubble& B : Bubbles)
	{
		if (B.Age < 0) continue;
		B.Age += H;
		const double R = B.R0 * FMath::Pow(B.P0 / AtaAt(FMath::Max(0.0, -B.P.Y)), 1.0 / 3);
		const double Rise = FMath::Min(.46, .26 * FMath::Sqrt(R / .011));
		B.V.X += (Flow.X - B.V.X) * Relax;
		B.V.Z += (Flow.Y - B.V.Z) * Relax;
		const double W = B.WobF * (T + B.WobP);
		B.P += FVector((B.V.X + FMath::Cos(W) * B.WobA) * H, Rise * H, (B.V.Z + FMath::Sin(W * .9) * B.WobA) * H);
		if (B.P.Y > -.06 || B.Age > 110) B.Age = -1;
	}
	for (FPuff& P : Muzzle)
	{
		if (P.Life <= 0) continue;
		P.Life -= H;
		P.V.X += (Flow.X - P.V.X) * FMath::Min(1.0, H * 1.5);
		P.V.Z += (Flow.Y - P.V.Z) * FMath::Min(1.0, H * 1.5);
		P.V.Y += H * .8;
		P.P += P.V * H;
	}
}

// marine snow (world/underwater/marine-snow.ts): 900 specks wrapping in an 11 m box around the camera, drifting
// with the current and sinking; thicker with depth
void AKeysDiver::UpdateSnow(double Dt, double T)
{
	const double Frac = (.4 + .6 * FMath::Min(1.0, FMath::Max(0.0, -CamGame.Y) / 25)) * UnderAmt;
	if (Frac <= 0) return;
	const FVector2D Flow = KeysSea::CurrentAt(CamGame.X, CamGame.Z, T, DepthFast(CamGame.X, CamGame.Z)) * 1.6;
	for (FSnow& Sn : Snow)
	{
		Sn.P += FVector(Sn.V.X + Flow.X, Sn.V.Y, Sn.V.Z + Flow.Y) * Dt;
		for (int32 A = 0; A < 3; A++)
		{
			const double Rel = Sn.P[A] - CamGame[A];
			Sn.P[A] = CamGame[A] + (FMath::Fmod(FMath::Fmod(Rel + 11, 22.0) + 22, 22.0) - 11);
		}
	}
}

// seep bubbles (world/underwater/seep-bubbles.ts): a few vents on the bottom near the camera, trickling bubbles
void AKeysDiver::UpdateSeeps(double Dt, double T)
{
	if (UnderAmt <= .01 || CamGame.Y > -.4) return;
	constexpr double Cell = 16;
	auto Hs = [](double A, double B, double Sd) { const double Hh = FMath::Sin(A * 127.1 + B * 311.7 + Sd * 74.7) * 43758.5453; return Hh - FMath::FloorToDouble(Hh); };
	if (FVector2D(CamGame.X - LastSeepX, CamGame.Z - LastSeepZ).Size() > 5)
	{
		LastSeepX = CamGame.X; LastSeepZ = CamGame.Z;
		struct FC { int64 Key; FVector P; double Rate, D; };
		TArray<FC> Cands;
		const int32 IX = FMath::FloorToInt32(CamGame.X / Cell), IZ = FMath::FloorToInt32(CamGame.Z / Cell);
		for (int32 j = -2; j <= 2; j++)
			for (int32 i = -2; i <= 2; i++)
			{
				const int32 GX = IX + i, GZ = IZ + j;
				if (Hs(GX, GZ, 1) > .3) continue;
				const double X = (GX + .15 + Hs(GX, GZ, 2) * .7) * Cell, Z = (GZ + .15 + Hs(GX, GZ, 3) * .7) * Cell;
				if (DepthAt(X, Z) < 1.8) continue;
				Cands.Add({(int64(GX) << 32) ^ int64(uint32(GZ)), FVector(X, KeysSea::SeafloorHeightAt(X, Z), Z), 4 + Hs(GX, GZ, 4) * 6, FVector2D(X - CamGame.X, Z - CamGame.Z).Size()});
			}
		Cands.Sort([](const FC& A, const FC& B) { return A.D < B.D; });
		TArray<int32> Free;
		TSet<int64> Keep;
		for (int32 k = 0; k < FMath::Min(8, Cands.Num()); k++) Keep.Add(Cands[k].Key);
		for (int32 s = 0; s < Seeps.Num(); s++) if (!Keep.Contains(Seeps[s].Key)) { Seeps[s].Key = MIN_int64; Free.Add(s); }
		for (int32 k = 0; k < FMath::Min(8, Cands.Num()); k++)
		{
			bool bHave = false;
			for (const FSeep& Sp : Seeps) if (Sp.Key == Cands[k].Key) { bHave = true; break; }
			if (bHave || !Free.Num()) continue;
			const int32 s = Free.Pop();
			Seeps[s] = {Cands[k].Key, Cands[k].P, Cands[k].Rate, 0};
			for (FSeepB& B : SeepB) if (B.Slot == s) B.Age = -1;
		}
	}
	const FVector2D Flow = KeysSea::CurrentAt(CamGame.X, CamGame.Z, T, FMath::Max(0.0, -CamGame.Y)) * .92;
	const double Relax = FMath::Min(1.0, Dt * 1.6);
	for (int32 s = 0; s < Seeps.Num(); s++)
	{
		FSeep& E = Seeps[s];
		if (E.Key == MIN_int64) continue;
		E.Acc = FMath::Min(2.0, E.Acc + E.Rate * Dt);
		for (int32 b = 0; b < 34; b++)
		{
			FSeepB& B = SeepB[s * 34 + b];
			if (B.Age < 0)
			{
				if (E.Acc < 1) continue;
				E.Acc -= 1;
				B.Age = 0; B.Life = 3.2 + FMath::FRand() * 3;
				B.P = E.P + FVector((FMath::FRand() - .5) * .25, .05, (FMath::FRand() - .5) * .25);
				B.V = FVector(0, .3 * (.75 + FMath::FRand() * .5), 0);
				B.Size = .018 + FMath::FRand() * .03;
			}
			B.Age += Dt;
			B.V.X += (Flow.X - B.V.X) * Relax;
			B.V.Z += (Flow.Y - B.V.Z) * Relax;
			B.P += FVector(B.V.X * Dt, B.V.Y * Dt * (1 + B.Age * .06), B.V.Z * Dt);
			B.Size += Dt * .004;
			if (B.Age > B.Life || B.P.Y > -.1) B.Age = -1;
		}
	}
}

// all the bubbles and the snow as camera-facing quads in one translucent mesh
void AKeysDiver::DrawParticles()
{
	TArray<FVector> V, N;
	TArray<FVector2D> UV;
	TArray<FLinearColor> C;
	TArray<int32> Tr;
	FVector CamU = CamPosUE;
	FRotator CamR = CamRotUE;
	if (!bDiving)
		if (APlayerCameraManager* PCM = UGameplayStatics::GetPlayerCameraManager(this, 0)) { CamU = PCM->GetCameraLocation(); CamR = PCM->GetCameraRotation(); }
	const FQuat Q = CamR.Quaternion();
	const FVector Rt = Q.GetRightVector(), Up = Q.GetUpVector(), Fw = Q.GetForwardVector();
	auto Quad = [&](const FVector& G, double Diam, const FLinearColor& Col, bool bSnow)
	{
		const FVector P = G2U(G);
		const double Dist = FVector::DotProduct(P - CamU, Fw);
		if (Dist < 5) return;
		const double Hs = FMath::Max(Diam * 100, Dist * .0012) * .5;
		const int32 I = V.Num();
		const double U0 = bSnow ? 2 : 0;
		V.Add(P - Rt * Hs - Up * Hs); V.Add(P + Rt * Hs - Up * Hs); V.Add(P + Rt * Hs + Up * Hs); V.Add(P - Rt * Hs + Up * Hs);
		UV.Add(FVector2D(U0, 1)); UV.Add(FVector2D(U0 + .999, 1)); UV.Add(FVector2D(U0 + .999, 0)); UV.Add(FVector2D(U0, 0));
		for (int32 k = 0; k < 4; k++) { N.Add(-Fw); C.Add(Col); }
		// faces the camera: Dot(Cross(B-A, C-A), -Fw) < 0 for this order
		Tr.Append({I, I + 2, I + 1, I, I + 3, I + 2});
	};
	for (const FBubble& B : Bubbles)
	{
		if (B.Age < 0) continue;
		const double R = B.R0 * FMath::Pow(B.P0 / AtaAt(FMath::Max(0.0, -B.P.Y)), 1.0 / 3);
		const double A = FMath::Min(1.0, B.Age * 6) * (B.Age > 108 ? (110 - B.Age) / 2 : 1) * .9;
		Quad(B.P, R * 2.4, FLinearColor(.88f, .97f, 1.f, float(A)), false);
	}
	for (const FPuff& P : Muzzle)
		if (P.Life > 0) Quad(P.P, (.04 + (1 - P.Life / P.Max) * .05) * .6, FLinearColor(.85f, .95f, 1.f, float(DvClamp(P.Life / P.Max, 0, 1) * .8)), false);
	if (UnderAmt > .01)
	{
		for (const FSeepB& B : SeepB)
			if (B.Age >= 0)
			{
				const double U = B.Age / B.Life;
				Quad(B.P, B.Size, FLinearColor(.86f, .96f, 1.f, float(FMath::Min(1.0, B.Age * 4) * (1 - U * U) * .75 * UnderAmt)), false);
			}
		const double Dens = (.4 + .6 * FMath::Min(1.0, FMath::Max(0.0, -CamGame.Y) / 25)) * UnderAmt;
		for (const FSnow& Sn : Snow)
		{
			const double Fade = 1 - DvClamp(((Sn.P - CamGame).Size() - 11 * .55) / (11 * .45), 0, 1);
			if (Fade <= 0) continue;
			Quad(Sn.P, (1.1 + Sn.Seed * 1.4) * .126 * .25, FLinearColor(.82f, .88f, .86f, float((.12 + .23 * Sn.Seed) * Dens * Fade)), true);
		}
	}
	if (!V.Num()) { Particles->ClearAllMeshSections(); return; }
	Particles->CreateMeshSection_LinearColor(0, V, Tr, N, UV, C, TArray<FProcMeshTangent>(), false);
	if (BubbleMID) Particles->SetMaterial(0, BubbleMID);
}

void AKeysDiver::DebugCmd(const FString& Cmd, const FString& Arg)
{
	if (!bDiving) return;
	if (Cmd == TEXT("depth"))
	{
		const double Fl = KeysSea::SeafloorHeightAt(S.X, S.Z);
		S.Y = FMath::Max(-FCString::Atod(*Arg), Fl + .6);
		S.VX = S.VY = S.VZ = 0;
	}
	else if (Cmd == TEXT("at"))
	{
		TArray<FString> P;
		Arg.ParseIntoArray(P, TEXT(","));
		if (P.Num() >= 2) { S.X = FCString::Atod(*P[0]); S.Z = FCString::Atod(*P[1]); S.VX = S.VY = S.VZ = 0; }
	}
	else if (Cmd == TEXT("look"))
	{
		TArray<FString> P;
		Arg.ParseIntoArray(P, TEXT(","));
		if (P.Num() >= 1) CamYaw = FCString::Atod(*P[0]);
		if (P.Num() >= 2) CamPitch = FCString::Atod(*P[1]);
		PrevYaw = CamYaw;
	}
	else if (Cmd == TEXT("aimcaelen"))
	{
		// point the gun at Caelen (from the muzzle), for the spear-the-gull test
		FVector CA, CB; double CR;
		if (AKeysExtras* Ex = AKeysExtras::Get(this); Ex && Ex->CaelenHitbox(CA, CB, CR))
		{
			const FVector D = ((CA + CB) * .5 - MuzzleWorld()).GetSafeNormal();
			CamYaw = FMath::Atan2(-D.X, -D.Z); CamPitch = FMath::Asin(FMath::Clamp(D.Y, -1.0, 1.0));
			UE_LOG(LogTemp, Log, TEXT("KRDive Caelen at %.1f m"), ((CA + CB) * .5 - MuzzleWorld()).Size());
		}
	}
	else if (Cmd == TEXT("aimfish"))
	{
		// look straight at the nearest catchable fish within range
		TArray<FTarget> Tg;
		GetTargets(Tg);
		const FVector Eye(S.X, S.Y + .15, S.Z);
		double Best = 1e9;
		for (const FTarget& T : Tg)
		{
			if (!T.bCatchable) continue;
			const FVector C = (T.A + T.B) * .5;
			const double D = (C - Eye).Size();
			if (D < 2.5 || D > 16 || D > Best) continue;
			Best = D;
			const FVector Dir = (C - Eye) / D;
			CamYaw = FMath::Atan2(-Dir.X, -Dir.Z);
			CamPitch = FMath::Asin(DvClamp(Dir.Y, -1, 1));
		}
		PrevYaw = CamYaw;
		UE_LOG(LogTemp, Log, TEXT("KRDive aimfish: %d targets, nearest %.1f m"), Tg.Num(), Best);
	}
	else if (Cmd == TEXT("fishahead"))
	{
		// fishahead or fishahead=marlin[,dist]: the nearest school (of that species) to a few metres in front of the mask
		FString Ty, Ds;
		if (!Arg.Split(TEXT(","), &Ty, &Ds)) Ty = Arg;
		const double D = Ds.IsEmpty() ? 7 : FCString::Atod(*Ds);
		if (AKeysLife* L = AKeysLife::Get(this)) L->DebugBringSchool(S.X - FMath::Sin(CamYaw) * D, S.Z - FMath::Cos(CamYaw) * D, Ty);
	}
	else if (Cmd == TEXT("hold")) DebugHoldDepth = FCString::Atod(*Arg);
	else if (Cmd == TEXT("spear")) bFireQueued = true;
	else if (Cmd == TEXT("reload")) Gun = FGun();
	else if (Cmd == TEXT("air")) S.Air = S.AirMaxV * DvClamp(FCString::Atod(*Arg), 0, 1);
	else if (Cmd == TEXT("swim")) bDebugSwim = FCString::Atoi(*Arg) != 0;
	else if (Cmd == TEXT("haul")) bDebugHaul = FCString::Atoi(*Arg) != 0;
	else if (Cmd == TEXT("chase")) { if (CamMode == 0) CycleView(); }
	else if (Cmd == TEXT("mask")) { if (CamMode == 1) CycleView(); }
	UE_LOG(LogTemp, Log, TEXT("KRDive %s %s: pos %.1f %.1f %.1f air %.0f%% floor %.1f"), *Cmd, *Arg, S.X, S.Y, S.Z, AirFrac() * 100, KeysSea::SeafloorHeightAt(S.X, S.Z));
}

double AKeysDiver::NextShotRange() const
{
	return FMath::Clamp(KeysDive::SpearRange + FMath::Max(0.0, SpearRangeBonusM), KeysDive::SpearRange * .6, KeysDive::SpearRange + 6);
}
