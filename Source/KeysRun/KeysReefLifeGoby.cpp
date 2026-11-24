// Neon goby, Elacatinus oceanops: the reef's cleaner (reeflinks, criterion 3). A Florida Keys fish above all others:
// its range is the Florida reef tract, the Bahamas and the Gulf.
//
// Reference:
//   Adult, 3-5 cm: a slim, round-bodied little fish (depth about a sixth of the length), blunt-headed with the
//     mouth underneath. Back black; an electric-blue stripe from the snout through the top of the eye to the tail
//     fin; under it a broad black band from the eye to the tail; belly pale grey-white. Dorsal fins (two, the second
//     longer) and anal fin dusky, nearly clear; tail rounded, dusky, the blue stripe running into it; pectorals clear.
//     Eye dark with a pale ring.
//   Lives on the tops of brain and boulder star coral heads, 1 to 40 m, in twos to a handful: a cleaning station.
//     Perches still on the coral, darting; when a fish comes and holds still it swims out onto it and picks it over
//     - flanks, fins, gill covers - and goes back to its head. By day only; at night it shelters in the coral.
#include "KeysReefLife.h"

namespace KeysReefLifeGoby
{
	using namespace KeysReefLife;

	static FLinearColor PaintGoby(int32 Phase, const FPaint& At)
	{
		static const FLinearColor Black = Hex(0x101216), Blue = Hex(0x29c4ff), Belly = Hex(0xc8ccd0), Dusk = Hex(0x343a44), Clear = Hex(0x8a929c);
		switch (At.Part)
		{
		case PBody:
		{
			// (the stripe is wide enough for the near grid's 16 round: 0.16 of the side)
			const double T = At.Lod == 0 ? 0 : .05;
			if (At.V > .2 - T && At.V < .5 + T) return Blue;
			if (At.V < -.45) return Belly;
			return Black;
		}
		case PTail: return FMath::Abs(At.V) < .25 ? Blue : Dusk;
		case PDorsal: case PAnal: return Dusk;
		case PPectoral: case PPelvic: return Clear;
		case PEye: return At.U < .65 ? FLinearColor(.01f, .01f, .012f) : Hex(0xb8c2c8);
		default: return Black;
		}
	}

	static FSpecies MakeNeonGoby()
	{
		FSpecies S;
		S.Key = TEXT("neongoby");
		S.Common = TEXT("Neon goby");
		S.Scientific = TEXT("Elacatinus oceanops");
		S.Role = ERole::Sitter;

		FBody B;
		B.HalfH = .08; B.HalfW = .07; B.Peak = .3; B.Blunt = .65; B.NoseR = .35; B.Ped = .55; B.PedW = .5; B.Arch = .03;
		B.TailLen = .17; B.TailH = .3; B.TailFork = -.35;
		B.Dor0 = .32; B.Dor1 = .86; B.DorH = .05; B.An0 = .55; B.An1 = .86; B.AnH = .04;
		B.PecU = .26; B.PecV = -.2; B.PecLen = .13; B.PecW = .07; B.PelU = .28; B.PelLen = .06;
		B.EyeU = .1; B.EyeV = .45; B.EyeR = .03;
		S.Phases = {FPhase{TEXT("adult"), .03, .05, B}};
		S.Paint = &PaintGoby;

		FMix Station; Station.Weight = 1; Station.A = 0; Station.AMin = 2; Station.AMax = 5;
		S.Mixes = {Station};

		S.Depth[0] = 1; S.Depth[1] = 2.5; S.Depth[2] = 25; S.Depth[3] = 40;
		S.Zones = ZTop | ZSpur | ZPatch | ZFore;
		S.Habit = EHabit::Day;
		S.GroupsPerHa = 5; S.MaxAlive = 30; S.Weight = .25;
		S.Shy = .25;                       // it keeps to its coral and only darts aside
		S.Cruise = .12; S.Burst = .9; S.Roam = .3; S.Alt[0] = 0; S.Alt[1] = 0; S.Spacing = 3; S.Cull = 18;
		S.BiteEvery = 9;                   // (a Sitter's BiteEvery: how often it darts to a new perch on its head)
		S.bHomeOnHead = true;              // its home is the top of the nearest stony head: the station
		S.bCleaner = true;
		S.FirstSight = TEXT("Neon gobies (Elacatinus oceanops) - a cleaning station: reef fish come here to be picked over.");
		return S;
	}

	static KeysReefLife::FAuto GNeonGoby(&MakeNeonGoby);
}
