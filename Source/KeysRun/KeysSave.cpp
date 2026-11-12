#include "KeysSave.h"
#include "Kismet/GameplayStatics.h"

UKeysSave* UKeysSave::LoadOrCreate()
{
	UKeysSave* S = nullptr;
	if (UGameplayStatics::DoesSaveGameExist(SlotName(), 0))
		S = Cast<UKeysSave>(UGameplayStatics::LoadGameFromSlot(SlotName(), 0));
	if (!S) S = Cast<UKeysSave>(UGameplayStatics::CreateSaveGameObject(UKeysSave::StaticClass()));
	return S;
}

void UKeysSave::Write()
{
	UGameplayStatics::SaveGameToSlot(this, SlotName(), 0);
}

void UKeysSave::Record(const FString& Key, double Weight, const FString& Boat)
{
	Count++;
	double& Sp = SpeciesBest.FindOrAdd(Key);
	if (Weight > Sp) Sp = Weight;
	if (BestKey.IsEmpty() || Weight > BestWeight) { BestKey = Key; BestWeight = Weight; }

	FKeysBoardCatch C;
	C.Key = Key;
	C.Weight = Weight;
	C.Boat = Boat;
	C.Date = FDateTime::Now().ToString(TEXT("%Y-%m-%d"));
	int32 At = 0;
	while (At < TopCatches.Num() && TopCatches[At].Weight >= Weight) At++;
	if (At < MaxTop)
	{
		TopCatches.Insert(C, At);
		if (TopCatches.Num() > MaxTop) TopCatches.SetNum(MaxTop);
	}
	Write();
}

bool UKeysSave::RecordScore(int64 Score)
{
	if (Score <= BestScore) return false;
	BestScore = Score;
	Write();
	return true;
}
