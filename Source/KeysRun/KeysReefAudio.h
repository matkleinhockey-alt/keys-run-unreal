#pragma once

#include "CoreMinimal.h"
#include "Components/SynthComponent.h"
#include "Subsystems/WorldSubsystem.h"
#include "KeysSynth.h"
#include <atomic>
#include "KeysReefAudio.generated.h"

// Reef and mangrove sound (lane reefaudio). Everything is synthesised live, like the rest of the game's sound; no
// file is played. Four files:
//   KeysReefAudio.h          this header: the hook other files call, the synth component, the world subsystem
//   KeysReefAudio.cpp        the game thread: where the listener is, what is near, what to play and how loud, the log
//   KeysReefAudioSynth.cpp   the audio thread: the bed (snapping shrimp, surge, deep rumble) and the one-shot notes
//   KeysReefAudioVoices.cpp  what each animal's sound is made of (the recipes), one function a sound
//
// It runs by itself: UKeysReefAudioSub is a world subsystem the engine makes for every game world, so no hub file
// has to spawn or tick anything. It makes its own synth component on the AKeysAudio actor (its own, so the reef's
// high crackle is not shut behind the hub's 150-900 Hz under-water low-pass) and reads the diver, the bottom
// (KeysReefLife::HabitatAt), the feeding clock, the spear targets and the mangrove ecosystem's sound queue
// (KeysMangroveEco::PopSound) through their public functions. What it cannot see from outside reaches it through
// Heard(), which is the one call the hub patches (.marshal/requests/reefaudio) make. -KRNoReefAudio turns it off.
// Test hooks are a console command (script: con=kr.reefaudio ...): stats, test <sound> [metres], all, rec <seconds>
// [name], force 0|1|2 (as it is | under water | above water), bed <cover 0..1> <depth m> | bed off.
namespace KeysReefAudio
{
	// One call for every animal the audio cannot find by itself. GamePos in game metres. What is one of:
	//   "school"  Who is the KeysSchools species key ("grunt", "grouper", "redgrouper"), Amount the number of fish.
	//             Call it every time the school is stepped; the nearest one is kept for a second.
	//   "chase"   Who is the KeysReefLife species key of the chaser; once, as the chase starts.
	//   "pod"     Who is the KeysLife type ("dolphin"), Amount the number of animals; every frame it is alive.
	//   "rasp"    a spiny lobster rubbing its antennae at a diver or a fish (reefdens); once, as it does it.
	KEYSRUN_API void Heard(const TCHAR* What, const FString& Who, const FVector& GamePos, double Amount = 1);

	// the caps (criterion 4): notes sounding at once, and shrimp clicks sounding at once; plus three noise layers
	constexpr int32 MaxNotes = 24;
	constexpr int32 MaxClicks = 12;
}

// One one-shot note. A sound is a handful of these (KeysReefAudioVoices.cpp). Gains already hold distance and pan.
struct FKeysReefNote
{
	bool bTone = false;                               // an oscillator, else the shared noise table
	KeysDSP::EWave Wave = KeysDSP::EWave::Sine;
	float F0 = 440.f, F1 = 0.f, F2 = 0.f;             // pitch: start, middle (0: straight from F0 to F2), end (0: steady)
	float Vib = 0.f, VibHz = 0.f;                     // pitch wobble: fraction of the pitch, and its rate
	float Fc0 = 0.f, Fc1 = 0.f, Q = 1.f;              // filter (Fc0 0: none; always on for noise): from, to (0: steady)
	KeysDSP::EFilter Filter = KeysDSP::EFilter::Bandpass;
	float Am = 0.f, AmHz = 0.f;                       // amplitude chop 0..1 and its rate: a rough, pulsed voice
	float Gain = .1f, Att = .005f, Dur = .2f;         // peak, attack (s), length (s)
	float Rel = 0.f;                                  // 0: dies away exponentially from the attack (a tick, a splash);
	                                                  // > 0: held, then let go over this many seconds (a call)
	float Lp = 0.f;                                   // > 0: a one-pole low-pass at this many Hz (heard through the surface)
	float Delay = 0.f;                                // seconds before it starts
	float L = 1.f, R = 1.f;
};

// The bed, written by the game thread every frame and glided on the audio thread.
struct FKeysReefBed
{
	float SnapRate = 0.f;      // snapping-shrimp clicks a second
	float SnapGain = 0.f;      // the loudest click's peak
	float Sizzle = 0.f;        // the far crackle that has run together into a hiss
	float Surge = 0.f;         // the swell washing over the reef top
	float SurgeHz = 260.f;
	float Deep = 0.f;          // the low rumble of deep water
	float Lp = 9000.f;         // one-pole low-pass over the whole bed, Hz (closes a little with depth)
	float Master = 0.f;        // 0 out of the water, on the menus, in town, with the sound off
};

UCLASS(ClassGroup = (Audio))
class KEYSRUN_API UKeysReefSynth : public USynthComponent
{
	GENERATED_BODY()

public:
	UKeysReefSynth(const FObjectInitializer& ObjectInitializer);

	// game thread
	void SetBed(const FKeysReefBed& B);
	void PushNote(const FKeysReefNote& N);
	// record what the synth puts out (test hook): StartRec, then poll TakeRec until it gives the samples (stereo)
	void StartRec(double Seconds);
	bool TakeRec(TArray<float>& Out, int32& OutRate);
	// counters for the log
	int32 TakeClicks() { return Clicks.exchange(0); }
	int32 NotesNow() const { return NotesAlive.load(); }
	int32 PeakNotes() { return NotesPeak.exchange(0); }
	// audio-thread seconds spent, and seconds of sound made, since the last call
	void TakeCost(double& OutCpu, double& OutAudio);
	float TakePeak() { return OutPeak.exchange(0.f); }

protected:
	virtual bool Init(int32& SampleRate) override;
	virtual int32 OnGenerateAudio(float* OutAudio, int32 NumSamples) override;

private:
	struct FNoteVoice
	{
		FKeysReefNote N;
		int32 Age = 0;
		KeysDSP::FOsc Osc;
		KeysDSP::FNoise Noise;
		KeysDSP::FBiquad Flt;
		double VibPh = 0, AmPh = 0;
		float LpZ = 0.f;
	};
	struct FClick
	{
		KeysDSP::FBiquad Bp;
		KeysDSP::FNoise Noise;
		int32 Age = 0, Len = 0;
		float G = 0.f, L = 1.f, R = 1.f;
		bool bOn = false;
	};

	void RenderBlock(float* Out, int32 Frames);
	void RenderNote(FNoteVoice& V, float* Out, int32 Frames, bool& bDone);

	FCriticalSection Lock;
	FKeysReefBed PendingBed;
	TArray<FKeysReefNote> PendingNotes;

	// audio thread only
	float Rate = 48000.f;
	FRandomStream Rng;
	FKeysReefBed Bed;
	TArray<FNoteVoice> Notes;
	FClick Click[KeysReefAudio::MaxClicks];
	double NextClick = 0;
	KeysDSP::FSmooth SnapGain, Sizzle, Surge, SurgeHz, Deep, Lp, Master;
	KeysDSP::FNoise SizzleN, SurgeN, DeepN;
	KeysDSP::FBiquad SizzleF, SurgeF, DeepF;
	float BedLpL = 0.f, BedLpR = 0.f;

	// shared counters
	std::atomic<int32> Clicks{0}, NotesAlive{0}, NotesPeak{0};
	std::atomic<float> OutPeak{0.f};
	std::atomic<double> CpuSecs{0.0}, AudioSecs{0.0};
	// the recorder: the buffer is sized on the game thread before RecLen is set, and read after RecPos reaches it
	TArray<float> Rec;
	std::atomic<int32> RecLen{0}, RecPos{0};
};

// The game-thread half. One per game world; nothing in a hub file refers to it.
UCLASS()
class KEYSRUN_API UKeysReefAudioSub : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;
	virtual void Deinitialize() override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;

	void DebugCmd(const FString& Cmd);
	void OnHeard(const TCHAR* What, const FString& Who, const FVector& GamePos, double Amount);

private:
	UPROPERTY() TObjectPtr<UKeysReefSynth> Synth;
	struct FState;
	FState* St = nullptr;
};
