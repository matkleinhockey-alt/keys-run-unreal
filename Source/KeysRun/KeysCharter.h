#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Engine/DataTable.h"
#include "Engine/DataAsset.h"
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

// Every tuning number for charter pay and ratings, in one asset (Content/KeysRun/Data/DA_CharterTuning) so they can be
// tweaked in the editor without touching code. The defaults here are what a fresh asset starts with.
UCLASS(BlueprintType)
class KEYSRUN_API UCharterTuning : public UPrimaryDataAsset
{
	GENERATED_BODY()
public:
	// tips
	UPROPERTY(EditAnywhere, Category = "Tips") float TipPercentPerMinuteLeft = 10.f;   // % of BasePay per whole minute left on the clock
	UPROPERTY(EditAnywhere, Category = "Tips") float TimeTipCapPercent = 0.f;          // cap on that tip, % of BasePay (0 = no cap)
	UPROPERTY(EditAnywhere, Category = "Tips") float BigFishMargin = .25f;             // a fish this far over MinWeight (0.25 = 25%) earns...
	UPROPERTY(EditAnywhere, Category = "Tips") int32 BigFishTip = 50;                  // ...this much, each
	UPROPERTY(EditAnywhere, Category = "Tips") int32 ExtraFishTip = 75;                // each counted fish beyond RequiredCount
	// penalties
	UPROPERTY(EditAnywhere, Category = "Penalties") float LatePayMultiplier = .5f;      // the clock ran out before you got back
	UPROPERTY(EditAnywhere, Category = "Penalties") float FailedPayMultiplier = 0.f;    // a failed release (kept or speared the fish)
	UPROPERTY(EditAnywhere, Category = "Penalties") bool bPayIncompleteByFraction = true;   // back short of the count: BasePay x fraction done
	// stars: a 0-1 score from completion, speed and fish size, weighted, then thresholds for 2, 3, 4 and 5 stars
	UPROPERTY(EditAnywhere, Category = "Stars") float CompletionWeight = .5f;
	UPROPERTY(EditAnywhere, Category = "Stars") float SpeedWeight = .25f;
	UPROPERTY(EditAnywhere, Category = "Stars") float SizeWeight = .25f;
	UPROPERTY(EditAnywhere, Category = "Stars") TArray<float> StarThresholds = {.3f, .5f, .7f, .85f};
	UPROPERTY(EditAnywhere, Category = "Stars") float SizeFullMarksRatio = 1.5f;        // average counted fish this many times the reference weight = full size marks
	UPROPERTY(EditAnywhere, Category = "Stars") int32 LateStars = 1;
	UPROPERTY(EditAnywhere, Category = "Stars") int32 FailedStars = 1;
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
	bool IsBoardShown() const { return (bAtMarina || bBoardAfterSummary) && !bHasActive && !bSummaryOpen; }
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
	// the trip: every fish landed while the charter is on, and the summary at the end
	struct FTripFish { FString Name; double Weight = 0; bool bCounted = false; FString Verdict; };
	struct FTripSummary
	{
		FString Client, Request, Target;
		TArray<FTripFish> Fish;
		double TimeUsed = 0, TimeLimit = 0;
		int32 Required = 0, Counted = 0, BasePay = 0, TipTime = 0, TipBig = 0, TipExtra = 0, BigFish = 0, ExtraFish = 0, Total = 0, Stars = 1;
		float Completion = 0, Speed = 0, Size = 0;
		bool bLate = false, bFailed = false;
		FString FailReason;
	};
	bool IsSummaryOpen() const { return bSummaryOpen; }
	const FTripSummary& GetSummary() const { return Summary; }
	void CloseSummary();
	const UCharterTuning* Tuning() const;
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
	bool bPendingRelease = false, bFailed = false, bSummaryOpen = false, bBoardAfterSummary = false, bLeftDock = false;
	double PendingWeight = 0;
	FString PendingName;
	FString FailReason, PendingLine;
	TArray<FTripFish> TripFish;
	FTripSummary Summary;
	UPROPERTY() TObjectPtr<UCharterTuning> TuningAsset;
	void FinishTrip(bool bLate);
	void LogFish(const FString& Name, double Weight, bool bCounted, const FString& Verdict);
	void ShowResult(const FString& Text, const FString& Sub, bool bGood);
	void Fail(const FString& Why);

	void LoadTable();
	void RollOffers();
};
