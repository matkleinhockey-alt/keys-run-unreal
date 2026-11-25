// The action key (E): one key that does what makes sense where you are, and says so on the screen first.
//
// Until now every place had a letter of its own and a rule you could not see: E weighed in (at a marina, slowly), Q tied
// up (beside a berth, more slowly still), J went over the side, F used a counter ashore and prised a box open under
// water. ContextAction() gathers those checks in one place and returns the words for the HUD. It adds no rule of its
// own: each branch calls what the old key called (WeighIn, ToggleAshore, ToggleDive, WalkInteract, TryOpen), and the old
// keys still work.
//
// The order is the answer to "what would you most likely mean here": under water a box in reach before the ladder;
// ashore whatever the walker's own prompt names; at the helm a catch to weigh, then a berth to tie up at, then (stopped,
// anywhere) over the side.

#include "KeysBoatPawn.h"
#include "KeysDiver.h"
#include "KeysWalker.h"
#include "KeysTreasure.h"
#include "KeysHandgun.h"
#include "KeysGeo.h"
#include "KeysTownData.h"
#include "Engine/World.h"

// True when there is something to do here (and it has been done, with bDo). False with words in OutPrompt is a notice:
// there is something here, but not yet ("Slow down to tie up").
bool AKeysBoatPawn::ContextAction(FString& OutPrompt, bool bDo)
{
	OutPrompt.Reset();
	if (!bRunning || Menu != EMenu::None || bBoardOpen || bLoginOpen || bGearOpen || bPhoneOpen) return false;   // (on the phone E chooses a row)
	if (const AKeysHandgun* Pistol = AKeysHandgun::Get(this); Pistol && Pistol->IsOut()) return false;
	// a fish on the card has its own keys (K, Space, T)
	if (FState == EKeysFish::Caught) return false;

	// in the water
	if (AKeysDiver* Dv = AKeysDiver::Get(this); Dv && Dv->IsDiving())
	{
		if (Dv->IsBlackedOut()) return false;
		if (AKeysTreasure* Box = AKeysTreasure::Get(this); Box && Box->bPromptKey)
		{
			OutPrompt = Box->Prompt;
			if (bDo) Box->TryOpen();
			return true;
		}
		if (Dv->CanReboard())
		{
			OutPrompt = TEXT("E  Climb aboard");
			if (bDo) ToggleDive();
			return true;
		}
		return false;
	}

	// ashore: the walker already finds what is in reach (a counter, a sign, the boat) and words it
	if (AKeysWalker* Wk = AKeysWalker::Get(this); Wk && Wk->IsAshore())
	{
		if (Wk->bShopOpen || bCharging || IsLineOut() || Wk->Prompt().IsEmpty()) return false;
		OutPrompt = Wk->Prompt();
		if (bDo) { if (Wk->PromptBoards()) ToggleAshore(); else WalkInteract(); }
		return true;
	}

	// at the helm, with the rod put away
	if (FState != EKeysFish::Idle || bCharging) return false;
	const double Spd = FMath::Abs(Speed);
	if (Spd > 6) return false;   // running past a dock is not a reason to say anything
	// the berth search walks the town's docks: a few times a second is plenty for a prompt
	const double Now = GetWorld()->GetRealTimeSeconds();
	if (Now - BerthCheckT > .25 || Now < BerthCheckT)
	{
		BerthCheckT = Now;
		double BerthX, BerthZ;
		FString BerthName;
		bBerthNear = KeysTown::FindBerth(X, Z, Spec().Len / 2 + 9, BerthX, BerthZ, BerthName);   // the reach AKeysWalker::TryEnter uses
	}
	if (KeysGeo::NearMarina(X, Z) && Cooler.Num())
	{
		if (Spd >= 2.5) { OutPrompt = TEXT("Slow down to weigh in"); return false; }
		OutPrompt = TEXT("E  Weigh in");
		if (bDo) WeighIn();
		return true;
	}
	if (bBerthNear)
	{
		if (Spd > 1.6) { OutPrompt = TEXT("Slow down to tie up"); return false; }
		OutPrompt = TEXT("E  Tie up and step ashore");
		if (bDo) ToggleAshore();
		return true;
	}
	if (Spd <= 1.6)
	{
		OutPrompt = TEXT("E  Dive in");
		if (bDo) ToggleDive();
		return true;
	}
	return false;
}
