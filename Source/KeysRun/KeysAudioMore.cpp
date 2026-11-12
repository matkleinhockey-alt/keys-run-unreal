#include "KeysAudio.h"

// The second pass's sounds, all synthesized like the rest (web audio/engine.ts): a terrier's yap, the party boats'
// kick drum, the pirates' ship's bell, the boat pistol, a flare, the slot machine, a crowd and the treasure.
bool AKeysAudio::PlayMore(FName Id, const FVector& GamePos, float Power)
{
	auto Tone = [&](KeysDSP::EWave W, float F0, float F1, float Gain, float Att, float Dur, float Delay, bool bPlaced, float Ref = 6.f)
	{
		FKeysBurst B; B.bTone = true; B.Wave = W; B.Tone = F0; B.ToneTo = F1; B.Gain = Gain; B.Att = Att; B.Dur = Dur; B.Delay = Delay;
		Burst(B, bPlaced ? &GamePos : nullptr, Ref);
	};
	auto Noise = [&](KeysDSP::EFilter Fl, float F0, float F1, float Q, float Gain, float Att, float Dur, float Delay, bool bPlaced, float Ref = 6.f)
	{
		FKeysBurst B; B.Filter = Fl; B.Freq = F0; B.FreqTo = F1; B.Q = Q; B.Gain = Gain; B.Att = Att; B.Dur = Dur; B.Delay = Delay;
		Burst(B, bPlaced ? &GamePos : nullptr, Ref);
	};
	using KeysDSP::EWave;
	using KeysDSP::EFilter;
	if (Id == TEXT("Bark"))
	{
		// two sharp yaps: a falling square chirp over a puff of noise
		for (int32 k = 0; k < 2; k++)
		{
			Tone(EWave::Square, 820, 460, .05f, .004f, .09f, k * .17f, true, 4);
			Noise(EFilter::Bandpass, 1400, 900, 2, .06f, .003f, .07f, k * .17f, true, 4);
		}
	}
	else if (Id == TEXT("PartyBeat"))
	{
		// a muffled kick across the water and the off-beat hat
		Tone(EWave::Sine, 95, 42, .18f * Power, .003f, .16f, 0, true, 30);
		Noise(EFilter::Highpass, 6000, 0, .7f, .03f * Power, .002f, .04f, .25f, true, 30);
	}
	else if (Id == TEXT("Bell"))
	{
		// a ship's bell: two strikes, a bright inharmonic partial set ringing out
		const float Partials[4] = {1.f, 2.76f, 5.4f, 8.93f}, Gains[4] = {.12f, .06f, .035f, .02f};
		for (int32 s = 0; s < 2; s++)
			for (int32 k = 0; k < 4; k++) Tone(EWave::Sine, 520 * Partials[k], 0, Gains[k] * Power, .002f, 2.6f / (1 + k * .6f), s * .45f, true, 220);
	}
	else if (Id == TEXT("Gunshot"))
	{
		// the crack, the boom and the slap back off the water
		Noise(EFilter::Highpass, 2500, 800, .7f, .5f * Power, .0008f, .07f, 0, false);
		Noise(EFilter::Lowpass, 900, 120, .8f, .7f * Power, .001f, .28f, 0, false);
		Tone(EWave::Sine, 120, 45, .35f * Power, .001f, .2f, 0, false);
		Noise(EFilter::Bandpass, 700, 300, 1.2f, .08f * Power, .01f, .35f, .11f, false);
	}
	else if (Id == TEXT("DryClick"))
	{
		Noise(EFilter::Bandpass, 3200, 0, 6, .12f, .0005f, .02f, 0, false);
	}
	else if (Id == TEXT("Reload"))
	{
		// mag out, mag in, the slide
		Noise(EFilter::Bandpass, 1800, 0, 4, .1f, .001f, .04f, 0, false);
		Noise(EFilter::Bandpass, 1300, 0, 4, .14f, .001f, .05f, .55f * Power, false);
		Noise(EFilter::Bandpass, 2600, 1500, 3, .16f, .001f, .09f, .9f * Power, false);
	}
	else if (Id == TEXT("Flare"))
	{
		// the pop and the long hissing climb
		Noise(EFilter::Lowpass, 1200, 300, .8f, .45f, .001f, .18f, 0, false);
		Noise(EFilter::Bandpass, 2200, 5200, 1.2f, .14f, .1f, 2.4f, .05f, false);
	}
	else if (Id == TEXT("Ricochet"))
	{
		Tone(EWave::Sine, 3400, 1500, .08f * Power, .002f, .45f, 0, true, 20);
		Noise(EFilter::Bandpass, 1200, 0, 2, .1f * Power, .002f, .06f, 0, true, 20);
	}
	else if (Id == TEXT("Thud"))
	{
		Noise(EFilter::Lowpass, 500, 120, .8f, .2f * Power, .002f, .12f, 0, true, 20);
	}
	else if (Id == TEXT("Plink"))
	{
		// a hit: a wet slap and a little splash
		Noise(EFilter::Bandpass, 1600, 600, 1.5f, .22f * Power, .002f, .14f, 0, true, 15);
		Tone(EWave::Triangle, 900, 500, .05f * Power, .002f, .1f, 0, true, 15);
	}
	else if (Id == TEXT("Squawk"))
	{
		Tone(EWave::Saw, 1650, 900, .07f * Power, .01f, .2f, 0, true, 25);
		Tone(EWave::Saw, 1500, 800, .06f * Power, .01f, .26f, .22f, true, 25);
	}
	else if (Id == TEXT("SlotSpin"))
	{
		// the reels' ratchet: a run of quick ticks
		for (int32 k = 0; k < 14; k++) Noise(EFilter::Bandpass, 2400 + (k % 3) * 300, 0, 8, .05f, .001f, .025f, k * .07f, false);
	}
	else if (Id == TEXT("SlotStop"))
	{
		Noise(EFilter::Bandpass, 900, 0, 3, .16f, .001f, .06f, 0, false);
		Tone(EWave::Triangle, 330, 300, .05f, .002f, .08f, 0, false);
	}
	else if (Id == TEXT("SlotWin") || Id == TEXT("Coins"))
	{
		// a rising arpeggio and the coins
		const float Notes[5] = {523, 659, 784, 1047, 1319};
		const int32 N = Power > 2 ? 5 : 3;
		for (int32 k = 0; k < N; k++) Tone(EWave::Square, Notes[k], 0, .045f, .004f, .16f, k * .09f, false);
		for (int32 k = 0; k < 8 + int32(Power) * 2; k++) Tone(EWave::Sine, 3800 + (k % 4) * 450, 0, .03f, .001f, .07f, .3f + k * .055f, false);
	}
	else if (Id == TEXT("Cheer"))
	{
		const float Steps[5] = {0.f, .6f, 1.2f, 1.8f, 2.6f}, Gains[5] = {.25f, .3f, .28f, .22f, .14f};
		for (int32 i = 0; i < 5; i++) Noise(EFilter::Bandpass, 1300, 900, .7f, Gains[i] * Power, i == 0 ? .3f : .1f, .9f, Steps[i], false);
		for (int32 k = 0; k < 6; k++) Tone(EWave::Saw, 1200 + k * 140, 1600 + k * 120, .012f * Power, .05f, .4f, .3f + k * .37f, false);
	}
	else if (Id == TEXT("Chest"))
	{
		// a lid prised up under water: a dull crack and the metal giving way
		Noise(EFilter::Lowpass, 700, 180, .8f, .3f, .002f, .25f, 0, false);
		Tone(EWave::Triangle, 260, 180, .08f, .01f, .5f, .1f, false);
	}
	else return false;
	return true;
}
