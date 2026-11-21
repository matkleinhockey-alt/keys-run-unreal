#pragma once

#include "CoreMinimal.h"
#include "KeysMesh.h"

// The boats and the clutter of the dry stack and the boatyard (KeysYards.cpp places them). Everything is built in
// metres in its own frame, X forward, Y to starboard / the right, Z up. The boats are the game's own lofted hull
// (KeysMesh::BuildHullLoft) in four sets of proportions; nothing here is a new hull shape.
namespace KeysYardProps
{
	FLinearColor Hex(uint32 C);
	double Hash(double A, double B);
	// appends Src (built in its own frame) to Dst, moved by T (translation in metres)
	void Append(FKeysMesh& Dst, const FKeysMesh& Src, const FTransform& T);
	// a solid wheel or drum on the axle A - B
	void Wheel(FKeysMesh& M, const FVector& A, const FVector& B, double R, const FLinearColor& C, int32 Sides = 12);
	void Box(FKeysMesh& M, double CX, double CY, double CZ, double SX, double SY, double SZ, const FLinearColor& C, double YawRad = 0);

	enum EBoatKind : uint8 { BoatSkiff, BoatBay, BoatCC, BoatSport };
	struct FBoat
	{
		uint8 Kind = BoatCC;
		double L = 8, B = 2.6;
		uint32 Hull = 0xffffff, Accent = 0x23466e, Bottom = 0x1f3a5c, Top = 0xffffff, Cowl = 0xf2f2f2;
		int32 Engines = 2;
		bool bTop = true;
		bool bSanded = false;      // the bottom paint sanded back in patches to primer and gelcoat
		bool bNoEngines = false;   // outboards off: a bare transom with its bolt holes and the rigging hanging out
		bool bWrapped = false;     // shrink-wrapped for storage: the hull and the wrap, nothing else to see
		FString Name, Brand;
	};
	// one of the invented fleet: a flats skiff, a bay boat or a centre console no longer than MaxL
	FBoat MakeBoat(int32 Seed, double MaxL = 9.2, bool bNoSkiff = false);
	KeysMesh::FHullDef HullOf(const FBoat& B);
	// boat frame: X bow, Y starboard, Z up, the waterline at Z = 0
	void BuildBoat(FKeysMesh& M, const FBoat& B);
	double KeelZ(const FBoat& B);                 // the keel, aft of amidships
	double BottomZ(const FBoat& B, double Off);   // the hull bottom Off metres off the centreline, aft of amidships
	double SheerZ(const FBoat& B);
	double TopZ(const FBoat& B);                  // the highest thing aboard

	// ---- what a boat stands on ashore: keel blocks under her and jack stands along the chines. Ground is the
	// ground's height in the boat's frame (her keel sits on the blocks).
	void BuildStands(FKeysMesh& M, const FBoat& B, double GroundZ);
	// the height of the waterline over the ground for a boat on blocks
	double BlockedHeight(const FBoat& B);

	// ---- yard clutter, each standing on Z = 0 at its origin
	void JackStand(FKeysMesh& M, const FVector& At, double H, double TiltYaw = 0);
	void JackStandStack(FKeysMesh& M, const FVector& At, int32 N);
	void PaintBucket(FKeysMesh& M, const FVector& At, uint32 Label, bool bOpen = false);
	void OutboardOnStand(FKeysMesh& M, const FVector& At, double Yaw, uint32 Cowl, bool bCowlOff = false);
	void Trailer(FKeysMesh& M, const FVector& At, double Yaw, double L);
	void SignBoard(FKeysMesh& M, const FVector& At, double Yaw, double W, double H, double PostH, uint32 Face);
	void FuelPump(FKeysMesh& M, const FVector& At, double Yaw, uint32 Body);
	void PressureWasher(FKeysMesh& M, const FVector& At, double Yaw);
	void Drum(FKeysMesh& M, const FVector& At, uint32 C);
	void Ladder(FKeysMesh& M, const FVector& Foot, const FVector& Top);
	void Dumpster(FKeysMesh& M, const FVector& At, double Yaw);
	void Pallet(FKeysMesh& M, const FVector& At, double Yaw);

	// ---- the travel lift: a blue steel gantry on four wheels, open at one end, with two slings hung from its top
	// beams. Origin on the ground under its middle, X along the slip. SlingZ: the bottom of the slings' belly.
	// the boat's section where the slings pass (in the lift's frame)
	struct FTravelLift { double L = 9.5, W = 8.2, H = 8.6, SlingX = 2.2, KeelZ = 1.2, ChineZ = 1.6, ChineHalf = 1.5, SheerZ = 3.2, SheerHalf = 1.8; };
	void TravelLift(FKeysMesh& M, const FTravelLift& T);
}
