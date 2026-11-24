#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "KeysSfxSynth.h"
#include "KeysReefLife.generated.h"

struct FKeysMesh;

// Reef life: the animals of the reef that are not schooling boids (KeysSchools) and not spear targets (KeysLife).
// One actor, AKeysReefLife, owns every individual near the diver or the boat. Species are not listed here.
//
// THE REGISTRY. A species lives in its own file, KeysReefLife<Group>.cpp, and registers itself with one line:
//
//     static KeysReefLife::FAuto GStoplight(&MakeStoplight);      // FSpecies MakeStoplight();
//
// FAuto only notes the function; the actor calls it at BeginPlay, sorts the species by Key (so the order files
// happen to link in changes nothing) and from then on treats them all alike. Adding a species never edits this
// file, KeysReefLife.cpp or another species' file. What MakeXxx fills in (FSpecies, below):
//   Key, Common, Scientific     the test-command key, the real common name, the binomial
//   Phases                      one FPhase per colour phase or life stage: name, length range (m), body shape (FBody)
//   Paint                       colour of every vertex: (phase, FPaint{part, U, V, ...}) -> linear colour
//   Mixes                       how groups are made up: e.g. 1 terminal male + 3..6 initial phase, or 1..2 alone
//   Role                        one of the six behaviours written once in KeysReefLife.cpp (ERole)
//   Depth[4], Zones             where it lives: a depth trapezoid (m) and a mask of reef zones (EZone)
//   GroupsPerHa, MaxAlive       how many groups a hectare of perfect habitat holds; the species' own cap
//   Weight                      budget units one individual costs (1 = a 30 cm fish); the system holds 900 units
//   Habit, Shy                  awake by day, by night or both; metres it keeps from a slow diver
//   Cruise, Burst, Roam, Alt, Spacing, Cull   speeds (m/s), home range (m), height over the reef (m), spacing in
//                               body lengths, distance (m) past which it is not drawn
//   traits                      bRows (swims with its pectoral fins), bBitesRock, BiteEvery, ScarCm, PuffEvery,
//                               bCocoon, ChaserPhase, ChaseR, BiteSound, FirstSight
//
// PLACEMENT. The reef is cut into 32 m chunks. A chunk within 64 m of the diver (or the boat) is dealt once from a
// seed made of its coordinates and the species' Key: the same animals in the same places every run. Sixteen points
// of the chunk are read through KeysGeo / KeysSea / KeysReef (HabitatAt: depth, zone, reef cover, coral heads); a
// species takes a point with probability Depth x Zones x cover x GroupsPerHa. Chunks past 84 m are let go.
//
// DRAWING. One instanced mesh per species, phase and level of detail (three), M_KR_Boid, culled past Cull. A fish
// that rows is drawn without pectoral fins up close and gets two fin instances that the CPU swings.
//
// BEHAVIOUR. Each individual is a small state machine stepped by its role; what it does is a function of the
// feeding clock's hour, the diver's distance and speed and its neighbours, never of a timer alone. All of them steer
// round coral heads (a 1 m height field of the bottom, heads and wrecks), cannot enter rock and stay under the
// surface. Cause-and-effect log lines start "KRReef".
//
// BUDGET. At most 900 budget units alive (individuals x Weight) and never more than a species' MaxAlive; the nearest
// chunks are dealt first, so what is left out is always the far edge. reef:stats says how many were refused.
//
// TEST COMMANDS (DebugCmd, routed from -KRScript as reef:<command>): stats, find <key> [phase], look <key> [phase]
// [metres] | look off, census [metres], hab, pins 0|1, on, off, role <key> <0..5>, rival, do <bite|puff|breath|lift|dig>.
namespace KeysReefLife
{
	enum class ERole : uint8 { Grazer, Pair, Territory, Hoverer, Sitter, Cruiser };
	enum class EHabit : uint8 { Day, Night, Both };
	// reef zones (HabitatAt): the breaking crest, the reef top, a spur's top, a patch reef inshore of the reef line,
	// the fore reef between spurs, the deep wall; and what is not reef: sand, seagrass, low hard bottom
	enum EZone : uint32 { ZCrest = 1, ZTop = 2, ZSpur = 4, ZPatch = 8, ZFore = 16, ZWall = 32, ZSand = 64, ZGrass = 128, ZHard = 256 };
	enum EPart : uint8 { PBody, PDorsal, PAnal, PTail, PPectoral, PPelvic, PEye };

	// one vertex to colour. Body: U 0 nose .. 1 tail base, V -1 belly .. 1 back, I / J the station and the place
	// round it (for scale patterns), Side -1 / 1. Fins: U 0 front (or root) .. 1 back (or tip), V 0 root .. 1 edge
	// (tail and pectoral: V -1 lower .. 1 upper edge). Eye: U 0 centre .. 1 rim. Lod 0 near .. 2 far.
	struct FPaint { EPart Part = PBody; double U = 0, V = 0; int32 I = 0, J = 0, Side = 1, Lod = 0; };

	// a fish's shape, in body lengths (the whole fish, nose to tail tip, is 1)
	struct FBody
	{
		double HalfH = .17, HalfW = .075;        // half the greatest depth and width
		double Peak = .32;                       // where it is deepest (0 nose .. 1 tail base)
		double Blunt = .5;                       // 0 a pointed snout .. 1 a blunt, steep head
		double NoseR = .22;                      // the snout's own half-depth, of the greatest
		double Ped = .4, PedW = .2;              // the tail base's half-depth and half-width, of the greatest
		double Arch = .08;                       // how much more the back curves than the belly
		double TailLen = .17, TailH = .3;        // the tail fin: length, and height at its trailing edge
		double TailFork = 0;                     // + a crescent or fork (the middle cut in), - rounded
		double Dor0 = .3, Dor1 = .9, DorH = .07; // dorsal fin: from, to, height
		double An0 = .62, An1 = .9, AnH = .055;  // anal fin
		double DorTrail = 0, AnTrail = 0;        // > 0: the fin rises toward its rear and the tip is drawn out this far behind its base (angelfish)
		double PecU = .27, PecV = -.15, PecLen = .17, PecW = .09;   // pectoral fin: where its root is, length, width
		double PelU = .34, PelLen = .09;         // pelvic fins
		double EyeU = .1, EyeV = .4, EyeR = .022;
	};

	struct FPhase { FString Name; double LenMin = .2, LenMax = .3; FBody Body; };
	// a kind of group: Weight is how often it is dealt; NA of phase A and NB of phase B (B < 0: none)
	struct FMix { double Weight = 1; int32 A = 0, AMin = 1, AMax = 1, B = -1, BMin = 0, BMax = 0; };

	// what the bottom is at a point (game metres): Zones is one EZone bit, or 0 where reef life never goes (land,
	// canals, mangrove creeks, dredged ground)
	struct FHabitat { double Depth = 0, FloorY = 0, Reef = 0, Hard = 0, Grass = 0, Relief = 0; uint32 Zone = 0; };

	// ---- bottom animals and others that are not fish-shaped (a species file may test this macro)
	#define KEYS_REEFLIFE_BOTTOM_HOOKS 1
	// what a species' own Pose is told about one animal, and what it may change: the body's transform, the two part
	// transforms (the Kind 3 mesh, drawn twice near: rowing fins, eye stalks) and the eight M_KR_Boid floats
	struct FPoseIn { int32 Phase = 0; double Len = .3, Seed = 0, Now = 0, Hour = 12, DiverDist = 1e9, Speed = 0, StateT = 0; bool bHop = false; FVector P = FVector::ZeroVector, Fwd = FVector(1, 0, 0); };
	struct FPoseOut { FTransform Xf, Part[2]; float Data[8] = {0, 0, 0, 0, 0, 0, 1, 0}; };

	struct FSpecies
	{
		FString Key, Common, Scientific;
		ERole Role = ERole::Grazer;
		TArray<FPhase> Phases;
		TArray<FMix> Mixes;
		TFunction<FLinearColor(int32 Phase, const FPaint& At)> Paint;
		double Depth[4] = {1, 3, 8, 15};
		uint32 Zones = ZTop | ZSpur | ZPatch;
		EHabit Habit = EHabit::Day;
		double GroupsPerHa = 6;
		int32 MaxAlive = 60;
		double Weight = 1;
		double Shy = 2;
		double Cruise = .35, Burst = 2.5, Roam = 16, Alt[2] = {.3, 1.2}, Spacing = 2.5, Cull = 60;
		// ---- traits (all off by default)
		bool bRows = false;          // swims with its pectoral fins: a rowing beat, the tail still except in a burst
		bool bBitesRock = false;     // grazes rock: tips beak-down onto a coral head or the reef, bites, moves on
		double BiteEvery = 8;        // seconds between bites for one fish at the best of the day
		double ScarCm = 0;           // > 0: a bite leaves a pale scar this wide that fades
		double PuffEvery = 0;        // > 0: lets go a puff of white sand about this often (s)
		bool bCocoon = false;        // sleeps in a clear mucus cocoon
		int32 ChaserPhase = -1;      // this phase chases a rival of the same phase off its group
		double ChaseR = 5;           // ... when the rival comes this near (m); a Territory holder's radius too
		// the bite's sound: fill Out with one-shots for AKeysAudio::Burst and say in What exactly what they are
		TFunction<void(int32 Phase, TArray<FKeysBurst>& Out, FString& What)> BiteSound;
		FString FirstSight;          // the toast the first time the diver sees one
		// ---- animals that are not fish-shaped (a ray, a turtle; a generated model later)
		// Its own body. Fills M for one level of detail (Kind 0 near, 1 mid, 2 far, 3 one limb) in the body builder's
		// frame: nose +X, back +Z, the whole animal 1 long (FKeysMesh::AddVert turns that into 100 units). When set,
		// BuildBody and BuildPectoral are not used and FBody only gives behaviour its sizes (HalfH, HalfW). A model
		// replaces the animal's looks by replacing this function and nothing else.
		TFunction<void(FKeysMesh& M, int32 Phase, int32 Kind)> Build;
		// With bRows: where its two limbs (the Kind 3 mesh) are in the body's frame (Unreal units, 100 = its length),
		// from the stroke's running phase (cycles) and its effort 0..1. Side 0 right, 1 left. Unset: a fish's rowing fins.
		TFunction<FTransform(int32 Phase, int32 Side, double Stroke, double Effort)> Limb;
		double LimbWave = 0;         // > 0: the limbs are bent by the material's wave (a ray's wings): its amplitude at full effort
		bool bBitesBottom = false;   // with bBitesRock: feeds off any bottom of its zones (seagrass, sand), not rock alone
		double BiteTilt = 50;        // how far it tips nose-down to bite, degrees
		double BreathEvery = 0;      // > 0: breathes air: goes up to the surface about this often (s), stays BreathSecs, goes down again
		double BreathSecs = 4;
		double Bury = 0;             // > 0 (a Sitter): lies sunk this fraction of its body depth in the sand; a cloud of sand when it bolts
		double DigEvery = 0;         // > 0 (a Sitter): digs for food about this often (s) at the best of the day: sand clouds, and a pit left behind
		// ---- not fish-shaped (all unset by default)
		// its own mesh in place of the fish builder: Kind 0..2 a level of detail, 3 the part drawn twice near (needs
		// bRows). Unreal local frame, +X ahead, +Y right, +Z up, in body lengths (FKeysMesh::AddVert). False: the fish.
		TFunction<bool(int32 Phase, int32 Kind, FKeysMesh& M)> Mesh;
		// called after the framework has posed the animal, each time it is stepped
		TFunction<void(const FPoseIn& In, FPoseOut& Out)> Pose;
		// its own say in where it lives: multiplies the suitability of a dealt point (0 never .. 1)
		TFunction<double(double X, double Z, const FHabitat& Hab)> Where;
		bool bNoBolt = false;        // never bolts: not from the diver, the boat or a spear
		bool bOnFloor = false;       // lies on the floor itself, never on top of a coral head
		double HeadWithin = 0;       // > 0: lives at the foot of a stony coral head: each is put against the nearest
		                             // within this many metres of where it was dealt; with none, the group is not spawned
		bool bHomeOnHead = false;    // the group's home is the top of the nearest coral head within 8 m (a nest on a rock, a station over a head)
	};

	using FMake = FSpecies (*)();
	void Register(FMake Make);
	struct FAuto { explicit FAuto(FMake Make) { Register(Make); } };
	const TArray<FSpecies>& Registry();      // built on first use, sorted by Key
	int32 FindSpecies(const FString& Key);

	FHabitat HabitatAt(double X, double Z);
	const TCHAR* ZoneName(uint32 Zone);

	// the body builder: Lod 0 near (bWithPectorals false leaves them off for the rowing fins), 1 mid, 2 far.
	// Unreal local frame, nose along +X, 100 units long; BuildPectoral is one fin, root at the origin, pointing aft.
	void BuildBody(FKeysMesh& M, const FBody& B, int32 Lod, bool bWithPectorals, const TFunction<FLinearColor(const FPaint&)>& Paint);
	void BuildPectoral(FKeysMesh& M, const FBody& B, const TFunction<FLinearColor(const FPaint&)>& Paint);
	inline FLinearColor Hex(uint32 H) { return FLinearColor(FColor((H >> 16) & 255, (H >> 8) & 255, H & 255)); }
	constexpr double BudgetUnits = 900;
	struct FImpl;
}

UCLASS()
class KEYSRUN_API AKeysReefLife : public AActor
{
	GENERATED_BODY()

public:
	AKeysReefLife();
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;
	virtual void Tick(float DeltaSeconds) override;

	static AKeysReefLife* Get(const UObject* WorldContext);
	// the test hooks listed above
	void DebugCmd(const FString& Cmd);
	// a shot, a spear: reef animals within R of the point bolt (game metres)
	void Startle(const FVector& P, double R, const TCHAR* Why);
	int32 Alive() const;
	int32 AliveWithin(double X, double Z, double R) const;

private:
	friend struct KeysReefLife::FImpl;
	UPROPERTY() TObjectPtr<USceneComponent> Root;
	UPROPERTY() TArray<TObjectPtr<class UInstancedStaticMeshComponent>> Pools;
	UPROPERTY() TArray<TObjectPtr<class UStaticMesh>> Meshes;
	UPROPERTY() TObjectPtr<class UMaterialInstanceDynamic> MID;
	UPROPERTY() TObjectPtr<class UProceduralMeshComponent> Fx;
	UPROPERTY() TObjectPtr<class UMaterialInstanceDynamic> FxMID;
	KeysReefLife::FImpl* Impl = nullptr;
};
