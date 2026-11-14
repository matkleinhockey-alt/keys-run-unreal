#include "KeysAudio.h"
#include "KeysUserMusic.h"
#include "KeysSpeech.h"
#include "KeysOnline.h"
#include "KeysLocals.h"
#include "KeysTraffic.h"
#include "Components/AudioComponent.h"
#include "Sound/SoundWave.h"
#include "KeysBoatPawn.h"
#include "KeysMusicSynth.h"
#include "KeysSynth.h"
#include "KeysGeo.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "Camera/PlayerCameraManager.h"
#include "InputCoreTypes.h"

namespace
{
	// ENG450: the supercharged 4.6 L racing V8 every powered boat in the game runs
	constexpr double EngIdle = 700, EngMax = 6400, EngTone = 1.25, EngWhine = 1, EngVol = 1.08;
	constexpr double EngineVol = .07;   // ENGINE_VOL
	const TCHAR* StationNames[3] = {TEXT("Margaritaville"), TEXT("Hip-hop"), TEXT("Beno Bonanza")};
}

AKeysAudio::AKeysAudio()
{
	PrimaryActorTick.bCanEverTick = true;
	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent = Root;
	Sfx = CreateDefaultSubobject<UKeysSfxSynth>(TEXT("Sfx"));
	Sfx->SetupAttachment(Root);
	Music = CreateDefaultSubobject<UKeysMusicSynth>(TEXT("Music"));
	Music->SetupAttachment(Root);
}

AKeysAudio* AKeysAudio::Get(const UObject* WorldContext)
{
	UWorld* W = GEngine ? GEngine->GetWorldFromContextObject(WorldContext, EGetWorldErrorMode::ReturnNull) : nullptr;
	if (!W) return nullptr;
	TActorIterator<AKeysAudio> It(W);
	return It ? *It : nullptr;
}

void AKeysAudio::BeginPlay()
{
	Super::BeginPlay();
	// build the shared tables on the game thread, then start both synths (silent until the game starts)
	KeysDSP::NoiseTable(); KeysDSP::SineTable(); KeysDSP::EngineTable();
	Loops.Master = 0.f;
	if (Sfx)
	{
		Sfx->SetLoopParams(Loops);
		Sfx->Start();
	}
	if (Music)
	{
		Music->SetOutGain(0.f, .2f);
		Music->SetSpatial(1.f, 1.f, 0.f);
		Music->Start();
	}
}

// ---------------------------------------------------------------- startup, motors, master

void AKeysAudio::Ready()
{
	if (bReady) return;
	bReady = true;
	SetPlayer();
}

void AKeysAudio::SetPlayer()
{
	// AUD.setPlayer(spec): a fresh engine voice for this boat's engine count, then fire them up
	PlayerBoat = Boat.BoatIndex;
	const int32 N = FMath::Clamp(Boat.Engines, 1, 4);
	Cyl.SetNum(N);
	for (int32 I = 0; I < N; ++I)
	{
		FCyl& C = Cyl[I];
		C = FCyl();
		C.Base = .6 / FMath::Sqrt((double)N);
		C.Off = (I - (N - 1) * .5) * .0009;
		C.Ph = FMath::FRand() * 6.0;
	}
	Rpm = EngIdle;
	StartEngines();
}

void AKeysAudio::StartEngines()
{
	if (!bReady || Cyl.Num() == 0 || !GetWorld()) return;
	const double Now = GetWorld()->GetRealTimeSeconds();
	bEngineOn = true;
	const double StartT = Now + .4;
	for (int32 I = 0; I < Cyl.Num(); ++I)
	{
		FCyl& C = Cyl[I];
		C.Gate = 0; C.Mul = 1;
		const double St = StartT + I * 1.1;
		C.StartAt = St;
		C.bStarting = true;
		if (bSoundOn)
		{
			// the starter cranking: a filtered grind and a rising whir, .7 s before each motor catches
			const float Delay = (float)FMath::Max(0.0, St - .7 - Now);
			FKeysBurst A; A.Filter = KeysDSP::EFilter::Bandpass; A.Freq = 420; A.Q = 2; A.Dur = .75f; A.Gain = .035f; A.Att = .05f; A.Delay = Delay;
			FKeysBurst B; B.bTone = true; B.Tone = 150; B.ToneTo = 210; B.Wave = KeysDSP::EWave::Square; B.Dur = .7f; B.Gain = .01f; B.Att = .05f; B.Delay = Delay;
			Burst(A);
			Burst(B);
		}
	}
}

void AKeysAudio::StopEngines()
{
	bEngineOn = false;
	for (FCyl& C : Cyl) { C.Gate = 0; C.bStarting = false; }
}

void AKeysAudio::ToggleEngines()
{
	if (!bReady) { Ready(); return; }
	if (bEngineOn) { StopEngines(); AddToast(TEXT("Motors off.")); }
	else { StartEngines(); AddToast(TEXT("Firing them up...")); }
}

void AKeysAudio::ToggleSound()
{
	bSoundOn = !bSoundOn;
	AddToast(bSoundOn ? TEXT("Sound on.") : TEXT("Sound off."));
}

// ---------------------------------------------------------------- one-shots

void AKeysAudio::Place(FKeysBurst& B, const FVector& GamePos, float RefDist, float Rolloff) const
{
	float L = 1.f, R = 1.f;
	KeysDSP::Spatialize(ListenerPos, ListenerRight, KeysGeo::ToUE(GamePos.X, GamePos.Y, GamePos.Z), RefDist, Rolloff, L, R);
	B.GainL *= L;
	B.GainR *= R;
}

void AKeysAudio::Burst(const FKeysBurst& In, const FVector* GamePos, float RefDist)
{
	if (!bReady || !bSoundOn || !Sfx) return;
	FKeysBurst B = In;
	if (GamePos) Place(B, *GamePos, RefDist);
	Sfx->PushBurst(B);
}

void AKeysAudio::Splash(double X, double Z, int32 N, double Power)
{
	if (!bReady) return;
	const double Size = Power * FMath::Min(1.6, N / 12.0);
	const double Now = FPlatformTime::Seconds();
	if (Now - LastSplash < .045) return;
	const double CamX = ListenerPos.X / 100.0, CamZ = ListenerPos.Y / 100.0;
	if (FMath::Sqrt((X - CamX) * (X - CamX) + (Z - CamZ) * (Z - CamZ)) > 140) return;
	LastSplash = Now;
	FKeysBurst B;
	B.Dur = (float)(.25 + Size * .35); B.Freq = (float)(700 + Size * 600); B.FreqTo = 260; B.Gain = (float)FMath::Min(.9, .25 + Size * .4);
	const FVector P(X, .2, Z);
	Burst(B, &P, (float)(5 + Size * 6));
}

void AKeysAudio::Slam(double K)
{
	const float G = (float)FMath::Clamp(K, 0.0, 1.0);
	FKeysBurst A; A.bTone = true; A.Tone = 75; A.ToneTo = 38; A.Dur = .4f; A.Gain = .9f * G;
	FKeysBurst B; B.Freq = 520; B.FreqTo = 140; B.Dur = .5f; B.Gain = .7f * G;
	FKeysBurst C; C.Filter = KeysDSP::EFilter::Highpass; C.Freq = 2500; C.Dur = .6f; C.Gain = .25f * G; C.Att = .03f;
	Burst(A); Burst(B); Burst(C);
}

void AKeysAudio::Cast(double Power)
{
	const float P = (float)Power;
	FKeysBurst A; A.Filter = KeysDSP::EFilter::Bandpass; A.Freq = 350; A.FreqTo = 2400; A.Q = 1.4f; A.Dur = .35f + P * .25f; A.Gain = .3f;
	FKeysBurst B; B.bTone = true; B.Tone = 1900 + P * 900; B.ToneTo = 900; B.Wave = KeysDSP::EWave::Triangle; B.Dur = .6f + P * .8f; B.Gain = .04f; B.Att = .05f;
	Burst(A); Burst(B);
}

void AKeysAudio::Nibble(double BobX, double BobZ)
{
	FKeysBurst A; A.Freq = 900; A.Dur = .08f; A.Gain = .12f;
	const FVector P(BobX, .1, BobZ);
	Burst(A, &P);
}

void AKeysAudio::Strike(double BobX, double BobZ)
{
	const FVector P(BobX, .1, BobZ);
	FKeysBurst A; A.bTone = true; A.Tone = 320; A.ToneTo = 110; A.Dur = .18f; A.Gain = .3f;
	FKeysBurst B; B.Freq = 1100; B.FreqTo = 300; B.Dur = .4f; B.Gain = .6f;
	Burst(A, &P); Burst(B, &P);
}

void AKeysAudio::Snap()
{
	FKeysBurst A; A.Filter = KeysDSP::EFilter::Highpass; A.Freq = 2600; A.Dur = .09f; A.Gain = .8f;
	FKeysBurst B; B.bTone = true; B.Tone = 1400; B.ToneTo = 500; B.Wave = KeysDSP::EWave::Triangle; B.Dur = .12f; B.Gain = .2f;
	Burst(A); Burst(B);
}

void AKeysAudio::Land()
{
	FKeysBurst A; A.bTone = true; A.Tone = 784; A.Dur = .35f; A.Gain = .12f;
	FKeysBurst B; B.bTone = true; B.Tone = 1175; B.Dur = .5f; B.Gain = .1f; B.Delay = .11f;
	Burst(A); Burst(B);
}

void AKeysAudio::GearShift()
{
	FKeysBurst A; A.Filter = KeysDSP::EFilter::Bandpass; A.Freq = 180; A.Q = 3; A.Dur = .12f; A.Gain = .18f;
	Burst(A);
}

void AKeysAudio::Gull(double X, double Z)
{
	FKeysBurst A; A.bGull = true; A.Dur = .5f;
	const FVector P(X, 14, Z);
	Burst(A, &P, 10.f);
}

// ---------------------------------------------------------------- per frame (AUD.update + updateMusic)

void AKeysAudio::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	UWorld* W = GetWorld();
	if (!W) return;
	const double Dt = FMath::Clamp((double)DeltaSeconds, 0.0, .1);
	const double Now = W->GetRealTimeSeconds();

	// the JS starts audio (and queues the house station 2.5 s later) when you press Go
	if (Boat.bRunning && !bReady)
	{
		Ready();
		AutoStartAt = Now + 2.5;
	}
	if (bReady && Boat.BoatIndex != PlayerBoat) SetPlayer();

	if (bHandleKeys) HandleKeys();
	UpdateListener();
	if (bReady)
	{
		UpdateEngine(Dt, Now);
		UpdateLoops(Dt, Now);
		UpdateRemoteEngines(Dt);
	}
	UpdateMusic(Dt, Now);
}

void AKeysAudio::HandleKeys()
{
	APlayerController* PC = GetWorld()->GetFirstPlayerController();
	if (!PC) return;
	// menus and the login box have the keyboard (typing a name with an N in it shouldn't mute the game)
	if (const AKeysBoatPawn* BP = Cast<AKeysBoatPawn>(PC->GetPawn()); BP && BP->IsCapturingKeys()) return;
	if (PC->WasInputKeyJustPressed(EKeys::P)) MusicToggle();
	if (PC->WasInputKeyJustPressed(EKeys::N)) ToggleSound();
	if (PC->WasInputKeyJustPressed(EKeys::I)) ToggleEngines();
}

void AKeysAudio::UpdateListener()
{
	APlayerController* PC = GetWorld()->GetFirstPlayerController();
	if (!PC || !PC->PlayerCameraManager) return;
	ListenerPos = PC->PlayerCameraManager->GetCameraLocation();
	ListenerRight = PC->PlayerCameraManager->GetCameraRotation().Quaternion().GetRightVector();
}

void AKeysAudio::UpdateEngine(double Dt, double Now)
{
	const FKeysBoatAudioState& S = Boat;
	const double TopMs = FMath::Max(S.TopMs, .1);
	const double F = FMath::Min(1.0, FMath::Abs(S.Speed) / TopMs), Thr = FMath::Abs(S.Thr);

	// your engines: rpm follows throttle, over-revs when the props come out of the water
	double Target = EngIdle + (EngMax - EngIdle) * FMath::Clamp(Thr * .82 + F * .25 + (S.TrimV - .4) * .08 * Thr + S.TrimVent * .22, 0.0, 1.1);
	if (S.Thr < 0) Target = EngIdle + (EngMax - EngIdle) * .25 * Thr / .35;
	GearRev = FMath::Lerp(GearRev, (S.Gear == 1 && bEngineOn && S.bFwd) ? 1.0 : 0.0, FMath::Min(1.0, Dt * (S.bFwd ? 2.2 : 1.4)));
	if (GearRev > .01) Target = FMath::Max(Target, EngIdle + (EngMax - EngIdle) * .72 * GearRev);   // revving in neutral
	if (S.bAir) Target = FMath::Min(EngMax * 1.12, Target * 1.18);

	for (FCyl& C : Cyl)
	{
		if (!bEngineOn) { C.Gate = 0; continue; }
		if (!C.bStarting) { C.Gate = 1; C.Mul = 1; continue; }
		const double DtS = Now - C.StartAt;
		if (DtS < 0) C.Gate = 0;
		else
		{
			// each motor catches and flares before settling to idle
			C.Gate = FMath::Min(1.0, DtS * 6);
			C.Mul = 1 + 1.15 * FMath::Exp(-DtS * 2.4) * FMath::Min(1.0, DtS * 8);
			if (DtS > 4) { C.bStarting = false; C.Mul = 1; }
		}
	}

	Rpm += (Target - Rpm) * FMath::Min(1.0, Dt * (Target > Rpm ? 2.6 : 1.6));
	// pops and crackle on the overrun when you chop the throttle at speed
	if (PrevThr > .6 && Thr < .15 && Rpm > EngMax * .55 && bSoundOn)
	{
		for (int32 K = 0; K < 7; ++K)
		{
			FKeysBurst B; B.Filter = KeysDSP::EFilter::Bandpass; B.Freq = (float)Rand(900, 2200); B.Q = 1.3f; B.Dur = .05f; B.Gain = (float)Rand(.15, .32);
			B.Delay = (float)Rand(.04, .9);
			Burst(B);
		}
	}
	PrevThr = Thr;
	const double Load = FMath::Clamp(Thr - F * .7 + .25, 0.0, 1.0) * (S.bAir ? .2 : 1);
	PushEngine(Rpm, Load, S.bRunning ? 1 : .6, 1, Now);
}

void AKeysAudio::PushEngine(double InRpm, double Load, double Vol, double PitchMul, double T)
{
	// setEngine(sy, rpm, load, vol, pitchMul) for the racing V8 profile
	if (!Sfx) return;
	FKeysEngineParams P;
	const int32 N = FMath::Clamp(Cyl.Num(), 1, 4);
	P.NumOsc = N;
	const double Crank = InRpm / 60 * PitchMul;
	const double R = FMath::Clamp((InRpm - EngIdle) / (EngMax - EngIdle), 0.0, 1.2), R1 = FMath::Min(R, 1.0);
	const double Lo = (1 - R1) * (1 - R1);
	double GateSum = 0, GateMulSum = 0;
	for (int32 I = 0; I < N; ++I)
	{
		const FCyl C = Cyl.IsValidIndex(I) ? Cyl[I] : FCyl();
		const double Det = 1 + C.Off + .0012 * FMath::Sin(T * (.17 + I * .05) + C.Ph) * (1 - .6 * R1);
		const double Hunt = 1 + .035 * Lo * FMath::Sin(T * (2.1 + I * .37) + C.Ph) + .02 * Lo * FMath::Sin(T * 5.3 + C.Ph * 2);   // a racing V8 never idles perfectly steady
		P.OscFreq[I] = (float)(Crank * Det * Hunt * C.Mul);
		P.OscGain[I] = (float)(C.Base * C.Gate * (1 - .62 * R1 * R1));   // at speed the crank note gives way to the firing harmonics
		P.LfoFreq[I] = (float)(Crank * .5 * Det * C.Mul * (1 + .04 * FMath::Sin(T * 1.3 + C.Ph)));   // cam-speed lope
		P.LfoDepth[I] = (float)(.62 * Lo + .06);
		GateSum += C.Gate;
		GateMulSum += C.Gate * C.Mul;
	}
	const double AvgGate = GateSum / N, AvgGateMul = GateMulSum / N;
	P.SubFreq = (float)(Crank * 2);
	// tuned against a recording of quad Mercury Racing 500Rs at speed: the weight is in the 2nd and 4th harmonics of
	// the crank (the V8's firing pulse), the 250-500 Hz body is full, 500 Hz-1 kHz is dark, and the supercharger
	// breathes in at 2-4 kHz
	P.SubGain = (float)(.24 * (1 + .6 * Load) * (1 + 1.1 * R1) * AvgGate);
	P.MuffleFreq = (float)((260 + R * 1250) * EngTone * (.8 + .35 * Load));
	P.FireFreq = (float)(Crank * 4);
	P.FireGain = (float)((.04 + .48 * R1 * R1) * (.7 + .3 * Load) * AvgGate);
	P.ReliefGain = (float)(.17 * Lo + .09 + .05 * R + .06 * Load);
	P.BubGain = (float)(.035 * Lo * FMath::Sqrt((double)N));
	P.BubEnvDepth = (float)(.5 * Lo);
	P.InsGain = (float)(.015 + .55 * Load * R);
	P.InsFreq = (float)(1400 + R * 1600);
	P.GwFreq = (float)(InRpm * .21 * PitchMul);
	P.GwGain = (float)(.003 + .01 * R * R);
	P.WhineFreq = (float)(InRpm / 60 * 27 * PitchMul);
	P.WhineGain = (float)(EngWhine * (.0075 + .14 * R * R) * AvgGateMul);
	P.OutGain = (float)(EngineVol * Vol * EngVol * (.36 + .45 * R + .3 * Load) * (1 + .12 * (N - 1)) * (1 - .8 * EngDuck));
	Sfx->SetEngineParams(P);
}

void AKeysAudio::UpdateLoops(double Dt, double T)
{
	const FKeysBoatAudioState& S = Boat;
	const double F = FMath::Min(1.0, FMath::Abs(S.Speed) / FMath::Max(S.TopMs, .1));
	const double Sea = FMath::Min(FMath::Max(KeysGeo::SeaSW() * 1.1, KeysGeo::SeaCH()), 2.5);

	// water and wind
	Loops.RushGain = (float)(S.bAir ? .02 : .04 + F * .32);
	Loops.RushFreq = (float)(400 + F * 900);
	// rain hisses on the water and drums on the boat; a blow howls
	Loops.HissGain = (float)((S.bAir ? 0 : F * F * .12) + WxRain * .2);
	Loops.WindGain = (float)(.015 + F * .09 + Sea * .015 + FMath::Clamp((WxWind - 8) / 14, 0.0, 1.0) * .12);
	Loops.WindFreq = (float)(400 + F * 700);
	Loops.WashGain = (float)((.05 + .05 * Sea) * (.6 + .4 * FMath::Sin(T * .7) * FMath::Sin(T * .31)));
	// waves slapping the hull when you sit still
	if (F < .15 && FMath::FRand() < Dt * (.6 + Sea * .8))
	{
		FKeysBurst B; B.Freq = 300; B.FreqTo = 120; B.Dur = .25f; B.Gain = (float)(.08 + Sea * .05); B.Att = .02f;
		Burst(B);
	}

	// the reel: drag scream while a fish runs, the crank ratchet while you reel
	bool bSampled = false;
	UpdateReelSample(Dt, bSampled);
	double Rate = 0, Rg = 0, Pitch = 2700;
	if (S.bFight && bSampled && !(S.bReeling && !S.bFishRunning)) { }   // the recording has it
	else if (S.bFight)
	{
		if (S.bFishRunning && !S.bReeling) { Rate = 40 + S.FishStr * 30; Rg = .025 + .012 * S.Drag; }
		else if (S.bFishRunning) { Rate = 55; Rg = .02 + .01 * S.Drag; Pitch = 3100; }
		else if (S.bReeling) { Rate = 14; Rg = .012; Pitch = 1900; }
	}
	else if (S.bReeling && S.bLineOut) { Rate = 12; Rg = .01; Pitch = 1900; }
	Loops.ReelRate = (float)FMath::Max(1.0, Rate);
	Loops.ReelPitch = (float)Pitch;
	Loops.ReelDepth = (float)Rg;
	Loops.Master = (bReady && bSoundOn) ? .8f : 0.f;
	if (Sfx) Sfx->SetLoopParams(Loops);
}

// ---------------------------------------------------------------- the reel drag, from the recording
void AKeysAudio::UpdateReelSample(double Dt, bool& bOutHandled)
{
	static TWeakObjectPtr<USoundWave> StrikeW, LoopW;
	static bool bTried = false;
	if (!bTried)
	{
		bTried = true;
		StrikeW = LoadObject<USoundWave>(nullptr, TEXT("/Game/KeysRun/Sounds/KR_ReelStrike.KR_ReelStrike"));
		LoopW = LoadObject<USoundWave>(nullptr, TEXT("/Game/KeysRun/Sounds/KR_ReelLoop.KR_ReelLoop"));
	}
	bOutHandled = false;
	EngDuck = FMath::Max(0.0, EngDuck - Dt / 1.6);
	if (!StrikeW.IsValid() || !LoopW.IsValid()) return;
	auto Make = [this](USoundWave* W) {
		UAudioComponent* C = NewObject<UAudioComponent>(this);
		C->SetSound(W);
		C->bAutoActivate = false;
		C->bIsUISound = false;
		C->bAllowSpatialization = false;
		C->RegisterComponent();
		return C;
	};
	if (!ReelStrikeAC) ReelStrikeAC = Make(StrikeW.Get());
	if (!ReelLoopAC) ReelLoopAC = Make(LoopW.Get());
	const FKeysBoatAudioState& S = Boat;
	const bool bOn = bReady && bSoundOn;
	// the hit: the clicker goes from nothing to a full scream at once, the engine and the radio duck under it
	if (S.bFight && !bWasFight && bOn)
	{
		StrikeT = 0;
		ReelStrikeAC->SetVolumeMultiplier(2.8f);
		ReelStrikeAC->SetPitchMultiplier(.97f + .015f * S.Drag);
		ReelStrikeAC->Play(0.f);
		EngDuck = 1;
		if (bMusicOn && Music) { Music->SetOutGain(.3f, .08f); DuckUntil = GetWorld()->GetTimeSeconds() + 3.2; }
	}
	bWasFight = S.bFight;
	StrikeT += Dt;
	if (!S.bFight || !bOn)
	{
		if (ReelStrikeAC->IsPlaying() && (!bOn || StrikeT > .2)) ReelStrikeAC->FadeOut(.25f, 0.f);
		if (ReelLoopAC->IsPlaying()) ReelLoopAC->FadeOut(.3f, 0.f);
		ReelLoopVol = 0;
		return;
	}
	bOutHandled = true;
	// the fight: the loop's level and pitch follow the line going out (a running fish, how hard it pulls, the drag);
	// it takes over as the strike's first run dies away
	const double Str = FMath::Clamp(S.FishStr, 0.0, 1.5);
	double Want = 0;
	if (S.bFishRunning) Want = (S.bReeling ? .45 : .7) + .3 * FMath::Min(1.0, Str);
	const double Hand = FMath::Clamp((StrikeT - 2.6) / 1.4, 0.0, 1.0);   // the strike's run, then the loop
	ReelLoopVol += (Want * Hand - ReelLoopVol) * FMath::Min(1.0, Dt * (Want > ReelLoopVol ? 9.0 : 3.0));
	if (ReelLoopVol > .02 && !ReelLoopAC->IsPlaying())
		ReelLoopAC->Play(FMath::FRandRange(0.f, 11.f));   // a random way into the loop, so it never starts the same
	if (ReelLoopAC->IsPlaying())
	{
		ReelLoopAC->SetVolumeMultiplier(float(ReelLoopVol * 2.2));
		// heavier drag: tighter and higher; a hard run: a touch faster
		ReelLoopAC->SetPitchMultiplier(float(.9 + .03 * S.Drag + .08 * FMath::Min(1.0, Str)));
		if (ReelLoopVol < .01) ReelLoopAC->Stop();
	}
}

// ---------------------------------------------------------------- the boat stereo

void AKeysAudio::PlayStation(int32 K)
{
	Ready();
	if (!Music) return;
	Station = FMath::Clamp(K, 0, 2);
	bMusicOn = true;
	KeysUserMusic::Stop();
	Music->PlayStation(Station);
	Music->SetOutGain(.85f, .2f);
	DuckUntil = -1;
}

void AKeysAudio::PlayMyMusic()
{
	Ready();
	if (!Music || !KeysUserMusic::Play(FMath::Max(0, KeysUserMusic::Current()))) return;
	Station = 3;
	bMusicOn = true;
	Music->PlayStation(-1);
	Music->SetOutGain(0.f, .15f);
	DuckUntil = -1;
	AddToast(FString::Printf(TEXT("My Music - \"%s\" on the boat speakers. P changes the station."), *KeysUserMusic::Title()));
}

void AKeysAudio::MusicOff()
{
	KeysUserMusic::Stop();
	bMusicOn = false;
	if (Music)
	{
		Music->PlayStation(-1);
		Music->SetOutGain(0.f, .15f);
	}
}

void AKeysAudio::MusicToggle()
{
	if (!bMusicOn) { bUserOff = false; PlayStation(2); AddToast(TEXT("Beno Bonanza - house music, four on the floor. P changes the station.")); }
	else if (Station == 2) { PlayStation(0); AddToast(TEXT("Margaritaville - easygoing island songs.")); }
	else if (Station == 0) { PlayStation(1); AddToast(TEXT("Hip-hop station - boom-bap beats on the boat speakers.")); }
	else if (Station == 1 && KeysUserMusic::Scan() > 0) PlayMyMusic();
	else
	{
		const bool bNone = Station == 1 && KeysUserMusic::Count() == 0;
		MusicOff(); bUserOff = true;
		AddToast(bNone ? TEXT("Radio off. (Put your own songs in Music > Keys Run for a station of your own.)") : TEXT("Radio off."));
	}
}

FString AKeysAudio::GetRadioLabel() const
{
	if (!bMusicOn) return TEXT("Radio: Off");
	if (Station == 3) return FString(TEXT("Radio: My Music - \"")) + KeysUserMusic::Title() + TEXT("\"");
	FString L = FString(TEXT("Radio: ")) + StationNames[FMath::Clamp(Station, 0, 2)];
	if (Music)
	{
		const FString Title = Station == 0 ? Music->GetIslandTitle() : Station == 2 ? Music->GetHouseTitle() : FString();
		if (!Title.IsEmpty()) L += FString(TEXT(" - \"")) + Title + TEXT("\"");
	}
	return L;
}

FString AKeysAudio::GetDJLine() const
{
	const UWorld* W = GetWorld();
	if (!W || DJUntil < 0 || W->GetRealTimeSeconds() > DJUntil) return FString();
	return DJText;
}

float AKeysAudio::GetMusicLevel() const
{
	return Music ? Music->GetLevel() : 0.f;
}

bool AKeysAudio::ConsumeToast(FString& Out)
{
	if (Toasts.Num() == 0) return false;
	Out = Toasts[0];
	Toasts.RemoveAt(0);
	return true;
}

void AKeysAudio::UpdateMusic(double Dt, double Now)
{
	if (!Music) return;
	if (AutoStartAt > 0 && Now >= AutoStartAt)
	{
		AutoStartAt = -1;
		if (!bMusicOn && !bUserOff) PlayStation(2);
	}

	// the station DJ cuts in now and then on Margaritaville
	if (bMusicOn && Station == 0)
	{
		if (DJT < 0) DJT = Rand(40, 70);
		DJT -= Dt;
		if (DJT <= 0)
		{
			DJT = Rand(150, 240);
			DJText = TEXT("Margaritaville - easy music to get hammered to.");
			DJUntil = Now + 6;
			AddToast(TEXT("DJ: ") + DJText);
			if (bSoundOn)
			{
				Music->SetOutGain(.3f, .2f); DuckUntil = Now + 4;   // talk-over duck
				KeysSpeech::Speak(TEXT("Margaritaville. Easy music to get hammered to."), .92f, .85f, .9f);
			}
		}
	}
	if (DuckUntil > 0 && Now >= DuckUntil)
	{
		DuckUntil = -1;
		if (bMusicOn) Music->SetOutGain(.85f, .4f);
	}

	// the boat stereo plays through the speakers on the console
	float L = 1.f, R = 1.f;
	const FVector Spk = KeysGeo::ToUE(Boat.BoatPos.X, Boat.BoatPos.Y + 1.5, Boat.BoatPos.Z);
	KeysDSP::Spatialize(ListenerPos, ListenerRight, Spk, 10.f, 1.f, L, R);
	Music->SetSpatial(L, R, (bReady && bSoundOn) ? .8f : 0.f);
	// your own songs play through macOS: the same speaker placement as volume and pan, ducked under a callout and
	// dulled underwater (it can't be filtered, so it's turned down instead)
	if (bMusicOn && Station == 3)
	{
		const float Duck = DuckUntil > 0 ? .35f : 1.f;
		const float Vol = (bReady && bSoundOn) ? .8f * .5f * (L + R) * .6f * Duck * float(1 - .75 * UwLevel) : 0.f;
		KeysUserMusic::Update(Vol, (R - L) / FMath::Max(.01f, L + R));
	}
}

void AKeysAudio::PlayExtra(FName Id, const FVector& GamePos, float Power)
{
	if (Id == TEXT("AirHorn"))
	{
		// three blasts of a four-note sawtooth chord (JS airHorn)
		const float Starts[3] = { 0.f, .5f, .72f }, Durs[3] = { .42f, .16f, .62f }, Notes[4] = { 466.f, 587.f, 698.f, 932.f };
		for (int32 b = 0; b < 3; b++)
			for (int32 k = 0; k < 4; k++)
			{
				FKeysBurst B; B.bTone = true; B.Wave = KeysDSP::EWave::Saw;
				B.Tone = Notes[k] * .97f * FMath::Pow(2.f, (k - 1.5f) * 7.f / 1200.f); B.ToneTo = Notes[k];
				B.Gain = .05f; B.Att = .02f; B.Dur = Durs[b]; B.Delay = Starts[b];
				Burst(B);
			}
	}
	else if (Id == TEXT("CrowdRoar"))
	{
		// band-passed noise swelling for about 3 s (JS crowdRoar)
		const float Steps[4] = { 0.f, .8f, 1.6f, 2.4f }, Gains[4] = { .3f, .25f, .2f, .12f };
		for (int32 i = 0; i < 4; i++)
		{
			FKeysBurst B; B.Filter = KeysDSP::EFilter::Bandpass; B.Freq = 1100; B.Q = .6f;
			B.Gain = Gains[i]; B.Att = i == 0 ? .4f : .1f; B.Dur = 1.f; B.Delay = Steps[i];
			Burst(B);
		}
	}
	else if (Id == TEXT("HypeVoice"))
	{
		// the Luigi-mode hype man, shouting over the horn and the crowd (web dj.ts hypeCallout)
		if (bSoundOn)
		{
			KeysSpeech::Speak(TEXT("Luigi mode!"), 1.05f, 1.05f, 1.f, .7f);
			KeysSpeech::Speak(TEXT("Neeeeeck!"), .7f, .85f, 1.f, .71f);
			KeysSpeech::Speak(TEXT("Fuck!"), 1.15f, .7f, 1.f, .72f);
		}
	}
	else if (Id == TEXT("MusicDuck"))
	{
		if (bSoundOn && bMusicOn && Music) { Music->SetOutGain(.35f, .2f); DuckUntil = GetWorld()->GetTimeSeconds() + 3.6; }
	}
	else if (PlayMore(Id, GamePos, Power)) {}
	else if (Id == TEXT("CaelenSquawk"))
	{
		const float Starts[3] = { 0.f, .28f, .5f };
		for (int32 k = 0; k < 3; k++)
		{
			const float F0 = k == 2 ? 1500.f : 1750.f;
			FKeysBurst B; B.bTone = true; B.Wave = KeysDSP::EWave::Saw; B.Tone = F0; B.ToneTo = F0 * .55f;
			B.Gain = .08f; B.Att = .02f; B.Dur = .24f; B.Delay = Starts[k];
			Burst(B, &GamePos, 4.f);
		}
	}
}

void AKeysAudio::SetUnderwater(bool bUnder, double Depth, double AirFrac, bool bHolding, double Dt)
{
	UwDepth = FMath::Lerp(UwDepth, bUnder ? FMath::Max(0.0, Depth) : 0.0, FMath::Min(1.0, Dt * 3));
	UwLevel = FMath::Lerp(UwLevel, bUnder ? 1.0 : 0.0, FMath::Min(1.0, Dt * 4));
	// the muffle: 900 Hz just under the surface, closing toward 150 Hz by ~14 m and below
	const bool bWant = UwLevel > .01;
	const float Hz = bUnder ? float(FMath::Max(150.0, 900 * FMath::Exp(-UwDepth / 14))) : 20000.f;
	for (USynthComponent* C : {(USynthComponent*)Sfx.Get(), (USynthComponent*)Music.Get()})
	{
		if (!C) continue;
		if (bWant != bLpfOn) C->SetLowPassFilterEnabled(bWant);
		if (bWant) C->SetLowPassFilterFrequency(FMath::Lerp(20000.f, Hz, float(UwLevel)));
	}
	bLpfOn = bWant;
	// your own breathing: louder and a touch higher while holding a breath down there than idling in the shallows
	Loops.BreathGain = UwLevel < .01 ? 0.f : float((bHolding ? .1 : .04) * UwLevel);
	Loops.BreathDepth = UwLevel < .01 ? 0.f : float((bHolding ? .07 : .02) * UwLevel);
	Loops.BreathFreq = bHolding ? 520.f : 440.f;
	if (UwLevel < .01 || !bReady) return;
	// bubbles off the mask
	BubbleT -= Dt;
	if (BubbleT <= 0)
	{
		BubbleT = bHolding ? Rand(.4, 1.4) : Rand(1.5, 4);
		FKeysBurst B;
		B.Filter = KeysDSP::EFilter::Bandpass; B.Freq = float(Rand(900, 2600)); B.Q = 3;
		B.Gain = float(Rand(.04, .09) * UwLevel); B.Att = .004f; B.Dur = .09f;
		Burst(B);
	}
	// the reef crackling (snapping shrimp)
	if (FMath::FRand() < Dt * 2.2)
	{
		FKeysBurst B;
		B.Filter = KeysDSP::EFilter::Bandpass; B.Freq = float(Rand(3200, 6500)); B.Q = float(Rand(4, 9));
		B.Gain = float(Rand(.01, .03) * UwLevel); B.Att = .002f; B.Dur = .05f;
		Burst(B);
	}
	// heartbeat: 62 bpm at full air racing to 150 near blackout
	const double Air = FMath::Clamp(AirFrac, 0.0, 1.0);
	if (bHolding && Air < .97)
	{
		HeartT -= Dt * (FMath::Lerp(150.0, 62.0, Air) / 60);
		if (HeartT <= 0)
		{
			HeartT = 1;
			const double Str = UwLevel * FMath::Lerp(1.6, .7, Air);
			const float Beats[2][4] = {{0, 56, .16f, .22f}, {.14f, 46, .2f, .16f}};
			for (const auto& Bt : Beats)
			{
				FKeysBurst B;
				B.bTone = true; B.Wave = KeysDSP::EWave::Sine;
				B.Tone = Bt[1]; B.ToneTo = Bt[1] * .7f;
				B.Gain = float(Bt[3] * Str); B.Att = .015f; B.Dur = Bt[2]; B.Delay = Bt[0];
				Burst(B);
			}
		}
	}
	else HeartT = FMath::Min(HeartT, 1.0);
}

// ---------------------------------------------------------------- other boats' engines (web audio/engine.ts)
// Traffic, the go-fast racers, the working fleet and friends online: the three nearest within earshot get a voice,
// pitched by how fast they're going (from their motion) and shifted by the Doppler of a pass, panned where they are.
void AKeysAudio::UpdateRemoteEngines(double Dt)
{
	if (!Sfx || Dt <= 0) return;
	struct FSrc { FString Id; double X, Z, Range; bool bRacer; };
	TArray<FSrc> Src;
	const double LX = ListenerPos.X / 100.0, LZ = ListenerPos.Y / 100.0;
	if (TActorIterator<AKeysTraffic> It(GetWorld()); It)
	{
		const TArray<FKeysAIBoat>& Tr = It->Traffic();
		for (int32 I = 0; I < Tr.Num(); ++I) if (Tr[I].Kind != FKeysAIBoat::Sail) Src.Add({FString::Printf(TEXT("t%d"), I), Tr[I].X, Tr[I].Z, 650, false});
		const TArray<FKeysAIBoat>& Rc = It->Racers();
		for (int32 I = 0; I < Rc.Num(); ++I) Src.Add({FString::Printf(TEXT("r%d"), I), Rc[I].X, Rc[I].Z, 1400, true});
	}
	if (AKeysLocals* Lc = AKeysLocals::Get(this))
	{
		TArray<FVector2D> Fl; Lc->FleetDots(Fl);
		for (int32 I = 0; I < Fl.Num(); ++I) Src.Add({FString::Printf(TEXT("f%d"), I), Fl[I].X, Fl[I].Y, 650, false});
	}
	if (AKeysOnline* On = AKeysOnline::Get(this))
	{
		TArray<TPair<FString, FVector2D>> Fr; On->FriendBoats(Fr);
		for (const auto& P : Fr) Src.Add({TEXT("o") + P.Key, P.Value.X, P.Value.Y, 900, false});
	}
	// speeds and closing rates from the last frame's positions
	struct FHeard { int32 I; double D, Sp, Vr; };
	TArray<FHeard> Near;
	TMap<FString, FVector2D> Now;
	for (int32 I = 0; I < Src.Num(); ++I)
	{
		const FSrc& S = Src[I];
		Now.Add(S.Id, FVector2D(S.X, S.Z));
		const double D = FMath::Sqrt(FMath::Square(S.X - LX) + FMath::Square(S.Z - LZ));
		if (D > S.Range) continue;
		double Sp = 0, Vr = 0;
		if (const FVector2D* Pv = RemotePrev.Find(S.Id))
		{
			const double VX = (S.X - Pv->X) / Dt, VZ = (S.Z - Pv->Y) / Dt;
			if (FMath::Abs(VX) < 120 && FMath::Abs(VZ) < 120)   // (a respawn jump isn't a speed)
			{
				Sp = FMath::Sqrt(VX * VX + VZ * VZ);
				Vr = FMath::Clamp((VX * (LX - S.X) + VZ * (LZ - S.Z)) / FMath::Max(D, 1.0), -90.0, 90.0);
			}
		}
		double& Sm = RemoteVr.FindOrAdd(S.Id);
		Sm = FMath::Lerp(Sm, Vr, FMath::Min(1.0, Dt * 4));
		Near.Add({I, D, Sp, Sm});
	}
	RemotePrev = MoveTemp(Now);
	Near.Sort([](const FHeard& A, const FHeard& B) { return A.D < B.D; });
	FKeysRemoteParams P;
	const bool bOn = bSoundOn;
	for (int32 K = 0; K < 3; ++K)
	{
		FKeysRemoteVoice& V = P.V[K];
		if (!bOn || K >= Near.Num()) { V.Gain = 0; continue; }
		const FHeard& H = Near[K]; const FSrc& S = Src[H.I];
		const double Fr = FMath::Clamp(H.Sp / (S.bRacer ? 40.0 : 22.0), 0.0, 1.0);
		const double Dop = FMath::Clamp(343 / (343 - H.Vr), .82, 1.22);
		V.Freq = float((S.bRacer ? FMath::Lerp(55.0, 120.0, Fr) : FMath::Lerp(34.0, 85.0, Fr)) * Dop);
		V.Cut = float(FMath::Lerp(380.0, S.bRacer ? 2200.0 : 1300.0, Fr) * FMath::Clamp(120 / FMath::Max(H.D, 30.0), .35, 1.0));
		const double Loud = (S.bRacer ? .11 : .07) * (.35 + .65 * Fr);
		const double Att = 18 / (18 + FMath::Max(0.0, H.D - 10) * 1.0);   // inverse-distance, ref 18 m
		float L = 1.f, R = 1.f;
		KeysDSP::Spatialize(ListenerPos, ListenerRight, KeysGeo::ToUE(S.X, 1.2, S.Z), 1e6f, 0.f, L, R);   // pan only
		V.Gain = float(Loud * Att); V.PanL = L; V.PanR = R;
	}
	static double LogT = 0;
	LogT += Dt;
	if (LogT > 5 && FString(FCommandLine::Get()).Contains(TEXT("KRScript=")))
	{
		LogT = 0;
		FString Msg;
		for (int32 K = 0; K < FMath::Min(3, Near.Num()); ++K) Msg += FString::Printf(TEXT(" [%s %.0f m %.1f m/s %.0f Hz gain %.3f]"), *Src[Near[K].I].Id, Near[K].D, Near[K].Sp, P.V[K].Freq, P.V[K].Gain);
		UE_LOG(LogTemp, Log, TEXT("KRAudio remote engines: %d in earshot of %d%s"), Near.Num(), Src.Num(), *Msg);
	}
	Sfx->SetRemoteParams(P);
}
