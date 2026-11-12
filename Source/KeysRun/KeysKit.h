#pragma once

#include "CoreMinimal.h"
#include "KeysMesh.h"

// Small helpers shared by the systems added in the second port pass (the working fleet, the street, the pistol,
// treasure, estates, the tournament show). Game frame as KeysGeo: meters, x east, y up, z south; a web heading h
// points (-sin h, -cos h) in x/z. A web object frame (x starboard, y up, z toward the stern) maps onto the Unreal
// boat-local frame (X bow, Y starboard, Z up) as (x, y, z) -> (-z, x, y).
namespace KeysKit
{
	inline FLinearColor Hex(uint32 C) { return FLinearColor(FColor(uint8((C >> 16) & 255), uint8((C >> 8) & 255), uint8(C & 255))); }
	inline double Smooth01(double A, double B, double X) { const double T = FMath::Clamp((X - A) / (B - A), 0.0, 1.0); return T * T * (3 - 2 * T); }
	inline double AngDelta(double A, double B) { return FMath::Fmod(FMath::Fmod(B - A + PI, 2 * PI) + 2 * PI, 2 * PI) - PI; }
	inline double AngTo(double A, double B, double T) { return A + AngDelta(A, B) * FMath::Min(1.0, T); }
	// a web heading to an Unreal yaw in degrees
	inline double YawDeg(double H) { return FMath::RadiansToDegrees(FMath::Atan2(-FMath::Cos(H), -FMath::Sin(H))); }
	inline double HeadingOf(double DX, double DZ) { return FMath::Atan2(-DX, -DZ); }
	inline FVector Web(double X, double Y, double Z) { return FVector(-Z, X, Y); }
	inline FString Money(double N)
	{
		FString S = FString::Printf(TEXT("%lld"), int64(FMath::RoundToDouble(FMath::Abs(N))));
		for (int32 i = S.Len() - 3; i > 0; i -= 3) S.InsertAt(i, TEXT(","));
		return (N < 0 ? TEXT("-$") : TEXT("$")) + S;
	}
	// three.js box / cylinder in a web object frame (rotation.set(rx, 0, rz), Euler XYZ), into Unreal boat-local meters
	inline void WebBox(FKeysMesh& M, double W, double H, double D, double X, double Y, double Z, const FLinearColor& C, double RX = 0, double RY = 0, double RZ = 0)
	{
		// Unreal local: X = -z, Y = x, Z = y (a mirror, so every angle changes sign): web x turns about Unreal Y, web y
		// about Unreal Z, web z about -X
		const FQuat Q = FQuat(FVector::YAxisVector, -RX) * FQuat(FVector::ZAxisVector, -RY) * FQuat(FVector::XAxisVector, RZ);
		M.AddBox(Web(X, Y, Z), FVector(D, W, H), C, Q);
	}
	inline void WebTube(FKeysMesh& M, double R, double Len, double X, double Y, double Z, const FLinearColor& C, double RX = 0, double RZ = 0, int32 Sides = 7)
	{
		const FVector Dir(-FMath::Sin(RZ), FMath::Cos(RZ) * FMath::Cos(RX), FMath::Cos(RZ) * FMath::Sin(RX));   // web frame
		const FVector A(X - Dir.X * Len / 2, Y - Dir.Y * Len / 2, Z - Dir.Z * Len / 2), B(X + Dir.X * Len / 2, Y + Dir.Y * Len / 2, Z + Dir.Z * Len / 2);
		M.AddTube(Web(A.X, A.Y, A.Z), Web(B.X, B.Y, B.Z), R, R, Sides, C);
	}
	// a hull's pose on the real waves: bow / stern and both rails (traffic.ts), Roll scaled by RollK
	struct FPose { double Y, Pitch, Roll; };
	FPose RidePose(double X, double Z, double H, double Len, double Beam, double T, double RollK = .9);
	FRotator PoseRot(const FPose& P, double H);
}

// World-space labels and speech the HUD projects onto the screen each frame (the web game's DOM `.tag`s):
// Larry's "yeahhhhh dude", the pirates' lines, a boat's name. Anything can add one per frame; the HUD draws and clears them.
struct FKeysTag
{
	FVector Game;           // game meters
	FString Text;
	double MaxDist = 260;
	bool bBubble = false;   // a speech bubble (white, with a tail) rather than a plain name tag
};
namespace KeysTags
{
	void Add(const FVector& Game, const FString& Text, double MaxDist = 260, bool bBubble = false);
	TArray<FKeysTag>& Live();
}
