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
	// hull, deck, console, T-top or hardtop and outboards for one of the five boats
	void BuildBoat(FKeysMesh& M, const KeysData::FBoatSpec& S);
	// Boats with a tuna tower (the Grady-White and the Freeman): true, and where your eyes are standing at the upper
	// helm, in the boat's own frame (meters, +X toward the bow, Z up).
	bool TowerEye(const KeysData::FBoatSpec& S, FVector& OutEye);
	// a simple fish body with tail and dorsal fins, nose along +X, about Len meters long
	void BuildFish(FKeysMesh& M, double Len, const FLinearColor& Color, bool bBill);
	double FishLength(double WeightLb);
}
