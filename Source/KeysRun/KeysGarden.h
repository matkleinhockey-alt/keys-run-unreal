#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "KeysGarden.generated.h"

class UProceduralMeshComponent;
class UMaterialInstanceDynamic;

// The Abyssal Garden: one dive site built in the style of Noniv's "Abyssal Garden" (MIT, github.com/Noniv/abyssal-garden)
// - shafts of sunlight slanting down from the surface with gold specks drifting through them, a sunken wooden ship
// listing on the sand, a giant stone whale fluke rising off the bottom, a rock arch, barrel sponges and sea fans, a
// patch of glowing magenta ground cover, and manta rays gliding laps overhead.
// Where that scene has a kelp forest this one has what a deep Florida Keys reef grows (kelp is a cold-water plant and
// does not live here): rock ledges crowded with sea plumes, sea rods, sea whips, wire coral, black coral bushes,
// deep-water sea fans and rope sponges, all leaning and rocking in the surge as the kelp did.
// Everything is generated from code at load time (the thickets and the mantas are rebuilt as they move while you're near).
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
	UPROPERTY() TObjectPtr<UProceduralMeshComponent> Static;   // ship, fluke, arch, ledges, sponges, fans, ground cover
	UPROPERTY() TObjectPtr<UProceduralMeshComponent> Sway;     // the gorgonian thickets, rebuilt as they sway
	UPROPERTY() TObjectPtr<UProceduralMeshComponent> Mantas;   // rebuilt as they fly
	UPROPERTY() TObjectPtr<UProceduralMeshComponent> Glow;     // sun shafts, gold specks, the magenta glow
	UPROPERTY() TObjectPtr<UMaterialInstanceDynamic> GlowMID;

	void FindSite();
	void BuildAll();
	void BuildStatic();
	void BuildGlow();
	void PlantThickets();
	void BuildSway(double T);
	void BuildMantas(double T);
	double Ground(double LX, double LY) const;   // seabed height (UE z, meters) at a site-local point

	// a rock ledge a thicket stands on: its middle (site-local), half-sizes, height, heading and the height of its top; bUpper: a block standing on another ledge
	struct FLedge { FVector2D At; double A, B, H, Yaw, Top; bool bUpper; };
	// what sways, kept in its pose at rest: round branches (FSeg) and flat branchlets (FLeaf), colony by colony
	struct FSeg { FVector A, B; float RA, RB; uint8 Sides; FLinearColor C; };
	struct FLeaf { FVector P[4]; FLinearColor C; };
	struct FColony { FVector Base; double H, Phase, Flex; int32 Seg0, SegN, Leaf0, LeafN; };
	TArray<FLedge> Ledges;
	TArray<FSeg> Segs;
	TArray<FLeaf> Leaves;
	TArray<FColony> Colonies;
	double CX = 0, CZ = 0;
	double SwayAcc = 0;
	bool bNear = false, bBuilt = false, bSwayLogged = false;
};
