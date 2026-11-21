// The dry stack's forklift: its script of moves and its mesh. See KeysForklift.h.
#include "KeysForklift.h"
#include "KeysMesh.h"
#include "KeysYardProps.h"

namespace
{
	double AngDiff(double From, double To) { return FMath::Fmod(FMath::Fmod(To - From + PI, 2 * PI) + 2 * PI, 2 * PI) - PI; }
	// ease toward a stop: as fast as VMax, slowing over the last stretch so it arrives without a jerk
	double EaseSpeed(double Remaining, double VMax, double Decel) { return FMath::Min(VMax, FMath::Sqrt(2 * Decel * FMath::Max(0.0, Remaining)) + .04); }
}

void FKeysForklift::Park()
{
	X = AisleX; Y = ParkY; Yaw = ParkYaw; ForkZ = CarryZ; Speed = Turn = ForkV = 0;
	Steps.Reset(); StepI = 0; StepT = 0; Job = 0; Target = -1; bEngaged = false;
	for (FBoat& B : Boats)
	{
		if (B.State == 2) continue;
		B.State = 0; B.Float = 0;
		B.X = FaceX - .3 - B.L / 2; B.Y = B.SlotY; B.Z = B.SlotZ; B.Yaw = PI;
	}
}

void FKeysForklift::Along(double ToY)
{
	if (FMath::Abs(ToY - Y) < .3 && Steps.Num() == 0) return;
	// the caller knows where it will be standing; face that way down the aisle, then go
	Steps.Add({OpTurn, 99, ToY});   // A = 99: face toward Y = B from wherever it is then
	Drive(AisleX, ToY);
}

void FKeysForklift::StartLaunch(int32 Boat)
{
	if (!Boats.IsValidIndex(Boat) || Boats[Boat].State != 0) return;
	const FBoat& B = Boats[Boat];
	Steps.Reset(); StepI = 0; StepT = 0; Job = 1; Target = Boat; bEngaged = false;
	const double InX = FaceX - .3 + ForkX0 + SternGap;
	Along(B.SlotY);
	TurnTo(PI);
	Fork(B.SlotZ - B.Seat - .32);
	Drive(InX, B.SlotY);
	Steps.Add({OpEngage, 0, 0});
	Fork(B.SlotZ - B.Seat + .42);
	Wait(.6);
	Drive(AisleX, B.SlotY);
	Fork(CarryZ);
	if (FMath::Abs(B.SlotY - WellY) > .5) Along(WellY);
	TurnTo(0);
	Drive(WellX, WellY);
	Wait(.8);
	Fork(LowZ);
	Steps.Add({OpRelease, 0, 0});
	Wait(1.0);
	Shift(BerthDY);
	Wait(.6);
	Fork(CarryZ);
	Drive(AisleX, WellY);
	Along(ParkY);
	TurnTo(ParkYaw);
}

void FKeysForklift::StartFetch(int32 Boat)
{
	if (!Boats.IsValidIndex(Boat) || Boats[Boat].State != 2) return;
	const FBoat& B = Boats[Boat];
	Steps.Reset(); StepI = 0; StepT = 0; Job = 2; Target = Boat; bEngaged = false;
	const double InX = FaceX - .3 + ForkX0 + SternGap;
	Along(WellY);
	TurnTo(0);
	Drive(WellX, WellY);
	Wait(.5);
	Fork(LowZ);
	Shift(0);
	Wait(.8);
	Steps.Add({OpEngage, 0, 0});
	Fork(CarryZ);
	Wait(1.5);   // let her drain
	Drive(AisleX, WellY);
	if (FMath::Abs(B.SlotY - WellY) > .5) Along(B.SlotY);
	TurnTo(PI);
	Fork(B.SlotZ - B.Seat + .42);
	Drive(InX, B.SlotY);
	Wait(.5);
	Fork(B.SlotZ - B.Seat - .32);
	Steps.Add({OpRelease, 0, 0});
	Drive(AisleX, B.SlotY);
	Fork(CarryZ);
	Along(ParkY);
	TurnTo(ParkYaw);
}

double FKeysForklift::InnerMastZ() const
{
	// the inner mast stays down until the carriage runs out of free lift, then rises with it; for a negative lift
	// it slides down past the apron with the carriage
	if (ForkZ > FreeLift) return ForkZ - FreeLift;
	if (ForkZ < .2) return ForkZ - .2;
	return 0;
}

void FKeysForklift::PlaceBoats(double FloatZ)
{
	for (int32 i = 0; i < Boats.Num(); i++)
	{
		FBoat& B = Boats[i];
		const bool bMine = bEngaged && i == Target;
		const double OnFork = ForkZ + B.Seat;
		if (bMine)
		{
			// is she over the water (the well) or over her bunks?
			const bool bWell = FMath::Abs(Yaw) < .5 && X > AisleX + 4;
			const double Rest = bWell ? FloatZ : B.SlotZ;
			const bool bAtRest = bWell ? FMath::Abs(X - WellX) < .05 : FMath::Abs(X - (FaceX - .3 + ForkX0 + SternGap)) < .05;
			if (bAtRest && OnFork <= Rest)
			{
				B.State = bWell ? 2 : 0;
				B.Z = Rest;
				B.Float = bWell ? FMath::Clamp((Rest - OnFork) / .35, 0.0, 1.0) : 0;
			}
			else
			{
				B.State = 1;
				B.Z = OnFork;
				B.Float = 0;
			}
			const double D = ForkX0 + SternGap + B.L / 2;
			B.X = X + FMath::Cos(Yaw) * D; B.Y = Y + FMath::Sin(Yaw) * D; B.Yaw = Yaw;
		}
		else if (B.State == 2) { B.Z = FloatZ; B.Float = 1; }
		else if (B.State == 0) B.Z = B.SlotZ;
	}
}

void FKeysForklift::Tick(double Dt, double FloatZ)
{
	Dt *= Rate;
	if (Job == 0)
	{
		Speed = Turn = ForkV = 0;
		IdleT -= Dt;
		if (IdleT <= 0 && Boats.Num())
		{
			if (Afloat >= 0) StartFetch(Afloat);
			else
			{
				for (int32 k = 0; k < Boats.Num(); k++) { const int32 i = (NextBoat + k) % Boats.Num(); if (Boats[i].State == 0) { StartLaunch(i); NextBoat = i + 1; break; } }
			}
			if (Job == 0) IdleT = 10;
		}
		PlaceBoats(FloatZ);
		return;
	}
	if (!Steps.IsValidIndex(StepI))
	{
		// the job is done
		if (Job == 1) { Afloat = Target; IdleT = 26; }
		else { Afloat = -1; IdleT = 20; Cycles++; }
		Job = 0; Target = -1; bEngaged = false;
		PlaceBoats(FloatZ);
		return;
	}
	const bool bLoad = Loaded();
	FStep& S = Steps[StepI];
	bool bDone = false;
	switch (S.Op)
	{
	case OpDrive:
	{
		const double DX = S.A - X, DY = S.B - Y, Rem = FMath::Sqrt(DX * DX + DY * DY);
		if (Rem < .004) { X = S.A; Y = S.B; Speed = 0; bDone = true; break; }
		const double Dir = DX * FMath::Cos(Yaw) + DY * FMath::Sin(Yaw) >= 0 ? 1 : -1;
		const double VMax = (bLoad ? 1.9 : 3.0) * (Dir < 0 ? .8 : 1) * (ForkZ > 2.5 || ForkZ < .5 || X < FaceX + 7 ? .5 : 1);
		const double Want = EaseSpeed(Rem, VMax, .9);
		const double V = FMath::Min(Want, FMath::Abs(Speed) + 1.1 * Dt);
		const double Step = FMath::Min(Rem, V * Dt);
		X += DX / Rem * Step; Y += DY / Rem * Step;
		Speed = Dir * V;
		break;
	}
	case OpTurn:
	{
		if (S.A > 50) { S.A = S.B >= Y ? PI / 2 : -PI / 2; if (FMath::Abs(S.B - Y) < .05) { bDone = true; break; } }
		const double D = AngDiff(Yaw, S.A);
		if (FMath::Abs(D) < .002) { Yaw = S.A; Turn = 0; bDone = true; break; }
		const double Want = EaseSpeed(FMath::Abs(D), bLoad ? .42 : .6, .32);
		const double W = FMath::Min(Want, FMath::Abs(Turn) + .5 * Dt);
		Yaw += FMath::Sign(D) * FMath::Min(FMath::Abs(D), W * Dt);
		Turn = FMath::Sign(D) * W;
		break;
	}
	case OpFork:
	{
		const double D = S.A - ForkZ;
		if (FMath::Abs(D) < .003) { ForkZ = S.A; ForkV = 0; bDone = true; break; }
		const double Want = EaseSpeed(FMath::Abs(D), D > 0 ? (bLoad ? .62 : .85) : .9, .7);
		const double V = FMath::Min(Want, FMath::Abs(ForkV) + 1.2 * Dt);
		ForkZ += FMath::Sign(D) * FMath::Min(FMath::Abs(D), V * Dt);
		ForkV = FMath::Sign(D) * V;
		break;
	}
	case OpEngage: bEngaged = true; bDone = true; break;
	case OpRelease:
		PlaceBoats(FloatZ);
		bEngaged = false;
		if (Boats.IsValidIndex(Target) && Boats[Target].State == 1) Boats[Target].State = 0;   // never leave one hanging
		bDone = true;
		break;
	case OpShift:
	{
		if (!Boats.IsValidIndex(Target) || Boats[Target].State != 2) { bDone = true; break; }
		FBoat& Bt = Boats[Target];
		const double D = WellY + S.A - Bt.Y;
		if (FMath::Abs(D) < .004) { Bt.Y = WellY + S.A; bDone = true; break; }
		S.B = FMath::Min(EaseSpeed(FMath::Abs(D), .55, .12), S.B + .25 * Dt);
		Bt.Y += FMath::Sign(D) * FMath::Min(FMath::Abs(D), S.B * Dt);
		break;
	}
	default:
		StepT += Dt;
		bDone = StepT >= S.A;
		break;
	}
	if (bDone) { StepI++; StepT = 0; }
	Yaw = FMath::Fmod(Yaw + 3 * PI, 2 * PI) - PI;
	PlaceBoats(FloatZ);
}

FString FKeysForklift::Describe() const
{
	static const TCHAR* Ops[] = {TEXT("drive"), TEXT("turn"), TEXT("fork"), TEXT("engage"), TEXT("release"), TEXT("wait"), TEXT("shift")};
	FString S = FString::Printf(TEXT("job %d step %d/%d %s pos %.2f %.2f yaw %.2f fork %.2f speed %.2f target %d afloat %d cycles %d idle %.0f"), Job, StepI, Steps.Num(),
		Steps.IsValidIndex(StepI) ? Ops[Steps[StepI].Op] : TEXT("-"), X, Y, Yaw, ForkZ, Speed, Target, Afloat, Cycles, IdleT);
	if (Boats.IsValidIndex(Target)) S += FString::Printf(TEXT(" boat state %d at %.2f %.2f %.2f"), Boats[Target].State, Boats[Target].X, Boats[Target].Y, Boats[Target].Z);
	return S;
}

// ---------------------------------------------------------------- the machine
void FKeysForklift::BuildBody(FKeysMesh& M)
{
	using namespace KeysYardProps;
	const FLinearColor Yel = Hex(0xe3ac1c), Dark = Hex(0x2a2d31), Tire = Hex(0x17181a), Grey = Hex(0x6d747b), Steel = Hex(0xb4b9bd);
	// the drive wheels under the mast and the smaller steer wheels at the back
	for (int32 a = -1; a <= 1; a += 2)
	{
		M.Surf = SurfRubber;
		Wheel(M, FVector(0, a * 1.0, .85), FVector(0, a * 1.75, .85), .85, Tire, 18);
		Wheel(M, FVector(-3.5, a * .75, .6), FVector(-3.5, a * 1.25, .6), .6, Tire, 14);
		M.Surf = SurfMetalPaint;
		Wheel(M, FVector(0, a * 1.72, .85), FVector(0, a * 1.78, .85), .4, Yel, 10);
		Wheel(M, FVector(-3.5, a * 1.22, .6), FVector(-3.5, a * 1.28, .6), .28, Yel, 10);
		// fenders over the drive wheels
		Box(M, 0, a * 1.38, 1.82, 1.5, .8, .1, Yel);
		Box(M, -.78, a * 1.38, 1.5, .1, .8, .7, Yel);
	}
	// the frame, the engine cover and the counterweight
	M.Surf = SurfMetalPaint;
	M.AddRoundBox(FVector(-1.55, 0, 1.0), FVector(4.6, 1.9, .9), .08, Dark);
	M.AddRoundBox(FVector(-2.55, .3, 1.85), FVector(2.3, 1.5, .9), .1, Yel);
	M.AddRoundBox(FVector(-4.05, 0, 1.45), FVector(1.25, 2.3, 1.7), .22, Grey);
	Box(M, -4.69, 0, 1.2, .03, 1.5, .5, Hex(0x15171a));
	for (int32 a = -1; a <= 1; a += 2) Box(M, -4.69, a * .95, 1.75, .03, .2, .14, Hex(0xb8322a));
	// grille slats down the side of the engine cover
	for (int32 k = 0; k < 5; k++) Box(M, -2.9 + k * .22, 1.06, 1.85, .08, .02, .6, Dark);
	// the exhaust
	M.Surf = SurfSteel;
	M.AddTube(FVector(-3.2, .8, 2.3), FVector(-3.2, .8, 3.9), .07, .07, 8, Hex(0x3a3d40));
	// the cab, up on the left where the driver can see past a hull: floor, corner posts, roof with a guard, glass
	M.Surf = SurfMetalPaint;
	const FVector Seat = DriverSeat();
	const double Cx = Seat.X + .15, Cy = Seat.Y, Fz = Seat.Z - .06;
	Box(M, Cx, Cy, Fz - .5, 1.5, 1.25, 1.0, Yel);
	Box(M, Cx, Cy, Fz, 1.7, 1.35, .08, Dark);
	for (int32 a = -1; a <= 1; a += 2) for (int32 b = -1; b <= 1; b += 2) Box(M, Cx + a * .78, Cy + b * .6, Fz + .95, .07, .07, 1.9, Dark);
	M.AddRoundBox(FVector(Cx, Cy, Fz + 1.94), FVector(1.9, 1.5, .1), .04, Yel);
	for (int32 k = -2; k <= 2; k++) Box(M, Cx + k * .36, Cy, Fz + 2.02, .05, 1.4, .05, Dark);
	Box(M, Cx - .78, Cy, Fz + .5, .05, 1.2, .9, Dark);
	M.Surf = SurfGlass;
	Box(M, Cx - .78, Cy, Fz + 1.4, .02, 1.14, .8, Hex(0x1b232b));
	// the seat, the wheel on its column and the lever bank
	M.Surf = SurfVinyl;
	M.AddRoundBox(FVector(Seat.X - .05, Seat.Y, Seat.Z + .4), FVector(.5, .5, .12), .05, Hex(0x1c1c1e));
	M.AddRoundBox(FVector(Seat.X - .3, Seat.Y, Seat.Z + .78), FVector(.12, .5, .66), .05, Hex(0x1c1c1e));
	M.Surf = SurfMetalPaint;
	M.AddTube(FVector(Seat.X + .62, Seat.Y, Seat.Z + .1), FVector(Seat.X + .46, Seat.Y, Seat.Z + .78), .035, .035, 6, Dark);
	M.AddTube(FVector(Seat.X + .46, Seat.Y, Seat.Z + .78), FVector(Seat.X + .43, Seat.Y, Seat.Z + .81), .2, .2, 12, Hex(0x15171a));
	for (int32 k = 0; k < 3; k++) M.AddTube(FVector(Seat.X + .45, Seat.Y + .32 + k * .09, Seat.Z + .2), FVector(Seat.X + .5, Seat.Y + .32 + k * .09, Seat.Z + .62), .014, .014, 4, Dark);
	// a ladder up the side, the amber beacon and two work lights
	Ladder(M, FVector(Cx + .2, Cy - .72, .5), FVector(Cx + .2, Cy - .7, Fz));
	M.Surf = SurfGlass;
	M.AddTube(FVector(Cx - .5, Cy, Fz + 2.0), FVector(Cx - .5, Cy, Fz + 2.2), .08, .06, 8, Hex(0xf08a1c));
	for (int32 a = -1; a <= 1; a += 2) Box(M, Cx + .94, Cy + a * .55, Fz + 1.86, .08, .2, .14, Hex(0xf4f1dc));
	// the outer mast: two tall channels tied across, leaning on their tilt rams
	M.Surf = SurfMetalPaint;
	for (int32 a = -1; a <= 1; a += 2)
	{
		Box(M, 1.34, a * .8, MastH / 2 + .12, .22, .2, MastH - .24, Dark);
		M.Surf = SurfSteel;
		M.AddTube(FVector(.2, a * .8, 1.5), FVector(1.25, a * .8, 2.7), .07, .07, 8, Steel);
		M.Surf = SurfMetalPaint;
	}
	for (double Zt : {.5, 3.0, 5.6, MastH - .1}) Box(M, 1.26, 0, Zt, .12, 1.8, .22, Dark);
	Box(M, .95, 0, .75, .7, 1.9, .5, Dark);
	M.Surf = SurfDefault;
}

void FKeysForklift::BuildInnerMast(FKeysMesh& M)
{
	using namespace KeysYardProps;
	const FLinearColor Dark = Hex(0x33373c), Steel = Hex(0xc4c9cd);
	M.Surf = SurfMetalPaint;
	for (int32 a = -1; a <= 1; a += 2) Box(M, 1.52, a * .58, MastH / 2 + .2, .16, .16, MastH - .2, Dark);
	Box(M, 1.52, 0, MastH + .2, .18, 1.34, .2, Dark);
	Box(M, 1.52, 0, .5, .14, 1.2, .16, Dark);
	// the lift ram and the chains
	M.Surf = SurfSteel;
	M.AddTube(FVector(1.5, 0, .4), FVector(1.5, 0, MastH), .075, .075, 8, Steel);
	for (int32 a = -1; a <= 1; a += 2) M.AddTube(FVector(1.56, a * .22, .6), FVector(1.56, a * .22, MastH + .1), .02, .02, 4, Hex(0x3a3d40));
	M.Surf = SurfDefault;
}

void FKeysForklift::BuildForks(FKeysMesh& M)
{
	using namespace KeysYardProps;
	const FLinearColor Dark = Hex(0x2a2d31), Yel = Hex(0xe3ac1c), Pad = Hex(0x16171a);
	// the carriage: a frame of two bars and uprights riding the inner mast, with a load backrest
	M.Surf = SurfMetalPaint;
	for (double Zc : {.25, 1.0}) Box(M, 1.76, 0, Zc, .2, 2.1, .22, Dark);
	for (int32 a = -1; a <= 1; a += 2) Box(M, 1.72, a * .58, .62, .2, .14, 1.1, Dark);
	for (int32 k = -2; k <= 2; k++) Box(M, 1.84, k * .42, 1.65, .05, .05, 1.1, Dark);
	Box(M, 1.84, 0, 2.2, .05, 1.74, .06, Dark);
	// the forks: a shank down the carriage, then a long tine tapering to its tip, padded on top so it doesn't mark a hull
	for (int32 a = -1; a <= 1; a += 2)
	{
		const double Yf = a * ForkHalf;
		M.Surf = SurfMetalPaint;
		Box(M, ForkX0 - .02, Yf, .4, .16, .2, 1.0, Yel);
		const double X1 = ForkX0 + ForkLen;
		// tine: 0.13 thick at the heel, 0.05 at the tip
		const FVector A0(ForkX0 - .1, Yf - .11, -.03), A1(ForkX0 - .1, Yf + .11, -.03), B0(X1, Yf - .09, -.03), B1(X1, Yf + .09, -.03);
		const FVector C0(ForkX0 - .1, Yf - .11, -.16), C1(ForkX0 - .1, Yf + .11, -.16), D0(X1, Yf - .09, -.08), D1(X1, Yf + .09, -.08);
		M.AddQuad(C0, C1, D1, D0, Yel, FVector(0, 0, -1));
		M.AddQuad(A0, B0, D0, C0, Yel, FVector(0, -1, 0));
		M.AddQuad(A1, C1, D1, B1, Yel, FVector(0, 1, 0));
		M.AddQuad(B0, B1, D1, D0, Yel, FVector(1, 0, 0));
		M.Surf = SurfRubber;
		M.AddQuad(A0 + FVector(0, 0, .03), A1 + FVector(0, 0, .03), B1 + FVector(0, 0, .03), B0 + FVector(0, 0, .03), Pad, FVector(0, 0, 1));
		M.AddQuad(A0, A0 + FVector(0, 0, .03), B0 + FVector(0, 0, .03), B0, Pad, FVector(0, -1, 0));
		M.AddQuad(A1, B1, B1 + FVector(0, 0, .03), A1 + FVector(0, 0, .03), Pad, FVector(0, 1, 0));
	}
	M.Surf = SurfDefault;
}
