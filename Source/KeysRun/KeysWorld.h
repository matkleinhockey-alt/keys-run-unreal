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
	void SnapSunset();
	// hide the scattered houses and palms standing on these plat parcels (built-on Keys Run Estates lots)
	void SetClearedParcels(const TSet<int32>& Parcels);
	void SceneryParcels(TArray<int32>& Out) const { ParcelScenery.GetKeys(Out); }   // straight to sunset, no glide (the motel's night, behind a black screen)
	bool IsSunset() const { return SunsetTarget > .5; }
	// 0 by day, 1 at full sunset: what lights the lighthouse beam, the lamps' halos and the neon (web lightMats)
	double DuskK() const;
	static double Dusk(const UObject* WorldContext);
	// the soft translucent glow material (M_KR_Bubble's "snow" mode: UV x in [2, 3] draws a soft-edged blob or band,
	// vertex alpha is the opacity); each user makes its own instance and sets "Bright"
	static UMaterialInterface* GlowMaterial();

	static AKeysWorld* Get(const UObject* WorldContext);
	// the camera is under the surface: draw the sea's underside (Snell's window) and keep it in step with the waves
	void SetUnderside(bool bOn);
	// test hook: the old opaque chart-colored sea (for frame-time comparisons)
	void DebugOldWater(bool bOld);

	static UMaterialInterface* VertexColorMaterial();
	static UMaterialInterface* WaterMaterial();
	static UMaterialInterface* SolidMaterial();

	// where the fine water grid is centered (game meters)
	double FocusX = 0, FocusZ = 0;

private:
	void BuildTerrain();
	void BuildScenery();
	void UpdateWater();
	void ApplySun(double K);
	UInstancedStaticMeshComponent* MakeISM(const TCHAR* Name, UStaticMesh* Mesh, const FLinearColor& Color, double Roughness = .8);

	UPROPERTY() TObjectPtr<USceneComponent> Root;
	UPROPERTY() TObjectPtr<UProceduralMeshComponent> Terrain;
	UPROPERTY() TObjectPtr<UProceduralMeshComponent> Water;
	UPROPERTY() TObjectPtr<class UMaterialInstanceDynamic> WaterMID;
	UPROPERTY() TObjectPtr<class UMaterialInstanceDynamic> UnderMID;
	bool bUnderside = false;
	TArray<FVector> WNU;
	FVector SunDirUE = FVector::UpVector;
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
	UPROPERTY() TMap<int32, TObjectPtr<class UProceduralMeshComponent>> ParcelScenery;
	TSet<int32> ClearedParcels;
};
