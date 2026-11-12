#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "KeysHandgun.generated.h"

class UProceduralMeshComponent;
class UPointLightComponent;
class UMaterialInstanceDynamic;
class AKeysBoatPawn;
struct FKeysBird;

// The boat pistol, ported from the web game's shared/sim/handgun.ts (ballistics: 350 m/s, drag in path length,
// gravity, the water stopping a round inside 2.4 m and skipping one under 5 degrees, the magazine, the shaky hand,
// the aim assist with its lead solve, what a report does to fish and birds) and client/entities/handgun/**
// (9 draws and holsters, click or Space fires, R reloads, Z fires a parachute flare, the view model, brass, tracers,
// the stunned fish that bob up offended, the HUD), entities/life/boat-gulls.ts (the gang of gulls that follows the
// boat) and life/carcass.ts with shared/sim/chum.ts (a bird shot down floats and the toothy fish come to it).
// It shoots at fish near the surface and seabirds; nothing here knows what a person is.
UCLASS()
class KEYSRUN_API AKeysHandgun : public AActor
{
	GENERATED_BODY()

public:
	AKeysHandgun();
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	static AKeysHandgun* Get(const UObject* WorldContext);

	bool IsOut() const { return bOut; }
	void Toggle();
	void Holster();
	void HandleInput(class APlayerController* PC, float Dt);
	// the pistol's first-person camera while it is out
	bool CameraFor(FVector& OutPosUE, FRotator& OutRot, float& OutFov) const;
	// a carcass on the water near (X, Z): the scavengers come (sim/chum.ts CARCASS_REACH_M)
	bool NearCarcass(double X, double Z, double R = 55) const;
	static bool IsScavenger(const FString& Key);

	// ---- for the HUD
	int32 Mag = 0, MagSize = 12;
	bool bReloading = false;
	double ReloadT = 0, ReloadDur = 1, Heat = 0, HitFlash = 0, NoteT = 0;
	int32 TallyFish = 0, TallyBirds = 0, TallyShots = 0;
	FString Note;
	int32 Reserve() const;
	int32 Flares() const;
	double Spread() const;
	// test hooks
	void DebugFire(double Yaw, double Pitch);
	void DebugLook(double Yaw, double Pitch) { LookYaw = Yaw; LookPitch = Pitch; }
	void DebugFlare() { FireFlare(); }

private:
	struct FBullet
	{
		FVector P, V;   // game meters, m/s
		double Travelled = 0, WaterPath = 0, VEntry = 0, V0 = 0;
		bool bInWater = false, bAlive = true;
		int32 Skips = 0;
		FVector LegStart;
		TArray<TPair<FVector, FVector>> Legs;
		double Age = 0;
	};
	struct FTgt { int32 Kind; int32 Id; FString Key; double Weight; FVector A, B, Vel; double R; FKeysBird* Bird; int32 Gull; };
	struct FFloater { FString Key; double Weight, X, Z, T, Yaw, DX, DZ; };
	struct FBrass { FVector P, V; double T; };
	struct FFall { FVector P, V; double T; uint8 Kind; };
	struct FCarcass { double X, Z, Age; };
	struct FStreak { FVector A, B; double T; };
	struct FFlare { FVector O, V; double T; };
	struct FGull { double R, H, Sp, Ph, DownUntil = -1; uint8 Kind; FVector Pos; };

	void Draw();
	void Fire();
	void StartReload();
	void FireFlare();
	void Flash(const FString& S, double Secs = 2.2) { Note = S; NoteT = Secs; }
	void StepBullets(double Dt);
	void GatherTargets(const FVector& Eye, TArray<FTgt>& Out) const;
	void OnHit(const FTgt& T, const FVector& Pt);
	void Report(const FVector& From, const FVector* Impact);
	void Eye(FVector& OutEye, FVector& OutDir) const;
	void UpdateGulls(double Dt, double T);
	void Render(double T);
	FVector FlareAt(const FFlare& F, double& OutGlow, bool& bOutOut) const;
	AKeysBoatPawn* Pawn() const;

	UPROPERTY() TObjectPtr<USceneComponent> Root;
	UPROPERTY() TObjectPtr<UProceduralMeshComponent> ViewModel;
	UPROPERTY() TObjectPtr<UProceduralMeshComponent> Solid;
	UPROPERTY() TObjectPtr<UProceduralMeshComponent> Glow;
	UPROPERTY() TObjectPtr<UPointLightComponent> FlareLight;
	UPROPERTY() TObjectPtr<UMaterialInstanceDynamic> GlowMID;

	bool bOut = false;
	double LookYaw = 0, LookPitch = .04, FireT = 0, Kick = 0, MuzzleT = 0, SlideT = 0;
	bool bFromEmpty = false;
	int32 BuiltMagSize = -1;
	bool bBuiltDot = false;
	TArray<FBullet> Bullets;
	TArray<FFloater> Floaters;
	TArray<FBrass> Brass;
	TArray<FFall> Falls;
	TArray<FCarcass> Carcasses;
	TArray<FStreak> Streaks;
	TArray<FFlare> FlaresUp;
	TArray<FGull> Gulls;
	double GCX = 0, GCZ = 0;
	bool bGullInit = false, bFirstFish = false, bFirstBird = false;
};
