#pragma once

#include "CoreMinimal.h"

// The world under the surface, ported from the web game's pure modules:
//   shared/world/relief.ts       (rock outcrops, coral heads, spur-and-groove, wall terraces, sand waves)
//   client/world/terrain/heightfield.ts (seafloor height and its sand / seagrass / rubble / coral / deep palette)
//   shared/world/current.ts      (Florida Current, tide, the jets through the cuts, wave surge)
//   shared/rng hashCell / mulberry32 (deterministic placement for the reef)
// Everything is in game meters (x east, y up, z south), like KeysGeo.
namespace KeysSea
{
	// Shallowest the relief may bring the floor, meters below the surface.
	constexpr double ReliefMinTop = 2.3;

	// Height (m, + = up) the real floor stands above the smooth navigable depth D at (X, Z). 0 where it must not apply.
	double ReliefAt(double X, double Z, double D);
	// World height of the seafloor (negative underwater). The terrain mesh, the reef and the diver all use this.
	double SeafloorHeightAt(double X, double Z);
	// height and relief in one pass
	void SeafloorSample(double X, double Z, double& OutH, double& OutRel);
	// biome color (linear) at a point with base depth D; Shade darkens steep faces, Rel is the relief there
	FLinearColor SeafloorColorAt(double X, double Z, double D, double Shade = 1, double Rel = 0);

	// ---- water movement (m/s, horizontal, game x/z)
	FVector2D ChainTangent(double X);
	FVector2D ChainNormal(double X);   // points offshore (+z)
	FVector2D SteadyCurrentAt(double X, double Z, double T);
	FVector2D SurgeAt(double X, double Z, double T, double Depth);
	FVector2D CurrentAt(double X, double Z, double T, double Depth);
	constexpr double SurgeMaxSpeed = .55, SurgeDecayDepth = 22, SurgePeriod = 7.5;

	// ---- deterministic hashing (shared/rng)
	double HashCell(uint32 Seed, int32 CX, int32 CZ, int32 Salt);   // [0, 1)
	struct FMulberry
	{
		uint32 S;
		explicit FMulberry(uint32 Seed) : S(Seed) {}
		double operator()();
	};
}
