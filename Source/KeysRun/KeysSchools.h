#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "KeysSchools.generated.h"

class UInstancedStaticMeshComponent;
class UStaticMesh;
class UMaterialInstanceDynamic;
class UTexture2D;
struct FKeysMesh;

// The living schools under the boat: bait balls of silversides and herring in mid-water, snapper and grunt schools
// hovering on coral heads and wrecks, grouper by their holes, barracuda hanging still, bonefish and permit tailing
// across the flats, jacks and sharks hunting through it all. Ambient only: the spearable fish stay AKeysLife's.
//
// Motion runs mostly on the GPU (M_KR_Fish). Each school is a small tree of leaders: one main leader that steers
// (wanders its habitat, keeps off the bottom, the coral and the wrecks and under the surface, runs from the diver, the
// boat and predators) and a few sub-leaders that trail it along its own track with a delay, keeping their place in
// the formation (and breaking away when a predator cuts them off, rejoining later). Only the leaders are simulated
// here. Twenty times a second their state is written into a float texture (one row per leader, ~2.4 s of history),
// and every fish is an instance that never moves: the material reads where its leader was a moment ago and places,
// turns and bends the fish around it, so turns ripple back through the school. Instance data is only rewritten when
// a school appears, leaves, changes detail level or drifts far from where its instances are anchored.
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
	// test hooks (-KRScript fish:...): bait, snapper, grouper, cuda, flats, pred, scare, dash, clear, stats
	void DebugCmd(const FString& Cmd);

	enum class ERole : uint8 { Bait, Reef, Hole, Hanger, Flats, Hunter, Patrol };

	struct FLeader
	{
		FVector P = FVector::ZeroVector, V = FVector::ZeroVector;   // game meters, y up
		FVector Fwd = FVector(1, 0, 0);
		FVector Slot = FVector::ZeroVector;   // place in the main leader's frame (m): x ahead, y right, z up
		double Delay = 0;                      // how far behind the main leader's track it swims (s)
		double Phase = 0, Amp = .4, Alarm = 0, ThreatK = 0, LooseT = 0, Ext = 1;
		FVector ThreatP = FVector::ZeroVector;
		FVector LooseDir = FVector(1, 0, 0);
		int32 Row = -1;
	};

	struct FSchool
	{
		int32 Id = 0, Sp = 0, Lod = -1, Pool = -1, First = 0, NFish = 0;
		double Len = .3, Radius = 1, Ball = 0, GoalT = 0, Step = 0, HuntT = 0, HuntLeft = 0;
		FVector Home = FVector::ZeroVector, Goal = FVector::ZeroVector, HuntDir = FVector::ZeroVector;
		double HomeTop = 0;
		int64 Cell = 0;
		int32 PreyId = -1;
		bool bPinned = false;
		FVector AnchorUE = FVector::ZeroVector;
		TArray<FLeader> L;     // [0] is the main leader
		TArray<float> Data;    // 8 custom floats per fish (see M_KR_Fish)
		// the main leader's track (what the texture holds), for the sub-leaders to follow
		TArray<FVector> TrackP, TrackF;
		int32 TrackN = 0;
		FRandomStream Rng;
	};

	struct FThreat
	{
		FVector P = FVector::ZeroVector, V = FVector::ZeroVector;
		double R = 5, Power = 1;
		uint8 Kind = 0;   // 0 diver, 1 boat, 2 predator, 3 startle
		int32 SchoolId = -1;
	};

private:
	struct FTile { float Top[32 * 32]; float Floor[32 * 32]; };

	void ReadWorld(double Dt);
	void Populate();
	FSchool* Spawn(int32 Sp, const FVector& Home, double HomeTop, int64 Cell, uint32 Seed, double NScale = 1);
	void Despawn(int32 Index);
	void StepSchool(FSchool& S, double Dt, double T);
	void SteerLeader(FSchool& S, int32 K, double Dt);
	void MainGoal(FSchool& S, double Dt);
	FVector TrackAt(const FSchool& S, double Delay, FVector& OutFwd) const;
	void WriteSample();
	void FlushPools();
	void SetupTexture();
	int32 AllocRow();
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
	UPROPERTY() TObjectPtr<UTexture2D> Tex;

	TArray<int32> PoolKey;          // Sp * 3 + Lod, per pool
	TArray<bool> PoolDirty;
	TArray<FSchool> Schools;
	TArray<FThreat> Threats;
	struct FStartle { FVector P; double R, Left; };
	TArray<FStartle> Startles;
	TMap<int64, int32> Cells;       // spawn cell -> school id (or -1: rolled, nothing there)
	TMap<int64, TUniquePtr<FTile>> Tiles;
	TArray<int32> FreeRows;
	TArray<int32> FreshRows;        // rows whose whole history must be filled at the next sample
	TArray<FLinearColor> Column;    // the sample being written: Rows x 4 texels
	int32 TilesThisFrame = 0;
	int32 NextId = 1, LastPut = -1;
	int64 SampleIdx = 0;
	double SampleT = 0, PopT = 0, PredT = 0, StatT = 0, NowT = 0;
	FVector Cam = FVector::ZeroVector;   // game meters
	FVector BoatP = FVector::ZeroVector, BoatV = FVector::ZeroVector;
	bool bHaveBoat = false, bDiving = false, bScriptLog = false, bOff = false;
	TArray<FVector4> LifePreds;
	// rough timing for the log
	double CpuAcc = 0, FrameAcc = 0;
	int32 CpuN = 0, Uploads = 0, Rebuilds = 0, Rebases = 0;
};
