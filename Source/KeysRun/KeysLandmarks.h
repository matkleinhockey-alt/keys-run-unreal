#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "KeysLandmarks.generated.h"

class UProceduralMeshComponent;
class UInstancedStaticMeshComponent;
class UTextRenderComponent;
class UPointLightComponent;

// The world's landmarks and waterfront, ported from the web game:
//   world/bridge.ts         US-1 and the Seven Mile Bridge (barriers, striping, bents with caps, the hump over the channel)
//                           and the old bridge (lower and narrower, arch ribs, the missing swing span, broken stubs)
//   world/marinas.ts        the four marinas' docks (main pier, T-head, finger piers, pilings), harbourmaster sheds, dry-stack
//                           racks and the boats tied up in the slips
//   world/lighthouse.ts     Sombrero Key Light: the skeletal iron pyramid on the reef crest with the keeper's house and lantern
//   world/landmarks.ts      Faro Blanco's lighthouse tower and the reef mooring-ball line
//   world/channel-buoys.ts  + shared/world/channel-marks.ts: red nuns and green cans down both sides of every dredged channel
//                           (red right returning), numbered, riding the swell, lit at night
//   world/piers.ts          public fishing piers out from the keys (walkable)
//   world/waterfront.ts     waterfront estates: a house, a pool and a private dock with a boat on a lift
UCLASS()
class KEYSRUN_API AKeysLandmarks : public AActor
{
	GENERATED_BODY()

public:
	AKeysLandmarks();
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

private:
	void BuildMarinas(struct FTownBatch& B);
	void BuildLighthouse(struct FTownBatch& B, struct FTownBatch& G);
	void BuildPiers(struct FTownBatch& B);
	void BuildEstates(struct FTownBatch& B);
	void BuildBuoys();
	void BuildBridges(struct FTownBatch& B);

	UPROPERTY() TObjectPtr<USceneComponent> Root;
	UPROPERTY() TObjectPtr<UProceduralMeshComponent> Static;
	UPROPERTY() TObjectPtr<UProceduralMeshComponent> Lamps;
	UPROPERTY() TObjectPtr<UProceduralMeshComponent> Beam;
	UPROPERTY() TArray<TObjectPtr<UInstancedStaticMeshComponent>> BuoyISMs;
	UPROPERTY() TArray<TObjectPtr<UTextRenderComponent>> Labels;

	struct FMark { double X, Z; bool bRed; int32 N; };
	TArray<FMark> Marks;
	TArray<FVector2D> Balls;
	double BobT = 0;
};
