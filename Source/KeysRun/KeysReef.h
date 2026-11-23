#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "KeysReef.generated.h"

class UInstancedStaticMeshComponent;
class UStaticMesh;
class UMaterialInstanceDynamic;
namespace KeysReefDen { struct FDens; }

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
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;

	struct FInst
	{
		int32 Species = 0;
		double X = 0, Y = 0, Z = 0, RotY = 0, TiltX = 0, TiltZ = 0, SX = 1, SY = 1, SZ = 1, ColorT = 0;
		// 1: not a coral but a den's rock (KeysReefDen.h), standing in the list as a coral head of its size so that
		// whatever reads the list for what is in a fish's way finds it; the reef does not draw it (the dens do)
		uint8 Den = 0;
	};
	// instances placed in one 50 m chunk (cached), for tests and anything that wants to know what grows where
	const TArray<FInst>& ChunkInstances(int32 CX, int32 CZ);
	int32 DrawnInstances() const { return Drawn; }

	// ---- the dens (KeysReefDen*.cpp): hiding places in the reef and what lives in them
	// a coral that stands up solid (heads, branches, sponges), which a den's rock must not bury
	static bool IsSolidCoral(int32 Species);
	// one that stands up but is no more than a card (sea fans, plumes and rods): it may grow through a rock, but not
	// across a den's opening
	static bool IsTallCoral(int32 Species);
	// the species a den's rock stands in the instance list as (a boulder star coral)
	static int32 DenStandIn();
	KeysReefDen::FDens* DenState() const { return Dens; }
	UPROPERTY() TArray<TObjectPtr<UInstancedStaticMeshComponent>> DenPools;
	UPROPERTY() TArray<TObjectPtr<UStaticMesh>> DenMeshes;
	UPROPERTY() TArray<TObjectPtr<UMaterialInstanceDynamic>> DenMats;

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
	TArray<double> GeoH;   // each pool's model height (m), to keep tips under the surface
	bool bPending = false;
	double PendX = 0, PendZ = 0;
	KeysReefDen::FDens* Dens = nullptr;
};
