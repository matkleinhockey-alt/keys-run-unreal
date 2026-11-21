#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "KeysMangroveLife.generated.h"

struct FTownBatch;

// The mangrove creeks of the real map and what lives in and over them (KeysMangroveLife.cpp).
namespace KeysMangroveLife
{
	// a creek a skiff can idle along: its centre line (game metres, smoothed, mouth to head), its width, and the stretch
	// of it, in metres along the line, that the canopy closes over (Roof1 <= Roof0: open to the sky)
	struct FCreek
	{
		FString Name;
		double W = 14, Roof0 = 0, Roof1 = 0, Len = 0;
		TArray<FVector2D> Pts;
		double X0 = 0, X1 = 0, Z0 = 0, Z1 = 0;   // bounds, with room for the banks
	};
	// The hub's hook: KeysGeo::Init() copies these into its creek list on the real map, so the ground is cut and the
	// water is deep along them (request R1 in .marshal/status/mangroves.md).
	const TArray<FCreek>& Creeks();

	// where a point lies against the nearest creek
	struct FNear
	{
		double D = 1e9;        // metres outside the creek's edge (negative: in the channel)
		int32 Creek = -1;
		double S = 0;          // metres along it
		FVector2D C, T;        // the nearest point of the centre line, and the line's direction there
		bool bRoof = false;    // in the stretch the canopy closes over
	};
	FNear Near(double X, double Z);

	// The test harness's helmsman (the creek=N script command, request R2): the heading (as the boat's H) that keeps a
	// boat at (X, Z) on creek N's line, bound from its mouth to its head. False once she is at the head, or N is no creek.
	bool Pilot(int32 Creek, double X, double Z, double& InOutH);

	// where a manatee or two lie up: calm, deep bends and the pond at the head of Whiskey Creek
	const TArray<FVector2D>& Haunts();
	// dead snags at the creek mouths where the pelicans roost (x, z)
	const TArray<FVector2D>& Roosts();

	// a three-sided limb from P0 to P1 (game metres, y up), R thick, tapering a little
	void Limb(FTownBatch& B, const FVector& P0, const FVector& P1, double R, const FLinearColor& C);
	// a brown pelican perched with its feet at Feet, facing Yaw: bill tucked down on the breast (Pose 0), or head up
	// and looking about (Pose 1). Sz 1 is life size; like the waders they are drawn large (1.7 and up) to read from a boat
	void Pelican(FTownBatch& B, const FVector& Feet, double Yaw, double Sz, int32 Pose);
	// a dead, bare mangrove standing at (X, Z) with its foot at Y0, and N pelicans along its limbs; returns how many
	int32 Snag(FTownBatch& B, double X, double Y0, double Z, double Seed, int32 N);
}

// Keeps a manatee or two in the calm creeks while the boat is about: they are KeysLife's own manatees, asked for at
// the haunts when you come within sight and left to KeysLife to move, draw and retire.
UCLASS()
class KEYSRUN_API AKeysMangroveLife : public AActor
{
	GENERATED_BODY()

public:
	AKeysMangroveLife();
	virtual void Tick(float DeltaSeconds) override;

private:
	TArray<bool> Armed;
	double CheckT = 0;
};
