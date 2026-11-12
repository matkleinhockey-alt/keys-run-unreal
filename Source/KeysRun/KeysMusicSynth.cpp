#include "KeysMusicSynth.h"


namespace
{
	constexpr int32 MusSubBlock = 32;
	constexpr int32 MusMaxVoices = 192;

	// island song sections and degrees
	enum { SIntro, SVerse, SChorus, SBridge, SOutro };
	constexpr int32 DI = 0, Dii = 2, DIV = 5, DV = 7, Dvi = 9;
	const int32 Pent[5] = {0, 2, 4, 7, 9};

	const TCHAR* SongTitles[] = {TEXT("Low Tide, High Hopes"), TEXT("Sunburn Serenade"), TEXT("Conch Shell Sunday"), TEXT("Flip-Flop Forecast"),
		TEXT("Two-Stroke Tuesday"), TEXT("Mangrove Moon"), TEXT("Last Call at Mile Marker 50"), TEXT("Lime Wedge Lullaby"), TEXT("Hammock Weather"),
		TEXT("Tarpon Two-Step"), TEXT("Barefoot at the Bait Shop"), TEXT("Seven Mile Sundown"), TEXT("No Shoes, No Schedule"), TEXT("Salt Life Shuffle"),
		TEXT("Hawk Channel Breeze")};

	const TCHAR* HouseTitles[] = {TEXT("Mile Marker Midnight"), TEXT("Sandbar Sunrise"), TEXT("Tarpon Tide"), TEXT("Boot Key Groove"), TEXT("Seven Mile Jack"),
		TEXT("Neon Mangroves"), TEXT("Biscayne Afterglow"), TEXT("Reef Rave"), TEXT("Salt & Strobe"), TEXT("Hawk Channel Heat"), TEXT("Trim It Up"),
		TEXT("Wake Zone"), TEXT("Flats at Four AM"), TEXT("Sombrero Lights"), TEXT("Gulf Stream Pulse"), TEXT("No Wake, No Sleep"), TEXT("Bait Ball Bounce"),
		TEXT("Outrigger Disco"), TEXT("Blue Water Bounce"), TEXT("Pelican Drop")};

	// house
	enum { HIntro, HBuild, HDrop, HBreak, HRoll, HOutro };
	enum { SDeep, STech, SPiano, SDisco, STropical, SProg, NumStyles };
	enum class EChords : uint8 { PadStab, None, Piano, Filtered, Soft, Pad };
	enum class EBass : uint8 { Offbeat, Rolling, Octave, Disco };
	enum class EHats : uint8 { Soft, Ride, Open, Tamb, Shaker };
	enum class ELead : uint8 { None, Perc, Marimba, Arp };
	struct FStyle { int32 Bpm0, Bpm1; bool bMinor; EChords Chords; EBass Bass; EHats Hats; ELead Lead; };
	const FStyle Styles[NumStyles] = {
		{120, 122, true, EChords::PadStab, EBass::Offbeat, EHats::Soft, ELead::None},
		{125, 127, true, EChords::None, EBass::Rolling, EHats::Ride, ELead::Perc},
		{122, 124, false, EChords::Piano, EBass::Octave, EHats::Open, ELead::None},
		{120, 123, false, EChords::Filtered, EBass::Disco, EHats::Tamb, ELead::None},
		{108, 112, false, EChords::Soft, EBass::Offbeat, EHats::Shaker, ELead::Marimba},
		{124, 126, true, EChords::Pad, EBass::Offbeat, EHats::Ride, ELead::Arp}};
	const int32 ProgMinor[5][4] = {{0, -4, -7, -5}, {0, -2, -4, -2}, {0, 3, -2, -4}, {0, -4, 1, -2}, {0, -5, -2, -7}};
	const int32 ProgMajor[5][4] = {{0, 5, -3, 7}, {0, -3, 5, 7}, {0, 7, -3, 5}, {0, 4, 5, 7}, {0, -5, -3, -1}};

	// boom-bap
	const int32 BeatProg[4][4] = {{57, 60, 64, 67}, {50, 53, 57, 60}, {55, 59, 62, 66}, {48, 52, 55, 59}};
	const int32 BeatBass[4][3][2] = {{{45, 0}, {45, 6}, {52, 10}}, {{38, 0}, {38, 7}, {45, 12}}, {{43, 0}, {43, 6}, {50, 10}}, {{36, 0}, {40, 8}, {43, 12}}};
}

UKeysMusicSynth::UKeysMusicSynth(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

// ---------------------------------------------------------------- game thread

void UKeysMusicSynth::PlayStation(int32 K)
{
	CmdStation.store(K);
	CmdSerial.fetch_add(1);
}

void UKeysMusicSynth::SetOutGain(float Target, float Tau)
{
	CmdOutGain.store(Target);
	CmdOutTau.store(Tau);
}

void UKeysMusicSynth::SetSpatial(float GainL, float GainR, float InMaster)
{
	CmdL.store(GainL);
	CmdR.store(GainR);
	CmdMaster.store(InMaster);
}

FString UKeysMusicSynth::GetIslandTitle() const
{
	FScopeLock L(&TitleLock);
	return IslandTitle;
}

FString UKeysMusicSynth::GetHouseTitle() const
{
	FScopeLock L(&TitleLock);
	return HouseTitle;
}

// ---------------------------------------------------------------- audio thread

bool UKeysMusicSynth::Init(int32& SampleRate)
{
	NumChannels = 2;
	Rate = SampleRate > 0 ? (float)SampleRate : 48000.f;
	Rng.Initialize((int32)((FPlatformTime::Cycles() * 2654435761u) & 0x7fffffff));
	KeysDSP::NoiseTable(); KeysDSP::SineTable();
	Voices.Reset();
	Voices.Reserve(MusMaxVoices + 1);
	Mono.SetNumZeroed(MusSubBlock);
	OutG.Init(0.f, .2f);
	SpL.Init(1.f, .04f);
	SpR.Init(1.f, .04f);
	Mast.Init(.8f, .05f);
	Clock = 0;
	return true;
}

int32 UKeysMusicSynth::OnGenerateAudio(float* OutAudio, int32 NumSamples)
{
	const int32 Frames = NumSamples / 2;

	// commands from the game thread
	const int32 Serial = CmdSerial.load();
	if (Serial != SeenSerial)
	{
		SeenSerial = Serial;
		const int32 K = CmdStation.load();
		if (K >= 0 && K <= 2) { Station = K; bOn = true; NextStep = (double)Clock / Rate + .05; Step = 0; }
		else bOn = false;
	}
	OutG.Target = CmdOutGain.load();
	OutG.Tau = CmdOutTau.load();
	SpL.Target = CmdL.load();
	SpR.Target = CmdR.load();
	Mast.Target = CmdMaster.load();

	float SumSq = 0.f;
	int32 Done = 0;
	while (Done < Frames)
	{
		const int32 N = FMath::Min(MusSubBlock, Frames - Done);
		const double BlockEnd = (double)(Clock + N) / Rate;

		// beatScheduler: queue every step that starts before the end of this block
		int32 Guard = 0;
		while (bOn && NextStep < BlockEnd && Guard++ < 8)
		{
			const double Spb = 60.0 / StationBpm() / 4.0;
			const double Swing = (Step % 2) ? Spb * StationSwing() : 0.0;
			StepStation(NextStep + Swing, Step);
			NextStep += Spb;
			Step++;
		}

		FMemory::Memzero(Mono.GetData(), sizeof(float) * MusSubBlock);
		for (int32 I = Voices.Num() - 1; I >= 0; --I)
		{
			bool bDone = false;
			RenderVoice(Voices[I], Mono.GetData(), N, bDone);
			if (bDone) Voices.RemoveAt(I, 1, EAllowShrinking::No);
		}

		const float Dt = (float)N / Rate;
		OutG.Step(Dt); SpL.Step(Dt); SpR.Step(Dt); Mast.Step(Dt);
		float* O = OutAudio + Done * 2;
		for (int32 F = 0; F < N; ++F)
		{
			const float S = Mono[F] * OutG.V;
			SumSq += S * S;
			O[F * 2] = S * SpL.V * Mast.V;
			O[F * 2 + 1] = S * SpR.V * Mast.V;
		}
		Clock += N;
		Done += N;
	}
	Comp.Process(OutAudio, Frames, 2, Rate);

	// MUSIC.level: the analyser's low bins, roughly -100..-30 dB mapped to 0..1
	const float Rms = FMath::Sqrt(SumSq / FMath::Max(1, Frames));
	const float Db = 8.68588964f * FMath::Loge(FMath::Max(Rms, 1e-6f));
	const float Lv = bOn ? FMath::Clamp((Db + 100.f) / 70.f, 0.f, 1.f) : 0.f;
	LevelAcc += (Lv - LevelAcc) * .4f;
	LevelOut.store(LevelAcc);
	return NumSamples;
}

// ---------------------------------------------------------------- voices

UKeysMusicSynth::FMVoice& UKeysMusicSynth::NewVoice(double T)
{
	if (Voices.Num() >= MusMaxVoices) Voices.RemoveAt(0, 1, EAllowShrinking::No);
	FMVoice& V = Voices.AddDefaulted_GetRef();
	V.Start = (int64)(T * Rate);
	return V;
}

void UKeysMusicSynth::Hit(double T, const FHitOpts& O)
{
	// bHit(): a noise hit through one filter, or an oscillator with an optional pitch drop, detune and lowpass
	FMVoice& V = NewVoice(T);
	V.Gain = V.Gain2 = O.Gain;
	V.Att = V.HoldT = FMath::Max(O.Att, .0005f);
	V.EndT = FMath::Max(O.Dur, V.Att + .001f);
	if (O.bNoise)
	{
		V.bNoise = true;
		V.Noise.Seed(Rng);
		V.bFilter = true; V.FType = O.Type; V.FF0 = O.Freq; V.FQ = O.Q;
	}
	else
	{
		V.NumOsc = 1;
		V.Osc[0].Wave = O.Wave;
		V.Mul[0] = O.Det != 0.f ? KeysDSP::CentsToMul(O.Det) : 1.f;
		V.F0 = O.F0; V.F1 = O.F1; V.RampDur = O.Dur * .6f;
		if (O.Lp > 0.f) { V.bFilter = true; V.FType = KeysDSP::EFilter::Lowpass; V.FF0 = O.Lp; V.FQ = 1.f; }
	}
}

void UKeysMusicSynth::SteelPan(double T, float M, float Dur, float Gain)
{
	FMVoice& V = NewVoice(T);
	V.NumOsc = 4;
	const float K[4] = {1.f, 2.f, 3.98f, 5.1f}, A[4] = {1.f, .42f, .16f, .06f};
	for (int32 I = 0; I < 4; ++I) { V.Osc[I].Wave = KeysDSP::EWave::Sine; V.Mul[I] = K[I]; V.Amp[I] = A[I]; }
	V.F0 = KeysDSP::MidiToHz(M);
	V.Gain = V.Gain2 = Gain; V.Att = V.HoldT = .006f; V.EndT = FMath::Max(Dur, .01f);
}

void UKeysMusicSynth::Pluck(double T, float M, float Dur, float Gain)
{
	FMVoice& V = NewVoice(T);
	V.NumOsc = 2;
	V.Osc[0].Wave = KeysDSP::EWave::Saw; V.Amp[0] = 1.f;
	V.Osc[1].Wave = KeysDSP::EWave::Triangle; V.Amp[1] = 1.f; V.Mul[1] = KeysDSP::CentsToMul(6.f);
	V.F0 = KeysDSP::MidiToHz(M);
	V.bFilter = true; V.FType = KeysDSP::EFilter::Lowpass; V.FQ = 1.5f; V.FF0 = 3800.f; V.FF1 = 420.f; V.FRamp = .35f;
	V.Gain = V.Gain2 = Gain; V.Att = V.HoldT = .003f; V.EndT = FMath::Max(Dur, .01f);
}

void UKeysMusicSynth::Strum(double T, const int32 Ch[3], bool bDown, float Dur, float Gain)
{
	if (Gain <= 0.f) return;
	const int32 Vs[6] = {Ch[0] - 12, Ch[2] - 12, Ch[0], Ch[1], Ch[2], Ch[0] + 12};
	for (int32 K = 0; K < 6; ++K)
	{
		const int32 Note = bDown ? Vs[K] : Vs[5 - K];
		Pluck(T + K * .011, (float)Note, Dur, Gain * (bDown ? 1.f : .7f));
	}
}

void UKeysMusicSynth::Harmonica(double T, float M, float Dur, float Gain)
{
	FMVoice& V = NewVoice(T);
	V.NumOsc = 2;
	V.Osc[0].Wave = KeysDSP::EWave::Square; V.Amp[0] = 1.f; V.bVib[0] = true;
	V.Osc[1].Wave = KeysDSP::EWave::Saw; V.Amp[1] = 1.f; V.Mul[1] = KeysDSP::CentsToMul(-9.f); V.bVib[1] = true;
	V.F0 = KeysDSP::MidiToHz(M);
	V.VibRate = 5.4f; V.VibDepth = V.F0 * .006f;
	V.bFilter = true; V.FType = KeysDSP::EFilter::Bandpass; V.FF0 = 1500.f; V.FQ = 1.1f;
	V.Gain = Gain; V.Att = .035f; V.HoldT = FMath::Max(.05f, Dur - .08f); V.Gain2 = Gain * .85f; V.EndT = Dur + .12f;
	if (V.EndT <= V.HoldT) V.EndT = V.HoldT + .05f;
}

void UKeysMusicSynth::PedalSteel(double T, float M, float Dur, float Gain)
{
	FMVoice& V = NewVoice(T);
	const float F = KeysDSP::MidiToHz(M);
	V.NumOsc = 2;
	V.Osc[0].Wave = KeysDSP::EWave::Saw; V.Amp[0] = 1.f; V.bVib[0] = true;
	V.Osc[1].Wave = KeysDSP::EWave::Sine; V.Amp[1] = 1.f;
	V.F0 = F * .89f; V.F1 = F; V.RampDur = .22f;
	V.VibRate = 5.f; V.VibDepth = F * .004f;
	V.bFilter = true; V.FType = KeysDSP::EFilter::Lowpass; V.FF0 = 1600.f; V.FQ = 1.f;
	V.Gain = V.Gain2 = Gain; V.Att = V.HoldT = .12f; V.EndT = Dur + .4f;
}

void UKeysMusicSynth::RenderVoice(FMVoice& V, float* Out, int32 Frames, bool& bDone)
{
	const int64 BlockStart = Clock;
	const int64 BlockEnd = Clock + Frames;
	if (V.Start >= BlockEnd) return;
	const int32 Skip = (int32)FMath::Clamp<int64>(V.Start - BlockStart, 0, Frames);
	const float T0 = (float)(BlockStart + Skip - V.Start) / Rate;
	const float T1 = (float)(BlockEnd - V.Start) / Rate;
	if (T0 >= V.EndT) { bDone = true; return; }

	auto Env = [&V](float T) -> float
	{
		if (T <= 0.f) return 0.f;
		if (T < V.Att) return V.Gain * T / V.Att;
		if (T < V.HoldT) return V.Gain;
		if (T >= V.EndT) return 0.f;
		return KeysDSP::ExpRamp(V.Gain2, .0001f, (T - V.HoldT) / FMath::Max(V.EndT - V.HoldT, .001f));
	};
	const float E0 = Env(T0), E1 = Env(T1);
	const int32 Count = Frames - Skip;
	const float EStep = Count > 0 ? (E1 - E0) / (float)Count : 0.f;

	if (V.bFilter)
	{
		const float Fq = (V.FF1 > 0.f && V.FRamp > 0.f) ? KeysDSP::ExpRamp(V.FF0, V.FF1, T0 / V.FRamp) : V.FF0;
		V.Flt.Set(V.FType, Fq, V.FQ, Rate);
	}

	float E = E0;
	if (V.bNoise)
	{
		for (int32 F = Skip; F < Frames; ++F)
		{
			Out[F] += V.Flt.Tick(V.Noise.Tick()) * E;
			E += EStep;
		}
	}
	else
	{
		const float Base = (V.F1 > 0.f && V.RampDur > 0.f) ? KeysDSP::ExpRamp(V.F0, V.F1, T0 / V.RampDur) : V.F0;
		float Vib = 0.f;
		if (V.VibRate > 0.f) Vib = V.VibDepth * V.Vib.Tick(V.VibRate * (float)Frames / Rate);
		float Inc[4];
		for (int32 K = 0; K < V.NumOsc; ++K) Inc[K] = (Base + (V.bVib[K] ? Vib : 0.f)) * V.Mul[K] / Rate;
		for (int32 F = Skip; F < Frames; ++F)
		{
			float S = 0.f;
			for (int32 K = 0; K < V.NumOsc; ++K) S += V.Osc[K].Tick(Inc[K]) * V.Amp[K];
			if (V.bFilter) S = V.Flt.Tick(S);
			Out[F] += S * E;
			E += EStep;
		}
	}
	if (T1 >= V.EndT) bDone = true;
}

// ---------------------------------------------------------------- stations

float UKeysMusicSynth::StationBpm() const
{
	if (Station == 0) return bHaveSong ? (float)Song.Bpm : 104.f;
	if (Station == 1) return 90.f;
	return bHaveHouse ? (float)House.Bpm : 124.f;
}

float UKeysMusicSynth::StationSwing() const
{
	return Station == 0 ? .06f : Station == 1 ? .18f : 0.f;
}

void UKeysMusicSynth::StepStation(double T, int32 I)
{
	if (Station == 0) IslandStep(T, I);
	else if (Station == 1) BeatStep(T, I);
	else HouseStep(T, I);
}

// 90 BPM boom-bap: dusty kick and snare, swung hats, a minor-seventh soul progression on a warm electric piano, walking bass, vinyl crackle
void UKeysMusicSynth::BeatStep(double T, int32 I)
{
	const int32 St = I % 16, Bar = (I / 16) % 4;
	FHitOpts O;
	if (St == 0 || St == 7 || St == 10) { O = FHitOpts(); O.F0 = 120; O.F1 = 42; O.Dur = .42f; O.Gain = .9f; Hit(T, O); }
	if (St == 4 || St == 12)
	{
		O = FHitOpts(); O.bNoise = true; O.Type = KeysDSP::EFilter::Bandpass; O.Freq = 1800; O.Q = .8f; O.Dur = .22f; O.Gain = .55f; Hit(T, O);
		O = FHitOpts(); O.F0 = 190; O.F1 = 140; O.Dur = .12f; O.Gain = .35f; O.Wave = KeysDSP::EWave::Triangle; Hit(T, O);
	}
	if (St % 2 == 0 || Rnd() < .18f)
	{
		O = FHitOpts(); O.bNoise = true; O.Freq = 7000; O.Dur = St % 4 == 2 ? .09f : .04f; O.Gain = St % 4 == 2 ? .16f : .09f; Hit(T, O);
	}
	if (St == 0 || St == 8)
	{
		for (int32 N : BeatProg[Bar])
			for (float D : {-6.f, 6.f})
			{
				O = FHitOpts(); O.F0 = KeysDSP::MidiToHz((float)N); O.Dur = St == 0 ? 1.25f : .6f; O.Gain = .07f; O.Wave = KeysDSP::EWave::Triangle; O.Det = D; O.Att = .01f; O.Lp = 1800; Hit(T, O);
			}
	}
	for (int32 K = 0; K < 3; ++K)
		if (BeatBass[Bar][K][1] == St)
		{
			O = FHitOpts(); O.F0 = KeysDSP::MidiToHz((float)BeatBass[Bar][K][0]); O.Dur = .38f; O.Gain = .38f; O.Att = .008f; Hit(T, O);
		}
	if (Rnd() < .35f)
	{
		O = FHitOpts(); O.bNoise = true; O.Freq = 3000; O.Dur = .012f; O.Gain = .05f; Hit(T + Rnd() * .1, O);
	}
}

// ---- Margaritaville: original laid-back island songs, written on the fly
void UKeysMusicSynth::NewSong(int32 StartStep)
{
	const int32 Keys[5] = {62, 64, 57, 59, 55};
	Song = FIslandSong();
	Song.Key = Keys[Pick(5)];
	Song.Bpm = FMath::RoundToInt(Rand(96.f, 112.f));
	Song.Start = StartStep;
	const int32 Key = Song.Key;

	auto MkLead = [&](const TArray<int32>& Prog) -> int32
	{
		static const TArray<TArray<int32>> Motifs = {{0, 4, 6, 8}, {0, 3, 6, 10, 12}, {2, 4, 6, 8, 12}, {0, 2, 4, 8, 10}, {4, 6, 8, 12, 14}};
		const TArray<int32>& Motif = Motifs[Pick(5)];
		int32 Note = Key + 12 + Pent[Pick(3)];
		TArray<int32> Pool;
		for (int32 Oc = 0; Oc < 2; ++Oc) for (int32 P : Pent) Pool.Add(Key + 12 + Oc * 12 + P);
		TArray<FLeadNote> PhraseA;
		bool bHavePhrase = false;
		const int32 First = Song.LeadBars.Num();
		for (int32 B = 0; B < Prog.Num(); ++B)
		{
			const int32 Deg = Prog[B];
			const int32 Ct[3] = {Key + 12 + Deg, Key + 12 + Deg + ((Deg == Dvi || Deg == Dii) ? 3 : 4), Key + 12 + Deg + 7};
			TArray<FLeadNote> Ev;
			if (B % 4 == 2 && bHavePhrase)
			{
				for (int32 E = 0; E < PhraseA.Num(); ++E)
				{
					FLeadNote L = PhraseA[E];
					const int32 Shift = Rnd() < .5f ? 2 : -3;
					if (E == PhraseA.Num() - 1) L.Note += Shift;
					Ev.Add(L);
				}
			}
			else
			{
				TArray<int32> Steps = Motif;
				if (B % 2 == 1) Steps.SetNum(FMath::Max(2, Motif.Num() - 2));
				for (int32 K = 0; K < Steps.Num(); ++K)
				{
					const bool bLast = K == Steps.Num() - 1;
					TArray<int32> Cand;
					for (int32 N : Pool) if (FMath::Abs(N - Note) <= 5) Cand.Add(N);
					if (bLast)
					{
						TArray<int32> Ok;
						for (int32 N : Cand) { for (int32 C : Ct) if ((N - C) % 12 == 0) { Ok.Add(N); break; } }
						if (Cand.Num() == 0) Ok.Add(Ct[0]);
						Cand = Ok;
					}
					if (Cand.Num() == 0) Cand.Add(Ct[0]);
					Note = Cand[Pick(Cand.Num())];
					Ev.Add({Steps[K], Note, bLast ? 16 - Steps[K] : 2});
				}
				if (B % 4 == 0) { PhraseA = Ev; bHavePhrase = true; }
			}
			Song.LeadBars.Add(Ev);
		}
		return First;
	};

	const TArray<int32> Intro = {DI, DIV, DI, DV};
	const TArray<int32> Verse = {DI, DI, DIV, DI, DI, DV, DIV, DI};
	const TArray<int32> Chorus = {DIV, DI, DV, DI, DIV, DI, DV, DI};
	const TArray<int32> Bridge = {Dvi, DIV, DI, DV};
	const TArray<int32> Outro = {DIV, DV, DI, DI};
	const int32 VLead = MkLead(Verse), CLead = MkLead(Chorus), BLead = MkLead(Bridge);

	struct FSec { int32 Sec; const TArray<int32>* P; int32 Lead; };
	const FSec Secs[] = {{SIntro, &Intro, -1}, {SVerse, &Verse, VLead}, {SChorus, &Chorus, CLead}, {SVerse, &Verse, VLead}, {SChorus, &Chorus, CLead},
		{SBridge, &Bridge, BLead}, {SChorus, &Chorus, CLead}, {SOutro, &Outro, -1}};
	for (const FSec& S : Secs)
		for (int32 B = 0; B < S.P->Num(); ++B)
			Song.Bars.Add({S.Sec, (*S.P)[B], S.Lead >= 0 ? S.Lead + B : -1, B == S.P->Num() - 1});
	bHaveSong = true;

	FScopeLock L(&TitleLock);
	IslandTitle = SongTitles[Pick((int32)UE_ARRAY_COUNT(SongTitles))];
}

void UKeysMusicSynth::ChordOf(int32 Deg, int32 Out[3]) const
{
	const int32 R = Song.Key + Deg;
	const bool bMinor = Deg == 2 || Deg == 9;
	Out[0] = R; Out[1] = R + (bMinor ? 3 : 4); Out[2] = R + 7;
}

void UKeysMusicSynth::IslandStep(double T, int32 I)
{
	if (!bHaveSong) NewSong(I);
	int32 Si = I - Song.Start;
	if (Si < 0 || Si >= Song.Bars.Num() * 16) { NewSong(I); Si = 0; }
	const int32 St = Si % 16, BarI = Si / 16;
	const FIslandBar& B = Song.Bars[BarI];
	int32 Ch[3]; ChordOf(B.Deg, Ch);
	const float Spb = 60.f / Song.Bpm / 4.f;
	const int32 Sec = B.Sec;
	const FIslandBar& Nxt = Song.Bars.IsValidIndex(BarI + 1) ? Song.Bars[BarI + 1] : B;
	FHitOpts O;

	// drums: kick on 1 and 3, cross-stick on 2 and 4, shaker eighths, conga fills at section ends
	if (St == 0 || St == 8) { O = FHitOpts(); O.F0 = 100; O.F1 = 46; O.Dur = .3f; O.Gain = Sec == SIntro ? .3f : .55f; Hit(T, O); }
	if ((St == 4 || St == 12) && Sec != SIntro)
	{
		O = FHitOpts(); O.bNoise = true; O.Type = KeysDSP::EFilter::Bandpass; O.Freq = 2300; O.Q = 3; O.Dur = .05f; O.Gain = .3f; Hit(T, O);
		O = FHitOpts(); O.F0 = 820; O.Dur = .04f; O.Gain = .08f; O.Wave = KeysDSP::EWave::Triangle; Hit(T, O);
	}
	if (St % 2 == 0) { O = FHitOpts(); O.bNoise = true; O.Freq = 6800; O.Dur = .05f; O.Gain = St % 4 == 2 ? .07f : .04f; Hit(T, O); }
	if (B.bLast && St >= 10)
	{
		const float Cf[3] = {230.f, 190.f, 170.f};
		O = FHitOpts(); O.F0 = Cf[St % 3]; O.F1 = 150; O.Dur = .16f; O.Gain = .18f; Hit(T, O);
	}
	if (Sec == SChorus && St == 0 && Rnd() < .5f) { O = FHitOpts(); O.F0 = 2600; O.Dur = .6f; O.Gain = .04f; Hit(T, O); }

	// acoustic guitar: D D U U D U on the eighths
	int32 Pat = -1;
	switch (St) { case 0: case 4: case 12: Pat = 1; break; case 6: case 10: case 14: Pat = 0; break; default: break; }
	if (Pat >= 0)
	{
		const bool bSilent = Sec == SOutro && BarI == Song.Bars.Num() - 1 && St > 0;
		Strum(T, Ch, Pat == 1, St == 0 ? .9f : .4f, bSilent ? 0.f : .035f);
	}

	// bass: root and fifth with a walk-up into the next chord
	int32 NCh[3]; ChordOf(Nxt.Deg, NCh);
	const int32 Root = Ch[0] - 24, NRoot = NCh[0] - 24;
	auto Bass = [&](int32 M, float Dur, float Gain)
	{
		FHitOpts B2; B2.F0 = KeysDSP::MidiToHz((float)M); B2.Dur = Dur; B2.Gain = Gain; B2.Wave = KeysDSP::EWave::Triangle; B2.Lp = 520; B2.Att = .006f; Hit(T, B2);
	};
	if (St == 0) Bass(Root, .42f, .36f);
	if (St == 8) Bass(Root + 7, .32f, .32f);
	if (St == 12 || St == 14)
	{
		const int32 W = St == 12 ? (NRoot > Root ? NRoot - 2 : NRoot + 2) : (NRoot > Root ? NRoot - 1 : NRoot + 1);
		Bass(W, .18f, .28f);
	}

	// the "singer": harmonica on the verses, harmonica plus steel drum on the choruses, pedal steel on the bridge
	if (B.Lead >= 0 && Song.LeadBars.IsValidIndex(B.Lead))
	{
		for (const FLeadNote& L : Song.LeadBars[B.Lead])
		{
			if (L.Step != St) continue;
			const float Dur = L.Len * Spb;
			if (Sec == SBridge) PedalSteel(T, (float)L.Note, Dur, .075f);
			else
			{
				Harmonica(T, (float)L.Note, Dur, Sec == SChorus ? .06f : .07f);
				if (Sec == SChorus) SteelPan(T, (float)(L.Note + 12), FMath::Min(.8f, Dur + .2f), .07f);
			}
		}
	}
	if (Sec == SIntro && St % 4 == 0 && Rnd() < .7f) SteelPan(T, (float)(Ch[Pick(3)] + 12), .6f, .08f);
	if (Sec == SChorus && St == 0)
		for (int32 N : Ch) { O = FHitOpts(); O.F0 = KeysDSP::MidiToHz((float)N); O.Dur = 60.f / Song.Bpm * 3.6f; O.Gain = .025f; O.Att = .2f; Hit(T, O); }
}

// ---- Beno Bonanza: endless original house tracks, each in its own style, key, tempo and arrangement
void UKeysMusicSynth::NewHouse(int32 StartStep)
{
	int32 Style = Pick(NumStyles);
	if (bHaveHouse && Style == House.Style) Style = (Style + 1) % NumStyles;
	const FStyle& S = Styles[Style];
	House = FHouseSong();
	House.Style = Style;
	House.Root = 45 + Pick(8);
	House.Bpm = FMath::RoundToInt(Rand((float)S.Bpm0, (float)S.Bpm1));
	House.Start = StartStep;
	const int32* P = S.bMinor ? ProgMinor[Pick(5)] : ProgMajor[Pick(5)];
	for (int32 K = 0; K < 4; ++K)
	{
		const int32 O = P[K], R = House.Root + O;
		const bool bMinorCh = S.bMinor ? (K != 2 || Rnd() < .6f) : ((O == -3 || O == 2 || O == 4) && O != 0);
		House.Chords[K][0] = R; House.Chords[K][1] = R + (bMinorCh ? 3 : 4); House.Chords[K][2] = R + 7; House.Chords[K][3] = R + (bMinorCh ? 10 : 11);
	}
	static const TArray<TArray<int32>> Rx = {{0, 3, 6, 10}, {2, 6, 10, 14}, {0, 6, 8, 14}, {3, 6, 11, 14}, {0, 3, 8, 11, 14}};
	House.StabRx = Rx[Pick(5)];
	for (int32 K = 0; K < 8; ++K) { House.Motif[K][0] = K * 2 + (Rnd() < .3f ? 1 : 0); House.Motif[K][1] = Pick(4); }
	const int32 Secs[7][2] = {{HIntro, 16}, {HBuild, 8}, {HDrop, 32}, {HBreak, 8}, {HRoll, 4}, {HDrop, 32}, {HOutro, 8}};
	for (const auto& Sc : Secs)
		for (int32 B = 0; B < Sc[1]; ++B) House.Bars.Add({Sc[0], B, Sc[1]});
	bHaveHouse = true;

	FScopeLock L(&TitleLock);
	HouseTitle = HouseTitles[Pick((int32)UE_ARRAY_COUNT(HouseTitles))];
}

void UKeysMusicSynth::HouseStep(double T, int32 I)
{
	if (!bHaveHouse) NewHouse(I);
	int32 Si = I - House.Start;
	if (Si < 0 || Si >= House.Bars.Num() * 16) { NewHouse(I); Si = 0; }
	const FHouseSong& H = House;
	const FStyle& S = Styles[H.Style];
	const int32 St = Si % 16;
	const FHouseBar& B = H.Bars[Si / 16];
	const int32* Ch = H.Chords[(Si / 16) % 4];
	const int32 N = B.Sec;
	const float Spb = 60.f / H.Bpm / 4.f;
	const bool bKick = N != HBreak && N != HRoll, bFull = N == HDrop;
	const bool bBassOn = N == HBuild || N == HDrop || (N == HOutro && B.B < 4);
	const bool bChordsOn = N == HDrop || N == HBreak || (N == HBuild && B.B >= 4);
	const bool bLeadOn = N == HDrop && B.B >= 8;
	const bool bTropical = H.Style == STropical;
	FHitOpts O;

	// drums
	if (bKick && St % 4 == 0) { O = FHitOpts(); O.F0 = S.bMinor ? 120.f : 132.f; O.F1 = 44; O.Dur = .3f; O.Gain = bTropical ? .7f : .95f; Hit(T, O); }
	if ((St == 4 || St == 12) && (bFull || N == HBuild || N == HOutro))
	{
		if (bTropical) { O = FHitOpts(); O.bNoise = true; O.Type = KeysDSP::EFilter::Bandpass; O.Freq = 2600; O.Q = 2; O.Dur = .06f; O.Gain = .32f; Hit(T, O); }
		else
		{
			O = FHitOpts(); O.bNoise = true; O.Type = KeysDSP::EFilter::Bandpass; O.Freq = 1400; O.Q = 1.2f; O.Dur = .14f; O.Gain = .4f; Hit(T, O);
			O = FHitOpts(); O.bNoise = true; O.Type = KeysDSP::EFilter::Bandpass; O.Freq = 1750; O.Q = 1.2f; O.Dur = .18f; O.Gain = .25f; Hit(T + .012, O);
		}
	}
	if ((St % 4 == 2 && N != HIntro) || (St % 4 == 2 && B.B >= 8))
	{
		O = FHitOpts(); O.bNoise = true; O.Freq = 8500; O.Dur = S.Hats == EHats::Open ? .16f : .1f; O.Gain = N == HBreak ? .05f : .12f; Hit(T, O);
	}
	if (S.Hats == EHats::Ride && bFull && St % 2 == 0)
	{
		O = FHitOpts(); O.F0 = 5200; O.Dur = .25f; O.Gain = .012f; O.Wave = KeysDSP::EWave::Square; Hit(T, O);
		O = FHitOpts(); O.bNoise = true; O.Freq = 9000; O.Dur = .2f; O.Gain = .04f; Hit(T, O);
	}
	if (S.Hats == EHats::Tamb && (bFull || N == HBuild))
	{
		O = FHitOpts(); O.bNoise = true; O.Type = KeysDSP::EFilter::Bandpass; O.Freq = 7000; O.Q = 2; O.Dur = .05f; O.Gain = St % 2 ? .06f : .035f; Hit(T, O);
	}
	if (S.Hats == EHats::Shaker || S.Hats == EHats::Soft)
	{
		O = FHitOpts(); O.bNoise = true; O.Freq = 9500; O.Dur = .03f; O.Gain = St % 2 ? .035f : .055f; Hit(T, O);
	}
	if (bFull && (St == 3 || St == 7 || St == 10 || St == 13) && (H.Style == SDeep || H.Style == STropical || H.Style == STech))
	{
		O = FHitOpts(); O.F0 = St % 2 ? 260.f : 195.f; O.F1 = St % 2 ? 210.f : 160.f; O.Dur = .16f; O.Gain = .11f; Hit(T, O);
	}

	// bass
	if (bBassOn)
	{
		const int32 R = Ch[0] - 12;
		int32 Note = 0; bool bNote = false; float Dur = .2f;
		if (S.Bass == EBass::Offbeat && (St % 4 == 2 || St == 15)) { Note = R + (St == 15 ? 7 : 0); bNote = true; }
		else if (S.Bass == EBass::Rolling && St % 2 == 1) { Note = R + ((St == 7 || St == 15) ? (St == 7 ? 3 : -2) : 0); Dur = .11f; bNote = true; }
		else if (S.Bass == EBass::Octave && (St % 4 == 2 || St == 7 || St == 11)) { Note = R + (St == 7 ? 12 : 0); bNote = true; }
		else if (S.Bass == EBass::Disco && St % 2 == 0) { Note = R + (St % 4 == 2 ? 12 : 0) + (St == 14 ? 7 : 0); Dur = .14f; bNote = true; }
		if (bNote)
		{
			O = FHitOpts(); O.F0 = KeysDSP::MidiToHz((float)Note); O.Dur = Dur; O.Gain = .32f; O.Wave = KeysDSP::EWave::Saw; O.Lp = H.Style == STech ? 320.f : 480.f; O.Att = .004f; Hit(T, O);
			O = FHitOpts(); O.F0 = KeysDSP::MidiToHz((float)Note); O.Dur = Dur; O.Gain = .2f; O.Att = .004f; Hit(T, O);
		}
	}

	// chords
	if (bChordsOn)
	{
		const EChords C = S.Chords;
		if ((C == EChords::Pad || C == EChords::PadStab || N == HBreak) && St == 0)
			for (int32 K = 0; K < 4; ++K)
			{
				O = FHitOpts(); O.F0 = KeysDSP::MidiToHz((float)(Ch[K] + 12)); O.Dur = Spb * 16 * .98f; O.Gain = N == HBreak ? .045f : .022f; O.Wave = KeysDSP::EWave::Saw;
				O.Lp = N == HBreak ? 1500.f : 900.f; O.Att = .3f; Hit(T, O);
			}
		if ((C == EChords::PadStab || C == EChords::Piano) && H.StabRx.Contains(St) && N != HBreak)
		{
			const bool bPiano = C == EChords::Piano;
			for (int32 K = 0; K < 4; ++K)
			{
				for (float D : {-6.f, 6.f})
				{
					O = FHitOpts(); O.F0 = KeysDSP::MidiToHz((float)(Ch[K] + (bPiano ? 24 : 12))); O.Dur = bPiano ? .35f : .2f; O.Gain = bPiano ? .055f : .045f;
					O.Wave = KeysDSP::EWave::Triangle; O.Det = D; O.Lp = bPiano ? 4200.f : 2600.f; O.Att = .003f; Hit(T, O);
				}
				if (bPiano) { O = FHitOpts(); O.F0 = KeysDSP::MidiToHz((float)(Ch[K] + 36)); O.Dur = .15f; O.Gain = .012f; Hit(T, O); }
			}
		}
		if (C == EChords::Filtered && St % 4 == 2 && N != HBreak)
			for (int32 K = 0; K < 4; ++K)
			{
				O = FHitOpts(); O.F0 = KeysDSP::MidiToHz((float)(Ch[K] + 12)); O.Dur = .16f; O.Gain = .05f; O.Wave = KeysDSP::EWave::Saw;
				O.Lp = 900.f + 700.f * FMath::Sin((float)Si / 64.f * KeysDSP::TwoPi); O.Att = .004f; Hit(T, O);
			}
		if (C == EChords::Soft && (St == 0 || St == 10) && N != HBreak)
			for (int32 K = 0; K < 4; ++K)
			{
				O = FHitOpts(); O.F0 = KeysDSP::MidiToHz((float)(Ch[K] + 12)); O.Dur = .5f; O.Gain = .03f; O.Wave = KeysDSP::EWave::Triangle; O.Lp = 2000; O.Att = .02f; Hit(T, O);
			}
	}

	// lead
	if (bLeadOn || (N == HBreak && S.Lead == ELead::Marimba))
	{
		if (S.Lead == ELead::Arp)
		{
			const int32 Seq[8] = {0, 1, 2, 3, 2, 1, 3, 2};
			Pluck(T, (float)(Ch[Seq[St % 8]] + 24 + (St >= 8 ? 12 : 0)), .22f, .03f);
		}
		if (S.Lead == ELead::Marimba)
			for (int32 K = 0; K < 8; ++K)
				if (H.Motif[K][0] == St) SteelPan(T, (float)(Ch[H.Motif[K][1]] + 24), .45f, .09f);
		if (S.Lead == ELead::Perc && (St == 3 || St == 11 || St == 14))
		{
			O = FHitOpts(); O.F0 = KeysDSP::MidiToHz((float)(Ch[0] + 36)); O.Dur = .07f; O.Gain = .05f; O.Wave = KeysDSP::EWave::Square; O.Lp = 3000; Hit(T, O);
		}
	}

	// build-up roll and riser
	if (N == HRoll || (N == HBuild && B.B >= B.Len - 2))
	{
		const int32 Dens = B.B >= B.Len - 1 ? 1 : 2;
		if (St % Dens == 0)
		{
			O = FHitOpts(); O.bNoise = true; O.Type = KeysDSP::EFilter::Bandpass; O.Freq = 1800.f + St * 90.f + B.B * 300.f; O.Q = 1; O.Dur = .07f;
			O.Gain = .1f + .012f * St + B.B * .03f; Hit(T, O);
		}
	}
	if (N == HRoll && B.B == 0 && St == 0)
	{
		O = FHitOpts(); O.bNoise = true; O.Type = KeysDSP::EFilter::Highpass; O.Freq = 900; O.Dur = Spb * 64; O.Gain = .08f; O.Att = Spb * 60; Hit(T, O);
	}
}
