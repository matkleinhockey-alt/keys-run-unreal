// Reef life: the actor. Placement by chunk, the six behaviour roles, instanced drawing, effects, sound, test hooks.
// The registry, HabitatAt and the body builder are in KeysReefLifeBody.cpp; species are in their own files.
#include "KeysReefLife.h"
#include "KeysGeo.h"
#include "KeysSea.h"
#include "KeysOcean.h"
#include "KeysMesh.h"
#include "KeysReef.h"
#include "KeysWrecks.h"
#include "KeysDiver.h"
#include "KeysBoatPawn.h"
#include "KeysAudio.h"
#include "KeysFeed.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "ProceduralMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "StaticMeshAttributes.h"
#include "MeshDescription.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Kismet/GameplayStatics.h"
#include "Camera/PlayerCameraManager.h"
#include "EngineUtils.h"
#include "HAL/PlatformTime.h"
#include "Misc/CommandLine.h"

namespace KeysReefLife
{
	constexpr double ChunkSize = 32, LoadR = 64, DropR = 84;
	constexpr int32 ChunkPoints = 16;
	constexpr double Dawn = 6.0, Dusk = 19.25;          // the feeding clock's day, for a fish that sleeps at night
	constexpr double BiteHearR = 9;                     // a bite is played when the diver is this near (m)
	constexpr double RlBoundsExt = 40;

	enum EState : uint8 { SRove, SToBite, SBite, SFlee, SChase, SChased, SToSleep, SSleep, SHop, SBreathe, SDig };
	static const TCHAR* StateName(uint8 S)
	{
		static const TCHAR* N[] = {TEXT("roving"), TEXT("going to bite"), TEXT("biting"), TEXT("fleeing"), TEXT("chasing"), TEXT("chased"), TEXT("going to sleep"), TEXT("asleep"), TEXT("hopping"), TEXT("up for air"), TEXT("digging")};
		return S < UE_ARRAY_COUNT(N) ? N[S] : TEXT("?");
	}
	static const TCHAR* RoleName(ERole R)
	{
		static const TCHAR* N[] = {TEXT("grazer"), TEXT("pair"), TEXT("territory"), TEXT("hoverer"), TEXT("sitter"), TEXT("cruiser")};
		return N[FMath::Clamp(int32(R), 0, 5)];
	}
	static FVector RlG2U(const FVector& P) { return FVector(P.X * 100.0, P.Z * 100.0, P.Y * 100.0); }
	static FVector RlD2U(const FVector& D) { return FVector(D.X, D.Z, D.Y); }
	static double RlHyp(double A, double B) { return FMath::Sqrt(A * A + B * B); }
	static int64 RlKey(int32 X, int32 Z) { return (int64(X) << 32) ^ int64(uint32(Z)); }
	static FVector RlFlat(const FVector& V) { return FVector(V.X, 0, V.Z); }
	static double RlTrap(double D, const double T[4])
	{
		if (D <= T[0] || D >= T[3]) return 0;
		if (D < T[1]) return (D - T[0]) / FMath::Max(1e-6, T[1] - T[0]);
		if (D > T[2]) return (T[3] - D) / FMath::Max(1e-6, T[3] - T[2]);
		return 1;
	}

	struct FAnimal
	{
		FVector P = FVector::ZeroVector, V = FVector::ZeroVector, Fwd = FVector(1, 0, 0);
		FVector Aim = FVector::ZeroVector, Off = FVector::ZeroVector, PoseDir = FVector(1, 0, 0), FleeDir = FVector(1, 0, 0);
		FVector Own = FVector::ZeroVector;   // where it was put down: a Sitter's hops stay within Roam of this
		double Len = .3, Pace = 1, Seed = 0, Shade = 1, AltWant = .6;
		double StateT = 0, Hold = 0, BiteT = 5, PuffT = 30, PoseT = 0, AirT = 0, DownT = 0;
		double TailPh = 0, TailFreq = 3, TailAmp = .03, PecPh = 0, PecAmp = .6, Roll = 0, YawRate = 0, Cocoon = 0;
		uint8 Phase = 0, State = SRove;
		int8 BitesLeft = 0, Lod = -1;
		bool bFloorOnly = false;
		int32 Id = 0, Pool = -1, Slot = -1, FinPool = -1, FinSlot = -1;
		FTransform Xf, FinXf[2];
		float Data[8] = {0, 0, 0, 0, 0, 0, 1, 0};
	};

	struct FGroup
	{
		int32 Id = 0, Sp = 0;
		ERole Role = ERole::Grazer;
		int64 Chunk = 0;
		FVector Home = FVector::ZeroVector, Goal = FVector::ZeroVector, Centre = FVector::ZeroVector;
		double GoalT = 0, StepAcc = 0, ChaseCool = 0, Circ = 0, CircDir = 1, HomeReef = 0, Depth = 0;
		uint32 Zone = 0;
		bool bPosed = false;
		int32 ChaseGroup = -1, ChaseIx = 0, ChasedBy = -1;   // who member 0 is chasing; who is chasing this group's male
		FVector ChaseFrom = FVector::ZeroVector;
		double ChaseStartD = 0;
		FRandomStream Rng;
		TArray<FAnimal> M;
	};

	// one group as the chunk deals it, before anything is spawned
	struct FSeed { int32 Sp = 0, NA = 0, NB = 0, A = 0, B = -1; FVector Home = FVector::ZeroVector; uint32 Zone = 0, Seed = 0; double Depth = 0, Reef = 0; };

	struct FTile { float Top[32 * 32]; float Floor[32 * 32]; uint8 Kind[32 * 32]; };   // Kind: 0 bottom, 1 stony coral, 2 sponge or wreck
	struct FHead { double X, Z, R, TopY, FloorY; bool bStony; };
	struct FPool { int32 Key = 0, Tris = 0; UInstancedStaticMeshComponent* C = nullptr; TArray<FAnimal*> Items; bool bDirty = false, bMoved = false, bFin = false; };
	struct FScar { FVector P; double Size, Age, Life; bool bPit = false; FVector AxU = FVector(1, 0, 0), AyU = FVector(0, 1, 0); };
	struct FPuff { FVector P, V; double Size, Age, Life; };

	// a coral or sponge as something solid: its radius and height, and whether it is stony coral (rock a beak can
	// scrape). Sea fans, plumes, rods, algae and grass are soft: fish pass through them. Sizes are KeysSchools' table
	// for the hand-built kinds and KeysReef's model table (footprint, mean height) for the modelled ones.
	static bool HeadDims(const AKeysReef::FInst& I, double& R, double& H, bool& bStony)
	{
		static const double PH[] = {1.0, 1.1, .56, .72, 0, 0, 1.0, 1.1, .24, 0};
		static const double PR[] = {.85, .8, .9, 1.0, 0, 0, .65, .5, .65, 0};
		static const double MF[] = {2.2, 1.6, 1.0, 1.8, 2.2, .7, 0, 0, 1.4, 0, 1.2, .4, 1.2, 0, 0, 0};
		static const double MH[] = {1.9, 1.2, 1.8, 1.1, 1.6, .45, 1.2, 1.2, .8, 2.1, 1.5, .6, 1.35, .26, .8, .35};
		static const bool MStony[] = {true, true, true, true, true, true, false, false, true, false, false, false, false, false, false, false};
		if (I.Species < 0) return false;
		if (I.Species < 10)
		{
			if (PH[I.Species] <= 0) return false;
			R = PR[I.Species] * FMath::Max(I.SX, I.SZ); H = PH[I.Species] * I.SY; bStony = I.Species <= 3 || I.Species == 8;
			return true;
		}
		const int32 Mi = I.Species - 10;
		if (Mi >= 16 || MF[Mi] <= 0) return false;
		H = I.SY; R = MF[Mi] * .5 * I.SY / MH[Mi]; bStony = MStony[Mi];
		return true;
	}

	struct FImpl
	{
		AKeysReefLife* Act = nullptr;
		const TArray<FSpecies>* Reg = nullptr;
		TWeakObjectPtr<AKeysReef> Reef;
		// ---- the world this frame (game metres unless named ...U)
		FVector Cam = FVector::ZeroVector, CamFwd = FVector(1, 0, 0), CamU = FVector::ZeroVector, CamFwdU = FVector(1, 0, 0), CamRtU = FVector(0, 1, 0), CamUpU = FVector(0, 0, 1);
		FVector Centre = FVector::ZeroVector, DiverP = FVector::ZeroVector, DiverV = FVector::ZeroVector, BoatP = FVector::ZeroVector, BoatV = FVector::ZeroVector;
		double DiverSpd = 0, SurfY = 0, NowT = 0, Hour = 12;
		bool bDiving = false, bUnder = false, bHaveBoat = false, bShotWas = false, bOff = false, bPins = false, bScriptLog = false;
		// ---- the bottom
		TMap<int64, TUniquePtr<FTile>> Tiles;
		TMap<int64, TArray<FHead>> HeadCache;
		int32 TilesThisFrame = 0;
		// ---- who is alive
		TMap<int64, TArray<int32>> Chunks;        // chunk -> the ids of its groups
		TArray<TUniquePtr<FGroup>> Groups;
		TArray<int32> SpAlive, SpRole;            // per species: individuals alive; a role forced by reef:role (-1 none)
		TArray<bool> Seen;
		double Units = 0, BudgetScale = 1;
		int32 NextGroup = 1, NextAnimal = 1, RefusedCap = 0, RefusedSp = 0;
		// ---- drawing
		TArray<FPool> PoolList;
		bool bBuiltMesh = false, bLodPending = true;
		double LodT = 0, ChunkT = 0, SightT = 0;
		uint32 TickN = 0;
		// ---- effects
		TArray<FScar> Scars;
		TArray<FPuff> Puffs;
		bool bFxDrawn = false;
		double CrunchGate = 0;
		// ---- the test camera (reef:look)
		int32 LookGroup = -1, LookIx = 0;
		double LookDist = 1.6, LookT = 0;
		// ---- statistics
		double CpuAcc = 0, CpuWorst = 0, ChunkWorstMs = 0, FrameAfterChunkWorst = 0, StatT = 0;
		int32 CpuN = 0, Wakes = 0, Bites = 0, Crunches = 0, PuffN = 0, Flees = 0, Chases = 0, Sleeps = 0, ChunksDealt = 0, Breaths = 0, Lifts = 0, Digs = 0;
		bool bChunkLastFrame = false;

		const FSpecies& Sp(int32 I) const { return (*Reg)[I]; }
		FGroup* FindGroup(int32 Id) { for (TUniquePtr<FGroup>& G : Groups) if (G->Id == Id) return G.Get(); return nullptr; }
		bool SleepTime(const FSpecies& S) const
		{
			const bool bNight = Hour < Dawn || Hour >= Dusk;
			return S.Habit == EHabit::Day ? bNight : S.Habit == EHabit::Night ? !bNight : false;
		}
		// 0..1: how busy an animal awake by day is at this hour (the last light is slow), and how keenly it feeds
		// (grazers bite fastest in the afternoon, when the algae are richest in sugars)
		double Activity(const FSpecies& S) const
		{
			if (S.Habit != EHabit::Day) return SleepTime(S) ? 0 : 1;
			return FMath::Clamp(FMath::Min((Hour - Dawn) / .75, (Dusk - Hour) / .75), 0.0, 1.0);
		}
		double Feeding(const FSpecies& S) const { return Activity(S) * (.4 + .6 * FMath::Exp(-FMath::Square((Hour - 14.5) / 3.5))); }

		// ================================================================ the bottom: floor, coral heads and wrecks on 1 m tiles
		AKeysReef* ReefActor()
		{
			if (!Reef.IsValid()) { TActorIterator<AKeysReef> It(Act->GetWorld()); if (It) Reef = *It; }
			return Reef.Get();
		}
		const TArray<FHead>& HeadsOf(int32 CX, int32 CZ)
		{
			const int64 K = RlKey(CX, CZ);
			if (const TArray<FHead>* Have = HeadCache.Find(K)) return *Have;
			if (HeadCache.Num() > 80) HeadCache.Reset();
			TArray<FHead> Out;
			if (AKeysReef* R = ReefActor())
				for (const AKeysReef::FInst& I : R->ChunkInstances(CX, CZ))
				{
					double Rr, H; bool bSt;
					if (!HeadDims(I, Rr, H, bSt)) continue;
					Out.Add({I.X, I.Z, Rr, I.Y + H, I.Y, bSt});
				}
			return HeadCache.Add(K, MoveTemp(Out));
		}
		const FTile* TileFor(int32 TX, int32 TZ)
		{
			const int64 K = RlKey(TX, TZ);
			if (const TUniquePtr<FTile>* Have = Tiles.Find(K)) return Have->Get();
			if (TilesThisFrame >= 3) return nullptr;
			TilesThisFrame++;
			if (Tiles.Num() > 400) Tiles.Reset();
			TUniquePtr<FTile> T = MakeUnique<FTile>();
			const double X0 = TX * 32.0, Z0 = TZ * 32.0;
			for (int32 j = 0; j < 32; j++)
				for (int32 i = 0; i < 32; i++)
				{
					const float F = float(KeysSea::SeafloorHeightAt(X0 + i + .5, Z0 + j + .5));
					T->Floor[j * 32 + i] = F; T->Top[j * 32 + i] = F; T->Kind[j * 32 + i] = 0;
				}
			const int32 C0x = FMath::FloorToInt32((X0 - 3) / 50), C1x = FMath::FloorToInt32((X0 + 35) / 50);
			const int32 C0z = FMath::FloorToInt32((Z0 - 3) / 50), C1z = FMath::FloorToInt32((Z0 + 35) / 50);
			for (int32 cz = C0z; cz <= C1z; cz++)
				for (int32 cx = C0x; cx <= C1x; cx++)
					for (const FHead& H : HeadsOf(cx, cz))
					{
						if (H.X < X0 - 3 || H.X > X0 + 35 || H.Z < Z0 - 3 || H.Z > Z0 + 35) continue;
						const int32 I0 = FMath::Max(0, FMath::FloorToInt32(H.X - H.R - X0)), I1 = FMath::Min(31, FMath::FloorToInt32(H.X + H.R - X0));
						const int32 J0 = FMath::Max(0, FMath::FloorToInt32(H.Z - H.R - Z0)), J1 = FMath::Min(31, FMath::FloorToInt32(H.Z + H.R - Z0));
						for (int32 j = J0; j <= J1; j++)
							for (int32 i = I0; i <= I1; i++)
								if (RlHyp(X0 + i + .5 - H.X, Z0 + j + .5 - H.Z) <= H.R + .5 && H.TopY > T->Top[j * 32 + i]) { T->Top[j * 32 + i] = float(H.TopY); T->Kind[j * 32 + i] = H.bStony ? 1 : 2; }
					}
			for (const KeysWrecks::FWreck& W : KeysWrecks::Wrecks())
			{
				if (KeysWrecks::EdgeDist(W, X0 + 16, Z0 + 16) > 30) continue;
				for (int32 j = 0; j < 32; j++)
					for (int32 i = 0; i < 32; i++)
					{
						const double E = KeysWrecks::EdgeDist(W, X0 + i + .5, Z0 + j + .5);
						const float Wt = float(T->Floor[j * 32 + i] + W.Height * (1 - E / 1.5 * .6));
						if (E < 1.5 && Wt > T->Top[j * 32 + i]) { T->Top[j * 32 + i] = Wt; T->Kind[j * 32 + i] = 2; }
					}
			}
			return Tiles.Add(K, MoveTemp(T)).Get();
		}
		double TopAt(double X, double Z, double* OutFloor = nullptr, uint8* OutKind = nullptr)
		{
			const int32 TX = FMath::FloorToInt32(X / 32), TZ = FMath::FloorToInt32(Z / 32);
			const FTile* T = TileFor(TX, TZ);
			if (!T)
			{
				const double F = KeysSea::SeafloorHeightAt(X, Z);
				if (OutFloor) *OutFloor = F;
				if (OutKind) *OutKind = 0;
				return F;
			}
			const int32 I = FMath::Clamp(FMath::FloorToInt32(X - TX * 32.0), 0, 31), J = FMath::Clamp(FMath::FloorToInt32(Z - TZ * 32.0), 0, 31);
			if (OutFloor) *OutFloor = T->Floor[J * 32 + I];
			if (OutKind) *OutKind = T->Kind[J * 32 + I];
			return T->Top[J * 32 + I];
		}
		bool NearestHead(const FVector& At, double MaxR, FHead& Out, bool bStonyOnly = false)
		{
			double Best = MaxR;
			bool bFound = false;
			for (int32 cz = FMath::FloorToInt32((At.Z - MaxR) / 50); cz <= FMath::FloorToInt32((At.Z + MaxR) / 50); cz++)
				for (int32 cx = FMath::FloorToInt32((At.X - MaxR) / 50); cx <= FMath::FloorToInt32((At.X + MaxR) / 50); cx++)
					for (const FHead& H : HeadsOf(cx, cz))
					{
						const double D = RlHyp(H.X - At.X, H.Z - At.Z) - H.R;
						if (bStonyOnly && !H.bStony) continue;
						if (D < Best && H.TopY - H.FloorY > .3 && H.TopY < SurfY - .4) { Best = D; Out = H; bFound = true; }
					}
			return bFound;
		}

		// ================================================================ placement
		void DealChunk(int32 CX, int32 CZ, TArray<FSeed>& Out) const
		{
			FHabitat Hab[ChunkPoints];
			FVector2D Pt[ChunkPoints];
			bool bAny = false;
			for (int32 k = 0; k < ChunkPoints; k++)
			{
				// a jittered 4 x 4 grid
				Pt[k].X = (CX + ((k % 4) + KeysSea::HashCell(0x7eef11, CX, CZ, k * 2)) / 4.0) * ChunkSize;
				Pt[k].Y = (CZ + ((k / 4) + KeysSea::HashCell(0x7eef11, CX, CZ, k * 2 + 1)) / 4.0) * ChunkSize;
				Hab[k] = HabitatAt(Pt[k].X, Pt[k].Y);
				bAny |= Hab[k].Zone != 0;
			}
			if (!bAny) return;
			for (int32 s = 0; s < Reg->Num(); s++)
			{
				const FSpecies& S = Sp(s);
				const uint32 KeyHash = FCrc::StrCrc32(*S.Key);
				double MixTot = 0;
				for (const FMix& Mx : S.Mixes) MixTot += Mx.Weight;
				for (int32 k = 0; k < ChunkPoints; k++)
				{
					const FHabitat& H = Hab[k];
					if (!(S.Zones & H.Zone)) continue;
					const bool bReefZone = (H.Zone & (ZCrest | ZTop | ZSpur | ZPatch | ZFore | ZWall)) != 0;
					double Suit = RlTrap(H.Depth, S.Depth) * (bReefZone ? FMath::Clamp(H.Reef * 1.3, 0.0, 1.0) : 1.0);
					if (S.Where && Suit > 0) Suit *= S.Where(Pt[k].X, Pt[k].Y, H);
					const double Prob = Suit * S.GroupsPerHa * (ChunkSize * ChunkSize / 10000.0) / ChunkPoints * BudgetScale;
					if (KeysSea::HashCell(KeyHash, CX, CZ, k * 8 + 1) >= Prob) continue;
					double Pick = KeysSea::HashCell(KeyHash, CX, CZ, k * 8 + 2) * MixTot;
					const FMix* Mx = &S.Mixes.Last();
					for (const FMix& Q : S.Mixes) { Pick -= Q.Weight; if (Pick <= 0) { Mx = &Q; break; } }
					FSeed Sd;
					Sd.Sp = s; Sd.A = FMath::Clamp(Mx->A, 0, S.Phases.Num() - 1); Sd.B = Mx->B < S.Phases.Num() ? Mx->B : -1;
					Sd.NA = Mx->AMin + FMath::FloorToInt32(KeysSea::HashCell(KeyHash, CX, CZ, k * 8 + 3) * (Mx->AMax - Mx->AMin + 1));
					Sd.NB = Sd.B >= 0 ? Mx->BMin + FMath::FloorToInt32(KeysSea::HashCell(KeyHash, CX, CZ, k * 8 + 4) * (Mx->BMax - Mx->BMin + 1)) : 0;
					// (every species reads the same sixteen points: each stands a few metres off its point, its own way)
					Sd.Home = FVector(Pt[k].X + (KeysSea::HashCell(KeyHash, CX, CZ, k * 8 + 6) - .5) * 6, H.FloorY, Pt[k].Y + (KeysSea::HashCell(KeyHash, CX, CZ, k * 8 + 7) - .5) * 6);
					Sd.Zone = H.Zone; Sd.Depth = H.Depth; Sd.Reef = H.Reef;
					Sd.Seed = uint32(KeysSea::HashCell(KeyHash, CX, CZ, k * 8 + 5) * 4294967295.0);
					Out.Add(Sd);
				}
			}
		}

		bool SleepSpot(const FAnimal& A, const FBody& B, FVector& OutP, FVector& OutDir, bool& bByHead)
		{
			FHead H;
			bByHead = NearestHead(A.P, 7, H);
			if (bByHead)
			{
				FVector Dir = RlFlat(A.P - FVector(H.X, 0, H.Z));
				if (!Dir.Normalize()) Dir = FVector(FMath::Cos(A.Seed * 6.28), 0, FMath::Sin(A.Seed * 6.28));
				Dir = Dir.RotateAngleAxis((A.Seed - .5) * 70, FVector(0, 1, 0));   // (each its own place round the head)
				const FVector At = FVector(H.X, 0, H.Z) + Dir * (H.R + B.HalfW * A.Len + .04);
				double Fl;
				TopAt(At.X, At.Z, &Fl);
				OutP = FVector(At.X, Fl + B.HalfH * A.Len + .02, At.Z);
				OutDir = FVector(-Dir.Z, 0, Dir.X);
				return true;
			}
			double Fl;
			TopAt(A.P.X, A.P.Z, &Fl);
			OutP = FVector(A.P.X, Fl + B.HalfH * A.Len + .02, A.P.Z);
			OutDir = RlFlat(A.Fwd).GetSafeNormal();
			if (OutDir.IsNearlyZero()) OutDir = FVector(1, 0, 0);
			return true;
		}

		void SetOffsets(FGroup& G, const FSpecies& S)
		{
			double MeanLen = 0;
			for (const FAnimal& A : G.M) MeanLen += A.Len;
			MeanLen /= FMath::Max(1, G.M.Num());
			const double Rg = S.Spacing * MeanLen * FMath::Sqrt(double(G.M.Num())) * .55;
			for (int32 i = 0; i < G.M.Num(); i++)
			{
				const double A = G.Rng.FRand() * 2 * PI, Rr = (i == 0 && G.M.Num() > 1) ? 0 : Rg * FMath::Sqrt(G.Rng.FRand());
				G.M[i].Off = FVector(FMath::Cos(A) * Rr, 0, FMath::Sin(A) * Rr);
				G.M[i].AltWant = FMath::Lerp(S.Alt[0], S.Alt[1], G.Rng.FRand());
			}
		}

		void PickGoal(FGroup& G, const FSpecies& S, const FVector* Away = nullptr)
		{
			const double Rad = G.Role == ERole::Territory ? S.ChaseR * .6 : G.Role == ERole::Hoverer || G.Role == ERole::Sitter ? 0 : S.Roam;
			FVector Best = G.Home;
			double BestSc = -1e9;
			for (int32 k = 0; k < 8 && Rad > 0; k++)
			{
				const double A = G.Rng.FRand() * 2 * PI, Rr = Rad * FMath::Sqrt(G.Rng.FRand());
				const double X = G.Home.X + FMath::Cos(A) * Rr, Z = G.Home.Z + FMath::Sin(A) * Rr;
				double Fl; uint8 Kd;
				const double Tp = TopAt(X, Z, &Fl, &Kd);
				if (KeysGeo::LandH(X, Z) >= 0 || -Fl < S.Depth[0] || -Fl > S.Depth[3] || Tp > SurfY - .7) continue;
				double Sc = (Kd == 1 ? 1.0 : 0.0) + G.Rng.FRand();
				if (bDiving && !G.bPosed && RlHyp(X - DiverP.X, Z - DiverP.Z) < S.Shy * 2.5) Sc -= 3;
				if (Away) Sc += FMath::Min(10.0, RlHyp(X - Away->X, Z - Away->Z)) * .4;
				if (Sc > BestSc) { BestSc = Sc; Best = FVector(X, Tp, Z); }
			}
			G.Goal = Best;
			G.GoalT = G.Rng.FRandRange(9.f, 22.f);
			SetOffsets(G, S);
		}

		bool SpawnGroup(const FSeed& Sd, int64 ChunkKey)
		{
			const FSpecies& S = Sp(Sd.Sp);
			const int32 N = Sd.NA + Sd.NB;
			if (N <= 0) return false;
			if (SpAlive[Sd.Sp] + N > S.MaxAlive) { RefusedSp++; return false; }
			if (Units + N * S.Weight > BudgetUnits) { RefusedCap++; return false; }
			TUniquePtr<FGroup> Gp = MakeUnique<FGroup>();
			FGroup& G = *Gp;
			G.Id = NextGroup++; G.Sp = Sd.Sp; G.Chunk = ChunkKey;
			G.Role = SpRole[Sd.Sp] >= 0 ? ERole(SpRole[Sd.Sp]) : S.Role;
			G.Rng.Initialize(int32(Sd.Seed));
			G.Zone = Sd.Zone; G.Depth = Sd.Depth; G.HomeReef = Sd.Reef;
			double Fl;
			const double Tp = TopAt(Sd.Home.X, Sd.Home.Z, &Fl);
			G.Home = FVector(Sd.Home.X, Tp, Sd.Home.Z);
			if (S.bHomeOnHead) { FHead Hd; if (NearestHead(G.Home, 8, Hd)) G.Home = FVector(Hd.X, Hd.TopY, Hd.Z); }
			G.Goal = G.Home; G.Centre = G.Home;
			G.Circ = G.Rng.FRand() * 2 * PI; G.CircDir = G.Rng.FRand() < .5 ? -1 : 1;
			G.M.SetNum(N);
			for (int32 i = 0; i < N; i++)
			{
				FAnimal& A = G.M[i];
				A.Id = NextAnimal++;
				A.Phase = uint8(i < Sd.NA ? Sd.A : Sd.B);
				const FPhase& Ph = S.Phases[A.Phase];
				A.Len = FMath::Lerp(Ph.LenMin, Ph.LenMax, G.Rng.FRand());
				A.Pace = G.Rng.FRandRange(.85f, 1.15f);
				A.Seed = G.Rng.FRand();
				A.Shade = G.Rng.FRandRange(.9f, 1.06f);
				A.BiteT = G.Rng.FRandRange(.5f, 1.f) * S.BiteEvery;
				A.PuffT = (S.DigEvery > 0 ? S.DigEvery : S.PuffEvery) * G.Rng.FRandRange(.2f, 1.2f);
				A.AirT = S.BreathEvery * G.Rng.FRandRange(.15f, 1.f);
				A.PecPh = G.Rng.FRand();
				const double An = G.Rng.FRand() * 2 * PI;
				A.Fwd = FVector(FMath::Cos(An), 0, FMath::Sin(An));
			}
			SetOffsets(G, S);
			const bool bAsleep = SleepTime(S);
			for (FAnimal& A : G.M)
			{
				const FBody& B = S.Phases[A.Phase].Body;
				const double X = G.Home.X + A.Off.X, Z = G.Home.Z + A.Off.Z;
				const double Clear = B.HalfH * A.Len + .06;
				A.P = FVector(X, FMath::Min(TopAt(X, Z) + FMath::Max(Clear, A.AltWant), SurfY - Clear - .25), Z);
				if (S.bOnFloor || S.HeadWithin > 0)
				{
					// a bottom animal: on the floor itself; one that lives against coral, at the foot of the nearest stony head
					FHead Hd;
					uint8 Kd = 0;
					double Fl0;
					TopAt(X, Z, &Fl0, &Kd);
					const bool bFoot = NearestHead(A.P, S.HeadWithin > 0 ? S.HeadWithin : 1.5, Hd, S.HeadWithin > 0);
					if (S.HeadWithin > 0 && !bFoot) return false;   // (nothing has been counted yet: the group is simply not there)
					if (bFoot && (S.HeadWithin > 0 || Kd != 0))
					{
						FVector Dir = RlFlat(A.P - FVector(Hd.X, 0, Hd.Z));
						if (!Dir.Normalize()) Dir = FVector(FMath::Cos(A.Seed * 6.28), 0, FMath::Sin(A.Seed * 6.28));
						const double Off = S.HeadWithin > 0 ? Hd.R * .97 + B.HalfW * A.Len : Hd.R + B.HalfW * A.Len + .05;
						A.P.X = Hd.X + Dir.X * Off; A.P.Z = Hd.Z + Dir.Z * Off;
						A.Fwd = Dir;
					}
					double Fl1;
					TopAt(A.P.X, A.P.Z, &Fl1);
					A.P.Y = Fl1 + B.HalfH * A.Len + .01;
					A.bFloorOnly = true;
				}
				A.Own = A.P;
				if (bAsleep)
				{
					// dealt at night: already tucked in
					bool bHead;
					SleepSpot(A, B, A.Aim, A.PoseDir, bHead);
					A.P = A.Aim; A.Fwd = A.PoseDir; A.State = SSleep; A.Cocoon = S.bCocoon ? 1 : 0; A.bFloorOnly = true;
				}
				Pose(A, S, 0);
			}
			SpAlive[Sd.Sp] += N;
			Units += N * S.Weight;
			Chunks.FindOrAdd(ChunkKey).Add(G.Id);
			Groups.Add(MoveTemp(Gp));
			bLodPending = true;
			return true;
		}

		void DropGroup(int32 Index)
		{
			FGroup& G = *Groups[Index];
			for (FAnimal& A : G.M)
			{
				if (A.Pool >= 0) PoolList[A.Pool].bDirty = true;
				if (A.FinPool >= 0) PoolList[A.FinPool].bDirty = true;
			}
			// (the pools still hold pointers into this group: they are laid out again before anything reads them)
			for (FPool& Pl : PoolList) if (Pl.bDirty) Pl.Items.Reset();
			SpAlive[G.Sp] -= G.M.Num();
			Units -= G.M.Num() * Sp(G.Sp).Weight;
			if (LookGroup == G.Id) LookGroup = -1;
			for (TUniquePtr<FGroup>& O : Groups) { if (O->ChaseGroup == G.Id) O->ChaseGroup = -1; if (O->ChasedBy == G.Id) O->ChasedBy = -1; }
			Groups.RemoveAtSwap(Index);
		}

		void UpdateChunks()
		{
			// let go of what is behind
			for (auto It = Chunks.CreateIterator(); It; ++It)
			{
				const int32 CX = int32(It.Key() >> 32), CZ = int32(uint32(It.Key() & 0xffffffff));
				if (RlHyp((CX + .5) * ChunkSize - Centre.X, (CZ + .5) * ChunkSize - Centre.Z) <= DropR) continue;
				for (int32 i = Groups.Num() - 1; i >= 0; i--) if (Groups[i]->Chunk == It.Key()) DropGroup(i);
				It.RemoveCurrent();
			}
			if (Cam.Y > 150) return;
			// deal the nearest chunk that has not been: one a frame
			const int32 R = FMath::CeilToInt32(LoadR / ChunkSize), IX = FMath::FloorToInt32(Centre.X / ChunkSize), IZ = FMath::FloorToInt32(Centre.Z / ChunkSize);
			double BestD = 1e9;
			int32 BX = 0, BZ = 0;
			for (int32 j = -R; j <= R; j++)
				for (int32 i = -R; i <= R; i++)
				{
					const double D = RlHyp((IX + i + .5) * ChunkSize - Centre.X, (IZ + j + .5) * ChunkSize - Centre.Z);
					if (D > LoadR || D >= BestD || Chunks.Contains(RlKey(IX + i, IZ + j))) continue;
					BestD = D; BX = IX + i; BZ = IZ + j;
				}
			if (BestD > 1e8) return;
			const double T0 = FPlatformTime::Seconds();
			const int64 K = RlKey(BX, BZ);
			Chunks.Add(K);
			TArray<FSeed> Seeds;
			DealChunk(BX, BZ, Seeds);
			int32 NG = 0, NA = 0;
			for (const FSeed& Sd : Seeds) if (SpawnGroup(Sd, K)) { NG++; NA += Sd.NA + Sd.NB; }
			const double Ms = (FPlatformTime::Seconds() - T0) * 1000;
			ChunkWorstMs = FMath::Max(ChunkWorstMs, Ms);
			ChunksDealt++;
			bChunkLastFrame = true;
			if (NG && bScriptLog) UE_LOG(LogTemp, Log, TEXT("KRReef chunk %d,%d: %d groups, %d animals, dealt in %.2f ms (alive %d, %.0f of %.0f units)"), BX, BZ, NG, NA, Ms, AliveCount(), Units, BudgetUnits);
		}
		int32 AliveCount() const { int32 N = 0; for (const int32 C : SpAlive) N += C; return N; }

		// ================================================================ meshes and pools
		int32 PoolFor(int32 SpI, int32 Phase, int32 Kind)   // Kind 0..2 a level of detail, 3 the rowing fin
		{
			const int32 Key = SpI * 64 + Phase * 4 + Kind;
			for (int32 i = 0; i < PoolList.Num(); i++) if (PoolList[i].Key == Key) return PoolList[i].C ? i : -1;
			if (!Act->MID || bBuiltMesh) return -1;
			bBuiltMesh = true;
			const double T0 = FPlatformTime::Seconds();
			const FSpecies& S = Sp(SpI);
			const FBody& B = S.Phases[Phase].Body;
			FKeysMesh M;
			const TFunction<FLinearColor(const FPaint&)> Paint = [&S, Phase](const FPaint& At) { return S.Paint(Phase, At); };
			if (S.Build) S.Build(M, Phase, Kind);
			else if (S.Mesh && S.Mesh(Phase, Kind, M)) {}
			else if (Kind == 3) BuildPectoral(M, B, Paint);
			else BuildBody(M, B, Kind, !(S.bRows && Kind == 0), Paint);
			M.SyncKinds();
			FMeshDescription MD;
			FStaticMeshAttributes Attr(MD);
			Attr.Register();
			TVertexAttributesRef<FVector3f> Pos = Attr.GetVertexPositions();
			TVertexInstanceAttributesRef<FVector3f> Nrm = Attr.GetVertexInstanceNormals();
			TVertexInstanceAttributesRef<FVector4f> Col = Attr.GetVertexInstanceColors();
			TVertexInstanceAttributesRef<FVector2f> UVs = Attr.GetVertexInstanceUVs();
			UVs.SetNumChannels(2);
			const FPolygonGroupID Grp = MD.CreatePolygonGroup();
			Attr.GetPolygonGroupMaterialSlotNames()[Grp] = FName(TEXT("Mat"));
			TArray<FVertexInstanceID> Inst;
			Inst.SetNum(M.V.Num());
			for (int32 i = 0; i < M.V.Num(); i++)
			{
				const FVertexID Vid = MD.CreateVertex();
				Pos[Vid] = FVector3f(M.V[i]);
				const FVertexInstanceID Iid = MD.CreateVertexInstance(Vid);
				Nrm[Iid] = FVector3f(M.N[i]);
				Col[Iid] = FVector4f(M.C[i].R, M.C[i].G, M.C[i].B, 1);
				UVs.Set(Iid, 0, FVector2f(M.UV[i]));
				UVs.Set(Iid, 1, FVector2f(M.K[i]));
				Inst[i] = Iid;
			}
			int32 Tris = 0;
			for (int32 i = 0; i + 2 < M.T.Num(); i += 3)
			{
				const int32 Ia = M.T[i], Ib = M.T[i + 1], Ic = M.T[i + 2];
				const FVector Fn = FVector::CrossProduct(M.V[Ib] - M.V[Ia], M.V[Ic] - M.V[Ia]);
				if (Fn.SizeSquared() < 1e-10) continue;
				const bool bFlip = FVector::DotProduct(Fn, M.N[Ia] + M.N[Ib] + M.N[Ic]) > 0;
				MD.CreateTriangle(Grp, {Inst[Ia], Inst[bFlip ? Ic : Ib], Inst[bFlip ? Ib : Ic]});
				Tris++;
			}
			const FString Nm = FString::Printf(TEXT("%s_%d_%d"), *S.Key, Phase, Kind);
			UStaticMesh* SM = NewObject<UStaticMesh>(Act, *(TEXT("ReefLife_") + Nm));
			SM->GetStaticMaterials().Add(FStaticMaterial(Act->MID, FName(TEXT("Mat"))));
			SM->SetPositiveBoundsExtension(FVector(RlBoundsExt));
			SM->SetNegativeBoundsExtension(FVector(RlBoundsExt));
			UStaticMesh::FBuildMeshDescriptionsParams BP;
			BP.bFastBuild = true;
			BP.bMarkPackageDirty = false;
			BP.bBuildSimpleCollision = false;
			SM->BuildFromMeshDescriptions({&MD}, BP);
			SM->CalculateExtendedBounds();
			Act->Meshes.Add(SM);
			UInstancedStaticMeshComponent* C = NewObject<UInstancedStaticMeshComponent>(Act, *(TEXT("ReefLifePool_") + Nm));
			C->SetStaticMesh(SM);
			C->SetupAttachment(Act->Root);
			C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			C->NumCustomDataFloats = 8;   // tail phase, its rate, when, amplitude, body bend, flash, shade, seed (M_KR_Boid)
			C->SetCastShadow(Kind == 0);   // only the nearest level of detail throws a shadow (marshal, 2026-11-24: dive GPU)
			C->SetMaterial(0, Act->MID);
			C->SetCullDistances(0, int32(S.Cull * 100));
			C->bAffectDistanceFieldLighting = false;
			C->RegisterComponent();
			Act->Pools.Add(C);
			FPool Pl;
			Pl.Key = Key; Pl.Tris = Tris; Pl.C = C; Pl.bFin = Kind == 3;
			PoolList.Add(Pl);
			UE_LOG(LogTemp, Log, TEXT("KRReef mesh %s phase %s %s: %d triangles, built in %.1f ms"), *S.Key, *S.Phases[Phase].Name, Kind == 3 ? TEXT("pectoral fin") : *FString::Printf(TEXT("lod %d"), Kind), Tris, (FPlatformTime::Seconds() - T0) * 1000);
			return PoolList.Num() - 1;
		}

		// how hard an animal with limbs of its own is working them: the stroke it rows with, or its burst
		static double LimbEffort(const FAnimal& A) { return FMath::Clamp(FMath::Max(A.PecAmp, A.TailAmp * 1.6), 0.0, 1.0); }
		// where the fish and its two rowing fins are, for the instances
		void Pose(FAnimal& A, const FSpecies& S, double Dt)
		{
			FRotator R = FRotationMatrix::MakeFromX(RlD2U(A.Fwd)).Rotator();
			R.Roll = FMath::RadiansToDegrees(A.Roll);
			A.Xf = FTransform(R, RlG2U(A.P), FVector(A.Len));
			A.Data[0] = float(FMath::Fmod(A.TailPh, 2 * PI)); A.Data[1] = float(A.TailFreq); A.Data[2] = float(NowT); A.Data[3] = float(A.TailAmp);
			A.Data[4] = float(FMath::Clamp(A.YawRate * .45, -.9, .9)); A.Data[5] = 0; A.Data[6] = float(A.Shade); A.Data[7] = float(A.Seed);
			if (S.bRows) PoseFins(A, S);
			if (!S.Pose) return;
			FPoseIn In;
			In.Phase = A.Phase; In.Len = A.Len; In.Seed = A.Seed; In.Now = NowT; In.Hour = Hour; In.Speed = A.V.Size(); In.StateT = A.StateT; In.bHop = A.State == SHop;
			In.DiverDist = bDiving ? (A.P - DiverP).Size() : 1e9;
			In.P = A.P; In.Fwd = A.Fwd;
			FPoseOut Out;
			Out.Xf = A.Xf; Out.Part[0] = A.FinXf[0]; Out.Part[1] = A.FinXf[1];
			FMemory::Memcpy(Out.Data, A.Data, sizeof(A.Data));
			S.Pose(In, Out);
			A.Xf = Out.Xf; A.FinXf[0] = Out.Part[0]; A.FinXf[1] = Out.Part[1];
			FMemory::Memcpy(A.Data, Out.Data, sizeof(A.Data));
		}

		void PoseFins(FAnimal& A, const FSpecies& S)
		{
			if (S.Limb)
			{
				for (int32 k = 0; k < 2; k++) A.FinXf[k] = S.Limb(A.Phase, k, A.PecPh, LimbEffort(A)) * A.Xf;
				return;
			}
			// the rowing beat: the fin swings forward feathered (flat to the water), then sweeps back broadside
			const FBody& B = S.Phases[A.Phase].Body;
			const double Cyc = FMath::Frac(A.PecPh);
			double Sw, Feather;
			if (Cyc < .6) { const double T = Cyc / .6; Sw = T * T * (3 - 2 * T); Feather = FMath::Sin(PI * T); }
			else { const double T = (Cyc - .6) / .4; Sw = 1 - T * T * (3 - 2 * T); Feather = 0; }
			const double Ang = 10 + 62 * A.PecAmp * Sw, Fe = 60 * A.PecAmp * Feather;
			const double Th = FMath::Asin(FMath::Clamp(B.PecV, -1.0, 1.0));
			const double Yr = B.HalfW * .98 * FMath::Cos(Th) + .004, Zr = B.HalfH * FMath::Sin(Th) * (1 - B.Arch) * .95, Xr = .5 - B.PecU * (1 - B.TailLen);
			for (int32 k = 0; k < 2; k++)
			{
				const double Sd = k ? -1 : 1;
				A.FinXf[k] = FTransform(FRotator(-8, -Sd * Ang, Sd * Fe), FVector(Xr, Sd * Yr, Zr) * 100.0) * A.Xf;
			}
		}

		void UpdateLods()
		{
			bLodPending = false;
			for (TUniquePtr<FGroup>& Gp : Groups)
			{
				const FSpecies& S = Sp(Gp->Sp);
				for (FAnimal& A : Gp->M)
				{
					const double D = (A.P - Cam).Size();
					int32 Lod = D < 11 ? 0 : D < 28 ? 1 : 2;
					if (A.Lod >= 0 && FMath::Abs(Lod - A.Lod) == 1 && FMath::Abs(D - (FMath::Max(Lod, int32(A.Lod)) == 1 ? 11 : 28)) < 2) Lod = A.Lod;   // hysteresis
					if (Lod == A.Lod && A.Pool >= 0) continue;
					const int32 Pl = PoolFor(Gp->Sp, A.Phase, Lod);
					if (Pl < 0) { bLodPending = true; continue; }
					int32 Fin = -1;
					if (Lod == 0 && S.bRows) { Fin = PoolFor(Gp->Sp, A.Phase, 3); if (Fin < 0) { bLodPending = true; continue; } }
					if (A.Pool >= 0) PoolList[A.Pool].bDirty = true;
					if (A.FinPool >= 0) PoolList[A.FinPool].bDirty = true;
					A.Lod = int8(Lod); A.Pool = Pl; A.FinPool = Fin;
					PoolList[Pl].bDirty = true;
					if (Fin >= 0) PoolList[Fin].bDirty = true;
				}
			}
		}

		void WritePools()
		{
			bool bAnyDirty = false;
			for (FPool& Pl : PoolList) if (Pl.bDirty) { Pl.Items.Reset(); bAnyDirty = true; }
			if (bAnyDirty)
				for (TUniquePtr<FGroup>& Gp : Groups)
					for (FAnimal& A : Gp->M)
					{
						if (A.Pool >= 0 && PoolList[A.Pool].bDirty) { A.Slot = PoolList[A.Pool].Items.Add(&A); }
						if (A.FinPool >= 0 && PoolList[A.FinPool].bDirty) { A.FinSlot = PoolList[A.FinPool].Items.Add(&A); }
					}
			TArray<FTransform> Xf;
			for (FPool& Pl : PoolList)
			{
				if (!Pl.C || (!Pl.bDirty && !Pl.bMoved)) continue;
				const int32 Per = Pl.bFin ? 2 : 1;
				Xf.SetNum(Pl.Items.Num() * Per, EAllowShrinking::No);
				for (int32 i = 0; i < Pl.Items.Num(); i++)
				{
					const FAnimal& A = *Pl.Items[i];
					if (Pl.bFin) { Xf[i * 2] = A.FinXf[0]; Xf[i * 2 + 1] = A.FinXf[1]; }
					else Xf[i] = A.Xf;
				}
				if (Pl.bDirty)
				{
					Pl.C->ClearInstances();
					if (Xf.Num()) Pl.C->AddInstances(Xf, false, true);
				}
				else if (Xf.Num()) Pl.C->BatchUpdateInstancesTransforms(0, Xf, true, false, false);
				for (int32 i = 0; i < Pl.Items.Num(); i++)
				{
					const FAnimal& A = *Pl.Items[i];
					if (Pl.bFin)
					{
						// a fin is not bent by the body's wave; a limb with LimbWave (a ray's wing) is, by its effort
						const double Lw = Sp(Pl.Key / 64).LimbWave;
						const float D[8] = {Lw > 0 ? A.Data[0] : 0.f, Lw > 0 ? A.Data[1] : 0.f, A.Data[2], float(Lw * LimbEffort(A)), 0, 0, A.Data[6], A.Data[7]};
						Pl.C->SetCustomData(i * 2, TArrayView<const float>(D, 8), false);
						Pl.C->SetCustomData(i * 2 + 1, TArrayView<const float>(D, 8), false);
					}
					else Pl.C->SetCustomData(i, TArrayView<const float>(A.Data, 8), false);
				}
				Pl.C->MarkRenderStateDirty();
				Pl.bDirty = Pl.bMoved = false;
			}
		}

		// ================================================================ what the animals do
		void Log(const FString& Line) { UE_LOG(LogTemp, Log, TEXT("KRReef %s"), *Line); }
		FString Who(const FGroup& G, const FAnimal& A) const { const FSpecies& S = Sp(G.Sp); return FString::Printf(TEXT("%s %s #%d (group %d)"), *S.Key, *S.Phases[A.Phase].Name, A.Id, G.Id); }
		double RandExp(FGroup& G, double Mean) { return FMath::Clamp(-Mean * FMath::Loge(FMath::Max(1e-4, 1.0 - double(G.Rng.FRand()))), Mean * .3, Mean * 3); }

		void StartFlee(FGroup& G, FAnimal& A, const FVector& From, double Secs)
		{
			FVector Dir = RlFlat(A.P - From);
			if (!Dir.Normalize()) Dir = RlFlat(A.Fwd).GetSafeNormal();
			Dir.Y = -.12;
			A.FleeDir = Dir.GetSafeNormal();
			A.State = SFlee; A.StateT = 0; A.Hold = Secs; A.bFloorOnly = false;
		}
		// one animal bolts and those of its group close by go with it
		void Scare(FGroup& G, int32 I, const FVector& From, const FString& Cause)
		{
			const FSpecies& S = Sp(G.Sp);
			if (S.bNoBolt) return;
			FAnimal& A = G.M[I];
			const double Secs = G.Rng.FRandRange(1.1f, 1.8f);
			if (S.Bury > 0 && A.State != SFlee && A.P.Y - TopAt(A.P.X, A.P.Z) < S.Phases[A.Phase].Body.HalfH * A.Len + .08)
			{
				// it was lying in the sand: the sand goes up with it
				SandCloud(G, A.P, A.Len * .22, 16, A.Len);
				Lifts++;
				Log(FString::Printf(TEXT("lift: %s left the sand it lay in (sunk %.0f%% of its depth) because %s; effect: a cloud of sand %.1f m across at %.1f, %.1f, %.1f, and it glides off low"), *Who(G, A), S.Bury * 100, *Cause, A.Len * .6, A.P.X, A.P.Y, A.P.Z));
			}
			StartFlee(G, A, From, Secs);
			int32 With = 0;
			for (int32 k = 0; k < G.M.Num(); k++)
			{
				FAnimal& O = G.M[k];
				if (k == I || O.State == SFlee || O.State == SSleep || (O.P - A.P).Size() > 2.5) continue;
				StartFlee(G, O, From, Secs * G.Rng.FRandRange(.8f, 1.1f));
				With++;
			}
			Flees++;
			G.GoalT = 0;
			Log(FString::Printf(TEXT("flee: %s bolted because %s; effect: burst at %.1f m/s for %.1f s away from it, %d of its group went with it"), *Who(G, A), *Cause, S.Burst, Secs, With));
		}

		// where an air breather's middle is while it breathes: the top of its back just awash. Down in a trough it goes
		// with the water; on a crest it stays at the mean level (the sea is drawn smoother than KeysOcean::Height says, and
		// an animal above the drawn surface is not seen from below)
		double AirY(const FAnimal& A, const FBody& B, double X, double Z) const { return SurfY + FMath::Min(0.0, KeysOcean::Height(X, Z)) - B.HalfH * A.Len * .3 - .06; }

		// sand thrown up off the bottom: N soft puffs over a patch R wide
		void SandCloud(FGroup& G, const FVector& At, double R, int32 N, double Len)
		{
			for (int32 k = 0; k < N; k++)
			{
				if (Puffs.Num() >= 160) Puffs.RemoveAt(0);
				const double An = G.Rng.FRand() * 2 * PI, Rr = R * FMath::Sqrt(G.Rng.FRand());
				const FVector Dir(FMath::Cos(An), 0, FMath::Sin(An));
				Puffs.Add({At + Dir * Rr + FVector(0, .03, 0), Dir * (.1 + .25 * G.Rng.FRand()) + FVector(0, .06 + .2 * G.Rng.FRand(), 0), (.05 + .05 * G.Rng.FRand()) * FMath::Clamp(Len, .5, 2.0), 0, G.Rng.FRandRange(3.f, 5.f)});
			}
		}

		bool PickBite(FGroup& G, FAnimal& A, const FSpecies& S)
		{
			FVector Best = FVector::ZeroVector;
			double BestSc = -1e9;
			const FVector Toward = RlFlat(G.Goal + A.Off - A.P).GetClampedToMaxSize(1.0);
			for (int32 k = 0; k < 7; k++)
			{
				const double An = G.Rng.FRand() * 2 * PI, Rr = .25 + 1.5 * G.Rng.FRand();
				const double X = A.P.X + Toward.X * .5 + FMath::Cos(An) * Rr, Z = A.P.Z + Toward.Z * .5 + FMath::Sin(An) * Rr;
				double Fl; uint8 Kd;
				const double Tp = TopAt(X, Z, &Fl, &Kd);
				// rock: a stony coral head, or the reef's own rock where the bottom is mostly reef
				if (Kd == 2 || (Kd == 0 && G.HomeReef < .5 && !S.bBitesBottom) || Tp > SurfY - .6) continue;
				if (bDiving && !G.bPosed && (FVector(X, Tp, Z) - DiverP).Size() < S.Shy * 1.3) continue;
				const double Sc = (Kd == 1 ? 1.0 : 0.0) - Rr * .3 + G.Rng.FRand() * .5;
				if (Sc > BestSc) { BestSc = Sc; Best = FVector(X, Tp + .01, Z); }
			}
			if (BestSc < -1e8) return false;
			A.Aim = Best;
			A.BitesLeft = int8(G.Rng.RandRange(1, 3));
			A.State = SToBite; A.StateT = 0;
			return true;
		}

		void OnBite(FGroup& G, FAnimal& A, const FSpecies& S)
		{
			Bites++;
			if (S.ScarCm > 0)
			{
				if (Scars.Num() >= 160) Scars.RemoveAt(0);
				Scars.Add({A.Aim + FVector(0, .012, 0), S.ScarCm / 100 * (.7 + .6 * A.Len / S.Phases[A.Phase].LenMax), 0, 75});
			}
			if (!S.BiteSound || !bUnder) return;
			const double D = (A.Aim - DiverP).Size();
			if (D > BiteHearR || NowT < CrunchGate) return;
			CrunchGate = NowT + .12;
			TArray<FKeysBurst> Bs;
			FString What;
			S.BiteSound(A.Phase, Bs, What);
			if (AKeysAudio* Au = AKeysAudio::Get(Act)) for (const FKeysBurst& Bst : Bs) Au->Burst(Bst, &A.Aim, 2.f);
			Crunches++;
			Log(FString::Printf(TEXT("crunch: %s bit the %s at %.1f, %.1f, %.1f, %.1f m from the diver; played %s"), *Who(G, A), S.bBitesBottom ? TEXT("bottom") : TEXT("rock"), A.Aim.X, A.Aim.Y, A.Aim.Z, D, *What));
		}

		void DoPuff(FGroup& G, FAnimal& A, const FSpecies& S)
		{
			const FVector At = A.P - A.Fwd * (.18 * A.Len) - FVector(0, .08 * A.Len, 0);
			for (int32 k = 0; k < 8; k++)
			{
				if (Puffs.Num() >= 96) Puffs.RemoveAt(0);
				const FVector Vv = -A.Fwd * (.05 + .12 * G.Rng.FRand()) + FVector(G.Rng.FRandRange(-.07f, .07f), G.Rng.FRandRange(-.12f, .02f), G.Rng.FRandRange(-.07f, .07f));
				Puffs.Add({At, Vv, (.025 + .03 * G.Rng.FRand()) * (A.Len / .3), 0, G.Rng.FRandRange(2.2f, 3.4f)});
			}
			PuffN++;
			Log(FString::Printf(TEXT("puff: %s let go a cloud of white sand at %.1f, %.1f, %.1f (%.1f m above the bottom, %.1f m from the camera)"), *Who(G, A), At.X, At.Y, At.Z, At.Y - TopAt(At.X, At.Z), (At - Cam).Size()));
		}

		// swim toward Want (a velocity), round and over coral, never into rock or out of the water; then face and animate
		void Move(FAnimal& A, const FSpecies& S, const FBody& B, FVector Want, double Acc, double Clear, const FVector* Face, double TailAmp, double PecAmp, double Dt)
		{
			const double WSp = Want.Size();
			// (only for a swimmer going somewhere: one sinking straight down onto its perch would take the bottom under it for
			// rock ahead and hang half a metre over it)
			if (WSp > .03 && !A.bFloorOnly && RlHyp(Want.X, Want.Z) > WSp * .3)
			{
				const FVector Dir = Want / WSp, Sd(-Dir.Z, 0, Dir.X);
				for (int32 q = 0; q < 2; q++)
				{
					const double Look = (q ? 1.0 : .4) + A.Len * .5;
					const FVector Q = A.P + Dir * Look;
					const double Need = TopAt(Q.X, Q.Z) + Clear;
					if (A.P.Y + Dir.Y * Look >= Need) continue;
					// rock ahead: over it, and round by whichever side is lower
					Want.Y += (Need - (A.P.Y + Dir.Y * Look)) * (q ? 1.2 : 2.5);
					const double TL = TopAt(Q.X + Sd.X * .8, Q.Z + Sd.Z * .8), TR = TopAt(Q.X - Sd.X * .8, Q.Z - Sd.Z * .8);
					if (FMath::Abs(TL - TR) > .05) Want += Sd * ((TL < TR ? 1 : -1) * WSp * .7);
					break;
				}
			}
			A.V += (Want - A.V).GetClampedToMaxSize(Acc * Dt);
			A.V = A.V.GetClampedToMaxSize(S.Burst * 1.1);
			FVector NP = A.P + A.V * Dt;
			double Fl;
			double Need = TopAt(NP.X, NP.Z, &Fl) + Clear;
			if (A.bFloorOnly) Need = Fl + Clear;
			// one that lies in the sand lies in the sand as it is drawn, not in the 1 m tile's
			if (S.Bury > 0 && Clear < B.HalfH * A.Len && Need - Clear <= Fl + .01) Need = KeysSea::SeafloorHeightAt(NP.X, NP.Z) + Clear;
			if (NP.Y < Need)
			{
				const double MaxLift = (S.Cruise + 1.2) * Dt + .01;
				if (Need - NP.Y > MaxLift && A.P.Y >= TopAt(A.P.X, A.P.Z) + Clear - .02)
				{
					// a wall of coral it cannot get over this step: it does not go in
					NP.X = A.P.X; NP.Z = A.P.Z;
					A.V.X *= .4; A.V.Z *= .4;
					NP.Y = FMath::Max(NP.Y, A.P.Y + MaxLift * .7);
				}
				else NP.Y = Need;
				A.V.Y = FMath::Max(0.0, A.V.Y);
			}
			const double Ceil = A.State == SBreathe ? AirY(A, B, NP.X, NP.Z) : SurfY - FMath::Max(.3, B.HalfH * A.Len + .22);
			if (NP.Y > Ceil) { NP.Y = FMath::Max(Ceil, FMath::Min(NP.Y, A.P.Y)); A.V.Y = FMath::Min(0.0, A.V.Y); }
			A.P = NP;
			// the body
			const FVector OldF = A.Fwd;
			const double Spd = A.V.Size();
			FVector F = Face ? *Face : (Spd > .05 ? A.V / Spd : A.Fwd);
			if (!Face)
			{
				F.Y = FMath::Clamp(F.Y, -.45, .45);
				// going straight up or down it keeps its heading (the clamp alone left it nothing but up or down: it stood on its nose)
				if (RlHyp(F.X, F.Z) < .5)
				{
					FVector Hd = RlFlat(A.Fwd).GetSafeNormal();
					if (Hd.IsNearlyZero()) Hd = FVector(1, 0, 0);
					F = Hd * .9 + FVector(0, F.Y, 0);
				}
			}
			A.Fwd = FMath::Lerp(A.Fwd, F.GetSafeNormal(), FMath::Min(1.0, Dt * (Face ? 6 : 5))).GetSafeNormal();
			if (A.Fwd.IsNearlyZero()) A.Fwd = OldF;
			const double Yr = Dt > 0 ? FMath::FindDeltaAngleRadians(FMath::Atan2(OldF.Z, OldF.X), FMath::Atan2(A.Fwd.Z, A.Fwd.X)) / Dt : 0;
			A.YawRate += (Yr - A.YawRate) * FMath::Min(1.0, Dt * 8);
			const double RollWant = A.State == SSleep ? .22 : A.State == SBite ? FMath::Sin(A.StateT * 55) * .14 : FMath::Clamp(A.YawRate * Spd * .5, -.5, .5);
			A.Roll += (RollWant - A.Roll) * FMath::Min(1.0, Dt * (A.State == SBite ? 30 : 5));
			const double Rel = Spd / FMath::Max(.05, A.Len);
			A.TailFreq = 2 * PI * FMath::Clamp(.9 + .55 * Rel, .9, 9.0);
			A.TailPh = FMath::Fmod(A.TailPh + A.TailFreq * Dt, 2 * PI * 64);
			A.TailAmp += (TailAmp - A.TailAmp) * FMath::Min(1.0, Dt * 4);
			A.PecAmp += (PecAmp - A.PecAmp) * FMath::Min(1.0, Dt * 5);
			A.PecPh += Dt * (1.3 + 1.7 * FMath::Clamp(Spd / FMath::Max(.05, S.Cruise), 0.0, 1.3)) * (A.PecAmp > .05 || (S.Limb && A.TailAmp > .1) ? 1 : 0);
		}

		void StepAnimal(FGroup& G, int32 I, double Dt)
		{
			const FSpecies& S = Sp(G.Sp);
			FAnimal& A = G.M[I];
			const FBody& B = S.Phases[A.Phase].Body;
			A.StateT += Dt;
			A.DownT += Dt;
			const bool bSleepNow = SleepTime(S);
			const double Clear0 = B.HalfH * A.Len + .06;
			// a swimmer that rows holds its tail still; one that does not beats it all the time
			const double TailEase = S.bRows ? .03 : .3, PecEase = S.bRows ? 1 : 0;
			FVector Want = FVector::ZeroVector;
			// ---- the diver: about Shy metres from a slow one, more from a fast one; a rush sends it off
			double Keep = 0, DvD = 1e9;
			FVector AwayDv = FVector::ZeroVector;
			if (bUnder && !G.bPosed && A.State != SSleep)
			{
				const FVector Rel = A.P - DiverP;
				DvD = Rel.Size();
				Keep = S.Shy * (1 + .8 * FMath::Clamp(DiverSpd / 1.0, 0.0, 2.0));
				AwayDv = DvD > 1e-3 ? Rel / DvD : FVector(1, 0, 0);
				const double Closing = FMath::Max(0.0, FVector::DotProduct(DiverV, AwayDv));   // how fast the diver is coming at it
				if (A.State != SFlee && A.State != SChased && (DvD < Keep * .45 || (DvD < Keep * 1.4 && Closing > 1.3)))
					Scare(G, I, DiverP, FString::Printf(TEXT("the diver came within %.1f m at %.1f m/s (it keeps %.1f m at that speed)"), DvD, DiverSpd, Keep));
			}
			if (bHaveBoat && !bDiving && A.State != SSleep && A.State != SFlee && A.P.Y > -3 && BoatV.Size() > 2 && RlHyp(A.P.X - BoatP.X, A.P.Z - BoatP.Z) < 5)
				Scare(G, I, FVector(BoatP.X, 0, BoatP.Z), FString::Printf(TEXT("the boat ran over it at %.1f m/s"), BoatV.Size()));
			const bool bAvoid = DvD < Keep;
			switch (A.State)
			{
			case SFlee:
			{
				Want = A.FleeDir * S.Burst;
				Move(A, S, B, Want, 9, Clear0, nullptr, .6, 0, Dt);
				if (A.StateT > A.Hold) { A.State = SRove; A.StateT = 0; A.BiteT = FMath::Max(A.BiteT, 4.0); }
				break;
			}
			case SChase:
			{
				FGroup* R = FindGroup(G.ChaseGroup);
				const bool bGone = !R || !R->M.IsValidIndex(G.ChaseIx);
				const FVector Tp = bGone ? A.P : R->M[G.ChaseIx].P;
				const double D = (Tp - A.P).Size(), FromHome = (A.P - G.ChaseFrom).Size();
				Want = D > 1e-3 ? (Tp - A.P) / D * S.Burst * .72 : FVector::ZeroVector;
				Move(A, S, B, Want, 6, Clear0, nullptr, .5, 0, Dt);
				if (bGone || A.StateT > 4 || FromHome > S.ChaseR * 2.4 || D > S.ChaseR * 2)
				{
					Log(FString::Printf(TEXT("chase over: %s saw the rival off; effect: pursued it %.1f m in %.1f s, rival now %.1f m away, he turns back to his group"), *Who(G, A), FromHome, A.StateT, D));
					if (R) { R->ChasedBy = -1; PickGoal(*R, Sp(R->Sp), &G.Centre); }
					G.ChaseGroup = -1; G.ChaseCool = 25;
					A.State = SRove; A.StateT = 0;
				}
				break;
			}
			case SChased:
			{
				FGroup* R = FindGroup(G.ChasedBy);
				FVector Dir = R && R->M.Num() ? RlFlat(A.P - R->M[0].P) : A.FleeDir;
				if (!Dir.Normalize()) Dir = A.FleeDir;
				Want = Dir * S.Burst * .8;
				Move(A, S, B, Want, 7, Clear0, nullptr, .55, 0, Dt);
				if ((!R && A.StateT > 1.2) || A.StateT > 5) { G.ChasedBy = -1; A.State = SRove; A.StateT = 0; }
				break;
			}
			case SToBite:
			{
				FVector Hd = RlFlat(A.Aim - A.P);
				const double HDist = Hd.Size();
				if (HDist > .02) Hd /= HDist; else Hd = RlFlat(A.Fwd).GetSafeNormal();
				// beak-down: the body tipped some 50 degrees onto the rock
				A.PoseDir = (Hd * FMath::Cos(FMath::DegreesToRadians(S.BiteTilt)) - FVector(0, FMath::Sin(FMath::DegreesToRadians(S.BiteTilt)), 0)).GetSafeNormal();
				const FVector Ctr = A.Aim - A.PoseDir * (.47 * A.Len);
				const FVector To = Ctr - A.P;
				const double D = To.Size();
				Want = D > 1e-3 ? To / D * FMath::Min(S.Cruise * 1.4, D * 3 + .03) : FVector::ZeroVector;
				const bool bNear = D < A.Len * 1.6;
				Move(A, S, B, Want, 2.5, B.HalfW * A.Len + .02, bNear ? &A.PoseDir : nullptr, TailEase, PecEase * .7, Dt);
				if (bAvoid) { A.State = SRove; A.StateT = 0; A.BiteT = 3; }
				else if (D < .05) { A.State = SBite; A.StateT = 0; A.V = FVector::ZeroVector; OnBite(G, A, S); }
				else if (A.StateT > 5) { A.State = SRove; A.StateT = 0; A.BiteT = 2; }   // could not get its beak to the spot: tries another
				break;
			}
			case SBite:
			{
				Move(A, S, B, FVector::ZeroVector, 20, B.HalfW * A.Len + .02, &A.PoseDir, TailEase, PecEase * .3, Dt);
				if (A.StateT > .24)
				{
					if (--A.BitesLeft > 0 && !bAvoid)
					{
						// another mouthful a hand's width along
						const double An = G.Rng.FRand() * 2 * PI, X = A.Aim.X + FMath::Cos(An) * .09, Z = A.Aim.Z + FMath::Sin(An) * .09;
						A.Aim = FVector(X, TopAt(X, Z) + .01, Z);
						A.State = SToBite; A.StateT = 0;
						A.V = -A.PoseDir * .25;
					}
					else
					{
						A.State = SRove; A.StateT = 0;
						A.V = -RlFlat(A.PoseDir) * .2 + FVector(0, .35, 0);
						A.BiteT = RandExp(G, S.BiteEvery);
					}
				}
				break;
			}
			case SToSleep:
			{
				const FVector To = A.Aim - A.P;
				const double D = To.Size();
				A.bFloorOnly = RlHyp(To.X, To.Z) < 1.3;   // the last of the way it sinks down beside the head, not onto it
				Want = D > 1e-3 ? To / D * FMath::Min(S.Cruise * 1.2, D * 2 + .03) : FVector::ZeroVector;
				Move(A, S, B, Want, 1.5, A.bFloorOnly ? B.HalfH * A.Len + .015 : Clear0, D < .5 ? &A.PoseDir : nullptr, TailEase, PecEase * .6, Dt);
				if (!bSleepNow) { A.State = SRove; A.StateT = 0; A.bFloorOnly = false; }
				else if (D < .06 || A.StateT > 30)
				{
					A.State = SSleep; A.StateT = 0; A.V = FVector::ZeroVector;
					Sleeps++;
					if ((A.P - Cam).Size() < 25) Log(FString::Printf(TEXT("sleep: %s tucked in at %.1f, %.1f, %.1f because the feeding clock reads %s (night from %s); effect: lying still against the reef%s"), *Who(G, A), A.P.X, A.P.Y, A.P.Z,
						*KeysFeed::ClockText(Hour), *KeysFeed::ClockText(Dusk), S.bCocoon ? TEXT(", spinning its mucus cocoon") : TEXT("")));
				}
				break;
			}
			case SSleep:
			{
				A.V = FVector::ZeroVector;
				Move(A, S, B, FVector::ZeroVector, 5, B.HalfH * A.Len + .015, &A.PoseDir, 0, 0, Dt);
				if (S.bCocoon) A.Cocoon = FMath::Min(1.0, A.Cocoon + Dt / 20);
				if (!bSleepNow)
				{
					A.State = SRove; A.StateT = 0; A.bFloorOnly = false;
					A.V = FVector(0, .3, 0);
					Wakes++;
					if ((A.P - Cam).Size() < 25) Log(FString::Printf(TEXT("wake: %s left its sleeping place because the feeding clock reads %s (day from %s); effect: out over the reef again%s"), *Who(G, A), *KeysFeed::ClockText(Hour), *KeysFeed::ClockText(Dawn),
						S.bCocoon ? TEXT(", the cocoon left behind") : TEXT("")));
				}
				break;
			}
			case SBreathe:
			{
				// up to the surface on a slant, the head out for a few breaths, then down again
				const double TopY = AirY(A, B, A.P.X, A.P.Z);
				const bool bUp = A.P.Y > TopY - .1;
				FVector Hd = RlFlat(A.Fwd).GetSafeNormal();
				if (Hd.IsNearlyZero()) Hd = FVector(1, 0, 0);
				const FVector HeadUp = (Hd + FVector(0, .26, 0)).GetSafeNormal();
				if (bUp) { Want = Hd * (S.Cruise * .25); A.Hold += Dt; }
				else { Want = Hd * (S.Cruise * .8) + FVector(0, FMath::Min(.45, (TopY - A.P.Y) * .8 + .1), 0); A.Hold = 0; }
				Move(A, S, B, Want, 1.2, Clear0, bUp ? &HeadUp : nullptr, TailEase, PecEase * (bUp ? .3 : 1), Dt);
				if (A.Hold > S.BreathSecs || A.StateT > 120)
				{
					Breaths++;
					if ((A.P - Cam).Size() < 45) Log(FString::Printf(TEXT("breath: %s took air for %.1f s at the surface (%.1f, %.1f, %.1f) because it had been down %.0f s (it breathes about every %.0f s); effect: it turns down again for the bottom %.1f m below"), *Who(G, A), A.Hold,
						A.P.X, A.P.Y, A.P.Z, A.DownT - A.StateT, S.BreathEvery, A.P.Y - TopAt(A.P.X, A.P.Z)));
					A.AirT = RandExp(G, S.BreathEvery); A.DownT = 0;
					A.State = SRove; A.StateT = 0;
				}
				break;
			}
			case SDig:
			{
				// wings beating in place, sand going up, a pit under its mouth; then it moves on
				const FVector Flat = RlFlat(A.Fwd).GetSafeNormal();
				Move(A, S, B, FVector::ZeroVector, 4, B.HalfH * A.Len * (1 - S.Bury) + .01, &Flat, .5, 0, Dt);
				A.Hold -= Dt;
				if (A.Hold <= 0) { A.Hold = .45; SandCloud(G, A.P + Flat * (.22 * A.Len), A.Len * .1, 4, A.Len); }
				if (A.StateT > 5 || bAvoid)
				{
					const FVector Pit(A.P.X + Flat.X * .22 * A.Len, KeysSea::SeafloorHeightAt(A.P.X + Flat.X * .22 * A.Len, A.P.Z + Flat.Z * .22 * A.Len) + .015, A.P.Z + Flat.Z * .22 * A.Len);
					const double Wide = FMath::Clamp(A.Len * .2, .2, .6);
					if (Scars.Num() >= 160) Scars.RemoveAt(0);
					// (it lies on the bottom as the bottom slopes there)
					const double Sx = (KeysSea::SeafloorHeightAt(Pit.X + .4, Pit.Z) - KeysSea::SeafloorHeightAt(Pit.X - .4, Pit.Z)) / .8, Sz = (KeysSea::SeafloorHeightAt(Pit.X, Pit.Z + .4) - KeysSea::SeafloorHeightAt(Pit.X, Pit.Z - .4)) / .8;
					Scars.Add({Pit, Wide, 0, 300, true, RlD2U(FVector(1, Sx, 0).GetSafeNormal()), RlD2U(FVector(0, Sz, 1).GetSafeNormal())});
					Digs++;
					Log(FString::Printf(TEXT("dig: %s dug for %.1f s for what lives in the sand; effect: a pit %.0f cm across left at %.1f, %.1f, %.1f (it fades in 5 min) and the sand it threw up drifting off; it moves on"), *Who(G, A), A.StateT, Wide * 100, Pit.X, Pit.Y, Pit.Z));
					A.PuffT = RandExp(G, S.DigEvery); A.BiteT = bAvoid ? A.BiteT : 0;
					A.State = SRove; A.StateT = 0;
				}
				break;
			}
			case SHop:
			{
				const FVector To = A.Aim - A.P;
				const double D = To.Size();
				Want = D > 1e-3 ? To / D * FMath::Min(S.Burst * .5, D * 4 + .05) : FVector::ZeroVector;
				Move(A, S, B, Want, 6, B.HalfH * A.Len + .01, nullptr, .4, PecEase * .4, Dt);
				if (D < .05 || A.StateT > 4) { A.State = SRove; A.StateT = 0; A.V = FVector::ZeroVector; }
				break;
			}
			default:
			{
				if (bSleepNow)
				{
					bool bHead;
					SleepSpot(A, B, A.Aim, A.PoseDir, bHead);
					A.State = SToSleep; A.StateT = 0;
					break;
				}
				const double Act01 = Activity(S);
				double Clear = Clear0, PecA = PecEase * (.45 + .55 * Act01);
				const FVector* Face = nullptr;
				FVector FaceV;
				if (A.PoseT > 0)
				{
					// held for the test camera: hovering in place, side on
					A.PoseT -= Dt;
					FaceV = RlFlat(A.Fwd).GetSafeNormal();
					Move(A, S, B, FVector::ZeroVector, 3, Clear, &FaceV, TailEase, PecEase * .5, Dt);
					break;
				}
				if (S.BreathEvery > 0)
				{
					A.AirT -= Dt;
					if (A.AirT <= 0)
					{
						if ((A.P - Cam).Size() < 45) Log(FString::Printf(TEXT("breath: %s leaves the bottom for the surface %.1f m up because it has been down %.0f s"), *Who(G, A), SurfY - A.P.Y, A.DownT));
						A.State = SBreathe; A.StateT = 0; A.Hold = 0; A.bFloorOnly = false;
						break;
					}
				}
				switch (G.Role)
				{
				case ERole::Hoverer:
				{
					// over its head, facing the current; down onto the head when a diver is near
					const bool bLow = DvD < Keep * 1.6;
					const FVector Tgt(G.Home.X + A.Off.X * .3, G.Home.Y + (bLow ? .12 : A.AltWant) + .05 * FMath::Sin(NowT * 1.3 + A.Seed * 9), G.Home.Z + A.Off.Z * .3);
					Want = (Tgt - A.P).GetClampedToMaxSize(1.0) * S.Cruise * (bLow ? 3 : 1.5);
					const FVector2D Cur = KeysSea::CurrentAt(A.P.X, A.P.Z, NowT, -A.P.Y);
					FaceV = FVector(-Cur.X, 0, -Cur.Y).GetSafeNormal();
					if ((Tgt - A.P).Size() < .5 && !FaceV.IsNearlyZero()) Face = &FaceV;
					break;
				}
				case ERole::Sitter:
				{
					// on the bottom, still; it shifts to a new perch now and then, sooner with a diver close
					double Fl;
					const double Tp = TopAt(A.P.X, A.P.Z, &Fl);
					Clear = B.HalfH * A.Len * (1 - 2 * S.Bury) + .01;
					Want = FVector(0, ((S.bOnFloor ? Fl : (S.Bury > 0 && Tp <= Fl + .01 ? KeysSea::SeafloorHeightAt(A.P.X, A.P.Z) : Tp)) + Clear - A.P.Y) * 3, 0);
					PecA = 0;
					// it lies level, facing the way it faced (left to Move it would point the way it sank: nose down)
					FaceV = RlFlat(A.Fwd).GetSafeNormal();
					if (FaceV.IsNearlyZero()) FaceV = FVector(1, 0, 0);
					Face = &FaceV;
					if (S.Bury > 0 && Tp > Fl + .05) A.BiteT = 0;   // it lies in sand, not on a coral head: off it goes to find some
					if (S.DigEvery > 0 && !bAvoid && A.P.Y < Tp + B.HalfH * A.Len + .05)
					{
						A.PuffT -= Dt * Feeding(S);
						if (A.PuffT <= 0)
						{
							A.State = SDig; A.StateT = 0; A.Hold = 0; A.V = FVector::ZeroVector;
							Log(FString::Printf(TEXT("dig: %s starts to dig at %.1f, %.1f, %.1f (it digs about every %.0f s at the best of the day; the clock reads %s)"), *Who(G, A), A.P.X, A.P.Y, A.P.Z, S.DigEvery, *KeysFeed::ClockText(Hour)));
							break;
						}
					}
					A.BiteT -= Dt * Act01 * (bAvoid ? 6 : 1);
					if (A.BiteT <= 0)
					{
						A.BiteT = RandExp(G, FMath::Max(4.0, S.BiteEvery));
						const double An = bAvoid ? FMath::Atan2(AwayDv.Z, AwayDv.X) + G.Rng.FRandRange(-.6f, .6f) : G.Rng.FRand() * 2 * PI, Rr = FMath::Min(S.Roam, 2.5) * (.4 + .6 * G.Rng.FRand());
						FVector Q = A.P + FVector(FMath::Cos(An) * Rr, 0, FMath::Sin(An) * Rr);
						// (each within Roam of where it was put down: measured from the group's home, every member of a
						// scatter wider than Roam hopped to the same point)
						if (RlHyp(Q.X - A.Own.X, Q.Z - A.Own.Z) > S.Roam) Q = A.Own;
						double QFl;
						const double QTp = TopAt(Q.X, Q.Z, &QFl);
						A.Aim = FVector(Q.X, (S.bOnFloor ? QFl : QTp) + Clear, Q.Z);
						A.State = SHop; A.StateT = 0;
					}
					break;
				}
				case ERole::Cruiser:
				{
					// a long round through the reef in mid-water, in file
					const FVector Tan(-FMath::Sin(G.Circ) * G.CircDir, 0, FMath::Cos(G.Circ) * G.CircDir);
					FVector Tgt = G.Goal - Tan * (I * S.Spacing * A.Len);
					Tgt.Y = FMath::Min(TopAt(Tgt.X, Tgt.Z) + A.AltWant, SurfY - 1);
					const FVector To = Tgt - A.P;
					Want = To.GetSafeNormal() * S.Cruise * A.Pace * FMath::Clamp(To.Size() / 2, .5, 1.6);
					break;
				}
				default:
				{
					// Grazer, Pair, Territory: to its place in the group as the group roves
					FVector Tgt = G.Goal + A.Off;
					if (G.Role == ERole::Pair && I > 0)
					{
						const FAnimal& Ld = G.M[0];
						const FVector Sd(-Ld.Fwd.Z, 0, Ld.Fwd.X);
						Tgt = Ld.P - RlFlat(Ld.Fwd) * (1.3 * A.Len) + Sd * ((I & 1 ? .5 : -.5) * A.Len);
						Tgt.Y = Ld.P.Y - TopAt(Ld.P.X, Ld.P.Z) - A.AltWant;   // (the same height over the bottom as its mate)
					}
					Tgt.Y = TopAt(Tgt.X, Tgt.Z) + FMath::Max(Clear, A.AltWant + (G.Role == ERole::Pair && I > 0 ? Tgt.Y : 0));
					const FVector To = Tgt - A.P;
					const double D = To.Size();
					Want = D > 1e-3 ? To / D * S.Cruise * A.Pace * FMath::Clamp(D / 1.2, .1, D > 6 ? 1.8 : 1.0) * (.35 + .65 * Act01) : FVector::ZeroVector;
					if (S.bBitesRock && !bAvoid)
					{
						A.BiteT -= Dt * Feeding(S);
						if (A.BiteT <= 0 && !PickBite(G, A, S)) A.BiteT = 1.5;
					}
					break;
				}
				}
				// keep off its neighbours, and off the diver
				const int32 N = G.M.Num();
				for (int32 k = 1; k <= FMath::Min(4, N - 1); k++)
				{
					const FAnimal& O = G.M[(I + k * FMath::Max(1, N / 5)) % N];
					if (&O == &A) continue;
					const FVector Dl = A.P - O.P;
					const double D = Dl.Size(), Sep = S.Spacing * A.Len * .6;
					if (D < Sep && D > 1e-3) Want += Dl / D * ((Sep - D) / Sep * S.Cruise * 1.5);
				}
				if (bAvoid) Want += (RlFlat(AwayDv).GetSafeNormal() + FVector(0, -.1, 0)) * (S.Cruise * 2.6 * (1 - DvD / Keep) + .1);
				if (S.PuffEvery > 0 && A.State == SRove)
				{
					A.PuffT -= Dt * Act01;
					if (A.PuffT <= 0) { A.PuffT = S.PuffEvery * G.Rng.FRandRange(.6f, 1.4f); if (A.P.Y - TopAt(A.P.X, A.P.Z) > .3) DoPuff(G, A, S); }
				}
				if (A.State == SRove) Move(A, S, B, Want, bAvoid ? 3 : 1.2, Clear, Face, TailEase, PecA, Dt);
				break;
			}
			}
			if (A.State != SSleep && A.Cocoon > 0) A.Cocoon = FMath::Max(0.0, A.Cocoon - Dt / 3);
			Pose(A, S, Dt);
			if (A.Pool >= 0) PoolList[A.Pool].bMoved = true;
			if (A.FinPool >= 0) PoolList[A.FinPool].bMoved = true;
		}

		void StepGroup(FGroup& G, double Dt)
		{
			const FSpecies& S = Sp(G.Sp);
			FVector C = FVector::ZeroVector;
			for (const FAnimal& A : G.M) C += A.P;
			G.Centre = C / FMath::Max(1, G.M.Num());
			G.GoalT -= Dt;
			G.ChaseCool -= Dt;
			const bool bAwake = !SleepTime(S);
			if (G.Role == ERole::Cruiser)
			{
				G.Circ += G.CircDir * Dt * S.Cruise / FMath::Max(4.0, S.Roam);
				const double X = G.Home.X + FMath::Cos(G.Circ) * S.Roam, Z = G.Home.Z + FMath::Sin(G.Circ) * S.Roam;
				if (KeysGeo::LandH(X, Z) >= 0 || TopAt(X, Z) > SurfY - 1.2) G.CircDir = -G.CircDir;   // shoal water ahead: back the way it came
				else G.Goal = FVector(X, 0, Z);
			}
			else if (bAwake && (G.GoalT <= 0 || (bUnder && !G.bPosed && RlHyp(DiverP.X - G.Goal.X, DiverP.Z - G.Goal.Z) < S.Shy * 1.5 && G.GoalT < 6))) PickGoal(G, S);
			// ---- the male sees a rival off; a territory holder sees off anything its size that comes in
			if (bAwake && G.ChaseGroup < 0 && G.ChasedBy < 0 && G.ChaseCool <= 0 && G.M.Num() && (G.M[0].State == SRove || G.M[0].State == SToBite))
			{
				const bool bMale = S.ChaserPhase >= 0 && G.M[0].Phase == S.ChaserPhase && G.Role != ERole::Territory;
				if (bMale || G.Role == ERole::Territory)
				{
					FAnimal& Me = G.M[0];
					for (TUniquePtr<FGroup>& Op : Groups)
					{
						FGroup& O = *Op;
						if (&O == &G || O.ChasedBy >= 0 || O.ChaseGroup >= 0 || !O.M.Num()) continue;
						if (bMale && (O.Sp != G.Sp || O.M[0].Phase != S.ChaserPhase)) continue;
						if (RlHyp(O.Centre.X - G.Centre.X, O.Centre.Z - G.Centre.Z) > S.ChaseR + 30) continue;
						int32 Ix = -1;
						double D = S.ChaseR;
						if (bMale) { D = (O.M[0].P - Me.P).Size(); if (D < S.ChaseR && O.M[0].State != SSleep && O.M[0].State != SFlee) Ix = 0; }
						else for (int32 k = 0; k < O.M.Num(); k++) { const double Dk = (O.M[k].P - G.Home).Size(); if (Dk < D && O.M[k].Len < Me.Len * 1.5 && O.M[k].State != SSleep) { D = Dk; Ix = k; } }
						if (Ix < 0) continue;
						// between two males the one nearer his own ground does the chasing
						if (bMale && (Me.P - G.Home).Size() > (O.M[0].P - O.Home).Size() + .5) continue;
						G.ChaseGroup = O.Id; G.ChaseIx = Ix; G.ChaseFrom = Me.P; G.ChaseStartD = D;
						Me.State = SChase; Me.StateT = 0;
						FAnimal& Him = O.M[Ix];
						Him.State = SChased; Him.StateT = 0; Him.FleeDir = RlFlat(Him.P - Me.P).GetSafeNormal();
						if (Ix == 0) O.ChasedBy = G.Id;
						Chases++;
						Log(FString::Printf(TEXT("chase: %s went for %s because it came within %.1f m of %s (he drives rivals off inside %.1f m); effect: he charges at %.1f m/s, the rival turns and runs"), *Who(G, Me), *Who(O, Him), D,
							bMale ? TEXT("him and his group") : TEXT("his territory"), S.ChaseR, S.Burst * .72));
						break;
					}
				}
			}
			for (int32 i = 0; i < G.M.Num(); i++) StepAnimal(G, i, Dt);
		}

		// ================================================================ the frame
		void ReadWorld(double Dt)
		{
			UWorld* W = Act->GetWorld();
			NowT = W->GetTimeSeconds();
			SurfY = KeysOcean::SeaLevel();
			Hour = KeysFeed::HourNow(Act);
			if (APlayerCameraManager* C = UGameplayStatics::GetPlayerCameraManager(Act, 0))
			{
				CamU = C->GetCameraLocation();
				Cam = FVector(CamU.X / 100, CamU.Z / 100, CamU.Y / 100);
				const FRotationMatrix Rm(C->GetCameraRotation());
				CamFwdU = Rm.GetScaledAxis(EAxis::X); CamRtU = Rm.GetScaledAxis(EAxis::Y); CamUpU = Rm.GetScaledAxis(EAxis::Z);
				CamFwd = FVector(CamFwdU.X, CamFwdU.Z, CamFwdU.Y);
			}
			Centre = Cam;
			bHaveBoat = false;
			if (AKeysBoatPawn* P = Cast<AKeysBoatPawn>(UGameplayStatics::GetPlayerPawn(Act, 0)))
			{
				const FVector N(P->BoatX(), 0, P->BoatZ());
				if (Dt > 0) { const FVector Vn = (N - BoatP) / Dt; if (Vn.Size() < 60) BoatV = FMath::Lerp(BoatV, Vn, FMath::Min(1.0, Dt * 4)); }
				BoatP = N;
				bHaveBoat = true;
				Centre = BoatP;
			}
			bDiving = bUnder = false;
			if (AKeysDiver* D = AKeysDiver::Get(Act); D && D->IsDiving())
			{
				const KeysDive::FState& Ds = D->State();
				bDiving = true;
				DiverP = FVector(Ds.X, Ds.Y, Ds.Z);
				DiverV = FVector(Ds.VX, Ds.VY, Ds.VZ);
				DiverSpd = Ds.Speed();
				bUnder = DiverP.Y < SurfY - .3;
				Centre = DiverP;
				// a spear let go: everything in front of the gun bolts
				const bool bShot = D->ShotInFlight();
				if (bShot && !bShotWas) Act->Startle(DiverP + CamFwd * 4, 7, TEXT("a spear was fired"));
				bShotWas = bShot;
			}
		}

		void FirstSights()
		{
			if (!bUnder) return;
			for (TUniquePtr<FGroup>& Gp : Groups)
			{
				if (Seen[Gp->Sp]) continue;
				for (const FAnimal& A : Gp->M)
				{
					const FVector To = A.P - Cam;
					const double D = To.Size();
					if (D > 7 || D < .2 || FVector::DotProduct(To / D, CamFwd) < .6) continue;
					Seen[Gp->Sp] = true;
					const FSpecies& S = Sp(Gp->Sp);
					const FString CountKey = TEXT("reef.seen.") + S.Key;
					bool bNew = true;
					if (AKeysBoatPawn* P = Cast<AKeysBoatPawn>(UGameplayStatics::GetPlayerPawn(Act, 0)))
					{
						bNew = P->CountOf(*CountKey) == 0;
						P->AddCount(*CountKey, 1);
						if (bNew && !S.FirstSight.IsEmpty()) P->ToastMsg(S.FirstSight, 6);
					}
					Log(FString::Printf(TEXT("first sight: %s (%s, %s) %.1f m ahead of the mask; toast %s: \"%s\"; logbook count %s"), *S.Key, *S.Common, *S.Scientific, D, bNew ? TEXT("shown") : TEXT("not shown (seen on an earlier dive)"), *S.FirstSight, *CountKey));
					break;
				}
			}
		}

		// ---- scars, sand, cocoons and the test pins: a few soft quads in M_KR_Bubble, rebuilt while any exist
		void DrawFx(double Dt)
		{
			for (int32 i = Scars.Num() - 1; i >= 0; i--) { Scars[i].Age += Dt; if (Scars[i].Age > Scars[i].Life) Scars.RemoveAt(i); }
			for (int32 i = Puffs.Num() - 1; i >= 0; i--)
			{
				FPuff& Pf = Puffs[i];
				Pf.Age += Dt;
				if (Pf.Age > Pf.Life) { Puffs.RemoveAtSwap(i); continue; }
				Pf.V *= FMath::Max(0.0, 1 - Dt * 1.6);
				Pf.V.Y -= .05 * Dt;
				Pf.P += Pf.V * Dt;
			}
			if (!Act->Fx) return;
			TArray<FVector> Vx, Nx;
			TArray<int32> Tr;
			TArray<FVector2D> Uv;
			TArray<FLinearColor> Cl;
			auto Quad = [&](const FVector& G, const FVector& AxU, const FVector& AyU, const FLinearColor& Col, bool bSoft)
			{
				const FVector Pu = RlG2U(G);
				const double Dist = FVector::DotProduct(Pu - CamU, CamFwdU);
				if (Dist < 8 || Dist > 6000) return;
				const int32 I0 = Vx.Num();
				const double U0 = bSoft ? 2 : 0;
				// (wound to face the camera, as the diver's bubbles are)
				const FVector Ay2 = FVector::DotProduct(FVector::CrossProduct(AxU, AyU), CamFwdU) < 0 ? -AyU : AyU;
				Vx.Add(Pu - AxU - Ay2); Vx.Add(Pu + AxU - Ay2); Vx.Add(Pu + AxU + Ay2); Vx.Add(Pu - AxU + Ay2);
				Uv.Add(FVector2D(U0, 1)); Uv.Add(FVector2D(U0 + .999, 1)); Uv.Add(FVector2D(U0 + .999, 0)); Uv.Add(FVector2D(U0, 0));
				for (int32 k = 0; k < 4; k++) { Nx.Add(-CamFwdU); Cl.Add(Col); }
				Tr.Append({I0, I0 + 2, I0 + 1, I0, I0 + 3, I0 + 2});
			};
			for (const FScar& Sc : Scars)
			{
				const double Hs = Sc.Size * 50, Al = .85 * FMath::Pow(1 - Sc.Age / Sc.Life, .7);
				if (Sc.bPit)
				{
					// a pit dug in the sand, seen as it lies: the spoil thrown out round it, pale, and the hollow, in shadow
					Quad(Sc.P + FVector(0, .012, 0), Sc.AxU * (Hs * 1.7), Sc.AyU * (Hs * 1.7), FLinearColor(.93f, .9f, .8f, float(.32 * Al)), true);
					Quad(Sc.P + FVector(0, .024, 0), Sc.AxU * Hs, Sc.AyU * Hs, FLinearColor(.2f, .19f, .16f, float(.7 * Al)), true);
					continue;
				}
				Quad(Sc.P, CamRtU * Hs, CamUpU * Hs, FLinearColor(.86f, .84f, .76f, float(Al)), true);
			}
			for (const FPuff& Pf : Puffs)
			{
				const double K = Pf.Age / Pf.Life, Hs = Pf.Size * (1 + 3.5 * K) * 50;
				Quad(Pf.P, CamRtU * Hs, CamUpU * Hs, FLinearColor(.9f, .87f, .78f, float(.38 * (1 - K) * FMath::Min(1.0, Pf.Age * 8))), true);
			}
			for (TUniquePtr<FGroup>& Gp : Groups)
			{
				if ((Gp->Centre - Cam).Size() > 30) continue;
				for (const FAnimal& A : Gp->M)
				{
					if (A.Cocoon > .01)
					{
						// a clear envelope round the sleeping fish: a rim and a highlight, a little larger than the body
						const FBody& B = Sp(Gp->Sp).Phases[A.Phase].Body;
						const FVector Ax = RlD2U(A.Fwd).GetSafeNormal();
						FVector Ay = FVector::CrossProduct(Ax, (RlG2U(A.P) - CamU).GetSafeNormal());
						if (!Ay.Normalize()) Ay = CamUpU;
						Quad(A.P, Ax * (A.Len * 64), Ay * (A.Len * 100 * (B.HalfH + .12)), FLinearColor(.72f, .9f, .95f, float(.5 * A.Cocoon)), false);
					}
					const double PinR = FMath::Max(9.0, (RlG2U(A.P) - CamU).Size() * .008);
					if (bPins) Quad(A.P + FVector(0, .3, 0), CamRtU * PinR, CamUpU * PinR, A.Phase ? FLinearColor(1, .1f, .9f, 1) : FLinearColor(1, .9f, .1f, 1), true);
				}
			}
			if (!Vx.Num() && !bFxDrawn) return;
			bFxDrawn = Vx.Num() > 0;
			Act->Fx->CreateMeshSection_LinearColor(0, Vx, Tr, Nx, Uv, Cl, TArray<FProcMeshTangent>(), false);
			if (Act->FxMID) Act->Fx->SetMaterial(0, Act->FxMID);
		}

		void LookCam(double Dt)
		{
			if (LookGroup < 0) return;
			FGroup* G = FindGroup(LookGroup);
			AKeysDiver* D = AKeysDiver::Get(Act);
			if (!G || !G->M.IsValidIndex(LookIx) || !D || !D->IsDiving()) { LookGroup = -1; return; }
			LookT -= Dt;
			if (LookT > 0) return;
			LookT = .25;
			const FAnimal& A = G->M[LookIx];
			const FVector Eye(DiverP.X, DiverP.Y + .15, DiverP.Z), Dir = (A.P - Eye).GetSafeNormal();
			D->DebugCmd(TEXT("look"), FString::Printf(TEXT("%.4f,%.4f"), FMath::Atan2(-Dir.X, -Dir.Z), FMath::Asin(FMath::Clamp(Dir.Y, -1.0, 1.0))));
		}

		void Tick(double Dt, double RealDt)
		{
			const double T0 = FPlatformTime::Seconds();
			if (bChunkLastFrame) { if (NowT > 20) FrameAfterChunkWorst = FMath::Max(FrameAfterChunkWorst, RealDt * 1000); bChunkLastFrame = false; }   // (the first seconds are the world loading)
			ReadWorld(Dt);
			TilesThisFrame = 0;
			bBuiltMesh = false;
			TickN++;
			if (!bOff)
			{
				UpdateChunks();
				for (TUniquePtr<FGroup>& Gp : Groups)
				{
					FGroup& G = *Gp;
					const double D = RlHyp(G.Centre.X - Cam.X, G.Centre.Z - Cam.Z);
					const uint32 Every = D < 25 ? 1 : D < 50 ? 2 : 4;
					G.StepAcc += Dt;
					if ((TickN + uint32(G.Id)) % Every) continue;
					const double Sd = FMath::Min(G.StepAcc, .15);
					G.StepAcc = 0;
					StepGroup(G, Sd);
				}
				LodT -= Dt;
				if (LodT <= 0 || bLodPending) { LodT = .2; UpdateLods(); }
				WritePools();
				SightT -= Dt;
				if (SightT <= 0) { SightT = .5; FirstSights(); }
				LookCam(Dt);
				DrawFx(Dt);
			}
			const double Ms = (FPlatformTime::Seconds() - T0) * 1000;
			CpuAcc += Ms; CpuN++; CpuWorst = FMath::Max(CpuWorst, Ms);
			StatT += Dt;
			if (bScriptLog && StatT > 10) Stats();
		}

		// ================================================================ test hooks
		void Stats()
		{
			int32 Inst = 0, Tris = 0, Near = 0, SpN = 0;
			for (const FPool& Pl : PoolList) { const int32 N = Pl.Items.Num() * (Pl.bFin ? 2 : 1); Inst += N; Tris += N * Pl.Tris; }
			for (const TUniquePtr<FGroup>& Gp : Groups) for (const FAnimal& A : Gp->M) if (RlHyp(A.P.X - Centre.X, A.P.Z - Centre.Z) <= 60) Near++;
			for (const int32 C : SpAlive) SpN += C > 0 ? 1 : 0;
			Log(FString::Printf(TEXT("stats: %d species alive of %d registered, %d groups, %d individuals (%d within 60 m; cap %.0f units, %.0f used, %d groups refused by the cap, %d by a species' own cap), %d instances, %d triangles, cpu %.3f ms a frame avg (worst %.2f), %d chunks held, worst chunk %.2f ms, worst frame after a chunk %.1f ms; %d bites, %d crunches played, %d puffs, %d flees, %d chases, %d gone to sleep, %d woken, %d breaths taken, %d lift-offs, %d pits dug (sleep and wake are logged one by one within 25 m of the camera); clock %s, %s; centre %.0f, %.0f"),
				SpN, Reg->Num(), Groups.Num(), AliveCount(), Near, BudgetUnits, Units, RefusedCap, RefusedSp, Inst, Tris, CpuN ? CpuAcc / CpuN : 0.0, CpuWorst, Chunks.Num(), ChunkWorstMs, FrameAfterChunkWorst,
				Bites, Crunches, PuffN, Flees, Chases, Sleeps, Wakes, Breaths, Lifts, Digs, *KeysFeed::ClockText(Hour), bOff ? TEXT("OFF") : TEXT("on"), Centre.X, Centre.Z));
			CpuAcc = 0; CpuN = 0; CpuWorst = 0; StatT = 0;
		}

		bool Nearest(int32 SpI, int32 Phase, FGroup*& OutG, int32& OutI)
		{
			double Best = 1e18;
			OutG = nullptr;
			for (TUniquePtr<FGroup>& Gp : Groups)
			{
				if (Gp->Sp != SpI) continue;
				for (int32 i = 0; i < Gp->M.Num(); i++)
				{
					if (Phase >= 0 && Gp->M[i].Phase != Phase) continue;
					const double D = (Gp->M[i].P - Centre).SizeSquared();
					if (D < Best) { Best = D; OutG = Gp.Get(); OutI = i; }
				}
			}
			return OutG != nullptr;
		}

		void Census(double Radius)
		{
			// live: who is alive now. With a radius: what the chunks within it would deal, spawning nothing.
			static const TCHAR* Bands[] = {TEXT("0-3 m"), TEXT("3-8 m"), TEXT("8-15 m"), TEXT("15 m+")};
			auto Band = [](double D) { return D < 3 ? 0 : D < 8 ? 1 : D < 15 ? 2 : 3; };
			struct FRow { int32 Groups = 0, N[8] = {0}, ByBand[4] = {0}; TMap<uint32, int32> ByZone; };
			TArray<FRow> Rows;
			Rows.SetNum(Reg->Num());
			int32 NChunks = 0;
			double MinSp = 1e9;
			TArray<FVector2D> Homes;
			if (Radius > 0)
			{
				const int32 R = FMath::CeilToInt32(Radius / ChunkSize), IX = FMath::FloorToInt32(Centre.X / ChunkSize), IZ = FMath::FloorToInt32(Centre.Z / ChunkSize);
				for (int32 j = -R; j <= R; j++)
					for (int32 i = -R; i <= R; i++)
					{
						if (RlHyp((IX + i + .5) * ChunkSize - Centre.X, (IZ + j + .5) * ChunkSize - Centre.Z) > Radius) continue;
						NChunks++;
						TArray<FSeed> Seeds;
						DealChunk(IX + i, IZ + j, Seeds);
						for (const FSeed& Sd : Seeds)
						{
							FRow& Rw = Rows[Sd.Sp];
							Rw.Groups++; Rw.N[FMath::Min(Sd.A, 7)] += Sd.NA; if (Sd.B >= 0) Rw.N[FMath::Min(Sd.B, 7)] += Sd.NB;
							Rw.ByBand[Band(Sd.Depth)] += Sd.NA + Sd.NB; Rw.ByZone.FindOrAdd(Sd.Zone) += Sd.NA + Sd.NB;
							Homes.Add(FVector2D(Sd.Home.X, Sd.Home.Z));
						}
					}
			}
			else
				for (const TUniquePtr<FGroup>& Gp : Groups)
				{
					FRow& Rw = Rows[Gp->Sp];
					Rw.Groups++;
					for (const FAnimal& A : Gp->M) Rw.N[FMath::Min(int32(A.Phase), 7)]++;
					Rw.ByBand[Band(Gp->Depth)] += Gp->M.Num(); Rw.ByZone.FindOrAdd(Gp->Zone) += Gp->M.Num();
					Homes.Add(FVector2D(Gp->Home.X, Gp->Home.Z));
				}
			double SumNear = 0;
			for (int32 a = 0; a < Homes.Num(); a++)
			{
				double Nr = 1e9;
				for (int32 b = 0; b < Homes.Num(); b++) if (a != b) Nr = FMath::Min(Nr, FVector2D::Distance(Homes[a], Homes[b]));
				if (Nr < 1e8) { SumNear += Nr; MinSp = FMath::Min(MinSp, Nr); }
			}
			const FString Where = Radius > 0 ? FString::Printf(TEXT("dealt (not spawned) over %d chunks within %.0f m"), NChunks, Radius) : FString(TEXT("alive now"));
			Log(FString::Printf(TEXT("census at %.0f, %.0f (%s; zone here %s, depth here %.1f m), %s: %d groups, nearest-neighbour spacing of group homes mean %.1f m, least %.1f m"), Centre.X, Centre.Z, *KeysGeo::ZoneAt(Centre.X, Centre.Z),
				ZoneName(HabitatAt(Centre.X, Centre.Z).Zone), HabitatAt(Centre.X, Centre.Z).Depth, *Where, Homes.Num(), Homes.Num() > 1 ? SumNear / Homes.Num() : 0.0, MinSp < 1e8 ? MinSp : 0.0));
			for (int32 s = 0; s < Reg->Num(); s++)
			{
				const FSpecies& S = Sp(s);
				const FRow& Rw = Rows[s];
				FString Ph, Zn;
				int32 Tot = 0;
				for (int32 p = 0; p < S.Phases.Num() && p < 8; p++) { Ph += FString::Printf(TEXT("%s%d %s"), p ? TEXT(", ") : TEXT(""), Rw.N[p], *S.Phases[p].Name); Tot += Rw.N[p]; }
				for (uint32 Zb = 1; Zb <= ZHard; Zb <<= 1) Zn += FString::Printf(TEXT("%s%s %d"), Zb > 1 ? TEXT(", ") : TEXT(""), ZoneName(Zb), Rw.ByZone.Contains(Zb) ? Rw.ByZone[Zb] : 0);
				Log(FString::Printf(TEXT("census %s (%s): %d individuals in %d groups (%s); by depth: %s %d, %s %d, %s %d, %s %d; by zone: %s"), *S.Key, *S.Common, Tot, Rw.Groups, *Ph,
					Bands[0], Rw.ByBand[0], Bands[1], Rw.ByBand[1], Bands[2], Rw.ByBand[2], Bands[3], Rw.ByBand[3], *Zn));
			}
		}

		void HabProbe()
		{
			// what HabitatAt makes of the water round here: a 300 m square on a 10 m grid
			TMap<uint32, int32> Zn;
			double RMax = 0, GMax = 0, HMax = 0, RelMax = 0;
			for (int32 j = -15; j <= 15; j++)
				for (int32 i = -15; i <= 15; i++)
				{
					const FHabitat H = HabitatAt(Centre.X + i * 10, Centre.Z + j * 10);
					Zn.FindOrAdd(H.Zone)++;
					RMax = FMath::Max(RMax, H.Reef); GMax = FMath::Max(GMax, H.Grass); HMax = FMath::Max(HMax, H.Hard); RelMax = FMath::Max(RelMax, H.Relief);
				}
			FString Zs;
			for (const TPair<uint32, int32>& Z : Zn) Zs += FString::Printf(TEXT(" %s %d,"), ZoneName(Z.Key), Z.Value);
			const FHabitat Here = HabitatAt(Centre.X, Centre.Z);
			Log(FString::Printf(TEXT("hab at %.0f, %.0f: here %s depth %.1f reef %.2f hard %.2f grass %.2f relief %.2f (game zone %s); 961 points on a 300 m square:%s greatest reef %.2f hard %.2f grass %.2f relief %.2f"), Centre.X, Centre.Z,
				ZoneName(Here.Zone), Here.Depth, Here.Reef, Here.Hard, Here.Grass, Here.Relief, *KeysGeo::ZoneAt(Centre.X, Centre.Z), *Zs, RMax, HMax, GMax, RelMax));
		}

		void Cmd(const FString& In)
		{
			TArray<FString> W;
			In.Replace(TEXT("="), TEXT(" ")).Replace(TEXT(","), TEXT(" ")).ParseIntoArrayWS(W);
			if (!W.Num()) return;
			const FString& C = W[0];
			auto SpArg = [&](int32 Ix, int32& OutSp, int32& OutPhase)
			{
				OutSp = W.IsValidIndex(Ix) ? FindSpecies(W[Ix]) : -1;
				OutPhase = -1;
				if (OutSp < 0) { Log(FString::Printf(TEXT("%s: no species '%s' (registered: %s)"), *C, W.IsValidIndex(Ix) ? *W[Ix] : TEXT(""), *FString::JoinBy(*Reg, TEXT(", "), [](const FSpecies& S) { return S.Key; }))); return false; }
				if (W.IsValidIndex(Ix + 1)) for (int32 p = 0; p < Sp(OutSp).Phases.Num(); p++) if (Sp(OutSp).Phases[p].Name == W[Ix + 1]) OutPhase = p;
				return true;
			};
			if (C == TEXT("stats")) Stats();
			else if (C == TEXT("on") || C == TEXT("off"))
			{
				bOff = C == TEXT("off");
				if (bOff)
				{
					for (int32 i = Groups.Num() - 1; i >= 0; i--) DropGroup(i);
					Chunks.Reset(); Scars.Reset(); Puffs.Reset();
					WritePools();
					DrawFx(0);
				}
				Log(FString::Printf(TEXT("reef life %s"), bOff ? TEXT("OFF: nothing alive, nothing stepped") : TEXT("on")));
			}
			else if (C == TEXT("pins")) { bPins = W.IsValidIndex(1) ? FCString::Atoi(*W[1]) != 0 : !bPins; Log(FString::Printf(TEXT("pins %d (test markers over every individual: yellow phase 0, magenta the others)"), bPins ? 1 : 0)); }
			else if (C == TEXT("hab")) HabProbe();
			else if (C == TEXT("census")) Census(W.IsValidIndex(1) ? FCString::Atod(*W[1]) : 0);
			else if (C == TEXT("find"))
			{
				int32 SpI, Ph;
				if (!SpArg(1, SpI, Ph)) return;
				FGroup* G; int32 Ix = 0;
				if (!Nearest(SpI, Ph, G, Ix)) { Log(FString::Printf(TEXT("find %s: none alive within %.0f m of %.0f, %.0f"), *W[1], DropR, Centre.X, Centre.Z)); return; }
				const FAnimal& A = G->M[Ix];
				double Fl;
				const double Tp = TopAt(A.P.X, A.P.Z, &Fl);
				Log(FString::Printf(TEXT("find %s: nearest is %s, %.2f m long, at %.1f, %.1f, %.1f (%.1f m deep, %.2f m over the reef, bottom %.1f m), %.1f m from %s, %s, speed %.2f m/s, role %s, group of %d, home zone %s, lod %d"), *W[1], *Who(*G, A), A.Len,
					A.P.X, A.P.Y, A.P.Z, SurfY - A.P.Y, A.P.Y - Tp, -Fl, (A.P - Centre).Size(), bDiving ? TEXT("the diver") : TEXT("the boat"), StateName(A.State), A.V.Size(), RoleName(G->Role), G->M.Num(), ZoneName(G->Zone), int32(A.Lod)));
			}
			else if (C == TEXT("look"))
			{
				if (W.IsValidIndex(1) && W[1] == TEXT("off")) { if (FGroup* G = FindGroup(LookGroup)) G->bPosed = false; LookGroup = -1; Log(TEXT("look off")); return; }
				int32 SpI, Ph;
				if (!SpArg(1, SpI, Ph)) return;
				AKeysDiver* D = AKeysDiver::Get(Act);
				if (!D || !D->IsDiving()) { Log(TEXT("look: the diver is not in the water (dive first)")); return; }
				FGroup* G; int32 Ix = 0;
				if (!Nearest(SpI, Ph, G, Ix)) { Log(FString::Printf(TEXT("look %s: none alive near"), *W[1])); return; }
				if (FGroup* Old = FindGroup(LookGroup)) Old->bPosed = false;
				LookDist = 1.6;
				for (int32 k = 2; k < W.Num(); k++) if (W[k].IsNumeric()) LookDist = FCString::Atod(*W[k]);
				FAnimal& A = G->M[Ix];
				G->bPosed = true;
				LookGroup = G->Id; LookIx = Ix; LookT = 0;
				if (A.State != SSleep && A.State != SToSleep) { A.State = SRove; A.PoseT = 3; A.V = FVector::ZeroVector; A.Fwd = RlFlat(A.Fwd).GetSafeNormal(); if (A.Fwd.IsNearlyZero()) A.Fwd = FVector(1, 0, 0); }
				// abeam of it, on the side with the more open water
				const FVector Sd(-A.Fwd.Z, 0, A.Fwd.X);
				const FVector P1 = A.P + Sd * LookDist, P2 = A.P - Sd * LookDist;
				const bool bFlip = W.Contains(TEXT("flip"));   // 'flip': the other side (for the light)
				FVector At = (TopAt(P1.X, P1.Z) <= TopAt(P2.X, P2.Z)) != bFlip ? P1 : P2;
				if (TopAt(At.X, At.Z) > A.P.Y - .25) At = At == P1 ? P2 : P1;   // (not from inside a coral head)
				D->DebugCmd(TEXT("at"), FString::Printf(TEXT("%.3f,%.3f"), At.X, At.Z));
				D->DebugCmd(TEXT("depth"), FString::Printf(TEXT("%.3f"), -(A.P.Y - .15)));
				D->DebugCmd(TEXT("hold"), FString::Printf(TEXT("%.3f"), -(A.P.Y - .15)));
				Log(FString::Printf(TEXT("look (test camera): the diver is put %.1f m abeam of %s, %.2f m long, at %.1f, %.1f, %.1f, and its group does not shy from the diver until 'look off'"), LookDist, *Who(*G, A), A.Len, A.P.X, A.P.Y, A.P.Z));
			}
			else if (C == TEXT("do"))
			{
				FGroup* G = FindGroup(LookGroup);
				if (!G || !G->M.IsValidIndex(LookIx)) { Log(TEXT("do: nothing is being looked at (reef:look first)")); return; }
				FAnimal& A = G->M[LookIx];
				if (W.IsValidIndex(1) && W[1] == TEXT("bite")) { A.PoseT = 0; A.BiteT = 0; }
				else if (W.IsValidIndex(1) && (W[1] == TEXT("puff") || W[1] == TEXT("dig"))) { A.PoseT = 0; A.PuffT = 0; }
				else if (W.IsValidIndex(1) && W[1] == TEXT("breath")) { A.PoseT = 0; A.AirT = 0; }
				else if (W.IsValidIndex(1) && W[1] == TEXT("lift")) { A.PoseT = 0; G->bPosed = false; Scare(*G, LookIx, A.P - A.Fwd * 1.5, TEXT("the test hook asked (reef:do lift)")); }
				Log(FString::Printf(TEXT("do %s (test hook): asked of %s"), W.IsValidIndex(1) ? *W[1] : TEXT("?"), *Who(*G, A)));
			}
			else if (C == TEXT("rival"))
			{
				// bring the nearest other group whose leader is a chaser to just inside the looked-at (or nearest) male's reach
				FGroup* G = FindGroup(LookGroup);
				if (!G || Sp(G->Sp).ChaserPhase < 0 || G->M[0].Phase != Sp(G->Sp).ChaserPhase)
				{
					G = nullptr;
					double Best = 1e18;
					for (TUniquePtr<FGroup>& Gp : Groups) { const FSpecies& S = Sp(Gp->Sp); const double D = (Gp->Centre - Centre).SizeSquared(); if (S.ChaserPhase >= 0 && Gp->M[0].Phase == S.ChaserPhase && D < Best) { Best = D; G = Gp.Get(); } }
				}
				if (!G) { Log(TEXT("rival: no group led by a chasing male is alive near")); return; }
				const FSpecies& S = Sp(G->Sp);
				FGroup* R = nullptr;
				double Best = 1e18;
				for (TUniquePtr<FGroup>& Gp : Groups) { const double D = (Gp->Centre - G->Centre).SizeSquared(); if (Gp.Get() != G && Gp->Sp == G->Sp && Gp->M[0].Phase == S.ChaserPhase && D < Best) { Best = D; R = Gp.Get(); } }
				if (!R) { Log(TEXT("rival: there is no second male near to bring over")); return; }
				FVector Dir = RlFlat(R->Centre - G->M[0].P).GetSafeNormal();
				if (Dir.IsNearlyZero()) Dir = FVector(1, 0, 0);
				const FVector To = G->M[0].P + Dir * (S.ChaseR * .8), Shift = RlFlat(To - R->M[0].P);
				for (FAnimal& A : R->M) { A.P += Shift; A.P.Y = FMath::Max(A.P.Y, TopAt(A.P.X, A.P.Z) + .3); }
				R->Goal = To; R->GoalT = 8; G->ChaseCool = 0; R->ChaseCool = 30; G->bPosed = true; G->M[0].PoseT = 0;
				Log(FString::Printf(TEXT("rival (test hook): group %d and its male #%d moved %.1f m to stand %.1f m from %s; the chase that follows is the game's own rule"), R->Id, R->M[0].Id, Shift.Size(), S.ChaseR * .8, *Who(*G, G->M[0])));
			}
			else if (C == TEXT("role"))
			{
				int32 SpI, Ph;
				if (!SpArg(1, SpI, Ph) || !W.IsValidIndex(2)) return;
				SpRole[SpI] = FMath::Clamp(FCString::Atoi(*W[2]), -1, 5);
				for (TUniquePtr<FGroup>& Gp : Groups) if (Gp->Sp == SpI) { Gp->Role = SpRole[SpI] >= 0 ? ERole(SpRole[SpI]) : Sp(SpI).Role; Gp->GoalT = 0; Gp->Goal = Gp->Home; }
				Log(FString::Printf(TEXT("role (test hook): %s now behaves as %s"), *Sp(SpI).Key, SpRole[SpI] >= 0 ? RoleName(ERole(SpRole[SpI])) : TEXT("its own role")));
			}
			else if (C == TEXT("roles"))
			{
				// one line a role: how many groups, how fast they move and how high over the reef they are
				for (int32 r = 0; r < 6; r++)
				{
					int32 N = 0, NG = 0;
					double Spd = 0, Alt = 0, MaxFromHome = 0;
					for (TUniquePtr<FGroup>& Gp : Groups)
					{
						if (int32(Gp->Role) != r) continue;
						NG++;
						for (const FAnimal& A : Gp->M) { N++; Spd += A.V.Size(); Alt += A.P.Y - TopAt(A.P.X, A.P.Z); MaxFromHome = FMath::Max(MaxFromHome, RlHyp(A.P.X - Gp->Home.X, A.P.Z - Gp->Home.Z)); }
					}
					if (N) Log(FString::Printf(TEXT("roles: %s: %d groups, %d animals, mean speed %.2f m/s, mean height over the reef %.2f m, furthest from home %.1f m"), RoleName(ERole(r)), NG, N, Spd / N, Alt / N, MaxFromHome));
				}
			}
			else Log(FString::Printf(TEXT("unknown command '%s'"), *In));
		}
	};
}

// ================================================================ the actor
AKeysReefLife::AKeysReefLife()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickGroup = TG_PostPhysics;
	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent = Root;
	Fx = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("Fx"));
	Fx->SetupAttachment(Root);
	Fx->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Fx->SetCastShadow(false);
	Fx->bUseAsyncCooking = false;
}

AKeysReefLife* AKeysReefLife::Get(const UObject* WorldContext)
{
	UWorld* W = WorldContext ? WorldContext->GetWorld() : nullptr;
	if (!W) return nullptr;
	TActorIterator<AKeysReefLife> It(W);
	return It ? *It : nullptr;
}

void AKeysReefLife::BeginPlay()
{
	Super::BeginPlay();
	Impl = new KeysReefLife::FImpl();
	Impl->Act = this;
	Impl->Reg = &KeysReefLife::Registry();
	Impl->SpAlive.Init(0, Impl->Reg->Num());
	Impl->SpRole.Init(-1, Impl->Reg->Num());
	Impl->Seen.Init(false, Impl->Reg->Num());
	FString Script;
	Impl->bScriptLog = FParse::Value(FCommandLine::Get(), TEXT("KRScript="), Script, false);
	Impl->bOff = FParse::Param(FCommandLine::Get(), TEXT("KRNoReefLife"));
	// if every species filled its own cap at once the system would pass its budget: all are thinned alike
	double Sum = 0;
	for (const KeysReefLife::FSpecies& S : *Impl->Reg) Sum += S.MaxAlive * S.Weight;
	Impl->BudgetScale = Sum > KeysReefLife::BudgetUnits ? KeysReefLife::BudgetUnits / Sum : 1;
	if (UMaterialInterface* M = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/KeysRun/Materials/M_KR_Boid.M_KR_Boid"))) MID = UMaterialInstanceDynamic::Create(M, this);
	else UE_LOG(LogTemp, Warning, TEXT("KRReef: M_KR_Boid missing; no reef life will be drawn"));
	if (UMaterialInterface* BM = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/KeysRun/Materials/M_KR_Bubble.M_KR_Bubble")))
	{
		FxMID = UMaterialInstanceDynamic::Create(BM, this);
		FxMID->SetScalarParameterValue(TEXT("Bright"), .5f);
	}
	FString Keys;
	for (const KeysReefLife::FSpecies& S : *Impl->Reg) Keys += FString::Printf(TEXT(" %s (%s, %s; cap %d);"), *S.Key, *S.Common, *S.Scientific, S.MaxAlive);
	UE_LOG(LogTemp, Log, TEXT("KRReef registry: %d species:%s budget %.0f units, caps sum to %.0f, density scale %.2f%s"), Impl->Reg->Num(), *Keys, KeysReefLife::BudgetUnits, Sum, Impl->BudgetScale, Impl->bOff ? TEXT(" (-KRNoReefLife: off)") : TEXT(""));
}

void AKeysReefLife::EndPlay(const EEndPlayReason::Type Reason)
{
	delete Impl;
	Impl = nullptr;
	Super::EndPlay(Reason);
}

void AKeysReefLife::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (Impl) Impl->Tick(FMath::Min(double(DeltaSeconds), .1), DeltaSeconds);
}

void AKeysReefLife::DebugCmd(const FString& Cmd) { if (Impl) Impl->Cmd(Cmd); }

void AKeysReefLife::Startle(const FVector& P, double R, const TCHAR* Why)
{
	if (!Impl) return;
	for (TUniquePtr<KeysReefLife::FGroup>& Gp : Impl->Groups)
	{
		if ((Gp->Centre - P).Size() > R + 12) continue;
		for (int32 i = 0; i < Gp->M.Num(); i++)
		{
			const KeysReefLife::FAnimal& A = Gp->M[i];
			const double D = (A.P - P).Size();
			if (D > R || A.State == KeysReefLife::SFlee || A.State == KeysReefLife::SSleep) continue;
			Impl->Scare(*Gp, i, P, FString::Printf(TEXT("%s %.1f m from it"), Why, D));
		}
	}
}

int32 AKeysReefLife::Alive() const { return Impl ? Impl->AliveCount() : 0; }

int32 AKeysReefLife::AliveWithin(double X, double Z, double R) const
{
	int32 N = 0;
	if (Impl) for (const TUniquePtr<KeysReefLife::FGroup>& Gp : Impl->Groups) for (const KeysReefLife::FAnimal& A : Gp->M) if (FMath::Square(A.P.X - X) + FMath::Square(A.P.Z - Z) <= R * R) N++;
	return N;
}
