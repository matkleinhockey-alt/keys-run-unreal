#pragma once

#include "CoreMinimal.h"
#include "KeysMesh.h"
#include "KeysYardProps.h"

// The boat lifts of the private docks (KeysDocks.cpp places them): a four-pile cradle lift, an elevator lift on
// inclined rails against a seawall or a dock's edge, and a pair of davits with a skiff. Each is built in its own
// frame - X along the boat toward her bow, Y to starboard, Z up from the datum the depths are measured from, the
// origin under the middle of the berth - and then set down in the dock's frame. The boats are the yards' invented
// fleet (KeysYardProps::MakeBoat / BuildBoat); nothing here is a new hull.
namespace KeysLifts
{
	enum ELiftKind : uint8 { LiftNone, LiftCradle, LiftElevator, LiftDavits };
	// Hung: the boat up clear of the water. Afloat: the cradle lowered and the boat floating over it.
	// EmptyUp / EmptyDown: nobody home, the cradle left up or down.
	enum ELiftState : uint8 { StateHung, StateAfloat, StateEmptyUp, StateEmptyDown };

	constexpr double HighTide = .32;   // the tide's rise above the datum, m (KeysGeo's TideRangeM)

	struct FLift
	{
		uint8 Kind = LiftNone, State = StateHung;
		KeysYardProps::FBoat Boat;   // the boat the lift was bought for (its size sets the lift's, even when it is empty)
		double X = 0, Y = 0, Yaw = 0;   // the berth's middle and the bow's direction, in the dock's frame
		double DeckZ = 1.4;             // the deck or the seawall top the lift stands beside
		uint32 PileC = 0x6e5a44;        // the timber of its piles
		bool bSleeves = false;          // white sleeves on the piles above the water
		int32 Seed = 0;
		bool HasBoat() const { return Kind != LiftNone && (State == StateHung || State == StateAfloat); }
		bool Down() const { return State == StateAfloat || State == StateEmptyDown; }
	};

	// where a lift's parts go, all in the dock's frame. Float is drawn at the tide (its Z = 0 is the sea's level):
	// boats afloat, lowered cradles and the cables down to them.
	struct FParts
	{
		FKeysMesh* Structure = nullptr;   // piles, beams, rails, the raised cradle: seen from far off
		FKeysMesh* Detail = nullptr;      // cables, motors' small parts, guide posts: only close to
		FKeysMesh* Boats = nullptr;       // boats hung clear of the water
		FKeysMesh* Float = nullptr;
	};

	// one of the fleet for a lift: Want is a KeysYardProps::EBoatKind, or -1 for whatever comes
	KeysYardProps::FBoat PickBoat(int32 Seed, double MaxL, int32 Want = -1);
	// the height of a hung boat's waterline over the datum: her lowest part (the skeg) clears a high tide
	double HungWaterline(const KeysYardProps::FBoat& B);
	double LowestZ(const KeysYardProps::FBoat& B);
	// how far the berth's middle lies off the seawall or dock edge an elevator lift or davits stand on
	double EdgeOff(const KeysYardProps::FBoat& B);
	// what the lift and its boat cover, in the lift's own frame
	void Extent(const FLift& L, double& X0, double& X1, double& Y0, double& Y1);
	void Build(const FLift& L, const FParts& Out);

	// shared with the docks: a square beam from A to B, and a pile with the dark of the tide line on it
	void Beam(FKeysMesh& M, const FVector& A, const FVector& B, double W, double H, const FLinearColor& C, const FVector& Up = FVector::UpVector);
	// Cap: 0 none, 1 a white cone, 2 a black cap, 3 copper
	void Pile(FKeysMesh& M, double X, double Y, double Top, double R, uint32 Wood, int32 Cap, bool bSleeve = false);
}
