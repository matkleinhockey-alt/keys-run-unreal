#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "KeysGeo.h"
#include "KeysData.h"
#include "KeysDive.h"
#include "KeysLife.generated.h"

class UProceduralMeshComponent;
struct FKeysMesh;
struct FKeysVis;

// Where the bobber landed: mirrors F.zone / F.hs in the JS (index.html 2750-2752).
struct FKeysFishSpot
{
	FString Zone;           // ZoneAt(), or "Weedline" / "Oil Rig"
	int32 HotspotId = -1;   // the bird pile you cast into (-1 if none, or if a rig overrides it)
	bool bHot = false;      // any school: bird pile, weedline or rig (JS: F.hs != null)
	bool bFrenzy = false;   // the bird pile is feeding right now (JS: F.hs.fr > .5)
};

// a tuna that ran down the bait (JS tunaStrike, index.html 2520)
struct FKeysTunaStrike
{
	FString Key;
	double FX = 0, FZ = 0;
};

// chart markers: 0 oil rig, 1 weedline (a line Len long at angle Ang), 2 bird pile, 3 wreck (Len long, heading Ang)
struct FKeysMarker
{
	uint8 Kind = 0;
	double X = 0, Z = 0, Ang = 0, Len = 0, Frenzy = 0;
};

// CPU-built triangle soup for one procedural mesh section, rebuilt every frame
struct FKeysLifeBuf
{
	TArray<FVector> V, N;
	TArray<int32> T;
	TArray<FVector2D> UV;
	TArray<FLinearColor> C;
	uint32 Sig = 0, LastSig = 0;
	int32 LastV = -1;
	void Reset() { V.Reset(); N.Reset(); T.Reset(); UV.Reset(); C.Reset(); Sig = 0; }
};

struct FKeysBird
{
	uint8 Kind = 0;          // 0 gull, 1 tern, 2 frigatebird, 3 brown pelican
	double Scale = 1, Ph = 0;
	bool bPrev = false, bVis = false;
	double PX = 0, PY = 0, PZ = 0, Yaw = 0, Roll = 0, Pitch = 0;
	double Up = .12, Hand = .05, Tuck = 0;
	// circling a bird pile or a weedline (relative to its center)
	double R = 0, H = 0, Sp = 0, Ph2 = 0;
	// a plunge dive
	bool bDive = false, bSpl = false, bWX = false;
	double DT = 0, DX = 0, DZ = 0, WX = 0, WZ = 0;
	double DownUntil = -1;   // shot down or scared off by the pistol: gone until then (web seabirds.ts scareBird / shootBird)
};

// a bird the boat pistol can see (web life.shootableBirds)
struct FKeysBirdTarget { FKeysBird* Ref = nullptr; FVector Pos = FVector::ZeroVector; uint8 Kind = 0; };

struct FKeysWeedBit { double OX, OZ, S, Rot; int32 Col; };

struct FKeysHotspot
{
	int32 Id = 0;
	FString Zone;
	double X = 0, Z = 0, Amp = 1;
	int32 Fish = 2;
	double FT = 0, Fr = 0;
	bool bActive = false, bTold = false;
	TArray<FKeysBird> Birds;
	TArray<FVector4> Weeds;   // offshore piles carry a little weed: (x, z, yaw, length) plus width in WeedW
	TArray<double> WeedW;
	int32 GrpId = -1;
};

struct FKeysWeedline
{
	double X = 0, Z = 0, Ang = 0, Len = 0, Wid = 0, Amp = 1;
	TArray<FKeysWeedBit> Bits;
	TArray<FKeysBird> Birds;
	bool bTold = false;
	int32 GrpId = -1;
};

struct FKeysRig
{
	double X = 0, Z = 0, Rot = 0;
	bool bTwin = false, bTold = false;
	int32 MeshIndex = -1;
	int32 GrpId = -1;
};

struct FKeysPelicanFlock
{
	double X = 0, Z = 0, H = 0, Sp = 10, Alt = 3, FlapT = 0, Turn = 0;
	bool bVee = false;
	TArray<FKeysBird> Birds;
};

struct FKeysSwimmer
{
	double OX = 0, OZ = 0, OY = 0, Ph = 0, S = 1, Shade = 1, SizeU = .5;
	uint8 Act = 0;   // 0 none, else EAct
	double AP = 0, ActT = 0;
	bool bSpl = false;
	// pose this frame (game meters)
	double WX = 0, WY = 0, WZ = 0, Pitch = 0, Roll = 0, Yaw = 0, Sz = 1;
};

struct FKeysSchool
{
	int32 Id = 0;
	FString Type;
	const FKeysVis* V = nullptr;
	double CX = 0, CZ = 0, H = 0, Ph = 0, Turn = 0, Flee = 0, FleeH = 0, Glide = 0, KeepT = 0, FleeMul = 0;
	bool bChased = false;
	double DvT = -1, DvTarget = 0, Dv = 0;
	double Cur = 0;   // 0..1: how much it has come in to look at the diver (draws it toward the diver's depth)
	uint8 AnchorKind = 0;   // 0 none, 1 rig, 2 bird pile, 3 weedline, 4 a fixed spot (the diver's near-field schools)
	double AnchorX = 0, AnchorZ = 0;
	int64 NearCell = 0;   // the 18 m cell a near-field school belongs to (0 for none)
	int32 AnchorRef = -1;   // rig index, hotspot id, weedline index
	double AnchorR = 10;
	TArray<FKeysSwimmer> Members;
};

struct FKeysDrop
{
	FVector P, Vel;   // game meters
	double Age = 0, Life = 1, Size = .2;
	bool bGrav = true;
};

// The living sea: pelicans and seabirds, bird piles over feeding fish, sargassum weedlines, offshore oil rigs,
// and the schools of fish, sharks, rays, turtles, manatees and dolphins you can see from the boat.
// Ported from index.html ~1884-2604. It also owns the fishing bonuses those places give (see the "fishing hooks" below).
UCLASS()
class KEYSRUN_API AKeysLife : public AActor
{
	GENERATED_BODY()

public:
	AKeysLife();
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

	static AKeysLife* Get(const UObject* WorldContext);

	// ---- fishing hooks, called by the pawn's fishing code
	// what the bobber landed in (JS updateFishing, index.html 2750-2752)
	FKeysFishSpot SpotAt(double X, double Z) const;
	bool NearHotspot(double X, double Z, double R = 50) const;
	bool NearWeed(double X, double Z, double Extra = 25) const;
	bool NearRig(double X, double Z, double R = 60) const;
	// the zone a cast at (X, Z) fishes: "Oil Rig", "Weedline" or ZoneAt()
	FString SpeciesBias(double X, double Z) const;
	// multiply the first bite timer by this (JS 2753)
	static double BiteTimeScale(const FKeysFishSpot& S) { return (S.bHot ? .4 : 1.0) * (S.bFrenzy ? .5 : 1.0); }
	// multiply the re-bite timer after a miss by this (JS missFish, 2661)
	static double MissBiteScale(const FKeysFishSpot& S) { return S.bHot ? .5 : 1.0; }
	// multiply the size roll inside Min(1, ...) by this (JS chooseFish, 2657)
	static double WeightBoost(const FKeysFishSpot& S) { return S.bHot ? 1.2 : 1.0; }
	// species table for a zone, including the "Oil Rig" and "Weedline" tables KeysData doesn't have
	static const TArray<KeysData::FZoneRow>& FishTable(const FString& Zone);
	// full port of JS pickSpecies (2650): offshore size bias, hump bonus, and the school bonus for rare fish
	FString PickSpecies(const FKeysFishSpot& S, double BobX, double BobZ) const;
	// a cast landed: offshore it may bring a tuna school in from 200 m away (JS 2754)
	void OnCastLanded(const FKeysFishSpot& S, double BobX, double BobZ);
	// a fish was landed: a bird pile runs out of fish and moves on (JS land, 2704)
	void OnCatchLanded(const FKeysFishSpot& S);
	// every tick, before PollTunaStrike: is the bait sitting offshore with no fish chosen yet? false also drops a stale strike (JS 2552)
	void SetBait(bool bActive, double BobX, double BobZ);
	// a tuna school is charging the bait: keep BiteT >= 5 so the tuna gets there first (JS 2554)
	bool TunaClosing() const { return bTunaClosing; }
	// a tuna hit the bait: returns true once (JS tunaStrike, 2520)
	bool PollTunaStrike(FKeysTunaStrike& Out);
	// tuna weight roll from tunaStrike (2525)
	static double TunaWeight(const KeysData::FSpecies& S, double BobX, double BobZ);
	// toasts for the pawn to show ("Oil rig ahead", "hot spot", "birds moved on" ...); empty if none
	FString PopToast();
	// helm zone line override (JS 3759): "Oil Rig", "Weedline", "Feeding frenzy!" or empty
	FString AreaLabel(double X, double Z) const;
	void GetMarkers(TArray<FKeysMarker>& Out) const;
	// rig legs at the waterline, for boat collisions (the JS adds these to the piling grid)
	const TArray<KeysGeo::FPillar>& RigPillars() const { return RigLegs; }
	// visible fish near a point, for a fish finder: positions in game meters and body lengths
	void FishNear(double X, double Z, double R, TArray<FVector>& OutPos, TArray<double>& OutLen) const;
	// the real species model (nose along +X, meters) for the hooked fish; false if the key has no model
	static bool BuildCreatureMesh(const FString& Key, FKeysMesh& Out, double Scale = 1.35);
	// the same close-up, its body bent by the tail beat (the swimmers' own weights): Phase in radians, Amp 1 = swimming
	static bool BuildCreatureMeshBent(const FString& Key, FKeysMesh& Out, double Scale, double Phase, double Amp);
	// a cheaper body for the instanced school fish (KeysSchools): Lod 1 near, 2 far; same frame and units as BuildCreatureMesh
	static bool BuildSchoolMesh(const FString& Key, FKeysMesh& Out, int32 Lod);
	// sharks and the fish that hunt bait (barracuda, jacks, tarpon, mackerels...) near a point: (x, y, z, length), game meters
	void PredatorsNear(double X, double Z, double R, TArray<FVector4>& Out) const;
	// a burst of spray at a point on the water
	void Splash(double X, double Z, int32 N, double Power);
	// water streaming off a leaping fish: N drops at P (game meters, Y up) flung with the fish's velocity V
	void Shed(const FVector& P, const FVector& V, int32 N, double Size);

	// ---- diving (web game entities/fish: the diver as a threat, the near-field schools around a diver, spear targets)
	// the diver is in the water at (X, Z), moving RelSpeed m/s through it; schools near it fill in and stand off
	void SetDiver(bool bOn, double X, double Z, double RelSpeed, double Y = -3);
	// every catchable fish within R of a point, as a capsule the spear can hit
	void SpearTargetsNear(double X, double Y, double Z, double R, TArray<KeysDive::FTarget>& Out) const;
	// a fish was speared: it leaves its school
	void TakeSpeared(int32 TargetId);
	// a shaft hissed past here: schools nearby bolt
	void SpookAt(double X, double Z);
	// ---- the boat pistol (web sim/handgun.ts, seabirds.ts): the seabirds in range, and what a shot does to one
	void ShootableBirds(double X, double Z, double R, TArray<FKeysBirdTarget>& Out);
	void DownBird(FKeysBird* B, double Secs);
	// test hook: bring the nearest spearable school to (X, Z) and keep it there
	void DebugBringSchool(double X, double Z, const FString& Type = FString(), double Dist = 0);

	// test hook: a school of that kind there now, past the population cap
	bool DebugSpawn(const FString& Type, double X, double Z);
private:
	void SpawnHotspot(const FString& Zone);
	void BuildWeeds();
	void BuildRigs();
	void InitPelicans();
	void SpawnFlock(FKeysPelicanFlock& F, bool bNear);
	void UpdatePelicans(double Dt, double T);
	void UpdateHotspots(double Dt, double T);
	void UpdateWeeds(double Dt, double T);
	void UpdateRigs(double Dt, double T);
	void UpdateCreatures(double Dt, double T);
	void UpdateDrops(double Dt, double T);
	void ManagePopulation(double RMin);
	void TrySpawn(double RMin);
	FKeysSchool* SpawnGroupAt(const FString& Type, double X, double Z, double H = 1e9, bool bIgnoreCap = false);
	void SpawnTunaNear(double X, double Z);
	void EnsureAnchors();
	bool AnchorPos(const FKeysSchool& G, double& OX, double& OZ) const;
	bool HasGroup(int32 Id) const;
	int32 FreeSlots(const FKeysVis* V) const;
	void FreeGroup(FKeysSchool& G);
	void SpawnDrop(const FVector& P, const FVector& V, double Life, double Size, bool bGrav);
	void Toast(const FString& Msg);
	const FKeysHotspot* FindHotspot(int32 Id) const;
	const FKeysWeedline* NearWeedline(double X, double Z, double Extra) const;
	const FKeysRig* NearRigPtr(double X, double Z, double R) const;
	void PoseBird(FKeysBird& B, double Dt, double Flap, double Tuck, double Rate);
	void MoveBird(FKeysBird& B, double X, double Y, double Z, double Dt);
	void EmitBird(const FKeysBird& B);
	void Flush(UProceduralMeshComponent* M, FKeysLifeBuf& B, uint8 Kind = 0);
	void ReadViewer(double Dt);
	void UpdateNearField();

	UPROPERTY() TObjectPtr<USceneComponent> Root;
	UPROPERTY() TObjectPtr<UProceduralMeshComponent> BirdMesh;
	UPROPERTY() TObjectPtr<UProceduralMeshComponent> SeaMesh;
	UPROPERTY() TObjectPtr<UProceduralMeshComponent> WeedMesh;
	UPROPERTY() TObjectPtr<UProceduralMeshComponent> DropMesh;
	UPROPERTY() TArray<TObjectPtr<UProceduralMeshComponent>> RigMeshes;
	UPROPERTY() TArray<TObjectPtr<UProceduralMeshComponent>> FlameMeshes;

	TArray<FKeysHotspot> Hotspots;
	TArray<FKeysWeedline> Weeds;
	TArray<FKeysRig> Rigs;
public:
	// the I-th rig (not counting the twins), game meters
	bool RigAt(int32 I, double& OutX, double& OutZ) const { int32 N = 0; for (const FKeysRig& R : Rigs) if (!R.bTwin && N++ == I) { OutX = R.X; OutZ = R.Z; return true; } return false; }
private:
	TArray<KeysGeo::FPillar> RigLegs;
	TArray<FKeysPelicanFlock> Pelicans;
	TArray<FKeysSchool> Schools;
	TMap<const FKeysVis*, int32> Used;
	TArray<FKeysDrop> Drops;
	TArray<FString> Toasts;

	FKeysLifeBuf BirdBuf, SeaBuf, WeedBuf, DropBuf;

	int32 NextId = 1;
	double PopT = 0, NowT = 0;
	double BoatX = 0, BoatZ = 0, BoatSpd = 0, CamX = 0, CamZ = 0, CamY = 2;
	bool bHaveBoat = false;
	bool bBait = false, bTunaClosing = false, bStrikePending = false;
	double BaitX = 0, BaitZ = 0;
	FKeysTunaStrike Pending;
	bool bDiver = false;
	double DiverX = 0, DiverZ = 0, DiverSpeed = 0, DiverY = -3;
	TArray<FVector2D> Spooks;
	TMap<int64, int32> NearCells;   // 18 m cell -> school id
	double NearT = 0;
};
