// The two big Scarus parrotfish of the Florida Keys reef tract: rainbow and midnight. Both use the stoplight
// parrotfish's role (Grazer) and its traits; everything about the animals is in this file.
//
// Reference (what the paint and the numbers below are held to):
//   Rainbow parrotfish, Scarus guacamaia. The largest parrotfish in the Atlantic: to 1.2 m, usually 50-90 cm.
//     Head, shoulders and the front of the body dull bronze-orange; the rear half of the body bright green, each
//     scale green with a bronze edge; dorsal and anal fins bronze at the root and front, green behind and along
//     the edge; tail green with a bronze streak through the middle, its lobes drawn out a little in big fish;
//     beak blue-green. Males and females look alike (no separate colour phases). Deep, heavy body (about a third
//     of the length), blunt steep head, big scales. Swims with its pectoral fins.
//     Small groups (2-5) and lone fish on the reef crest, the back reef and patch reefs, 1 to 15 m, most at 2 to
//     8; the young grow up among mangrove roots. Scrapes algae off dead coral rock by day and passes white sand;
//     at night it goes back to the same shelter in the reef. Wary: keeps well off a diver.
//   Midnight parrotfish, Scarus coelestinus, 50-75 cm. Deep navy blue to blue-black all over, the middle of each
//     scale a little brighter; bright blue patches on the head with no scales on them: a band over the forehead
//     between the eyes, the cheek below the eye, the chin; fins navy with a bright blue edge; beak blue-green.
//     No separate colour phases. Same shape as the rainbow, tail square with slightly longer corners.
//     Roving groups of 3-8 (often with blue tangs and surgeonfish) over the reef top, spurs and the fore reef, 3
//     to 20 m, grazing rock and the edges of sand channels by day; sleeps in the reef at night.
//   Both are far bigger than a stoplight parrotfish (20-45 cm). Taking either is prohibited in Florida: they are
//   neither catchable nor spear targets (they are in none of KeysLife's or KeysData's tables).
#include "KeysReefLife.h"

namespace KeysReefLifeFishParrots
{
	using namespace KeysReefLife;

	static FLinearColor Mix(const FLinearColor& A, const FLinearColor& B, double T) { return FMath::Lerp(A, B, float(FMath::Clamp(T, 0.0, 1.0))); }
	static double Smooth(double T) { T = FMath::Clamp(T, 0.0, 1.0); return T * T * (3 - 2 * T); }

	static FLinearColor PaintRainbow(const FPaint& At)
	{
		static const FLinearColor Bronze = Hex(0xb8662a), BronzeD = Hex(0x93501f), Green = Hex(0x2fa25e), GreenD = Hex(0x1f7f4b), Beak = Hex(0x4ab4a8), Pale = Hex(0xd29a5c);
		switch (At.Part)
		{
		case PBody:
		{
			if (At.U < .06) return Beak;                                     // a big beak: the first two rings of the snout
			// bronze-orange head and shoulders, green from the middle of the body back; up close each scale shows
			const bool bOdd = ((At.I + At.J) & 1) != 0;
			const FLinearColor Front = At.Lod == 0 ? (bOdd ? Bronze : BronzeD) : Mix(Bronze, BronzeD, .5);
			const FLinearColor Rear = At.Lod == 0 ? (bOdd ? Green : Mix(GreenD, Bronze, .4)) : Mix(Green, Mix(GreenD, Bronze, .4), .5);
			FLinearColor C = Mix(Front, Rear, Smooth((At.U - .36) / .22));
			C = Mix(Mix(C, Pale, .3), C, (At.V + .95) / .5);                   // a paler belly
			return C;
		}
		case PDorsal: case PAnal: return At.V > .5 ? Green : Mix(Bronze, Green, (At.U - .15) / .5);
		case PTail:
		{
			if (At.U > .82) return GreenD;
			return Mix(Green, Bronze, (1 - FMath::Abs(At.V) * 1.6) * .75);        // the bronze streak through the middle
		}
		case PPectoral: return Mix(Bronze, Pale, At.U);
		case PPelvic: return Bronze;
		case PEye: return At.U < .6 ? FLinearColor(.004f, .004f, .004f) : Hex(0xc98a2e);
		default: return Green;
		}
	}

	static FLinearColor PaintMidnight(const FPaint& At)
	{
		static const FLinearColor Navy = Hex(0x18266e), NavyD = Hex(0x0c1340), Bright = Hex(0x2a8cf0), Beak = Hex(0x5ac0b6);
		switch (At.Part)
		{
		case PBody:
		{
			if (At.U < .06 && FMath::Abs(At.V) < .8) return Beak;
			FLinearColor C = At.Lod == 0 ? (((At.I + At.J) & 1) ? Navy : NavyD) : Mix(Navy, NavyD, .5);
			// the bright blue, scaleless patches of the head: across the forehead between the eyes, the cheek, the chin
			if (At.U > .06 && At.U < .155 && At.V > .45) C = Bright;
			else if (At.U > .065 && At.U < .2 && At.V > -.8 && At.V < -.15) C = Mix(C, Bright, .85);
			else if (At.U < .075 && At.V < -.2) C = Bright;
			return C;
		}
		case PDorsal: case PAnal: return At.V > .6 ? Bright : Navy;
		case PTail: return At.U > .95 ? Bright : Navy;
		case PPectoral: return Mix(Navy, Hex(0x3f66b8), At.U);
		case PEye: return At.U < .6 ? FLinearColor(.004f, .004f, .004f) : Hex(0x24306e);
		default: return Navy;
		}
	}

	// the beak scraping rock, as the stoplight's but from a far bigger beak: K times its pitch, a little longer
	static void Scrape(float K, TArray<FKeysBurst>& Out, FString& What)
	{
		FKeysBurst A;
		A.Filter = KeysDSP::EFilter::Bandpass; A.Freq = 3000 * K; A.FreqTo = 1700 * K; A.Q = 2.2f; A.Gain = .34f; A.Att = .002f; A.Dur = .085f;
		FKeysBurst B2 = A;
		B2.Freq = 2300 * K; B2.FreqTo = 1300 * K; B2.Q = 2.f; B2.Gain = .23f; B2.Dur = .065f; B2.Delay = .06f;
		FKeysBurst Tok;
		Tok.bTone = true; Tok.Wave = KeysDSP::EWave::Triangle; Tok.Tone = 480 * K; Tok.ToneTo = 240 * K; Tok.Gain = .12f; Tok.Att = .002f; Tok.Dur = .04f;
		Out = {A, B2, Tok};
		What = FString::Printf(TEXT("3 one-shots through AKeysAudio::Burst at the bite, reference distance 2 m: noise band-pass %.0f>%.0f Hz Q 2.2 gain 0.34 85 ms; noise band-pass %.0f>%.0f Hz Q 2.0 gain 0.23 65 ms after 60 ms; triangle tone %.0f>%.0f Hz gain 0.12 40 ms"),
			A.Freq, A.FreqTo, B2.Freq, B2.FreqTo, Tok.Tone, Tok.ToneTo);
	}

	static FBody ScarusBody()
	{
		FBody B;
		B.HalfH = .17; B.HalfW = .078; B.Peak = .36; B.Blunt = .8; B.NoseR = .32; B.Ped = .4; B.PedW = .22; B.Arch = .1;
		B.TailLen = .17; B.TailH = .3; B.TailFork = .14;
		B.Dor0 = .26; B.Dor1 = .9; B.DorH = .055; B.An0 = .6; B.An1 = .9; B.AnH = .045;
		B.PecU = .29; B.PecV = -.1; B.PecLen = .19; B.PecW = .1; B.PelU = .35; B.PelLen = .085;
		B.EyeU = .125; B.EyeV = .48; B.EyeR = .017;
		return B;
	}

	static void Graze(FSpecies& S)
	{
		S.Role = ERole::Grazer;
		S.Habit = EHabit::Day;
		S.bRows = true;
		S.bBitesRock = true; S.BiteEvery = 8; S.ScarCm = 5;       // a beak twice the stoplight's: a wider scar
		S.PuffEvery = 45;
		S.Cull = 60;
	}

	static FSpecies MakeRainbow()
	{
		FSpecies S;
		S.Key = TEXT("rainbowparrot");
		S.Common = TEXT("Rainbow parrotfish");
		S.Scientific = TEXT("Scarus guacamaia");
		Graze(S);
		S.Phases = {FPhase{TEXT("adult"), .50, .90, ScarusBody()}};
		S.Paint = [](int32, const FPaint& At) { return PaintRainbow(At); };
		FMix Group; Group.Weight = .7; Group.A = 0; Group.AMin = 2; Group.AMax = 5;
		FMix Lone; Lone.Weight = .3; Lone.A = 0; Lone.AMin = 1; Lone.AMax = 1;
		S.Mixes = {Group, Lone};
		// reef crest, reef top (the back reef), spur tops and patch reefs, 1 to 15 m, most between 2 and 8
		S.Depth[0] = 1; S.Depth[1] = 2; S.Depth[2] = 8; S.Depth[3] = 15;
		S.Zones = ZCrest | ZTop | ZSpur | ZPatch;
		S.GroupsPerHa = 3.5; S.MaxAlive = 12; S.Weight = 2;
		S.Shy = 3.5;
		S.Cruise = .5; S.Burst = 3.2; S.Roam = 24; S.Alt[0] = .4; S.Alt[1] = 1.4; S.Spacing = 3;
		S.BiteSound = [](int32, TArray<FKeysBurst>& Out, FString& What) { Scrape(.6f, Out, What); };
		S.FirstSight = TEXT("Rainbow parrotfish (Scarus guacamaia), the biggest parrotfish in the Atlantic. Protected in Florida: look, don't spear.");
		return S;
	}

	static FSpecies MakeMidnight()
	{
		FSpecies S;
		S.Key = TEXT("midnightparrot");
		S.Common = TEXT("Midnight parrotfish");
		S.Scientific = TEXT("Scarus coelestinus");
		Graze(S);
		FBody B = ScarusBody();
		B.HalfH = .175; B.TailFork = .09; B.EyeR = .018;
		S.Phases = {FPhase{TEXT("adult"), .50, .75, B}};
		S.Paint = [](int32, const FPaint& At) { return PaintMidnight(At); };
		FMix Group; Group.Weight = .8; Group.A = 0; Group.AMin = 3; Group.AMax = 8;
		FMix Few; Few.Weight = .2; Few.A = 0; Few.AMin = 1; Few.AMax = 2;
		S.Mixes = {Group, Few};
		// reef top, spurs, the fore reef and patch reefs, 3 to 20 m
		S.Depth[0] = 2; S.Depth[1] = 4; S.Depth[2] = 15; S.Depth[3] = 22;
		S.Zones = ZTop | ZSpur | ZFore | ZPatch;
		S.GroupsPerHa = 3; S.MaxAlive = 16; S.Weight = 2;
		S.Shy = 2.5;
		S.Cruise = .55; S.Burst = 3; S.Roam = 28; S.Alt[0] = .4; S.Alt[1] = 1.5; S.Spacing = 3;
		S.BiteSound = [](int32, TArray<FKeysBurst>& Out, FString& What) { Scrape(.68f, Out, What); };
		S.FirstSight = TEXT("Midnight parrotfish (Scarus coelestinus). Protected in Florida: look, don't spear.");
		return S;
	}

	static KeysReefLife::FAuto GRainbow(&MakeRainbow);
	static KeysReefLife::FAuto GMidnight(&MakeMidnight);
}
