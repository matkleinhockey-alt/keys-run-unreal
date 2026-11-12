#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "KeysData.h"
#include "KeysTraffic.generated.h"

class UProceduralMeshComponent;
class AKeysBoatPawn;

// A closed loop of waypoints on the flat (x, z) map, game meters, with cumulative arc length.
struct FKeysRoute
{
	TArray<FVector2D> Pts;   // (x, z)
	TArray<double> Cum;      // Cum[i] = distance to Pts[i]; Cum[Num] = Total
	double Total = 0;
};

// One AI boat: a go-fast racer or a piece of traffic (center console, sportfisher, sailboat, wake boat or jet ski).
struct FKeysAIBoat
{
	enum EKind : uint8 { Racer, CC, Sport, Sail, Wake, JetSki };
	EKind Kind = CC;
	FKeysRoute R;
	double L = 9, B = 3;
	double S = 0, Speed = 0, Off = 0, OffT = 0, H = 0, RelT = 0, WT = 0, Ph = 0, HopT = 0, Hop = 0, HitT = -9;
	bool bLocal = false;
	double X = 0, Z = 0;    // where it is now, game meters
	int32 Mesh = -1;        // index into AKeysTraffic::Meshes
	int32 Surfer = -1;      // index into AKeysTraffic::Surfers (wake boats only)
};

// Owns every AI boat on the water: the three go-fast racers circling the Keys and the 23 boats of everyday traffic.
// Ported from makeRoute / routeAt / closestS / makeRacer / updateRacers / routeFrom / makeTraffic / updateTraffic in the JS game.
UCLASS()
class KEYSRUN_API AKeysTraffic : public AActor
{
	GENERATED_BODY()

public:
	AKeysTraffic();
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

	// for the chart: positions in game meters (x, z)
	void GetChartDots(TArray<FVector2D>& OutTraffic, TArray<FVector2D>& OutRacers) const;
	const TArray<FKeysAIBoat>& Racers() const { return RacerBoats; }
	const TArray<FKeysAIBoat>& Traffic() const { return TrafficBoats; }

	// JS spotClear / unstick: is (X, Z) open water clear of docks and pilings for this hull?
	static bool SpotClear(double X, double Z, const KeysData::FBoatSpec& S);
	// spirals out up to 400 m for a clear spot; leaves X, Z alone and returns false if there is none
	static bool FindClearSpot(double& X, double& Z, const KeysData::FBoatSpec& S);

	// route helpers (public so other systems, e.g. a buddy boat, can reuse them)
	static FKeysRoute MakeRoute(double DzOut, double DzBack);
	static FKeysRoute RouteFrom(const TArray<FVector2D>& Pts);
	// position (X, Z) and unit tangent (TX, TZ) at arc length S
	static void RouteAt(const FKeysRoute& R, double S, double& X, double& Z, double& TX, double& TZ);
	static double ClosestS(const FKeysRoute& R, double X, double Z);

private:
	void BuildRacers();
	void BuildTraffic();
	void UpdateRacers(double Dt, double T, AKeysBoatPawn* P, const FVector2D& Cam);
	void UpdateTraffic(double Dt, double T, AKeysBoatPawn* P, const FVector2D& Cam);
	UProceduralMeshComponent* NewMesh(const FName& Name, USceneComponent* Parent);

	UPROPERTY() TObjectPtr<USceneComponent> Root;
	UPROPERTY() TArray<TObjectPtr<UProceduralMeshComponent>> Meshes;
	UPROPERTY() TArray<TObjectPtr<UProceduralMeshComponent>> Surfers;

	TArray<FKeysAIBoat> RacerBoats;
	TArray<FKeysAIBoat> TrafficBoats;
	bool bBuilt = false;
};
