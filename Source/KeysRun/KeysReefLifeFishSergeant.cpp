// Sergeant major, Abudefduf saxatilis: the hovering groups, and the nest-guarding male (a second registry key,
// because a role belongs to a species entry and the two behave differently).
//
// Reference (what the paint and the numbers below are held to):
//   Adult, 12-18 cm: a deep oval damselfish (depth a little under half the length) with a forked tail. Silver-
//     white to pale bluish-grey, the upper back yellow, five black bars down the side that narrow toward the
//     belly (the first behind the gill cover, the last on the tail base); head grey with a darker snout; tail
//     and fins dusky grey. The commonest damselfish of the Keys.
//     Loose groups of 10 to 40 hover high in the water (1-3 m up) over coral heads, round wrecks and along
//     mooring lines, facing the current and picking plankton; when frightened the whole group drops to the
//     coral. Reef crest, reef top, spurs and patch reefs, 1 to 12 m. By day; in the reef at night.
//   Guarding male, 14-18 cm: while he guards eggs his body goes dark dusky blue, the bars hardly darker than
//     the rest and the yellow gone. The eggs are a purple-red patch a hand or two across on a cleared rock
//     face; he stays within a body length or two of it, fans it, and rushes any fish near his own size that
//     comes close, then goes straight back.
//   Not catchable and not a spear target (in none of KeysLife's or KeysData's tables).
#include "KeysReefLife.h"

namespace KeysReefLifeFishSergeant
{
	using namespace KeysReefLife;

	static FLinearColor Mix(const FLinearColor& A, const FLinearColor& B, double T) { return FMath::Lerp(A, B, float(FMath::Clamp(T, 0.0, 1.0))); }

	// is this place on one of the five bars? Up close the bars sit on body stations 8, 11, 14, 17 and 20; the
	// middle level of detail has 10 stations, enough for four; the far one gets the mean colour instead
	static bool OnBar(const FPaint& At)
	{
		if (At.V < -.6) return false;                       // they stop short of the belly
		if (At.Lod == 0)
		{
			static const double C[5] = {.249, .370, .503, .644, .791};
			for (const double U : C) if (FMath::Abs(At.U - U) < .02) return true;
			return false;
		}
		static const double C1[4] = {.218, .413, .633, .873};
		for (const double U : C1) if (FMath::Abs(At.U - U) < .03) return true;
		return false;
	}

	static FLinearColor PaintSergeant(int32 Guard, const FPaint& At)
	{
		static const FLinearColor Silver = Hex(0xdde2e2), White = Hex(0xf0f1ee), Yellow = Hex(0xe9cf24), Bar = Hex(0x0e0e10), HeadC = Hex(0x9aa3a8), Snout = Hex(0x4f565c), Fin = Hex(0x5c6166);
		static const FLinearColor GBlue = Hex(0x3b52a0), GBar = Hex(0x18234f), GBelly = Hex(0x5f71ad);
		const bool bG = Guard != 0;
		switch (At.Part)
		{
		case PBody:
		{
			if (At.Lod == 2)
			{
				// far off: the mean of bars and ground, the yellow back still showing
				if (bG) return Mix(GBlue, GBar, .3);
				return At.V > .4 && At.U > .2 ? Mix(Yellow, Bar, .35) : Mix(Silver, Bar, .3);
			}
			if (bG)
			{
				if (At.U >= .2 && OnBar(At)) return GBar;
				return Mix(GBelly, GBlue, (At.V + .9) / .6);
			}
			if (At.U < .2) return At.U < .07 && At.V > 0 ? Snout : Mix(Silver, HeadC, (At.V + .2) / .6);
			if (OnBar(At)) return At.Lod == 0 ? Bar : Mix(Bar, Silver, .35);   // (the coarser grid draws them wider: lighter to keep the mean)
			if (At.V > .35 && At.U < .86) return Yellow;                 // the yellow back between the bars
			return Mix(White, Silver, (At.V + .8) / .6);
		}
		case PDorsal: return bG ? GBar : (At.V > .5 ? Fin : Mix(Yellow, Fin, .5));
		case PTail: return bG ? GBar : Mix(Hex(0x8c9296), Fin, At.U);
		case PPectoral: return bG ? Hex(0x3a4a80) : Hex(0xb8bdbf);
		case PPelvic: case PAnal: return bG ? GBar : Fin;
		case PEye: return At.U < .6 ? FLinearColor(.004f, .004f, .004f) : (bG ? Hex(0x7f8ab0) : Hex(0xd8dcd8));
		default: return Silver;
		}
	}

	static FBody SergeantBody()
	{
		FBody B;
		B.HalfH = .21; B.HalfW = .065; B.Peak = .4; B.Blunt = .55; B.NoseR = .2; B.Ped = .3; B.PedW = .25; B.Arch = .06;
		B.TailLen = .2; B.TailH = .36; B.TailFork = .45;
		B.Dor0 = .22; B.Dor1 = .88; B.DorH = .09; B.An0 = .56; B.An1 = .88; B.AnH = .08;
		B.PecU = .28; B.PecV = -.15; B.PecLen = .2; B.PecW = .09; B.PelU = .32; B.PelLen = .11;
		B.EyeU = .11; B.EyeV = .3; B.EyeR = .028;
		return B;
	}

	static FSpecies MakeSergeant()
	{
		FSpecies S;
		S.Key = TEXT("sergeantmajor");
		S.Common = TEXT("Sergeant major");
		S.Scientific = TEXT("Abudefduf saxatilis");
		S.Role = ERole::Hoverer;
		S.Phases = {FPhase{TEXT("adult"), .12, .18, SergeantBody()}};
		S.Paint = [](int32, const FPaint& At) { return PaintSergeant(0, At); };
		FMix Group; Group.A = 0; Group.AMin = 10; Group.AMax = 40;
		S.Mixes = {Group};
		S.Depth[0] = .8; S.Depth[1] = 1.5; S.Depth[2] = 9; S.Depth[3] = 13;
		S.Zones = ZCrest | ZTop | ZSpur | ZPatch;
		S.Habit = EHabit::Day;
		S.GroupsPerHa = 3; S.MaxAlive = 100; S.Weight = 1;
		S.Shy = 2.2;
		S.Cruise = .35; S.Burst = 2.4; S.Roam = 6; S.Alt[0] = 1; S.Alt[1] = 3; S.Spacing = 8; S.Cull = 50;
		S.bRows = true;
		S.FirstSight = TEXT("Sergeant major (Abudefduf saxatilis): five black bars, like a sergeant's stripes.");
		return S;
	}

	static FSpecies MakeGuard()
	{
		FSpecies S;
		S.Key = TEXT("sergeantmajorguard");
		S.Common = TEXT("Sergeant major (male guarding eggs)");
		S.Scientific = TEXT("Abudefduf saxatilis");
		S.Role = ERole::Territory;
		S.Phases = {FPhase{TEXT("guard"), .14, .18, SergeantBody()}};
		S.Paint = [](int32, const FPaint& At) { return PaintSergeant(1, At); };
		FMix One; One.A = 0; One.AMin = 1; One.AMax = 1;
		S.Mixes = {One};
		S.Depth[0] = .8; S.Depth[1] = 1.5; S.Depth[2] = 9; S.Depth[3] = 13;
		S.Zones = ZCrest | ZTop | ZSpur | ZPatch;
		S.Habit = EHabit::Day;
		S.GroupsPerHa = 3; S.MaxAlive = 5; S.Weight = 1;
		S.Shy = .9;                         // he does not leave his eggs for a diver
		S.Cruise = .3; S.Burst = 2.6; S.Roam = 1.5; S.Alt[0] = .1; S.Alt[1] = .3; S.Spacing = 2; S.Cull = 45;
		S.bRows = true;
		S.bHomeOnHead = true;               // the eggs are on a rock: he keeps station over a coral head
		S.ChaseR = 2.5;                     // the ground he keeps round the eggs
		S.FirstSight = TEXT("A sergeant major gone dark blue: a male guarding his eggs.");
		return S;
	}

	static KeysReefLife::FAuto GSergeant(&MakeSergeant);
	static KeysReefLife::FAuto GGuard(&MakeGuard);
}
