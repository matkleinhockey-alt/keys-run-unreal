#pragma once

// The mangrove ecosystem's own working types, shared by KeysMangroveEco.cpp (the rules, the tiles, the links),
// KeysMangroveEcoBirds.cpp (the birds) and KeysMangroveEcoWater.cpp (what is in the water and on the mud).
#include "KeysMangroveEco.h"
#include "Async/Future.h"

class AKeysLife;

namespace KeysMangroveEco
{
	inline double EHash(double A, double B) { const double H = FMath::Sin(A * 127.1 + B * 311.7) * 43758.5453; return H - FMath::FloorToDouble(H); }
	// a site's own dice: the same tree gives the same answer every run (the fixed seed is the 17.3 / 5.7 pair)
	inline double SHash(const FSite& S, double Salt) { return EHash(S.X * 1.37 + Salt * 17.3, S.Z * .91 - Salt * 5.7); }
	inline double Hyp(double A, double B) { return FMath::Sqrt(A * A + B * B); }
	inline FVector G2U(const FVector& P) { return FVector(P.X * 100.0, P.Z * 100.0, P.Y * 100.0); }
	inline FVector D2U(const FVector& D) { return FVector(D.X, D.Z, D.Y); }
	inline double WrapPi(double A) { while (A > PI) A -= 2 * PI; while (A < -PI) A += 2 * PI; return A; }

	// one thing a tile's plan puts somewhere
	struct FSpot
	{
		uint8 Kind = 0;
		int16 N = 1, Rook = -1;
		FVector P = FVector::ZeroVector;           // where (y matters for a perch; the water's own level otherwise)
		FVector2D Out = FVector2D(1, 0);           // toward open water
		FVector2D Mouth = FVector2D::ZeroVector;   // where small fish from here go on the ebb
		FSite Site;
		float Depth = .4f;
		uint32 Seed = 0;
	};
	struct FPlan
	{
		int64 Key = 0;
		FVector2D C = FVector2D::ZeroVector;
		TArray<FSpot> Spots;
		TArray<FVector> Roosts;          // crown tops the day birds sleep on
		TArray<FVector2D> Wade;          // places off the roots a wader can stand
		int32 Count[NumKinds] = {};
		int32 Sites = 0;
	};
	// the rule: everything that lives in a tile, from the map alone (any thread)
	void PlanTile(int64 Key, FPlan& Out);
	FVector2D Outward(double X, double Z);
	int32 RookAt(double X, double Z);
	// the way a rookery's flights go and the side to look at it from: toward the most open water round it (game thread)
	FVector2D RookSeaDir(int32 R);

	enum EBirdState : uint8 { BStand, BWalk, BStrike, BFly, BPerch, BDry, BRoost, BHover, BDive, BSwim, BAway, BPatrol };
	struct FBird
	{
		uint8 Kind = 0, State = BStand, Next = BStand;
		int16 Rook = -1;
		int32 Leader = -1;
		bool bStill = false, bCarry = false, bHasSite = false, bWasStill = false, bInStill = false, bLine = false;   // bInStill: drawn in the still mesh as it was last laid out
		FVector P = FVector::ZeroVector, Home = FVector::ZeroVector, Perch = FVector::ZeroVector, From = FVector::ZeroVector, To = FVector::ZeroVector;
		FVector2D Out = FVector2D(1, 0), Off = FVector2D::ZeroVector;
		double Yaw = 0, T = 0, Ph = 0, FlyT = 0, FlyDur = 1, Arc = 2, Sz = 1.25, Neck = 0, Wing = 0, Pitch = 0, Peck = 0;
		double Think = 0, FlushCD = 0, Delay = 0, Rank = 0, FlapHz = 3;
		int32 Prey = -1, Strikes = 0, Shower = 0, RoostIx = -1;
		double Move = 60, Cool = 0;
		uint32 Seed = 0;
		FSite Site;
	};

	struct FFishE
	{
		FVector P = FVector::ZeroVector, V = FVector::ZeroVector, Slot = FVector::ZeroVector, Fwd = FVector(1, 0, 0);
		float Len = .1f, Ph = 0, Shade = 1, Seed = 0, Air = 0, Flash = 0;
	};
	struct FSchoolE
	{
		uint8 Kind = 0;
		int32 Id = 0, N = 0;
		uint32 Seed = 0;
		FVector2D Home = FVector2D::ZeroVector, Mouth = FVector2D::ZeroVector, C = FVector2D::ZeroVector, Out = FVector2D(1, 0), ThreatP = FVector2D::ZeroVector;
		double Th = 0, Scatter = 0, HitT = -1e9, R = 1, Y = -.2, Floor = -.4, Mill = 0;
		bool bOut = false, bAwake = false, bBank = false;
		TArray<FFishE> Fish;
	};
	struct FSwim
	{
		FVector P = FVector::ZeroVector, V = FVector::ZeroVector, Fwd = FVector(1, 0, 0);
		FVector2D Off = FVector2D::ZeroVector;
		double Ph = 0, Air = 0, Len = 1, T = 0, Roll = 0;
	};
	enum EGroupState : uint8 { GHold, GRush, GReturn, GFlee };
	struct FGroup
	{
		uint8 Kind = 0, State = GHold;
		uint32 Seed = 0;
		FVector2D Home = FVector2D::ZeroVector, C = FVector2D::ZeroVector, Out = FVector2D(1, 0), Aim = FVector2D::ZeroVector;
		double T = 0, Hunger = 30, Yaw = 0, Depth = 1, FleeT = 0, Wander = 0, RushD = 0, Spd = 0;
		int32 Prey = -1;
		bool bBank = false, bActive = false;
		TArray<FSwim> M;
	};
	struct FCrabs
	{
		uint8 Kind = 0;
		FSite Site;
		FVector2D C = FVector2D::ZeroVector, Out = FVector2D(1, 0);
		int32 N = 1;
		uint32 Seed = 0;
		double Hide = 0, T = 0, FloorY = 0;   // Hide 0 out .. 1 gone (down a burrow, or dropped off the root)
		bool bGone = false;
	};
	struct FTileE
	{
		int64 Key = 0;
		FVector2D C = FVector2D::ZeroVector;
		TArray<FBird> Birds;
		TArray<FSchoolE> Schools;
		TArray<FGroup> Groups;
		TArray<FCrabs> Crabs;
		TArray<FVector> Roosts;
		TArray<bool> RoostUsed;
		TArray<FVector2D> Wade;
		TArray<FSpot> Haunts;
		TArray<bool> Armed;
		int32 Count[NumKinds] = {};
	};
	// bait showering at the surface: who drove it up, and who came to it
	struct FShower
	{
		int32 Id = 0;
		FVector2D P = FVector2D::ZeroVector;
		double T = 0;
		uint8 Prey = 0, Hunter = 0;
		int32 Terns = 0, Pelicans = 0, Others = 0;
		bool bTold = false;
	};
	struct FSoundEv { FName What; int32 Kind = 0; FVector Where = FVector::ZeroVector; float Power = 1; };
	struct FSoup
	{
		TArray<FVector> V, N;
		TArray<int32> T;
		TArray<FLinearColor> C;
		int32 LastV = -1;
		void Reset() { V.Reset(); N.Reset(); T.Reset(); C.Reset(); }
	};
	struct FPoolE
	{
		TArray<FTransform> Xf;
		TArray<float> Data;   // 8 per instance (M_KR_Boid)
		int32 Shown = 0, Tris = 0;
		bool bTried = false;
	};
	constexpr int32 NumPools = Horseshoe - Silverside + 1;
	enum ELink { LinkRush, LinkBirds, LinkWaders, LinkTide, LinkNight, LinkFlush, LinkOsprey, NumLinks };

	struct FCensus
	{
		int32 Tiles = 0, Sites = 0;
		int32 Count[4][NumKinds] = {};
		TArray<int32> RookBirds;
		FVector2D Example[4][NumKinds];
		FVector2D ExampleOut[4][NumKinds];
		bool bHave[4][NumKinds] = {};
		double Secs = 0;
	};

	struct FEco
	{
		TMap<int64, FTileE*> Tiles;
		TMap<int64, FVector2D> Have;
		TFuture<FPlan*> Job;
		bool bJob = false;
		int64 JobKey = 0;
		double ScanT = 0;
		TFuture<FCensus*> CensusJob;
		bool bCensusJob = false;
		TUniquePtr<FCensus> Census;
		// the viewer and the world this frame
		FVector Cam = FVector::ZeroVector;
		FVector2D Boat = FVector2D::ZeroVector, BoatF = FVector2D(0, -1);
		double BoatSpd = 0, Now = 0, Dt = 0, Sea = 0, Tide = 0, Hour = 12, Night = 0, HomeFrac = .3;
		int32 Phase = 2;   // 0 night, 1 dawn, 2 day, 3 dusk
		bool bHaveBoat = false, bDiver = false;
		FVector Diver = FVector::ZeroVector;
		AKeysLife* Life = nullptr;
		// drawing
		FSoup Live, Still;
		bool bStillDirty = true, bStillSoon = false;   // lay the still mesh out again now; or within a second (a far bird changed)
		double StillT = 0;
		FVector StillCam = FVector::ZeroVector;
		FPoolE Pools[NumPools];
		// the links
		TArray<FShower> Showers;
		int32 NextShower = 1, NextSchool = 1;
		double LinkT[NumLinks] = {};
		int32 LinkN[NumLinks] = {};
		int32 LastPhase = -1, LastTideSide = 0;
		TArray<double> RookCD;
		// flushes gathered over a second, then told as one line
		int32 FlushN[NumBirds] = {};
		double FlushT = 0, FlushD = 0, FlushSpd = 0;
		// counters for the log
		double CpuAcc = 0;
		int32 CpuN = 0, StillN = 0, BirdsDrawn = 0, BirdsMoved = 0, FishDrawn = 0, SchoolsAwake = 0;
		bool bScript = false;
		// the test camera (kr.eco look ...): what it is held on, from how far and how high
		TWeakObjectPtr<AActor> LookCam;
		FString LookName;
		FVector LookAt = FVector::ZeroVector;
		FVector2D LookDir = FVector2D(1, 0);
		double LookDist = 6, LookH = 1.2, LookSide = 0;
		bool bLook = false;
		// the last wader's strike (for the test camera), and the stills the test camera takes of an event by itself
		FVector StrikeAt = FVector::ZeroVector;
		FVector2D StrikeDir = FVector2D(1, 0);
		int32 StrikeId = 0, LookEvent = 0, LookShots = 0;
		double LookShotT = 0;
		// stills of an event taken from the player's own view (kr.eco jump, kr.eco shower): how many are left, when the
		// next is due, what they are called; armed: they start at the next shower of bait
		int32 EvShots = 0, EvShowerId = 0;
		double EvShotT = 0;
		FString EvName;
		bool bEvArmed = false;
		// what made a noise this frame, for the audio to take (PopSound); only what is within earshot is kept
		TArray<FSoundEv> Sounds;
		void Sound(const TCHAR* What, int32 Kind, const FVector& Where, float Power = 1)
		{
			if (Sounds.Num() < 24 && FVector::DistSquared(Where, Cam) < 250.0 * 250.0) Sounds.Add({FName(What), Kind, Where, Power});
		}
	};

	// ---- KeysMangroveEcoBirds.cpp
	void SpawnBirds(FEco& E, FTileE& T, const FPlan& P);
	void UpdateBirds(FEco& E, FTileE& T);
	void EmitBirds(FEco& E, FTileE& T, bool bStillPass);
	void TickRookeries(FEco& E);
	// how much larger than life a kind is drawn; one bird's triangles as it is drawn (Rim: the width in metres of its
	// dark edge, 0 for none); a kind in the pose it is mostly seen in, at the origin
	double BirdScale(int32 Kind);
	bool BirdsBefore();
	void BirdSoup(FSoup& Buf, const FBird& B, bool bFar, double Rim);
	void PoseBird(FBird& B, int32 Kind);
	void FlyPast(FBird& B, const FVector& To, double Delay);
	// ---- KeysMangroveEcoView.cpp
	void CloserCmd(const FString& Arg);
	void PxReport(FEco& E, UWorld* World, const FString& Tag);
	void AfloatReport(FEco& E, UWorld* World, const FString& Tag);
	// ---- KeysMangroveEcoWater.cpp
	void SpawnWater(FEco& E, FTileE& T, const FPlan& P);
	void UpdateWater(FEco& E, FTileE& T);
	void FlushPools(class AKeysMangroveEco* A, FEco& E);
	// what shows of them at the surface from a boat: a school's dark patch and flashes, a wake (into the moving mesh)
	void EmitWaterMarks(FEco& E, FTileE& T);
	// the nearest school of small fish to a point (within R): where it is now and how many it holds
	FSchoolE* NearestSchool(FEco& E, const FVector2D& P, double R, double* OutD = nullptr);
	FSchoolE* FindSchool(FEco& E, int32 Id);
	int32 BaitNear(FEco& E, const FVector2D& P, double R);
	FGroup* NearestGroup(FEco& E, const FVector2D& P, double R, int32 Kind);
	// ---- KeysMangroveEco.cpp
	// the water at a point: the bottom's height (game y), and whether it is water at all
	double FloorAt(double X, double Z);
	bool Wet(double X, double Z);
	// one line for a link the player can watch, no more often than every Every seconds
	bool LinkDue(FEco& E, int32 Link, double Every);
	const TCHAR* TideWord(const FEco& E);
	const TCHAR* PhaseWord(const FEco& E);
}
