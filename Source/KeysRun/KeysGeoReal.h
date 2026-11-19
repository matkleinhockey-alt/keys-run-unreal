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

	// a real aid to navigation (NOAA ENC): a red (even, triangle) or green (odd, square) daybeacon on a pile
	struct FNavMark
	{
		double X = 0, Z = 0;
		bool bRed = false, bLit = false;
		int32 Num = 0;
		FString Name;
	};
	const TArray<FNavMark>& Marks();

	// a lot of the Keys Run Estates plat surveyed on the real islands (Tools/geo/plat.py); Tier as AKeysEstates::ETier
	struct FPlatLot
	{
		FString Id, Name, Island, Side;
		int32 Tier = 4;
		double CX = 0, CZ = 0, Area = 0, Frontage = 0;
		TArray<FVector2D> Poly;
	};
	const TArray<FPlatLot>& Plat();

	// mangrove cover on the height grid's cells (Tools/geo/bake_mangrove.py): 0 none, 1..127 the fringe wading in the
	// shallows, 200..255 mangrove on (low) ground
	uint8 MangroveCell(int32 I, int32 J);
	uint8 MangroveAt(double X, double Z);
	int32 NX();
	int32 NZ();
	double Cell();

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
	void FlattenChain(double XA, double XB, double Z);   // run US-1 straight along z = Z between XA and XB (through the town)
	const TArray<FIsl>& Islands();
	// game point <-> real position
	void ToLonLat(double X, double Z, double& Lon, double& Lat);
	void FromLonLat(double Lon, double Lat, double& X, double& Z);
	double Scale();
	double Theta();
}
