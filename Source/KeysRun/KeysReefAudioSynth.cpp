#include "KeysReefAudio.h"

// The audio-thread half of the reef and mangrove sound: the bed and the one-shot notes. The offline copy of this
// maths that draws the proof pictures is .marshal/requests/reefaudio/render.py: change the two together.
namespace
{
	constexpr int32 ReefSubBlock = 32;
	constexpr float ClickSecs = .010f;      // one snap: a 10 ms voice
	constexpr float ClickTau = .0012f;      // its noise tail dies in about a millisecond
}

UKeysReefSynth::UKeysReefSynth(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UKeysReefSynth::SetBed(const FKeysReefBed& B)
{
	FScopeLock L(&Lock);
	PendingBed = B;
}

void UKeysReefSynth::PushNote(const FKeysReefNote& N)
{
	FScopeLock L(&Lock);
	if (PendingNotes.Num() < 96) PendingNotes.Add(N);
}

void UKeysReefSynth::StartRec(double Seconds)
{
	if (RecLen.load() > 0) return;   // one at a time
	const int32 N = int32(FMath::Clamp(Seconds, .5, 120.0) * Rate) * 2;
	Rec.SetNumZeroed(N);
	RecPos.store(0);
	RecLen.store(N);
}

bool UKeysReefSynth::TakeRec(TArray<float>& Out, int32& OutRate)
{
	const int32 N = RecLen.load();
	if (N <= 0 || RecPos.load() < N) return false;
	Out = MoveTemp(Rec);
	Rec.Reset();
	OutRate = int32(Rate);
	RecLen.store(0);
	return true;
}

void UKeysReefSynth::TakeCost(double& OutCpu, double& OutAudio)
{
	OutCpu = CpuSecs.exchange(0.0);
	OutAudio = AudioSecs.exchange(0.0);
}

bool UKeysReefSynth::Init(int32& SampleRate)
{
	NumChannels = 2;
	Rate = SampleRate > 0 ? (float)SampleRate : 48000.f;
	Rng.Initialize((int32)(FPlatformTime::Cycles() & 0x7fffffff) ^ 0x52656566);
	KeysDSP::NoiseTable(); KeysDSP::SineTable();
	Notes.Reset();
	Notes.Reserve(KeysReefAudio::MaxNotes);
	for (FClick& C : Click) { C = FClick(); C.Noise.Seed(Rng); }
	SnapGain.Init(0, .2f); Sizzle.Init(0, .4f); Surge.Init(0, .15f); SurgeHz.Init(260.f, .15f); Deep.Init(0, .5f);
	Lp.Init(9000.f, .3f); Master.Init(0, .12f);
	SizzleN.Seed(Rng); SurgeN.Seed(Rng); DeepN.Seed(Rng);
	NextClick = .2;
	return true;
}

int32 UKeysReefSynth::OnGenerateAudio(float* OutAudio, int32 NumSamples)
{
	const double T0 = FPlatformTime::Seconds();
	const int32 Frames = NumSamples / 2;
	{
		// (the only allocation here is a note list that grows to its reserve once)
		FScopeLock L(&Lock);
		Bed = PendingBed;
		for (const FKeysReefNote& N : PendingNotes)
		{
			// at the cap the oldest note is dropped: no more than MaxNotes ever sound
			if (Notes.Num() >= KeysReefAudio::MaxNotes) Notes.RemoveAt(0, 1, EAllowShrinking::No);
			FNoteVoice& V = Notes.AddDefaulted_GetRef();
			V.N = N;
			V.N.Dur = FMath::Max(V.N.Dur, .004f);
			V.N.Att = FMath::Clamp(V.N.Att, .0003f, V.N.Dur * .9f);
			V.N.Rel = FMath::Clamp(V.N.Rel, 0.f, V.N.Dur - V.N.Att);
			V.Osc.Wave = N.Wave;
			V.Noise.Seed(Rng);
		}
		PendingNotes.Reset();
	}
	int32 Done = 0;
	while (Done < Frames)
	{
		const int32 N = FMath::Min(ReefSubBlock, Frames - Done);
		RenderBlock(OutAudio + Done * 2, N);
		Done += N;
	}
	float Pk = 0.f;
	for (int32 I = 0; I < Frames * 2; ++I)
	{
		OutAudio[I] = FMath::Clamp(OutAudio[I], -1.f, 1.f);
		Pk = FMath::Max(Pk, FMath::Abs(OutAudio[I]));
	}
	if (Pk > OutPeak.load()) OutPeak.store(Pk);
	NotesAlive.store(Notes.Num());
	if (Notes.Num() > NotesPeak.load()) NotesPeak.store(Notes.Num());
	if (const int32 RL = RecLen.load(); RL > 0)
	{
		const int32 P = RecPos.load();
		if (P < RL)
		{
			const int32 N = FMath::Min(RL - P, Frames * 2);
			FMemory::Memcpy(Rec.GetData() + P, OutAudio, N * sizeof(float));
			RecPos.store(P + N);
		}
	}
	CpuSecs.store(CpuSecs.load() + (FPlatformTime::Seconds() - T0));
	AudioSecs.store(AudioSecs.load() + double(Frames) / Rate);
	return NumSamples;
}

void UKeysReefSynth::RenderBlock(float* Out, int32 Frames)
{
	const float Dt = (float)Frames / Rate;
	SnapGain.Target = Bed.SnapGain; Sizzle.Target = Bed.Sizzle; Surge.Target = Bed.Surge; SurgeHz.Target = Bed.SurgeHz;
	Deep.Target = Bed.Deep; Lp.Target = Bed.Lp; Master.Target = Bed.Master;
	for (KeysDSP::FSmooth* S : {&SnapGain, &Sizzle, &Surge, &SurgeHz, &Deep, &Lp, &Master}) S->Step(Dt);
	for (int32 I = 0; I < Frames * 2; ++I) Out[I] = 0.f;

	const bool bBed = Master.V > 1e-4f || Master.Target > 1e-4f;
	if (bBed)
	{
		// ---- snapping shrimp: clicks arrive at random (a Poisson stream at SnapRate), each from its own place and
		// distance: most are faint and far, a few are near and sharp
		NextClick -= Dt;
		const float SnapRate = FMath::Clamp(Bed.SnapRate, 0.f, 80.f);
		while (NextClick <= 0)
		{
			if (SnapRate < .01f) { NextClick = .25; break; }
			NextClick += -FMath::Loge(FMath::Max(1e-6f, Rng.FRand())) / SnapRate;
			for (FClick& C : Click)
			{
				if (C.bOn) continue;
				C.bOn = true; C.Age = 0; C.Len = int32(ClickSecs * Rate);
				const float U = Rng.FRand();
				C.G = SnapGain.V * (.06f + .94f * U * U * U);
				const float Pan = Rng.FRand();
				C.L = FMath::Cos(Pan * UE_PI * .5f) * 1.41421f; C.R = FMath::Sin(Pan * UE_PI * .5f) * 1.41421f;
				C.Bp.Reset();
				// a far snap has lost its top: the faint ones ring lower
				C.Bp.Set(KeysDSP::EFilter::Bandpass, FMath::Lerp(2400.f, 7800.f, Rng.FRand() * (.45f + .55f * U)), FMath::Lerp(.8f, 2.4f, Rng.FRand()), Rate);
				Clicks.fetch_add(1);
				break;   // (all twelve busy: the click is not played)
			}
		}
		for (FClick& C : Click)
		{
			if (!C.bOn) continue;
			for (int32 F = 0; F < Frames; ++F)
			{
				const float T = (float)C.Age / Rate;
				const float X = (C.Age == 0 ? 4.f : 0.f) + C.Noise.Tick() * 2.f * FMath::Exp(-T / ClickTau);
				const float S = C.Bp.Tick(X) * C.G;
				Out[F * 2] += S * C.L;
				Out[F * 2 + 1] += S * C.R;
				if (++C.Age >= C.Len) { C.bOn = false; break; }
			}
		}
		// ---- the far crackle run together, the surge over the reef top, the deep rumble
		SizzleF.Set(KeysDSP::EFilter::Bandpass, 5200.f, .7f, Rate);
		SurgeF.Set(KeysDSP::EFilter::Lowpass, SurgeHz.V, 0.f, Rate);
		DeepF.Set(KeysDSP::EFilter::Lowpass, 70.f, 0.f, Rate);
		const float K = 1.f - FMath::Exp(-KeysDSP::TwoPi * FMath::Clamp(Lp.V, 200.f, 20000.f) / Rate);
		const float M = Master.V;
		for (int32 F = 0; F < Frames; ++F)
		{
			const float Mono = SizzleF.Tick(SizzleN.Tick()) * Sizzle.V + SurgeF.Tick(SurgeN.Tick()) * Surge.V + DeepF.Tick(DeepN.Tick()) * Deep.V;
			BedLpL += (Out[F * 2] + Mono - BedLpL) * K;
			BedLpR += (Out[F * 2 + 1] + Mono - BedLpR) * K;
			Out[F * 2] = BedLpL * M;
			Out[F * 2 + 1] = BedLpR * M;
		}
	}

	// ---- one-shot notes (not under the bed's master: the game thread decides what may be heard)
	for (int32 I = Notes.Num() - 1; I >= 0; --I)
	{
		bool bDone = false;
		RenderNote(Notes[I], Out, Frames, bDone);
		if (bDone) Notes.RemoveAt(I, 1, EAllowShrinking::No);
	}
}

void UKeysReefSynth::RenderNote(FNoteVoice& V, float* Out, int32 Frames, bool& bDone)
{
	const FKeysReefNote& N = V.N;
	const int32 DelayS = (int32)(N.Delay * Rate);
	const int32 Start = V.Age;
	V.Age += Frames;
	if (V.Age <= DelayS) return;
	const int32 Skip = FMath::Max(0, DelayS - Start);
	const float T0 = (float)(Start + Skip - DelayS) / Rate;
	const float T1 = (float)(V.Age - DelayS) / Rate;
	if (T0 >= N.Dur) { bDone = true; return; }

	auto Env = [&N](float T) -> float
	{
		if (T <= 0.f || T >= N.Dur) return 0.f;
		if (T < N.Att) return N.Gain * T / N.Att;
		if (N.Rel > 0.f) return T < N.Dur - N.Rel ? N.Gain : N.Gain * (N.Dur - T) / N.Rel;
		return KeysDSP::ExpRamp(N.Gain, .0001f, (T - N.Att) / FMath::Max(N.Dur - N.Att, .001f));
	};
	const float E0 = Env(T0), E1 = Env(T1);
	const int32 Count = Frames - Skip;
	const float EStep = Count > 0 ? (E1 - E0) / (float)Count : 0.f;
	const float U = T0 / N.Dur;

	float Inc = 0.f;
	if (N.bTone)
	{
		float Hz;
		if (N.F1 > 0.f) Hz = U < .5f ? KeysDSP::ExpRamp(N.F0, N.F1, U * 2.f) : KeysDSP::ExpRamp(N.F1, N.F2 > 0.f ? N.F2 : N.F1, (U - .5f) * 2.f);
		else Hz = N.F2 > 0.f ? KeysDSP::ExpRamp(N.F0, N.F2, U) : N.F0;
		if (N.Vib > 0.f)
		{
			Hz *= 1.f + N.Vib * KeysDSP::SineLookup(float(V.VibPh));
			V.VibPh += double(N.VibHz) * Frames / Rate;
			V.VibPh -= FMath::FloorToDouble(V.VibPh);
		}
		Inc = Hz / Rate;
	}
	const bool bFilt = !N.bTone || N.Fc0 > 0.f;
	if (bFilt)
	{
		const float Fc0 = N.Fc0 > 0.f ? N.Fc0 : 1000.f;
		V.Flt.Set(N.Filter, N.Fc1 > 0.f ? KeysDSP::ExpRamp(Fc0, N.Fc1, U) : Fc0, N.Q, Rate);
	}
	const float AmInc = N.Am > 0.f ? N.AmHz / Rate : 0.f;
	const float LpK = N.Lp > 0.f ? 1.f - FMath::Exp(-KeysDSP::TwoPi * N.Lp / Rate) : 1.f;
	float E = E0;
	for (int32 F = Skip; F < Frames; ++F)
	{
		float S = N.bTone ? V.Osc.Tick(Inc) : V.Noise.Tick();
		if (bFilt) S = V.Flt.Tick(S);
		if (AmInc > 0.f)
		{
			// the chop: full at the top of each cycle, down to 1 - Am between
			S *= 1.f - N.Am * (.5f + .5f * KeysDSP::SineLookup(float(V.AmPh)));
			V.AmPh += AmInc;
			if (V.AmPh >= 1.0) V.AmPh -= 1.0;
		}
		S *= E;
		V.LpZ += (S - V.LpZ) * LpK;
		Out[F * 2] += V.LpZ * N.L;
		Out[F * 2 + 1] += V.LpZ * N.R;
		E += EStep;
	}
	if (T1 >= N.Dur) bDone = true;
}
