#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "KeysEvents.generated.h"

class UProceduralMeshComponent;
class AKeysBoatPawn;

// The two organised events from the web game, run against the AI field on this machine:
//  - the Marathon race (packages/shared/src/sim/race.ts, apps/client/src/game/race/**): two laps of nine marks down Hawk
//    Channel and back along the reef, against Reel Deal, Sand Bar and Conch Runner;
//  - the fishing tournament (sim/tournament.ts, game/tournament/**): fifteen minutes to keep one each of five species,
//    then run them in to Boot Key Harbor. First boat in wins; the AI boats' days are planned from the tournament's seed.
UCLASS()
class KEYSRUN_API AKeysEvents : public AActor
{
	GENERATED_BODY()

public:
	AKeysEvents();
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	static AKeysEvents* Get(const UObject* WorldContext);

	// ---- race (R at the helm)
	enum class ERace : uint8 { Idle, Countdown, Racing, Finished };
	struct FRacer { FString Id, Name; int32 Next = 0, Lap = 0, Place = 0; double FinishT = -1, X = 0, Z = 0; };
	struct FMark { FString Name; double X, Z; };
	void ToggleRace();
	bool IsRacing() const { return Race != ERace::Idle; }
	ERace RacePhase() const { return Race; }
	double RaceClock() const { return Clock; }
	static constexpr int32 RaceLaps = 2;
	const TArray<FMark>& Marks() const { return Course; }
	const FRacer* Me() const { return Racers.Num() ? &Racers[0] : nullptr; }
	void Standings(TArray<const FRacer*>& Out) const;
	double CourseKm() const { return PathLen * RaceLaps / 1000; }
	void OpponentDots(TArray<FVector2D>& Out) const;

	// ---- tournament (pause menu)
	enum class ETourney : uint8 { Off, Countdown, Live, Ceremony, Over };
	struct FEntry { FString Name; int32 Caught; bool bHome, bWon, bMe; };
	void ToggleTourney();
	ETourney TourneyPhase() const;
	bool InTourney() const { return bTourney; }
	const TArray<FString>& TourneySpecies() const { return TSpecies; }
	int32 MyMask() const { return Mask; }
	double TimeLeft() const;
	double CountdownLeft() const;
	void TourneyStandings(TArray<FEntry>& Out) const;
	FString WinnerName() const;
	bool bWinnerMe = false;
	// a fish went in the cooler: does it tick one of the five off?
	void OnKeep(const FString& Key, double Weight);
	static double HarbourX(), HarbourZ();
	static constexpr double HarbourR = 90;
	FString DebugCmd(const FString& Cmd);   // -KRScript ev:...
	FString FirstSpecies() const { return TSpecies.Num() ? TSpecies[0] : FString(); }

private:
	AKeysBoatPawn* Pawn() const;
	// race
	void BuildCourse();
	void PathAt(double S, double& OX, double& OZ, double& OTX, double& OTZ) const;
	double CurvatureAt(double S) const;
	void StepRace(double Dt);
	void UpdateField(double Dt, double T, bool bReleased);
	void UpdateMarks(double T);
	void LineUp();
	void SetRaceVisible(bool bVis);
	void Say(const FString& Msg, double Secs = 3.2);
	void Sound(const TCHAR* Id);
	ERace Race = ERace::Idle;
	double Clock = 0;
	int32 Finished = 0;
	TArray<FMark> Course;
	TArray<FRacer> Racers;                 // [0] is you
	TArray<FVector2D> PathPts;             // the AI's line round the course, closed
	TArray<double> PathCum;
	double PathLen = 1;
	struct FOpp { double S = 0, Speed = 0, Pace = 1, Line = 0, CurLine = 0, H = 0, WakeT = 0; FString Boat; };
	TArray<FOpp> Opps;
	UPROPERTY() TArray<TObjectPtr<UProceduralMeshComponent>> OppMeshes;
	UPROPERTY() TArray<TObjectPtr<UProceduralMeshComponent>> MarkMeshes;
	UPROPERTY() TObjectPtr<UProceduralMeshComponent> NextMesh;   // the beacon over the mark you're heading for
	UPROPERTY() TObjectPtr<USceneComponent> Root;

	// tournament
	bool bTourney = false;
	uint32 Seed = 0;
	double T0 = 0;                          // game time lines go in
	TArray<FString> TSpecies;
	int32 Mask = 0;
	TArray<double> MyW;
	bool bWon = false;
	FString WinnerId;                       // "" none yet, "me", or "npc0".."npc2"
	double WinAt = 0;                       // seconds after the start
	ETourney LastPhase = ETourney::Off;
	int32 NpcSeen[3] = {0, 0, 0};
	bool bAnnouncedAll = false;
	struct FPlan { TArray<double> CatchAt; double HomeAt = 0; };
	FPlan Plan(int32 I) const;
	void StepTourney();
	int32 NpcMask(int32 I, double El) const;
};
