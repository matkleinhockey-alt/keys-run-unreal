#include "KeysSfxSynth.h"


namespace
{
	constexpr int32 SfxSubBlock = 32;
	constexpr int32 SfxMaxVoices = 64;
}

UKeysSfxSynth::UKeysSfxSynth(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UKeysSfxSynth::SetEngineParams(const FKeysEngineParams& P)
{
	FScopeLock L(&ParamLock);
	PendingEngine = P;
	bEngineDirty = true;
}

void UKeysSfxSynth::SetLoopParams(const FKeysLoopParams& P)
{
	FScopeLock L(&ParamLock);
	PendingLoops = P;
	bLoopsDirty = true;
}

void UKeysSfxSynth::SetRemoteParams(const FKeysRemoteParams& P)
{
	FScopeLock L(&ParamLock);
	PendingRemote = P;
	bRemoteDirty = true;
}

void UKeysSfxSynth::PushBurst(const FKeysBurst& B)
{
	FScopeLock L(&ParamLock);
	if (PendingBursts.Num() < 128) PendingBursts.Add(B);
}

bool UKeysSfxSynth::Init(int32& SampleRate)
{
	NumChannels = 2;
	Rate = SampleRate > 0 ? (float)SampleRate : 48000.f;
	Rng.Initialize((int32)(FPlatformTime::Cycles() & 0x7fffffff));

	// touch the shared tables here so they are never built on the audio thread
	KeysDSP::NoiseTable(); KeysDSP::SineTable(); KeysDSP::EngineTable();

	CurNumOsc = 0;
	for (FEngineOsc& O : Osc)
	{
		O.Osc.Wave = KeysDSP::EWave::Engine; O.Osc.Phase = Rng.FRand();
		O.Lfo.Wave = KeysDSP::EWave::Sine;
		O.Freq.Init(11.7f, .04f); O.Gain.Init(0, .05f); O.LfoFreq.Init(5.8f, .05f); O.LfoDepth.Init(0, .08f);
	}
	MuffleF.Init(375.f, .06f); Relief.Init(0, .08f);
	Sub.Wave = KeysDSP::EWave::Sine; SubF.Init(23.f, .05f); SubG.Init(0, .08f);
	BubN.Seed(Rng); EnvN.Seed(Rng); BubG.Init(0, .1f); BubEd.Init(0, .1f);
	InsN.Seed(Rng); InsG.Init(0, .08f); InsF.Init(800.f, .1f);
	Gw.Wave = KeysDSP::EWave::Sine; GwF.Init(147.f, .06f); GwG.Init(0, .08f);
	Wh1.Wave = Wh2.Wave = KeysDSP::EWave::Sine; WhF.Init(315.f, .06f); WhG.Init(0, .08f);
	EngOut.Init(0, .07f);

	RushN.Seed(Rng); HissN.Seed(Rng); WindN.Seed(Rng); WashN.Seed(Rng); BreathN.Seed(Rng);
	for (FRemoteVoice& V : Rv) { V.A.Wave = KeysDSP::EWave::Saw; V.B.Wave = KeysDSP::EWave::Square; V.Fr.Init(40.f, .25f); V.G.Init(0, .25f); V.L.Init(.7f, .15f); V.R.Init(.7f, .15f); V.Cut.Init(700.f, .3f); } BreathG.Init(0, .3f); BreathD.Init(0, .3f); BreathFr.Init(480.f, .3f);
	RushG.Init(0, .08f); RushFr.Init(650.f, .1f); HissG.Init(0, .08f); WindG.Init(0, .2f); WindFr.Init(550.f, .2f); WashG.Init(0, .3f);
	ReelTone.Wave = KeysDSP::EWave::Square; ReelLfo.Wave = KeysDSP::EWave::Square;
	ReelRate.Init(30.f, .03f); ReelPitch.Init(2700.f, .05f); ReelDepth.Init(0, .03f);
	ReelBP.Set(KeysDSP::EFilter::Bandpass, 2600.f, 3.f, Rate);
	Master.Init(.8f, .05f);

	Voices.Reset();
	Voices.Reserve(SfxMaxVoices);
	Scratch.SetNumZeroed(SfxSubBlock * 2);
	return true;
}

void UKeysSfxSynth::PullParams()
{
	TArray<FKeysBurst> NewBursts;
	{
		FScopeLock L(&ParamLock);
		if (bEngineDirty) { Eng = PendingEngine; bEngineDirty = false; }
		if (bLoopsDirty) { Loops = PendingLoops; bLoopsDirty = false; }
		if (bRemoteDirty) { Remote = PendingRemote; bRemoteDirty = false; }
		if (PendingBursts.Num()) { NewBursts = MoveTemp(PendingBursts); PendingBursts.Reset(); }
	}
	for (const FKeysBurst& B : NewBursts)
	{
		if (Voices.Num() >= SfxMaxVoices) Voices.RemoveAt(0, 1, EAllowShrinking::No);
		FBurstVoice& V = Voices.AddDefaulted_GetRef();
		V.B = B;
		V.B.Dur = FMath::Max(V.B.Dur, .01f);
		V.B.Att = FMath::Clamp(V.B.Att, .0005f, V.B.Dur * .9f);
		V.Osc.Wave = B.bGull ? KeysDSP::EWave::Triangle : B.Wave;
		V.Vib.Wave = KeysDSP::EWave::Sine;
		V.Noise.Seed(Rng);
	}

	// engine voice layout: multi-engine rigs sit across the transom (StereoPanner, equal power)
	const int32 N = FMath::Clamp(Eng.NumOsc, 1, 4);
	if (N != CurNumOsc)
	{
		CurNumOsc = N;
		for (int32 I = 0; I < 4; ++I)
		{
			FEngineOsc& O = Osc[I];
			if (N > 1)
			{
				const float Half = (N - 1) * .5f;
				const float Pan = ((float)I - Half) / (Half > 0.f ? Half : 1.f) * .55f;
				const float X = (Pan + 1.f) * .5f;
				O.PanL = FMath::Cos(X * UE_PI * .5f);
				O.PanR = FMath::Sin(X * UE_PI * .5f);
			}
			else { O.PanL = O.PanR = 1.f; }
			O.Gain.V = 0.f;
		}
	}
}

int32 UKeysSfxSynth::OnGenerateAudio(float* OutAudio, int32 NumSamples)
{
	const int32 Ch = 2;
	const int32 Frames = NumSamples / Ch;
	PullParams();
	int32 Done = 0;
	while (Done < Frames)
	{
		const int32 N = FMath::Min(SfxSubBlock, Frames - Done);
		RenderBlock(OutAudio + Done * Ch, N);
		Done += N;
	}
	Comp.Process(OutAudio, Frames, Ch, Rate);
	return NumSamples;
}

void UKeysSfxSynth::RenderBlock(float* Out, int32 Frames)
{
	const float Dt = (float)Frames / Rate;
	const float InvRate = 1.f / Rate;

	// ---- glide every parameter toward its target (control rate)
	const int32 N = CurNumOsc;
	for (int32 I = 0; I < N; ++I)
	{
		FEngineOsc& O = Osc[I];
		O.Freq.Target = Eng.OscFreq[I]; O.Gain.Target = Eng.OscGain[I]; O.LfoFreq.Target = Eng.LfoFreq[I]; O.LfoDepth.Target = Eng.LfoDepth[I];
		O.Freq.Step(Dt); O.Gain.Step(Dt); O.LfoFreq.Step(Dt); O.LfoDepth.Step(Dt);
	}
	SubF.Target = Eng.SubFreq; SubG.Target = Eng.SubGain; MuffleF.Target = Eng.MuffleFreq; Relief.Target = Eng.ReliefGain;
	BubG.Target = Eng.BubGain; BubEd.Target = Eng.BubEnvDepth; InsG.Target = Eng.InsGain; InsF.Target = Eng.InsFreq;
	GwF.Target = Eng.GwFreq; GwG.Target = Eng.GwGain; WhF.Target = Eng.WhineFreq; WhG.Target = Eng.WhineGain; EngOut.Target = Eng.OutGain;
	for (KeysDSP::FSmooth* S : {&SubF, &SubG, &MuffleF, &Relief, &BubG, &BubEd, &InsG, &InsF, &GwF, &GwG, &WhF, &WhG, &EngOut}) S->Step(Dt);

	RushG.Target = Loops.RushGain; RushFr.Target = Loops.RushFreq; HissG.Target = Loops.HissGain; WindG.Target = Loops.WindGain;
	WindFr.Target = Loops.WindFreq; WashG.Target = Loops.WashGain;
	BreathG.Target = Loops.BreathGain; BreathD.Target = Loops.BreathDepth; BreathFr.Target = Loops.BreathFreq;
	for (KeysDSP::FSmooth* S : {&BreathG, &BreathD, &BreathFr}) S->Step(Dt);
	bool bRemote = false;
	for (int32 I = 0; I < 3; ++I)
	{
		FRemoteVoice& V = Rv[I]; const FKeysRemoteVoice& P = Remote.V[I];
		V.Fr.Target = P.Freq; V.G.Target = P.Gain; V.L.Target = P.PanL; V.R.Target = P.PanR; V.Cut.Target = P.Cut;
		for (KeysDSP::FSmooth* S : {&V.Fr, &V.G, &V.L, &V.R, &V.Cut}) S->Step(Dt);
		V.F.Set(KeysDSP::EFilter::Lowpass, V.Cut.V, .9f, Rate);
		bRemote |= V.G.V > 1e-5f || V.G.Target > 1e-5f;
	}
	ReelRate.Target = FMath::Max(1.f, Loops.ReelRate); ReelPitch.Target = Loops.ReelPitch; ReelDepth.Target = Loops.ReelDepth;
	Master.Target = Loops.Master;
	for (KeysDSP::FSmooth* S : {&RushG, &RushFr, &HissG, &WindG, &WindFr, &WashG, &ReelRate, &ReelPitch, &ReelDepth, &Master}) S->Step(Dt);

	// ---- filters (k-rate, like WebAudio's AudioParams on a biquad)
	MuffleL.Set(KeysDSP::EFilter::Lowpass, MuffleF.V, 1.1f, Rate); MuffleR.Set(KeysDSP::EFilter::Lowpass, MuffleF.V, 1.1f, Rate);
	ReliefL.Set(KeysDSP::EFilter::Highpass, 260.f, 1.f, Rate); ReliefR.Set(KeysDSP::EFilter::Highpass, 260.f, 1.f, Rate);
	BubBP.Set(KeysDSP::EFilter::Bandpass, 170.f, 1.2f, Rate); EnvLP.Set(KeysDSP::EFilter::Lowpass, 7.f, 1.f, Rate);
	InsBP.Set(KeysDSP::EFilter::Bandpass, InsF.V, 1.1f, Rate);
	RushF.Set(KeysDSP::EFilter::Lowpass, RushFr.V, .6f, Rate); HissF.Set(KeysDSP::EFilter::Highpass, 3200.f, .5f, Rate);
	WindF.Set(KeysDSP::EFilter::Bandpass, WindFr.V, .4f, Rate); WashF.Set(KeysDSP::EFilter::Lowpass, 380.f, .5f, Rate);
	BreathF.Set(KeysDSP::EFilter::Bandpass, BreathFr.V, .9f, Rate);
	const bool bBreath = BreathG.V > 1e-5f || BreathG.Target > 1e-5f;

	const float RaspK = 8.f + .8f * 40.f;   // audShaper(8 + rough * 40)
	const bool bEngineAudible = EngOut.V > 1e-5f || EngOut.Target > 1e-5f;

	for (int32 F = 0; F < Frames; ++F)
	{
		float L = 0.f, R = 0.f;

		if (bEngineAudible)
		{
			float CylL = 0.f, CylR = 0.f;
			for (int32 I = 0; I < N; ++I)
			{
				FEngineOsc& O = Osc[I];
				const float W = O.Osc.Tick(O.Freq.V * InvRate);
				const float Lope = 1.f + O.LfoDepth.V * O.Lfo.Tick(O.LfoFreq.V * InvRate);
				const float S = W * O.Gain.V * Lope;
				CylL += S * O.PanL;
				CylR += S * O.PanR;
			}
			float EL = MuffleL.Tick(CylL) + KeysDSP::Shape(ReliefL.Tick(CylL), RaspK) * Relief.V;
			float ER = MuffleR.Tick(CylR) + KeysDSP::Shape(ReliefR.Tick(CylR), RaspK) * Relief.V;
			float M = Sub.Tick(SubF.V * InvRate) * SubG.V;
			M += BubBP.Tick(BubN.Tick()) * (BubG.V + BubEd.V * EnvLP.Tick(EnvN.Tick()));
			M += InsBP.Tick(InsN.Tick()) * InsG.V;
			M += Gw.Tick(GwF.V * InvRate) * GwG.V;
			M += (Wh1.Tick(WhF.V * InvRate) + Wh2.Tick(WhF.V * 1.0015f * InvRate)) * WhG.V;
			L += (EL + M) * EngOut.V;
			R += (ER + M) * EngOut.V;
		}

		// water and wind loops
		float Amb = RushF.Tick(RushN.Tick()) * RushG.V;
		Amb += HissF.Tick(HissN.Tick()) * HissG.V;
		Amb += WindF.Tick(WindN.Tick()) * WindG.V;
		Amb += WashF.Tick(WashN.Tick()) * WashG.V;
		// breathing: band-passed noise swelling in and out on a 4.2 s inhale / exhale
		if (bBreath)
		{
			BreathPh += InvRate / 4.2;
			if (BreathPh >= 1) BreathPh -= 1;
			Amb += BreathF.Tick(BreathN.Tick()) * FMath::Max(0.f, BreathG.V + BreathD.V * FMath::Sin(float(BreathPh) * 2 * PI));
		}
		// the reel: an amplitude-chopped high square through a bandpass
		const float Chop = ReelDepth.V * ReelLfo.Tick(ReelRate.V * InvRate);
		Amb += ReelBP.Tick(ReelTone.Tick(ReelPitch.V * InvRate) * Chop);

		// other boats' engines
		if (bRemote)
			for (FRemoteVoice& V : Rv)
			{
				if (V.G.V < 1e-5f && V.G.Target < 1e-5f) continue;
				const float S = V.F.Tick(V.A.Tick(V.Fr.V * InvRate) * .6f + V.B.Tick(V.Fr.V * .5f * InvRate) * .4f) * V.G.V;
				L += S * V.L.V; R += S * V.R.V;
			}
		Out[F * 2] = L + Amb;
		Out[F * 2 + 1] = R + Amb;
	}

	// one-shots
	for (int32 I = Voices.Num() - 1; I >= 0; --I)
	{
		bool bDone = false;
		RenderBurst(Voices[I], Out, Frames, bDone);
		if (bDone) Voices.RemoveAt(I, 1, EAllowShrinking::No);
	}

	const float M = Master.V;
	for (int32 I = 0; I < Frames * 2; ++I) Out[I] *= M;
}

void UKeysSfxSynth::RenderBurst(FBurstVoice& V, float* Out, int32 Frames, bool& bDone)
{
	const FKeysBurst& B = V.B;
	const int32 DelayS = (int32)(B.Delay * Rate);
	const int32 Start = V.Age;
	V.Age += Frames;
	if (V.Age <= DelayS) return;
	const int32 Skip = FMath::Max(0, DelayS - Start);
	const float T0 = (float)(Start + Skip - DelayS) / Rate;
	const float T1 = (float)(V.Age - DelayS) / Rate;
	const float Dur = B.bGull ? .5f : B.Dur;
	if (T0 > Dur + .02f) { bDone = true; return; }

	auto Env = [&](float T) -> float
	{
		float G = B.bGull ? .12f : B.Gain, A = B.bGull ? .04f : B.Att;
		if (T <= 0.f) return 0.f;
		if (T < A) return G * T / A;
		if (T >= Dur) return 0.f;
		return KeysDSP::ExpRamp(G, .0001f, (T - A) / FMath::Max(Dur - A, .001f));
	};
	const float E0 = Env(T0), E1 = Env(T1);
	const int32 Count = Frames - Skip;
	const float EStep = Count > 0 ? (E1 - E0) / (float)Count : 0.f;

	if (B.bGull || B.bTone)
	{
		float Hz;
		if (B.bGull)
		{
			// 1350 -> 1750 over .08 s, -> 1050 at .45 s, with a 22 Hz warble of +-60 Hz
			Hz = T0 < .08f ? KeysDSP::ExpRamp(1350.f, 1750.f, T0 / .08f) : KeysDSP::ExpRamp(1750.f, 1050.f, (T0 - .08f) / .37f);
			Hz += 60.f * V.Vib.Tick(22.f * (float)Frames / Rate);
		}
		else Hz = B.ToneTo > 0.f ? KeysDSP::ExpRamp(B.Tone, B.ToneTo, T0 / Dur) : B.Tone;
		const float Inc = Hz / Rate;
		float E = E0;
		for (int32 F = Skip; F < Frames; ++F)
		{
			const float S = V.Osc.Tick(Inc) * E;
			Out[F * 2] += S * B.GainL;
			Out[F * 2 + 1] += S * B.GainR;
			E += EStep;
		}
	}
	else
	{
		const float Fq = B.FreqTo > 0.f ? KeysDSP::ExpRamp(B.Freq, B.FreqTo, T0 / Dur) : B.Freq;
		V.Flt.Set(B.Filter, Fq, B.Q, Rate);
		float E = E0;
		for (int32 F = Skip; F < Frames; ++F)
		{
			const float S = V.Flt.Tick(V.Noise.Tick()) * E;
			Out[F * 2] += S * B.GainL;
			Out[F * 2 + 1] += S * B.GainR;
			E += EStep;
		}
	}
	if (T1 > Dur + .02f) bDone = true;
}
