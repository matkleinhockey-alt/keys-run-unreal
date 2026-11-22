#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "KeysLandmarks.generated.h"

class UProceduralMeshComponent;
class UInstancedStaticMeshComponent;
class UTextRenderComponent;
class UPointLightComponent;

// The world's landmarks and waterfront, ported from the web game:
//   world/bridge.ts         US-1 and the Seven Mile Bridge (barriers, striping, bents with caps, the hump over the channel)
//                           and the old bridge (lower and narrower, arch ribs, the missing swing span, broken stubs)
//   world/marinas.ts        the four marinas' docks (main pier, T-head, finger piers, pilings), harbourmaster sheds, dry-stack
//                           racks and the boats tied up in the slips
//   world/lighthouse.ts     Sombrero Key Light: the skeletal iron pyramid on the reef crest with the keeper's house and lantern
//   world/landmarks.ts      Faro Blanco's lighthouse tower and the reef mooring-ball line
//   world/channel-buoys.ts  + shared/world/channel-marks.ts: red nuns and green cans down both sides of every dredged channel
//                           (red right returning), numbered, riding the swell, lit at night
//   world/piers.ts          public fishing piers out from the keys (walkable)
//   world/waterfront.ts     waterfront estates: a house, a pool and a private dock with a boat on a lift
UCLASS()
class KEYSRUN_API AKeysLandmarks : public AActor
{
	GENERATED_BODY()

public:
	AKeysLandmarks();
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

private:
	TMap<int64, struct FTownBatch*> LmChunks;                // while building: the spread-out landmarks by 1200 m square
	struct FTownBatch& LmAt(double X, double Z);
	void BuildMarinas(struct FTownBatch& B);
	void BuildLighthouse(struct FTownBatch& B, struct FTownBatch& G);
	void BuildPiers(struct FTownBatch& B);
	void BuildEstates(struct FTownBatch& B);
	void BuildBuoys();
	void BuildDaymarks(FTownBatch& B, FTownBatch& G);   // the real map's red and green daybeacons
	void BuildBridges(struct FTownBatch& Road);   // KeysBridges.cpp
	void BuildSideBridges();                      // the road bridges off US-1's line (Jewfish Creek, Card Sound)
	// the bridges in chunks, each a detailed and a plain version, swapped by distance (UpdateBridgeLod)
	struct FBridgeChunk { double X0 = 0, X1 = 0, ZOff = 0; UProceduralMeshComponent* Near = nullptr; UProceduralMeshComponent* Far = nullptr; bool bNear = false; };
	TArray<FBridgeChunk> BridgeChunks;
	UPROPERTY() TArray<TObjectPtr<UProceduralMeshComponent>> BridgeComps;
	void UpdateBridgeLod();

	UPROPERTY() TObjectPtr<USceneComponent> Root;
	UPROPERTY() TObjectPtr<UProceduralMeshComponent> Static;
	UPROPERTY() TObjectPtr<UProceduralMeshComponent> Lamps;
	UPROPERTY() TObjectPtr<UProceduralMeshComponent> Beam;
	UPROPERTY() TObjectPtr<UProceduralMeshComponent> WreckMesh;
	UPROPERTY() TArray<TObjectPtr<UInstancedStaticMeshComponent>> BuoyISMs;
	UPROPERTY() TArray<TObjectPtr<UTextRenderComponent>> Labels;

	struct FMark { double X, Z; bool bRed; int32 N; };
	TArray<FMark> Marks;
	TArray<FVector2D> Balls;
	// the buoy models (Content/KeysRun/Props, made from generated models by Tools/propglb.py): when they load, the marks,
	// mooring balls and lobster-trap floats use them instead of the primitive shapes
	struct FPot { double X, Z, Yaw, Tilt, Ph; };
	TArray<FPot> Pots;
	bool bBuoyModels = false;
	UStaticMesh* LoadProp(const TCHAR* Key);
public:
	// a prop model from Content/KeysRun/Props/<Key>.krfish as a static mesh owned by Outer (keep a UPROPERTY to it), or null
	static UStaticMesh* LoadPropMesh(UObject* Outer, const TCHAR* Key);
private:
	UPROPERTY() TArray<TObjectPtr<UStaticMesh>> PropMeshes;
	double BobT = 0;
	UPROPERTY() TObjectPtr<class UMaterialInstanceDynamic> BeamMID;
	FVector LightPos = FVector::ZeroVector;   // Sombrero Key Light's lantern, game meters
	void UpdateBeam();
};
