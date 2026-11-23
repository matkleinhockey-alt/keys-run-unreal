// Giant Caribbean anemone, Condylactis gigantea. A bottom animal of the reef-life framework that never moves from
// its place. Everything about the animal is in this file.
//
// Reference (what the mesh and the numbers below are held to):
//   The crown of tentacles 15-30 cm across. Tentacles long, thick and tapering, about a hundred in life, white to
//     cream, pale green or pinkish, each ending in a swollen tip that is purple, magenta or pink (sometimes plain).
//     The column is short and thick, orange-buff to pale, and is tucked into a crevice or under the edge of a coral
//     head so that only the crown shows.
//   Lives singly, attached to rock in crevices and at the base of coral heads on the reef and its patch reefs,
//     1-15 m. The tentacles sway with the surge. Touched or crowded, it draws them in and shortens.
//   No fish live in it in the Keys: there are no anemonefish in the Atlantic. (Its real lodgers are small cleaner
//     shrimps and a few juvenile fish; none are made here.)
//
// How it sways: the mesh is built "tail up" (its base at +X, the tentacle tips at -X) and stood on end by Pose, so
// the school-fish material's tail wave, which grows toward -X and swings across local Y, becomes a sway of the
// tentacles; Pose turns local Y along the surge's own axis and sets the wave to the surge's period and phase
// (KeysSea::SurgeAt), so an anemone swings with the sea fans beside it.
//
// Needs the bottom-animal hooks of the framework (request reefbottom R1: FSpecies::Mesh, Pose, bNoBolt, bOnFloor,
// HeadWithin). Without them this file compiles to nothing and no anemone is registered.
#include "KeysReefLife.h"
#include "KeysMesh.h"
#include "Misc/CommandLine.h"
#include "KeysSea.h"

#if defined(KEYS_REEFLIFE_BOTTOM_HOOKS)
namespace KeysReefLifeBottomAnemone
{
	using namespace KeysReefLife;

	static double Sm(double T) { T = FMath::Clamp(T, 0.0, 1.0); return T * T * (3 - 2 * T); }
	static FLinearColor Mix(const FLinearColor& A, const FLinearColor& B, double T) { return FMath::Lerp(A, B, float(FMath::Clamp(T, 0.0, 1.0))); }
	static double Hash(int32 I, int32 J) { const double H = FMath::Sin(I * 12.9898 + J * 78.233) * 43758.5453; return H - FMath::FloorToDouble(H); }

	constexpr double BaseX = .33;    // the column's foot, in crown diameters below the mesh's origin
	constexpr double DiscX = .07;    // the oral disc the tentacles rise from

	struct FMorph { uint32 Body, Root, Tip; };
	// white with purple tips, pale green with purple tips, pink with magenta tips
	static const FMorph Morphs[3] = {{0xece8dc, 0xcfc4ae, 0x9a4fc0}, {0xc9dba8, 0xa9b98a, 0xa04cb8}, {0xeec3c6, 0xd9a3a0, 0xe04c9a}};

	// rings of NA points along a centre line (Ctr[r], radius Rad[r]), closed round; normals point away from the line
	static void AddTube(FKeysMesh& M, const TArray<FVector>& Ctr, const TArray<double>& Rad, const TArray<FLinearColor>& Col, int32 NA, const FVector& SideHint)
	{
		const int32 NR = Ctr.Num(), I0 = M.V.Num();
		for (int32 r = 0; r < NR; r++)
		{
			FVector D = (Ctr[FMath::Min(r + 1, NR - 1)] - Ctr[FMath::Max(r - 1, 0)]).GetSafeNormal();
			if (D.IsNearlyZero()) D = FVector(-1, 0, 0);
			FVector E1 = FVector::CrossProduct(D, SideHint).GetSafeNormal();
			if (E1.IsNearlyZero()) E1 = FVector::CrossProduct(D, FVector(0, 0, 1)).GetSafeNormal();
			const FVector E2 = FVector::CrossProduct(D, E1);
			for (int32 a = 0; a < NA; a++)
			{
				const double Th = 2 * PI * a / NA;
				const FVector N = E1 * FMath::Cos(Th) + E2 * FMath::Sin(Th);
				M.AddVert(Ctr[r] + N * Rad[r], N, Col[r]);
			}
		}
		for (int32 r = 0; r + 1 < NR; r++)
			for (int32 a = 0; a < NA; a++)
			{
				const int32 A = I0 + r * NA + a, B = I0 + r * NA + (a + 1) % NA, Cq = I0 + (r + 1) * NA + (a + 1) % NA, D = I0 + (r + 1) * NA + a;
				M.T.Append({A, B, Cq, A, Cq, D});
			}
	}

	static void BuildAnemone(FKeysMesh& M, int32 Phase, int32 Lod)
	{
		const FMorph& Mo = Morphs[FMath::Clamp(Phase, 0, 2)];
		const FLinearColor Body = Hex(Mo.Body), Root = Hex(Mo.Root), Tip = Hex(Mo.Tip);
		M.Surf = SurfSkin;
		// ---- the column and the oral disc (up is -X)
		{
			const int32 NA = Lod == 0 ? 8 : 6;
			AddTube(M, {FVector(BaseX, 0, 0), FVector(.12, 0, 0), FVector(DiscX, 0, 0), FVector(DiscX - .005, 0, 0)}, {.1, .14, .16, .012},
				{Hex(0xb8683e), Hex(0xd98a5a), Mix(Hex(0xe6d9c4), Root, .4), Hex(0xc98f7a)}, NA, FVector(0, 0, 1));
		}
		// ---- the tentacles: three whorls, the inner ones upright, the outer ones spread, every one curving up to
		// its swollen tip
		static const int32 Count[3][3] = {{6, 9, 13}, {4, 6, 8}, {3, 0, 5}};
		static const double RootR[3] = {.04, .085, .13}, Lean[3] = {10, 34, 60}, Long[3] = {.52, .47, .42};
		static const double T0[] = {0, .45, .82, .94, 1}, R0[] = {.034, .028, .024, .04, .008};
		static const double T1[] = {0, .82, .94, 1}, R1[] = {.036, .026, .042, .008};
		static const double T2[] = {0, 1}, R2[] = {.04, .03};
		const double* Ts = Lod == 0 ? T0 : Lod == 1 ? T1 : T2;
		const double* Rs = Lod == 0 ? R0 : Lod == 1 ? R1 : R2;
		const int32 NT = Lod == 0 ? 5 : Lod == 1 ? 4 : 2, Sides = Lod == 0 ? 4 : 3;
		for (int32 w = 0; w < 3; w++)
			for (int32 i = 0; i < Count[Lod][w]; i++)
			{
				const int32 N = Count[Lod][w];
				const double Az = 2 * PI * (i + .5 * (w & 1)) / N + (Hash(w * 31 + i, 1) - .5) * .3;
				const double Ln = FMath::DegreesToRadians(Lean[w] + (Hash(w * 31 + i, 2) - .5) * 10), Len = Long[w] * (.9 + .2 * Hash(w * 31 + i, 3));
				const FVector Out(0, FMath::Cos(Az), FMath::Sin(Az));
				TArray<FVector> Ctr;
				TArray<double> Rad;
				TArray<FLinearColor> Col;
				FVector P = FVector(DiscX, 0, 0) + Out * RootR[w];
				double Tp = 0;
				for (int32 k = 0; k < NT; k++)
				{
					const double T = Ts[k];
					// (stepped along the curve: it leans out at the root and turns up toward the tip)
					for (int32 s = 0; s < 4; s++)
					{
						const double Tm = Tp + (T - Tp) * (s + .5) / 4, Ph = Ln - FMath::DegreesToRadians(16.0) * Tm;
						P += (Out * FMath::Sin(Ph) - FVector(FMath::Cos(Ph), 0, 0)) * (Len * (T - Tp) / 4);
					}
					Tp = T;
					Ctr.Add(P);
					Rad.Add(Rs[k]);
					Col.Add(Lod == 2 ? Mix(Body, Tip, T * .7) : T < .5 ? Mix(Root, Body, T / .45) : T < .9 ? Mix(Body, Tip, .25) : Tip);
				}
				AddTube(M, Ctr, Rad, Col, Sides, FVector::CrossProduct(Out, FVector(1, 0, 0)));
			}
		M.Surf = 0;
	}

	static void PoseAnemone(const FPoseIn& In, FPoseOut& Out)
	{
		const FVector2D N = KeysSea::ChainNormal(In.P.X);            // the surge runs in and out along this
		const double Decay = FMath::Exp(-FMath::Max(0.0, -In.P.Y) / KeysSea::SurgeDecayDepth);
		// drawn in as the diver's hand comes near: shorter, narrower, and stiller
		const double Shut = 1 - Sm((In.DiverDist - .35) / .6);
		const double SLen = 1 - .4 * Shut, SWide = 1 - .22 * Shut;
		// stood on end, leaning a little out from the coral head it is tucked against (the framework faces it away
		// from the head), local Y along the surge
		FVector UpU = FVector(0, 0, 1) + FVector(In.Fwd.X, In.Fwd.Z, 0) * .22;
		UpU.Normalize();
		const FQuat Rot = FRotationMatrix::MakeFromXY(-UpU, FVector(N.X, N.Y, 0)).ToQuat();
		// (the framework holds the origin HalfH = 0.30 crown diameters over the floor; shortened, the foot stays down)
		const FVector Loc = FVector(In.P.X, In.P.Z, In.P.Y) * 100.0 - UpU * (BaseX * (1 - SLen) * In.Len * 100.0);
		Out.Xf = FTransform(Rot, Loc, FVector(In.Len * SLen, In.Len * SWide, In.Len * SWide));
		// the tentacles follow the water a little behind it
		const double Ph = (In.Now / KeysSea::SurgePeriod) * 2 * PI - (In.P.X * N.X + In.P.Z * N.Y) * (2 * PI / 34) - 1.2 + In.Seed;
		Out.Data[0] = float(FMath::Fmod(FMath::Fmod(Ph, 2 * PI) + 2 * PI, 2 * PI));
		Out.Data[1] = float(2 * PI / KeysSea::SurgePeriod);
		Out.Data[2] = float(In.Now);
		Out.Data[3] = float(2.4 * Decay * (1 - .7 * Shut) * (.85 + .3 * In.Seed));
		Out.Data[4] = float((In.Seed - .5) * .3);                    // each leans its own way a little
	}

	static FSpecies MakeGiantAnemone()
	{
		FSpecies S;
		if (FParse::Param(FCommandLine::Get(), TEXT("KRNoReefBottom"))) return S;   // a test switch: the same build without this lane's animals (the registry logs it as left out)
		S.Key = TEXT("giantanemone");
		S.Common = TEXT("Giant Caribbean anemone");
		S.Scientific = TEXT("Condylactis gigantea");
		S.Role = ERole::Sitter;

		FBody B;
		B.HalfH = .3;     // (a Sitter is held HalfH above the floor: the column's foot, 0.33 below the origin, is just in the rock)
		B.HalfW = .12;
		// the three colours it comes in; Len is the crown's width
		S.Phases = {FPhase{TEXT("white"), .15, .30, B}, FPhase{TEXT("green"), .15, .30, B}, FPhase{TEXT("pink"), .15, .28, B}};
		S.Paint = [](int32 Phase, const FPaint&) { return Hex(Morphs[FMath::Clamp(Phase, 0, 2)].Body); };   // (the registry asks for one; the mesh carries its own colours)
		S.Mesh = [](int32 Phase, int32 Kind, FKeysMesh& M) { if (Kind > 2) return false; BuildAnemone(M, Phase, Kind); return true; };
		S.Pose = &PoseAnemone;

		// singly
		FMix White; White.Weight = .45; White.A = 0;
		FMix Green; Green.Weight = .3; Green.A = 1;
		FMix Pinkish; Pinkish.Weight = .25; Pinkish.A = 2;
		S.Mixes = {White, Green, Pinkish};
		S.Depth[0] = 1; S.Depth[1] = 2; S.Depth[2] = 12; S.Depth[3] = 15;
		S.Zones = ZCrest | ZTop | ZSpur | ZPatch | ZFore;
		S.Habit = EHabit::Both;
		S.GroupsPerHa = 14; S.MaxAlive = 24; S.Weight = 1;
		S.Shy = 0; S.Cruise = 0; S.Burst = 0; S.Roam = 0; S.Alt[0] = 0; S.Alt[1] = 0; S.Cull = 40;
		S.bNoBolt = true; S.bOnFloor = true;
		S.HeadWithin = 3;               // against the nearest stony coral head within 3 m, or not there at all
		S.BiteEvery = 1e7;              // (a Sitter's BiteEvery is how often it shifts: never)
		S.FirstSight = TEXT("Giant Caribbean anemone (Condylactis gigantea).");
		return S;
	}

	static KeysReefLife::FAuto GGiantAnemone(&MakeGiantAnemone);
}
#endif
