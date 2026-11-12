// The last pass, on the boat pawn: tag-and-release (web shared/sim/tagging.ts, game/catch/tags.ts, catch-flow.ts),
// the town's services (shared/sim/services.ts, game/services/index.ts), the pony bottle's air fills
// (shared/sim/dive-gear.ts), the run's best fish on the dock, and the test hooks for all of it.
#include "KeysBoatPawn.h"
#include "KeysGeo.h"
#include "KeysSea.h"
#include "KeysData.h"
#include "KeysWorld.h"
#include "KeysDiver.h"
#include "KeysWalker.h"
#include "KeysFinal.h"
#include "KeysAudio.h"
#include "KeysOnline.h"
#include "KeysTownData.h"
#include "Engine/World.h"

using namespace KeysGeo;
using namespace KeysData;

namespace
{
	// tagging.ts
	constexpr double GrowthKPerDay = .02, RecapCooldownS = 600, RecapP = .5;
	FDateTime TagNow() { return FDateTime::UtcNow(); }
	double DaysSince(const FString& Iso)
	{
		FDateTime D;
		if (!FDateTime::ParseIso8601(*Iso, D)) return 0;
		return FMath::Max(0.0, (TagNow() - D).GetTotalDays());
	}
	int32 TagNoFromCode(const FString& Code) { return FCString::Atoi(*Code.Replace(TEXT("KR-"), TEXT(""))); }
	FString TagCode(int32 N) { return FString::Printf(TEXT("KR-%05d"), N); }
	// each tag draws its own ceiling between the middle of the species range and the record, from its number
	double GrowthCeiling(const FString& Key, double Lb, int32 TagNo)
	{
		const FSpecies* S = Species(Key);
		if (!S) return Lb;
		uint32 H = uint32(TagNo) ^ 0x9e3779b9u;
		H *= 0x85ebca6bu;
		H = (H ^ (H >> 13)) * 0xc2b2ae35u;
		const double U = double(H ^ (H >> 16)) / 4294967296.0;
		const double Mid = (S->Min + S->Max) / 2;
		return FMath::Max(Lb, Mid + (S->Max - Mid) * U);
	}
	double GrownWeight(const FString& Key, double Lb, double Days, int32 TagNo)
	{
		if (!Species(Key) || !(Days > 0)) return FMath::RoundToDouble(Lb * 100) / 100;
		const double Ceil = GrowthCeiling(Key, Lb, TagNo);
		if (Lb >= Ceil) return FMath::RoundToDouble(Lb * 100) / 100;
		const double W = Ceil - (Ceil - Lb) * FMath::Exp(-GrowthKPerDay * Days);
		return FMath::Min(FMath::RoundToDouble(W * 100) / 100, FMath::RoundToDouble(Ceil * 100) / 100);
	}
	double WanderRadius(double Days) { return FMath::Min(1500.0, 150 + 40 * FMath::Max(0.0, Days)); }
	FString Ago(const FString& Iso)
	{
		FDateTime D;
		if (!FDateTime::ParseIso8601(*Iso, D)) return TEXT("a while ago");
		const double S = FMath::Max(0.0, (TagNow() - D).GetTotalSeconds());
		if (S < 90) return TEXT("just now");
		const TPair<double, const TCHAR*> Units[] = {{86400, TEXT("day")}, {3600, TEXT("hour")}, {60, TEXT("minute")}};
		for (const auto& U : Units)
			if (S >= U.Key) { const int32 V = FMath::FloorToInt(S / U.Key); return FString::Printf(TEXT("%d %s%s ago"), V, U.Value, V == 1 ? TEXT("") : TEXT("s")); }
		return TEXT("just now");
	}

	// services.ts
	uint32 HashSeed(uint32 Seed, int32 A, int32 B, int32 Salt) { return uint32(KeysSea::HashCell(Seed, A, B, Salt) * 4294967296.0); }
	constexpr int32 ScratchSalt = 0x5c7a7c4, ScratchDenom = 1000;
	const int32 ScratchTable[5][2] = {{5, 150}, {10, 80}, {25, 28}, {100, 7}, {500, 1}};
	const TCHAR* ScratchSymbols[6] = {TEXT("palm"), TEXT("conch"), TEXT("marlin"), TEXT("sun"), TEXT("anchor"), TEXT("gull")};
	const TCHAR* CoverLines[4][2] = {
		{TEXT("The doorman looks at your flip-flops, then your wallet. The rope comes off its hook."), TEXT("\"Twenty.\" He takes it without looking at it. The rope comes off its hook.")},
		{TEXT("\"You again.\" A nod that costs him nothing. The rope drops."), TEXT("He stamps your hand with a flamingo. It will not wash off for three days.")},
		{TEXT("\"Captain.\" He has already unhooked the rope. \"Usual spot.\""), TEXT("The doorman holds the door and, for the first time, almost smiles.")},
		{TEXT("The rope is gone before you reach it. \"Evening, boss. Table is warm.\""), TEXT("He takes your twenty like it is a favour he is doing you.")}};
	const TCHAR* InsideLines[3] = {
		TEXT("Inside: magenta haze, a mirror ball with one bad bulb, a band doing a bossa nova of a song you almost know."),
		TEXT("The stage lights go from pink to a pink you did not know existed. Somebody applauds a saxophone."),
		TEXT("The bar is lit like an aquarium. A man in a white suit is explaining crypto to a lamp.")};
	const TCHAR* BottleLines[3] = {
		TEXT("A bottle arrives with a sparkler in it, carried by two staff and a sense of occasion. Heads turn. A man you do not know raises a glass to you."),
		TEXT("The MC stops the show: \"Ladies, gentlemen, and whoever left that boat double-parked: our friend has ordered the big one.\" You are escorted to a booth with a tiny lamp."),
		TEXT("The sparkler goes off like the Fourth of July. The bouncer, from the door, gives you a thumbs up. It is the nicest thing he has ever done.")};
	const TCHAR* TipLines[3] = {
		TEXT("The MC taps the mic: \"A hand for the captain!\" The room does it. You will be charging this to the boat."),
		TEXT("The jar goes round. Somebody on stage tips an imaginary hat at you."),
		TEXT("A towering performer in feathers sweeps a bow in your direction that moves the air in the room.")};
	const TCHAR* WashLines[2] = {
		TEXT("Machine 4, warm, delicate. Forty minutes of orange light and your dignity returns."),
		TEXT("The dryer eats a quarter and gives back a shirt that smells like a field you have never been in.")};
	const TCHAR* Socks[8] = {TEXT("a single argyle"), TEXT("a tube sock with \"DENNIS\" in marker"), TEXT("a sock shaped like a crab"),
		TEXT("a wool hiking sock, in Florida"), TEXT("a child's sock with a little shark"), TEXT("a sock that is somehow wet"),
		TEXT("a fishnet stocking (nobody asks)"), TEXT("a golden sock. Probably painted.")};
	constexpr double SleepOut = .9, SleepHold = 2.2, SleepIn = 1.2;
	int32 BitCount(int64 N) { int32 C = 0; for (uint32 X = uint32(N); X; X &= X - 1) C++; return C; }
}

// ================================================================ tag-and-release
void AKeysBoatPawn::PickRecapture()
{
	CurTag = -1;
	const bool bForce = bForceRecap;
	bForceRecap = false;
	if (Key.IsEmpty()) return;
	const double CX = bTrolling ? X : Bob.X, CZ = bTrolling ? Z : Bob.Z;
	int32 Best = -1;
	double BestD = 1e18;
	for (int32 i = 0; i < Tags.Num(); i++)
	{
		const FKeysTagged& F = Tags[i];
		if (F.bHarvested || F.Species != Key) continue;
		const double Days = DaysSince(F.LastSeen);
		if (!bForce && Days * 86400 < RecapCooldownS) continue;
		const double D = FMath::Sqrt(FMath::Square(F.X - CX) + FMath::Square(F.Z - CZ));
		if (!bForce && D > WanderRadius(Days)) continue;
		if (D < BestD) { BestD = D; Best = i; }
	}
	if (Best < 0 || (!bForce && FMath::FRand() >= RecapP)) return;
	CurTag = Best;
	const FKeysTagged& F = Tags[Best];
	Weight = FMath::RoundToDouble(GrownWeight(F.Species, F.Weight, DaysSince(F.LastSeen), TagNoFromCode(F.Code)) * 10) / 10;
}

void AKeysBoatPawn::SetCardTag()
{
	CardTagHead.Reset();
	CardTagLines.Reset();
	if (!Tags.IsValidIndex(CurTag)) { CurTag = -1; return; }
	const FKeysTagged& F = Tags[CurTag];
	const int32 N = F.Events.Num();
	CardTagHead = FString::Printf(TEXT("Tagged fish %s - landed %d time%s before you"), *F.Code, N, N == 1 ? TEXT("") : TEXT("s"));
	if (N) CardTagLines.Add(FString::Printf(TEXT("Tagged by %s %s at %.1f lb"), *F.Events[0].Name, *Ago(F.Events[0].At), F.Events[0].Weight));
	const int32 Rest = N - 1;
	if (Rest > 3) CardTagLines.Add(FString::Printf(TEXT("...%d earlier recapture%s"), Rest - 3, Rest - 3 == 1 ? TEXT("") : TEXT("s")));
	for (int32 i = FMath::Max(1, N - 3); i < N; i++) CardTagLines.Add(FString::Printf(TEXT("Caught by %s %s at %.1f lb"), *F.Events[i].Name, *Ago(F.Events[i].At), F.Events[i].Weight));
	const double Gain = Weight - (N ? F.Events.Last().Weight : F.Weight);
	CardTagLines.Add(Gain >= .05 ? FString::Printf(TEXT("Grown %.1f lb since it was last in hand"), Gain) : FString(TEXT("Hasn't had time to grow since it was last in hand")));
	if (bCanKeep) CardNote = FString::Printf(TEXT("Keeping it retires tag %s; releasing it reports the recapture (+60%%) and lets it keep growing."), *F.Code);
}

void AKeysBoatPawn::ReportTag(bool bKept)
{
	if (!Tags.IsValidIndex(CurTag)) return;
	FKeysTagged& F = Tags[CurTag];
	const FString Now = TagNow().ToIso8601();
	F.Weight = Weight;
	F.X = FX; F.Z = FZ;
	F.LastSeen = Now;
	F.bHarvested = bKept;
	FKeysTagEvent E;
	E.Kind = bKept ? TEXT("harvested") : TEXT("released");
	E.Name = TEXT("You"); E.Weight = Weight; E.Zone = Zone; E.X = FX; E.Z = FZ; E.At = Now;
	F.Events.Add(E);
	if (bKept) ShowToast(FString::Printf(TEXT("%s is on ice. Tag %s reported - that fish's story ends in your cooler."), *Sp->Name, *F.Code), 5);
	SaveEconomy();
}

void AKeysBoatPawn::TagAndRelease()
{
	if (FState != EKeysFish::Caught || !Sp) return;
	if (CurTag >= 0) { ShowToast(TEXT("It already carries a tag - Space releases it and reports the recapture.")); return; }
	if (bCardSpeared) { ShowToast(TEXT("A speared fish can't be tagged and let go.")); return; }
	int32 No = 0;
	for (const FKeysTagged& F : Tags) No = FMath::Max(No, TagNoFromCode(F.Code));
	No++;
	const FString Now = TagNow().ToIso8601();
	FKeysTagged F;
	F.Id = FString::Printf(TEXT("local-%d"), No);
	F.Code = TagCode(No);
	F.Species = Key;
	F.Weight = FMath::RoundToDouble(Weight * 100) / 100;
	F.X = FX; F.Z = FZ;
	F.LastSeen = Now;
	FKeysTagEvent E;
	E.Kind = TEXT("tagged"); E.Name = TEXT("You"); E.Weight = F.Weight; E.Zone = Zone; E.X = FX; E.Z = FZ; E.At = Now;
	F.Events.Add(E);
	Tags.Add(F);
	const int32 Bonus = FMath::RoundToInt(LastPts * .4);
	Score += Bonus;
	if (RunBest.Id == Count) RunBest.bReleased = true;
	ShowToast(FString::Printf(TEXT("Tagged and released! +%d conservation bonus. Tag %s is swimming - you'll hear about it if anyone lands that %s."), Bonus, *F.Code, *Sp->Name), 6);
	if (Save) Save->RecordScore(Score);
	if (AKeysOnline* O = AKeysOnline::Get(this)) O->ReportScore(Score);
	SaveEconomy();
	FinishCatch();
}

// ================================================================ the town's services
bool AKeysBoatPawn::ServiceRefuse(const FString& Eff, FString& Why) const
{
	if (Eff == TEXT("motel.night"))
	{
		const AKeysWorld* W = const_cast<AKeysBoatPawn*>(this)->World();
		if ((W && W->IsSunset()) || SleepT >= 0) { Why = TEXT("The desk clerk squints at the sky. \"It is already evening, hon. Rooms are for people who want it to be.\""); return true; }
	}
	else if (Eff == TEXT("club.cover") && SvcOf(TEXT("admitted"))) { Why = TEXT("The doorman checks his clipboard. \"You're already on the list tonight. Go in.\""); return true; }
	else if (Eff == TEXT("club.tip") && !SvcOf(TEXT("admitted"))) { Why = TEXT("The doorman does not take the jar outside. \"Tips go to the staff. Cover first, fisherman.\""); return true; }
	else if (Eff == TEXT("boat.fuel")) { Why = TEXT("The clerk slides the jerry can back. \"Your boat doesn't take it, captain. Pump one's broken, pump two is broken, and it runs fine without either. Keep your thirty bucks.\""); return true; }
	else if (Eff == TEXT("laundromat.wash"))
	{
		const double T = GetWorld()->GetTimeSeconds();
		bool bTipsy = false;
		for (const TPair<FString, double>& B : Buffs) bTipsy |= KeysBuffs::IsDrink(B.Key) && B.Value > T;
		if (!bTipsy) { Why = TEXT("You have nothing to wash off. The machine looks at you the way the bouncer does."); return true; }
	}
	return false;
}

bool AKeysBoatPawn::ServiceResolve(const FString& Eff, FString& Msg)
{
	static const TSet<FString> Keys = {TEXT("motel.night"), TEXT("club.cover"), TEXT("club.bottle"), TEXT("club.tip"), TEXT("liquor.scratch"), TEXT("laundromat.wash"), TEXT("laundromat.sock")};
	if (!Keys.Contains(Eff)) return false;
	// a per-save seed, fixed for the life of the save; a monotonic beat varies the flavour lines
	if (!SvcOf(TEXT("seed"))) Svc.Add(TEXT("seed"), int64(uint32(FDateTime::UtcNow().GetTicks() ^ 0x5bd1e995) | 1u));
	const uint32 Seed = uint32(SvcOf(TEXT("seed")));
	KeysSea::FMulberry Rng(HashSeed(Seed, int32(SvcOf(TEXT("beat"))), 1, 0x77));
	auto PickLine = [&Rng](const TCHAR* const* L, int32 N) { return FString(L[FMath::Min(N - 1, int32(Rng() * N))]); };
	Svc.Add(TEXT("beat"), SvcOf(TEXT("beat")) + 1);
	const FString BoatName = Spec().Brand + TEXT(" ") + Spec().Name;
	auto Card = [this](const TCHAR* Kind, const FString& Title, const TArray<FString>& Lines, double Life)
	{
		SvcCard = FSvcCard();
		SvcCard.Kind = Kind; SvcCard.Title = Title; SvcCard.Lines = Lines; SvcCard.Life = Life; SvcCard.bOn = true;
	};
	if (Eff == TEXT("motel.night"))
	{
		// state at purchase; only the sky waits for the black
		Svc.Add(TEXT("nights"), SvcOf(TEXT("nights")) + 1);
		Svc.Add(TEXT("admitted"), 0);
		Svc.Add(TEXT("tipsTonight"), 0);
		Buffs.Reset();   // eight hours is longer than any buff, and it is how a night sobers you up
		SleepT = 0; bSleepBlack = false;
		SvcCard.bOn = false;
		Msg = TEXT("Room 7. The ice machine hums all night.");
		if (AKeysWalker* W = AKeysWalker::Get(this)) W->bShopOpen = false;
	}
	else if (Eff == TEXT("club.cover"))
	{
		const int64 Spent = SvcOf(TEXT("clubSpent"));
		const int32 Tier = Spent >= 400 ? 3 : Spent >= 100 ? 2 : Spent > 0 ? 1 : 0;
		const FString A = PickLine(CoverLines[Tier], 2), B = PickLine(InsideLines, 3);
		Svc.Add(TEXT("admitted"), 1);
		Svc.Add(TEXT("clubSpent"), Spent + 20);
		Card(TEXT("rope"), TEXT("Pink Flamingo Cabaret"), {A, B}, 9);
		Msg = TEXT("On the list tonight.");
	}
	else if (Eff == TEXT("club.bottle"))
	{
		TArray<FString> L{PickLine(BottleLines, 3)};
		if (!SvcOf(TEXT("admitted"))) L.Add(TEXT("The cover was waived. It always is, for this."));
		Svc.Add(TEXT("admitted"), 1);
		Svc.Add(TEXT("clubSpent"), SvcOf(TEXT("clubSpent")) + 250);
		Card(TEXT("sparkler"), TEXT("Pink Flamingo Cabaret"), L, 9);
		Msg = TEXT("Bottle service.");
	}
	else if (Eff == TEXT("club.tip"))
	{
		const int64 Tips = SvcOf(TEXT("tipsTonight")) + 1;
		TArray<FString> L{PickLine(TipLines, 3)};
		if (Tips >= 3) L.Add(FString::Printf(TEXT("Third tip tonight. The MC now introduces you as \"the captain of the %s, patron of the arts\"."), *BoatName));
		Svc.Add(TEXT("tipsTonight"), Tips);
		Svc.Add(TEXT("clubSpent"), SvcOf(TEXT("clubSpent")) + 10);
		Card(TEXT("spotlight"), TEXT("Pink Flamingo Cabaret"), L, 9);
		Msg = TEXT("Tipped.");
	}
	else if (Eff == TEXT("liquor.scratch"))
	{
		// ticket n is a fixed function of (seed, n) and n only goes up: a reload cannot re-roll it
		const int64 N = SvcOf(TEXT("tickets"));
		KeysSea::FMulberry R(HashSeed(Seed, int32(N), 0, ScratchSalt));
		const int32 Draw = FMath::FloorToInt(R() * ScratchDenom);
		int32 Acc = 0, Prize = 0;
		for (const auto& Row : ScratchTable) { Acc += Row[1]; if (Draw < Acc) { Prize = Row[0]; break; } }
		TArray<FString> Cells;
		auto PickSym = [&R]() { return FString(ScratchSymbols[FMath::Min(5, int32(R() * 6))]); };
		FString WinSym;
		if (Prize > 0)
		{
			WinSym = PickSym();
			TArray<FString> Others;
			for (const TCHAR* S : ScratchSymbols) if (WinSym != S) Others.Add(S);
			TSet<int32> Spots;
			while (Spots.Num() < 3) Spots.Add(FMath::Min(5, int32(R() * 6)));
			for (int32 i = 0; i < 6; i++) Cells.Add(Spots.Contains(i) ? WinSym : Others[FMath::Min(Others.Num() - 1, int32(R() * Others.Num()))]);
		}
		else for (int32 i = 0; i < 6; i++) Cells.Add(PickSym());
		// capAtTwo: no other symbol shows three times
		for (const TCHAR* S : ScratchSymbols)
		{
			if (WinSym == S) continue;
			int32 Seen = 0;
			for (FString& C : Cells)
			{
				if (C != S || ++Seen <= 2) continue;
				for (const TCHAR* Alt : ScratchSymbols)
				{
					int32 Cnt = 0;
					for (const FString& X2 : Cells) Cnt += X2 == Alt;
					if (WinSym != Alt && Cnt < 2) { C = Alt; break; }
				}
			}
		}
		Svc.Add(TEXT("tickets"), N + 1);
		Svc.Add(TEXT("scratchSpent"), SvcOf(TEXT("scratchSpent")) + 5);
		Svc.Add(TEXT("scratchWon"), SvcOf(TEXT("scratchWon")) + Prize);
		Cash += Prize;
		TArray<FString> L;
		if (Prize > 0) { L.Add(FString::Printf(TEXT("Three %ss. You win $%d."), *WinSym, Prize)); L.Add(Prize >= 100 ? TEXT("The clerk checks it against a light. Twice. \"Huh.\"") : TEXT("The clerk nods, unimpressed.")); }
		else L.Add(TEXT("Nothing. The card says \"TRY AGAIN\", as it says to everyone."));
		L.Add(TEXT("Odds: about 1 in 3.8 tickets wins anything, and over time it pays back 69 cents on the dollar. Top prize $500."));
		Card(TEXT("scratch"), TEXT("Scratch & Win"), L, 12);
		SvcCard.Cells = Cells;
		SvcCard.Prize = Prize;
		Msg = Prize > 0 ? FString::Printf(TEXT("Scratch-off: $%d."), Prize) : FString(TEXT("Scratch-off: no luck."));
	}
	else if (Eff == TEXT("laundromat.wash"))
	{
		Svc.Add(TEXT("washes"), SvcOf(TEXT("washes")) + 1);
		for (auto It = Buffs.CreateIterator(); It; ++It) if (KeysBuffs::IsDrink(It.Key())) It.RemoveCurrent();   // the drink only: good buffs survive
		Msg = PickLine(WashLines, 2) + TEXT(" The bar is out of your shirt.");
	}
	else if (Eff == TEXT("laundromat.sock"))
	{
		const int32 I = FMath::Min(7, int32(Rng() * 8));
		const int64 Had = (SvcOf(TEXT("socks")) >> I) & 1;
		const int64 Socks2 = SvcOf(TEXT("socks")) | (int64(1) << I);
		Svc.Add(TEXT("socks"), Socks2);
		const int32 Cnt = BitCount(Socks2);
		Msg = FString::Printf(TEXT("You pull %s out of the lost-and-found bin. "), Socks[I]) + (Had ? FString(TEXT("You already had one of those. Somehow this is worse.")) : FString::Printf(TEXT("Collection: %d of 8."), Cnt));
		if (!Had && Cnt == 8) Msg += TEXT(" That is the whole set. The attendant bows. You have achieved nothing, completely.");
	}
	SaveEconomy();
	return true;
}

double AKeysBoatPawn::SleepAlpha() const
{
	if (SleepT < 0) return 0;
	if (SleepT < SleepOut) return SleepT / SleepOut;
	if (SleepT < SleepOut + SleepHold) return 1;
	return FMath::Max(0.0, 1 - (SleepT - SleepOut - SleepHold) / SleepIn);
}

bool AKeysBoatPawn::SleepCard() const { return SleepT >= SleepOut && SleepT < SleepOut + SleepHold; }

// ================================================================ per frame
void AKeysBoatPawn::UpdateFinal(double Dt, double T)
{
	if (SleepT >= 0)
	{
		SleepT += Dt;
		if (SleepT >= SleepOut && !bSleepBlack)
		{
			// fully black: the only moment the sky may change
			bSleepBlack = true;
			if (AKeysWorld* W = World()) W->SnapSunset();
		}
		if (SleepT >= SleepOut + SleepHold + SleepIn) { SleepT = -1; ShowToast(TEXT("Slept. It is sunset. Progress saved.")); }
	}
	if (SvcCard.bOn)
	{
		SvcCard.Age += Dt;
		APlayerController* PC = Cast<APlayerController>(GetController());
		const bool bKey = PC && SvcCard.Age > .6 && (PC->WasInputKeyJustPressed(EKeys::SpaceBar) || PC->WasInputKeyJustPressed(EKeys::Escape));
		if (SvcCard.Age > SvcCard.Life || bKey) SvcCard.bOn = false;
	}
	// the pony bottle's fill is spent the first time a dive passes 2 m (dive-gear.ts shouldChargeFill)
	if (const AKeysDiver* D = AKeysDiver::Get(this); D && D->IsDiving() && bTankLive && !bFillCharged && D->State().Depth() >= 2)
	{
		bFillCharged = true;
		UseOne(TEXT("dive.fill"));
		SaveEconomy();
		const int32 Left = CountOf(TEXT("dive.fill"));
		ShowToast(Left > 0 ? FString::Printf(TEXT("On the pony bottle - one air fill spent, %d left."), Left) : FString(TEXT("On the pony bottle - that was the last air fill. Scuba Steve's tops it up for $12.")), 4);
	}
	// the underwater portrait follows the card
	if (AKeysFinal* Fn = AKeysFinal::Get(this))
	{
		const bool bWant = FState == EKeysFish::Caught && bCardSpeared;
		if (bWant && !Fn->Portrait()) Fn->ShowPortrait(Key, Weight);
		else if (!bWant && Fn->Portrait()) Fn->HidePortrait();
	}
}

// ================================================================ test hooks: fin:<cmd>
void AKeysBoatPawn::DebugFinal(const FString& Cmd)
{
	FString C = Cmd, A;
	Cmd.Split(TEXT("="), &C, &A);
	if (C.IsEmpty()) C = Cmd;
	if (C == TEXT("tag")) TagAndRelease();
	else if (C == TEXT("recapture")) bForceRecap = true;   // the next fish of a tagged species is the tagged one
	else if (C == TEXT("age"))
	{
		// age=30: every tag was last seen that many days earlier
		const FTimespan Back = FTimespan::FromDays(FCString::Atod(*A));
		for (FKeysTagged& F : Tags)
		{
			FDateTime D;
			if (FDateTime::ParseIso8601(*F.LastSeen, D)) F.LastSeen = (D - Back).ToIso8601();
			for (FKeysTagEvent& E : F.Events) if (FDateTime::ParseIso8601(*E.At, D)) E.At = (D - Back).ToIso8601();
		}
	}
	else if (C == TEXT("svc"))
	{
		// svc=club.cover: as if bought at the counter (no charge)
		FString Why, Msg;
		if (ServiceRefuse(A, Why)) ShowToast(Why, 6);
		else if (ServiceResolve(A, Msg)) ShowToast(Msg, 5);
	}
	else if (C == TEXT("legend"))
	{
		// the boat 40 m off the gold bluefin's school, looking at it
		double LX, LZ, LH;
		if (AKeysFinal* Fn = AKeysFinal::Get(this); Fn && Fn->LegendPose(LX, LZ, LH))
		{
			X = LX + FMath::Cos(LH) * 30; Z = LZ - FMath::Sin(LH) * 30; H = LH; Speed = Thr = DVX = DVZ = 0; bCamInit = false;
			UE_LOG(LogTemp, Log, TEXT("KRScript legend at %.0f, %.0f heading %.2f"), LX, LZ, LH);
		}
	}
	else if (C == TEXT("spout"))
	{
		// a whale blow 40 m off the starboard bow
		if (AKeysFinal* Fn = AKeysFinal::Get(this)) Fn->Spout(X - FMath::Sin(H) * 30 + FMath::Cos(H) * 25, 0, Z - FMath::Cos(H) * 30 - FMath::Sin(H) * 25);
	}
	else if (C == TEXT("best"))
	{
		// best=tarpon,120: the run's best fish
		FString K, W;
		if (A.Split(TEXT(","), &K, &W)) RunBest = {K, FCString::Atod(*W), 999, false, Spec().Brand + TEXT(" ") + Spec().Name};
	}
	else if (C == TEXT("spear"))
	{
		// spear=hogfish: a speared catch card
		LandSpeared(A.IsEmpty() ? FString(TEXT("hogfish")) : A, 14.2, X + 4, Z + 2);
	}
	else if (C == TEXT("wrecksreset")) WrecksFound.Reset();
	else if (C == TEXT("fills")) AddCount(TEXT("dive.fill"), FCString::Atoi(*A));
}
