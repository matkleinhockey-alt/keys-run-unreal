#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "KeysCanals.generated.h"

class UProceduralMeshComponent;

// The seawalls of the residential canals on the real map (KeysGeoReal::Canals, cut into the ground by KeysGeo::LandH):
// a concrete wall on the canal's edge and the lawn behind it, built into 600 m tiles with a draw distance. Nothing
// here moves. Nothing is built on the classic map or with -KRNoCanals.
UCLASS()
class KEYSRUN_API AKeysCanals : public AActor
{
	GENERATED_BODY()

public:
	AKeysCanals();
	virtual void BeginPlay() override;

private:
	UPROPERTY()
	TObjectPtr<USceneComponent> Root;
	UPROPERTY()
	TArray<TObjectPtr<UProceduralMeshComponent>> Meshes;
};
