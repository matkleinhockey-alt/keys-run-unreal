#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "KeysOnline.generated.h"

class IWebSocket;
class FJsonObject;
class UProceduralMeshComponent;

// A friend's boat as last heard from the server, plus where we're drawing it (smoothed between updates).
struct FKeysFriend
{
	FString Id, Name, Boat;
	double X = 0, Z = 0, H = 0, Y = 0, P = 0, R = 0, V = 0;   // latest from the server (game meters / radians)
	double SX = 0, SZ = 0, SH = 0, SY = 0, SP = 0, SR = 0;      // drawn
	double HeardT = 0, WakeT = 0;
	bool bOn = false, bPlaced = false, bSeen = false;
	int32 Slot = -1;
	FString MeshBoat;
};

struct FKeysCrewCatch { FString Name, Key, Boat, Date; double W = 0; };
struct FKeysCrewScore { FString Name; int64 Score = 0; };

enum class EKeysNet : uint8 { Offline, Connecting, Online, Failed };

// Online play: log in with a name and a crew code, see your crew's boats (and their wakes) live, and share a crew leaderboard.
// Talks JSON over a WebSocket to the small Node server in Server/ (see README). The server address is
// [KeysRun.Online] ServerUrl in Config/DefaultGame.ini, or -KRServer=ws://host:port on the command line.
UCLASS()
class KEYSRUN_API AKeysOnline : public AActor
{
	GENERATED_BODY()

public:
	AKeysOnline();
	static AKeysOnline* Get(const UObject* WorldContext);
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;
	virtual void Tick(float DeltaSeconds) override;

	void Connect(const FString& Name, const FString& Crew);
	void Disconnect();
	void ReportCatch(const FString& Key, double Weight, const FString& Boat, int64 Score);
	void ReportScore(int64 Score);

	EKeysNet NetState() const { return State; }
	bool IsOnline() const { return State == EKeysNet::Online; }
	FString StatusText() const;
	const FString& Name() const { return MyName; }
	const FString& Crew() const { return MyCrew; }
	int32 FriendsOnWater() const;
	// friends' boats on the water (drawn position), for their engine sound
	void FriendBoats(TArray<TPair<FString, FVector2D>>& Out) const { for (const auto& KV : Friends) if (KV.Value.bOn && KV.Value.bPlaced) Out.Add({KV.Key, FVector2D(KV.Value.SX, KV.Value.SZ)}); }

	const TArray<FKeysCrewCatch>& Catches() const { return CrewCatches; }
	const TArray<FKeysCrewScore>& Scores() const { return CrewScores; }

	// for the HUD: name tags (UE world position + text) and chart dots (game meters)
	void GetTags(TArray<TPair<FVector, FString>>& Out) const;
	void GetChartDots(TArray<FVector2D>& Out) const;

	// messages for the player ("Sam joined", "Sam landed a 40 lb cobia"); the boat pawn shows them as toasts
	TArray<FString> Toasts;

private:
	void OnMessage(const FString& Msg);
	void Send(const TSharedRef<FJsonObject>& O);
	void SendState();
	void UpdateFriends(double Dt, double T);
	UProceduralMeshComponent* MeshFor(FKeysFriend& F);

	TSharedPtr<IWebSocket> Socket;
	EKeysNet State = EKeysNet::Offline;
	FString ServerUrl, MyName, MyCrew, MyId, Error;
	bool bWant = false;          // stay connected (reconnect if the link drops)
	double SendT = 0, RetryAt = 0;
	int32 Tries = 0;

	TMap<FString, FKeysFriend> Friends;
	TArray<FKeysCrewCatch> CrewCatches;
	TArray<FKeysCrewScore> CrewScores;

	UPROPERTY() TObjectPtr<USceneComponent> Root;
	UPROPERTY() TArray<TObjectPtr<UProceduralMeshComponent>> Meshes;
	TArray<bool> MeshUsed;
};
