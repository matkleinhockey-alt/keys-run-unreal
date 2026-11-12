#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "KeysHuman.h"
#include "KeysFinal.generated.h"

class UProceduralMeshComponent;
class UTextRenderComponent;
class USceneCaptureComponent2D;
class UTextureRenderTarget2D;
class UPointLightComponent;
class UMaterialInstanceDynamic;

// The last pieces of the web game, in one actor:
//  - the gold bluefin (entities/fish/legend-path.ts): a school of three gilded giants on a closed loop through deep
//    legal water, placed by the wall clock so everyone sees them in the same place; they cannot be caught
//  - whale spouts (entities/fish/spout.ts): a rising, thinning plume when a whale blows at the surface
//  - the dockside trophy (entities/land/dock-celebration.ts): the run's biggest fish strung up on a weigh station on
//    the planks when you step ashore, with a crowd round it, phones up and flashing, cheering, then admiring
//  - the underwater catch photo (game/catch/underwater-trophy.ts): a diver in a wetsuit, mask and snorkel holding the
//    speared fish up to the lens, drawn into a render target for the catch card
//  - the wreck chart's memory (entities/life/wreck-chart.ts): a wreck is "found" the first time the boat or the diver
//    passes within 260 m of its hull, and the chart remembers it
UCLASS()
class KEYSRUN_API AKeysFinal : public AActor
{
	GENERATED_BODY()

public:
	AKeysFinal();
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	static AKeysFinal* Get(const UObject* WorldContext);

	// a whale's blow at the surface (game meters)
	void Spout(double X, double Y, double Z);
	// the gold bluefin's pose now (wall clock); false if the chart has no legal water
	bool LegendPose(double& OutX, double& OutZ, double& OutH) const;
	// stepping ashore at a dock, with the run's biggest fish (empty key: nothing to show)
	void OnAshore(const FString& Key, double Weight, int32 Id, bool bReleased, const FString& Boat, double DockX, double DockZ, double BoatX, double BoatZ);
	void OnLeaveShore();
	bool StationAt(FVector& Out) const { Out = StationPos; return bStation; }
	// the underwater portrait for a speared catch
	void ShowPortrait(const FString& Key, double Weight);
	void HidePortrait();
	UTextureRenderTarget2D* Portrait() const { return bPortrait ? PortraitRT : nullptr; }

private:
	void BuildLegendLoop();
	void TickLegend(double Dt, double T);
	void TickSpouts(double Dt);
	void TickCrowd(double Dt, double T);
	void TickWrecks();
	void TickPortrait(double Dt, double T);
	bool FindStation(double DockX, double DockZ, double BoatX, double BoatZ, FVector& Out) const;

	UPROPERTY() TObjectPtr<USceneComponent> Root;
	UPROPERTY() TObjectPtr<UProceduralMeshComponent> LegendMesh;
	UPROPERTY() TObjectPtr<UProceduralMeshComponent> SpoutMesh;
	UPROPERTY() TObjectPtr<UProceduralMeshComponent> StationMesh;
	UPROPERTY() TObjectPtr<UProceduralMeshComponent> CrowdMesh;
	UPROPERTY() TObjectPtr<UProceduralMeshComponent> FlashMesh;
	UPROPERTY() TArray<TObjectPtr<UTextRenderComponent>> SignTexts;
	UPROPERTY() TObjectPtr<UProceduralMeshComponent> StudioMesh;
	UPROPERTY() TObjectPtr<USceneCaptureComponent2D> Capture;
	UPROPERTY() TObjectPtr<UTextureRenderTarget2D> PortraitRT;
	UPROPERTY() TObjectPtr<UPointLightComponent> StudioKey;
	UPROPERTY() TObjectPtr<UPointLightComponent> StudioRim;
	UPROPERTY() TObjectPtr<UMaterialInstanceDynamic> SpoutMID;
	UPROPERTY() TObjectPtr<UMaterialInstanceDynamic> FlashMID;

	// the gold bluefin
	TArray<FVector2D> Loop;
	double LoopLen = 0, LoopStep = 20, LoopOffset = 0, LoopPhase = 0;
	bool bLoopTried = false, bLegendSeen = false;
	struct FMember { double OX, OZ, Ph, ActT = 4, AP = -1; bool bSpl = false; };
	TArray<FMember> Members;

	// spouts
	struct FBlow { double X, Y, Z, Age; };
	TArray<FBlow> Blows;

	// the dock display
	bool bStation = false, bCrowdOn = false, bOnlyCore = false;
	FVector StationPos = FVector::ZeroVector, FishCenter = FVector::ZeroVector;
	double FaceYaw = 0, HypeT = 1e9, FishLenM = 1;
	int32 ShownId = -1;
	struct FFan { FKeysHuman H; FVector Base; int32 Role; double NextFlash, FlashT; bool bCore; };
	TArray<FFan> Fans;

	// the wreck chart
	double WreckCheckT = 0;

	// the portrait
	bool bPortrait = false;
	FString PortraitKey;
	double PortraitT = 0, PortraitLen = 1;
	FVector StudioO = FVector::ZeroVector;   // UE cm
	FKeysHuman Diver;
};
