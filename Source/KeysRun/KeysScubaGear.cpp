// The scuba set's meshes (declared in KeysScuba.h): the set as the diver wears it in the chase view, the gauge console
// she lifts into the dive-mask view, and the cylinder rack and diver-down flag in the boat's cockpit. All of it is
// built from tubes, ellipsoids and boxes in metres; no brands or logos on anything.
#include "KeysScuba.h"
#include "KeysMesh.h"
#include "KeysData.h"

// the gloved hand closed round a grip (KeysBoatPawn.cpp)
void AddGripHand(FKeysMesh& M, const FVector& C, const FVector& A0, const FVector& N0, double R, double Curl, double IndexLift, bool bMirror,
	const FVector& Elbow, const FLinearColor& Skin, const FLinearColor& Sleeve, const FLinearColor& Nail);

namespace
{
	FLinearColor SgHex(uint32 H) { return FLinearColor::FromSRGBColor(FColor(uint8(H >> 16), uint8(H >> 8), uint8(H))); }

	// smooth-shaded triangle with the winding Unreal wants (front face: Dot(Cross(B-A, C-A), N) < 0)
	void SgTri(FKeysMesh& M, const FVector& A, const FVector& B, const FVector& C, const FVector& NA, const FVector& NB, const FVector& NC, const FLinearColor& Col)
	{
		const FVector Avg = NA + NB + NC;
		const bool bFlip = FVector::DotProduct(FVector::CrossProduct(B - A, C - A), Avg) > 0;
		const int32 I0 = M.AddVert(A, NA, Col);
		const int32 I1 = M.AddVert(bFlip ? C : B, bFlip ? NC : NB, Col);
		const int32 I2 = M.AddVert(bFlip ? B : C, bFlip ? NB : NC, Col);
		M.T.Append({I0, I1, I2});
	}

	void SgBall(FKeysMesh& M, const FVector& C, const FVector& R, int32 Seg, const FLinearColor& Col)
	{
		const int32 Rings = FMath::Max(3, Seg / 2);
		auto P = [&](int32 i, int32 j, FVector& OutP, FVector& OutN)
		{
			const double Th = PI * j / Rings, Ph = 2 * PI * i / Seg;
			const FVector U(FMath::Sin(Th) * FMath::Cos(Ph), FMath::Sin(Th) * FMath::Sin(Ph), FMath::Cos(Th));
			OutP = C + U * R;
			OutN = FVector(U.X / R.X, U.Y / R.Y, U.Z / R.Z).GetSafeNormal();
		};
		for (int32 j = 0; j < Rings; j++)
			for (int32 i = 0; i < Seg; i++)
			{
				FVector A, B, Cc, D, NA, NB, NC, ND;
				P(i, j, A, NA); P(i + 1, j, B, NB); P(i + 1, j + 1, Cc, NC); P(i, j + 1, D, ND);
				if (j > 0) SgTri(M, A, B, Cc, NA, NB, NC, Col);
				if (j < Rings - 1) SgTri(M, A, Cc, D, NA, NC, ND, Col);
			}
	}

	void SgTube(FKeysMesh& M, const FVector& A, const FVector& B, double RA, double RB, int32 Sides, const FLinearColor& Col, bool bCaps = false)
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
			SgTri(M, A + O0 * RA, B + O0 * RB, B + O1 * RB, N0, N0, N1, Col);
			SgTri(M, A + O0 * RA, B + O1 * RB, A + O1 * RA, N0, N1, N1, Col);
			if (bCaps)
			{
				SgTri(M, A, A + O1 * RA, A + O0 * RA, -Dir, -Dir, -Dir, Col);
				SgTri(M, B, B + O0 * RB, B + O1 * RB, Dir, Dir, Dir, Col);
			}
		}
	}

	// a hose through points, rounded off at each joint
	void SgHose(FKeysMesh& M, const TArray<FVector>& P, double R, int32 Sides, const FLinearColor& Col, int32 Sub = 4)
	{
		if (P.Num() < 2) return;
		// Catmull-Rom through the points
		auto At = [&](double T)
		{
			const int32 N = P.Num() - 1;
			const int32 i = FMath::Clamp(int32(T), 0, N - 1);
			const double u = T - i;
			const FVector& P0 = P[FMath::Max(0, i - 1)]; const FVector& P1 = P[i]; const FVector& P2 = P[i + 1]; const FVector& P3 = P[FMath::Min(N, i + 2)];
			return .5 * ((2 * P1) + (-P0 + P2) * u + (2 * P0 - 5 * P1 + 4 * P2 - P3) * u * u + (-P0 + 3 * P1 - 3 * P2 + P3) * u * u * u);
		};
		const int32 Steps = (P.Num() - 1) * Sub;
		FVector Prev = At(0);
		for (int32 s = 1; s <= Steps; s++)
		{
			const FVector Q = At(double(s) / Sub);
			SgTube(M, Prev, Q, R, R, Sides, Col);
			Prev = Q;
		}
	}

	// part of a shell round the X axis (through Y = 0, Z = Cz): from X0 to X1, radius R0 to R1, angles A0..A1 measured
	// from +Z toward +Y (radians), outward-facing
	void SgShell(FKeysMesh& M, double X0, double X1, double R0, double R1, double A0, double A1, int32 N, const FLinearColor& Col, double Cz = 0, double SquashZ = 1)
	{
		auto P = [&](double X, double R, double A) { return FVector(X, FMath::Sin(A) * R, Cz + FMath::Cos(A) * R * SquashZ); };
		for (int32 i = 0; i < N; i++)
		{
			const double Aa = FMath::Lerp(A0, A1, double(i) / N), Ab = FMath::Lerp(A0, A1, double(i + 1) / N);
			const FVector Na(0, FMath::Sin(Aa), FMath::Cos(Aa)), Nb(0, FMath::Sin(Ab), FMath::Cos(Ab));
			SgTri(M, P(X0, R0, Aa), P(X1, R1, Aa), P(X1, R1, Ab), Na, Na, Nb, Col);
			SgTri(M, P(X0, R0, Aa), P(X1, R1, Ab), P(X0, R0, Ab), Na, Nb, Nb, Col);
		}
	}

	// a cylinder with a domed shoulder, a neck and a valve with its handwheel, a rubber boot on the other end.
	// Base is the centre of the bottom, Ax points to the valve, Side is where the handwheel sticks out.
	void SgCylinder(FKeysMesh& M, const FVector& Base, const FVector& Ax, const FVector& Side, double R, double Len, const FLinearColor& Metal, bool bBands)
	{
		const FLinearColor Rubber = SgHex(0x0d0e10), Chrome = SgHex(0xcfd4d8), Band = SgHex(0x202327);
		const FVector Sh = Base + Ax * (Len - .09);
		M.Surf = SurfMetalPaint;
		SgTube(M, Base + Ax * .02, Sh, R, R, 16, Metal);
		// the shoulder: a stack of narrowing rings, then the neck
		for (int32 k = 0; k < 4; k++)
		{
			const double A0 = k * (PI / 2) / 4, A1 = (k + 1) * (PI / 2) / 4;
			SgTube(M, Sh + Ax * (FMath::Sin(A0) * .06), Sh + Ax * (FMath::Sin(A1) * .06), FMath::Max(.024, R * FMath::Cos(A0)), FMath::Max(.024, R * FMath::Cos(A1)), 16, Metal);
		}
		SgTube(M, Sh + Ax * .055, Sh + Ax * .09, .024, .022, 10, Metal, true);
		M.Surf = SurfSteel;
		const FVector Vc = Sh + Ax * .115;
		M.AddRoundBox(Vc, FVector(.05, .042, .042), .008, Chrome, FRotationMatrix::MakeFromXY(Ax, Side).ToQuat(), 1);
		SgTube(M, Vc, Vc + Side * .05, .009, .009, 6, Chrome);
		M.Surf = SurfRubber;
		SgTube(M, Vc + Side * .045, Vc + Side * .075, .024, .022, 10, Rubber, true);   // the handwheel
		// the boot
		SgTube(M, Base - Ax * .012, Base + Ax * .075, R + .006, R + .006, 16, Rubber, true);
		if (bBands)
		{
			M.Surf = SurfCloth;
			for (const double At : {.2, .46})
				SgTube(M, Base + Ax * (At - .025), Base + Ax * (At + .025), R + .004, R + .004, 16, Band);
		}
		M.Surf = SurfDefault;
	}

	// a regulator second stage: a round body with its cover, the exhaust tee under it
	void SgSecondStage(FKeysMesh& M, const FVector& C, const FVector& Fwd, const FVector& Down, const FLinearColor& Body, const FLinearColor& Cover)
	{
		const FVector Side = FVector::CrossProduct(Fwd, Down).GetSafeNormal();
		M.Surf = SurfMetalPaint;
		SgTube(M, C - Fwd * .012, C + Fwd * .03, .036, .034, 12, Body, true);
		SgTube(M, C + Fwd * .03, C + Fwd * .036, .026, .022, 12, Cover, true);
		M.Surf = SurfRubber;
		SgTube(M, C + Down * .034 - Side * .045, C + Down * .034 + Side * .045, .012, .012, 8, SgHex(0x101214), true);
		M.Surf = SurfDefault;
	}

	// seven-segment digits and a few letters, flat on the plane through O spanned by R (right) and U (up), facing Nrm
	uint8 SgSegs(TCHAR Ch)
	{
		// bits: 0 top, 1 top-right, 2 bottom-right, 3 bottom, 4 bottom-left, 5 top-left, 6 middle
		switch (Ch)
		{
		case '0': return 0x3f; case '1': return 0x06; case '2': return 0x5b; case '3': return 0x4f; case '4': return 0x66;
		case '5': return 0x6d; case '6': return 0x7d; case '7': return 0x07; case '8': return 0x7f; case '9': return 0x6f;
		case 'd': return 0x5e; case 't': return 0x78; case 'r': return 0x50; case 'P': return 0x73; case '-': return 0x40;
		case 'E': return 0x79; case 'n': return 0x54; case 'o': return 0x5c; case 'L': return 0x38; case 'A': return 0x77;
		default: return 0;
		}
	}

	// returns the width used
	double SgText(FKeysMesh& M, const FString& S, const FVector& O, const FVector& R, const FVector& U, const FVector& Nrm, double H, const FLinearColor& Col)
	{
		const double W = H * .52, Th = H * .13, Gap = H * .22;
		const FQuat Q = FRotationMatrix::MakeFromXZ(R, U).ToQuat();
		double X = 0;
		auto Bar = [&](double Cx, double Cy, double Sx, double Sy) { M.AddBox(O + R * (X + Cx) + U * Cy + Nrm * .0004, FVector(Sx, .0006, Sy), Col, Q); };
		for (const TCHAR Ch : S)
		{
			if (Ch == ' ') { X += W * .7; continue; }
			if (Ch == '.') { Bar(Th * .5, Th * .5, Th, Th); X += Th + Gap * .7; continue; }
			const uint8 B = SgSegs(Ch);
			if (B & 0x01) Bar(W * .5, H - Th * .5, W - Th, Th);
			if (B & 0x02) Bar(W - Th * .5, H * .75, Th, H * .5 - Th);
			if (B & 0x04) Bar(W - Th * .5, H * .25, Th, H * .5 - Th);
			if (B & 0x08) Bar(W * .5, Th * .5, W - Th, Th);
			if (B & 0x10) Bar(Th * .5, H * .25, Th, H * .5 - Th);
			if (B & 0x20) Bar(Th * .5, H * .75, Th, H * .5 - Th);
			if (B & 0x40) Bar(W * .5, H * .5, W - Th, Th);
			X += W + Gap;
		}
		return X;
	}

	// ---- the console's layout, its own frame (the face looks down -X; Y right, Z up as the eye sees it)
	constexpr double ConW = .078, ConTop = .088, ConBot = -.062, ConT = .028;
	constexpr double DialZ = -.018, DialR = .031;
	constexpr double LcdZ0 = .026, LcdZ1 = .08;
	// the needle's angle for a pressure: 0 psi at 7 o'clock, 4,000 at 5 o'clock, clockwise as seen
	double SgDialAngle(double Psi) { return FMath::DegreesToRadians(-135 + 270 * FMath::Clamp(Psi / 4000, 0.0, 1.0)); }
	FVector SgDial(double Ang, double R) { return FVector(0, FMath::Sin(Ang) * R, DialZ + FMath::Cos(Ang) * R); }

	// where the rack stands in the cockpit, boat frame: its centre on the sole against the port side, aft
	void SgRackAt(const KeysData::FBoatSpec& S, FVector& OutBase, double& OutSide)
	{
		double Lo = 0, Hi = 0;
		KeysMesh::ClearSole(S, Lo, Hi);
		const double X = Lo + .32;
		OutSide = -1;
		OutBase = FVector(X, OutSide * (KeysMesh::InnerHalfBeam(S, X) - .15), KeysMesh::SoleAt(S, X));
	}
}

namespace KeysScuba
{
	// ================================================================ the set as worn
	// The diver model (KeysDiver.cpp) is a prone capsule: +X to the head (at .62), the torso's radius .22, +Z out of her
	// back, the arms straight out sideways at X = .25, the fins pivoting at X = -.55.
	int32 BuildWornKit(FKeysMesh& M, const FKitLook& L)
	{
		const int32 Tri0 = M.T.Num() / 3;
		const FLinearColor Black = SgHex(0x14171a), Web = SgHex(0x23272c), Teal = SgHex(0x148a86), Alu = SgHex(0xb7bcc1), Chrome = SgHex(0xcfd4d8),
			Hose = SgHex(0x0e0f11), Yellow = SgHex(0xf0c419), Lead = SgHex(0x6c7075), Grey = SgHex(0x4a4e53), Lcd = SgHex(0x9fd0b8);
		const double BodyR = .22, TankR = .092, TankZ = .372;
		const double D2R = PI / 180;

		// ---- the jacket: a back panel the length of the torso, wings that wrap under the arms to the belly, shoulder
		// straps over the top, a waist band and a chest strap
		M.Surf = SurfCloth;
		SgShell(M, -.14, .38, BodyR + .028, BodyR + .028, -62 * D2R, 62 * D2R, 10, Black);            // back panel
		SgShell(M, .38, .43, BodyR + .028, BodyR + .005, -62 * D2R, 62 * D2R, 10, Black);             // its rounded top
		SgShell(M, -.18, -.14, BodyR + .004, BodyR + .028, -150 * D2R, 150 * D2R, 16, Black);         // lower hem
		for (int32 Side = -1; Side <= 1; Side += 2)
		{
			// the wing: the air cell, thicker at the side
			SgShell(M, -.14, .13, BodyR + .028, BodyR + .028, Side * 62 * D2R, Side * 150 * D2R, 8, Black);
			SgShell(M, .13, .16, BodyR + .028, BodyR + .004, Side * 62 * D2R, Side * 150 * D2R, 8, Black);
			SgBall(M, FVector(-.01, Side * (BodyR + .02), -.03), FVector(.13, .045, .12), 10, Black);
			SgBall(M, FVector(-.01, Side * (BodyR + .045), -.03), FVector(.085, .03, .075), 8, Teal);    // the pocket panel
			// shoulder strap: from the top of the back panel over the shoulder and down the chest to the wing
			TArray<FVector> St = {FVector(.4, Side * .12, BodyR * .86), FVector(.47, Side * .13, .1), FVector(.44, Side * .135, -.1), FVector(.34, Side * .14, -BodyR * .9), FVector(.14, Side * .15, -BodyR * .93)};
			SgHose(M, St, .019, 6, Web, 3);
			M.Surf = SurfSteel;
			M.AddRoundBox(FVector(.33, Side * .14, -BodyR * .93 - .012), FVector(.035, .045, .012), .004, Chrome, FQuat::Identity, 1);   // strap buckle
			M.Surf = SurfCloth;
		}
		SgShell(M, -.115, -.035, BodyR + .034, BodyR + .034, -PI, PI, 20, Web);                         // waist band
		SgTube(M, FVector(.24, -.14, -BodyR * .97), FVector(.24, .14, -BodyR * .97), .012, .012, 6, Web);   // chest strap
		SgShell(M, .335, .36, BodyR + .031, BodyR + .031, -62 * D2R, 62 * D2R, 10, Teal);              // a stripe of colour across the shoulders
		M.Surf = SurfSteel;
		M.AddRoundBox(FVector(-.075, 0, -(BodyR + .04)), FVector(.06, .07, .014), .004, Chrome, FQuat::Identity, 1);   // waist buckle
		M.AddRoundBox(FVector(.24, 0, -BodyR * .97 - .012), FVector(.03, .04, .012), .004, SgHex(0x1a1c1f), FQuat::Identity, 1);

		// ---- the cylinder on her back, valve toward her head, in two cam bands
		SgCylinder(M, FVector(-.4, 0, TankZ), FVector::XAxisVector, FVector(0, 1, 0), TankR, .7, Alu, true);
		M.Surf = SurfSteel;
		for (const double Bx : {-.2, .06})
			M.AddRoundBox(FVector(Bx, .05, TankZ + TankR + .008), FVector(.05, .06, .014), .004, Chrome, FQuat::Identity, 1);   // cam buckles
		M.Surf = SurfMetalPaint;
		M.AddRoundBox(FVector(-.07, 0, BodyR + .05), FVector(.5, .2, .05), .02, Black, FQuat::Identity, 1);   // the hard pack between tank and jacket

		// ---- the first stage on the valve, and its hoses
		const FVector Valve(.325, 0, TankZ);
		const FVector Fs = Valve + FVector(.012, 0, -.058);
		M.Surf = SurfSteel;
		SgTube(M, Fs - FVector(0, .042, 0), Fs + FVector(0, .042, 0), .026, .026, 12, Chrome, true);
		SgTube(M, Valve + FVector(0, 0, -.02), Fs, .02, .02, 8, Chrome);
		M.Surf = SurfRubber;
		SgTube(M, Valve + FVector(.03, 0, 0), Valve + FVector(.065, 0, 0), .022, .02, 10, Hose, true);   // the yoke screw's knob
		// the primary: over the right shoulder to the mouth
		const FVector Mouth(.765, 0, -.055);
		SgHose(M, {Fs + FVector(0, .045, 0), FVector(.42, .17, .3), FVector(.58, .23, .16), FVector(.72, .17, .0), Mouth + FVector(-.005, .04, 0)}, .0085, 6, Hose);
		SgSecondStage(M, Mouth, FVector::XAxisVector, FVector(0, 0, -1), SgHex(0x2a2e33), Chrome);
		// the octopus: a yellow hose down the right side to a yellow second stage clipped on the chest
		SgHose(M, {Fs + FVector(-.01, .045, 0), FVector(.2, .2, .27), FVector(.14, BodyR + .07, .1), FVector(.15, BodyR + .05, -.08), FVector(.2, .17, -BodyR - .01)}, .0085, 6, Yellow);
		SgSecondStage(M, FVector(.21, .13, -BodyR - .03), FVector(0, 0, -1), FVector(-1, 0, 0), Yellow, SgHex(0x2a2e33));
		// the high-pressure hose down the left side to the gauge console at her hip
		SgHose(M, {Fs + FVector(-.01, -.045, 0), FVector(.2, -.2, .27), FVector(.1, -(BodyR + .075), .08), FVector(-.02, -(BodyR + .07), -.05), FVector(-.12, -(BodyR + .06), -.1)}, .007, 6, Hose);
		M.Surf = SurfRubber;
		const FVector Con(-.19, -(BodyR + .062), -.115);
		M.AddRoundBox(Con, FVector(.15, .034, .07), .014, Hose, FQuat::Identity, 2);
		M.Surf = SurfGlass;
		for (const double Dx : {-.035, .035})
			SgTube(M, Con + FVector(Dx, -.017, 0), Con + FVector(Dx, -.02, 0), .027, .027, 12, SgHex(0xe9ecef), true);   // the two faces
		// the inflator: a corrugated hose from the left shoulder down the chest, the low-pressure hose beside it
		{
			const TArray<FVector> Cor = {FVector(.4, -.13, BodyR * .9), FVector(.47, -.16, .1), FVector(.45, -.185, -.08), FVector(.36, -.19, -BodyR * .9), FVector(.24, -.19, -BodyR * 1.02)};
			M.Surf = SurfRubber;
			SgHose(M, Cor, .017, 8, SgHex(0x1b1d20), 5);
			// its ribs
			for (int32 k = 1; k < 12; k++)
			{
				const double T = k / 12.0 * (Cor.Num() - 1);
				const int32 i = FMath::Min(Cor.Num() - 2, int32(T));
				const FVector P = FMath::Lerp(Cor[i], Cor[i + 1], T - i), Dd = (Cor[i + 1] - Cor[i]).GetSafeNormal();
				SgTube(M, P - Dd * .004, P + Dd * .004, .0205, .0205, 8, SgHex(0x2c2f33));
			}
			M.Surf = SurfMetalPaint;
			M.AddRoundBox(FVector(.2, -.19, -BodyR * 1.02 - .004), FVector(.075, .034, .032), .008, Grey, FQuat::Identity, 1);   // the inflator head
			M.AddRoundBox(FVector(.185, -.19, -BodyR * 1.02 - .024), FVector(.018, .02, .01), .003, SgHex(0xc0392b), FQuat::Identity, 1);   // its button
			M.Surf = SurfRubber;
			SgHose(M, {Fs + FVector(.005, -.045, 0), FVector(.4, -.17, .28), FVector(.5, -.2, .12), FVector(.42, -.215, -.1), FVector(.24, -.215, -BodyR * 1.0)}, .006, 5, Hose);
		}
		// the dump valve on the right shoulder
		M.Surf = SurfMetalPaint;
		SgTube(M, FVector(.36, .15, BodyR + .02), FVector(.36, .15, BodyR + .045), .022, .02, 8, Grey, true);

		// ---- the weight belt: webbing with four lead blocks and a steel buckle
		M.Surf = SurfCloth;
		SgShell(M, -.275, -.225, BodyR + .008, BodyR + .008, -PI, PI, 20, SgHex(0x1b3a5c));
		M.Surf = SurfMetalPaint;
		for (const double A : {55.0, -55.0, 120.0, -120.0})
		{
			const FVector Nn(0, FMath::Sin(A * D2R), FMath::Cos(A * D2R));
			M.AddRoundBox(FVector(-.25, 0, 0) + Nn * (BodyR + .028), FVector(.075, .065, .04), .008, Lead, FRotationMatrix::MakeFromZX(Nn, FVector::XAxisVector).ToQuat(), 1);
		}
		M.Surf = SurfSteel;
		M.AddRoundBox(FVector(-.25, 0, -(BodyR + .016)), FVector(.055, .06, .012), .003, Chrome, FQuat::Identity, 1);

		// ---- the mask strap round the back of her head, and the fins' foot pockets and heel straps
		M.Surf = SurfRubber;
		SgShell(M, .6, .632, .156, .156, -PI, PI, 14, Hose, .04);
		for (int32 Side = -1; Side <= 1; Side += 2)
		{
			SgTube(M, FVector(-.63, Side * .11, 0), FVector(-.52, Side * .11, 0), .05, .058, 8, Hose, true);
			SgTube(M, FVector(-.54, Side * .11, 0), FVector(-.515, Side * .11, 0), .063, .063, 10, Teal);
		}

		// ---- the wrist computer on her left arm (the arm runs out to Y = -.57)
		if (L.bComputer)
		{
			const FVector Arm(0, -.976, .217), Wr = FVector(.25, -.3, .02) + Arm * .2;
			const FVector Up = FVector::CrossProduct(Arm, FVector::XAxisVector).GetSafeNormal() * (FVector::CrossProduct(Arm, FVector::XAxisVector).Z > 0 ? 1 : -1);
			M.Surf = SurfRubber;
			SgTube(M, Wr - Arm * .018, Wr + Arm * .018, .066, .066, 10, Hose);
			SgTube(M, Wr + Up * .058, Wr + Up * .082, .04, .038, 12, SgHex(0x181a1d), true);
			M.Surf = SurfGlass;
			SgTube(M, Wr + Up * .082, Wr + Up * .084, .03, .03, 12, Lcd, true);
		}
		// ---- the pony bottle, slung down her right side as the bail-out, with its own regulator on a short hose
		if (L.bPony)
		{
			SgCylinder(M, FVector(-.3, BodyR + .1, -.1), FVector::XAxisVector, FVector(0, 1, 0), .052, .42, SgHex(0xe8e4d8), false);
			M.Surf = SurfRubber;
			SgHose(M, {FVector(.13, BodyR + .1, -.1), FVector(.2, BodyR + .09, -.14), FVector(.12, BodyR + .04, -.19), FVector(.02, BodyR - .02, -.2)}, .007, 5, Hose);
			SgSecondStage(M, FVector(0, BodyR - .03, -.215), FVector(0, 0, -1), FVector(-1, 0, 0), SgHex(0x2a2e33), Chrome);
			M.Surf = SurfCloth;
			SgTube(M, FVector(-.2, BodyR + .1, -.05), FVector(-.2, BodyR + .02, .02), .008, .008, 5, Web);
			SgTube(M, FVector(.02, BodyR + .1, -.05), FVector(.02, BodyR + .02, .02), .008, .008, 5, Web);
		}
		// ---- the dive light clipped to the right shoulder strap
		if (L.bLight)
		{
			M.Surf = SurfMetalPaint;
			SgTube(M, FVector(.3, .15, -BodyR - .035), FVector(.43, .15, -BodyR - .035), .019, .019, 10, SgHex(0x2b2f34), true);
			SgTube(M, FVector(.43, .15, -BodyR - .035), FVector(.47, .15, -BodyR - .035), .019, .028, 10, SgHex(0x2b2f34));
			M.Surf = SurfGlass;
			SgTube(M, FVector(.47, .15, -BodyR - .035), FVector(.472, .15, -BodyR - .035), .026, .026, 10, SgHex(0xf4f1d8), true);
		}
		M.Surf = SurfDefault;
		return M.T.Num() / 3 - Tri0;
	}

	// ================================================================ the gauge console in the hand
	void BuildConsole(FKeysMesh& M, bool bComputer)
	{
		const FLinearColor Boot = SgHex(0x121416), Face = SgHex(0xeef0ee), Mark = SgHex(0x14171a), Red = SgHex(0xc0392b), Glove = SgHex(0x1c1e21), Suit = SgHex(0x2a2e2c),
			Lcd = SgHex(0xa9c9b2), Steel = SgHex(0xcfd4d8);
		const FVector R(0, 1, 0), U(0, 0, 1), Nrm(-1, 0, 0);
		// the rubber boot, the hose out of its foot
		M.Surf = SurfRubber;
		M.AddRoundBox(FVector(ConT * .5, 0, (ConTop + ConBot) * .5), FVector(ConT, ConW, ConTop - ConBot), .012, Boot, FQuat::Identity, 2);
		SgTube(M, FVector(ConT * .5, 0, ConBot + .004), FVector(ConT * .5, 0, ConBot - .1), .013, .011, 8, Boot);
		SgHose(M, {FVector(ConT * .5, 0, ConBot - .1), FVector(ConT * .5 + .02, -.03, ConBot - .2), FVector(.1, -.12, ConBot - .34), FVector(.2, -.2, ConBot - .5)}, .0075, 6, Boot);
		// the pressure gauge: a white face, a tick every 500 psi, the reserve in red, numbers every thousand
		M.Surf = SurfSteel;
		SgTube(M, FVector(.002, 0, DialZ), FVector(-.002, 0, DialZ), DialR + .004, DialR + .003, 20, Steel);
		M.Surf = SurfDefault;
		SgTube(M, FVector(.001, 0, DialZ), FVector(-.0006, 0, DialZ), DialR, DialR, 20, Face, true);
		for (int32 k = 0; k <= 8; k++)
		{
			const double A = SgDialAngle(k * 500.0);
			const bool bBig = k % 2 == 0;
			const FVector P = SgDial(A, DialR * (bBig ? .84 : .88)) + Nrm * .001;
			M.AddBox(P, FVector(.0006, bBig ? .0022 : .0014, DialR * (bBig ? .2 : .12)), Mark, FQuat(FVector(1, 0, 0), -A));
			if (bBig)
			{
				const FVector Tp = SgDial(A, DialR * .56) + Nrm * .0008;
				SgText(M, FString::FromInt(k / 2), Tp - R * .0022 - U * .0036, R, U, Nrm, .0072, Mark);
			}
		}
		// the red reserve: 0 to 500 psi, a band of short blocks round the rim
		for (int32 k = 0; k < 6; k++)
		{
			const double A = SgDialAngle(k * 500.0 / 5);
			M.AddBox(SgDial(A, DialR * .95) + Nrm * .0009, FVector(.0006, .0045, DialR * .09), Red, FQuat(FVector(1, 0, 0), -A));
		}
		// the computer's window (with the wrist computer owned it is slaved to this second screen); else a depth-only one
		M.Surf = SurfSteel;
		M.AddRoundBox(FVector(.001, 0, (LcdZ0 + LcdZ1) * .5), FVector(.004, ConW - .012, LcdZ1 - LcdZ0 + .006), .002, SgHex(0x2a2d31), FQuat::Identity, 1);
		M.Surf = SurfDefault;
		M.AddBox(FVector(-.0008, 0, (LcdZ0 + LcdZ1) * .5), FVector(.001, ConW - .02, LcdZ1 - LcdZ0), Lcd);
		(void)bComputer;
		// the gloved left hand round the boot's foot, from behind
		const int32 HandFrom = M.V.Num();
		AddGripHand(M, FVector(ConT * .5, 0, ConBot - .045), FVector(0, 0, 1), FVector(1, -.25, 0), .016, .95, 0, true, FVector(.02, -.3, -.5), Glove, Suit, Glove);
		M.SetSurfFrom(HandFrom, SurfRubber);
		M.Surf = SurfDefault;
	}

	void BuildConsoleFace(FKeysMesh& M, double Psi, double Depth, int32 Mins, int32 MinLeft, bool bComputer)
	{
		const FLinearColor Needle = SgHex(0xe8641b), Ink = SgHex(0x18201b), Hub = SgHex(0x14171a);
		const FVector R(0, 1, 0), U(0, 0, 1), Nrm(-1, 0, 0);
		const double A = SgDialAngle(Psi);
		M.AddBox(SgDial(A, DialR * .36) + Nrm * .0016, FVector(.0008, .0024, DialR * .98), Needle, FQuat(FVector(1, 0, 0), -A));
		SgTube(M, FVector(-.0012, 0, DialZ), FVector(-.0024, 0, DialZ), .0042, .0042, 10, Hub, true);
		// the screen: depth in metres on top; under it the dive's minutes and (with the computer) the minutes of gas left
		const FVector O(0, -(ConW - .02) * .5 + .004, 0);
		const FVector Face = Nrm * .0014;
		// (three rows, so the widest of them still fits the window: "d 31.5", "t 120", "r 99")
		SgText(M, FString::Printf(TEXT("d %.1f"), FMath::Clamp(Depth, 0.0, 99.9)), O + U * (LcdZ1 - .021) + Face, R, U, Nrm, .016, Ink);
		SgText(M, FString::Printf(TEXT("t %d"), FMath::Clamp(Mins, 0, 999)), O + U * (LcdZ0 + .0185) + Face, R, U, Nrm, .0105, Ink);
		if (bComputer && MinLeft >= 0)
			SgText(M, FString::Printf(TEXT("r %d"), FMath::Clamp(MinLeft, 0, 99)), O + U * (LcdZ0 + .004) + Face, R, U, Nrm, .0105, Ink);
	}

	// ================================================================ on the boat
	void BuildBoatRack(FKeysMesh& M, const KeysData::FBoatSpec& S, bool bFirst, bool bRigged, bool bSpare)
	{
		const FLinearColor White = SgHex(0xeceae4), Steel = SgHex(0xcfd4d8), Alu = SgHex(0xb7bcc1), Black = SgHex(0x14171a), Teal = SgHex(0x148a86), Yellow = SgHex(0xf0c419), Bungee = SgHex(0x1b3a5c);
		FVector B; double Side;
		SgRackAt(S, B, Side);
		const FVector In(0, -Side, 0);   // toward the middle of the boat
		// a two-hole rack against the liner: a base tray, a backboard, a collar for each cylinder and a strap across
		M.Surf = SurfGelcoat;
		M.AddRoundBox(B + FVector(0, 0, .02) + In * .0, FVector(.5, .27, .04), .012, White, FQuat::Identity, 1);
		M.AddRoundBox(B + FVector(0, 0, .3) - In * .12, FVector(.5, .03, .56), .01, White, FQuat::Identity, 1);
		M.AddRoundBox(B + FVector(0, 0, .42) - In * .0, FVector(.5, .25, .03), .008, White, FQuat::Identity, 1);
		const double Slot[2] = {-.12, .12};
		for (int32 i = 0; i < 2; i++)
		{
			const FVector C = B + FVector(Slot[i], 0, 0) + In * .01;
			M.Surf = SurfSteel;
			SgTube(M, C + FVector(0, 0, .405), C + FVector(0, 0, .44), .104, .104, 16, Steel);
			const bool bHere = i == 0 ? bFirst : bSpare;
			if (!bHere) continue;
			SgCylinder(M, C + FVector(0, 0, .045), FVector::UpVector, In, .092, .7, Alu, i == 0);
			M.Surf = SurfCloth;
			SgTube(M, C + FVector(0, 0, .53), C + FVector(0, 0, .56), .096, .096, 16, Bungee);   // the strap that holds it
			if (i == 0 && bRigged)
			{
				// the rigged set: the jacket on the cylinder, facing inboard, the regulators coiled over the valve
				M.Surf = SurfCloth;
				M.AddRoundBox(C + FVector(0, 0, .5) + In * .1, FVector(.34, .1, .46), .04, Black, FQuat::Identity, 2);
				for (const double Dx : {-.15, .15})
					M.AddRoundBox(C + FVector(Dx, 0, .44) + In * .09, FVector(.1, .2, .26), .04, Black, FQuat::Identity, 1);
				M.AddRoundBox(C + FVector(0, 0, .62) + In * .155, FVector(.3, .012, .03), .004, Teal, FQuat::Identity, 1);
				M.Surf = SurfSteel;
				SgTube(M, C + FVector(-.04, 0, .8) + In * .0, C + FVector(.04, 0, .8), .026, .026, 10, Steel, true);   // first stage
				M.Surf = SurfRubber;
				SgHose(M, {C + FVector(.04, 0, .8), C + FVector(.13, 0, .7) + In * .08, C + FVector(.08, 0, .52) + In * .17, C + FVector(-.04, 0, .5) + In * .17}, .0085, 6, SgHex(0x0e0f11));
				SgHose(M, {C + FVector(-.04, 0, .8), C + FVector(-.13, 0, .68) + In * .08, C + FVector(-.1, 0, .46) + In * .17}, .0085, 6, Yellow);
				SgSecondStage(M, C + FVector(-.05, 0, .49) + In * .17, In, FVector(0, 0, -1), SgHex(0x2a2e33), Steel);
				SgSecondStage(M, C + FVector(-.1, 0, .42) + In * .17, In, FVector(0, 0, -1), Yellow, SgHex(0x2a2e33));
			}
		}
		// the flag's staff at the aft end of the rack, up past the gunwale
		M.Surf = SurfSteel;
		const FVector Foot = B + FVector(-.27, 0, 0) - In * .1;
		SgTube(M, Foot, Foot + FVector(0, 0, 2.35), .014, .011, 8, White, true);
		SgBall(M, Foot + FVector(0, 0, 2.36), FVector(.022), 8, Steel);
		M.Surf = SurfDefault;
	}

	// the diver-down flag: red with a white stripe from the top of the hoist to the bottom of the fly, streaming aft
	void BuildFlag(FKeysMesh& M, const KeysData::FBoatSpec& S, double T)
	{
		FVector B; double Side;
		SgRackAt(S, B, Side);
		const FVector Hoist = B + FVector(-.27, 0, 2.32) + FVector(0, Side * .1, 0);
		const double W = .62, H = .5;
		const int32 NU = 14, NV = 8;
		const FLinearColor Red = SgHex(0xd3202b), White = SgHex(0xf4f4f0);
		auto P = [&](int32 u, int32 v)
		{
			const double Fu = double(u) / NU, Fv = double(v) / NV;
			const double Wave = FMath::Sin(Fu * 7.5 - T * 7 + Fv * 1.3) * .05 * Fu + FMath::Sin(Fu * 3.1 - T * 4.3) * .035 * Fu;
			return Hoist + FVector(-Fu * W * (1 - .04 * Fu), Wave, -Fv * H - Fu * Fu * .06);
		};
		M.Surf = SurfCloth;
		for (int32 v = 0; v < NV; v++)
			for (int32 u = 0; u < NU; u++)
			{
				// the stripe: top of the hoist (u 0, v 0) to the bottom of the fly (u 1, v 1)
				const double Cu = (u + .5) / NU, Cv = (v + .5) / NV;
				const FLinearColor Col = FMath::Abs(Cu - Cv) < .17 ? White : Red;
				const FVector A = P(u, v), Bb = P(u + 1, v), C = P(u + 1, v + 1), D = P(u, v + 1);
				const FVector Nn = FVector::CrossProduct(Bb - A, D - A).GetSafeNormal();
				M.AddQuad(A, Bb, C, D, Col, Nn);
				M.AddQuad(A, D, C, Bb, Col, -Nn);
			}
		M.Surf = SurfDefault;
	}
}
