#include "KeysBoatPawn.h"
#include "KeysGeoReal.h"
#include "KeysMangroveLife.h"
#include "Components/TextRenderComponent.h"
#include "KeysGarden.h"
#include "KeysCharter.h"
#include "KeysHUD.h"
#include "KeysWrecks.h"
#include "KeysOcean.h"
#include "KeysSchools.h"
#include "KeysWeather.h"
#include "Components/PointLightComponent.h"
#include "KeysGeo.h"
#include "KeysMesh.h"
#include "KeysWorld.h"
#include "KeysSave.h"
#include "KeysTraffic.h"
#include "KeysExtras.h"
#include "KeysFeed.h"
#include "KeysAudio.h"
#include "KeysOnline.h"
#include "KeysDiver.h"
#include "KeysWalker.h"
#include "KeysVisitors.h"
#include "KeysLocals.h"
#include "KeysYards.h"
#include "KeysDocks.h"
#include "KeysHandgun.h"
#include "KeysTreasure.h"
#include "KeysEstates.h"
#include "KeysTourneyShow.h"
#include "KeysSlots.h"
#include "KeysFinal.h"
#include "KeysTown.h"
#include "KeysTownData.h"
#include "KeysEvents.h"
#include "KeysGfx.h"
#include "KeysTrim.h"
#include "AudioMixerBlueprintLibrary.h"
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
#include "Misc/FileHelper.h"
#include "Engine/GameViewportClient.h"
#include "Misc/App.h"
#include "DynamicRHI.h"
#include "RenderTimer.h"

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
	FishMesh->LightingChannels.bChannel1 = true;   // the diver's fill light (a speared fish held up under water)

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
	KeysGfx::Init(GetWorld());
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
		bFreeFish = Save->bFreeFish; bUnlimitedFuel = Save->bUnlimitedFuel; Fuel = Save->Fuel; SpotsFound = Save->SpotsFound;
		if (bFreeFish) PickBestOwnedBoat();
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
	KeysMesh::GRodLod = 0;   // your own boat's rods are seen up close
	KeysMesh::BuildBoat(M, Spec(), false);
	KeysMesh::GRodLod = 1;
	// the windshields in clear glass, so the helm view looks out through them
	static TWeakObjectPtr<UMaterialInterface> GlassMat;
	if (!GlassMat.IsValid()) GlassMat = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/KeysRun/Materials/M_KR_Glass.M_KR_Glass"));
	FKeysMesh Clear;
	if (GlassMat.IsValid()) M.SplitClear(Clear);
	M.Commit(BoatMesh, 0, AKeysWorld::VertexColorMaterial());
	if (Clear.T.Num()) Clear.Commit(BoatMesh, 1, GlassMat.Get());
	else BoatMesh->ClearMeshSection(1);
	// the Viking's name across her varnished teak transom, her home port under it
	{
		const KeysData::FBoatSpec& BS = Spec();
		const bool bName = BS.Id == TEXT("viking");
		if (bName && TransomName.Num() == 0)
			for (int32 k = 0; k < 2; k++)
			{
				UTextRenderComponent* Tx = NewObject<UTextRenderComponent>(this);
				Tx->SetupAttachment(BoatMesh);
				Tx->RegisterComponent();
				Tx->SetHorizontalAlignment(EHTA_Center);
				Tx->SetVerticalAlignment(EVRTA_TextCenter);
				Tx->SetCastShadow(false);
				TransomName.Add(Tx);
			}
		for (int32 k = 0; k < TransomName.Num(); k++)
		{
			UTextRenderComponent* Tx = TransomName[k];
			if (!Tx) continue;
			Tx->SetVisibility(bName);
			if (!bName) continue;
			const double XT = KeysMesh::SternX(BS) - .03, ZTop = KeysMesh::SheerAt(BS, XT + .1) - .08, ZBot = -.25;
			const double Zc = k == 0 ? FMath::Lerp(ZBot, ZTop, .62) : FMath::Lerp(ZBot, ZTop, .3);
			Tx->SetText(FText::FromString(k == 0 ? TEXT("BADABING") : TEXT("MARATHON, FL")));
			Tx->SetTextRenderColor(k == 0 ? FColor(0xf6, 0xf1, 0xe2) : FColor(0xdd, 0xe6, 0xee));
			Tx->SetWorldSize(k == 0 ? 52.f : 18.f);
			Tx->SetRelativeLocationAndRotation(FVector(XT * 100, 0, Zc * 100), FRotator(0, 180, 0));
		}
	}
	RigBuilt = -1;   // the outriggers are their own mesh, so they can swing (KeysTroll.cpp)
}

void AKeysBoatPawn::PlaceAtSpawn()
{
	// the nearest deep blue water to Boot Key (40 m and more, off the reef), pointed back toward the Keys
	X = -1150;
	Z = LegacyZ(-1150, 305);
	H = PI / 2;
	{
		const double HX = X, HZ = Z;
		bool bFound = false;
		for (double R = 200; R < 8000 && !bFound; R += 40)
		{
			const int32 N = FMath::Max(8, int32(R / 15));
			for (int32 k = 0; k < N && !bFound; k++)
			{
				const double A = k * 2 * PI / N, PX = HX + FMath::Cos(A) * R, PZ = HZ + FMath::Sin(A) * R;
				if (DepthAt(PX, PZ) < 40 || DepthAt(PX + 60, PZ) < 40 || DepthAt(PX - 60, PZ) < 40 || DepthAt(PX, PZ + 60) < 40 || DepthAt(PX, PZ - 60) < 40) continue;
				if (KeysWrecks::Near(PX, PZ, 250)) continue;   // not on top of a wreck: that would find it for free
				X = PX; Z = PZ; bFound = true;
				H = FMath::Atan2(-(HX - PX), -(HZ - PZ));   // KeysKit::HeadingOf
			}
		}
		UE_LOG(LogTemp, Log, TEXT("KRSpawn %.0f, %.0f depth %.0f found %d"), X, Z, DepthAt(X, Z), bFound ? 1 : 0);
	}
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
double AKeysBoatPawn::DepthHere() const { return FMath::Max(0.0, DepthAt(X, Z) + TideRise()); }   // what the sounder reads: the bottom, at this state of the tide
FString AKeysBoatPawn::NearName() const { return NearestName(X, Z); }
double AKeysBoatPawn::CoolerLb() const { double S = 0; for (const FKeysCooler& C : Cooler) S += C.Weight; return S; }

FString AKeysBoatPawn::ZoneName() const { return ZoneNameAt(X, Z); }

FString AKeysBoatPawn::ZoneNameAt(double PX, double PZ) const
{
	if (const AKeysLife* L = AKeysLife::Get(this))
	{
		const FString Area = L->AreaLabel(PX, PZ);
		if (!Area.IsEmpty()) return Area;
	}
	const FString Zn = ZoneAt(PX, PZ);
	if (Zn == TEXT("Offshore") && OffshoreF(PX, PZ) > .5) return TEXT("Deep blue water");
	if (InMiami(PX, PZ) && Zn != TEXT("Creek")) return TEXT("Biscayne Bay");
	if (Zn == TEXT("Creek") && CanalDist(PX, PZ) < 0) return TEXT("Residential canal");
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
		Add(TEXT("mode"), TEXT("Mode"), bFreeFish ? TEXT("Free Fish - no clock, no charters, no pay") : TEXT("Charter Captain - clients, clocks, pay"), true);
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
		Add(TEXT("mode"), TEXT("Mode"), bFreeFish ? TEXT("Free Fish - no clock, no charters, no pay") : TEXT("Charter Captain - clients, clocks, pay"), true);
		const AKeysEvents* Ev = AKeysEvents::Get(this);
		const bool bTour = Ev && Ev->InTourney() && Ev->TourneyPhase() != AKeysEvents::ETourney::Over;
		const bool bRace = Ev && (Ev->RacePhase() == AKeysEvents::ERace::Countdown || Ev->RacePhase() == AKeysEvents::ERace::Racing);
		// Free Fish is a sandbox: no timed events
		if (!bFreeFish || bTour) Add(TEXT("tourney"), TEXT("Fishing tournament"), bTour ? FString(TEXT("quit it")) : FString(TEXT("15 min, five species")));
		if (!bFreeFish || bRace) Add(TEXT("race"), TEXT("Marathon race"), bRace ? FString(TEXT("abandon it")) : FString(TEXT("2 laps, R at the helm")));
		Add(TEXT("estates"), TEXT("Land office"), TEXT("Keys Run Estates  ( , )"));
		{
			TArray<FRunDest> Ds;
			RunDests(Ds, true);
			if (!Ds.Num()) Add(TEXT("runto"), TEXT("Teleport to"), TEXT("nowhere yet - get within 200 m of a marina, reef, wreck or rig"));
			if (Ds.Num())
			{
				const FRunDest& D = Ds[FMath::Clamp(RunSel, 0, Ds.Num() - 1)];
				const double Km = FMath::Sqrt(FMath::Square(D.X - X) + FMath::Square(D.Z - Z)) / 1000;
				Add(TEXT("runto"), FString::Printf(TEXT("Teleport to (%d found)"), Ds.Num()), FString::Printf(TEXT("%s - %s, %s"), *D.Name, *D.Note, Km < 1 ? *FString::Printf(TEXT("%d m"), FMath::RoundToInt(Km * 1000)) : *FString::Printf(TEXT("%.1f km"), Km)), true);
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
		Add(TEXT("fuel"), TEXT("Fuel"), bUnlimitedFuel ? TEXT("Unlimited") : FString::Printf(TEXT("Realistic (%d%% - refuel free at any marina)"), FMath::RoundToInt(Fuel * 100)), true);
		Add(TEXT("autochase"), TEXT("Auto-chase hooked fish"), bAutoChase ? TEXT("On") : TEXT("Off"), true);
		Add(TEXT("diveview"), TEXT("Dive view"), bDiveChase ? TEXT("Chase") : TEXT("Dive mask"), true);
		Add(TEXT("haze"), TEXT("Underwater haze"), UnderwaterHaze < .8 ? TEXT("Clear") : UnderwaterHaze > 1.2 ? TEXT("Murky") : TEXT("Normal"), true);
		Add(TEXT("gfx"), TEXT("Graphics quality"), KeysGfx::QualityLabel(), true);
		Add(TEXT("map"), TEXT("Map"), FString(RealMapWanted() ? TEXT("Real Florida Keys - Middle Keys") : TEXT("Classic")) + (RealMapWanted() != RealMap() ? TEXT("  (quit and reopen to switch)") : TEXT("")), true);
		Add(TEXT("res"), TEXT("Resolution scale"), KeysGfx::ResScaleLabel(), true);
		Add(TEXT("display"), TEXT("Display"), KeysGfx::IsFullscreen() ? TEXT("Full screen") : TEXT("Window"), true);
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
	else if (Id == TEXT("mode"))
	{
		bFreeFish = !bFreeFish;
		if (Menu == EMenu::Pause)
		{
			// mid-run: keep the boat you're on; a charter in progress is called off going to Free Fish
			AKeysCharter* Ch = AKeysCharter::Get(this);
			const bool bHad = bFreeFish && Ch && Ch->HasActiveCharter();
			if (bHad) Ch->CancelCharter();
			ShowToast(bFreeFish ? (bHad ? TEXT("Free Fish - the charter's called off. No clock, no pay: just fish.") : TEXT("Free Fish - no clock, no charters, no pay."))
				: TEXT("Charter Captain - pick up a charter at any marina's board."), 4);
		}
		else if (bFreeFish) PickBestOwnedBoat();
		SaveEconomy();
	}
	else if (Id == TEXT("fuel")) { bUnlimitedFuel = !bUnlimitedFuel; bFuelOutMsg = false; SaveEconomy(); }
	else if (Id == TEXT("map"))
	{
		SetRealMapWanted(!RealMapWanted());
		ShowToast(RealMapWanted() != RealMap() ? TEXT("Map changed - quit and reopen Keys Run to load it.") : TEXT("Staying on this map."), 5);
	}
	else if (Id == TEXT("runto"))
	{
		TArray<FRunDest> Ds;
		RunDests(Ds, true);
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
			const bool bRaceOn = Ev->RacePhase() == AKeysEvents::ERace::Countdown || Ev->RacePhase() == AKeysEvents::ERace::Racing;
			const bool bTourOn = Ev->InTourney() && Ev->TourneyPhase() != AKeysEvents::ETourney::Over;
			if (bFreeFish && !(Id == TEXT("race") ? bRaceOn : bTourOn)) ShowToast(TEXT("No timed events in Free Fish."));
			else if (Id == TEXT("race")) { StopTroll(true); Ev->ToggleRace(); }
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
	else if (Id == TEXT("gfx")) KeysGfx::SetQuality(KeysGfx::Quality() + (Dir < 0 ? -1 : 1));
	else if (Id == TEXT("res"))
	{
		// Auto, 50, 60 ... 100 percent
		static const int32 Steps[7] = {0, 50, 60, 70, 80, 90, 100};
		int32 I = 0;
		for (int32 k = 0; k < 7; k++) if (Steps[k] == KeysGfx::ResScale()) I = k;
		KeysGfx::SetResScale(Steps[(I + (Dir < 0 ? 6 : 1)) % 7]);
	}
	else if (Id == TEXT("display")) KeysGfx::SetFullscreen(!KeysGfx::IsFullscreen());
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
	ShowToast(FString::Printf(TEXT("Welcome to Marathon. The %s %s is fueled up and out in the Gulf Stream off Marathon. Esc for the menu."), *Spec().Brand, *Spec().Name), 5);
}

static int32 GScriptCreek = -1;   // creek=N: the mangrove creek the script is idling her along

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
		bScriptLogFight = FCommandLine::Get() && FCString::Strifind(FCommandLine::Get(), TEXT("KRScript=")) != nullptr;
		if (FParse::Value(FCommandLine::Get(), TEXT("KRScript="), Script, false))
		{
			TArray<FString> Parts;
			Script.ParseIntoArray(Parts, TEXT(";"));
			if (!FParse::Param(FCommandLine::Get(), TEXT("KRTestSave"))) Save = nullptr;   // test runs never write your leaderboard (unless given their own slot)
			bScriptLog = true;
			for (const FString& Pt : Parts)
			{
				FString When, What;
				if (Pt.Split(TEXT(":"), &When, &What)) Steps.Add(TPair<double, FString>(FCString::Atod(*When), What.TrimStartAndEnd()));
			}
		}
	}
	// perf=<label>: a second to settle, then five seconds of frame times (as stat unit: frame, game, draw, GPU), one line
	// appended to Saved/PerfLog.txt per measurement (the perf bookmarks, Tools/perf_bookmarks.sh)
	{
		struct FPerfRun { FString Label; double Start = -1, End = -1, Frame = 0, Game = 0, Draw = 0, Gpu = 0, Worst = 0; int32 N = 0; };
		static FPerfRun PR;
		if (PR.Start >= 0 && T >= PR.Start)
		{
			const double Fm = FApp::GetDeltaTime() * 1000;
			PR.Frame += Fm; PR.Worst = FMath::Max(PR.Worst, Fm);
			PR.Game += FPlatformTime::ToMilliseconds(GGameThreadTime);
			PR.Draw += FPlatformTime::ToMilliseconds(GRenderThreadTime);
			PR.Gpu += FPlatformTime::ToMilliseconds(RHIGetGPUFrameCycles());
			PR.N++;
			if (T >= PR.End && PR.N > 0)
			{
				const double Ms = PR.Frame / PR.N;
				int32 PVX = 0, PVY = 0;
				if (GEngine && GEngine->GameViewport && GEngine->GameViewport->Viewport) { const FIntPoint Sz = GEngine->GameViewport->Viewport->GetSizeXY(); PVX = Sz.X; PVY = Sz.Y; }
				const FString PLine = FString::Printf(TEXT("%s\t%s\t%dx%d\t%.1f fps\tframe %.2f ms\tgame %.2f\tdraw %.2f\tgpu %.2f\tworst %.1f ms\t%d frames\n"),
					*PR.Label, *KeysGfx::QualityLabel(), PVX, PVY, 1000 / FMath::Max(.01, Ms), Ms, PR.Game / PR.N, PR.Draw / PR.N, PR.Gpu / PR.N, PR.Worst, PR.N);
				FFileHelper::SaveStringToFile(PLine, *(FPaths::ProjectSavedDir() / TEXT("PerfLog.txt")), FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM, &IFileManager::Get(), FILEWRITE_Append);
				UE_LOG(LogTemp, Log, TEXT("KRPerf %s"), *PLine);
				PR.Start = -1;
			}
		}
		if (Steps.Num() && T >= Steps[0].Key && Steps[0].Value.StartsWith(TEXT("perf=")))
		{
			PR = FPerfRun();
			PR.Label = Steps[0].Value.Mid(5);
			PR.Start = T + 1; PR.End = T + 6;
			Steps.RemoveAt(0);
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
		else if (Cmd == TEXT("shotclean"))
		{
			// the same, without the HUD (for comparing a model with photos)
			static int32 Clean = 50;
			FScreenshotRequest::RequestScreenshot(FPaths::ScreenShotDir() / FString::Printf(TEXT("KRScript_%02d.png"), Clean++), false, false);
		}
		else if (Cmd.StartsWith(TEXT("hide=")))
		{
			// hide=KeysExtras: hide every actor of that class (the crew, the gull, the buddy boat...)
			for (TActorIterator<AActor> It(GetWorld()); It; ++It)
				if (It->GetClass()->GetName() == Cmd.Mid(5)) It->SetActorHiddenInGame(true);
		}
		else if (Cmd.StartsWith(TEXT("seastate="))) SetSeaState(FMath::Clamp(FCString::Atoi(*Cmd.Mid(9)), 0, SeaStates().Num() - 1));
		else if (Cmd == TEXT("esc")) { Menu = EMenu::Pause; MenuSel = 0; }
		else if (Cmd.StartsWith(TEXT("sel="))) MenuSel = FCString::Atoi(*Cmd.Mid(4));
		else if (Cmd == TEXT("login")) { bLoginOpen = true; LoginField = 0; LoginName = TEXT("Mat"); LoginCrew = TEXT("keys"); }
		else if (Cmd == TEXT("closeall")) { bLoginOpen = bBoardOpen = false; }
		else if (Cmd == TEXT("cast")) CastLine(.55);
		else if (Cmd == TEXT("charge")) StartCharge();                                    // Space held, as a player would
		else if (Cmd == TEXT("release")) { if (bCharging) { bCharging = false; CastLine(Charge); } }
		else if (Cmd == TEXT("autostrike")) bScriptAutoStrike = true;   // strike on the bite, as a player would
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
		else if (Cmd.StartsWith(TEXT("rig=")))
		{
			// rig=N[,metres]: the boat that far off the N-th oil rig (150 m by default), bow on to it
			FString Ns, Ds;
			if (!Cmd.Mid(4).Split(TEXT(","), &Ns, &Ds)) Ns = Cmd.Mid(4);
			const double Dd = Ds.IsEmpty() ? 150 : FCString::Atod(*Ds);
			double RX, RZ;
			if (AKeysLife* L = AKeysLife::Get(this); L && L->RigAt(FCString::Atoi(*Ns), RX, RZ))
			{
				X = RX + Dd * .966; Z = RZ + Dd * .259; H = FMath::Atan2(-(RX - X), -(RZ - Z)); Speed = Thr = 0; bCamInit = false;
			}
		}
		else if (Cmd.StartsWith(TEXT("lookrig")))
		{
			// lookrig[=pitch]: point the diver's mask (or the chase camera) at the nearest oil rig
			const double LkPitch = Cmd.Contains(TEXT("=")) ? FCString::Atod(*Cmd.Mid(8)) : 0;
			double BX = 1e18, BZ = 0, RX, RZ, Ex = X, Ez = Z;
			AKeysDiver* Dv = AKeysDiver::Get(this);
			if (Dv && Dv->IsDiving()) { Ex = Dv->State().X; Ez = Dv->State().Z; }
			if (AKeysLife* L = AKeysLife::Get(this))
				for (int32 i = 0; i < 4 && L->RigAt(i, RX, RZ); i++)
					if (Hyp(RX - Ex, RZ - Ez) < Hyp(BX - Ex, BZ - Ez)) { BX = RX; BZ = RZ; }
			const double Yaw = FMath::Atan2(-(BX - Ex), -(BZ - Ez));
			const double DvYaw = FMath::Atan2(-(BZ - Ez), -(BX - Ex)) + (Cmd.Contains(TEXT(",")) ? FCString::Atod(*Cmd.RightChop(Cmd.Find(TEXT(",")) + 1)) : 0);   // the mask's own yaw; lookrig=pitch,turn
			if (Dv && Dv->IsDiving()) Dv->DebugCmd(TEXT("look"), FString::Printf(TEXT("%f,%f"), DvYaw, LkPitch));
			else CamYaw = FMath::UnwindRadians(Yaw - H + PI);
		}
		else if (Cmd == TEXT("bookmark=marina"))
		{
			// the perf bookmark: the boat just off the first marina, bow on to its docks
			TArray<FRunDest> Ds;
			RunDests(Ds, false);
			for (const FRunDest& D : Ds)
			{
				if (!D.Name.Contains(TEXT("Marina"))) continue;
				double NX, NZ;
				if (!TeleportSpot(D, NX, NZ)) continue;
				X = NX; Z = NZ;
				H = FMath::Atan2(-(D.Z - Z), D.X - X);
				Speed = Thr = DVX = DVZ = 0; bCamInit = false;
				if (AKeysWorld* Wd = World()) { Wd->FocusX = X; Wd->FocusZ = Z; }
				UE_LOG(LogTemp, Log, TEXT("KRScript bookmark marina: %s at %.0f %.0f"), *D.Name, X, Z);
				break;
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
		else if (Cmd.StartsWith(TEXT("pelicans")))
		{
			if (AKeysLife* Lf = AKeysLife::Get(this)) Lf->BringPelicans(X, Z, H, Cmd.Len() > 9 ? FCString::Atod(*Cmd.Mid(9)) : 30);
		}
		else if (Cmd == TEXT("goweed"))
		{
			// beside the nearest sargassum weedline, looking along it
			if (const AKeysLife* Lf = AKeysLife::Get(this))
			{
				const FKeysWeedline* Best = nullptr;
				for (const FKeysWeedline& Wd : Lf->AllWeeds()) if (!Best || Hyp(Wd.X - X, Wd.Z - Z) < Hyp(Best->X - X, Best->Z - Z)) Best = &Wd;
				if (Best) { X = Best->X - FMath::Sin(Best->Ang) * 14; Z = Best->Z + FMath::Cos(Best->Ang) * 14; H = Best->Ang; Speed = Thr = 0; bCamInit = false; }
			}
		}
		else if (Cmd == TEXT("weigh")) WeighIn();
		else if (Cmd.StartsWith(TEXT("pin=")))
		{
			// pin=t:N[:dist] or pin=r:N[:dist]: hold a passing boat alongside for a look
			TArray<FString> Pp; Cmd.Mid(4).ParseIntoArray(Pp, TEXT(":"));
			if (AKeysTraffic* Tr = Cast<AKeysTraffic>(UGameplayStatics::GetActorOfClass(this, AKeysTraffic::StaticClass())); Tr && Pp.Num() >= 2)
			{
				const int32 N = FCString::Atoi(*Pp[1]);
				Tr->PinTraffic = Pp[0] == TEXT("t") ? N : -1;
				Tr->PinRacer = Pp[0] == TEXT("r") ? N : -1;
				if (Pp.Num() >= 3) Tr->PinDist = FCString::Atod(*Pp[2]);
			}
		}
		else if (Cmd == TEXT("mode=charter")) bFreeFish = false;
		else if (Cmd == TEXT("mode=free")) bFreeFish = true;
		else if (Cmd == TEXT("ffstate"))
		{
			UE_LOG(LogTemp, Log, TEXT("KRSave slot %s, writing %s, money in the save file %lld"), UKeysSave::SlotName(), Save ? TEXT("yes") : TEXT("no"), Save ? Save->Cash : -1);
			const AKeysCharter* Ch = AKeysCharter::Get(this);
			const AKeysEvents* Ev = AKeysEvents::Get(this);
			const double* Best = nullptr;
			UE_LOG(LogTemp, Log, TEXT("KRFree mode %s cash %lld count %d boat %s charterBoard %d charterActive %d timer %d tourney %d racing %d fuel %.2f unlimited %d"),
				bFreeFish ? TEXT("FreeFish") : TEXT("Charter"), Cash, Count, *Spec().Id, Ch && Ch->IsBoardShown() ? 1 : 0, Ch && Ch->HasActiveCharter() ? 1 : 0,
				Ch && Ch->IsTimerRunning() ? 1 : 0, Ev && Ev->InTourney() ? 1 : 0, Ev && Ev->IsRacing() ? 1 : 0, Fuel, bUnlimitedFuel ? 1 : 0);
			if (Save) for (const TPair<FString, double>& B : Save->SpeciesBest) UE_LOG(LogTemp, Log, TEXT("KRFree best %s %.1f lb"), *B.Key, B.Value);
			(void)Best;
		}
		else if (Cmd.StartsWith(TEXT("tide="))) { ForceTide(FCString::Atod(*Cmd.Mid(5))); UE_LOG(LogTemp, Log, TEXT("KRTide %s level %.2f rise %.2f m, bite flats x%.2f channel x%.2f"), *TideName(), TideLevel(), TideRise(), TideBite(TEXT("Flats")), TideBite(TEXT("Bridge"))); }
		else if (Cmd == TEXT("picks"))
		{
			// what bites here: 400 draws from the table for the water under the boat
			if (const AKeysLife* L = AKeysLife::Get(this))
			{
				const FKeysFishSpot Here = L->SpotAt(X, Z);
				TMap<FString, int32> Tally;
				for (int32 i = 0; i < 400; i++) Tally.FindOrAdd(L->PickSpecies(Here, X, Z))++;
				Tally.ValueSort([](int32 A, int32 B) { return A > B; });
				FString Out;
				for (const TPair<FString, int32>& K : Tally) Out += FString::Printf(TEXT(" %s %d%%"), *K.Key, K.Value / 4);
				UE_LOG(LogTemp, Log, TEXT("KRPicks at %.0f, %.0f zone %s depth %.1f m:%s"), X, Z, *Here.Zone, DepthAt(X, Z), *Out);
			}
		}
		else if (Cmd == TEXT("spots"))
		{
			TArray<FRunDest> All, Found;
			RunDests(All); RunDests(Found, true);
			UE_LOG(LogTemp, Log, TEXT("KRSpots %d of %d discovered"), Found.Num(), All.Num());
			for (const FRunDest& D : Found) UE_LOG(LogTemp, Log, TEXT("KRSpots found %s (%s)"), *D.Name, *D.Note);
		}
		else if (Cmd == TEXT("tpcheck"))
		{
			// every destination's landing spot: water depth, shore distance, nearest rig
			TArray<FRunDest> All;
			RunDests(All);
			TArray<FVector2D> Rigs;
			if (const AKeysLife* L = AKeysLife::Get(this)) { TArray<FKeysMarker> Mk; L->GetMarkers(Mk); for (const FKeysMarker& M : Mk) if (M.Kind == 0) Rigs.Add(FVector2D(M.X, M.Z)); }
			int32 Bad = 0;
			for (const FRunDest& D : All)
			{
				double TX, TZ;
				const bool bOk = TeleportSpot(D, TX, TZ);
				double RigD = 1e9;
				for (const FVector2D& R : Rigs) RigD = FMath::Min(RigD, FVector2D::Distance(R, FVector2D(TX, TZ)));
				const double Dep = DepthAt(TX, TZ), Sh = ShoreInfo(TX, TZ).D;
				const bool bGood = bOk && Dep >= Spec().Draft + .6 && (Sh > 6 || (RealMap() && CreekDist(TX, TZ) < -3)) && RigD > 100;   // (in a mangrove creek the banks are close by design)
				Bad += bGood ? 0 : 1;
				UE_LOG(LogTemp, Log, TEXT("KRTp %s %-32s (%s) -> %.0f,%.0f depth %.1f m shore %.0f m rig %.0f m moved %.0f m"), bGood ? TEXT("OK ") : TEXT("BAD"), *D.Name, *D.Note, TX, TZ, Dep, Sh, RigD, FMath::Sqrt(FMath::Square(TX - D.X) + FMath::Square(TZ - D.Z)));
			}
			UE_LOG(LogTemp, Log, TEXT("KRTp checked %d destinations, %d bad"), All.Num(), Bad);
		}
		else if (Cmd.StartsWith(TEXT("charter=")))
		{
			if (AKeysCharter* Ch = AKeysCharter::Get(this)) Ch->DebugCmd(Cmd.Mid(8));
		}
		else if (Cmd == TEXT("goto=garden"))
		{
			// the Abyssal Garden dive site: stop the boat over it
			if (const AKeysGarden* G = AKeysGarden::Get(this)) { X = G->Center().X + 18; Z = G->Center().Y - 6; Speed = Thr = 0; bCamInit = false; }
		}
		else if (Cmd.StartsWith(TEXT("goto=")))
		{
			// goto=flats|bay|hawk|reef|wall|offshore: the nearest open water of that kind, searched in a spiral
			const FString W = Cmd.Mid(5);
			double DMin = 1, DMax = 2.2;
			FString Zn = TEXT("Flats");
			if (W == TEXT("skinny")) { DMin = .5; DMax = .62; }   // ankle-deep flats: the props touch bottom unless tilted
			else if (W == TEXT("bay")) { Zn = TEXT("Backcountry"); DMin = 2.6; DMax = 4; }
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
		else if (Cmd == TEXT("jkey")) ToggleDive();   // J, as pressed (in the water: back aboard from anywhere)
		else if (Cmd == TEXT("tie"))
		{
			// bring the boat alongside the town marina's main pier, stopped, and step ashore
			const FMarina& M = Marinas()[0];
			X = M.SX + 4.8; Z = M.SZ + M.Dir * 30; H = M.Dir > 0 ? PI : 0;
			Speed = Thr = DVX = DVZ = 0; bCamInit = false;
			if (!IsAshore()) ToggleAshore();
		}
		else if (Cmd == TEXT("ashore")) { if (!IsAshore()) ToggleAshore(); }
		else if (Cmd == TEXT("towndock"))
		{
			// tie up at the town marina's first deck and step off (for shots of the town)
			if (KeysTown::DeckBoxes().Num() && !IsAshore())
			{
				const KeysTown::FDeckBox& D = KeysTown::DeckBoxes()[0];
				X = D.CX; Z = D.CZ; Speed = Thr = DVX = DVZ = 0; bCamInit = false;
				ToggleAshore();
			}
		}
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
		else if (Cmd.StartsWith(TEXT("gfx="))) KeysGfx::SetQuality(FCString::Atoi(*Cmd.Mid(4)));
		else if (Cmd.StartsWith(TEXT("fp="))) bFpPref = FCString::Atoi(*Cmd.Mid(3)) != 0;
		else if (Cmd == TEXT("rec=start")) UAudioMixerBlueprintLibrary::StartRecordingOutput(this, 30.f);
		else if (Cmd.StartsWith(TEXT("rec=stop")))
		{
			// rec=stop:<name> writes <name>.wav of what the game played into Saved/BouncedWavFiles
			UAudioMixerBlueprintLibrary::StopRecordingOutput(this, EAudioRecordingExportType::WavFile, Cmd.Len() > 9 ? Cmd.Mid(9) : FString(TEXT("krrec")), FString());
		}
		else if (Cmd.StartsWith(TEXT("boatcam=")))
		{
			TArray<FString> Pp;
			Cmd.Mid(8).ParseIntoArray(Pp, TEXT(","));
			bDbgCam = Pp.Num() >= 6;
			if (bDbgCam)
			{
				DbgCamPos = FVector(FCString::Atod(*Pp[0]), FCString::Atod(*Pp[1]), FCString::Atod(*Pp[2]));
				DbgCamRot = FRotator(FCString::Atod(*Pp[4]), FCString::Atod(*Pp[3]), 0);
				DbgCamFov = FCString::Atof(*Pp[5]);
			}
		}
		else if (Cmd.StartsWith(TEXT("res="))) KeysGfx::SetResScale(FCString::Atoi(*Cmd.Mid(4)));
		else if (Cmd.StartsWith(TEXT("fishcam"))) { FishCamT = -1; TriggerFishCam(Cmd.Len() > 8 ? FCString::Atod(*Cmd.Mid(8)) : 0); }
		else if (Cmd == TEXT("gobeach"))
		{
			// gobeach: the nearest shelving beach (where the surf breaks), the boat 25 m off it facing the shore
			for (double R = 50; R < 4000; R += 25)
			{
				bool bDone = false;
				for (int32 a = 0; a < 48 && !bDone; a++)
				{
					const double PX = X + FMath::Cos(a * PI / 24) * R, PZ = Z + FMath::Sin(a * PI / 24) * R, D = DepthAt(PX, PZ);
					if (D < 1.6 || D > 2.6) continue;
					const double GX = (DepthAt(PX + 1.5, PZ) - DepthAt(PX - 1.5, PZ)) / 3, GZ = (DepthAt(PX, PZ + 1.5) - DepthAt(PX, PZ - 1.5)) / 3;
					const double Gl = FMath::Sqrt(GX * GX + GZ * GZ);
					if (Gl < .04) continue;
					X = PX + GX / Gl * 18; Z = PZ + GZ / Gl * 18; H = FMath::Atan2(GX, GZ); Speed = 0;
					UE_LOG(LogTemp, Log, TEXT("KRScript gobeach -> %.0f, %.0f slope %.3f depth %.1f"), X, Z, Gl, DepthAt(X, Z));
					bDone = true;
				}
				if (bDone) break;
			}
		}
		else if (Cmd.StartsWith(TEXT("ocean="))) { KeysOcean::SetEnabled(Cmd.Mid(6) != TEXT("0")); KeysGeo::SnapSea(); }
		else if (Cmd == TEXT("oceanstats")) { UE_LOG(LogTemp, Log, TEXT("KROcean%s"), *KeysOcean::Stats()); }
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
		else if (Cmd.StartsWith(TEXT("fish:"))) { if (AKeysSchools* Sc = AKeysSchools::Get(this)) Sc->DebugCmd(Cmd.Mid(5)); }   // fish:bait, fish:snapper, fish:pred, fish:scare, fish:flats, fish:stats ...
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
		else if (Cmd.StartsWith(TEXT("lurecam=")))
		{
			// lurecam=N[,dx,dy,dz]: hold the camera beside lure N (-1 to let go)
			TArray<FString> P;
			Cmd.Mid(8).ParseIntoArray(P, TEXT(","));
			LureCamN = P.Num() ? FCString::Atoi(*P[0]) : -1;
			if (P.Num() >= 4) LureCamOff = FVector(FCString::Atod(*P[1]), FCString::Atod(*P[2]), FCString::Atod(*P[3]));
		}
		else if (Cmd == TEXT("trollin")) { if (bTrolling) ToggleTroll(); }
		else if (Cmd == TEXT("trollraise")) { if (bTrolling && PendKey.IsEmpty()) TrollRaise(.9); }   // a fish comes up on the spread now
		else if (Cmd == TEXT("trollstrike") || Cmd.StartsWith(TEXT("trollstrike=")))
		{
			// trollstrike=<species>: that species takes the spread
			DbgTrollKey = Cmd.Len() > 12 ? Cmd.Mid(12) : FString();
			if (bTrolling && PendKey.IsEmpty()) { TrollRaise(.9); if (!PendKey.IsEmpty()) TrollCommit(); }
			DbgTrollKey.Reset();
		}
		else if (Cmd.StartsWith(TEXT("buff=")))
		{
			// buff=diner.coffee: as if bought at the counter
			const FString K = Cmd.Mid(5);
			if (KeysBuffs::Seconds(K) > 0) { Buffs.Add(K, GetWorld()->GetTimeSeconds() + KeysBuffs::Seconds(K)); ShowToast(KeysBuffs::StartText(K)); }
		}
		else if (Cmd == TEXT("keep")) KeepFish();
		else if (Cmd == TEXT("letgo")) ReleaseFish();
		else if (Cmd == TEXT("tagrel")) TagAndRelease();
		else if (Cmd == TEXT("harbour")) { X = AKeysEvents::HarbourX(); Z = AKeysEvents::HarbourZ(); Speed = Thr = 0; bCamInit = false; }
		else if (Cmd.StartsWith(TEXT("ev:"))) { if (AKeysEvents* Ev = AKeysEvents::Get(this)) Ev->DebugCmd(Cmd.Mid(3)); }
		else if (Cmd == TEXT("bigmap")) bBigMap = !bBigMap;
		else if (Cmd.StartsWith(TEXT("pirates"))) { if (AKeysVisitors* Vs = AKeysVisitors::Get(this)) Vs->SummonPirates(Cmd.Len() > 8 ? FCString::Atod(*Cmd.Mid(8)) : 300); }   // pirates or pirates=120
		else if (Cmd.StartsWith(TEXT("lc:"))) { if (AKeysLocals* Lc = AKeysLocals::Get(this)) { const FString R = Lc->DebugCmd(Cmd.Mid(3)); UE_LOG(LogTemp, Log, TEXT("KRScript lc %s"), *R); } }
		else if (Cmd.StartsWith(TEXT("dk:"))) { if (AKeysDocks* Dk = AKeysDocks::Get(this)) { const FString R = Dk->DebugCmd(Cmd.Mid(3)); UE_LOG(LogTemp, Log, TEXT("KRScript dk %s"), *R); } }   // the docks' test hooks (KeysDocks.h)
		else if (Cmd.StartsWith(TEXT("yd:"))) { if (AKeysYards* Yd = AKeysYards::Get(this)) { const FString R = Yd->DebugCmd(Cmd.Mid(3)); UE_LOG(LogTemp, Log, TEXT("KRScript yd %s"), *R); } }   // the yards' test hooks (KeysYards.h)
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
		else if (Cmd.StartsWith(TEXT("clock=")))
		{
			// clock=HH:MM fakes the hour the fish feed by; clock=off goes back to the real clock
			FString Hh, Mm;
			if (Cmd.Mid(6).Split(TEXT(":"), &Hh, &Mm)) KeysFeed::SetClockOverride(FCString::Atoi(*Hh) + FCString::Atoi(*Mm) / 60.0);
			else KeysFeed::SetClockOverride(-1);
			FeedT = 0;
		}
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
		else if (Cmd.StartsWith(TEXT("reel="))) { bScriptReelAuto = Cmd.Mid(5) == TEXT("auto"); bScriptReel = !bScriptReelAuto && FCString::Atoi(*Cmd.Mid(5)) != 0; }   // reel=1 holds S; reel=auto reels like a player watching the bar
		else if (Cmd == TEXT("engines")) { if (AKeysAudio* Au = AKeysAudio::Get(this)) Au->ToggleEngines(); }   // the I key
		else if (Cmd == TEXT("hulllog")) bScriptLog = true;   // KRHull lines ten times a second
		else if (Cmd == TEXT("wakepass"))
		{
			// a 30 ft boat's wake laid 30 m off the starboard beam, as if one had just run past at 24 kn
			const double Fwx = -FMath::Sin(H), Fwz = -FMath::Cos(H), Rwx = FMath::Cos(H), Rwz = -FMath::Sin(H);
			for (int32 i = -20; i <= 20; i++) KeysGeo::EmitWake(X + Rwx * 30 + Fwx * i * 2.5, Z + Rwz * 30 + Fwz * i * 2.5, 12, 9, GetWorld()->GetTimeSeconds(), .2);
		}
		else if (Cmd.StartsWith(TEXT("heading=")))
		{
			// heading=head|follow|beam: point the bow into, with or across the sea (it runs with the wind); heading=<radians>
			const FString W = Cmd.Mid(8);
			const double Wd = WindDir();
			const double Dx = W == TEXT("follow") ? FMath::Cos(Wd) : W == TEXT("beam") ? -FMath::Sin(Wd) : -FMath::Cos(Wd);
			const double Dz = W == TEXT("follow") ? FMath::Sin(Wd) : W == TEXT("beam") ? FMath::Cos(Wd) : -FMath::Sin(Wd);
			H = FMath::Atan2(-Dx, -Dz);   // forward is (-sin H, -cos H)
			if (W.IsNumeric()) H = FCString::Atod(*W);   // heading=4.71: that heading, in radians as tp= takes it
		}
		else if (Cmd.StartsWith(TEXT("trim="))) { TrimDeg = FCString::Atod(*Cmd.Mid(5)); bScriptLog = true; }   // trim=<degrees>, logs KRTrim twice a second
		else if (Cmd.StartsWith(TEXT("tabs=")))
		{
			// tabs=<port>,<starboard> 0..1
			FString A, B;
			if (Cmd.Mid(5).Split(TEXT(","), &A, &B)) { TabL = FMath::Clamp(FCString::Atod(*A), 0.0, 1.0); TabR = FMath::Clamp(FCString::Atod(*B), 0.0, 1.0); }
		}
		else if (Cmd.StartsWith(TEXT("steer="))) ScriptSteer = FCString::Atod(*Cmd.Mid(6));   // steer=0.3 (left positive)
		else if (Cmd.StartsWith(TEXT("creek="))) GScriptCreek = FCString::Atoi(*Cmd.Mid(6));   // creek=1: hold her on that mangrove creek's line to its head, then stop (KeysMangroveLife::Creeks(); -1 lets go)
		else if (Cmd.StartsWith(TEXT("catch=")))
		{
			// land a fish of that species (a fifth of the way up its size range) and show the catch card; catch=tourney: the tournament's first
			if (FState != EKeysFish::Idle) ReelIn(FString());
			// catch=key[:lb[:cast|troll|spear]] for the charter tests
			TArray<FString> Parts;
			Cmd.Mid(6).ParseIntoArray(Parts, TEXT(":"));
			Key = Parts.Num() ? Parts[0] : FString();
			const double ForceLb = Parts.Num() > 1 ? FCString::Atod(*Parts[1]) : 0;
			const FString How = Parts.Num() > 2 ? Parts[2] : FString();
			if (How == TEXT("spear") && Species(Key)) { LandSpeared(Key, ForceLb > 0 ? ForceLb : 2, X + FMath::Cos(H) * 4, Z - FMath::Sin(H) * 4); Key = Parts[0]; }
			else if (Key == TEXT("tourney")) if (const AKeysEvents* Ev = AKeysEvents::Get(this)) Key = Ev->FirstSpecies();   // the first of the tournament's five
			if (How != TEXT("spear")) Sp = Species(Key);
			if (Sp && How != TEXT("spear"))
			{
				Weight = ForceLb > 0 ? ForceLb : FMath::RoundToDouble((Sp->Min + (Sp->Max - Sp->Min) * .2) * 10) / 10;
				FX = X + FMath::Cos(H) * 8; FZ = Z - FMath::Sin(H) * 8;
				Bob = FVector(FX, 0, FZ);
				PickRecapture();
				BuildHookedMesh();
				if (How == TEXT("troll")) HookedStation = 0;   // as if it came off the spread
				Land();
				HookedStation = -1;
			}
		}
		else if (Cmd.StartsWith(TEXT("fin:"))) DebugFinal(Cmd.Mid(4));
		else if (Cmd.StartsWith(TEXT("storm=")) || Cmd.StartsWith(TEXT("weather=")) || Cmd.StartsWith(TEXT("spout")) || Cmd.StartsWith(TEXT("bolt")) || Cmd.StartsWith(TEXT("wxskip=")))
		{
			// the weather's test hooks (KeysWeather.h DebugCmd)
			// (called outside the log: a shipping build compiles UE_LOG out, arguments and all)
			if (AKeysWeather* Wx = AKeysWeather::Get(this)) { const FString R = Wx->DebugCmd(Cmd); UE_LOG(LogTemp, Log, TEXT("KRScript %s -> %s"), *Cmd, *R); }
		}
		else if (Cmd == TEXT("seasnap")) SnapSea();
		else if (Cmd.StartsWith(TEXT("chart="))) ChartRange = Clamp(FCString::Atod(*Cmd.Mid(6)), 120, 2400);   // chart=2400: the plotter's half-range, m   // the sea jumps to what the wind would raise (no easing)
		else if (Cmd.StartsWith(TEXT("gps="))) bElectronics = FCString::Atoi(*Cmd.Mid(4)) != 0;   // gps=1: the chartplotter (G) on
		else if (Cmd.StartsWith(TEXT("touch="))) AKeysHUD::ScriptTouch(Cmd.Mid(6));   // hold an on-screen control (-KRTouch)
		else if (Cmd == TEXT("untouch")) AKeysHUD::ScriptTouchRelease();
		else if (Cmd.StartsWith(TEXT("sink="))) ScriptDescendUntil = T + FCString::Atod(*Cmd.Mid(5));   // sink=1.5: the diver swims down that many seconds
		else if (Cmd.StartsWith(TEXT("look=")))
		{
			FString A, B;
			if (Cmd.Mid(5).Split(TEXT(","), &A, &B)) AKeysHUD::ScriptLook(FCString::Atof(*A), FCString::Atof(*B));
		}
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
		bReelKey = bFwd = bBack = bLeft = bRight = bTrimUp = bTrimDn = bAction = false;
		bTrimMode = false;
		D->SetDefaultChase(bDiveChase);
		D->UnderwaterHaze = UnderwaterHaze;
		// sim/dive-gear.ts: the pony bottle is live only with an air fill in hand, and one fill is spent per dive at 2 m
		bTankLive = HasGear(TEXT("dive.tank")) && CountOf(TEXT("dive.fill")) > 0;
		bFillCharged = false;
		if (HasGear(TEXT("dive.tank")) && !bTankLive) ShowToast(TEXT("The pony bottle is empty - an air fill at Scuba Steve's makes it live again."), 4);
		D->AirMul = (bTankLive ? 2.2 : 1) * (HasGear(TEXT("dive.wetsuit")) ? 1.15 : 1) * (HasGear(TEXT("dive.fins")) ? 1.1 : 1) * (BuffOn(TEXT("diner.mahi")) ? 1.05 : 1);
		// the rest of the kit (web sim/shop.ts): fins swim faster, the wide mask sees further, the band gun reaches and
		// draws quicker, the reel conversion shortens a speared fish's run; every shot spends a shaft
		D->SwimMul = HasGear(TEXT("dive.fins")) ? 1.28 : 1;
		D->ViewBonusM = HasGear(TEXT("dive.mask")) ? 6 : 0;
		D->SpearRangeBonusM = HasGear(TEXT("spear.long")) ? 6 : 0;
		D->SpearDrawMul = (HasGear(TEXT("spear.long")) ? .65 : 1) * (HasGear(TEXT("spear.reel")) ? .85 : 1);
		D->SpearTetherMul = HasGear(TEXT("spear.reel")) ? .6 : 1;
		D->TakeShaft = [this]() -> int32
		{
			if (CountOf(TEXT("spear.shafts")) <= 0) { ShowToast(TEXT("Out of shafts - firing the bent spare (short range). Buy more at the gun counter."), 4); return -1; }
			AddCount(TEXT("spear.shafts"), -1);
			const int32 Left = CountOf(TEXT("spear.shafts"));
			UE_LOG(LogTemp, Log, TEXT("KRDive shaft spent: %d left"), Left);
			if (Left == 0) ShowToast(TEXT("Last shaft spent. Next shot is the bent spare (short range)."), 3.5);
			else if (Left == 1) ShowToast(TEXT("1 shaft left."), 2.5);
			return Left;
		};
		D->EnterWater(X, Z, H);
		ShowToast(TEXT("Overboard - WASD and the mouse to swim and look, Space / C to ascend / descend, Shift to sprint. Click to fire, R to reload. J to get back aboard from anywhere."), 6);
	}
	else if (D->RequestReboard()) { SyncChaseCam(); bCamInit = false; }
	else if (ReboardT < 0 && !D->IsBlackedOut())
	{
		// out of reach of the ladder: a short fade to black and you're back aboard, wherever you were (Tick finishes it)
		ReboardT = 0;
	}
}

void AKeysBoatPawn::HandleDiveInput(APlayerController* PC, float Dt)
{
	auto Down = [PC](const FKey& K) { return PC->IsInputKeyDown(K); };
	auto Hit = [PC](const FKey& K) { return PC->WasInputKeyJustPressed(K); };
	AKeysDiver* D = AKeysDiver::Get(this);
	bReelKey = bFwd = bBack = bLeft = bRight = bTrimUp = bTrimDn = bAction = false;
	bCharging = false;
	if (!D) return;
	FKeysDiverButtons B;
	B.bFwd = Down(EKeys::W) || Down(EKeys::Up);
	B.bBack = Down(EKeys::S) || Down(EKeys::Down);
	B.bLeft = Down(EKeys::A) || Down(EKeys::Left);
	B.bRight = Down(EKeys::D) || Down(EKeys::Right);
	B.bAscend = Down(EKeys::SpaceBar);
	B.bDescend = Down(EKeys::C) || Down(EKeys::LeftControl) || Down(EKeys::RightControl) || GetWorld()->GetRealTimeSeconds() < ScriptDescendUntil;
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
	bLandingSpear = true;
	Land();
	bLandingSpear = false;
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
		if (!W->TryEnter(X, Z, H, Speed, Spec().Len, Why)) { UE_LOG(LogTemp, Warning, TEXT("KeysRun: can't go ashore at %.0f %.0f: %s"), X, Z, *Why); ShowToast(Why); return; }
		if (bCharging || IsLineOut()) ReelIn(FString());
		bCharging = false;
		bReelKey = bFwd = bBack = bLeft = bRight = bTrimUp = bTrimDn = bAction = false;
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
	bReelKey = bFwd = bBack = bLeft = bRight = bTrimUp = bTrimDn = false;
	if (!W) return;
	// a rod out from the planks (shore.ts shoreHoldsStill): the angler stands still from the charge to the card,
	// S (or holding Space in a fight) reels, R reels in, K / Space / T deal with the catch; the mouse still looks round
	if (!W->bShopOpen && (bCharging || FState != EKeysFish::Idle))
	{
		FKeysWalkButtons B;
		float MX = 0, MY = 0;
		PC->GetInputMouseDelta(MX, MY);
		B.LookYaw = MX * .004;
		B.LookPitch = MY * .004;
		W->SetButtons(B);
		bBack = Down(EKeys::S);
		bReelKey = Down(EKeys::Down);   // reel: the down arrow
		bAction = Down(EKeys::SpaceBar) && FState == EKeysFish::Fight;
		if (Hit(EKeys::SpaceBar)) { bAction = true; OnAction(); }
		if (PC->WasInputKeyJustReleased(EKeys::SpaceBar) && bCharging) { bCharging = false; BeginShoreSwap(); CastLine(Charge); EndShoreSwap(); }
		if (Hit(EKeys::R) && (FState == EKeysFish::Waiting || FState == EKeysFish::Casting || FState == EKeysFish::Nibble)) ReelIn(TEXT("Line reeled in. Walk on when you like."));
		if (FState == EKeysFish::Caught)
		{
			if (Hit(EKeys::K) && GetWorld()->GetTimeSeconds() - CaughtT > .7) KeepFish();
			else if (Hit(EKeys::T) && GetWorld()->GetTimeSeconds() - CaughtT > .7) TagAndRelease();
		}
		if (Hit(EKeys::T) && FState == EKeysFish::Fight)
			Drag = FMath::Clamp(Drag + (Down(EKeys::LeftShift) || Down(EKeys::RightShift) ? -1 : 1), 1, 5);
		if (Hit(EKeys::Q)) ShowToast(TEXT("Reel in first (R) - or land that fish."));
		if (Hit(EKeys::M)) bBigMap = !bBigMap;
		return;
	}
	bAction = false;
	bCharging = false;
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
	bShoreCanFish = ShoreFishable();
	if (Hit(EKeys::SpaceBar))
	{
		if (bShoreCanFish) { bFpOn = false; StartCharge(); }
		else ShowToast(TEXT("Walk out onto a pier, a dock or the marina's planks, facing the water, to fish."));
	}
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
	if (W->NearText().StartsWith(TEXT("MENU|"))) { MenuOpen = MenuOpen.IsEmpty() ? W->NearText().Mid(5) : FString(); return; }
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

void AKeysBoatPawn::RunDests(TArray<FRunDest>& Out, bool bOnlyFound) const
{
	Out.Reset();
	ON_SCOPE_EXIT
	{
		// the wrecks, then (for the teleport list) only the places you've been near
		if (bOnlyFound) Out.RemoveAll([this](const FRunDest& D) { return !SpotsFound.Contains(D.Name); });
	};
	for (const KeysWrecks::FWreck& W : KeysWrecks::Wrecks()) Out.Add({FString(W.Name), TEXT("wreck"), W.X + 30, W.Z});
	for (const FMarina& M : Marinas()) Out.Add({M.Name, TEXT("marina"), M.SX, M.SZ + M.Dir * 48});
	if (RealMap()) Out.Add({TEXT("Sombrero Reef"), TEXT("the crest"), -1800, 2790}); else Out.Add({TEXT("Sombrero Reef"), TEXT("the crest"), 60, 1462});
	if (RealMap())
	{
		// the mangrove country round Boot Key (creek lines: KeysMangroveLife.cpp)
		Out.Add({TEXT("Sister Creek"), TEXT("mangrove creek"), -272, 426});
		Out.Add({TEXT("Whiskey Creek"), TEXT("mangrove tunnel"), -292, 462});
		Out.Add({TEXT("Sister Creek mouth"), TEXT("tarpon at first and last light"), -342, 792});
		Out.Add({TEXT("Boot Key flat"), TEXT("mangrove flat"), -1150, 452});
	}
	for (const FHump& Hm : Humps()) Out.Add({Hm.Name, Hm.bPatch ? TEXT("patch reef") : TEXT("offshore hump"), Hm.X, ChainZ(Hm.X) + Hm.DZ});
	// the Lower Keys and Key Largo (KeysGeoReal::Sites, from Tools/geo/sites_p3.py): the reefs by name, and the open
	// water asked for by name off the towns and harbors
	if (RealMap())
		for (const KeysGeoReal::FSite& S : KeysGeoReal::Sites())
		{
			if (S.Kind == TEXT("reef")) Out.Add({S.Name, TEXT("the reef"), S.X, S.Z - 90});
			else if (S.Kind == TEXT("spot")) Out.Add({S.Name, TEXT("open water"), S.X, S.Z});
		}
	if (const AKeysLife* L = AKeysLife::Get(this))
	{
		TArray<FKeysMarker> Mk;
		L->GetMarkers(Mk);
		int32 N = 0;
		for (const FKeysMarker& M : Mk) if (M.Kind == 0) Out.Add({N++ == 0 ? FString(TEXT("The production platform")) : FString::Printf(TEXT("Oil rig %d"), N), TEXT("oil rig"), M.X + 130, M.Z});
	}
}

// The tank: realistic fuel burns with the throttle (a full tank is about 25 minutes flat out) and fills up for free
// whenever you're slow at a marina's slips. Unlimited (the default) never drains.
void AKeysBoatPawn::UpdateFuel(double Dt)
{
	if (bUnlimitedFuel) { Fuel = 1; return; }
	const double Burn = (.05 + .95 * FMath::Pow(FMath::Abs(Thr), 1.5)) / 1500.0;
	if (bRunning) Fuel = FMath::Max(0.0, Fuel - Burn * TrimFuelK * Dt);   // trim near the sweet spot saves fuel
	if (NearMarina(X, Z) && FMath::Abs(Speed) < 1.5 && Fuel < 1)
	{
		Fuel = FMath::Min(1.0, Fuel + Dt / 12);
		if (Fuel >= 1) ShowToast(TEXT("Topped up at the fuel dock."));
		bFuelOutMsg = false;
	}
	if (Fuel <= 0 && !bFuelOutMsg) { bFuelOutMsg = true; ShowToast(TEXT("Out of fuel. Teleport to a marina from the menu (Esc), or turn on unlimited fuel in Settings."), 6); }
}

// discovery: the first time you come within 200 m of a marina, reef, wreck or rig it joins the teleport list. The
// marina nearest where you first start is known from the outset (it's your home port).
void AKeysBoatPawn::UpdateDiscovery(double Dt)
{
	DiscoverT -= Dt;
	if (DiscoverT > 0 || !bRunning) return;
	DiscoverT = 1;
	TArray<FRunDest> Ds;
	RunDests(Ds);
	if (!SpotsFound.Num())
	{
		const FRunDest* Home = nullptr; double Best = 1e18;
		for (const FRunDest& D : Ds) if (D.Note == TEXT("marina")) { const double Dd = FMath::Square(D.X - X) + FMath::Square(D.Z - Z); if (Dd < Best) { Best = Dd; Home = &D; } }
		if (Home) SpotsFound.Add(Home->Name);
	}
	for (const FRunDest& D : Ds)
	{
		if (SpotsFound.Contains(D.Name) || FMath::Square(D.X - X) + FMath::Square(D.Z - Z) > 200.0 * 200.0) continue;
		SpotsFound.Add(D.Name);
		ShowToast(FString::Printf(TEXT("Discovered %s (%s) - you can teleport here from the menu."), *D.Name, *D.Note), 4);
		SaveEconomy();
	}
}

// Free Fish starts you in the best boat you own: the dearest, and the longest of those
void AKeysBoatPawn::PickBestOwnedBoat()
{
	const TArray<FBoatSpec>& B = Boats();
	int32 Best = -1;
	for (int32 i = 0; i < B.Num(); i++)
	{
		if (!OwnsBoat(B[i].Id)) continue;
		if (Best < 0 || B[i].Price > B[Best].Price || (B[i].Price == B[Best].Price && B[i].Len > B[Best].Len)) Best = i;
	}
	if (Best >= 0 && Best != BoatIndex) { BoatIndex = PickIndex = Best; Rebuild(); }
}

// Where a teleport puts the boat: clear water near the spot (deep enough for this hull, off the shore, clear of the
// marina docks and bridge piers: AKeysTraffic::SpotClear) and well clear of every oil rig's legs and platform.
bool AKeysBoatPawn::TeleportSpot(const FRunDest& D, double& OX, double& OZ) const
{
	TArray<FVector2D> Rigs;
	if (const AKeysLife* L = AKeysLife::Get(this))
	{
		TArray<FKeysMarker> Mk;
		L->GetMarkers(Mk);
		for (const FKeysMarker& M : Mk) if (M.Kind == 0) Rigs.Add(FVector2D(M.X, M.Z));
	}
	constexpr double RigClear = 110;   // the biggest platform's half-diagonal plus the boat and a margin
	auto NearRig = [&](double PX, double PZ) { for (const FVector2D& R : Rigs) if (FVector2D::Distance(R, FVector2D(PX, PZ)) < RigClear) return true; return false; };
	for (int32 Try = 0; Try < 6; Try++)
	{
		double TX = D.X, TZ = D.Z;
		if (Try > 0) { const double A = Try * 1.05; TX += FMath::Cos(A) * 60 * Try; TZ += FMath::Sin(A) * 60 * Try; }
		if (!AKeysTraffic::FindClearSpot(TX, TZ, Spec())) continue;
		if (NearRig(TX, TZ))
		{
			// push straight out from the rig, then find clear water again
			for (const FVector2D& R : Rigs)
			{
				const FVector2D V(TX - R.X, TZ - R.Y);
				if (V.Size() < RigClear) { const FVector2D Dir = V.Size() > 1 ? V.GetSafeNormal() : FVector2D(1, 0); TX = R.X + Dir.X * (RigClear + 20); TZ = R.Y + Dir.Y * (RigClear + 20); }
			}
			if (!AKeysTraffic::FindClearSpot(TX, TZ, Spec()) || NearRig(TX, TZ)) continue;
		}
		OX = TX; OZ = TZ;
		return true;
	}
	return false;
}

void AKeysBoatPawn::RunTo(int32 I)
{
	TArray<FRunDest> Ds;
	RunDests(Ds, true);
	if (!Ds.IsValidIndex(I)) return;
	// refuse while something is going on that teleporting would spoil
	const AKeysEvents* Ev = AKeysEvents::Get(this);
	FString Why;
	if (IsBusy(Why) || IsLineOut()) { if (Why.IsEmpty()) Why = TEXT("Reel in first."); }
	else if (Ev && (Ev->IsRacing() || (Ev->InTourney() && Ev->TourneyPhase() != AKeysEvents::ETourney::Over))) Why = TEXT("Not in the middle of a race or a tournament.");
	if (!Why.IsEmpty()) { Menu = EMenu::None; ShowToast(Why); return; }
	double NX, NZ;
	if (!TeleportSpot(Ds[I], NX, NZ)) { Menu = EMenu::None; ShowToast(FString::Printf(TEXT("No clear water to put the boat at %s."), *Ds[I].Name)); return; }
	StopTroll(true);
	X = NX; Z = NZ;
	Speed = Thr = DVX = DVZ = 0;
	bCamInit = false;
	if (AKeysWorld* Wd = World()) { Wd->FocusX = X; Wd->FocusZ = Z; }
	Menu = EMenu::None;
	ShowToast(FString::Printf(TEXT("Teleported to %s."), *Ds[I].Name));
}

void AKeysBoatPawn::PrepareToLeaveHelm()
{
	if (bCharging || IsLineOut()) ReelIn(FString());
	bCharging = false;
	StopTroll(true);
	bReelKey = bFwd = bBack = bLeft = bRight = bTrimUp = bTrimDn = bAction = false;
	bTrimMode = false;
	ScriptThr = -1;
}

bool AKeysBoatPawn::InChair() const
{
	return KeysMesh::HasFightChair(Spec()) && FState == EKeysFish::Fight && HookedStation >= 0 && !IsDiving() && !IsAshore();
}

FVector AKeysBoatPawn::ChairEyeLocal() const
{
	// the angler's own eyes as posed this frame (he leans back and pumps); before he's built, about where they'll be
	FVector E;
	if (const AKeysExtras* Ex = AKeysExtras::Get(this); Ex && Ex->ChairEye(E)) return E;
	return KeysMesh::FightChairBase(Spec()) + FQuat(FVector::UpVector, ChairYaw).RotateVector(FVector(.2, 0, 1.15));
}

bool AKeysBoatPawn::FpEyeLocal(FVector& Out) const
{
	Out = FpEyeL;
	return bFpOn && FishCamT < 0;   // (during the fish cam you are back in the shot, rod in hand)
}

bool AKeysBoatPawn::ExternalCamera(FVector& OutPos, FRotator& OutRot, float& OutFov) const
{
	if (bDbgCam && BoatMesh)
	{
		const FTransform BT = BoatMesh->GetComponentTransform();
		OutPos = BT.TransformPosition(DbgCamPos * 100.0);
		OutRot = (BT.GetRotation() * DbgCamRot.Quaternion()).Rotator();
		OutFov = DbgCamFov;
		return true;
	}
	if (InChair() && BoatMesh)
	{
		const FTransform BT = BoatMesh->GetComponentTransform();
		const FVector Base = KeysMesh::FightChairBase(Spec());
		const FQuat Q(FVector::UpVector, ChairYaw);
		const FVector FishW = ToUE(FX, 0, FZ);
		// first person (V): the seated angler's own view down the rod (UpdateCamera) once the opening shot is done;
		// otherwise the cinematic below
		if (bFpPref && ChairIntroT <= 0) return false;
		// the chair fight from outside the boat, low off the stern quarter: side-on to the angler, so he, the chair and
		// the bent rod fill the shot with the hull and the wake behind, and nothing (hardtop, tower, riggers) between.
		// The lens sits on the side away from the fish, so the rod and line run off into the frame, holding that side
		// until the fish has clearly crossed.
		const FBoatSpec& S = Spec();
		const double Sole = KeysMesh::SoleAt(S, Base.X);
		const FVector FishL = BT.InverseTransformPosition(FishW) / 100.0;
		if (ChairCamSide == 0) ChairCamSide = FishL.Y > 0 ? 1 : -1;
		else if (FishL.Y * ChairCamSide < -6) ChairCamSide = -ChairCamSide;
		const double A = ChairYaw + ChairCamSide * 2.1, R = 8.0;
		FVector LocalCam(Base.X + FMath::Cos(A) * R, Base.Y + FMath::Sin(A) * R, Sole + 3.0);
		LocalCam.X = FMath::Min(LocalCam.X, Base.X + 2.6);   // never forward, toward the console and hardtop
		LocalCam.Y = FMath::Sign(LocalCam.Y == 0 ? ChairCamSide : LocalCam.Y) * FMath::Max(FMath::Abs(LocalCam.Y), S.Beam * .5 + 3.0);
		OutPos = BT.TransformPosition(LocalCam * 100.0);
		// aim at the angler's chest, nudged a little out toward the fish so the rod tip and the line stay in frame
		const FVector Head = BT.TransformPosition((Base + FVector(0, 0, .75)) * 100.0);
		const FVector ToFish = (FishW - Head).GetSafeNormal2D();
		const FVector Look = Head + ToFish * 120.0;
		OutRot = (Look - OutPos).Rotation();
		OutFov = 46.f;
		return true;
	}
	ChairCamSide = 0;
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
	Save->bFreeFish = bFreeFish; Save->bUnlimitedFuel = bUnlimitedFuel; Save->Fuel = Fuel; Save->SpotsFound = SpotsFound;
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
	PC->bEnableTouchEvents = bWant;   // a tap on a menu row (phones and tablets)
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
		bReelKey = bFwd = bBack = bLeft = bRight = bTrimUp = bTrimDn = bAction = false;
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
		bReelKey = bFwd = bBack = bLeft = bRight = bTrimUp = bTrimDn = bAction = false;
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
	if (Hit(EKeys::B)) { bBoardOpen = true; BoardTab = 0; bReelKey = bFwd = bBack = bLeft = bRight = bAction = false; bCharging = false; return; }

	// the gear screen: Tab opens and closes it (Esc too), nothing else reacts while it is up
	if (bRunning && Menu == EMenu::None && !IsDiving())
	{
		if (bGearOpen) { if (Hit(EKeys::Tab) || Hit(EKeys::Escape)) bGearOpen = false; bReelKey = bFwd = bBack = bLeft = bRight = bAction = false; return; }
		if (Hit(EKeys::Tab)) { if (AKeysWalker* Wk = AKeysWalker::Get(this); !Wk || !Wk->bShopOpen) { bGearOpen = true; return; } }
	}
	// a slot machine in the Gaff & Gill: it owns the keys while you sit at it
	if (bRunning && Menu == EMenu::None)
		if (AKeysSlots* Sl = AKeysSlots::Get(this); Sl && Sl->bOpen)
		{
			bReelKey = bFwd = bBack = bLeft = bRight = bTrimUp = bTrimDn = bAction = false;
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
				bReelKey = bFwd = bBack = bLeft = bRight = bTrimUp = bTrimDn = bAction = false;
				bCharging = false;
				Es->HandleInput(PC);
				return;
			}
		}
	// the boat pistol owns the keys while it is out (web entities/handgun/input.ts): the boat drifts in neutral
	if (bRunning && Menu == EMenu::None)
		if (AKeysHandgun* Gun = AKeysHandgun::Get(this); Gun && Gun->IsOut())
		{
			bReelKey = bFwd = bBack = bLeft = bRight = bTrimUp = bTrimDn = bAction = false;
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
		bReelKey = bFwd = bBack = bLeft = bRight = bTrimUp = bTrimDn = bAction = false;
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
	if (bScriptReelAuto) bAutoReelOn = FState == EKeysFish::Fight && (bAutoReelOn ? Tension < .84 : Tension < .55);   // ease off in the red, wind again once it drops
	// reel on the down arrow (it backs the boat down only with no line out); S is the throttle astern
	bReelKey = (IsLineOut() && Down(EKeys::Down)) || bScriptReel || bAutoReelOn;
	bBack = Down(EKeys::S) || (!bTrimMode && !IsLineOut() && Down(EKeys::Down));
	bLeft = Down(EKeys::A) || (!bTrimMode && Down(EKeys::Left));
	bRight = Down(EKeys::D) || (!bTrimMode && Down(EKeys::Right));
	bTabLeft = bTrimMode && Down(EKeys::Left);     // trim mode: the arrows work the tabs, A/D still steer
	bTabRight = bTrimMode && Down(EKeys::Right);
	if (FParse::Param(FCommandLine::Get(), TEXT("KRDemo")))
	{
		// full throttle, with a gentle turn every so often so the wake curves
		bFwd = true;
		const double Fdx = -FMath::Sin(H), Fdz = -FMath::Cos(H);
		bLeft = FMath::Fmod(T, 6.0) > 4.3 || DepthFast(X + Fdx * 150, Z + Fdz * 150) < 6 || DepthFast(X + Fdx * 60, Z + Fdz * 60) < 6;
	}
	bTrimUp = bTrimMode && Down(EKeys::Up);
	bTrimDn = bTrimMode && !IsLineOut() && Down(EKeys::Down);

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
		if (AKeysEvents* Ev = AKeysEvents::Get(this))
		{
			// Free Fish has no clocks: the race is a Charter Captain thing (but one already running can still be abandoned)
			if (bFreeFish && Ev->RacePhase() != AKeysEvents::ERace::Countdown && Ev->RacePhase() != AKeysEvents::ERace::Racing)
				ShowToast(TEXT("No races in Free Fish - switch to Charter Captain on the main menu for timed events."));
			else { StopTroll(true); Ev->ToggleRace(); }   // the Marathon race (web game/race)
		}
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
			ShowToast(bTrimMode ? TEXT("Trim mode on - Up/Down trim the motors (slow down to tilt them up), Left/Right lift that side with the tabs. A/D steer, W/S throttle. T to exit.") : TEXT("Trim mode off."));
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
	if (!bAutoChase || !bCanDrive || FState != EKeysFish::Fight || bFwd || bLeft || bRight) { bAutoOn = false; BackK = 0; bBackBurst = false; BackBurstT = 0; return false; }
	const double DX = FX - X, DZ = FZ - Z, Dist = Hyp(DX, DZ);
	const double Want = FMath::Atan2(-DX, -DZ);
	const double Diff = WrapPi(Want - H), DiffStern = WrapPi(Want - (H + PI));
	double T = 0, St = 0;
	const double Dt = FMath::Min(.1, double(GetWorld()->GetDeltaSeconds()));
	// keep the stern squared to the line; for a genuinely heavy fish (150 lb and up: big tuna, marlin, the larger
	// swordfish) that is running off line, or hanging far astern where the reel can't gain on it, back down on it the
	// way a sportfisher does: slowly, a couple of knots at most, in bursts that ease on and off, and never once it has
	// stopped running or is close. Light and medium fish are only ever held stern-to. Only a fish far out ahead gets the
	// bow turned on it and run up on.
	if (!(FMath::Abs(Diff) < .9 && Dist > 90))
	{
		// a hull turns the other way going astern: steer so the stern swings onto the fish either way
		St = Clamp(DiffStern * 2.2, -1, 1) * (Speed < -.2 ? -1 : 1);
		const bool bHeavy = Weight >= 150;
		const bool bWant = bHeavy && Dist > 18 && FMath::Abs(DiffStern) < .7 && (bRunningFish || Dist > 75);
		// bursts: a few seconds astern, then a breather in neutral to let the angler gain, for as long as it is wanted
		BackBurstT -= Dt;
		if (!bWant) { bBackBurst = false; BackBurstT = 0; }
		else if (BackBurstT <= 0) { bBackBurst = !bBackBurst; BackBurstT = bBackBurst ? Rand(3.0, 5.5) : Rand(1.5, 3.0); }
		BackK += ((bWant && bBackBurst ? 1.0 : 0.0) - BackK) * FMath::Min(1.0, Dt * (bWant && bBackBurst ? .8 : 1.4));
		// 0.5 - 1 m/s astern (1 - 2 knots), harder for a bigger fish and while it is taking line fast
		const double WK = FMath::Clamp((Weight - 150) / 450, 0.0, 1.0);
		const double RunSp = bRunningFish ? (3 + Str * 6.5) * (.4 + .6 * Stamina) : 0;
		const double V = FMath::Lerp(.5, .85, WK) + FMath::Min(.18, RunSp * .015);
		T = BackK > .02 ? -BackK * FMath::Min(1.03, V) / TopMs : 0;
		AutoMode = BackK > .15 ? TEXT("Backing down on a big fish") : TEXT("Holding the stern on the fish");
		if (BackK > .15 && !bBackMsg) { bBackMsg = true; ShowToast(TEXT("Big fish - backing down on it.")); }
	}
	else
	{
		BackK = 0; bBackBurst = false; BackBurstT = 0;
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
	if (bScriptLog && (AcLogT -= Dt) <= 0)
	{
		AcLogT = 1;
		UE_LOG(LogTemp, Log, TEXT("KRDBG chase %s %.0flb dist %.1f run %d stern-off %.2f speed %.2f thr %.3f %s"), *Key, Weight, Dist, bRunningFish ? 1 : 0, DiffStern, Speed, T, *AutoMode);
	}
	OutThr = T;
	OutSteer = St;
	return true;
}

void AKeysBoatPawn::Slam(double V)
{
	// trimmed out she lands harder, trimmed in she knifes through (UpdateTrim's TrimSlamK); the hull takes the strain
	const double K = Clamp(-V / 6 * TrimSlamK, 0, 1);
	if (AKeysAudio* A = AKeysAudio::Get(this)) A->Slam(K);
	HullStress += K * K * .065 * TrimSlamK;
	if (bScriptLog) UE_LOG(LogTemp, Log, TEXT("KRTrim slam k %.2f trimK %.2f stress %.2f"), K, TrimSlamK, HullStress);
	if (HullStress > 1)
	{
		HullStress = .35;
		const int32 Cost = TrimProfile().StressRepairCost;
		FString Msg = TEXT("CRACK - you've cracked a stringer slamming her. Trim down and ease off in a head sea.");
		if (!bFreeFish && Cost > 0) { const int64 Bill = FMath::Min<int64>(Cost, Cash); Cash -= Bill; SaveEconomy(); Msg += FString::Printf(TEXT(" Repair: $%lld."), Bill); }
		ShowToast(Msg, 6);
	}
	else if (HullStress > .7 && GetWorld()->GetTimeSeconds() - StressMsgT > 20) { StressMsgT = GetWorld()->GetTimeSeconds(); ShowToast(TEXT("The hull's taking a beating - trim down and back off a little.")); }
	Shake = FMath::Max(Shake, K);
	Speed *= 1 - .06 * K;
	// sheets of spray thrown off both bow chines, bigger the harder she hit
	if (AKeysLife* L = AKeysLife::Get(this); L && K > .12)
	{
		const FBoatSpec& S = Spec();
		const double Fx = -FMath::Sin(H), Fz = -FMath::Cos(H), Rx = FMath::Cos(H), Rz = -FMath::Sin(H);
		const FVector2D BV(Fx * Speed, Fz * Speed);
		for (const double Sd : {-1.0, 1.0})
		{
			const double BX = X + Fx * S.Len * .3 + Rx * Sd * S.Beam * .5, BZ = Z + Fz * S.Len * .3 + Rz * Sd * S.Beam * .5;
			L->BowSpray(FVector(BX, WaveH(BX, BZ, GetWorld()->GetTimeSeconds(), AmpAt(BX, BZ)) + .1, BZ), FVector2D(Rx * Sd, Rz * Sd), BV, 30 + int32(K * 70), .8 + K * 1.4);
		}
	}
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
	if (OutOfFuel()) TT = 0;   // dry tank: the engines idle and die
	double AcThr = 0, AcSteer = 0;
	const bool bAc = AutoChase(TopMs, bCanDrive, AcThr, AcSteer);
	if (bAc) TT = AcThr;
	double TrollT = 0;
	if (!bAc && bCanDrive && TrollThrottle(TopMs, TrollT)) { TT = TrollT; Gear = 0; }   // trolling holds seven knots
	else if (!bAc && bCanDrive && ScriptThr > -.5) TT = FMath::Max(ScriptThr, -.35);   // -1: no hold; down to -0.35 backs down
	if (OutOfFuel()) { TT = 0; ScriptThr = -1; }
	Thr += (TT - Thr) * FMath::Min(1.0, Dt * 2.5);
	UpdateFuel(Dt);
	UpdateDiscovery(Dt);

	// trim, tilt and tabs (KeysTrimPhysics.cpp): the sweet spot moves out as you get on plane; too far in plows the bow,
	// too far out porpoises and blows the props out; tilted up in the shallows it barely pushes
	UpdateTrim(Dt, T, TopMs);
	const double Target = Clamp(Thr * TopMs * TrimSpeedK, -TrimCapMs, TrimCapMs);
	Speed += (Target - Speed) * Dt * (FMath::Abs(Thr) < 1e-3 ? S.Accel * .5 : S.Accel * TrimHoleK);

	const double D = DepthFast(X, Z) + TideRise(), HullDraft = TrimProfile().HullDraftM;   // the tide floats her off, or sets her down
	if (D < HullDraft)
	{
		// run aground: she fetches up hard, mud boils up, and from there she only creeps (back off the way you came)
		if (!bAgroundPrev && FMath::Abs(Speed) > 3)
		{
			const double Hit = FMath::Min(1.0, FMath::Abs(Speed) / 14);
			Shake = FMath::Max(Shake, .35 + .55 * Hit);
			VP -= .5 * Hit;
			const double Now = GetWorld()->GetTimeSeconds();
			if (Now - AgroundMsgT > 8)
			{
				AgroundMsgT = Now;
				ShowToast(FString::Printf(TEXT("Hard aground in %.0f ft - she draws %.1f. Ease her back off the way you came, or tilt up and pole out."), D * 3.28, HullDraft * 3.28), 6);
			}
			if (AKeysLife* L = AKeysLife::Get(this))
			{
				const double BFx = -FMath::Sin(H), BFz = -FMath::Cos(H);
				for (const double Sd : {-1.0, 1.0})
					L->BowSpray(FVector(X + BFx * S.Len * .35, .1, Z + BFz * S.Len * .35), FVector2D(BFz * Sd, -BFx * Sd), FVector2D(BFx * Speed, BFz * Speed), int32(30 + 40 * Hit), 1.2);
			}
			Speed *= .25;
		}
		Speed = Clamp(Speed, -1.2, .9);
		bAgroundPrev = true;
	}
	else if (D > HullDraft + .08) bAgroundPrev = false;
	const double St = bAc ? AcSteer : bCanDrive ? (bLeft ? 1 : bRight ? -1 : ScriptSteer) : 0;
	if (GScriptCreek >= 0 && !KeysMangroveLife::Pilot(GScriptCreek, X, Z, H)) { GScriptCreek = -1; ScriptThr = 0; }   // test harness: creek=N
	Steer += (St - Steer) * FMath::Min(1.0, Dt * 4);
	const double F = FMath::Abs(Speed) / TopMs;
	H += S.Turn * Steer * (.3 + FMath::Min(1.0, FMath::Abs(Speed) / 8) * .9) * (1 - .35 * F) * (1.05 - .2 * TrimV) * TrimSteerK * (Speed < -.2 ? -1 : 1) * Dt + TrimYaw * Dt;
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
	// private docks, their lifts and the boats tied up at them (KeysDocks): she stops against them. A hull that is
	// already inside one (set down there by a test command) is let out again.
	if (RealMap())
		for (const double OffL : {0.0, S.Len * .42, -S.Len * .38})
		{
			const double Mg = S.Beam * .5 - .15;
			if (KeysDocks::InFootprint(NX + Fx * OffL, NZ + Fz * OffL, Mg) && !KeysDocks::InFootprint(X + Fx * OffL, Z + Fz * OffL, Mg))
			{
				NX = X; NZ = Z;
				Speed *= ((OffL >= 0) == (Speed >= 0)) ? .1 : .6;
				if (T - CollideMsgT > 3) { CollideMsgT = T; ShowToast(TEXT("Bumped a dock! Back off with S.")); }
				break;
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
	// rollers push the hull: running down the back of a swell speeds you up, climbing a face slows you, and a stopped boat surges with the swell
	const FVector2D Sl = WaveSlope(X, Z, T, Amp);
	const double Along = Sl.X * Fx + Sl.Y * Fz, BeamSl = Sl.X * Rx + Sl.Y * Rz;
	if (!bAir)
	{
		Speed -= Along * 9.8 * TrimProfile().SurfPush * Dt;   // surfing down the back of a swell, slowed climbing its face
		DVX -= Sl.X * 9.8 * .22 * Dt;
		DVZ -= Sl.Y * 9.8 * .22 * Dt;
		if (F < .25) H += BeamSl * .35 * Dt * (1 - F * 4);   // a slow boat gets nudged beam-to by the swell
	}
	DVX *= 1 - FMath::Min(1.0, Dt * .7);
	DVZ *= 1 - FMath::Min(1.0, Dt * .7);
	X += DVX * Dt;
	Z += DVZ * Dt;

	// heave, pitch and roll on the waves, the planing hump and lift, slams, stuffing, wakes and the weight aboard
	UpdateHullMotion(Dt, T, F, HullDraft);

	WakeT += Dt;
	// rings every 0.08 s instead of 0.35 s: the same wake height, but a smooth V instead of scalloped rings
	if (WakeT > .08 && FMath::Abs(Speed) > 2) { EmitWake(X - Fx * S.Len * .5, Z - Fz * S.Len * .5, FMath::Abs(Speed), S.Len, T, WakeT); WakeT = 0; }
	// spray off the bow chines on plane in a chop: the hull meeting each wave throws a sheet out to the sides, more as
	// the sea builds and the faster she goes (none when airborne or idling)
	SprayT += Dt;
	if (SprayT > .06)
	{
		SprayT = 0;
		const double Vs = FMath::Abs(Speed);
		// plowing throws it even in flat water; a lean (wrong tabs, wind) throws it off the low side
		const double Sk = bAir ? 0 : FMath::SmoothStep(7.0, 22.0, Vs) * (Clamp(WaveRms() * 3.2 + .15, 0, 1.4) + (TrimSprayK - 1) * .25) * (.6 + .4 * FMath::Abs(FMath::Sin(T * 2.7)));
		if (Sk > .08)
			if (AKeysLife* L = AKeysLife::Get(this))
			{
				const FVector2D BV(Fx * Speed, Fz * Speed);
				for (const double Sd : {-1.0, 1.0})
				{
					const double BX = X + Fx * S.Len * .22 + Rx * Sd * S.Beam * .48, BZ = Z + Fz * S.Len * .22 + Rz * Sd * S.Beam * .48;
					const double SideK = FMath::Max(.15, 1 + TrimSpraySide * Sd * 1.4);   // TrimSpraySide + = starboard low
					L->BowSpray(FVector(BX, WaveH(BX, BZ, T, AmpAt(BX, BZ)) + .05, BZ), FVector2D(Rx * Sd, Rz * Sd), BV, 3 + int32(Sk * 12 * SideK), (.3 + .5 * Sk) * FMath::Min(1.6, SideK));
				}
			}
	}
	// the white prop wash: wider and whiter as she gets on plane, none while the hull is airborne
	TrailT += Dt;
	if (TrailT > .07)
	{
		TrailT = 0;
		const double V = FMath::Abs(Speed);
		double Wash = bAir ? 0 : FMath::SmoothStep(2.0, 9.0, V) * (.55 + .45 * FMath::Min(1.0, F * 1.5));   // little white water off a displacement-speed hull (trolling)
		if (Speed < -.2 && !bAir) Wash = FMath::Max(Wash, .45 * BackK + .15 * FMath::SmoothStep(.2, 1.0, -Speed));   // backing down: the props churn white water up against the transom
		EmitTrail(0, X - Fx * S.Len * .48, Z - Fz * S.Len * .48, S.Beam * (.32 + .46 * FMath::Min(1.0, F * 1.4)), Wash, T);   // on plane the wash is wider than the transom
	}

	// pose the hull: bow up for positive pitch, starboard up for positive roll, lifted as she gets on plane
	const FVector Fw(Fx, Fz, 0), Rt(Rx, Rz, 0), Up(0, 0, 1);
	const double PosePitch = Pitch + TrimPorpPose;   // porpoising rides on top of the hull's pitch (UpdateTrim)
	const FVector Fp = Fw * FMath::Cos(PosePitch) + Up * FMath::Sin(PosePitch);
	const FVector Rr = Rt * FMath::Cos(Roll) + Up * FMath::Sin(Roll);
	const FRotator Rot = FRotationMatrix::MakeFromXY(Fp, Rr).Rotator();
	SetActorLocationAndRotation(ToUE(X, Y + (DkNow - RideNow) * 1.6 + TrimPorpLift, Z), Rot);
}

// ================================================================ camera
// where you stand to fish: the starboard side of the cockpit, a little aft (boat-local meters, deck height)
FVector AKeysBoatPawn::FishSpotLocal() const
{
	const FBoatSpec& S = Spec();
	// in the fighting chair: your eyes are the seated angler's (a touch high, so the view clears the transom)
	if (InChair()) return ChairEyeLocal() + FVector(0, 0, .3 - 1.62);
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
	Stage.bOn = false;   // set again below while a landing is being shot
	if (IsDiving() || IsAshore()) { bExtCam = false; return; }   // the diver / walker actor places the camera (ApplyDiveCamera)
	// a trolled fish on: a few seconds of the chair shot (your buddy strapping in, rod bent) before first person takes over
	{
		const bool bChair = InChair();
		if (bChair && !bWasInChair) { ChairIntroT = 1.5; ChairIntroAge = 0; ChairIntroJump = 0; }   // the hit: third person, then first person
		bWasInChair = bChair;
		// watch the first jump in third person, then 1.5 s on the guy in the chair (later jumps, a tail-walk, don't hold it;
		// and never more than 6 s in all)
		if (bChair && ChairIntroT > 0)
		{
			ChairIntroAge += Dt;
			if (bJumping && ChairIntroJump < 2) { ChairIntroJump = 1; ChairIntroT = FMath::Max(ChairIntroT, 1.5); }
			else if (!bJumping && ChairIntroJump == 1) ChairIntroJump = 2;
			if (ChairIntroAge > 6) ChairIntroT = 0;
		}
		ChairIntroT = FMath::Max(0.0, ChairIntroT - Dt);
	}
	if (InChair() && (!bFpPref || ChairIntroT > 0)) bFpOn = false;   // the chair cinematic: the angler is in the shot, not your own rod and hands
	{
		FVector EP; FRotator ER; float EF;
		bExtCam = ExternalCamera(EP, ER, EF);
		if (bExtCam) { Camera->SetWorldLocationAndRotation(EP, ER); Camera->SetFieldOfView(EF); return; }
	}
	const FBoatSpec& S = Spec();

	// first person while fishing (V), and the helm view (1) while driving
	const bool bWantFp = bRunning && bFpPref && IsFishingActive() && FState != EKeysFish::Caught && !(InChair() && ChairIntroT > 0);   // the landing has its own shot
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
		// the same standing view for a cast fish and a trolled one (FishSpotLocal moves you to the transom for the latter)
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
		// the fish cam takes the lens from the first-person view too
		FVector FcPos = Eye, FcLook = Eye + FVector(Dir.X, Dir.Z, Dir.Y) * 10;
		if (ApplyFishCam(Dt, T, FcPos, FcLook))
		{
			const FVector Pu = ToUE(FcPos.X, FcPos.Y, FcPos.Z), Lu = ToUE(FcLook.X, FcLook.Y, FcLook.Z);
			Camera->SetWorldLocationAndRotation(Pu, (Lu - Pu).Rotation());
			return;
		}
		Camera->SetWorldLocationAndRotation(EyeU, Dir.Rotation());
		return;
	}
	if (bDriveOn)
	{
		// standing at the wheel behind the console, looking down over the instrument panel (KeysDash.cpp lays out
		// where the driver's eye is); the horizon tilts with the hull
		const double MidX = .05 * S.Len, DeckZ = KeysMesh::SoleAt(S, MidX - .7);
		FVector Eye(MidX - .7, 0, DeckZ + 1.66), TowerEye;
		double LookDown = -.2;
		if (DashFor == S.Id) { Eye = DashL.Eye; LookDown = DashL.Pitch; }
		if (View == 2 && KeysMesh::TowerEye(S, TowerEye)) { Eye = TowerEye; LookDown = -.2; }   // up at the tuna tower helm
		FVector P = BT.TransformPosition(Eye * 100.0);
		if (!bDragging) { DYaw *= 1 - FMath::Min(1.0, Dt * 1.2); DPitch *= 1 - FMath::Min(1.0, Dt * 1.2); }
		const FQuat Look = FRotator(FMath::RadiansToDegrees(DPitch + LookDown), -FMath::RadiansToDegrees(DYaw), 0).Quaternion();
		if (Shake > 0) P += FVector(FMath::FRand() - .5, FMath::FRand() - .5, FMath::FRand() - .5) * Shake * 15;
		Camera->SetWorldLocationAndRotation(P, (BT.GetRotation() * Look).Rotator());
		return;
	}

	FVector WantPos, WantLook;
	if (FState == EKeysFish::Caught)
	{
		// the landing, shot like a film: you and the mate haul the fish in, over the gunwale and onto the cockpit sole,
		// or (too long for the cockpit) heave it up over the gunwale all the same and lay it across the cockpit head first,
		// the tail left hanging back over the rail.
		// The lens starts outboard and low to see it come up out of the sea and cranes up over the cockpit once it's in;
		// a long lens wide open, the boat around it soft; the lower third waits until the fish is aboard.
		const double Age0 = T - CaughtT;
		auto Ease = [](double V) { V = FMath::Clamp(V, 0.0, 1.0); return V * V * (3 - 2 * V); };
		const FBoatSpec& BS = Spec();
		const FTransform HullT = GetActorTransform();   // the crew's frame, so their hands meet the fish
		// measured once as it lands (the mesh is reshaped every frame from here on: the tail beat and the drape)
		if (Age0 < .05 || LandLB.BoxExtent.IsNearlyZero()) LandLB = FishMesh->CalcBounds(FTransform::Identity);
		const FBoxSphereBounds LB = LandLB;   // nose along +X
		const double Lv = FMath::Max(.4, LB.BoxExtent.X * 2 / 100);
		// where it lands: the open deck aft, in front of the stern bench, clear of the seats, the wet bar, the hardtop
		// and the fighting chair, so the camera has it in plain view
		double Aft, Fore;
		KeysMesh::ClearSole(BS, Aft, Fore);
		if (KeysMesh::HasFightChair(BS))
		{
			const double CX = KeysMesh::FightChairBase(BS).X, AftRoom = CX - .8 - Aft, ForeRoom = Fore - (CX + .8);
			if (AftRoom >= Lv + .05 || AftRoom >= ForeRoom) Fore = FMath::Min(Fore, CX - .8); else Aft = FMath::Max(Aft, CX + .8);
		}
		const bool bLong = Lv > (Fore - Aft) * .95 + .35;   // a little overhang is fine on the open deck
		const double Sd = ShowSgn >= 0 ? 1 : -1, HB = BS.Beam * .5;
		double LX, LY, LYaw = 0;
		const double DoorX = FMath::Max(Aft + .4, (Aft + Fore) * .5 - .3);   // the side door in the gunwale, by that deck
		if (bLong) { LX = DoorX; LY = 0; }
		else { LX = FMath::Min((Aft + Fore) * .5, Aft + Lv * .5 + .1); LY = -Sd * BS.Beam * .06; LYaw = .22 * Sd; }   // aft, a little askew
		const double SoleZ = KeysMesh::SoleAt(BS, FMath::Max(LX, -BS.Len * .5 + .3));
		double Haul = 0, Done = 2.3, Land = 0;
		FVector PosL;
		FQuat Q;
		const double Kick = FMath::Sin(Age0 * 6.1) * (Age0 < .7 ? 1.6 : FMath::Exp(-FMath::Max(0.0, Age0 - 1.5) * .5)) + FMath::Sin(Age0 * 1.3) * .08;
		if (!bLong)
		{
			// thrashing beside the hull, then two pulls: up out of the water head first, then over the gunwale and down
			const FVector P0(LX + .1, Sd * (HB + .6), SoleZ - 1.0), P1(LX + .05, Sd * (HB + .02), SoleZ + 1.0 + Lv * .3), P2(LX, LY, SoleZ);
			Haul = .55 * Ease((Age0 - .6) / .6) + .45 * Ease((Age0 - 1.3) / .6);
			PosL = Haul < .55 ? FMath::Lerp(P0, P1, Ease(Haul / .55)) : FMath::Lerp(P1, P2, Ease((Haul - .55) / .45));
			const FQuat Hang = FRotator(78, FMath::RadiansToDegrees(-Sd * PI / 2), Kick * 12).Quaternion();   // nose up, side to the boat
			const FQuat Flat = FRotator(0, FMath::RadiansToDegrees(LYaw + Kick * .05), 90 + Kick * 14).Quaternion();
			Q = FQuat::Slerp(Hang, Flat, float(Ease((Haul - .45) / .55)));
			Land = Ease((Haul - .85) / .15);
		}
		else
		{
			// too long for the cockpit: up the side on the gaff in two heavy pulls, over the gunwale, and laid across the
			// cockpit head first toward the far side; what doesn't fit lies back over the rail, the tail toward the water
			Done = 3.2;
			const double RailZ = KeysMesh::SheerAt(BS, LX) + .03, Inn = KeysMesh::InnerHalfBeam(BS, LX);
			const FVector NoseF(LX + .3, -Sd * FMath::Max(.3, Inn - .22), SoleZ), Rail(LX - .15, Sd * (HB - .04), RailZ);
			const FVector Ax = (NoseF - Rail).GetSafeNormal();
			const FVector P0(LX + .1, Sd * (HB + .6), SoleZ - 1.0), P1(LX + .05, Sd * (HB + .05), RailZ + Lv * .32), P2 = NoseF - Ax * Lv * .45;
			Haul = .5 * Ease((Age0 - .6) / .8) + .5 * Ease((Age0 - 1.6) / .9);
			PosL = Haul < .5 ? FMath::Lerp(P0, P1, Ease(Haul / .5)) : FMath::Lerp(P1, P2, Ease((Haul - .5) / .5));
			const FQuat Hang = FRotator(78, FMath::RadiansToDegrees(-Sd * PI / 2), Kick * 12).Quaternion();   // nose up, side to the boat
			const FQuat Lay = FRotator(FMath::RadiansToDegrees(FMath::Asin(FMath::Clamp(Ax.Z, -1.0, 1.0))), FMath::RadiansToDegrees(FMath::Atan2(Ax.Y, Ax.X)), 90 + Kick * 14).Quaternion();
			Q = FQuat::Slerp(Hang, Lay, float(Ease((Haul - .45) / .55)));
			Land = Ease((Haul - .85) / .15);
			LX = P2.X; LY = P2.Y;
		}
		// spray as it breaks the surface, and water streaming off it while it comes up
		if (Age0 < .05) { bLandSplash = false; LandShedT = 0; }
		if (AKeysLife* LL = AKeysLife::Get(this))
		{
			const FVector W = HullT.TransformPosition(PosL * 100.0) / 100;   // UE meters: X, Y, Z up
			if (!bLandSplash && Haul > .06) { bLandSplash = true; LL->Splash(W.X, W.Y, 7, FMath::Clamp(Lv * .18, .2, .45)); if (AKeysAudio* Au = AKeysAudio::Get(this)) Au->Splash(W.X, W.Y, 10, 1.2); }
			LandShedT -= Dt;
			if (Haul > .05 && Land < .5 && LandShedT <= 0) { LandShedT = .12; LL->Shed(FVector(W.X, W.Z, W.Y), FVector(0, 1.0, 0), 3, .03); }
		}
		// lying down it rests on its flank: lift it by half its girth (the model's half-width, nose along +X)
		PosL.Z += LB.BoxExtent.Y / 100 * .8 * Land;
		const FTransform CompT(HullT.GetRotation() * Q, HullT.TransformPosition(PosL * 100.0));
		// the fish isn't a plank: it beats its tail, hard while it thrashes at the side and hangs on the gaff, then in
		// slowing bursts as it lies on the deck. And it lies on what's under it: a long fish's tail past the gunwale has
		// nothing under it, so it bends down over the rail toward the water instead of sticking out stiff.
		{
			const double Aboard = Ease((Haul - .9) / .1);
			const double Thrash = Age0 < .7 ? 1.6 : 1.1;
			const double Settle = FMath::Max(0.0, Age0 - Done);
			const double Burst = .5 + .5 * FMath::Sin(Age0 * 1.7);
			const double Amp = FMath::Lerp(Thrash, (2.4 * FMath::Exp(-Settle * .45) + .25) * Burst, Aboard);
			LandFlopPh += Dt * FMath::Lerp(11.0, 8.0 + 4 * Burst, Aboard);
			FKeysMesh FM;
			const double Scale = 1.35 * FMath::Clamp(FMath::Pow(Weight / ((Sp->Min + Sp->Max) / 2), 1.0 / 3), .75, 1.5);
			if (AKeysLife::BuildCreatureMeshBent(Key, FM, Scale, LandFlopPh, Amp))
			{
				if (bLong && Land > 0)
				{
					const double Sill = HB - .04, SillZ = KeysMesh::SheerAt(BS, LX) + .03;   // the gunwale's outer edge, boat meters
					for (FVector& V : FM.V)
					{
						const FVector Bm = HullT.InverseTransformPosition(CompT.TransformPosition(V)) / 100;
						const double Out = Sd * Bm.Y - Sill;
						if (Out <= 0) continue;
						// curl about the sill: the further out, the more it hangs (toward 70 degrees), as a heavy body would
						const double Th = FMath::Min(1.2, Out * 1.1) * Land;
						const double Up = Bm.Z - SillZ;
						const double O2 = Out * FMath::Cos(Th) + Up * FMath::Sin(Th), U2 = Up * FMath::Cos(Th) - Out * FMath::Sin(Th);
						const FVector B2(Bm.X, Sd * (Sill + O2), SillZ + U2);
						V = CompT.InverseTransformPosition(HullT.TransformPosition(B2 * 100.0));
					}
				}
				FM.Commit(FishMesh, 0, AKeysWorld::VertexColorMaterial());
			}
		}
		FishMesh->SetWorldTransform(CompT);
		const FTransform FT = FishMesh->GetComponentTransform();
		const FBoxSphereBounds B = FishMesh->CalcBounds(FT);
		const FVector NoseU = FT.TransformPosition(FVector(LB.Origin.X + LB.BoxExtent.X * .72, 0, 0));
		Stage.bOn = true; Stage.bDoor = false; Stage.Age = Age0; Stage.Lv = Lv; Stage.Side = Sd; Stage.Pull = Haul; Stage.Done = Done; Stage.DoorX = DoorX;
		Stage.Fish = HullT.InverseTransformPosition(B.Origin) / 100; Stage.Grip = HullT.InverseTransformPosition(NoseU) / 100; Stage.Final = FVector(LX, LY, SoleZ);
		// while it's coming aboard, frame the rail too, where you two are hauling on it
		const FVector RailU = HullT.TransformPosition(FVector(LX, Sd * (HB - .25), SoleZ + 1.1) * 100.0);
		const FVector CtrU = FMath::Lerp(FMath::Lerp(B.Origin, NoseU, .25), RailU, .55 * (1 - Land));
		ShowPos = FVector(CtrU.X / 100, CtrU.Z / 100, CtrU.Y / 100);
		CatchFov = 34.f;
		// the lens, in the boat's frame: outboard and low (astern for the door) for the haul, craning up over the
		// cockpit once the fish is in; it arcs a little all the while
		const double Crane = Ease((Age0 - Done + .6) / 1.2);
		const double Arc = FMath::Lerp(-.15, .12, Ease(Age0 / (Done + 4))) + FMath::Max(0.0, Age0 - Done - 4) * .01;
		const FVector D0(-.35, Sd, .85), D1(-.4, -Sd * .62, .95);   // in: a three-quarter view from the far rail, the crew behind it
		const FVector DirL = FQuat(FVector::UpVector, Arc).RotateVector(FMath::Lerp(D0.GetSafeNormal(), D1.GetSafeNormal(), Crane).GetSafeNormal());
		const FVector DirU = HullT.GetRotation().RotateVector(DirL).GetSafeNormal();
		const double Span = FMath::Max(Lv, FMath::Lerp(3.2, 2.4, Land));   // the fish and the two of you
		const double Fit = Span / FMath::Lerp(.55, bLong ? .48 : .42, Crane) * .5 / FMath::Tan(FMath::DegreesToRadians(CatchFov * .5));
		const double Dist = Fit * FMath::Lerp(1.08, .98, Ease(Age0 / (Done + 5)));
		const FVector PosU = CtrU + DirU * Dist * 100;
		const FVector FwU = (CtrU - PosU).GetSafeNormal(), RightU = FVector::CrossProduct(FVector::UpVector, FwU).GetSafeNormal();
		const FVector LookU = CtrU - RightU * Dist * 100 * FMath::Tan(FMath::DegreesToRadians(CatchFov * .5)) * .28 * Ease((Age0 - Done) / 1.2);
		WantPos = FVector(PosU.X / 100, PosU.Z / 100, PosU.Y / 100);
		WantLook = FVector(LookU.X / 100, LookU.Z / 100, LookU.Y / 100);
		FPostProcessSettings& PP = Camera->PostProcessSettings;
		PP.bOverride_DepthOfFieldFocalDistance = true; PP.DepthOfFieldFocalDistance = float(Dist * 100);
		PP.bOverride_DepthOfFieldFstop = true; PP.DepthOfFieldFstop = 2.2f;
		PP.bOverride_DepthOfFieldSensorWidth = true; PP.DepthOfFieldSensorWidth = 36.f;
		PP.bOverride_VignetteIntensity = true; PP.VignetteIntensity = .55f;
		Camera->PostProcessBlendWeight = 1.f;
		Shake = 0;
		if (!CatchLight)
		{
			CatchLight = NewObject<UPointLightComponent>(this);
			CatchLight->SetupAttachment(GetRootComponent());
			CatchLight->SetUsingAbsoluteLocation(true);
			CatchLight->SetIntensityUnits(ELightUnits::Lumens);
			CatchLight->SetCastShadows(false);
			CatchLight->SetSourceRadius(40.f);
			CatchLight->SetSoftSourceRadius(60.f);
			CatchLight->SetLightColor(FLinearColor(1.f, .95f, .88f));
			CatchLight->RegisterComponent();
		}
		// up and to one side of the lens, so the scales and the eye carry a highlight even with the sun behind the fish
		const FVector Fish(ShowPos.X, ShowPos.Y, ShowPos.Z), ToLens = (CamPos - Fish).GetSafeNormal2D();
		CatchLight->SetWorldLocation(ToUE(Fish.X + ToLens.X * 1.6 - ToLens.Z * .8, Fish.Y + 1.2, Fish.Z + ToLens.Z * 1.6 + ToLens.X * .8));
		CatchLight->SetIntensity(float(180 + FishLen * 180));   // a soft fill: enough for a catchlight, not to burn out the belly
		CatchLight->SetAttenuationRadius(float((4 + FishLen * 2) * 100));
		CatchLight->SetVisibility(true);
	}
	else if (CatchLight && CatchLight->IsVisible()) CatchLight->SetVisibility(false);
	if (FState != EKeysFish::Caught)
	{
		const double Dist = (S.Len * 1.5 + 11) * CamZoom, Ht = (S.Len * .32 + 4.2) * CamZoom * (1 + CamPitchOff);
		if (!bDragging && FMath::Abs(Speed) > 5) CamYaw *= 1 - FMath::Min(1.0, Dt * 1.2);
		double A = H + CamYaw;
		// a fish on: frame the boat and the line from beyond the boat, a little off the line so the hull doesn't hide
		// it, rather than from astern (with the stern held on the fish, astern would put the boat behind the lens)
		if (FState == EKeysFish::Fight && FMath::Abs(Speed) < 6) A = FMath::Atan2(-(FX - X), -(FZ - Z)) + .45 + CamYaw;
		WantPos = FVector(X + FMath::Sin(A) * Dist, Y + Ht, Z + FMath::Cos(A) * Dist);
		if (!bRunning)
		{
			const double O = T * .08;
			WantPos = FVector(X + FMath::Sin(O) * Dist * 1.3, Y + Ht * 1.2, Z + FMath::Cos(O) * Dist * 1.3);
		}
		double LX = X - FMath::Sin(H) * 4, LZ = Z - FMath::Cos(H) * 4;
		if (FState == EKeysFish::Fight)
		{
			LX = Lerp(X, FX, .35); LZ = Lerp(Z, FZ, .35);
			// a fish in the air: swing out to the side of the line, so the boat (its tower, its top) isn't between the
			// lens and the jump, keeping to whichever side the camera is already on
			if (bJumping && bRunning && !bDragging)
			{
				const double B = FMath::Atan2(FZ - Z, FX - X), Cur = FMath::Atan2(CamPos.Z - Z, CamPos.X - X);
				const double A1 = B + PI + .8, A2 = B + PI - .8;
				const double Ab = FMath::Abs(FMath::UnwindRadians(A1 - Cur)) < FMath::Abs(FMath::UnwindRadians(A2 - Cur)) ? A1 : A2;
				WantPos = FVector(X + FMath::Cos(Ab) * Dist, Y + Ht, Z + FMath::Sin(Ab) * Dist);
			}
		}
		else if (FState != EKeysFish::Idle) { LX = Lerp(X, Bob.X, .35); LZ = Lerp(Z, Bob.Z, .35); }
		WantLook = FVector(LX, Y + 1.8, LZ);
		if (RaiseCamT > 0 && bRunning && !bDragging)
		{
			// a fish coming up on the spread: the camera drops down beside that lure, low over the water and a little
			// toward the boat, to watch the fish come up the wake behind it, track it, and take it
			FVector Lr(FX, 0, FZ);
			if (!PendKey.IsEmpty()) { FVector LP; if (LurePos(PendStation, LP)) Lr = LP; }
			double TX = X - Lr.X, TZ = Z - Lr.Z;
			const double TL = FMath::Max(1.0, Hyp(TX, TZ));
			TX /= TL; TZ /= TL;                                   // along the line, toward the boat
			double PX = -TZ, PZ = TX;                             // across it, outboard of the wake
			if ((Lr.X - X) * PX + (Lr.Z - Z) * PZ < 0) { PX = -PX; PZ = -PZ; }
			const double Wl = WaveH(Lr.X, Lr.Z, T, AmpAt(Lr.X, Lr.Z));
			const double Fl = KeysMesh::FishLength(!PendKey.IsEmpty() ? PendWeight : Weight);   // closer for a small fish
			const double Off = FMath::Clamp(2.2 + Fl * 2.6, 3.2, 8.0);
			WantPos = FVector(Lr.X + PX * Off + TX * Off * .45, Wl + Off * .38, Lr.Z + PZ * Off + TZ * Off * .45);
			WantLook = FVector(Lr.X - TX * 1.2, Wl - .35, Lr.Z - TZ * 1.2);
		}
	}
	if (!bCamInit) { CamPos = WantPos; CamLook = WantLook; bCamInit = true; }
	CamPos = FMath::Lerp(CamPos, WantPos, 1 - FMath::Exp(-Dt * 4));
	CamLook = FMath::Lerp(CamLook, WantLook, 1 - FMath::Exp(-Dt * 5));
	if (FState == EKeysFish::Caught) { CamPos = WantPos; CamLook = WantLook; }   // the landing shot flies its own path (and opens on a cut)
	FVector ShotPos = CamPos, ShotLook = CamLook;
	const bool bFishCam = ApplyFishCam(Dt, T, ShotPos, ShotLook);
	// keep the lens out of the water (but for the fish cam, which is meant to be under it)
	const double Surf = WaveH(CamPos.X, CamPos.Z, T, AmpAt(CamPos.X, CamPos.Z), false);
	if (CamPos.Y < Surf + .6) CamPos.Y = Surf + .6;
	if (!bFishCam) { ShotPos = CamPos; ShotLook = CamLook; }
	FVector P = ToUE(ShotPos.X, ShotPos.Y, ShotPos.Z);
	if (Shake > 0) P += FVector(FMath::FRand() - .5, FMath::FRand() - .5, FMath::FRand() - .5) * Shake * 60;
	const FVector L = ToUE(ShotLook.X, ShotLook.Y, ShotLook.Z);
	Camera->SetWorldLocationAndRotation(P, (L - P).Rotation());
	// the landing close-up fills the frame with white deck in full sun: stop the lens down for it, and ease the bloom
	CloseUpK = FMath::FInterpTo(CloseUpK, FState == EKeysFish::Caught || Stage.bOn ? 1.0 : 0.0, Dt, 3.0);
	// under a mangrove tunnel's roof the eye adaptation opens right up, and the water, still mirroring open sky, burns
	// out to white: hold the exposure down while she is in the roofed stretch
	static double TunnelK = 0;
	TunnelK = FMath::FInterpTo(TunnelK, RealMap() && KeysMangroveLife::Near(X, Z).bRoof ? 1.0 : 0.0, Dt, 1.2);
	Camera->PostProcessSettings.bOverride_AutoExposureBias = Camera->PostProcessSettings.bOverride_BloomIntensity = CloseUpK > .01 || TunnelK > .01;
	Camera->PostProcessSettings.AutoExposureBias = float(-.6 * CloseUpK - 1.5 * TunnelK);
	Camera->PostProcessSettings.BloomIntensity = float(.55 - .3 * CloseUpK);
}

// ================================================================ the fish cam
// On the hookset (never the bite: the strike window is the one skill check, and the camera stays with you for it), a
// 2.3 s cut under the water: low and looking a little up so the surface is in frame, on the far side of the fish from
// the boat, arcing round it and pushing in, with a little handheld drift, easing in and out so it never jump-cuts.
void AKeysBoatPawn::SmoothCameraCuts(double Dt)
{
	// the diver and the walker place their own cameras; a jump into or out of them (and a teleport) stays a cut
	if (IsDiving() || IsAshore() || !Camera) { bHaveLastCam = false; LastCamMode = -1; CutT = -1; return; }
	const int32 Mode = (bExtCam ? 1 : 0) | (bFpOn ? 2 : 0) | (bDriveOn ? 4 : 0) | (FState == EKeysFish::Caught ? 8 : 0);
	const FVector WantLoc = Camera->GetComponentLocation();
	const FQuat WantRot = Camera->GetComponentQuat();
	const float WantFov = Camera->FieldOfView;
	// (and never through the surface: a shot that starts or ends under the water - the hooked fish's underwater cam -
	// cuts, as a lens can't glide out of the sea)
	auto Under = [this](const FVector& U) { return U.Z / 100.0 < WaveH(U.X / 100.0, U.Y / 100.0, GetWorld()->GetTimeSeconds(), AmpAt(U.X / 100.0, U.Y / 100.0)) - .05; };
	if (bHaveLastCam && Mode != LastCamMode && FVector::Dist(WantLoc, LastCamLoc) < 6000 && !Under(WantLoc) && !Under(LastCamLoc))
	{
		// glide from wherever the lens was this instant (mid-glide too) to the new shot: quicker into the landing, which
		// has its own fly-in, a touch longer into and out of the chair shot and first person
		CutFromLoc = LastCamLoc; CutFromRot = LastCamRot; CutFromFov = LastCamFov;
		CutT = 0;
		CutDur = (Mode & 8) ? .55 : .85;
	}
	LastCamMode = Mode;
	FVector Loc = WantLoc;
	FQuat Rot = WantRot;
	float Fov = WantFov;
	if (CutT >= 0)
	{
		CutT += Dt;
		const double U = FMath::Clamp(CutT / CutDur, 0.0, 1.0);
		const double A = U * U * U * (U * (U * 6 - 15) + 10);   // smootherstep: no jolt at either end
		// a little lift through the middle, so the lens rises over the gunwale and the T-top edge rather than through them
		Loc = FMath::Lerp(CutFromLoc, WantLoc, A) + FVector(0, 0, 60 * FMath::Sin(A * PI) * FMath::Min(1.0, FVector::Dist(CutFromLoc, WantLoc) / 600));
		Rot = FQuat::Slerp(CutFromRot, WantRot, float(A));
		Fov = FMath::Lerp(CutFromFov, WantFov, float(A));
		Camera->SetWorldLocationAndRotation(Loc, Rot);
		Camera->SetFieldOfView(Fov);
		if (U >= 1) CutT = -1;
	}
	LastCamLoc = Loc; LastCamRot = Rot; LastCamFov = Fov;
	bHaveLastCam = true;
}

void AKeysBoatPawn::TriggerFishCam(double Hold)
{
	if (FishCamT >= 0 || Stage.bOn) return;
	FishCamT = 0; FishCamBlend = 0; FishCamLen = Hold > 0 ? Hold : 2.3;
	FishCamSeed = FMath::FRand() * 1000; FishCamSide = FMath::RandBool() ? -1 : 1;
	FishCamFrom = FVector(FX, 0, FZ);
}

bool AKeysBoatPawn::ApplyFishCam(double Dt, double T, FVector& Pos, FVector& Look)
{
	if (FishCamT < 0) return false;
	if (FState != EKeysFish::Fight || IsDiving()) { FishCamT = -1; FishCamBlend = 0; return false; }
	FishCamT += Dt;
	if (FishCamT >= FishCamLen) { FishCamT = -1; FishCamBlend = 0; return false; }
	auto SS = [](double V) { const double K = FMath::Clamp(V, 0.0, 1.0); return K * K * (3 - 2 * K); };
	FishCamBlend = FMath::Min(SS(FishCamT / .28), SS((FishCamLen - FishCamT) / .45));
	const double K = FMath::Min(1.0, FishCamT / FishCamLen);
	const double SurfY = WaveH(FX, FZ, T, AmpAt(FX, FZ), false), FloorY = -DepthAt(FX, FZ);
	const double Lo = FloorY + .7, Hi = SurfY - .5;
	// where the fish really is (just under the surface, or down where a sounder has taken it): look straight at it
	// from a little under it, so it's climbing across frame with the surface above
	const double FishY = SurfY - FishLen * .09 - Deep;
	FVector Lk(FX, FMath::Min(Hi, FMath::Max(Lo, FishY)), FZ);
	const double DeepY = FMath::Min(Hi, FMath::Max(Lo, FishY - .9));
	// composed round the boat: the lens on the far side of the fish from the hull, looking back along the line
	double AX = Lk.X - X, AZ = Lk.Z - Z;
	if (Hyp(AX, AZ) < .5) { AX = Lk.X - FishCamFrom.X; AZ = Lk.Z - FishCamFrom.Z; }
	const double Ang = FMath::Atan2(AX, AZ) + FishCamSide * (.85 * (K - .5));
	const double Range = 5.6 + (3.1 - 5.6) * SS(K);
	FVector Ps(Lk.X + FMath::Sin(Ang) * Range, DeepY, Lk.Z + FMath::Cos(Ang) * Range);
	// handheld: two octaves, the fast one settling
	auto N = [&](double F, double Ph) { return FMath::Sin(FishCamT * F + FishCamSeed + Ph); };
	const double J = .14 * (1 - .6 * K);
	Ps.X += N(2.1, 0) * J + N(5.7, 1.1) * J * .4;
	Ps.Y += N(1.7, 2.0) * J * .8 + N(6.3, .4) * J * .3;
	Ps.Z += N(1.9, 4.0) * J + N(5.1, 2.6) * J * .4;
	Pos = FMath::Lerp(Pos, Ps, FishCamBlend);
	Look = FMath::Lerp(Look, Lk, FishCamBlend);
	return true;
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
	if (bShoreSwap)
	{
		// ashore: the rod in the angler's hands, out toward the water
		const double DX = Target.X - X, DZ = Target.Z - Z, DL = FMath::Max(1.0, Hyp(DX, DZ));
		return FVector(X + DX / DL * 1.9, Y + 2.7, Z + DZ / DL * 1.9);
	}
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
	// never over the bow: from the cockpit that throws the line through the console and the T-top, and hides the float
	// behind the boat. Anything within 70 degrees of dead ahead goes out over the side the angler stands on instead.
	if (!bShoreSwap)
	{
		const FVector2D Fw(-FMath::Sin(H), -FMath::Cos(H)), Rt(FMath::Cos(H), -FMath::Sin(H));
		if (FVector2D::DotProduct(Dir, Fw) > FMath::Cos(FMath::DegreesToRadians(70.0)))
		{
			const FVector SpotU = GetActorTransform().TransformPosition(FishSpotLocal() * 100.0) / 100.0;   // Unreal X/Y = game X/Z
			const double Side = (SpotU.X - X) * Rt.X + (SpotU.Y - Z) * Rt.Y >= 0 ? 1 : -1;
			const double A = FMath::DegreesToRadians(80.0);
			Dir = Fw * FMath::Cos(A) + Rt * (Side * FMath::Sin(A));
		}
	}
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
		V *= KeysFeed::Mul(R.Key, KeysFeed::HourNow(this));   // the ones feeding now take the bait more often
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

FVector AKeysBoatPawn::LandedFishLocal() const
{
	const FVector C = FishMesh ? FishMesh->Bounds.Origin : GetActorLocation();
	return GetActorTransform().InverseTransformPosition(C) / 100.0;
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
	bBackMsg = false; BackK = 0; bBackBurst = false; BackBurstT = 0;
	Tension = .55; Stamina = 1; bRunningFish = true; RunT = Rand(2, 4); RunAng = Rand(-.7, .7); Pull = Str * 1.05; SlackT = 0; OverT = 0;
	JumpQ = 0; Deep = 0; DeepT = 0; ShakeT = Rand(.6, 1.4);
	FState = EKeysFish::Fight;
	if (RaiseCamT <= 0) TriggerFishCam();
	FightLabel = SizeT > .7 ? TEXT("Something heavy is on") : SizeT > .35 ? TEXT("Solid fish on") : TEXT("Fish on");
	FightSub = TEXT("It's running! Hold the down arrow (or Space) to wind up tension - more tension gains more line. Ease off before the red.");
	Shake = FMath::Max(Shake, .3);
	ShowFishMesh(true);
	// the hit itself: the jumpers go airborne straight away (a tarpon always, a billfish nearly always, greyhounding)
	const FJumpHabit Jh = JumpHabit(Key, Sp->bJump);
	JumpStyle = Jh.Style;
	bFirstJump = true;
	if (Jh.StrikeP > 0 && FMath::FRand() < Jh.StrikeP) { JumpQ = FMath::RandRange(Jh.NMin, Jh.NMax); JumpWait = Rand(.2, .5); }
	else if (IsSounder(Key)) DeepT = Rand(3, 7);
}

// How each species comes out of the water: on the strike (chance, how many in a row), on a later run, and the shape of
// it. Tarpon always jump when they hit, gill-rattling, and keep jumping; billfish greyhound; kingfish and wahoo
// skyrocket on the strike; a snook shakes its head on the surface; a blacktip spins; a swordfish only now and then.
// Tuna, grouper, snapper, permit, bonefish, amberjack and cobia stay down.
AKeysBoatPawn::FJumpHabit AKeysBoatPawn::JumpHabit(const FString& K, bool bJumper)
{
	if (K == TEXT("tarpon")) return {1.0, 2, 3, .8, 0};
	if (IsBillfish(K)) return {.93, 3, 5, .55, 1};
	if (K == TEXT("mahi")) return {.78, 1, 2, .45, 0};
	if (K == TEXT("ladyfish")) return {.8, 1, 3, .5, 0};
	if (K == TEXT("barracuda")) return {.72, 1, 1, .3, 0};
	if (K == TEXT("blacktip")) return {.72, 1, 2, .3, 4};
	if (K == TEXT("kingfish")) return {.6, 1, 1, .1, 2};
	if (K == TEXT("wahoo")) return {.3, 1, 1, .05, 2};
	if (K == TEXT("snook")) return {.5, 1, 1, .25, 3};
	if (K == TEXT("swordfish")) return {.15, 1, 1, .05, 0};
	if (bJumper) return {.8, 1, 1, .4, 0};
	return {0, 0, 0, 0, 0};
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
	if (!bLow && JumpStyle == 2)
	{
		// a skyrocket: straight up out of the water, well clear of it, and back down nearly where it left
		JRun = Lf * .5; JH = 2.6 + Lf * 1.3; JDur = 1.2 + Lf * .2;
	}
	if (JumpStyle == 4) JSpin = (FMath::FRand() < .5 ? -1 : 1) * Rand(3.5, 5.0);   // a spinner: two or three turns in the air
	// the first jump on the hit is the big one: a tarpon rattling its gills high over the water
	if (bFirstJump && !bLow && Key == TEXT("tarpon")) { JH *= 1.3; JDur *= 1.12; }
	bFirstJump = false;
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
	// water streams off the body through the leap, thinning as it dries, so the arc reads from far off
	if (AKeysLife* L = AKeysLife::Get(this))
	{
		const FVector JV(JDX * JRun / JDur, 4 * JH * (1 - 2 * P) / JDur, JDZ * JRun / JDur);
		const int32 NShed = FMath::RoundToInt(Dt * (90 + FishLen * 30) * (1.1 - P) + FMath::FRand());
		L->Shed(FVector(PX - JDX * FishLen * .3, PY, PZ - JDZ * FishLen * .3), JV, NShed, .05 + FishLen * .018);
	}
	// nose up on the way out, nose down on the way back in, with a head-shake wobble
	const double Yaw = FMath::Atan2(JDZ, JDX);
	const FRotator R(FMath::RadiansToDegrees((1 - 2 * P) * (bJumpLow ? .5 : 1.1)), FMath::RadiansToDegrees(Yaw + FMath::Sin(P * 9) * .25), FMath::RadiansToDegrees(FMath::Sin(P * 22) * .55 + JSpin * P * 1.4));
	FishMesh->SetWorldLocationAndRotation(ToUE(PX, PY, PZ), R);
}

void AKeysBoatPawn::Lose(const FString& Msg)
{
	if (Msg.StartsWith(TEXT("Snap")))
		if (AKeysAudio* A = AKeysAudio::Get(this)) A->Snap();
	if (bScriptLog) UE_LOG(LogTemp, Log, TEXT("KRDBG lost %s: %s"), *Key, *Msg);
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
	// what the charter (if any) makes of it: speared, trolled off the spread, or cast
	const ECharterMethod How = bLandingSpear ? ECharterMethod::Spear : HookedStation >= 0 ? ECharterMethod::Troll : ECharterMethod::Cast;
	FState = EKeysFish::Caught;
	bJumping = false;
	ReleaseT = -1;
	if (AKeysCharter* Ch = AKeysCharter::Get(this); Ch && !bFreeFish && Sp) Ch->OnLanded(Key, Sp->Name, Weight, How);
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
	// every catch goes on the leaderboard / species records, in Free Fish too (only the money is off there)
	if (Save) { Save->Record(Key, Weight, Spec().Brand + TEXT(" ") + Spec().Name); Save->RecordScore(Score); }   // lbRecord
	UE_LOG(LogTemp, Log, TEXT("KRCatch logged %s %.1f lb (mode %s, written to the save: %s)"), *Key, Weight, bFreeFish ? TEXT("FreeFish") : TEXT("Charter"), Save ? TEXT("yes") : TEXT("no - test run"));
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
	CardFact = S.Fact + TEXT("  ") + KeysFeed::BestTimes(Key);
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
	if (bShoreSwap) ShowPos = FVector(X - FMath::Sin(H) * .9, Y + 1.35, Z - FMath::Cos(H) * .9);   // held up on the planks
	FishMesh->SetWorldLocationAndRotation(ToUE(ShowPos.X, ShowPos.Y, ShowPos.Z), FRotator(78, FMath::RadiansToDegrees(H), 0));
	ShowFishMesh(true);
}

void AKeysBoatPawn::KeepFish()
{
	if (FState != EKeysFish::Caught) return;
	// trying to keep a catch-and-release charter's fish ends the charter, even one you couldn't legally keep anyway
	if (AKeysCharter* Ch = AKeysCharter::Get(this); Ch && Ch->IsPendingRelease())
	{
		Ch->OnKept();
		if (!bCanKeep) { ShowToast(FString::Printf(TEXT("A %s goes back - and the clients wanted it released. It's dead on deck now."), *Sp->Name), 5); FinishCatch(); return; }
	}
	if (!bCanKeep) return;
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
	if (AKeysCharter* Ch = AKeysCharter::Get(this)) Ch->OnReleased();
	const bool bSpeared = bCardSpeared;
	FinishCatch();
	if (!bSpeared) StartReleaseAnim();
}

// The release: whoever landed it carries the fish to the gunwale, leans out and lowers it into the water, lets go,
// and it shakes itself awake and swims off and down
void AKeysBoatPawn::StartReleaseAnim()
{
	if (!BoatMesh || !FishMesh) return;
	const FTransform BT = BoatMesh->GetComponentTransform();
	RelStart = BT.InverseTransformPosition(FishMesh->GetComponentLocation()) / 100.0;
	RelSide = RelStart.Y >= 0 ? 1 : -1;
	if (Stage.bOn || FMath::Abs(Stage.Side) > .5) RelSide = Stage.Side >= 0 ? 1 : -1;
	RelFish = RelStart;
	ReleaseT = 0;
	ShowFishMesh(true);
}

void AKeysBoatPawn::UpdateReleaseAnim(double Dt)
{
	if (ReleaseT < 0 || !BoatMesh) return;
	if (FState != EKeysFish::Idle) { ReleaseT = -1; return; }
	ReleaseT += Dt;
	const double T = ReleaseT;
	const FBoatSpec& S = Spec();
	const double RX = FMath::Clamp(RelStart.X, -S.Len * .42, S.Len * .3);
	const double HB = KeysMesh::InnerHalfBeam(S, RX) + .14;
	const FVector Rail(RX, RelSide * (HB + .25), KeysMesh::SheerAt(S, RX) + .25);
	const FVector Water(RX, RelSide * (HB + .45), -.05);
	auto Ease = [](double V) { V = FMath::Clamp(V, 0.0, 1.0); return V * V * (3 - 2 * V); };
	double FYaw = RelSide * 90, FRoll = 0, FPitch = 0;
	if (T < .9) RelFish = FMath::Lerp(RelStart + FVector(0, 0, .5), Rail, Ease(T / .9));            // lifted to the rail
	else if (T < 1.7) RelFish = FMath::Lerp(Rail, Water, Ease((T - .9) / .8));                      // lowered into the water
	else
	{
		// let go: a beat to come round, then a kick and away, nose out from the hull and angling down
		const double U = T - 1.7;
		const double Go = FMath::Max(0.0, U - .4);
		RelFish = Water + FVector(-Go * .8, RelSide * (Go * 2.2 + Go * Go * .8), -Go * .9);
		FYaw = RelSide * 90 - RelSide * 25 + FMath::Sin(T * (U < .4 ? 6 : 16)) * (U < .4 ? 10 : 22);
		FPitch = -12 * FMath::Min(1.0, Go);
	}
	if (T < 1.7) FRoll = 70 - 70 * Ease((T - .9) / .8);   // carried on its side, righted as it goes in
	const FTransform BT = BoatMesh->GetComponentTransform();
	FishMesh->SetWorldLocationAndRotation(BT.TransformPosition(RelFish * 100.0), (BT.GetRotation() * FRotator(FPitch, FYaw, FRoll).Quaternion()));
	if (T > 4.2) { ReleaseT = -1; ShowFishMesh(false); }
}

void AKeysBoatPawn::FinishCatch()
{
	LandLB = FBoxSphereBounds(ForceInit);
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
	const int64 Paid = bFreeFish ? 0 : FMath::RoundToInt64(Sale);   // Free Fish: logged and scored, but no sale
	Cash += Paid;
	UE_LOG(LogTemp, Log, TEXT("KRWeigh %d fish, sale would be $%.0f, paid $%lld (mode %s), cash now %lld"), Cooler.Num(), Sale, Paid, bFreeFish ? TEXT("FreeFish") : TEXT("Charter"), Cash);
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
		case EKeysFish::Waiting: ShowToast(TEXT("Wait for the float to go under, then strike. Hold the down arrow to retrieve, R to reel in.")); break;
		default: break;
	}
}

void AKeysBoatPawn::ShowFishMesh(bool bShow) { FishMesh->SetVisibility(bShow); }

void AKeysBoatPawn::UpdateFeeding(double Dt)
{
	FeedT -= Dt;
	if (FeedT > 0) return;
	FeedT = 1;
	// where you're fishing: the bait's zone with a line out, else the water under the boat
	const FString Zn = IsLineOut() && !Zone.IsEmpty() ? Zone : ZoneAt(X, Z);
	const TArray<FZoneRow>& Tbl = ZoneTable(Zn);
	const double Hr = KeysFeed::HourNow(this);
	FeedArea = KeysFeed::AreaMul(Tbl, Hr);
	TideBiteK = TideBite(Zn);
	BiteLineText = KeysFeed::BiteLine(Tbl, Hr);
	const int32 Now = FeedArea >= 1.2 ? 1 : 0;
	if (FeedState == 0 && Now == 1 && bRunning && !IsDiving() && !IsAshore() && Menu == EMenu::None)
		ShowToast(FString(TEXT("The fish are feeding - the bite's on! ")) + BiteLineText.Mid(BiteLineText.Find(TEXT("("))), 4);
	FeedState = Now;
}

void AKeysBoatPawn::UpdateFishing(double Dt, double T)
{
	if (bScriptLogFight && FState != LoggedFState)
	{
		static const TCHAR* Names[] = {TEXT("Idle"), TEXT("Casting"), TEXT("Waiting"), TEXT("Nibble"), TEXT("Bite"), TEXT("Fight"), TEXT("Caught")};
		const int32 I = int32(FState);
		UE_LOG(LogTemp, Log, TEXT("KRFish state %s  bob %.0f m out  key %s  toast \"%s\""), I >= 0 && I < 7 ? Names[I] : TEXT("?"), Hyp(Bob.X - X, Bob.Z - Z), *Key, *Toast);
		LoggedFState = FState;
	}
	// strike: like pressing Space the moment the float goes under (scripted runs)
	if (bScriptAutoStrike && FState == EKeysFish::Bite) OnAction();
	UpdateFeeding(Dt);
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
	const bool bReel = bReelKey || (bAction && FState == EKeysFish::Fight);

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
			ShowToast(FString::Printf(TEXT("%s%d m cast - %s."), Spot.bHot ? TEXT("Right in the school - ") : TEXT(""), FMath::RoundToInt(Hyp(Bob.X - X, Bob.Z - Z)), *ZoneNameAt(Bob.X, Bob.Z)));
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
			BiteT -= Dt * (bReel ? .6 : 1) * FeedArea * TideBiteK;   // a feeding window shortens the wait (never lengthens it)
			DbgWait += Dt;
			if (!Sp && BiteT < 3.2) ChooseFish();
			if (BiteT <= 0)
			{
				FState = EKeysFish::Nibble; Nibs = 1 + FMath::RandRange(0, 2); NibT = .05;
				if (bScriptLog) UE_LOG(LogTemp, Log, TEXT("KRDBG bite %d at hour %.2f, feed x%.2f, waited %.2f s"), ++DbgBites, KeysFeed::HourNow(this), FeedArea, DbgWait);
				DbgWait = 0;
			}
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
		const bool bSounder = IsSounder(Key);
		const FJumpHabit Jh = JumpHabit(Key, Sp->bJump);
		RunT -= Dt;
		if (bRunningFish && RunT <= 0) { bRunningFish = false; RunT = Rand(1.4, 3.2) * (1.3 - Stamina * .4); }
		else if (!bRunningFish && RunT <= 0)
		{
			bRunningFish = true;
			if (AKeysAudio* A = AKeysAudio::Get(this)) A->Splash(FX, FZ, 10, .9);
			RunT = Rand(.9, 2.4) * (.5 + Stamina * .8);
			RunAng = Rand(-1.1, 1.1);
			Pull = S * Rand(.7, 1.1) * (.45 + .55 * Stamina);
			if (Jh.RunP > 0 && FMath::FRand() < Jh.RunP * (.5 + .5 * Stamina) + (Key == TEXT("tarpon") ? .15 : 0))
			{
				JumpQ = Jh.Style == 1 ? 2 + FMath::RandRange(0, 2) : Key == TEXT("tarpon") ? 1 + FMath::RandRange(0, 1) : Jh.NMax > 1 && FMath::FRand() < .3 ? 2 : 1;
				JumpWait = Rand(.1, .4);
			}
			else if (bSounder && FMath::FRand() < .6)
			{
				DeepT = Rand(4, 9);
				if (T - SoundMsgT > 8) { SoundMsgT = T; ShowToast(TEXT("It's sounding - diving for the bottom!")); }
			}
		}
		// one after another until the queue is spent; a greyhounding billfish skips along low between the big ones
		if (JumpQ > 0 && !bJumping && (JumpWait -= Dt) <= 0)
		{
			StartJump(JumpStyle == 3 || (JumpStyle == 1 && JumpQ > 1));
			JumpQ--;
			JumpWait = JumpStyle == 1 ? .05 : Rand(.15, .6);
		}
		ShakeT -= Dt;
		if (ShakeT <= 0)
		{
			ShakeT = Rand(.5, 1.8) * (1.4 - Stamina * .6);
			Tension += (bReel ? .08 : .04) * S;
			Shake = FMath::Max(Shake, .1 + .1 * S);
			RodKick = FMath::Min(1.0, RodKick + .55 + .3 * S);   // the rod jerks down with each head shake
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
		// reeling pulls the line toward what the fish is doing, it doesn't just climb: on a resting fish it holds a working
		// tension and gains line fast; into a running fish it climbs into the red (ease off, let the drag work, reel
		// again when it turns); with the fish in the air it's the quickest way to snap it. Pump and wind.
		// Holding the reel BUILDS tension, steadily: slowly on a resting fish, faster into a run, fastest with it in the
		// air. Let go and it bleeds back to what the fish's own pull holds on the line. Past the drag setting the drag
		// starts slipping line, which slows the climb (more drag = it holds more before it slips). So you pump: wind it
		// up into the yellow, ease off before the red, wind again. (It used to settle on a fixed level while you held,
		// so on a resting fish holding the reel could never take the bar past the middle.)
		const double Base = bInAir ? .05 : bRunningFish ? Clamp(.24 + .1 * Dg + .17 * Pull * (DeepT > 0 ? 1.2 : 1), 0, .9) : .1;
		if (bReel)
		{
			double Up = bInAir ? 1.6 : bRunningFish ? .38 + .5 * Pull * PullK : .3;
			const double SlipAt = .62 + .08 * Dg;   // the drag's setting
			if (Tension > SlipAt && !bInAir) Up *= .45;   // the drag slips line
			Tension = Clamp(Tension + Up * Dt, 0, 1.15);
			if (Tension < Base) Tension += (Base - Tension) * FMath::Min(1.0, Dt * 3);
		}
		else Tension += (Base - Tension) * FMath::Min(1.0, Dt * 1.4);
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
			// more tension on the line = more line gained per turn (a slack line just spins the spool)
			const double R = FMath::Max(4.0, 10.5 * (1 - .22 * S)) * (bRunningFish ? .3 : 1) * (.75 + .35 * (1 - Stamina)) * (.7 + .1 * Dg) * (HasGear(TEXT("reel.lever")) ? 1.3 : 1)
				* (.35 + 1.0 * Clamp(Tension, 0, 1));
			FX -= DX * R * Dt;
			FZ -= DZ * R * Dt;
		}
		Stamina = FMath::Max(0.0, Stamina - Dt * .1 * (.45 + Tension) / FMath::Sqrt(S));
		if (Tension >= LineBreakAt()) { OverT += Dt; if (OverT > 1.0) { Lose(TEXT("Snap! Too much pressure and the line parted.")); return; } }
		else OverT = FMath::Max(0.0, OverT - Dt);
		if (Tension < .06) { SlackT += Dt; if (SlackT > 2.6) { Lose(TEXT("Slack line - the fish shook the hook.")); return; } }
		else SlackT = 0;
		if (Dist > MaxLine) { Lose(TEXT("Spooled! Chase the big ones with the boat next time.")); return; }
		if (LandH(FX, FZ) > .2) { Lose(TEXT("Cut off in the mangroves.")); return; }
		LineDist = Dist;
		if (bScriptLogFight && FMath::Fmod(T, 1.0) < Dt)
			UE_LOG(LogTemp, Log, TEXT("KRFight %s dist %.1f tension %.2f stamina %.2f reel %d run %d jump %d"), *Key, Dist, Tension, Stamina, bReel ? 1 : 0, bRunningFish ? 1 : 0, bJumping ? 1 : 0);
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
	if (InChair() && (bAction || bReelKey)) VmCrank -= Dt * 22;   // the chair angler winds the same reel
	RodKick *= FMath::Exp(-Dt * 3.2);
	if (!bFpOn) return;
	const double Now = GetWorld()->GetTimeSeconds();
	double Bend = .01;
	double LungeWant = 0, YawWant = 0;
	if (FState == EKeysFish::Fight)
	{
		// the blank loads with the line, kicks and buzzes on every head shake, and throbs while the fish runs
		Bend = Clamp(Tension, 0, 1.1) * .2 + Shake * .08 * FMath::Sin(Now * 45) + RodKick * .11 * (.55 + .45 * FMath::Sin(Now * 31))
			+ (bRunningFish ? .025 * FMath::Sin(Now * 9.5) : 0);
		LungeWant = RodKick * .2 + (bRunningFish ? .06 + .04 * FMath::Sin(Now * 3.1) : 0) + Clamp(Tension - .6, 0, .5) * .2;
		// and the rod leans toward wherever the fish is
		const double Bear = FMath::Atan2(-(FX - X), -(FZ - Z));
		YawWant = FMath::Clamp(FMath::UnwindRadians(Bear - FpYaw) * .4, -.32, .32) * Clamp(Tension + .3, 0, 1);
	}
	else if (FState == EKeysFish::Bite) Bend = .09;
	else if (FState == EKeysFish::Nibble && DipT > 0) Bend = .045;
	VmLunge = Lerp(VmLunge, LungeWant, FMath::Min(1.0, Dt * 12));
	VmYawPull = Lerp(VmYawPull, YawWant, FMath::Min(1.0, Dt * 3));
	VmBend = Lerp(VmBend, Bend, FMath::Min(1.0, Dt * 10));
	double Rx = -1.15 + (bCharging ? Charge * .7 : 0);
	if (VmWhip > 0) { VmWhip = FMath::Max(0.0, VmWhip - Dt / .35); Rx -= .45 * FMath::Sin(VmWhip * PI); }
	// pumping a fish: the rod comes up as you hold against it, and drops as you wind down onto it
	const double PumpWant = FState == EKeysFish::Fight ? (bAction ? -.1 : .12 + .14 * Clamp(Tension, 0, 1)) : 0;
	VmPump = Lerp(VmPump, PumpWant, FMath::Min(1.0, Dt * (bAction ? 2.5 : 1.4)));
	Rx += VmPump;
	Rx += VmLunge;
	VmRx = Lerp(VmRx, Rx, FMath::Min(1.0, Dt * 14));
	if ((bReelKey && IsLineOut()) || (FState == EKeysFish::Fight && bAction)) VmCrank -= Dt * 22;   // the down arrow winds the reel
}

// A hand closed round a cylinder (a rod grip, a reel knob): the axis A through C, radius R; N points from the axis to the
// palm. Four fingers stacked along the axis wrap round it joint by joint (Curl 1 is a full fist), the thumb comes round
// the other way from the index side, and the wrist and forearm run back to the elbow, the sleeve over the forearm.
// IndexLift lifts the index finger off the grip (feeling the line). Sc scales the whole hand (1 = an adult's).
// the realistic hand (Content/KeysRun/Props/hand_grip.krfish, Tools/handglb.py from an Artlist model): a right fist closed
// round the grip with the forearm and shirt cuff, in meters in the grip frame - x along the rod (thumb toward +x), z out
// toward the forearm, y = z cross x, the origin on the rod axis in the middle of the fist
namespace
{
	struct FGripHandModel
	{
		TArray<FVector> P, N;
		TArray<FLinearColor> C;
		TArray<bool> bCloth;
		TArray<int32> T;
		FVector CuffEnd = FVector::ZeroVector;   // the middle of the cuff's open end
		double CuffR = .04;
		bool bOk = false;
	};

	const FGripHandModel& GripHandModel()
	{
		static FGripHandModel M;
		static bool bLoaded = false;
		if (bLoaded) return M;
		bLoaded = true;
		TArray<uint8> B;
		const FString Path = FPaths::ProjectContentDir() / TEXT("KeysRun/Props/hand_grip.krfish");
		if (!FFileHelper::LoadFileToArray(B, *Path) || B.Num() < 16 || FMemory::Memcmp(B.GetData(), "KRF1", 4)) return M;
		uint32 NV, NT;
		FMemory::Memcpy(&NV, B.GetData() + 8, 4); FMemory::Memcpy(&NT, B.GetData() + 12, 4);
		if (16 + int64(NV) * 28 + int64(NT) * 12 > B.Num()) return M;
		double ZMax = -1e9;
		for (uint32 i = 0; i < NV; i++)
		{
			const uint8* R = B.GetData() + 16 + i * 28;
			float F[6]; FMemory::Memcpy(F, R, 24);
			M.P.Add(FVector(F[0], F[1], F[2]));
			M.N.Add(FVector(F[3], F[4], F[5]).GetSafeNormal());
			const bool bBlue = int32(R[26]) > int32(R[24]) + 38;
			M.bCloth.Add(bBlue);
			M.C.Add(FLinearColor(FColor(R[24], R[25], R[26], 255)));
			if (bBlue) ZMax = FMath::Max(ZMax, double(F[2]));
		}
		const uint8* Tp = B.GetData() + 16 + int64(NV) * 28;
		for (uint32 t = 0; t < NT; t++)
		{
			uint32 I[3]; FMemory::Memcpy(I, Tp + t * 12, 12);
			if (I[0] < NV && I[1] < NV && I[2] < NV) M.T.Append({int32(I[0]), int32(I[1]), int32(I[2])});
		}
		// the cuff's open end: the cloth within a centimetre of the top
		FVector Sum = FVector::ZeroVector; int32 Cnt = 0;
		for (int32 i = 0; i < M.P.Num(); i++) if (M.bCloth[i] && M.P[i].Z > ZMax - .012) { Sum += M.P[i]; Cnt++; }
		if (Cnt) M.CuffEnd = Sum / Cnt;
		double Rs = 0;
		for (int32 i = 0; i < M.P.Num(); i++) if (M.bCloth[i] && M.P[i].Z > ZMax - .012) Rs += FVector::Dist(M.P[i], M.CuffEnd);
		if (Cnt) M.CuffR = Rs / Cnt;
		M.bOk = M.T.Num() > 0;
		UE_LOG(LogTemp, Log, TEXT("KeysRun: grip hand model %d verts %d tris, cuff end %s r %.3f"), M.P.Num(), M.T.Num() / 3, *M.CuffEnd.ToString(), M.CuffR);
		return M;
	}

	// the model hand round the grip: thumb toward +A, the forearm turned toward the elbow; the left hand is the right
	// one mirrored. The shirt sleeve carries on from the cuff to the elbow. False if there's no model.
	bool AddModelHand(FKeysMesh& M, const FVector& C, const FVector& A0, bool bLeft, const FVector& Elbow, const FLinearColor& Skin, const FLinearColor& Sleeve,
		const FLinearColor* Glove = nullptr, const FVector* Shoulder = nullptr)
	{
		const FGripHandModel& H = GripHandModel();
		if (!H.bOk) return false;
		const FVector A = A0.GetSafeNormal();
		const FVector ToE = Elbow - C;
		const FVector Z = (ToE - A * FVector::DotProduct(ToE, A)).GetSafeNormal(UE_SMALL_NUMBER, FVector::CrossProduct(A, FVector::UpVector).GetSafeNormal());
		const FVector Y = FVector::CrossProduct(Z, A) * (bLeft ? -1.0 : 1.0);
		auto Xf = [&](const FVector& P) { return C + A * P.X + Y * P.Y + Z * P.Z; };
		auto Xn = [&](const FVector& N) { return (A * N.X + Y * N.Y + Z * N.Z).GetSafeNormal(); };
		// tint: the skin toward the game's skin tone, the cloth to the game's shirt, keeping the model's own shading
		const float SkinL = Skin.GetLuminance(), SleeveL = Sleeve.GetLuminance();
		const uint8 Keep = M.Surf;
		M.SyncKinds();
		const int32 I0 = M.V.Num();
		for (int32 i = 0; i < H.P.Num(); i++)
		{
			const FLinearColor Src = H.C[i];
			const float L = Src.GetLuminance();
			FLinearColor Col;
			if (Glove)
			{
				// a black fishing glove: the hand in the glove's colour with the model's shading, the shirt cuff its cuff
				const float Sh = H.bCloth[i] ? FMath::Clamp(L / .35f, .7f, 1.25f) : FMath::Clamp(FMath::Pow(L / FMath::Max(SkinL, .05f), 1.6f), .35f, 2.2f);
				Col = *Glove * Sh * (H.bCloth[i] ? 1.15f : 1.f);
			}
			else if (H.bCloth[i]) Col = Sleeve * FMath::Clamp(L / .35f, .55f, 1.35f);
			else Col = FLinearColor::LerpUsingHSV(Src, Skin * FMath::Clamp(L / FMath::Max(SkinL, .05f), .4f, 1.4f), .35f);
			Col.A = 1;
			M.Surf = Glove ? SurfVinyl : H.bCloth[i] ? SurfCloth : SurfSkin;
			M.AddVert(Xf(H.P[i]), Xn(H.N[i]), Col);
		}
		for (int32 t = 0; t + 2 < H.T.Num(); t += 3)
		{
			const int32 a = I0 + H.T[t], b = I0 + H.T[t + 1], c = I0 + H.T[t + 2];
			const FVector Nf = M.N[a] + M.N[b] + M.N[c];
			const bool bSwap = FVector::DotProduct(FVector::CrossProduct(M.V[b] - M.V[a], M.V[c] - M.V[a]), Nf) > 0;
			M.T.Append({a, bSwap ? c : b, bSwap ? b : c});
		}
		if (Glove)
		{
			// the bare forearm out of the glove's cuff: narrow at the wrist, swelling to the muscle below the elbow, a
			// little flattened, sun-tanned and darker (hairier) along the top; then the elbow and the upper arm on back
			const FVector W0 = Xf(H.CuffEnd - FVector(0, 0, .035));
			// a straight forearm on along the cuff's own axis (no bend), long enough to run out of the frame
			// the wrist bends inside the cuff: the forearm runs straight from there to Elbow (out of the frame)
			const double FL = FMath::Max(.3, FVector::Dist(W0, Elbow));
			const FVector Fa = (Elbow - W0).GetSafeNormal();
			const FVector TopRef = Y * (bLeft ? -1.0 : 1.0);
			const FVector Top = (TopRef - Fa * FVector::DotProduct(TopRef, Fa)).GetSafeNormal(UE_SMALL_NUMBER, A);
			const FVector Side = FVector::CrossProduct(Fa, Top).GetSafeNormal();
			const int32 NU = 20, NV = 18;
			TArray<FVector> G;
			for (int32 v = 0; v < NV; v++)
			{
				const double t = double(v) / (NV - 1);
				const double Dm = t * FL;                       // meters from the wrist
				const FVector Cn = W0 + Fa * Dm;
				// the wrist inside the cuff, swelling to the muscle about 18 cm up, then steady
				const double Rw = FMath::Lerp(H.CuffR * .93, .033, FMath::SmoothStep(0., .07, Dm));
				const double R = Rw + .011 * FMath::SmoothStep(.06, .2, Dm);
				for (int32 u = 0; u < NU; u++)
				{
					const double a = 2 * PI * u / NU;
					G.Add(Cn + Top * FMath::Cos(a) * R * .86 + Side * FMath::Sin(a) * R * 1.08);
				}
			}
			const int32 F0 = M.V.Num();
			M.Surf = SurfSkin;
			M.AddGrid(G, NU, NV, true, Skin, W0 + (Elbow - W0) * .5);
			// mottle: hair and freckles darken the top of the arm, the underside is paler
			for (int32 i = F0; i < M.V.Num(); i++)
			{
				const double Up = FVector::DotProduct(M.N[i].GetSafeNormal(), TopRef);
				const double Nz = FMath::Frac(FMath::Sin(FVector::DotProduct(M.V[i], FVector(127.1, 311.7, 74.7)) * 1000.0) * 43758.5);
				const float Dk = float(1 - .16 * FMath::Max(0., Up) - .1 * Nz * FMath::Max(0., Up + .3));
				const float Pale = float(1 + .08 * FMath::Max(0., -Up));
				FLinearColor Cc = M.C[i] * Dk * Pale; Cc.A = 1;
				M.C[i] = Cc;
			}
			(void)Shoulder;
			M.Surf = Keep;
			return true;
		}
		// the sleeve on up to the elbow, from just inside the cuff
		M.Surf = SurfCloth;
		const FVector S0 = Xf(H.CuffEnd - FVector(0, 0, .01));
		const FVector Dir = (Elbow - S0).GetSafeNormal();
		const double SL = FVector::Dist(Elbow, S0);
		const int32 NS = 10;
		for (int32 k = 0; k < NS; k++)
		{
			const double t0 = double(k) / NS, t1 = double(k + 1) / NS;
			auto RR = [&](double t) { return FMath::Lerp(H.CuffR * 1.02, .047, t) * (1 + .03 * FMath::Exp(-t * 4) * FMath::Sin(t * 30 + 1.1)); };
			M.AddTube(S0 + Dir * SL * t0, S0 + Dir * SL * t1, RR(t0), RR(t1), 14, Sleeve);
		}
		M.Surf = Keep;
		(void)SleeveL;
		return true;
	}
}

void AddGripHand(FKeysMesh& M, const FVector& C, const FVector& A0, const FVector& N0, double R, double Curl, double IndexLift, bool bMirror,
	const FVector& Elbow, const FLinearColor& Skin, const FLinearColor& Sleeve, const FLinearColor& Nail)
{
	const FVector A = A0.GetSafeNormal();
	const FVector N = (N0 - A * FVector::DotProduct(N0, A)).GetSafeNormal(UE_SMALL_NUMBER, FVector::CrossProduct(A, FVector::UpVector).GetSafeNormal());
	const FVector B = bMirror ? FVector::CrossProduct(N, A) : FVector::CrossProduct(A, N);
	auto Round = [&](double Th, double Rho, double Sx) { return C + A * Sx + (N * FMath::Cos(Th) + B * FMath::Sin(Th)) * Rho; };
	const uint8 Keep = M.Surf;
	M.Surf = SurfSkin;
	// the palm: a thick pad lying along the grip, knuckles toward +B, the heel toward -B
	const FVector PalmC = Round(0, R + .017, -.004) - B * .006;
	M.AddRoundBox(PalmC, FVector(.082, .07, .026), .0125, Skin, FRotationMatrix::MakeFromXZ(A, N).ToQuat() * FQuat(FVector::XAxisVector, 0), 2);
	// the fingers: index (toward +A, by the thumb) to little finger
	const double Fw = .0215;
	const double Len[4][3] = {{.044, .026, .021}, {.049, .029, .023}, {.046, .027, .022}, {.036, .021, .019}};
	const double Rad[4] = {.0088, .0092, .0086, .0076};
	const double Rho0 = R + .0095 + (1 - Curl) * .018;
	for (int32 f = 0; f < 4; f++)
	{
		const double Sx = (1.5 - f) * Fw - .004;
		double Th = .95;   // the knuckle, round on the +B side of the grip
		double Rho = Rho0 + (f == 3 ? .002 : 0);
		FVector Prev = Round(Th, Rho + .004, Sx);
		KeysShapes::AddBlob(M, Prev, FVector(Rad[f] * 1.15), FQuat::Identity, Skin, 8, 4);
		for (int32 j = 0; j < 3; j++)
		{
			const double L = Len[f][j];
			double Step = 2 * FMath::Asin(FMath::Min(.99, L / (2 * Rho))) * (.55 + .45 * Curl);
			double Out = 0;
			if (f == 0 && IndexLift > 0) { Step *= 1 - .7 * IndexLift; Out = IndexLift * .012 * (j + 1); }
			Th += Step;
			const FVector Nx = Round(Th, Rho + Out, Sx - (f == 3 ? .002 * j : 0));
			const double R0 = Rad[f] * (1 - .1 * j), R1 = Rad[f] * (1 - .1 * (j + 1));
			M.AddTube(Prev, Nx, R0, R1, 8, Skin);
			KeysShapes::AddBlob(M, Nx, FVector(R1), FQuat::Identity, Skin, 8, 4);
			if (j == 2)
			{
				// the nail, on the back of the fingertip
				const FVector Back = (Nx - (C + A * Sx)).GetSafeNormal();
				M.Surf = SurfDefault;
				KeysShapes::AddBlob(M, Nx + Back * R1 * .55 - (Nx - Prev).GetSafeNormal() * R1 * .3, FVector(R1 * .75, R1 * .75, R1 * .3), FRotationMatrix::MakeFromZ(Back).ToQuat(), Nail, 8, 3);
				M.Surf = SurfSkin;
			}
			Prev = Nx;
		}
	}
	// the thumb: from the heel of the hand on the index side, round the grip the other way to meet the index finger
	{
		double Th = -.55;
		const double Rho = R + .011;
		double Sx = 1.5 * Fw + .018;
		FVector Prev = Round(Th, Rho + .012, Sx);
		KeysShapes::AddBlob(M, Prev, FVector(.016, .014, .014), FQuat::Identity, Skin, 8, 4);
		const double TL[3] = {.036, .032, .027};
		for (int32 j = 0; j < 3; j++)
		{
			Th -= 2 * FMath::Asin(FMath::Min(.99, TL[j] / (2 * Rho))) * (.45 + .4 * Curl);
			Sx -= .007;
			const FVector Nx = Round(Th, Rho + (j == 0 ? .006 : 0), Sx);
			const double R0 = .0115 - .0012 * j, R1 = .0115 - .0012 * (j + 1);
			M.AddTube(Prev, Nx, R0, R1, 8, Skin);
			KeysShapes::AddBlob(M, Nx, FVector(R1), FQuat::Identity, Skin, 8, 4);
			Prev = Nx;
		}
	}
	// the wrist behind the heel of the hand, and the forearm back to the elbow
	const FVector Wrist = Round(-.35, R + .028, -.01) - B * .045;
	const FVector ToE = (Elbow - Wrist).GetSafeNormal();
	KeysShapes::AddBlob(M, Wrist, FVector(.026, .03, .022), FRotationMatrix::MakeFromXZ(ToE, N).ToQuat(), Skin, 10, 5);
	M.AddTube(Wrist, Wrist + ToE * .075, .025, .029, 10, Skin);
	M.Surf = SurfCloth;
	{
		// the sleeve: loose fabric that bunches into folds behind the cuff and hangs a little under the forearm,
		// with a seam down the outside, rather than a straight pipe
		const FVector S0 = Wrist + ToE * .06;
		const double SL = (Elbow - S0).Size();
		const FVector Down = (FVector::DownVector - ToE * FVector::DotProduct(FVector::DownVector, ToE)).GetSafeNormal(UE_SMALL_NUMBER, N);
		const int32 NS = 14;
		auto RingC = [&](double t) { return S0 + ToE * (SL * t) + Down * (.006 * FMath::Sin(t * PI)); };
		auto RingR = [&](double t)
		{
			const double Base = FMath::Lerp(.04, .047, t);
			const double Bunch = FMath::Exp(-t * 5.0);   // folds pile up near the cuff and smooth out up the arm
			return Base * (1 + (.07 * Bunch + .015) * FMath::Sin(t * 38.0 + 1.1));
		};
		for (int32 k = 0; k < NS; k++)
		{
			const double t0 = double(k) / NS, t1 = double(k + 1) / NS;
			M.AddTube(RingC(t0), RingC(t1), RingR(t0), RingR(t1), 14, Sleeve);
		}
		// the seam down the outside of the sleeve, a shade darker
		const FVector Out = FVector::CrossProduct(ToE, Down).GetSafeNormal() * (bMirror ? -1 : 1);
		for (int32 k = 0; k < NS; k++)
		{
			const double t0 = double(k) / NS, t1 = double(k + 1) / NS;
			M.AddTube(RingC(t0) + Out * (RingR(t0) - .001), RingC(t1) + Out * (RingR(t1) - .001), .0018, .0018, 4, Sleeve * .8f);
		}
		// the cuff: a turned-back band with a button
		M.AddTube(Wrist + ToE * .052, Wrist + ToE * .078, .037, .042, 14, Sleeve * .9f);
		M.AddTube(Wrist + ToE * .05, Wrist + ToE * .054, .036, .037, 14, Sleeve * .78f);
		M.Surf = SurfDefault;
		KeysShapes::AddBlob(M, Wrist + ToE * .065 + Out * .042, FVector(.0045, .0045, .0018), FRotationMatrix::MakeFromZ(Out).ToQuat(), FLinearColor(.9f, .9f, .88f), 8, 3);
	}
	M.Surf = Keep;
}

// the first-person rod, reel and hands, held in front of the camera (the JS view model, in three.js camera space:
// x right, y up, z back). Returns the rod tip in world meters, Unreal axes.
FVector AKeysBoatPawn::BuildViewModel(FKeysMesh& M)
{
	const FTransform CT = Camera->GetComponentTransform();
	auto W = [&CT](const FVector& V) { return CT.TransformPosition(FVector(-V.Z, V.X, V.Y) * 100.0) / 100.0; };
	// Ry(.16) * Rx(Th): the rod base's yaw-pitch-roll ("YXZ") rotation
	// while a fish is in the air the rod drops down and out to the right, so it isn't across the jump
	const double Jl = JumpLook * JumpLook * (3 - 2 * JumpLook);
	const double Yw = .16 + .42 * Jl - VmYawPull;
	auto Rot = [Yw](double Th, const FVector& V)
	{
		const double C = FMath::Cos(Th), Sn = FMath::Sin(Th), Cy = FMath::Cos(Yw), Sy = FMath::Sin(Yw);
		const FVector A(V.X, V.Y * C - V.Z * Sn, V.Y * Sn + V.Z * C);
		return FVector(A.X * Cy + A.Z * Sy, A.Y, -A.X * Sy + A.Z * Cy);
	};
	const FVector Base = FVector(.25, -.33, -.54) + FVector(.16, -.1, .05) * Jl;   // pulled in a little so the hands fill more of the frame
	const double Rx = VmRx - .3 * Jl;
	auto S0 = [&](double A, double B, double C) { return Base + Rot(Rx, FVector(A, B, C)); };   // point in the butt section's frame
	const FLinearColor RodC(FColor(0x1d, 0x21, 0x26)), Cork(FColor(0xb8, 0x92, 0x5e)), DarkC(FColor(0x22, 0x26, 0x2b)), GoldC(FColor(0xc9, 0xa2, 0x27)),
		Skin(FColor(0xd9, 0xa2, 0x7f)), Sleeve(FColor(0x6f, 0xa6, 0xcc)), Guide(FColor(0xc9, 0xce, 0xd3));
	const FQuat CQ = CT.GetRotation();

	// where the bending rod's tip ends up: five sections, each bent a little more toward the load
	const double SegL = .52;
	FVector P = Base;
	for (int32 i = 0; i < 5; i++) P += Rot(Rx - VmBend * i, FVector(0, 1, 0)) * SegL;
	(void)RodC; (void)Cork; (void)DarkC; (void)GoldC; (void)Guide; (void)CQ;
	// the tuna rod and lever-drag reel, the reel where the old one sat, the hands on the fore grip and the crank
	KeysMesh::FRodPose RP;
	RP.Butt = W(S0(0, -.115, 0));
	RP.Dir = (W(S0(0, 1, 0)) - W(S0(0, 0, 0))).GetSafeNormal();
	RP.Up = W(S0(0, 0, -1)) - W(S0(0, 0, 0));
	RP.Len = SegL * 5 + .115;
	RP.Bend = W(P) - (RP.Butt + RP.Dir * RP.Len);
	RP.Lod = 0;
	RP.CrankA = PI / 4 + VmCrank;
	const FVector TipW = KeysMesh::BuildTunaRod(M, RP);
	// the hands. The left closes round the fore grip ahead of the reel, squeezing harder the harder the fish pulls, its
	// index finger lifting to feel the line when a fish is mouthing the bait; the right holds the crank knob and goes
	// round with it, the wrist turning and the forearm swinging from the elbow as it winds.
	const FLinearColor Nail(FColor(0xe8, 0xc4, 0xb4));
	const double Squeeze = FState == EKeysFish::Fight ? Clamp(Tension, 0, 1) : 0;
	const double Lift = (FState == EKeysFish::Nibble || FState == EKeysFish::Waiting) ? .5 + .5 * FMath::Sin(GetWorld()->GetTimeSeconds() * 1.3) * (FState == EKeysFish::Nibble ? 1 : .3) : 0;
	{
		const FVector GripC = RP.Butt + RP.Dir * .76;
		const FVector ElbowL = W(FVector(-.12, -.78, -.12));
		// the realistic model hand in a black fishing glove, the bare tanned forearm running straight up from the bottom
		// of the frame, the wrist bending inside the glove's cuff. The diver's gloved hands stay procedural.
		const FVector ElbowFa = W(FVector(-.36, -.62, .05));
		const FLinearColor GloveC(FColor(0x44, 0x45, 0x4a)), Tan(FColor(0xb2, 0x86, 0x6c));
		if (!AddModelHand(M, GripC, RP.Dir, true, ElbowFa, Tan, Sleeve, &GloveC, nullptr))
			AddGripHand(M, GripC, RP.Dir, RP.OutUp + (ElbowL - GripC).GetSafeNormal() * .5, .025, .8 + .2 * Squeeze, Lift, false, ElbowL, Skin, Sleeve, Nail);
	}
	{
		const double Cc = FMath::Cos(VmCrank), Sn = FMath::Sin(VmCrank);
		const FVector ElbowR = W(FVector(.52 + .025 * Sn, -.74 + .03 * Cc, -.1 + .02 * Sn));
		if (!AddModelHand(M, RP.OutKnob, RP.OutKnobAxis, false, ElbowR, Skin, Sleeve))
			AddGripHand(M, RP.OutKnob, RP.OutKnobAxis, ElbowR - RP.OutKnob, .023, 1, 0, true, ElbowR, Skin, Sleeve, Nail);
	}
	return TipW;
}

// When a hooked fish jumps, the camera zooms in and swings to it, then eases back out once it's in the water again.
void AKeysBoatPawn::UpdateJumpCam(double Dt)
{
	// the chair fight's own cameras take the jump cam too (it swings and zooms from where they put the lens)
	if (IsDiving() || (bExtCam && (!InChair() || bDbgCam))) return;
	// only once the fish is breaking the surface (zooming onto it under water just fills the frame with sea)
	bool bOn = bJumping && FState == EKeysFish::Fight;
	if (bOn)
	{
		const FVector Fu = FishMesh->GetComponentLocation() / 100.0;
		bOn = Fu.Z > WaveH(Fu.X, Fu.Y, GetWorld()->GetTimeSeconds(), AmpAt(Fu.X, Fu.Y)) - FishLen * .25;
	}
	// after the hit's first jump: 1.5 s on the guy in the chair, unzoomed, before first person takes over
	const bool bChairBeat = InChair() && ChairIntroJump == 2 && ChairIntroT > 0;
	if (bChairBeat) bOn = false;
	JumpLook = FMath::FInterpConstantTo(JumpLook, bOn ? 1.0 : 0.0, Dt, bOn ? 3.0 : bChairBeat ? 5.0 : 1.2);
	// the chair cameras set their own field of view each frame: start the zoom from it and hand it back after
	if (bExtCam && !bOn && JumpLook < .001) { JumpFov = Camera->FieldOfView; return; }
	float Want = bExtCam ? Camera->FieldOfView : 88.f;
	// the landing shot's long lens: cut to it, and cut back out of it when the catch is done
	const bool bLand = FState == EKeysFish::Caught && !bExtCam && !IsDiving();
	if (bLand || bWasLanding)
	{
		if (!bLand)
		{
			// out of the landing shot: hand the lens back without its depth of field and vignette
			FPostProcessSettings& PP = Camera->PostProcessSettings;
			PP.bOverride_DepthOfFieldFocalDistance = PP.bOverride_DepthOfFieldFstop = PP.bOverride_DepthOfFieldSensorWidth = PP.bOverride_VignetteIntensity = false;
		}
		bWasLanding = bLand; JumpFov = bLand ? CatchFov : 88.f; Camera->SetFieldOfView(JumpFov);
		if (bLand) return;
	}
	if (bOn)
	{
		const FVector F = FishMesh->GetComponentLocation(), C = Camera->GetComponentLocation();
		const double D = FVector::Dist(F, C) / 100;
		const double Across = FishLen * 3 + 2.5;   // meters of scene across the frame
		// the full jump cam in first person too: the rod drops away out of frame (BuildViewModel's JumpLook), so the
		// lens can swing right onto the fish and zoom in on it
		const float MinFov = IsFirstPerson() ? 32.f : 11.f;
		Want = FMath::Clamp(float(FMath::RadiansToDegrees(2 * FMath::Atan(Across * .5 / FMath::Max(D, 1.0)))), MinFov, 88.f);
	}
	JumpFov = FMath::FInterpTo(JumpFov, Want, Dt, bOn ? 4.f : 1.8f);
	Camera->SetFieldOfView(JumpFov);
	if (JumpLook > .001)
	{
		const FVector F = FishMesh->GetComponentLocation(), C = Camera->GetComponentLocation();
		const FQuat To = (F - C).Rotation().Quaternion(), From = Camera->GetComponentQuat();
		const double A = JumpLook * JumpLook * (3 - 2 * JumpLook);
		Camera->SetWorldRotation(FQuat::Slerp(From, To, float(A)));
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
	if (bFpOn && FishCamT < 0) TipU = BuildViewModel(M);   // (the fish cam has the lens: the rod is back in your hands)
	else
	{
		const FVector Tip = RodTip(End);
		if (InChair() && BoatMesh)
		{
			// the butt in the chair's gimbal, the rod up at the fish over the angler's knees
			const FTransform BT = BoatMesh->GetComponentTransform();
			const FVector Base = KeysMesh::FightChairBase(Spec());
			const FVector G = KeysMesh::ChairGimbal(Base, ChairYaw);
			const FVector Dl = FQuat(FVector::UpVector, ChairYaw).RotateVector(FVector(FMath::Cos(.75), 0, FMath::Sin(.75)));
			KeysMesh::FRodPose RP;
			RP.Butt = BT.TransformPosition(G * 100.0) / 100.0;
			RP.Dir = BT.TransformVector(Dl).GetSafeNormal();
			RP.Up = BT.TransformVector(FVector(0, 0, 1));
			// the same rod and reel you cast with (BuildViewModel): full length, full detail, the crank turning
			RP.Len = .52 * 5 + .115;
			RP.Lod = 0;
			RP.CrankA = PI / 4 + VmCrank;
			const double Bend = FState == EKeysFish::Fight ? Clamp(Tension, 0, 1) : 0;
			RP.Bend = (G2U(End) - (RP.Butt + RP.Dir * RP.Len)).GetSafeNormal() * (Bend * .7 + RodKick * .35 * (.6 + .4 * FMath::Sin(T * 31)));
			TipU = KeysMesh::BuildTunaRod(M, RP);
		}
		else
		{
		const double Rx = FMath::Cos(H), Rz = -FMath::Sin(H);
		const double Sgn = ((End.X - X) * Rx + (End.Z - Z) * Rz) >= 0 ? 1 : -1;
		FVector Butt(X + Rx * Sgn * (Spec().Beam * .5 - .25) + FMath::Sin(H) * Spec().Len * .15, Y + 1.25, Z + Rz * Sgn * (Spec().Beam * .5 - .25) + FMath::Cos(H) * Spec().Len * .15);
		if (bShoreSwap) { const FVector D2 = FVector(Tip.X - X, 0, Tip.Z - Z).GetSafeNormal(); Butt = FVector(X + D2.X * .35, Y + .95, Z + D2.Z * .35); }
		// the rod bends toward the load when a fish is pulling
		const double Bend = FState == EKeysFish::Fight ? Clamp(Tension, 0, 1) : 0;
		const FVector Mid = (Butt + Tip) * .5 + (End - Tip).GetSafeNormal() * Bend * .5;
		const double Kick = RodKick * (.6 + .4 * FMath::Sin(T * 31));
		const FVector RodTipBent = Tip + (End - Tip).GetSafeNormal() * (Bend * .6 + Kick * .3) - FVector(0, Bend * .5 + Kick * .25, 0);
		(void)Mid;
		// the tuna rod: butt at the angler's hip, the blank bowing toward the load
		KeysMesh::FRodPose RP;
		RP.Butt = G2U(Butt) - (G2U(Tip) - G2U(Butt)).GetSafeNormal() * .6;
		RP.Dir = (G2U(Tip) - RP.Butt).GetSafeNormal();
		RP.Len = (G2U(Tip) - RP.Butt).Size();
		RP.Up = FVector::UpVector;
		RP.Bend = G2U(RodTipBent) - G2U(Tip);
		RP.Lod = 0;
		TipU = KeysMesh::BuildTunaRod(M, RP);
		}
	}
	FKeysMesh LineM;   // the line alone, in its own see-through section
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
		// thin clear mono: a few millimetres up close, and just under a pixel far off so it reads as a faint thread
	auto Rad = [&CamU](const FVector& Q) { return FMath::Max(.0035, (Q - CamU).Size() * .00055); };
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
			LineM.AddTube(Prev, P, Rad(Prev), Rad(P), 4, FLinearColor(.78f, .86f, .84f));   // clear mono, a faint blue-green glint
			Prev = P;
		}
		Bobber->SetWorldLocation(ToUE(Bob.X, Bob.Y + .05, Bob.Z));
	}
	// the line is see-through: the windshield glass material at a higher opacity
	static TWeakObjectPtr<UMaterialInstanceDynamic> LineMat;
	if (!LineMat.IsValid())
		if (UMaterialInterface* G = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/KeysRun/Materials/M_KR_Glass.M_KR_Glass")))
		{
			LineMat = UMaterialInstanceDynamic::Create(G, GetTransientPackage());
			LineMat->SetScalarParameterValue(TEXT("Opacity"), .45f);
			LineMat->AddToRoot();
		}
	M.Commit(LineMesh, 0, AKeysWorld::VertexColorMaterial());
	if (LineM.T.Num()) LineM.Commit(LineMesh, 1, LineMat.IsValid() ? (UMaterialInterface*)LineMat.Get() : AKeysWorld::VertexColorMaterial());
	else LineMesh->ClearMeshSection(1);
}

// ================================================================ HUD text
FString AKeysBoatPawn::Hint() const
{
	if (!bRunning) return FString();
	const double Kn = KnotsNow();
	const FMarina* Mar = NearMarina(X, Z);
	if (FState == EKeysFish::Idle && !bCharging && Mar && Kn < 5)
		return Cooler.Num() ? FString::Printf(TEXT("Press E to weigh in your cooler at %s"), *Mar->Name) : FString::Printf(TEXT("Tied up at %s - nothing on ice yet"), *Mar->Name);
	if (FState == EKeysFish::Caught)
		if (const AKeysCharter* Ch = AKeysCharter::Get(this); Ch && Ch->IsPendingRelease())
			return TEXT("Charter: SPACE releases it and it counts (T tags and releases) - keeping it fails the charter");
	if (FState == EKeysFish::Caught) return bCardSpeared || CurTag >= 0 ? TEXT("K keeps it in the cooler - Space releases it") : TEXT("K keeps it in the cooler - Space releases it - T tags it and lets it go");
	if (bTrimMode && FState == EKeysFish::Idle && !bCharging) return TEXT("Trim mode: Up/Down trim and tilt - Left/Right tabs - A/D steer - W/S throttle - T to exit");
	if (bCharging) return TEXT("Release to cast - hold longer to throw farther");
	if (bTrolling && FState == EKeysFish::Idle)
		return TResume > 0 ? FString(TEXT("Clearing the lines - back on the troll in a moment"))
			: TEXT("Trolling - A/D steer the spread over weedlines, birds and the edge - = brings the lines in");
	if (bDriveOn && FState == EKeysFish::Idle) return TEXT("G shows the chartplotter and fish finder - [ ] zoom the chart - 1 switches view");
	switch (FState)
	{
		case EKeysFish::Idle: return Kn > 3 ? TEXT("W/S throttle - A/D steer - H changes the sea state - O for sunset") : TEXT("Hold Space to power a cast - drag the mouse to aim");
		case EKeysFish::Casting:
		case EKeysFish::Waiting: return TEXT("Watch the float - hold the down arrow to retrieve - R reels in");
		case EKeysFish::Nibble: return TEXT("Nibble... wait for the float to go under");
		case EKeysFish::Bite: return TEXT("SPACE now to set the hook!");
		case EKeysFish::Fight: return (bAutoOn ? AutoMode + TEXT(" - ") : FString()) + TEXT("Hold the down arrow to reel - ease off when red - T/Shift+T drag - F toggles auto-chase");
		default: return FString();
	}
}

FString AKeysBoatPawn::Warning() const
{
	const double D = DepthAt(X, Z) + TideRise();
	const double Hull = TrimProfile().HullDraftM;
	if (D < Hull) return TEXT("Aground - back off to deeper water");
	if (D < PropDepthNow + .1 && PropDepthNow > .05) return TEXT("Props in the bottom - trim up / tilt (trim mode, Up)");
	if (D < Spec().Draft + .4 && KnotsNow() > 5) return TEXT("Shallow water ahead");
	return FString();
}

// ================================================================ frame
void AKeysBoatPawn::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	const double Dt = FMath::Min(double(DeltaSeconds), 1.0 / 20);
	const double T = GetWorld()->GetTimeSeconds();
	KeysGfx::Tick(GetWorld(), DeltaSeconds);
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
	if (ReboardT >= 0)
	{
		// J from far off while diving: fade out, end the dive at the black, fade back in on deck
		const double Was = ReboardT;
		ReboardT += Dt;
		if (Was < .4 && ReboardT >= .4)
		{
			AKeysDiver* D = AKeysDiver::Get(this);
			if (D && D->IsDiving())
			{
				const bool bLost = D->HasFishOnLine();
				D->ExitToBoat();
				SyncChaseCam();
				bCamInit = false;
				ShowToast(bLost ? TEXT("Lost the fish - back aboard") : TEXT("Back aboard"), 3);
			}
		}
		if (ReboardT >= .8) ReboardT = -1;
	}
	if (bRunning && OffshoreF(X, Z) > .5 && !bDeepTip) { bDeepTip = true; ShowToast(TEXT("Deep blue water - the farther out you run, the bigger the fish.")); }
	if (IsAshore()) { Thr = 0; Speed = 0; DVX = DVZ = 0; Steer = 0; }
	UpdateBoat(Dt, T);
	if (IsAshore()) { X = PinX; Z = PinZ; H = PinH; Speed = 0; }
	if (bRunning) { BeginShoreSwap(); UpdateFishing(Dt, T); EndShoreSwap(); }
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
		S.bReeling = bReelKey || (bAction && FState == EKeysFish::Fight);
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
	SmoothCameraCuts(Dt);
	UpdateViewModel(Dt);
	BeginShoreSwap();
	{
		// the chair swivels to follow the fish's bearing, and back to face the wake when it's over
		double Want = PI;
		if (InChair() && BoatMesh)
		{
			const FVector L = BoatMesh->GetComponentTransform().InverseTransformPosition(ToUE(FX, 0, FZ)) / 100.0;
			Want = FMath::Atan2(L.Y, L.X);
			if (FMath::Cos(Want) > -.2) Want = FMath::Atan2(FMath::Sign(L.Y) * .98, -.2);   // it can't face forward through the console
		}
		const double D = FMath::UnwindRadians(Want - ChairYaw);
		ChairYaw = FMath::UnwindRadians(ChairYaw + D * FMath::Min(1.0, Dt * 2.5));
	}
	DrawLineAndRod(T);
	EndShoreSwap();
	UpdateSpread(Dt, T);
	UpdateReleaseAnim(Dt);
	if (LureCamN >= 0 && LureCamN < NumStations && bSpreadOut)
	{
		// test only (lurecam=N): sit just off the side of a trolled lure, looking at it
		const FVector L(Lures[LureCamN].X, Lures[LureCamN].Z, Lures[LureCamN].Y);
		const FVector Pu = (L + FVector(LureCamOff.X, LureCamOff.Y, LureCamOff.Z)) * 100.0;
		Camera->SetWorldLocationAndRotation(Pu, (L * 100.0 - Pu).Rotation());
	}
}
