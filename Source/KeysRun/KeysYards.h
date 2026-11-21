#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "KeysHuman.h"
#include "KeysForklift.h"
#include "KeysYardProps.h"
#include "KeysYards.generated.h"

class UProceduralMeshComponent;
class UTextRenderComponent;
class UPointLightComponent;
class UMaterialInstanceDynamic;

// Where the dry stack and the boatyard stand: Cowfish Cove, the little cove on the Gulf side just east of Keys
// Fisheries on the real map, the dry stack on its west bank and the boatyard on its east. For anything that has to
// keep clear of them or shape the ground for them (the scenery, the mangroves, KeysGeo). Nothing on the classic map.
namespace KeysYards
{
	struct FSite
	{
		FString Name;
		double X = 0, Z = 0;           // the middle of it, game metres
		double BoatX = 0, BoatZ = 0;   // where to lie in a boat to look at it
		double BoatHeading = 0;
	};
	// the dry stack (0) and the boatyard (1); empty where there is no place for them
	const TArray<FSite>& Sites();
	// on a yard's made ground or in its dredged basin, Margin metres wider: nothing else should be built or grown here
	bool InFootprint(double X, double Z, double Margin = 0);
	// the ground the yards need: true inside them, with OutH the level of the made ground (> 0: the pads) or of the
	// dredged bottom (< 0: the basin and the travel lift's slip)
	bool Ground(double X, double Z, double& OutH);
}

// The dry stack marina and the boatyard.
//   The dry stack: an open-fronted steel rack building four bunks high with boats racked bow-in, a marina forklift
//   that pulls one out, carries it down the apron and lowers it over the bulkhead into the launch well (and later
//   puts it back), a fuel dock with pumps and the dockmaster's office.
//   The boatyard: a gravel yard of boats on jack stands and keel blocks (sanded bottoms, shrink wrap, a sportfisher
//   with her outboards off), a travel lift straddling its slip with a boat in the slings, three men at work, and the
//   clutter of a working yard.
// The fixed parts are a few meshes with their own draw distances; the forklift, the boat it carries and the workers
// only move (and the figures only rebuild) while the camera is within 600 m.
UCLASS()
class KEYSRUN_API AKeysYards : public AActor
{
	GENERATED_BODY()

public:
	AKeysYards();
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	static AKeysYards* Get(const UObject* WorldContext);
	// test hooks ("yd:<cmd>" in a script): cycle (start a launch now), fetch, speed=<k>, log (the forklift's state
	// twice a second), stats, where, eye=x,y,yawdeg[,pitchdeg[,height]] (a camera standing in the yard; eye=off)
	FString DebugCmd(const FString& Cmd);

private:
	UProceduralMeshComponent* NewMesh(const TCHAR* Name, USceneComponent* Parent, double DrawDistM, bool bShadow = true);
	UTextRenderComponent* NewText(USceneComponent* Parent, const FString& Text, const FVector& PosM, double YawDeg, double SizeM, const FColor& Color);
	void BuildGround(FKeysMesh& S, FKeysMesh& D);
	void BuildDryStack(FKeysMesh& S, FKeysMesh& Bt, FKeysMesh& D, FKeysMesh& G);
	void BuildBoatyard(FKeysMesh& S, FKeysMesh& Bt, FKeysMesh& D, FKeysMesh& G);
	void BuildForklift();
	void TickForklift(double T, double Dt, double CamDist);
	void TickWorkers(double T, double CamDist);
	void Survey();

	UPROPERTY() TObjectPtr<USceneComponent> Root;
	UPROPERTY() TObjectPtr<USceneComponent> Site;   // the yard's own frame: X east, Y south, Z up, at sea level
	UPROPERTY() TArray<TObjectPtr<UProceduralMeshComponent>> Meshes;
	UPROPERTY() TArray<TObjectPtr<UTextRenderComponent>> Texts;
	UPROPERTY() TObjectPtr<UProceduralMeshComponent> NightMesh;   // lamps and lit windows, shown from dusk
	UPROPERTY() TArray<TObjectPtr<UPointLightComponent>> Lamps;
	TArray<FVector> LampAt;
	bool bBuilt = false, bNight = false, bTextsShown = true, bAwake = false, bLog = false;
	double OX = 0, OZ = 0, SlowT = 0, LogT = 0, WorkerT = 0;
	int32 Tris = 0;

	// ---- the forklift and the boats it can move
	FKeysForklift Fork;
	UPROPERTY() TObjectPtr<UProceduralMeshComponent> LiftBody;
	UPROPERTY() TObjectPtr<UProceduralMeshComponent> LiftMast;
	UPROPERTY() TObjectPtr<UProceduralMeshComponent> LiftForks;
	UPROPERTY() TObjectPtr<UProceduralMeshComponent> LiftDriver;
	UPROPERTY() TArray<TObjectPtr<UProceduralMeshComponent>> LiveBoats;
	FKeysHuman Driver;

	// ---- the boatyard's workers: 0 sanding, 1 painting, 2 pressure washing
	struct FWorker { FKeysHuman Hm; int32 Job = 0; FVector Spot = FVector::ZeroVector; double Ph = 0; uint32 Paint = 0; };
	TArray<FWorker> Workers;
	UPROPERTY() TObjectPtr<UProceduralMeshComponent> WorkerMesh;
	UPROPERTY() TObjectPtr<UProceduralMeshComponent> SprayMesh;
	UPROPERTY() TObjectPtr<UMaterialInstanceDynamic> SprayMID;
	FVector WasherAt = FVector::ZeroVector;
	// yd:eye=x,y,yaw[,pitch[,height]]: look from a spot in the yard's frame, standing height by default (yd:eye=off)
	UPROPERTY() TObjectPtr<class ACameraActor> EyeCam;
};
