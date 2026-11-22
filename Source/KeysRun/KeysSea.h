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
	// What the bottom is made of at a point in water D deep, each 0..1 (what is left over is sand): reef (the reef
	// line and the patch reefs, less in their sand channels), hard bottom (low rock in beds), seagrass, the soft mud of
	// Florida Bay's basins, and the bare marl of a dredged canal or the way cut out of it. One rule for the whole map,
	// from depth and from where the point lies between the bay and the drop-off; every edge is a fade.
	struct FBottom { double Reef = 0, Hard = 0, Grass = 0, Mud = 0, Marl = 0; };
	FBottom BottomAt(double X, double Z, double D);
	// 0..1: in a dredged canal or the way cut from its mouth (FBottom::Marl by itself, for whoever plants the bottom)
	double DredgedAt(double X, double Z);
	// biome color (linear) at a point with base depth D; Shade darkens steep faces, Rel is the relief there; bGrain
	// adds the per-vertex speckle (averages to 1; off for meshes too coarse to carry it)
	FLinearColor SeafloorColorAt(double X, double Z, double D, double Shade = 1, double Rel = 0, bool bGrain = true);

	// The floor as a coarse mesh should carry it: the mean height and the mean colour of N x N points spread over the
	// SizeX x SizeZ rectangle centred on (X, Z), coloured as a fine mesh would be there (rock on the relief, steep
	// faces darker). A coarse mesh that samples single points instead aliases the reef's 30 m spurs and terraces, and
	// reads as a different tint from the fine chunks next to it. N = 0: the smooth depth alone, for the far distance.
	void SeafloorMean(double X, double Z, double SizeX, double SizeZ, int32 N, double& OutH, FLinearColor& OutC);

	// ---- water movement (m/s, horizontal, game x/z)
	FVector2D ChainTangent(double X);
	FVector2D ChainNormal(double X);   // points offshore (+z)
	FVector2D SteadyCurrentAt(double X, double Z, double T);
	FVector2D SurgeAt(double X, double Z, double T, double Depth);
	FVector2D CurrentAt(double X, double Z, double T, double Depth);
	constexpr double SurgeMaxSpeed = .55, SurgeDecayDepth = 22, SurgePeriod = 7.5;

	// a test (fish:canals): log, as KRCanal lines, every residential canal a hull needing MinD metres cannot leave for
	// the open sea, with the way that would open it
	void LogCanalWays(double MinD);

	// ---- deterministic hashing (shared/rng)
	double HashCell(uint32 Seed, int32 CX, int32 CZ, int32 Salt);   // [0, 1)
	struct FMulberry
	{
		uint32 S;
		explicit FMulberry(uint32 Seed) : S(Seed) {}
		double operator()();
	};
}
