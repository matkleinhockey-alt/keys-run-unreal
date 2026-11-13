#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "KeysWeather.generated.h"

class UProceduralMeshComponent;
class UMaterialInstanceDynamic;
class UPointLightComponent;

// Weather that the rest of the game reads (game meters, x east, z south).
namespace KeysWx
{
	// how hard it is raining on the water at (X, Z), 0..1. Thread-safe to read (the water mesh calls it in parallel).
	double RainAt(double X, double Z);

	// a storm cell as the radar sees it: where its rain is, how wide, how heavy (0..1), and its waterspout if it has one
	struct FCellView { double X, Z, R, Rain, Build, Seed; bool bSpout; double SX, SZ; };
	void Cells(TArray<FCellView>& Out);
	// one line for the HUD: the wind, and the nearest storm cell if there is one
	FString HudLine(double X, double Z);
}

// Real-time storms (the summer afternoon thunderstorms of the Keys). Cells spawn at random times and places, seeded from
// the real clock so each session differs; they build, drift across the map with the steering wind, rain out and decay
// over real minutes. Each one is a towering cumulonimbus in the volumetric clouds (M_KR_Cloud), a rain shaft under it,
// rain and darker light under and near it, a gusty outflow that drives the sea (KeysGeo::SetStormWind), lightning with
// thunder delayed by distance, and now and then a waterspout. The radio warns you; the chart shows the rain.
UCLASS()
class KEYSRUN_API AKeysWeather : public AActor
{
	GENERATED_BODY()

public:
	AKeysWeather();
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

	static AKeysWeather* Get(const UObject* WorldContext);

	// test hooks: storm=spawn[:dist,bearing[,age]] storm=clear weather=calm|breezy|storm spout[=dist,bearing]
	// bolt[=dist,bearing] wxskip=seconds. Returns what it did.
	FString DebugCmd(const FString& Cmd);

	double GloomNow() const { return Gloom; }
	double RainNow() const { return RainCam; }

	struct FCell
	{
		int32 Id = 0;
		double X = 0, Z = 0, VX = 0, VZ = 0;
		double Born = 0, Life = 900;   // weather clock seconds
		double R = 1500;               // mature rain radius, m
		double Power = 1;              // peak strength (taller tower, heavier rain, more lightning)
		double Seed = 0;
		bool bBig = false;             // big enough to drop a waterspout
		// now
		double Build = 0, Rain = 0, Elec = 0, Cloud = 0;
		double NextBolt = 0;
		bool bWarned = false, bOverhead = false, bSpoutTried = false;
		// its waterspout
		bool bSpout = false;
		double SX = 0, SZ = 0, SBorn = 0, SLife = 0, SVX = 0, SVZ = 0;
		bool bSpoutWarned = false;
	};

private:
	double Clock() const;
	void Schedule(bool bFirst);
	FCell& SpawnCell(double X, double Z, double AgeFrac, bool bBig);
	void UpdateCells(double Dt);
	void UpdateWind(double Dt);
	void UpdateLook(double Dt);
	void UpdateLightning(double Dt);
	void UpdateSpouts(double Dt);
	void Radio(const FString& Msg);
	void Bolt(double X, double Z, bool bToWater);
	void BuildBolt(const FVector& TopG, const FVector& BotG);
	void Thunder(double X, double Z, double Power, bool bCloud);
	void BuildUnitCone(UProceduralMeshComponent* M, int32 Seg, double R0, double R1, double A0, double A1);
	void BuildSpout(const FCell& C, double T);
	void BuildRainCyl();
	bool PlayerPos(double& OX, double& OZ) const;
	bool CameraPos(FVector& OutGame) const;

	UPROPERTY() TObjectPtr<USceneComponent> Root;
	UPROPERTY() TArray<TObjectPtr<UProceduralMeshComponent>> Shafts;
	UPROPERTY() TArray<TObjectPtr<UMaterialInstanceDynamic>> ShaftMIDs;
	UPROPERTY() TObjectPtr<UProceduralMeshComponent> RainCyl;
	UPROPERTY() TObjectPtr<UMaterialInstanceDynamic> RainMID;
	UPROPERTY() TObjectPtr<UProceduralMeshComponent> Spout;
	UPROPERTY() TObjectPtr<UMaterialInstanceDynamic> SpoutMID;
	UPROPERTY() TObjectPtr<UProceduralMeshComponent> Skirt;
	UPROPERTY() TObjectPtr<UMaterialInstanceDynamic> SkirtMID;
	UPROPERTY() TObjectPtr<UProceduralMeshComponent> BoltMesh;
	UPROPERTY() TObjectPtr<UMaterialInstanceDynamic> BoltMID;
	UPROPERTY() TObjectPtr<UPointLightComponent> BoltLight;
	UPROPERTY() TObjectPtr<UMaterialInstanceDynamic> CloudMID;

	TArray<FCell> Cells;
	FRandomStream Rng;
	int32 NextId = 1;
	double Skip = 0;           // test time skip, seconds
	double NextSpawn = 0;
	double SteerDir = 0, SteerSpd = 5;   // the steering wind the cells drift with (toward, radians; m/s)
	double DriftX = 0, DriftZ = 0;       // the fair-weather cumulus drift, m
	double Gloom = 0, RainCam = 0, GustT = 0, Gust = 1, GustTarget = 1;
	bool bCloudsOn = false;
	double CloudWaitT = 0, CloudLogT = -99;
	// lightning: a flash's strokes (time, strength), the light and where it is
	TArray<FVector2D> Strokes;
	double FlashI = 0, BoltT = -99, BoltShowUntil = -99;
	FVector FlashPos = FVector::ZeroVector;   // game meters
	struct FThunder { double At, Power, Dist; bool bCrack; };
	TArray<FThunder> Thunders;
	double SpoutPushMsgT = -99;
	double SprayT = 0;
};
