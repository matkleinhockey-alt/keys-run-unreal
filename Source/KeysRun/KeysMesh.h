#pragma once

#include "CoreMinimal.h"
#include "ProceduralMeshComponent.h"

namespace KeysData { struct FBoatSpec; }

// Vertex-colored geometry for the procedural meshes. Authored in meters; Add* calls convert to centimeters.
struct FKeysMesh
{
	TArray<FVector> V;
	TArray<int32> T;
	TArray<FVector> N;
	TArray<FVector2D> UV;
	TArray<FLinearColor> C;

	void Reset() { V.Reset(); T.Reset(); N.Reset(); UV.Reset(); C.Reset(); }
	int32 AddVert(const FVector& PosM, const FVector& Normal, const FLinearColor& Color);
	// one flat-shaded quad, a-b-c-d around the edge; Out is a hint for which way the face should point
	void AddQuad(const FVector& A, const FVector& B, const FVector& C, const FVector& D, const FLinearColor& Color, const FVector& Out);
	void AddTri(const FVector& A, const FVector& B, const FVector& C, const FLinearColor& Color, const FVector& Out);
	// box centered at Center with full Size, rotated by Rot (meters)
	void AddBox(const FVector& Center, const FVector& Size, const FLinearColor& Color, const FQuat& Rot = FQuat::Identity);
	// tapered tube between two points
	void AddTube(const FVector& A, const FVector& B, double RA, double RB, int32 Sides, const FLinearColor& Color);

	void Commit(UProceduralMeshComponent* Mesh, int32 Section, UMaterialInterface* Mat) const;

	// Smooth shading: vertices at the same spot share an averaged normal, unless their faces meet at more than
	// MaxAngleDeg (so fins and hard chines keep their edge).
	void SmoothNormals(double MaxAngleDeg = 55);
};

namespace KeysMesh
{
	// ---------------------------------------------------------------- the lofted hull (web entities/boat/hull.ts)
	// hullStation / zRake / hullProfile / buildHullGeo ported line for line: deadrise that warps from the transom
	// to a deep-V entry, chine flats, flared topsides, sheer spring, a raked stem, steps in the running bottom,
	// boot stripe, cove stripe and rub rail. Web frame (x starboard, y up, z toward the stern).
	struct FHullDef
	{
		double F = 1.3, Spring = .35, YK = .5, Dr0 = 20, Dr1 = 48, Rake = 1.2, Entry = .42;
		TArray<double> Steps;
		FLinearColor Bottom = FLinearColor(FColor(0xf8, 0xf7, 0xf2)), Boot = FLinearColor(FColor(0x23, 0x46, 0x6e)), Hull = FLinearColor(FColor(0xfb, 0xfa, 0xf6)),
			Cove = FLinearColor(FColor(0x23, 0x46, 0x6e)), Rub = FLinearColor(FColor(0xd0, 0xd4, 0xd8)), Cap = FLinearColor(FColor(0xfb, 0xfa, 0xf6)),
			Liner = FLinearColor(FColor(0xf3, 0xf2, 0xec)), Deck = FLinearColor(FColor(0xe0, 0xde, 0xd6));
		bool bCove = true;
		bool bCat = false;     // a catamaran: two asymmetric sponsons under a bridge deck (the Freeman)
		double Tunnel = .42;   // the cat's tunnel height
	};
	struct FHullSta { double T, Z, Bb, Bs, YK, YC, YS, BF; };
	FHullSta HullStation(const FHullDef& H, double L, double B, double T);
	FHullSta HullStationAtZ(const FHullDef& H, double L, double B, double Z);   // web z (toward the stern)
	double HullZRake(const FHullDef& H, const FHullSta& S, double Y);
	double HullSole(double L, const FHullSta& S);                               // the cockpit sole's height
	// one hull: the loft, the transom and (bDeck) covering boards, liner and sole. Xo shifts it across (a sponson);
	// AsymSide (-1 / 1) is the half that faces a cat's tunnel, flattened by AsymK.
	void BuildHullLoft(FKeysMesh& M, const FHullDef& H, double L, double B, bool bDeck = true, double Xo = 0, int32 AsymSide = 0, double AsymK = 1);
	// a cat's sponsons and bridge deck, or the monohull
	void BuildHullFor(FKeysMesh& M, const FHullDef& H, double L, double B);
	// the web game's HULLS[id] for one of the eight boats (content/boats.ts)
	const FHullDef& HullOf(const KeysData::FBoatSpec& S);
	// deck shape for the fit-out, the crew and the camera, in the boat's own frame (meters, +X toward the bow)
	double SoleAt(const KeysData::FBoatSpec& S, double X);     // cockpit sole
	double SheerAt(const KeysData::FBoatSpec& S, double X);    // top of the gunwale
	double InnerHalfBeam(const KeysData::FBoatSpec& S, double X);   // half-width inside the covering boards
	double BowX(const KeysData::FBoatSpec& S);                 // the stem at the sheer
	double SternX(const KeysData::FBoatSpec& S);               // the transom

	// hull, deck, console, T-top or hardtop and outboards for one of the eight boats. bRiggers: the tower boats'
	// outriggers, stowed (the player's boat leaves them out and swings its own with BuildRiggers).
	void BuildBoat(FKeysMesh& M, const KeysData::FBoatSpec& S, bool bRiggers = true);
	// Outriggers (web entities/boat/model.ts OUTRIGGERS): the tower boats (Grady-White, Freeman) carry two long poles
	// on swivel bases that swing from stowed upright (Rig 0) to about 70 degrees out and raked aft (Rig 1). Hulls with
	// fixed rigger poles on the T-top (Yellowfin, Contender) report those. False on a boat with nothing to hang a line
	// from. Tips and bases are in the boat's own frame, meters.
	bool RiggerTips(const KeysData::FBoatSpec& S, double Rig, FVector OutTip[2], FVector OutBase[2]);
	bool HasSwingingRiggers(const KeysData::FBoatSpec& S);
	void BuildRiggers(FKeysMesh& M, const KeysData::FBoatSpec& S, double Rig);
	// Boats with a tuna tower (the Grady-White and the Freeman): true, and where your eyes are standing at the upper
	// helm, in the boat's own frame (meters, +X toward the bow, Z up).
	bool TowerEye(const KeysData::FBoatSpec& S, FVector& OutEye);
	// a simple fish body with tail and dorsal fins, nose along +X, about Len meters long
	void BuildFish(FKeysMesh& M, double Len, const FLinearColor& Color, bool bBill);
	double FishLength(double WeightLb);
}
