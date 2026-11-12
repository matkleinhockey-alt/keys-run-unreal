#pragma once

#include "CoreMinimal.h"

// Free-diving and spearfishing rules, ported from the web game's pure modules:
//   shared/sim/diver.ts     breath-hold air, ATA burn, buoyancy, exertion, narcosis, blackout and shallow-water
//                           blackout, pulsed fin kicks, body-frame drag against the moving water, the ascend assist
//   shared/sim/dive-gear.ts per-dive loadout (no shop in this port, so always the no-gear loadout)
//   shared/sim/spear.ts     the band gun's hauled reload, the shaft's drag-limited flight, aim assist with lead,
//                           capsule hit tests, the hold chance, and the fight with a speared fish on the float line
// Game meters, y up (KeysGeo). Yaw convention is the boat's: forward = (-sin(yaw), 0, -cos(yaw)).
namespace KeysDive
{
	// ---- diver tuning (sim/diver.ts)
	constexpr double AirMax = 180;
	constexpr double SurfaceEps = .3;
	constexpr double NeutralDepth = 11;
	constexpr double BuoySurfaceAccel = 1.4;
	constexpr double BuoyDriftUpAccel = .12;
	constexpr double NarcosisDepth = 25;
	constexpr double ShallowBlackoutDepth = 5;
	constexpr double ShallowBlackoutAirFraction = .04;
	constexpr double ShallowBlackoutMinAscent = .15;
	constexpr double ExertionAirGain = 1.2;
	constexpr double SwimKickAccel = 2.6, SprintMult = 1.9, VerticalKickAccel = 2.2;
	constexpr double AscendSpeed = 4.6, AscendSpeedSprint = 6.4, AscendResponse = 12, AscendEaseDepth = 1.5;
	constexpr double BackKickFraction = .45;
	constexpr double DragAxialLin = .3, DragAxialQuad = .7, DragLateralLin = 1.5, DragLateralQuad = 1.2, VerticalLateralDragScale = .6;
	constexpr double KickPeriod = 1.1, KickPeriodSprint = .6;
	constexpr double CurrentCoupling = .75, BoundaryMin = .3, BoundaryH = 4;
	constexpr double ExertionRiseRate = 2.5, ExertionFallRate = .6;
	constexpr double BlackoutWakeDelay = 2.5;
	constexpr double DiverRadius = .35;
	constexpr double StepDt = 1.0 / 30;   // fixed step, the same rate the web game runs the boat and the diver at

	double AtaAt(double Depth);
	double BuoyancyAccel(double Depth);
	double AirBurnRate(double Depth, double Exertion);
	double KickPulse(double Phase);
	FVector2D WaterVelocity(double X, double Y, double Z, double T, double FloorY);

	struct FInput
	{
		bool bFwd = false, bBack = false, bLeft = false, bRight = false, bAscend = false, bDescend = false, bSprint = false;
		double LookYaw = 0, LookPitch = 0;
	};

	enum class EEvent : uint8 { BlackoutAir, BlackoutShallow, Surfaced, Submerged, GroundHit };

	struct FState
	{
		double X = 0, Y = 0, Z = 0, VX = 0, VY = 0, VZ = 0;
		double Yaw = 0, Pitch = 0;
		double Air = AirMax, AirMaxV = AirMax;
		double Exertion = 0, KickPhase = 0;
		bool bKicking = false, bNarcosis = false, bBlackedOut = false;
		double BlackoutT = 0;
		TArray<EEvent> Events;
		double Depth() const { return FMath::Max(0.0, -Y); }
		double Speed() const { return FMath::Sqrt(VX * VX + VY * VY + VZ * VZ); }
	};

	FState CreateState(double X, double Y, double Z, double Yaw, double Air = AirMax);
	// one fixed step, in place (Events is reset first)
	void Step(FState& S, const FInput& In, double T, double Dt, double SwimMul = 1);

	// ---- spear (sim/spear.ts)
	constexpr double SpearSpeed = 25, SpearRange = 18, SpearReload = 1.8, SpearReloadFloor = 1.6, SpearDrawTime = 2.8;
	constexpr int32 SpearDrawNotches = 3;
	constexpr double SpearDrawSlip = .45, SpearDrawSwimSpeed = 1.4, SpearDrawMinRate = .35;
	constexpr double AssistCone = .17, AssistStrength = .65;
	constexpr double SpearTetherM = 15;

	double DragKFor(double Range);
	double DistAtFlightTime(double T, double Range = SpearRange);
	double TimeToDistance(double Dist, double Range = SpearRange);
	double ShaftSpeedAtDist(double Dist, double Range = SpearRange);
	double HoldChance(double ImpactSpeed);
	double DrawRateFor(double Speed);

	struct FGun { double ReloadT = 0, Draw = 1; };
	inline bool CanFire(const FGun& G) { return G.ReloadT <= 0 && G.Draw >= 1; }
	void StepReload(FGun& G, double Dt, bool bDrawing, double Speed);
	inline FGun StartReload() { FGun G; G.ReloadT = FMath::Max(SpearReloadFloor, SpearReload); G.Draw = 0; return G; }

	struct FShot
	{
		FVector O = FVector::ZeroVector, D = FVector(0, 0, -1);   // game meters / unit direction
		double Dist = 0, T = 0, Range = SpearRange;
		bool bAlive = false;
		FVector At(double Dd) const { return O + D * Dd; }
	};

	// a spearable fish: its body as a capsule, plus what it is
	struct FTarget
	{
		int32 Id = 0;
		FString Key;
		double Weight = 0;
		FVector A, B;   // capsule ends, game meters
		double Radius = .2;
		FVector Vel = FVector::ZeroVector;
		bool bCatchable = true;
	};

	struct FHit
	{
		int32 Id = 0;
		double AtDist = 0, ImpactSpeed = 0;
		FVector Point = FVector::ZeroVector;
		bool bHeld = true;
	};

	FShot Fire(const FVector& Origin, const FVector& Dir, double Range = SpearRange);
	// nudges Dir toward the best target in the cone, leading it for the shaft's flight time; returns the new direction
	FVector AssistAim(const FVector& Origin, const FVector& Dir, const TArray<FTarget>& Targets, int32& OutTargetId,
		double Strength = AssistStrength, double Range = SpearRange, const FVector* Eye = nullptr);
	double ClosestDistSqSegSeg(const FVector& P1, const FVector& P2, const FVector& Q1, const FVector& Q2, double& OutS);
	// advance the shaft; true and OutHit filled if it struck something this step
	bool StepSpear(FShot& Shot, const TArray<FTarget>& Targets, double Dt, FHit& OutHit);

	struct FFightParams { FString Key; double Weight = 0, Str = 1, TetherMax = SpearTetherM; };
	enum class EFight : uint8 { Fighting, Landed, TornFree };
	struct FFight
	{
		double X = 0, Z = 0, Tension = .5, Stam = 1, ThrashT = 0, OverT = 0, Dist = 0;
		EFight Outcome = EFight::Fighting;
	};
	FFightParams FightParamsFor(const FString& Key, double Weight);
	FFight StartFight(double FishX, double FishZ);
	void StepFight(FFight& F, bool bHauling, const FFightParams& P, double DiverX, double DiverZ, double Dt);
}
