#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "KeysHuman.h"
#include "KeysTourneyShow.generated.h"

class UProceduralMeshComponent;
class UTextRenderComponent;

// The tournament as a show, ported from the web game's game/tournament/intro.ts (the opening titles over the countdown:
// letterbox bars, an aerial push over Boot Key Harbor with the five species, a shot of every boat in the field with its
// skipper's name, "Lines in" as the horn goes), fleet.ts (the AI field lined up abreast in the harbour and running out
// up the exit route to the course) and ceremony.ts (ten seconds on the planks at Boot Key Harbor City: the winner on
// the podium with the trophy over their head, the five fish that won it on a rack under the champion's banner, the crowd
// going wild, confetti, three camera shots; the display stays until the next one).
UCLASS()
class KEYSRUN_API AKeysTourneyShow : public AActor
{
	GENERATED_BODY()

public:
	AKeysTourneyShow();
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	static AKeysTourneyShow* Get(const UObject* WorldContext);

	bool CameraFor(FVector& OutPosUE, FRotator& OutRot, float& OutFov) const;
	// the card on screen: 0 none, 1 title, 2 a boat, 3 the gun, 4 the ceremony
	int32 CardKind = 0;
	FString CardTitle, CardSub;
	TArray<FString> CardChips;
	double CardAge = 0;
	bool bLetterbox = false;

private:
	void BuildRoute();
	void RouteAt(double S, double& X, double& Z, double& TX, double& TZ) const;
	void TickFleet(double Dt, double T, double Since);
	void BeginCeremony();
	void TickCeremony(double Dt, double T, double S);
	FVector Local(double X, double Y, double Z) const;   // ceremony frame -> game meters

	UPROPERTY() TObjectPtr<USceneComponent> Root;
	UPROPERTY() TArray<TObjectPtr<UProceduralMeshComponent>> FleetMeshes;
	UPROPERTY() TObjectPtr<UProceduralMeshComponent> Display;
	UPROPERTY() TObjectPtr<UProceduralMeshComponent> Party;
	UPROPERTY() TArray<TObjectPtr<UTextRenderComponent>> Texts;

	TArray<FVector2D> Route;
	TArray<double> Cum;
	double RouteLen = 1, StartS = 0;
	struct FBoat { double S = 0, Speed = 0, X = 0, Z = 0, H = 0; int32 Hull = 0; };
	TArray<FBoat> Boats;
	bool bFleetOn = false;

	// the camera this frame
	bool bCam = false;
	FVector CamPos = FVector::ZeroVector, CamLook = FVector::ZeroVector;   // game meters
	float CamFov = 70;

	// the ceremony
	bool bCeremony = false, bCeremonyBuilt = false;
	FVector Origin = FVector::ZeroVector;
	double Yaw = 0, CerT = 0;
	FKeysHuman Winner;
	TArray<FKeysHuman> Fans;
	TArray<FVector> FanBase;
	struct FBit { FVector P, V; double R, Spin; FLinearColor C; };
	TArray<FBit> Confetti;
	int32 LastPhase = 0;
};
