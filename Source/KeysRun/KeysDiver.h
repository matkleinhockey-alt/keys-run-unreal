#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "KeysDive.h"
#include "KeysScuba.h"
#include "KeysDiver.generated.h"

class UProceduralMeshComponent;
class UPostProcessComponent;
class UMaterialInstanceDynamic;
class AKeysBoatPawn;

// What the player's hands are on this frame while diving (the pawn reads the keyboard and fills this in).
struct FKeysDiverButtons
{
	bool bFwd = false, bBack = false, bLeft = false, bRight = false, bAscend = false, bDescend = false, bSprint = false;
	bool bHaul = false;        // S held: haul a speared fish in
	bool bFire = false;        // left click this frame
	bool bDrawToggle = false;  // R this frame: start / stop hauling the bands back
	double LookYaw = 0, LookPitch = 0;   // mouse deltas this frame, radians
};

// The diver, the speargun and the underwater look, ported from the web game's
// entities/diver/**, entities/speargun/** and world/underwater/** (see KeysDive.h for the rules themselves).
// Owns: going over the side and climbing back aboard, the fixed-step diver and spear sims, the dive-mask and chase
// cameras, the diver model, the first-person gun, the flying shaft, the float line and the fish on it, breath
// bubbles, seep bubbles and marine snow, the surface crossing (lens wetting) and the underwater post-process
// (per-channel extinction, depth-banded visibility), and the Snell's window under the surface (via AKeysWorld).
// Scuba (KeysScuba.h): when the set is owned and chosen she goes over with it on. This actor then runs the gas, ascent
// and stop rules round the same fixed step, wears the set in the chase view, lifts the gauge console in the mask view,
// breathes bubbles out of the regulator, works the dive light, keeps the logbook, and stands the cylinder rack and the
// diver-down flag in the boat's cockpit.
UCLASS()
class KEYSRUN_API AKeysDiver : public AActor
{
	GENERATED_BODY()

public:
	AKeysDiver();
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

	static AKeysDiver* Get(const UObject* WorldContext);

	// ---- controller (entities/diver/controller.ts)
	bool IsDiving() const { return bDiving; }
	void EnterWater(double BoatX, double BoatZ, double BoatH);
	bool CanReboard() const { return bCanReboard; }
	// J underwater: climbs aboard if close enough; false (and a toast) otherwise
	bool RequestReboard();
	bool IsBlackedOut() const { return bDiving && S.bBlackedOut; }
	bool HasFishOnLine() const { return Speared.bOn || FishMode != 0; }
	void ExitToBoat();
	void SetButtons(const FKeysDiverButtons& B);
	FString CycleView();
	bool IsChaseView() const { return CamMode == 1; }
	void SetDefaultChase(bool bChase) { bDefaultChase = bChase; }

	// the camera the pawn should use this frame (Unreal space)
	void GetCamera(FVector& OutPos, FRotator& OutRot, float& OutFovH) const { OutPos = CamPosUE; OutRot = CamRotUE; OutFovH = CamFovH; }

	// ---- read by the HUD
	const KeysDive::FState& State() const { return S; }
	// what is left to breathe, 0..1: the breath she is holding, or on scuba the cylinder (so the air bar, the heartbeat
	// and the fight's warnings all follow the gas without knowing about it)
	double AirFrac() const { return Sc.OnGas() ? Sc.GasFrac() : BreathFrac(); }
	double BreathFrac() const { return S.AirMaxV > 0 ? FMath::Clamp(S.Air / S.AirMaxV, 0.0, 1.0) : 0; }
	FString AscentWarning() const;
	double TunnelAmount() const;          // 0..1 dark vignette closing in as the air runs out; 1 when blacked out
	bool IsLoaded() const { return KeysDive::CanFire(Gun); }
	double GunDraw() const { return Gun.Draw; }
	double GunRecoverLeft() const { return Gun.ReloadT; }
	bool IsDrawing() const { return bDrawing && HandsFree() && !IsLoaded() && Gun.ReloadT <= 0; }
	double DrawEffort() const { return KeysDive::DrawRateFor(KickSpeed); }
	double NudgeFlash() const { return Nudge; }
	bool HasSpeared() const { return Speared.bOn; }
	const KeysDive::FFight& Fight() const { return Speared.F; }
	FString FightLabel() const { return Speared.Label; }
	bool AimLocked() const { return AimTarget >= 0; }
	bool ShotInFlight() const { return Shot.bAlive; }
	double UnderwaterAmount() const { return UnderAmt; }

	// ---- scuba (KeysScuba.h)
	bool OnScuba() const { return bDiving && Sc.bOn; }
	const KeysScuba::FState& Scuba() const { return Sc; }
	// the gauge's numbers for the HUD (bOn false on a breath-hold dive: draw the air bar as before)
	KeysScuba::FHud ScubaHud() const;
	// hold to lift the gauge console into the dive-mask view (it also comes up by itself when the gas gets low)
	void SetGlance(bool bHold) { bGlanceKey = bHold; }
	// the dive light: by itself it comes on in the dark (deep, or at night); the key cycles auto / on / off
	FString ToggleLight();
	// at the surface by the boat: hand up the cylinder and take the other one. A toast either way.
	FString SwapCylinder();
	// the regulator's sound, for the hub's audio: called at the start of each breath in and each breath out
	TFunction<void(bool bInhale, double Depth, double Effort)> OnBreath;

	// ---- test hooks for -KRScript (depth=N, look=yaw,pitch, aimfish, spear, reload, air=frac, swim=0/1, haul=0/1;
	// scuba: scuba=0/1 and scubatest before going over; psi=N, gauge=0/1, light=0/1/2, swap, orbit=yaw,pitch,dist,
	// topside=0/1, hover, gaslog, ascend=0/1/2, sprint=0/1, nearfish, kit)
	void DebugCmd(const FString& Cmd, const FString& Arg);

	// ---- tuning from the Settings menu
	double UnderwaterHaze = 1;   // the haze brightness
	// ---- the dive shop's gear (set by the pawn before going over): the pony bottle, wetsuit and fins stretch the breath hold
	double AirMul = 1;
	// the dive shop's and gun counter's gear (web sim/shop.ts modifiers), set by the boat as you go in
	double SwimMul = 1, ViewBonusM = 0, SpearRangeBonusM = 0, SpearDrawMul = 1, SpearTetherMul = 1;
	// spends a shaft for a shot: how many are left after it, or -1 when there were none (the bent spare flies)
	TFunction<int32()> TakeShaft;
	double NextShotRange() const;
	double ShotRange = KeysDive::SpearRange;

private:
	struct FSpeared
	{
		bool bOn = false;
		KeysDive::FTarget Target;
		KeysDive::FFightParams P;
		KeysDive::FFight F;
		FString Label;
	};
	struct FBubble { FVector P, V; double Age = -1, R0 = .005, P0 = 1, WobA = 0, WobF = 0, WobP = 0; bool bReg = false; };
	struct FPop { FVector P; double Age = -1, R = .1; };   // a regulator bubble breaking at the surface
	struct FPuff { FVector P, V; double Life = 0, Max = 1; };
	struct FBlood { FVector P, V; double Age = -1, Life = 6, R = .1, Ph = 0, Tone = 0; };
	struct FSnow { FVector P, V; double Seed; };
	struct FSeep { int64 Key; FVector P; double Rate; double Acc = 0; };
	struct FSeepB { FVector P, V; double Age = -1, Life = 1, Size = .02; int32 Slot = 0; };

	void FixedStep(double Dt, double T);
	void UpdateSpear(double Dt, double T);
	bool HandsFree() const { return !Shot.bAlive && !Speared.bOn && !S.bBlackedOut; }
	bool TryFire();
	void EndFight();
	void UpdateCamera(double Dt, double T);
	void UpdateModel(double T);
	void UpdateGun(double Dt);
	void UpdateShaftAndLine(double T);
	void UpdateSpearedFish(double Dt, double T);
	void UpdateBubbles(double Dt, double T);
	void UpdateSnow(double Dt, double T);
	void UpdateSeeps(double Dt, double T);
	void DrawParticles();
	void UpdateUnderwaterLook(double Dt);
	int32 SpawnBubble(const FVector& P, double R);
	// scuba
	void BeginScuba();
	void FinishDive();
	void HandleScubaEvents();
	void BreathSound(bool bInhale);
	void UpdateGauge(double Dt);
	void UpdateTorch(double Dt);
	void UpdateBoatKit(double T);
	bool TorchWanted() const;
	void Puff(int32 N, const FVector& P, double RMin, double RMax);
	void GetTargets(TArray<KeysDive::FTarget>& Out) const;
	FVector MuzzleWorld() const;   // game meters
	FVector EyeWorld() const;      // game meters
	AKeysBoatPawn* Pawn() const;

	UPROPERTY() TObjectPtr<USceneComponent> Root;
	UPROPERTY() TObjectPtr<UProceduralMeshComponent> Body;
	UPROPERTY() TObjectPtr<UProceduralMeshComponent> FinL;
	UPROPERTY() TObjectPtr<UProceduralMeshComponent> FinR;
	UPROPERTY() TObjectPtr<UProceduralMeshComponent> GunBase;
	UPROPERTY() TObjectPtr<UProceduralMeshComponent> GunBandL;
	UPROPERTY() TObjectPtr<UProceduralMeshComponent> GunBandR;
	UPROPERTY() TObjectPtr<UProceduralMeshComponent> GunShaft;
	UPROPERTY() TObjectPtr<UProceduralMeshComponent> GunGlove;
	UPROPERTY() TObjectPtr<UProceduralMeshComponent> GunHandL;
	UPROPERTY() TObjectPtr<UProceduralMeshComponent> GunWish;
	UPROPERTY() TObjectPtr<class UPointLightComponent> GunFill;     // lights only the first-person gun (lighting channel 2)
	UPROPERTY() TObjectPtr<UProceduralMeshComponent> GunLeader;  // the braided leader from the slide ring to the muzzle
	UPROPERTY() TObjectPtr<UProceduralMeshComponent> GunWorn;    // the gun in her hand in the chase view (section 1: its shaft)
	UPROPERTY() TObjectPtr<UProceduralMeshComponent> GunReel;   // the spool, flanges and crank: spins as the line pays out
	double ReelAng = 0, ReelSpin = 0;
	UPROPERTY() TObjectPtr<UProceduralMeshComponent> Lines;     // flying shaft, its tether, the float line and float
	UPROPERTY() TObjectPtr<UProceduralMeshComponent> FishOnLine;
	UPROPERTY() TObjectPtr<class UPointLightComponent> FishFill;   // lights only the speared fish (lighting channel 1)
	void UpdateFishFill();
	UPROPERTY() TObjectPtr<UProceduralMeshComponent> Particles;
	UPROPERTY() TObjectPtr<UPostProcessComponent> Post;
	UPROPERTY() TObjectPtr<UMaterialInstanceDynamic> PostMID;
	UPROPERTY() TObjectPtr<UMaterialInstanceDynamic> BubbleMID;
	// scuba: the worn set (on Body), the gauge console and its needle and digits (dive-mask view), the dive light,
	// and the rack and flag that ride on the boat's own mesh
	UPROPERTY() TObjectPtr<UProceduralMeshComponent> Kit;
	UPROPERTY() TObjectPtr<UProceduralMeshComponent> Gauge;
	UPROPERTY() TObjectPtr<UProceduralMeshComponent> GaugeFace;
	UPROPERTY() TObjectPtr<class USpotLightComponent> Torch;
	UPROPERTY() TObjectPtr<UProceduralMeshComponent> BoatRack;
	UPROPERTY() TObjectPtr<UProceduralMeshComponent> BoatFlag;
	KeysScuba::FState Sc;
	int32 CylIx = 0, KitSig = -1, RackSig = -1, FaceSig = -1, KitTris = 0;
	bool bGlanceKey = false, bDebugGlance = false, bBoatKitTried = false;
	double GlanceW = 0, GlanceAutoT = 0;
	double LeaderOut = -1;        // the shaft's travel the leader was last built for
	bool bWornShaft = true, bDbgGunShow = false;
	int32 TorchMode = 0;          // 0 auto, 1 on, 2 off
	double TorchW = 0;
	double DiveSecs = 0, DiveMaxDepth = 0, EnterT = 0, PsiUsedBefore = 0;
	bool bHelloSaid = false;
	double ExhAcc = 0;
	TArray<FPop> Pops;
	int32 PopCursor = 0;
	// test hooks: the chase camera swung round her, a camera on the boat looking at her bubbles, held keys, the hover and gas logs
	bool bDbgOrbit = false, bDbgTopside = false, bDebugSprint = false;
	double DbgOrbitYaw = 0, DbgOrbitPitch = .3, DbgOrbitDist = 3.4;
	int32 DebugAscend = 0;
	double HoverT = -1, HoverY0 = 0, HoverX0 = 0, HoverZ0 = 0;
	double GasLogT = -1, GasLogPsi0 = 0, GasLogSecs = 120;
	double FishLogT = -1;

	// controller
	bool bDiving = false, bCanReboard = false, bDefaultChase = false;
	double WakingT = 0, Acc = 0, SimT = 0;
	KeysDive::FState S;
	FKeysDiverButtons Btn;
	bool bFireQueued = false, bDrawQueued = false, bDebugSwim = false, bDebugHaul = false;
	double DebugHoldDepth = -1;   // test hook: keep the diver at this depth
	TArray<KeysDive::EEvent> FrameEvents;

	// camera (entities/diver/camera.ts)
	int32 CamMode = 0;   // 0 dive mask, 1 chase
	double CamYaw = 0, CamPitch = -.1, YawRate = 0, PrevYaw = 0, FovV = 58;
	FVector CamPosUE = FVector::ZeroVector;
	FRotator CamRotUE = FRotator::ZeroRotator;
	float CamFovH = 90;

	// speargun (entities/speargun/index.ts)
	KeysDive::FGun Gun;
	KeysDive::FShot Shot;
	FSpeared Speared;
	bool bDrawing = false, bDrawLatched = false, bHauling = false;
	int32 AimTarget = -1;
	double KickSpeed = 0, Nudge = 0;
	bool bPrevSpearActive = false, bShotHadFight = false;
	FVector ShotO = FVector::ZeroVector, ShotD = FVector::ForwardVector;
	// gun view model state
	double KickT = 0, KickYaw = 0, KickRoll = 0, SeatT = 0, PoseW = 0, StrainT = 0, SwayT = 0;
	bool bWasLoaded = true;
	FVector SwayV = FVector::ZeroVector;
	FVector MuzzleG = FVector::ZeroVector;   // the muzzle, game meters
	// the fish on the line (speared-fish-visual.ts)
	int32 FishMode = 0;   // 0 none, 1 held, 2 fleeing, 3 dying on the spear, 4 held up by the diver
	double KillT = 0;
	TArray<FBlood> Blood;
	void BleedAt(const FVector& G, int32 N, double Spread);
	FVector FishPos = FVector::ZeroVector, FishTgt = FVector::ZeroVector, FleeDir = FVector::ZeroVector;
	double FishHead = 0, FishTension = .5, FishStam = 1, FleeT = 0;

	// particles
	TArray<FBubble> Bubbles;
	int32 BubCursor = 0;
	double BubAcc = 0, LastEqDepth = 0, TopAcc = 0; bool bWasUnder = false;
	TArray<FPuff> Muzzle;
	int32 MuzCursor = 0;
	TArray<FSnow> Snow;
	TArray<FSeep> Seeps;
	TArray<FSeepB> SeepB;
	double LastSeepX = 1e9, LastSeepZ = 1e9;

	// surface crossing (world/underwater/transition.ts)
	bool bCamUnder = false;
	double UnderAmt = 0, Wet = 0;
	double ShockT = 99;            // seconds since the gun's pressure ring went out from the muzzle
	FVector2D ShockUV = FVector2D(.5, .5);
	FVector CamGame = FVector::ZeroVector;
};
