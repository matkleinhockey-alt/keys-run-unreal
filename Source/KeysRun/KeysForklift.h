#pragma once

#include "CoreMinimal.h"

struct FKeysMesh;

// The dry stack's marina forklift: a big negative-lift machine with long padded forks. It works a script of small
// steps (drive, turn on the spot, raise or lower the forks, wait), so everything it does is one continuous move: it
// drives down the aisle to a boat's bay, turns to face the rack, raises the forks, runs them in under the hull, lifts
// her off the bunks, backs out, carries her low to the bulkhead and lowers her over the edge until she floats in the
// launch well, where she is walked along to the float; later she is walked back over the sunk forks and it lifts her
// out and puts her back on her bunks.
//   Everything here is in the yard's flat frame, metres: X and Y on the apron, heights above the apron. The machine's
// origin is on the ground under the middle of its front axle, +X the way the forks point. A boat is carried, on its
// bunks or afloat according to which of them holds it higher, so it never jumps from one to the other.
struct FKeysForklift
{
	// the machine
	static constexpr double ForkX0 = 1.9;      // the fork heels (the carriage's face)
	static constexpr double ForkLen = 6.6;
	static constexpr double ForkHalf = .62;    // each fork's centreline off the middle
	static constexpr double SternGap = 1.15;   // carriage face to a carried boat's transom (room for her outboards)
	static constexpr double FreeLift = 5.6;    // the carriage's travel on the inner mast
	static constexpr double MastH = 8.2;
	static constexpr double CarryZ = 1.25;     // the forks' height on the move

	// the yard (set by AKeysYards)
	double AisleX = 0, FaceX = 0;              // the lane it drives, and the rack's open face (the sterns sit just inside)
	double ParkY = 0, ParkYaw = 0;
	double WellX = 0, WellY = 0;               // where its front axle stops at the bulkhead, facing +X
	double LowZ = -3.4;                        // the forks' lowest (below the apron, over the bulkhead, well under a floating hull)
	double BerthDY = -4.6;                     // a launched boat is walked this far along the bulkhead to lie at the float, clear of the forks

	struct FBoat
	{
		double L = 8, Seat = .27;              // length; her waterline above the top of the forks when carried
		double SlotY = 0, SlotZ = 0;           // her bay, and her waterline's height when she sits on her bunks
		int32 State = 0;                       // 0 on her bunks, 1 on the forks, 2 afloat
		double X = 0, Y = 0, Z = 0, Yaw = PI;  // where her origin is now
		double Float = 0;                      // 0 held, 1 floating free (for the bob)
	};
	TArray<FBoat> Boats;

	// pose and motion
	double X = 0, Y = 0, Yaw = 0, ForkZ = CarryZ, Speed = 0, Turn = 0, ForkV = 0;
	enum EOp : uint8 { OpDrive, OpTurn, OpFork, OpEngage, OpRelease, OpWait, OpShift };
	struct FStep { uint8 Op = OpWait; double A = 0, B = 0; };
	TArray<FStep> Steps;
	int32 StepI = 0;
	double StepT = 0;
	int32 Job = 0;            // 0 parked, 1 launching, 2 fetching
	int32 Target = -1;        // the boat this job is about
	bool bEngaged = false;    // the forks are under her
	int32 Afloat = -1;        // the boat in the well
	int32 NextBoat = 0;
	double IdleT = 14;        // until the next job
	double Rate = 1;
	int32 Cycles = 0;

	void Park();
	void StartLaunch(int32 Boat);
	void StartFetch(int32 Boat);
	// FloatZ: the height a boat's waterline floats at in the well right now (above the apron: negative)
	void Tick(double Dt, double FloatZ);
	double InnerMastZ() const;
	bool Loaded() const { return Target >= 0 && Boats.IsValidIndex(Target) && Boats[Target].State == 1; }
	bool Reversing() const { return Speed < -.05; }
	FString Describe() const;

	// the fixed machine (with the outer mast), the inner mast that telescopes, and the carriage with its forks; the
	// last two are built with their own origin at the apron / the top of the forks
	static void BuildBody(FKeysMesh& M);
	static void BuildInnerMast(FKeysMesh& M);
	static void BuildForks(FKeysMesh& M);
	// where the driver sits (feet) in the machine's frame
	static FVector DriverSeat() { return FVector(-1.3, -.72, 2.42); }

private:
	void Drive(double TX, double TY) { Steps.Add({OpDrive, TX, TY}); }
	void TurnTo(double A) { Steps.Add({OpTurn, A, 0}); }
	void Fork(double Z) { Steps.Add({OpFork, Z, 0}); }
	void Wait(double S) { Steps.Add({OpWait, S, 0}); }
	void Shift(double DY) { Steps.Add({OpShift, DY, 0}); }   // walk the floating boat to WellY + DY
	void Along(double ToY);   // turn to face along the aisle toward ToY and drive there
	void PlaceBoats(double FloatZ);
};
