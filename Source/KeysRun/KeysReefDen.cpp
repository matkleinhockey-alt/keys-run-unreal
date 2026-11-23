// The reef's hiding places (see KeysReefDen.h): the rock each den is made of, where the dens go, and the runtime that
// draws them round the camera and tells the residents (KeysReefDenLife.cpp) what is going on.
//
// The rock. Every den is one closed sheet of triangles: rings that start at the back of the pocket, come forward along
// its walls to the rim, then turn and run out over the face of the rock, over its top and down its back to a point
// under the sand. The pocket's walls are painted from the rock's colour at the lip to black a hand's width in, and
// they all face the opening, so whatever the angle the eye meets either rock or dark. The rock is star-shaped round
// its middle (one radius for every direction), which keeps the pocket inside it. It stands a little into the bottom,
// at the highest the seafloor mesh is drawn under it, so sand never shows inside.
//
// Five kinds: an undercut ledge (a long low slot at the foot of a block), a crevice (a tall narrow slot in the notch
// between two coral heads), a round hole in a mound of reef rock, a low cave mouth under a brow (the deep reef), and
// an overhang of plating star coral with shadow beneath. Three sizes of each. A sixth, a torn gap in steel plating, is
// dealt only by the wrecks (request R3).
//
// Where. Each 50 m reef chunk rolls 14 places. A place is kept by the habitat there (the flank of a spur beside its
// sand groove 6 to a chunk, the fore reef 5, the deep reef 4, the crest and back reef 2.5, a patch reef 1.2; nothing
// on sand, seagrass, flats, in canals or mangroves), then it must find room: no solid coral inside the rock or in
// front of the opening, ground that is level enough, and no other den within a metre. On a spur's flank the opening
// faces the sand.
#include "KeysReefDen.h"
#include "KeysReefDenInt.h"
#include "KeysGeo.h"
#include "KeysGeoReal.h"
#include "KeysSea.h"
#include "KeysWorld.h"
#include "KeysDiver.h"
#include "KeysFeed.h"
#include "KeysBoatPawn.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/SpotLightComponent.h"
#include "Engine/StaticMesh.h"
#include "StaticMeshAttributes.h"
#include "MeshDescription.h"
#include "Materials/MaterialInterface.h"
#include "Kismet/GameplayStatics.h"
#include "Camera/PlayerCameraManager.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "HAL/IConsoleManager.h"

namespace KeysReefDen
{
	bool Enabled() { static const bool b = !FParse::Param(FCommandLine::Get(), TEXT("KRNoDens")); return b; }

	const TCHAR* KindName(int32 Kind)
	{
		static const TCHAR* N[] = {TEXT("ledge"), TEXT("crevice"), TEXT("hole"), TEXT("cave"), TEXT("plate"), TEXT("gap")};
		return Kind >= 0 && Kind < NumRockKinds ? N[Kind] : TEXT("?");
	}
	const TCHAR* HabName(int32 Hab)
	{
		static const TCHAR* N[] = {TEXT("none"), TEXT("patch reef"), TEXT("crest and back reef"), TEXT("fore reef"), TEXT("spur and groove"), TEXT("deep reef")};
		return Hab >= 0 && Hab < NumHab ? N[Hab] : TEXT("?");
	}
	const TCHAR* ResName(int32 Res)
	{
		static const TCHAR* N[] = {TEXT("Empty"), TEXT("Eyes in the dark"), TEXT("Green moray"), TEXT("Spotted moray"), TEXT("Caribbean spiny lobster"), TEXT("Caribbean reef octopus"),
			TEXT("Squirrelfish"), TEXT("Blackbar soldierfish"), TEXT("Glassy sweepers"), TEXT("Nurse shark"), TEXT("Goliath grouper"), TEXT("Black grouper"),
			TEXT("Channel clinging crab"), TEXT("Stone crab"), TEXT("Banded coral shrimp")};
		return Res >= 0 && Res < NumRes ? N[Res] : TEXT("?");
	}
	const TCHAR* ResKey(int32 Res)
	{
		static const TCHAR* N[] = {TEXT("empty"), TEXT("eyes"), TEXT("greenmoray"), TEXT("spottedmoray"), TEXT("lobster"), TEXT("octopus"), TEXT("squirrelfish"), TEXT("soldierfish"),
			TEXT("sweepers"), TEXT("nurse"), TEXT("goliath"), TEXT("blackgrouper"), TEXT("clingingcrab"), TEXT("stonecrab"), TEXT("shrimp")};
		return Res >= 0 && Res < NumRes ? N[Res] : TEXT("?");
	}

	FLinearColor Hex(uint32 C) { return FLinearColor::FromSRGBColor(FColor(uint8(C >> 16), uint8(C >> 8), uint8(C))); }

	double Hash01(uint32 Seed, int32 Salt)
	{
		uint32 Hh = Seed * 0x9e3779b9u + uint32(Salt) * 0x85ebca6bu + 0x27d4eb2fu;
		Hh ^= Hh >> 16; Hh *= 0x7feb352du; Hh ^= Hh >> 15; Hh *= 0x846ca68bu; Hh ^= Hh >> 16;
		return double(Hh) / 4294967296.0;
	}

	namespace
	{
		double Lattice(int32 I, int32 J, int32 K, double Seed)
		{
			const double Hh = FMath::Sin((I + J * 57 + K * 113) * 12.9898 + Seed * 78.233) * 43758.5453;
			return Hh - FMath::FloorToDouble(Hh);
		}
	}

	// smooth value noise, 0..1
	double Noise3(const FVector& P, double Seed)
	{
		const int32 X0 = FMath::FloorToInt32(P.X), Y0 = FMath::FloorToInt32(P.Y), Z0 = FMath::FloorToInt32(P.Z);
		const FVector F(P.X - X0, P.Y - Y0, P.Z - Z0);
		const FVector U = F * F * (FVector(3) - 2 * F);
		auto L = [&](int32 i, int32 j, int32 k) { return Lattice(X0 + i, Y0 + j, Z0 + k, Seed); };
		const double A = FMath::Lerp(FMath::Lerp(L(0, 0, 0), L(1, 0, 0), U.X), FMath::Lerp(L(0, 1, 0), L(1, 1, 0), U.X), U.Y);
		const double B = FMath::Lerp(FMath::Lerp(L(0, 0, 1), L(1, 0, 1), U.X), FMath::Lerp(L(0, 1, 1), L(1, 1, 1), U.X), U.Y);
		return FMath::Lerp(A, B, U.Z);
	}

	// ================================================================ the rock
	namespace
	{
		// Style: 0 a plain block, 1 two coral heads with a notch between them, 2 a block with a brow over its opening,
		// 3 a low block roofed with plates of coral
		struct FRockDef
		{
			double Rx, Ry, Rz, Cy, Pw;            // the block: half-sizes, how high its middle stands, how square (2 round .. 4 boxy)
			double MW, MH, Sill, MExp, Flat, MX;  // the opening: width, height, its lower lip over the ground, how square, how flat its floor, how far off centre
			double Pitch, Deep;                   // how far the hole looks up (radians), how far back the dark goes
			double Amp, Freq;
			uint32 Seed;
			int32 Style;
		};
		const FRockDef Defs[NumKinds][NumVariants] = {
			// ledge: a long low slot at the foot of a block
			{{2.3, .62, 1.25, .27, 2.8,   3.0, .34, .06, 4.0, 1, 0,     0, .95,   .10, 1.1, 11, 0},
			 {1.7, .55, 1.10, .24, 2.6,   2.1, .30, .06, 3.5, 1, .1,    0, .80,   .11, 1.3, 23, 0},
			 {3.0, .78, 1.40, .32, 3.0,   3.9, .40, .07, 4.5, 1, -.2,   0, 1.05,  .09, .9, 37, 0}},
			// crevice: a tall narrow slot between two heads
			{{1.45, .95, 1.0, .38, 2.2,   .24, .78, .09, 2.6, 1, 0,     .05, .7,  .08, 1.5, 41, 1},
			 {1.2, .8, .9, .32, 2.2,      .20, .62, .08, 2.4, 1, 0,     .05, .6,  .09, 1.7, 53, 1},
			 {1.8, 1.15, 1.15, .45, 2.3,  .30, .95, .10, 2.8, 1, 0,     .05, .8,  .08, 1.3, 67, 1}},
			// hole: a round hole in a mound of reef rock
			{{.85, .62, .8, .16, 2.3,     .30, .26, .30, 2.0, 1, 0,     .22, .5,  .12, 1.9, 71, 0},
			 {.65, .5, .6, .13, 2.2,      .22, .20, .24, 2.0, 1, .08,   .25, .4,  .13, 2.3, 83, 0},
			 {1.1, .75, 1.0, .2, 2.4,     .38, .32, .34, 2.1, 1, -.15,  .18, .6,  .11, 1.6, 97, 0}},
			// cave: a low mouth under a brow
			{{2.4, 1.05, 1.7, .42, 2.8,   1.7, .62, .07, 2.6, .45, 0,   0, 1.35,  .09, .9, 101, 2},
			 {1.9, .9, 1.45, .36, 2.7,    1.3, .5, .07, 2.5, .45, .15,  0, 1.1,   .10, 1.1, 113, 2},
			 {2.9, 1.2, 1.9, .48, 2.9,    2.2, .7, .08, 2.7, .4, -.2,   0, 1.5,   .08, .8, 127, 2}},
			// plate: shadow under plates of coral
			{{1.35, .5, 1.0, .2, 2.4,     1.5, .26, .07, 3.6, 1, 0,     0, .7,    .09, 1.5, 131, 3},
			 {1.05, .45, .85, .18, 2.3,   1.1, .22, .06, 3.4, 1, 0,     0, .6,    .10, 1.8, 149, 3},
			 {1.7, .58, 1.15, .22, 2.5,   1.9, .30, .07, 3.8, 1, 0,     0, .8,    .08, 1.3, 163, 3}},
		};
		// the wreck gaps: a torn opening in a patch of plating (width, height, how far back)
		const double GapDefs[NumVariants][3] = {{.7, .42, .8}, {1.1, .6, 1.0}, {.45, .3, .6}};
		constexpr double Skirt = .8;   // how far the rock goes on down under its own ground line

		// the block's radius in a direction from its middle (unit Dir, the den's own frame)
		double RockR(const FRockDef& D, const FVector& Dir, bool bRough = true)
		{
			const double S = FMath::Pow(FMath::Abs(Dir.X) / D.Rx, D.Pw) + FMath::Pow(FMath::Abs(Dir.Y) / D.Ry, D.Pw) + FMath::Pow(FMath::Abs(Dir.Z) / D.Rz, D.Pw);
			double R = FMath::Pow(FMath::Max(S, 1e-9), -1 / D.Pw);
			if (D.Style == 1)
			{
				// two heads: a notch down the front and over the top, and each head a little rounder than the block
				const double G = FMath::Exp(-FMath::Square(Dir.X / .14)) * Smooth(Dir.Z, -.45, .1);
				R *= 1 - .36 * G;
				R *= 1 + .07 * FMath::Cos(Dir.X * PI) * FMath::Max(0.0, Dir.Y);
			}
			else if (D.Style == 2)
			{
				// a brow: the rock stands out over the mouth
				R *= 1 + .1 * FMath::Exp(-FMath::Square((Dir.Y - .42) / .22)) * FMath::Max(0.0, Dir.Z);
			}
			if (bRough)
			{
				const FVector Q = Dir * (D.Freq * 2.2);
				const double N = Noise3(Q + FVector(3.1), D.Seed) * .55 + Noise3(Q * 2.6 + FVector(9.7), D.Seed + 5) * .3 + Noise3(Q * 6.5 + FVector(4.4), D.Seed + 9) * .15;
				R *= 1 + (N - .5) * 2 * D.Amp * 1.6;
			}
			return R;
		}

		// a sheet of rings (P[v * NU + u], closed round u) with a colour at every point, added to M in Unreal's local
		// frame with normals from the sheet itself. SignRow: a ring whose normals must point away from Inside (the rest
		// follow it: the sheet has one side all the way along, so the pocket's walls come out facing the opening).
		void AddSheet(FKeysMesh& M, const TArray<FVector>& P, const TArray<FLinearColor>& Col, int32 NU, int32 NV, const FVector& Inside, int32 SignRow)
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
					if (Du.SizeSquared() < 1e-10)
					{
						const int32 vn = v == 0 ? 1 : v - 1;
						Du = P[vn * NU + u1] - P[vn * NU + u0];
					}
					FVector Nn = FVector::CrossProduct(Du, Dv).GetSafeNormal();
					Nr[v * NU + u] = Nn;
					if (v == SignRow) Flip += FVector::DotProduct(Nn, P[v * NU + u] - Inside);
				}
			const double Sg = Flip < 0 ? -1 : 1;
			const int32 Base = M.V.Num();
			for (int32 i = 0; i < NU * NV; i++)
			{
				FVector Nn = Nr[i] * Sg;
				if (Nn.IsNearlyZero()) Nn = (P[i] - Inside).GetSafeNormal();
				M.AddVert(FVector(P[i].X, P[i].Z, P[i].Y), FVector(Nn.X, Nn.Z, Nn.Y), Col[i]);
			}
			for (int32 v = 0; v < NV - 1; v++)
				for (int32 u = 0; u < NU; u++)
				{
					const int32 a = Base + v * NU + u, b = Base + v * NU + (u + 1) % NU, c = Base + (v + 1) * NU + (u + 1) % NU, d = Base + (v + 1) * NU + u;
					M.T.Append({a, b, c, a, c, d});
				}
		}

		// a closed lump (an ellipsoid with a rough outline) with a colour per point: Ax, Ay, Az are its half-axes as
		// vectors, Paint(unit point on the lump, 0..1 from its bottom pole to its top pole along Ay) gives the colour
		void AddLump(FKeysMesh& M, const FVector& Cn, const FVector& Ax, const FVector& Ay, const FVector& Az, int32 NU, int32 NV, double Rough, double Seed, TFunctionRef<FLinearColor(const FVector&, double)> Paint)
		{
			TArray<FVector> P;
			TArray<FLinearColor> Col;
			for (int32 v = 0; v < NV; v++)
				for (int32 u = 0; u < NU; u++)
				{
					const double Th = PI * v / (NV - 1), Ph = 2 * PI * u / NU;
					const FVector S(FMath::Sin(Th) * FMath::Cos(Ph), -FMath::Cos(Th), FMath::Sin(Th) * FMath::Sin(Ph));
					const double K = 1 + (Noise3(S * 2.3 + FVector(Seed), Seed) - .5) * 2 * Rough;
					P.Add(Cn + (Ax * S.X + Az * S.Z) * K + Ay * S.Y);
					Col.Add(Paint(S, double(v) / (NV - 1)));
				}
			AddSheet(M, P, Col, NU, NV, Cn, NV / 2);
		}

		const FLinearColor RockTan = Hex(0x7d7460), RockOlive = Hex(0x5c5636), RockMauve = Hex(0x7a5866), RockDark = Hex(0x3e382a), SandCol = Hex(0xb9ad8a);
		const FLinearColor CoralBrown = Hex(0x8c5a24), CoralMustard = Hex(0x9c7c3c), CoralGreen = Hex(0x5e6a30), SpongeOrange = Hex(0xa8502a), SpongeLav = Hex(0x74658a);
		const FLinearColor Rust = Hex(0x6a3a22), RustDark = Hex(0x3a2418), Steel = Hex(0x4a4e4c);

		FLinearColor Mul(const FLinearColor& C, double K) { return FLinearColor(float(C.R * K), float(C.G * K), float(C.B * K), 1); }

		// reef rock: old coral limestone under turf algae and crusts of coralline, sanded in at its foot
		FLinearColor RockColor(const FRockDef& D, const FVector& P)
		{
			const double N1 = Noise3(P * 1.3 + FVector(1.7), D.Seed + 11), N2 = Noise3(P * 4.1 + FVector(5.2), D.Seed + 17), N3 = Noise3(P * 13.0, D.Seed + 23);
			FLinearColor C = FMath::Lerp(RockTan, RockOlive, float(Smooth(N1, .32, .68)));
			C = FMath::Lerp(C, RockMauve, float(Smooth(N2, .6, .8) * .65));
			C = FMath::Lerp(C, RockDark, float(Smooth(N2, .3, .1) * .6));
			if (D.Style == 1)
			{
				// the two heads are living boulder coral above their dead feet
				const double Live = Smooth(P.Y, .22, .5);
				C = FMath::Lerp(C, FMath::Lerp(CoralBrown, CoralMustard, float(Smooth(P.X, -.2, .2))), float(Live * .85));
			}
			C = Mul(C, .8 + .4 * N3);
			return FMath::Lerp(C, Mul(SandCol, .78), float(Smooth(P.Y, .2, -.02) * .8));
		}

		struct FBuilt { FShape Shape; int32 Outside = 0; };

		FBuilt BuildRockGeo(const FRockDef& D, int32 Lod, FKeysMesh* M)
		{
			const bool bWide = D.MW > 1.2;
			const int32 NU = Lod == 0 ? (bWide ? 64 : 44) : (bWide ? 28 : 20);
			const int32 NE = Lod == 0 ? 16 : 7;
			const FVector C(0, D.Cy, 0);
			const FVector A(0, FMath::Sin(D.Pitch), FMath::Cos(D.Pitch));           // out of the opening
			const FVector Rt(1, 0, 0), Up(0, FMath::Cos(D.Pitch), -FMath::Sin(D.Pitch));
			// the opening as planned on a flat plane, then carried onto the face of the rock
			const FVector Dir0 = FVector(D.MX, D.Sill + D.MH * .5 * D.Flat - D.Cy, D.Rz).GetSafeNormal();
			const FVector Mc0 = C + Dir0 * RockR(D, Dir0, false);
			TArray<FVector> Rim;
			Rim.SetNum(NU);
			FVector Mc = FVector::ZeroVector;
			for (int32 u = 0; u < NU; u++)
			{
				const double Th = 2 * PI * u / NU, Cs = FMath::Cos(Th), Sn = FMath::Sin(Th);
				double Ex = FMath::Sign(Cs) * FMath::Pow(FMath::Abs(Cs), 2 / D.MExp), Ey = FMath::Sign(Sn) * FMath::Pow(FMath::Abs(Sn), 2 / D.MExp);
				if (Ey < 0) Ey *= D.Flat;
				// the upper lip is broken rock, higher here and lower there; the lower is the sand's own line
				const double Wob = 1 + .06 * FMath::Sin(3 * Th + D.Seed) + (Ey > 0 ? .7 * (Noise3(FVector(Ex * D.MW * 1.6 + D.Seed, .5, .5), D.Seed + 31) - .3) + .3 * (Noise3(FVector(Ex * D.MW * 5 + D.Seed, 1.5, .5), D.Seed + 37) - .5) : 0);
				const FVector Q = Mc0 + Rt * (D.MW * .5 * Ex * (1 + .05 * FMath::Sin(2 * Th + D.Seed * .9))) + Up * (D.MH * .5 * Ey * Wob);
				const FVector Dir = (Q - C).GetSafeNormal();
				FVector P = C + Dir * RockR(D, Dir) * .985;
				P.Y = FMath::Max(P.Y, D.Sill);
				Rim[u] = P;
				Mc += P / NU;
			}
			FBuilt Out;
			Out.Shape.Mouth = Mc; Out.Shape.Axis = A; Out.Shape.W = D.MW; Out.Shape.H = D.MH; Out.Shape.Deep = D.Deep;
			Out.Shape.C = C; Out.Shape.Rad = FVector(D.Rx, D.Ry, D.Rz); Out.Shape.Pw = D.Pw;
			Out.Shape.Top = D.Cy + D.Ry;

			// the pocket, back to front: (how far in, how much of the opening's size is left, how far the lip's own
			// curve has been flattened out, how much light is left)
			struct FIn { double D, Sh, T, K; };
			TArray<FIn> In;
			if (Lod == 0) In = {{D.Deep, 0, 1, .003}, {D.Deep, .3, 1, .004}, {D.Deep * .7, .66, .9, .006}, {D.Deep * .35, .86, .6, .012}, {.12, .93, .3, .03}, {.035, .97, .1, .10}};
			else In = {{D.Deep, 0, 1, .003}, {D.Deep * .7, .66, .9, .006}, {.12, .93, .3, .03}};
			const int32 NI = In.Num(), NV = NI + 1 + NE;
			TArray<FVector> P;
			TArray<FLinearColor> Col;
			P.SetNum(NU * NV);
			Col.SetNum(NU * NV);
			for (int32 v = 0; v < NI; v++)
				for (int32 u = 0; u < NU; u++)
				{
					const FVector Off = Rim[u] - Mc;
					const double Ax = FVector::DotProduct(Off, A);
					FVector Q = Mc - A * In[v].D + (Off - A * Ax) * In[v].Sh + A * (Ax * (1 - In[v].T));
					Q.Y = FMath::Max(Q.Y, D.Sill);
					P[v * NU + u] = Q;
					Col[v * NU + u] = Mul(RockColor(D, Rim[u]), In[v].K);
					// (is it inside the rock? the mesh is only right if it is)
					const FVector Rel = Q - C;
					if (Rel.Size() > RockR(D, Rel.GetSafeNormal()) * 1.0 && In[v].D > .05) Out.Outside++;
				}
			// the rim, then out over the rock: every ring a little further round from the opening, to a point at the back
			const FVector Am = (Mc - C).GetSafeNormal();
			FVector RtA = (Rt - Am * FVector::DotProduct(Rt, Am)).GetSafeNormal();
			FVector UpA = FVector::CrossProduct(Am, RtA);
			if (FVector::DotProduct(UpA, Up) < 0) UpA = -UpA;
			for (int32 e = 0; e <= NE; e++)
			{
				const double S = double(e) / NE;
				const int32 v = NI + e;
				for (int32 u = 0; u < NU; u++)
				{
					if (e == 0) { P[v * NU + u] = Rim[u]; Col[v * NU + u] = Mul(RockColor(D, Rim[u]), .5); continue; }
					const FVector D0 = (Rim[u] - C).GetSafeNormal();
					const double Cos0 = FMath::Clamp(FVector::DotProduct(D0, Am), -1.0, 1.0), Phi0 = FMath::Acos(Cos0);
					const FVector E0 = (D0 - Am * Cos0).GetSafeNormal();
					const double Th = 2 * PI * u / NU;
					const FVector EU = RtA * FMath::Cos(Th) + UpA * FMath::Sin(Th);
					const FVector E = FMath::Lerp(E0, EU, Smooth(S, 0, .45)).GetSafeNormal();
					const double Phi = Phi0 + (PI * .999 - Phi0) * FMath::Pow(S, .85);
					const FVector Dir = Am * FMath::Cos(Phi) + E * FMath::Sin(Phi);
					const double R0 = (Rim[u] - C).Size(), R1 = RockR(D, Dir);
					FVector Q = C + Dir * FMath::Lerp(R0, R1, Smooth(S, 0, .2));
					if (e == 1 && Q.Y < D.Sill && Rim[u].Y <= D.Sill + .001) Q.Y = FMath::Max(Q.Y, D.Sill - .05);   // the sill: a short step of sand in front of the floor
					if (Q.Y < 0)
					{
						// under its own ground line the rock goes straight on down, a little wider, instead of rounding in
						// under itself: where the bottom falls away beside it there is rock to see, not a gap
						const double Tt = FMath::Clamp(-Q.Y / FMath::Max(.05, D.Ry - D.Cy), 0.0, 1.0);
						const FVector2D Hh(Q.X, Q.Z);
						const double L = Hh.Size();
						if (L > 1e-4)
						{
							auto Section = [&](double Y) { return FMath::Pow(FMath::Max(0.0, 1 - FMath::Pow(FMath::Min(1.0, FMath::Abs(Y - D.Cy) / D.Ry), D.Pw)), 1 / D.Pw); };
							const double Here = Section(Q.Y), Ground = Section(0);
							const double Want = Here > .08 ? L * Ground / Here * (1 + .12 * Tt) : L;
							const double NewL = FMath::Lerp(FMath::Max(L, Want), L, Smooth(Tt, .9, 1));
							Q.X *= NewL / L; Q.Z *= NewL / L;
						}
						Q.Y = -Skirt * Tt;
					}
					P[v * NU + u] = Q;
					Col[v * NU + u] = Mul(RockColor(D, Q), FMath::Lerp(.55, 1.0, Smooth(S, 0, .07)));
					Out.Shape.Top = FMath::Max(Out.Shape.Top, Q.Y);
				}
			}
			if (!M) return Out;
			AddSheet(*M, P, Col, NU, NV, C, NV - 3);

			// what grows on it: crusts and small heads on the top and back, away from the opening
			KeysSea::FMulberry Rng(D.Seed * 7919u + 13u);
			const int32 NLump = Lod == 0 ? (D.Style == 3 ? 3 : 6) : 2;
			const FLinearColor LumpCols[] = {CoralBrown, CoralMustard, CoralGreen, SpongeOrange, SpongeLav, CoralMustard};
			for (int32 i = 0, Tries = 0; i < NLump && Tries < 60; Tries++)
			{
				const double Az = Rng() * 2 * PI, El = .25 + Rng() * .7;
				const FVector Dir = FVector(FMath::Cos(Az) * FMath::Cos(El), FMath::Sin(El), FMath::Sin(Az) * FMath::Cos(El));
				const double R = RockR(D, Dir);
				const FVector Q = C + Dir * R * .96;
				// not over or beside the opening
				const FVector Rel = Q - Mc;
				if (FVector::DotProduct(Dir, Am) > .55 || (FMath::Abs(FVector::DotProduct(Rel, Rt)) < D.MW * .5 + .2 && FVector::DotProduct(Rel, A) > -.35 && Rel.Y < D.MH + .25)) continue;
				const double Sz = (.1 + Rng() * .16) * FMath::Clamp(D.Rx, .7, 1.6), Sq = .35 + Rng() * .4;
				const FLinearColor Lc = LumpCols[i % UE_ARRAY_COUNT(LumpCols)];
				const FVector T1 = FVector::CrossProduct(Dir, FVector(.3, .2, .93)).GetSafeNormal(), T2 = FVector::CrossProduct(Dir, T1);
				const double Sd = D.Seed + i * 3.7;
				AddLump(*M, Q, T1 * Sz, Dir * Sz * Sq, T2 * Sz * (.8 + Rng() * .4), Lod == 0 ? 10 : 6, Lod == 0 ? 6 : 4, .18, Sd,
					[&](const FVector& S, double) { return Mul(Lc, .75 + .5 * Noise3(S * 3.1, Sd)); });
				i++;
			}
			// the plates: sheets of star coral that grow out flat over the edge, one shingled on the next
			if (D.Style == 3)
			{
				const FLinearColor PlTop = Hex(0x7a6230), PlRim = Hex(0xb9a568), PlUnder = Hex(0x2e2618);
				const int32 NPl = Lod == 0 ? 7 : 4;
				const double TopY = D.Sill + D.MH;
				for (int32 i = 0; i < NPl; i++)
				{
					// a front row across the top of the opening, a second row behind and above it, and one or two on top
					const int32 Row = i < 3 ? 0 : i < 5 ? 1 : 2;
					const double Fx = Row == 0 ? (i - 1) * .62 : Row == 1 ? (i - 3.5) * .8 : (i - 5.5) * .5;
					const double Rp = (Row == 0 ? .34 : Row == 1 ? .38 : .3) * D.MW * (.85 + Rng() * .3) * .62;
					const FVector Pc(D.MX + Fx * D.MW * .5 + (Rng() - .5) * .1, TopY + .05 + Row * .11 + Rng() * .03, Mc.Z + (Row == 0 ? .16 : Row == 1 ? -.08 : -.4) + Rng() * .08);
					const double Tilt = .16 + Rng() * .14, Yw = (Rng() - .5) * .7 + Fx * .5;
					// its own up leans back, so its front edge dips a little toward the sand
					const FVector PUp = FVector(FMath::Sin(Yw) * -FMath::Sin(Tilt), FMath::Cos(Tilt), -FMath::Sin(Tilt) * FMath::Cos(Yw)).GetSafeNormal();
					const FVector PX = FVector::CrossProduct(PUp, FVector(0, 0, 1)).GetSafeNormal(), PZ = FVector::CrossProduct(PX, PUp);
					const double Sd = D.Seed + 40 + i * 2.3;
					AddLump(*M, Pc, PX * Rp, PUp * .035, PZ * Rp * .85, Lod == 0 ? 16 : 8, Lod == 0 ? 7 : 5, .2, Sd,
						[&](const FVector& S, double V)
						{
							if (V < .45) return PlUnder;
							const double Edge = Smooth(FMath::Sqrt(S.X * S.X + S.Z * S.Z), .72, .98);
							return Mul(FMath::Lerp(PlTop, PlRim, float(Edge)), .8 + .4 * Noise3(S * 5.0, Sd));
						});
				}
			}
			return Out;
		}

		// a wreck's torn plating: a patch of rusted plate standing a few centimetres proud of the hull, with the tear in
		// the middle of it and the dark behind. Same sheet: back of the pocket, the tear's edge, out across the patch,
		// and its own edge tucked back into the hull.
		FBuilt BuildGapGeo(int32 Var, int32 Lod, FKeysMesh* M)
		{
			const double W = GapDefs[Var][0], H = GapDefs[Var][1], Deep = GapDefs[Var][2];
			const int32 NU = Lod == 0 ? 36 : 16;
			const double Seed = 300 + Var * 17;
			FBuilt Out;
			Out.Shape.Mouth = FVector(0, 0, .04); Out.Shape.Axis = FVector(0, 0, 1); Out.Shape.W = W; Out.Shape.H = H; Out.Shape.Deep = Deep;
			Out.Shape.C = FVector(0, 0, -.5); Out.Shape.Rad = FVector(W * .5 + .3, H * .5 + .3, .5); Out.Shape.Top = H * .5 + .3;
			if (!M) return Out;
			// rows: pocket back (a point), two pocket rings, just inside the tear, the tear's edge, the patch, its edge, tucked back
			struct FRow { double Z, K, Grow, Lt; };   // how far out of the plate, the tear's size, metres added all round, light
			const FRow Rows[] = {{-Deep, 0, 0, .003}, {-Deep, .5, 0, .004}, {-Deep * .4, .92, 0, .01}, {-.03, .98, 0, .08}, {.04, 1, 0, .5}, {.045, 1, .09, .9}, {.035, 1, .26, 1}, {-.08, 1, .3, .8}};
			const int32 NV = UE_ARRAY_COUNT(Rows);
			TArray<FVector> P;
			TArray<FLinearColor> Col;
			for (int32 v = 0; v < NV; v++)
				for (int32 u = 0; u < NU; u++)
				{
					const double Th = 2 * PI * u / NU, Cs = FMath::Cos(Th), Sn = FMath::Sin(Th);
					// a jagged tear: steel does not open in a neat oval
					const double Jag = 1 + .16 * FMath::Sin(3 * Th + Seed) + .12 * FMath::Sin(7 * Th + Seed * 2.1) + .07 * FMath::Sin(13 * Th + Seed * .7);
					const double Ex = FMath::Sign(Cs) * FMath::Pow(FMath::Abs(Cs), .8), Ey = FMath::Sign(Sn) * FMath::Pow(FMath::Abs(Sn), .8);
					const double Gr = Rows[v].Grow * (1 + .25 * FMath::Sin(4 * Th + Seed * 1.3));
					const FVector Q((W * .5 * Jag * Rows[v].K + Gr) * Ex, (H * .5 * Jag * Rows[v].K + Gr) * Ey, Rows[v].Z + (v == 4 ? .02 * FMath::Sin(5 * Th + Seed) : 0));
					P.Add(Q);
					const double N = Noise3(Q * 6.0 + FVector(2.2), Seed);
					Col.Add(Mul(FMath::Lerp(FMath::Lerp(Rust, RustDark, float(Smooth(N, .3, .7))), Steel, float(Smooth(Noise3(Q * 2.2, Seed + 3), .62, .8) * .5)), Rows[v].Lt));
				}
			AddSheet(*M, P, Col, NU, NV, FVector(0, 0, -.5), 5);
			return Out;
		}

		struct FRockSet { FShape Shape[NumRockKinds][NumVariants]; bool bDone = false; };
		FRockSet& Rocks()
		{
			static FRockSet S;
			if (!S.bDone)
			{
				S.bDone = true;
				for (int32 k = 0; k < NumKinds; k++)
					for (int32 v = 0; v < NumVariants; v++)
					{
						const FBuilt B = BuildRockGeo(Defs[k][v], 0, nullptr);
						S.Shape[k][v] = B.Shape;
						if (B.Outside) UE_LOG(LogTemp, Warning, TEXT("KRDen: %s %d has %d points of its pocket outside its rock"), KindName(k), v, B.Outside);
					}
				for (int32 v = 0; v < NumVariants; v++) S.Shape[GapKind][v] = BuildGapGeo(v, 0, nullptr).Shape;
			}
			return S;
		}
	}

	const FShape& ShapeOf(int32 Kind, int32 Var) { return Rocks().Shape[FMath::Clamp(Kind, 0, NumRockKinds - 1)][FMath::Clamp(Var, 0, NumVariants - 1)]; }

	void BuildRock(int32 Kind, int32 Var, int32 Lod, FKeysMesh& M)
	{
		if (Kind == GapKind) BuildGapGeo(Var, Lod, &M);
		else BuildRockGeo(Defs[Kind][Var], Lod, &M);
	}

	UStaticMesh* MakeStatic(UObject* Owner, const FKeysMesh& Src, UMaterialInterface* Mat, const FString& Name, bool bNormalize, int32* OutTris)
	{
		FKeysMesh M = Src;
		M.SyncKinds();
		double Kk = 1, Cx = 0;
		if (bNormalize)
		{
			double MinX = 1e9, MaxX = -1e9;
			for (const FVector& V : M.V) { MinX = FMath::Min(MinX, V.X); MaxX = FMath::Max(MaxX, V.X); }
			Kk = 100.0 / FMath::Max(1.0, MaxX - MinX); Cx = (MinX + MaxX) * .5;
		}
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
			const FVertexID V = MD.CreateVertex();
			Pos[V] = FVector3f((M.V[i] - FVector(Cx, 0, 0)) * Kk);
			const FVertexInstanceID I = MD.CreateVertexInstance(V);
			Nrm[I] = FVector3f(M.N[i]);
			Col[I] = FVector4f(M.C[i].R, M.C[i].G, M.C[i].B, 1);
			UVs.Set(I, 0, FVector2f(M.UV.IsValidIndex(i) ? M.UV[i] : FVector2D::ZeroVector));
			UVs.Set(I, 1, FVector2f(M.K[i]));
			Inst[i] = I;
		}
		int32 Tris = 0;
		for (int32 i = 0; i + 2 < M.T.Num(); i += 3)
		{
			const int32 Va = M.T[i], Vb = M.T[i + 1], Vc = M.T[i + 2];
			const FVector Fn = FVector::CrossProduct(M.V[Vb] - M.V[Va], M.V[Vc] - M.V[Va]);
			if (Fn.SizeSquared() < 1e-10) continue;
			// (Unreal's front face: the cross product points away from the normal)
			const bool bFlip = FVector::DotProduct(Fn, M.N[Va] + M.N[Vb] + M.N[Vc]) > 0;
			MD.CreateTriangle(Grp, {Inst[Va], Inst[bFlip ? Vc : Vb], Inst[bFlip ? Vb : Vc]});
			Tris++;
		}
		if (OutTris) *OutTris = Tris;
		if (!Tris) return nullptr;
		UStaticMesh* SM = NewObject<UStaticMesh>(Owner, *Name);
		SM->GetStaticMaterials().Add(FStaticMaterial(Mat, FName(TEXT("Mat"))));
		if (bNormalize) { SM->SetPositiveBoundsExtension(FVector(40)); SM->SetNegativeBoundsExtension(FVector(40)); }
		UStaticMesh::FBuildMeshDescriptionsParams Bp;
		Bp.bFastBuild = true;
		Bp.bMarkPackageDirty = false;
		Bp.bBuildSimpleCollision = false;
		SM->BuildFromMeshDescriptions({&MD}, Bp);
		if (bNormalize) SM->CalculateExtendedBounds();
		return SM;
	}

	// ================================================================ the ground
	namespace
	{
		// the floor as the seafloor mesh draws it on a lattice of Step metres (64 m chunks cut into squares, each split
		// corner to corner): what a den must stand clear of, which is not the true height between the lattice's points
		double LatticeFloor(double X, double Z, double Step)
		{
			const double X0 = FMath::FloorToDouble(X / 64) * 64, Z0 = FMath::FloorToDouble(Z / 64) * 64;
			const double Fu = (X - X0) / Step, Fv = (Z - Z0) / Step;
			const int32 I = FMath::FloorToInt32(Fu), J = FMath::FloorToInt32(Fv);
			const double U = Fu - I, V = Fv - J;
			const double HA = KeysSea::SeafloorHeightAt(X0 + I * Step, Z0 + J * Step), HB = KeysSea::SeafloorHeightAt(X0 + (I + 1) * Step, Z0 + J * Step);
			const double HC = KeysSea::SeafloorHeightAt(X0 + I * Step, Z0 + (J + 1) * Step), HD = KeysSea::SeafloorHeightAt(X0 + (I + 1) * Step, Z0 + (J + 1) * Step);
			if (U + V <= 1) return HA + (HB - HA) * U + (HC - HA) * V;
			return HD + (HC - HD) * (1 - U) + (HB - HD) * (1 - V);
		}
	}

	double FloorNear(double X, double Z) { return LatticeFloor(X, Z, 2.0); }

	FString ReefNameAt(double X, double Z)
	{
		struct FNamed { FString Name; double X, Z; };
		static TArray<FNamed> All;
		static bool bBuilt = false;
		if (!bBuilt)
		{
			bBuilt = true;
			if (KeysGeo::RealMap())
			{
				All.Add({TEXT("Sombrero Reef"), -1800, 2760});
				for (const KeysGeoReal::FSite& S : KeysGeoReal::Sites()) if (S.Kind == TEXT("reef")) All.Add({S.Name, S.X, S.Z});
			}
			else All.Add({TEXT("Sombrero Reef"), 250, KeysGeo::ChainZ(250) + 1380});
			for (const KeysGeo::FHump& Hm : KeysGeo::Humps()) if (Hm.bPatch) All.Add({Hm.Name, Hm.X, KeysGeo::ChainZ(Hm.X) + Hm.DZ});
		}
		double Best = 700.0 * 700.0;
		const FNamed* Hit = nullptr;
		for (const FNamed& N : All)
		{
			const double D2 = FMath::Square(N.X - X) + FMath::Square(N.Z - Z);
			if (D2 < Best) { Best = D2; Hit = &N; }
		}
		return Hit ? Hit->Name : FString(TEXT("the reef"));
	}

	EHab HabitatOf(const FGround& G)
	{
		if (G.Depth < 2.6 || G.Dredged > .2) return HabNone;           // flats, mangrove shallows, canals
		if (G.Zone == 2)
		{
			if (G.Sand <= .03) return HabNone;                           // the sand of a groove
			if (G.Depth >= 18) return HabDeep;
			if (G.Site == 1 || G.Sand < .8) return HabSpur;              // the flank of a spur, beside its sand
			return G.Depth < 6 ? HabCrest : HabFore;
		}
		if (G.Zone == 1) return (FMath::Max(G.Patch, G.bMicro ? 1.0 : 0.0) > .3 && G.Sand > .03) ? HabPatch : HabNone;
		// seaward of the reef line and below it: the ledges of the deep reef, where there is hard bottom
		if (G.DZ > 900 && G.Depth >= 18 && G.Depth < 70 && G.HardDeep > .5) return HabDeep;
		return HabNone;
	}

	namespace
	{
		constexpr int32 NCand = 14;
		constexpr int32 DenSalt = 50000000;
		const double HabDens[NumHab] = {0, 1.2, 2.5, 5, 6, 4};
		// which kind, by habitat (ledge, crevice, hole, cave, plate)
		const double KindW[NumHab][NumKinds] = {
			{0, 0, 0, 0, 0},
			{30, 30, 40, 0, 0},
			{20, 35, 45, 0, 0},
			{25, 25, 30, 8, 12},
			{45, 20, 20, 5, 10},
			{15, 10, 15, 30, 30},
		};
		struct FOdds { ERes Res; double W; };
		// who lives in which kind of den, in parts of 100. Most are empty or only a pair of eyes.
		const TArray<FOdds>& OddsOf(int32 Kind)
		{
			static const TArray<FOdds> T[NumRockKinds] = {
				/* ledge */   {{ResNone, 60}, {ResEyes, 12}, {Lobster, 11}, {Squirrelfish, 5}, {Soldierfish, 3}, {NurseShark, 3}, {GreenMoray, 2}, {Octopus, 1.5}, {StoneCrab, 1.5}, {Shrimp, 1}},
				/* crevice */ {{ResNone, 60}, {ResEyes, 12}, {SpottedMoray, 8}, {GreenMoray, 7}, {Lobster, 5}, {ClingingCrab, 4}, {Shrimp, 2.5}, {Octopus, 1.5}},
				/* hole */    {{ResNone, 58}, {ResEyes, 13}, {SpottedMoray, 9}, {Octopus, 5}, {Shrimp, 5}, {StoneCrab, 4}, {ClingingCrab, 3}, {GreenMoray, 3}},
				/* cave */    {{ResNone, 50}, {ResEyes, 10}, {Sweepers, 13}, {Soldierfish, 5}, {GreenMoray, 5}, {Squirrelfish, 4}, {NurseShark, 4}, {Lobster, 3.5}, {BlackGrouper, 3}, {Goliath, 2.5}},
				/* plate */   {{ResNone, 60}, {ResEyes, 12}, {Soldierfish, 8}, {Lobster, 6}, {Squirrelfish, 5}, {SpottedMoray, 5}, {Shrimp, 2}, {ClingingCrab, 2}},
				/* gap */     {{ResNone, 30}, {ResEyes, 10}, {GreenMoray, 25}, {Sweepers, 20}, {Goliath, 15}},
			};
			return T[FMath::Clamp(Kind, 0, NumRockKinds - 1)];
		}
	}

	// who lives here (and how many), from the den's own seed: the same every run
	void RollResident(FDen& D)
	{
		const TArray<FOdds>& O = OddsOf(D.Kind);
		double Sum = 0;
		for (const FOdds& Od : O) Sum += Od.W;
		double R = Hash01(D.Seed, 101) * Sum;
		ERes Res = ResNone;
		for (const FOdds& Od : O) { if (R < Od.W) { Res = Od.Res; break; } R -= Od.W; }
		// what the water over it allows
		if (Res == NurseShark && D.W < 1.25) Res = Lobster;                       // a nurse shark needs a long ledge
		if (Res == GreenMoray && D.Depth < 4) Res = SpottedMoray;                 // the big green keeps to deeper holes
		if (Res == Goliath && D.Depth > 40) Res = BlackGrouper;
		if (Res == BlackGrouper && D.Depth < 8) Res = ResNone;
		if (Res == StoneCrab && D.Depth > 14) Res = ClingingCrab;
		if (Res == Sweepers && D.W < .6) Res = ResEyes;
		D.Res = uint8(Res);
		const double Rc = Hash01(D.Seed, 102);
		D.Count = 1;
		if (Res == Lobster) D.Count = uint8(FMath::Clamp(D.Kind == Ledge || D.Kind == Cave || D.Kind == Plate ? 2 + int32(Rc * 4) : 1 + int32(Rc * 2), 1, FMath::Max(1, int32(D.W / .42))));
		else if (Res == Squirrelfish) D.Count = uint8(3 + int32(Rc * 4));
		else if (Res == Soldierfish) D.Count = uint8(4 + int32(Rc * 5));
		else if (Res == Sweepers) D.Count = uint8(22 + int32(Rc * 15));
		D.bBuddy = Res == GreenMoray && Hash01(D.Seed, 103) < .4;
	}

	// a den's shape carried to where it stands
	void Stand(FDen& D)
	{
		const FShape& S = ShapeOf(D.Kind, D.Var);
		const double Cs = FMath::Cos(D.Yaw), Sn = FMath::Sin(D.Yaw);
		auto Turn = [&](const FVector& L) { return FVector(L.X * Cs + L.Z * Sn, L.Y, -L.X * Sn + L.Z * Cs); };
		const FVector O(D.X, D.Y, D.Z);
		D.Mouth = O + Turn(S.Mouth) * D.Scale;
		D.Axis = Turn(S.Axis);
		D.Right = Turn(FVector(1, 0, 0));
		D.W = S.W * D.Scale; D.H = S.H * D.Scale; D.Deep = S.Deep * D.Scale;
		D.C = O + Turn(S.C) * D.Scale;
		D.Rad = S.Rad * D.Scale;
	}

	void PlaceChunk(int32 CX, int32 CZ, const TArray<AKeysReef::FInst>& Corals, TArray<FDen>& Out)
	{
		const double X0 = CX * ChunkM, Z0 = CZ * ChunkM;
		// the corals a den must leave alone: anything solid (the fans, rods and weed bend round a rock; a head would be buried)
		TArray<FVector2D> Solid;
		for (const AKeysReef::FInst& I : Corals) if (AKeysReef::IsSolidCoral(I.Species)) Solid.Add(FVector2D(I.X, I.Z));
		for (int32 Slot = 0; Slot < NCand; Slot++)
		{
			auto Hs = [&](int32 A) { return ChunkHash(CX, CZ, DenSalt + Slot * 64 + A); };
			// (kept 4 m inside the chunk, so no den lies across two chunks)
			const double X = X0 + 4 + Hs(0) * (ChunkM - 8), Z = Z0 + 4 + Hs(1) * (ChunkM - 8);
			const FGround G = GroundAt(X, Z);
			const EHab Hab = HabitatOf(G);
			if (Hab == HabNone || Hs(2) >= HabDens[Hab] / NCand) continue;
			FDen D;
			D.Hab = uint8(Hab);
			double Wk = 0, Rk = Hs(3);
			for (int32 k = 0; k < NumKinds; k++) Wk += KindW[Hab][k];
			Rk *= Wk;
			for (int32 k = 0; k < NumKinds; k++) { if (Rk < KindW[Hab][k]) { D.Kind = uint8(k); break; } Rk -= KindW[Hab][k]; }
			D.Var = uint8(FMath::Min(NumVariants - 1, int32(Hs(4) * NumVariants)));
			D.Scale = FMath::Lerp(.85, 1.18, Hs(5));
			D.Seed = uint32(Hs(6) * 4294967295.0);
			D.Id = (int64(CX) << 40) ^ (int64(uint32(CZ)) << 8) ^ int64(Slot);
			D.Depth = G.Depth;
			const FShape& S = ShapeOf(D.Kind, D.Var);
			const double Rx = S.Rad.X * D.Scale, Rz = S.Rad.Z * D.Scale;
			// not on top of another den
			bool bClash = false;
			for (const FDen& O : Out) if (FVector2D::Distance(FVector2D(O.X, O.Z), FVector2D(X, Z)) < FMath::Max(Rx, Rz) + FMath::Max(O.Rad.X, O.Rad.Z) + 1) { bClash = true; break; }
			if (bClash) continue;
			// which way it opens: on a spur's flank, toward the sand of the groove; else as the dice say
			double Yaw0 = Hs(7) * 2 * PI;
			if (Hab == HabSpur)
			{
				const double Gx = GroundAt(X + 1.5, Z).Sand - GroundAt(X - 1.5, Z).Sand, Gz = GroundAt(X, Z + 1.5).Sand - GroundAt(X, Z - 1.5).Sand;
				if (Gx * Gx + Gz * Gz > 1e-4) Yaw0 = FMath::Atan2(-Gx, -Gz) + (Hs(7) - .5) * .5;   // (local +z turns to (sin, cos) of the yaw)
			}
			const double YawTry[] = {0, .7, -.7, 1.5, -1.5, PI};
			bool bFits = false;
			for (const double Dy : YawTry)
			{
				const double Yaw = Yaw0 + Dy, Cs = FMath::Cos(Yaw), Sn = FMath::Sin(Yaw);
				bool bHit = false;
				for (const FVector2D& C : Solid)
				{
					const double Wx = C.X - X, Wz = C.Y - Z;
					if (FMath::Abs(Wx) > Rx + Rz + 2 || FMath::Abs(Wz) > Rx + Rz + 2) continue;
					const double Lx = Wx * Cs - Wz * Sn, Lz = Wx * Sn + Wz * Cs;
					// inside the rock, or in the way of the opening
					if (FMath::Square(Lx / (Rx + .15)) + FMath::Square(Lz / (Rz + .15)) < 1) { bHit = true; break; }
					if (FMath::Abs(Lx - S.Mouth.X * D.Scale) < S.W * D.Scale * .5 + .3 && Lz > 0 && Lz < Rz + 1.3) { bHit = true; break; }
				}
				if (bHit) continue;
				D.Yaw = Yaw; bFits = true;
				break;
			}
			if (!bFits) continue;
			// it stands at the highest the bottom is drawn under its opening and its pocket, at any of the seafloor's
			// levels of detail (2 m squares near a diver, 6.4 m further off, 3.6 m from the boat), so the floor never
			// comes up inside; the rock's own foot goes down past the lowest
			D.X = X; D.Z = Z; D.Y = 0;
			Stand(D);
			double Hi = -1e9, Lo = 1e9;
			const FVector2D Mc(D.Mouth.X, D.Mouth.Z), Ax(D.Axis.X, D.Axis.Z), Rt(D.Right.X, D.Right.Z);
			const FVector2D Pts[] = {Mc, Mc + Rt * (D.W * .45), Mc - Rt * (D.W * .45), Mc - Ax * D.Deep, Mc - Ax * (D.Deep * .5) + Rt * (D.W * .3), Mc - Ax * (D.Deep * .5) - Rt * (D.W * .3), Mc + Ax * .5, FVector2D(X, Z)};
			for (const FVector2D& P : Pts)
				for (const double Step : {2.0, 6.4, 64.0 / 18})
				{
					const double Hf = LatticeFloor(P.X, P.Y, Step);
					Hi = FMath::Max(Hi, Hf); Lo = FMath::Min(Lo, Hf);
				}
			if (Hi - Lo > .45) continue;   // too steep or too broken for a den to sit right
			if (-Hi < 2.2) continue;       // and never where its top would near the surface
			D.Y = Hi;
			Stand(D);
			RollResident(D);
			Out.Add(D);
		}
	}

	// ================================================================ the runtime
	struct FDens
	{
		AKeysReef* Reef = nullptr;
		FLife* Life = nullptr;
		UMaterialInterface* RockMat = nullptr;
		int32 Pool[NumRockKinds][NumVariants][2];
		int32 Tris[NumRockKinds][NumVariants][2];
		TMap<int64, TArray<FDen>> Cache;          // by chunk
		TArray<FDen> Gaps;                        // the wrecks' own
		TArray<FDen> Near;                        // within 60 m of the camera just now (copies: the chunk cache may let a chunk go)
		TArray<FDen> Solid;                       // what the diver and the spear cannot pass through, round the camera
		TArray<TArray<FTransform>> Xf;
		TArray<uint8> LodOf;                      // per drawn den, in Drawn order
		TArray<FDen> Drawn;
		int32 CCX = INT32_MIN, CCZ = INT32_MIN;
		double CamX = 0, CamZ = 0, LodT = 0, StatT = 0, NearT = 0;
		int32 DrawnN = 0, DrawnTris = 0, GapsSeen = 0;
		bool bHide = false, bCalm = false, bScript = false;
		FWorldNow W;
		// test camera work: hold the diver in front of a den
		bool bPin = false, bPinNew = false;
		FVector PinP = FVector::ZeroVector;
		double PinYaw = 0, PinPitch = 0;
		TWeakObjectPtr<USpotLightComponent> Torch;
		double PlaceMs = 0, WorstPlaceMs = 0;
		int32 PlaceN = 0;
		// census: a queue of chunks to count, a few a frame
		struct FCensusJob { FString Name; int32 CX, CZ; };
		TArray<FCensusJob> CensusQ;
		TMap<FString, TArray<int32>> CensusBy;    // reef -> [dens, then one count per resident]
	};
	static FDens* GDens = nullptr;
	static TArray<FDen> GPendingGaps;               // gaps a wreck asked for before the reef woke

	namespace
	{
		int64 ChunkKey(int32 CX, int32 CZ) { return (int64(CX) << 32) ^ int64(uint32(CZ)); }

		void LayOut(FDens* D)
		{
			AKeysReef* R = D->Reef;
			for (TArray<FTransform>& A : D->Xf) A.Reset();
			D->DrawnN = 0; D->DrawnTris = 0;
			for (int32 i = 0; i < D->Drawn.Num(); i++)
			{
				const FDen& Dn = D->Drawn[i];
				const int32 Lod = D->LodOf[i] & 1;
				const double Grow = (D->LodOf[i] >> 1) / 100.0;
				const int32 P = D->Pool[Dn.Kind][Dn.Var][Lod];
				if (P < 0) continue;
				if (Dn.Kind == GapKind)
				{
					// a gap lies on its plating: the mesh's front along the plate's own normal
					const FVector Fw = D2U(Dn.Axis), Up = D2U(FVector::CrossProduct(Dn.Axis, Dn.Right));
					D->Xf[P].Add(FTransform(FRotationMatrix::MakeFromYZ(Fw, Up).ToQuat(), G2U(Dn.Mouth), FVector(Dn.Scale)));
				}
				else D->Xf[P].Add(FTransform(FRotator(0, -FMath::RadiansToDegrees(Dn.Yaw), 0), G2U(FVector(Dn.X, Dn.Y, Dn.Z)), FVector(Dn.Scale * Grow)));
				D->DrawnN++;
				D->DrawnTris += D->Tris[Dn.Kind][Dn.Var][Lod];
			}
			for (int32 P = 0; P < D->Xf.Num(); P++)
			{
				UInstancedStaticMeshComponent* C = R->DenPools.IsValidIndex(P) ? R->DenPools[P].Get() : nullptr;
				if (!C) continue;
				if (C->GetInstanceCount() == 0 && D->Xf[P].Num() == 0) continue;
				C->ClearInstances();
				if (D->Xf[P].Num() && !D->bHide) C->AddInstances(D->Xf[P], false, true);
				C->MarkRenderStateDirty();
			}
		}
	}

	const TArray<FDen>& DensOfChunk(int32 CX, int32 CZ)
	{
		static const TArray<FDen> None;
		if (!GDens || !GDens->Reef) return None;
		GDens->Reef->ChunkInstances(CX, CZ);
		const TArray<FDen>* Have = GDens->Cache.Find(ChunkKey(CX, CZ));
		return Have ? *Have : None;
	}

	// called by the reef when it has dealt a chunk its corals: deal the dens, and stand each one in the coral list as a
	// coral head (not drawn by the reef), so whoever reads that list for what a fish must swim round swims round it
	void OnChunk(FDens* D, int32 CX, int32 CZ, TArray<AKeysReef::FInst>& Corals)
	{
		if (!D) return;
		const double T0 = FPlatformTime::Seconds();
		TArray<FDen> Dn;
		PlaceChunk(CX, CZ, Corals, Dn);
		for (const FDen& O : Dn)
		{
			// (a long ledge as two heads side by side)
			const int32 N = O.Rad.X > O.Rad.Z * 1.6 ? 2 : 1;
			for (int32 k = 0; k < N; k++)
			{
				AKeysReef::FInst I;
				I.Species = AKeysReef::DenStandIn();
				I.Den = 1;
				const double Off = N == 1 ? 0 : (k ? .5 : -.5) * O.Rad.X;
				I.X = O.X + O.Right.X * Off; I.Z = O.Z + O.Right.Z * Off; I.Y = O.Y;
				// (the reader takes a head of this species as 1 m across and .72 m tall at scale 1)
				I.SX = I.SZ = N == 1 ? FMath::Max(O.Rad.X, O.Rad.Z) : O.Rad.Z;
				I.SY = ShapeOf(O.Kind, O.Var).Top * O.Scale / .72;
				Corals.Add(I);
			}
		}
		const double Ms = (FPlatformTime::Seconds() - T0) * 1000;
		D->PlaceMs += Ms; D->PlaceN++; D->WorstPlaceMs = FMath::Max(D->WorstPlaceMs, Ms);
		D->Cache.Add(ChunkKey(CX, CZ), MoveTemp(Dn));
	}

	void Forget(FDens* D, int64 Key) { if (D) D->Cache.Remove(Key); }

	FDens* Create(AKeysReef* Reef)
	{
		if (!Enabled() || !Reef) return nullptr;
		FDens* D = new FDens;
		D->Reef = Reef;
		FString Script;
		D->bScript = FParse::Value(FCommandLine::Get(), TEXT("KRScript="), Script, false);
		D->RockMat = AKeysWorld::VertexColorMaterial();
		const double T0 = FPlatformTime::Seconds();
		int32 Total = 0;
		for (int32 k = 0; k < NumRockKinds; k++)
			for (int32 v = 0; v < NumVariants; v++)
				for (int32 Lod = 0; Lod < 2; Lod++)
				{
					FKeysMesh M;
					BuildRock(k, v, Lod, M);
					int32 Tris = 0;
					UStaticMesh* SM = MakeStatic(Reef, M, D->RockMat, FString::Printf(TEXT("Den_%d_%d_%d"), k, v, Lod), false, &Tris);
					D->Pool[k][v][Lod] = -1; D->Tris[k][v][Lod] = Tris;
					if (!SM) continue;
					Reef->DenMeshes.Add(SM);
					UInstancedStaticMeshComponent* C = NewObject<UInstancedStaticMeshComponent>(Reef, *FString::Printf(TEXT("DenPool_%d_%d_%d"), k, v, Lod));
					C->SetStaticMesh(SM);
					C->SetupAttachment(Reef->GetRootComponent());
					C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
					C->SetCastShadow(Lod == 0);
					C->SetMaterial(0, D->RockMat);
					C->bAffectDistanceFieldLighting = false;
					C->RegisterComponent();
					D->Pool[k][v][Lod] = Reef->DenPools.Add(C);
					Total += Tris;
				}
		D->Xf.SetNum(Reef->DenPools.Num());
		D->Life = CreateLife(Reef);
		D->Gaps = GPendingGaps;
		GDens = D;
		UE_LOG(LogTemp, Log, TEXT("KRDen: %d den meshes (%d kinds x %d sizes x 2 levels of detail), %d triangles in all, made in %.1f ms"), Reef->DenPools.Num(), NumRockKinds, NumVariants, Total, (FPlatformTime::Seconds() - T0) * 1000);
		return D;
	}

	void Destroy(FDens* D)
	{
		if (!D) return;
		if (GDens == D) GDens = nullptr;
		DestroyLife(D->Life);
		delete D;
	}

	void Rebuild(FDens* D, double CamX, double CamZ, int32 CCX, int32 CCZ)
	{
		if (!D) return;
		D->CCX = CCX; D->CCZ = CCZ; D->CamX = CamX; D->CamZ = CamZ;
		D->Drawn.Reset(); D->LodOf.Reset();
		// the same round reach as the reef's own instances, growing in over its last metres
		constexpr double Reach = 110, RimFade = 24;
		for (int32 dz = -3; dz <= 3; dz++)
			for (int32 dx = -3; dx <= 3; dx++)
			{
				const TArray<FDen>* Have = D->Cache.Find(ChunkKey(CCX + dx, CCZ + dz));
				if (!Have) continue;
				for (const FDen& Dn : *Have)
				{
					const double Rim = FVector2D(Dn.X - FMath::Clamp(Dn.X, CCX * ChunkM, (CCX + 1) * ChunkM), Dn.Z - FMath::Clamp(Dn.Z, CCZ * ChunkM, (CCZ + 1) * ChunkM)).Size();
					if (Rim >= Reach) continue;
					const double Grow = Smooth(Reach - Rim, 0, RimFade);
					const double Dist = FVector2D(Dn.X - CamX, Dn.Z - CamZ).Size();
					D->Drawn.Add(Dn);
					D->LodOf.Add(uint8((Dist < 24 ? 0 : 1) | (FMath::RoundToInt32(Grow * 100) << 1)));
				}
			}
		for (const FDen& Dn : D->Gaps)
		{
			const double Dist = FVector2D(Dn.Mouth.X - CamX, Dn.Mouth.Z - CamZ).Size();
			if (Dist > 160) continue;
			D->Drawn.Add(Dn);
			D->LodOf.Add(uint8((Dist < 24 ? 0 : 1) | (100 << 1)));
		}
		LayOut(D);
	}

	namespace
	{
		// is P (game metres) inside this den's rock, swollen by R? OutDir / OutR: the way out and the rock's radius there
		bool Inside(const FDen& Dn, const FVector& P, double R, FVector* OutDir = nullptr, double* OutR = nullptr)
		{
			if (Dn.Kind == GapKind) return false;   // (the wreck's own steel is the wreck's business)
			const FVector Rel = P - Dn.C;
			if (FMath::Abs(Rel.X) > Dn.Rad.X + Dn.Rad.Z + R || FMath::Abs(Rel.Z) > Dn.Rad.X + Dn.Rad.Z + R || Rel.Y > Dn.Rad.Y * 1.3 + R) return false;
			const double Cs = FMath::Cos(Dn.Yaw), Sn = FMath::Sin(Dn.Yaw);
			const FVector L(Rel.X * Cs - Rel.Z * Sn, Rel.Y, Rel.X * Sn + Rel.Z * Cs);
			const double Len = L.Size();
			if (Len < 1e-6) { if (OutDir) *OutDir = FVector(0, 1, 0); if (OutR) *OutR = Dn.Rad.Y; return true; }
			const FVector Dir = L / Len;
			const double Pw = ShapeOf(Dn.Kind, Dn.Var).Pw;
			const double S = FMath::Pow(FMath::Abs(Dir.X) / Dn.Rad.X, Pw) + FMath::Pow(FMath::Abs(Dir.Y) / Dn.Rad.Y, Pw) + FMath::Pow(FMath::Abs(Dir.Z) / Dn.Rad.Z, Pw);
			const double Rr = FMath::Pow(FMath::Max(S, 1e-9), -1 / Pw) * 1.04;
			if (Len >= Rr + R) return false;
			if (OutDir) *OutDir = FVector(Dir.X * Cs + Dir.Z * Sn, Dir.Y, -Dir.X * Sn + Dir.Z * Cs);
			if (OutR) *OutR = Rr;
			return true;
		}
	}

	bool PushOut(double& X, double& Y, double& Z, double R)
	{
		if (!GDens) return false;
		bool bAny = false;
		for (const FDen& Dn : GDens->Solid)
		{
			FVector Dir;
			double Rr;
			if (!Inside(Dn, FVector(X, Y, Z), R, &Dir, &Rr)) continue;
			// out along the rock's own radius, never down into the bottom
			if (Dir.Y < 0) { Dir.Y = 0; if (!Dir.Normalize()) Dir = FVector(0, 1, 0); }
			const double Have = FVector::DotProduct(FVector(X, Y, Z) - Dn.C, Dir);
			const FVector Q = FVector(X, Y, Z) + Dir * FMath::Max(0.0, Rr + R + .01 - Have);
			X = Q.X; Y = Q.Y; Z = Q.Z;
			bAny = true;
		}
		return bAny;
	}

	double RayHit(const FVector& O, const FVector& Dir, double MaxT)
	{
		if (!GDens) return MaxT;
		double Best = MaxT;
		for (const FDen& Dn : GDens->Solid)
		{
			// near enough to matter at all?
			const FVector Rel = Dn.C - O;
			const double Along = FVector::DotProduct(Rel, Dir), Big = FMath::Max3(Dn.Rad.X, Dn.Rad.Y, Dn.Rad.Z) * 1.1;
			if (Along < -Big || Along > Best + Big || (Rel - Dir * Along).Size() > Big) continue;
			for (double T = FMath::Max(0.0, Along - Big); T < FMath::Min(Best, Along + Big); T += .06)
				if (Inside(Dn, O + Dir * T, 0)) { Best = T; break; }
		}
		return Best;
	}

	void SchoolThreats(TArray<FVector4>& Out)
	{
		if (GDens && GDens->Life) LifeThreats(GDens->Life, Out);
	}

	void AddWreckGap(const FVector& Mouth, const FVector& OutDir, double W, double H, uint32 Seed)
	{
		FDen D;
		D.Kind = uint8(GapKind);
		// the size of patch nearest what was asked for
		double BestE = 1e9;
		for (int32 v = 0; v < NumVariants; v++)
		{
			const double E = FMath::Abs(GapDefs[v][0] - W) + FMath::Abs(GapDefs[v][1] - H);
			if (E < BestE) { BestE = E; D.Var = uint8(v); }
		}
		D.Scale = FMath::Clamp(W / GapDefs[D.Var][0], .7, 1.4);
		D.Seed = Seed;
		D.Id = (int64(0x7e) << 56) ^ int64(Seed);
		D.Hab = uint8(HabDeep);
		D.Mouth = Mouth; D.Axis = OutDir.GetSafeNormal(.0001, FVector(0, 0, 1));
		D.Right = FVector::CrossProduct(FVector(0, 1, 0), D.Axis).GetSafeNormal(.0001, FVector(1, 0, 0));
		D.X = Mouth.X; D.Y = Mouth.Y; D.Z = Mouth.Z;
		D.Yaw = FMath::Atan2(D.Axis.X, D.Axis.Z);
		D.W = GapDefs[D.Var][0] * D.Scale; D.H = GapDefs[D.Var][1] * D.Scale; D.Deep = GapDefs[D.Var][2] * D.Scale;
		D.C = Mouth - D.Axis * .5; D.Rad = FVector(D.W, D.H, .5);
		D.Depth = -Mouth.Y;
		RollResident(D);
		if (GDens) { GDens->Gaps.Add(D); GDens->CCX = INT32_MIN; }
		else GPendingGaps.Add(D);
	}

	namespace
	{
		void ReadWorld(FDens* D, double Dt)
		{
			FWorldNow& W = D->W;
			UWorld* Wd = D->Reef->GetWorld();
			W.Dt = Dt; W.Now = Wd->GetTimeSeconds();
			W.Hour = KeysFeed::HourNow(D->Reef);
			W.bNight = W.Hour >= 19.75 || W.Hour < 5.75;
			W.bCalm = D->bCalm; W.bScript = D->bScript;
			if (APlayerCameraManager* Cam = UGameplayStatics::GetPlayerCameraManager(D->Reef, 0))
			{
				const FVector L = Cam->GetCameraLocation(), F = Cam->GetCameraRotation().Vector();
				W.Cam = FVector(L.X / 100, L.Z / 100, L.Y / 100);
				W.CamFwd = FVector(F.X, F.Z, F.Y);
			}
			W.bDiver = false; W.bLight = false;
			if (AKeysDiver* Dv = AKeysDiver::Get(D->Reef); Dv && Dv->IsDiving())
			{
				const KeysDive::FState& S = Dv->State();
				W.bDiver = true;
				W.Diver = FVector(S.X, S.Y, S.Z);
				W.DiverV = FVector(S.VX, S.VY, S.VZ);
				// the dive light is the diver's own spot light: lit when it is showing
				if (!D->Torch.IsValid()) D->Torch = Dv->FindComponentByClass<USpotLightComponent>();
				if (const USpotLightComponent* T = D->Torch.Get(); T && T->IsVisible() && T->Intensity > 5)
				{
					const FVector P = T->GetComponentLocation(), F = T->GetForwardVector();
					W.bLight = true;
					W.LightP = FVector(P.X / 100, P.Z / 100, P.Y / 100);
					W.LightD = FVector(F.X, F.Z, F.Y);
				}
				if (D->bPin)
				{
					// (the current carries her off: put back when she has drifted a hand's width)
					if (D->bPinNew || FVector2D(S.X - D->PinP.X, S.Z - D->PinP.Z).Size() > .08) Dv->DebugCmd(TEXT("at"), FString::Printf(TEXT("%.3f,%.3f"), D->PinP.X, D->PinP.Z));
					if (D->bPinNew)
					{
						Dv->DebugCmd(TEXT("hold"), FString::Printf(TEXT("%.3f"), -D->PinP.Y));
						Dv->DebugCmd(TEXT("look"), FString::Printf(TEXT("%.4f,%.4f"), D->PinYaw, D->PinPitch));
					}
					D->bPinNew = false;
				}
			}
		}

		// every den within R of a point, nearest first (from the chunks already dealt and those the search deals)
		void DensNear(FDens* D, const FVector2D& At, double R, TArray<FDen>& Out)
		{
			const int32 C0x = FMath::FloorToInt32((At.X - R) / ChunkM), C1x = FMath::FloorToInt32((At.X + R) / ChunkM);
			const int32 C0z = FMath::FloorToInt32((At.Y - R) / ChunkM), C1z = FMath::FloorToInt32((At.Y + R) / ChunkM);
			for (int32 cz = C0z; cz <= C1z; cz++)
				for (int32 cx = C0x; cx <= C1x; cx++)
					for (const FDen& Dn : DensOfChunk(cx, cz))
						if (FVector2D::Distance(FVector2D(Dn.Mouth.X, Dn.Mouth.Z), At) <= R) Out.Add(Dn);
			for (const FDen& Dn : D->Gaps) if (FVector2D::Distance(FVector2D(Dn.Mouth.X, Dn.Mouth.Z), At) <= R) Out.Add(Dn);
			{
				TArray<FDen> Sheet;
				LifeSheetDens(D->Life, Sheet);
				for (const FDen& Dn : Sheet) if (FVector2D::Distance(FVector2D(Dn.Mouth.X, Dn.Mouth.Z), At) <= R) Out.Add(Dn);
			}
			Out.Sort([&](const FDen& A, const FDen& B) { return FVector2D::DistSquared(FVector2D(A.Mouth.X, A.Mouth.Z), At) < FVector2D::DistSquared(FVector2D(B.Mouth.X, B.Mouth.Z), At); });
		}

		bool Matches(const FDen& Dn, const FString& What)
		{
			if (What == TEXT("any")) return true;
			if (What == TEXT("resident")) return Dn.Res > ResEyes;
			for (int32 k = 0; k < NumRockKinds; k++) if (What == KindName(k)) return Dn.Kind == k;
			for (int32 r = 0; r < NumRes; r++) if (What == ResKey(r)) return Dn.Res == r;
			if (What == TEXT("buddy")) return Dn.bBuddy;
			return false;
		}
	}

	void Tick(FDens* D, double Dt)
	{
		if (!D) return;
		ReadWorld(D, Dt);
		const FWorldNow& W = D->W;
		// levels of detail follow the camera between the reef's own rebuilds (it only lays out again a chunk at a time)
		D->LodT -= Dt;
		if (D->LodT <= 0 && D->Drawn.Num() == D->LodOf.Num())
		{
			D->LodT = .4;
			bool bChanged = false;
			for (int32 i = 0; i < D->Drawn.Num(); i++)
			{
				const FDen& Dn = D->Drawn[i];
				const double Dist = FVector2D(Dn.Mouth.X - W.Cam.X, Dn.Mouth.Z - W.Cam.Z).Size();
				const uint8 Was = D->LodOf[i] & 1;
				const uint8 Is = Was ? (Dist < 22 ? 0 : 1) : (Dist > 27 ? 1 : 0);
				if (Is != Was) { D->LodOf[i] = uint8((D->LodOf[i] & ~1) | Is); bChanged = true; }
			}
			if (bChanged) LayOut(D);
		}
		// who is near: the residents' world, and the rock the diver cannot swim through
		D->NearT -= Dt;
		if (D->NearT <= 0)
		{
			D->NearT = .25;
			D->Near.Reset(); D->Solid.Reset();
			const int32 CX = FMath::FloorToInt32(W.Cam.X / ChunkM), CZ = FMath::FloorToInt32(W.Cam.Z / ChunkM);
			for (int32 dz = -2; dz <= 2; dz++)
				for (int32 dx = -2; dx <= 2; dx++)
					if (const TArray<FDen>* Have = D->Cache.Find(ChunkKey(CX + dx, CZ + dz)))
						for (const FDen& Dn : *Have)
						{
							const double Dist = FVector2D(Dn.Mouth.X - W.Cam.X, Dn.Mouth.Z - W.Cam.Z).Size();
							if (Dist < 60) D->Near.Add(Dn);
							if (Dist < 30) D->Solid.Add(Dn);
						}
			for (const FDen& Dn : D->Gaps) if (FVector2D(Dn.Mouth.X - W.Cam.X, Dn.Mouth.Z - W.Cam.Z).Size() < 60) D->Near.Add(Dn);
		}
		if (D->Life)
		{
			TArray<const FDen*> Ptrs;
			if (!D->bHide) for (const FDen& Dn : D->Near) Ptrs.Add(&Dn);
			TickLife(D->Life, W, Ptrs);
		}
		// the census, a few chunks a frame
		for (int32 n = 0; n < 6 && D->CensusQ.Num(); n++)
		{
			const FDens::FCensusJob J = D->CensusQ.Pop(EAllowShrinking::No);
			TArray<int32>& Row = D->CensusBy.FindOrAdd(J.Name);
			if (Row.Num() < NumRes + 1) Row.SetNumZeroed(NumRes + 1);
			for (const FDen& Dn : DensOfChunk(J.CX, J.CZ)) { Row[0]++; Row[1 + Dn.Res] += FMath::Max(1, int32(Dn.Count)); }
			if (!D->CensusQ.Num())
			{
				for (const TPair<FString, TArray<int32>>& Kv : D->CensusBy)
				{
					FString Who;
					for (int32 r = ResEyes; r < NumRes; r++) if (Kv.Value[1 + r]) Who += FString::Printf(TEXT(" %s %d,"), ResName(r), Kv.Value[1 + r]);
					UE_LOG(LogTemp, Log, TEXT("KRDen census %s (the 200 m square on it): %d dens, %d empty;%s"), *Kv.Key, Kv.Value[0], Kv.Value[1 + ResNone], *Who);
				}
				UE_LOG(LogTemp, Log, TEXT("KRDen census done: %d reefs"), D->CensusBy.Num());
			}
		}
	}

	void DebugCmd(FDens* D, const FString& Cmd)
	{
		if (!D) { UE_LOG(LogTemp, Log, TEXT("KRDen: no dens (-KRNoDens, or no reef)")); return; }
		TArray<FString> A;
		Cmd.ParseIntoArray(A, TEXT(" "));
		if (!A.Num()) return;
		const FWorldNow& W = D->W;
		const FVector2D At(W.bDiver ? W.Diver.X : W.Cam.X, W.bDiver ? W.Diver.Z : W.Cam.Z);
		if (A[0] == TEXT("stats"))
		{
			// dens by habitat over the chunks dealt round the camera, and what is drawn and alive just now
			int32 ByHab[NumHab] = {}, ByKind[NumRockKinds] = {}, Chunks[NumHab] = {}, All = 0, NChunks = 0;
			const int32 CX = FMath::FloorToInt32(W.Cam.X / ChunkM), CZ = FMath::FloorToInt32(W.Cam.Z / ChunkM);
			for (int32 dz = -3; dz <= 3; dz++)
				for (int32 dx = -3; dx <= 3; dx++)
				{
					const TArray<FDen>& Dn = DensOfChunk(CX + dx, CZ + dz);
					NChunks++;
					// the chunk's own habitat: what its middle is
					const EHab Hb = HabitatOf(GroundAt((CX + dx + .5) * ChunkM, (CZ + dz + .5) * ChunkM));
					Chunks[Hb]++;
					for (const FDen& O : Dn) { ByHab[O.Hab]++; ByKind[O.Kind]++; All++; }
				}
			FString Hs, Ks;
			for (int32 h = 1; h < NumHab; h++) Hs += FString::Printf(TEXT(" %s %d dens (%d chunks have it at their middle, %.1f a chunk);"), HabName(h), ByHab[h], Chunks[h], Chunks[h] ? double(ByHab[h]) / Chunks[h] : 0.0);
			for (int32 k = 0; k < NumRockKinds; k++) Ks += FString::Printf(TEXT(" %s %d"), KindName(k), ByKind[k]);
			UE_LOG(LogTemp, Log, TEXT("KRDen stats at %.0f, %.0f (%s, %.1f m of water, habitat %s): %d dens in the %d chunks round the camera (%d chunks with no reef):%s kinds:%s"),
				W.Cam.X, W.Cam.Z, *ReefNameAt(W.Cam.X, W.Cam.Z), KeysGeo::DepthFast(W.Cam.X, W.Cam.Z), HabName(HabitatOf(GroundAt(W.Cam.X, W.Cam.Z))), All, NChunks, Chunks[HabNone], *Hs, *Ks);
			int32 NearRes[NumRes] = {}, Near25 = 0;
			for (const FDen& Dn : D->Near) { NearRes[Dn.Res] += FMath::Max(1, int32(Dn.Count)); if (FVector::Dist(Dn.Mouth, W.Cam) < 25) Near25++; }
			FString Who;
			for (int32 r = ResEyes; r < NumRes; r++) if (NearRes[r]) Who += FString::Printf(TEXT(" %s %d,"), ResName(r), NearRes[r]);
			const FLifeStats Ls = D->Life ? LifeStatsOf(D->Life) : FLifeStats();
			UE_LOG(LogTemp, Log, TEXT("KRDen drawn: %d den rocks, %d triangles; %d dens within 60 m of the camera (%d within 25 m), %d of them empty; living there:%s"),
				D->DrawnN, D->DrawnTris, D->Near.Num(), Near25, NearRes[ResNone], Who.IsEmpty() ? TEXT(" nothing") : *Who);
			UE_LOG(LogTemp, Log, TEXT("KRDen animals: %d drawn (%d moving), %d instances in %d instanced meshes, %d triangles; residents' cpu %.3f ms a frame; hour %.1f (%s); dealing a chunk its dens: %.2f ms mean, %.2f ms worst over %d chunks"),
				Ls.Animals, Ls.Animated, Ls.Instances, Ls.Pools, Ls.Tris, Ls.CpuMs, W.Hour, W.bNight ? TEXT("night") : TEXT("day"), D->PlaceN ? D->PlaceMs / D->PlaceN : 0.0, D->WorstPlaceMs, D->PlaceN);
		}
		else if (A[0] == TEXT("census"))
		{
			// every named reef: the dens and residents of the 200 m square on its charted point
			struct FNamed { FString Name; double X, Z; };
			TArray<FNamed> All;
			if (KeysGeo::RealMap())
			{
				All.Add({TEXT("Sombrero Reef"), -1800, 2760});
				for (const KeysGeoReal::FSite& S : KeysGeoReal::Sites()) if (S.Kind == TEXT("reef")) All.Add({S.Name, S.X, S.Z});
			}
			else All.Add({TEXT("Sombrero Reef"), 250, KeysGeo::ChainZ(250) + 1380});
			for (const KeysGeo::FHump& Hm : KeysGeo::Humps()) if (Hm.bPatch) All.Add({Hm.Name, Hm.X, KeysGeo::ChainZ(Hm.X) + Hm.DZ});
			D->CensusQ.Reset(); D->CensusBy.Reset();
			for (const FNamed& N : All)
			{
				if (D->CensusBy.Contains(N.Name)) continue;
				D->CensusBy.Add(N.Name).SetNumZeroed(NumRes + 1);
				const int32 CX = FMath::FloorToInt32(N.X / ChunkM), CZ = FMath::FloorToInt32(N.Z / ChunkM);
				for (int32 dz = -2; dz <= 1; dz++) for (int32 dx = -2; dx <= 1; dx++) D->CensusQ.Add({N.Name, CX + dx, CZ + dz});
			}
			UE_LOG(LogTemp, Log, TEXT("KRDen census started: %d reefs, %d chunks to deal"), D->CensusBy.Num(), D->CensusQ.Num());
		}
		else if (A[0] == TEXT("find") && A.Num() >= 2)
		{
			// find <kind | resident | any | resident> [how many] [within metres]
			const int32 N = A.Num() > 2 ? FCString::Atoi(*A[2]) : 3;
			const double R = A.Num() > 3 ? FCString::Atod(*A[3]) : 250;
			TArray<FDen> Hits;
			DensNear(D, At, R, Hits);
			int32 Got = 0;
			for (const FDen& DnR : Hits)
			{
				const FDen* Dn = &DnR;
				if (!Matches(*Dn, A[1])) continue;
				UE_LOG(LogTemp, Log, TEXT("KRDen find %s #%d: a %s (size %d) at %.1f, %.1f, %.1f in %.1f m of water, %s, %.0f m off; lives there: %s x%d%s; opens toward %.2f, %.2f"),
					*A[1], Got, KindName(Dn->Kind), Dn->Var, Dn->Mouth.X, Dn->Mouth.Y, Dn->Mouth.Z, Dn->Depth, HabName(Dn->Hab), FVector2D::Distance(FVector2D(Dn->Mouth.X, Dn->Mouth.Z), At),
					ResName(Dn->Res), Dn->Count, Dn->bBuddy ? TEXT(" (a grouper hunts with it)") : TEXT(""), Dn->Axis.X, Dn->Axis.Z);
				if (++Got >= N) break;
			}
			if (!Got) UE_LOG(LogTemp, Log, TEXT("KRDen find %s: none within %.0f m of %.0f, %.0f"), *A[1], R, At.X, At.Y);
		}
		else if (A[0] == TEXT("goto") && A.Num() >= 2)
		{
			// goto <kind | resident> [which] [metres off] [metres to the side] [metres up] [within]: the diver is held in
			// front of that den, looking at its opening (she must be in the water). goto off lets her go.
			if (A[1] == TEXT("off")) { D->bPin = false; return; }
			const int32 Which = A.Num() > 2 ? FCString::Atoi(*A[2]) : 0;
			const double Off = A.Num() > 3 ? FCString::Atod(*A[3]) : 1.5, Side = A.Num() > 4 ? FCString::Atod(*A[4]) : 0, Up = A.Num() > 5 ? FCString::Atod(*A[5]) : .15;
			const double R = A.Num() > 6 ? FCString::Atod(*A[6]) : 150;
			TArray<FDen> Hits;
			DensNear(D, At, R, Hits);
			int32 Got = 0;
			for (const FDen& DnR : Hits)
			{
				const FDen* Dn = &DnR;
				if (!Matches(*Dn, A[1]) || Got++ < Which) continue;
				// where its animal is now, if it has walked off; else the opening
				const FVector Look = D->Life ? LifeWhere(D->Life, *Dn) : Dn->Mouth;
				const FVector Ax = FVector(Dn->Axis.X, 0, Dn->Axis.Z).GetSafeNormal(.0001, FVector(0, 0, 1));
				FVector P = Look + Ax * Off + Dn->Right * Side + FVector(0, Up, 0);
				P.Y = FMath::Max(P.Y, FloorNear(P.X, P.Z) + .45);
				const FVector To = (Look - P).GetSafeNormal();
				D->bPin = true; D->bPinNew = true; D->PinP = P;
				// (the diver's forward is (-sin yaw cos pitch, sin pitch, -cos yaw cos pitch))
				D->PinYaw = FMath::Atan2(-To.X, -To.Z); D->PinPitch = FMath::Asin(FMath::Clamp(To.Y, -1.0, 1.0));
				UE_LOG(LogTemp, Log, TEXT("KRDen goto %s #%d: a %s at %.1f, %.1f, %.1f (%s, %.1f m of water, %s), lives there: %s x%d; the diver held %.1f m off at %.1f, %.1f, %.1f"),
					*A[1], Which, KindName(Dn->Kind), Dn->Mouth.X, Dn->Mouth.Y, Dn->Mouth.Z, *ReefNameAt(Dn->Mouth.X, Dn->Mouth.Z), Dn->Depth, HabName(Dn->Hab), ResName(Dn->Res), Dn->Count, Off, P.X, P.Y, P.Z);
				return;
			}
			UE_LOG(LogTemp, Log, TEXT("KRDen goto %s #%d: none within %.0f m of %.0f, %.0f"), *A[1], Which, R, At.X, At.Y);
		}
		else if (A[0] == TEXT("hide")) { D->bHide = A.Num() < 2 || FCString::Atoi(*A[1]) != 0; LayOut(D); UE_LOG(LogTemp, Log, TEXT("KRDen hide %d"), D->bHide ? 1 : 0); }
		else if (A[0] == TEXT("calm")) { D->bCalm = A.Num() < 2 || FCString::Atoi(*A[1]) != 0; UE_LOG(LogTemp, Log, TEXT("KRDen calm %d (the animals %s the diver)"), D->bCalm ? 1 : 0, D->bCalm ? TEXT("take no notice of") : TEXT("mind")); }
		else if (A[0] == TEXT("solid"))
		{
			// the hooks the hub is asked to call (R1), tried from here: push a body out of the nearest rock, and a ray at it
			TArray<FDen> Hits;
			DensNear(D, At, 60, Hits);
			for (const FDen& DnR : Hits)
			{
				const FDen* Dn = &DnR;
				if (Dn->Kind == GapKind) continue;
				double X = Dn->C.X, Y = Dn->C.Y + .1, Z = Dn->C.Z;
				const bool bWas = PushOut(X, Y, Z, .35);
				const FVector From = Dn->Mouth + Dn->Axis * 4;
				const double T = RayHit(From, -Dn->Axis, 12);
				UE_LOG(LogTemp, Log, TEXT("KRDen solid: a body put in the middle of the %s at %.1f, %.1f was %s, to %.2f m from its middle (the rock is %.2f x %.2f x %.2f m); a shaft fired at its opening from 4 m stops after %.2f m of 12"),
					KindName(Dn->Kind), Dn->C.X, Dn->C.Z, bWas ? TEXT("pushed out") : TEXT("left there (not among the rocks near the camera)"), FVector::Dist(FVector(X, Y, Z), Dn->C), Dn->Rad.X * 2, Dn->Rad.Y * 2, Dn->Rad.Z * 2, T);
				break;
			}
		}
		else if (D->Life && LifeDebug(D->Life, W, A)) {}
		else UE_LOG(LogTemp, Log, TEXT("KRDen: stats | census | find <what> [n] [within] | goto <what> [which] [off] [side] [up] | goto off | hide 0/1 | calm 0/1 | solid | hunt | who   (what: a kind, a resident's key, any, resident, buddy)"));
	}

	static FAutoConsoleCommand GDenCmd(TEXT("kr.den"), TEXT("Reef den test hooks: stats, census, find <what>, goto <what>, hide, calm, solid, hunt, who"),
		FConsoleCommandWithArgsDelegate::CreateLambda([](const TArray<FString>& Args) { DebugCmd(GDens, FString::Join(Args, TEXT(" "))); }));
}
