#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "KeysHuman.h"
#include "KeysTraffic.h"
#include "KeysLocals.generated.h"

class UProceduralMeshComponent;
class UTextRenderComponent;
class AKeysBoatPawn;

// The locals: the people and boats who live out on this water. Ported from the web game's
//   entities/life/commercial.ts - the working fleet: Hot Tuna (king mackerel troller), Dog Pound (stone crabber),
//     Pin Wheel (shrimp trawler), No Limits (lobster boat) and Fat Tuna (western-rig trawler), each on its grounds
//     at working speed with her name and port on the transom
//   entities/life/larry.ts - Larry's POS, a plywood raft on three oil drums with a tent and a 2 hp kicker, out on
//     some eleven-minute windows of the wall clock on one of five slow loops; "yeahhhhh dude"
//   entities/life/sandbar.ts - the sandbar party: eight boats rafted round a bar you can stand on, people waist deep,
//     floats, on one of the real bars the bathymetry has, a different one every forty minutes
//   entities/life/party-boats.ts - boats on the hook off Sombrero Beach, East Sister Rock and Bamboo Key, music on
//   entities/life/dog.ts - the boat's terrier, who trots the cockpit, braces at speed and goes mad for a fish
UCLASS()
class KEYSRUN_API AKeysLocals : public AActor
{
	GENERATED_BODY()

public:
	AKeysLocals();
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	static AKeysLocals* Get(const UObject* WorldContext);

	// for the chart: where the party is today (false if the water has no bar), Larry (false while he is not out)
	bool SandbarSite(double& OX, double& OZ) const;
	bool LarryPos(double& OX, double& OZ) const;
	void FleetDots(TArray<FVector2D>& Out) const;
	// test hooks
	FString DebugCmd(const FString& Cmd);

private:
	void BuildFleet();
	void BuildLarry();
	void BuildSandbar();
	void BuildParties();
	void TickFleet(double Dt, double T, const FVector& Cam);
	void TickLarry(double Dt, double T, const FVector& Cam);
	void TickSandbar(double Dt, double T, const FVector& Cam);
	void TickParties(double Dt, double T, const FVector& Cam);
	void TickDog(double Dt, double T, AKeysBoatPawn* P);
	void ApplySandbar(int32 I);
	UProceduralMeshComponent* NewMesh(const TCHAR* Name);

	UPROPERTY() TObjectPtr<USceneComponent> Root;

	// ---- the working fleet
	struct FWorkBoat { FString Name; int32 Kind; double L, B, S, Speed, H, Ph; FKeysRoute R; double X = 0, Z = 0; };
	TArray<FWorkBoat> Fleet;
	UPROPERTY() TArray<TObjectPtr<UProceduralMeshComponent>> FleetMeshes;
	UPROPERTY() TArray<TObjectPtr<UTextRenderComponent>> FleetNames;

	// ---- Larry
	TArray<FKeysRoute> LarryLoops;
	bool bLarryOut = false, bLarryForce = false;
	int32 LarryRoute = 0;
	double LarryS = 0, LarryH = 0, LarryX = 0, LarryZ = 0, LarrySay = 0, LarryBubble = 0;
	bool bLarryShown = false;
	UPROPERTY() TObjectPtr<UProceduralMeshComponent> Larry;
	UPROPERTY() TObjectPtr<UProceduralMeshComponent> LarryMan;
	FKeysHuman LarryH0;

	// ---- the sandbar party
	struct FBar { double X, Z, Depth, Relief; };
	TArray<FBar> Bars;
	int32 BarI = -1;
	struct FBarBoat { int32 Hull; double LX, LZ, Yaw, Ph; };
	TArray<FBarBoat> BarBoats;
	struct FBarFolk { FKeysHuman Hm; double LX, LZ, RR, Yaw; };
	TArray<FBarFolk> BarFolk;
	struct FBarFloat { double LX, LZ, Spin; int32 Kind; };
	TArray<FBarFloat> BarFloats;
	UPROPERTY() TObjectPtr<UProceduralMeshComponent> BarSand;
	UPROPERTY() TObjectPtr<UProceduralMeshComponent> BarPeople;
	UPROPERTY() TObjectPtr<UProceduralMeshComponent> BarFloatMesh;
	UPROPERTY() TArray<TObjectPtr<UProceduralMeshComponent>> BarBoatMeshes;
	double BarPeopleT = 0;

	// ---- party boats on the hook
	struct FMoored { int32 Hull; double AX, AZ, BaseYaw, Ph, Len, TieX, TieZ, Bpm; TArray<FKeysHuman> Crew; };
	TArray<FMoored> Moored;
	UPROPERTY() TArray<TObjectPtr<UProceduralMeshComponent>> MooredMeshes;
	UPROPERTY() TArray<TObjectPtr<UProceduralMeshComponent>> MooredCrew;
	UPROPERTY() TObjectPtr<UProceduralMeshComponent> MooredRopes;

	// ---- the boat dog
	UPROPERTY() TObjectPtr<UProceduralMeshComponent> Dog;
	TWeakObjectPtr<AActor> DogOn;
	double DogX = 0, WantX = 0, ActT = 3, StepPh = 0, Wag = 0, BarkT = 12, LastSp = 0, DogDir = 1;
	int32 DogAct = 1;   // 0 trot, 1 sit, 2 brace
	double LastCaughtSeen = -1, ExciteT = 0;
};
