#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "KeysReef.generated.h"

class UInstancedStaticMeshComponent;
class UStaticMesh;
class UMaterialInstanceDynamic;

// The reef: elkhorn and staghorn, brain and star coral, sea fans and plumes, barrel and tube sponges, encrusting
// mounds and seagrass, placed deterministically on 50 m chunks around the camera and drawn as instanced meshes at
// three levels of detail. Ported from the web game's world/reef/** (placement, species, geometry, flow sway).
UCLASS()
class KEYSRUN_API AKeysReef : public AActor
{
	GENERATED_BODY()

public:
	AKeysReef();
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

	struct FInst
	{
		int32 Species = 0;
		double X = 0, Y = 0, Z = 0, RotY = 0, TiltX = 0, TiltZ = 0, SX = 1, SY = 1, SZ = 1, ColorT = 0;
	};
	// instances placed in one 50 m chunk (cached), for tests and anything that wants to know what grows where
	const TArray<FInst>& ChunkInstances(int32 CX, int32 CZ);
	int32 DrawnInstances() const { return Drawn; }

private:
	void Rebuild(double CamX, double CamZ);
	UStaticMesh* MakeMesh(const struct FKeysMesh& M, UMaterialInterface* Mat, const TCHAR* Name);

	UPROPERTY() TArray<TObjectPtr<UInstancedStaticMeshComponent>> Pools;   // species * 3 + lod
	UPROPERTY() TArray<TObjectPtr<UStaticMesh>> Meshes;
	UPROPERTY() TObjectPtr<UMaterialInstanceDynamic> CardMID;
	UPROPERTY() TObjectPtr<USceneComponent> Root;

	TMap<int64, TArray<FInst>> Cache;
	TArray<int64> CacheOrder;
	int32 LastCX = INT32_MIN, LastCZ = INT32_MIN;
	int32 Drawn = 0;
	bool bPending = false;
	double PendX = 0, PendZ = 0;
};
