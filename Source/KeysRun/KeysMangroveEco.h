#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "KeysMangroveEco.generated.h"

class UProceduralMeshComponent;
class UInstancedStaticMeshComponent;
class UStaticMesh;
class UMaterialInstanceDynamic;

// The mangrove ecosystem of the real map (KeysMangroveEco*.cpp): what lives on, under and over the red mangroves of
// every shore, placed from the map by rule with a fixed seed and tied to the tide, the hour and each other.
namespace KeysMangroveEco
{
	// on the real map, unless -KRNoMangroveEco (which also puts the old still waders back into the mangrove mesh)
	bool Enabled();
	// the life in and on the roots (the rootlife lane): thick schools in the roots, growth on the roots below the tide
	// line, snook and gray snapper where a diver can see them. -KRNoRootLife leaves all of it out (the ecosystem as it
	// was, oyster lumps in the tile mesh and all)
	bool RootLife();
	struct FEco;

	enum EKind : uint8
	{
		// birds
		GreatEgret, GreatBlue, LittleBlue, Ibis, Spoonbill, Cormorant, Anhinga, Kingfisher, GreenHeron, NightHeron, Tern, Osprey, Pelican, WhitePelican,   // (Pelican: the brown pelican)
		// under the roots
		Silverside, Pilchard, Snapper, Grunt, Schoolmaster, LaneSnapper, Sergeant,   // (the schools: Silverside..Sergeant)
		Needlefish, Mullet, Barracuda, LemonShark, NurseShark, Stingray, Jelly, Snook, Jack, Tarpon, GraySnapper,   // (GraySnapper: the adults)
		// on the roots and the mud
		TreeCrab, Fiddler, Horseshoe,
		// asked of KeysLife, and built into the mangrove tile
		Manatee, PelicanRoost,
		NumKinds
	};
	constexpr int32 NumBirds = WhitePelican + 1;
	const TCHAR* KindName(int32 Kind);
	// 0 Lower Keys, 1 Middle Keys, 2 Upper Keys, 3 Key Largo (by which phase of the map the chain line is in at that x)
	int32 RegionAt(double X);
	const TCHAR* RegionName(int32 Region);

	// a mangrove with its feet in the water, as KeysMangrove.cpp stands it (game metres)
	struct FSite
	{
		double X = 0, Z = 0, Ground = 0, Sz = 1, Hh = 1.5;
		uint8 Detail = 1, Bank = 0;   // Bank: a creek's bank tree; then In points at the channel, (CX, CZ) is its centre line there
		bool bRoof = false;
		bool bThird = true;   // a bank tree among the one in three the older rules use (SitesOfTile with bAllBank)
		FVector2D In = FVector2D::ZeroVector;
		double MouthX = 0, MouthZ = 0, Along = 0, CX = 0, CZ = 0;
		FVector Top = FVector::ZeroVector;   // the top of its crown (not for a bank tree, which leans)
	};
	// KeysMangrove.cpp: the trees of a tile and the crown tops a bird can stand on; a point on one of a tree's prop roots;
	// every tile that has mangrove (game thread)
	void SitesOfTile(int64 Key, TArray<FSite>& Sites, TArray<FVector>& Tops, bool bAllBank = false);
	FVector RootPoint(const FSite& S, int32 Root, double U);
	// every prop root of that tree as it is drawn: the straight pieces, each from A to B, R thick (game metres)
	struct FRootSeg { FVector A, B; float R; };
	void RootPieces(const FSite& S, TArray<FRootSeg>& Out);
	void MangroveTiles(TArray<TPair<int64, FVector2D>>& Out);
	// rules the mangrove builder shares with the ecosystem: the bare low limb a cormorant or anhinga dries its wings on
	// (from A out to B), and the dead snag where a tile's pelicans roost (its foot; false when the tile has none)
	bool PerchLimb(const FSite& S, FVector& A, FVector& B);
	bool RoostSnag(int64 Key, const TArray<FSite>& Sites, FVector& Foot, double& Seed);

	// a rookery key: a small wild mangrove island that holds many birds (chosen by rule from the map's islands)
	struct FRookery { FVector2D C; double R = 60; int32 Region = 0, Island = -1; };
	const TArray<FRookery>& Rookeries();

	// ---- the fishing hook (request R1: KeysLife::PickSpecies and the pawn's bite timer). Both are 1 with no ecosystem.
	// how much likelier that species is where you can see it lying (snook under the roots, a tarpon rolling, jacks)
	double PickBoost(const FString& Key, double X, double Z);
	// multiply the wait for a bite by this: shorter where the small fish are thick
	double BiteScale(double X, double Z);

	// ---- the sound hook (request R2: AKeysAudio polls this each tick and plays what it has a voice for). What: one
	// of "shower", "plunge", "jump", "roll", "flush", "strike", "crabdrop", "rattle" (kingfisher), "croak" (heron),
	// "honk" (ibis), "kree" (tern), "whistle" (osprey), "quok" (night heron), "flight" (a rookery flight's wings);
	// Kind is the animal (EKind), Where in game metres, Power about 0..1. False when there is nothing waiting.
	bool PopSound(FName& What, int32& Kind, FVector& Where, float& Power);

	// ---- the closer look (request R1 of the birdview lane: the chase view in AKeysBoatPawn::UpdateCamera asks this each
	// frame, and UpdateJumpCam asks CloserFov for the lens). Idling or stopped (under about 3 kn) with the animals of
	// a mangrove shore or a rookery key in reach, the camera comes in, down and round to the far side of the boat from
	// them and the lens closes up; it lets go above about 5 kn, when bBusy (a line out, a fish on, a menu), and the
	// moment the player moves the camera himself (CamYaw, CamPitchOff or CamZoom change under his hand), until she has
	// been under way again. With no ecosystem, away from mangroves, or not asked this frame: K 0, Dist 1, Ht 1, Yaw 0
	// and a lens of 88, which leave the chase view exactly as it is.
	struct FCloser
	{
		double K = 0;                          // how far in, 0..1: blend the chase view's look point toward Look by this
		double Dist = 1, Ht = 1;               // multiply the chase camera's distance and its height by these
		double Yaw = 0;                        // add to the angle the camera sits at round the boat (H + CamYaw)
		FVector Look = FVector::ZeroVector;    // what to look at (game metres)
		float Fov = 88.f;
	};
	FCloser CloserLook(double X, double Z, double H, double Speed, bool bBusy, double CamYaw, double CamPitchOff, double CamZoom, double BaseDist, double BaseHt, double Dt);
	float CloserFov();
}

// Runs the ecosystem round the boat: plans a mangrove tile's animals as she comes within 900 m (off the game thread,
// from the map alone), moves the ones within 600 m of the camera, draws the birds as one mesh and everything in the
// water as instances, and lets it all go 1.3 km behind her. Test hooks are console commands (script: con=kr.eco ...):
// stats, census, fish, links, flush, shower, rook, goto <kind> [n], gorook <n>.
UCLASS()
class KEYSRUN_API AKeysMangroveEco : public AActor
{
	GENERATED_BODY()

public:
	AKeysMangroveEco();
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;
	virtual void Tick(float DeltaSeconds) override;

	static AKeysMangroveEco* Get();
	void DebugCmd(const FString& Cmd);
	KeysMangroveEco::FEco* State() const { return E; }

	UPROPERTY() TObjectPtr<USceneComponent> Root;
	UPROPERTY() TObjectPtr<UProceduralMeshComponent> LiveMesh;    // the birds that are moving, rebuilt every frame
	UPROPERTY() TObjectPtr<UProceduralMeshComponent> StillMesh;   // the ones standing or far off, rebuilt when one changes
	UPROPERTY() TArray<TObjectPtr<UInstancedStaticMeshComponent>> Pools;
	UPROPERTY() TArray<TObjectPtr<UStaticMesh>> Meshes;
	UPROPERTY() TArray<TObjectPtr<UInstancedStaticMeshComponent>> GrowPools;   // (root life) what grows on the roots near the camera
	UPROPERTY() TObjectPtr<UMaterialInstanceDynamic> MID;

private:
	KeysMangroveEco::FEco* E = nullptr;
};
