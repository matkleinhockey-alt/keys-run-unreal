#pragma once

#include "CoreMinimal.h"
#include "Components/SynthComponent.h"
#include "KeysSynth.h"
#include <atomic>
#include "KeysMusicSynth.generated.h"

// The boat stereo: three procedural radio stations, written on the fly and sequenced sample-accurately on the audio thread.
//  0 Margaritaville  - original island songs (strummed acoustic, bass walk-ups, cross-stick and congas, harmonica, steel pan, pedal steel)
//  1 Hip-hop         - 90 BPM boom-bap with a minor-seventh soul progression
//  2 Beno Bonanza    - endless house tracks (deep, tech, piano, disco, tropical, prog) with intro / build / drop / breakdown / outro
UCLASS(ClassGroup = (Audio))
class KEYSRUN_API UKeysMusicSynth : public USynthComponent
{
	GENERATED_BODY()

public:
	UKeysMusicSynth(const FObjectInitializer& ObjectInitializer);

	// ---- game thread
	// starts station K from step 0 (musicPlayStation); K < 0 turns the radio off (musicOff)
	void PlayStation(int32 K);
	// MUSIC.out gain target (.85 playing, .3 under the DJ, 0 off), glided with time constant Tau
	void SetOutGain(float Target, float Tau);
	// boat-speaker placement relative to the camera (distance + stereo), and the master on/off gain
	void SetSpatial(float GainL, float GainR, float InMaster);
	// current song titles (empty until a song has been written)
	FString GetIslandTitle() const;
	FString GetHouseTitle() const;
	// smoothed loudness 0..1 of the low end, for the speaker LEDs (MUSIC.level)
	float GetLevel() const { return LevelOut.load(); }

protected:
	virtual bool Init(int32& SampleRate) override;
	virtual int32 OnGenerateAudio(float* OutAudio, int32 NumSamples) override;

private:
	// ---- one scheduled note (bHit, steelPan, pluck, harmonica or pedalSteel)
	struct FMVoice
	{
		int64 Start = 0;
		int32 NumOsc = 1;
		KeysDSP::FOsc Osc[4];
		float Amp[4] = {1, 0, 0, 0};
		float Mul[4] = {1, 1, 1, 1};      // partial ratio * detune
		bool bVib[4] = {false, false, false, false};
		float F0 = 440.f, F1 = 0.f, RampDur = 0.f;   // exponential pitch ramp from the start
		float VibRate = 0.f, VibDepth = 0.f; KeysDSP::FOsc Vib;
		bool bNoise = false; KeysDSP::FNoise Noise;
		bool bFilter = false; KeysDSP::EFilter FType = KeysDSP::EFilter::Lowpass; KeysDSP::FBiquad Flt;
		float FF0 = 1000.f, FF1 = 0.f, FRamp = 0.f, FQ = 1.f;
		// envelope: 0 -> Gain linearly over Att, hold until HoldT, step to Gain2, exponential to .0001 at EndT
		float Gain = .1f, Att = .003f, HoldT = .003f, Gain2 = .1f, EndT = .3f;
	};

	struct FHitOpts
	{
		bool bNoise = false;
		KeysDSP::EFilter Type = KeysDSP::EFilter::Highpass;
		float Freq = 1000.f, Q = .7f;
		float F0 = 100.f, F1 = 0.f, Det = 0.f, Lp = 0.f;
		KeysDSP::EWave Wave = KeysDSP::EWave::Sine;
		float Dur = .2f, Gain = .1f, Att = .003f;
	};

	// island song
	struct FIslandBar { int32 Sec; int32 Deg; int32 Lead; bool bLast; };   // Lead: index into LeadBars, -1 none
	struct FLeadNote { int32 Step; int32 Note; int32 Len; };
	struct FIslandSong
	{
		int32 Key = 62, Bpm = 104, Start = 0;
		TArray<FIslandBar> Bars;
		TArray<TArray<FLeadNote>> LeadBars;
	};
	// house track
	struct FHouseBar { int32 Sec; int32 B; int32 Len; };
	struct FHouseSong
	{
		int32 Style = 0, Root = 45, Bpm = 124, Start = 0;
		int32 Chords[4][4];
		TArray<int32> StabRx;
		int32 Motif[8][2];
		TArray<FHouseBar> Bars;
	};

	// voices
	FMVoice& NewVoice(double T);
	void Hit(double T, const FHitOpts& O);
	void SteelPan(double T, float M, float Dur, float Gain);
	void Pluck(double T, float M, float Dur, float Gain);
	void Strum(double T, const int32 Ch[3], bool bDown, float Dur, float Gain);
	void Harmonica(double T, float M, float Dur, float Gain);
	void PedalSteel(double T, float M, float Dur, float Gain);
	void RenderVoice(FMVoice& V, float* Out, int32 Frames, bool& bDone);

	// stations
	float StationBpm() const;
	float StationSwing() const;
	void StepStation(double T, int32 I);
	void BeatStep(double T, int32 I);
	void NewSong(int32 StartStep);
	void ChordOf(int32 Deg, int32 Out[3]) const;
	void IslandStep(double T, int32 I);
	void NewHouse(int32 StartStep);
	void HouseStep(double T, int32 I);

	float Rnd() { return Rng.FRand(); }
	float Rand(float A, float B) { return A + (B - A) * Rng.FRand(); }
	int32 Pick(int32 N) { return FMath::Min(N - 1, (int32)(Rng.FRand() * N)); }

	// ---- shared with the game thread
	std::atomic<int32> CmdStation{-1};
	std::atomic<int32> CmdSerial{0};
	std::atomic<float> CmdOutGain{0.f};
	std::atomic<float> CmdOutTau{.2f};
	std::atomic<float> CmdL{1.f}, CmdR{1.f}, CmdMaster{.8f};
	std::atomic<float> LevelOut{0.f};
	mutable FCriticalSection TitleLock;
	FString IslandTitle, HouseTitle;

	// ---- audio thread only
	float Rate = 48000.f;
	FRandomStream Rng;
	int64 Clock = 0;           // samples rendered
	int32 SeenSerial = 0;
	int32 Station = -1;
	bool bOn = false;
	double NextStep = 0;       // in samples
	int32 Step = 0;
	bool bHaveSong = false, bHaveHouse = false;
	FIslandSong Song;
	FHouseSong House;
	TArray<FMVoice> Voices;
	KeysDSP::FSmooth OutG, SpL, SpR, Mast;
	float LevelAcc = 0.f;
	KeysDSP::FCompressor Comp;
	TArray<float> Mono;
};
