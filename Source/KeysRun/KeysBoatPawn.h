#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "KeysData.h"
#include "KeysLife.h"
#include "KeysSave.h"
#include "KeysBoatPawn.generated.h"

class UProceduralMeshComponent;
class UStaticMeshComponent;
class UCameraComponent;
class AKeysWorld;
class UKeysSave;
struct FKeysMesh;

// timed food and drink from the town (web sim/shop.ts BUFF_SECONDS, sim/steady.ts BUFF_LABELS): 0 if Key isn't one
namespace KeysBuffs
{
	double Seconds(const FString& Key);
	FString Label(const FString& Key);
	FString StartText(const FString& Key);
	bool IsDrink(const FString& Key);
}

UENUM()
enum class EKeysFish : uint8 { Idle, Casting, Waiting, Nibble, Bite, Fight, Caught };

struct FKeysCooler
{
	FString Name;
	double Weight;
	int32 Pts;
	FString Key;
};

// The player's center console: wave-riding physics, the chase camera, and the whole fishing loop.
UCLASS()
class KEYSRUN_API AKeysBoatPawn : public APawn
{
	GENERATED_BODY()

public:
	AKeysBoatPawn();
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

	// ---- read by the HUD
	bool bRunning = false;
	int32 PickIndex = 1;
	const KeysData::FBoatSpec& Spec() const { return KeysData::Boats()[BoatIndex]; }
	double KnotsNow() const;
	double DepthHere() const;
	FString ZoneName() const;
	FString NearName() const;
	double BoatX() const { return X; }
	double BoatZ() const { return Z; }
	double Heading() const { return H; }
	double Throttle() const { return Thr; }

	EKeysFish FState = EKeysFish::Idle;
	bool bCharging = false;
	double Charge = 0;
	double Tension = 0, Stamina = 1, LineDist = 0;
	int32 Drag = 3;
	FString FightLabel, FightSub;
	bool bStrikeShown = false;

	int64 Score = 0;
	int32 Count = 0;
	TSet<FString> CaughtKeys;
	TArray<FKeysCooler> Cooler;
	double CoolerLb() const;

	// catch card
	FString CardName, CardZone, CardFact, CardBonus, CardNote, CardSex;
	double CardWeight = 0;
	int32 CardPts = 0, CardInches = 0;
	bool bCanKeep = false;

	bool bTrimMode = false;
	double TrimV = .2;
	int32 Gear = 0;   // 0 drive, 1 neutral, 2 reverse
	FString GearName() const { return Gear == 0 ? TEXT("D") : Gear == 1 ? TEXT("N") : TEXT("R"); }
	bool bAutoChase = true, bAutoOn = false;
	FString AutoMode;
	bool bBigMap = false;
	FString Toast;
	double ToastUntil = 0;
	FString Hint() const;
	FString Warning() const;

	// ---- views (1 cycles third person / helm, V toggles the first-person fishing view)
	int32 View = 0;            // 0 third person, 1 first person from the helm
	bool bFpPref = true;       // go first person while a cast is charging or the line is out
	bool bFpOn = false;        // first-person fishing view is active
	bool bDriveOn = false;     // helm view is active
	// ---- electronics (G): chartplotter and fish finder; [ ] zoom the chart
	bool bElectronics = false;
	double ChartRange = 400;   // meters from the boat to the chart edge
	// ---- local leaderboard (B)
	bool bBoardOpen = false;
	int32 BoardTab = 0;        // 0 biggest fish, 1 species records, 2 crew (when online)
	// ---- menus, by mouse or keyboard: the main menu before you start, Esc for the pause menu
	enum class EMenu : uint8 { None, Main, Pause, Settings, Controls };
	struct FMenuItem { FName Id; FString Label, Value; bool bArrows = false; };
	EMenu Menu = EMenu::Main;
	int32 MenuSel = 0;
	void GetMenuItems(TArray<FMenuItem>& Out) const;
	void MenuActivate(FName Id, int32 Dir = 0);   // Dir 0 = choose, -1 / +1 = the left / right arrow
	void OnHudClick(FName Box);                    // a click on any HUD button (menus, leaderboard tabs, login box)
	void OnHudHover(FName Box);
	// true while a menu, the leaderboard or the login box owns the keyboard
	bool IsCapturingKeys() const { return Menu != EMenu::None || bLoginOpen || bBoardOpen; }
	int32 BaitLeft() const { return BaitCasts; }

	// ---- online (L): log in with a name and a crew code
	bool bLoginOpen = false;
	int32 LoginField = 0;      // 0 name, 1 crew code
	FString LoginName, LoginCrew;
	UKeysSave* Board() const;

	// ---- for other systems (game meters, y up; see KeysGeo.h)
	double GetSpeed() const { return Speed; }        // m/s along the heading, negative in reverse (game speed, includes SpeedScale)
	double GetY() const { return Y; }                // hull heave
	double GetVY() const { return VY; }
	double GetPitch() const { return Pitch; }        // radians, bow up positive
	double GetRoll() const { return Roll; }          // radians, starboard up positive
	bool IsAir() const { return bAir; }
	int32 GetGear() const { return Gear; }           // 0 drive, 1 neutral, 2 reverse
	bool IsInFight() const { return FState == EKeysFish::Fight; }
	bool IsLineOut() const { return FState == EKeysFish::Casting || FState == EKeysFish::Waiting || FState == EKeysFish::Nibble || FState == EKeysFish::Bite || FState == EKeysFish::Fight; }
	bool IsFishingActive() const { return bCharging || IsLineOut(); }
	const KeysData::FSpecies* HookedSpecies() const { return Sp; }   // the fish on (or about to bite); null if none
	FString HookedKey() const { return Key; }
	double HookedWeight() const { return Sp ? Weight : 0; }
	double FishX() const { return FX; }
	double FishZ() const { return FZ; }
	double FishDeep() const { return Deep; }         // how far the hooked fish has sounded, meters
	bool IsFishJumping() const { return bJumping; }
	FVector BobberPos() const { return Bob; }        // game meters (x, y up, z)
	bool IsBobberInWater() const { return FState == EKeysFish::Waiting || FState == EKeysFish::Nibble || FState == EKeysFish::Bite; }
	bool IsFirstPerson() const { return bFpOn || bDriveOn; }
	bool FpEyeLocal(FVector& Out) const { Out = FpEyeL; return bFpOn; }   // where you stand to fish, boat-local meters
	// the Freeman's fighting chair: a trolled fish is fought from it. ChairYaw turns it toward the fish (boat frame,
	// 0 = the bow); bInChair while that fight (and its catch card) lasts
	bool InChair() const;
	double GetChairYaw() const { return ChairYaw; }
	double ChairYaw = PI;
	void NudgeBoat(double DX, double DZ, double SpeedMul) { X += DX; Z += DZ; Speed *= SpeedMul; }
	// test hook: put the boat there, stopped (game meters, web heading)
	void DebugPlace(double InX, double InZ, double InH) { X = InX; Z = InZ; H = InH; Speed = Thr = DVX = DVZ = 0; bCamInit = false; }
	// the chase camera's yaw offset, pitch and zoom (tests)
	void DebugCam(double Yaw, double PitchOff, double Zoom) { CamYaw = Yaw; CamPitchOff = PitchOff; CamZoom = Zoom; }

	// ---- diving (J): the diver actor drives the camera and reports toasts and speared catches back here
	bool IsDiving() const;
	void ShowMsg(const FString& Msg, double Secs = 3.2) { ShowToast(Msg, Secs); }
	// a speared fish was hauled in: the same catch card, cooler and scoring as a rod catch
	void LandSpeared(const FString& InKey, double InWeight, double InFX, double InFZ);
	void ApplyDiveCamera(const FVector& PosUE, const FRotator& Rot, float FovH);
	bool bDiveChase = false;   // Settings: start dives in the chase view instead of the dive mask
	// ---- ashore (Q): tie up and walk Boot Key Harbor City; the walker actor drives the camera through ApplyDiveCamera
	bool IsAshore() const;
	int64 Cash = 0;            // dollars: fillets sold at weigh-in, spent in the shops
	TSet<FString> Owned;       // shop items you own (gear, cosmetics)
	TArray<FString> Estates;   // parcels you own (Harbor Row and the Keys Run Estates plat)
	TMap<FString, int32> Counts;            // consumables by count: handgun.ammo, safety.flare, spear.shafts
	TMap<FString, FString> Wearing;         // cosmetic slot -> key (shirt, cap, glasses, flag)
	TArray<FString> TreasureOpened;         // wreck containers emptied
	TMap<FString, FString> EstateBuilds;    // parcel id -> build sheet
	int64 Spins = 0;                        // slot pulls, for the seeded draw
	int32 CountOf(const TCHAR* K) const { const int32* N = Counts.Find(K); return N ? *N : 0; }
	bool UseOne(const TCHAR* K) { int32* N = Counts.Find(K); if (!N || *N <= 0) return false; (*N)--; return true; }
	void AddCount(const TCHAR* K, int32 N) { Counts.FindOrAdd(K) += N; }
	void Credit(int64 N) { Cash += N; SaveEconomy(); }
	bool Debit(int64 N) { if (Cash < N) return false; Cash -= N; SaveEconomy(); return true; }
	void SaveEconomy();
	void ApplyOutfit();
	bool IsBusy(FString& Why) const;        // something that leaving the helm would spoil (a fish on, a catch card, a race)
	void ToastMsg(const FString& Msg, double Secs = 3.2) { ShowToast(Msg, Secs); }
	// leaving the helm for the pistol (or anything else): reel in, bring the spread in, let go of the throttle
	void PrepareToLeaveHelm();
	// another system's camera this frame (the pistol, the tournament's titles and ceremony); false for the boat's own
	bool ExternalCamera(FVector& OutPos, FRotator& OutRot, float& OutFov) const;
	bool bExtCam = false;
	// -KRScript boatcam=x,y,z,yaw,pitch,fov: a camera held in the boat's own frame (meters, degrees), for close-ups
	bool bDbgCam = false;
	FVector DbgCamPos = FVector::ZeroVector;
	FRotator DbgCamRot = FRotator::ZeroRotator;
	float DbgCamFov = 70.f;
	// the glass helm screens and the boat's lights (KeysHelm.cpp); ; switches the underwater and cockpit lights
	void ToggleLights();
	bool bUwLights = false;
	// "Run to" (web ui/runto.ts): the marinas' approaches, Sombrero Reef, the humps and the rigs; the Esc menu picks one
	struct FRunDest { FString Name, Note; double X, Z; };
	void RunDests(TArray<FRunDest>& Out) const;
	int32 RunSel = 0;
	bool bGearOpen = false;   // the gear screen (Tab; web ui/gear.ts)
	double LineBreakNow() const { return LineBreakAt(); }
	void RunTo(int32 I);
	bool HasGear(const TCHAR* Effect) const { return Owned.Contains(Effect); }
	void ShopBuy(int32 Index);
	void ShopClose();
	double UnderwaterHaze = 1; // Settings: how bright the underwater haze is

	// ---- trolling (=): outriggers out, the throttle held at seven knots, the spread skipping in the wake (KeysTroll.cpp)
	bool IsTrolling() const { return bTrolling; }
	double RigOut() const { return RigK; }                 // 0 stowed .. 1 out, eased
	double TrollQualityNow() const { return TrollQ; }      // 0..1, how well the lures are swimming
	double TrollSoak() const { return TSoak; }
	bool TrollResuming() const { return TResume > 0; }
	// buffs: what is live (label, seconds left, a drink?), and what they add up to
	struct FBuffShow { FString Label; int32 Left; bool bDrink; };
	void LiveBuffs(TArray<FBuffShow>& Out) const;
	double SteadyMul() const;   // >1 steadier hands: a longer strike window
	double Sway() const;        // 0..1 how tipsy: a shorter strike window, the view wobbles, no diving past 0.6
	bool OwnsBoat(const FString& Id) const;   // the five original boats are always yours (Price 0); the new three are bought

	// ---- the last pass (KeysTags.cpp)
	// tag-and-release (web shared/sim/tagging.ts, game/catch/tags.ts, catch-flow.ts): T on the card tags an untagged rod
	// catch and lets it go; a fish you hook may be one somebody tagged, at its grown weight, with its history on the card
	TArray<FKeysTagged> Tags;
	int32 CurTag = -1;                 // the tagged fish on the card, or -1
	bool bCardSpeared = false;         // the card is a speared fish (it can't be tagged)
	FString CardTagHead;
	TArray<FString> CardTagLines;
	void TagAndRelease();
	// the run's biggest fish (catch-flow.ts session.biggest), strung up on the dock when you step ashore
	struct FRunBest { FString Key; double Weight = 0; int32 Id = -1; bool bReleased = false; FString Boat; };
	FRunBest RunBest;
	// the town's services (web shared/sim/services.ts, game/services): the cabaret's doorstep, the motel's night,
	// the scratch-off, the wash and the lost-and-found socks
	TMap<FString, int64> Svc;
	int64 SvcOf(const TCHAR* K) const { const int64* V = Svc.Find(K); return V ? *V : 0; }
	struct FSvcCard { FString Kind, Title; TArray<FString> Lines; TArray<FString> Cells; int32 Prize = 0; double Age = 0, Life = 9; bool bOn = false; };
	FSvcCard SvcCard;
	double SleepT = -1;                // the motel's fade, seconds since it started (-1: none)
	double ReboardT = -1;              // J from far off while diving: the fade back aboard, seconds since it started (-1: none)
	double ReboardAlpha() const { return ReboardT < 0 ? 0 : ReboardT < .4 ? ReboardT / .4 : FMath::Max(0.0, 1 - (ReboardT - .4) / .4); }
	bool bSleepBlack = false;
	double SleepAlpha() const;
	bool SleepCard() const;
	TArray<FString> WrecksFound;       // wreck ids passed over within 260 m (the chart's memory)
	void DebugFinal(const FString& Cmd);
	// fishing from a pier, a dock or a marina's planks (web game/fishing/shore.ts): the rod's state machine runs with the
	// angler standing in for the boat (shoreBoat), so every rule downstream works unchanged
	bool bShoreCanFish = false;        // the walker stands somewhere fishable
	bool ShoreFishable() const;

private:
	void Rebuild();
	void PlaceAtSpawn();
	void HandleInput(float Dt);
	void UpdateBoat(double Dt, double T);
	bool AutoChase(double TopMs, bool bCanDrive, double& OutThr, double& OutSteer);
	void Slam(double VY);
	void UpdateCamera(double Dt, double T);
	void CycleView();
	void SyncChaseCam();
	FVector FishSpotLocal() const;
	void UpdateViewModel(double Dt);
	FVector BuildViewModel(FKeysMesh& M);
	void ShowToast(const FString& Msg, double Secs = 3.2);

	// fishing
	void OnAction();
	void StartCharge();
	void CastLine(double Power);
	void ReelIn(const FString& Msg);
	FString PickSpecies() const;
	void ChooseFish();
	void BuildHookedMesh();
	void MissFish(const FString& Msg);
	void SetHook();
	void StartJump(bool bLow);
	void UpdateJump(double Dt);
	void Lose(const FString& Msg);
	void Land();
	void KeepFish();
	void ReleaseFish();
	void FinishCatch();
	void PickRecapture();
	void SetCardTag();
	void ReportTag(bool bKept);
	bool ServiceRefuse(const FString& Eff, FString& Why) const;
	bool ServiceResolve(const FString& Eff, FString& Msg);
	void UpdateFinal(double Dt, double T);
	bool bForceRecap = false;
	bool bTankLive = false, bFillCharged = false;
	bool bShoreSwap = false;
	double SwX = 0, SwZ = 0, SwH = 0, SwY = 0;
	bool BeginShoreSwap();
	void EndShoreSwap();
	void WeighIn();
	void UpdateFishing(double Dt, double T);
	void DrawLineAndRod(double T);
	void ShowFishMesh(bool bShow);
	FVector RodTip(const FVector& Target) const;
	AKeysWorld* World();

	UPROPERTY() TObjectPtr<USceneComponent> Root;
	UPROPERTY() TObjectPtr<UProceduralMeshComponent> BoatMesh;
	UPROPERTY() TObjectPtr<UProceduralMeshComponent> LineMesh;
	UPROPERTY() TObjectPtr<UProceduralMeshComponent> FishMesh;
	UPROPERTY() TObjectPtr<UProceduralMeshComponent> RiggerMesh;   // the tower boats' swinging outriggers
	UPROPERTY() TObjectPtr<UProceduralMeshComponent> TrollMesh;    // the trolling spread: lines, lures, teasers
	UPROPERTY() TObjectPtr<UStaticMeshComponent> Bobber;
	UPROPERTY() TObjectPtr<UCameraComponent> Camera;
	UPROPERTY() TWeakObjectPtr<AKeysWorld> WorldActor;
	UPROPERTY() TObjectPtr<UKeysSave> Save;

	int32 BoatIndex = 1;

	// boat state, game meters, y up (see KeysGeo.h)
	double X = 0, Z = 0, H = 0, Y = 0, VY = 0, Pitch = 0, Roll = 0, VP = 0, VR = 0, DVX = 0, DVZ = 0;
	double Speed = 0, Thr = 0, Steer = 0, WakeT = 0, TrailT = 0, RideNow = 0, DkNow = 0;
	bool bOnlineTried = false;
	bool bCursorOn = false;
	EMenu MenuBack = EMenu::Main;   // where Back goes from Settings / Controls
	void SubmitLogin();
	void ToggleOnline();
	void StartRun(bool bDemo);
	void UpdateCursor();
	void RunTestScript(double T);
	void ToggleDive();
	void HandleDiveInput(class APlayerController* PC, float Dt);
	void ToggleAshore();
	void HandleWalkInput(class APlayerController* PC, float Dt);
	void WalkInteract();
	// trolling (KeysTroll.cpp; web game/fishing/trolling.ts, troll-spread.ts, troll-raise.ts, shared sim/troll.ts)
	void ToggleTroll();
	void StopTroll(bool bQuiet);
	void UpdateTroll(double Dt, double T);
	void UpdateSpread(double Dt, double T);
	bool TrollThrottle(double TopMs, double& OutThr) const;
	FString TrollSpecies(double Q) const;
	void TrollRaise(double Q);
	void TrollCommit();
	bool LurePos(int32 Station, FVector& Out) const;
	FString TrollSideName(int32 Station) const;
	void PushTrack();
	bool TrackBack(double Back, double& OX, double& OZ, double& OH) const;
	bool bTrolling = false;
	double RigK = 0, RigBuilt = -1, TrollQ = 0;
	double TUntil = 48, TSoak = 0, TSince = 22, TResume = 0;   // sim/troll.ts TrollState, plus the resume pause
	bool bTrollBusy = false;
	FString PendKey;                 // a fish on its way up to a lure (the raise)
	double PendWeight = 0, RaiseLeft = 0, RaiseCamT = 0;
	int32 PendStation = -1, HookedStation = -1;
	FVector RaisePos = FVector::ZeroVector;
	static constexpr int32 NumStations = 5;
	FVector Lures[NumStations];      // game meters, as of the last spread update
	double Pulled[NumStations] = {0, 0, 0, 0, 0};
	bool bSpreadOut = false;
	TArray<FVector> Trk;             // the boat's track by distance: x, z, heading
	int32 TrkHead = -1, TrkCount = 0;
	// boat purchase (sim/economy.ts): Enter on the Boat row buys the boat on show
	void BuyBoat();
	double ScriptThr = -1, ScriptSteer = 0;
	bool bScriptReel = false;   // -KRScript throttle= / steer= holds
	// timed food and drink buffs (KeysBuffs.cpp; web game/buffs.ts, sim/shop.ts modifiersFor, sim/steady.ts)
	TMap<FString, double> Buffs;     // effect key -> game time it wears off
	void UpdateBuffs(double T);
	bool BuffOn(const TCHAR* BuffKey) const;
	double StrikeWindow(double Base) const;
	double LineBreakAt() const;
	int32 BaitCasts = 0;                            // live shrimp left: quicker bites
	double PinX = 0, PinZ = 0, PinH = 0;            // where the boat is tied up while you are ashore
	bool bAir = false;
	double TrimVent = 0, TrimMsgT = -99, CollideMsgT = -99, SlamT = -99, AcMsgT = -99;
	bool bTrimUp = false, bTrimDn = false;
	bool bFwd = false, bBack = false, bLeft = false, bRight = false, bAction = false;

	// camera
	double CamYaw = 0, CamPitchOff = 0, CamZoom = 1, Shake = 0;
	bool bDragging = false;
	FVector ShowPos = FVector::ZeroVector;   // where the catch is held up for the photo, game meters
	double ShowSgn = 1;
	FVector CamPos = FVector::ZeroVector, CamLook = FVector::ZeroVector;   // game meters
	bool bCamInit = false;
	// first-person fishing eye (fp) and helm look offsets
	FVector FpEyeL = FVector::ZeroVector;   // boat-local meters (x forward, y starboard, z up)
	double FpYaw = 0, FpPitch = -.12, FpUserT = 0, DYaw = 0, DPitch = 0;
	// first-person rod (view model)
	double VmRx = -1.15, VmWhip = 0, VmBend = .01, VmCrank = 0;

	// fishing state, mirrors the JS F object
	FVector Bob = FVector::ZeroVector, CastFrom = FVector::ZeroVector, CastTo = FVector::ZeroVector;
	double CastT = 0, CastDur = .7, CastH = 10, ChargeT = 0, BiteT = 0, Win = 0, NibT = 0, DipT = 0;
	int32 Nibs = 0;
	FString Zone, Key;
	FKeysFishSpot Spot;   // bird pile / weedline / rig the bait landed in (JS F.hs)
	const KeysData::FSpecies* Sp = nullptr;
	double Weight = 0, Str = 0, FX = 0, FZ = 0, OFX = 0, OFZ = 0;
	bool bRunningFish = false;
	double RunT = 0, RunAng = 0, Pull = 0, SlackT = 0, OverT = 0, MaxLine = 200;
	int32 JumpQ = 0;
	double Deep = 0, DeepT = 0, ShakeT = 0, SoundMsgT = -99, CaughtT = -99, LastPts = 0;
	bool bSlam1 = false, bSlam2 = false, bDeepTip = false;

	// a fish in the air
	bool bJumping = false, bJumpLow = false;
	double JT = 0, JDur = 1, JX = 0, JZ = 0, JDX = 0, JDZ = 0, JRun = 0, JH = 0, JSpin = 0;
	// jump cinematics: the camera zooms in and swings toward a fish in the air
	float JumpFov = 88.f;
	double JumpLook = 0;
	void UpdateJumpCam(double Dt);
	double FishLen = 1;
	void UpdateHelm(double Dt, double T);
	UPROPERTY() TObjectPtr<UProceduralMeshComponent> HelmScreens;
	UPROPERTY() TObjectPtr<UProceduralMeshComponent> BoatLights;
	UPROPERTY() TArray<TObjectPtr<class UPointLightComponent>> UwLights;
	UPROPERTY() TObjectPtr<class UMaterialInstanceDynamic> ScreenMID;
	UPROPERTY() TObjectPtr<class UMaterialInstanceDynamic> LightMID;
	double HelmT = 0;
	TArray<float> SonarHist;
};
