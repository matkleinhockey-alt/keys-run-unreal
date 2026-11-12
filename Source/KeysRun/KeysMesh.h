#pragma once

#include "CoreMinimal.h"
#include "ProceduralMeshComponent.h"

namespace KeysData { struct FBoatSpec; }

// What M_KR_VertexColor shades a vertex as (UV1.x): roughness, metalness and a procedural detail normal per kind.
// Default is a matte painted surface with a little grain; nothing needs tagging to look right.
enum EKeysSurf : uint8
{
	SurfDefault = 0, SurfSkin = 1, SurfCloth = 2, SurfHair = 3, SurfGelcoat = 4, SurfNonSkid = 5, SurfSteel = 6,
	SurfVinyl = 7, SurfWood = 8, SurfStucco = 9, SurfTin = 10, SurfAsphalt = 11, SurfSand = 12, SurfLeaf = 13,
	SurfGlass = 14, SurfFish = 15, SurfBark = 16, SurfRubber = 17, SurfSiding = 18, SurfConcrete = 19, SurfFin = 20,
	SurfEye = 21, SurfShingle = 22, SurfGrass = 23, SurfQuilt = 24, SurfHexQuilt = 25, SurfMetalPaint = 26, SurfTeakDeck = 27
};

// Vertex-colored geometry for the procedural meshes. Authored in meters; Add* calls convert to centimeters.
struct FKeysMesh
{
	TArray<FVector> V;
	TArray<int32> T;
	TArray<FVector> N;
	TArray<FVector2D> UV;
	TArray<FLinearColor> C;
	// UV1: x = the surface kind M_KR_VertexColor shades it as (EKeysSurf), stamped by AddVert from Surf
	TArray<FVector2D> K;
	uint8 Surf = 0;

	void Reset() { V.Reset(); T.Reset(); N.Reset(); UV.Reset(); C.Reset(); K.Reset(); Surf = 0; }
	// pads K to V (for code that appends to V / N / UV / C directly)
	void SyncKinds() { if (K.Num() < V.Num()) { const int32 Old = K.Num(); K.SetNum(V.Num()); for (int32 i = Old; i < K.Num(); i++) K[i] = FVector2D::ZeroVector; } else if (K.Num() > V.Num()) K.SetNum(V.Num()); }
	// re-stamp the surface kind of the vertices from index From on
	void SetSurfFrom(int32 From, uint8 Kind) { SyncKinds(); for (int32 i = FMath::Max(0, From); i < K.Num(); i++) K[i].X = Kind; }
	int32 AddVert(const FVector& PosM, const FVector& Normal, const FLinearColor& Color);
	// one flat-shaded quad, a-b-c-d around the edge; Out is a hint for which way the face should point
	void AddQuad(const FVector& A, const FVector& B, const FVector& C, const FVector& D, const FLinearColor& Color, const FVector& Out);
	void AddTri(const FVector& A, const FVector& B, const FVector& C, const FLinearColor& Color, const FVector& Out);
	// box centered at Center with full Size, rotated by Rot (meters)
	void AddBox(const FVector& Center, const FVector& Size, const FLinearColor& Color, const FQuat& Rot = FQuat::Identity);
	// a box with rounded edges and corners of radius R (meters), smooth-shaded; Seg segments per quarter round
	void AddRoundBox(const FVector& Center, const FVector& Size, double R, const FLinearColor& Color, const FQuat& Rot = FQuat::Identity, int32 Seg = 2);
	// tapered tube between two points
	void AddTube(const FVector& A, const FVector& B, double RA, double RB, int32 Sides, const FLinearColor& Color);
	// a smooth-shaded surface through a grid of points (NU across, NV along; P[v * NU + u]), sharing vertices so it
	// shades round; faces point away from Inside. bWrapU closes each row into a ring.
	void AddGrid(const TArray<FVector>& P, int32 NU, int32 NV, bool bWrapU, const FLinearColor& Color, const FVector& Inside);
	// a rounded bar swept along a path (Pts), its cross-section a superellipse HalfW across (toward Side) and HalfD deep
	void AddSweep(const TArray<FVector>& Pts, double HalfW, double HalfD, const FVector& Side, const FLinearColor& Color, int32 Sides = 14);

	void Commit(UProceduralMeshComponent* Mesh, int32 Section, UMaterialInterface* Mat) const;

	// For AKeysWorld::GlowMaterial: a soft blob seen from any side (three crossed squares), and a soft-edged
	// triangle whose brightness falls off across its far edge (Across 0 at B, 1 at C) and along it (alpha A0 -> A1)
	void AddGlowBlob(const FVector& C, double R, const FLinearColor& Color);
	void AddGlowFan(const FVector& A, const FVector& B, const FVector& Cc, const FLinearColor& Color, double A0, double A1);

	// Smooth shading: vertices at the same spot share an averaged normal, unless their faces meet at more than
	// MaxAngleDeg (so fins and hard chines keep their edge).
	void SmoothNormals(double MaxAngleDeg = 55);
	// the same, only for the vertices from index From on (one part of a mesh)
	void SmoothNormalsFrom(int32 From, double MaxAngleDeg);
};

namespace KeysMesh
{
	// ---------------------------------------------------------------- the lofted hull (web entities/boat/hull.ts)
	// hullStation / zRake / hullProfile / buildHullGeo ported line for line: deadrise that warps from the transom
	// to a deep-V entry, chine flats, flared topsides, sheer spring, a raked stem, steps in the running bottom,
	// boot stripe, cove stripe and rub rail. Web frame (x starboard, y up, z toward the stern).
	struct FHullDef
	{
		double F = 1.3, Spring = .35, YK = .5, Dr0 = 20, Dr1 = 48, Rake = 1.2, Entry = .42;
		TArray<double> Steps;
		FLinearColor Bottom = FLinearColor(FColor(0xf8, 0xf7, 0xf2)), Boot = FLinearColor(FColor(0x23, 0x46, 0x6e)), Hull = FLinearColor(FColor(0xfb, 0xfa, 0xf6)),
			Cove = FLinearColor(FColor(0x23, 0x46, 0x6e)), Rub = FLinearColor(FColor(0xd0, 0xd4, 0xd8)), Cap = FLinearColor(FColor(0xfb, 0xfa, 0xf6)),
			Liner = FLinearColor(FColor(0xf3, 0xf2, 0xec)), Deck = FLinearColor(FColor(0xe0, 0xde, 0xd6));
		bool bCove = true;
		bool bCat = false;     // a catamaran: two asymmetric sponsons under a bridge deck (the Freeman)
		double Tunnel = .42;   // the cat's tunnel height at the transom
		double TunnelRise = .5, SponW = .27, StemFoot = .6;   // the tunnel roof's rise to the bow, each sponson's width (of the beam), the stem's foot
		double MinBow = 0;     // the narrowest the deck gets at the bow (fraction of the half-beam): a cat's blunt twin bow
		double Tumble = 0;     // tumblehome at the stern: the topsides roll in toward the sheer
		double Flare = 0;      // extra flare forward: the sheer stands out past the chine sooner
	};
	struct FHullSta { double T, Z, Bb, Bs, YK, YC, YS, BF; };
	FHullSta HullStation(const FHullDef& H, double L, double B, double T);
	FHullSta HullStationAtZ(const FHullDef& H, double L, double B, double Z);   // web z (toward the stern)
	double HullZRake(const FHullDef& H, const FHullSta& S, double Y);
	double HullSole(double L, const FHullSta& S);                               // the cockpit sole's height
	// one hull: the loft, the transom and (bDeck) covering boards, liner and sole. Xo shifts it across (a sponson);
	// AsymSide (-1 / 1) is the half that faces a cat's tunnel, flattened by AsymK.
	void BuildHullLoft(FKeysMesh& M, const FHullDef& H, double L, double B, bool bDeck = true, double Xo = 0, int32 AsymSide = 0, double AsymK = 1);
	// a cat's sponsons and bridge deck, or the monohull
	void BuildHullFor(FKeysMesh& M, const FHullDef& H, double L, double B);
	// the web game's HULLS[id] for one of the eight boats (content/boats.ts)
	const FHullDef& HullOf(const KeysData::FBoatSpec& S);
	// deck shape for the fit-out, the crew and the camera, in the boat's own frame (meters, +X toward the bow)
	double SoleAt(const KeysData::FBoatSpec& S, double X);     // cockpit sole
	double SheerAt(const KeysData::FBoatSpec& S, double X);    // top of the gunwale
	double InnerHalfBeam(const KeysData::FBoatSpec& S, double X);   // half-width inside the covering boards
	double BowX(const KeysData::FBoatSpec& S);                 // the stem at the sheer
	double SternX(const KeysData::FBoatSpec& S);               // the transom

	// hull, deck, console, T-top or hardtop and outboards for one of the eight boats. bRiggers: the tower boats'
	// outriggers, stowed (the player's boat leaves them out and swings its own with BuildRiggers).
	void BuildBoat(FKeysMesh& M, const KeysData::FBoatSpec& S, bool bRiggers = true);
	// Outriggers (web entities/boat/model.ts OUTRIGGERS): the tower boats (Grady-White, Freeman) carry two long poles
	// on swivel bases that swing from stowed upright (Rig 0) to about 70 degrees out and raked aft (Rig 1). Hulls with
	// fixed rigger poles on the T-top (Yellowfin, Contender) report those. False on a boat with nothing to hang a line
	// from. Tips and bases are in the boat's own frame, meters.
	bool RiggerTips(const KeysData::FBoatSpec& S, double Rig, FVector OutTip[2], FVector OutBase[2]);
	bool HasSwingingRiggers(const KeysData::FBoatSpec& S);
	void BuildRiggers(FKeysMesh& M, const KeysData::FBoatSpec& S, double Rig);
	// Boats with a tuna tower (the Grady-White, and the Freeman when GFreemanTower is on): true, and where your eyes are
	// standing at the upper helm, in the boat's own frame (meters, +X toward the bow, Z up). The Freeman without its
	// tower gives the view from on top of its hardtop.
	bool TowerEye(const KeysData::FBoatSpec& S, FVector& OutEye);
	// the Freeman's tuna tower is an option (off by default, as in the owner's photos; -KRFreemanTower turns it on)
	extern bool GFreemanTower;
	// where the console, the helm and the top stand (boat frame, meters): MidX is the console's aft block, ConFwd how far
	// the console runs forward of it, TopX / TopL / TopW / TopZ the hardtop's centre, length, width and underside
	struct FBoatLayout { double MidX, DeckZ, ConL, ConW, ConFwd, TopX, TopL, TopW, TopZ; };
	FBoatLayout BoatLayout(const KeysData::FBoatSpec& S);
	// A big outboard (KeysBoatParts.cpp): a tall rounded cowl over its midsection, a dark panel on the aft face and an
	// accent stripe, the streamlined driveshaft housing, cavitation plate, gearcase torpedo, skeg and a three-blade
	// propeller. Mount is the clamp on the transom or bracket (its aft face, boat frame); the engine hangs aft of it.
	struct FOutboard
	{
		FVector Mount = FVector::ZeroVector;
		double W = .7, CavZ = -.1, TopZ = 1.4;
		FLinearColor Cowl = FLinearColor::White, Panel = FLinearColor::Black, Accent = FLinearColor::Black, Lower = FLinearColor::Black;
		uint8 CowlKind = SurfGelcoat;
	};
	void BuildOutboard(FKeysMesh& M, const FOutboard& O);
	// a contoured bucket seat facing Fwd (horizontal): a cushion with raised side bolsters, a tall back with wings and a
	// rounded top, quilted centre panels with dark piping. Base is the cushion's underside centre.
	void BuildBucketSeat(FKeysMesh& M, const FVector& Base, const FVector& Fwd, double W, const FLinearColor& Uph, uint8 InsertKind, double BackH = .72);
	// A stand-up tuna rod with a two-speed lever-drag reel (KeysRod.cpp): an aluminium gimbal butt, the reel seat and
	// its clamp, the reel (machined side plates with bolts, pillars, a spool packed with blue mono, the lever drag with a
	// knurled knob, the crank with its big rubber knob), a long black foam fore grip, a blue-and-silver cross-wrapped
	// butt section, a black blank with wrapped ring guides and a tip-top. Butt at the gimbal end, Dir along the rod, Up the
	// side the reel sits on, meters. Bend (meters at the tip) curves the blank under load. Lod 0 near, 1 far (no bolts,
	// fewer guides). Returns the tip.
	struct FRodPose
	{
		FVector Butt = FVector::ZeroVector, Dir = FVector::UpVector, Up = FVector::ForwardVector, Bend = FVector::ZeroVector;
		double Len = 1.9;
		int32 Lod = 0;
		bool bReel = true;
	};
	FVector BuildTunaRod(FKeysMesh& M, const FRodPose& P);
	// the level of detail BuildBoat gives the rods in its holders (0 for the player's boat, 1 for everyone else's)
	extern int32 GRodLod;
	// The Freeman's big-game fighting chair, centred in the cockpit aft of the console (KeysRod.cpp). The pedestal and
	// swivel base are part of the hull (BuildBoat); the chair itself swivels, so it is built each frame by AKeysExtras:
	// varnished teak seat, back and arms, the gimbal socket between the knees, the footrest bar, rod holders on the arms.
	// Yaw 0 faces the bow (the chair faces aft at rest, Yaw PI). ChairGimbal is the rod butt's socket, boat frame.
	bool HasFightChair(const KeysData::FBoatSpec& S);
	FVector FightChairBase(const KeysData::FBoatSpec& S);   // the top of the swivel, boat-local meters
	void BuildFightChairPedestal(FKeysMesh& M, const FVector& Base, double Sole);
	void BuildFightChair(FKeysMesh& M, const FVector& Base, double Yaw);
	FVector ChairGimbal(const FVector& Base, double Yaw);
	// a stainless flush-mount rod holder's flange and tube, set into a deck or gunwale top (Axis points out of the hole)
	void BuildRodHolder(FKeysMesh& M, const FVector& Mouth, const FVector& Axis, const FVector& SurfaceN);
	// a simple fish body with tail and dorsal fins, nose along +X, about Len meters long
	void BuildFish(FKeysMesh& M, double Len, const FLinearColor& Color, bool bBill);
	double FishLength(double WeightLb);
}
