#pragma once

#include "CoreMinimal.h"

// Boot Key Harbor City: where the town goes and everything you can stand on in it. Pure placement data, ported from the web
// game's packages/shared/src/world/town.ts, world/casino.ts and content/venues.ts. The renderer (KeysTown.cpp) and the walker
// (KeysWalker.cpp) both build from these tables, so what you see is what you collide with.
//
// The frame: the town hangs off MARINAS[0] (Boot Key Harbor City Marina). Local (u, v): u runs across the frontage, v runs
// INLAND from the marina's shoreline sz. World x = sx + dir*u, z = sz - dir*v (game meters, see KeysGeo.h).
namespace KeysTown
{
	constexpr double DeckTop = 1.425;   // top of the dock planks
	constexpr double PadY = 2.35;       // the town's built-up fill pad
	constexpr double WallT = .4, DoorW = 2.8;
	constexpr double PadU0 = -104, PadU1 = 104, PadV0 = 18, PadV1 = 172;
	constexpr double RampU0 = -2.2, RampU1 = 2.2, RampV0 = 8, RampV1 = PadV0;

	struct FRect { double X0, X1, Z0, Z1; bool Contains(double X, double Z) const { return X >= X0 && X <= X1 && Z >= Z0 && Z <= Z1; } };
	struct FLocalBox { double U0, U1, V0, V1; };

	enum class EStyle : uint8 { Diner, Tackle, Guns, Liquor, Bar, Club, Gifts, Pawn, Laundromat, IceCream, Motel, Convenience, Dive, Casino };

	struct FLot
	{
		const TCHAR* Id;
		const TCHAR* Venue;
		EStyle Style;
		double U0, U1, V0, V1, H, Door;
	};

	// paint.ts LOOKS: wall, trim, roof, sign background / lettering / rim, neon, windows (0 store, 1 barred, 2 none, 3 diner, 4 small),
	// pattern (0 stucco, 1 brick, 2 planks, 3 chrome, 4 dark), awning stripes (0 = none)
	struct FLook { uint32 Wall, Trim, Roof, SignBg, SignFg, SignRim, Neon; int32 Windows, Pattern; uint32 Awn1, Awn2; };
	const FLook& LookOf(EStyle S);

	struct FShelfItem { const TCHAR* Id; const TCHAR* Name; const TCHAR* Blurb; int32 Price; const TCHAR* Effect; };
	struct FVenue
	{
		const TCHAR* Id;
		const TCHAR* Name;
		const TCHAR* Tagline;
		TArray<const TCHAR*> Greeting;
		TArray<FShelfItem> Sells;
	};
	const FVenue* Venue(const FString& Id);

	enum class ESurface : uint8 { Promenade, Road, Sidewalk, Lot };
	struct FSurface { ESurface Kind; double U0, U1, V0, V1; };

	struct FPalm { double U, V, H; };
	struct FPoint { double U, V; };
	enum class ECar : uint8 { Sedan, Pickup, Convertible, Suv, Van, Wagon, Cart };
	struct FCar { const TCHAR* Id; double U, V, Rot; ECar Kind; uint32 Color; bool bTrailer; };
	struct FSolidProp { double U, V, W, D; };
	struct FInteriorBox { const TCHAR* Id; const TCHAR* Lot; double U0, U1, V0, V1; };

	struct FInteract
	{
		FString Id;
		FString Venue;   // empty for a plaque
		double U, V, R;
		FString Prompt;
		FString Text;    // plaque text
	};

	// a figure that stands (or sits, or dances) at a fixed spot; Look picks the outfit (people.ts humanOptionsFor)
	struct FNpc { FString Look; double U, V, Yaw; FString Inside; double Y = 0; int32 Variant = 0; };

	// the casino's slot cabinets (world/casino.ts)
	struct FCabinet { double U, V, FU, FV; int32 Theme; double SU, SV; bool bOccupied; };

	const TArray<FLot>& Lots();
	const FLot* Lot(const FString& Id);
	TArray<FLocalBox> LotWalls(const FLot& L);
	const TArray<FSurface>& Surfaces();
	const TArray<FPalm>& Palms();
	const TArray<FPoint>& Lamps();
	const TArray<FPoint>& Benches();
	const TArray<FPoint>& TrashCans();
	const TArray<FCar>& Cars();
	const TArray<FSolidProp>& SolidProps();
	const TArray<FInteriorBox>& InteriorBoxes();
	const FInteriorBox* InteriorBox(const FString& Id);
	const TArray<FInteract>& Interacts();
	const TArray<FNpc>& Npcs();
	const TArray<FCabinet>& Cabinets();
	extern const TCHAR* const CabinetThemes[8];

	// the frame (MARINAS[0])
	double OX();
	double OZ();
	int32 Dir();
	inline void ToWorld(double U, double V, double& X, double& Z) { X = OX() + Dir() * U; Z = OZ() - Dir() * V; }
	inline void ToLocal(double X, double Z, double& U, double& V) { U = Dir() * (X - OX()); V = Dir() * (OZ() - Z); }
	FRect WorldRect(double U0, double U1, double V0, double V1);
	// game meters (x, y, z) of a local point at height Y
	FVector WorldPos(double U, double V, double Y);

	// Every marina's dock planks: main pier, T-head and six finger piers (world/marinas.ts via marinaDeckBoxes).
	struct FDeckBox { double W, D, CX, CZ, T, Y; };
	const TArray<FDeckBox>& DeckBoxes();
	// the harbourmaster's shed 9 m inland of each pier head
	FRect MarinaShedRect(int32 MarinaIndex);

	// What you stand on at world (x, z): the dock planks, the ramp, the pad; with bNatural, any dry land as well.
	bool GroundAt(double X, double Z, double& OutY, bool bNatural = true);
	// every solid thing a person collides with (walls, furniture, cars, props, sheds), world AABBs
	const TArray<FRect>& Colliders();
	void CollidersNear(double X, double Z, double R, TArray<const FRect*>& Out);
	// Solid scenery out on the keys, for a person on foot: the conch houses and palms (KeysWorld) and the waterfront
	// estates' houses (KeysLandmarks). A box is centered at (CX, CZ) with half-length HA along the unit axis (AX, AZ)
	// and HB across it; a round one is a trunk.
	void AddObstacleBox(double CX, double CZ, double AX, double AZ, double HA, double HB, int32 Group = -1);
	void AddObstacleRound(double X, double Z, double R, int32 Group = -1);
	bool ObstacleAt(double X, double Z, double R);
	// a group of obstacles that can be taken away: the scattered houses and palms standing on a Keys Run Estates
	// parcel (the group is the parcel's index), cleared when the lot is built on
	void SetObstacleGroupsOff(const TSet<int32>& Off);
	// what stands on your built lots (AKeysEstates): the residences, solid to a person on foot
	struct FExtraSolid { double CX, CZ, AX, AZ, HA, HB; };
	TArray<FExtraSolid>& ExtraSolids();
	// near a waterfront estate's grounds (the keys' scattered houses and palms keep off them)
	bool NearEstate(double X, double Z, double R);
	// the town pad grown by Margin (for keeping the island's scattered palms and houses out of the streets)
	bool InFootprint(double X, double Z, double Margin = 0);
	// the made ground under the town (depth.ts TOWN_FILL), in the marina's frame
	bool InFill(double X, double Z);
	// the closest point on any marina's deck to (x, z) within Reach, pulled 0.45 m inside the plank
	// docks that come and go: what you have built on your Keys Run Estates lots (AKeysEstates). A ramp up from the
	// shore over DockRamp m, then the deck at 1.425 m out to Len, and a T-head HalfT wide on a yacht dock.
	struct FExtraDock { double X0, Z0, SX, SZ, Len, HalfW, THead, G0; FString Name; };
	TArray<FExtraDock>& ExtraDocks();
	bool FindBerth(double X, double Z, double Reach, double& OutX, double& OutZ, FString& OutName);
	// which enterable lot's interior (x, z) is in, or nullptr
	const FLot* InsideLot(double X, double Z);

	// Harbor Row: the four town estates behind the casino (content/estates.ts tier 'harbor')
	struct FEstateLot { const TCHAR* Id; const TCHAR* Name; double U0, U1, V0, V1; int32 Price; };
	const TArray<FEstateLot>& HarborRow();

	// Public fishing piers out from the keys' shores (world/piers.ts): a walkway from the shore out to fishing water and a T at the end.
	struct FPier { FString Name; double X0, Z0, NX, NZ, Len; };
	constexpr double PierTop = 1.3, PierW = 3, PierHeadW = 14, PierHeadD = 5;
	const TArray<FPier>& Piers();
	// Waterfront estates (world/waterfront.ts): a house set back from the water, a pool, and a private dock with a boat lift.
	struct FEstateSite { double X, Z, NX, NZ, DockLen; int32 Style; uint32 Color; FString Island; };
	const TArray<FEstateSite>& WaterfrontEstates();

	// rough dock prices for fillets, $/lb (content/economy.ts MEAT_PRICE); 0 = no sale
	double MeatPrice(const FString& SpeciesKey);
}
