#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "KeysHuman.h"
#include "KeysExtras.generated.h"

class UProceduralMeshComponent;
class UPostProcessComponent;
class AKeysBoatPawn;
struct FKeysMesh;
namespace KeysData { struct FBoatSpec; }

// Sound cues for the audio actor. Id is one of: AirHorn, CrowdRoar, MusicDuck (dip the radio to 35% for 3.6 s),
// HypeVoice, CaelenSquawk, CaelenVoice, Splash. Pos is in game meters (x, y up, z south; zero when not positional), Power 0..2.
DECLARE_MULTICAST_DELEGATE_ThreeParams(FOnKeysExtrasSound, FName, FVector, float);

// A floating label for the HUD to project (Unreal world position).
struct FKeysExtrasTag
{
	FVector WorldPos = FVector::ZeroVector;
	FString Text;
	float MaxDist = 900.f;   // meters from the camera
	float MinDist = 0.f;
};

// Deck layout of the procedural hull built by KeysMesh::BuildBoat, so people stand on it (boat-local meters, X bow, Y starboard, Z up).
struct FKeysDeck
{
	double L = 7, HB = 1.3, Free = .9, DeckZ = .5, MidX = .35, TopZ = 2.8, TopL = 1.8, TopW = 1.7;
	const KeysData::FBoatSpec* Spec = nullptr;   // the boats table never moves, so this stays good
	void Init(const KeysData::FBoatSpec& S);
	double SoleAt(double X) const;     // deck height at X
	double InnerHB(double X) const;    // half-width inside the gunwale at X
};

// One of the party crowd that walks laps of the cockpit, dances when the boat is slow, and hangs on when it runs.
struct FKeysPartyGoer
{
	FKeysHuman H;
	double S = 0, Ph = 0, ModeT = 5;
	bool bWalk = true;
	int32 Move = 0;
	double MoveT = 0, Move0 = 0;
	bool bBlend = false;
	FVector AL, AR, FL, FR;
	double Sw = 0, Ln = 0;
};

// The side features from the JS game: the buddy boat (Y), Luigi mode (trim all the way up on plane),
// Caelen the seagull, and the people, party, shower (J), dance pole and flags on your own boat.
UCLASS()
class KEYSRUN_API AKeysExtras : public AActor
{
	GENERATED_BODY()

public:
	static AKeysExtras* Get(const UObject* WorldContext);
	// a cosmetic flag flown from the stern of every boat you take out (content/cosmetics.ts FlagLook); empty for none
	void SetCosmeticFlag(const FString& Key) { if (Key != CosFlag) { CosFlag = Key; LayoutFor.Empty(); } }
	AKeysExtras();
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

	// ---- input. With bHandleKeys the actor polls Y (buddy) and J (shower) itself.
	bool bHandleKeys = true;
	void ToggleBuddy();
	void ToggleShower();
	// X: where travelMiami puts the boat. Returns false (and a toast in OutMsg) if no open water was found.
	static bool MiamiDestination(double BoatX, double BoatZ, double Draft, double& OutX, double& OutZ, double& OutH, FString& OutMsg);
	// buddyWater: nearest open, deep enough water to (X, Z), searching rings out to 900 m
	static bool OpenWater(double X, double Z, double Draft, double& OutX, double& OutZ);

	// ---- for the HUD
	void GetTags(TArray<FKeysExtrasTag>& Out) const;
	bool IsLuigiOn() const { return bLuigiOn; }
	double GetLuigiK() const { return LuigiK; }
	double GetHazeAlpha() const { return LuigiK * .85; }
	// the big "LUIGI MODE" / "NECKKKK FUCK" banner: false when hidden
	bool GetLuigiBanner(float& OutAlpha, float& OutScale, float& OutRotDeg) const;
	// the hype-man lines shouted over the air horn (stands in for speech synthesis); empty when quiet
	FString GetCallout() const;
	bool IsBuddyOn() const { return bBuddyOn; }

	// radio tempo for the dancers; the audio actor can set this from the current station
	double MusicBpm = 96;

	FOnKeysExtrasSound OnSound;

private:
	AKeysBoatPawn* FindPawn();
	void Toast(const FString& Msg, double Secs = 2.6);
	void Sound(FName Id, const FVector& Pos = FVector::ZeroVector, float Power = 1.f);
	double Now() const;
	double PawnSpeed() const;    // m/s, negative in reverse
	double PawnY() const;        // game meters

	// people and props on your boat (KeysCrew.cpp)
	void BuildLayout();
	void UpdateCrew(double Dt, double T);
	void UpdateShower(double Dt, double T);
	void UpdatePole(double Dt, double T);
	void UpdateParty(double Dt, double T);
	void BuildFlag(FKeysMesh& M, double T, double Wind) const;
	void CommitCrew(double T);

	// Luigi mode
	void UpdateLuigi(double Dt, double T);
	void LuigiStart();
	void LuigiEnd();
	void LuigiCleanup();
	void BuildRooster(double Dt, double T);

	// buddy boat
	void BuddyBuild();
	void BuddyPlaceNear();
	void UpdateBuddy(double Dt, double T);

	// Caelen
	void UpdateCaelen(double Dt, double T);
	void BuildGull(FKeysMesh& M, double Up, double Hand) const;

	UPROPERTY() TObjectPtr<USceneComponent> Root;
	UPROPERTY() TObjectPtr<UProceduralMeshComponent> CrewMesh;      // attached to the player's boat: 0 people, 1 props, 2 flag
	UPROPERTY() TObjectPtr<UProceduralMeshComponent> RoosterMesh;   // attached to the player's boat
	UPROPERTY() TObjectPtr<UProceduralMeshComponent> BuddyMesh;     // 0 hull, 1 rod
	UPROPERTY() TObjectPtr<UProceduralMeshComponent> BuddyFish;
	UPROPERTY() TObjectPtr<UProceduralMeshComponent> GullMesh;
	UPROPERTY() TObjectPtr<UPostProcessComponent> PostFX;
	UPROPERTY() TWeakObjectPtr<AKeysBoatPawn> PawnRef;

	bool bAttached = false;
	double CamX = 0, CamZ = 0;   // camera, game meters

	// ---- crew layout
	FString LayoutFor;
	FKeysDeck Deck;
	FKeysHuman Captain, Crew;
	FVector HelmL, HelmR;
	TArray<FKeysPartyGoer> Party;
	TArray<FVector2D> PathPts;
	TArray<double> PathCum;
	double PathTotal = 1;
	bool bHasShower = false, bShowerOn = false;
	int32 ShowerForced = -1;   // -1 automatic, 0 forced off, 1 forced on
	double ShowerT = 10;
	FVector ShowerHead = FVector::ZeroVector;
	FKeysHuman ShowerH;
	bool bHasPole = false;
	FVector PoleBase = FVector::ZeroVector;
	double PoleAng = 0, PoleSpin = 0, PoleClimbT = 6;
	FKeysHuman PoleH;
	int32 FlagKind = 0;   // 0 none, 1 Old Glory, 2 Jolly Roger, 3 the flag you bought at Conch Republic Gifts
	FString CosFlag;
	FVector FlagTop = FVector::ZeroVector;
	double FlagW = 1.5, FlagH = .79;

	// ---- Luigi mode
	bool bLuigiOn = false;
	double LuigiK = 0, LuigiCool = 0, LuigiHeld = 0, LuigiStartT = -99, RoosterScroll = 0;

	// ---- buddy
	bool bBuddyOn = false;
	int32 BuddyIdx = -1;
	double BX = 0, BZ = 0, BH = 0, BSpeed = 0, BY = 0, BPitch = 0, BRoll = 0, BWakeT = 0;
	int32 BState = 0;   // 0 follow, 1 moving, 2 fishing
	bool bBSpot = false;
	double BSpotX = 0, BSpotZ = 0, BStillT = 0, BHookT = 50, BFight = 0, BFX = 0, BFZ = 0;
	FString BKey;
	bool bBJump = false;
	double BJT = 0, BJX = 0, BJZ = 0, BJA = 0;

	// ---- Caelen
	bool bCaelenInit = false;
	double CA = 0, CR = 12, CHt = 8, CTalkT = 25, CSwoop = 0, CBubble = 0, CPh = 0, CYaw = 0, CRoll = 0;
	FVector CPos = FVector::ZeroVector;   // game meters
};
