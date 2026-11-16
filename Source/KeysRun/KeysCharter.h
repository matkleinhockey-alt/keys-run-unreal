#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Engine/DataTable.h"
#include "KeysCharter.generated.h"

// Charter Captain mode: clients post requests on the marina's charter board; you take one, run out and (later)
// fish it for them. This is the board and the active charter's clock. Catch validation and payout come later.

UENUM(BlueprintType)
enum class ECharterMethod : uint8
{
	Any,
	Cast,
	Troll,
	Spear
};

// one row of DT_CharterRequests (Content/KeysRun/Data, built from CharterRequests.json by Content/Python)
USTRUCT(BlueprintType)
struct FCharterRequest : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite) FString ClientName;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) FString RequestText;
	// species keys (KeysData); empty means anything that swims
	UPROPERTY(EditAnywhere, BlueprintReadWrite) TArray<FName> TargetSpecies;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float MinWeight = 0;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 RequiredCount = 1;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) ECharterMethod Method = ECharterMethod::Any;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) bool CatchAndRelease = false;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float TimeLimitMinutes = 120;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 BasePay = 0;
};

UCLASS()
class KEYSRUN_API AKeysCharter : public AActor
{
	GENERATED_BODY()

public:
	AKeysCharter();
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	static AKeysCharter* Get(const UObject* WorldContext);

	// the board: shown while you're in a marina's zone with no charter on
	bool IsBoardShown() const { return bAtMarina && !bHasActive; }
	int32 NumOffers() const { return Offers.Num(); }
	const FCharterRequest* Offer(int32 I) const;
	bool TakeOffer(int32 I);   // F1-F3 at the board
	void CancelCharter();      // F4

	// the active charter
	bool HasActiveCharter() const { return bHasActive; }
	const FCharterRequest& GetActiveCharter() const { return ActiveCharter; }
	int32 GetProgress() const { return Progress; }
	bool IsTimerRunning() const { return bTimerRunning; }
	double SecondsLeft() const { return TimeLeft; }
	bool IsAtMarina() const { return bAtMarina; }
	FString MarinaName() const { return Marina; }
	static FString MethodName(ECharterMethod M);
	static FString TargetLine(const FCharterRequest& R);   // "2 x Mahi-Mahi over 8 lb, trolled"

	void DebugCmd(const FString& Cmd);   // charter=board|take:N|cancel|leave

	// ---- checking catches against the active charter (no payout yet)
	// a fish has just come aboard (or been speared): species, weight and method against the request
	void OnLanded(const FString& SpeciesKey, const FString& SpeciesName, double Weight, ECharterMethod How);
	void OnKept();       // K on the catch card: on a catch-and-release charter that's a fail
	void OnReleased();   // Space / T on the catch card: counts on a catch-and-release charter
	void OnCatchGone();  // the card closed some other way
	bool IsPendingRelease() const { return bPendingRelease; }
	bool IsComplete() const { return bHasActive && Progress >= ActiveCharter.RequiredCount; }
	bool IsFailed() const { return bFailed; }
	FString GetFailReason() const { return FailReason; }
	static FString ShortTarget(const FCharterRequest& R, int32 N);   // "snapper", "Mahi-Mahi", "fish"
	// the result banner
	FString ResultText, ResultSub;
	bool bResultGood = false;
	double ResultT = 0;

private:
	UPROPERTY() TObjectPtr<UDataTable> Table;
	TArray<FName> Offers;
	bool bHasActive = false, bTimerRunning = false, bAtMarina = false, bWasAtMarina = false;
	FCharterRequest ActiveCharter;
	FName ActiveRow;
	int32 Progress = 0;
	double TimeLeft = 0;
	FString Marina;
	bool bForceAway = false;
	bool bPendingRelease = false, bFailed = false;
	FString FailReason, PendingLine;
	void ShowResult(const FString& Text, const FString& Sub, bool bGood);
	void Fail(const FString& Why);

	void LoadTable();
	void RollOffers();
};
