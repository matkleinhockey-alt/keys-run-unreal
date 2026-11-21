// Boat lifts for the private docks of the real map (KeysDocks.cpp says where they stand and what is on them).
//  - the cradle lift: four piles, a top beam down each side with its motor, gearbox and drive pipe, four cables down
//    to a cradle of two cross beams carrying carpeted bunks, and guide posts to steer the boat on
//  - the elevator lift: two rails raked down a seawall (or a dock's edge) into the water, a carriage with cantilever
//    arms and bunks riding them, the motor and drive pipe across the top
//  - davits: two pipe cranes on the edge with falls down to a skiff's bow and stern
// A boat that is hung clears a high tide; a lift that is down goes into the "Float" part, which the docks draw at
// the tide. Everything is built in the lift's own frame (X toward the bow, Y to starboard, Z up from the datum).
#include "KeysLifts.h"

using namespace KeysYardProps;

namespace KeysLifts
{
	namespace
	{
		FVector P3(double X, double Y, double Z) { return FVector(X, Y, Z); }
		const uint32 AluC = 0xc4c9cd, AluDarkC = 0x959ca1, CableC = 0x3a3d40, CarpetC = 0x2f3640, PvcC = 0xeef0f1;

		// two carpeted bunks along the boat, on brackets off the cross members at X = +-CX; BeamTop is the top of those
		// members, BunkTop where the hull sits
		void Bunks(FKeysMesh& M, const FBoat& B, double CX, double BeamTop, double BunkTop, double Len)
		{
			const double Off = FMath::Min(.8, B.B * .3);
			for (int32 b = -1; b <= 1; b += 2)
			{
				M.Surf = SurfSteel;
				for (int32 a = -1; a <= 1; a += 2)
					M.AddBox(P3(a * CX, b * Off, (BeamTop + BunkTop - .09) / 2), FVector(.07, .09, FMath::Max(.04, BunkTop - .09 - BeamTop)), Hex(AluDarkC));
				M.Surf = SurfCloth;
				// laid over to the deadrise, so the hull lands on the flat of the board
				M.AddBox(P3(0, b * Off, BunkTop - .05), FVector(Len, .24, .09), Hex(CarpetC), FQuat(FVector::XAxisVector, b * .2));
			}
			M.Surf = SurfDefault;
		}

		// the motor on the end of a drive pipe: gearbox, motor and its white cover
		void Motor(FKeysMesh& S, const FVector& At, const FVector& Along)
		{
			const FQuat Q = FRotationMatrix::MakeFromXZ(Along, FVector::UpVector).ToQuat();
			S.Surf = SurfMetalPaint;
			S.AddBox(At, FVector(.3, .2, .26), Hex(AluDarkC), Q);
			S.Surf = SurfGelcoat;
			S.AddRoundBox(At - Along * .3 + FVector(0, 0, .02), FVector(.36, .26, .3), .05, Hex(0xf1f2f0), Q);
			S.Surf = SurfDefault;
		}

		void Cradle(const FLift& L, const FParts& O)
		{
			const FBoat& B = L.Boat;
			FKeysMesh& S = *O.Structure;
			FKeysMesh& D = *O.Detail;
			const double PA = FMath::Clamp(B.L * .52, 3.3, 4.7), PW = B.B + 1.25, CX = PA / 2 - .55;
			const double Wl = HungWaterline(B);
			const double Top = FMath::Max(L.DeckZ + 2.0, Wl + SheerZ(B) + .35) + .25 * Hash(L.Seed, 3.1);
			for (int32 a = -1; a <= 1; a += 2) for (int32 b = -1; b <= 1; b += 2) Pile(S, a * PA / 2, b * PW / 2, Top, .16, L.PileC, 0, L.bSleeves);
			// a beam down each side on the pile heads, the drive pipe along it in its bearings, the motor at the stern end
			for (int32 b = -1; b <= 1; b += 2)
			{
				const double Y = b * PW / 2;
				S.Surf = SurfSteel;
				Beam(S, P3(-PA / 2 - .55, Y, Top + .13), P3(PA / 2 + .55, Y, Top + .13), .15, .26, Hex(AluC));
				S.AddTube(P3(-PA / 2 - .5, Y, Top + .34), P3(PA / 2 + .45, Y, Top + .34), .042, .042, 6, Hex(AluDarkC));
				Motor(S, P3(-PA / 2 - .62, Y, Top + .4), FVector(1, 0, 0));
				D.Surf = SurfSteel;
				for (int32 a = -1; a <= 1; a += 2)
				{
					D.AddBox(P3(a * PA / 2, Y, Top + .31), FVector(.1, .13, .1), Hex(AluDarkC));
					Wheel(D, P3(a * CX - .1, Y, Top + .34), P3(a * CX + .1, Y, Top + .34), .085, Hex(AluDarkC), 8);   // the cable's winder
				}
			}
			S.Surf = SurfDefault;
			D.Surf = SurfDefault;
			// the cradle: up with the boat (or empty), or down at the tide
			const bool bDown = L.Down();
			FKeysMesh& Cs = bDown ? *O.Float : S;
			FKeysMesh& Cd = bDown ? *O.Float : D;
			const double BoatWl = bDown ? 0 : Wl;
			const double Off = FMath::Min(.8, B.B * .3);
			// (lowered under a boat it hangs a hand's breadth clear of her; lowered empty it lies a little deeper)
			const double BunkTop = BoatWl + BottomZ(B, Off) - (L.State == StateAfloat ? .28 : L.State == StateEmptyDown ? .4 : 0);
			const double BeamTop = BunkTop - (BottomZ(B, Off) - KeelZ(B)) - .1;
			Cs.Surf = SurfSteel;
			for (int32 a = -1; a <= 1; a += 2) Beam(Cs, P3(a * CX, -PW / 2 + .02, BeamTop - .11), P3(a * CX, PW / 2 - .02, BeamTop - .11), .13, .22, Hex(AluC));
			Bunks(Cs, B, CX, BeamTop, BunkTop, PA + 1.3);
			Cd.Surf = SurfVinyl;
			for (int32 b = -1; b <= 1; b += 2) Cd.AddTube(P3(-CX, b * (B.B / 2 + .2), BeamTop), P3(-CX, b * (B.B / 2 + .2), BeamTop + (bDown ? 2.3 : 1.7)), .045, .045, 6, Hex(PvcC));
			Cd.Surf = SurfSteel;
			for (int32 a = -1; a <= 1; a += 2)
				for (int32 b = -1; b <= 1; b += 2)
				{
					const FVector Foot = P3(a * CX, b * (PW / 2 - .02), BeamTop - .02);
					if (!bDown) { Cd.AddTube(Foot, P3(Foot.X, Foot.Y, Top), .012, .012, 4, Hex(CableC)); continue; }
					// down: the cable rides the tide with the cradle, and a length of it hangs fixed from the beam, so the two
					// always overlap whatever the water is doing
					Cd.AddTube(Foot, P3(Foot.X, Foot.Y, Top - HighTide - .12), .012, .012, 4, Hex(CableC));
					D.AddTube(P3(Foot.X, Foot.Y, Top - 2 * HighTide - .5), P3(Foot.X, Foot.Y, Top), .012, .012, 4, Hex(CableC));
				}
			Cd.Surf = SurfDefault;
			D.Surf = SurfDefault;
			if (L.HasBoat())
			{
				FKeysMesh Bm;
				BuildBoat(Bm, B);
				Append(bDown ? *O.Float : *O.Boats, Bm, FTransform(FVector(0, 0, BoatWl)));
			}
		}

		void Elevator(const FLift& L, const FParts& O)
		{
			const FBoat& B = L.Boat;
			FKeysMesh& S = *O.Structure;
			FKeysMesh& D = *O.Detail;
			const double Wy = -EdgeOff(B), RA = FMath::Clamp(B.L * .42, 2.6, 3.4);
			const double Wl = HungWaterline(B);
			const double TopZ = FMath::Max(L.DeckZ + 1.9, Wl + 1.0), FootZ = -2.4, Lean = .6;
			// where a rail is at height Z (it leans out over the water as it goes down)
			auto RailY = [&](double Z) { return Wy + Lean * (TopZ - Z) / (TopZ - FootZ); };
			S.Surf = SurfSteel;
			for (int32 a = -1; a <= 1; a += 2)
			{
				const double X = a * RA / 2;
				Beam(S, P3(X, Wy, TopZ), P3(X, RailY(FootZ), FootZ), .14, .2, Hex(AluC));
				// bolted to the cap of the wall, and a strut back to it lower down
				S.AddBox(P3(X, Wy - .12, L.DeckZ - .1), FVector(.22, .36, .2), Hex(AluDarkC));
				Beam(S, P3(X, RailY(.5), .5), P3(X, Wy - .1, .5), .08, .08, Hex(AluDarkC));
			}
			S.AddTube(P3(-RA / 2 - .35, Wy, TopZ + .06), P3(RA / 2 + .25, Wy, TopZ + .06), .05, .05, 6, Hex(AluDarkC));
			Motor(S, P3(-RA / 2 - .5, Wy, TopZ + .1), FVector(1, 0, 0));
			S.Surf = SurfDefault;
			const bool bDown = L.Down();
			FKeysMesh& Cs = bDown ? *O.Float : S;
			FKeysMesh& Cd = bDown ? *O.Float : D;
			const double BoatWl = bDown ? 0 : Wl;
			const double Off = FMath::Min(.8, B.B * .3);
			const double BunkTop = BoatWl + BottomZ(B, Off) - (L.State == StateAfloat ? .28 : L.State == StateEmptyDown ? .4 : 0);
			const double ArmTop = BunkTop - (BottomZ(B, Off) - KeelZ(B)) - .1;
			// (for a carriage in the Float part the rail is that much further out or in as the tide moves it; a few
			// centimetres, and it is under water)
			Cs.Surf = SurfSteel;
			for (int32 a = -1; a <= 1; a += 2)
			{
				const double X = a * RA / 2, Y0 = RailY(ArmTop) + .14, UpZ = ArmTop + 1.25;
				Beam(Cs, P3(X, Y0, ArmTop - .1), P3(X, B.B / 2 + .2, ArmTop - .1), .13, .2, Hex(AluC));
				Beam(Cs, P3(X, Y0, ArmTop - .2), P3(X, RailY(UpZ) + .14, UpZ), .1, .16, Hex(AluC));
				Beam(Cs, P3(X, RailY(UpZ) + .16, UpZ - .1), P3(X, Y0 + (B.B / 2 + .2 - Y0) * .55, ArmTop - .02), .06, .06, Hex(AluDarkC));
				const FVector Eye = P3(X, RailY(UpZ) + .1, UpZ);
				Cd.Surf = SurfSteel;
				if (!bDown) Cd.AddTube(Eye, P3(X, Wy + .05, TopZ), .012, .012, 4, Hex(CableC));
				else
				{
					const double Zt = TopZ - HighTide - .12;
					Cd.AddTube(Eye, P3(X, RailY(Zt) + .08, Zt), .012, .012, 4, Hex(CableC));
					D.Surf = SurfSteel;
					D.AddTube(P3(X, RailY(TopZ - 2 * HighTide - .5) + .08, TopZ - 2 * HighTide - .5), P3(X, Wy + .05, TopZ), .012, .012, 4, Hex(CableC));
					D.Surf = SurfDefault;
				}
				Cs.Surf = SurfSteel;
			}
			Bunks(Cs, B, RA / 2, ArmTop, BunkTop, RA + 1.7);
			Cd.Surf = SurfVinyl;
			for (int32 a = -1; a <= 1; a += 2) Cd.AddTube(P3(a * RA / 2, B.B / 2 + .18, ArmTop), P3(a * RA / 2, B.B / 2 + .18, ArmTop + (bDown ? 2.2 : 1.6)), .045, .045, 6, Hex(PvcC));
			Cd.Surf = SurfDefault;
			if (L.HasBoat())
			{
				FKeysMesh Bm;
				BuildBoat(Bm, B);
				Append(bDown ? *O.Float : *O.Boats, Bm, FTransform(FVector(0, 0, BoatWl)));
			}
		}

		void Davits(const FLift& L, const FParts& O)
		{
			const FBoat& B = L.Boat;
			FKeysMesh& S = *O.Structure;
			FKeysMesh& D = *O.Detail;
			const double Ey = -EdgeOff(B) - .2, DA = B.L * .6;
			const double Wl = HungWaterline(B);
			const double TipZ = FMath::Max(L.DeckZ + 2.3, Wl + SheerZ(B) + 1.0);
			const FLinearColor Paint = Hex(Hash(L.Seed, 5.5) < .5 ? 0xf1f2f0 : AluC), Dark = Hex(AluDarkC);
			const bool bHung = L.State == StateHung;
			for (int32 a = -1; a <= 1; a += 2)
			{
				const double X = a * DA / 2;
				const FVector Base = P3(X, Ey, L.DeckZ), Knee = P3(X, Ey, TipZ - .5), Elbow = P3(X, Ey + (0 - Ey) * .45, TipZ - .08), Tip = P3(X, .02, TipZ);
				S.Surf = SurfMetalPaint;
				S.AddBox(Base + FVector(0, 0, .02), FVector(.34, .34, .04), Dark);
				S.AddTube(Base, Knee, .075, .07, 8, Paint);
				S.AddTube(Knee, Elbow, .07, .06, 8, Paint);
				S.AddTube(Elbow, Tip, .06, .05, 8, Paint);
				S.AddTube(Base + FVector(0, .05, .2), Knee + FVector(0, .5, -.1), .025, .025, 5, Paint);   // the stay
				D.Surf = SurfSteel;
				D.AddBox(Base + FVector(0, -.14, 1.05), FVector(.2, .16, .24), Dark);                       // the winch
				Wheel(D, Base + FVector(-.13, -.14, 1.05), Base + FVector(-.2, -.14, 1.05), .1, Dark, 8);
				Wheel(D, Tip + FVector(-.03, 0, -.02), Tip + FVector(.03, 0, -.02), .06, Dark, 8);          // the sheave
				// the fall: down to the lifting eye in the boat, or hooked up short with the boat away or afloat
				const double HookZ = bHung ? Wl + SheerZ(B) - .12 : TipZ - .85;
				D.AddTube(Tip + FVector(0, 0, -.06), P3(X, .02, HookZ + .1), .009, .009, 4, Hex(CableC));
				D.AddBox(P3(X, .02, HookZ + .05), FVector(.06, .05, .12), Dark);
				D.AddTube(Tip + FVector(0, -.03, -.04), Base + FVector(0, -.1, 1.12), .008, .008, 4, Hex(CableC));
				if (bHung)
				{
					// a bridle from the hook to the two sides of her
					for (int32 b = -1; b <= 1; b += 2) D.AddTube(P3(X, .02, HookZ), P3(X, b * B.B * .3, Wl + SheerZ(B) - .3), .008, .008, 4, Hex(CableC));
				}
			}
			S.Surf = SurfDefault;
			D.Surf = SurfDefault;
			if (L.HasBoat())
			{
				FKeysMesh Bm;
				BuildBoat(Bm, B);
				Append(bHung ? *O.Boats : *O.Float, Bm, FTransform(FVector(0, 0, bHung ? Wl : 0)));
			}
		}
	}

	void Beam(FKeysMesh& M, const FVector& A, const FVector& B, double W, double H, const FLinearColor& C, const FVector& Up)
	{
		const FVector Dv = B - A;
		const double Len = Dv.Size();
		if (Len < 1e-4) return;
		FVector U = Up;
		if (FMath::Abs(FVector::DotProduct(Dv / Len, U.GetSafeNormal())) > .97) U = FVector(0, 1, 0);
		M.AddBox((A + B) * .5, FVector(Len, W, H), C, FRotationMatrix::MakeFromXZ(Dv, U).ToQuat());
	}

	void Pile(FKeysMesh& M, double X, double Y, double Top, double R, uint32 Wood, int32 Cap, bool bSleeve)
	{
		const uint8 Was = M.Surf;
		// below the top of the tide it is dark with weed and barnacles
		const double Wet = HighTide + .1;
		M.Surf = SurfBark;
		M.AddTube(FVector(X, Y, -4.6), FVector(X, Y, Wet), R * 1.06, R * 1.03, 6, Hex(0x4f4a3e));
		M.Surf = bSleeve ? SurfVinyl : SurfWood;
		M.AddTube(FVector(X, Y, Wet), FVector(X, Y, Top), R * (bSleeve ? 1.08 : 1.0), R * (bSleeve ? 1.08 : .93), 8, Hex(bSleeve ? 0xe9ecee : Wood));
		M.Surf = SurfDefault;
		if (Cap == 1) M.AddTube(FVector(X, Y, Top), FVector(X, Y, Top + .14), R * 1.12, .02, 8, Hex(0xeef0f1));
		else if (Cap == 2) M.AddTube(FVector(X, Y, Top), FVector(X, Y, Top + .05), R * 1.1, R * .7, 8, Hex(0x1b1d20));
		else if (Cap == 3) { M.Surf = SurfSteel; M.AddTube(FVector(X, Y, Top), FVector(X, Y, Top + .06), R * 1.1, R * .3, 8, Hex(0xb8733a)); }
		else M.AddTube(FVector(X, Y, Top), FVector(X, Y, Top + .004), R * .93, 0, 8, Hex(Wood) * .8f);
		M.Surf = Was;
	}

	FBoat PickBoat(int32 Seed, double MaxL, int32 Want)
	{
		FBoat B;
		for (int32 Try = 0; Try < 40; Try++)
		{
			B = MakeBoat(Seed + Try * 101, MaxL, false);
			if (Want < 0 || B.Kind == Want) break;
		}
		return B;
	}

	double LowestZ(const FBoat& B)
	{
		// the outboard's skeg hangs about a foot under the cavitation plate (KeysYardProps' Engines)
		const KeysMesh::FHullDef H = HullOf(B);
		return FMath::Min(KeelZ(B), -(H.YK * .95 + .1) - .34);
	}

	double HungWaterline(const FBoat& B) { return HighTide + .3 - LowestZ(B); }
	double EdgeOff(const FBoat& B) { return B.B / 2 + .55; }

	void Extent(const FLift& L, double& X0, double& X1, double& Y0, double& Y1)
	{
		const FBoat& B = L.Boat;
		X0 = -B.L / 2 - .8; X1 = B.L / 2 + .3;
		if (L.Kind == LiftCradle)
		{
			const double PA = FMath::Clamp(B.L * .52, 3.3, 4.7), PW = B.B + 1.25;
			X0 = FMath::Min(X0, -PA / 2 - 1.1); X1 = FMath::Max(X1, PA / 2 + .8);
			Y0 = -PW / 2 - .25; Y1 = PW / 2 + .25;
		}
		else if (L.Kind == LiftElevator) { Y0 = -EdgeOff(B) - .3; Y1 = B.B / 2 + .35; }
		else { Y0 = -EdgeOff(B) - .4; Y1 = B.B / 2 + .3; }
	}

	void Build(const FLift& L, const FParts& Out)
	{
		if (L.Kind == LiftNone || !Out.Structure || !Out.Detail || !Out.Boats || !Out.Float) return;
		FKeysMesh S, D, Bt, F;
		const FParts Own{&S, &D, &Bt, &F};
		if (L.Kind == LiftCradle) Cradle(L, Own);
		else if (L.Kind == LiftElevator) Elevator(L, Own);
		else Davits(L, Own);
		const FTransform T(FQuat(FVector::ZAxisVector, L.Yaw), FVector(L.X, L.Y, 0));
		Append(*Out.Structure, S, T);
		Append(*Out.Detail, D, T);
		Append(*Out.Boats, Bt, T);
		Append(*Out.Float, F, T);
	}
}
