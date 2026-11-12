#pragma once

#include "CoreMinimal.h"

// The Keys Run world, ported from the three.js game. All of this works in game meters on the flat (x, z) map:
// x runs along the island chain (west = -x toward the Seven Mile Bridge, east = +x toward Grassy Key),
// north (-z) is Florida Bay, south (+z) is Hawk Channel, Sombrero Reef and the Gulf Stream.
// Unreal space is centimeters with Z up, so a game point (x, y, z) sits at FVector(x*100, z*100, y*100).
namespace KeysGeo
{
	constexpr double WorldX0 = -4300, WorldZ0 = -3100, WorldSize = 8600;
	constexpr double WBX0 = -4150, WBX1 = 4150, WBZ0 = -2950, WBZ1 = 5350;

	struct FIsland
	{
		FString Name;
		double X, Z, A, B, Th, C, S;
		bool bSmall, bMainland, bMiami, bNoHouses;
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
	};

	// Builds islands, creeks, marinas and the fast depth grids. Safe to call more than once.
	void Init();

	inline double ChainZ(double X) { return 0.000012 * X * X; }
	inline FVector ToUE(double X, double Y, double Z) { return FVector(X * 100.0, Z * 100.0, Y * 100.0); }

	const TArray<FIsland>& Islands();
	const TArray<FMarina>& Marinas();
	const TArray<FHump>& Humps();

	FShore ShoreInfo(double X, double Z);
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
	void SetSeaState(int32 Index);
	int32 SeaIndex();
	double SeaSW();
	double SeaCH();
	double AmpFor(double D);
	double AmpAt(double X, double Z);
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

	// The white prop wash behind a planing hull: a trail of stern points that spreads and fades.
	// Source tells boats apart so their trails don't join up. Strength is 0..1.
	void EmitTrail(int32 Source, double X, double Z, double Width, double Strength, double T);
	struct FLiveTrail { double AX, AZ, BX, BZ, W0, W1, F0, F1, A0, A1, MinX, MinZ, MaxX, MaxZ; };
	void GatherTrails(double CX, double CZ, double Radius, double T, TArray<FLiveTrail>& Out);
	// foam amount (0..1) and how aerated the water is (0..1) at a point
	FVector2D TrailFrom(const TArray<FLiveTrail>& Live, double X, double Z);

	// bridge pilings, for boat collisions
	struct FPillar { double X, Z, R; bool bOld; };
	const TArray<FPillar>& Pillars();
	void PillarsNear(double X, double Z, TArray<const FPillar*>& Out);
}
