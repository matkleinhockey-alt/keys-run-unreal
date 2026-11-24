#include "KeysReefAudioInt.h"

// The recipes: what each sound is made of. Every one is a stand-in built from oscillators and filtered noise for a
// real sound of the Florida Keys; the note at the head of each says what the real one is. None has been listened to.
// Gains are the peaks at the sound's reference distance (Sounds(), below); the game thread scales them by distance.
namespace KeysReefAudio
{
	using KeysDSP::EWave;
	using KeysDSP::EFilter;

	const TArray<FSoundInfo>& Sounds()
	{
		static const TArray<FSoundInfo> S = {
			// under water
			{TEXT("grunt"), 2, 12, true}, {TEXT("pop"), 1.5f, 10, true}, {TEXT("boom"), 4, 25, true}, {TEXT("boomgoliath"), 6, 35, true},
			{TEXT("rasp"), 1.5f, 10, true}, {TEXT("whistle"), 15, 150, true}, {TEXT("clicks"), 10, 150, true},
			// above water
			{TEXT("croak.greatblue"), 15, 200, false}, {TEXT("croak.egret"), 15, 200, false}, {TEXT("croak.small"), 12, 150, false},
			{TEXT("skeow"), 12, 150, false}, {TEXT("quok"), 15, 200, false}, {TEXT("honk"), 12, 150, false}, {TEXT("rattle"), 15, 200, false},
			{TEXT("kree"), 15, 200, false}, {TEXT("osprey"), 20, 250, false}, {TEXT("flush.big"), 8, 100, false}, {TEXT("flush.small"), 6, 60, false},
			{TEXT("flight"), 15, 200, false}, {TEXT("plunge.pelican"), 12, 150, false}, {TEXT("plunge.small"), 6, 60, false},
			{TEXT("jump"), 6, 80, false}, {TEXT("shower"), 8, 100, false}, {TEXT("roll"), 5, 50, false}, {TEXT("crabdrop"), 3, 25, false},
			{TEXT("strike"), 4, 30, false}};
		return S;
	}

	const FSoundInfo* FindSound(const FString& Name)
	{
		for (const FSoundInfo& I : Sounds()) if (Name == I.Name) return &I;
		return nullptr;
	}

	bool MakeSound(const FString& Name, FRandomStream& Rng, TArray<FKeysReefNote>& Out, FString& Desc)
	{
		auto Rnd = [&Rng](float A, float B) { return A + (B - A) * Rng.FRand(); };
		// a tone: wave, pitch start / middle / end, peak, attack, length, delay, release (0: dies away)
		auto Tone = [&Out](EWave W, float F0, float F1, float F2, float Gain, float Att, float Dur, float Delay, float Rel = 0.f) -> FKeysReefNote&
		{
			FKeysReefNote N; N.bTone = true; N.Wave = W; N.F0 = F0; N.F1 = F1; N.F2 = F2; N.Gain = Gain; N.Att = Att; N.Dur = Dur; N.Delay = Delay; N.Rel = Rel;
			return Out.Add_GetRef(N);
		};
		// filtered noise: filter, from, to, Q, peak, attack, length, delay
		auto Noise = [&Out](EFilter Fl, float Fc0, float Fc1, float Q, float Gain, float Att, float Dur, float Delay) -> FKeysReefNote&
		{
			FKeysReefNote N; N.Filter = Fl; N.Fc0 = Fc0; N.Fc1 = Fc1; N.Q = Q; N.Gain = Gain; N.Att = Att; N.Dur = Dur; N.Delay = Delay;
			return Out.Add_GetRef(N);
		};
		// a rough voice: a saw through a band-pass (the bird's throat), chopped at AmHz
		auto Croak = [&Tone](float F0, float F2, float Fc, float Q, float AmHz, float Am, float Gain, float Att, float Dur, float Rel, float Delay) -> FKeysReefNote&
		{
			FKeysReefNote& N = Tone(EWave::Saw, F0, 0, F2, Gain, Att, Dur, Delay, Rel);
			N.Fc0 = Fc; N.Q = Q; N.Filter = EFilter::Bandpass; N.Am = Am; N.AmHz = AmHz;
			return N;
		};
		// wing beats: N soft thumps of low-passed noise
		auto Wings = [&Noise, &Rnd](int32 N, float Every, float Fc, float Dur, float Gain, float Delay)
		{
			for (int32 k = 0; k < N; k++)
			{
				const float T = Delay + k * Every * Rnd(.92f, 1.08f);
				Noise(EFilter::Lowpass, Fc, Fc * .38f, 0, Gain * Rnd(.75f, 1.f), Dur * .3f, Dur, T);
				Noise(EFilter::Bandpass, 2400, 0, .7f, Gain * .1f, Dur * .25f, Dur * .6f, T);
			}
		};

		// ------------------------------------------------------------ under water
		if (Name == TEXT("grunt"))
		{
			// Real: grunts (Haemulon) grind their throat teeth and the swim bladder makes it a short rasping grunt,
			// most of it between 300 Hz and 1 kHz, a few pulses in about a tenth of a second.
			const int32 N = Rng.RandRange(4, 7);
			const float Fc = Rnd(560, 880);
			float T = 0;
			for (int32 k = 0; k < N; k++) { Noise(EFilter::Bandpass, Fc * Rnd(.92f, 1.08f), 0, 3, .55f * Rnd(.7f, 1.f), .002f, .022f, T); T += Rnd(.016f, .023f); }
			Desc = FString::Printf(TEXT("%d pulses of band-passed noise (%.0f Hz, Q 3, 22 ms each, 16-23 ms apart)"), N, Fc);
		}
		else if (Name == TEXT("pop"))
		{
			// Real: a damselfish or sergeant major driving something off its ground makes one to four low pops,
			// each 10-20 ms, 300-600 Hz.
			const int32 N = Rng.RandRange(2, 4);
			float T = 0;
			for (int32 k = 0; k < N; k++)
			{
				Tone(EWave::Sine, Rnd(450, 520), 0, 330, .15f, .002f, .03f, T);
				Noise(EFilter::Bandpass, 1200, 0, 2, .04f, .001f, .008f, T);
				T += Rnd(.045f, .07f);
			}
			Desc = FString::Printf(TEXT("%d pops, each a sine falling 450-520 -> 330 Hz in 30 ms over an 8 ms tick of noise at 1.2 kHz, 45-70 ms apart"), N);
		}
		else if (Name == TEXT("boom"))
		{
			// Real: black, gag and red grouper give a low tonal boom or growl, roughly 70-150 Hz, a third of a second.
			Tone(EWave::Sine, 96, 0, 70, .10f, .03f, .34f, 0);
			Tone(EWave::Sine, 192, 0, 140, .02f, .03f, .26f, 0);
			Desc = TEXT("a sine falling 96 -> 70 Hz, 0.34 s, 30 ms attack, with its octave at a fifth of the level");
		}
		else if (Name == TEXT("boomgoliath"))
		{
			// Real: a goliath grouper's boom is about 60 Hz and can be felt in the chest; it booms at divers who come close.
			const int32 N = Rng.FRand() < .35f ? 2 : 1;
			for (int32 k = 0; k < N; k++)
			{
				const float T = k * .42f;
				Tone(EWave::Sine, 62, 0, 46, .15f, .035f, .5f, T);
				Tone(EWave::Triangle, 124, 0, 92, .022f, .03f, .4f, T);
				Noise(EFilter::Lowpass, 130, 60, 0, .05f, .02f, .22f, T);
			}
			Desc = FString::Printf(TEXT("%d boom(s): a sine falling 62 -> 46 Hz, 0.5 s, a triangle an octave up at a seventh of the level, a thump of noise under 130 Hz"), N);
		}
		else if (Name == TEXT("rasp"))
		{
			// Real: a Caribbean spiny lobster rubs a soft plectrum at the base of each antenna over a file: a rasp of
			// stick-slip pulses, broadband, a tenth to a fifth of a second.
			const int32 N = Rng.RandRange(12, 18);
			float T = 0;
			for (int32 k = 0; k < N; k++) { Noise(EFilter::Bandpass, Rnd(2600, 3600), 0, .7f, .40f * Rnd(.6f, 1.f), .0006f, .006f, T); T += Rnd(.007f, .0095f) * (1.f - .25f * k / N); }
			Desc = FString::Printf(TEXT("%d ticks of wide band-passed noise (2.6-3.6 kHz, Q 0.7, 6 ms each), 7-9 ms apart and closing up"), N);
		}
		else if (Name == TEXT("whistle"))
		{
			// Real: a bottlenose dolphin's whistle is a pure tone sweeping between about 5 and 15 kHz for half a second to a second.
			const int32 N = Rng.RandRange(1, 3);
			float T = 0;
			for (int32 k = 0; k < N; k++)
			{
				const float Dur = Rnd(.45f, .9f);
				FKeysReefNote& W = Tone(EWave::Sine, Rnd(5200, 7800), Rnd(10500, 14500), Rnd(7000, 9500), .04f, .06f, Dur, T, .12f);
				W.Vib = .004f; W.VibHz = 9;
				T += Dur + Rnd(.12f, .5f);
			}
			Desc = FString::Printf(TEXT("%d whistle(s): a sine rising from 5.2-7.8 kHz to 10.5-14.5 kHz and falling to 7-9.5 kHz, 0.45-0.9 s"), N);
		}
		else if (Name == TEXT("clicks"))
		{
			// Real: echolocation clicks, each far shorter than a millisecond and mostly above hearing; what a diver
			// hears is a train of ticks that runs together into a creak as the dolphin closes on something.
			const int32 N = Rng.RandRange(20, 30);
			float T = 0;
			for (int32 k = 0; k < N; k++) { Noise(EFilter::Bandpass, 9000, 0, 1, .25f, .0004f, .004f, T); T += FMath::Lerp(.046f, .011f, float(k) / N); }
			Desc = FString::Printf(TEXT("a train of %d ticks (noise round 9 kHz, 4 ms each), 46 ms apart closing to 11 ms"), N);
		}
		// ------------------------------------------------------------ above water: calls
		else if (Name == TEXT("croak.greatblue"))
		{
			// Real: a great blue heron put up off its perch gives a deep, harsh "fraaahnk".
			Croak(176, 140, 950, 1.1f, 36, .9f, .22f, .03f, .62f, .16f, 0);
			FKeysReefNote& B = Noise(EFilter::Bandpass, 1600, 1200, 1, .05f, .03f, .6f, 0); B.Am = .9f; B.AmHz = 36; B.Rel = .16f;
			Desc = TEXT("a saw falling 176 -> 140 Hz through a 950 Hz band-pass, chopped 36 times a second, 0.62 s, with breath noise chopped the same");
		}
		else if (Name == TEXT("croak.egret"))
		{
			// Real: a great egret's alarm is a low dry croak, "kraak".
			Croak(232, 200, 1250, 1.4f, 52, .85f, .18f, .02f, .38f, .09f, 0);
			Desc = TEXT("a saw falling 232 -> 200 Hz through a 1.25 kHz band-pass, chopped 52 times a second, 0.38 s");
		}
		else if (Name == TEXT("croak.small"))
		{
			// Real: the small herons, the spoonbill, the cormorant and the anhinga all croak or grunt when put up; this one voice stands for them.
			Croak(360, 300, 1700, 1.5f, 60, .8f, .14f, .015f, .22f, .06f, 0);
			Desc = TEXT("a saw falling 360 -> 300 Hz through a 1.7 kHz band-pass, chopped 60 times a second, 0.22 s");
		}
		else if (Name == TEXT("skeow"))
		{
			// Real: a green heron flushes with a sharp, falling "skeow".
			FKeysReefNote& N = Tone(EWave::Square, 2100, 2400, 1300, .035f, .01f, .2f, 0, .06f);
			N.Fc0 = 2200; N.Q = 1.2f;
			Desc = TEXT("a square wave up from 2.1 to 2.4 kHz and down to 1.3 kHz in 0.2 s, through a 2.2 kHz band-pass");
		}
		else if (Name == TEXT("quok"))
		{
			// Real: a night heron's flat, barking "quok", given on the wing.
			Croak(540, 420, 1500, 2, 85, .7f, .18f, .008f, .13f, .05f, 0);
			Desc = TEXT("a saw falling 540 -> 420 Hz through a 1.5 kHz band-pass, chopped 85 times a second, 0.13 s");
		}
		else if (Name == TEXT("honk"))
		{
			// Real: a white ibis grunts a nasal "hunk, hunk" as it flies.
			const int32 N = Rng.RandRange(2, 3);
			for (int32 k = 0; k < N; k++) Croak(312, 268, 850, 3, 45, .5f, .15f, .012f, .15f, .05f, k * Rnd(.22f, .27f));
			Desc = FString::Printf(TEXT("%d notes, each a saw falling 312 -> 268 Hz through a narrow 850 Hz band-pass (the nasal part), 0.15 s, a quarter second apart"), N);
		}
		else if (Name == TEXT("rattle"))
		{
			// Real: a belted kingfisher's dry mechanical rattle as it leaves its perch.
			const int32 N = Rng.RandRange(15, 21);
			for (int32 k = 0; k < N; k++)
			{
				const float T = k * .042f, G = Rnd(.75f, 1.f) * (k < 2 ? .6f : 1.f);
				Noise(EFilter::Bandpass, FMath::Lerp(3500.f, 2900.f, float(k) / N), 0, 3, .25f * G, .001f, .017f, T);
			}
			Desc = FString::Printf(TEXT("%d ticks 42 ms apart (24 a second), each 17 ms of noise through a band-pass (Q 3) sliding 3.5 -> 2.9 kHz"), N);
		}
		else if (Name == TEXT("kree"))
		{
			// Real: a royal tern's grating "kee-rreet" as it turns over bait.
			FKeysReefNote& N = Tone(EWave::Saw, 2500, 3100, 2300, .09f, .015f, .32f, 0, .08f);
			N.Fc0 = 2900; N.Q = 1.5f; N.Am = .45f; N.AmHz = 95;
			Desc = TEXT("a saw up from 2.5 to 3.1 kHz and down to 2.3 kHz in 0.32 s, through a 2.9 kHz band-pass, roughened 95 times a second");
		}
		else if (Name == TEXT("osprey"))
		{
			// Real: an osprey's run of high, clear chirps, "cheep cheep cheep", rising then falling away.
			const int32 N = Rng.RandRange(5, 8);
			for (int32 k = 0; k < N; k++)
			{
				const float Up = 1.f + .16f * FMath::Sin(UE_PI * float(k) / (N - 1));
				Tone(EWave::Sine, 2250 * Up, 0, 3300 * Up, .045f, .012f, .1f, k * .15f, .04f);
				Tone(EWave::Sine, 4500 * Up, 0, 6600 * Up, .009f, .012f, .1f, k * .15f, .04f);
			}
			Desc = FString::Printf(TEXT("%d chirps 0.15 s apart, each a sine rising 2.25 -> 3.3 kHz in 0.1 s with its octave at a fifth of the level; the run is a sixth higher in the middle"), N);
		}
		// ------------------------------------------------------------ above water: wings and water
		else if (Name == TEXT("flush.big"))
		{
			// Real: a heron, egret, pelican or osprey taking off: four or five slow, heavy wing beats.
			const int32 N = Rng.RandRange(4, 5);
			Wings(N, .3f, 320, .16f, .26f, 0);
			Desc = FString::Printf(TEXT("%d wing beats 0.3 s apart, each 0.16 s of noise low-passed from 320 Hz down to 120 Hz with a tenth as much rustle at 2.4 kHz"), N);
		}
		else if (Name == TEXT("flush.small"))
		{
			// Real: an ibis, a kingfisher or a small heron taking off: quick, light wing beats.
			const int32 N = Rng.RandRange(5, 7);
			Wings(N, .13f, 520, .08f, .15f, 0);
			Desc = FString::Printf(TEXT("%d wing beats 0.13 s apart, each 0.08 s of noise low-passed from 520 Hz down to 200 Hz with a tenth as much rustle at 2.4 kHz"), N);
		}
		else if (Name == TEXT("flight"))
		{
			// Real: a flight of birds leaving or coming in to a rookery: many wings at once.
			for (int32 k = 0; k < 14; k++)
			{
				const float T = Rnd(0, 1.8f), Pan = Rnd(.25f, .75f);
				FKeysReefNote& N = Noise(EFilter::Lowpass, Rnd(320, 520), 150, 0, .12f * Rnd(.6f, 1.f), .03f, Rnd(.09f, .15f), T);
				N.L = FMath::Cos(Pan * UE_PI * .5f) * 1.41421f; N.R = FMath::Sin(Pan * UE_PI * .5f) * 1.41421f;
			}
			Desc = TEXT("14 wing beats scattered over 1.8 s and across the stereo field, each 0.09-0.15 s of noise low-passed from 320-520 Hz down to 150 Hz");
		}
		else if (Name == TEXT("plunge.pelican"))
		{
			// Real: a brown pelican (or an osprey) hitting the water from a height: a heavy slap, a boom and falling spray.
			Noise(EFilter::Lowpass, 1100, 180, 0, .18f, .004f, .7f, 0);
			Tone(EWave::Sine, 90, 0, 45, .09f, .004f, .25f, 0);
			Noise(EFilter::Highpass, 2600, 0, 0, .05f, .03f, .9f, .03f);
			Desc = TEXT("noise low-passed from 1.1 kHz down to 180 Hz over 0.7 s, a sine thump falling 90 -> 45 Hz, and 0.9 s of spray (noise above 2.6 kHz)");
		}
		else if (Name == TEXT("plunge.small"))
		{
			// Real: a tern or a kingfisher going in: a small, bright plip.
			Noise(EFilter::Bandpass, 1900, 700, 1.2f, .2f, .002f, .14f, 0);
			Noise(EFilter::Highpass, 3500, 0, 0, .05f, .01f, .2f, .02f);
			Desc = TEXT("0.14 s of noise through a band-pass falling 1.9 kHz -> 700 Hz and 0.2 s of spray above 3.5 kHz");
		}
		else if (Name == TEXT("jump"))
		{
			// Real: a mullet landing flat on its side.
			Noise(EFilter::Bandpass, 1500, 450, 1, .28f, .003f, .2f, 0);
			Noise(EFilter::Lowpass, 420, 160, 0, .2f, .004f, .3f, 0);
			Desc = TEXT("a slap: 0.2 s of noise through a band-pass falling 1.5 kHz -> 450 Hz over 0.3 s of noise low-passed from 420 Hz down");
		}
		else if (Name == TEXT("shower"))
		{
			// Real: a school of bait leaving the water ahead of a predator: a hiss of hundreds of tiny splashes, then patter.
			Noise(EFilter::Highpass, 3600, 0, 0, .12f, .09f, .7f, 0);
			for (int32 k = 0; k < 12; k++) Noise(EFilter::Bandpass, Rnd(2400, 5200), 0, 3, .06f * Rnd(.5f, 1.f), .001f, .03f, Rnd(.1f, 1.3f));
			Desc = TEXT("0.7 s of hiss (noise above 3.6 kHz, 90 ms attack) and 12 ticks of band-passed noise (2.4-5.2 kHz, 30 ms) scattered over the next 1.3 s");
		}
		else if (Name == TEXT("roll"))
		{
			// Real: a tarpon rolling at the surface to gulp air: a soft gulp and a swirl.
			Tone(EWave::Sine, 170, 0, 80, .08f, .02f, .2f, 0);
			Noise(EFilter::Lowpass, 520, 180, 0, .08f, .12f, .6f, .05f);
			Desc = TEXT("a sine falling 170 -> 80 Hz in 0.2 s (the gulp) and 0.6 s of noise low-passed from 520 Hz down, swelling in over 0.12 s (the swirl)");
		}
		else if (Name == TEXT("crabdrop"))
		{
			// Real: mangrove tree crabs letting go of a root and dropping into the water as a boat comes near.
			const int32 N = Rng.RandRange(2, 3);
			float T = 0;
			for (int32 k = 0; k < N; k++)
			{
				Tone(EWave::Sine, Rnd(560, 700), 0, 250, .12f, .002f, .05f, T);
				Noise(EFilter::Bandpass, 2200, 0, 2, .04f, .001f, .02f, T);
				T += Rnd(.12f, .35f);
			}
			Desc = FString::Printf(TEXT("%d plops, each a sine falling 560-700 -> 250 Hz in 50 ms over a 20 ms tick of noise at 2.2 kHz"), N);
		}
		else if (Name == TEXT("strike"))
		{
			// Real: a wading bird's bill going into the water.
			Noise(EFilter::Bandpass, 2700, 0, 3, .36f, .001f, .04f, 0);
			Noise(EFilter::Lowpass, 600, 0, 0, .15f, .002f, .08f, 0);
			Desc = TEXT("a 40 ms tick of noise at 2.7 kHz over 80 ms of noise below 600 Hz");
		}
		else return false;
		return true;
	}

	double DuskAt(double Hour)
	{
		auto Bump = [](double H, double C) { const double D = (H - C) / 1.1; return FMath::Exp(-D * D); };
		return FMath::Max(Bump(Hour, 6.15), Bump(Hour, 19.15));
	}

	FKeysReefBed BedFor(const FBedIn& In)
	{
		auto Smooth = [](double A, double B, double X) { const double T = FMath::Clamp((X - A) / (B - A), 0.0, 1.0); return T * T * (3 - 2 * T); };
		FKeysReefBed B;
		const double Cover = FMath::Clamp(In.Cover, 0.0, 1.0);
		// snapping shrimp live in the reef and hard bottom: thick there, almost none over sand; fewer under deep water
		// (the bottom is far below); busiest at dusk and dawn, a little busier all night than by day
		const bool bNight = In.Hour < 5.5 || In.Hour > 20;
		const double Busy = 1 + .4 * DuskAt(In.Hour) + (bNight ? .12 : 0);
		const double DeepWater = FMath::Lerp(1.0, .3, Smooth(14, 40, In.WaterDepth));
		const double Shrimp = FMath::Pow(Cover, 1.3) * DeepWater;
		B.SnapRate = float((.35 + 46 * Shrimp) * Busy);
		B.SnapGain = float(.02 + .10 * Shrimp);
		B.Sizzle = float((.0012 + .010 * Shrimp) * Busy);
		// the surge: the swell working over a reef top that is within a few metres of the surface, heard from close under it
		const double Shallow = 1 - Smooth(2.5, 10, In.WaterDepth);
		const double Sea = .6 + .4 * FMath::Min(In.Sea, 2.0);
		const double Env = .25 + .75 * FMath::Pow(FMath::Clamp(In.Swell, 0.0, 1.0), 1.5);
		B.Surge = float(.055 * (.25 + .75 * Cover) * Shallow * Sea * Env * FMath::Exp(-In.DiverDepth / 6));
		B.SurgeHz = float(190 + 330 * Env);
		// the deep: the crackle and the surge fall away and a low rumble is left
		B.Deep = float(.012 * Smooth(6, 28, In.DiverDepth));
		B.Lp = float(FMath::Lerp(9000.0, 4200.0, Smooth(3, 30, In.DiverDepth)));
		B.Master = 1.f;
		return B;
	}
}
