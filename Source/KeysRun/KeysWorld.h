#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "KeysGeo.h"
#include "KeysWorld.generated.h"

class UProceduralMeshComponent;
class UInstancedStaticMeshComponent;
class UDirectionalLightComponent;
class USkyLightComponent;
class USkyAtmosphereComponent;
class UExponentialHeightFogComponent;
class UVolumetricCloudComponent;
class UMaterialInterface;
class UStaticMesh;

// Everything that isn't the player's boat: islands, the moving sea surface, bridges, palms, houses, marinas and the sky.
UCLASS()
class KEYSRUN_API AKeysWorld : public AActor
{
	GENERATED_BODY()

public:
	AKeysWorld();
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

	void ToggleSunset();
	bool IsSunset() const { return SunsetTarget > .5; }

	static UMaterialInterface* VertexColorMaterial();
	static UMaterialInterface* WaterMaterial();
	static UMaterialInterface* SolidMaterial();

	// where the fine water grid is centered (game meters)
	double FocusX = 0, FocusZ = 0;

private:
	void BuildTerrain();
	void BuildBridges();
	void BuildScenery();
	void BuildMarinas();
	void UpdateWater();
	void ApplySun(double K);
	UInstancedStaticMeshComponent* MakeISM(const TCHAR* Name, UStaticMesh* Mesh, const FLinearColor& Color, double Roughness = .8);

	UPROPERTY() TObjectPtr<USceneComponent> Root;
	UPROPERTY() TObjectPtr<UProceduralMeshComponent> Terrain;
	UPROPERTY() TObjectPtr<UProceduralMeshComponent> Water;
	UPROPERTY() TObjectPtr<UDirectionalLightComponent> Sun;
	UPROPERTY() TObjectPtr<USkyLightComponent> SkyLight;
	UPROPERTY() TObjectPtr<USkyAtmosphereComponent> Atmosphere;
	UPROPERTY() TObjectPtr<UExponentialHeightFogComponent> Fog;
	UPROPERTY() TObjectPtr<UVolumetricCloudComponent> Clouds;
	UPROPERTY() TArray<TObjectPtr<UInstancedStaticMeshComponent>> ISMs;

	// water grid
	int32 WN = 288;
	double WR = 5200;
	TArray<double> WGrid;     // warped offsets, meters
	TArray<FVector> WV, WN_;
	TArray<FVector2D> WUV;
	TArray<FLinearColor> WC;
	TArray<double> WH;
	bool bWaterBuilt = false;
	TArray<KeysGeo::FLiveWake> LiveWakes;
	TArray<KeysGeo::FLiveTrail> LiveTrails;

	double SunK = 0, SunsetTarget = 0;
};
