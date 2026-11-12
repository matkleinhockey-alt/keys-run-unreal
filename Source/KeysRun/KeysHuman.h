#pragma once

#include "CoreMinimal.h"

struct FKeysMesh;

// What a person looks like. Ported (simplified) from the JS makeHuman options.
struct FKeysHumanLook
{
	FLinearColor Skin = FLinearColor(FColor(0xd9, 0xa2, 0x7f));
	FLinearColor Shirt = FLinearColor::White;
	FLinearColor Bottom = FLinearColor(FColor(0x33, 0x44, 0x55));   // shorts, or the bikini
	FLinearColor Hair = FLinearColor(FColor(0x3b, 0x2a, 0x1e));
	FLinearColor Shoes = FLinearColor::White;
	FLinearColor CapColor = FLinearColor(FColor(0x1d, 0x35, 0x57));
	bool bBikini = false;
	bool bLongHair = false;
	bool bGlasses = false;
	bool bCap = false;
	bool bShoes = false;
	bool bShortsLong = false;
	int32 HairStyle = 0;   // 0 long, 1 wavy, 2 pony, 3 bun
};

// A low-poly person, rebuilt into an FKeysMesh every frame from a handful of pose targets.
// The person's own frame is X forward (the way they face), Y to their side, Z up, meters, feet on Z = 0.
// Poses are written with J(x, y, z), which takes the JS human frame (x side, y up, z forward) so the JS numbers carry over.
struct FKeysHuman
{
	FKeysHumanLook Look;
	bool bSeated = false;
	double HipY = .93;

	// hands and feet, in the person's frame (set with Pose / PoseLegs)
	FVector HandL = FVector::ZeroVector, HandR = FVector::ZeroVector, FootL = FVector::ZeroVector, FootR = FVector::ZeroVector;
	// placement on the boat (boat-local meters, UE axes: X bow, Y starboard, Z up) and body lean, radians
	FVector Pos = FVector::ZeroVector;
	double Yaw = 0, Pitch = 0, Roll = 0;
	// head: JS-style nod (positive tips the chin down) and look-around yaw, radians
	double HeadYaw = 0, HeadNod = 0;
	bool bHoldHead = false;   // when set, IdleUpdate leaves the nod alone
	double Ph = 0;
	bool bVisible = true;

	// Luigi mode: a can in the right hand
	bool bBeer = false;
	double Sip = 0, SipT = -1, SipAmt = 0;

	// how hard the wind is blowing the hair back, 0..1
	double Wind = 0;

	static FVector J(double X, double Y, double Z) { return FVector(Z, X, Y); }

	void Init(const FKeysHumanLook& InLook, bool bInSeated);
	void Pose(const FVector& L, const FVector& R) { HandL = L; HandR = R; }
	void PoseLegs(const FVector& L, const FVector& R) { FootL = L; FootR = R; }
	void DefaultPose();
	// gentle look-around and hair in the wind (JS h.update)
	void IdleUpdate(double T, double BoatSpeedMs);
	// mouth position for the beer sip, in the person's frame
	FVector Mouth() const { return J(.05, HipY + .71, .15); }

	// appends the posed person in boat-local meters
	void Build(FKeysMesh& M) const;
	FTransform ToBoat() const;
};

namespace KeysShapes
{
	// a low-poly ellipsoid (or the upper part of one, VMax < 1 cuts it off below: 0 = hemisphere)
	void AddBlob(FKeysMesh& M, const FVector& Center, const FVector& Radii, const FQuat& Rot, const FLinearColor& Color, int32 NU = 8, int32 NV = 5, double VMin = -1);
	// two-bone IK: shoulder S reaching for Target, bones L1 and L2, elbow bending toward Pole
	void SolveIK(const FVector& S, const FVector& Target, double L1, double L2, const FVector& Pole, FVector& OutElbow, FVector& OutEnd);
}
