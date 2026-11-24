// Stoplight parrotfish, Sparisoma viride: the reef-life template species. Everything about the animal is in this
// file; the framework (KeysReefLife.h) knows nothing of it.
//
// Reference (what the paint and the numbers below are held to):
//   Initial phase, 20-30 cm: head and upper body mottled grey-brown, the large scales outlined in white so the
//     flank reads as a checker; belly and all fins bright red; a pale band across the tail base; white beak.
//   Terminal male, 35-45 cm: emerald green; three pink bands on the head and a pink edge to the gill cover with a
//     small yellow spot at its top; a yellow spot at the tail base; the tail a crescent with long lobes, a yellow
//     crescent across it and a blue-green margin; dorsal and anal fins yellow-orange with a blue edge.
//   Both: deep body (about a third of the length), blunt steep head, teeth fused into a beak, big scales (about
//     24 down the side). Swims with its pectoral fins. Grazes algae off dead coral rock by day, leaving pale bite
//     scars, and passes the ground coral as white sand. Sleeps at night against the reef in a mucus cocoon.
//   A terminal male keeps a harem of initial-phase fish and drives other males off. Taking one is prohibited in
//   Florida, so it is neither catchable nor a spear target (it is not in KeysLife's or KeysData's tables at all).
#include "KeysReefLife.h"

namespace KeysReefLifeParrot
{
	using namespace KeysReefLife;

	static FLinearColor Mix(const FLinearColor& A, const FLinearColor& B, double T) { return FMath::Lerp(A, B, float(FMath::Clamp(T, 0.0, 1.0))); }

	static FLinearColor PaintInitial(const FPaint& At)
	{
		static const FLinearColor Scale = Hex(0x4f3d34), Edge = Hex(0xcfc8ba), Red = Hex(0xc9261c), Beak = Hex(0xece6d6), Brown = Hex(0x6e5546), FinRed = Hex(0xbf2a20), Pale = Hex(0xddd2c4);
		switch (At.Part)
		{
		case PBody:
		{
			if (At.U < .035) return Beak;
			// the checker of big scales above; far off, its mean
			const FLinearColor Flank = At.Lod == 0 ? (((At.I + At.J) & 1) ? Edge : Scale) : Mix(Scale, Edge, .3);
			const FLinearColor Upper = At.U < .2 ? Mix(Brown, Flank, (At.U - .08) / .12) : Flank;
			FLinearColor C = Mix(Red, Upper, (At.V + .35) / .3);            // red from the belly up to below the mid-line
			if (At.U > .93) C = Mix(C, Pale, .7);                           // the pale band across the tail base
			return C;
		}
		case PTail: return At.U < .22 ? Mix(Pale, FinRed, At.U / .22) : Mix(FinRed, Hex(0x8e1d18), (At.U - .7) / .3);
		case PPectoral: return Mix(Hex(0xb9584a), Hex(0xd79c8c), At.U);
		case PEye: return At.U < .6 ? FLinearColor(.004f, .004f, .004f) : Hex(0xb87a2e);
		default: return FinRed;                                              // dorsal, anal, pelvic
		}
	}

	static FLinearColor PaintTerminal(const FPaint& At)
	{
		static const FLinearColor Green = Hex(0x13a36b), GreenD = Hex(0x0f8459), Belly = Hex(0x4fcaa2), Pink = Hex(0xee6f8c), Yellow = Hex(0xf4d318), Beak = Hex(0xc4e6da), Teal = Hex(0x177f93), Orange = Hex(0xf0a629);
		switch (At.Part)
		{
		case PBody:
		{
			if (At.U < .035) return Beak;
			FLinearColor C = At.Lod == 0 ? (((At.I + At.J) & 1) ? Green : GreenD) : Mix(Green, GreenD, .5);   // each scale a shade apart
			C = Mix(Belly, C, (At.V + .75) / .5);
			const double Tol = At.Lod == 0 ? .02 : .045;
			// the pink edge of the gill cover, and the yellow spot at its top
			if (FMath::Abs(At.U - .25) < Tol && At.V > -.65 && At.V < .5) C = Pink;
			if (FMath::Abs(At.U - .25) < Tol && At.V >= .5 && At.V < .8) C = Yellow;
			// pink bands across the cheek and snout
			if (At.Lod == 0 && At.U > .05 && At.U < .2 && FMath::Abs(At.V + .1 - (At.U - .05) * 2.2) < .12) C = Mix(C, Pink, .8);
			// the yellow spot at the tail base
			if (At.U > .93 && FMath::Abs(At.V) < .6) C = Yellow;
			return C;
		}
		case PTail:
		{
			// green at the root, a yellow crescent across the middle, a blue-green margin
			const double Cres = At.U - .12 * At.V * At.V;
			if (Cres > .3 && Cres < .62) return Yellow;
			return Cres >= .62 ? Teal : Green;
		}
		case PDorsal: case PAnal: return At.V > .6 ? Teal : Orange;
		case PPectoral: return Mix(Hex(0x1c6f5e), Hex(0x8fd6c2), At.U);
		case PPelvic: return Orange;
		case PEye: return At.U < .6 ? FLinearColor(.004f, .004f, .004f) : Hex(0xf0b030);
		default: return Green;
		}
	}

	static FSpecies MakeStoplight()
	{
		FSpecies S;
		S.Key = TEXT("stoplightparrot");
		S.Common = TEXT("Stoplight parrotfish");
		S.Scientific = TEXT("Sparisoma viride");
		S.Role = ERole::Grazer;

		FBody B;
		B.HalfH = .18; B.HalfW = .072; B.Peak = .38; B.Blunt = .75; B.NoseR = .3; B.Ped = .42; B.PedW = .2; B.Arch = .1;
		B.TailLen = .17; B.TailH = .28; B.TailFork = .04;
		B.Dor0 = .27; B.Dor1 = .9; B.DorH = .06; B.An0 = .6; B.An1 = .9; B.AnH = .05;
		B.PecU = .3; B.PecV = -.1; B.PecLen = .2; B.PecW = .1; B.PelU = .36; B.PelLen = .09;
		B.EyeU = .12; B.EyeV = .45; B.EyeR = .02;
		FPhase Initial{TEXT("initial"), .20, .30, B};
		FPhase Terminal{TEXT("terminal"), .35, .45, B};
		Terminal.Body.TailH = .36; Terminal.Body.TailFork = .32;      // the male's crescent tail with its long lobes
		S.Phases = {Initial, Terminal};
		S.Paint = [](int32 Phase, const FPaint& At) { return Phase == 1 ? PaintTerminal(At) : PaintInitial(At); };

		// a terminal male with his three to six initial-phase fish; or one or two initial-phase fish by themselves
		FMix Harem; Harem.Weight = .62; Harem.A = 1; Harem.AMin = 1; Harem.AMax = 1; Harem.B = 0; Harem.BMin = 3; Harem.BMax = 6;
		FMix Loose; Loose.Weight = .38; Loose.A = 0; Loose.AMin = 1; Loose.AMax = 2;
		S.Mixes = {Harem, Loose};

		// reef top, crest, spur tops and patch reefs, 1 to 15 m, most between 3 and 8
		S.Depth[0] = 1; S.Depth[1] = 3; S.Depth[2] = 8; S.Depth[3] = 15;
		S.Zones = ZCrest | ZTop | ZSpur | ZPatch;
		S.Habit = EHabit::Day;
		S.GroupsPerHa = 12; S.MaxAlive = 70; S.Weight = 1;
		S.Shy = 2;
		S.Cruise = .35; S.Burst = 2.6; S.Roam = 15; S.Alt[0] = .25; S.Alt[1] = .9; S.Spacing = 4; S.Cull = 60;

		S.bRows = true;
		S.bBitesRock = true; S.BiteEvery = 7; S.ScarCm = 3;
		S.PuffEvery = 50;
		S.bCocoon = true;
		S.bSleepsInHoles = true;            // (reeflinks) at night into a hole in the reef, in its cocoon
		S.bClient = true;                   // (reeflinks) visits the cleaning stations by day
		S.ChaserPhase = 1; S.ChaseR = 5;
		S.BiteSound = [](int32 Phase, TArray<FKeysBurst>& Out, FString& What)
		{
			// the beak scraping rock: two short grains of band-passed noise falling in pitch, and a dull knock under
			// them. The male's beak is bigger: everything a fifth lower.
			const float K = Phase == 1 ? .8f : 1.f;
			FKeysBurst A;
			A.Filter = KeysDSP::EFilter::Bandpass; A.Freq = 3000 * K; A.FreqTo = 1700 * K; A.Q = 2.2f; A.Gain = .3f; A.Att = .002f; A.Dur = .07f;
			FKeysBurst B2 = A;
			B2.Freq = 2300 * K; B2.FreqTo = 1300 * K; B2.Q = 2.f; B2.Gain = .2f; B2.Dur = .055f; B2.Delay = .05f;
			FKeysBurst Tok;
			Tok.bTone = true; Tok.Wave = KeysDSP::EWave::Triangle; Tok.Tone = 480 * K; Tok.ToneTo = 240 * K; Tok.Gain = .1f; Tok.Att = .002f; Tok.Dur = .035f;
			Out = {A, B2, Tok};
			What = FString::Printf(TEXT("3 one-shots through AKeysAudio::Burst at the bite, reference distance 2 m: noise band-pass %.0f>%.0f Hz Q 2.2 gain 0.30 70 ms; noise band-pass %.0f>%.0f Hz Q 2.0 gain 0.20 55 ms after 50 ms; triangle tone %.0f>%.0f Hz gain 0.10 35 ms"),
				A.Freq, A.FreqTo, B2.Freq, B2.FreqTo, Tok.Tone, Tok.ToneTo);
		};
		S.FirstSight = TEXT("Stoplight parrotfish (Sparisoma viride). Protected in Florida: look, don't spear.");
		return S;
	}

	static KeysReefLife::FAuto GStoplight(&MakeStoplight);
}
