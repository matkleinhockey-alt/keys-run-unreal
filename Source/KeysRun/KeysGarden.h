#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "KeysGarden.generated.h"

class UProceduralMeshComponent;
class UMaterialInstanceDynamic;

// The Abyssal Garden: one dive site built in the style of Noniv's "Abyssal Garden" (MIT, github.com/Noniv/abyssal-garden)
// - a kelp forest swaying in the current with gold specks drifting through it, shafts of sunlight slanting down from
// the surface, a sunken wooden ship listing on the sand, a giant stone whale fluke rising off the bottom, a rock arch,
// barrel sponges and sea fans, a patch of glowing magenta ground cover, and manta rays gliding laps overhead.
// Everything is generated from code at load time (the kelp and the mantas are rebuilt each frame while you're near).
UCLASS()
class KEYSRUN_API AKeysGarden : public AActor
{
	GENERATED_BODY()

public:
	AKeysGarden();
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

	static AKeysGarden* Get(const UObject* WorldContext);
	// the middle of the site, game meters (x, z)
	FVector2D Center() const { return FVector2D(CX, CZ); }

private:
	UPROPERTY() TObjectPtr<USceneComponent> Root;
	UPROPERTY() TObjectPtr<UProceduralMeshComponent> Static;   // ship, fluke, arch, sponges, fans, ground cover
	UPROPERTY() TObjectPtr<UProceduralMeshComponent> Kelp;     // rebuilt as it sways
	UPROPERTY() TObjectPtr<UProceduralMeshComponent> Mantas;   // rebuilt as they fly
	UPROPERTY() TObjectPtr<UProceduralMeshComponent> Glow;     // sun shafts, gold specks, the magenta glow
	UPROPERTY() TObjectPtr<UMaterialInstanceDynamic> GlowMID;

	void FindSite();
	void BuildAll();
	void BuildStatic();
	void BuildGlow();
	void BuildKelp(double T);
	void BuildMantas(double T);
	double Ground(double LX, double LY) const;   // seabed height (UE z, meters) at a site-local point

	struct FStalk { FVector Base; double H, Phase, Lean, Hue; int32 Blades; };
	TArray<FStalk> Stalks;
	double CX = 0, CZ = 0;
	double KelpAcc = 0;
	bool bNear = false, bBuilt = false;
};
