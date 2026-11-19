#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "KeysTrim.generated.h"

// Outboard trim, tilt and trim tabs, one profile per hull. Every number lives in Content/KeysRun/Data/DA_TrimTuning so
// the feel can be tweaked in the editor; the defaults below are what a fresh asset starts with (and what the game uses
// if the asset is missing). The physics that reads them is in AKeysBoatPawn::UpdateTrim (KeysTrimPhysics.cpp).
USTRUCT(BlueprintType)
struct FKeysTrimProfile
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hull") FName BoatId;
	// inboard diesels (the Viking): no trim or tilt, the tabs do everything
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hull") bool bOutboards = true;

	// ---- range, degrees (negative = trimmed in / bow down)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Range") float TrimMinDeg = -6.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Range") float TrimMaxDeg = 20.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Range") float TiltMaxDeg = 70.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Range") float TrimRateDegPerSec = 6.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Range") float TiltRateDegPerSec = 14.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Range") float TiltAllowedBelowKnots = 8.f;   // the tilt switch only works slow

	// ---- the ideal trim curve: X = speed as a fraction of top speed (0..1), Y = ideal trim in degrees, calm water.
	// Rough water moves it down by RoughTrimDownDeg at full sea state (more of it into a head sea).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ideal") TArray<FVector2D> IdealCurve;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ideal") float RoughTrimDownDeg = 5.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ideal") float SweetBandDeg = 2.5f;   // within this of ideal = full benefit

	// ---- speed and fuel
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Efficiency") float TopSpeedBonus = .08f;      // at ideal
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Efficiency") float FuelBurnSaving = .15f;     // at ideal
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Efficiency") float OffSpeedLossPerBand = .035f;   // each SweetBand further off
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Efficiency") float OffFuelGainPerBand = .08f;

	// ---- the hole shot (getting on plane)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hole shot") float HoleShotDownGain = .3f;   // accel bonus trimmed all the way in
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hole shot") float HoleShotUpLoss = .6f;     // accel loss trimmed all the way out
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hole shot") float BowRiseDeg = 9.f;         // extra bow rise trimmed out before planing

	// ---- trimmed too far in at speed: the bow plows
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Plow") float PlowStartDeg = 3.f;    // this far under ideal starts it
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Plow") float PlowSpeedLoss = .05f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Plow") float BowSteerGain = .9f;    // steering oversensitivity at full plow
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Plow") float PlowSpray = 2.5f;

	// ---- trimmed too far out at speed: porpoising, then ventilation
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Porpoise") float PorpoiseStartDeg = 3.5f;   // this far over ideal starts it
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Porpoise") float PorpoiseAmpDeg = 3.5f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Porpoise") float PorpoiseHz = 1.3f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Porpoise") float VentStartDeg = 7.f;        // this far over ideal blows the props out
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Porpoise") float VentThrustLoss = .35f;

	// ---- rough water
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rough") float SlamPerDegUp = .07f;     // slam strength per degree over calm ideal
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rough") float LaunchPerDegUp = .0045f; // extra bow lift (rad) per degree, scaled by sea
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rough") int32 StressRepairCost = 900;  // Charter mode: a cracked stringer

	// ---- shallow water: the lower unit
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shallow") float PropDepthM = .55f;      // prop tip below the hull's waterline at 0 deg
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shallow") float LowerUnitM = .9f;       // pivot to prop tip: how much tilting lifts it
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shallow") float HullDraftM = .45f;      // the hull itself (it grounds here)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shallow") float TiltedMaxKnots = 3.f;   // tilted up: barely more than idle
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shallow") int32 PropStrikeCost = 650;   // Charter mode repair bill

	// ---- trim tabs
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tabs") float TabAuthorityDeg = 5.f;     // roll one tab fully down gives at speed
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tabs") float TabRatePerSec = .5f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tabs") float WindLeanDegPerMs = .22f;   // heel from a beam wind
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tabs") float CrewLeanDeg = 1.6f;        // the angler at the starboard rail

	double IdealAt(double Fr) const;
};

UCLASS(BlueprintType)
class KEYSRUN_API UKeysTrimTuning : public UPrimaryDataAsset
{
	GENERATED_BODY()
public:
	UKeysTrimTuning();
	// one per hull, by FBoatSpec::Id; a hull not listed uses Fallback
	UPROPERTY(EditAnywhere, Category = "Trim") TArray<FKeysTrimProfile> Boats;
	UPROPERTY(EditAnywhere, Category = "Trim") FKeysTrimProfile Fallback;

	// the loaded asset (or these defaults), and the profile for a hull
	static const UKeysTrimTuning* Get();
	static const FKeysTrimProfile& For(const FString& BoatId);
};
