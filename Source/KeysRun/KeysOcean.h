#pragma once

#include "CoreMinimal.h"

// The simulated wind sea (Tessendorf's FFT ocean, the way GodotOceanWaves and most games since do it): a spectrum of
// waves on two square tiles that repeat across the sea, 133 m (the 8-130 m waves) and 31 m (the 1-8 m chop), each a
// 64 x 64 grid. Every frame each wave's phase moves on by its own deep-water speed and an inverse FFT turns the spectrum
// into heights, sideways displacements (sharp crests, broad troughs), slopes and the surface's Jacobian - which goes
// negative where a crest folds over itself, and that is where the whitecaps are.
//
// It only replaces the short wind sea. The long ocean swell stays the four analytic trains in KeysGeo (they read right
// at any distance), and the wind, its direction, the storm's confusion and the sea state's height all still come from
// KeysGeo's driver, so the sea states and the weather behave as they did.
//
// All sampling is on the CPU (the water mesh, the boat's buoyancy, buoys, fish and camera all read it), so it is cheap:
// 8 complex 64 x 64 FFTs a frame, well under a millisecond.
namespace KeysOcean
{
	// set the sea: target rms height (m), the peak wavelength (m), the direction the sea runs (radians, game x/z), how
	// confused it is (0 one direction .. 1 from everywhere) and how steep (0..1, sharper crests); then step to time T
	void Update(double T, double Rms, double PeakLen, double SeaDir, double Confusion, double Steep);

	// what the simulated sea adds at a point: height, and (for the water mesh) the sideways displacement; MinLen drops
	// the waves shorter than that (a coarse mesh far out can't show them, and they would only alias)
	double Height(double X, double Z, double MinLen = 0);
	FVector Displacement(double X, double Z, double MinLen = 0);   // (dx, dh, dz)
	FVector2D Slope(double X, double Z);                           // (dh/dx, dh/dz)
	// whitecap foam, 0..1: where crests have folded over lately (it builds while they break and fades over seconds)
	double Foam(double X, double Z);

	FString Stats();
	// the water material's copy of a tile (0 = 133 m, 1 = 31 m): per grid point (slope x, slope z, foam, height)
	void FillTexels(int32 Tile, TArray<FLinearColor>& Out);
	double TileLen(int32 Tile);
	int32 GridN();
	bool Enabled();
	void SetEnabled(bool bOn);   // off: KeysGeo falls back to its classic wave trains
}
