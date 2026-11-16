#include "KeysCharter.h"
#include "KeysBoatPawn.h"
#include "KeysData.h"
#include "KeysGeo.h"
#include "EngineUtils.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/PlayerController.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

namespace
{
	// how far from a marina's slips its "zone" reaches: inside it the board is up and the charter clock waits
	constexpr double MarinaZoneR = 150;
	const TCHAR* TablePath = TEXT("/Game/KeysRun/Data/DT_CharterRequests.DT_CharterRequests");
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
	bWasAtMarina = bAtMarina;

	// the board's keys
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
		if (!bTimerRunning && !bAtMarina) bTimerRunning = true;
		if (bTimerRunning) TimeLeft = FMath::Max(0.0, TimeLeft - DeltaSeconds);
	}
}

void AKeysCharter::DebugCmd(const FString& Cmd)
{
	if (Cmd == TEXT("away")) bForceAway = true;
	else if (Cmd == TEXT("home")) bForceAway = false;
	else if (Cmd == TEXT("cancel")) CancelCharter();
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
