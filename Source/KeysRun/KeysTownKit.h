#pragma once

#include "CoreMinimal.h"
#include "KeysTown.h"

// The town style kit: the fish-town set dressing every building in the game draws from - the Boot Key Harbor City
// storefronts (AKeysTown), their interiors, and the conch houses scattered over the keys (AKeysWorld).
//
// Each piece is a prefab: one function that builds it into an FTownBatch (three.js town frame: x = u, y up, z = -v,
// meters) at a spot and a turn you give it, with a dice roll (FKitDice) that varies its paint, wear, size and pose, so
// two copies never come out the same. On top of the pieces, DressShop / DressSideWall / DressInterior / DressHouse are
// the recipes: given a building's walls they pick which pieces go where, in which colours, from the building's own seed.
//
// Everything is invented: no real business, brand or logo. The look: sun-faded pastels (coral, seafoam, faded yellow,
// sky blue), salt-grey wood, rust and peeling paint.
namespace KeysTownKit
{
	// a small deterministic dice (one per building, seeded from its id or position)
	struct FKitDice
	{
		uint32 S;
		explicit FKitDice(uint32 Seed) : S(Seed * 2654435761u + 0x9e3779b9u) { Next(); Next(); }
		uint32 Next() { S ^= S << 13; S ^= S >> 17; S ^= S << 5; return S; }
		double F() { return (Next() & 0xffffff) / double(0x1000000); }                 // 0..1
		double R(double A, double B) { return A + (B - A) * F(); }
		int32 I(int32 N) { return N > 0 ? int32(Next() % uint32(N)) : 0; }
		bool Chance(double P) { return F() < P; }
	};
	uint32 SeedOf(const TCHAR* Text);
	uint32 SeedOf(double A, double B);

	// ---------------------------------------------------------------- the palette
	enum class EPaint : uint8 { Coral, Seafoam, FadedYellow, SkyBlue, WhiteWash, Teal, Navy, Count };
	FLinearColor Paint(EPaint P);
	FLinearColor RandomPastel(FKitDice& D);
	// sun-fade and salt: K 0 = fresh paint, 1 = bleached nearly to chalk
	FLinearColor Weathered(const FLinearColor& C, double K, FKitDice& D);
	FLinearColor SaltWood(FKitDice& D);       // grey, silvered planks
	FLinearColor WarmWood(FKitDice& D);       // varnished, a bit sun-burnt
	FLinearColor Rust(FKitDice& D);

	// a flat surface to hang things on: O on the surface at ground level, R along it (left to right as you face it),
	// N straight out of it. At(a, y, d): a along, y up, d out.
	struct FWall
	{
		FVector O, R, N;
		FVector At(double A, double Y, double D) const { return O + R * A + FVector(0, 1, 0) * Y + N * D; }
		static FWall Make(const FVector& O, const FVector& Out)
		{
			const FVector N = FVector(Out.X, 0, Out.Z).GetSafeNormal();
			return {O, FVector::CrossProduct(FVector(0, 1, 0), N), N};
		}
	};

	// lettering for the caller to put up as TextRender (the town has text; the keys' houses don't): P in the town frame,
	// facing Facing (horizontal)
	struct FKitText { FString Text; FVector P; FVector Facing; double Size, Width; FLinearColor Ink; bool bInterior = false; };

	// ---------------------------------------------------------------- the prefabs
	// A trophy fish on a varnished plaque, its side to the room. Pose 0 straight, 1 arched (a jump), 2 turned (head and
	// tail curling out at you), 3 leaping (nose up). Species: sailfish, tarpon, mahi, permit, snook, barracuda, goliath.
	void MountedFish(FTownBatch& B, const FWall& W, double A, double Y, const FString& Species, double Len, int32 Pose, FKitDice& D);
	// A hand-painted fish-shaped wooden sign. Flush on a wall, or bBlade: hanging off an iron bracket square to the wall
	// so you read it from down the street (lettering on both faces). Text may be empty (the keys' houses).
	void FishSign(FTownBatch& B, const FWall& W, double A, double Y, double Len, const FString& Text, FKitDice& D, TArray<FKitText>* Texts, bool bBlade);
	// a plank sign with a painted fish and lettering, flush on a wall
	void PlankSign(FTownBatch& B, const FWall& W, double A, double Y, double Wd, const FString& Text, FKitDice& D, TArray<FKitText>* Texts);
	// a lobster buoy hanging by its rope from a nail at Top, banded in two or three colours
	void Buoy(FTownBatch& B, const FWall& W, double A, double Y, double Size, FKitDice& D);
	// a few net floats (old glass or cork and plastic) bunched in a net, hanging from a nail
	void NetFloats(FTownBatch& B, const FWall& W, double A, double Y, FKitDice& D);
	// a slatted lobster trap (or a stack of them) on the ground at Base, turned Yaw about the vertical
	void LobsterTraps(FTownBatch& B, const FVector& Base, double Yaw, int32 Count, FKitDice& D, bool bSimple = false);
	// a coil of rope, lying on the ground (bHang false) or hanging on a peg on a wall
	void RopeCoil(FTownBatch& B, const FVector& C, const FVector& Axis, double R, FKitDice& D);
	void RopeOnWall(FTownBatch& B, const FWall& W, double A, double Y, FKitDice& D);
	// a cast net hung from a peg, its lead line weighting the hem
	void CastNet(FTownBatch& B, const FWall& W, double A, double Y, FKitDice& D);
	// two or three old rods crossed on pegs, cork grips and old reels
	void OldRods(FTownBatch& B, const FWall& W, double A, double Y, FKitDice& D);
	// a weathervane: a fish on a spindle over the four points, on a roof at Base
	void Weathervane(FTownBatch& B, const FVector& Base, double Size, FKitDice& D);
	// a fish-shaped mailbox on a post (mouth = the door, a red flag), at Base facing Facing
	void FishMailbox(FTownBatch& B, const FVector& Base, const FVector& Facing, FKitDice& D);
	// the town square's giant fibreglass leaping tarpon on a painted wave, on a plinth (Base = the plinth's top centre)
	void Statue(FTownBatch& B, const FVector& Base, double Height, FKitDice& D, TArray<FKitText>* Texts);
	// a faded tourist mural over A0..A1 x Y0..Y1 of a wall: sky, sea, sun, jumping tarpon or sailfish, peeling
	void Mural(FTownBatch& B, const FWall& W, double A0, double A1, double Y0, double Y1, const FLinearColor& WallC, int32 Theme, FKitDice& D, TArray<FKitText>* Texts);

	// ---------------------------------------------------------------- wear (on any wall)
	// salt-bleached streaks rising from the foot of a wall, chips of peeling paint, rust running down from a fixture
	// WallSurf: the wall's own surface kind, so the wear takes its texture (stucco, planks, siding)
	void SaltStains(FTownBatch& B, const FWall& W, double A0, double A1, double Y0, const FLinearColor& WallC, FKitDice& D, uint8 WallSurf = SurfStucco);
	void PeelingPaint(FTownBatch& B, const FWall& W, double A0, double A1, double Y0, double Y1, const FLinearColor& WallC, int32 Count, FKitDice& D, uint8 WallSurf = SurfStucco);
	void RustStreak(FTownBatch& B, const FWall& W, double A, double Y, double Len, const FLinearColor& WallC, FKitDice& D, uint8 WallSurf = SurfStucco);

	// ---------------------------------------------------------------- the recipes
	// a storefront: Front is the wall at the street; DoorA / DoorW where the doorway is along it; Width, Height the
	// wall's size; WindowY0/Y1 the height band the windows take; Free (pairs of along-ranges) the bits of wall clear of
	// windows and the door; Pun the blade sign's lettering; WallC the paint behind
	struct FShopSpots
	{
		FWall Front;
		double Width, Height, DoorA, DoorW, AwningY;
		TArray<FVector2D> Free;
		FString Pun;
		FLinearColor WallC;
		uint8 WallSurf = SurfStucco;
		bool bRoofFree;        // nothing (a hero sign) up on the roof, so a weathervane can go there
		bool bAwning = false;  // a striped awning out over the sidewalk (things can hang off its front edge)
		FVector RoofTop;       // the middle of the roof, at its top
	};
	void DressShop(FTownBatch& B, FTownBatch& Gl, const FShopSpots& S, FKitDice& D, TArray<FKitText>& Texts);
	// a bare side wall (A0..A1 along it, Height up): a mural (bMural) or a wall of fishing junk and trophies
	void DressSideWall(FTownBatch& B, const FWall& W, double A0, double A1, double Height, const FLinearColor& WallC, uint8 WallSurf, bool bMural, FKitDice& D, TArray<FKitText>& Texts);
	// inside a shop: trophies on the side and back walls above the shelves
	void DressInterior(FTownBatch& B, const FWall& Back, double BackLen, const FWall& Left, const FWall& Right, double SideLen, double Y, bool bBig, FKitDice& D);
	// a conch house: Front wall (Width wide, door at DoorA), the porch's front edge (PorchFront: a point on the porch at
	// its front, PorchHalf its half-width), Ridge the roof's peak, Road the way to the street
	struct FHouseSpots { FWall Front; double Width, DoorA, WallH; FVector PorchFront; double PorchHalf; FVector Ridge; FLinearColor WallC; double GroundY; };
	void DressHouse(FTownBatch& B, const FHouseSpots& S, FKitDice& D);

	// the kit's pun names (invented), for blade signs
	const TArray<FString>& Puns();

	// ---------------------------------------------------------------- the kit's shapes, for other builders (town frame)
	void KitBox(FTownBatch& B, const FVector& C, const FVector& AX, const FVector& AY, const FVector& AZ, double HX, double HY, double HZ, const FLinearColor& Col);
	void KitTube(FTownBatch& B, const FVector& A, const FVector& E, double RA, double RB, int32 Sides, const FLinearColor& Col);
	void KitEllip(FTownBatch& B, const FVector& C, const FVector& AX, const FVector& AY, const FVector& AZ, double RX, double RY, double RZ, const FLinearColor& Col, int32 Seg = 8);
	// a flat convex polygon (in the plane AX, AY at O) extruded T along N
	void KitSlab(FTownBatch& B, const TArray<FVector2D>& Poly, const FVector& O, const FVector& AX, const FVector& AY, const FVector& N, double T, const FLinearColor& Col, const FLinearColor& EdgeC, bool bBack = true);
	TArray<FVector2D> KitEllipse(double CX, double CY, double A, double Bb, int32 N);
	// a species' real body laid along Nose, back to UpV, flank toward Side; Arch lifts its middle, Thin < 1 flattens it
	// (a painting or a photo), FadeK fades it, PaintK makes it poster-bright
	void KitFish(FTownBatch& B, const FString& Key, double Len, const FVector& C, const FVector& Nose, const FVector& UpV, const FVector& Side,
		double Arch, double Thin, double FadeK, double PaintK, FKitDice& D, bool bFull = false);
	// a cut-out side-on fish (Kind 0 tarpon, 1 sailfish, 2 mahi, 3 grouper, 4 barracuda, 5 permit), both faces
	void KitCutout(FTownBatch& B, int32 Kind, const FVector& C, const FVector& AX, const FVector& AY, double Len, double T, const FLinearColor& Body, const FLinearColor& Fin, const FLinearColor& Edge, bool bEye);
	// the outline of that fish (pieces, length 1, nose to +x), for neon tubes
	TArray<TArray<FVector2D>> KitFishOutline(int32 Kind);
	// a brown pelican perched at Feet, facing Facing, its head cocked by Cock (radians)
	void Pelican(FTownBatch& B, const FVector& Feet, const FVector& Facing, double Cock, FKitDice& D);
}
