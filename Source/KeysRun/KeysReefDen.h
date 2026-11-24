#pragma once

#include "CoreMinimal.h"
#include "KeysReef.h"

struct FKeysMesh;

// The reef's hiding places and what lives in them (KeysReefDen*.cpp). A den is a look and a home, not a cave to swim
// into: a block of reef rock with a dark pocket modelled into it - an undercut ledge, a crevice between two coral
// heads, a round hole, a low cave mouth, a shadow under plates of coral - and now and then an animal with its front end
// showing. The seafloor is a height mesh and cannot be cut, so the rock stands on it and carries its own hole: one
// closed surface that runs from the back of the pocket, out over the rim and round the outside, so that no angle
// shows the bottom, the sky or the back of a polygon. Dens are dealt by 50 m reef chunk after the corals (no coral
// moves for them), by depth, zone and what the reef is doing there; the same dens and the same animals every run.
namespace KeysReefDen
{
	// on both maps, unless -KRNoDens (the reef as it was, for before / after comparisons)
	bool Enabled();

	enum EKind : uint8 { Ledge, Crevice, Hole, Cave, Plate, NumKinds };
	// where on the reef: none (sand, seagrass, flats, canals), a patch reef, the crest and back reef, the fore reef,
	// the flank of a spur beside its sand groove, the deep reef
	enum EHab : uint8 { HabNone, HabPatch, HabCrest, HabFore, HabSpur, HabDeep, NumHab };
	enum ERes : uint8
	{
		ResNone, ResEyes, GreenMoray, SpottedMoray, Lobster, Octopus, Squirrelfish, Soldierfish, Sweepers, NurseShark,
		Goliath, BlackGrouper, ClingingCrab, StoneCrab, Shrimp, NumRes
	};
	const TCHAR* KindName(int32 Kind);
	const TCHAR* HabName(int32 Hab);
	const TCHAR* ResName(int32 Res);      // the common name, as the logbook line gives it
	const TCHAR* ResKey(int32 Res);       // one word, for the test commands
	constexpr int32 NumVariants = 3;

	// one den, placed (game metres, y up)
	struct FDen
	{
		int64 Id = 0;
		uint8 Kind = 0, Var = 0, Hab = 0, Res = 0, Count = 0;
		bool bBuddy = false;                  // a green moray a grouper comes to hunt with
		uint32 Seed = 0;
		double X = 0, Y = 0, Z = 0, Yaw = 0, Scale = 1, Depth = 0;   // Depth: the water over it
		FVector Mouth = FVector::ZeroVector;  // the middle of the opening
		FVector Axis = FVector(0, 0, 1);      // out of the opening
		FVector Right = FVector(1, 0, 0);     // across it
		double W = .3, H = .3, Deep = .5;     // the opening's width and height, and how far back the dark goes
		FVector C = FVector::ZeroVector, Rad = FVector(1);   // the rock, as an ellipsoid (its own axes turned by Yaw)
	};

	// what the reef's own rules say about a spot (KeysReef.cpp): the dens are dealt from the same ground as the corals
	struct FGround
	{
		double Depth = 0, DZ = 0, Sand = 0, Patch = 0, HardDeep = 0, Dredged = 0;
		int32 Zone = 0;       // 0 other, 1 Hawk Channel, 2 the reef line
		int32 Site = 0;       // 1 the Sombrero dive site (spur and groove)
		bool bMicro = false;  // a micro patch reef in Hawk Channel
	};
	FGround GroundAt(double X, double Z);
	double ChunkHash(int32 CX, int32 CZ, int32 Salt);
	EHab HabitatOf(const FGround& G);

	// the dens of one 50 m chunk, given the corals already placed there. Deterministic. Stats (optional): how many
	// candidates the habitat accepted, and how many found room.
	void PlaceChunk(int32 CX, int32 CZ, const TArray<AKeysReef::FInst>& Corals, TArray<FDen>& Out);
	// the dens of a chunk as placed (cached with the reef's own chunk cache); empty when there is no reef actor
	const TArray<FDen>& DensOfChunk(int32 CX, int32 CZ);

	// ---- hooks for the hub (requests R1 - R3), all safe to call with no reef in the world
	// R1, the diver: a body of radius R at (X, Y, Z) is moved out of any den's rock; true when it was touching.
	bool PushOut(double& X, double& Y, double& Z, double R);
	// R1, the spear: how far along the ray (O, unit D) the shaft flies before it meets a den's rock; MaxT when it does not.
	double RayHit(const FVector& O, const FVector& D, double MaxT);
	// R2, the schools: the holes a small fish keeps away from just now (x, y, z, keep-off radius in metres)
	void SchoolThreats(TArray<FVector4>& Out);
	// R2: the schools lend a count of their fish within R metres of a point, so a test line can say what the keep-off
	// did (unset, nothing is counted)
	extern TFunction<int32(const FVector&, double)> SchoolFishCounter;
	// R3, the wrecks: where a hull's plating is torn open. The wreck builder calls this for each gap it wants a den in
	// (the middle of the gap, the plating's outward normal, the gap's width and height, game metres); the den system
	// draws the dark behind it and deals it a resident (green moray, goliath grouper, glassy sweepers).
	void AddWreckGap(const FVector& Mouth, const FVector& Out, double W, double H, uint32 Seed);

	// ---- the runtime, owned by the reef actor
	struct FDens;
	FDens* Create(AKeysReef* Reef);
	void Destroy(FDens* D);
	// the rocks, laid out again when the camera changes chunk (with the reef's own instances)
	void Rebuild(FDens* D, double CamX, double CamZ, int32 CCX, int32 CCZ);
	void Tick(FDens* D, double Dt);
	void DebugCmd(FDens* D, const FString& Cmd);
}
