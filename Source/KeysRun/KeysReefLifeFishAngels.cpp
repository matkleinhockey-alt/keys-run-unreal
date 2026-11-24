// The three large angelfish of the Florida Keys reef tract: queen, French and gray. Role Pair (the framework's).
// The bodies are built in code; generated models of the queen and the French are on order (December 2026) and
// replace FPhase::Body / Paint only: nothing below the body and the paint needs to change for them.
//
// Reference (what the paint and the numbers below are held to):
//   All three: a deep, flat disc of a body (depth half the length or more), small mouth at the end of a short
//     snout, steep forehead, the soft dorsal and anal fins tall and drawn out behind into points that trail past
//     the tail base. They cruise slowly among sponges, sea fans and coral heads, stopping to nip at sponges,
//     and are not afraid of a slow diver (the gray often comes up to one). Active by day, wedged into the reef
//     at night.
//   Queen angelfish, Holacanthus ciliaris, 30-45 cm. Body electric blue to blue-green, every scale edged in
//     yellow, so the flank shimmers blue and yellow; face yellow with blue round the mouth, on the chin and
//     along the edge of the gill cover; on the forehead the "crown": a dark blue spot speckled and ringed with
//     electric blue; tail all yellow; pectoral and pelvic fins yellow; dorsal and anal fins blue-edged with
//     yellow-orange trailing tips. Usually alone, sometimes a pair. 5 to 25 m, seldom shallower than 3.
//   French angelfish, Pomacanthus paru, 30-40 cm. Black, every scale of the body with a bright yellow edge (a
//     yellow crescent on each); head slate-black without the edges; a yellow ring round the eye; mouth and chin
//     whitish; a yellow bar at the root of the pectoral fin; fins black, the dorsal's trailing filament yellow;
//     tail black and rounded. Nearly always a bonded pair that keeps together. 2 to 25 m.
//   Gray angelfish, Pomacanthus arcuatus, 35-50 cm (the largest of the three). Grey, each scale with a dark
//     centre and a pale edge; head a paler, even grey; mouth and chin white; tail square-cut with a pale
//     bluish-white margin; dorsal and anal fins dark grey with a thin pale edge; the inner face of the
//     pectoral fin yellow. Pairs, sometimes alone. 2 to 30 m.
//   None of the three is in KeysData (not catchable). KeysLife's reef table still holds a generic "angelfish"
//   (blue back, yellow belly: no real species); the request to retire it is in .marshal/status/reeffish.md.
#include "KeysReefLife.h"

namespace KeysReefLifeFishAngels
{
	using namespace KeysReefLife;

	static FLinearColor Mix(const FLinearColor& A, const FLinearColor& B, double T) { return FMath::Lerp(A, B, float(FMath::Clamp(T, 0.0, 1.0))); }
	static const FLinearColor Pupil(.004f, .004f, .004f);

	static FLinearColor PaintQueen(const FPaint& At)
	{
		static const FLinearColor Blue = Hex(0x2c7fe6), Elec = Hex(0x1f8bff), Navy = Hex(0x101c5c), Yellow = Hex(0xf2cf1c), YelO = Hex(0xf0a81e), Face = Hex(0xe6c93a);
		switch (At.Part)
		{
		case PBody:
		{
			// the crown, on the nape above and behind the eye: a dark blue spot ringed with electric blue (up close
			// the spot is body stations 7 and 8, the ring 6 and 9 and the rows above and below)
			if (At.V > .25 && At.U > (At.Lod == 0 ? .15 : .1) && At.U < (At.Lod == 0 ? .29 : .33)) return (At.V > .6 && At.V < .97 && At.U > .19 && At.U < .27) ? Navy : Elec;
			if (At.U < .05) return Blue;                                                  // round the mouth
			if (At.U < .22) return (At.V < -.55) ? Blue : Face;                           // yellow face, blue chin and throat
			if (FMath::Abs(At.U - .25) < (At.Lod == 0 ? .02 : .04) && At.V < .5) return Elec;   // the edge of the gill cover
			if (At.U > .94) return Yellow;                                                // the tail base goes yellow with the tail
			// blue scales edged in yellow; far off, their mean
			return At.Lod == 0 ? (((At.I + At.J) & 1) ? Blue : Yellow) : Mix(Blue, Yellow, .42);
		}
		case PDorsal: case PAnal:
		{
			if (At.U > .8) return YelO;                                                   // the trailing tips
			return At.V > .5 ? Blue : Mix(Blue, Yellow, .45);                        // the body's colour at the root, blue toward the edge
		}
		case PTail: case PPectoral: case PPelvic: return Yellow;
		case PEye: return At.U < .6 ? Pupil : Elec;
		default: return Blue;
		}
	}

	static FLinearColor PaintFrench(const FPaint& At)
	{
		static const FLinearColor Black = Hex(0x0f0f12), Slate = Hex(0x23242a), Yellow = Hex(0xf0cb26), Chin = Hex(0xd9d4b8);
		switch (At.Part)
		{
		case PBody:
		{
			if (At.U < .06) return Chin;                                                  // whitish mouth and chin
			if (At.U < .22) return Slate;                                                 // the head: no yellow edges
			if (FMath::Abs(At.U - .3) < (At.Lod == 0 ? .025 : .05) && At.V > -.5 && At.V < -.05) return Yellow;   // the bar at the pectoral root
			// a yellow crescent on the edge of each black scale: one grid point in four up close; far off, the mean
			if (At.Lod == 0) return ((At.I & 1) && (At.J & 1)) ? Yellow : Black;
			return Mix(Black, Yellow, .2);
		}
		case PDorsal: return (At.U > .82 && At.V > .5) ? Yellow : Black;                // the yellow trailing filament
		case PEye: return At.U < .5 ? Pupil : Yellow;                                    // the yellow eye ring
		case PPectoral: return Mix(Yellow, Black, At.U * 3);
		default: return Black;                                                           // anal, tail, pelvic
		}
	}

	static FLinearColor PaintGray(const FPaint& At)
	{
		static const FLinearColor Grey = Hex(0xa2a299), Dark = Hex(0x56564f), Head = Hex(0xaaa9a0), White = Hex(0xe6e3d8), FinD = Hex(0x55554f), Margin = Hex(0xcfd6da);
		switch (At.Part)
		{
		case PBody:
		{
			if (At.U < .06) return White;                                                 // the white mouth and chin
			if (At.U < .21) return Head;
			// dark-centred scales; far off, the mean grey
			return At.Lod == 0 ? (((At.I + At.J) & 1) ? Grey : Dark) : Mix(Grey, Dark, .5);
		}
		case PDorsal: case PAnal: return At.V > .7 ? Margin : FinD;
		case PTail: return At.U > .6 ? Margin : FinD;                                    // the square tail's pale margin
		case PPectoral: return Mix(Hex(0xb9a94a), Hex(0x8f8a62), At.U);                  // (its inner face is yellow)
		case PPelvic: return FinD;
		case PEye: return At.U < .6 ? Pupil : Hex(0x8d8d86);
		default: return Grey;
		}
	}

	static FBody AngelBody()
	{
		FBody B;
		B.HalfH = .26; B.HalfW = .055; B.Peak = .45; B.Blunt = .7; B.NoseR = .18; B.Ped = .2; B.PedW = .25; B.Arch = .06;
		B.TailLen = .16; B.TailH = .3; B.TailFork = -.2;
		// (the builder's fin is a hump: the tall soft fins are set as far back as it lets them go; see the request
		// for a trailing lobe in the status file)
		B.Dor0 = .24; B.Dor1 = 1; B.DorH = .17; B.An0 = .5; B.An1 = 1; B.AnH = .16;
		B.DorTrail = .2; B.AnTrail = .18;
		B.PecU = .31; B.PecV = -.25; B.PecLen = .16; B.PecW = .08; B.PelU = .36; B.PelLen = .12;
		B.EyeU = .13; B.EyeV = .35; B.EyeR = .022;
		return B;
	}

	static void Angel(FSpecies& S)
	{
		S.Role = ERole::Pair;
		S.Habit = EHabit::Day;
		S.Weight = 1;
		S.Cruise = .3; S.Burst = 2.2; S.Roam = 22; S.Alt[0] = .4; S.Alt[1] = 1.6; S.Spacing = 2.5; S.Cull = 60;
		S.bRows = true;
		// nips at sponges as it goes, from the side, at their walls; rock (algae, tunicates) only where no sponge is
		// in reach (reeflinks)
		S.bBitesRock = true; S.BiteEvery = 14;
		S.bSponges = true;
		S.bClient = true;                   // (reeflinks) visits the cleaning stations by day
	}

	static FSpecies MakeQueen()
	{
		FSpecies S;
		S.Key = TEXT("queenangel");
		S.Common = TEXT("Queen angelfish");
		S.Scientific = TEXT("Holacanthus ciliaris");
		Angel(S);
		FBody B = AngelBody();
		B.HalfH = .25;
		B.EyeU = .115; B.EyeV = .12;                          // (below the crown)
		S.Phases = {FPhase{TEXT("adult"), .30, .45, B}};
		S.Paint = [](int32, const FPaint& At) { return PaintQueen(At); };
		FMix Alone; Alone.Weight = .65; Alone.A = 0; Alone.AMin = 1; Alone.AMax = 1;
		FMix Two; Two.Weight = .35; Two.A = 0; Two.AMin = 2; Two.AMax = 2;
		S.Mixes = {Alone, Two};
		S.Depth[0] = 3; S.Depth[1] = 5; S.Depth[2] = 22; S.Depth[3] = 30;
		S.Zones = ZTop | ZSpur | ZFore | ZWall | ZPatch;
		S.GroupsPerHa = 4; S.MaxAlive = 8;
		S.Shy = 1.5;
		S.FirstSight = TEXT("Queen angelfish (Holacanthus ciliaris): the blue-ringed crown on its forehead gives it the name.");
		return S;
	}

	static FSpecies MakeFrench()
	{
		FSpecies S;
		S.Key = TEXT("frenchangel");
		S.Common = TEXT("French angelfish");
		S.Scientific = TEXT("Pomacanthus paru");
		Angel(S);
		FBody B = AngelBody();
		B.HalfH = .27; B.TailFork = -.25;
		S.Phases = {FPhase{TEXT("adult"), .30, .40, B}};
		S.Paint = [](int32, const FPaint& At) { return PaintFrench(At); };
		FMix Two; Two.Weight = .85; Two.A = 0; Two.AMin = 2; Two.AMax = 2;
		FMix Alone; Alone.Weight = .15; Alone.A = 0; Alone.AMin = 1; Alone.AMax = 1;
		S.Mixes = {Two, Alone};
		S.Depth[0] = 1.5; S.Depth[1] = 3; S.Depth[2] = 18; S.Depth[3] = 28;
		S.Zones = ZTop | ZSpur | ZFore | ZPatch;
		S.GroupsPerHa = 3.5; S.MaxAlive = 10;
		S.Shy = 1.1;
		S.FirstSight = TEXT("French angelfish (Pomacanthus paru). A pair stays together for years.");
		return S;
	}

	static FSpecies MakeGray()
	{
		FSpecies S;
		S.Key = TEXT("grayangel");
		S.Common = TEXT("Gray angelfish");
		S.Scientific = TEXT("Pomacanthus arcuatus");
		Angel(S);
		FBody B = AngelBody();
		B.HalfH = .275; B.TailFork = 0; B.TailH = .32;        // the square-cut tail
		S.Phases = {FPhase{TEXT("adult"), .35, .50, B}};
		S.Paint = [](int32, const FPaint& At) { return PaintGray(At); };
		FMix Two; Two.Weight = .8; Two.A = 0; Two.AMin = 2; Two.AMax = 2;
		FMix Alone; Alone.Weight = .2; Alone.A = 0; Alone.AMin = 1; Alone.AMax = 1;
		S.Mixes = {Two, Alone};
		S.Depth[0] = 1.5; S.Depth[1] = 3; S.Depth[2] = 20; S.Depth[3] = 30;
		S.Zones = ZTop | ZSpur | ZFore | ZWall | ZPatch;
		S.GroupsPerHa = 3.5; S.MaxAlive = 10;
		S.Shy = .9;
		S.FirstSight = TEXT("Gray angelfish (Pomacanthus arcuatus).");
		return S;
	}

	static KeysReefLife::FAuto GQueen(&MakeQueen);
	static KeysReefLife::FAuto GFrench(&MakeFrench);
	static KeysReefLife::FAuto GGray(&MakeGray);
}
