#include "KeysBoatPawn.h"
#include "KeysGeo.h"
#include "KeysMesh.h"
#include "KeysWorld.h"
#include "KeysSave.h"
#include "KeysTraffic.h"
#include "KeysExtras.h"
#include "KeysAudio.h"
#include "KeysOnline.h"
#include "KeysDiver.h"
#include "KeysWalker.h"
#include "KeysVisitors.h"
#include "KeysLocals.h"
#include "KeysHandgun.h"
#include "KeysTreasure.h"
#include "KeysEstates.h"
#include "KeysTourneyShow.h"
#include "KeysSlots.h"
#include "KeysFinal.h"
#include "KeysTown.h"
#include "KeysTownData.h"
#include "KeysEvents.h"
#include "ProceduralMeshComponent.h"
#include "Camera/CameraComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/PlayerController.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"
#include "EngineUtils.h"
#include "Kismet/GameplayStatics.h"
#include "InputCoreTypes.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "UnrealClient.h"
#include "Misc/Paths.h"

using namespace KeysGeo;
using namespace KeysData;

namespace
{
	constexpr double SpeedScale = 1.35;
	double Clamp(double V, double A, double B) { return V < A ? A : V > B ? B : V; }
	double Lerp(double A, double B, double T) { return A + (B - A) * T; }
	double Rand(double A, double B) { return FMath::FRandRange(A, B); }
	double Hyp(double A, double B) { return FMath::Sqrt(A * A + B * B); }
	double WrapPi(double A) { return FMath::Fmod(FMath::Fmod(A + PI, 2 * PI) + 2 * PI, 2 * PI) - PI; }
	double PawnAngLerp(double A, double B, double K) { return A + WrapPi(B - A) * FMath::Min(1.0, K); }
	FString Commas(int64 N) { return FText::AsNumber(N).ToString(); }
}

AKeysBoatPawn::AKeysBoatPawn()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bTickEvenWhenPaused = true;   // so the pause menu can still be used (and closed)

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent = Root;

	BoatMesh = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("Boat"));
	BoatMesh->SetupAttachment(Root);
	BoatMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	RiggerMesh = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("Riggers"));
	RiggerMesh->SetupAttachment(Root);
	RiggerMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	TrollMesh = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("Spread"));
	TrollMesh->SetupAttachment(Root);
	TrollMesh->SetUsingAbsoluteLocation(true);
	TrollMesh->SetUsingAbsoluteRotation(true);
	TrollMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	TrollMesh->SetCastShadow(false);

	LineMesh = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("Line"));
	LineMesh->SetupAttachment(Root);
	LineMesh->SetUsingAbsoluteLocation(true);
	LineMesh->SetUsingAbsoluteRotation(true);
	LineMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	LineMesh->SetCastShadow(false);

	FishMesh = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("Fish"));
	FishMesh->SetupAttachment(Root);
	FishMesh->SetUsingAbsoluteLocation(true);
	FishMesh->SetUsingAbsoluteRotation(true);
	FishMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereMesh(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	Bobber = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Bobber"));
	Bobber->SetupAttachment(Root);
	Bobber->SetUsingAbsoluteLocation(true);
	Bobber->SetUsingAbsoluteRotation(true);
	Bobber->SetUsingAbsoluteScale(true);
	Bobber->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	if (SphereMesh.Succeeded()) Bobber->SetStaticMesh(SphereMesh.Object);
	Bobber->SetWorldScale3D(FVector(.22));

	Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
	Camera->SetupAttachment(Root);
	Camera->SetUsingAbsoluteLocation(true);
	Camera->SetUsingAbsoluteRotation(true);
	Camera->bUsePawnControlRotation = false;
	Camera->SetFieldOfView(88.f);
}

void AKeysBoatPawn::BeginPlay()
{
	Super::BeginPlay();
	KeysGeo::Init();
	BoatIndex = PickIndex = 1;
	Rebuild();
	PlaceAtSpawn();
	LineMesh->SetWorldLocationAndRotation(FVector::ZeroVector, FRotator::ZeroRotator);
	TrollMesh->SetWorldLocationAndRotation(FVector::ZeroVector, FRotator::ZeroRotator);
	if (UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(AKeysWorld::SolidMaterial(), this))
	{
		MID->SetVectorParameterValue(TEXT("Color"), FLinearColor(.9f, .05f, .04f));
		MID->SetScalarParameterValue(TEXT("Roughness"), .4f);
		Bobber->SetMaterial(0, MID);
	}
	Bobber->SetVisibility(false);
	FishMesh->SetVisibility(false);
	Save = UKeysSave::LoadOrCreate();
	if (Save)
	{
		Cash = Save->Cash;
		for (const FString& O : Save->Owned) Owned.Add(O);
		Estates = Save->Estates;
		Counts = Save->Counts;
		Wearing = Save->Wearing;
		TreasureOpened = Save->TreasureOpened;
		EstateBuilds = Save->EstateBuilds;
		Spins = Save->Spins;
		Tags = Save->Tags;
		Svc = Save->Services;
		WrecksFound = Save->WrecksFound;
		// every boat in the Keys has a pistol in the console: the starter kit is two magazines' worth and three shafts (sim/shop.ts)
		if (!Save->bStarterKit) { Counts.FindOrAdd(TEXT("handgun.ammo")) += 24; Counts.FindOrAdd(TEXT("spear.shafts")) += 3; Save->bStarterKit = true; }
	}
	else { Counts.Add(TEXT("handgun.ammo"), 24); Counts.Add(TEXT("spear.shafts"), 3); }
	if (AKeysTown* Tn = AKeysTown::Get(this)) Tn->SetOwnedEstates(Estates);
	ApplyOutfit();
	if (APlayerController* PC = Cast<APlayerController>(GetController()))
	{
		PC->SetInputMode(FInputModeGameOnly());
		PC->bShowMouseCursor = false;
	}
}

UKeysSave* AKeysBoatPawn::Board() const { return Save; }

AKeysWorld* AKeysBoatPawn::World()
{
	if (!WorldActor.IsValid())
	{
		TActorIterator<AKeysWorld> It(GetWorld());
		if (It) WorldActor = *It;
	}
	return WorldActor.Get();
}

void AKeysBoatPawn::Rebuild()
{
	FKeysMesh M;
	KeysMesh::BuildBoat(M, Spec(), false);
	M.Commit(BoatMesh, 0, AKeysWorld::VertexColorMaterial());
	RigBuilt = -1;   // the outriggers are their own mesh, so they can swing (KeysTroll.cpp)
}

void AKeysBoatPawn::PlaceAtSpawn()
{
	// open water in Boot Key Harbor, clear of the marinas
	X = -1150;
	Z = ChainZ(-1150) + 305;
	H = PI / 2;
	Y = VY = Pitch = Roll = VP = VR = DVX = DVZ = Speed = Thr = Steer = 0;
	bAir = false;
	bCamInit = false;
	double NX = X, NZ = Z;
	if (AKeysTraffic::FindClearSpot(NX, NZ, Spec())) { X = NX; Z = NZ; }
}

void AKeysBoatPawn::ShowToast(const FString& Msg, double Secs)
{
	Toast = Msg;
	ToastUntil = GetWorld()->GetTimeSeconds() + Secs;
}

double AKeysBoatPawn::KnotsNow() const { return FMath::Abs(Speed) / SpeedScale / .5144; }
double AKeysBoatPawn::DepthHere() const { return DepthAt(X, Z); }
FString AKeysBoatPawn::NearName() const { return NearestName(X, Z); }
double AKeysBoatPawn::CoolerLb() const { double S = 0; for (const FKeysCooler& C : Cooler) S += C.Weight; return S; }

FString AKeysBoatPawn::ZoneName() const
{
	if (const AKeysLife* L = AKeysLife::Get(this))
	{
		const FString Area = L->AreaLabel(X, Z);
		if (!Area.IsEmpty()) return Area;
	}
	const FString Zn = ZoneAt(X, Z);
	if (Zn == TEXT("Offshore") && OffshoreF(X, Z) > .5) return TEXT("Deep blue water");
	if (InMiami(X, Z) && Zn != TEXT("Creek")) return TEXT("Biscayne Bay");
	return ZoneDesc(Zn);
}

// ================================================================ menus
void AKeysBoatPawn::GetMenuItems(TArray<FMenuItem>& Out) const
{
	Out.Reset();
	const AKeysOnline* O = AKeysOnline::Get(this);
	const bool bOnline = O && O->NetState() != EKeysNet::Offline;
	auto Add = [&Out](const TCHAR* Id, const FString& Label, const FString& Value = FString(), bool bArrows = false) { Out.Add({FName(Id), Label, Value, bArrows}); };
	const FString OnlineLabel = bOnline ? FString(TEXT("Log out")) : FString(TEXT("Play online"));
	const FString OnlineValue = bOnline ? O->StatusText() : FString(TEXT("fish with friends"));
	switch (Menu)
	{
	case EMenu::Main:
		Add(TEXT("play"), TEXT("Start fishing"));
		Add(TEXT("boat"), TEXT("Boat"), Spec().Brand + TEXT(" ") + Spec().Name + (OwnsBoat(Spec().Id) ? FString() : FString::Printf(TEXT("  ($%s)"), *Commas(Spec().Price))), true);
		Add(TEXT("online"), OnlineLabel, OnlineValue);
		Add(TEXT("board"), TEXT("Leaderboard"));
		Add(TEXT("settings"), TEXT("Settings"));
		Add(TEXT("controls"), TEXT("Controls"));
		Add(TEXT("quit"), TEXT("Quit"));
		break;
	case EMenu::Pause:
	{
		Add(TEXT("resume"), TEXT("Resume"));
		const AKeysEvents* Ev = AKeysEvents::Get(this);
		const bool bTour = Ev && Ev->InTourney() && Ev->TourneyPhase() != AKeysEvents::ETourney::Over;
		const bool bRace = Ev && (Ev->RacePhase() == AKeysEvents::ERace::Countdown || Ev->RacePhase() == AKeysEvents::ERace::Racing);
		Add(TEXT("tourney"), TEXT("Fishing tournament"), bTour ? FString(TEXT("quit it")) : FString(TEXT("15 min, five species")));
		Add(TEXT("race"), TEXT("Marathon race"), bRace ? FString(TEXT("abandon it")) : FString(TEXT("2 laps, R at the helm")));
		Add(TEXT("estates"), TEXT("Land office"), TEXT("Keys Run Estates  ( , )"));
		{
			TArray<FRunDest> Ds;
			RunDests(Ds);
			if (Ds.Num())
			{
				const FRunDest& D = Ds[FMath::Clamp(RunSel, 0, Ds.Num() - 1)];
				const double Km = FMath::Sqrt(FMath::Square(D.X - X) + FMath::Square(D.Z - Z)) / 1000;
				Add(TEXT("runto"), TEXT("Run to"), FString::Printf(TEXT("%s - %s, %s"), *D.Name, *D.Note, Km < 1 ? *FString::Printf(TEXT("%d m"), FMath::RoundToInt(Km * 1000)) : *FString::Printf(TEXT("%.1f km"), Km)), true);
			}
		}
		Add(TEXT("online"), OnlineLabel, OnlineValue);
		Add(TEXT("board"), TEXT("Leaderboard"));
		Add(TEXT("settings"), TEXT("Settings"));
		Add(TEXT("controls"), TEXT("Controls"));
		Add(TEXT("mainmenu"), TEXT("Main menu"), TEXT("change boats"));
		Add(TEXT("quit"), TEXT("Quit"));
		break;
	}
	case EMenu::Settings:
	{
		const AKeysAudio* A = AKeysAudio::Get(this);
		AKeysWorld* W = const_cast<AKeysBoatPawn*>(this)->World();
		Add(TEXT("sea"), TEXT("Sea state"), SeaStates()[SeaIndex()].Name, true);
		Add(TEXT("time"), TEXT("Time of day"), W && W->IsSunset() ? TEXT("Sunset") : TEXT("Day"), true);
		Add(TEXT("sound"), TEXT("Sound"), A && A->IsSoundOn() ? TEXT("On") : TEXT("Off"), true);
		Add(TEXT("radio"), TEXT("Radio"), A ? A->GetRadioLabel().Replace(TEXT("Radio: "), TEXT("")) : FString(TEXT("Off")), true);
		Add(TEXT("view"), TEXT("Camera"), View == 0 ? TEXT("Chase") : View == 1 ? TEXT("Helm") : TEXT("Tuna tower"), true);
		Add(TEXT("fpview"), TEXT("Fishing view"), bFpPref ? TEXT("First person") : TEXT("Third person"), true);
		Add(TEXT("autochase"), TEXT("Auto-chase hooked fish"), bAutoChase ? TEXT("On") : TEXT("Off"), true);
		Add(TEXT("diveview"), TEXT("Dive view"), bDiveChase ? TEXT("Chase") : TEXT("Dive mask"), true);
		Add(TEXT("haze"), TEXT("Underwater haze"), UnderwaterHaze < .8 ? TEXT("Clear") : UnderwaterHaze > 1.2 ? TEXT("Murky") : TEXT("Normal"), true);
		Add(TEXT("back"), TEXT("Back"));
		break;
	}
	case EMenu::Controls:
		Add(TEXT("back"), TEXT("Back"));
		break;
	default:
		break;
	}
}

void AKeysBoatPawn::MenuActivate(FName Id, int32 Dir)
{
	const int32 Step = Dir < 0 ? -1 : 1;
	if (Id == TEXT("play"))
	{
		if (OwnsBoat(Spec().Id)) StartRun(false);
		else ShowToast(FString::Printf(TEXT("The %s %s isn't yours yet - Enter on the Boat row buys it for $%s (you have $%s), or pick a boat you own."),
			*Spec().Brand, *Spec().Name, *Commas(Spec().Price), *Commas(Cash)), 5);
	}
	else if (Id == TEXT("resume")) Menu = EMenu::None;
	else if (Id == TEXT("runto"))
	{
		TArray<FRunDest> Ds;
		RunDests(Ds);
		if (Dir != 0 && Ds.Num()) RunSel = (RunSel + Dir + Ds.Num()) % Ds.Num();
		else if (Dir == 0) RunTo(RunSel);
	}
	else if (Id == TEXT("estates")) { Menu = EMenu::None; if (AKeysEstates* Es = AKeysEstates::Get(this)) Es->Open(true); }
	else if (Id == TEXT("boat") && Dir == 0) BuyBoat();
	else if (Id == TEXT("boat"))
	{
		const int32 NB = Boats().Num();
		PickIndex = (PickIndex + Step + NB) % NB;
		if (PickIndex != BoatIndex) { BoatIndex = PickIndex; Rebuild(); }
		FVector Eye;
		if (View == 2 && !KeysMesh::TowerEye(Spec(), Eye)) View = 1;   // this boat has no tower
	}
	else if (Id == TEXT("online")) ToggleOnline();
	else if (Id == TEXT("tourney") || Id == TEXT("race"))
	{
		if (AKeysEvents* Ev = AKeysEvents::Get(this))
		{
			Menu = EMenu::None;
			if (Id == TEXT("race")) { StopTroll(true); Ev->ToggleRace(); }
			else Ev->ToggleTourney();
		}
	}
	else if (Id == TEXT("board")) { bBoardOpen = true; BoardTab = 0; }
	else if (Id == TEXT("settings")) { MenuBack = Menu; Menu = EMenu::Settings; MenuSel = 0; }
	else if (Id == TEXT("controls")) { MenuBack = Menu; Menu = EMenu::Controls; MenuSel = 0; }
	else if (Id == TEXT("back")) { Menu = MenuBack; MenuSel = 0; }
	else if (Id == TEXT("mainmenu"))
	{
		if (FState != EKeysFish::Idle) ReelIn(FString());
		StopTroll(true);
		Speed = Thr = 0;
		bRunning = false;
		Menu = EMenu::Main;
		MenuSel = 0;
		PickIndex = BoatIndex;
	}
	else if (Id == TEXT("quit"))
	{
		if (APlayerController* PC = Cast<APlayerController>(GetController())) PC->ConsoleCommand(TEXT("quit"));
	}
	else if (Id == TEXT("sea")) SetSeaState((SeaIndex() + Step + SeaStates().Num()) % SeaStates().Num());
	else if (Id == TEXT("time")) { if (AKeysWorld* W = World()) W->ToggleSunset(); }
	else if (Id == TEXT("sound")) { if (AKeysAudio* A = AKeysAudio::Get(this)) A->ToggleSound(); }
	else if (Id == TEXT("radio")) { if (AKeysAudio* A = AKeysAudio::Get(this)) A->MusicToggle(); }
	else if (Id == TEXT("view")) CycleView();
	else if (Id == TEXT("fpview")) bFpPref = !bFpPref;
	else if (Id == TEXT("autochase")) bAutoChase = !bAutoChase;
	else if (Id == TEXT("diveview"))
	{
		bDiveChase = !bDiveChase;
		if (AKeysDiver* D = AKeysDiver::Get(this)) { D->SetDefaultChase(bDiveChase); if (D->IsDiving() && D->IsChaseView() != bDiveChase) D->CycleView(); }
	}
	else if (Id == TEXT("haze"))
	{
		// Clear 0.6, Normal 1, Murky 1.5
		const double Steps[3] = {.6, 1, 1.5};
		int32 I = UnderwaterHaze < .8 ? 0 : UnderwaterHaze > 1.2 ? 2 : 1;
		I = (I + (Dir < 0 ? 2 : 1)) % 3;
		UnderwaterHaze = Steps[I];
		if (AKeysDiver* D = AKeysDiver::Get(this)) D->UnderwaterHaze = UnderwaterHaze;
	}
}

// the boat broker (web sim/economy.ts stepBuy): buying also selects the hull
bool AKeysBoatPawn::OwnsBoat(const FString& Id) const
{
	if (Id == KeysData::StarterBoat() || Owned.Contains(TEXT("boat.") + Id)) return true;
	for (const FBoatSpec& B : KeysData::Boats()) if (B.Id == Id) return B.Price <= 0;
	return false;
}

void AKeysBoatPawn::BuyBoat()
{
	const FBoatSpec& S = Spec();
	if (OwnsBoat(S.Id)) { ShowToast(FString::Printf(TEXT("The %s %s is yours. Start fishing!"), *S.Brand, *S.Name)); return; }
	if (Cash < S.Price)
	{
		ShowToast(FString::Printf(TEXT("The %s %s is $%s and you have $%s. Sell fillets at a weigh-in (E at a marina dock) - $%s to go."),
			*S.Brand, *S.Name, *Commas(S.Price), *Commas(Cash), *Commas(S.Price - Cash)), 5);
		return;
	}
	Cash -= S.Price;
	Owned.Add(TEXT("boat.") + S.Id);
	SaveEconomy();
	ShowToast(FString::Printf(TEXT("SOLD! The %s %s is yours - fuelled up in Boot Key Harbor. $%s left."), *S.Brand, *S.Name, *Commas(Cash)), 5);
}

void AKeysBoatPawn::OnHudClick(FName Box)
{
	const FString B = Box.ToString();
	FString Id;
	if (B.Split(TEXT(":"), nullptr, &Id))
	{
		if (B.StartsWith(TEXT("m:"))) MenuActivate(FName(*Id));
		else if (B.StartsWith(TEXT("m<:"))) MenuActivate(FName(*Id), -1);
		else if (B.StartsWith(TEXT("m>:"))) MenuActivate(FName(*Id), 1);
		else if (B.StartsWith(TEXT("tab:"))) BoardTab = FCString::Atoi(*Id);
		else if (B == TEXT("board:close")) bBoardOpen = false;
		else if (B == TEXT("login:name")) LoginField = 0;
		else if (B == TEXT("login:crew")) LoginField = 1;
		else if (B == TEXT("login:go")) { LoginField = 1; SubmitLogin(); }
		else if (B == TEXT("login:cancel")) bLoginOpen = false;
	}
}

void AKeysBoatPawn::OnHudHover(FName Box)
{
	// hovering a menu row selects it, so the mouse and the keyboard always agree
	FString Id;
	if (!Box.ToString().Split(TEXT(":"), nullptr, &Id)) return;
	TArray<FMenuItem> Items;
	GetMenuItems(Items);
	for (int32 i = 0; i < Items.Num(); i++) if (Items[i].Id == FName(*Id)) MenuSel = i;
}

void AKeysBoatPawn::SubmitLogin()
{
	LoginName.TrimStartAndEndInline();
	LoginCrew.TrimStartAndEndInline();
	if (LoginName.IsEmpty()) { LoginField = 0; return; }
	if (LoginField == 0 || LoginCrew.IsEmpty()) { LoginField = 1; return; }
	if (AKeysOnline* O = AKeysOnline::Get(this))
	{
		O->Connect(LoginName, LoginCrew);
		if (Save) { Save->OnlineName = LoginName; Save->OnlineCrew = LoginCrew; Save->bOnlineAuto = true; Save->Write(); }
		bLoginOpen = false;
	}
}

void AKeysBoatPawn::ToggleOnline()
{
	AKeysOnline* O = AKeysOnline::Get(this);
	if (!O) return;
	if (O->NetState() != EKeysNet::Offline)
	{
		O->Disconnect();
		if (Save) { Save->bOnlineAuto = false; Save->Write(); }
		ShowToast(TEXT("Logged out - playing solo."));
		return;
	}
	bLoginOpen = true;
	LoginField = 0;
	if (Save && LoginName.IsEmpty()) { LoginName = Save->OnlineName; LoginCrew = Save->OnlineCrew; }
}

void AKeysBoatPawn::StartRun(bool bDemo)
{
	PlaceAtSpawn();
	if (bDemo)
	{
		// demo: start in the nearest stretch of open water at least 400 m across, so it can run without beaching
		double Best = 1e18;
		for (double GX = -4000; GX <= 4000; GX += 100)
			for (double GZ = -4000; GZ <= 4000; GZ += 100)
			{
				const double CX = X + GX, CZ = Z + GZ, Dist = GX * GX + GZ * GZ;
				if (Dist >= Best || DepthFast(CX, CZ) < 12) continue;
				bool bOpen = true;
				for (int32 k = 0; k < 24 && bOpen; k++)
					for (double R = 100; R <= 400 && bOpen; R += 100)
						bOpen = DepthFast(CX + FMath::Cos(k * PI / 12) * R * 1.5, CZ + FMath::Sin(k * PI / 12) * R * 1.5) > 6;
				if (bOpen) { Best = Dist; X = CX; Z = CZ; }
			}
	}
	bRunning = true;
	Menu = EMenu::None;
	ShowToast(FString::Printf(TEXT("Welcome to Marathon. The %s %s is fueled up in Boot Key Harbor. Esc for the menu."), *Spec().Brand, *Spec().Name), 5);
}

// -KRScript="8:shot;9:settings;11:shot;14:quit" runs menu actions and screenshots on a timer (seconds of game time),
// so menus and models can be checked in an offscreen run without a keyboard. Words are menu ids (play, settings, back,
// board, esc, ...), "shot" saves a screenshot, "sel=N" selects row N, "quit" exits.
void AKeysBoatPawn::RunTestScript(double T)
{
	static TArray<TPair<double, FString>> Steps;
	static bool bParsed = false;
	if (!bParsed)
	{
		bParsed = true;
		FString Script;
		if (FParse::Value(FCommandLine::Get(), TEXT("KRScript="), Script, false))
		{
			TArray<FString> Parts;
			Script.ParseIntoArray(Parts, TEXT(";"));
			Save = nullptr;   // test runs never write your leaderboard
			for (const FString& Pt : Parts)
			{
				FString When, What;
				if (Pt.Split(TEXT(":"), &When, &What)) Steps.Add(TPair<double, FString>(FCString::Atod(*When), What.TrimStartAndEnd()));
			}
		}
	}
	while (Steps.Num() && T >= Steps[0].Key)
	{
		const FString Cmd = Steps[0].Value;
		Steps.RemoveAt(0);
		if (Cmd == TEXT("shot"))
		{
			// with the HUD, into Saved/Screenshots/KRScript_N.png
			static int32 Shot = 0;
			FScreenshotRequest::RequestScreenshot(FPaths::ScreenShotDir() / FString::Printf(TEXT("KRScript_%02d.png"), Shot++), true, false);
		}
		else if (Cmd == TEXT("esc")) { Menu = EMenu::Pause; MenuSel = 0; }
		else if (Cmd.StartsWith(TEXT("sel="))) MenuSel = FCString::Atoi(*Cmd.Mid(4));
		else if (Cmd == TEXT("login")) { bLoginOpen = true; LoginField = 0; LoginName = TEXT("Mat"); LoginCrew = TEXT("keys"); }
		else if (Cmd == TEXT("closeall")) { bLoginOpen = bBoardOpen = false; }
		else if (Cmd == TEXT("cast")) CastLine(.55);
		else if (Cmd.StartsWith(TEXT("hook=")))
		{
			// a fish of that species takes the bait right now: straight into the fight
			Key = Cmd.Mid(5);
			Sp = Species(Key);
			if (Sp)
			{
				Weight = FMath::RoundToDouble((Sp->Min + Sp->Max) * .5 * 10) / 10;
				const double A = FMath::FRand() * 2 * PI;
				FX = Bob.X + FMath::Cos(A) * 11; FZ = Bob.Z + FMath::Sin(A) * 11;
				OFX = FMath::Cos(A) * 1.3; OFZ = FMath::Sin(A) * 1.3;
				BuildHookedMesh();
				SetHook();
			}
		}
		else if (Cmd.StartsWith(TEXT("tp=")))
		{
			// tp=X,Z[,heading]: put the boat there (game meters)
			TArray<FString> P;
			Cmd.Mid(3).ParseIntoArray(P, TEXT(","));
			if (P.Num() >= 2)
			{
				X = FCString::Atod(*P[0]); Z = FCString::Atod(*P[1]);
				if (P.Num() >= 3) H = FCString::Atod(*P[2]);
				Speed = Thr = 0; bCamInit = false;
			}
		}
		else if (Cmd.StartsWith(TEXT("goto=")))
		{
			// goto=flats|bay|hawk|reef|wall|offshore: the nearest open water of that kind, searched in a spiral
			const FString W = Cmd.Mid(5);
			double DMin = 1, DMax = 2.2;
			FString Zn = TEXT("Flats");
			if (W == TEXT("bay")) { Zn = TEXT("Backcountry"); DMin = 2.6; DMax = 4; }
			else if (W == TEXT("hawk")) { Zn = TEXT("Hawk Channel"); DMin = 6; DMax = 9; }
			else if (W == TEXT("reef")) { Zn = TEXT("Reef"); DMin = 4; DMax = 7; }
			else if (W == TEXT("wall")) { Zn = TEXT("Reef"); DMin = 12; DMax = 20; }
			else if (W == TEXT("offshore")) { Zn = TEXT("Offshore"); DMin = 100; DMax = 400; }
			bool bFound = false;
			for (double R = 0; R < 6000 && !bFound; R += 25)
				for (int32 k = 0; k < FMath::Max(1, int32(R / 12)) && !bFound; k++)
				{
					const double A = k * 2 * PI / FMath::Max(1, int32(R / 12));
					const double PX = X + FMath::Cos(A) * R, PZ = Z + FMath::Sin(A) * R;
					const double D = DepthAt(PX, PZ);
					if (D < DMin || D > DMax || ShoreInfo(PX, PZ).D < 40 || ZoneAt(PX, PZ) != Zn) continue;
					X = PX; Z = PZ; bFound = true;
				}
			Speed = Thr = 0; bCamInit = false;
			UE_LOG(LogTemp, Log, TEXT("KRScript goto %s -> %.0f, %.0f depth %.1f found %d"), *W, X, Z, DepthAt(X, Z), bFound ? 1 : 0);
		}
		else if (Cmd.StartsWith(TEXT("cam=")))
		{
			// cam=yaw,pitchoff,zoom for the chase camera
			TArray<FString> P;
			Cmd.Mid(4).ParseIntoArray(P, TEXT(","));
			if (P.Num() >= 1) CamYaw = FCString::Atod(*P[0]);
			if (P.Num() >= 2) CamPitchOff = FCString::Atod(*P[1]);
			if (P.Num() >= 3) CamZoom = FCString::Atod(*P[2]);
		}
		else if (Cmd == TEXT("dive")) { if (!IsDiving()) ToggleDive(); }
		else if (Cmd == TEXT("tie"))
		{
			// bring the boat alongside the town marina's main pier, stopped, and step ashore
			const FMarina& M = Marinas()[0];
			X = M.SX + 4.8; Z = M.SZ + M.Dir * 30; H = M.Dir > 0 ? PI : 0;
			Speed = Thr = DVX = DVZ = 0; bCamInit = false;
			if (!IsAshore()) ToggleAshore();
		}
		else if (Cmd == TEXT("ashore")) { if (!IsAshore()) ToggleAshore(); }
		else if (Cmd == TEXT("board")) { if (IsAshore()) ToggleAshore(); }
		else if (Cmd.StartsWith(TEXT("walk=")))
		{
			// walk=u,v,yaw: put the walker at a spot in the town's own frame (u along the frontage, v inland; yaw 0 faces +u, 1.57 inland)
			TArray<FString> Pp;
			Cmd.Mid(5).ParseIntoArray(Pp, TEXT(","));
			if (AKeysWalker* Wk = AKeysWalker::Get(this); Wk && Pp.Num() >= 3)
				if (!Wk->DebugPlaceLocal(FCString::Atod(*Pp[0]), FCString::Atod(*Pp[1]), FCString::Atod(*Pp[2])))
					UE_LOG(LogTemp, Warning, TEXT("KRScript walk: nowhere to stand at %s"), *Cmd);
		}
		else if (Cmd.StartsWith(TEXT("wlook=")))
		{
			// wlook=yawoffset,pitch relative to the body's heading
			TArray<FString> Pp;
			Cmd.Mid(6).ParseIntoArray(Pp, TEXT(","));
			if (AKeysWalker* Wk = AKeysWalker::Get(this); Wk && Pp.Num() >= 2) Wk->DebugLook(Wk->BodyYaw() + FCString::Atod(*Pp[0]), FCString::Atod(*Pp[1]));
		}
		else if (Cmd == TEXT("wview")) { if (AKeysWalker* Wk = AKeysWalker::Get(this)) Wk->CycleView(); }
		else if (Cmd == TEXT("use")) WalkInteract();
		else if (Cmd.StartsWith(TEXT("buy="))) ShopBuy(FCString::Atoi(*Cmd.Mid(4)));
		else if (Cmd == TEXT("shopclose")) ShopClose();
		else if (Cmd.StartsWith(TEXT("cash="))) Cash = FCString::Atoi64(*Cmd.Mid(5));
		else if (Cmd == TEXT("sunset")) { if (AKeysWorld* Wd = World()) Wd->ToggleSunset(); }
		else if (Cmd.StartsWith(TEXT("con=")))
		{
			// con=stat unit: any console command
			if (APlayerController* PC = Cast<APlayerController>(GetController())) PC->ConsoleCommand(Cmd.Mid(4));
		}
		else if (Cmd == TEXT("perf:town0") || Cmd == TEXT("perf:town1"))
		{
			// frame-time comparison: hide the town and the landmarks
			for (TActorIterator<AActor> It(GetWorld()); It; ++It)
				if (It->GetClass()->GetName() == TEXT("KeysTown") || It->GetClass()->GetName() == TEXT("KeysLandmarks")) It->SetActorHiddenInGame(Cmd == TEXT("perf:town0"));
		}
		else if (Cmd == TEXT("perf:old") || Cmd == TEXT("perf:new"))
		{
			// frame-time comparison: perf:old puts back the opaque chart-colored sea and hides the floor and the reef
			const bool bOld = Cmd == TEXT("perf:old");
			if (AKeysWorld* W = World()) W->DebugOldWater(bOld);
			for (TActorIterator<AActor> It(GetWorld()); It; ++It)
				if (It->GetClass()->GetName() == TEXT("KeysSeafloor") || It->GetClass()->GetName() == TEXT("KeysReef")) It->SetActorHiddenInGame(bOld);
		}
		else if (Cmd.StartsWith(TEXT("dv:")))
		{
			// dv:depth=6, dv:look=1.2,-0.3, dv:aimfish, dv:spear, dv:reload, dv:air=0.2, dv:swim=1, dv:haul=1, dv:chase, dv:mask
			FString C = Cmd.Mid(3), A;
			Cmd.Mid(3).Split(TEXT("="), &C, &A);
			if (C.IsEmpty()) C = Cmd.Mid(3);
			if (AKeysDiver* Dv = AKeysDiver::Get(this)) Dv->DebugCmd(C, A);
		}
		else if (Cmd == TEXT("jump")) { if (FState == EKeysFish::Fight) StartJump(false); }
		else if (Cmd == TEXT("troll")) { if (!bTrolling) ToggleTroll(); }
		else if (Cmd == TEXT("trollin")) { if (bTrolling) ToggleTroll(); }
		else if (Cmd == TEXT("trollraise")) { if (bTrolling && PendKey.IsEmpty()) TrollRaise(.9); }   // a fish comes up on the spread now
		else if (Cmd == TEXT("trollstrike")) { if (bTrolling && PendKey.IsEmpty()) { TrollRaise(.9); if (!PendKey.IsEmpty()) TrollCommit(); } }
		else if (Cmd.StartsWith(TEXT("buff=")))
		{
			// buff=diner.coffee: as if bought at the counter
			const FString K = Cmd.Mid(5);
			if (KeysBuffs::Seconds(K) > 0) { Buffs.Add(K, GetWorld()->GetTimeSeconds() + KeysBuffs::Seconds(K)); ShowToast(KeysBuffs::StartText(K)); }
		}
		else if (Cmd == TEXT("keep")) KeepFish();
		else if (Cmd == TEXT("harbour")) { X = AKeysEvents::HarbourX(); Z = AKeysEvents::HarbourZ(); Speed = Thr = 0; bCamInit = false; }
		else if (Cmd.StartsWith(TEXT("ev:"))) { if (AKeysEvents* Ev = AKeysEvents::Get(this)) Ev->DebugCmd(Cmd.Mid(3)); }
		else if (Cmd == TEXT("bigmap")) bBigMap = !bBigMap;
		else if (Cmd.StartsWith(TEXT("pirates"))) { if (AKeysVisitors* Vs = AKeysVisitors::Get(this)) Vs->SummonPirates(Cmd.Len() > 8 ? FCString::Atod(*Cmd.Mid(8)) : 300); }   // pirates or pirates=120
		else if (Cmd.StartsWith(TEXT("lc:"))) { if (AKeysLocals* Lc = AKeysLocals::Get(this)) UE_LOG(LogTemp, Log, TEXT("KRScript lc %s"), *Lc->DebugCmd(Cmd.Mid(3))); }
		else if (Cmd.StartsWith(TEXT("give=")))
		{
			// give=safety.flare,3: consumables as if bought; give=handgun.dot: gear
			FString K, N;
			if (!Cmd.Mid(5).Split(TEXT(","), &K, &N)) { K = Cmd.Mid(5); N.Empty(); }
			if (N.IsEmpty()) Owned.Add(K); else AddCount(*K, FCString::Atoi(*N));
		}
		else if (Cmd == TEXT("estates")) { if (AKeysEstates* Es = AKeysEstates::Get(this)) Es->Open(!Es->bOpen); }
		else if (Cmd.StartsWith(TEXT("estsel=")))
		{
			// estsel=<parcel id>: the land office's chart on that lot
			if (AKeysEstates* Es = AKeysEstates::Get(this))
				for (int32 r = 0; r < AKeysEstates::RegionNames().Num(); r++)
				{
					Es->SetRegion(r);
					const int32 I = Es->RegionList.IndexOfByPredicate([&](int32 Ix) { return Es->Parcels()[Ix].Id == Cmd.Mid(7); });
					if (I >= 0 && r > 0) { Es->Sel = I; break; }
				}
		}
		else if (Cmd.StartsWith(TEXT("estbuy="))) { if (AKeysEstates* Es = AKeysEstates::Get(this)) UE_LOG(LogTemp, Log, TEXT("KRScript %s"), *Es->Buy(Cmd.Mid(7))); }
		else if (Cmd.StartsWith(TEXT("estbuild=")))
		{
			// estbuild=<id>|res=stilt;dock=yacht;...
			FString Id, B;
			if (AKeysEstates* Es = AKeysEstates::Get(this); Es && Cmd.Mid(9).Split(TEXT("|"), &Id, &B))
				if (const AKeysEstates::FParcel* Pc = Es->ById(Id)) Es->SetBuild(Id, Es->BuildFromString(*Pc, AKeysEstates::BuildToString(Es->BuildOf(Id)) + TEXT(";") + B));
		}
		else if (Cmd.StartsWith(TEXT("estgo=")))
		{
			// estgo=<id>: the boat off that lot's shore, looking at it
			if (AKeysEstates* Es = AKeysEstates::Get(this))
				if (const AKeysEstates::FParcel* Pc = Es->ById(Cmd.Mid(6)))
				{
					const AKeysEstates::FLayout L = Es->Layout(*Pc, Es->BuildOf(Pc->Id));
					double PX = L.HX + L.SX * 70, PZ = L.HZ + L.SZ * 70;
					for (int32 k = 0; k < 40 && DepthAt(PX, PZ) < 1.2; k++) { PX += L.SX * 8; PZ += L.SZ * 8; }
					X = PX; Z = PZ; H = FMath::Atan2(L.SX, L.SZ); Speed = Thr = 0; bCamInit = false;
				}
		}
		else if (Cmd == TEXT("treasure"))
		{
			// the boat beside the first unopened box on the wrecks, ready to dive on it
			double TX, TZ, TY;
			if (AKeysTreasure* Tr = AKeysTreasure::Get(this); Tr && Tr->DebugNearest(TX, TZ, TY)) { X = TX + 6; Z = TZ + 2; Speed = Thr = 0; bCamInit = false; UE_LOG(LogTemp, Log, TEXT("KRScript treasure at %.0f, %.0f floor %.1f"), TX, TZ, TY); }
		}
		else if (Cmd == TEXT("treasuredive"))
		{
			double TX, TZ, TY;
			if (AKeysTreasure* Tr = AKeysTreasure::Get(this); Tr && Tr->DebugNearest(TX, TZ, TY))
				if (AKeysDiver* Dv = AKeysDiver::Get(this)) { Dv->DebugCmd(TEXT("at"), FString::Printf(TEXT("%f,%f"), TX + 1.7, TZ + .5)); Dv->DebugCmd(TEXT("depth"), TEXT("200")); }
		}
		else if (Cmd == TEXT("open")) { if (AKeysTreasure* Tr = AKeysTreasure::Get(this)) Tr->TryOpen(); }
		else if (Cmd == TEXT("flare")) { if (AKeysHandgun* Gun = AKeysHandgun::Get(this)) Gun->DebugFlare(); }
		else if (Cmd == TEXT("lights")) ToggleLights();
		else if (Cmd.StartsWith(TEXT("wear=")))
		{
			const FString K = Cmd.Mid(5);
			Owned.Add(K);
			Wearing.Add(K.Mid(9).Left(K.Mid(9).Find(TEXT("."))), K);
			ApplyOutfit();
		}
		else if (Cmd == TEXT("gear")) bGearOpen = !bGearOpen;
		else if (Cmd.StartsWith(TEXT("runto="))) RunTo(FCString::Atoi(*Cmd.Mid(6)));
		else if (Cmd == TEXT("slots")) { if (AKeysSlots* Sl = AKeysSlots::Get(this)) Sl->Open(!Sl->bOpen); }
		else if (Cmd == TEXT("spin")) { if (AKeysSlots* Sl = AKeysSlots::Get(this)) Sl->Spin(); }
		else if (Cmd.StartsWith(TEXT("view="))) View = FCString::Atoi(*Cmd.Mid(5));
		else if (Cmd == TEXT("gun")) { if (AKeysHandgun* Gun = AKeysHandgun::Get(this)) Gun->Toggle(); }
		else if (Cmd.StartsWith(TEXT("gunlook=")) || Cmd.StartsWith(TEXT("fire=")))
		{
			// gunlook=yaw,pitch / fire=yaw,pitch (radians, yaw relative to the bow)
			TArray<FString> Pp;
			Cmd.Mid(Cmd.Find(TEXT("=")) + 1).ParseIntoArray(Pp, TEXT(","));
			if (AKeysHandgun* Gun = AKeysHandgun::Get(this); Gun && Pp.Num() >= 2)
			{
				if (Cmd.StartsWith(TEXT("fire="))) Gun->DebugFire(FCString::Atod(*Pp[0]), FCString::Atod(*Pp[1]));
				else Gun->DebugLook(FCString::Atod(*Pp[0]), FCString::Atod(*Pp[1]));
			}
		}
		else if (Cmd == TEXT("banner")) { if (AKeysVisitors* Vs = AKeysVisitors::Get(this)) Vs->StartBanner(); }
		else if (Cmd.StartsWith(TEXT("indoorev="))) AKeysWalker::IndoorEV = FCString::Atod(*Cmd.Mid(9));
		else if (Cmd == TEXT("ownall")) { for (const FBoatSpec& B : Boats()) Owned.Add(TEXT("boat.") + B.Id); }
		else if (Cmd.StartsWith(TEXT("boat=")))
		{
			// boat=<id>: switch to that hull (the picker's preview, owned or not)
			const int32 I = BoatIndexOf(Cmd.Mid(5));
			if (I >= 0) { StopTroll(true); BoatIndex = PickIndex = I; Rebuild(); }
		}
		else if (Cmd.StartsWith(TEXT("throttle=")))
		{
			// throttle=0.3: hold the throttle lever there (for driving shots without a keyboard)
			ScriptThr = FCString::Atod(*Cmd.Mid(9));
		}
		else if (Cmd.StartsWith(TEXT("reel="))) bScriptReel = FCString::Atoi(*Cmd.Mid(5)) != 0;   // reel=1 holds S
		else if (Cmd.StartsWith(TEXT("steer="))) ScriptSteer = FCString::Atod(*Cmd.Mid(6));   // steer=0.3 (left positive)
		else if (Cmd.StartsWith(TEXT("catch=")))
		{
			// land a fish of that species (a fifth of the way up its size range) and show the catch card; catch=tourney: the tournament's first
			if (FState != EKeysFish::Idle) ReelIn(FString());
			Key = Cmd.Mid(6);
			if (Key == TEXT("tourney")) if (const AKeysEvents* Ev = AKeysEvents::Get(this)) Key = Ev->FirstSpecies();   // the first of the tournament's five
			Sp = Species(Key);
			if (Sp)
			{
				Weight = FMath::RoundToDouble((Sp->Min + (Sp->Max - Sp->Min) * .2) * 10) / 10;
				FX = X + FMath::Cos(H) * 8; FZ = Z - FMath::Sin(H) * 8;
				Bob = FVector(FX, 0, FZ);
				PickRecapture();
				BuildHookedMesh();
				Land();
			}
		}
		else if (Cmd.StartsWith(TEXT("fin:"))) DebugFinal(Cmd.Mid(4));
		else if (Cmd == TEXT("quit")) { if (APlayerController* PC = Cast<APlayerController>(GetController())) PC->ConsoleCommand(TEXT("quit")); }
		else MenuActivate(FName(*Cmd));
	}
}

// ================================================================ diving (web game entities/diver + entities/speargun)
bool AKeysBoatPawn::IsDiving() const
{
	const AKeysDiver* D = AKeysDiver::Get(this);
	return D && D->IsDiving();
}

void AKeysBoatPawn::ToggleDive()
{
	AKeysDiver* D = AKeysDiver::Get(this);
	if (!D || !bRunning) return;
	if (!D->IsDiving())
	{
		if (FState == EKeysFish::Caught) { ShowToast(TEXT("Finish with your catch first.")); return; }
		if (Sway() > .6) { ShowToast(TEXT("You've had a few too many to go over the side. Sleep it off first.")); return; }   // sim/steady.ts tooDrunkToDive
		// going over the side with a line out (or a cast still charging) would leave the rod with nobody on it
		if (bCharging || IsLineOut()) ReelIn(FString());
		bCharging = false;
		bFwd = bBack = bLeft = bRight = bTrimUp = bTrimDn = bAction = false;
		bTrimMode = false;
		D->SetDefaultChase(bDiveChase);
		D->UnderwaterHaze = UnderwaterHaze;
		// sim/dive-gear.ts: the pony bottle is live only with an air fill in hand, and one fill is spent per dive at 2 m
		bTankLive = HasGear(TEXT("dive.tank")) && CountOf(TEXT("dive.fill")) > 0;
		bFillCharged = false;
		if (HasGear(TEXT("dive.tank")) && !bTankLive) ShowToast(TEXT("The pony bottle is empty - an air fill at Scuba Steve's makes it live again."), 4);
		D->AirMul = (bTankLive ? 2.2 : 1) * (HasGear(TEXT("dive.wetsuit")) ? 1.15 : 1) * (HasGear(TEXT("dive.fins")) ? 1.1 : 1) * (BuffOn(TEXT("diner.mahi")) ? 1.05 : 1);
		D->EnterWater(X, Z, H);
		ShowToast(TEXT("Overboard - WASD and the mouse to swim and look, Space / C to ascend / descend, Shift to sprint. Click to fire, R to reload. J to climb back aboard."), 6);
	}
	else if (!D->RequestReboard()) ShowToast(TEXT("Too far from the boat to climb aboard."));
	else { SyncChaseCam(); bCamInit = false; }
}

void AKeysBoatPawn::HandleDiveInput(APlayerController* PC, float Dt)
{
	auto Down = [PC](const FKey& K) { return PC->IsInputKeyDown(K); };
	auto Hit = [PC](const FKey& K) { return PC->WasInputKeyJustPressed(K); };
	AKeysDiver* D = AKeysDiver::Get(this);
	bFwd = bBack = bLeft = bRight = bTrimUp = bTrimDn = bAction = false;
	bCharging = false;
	if (!D) return;
	FKeysDiverButtons B;
	B.bFwd = Down(EKeys::W) || Down(EKeys::Up);
	B.bBack = Down(EKeys::S) || Down(EKeys::Down);
	B.bLeft = Down(EKeys::A) || Down(EKeys::Left);
	B.bRight = Down(EKeys::D) || Down(EKeys::Right);
	B.bAscend = Down(EKeys::SpaceBar);
	B.bDescend = Down(EKeys::C) || Down(EKeys::LeftControl) || Down(EKeys::RightControl);
	B.bSprint = Down(EKeys::LeftShift) || Down(EKeys::RightShift);
	B.bHaul = B.bBack;
	B.bFire = Hit(EKeys::LeftMouseButton);
	B.bDrawToggle = Hit(EKeys::R);
	float MX = 0, MY = 0;
	PC->GetInputMouseDelta(MX, MY);
	B.LookYaw = MX * .004;
	B.LookPitch = -MY * .004;
	D->SetButtons(B);
	if (Hit(EKeys::J)) ToggleDive();
	if (Hit(EKeys::F)) if (AKeysTreasure* Tr = AKeysTreasure::Get(this)) Tr->TryOpen();   // prise open a wreck's box (sim/treasure.ts)
	if (Hit(EKeys::One) || Hit(EKeys::NumPadOne)) ShowToast(D->CycleView() + TEXT(" - press 1 to switch"));
	// a speared catch: K keeps it, Space releases it (once the card has been up a moment)
	if (FState == EKeysFish::Caught)
	{
		if (Hit(EKeys::K) && GetWorld()->GetTimeSeconds() - CaughtT > .7) KeepFish();
		else if (Hit(EKeys::SpaceBar) && GetWorld()->GetTimeSeconds() - CaughtT > 1.6) ReleaseFish();
	}
	if (Hit(EKeys::M)) bBigMap = !bBigMap;
	if (Hit(EKeys::H))
	{
		const int32 I = (SeaIndex() + 1) % SeaStates().Num();
		SetSeaState(I);
		ShowToast(SeaStates()[I].Name + TEXT(" - ") + SeaStates()[I].Desc);
	}
	if (Hit(EKeys::O))
		if (AKeysWorld* W = World()) { W->ToggleSunset(); ShowToast(W->IsSunset() ? TEXT("Sunset mode - golden hour over the Seven Mile Bridge.") : TEXT("Back to daylight.")); }
}

void AKeysBoatPawn::LandSpeared(const FString& InKey, double InWeight, double InFX, double InFZ)
{
	if (FState == EKeysFish::Caught) FinishCatch();
	Sp = Species(InKey);
	if (!Sp) return;
	Key = InKey;
	Weight = InWeight;
	FX = InFX; FZ = InFZ;
	Zone = ZoneAt(FX, FZ);
	Spot = FKeysFishSpot();
	Spot.Zone = Zone;
	CurTag = -1;
	BuildHookedMesh();
	Land();
	CardBonus = CardBonus.IsEmpty() ? FString(TEXT("Speared underwater")) : TEXT("Speared underwater - ") + CardBonus;
	bCardSpeared = true;
	if (bCanKeep) CardNote = TEXT("Release it for a 25% conservation bonus, or keep it and weigh in at a marina dock. (A speared fish can't be tagged.)");
	ShowFishMesh(true);
}

// ================================================================ ashore (web game entities/land/**, entities/town/**, sim/shop.ts)
bool AKeysBoatPawn::IsAshore() const
{
	const AKeysWalker* W = AKeysWalker::Get(this);
	return W && W->IsAshore();
}

void AKeysBoatPawn::ToggleAshore()
{
	AKeysWalker* W = AKeysWalker::Get(this);
	if (!W || !bRunning) return;
	if (!W->IsAshore())
	{
		if (FState == EKeysFish::Caught) { ShowToast(TEXT("Finish with your catch first.")); return; }
		FString Why;
		if (!W->TryEnter(X, Z, H, Speed, Spec().Len, Why)) { ShowToast(Why); return; }
		if (bCharging || IsLineOut()) ReelIn(FString());
		bCharging = false;
		bFwd = bBack = bLeft = bRight = bTrimUp = bTrimDn = bAction = false;
		bTrimMode = false;
		PinX = X; PinZ = Z; PinH = H;
		ApplyOutfit();
		Thr = Speed = 0;
		ShowToast(FString::Printf(TEXT("Tied up at %s. WASD and the mouse to walk, Shift to run, F at a counter or a door, 1 for the view, Q back aboard."), *Why), 6);
		// the run's trophy goes up on the dock; at your own estate the fish already hangs there (dock-celebration.ts)
		if (AKeysFinal* Fn = AKeysFinal::Get(this); Fn && !Why.Contains(TEXT("your dock")))
			Fn->OnAshore(RunBest.Key, RunBest.Weight, RunBest.Id, RunBest.bReleased, RunBest.Boat, W->Pos().X, W->Pos().Z, X, Z);
		return;
	}
	switch (W->RequestBoard())
	{
		case AKeysWalker::EBoard::Boarded: if (AKeysFinal* Fn = AKeysFinal::Get(this)) Fn->OnLeaveShore(); SyncChaseCam(); bCamInit = false; ShowToast(TEXT("Back aboard. Cast off!")); break;
		case AKeysWalker::EBoard::Far: ShowToast(FString::Printf(TEXT("The boat is %d m %s of you. Press Q again for a cab back to the dock."), FMath::RoundToInt(W->BoardDistance()), *W->BoatBearing()), 4); break;
		case AKeysWalker::EBoard::Cab: ShowToast(TEXT("The cab drops you back on the dock. The driver says your boat's still there.")); break;
	}
}

void AKeysBoatPawn::HandleWalkInput(APlayerController* PC, float Dt)
{
	auto Down = [PC](const FKey& K) { return PC->IsInputKeyDown(K); };
	auto Hit = [PC](const FKey& K) { return PC->WasInputKeyJustPressed(K); };
	AKeysWalker* W = AKeysWalker::Get(this);
	bFwd = bBack = bLeft = bRight = bTrimUp = bTrimDn = bAction = false;
	bCharging = false;
	if (!W) return;
	if (W->bShopOpen)
	{
		// the counter panel: Up / Down (W / S) pick, Enter or Space buys, 1-9 buy that row, F / Esc / Q walks away
		const KeysTown::FVenue* V = KeysTown::Venue(W->ShopVenue);
		const int32 N = V ? V->Sells.Num() : 0;
		if (N)
		{
			if (Hit(EKeys::Up) || Hit(EKeys::W)) W->ShopSel = (W->ShopSel + N - 1) % N;
			if (Hit(EKeys::Down) || Hit(EKeys::S)) W->ShopSel = (W->ShopSel + 1) % N;
			if (Hit(EKeys::Enter) || Hit(EKeys::SpaceBar)) ShopBuy(W->ShopSel);
			static const FKey Digits[9] = {EKeys::One, EKeys::Two, EKeys::Three, EKeys::Four, EKeys::Five, EKeys::Six, EKeys::Seven, EKeys::Eight, EKeys::Nine};
			for (int32 i = 0; i < FMath::Min(9, N); i++) if (Hit(Digits[i])) { W->ShopSel = i; ShopBuy(i); }
		}
		if (Hit(EKeys::F) || Hit(EKeys::Q)) ShopClose();
		W->SetButtons(FKeysWalkButtons());
		return;
	}
	FKeysWalkButtons B;
	B.bFwd = Down(EKeys::W) || Down(EKeys::Up);
	B.bBack = Down(EKeys::S) || Down(EKeys::Down);
	B.bLeft = Down(EKeys::A);
	B.bRight = Down(EKeys::D);
	B.bSprint = Down(EKeys::LeftShift) || Down(EKeys::RightShift);
	B.Turn = (Down(EKeys::Right) ? 1 : 0) - (Down(EKeys::Left) ? 1 : 0);
	float MX = 0, MY = 0;
	PC->GetInputMouseDelta(MX, MY);
	B.LookYaw = MX * .004;
	B.LookPitch = MY * .004;
	W->SetButtons(B);
	if (Hit(EKeys::Q)) ToggleAshore();
	if (Hit(EKeys::F)) WalkInteract();
	if (Hit(EKeys::One) || Hit(EKeys::NumPadOne) || Hit(EKeys::V)) ShowToast(W->CycleView() + TEXT(" - press 1 to switch"));
	if (Hit(EKeys::M)) bBigMap = !bBigMap;
	if (Hit(EKeys::O))
		if (AKeysWorld* Wd = World()) { Wd->ToggleSunset(); ShowToast(Wd->IsSunset() ? TEXT("Sunset over the harbour - the neon comes on.") : TEXT("Back to daylight.")); }
}

void AKeysBoatPawn::WalkInteract()
{
	AKeysWalker* W = AKeysWalker::Get(this);
	if (!W) return;
	if (!W->NearText().IsEmpty()) { ShowToast(W->NearText(), 7); return; }
	const FString V = W->NearVenue();
	if (V.IsEmpty()) { ShowToast(TEXT("Nothing to do here. Walk up to a counter, a slot machine or a For Sale sign.")); return; }
	if (V.StartsWith(TEXT("estate:")))
	{
		// Harbor Row: a town lot behind the casino (content/estates.ts tier 'harbor')
		const FString Id = V.Mid(7);
		for (const KeysTown::FEstateLot& E : KeysTown::HarborRow())
		{
			if (Id != E.Id) continue;
			if (Estates.Contains(Id)) { ShowToast(FString::Printf(TEXT("%s is yours. The pool is warm and the neighbours are the casino."), E.Name)); return; }
			if (Cash < E.Price) { ShowToast(FString::Printf(TEXT("%s - $%s. You have $%s. Sell some fillets at a weigh-in (E at a marina dock)."), E.Name, *Commas(E.Price), *Commas(Cash)), 5); return; }
			Cash -= E.Price;
			Estates.Add(Id);
			SaveEconomy();
			if (AKeysTown* Tn = AKeysTown::Get(this)) Tn->SetOwnedEstates(Estates);
			ShowToast(FString::Printf(TEXT("SOLD! %s is yours - the builders were suspiciously quick. $%s left."), E.Name, *Commas(Cash)), 6);
			return;
		}
		return;
	}
	const KeysTown::FVenue* Vn = KeysTown::Venue(V);
	if (!Vn) return;
	W->bShopOpen = true;
	W->ShopVenue = V;
	W->ShopSel = 0;
	W->ShopGreeting = Vn->Greeting.Num() ? Vn->Greeting[W->Visits % Vn->Greeting.Num()] : TEXT("");
	W->Visits++;
}

void AKeysBoatPawn::ShopClose()
{
	if (AKeysWalker* W = AKeysWalker::Get(this)) W->bShopOpen = false;
}

void AKeysBoatPawn::ShopBuy(int32 Index)
{
	AKeysWalker* W = AKeysWalker::Get(this);
	if (!W) return;
	const KeysTown::FVenue* V = KeysTown::Venue(W->ShopVenue);
	if (!V || !V->Sells.IsValidIndex(Index)) return;
	const KeysTown::FShelfItem& It = V->Sells[Index];
	const FString Eff = It.Effect;
	const bool bGear = Eff.StartsWith(TEXT("rod.")) || Eff.StartsWith(TEXT("reel.")) || (Eff.StartsWith(TEXT("dive.")) && Eff != TEXT("dive.fill")) || Eff.StartsWith(TEXT("spear.long")) || Eff.StartsWith(TEXT("spear.reel"))
		|| Eff.StartsWith(TEXT("cosmetic.")) || Eff == TEXT("handgun.extmag") || Eff == TEXT("handgun.dot") || Eff.StartsWith(TEXT("tackle."));
	if (Eff.StartsWith(TEXT("cosmetic.")) && Owned.Contains(Eff))
	{
		// sim/wardrobe.ts wear: one worn key per slot; the same one again takes it off
		const FString Slot = Eff.Mid(9).Left(Eff.Mid(9).Find(TEXT(".")));
		if (Wearing.FindRef(Slot) == Eff) { Wearing.Remove(Slot); ShowToast(FString::Printf(TEXT("You take off the %s."), It.Name)); }
		else { Wearing.Add(Slot, Eff); ShowToast(FString::Printf(TEXT("You put on the %s."), It.Name)); }
		ApplyOutfit();
		SaveEconomy();
		return;
	}
	if (Eff == TEXT("casino.slot")) { ShopClose(); if (AKeysSlots* Sl = AKeysSlots::Get(this)) Sl->Open(true); return; }   // the machines settle on their own screen (game/casino)
	if (bGear && Owned.Contains(Eff)) { ShowToast(FString::Printf(TEXT("You already own the %s."), It.Name)); return; }
	if (Eff.StartsWith(TEXT("tackle.")) && Eff == TEXT("tackle.braid80") && Owned.Contains(TEXT("tackle.braid130"))) { ShowToast(TEXT("You already fish 130 lb. The 80 lb rig would be a step down.")); return; }
	{ FString Why; if (ServiceRefuse(Eff, Why)) { ShowToast(Why, 7); return; } }   // refused before charging (sim/services.ts canBuy)
	if (Cash < It.Price) { ShowToast(FString::Printf(TEXT("%s is $%s and you have $%s. Sell fillets at a weigh-in (E at a marina dock)."), It.Name, *Commas(It.Price), *Commas(Cash)), 4); return; }
	Cash -= It.Price;
	const double T = GetWorld()->GetTimeSeconds();
	FString Msg = FString::Printf(TEXT("Bought: %s ($%s). $%s left."), It.Name, *Commas(It.Price), *Commas(Cash));
	if (bGear) Owned.Add(Eff);
	if (KeysBuffs::Seconds(Eff) > 0)
	{
		// a second one starts the clock again (sim/shop.ts buy); the strip and the end toast come from UpdateBuffs
		const bool bTop = Buffs.Contains(Eff);
		Buffs.Add(Eff, T + KeysBuffs::Seconds(Eff));
		Msg += TEXT(" ") + (bTop ? KeysBuffs::Label(Eff) + TEXT(": topped up.") : KeysBuffs::StartText(Eff));
	}
	if (Eff == TEXT("leader.fluoro")) { AddCount(*Eff, 1); Msg += FString::Printf(TEXT(" %d leaders: the next toothy fish cannot bite through."), CountOf(*Eff)); }
	else if (Eff.StartsWith(TEXT("tackle."))) Msg += Eff == TEXT("tackle.braid130") ? TEXT(" 130 lb line on the reels now.") : TEXT(" 80 lb line on the reels now.");
	else if (Eff == TEXT("handgun.ammo")) { AddCount(*Eff, 50); Msg += FString::Printf(TEXT(" %d rounds for the console pistol (9)."), CountOf(*Eff)); }
	else if (Eff == TEXT("safety.flare")) { AddCount(*Eff, 3); Msg += FString::Printf(TEXT(" %d flares: Z with the pistol out."), CountOf(*Eff)); }
	else if (Eff == TEXT("spear.shafts")) { AddCount(*Eff, 3); }
	else if (Eff == TEXT("dive.fill")) { AddCount(*Eff, 1); Msg += FString::Printf(TEXT(" %d fill%s: the pony bottle is live for that many dives."), CountOf(*Eff), CountOf(*Eff) == 1 ? TEXT("") : TEXT("s")); }
	else if (Eff == TEXT("dive.tank")) { AddCount(TEXT("dive.fill"), 1); Msg += TEXT(" Steve throws in the first fill. Each dive past 2 m spends one."); }
	else if (Eff == TEXT("bait.shrimp")) { BaitCasts += 12; Msg += TEXT(" Quicker bites on your next 12 casts."); }
	else if (Eff == TEXT("bait.ballyhoo")) { BaitCasts += 6; Msg += TEXT(" Quicker bites on your next 6 casts."); }
	else if (ServiceResolve(Eff, Msg)) {}   // the motel, the cabaret, the scratch-off, the wash, the socks (KeysTags.cpp)
	else if (Eff == TEXT("casino.slot"))
	{
		// three reels of fish: two of a kind pays 2x, three pays 10x (sim/slots.ts, simplified)
		static const TCHAR* Reel[] = {TEXT("Tarpon"), TEXT("Permit"), TEXT("Bonefish"), TEXT("Marlin"), TEXT("Snook")};
		const int32 A = FMath::RandRange(0, 4), B = FMath::RandRange(0, 4), C = FMath::RandRange(0, 4);
		const int32 Payout = (A == B && B == C) ? It.Price * 10 : (A == B || B == C || A == C) ? It.Price * 2 : 0;
		Cash += Payout;
		Msg = FString::Printf(TEXT("%s | %s | %s  -  %s  ($%s)"), Reel[A], Reel[B], Reel[C], Payout ? *FString::Printf(TEXT("you win $%d!"), Payout) : TEXT("nothing"), *Commas(Cash));
	}
	else if (Eff.StartsWith(TEXT("cosmetic."))) { Wearing.Add(Eff.Mid(9).Left(Eff.Mid(9).Find(TEXT("."))), Eff); ApplyOutfit(); Msg += Eff.StartsWith(TEXT("cosmetic.flag.")) ? TEXT(" It flies from your stern.") : TEXT(" You're wearing it."); }
	SaveEconomy();
	ShowToast(Msg, 5);
}

void AKeysBoatPawn::ApplyOutfit()
{
	AKeysWalker* W = AKeysWalker::Get(this);
	if (!W) return;
	// content/cosmetics.ts: what each worn key looks like (a save from before the wardrobe wears what it owns)
	if (!Wearing.Num())
		for (const FString& O : Owned)
			if (O.StartsWith(TEXT("cosmetic."))) Wearing.Add(O.Mid(9).Left(O.Mid(9).Find(TEXT("."))), O);
	auto C = [](uint32 X) { return FLinearColor(FColor(uint8(X >> 16), uint8(X >> 8), uint8(X))); };
	FLinearColor Shirt = C(0x3fb6c9), Cap = C(0x1d3557);
	const FString Sh = Wearing.FindRef(TEXT("shirt")), Cp = Wearing.FindRef(TEXT("cap"));
	if (Sh == TEXT("cosmetic.shirt.hawaiian")) Shirt = C(0xf0a01c);
	else if (Sh == TEXT("cosmetic.shirt.breton")) Shirt = C(0xf2efe6);
	else if (Sh == TEXT("cosmetic.shirt.sunset")) Shirt = C(0xffd166);
	else if (Sh == TEXT("cosmetic.shirt.luigi")) Shirt = C(0x2e9b3f);
	if (Cp == TEXT("cosmetic.cap.conch")) Cap = C(0xf4a58a);
	else if (Cp == TEXT("cosmetic.cap.bucket")) Cap = C(0xc9b37a);
	else if (Cp == TEXT("cosmetic.cap.luigi")) Cap = C(0x2e9b3f);
	W->SetOutfit(Shirt, true, Cap, true);
	if (AKeysExtras* Ex = AKeysExtras::Get(this)) Ex->SetCosmeticFlag(Wearing.FindRef(TEXT("flag")));
}

void AKeysBoatPawn::RunDests(TArray<FRunDest>& Out) const
{
	Out.Reset();
	for (const FMarina& M : Marinas()) Out.Add({M.Name, TEXT("marina"), M.SX, M.SZ + M.Dir * 48});
	Out.Add({TEXT("Sombrero Reef"), TEXT("the crest"), 60, 1462});
	for (const FHump& Hm : Humps()) Out.Add({Hm.Name, Hm.bPatch ? TEXT("patch reef") : TEXT("offshore hump"), Hm.X, ChainZ(Hm.X) + Hm.DZ});
	if (const AKeysLife* L = AKeysLife::Get(this))
	{
		TArray<FKeysMarker> Mk;
		L->GetMarkers(Mk);
		int32 N = 0;
		for (const FKeysMarker& M : Mk) if (M.Kind == 0) Out.Add({N++ == 0 ? FString(TEXT("The production platform")) : FString::Printf(TEXT("Oil rig %d"), N), TEXT("oil rig"), M.X + 130, M.Z});
	}
}

void AKeysBoatPawn::RunTo(int32 I)
{
	TArray<FRunDest> Ds;
	RunDests(Ds);
	if (!Ds.IsValidIndex(I)) return;
	// refuse while something is going on that teleporting would spoil
	const AKeysEvents* Ev = AKeysEvents::Get(this);
	FString Why;
	if (IsBusy(Why) || IsLineOut()) { if (Why.IsEmpty()) Why = TEXT("Reel in first."); }
	else if (Ev && (Ev->IsRacing() || (Ev->InTourney() && Ev->TourneyPhase() != AKeysEvents::ETourney::Over))) Why = TEXT("Not in the middle of a race or a tournament.");
	if (!Why.IsEmpty()) { Menu = EMenu::None; ShowToast(Why); return; }
	StopTroll(true);
	X = Ds[I].X; Z = Ds[I].Z;
	AKeysTraffic::FindClearSpot(X, Z, Spec());
	Speed = Thr = DVX = DVZ = 0;
	bCamInit = false;
	if (AKeysWorld* Wd = World()) { Wd->FocusX = X; Wd->FocusZ = Z; }
	Menu = EMenu::None;
	ShowToast(FString::Printf(TEXT("Run to %s."), *Ds[I].Name));
}

void AKeysBoatPawn::PrepareToLeaveHelm()
{
	if (bCharging || IsLineOut()) ReelIn(FString());
	bCharging = false;
	StopTroll(true);
	bFwd = bBack = bLeft = bRight = bTrimUp = bTrimDn = bAction = false;
	bTrimMode = false;
	ScriptThr = -1;
}

bool AKeysBoatPawn::ExternalCamera(FVector& OutPos, FRotator& OutRot, float& OutFov) const
{
	if (const AKeysTourneyShow* Sh = AKeysTourneyShow::Get(this); Sh && Sh->CameraFor(OutPos, OutRot, OutFov)) return true;
	if (const AKeysHandgun* G = AKeysHandgun::Get(this); G && G->CameraFor(OutPos, OutRot, OutFov)) return true;
	return false;
}

bool AKeysBoatPawn::IsBusy(FString& Why) const
{
	if (FState == EKeysFish::Fight) { Why = TEXT("Land your fish first."); return true; }
	if (FState == EKeysFish::Caught) { Why = TEXT("Finish with your catch first."); return true; }
	if (IsDiving()) { Why = TEXT("Climb back aboard first."); return true; }
	if (IsAshore()) { Why = TEXT("Back aboard first."); return true; }
	return false;
}

void AKeysBoatPawn::SaveEconomy()
{
	if (!Save) return;
	Save->Cash = Cash;
	Save->Owned = Owned.Array();
	Save->Estates = Estates;
	Save->Counts = Counts;
	Save->Wearing = Wearing;
	Save->TreasureOpened = TreasureOpened;
	Save->EstateBuilds = EstateBuilds;
	Save->Spins = Spins;
	Save->Tags = Tags;
	Save->Services = Svc;
	Save->WrecksFound = WrecksFound;
	Save->Write();
}

void AKeysBoatPawn::ApplyDiveCamera(const FVector& PosUE, const FRotator& Rot, float FovH)
{
	Camera->SetWorldLocationAndRotation(PosUE, Rot);
	Camera->SetFieldOfView(FovH);
	// a speared catch is held up in front of the mask for the photo (in the chase view, in front of the diver)
	if (FState == EKeysFish::Caught)
	{
		const double Age = GetWorld()->GetTimeSeconds() - CaughtT;
		const FQuat Q = Rot.Quaternion();
		const AKeysDiver* Dv = AKeysDiver::Get(this);
		if (Dv && Dv->IsChaseView())
		{
			const KeysDive::FState& DS = Dv->State();
			const FVector Fw(-FMath::Sin(DS.Yaw), -FMath::Cos(DS.Yaw), 0);
			const FVector P = ToUE(DS.X, DS.Y + .1, DS.Z) + Fw * (60 + FishLen * 50);
			FishMesh->SetWorldLocationAndRotation(P, FRotator(10, FMath::RadiansToDegrees(FMath::Atan2(Fw.Y, Fw.X)) + 90, 0));
			return;
		}
		// centimeters in the camera's frame: far enough out that the whole fish fits, a little left and low of center
		const double Back = (1.1 + FishLen * 1.2) * 100;
		const FVector P = PosUE + Q.RotateVector(FVector(Back, -FishLen * 30, -FishLen * 25 + FMath::Sin(Age * 2.1) * 3));
		const double Kick = FMath::Sin(Age * 5.3) * .14 * FMath::Exp(-Age * .35) + FMath::Sin(Age * 1.7) * .04;
		FishMesh->SetWorldLocationAndRotation(P, (Q * FRotator(10, 90 + FMath::RadiansToDegrees(Kick), FMath::RadiansToDegrees(Kick * .6)).Quaternion()).Rotator());
	}
}

// the mouse pointer shows while a menu, the leaderboard or the login box is up
void AKeysBoatPawn::UpdateCursor()
{
	APlayerController* PC = Cast<APlayerController>(GetController());
	const bool bWant = IsCapturingKeys();
	if (!PC || bWant == bCursorOn) return;
	bCursorOn = bWant;
	PC->bShowMouseCursor = bWant;
	PC->bEnableClickEvents = bWant;
	PC->bEnableMouseOverEvents = bWant;
	if (bWant)
	{
		FInputModeGameAndUI Mode;
		Mode.SetHideCursorDuringCapture(false);
		Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
		PC->SetInputMode(Mode);
	}
	else PC->SetInputMode(FInputModeGameOnly());
}

// ================================================================ input
void AKeysBoatPawn::HandleInput(float Dt)
{
	APlayerController* PC = Cast<APlayerController>(GetController());
	if (!PC) return;
	auto Down = [PC](const FKey& K) { return PC->IsInputKeyDown(K); };
	auto Hit = [PC](const FKey& K) { return PC->WasInputKeyJustPressed(K); };
	const double T = GetWorld()->GetTimeSeconds();

	// online login panel: typing goes into the name or crew code, nothing else reacts
	if (bLoginOpen)
	{
		bFwd = bBack = bLeft = bRight = bTrimUp = bTrimDn = bAction = false;
		bCharging = false;
		FString& Field = LoginField == 0 ? LoginName : LoginCrew;
		const int32 MaxLen = LoginField == 0 ? 20 : 24;
		const bool bShift = Down(EKeys::LeftShift) || Down(EKeys::RightShift);
		static const FKey Letters[26] = {EKeys::A, EKeys::B, EKeys::C, EKeys::D, EKeys::E, EKeys::F, EKeys::G, EKeys::H, EKeys::I, EKeys::J, EKeys::K, EKeys::L, EKeys::M,
			EKeys::N, EKeys::O, EKeys::P, EKeys::Q, EKeys::R, EKeys::S, EKeys::T, EKeys::U, EKeys::V, EKeys::W, EKeys::X, EKeys::Y, EKeys::Z};
		static const FKey Digits[10] = {EKeys::Zero, EKeys::One, EKeys::Two, EKeys::Three, EKeys::Four, EKeys::Five, EKeys::Six, EKeys::Seven, EKeys::Eight, EKeys::Nine};
		for (int32 i = 0; i < 26; i++) if (Hit(Letters[i]) && Field.Len() < MaxLen) Field.AppendChar(TCHAR((bShift ? 'A' : 'a') + i));
		for (int32 i = 0; i < 10; i++) if (Hit(Digits[i]) && Field.Len() < MaxLen) Field.AppendChar(TCHAR('0' + i));
		if (Hit(EKeys::SpaceBar) && !Field.IsEmpty() && Field.Len() < MaxLen) Field.AppendChar(TEXT(' '));
		if (Hit(EKeys::Hyphen) && Field.Len() < MaxLen) Field.AppendChar(TEXT('-'));
		if (Hit(EKeys::BackSpace) && Field.Len()) Field.LeftChopInline(1);
		if (Hit(EKeys::Tab) || Hit(EKeys::Up) || Hit(EKeys::Down)) LoginField = 1 - LoginField;
		if (Hit(EKeys::Escape)) bLoginOpen = false;
		if (Hit(EKeys::Enter)) SubmitLogin();
		return;
	}
	if (Hit(EKeys::L) && !bBoardOpen) { ToggleOnline(); return; }

	// the leaderboard is modal, like the JS panel: B (or Esc) closes it, Tab / Left / Right switch tabs
	if (bBoardOpen)
	{
		bFwd = bBack = bLeft = bRight = bTrimUp = bTrimDn = bAction = false;
		bCharging = false;
		if (Hit(EKeys::B) || Hit(EKeys::Escape)) bBoardOpen = false;
		{
			const AKeysOnline* O = AKeysOnline::Get(this);
			const int32 Tabs = O && O->IsOnline() ? 3 : 2;
			if (Hit(EKeys::Tab) || Hit(EKeys::Right) || Hit(EKeys::D)) BoardTab = (BoardTab + 1) % Tabs;
			if (Hit(EKeys::Left) || Hit(EKeys::A)) BoardTab = (BoardTab + Tabs - 1) % Tabs;
			BoardTab = FMath::Min(BoardTab, Tabs - 1);
		}
		return;
	}
	if (Hit(EKeys::B)) { bBoardOpen = true; BoardTab = 0; bFwd = bBack = bLeft = bRight = bAction = false; bCharging = false; return; }

	// the gear screen: Tab opens and closes it (Esc too), nothing else reacts while it is up
	if (bRunning && Menu == EMenu::None && !IsDiving())
	{
		if (bGearOpen) { if (Hit(EKeys::Tab) || Hit(EKeys::Escape)) bGearOpen = false; bFwd = bBack = bLeft = bRight = bAction = false; return; }
		if (Hit(EKeys::Tab)) { if (AKeysWalker* Wk = AKeysWalker::Get(this); !Wk || !Wk->bShopOpen) { bGearOpen = true; return; } }
	}
	// a slot machine in the Gaff & Gill: it owns the keys while you sit at it
	if (bRunning && Menu == EMenu::None)
		if (AKeysSlots* Sl = AKeysSlots::Get(this); Sl && Sl->bOpen)
		{
			bFwd = bBack = bLeft = bRight = bTrimUp = bTrimDn = bAction = false;
			bCharging = false;
			Sl->HandleInput(PC);
			return;
		}
	// the land office (Keys Run Estates): , opens it from the helm or ashore, and it owns the keys while it is up
	if (bRunning && Menu == EMenu::None)
		if (AKeysEstates* Es = AKeysEstates::Get(this))
		{
			if (!Es->bOpen && Hit(EKeys::Comma) && !IsDiving()) { Es->Open(true); return; }
			if (Es->bOpen)
			{
				bFwd = bBack = bLeft = bRight = bTrimUp = bTrimDn = bAction = false;
				bCharging = false;
				Es->HandleInput(PC);
				return;
			}
		}
	// the boat pistol owns the keys while it is out (web entities/handgun/input.ts): the boat drifts in neutral
	if (bRunning && Menu == EMenu::None)
		if (AKeysHandgun* Gun = AKeysHandgun::Get(this); Gun && Gun->IsOut())
		{
			bFwd = bBack = bLeft = bRight = bTrimUp = bTrimDn = bAction = false;
			bCharging = false;
			Gun->HandleInput(PC, Dt);
			if (Hit(EKeys::M)) bBigMap = !bBigMap;
			return;
		}
	// menus: Up / Down (or W / S, or the mouse) pick, Enter or Space chooses, Left / Right change a setting, Esc goes back
	if (Menu == EMenu::None && bRunning && IsAshore() && Hit(EKeys::Escape))
		if (AKeysWalker* Wk = AKeysWalker::Get(this); Wk && Wk->bShopOpen) { ShopClose(); return; }
	if (Menu == EMenu::None && bRunning && Hit(EKeys::Escape)) { Menu = EMenu::Pause; MenuSel = 0; bCharging = false; return; }
	if (Menu != EMenu::None)
	{
		bFwd = bBack = bLeft = bRight = bTrimUp = bTrimDn = bAction = false;
		bCharging = false;
		if (Menu == EMenu::Main) CamYaw += Dt * .05;   // the camera drifts slowly around your boat behind the main menu
		TArray<FMenuItem> Items;
		GetMenuItems(Items);
		const int32 N = Items.Num();
		MenuSel = FMath::Clamp(MenuSel, 0, FMath::Max(0, N - 1));
		if (N)
		{
			if (Hit(EKeys::Up) || Hit(EKeys::W)) MenuSel = (MenuSel + N - 1) % N;
			if (Hit(EKeys::Down) || Hit(EKeys::S) || Hit(EKeys::Tab)) MenuSel = (MenuSel + 1) % N;
			if (Items[MenuSel].bArrows && (Hit(EKeys::Left) || Hit(EKeys::A))) MenuActivate(Items[MenuSel].Id, -1);
			else if (Items[MenuSel].bArrows && (Hit(EKeys::Right) || Hit(EKeys::D))) MenuActivate(Items[MenuSel].Id, 1);
			else if (Hit(EKeys::Enter) || Hit(EKeys::SpaceBar)) MenuActivate(Items[MenuSel].Id);
		}
		if (Hit(EKeys::Escape))
		{
			if (Menu == EMenu::Pause) Menu = EMenu::None;
			else if (Menu == EMenu::Settings || Menu == EMenu::Controls) { Menu = MenuBack; MenuSel = 0; }
		}
		// -KRDemo on the command line: start on its own a few seconds in (for testing the look without a keyboard)
		if (Menu == EMenu::Main && FParse::Param(FCommandLine::Get(), TEXT("KRDemo")) && T > 4) StartRun(true);
		return;
	}

	// drag with either mouse button: orbits the chase camera, or looks around in the first-person views; the wheel zooms
	bDragging = !IsDiving() && !IsAshore() && (Down(EKeys::LeftMouseButton) || Down(EKeys::RightMouseButton));
	if (bDragging)
	{
		float MX = 0, MY = 0;
		PC->GetInputMouseDelta(MX, MY);
		if (bDriveOn && !bFpOn) { DYaw -= MX * .004; DPitch = Clamp(DPitch + MY * .004, -1, .7); }
		else if (bFpOn) { FpYaw -= MX * .004; FpPitch = Clamp(FpPitch + MY * .004, -1.1, .6); FpUserT = 2.5; }
		else
		{
			CamYaw -= MX * .006;
			CamPitchOff = Clamp(CamPitchOff + MY * .006, -.6, 1.6);
		}
	}
	if (Hit(EKeys::MouseScrollUp)) CamZoom = FMath::Max(.55, CamZoom * .9);
	if (Hit(EKeys::MouseScrollDown)) CamZoom = FMath::Min(3.0, CamZoom / .9);

	if (!bRunning) return;
	if (IsDiving()) { HandleDiveInput(PC, Dt); return; }
	if (IsAshore()) { HandleWalkInput(PC, Dt); return; }
	if (Hit(EKeys::J)) { ToggleDive(); return; }
	if (Hit(EKeys::Q)) { ToggleAshore(); return; }

	bFwd = Down(EKeys::W) || (!bTrimMode && Down(EKeys::Up));
	bBack = Down(EKeys::S) || (!bTrimMode && Down(EKeys::Down)) || bScriptReel;
	bLeft = Down(EKeys::A) || Down(EKeys::Left);
	bRight = Down(EKeys::D) || Down(EKeys::Right);
	if (FParse::Param(FCommandLine::Get(), TEXT("KRDemo")))
	{
		// full throttle, with a gentle turn every so often so the wake curves
		bFwd = true;
		const double Fdx = -FMath::Sin(H), Fdz = -FMath::Cos(H);
		bLeft = FMath::Fmod(T, 6.0) > 4.3 || DepthFast(X + Fdx * 150, Z + Fdz * 150) < 6 || DepthFast(X + Fdx * 60, Z + Fdz * 60) < 6;
	}
	bTrimUp = bTrimMode && Down(EKeys::Up);
	bTrimDn = bTrimMode && Down(EKeys::Down);

	if (Hit(EKeys::SpaceBar)) { bAction = true; OnAction(); }
	if (PC->WasInputKeyJustReleased(EKeys::SpaceBar))
	{
		bAction = false;
		if (bCharging) { bCharging = false; CastLine(Charge); }
	}
	if (Hit(EKeys::X))
	{
		// travel to Miami and back
		double NX, NZ, NH;
		FString Msg;
		if (AKeysExtras::MiamiDestination(X, Z, Spec().Draft, NX, NZ, NH, Msg))
		{
			if (IsLineOut()) ReelIn(TEXT(""));
			StopTroll(true);
			X = NX; Z = NZ; H = NH;
			Speed = Thr = DVX = DVZ = 0;
			bCamInit = false;
			if (AKeysWorld* Wd = World()) { Wd->FocusX = X; Wd->FocusZ = Z; }
		}
		ShowToast(Msg, 5);
	}
	if (Hit(EKeys::R) && (FState == EKeysFish::Waiting || FState == EKeysFish::Casting || FState == EKeysFish::Nibble)) ReelIn(TEXT("Line reeled in."));
	else if (Hit(EKeys::R) && FState == EKeysFish::Idle && !bCharging)
		if (AKeysEvents* Ev = AKeysEvents::Get(this)) { StopTroll(true); Ev->ToggleRace(); }   // the Marathon race (web game/race)
	if (Hit(EKeys::T) && FState == EKeysFish::Caught) { if (T - CaughtT > .7) TagAndRelease(); }
	else if (Hit(EKeys::T))
	{
		if (FState == EKeysFish::Fight)
		{
			Drag = FMath::Clamp(Drag + (Down(EKeys::LeftShift) || Down(EKeys::RightShift) ? -1 : 1), 1, 5);
			ShowToast(Drag >= 4 ? FString::Printf(TEXT("Drag tightened (%d/5) - more pressure, more risk of a break-off."), Drag)
				: Drag <= 2 ? FString::Printf(TEXT("Drag loosened (%d/5) - the fish can take line easier."), Drag)
				: FString::Printf(TEXT("Drag set to %d/5."), Drag));
		}
		else
		{
			bTrimMode = !bTrimMode;
			ShowToast(bTrimMode ? TEXT("Trim mode on - Up trims up, Down trims down. Throttle holds where you left it; W/S still change it. T to exit.") : TEXT("Trim mode off."));
		}
	}
	if (Hit(EKeys::K) && T - CaughtT > .7) KeepFish();
	if (Hit(EKeys::Equals) || Hit(EKeys::Add)) ToggleTroll();
	if (Hit(EKeys::Nine)) if (AKeysHandgun* Gun = AKeysHandgun::Get(this)) Gun->Toggle();   // the boat pistol
	if (Hit(EKeys::Semicolon)) ToggleLights();   // the web game's L (which logs in here)
	if (Hit(EKeys::E))
	{
		if (NearMarina(X, Z) && FMath::Abs(Speed) < 2.5) WeighIn();
		else if (NearMarina(X, Z)) ShowToast(TEXT("Slow down to tie up at the dock."));
	}
	if (Hit(EKeys::H))
	{
		const int32 I = (SeaIndex() + 1) % SeaStates().Num();
		SetSeaState(I);
		ShowToast(SeaStates()[I].Name + TEXT(" - ") + SeaStates()[I].Desc);
	}
	if (Hit(EKeys::F))
	{
		bAutoChase = !bAutoChase;
		ShowToast(bAutoChase ? TEXT("Auto-chase on - the boat will follow hooked fish.") : TEXT("Auto-chase off - you drive during the fight."));
	}
	if (Hit(EKeys::M)) bBigMap = !bBigMap;
	if (Hit(EKeys::C)) CamZoom = CamZoom > 1.4 ? 1 : 1.9;
	if (Hit(EKeys::O))
	{
		if (AKeysWorld* W = World())
		{
			W->ToggleSunset();
			ShowToast(W->IsSunset() ? TEXT("Sunset mode - golden hour over the Seven Mile Bridge.") : TEXT("Back to daylight."));
		}
	}
	if (Hit(EKeys::U))
	{
		Gear = (Gear + 1) % 3;
		if (AKeysAudio* A = AKeysAudio::Get(this)) A->GearShift();
		ShowToast(Gear == 0 ? TEXT("In gear - forward.") : Gear == 1 ? TEXT("Neutral - W revs the motors.") : TEXT("Reverse - W backs you down."));
	}
	if (Hit(EKeys::One) || Hit(EKeys::NumPadOne)) CycleView();
	if (Hit(EKeys::V))
	{
		bFpPref = !bFpPref;
		ShowToast(bFpPref ? TEXT("First-person fishing view on.") : TEXT("Third-person fishing view."));
	}
	if (Hit(EKeys::G))
	{
		bElectronics = !bElectronics;
		ShowToast(bElectronics ? TEXT("Chartplotter and fish finder on - [ and ] zoom the chart, G hides them.") : TEXT("Electronics hidden."));
	}
	const bool bZoomIn = Hit(EKeys::LeftBracket), bZoomOut = Hit(EKeys::RightBracket);
	if (bZoomIn || bZoomOut)
	{
		ChartRange = Clamp(ChartRange * (bZoomIn ? .7 : 1 / .7), 120, 2400);
		bElectronics = true;
		ShowToast(FString::Printf(TEXT("Chart range %.2f nm"), ChartRange * 2 * .00054 * 2.5));
	}
}

void AKeysBoatPawn::CycleView()
{
	// chase, the helm, and on the boats with a tuna tower, up top at the tower helm
	FVector Eye;
	View = (View + 1) % (KeysMesh::TowerEye(Spec(), Eye) ? 3 : 2);
	ShowToast(View == 0 ? TEXT("Third-person view - press 1 to switch") : View == 1 ? TEXT("First person from the helm - press 1 to switch")
		: TEXT("Up in the tuna tower - spot fish, birds and weedlines from up high. Press 1 to switch"));
}

// ================================================================ boat
bool AKeysBoatPawn::AutoChase(double TopMs, bool bCanDrive, double& OutThr, double& OutSteer)
{
	if (!bAutoChase || !bCanDrive || FState != EKeysFish::Fight || bFwd || bLeft || bRight) { bAutoOn = false; return false; }
	const double DX = FX - X, DZ = FZ - Z, Dist = Hyp(DX, DZ);
	const double Want = FMath::Atan2(-DX, -DZ);
	const double Diff = WrapPi(Want - H), DiffStern = WrapPi(Want - (H + PI));
	double T = 0, St = 0;
	// sportfish style: keep the stern pointed at the fish and back down on it; only spin around and run forward for a fish far out ahead
	if (!(FMath::Abs(Diff) < .9 && Dist > 90))
	{
		St = Clamp(-DiffStern * 2.2, -1, 1);
		const double V = Clamp((Dist - 14) * .22 + (bRunningFish ? 2.5 : 0), 0, 6) * (FMath::Abs(DiffStern) > .8 ? .35 : 1);
		T = -FMath::Min(.5, V / TopMs);
		AutoMode = T < 0 ? TEXT("Backing down on the fish") : TEXT("Holding the stern on the fish");
	}
	else
	{
		St = Clamp(Diff * 2.5, -1, 1);
		const double V = Clamp((Dist - 60) * .3 + (bRunningFish ? 3 : 0), 0, 12) * (FMath::Abs(Diff) > 1 ? .4 : 1);
		T = V / TopMs;
		AutoMode = TEXT("Running up on the fish");
	}
	const double Fx = -FMath::Sin(H), Fz = -FMath::Cos(H), Dir = T < 0 ? -1 : 1;
	const double Ahead = DepthFast(X + Fx * 16 * Dir, Z + Fz * 16 * Dir);
	if (Ahead < Spec().Draft + .35 || LandH(X + Fx * 12 * Dir, Z + Fz * 12 * Dir) > -.3)
	{
		T = 0;
		AutoMode = TEXT("Stopped - shallow water");
		const double Now = GetWorld()->GetTimeSeconds();
		if (Now - AcMsgT > 5) { AcMsgT = Now; ShowToast(TEXT("Auto-chase stopped: shallow water. Steer it yourself.")); }
	}
	bAutoOn = true;
	OutThr = T;
	OutSteer = St;
	return true;
}

void AKeysBoatPawn::Slam(double V)
{
	const double K = Clamp(-V / 6, 0, 1);
	if (AKeysAudio* A = AKeysAudio::Get(this)) A->Slam(K);
	Shake = FMath::Max(Shake, K);
	Speed *= 1 - .06 * K;
	const double Now = GetWorld()->GetTimeSeconds();
	if (K > .6 && Now - SlamT > 6)
	{
		SlamT = Now;
		static const TCHAR* Msgs[] = {TEXT("Stuffed it! Trim up and ease off a little."), TEXT("Big air - hang on!"), TEXT("That one hurt. Back off in the slop.")};
		ShowToast(Msgs[FMath::RandRange(0, 2)]);
	}
}

void AKeysBoatPawn::UpdateBoat(double Dt, double T)
{
	const FBoatSpec& S = Spec();
	const double TopMs = S.Top * .5144 * SpeedScale;
	const AKeysAudio* Aud = AKeysAudio::Get(this);
	const bool bCanDrive = bRunning && FState != EKeysFish::Caught && (!Aud || Aud->EnginesOn());
	const bool bLineOut = FState == EKeysFish::Casting || FState == EKeysFish::Waiting || FState == EKeysFish::Nibble || FState == EKeysFish::Bite || FState == EKeysFish::Fight;

	double TT = 0;
	if (bCanDrive)
	{
		if (Gear == 0) TT = bFwd ? 1 : (bBack && !bLineOut) ? -.35 : 0;
		else if (Gear == 2) TT = (bFwd || (bBack && !bLineOut)) ? -.35 : 0;
	}
	if (bTrimMode && bCanDrive && !bFwd && !bBack) TT = Thr;   // throttle holds while you trim
	double AcThr = 0, AcSteer = 0;
	const bool bAc = AutoChase(TopMs, bCanDrive, AcThr, AcSteer);
	if (bAc) TT = AcThr;
	double TrollT = 0;
	if (!bAc && bCanDrive && TrollThrottle(TopMs, TrollT)) { TT = TrollT; Gear = 0; }   // trolling holds seven knots
	else if (!bAc && bCanDrive && ScriptThr >= 0) TT = ScriptThr;
	Thr += (TT - Thr) * FMath::Min(1.0, Dt * 2.5);

	// trim: the sweet spot moves out as you get on plane; too far in plows the bow, too far out blows the props out
	if (bTrimUp) TrimV = FMath::Min(1.0, TrimV + Dt * .35);
	if (bTrimDn) TrimV = FMath::Max(0.0, TrimV - Dt * .35);
	const double Fr = FMath::Abs(Speed) / TopMs, Opt = Fr < .35 ? .15 : .15 + .55 * FMath::Min(1.0, (Fr - .35) / .5), Off = TrimV - Opt;
	TrimVent = Fr > .4 && TrimV > .82 ? FMath::Min(1.0, (TrimV - .82) / .18 * (Fr - .4) / .4 * 1.6) : 0;
	const double TrimK = (1 + .06 - .35 * Off * Off) * (1 - .25 * TrimVent);
	const double Target = Thr * TopMs * TrimK;
	const double HoleShot = Fr < .4 && Thr > .5 ? 1 + .45 * (.3 - TrimV) : 1;
	Speed += (Target - Speed) * Dt * (FMath::Abs(Thr) < 1e-3 ? S.Accel * .5 : S.Accel * HoleShot);
	if (TrimVent > .2 && T - TrimMsgT > 5) { TrimMsgT = T; ShowToast(TEXT("Relax, Chum Lee - your props are out of the water!")); }

	const double D = DepthFast(X, Z);
	if (D < S.Draft) Speed = Clamp(Speed, -1.2, 1.5);
	const double St = bAc ? AcSteer : bCanDrive ? (bLeft ? 1 : bRight ? -1 : ScriptSteer) : 0;
	Steer += (St - Steer) * FMath::Min(1.0, Dt * 4);
	const double F = FMath::Abs(Speed) / TopMs;
	H += S.Turn * Steer * (.3 + FMath::Min(1.0, FMath::Abs(Speed) / 8) * .9) * (1 - .35 * F) * (1.15 - .35 * TrimV) * (Speed < -.2 ? -1 : 1) * Dt;
	const double Fx = -FMath::Sin(H), Fz = -FMath::Cos(H);
	double NX = X + Fx * Speed * Dt, NZ = Z + Fz * Speed * Dt;

	// land collisions: beaches, banks and creek walls
	for (const double OffL : {0.0, S.Len * .42, -S.Len * .32})
	{
		const double PX = NX + Fx * OffL, PZ = NZ + Fz * OffL, HN = LandH(PX, PZ);
		if (HN > .15 && HN >= LandH(X + Fx * OffL, Z + Fz * OffL) - .01)
		{
			NX = X; NZ = Z;
			Speed *= ((OffL >= 0) == (Speed >= 0)) ? .1 : .6;
			if (T - CollideMsgT > 3) { CollideMsgT = T; ShowToast(TEXT("Beached! Back off with S.")); }
			break;
		}
	}
	// piling collisions
	TArray<const FPillar*> Near;
	PillarsNear(NX, NZ, Near);
	if (const AKeysLife* L = AKeysLife::Get(this))
		for (const FPillar& RP : L->RigPillars()) if (Hyp(RP.X - NX, RP.Z - NZ) < 40) Near.Add(&RP);
	for (const FPillar* P : Near)
		for (const double OffL : {0.0, S.Len * .4, -S.Len * .4})
		{
			const double PX = NX + Fx * OffL, PZ = NZ + Fz * OffL, DX = PX - P->X, DZ = PZ - P->Z, DD = Hyp(DX, DZ), R = S.Beam * .5 + P->R;
			if (DD < R && DD > .001)
			{
				NX += DX / DD * (R - DD);
				NZ += DZ / DD * (R - DD);
				Speed *= .4;
				if (T - CollideMsgT > 2) { CollideMsgT = T; ShowToast(TEXT("Bumped a bridge piling!")); }
			}
		}
	X = Clamp(NX, WBX0, WBX1);
	Z = Clamp(NZ, WBZ0, WBZ1);
	if (NX < WBX0 + 1 || NX > WBX1 - 1 || NZ < WBZ0 + 1 || NZ > WBZ1 - 1)
	{
		Speed *= .9;
		if (T - CollideMsgT > 4) { CollideMsgT = T; ShowToast(TEXT("Edge of the chart - turn back toward Marathon.")); }
	}

	// ride on the waves
	const double Amp = AmpAt(X, Z), Rx = FMath::Cos(H), Rz = -FMath::Sin(H);
	const double Plane = .11 * FMath::Sin(FMath::Min(F * 2.6, 1.0) * PI) * (1.25 - .5 * TrimV) + .035 * F + (TrimV - .3) * .11 * FMath::Min(1.0, F * 2) + TrimVent * .05 * FMath::Sin(T * 9);
	// rollers push the hull: running down the back of a swell speeds you up, climbing a face slows you, and a stopped boat surges with the swell
	const FVector2D Sl = WaveSlope(X, Z, T, Amp);
	const double Along = Sl.X * Fx + Sl.Y * Fz, BeamSl = Sl.X * Rx + Sl.Y * Rz;
	if (!bAir)
	{
		Speed -= Along * 9.8 * .5 * Dt;
		DVX -= Sl.X * 9.8 * .22 * Dt;
		DVZ -= Sl.Y * 9.8 * .22 * Dt;
		if (F < .25) H += BeamSl * .35 * Dt * (1 - F * 4);   // a slow boat gets nudged beam-to by the swell
	}
	DVX *= 1 - FMath::Min(1.0, Dt * .7);
	DVZ *= 1 - FMath::Min(1.0, Dt * .7);
	X += DVX * Dt;
	Z += DVZ * Dt;

	// buoyancy: 15 points under the hull each push up by how deep they sit in the water.
	// Their sum lifts the boat (heave), their front/back imbalance pitches it, and their side-to-side imbalance rolls it.
	const double Dk = (.45 + S.Len * .035) * (S.bCat ? .85 : 1);
	const double Ride = Dk * (1 - .55 * FMath::Min(1.0, F * 1.6) * (.8 + .4 * TrimV));
	static const double NXs[3] = {-.42, 0, .42};
	static const double NZs[5] = {-.46, -.22, .02, .26, .46};
	const double K = 9.8 / (15 * Dk), Ip = S.Len * S.Len / 12 * 1.15, Ir = S.Beam * S.Beam / 12 * (S.bCat ? 2.4 : 1.7);
	double Sz2 = 0, Sx2 = 0;
	for (double A : NZs) Sz2 += 3 * FMath::Square(A * S.Len);
	for (double A : NXs) Sx2 += 5 * FMath::Square(A * S.Beam);
	const double WH = FMath::Sqrt(9.8 / Dk), WP = FMath::Sqrt(K * Sz2 / Ip), WR = FMath::Sqrt(K * Sx2 / Ir), Zeta = .3;
	int32 Wet = 0;
	const int32 Sub = 3;
	const double HDt = Dt / Sub;
	for (int32 It = 0; It < Sub; It++)
	{
		double Fy = 0, Tp = 0, Tr = 0;
		Wet = 0;
		const double Spn = FMath::Sin(Pitch), Srn = FMath::Sin(Roll), Tt = T - Dt + HDt * (It + 1);
		for (double Ax : NXs)
			for (double Az : NZs)
			{
				const double Px = Ax * S.Beam, Pz = Az * S.Len;
				const double Wx = X + Fx * (-Pz) + Rx * Px, Wz = Z + Fz * (-Pz) + Rz * Px;
				const double Yb = Y + (-Pz) * Spn + Px * Srn - Ride;
				const double Sbm = Clamp(WaveH(Wx, Wz, Tt, Amp) - Yb, 0, Dk * 2.4);
				if (Sbm > 0) Wet++;
				const double Fi = K * Sbm;
				Fy += Fi;
				Tp += Fi * (-Pz);
				Tr += Fi * Px;
			}
		const double InW = Wet > 0 ? 1 : .15;
		VY += (Fy - 9.8 - 2 * Zeta * WH * VY * InW) * HDt;
		Y += VY * HDt;
		VP += (Tp / Ip + Plane * WP * WP * (Wet ? 1 : 0) - (Wet ? 0 : .5) - 2 * Zeta * WP * VP * InW) * HDt;
		Pitch += VP * HDt;
		VR += (Tr / Ir + Steer * F * (S.bCat ? .05 : .16) * WR * WR * (Wet ? 1 : 0) - 2 * Zeta * WR * VR * InW) * HDt;
		Roll += VR * HDt;
	}
	Pitch = Clamp(Pitch, -.6, .75);
	Roll = Clamp(Roll, -.7, .7);
	const bool bWasAir = bAir;
	bAir = Wet == 0;
	if (bWasAir && !bAir && VY < -2.2) Slam(VY);
	if (D < S.Draft && Y < -.15) { Y = -.15; VY = FMath::Max(0.0, VY); }
	RideNow = Ride;
	DkNow = Dk;

	WakeT += Dt;
	// rings every 0.08 s instead of 0.35 s: the same wake height, but a smooth V instead of scalloped rings
	if (WakeT > .08 && FMath::Abs(Speed) > 2) { EmitWake(X - Fx * S.Len * .5, Z - Fz * S.Len * .5, FMath::Abs(Speed), S.Len, T, WakeT); WakeT = 0; }
	// the white prop wash: wider and whiter as she gets on plane, none while the hull is airborne
	TrailT += Dt;
	if (TrailT > .07)
	{
		TrailT = 0;
		const double V = FMath::Abs(Speed);
		const double Wash = bAir ? 0 : FMath::SmoothStep(2.0, 9.0, V) * (.55 + .45 * FMath::Min(1.0, F * 1.5));   // little white water off a displacement-speed hull (trolling)
		EmitTrail(0, X - Fx * S.Len * .48, Z - Fz * S.Len * .48, S.Beam * (.32 + .3 * FMath::Min(1.0, F * 1.4)), Wash, T);
	}

	// pose the hull: bow up for positive pitch, starboard up for positive roll, lifted as she gets on plane
	const FVector Fw(Fx, Fz, 0), Rt(Rx, Rz, 0), Up(0, 0, 1);
	const FVector Fp = Fw * FMath::Cos(Pitch) + Up * FMath::Sin(Pitch);
	const FVector Rr = Rt * FMath::Cos(Roll) + Up * FMath::Sin(Roll);
	const FRotator Rot = FRotationMatrix::MakeFromXY(Fp, Rr).Rotator();
	SetActorLocationAndRotation(ToUE(X, Y + (Dk - Ride) * 1.6, Z), Rot);
}

// ================================================================ camera
// where you stand to fish: the starboard side of the cockpit, a little aft (boat-local meters, deck height)
FVector AKeysBoatPawn::FishSpotLocal() const
{
	const FBoatSpec& S = Spec();
	if (HookedStation >= 0)
	{
		// a trolled fish is astern: fight it from the transom, on the side it is on
		const double Sgn = ((FX - X) * FMath::Cos(H) + (FZ - Z) * -FMath::Sin(H)) >= 0 ? 1 : -1;
		return FVector(-S.Len * .36, Sgn * FMath::Min(.6, S.Beam * .2), KeysMesh::SoleAt(S, -S.Len * .36) + .03);
	}
	return FVector(-S.Len * .15, FMath::Max(.3, KeysMesh::InnerHalfBeam(S, -S.Len * .15) - .4), KeysMesh::SoleAt(S, -S.Len * .15) + .03);
}

// leaving a first-person view: start the chase camera from where the eye was, so it eases back out
void AKeysBoatPawn::SyncChaseCam()
{
	const FVector L = Camera->GetComponentLocation() / 100.0;
	CamPos = FVector(L.X, L.Z, L.Y);
	bCamInit = true;
}

void AKeysBoatPawn::UpdateCamera(double Dt, double T)
{
	if (IsDiving() || IsAshore()) { bExtCam = false; return; }   // the diver / walker actor places the camera (ApplyDiveCamera)
	{
		FVector EP; FRotator ER; float EF;
		bExtCam = ExternalCamera(EP, ER, EF);
		if (bExtCam) { Camera->SetWorldLocationAndRotation(EP, ER); Camera->SetFieldOfView(EF); return; }
	}
	const FBoatSpec& S = Spec();

	// first person while fishing (V), and the helm view (1) while driving
	const bool bWantFp = bRunning && bFpPref && IsFishingActive();
	if (bWantFp != bFpOn)
	{
		bFpOn = bWantFp;
		if (bFpOn)
		{
			FpEyeL = FishSpotLocal();
			FpYaw = IsLineOut() ? FMath::Atan2(-(Bob.X - X), -(Bob.Z - Z)) : H - PI / 2;
			FpPitch = -.12;
			FpUserT = 0;
		}
		else SyncChaseCam();
	}
	const bool bWantDrive = bRunning && View > 0 && !bFpOn && FState != EKeysFish::Caught;
	if (bWantDrive != bDriveOn)
	{
		bDriveOn = bWantDrive;
		DYaw = DPitch = 0;
		if (!bDriveOn && !bFpOn) SyncChaseCam();
	}
	Shake = FMath::Max(0.0, Shake - Dt * 2.5);
	const FTransform BT = GetActorTransform();

	if (bFpOn)
	{
		FpEyeL = FMath::Lerp(FpEyeL, FishSpotLocal(), FMath::Min(1.0, Dt * 3));
		const FVector EyeU = BT.TransformPosition((FpEyeL + FVector(0, 0, 1.62)) * 100.0);
		const FVector Eye(EyeU.X / 100.0, EyeU.Z / 100.0, EyeU.Y / 100.0);   // game meters
		// look at the float, or the fish once it's on, unless you just moved the view yourself
		bool bHasTgt = false;
		FVector Tgt = FVector::ZeroVector;
		if (FState == EKeysFish::Fight) { Tgt = FVector(FX, 0, FZ); bHasTgt = true; }
		else if (IsLineOut()) { Tgt = Bob; bHasTgt = true; }
		if (bHasTgt && FpUserT <= 0 && !bCharging)
		{
			const double DX = Tgt.X - Eye.X, DZ = Tgt.Z - Eye.Z, HD = FMath::Max(1e-3, Hyp(DX, DZ));
			FpYaw = PawnAngLerp(FpYaw, FMath::Atan2(-DX, -DZ), Dt * 2.5);
			FpPitch = Lerp(FpPitch, Clamp(FMath::Atan2(Tgt.Y - Eye.Y, HD) + .06, -.7, .3), FMath::Min(1.0, Dt * 2.5));
		}
		FpUserT -= Dt;
		// steady or unsteady hands (sim/steady.ts handWobble): a slow drift, much bigger after a few drinks
		const double Sw = Sway(), Idle = .006 / FMath::Square(FMath::Clamp(SteadyMul(), .5, 2.0));
		const double Pp = FpPitch + (FMath::FRand() - .5) * Shake * .03 + FMath::Sin(T * 1.7) * Idle + FMath::Sin(T * .9 + 1) * .03 * Sw;
		FpYaw += FMath::Sin(T * .5 + 2) * .045 * Sw * Dt;
		// game yaw: forward is (-sin, -cos) in x/z, which is Unreal X/Y
		const FVector Dir(-FMath::Sin(FpYaw) * FMath::Cos(Pp), -FMath::Cos(FpYaw) * FMath::Cos(Pp), FMath::Sin(Pp));
		Camera->SetWorldLocationAndRotation(EyeU, Dir.Rotation());
		return;
	}
	if (bDriveOn)
	{
		// standing at the wheel behind the console; the horizon tilts with the hull
		const double MidX = .05 * S.Len, DeckZ = KeysMesh::SoleAt(S, MidX - .7);
		FVector Eye(MidX - .7, 0, DeckZ + 1.66), TowerEye;
		if (View == 2 && KeysMesh::TowerEye(S, TowerEye)) Eye = TowerEye;   // up at the tuna tower helm
		FVector P = BT.TransformPosition(Eye * 100.0);
		if (!bDragging) { DYaw *= 1 - FMath::Min(1.0, Dt * 1.2); DPitch *= 1 - FMath::Min(1.0, Dt * 1.2); }
		const FQuat Look = FRotator(FMath::RadiansToDegrees(DPitch - .2), -FMath::RadiansToDegrees(DYaw), 0).Quaternion();
		if (Shake > 0) P += FVector(FMath::FRand() - .5, FMath::FRand() - .5, FMath::FRand() - .5) * Shake * 15;
		Camera->SetWorldLocationAndRotation(P, (BT.GetRotation() * Look).Rotator());
		return;
	}

	FVector WantPos, WantLook;
	if (FState == EKeysFish::Caught)
	{
		// a hero shot of the catch held up over the gunwale
		const double Rx = FMath::Cos(H), Rz = -FMath::Sin(H), Fx = -FMath::Sin(H), Fz = -FMath::Cos(H);
		const double Back = 2.4 + FishLen * 1.1;
		WantPos = FVector(ShowPos.X + Rx * ShowSgn * Back + Fx * .9, ShowPos.Y + .5, ShowPos.Z + Rz * ShowSgn * Back + Fz * .9);
		// aim a little to the fish's right so it sits left of center, clear of the catch card
		WantLook = ShowPos + FVector(Fx, 0, Fz) * ShowSgn * Back * .32;
		// hold it up side-on to the lens, nose high, kicking a little
		const double Age = T - CaughtT;
		const double Kick = FMath::Sin(Age * 5.3) * .14 * FMath::Exp(-Age * .35) + FMath::Sin(Age * 1.7) * .04;
		const double ToCam = FMath::Atan2(CamPos.Z - ShowPos.Z, CamPos.X - ShowPos.X);
		FishMesh->SetWorldLocationAndRotation(ToUE(ShowPos.X, ShowPos.Y + FMath::Sin(Age * 2.1) * .03, ShowPos.Z),
			FRotator(80, FMath::RadiansToDegrees(ToCam + Kick) - 90, FMath::RadiansToDegrees(Kick * .6)));
	}
	else
	{
		const double Dist = (S.Len * 1.5 + 11) * CamZoom, Ht = (S.Len * .32 + 4.2) * CamZoom * (1 + CamPitchOff);
		if (!bDragging && FMath::Abs(Speed) > 5) CamYaw *= 1 - FMath::Min(1.0, Dt * 1.2);
		const double A = H + CamYaw;
		WantPos = FVector(X + FMath::Sin(A) * Dist, Y + Ht, Z + FMath::Cos(A) * Dist);
		if (!bRunning)
		{
			const double O = T * .08;
			WantPos = FVector(X + FMath::Sin(O) * Dist * 1.3, Y + Ht * 1.2, Z + FMath::Cos(O) * Dist * 1.3);
		}
		double LX = X - FMath::Sin(H) * 4, LZ = Z - FMath::Cos(H) * 4;
		if (FState == EKeysFish::Fight) { LX = Lerp(X, FX, .35); LZ = Lerp(Z, FZ, .35); }
		else if (FState != EKeysFish::Idle) { LX = Lerp(X, Bob.X, .35); LZ = Lerp(Z, Bob.Z, .35); }
		WantLook = FVector(LX, Y + 1.8, LZ);
		if (RaiseCamT > 0 && bRunning && !bDragging)
		{
			// a fish coming up on the spread: the camera swings round over the console to look back down the wake at it
			const FVector Tgt = !PendKey.IsEmpty() ? RaisePos : FVector(FX, 0, FZ);
			const double DX = Tgt.X - X, DZ = Tgt.Z - Z, DL = FMath::Max(1.0, Hyp(DX, DZ));
			FVector TowerEye;
			const double Up = KeysMesh::TowerEye(S, TowerEye) ? TowerEye.Z + 3.5 : 7.5;   // clear of the tower and the top
			WantPos = FVector(X - DX / DL * (S.Len * .5 + 9), Y + Up, Z - DZ / DL * (S.Len * .5 + 9));
			WantLook = FVector(Lerp(X, Tgt.X, .75), 0, Lerp(Z, Tgt.Z, .75));
		}
	}
	if (!bCamInit) { CamPos = WantPos; CamLook = WantLook; bCamInit = true; }
	CamPos = FMath::Lerp(CamPos, WantPos, 1 - FMath::Exp(-Dt * 4));
	CamLook = FMath::Lerp(CamLook, WantLook, 1 - FMath::Exp(-Dt * 5));
	// keep the lens out of the water
	const double Surf = WaveH(CamPos.X, CamPos.Z, T, AmpAt(CamPos.X, CamPos.Z), false);
	if (CamPos.Y < Surf + .6) CamPos.Y = Surf + .6;
	FVector P = ToUE(CamPos.X, CamPos.Y, CamPos.Z);
	if (Shake > 0) P += FVector(FMath::FRand() - .5, FMath::FRand() - .5, FMath::FRand() - .5) * Shake * 60;
	const FVector L = ToUE(CamLook.X, CamLook.Y, CamLook.Z);
	Camera->SetWorldLocationAndRotation(P, (L - P).Rotation());
}

// ================================================================ fishing
void AKeysBoatPawn::StartCharge()
{
	if (FMath::Abs(Speed) > 1.6) { ShowToast(TEXT("Slow under 3 knots to cast.")); return; }
	bCharging = true;
	Charge = 0;
	ChargeT = 0;
}

FVector AKeysBoatPawn::RodTip(const FVector& Target) const
{
	// the rod comes out over whichever gunwale faces the target, a little aft of the console
	const FBoatSpec& S = Spec();
	const double Rx = FMath::Cos(H), Rz = -FMath::Sin(H), Fx = -FMath::Sin(H), Fz = -FMath::Cos(H);
	const double Sgn = ((Target.X - X) * Rx + (Target.Z - Z) * Rz) >= 0 ? 1 : -1;
	const double TX = Target.X - X, TZ = Target.Z - Z, TL = FMath::Max(1.0, Hyp(TX, TZ));
	return FVector(X + Rx * Sgn * (S.Beam * .5 + .5) - Fx * S.Len * .15 + TX / TL * 1.2, Y + 3.0, Z + Rz * Sgn * (S.Beam * .5 + .5) - Fz * S.Len * .15 + TZ / TL * 1.2);
}

void AKeysBoatPawn::CastLine(double Power)
{
	if (AKeysAudio* A = AKeysAudio::Get(this)) A->Cast(Power);
	const FVector CF = Camera->GetForwardVector();
	FVector2D Dir(CF.X, CF.Y);
	Dir = Dir.GetSafeNormal();
	if (Dir.IsNearlyZero()) Dir = FVector2D(-FMath::Sin(H), -FMath::Cos(H));
	double Dist = 12 + Power * 73;
	for (int32 k = 0; k < 8 && LandH(X + Dir.X * Dist, Z + Dir.Y * Dist) > -.4; k++) Dist *= .8;
	CastTo = FVector(X + Dir.X * Dist, 0, Z + Dir.Y * Dist);
	if (BaitCasts > 0) BaitCasts--;
	CastFrom = RodTip(CastTo);
	CastT = 0;
	CastDur = .5 + Power * .9;
	CastH = 3 + Power * 14;
	FState = EKeysFish::Casting;
	Key.Reset();
	Sp = nullptr;
	Bob = CastFrom;
	VmWhip = 1;
}

void AKeysBoatPawn::ReelIn(const FString& Msg)
{
	CurTag = -1;
	ShowFishMesh(false);
	FState = EKeysFish::Idle;
	Key.Reset();
	Sp = nullptr;
	bCharging = false;
	bStrikeShown = false;
	bJumping = false;
	if (!Msg.IsEmpty()) ShowToast(Msg);
}

FString AKeysBoatPawn::PickSpecies() const
{
	// a bird carcass on the water: the toothy end of the food chain comes to it (sim/chum.ts SCAVENGER_KEYS)
	if (const AKeysHandgun* Gun = AKeysHandgun::Get(this); Gun && Gun->NearCarcass(Bob.X, Bob.Z) && FMath::FRand() < .55)
	{
		TArray<FString> Sc;
		for (const FZoneRow& R : AKeysLife::FishTable(Zone)) if (AKeysHandgun::IsScavenger(R.Key)) Sc.Add(R.Key);
		if (!Sc.Num()) Sc = {TEXT("barracuda"), TEXT("blacktip"), TEXT("jackcrevalle")};
		const FString K = Sc[FMath::RandRange(0, Sc.Num() - 1)];
		if (Species(K)) return K;
	}
	if (const AKeysLife* L = AKeysLife::Get(this)) return L->PickSpecies(Spot, Bob.X, Bob.Z);
	const FHump* Hump = NearHump(Bob.X, Bob.Z);
	const TArray<FZoneRow>& Tbl = ZoneTable(Zone);
	const double Of = Zone == TEXT("Offshore") ? OffshoreF(Bob.X, Bob.Z) : 0;
	auto Big = [](const FString& K) { return K == TEXT("marlin") || K == TEXT("swordfish") || K == TEXT("bluefin") ? 1.0 : K == TEXT("yellowfin") ? .6 : K == TEXT("wahoo") ? .5 : K == TEXT("sailfish") ? .3 : 0.0; };
	TArray<double> W;
	double Tot = 0;
	for (const FZoneRow& R : Tbl)
	{
		double V = R.W;
		const double B = Big(R.Key);
		V *= B > 0 ? 1 + 4 * Of * B : 1 - .45 * Of;
		if (Hump && (R.Key == TEXT("amberjack") || R.Key == TEXT("blackfin") || R.Key == TEXT("kingfish"))) V *= 4;
		Tot += V;
		W.Add(V);
	}
	double Rr = FMath::FRand() * Tot;
	for (int32 i = 0; i < Tbl.Num(); i++) { Rr -= W[i]; if (Rr <= 0) return Tbl[i].Key; }
	return Tbl[0].Key;
}

void AKeysBoatPawn::ChooseFish()
{
	Key = PickSpecies();
	Sp = Species(Key);
	const double Of = OffshoreF(Bob.X, Bob.Z);
	Weight = FMath::RoundToDouble((Sp->Min + (Sp->Max - Sp->Min) * FMath::Min(1.0, FMath::Pow(FMath::FRand(), 1.8 - 1.2 * Of) * AKeysLife::WeightBoost(Spot) * (1 + .3 * Of))) * 10) / 10;
	const double A = FMath::FRand() * 2 * PI;
	FX = Bob.X + FMath::Cos(A) * 11;
	FZ = Bob.Z + FMath::Sin(A) * 11;
	OFX = FMath::Cos(A) * 1.3;
	OFZ = FMath::Sin(A) * 1.3;
	PickRecapture();   // a tagged fish in range may be this one, at its own grown weight (fishing/update.ts)
	BuildHookedMesh();
}

void AKeysBoatPawn::BuildHookedMesh()
{
	FishLen = KeysMesh::FishLength(Weight);
	FKeysMesh M;
	// the species' own model from the living-sea port, else the generic fish
	const double Scale = 1.35 * FMath::Clamp(FMath::Pow(Weight / ((Sp->Min + Sp->Max) / 2), 1.0 / 3), .75, 1.5);
	if (!AKeysLife::BuildCreatureMesh(Key, M, Scale))
		KeysMesh::BuildFish(M, FishLen, Sp->Color, IsBillfish(Key) || Key == TEXT("swordfish"));
	M.SmoothNormals();
	M.Commit(FishMesh, 0, AKeysWorld::VertexColorMaterial());
}

void AKeysBoatPawn::MissFish(const FString& Msg)
{
	bStrikeShown = false;
	ShowFishMesh(false);
	Key.Reset();
	Sp = nullptr;
	FState = EKeysFish::Waiting;
	BiteT = Rand(4, 8) * AKeysLife::MissBiteScale(Spot);
	ShowToast(Msg);
}

void AKeysBoatPawn::SetHook()
{
	bStrikeShown = false;
	if (!Sp) ChooseFish();
	if (AKeysAudio* A = AKeysAudio::Get(this)) A->Splash(FX, FZ, 30, 1.5);
	const double SizeT = (Weight - Sp->Min) / (Sp->Max - Sp->Min);
	Str = Sp->Str * (.8 + .55 * SizeT);
	MaxLine = Sp->Str > 1.2 ? 320 : 200;
	Tension = .55; Stamina = 1; bRunningFish = true; RunT = Rand(2, 4); RunAng = Rand(-.7, .7); Pull = Str * 1.05; SlackT = 0; OverT = 0;
	JumpQ = 0; Deep = 0; DeepT = 0; ShakeT = Rand(.6, 1.4);
	FState = EKeysFish::Fight;
	FightLabel = SizeT > .7 ? TEXT("Something heavy is on") : SizeT > .35 ? TEXT("Solid fish on") : TEXT("Fish on");
	FightSub = TEXT("It's running! Hold S (or Space) to reel, ease off when the bar runs red.");
	Shake = FMath::Max(Shake, .3);
	ShowFishMesh(true);
	if (Sp->bJump && FMath::FRand() < .85) JumpQ = IsBillfish(Key) ? 2 + FMath::RandRange(0, 2) : 1;
	else if (Key == TEXT("kingfish") && FMath::FRand() < .6) JumpQ = 1;
	else if (IsSounder(Key)) DeepT = Rand(3, 7);
}

void AKeysBoatPawn::StartJump(bool bLow)
{
	const double Ang = FMath::Atan2(FZ - Z, FX - X) + Rand(-1.2, 1.2), Lf = FishLen;
	bJumping = true; bJumpLow = bLow; JT = 0;
	JDur = bLow ? .55 + Lf * .08 : 1.1 + Lf * .18;
	JX = FX; JZ = FZ; JDX = FMath::Cos(Ang); JDZ = FMath::Sin(Ang);
	JRun = bLow ? Lf * 1.6 : 3 + Lf;
	JH = bLow ? .5 + Lf * .22 : 1.6 + Lf * .9;
	JSpin = Rand(-1, 1);
	if (AKeysAudio* A = AKeysAudio::Get(this)) A->Splash(FX, FZ, bLow ? 16 : 34, bLow ? 1.1 : 1.7);
	// white water where it explodes out
	if (AKeysLife* L = AKeysLife::Get(this)) L->Splash(FX, FZ, bLow ? 26 : 48 + FMath::RoundToInt(FishLen * 10), bLow ? 1.1 : 1.6 + FishLen * .2);
	Shake = FMath::Max(Shake, .18);
	DeepT = 0;
	FightLabel = bLow ? TEXT("It's tail-walking!") : TEXT("It jumped!");
	FightSub = TEXT("Bow to the fish - ease off the reel while it's airborne.");
}

void AKeysBoatPawn::UpdateJump(double Dt)
{
	if (!bJumping) return;
	JT += Dt;
	const double P = JT / JDur;
	if (P >= 1)
	{
		FX = JX + JDX * JRun; FZ = JZ + JDZ * JRun; bJumping = false;
		if (AKeysAudio* A = AKeysAudio::Get(this)) A->Splash(FX, FZ, bJumpLow ? 18 : 30, bJumpLow ? 1.1 : 1.6);
		// and a bigger crash on the way back in
		if (AKeysLife* L = AKeysLife::Get(this)) L->Splash(FX, FZ, bJumpLow ? 30 : 60 + FMath::RoundToInt(FishLen * 14), bJumpLow ? 1.2 : 1.9 + FishLen * .25);
		return;
	}
	const double PX = JX + JDX * JRun * P, PZ = JZ + JDZ * JRun * P, PY = -.5 + 4 * JH * P * (1 - P);
	// a tail-walk throws spray the whole way
	if (bJumpLow && PY < .7 && FMath::FRand() < Dt * 14)
		if (AKeysLife* L = AKeysLife::Get(this)) L->Splash(PX, PZ, 5, .7);
	// nose up on the way out, nose down on the way back in, with a head-shake wobble
	const double Yaw = FMath::Atan2(JDZ, JDX);
	const FRotator R(FMath::RadiansToDegrees((1 - 2 * P) * (bJumpLow ? .5 : 1.1)), FMath::RadiansToDegrees(Yaw + FMath::Sin(P * 9) * .25), FMath::RadiansToDegrees(FMath::Sin(P * 22) * .55 + JSpin * P * 1.4));
	FishMesh->SetWorldLocationAndRotation(ToUE(PX, PY, PZ), R);
}

void AKeysBoatPawn::Lose(const FString& Msg)
{
	if (Msg.StartsWith(TEXT("Snap")))
		if (AKeysAudio* A = AKeysAudio::Get(this)) A->Snap();
	ReelIn(FString());
	ShowToast(Msg);
}

void AKeysBoatPawn::Land()
{
	if (AKeysLife* L = AKeysLife::Get(this)) L->OnCatchLanded(Spot);
	if (AKeysAudio* A = AKeysAudio::Get(this)) A->Land();
	{
		// a fluorocarbon leader did its job on a toothy fish: it is used up
		static const TSet<FString> Toothy = {TEXT("barracuda"), TEXT("kingfish"), TEXT("wahoo"), TEXT("cero"), TEXT("blacktip"), TEXT("bullshark"), TEXT("lemonshark"), TEXT("tigershark"), TEXT("hammerhead"), TEXT("bluefish")};
		if (Toothy.Contains(Key) && UseOne(TEXT("leader.fluoro"))) SaveEconomy();
	}
	FState = EKeysFish::Caught;
	bJumping = false;
	const FSpecies& S = *Sp;
	int32 Pts = FMath::RoundToInt(Weight * S.Mult);
	Count++;
	Score += Pts;
	CaughtKeys.Add(Key);
	FString Bonus;
	if (!bSlam1 && CaughtKeys.Contains(TEXT("tarpon")) && CaughtKeys.Contains(TEXT("bonefish")) && CaughtKeys.Contains(TEXT("permit")))
	{ bSlam1 = true; Score += 1000; Bonus = TEXT("Inshore grand slam - +1,000"); }
	if (!bSlam2 && CaughtKeys.Contains(TEXT("sailfish")) && CaughtKeys.Contains(TEXT("mahi")) && CaughtKeys.Contains(TEXT("wahoo")))
	{ bSlam2 = true; Score += 1500; Bonus += (Bonus.IsEmpty() ? TEXT("") : TEXT(" - ")) + FString(TEXT("Blue water slam - +1,500")); }
	if (Save) { Save->Record(Key, Weight, Spec().Brand + TEXT(" ") + Spec().Name); Save->RecordScore(Score); }   // lbRecord
	if (AKeysOnline* O = AKeysOnline::Get(this)) O->ReportCatch(Key, Weight, Spec().Brand + TEXT(" ") + Spec().Name, Score);

	const double SizeT = (Weight - S.Min) / (S.Max - S.Min);
	bool bFemale;
	if (Key == TEXT("marlin") || Key == TEXT("blackmarlin") || Key == TEXT("sailfish") || Key == TEXT("swordfish") || Key == TEXT("tarpon") || Key == TEXT("permit") || Key == TEXT("snook"))
		bFemale = FMath::FRand() < .45 + SizeT * .5;
	else if (Key == TEXT("grouper") || Key == TEXT("hogfish")) bFemale = FMath::FRand() > .2 + SizeT * .7;
	else bFemale = FMath::FRand() < .5;
	CardSex = Key == TEXT("mahi") ? (bFemale ? TEXT("Cow") : TEXT("Bull")) : (bFemale ? TEXT("Female") : TEXT("Male"));
	CardName = S.Name;
	CardWeight = Weight;
	CardPts = Pts;
	CardInches = FMath::RoundToInt(FishLen * 1.1 * 39.37);
	CardFact = S.Fact;
	CardBonus = Bonus;
	CardZone = ZoneDesc(Zone) + TEXT(" - ") + NearestName(FX, FZ);
	LastPts = Pts;
	const double Left = Spec().CoolerCap - CoolerLb();
	const FString CR = ReleaseOnlyReason(Key);
	bCanKeep = CR.IsEmpty() && Weight <= Left;
	CardNote = !CR.IsEmpty() ? CR : Weight > Left ? FString::Printf(TEXT("Too big for your cooler - %d lb of space left. Weigh in at a marina to empty it."), FMath::Max(0, FMath::RoundToInt32(Left)))
		: FString(TEXT("Release for a 25% conservation bonus, tag it for 40%, or keep it and weigh in at a marina dock."));
	CaughtT = GetWorld()->GetTimeSeconds();
	bCardSpeared = false;
	SetCardTag();
	if (Weight > RunBest.Weight) RunBest = {Key, Weight, Count, false, Spec().Brand + TEXT(" ") + Spec().Name};

	// hold it up over the gunwale for the photo
	const double Rx = FMath::Cos(H), Rz = -FMath::Sin(H);
	const double Sgn = ((FX - X) * Rx + (FZ - Z) * Rz) >= 0 ? 1 : -1;
	ShowSgn = Sgn;
	ShowPos = FVector(X + Rx * Sgn * (Spec().Beam * .5 + .35), Y + 1.9, Z + Rz * Sgn * (Spec().Beam * .5 + .35));
	FishMesh->SetWorldLocationAndRotation(ToUE(ShowPos.X, ShowPos.Y, ShowPos.Z), FRotator(78, FMath::RadiansToDegrees(H), 0));
	ShowFishMesh(true);
}

void AKeysBoatPawn::KeepFish()
{
	if (FState != EKeysFish::Caught || !bCanKeep) return;
	Cooler.Add({Sp->Name, Weight, int32(LastPts), Key});
	if (AKeysEvents* Ev = AKeysEvents::Get(this)) Ev->OnKeep(Key, Weight);   // a tournament counts what you bring in
	ShowToast(FString::Printf(TEXT("%s is on ice. %d fish in the cooler."), *Sp->Name, Cooler.Num()));
	if (CurTag >= 0) ReportTag(true);
	FinishCatch();
}

void AKeysBoatPawn::ReleaseFish()
{
	if (FState != EKeysFish::Caught) return;
	const bool bTagged = CurTag >= 0;
	const int32 Bonus = FMath::RoundToInt(LastPts * (bTagged ? .6 : .25));
	Score += Bonus;
	if (RunBest.Id == Count) RunBest.bReleased = true;
	if (bTagged) { const FString Code = Tags[CurTag].Code; ReportTag(false); ShowToast(FString::Printf(TEXT("Released %s again! +%d tag-report bonus. Recapture logged - %d landings on record."), *Code, Bonus, Tags.IsValidIndex(CurTag) ? Tags[CurTag].Events.Num() : 0), 5); }
	else ShowToast(FString::Printf(TEXT("Released! +%d conservation bonus."), Bonus));
	if (Save) Save->RecordScore(Score);
	if (AKeysOnline* O = AKeysOnline::Get(this)) O->ReportScore(Score);
	FinishCatch();
}

void AKeysBoatPawn::FinishCatch()
{
	CurTag = -1;
	CardTagHead.Reset();
	CardTagLines.Reset();
	bCardSpeared = false;
	ShowFishMesh(false);
	FState = EKeysFish::Idle;
	Key.Reset();
	Sp = nullptr;
}

void AKeysBoatPawn::WeighIn()
{
	const FMarina* M = NearMarina(X, Z);
	if (!M) return;
	if (!Cooler.Num()) { ShowToast(TEXT("Your cooler is empty - go catch something to weigh in.")); return; }
	const double Lb = CoolerLb();
	const int32 Bonus = FMath::RoundToInt(Lb * 6);
	const FKeysCooler* BigF = &Cooler[0];
	for (const FKeysCooler& C : Cooler) if (C.Weight > BigF->Weight) BigF = &C;
	Score += Bonus;
	// the fillets go to the fish house at the dock price (content/economy.ts MEAT_PRICE); game fish and billfish don't sell
	double Sale = 0;
	for (const FKeysCooler& C : Cooler) Sale += C.Weight * .45 * KeysTown::MeatPrice(C.Key);
	const int64 Paid = FMath::RoundToInt64(Sale);
	Cash += Paid;
	SaveEconomy();
	ShowToast(FString::Printf(TEXT("Weighed in %d fish (%d lb) at %s. Top fish: %s, %.1f lb. +%s points%s"), Cooler.Num(), FMath::RoundToInt(Lb), *M->Name, *BigF->Name, BigF->Weight, *Commas(Bonus),
		Paid > 0 ? *FString::Printf(TEXT(", and the fish house paid $%s for the fillets."), *Commas(Paid)) : TEXT("!")), 6);
	Cooler.Reset();
	if (Save) Save->RecordScore(Score);
	if (AKeysOnline* O = AKeysOnline::Get(this)) O->ReportScore(Score);
}

void AKeysBoatPawn::OnAction()
{
	switch (FState)
	{
		case EKeysFish::Idle: StartCharge(); break;
		case EKeysFish::Nibble: MissFish(TEXT("Too early! You pulled the bait away from the fish.")); break;
		case EKeysFish::Bite: SetHook(); break;
		case EKeysFish::Caught: if (GetWorld()->GetTimeSeconds() - CaughtT > 1.6) ReleaseFish(); break;   // ignore the reeling taps still coming in as the fish lands
		case EKeysFish::Waiting: ShowToast(TEXT("Wait for the float to go under, then strike. Hold S to retrieve, R to reel in.")); break;
		default: break;
	}
}

void AKeysBoatPawn::ShowFishMesh(bool bShow) { FishMesh->SetVisibility(bShow); }

void AKeysBoatPawn::UpdateFishing(double Dt, double T)
{
	if (bCharging) { ChargeT += Dt; Charge = FMath::Min(1.0, ChargeT / 1.3); }
	if (AKeysLife* L = AKeysLife::Get(this))
	{
		// tuna schools hunt down a bait left soaking offshore (JS 2520-2528, 2552)
		const bool bBaitOut = FState == EKeysFish::Waiting && !Sp && Zone == TEXT("Offshore");
		L->SetBait(bBaitOut, Bob.X, Bob.Z);
		if (bBaitOut)
		{
			if (L->TunaClosing()) BiteT = FMath::Max(BiteT, 5.0);
			FKeysTunaStrike TS;
			if (L->PollTunaStrike(TS) && (Sp = Species(TS.Key)) != nullptr)
			{
				Key = TS.Key;
				Weight = AKeysLife::TunaWeight(*Sp, Bob.X, Bob.Z);
				FX = TS.FX; FZ = TS.FZ; OFX = OFZ = 0;
				BuildHookedMesh();
				FState = EKeysFish::Bite; Win = StrikeWindow(1.6); bStrikeShown = true; Shake = FMath::Max(Shake, .3);
				if (AKeysAudio* A = AKeysAudio::Get(this)) { A->Strike(Bob.X, Bob.Z); A->Splash(TS.FX, TS.FZ, 30, 1.5); }
			}
		}
	}
	if (FState == EKeysFish::Idle || FState == EKeysFish::Caught) return;
	const bool bReel = bBack || (bAction && FState == EKeysFish::Fight);

	if (FState == EKeysFish::Casting)
	{
		CastT += Dt / CastDur;
		const double P = FMath::Min(1.0, CastT);
		Bob = FVector(Lerp(CastFrom.X, CastTo.X, P), Lerp(CastFrom.Y, 0, P) + CastH * 4 * P * (1 - P), Lerp(CastFrom.Z, CastTo.Z, P));
		if (P >= 1)
		{
			FState = EKeysFish::Waiting;
			if (AKeysAudio* A = AKeysAudio::Get(this)) A->Splash(Bob.X, Bob.Z, 8, .6);
			Spot = FKeysFishSpot();
			Spot.Zone = ZoneAt(Bob.X, Bob.Z);
			AKeysLife* L = AKeysLife::Get(this);
			if (L) Spot = L->SpotAt(Bob.X, Bob.Z);
			Zone = Spot.Zone;
			BiteT = Rand(4, 10) * AKeysLife::BiteTimeScale(Spot) * (BaitCasts > 0 ? .6 : 1);
			Key.Reset();
			Sp = nullptr;
			if (L) L->OnCastLanded(Spot, Bob.X, Bob.Z);
			ShowToast(FString::Printf(TEXT("%s%d m cast into the %s."), Spot.bHot ? TEXT("Right in the school - ") : TEXT(""), FMath::RoundToInt(Hyp(Bob.X - X, Bob.Z - Z)), *ZoneDesc(Zone).ToLower()));
		}
	}
	else if (FState == EKeysFish::Waiting || FState == EKeysFish::Nibble || FState == EKeysFish::Bite)
	{
		const double Surf = WaveH(Bob.X, Bob.Z, T, AmpAt(Bob.X, Bob.Z));
		const double BD = FMath::Max(1.0, Hyp(Bob.X - X, Bob.Z - Z));
		if (BD > 140) { ReelIn(TEXT("Drifted too far - line reeled in.")); return; }
		if (FMath::Abs(Speed) > 3.2) { ReelIn(TEXT("Line reeled in so you can run.")); return; }
		if (bReel && FState == EKeysFish::Waiting)
		{
			Bob.X += (X - Bob.X) / BD * 3.5 * Dt;
			Bob.Z += (Z - Bob.Z) / BD * 3.5 * Dt;
			if (BD < 6) { ReelIn(TEXT("Line in.")); return; }
		}
		double Dip = FMath::Sin(T * 2) * .05;
		if (FState == EKeysFish::Waiting)
		{
			BiteT -= Dt * (bReel ? .6 : 1);
			if (!Sp && BiteT < 3.2) ChooseFish();
			if (BiteT <= 0) { FState = EKeysFish::Nibble; Nibs = 1 + FMath::RandRange(0, 2); NibT = .05; }
		}
		else if (FState == EKeysFish::Nibble)
		{
			NibT -= Dt;
			if (NibT <= 0)
			{
				AKeysAudio* A = AKeysAudio::Get(this);
				if (Nibs <= 0)
				{
					FState = EKeysFish::Bite; Win = StrikeWindow(1.4); bStrikeShown = true; Shake = FMath::Max(Shake, .15);
					if (A) { A->Strike(Bob.X, Bob.Z); A->Splash(Bob.X, Bob.Z, 20, 1.2); }
				}
				else { Nibs--; DipT = .28; NibT = Rand(.8, 1.6); if (A) A->Nibble(Bob.X, Bob.Z); }
			}
		}
		else
		{
			Win -= Dt;
			if (Win <= 0) { MissFish(TEXT("Missed it - the fish dropped the bait.")); return; }
		}
		if (DipT > 0) { DipT -= Dt; Dip = -.16 * FMath::Sin(Clamp(1 - DipT / .28, 0, 1) * PI); }
		if (FState == EKeysFish::Bite) Dip = -.6 + FMath::Sin(T * 30) * .1;
		Bob.Y = Surf + Dip;
		if (Sp)
		{
			double TX = Bob.X + OFX, TZ = Bob.Z + OFZ, KK = Dt * .9;
			if (FState == EKeysFish::Nibble) { TX = Bob.X + OFX * .3; TZ = Bob.Z + OFZ * .3; KK = Dt * 2.5; }
			if (FState == EKeysFish::Bite)
			{
				const double AX = Bob.X - X, AZ = Bob.Z - Z, AL = FMath::Max(1e-3, Hyp(AX, AZ));
				Bob.X += AX / AL * 2 * Dt;
				Bob.Z += AZ / AL * 2 * Dt;
				TX = Bob.X; TZ = Bob.Z; KK = Dt * 5;
			}
			FX += (TX - FX) * FMath::Min(1.0, KK);
			FZ += (TZ - FZ) * FMath::Min(1.0, KK);
		}
	}
	else if (FState == EKeysFish::Fight)
	{
		const double S = Str;
		const bool bJumper = Sp->bJump, bBill = IsBillfish(Key), bSounder = IsSounder(Key);
		RunT -= Dt;
		if (bRunningFish && RunT <= 0) { bRunningFish = false; RunT = Rand(1.4, 3.2) * (1.3 - Stamina * .4); }
		else if (!bRunningFish && RunT <= 0)
		{
			bRunningFish = true;
			if (AKeysAudio* A = AKeysAudio::Get(this)) A->Splash(FX, FZ, 10, .9);
			RunT = Rand(.9, 2.4) * (.5 + Stamina * .8);
			RunAng = Rand(-1.1, 1.1);
			Pull = S * Rand(.7, 1.1) * (.45 + .55 * Stamina);
			if (bJumper && FMath::FRand() < .4 + .4 * Stamina) JumpQ = bBill ? 2 + FMath::RandRange(0, 2) : 1 + (FMath::FRand() < .3 ? 1 : 0);
			else if (bSounder && FMath::FRand() < .6)
			{
				DeepT = Rand(4, 9);
				if (T - SoundMsgT > 8) { SoundMsgT = T; ShowToast(TEXT("It's sounding - diving for the bottom!")); }
			}
		}
		if (JumpQ > 0 && !bJumping) { StartJump(bBill && JumpQ > 1); JumpQ--; }
		ShakeT -= Dt;
		if (ShakeT <= 0)
		{
			ShakeT = Rand(.5, 1.8) * (1.4 - Stamina * .6);
			Tension += (bReel ? .08 : .04) * S;
			Shake = FMath::Max(Shake, .1 + .1 * S);
			if (!(DeepT > 0) && !bJumping)
				if (AKeysAudio* A = AKeysAudio::Get(this)) A->Splash(FX, FZ, 6, .7);
		}
		DeepT = FMath::Max(0.0, DeepT - Dt);
		if (FMath::FRand() < Dt * 1.5)
			if (AKeysAudio* A = AKeysAudio::Get(this)) A->Splash(FX, FZ, 3, .5);
		Deep = Lerp(Deep, DeepT > 0 ? FMath::Max(0.0, FMath::Min(DepthAt(FX, FZ) - 1, 4 + 8 * S)) : 0, FMath::Min(1.0, Dt * .8));
		const bool bInAir = bJumping;
		const double Dg = Drag, PullK = (.55 + .15 * Dg) * (DeepT > 0 ? 1.25 : 1);
		// reeling loads the line; when you stop, the drag lets line slip and tension settles to what the drag holds
		if (bReel) Tension = Clamp(Tension + (.36 + (bRunningFish ? Pull * .72 * PullK : .04) + (bInAir ? .65 : 0)) * Dt, 0, 1.15);
		else
		{
			const double Tgt = bInAir ? .05 : bRunningFish ? Clamp(.28 + .11 * Dg + .17 * Pull * (DeepT > 0 ? 1.2 : 1), 0, .97) : .08;
			Tension += (Tgt - Tension) * FMath::Min(1.0, Dt * 1.6);
		}
		double DX = FX - X, DZ = FZ - Z;
		const double Dist = FMath::Max(1e-3, Hyp(DX, DZ));
		DX /= Dist; DZ /= Dist;
		if (bRunningFish)
		{
			const double A = FMath::Atan2(DZ, DX) + RunAng;
			const double Sp2 = (3 + S * 6.5) * (.4 + .6 * Stamina) * (bReel ? .5 : 1) * (1.45 - .15 * Dg);
			FX += FMath::Cos(A) * Sp2 * Dt;
			FZ += FMath::Sin(A) * Sp2 * Dt;
		}
		if (bReel)
		{
			const double R = FMath::Max(4.0, 10.5 * (1 - .22 * S)) * (bRunningFish ? .3 : 1) * (.75 + .35 * (1 - Stamina)) * (.7 + .1 * Dg) * (HasGear(TEXT("reel.lever")) ? 1.3 : 1);
			FX -= DX * R * Dt;
			FZ -= DZ * R * Dt;
		}
		Stamina = FMath::Max(0.0, Stamina - Dt * .1 * (.45 + Tension) / FMath::Sqrt(S));
		if (Tension >= LineBreakAt()) { OverT += Dt; if (OverT > .6) { Lose(TEXT("Snap! Too much pressure and the line parted.")); return; } }
		else OverT = FMath::Max(0.0, OverT - Dt);
		if (Tension < .06) { SlackT += Dt; if (SlackT > 2.6) { Lose(TEXT("Slack line - the fish shook the hook.")); return; } }
		else SlackT = 0;
		if (Dist > MaxLine) { Lose(TEXT("Spooled! Chase the big ones with the boat next time.")); return; }
		if (LandH(FX, FZ) > .2) { Lose(TEXT("Cut off in the mangroves.")); return; }
		LineDist = Dist;
		if (Dist < 4.5) { Land(); return; }
		UpdateJump(Dt);
		if (!bJumping)
		{
			// swimming just under the surface: you see its back and tail break the water, and it vanishes when it sounds
			const double Yaw = bRunningFish ? FMath::Atan2(DZ, DX) + RunAng : FMath::Atan2(-DZ, -DX);
			const double FY = WaveH(FX, FZ, T, AmpAt(FX, FZ)) - FishLen * .09 - Deep;
			FishMesh->SetWorldLocationAndRotation(ToUE(FX, FY, FZ), FRotator(0, FMath::RadiansToDegrees(Yaw), FMath::Sin(T * 7) * 8));
		}
	}
}

void AKeysBoatPawn::UpdateViewModel(double Dt)
{
	if (!bFpOn) return;
	const double Now = GetWorld()->GetTimeSeconds();
	double Bend = .01;
	if (FState == EKeysFish::Fight) Bend = Clamp(Tension, 0, 1.1) * .15 + Shake * .08 * FMath::Sin(Now * 45);
	else if (FState == EKeysFish::Bite) Bend = .09;
	else if (FState == EKeysFish::Nibble && DipT > 0) Bend = .045;
	VmBend = Lerp(VmBend, Bend, FMath::Min(1.0, Dt * 10));
	double Rx = -1.15 + (bCharging ? Charge * .7 : 0);
	if (VmWhip > 0) { VmWhip = FMath::Max(0.0, VmWhip - Dt / .35); Rx -= .45 * FMath::Sin(VmWhip * PI); }
	VmRx = Lerp(VmRx, Rx, FMath::Min(1.0, Dt * 14));
	if ((bBack && IsLineOut()) || (FState == EKeysFish::Fight && bAction)) VmCrank -= Dt * 22;
}

// the first-person rod, reel and hands, held in front of the camera (the JS view model, in three.js camera space:
// x right, y up, z back). Returns the rod tip in world meters, Unreal axes.
FVector AKeysBoatPawn::BuildViewModel(FKeysMesh& M)
{
	const FTransform CT = Camera->GetComponentTransform();
	auto W = [&CT](const FVector& V) { return CT.TransformPosition(FVector(-V.Z, V.X, V.Y) * 100.0) / 100.0; };
	// Ry(.16) * Rx(Th): the rod base's yaw-pitch-roll ("YXZ") rotation
	auto Rot = [](double Th, const FVector& V)
	{
		const double C = FMath::Cos(Th), Sn = FMath::Sin(Th), Cy = FMath::Cos(.16), Sy = FMath::Sin(.16);
		const FVector A(V.X, V.Y * C - V.Z * Sn, V.Y * Sn + V.Z * C);
		return FVector(A.X * Cy + A.Z * Sy, A.Y, -A.X * Sy + A.Z * Cy);
	};
	const FVector Base(.3, -.42, -.55);
	const double Rx = VmRx;
	auto S0 = [&](double A, double B, double C) { return Base + Rot(Rx, FVector(A, B, C)); };   // point in the butt section's frame
	const FLinearColor RodC(FColor(0x1d, 0x21, 0x26)), Cork(FColor(0xb8, 0x92, 0x5e)), DarkC(FColor(0x22, 0x26, 0x2b)), GoldC(FColor(0xc9, 0xa2, 0x27)),
		Skin(FColor(0xd9, 0xa2, 0x7f)), Sleeve(FColor(0x9c, 0xc8, 0xe0)), Guide(FColor(0xc9, 0xce, 0xd3));
	const FQuat CQ = CT.GetRotation();

	// five rod sections, each bent a little more toward the load
	const double SegL = .52;
	FVector P = Base;
	for (int32 i = 0; i < 5; i++)
	{
		const FVector Dir = Rot(Rx - VmBend * i, FVector(0, 1, 0));
		const FVector Q = P + Dir * SegL;
		const double R0 = .022 - .0034 * i, R1 = FMath::Max(.004, R0 - .0034);
		M.AddTube(W(P), W(Q), R0, R1, 8, RodC);
		if (i > 0) M.AddBox(W(P + Dir * .02 + Rot(Rx - VmBend * i, FVector(0, 0, -.02))), FVector(.006, .03, .03), Guide, CQ);
		P = Q;
	}
	// cork grip, reel and crank
	M.AddTube(W(S0(0, -.05, 0)), W(S0(0, .29, 0)), .032, .03, 10, Cork);
	M.AddTube(W(S0(0, .26, -.075)), W(S0(0, .34, -.075)), .05, .045, 12, DarkC);
	M.AddTube(W(S0(0, .34, -.075)), W(S0(0, .39, -.075)), .042, .042, 12, GoldC);
	const double Cc = FMath::Cos(VmCrank), Sc = FMath::Sin(VmCrank);
	const FVector H0 = S0(.055, .3, -.075), Arm = S0(.055, .3 + .1 * Cc, -.075 + .1 * Sc);
	M.AddTube(W(H0), W(Arm), .008, .008, 4, DarkC);
	M.AddBox(W(S0(.1, .3 + .1 * Cc, -.075 + .1 * Sc)), FVector(.075, .075, .075), Skin, CQ);
	// the hand on the grip and two forearms in sleeves
	M.AddBox(W(S0(0, .38, -.02)), FVector(.1, .085, .11), Skin, CQ);
	{
		const FVector C1(.02, .3, -.2), D1(0, FMath::Cos(1.1), FMath::Sin(1.1));
		M.AddTube(W(S0(C1.X - D1.X * .225, C1.Y - D1.Y * .225, C1.Z - D1.Z * .225)), W(S0(C1.X + D1.X * .225, C1.Y + D1.Y * .225, C1.Z + D1.Z * .225)), .055, .05, 8, Sleeve);
		const FVector D2 = FVector(.479, .546, .688).GetSafeNormal(), C2(.16, .16, -.22);
		M.AddTube(W(S0(C2.X - D2.X * .2, C2.Y - D2.Y * .2, C2.Z - D2.Z * .2)), W(S0(C2.X + D2.X * .2, C2.Y + D2.Y * .2, C2.Z + D2.Z * .2)), .055, .05, 8, Sleeve);
	}
	return W(P);
}

// When a hooked fish jumps, the camera zooms in and swings to it, then eases back out once it's in the water again.
void AKeysBoatPawn::UpdateJumpCam(double Dt)
{
	if (IsDiving() || bExtCam) return;
	const bool bOn = bJumping && FState == EKeysFish::Fight;
	JumpLook = FMath::FInterpConstantTo(JumpLook, bOn ? 1.0 : 0.0, Dt, bOn ? 3.0 : 1.2);
	float Want = 88.f;
	if (bOn)
	{
		const FVector F = FishMesh->GetComponentLocation(), C = Camera->GetComponentLocation();
		const double D = FVector::Dist(F, C) / 100;
		const double Across = FishLen * 3 + 2.5;   // meters of scene across the frame
		// in first person the rod and hands are in shot, so don't zoom as hard
		const float MinFov = IsFirstPerson() ? 50.f : 11.f;
		Want = FMath::Clamp(float(FMath::RadiansToDegrees(2 * FMath::Atan(Across * .5 / FMath::Max(D, 1.0)))), MinFov, 88.f);
	}
	JumpFov = FMath::FInterpTo(JumpFov, Want, Dt, bOn ? 4.f : 1.8f);
	Camera->SetFieldOfView(JumpFov);
	if (JumpLook > .001)
	{
		const FVector F = FishMesh->GetComponentLocation(), C = Camera->GetComponentLocation();
		const FQuat To = (F - C).Rotation().Quaternion(), From = Camera->GetComponentQuat();
		const double A = JumpLook * JumpLook * (3 - 2 * JumpLook);
		Camera->SetWorldRotation(FQuat::Slerp(From, To, float(A * (IsFirstPerson() ? .6 : 1.0))));
	}
}

void AKeysBoatPawn::DrawLineAndRod(double T)
{
	const bool bLine = IsLineOut();
	Bobber->SetVisibility(bLine && FState != EKeysFish::Fight);
	if (!bLine && !bCharging) { LineMesh->ClearAllMeshSections(); return; }

	FVector End = bLine ? Bob : FVector(X - FMath::Sin(H) * 10, Y, Z - FMath::Cos(H) * 10);
	if (FState == EKeysFish::Fight)
	{
		End = bJumping ? FishMesh->GetComponentLocation() / 100.0 : FVector(FX, 0, FZ);
		if (bJumping) End = FVector(End.X, End.Z, End.Y);   // back to game axes
		else End.Y = WaveH(FX, FZ, T, AmpAt(FX, FZ)) - Deep * .5;
	}
	auto G2U = [](const FVector& G) { return FVector(G.X, G.Z, G.Y); };   // game meters -> Unreal axes, still meters

	FKeysMesh M;
	FVector TipU;   // rod tip, Unreal axes, meters
	if (bFpOn) TipU = BuildViewModel(M);
	else
	{
		const FVector Tip = RodTip(End);
		const double Rx = FMath::Cos(H), Rz = -FMath::Sin(H);
		const double Sgn = ((End.X - X) * Rx + (End.Z - Z) * Rz) >= 0 ? 1 : -1;
		const FVector Butt(X + Rx * Sgn * (Spec().Beam * .5 - .25) + FMath::Sin(H) * Spec().Len * .15, Y + 1.25, Z + Rz * Sgn * (Spec().Beam * .5 - .25) + FMath::Cos(H) * Spec().Len * .15);
		// the rod bends toward the load when a fish is pulling
		const double Bend = FState == EKeysFish::Fight ? Clamp(Tension, 0, 1) : 0;
		const FVector Mid = (Butt + Tip) * .5 + (End - Tip).GetSafeNormal() * Bend * .5;
		const FVector RodTipBent = Tip + (End - Tip).GetSafeNormal() * Bend * .6 - FVector(0, Bend * .5, 0);
		M.AddTube(G2U(Butt), G2U(Mid), .025, .016, 6, FLinearColor(.02f, .02f, .025f));
		M.AddTube(G2U(Mid), G2U(RodTipBent), .016, .006, 6, FLinearColor(.02f, .02f, .025f));
		TipU = G2U(RodTipBent);
	}
	if (bLine)
	{
		const FVector EndU = G2U(End);
		const double Len = (EndU - TipU).Size();
		const double Sag = FState == EKeysFish::Fight ? (1 - Clamp(Tension, 0, 1)) * Len * .12 : FState == EKeysFish::Casting ? 0 : Len * .06;
		const FVector Ctrl = (TipU + EndU) * .5 - FVector(0, 0, Sag);
		const int32 Segs = 24;
		// the braid keeps about a pixel and a half of width however far out the fish is (a 1.3 cm tube is lost against
		// the water at 60 m off the transom in a trolling fight)
		const FVector CamU = Camera->GetComponentLocation() / 100.0;
		auto Rad = [&CamU](const FVector& Q) { return FMath::Max(.013, (Q - CamU).Size() * .0011); };
		FVector Prev = TipU;
		for (int32 i = 1; i <= Segs; i++)
		{
			const double U = double(i) / Segs;
			FVector P = TipU * FMath::Square(1 - U) + Ctrl * 2 * U * (1 - U) + EndU * U * U;
			// a long line under load lies on the water, not through it: only the last stretch goes down to the fish
			if (FState == EKeysFish::Fight && U < .92)
			{
				const double Surf = WaveH(P.X, P.Y, T, AmpAt(P.X, P.Y)) + .06;
				if (P.Z < Surf) P.Z = Surf;
			}
			M.AddTube(Prev, P, Rad(Prev), Rad(P), 4, FLinearColor(.98f, .86f, .12f));   // hi-vis yellow braid
			Prev = P;
		}
		Bobber->SetWorldLocation(ToUE(Bob.X, Bob.Y + .05, Bob.Z));
	}
	M.Commit(LineMesh, 0, AKeysWorld::VertexColorMaterial());
}

// ================================================================ HUD text
FString AKeysBoatPawn::Hint() const
{
	if (!bRunning) return FString();
	const double Kn = KnotsNow();
	const FMarina* Mar = NearMarina(X, Z);
	if (FState == EKeysFish::Idle && !bCharging && Mar && Kn < 5)
		return Cooler.Num() ? FString::Printf(TEXT("Press E to weigh in your cooler at %s"), *Mar->Name) : FString::Printf(TEXT("Tied up at %s - nothing on ice yet"), *Mar->Name);
	if (FState == EKeysFish::Caught) return bCardSpeared || CurTag >= 0 ? TEXT("K keeps it in the cooler - Space releases it") : TEXT("K keeps it in the cooler - Space releases it - T tags it and lets it go");
	if (bTrimMode && FState == EKeysFish::Idle && !bCharging) return TEXT("Trim mode: Up trims up, Down trims down - W/S throttle - T to exit");
	if (bCharging) return TEXT("Release to cast - hold longer to throw farther");
	if (bTrolling && FState == EKeysFish::Idle)
		return TResume > 0 ? FString(TEXT("Clearing the lines - back on the troll in a moment"))
			: TEXT("Trolling - A/D steer the spread over weedlines, birds and the edge - = brings the lines in");
	if (bDriveOn && FState == EKeysFish::Idle) return TEXT("G shows the chartplotter and fish finder - [ ] zoom the chart - 1 switches view");
	switch (FState)
	{
		case EKeysFish::Idle: return Kn > 3 ? TEXT("W/S throttle - A/D steer - H changes the sea state - O for sunset") : TEXT("Hold Space to power a cast - drag the mouse to aim");
		case EKeysFish::Casting:
		case EKeysFish::Waiting: return TEXT("Watch the float - hold S to retrieve - R reels in");
		case EKeysFish::Nibble: return TEXT("Nibble... wait for the float to go under");
		case EKeysFish::Bite: return TEXT("SPACE now to set the hook!");
		case EKeysFish::Fight: return (bAutoOn ? AutoMode + TEXT(" - ") : FString()) + TEXT("Hold S to reel - ease off when red - T/Shift+T drag - F toggles auto-chase");
		default: return FString();
	}
}

FString AKeysBoatPawn::Warning() const
{
	const double D = DepthAt(X, Z);
	if (D < Spec().Draft) return TEXT("Aground - back off to deeper water");
	if (D < Spec().Draft + .4 && KnotsNow() > 5) return TEXT("Shallow water ahead");
	return FString();
}

// ================================================================ frame
void AKeysBoatPawn::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	const double Dt = FMath::Min(double(DeltaSeconds), 1.0 / 20);
	const double T = GetWorld()->GetTimeSeconds();
	HandleInput(DeltaSeconds);
	UpdateCursor();
	RunTestScript(GetWorld()->GetRealTimeSeconds());
	// the pause menu (and its Settings / Controls pages) stops the world: the boat, the fish, the traffic, the waves
	const bool bWantPause = bRunning && Menu != EMenu::None;
	if (bWantPause != UGameplayStatics::IsGamePaused(this)) UGameplayStatics::SetGamePaused(this, bWantPause);
	if (bWantPause) return;
	if (AKeysOnline* O = AKeysOnline::Get(this))
	{
		// log back in automatically if you were online last time
		if (!bOnlineTried) { bOnlineTried = true; if (Save && Save->bOnlineAuto && !Save->OnlineName.IsEmpty()) O->Connect(Save->OnlineName, Save->OnlineCrew); }
		if (O->Toasts.Num() && T > ToastUntil - 1) { ShowToast(O->Toasts[0], 4); O->Toasts.RemoveAt(0); }
	}
	if (bRunning && OffshoreF(X, Z) > .5 && !bDeepTip) { bDeepTip = true; ShowToast(TEXT("Deep blue water - the farther out you run, the bigger the fish.")); }
	if (IsAshore()) { Thr = 0; Speed = 0; DVX = DVZ = 0; Steer = 0; }
	UpdateBoat(Dt, T);
	if (IsAshore()) { X = PinX; Z = PinZ; H = PinH; Speed = 0; }
	if (bRunning) UpdateFishing(Dt, T);
	if (bRunning) UpdateBuffs(T);
	UpdateFinal(Dt, T);
	UpdateTroll(Dt, T);
	if (AKeysAudio* A = AKeysAudio::Get(this))
	{
		FKeysBoatAudioState S;
		S.bRunning = bRunning; S.BoatIndex = BoatIndex; S.Engines = Spec().Engines;
		S.Speed = Speed; S.TopMs = Spec().Top * .5144 * SpeedScale; S.Thr = Thr; S.TrimV = TrimV; S.TrimVent = TrimVent;
		S.bAir = bAir; S.Gear = Gear; S.bFwd = bFwd; S.BoatPos = FVector(X, Y, Z);
		S.bLineOut = IsLineOut();
		S.bFight = FState == EKeysFish::Fight; S.bFishRunning = bRunningFish;
		S.bReeling = bBack || (bAction && FState == EKeysFish::Fight);
		S.FishStr = Str; S.Drag = Drag;
		A->SetBoatState(S);
		FString M;
		while (A->ConsumeToast(M)) ShowToast(M);
	}
	if (AKeysLife* L = AKeysLife::Get(this))
		if (GetWorld()->GetTimeSeconds() > ToastUntil) { const FString M = L->PopToast(); if (!M.IsEmpty()) ShowToast(M); }
	UpdateCamera(Dt, T);
	UpdateHelm(Dt, T);
	UpdateJumpCam(Dt);
	UpdateViewModel(Dt);
	DrawLineAndRod(T);
	UpdateSpread(Dt, T);
}
