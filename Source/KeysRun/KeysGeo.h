#pragma once

#include "CoreMinimal.h"

// The Keys Run world, ported from the three.js game. All of this works in game meters on the flat (x, z) map:
// x runs along the island chain (west = -x toward the Seven Mile Bridge, east = +x toward Grassy Key),
// north (-z) is Florida Bay, south (+z) is Hawk Channel, Sombrero Reef and the Gulf Stream.
// Unreal space is centimeters with Z up, so a game point (x, y, z) sits at FVector(x*100, z*100, y*100).
namespace KeysGeo
{
	// The map rectangle (terrain, depth grids, chart) and the bounds boats are kept inside. Either the original hand-made
	// 8.6 km square around Marathon, or the real Keys baked from NOAA / USGS / OpenStreetMap data (KeysGeoReal.h) when
	// that map is switched on with -KRRealMap. Set by Init().
	extern double WorldX0, WorldZ0, WorldSizeX, WorldSizeZ;
	extern double WBX0, WBX1, WBZ0, WBZ1;
	bool RealMap();
	// the square the chart shows: the whole map, centred (the real map is wider than it is tall)
	inline double ChartSize() { return FMath::Max(WorldSizeX, WorldSizeZ); }
	inline double ChartX0() { return WorldX0 + (WorldSizeX - ChartSize()) * .5; }
	inline double ChartZ0() { return WorldZ0 + (WorldSizeZ - ChartSize()) * .5; }

	struct FIsland
	{
		FString Name;
		double X, Z, A, B, Th, C, S;
		bool bSmall, bMainland, bMiami, bNoHouses;
		int32 Coast = -1;   // its real OSM outline (KeysCoastData.h), or -1 for the procedural mainland's ellipse
	};

	struct FMarina
	{
		FString Name;
		double SX, SZ, EZ;
		int32 Dir;
	};

	struct FShore
	{
		double D;
		const FIsland* Isl;
		double E;
	};

	struct FHump
	{
		FString Name;
		double X, DZ;
		bool bPatch;
	};

	struct FSeaState
	{
		FString Name;
		double SW, CH;
		FString Desc;
		double Wind = 7;   // the trade wind this sea state stands for, m/s
	};

	// Builds islands, creeks, marinas and the fast depth grids. Safe to call more than once.
	void Init();

	// US-1's line down the island chain: its z at x, and the angle it runs at there
	double ChainZ(double X);
	double ChainAngle(double X);
	// Content laid out on the original map as "DZ metres south of the chain" (reef 1300-1460, wall to 1650, Gulf Stream
	// beyond): the matching distance on the map in use. The real reef line and drop-off lie about twice as far out.
	// LegacyZ: where a point "DZ south of the chain at X" on the original map belongs on the map in use (the real reef
	// is measured against the real drop-off at that X). DZAt is the other way: the original-map DZ a point stands for,
	// so everything written in those terms (reef zones, relief, spawn bands) reads the real map correctly.
	double LegacyZ(double X, double DZ);
	double DZAt(double X, double Z);
	inline FVector ToUE(double X, double Y, double Z) { return FVector(X * 100.0, Z * 100.0, Y * 100.0); }

	const TArray<FIsland>& Islands();
	const TArray<FMarina>& Marinas();
	const TArray<FHump>& Humps();

	FShore ShoreInfo(double X, double Z);
	// Real coastlines (web shared/world/coast.ts, chain.ts): the signed distance outside island I's shore in meters
	// (negative on land, exact within 120 m), the normalized shore radius (<1 land, 1 shore, >1 sea), the outward
	// direction (the gradient of the distance), and the outline rings themselves (null for the mainland).
	double IslandDist(const FIsland& I, double X, double Z);
	double IslandE(const FIsland& I, double X, double Z);
	FVector2D IslandOutward(const FIsland& I, double X, double Z);
	const TArray<TArray<FVector2D>>* CoastRings(const FIsland& I);
	void IslandLocalXZ(const FIsland& I, double X, double Z, double& LX, double& LZ);
	void IslandWorldXZ(const FIsland& I, double LX, double LZ, double& X, double& Z);
	const FIsland* FindIsland(const TCHAR* Name);
	// the town's fill pad, US-1's causeway fill, the golf courses and the runway (web depth.ts inFill)
	bool InFill(double X, double Z);
	bool InTownPad(double X, double Z);            // on the town's raised pad (its top is 2.35 m)
	constexpr double TownUS1V = 132;               // real map: how far in from the town marina US-1 crosses the pad
	// how far inside the dredged cut of a marina basin (web depth.ts marinaCutDepth): > 0 inside
	double MarinaCutDepth(double X, double Z);
	double CreekDist(double X, double Z);
	bool OnCourse(const FIsland* I, double LX, double LZ);
	double LandH(double X, double Z);
	double DepthAt(double X, double Z);
	double DepthFast(double X, double Z);
	double OffshoreF(double X, double Z);
	bool NearBridge(double X, double Z);
	const FHump* NearHump(double X, double Z);
	FString ZoneAt(double X, double Z);
	FString ZoneDesc(const FString& Zone);
	FString NearestName(double X, double Z);
	bool InMiami(double X, double Z);
	const FMarina* NearMarina(double X, double Z);
	FLinearColor DepthRGB(double D);
	FLinearColor DepthColorFast(double X, double Z);
	// 0 natural ground, 1 golf course, 2 airport runway, 3 city
	int32 SurfaceKind(double X, double Z);
	bool InCity(double X, double Z);

	// sea state and waves
	const TArray<FSeaState>& SeaStates();
	// picks the sea state's wind and swell as the driver's base (the sea eases into it; SnapSea jumps)
	void SetSeaState(int32 Index);
	int32 SeaIndex();
	double SeaSW();   // the swell's height factor right now (eased)
	double SeaCH();   // the wind sea's height factor right now (eased; ~1.2 in a 7 m/s breeze)

	// ---- the wind and sea driver. One wind (speed m/s and the direction it blows toward, radians in the game x/z plane)
	// sets the wind sea: how tall, how long, how steep, how confused and how much whitecap. The base wind comes from the
	// sea state (H); weather (storm outflow) adds its own on top. Everything eases - wind in seconds, the sea it raises
	// in tens of seconds, and it lies down again slowly after the wind drops - so nothing ever snaps.
	// The waves are a fixed spectrum of wave trains (fixed lengths and directions, so their phases never jump) whose
	// heights are reweighted every frame. The water mesh and every hull sample the same WaveH.
	void SetStormWind(double Speed, double DirToward, double Confuse, double SwellAdd);
	void TickSea(double Dt);
	void SnapSea();
	double WindSpeed();
	double WindDir();          // radians, toward (game x/z)
	double SeaConfusion();     // 0 a clean wind sea, 1 short steep seas from everywhere
	double Whitecaps();        // 0..1
	double WaveRms();          // rms height of the open-water sea for Amp = 1 (crest foam normalizes by it)
	// the same sea, without the wave trains shorter than MinLen meters (a coarse mesh can't show them and would alias)
	double WaveHLod(double X, double Z, double T, double Amp, double MinLen);
	double AmpFor(double D);
	double AmpAt(double X, double Z);
	double ChopScale(double Amp);
	// waves breaking on a beach or reef edge: (extra height, foam) at a point of depth D. The crests follow the depth
	// contours (they slow in shallow water and swing round parallel to the shore), steepen as they shoal and break at
	// about 0.78 x the depth, leaving a band of white water rolling in behind; only where the bottom actually shelves.
	FVector2D SurfAt(double X, double Z, double T, double D);   // how much of the wind sea reaches water of that Amp
	double WaveH(double X, double Z, double T, double Amp, bool bWithWake = true);
	FVector2D WaveSlope(double X, double Z, double T, double Amp);
	// Interval: seconds since this boat's last EmitWake. Rings dropped more often are made weaker, so the wake's height
	// doesn't depend on the rate - a smaller interval just makes the V smoother.
	void EmitWake(double X, double Z, double V, double Len, double T, double Interval = .35);
	double WakeH(double X, double Z, double T);

	// The wake rings alive near (CX, CZ) right now, gathered once per frame so the water mesh doesn't scan the whole buffer per vertex.
	struct FLiveWake { double X, Z, R, WD, Amp, Reach; };
	void GatherWakes(double CX, double CZ, double Radius, double T, TArray<FLiveWake>& Out);
	double WakeHFrom(const TArray<FLiveWake>& Live, double X, double Z);
	double WakeHFrom(const TArray<FLiveWake>& Live, const TArray<int32>& Only, double X, double Z);   // just those wakes (a grid cell's)

	// The white prop wash behind a planing hull: a trail of stern points that spreads and fades.
	// Source tells boats apart so their trails don't join up. Strength is 0..1.
	void EmitTrail(int32 Source, double X, double Z, double Width, double Strength, double T);
	struct FLiveTrail { double AX, AZ, BX, BZ, W0, W1, F0, F1, A0, A1, MinX, MinZ, MaxX, MaxZ, G0 = 0, G1 = 0, S0 = 0, S1 = 0; };   // G: age (s), S: strength
	void GatherTrails(double CX, double CZ, double Radius, double T, TArray<FLiveTrail>& Out);
	// foam amount (0..1) and how aerated the water is (0..1) at a point
	FVector2D TrailFrom(const TArray<FLiveTrail>& Live, double X, double Z);
	FVector2D TrailFrom(const TArray<FLiveTrail>& Live, const TArray<int32>& Only, double X, double Z);
	// buckets the live wakes / trails on a square grid around (CX, CZ) so a water vertex only tests its own cell's
	void BucketWakes(const TArray<FLiveWake>& Live, double CX, double CZ, double Radius, double Cell, int32 N, TArray<TArray<int32>>& Out);
	void BucketTrails(const TArray<FLiveTrail>& Live, double CX, double CZ, double Radius, double Cell, int32 N, TArray<TArray<int32>>& Out);

	// bridge pilings, for boat collisions
	struct FPillar { double X, Z, R; bool bOld; };
	// US-1's bents stand every BridgeBentStep m from WBX0; over the boat channel a concrete arch spans from the pier at
	// BridgeArchX0 to the one at BridgeArchX1 with nothing in between (KeysBridges.cpp builds it to match)
	// the old Bahia Honda rail bridge (real map only): steel through-trusses on concrete piers from (X0, Z0) on Bahia
	// Honda Key to (X1, Z1), the road laid over the top of them; spans Gap0 .. Gap1 - 1 were cut out for boats
	struct FTruss { double X0, Z0, X1, Z1; int32 Spans, Gap0, Gap1; };
	const FTruss* BahiaHonda();
	constexpr double BridgeBentStep = 36;
	extern double BridgeArchX0, BridgeArchX1;
	// the old bridge: piers every 30 m from OldBridgeX0 to OldBridgeX1, at ChainZ + OldBridgeDZ, with the swing span gone
	constexpr double OldBridgeDZ = -58;
	extern double OldBridgeX0, OldBridgeX1, OldBridgeGap0, OldBridgeGap1;
	const TArray<FPillar>& Pillars();
	void PillarsNear(double X, double Z, TArray<const FPillar*>& Out);
}
