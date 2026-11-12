#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "KeysSeafloor.generated.h"

class UProceduralMeshComponent;
class UMaterialInterface;

// The sea floor under the see-through water: 64 m chunks of real bathymetry plus relief (KeysSea), built around the
// camera at a level of detail set by distance, a coarse far skirt out to the horizon and a dark backstop far below.
// Ported from the web game's world/seafloor.ts + world/terrain/{chunk,lod}.ts.
UCLASS()
class KEYSRUN_API AKeysSeafloor : public AActor
{
	GENERATED_BODY()

public:
	AKeysSeafloor();
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

	static UMaterialInterface* FloorMaterial();
	// true if the chunk under (X, Z) is built
	bool IsReady(double X, double Z) const;

private:
	struct FRing { double Outer; int32 Segs; };
	struct FChunk { int32 Section = -1; int32 Lod = 0; };
	struct FJob { int32 CX, CZ, Lod; };

	void Scan(double CamX, double CamZ);
	void ProcessQueue();
	void BuildChunk(int32 CX, int32 CZ, int32 Segs, int32 Section);
	void BuildSkirt(double CamX, double CamZ);
	const TArray<FRing>& Rings() const;

	UPROPERTY() TObjectPtr<USceneComponent> Root;
	UPROPERTY() TObjectPtr<UProceduralMeshComponent> Chunks;
	UPROPERTY() TObjectPtr<UProceduralMeshComponent> Skirt;

	TMap<int64, FChunk> Resident;
	TArray<FJob> Queue;
	TSet<int64> Queued;
	TArray<int32> FreeSections;
	int32 NextSection = 0;
	bool bSubmerged = false, bProfileSub = false;
	double LastScanX = 1e9, LastScanZ = 1e9, LastSkirtX = 1e9, LastSkirtZ = 1e9;
};
