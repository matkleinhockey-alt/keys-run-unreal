// The speargun's meshes (declared in KeysScubaSpeargun.h): a roller gun built on the user's reference picture, for the
// dive-mask view, the diver's hand in the chase view, the shaft in flight and the shaft through a speared fish. Tubes,
// boxes and flat plates in metres; no name, logo or lettering on any of it.
#include "KeysScubaSpeargun.h"
#include "KeysMesh.h"

namespace
{
	FLinearColor HpHex(uint32 H) { return FLinearColor::FromSRGBColor(FColor(uint8(H >> 16), uint8(H >> 8), uint8(H))); }

	const uint32 HpBlack = 0x131518, HpOlive = 0x4b5a33, HpOliveDark = 0x394529, HpSteel = 0xa7adb3, HpShaft = 0x17191c, HpGrey = 0x55595e, HpMono = 0xd9dde0;

	// smooth-shaded triangle with the winding Unreal wants (front face: Dot(Cross(B-A, C-A), N) < 0)
	void HpTri(FKeysMesh& M, const FVector& A, const FVector& B, const FVector& C, const FVector& NA, const FVector& NB, const FVector& NC, const FLinearColor& Col)
	{
		const FVector Avg = NA + NB + NC;
		const bool bFlip = FVector::DotProduct(FVector::CrossProduct(B - A, C - A), Avg) > 0;
		const int32 I0 = M.AddVert(A, NA, Col);
		const int32 I1 = M.AddVert(bFlip ? C : B, bFlip ? NC : NB, Col);
		const int32 I2 = M.AddVert(bFlip ? B : C, bFlip ? NB : NC, Col);
		M.T.Append({I0, I1, I2});
	}

	void HpTube(FKeysMesh& M, const FVector& A, const FVector& B, double RA, double RB, int32 Sides, const FLinearColor& Col, bool bCaps = false)
	{
		const FVector Dir = (B - A).GetSafeNormal();
		if (Dir.IsNearlyZero()) return;
		FVector U = FVector::CrossProduct(Dir, FVector::ZAxisVector);
		if (U.SizeSquared() < 1e-6) U = FVector::CrossProduct(Dir, FVector::YAxisVector);
		U.Normalize();
		const FVector W = FVector::CrossProduct(Dir, U);
		const double Slope = (RA - RB) / FMath::Max(1e-6, (B - A).Size());
		for (int32 i = 0; i < Sides; i++)
		{
			const double A0 = 2 * PI * i / Sides, A1 = 2 * PI * (i + 1) / Sides;
			const FVector O0 = U * FMath::Cos(A0) + W * FMath::Sin(A0), O1 = U * FMath::Cos(A1) + W * FMath::Sin(A1);
			const FVector N0 = (O0 + Dir * Slope).GetSafeNormal(), N1 = (O1 + Dir * Slope).GetSafeNormal();
			HpTri(M, A + O0 * RA, B + O0 * RB, B + O1 * RB, N0, N0, N1, Col);
			HpTri(M, A + O0 * RA, B + O1 * RB, A + O1 * RA, N0, N1, N1, Col);
			if (bCaps)
			{
				HpTri(M, A, A + O1 * RA, A + O0 * RA, -Dir, -Dir, -Dir, Col);
				HpTri(M, B, B + O0 * RB, B + O1 * RB, Dir, Dir, Dir, Col);
			}
		}
	}

	void HpBall(FKeysMesh& M, const FVector& C, double R, int32 Seg, const FLinearColor& Col)
	{
		const int32 Rings = FMath::Max(3, Seg / 2);
		auto P = [&](int32 i, int32 j) { const double Th = PI * j / Rings, Ph = 2 * PI * i / Seg; return FVector(FMath::Sin(Th) * FMath::Cos(Ph), FMath::Sin(Th) * FMath::Sin(Ph), FMath::Cos(Th)); };
		for (int32 j = 0; j < Rings; j++)
			for (int32 i = 0; i < Seg; i++)
			{
				const FVector A = P(i, j), B = P(i + 1, j), Cc = P(i + 1, j + 1), D = P(i, j + 1);
				if (j > 0) HpTri(M, C + A * R, C + B * R, C + Cc * R, A, B, Cc, Col);
				if (j < Rings - 1) HpTri(M, C + A * R, C + Cc * R, C + D * R, A, Cc, D, Col);
			}
	}

	// a run of tube through points
	void HpPath(FKeysMesh& M, const TArray<FVector>& P, double R, int32 Sides, const FLinearColor& Col)
	{
		for (int32 i = 0; i + 1 < P.Num(); i++) HpTube(M, P[i], P[i + 1], R, R, Sides, Col);
		for (int32 i = 1; i + 1 < P.Num(); i++) if (Sides >= 6) HpBall(M, P[i], R, 6, Col);   // round the joints
	}

	// a flat plate with two faces and an edge all round: the outline Pts (in order), its faces along +/-Nrm
	void HpPlate(FKeysMesh& M, const TArray<FVector>& Pts, const FVector& Nrm, double Thick, const FLinearColor& Col)
	{
		const FVector H = Nrm * (Thick * .5);
		FVector Ctr = FVector::ZeroVector;
		for (const FVector& P : Pts) Ctr += P;
		Ctr /= Pts.Num();
		for (int32 i = 0; i < Pts.Num(); i++)
		{
			const FVector& A = Pts[i]; const FVector& B = Pts[(i + 1) % Pts.Num()];
			M.AddTri(Ctr + H, A + H, B + H, Col, Nrm);
			M.AddTri(Ctr - H, A - H, B - H, Col, -Nrm);
			const FVector Out = FVector::CrossProduct(B - A, Nrm).GetSafeNormal() * (FVector::DotProduct(FVector::CrossProduct(B - A, Nrm), (A + B) * .5 - Ctr) >= 0 ? 1 : -1);
			M.AddQuad(A - H, B - H, B + H, A + H, Col, Out);
		}
	}
}

namespace KeysSpeargun
{
	void BuildGun(FKeysMesh& M, bool bLod)
	{
		const FLinearColor Black = HpHex(HpBlack), Olive = HpHex(HpOlive), OliveD = HpHex(HpOliveDark), Steel = HpHex(HpSteel), Grey = HpHex(HpGrey), Mono = HpHex(HpMono);
		const int32 Rnd = bLod ? 8 : 16, Sm = bLod ? 5 : 8;
		// ---- the barrel: a slim matt-black tube, and the rail the shaft lies on along its top
		M.Surf = SurfRubber;
		HpTube(M, FVector(-.03, 0, 0), FVector(BarrelLen - .07, 0, 0), BarrelR, BarrelR, Rnd, Black, true);
		M.Surf = SurfMetalPaint;
		M.AddRoundBox(FVector((BarrelLen - .09) * .5, 0, BarrelR + .0015), FVector(BarrelLen - .11, .011, .005), .0015, HpHex(0x08090a), FQuat::Identity, 1);
		if (!bLod)
			for (int32 Side = -1; Side <= 1; Side += 2)   // the rail's two lips
				M.AddRoundBox(FVector((BarrelLen - .09) * .5, Side * .0052, BarrelR + .0042), FVector(BarrelLen - .11, .0016, .002), .0006, HpHex(0x1d2023), FQuat::Identity, 1);

		// ---- the roller head: a block on the end of the barrel, a nose with the shaft's guide on top, and a black
		// roller on an axle out each side
		M.Surf = SurfMetalPaint;
		M.AddRoundBox(FVector(RollerX - .004, 0, -.001), FVector(.078, (RollerY - RollerHalfW) * 2 - .002, .04), .008, OliveD, FQuat::Identity, 2);
		HpTube(M, FVector(RollerX + .03, 0, -.002), FVector(BarrelLen + .004, 0, -.002), .0135, .0105, Rnd, Black, true);
		M.Surf = SurfSteel;
		HpTube(M, FVector(BarrelLen - .02, 0, RailY), FVector(BarrelLen - .004, 0, RailY), .0068, .0068, Sm + 2, Steel);   // the guide ring
		M.AddRoundBox(FVector(BarrelLen - .012, 0, RailY - .0085), FVector(.014, .008, .008), .002, Black, FQuat::Identity, 1);
		for (int32 Side = -1; Side <= 1; Side += 2)
		{
			const double Y0 = Side * (RollerY - RollerHalfW), Y1 = Side * (RollerY + RollerHalfW);
			M.Surf = SurfRubber;
			HpTube(M, FVector(RollerX, Y0, 0), FVector(RollerX, Y1, 0), RollerR, RollerR, bLod ? 10 : 20, HpHex(0x070808), true);
			// the flanges that keep the bands on, the hub and the axle's bolt head
			M.Surf = SurfMetalPaint;
			HpTube(M, FVector(RollerX, Y1, 0), FVector(RollerX, Y1 + Side * .0025, 0), RollerR + .0035, RollerR + .0035, bLod ? 10 : 20, HpHex(0x101214), true);
			if (!bLod)
			{
				HpTube(M, FVector(RollerX, Y1 + Side * .0025, 0), FVector(RollerX, Y1 + Side * .004, 0), .0125, .011, 14, HpHex(0x2b2e31), true);
				M.Surf = SurfSteel;
				HpTube(M, FVector(RollerX, Y1 + Side * .004, 0), FVector(RollerX, Y1 + Side * .0065, 0), .0052, .0045, 6, Steel, true);
			}
		}

		// ---- the bands' lower runs: each band comes off the top of its roller (the animated part), wraps down round
		// the front of it and runs back under the barrel to an anchor, bowing out a little as slack rubber does
		M.Surf = SurfMetalPaint;
		const double AnchorX = .34;
		M.AddRoundBox(FVector(AnchorX - .01, 0, -BarrelR - .009), FVector(.04, .062, .02), .006, Black, FQuat::Identity, 1);
		M.Surf = SurfRubber;
		for (int32 Side = -1; Side <= 1; Side += 2)
			for (int32 k = 0; k < 2; k++)
			{
				const double Y = Side * (RollerY + (k ? .008 : -.008));
				const double Rr = RollerR + BandR * .9;
				// round the front of the roller, top to bottom
				TArray<FVector> Wrap;
				const int32 NW = bLod ? 4 : 8;
				for (int32 i = 0; i <= NW; i++)
				{
					const double A = PI * .5 - PI * i / NW;   // +90 (top) through 0 (front) to -90 (bottom)
					Wrap.Add(FVector(RollerX + FMath::Cos(A) * Rr, Y, FMath::Sin(A) * Rr));
				}
				HpPath(M, Wrap, BandR * .92, Sm, Olive);
				// back under the barrel
				const FVector B0(RollerX, Y, -Rr), B3(AnchorX, Side * (.012 + k * .014), -BarrelR - .012);
				TArray<FVector> Run;
				const int32 NR = bLod ? 3 : 7;
				for (int32 i = 0; i <= NR; i++)
				{
					const double U = double(i) / NR, Bow = FMath::Sin(U * PI);
					Run.Add(FMath::Lerp(B0, B3, U) + FVector(0, Side * Bow * (.012 + .006 * k), -Bow * (.016 + .007 * k)));
				}
				HpPath(M, Run, BandR * .92, Sm, Olive);
			}

		// ---- the handle: a receiver round the back of the barrel, the raked pistol grip with an olive panel on each
		// side, the trigger in its guard, a safety, and the butt pad that goes against the chest to load
		M.Surf = SurfRubber;
		M.AddRoundBox(FVector(-.09, 0, -.006), FVector(.2, .031, .044), .011, Black, FQuat::Identity, 2);
		const FVector GT = GripTop(), GB = GripBot();
		const FQuat GripQ = FRotationMatrix::MakeFromZ(GT - GB).ToQuat();
		M.AddRoundBox((GT + GB) * .5, FVector(.038, .029, (GT - GB).Size() + .01), .012, Black, GripQ, 2);
		M.AddRoundBox(GB + FVector(-.004, 0, -.006), FVector(.047, .033, .014), .006, Black, GripQ, 2);
		for (int32 Side = -1; Side <= 1; Side += 2)
		{
			M.Surf = SurfRubber;
			M.AddRoundBox((GT + GB) * .5 + FVector(0, Side * .0142, 0), FVector(.028, .0034, (GT - GB).Size() * .8), .0015, Olive, GripQ, 1);
			M.AddRoundBox(FVector(-.085, Side * .0152, .002), FVector(.12, .0022, .014), .001, Olive, FQuat::Identity, 1);   // and a stripe down the receiver
			if (bLod) continue;
			// the panel's texture: rows of raised pips
			const FVector Ax = (GT - GB).GetSafeNormal(), Fw = GripQ.RotateVector(FVector::XAxisVector);
			for (int32 r = 0; r < 8; r++)
				for (int32 c = 0; c < 3; c++)
				{
					const FVector Pp = (GT + GB) * .5 + Ax * ((r - 3.5) * .0105) + Fw * ((c - 1) * .0075 + (r % 2 ? .0019 : -.0019)) + FVector(0, Side * .0162, 0);
					M.AddRoundBox(Pp, FVector(.0042, .0016, .0042), .0006, OliveD, GripQ, 1);
				}
		}
		M.Surf = SurfSteel;
		HpTube(M, FVector(-.04, 0, -.026), FVector(-.047, 0, -.056), .0036, .003, 6, Grey, true);   // trigger
		M.AddRoundBox(FVector(-.06, -.0165, .004), FVector(.02, .004, .008), .0015, Steel, FQuat(FVector::YAxisVector, -.25), 1);   // the safety's lever
		M.Surf = SurfRubber;
		HpPath(M, {FVector(-.004, 0, -.026), FVector(-.002, 0, -.066), FVector(-.03, 0, -.08), FVector(-.078, 0, -.083)}, .0038, 6, Black);   // guard
		// the butt: a short neck and a broad pad
		HpTube(M, FVector(-.185, 0, -.004), FVector(-.215, 0, -.006), .014, .012, Rnd, Black);
		M.AddRoundBox(FVector(-.224, 0, -.008), FVector(.02, .056, .082), .009, HpHex(0x08090a), FQuat(FVector::YAxisVector, .08), 2);

		// ---- under the barrel: the reel's stem, a line guide, the hook the leader ties to, and the line from the
		// spool up to it
		M.Surf = SurfMetalPaint;
		HpTube(M, FVector(ReelAt().X, 0, -BarrelR + .002), ReelAt(), .006, .006, 6, Black);
		M.Surf = SurfSteel;
		HpTube(M, FVector(.1, -.004, -BarrelR + .001), FVector(.1, -.004, -BarrelR - .012), .0022, .0022, 6, Grey, true);
		HpPath(M, {LeaderHook() + FVector(.006, 0, .01), LeaderHook() + FVector(.008, 0, -.002), LeaderHook() + FVector(-.004, 0, -.004)}, .002, 5, Steel);
		if (!bLod)
		{
			M.Surf = SurfDefault;
			const FVector L0 = ReelAt() + FVector(.03, 0, .01), L1 = LeaderHook();
			TArray<FVector> Ln;
			for (int32 i = 0; i <= 8; i++) { const double t = i / 8.0; Ln.Add(FMath::Lerp(L0, L1, t) + FVector(0, 0, -.03 * FMath::Sin(t * PI))); }
			HpPath(M, Ln, .0009, 4, Mono);
		}
		M.Surf = SurfDefault;
	}

	void BuildReel(FKeysMesh& Rl)
	{
		const FLinearColor ReelC = HpHex(0x15171a), Mono = HpHex(HpMono), Grey = HpHex(HpGrey), Olive = HpHex(HpOlive);
		const FVector O = FVector::ZeroVector;
		Rl.Surf = SurfMetalPaint;
		HpTube(Rl, O + FVector(0, -.016, 0), O + FVector(0, .016, 0), .036, .036, 18, ReelC, true);
		Rl.Surf = SurfDefault;
		HpTube(Rl, O + FVector(0, -.013, 0), O + FVector(0, .0175, 0), .031, .031, 18, Mono, true);
		Rl.Surf = SurfMetalPaint;
		for (int32 Side = -1; Side <= 1; Side += 2)
			HpTube(Rl, O + FVector(0, Side * .0165, 0), O + FVector(0, Side * .0185, 0), .039, .039, 20, ReelC, true);   // flanges
		// three lightening holes in the outer flange (pale), so you can see it turn
		for (int32 k = 0; k < 3; k++)
		{
			const double A = k * 2 * PI / 3;
			const FVector Hc = O + FVector(FMath::Cos(A) * .025, -.0189, FMath::Sin(A) * .025);
			HpTube(Rl, Hc, Hc + FVector(0, -.0008, 0), .0055, .0055, 10, Grey, true);
		}
		Rl.Surf = SurfSteel;
		HpTube(Rl, O + FVector(0, -.018, 0), O + FVector(0, -.027, 0), .008, .008, 10, Grey, true);   // hub
		HpTube(Rl, O + FVector(0, -.025, 0), O + FVector(-.012, -.025, -.024), .0035, .0035, 6, Grey, true);   // crank arm
		Rl.Surf = SurfRubber;
		HpTube(Rl, O + FVector(-.012, -.026, -.024), O + FVector(-.012, -.042, -.024), .0055, .005, 8, Olive, true);   // knob
		Rl.Surf = SurfDefault;
	}

	void BuildBandPair(FKeysMesh& B, int32 Side)
	{
		// the component is turned to run back along the gun (yaw 180), so its local -Y * Side is outboard
		B.Surf = SurfRubber;
		HpTube(B, FVector(0, 0, 0), FVector(1, 0, 0), 1, 1, 8, HpHex(HpOlive));
		HpTube(B, FVector(0, -Side * 2.15, 0), FVector(1, -Side * 2.15, 0), 1, 1, 8, HpHex(HpOlive));
		B.Surf = SurfDefault;
	}

	void BuildWishbone(FKeysMesh& Wb)
	{
		// origin: where the bands end, on the gun's centre line at the shaft's height
		const FLinearColor Cord = HpHex(0xc9ccc4), Black = HpHex(HpBlack);
		for (int32 Side = -1; Side <= 1; Side += 2)
		{
			Wb.Surf = SurfMetalPaint;
			// the band pair's end cap, then the bridle back to the notch
			const FVector End(0, Side * RollerY, BandZ(BandR * .7) - RailY);
			Wb.AddRoundBox(End + FVector(-.004, 0, 0), FVector(.016, .034, .014), .004, Black, FQuat::Identity, 1);
			Wb.Surf = SurfCloth;
			HpPath(Wb, {End + FVector(-.01, 0, 0), FVector(-WishBack * .6, Side * RollerY * .45, ShaftR + .004), FVector(-WishBack, Side * .003, ShaftR + .0035)}, .0022, 6, Cord);
		}
		Wb.Surf = SurfSteel;
		HpBall(Wb, FVector(-WishBack, 0, ShaftR + .004), .0048, 8, HpHex(HpSteel));
		Wb.Surf = SurfDefault;
	}

	void AddHead(FKeysMesh& M, const FVector& Tip, const FVector& Dir, const FVector& Up, double Sc, double FlopperDeg, bool bLod)
	{
		const FLinearColor Steel = HpHex(0x1b1d20), Edge = HpHex(0x8f959b), OliveD = HpHex(HpOliveDark);
		const FVector D = Dir.GetSafeNormal();
		FVector U = (Up - D * FVector::DotProduct(Up, D)).GetSafeNormal();
		if (U.IsNearlyZero()) U = FVector::CrossProduct(D, FVector::YAxisVector).GetSafeNormal();
		const FVector R = FVector::CrossProduct(U, D);
		// head-local: x back from the point, y across, z toward Up
		auto P = [&](double X, double Y, double Z) { return Tip - D * (X * Sc) + R * (Y * Sc) + U * (Z * Sc); };
		const uint8 Keep = M.Surf;
		M.Surf = SurfMetalPaint;
		// the main blade: a wide swept broadhead lying flat, each wing cut back in a hook toward the shaft
		for (int32 Side = -1; Side <= 1; Side += 2)
		{
			HpPlate(M, {P(0, 0, 0), P(.062, Side * .03, 0), P(.094, Side * .047, 0), P(.078, Side * .026, 0), P(.085, Side * .014, 0), P(.066, Side * .004, 0)}, U, .0026 * Sc, Steel);
			// its ground edge, bright
			if (!bLod) HpTube(M, P(.002, Side * .001, 0), P(.094, Side * .047, 0), .0009 * Sc, .0009 * Sc, 4, Edge);
		}
		// the ferrule the blades are set in, running back into the collar
		HpTube(M, P(.012, 0, 0), P(.07, 0, 0), .0012 * Sc, .0046 * Sc, 6, Steel);
		HpTube(M, P(.07, 0, 0), P(.14, 0, 0), .0046 * Sc, .0046 * Sc, 6, Steel);
		// two smaller barbs behind it, standing the other way (up and down), swept the same
		for (int32 Side = -1; Side <= 1; Side += 2)
			HpPlate(M, {P(.082, 0, Side * .003), P(.118, 0, Side * .022), P(.142, 0, Side * .031), P(.13, 0, Side * .015), P(.134, 0, Side * .004)}, R, .0024 * Sc, Steel);
		// the collar, and the flopper hinged under it: shut it lies back along the shaft, open it stands out to hold
		M.Surf = SurfMetalPaint;
		HpTube(M, P(.14, 0, 0), P(.17, 0, 0), .0072 * Sc, .0072 * Sc, 8, OliveD, true);
		const double A = FMath::DegreesToRadians(FlopperDeg);
		const FVector Hinge = P(.146, 0, -.0072);
		const FVector Fd = (-D * FMath::Cos(A) - U * FMath::Sin(A));   // back and away from Up
		const FVector Fn = FVector::CrossProduct(Fd, R).GetSafeNormal();
		HpPlate(M, {Hinge + R * (.005 * Sc), Hinge + R * (.005 * Sc) + Fd * (.062 * Sc), Hinge - R * (.005 * Sc) + Fd * (.062 * Sc), Hinge - R * (.005 * Sc)}, Fn, .003 * Sc, Steel);
		M.AddRoundBox(Hinge + Fd * (.066 * Sc), FVector(.014, .012, .008) * Sc, .002 * Sc, Steel, FRotationMatrix::MakeFromXY(Fd, R).ToQuat(), 1);
		HpTube(M, Hinge - R * (.0075 * Sc), Hinge + R * (.0075 * Sc), .0026 * Sc, .0026 * Sc, 6, Edge, true);   // the hinge pin
		M.Surf = Keep;
	}

	void BuildShaft(FKeysMesh& Sh, bool bLod, const FVector& At)
	{
		const FLinearColor Shaft = HpHex(HpShaft), Steel = HpHex(HpSteel);
		const double Front = ShaftPoke * .5, Back = Front - ShaftLen;
		Sh.Surf = SurfMetalPaint;
		HpTube(Sh, At + FVector(Back, 0, 0), At + FVector(Front, 0, 0), ShaftR, ShaftR, bLod ? 5 : 8, Shaft, true);
		AddHead(Sh, At + FVector(Front + HeadLen, 0, 0), FVector::XAxisVector, FVector::ZAxisVector, HeadScale, 22, bLod);
		// the notches the wishbone drops into (shark fins), and the slide ring with its eye
		Sh.Surf = SurfMetalPaint;
		const double FinX = BandAnchorX - BandLong - WishBack - (BarrelLen + Front);
		if (!bLod)
			for (int32 k = 0; k < 2; k++)
				Sh.AddRoundBox(At + FVector(FinX - .008 - k * .06, 0, ShaftR + .0025), FVector(.014, .0026, .007), .001, Shaft, FQuat::Identity, 1);
		Sh.Surf = SurfSteel;
		HpTube(Sh, At + FVector(RingX - .009, 0, 0), At + FVector(RingX + .009, 0, 0), .0078, .0078, 8, Steel, true);
		HpTube(Sh, At + FVector(RingX, 0, -.0078), At + FVector(RingX, 0, -.016), .003, .003, 5, Steel, true);
		Sh.Surf = SurfDefault;
	}

	void BuildLeader(FKeysMesh& M, const FVector& Ring, const FVector& Hook, double Len)
	{
		const FLinearColor Cable = HpHex(0x9da3a8), Crimp = HpHex(0x5d6166);
		const double D = (Ring - Hook).Size();
		const double Sag = D < Len ? FMath::Sqrt(Len * Len - D * D) * .5 : 0;
		const FVector Mid = (Ring + Hook) * .5 + FVector(0, 0, -Sag * 1.9);   // the quadratic's control point
		M.Surf = SurfSteel;
		const int32 Seg = 12;
		FVector Prev = Ring;
		for (int32 i = 1; i <= Seg; i++)
		{
			const double t = double(i) / Seg;
			const FVector P = Ring * ((1 - t) * (1 - t)) + Mid * (2 * t * (1 - t)) + Hook * (t * t);
			// braided: the cable's colour alternates strand by strand
			HpTube(M, Prev, P, .0026, .0026, 5, i % 2 ? Cable : HpHex(0x7f858a));
			Prev = P;
		}
		// the crimped sleeves at each end
		const FVector T0 = (Mid - Ring).GetSafeNormal(), T1 = (Mid - Hook).GetSafeNormal();
		HpTube(M, Ring + T0 * .012, Ring + T0 * .04, .0046, .0046, 6, Crimp, true);
		HpTube(M, Hook + T1 * .012, Hook + T1 * .036, .0046, .0046, 6, Crimp, true);
		M.Surf = SurfDefault;
	}

	void BuildCarried(FKeysMesh& Gun, FKeysMesh& Shaft)
	{
		BuildGun(Gun, true);
		// the bands at the loaded stretch, and the wishbone on the notch
		const double Br = .0048;
		Gun.Surf = SurfRubber;
		for (int32 Side = -1; Side <= 1; Side += 2)
			for (int32 k = 0; k < 2; k++)
			{
				const double Y = Side * (RollerY + (k ? .006 : -.006));
				HpTube(Gun, FVector(BandAnchorX, Y, BandZ(Br)), FVector(BandAnchorX - BandLong, Y, BandZ(Br)), Br, Br, 5, HpHex(HpOlive));
			}
		Gun.Surf = SurfCloth;
		for (int32 Side = -1; Side <= 1; Side += 2)
			HpTube(Gun, FVector(BandAnchorX - BandLong, Side * RollerY, BandZ(Br)), FVector(BandAnchorX - BandLong - WishBack, 0, RailY + ShaftR), .002, .002, 4, HpHex(0xc9ccc4));
		Gun.Surf = SurfDefault;
		// the shaft where it lies when the gun is loaded, and its leader
		const FVector Off(BarrelLen + ShaftPoke * .5, 0, RailY);
		BuildShaft(Shaft, true, Off);
		BuildLeader(Shaft, Off + FVector(RingX, 0, -.016), LeaderHook(), LeaderLen);
	}

	void AddShaftThroughFish(FKeysMesh& M, double HalfY, double HalfZ)
	{
		// in low on the near flank, out high on the far one: mostly upward, so it reads from either side
		const FVector Dn = FVector(.22, .5, .78).GetSafeNormal();
		const FVector C(0, 0, 0);
		const double Sc = 1.25;
		const double Through = FMath::Sqrt(FMath::Square(HalfY * Dn.Y) + FMath::Square(HalfZ * Dn.Z)) / FMath::Max(.2, Dn.Y * Dn.Y + Dn.Z * Dn.Z) + .03;
		const FVector Collar = C + Dn * Through;                    // the back of the head's collar sits at the skin
		const FVector Tip = Collar + Dn * (.17 * Sc);
		const FVector Tail = C - Dn * (Through + .5);
		const uint8 Keep = M.Surf;
		M.Surf = SurfMetalPaint;
		HpTube(M, Tail, Collar, ShaftR * Sc, ShaftR * Sc, 6, HpHex(HpShaft), true);
		AddHead(M, Tip, Dn, FVector(-1, 0, .2), Sc, 62, false);     // the flopper swung open: it is what holds the fish
		// the slide ring run down to the fish, and the first of the leader going away
		M.Surf = SurfSteel;
		const FVector Ring = C - Dn * (Through + .05);
		HpTube(M, Ring - Dn * .008, Ring + Dn * .008, .0068 * Sc, .0068 * Sc, 8, HpHex(HpSteel), true);
		BuildLeader(M, Ring + FVector(0, 0, -.01), Tail + FVector(-.1, -.06, -.12), .5);
		M.Surf = Keep;
	}
}
