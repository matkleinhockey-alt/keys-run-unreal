#include "KeysCharter.h"
#include "KeysBoatPawn.h"
#include "KeysData.h"
#include "KeysGeo.h"
#include "EngineUtils.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/PlayerController.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "KeysSave.h"

namespace
{
	// how far from a marina's slips its "zone" reaches: inside it the board is up and the charter clock waits
	constexpr double MarinaZoneR = 150;
	const TCHAR* TablePath = TEXT("/Game/KeysRun/Data/DT_CharterRequests.DT_CharterRequests");
	const TCHAR* TuningPath = TEXT("/Game/KeysRun/Data/DA_CharterTuning.DA_CharterTuning");
}

AKeysCharter::AKeysCharter()
{
	PrimaryActorTick.bCanEverTick = true;
}

AKeysCharter* AKeysCharter::Get(const UObject* WorldContext)
{
	UWorld* W = WorldContext ? WorldContext->GetWorld() : nullptr;
	if (!W) return nullptr;
	TActorIterator<AKeysCharter> It(W);
	return It ? *It : nullptr;
}

void AKeysCharter::BeginPlay()
{
	Super::BeginPlay();
	LoadTable();
	TuningAsset = LoadObject<UCharterTuning>(nullptr, TuningPath);
	UE_LOG(LogTemp, Log, TEXT("KRCharter tuning: %s"), TuningAsset ? *TuningAsset->GetName() : TEXT("defaults (no DA_CharterTuning asset)"));
	RollOffers();
}

void AKeysCharter::LoadTable()
{
	Table = LoadObject<UDataTable>(nullptr, TablePath);
	if (!Table)
	{
		// no cooked asset (a fresh checkout before the editor has built it): read the seed straight from the JSON
		Table = NewObject<UDataTable>(this, TEXT("DT_CharterRequests_Seed"));
		Table->RowStruct = FCharterRequest::StaticStruct();
		FString Json;
		const FString File = FPaths::ProjectContentDir() / TEXT("KeysRun/Data/CharterRequests.json");
		if (FFileHelper::LoadFileToString(Json, *File)) Table->CreateTableFromJSONString(Json);
	}
	UE_LOG(LogTemp, Log, TEXT("KRCharter table %s: %d requests"), *Table->GetName(), Table->GetRowMap().Num());
}

void AKeysCharter::RollOffers()
{
	Offers.Reset();
	if (!Table) return;
	TArray<FName> Names = Table->GetRowNames();
	if (bHasActive) Names.Remove(ActiveRow);   // never re-offer the charter you're already running
	for (int32 i = Names.Num() - 1; i > 0; i--) Names.Swap(i, FMath::RandRange(0, i));
	for (int32 i = 0; i < FMath::Min(3, Names.Num()); i++) Offers.Add(Names[i]);
	UE_LOG(LogTemp, Log, TEXT("KRCharter board rolled: %s"), *FString::JoinBy(Offers, TEXT(", "), [](const FName& N) { return N.ToString(); }));
}

const UCharterTuning* AKeysCharter::Tuning() const
{
	return TuningAsset ? TuningAsset.Get() : GetDefault<UCharterTuning>();
}

const FCharterRequest* AKeysCharter::Offer(int32 I) const
{
	if (!Table || !Offers.IsValidIndex(I)) return nullptr;
	return Table->FindRow<FCharterRequest>(Offers[I], TEXT("Charter offer"));
}

bool AKeysCharter::TakeOffer(int32 I)
{
	const FCharterRequest* R = Offer(I);
	if (!R || bHasActive) return false;
	if (const AKeysBoatPawn* P = Cast<AKeysBoatPawn>(UGameplayStatics::GetPlayerPawn(this, 0)); P && P->bFreeFish) return false;   // no charters in Free Fish
	ActiveCharter = *R;
	ActiveRow = Offers[I];
	bHasActive = true;
	Progress = 0;
	bPendingRelease = bFailed = false;
	FailReason.Reset();
	TripFish.Reset();
	bLeftDock = bBoardAfterSummary = false;
	TimeLeft = ActiveCharter.TimeLimitMinutes * 60.0;
	bTimerRunning = false;   // the clock starts when you leave the marina zone
	if (AKeysBoatPawn* P = Cast<AKeysBoatPawn>(UGameplayStatics::GetPlayerPawn(this, 0)))
		P->ShowMsg(FString::Printf(TEXT("Charter booked: %s. The clock starts when you leave the marina."), *ActiveCharter.ClientName), 4);
	UE_LOG(LogTemp, Log, TEXT("KRCharter took %s (%s) - %s, %.0f min, $%d"), *ActiveRow.ToString(), *ActiveCharter.ClientName, *TargetLine(ActiveCharter), ActiveCharter.TimeLimitMinutes, ActiveCharter.BasePay);
	RollOffers();   // the board re-rolls once you've picked: the other two go back in the pile
	return true;
}

void AKeysCharter::CancelCharter()
{
	if (!bHasActive) return;
	bHasActive = false;
	bTimerRunning = false;
	TimeLeft = 0;
	Progress = 0;
	bPendingRelease = bFailed = false;
	FailReason.Reset();
	ActiveRow = NAME_None;
	RollOffers();
}

FString AKeysCharter::MethodName(ECharterMethod M)
{
	switch (M)
	{
	case ECharterMethod::Cast: return TEXT("on a cast");
	case ECharterMethod::Troll: return TEXT("trolled");
	case ECharterMethod::Spear: return TEXT("speared");
	default: return FString();
	}
}

FString AKeysCharter::TargetLine(const FCharterRequest& R)
{
	FString What;
	if (R.TargetSpecies.Num() == 0) What = TEXT("any fish");
	else if (R.TargetSpecies.Num() == 1)
	{
		const KeysData::FSpecies* Sp = KeysData::Species(R.TargetSpecies[0].ToString());
		What = Sp ? Sp->Name : R.TargetSpecies[0].ToString();
	}
	else What = TEXT("snapper (any kind)");
	FString S = FString::Printf(TEXT("%d x %s"), R.RequiredCount, *What);
	if (R.MinWeight > 0) S += FString::Printf(TEXT(" over %.0f lb"), R.MinWeight);
	const FString M = MethodName(R.Method);
	if (!M.IsEmpty()) S += TEXT(", ") + M;
	if (R.CatchAndRelease) S += TEXT(", released");
	return S;
}

FString AKeysCharter::WhereLine(const FCharterRequest& R)
{
	// where the target is likeliest: each zone scored by the target's share of what bites there, best two named
	static const TCHAR* Zones[] = {TEXT("Flats"), TEXT("Backcountry"), TEXT("Creek"), TEXT("Bridge"), TEXT("Hawk Channel"), TEXT("Reef"), TEXT("Offshore")};
	if (R.TargetSpecies.Num() == 0) return TEXT("Where: anywhere - the reef is a safe bet");
	TArray<TPair<double, FString>> Score;
	for (const TCHAR* Z : Zones)
	{
		double Tot = 0, Hit = 0;
		for (const KeysData::FZoneRow& Row : KeysData::ZoneTable(Z))
		{
			Tot += Row.W;
			if (R.TargetSpecies.Contains(FName(*Row.Key))) Hit += Row.W;
		}
		if (Hit > 0 && Tot > 0) Score.Add({Hit / Tot, Z});
	}
	if (!Score.Num()) return FString();
	Score.Sort([](const TPair<double, FString>& A, const TPair<double, FString>& B) { return A.Key > B.Key; });
	FString S = TEXT("Where: ") + Score[0].Value;
	if (Score.Num() > 1 && Score[1].Key > Score[0].Key * .35) S += TEXT(", or ") + Score[1].Value;
	if (R.Method == ECharterMethod::Troll) S += TEXT(" - troll the deep edge");
	else if (R.Method == ECharterMethod::Spear) S += TEXT(" - dive it");
	return S;
}

void AKeysCharter::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	AKeysBoatPawn* P = Cast<AKeysBoatPawn>(UGameplayStatics::GetPlayerPawn(this, 0));
	if (!P) return;
	// Free Fish is a sandbox: no board, no charter, no clock
	if (P->bFreeFish)
	{
		if (bHasActive) CancelCharter();
		bAtMarina = bWasAtMarina = false;
		return;
	}
	// in a marina's zone: near its line of slips
	bAtMarina = false;
	for (const KeysGeo::FMarina& M : KeysGeo::Marinas())
	{
		const double DZ = FMath::Clamp(P->BoatZ(), FMath::Min(M.SZ, M.EZ), FMath::Max(M.SZ, M.EZ));
		if (FMath::Sqrt(FMath::Square(P->BoatX() - M.SX) + FMath::Square(P->BoatZ() - DZ)) < MarinaZoneR) { bAtMarina = true; Marina = M.Name; break; }
	}
	if (bForceAway) bAtMarina = false;
	if (bAtMarina && !bWasAtMarina && !bHasActive) RollOffers();   // a fresh board each time you come in
	// the trip ends when you come back into a marina's zone after leaving it - done, short, or failed - with the summary
	if (bHasActive && bLeftDock && bAtMarina && !bSummaryOpen) FinishTrip(false);
	if (ResultT > 0) ResultT = FMath::Max(0.0, ResultT - DeltaSeconds);
	bWasAtMarina = bAtMarina;

	// the summary: Enter closes it, back to a fresh board
	if (bSummaryOpen)
	{
		if (APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0); PC && (PC->WasInputKeyJustPressed(EKeys::Enter) || PC->WasInputKeyJustPressed(EKeys::F4))) CloseSummary();
		return;
	}
	// the board's keys
	if (IsBoardShown() && !bAtMarina)
		if (APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0); PC && PC->WasInputKeyJustPressed(EKeys::F4)) bBoardAfterSummary = false;   // put the board away out at sea
	if (IsBoardShown())
		if (APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0))
		{
			if (PC->WasInputKeyJustPressed(EKeys::F1)) TakeOffer(0);
			else if (PC->WasInputKeyJustPressed(EKeys::F2)) TakeOffer(1);
			else if (PC->WasInputKeyJustPressed(EKeys::F3)) TakeOffer(2);
		}
	if (bHasActive)
	{
		if (APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0); PC && PC->WasInputKeyJustPressed(EKeys::F4)) { CancelCharter(); return; }
		// the clock runs from the moment you clear the marina zone
		if (!bTimerRunning && !bAtMarina && !bFailed && !bLeftDock) { bTimerRunning = true; bLeftDock = true; }
		if (bTimerRunning)
		{
			TimeLeft = FMath::Max(0.0, TimeLeft - DeltaSeconds);
			// out of time before you got back: the trip still ends, late, wherever you are
			if (TimeLeft <= 0) FinishTrip(true);
		}
	}
}

void AKeysCharter::DebugCmd(const FString& Cmd)
{
	if (Cmd == TEXT("away")) bForceAway = true;
	else if (Cmd == TEXT("home")) bForceAway = false;
	else if (Cmd == TEXT("cancel")) CancelCharter();
	else if (Cmd == TEXT("close")) CloseSummary();
	else if (Cmd.StartsWith(TEXT("clock:"))) TimeLeft = FCString::Atod(*Cmd.Mid(6));   // test: set the time left, seconds
	else if (Cmd == TEXT("roll")) RollOffers();
	else if (Cmd.StartsWith(TEXT("take:"))) TakeOffer(FCString::Atoi(*Cmd.Mid(5)));
	else if (Cmd.StartsWith(TEXT("pick:")))
	{
		// test: re-roll the board until that request is on it, then take it from its slot, as F1-F3 would
		const FName Want(*Cmd.Mid(5));
		int32 Rolls = 0, Slot = INDEX_NONE;
		while ((Slot = Offers.IndexOfByKey(Want)) == INDEX_NONE && Rolls++ < 200) RollOffers();
		UE_LOG(LogTemp, Log, TEXT("KRCharter pick %s: on the board in slot F%d after %d re-rolls"), *Want.ToString(), Slot + 1, Rolls);
		if (Slot != INDEX_NONE && !TakeOffer(Slot)) UE_LOG(LogTemp, Warning, TEXT("KRCharter pick %s: TakeOffer refused (board shown %d, active %d)"), *Want.ToString(), IsBoardShown() ? 1 : 0, bHasActive ? 1 : 0);
	}
	else if (Cmd == TEXT("state"))
	{
		UE_LOG(LogTemp, Log, TEXT("KRCharter state: at marina %d (%s), board shown %d, active %s, timer running %d, left %.1f s of %.0f, board: %s"),
			bAtMarina ? 1 : 0, *Marina, IsBoardShown() ? 1 : 0, bHasActive ? *ActiveRow.ToString() : TEXT("none"), bTimerRunning ? 1 : 0, TimeLeft, bHasActive ? ActiveCharter.TimeLimitMinutes * 60.0 : 0.0,
			*FString::JoinBy(Offers, TEXT(", "), [](const FName& N) { return N.ToString(); }));
	}
}

// ================================================================ catches against the charter
FString AKeysCharter::ShortTarget(const FCharterRequest& R, int32 N)
{
	if (R.TargetSpecies.Num() == 0) return N == 1 ? TEXT("fish") : TEXT("fish");
	if (R.TargetSpecies.Num() > 1) return TEXT("snapper");
	const KeysData::FSpecies* Sp = KeysData::Species(R.TargetSpecies[0].ToString());
	return Sp ? Sp->Name : R.TargetSpecies[0].ToString();
}

void AKeysCharter::ShowResult(const FString& Text, const FString& Sub, bool bGood)
{
	ResultText = Text; ResultSub = Sub; bResultGood = bGood; ResultT = 4.0;
	UE_LOG(LogTemp, Log, TEXT("KRCharter result: %s | %s | progress %d/%d%s%s"), *Text, *Sub, Progress, ActiveCharter.RequiredCount, bFailed ? TEXT(" FAILED") : TEXT(""), IsComplete() ? TEXT(" COMPLETE") : TEXT(""));
}

void AKeysCharter::Fail(const FString& Why)
{
	bFailed = true;
	bPendingRelease = false;
	FailReason = Why;
	bTimerRunning = false;
	ShowResult(Why, FString::Printf(TEXT("Charter failed - %s won't be booking again."), *ActiveCharter.ClientName), false);
}

void AKeysCharter::LogFish(const FString& Name, double Weight, bool bCounted, const FString& Verdict)
{
	FTripFish F;
	F.Name = Name; F.Weight = Weight; F.bCounted = bCounted; F.Verdict = Verdict;
	TripFish.Add(F);
}

void AKeysCharter::OnLanded(const FString& SpeciesKey, const FString& SpeciesName, double Weight, ECharterMethod How)
{
	bPendingRelease = false;
	if (!bHasActive || bFailed || bSummaryOpen) return;
	const FCharterRequest& R = ActiveCharter;
	const FString Lb = FString::Printf(TEXT("%.0f lb %s"), Weight, *SpeciesName.ToLower());
	if (R.TargetSpecies.Num() && !R.TargetSpecies.Contains(FName(*SpeciesKey)))
	{
		LogFish(SpeciesName, Weight, false, TEXT("wrong species"));
		ShowResult(TEXT("Wrong species"), FString::Printf(TEXT("%s - they want %s."), *Lb, *ShortTarget(R, R.RequiredCount)), false);
		return;
	}
	// a catch-and-release charter's fish that's been speared is dead, whatever else: that's the charter gone
	if (R.CatchAndRelease && How == ECharterMethod::Spear) { LogFish(SpeciesName, Weight, false, TEXT("speared - should've been released")); Fail(TEXT("Should've released it")); return; }
	if (Weight < R.MinWeight)
	{
		LogFish(SpeciesName, Weight, false, FString::Printf(TEXT("too small (needs %.0f lb)"), R.MinWeight));
		ShowResult(FString::Printf(TEXT("Too small (needs %.0f lb)"), R.MinWeight), FString::Printf(TEXT("That one's %s."), *Lb), false);
		return;
	}
	if (R.Method != ECharterMethod::Any && R.Method != How)
	{
		LogFish(SpeciesName, Weight, false, TEXT("wrong method"));
		ShowResult(TEXT("Wrong method"), FString::Printf(TEXT("It has to be %s - that one was %s."), *MethodName(R.Method), *MethodName(How)), false);
		return;
	}
	if (R.CatchAndRelease)
	{
		bPendingRelease = true;
		PendingLine = Lb; PendingName = SpeciesName; PendingWeight = Weight;
		ShowResult(TEXT("That's the one - release it!"), TEXT("Space releases it (it only counts if it swims away)."), true);
		return;
	}
	const bool bExtra = IsComplete();
	Progress++;
	LogFish(SpeciesName, Weight, true, bExtra ? TEXT("counted - bonus fish") : TEXT("counted"));
	ShowResult(FString::Printf(TEXT("Counts! %s ✓"), *Lb), bExtra ? FString::Printf(TEXT("Bonus fish - %d over the %d they asked for."), Progress - R.RequiredCount, R.RequiredCount)
		: FString::Printf(TEXT("%d / %d %s"), Progress, R.RequiredCount, *ShortTarget(R, R.RequiredCount)), true);
	if (!bExtra && IsComplete()) ResultSub = TEXT("That's the lot - head back to the marina!");
}

void AKeysCharter::OnKept()
{
	if (!bHasActive || !bPendingRelease) return;
	LogFish(PendingName, PendingWeight, false, TEXT("kept - should've been released"));
	Fail(TEXT("Should've released it"));
}

void AKeysCharter::OnReleased()
{
	if (!bHasActive || !bPendingRelease) return;
	bPendingRelease = false;
	const bool bExtra = IsComplete();
	Progress++;
	LogFish(PendingName, PendingWeight, true, bExtra ? TEXT("released - bonus fish") : TEXT("released"));
	ShowResult(FString::Printf(TEXT("Counts! %s released ✓"), *PendingLine), bExtra ? FString::Printf(TEXT("Bonus fish - %d over the %d they asked for."), Progress - ActiveCharter.RequiredCount, ActiveCharter.RequiredCount)
		: FString::Printf(TEXT("%d / %d %s"), Progress, ActiveCharter.RequiredCount, *ShortTarget(ActiveCharter, ActiveCharter.RequiredCount)), true);
	if (!bExtra && IsComplete()) ResultSub = TEXT("That's the lot - head back to the marina!");
}

void AKeysCharter::OnCatchGone()
{
	bPendingRelease = false;
}

// ================================================================ the end of the trip: summary, pay, stars
void AKeysCharter::FinishTrip(bool bLate)
{
	if (!bHasActive || bSummaryOpen) return;
	const UCharterTuning* Tn = Tuning();
	const FCharterRequest& R = ActiveCharter;
	FTripSummary S;
	S.Client = R.ClientName; S.Request = R.RequestText; S.Target = TargetLine(R);
	S.Fish = TripFish;
	S.TimeLimit = R.TimeLimitMinutes * 60.0;
	S.TimeUsed = S.TimeLimit - TimeLeft;
	S.Required = R.RequiredCount;
	S.Counted = Progress;
	S.BasePay = R.BasePay;
	S.bLate = bLate;
	S.bFailed = bFailed;
	S.FailReason = FailReason;
	S.Completion = R.RequiredCount > 0 ? FMath::Clamp(float(Progress) / R.RequiredCount, 0.f, 1.f) : 1.f;
	S.Speed = S.TimeLimit > 0 ? FMath::Clamp(float(TimeLeft / S.TimeLimit), 0.f, 1.f) : 0.f;
	// size: the counted fish against the request's minimum (or, with none, the species' middle weight)
	{
		double Sum = 0; int32 N = 0;
		for (const FTripFish& F : TripFish)
		{
			if (!F.bCounted) continue;
			double Ref = R.MinWeight;
			if (Ref <= 0)
				for (const FName& K : R.TargetSpecies)
					if (const KeysData::FSpecies* Sp = KeysData::Species(K.ToString()); Sp && Sp->Name == F.Name) { Ref = (Sp->Min + Sp->Max) * .5; break; }
			if (Ref <= 0) Ref = F.Weight;   // "anything": every fish is its own yardstick
			Sum += F.Weight / FMath::Max(.01, Ref); N++;
			if (R.MinWeight > 0 && F.Weight >= R.MinWeight * (1 + Tn->BigFishMargin)) S.BigFish++;
		}
		const double Avg = N ? Sum / N : 0;
		S.Size = N ? FMath::Clamp(float((Avg - .5) / FMath::Max(.01, Tn->SizeFullMarksRatio - .5)), 0.f, 1.f) : 0.f;
	}
	S.ExtraFish = FMath::Max(0, Progress - R.RequiredCount);
	// pay: the base (by the fraction done, if you came back short), tips for time left, big fish and extra fish
	const bool bDone = Progress >= R.RequiredCount;
	const double BaseEarned = bDone || !Tn->bPayIncompleteByFraction ? R.BasePay : R.BasePay * S.Completion;
	double TimeTipPct = bDone && !bLate ? Tn->TipPercentPerMinuteLeft * FMath::FloorToDouble(TimeLeft / 60.0) : 0;
	if (Tn->TimeTipCapPercent > 0) TimeTipPct = FMath::Min(TimeTipPct, double(Tn->TimeTipCapPercent));
	S.TipTime = FMath::RoundToInt(R.BasePay * TimeTipPct / 100.0);
	S.TipBig = S.BigFish * Tn->BigFishTip;
	S.TipExtra = S.ExtraFish * Tn->ExtraFishTip;
	double Total = BaseEarned + S.TipTime + S.TipBig + S.TipExtra;
	if (bLate) Total *= Tn->LatePayMultiplier;
	if (bFailed) Total *= Tn->FailedPayMultiplier;
	S.Total = FMath::Max(0, FMath::RoundToInt(Total));
	// stars
	if (bFailed) S.Stars = Tn->FailedStars;
	else if (bLate) S.Stars = Tn->LateStars;
	else
	{
		const float Score = Tn->CompletionWeight * S.Completion + Tn->SpeedWeight * S.Speed * (bDone ? 1.f : 0.f) + Tn->SizeWeight * S.Size;
		const float WSum = FMath::Max(.001f, Tn->CompletionWeight + Tn->SpeedWeight + Tn->SizeWeight);
		const float Norm = Score / WSum;
		S.Stars = 1;
		for (const float Th : Tn->StarThresholds) if (Norm >= Th) S.Stars++;
		S.Stars = FMath::Clamp(S.Stars, 1, 5);
	}
	S.Stars = FMath::Clamp(S.Stars, 1, 5);
	Summary = S;
	bSummaryOpen = true;
	bTimerRunning = false;
	// pay up and save straight away
	if (AKeysBoatPawn* P = Cast<AKeysBoatPawn>(UGameplayStatics::GetPlayerPawn(this, 0)))
	{
		P->Cash += S.Total;
		P->SaveEconomy();
		UE_LOG(LogTemp, Log, TEXT("KRTrip %s: counted %d/%d, late %d, failed %d, time %.0f/%.0f s, base %d (earned %.0f), tips time %d big %d (%d fish) extra %d (%d fish), total $%d, stars %d (completion %.2f speed %.2f size %.2f), money now %lld, saved %s"),
			*ActiveRow.ToString(), S.Counted, S.Required, bLate ? 1 : 0, bFailed ? 1 : 0, S.TimeUsed, S.TimeLimit, S.BasePay, BaseEarned, S.TipTime, S.TipBig, S.BigFish, S.TipExtra, S.ExtraFish, S.Total, S.Stars, S.Completion, S.Speed, S.Size, P->Cash, P->HasSave() ? TEXT("yes") : TEXT("no (test run)"));
		for (const FTripFish& F : S.Fish) UE_LOG(LogTemp, Log, TEXT("KRTrip   fish %s %.1f lb - %s"), *F.Name, F.Weight, *F.Verdict);
	}
	bHasActive = false;
	bPendingRelease = false;
	ActiveRow = NAME_None;
}

void AKeysCharter::CloseSummary()
{
	if (!bSummaryOpen) return;
	bSummaryOpen = false;
	bFailed = false;
	Progress = 0;
	TimeLeft = 0;
	TripFish.Reset();
	RollOffers();                 // three fresh requests
	bBoardAfterSummary = true;    // and the board comes up, even out at sea after a late finish
	UE_LOG(LogTemp, Log, TEXT("KRCharter summary closed - board up with fresh requests"));
}
