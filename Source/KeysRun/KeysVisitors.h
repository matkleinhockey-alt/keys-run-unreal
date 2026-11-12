#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "KeysVisitors.generated.h"

class UProceduralMeshComponent;
class UTextRenderComponent;

// The ones who come to you: ported from the web game's
//   entities/life/pirates.ts, pirate-ship.ts, pirate-nav.ts and shared/world/pirates.ts - Cap'n Barnacle Bev and
//     Mr. Mildew in a tired wooden gaff sloop, who sail up about every half hour on the wall clock, hail you, appraise
//     your cooler and your wallet, chat, and sail off. They never take anything.
//   entities/life/banner-plane.ts - a tail-dragger droning along at 100-150 m every few minutes towing a forty-metre
//     banner for one of the town's businesses (content/venues.ts banner lines).
UCLASS()
class KEYSRUN_API AKeysVisitors : public AActor
{
	GENERATED_BODY()

public:
	AKeysVisitors();
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	static AKeysVisitors* Get(const UObject* WorldContext);

	// test hooks: bring the pirates in now from Dist meters, or start a banner pass
	void SummonPirates(double Dist = 300);
	void StartBanner();

private:
	void TickPirates(double Dt, double T);
	void TickBanner(double Dt, double T);
	bool BeginPirates(int64 Bucket, double Dist);

	UPROPERTY() TObjectPtr<USceneComponent> Root;
	UPROPERTY() TObjectPtr<UProceduralMeshComponent> Ship;
	UPROPERTY() TObjectPtr<UProceduralMeshComponent> Plane;
	UPROPERTY() TObjectPtr<UProceduralMeshComponent> Banner;
	UPROPERTY() TObjectPtr<UTextRenderComponent> BannerText;
	UPROPERTY() TObjectPtr<UTextRenderComponent> BannerTextBack;

	// pirates: 0 away, 1 approach, 2 linger, 3 depart
	int32 Phase = 0;
	int64 Handled = -1;
	double PX = 0, PZ = 0, PH = 0, PSpeed = 0, PhaseT = 0, EscapeT = 0, EscapeH = 0;
	int32 NextBeat = 0;
	struct FBeat { double At; bool bCaptain; FString Text; };
	TArray<FBeat> Beats;

	// banner plane
	bool bPass = false;
	double Gap = 60, BX = 0, BZ = 0, BH = 0, BSp = 28, BAlt = 120, Flown = 0, BLen = 3300;
};
