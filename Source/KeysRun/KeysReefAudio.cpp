#include "KeysReefAudio.h"
#include "KeysReefAudioInt.h"
#include "KeysAudio.h"
#include "KeysBoatPawn.h"
#include "KeysDiver.h"
#include "KeysDive.h"
#include "KeysLife.h"
#include "KeysReefLife.h"
#include "KeysMangroveEco.h"
#include "KeysFeed.h"
#include "KeysGeo.h"
#include "KeysOcean.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "Camera/PlayerCameraManager.h"
#include "HAL/IConsoleManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/CommandLine.h"

// The game-thread half of the reef and mangrove sound: see KeysReefAudio.h. Log lines start "KRReefAudio".
namespace
{
	TWeakObjectPtr<UKeysReefAudioSub> GReefAudio;

	// hearing rules that are not a property of one sound (the per-sound distances are in Sounds())
	constexpr double GruntHearM = 12, GrouperHearM = 25, GoliathHearM = 35, PodHearM = 150, ChaseHearM = 10, RaspHearM = 10;
	constexpr double MuffledSplashM = 40;     // a splash at the surface, heard from under water, no further than this
	constexpr float MuffleHz = 420.f, MuffleGain = .5f;
	constexpr int32 CoverPts = 17;            // the listener's own spot, eight points 15 m round it, eight 30 m round it

	double Hyp3(const FVector& A, const FVector& B) { return (A - B).Size(); }
	float ToDb(double V) { return V > 1e-7 ? float(20 * FMath::LogX(10.0, V)) : -140.f; }
}

struct UKeysReefAudioSub::FState
{
	FRandomStream Rng{0x4b525241};
	bool bOff = false, bScript = false;
	TWeakObjectPtr<AKeysBoatPawn> Pawn;
	TWeakObjectPtr<AKeysAudio> Audio;

	// the listener, this frame
	FVector Cam = FVector::ZeroVector;        // game metres (x east, y up, z south)
	FVector CamUE = FVector::ZeroVector, RightUE = FVector(0, 1, 0);
	double Under = 0;                         // 0 above the water .. 1 the camera is under it
	double DiverDepth = 0;
	bool bLive = false;                       // in the game, no menu, not ashore, sound on
	FString WhyNot = TEXT("not started");

	// the bottom round the listener
	double CoverAt[CoverPts] = {};
	int32 CoverNext = 0;
	double Cover = 0, WaterDepth = 0;
	uint32 Zone = 0;
	double SwellPh = 0;
	FKeysReefBed Bed;

	// animals other files told us about (Heard), kept a second
	struct FNear { FVector P = FVector::ZeroVector; double N = 0, At = -100, D = 1e9; FString Who; };
	FNear Grunt, GrouperBoid, Pod;
	// ... and the groupers found among the spear targets
	FNear GrouperLife;
	double TargetT = 0;
	double GruntT = 3, BoomT = 12, LastBoom = -100, LastGrouperD = 1e9, WhistleT = 4, ClickT = 7;
	double LastFlush = -100;

	// test hooks
	int32 Force = 0;                          // 1: as if under water, 2: as if above
	bool bBedTest = false;
	KeysReefAudio::FBedIn BedTest;
	TArray<TPair<double, FString>> Queue;     // the "all" command: (time, sound)
	FString RecName;
	// the log
	double LogT = 0, CpuAcc = 0, CpuMax = 0;
	int32 CpuN = 0, Played = 0, Skipped = 0, Dropped = 0;
	double Now = 0;
};

void KeysReefAudio::Heard(const TCHAR* What, const FString& Who, const FVector& GamePos, double Amount)
{
	if (UKeysReefAudioSub* S = GReefAudio.Get()) S->OnHeard(What, Who, GamePos, Amount);
}

static FAutoConsoleCommand GReefAudioCmd(TEXT("kr.reefaudio"), TEXT("Reef and mangrove sound test hooks: stats, test <sound> [metres], all, list, dump, rec <seconds> [name], force 0|1|2, bed <cover> <water m> <diver m> | bed off"),
	FConsoleCommandWithArgsDelegate::CreateLambda([](const TArray<FString>& Args) { if (UKeysReefAudioSub* S = GReefAudio.Get()) S->DebugCmd(FString::Join(Args, TEXT(" "))); }));

bool UKeysReefAudioSub::ShouldCreateSubsystem(UObject* Outer) const
{
	return !FParse::Param(FCommandLine::Get(), TEXT("KRNoReefAudio")) && Super::ShouldCreateSubsystem(Outer);
}

bool UKeysReefAudioSub::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

void UKeysReefAudioSub::Deinitialize()
{
	if (GReefAudio.Get() == this) GReefAudio.Reset();
	delete St;
	St = nullptr;
	Super::Deinitialize();
}

TStatId UKeysReefAudioSub::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UKeysReefAudioSub, STATGROUP_Tickables);
}

namespace
{
	// one sound, placed: scales the recipe by distance and power, pans it, hands it to the synth, writes the log line
	struct FPlayed { bool bOk = false; double Dist = 0, Level = 0; int32 Notes = 0; };
}

// (a member in all but name: it needs the private state)
static FPlayed ReefAudioPlay(UKeysReefSynth* Synth, FRandomStream& Rng, const FVector& Cam, const FVector& CamUE, const FVector& RightUE, double Under,
	const FString& Name, const FVector& At, double Power, bool bMuffle, const FString& Why, int32& Skipped)
{
	FPlayed Out;
	const KeysReefAudio::FSoundInfo* Info = KeysReefAudio::FindSound(Name);
	if (!Synth || !Info) return Out;
	const double D = Hyp3(At, Cam);
	Out.Dist = D;
	const double MaxM = bMuffle ? FMath::Min(double(Info->MaxM), MuffledSplashM) : double(Info->MaxM);
	if (D > MaxM) return Out;   // never past its stated distance
	TArray<FKeysReefNote> Notes;
	FString Desc;
	if (!KeysReefAudio::MakeSound(Name, Rng, Notes, Desc)) return Out;
	if (Synth->NotesNow() + Notes.Num() > KeysReefAudio::MaxNotes)
	{
		Skipped++;
		UE_LOG(LogTemp, Log, TEXT("KRReefAudio skip: %s (%s) at %.1f m: %d notes sounding, %d more would pass the cap of %d"), *Name, *Why, D, Synth->NotesNow(), Notes.Num(), KeysReefAudio::MaxNotes);
		return Out;
	}
	// inverse distance from the reference distance, then faded to nothing over the last fifth of its range
	double G = Info->RefM / FMath::Max(D, double(Info->RefM));
	G *= FMath::Clamp((MaxM - D) / (.2 * MaxM), 0.0, 1.0);
	G *= Power * (bMuffle ? MuffleGain : 1.f);
	float PanL = 1.f, PanR = 1.f;
	KeysDSP::Spatialize(CamUE, RightUE, KeysGeo::ToUE(At.X, At.Y, At.Z), 1e6f, 0.f, PanL, PanR);
	// under water a diver cannot tell well where a sound comes from: most of the pan is taken out
	const float PanK = Under > .5 ? .35f : 1.f;
	PanL = FMath::Lerp(1.f, PanL, PanK); PanR = FMath::Lerp(1.f, PanR, PanK);
	float Peak = 0.f;
	for (FKeysReefNote& N : Notes)
	{
		N.Gain *= float(G);
		N.L *= PanL; N.R *= PanR;
		if (bMuffle) N.Lp = MuffleHz;
		Peak = FMath::Max(Peak, N.Gain);
		Synth->PushNote(N);
	}
	Out.bOk = true; Out.Level = Peak; Out.Notes = Notes.Num();
	UE_LOG(LogTemp, Log, TEXT("KRReefAudio play: %s why=[%s] dist=%.1f m (ref %.1f, max %.0f) gain=%.3f level=%.4f (%.1f dB, loudest note's peak)%s notes=%d: %s"),
		*Name, *Why, D, Info->RefM, MaxM, G, Peak, ToDb(Peak), bMuffle ? TEXT(" muffled through the surface (420 Hz low-pass)") : TEXT(""), Notes.Num(), *Desc);
	return Out;
}

void UKeysReefAudioSub::OnHeard(const TCHAR* What, const FString& Who, const FVector& GamePos, double Amount)
{
	if (!St) return;
	FState& S = *St;
	const FString W(What);
	const double D = Hyp3(GamePos, S.Cam);
	auto Keep = [&](FState::FNear& N)
	{
		// the nearest one wins; an old entry is replaced whatever its distance
		if (S.Now - N.At < 1 && D > N.D && !(N.P - GamePos).IsNearlyZero(3.0)) return;
		N.P = GamePos; N.N = Amount; N.At = S.Now; N.D = D; N.Who = Who;
	};
	if (W == TEXT("school"))
	{
		if (Who == TEXT("grunt")) Keep(S.Grunt);
		else if (Who.Contains(TEXT("grouper")) || Who == TEXT("gag")) Keep(S.GrouperBoid);
	}
	else if (W == TEXT("pod")) { if (Who == TEXT("dolphin")) Keep(S.Pod); }
	else if (W == TEXT("chase"))
	{
		// only the damselfish family pops when it drives something off; a parrotfish's chase is silent
		if (!(Who.Contains(TEXT("sergeant")) || Who.Contains(TEXT("damsel")) || Who.Contains(TEXT("gregory")))) return;
		if (!S.bLive || S.Under < .5 || D > ChaseHearM) { S.Dropped++; return; }
		if (ReefAudioPlay(Synth, S.Rng, S.Cam, S.CamUE, S.RightUE, S.Under, TEXT("pop"), GamePos, 1, false, FString::Printf(TEXT("a %s began a chase"), *Who), S.Skipped).bOk) S.Played++;
	}
	else if (W == TEXT("rasp"))
	{
		if (!S.bLive || S.Under < .5 || D > RaspHearM) { S.Dropped++; return; }
		if (ReefAudioPlay(Synth, S.Rng, S.Cam, S.CamUE, S.RightUE, S.Under, TEXT("rasp"), GamePos, 1, false, FString::Printf(TEXT("a %s rubbed its antennae"), Who.IsEmpty() ? TEXT("spiny lobster") : *Who), S.Skipped).bOk) S.Played++;
	}
}

void UKeysReefAudioSub::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	UWorld* W = GetWorld();
	if (!W) return;
	const double T0 = FPlatformTime::Seconds();
	if (!St)
	{
		St = new FState;
		St->bScript = FString(FCommandLine::Get()).Contains(TEXT("KRScript="));
		GReefAudio = this;
	}
	FState& S = *St;
	const double Dt = FMath::Clamp(double(DeltaTime), 0.0, .1);
	S.Now = W->GetTimeSeconds();

	// ---- the synth: our own component on the audio actor, made when that actor is there
	if (!S.Audio.IsValid()) S.Audio = AKeysAudio::Get(W);
	if (!Synth && S.Audio.IsValid())
	{
		Synth = NewObject<UKeysReefSynth>(S.Audio.Get(), TEXT("ReefSynth"));
		Synth->SetupAttachment(S.Audio->GetRootComponent());
		Synth->RegisterComponent();
		Synth->Start();
		UE_LOG(LogTemp, Log, TEXT("KRReefAudio: synth started on %s; caps: %d notes, %d shrimp clicks, 3 noise layers; %d sounds"), *S.Audio->GetName(), KeysReefAudio::MaxNotes, KeysReefAudio::MaxClicks, KeysReefAudio::Sounds().Num());
	}
	if (!Synth) return;

	// ---- where the listener is, and whether anything may be heard at all
	if (!S.Pawn.IsValid()) { TActorIterator<AKeysBoatPawn> It(W); if (It) S.Pawn = *It; }
	const AKeysBoatPawn* Pawn = S.Pawn.Get();
	const AKeysDiver* Dv = AKeysDiver::Get(W);
	if (APlayerController* PC = W->GetFirstPlayerController(); PC && PC->PlayerCameraManager)
	{
		S.CamUE = PC->PlayerCameraManager->GetCameraLocation();
		S.RightUE = PC->PlayerCameraManager->GetCameraRotation().Quaternion().GetRightVector();
		S.Cam = FVector(S.CamUE.X / 100, S.CamUE.Z / 100, S.CamUE.Y / 100);
	}
	S.Under = Dv ? Dv->UnderwaterAmount() : 0;
	if (S.Force == 1) S.Under = 1; else if (S.Force == 2) S.Under = 0;
	S.DiverDepth = FMath::Max(0.0, KeysOcean::SeaLevel() - S.Cam.Y);
	S.WhyNot.Reset();
	if (!Pawn || !Pawn->bRunning) S.WhyNot = TEXT("title screen");
	else if (Pawn->IsCapturingKeys()) S.WhyNot = TEXT("a menu is open");
	else if (Pawn->IsAshore()) S.WhyNot = TEXT("ashore");
	else if (!S.Audio.IsValid() || !S.Audio->IsSoundOn()) S.WhyNot = TEXT("sound off");
	S.bLive = S.WhyNot.IsEmpty();
	const bool bUnder = S.Under > .5;

	// ---- the bottom here: one point a frame, round and round (17 frames for the lot)
	{
		const int32 I = S.CoverNext;
		S.CoverNext = (S.CoverNext + 1) % CoverPts;
		double X = S.Cam.X, Z = S.Cam.Z;
		if (I > 0)
		{
			const double R = I <= 8 ? 15 : 30, An = (I - 1) % 8 * (PI / 4) + (I > 8 ? PI / 8 : 0);
			X += R * FMath::Cos(An); Z += R * FMath::Sin(An);
		}
		const KeysReefLife::FHabitat H = KeysReefLife::HabitatAt(X, Z);
		S.CoverAt[I] = FMath::Clamp(FMath::Max(H.Reef, .55 * H.Hard), 0.0, 1.0);
		if (I == 0) { S.WaterDepth = H.Depth; S.Zone = H.Zone; }
		// under the listener it counts in full, 15 m off most of it, 30 m off about half
		double C = S.CoverAt[0];
		for (int32 k = 1; k < CoverPts; k++) C = FMath::Max(C, S.CoverAt[k] * (k <= 8 ? .85 : .5));
		S.Cover += (C - S.Cover) * FMath::Min(1.0, Dt / 1.2);
	}

	// ---- the bed
	S.SwellPh += Dt / 6.5;
	S.SwellPh -= FMath::FloorToDouble(S.SwellPh);
	KeysReefAudio::FBedIn In;
	In.Cover = S.Cover; In.WaterDepth = S.WaterDepth; In.DiverDepth = S.DiverDepth;
	In.Hour = KeysFeed::HourNow(W);
	In.Sea = FMath::Min(FMath::Max(KeysGeo::SeaSW() * 1.1, KeysGeo::SeaCH()), 2.5);
	In.Swell = (.5 - .5 * FMath::Cos(2 * PI * S.SwellPh)) * (.8 + .2 * FMath::Sin(S.Now * .13));
	if (S.bBedTest) { In.Cover = S.BedTest.Cover; In.WaterDepth = S.BedTest.WaterDepth; In.DiverDepth = S.BedTest.DiverDepth; }
	S.Bed = KeysReefAudio::BedFor(In);
	// reef sound is for a head under water: nothing on deck, on the menus, in town or with the sound off
	S.Bed.Master = S.bBedTest ? 1.f : float(S.bLive ? S.Under : 0.0);
	Synth->SetBed(S.Bed);

	// ---- animals close by, under water
	if (S.bLive && bUnder)
	{
		// grunts: a school within earshot grunts now and then, a big school more often
		if (S.Now - S.Grunt.At < 1 && Hyp3(S.Grunt.P, S.Cam) < GruntHearM)
		{
			S.GruntT -= Dt * FMath::Clamp(S.Grunt.N / 20, .5, 2.0);
			if (S.GruntT <= 0)
			{
				S.GruntT = S.Rng.FRandRange(2.5f, 7.f);
				const FVector P = S.Grunt.P + FVector(S.Rng.FRandRange(-1.f, 1.f), S.Rng.FRandRange(-.3f, .3f), S.Rng.FRandRange(-1.f, 1.f));
				if (ReefAudioPlay(Synth, S.Rng, S.Cam, S.CamUE, S.RightUE, S.Under, TEXT("grunt"), P, 1, false, FString::Printf(TEXT("a school of %.0f grunts within %.0f m"), S.Grunt.N, GruntHearM), S.Skipped).bOk) S.Played++;
			}
		}
		// groupers: the boids at their holes (Heard) and the big ones among the spear targets
		S.TargetT -= Dt;
		if (S.TargetT <= 0)
		{
			S.TargetT = .5;
			S.GrouperLife.At = -100;
			if (const AKeysLife* L = AKeysLife::Get(W))
			{
				TArray<KeysDive::FTarget> Tg;
				L->SpearTargetsNear(S.Cam.X, S.Cam.Y, S.Cam.Z, GoliathHearM, Tg);
				double Best = 1e9;
				for (const KeysDive::FTarget& Tt : Tg)
				{
					const bool bBig = Tt.Key == TEXT("goliath") || Tt.Key == TEXT("warsaw");
					if (!bBig && Tt.Key != TEXT("grouper") && Tt.Key != TEXT("gag") && Tt.Key != TEXT("redgrouper")) continue;
					const FVector P = (Tt.A + Tt.B) * .5;
					const double D = Hyp3(P, S.Cam);
					if (D > (bBig ? GoliathHearM : GrouperHearM) || D > Best) continue;
					Best = D;
					S.GrouperLife.P = P; S.GrouperLife.At = S.Now; S.GrouperLife.D = D; S.GrouperLife.Who = Tt.Key; S.GrouperLife.N = Tt.Weight;
				}
			}
		}
		{
			const FState::FNear* G = nullptr;
			if (S.Now - S.GrouperLife.At < 1) G = &S.GrouperLife;
			if (S.Now - S.GrouperBoid.At < 1 && Hyp3(S.GrouperBoid.P, S.Cam) < GrouperHearM && (!G || Hyp3(S.GrouperBoid.P, S.Cam) < Hyp3(G->P, S.Cam))) G = &S.GrouperBoid;
			if (G)
			{
				const bool bBig = G->Who == TEXT("goliath") || G->Who == TEXT("warsaw");
				const double D = Hyp3(G->P, S.Cam);
				S.BoomT -= Dt;
				// it booms now and then where it lies, and at a diver who has just come close
				const bool bClosed = D < 7 && S.LastGrouperD >= 7 && S.Now - S.LastBoom > 8;
				if (S.BoomT <= 0 || bClosed)
				{
					S.BoomT = S.Rng.FRandRange(18.f, 45.f);
					S.LastBoom = S.Now;
					const FString Why = bClosed ? FString::Printf(TEXT("the diver came within 7 m of a %s"), *G->Who) : FString::Printf(TEXT("a %s at its ledge, now and then"), *G->Who);
					if (ReefAudioPlay(Synth, S.Rng, S.Cam, S.CamUE, S.RightUE, S.Under, bBig ? TEXT("boomgoliath") : TEXT("boom"), G->P, 1, false, Why, S.Skipped).bOk) S.Played++;
				}
				S.LastGrouperD = D;
			}
			else S.LastGrouperD = 1e9;
		}
		// dolphins: whistles and click trains while a pod is within 150 m
		if (S.Now - S.Pod.At < 1 && Hyp3(S.Pod.P, S.Cam) < PodHearM)
		{
			const double Many = FMath::Clamp(S.Pod.N / 4, .6, 1.6);
			S.WhistleT -= Dt * Many; S.ClickT -= Dt * Many;
			const FVector P = S.Pod.P + FVector(S.Rng.FRandRange(-4.f, 4.f), 0, S.Rng.FRandRange(-4.f, 4.f));
			if (S.WhistleT <= 0)
			{
				S.WhistleT = S.Rng.FRandRange(4.f, 11.f);
				if (ReefAudioPlay(Synth, S.Rng, S.Cam, S.CamUE, S.RightUE, S.Under, TEXT("whistle"), P, 1, false, FString::Printf(TEXT("a pod of %.0f dolphins within %.0f m"), S.Pod.N, PodHearM), S.Skipped).bOk) S.Played++;
			}
			if (S.ClickT <= 0)
			{
				S.ClickT = S.Rng.FRandRange(6.f, 16.f);
				if (ReefAudioPlay(Synth, S.Rng, S.Cam, S.CamUE, S.RightUE, S.Under, TEXT("clicks"), P, 1, false, FString::Printf(TEXT("a pod of %.0f dolphins within %.0f m"), S.Pod.N, PodHearM), S.Skipped).bOk) S.Played++;
			}
		}
	}

	// ---- the mangroves: play what the ecosystem queued. The queue is always emptied, heard or not.
	{
		using namespace KeysMangroveEco;
		FName What; int32 Kind = 0; FVector Where; float Power = 1;
		int32 Guard = 0;
		while (PopSound(What, Kind, Where, Power) && Guard++ < 32)
		{
			if (!S.bLive) { S.Dropped++; continue; }
			const FString Ev = What.ToString();
			const TCHAR* KindN = KindName(Kind);
			// the kind's own voice
			const TCHAR* Call = nullptr;
			switch (Kind)
			{
			case GreatBlue: Call = TEXT("croak.greatblue"); break;
			case GreatEgret: Call = TEXT("croak.egret"); break;
			case LittleBlue: case Spoonbill: case Cormorant: case Anhinga: Call = TEXT("croak.small"); break;
			case GreenHeron: Call = TEXT("skeow"); break;
			case NightHeron: Call = TEXT("quok"); break;
			case Ibis: Call = TEXT("honk"); break;
			case Kingfisher: Call = TEXT("rattle"); break;
			case Tern: Call = TEXT("kree"); break;
			case Osprey: Call = TEXT("osprey"); break;
			default: break;   // a pelican is all but silent
			}
			const bool bBigBird = Kind == GreatBlue || Kind == GreatEgret || Kind == Pelican || Kind == Osprey || Kind == Spoonbill || Kind == Cormorant || Kind == Anhinga;
			TArray<TPair<FString, float>> Parts;   // (sound, power)
			auto Part = [&Parts](const TCHAR* Snd, float Pw) { Parts.Emplace(FString(Snd), Pw); };
			bool bSplash = false;
			if (Ev == TEXT("flush"))
			{
				// one flush a quarter second at most: a whole rookery going up is the "flight" event's
				if (S.Now - S.LastFlush < .25) { S.Dropped++; continue; }
				S.LastFlush = S.Now;
				Part(bBigBird ? TEXT("flush.big") : TEXT("flush.small"), 1.f);
				if (Call) Part(Call, 1.f);
			}
			else if (Ev == TEXT("flight")) { Part(TEXT("flight"), FMath::Clamp(Power, .4f, 1.f)); if (Call) Part(Call, .8f); }
			else if (Ev == TEXT("croak") || Ev == TEXT("honk") || Ev == TEXT("quok")) { if (Call) Part(Call, 1.f); }
			else if (Ev == TEXT("rattle")) Part(TEXT("rattle"), 1.f);
			else if (Ev == TEXT("kree")) Part(TEXT("kree"), 1.f);
			else if (Ev == TEXT("whistle")) Part(TEXT("osprey"), 1.f);
			else if (Ev == TEXT("plunge")) { Part(Power >= .75f ? TEXT("plunge.pelican") : TEXT("plunge.small"), FMath::Clamp(Power + .2f, .5f, 1.f)); bSplash = true; }
			else if (Ev == TEXT("jump")) { Part(TEXT("jump"), 1.f); bSplash = true; }
			else if (Ev == TEXT("shower")) { Part(TEXT("shower"), FMath::Clamp(Power + .3f, .5f, 1.f)); bSplash = true; }
			else if (Ev == TEXT("roll")) { Part(TEXT("roll"), 1.f); bSplash = true; }
			else if (Ev == TEXT("crabdrop")) Part(TEXT("crabdrop"), 1.f);
			else if (Ev == TEXT("strike")) Part(TEXT("strike"), 1.f);
			// under water only the splashes come through, muffled
			if (bUnder && !bSplash) { S.Dropped++; continue; }
			bool bAny = false;
			for (const TPair<FString, float>& P : Parts)
				bAny |= ReefAudioPlay(Synth, S.Rng, S.Cam, S.CamUE, S.RightUE, S.Under, P.Key, Where, P.Value, bUnder, FString::Printf(TEXT("mangrove event %s by a %s"), *Ev, KindN), S.Skipped).bOk;
			if (bAny) S.Played++; else S.Dropped++;
		}
	}

	// ---- test hooks: the "all" queue and the recorder
	for (int32 i = S.Queue.Num() - 1; i >= 0; i--)
		if (S.Now >= S.Queue[i].Key)
		{
			DebugCmd(TEXT("test ") + S.Queue[i].Value);
			S.Queue.RemoveAt(i);
		}
	if (!S.RecName.IsEmpty())
	{
		TArray<float> Buf; int32 Rate = 48000;
		if (Synth->TakeRec(Buf, Rate))
		{
			// a 16-bit stereo WAV
			const int32 N = Buf.Num();
			TArray<uint8> Wav;
			Wav.SetNumUninitialized(44 + N * 2);
			auto Put32 = [&Wav](int32 At, uint32 V) { FMemory::Memcpy(Wav.GetData() + At, &V, 4); };
			auto Put16 = [&Wav](int32 At, uint16 V) { FMemory::Memcpy(Wav.GetData() + At, &V, 2); };
			FMemory::Memcpy(Wav.GetData(), "RIFF", 4); Put32(4, 36 + N * 2); FMemory::Memcpy(Wav.GetData() + 8, "WAVEfmt ", 8);
			Put32(16, 16); Put16(20, 1); Put16(22, 2); Put32(24, Rate); Put32(28, Rate * 4); Put16(32, 4); Put16(34, 16);
			FMemory::Memcpy(Wav.GetData() + 36, "data", 4); Put32(40, N * 2);
			float Pk = 0;
			for (int32 i = 0; i < N; i++) { Pk = FMath::Max(Pk, FMath::Abs(Buf[i])); Put16(44 + i * 2, uint16(int16(FMath::Clamp(Buf[i], -1.f, 1.f) * 32767.f))); }
			const FString Path = FPaths::ProjectSavedDir() / TEXT("KRReefAudio") / (S.RecName + TEXT(".wav"));
			const bool bOk = FFileHelper::SaveArrayToFile(Wav, *Path);
			UE_LOG(LogTemp, Log, TEXT("KRReefAudio rec: %s %s, %.1f s at %d Hz, peak %.4f (%.1f dB)"), bOk ? TEXT("wrote") : TEXT("COULD NOT WRITE"), *Path, N / 2.0 / Rate, Rate, Pk, ToDb(Pk));
			S.RecName.Reset();
		}
	}

	// ---- cost, and the bed's log line
	const double Cpu = FPlatformTime::Seconds() - T0;
	S.CpuAcc += Cpu; S.CpuMax = FMath::Max(S.CpuMax, Cpu); S.CpuN++;
	S.LogT += Dt;
	if (S.bScript && S.LogT >= 3) DebugCmd(TEXT("stats"));
}

void UKeysReefAudioSub::DebugCmd(const FString& Cmd)
{
	if (!St || !Synth) return;
	FState& S = *St;
	TArray<FString> A;
	Cmd.ParseIntoArrayWS(A);
	if (!A.Num()) return;
	const FString C = A[0].ToLower();
	if (C == TEXT("stats"))
	{
		double ACpu = 0, ASecs = 0;
		Synth->TakeCost(ACpu, ASecs);
		const double Span = FMath::Max(S.LogT, 1e-3);
		const float OutPk = Synth->TakePeak();
		UE_LOG(LogTemp, Log, TEXT("KRReefAudio bed: %s under=%.2f diver %.1f m down, water %.1f m, cover %.2f (%s), hour %.2f dusk %.2f -> snaps %.1f/s peak %.3f, sizzle %.4f, surge %.4f at %.0f Hz, deep %.4f, lp %.0f Hz, master %.2f | last %.1f s: %d clicks, notes now %d most %d, out peak %.4f (%.1f dB), played %d dropped %d skipped %d | cost: game %.4f ms a frame (worst %.3f), audio %.3f%% of real time"),
			S.bLive ? TEXT("live") : *FString::Printf(TEXT("silent (%s)"), *S.WhyNot), S.Under, S.DiverDepth, S.WaterDepth, S.Cover, KeysReefLife::ZoneName(S.Zone), KeysFeed::HourNow(GetWorld()), KeysReefAudio::DuskAt(KeysFeed::HourNow(GetWorld())),
			S.Bed.SnapRate, S.Bed.SnapGain, S.Bed.Sizzle, S.Bed.Surge, S.Bed.SurgeHz, S.Bed.Deep, S.Bed.Lp, S.Bed.Master,
			Span, Synth->TakeClicks(), Synth->NotesNow(), Synth->PeakNotes(), OutPk, ToDb(OutPk), S.Played, S.Dropped, S.Skipped,
			S.CpuN ? S.CpuAcc / S.CpuN * 1000 : 0.0, S.CpuMax * 1000, ASecs > 0 ? ACpu / ASecs * 100 : 0.0);
		S.LogT = 0; S.CpuAcc = 0; S.CpuMax = 0; S.CpuN = 0;
	}
	else if (C == TEXT("list"))
	{
		for (const KeysReefAudio::FSoundInfo& I : KeysReefAudio::Sounds())
			UE_LOG(LogTemp, Log, TEXT("KRReefAudio sound: %s, %s, reference %.1f m, never past %.0f m"), I.Name, I.bReef ? TEXT("under water") : TEXT("above water"), I.RefM, I.MaxM);
	}
	else if (C == TEXT("test") && A.Num() >= 2)
	{
		// a sound at its reference distance (or the metres given), dead ahead of the camera, whatever the rules say
		const KeysReefAudio::FSoundInfo* I = KeysReefAudio::FindSound(A[1]);
		if (!I) { UE_LOG(LogTemp, Warning, TEXT("KRReefAudio test: no sound called %s"), *A[1]); return; }
		const double M = A.Num() >= 3 ? FCString::Atod(*A[2]) : double(I->RefM);
		const FVector FwdUE = FVector::CrossProduct(S.RightUE, FVector::UpVector).GetSafeNormal();
		const FVector At = S.Cam + FVector(FwdUE.X, 0, FwdUE.Y) * M;
		ReefAudioPlay(Synth, S.Rng, S.Cam, S.CamUE, S.RightUE, S.Under, A[1], At, 1, A.Num() >= 4 && A[3] == TEXT("muffled"), TEXT("test command"), S.Skipped);
	}
	else if (C == TEXT("all"))
	{
		// every sound in turn, 2.5 s apart, at its reference distance
		double T = S.Now + .5;
		for (const KeysReefAudio::FSoundInfo& I : KeysReefAudio::Sounds()) { S.Queue.Emplace(T, FString(I.Name)); T += 2.5; }
		UE_LOG(LogTemp, Log, TEXT("KRReefAudio all: %d sounds queued, %.1f s"), S.Queue.Num(), T - S.Now);
	}
	else if (C == TEXT("rec") && A.Num() >= 2)
	{
		S.RecName = A.Num() >= 3 ? A[2] : TEXT("rec");
		Synth->StartRec(FCString::Atod(*A[1]));
		UE_LOG(LogTemp, Log, TEXT("KRReefAudio rec: recording %.1f s of the reef synth's output as %s"), FCString::Atod(*A[1]), *S.RecName);
	}
	else if (C == TEXT("force") && A.Num() >= 2) { S.Force = FCString::Atoi(*A[1]); UE_LOG(LogTemp, Log, TEXT("KRReefAudio force: %d"), S.Force); }
	else if (C == TEXT("bed"))
	{
		S.bBedTest = A.Num() >= 4;
		if (S.bBedTest) { S.BedTest.Cover = FCString::Atod(*A[1]); S.BedTest.WaterDepth = FCString::Atod(*A[2]); S.BedTest.DiverDepth = FCString::Atod(*A[3]); }
		UE_LOG(LogTemp, Log, TEXT("KRReefAudio bed test: %s cover %.2f water %.1f m diver %.1f m"), S.bBedTest ? TEXT("on") : TEXT("off"), S.BedTest.Cover, S.BedTest.WaterDepth, S.BedTest.DiverDepth);
	}
	else if (C == TEXT("dump"))
	{
		// every recipe as the synth is given it (seed 1), and the bed for a set of places, for the offline renderer
		FString J = TEXT("{\n\"sounds\": [\n");
		const TArray<KeysReefAudio::FSoundInfo>& All = KeysReefAudio::Sounds();
		for (int32 i = 0; i < All.Num(); i++)
		{
			FRandomStream R(1 + i);
			TArray<FKeysReefNote> Notes; FString Desc;
			KeysReefAudio::MakeSound(All[i].Name, R, Notes, Desc);
			J += FString::Printf(TEXT(" {\"name\": \"%s\", \"reef\": %d, \"ref\": %g, \"max\": %g, \"desc\": \"%s\", \"notes\": [\n"), All[i].Name, All[i].bReef ? 1 : 0, All[i].RefM, All[i].MaxM, *Desc.ReplaceCharWithEscapedChar());
			for (int32 k = 0; k < Notes.Num(); k++)
			{
				const FKeysReefNote& N = Notes[k];
				J += FString::Printf(TEXT("  {\"tone\": %d, \"wave\": %d, \"f0\": %g, \"f1\": %g, \"f2\": %g, \"vib\": %g, \"vibhz\": %g, \"fc0\": %g, \"fc1\": %g, \"q\": %g, \"filter\": %d, \"am\": %g, \"amhz\": %g, \"gain\": %g, \"att\": %g, \"dur\": %g, \"rel\": %g, \"lp\": %g, \"delay\": %g, \"l\": %g, \"r\": %g}%s\n"),
					N.bTone ? 1 : 0, int32(N.Wave), N.F0, N.F1, N.F2, N.Vib, N.VibHz, N.Fc0, N.Fc1, N.Q, int32(N.Filter), N.Am, N.AmHz, N.Gain, N.Att, N.Dur, N.Rel, N.Lp, N.Delay, N.L, N.R, k + 1 < Notes.Num() ? TEXT(",") : TEXT(""));
			}
			J += FString::Printf(TEXT(" ]}%s\n"), i + 1 < All.Num() ? TEXT(",") : TEXT(""));
		}
		J += TEXT("],\n\"beds\": [\n");
		struct FPlace { const TCHAR* Name; double Cover, Water, Diver, Hour; };
		const FPlace Places[] = {
			{TEXT("reef top, 3 m down in 5 m, noon"), 1, 5, 3, 12}, {TEXT("reef top, 3 m down in 5 m, dusk"), 1, 5, 3, 19.15},
			{TEXT("reef crest, 1.5 m down in 2.5 m, noon"), 1, 2.5, 1.5, 12}, {TEXT("30 m off the reef, 4 m down in 6 m"), .5, 6, 4, 12},
			{TEXT("open sand, 3 m down in 5 m, noon"), 0, 5, 3, 12}, {TEXT("fore reef, 12 m down in 14 m"), .8, 14, 12, 12},
			{TEXT("the wall, 25 m down in 30 m"), .6, 30, 25, 12}, {TEXT("blue water, 25 m down in 60 m"), 0, 60, 25, 12}};
		for (int32 i = 0; i < UE_ARRAY_COUNT(Places); i++)
		{
			KeysReefAudio::FBedIn In; In.Cover = Places[i].Cover; In.WaterDepth = Places[i].Water; In.DiverDepth = Places[i].Diver; In.Hour = Places[i].Hour; In.Sea = 1; In.Swell = 1;
			const FKeysReefBed B = KeysReefAudio::BedFor(In);
			J += FString::Printf(TEXT(" {\"name\": \"%s\", \"cover\": %g, \"water\": %g, \"diver\": %g, \"hour\": %g, \"snaprate\": %g, \"snapgain\": %g, \"sizzle\": %g, \"surge\": %g, \"surgehz\": %g, \"deep\": %g, \"lp\": %g}%s\n"),
				Places[i].Name, Places[i].Cover, Places[i].Water, Places[i].Diver, Places[i].Hour, B.SnapRate, B.SnapGain, B.Sizzle, B.Surge, B.SurgeHz, B.Deep, B.Lp, i + 1 < UE_ARRAY_COUNT(Places) ? TEXT(",") : TEXT(""));
		}
		J += TEXT("]\n}\n");
		const FString Path = FPaths::ProjectSavedDir() / TEXT("KRReefAudio") / TEXT("recipes.json");
		const bool bOk = FFileHelper::SaveStringToFile(J, *Path);
		UE_LOG(LogTemp, Log, TEXT("KRReefAudio dump: %s %s (%d sounds)"), bOk ? TEXT("wrote") : TEXT("COULD NOT WRITE"), *Path, All.Num());
	}
}
