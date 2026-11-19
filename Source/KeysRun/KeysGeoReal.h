#pragma once

#include "CoreMinimal.h"

// The real Florida Keys, baked into the game frame by Tools/geo/bake.py from public data: NOAA NCEI / USGS elevation
// and bathymetry, and OpenStreetMap coastlines (c) OpenStreetMap contributors, ODbL 1.0. One file per phase in
// Content/KeysRun/Geo. Real positions are compressed Scale : 1 and turned Theta so the island chain runs along +x;
// depths and heights are real.
namespace KeysGeoReal
{
	struct FIsl
	{
		FString Name;
		double X = 0, Z = 0, Area = 0, MinX = 0, MinZ = 0, MaxX = 0, MaxZ = 0;
		TArray<double> Ring;   // x0, z0, x1, z1, ... (not repeated at the end)
	};

	bool Load(const TCHAR* Phase);
	bool Loaded();
	double X0();
	double Z0();
	double SizeX();
	double SizeZ();
	double Height(double X, double Z);   // metres: + land, - the depth of water
	double Shore(double X, double Z);    // metres from the shoreline: + at sea, - on land
	int32 IslandAt(double X, double Z);  // the nearest island's index
	double ChainZ(double X);             // US-1's line
	double ChainSlope(double X);
	const TArray<FIsl>& Islands();
	// game point <-> real position
	void ToLonLat(double X, double Z, double& Lon, double& Lat);
	void FromLonLat(double Lon, double Lat, double& X, double& Z);
	double Scale();
	double Theta();
}
