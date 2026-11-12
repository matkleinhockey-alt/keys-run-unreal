#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "KeysSfxSynth.h"
#include "KeysAudio.generated.h"

class UKeysMusicSynth;

// Everything the sound engine needs from the player's boat each frame (the inputs to the JS AUD.update()).
// Positions are game meters (x east, y up, z south), like the rest of the port.
struct FKeysBoatAudioState
{
	bool bRunning = false;     // game.running (start screen = false)
	int32 BoatIndex = 0;       // a change rebuilds the engine voice and refires the motors (AUD.setPlayer)
	int32 Engines = 2;         // spec.engines
	double Speed = 0;          // boat.speed, m/s, signed
	double TopMs = 1;          // spec.top * .5144 * SPEED_SCALE
	double Thr = 0;            // boat.thr, -.35..1
	double TrimV = .2;         // TRIM.v
	double TrimVent = 0;       // TRIM.vent (props blowing out)
	bool bAir = false;         // boat.air
	int32 Gear = 0;            // 0 D, 1 N, 2 R
	bool bFwd = false;         // input.fwd (W): revs the motors in neutral
	FVector BoatPos = FVector::ZeroVector;   // game meters, for the boat stereo speakers
	// fishing (the reel)
	bool bLineOut = false;     // lineOut(): casting, waiting, nibble, bite or fight
	bool bFight = false;       // F.state == 'fight'
	bool bFishRunning = false; // F.running
	bool bReeling = false;     // F.reeling (S held, or Space during a fight)
	double FishStr = 0;        // F.str
	int32 Drag = 3;            // F.drag
};

// The game's sound: a real-time engine / water / reel / effects synth and the boat stereo with three procedural radio
// stations, ported from the WebAudio code in the three.js game (AUD, setEngine, audBurst, MUSIC, STATIONS).
// Spawn one per world; the pawn feeds it SetBoatState() every tick and calls the one-shots on game events.
UCLASS()
class KEYSRUN_API AKeysAudio : public AActor
{
	GENERATED_BODY()

public:
	AKeysAudio();
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

	// the first AKeysAudio in the world (or null)
	static AKeysAudio* Get(const UObject* WorldContext);

	// ---- per-frame state (AUD.update)
	void SetBoatState(const FKeysBoatAudioState& S) { Boat = S; }

	// ---- one-shots; positions in game meters
	void Splash(double X, double Z, int32 N, double Power);   // the sound half of the JS splash(x,z,n,power)
	void Slam(double K);                                     // AUD.slam(k), k = clamp(-vy/6, 0, 1)
	void Cast(double Power);                                 // AUD.cast(p)
	void Nibble(double BobX, double BobZ);                   // AUD.nibble()
	void Strike(double BobX, double BobZ);                   // AUD.strike()
	void Snap();                                             // AUD.snap(): the line breaks
	void Land();                                             // AUD.land(): fish landed chime
	void GearShift();                                        // the clunk in shiftGear()
	void Gull(double X, double Z);
	// sounds from AKeysExtras: AirHorn, CrowdRoar, MusicDuck, CaelenSquawk (others are ignored)
	void PlayExtra(FName Id, const FVector& GamePos, float Power);
	// the second port pass's sounds (KeysAudioMore.cpp): Bark, PartyBeat, Bell, Gunshot, DryClick, Reload, Flare, Ricochet,
	// Thud, Plink, SlotSpin, SlotStop, SlotWin, Cheer, Coins, Chest, Squawk; false if Id is none of them
	bool PlayMore(FName Id, const FVector& GamePos, float Power);                           // AUD.gull(x,z): a seabird over a school
	// a raw audBurst(); with GamePos it is placed in 3D with the given reference distance
	void Burst(const FKeysBurst& B, const FVector* GamePos = nullptr, float RefDist = 6.f);

	// ---- motors and master
	// under water (web audio/underwater.ts): everything muffles with depth, plus bubble pops, reef crackle and a
	// heartbeat that races as the air runs out. Call every frame; bUnder false fades it all back out.
	void SetUnderwater(bool bUnder, double Depth, double AirFrac, bool bHolding, double Dt);

	void StartEngines();
	void StopEngines();
	void ToggleEngines();                  // I key
	bool EnginesOn() const { return bEngineOn; }   // the JS ENGINE.on also gates the throttle (line 3899)
	void ToggleSound();                    // N key
	bool IsSoundOn() const { return bSoundOn; }

	// ---- boat stereo
	void MusicToggle();                    // P key: Beno Bonanza -> Margaritaville -> Hip-hop -> off
	void PlayStation(int32 K);             // 0 Margaritaville, 1 Hip-hop, 2 Beno Bonanza
	void MusicOff();
	bool IsMusicOn() const { return bMusicOn; }
	int32 GetStation() const { return Station; }
	FString GetRadioLabel() const;         // "Radio: Beno Bonanza - "Reef Rave"" / "Radio: Off" (the btnMusic text)
	FString GetDJLine() const;             // the DJ's words while he is talking, else empty
	float GetMusicLevel() const;           // 0..1 low-end level, for speaker LEDs

	// toasts the sound code wants shown (radio changes, motors, DJ); the pawn or HUD drains them
	bool ConsumeToast(FString& Out);

	// poll P / N / I here so nothing else has to bind them
	UPROPERTY(EditAnywhere, Category = "Keys Audio")
	bool bHandleKeys = true;

private:
	void Ready();
	void SetPlayer();
	void UpdateListener();
	void UpdateEngine(double Dt, double Now);
	void PushEngine(double Rpm, double Load, double Vol, double PitchMul, double Now);
	void UpdateLoops(double Dt, double Now);
	void UpdateMusic(double Dt, double Now);
	void HandleKeys();
	void Place(FKeysBurst& B, const FVector& GamePos, float RefDist, float Rolloff = 1.1f) const;
	void AddToast(const FString& S) { if (Toasts.Num() < 8) Toasts.Add(S); }
	double Rand(double A, double B) { return A + (B - A) * FMath::FRand(); }

	UPROPERTY() TObjectPtr<USceneComponent> Root;
	UPROPERTY() TObjectPtr<UKeysSfxSynth> Sfx;
	// the drag screaming as a fish takes line: a real recording (Content/KeysRun/Sounds, audiopapkin via Pixabay). The
	// strike plays its strongest surge at full level the instant a fish is on; the fight loop then follows the line
	// going out. Empty if the assets are missing, and the synth's chopped tone takes over again.
	UPROPERTY() TObjectPtr<class UAudioComponent> ReelStrikeAC;
	UPROPERTY() TObjectPtr<class UAudioComponent> ReelLoopAC;
	bool bWasFight = false;
	double StrikeT = 99, ReelLoopVol = 0, EngDuck = 0;
	void UpdateReelSample(double Dt, bool& bOutHandled);
	UPROPERTY() TObjectPtr<UKeysMusicSynth> Music;

	FKeysBoatAudioState Boat;
	bool bReady = false;       // AUD.ready: sound starts with the game (the JS starts it on the Go button)
	bool bSoundOn = true;      // AUD.on
	FVector ListenerPos = FVector::ZeroVector, ListenerRight = FVector(0, 1, 0);   // Unreal space

	// the player's engine voice (makeEngineSynth + the gate / starter state from AUD.update)
	struct FCyl { double Gate = 1, Mul = 1, StartAt = 0, Ph = 0, Off = 0, Base = .5; bool bStarting = false; };
	TArray<FCyl> Cyl;
	int32 PlayerBoat = -1;
	bool bEngineOn = true;
	double Rpm = 700, PrevThr = 0, GearRev = 0;
	double LastSplash = -1;
	FKeysLoopParams Loops;

	// radio
	bool bMusicOn = false, bUserOff = false;
	int32 Station = 0;
	double AutoStartAt = -1;
	double DJT = -1, DJUntil = -1, DuckUntil = -1;
	FString DJText;

	TArray<FString> Toasts;
	double UwLevel = 0, UwDepth = 0, BubbleT = .5, HeartT = 0;
	bool bLpfOn = false;
};
