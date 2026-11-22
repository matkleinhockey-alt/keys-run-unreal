#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "KeysSeafloor.generated.h"

class UProceduralMeshComponent;
class UMaterialInterface;

// The sea floor under the see-through water: 64 m chunks of real bathymetry plus relief (KeysSea), built around the
// camera at a level of detail set by distance, a coarse far skirt out to the horizon (open where chunks are built)
// and a dark backstop far below.
// Ported from the web game's world/seafloor.ts + world/terrain/{chunk,lod}.ts.
UCLASS()
class KEYSRUN_API AKeysSeafloor : public AActor
{
	GENERATED_BODY()

public:
	AKeysSeafloor();
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;

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
	void TakeSkirt(void* OutPtr);   // adopt a finished FSkirtOut (KeysSeafloor.cpp) and cut its triangles
	void BuildSkirtTris();
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
	double LastScanX = 1e9, LastScanZ = 1e9;

	// the far skirt: its grid lines, vertices (kept so the hole under the chunks can be recut without resampling) and
	// the chunk cell it is centred on
	TArray<double> SkirtX, SkirtZ;
	TArray<FVector> SkirtV, SkirtN;
	TArray<FVector2D> SkirtUV;
	TArray<FLinearColor> SkirtCol;
	int32 SkirtCX = 0, SkirtCZ = 0;
	bool bSkirtDirty = false;
};
