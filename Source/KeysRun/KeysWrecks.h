#pragma once

#include "CoreMinimal.h"

struct FKeysMesh;

// Shipwrecks on the bottom, ported from the web game's packages/shared/src/world/wrecks.ts (where they lie: each spec
// walks hashed candidates until one passes every test against the real depth, land, zone and shore) and
// apps/client/src/entities/life/wrecks.ts / wreck-chart.ts (the steel and the chart symbol). A cast within 28 m of
// a hull fishes the "Wreck" table (WRECK_RESIDENTS).
namespace KeysWrecks
{
	enum class EKind : uint8 { Plane, Barge, Tug, Freighter };

	struct FWreck
	{
		const TCHAR* Id;
		const TCHAR* Name;
		EKind Kind;
		double Len, Beam, Height, List, Mass;
		bool bBroken;
		// placed
		double X = 0, Z = 0, SeaDepth = 0, Heading = 0, Pitch = 0, Gap = 0, BreakYaw = 0;
	};

	constexpr double CastM = 28;    // WRECK_CAST_M
	constexpr uint32 WorldSeed = 20240817;

	const TArray<FWreck>& Wrecks();
	// the hull whose edge is within R of (X, Z), nearest first
	const FWreck* Near(double X, double Z, double R = CastM);
	double EdgeDist(const FWreck& W, double X, double Z);
	// the fish that live on them (WRECK_RESIDENTS)
	const TArray<TPair<FString, double>>& Residents();
	// every hull, in Unreal world space (meters; FKeysMesh scales to centimeters), resting on the bottom
	void Build(FKeysMesh& M);
}
