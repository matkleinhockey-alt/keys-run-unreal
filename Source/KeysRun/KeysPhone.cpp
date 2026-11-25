// The phone (P): one place for everything that used to be a key of its own.
//
// The keyboard had a letter for the sea state, one for sunset, one for the gearbox, one for the cockpit lights, one for
// the buddy boat, one to jump to Miami... none of them on the screen, and most of them one slip away from the keys you
// do use. The phone gathers them: a handset slides up in the corner with a grid of apps (Travel, Chart, Radio, Weather,
// Boat, Camera, Crew, Events, Friends, Gear, Land, Settings); an app is a short list of rows, each showing what it is set
// to now. The game carries on behind it (the boat coasts, a troll keeps trolling), and a bite puts the phone away.
//
// Like the action key (KeysAction.cpp) it adds no rule of its own: every row calls what the old key or the menu row
// called. Travel is the teleport list as a list you can read, in place of the pause menu's one row you cycled.
//
// The look is the Scrap King handset (GameUI.Devices.cs there): a rubber bumper with screws and side buttons, a status
// bar (clock, signal, battery), a dusk wallpaper behind chunky drawn icons, a title bar with a back chevron in each app,
// a home indicator. Drawn here with the HUD's rectangles and triangles; the wallpaper is the Seven Mile Bridge at dusk.
//
// Keys: P opens and closes. Arrows or W A S D move, Enter / Space / E choose, Left / Right change a setting, Esc or
// Backspace go back. The mouse works on every row (hit boxes "ph:"), and the wheel scrolls a long list.

#include "KeysBoatPawn.h"
#include "KeysHUD.h"
#include "KeysAudio.h"
#include "KeysExtras.h"
#include "KeysHandgun.h"
#include "KeysEstates.h"
#include "KeysSlots.h"
#include "KeysWalker.h"
#include "KeysOnline.h"
#include "KeysWorld.h"
#include "KeysScuba.h"
#include "KeysGeo.h"
#include "KeysData.h"
#include "KeysKit.h"
#include "CanvasItem.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Engine/Font.h"
#include "Engine/Texture2D.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/GameViewportClient.h"
#include "UnrealClient.h"
#include "RenderUtils.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "InputCoreTypes.h"
#include "Misc/DateTime.h"

// ================================================================ the pawn's side: what is on the phone, and doing it

bool AKeysBoatPawn::CanUsePhone() const
{
	if (!bRunning || Menu != EMenu::None || bLoginOpen || bBoardOpen || bGearOpen || SvcCard.bOn) return false;
	if (IsDiving()) return false;
	// both hands are busy: a cast winding up, a fish on, a fish on the card. (A line just sitting out is fine.)
	if (bCharging || FState == EKeysFish::Bite || FState == EKeysFish::Fight || FState == EKeysFish::Caught) return false;
	if (const AKeysHandgun* PhGun = AKeysHandgun::Get(this); PhGun && PhGun->IsOut()) return false;
	if (const AKeysSlots* PhSlots = AKeysSlots::Get(this); PhSlots && PhSlots->bOpen) return false;
	if (const AKeysEstates* PhEst = AKeysEstates::Get(this); PhEst && PhEst->bOpen) return false;
	if (const AKeysWalker* PhWalk = AKeysWalker::Get(this); PhWalk && PhWalk->bShopOpen) return false;
	return true;
}

void AKeysBoatPawn::OpenPhone()
{
	bPhoneOpen = true;
	PhoneApp = NAME_None;
	PhoneSel = PhoneTop = PhoneHomeSel = 0;
	PhoneOpenT = GetWorld()->GetRealTimeSeconds();
}

void AKeysBoatPawn::ClosePhone()
{
	bPhoneOpen = false;
	PhoneApp = NAME_None;
}

void AKeysBoatPawn::PhoneBack()
{
	if (PhoneApp.IsNone()) { ClosePhone(); return; }
	PhoneApp = NAME_None;
	PhoneSel = PhoneHomeSel;
	PhoneTop = 0;
}

void AKeysBoatPawn::GetPhoneItems(TArray<FMenuItem>& Out) const
{
	Out.Reset();
	auto Add = [&Out](const TCHAR* Id, const FString& Label, const FString& Value = FString(), bool bArrows = false) { Out.Add({FName(Id), Label, Value, bArrows}); };
	const AKeysAudio* Aud = AKeysAudio::Get(this);
	const AKeysExtras* Ext = AKeysExtras::Get(this);
	const AKeysOnline* Net = AKeysOnline::Get(this);
	const bool bNet = Net && Net->NetState() != EKeysNet::Offline;
	const FString Station = Aud ? Aud->GetRadioLabel().Replace(TEXT("Radio: "), TEXT("")) : FString(TEXT("Off"));
	const TCHAR* ViewName = View == 0 ? TEXT("Chase") : View == 1 ? TEXT("Helm") : TEXT("Tuna tower");

	// the home screen: the apps, each with a line of what it is set to
	if (PhoneApp.IsNone())
	{
		TArray<FRunDest> Places;
		RunDests(Places, true);
		Add(TEXT("app:travel"), TEXT("Travel"), FString::Printf(TEXT("%d places"), Places.Num()));
		Add(TEXT("app:chart"), TEXT("Chart"), bBigMap ? TEXT("Open") : TEXT(""));
		Add(TEXT("app:radio"), TEXT("Radio"), Station);
		Add(TEXT("app:weather"), TEXT("Weather"), KeysGeo::SeaStates()[KeysGeo::SeaIndex()].Name);
		Add(TEXT("app:boat"), TEXT("Boat"), Aud && !Aud->EnginesOn() ? TEXT("Motors off") : TEXT("Motors on"));
		Add(TEXT("app:camera"), TEXT("Camera"), ViewName);
		Add(TEXT("app:crew"), TEXT("Crew"), Ext && Ext->IsBuddyOn() ? TEXT("Buddy out") : TEXT(""));
		Add(TEXT("app:events"), TEXT("Events"), bFreeFish ? TEXT("Free Fish") : TEXT("Charter"));
		Add(TEXT("app:friends"), TEXT("Friends"), bNet ? TEXT("Online") : TEXT("Solo"));
		Add(TEXT("app:gear"), TEXT("Gear"), KeysKit::Money(double(Cash)));
		Add(TEXT("app:land"), TEXT("Land"), Estates.Num() ? FString::Printf(TEXT("%d lots"), Estates.Num()) : FString());
		Add(TEXT("app:settings"), TEXT("Settings"));
		return;
	}

	if (PhoneApp == TEXT("travel"))
	{
		// every place you have been near, with how far it is: Enter goes there through black
		TArray<FRunDest> Places;
		RunDests(Places, true);
		for (int32 i = 0; i < Places.Num(); i++)
		{
			const double Km = FMath::Sqrt(FMath::Square(Places[i].X - X) + FMath::Square(Places[i].Z - Z)) / 1000;
			const FString Far = Km < 1 ? FString::Printf(TEXT("%d m"), FMath::RoundToInt(Km * 1000)) : FString::Printf(TEXT("%.1f km"), Km);
			Out.Add({FName(*FString::Printf(TEXT("go:%d"), i)), Places[i].Name, Places[i].Note + TEXT("  -  ") + Far, false});
		}
		Add(TEXT("miami"), TEXT("Miami run"), TEXT("up to Biscayne Bay, or back home from it"));
	}
	else if (PhoneApp == TEXT("radio"))
	{
		Add(TEXT("radio"), TEXT("Station"), Station, true);
		Add(TEXT("sound"), TEXT("Sound"), Aud && Aud->IsSoundOn() ? TEXT("On") : TEXT("Off"));
	}
	else if (PhoneApp == TEXT("weather"))
	{
		AKeysWorld* Wd = const_cast<AKeysBoatPawn*>(this)->World();
		Add(TEXT("sea"), TEXT("Sea state"), KeysGeo::SeaStates()[KeysGeo::SeaIndex()].Name, true);
		Add(TEXT("time"), TEXT("Time of day"), Wd && Wd->IsSunset() ? TEXT("Sunset") : TEXT("Day"));
	}
	else if (PhoneApp == TEXT("boat"))
	{
		Add(TEXT("motors"), TEXT("Motors"), Aud && !Aud->EnginesOn() ? TEXT("Off") : TEXT("On"));
		Add(TEXT("shift"), TEXT("Gearbox"), Gear == 0 ? TEXT("Forward") : Gear == 1 ? TEXT("Neutral") : TEXT("Reverse"), true);
		Add(TEXT("trim"), TEXT("Trim mode"), bTrimMode ? TEXT("On - the arrows trim and work the tabs") : TEXT("Off"));
		Add(TEXT("troll"), TEXT("Troll"), bTrolling ? TEXT("Lines out, holding 5 kn") : TEXT("Lines in"));
		Add(TEXT("plotter"), TEXT("Chartplotter and fish finder"), bElectronics ? TEXT("On") : TEXT("Off"));
		Add(TEXT("lights"), TEXT("Underwater and cockpit lights"), bUwLights ? TEXT("On") : TEXT("Off"));
		Add(TEXT("autochase"), TEXT("Follow a hooked fish"), bAutoChase ? TEXT("On") : TEXT("Off"));
		Add(TEXT("fuel"), TEXT("Fuel"), bUnlimitedFuel ? FString(TEXT("Unlimited")) : FString::Printf(TEXT("Realistic - %d%% in the tank"), FMath::RoundToInt(Fuel * 100)));
		if (KeysScuba::Cylinders(this) > 0) Add(TEXT("scuba"), TEXT("Next dive"), TEXT("scuba, or on a breath"));
	}
	else if (PhoneApp == TEXT("camera"))
	{
		Add(TEXT("view"), TEXT("Camera"), ViewName, true);
		Add(TEXT("fpview"), TEXT("Fishing view"), bFpPref ? TEXT("First person") : TEXT("Third person"));
		Add(TEXT("zoom"), TEXT("Chase distance"), CamZoom > 1.4 ? TEXT("Wide") : TEXT("Close"));
		Add(TEXT("diveview"), TEXT("Dive view"), bDiveChase ? TEXT("Chase") : TEXT("Dive mask"));
	}
	else if (PhoneApp == TEXT("crew"))
	{
		Add(TEXT("buddy"), TEXT("Buddy boat"), Ext && Ext->IsBuddyOn() ? TEXT("Out with you") : TEXT("At home"));
		Add(TEXT("shower"), TEXT("Deck shower"), TEXT("on the boats that have one"));
		Add(TEXT("pistol"), TEXT("Boat pistol"), TEXT("draw it from the console"));
	}
	else if (PhoneApp == TEXT("events"))
	{
		Add(TEXT("mode"), TEXT("Mode"), bFreeFish ? TEXT("Free Fish - no clock, no pay") : TEXT("Charter Captain - clients, clocks, pay"));
		Add(TEXT("tourney"), TEXT("Fishing tournament"), TEXT("15 min, five species"));
		Add(TEXT("race"), TEXT("Marathon race"), TEXT("2 laps"));
	}
	else if (PhoneApp == TEXT("friends"))
	{
		Add(TEXT("board"), TEXT("Leaderboard"));
		Add(TEXT("online"), bNet ? TEXT("Log out") : TEXT("Play online"), bNet ? Net->StatusText() : FString(TEXT("fish with friends")));
	}
	else if (PhoneApp == TEXT("settings"))
	{
		Add(TEXT("settings"), TEXT("Settings"), TEXT("graphics, display, sound"));
		Add(TEXT("controls"), TEXT("Controls"), TEXT("the keys that are left"));
		Add(TEXT("pause"), TEXT("Pause menu"));
	}
}

void AKeysBoatPawn::PhoneActivate(FName Id, int32 Dir)
{
	const FString IdS = Id.ToString();
	// an app on the home screen: three of them are the thing itself (the chart, the gear screen, the land office)
	if (IdS.StartsWith(TEXT("app:")))
	{
		const FName App(*IdS.Mid(4));
		if (App == TEXT("chart")) { bBigMap = !bBigMap; ClosePhone(); return; }
		if (App == TEXT("gear")) { ClosePhone(); bGearOpen = true; return; }
		if (App == TEXT("land")) { ClosePhone(); if (AKeysEstates* Es = AKeysEstates::Get(this)) Es->Open(true); return; }
		PhoneHomeSel = PhoneSel;
		PhoneApp = App;
		PhoneSel = PhoneTop = 0;
		return;
	}
	if (IdS.StartsWith(TEXT("go:"))) { ClosePhone(); RunTo(FCString::Atoi(*IdS.Mid(3))); return; }   // RunTo says why not, when not

	// the rows that are the helm's own switches
	const bool bHelmRow = Id == TEXT("miami") || Id == TEXT("motors") || Id == TEXT("shift") || Id == TEXT("trim") || Id == TEXT("troll")
		|| Id == TEXT("buddy") || Id == TEXT("shower") || Id == TEXT("pistol") || Id == TEXT("scuba");
	if (bHelmRow && IsAshore()) { ShowToast(TEXT("That one is at the helm - step back aboard first.")); return; }

	if (Id == TEXT("miami"))
	{
		// travel to Miami and back (the old X key)
		double NX, NZ, NH;
		FString Msg;
		const bool bGo = AKeysExtras::MiamiDestination(X, Z, Spec().Draft, NX, NZ, NH, Msg);
		ClosePhone();
		if (bGo)
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
	else if (Id == TEXT("motors")) { if (AKeysAudio* Aud = AKeysAudio::Get(this)) Aud->ToggleEngines(); }
	else if (Id == TEXT("shift"))
	{
		Gear = (Gear + (Dir < 0 ? 2 : 1)) % 3;
		if (AKeysAudio* Aud = AKeysAudio::Get(this)) Aud->GearShift();
	}
	else if (Id == TEXT("trim"))
	{
		bTrimMode = !bTrimMode;
		if (bTrimMode) { ClosePhone(); ShowToast(TEXT("Trim mode on - Up/Down trim the motors (slow down to tilt them up), Left/Right lift that side with the tabs. A/D steer, W/S throttle. T to exit."), 6); }
	}
	else if (Id == TEXT("troll")) { ClosePhone(); ToggleTroll(); }
	else if (Id == TEXT("plotter")) bElectronics = !bElectronics;
	else if (Id == TEXT("lights")) ToggleLights();
	else if (Id == TEXT("scuba")) ShowToast(KeysScuba::ToggleMode(this), 4);
	else if (Id == TEXT("zoom")) CamZoom = CamZoom > 1.4 ? 1 : 1.9;
	else if (Id == TEXT("buddy")) { if (AKeysExtras* Ext = AKeysExtras::Get(this)) Ext->ToggleBuddy(); }
	else if (Id == TEXT("shower")) { if (AKeysExtras* Ext = AKeysExtras::Get(this)) Ext->ToggleShower(); }
	else if (Id == TEXT("pistol")) { ClosePhone(); if (AKeysHandgun* Gun = AKeysHandgun::Get(this)) Gun->Toggle(); }
	else if (Id == TEXT("pause")) { ClosePhone(); Menu = EMenu::Pause; MenuSel = 0; }
	else if (Id == TEXT("mode"))
	{
		// the pause menu's row, and its mid-run rules (a charter under way is called off; the boat stays the one you are on)
		Menu = EMenu::Pause;
		MenuActivate(Id, Dir);
		Menu = EMenu::None;
	}
	else if (Id == TEXT("board") || Id == TEXT("online") || Id == TEXT("settings") || Id == TEXT("controls") || Id == TEXT("tourney") || Id == TEXT("race"))
	{
		// these open something of their own (or start a clock): the phone goes away first
		ClosePhone();
		MenuActivate(Id, Dir);
	}
	else MenuActivate(Id, Dir);   // sea, time, sound, radio, view, fpview, autochase, diveview, fuel: the settings menu's own rows
}

// a click on a row (Dir 0), or on one of its arrows
void AKeysBoatPawn::PhoneClick(int32 I, int32 Dir)
{
	TArray<FMenuItem> Items;
	GetPhoneItems(Items);
	if (!Items.IsValidIndex(I)) return;
	PhoneSel = I;
	PhoneActivate(Items[I].Id, Dir);
}

// True when the phone took this frame's keys (it is open, or P just opened it).
bool AKeysBoatPawn::HandlePhoneInput(APlayerController* PC)
{
	auto PhHit = [PC](const FKey& K) { return PC->WasInputKeyJustPressed(K); };
	if (!bPhoneOpen)
	{
		if (!PhHit(EKeys::P)) return false;
		if (!CanUsePhone())
		{
			if (bRunning && Menu == EMenu::None && IsDiving()) ShowToast(TEXT("The phone stays dry aboard."));
			return false;
		}
		OpenPhone();
	}
	else if (!CanUsePhone()) { ClosePhone(); return false; }   // a bite, a panel: hands back on the rod
	else if (PhHit(EKeys::P)) { ClosePhone(); return true; }

	// hands off the wheel and the rod while you thumb through it: the boat coasts
	bReelKey = bFwd = bBack = bLeft = bRight = bTrimUp = bTrimDn = bAction = false;

	TArray<FMenuItem> Items;
	GetPhoneItems(Items);
	const int32 Num = Items.Num();
	PhoneSel = FMath::Clamp(PhoneSel, 0, FMath::Max(0, Num - 1));
	const bool bUp = PhHit(EKeys::Up) || PhHit(EKeys::W), bDn = PhHit(EKeys::Down) || PhHit(EKeys::S);
	const bool bLf = PhHit(EKeys::Left) || PhHit(EKeys::A), bRt = PhHit(EKeys::Right) || PhHit(EKeys::D);
	const bool bGo = PhHit(EKeys::Enter) || PhHit(EKeys::SpaceBar) || PhHit(EKeys::E);
	if (PhHit(EKeys::Escape) || PhHit(EKeys::BackSpace)) { PhoneBack(); return true; }
	if (!Num) return true;
	if (PhoneApp.IsNone())
	{
		// the grid
		if (bLf) PhoneSel = (PhoneSel + Num - 1) % Num;
		if (bRt) PhoneSel = (PhoneSel + 1) % Num;
		if (bUp && PhoneSel - PhoneCols >= 0) PhoneSel -= PhoneCols;
		if (bDn && PhoneSel + PhoneCols < Num) PhoneSel += PhoneCols;
		if (bGo) PhoneActivate(Items[PhoneSel].Id);
		return true;
	}
	// a list: the wheel scrolls it, the selection stays on the screen
	const int32 MaxTop = FMath::Max(0, Num - PhoneVisRows);
	if (bUp) PhoneSel = (PhoneSel + Num - 1) % Num;
	if (bDn || PhHit(EKeys::Tab)) PhoneSel = (PhoneSel + 1) % Num;
	if (bUp || bDn || PhHit(EKeys::Tab)) PhoneTop = FMath::Clamp(PhoneTop, PhoneSel - PhoneVisRows + 1, PhoneSel);
	if (PhHit(EKeys::MouseScrollUp)) PhoneTop -= 2;
	if (PhHit(EKeys::MouseScrollDown)) PhoneTop += 2;
	PhoneTop = FMath::Clamp(PhoneTop, 0, MaxTop);
	PhoneSel = FMath::Clamp(PhoneSel, PhoneTop, FMath::Min(Num - 1, PhoneTop + PhoneVisRows - 1));
	if (Items[PhoneSel].bArrows && bLf) PhoneActivate(Items[PhoneSel].Id, -1);
	else if (Items[PhoneSel].bArrows && bRt) PhoneActivate(Items[PhoneSel].Id, 1);
	else if (bGo) PhoneActivate(Items[PhoneSel].Id);
	return true;
}

// ================================================================ the HUD's side: the handset

namespace
{
	FLinearColor KrPhHex(uint32 C, float A = 1.f) { return FLinearColor(((C >> 16) & 255) / 255.f, ((C >> 8) & 255) / 255.f, (C & 255) / 255.f, A); }
	FLinearColor KrPhMix(const FLinearColor& A, const FLinearColor& B, float K) { return FLinearColor(A.R + (B.R - A.R) * K, A.G + (B.G - A.G) * K, A.B + (B.B - A.B) * K, 1.f); }
	FLinearColor KrPhShade(const FLinearColor& C, float K) { return FLinearColor(FMath::Min(1.f, C.R * K), FMath::Min(1.f, C.G * K), FMath::Min(1.f, C.B * K), C.A); }
	FLinearColor KrPhAlpha(const FLinearColor& C, float A) { return FLinearColor(C.R, C.G, C.B, A); }
	// the Scrap King palette (Theme/UiPalette.cs), so the two games' phones are one phone
	const FLinearColor KrPhKraft = KrPhHex(0xd9c7a3), KrPhInk = KrPhHex(0x2b241c), KrPhRust = KrPhHex(0xb5522a), KrPhIron = KrPhHex(0x3a3d42);
	const FLinearColor KrPhTeal = KrPhHex(0x3f8a8a), KrPhAmber = KrPhHex(0xe3a63b), KrPhChalk = KrPhHex(0xf2ebdc);
	constexpr int32 KrPhWallW = 128, KrPhWallH = 192;

	// an app's tile colour
	FLinearColor KrPhTile(FName App)
	{
		if (App == TEXT("travel")) return KrPhAmber;
		if (App == TEXT("chart")) return KrPhKraft;
		if (App == TEXT("radio")) return KrPhRust;
		if (App == TEXT("weather")) return KrPhTeal;
		if (App == TEXT("boat")) return KrPhMix(KrPhTeal, KrPhInk, .55f);
		if (App == TEXT("camera")) return KrPhShade(KrPhIron, 1.35f);
		if (App == TEXT("crew")) return KrPhMix(KrPhRust, KrPhInk, .45f);
		if (App == TEXT("events")) return KrPhAmber;
		if (App == TEXT("friends")) return KrPhTeal;
		if (App == TEXT("gear")) return KrPhChalk;
		if (App == TEXT("land")) return KrPhMix(KrPhKraft, KrPhTeal, .35f);
		return KrPhIron;
	}
}

// A box with round corners, out of strips (the canvas has no rounded rectangle).
void AKeysHUD::PhoneRound(float X, float Y, float W, float H, float R, const FLinearColor& C)
{
	R = FMath::Min(R, FMath::Min(W, H) * .5f);
	constexpr int32 Steps = 6;
	const float Strip = R / Steps;
	for (int32 i = 0; i < Steps; i++)
	{
		const float Dy = R - (i + .5f) * Strip;
		const float Inset = R - FMath::Sqrt(FMath::Max(0.f, R * R - Dy * Dy));
		DrawRect(C, X + Inset, Y + i * Strip, W - Inset * 2, Strip + .5f);
		DrawRect(C, X + Inset, Y + H - (i + 1) * Strip, W - Inset * 2, Strip + .5f);
	}
	DrawRect(C, X, Y + R, W, H - R * 2);
}

// An app's icon: a chunky tile with a lip under it and a light top, and a flat glyph (Scrap King's DeviceIcons, redrawn
// with the canvas's own shapes). S is the tile's side in pixels.
void AKeysHUD::PhoneIcon(FName App, float X, float Y, float S)
{
	const FLinearColor Tile = KrPhTile(App);
	const bool bLight = Tile.R + Tile.G + Tile.B > 1.9f;   // a pale tile takes an ink glyph
	const FLinearColor Ink = bLight ? KrPhInk : KrPhChalk;
	const float Rad = S * .2f;
	PhoneRound(X, Y + S * .06f, S, S, Rad, KrPhShade(Tile, .55f));      // the lip
	PhoneRound(X, Y, S, S, Rad, Tile);
	DrawRect(KrPhAlpha(FLinearColor::White, .22f), X + Rad, Y + S * .03f, S - Rad * 2, S * .035f);   // the light along the top

	auto Box = [&](float BX, float BY, float BW, float BH, const FLinearColor& C) { DrawRect(C, X + BX * S, Y + BY * S, BW * S, BH * S); };
	auto Line = [&](float X1, float Y1, float X2, float Y2, float Th, const FLinearColor& C) { DrawLine(X + X1 * S, Y + Y1 * S, X + X2 * S, Y + Y2 * S, C, Th * S); };
	auto Tri = [&](float X1, float Y1, float X2, float Y2, float X3, float Y3, const FLinearColor& C)
	{
		FCanvasTriangleItem It(FVector2D(X + X1 * S, Y + Y1 * S), FVector2D(X + X2 * S, Y + Y2 * S), FVector2D(X + X3 * S, Y + Y3 * S), GWhiteTexture);
		It.SetColor(C);
		Canvas->DrawItem(It);
	};
	auto Disc = [&](float CX, float CY, float Rr, const FLinearColor& C)
	{
		constexpr int32 Seg = 14;
		for (int32 k = 0; k < Seg; k++)
		{
			const float A0 = 2 * PI * k / Seg, A1 = 2 * PI * (k + 1) / Seg;
			Tri(CX, CY, CX + FMath::Cos(A0) * Rr, CY + FMath::Sin(A0) * Rr, CX + FMath::Cos(A1) * Rr, CY + FMath::Sin(A1) * Rr, C);
		}
	};

	if (App == TEXT("travel"))
	{
		// a map pin
		Disc(.5f, .4f, .2f, Ink);
		Tri(.32f, .48f, .68f, .48f, .5f, .82f, Ink);
		Disc(.5f, .4f, .08f, Tile);
	}
	else if (App == TEXT("chart"))
	{
		// a folded chart with a course across it
		Box(.2f, .3f, .2f, .46f, Ink);
		Box(.4f, .24f, .2f, .46f, KrPhMix(Ink, Tile, .3f));
		Box(.6f, .3f, .2f, .46f, Ink);
		Line(.27f, .62f, .47f, .42f, .04f, KrPhRust);
		Line(.47f, .42f, .72f, .5f, .04f, KrPhRust);
	}
	else if (App == TEXT("radio"))
	{
		Line(.3f, .42f, .64f, .18f, .04f, Ink);
		Box(.2f, .4f, .6f, .38f, Ink);
		Disc(.38f, .59f, .1f, Tile);
		Box(.56f, .5f, .16f, .06f, Tile);
		Box(.56f, .62f, .16f, .06f, Tile);
	}
	else if (App == TEXT("weather"))
	{
		// the sun behind a cloud
		Disc(.38f, .38f, .17f, KrPhAmber);
		Disc(.46f, .62f, .13f, Ink);
		Disc(.62f, .58f, .16f, Ink);
		Box(.4f, .6f, .34f, .15f, Ink);
		Disc(.76f, .66f, .09f, Ink);
	}
	else if (App == TEXT("boat"))
	{
		// a center console on her lines
		Tri(.12f, .54f, .88f, .54f, .74f, .74f, Ink);
		Tri(.12f, .54f, .74f, .74f, .24f, .74f, Ink);
		Box(.42f, .36f, .2f, .18f, Ink);
		Box(.4f, .28f, .26f, .05f, Ink);
		Line(.14f, .85f, .86f, .85f, .035f, KrPhAlpha(Ink, .7f));
	}
	else if (App == TEXT("camera"))
	{
		Box(.18f, .36f, .64f, .4f, Ink);
		Box(.38f, .27f, .24f, .1f, Ink);
		Disc(.5f, .56f, .14f, Tile);
		Disc(.5f, .56f, .07f, Ink);
	}
	else if (App == TEXT("crew"))
	{
		Disc(.66f, .42f, .09f, KrPhAlpha(Ink, .75f));
		Box(.55f, .54f, .22f, .22f, KrPhAlpha(Ink, .75f));
		Disc(.38f, .38f, .12f, Ink);
		Box(.23f, .54f, .3f, .24f, Ink);
	}
	else if (App == TEXT("events"))
	{
		// the cup
		Box(.34f, .22f, .32f, .26f, Ink);
		Tri(.34f, .48f, .66f, .48f, .5f, .62f, Ink);
		Box(.47f, .58f, .06f, .14f, Ink);
		Box(.36f, .72f, .28f, .07f, Ink);
		Line(.34f, .28f, .24f, .4f, .035f, Ink);
		Line(.66f, .28f, .76f, .4f, .035f, Ink);
	}
	else if (App == TEXT("friends"))
	{
		// the podium
		Box(.2f, .5f, .19f, .28f, Ink);
		Box(.405f, .32f, .19f, .46f, Ink);
		Box(.61f, .58f, .19f, .2f, Ink);
	}
	else if (App == TEXT("gear"))
	{
		// a hook on its line
		Disc(.56f, .22f, .055f, Ink);
		Line(.56f, .22f, .56f, .62f, .06f, Ink);
		Line(.56f, .62f, .47f, .76f, .06f, Ink);
		Line(.47f, .76f, .35f, .68f, .06f, Ink);
		Line(.35f, .68f, .35f, .54f, .06f, Ink);
		Tri(.3f, .56f, .4f, .56f, .35f, .44f, Ink);
	}
	else if (App == TEXT("land"))
	{
		Tri(.18f, .5f, .82f, .5f, .5f, .22f, Ink);
		Box(.28f, .5f, .44f, .28f, Ink);
		Box(.45f, .6f, .1f, .18f, Tile);
	}
	else
	{
		// a cog
		Line(.22f, .5f, .78f, .5f, .11f, Ink);
		Line(.5f, .22f, .5f, .78f, .11f, Ink);
		Line(.3f, .3f, .7f, .7f, .11f, Ink);
		Line(.3f, .7f, .7f, .3f, .11f, Ink);
		Disc(.5f, .5f, .2f, Ink);
		Disc(.5f, .5f, .09f, Tile);
	}
}

// The home screen's wallpaper, as pixels: the Seven Mile Bridge at dusk from the water (the Keys' answer to Scrap King's
// Port Calder at dusk, DeviceArt.PhoneWallpaper there). An ink-to-amber sky, the low sun, the bridge's piers and the
// hump of its channel span against the light, a mangrove key with a palm, the sun's streak on the water; darker toward
// the bottom so the app names read over it.
void AKeysHUD::BuildPhoneWallpaper()
{
	PhoneWall = UTexture2D::CreateTransient(KrPhWallW, KrPhWallH, PF_B8G8R8A8);
	if (!PhoneWall) return;
	PhoneWall->SRGB = true;
	PhoneWall->Filter = TF_Nearest;   // pixel art: crisp
	TArray<FColor> Px;
	Px.SetNumUninitialized(KrPhWallW * KrPhWallH);
	const float WW = KrPhWallW, WH = KrPhWallH;
	const float Horizon = WH * .7f;
	const FLinearColor SkyTop = KrPhShade(KrPhInk, .85f), SkyMid = KrPhMix(KrPhRust, KrPhInk, .35f), SkyLow = KrPhMix(KrPhAmber, KrPhRust, .35f);
	const FLinearColor Dark = KrPhShade(KrPhInk, .6f);
	const FLinearColor Water = KrPhMix(KrPhTeal, KrPhInk, .6f), WaterDeep = KrPhMix(KrPhTeal, KrPhInk, .88f);
	const float SunX = WW * .64f, SunY = Horizon - WH * .07f, SunR = WW * .075f;
	for (int32 j = 0; j < KrPhWallH; j++)
		for (int32 i = 0; i < KrPhWallW; i++)
		{
			FLinearColor C;
			if (j < Horizon)
			{
				const float K = j / Horizon;
				C = K < .55f ? KrPhMix(SkyTop, SkyMid, K / .55f) : KrPhMix(SkyMid, SkyLow, (K - .55f) / .45f);
				const float Dist = FMath::Sqrt(FMath::Square(i - SunX) + FMath::Square(j - SunY));
				if (Dist < SunR) C = KrPhMix(KrPhAmber, KrPhChalk, .25f);
				else if (Dist < SunR * 2.6f) C = KrPhMix(C, KrPhAmber, .35f * (1 - (Dist - SunR) / (SunR * 1.6f)));
				// the bridge: a deck that humps up over the channel, on piers every few pixels
				const float Hump = 7.f * FMath::Exp(-FMath::Square((i - WW * .3f) / (WW * .13f)));
				const float Deck = Horizon - 7.f - Hump;
				if (j >= Deck && j < Deck + 2) C = Dark;
				else if (j >= Deck + 2 && i % 7 < 2) C = Dark;
				// a mangrove key on the right, and its one palm
				const float Mound = Horizon - 5.f * FMath::Exp(-FMath::Square((i - WW * .88f) / (WW * .1f)));
				if (j > Mound) C = Dark;
				const int32 PalmX = FMath::RoundToInt32(WW * .86f);
				if (i == PalmX && j > Horizon - 20 && j <= Horizon - 4) C = Dark;
				if (FMath::Abs(j - (Horizon - 20)) <= 1 && FMath::Abs(i - PalmX) <= 5 && FMath::Abs(i - PalmX) + FMath::Abs(j - (Horizon - 20)) * 3 <= 6) C = Dark;
				if (FMath::RoundToInt32(j - (Horizon - 20)) == FMath::Abs(i - PalmX) / 2 + 1 && FMath::Abs(i - PalmX) <= 6) C = Dark;
			}
			else
			{
				const float K = (j - Horizon) / (WH - Horizon);
				C = KrPhMix(Water, WaterDeep, FMath::Clamp(K * 1.3f, 0.f, 1.f));
				// the sun's streak, broken into ripples
				const float Spread = SunR * (1.2f + K * 2.2f);
				if (FMath::Abs(i - SunX) < Spread && (j + i / 2) % 5 < 2 && K < .6f) C = KrPhMix(C, KrPhAmber, .55f * (1 - K / .6f));
			}
			Px[j * KrPhWallW + i] = FColor(uint8(C.R * 255), uint8(C.G * 255), uint8(C.B * 255), 255);
		}
	FTexture2DMipMap& Mip = PhoneWall->GetPlatformData()->Mips[0];
	void* Data = Mip.BulkData.Lock(LOCK_READ_WRITE);
	FMemory::Memcpy(Data, Px.GetData(), Px.Num() * sizeof(FColor));
	Mip.BulkData.Unlock();
	PhoneWall->UpdateResource();
}

void AKeysHUD::DrawPhone(AKeysBoatPawn* P)
{
	if (!PhoneWall) BuildPhoneWallpaper();
	UFont* Font = GEngine->GetMediumFont();
	const bool bHome = P->PhoneApp.IsNone();
	TArray<AKeysBoatPawn::FMenuItem> Items;
	P->GetPhoneItems(Items);

	// the handset slides up from under the screen's edge, in the bottom right corner where a hand would hold it
	const float W = 460 * U, H = 720 * U;
	const float Age = float(GetWorld()->GetRealTimeSeconds() - P->PhoneOpenT);
	const float In = 1 - FMath::Pow(1 - FMath::Clamp(Age / .2f, 0.f, 1.f), 3.f);
	const float X = Canvas->SizeX - W - 70 * U, Y = Canvas->SizeY - (H + 34 * U) * In;

	// text that has to fit a width: cut short with two dots
	auto Fit = [&](FString S, float MaxW, float Scale)
	{
		float TW = 0, TH = 0;
		GetTextSize(S, TW, TH, Font, Scale * U);
		if (TW <= MaxW) return S;
		while (S.Len() > 1)
		{
			S.LeftChopInline(1);
			GetTextSize(S + TEXT(".."), TW, TH, Font, Scale * U);
			if (TW <= MaxW) break;
		}
		return S + TEXT("..");
	};
	// plain text (the HUD's Text() drops a shadow under it, which muddies small type on the phone's dark screen)
	auto Plain = [&](const FString& S, float TX, float TY, float Scale, const FLinearColor& C, bool bCenter = false)
	{
		if (bCenter)
		{
			float TW = 0, TH = 0;
			GetTextSize(S, TW, TH, Font, Scale * U);
			TX -= TW * .5f;
		}
		DrawText(S, C, TX, TY, Font, Scale * U);
	};

	// ---- the body: side buttons, the rubber bumper, the shell, a screw in each corner
	const FLinearColor Rubber = KrPhShade(KrPhIron, .42f), Shell = KrPhShade(KrPhIron, .85f);
	DrawRect(KrPhShade(KrPhIron, .5f), X - 4 * U, Y + 112 * U, 6 * U, 62 * U);       // the volume rocker
	DrawRect(KrPhShade(KrPhIron, .5f), X + W - 2 * U, Y + 98 * U, 6 * U, 42 * U);    // the power key
	PhoneRound(X, Y, W, H, 38 * U, Rubber);
	PhoneRound(X + 4 * U, Y + 4 * U, W - 8 * U, H - 8 * U, 34 * U, KrPhShade(Shell, 1.25f));
	PhoneRound(X + 5 * U, Y + 6 * U, W - 10 * U, H - 11 * U, 33 * U, Shell);
	for (int32 k = 0; k < 4; k++)
	{
		const float CX = k % 2 ? X + W - 20 * U : X + 20 * U, CY = k / 2 ? Y + H - 20 * U : Y + 20 * U;
		PhoneRound(CX - 4 * U, CY - 4 * U, 8 * U, 8 * U, 4 * U, KrPhShade(KrPhIron, 1.55f));
		DrawLine(CX - 2.5f * U, CY + 2.5f * U, CX + 2.5f * U, CY - 2.5f * U, KrPhShade(KrPhIron, .5f), 1.5f * U);
	}

	// ---- the screen: glass, then the wallpaper (home) or the app's dark page
	const float SX = X + 18 * U, SY = Y + 38 * U, SW = W - 36 * U, SH = H - 38 * U - 48 * U;
	DrawRect(KrPhShade(KrPhInk, .45f), SX - 3 * U, SY - 3 * U, SW + 6 * U, SH + 6 * U);
	if (bHome && PhoneWall) DrawTexture(PhoneWall, SX, SY, SW, SH, 0, 0, 1, 1, FLinearColor::White, BLEND_Opaque);
	else DrawRect(KrPhShade(KrPhInk, .72f), SX, SY, SW, SH);
	// the earpiece and the camera in the top bezel, the home indicator in the bottom one
	PhoneRound(X + W * .5f - 30 * U, Y + 16 * U, 60 * U, 7 * U, 3.5f * U, KrPhShade(KrPhInk, .5f));
	PhoneRound(X + W * .5f + 46 * U, Y + 14 * U, 10 * U, 10 * U, 5 * U, KrPhShade(KrPhInk, .5f));
	PhoneRound(X + W * .5f - 55 * U, SY + SH + 21 * U, 110 * U, 5 * U, 2.5f * U, KrPhAlpha(KrPhChalk, .6f));

	// ---- the status bar: the clock on the left; the carrier, signal and battery on the right
	const float BarH = 28 * U;
	DrawRect(KrPhAlpha(KrPhShade(KrPhInk, .7f), bHome ? .45f : .92f), SX, SY, SW, BarH);
	{
		const FDateTime Clock = FDateTime::Now();
		Plain(FString::Printf(TEXT("%d:%02d"), Clock.GetHour(), Clock.GetMinute()), SX + 12 * U, SY + 4 * U, .7f, KrPhChalk);
		// the battery runs down over a long session (display only): full at the start, a fifth after two hours
		const float Batt = 1 - FMath::Clamp(float(GetWorld()->GetRealTimeSeconds()) / 7200.f, 0.f, .8f);
		const float CellW = 26 * U, CellH = 12 * U, CellX = SX + SW - 12 * U - CellW, CellY = SY + BarH * .5f - CellH * .5f;
		DrawRect(KrPhChalk, CellX, CellY, CellW, CellH);
		DrawRect(KrPhShade(KrPhInk, .8f), CellX + 1.5f * U, CellY + 1.5f * U, CellW - 3 * U, CellH - 3 * U);
		DrawRect(KrPhChalk, CellX + CellW, CellY + 3.5f * U, 2 * U, 5 * U);
		DrawRect(Batt <= .2f ? KrPhRust : Batt <= .4f ? KrPhAmber : KrPhChalk, CellX + 3 * U, CellY + 3 * U, (CellW - 6 * U) * Batt, CellH - 6 * U);
		// signal: four bars at a marina, none far offshore (display only: it still works out there)
		double Near = 1e12;
		for (const KeysGeo::FMarina& M : KeysGeo::Marinas()) Near = FMath::Min(Near, FMath::Square(M.SX - P->BoatX()) + FMath::Square(M.SZ - P->BoatZ()));
		Near = FMath::Sqrt(Near);
		const int32 Bars = Near < 1500 ? 4 : Near < 4000 ? 3 : Near < 9000 ? 2 : Near < 20000 ? 1 : 0;
		float BX = CellX - 10 * U;
		for (int32 k = 3; k >= 0; k--)
		{
			const float BH = (4 + k * 2.5f) * U;
			DrawRect(k < Bars ? KrPhChalk : KrPhAlpha(KrPhChalk, .25f), BX - 3 * U, SY + BarH * .5f + 6 * U - BH, 3 * U, BH);
			BX -= 5 * U;
		}
		float TW = 0, TH = 0;
		GetTextSize(TEXT("KEYS"), TW, TH, Font, .62f * U);
		Plain(TEXT("KEYS"), BX - 6 * U - TW, SY + 5 * U, .62f, KrPhChalk);
	}

	if (bHome)
	{
		// ---- the grid of apps: the drawn icon, the name, its line
		const int32 Cols = AKeysBoatPawn::PhoneCols;
		const float CellW = SW / Cols, CellH = 138 * U, Icon = 70 * U, GY = SY + BarH + 14 * U;
		for (int32 i = 0; i < Items.Num(); i++)
		{
			const float CX = SX + (i % Cols) * CellW, CY = GY + (i / Cols) * CellH;
			const bool bHot = i == P->PhoneSel;
			if (bHot) PhoneRound(CX + 5 * U, CY, CellW - 10 * U, CellH - 6 * U, 14 * U, KrPhAlpha(KrPhChalk, .2f));
			AddHitBox(FVector2D(CX, CY), FVector2D(CellW, CellH), FName(*FString::Printf(TEXT("ph:%d"), i)), true, 5);
			PhoneIcon(FName(*Items[i].Id.ToString().Mid(4)), CX + (CellW - Icon) * .5f, CY + 8 * U, Icon);
			const float NameY = CY + 8 * U + Icon + 9 * U;
			Plain(Items[i].Label, CX + CellW * .5f + U, NameY + U, .78f, KrPhAlpha(KrPhInk, .85f), true);
			Plain(Items[i].Label, CX + CellW * .5f, NameY, .78f, bHot ? KrPhAmber : KrPhChalk, true);
			if (!Items[i].Value.IsEmpty())
			{
				const FString Line = Fit(Items[i].Value, CellW - 12 * U, .6f);
				Plain(Line, CX + CellW * .5f + U, NameY + 22 * U + U, .6f, KrPhAlpha(KrPhInk, .85f), true);
				Plain(Line, CX + CellW * .5f, NameY + 22 * U, .6f, KrPhMix(KrPhChalk, KrPhKraft, .35f), true);
			}
		}
		Plain(TouchMode() ? TEXT("Tap an app") : TEXT("Arrows and Enter, or the mouse  -  P puts it away"), SX + SW * .5f, SY + SH - 26 * U, .6f, KrPhAlpha(KrPhChalk, .8f), true);
		return;
	}

	// ---- an app: the title bar (back chevron, icon, name, a rule in the icon's colour), then its rows
	const float TitleY = SY + BarH, TitleH = 56 * U;
	const FLinearColor Tile = KrPhTile(P->PhoneApp);
	DrawRect(KrPhAlpha(KrPhChalk, .08f), SX + 8 * U, TitleY + 9 * U, 38 * U, 38 * U);
	AddHitBox(FVector2D(SX, TitleY), FVector2D(60 * U, TitleH), FName(TEXT("phb:0")), true, 6);
	DrawLine(SX + 32 * U, TitleY + 18 * U, SX + 22 * U, TitleY + 28 * U, KrPhChalk, 3 * U);
	DrawLine(SX + 22 * U, TitleY + 28 * U, SX + 32 * U, TitleY + 38 * U, KrPhChalk, 3 * U);
	PhoneIcon(P->PhoneApp, SX + 58 * U, TitleY + 10 * U, 34 * U);
	{
		FString Name = P->PhoneApp.ToString();
		if (Name.Len()) Name = Name.Left(1).ToUpper() + Name.Mid(1);
		Plain(Name, SX + 104 * U, TitleY + 13 * U, 1.05f, KrPhChalk);
	}
	DrawRect(Tile, SX, TitleY + TitleH - 2 * U, SW, 2 * U);

	const int32 Vis = AKeysBoatPawn::PhoneVisRows;
	const float RowH = 58 * U, RY0 = TitleY + TitleH + 6 * U;
	const int32 Top = FMath::Clamp(P->PhoneTop, 0, FMath::Max(0, Items.Num() - Vis));
	for (int32 i = Top; i < Items.Num() && i < Top + Vis; i++)
	{
		const AKeysBoatPawn::FMenuItem& It = Items[i];
		const float RY = RY0 + (i - Top) * RowH;
		const bool bHot = i == P->PhoneSel;
		DrawRect(KrPhAlpha(KrPhChalk, bHot ? .16f : .05f), SX + 8 * U, RY, SW - 16 * U, RowH - 5 * U);
		if (bHot) DrawRect(KrPhAmber, SX + 8 * U, RY, 4 * U, RowH - 5 * U);
		AddHitBox(FVector2D(SX + 8 * U, RY), FVector2D(SW - 16 * U, RowH - 5 * U), FName(*FString::Printf(TEXT("ph:%d"), i)), true, 5);
		const float Room = SW - 16 * U - 22 * U - (It.bArrows ? 96 * U : 0.f);
		const bool bOne = It.Value.IsEmpty();
		Plain(Fit(It.Label, Room, .88f), SX + 24 * U, RY + (bOne ? 15 : 6) * U, .88f, bHot ? KrPhAmber : KrPhChalk);
		if (!bOne) Plain(Fit(It.Value, Room, .66f), SX + 24 * U, RY + 31 * U, .66f, KrPhMix(KrPhChalk, KrPhIron, .35f));
		if (It.bArrows)
		{
			// < >: their own buttons, on top of the row
			const float AW = 40 * U, AH = RowH - 17 * U, AY = RY + 6 * U, AX1 = SX + SW - 16 * U - AW, AX0 = AX1 - AW - 6 * U;
			DrawRect(KrPhAlpha(KrPhChalk, .12f), AX0, AY, AW, AH);
			DrawRect(KrPhAlpha(KrPhChalk, .12f), AX1, AY, AW, AH);
			AddHitBox(FVector2D(AX0, AY), FVector2D(AW, AH), FName(*FString::Printf(TEXT("ph<:%d"), i)), true, 7);
			AddHitBox(FVector2D(AX1, AY), FVector2D(AW, AH), FName(*FString::Printf(TEXT("ph>:%d"), i)), true, 7);
			Plain(TEXT("<"), AX0 + AW * .5f, AY + 8 * U, 1.f, bHot ? KrPhAmber : KrPhChalk, true);
			Plain(TEXT(">"), AX1 + AW * .5f, AY + 8 * U, 1.f, bHot ? KrPhAmber : KrPhChalk, true);
		}
	}
	// a long list: where you are in it, down the right edge
	if (Items.Num() > Vis)
	{
		const float TrackH = Vis * RowH - 5 * U, Knob = TrackH * Vis / Items.Num();
		DrawRect(KrPhAlpha(KrPhChalk, .1f), SX + SW - 5 * U, RY0, 3 * U, TrackH);
		DrawRect(KrPhAlpha(KrPhChalk, .6f), SX + SW - 5 * U, RY0 + (TrackH - Knob) * Top / FMath::Max(1, Items.Num() - Vis), 3 * U, Knob);
	}
	Plain(TouchMode() ? TEXT("Tap a row  -  the arrow goes back") : TEXT("Enter chooses  -  Left / Right change  -  Esc back"), SX + SW * .5f, SY + SH - 24 * U, .58f, KrPhAlpha(KrPhChalk, .7f), true);
}
