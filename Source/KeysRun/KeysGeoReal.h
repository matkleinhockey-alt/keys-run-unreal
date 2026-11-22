#pragma once

#include "CoreMinimal.h"

// The real Florida Keys, baked into the game frame by Tools/geo/bake.py from public data: NOAA NCEI / USGS elevation
// and bathymetry, and OpenStreetMap coastlines (c) OpenStreetMap contributors, ODbL 1.0. One file per phase in
// Content/KeysRun/Geo, loaded side by side as one map. Real positions are compressed Scale : 1 and turned Theta so the island chain runs along +x;
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

	// the residential canals (Tools/geo/canals.py, <phase>_canals.txt): centre lines the bake widened to Width
	struct FCanal
	{
		double Width = 24;
		TArray<FVector2D> Pts;
	};
	const TArray<FCanal>& Canals();
	// metres outside the nearest canal's edge (negative in the canal), 1e9 when none is within 28 m of it. bCap: the
	// nearest point is past the free end of a line (a canal's mouth, or its dead end)
	double CanalDist(double X, double Z, bool* bCap = nullptr);
	// the same distance, and whether the canal has a bank here: always along its sides and round a dead end, not across
	// a mouth (past a free end with water beyond)
	bool CanalBank(double X, double Z, double& OutDist);
	// metres outside the edge of the nearest way dredged from a canal's mouth to deep water (negative in it), 1e9 if none near
	double WayOutDist(double X, double Z);

	// a named place of a later phase (Tools/geo/sites_p3.py, <phase>_sites.txt), for the hub to build on. Kind: "light"
	// (a reef light: Value is the depth it stands in), "reef" (Value the depth on its crest), "marina" (an invented
	// name; X2, Z2 a point in open water off it, Value the depth there), "bridge" (a road bridge that is not on the
	// chain line, from X, Z to X2, Z2), "spot" (somewhere worth a teleport: Value is a heading), "town" (a place name)
	struct FSite
	{
		FString Kind, Name;
		double X = 0, Z = 0, X2 = 0, Z2 = 0, Value = 0;
	};
	const TArray<FSite>& Sites();
	// US-1 itself: the chain line carries the road only between these x. West of RoadX0 (Key West) and east of RoadX1
	// (where US-1 leaves Key Largo for the mainland) the line goes on as the axis of the keys, with no road or bridge
	// on it. The map's own ends when no phase says otherwise.
	double RoadX0();
	double RoadX1();

	// mangrove cover on the height grid's cells (Tools/geo/bake_mangrove.py): 0 none, 1..127 the fringe wading in the
	// shallows, 200..255 mangrove on (low) ground
	uint8 MangroveCell(int32 I, int32 J);
	uint8 MangroveAt(double X, double Z);
	int32 NX();
	int32 NZ();
	double Cell();
	// cell (0, 0) of the whole map, counted on the first phase's own grid: (0, 0) with p1 alone, negative once a later
	// phase reaches further north or west. Anything seeded by cell number adds these, so it does not change as the map grows.
	int32 CellI0();
	int32 CellJ0();

	// The map is baked in phases, one .kgeo each, that tile it: p1 (the Middle Keys), p2 (the Upper Keys), p2_bay (the
	// strip of Florida Bay north of p1), p3 (the Lower Keys), p4 (Key Largo) and p4_bay (the bay north of the rest).
	// Everything below answers for the whole map, whichever files a point falls in.
	bool Load(const TCHAR* Phase);    // the first phase alone
	bool Add(const TCHAR* Phase);     // another phase beside what is loaded (false, and no change, if its file is missing)
	bool LoadAll();                   // p1 and every later phase that is there (a missing one is left out): the real map
	bool Loaded();
	int32 Phases();                   // how many files are loaded
	int32 PhaseAt(double X, double Z);   // which of them a point is in (0 = p1)
	double X0();
	double Z0();
	double SizeX();
	double SizeZ();
	double Height(double X, double Z);   // metres: + land, - the depth of water
	double Shore(double X, double Z);    // metres from the shoreline: + at sea, - on land
	// is any cell of this box on land or within D metres of it? (what scenery built by tile asks before it makes one:
	// most of the map is open water)
	bool ShoreUnder(double X0, double Z0, double X1, double Z1, double D);
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
