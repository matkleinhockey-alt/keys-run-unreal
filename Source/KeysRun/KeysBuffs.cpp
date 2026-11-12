// Timed food and drink buffs from the town's counters, ported from the web game's game/buffs.ts (the runtime: expiry,
// toasts, the strip), sim/shop.ts (BUFF_SECONDS and modifiersFor's food-and-drink block) and sim/steady.ts (what
// steady hands and a few drinks do: the strike window, the hand wobble, too drunk to dive).

#include "KeysBoatPawn.h"

namespace KeysBuffs
{
	struct FDef { const TCHAR* Key; const TCHAR* Label; double Secs; double Steady; double Sway; };
	// steady: multiplies Modifiers.steadyMul; sway: adds to Modifiers.sway (sim/shop.ts modifiersFor)
	static const FDef Defs[] = {
		{TEXT("diner.coffee"), TEXT("Coffee"), 600, 1.25, 0},
		{TEXT("diner.mahi"), TEXT("Mahi sandwich"), 900, 1.12, 0},
		{TEXT("diner.pie"), TEXT("Key lime pie"), 480, 1.08, 0},
		{TEXT("bar.wings"), TEXT("Hot wings"), 600, 1.06, 0},
		{TEXT("diner.breakfast"), TEXT("Fisherman's breakfast"), 900, 1.18, 0},
		{TEXT("icecream.cone"), TEXT("Ice cream"), 300, 1.05, 0},
		{TEXT("conv.snacks"), TEXT("Snacks"), 420, 1.05, 0},
		{TEXT("bar.shot"), TEXT("Key lime shot"), 240, 1, .35},
		{TEXT("liquor.sixer"), TEXT("Six-pack"), 300, 1, .5},
		{TEXT("bar.beer"), TEXT("Cold beer"), 420, 1, .22},
	};
	static const FDef* Find(const FString& Key) { for (const FDef& D : Defs) if (Key == D.Key) return &D; return nullptr; }

	double Seconds(const FString& Key) { const FDef* D = Find(Key); return D ? D->Secs : 0; }
	FString Label(const FString& Key) { const FDef* D = Find(Key); return D ? FString(D->Label) : Key; }
	bool IsDrink(const FString& Key) { return Key == TEXT("bar.shot") || Key == TEXT("liquor.sixer") || Key == TEXT("bar.beer"); }
	FString StartText(const FString& Key)
	{
		if (Key == TEXT("bar.shot")) return Label(Key) + TEXT(": the horizon is optional now. Hands less steady.");
		if (Key == TEXT("liquor.sixer")) return Label(Key) + TEXT(": the deck party approves, your rod does not.");
		if (Key == TEXT("bar.beer")) return Label(Key) + TEXT(": the crowd likes you more, your cast less.");
		return Label(Key) + TEXT(": steadier hands, longer strike window.");
	}
}

bool AKeysBoatPawn::BuffOn(const TCHAR* BuffKey) const
{
	const double* E = Buffs.Find(BuffKey);
	return E && *E > GetWorld()->GetTimeSeconds();
}

double AKeysBoatPawn::Sway() const
{
	double S = 0;
	for (const KeysBuffs::FDef& D : KeysBuffs::Defs) if (D.Sway > 0 && BuffOn(D.Key)) S = FMath::Min(1.0, S + D.Sway);
	return S;
}

double AKeysBoatPawn::SteadyMul() const
{
	double M = 1;
	for (const KeysBuffs::FDef& D : KeysBuffs::Defs) if (D.Steady != 1 && BuffOn(D.Key)) M *= D.Steady;
	const double S = Sway();
	if (S > 0) M *= 1 - .4 * S;   // the drink penalty rides on the same number
	return M;
}

// sim/steady.ts strikeWindowS: steadier hands are forgiven up to half as long again, a drunk strike is late by feel
double AKeysBoatPawn::StrikeWindow(double Base) const
{
	const double S = Base * FMath::Clamp(SteadyMul(), .5, 1.5) * (1 - .25 * FMath::Clamp(Sway(), 0.0, 1.0));
	return FMath::Max(FMath::Min(Base, .8), S);
}

// how much tension the line takes before it parts: the heavy rod, and a hot breakfast steadying the fight (lineMul 1.06)
double AKeysBoatPawn::LineBreakAt() const
{
	double LineMul = HasGear(TEXT("rod.heavy")) ? 1.35 : 1;
	if (HasGear(TEXT("reel.lever"))) LineMul *= 1.05;
	if (BuffOn(TEXT("diner.breakfast"))) LineMul *= 1.06;
	return 1.06 + (LineMul - 1) * .17;
}

void AKeysBoatPawn::LiveBuffs(TArray<FBuffShow>& Out) const
{
	Out.Reset();
	const double T = GetWorld()->GetTimeSeconds();
	for (const TPair<FString, double>& B : Buffs)
		if (B.Value > T) Out.Add({KeysBuffs::Label(B.Key), FMath::CeilToInt(B.Value - T), KeysBuffs::IsDrink(B.Key)});
	Out.Sort([](const FBuffShow& A, const FBuffShow& B) { return A.Left < B.Left; });
}

// game/buffs.ts update: prune what has worn off and say so
void AKeysBoatPawn::UpdateBuffs(double T)
{
	for (auto It = Buffs.CreateIterator(); It; ++It)
	{
		if (It.Value() > T) continue;
		const FString K = It.Key();
		It.RemoveCurrent();
		ShowToast(KeysBuffs::IsDrink(K) ? KeysBuffs::Label(K) + TEXT(" wears off. The dock stops moving.") : KeysBuffs::Label(K) + TEXT(" wears off."));
	}
}
