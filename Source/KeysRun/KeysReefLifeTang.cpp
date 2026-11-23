// Blue tang, Acanthurus coeruleus. Written by following the reef-creature recipe only (the proof of the recipe).
//
// Reference:
//   Adult, 15-25 cm: a deep, thin oval disc (depth about half the length), deep blue to purple-blue all over with
//     faint darker lines along the flank; dorsal and anal fins the same blue with a bright blue edge; a yellow
//     spine at the tail base; tail a shallow crescent; small pointed mouth; eye high on the head.
//   Juvenile, 5-10 cm: the same shape, bright yellow all over, a blue ring round the eye and blue edges to the
//     dorsal and anal fins.
//   Swims with its pectoral fins. By day adults graze algae off the reef top in schools of 10 to 40, moving on
//     together; juveniles keep alone to one patch. 1 to 18 m, most at 2 to 12. Sleeps at night in the reef.
#include "KeysReefLife.h"

namespace KeysReefLifeTang
{
	using namespace KeysReefLife;

	static FLinearColor PaintTang(int32 Phase, const FPaint& At)
	{
		static const FLinearColor Blue = Hex(0x2a4fb0), BlueD = Hex(0x1e3a8c), Bright = Hex(0x4f8ff0), Yellow = Hex(0xf6d21a), YellowD = Hex(0xe0b810);
		const bool bJuv = Phase == 1;
		switch (At.Part)
		{
		case PBody:
		{
			if (bJuv) return At.V > .3 ? YellowD : Yellow;
			// faint darker lines along the flank (near only); the yellow spine at the tail base
			FLinearColor C = (At.Lod == 0 && (At.J & 1)) ? BlueD : Blue;
			if (At.U > .9 && FMath::Abs(At.V) < .45) C = Yellow;
			return C;
		}
		case PDorsal: case PAnal: return At.V > .6 ? Bright : (bJuv ? Yellow : Blue);
		case PTail: return bJuv ? Yellow : (At.U > .8 ? Bright : Blue);
		case PPectoral: return bJuv ? Hex(0xf2dc6a) : Hex(0x6f8fd0);
		case PEye: return At.U < .6 ? FLinearColor(.004f, .004f, .004f) : (bJuv ? Bright : Hex(0xd9c75a));
		default: return bJuv ? Yellow : Blue;
		}
	}

	static FSpecies MakeBlueTang()
	{
		FSpecies S;
		S.Key = TEXT("bluetang");
		S.Common = TEXT("Blue tang");
		S.Scientific = TEXT("Acanthurus coeruleus");
		S.Role = ERole::Grazer;

		FBody B;
		B.HalfH = .25; B.HalfW = .05; B.Peak = .4; B.Blunt = .6; B.NoseR = .16; B.Ped = .2; B.PedW = .25; B.Arch = .04;
		B.TailLen = .16; B.TailH = .34; B.TailFork = .25;
		B.Dor0 = .2; B.Dor1 = .93; B.DorH = .08; B.An0 = .45; B.An1 = .93; B.AnH = .07;
		B.PecU = .3; B.PecV = -.05; B.PecLen = .17; B.PecW = .08; B.PelU = .34; B.PelLen = .08;
		B.EyeU = .13; B.EyeV = .5; B.EyeR = .022;
		S.Phases = {FPhase{TEXT("adult"), .15, .25, B}, FPhase{TEXT("juvenile"), .05, .10, B}};
		S.Paint = &PaintTang;

		FMix School; School.Weight = .7; School.A = 0; School.AMin = 10; School.AMax = 40;
		FMix Young; Young.Weight = .3; Young.A = 1; Young.AMin = 1; Young.AMax = 1;
		S.Mixes = {School, Young};

		S.Depth[0] = 1; S.Depth[1] = 2; S.Depth[2] = 12; S.Depth[3] = 18;
		S.Zones = ZTop | ZSpur | ZPatch | ZFore;
		S.Habit = EHabit::Day;
		S.GroupsPerHa = 3; S.MaxAlive = 110; S.Weight = 1;
		S.Shy = 1.5;
		S.Cruise = .45; S.Burst = 2.4; S.Roam = 20; S.Alt[0] = .2; S.Alt[1] = .8; S.Spacing = 2.2; S.Cull = 60;
		S.bRows = true;
		S.bBitesRock = true; S.BiteEvery = 4;
		S.FirstSight = TEXT("Blue tang (Acanthurus coeruleus).");
		return S;
	}

	static KeysReefLife::FAuto GBlueTang(&MakeBlueTang);
}
