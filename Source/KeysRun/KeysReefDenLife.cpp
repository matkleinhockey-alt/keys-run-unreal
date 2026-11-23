// What lives in the reef's dens (see KeysReefDen.h), all of it of the Florida Keys reef tract and built here in code:
//  - green moray (to 1.8 m) and spotted moray (to 0.9 m): head and a little body out of the hole, the mouth opening
//    and closing as it breathes; it draws back when the diver comes close or the dive light is on it and comes out
//    again after. By the feeding clock's night it leaves the hole and hunts along the bottom a few metres from home.
//    A green moray that a grouper hunts with (two in five) is called out by day now and then.
//  - Caribbean spiny lobster: several to a ledge, bodies in the dark, the long antennae out, waving, and pointing at a
//    diver; they flick back in when she comes near. At night they walk out on the sand.
//  - Caribbean reef octopus: a den with a midden of shells in front; by day an eye and an arm, at night it sits out at
//    the mouth and goes hunting over the bottom near by.
//  - squirrelfish and blackbar soldierfish: small groups hovering just inside a ledge's shadow by day; at night they
//    leave it, each to its own beat.
//  - glassy sweepers: a tight school milling inside a cave mouth; at night it spreads out over the reef above.
//  - nurse shark asleep under a ledge with its tail out; goliath or black grouper filling a big hole.
//  - channel clinging crab and stone crab in a hole; banded coral shrimp at a hole's mouth, where a fish comes to be
//    cleaned and holds still (seen from close up only).
// Most dens are empty or show a pair of eyes (KeysReefDen.cpp deals who lives where, from each den's own seed).
//
// Every animal is a few rigid parts (a head, a jaw, body segments, an antenna ...) drawn as instances of one small mesh
// per part with the school fish's material (M_KR_Boid: it beats a fish's tail, and shades each instance, which is how
// a body goes dark as it backs into its hole). Households exist only within 60 m of the camera, are drawn within 40 m
// (shrimp and crabs 8 m) and move only within 25 m.
#include "KeysReefDenInt.h"
#include "KeysGeo.h"
#include "KeysSea.h"
#include "KeysLife.h"
#include "KeysBoatPawn.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/World.h"

namespace KeysReefDen
{
	TFunction<int32(const FVector&, double)> SchoolFishCounter;

	namespace
	{
		enum EPart
		{
			PEyes, PGMHead, PGMJaw, PGMSeg, PSMHead, PSMJaw, PSMSeg, PLobBody, PLobAnt, PLobLegs, POctoBody, POctoArm, PMidden,
			PSquirrel, PSoldier, PSweeper, PNurse, PGoliath, PGrouper, PClingCrab, PStoneCrab, PShrimp, PShrimpFeeler, PClient, NumParts
		};
		// bFish: a whole fish, laid nose to tail on the material's -50..50 so its tail beats; Cull: drawn to this far (m)
		struct FPartSpec { const TCHAR* Name; bool bFish; double Cull; };
		const FPartSpec Parts[NumParts] = {
			{TEXT("eyes"), false, 14}, {TEXT("green moray head"), false, 40}, {TEXT("green moray jaw"), false, 40}, {TEXT("green moray body"), false, 40},
			{TEXT("spotted moray head"), false, 40}, {TEXT("spotted moray jaw"), false, 40}, {TEXT("spotted moray body"), false, 40},
			{TEXT("lobster body"), false, 40}, {TEXT("lobster antenna"), false, 40}, {TEXT("lobster legs"), false, 25},
			{TEXT("octopus body"), false, 40}, {TEXT("octopus arm"), false, 40}, {TEXT("octopus midden"), false, 25},
			{TEXT("squirrelfish"), true, 40}, {TEXT("soldierfish"), true, 40}, {TEXT("glassy sweeper"), true, 40},
			{TEXT("nurse shark"), true, 40}, {TEXT("goliath grouper"), true, 40}, {TEXT("black grouper"), true, 40},
			{TEXT("clinging crab"), false, 8}, {TEXT("stone crab"), false, 8}, {TEXT("banded coral shrimp"), false, 8}, {TEXT("shrimp feeler"), false, 8},
			{TEXT("grunt at the cleaning station"), true, 40},
		};

		FLinearColor Mul(const FLinearColor& C, double K) { return FLinearColor(float(C.R * K), float(C.G * K), float(C.B * K), 1); }
		double SgnPow(double V, double P) { return FMath::Sign(V) * FMath::Pow(FMath::Abs(V), P); }

		// ================================================================ mesh helpers (Unreal's local frame, metres: x ahead, y to the side, z up)
		void SheetUE(FKeysMesh& M, const TArray<FVector>& P, const TArray<FLinearColor>& Col, int32 NU, int32 NV, const FVector& Inside, int32 SignRow)
		{
			TArray<FVector> Nr;
			Nr.SetNum(NU * NV);
			double Flip = 0;
			for (int32 v = 0; v < NV; v++)
				for (int32 u = 0; u < NU; u++)
				{
					const int32 u0 = (u + NU - 1) % NU, u1 = (u + 1) % NU, v0 = FMath::Max(0, v - 1), v1 = FMath::Min(NV - 1, v + 1);
					FVector Du = P[v * NU + u1] - P[v * NU + u0];
					const FVector Dv = P[v1 * NU + u] - P[v0 * NU + u];
					if (Du.SizeSquared() < 1e-12)
					{
						const int32 vn = v == 0 ? 1 : v - 1;
						Du = P[vn * NU + u1] - P[vn * NU + u0];
					}
					const FVector Nn = FVector::CrossProduct(Du, Dv).GetSafeNormal();
					Nr[v * NU + u] = Nn;
					if (v == SignRow) Flip += FVector::DotProduct(Nn, P[v * NU + u] - Inside);
				}
			const double Sg = Flip < 0 ? -1 : 1;
			const int32 Base = M.V.Num();
			for (int32 i = 0; i < NU * NV; i++)
			{
				FVector Nn = Nr[i] * Sg;
				if (Nn.IsNearlyZero()) Nn = (P[i] - Inside).GetSafeNormal(.0001, FVector(0, 0, 1));
				M.AddVert(P[i], Nn, Col[i]);
			}
			for (int32 v = 0; v < NV - 1; v++)
				for (int32 u = 0; u < NU; u++)
				{
					const int32 a = Base + v * NU + u, b = Base + v * NU + (u + 1) % NU, c = Base + (v + 1) * NU + (u + 1) % NU, d = Base + (v + 1) * NU + u;
					M.T.Append({a, b, c, a, c, d});
				}
		}

		// a body lofted through stations along X: at each a centre height Z, half-width W and half-height H. Closed at
		// both ends. Paint(0..1 along, angle round from the back (0) to the belly (PI), the point). Pw: 2 round, more is
		// squarer. Crest: the back's own ridge, as a part of H (a moray's fin).
		struct FSta { double X, Z, W, H; };
		using FPaint = TFunctionRef<FLinearColor(double, double, const FVector&)>;
		void Loft(FKeysMesh& M, const TArray<FSta>& S, int32 Sides, FPaint Paint, double Pw = 2, double Crest = 0, bool bOpen = false)
		{
			if (bOpen)
			{
				// (a length of body that meets its neighbours end to end: no caps, so no ring of shade where they join)
				TArray<FVector> Po;
				TArray<FLinearColor> Co;
				for (const FSta& St : S)
					for (int32 u = 0; u < Sides; u++)
					{
						const double Ang = 2 * PI * u / Sides;
						double Z = St.H * SgnPow(FMath::Cos(Ang), 2 / Pw);
						if (Crest > 0) { if (u == 0) Z += Crest * St.H; else if (u * 2 == Sides) Z -= Crest * .5 * St.H; }
						const FVector Q(St.X, St.W * SgnPow(FMath::Sin(Ang), 2 / Pw), St.Z + Z);
						Po.Add(Q);
						Co.Add(Paint(0, Ang > PI ? 2 * PI - Ang : Ang, Q));
					}
				const int32 Base = M.V.Num();
				for (int32 i = 0; i < Po.Num(); i++)
				{
					const FSta& St = S[i / Sides];
					M.AddVert(Po[i], FVector(0, Po[i].Y / FMath::Max(1e-6, St.W * St.W), (Po[i].Z - St.Z) / FMath::Max(1e-6, St.H * St.H)).GetSafeNormal(.0001, FVector(0, 0, 1)), Co[i]);
				}
				for (int32 v = 0; v + 1 < S.Num(); v++)
					for (int32 u = 0; u < Sides; u++)
					{
						const int32 a = Base + v * Sides + u, b = Base + v * Sides + (u + 1) % Sides, c = Base + (v + 1) * Sides + (u + 1) % Sides, d = Base + (v + 1) * Sides + u;
						M.T.Append({a, b, c, a, c, d});
					}
				return;
			}
			const int32 NV = S.Num() + 2;
			TArray<FVector> P;
			TArray<FLinearColor> Col;
			const double X0 = S[0].X, X1 = S.Last().X;
			for (int32 v = 0; v < NV; v++)
			{
				const FSta& St = S[FMath::Clamp(v - 1, 0, S.Num() - 1)];
				const bool bCap = v == 0 || v == NV - 1;
				for (int32 u = 0; u < Sides; u++)
				{
					const double Ang = 2 * PI * u / Sides;
					double Y = St.W * SgnPow(FMath::Sin(Ang), 2 / Pw), Z = St.H * SgnPow(FMath::Cos(Ang), 2 / Pw);
					if (Crest > 0) { if (u == 0) Z += Crest * St.H; else if (u * 2 == Sides) Z -= Crest * .5 * St.H; }
					const FVector Q = bCap ? FVector(St.X, 0, St.Z) : FVector(St.X, Y, St.Z + Z);
					P.Add(Q);
					Col.Add(Paint(FMath::Abs(X1 - X0) > 1e-9 ? (St.X - X0) / (X1 - X0) : 0, Ang > PI ? 2 * PI - Ang : Ang, Q));
				}
			}
			const FSta& Mid = S[S.Num() / 2];
			SheetUE(M, P, Col, Sides, NV, FVector(Mid.X, 0, Mid.Z), S.Num() / 2 + 1);
		}

		// a tube swept along a path, a radius at every point, closed at both ends (limbs, arms, feelers)
		void Sweep(FKeysMesh& M, const TArray<FVector>& Pts, const TArray<double>& R, int32 Sides, FPaint Paint, const FVector& UpHint = FVector(0, 0, 1))
		{
			const int32 N = Pts.Num(), NV = N + 2;
			if (N < 2) return;
			TArray<FVector> P;
			TArray<FLinearColor> Col;
			for (int32 v = 0; v < NV; v++)
			{
				const int32 i = FMath::Clamp(v - 1, 0, N - 1);
				const FVector Tan = (Pts[FMath::Min(N - 1, i + 1)] - Pts[FMath::Max(0, i - 1)]).GetSafeNormal(.0001, FVector(1, 0, 0));
				FVector Side = FVector::CrossProduct(UpHint, Tan);
				if (Side.SizeSquared() < 1e-6) Side = FVector::CrossProduct(FVector(0, 1, 0), Tan);
				Side.Normalize();
				const FVector Up = FVector::CrossProduct(Tan, Side);
				const bool bCap = v == 0 || v == NV - 1;
				for (int32 u = 0; u < Sides; u++)
				{
					const double Ang = 2 * PI * u / Sides;
					const FVector Q = bCap ? Pts[i] : Pts[i] + (Side * FMath::Sin(Ang) + Up * FMath::Cos(Ang)) * R[i];
					P.Add(Q);
					Col.Add(Paint(double(i) / (N - 1), Ang > PI ? 2 * PI - Ang : Ang, Q));
				}
			}
			SheetUE(M, P, Col, Sides, NV, Pts[0], 1);
		}
		void Limb(FKeysMesh& M, const TArray<FVector>& Pts, double R0, double R1, int32 Sides, const FLinearColor& A, const FLinearColor& B, double Bands = 0)
		{
			TArray<double> R;
			for (int32 i = 0; i < Pts.Num(); i++) R.Add(FMath::Lerp(R0, R1, double(i) / FMath::Max(1, Pts.Num() - 1)));
			Sweep(M, Pts, R, Sides, [&](double T, double, const FVector&) { return Bands > 0 ? (FMath::Fmod(T * Bands, 1.0) < .5 ? A : B) : FMath::Lerp(A, B, float(T)); });
		}

		// a round lump with one colour, or a pupil looking along Look
		void Ball(FKeysMesh& M, const FVector& C, const FVector& Rad, const FLinearColor& Col, int32 Sides = 8, const FVector& Look = FVector::ZeroVector, const FLinearColor& Pupil = FLinearColor::Black, double PupilCos = .6)
		{
			TArray<FVector> P;
			TArray<FLinearColor> Cl;
			const int32 NV = Sides / 2 + 2;
			for (int32 v = 0; v < NV; v++)
				for (int32 u = 0; u < Sides; u++)
				{
					const double Th = PI * v / (NV - 1), Ph = 2 * PI * u / Sides;
					const FVector S(FMath::Cos(Th), FMath::Sin(Th) * FMath::Cos(Ph), FMath::Sin(Th) * FMath::Sin(Ph));
					P.Add(C + S * Rad);
					Cl.Add(!Look.IsZero() && FVector::DotProduct(S, Look) > PupilCos ? Pupil : Col);
				}
			SheetUE(M, P, Cl, Sides, NV, C, NV / 2);
		}

		void Fin(FKeysMesh& M, const FVector& A, const FVector& B, const FVector& C, const FLinearColor& Col)
		{
			const FVector N = FVector::CrossProduct(B - A, C - A).GetSafeNormal(.0001, FVector(0, 1, 0));
			M.AddTri(A, B, C, Col, N);
		}
		void Fin4(FKeysMesh& M, const FVector& A, const FVector& B, const FVector& C, const FVector& D, const FLinearColor& Col) { Fin(M, A, B, C, Col); Fin(M, A, C, D, Col); }

		// ================================================================ the animals' meshes
		void BuildEyes(FKeysMesh& M)
		{
			// a pair of eyes that catch the light from the back of a hole (whose they are is not told)
			M.Surf = SurfEye;
			for (const double Sd : {-1.0, 1.0}) Ball(M, FVector(0, Sd * .022, 0), FVector(.009, .011, .011), Hex(0xc9d4a8), 8, FVector(1, 0, 0), Hex(0x0a0a08), .55);
		}

		// a moray's head (1 long, the neck at x 0, the snout at 1), its lower jaw (hinge at the origin) and one length of
		// body (x -.5 .. .5, a unit across and high, with the fin's ridge along the back). bSpot: the spotted moray.
		FLinearColor MorayPaint(bool bSpot, const FVector& P, double Ang, double Scale)
		{
			if (!bSpot)
			{
				// green moray: one even colour, a yellow-green that is really a brown skin under yellow slime
				const FLinearColor C = FMath::Lerp(Hex(0x55702a), Hex(0x7d8c3e), float(Smooth(Ang, 1.2, 2.9)));
				return Mul(C, .88 + .24 * Noise3(P * (9 / Scale), 3.3));
			}
			// spotted moray: pale, crowded with small dark spots that run together
			const double N = Noise3(P * (26 / Scale), 7.1) * .6 + Noise3(P * (61 / Scale) + FVector(4.2), 9.4) * .4;
			return FMath::Lerp(Hex(0xe9e2c6), Hex(0x30242a), float(Smooth(N, .44, .56) * (Ang > 2.6 ? .5 : 1)));
		}
		void BuildMorayHead(FKeysMesh& M, bool bSpot)
		{
			M.Surf = SurfSkin;
			const TArray<FSta> S = {{0, 0, .21, .33}, {.2, .005, .215, .325}, {.4, .02, .2, .29}, {.5, .05, .185, .23}, {.6, .085, .16, .15}, {.75, .08, .125, .1}, {.9, .07, .09, .07}, {.99, .06, .05, .04}};
			Loft(M, S, 12, [&](double T, double Ang, const FVector& P)
			{
				// the roof of the mouth is pale
				if (T > .5 && Ang > 2.2) return Hex(bSpot ? 0xd9c9c0 : 0xb9b08a);
				return MorayPaint(bSpot, P, Ang, 1.0);
			}, 2, .0);
			M.Surf = SurfEye;
			for (const double Sd : {-1.0, 1.0})
			{
				Ball(M, FVector(.68, Sd * .118, .15), FVector(.04, .028, .04), Hex(bSpot ? 0xe8dca0 : 0xb9c4c0), 8, FVector(.2, Sd, .1).GetSafeNormal(), Hex(0x080808), .72);
				// the nostril tubes at the tip of the snout
				M.Surf = SurfSkin;
				M.AddTube(FVector(.95, Sd * .04, .09), FVector(1.02, Sd * .05, .13), .014, .01, 5, bSpot ? Hex(0xd8cfae) : Hex(0x6d8236));
				M.Surf = SurfEye;
			}
			// teeth along the upper jaw
			M.Surf = SurfDefault;
			for (int32 i = 0; i < 6; i++)
				for (const double Sd : {-1.0, 1.0})
				{
					const double X = .58 + i * .07, Y = Sd * (.15 - i * .016);
					Fin(M, FVector(X - .012, Y, .0), FVector(X + .012, Y, .0), FVector(X, Y, -.035), Hex(0xf0ead8));
				}
		}
		void BuildMorayJaw(FKeysMesh& M, bool bSpot)
		{
			// (its own frame: the hinge at the origin, the chin toward +x)
			M.Surf = SurfSkin;
			const TArray<FSta> S = {{-.03, -.01, .17, .1}, {.12, -.03, .16, .085}, {.3, -.03, .12, .06}, {.44, -.015, .08, .04}, {.52, -.005, .04, .02}};
			Loft(M, S, 10, [&](double, double Ang, const FVector& P)
			{
				if (Ang < .9) return Hex(bSpot ? 0xd9c9c0 : 0xb9b08a);   // the floor of the mouth
				return MorayPaint(bSpot, P, PI, 1.0);
			});
			M.Surf = SurfDefault;
			for (int32 i = 0; i < 5; i++)
				for (const double Sd : {-1.0, 1.0})
				{
					const double X = .12 + i * .075, Y = Sd * (.13 - i * .018);
					Fin(M, FVector(X - .012, Y, .03), FVector(X + .012, Y, .03), FVector(X, Y, .065), Hex(0xf0ead8));
				}
		}
		void BuildMoraySeg(FKeysMesh& M, bool bSpot)
		{
			M.Surf = SurfSkin;
			const TArray<FSta> S = {{-.5, 0, .5, .5}, {-.3, 0, .5, .5}, {-.1, 0, .5, .5}, {.1, 0, .5, .5}, {.3, 0, .5, .5}, {.5, 0, .5, .5}};
			// (painted at the size of a real body: a unit here is some 8 cm across and 14 long)
			Loft(M, S, 12, [&](double, double Ang, const FVector& P) { return MorayPaint(bSpot, FVector(P.X * .14, P.Y * .06, P.Z * .09), Ang, .13); }, 2, .38, true);
		}

		// Caribbean spiny lobster, at 1 m long in the body (it is scaled to 25 - 45 cm): the carapace with its two
		// horns over the eyes, the tail with its cream spots and fan; one side's five walking legs; one antenna
		void BuildLobsterBody(FKeysMesh& M)
		{
			const FLinearColor Shell = Hex(0x8a5632), Dark = Hex(0x4f2f20), Cream = Hex(0xe9d9a2), TailC = Hex(0x9a6a3e);
			M.Surf = SurfDefault;
			// carapace: x 0 (its back edge) to .42
			const TArray<FSta> Car = {{0, 0, .1, .085}, {.1, .005, .112, .1}, {.25, .005, .108, .098}, {.36, 0, .085, .075}, {.42, -.005, .05, .045}};
			Loft(M, Car, 12, [&](double, double Ang, const FVector& P)
			{
				const double N = Noise3(P * 21.0, 5.5);
				if (Ang > 2.4) return Hex(0xc9a878);
				return FMath::Lerp(Shell, Dark, float(Smooth(N, .45, .7) * .8));
			}, 2.4);
			// the tail: six rings curling a little under, each with its pair of cream spots, then the fan
			const TArray<FSta> Tail = {{-.44, -.05, .05, .028}, {-.36, -.03, .07, .04}, {-.27, -.015, .08, .05}, {-.18, -.005, .088, .06}, {-.09, 0, .095, .07}, {.02, 0, .1, .08}};
			Loft(M, Tail, 12, [&](double T, double Ang, const FVector& P)
			{
				if (Ang > 2.3) return Hex(0xd8c090);
				const double Seg = FMath::Frac(T * 6);
				const bool bSpotHere = Ang > .75 && Ang < 1.35 && Seg > .3 && Seg < .75;
				if (bSpotHere) return Cream;
				return Mul(TailC, Seg < .1 ? .6 : .9 + .2 * Noise3(P * 30.0, 2.2));
			}, 2.3);
			for (int32 i = -2; i <= 2; i++)
			{
				const double A0 = i * .42 - .2, A1 = i * .42 + .2;
				const FVector O(-.43, 0, -.05);
				const FVector B = O + FVector(-.17 * FMath::Cos(A0), .17 * FMath::Sin(A0), -.02), C = O + FVector(-.17 * FMath::Cos(A1), .17 * FMath::Sin(A1), -.02);
				Fin(M, O, B, C, i % 2 ? Hex(0x7a5030) : Hex(0x9a6a3e));
				Fin4(M, B, C, O + (C - O) * 1.08, O + (B - O) * 1.08, Hex(0x2a1c14));   // the dark band along the fan's edge
			}
			// the two horns over the eyes, and the eyes on their stalks
			for (const double Sd : {-1.0, 1.0})
			{
				Limb(M, {FVector(.36, Sd * .045, .06), FVector(.43, Sd * .05, .11), FVector(.5, Sd * .052, .12)}, .016, .003, 5, Shell, Cream);
				M.AddTube(FVector(.4, Sd * .07, .03), FVector(.44, Sd * .095, .05), .012, .012, 5, Dark);
				M.Surf = SurfEye;
				Ball(M, FVector(.445, Sd * .1, .052), FVector(.017, .017, .017), Hex(0x14100c), 6);
				M.Surf = SurfDefault;
				// the small forked feelers between the antennae
				Limb(M, {FVector(.42, Sd * .02, .0), FVector(.55, Sd * .035, .03), FVector(.68, Sd * .06, .02)}, .008, .002, 4, Hex(0x5a4a6a), Cream, 6);
			}
		}
		void BuildLobsterLegs(FKeysMesh& M)
		{
			// one side's five legs (the other side is this one mirrored): striped lengthwise in life, banded here
			M.Surf = SurfDefault;
			for (int32 l = 0; l < 5; l++)
			{
				const double X = .34 - l * .075, Sw = (l - 2) * .05;
				const FVector A(X, .085, -.03), B(X + Sw * .6, .22 + l * .005, .03), C(X + Sw * 1.3, .33, -.07), D(X + Sw * 1.6, .36, -.13);
				Limb(M, {A, B, C, D}, .014, .004, 5, Hex(0xb89a66), Hex(0x4a4f7a), 7);
			}
		}
		void BuildLobsterAntenna(FKeysMesh& M)
		{
			// from its socket along +x: three thick spiny joints, then the whip, longer than the animal
			M.Surf = SurfDefault;
			const FLinearColor Base = Hex(0x7a3f2a), Whip = Hex(0xb07040), Pale = Hex(0xe0c898);
			Limb(M, {FVector(0, 0, 0), FVector(.07, 0, .012), FVector(.15, 0, .016), FVector(.22, 0, .02)}, .026, .017, 6, Base, Hex(0x9a5a36));
			for (int32 i = 0; i < 5; i++) Fin(M, FVector(.03 + i * .04, 0, .02), FVector(.05 + i * .04, 0, .022), FVector(.045 + i * .04, 0, .05), Pale);
			TArray<FVector> Pts;
			for (int32 i = 0; i <= 10; i++) { const double T = i / 10.0; Pts.Add(FVector(.22 + T * 1.15, 0, .02 + .16 * T - .22 * T * T)); }
			Limb(M, Pts, .02, .004, 5, Whip, Pale, 9);
		}

		// Caribbean reef octopus at life size: mantle and head with the eyes raised on top; one arm along +x, 1 long
		FLinearColor OctoPaint(const FVector& P, double Ang)
		{
			// pale blue-green with a sheen, brown mottling on the back
			const double N = Noise3(P * 40.0, 4.4), N2 = Noise3(P * 110.0, 8.8);
			FLinearColor C = FMath::Lerp(Hex(0x7fd0c4), Hex(0x4fa8a4), float(Smooth(N, .35, .7)));
			C = FMath::Lerp(C, Hex(0x8a6a4a), float(Smooth(N2, .66, .8) * .5));
			return FMath::Lerp(C, Hex(0xdcecdc), float(Smooth(Ang, 2.0, 2.9)));
		}
		void BuildOctoBody(FKeysMesh& M)
		{
			M.Surf = SurfSkin;
			// the mantle, a sac that stands up and back from the head; the head between the eyes; the crown the arms spring from
			const TArray<FSta> S = {{-.17, .1, .028, .03}, {-.13, .1, .05, .052}, {-.07, .085, .058, .06}, {-.02, .06, .046, .046}, {.02, .04, .044, .034}, {.055, .028, .05, .028}, {.085, .02, .04, .02}};
			Loft(M, S, 12, [&](double, double Ang, const FVector& P) { return OctoPaint(P, Ang); });
			M.Surf = SurfEye;
			for (const double Sd : {-1.0, 1.0})
			{
				M.Surf = SurfSkin;
				Ball(M, FVector(.02, Sd * .036, .07), FVector(.02, .016, .02), Hex(0x72c0b4), 8);
				M.Surf = SurfEye;
				Ball(M, FVector(.024, Sd * .049, .074), FVector(.012, .006, .011), Hex(0xe0cf90), 6, FVector(0, Sd, 0), Hex(0x0c0c0a), .45);
			}
		}
		void BuildOctoArm(FKeysMesh& M)
		{
			M.Surf = SurfSkin;
			TArray<FVector> Pts;
			TArray<double> R;
			// out along the ground, then the tip curled up and back on itself
			for (int32 i = 0; i <= 18; i++)
			{
				const double T = i / 18.0;
				FVector Q;
				if (T < .7) Q = FVector(T, .05 * FMath::Sin(T * 5.0), .012 * FMath::Sin(T * 9.0));
				else
				{
					const double A = (T - .7) / .3 * 4.4, Rr = .07 * (1 - (T - .7) / .3 * .7);
					Q = FVector(.7 + FMath::Sin(A) * Rr, .05 * FMath::Sin(3.5), Rr - FMath::Cos(A) * Rr);
				}
				Pts.Add(Q);
				R.Add(FMath::Lerp(.04, .007, FMath::Pow(T, .8)));
			}
			Sweep(M, Pts, R, 7, [&](double T, double Ang, const FVector& P)
			{
				if (Ang > 2.2) return Hex(FMath::Fmod(T * 26, 1.0) < .5 ? 0xf0e8dc : 0xd9c8c0);   // the suckers' side
				return OctoPaint(P * .5, 0);
			});
		}
		void BuildMidden(FKeysMesh& M)
		{
			// what an octopus leaves at its door: the shells of what it has eaten, in a litter on the sand
			M.Surf = SurfDefault;
			KeysSea::FMulberry Rng(7717);
			const uint32 Cols[] = {0xe8dcc6, 0xd9c2a8, 0xe9b9a8, 0xc9c0b0, 0xe0d0b0, 0xb99a86, 0xf0e6d8};
			for (int32 i = 0; i < 16; i++)
			{
				const double A = (Rng() - .5) * 2.4, D = .04 + Rng() * .26, Sz = .014 + Rng() * .028;
				const FVector C(FMath::Cos(A) * D, FMath::Sin(A) * D * 1.3, Sz * .25);
				const FLinearColor Cl = Mul(Hex(Cols[i % UE_ARRAY_COUNT(Cols)]), .8 + Rng() * .3);
				if (i % 4 == 3)
				{
					// a crab's emptied claw or leg
					const double Yw = Rng() * 6.28;
					Limb(M, {C, C + FVector(FMath::Cos(Yw), FMath::Sin(Yw), .1) * Sz * 1.6, C + FVector(FMath::Cos(Yw + .5), FMath::Sin(Yw + .5), 0) * Sz * 2.6}, Sz * .35, Sz * .1, 5, Hex(0xd98a5a), Hex(0xf0e0d0));
				}
				else Ball(M, C, FVector(Sz, Sz * (.7 + Rng() * .5), Sz * .38), Cl, 6);   // a clam's or a scallop's half shell, hollow side down
			}
		}

		// ---- the fish (drawn nose at +x; the pool lays them on -50..50 whatever their length)
		struct FFishFin { double X0, X1, H; };
		void FishFins(FKeysMesh& M, double TailX, double TipX, double TailUp, double TailDn, const FLinearColor& TailC, const TArray<FFishFin>& Dorsal, const TArray<FFishFin>& Anal, TFunctionRef<double(double)> TopAt, TFunctionRef<double(double)> BotAt, const FLinearColor& FinC)
		{
			M.Surf = SurfFin;
			// a forked tail
			Fin(M, FVector(TailX, 0, .012), FVector(TipX, 0, TailUp), FVector(TailX - (TailX - TipX) * .45, 0, 0), TailC);
			Fin(M, FVector(TailX, 0, -.012), FVector(TailX - (TailX - TipX) * .45, 0, 0), FVector(TipX, 0, -TailDn), TailC);
			Fin(M, FVector(TailX, 0, .012), FVector(TailX - (TailX - TipX) * .45, 0, 0), FVector(TailX, 0, -.012), TailC);
			for (const FFishFin& F : Dorsal)
				Fin4(M, FVector(F.X0, 0, TopAt(F.X0) - .004), FVector(F.X0 - (F.X0 - F.X1) * .25, 0, TopAt(F.X0) + F.H), FVector(F.X1, 0, TopAt(F.X1) + F.H * .55), FVector(F.X1, 0, TopAt(F.X1) - .004), FinC);
			for (const FFishFin& F : Anal)
				Fin4(M, FVector(F.X0, 0, BotAt(F.X0) + .004), FVector(F.X0 - (F.X0 - F.X1) * .25, 0, BotAt(F.X0) - F.H), FVector(F.X1, 0, BotAt(F.X1) - F.H * .5), FVector(F.X1, 0, BotAt(F.X1) + .004), FinC);
		}
		double StaAt(const TArray<FSta>& S, double X, bool bTop)
		{
			// (stations run from the nose back: X falling)
			for (int32 i = 0; i + 1 < S.Num(); i++)
				if (X <= S[i].X && X >= S[i + 1].X)
				{
					const double T = (S[i].X - X) / FMath::Max(1e-6, S[i].X - S[i + 1].X);
					return FMath::Lerp(S[i].Z, S[i + 1].Z, T) + (bTop ? 1 : -1) * FMath::Lerp(S[i].H, S[i + 1].H, T);
				}
			return 0;
		}
		void FishEyes(FKeysMesh& M, double X, double Y, double Z, double R, const FLinearColor& Iris)
		{
			M.Surf = SurfEye;
			for (const double Sd : {-1.0, 1.0}) Ball(M, FVector(X, Sd * Y, Z), FVector(R, R * .45, R), Iris, 8, FVector(0, Sd, 0), Hex(0x0a0808), .62);
		}
		void BuildSquirrelfish(FKeysMesh& M)
		{
			// Holocentrus adscensionis: long in the body, red on the back with pale stripes along the flanks, a yellowish
			// spiny fin, a very big eye and a deeply forked tail with the upper lobe the longer
			const TArray<FSta> S = {{.49, -.005, .012, .018}, {.44, 0, .03, .045}, {.34, 0, .05, .088}, {.2, .004, .06, .118}, {.02, .004, .058, .122}, {-.14, 0, .042, .09}, {-.26, 0, .02, .04}, {-.33, 0, .012, .028}};
			M.Surf = SurfFish;
			Loft(M, S, 12, [&](double, double Ang, const FVector& P)
			{
				if (Ang > 2.5) return Hex(0xf2e2dc);
				const bool bStripe = FMath::Fmod(Ang * 4.2 + .3, 1.0) < .42 && Ang > .5 && P.X < .36;
				return bStripe ? Hex(0xf0cfc6) : FMath::Lerp(Hex(0xc8442e), Hex(0xe0705a), float(Smooth(Ang, .3, 2.2)));
			});
			FishFins(M, -.33, -.5, .15, .11, Hex(0xd8563e), {{.24, 0, .075}, {-.02, -.2, .08}}, {{-.08, -.22, .07}}, [&](double X) { return StaAt(S, X, true); }, [&](double X) { return StaAt(S, X, false); }, Hex(0xd9b84a));
			Fin(M, FVector(.2, .06, -.03), FVector(.08, .1, -.06), FVector(.1, .06, -.09), Hex(0xe8a090));
			Fin(M, FVector(.2, -.06, -.03), FVector(.08, -.1, -.06), FVector(.1, -.06, -.09), Hex(0xe8a090));
			FishEyes(M, .35, .047, .03, .034, Hex(0xd8c8c0));
		}
		void BuildSoldierfish(FKeysMesh& M)
		{
			// Myripristis jacobus: deeper and blunter, bright red, a dark bar behind the gill cover, a huge dark eye,
			// white leading edges to the fins
			const TArray<FSta> S = {{.49, 0, .014, .03}, {.44, 0, .036, .075}, {.32, .004, .056, .13}, {.16, .006, .064, .155}, {-.02, .004, .058, .14}, {-.17, 0, .04, .09}, {-.27, 0, .02, .04}, {-.33, 0, .012, .03}};
			M.Surf = SurfFish;
			Loft(M, S, 12, [&](double, double Ang, const FVector& P)
			{
				if (FMath::Abs(P.X - .2) < .028 && Ang < 2.3) return Hex(0x2a1210);
				if (Ang > 2.5) return Hex(0xf0c8c0);
				return FMath::Lerp(Hex(0xc8281e), Hex(0xe2503c), float(Smooth(Ang, .3, 2.3)));
			});
			FishFins(M, -.33, -.5, .13, .13, Hex(0xd43a2a), {{.2, .02, .08}, {-.02, -.2, .09}}, {{-.06, -.22, .09}}, [&](double X) { return StaAt(S, X, true); }, [&](double X) { return StaAt(S, X, false); }, Hex(0xd8463a));
			// the white leading edges
			Fin(M, FVector(-.02, 0, StaAt(S, -.02, true)), FVector(-.06, 0, StaAt(S, -.02, true) + .09), FVector(-.05, 0, StaAt(S, -.02, true)), Hex(0xf6f0ec));
			Fin(M, FVector(-.06, 0, StaAt(S, -.06, false)), FVector(-.1, 0, StaAt(S, -.06, false) - .09), FVector(-.09, 0, StaAt(S, -.06, false)), Hex(0xf6f0ec));
			FishEyes(M, .36, .05, .035, .043, Hex(0x5a1a14));
		}
		void BuildSweeper(FKeysMesh& M)
		{
			// Pempheris schomburgkii: a hatchet of a fish, deep in the chest and thin as a coin, coppery and half
			// transparent, a big eye, a dark line along the base of its long anal fin
			const TArray<FSta> S = {{.49, 0, .01, .025}, {.44, -.01, .024, .07}, {.33, -.035, .034, .15}, {.2, -.05, .036, .19}, {.05, -.03, .03, .15}, {-.1, 0, .022, .085}, {-.24, .01, .012, .035}, {-.32, .01, .008, .022}};
			M.Surf = SurfFish;
			Loft(M, S, 10, [&](double, double Ang, const FVector& P)
			{
				if (Ang > 2.75 && P.X < .2 && P.X > -.26) return Hex(0x3a2418);
				return FMath::Lerp(Hex(0xb9764a), Hex(0xe0b08a), float(Smooth(Ang, .4, 2.4)));
			});
			FishFins(M, -.32, -.5, .1, .1, Hex(0xc98a60), {{.14, .02, .1}}, {{.14, -.24, .06}}, [&](double X) { return StaAt(S, X, true); }, [&](double X) { return StaAt(S, X, false); }, Hex(0xd9a078));
			FishEyes(M, .37, .034, .0, .04, Hex(0xd9c8b0));
		}

		// a crab: Wd its shell's width (m). bStone: the stone crab (a smooth oval shell, short banded legs, great claws
		// with black fingers); else the channel clinging crab (a knobbed, pear-shaped shell, long legs, spoon-tipped claws)
		void BuildCrab(FKeysMesh& M, bool bStone)
		{
			M.Surf = SurfDefault;
			const double Wd = bStone ? .12 : .16;
			const FLinearColor Shell = bStone ? Hex(0x7a4a3a) : Hex(0x8a3f30), Pale = bStone ? Hex(0xc9b08a) : Hex(0xb9785a), Leg = bStone ? Hex(0x9a4a34) : Hex(0x7a3428);
			const TArray<FSta> S = bStone
				? TArray<FSta>{{-.05, .035, Wd * .3, .02}, {-.02, .04, Wd * .48, .032}, {.02, .04, Wd * .5, .034}, {.05, .035, Wd * .36, .024}}
				: TArray<FSta>{{-.08, .05, Wd * .3, .03}, {-.04, .055, Wd * .5, .045}, {.02, .055, Wd * .44, .042}, {.07, .045, Wd * .2, .026}};
			Loft(M, S, 12, [&](double, double Ang, const FVector& P)
			{
				if (Ang > 2.2) return Pale;
				const double N = Noise3(P * (bStone ? 60.0 : 45.0), 6.1);
				return bStone ? FMath::Lerp(Shell, Hex(0x5a5048), float(Smooth(N, .55, .7) * .7)) : Mul(Shell, .75 + .5 * N);
			}, 2.3);
			if (!bStone)
				for (int32 i = 0; i < 7; i++) { const double A = i * .9; Ball(M, FVector(-.03 + .04 * FMath::Cos(A), .045 * FMath::Sin(A), .095), FVector(.012, .012, .008), Hex(0x9a4a38), 6); }
			const double LegL = bStone ? .085 : .17;
			for (const double Sd : {-1.0, 1.0})
			{
				for (int32 l = 0; l < 4; l++)
				{
					const double X = .02 - l * (bStone ? .028 : .035), Sw = (l - 1.5) * .02;
					const FVector A(X, Sd * Wd * .42, .04), B(X + Sw, Sd * (Wd * .42 + LegL * .55), .075), C(X + Sw * 2.4, Sd * (Wd * .42 + LegL), 0);
					Limb(M, {A, B, C}, bStone ? .011 : .012, .004, 5, Leg, bStone ? Hex(0xe0c890) : Hex(0x5a2a20), bStone ? 5 : 0);
				}
				// the claw: an arm out from the shell's front corner, and the hand
				const FVector Sh(.045, Sd * Wd * .34, .035), El(.1, Sd * (Wd * .5 + .02), .04), Hd(.15, Sd * Wd * .26, .035);
				Limb(M, {Sh, El, Hd}, bStone ? .02 : .016, bStone ? .024 : .016, 6, Leg, Shell);
				Ball(M, Hd + FVector(.03, 0, 0), bStone ? FVector(.045, .028, .032) : FVector(.04, .017, .02), bStone ? Hex(0x9a6a52) : Hex(0x7a4a5a), 8);
				Limb(M, {Hd + FVector(.06, 0, .012), Hd + FVector(.1, -Sd * .008, .008)}, .012, .005, 5, bStone ? Hex(0x16120e) : Hex(0xd9c8b0), bStone ? Hex(0x16120e) : Hex(0xf0e6d8));
				Limb(M, {Hd + FVector(.06, 0, -.012), Hd + FVector(.1, -Sd * .008, -.004)}, .01, .004, 5, bStone ? Hex(0x16120e) : Hex(0xd9c8b0), bStone ? Hex(0x16120e) : Hex(0xf0e6d8));
				// eyes on short stalks
				M.Surf = SurfEye;
				Ball(M, FVector(S.Last().X + .012, Sd * .022, S.Last().Z + .02), FVector(.007, .007, .007), Hex(0x18120e), 6);
				M.Surf = SurfDefault;
			}
		}

		// banded coral shrimp at life size (5 cm in the body): red and white bands, the third pair of legs long banded
		// claws held out in front; its six long white feelers are separate (one mesh, along +x)
		void BuildShrimp(FKeysMesh& M)
		{
			M.Surf = SurfDefault;
			const FLinearColor Red = Hex(0xc81e28), White = Hex(0xf4f0ea);
			const TArray<FSta> S = {{-.03, -.004, .003, .003}, {-.02, .002, .0055, .0055}, {-.005, .006, .007, .0075}, {.012, .006, .007, .008}, {.024, .004, .0045, .005}, {.03, .003, .002, .002}};
			Loft(M, S, 8, [&](double T, double, const FVector&) { return FMath::Fmod(T * 3.2 + .15, 1.0) < .5 ? White : Red; });
			for (const double Sd : {-1.0, 1.0})
			{
				// the big banded claws
				TArray<FVector> Pts = {FVector(.012, Sd * .006, 0), FVector(.03, Sd * .022, .004), FVector(.055, Sd * .03, 0), FVector(.08, Sd * .026, -.004)};
				Limb(M, Pts, .0024, .0034, 5, White, Red, 5);
				Fin(M, Pts.Last(), Pts.Last() + FVector(.016, Sd * .004, .004), Pts.Last() + FVector(.016, -Sd * .002, -.004), White);
				for (int32 l = 0; l < 3; l++)
					Limb(M, {FVector(.006 - l * .008, Sd * .005, 0), FVector(.004 - l * .01, Sd * .02, .002), FVector(.002 - l * .012, Sd * .028, -.014)}, .0012, .0008, 4, White, White);
			}
		}
		void BuildShrimpFeeler(FKeysMesh& M)
		{
			M.Surf = SurfDefault;
			TArray<FVector> Pts;
			for (int32 i = 0; i <= 6; i++) { const double T = i / 6.0; Pts.Add(FVector(T * .1, 0, .012 * FMath::Sin(T * 2.6))); }
			Limb(M, Pts, .0014, .0006, 4, Hex(0xf6f4f0), Hex(0xf6f4f0));
		}

		bool BuildPart(int32 Part, FKeysMesh& M)
		{
			switch (Part)
			{
			case PEyes: BuildEyes(M); break;
			case PGMHead: BuildMorayHead(M, false); break;
			case PGMJaw: BuildMorayJaw(M, false); break;
			case PGMSeg: BuildMoraySeg(M, false); break;
			case PSMHead: BuildMorayHead(M, true); break;
			case PSMJaw: BuildMorayJaw(M, true); break;
			case PSMSeg: BuildMoraySeg(M, true); break;
			case PLobBody: BuildLobsterBody(M); break;
			case PLobAnt: BuildLobsterAntenna(M); break;
			case PLobLegs: BuildLobsterLegs(M); break;
			case POctoBody: BuildOctoBody(M); break;
			case POctoArm: BuildOctoArm(M); break;
			case PMidden: BuildMidden(M); break;
			case PSquirrel: BuildSquirrelfish(M); break;
			case PSoldier: BuildSoldierfish(M); break;
			case PSweeper: BuildSweeper(M); break;
			case PNurse: return AKeysLife::BuildSchoolMesh(TEXT("nurse"), M, 1);
			case PGoliath: return AKeysLife::BuildSchoolMesh(TEXT("goliath"), M, 1);
			case PGrouper: return AKeysLife::BuildSchoolMesh(TEXT("grouper"), M, 1);
			case PClient: return AKeysLife::BuildSchoolMesh(TEXT("grunt"), M, 1);
			case PClingCrab: BuildCrab(M, false); break;
			case PStoneCrab: BuildCrab(M, true); break;
			case PShrimp: BuildShrimp(M); break;
			case PShrimpFeeler: BuildShrimpFeeler(M); break;
			default: return false;
			}
			return true;
		}

		// ================================================================ households
		struct FMember
		{
			FVector P = FVector::ZeroVector, Fwd = FVector(0, 0, 1), Slot = FVector::ZeroVector, Goal = FVector::ZeroVector;
			double Out = 1, Size = 1, Phase = 0, Seed = 0, T = 0, Hold = 0, U = 0, V = 0, In = 0;
			int32 Mode = 0, Step = 0;
		};
		// a den's household: one moray, a ledge's lobsters, a school of sweepers ...
		struct FHouse
		{
			FDen D;
			TArray<FMember> M;
			TArray<FVector> Trail;       // where a swimming moray's head has been, newest first
			double Out = 1;              // 1 showing at the opening, 0 drawn back into the dark
			double Scare = 0;            // seconds left before it dares come out again
			double NightK = 0;           // 0 by day .. 1 by night, eased
			double T = 0, Dist = 0, Size = 1;
			int32 State = 0;             // 0 at home; the kinds give the others their own meaning
			double StateT = 0, NextHunt = 0, Gape = 0;
			int32 Hunt = 0;              // the hunt with a grouper: 0 none, 1 it comes, 2 it shakes its head, 3 they hunt, 4 it leaves
			FMember Buddy;
			TArray<FMember> Clients;     // the fish at a shrimp's cleaning station
			bool bInit = false, bSeen = false, bNightSaid = false, bWasNight = false, bShown = false, bScared = false, bStation = false;
			FVector Where = FVector::ZeroVector;   // where its animal is now (the one to look at)
			int32 LastCause = 0;         // 1 the diver, 2 the light
			double ShyT = 0;
		};
	}

	struct FLife
	{
		AKeysReef* Reef = nullptr;
		UMaterialInstanceDynamic* Mat = nullptr;
		int32 Pool[NumParts];
		int32 Tris[NumParts];
		bool bTried[NumParts];
		TArray<FTransform> Xf[NumParts];
		TArray<float> Data[NumParts];
		TMap<int64, FHouse> Houses;
		TSet<int32> Found;
		TArray<FVector4> Threats;
		FWorldNow W;
		FLifeStats Stats;
		double CpuAcc = 0, FoundT = 0;
		int32 CpuN = 0, Links[4] = {0, 0, 0, 0};
		bool bForceHunt = false, bDirty = true;
		TArray<FHouse> Sheet;   // test: one of every resident set out in open water (kr.den sheet)
		FRandomStream Rng = FRandomStream(4242);
	};

	namespace
	{
		UInstancedStaticMeshComponent* PoolOf(FLife& L, int32 Part)
		{
			if (L.Pool[Part] >= 0) return L.Reef->DenPools[L.Pool[Part]];
			if (L.bTried[Part] || !L.Mat) return nullptr;
			L.bTried[Part] = true;
			const double T0 = FPlatformTime::Seconds();
			FKeysMesh M;
			if (!BuildPart(Part, M)) { UE_LOG(LogTemp, Warning, TEXT("KRDen: no model for the %s"), Parts[Part].Name); return nullptr; }
			int32 Tris = 0;
			UStaticMesh* SM = MakeStatic(L.Reef, M, L.Mat, FString::Printf(TEXT("DenLife_%d"), Part), Parts[Part].bFish, &Tris);
			if (!SM) return nullptr;
			L.Reef->DenMeshes.Add(SM);
			UInstancedStaticMeshComponent* C = NewObject<UInstancedStaticMeshComponent>(L.Reef, *FString::Printf(TEXT("DenLifePool_%d"), Part));
			C->SetStaticMesh(SM);
			C->SetupAttachment(L.Reef->GetRootComponent());
			C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			C->NumCustomDataFloats = 8;   // tail phase, its rate, when, amplitude, body bend, flash, shade, seed (M_KR_Boid)
			C->SetCastShadow(false);
			C->SetMaterial(0, L.Mat);
			C->SetCullDistances(0, int32(Parts[Part].Cull * 100));
			C->bAffectDistanceFieldLighting = false;
			C->RegisterComponent();
			L.Pool[Part] = L.Reef->DenPools.Add(C);
			L.Tris[Part] = Tris;
			UE_LOG(LogTemp, Log, TEXT("KRDen: %s mesh, %d triangles, drawn to %.0f m, made in %.1f ms"), Parts[Part].Name, Tris, Parts[Part].Cull, (FPlatformTime::Seconds() - T0) * 1000);
			return C;
		}

		FQuat Face(const FVector& FwdG, const FVector& UpG = FVector(0, 1, 0))
		{
			const FVector F = D2U(FwdG.GetSafeNormal(.0001, FVector(0, 0, 1)));
			FVector U = D2U(UpG);
			if (FMath::Abs(FVector::DotProduct(F, U.GetSafeNormal())) > .98) U = FVector(.3, .2, 1);
			return FRotationMatrix::MakeFromXZ(F, U).ToQuat();
		}

		void Put(FLife& L, int32 Part, const FVector& PosG, const FQuat& Q, const FVector& Scale, double Shade = 1, double Amp = 0, double Freq = 0, double Phase = 0, double Bend = 0, double Seed = 0)
		{
			L.Xf[Part].Add(FTransform(Q, G2U(PosG), Scale));
			const float D[8] = {float(FMath::Fmod(Phase, 2 * PI)), float(Freq), float(L.W.Now), float(Amp), float(Bend), 0.f, float(FMath::Clamp(Shade, 0.0, 1.5)), float(Seed)};
			L.Data[Part].Append(D, 8);
		}

		// how much light is left on something this far inside the opening (m; 0 or less is outside)
		bool GNoShade = false;   // (the specimen sheet: every animal in full light)
		double ShadeIn(double Inside) { return GNoShade ? 1.0 : FMath::Lerp(1.0, .1, Smooth(Inside, -.02, .22)); }
		double InsideOf(const FDen& D, const FVector& P) { return FVector::DotProduct(D.Mouth - P, D.Axis); }
		double ShadeAt(const FDen& D, const FVector& P) { return ShadeIn(InsideOf(D, P)); }
		// the floor of the opening (the lip of its sill), and the sand in front of the den
		double SillY(const FDen& D) { return D.Mouth.Y - D.H * .5 + .01; }
		double SandY(double X, double Z, const FDen& D) { return FMath::Min(FloorNear(X, Z), SillY(D)); }
		FVector Up3() { return FVector(0, 1, 0); }
		FVector Flat(const FVector& V) { return FVector(V.X, 0, V.Z); }

		// is something frightening at this den's door? R: how near the diver may come. 1 the diver, 2 her light, 0 no
		int32 Threat(const FLife& L, const FVector& At, double R)
		{
			const FWorldNow& W = L.W;
			if (W.bCalm || !W.bDiver) return 0;
			if (FVector::Dist(W.Diver, At) < R || FVector::Dist(W.Cam, At) < R * .8) return 1;
			if (W.bLight)
			{
				const FVector To = At - W.LightP;
				const double Dd = To.Size();
				if (Dd < 9 && Dd > .01 && FVector::DotProduct(To / Dd, W.LightD) > .975) return 2;
			}
			return 0;
		}
		const TCHAR* CauseText(int32 C) { return C == 2 ? TEXT("the dive light is on it") : TEXT("the diver"); }

		// a point's steady little wander round its place (so nothing hangs dead still)
		FVector Drift(double T, double Seed, double Amp)
		{
			return FVector(FMath::Sin(T * .7 + Seed * 9.1), FMath::Sin(T * .53 + Seed * 5.3) * .6, FMath::Cos(T * .61 + Seed * 7.7)) * Amp;
		}

		// swim a member toward its goal: turn at most Turn rad/s, move at Speed. True when within Reach of it.
		bool SwimTo(FMember& Mb, const FVector& Goal, double Speed, double Turn, double Dt, double Reach = .25)
		{
			const FVector To = Goal - Mb.P;
			const double Dd = To.Size();
			if (Dd < Reach) return true;
			const FVector Want = To / Dd;
			const double Cs = FMath::Clamp(FVector::DotProduct(Mb.Fwd, Want), -1.0, 1.0), Ang = FMath::Acos(Cs);
			if (Ang > 1e-4)
			{
				const double K = FMath::Min(1.0, Turn * Dt / Ang);
				Mb.Fwd = FMath::Lerp(Mb.Fwd, Want, K).GetSafeNormal(.0001, Want);
			}
			Mb.P += Mb.Fwd * FMath::Min(Dd, Speed * Dt) * FMath::Max(.25, Cs);
			return false;
		}

		// ---------------------------------------------------------------- morays
		struct FMorayDims { double Len, HeadL, SegL, Wd, Ht; int32 NSeg; };
		FMorayDims MorayDims(const FHouse& H)
		{
			FMorayDims Md;
			Md.Len = H.Size;
			Md.HeadL = Md.Len * .13;
			Md.Ht = Md.Len * (H.D.Res == GreenMoray ? .088 : .07);
			Md.Wd = Md.Ht * .62;
			Md.NSeg = 12;
			Md.SegL = (Md.Len - Md.HeadL) / Md.NSeg;
			return Md;
		}
		// draw a moray whose head is at Head looking along Fwd, its body following Path (points from the neck back,
		// evenly SegL apart). Gape: how far the jaw hangs open (rad). bHome: shade each part by how far in the hole it is
		void DrawMoray(FLife& L, const FHouse& H, const FVector& Neck, const FVector& Fwd, const TArray<FVector>& Path, double Gape, bool bHome, double Roll = 0)
		{
			const bool bG = H.D.Res == GreenMoray;
			const FMorayDims Md = MorayDims(H);
			const FQuat HQ = Face(Fwd) * FQuat(FVector(1, 0, 0), Roll);
			const double HeadShade = bHome ? ShadeAt(H.D, Neck + Fwd * Md.HeadL * .6) : 1;
			Put(L, bG ? PGMHead : PSMHead, Neck, HQ, FVector(Md.HeadL), HeadShade);
			// the jaw hangs from its hinge under the back of the head
			const FVector HingeU = G2U(Neck) + HQ.RotateVector(FVector(.47, 0, -.08) * Md.HeadL * 100);
			const FQuat JQ = HQ * FRotator(-FMath::RadiansToDegrees(Gape), 0, 0).Quaternion();
			L.Xf[bG ? PGMJaw : PSMJaw].Add(FTransform(JQ, HingeU, FVector(Md.HeadL)));
			const float Dj[8] = {0, 0, float(L.W.Now), 0, 0, 0, float(HeadShade), 0};
			L.Data[bG ? PGMJaw : PSMJaw].Append(Dj, 8);
			for (int32 i = 0; i + 1 < Path.Num(); i++)
			{
				const FVector A = Path[i], B = Path[i + 1], Mid = (A + B) * .5;
				const double Taper = i > Md.NSeg - 4 ? FMath::Lerp(1.0, .35, double(i - (Md.NSeg - 4)) / 3.5) : 1;
				const double Sh = bHome ? ShadeAt(H.D, Mid) : 1;
				if (bHome && InsideOf(H.D, Mid) > H.D.Deep + Md.SegL) break;   // lost in the rock: not worth an instance
				Put(L, bG ? PGMSeg : PSMSeg, Mid, Face(A - B), FVector((A - B).Size() * 1.12, Md.Wd * Taper, Md.Ht * Taper), Sh);
			}
		}

		// the body of a moray that swims: laid along where its head has been
		void TrailPath(const FHouse& H, const FMorayDims& Md, const FVector& Neck, TArray<FVector>& Path)
		{
			Path.Reset();
			Path.Add(Neck);
			double Need = Md.SegL, Acc = 0;
			FVector Prev = Neck;
			for (int32 i = 0; i < H.Trail.Num() && Path.Num() <= Md.NSeg; i++)
			{
				const FVector Q = H.Trail[i];
				double Seg = (Q - Prev).Size();
				while (Seg > 1e-6 && Acc + Seg >= Need && Path.Num() <= Md.NSeg)
				{
					const double T = (Need - Acc) / Seg;
					Prev = FMath::Lerp(Prev, Q, T);
					Path.Add(Prev);
					Seg = (Q - Prev).Size(); Acc = 0; Need = Md.SegL;
				}
				Acc += Seg; Prev = Q;
			}
			// (not enough trail yet: the rest runs straight on behind)
			while (Path.Num() <= Md.NSeg)
			{
				const FVector Dir = Path.Num() > 1 ? (Path.Last() - Path[Path.Num() - 2]).GetSafeNormal() : -H.D.Axis;
				Path.Add(Path.Last() + Dir * Md.SegL);
			}
		}

		FVector MorayStop(const FHouse& H, int32 I)
		{
			// the places it hunts: a few spots on the bottom in front of and beside its hole, 2 - 4 m off
			const double A = (Hash01(H.D.Seed, 300 + I) - .5) * 3.4, R = 2 + Hash01(H.D.Seed, 320 + I) * 2;
			const FVector Dir = (H.D.Axis * FMath::Cos(A) + H.D.Right * FMath::Sin(A));
			FVector P = H.D.Mouth + Flat(Dir).GetSafeNormal(.0001, FVector(0, 0, 1)) * R;
			P.Y = FloorNear(P.X, P.Z) + .12;
			return P;
		}

		void TickMoray(FLife& L, FHouse& H, bool bMove)
		{
			const FWorldNow& W = L.W;
			const FDen& D = H.D;
			const bool bG = D.Res == GreenMoray;
			const FMorayDims Md = MorayDims(H);
			const double Dt = bMove ? W.Dt : 0;
			// where it lies in its hole: on the floor of the opening
			const double LieY = FMath::Min(D.Mouth.Y, SillY(D) + Md.Ht * .55);
			const FVector Door(D.Mouth.X, LieY, D.Mouth.Z);
			FMember& Me = H.M[0];
			// ---- states: 0 at home, 1 out hunting, 2 going home, 3 turning round inside
			if (H.State == 0)
			{
				const int32 Th = Threat(L, D.Mouth, bG ? 1.3 : 1.0);
				if (Th && !H.bScared)
				{
					H.bScared = true; H.LastCause = Th;
					if (W.bScript) UE_LOG(LogTemp, Log, TEXT("KRDen withdraw: the %s at %.1f, %.1f draws back into its hole: %s (%.1f m off)"), ResName(D.Res), D.Mouth.X, D.Mouth.Z, CauseText(Th), FVector::Dist(W.Diver, D.Mouth));
				}
				if (Th) H.Scare = 4.5;
				else if (H.Scare > 0) H.Scare -= Dt;
				if (!Th && H.Scare <= 0 && H.bScared)
				{
					H.bScared = false;
					if (W.bScript) UE_LOG(LogTemp, Log, TEXT("KRDen withdraw: the %s at %.1f, %.1f comes out again"), ResName(D.Res), D.Mouth.X, D.Mouth.Z);
				}
				const double Want = H.bScared ? 0 : 1;
				H.Out += FMath::Clamp(Want - H.Out, -Dt * 3.2, Dt * .3);
				// leave to hunt: at night, or by day when the grouper has come for it
				const bool bLeave = (W.bNight && !H.bScared && H.StateT > 6) || H.Hunt == 3;
				H.StateT += Dt;
				if (bLeave && H.Out > .9)
				{
					H.State = 1; H.StateT = 0; Me.Step = 0; Me.Hold = 0;
					Me.P = Door + D.Axis * (Md.Len * .2); Me.Fwd = D.Axis;
					// its body is still in the hole: the trail starts as the line it lay along
					H.Trail.Reset();
					for (int32 i = 1; i <= 60; i++) H.Trail.Add(Me.P - D.Axis * (i * .04));
					if (W.bScript && H.Hunt != 3) UE_LOG(LogTemp, Log, TEXT("KRDen night: the %s at %.1f, %.1f leaves its hole to hunt (hour %.1f)"), ResName(D.Res), D.Mouth.X, D.Mouth.Z, W.Hour);
				}
			}
			else if (H.State == 1 || H.State == 2)
			{
				const int32 NStops = H.Hunt == 3 ? 2 : 3;
				FVector Goal = H.State == 2 ? Door + D.Axis * .35 : MorayStop(H, Me.Step);
				const bool bHurry = Threat(L, Me.P, 1.0) != 0;
				if (bHurry && H.State == 1) { H.State = 2; Me.Hold = 0; }
				if (Me.Hold > 0)
				{
					// nosing into the bottom at a stop
					Me.Hold -= Dt;
					if (Me.Hold <= 0) { Me.Step++; if (Me.Step >= NStops || (!W.bNight && H.Hunt != 3)) H.State = 2; }
				}
				else
				{
					// weave as it goes: a moray swims by throwing its whole length into waves
					const FVector Side = FVector::CrossProduct(Up3(), Me.Fwd).GetSafeNormal(.0001, D.Right);
					const FVector Aim = Goal + Side * FMath::Sin(H.T * 2.4) * .25;
					if (SwimTo(Me, Aim, bHurry ? .7 : .26, 1.6, Dt, .3))
					{
						if (H.State == 2) { H.State = 3; H.StateT = 0; }
						else Me.Hold = 3 + Hash01(D.Seed, 340 + Me.Step) * 3;
					}
					Me.P.Y = FMath::Max(Me.P.Y, FloorNear(Me.P.X, Me.P.Z) + .1);
				}
				if (Dt > 0 && (H.Trail.Num() == 0 || (H.Trail[0] - Me.P).Size() > .035)) { H.Trail.Insert(Me.P, 0); if (H.Trail.Num() > 140) H.Trail.SetNum(140); }
			}
			else if (H.State == 3)
			{
				// in at the door head first, and round inside where no one sees
				H.StateT += Dt;
				Me.P += -D.Axis * (.45 * Dt);
				if (Dt > 0) { H.Trail.Insert(Me.P, 0); if (H.Trail.Num() > 140) H.Trail.SetNum(140); }
				Me.Fwd = (-D.Axis + FVector(0, -.05, 0)).GetSafeNormal();
				if (H.StateT > (Md.Len + D.Deep) / .45 + 1.5)
				{
					H.State = 0; H.StateT = 0; H.Out = 0; H.bScared = false; H.Scare = 0;
					if (H.Hunt == 3) H.Hunt = 4;
					if (W.bScript) UE_LOG(LogTemp, Log, TEXT("KRDen: the %s at %.1f, %.1f is back in its hole"), ResName(D.Res), D.Mouth.X, D.Mouth.Z);
				}
			}
			// ---- draw
			H.Gape = .08 + .2 * (.5 + .5 * FMath::Sin(H.T * 2 * PI / 1.7));
			TArray<FVector> Path;
			if (H.State == 0)
			{
				// head and a little body out, swaying a little; drawn back, only the snout shows in the dark
				const double OutLen = FMath::Lerp(-Md.HeadL * 1.25, Md.Len * .2 - Md.HeadL, Smooth(H.Out, 0, 1));
				const double Sway = .22 * FMath::Sin(H.T * .45 + Me.Seed) * H.Out, Lift = (.16 + .06 * FMath::Sin(H.T * .3 + 1)) * H.Out;
				const FVector Fwd = (D.Axis * FMath::Cos(Sway) + D.Right * FMath::Sin(Sway) + Up3() * Lift).GetSafeNormal();
				const FVector NeckBase = Door + D.Axis * OutLen;
				const FVector Neck = NeckBase + Up3() * (Lift * Md.Len * .1);
				Path.Add(Neck);
				for (int32 i = 1; i <= Md.NSeg; i++)
				{
					const double Back = i * Md.SegL;
					FVector Q = NeckBase - D.Axis * Back;
					Q.Y = FMath::Max(LieY, Neck.Y - Back * .5);
					if (Q.Y > LieY + 1e-3) Q = Neck - Fwd * Back * .9 + FVector(0, -Back * .2, 0);
					Q.Y = FMath::Max(Q.Y, LieY);
					Path.Add(Q);
				}
				DrawMoray(L, H, Neck, Fwd, Path, H.Gape, true);
				H.Where = Neck + Fwd * Md.HeadL * .5;
				if (H.Out > .55) L.Threats.Add(FVector4(H.Where.X, H.Where.Y, H.Where.Z, bG ? 1.6 : 1.0));
			}
			else
			{
				const FVector Fwd = Me.Hold > 0 ? (Me.Fwd + FVector(0, -.45 - .15 * FMath::Sin(H.T * 3), 0)).GetSafeNormal() : Me.Fwd;
				TrailPath(H, Md, Me.P, Path);
				// (a swimming moray rolls a little with each wave of its body)
				DrawMoray(L, H, Me.P, Fwd, Path, H.Gape, H.State == 3, .12 * FMath::Sin(H.T * 2.4));
				H.Where = Me.P;
				L.Threats.Add(FVector4(Me.P.X, Me.P.Y, Me.P.Z, bG ? 1.8 : 1.1));
			}

			// ---- the grouper that hunts with it (green morays, by day): it comes to the hole and shakes its head; the
			// moray comes out and they work the bottom together, the grouper over the moray; then each goes its way
			if (!D.bBuddy) return;
			FMember& Gr = H.Buddy;
			const double GLen = .85 + Hash01(D.Seed, 360) * .25;
			const FVector Wait = D.Mouth + D.Axis * 1.0 + Up3() * .45;
			if (H.Hunt == 0)
			{
				H.NextHunt -= Dt;
				const bool bClear = !W.bDiver || W.bCalm || FVector::Dist(W.Diver, D.Mouth) > 2.5;
				if ((H.NextHunt <= 0 || L.bForceHunt) && !W.bNight && H.State == 0 && !H.bScared && bClear && bMove)
				{
					H.Hunt = 1; L.bForceHunt = false;
					Gr.P = D.Mouth + D.Axis * 7 + D.Right * 3 + Up3() * 1.6; Gr.Fwd = -D.Axis;
					L.Links[0]++;
					if (W.bScript) UE_LOG(LogTemp, Log, TEXT("KRDen link hunt: a black grouper comes to the green moray's hole at %.1f, %.1f (cause: day, hour %.1f; the moray is at home and showing; no diver within 2.5 m)"), D.Mouth.X, D.Mouth.Z, W.Hour);
				}
			}
			else if (H.Hunt == 1)
			{
				if (SwimTo(Gr, Wait, .8, 1.4, Dt, .3)) { H.Hunt = 2; Gr.T = 0; }
			}
			else if (H.Hunt == 2)
			{
				// the head shake: the grouper's signal to the moray
				Gr.T += Dt;
				Gr.Fwd = (-D.Axis + D.Right * (.5 * FMath::Sin(Gr.T * 11)) + FVector(0, -.25, 0)).GetSafeNormal();
				if (Gr.T > 2.6)
				{
					H.Hunt = 3;
					if (W.bScript) UE_LOG(LogTemp, Log, TEXT("KRDen link hunt: the grouper shakes its head at the hole at %.1f, %.1f (effect: the moray comes out and they hunt the bottom together, the grouper above it)"), D.Mouth.X, D.Mouth.Z);
				}
			}
			else if (H.Hunt == 3)
			{
				// over and a little behind the moray's head
				const FVector Over = H.Where + Up3() * .55 - Flat(Me.Fwd) * .35 + D.Right * .3;
				SwimTo(Gr, Over, .5, 1.6, Dt, .15);
				if (H.State == 1 && Me.Hold > 0) Gr.Fwd = (Gr.Fwd + (H.Where - Gr.P).GetSafeNormal() * Dt * 2).GetSafeNormal();
			}
			else if (H.Hunt == 4)
			{
				const FVector Away = D.Mouth + D.Axis * 9 - D.Right * 6 + Up3() * 2;
				if (SwimTo(Gr, Away, .7, 1.2, Dt, .6))
				{
					H.Hunt = 0; H.NextHunt = 70 + Hash01(D.Seed, 361) * 60;
					if (W.bScript) UE_LOG(LogTemp, Log, TEXT("KRDen link hunt: over at %.1f, %.1f; the moray is back in its hole and the grouper has gone"), D.Mouth.X, D.Mouth.Z);
				}
			}
			if (H.Hunt > 0)
			{
				const double Sp = H.Hunt == 2 ? .5 : .28;
				Put(L, PGrouper, Gr.P, Face(Gr.Fwd), FVector(GLen), 1, Sp, H.Hunt == 2 ? 7 : 3.2, H.T * 3.2, 0, Me.Seed);
			}
		}

		// ---------------------------------------------------------------- lobsters
		void DrawLobster(FLife& L, const FMember& Mb, const FDen& D, bool bHome, double AntYaw, double AntPitch, const FVector* PointAt, double Walk)
		{
			const double S = Mb.Size;
			const FVector Fwd = Mb.Fwd.GetSafeNormal(.0001, D.Axis);
			const FQuat Q = Face(Fwd);
			// the carapace's own shade, and the antennae's (they are further out)
			const double Sh = bHome ? ShadeAt(D, Mb.P + Fwd * .25 * S) : 1;
			Put(L, PLobBody, Mb.P, Q, FVector(S), Sh);
			// the legs: each side rocks against the other as it walks
			const double Rock = Walk * 9 * FMath::Sin(Mb.T * 7);
			L.Xf[PLobLegs].Add(FTransform(Q * FRotator(0, 0, Rock).Quaternion(), G2U(Mb.P), FVector(S)));
			L.Xf[PLobLegs].Add(FTransform(Q * FRotator(0, 0, Rock).Quaternion(), G2U(Mb.P), FVector(S, -S, S)));
			const float Dl[8] = {0, 0, float(L.W.Now), 0, 0, 0, float(Sh), 0};
			L.Data[PLobLegs].Append(Dl, 8); L.Data[PLobLegs].Append(Dl, 8);
			const FVector Side = FVector::CrossProduct(Up3(), Fwd).GetSafeNormal(.0001, D.Right);
			for (int32 k = 0; k < 2; k++)
			{
				const double Sd = k ? 1 : -1;
				const FVector Base = Mb.P + Fwd * (.4 * S) + Side * (Sd * .055 * S) + Up3() * (.03 * S);
				FVector Dir;
				if (PointAt)
				{
					// both whips swung onto whatever has come too near
					Dir = ((*PointAt - Base).GetSafeNormal() + Side * (Sd * .12) + Up3() * (.08 * FMath::Sin(Mb.T * 5 + k))).GetSafeNormal();
					if (FVector::DotProduct(Dir, Fwd) < .1) Dir = (Fwd * .4 + Side * Sd + Up3() * .2).GetSafeNormal();
				}
				else
				{
					const double Yw = Sd * (AntYaw + .22 * FMath::Sin(Mb.T * (1.1 + .3 * k) + Mb.Seed * 6 + k * 1.7)), Pt = AntPitch + .14 * FMath::Sin(Mb.T * .8 + Mb.Seed * 3 + k);
					Dir = (Fwd * FMath::Cos(Yw) + Side * FMath::Sin(Yw)) * FMath::Cos(Pt) + Up3() * FMath::Sin(Pt);
				}
				Put(L, PLobAnt, Base, Face(Dir), FVector(S), bHome ? ShadeAt(D, Base + Dir * .3 * S) : 1);
			}
		}

		void TickLobsters(FLife& L, FHouse& H, bool bMove)
		{
			const FWorldNow& W = L.W;
			const FDen& D = H.D;
			const double Dt = bMove ? W.Dt : 0;
			int32 Flicked = 0, Walking = 0;
			for (FMember& Mb : H.M)
			{
				Mb.T += Dt;
				const double S = Mb.Size;
				// its place under the ledge: the horns a hand's width inside the lip
				const FVector Home = FVector(D.Mouth.X, SillY(D) + .13 * S, D.Mouth.Z) + D.Right * Mb.U - D.Axis * (.46 * S + .03);
				const bool bOutAtNight = W.bNight;
				if (Mb.Mode == 0)
				{
					// at home: flick back from anything that comes near (a tail-flip takes it a forearm's length in one beat)
					const int32 Th = Threat(L, Home + D.Axis * .4 * S, 1.6);
					if (Th && Mb.Hold <= 0) { Flicked++; H.LastCause = Th; }
					if (Th) Mb.Hold = 5 + Mb.Seed * 2; else Mb.Hold = FMath::Max(0.0, Mb.Hold - Dt);
					const double Want = Mb.Hold > 0 ? 0 : 1;
					Mb.Out += FMath::Clamp(Want - Mb.Out, -Dt * 9, Dt * .35);
					Mb.P = Home - D.Axis * ((1 - Mb.Out) * .38);
					Mb.Fwd = (D.Axis + D.Right * Mb.V).GetSafeNormal();
					if (bOutAtNight && Mb.Hold <= 0 && H.StateT > 3 + Mb.Seed * 14) { Mb.Mode = 1; Mb.Step = 0; }
				}
				else
				{
					// out on the sand by night: walk to a patch a few metres from the ledge, potter there, go home at dawn
					Walking++;
					const double A = (Hash01(D.Seed, 400 + int32(Mb.Seed * 97) + Mb.Step) - .5) * 2.2, R = 1.6 + Hash01(D.Seed, 420 + int32(Mb.Seed * 89) + Mb.Step) * 3.2;
					FVector Goal = D.Mouth + Flat(D.Axis * FMath::Cos(A) + D.Right * FMath::Sin(A)).GetSafeNormal(.0001, FVector(0, 0, 1)) * R;
					if (Mb.Mode == 2) Goal = Home + D.Axis * (.5 * S);
					const int32 Th = Threat(L, Mb.P, 1.4);
					if (Th && Mb.Hold <= 0)
					{
						// a tail-flip backwards, then make for home
						Mb.P -= Mb.Fwd * .7; Mb.Hold = 3; Mb.Mode = 2; Flicked++; H.LastCause = Th;
					}
					Mb.Hold = FMath::Max(0.0, Mb.Hold - Dt);
					if (!bOutAtNight && Mb.Mode == 1) Mb.Mode = 2;
					if (SwimTo(Mb, Goal, Mb.Mode == 2 ? .22 : .1, 1.2, Dt, .2))
					{
						if (Mb.Mode == 2) { Mb.Mode = 0; Mb.Out = 0; Mb.Hold = 0; }
						else if ((Mb.In -= Dt) <= 0) { Mb.Step++; Mb.In = 6 + Mb.Seed * 8; }
					}
					Mb.P.Y = SandY(Mb.P.X, Mb.P.Z, D) + .13 * S;
					Mb.Fwd = Flat(Mb.Fwd).GetSafeNormal(.0001, D.Axis);
				}
				const bool bHome = Mb.Mode == 0;
				const bool bNear = W.bDiver && !W.bCalm && FVector::Dist(W.Diver, Mb.P) < 3.2;
				DrawLobster(L, Mb, D, bHome, bHome && Mb.Out < .5 ? .9 : .38, .2, bNear ? &W.Diver : nullptr, bHome ? 0 : 1);
			}
			H.StateT += Dt;
			if (Flicked)
			{
				L.Links[1]++;
				if (W.bScript) UE_LOG(LogTemp, Log, TEXT("KRDen link flick: %d of %d spiny lobsters at %.1f, %.1f flick back %s (cause: %s within 1.6 m, %.1f m from the ledge)"),
					Flicked, H.M.Num(), D.Mouth.X, D.Mouth.Z, W.bNight ? TEXT("and make for the ledge") : TEXT("under the ledge"), CauseText(H.LastCause), FVector::Dist(W.Diver, D.Mouth));
			}
			if (W.bNight != H.bWasNight)
			{
				H.bWasNight = W.bNight; H.StateT = 0;
				if (W.bScript) UE_LOG(LogTemp, Log, TEXT("KRDen %s: %d spiny lobsters at the ledge at %.1f, %.1f %s (hour %.1f)"), W.bNight ? TEXT("night") : TEXT("day"), H.M.Num(), D.Mouth.X, D.Mouth.Z,
					W.bNight ? TEXT("walk out onto the sand") : TEXT("go back under it"), W.Hour);
			}
			H.Where = Walking && H.M.Num() ? H.M[0].P : D.Mouth;
		}

		// ---------------------------------------------------------------- octopus
		void TickOctopus(FLife& L, FHouse& H, bool bMove)
		{
			const FWorldNow& W = L.W;
			const FDen& D = H.D;
			const double Dt = bMove ? W.Dt : 0, S = H.Size;
			FMember& Me = H.M[0];
			const FVector Sill(D.Mouth.X, SillY(D), D.Mouth.Z);
			// the midden: what it has eaten, left at the door
			{
				FVector Mp = Sill + Flat(D.Axis).GetSafeNormal(.0001, FVector(0, 0, 1)) * .3;
				Mp.Y = SandY(Mp.X, Mp.Z, D) + .012;
				Put(L, PMidden, Mp, Face(Flat(D.Axis)), FVector(1.15));
			}
			const int32 Th = Threat(L, H.State == 0 ? D.Mouth : Me.P, 1.3);
			if (Th && !H.bScared) { H.bScared = true; if (W.bScript) UE_LOG(LogTemp, Log, TEXT("KRDen withdraw: the reef octopus at %.1f, %.1f pulls its arm in: %s"), D.Mouth.X, D.Mouth.Z, CauseText(Th)); }
			if (Th) H.Scare = 5; else H.Scare = FMath::Max(0.0, H.Scare - Dt);
			if (!Th && H.Scare <= 0) H.bScared = false;
			// by day 0: in the den, an eye and an arm showing. By night: 1 sitting out at the mouth, 2 crawling to a
			// spot, 3 spread over it, 4 crawling home
			H.NightK += FMath::Clamp((W.bNight ? 1.0 : 0.0) - H.NightK, -Dt * .4, Dt * .4);
			H.StateT += Dt;
			const FVector In = Sill - D.Axis * (.13 * S) + Up3() * (.03 * S), OutSeat = Sill + D.Axis * (.16 * S) + Up3() * (.03 * S);
			if (H.State == 0)
			{
				H.Out += FMath::Clamp((H.bScared ? 0.0 : 1.0) - H.Out, -Dt * 2.5, Dt * .3);
				Me.P = FMath::Lerp(In - D.Axis * (.25 * (1 - H.Out)), OutSeat, Smooth(H.NightK, 0, 1) * H.Out);
				Me.Fwd = D.Axis;
				if (W.bNight && H.NightK > .98 && H.StateT > 9 && !H.bScared) { H.State = 2; H.StateT = 0; if (W.bScript) UE_LOG(LogTemp, Log, TEXT("KRDen night: the reef octopus at %.1f, %.1f leaves its den to hunt (hour %.1f)"), D.Mouth.X, D.Mouth.Z, W.Hour); }
			}
			else
			{
				const double A = (Hash01(D.Seed, 500 + Me.Step) - .5) * 2.6, R = 1.3 + Hash01(D.Seed, 520 + Me.Step) * 1.8;
				FVector Goal = D.Mouth + Flat(D.Axis * FMath::Cos(A) + D.Right * FMath::Sin(A)).GetSafeNormal(.0001, FVector(0, 0, 1)) * R;
				if (H.State == 4) Goal = OutSeat;
				if (H.bScared && H.State != 4) { H.State = 4; H.StateT = 0; }
				if (H.State == 3)
				{
					// spread over a spot, its web a tent over whatever is under it
					if (H.StateT > 7 + Hash01(D.Seed, 540 + Me.Step) * 5) { Me.Step++; H.State = (Me.Step % 3 == 0 || !W.bNight) ? 4 : 2; H.StateT = 0; }
				}
				else if (SwimTo(Me, Goal, H.bScared ? .5 : .14, 2.0, Dt, .12))
				{
					if (H.State == 4) { H.State = 0; H.StateT = 0; Me.Step++; }
					else { H.State = 3; H.StateT = 0; }
				}
				if (H.State != 0)
				{
					Me.P.Y = SandY(Me.P.X, Me.P.Z, D) + .03 * S;
					Me.Fwd = Flat(Me.Fwd).GetSafeNormal(.0001, D.Axis);
				}
			}
			const bool bOutside = H.State != 0 || H.NightK > .5;
			const double Sh = bOutside ? 1 : ShadeAt(D, Me.P + D.Axis * .05);
			// the mantle stands up behind the head when it sits, lies back when it crawls
			const double Breathe = 1 + .06 * FMath::Sin(H.T * 1.9);
			Put(L, POctoBody, Me.P, Face(Me.Fwd, Up3()), FVector(S * 1.15, S * 1.15 * Breathe, S * 1.15 * Breathe), Sh);
			// arms: by day one or two trail out over the sill and the rest are coiled inside; out in the open all eight
			// lie spread round it, and wider when it has pounced
			const double ArmL = .46 * S;
			const FVector Side = FVector::CrossProduct(Up3(), Me.Fwd).GetSafeNormal(.0001, D.Right);
			for (int32 a = 0; a < 8; a++)
			{
				double Yaw, Len = ArmL, Curl;
				if (!bOutside)
				{
					// three trail out over the sill, feeling about; the other five lie coiled back beside it in the dark
					const bool bFront = a < 3;
					Yaw = bFront ? (a - 1) * .55 + .14 * FMath::Sin(H.T * .5 + a * 1.7) : (a - 5.0) * .5 + PI + .05 * FMath::Sin(H.T * .4 + a);
					Len = ArmL * (bFront ? (.45 + .5 * H.Out) * (.85 + .15 * FMath::Sin(H.T * .35 + a * 2.3)) : .55);
					Curl = .35 * FMath::Sin(H.T * .7 + a * 2);
				}
				else
				{
					const double Spread = H.State == 3 ? 1 : .8;
					Yaw = (a + .5) / 8.0 * 2 * PI + .1 * FMath::Sin(H.T * (H.State == 2 || H.State == 4 ? 2.6 : .6) + a * 1.3);
					Len = ArmL * Spread * (.85 + .15 * FMath::Sin(H.T * (H.State == 2 || H.State == 4 ? 2.2 : .5) + a * 2.1));
					Curl = .25 * FMath::Sin(H.T * .8 + a);
				}
				const FVector Dir = (Me.Fwd * FMath::Cos(Yaw) + Side * FMath::Sin(Yaw)).GetSafeNormal();
				const FVector Base = Me.P + Dir * (.035 * S) + Up3() * (-.012 * S);
				const FVector Aim = (Dir + Up3() * (bOutside ? -.03 : -.12)).GetSafeNormal();
				Put(L, POctoArm, Base, Face(Aim) * FQuat(FVector(1, 0, 0), Curl), FVector(Len), bOutside ? 1 : ShadeAt(D, Base + Dir * Len * .5));
			}
			H.Where = Me.P;
			if (bOutside && !H.bScared) L.Threats.Add(FVector4(Me.P.X, Me.P.Y, Me.P.Z, 1.0));
		}

		// ---------------------------------------------------------------- squirrelfish, soldierfish, sweepers
		void TickLedgeFish(FLife& L, FHouse& H, bool bMove)
		{
			const FWorldNow& W = L.W;
			const FDen& D = H.D;
			const double Dt = bMove ? W.Dt : 0;
			const bool bSweep = D.Res == Sweepers;
			const int32 Part = bSweep ? PSweeper : D.Res == Squirrelfish ? PSquirrel : PSoldier;
			const int32 Th = Threat(L, D.Mouth, 1.6);
			if (Th && !H.bScared && W.bScript) UE_LOG(LogTemp, Log, TEXT("KRDen withdraw: %d %s at %.1f, %.1f back away into the dark: %s"), H.M.Num(), ResName(D.Res), D.Mouth.X, D.Mouth.Z, CauseText(Th));
			H.bScared = Th != 0;
			H.Out += FMath::Clamp((H.bScared ? 0.0 : 1.0) - H.Out, -Dt * 2.5, Dt * .5);
			H.NightK += FMath::Clamp((W.bNight ? 1.0 : 0.0) - H.NightK, -Dt * .25, Dt * .25);
			if (W.bNight != H.bWasNight)
			{
				H.bWasNight = W.bNight; H.StateT = 0;
				if (W.bScript) UE_LOG(LogTemp, Log, TEXT("KRDen %s: %d %s at %.1f, %.1f %s (hour %.1f)"), W.bNight ? TEXT("night") : TEXT("day"), H.M.Num(), ResName(D.Res), D.Mouth.X, D.Mouth.Z,
					W.bNight ? (bSweep ? TEXT("pour out of the cave mouth and spread over the reef to feed") : TEXT("leave the ledge, each to its own beat over the bottom")) : TEXT("go back into its shadow"), W.Hour);
			}
			H.StateT += Dt;
			const FVector Centre = D.Mouth - D.Axis * (.1 + .3 * (1 - H.Out));
			for (FMember& Mb : H.M)
			{
				Mb.T += Dt;
				FVector Home, HomeFwd;
				if (bSweep)
				{
					// milling: every fish goes round the same slow wheel inside the mouth, the school turning as one
					const double Th2 = Mb.Phase + H.T * .55, Tight = .75 + .25 * H.Out;
					Home = Centre + D.Right * (FMath::Cos(Th2) * Mb.U * D.W * .36 * Tight) + Up3() * (Mb.V * D.H * .3) - D.Axis * (.05 + (FMath::Sin(Th2) * .5 + .5) * Mb.In * .5 * Tight);
					HomeFwd = (D.Right * -FMath::Sin(Th2) - D.Axis * (FMath::Cos(Th2) * .4)).GetSafeNormal();
				}
				else
				{
					// hovering in the ledge's shadow, side on to the open water
					Home = Centre + D.Right * Mb.U + Up3() * (Mb.V * D.H * .3) - D.Axis * Mb.In + Drift(H.T, Mb.Seed, .018);
					HomeFwd = (D.Right * (Mb.Seed > .5 ? 1 : -1) + D.Axis * .35).GetSafeNormal();
				}
				// 0 at home in the shadow, 1 out on its own beat over the reef by night (the sweepers keep together, a looser
				// cloud above the cave), 2 swimming home at dawn
				if (Mb.Mode == 0)
				{
					Mb.P = Home; Mb.Fwd = HomeFwd;
					if (W.bNight && H.StateT > 2 + Mb.Seed * 7 && !H.bScared) { Mb.Mode = 1; Mb.Hold = 0; }
				}
				else if (Mb.Mode == 1)
				{
					if ((Mb.Hold -= Dt) <= 0)
					{
						Mb.Step++; Mb.Hold = 5 + Hash01(D.Seed, 600 + int32(Mb.Seed * 131) + Mb.Step) * 8;
						const double A = Hash01(D.Seed, 620 + int32(Mb.Seed * 113) + Mb.Step) * 2 * PI, R = bSweep ? 1 + Hash01(D.Seed, 640 + Mb.Step + int32(Mb.Seed * 71)) * 2.2 : 2.5 + Hash01(D.Seed, 640 + Mb.Step + int32(Mb.Seed * 71)) * 6;
						const FVector Mid = bSweep ? D.Mouth + D.Axis * 2.2 + Up3() * 1.4 : D.Mouth + D.Axis * 1.5;
						Mb.Goal = Mid + FVector(FMath::Cos(A), 0, FMath::Sin(A)) * R;
						Mb.Goal.Y = FMath::Max(Mb.Goal.Y, FloorNear(Mb.Goal.X, Mb.Goal.Z) + (bSweep ? 1.0 : .35) + Mb.Seed * .8);
					}
					SwimTo(Mb, Mb.Goal, .22, 2.2, Dt, .15);
					if (!W.bNight) Mb.Mode = 2;
				}
				else if (SwimTo(Mb, Home, .5, 2.2, Dt, .15)) Mb.Mode = 0;
				const bool bAtHome = Mb.Mode == 0 || (Mb.Mode == 2 && InsideOf(D, Mb.P) > -.05);
				const double Len = Mb.Size;
				Put(L, Part, Mb.P, Face(Mb.Fwd), FVector(Len), bAtHome ? FMath::Max(.3, ShadeAt(D, Mb.P) * 1.6) : 1, bAtHome && !bSweep ? .1 : .3, bAtHome && !bSweep ? 2.5 : 5, Mb.T * 5 + Mb.Seed * 6, 0, Mb.Seed);
			}
			H.Where = H.M.Num() && H.M[0].Mode != 0 ? H.M[0].P : D.Mouth;
		}

		// ---------------------------------------------------------------- nurse shark, goliath and black grouper
		void TickBig(FLife& L, FHouse& H, bool bMove)
		{
			const FWorldNow& W = L.W;
			const FDen& D = H.D;
			const double Dt = bMove ? W.Dt : 0, Len = H.Size;
			FMember& Me = H.M[0];
			if (D.Res == NurseShark)
			{
				// asleep on the sand under the ledge, head in the dark and tail out, lying a little across the opening;
				// by night it is off hunting on a wide slow round
				const double Skew = (Me.Seed - .5) * .7;
				const FVector Lie = (Flat(D.Axis) * FMath::Cos(Skew) + D.Right * FMath::Sin(Skew)).GetSafeNormal(.0001, FVector(0, 0, 1));   // from head to tail
				FVector Rest = D.Mouth + Lie * (Len * .08) + D.Right * Me.U;
				Rest.Y = SillY(D) + Len * .062;
				if (W.bNight != H.bWasNight)
				{
					H.bWasNight = W.bNight; H.StateT = 0;
					if (W.bScript) UE_LOG(LogTemp, Log, TEXT("KRDen %s: the nurse shark under the ledge at %.1f, %.1f %s (hour %.1f)"), W.bNight ? TEXT("night") : TEXT("day"), D.Mouth.X, D.Mouth.Z, W.bNight ? TEXT("backs out and goes off to hunt") : TEXT("comes back to sleep"), W.Hour);
				}
				H.StateT += Dt;
				// 0 asleep, 1 backing out from under the ledge, 2 away on its round, 3 coming back to a point off the ledge,
				// 4 gliding in under it
				const FVector Off = Rest + Lie * (Len * .9);
				if (Me.Mode == 0)
				{
					Me.P = Rest; Me.Fwd = -Lie;
					if (W.bNight && H.StateT > 4) { Me.Mode = 1; Me.Hold = 0; }
				}
				else if (Me.Mode == 1) { Me.P += Lie * (.45 * Dt); if ((Me.Hold += Dt) > 4) Me.Mode = 2; }
				else if (Me.Mode == 2)
				{
					const double A = H.T * .06 + Me.Seed * 6;
					FVector Goal = D.Mouth + Flat(D.Axis) * 9 + FVector(FMath::Cos(A), 0, FMath::Sin(A)) * 7;
					Goal.Y = FloorNear(Goal.X, Goal.Z) + .6;
					SwimTo(Me, Goal, .5, .7, Dt, .5);
					Me.P.Y = FMath::Max(Me.P.Y, FloorNear(Me.P.X, Me.P.Z) + .35);
					if (!W.bNight) Me.Mode = 3;
				}
				else if (Me.Mode == 3) { if (SwimTo(Me, Off, .5, .9, Dt, .4)) Me.Mode = 4; }
				else if (SwimTo(Me, Rest, .3, 1.2, Dt, .08)) Me.Mode = 0;
				const bool bAsleep = Me.Mode == 0;
				// (a body this long is shaded as one: by where its shoulders are)
				Put(L, PNurse, Me.P, Face(Me.Fwd), FVector(Len), bAsleep ? FMath::Max(.45, ShadeAt(D, Me.P + Me.Fwd * Len * .1)) : 1, bAsleep ? .012 : .26, bAsleep ? .8 : 2.6, H.T * 2.6, 0, Me.Seed);
				H.Where = bAsleep ? Me.P - Me.Fwd * Len * .3 : Me.P;
				return;
			}
			// a big grouper filling the hole, looking out; it backs in when crowded
			const int32 Th = Threat(L, D.Mouth, 1.2);
			if (Th && !H.bScared && W.bScript) UE_LOG(LogTemp, Log, TEXT("KRDen withdraw: the %s at %.1f, %.1f backs into its hole: %s"), ResName(D.Res), D.Mouth.X, D.Mouth.Z, CauseText(Th));
			H.bScared = Th != 0;
			if (Th) H.Scare = 4; else H.Scare = FMath::Max(0.0, H.Scare - Dt);
			H.Out += FMath::Clamp((H.Scare > 0 ? 0.0 : 1.0) - H.Out, -Dt * 1.6, Dt * .3);
			const double Ht = Len * (D.Res == Goliath ? .36 : .29);
			FVector P = D.Mouth - D.Axis * (Len * .5 - Len * .1 * H.Out + .45 * (1 - H.Out));
			P.Y = FMath::Max(SillY(D) + Ht * .5, D.Mouth.Y - .05);
			Me.P = P + Drift(H.T, Me.Seed, .012);
			Me.Fwd = (D.Axis + D.Right * (.1 * FMath::Sin(H.T * .3))).GetSafeNormal();
			Put(L, D.Res == Goliath ? PGoliath : PGrouper, Me.P, Face(Me.Fwd), FVector(Len), FMath::Max(.35, ShadeAt(D, Me.P + Me.Fwd * Len * .42) * 1.4), .03, 1.4, H.T * 1.4, 0, Me.Seed);
			H.Where = D.Mouth;
			if (H.Out > .5) L.Threats.Add(FVector4(D.Mouth.X, D.Mouth.Y, D.Mouth.Z, 1.4));
		}

		// ---------------------------------------------------------------- crabs, shrimp (and its cleaning station), eyes
		void TickSmall(FLife& L, FHouse& H, bool bMove)
		{
			const FWorldNow& W = L.W;
			const FDen& D = H.D;
			const double Dt = bMove ? W.Dt : 0, S = H.Size;
			FMember& Me = H.M[0];
			const FVector Sill(D.Mouth.X, SillY(D), D.Mouth.Z);
			if (D.Res == ResEyes)
			{
				// a pair of eyes in the back of the hole; gone when you come to look
				const int32 Th = Threat(L, D.Mouth, 1.5);
				H.Out += FMath::Clamp((Th ? 0.0 : 1.0) - H.Out, -Dt * 3, Dt * .25);
				const double Blink = FMath::Fmod(H.T + Me.Seed * 7, 5.5) < .12 ? 0 : 1;
				const FVector P = D.Mouth - D.Axis * (.16 + .3 * (1 - H.Out)) + D.Right * Me.U + Up3() * (Me.V * D.H * .2);
				if (H.Out > .05) Put(L, PEyes, P, Face(D.Axis + D.Right * (.2 * FMath::Sin(H.T * .2 + Me.Seed))), FVector(S, S, S * Blink), .9 * H.Out);
				H.Where = D.Mouth;
				return;
			}
			if (D.Res == ClingingCrab || D.Res == StoneCrab)
			{
				const bool bCling = D.Res == ClingingCrab;
				const int32 Th = Threat(L, D.Mouth, .9);
				if (Th && !H.bScared && W.bScript) UE_LOG(LogTemp, Log, TEXT("KRDen withdraw: the %s at %.1f, %.1f backs into its hole: %s"), ResName(D.Res), D.Mouth.X, D.Mouth.Z, CauseText(Th));
				H.bScared = Th != 0;
				if (Th) H.Scare = 4; else H.Scare = FMath::Max(0.0, H.Scare - Dt);
				H.Out += FMath::Clamp((H.Scare > 0 ? 0.0 : 1.0) - H.Out, -Dt * 2.5, Dt * .3);
				H.NightK += FMath::Clamp((W.bNight ? 1.0 : 0.0) - H.NightK, -Dt * .2, Dt * .2);
				// by day in the mouth of the hole, claws at the lip; by night out on the rock beside it, grazing
				const FVector Day = Sill - D.Axis * (.06 + .22 * (1 - H.Out)), Night = Sill + D.Axis * (bCling ? .22 : .12) + D.Right * (bCling ? .2 * (Me.Seed > .5 ? 1 : -1) : 0);
				FVector P = FMath::Lerp(Day, Night, Smooth(H.NightK, 0, 1) * H.Out);
				if (H.NightK > .3) P.Y = FMath::Max(SandY(P.X, P.Z, D), P.Y - .3 * H.NightK);
				P += D.Right * (.012 * FMath::Sin(H.T * .6 + Me.Seed * 4) * H.Out);
				const FVector Fwd = (D.Axis + D.Right * (.25 * FMath::Sin(H.T * .17 + Me.Seed * 3))).GetSafeNormal();
				Put(L, bCling ? PClingCrab : PStoneCrab, P, Face(Fwd), FVector(S), H.NightK > .5 ? 1 : ShadeAt(D, P + Fwd * .05));
				H.Where = P;
				return;
			}
			// banded coral shrimp: upside down under the lip of its hole, the long white feelers out and waving. A
			// fish that wants cleaning comes, stops in front of it and holds still, head up.
			const int32 Th = Threat(L, D.Mouth, .8);
			H.bScared = Th != 0;
			if (Th) H.Scare = 4; else H.Scare = FMath::Max(0.0, H.Scare - Dt);
			H.Out += FMath::Clamp((H.Scare > 0 ? 0.0 : 1.0) - H.Out, -Dt * 2.5, Dt * .3);
			const FVector Sp = D.Mouth + Up3() * (D.H * .3) + D.Right * Me.U - D.Axis * (.015 + .15 * (1 - H.Out));
			const FVector SFwd = (D.Axis * .85 - Up3() * .5).GetSafeNormal();
			const FQuat SQ = Face(SFwd, -Up3());
			const double SSh = ShadeAt(D, Sp + D.Axis * .03);
			Put(L, PShrimp, Sp, SQ, FVector(S), SSh);
			for (int32 f = 0; f < 6; f++)
			{
				const double Yw = (f - 2.5) * .34 + .16 * FMath::Sin(H.T * (1.3 + f * .17) + f), Pt = -.15 + .25 * FMath::Sin(H.T * .9 + f * 1.9);
				const FVector Side = FVector::CrossProduct(Up3(), SFwd).GetSafeNormal(.0001, D.Right);
				const FVector Dir = ((SFwd * FMath::Cos(Yw) + Side * FMath::Sin(Yw)) * FMath::Cos(Pt) + Up3() * FMath::Sin(Pt)).GetSafeNormal();
				Put(L, PShrimpFeeler, Sp + SFwd * (.025 * S), Face(Dir), FVector(S * (1 + .15 * (f % 3))), SSh);
			}
			H.Where = Sp;
			// ---- the cleaning station: open by day while the shrimp is out and no diver is within 2.5 m
			const bool bDiverNear = W.bDiver && !W.bCalm && FVector::Dist(W.Diver, D.Mouth) < 2.5;
			const bool bOpen = !W.bNight && H.Out > .8 && !bDiverNear;
			int32 Posing = 0, Fled = 0;
			for (int32 c = 0; c < H.Clients.Num(); c++)
			{
				FMember& Cl = H.Clients[c];
				Cl.T += Dt;
				const FVector Stand = D.Mouth + D.Axis * (.3 + .14 * c) + D.Right * ((c ? -.13 : .1)) + Up3() * (D.H * .12 + .04 * c);
				const FVector Far = D.Mouth + D.Axis * 4.5 + D.Right * (c ? -3.0 : 2.5) + Up3() * 1.2;
				double Amp = .3, Freq = 5;
				FVector Fw = Cl.Fwd;
				if (Cl.Mode == 0)
				{
					// away: it comes when the station is open and its turn is up
					Cl.Hold -= Dt;
					if (bOpen && Cl.Hold <= 0 && bMove) { Cl.Mode = 1; Cl.P = Far; Cl.Fwd = (Stand - Far).GetSafeNormal(); }
					else continue;
				}
				if (Cl.Mode == 1)
				{
					if (!bOpen) { Cl.Mode = 3; Fled++; }
					else if (SwimTo(Cl, Stand, .45, 2.0, Dt, .06))
					{
						Cl.Mode = 2; Cl.Hold = 8 + Cl.Seed * 6; L.Links[2]++;
						if (W.bScript) UE_LOG(LogTemp, Log, TEXT("KRDen link clean: a grunt stops at the banded coral shrimp's hole at %.1f, %.1f and holds still, head up, to be cleaned (cause: the shrimp is out with its feelers waving, by day, hour %.1f, no diver within 2.5 m)"), D.Mouth.X, D.Mouth.Z, W.Hour);
					}
				}
				else if (Cl.Mode == 2)
				{
					// the pose: stock still, head up, fins spread
					Posing++;
					Cl.P = Stand + Drift(Cl.T, Cl.Seed, .006);
					Fw = (-D.Axis * .55 + Up3() * .8 + D.Right * (c ? .2 : -.2)).GetSafeNormal();
					Cl.Fwd = FMath::Lerp(Cl.Fwd, Fw, FMath::Min(1.0, Dt * 2)).GetSafeNormal();
					Fw = Cl.Fwd;
					Amp = .015; Freq = 1.5;
					Cl.Hold -= Dt;
					if (!bOpen) { Cl.Mode = 3; Fled++; }
					else if (Cl.Hold <= 0) Cl.Mode = 3;
				}
				else if (Cl.Mode == 3)
				{
					if (SwimTo(Cl, Far, bDiverNear ? 1.4 : .5, 2.5, Dt, .5)) { Cl.Mode = 0; Cl.Hold = 6 + Cl.Seed * 10; continue; }
					Fw = Cl.Fwd;
				}
				Put(L, PClient, Cl.P, Face(Fw), FVector(Cl.Size), 1, Amp, Freq, Cl.T * Freq, 0, Cl.Seed);
			}
			if (Fled)
			{
				L.Links[2]++;
				if (W.bScript) UE_LOG(LogTemp, Log, TEXT("KRDen link clean: %d grunts leave the cleaning station at %.1f, %.1f (cause: %s)"), Fled, D.Mouth.X, D.Mouth.Z,
					bDiverNear ? *FString::Printf(TEXT("the diver came within %.1f m"), FVector::Dist(W.Diver, D.Mouth)) : W.bNight ? TEXT("night") : TEXT("the shrimp has backed into its hole"));
			}
			H.bStation = Posing > 0;
		}

		void InitHouse(FLife& L, FHouse& H)
		{
			const FDen& D = H.D;
			H.bInit = true;
			H.bWasNight = L.W.bNight;
			H.NightK = L.W.bNight ? 1 : 0;
			H.T = Hash01(D.Seed, 200) * 100;
			H.NextHunt = 14 + Hash01(D.Seed, 201) * 30;
			const double Rs = Hash01(D.Seed, 202);
			int32 N = 1;
			switch (D.Res)
			{
			case GreenMoray: H.Size = FMath::Lerp(1.1, 1.8, Rs); break;
			case SpottedMoray: H.Size = FMath::Lerp(.5, .9, Rs); break;
			case Octopus: H.Size = FMath::Lerp(.85, 1.2, Rs); break;
			case NurseShark: H.Size = FMath::Lerp(2.1, 2.7, Rs); break;
			case Goliath: H.Size = FMath::Lerp(1.3, 1.9, Rs); break;
			case BlackGrouper: H.Size = FMath::Lerp(.8, 1.15, Rs); break;
			case ClingingCrab: H.Size = FMath::Lerp(.8, 1.15, Rs); break;
			case StoneCrab: H.Size = FMath::Lerp(.85, 1.1, Rs); break;
			case Shrimp: H.Size = FMath::Lerp(.9, 1.15, Rs); break;
			default: H.Size = 1; N = FMath::Max(1, int32(D.Count)); break;
			}
			// a hole no wider than its tenant
			if (D.Res == Goliath) H.Size = FMath::Min(H.Size, FMath::Max(1.0, D.W / .42));
			H.M.SetNum(N);
			for (int32 i = 0; i < N; i++)
			{
				FMember& Mb = H.M[i];
				Mb.Seed = Hash01(D.Seed, 210 + i);
				Mb.Phase = Hash01(D.Seed, 230 + i) * 2 * PI;
				Mb.T = Mb.Seed * 50;
				Mb.P = D.Mouth; Mb.Fwd = D.Axis;
				const double Spread = N > 1 ? (i + .5) / N - .5 : (Mb.Seed - .5) * .4;
				if (D.Res == Lobster)
				{
					Mb.Size = FMath::Lerp(.25, .45, Hash01(D.Seed, 250 + i));
					Mb.U = Spread * FMath::Max(0.0, D.W - .4) + (Mb.Seed - .5) * .08;
					Mb.V = (Hash01(D.Seed, 270 + i) - .5) * .5;
					Mb.Hold = 0;
					Mb.Mode = L.W.bNight ? 1 : 0;
					if (Mb.Mode == 1) { Mb.P = D.Mouth + D.Axis * (1.5 + Mb.Seed * 2) + D.Right * Mb.U; Mb.Step = 0; }
				}
				else if (D.Res == Squirrelfish || D.Res == Soldierfish)
				{
					Mb.Size = FMath::Lerp(.16, .23, Hash01(D.Seed, 250 + i));
					Mb.U = Spread * FMath::Max(.1, D.W - .35);
					Mb.V = (Hash01(D.Seed, 270 + i) - .5) * 1.2;
					Mb.In = .04 + Hash01(D.Seed, 290 + i) * .2;
				}
				else if (D.Res == Sweepers)
				{
					Mb.Size = FMath::Lerp(.09, .13, Hash01(D.Seed, 250 + i));
					Mb.U = .35 + Hash01(D.Seed, 270 + i) * .65;
					Mb.V = (Hash01(D.Seed, 290 + i) - .5) * 2;
					Mb.In = .1 + Hash01(D.Seed, 310 + i) * .5;
				}
				else if (D.Res == ResEyes) { Mb.U = (Mb.Seed - .5) * D.W * .3; Mb.V = Hash01(D.Seed, 270) - .5; }
				else if (D.Res == NurseShark) Mb.U = (Mb.Seed - .5) * FMath::Max(0.0, D.W - 1.2);
				else if (D.Res == Shrimp) Mb.U = (Mb.Seed - .5) * D.W * .4;
				Mb.Mode = D.Res == Lobster ? Mb.Mode : 0;
			}
			if (D.Res == Shrimp)
			{
				H.Clients.SetNum(2);
				for (int32 c = 0; c < 2; c++) { H.Clients[c].Seed = Hash01(D.Seed, 380 + c); H.Clients[c].Size = .2 + H.Clients[c].Seed * .07; H.Clients[c].Hold = 2 + c * 7 + H.Clients[c].Seed * 4; }
			}
			// at night it is found already out
			if (L.W.bNight && (D.Res == GreenMoray || D.Res == SpottedMoray)) H.StateT = 6;
			if (L.W.bNight && D.Res == Octopus) H.StateT = 5;
			if (L.W.bNight && D.Res == NurseShark) { H.M[0].P = D.Mouth + D.Axis * 6; H.M[0].Mode = 2; }
			if (L.W.bNight && (D.Res == Squirrelfish || D.Res == Soldierfish || D.Res == Sweepers)) for (FMember& Mb : H.M) { Mb.Mode = 1; Mb.P = D.Mouth + D.Axis * (1 + Mb.Seed * 3) + D.Right * ((Mb.Seed - .5) * 4) + Up3() * .5; }
		}

		void TickHouse(FLife& L, FHouse& H, bool bMove)
		{
			if (bMove) H.T += L.W.Dt;
			switch (H.D.Res)
			{
			case GreenMoray: case SpottedMoray: TickMoray(L, H, bMove); break;
			case Lobster: TickLobsters(L, H, bMove); break;
			case Octopus: TickOctopus(L, H, bMove); break;
			case Squirrelfish: case Soldierfish: case Sweepers: TickLedgeFish(L, H, bMove); break;
			case NurseShark: case Goliath: case BlackGrouper: TickBig(L, H, bMove); break;
			case ResEyes: case ClingingCrab: case StoneCrab: case Shrimp: TickSmall(L, H, bMove); break;
			default: break;
			}
		}

		void Flush(FLife& L)
		{
			L.Stats.Instances = 0; L.Stats.Tris = 0; L.Stats.Pools = 0;
			for (int32 P = 0; P < NumParts; P++)
			{
				const int32 N = L.Xf[P].Num();
				if (!N && L.Pool[P] < 0) continue;
				UInstancedStaticMeshComponent* C = PoolOf(L, P);
				if (!C) continue;
				if (C->GetInstanceCount() != N)
				{
					C->ClearInstances();
					if (N) C->AddInstances(L.Xf[P], false, true);
				}
				else if (N) C->BatchUpdateInstancesTransforms(0, L.Xf[P], true, false, false);
				for (int32 i = 0; i < N; i++) C->SetCustomData(i, TArrayView<const float>(L.Data[P].GetData() + i * 8, 8), false);
				C->MarkRenderStateDirty();
				L.Stats.Instances += N; L.Stats.Tris += N * L.Tris[P];
				if (N) L.Stats.Pools++;
			}
		}
	}

	FLife* CreateLife(AKeysReef* Reef)
	{
		FLife* L = new FLife;
		L->Reef = Reef;
		for (int32 P = 0; P < NumParts; P++) { L->Pool[P] = -1; L->Tris[P] = 0; L->bTried[P] = false; }
		if (UMaterialInterface* M = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/KeysRun/Materials/M_KR_Boid.M_KR_Boid")))
		{
			L->Mat = UMaterialInstanceDynamic::Create(M, Reef);
			Reef->DenMats.Add(L->Mat);
		}
		else UE_LOG(LogTemp, Warning, TEXT("KRDen: M_KR_Boid missing; the dens will be empty"));
		return L;
	}

	void DestroyLife(FLife* L) { delete L; }

	void TickLife(FLife* LP, const FWorldNow& W, const TArray<const FDen*>& Near)
	{
		if (!LP || !LP->Mat) return;
		FLife& L = *LP;
		const double T0 = FPlatformTime::Seconds();
		L.W = W;
		// households: made for the dens that have come within 60 m, let go with the ones that have left
		TSet<int64> Have;
		for (const FDen* D : Near)
		{
			if (D->Res == ResNone) continue;
			Have.Add(D->Id);
			FHouse& H = L.Houses.FindOrAdd(D->Id);
			if (!H.bInit) { H.D = *D; InitHouse(L, H); L.bDirty = true; }
		}
		for (auto It = L.Houses.CreateIterator(); It; ++It) if (!Have.Contains(It.Key())) { It.RemoveCurrent(); L.bDirty = true; }
		// who moves this frame (within 25 m), who is only drawn (within 40 m; the small ones 8 m)
		int32 Moving = 0, Drawn = 0;
		for (TPair<int64, FHouse>& Kv : L.Houses)
		{
			FHouse& H = Kv.Value;
			H.Dist = FVector::Dist(H.Where.IsZero() ? H.D.Mouth : H.Where, W.Cam);
			const bool bSmall = H.D.Res == ClingingCrab || H.D.Res == StoneCrab || H.D.Res == Shrimp;
			const bool bShow = H.Dist < (bSmall ? 9.0 : 42.0);
			if (bShow != H.bShown) { H.bShown = bShow; L.bDirty = true; }
			if (bShow) { Drawn++; if (H.Dist < 25) Moving++; }
		}
		// nothing near enough to move, and nothing come or gone: last frame's instances stand
		if (!Moving && !L.bDirty && !L.Sheet.Num()) { L.CpuAcc += (FPlatformTime::Seconds() - T0) * 1000; L.CpuN++; return; }
		L.bDirty = Moving > 0;
		for (int32 P = 0; P < NumParts; P++) { L.Xf[P].Reset(); L.Data[P].Reset(); }
		L.Threats.Reset();
		FMemory::Memzero(L.Stats.ByRes);
		for (TPair<int64, FHouse>& Kv : L.Houses)
		{
			FHouse& H = Kv.Value;
			if (!H.bShown) continue;
			TickHouse(L, H, H.Dist < 25);
			L.Stats.ByRes[H.D.Res] += H.M.Num();
		}
		if (L.Sheet.Num())
		{
			GNoShade = true;
			for (FHouse& H : L.Sheet) TickHouse(L, H, true);
			GNoShade = false;
		}
		Flush(L);
		L.Stats.Animals = Drawn; L.Stats.Animated = Moving;

		// ---- found for the first time: a line for the diver ("Green moray - Sombrero Reef, 9 m")
		L.FoundT -= W.Dt;
		if (W.bDiver && L.FoundT <= 0)
		{
			L.FoundT = .3;
			for (TPair<int64, FHouse>& Kv : L.Houses)
			{
				FHouse& H = Kv.Value;
				if (H.D.Res <= ResEyes || !H.bShown || L.Found.Contains(H.D.Res)) continue;
				const FVector To = H.Where - W.Cam;
				const double Dd = To.Size();
				const bool bSmall = H.D.Res == ClingingCrab || H.D.Res == StoneCrab || H.D.Res == Shrimp;
				if (Dd > (bSmall ? 2.2 : 4.5) || Dd < .05 || FVector::DotProduct(To / Dd, W.CamFwd) < .8) continue;
				if (H.State == 0 && H.Out < .3 && H.D.Res != NurseShark && H.D.Res != Lobster) continue;   // drawn back out of sight
				L.Found.Add(H.D.Res);
				const FString Line = FString::Printf(TEXT("%s - %s, %.0f m"), ResName(H.D.Res), *ReefNameAt(H.D.Mouth.X, H.D.Mouth.Z), FMath::Max(1.0, -H.Where.Y));
				// the first of its kind ever (the boat's own counts are kept with the save: den.found.<kind>), else only
				// the first this session, which is not worth a line on the screen
				const FString Key = FString::Printf(TEXT("den.found.%s"), ResKey(H.D.Res));
				AKeysBoatPawn* Pw = Cast<AKeysBoatPawn>(UGameplayStatics::GetPlayerPawn(L.Reef, 0));
				const bool bKnown = Pw && Pw->CountOf(*Key) > 0;
				if (Pw && !bKnown) { Pw->ShowMsg(Line, 4.5); Pw->AddCount(*Key, 1); }
				UE_LOG(LogTemp, Log, TEXT("KRDen found: %s (%s; %d of %d kinds found this session)"), *Line, bKnown ? TEXT("seen before: no line") : TEXT("the first of its kind: a line on the screen"), L.Found.Num(), NumRes - 2);
				break;
			}
		}
		// ---- the schools and the holes: what the keep-off does, counted when the schools lend their counter (R2)
		if (W.bScript && SchoolFishCounter)
			for (TPair<int64, FHouse>& Kv : L.Houses)
			{
				FHouse& H = Kv.Value;
				if ((H.D.Res != GreenMoray && H.D.Res != SpottedMoray && H.D.Res != Octopus) || H.Dist > 25) continue;
				if ((H.ShyT -= W.Dt) > 0) continue;
				H.ShyT = 6;
				const bool bOut = H.State != 0 || H.Out > .55;
				const double R = H.D.Res == GreenMoray ? 1.6 : 1.0;
				L.Links[3]++;
				UE_LOG(LogTemp, Log, TEXT("KRDen link shy: the %s at %.1f, %.1f is %s (cause), so the schools are %s a %.1f m keep-off round it (effect): %d school fish within %.1f m of it, %d within 5 m"),
					ResName(H.D.Res), H.Where.X, H.Where.Z, bOut ? TEXT("out") : TEXT("drawn back in its hole"), bOut ? TEXT("given") : TEXT("not given"), R, SchoolFishCounter(H.Where, R), R, SchoolFishCounter(H.Where, 5));
			}
		L.CpuAcc += (FPlatformTime::Seconds() - T0) * 1000; L.CpuN++;
	}

	void LifeThreats(const FLife* L, TArray<FVector4>& Out) { if (L) Out.Append(L->Threats); }

	FVector LifeWhere(const FLife* L, const FDen& D)
	{
		if (L) if (const FHouse* H = L->Houses.Find(D.Id)) if (!H->Where.IsZero()) return H->Where;
		if (L) for (const FHouse& H : L->Sheet) if (H.D.Id == D.Id && !H.Where.IsZero()) return H.Where;
		return D.Mouth;
	}

	void LifeSheetDens(const FLife* L, TArray<FDen>& Out) { if (L) for (const FHouse& H : L->Sheet) Out.Add(H.D); }

	FLifeStats LifeStatsOf(const FLife* L)
	{
		FLifeStats S = L->Stats;
		S.CpuMs = L->CpuN ? L->CpuAcc / L->CpuN : 0;
		const_cast<FLife*>(L)->CpuAcc = 0; const_cast<FLife*>(L)->CpuN = 0;
		return S;
	}

	bool LifeDebug(FLife* L, const FWorldNow& W, const TArray<FString>& A)
	{
		if (A[0] == TEXT("hunt"))
		{
			// the grouper comes for the nearest green moray that hunts with one, now
			L->bForceHunt = true;
			for (TPair<int64, FHouse>& Kv : L->Houses) if (Kv.Value.D.bBuddy) Kv.Value.NextHunt = 0;
			UE_LOG(LogTemp, Log, TEXT("KRDen hunt: asked for now"));
			return true;
		}
		if (A[0] == TEXT("sheet"))
		{
			// one of every resident, at home, set out in a row in open water 3 m ahead of the camera and in full light:
			// to look the animals over without hunting the reef for them (goto finds them as it does real dens).
			// sheet 0 takes it away.
			L->Sheet.Reset();
			L->bDirty = true;
			if (A.Num() > 1 && A[1] == TEXT("0")) return true;
			const FVector Fw = Flat(W.CamFwd).GetSafeNormal(.0001, FVector(0, 0, 1)), Rt = FVector::CrossProduct(Up3(), Fw).GetSafeNormal();
			for (int32 r = ResEyes; r < NumRes; r++)
			{
				FHouse H;
				FDen& D = H.D;
				D.Id = -(r + 1); D.Kind = uint8(Ledge); D.Res = uint8(r); D.Seed = 1000 + r * 77;
				D.Count = r == Lobster ? 3 : r == Squirrelfish ? 4 : r == Soldierfish ? 5 : r == Sweepers ? 24 : 1;
				D.Mouth = W.Cam + Fw * 3 + Rt * ((r - ResEyes - 6.5) * 1.6) + Up3() * -.2;
				D.Axis = -Fw; D.Right = Rt;
				D.W = r == Sweepers || r == Goliath ? 1.6 : 1.3; D.H = r == Goliath || r == BlackGrouper ? .7 : .4; D.Deep = 1; D.Depth = -D.Mouth.Y;
				D.X = D.Mouth.X; D.Y = D.Mouth.Y - D.H * .5; D.Z = D.Mouth.Z;
				InitHouse(*L, H);
				L->Sheet.Add(H);
			}
			UE_LOG(LogTemp, Log, TEXT("KRDen sheet: %d residents set out at %.1f, %.1f, %.1f"), L->Sheet.Num(), W.Cam.X + Fw.X * 3, W.Cam.Y, W.Cam.Z + Fw.Z * 3);
			return true;
		}
		if (A[0] == TEXT("who"))
		{
			// every household near the camera and what it is doing
			for (const TPair<int64, FHouse>& Kv : L->Houses)
			{
				const FHouse& H = Kv.Value;
				UE_LOG(LogTemp, Log, TEXT("KRDen who: %s x%d in the %s at %.1f, %.1f, %.1f (%.1f m off): state %d, out %.2f, night %.2f, %s%s"), ResName(H.D.Res), H.M.Num(), KindName(H.D.Kind), H.D.Mouth.X, H.D.Mouth.Y, H.D.Mouth.Z,
					H.Dist, H.State, H.Out, H.NightK, H.bScared ? TEXT("drawn back") : TEXT("at ease"), H.D.bBuddy ? *FString::Printf(TEXT(", hunts with a grouper (stage %d, next in %.0f s)"), H.Hunt, H.NextHunt) : TEXT(""));
			}
			UE_LOG(LogTemp, Log, TEXT("KRDen links so far: %d hunts with a grouper, %d lobster flicks, %d cleaning station comings and goings, %d keep-off lines; kinds found %d"), L->Links[0], L->Links[1], L->Links[2], L->Links[3], L->Found.Num());
			return true;
		}
		return false;
	}
}
