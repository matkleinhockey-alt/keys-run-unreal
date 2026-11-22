#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "KeysSchools.generated.h"

class UInstancedStaticMeshComponent;
class UStaticMesh;
class UMaterialInstanceDynamic;
struct FKeysMesh;

// The living schools under the boat: bait balls of silversides and herring in mid-water, snapper and grunt schools
// hovering on coral heads and wrecks, grouper by their holes, barracuda hanging still, bonefish and permit tailing
// across the flats, jacks and sharks hunting through it all. Ambient only: the spearable fish stay AKeysLife's.
//
// Every fish is its own boid. It sees only its few nearest neighbours of its own kind (found through a spatial hash
// per species, so a thousand-odd fish cost little): it keeps its distance from them, matches their heading and drifts
// toward them. On top of that each fish keeps to its species' depth band and spacing, feels its way round coral heads,
// wrecks and the bottom, follows its school's loose intent (wander the habitat, hold a coral head, hunt), and runs from
// threats - a predator, the boat, a lure, the diver. Fear spreads from fish to fish faster than any of them can swim,
// so a scare runs through a school as a wave; each fish that catches it rolls onto its side, flashing. A bait ball
// under attack tightens and mills round in a vortex; a school that parts round a predator or a coral head closes up
// again behind it. Schools far from the camera are stepped less often.
//
// Rendering: one instanced static mesh per species and detail level; the instances are the fish, moved every time
// their school is stepped. The material (M_KR_Boid) bends the body and beats the tail with world position offset,
// the tail's frequency following each fish's swimming speed (carried in per-instance custom data).
UCLASS()
class KEYSRUN_API AKeysSchools : public AActor
{
	GENERATED_BODY()

public:
	AKeysSchools();
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

	static AKeysSchools* Get(const UObject* WorldContext);

	// a shot, a spear hissing past, a splash: fish near (X, Z) bolt (game meters)
	void Startle(double X, double Z, double Radius);
	// test hooks (-KRScript fish:...): bait, herring, snapper, grunt, grouper, cuda, flats, pred, shark, scare, look,
	// put=<species>, clear, clearall, off, on, stats, body, sound (KRSound: the water under the boat, afloat or aground), canals (KRCanal: canals with no way out)
	void DebugCmd(const FString& Cmd);

	enum class ERole : uint8 { Bait, Reef, Hole, Hanger, Flats, Hunter, Patrol };

	struct FFish
	{
		FVector P = FVector::ZeroVector, V = FVector::ZeroVector;   // game meters, y up
		FVector Fwd = FVector(1, 0, 0);
		FVector FleeDir = FVector::ZeroVector;
		double Roll = 0, YawRate = 0;
		double Pace = 1;                       // this fish's own speed, round the school's
		double Phase = 0, Freq = 6, Amp = .3;  // tail beat (rad, rad/s, 0-1)
		double Alarm = 0, AlarmIn = 0, Flash = 0, FlashSide = 1;
		double Len = .3, Shade = 1, Seed = 0;
		double BandY = 0;                      // the depth it likes, inside its species' band
	};

	struct FSchool
	{
		int32 Id = 0, Sp = 0, Lod = -1, Pool = -1, First = 0;
		double Len = .3, Radius = 1, GoalT = 0, Step = 0, HuntT = 0, HuntLeft = 0;
		double WanderYaw = 0, WanderTurn = 0, Mill = 0, MillDir = 1, Alarm = 0;
		bool bWander = false;
		double Pace = 1;
		FVector Home = FVector::ZeroVector, Goal = FVector::ZeroVector, HuntDir = FVector::ZeroVector;
		FVector Center = FVector::ZeroVector, CenterV = FVector::ZeroVector, Heading = FVector(1, 0, 0);
		double HomeTop = 0;
		int64 Cell = 0;
		int32 PreyId = -1;
		bool bPinned = false, bStepped = false;
		int32 Reef = -1;                        // the named reef it belongs to (KeysSchools.cpp ReefSites), or -1
		TArray<FFish> Fish;
		FRandomStream Rng;
	};

	struct FThreat
	{
		FVector P = FVector::ZeroVector, V = FVector::ZeroVector;
		double R = 5, Power = 1;
		uint8 Kind = 0;   // 0 diver, 1 boat, 2 predator, 3 startle, 4 lure
		int32 SchoolId = -1;
	};

private:
	struct FTile { float Top[32 * 32]; float Floor[32 * 32]; };

	void ReadWorld(double Dt);
	void Populate();
	FSchool* Spawn(int32 Sp, const FVector& Home, double HomeTop, int64 Cell, uint32 Seed, double NScale = 1, bool bMakeRoom = false);
	int32 WrapCount = 0;   // schools that swam out of the bubble and came back in on the far side
	void Despawn(int32 Index);
	void StepSchool(FSchool& S, double Dt);
	void StepFish(FSchool& S, int32 I, double Dt, int32 MaxNbr);
	void MainGoal(FSchool& S, double Dt);
	void BuildGrid();
	void WriteInstances(FSchool& S);
	void FlushPools();
	double TopAt(double X, double Z, double* OutFloor = nullptr);
	const FTile* TileFor(int32 TX, int32 TZ);
	int32 PoolFor(int32 Sp, int32 Lod);
	bool ValidWater(int32 Sp, double X, double Z) const;
	FVector PickGoal(FSchool& S);
	FSchool* Find(int32 Id);
	FSchool* NearestOfRole(const FVector& P, bool bBaitOnly, int32 NotId, double MaxD);

	UPROPERTY() TObjectPtr<USceneComponent> Root;
	UPROPERTY() TArray<TObjectPtr<UInstancedStaticMeshComponent>> Pools;
	UPROPERTY() TArray<TObjectPtr<UStaticMesh>> Meshes;
	UPROPERTY() TObjectPtr<UMaterialInstanceDynamic> MID;

	TArray<int32> PoolKey;          // Sp * 3 + Lod, per pool
	TArray<bool> PoolDirty;         // its instances must be laid out again (a school came, went or changed detail)
	TArray<bool> PoolMoved;         // its instances were moved this frame
	TArray<FSchool> Schools;
	TArray<FThreat> Threats;
	struct FStartle { FVector P; double R, Left; };
	TArray<FStartle> Startles;
	TMap<int64, int32> Cells;       // spawn cell -> school id (or -1: rolled, nothing there)
	TMap<int64, TUniquePtr<FTile>> Tiles;
	// the neighbour grid: every fish hashed by species and cell (cell size = its species' sight)
	struct FRef { int32 School, Fish; };
	TArray<FRef> GridItems;
	TArray<int32> GridNext;
	TMap<int64, int32> GridHead;
	int32 TilesThisFrame = 0;
	int32 NextId = 1, LastPut = -1;
	double PopT = 0, PredT = 0, StatT = 0, NowT = 0;
	FVector Cam = FVector::ZeroVector;   // game meters
	FVector PopCam = FVector::ZeroVector;   // where the camera was at the last Populate
	double SurfY = 0;                    // the water level now (KeysOcean::SeaLevel)
	FVector BoatP = FVector::ZeroVector, BoatV = FVector::ZeroVector;
	bool bHaveBoat = false, bDiving = false, bScriptLog = false, bOff = false;
	bool bSoundLog = false;              // fish:sound - a KRSound line a second
	double SoundT = 0;
	TArray<FVector4> LifePreds;
	// rough timing and behaviour for the log
	double CpuAcc = 0, FrameAcc = 0, NbrAcc = 0;
	int32 CpuN = 0, Rebuilds = 0, FishSteps = 0, Flashes = 0;
};
