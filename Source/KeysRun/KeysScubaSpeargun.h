#pragma once

#include "CoreMinimal.h"

struct FKeysMesh;

// The speargun's meshes, rebuilt on the user's reference picture (a roller gun): a slim matt-black barrel with a rail,
// a roller head at the muzzle with two black rollers, two pairs of olive rubber bands running from the rollers back
// along both sides to the wishbone, a black shaft with a broad barbed head (a wide swept main blade, two smaller barbs
// behind it, a hinged flopper), a slide ring on the shaft with a braided steel leader looping back to the muzzle, and a
// pistol grip with olive panels, trigger, guard, safety and a butt pad for loading against the chest. No name, logo or
// lettering anywhere on it. AKeysDiver (KeysDiver.cpp) owns the components and the animation (band stretch, recoil,
// reload, reel); everything here is static geometry in the gun's own frame: metres, +X out of the muzzle, Z up, the
// origin on the barrel's axis above the grip.
namespace KeysSpeargun
{
	constexpr double BarrelLen = .78, BarrelR = .014, ShaftR = .0035, BandR = .0075;
	constexpr double RailY = BarrelR + .0045 + ShaftR;          // the shaft's axis above the barrel's (it lies on the rail)
	constexpr double ShaftPoke = .3, ShaftLen = 1.0;            // the shaft's mesh has its origin half the poke behind the head's base
	constexpr double HeadLen = .17;                             // tip to the back of the collar
	constexpr double RollerX = BarrelLen - .04, RollerR = .021, RollerY = .034, RollerHalfW = .015;
	constexpr double BandAnchorX = RollerX, BandShort = .32, BandLong = .5;
	constexpr double WishBack = .03;                            // the wishbone's bridle runs this far back from the band ends to the shaft's notch
	constexpr double RingX = -.16;                              // the slide ring, in the shaft mesh's frame
	constexpr double LeaderLen = .36;
	inline double BandZ(double BandRad) { return RollerR + BandRad; }   // a band lies on top of its roller
	inline FVector LeaderHook() { return FVector(BarrelLen - .012, 0, -BarrelR - .008); }
	inline FVector ReelAt() { return FVector(.05, 0, -.062); }
	inline FVector GripTop() { return FVector(-.07, 0, -.025); }
	inline FVector GripBot() { return FVector(-.115, 0, -.135); }

	// barrel, rail, roller head and rollers, the bands' lower runs and their wrap round the rollers, the handle with its
	// grip, trigger, guard, safety and butt pad, the reel's stem and the line up to the muzzle. bLod: fewer sides and no
	// small detail (the gun in the diver's hand in the chase view).
	void BuildGun(FKeysMesh& M, bool bLod);
	// the reel's turning part, about its axle (local Y)
	void BuildReel(FKeysMesh& M);
	// a unit pair of bands along +X side by side (scaled per frame: length, radius, radius); Side -1 left, +1 right
	void BuildBandPair(FKeysMesh& M, int32 Side);
	// the wishbone: a bridle of braided line from each band pair back to a bead on the shaft's notch
	void BuildWishbone(FKeysMesh& M);
	// the shaft with its head, flopper, notches and slide ring; origin ShaftPoke / 2 behind the head's base
	void BuildShaft(FKeysMesh& M, bool bLod, const FVector& At = FVector::ZeroVector);
	// the barbed head alone: its point at Tip, flying along Dir, Up the side the flopper hangs away from
	void AddHead(FKeysMesh& M, const FVector& Tip, const FVector& Dir, const FVector& Up, double Scale, double FlopperDeg, bool bLod);
	// the braided steel leader from the slide ring to the hook under the muzzle, hanging in a loop while it is slack
	void BuildLeader(FKeysMesh& M, const FVector& Ring, const FVector& Hook, double Len);
	// the whole gun as carried in the chase view: section-ready pieces in one frame (bands at the loaded stretch)
	void BuildCarried(FKeysMesh& Gun, FKeysMesh& Shaft);
	// a shaft through a speared fish (the fish mesh's frame: nose +X, Z up, metres): in low on one flank, out high on
	// the other with the head and its open flopper standing clear of the back. HalfY / HalfZ: the body's half sizes.
	void AddShaftThroughFish(FKeysMesh& M, double HalfY, double HalfZ);
}
