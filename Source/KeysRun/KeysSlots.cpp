#include "KeysSlots.h"
#include "KeysSea.h"
#include "KeysKit.h"
#include "KeysAudio.h"
#include "KeysBoatPawn.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/World.h"
#include "EngineUtils.h"

namespace
{
	enum { Shrimp, Bonefish, Permit, Tarpon, Marlin, Boot };
	constexpr int32 PayThree[6] = {8, 16, 40, 125, 1000, 0};
	constexpr int32 PayShrimpPair = 1, MaxWinMult = 250;
	constexpr double MinSpinGap = 3.2, SpinUp = .9, ReelGap = .45;
	constexpr uint32 Seed = 0x6A11F1;   // the casino's seed (a per-save value in the web game)
	constexpr int32 SlotSalt = 0x51075;
}

const int32 AKeysSlots::Strips[3][24] = {
	{0, 3, 1, 0, 2, 5, 1, 0, 3, 0, 2, 5, 1, 4, 0, 1, 2, 3, 5, 0, 0, 1, 2, 5},
	{0, 3, 1, 0, 2, 5, 1, 0, 3, 0, 2, 5, 1, 0, 4, 1, 2, 0, 3, 5, 0, 1, 2, 5},
	{5, 0, 3, 1, 0, 2, 5, 0, 1, 3, 0, 2, 5, 1, 0, 4, 1, 2, 0, 5, 3, 0, 1, 2}};
const int32 AKeysSlots::Lines[5][3] = {{1, 1, 1}, {0, 0, 0}, {2, 2, 2}, {0, 1, 2}, {2, 1, 0}};
const int32 AKeysSlots::BetLadder[5] = {5, 10, 25, 50, 100};

const TCHAR* AKeysSlots::SymbolName(int32 S)
{
	static const TCHAR* N[] = {TEXT("Shrimp"), TEXT("Bonefish"), TEXT("Permit"), TEXT("Tarpon"), TEXT("Blue Marlin"), TEXT("Old Boot")};
	return N[FMath::Clamp(S, 0, 5)];
}

FLinearColor AKeysSlots::SymbolColor(int32 S)
{
	static const uint32 C[] = {0xf28c6b, 0xb8c4cc, 0x8fa2b0, 0xd9dde2, 0x2f6fa8, 0x6b5a3a};
	return KeysKit::Hex(C[FMath::Clamp(S, 0, 5)]);
}

AKeysSlots::AKeysSlots() { PrimaryActorTick.bCanEverTick = true; }

AKeysSlots* AKeysSlots::Get(const UObject* WorldContext)
{
	UWorld* W = WorldContext ? WorldContext->GetWorld() : nullptr;
	if (!W) return nullptr;
	TActorIterator<AKeysSlots> It(W);
	return It ? *It : nullptr;
}

double AKeysSlots::Rtp() const
{
	// slotOdds: every stop combination, the five lines, the cap
	static double Cached = -1;
	if (Cached >= 0) return Cached;
	double Total = 0;
	for (int32 a = 0; a < ReelLen; a++) for (int32 b = 0; b < ReelLen; b++) for (int32 c = 0; c < ReelLen; c++)
	{
		int32 Mult = 0;
		for (const auto& Ln : Lines)
		{
			const int32 A = Strips[0][(a + Ln[0]) % ReelLen], B = Strips[1][(b + Ln[1]) % ReelLen], C = Strips[2][(c + Ln[2]) % ReelLen];
			if (A == B && B == C) Mult += PayThree[A];
			else if (A == Shrimp && B == Shrimp) Mult += PayShrimpPair;
		}
		Total += FMath::Min(Mult, 5 * MaxWinMult);
	}
	Cached = Total / (ReelLen * ReelLen * ReelLen) / 5;
	return Cached;
}

int32 AKeysSlots::Symbol(int32 Reel, int32 Row) const
{
	const int32 P = FMath::FloorToInt(ReelPos[Reel]);
	return Strips[Reel][((P + Row) % ReelLen + ReelLen) % ReelLen];
}

void AKeysSlots::Open(bool bOn)
{
	bOpen = bOn;
	if (AKeysBoatPawn* P = Cast<AKeysBoatPawn>(UGameplayStatics::GetPlayerPawn(this, 0)))
		if (bOn) P->ToastMsg(FString::Printf(TEXT("Gaff & Gill slots: Space spins, Left / Right the bet, Esc to get up. Every machine pays %.1f cents on the dollar."), Rtp() * 100), 6);
}

void AKeysSlots::Spin()
{
	AKeysBoatPawn* P = Cast<AKeysBoatPawn>(UGameplayStatics::GetPlayerPawn(this, 0));
	if (!P) return;
	const int32 Bet = BetLadder[BetI];
	const double Now = GetWorld()->GetTimeSeconds();
	// canSpin
	FString Why;
	if (Pending > 0 || bSpinning) Why = TEXT("Let the reels finish.");
	else if (P->Cash < Bet) Why = P->Cash < 5 ? TEXT("The slots take cash, and you are out of it. The fish are biting.") : *FString::Printf(TEXT("You only have $%lld."), P->Cash);
	else if (Now - LastSpinT < MinSpinGap) Why = TEXT("Let the reels finish.");
	if (!Why.IsEmpty()) { ResultText = Why; return; }
	P->Debit(Bet);
	// drawStops: mulberry32 seeded from the spin counter, so a reload cannot re-roll a spin
	KeysSea::FMulberry R(uint32(KeysSea::HashCell(Seed, int32(P->Spins & 0x7fffffff), 0, SlotSalt) * 4294967296.0));
	for (int32 i = 0; i < 3; i++) { FromStops[i] = Stops[i]; Stops[i] = FMath::FloorToInt(R() * ReelLen) % ReelLen; }
	P->Spins++;
	// the lines and the pay (payoutFor)
	WinLines.Reset();
	int32 Mult = 0;
	bool bJack = false;
	for (int32 l = 0; l < 5; l++)
	{
		const int32 A = Strips[0][(Stops[0] + Lines[l][0]) % ReelLen], B = Strips[1][(Stops[1] + Lines[l][1]) % ReelLen], C = Strips[2][(Stops[2] + Lines[l][2]) % ReelLen];
		if (A == B && B == C && PayThree[A] > 0) { Mult += PayThree[A]; WinLines.Add(l); bJack |= A == Marlin; }
		else if (A == Shrimp && B == Shrimp) { Mult += PayShrimpPair; WinLines.Add(l); }
	}
	const int32 Win = FMath::Min(FMath::FloorToInt(Mult * (Bet / 5.0)), Bet * MaxWinMult);
	Pending = Win; LastWin = Win;
	Wagered += Bet; Won += Win;
	// teaseReel: a marlin or tarpon pair on a line holds the third reel back
	Tease = -1;
	for (const auto& Ln : Lines)
	{
		const int32 A = Strips[0][(Stops[0] + Ln[0]) % ReelLen], B = Strips[1][(Stops[1] + Ln[1]) % ReelLen];
		if (A == B && (A == Marlin || A == Tarpon)) Tease = 2;
	}
	// resultLine
	if (bJack) ResultText = FString::Printf(TEXT("MARLIN! The bell rings, the gulls panic. %s!"), *KeysKit::Money(Win));
	else if (Win > 0)
	{
		int32 BestSym = -1, BestM = 0;
		bool bPair = false;
		for (int32 l : WinLines)
		{
			const int32 A = Strips[0][(Stops[0] + Lines[l][0]) % ReelLen], B = Strips[1][(Stops[1] + Lines[l][1]) % ReelLen], C = Strips[2][(Stops[2] + Lines[l][2]) % ReelLen];
			const int32 M = A == B && B == C ? PayThree[A] : PayShrimpPair;
			if (M > BestM) { BestM = M; BestSym = A; bPair = !(A == B && B == C); }
		}
		ResultText = bPair ? FString::Printf(TEXT("Two shrimp, the bait pays for itself. %s"), *KeysKit::Money(Win))
			: FString::Printf(TEXT("Three %s%s %s"), *FString(SymbolName(BestSym)).ToLower(), BestSym == Shrimp ? TEXT("") : TEXT("!"), *KeysKit::Money(Win));
	}
	else ResultText = TEXT("Nothing. The machine hums to itself.");
	bSpinning = true; SpinT = 0; LastSpinT = Now;
	if (AKeysAudio* Au = AKeysAudio::Get(this)) Au->PlayExtra(TEXT("SlotSpin"), FVector::ZeroVector, 1);
	P->SaveEconomy();
}

void AKeysSlots::HandleInput(APlayerController* PC)
{
	auto Hit = [PC](const FKey& K) { return PC->WasInputKeyJustPressed(K); };
	if (Hit(EKeys::Escape) || Hit(EKeys::F)) { Open(false); return; }
	if (!bSpinning)
	{
		if (Hit(EKeys::Left) || Hit(EKeys::A) || Hit(EKeys::Down) || Hit(EKeys::S)) BetI = FMath::Max(0, BetI - 1);
		if (Hit(EKeys::Right) || Hit(EKeys::D) || Hit(EKeys::Up) || Hit(EKeys::W)) BetI = FMath::Min(4, BetI + 1);
	}
	if (Hit(EKeys::SpaceBar) || Hit(EKeys::Enter)) Spin();
}

void AKeysSlots::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!bSpinning) { for (int32 i = 0; i < 3; i++) ReelPos[i] = Stops[i]; return; }
	SpinT += DeltaSeconds;
	bool bAll = true;
	for (int32 i = 0; i < 3; i++)
	{
		// sim/reels.ts: spin up, run, an eased stop that overshoots a touch and settles
		const double StopAt = SpinUp + i * ReelGap + (i == Tease ? 1.1 : 0), Settle = .35;
		const double Laps = 3 + i, Dist = Laps * ReelLen + FMath::Fmod(double(Stops[i] - FromStops[i] + ReelLen), double(ReelLen));
		if (SpinT >= StopAt + Settle) { ReelPos[i] = Stops[i]; continue; }
		bAll = false;
		double U;
		if (SpinT < StopAt) { const double K = SpinT / StopAt; U = K * K * (3 - 2 * K) * .97; }
		else { const double K = (SpinT - StopAt) / Settle; U = .97 + .03 * (1 + FMath::Sin(K * PI) * .9); }
		ReelPos[i] = FromStops[i] + Dist * FMath::Min(1.0, U);
		if (SpinT >= StopAt && SpinT - DeltaSeconds < StopAt) if (AKeysAudio* Au = AKeysAudio::Get(this)) Au->PlayExtra(TEXT("SlotStop"), FVector::ZeroVector, 1);
	}
	if (bAll)
	{
		bSpinning = false;
		if (Pending > 0)
		{
			if (AKeysBoatPawn* P = Cast<AKeysBoatPawn>(UGameplayStatics::GetPlayerPawn(this, 0))) P->Credit(Pending);
			if (AKeysAudio* Au = AKeysAudio::Get(this)) Au->PlayExtra(TEXT("SlotWin"), FVector::ZeroVector, Pending >= BetLadder[BetI] * 20 ? 3.f : 1.f);
			Pending = 0;
		}
	}
}
