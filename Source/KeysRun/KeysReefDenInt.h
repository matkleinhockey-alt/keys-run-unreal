#pragma once

// What the den files share with each other (KeysReefDen.cpp: the rock, the placement, the runtime; KeysReefDenLife.cpp:
// the residents). Nothing outside them includes this.
#include "KeysReefDen.h"
#include "KeysMesh.h"

class UInstancedStaticMeshComponent;
class UStaticMesh;
class UMaterialInterface;

namespace KeysReefDen
{
	constexpr double ChunkM = 50;
	constexpr int32 GapKind = NumKinds;        // a torn gap in a wreck's plating (dealt by the wreck, not by the reef)
	constexpr int32 NumRockKinds = NumKinds + 1;

	// a den mesh in its own frame (metres; x across, y up, z out of the front), at scale 1
	struct FShape
	{
		FVector Mouth = FVector::ZeroVector, Axis = FVector(0, 0, 1);
		double W = .3, H = .3, Deep = .5, Top = 1;
		FVector C = FVector::ZeroVector, Rad = FVector(1);
		double Pw = 2;
	};
	const FShape& ShapeOf(int32 Kind, int32 Var);
	// the rock as a mesh in Unreal's local frame (x across, y out of the front, z up). Lod 0 near, 1 far.
	void BuildRock(int32 Kind, int32 Var, int32 Lod, FKeysMesh& M);

	inline FVector G2U(const FVector& P) { return FVector(P.X * 100.0, P.Z * 100.0, P.Y * 100.0); }
	inline FVector D2U(const FVector& D) { return FVector(D.X, D.Z, D.Y); }
	FLinearColor Hex(uint32 C);
	double Hash01(uint32 Seed, int32 Salt);
	double Noise3(const FVector& P, double Seed);
	inline double Smooth(double V, double Lo, double Hi) { const double T = FMath::Clamp((V - Lo) / (Hi - Lo), 0.0, 1.0); return T * T * (3 - 2 * T); }
	// the bottom as the seafloor mesh draws it near a diver (a 2 m lattice), which is not quite the true height
	double FloorNear(double X, double Z);
	// the nearest named reef to a spot ("Sombrero Reef"), or "the reef"
	FString ReefNameAt(double X, double Z);

	// Mat is the school fish's material (M_KR_Boid) or the plain vertex-colour one. bNormalize: nose at +50, tail at
	// -50 mesh units whatever its length, as the school fish are (the material's tail beat counts on it).
	UStaticMesh* MakeStatic(UObject* Owner, const FKeysMesh& M, UMaterialInterface* Mat, const FString& Name, bool bNormalize, int32* OutTris = nullptr);

	// what the residents are told each frame
	struct FWorldNow
	{
		double Now = 0, Dt = 0, Hour = 12;
		bool bNight = false;
		FVector Cam = FVector::ZeroVector, CamFwd = FVector(0, 0, 1);
		bool bDiver = false;
		FVector Diver = FVector::ZeroVector, DiverV = FVector::ZeroVector;
		bool bLight = false;
		FVector LightP = FVector::ZeroVector, LightD = FVector(0, 0, 1);
		bool bCalm = false, bScript = false;
	};

	// the reef tells the dens when it deals a chunk its corals (the dens go in after them), and when it lets one go
	void OnChunk(FDens* D, int32 CX, int32 CZ, TArray<AKeysReef::FInst>& Corals);
	void Forget(FDens* D, int64 Key);

	// ---- the residents (KeysReefDenLife.cpp)
	struct FLife;
	FLife* CreateLife(AKeysReef* Reef);
	void DestroyLife(FLife* L);
	// Near: every den within 60 m of the camera (the residents of anything further are let go)
	void TickLife(FLife* L, const FWorldNow& W, const TArray<const FDen*>& Near);
	void LifeThreats(const FLife* L, TArray<FVector4>& Out);
	// stats, hunt, calm ... (the part of kr.den that is about animals); false when it is not one of its words
	bool LifeDebug(FLife* L, const FWorldNow& W, const TArray<FString>& Args);
	// where to put a camera to look at this den's resident (it may have walked off): its place now
	FVector LifeWhere(const FLife* L, const FDen& D);
	void LifeSheetDens(const FLife* L, TArray<FDen>& Out);   // the test sheet's stand-in dens (kr.den sheet)
	struct FLifeStats { int32 Animals = 0, Animated = 0, Instances = 0, Tris = 0, Pools = 0; int32 ByRes[NumRes] = {}; double CpuMs = 0; };
	FLifeStats LifeStatsOf(const FLife* L);
}
