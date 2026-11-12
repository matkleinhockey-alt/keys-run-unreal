#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "KeysDive.h"
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
	void ExitToBoat();
	void SetButtons(const FKeysDiverButtons& B);
	FString CycleView();
	bool IsChaseView() const { return CamMode == 1; }
	void SetDefaultChase(bool bChase) { bDefaultChase = bChase; }

	// the camera the pawn should use this frame (Unreal space)
	void GetCamera(FVector& OutPos, FRotator& OutRot, float& OutFovH) const { OutPos = CamPosUE; OutRot = CamRotUE; OutFovH = CamFovH; }

	// ---- read by the HUD
	const KeysDive::FState& State() const { return S; }
	double AirFrac() const { return S.AirMaxV > 0 ? FMath::Clamp(S.Air / S.AirMaxV, 0.0, 1.0) : 0; }
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

	// ---- test hooks for -KRScript (depth=N, look=yaw,pitch, aimfish, spear, reload, air=frac, swim=0/1, haul=0/1)
	void DebugCmd(const FString& Cmd, const FString& Arg);

	// ---- tuning from the Settings menu
	double UnderwaterHaze = 1;   // the haze brightness

private:
	struct FSpeared
	{
		bool bOn = false;
		KeysDive::FTarget Target;
		KeysDive::FFightParams P;
		KeysDive::FFight F;
		FString Label;
	};
	struct FBubble { FVector P, V; double Age = -1, R0 = .005, P0 = 1, WobA = 0, WobF = 0, WobP = 0; };
	struct FPuff { FVector P, V; double Life = 0, Max = 1; };
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
	void SpawnBubble(const FVector& P, double R);
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
	UPROPERTY() TObjectPtr<UProceduralMeshComponent> Lines;     // flying shaft, its tether, the float line and float
	UPROPERTY() TObjectPtr<UProceduralMeshComponent> FishOnLine;
	UPROPERTY() TObjectPtr<UProceduralMeshComponent> Particles;
	UPROPERTY() TObjectPtr<UPostProcessComponent> Post;
	UPROPERTY() TObjectPtr<UMaterialInstanceDynamic> PostMID;
	UPROPERTY() TObjectPtr<UMaterialInstanceDynamic> BubbleMID;

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
	int32 FishMode = 0;   // 0 none, 1 held, 2 fleeing
	FVector FishPos = FVector::ZeroVector, FishTgt = FVector::ZeroVector, FleeDir = FVector::ZeroVector;
	double FishHead = 0, FishTension = .5, FishStam = 1, FleeT = 0;

	// particles
	TArray<FBubble> Bubbles;
	int32 BubCursor = 0;
	double BubAcc = 0, LastEqDepth = 0;
	TArray<FPuff> Muzzle;
	int32 MuzCursor = 0;
	TArray<FSnow> Snow;
	TArray<FSeep> Seeps;
	TArray<FSeepB> SeepB;
	double LastSeepX = 1e9, LastSeepZ = 1e9;

	// surface crossing (world/underwater/transition.ts)
	bool bCamUnder = false;
	double UnderAmt = 0, Wet = 0;
	FVector CamGame = FVector::ZeroVector;
};
