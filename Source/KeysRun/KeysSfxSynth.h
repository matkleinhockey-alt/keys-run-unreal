#pragma once

#include "CoreMinimal.h"
#include "Components/SynthComponent.h"
#include "KeysSynth.h"
#include "KeysSfxSynth.generated.h"

// Targets for one makeEngineSynth() voice, written by AKeysAudio (the JS setEngine()) and glided on the audio thread
// with the same setTargetAtTime time constants.
struct FKeysEngineParams
{
	int32 NumOsc = 2;
	float OscFreq[4] = {11.7f, 11.7f, 11.7f, 11.7f};
	float OscGain[4] = {0, 0, 0, 0};
	float LfoFreq[4] = {5.8f, 5.8f, 5.8f, 5.8f};
	float LfoDepth[4] = {0, 0, 0, 0};
	float SubFreq = 23.f, SubGain = 0;
	float MuffleFreq = 375.f, ReliefGain = 0;
	float BubGain = 0, BubEnvDepth = 0;
	float InsGain = 0, InsFreq = 800.f;
	float GwFreq = 147.f, GwGain = 0;
	float WhineFreq = 315.f, WhineGain = 0;
	float OutGain = 0;
};

// Other boats' engines (web audio/engine.ts remote synths): the nearest few, each a buzzy pair of oscillators
// through a lowpass, placed in stereo, pitched by their speed and the Doppler shift of a pass.
struct FKeysRemoteVoice { float Freq = 40.f, Gain = 0.f, PanL = .7f, PanR = .7f, Cut = 700.f; };
struct FKeysRemoteParams { FKeysRemoteVoice V[3]; };

// The ambient loops (rush / hiss / wind / wash), the reel, and the master gain.
struct FKeysLoopParams
{
	float RushGain = 0, RushFreq = 650.f;
	float HissGain = 0;
	float WindGain = 0, WindFreq = 550.f;
	float WashGain = 0;
	float BreathGain = 0, BreathDepth = 0, BreathFreq = 480.f;   // the diver's own breathing (web audio/underwater.ts)
	float ReelRate = 1.f, ReelPitch = 2700.f, ReelDepth = 0;
	float Master = .8f;
};

// One audBurst() one-shot, or a gull call. Gains already include distance attenuation and stereo placement.
struct FKeysBurst
{
	bool bTone = false;
	bool bGull = false;
	KeysDSP::EWave Wave = KeysDSP::EWave::Sine;
	float Tone = 440.f, ToneTo = 0.f;               // ToneTo 0 = steady pitch
	KeysDSP::EFilter Filter = KeysDSP::EFilter::Lowpass;
	float Freq = 800.f, FreqTo = 0.f, Q = .8f;      // noise filter; FreqTo 0 = steady
	float Gain = .4f, Att = .005f, Dur = .3f;
	float GainL = 1.f, GainR = 1.f;
	float Delay = 0.f;                               // seconds before it starts (setTimeout in the JS)
};

// Engine, water, wind, reel and all one-shot effects, synthesized in real time.
UCLASS(ClassGroup = (Audio))
class KEYSRUN_API UKeysSfxSynth : public USynthComponent
{
	GENERATED_BODY()

public:
	UKeysSfxSynth(const FObjectInitializer& ObjectInitializer);

	// game thread
	void SetEngineParams(const FKeysEngineParams& P);
	void SetLoopParams(const FKeysLoopParams& P);
	void SetRemoteParams(const FKeysRemoteParams& P);
	void PushBurst(const FKeysBurst& B);

protected:
	virtual bool Init(int32& SampleRate) override;
	virtual int32 OnGenerateAudio(float* OutAudio, int32 NumSamples) override;

private:
	struct FBurstVoice
	{
		FKeysBurst B;
		int32 Age = 0;   // samples since queued (includes the delay)
		KeysDSP::FOsc Osc, Vib;
		KeysDSP::FNoise Noise;
		KeysDSP::FBiquad Flt;
	};

	struct FEngineOsc
	{
		KeysDSP::FOsc Osc, Lfo;
		KeysDSP::FSmooth Freq, Gain, LfoFreq, LfoDepth;
		float PanL = 1.f, PanR = 1.f;
	};

	void PullParams();
	void RenderBlock(float* Out, int32 Frames);
	void RenderBurst(FBurstVoice& V, float* Out, int32 Frames, bool& bDone);

	// shared with the game thread
	FCriticalSection ParamLock;
	FKeysEngineParams PendingEngine;
	FKeysLoopParams PendingLoops;
	FKeysRemoteParams PendingRemote;
	bool bRemoteDirty = false;
	TArray<FKeysBurst> PendingBursts;
	bool bEngineDirty = false, bLoopsDirty = false;

	// audio thread only
	float Rate = 48000.f;
	FRandomStream Rng;
	FKeysEngineParams Eng;
	FKeysLoopParams Loops;
	int32 CurNumOsc = 0;
	FEngineOsc Osc[4];
	KeysDSP::FBiquad MuffleL, MuffleR, ReliefL, ReliefR;
	KeysDSP::FSmooth MuffleF, Relief;
	KeysDSP::FOsc Sub; KeysDSP::FSmooth SubF, SubG;
	KeysDSP::FNoise BubN, EnvN; KeysDSP::FBiquad BubBP, EnvLP; KeysDSP::FSmooth BubG, BubEd;
	KeysDSP::FNoise InsN; KeysDSP::FBiquad InsBP; KeysDSP::FSmooth InsG, InsF;
	KeysDSP::FOsc Gw; KeysDSP::FSmooth GwF, GwG;
	KeysDSP::FOsc Wh1, Wh2; KeysDSP::FSmooth WhF, WhG;
	KeysDSP::FSmooth EngOut;

	KeysDSP::FNoise RushN, HissN, WindN, WashN, BreathN;
	KeysDSP::FBiquad BreathF;
	struct FRemoteVoice { KeysDSP::FOsc A, B; KeysDSP::FBiquad F; KeysDSP::FSmooth Fr, G, L, R, Cut; };
	FRemoteVoice Rv[3];
	FKeysRemoteParams Remote;
	KeysDSP::FSmooth BreathG, BreathD, BreathFr;
	double BreathPh = 0;
	KeysDSP::FBiquad RushF, HissF, WindF, WashF;
	KeysDSP::FSmooth RushG, RushFr, HissG, WindG, WindFr, WashG;
	KeysDSP::FOsc ReelTone, ReelLfo; KeysDSP::FBiquad ReelBP;
	KeysDSP::FSmooth ReelRate, ReelPitch, ReelDepth;
	KeysDSP::FSmooth Master;

	TArray<FBurstVoice> Voices;
	KeysDSP::FCompressor Comp;
	TArray<float> Scratch;
};
