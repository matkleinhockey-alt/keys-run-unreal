// Queen conch, Aliger gigas (long known as Strombus gigas, then Lobatus gigas). A bottom animal of the reef-life
// framework. Everything about the animal is in this file.
//
// Reference (what the mesh, the paint and the numbers below are held to):
//   Adult shell 20-30 cm long: a heavy shell with a short conical spire of about eight whorls, each carrying a row of
//     blunt knobs (the largest on the shoulder of the last whorl), a big swollen body whorl, and in the adult a
//     broad, thick lip flared out from the right side like a wing. Outside sandy tan to buff, chalky, usually
//     dulled by a thin brown skin and a film of algae and sand; the knob tips worn paler. The aperture and the
//     inside and edge of the flared lip glossy pink, deepening to rose inside.
//   The animal: a mottled grey-brown foot ending in a brown, sickle-shaped operculum; two long eye stalks, each with
//     a ringed yellow eye at its tip, that it pokes out from under the front of the shell (the right one through a
//     notch in the lip); a long snout between them. The stalks are drawn in when it is disturbed.
//   It does not glide like other snails: it digs the operculum in and throws the shell forward in a lurching hop of
//     about half a shell length, lifting the front of the shell as it goes.
//   Sand and seagrass (turtle grass) on the ocean side of the Keys: the back reef, Hawk Channel's grass and the
//     sand beside the reef, mostly 1-15 m, in loose scatters. Grazes algae and detritus, day and night.
//   Protected: taking queen conch has been prohibited in Florida since 1985-86. It is not in the catch tables and
//     nothing in the game can pick it up or spear it; the first sight says so.
//
// Needs the bottom-animal hooks of the framework (request reefbottom R1: FSpecies::Mesh, Pose, Where, bNoBolt,
// bOnFloor). Without them this file compiles to nothing and no conch is registered: a fish-shaped stand-in would not
// be a queen conch.
#include "KeysReefLife.h"
#include "KeysMesh.h"
#include "Misc/CommandLine.h"
#include "KeysGeo.h"

#if defined(KEYS_REEFLIFE_BOTTOM_HOOKS)
namespace KeysReefLifeBottomConch
{
	using namespace KeysReefLife;

	static double Sm(double T) { T = FMath::Clamp(T, 0.0, 1.0); return T * T * (3 - 2 * T); }
	static FLinearColor Mix(const FLinearColor& A, const FLinearColor& B, double T) { return FMath::Lerp(A, B, float(FMath::Clamp(T, 0.0, 1.0))); }
	static double Hash(int32 I, int32 J) { const double H = FMath::Sin(I * 12.9898 + J * 78.233) * 43758.5453; return H - FMath::FloorToDouble(H); }

	// a surface through NR rows of NA points (P[r * NA + a]); bWrap closes each row into a ring. Normals come from the
	// grid and are turned to point away from the row's middle, or along Hint when one is given (an open sheet).
	static void AddSurf(FKeysMesh& M, const TArray<FVector>& P, const TArray<FLinearColor>& C, int32 NR, int32 NA, bool bWrap, const FVector* Hint)
	{
		const int32 I0 = M.V.Num();
		for (int32 r = 0; r < NR; r++)
		{
			FVector Ctr = FVector::ZeroVector;
			for (int32 a = 0; a < NA; a++) Ctr += P[r * NA + a];
			Ctr /= NA;
			for (int32 a = 0; a < NA; a++)
			{
				const int32 A0 = bWrap ? (a + NA - 1) % NA : FMath::Max(a - 1, 0), A1 = bWrap ? (a + 1) % NA : FMath::Min(a + 1, NA - 1);
				const int32 R0 = FMath::Max(r - 1, 0), R1 = FMath::Min(r + 1, NR - 1);
				FVector N = FVector::CrossProduct(P[r * NA + A1] - P[r * NA + A0], P[R1 * NA + a] - P[R0 * NA + a]).GetSafeNormal();
				const FVector Out = Hint ? *Hint : P[r * NA + a] - Ctr;
				if (N.IsNearlyZero()) N = Out.GetSafeNormal();
				if (N.IsNearlyZero()) N = FVector(0, 0, 1);
				if (FVector::DotProduct(N, Out) < 0) N = -N;
				M.AddVert(P[r * NA + a], N, C[r * NA + a]);
			}
		}
		const int32 NQ = bWrap ? NA : NA - 1;
		for (int32 r = 0; r + 1 < NR; r++)
			for (int32 a = 0; a < NQ; a++)
			{
				const int32 A = I0 + r * NA + a, B = I0 + r * NA + (a + 1) % NA, Cq = I0 + (r + 1) * NA + (a + 1) % NA, D = I0 + (r + 1) * NA + a;
				M.T.Append({A, B, Cq, A, Cq, D});
			}
	}

	// the shell's radius along its axis: U 0 at the front tip (the siphonal canal) .. 1 at the apex of the spire
	static double ShellR(double U)
	{
		static const double Us[] = {0, .08, .2, .35, .5, .6, .66, .72, .8, .88, .95, 1};
		static const double Rs[] = {.02, .07, .15, .22, .26, .27, .21, .165, .12, .072, .03, .006};
		for (int32 i = 0; i + 1 < UE_ARRAY_COUNT(Us); i++)
			if (U <= Us[i + 1]) return FMath::Lerp(Rs[i], Rs[i + 1], (U - Us[i]) / (Us[i + 1] - Us[i]));
		return Rs[UE_ARRAY_COUNT(Rs) - 1];
	}
	// where the shell's surface is: Th 0 to the right (+Y), 90 degrees straight up. The underside is flattened: it
	// lies on its aperture.
	static FVector ShellPt(double U, double Th, double Knob)
	{
		const double R = ShellR(U) * (1 + Knob), S = FMath::Sin(Th);
		return FVector(.5 - U, R * FMath::Cos(Th), R * S * (S < 0 ? .5 : 1.0));
	}

	static const FLinearColor& SandA() { static const FLinearColor C = Hex(0xd8c7a0); return C; }
	static const FLinearColor& SandB() { static const FLinearColor C = Hex(0xb99f78); return C; }
	static const FLinearColor& Film() { static const FLinearColor C = Hex(0x9c8763); return C; }      // the skin of algae and sand
	static const FLinearColor& Worn() { static const FLinearColor C = Hex(0xeadfc8); return C; }      // knob tips, the spire
	static const FLinearColor& Pink() { static const FLinearColor C = Hex(0xf2909f); return C; }
	static const FLinearColor& Rose() { static const FLinearColor C = Hex(0xe0597a); return C; }
	static const FLinearColor& Peach() { static const FLinearColor C = Hex(0xf3b9a6); return C; }

	static FLinearColor ShellCol(double U, double Th, double Knob01, int32 I, int32 J, int32 Lod)
	{
		const double S = FMath::Sin(Th), Cs = FMath::Cos(Th);
		// underneath: cream, and pink toward the aperture on the right
		if (S < -.25) return Mix(Hex(0xe9cdb6), Pink(), Sm(Cs * 1.4));
		FLinearColor C = Lod == 0 ? Mix(SandA(), SandB(), Hash(I, J)) : Mix(SandA(), SandB(), .5);
		C = Mix(C, Film(), .4 * (.5 + .5 * FMath::Sin(U * 23 + Th * 2)) * Sm((S + .1) / .5));
		C = Mix(C, Worn(), .22 * Sm((U - .66) / .15));
		C = Mix(C, Worn(), .65 * Knob01);
		return C;
	}

	static void BuildShell(FKeysMesh& M, int32 Lod)
	{
		static const double U0[] = {0, .08, .2, .28, .35, .43, .5, .6, .66, .72, .8, .88, .95, 1};
		static const double U1[] = {0, .12, .3, .5, .6, .66, .72, .8, .9, 1};
		static const double U2[] = {0, .2, .55, .68, 1};
		const double* Us = Lod == 0 ? U0 : Lod == 1 ? U1 : U2;
		const int32 NR = Lod == 0 ? UE_ARRAY_COUNT(U0) : Lod == 1 ? UE_ARRAY_COUNT(U1) : UE_ARRAY_COUNT(U2);
		const int32 NA = Lod == 0 ? 16 : Lod == 1 ? 8 : 5;
		M.Surf = SurfDefault;
		TArray<FVector> P;
		TArray<FLinearColor> C;
		for (int32 r = 0; r < NR; r++)
			for (int32 a = 0; a < NA; a++)
			{
				const double U = Us[r], Th = 2 * PI * a / NA;
				// the knobs: a row on the shoulder of the body whorl, smaller rows up the spire, on the back only.
				// (Every other point round is a knob, the rows staggered as the whorls turn.)
				double K = 0;
				if (Lod < 2)
				{
					const double Back = Sm((FMath::Sin(Th) + .15) / .4);
					const bool bEven = (a & 1) == 0;   // (near, eight knobs a whorl; mid, four)
					if (FMath::Abs(U - .6) < .01 && bEven) K = .24 * Back;
					else if (FMath::Abs(U - .72) < .01 && !bEven) K = .17 * Back;
					else if (FMath::Abs(U - .8) < .01 && bEven) K = .11 * Back;
				}
				P.Add(ShellPt(U, Th, K));
				C.Add(ShellCol(U, Th, K > 0 ? 1 : 0, r, a, Lod));
			}
		AddSurf(M, P, C, NR, NA, true, nullptr);

		// ---- the flared lip: a thick wing out from the right side, sandy above, glossy pink beneath and along its edge
		static const double LU[] = {.07, .15, .25, .35, .45, .55, .62, .67, .71};
		static const double LW[] = {0, .13, .22, .28, .31, .32, .29, .18, 0};
		static const double LV0[] = {0, .35, .7, .9, 1};
		static const double LV1[] = {0, .6, 1};
		const int32 NL = Lod == 2 ? 5 : 9, LStep = Lod == 2 ? 2 : 1;
		const double* Vs = Lod == 0 ? LV0 : LV1;
		const int32 NV = Lod == 0 ? UE_ARRAY_COUNT(LV0) : UE_ARRAY_COUNT(LV1);
		TArray<FVector> Top, Bot;
		TArray<FLinearColor> TopC, BotC;
		for (int32 i = 0; i < NL; i++)
		{
			const double U = LU[i * LStep], W = LW[i * LStep];
			const FVector B = ShellPt(U, FMath::DegreesToRadians(40.0), 0);
			for (int32 k = 0; k < NV; k++)
			{
				const double V = Vs[k];
				FVector Q(B.X + .05 * V * (U - .4), B.Y + W * V, B.Z + (-.02 - B.Z) * FMath::Pow(V, 1.5) + .035 * Sm((V - .75) / .25) * (W / .32));
				Top.Add(Q);
				FLinearColor Tc = Mix(Mix(SandA(), SandB(), Lod == 0 ? Hash(i + 40, k) : .5), Peach(), Sm((V - .45) / .4));
				if (V >= .9) Tc = Mix(Tc, Pink(), .75);
				TopC.Add(Tc);
				Bot.Add(Q - FVector(0, V < .01 ? .03 : 0, .016));
				BotC.Add(Mix(Rose(), Pink(), V));
			}
		}
		const FVector Up(0, .25, 1), Down(0, 0, -1), Side(0, 1, 0);
		AddSurf(M, Top, TopC, NL, NV, false, &Up);
		M.Surf = SurfGelcoat;
		AddSurf(M, Bot, BotC, NL, NV, false, &Down);
		if (Lod < 2)
		{
			// the lip's own thick edge
			TArray<FVector> Rim;
			TArray<FLinearColor> RimC;
			for (int32 i = 0; i < NL; i++) { Rim.Add(Top[i * NV + NV - 1]); Rim.Add(Bot[i * NV + NV - 1]); RimC.Add(Peach()); RimC.Add(Pink()); }
			AddSurf(M, Rim, RimC, NL, 2, false, &Side);
		}
		if (Lod == 0)
		{
			// the foot, under the shell: below the sand while the animal lies still, seen when a hop lifts the shell
			M.Surf = SurfSkin;
			TArray<FVector> Fp;
			TArray<FLinearColor> Fc;
			static const double FX[] = {.3, .12, -.08}, FR[] = {.035, .075, .05};
			for (int32 r = 0; r < 3; r++)
				for (int32 a = 0; a < 6; a++)
				{
					const double Th = 2 * PI * a / 6;
					Fp.Add(FVector(FX[r] + (r == 2 ? .0 : .02) * FMath::Sin(Th), .05 + FR[r] * FMath::Cos(Th), -.2 + FR[r] * .9 * FMath::Sin(Th)));
					Fc.Add(r == 2 ? Hex(0x5b4630) : Mix(Hex(0x9a8c74), Hex(0x6f6452), Hash(r + 7, a)));   // the brown operculum at its end
				}
			AddSurf(M, Fp, Fc, 3, 6, true, nullptr);
		}
		M.Surf = 0;
	}

	// one eye stalk: root at the origin, pointing along +X, 0.22 of a shell length; the ringed yellow eye at its tip
	static void BuildStalk(FKeysMesh& M)
	{
		M.Surf = SurfSkin;
		static const double X[] = {0, .09, .185, .2, .214, .226}, R[] = {.017, .014, .012, .02, .021, .012};
		static const uint32 Col[] = {0x8a7a5e, 0xb3a384, 0x8a7a5e, 0xe9c33a, 0xf0d24a, 0x101010};
		TArray<FVector> P;
		TArray<FLinearColor> C;
		for (int32 r = 0; r < 6; r++)
			for (int32 a = 0; a < 5; a++)
			{
				const double Th = 2 * PI * a / 5;
				P.Add(FVector(X[r], R[r] * FMath::Cos(Th), R[r] * FMath::Sin(Th)));
				C.Add(Hex(Col[r]));
			}
		AddSurf(M, P, C, 6, 5, true, nullptr);
		// the pupil closes the tip
		M.Surf = SurfEye;
		const int32 Tip = M.AddVert(FVector(.232, 0, 0), FVector(1, 0, 0), FLinearColor(.004f, .004f, .004f));
		const int32 Ring = Tip - 5;
		for (int32 a = 0; a < 5; a++) M.T.Append({Tip, Ring + a, Ring + (a + 1) % 5});
		M.Surf = 0;
	}

	static void PoseConch(const FPoseIn& In, FPoseOut& Out)
	{
		// a shell does not bend
		Out.Data[3] = 0; Out.Data[4] = 0;
		FVector F(In.Fwd.X, In.Fwd.Z, 0);
		if (!F.Normalize()) F = FVector(1, 0, 0);
		// the hop: each 1.15 s the shell is thrown forward and up in the first four tenths, then lies while the foot
		// is brought under it again (the framework moves it at a steady speed; this turns that into lurches)
		double Ahead = 0, Lift = 0, Pitch = 0;
		if (In.bHop)
		{
			const double Per = 1.15, Cy = FMath::Frac(In.StateT / Per);
			const double Thrown = Cy < .4 ? Sm(Cy / .4) : 1.0, Arc = Cy < .4 ? FMath::Sin(PI * Cy / .4) : 0.0;
			Ahead = In.Speed * Per * (Thrown - Cy);
			Lift = .2 * In.Len * Arc;
			Pitch = 15 * Arc;
		}
		const FQuat Rot = FRotationMatrix::MakeFromX(F).ToQuat() * FRotator(Pitch, 0, 0).Quaternion();
		const FVector Loc = FVector(In.P.X, In.P.Z, In.P.Y + Lift) * 100.0 + F * (Ahead * 100.0);
		Out.Xf = FTransform(Rot, Loc, FVector(In.Len));
		// the eye stalks: out when nothing is near, drawn in under the shell as the diver comes inside a metre and a half
		const double E = Sm((In.DiverDist - .7) / .8);
		for (int32 k = 0; k < 2; k++)
		{
			const double Sd = k ? -1 : 1, Wob = 6 * FMath::Sin(In.Now * .6 + k * 2.1 + In.Seed * 20);
			const FVector At = (k ? FVector(.4, -.005, -.085) : FVector(.33, .1, -.075)) - FVector(.1 * (1 - E), 0, 0);
			Out.Part[k] = FTransform(FRotator(28 + Wob * .5, Sd * 22 + Wob, 0), At * 100.0, FVector(E > .03 ? E : .001)) * Out.Xf;
		}
	}

	static FSpecies MakeQueenConch()
	{
		FSpecies S;
		if (FParse::Param(FCommandLine::Get(), TEXT("KRNoReefBottom"))) return S;   // a test switch: the same build without this lane's animals (the registry logs it as left out)
		S.Key = TEXT("queenconch");
		S.Common = TEXT("Queen conch");
		S.Scientific = TEXT("Aliger gigas");
		S.Role = ERole::Sitter;

		FBody B;
		B.HalfH = .08;    // (a Sitter lies HalfH above the floor: the shell's flattened underside is 0.135 below its axis, so it is bedded a little into the sand)
		B.HalfW = .3;
		S.Phases = {FPhase{TEXT("adult"), .20, .30, B}};
		S.Paint = [](int32, const FPaint&) { return SandA(); };   // (the registry asks for one; the mesh below carries its own colours)
		S.Mesh = [](int32, int32 Kind, FKeysMesh& M) { if (Kind == 3) BuildStalk(M); else BuildShell(M, Kind); return true; };
		S.Pose = &PoseConch;

		// loose scatters of three to twelve
		FMix Scatter; Scatter.A = 0; Scatter.AMin = 3; Scatter.AMax = 12;
		S.Mixes = {Scatter};
		S.Depth[0] = 1; S.Depth[1] = 2; S.Depth[2] = 10; S.Depth[3] = 15;
		S.Zones = ZSand | ZGrass;
		S.Where = [](double X, double Z, const FHabitat& H)
		{
			// the ocean side of the chain only (not Florida Bay), in to the fore reef's sand and no further; grass
			// anywhere there, bare sand mostly beside the reef
			const double DZ = KeysGeo::DZAt(X, Z);
			return Sm((DZ - 120) / 200) * (1 - Sm((DZ - 1900) / 300)) * (H.Zone == ZGrass ? 1.0 : DZ > 1050 ? .8 : .3);
		};
		S.Habit = EHabit::Both;
		S.GroupsPerHa = 2.2; S.MaxAlive = 36; S.Weight = 1;
		S.Shy = 0;                      // it does not swim off: it draws its eyes in (PoseConch)
		S.Cruise = 0; S.Burst = .24;    // a hop moves it at 0.12 m/s
		S.Roam = 1.2; S.Alt[0] = 0; S.Alt[1] = 0; S.Spacing = 9; S.Cull = 45;
		S.bRows = true;                 // the two eye stalks are the framework's two near "fin" instances
		S.bNoBolt = true; S.bOnFloor = true;
		S.BiteEvery = 70;               // a Sitter's BiteEvery is how often it shifts: a hop about every 70 s
		S.FirstSight = TEXT("Queen conch (Aliger gigas). Protected in Florida: look, don't take.");
		return S;
	}

	static KeysReefLife::FAuto GQueenConch(&MakeQueenConch);
}
#endif
