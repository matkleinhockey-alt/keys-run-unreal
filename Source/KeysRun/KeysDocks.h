#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "KeysHuman.h"
#include "KeysLifts.h"
#include "KeysDocks.generated.h"

class UProceduralMeshComponent;
class UPointLightComponent;
class UMaterialInstanceDynamic;

// Where the private docks of the real map stand: one in front of each conch house that has the water close by and
// deep enough within reach, and nothing else in the way. Worked out once from KeysGeo and the same on every run.
// For anything that has to keep clear of them (the scenery's palms, a boat's hull).
namespace KeysDocks
{
	enum EShape : uint8 { ShapeT, ShapeL, ShapeFinger, ShapePlatform, ShapeCount };
	// a rectangle in a dock's own frame: A metres out from the water's edge along the dock, C metres to its right
	struct FRectL { double A0 = 0, A1 = 0, C0 = 0, C1 = 0; };

	struct FDock
	{
		double X0 = 0, Z0 = 0;        // where the walkway crosses the water's edge, game metres
		double NX = 0, NZ = 1;        // the way it runs out, a unit vector
		double HX = 0, HZ = 0;        // the house it belongs to
		double Len = 0;               // the walkway, from the water's edge to the head
		double Back = 0, BackZ = 0;   // how far it runs inland to reach the yard, and the ground's height there
		double W = 1.6, DeckZ = 1.4;
		double HeadW = 0, HeadD = 0;  // the head: across the dock and along it
		uint8 Shape = ShapeT, Finish = 0;
		int32 Side = 1;               // the side the lift (and an L's arm) is on: 1 right, -1 left
		KeysLifts::FLift Lift;
		bool bUnder = false;          // lights under the water off the head
		bool bPocket = false;         // the shore was too thin: a berth is dredged under the head, the lift and the boat
		bool bCanal = false;          // on a canal's seawall: a platform along the wall, the boat lying along it too
		bool bAlong = false;          // a boat afloat alongside
		KeysYardProps::FBoat AlongBoat;
		double AlongA = 0, AlongC = 0, AlongYaw = 0;
		int32 Person = 0;             // 0 nobody, 1 someone fishing off the head, 2 someone hosing the boat down
		int32 Seed = 0;
		FString Island;
		double HeadDepth = 0;         // the water at the head at low tide, m
		TArray<FRectL> Parts;         // what it covers: walkway, head, lift, the boat alongside
		FRectL Box;                   // and the box round all of that
		// dock frame -> game metres
		FVector2D At(double A, double C) const { return FVector2D(X0 + NX * A - NZ * C, Z0 + NZ * A + NX * C); }
		void Local(double X, double Z, double& A, double& C) const { const double DX = X - X0, DZ = Z - Z0; A = DX * NX + DZ * NZ; C = -DX * NZ + DZ * NX; }
	};

	// every dock, in a fixed order; empty on the classic map and with -KRNoDocks
	const TArray<FDock>& Sites();
	// on (or within Margin metres of) a dock, its lift or the boat alongside it
	bool InFootprint(double X, double Z, double Margin = 0);
	// a berth dredged under a dock on a thin shore: how deep the bottom is cut there (m at the datum, feathered out
	// to nothing round its edge). Read by KeysGeo for the ground and the depth; false until the docks are planned.
	bool Pocket(double X, double Z, double& OutDepth);
}

// The private docks and boat lifts along the built-up shores of the real map (KeysDocks.cpp, KeysLifts.cpp).
// Every dock is built into the meshes of its 600 m tile: the structure (seen from far off), hung boats, the small
// fittings (only close to), what floats (boats afloat and lowered lifts, drawn at the tide) and the night lights.
// Nothing moves or is rebuilt for a tile more than 600 m from the camera.
UCLASS()
class KEYSRUN_API AKeysDocks : public AActor
{
	GENERATED_BODY()

public:
	AKeysDocks();
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	static AKeysDocks* Get(const UObject* WorldContext);
	// test hooks ("dk:<cmd>" in a script): stats, audit, where (logs every dock and the busiest shores),
	// go=<dock>[,off[,across]] (lie the boat off that dock), look=<dock>[,dist[,height[,angledeg]]] (a camera on its
	// lift), eye=<dock>,along,across,height,yawdeg[,pitchdeg] (a camera in that dock's frame; eye=off).
	// <dock> is a number or words such as cradle+hung, davits, elevator#1, fish, hose, under (KeysDocks.cpp FindDock)
	FString DebugCmd(const FString& Cmd);

private:
	struct FTile
	{
		double X = 0, Z = 0;          // its corner, game metres
		double CX = 0, CZ = 0, R = 0; // the middle of its docks and how far they reach from it
		int32 Docks = 0;
		UProceduralMeshComponent* Float = nullptr;
		UProceduralMeshComponent* Lamps = nullptr;
		TArray<FVector> LampAt, UnderAt;   // the lamps and the lights under the water, game metres as (x, z, up)
		double FloatZ = 0;
	};
	struct FPerson { int32 Dock = 0; FKeysHuman Hm; double Ph = 0; };

	UProceduralMeshComponent* NewMesh(const FVector& AtUE, USceneComponent* Parent, double DrawDistM, bool bShadow);
	void Audit() const;
	void LogSites() const;
	void TickPeople(double T);
	void BuildPerson(const FPerson& P, int32 Slot, double T);

	UPROPERTY() TObjectPtr<USceneComponent> Root;
	UPROPERTY() TArray<TObjectPtr<UProceduralMeshComponent>> Meshes;
	UPROPERTY() TArray<TObjectPtr<UPointLightComponent>> Lights;
	UPROPERTY() TArray<TObjectPtr<UProceduralMeshComponent>> PersonMesh;
	UPROPERTY() TArray<TObjectPtr<UProceduralMeshComponent>> SprayMesh;
	UPROPERTY() TObjectPtr<UMaterialInstanceDynamic> GlowMID;
	UPROPERTY() TObjectPtr<UMaterialInstanceDynamic> SprayMID;
	UPROPERTY() TObjectPtr<class ACameraActor> EyeCam;
	TArray<FTile> Tiles;
	TArray<FPerson> People;
	int32 PersonAt[2] = {-1, -1};
	bool bBuilt = false, bNight = false, bAwake = false;
	double SlowT = 0, PeopleT = 0;
	int32 Tris[5] = {0, 0, 0, 0, 0};   // structure, boats, detail, float, lights
	int32 AwakeTiles = 0;
};
