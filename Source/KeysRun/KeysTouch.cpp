// Touch controls for phones and tablets. Everything in the game reads the keyboard through the player controller
// (IsInputKeyDown / WasInputKeyJustPressed), so the on-screen controls simply press and release those same keys:
// a thumb stick on the left, context buttons on the right (what they are depends on what you are doing: driving,
// fishing, the catch card, ashore, diving, a panel), and a drag anywhere else looks round (the mouse's X / Y).
// Taps on the HUD's own buttons (the menus, the leaderboard) go to them through the engine's hit boxes.
#include "KeysHUD.h"
#include "KeysBoatPawn.h"
#include "KeysDiver.h"
#include "KeysWalker.h"
#include "KeysSlots.h"
#include "KeysEstates.h"
#include "KeysHandgun.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Engine/Font.h"
#include "Engine/GameViewportClient.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerInput.h"
#include "GenericPlatform/GenericPlatformInputDeviceMapper.h"
#include "InputKeyEventArgs.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Algo/AnyOf.h"

FString AKeysHUD::ScriptHold;
FVector2D AKeysHUD::ScriptLookDelta = FVector2D::ZeroVector;

bool AKeysHUD::TouchMode()
{
#if PLATFORM_IOS || PLATFORM_ANDROID
	return true;
#else
	static const bool bOn = FParse::Param(FCommandLine::Get(), TEXT("KRTouch"));
	return bOn;
#endif
}

void AKeysHUD::DrawHUD()
{
	DrawGame();
	if (!Canvas || !TouchMode()) return;
	if (AKeysBoatPawn* P = Cast<AKeysBoatPawn>(GetOwningPawn())) DrawTouch(P);
}

void AKeysHUD::SendKey(const FKey& K, bool bDown)
{
	APlayerController* PC = GetOwningPlayerController();
	if (!PC) return;
	FViewport* VP = GEngine && GEngine->GameViewport ? GEngine->GameViewport->Viewport : nullptr;
	const FInputDeviceId Dev = IPlatformInputDeviceMapper::Get().GetDefaultInputDevice();
	PC->InputKey(FInputKeyEventArgs(VP, Dev, K, bDown ? IE_Pressed : IE_Released, bDown ? 1.f : 0.f, false, FPlatformTime::Cycles64()));
}

// ---------------------------------------------------------------- what's on screen, by what you're doing
void AKeysHUD::BuildTouch(AKeysBoatPawn* P, TArray<FTouchCtl>& Out)
{
	const float SW = Canvas->SizeX, SH = Canvas->SizeY;
	// thumb-sized: about a fifth of the height on a phone, less on a squarer tablet
	const float B = SH * (SW / SH > 1.8f ? .17f : .12f), G = B * .16f, Sm = B * .62f;
	const FLinearColor Go(.35f, .9f, .55f, 1), Warn(1, .78f, .3f, 1), Red(1, .45f, .4f, 1);
	auto Btn = [&](const FString& L, float X, float Y, float W, float H, std::initializer_list<FKey> Keys, bool bBig = false, FLinearColor Tint = FLinearColor::White)
	{
		FTouchCtl C;
		C.Label = L;
		C.R = FBox2D(FVector2D(X, Y), FVector2D(X + W, Y + H));
		C.Keys = Keys;
		C.bBig = bBig;
		C.Tint = Tint;
		Out.Add(C);
	};
	auto Stick = [&](const FString& Mode)
	{
		FTouchCtl C;
		C.Label = Mode;
		C.bStick = true;
		C.R = FBox2D(FVector2D(0, SH * .38f), FVector2D(SW * .36f, SH));
		Out.Add(C);
	};
	// the right-hand cluster: a big main button in the corner, then up to three more around it
	const float BigX = SW - B * 1.35f - G * 2, BigY = SH - B * 1.35f - G * 2;
	auto Main = [&](const FString& L, std::initializer_list<FKey> Keys, FLinearColor Tint = FLinearColor::White) { Btn(L, BigX, BigY, B * 1.35f, B * 1.35f, Keys, true, Tint); };
	auto Side = [&](int32 Slot, const FString& L, std::initializer_list<FKey> Keys, FLinearColor Tint = FLinearColor::White)
	{
		// 0 left of the main button, 1 above it, 2 up and to the left, 3 further left
		const float X = Slot == 1 ? BigX + B * .35f * .5f : Slot == 2 ? BigX - B - G : Slot == 3 ? BigX - (B + G) * 2 : BigX - B - G;
		const float Y = Slot == 1 || Slot == 2 ? BigY - B - G : BigY + B * .35f;
		Btn(L, X, Y, B, B, Keys, false, Tint);
	};
	// a row of small buttons along the top right
	int32 TopN = 0;
	// (it ends left of the chart in the top right corner, when the chart is up)
	const float TopRight = P->bRunning && P->Menu == AKeysBoatPawn::EMenu::None ? SW - 290 * U : SW - G;
	auto Top = [&](const FString& L, std::initializer_list<FKey> Keys) { Btn(L, TopRight - (Sm * 1.25f + G * .6f) * (++TopN), G, Sm * 1.25f, Sm * .7f, Keys); };
	auto PanelPad = [&](bool bArrows, const FString& OkLabel, const FKey& Ok, const FString& BackLabel, const FKey& Back)
	{
		if (bArrows) Stick(TEXT("arrows"));
		Main(OkLabel, {Ok}, Go);
		Side(0, BackLabel, {Back});
	};

	const bool bCard = P->FState == EKeysFish::Caught;
	auto CardButtons = [&](bool bTag)
	{
		Main(TEXT("KEEP"), {EKeys::K}, Go);
		Side(0, TEXT("RELEASE"), {EKeys::SpaceBar});
		if (bTag) Side(1, TEXT("TAG"), {EKeys::T}, Warn);
	};

	// modal panels first, in the order HandleInput gives them the keys
	if (P->bLoginOpen) { PanelPad(false, TEXT("JOIN"), EKeys::Enter, TEXT("CLOSE"), EKeys::Escape); Side(1, TEXT("NEXT"), {EKeys::Tab}); return; }
	if (P->bBoardOpen) { Main(TEXT("CLOSE"), {EKeys::B}); Side(0, TEXT("<"), {EKeys::Left}); Side(1, TEXT(">"), {EKeys::Right}); return; }
	if (!P->bRunning || P->Menu != AKeysBoatPawn::EMenu::None)
	{
		// the menu rows are tapped directly; Back where there is somewhere to go back to
		if (P->Menu != AKeysBoatPawn::EMenu::Main && P->Menu != AKeysBoatPawn::EMenu::None) Btn(TEXT("BACK"), G, G, Sm * 1.6f, Sm * .8f, {EKeys::Escape});
		return;
	}
	if (P->bGearOpen) { Main(TEXT("CLOSE"), {EKeys::Tab}); return; }
	if (AKeysSlots* Sl = AKeysSlots::Get(this); Sl && Sl->bOpen) { PanelPad(true, TEXT("SPIN"), EKeys::SpaceBar, TEXT("LEAVE"), EKeys::Escape); Side(1, TEXT("OK"), {EKeys::Enter}); return; }
	if (AKeysEstates* Es = AKeysEstates::Get(this); Es && Es->bOpen) { PanelPad(true, TEXT("OK"), EKeys::Enter, TEXT("CLOSE"), EKeys::Escape); Side(1, TEXT("TAB"), {EKeys::Tab}); Side(2, TEXT("PG UP"), {EKeys::PageUp}); Side(3, TEXT("PG DN"), {EKeys::PageDown}); return; }
	if (P->SvcCard.bOn) { Main(TEXT("OK"), {EKeys::SpaceBar}); return; }
	if (AKeysHandgun* Gun = AKeysHandgun::Get(this); Gun && Gun->IsOut())
	{
		Stick(TEXT("arrows"));
		Main(TEXT("FIRE"), {EKeys::SpaceBar}, Red);
		Side(0, TEXT("RELOAD"), {EKeys::R});
		Side(1, TEXT("FLARE"), {EKeys::Z}, Warn);
		Side(2, TEXT("HOLSTER"), {EKeys::Nine});
		Top(TEXT("MAP"), {EKeys::M});
		return;
	}
	Top(TEXT("MENU"), {EKeys::Escape});

	if (P->IsDiving())
	{
		if (bCard) { CardButtons(false); return; }
		Stick(TEXT("wasd"));
		Main(TEXT("FIRE"), {EKeys::LeftMouseButton}, Red);
		Side(0, TEXT("UP"), {EKeys::SpaceBar});
		Side(1, TEXT("DOWN"), {EKeys::C});
		Side(2, TEXT("RELOAD"), {EKeys::R});
		Side(3, TEXT("SPRINT"), {EKeys::LeftShift});
		Top(TEXT("ABOARD"), {EKeys::J}); Top(TEXT("OPEN"), {EKeys::F}); Top(TEXT("VIEW"), {EKeys::One}); Top(TEXT("MAP"), {EKeys::M});
		return;
	}
	if (P->IsAshore())
	{
		AKeysWalker* W = AKeysWalker::Get(this);
		if (W && W->bShopOpen)
		{
			Main(TEXT("BUY"), {EKeys::Enter}, Go);
			Side(0, TEXT("LEAVE"), {EKeys::Q});
			Side(1, TEXT("UP"), {EKeys::Up});
			Side(2, TEXT("DOWN"), {EKeys::Down});
			return;
		}
		if (P->bCharging || P->FState != EKeysFish::Idle)
		{
			if (bCard) { CardButtons(true); return; }
			Main(P->FState == EKeysFish::Fight ? TEXT("REEL") : P->IsLineOut() ? TEXT("STRIKE") : TEXT("CAST"), {EKeys::SpaceBar}, Go);
			if (P->IsLineOut()) { Side(0, TEXT("WIND"), {EKeys::Down}); if (P->FState != EKeysFish::Fight) Side(1, TEXT("REEL IN"), {EKeys::R}); else Side(1, TEXT("DRAG"), {EKeys::T}, Warn); }
			Top(TEXT("MAP"), {EKeys::M});
			return;
		}
		Stick(TEXT("walk"));
		Main(TEXT("CAST"), {EKeys::SpaceBar}, Go);
		Side(0, TEXT("USE"), {EKeys::F}, Warn);
		Side(1, TEXT("RUN"), {EKeys::LeftShift});
		Top(TEXT("ABOARD"), {EKeys::Q}); Top(TEXT("VIEW"), {EKeys::One}); Top(TEXT("MAP"), {EKeys::M}); Top(TEXT("GEAR"), {EKeys::Tab});
		return;
	}

	// at the helm
	if (bCard) { CardButtons(true); return; }
	Stick(TEXT("wasd"));
	const EKeysFish S = P->FState;
	if (S == EKeysFish::Fight) { Main(TEXT("REEL"), {EKeys::SpaceBar}, Go); Side(0, TEXT("WIND"), {EKeys::Down}); Side(1, TEXT("DRAG +"), {EKeys::T}, Warn); Side(2, TEXT("DRAG -"), {EKeys::LeftShift, EKeys::T}, Warn); }
	else if (P->IsLineOut()) { Main(S == EKeysFish::Bite || S == EKeysFish::Nibble ? TEXT("STRIKE!") : TEXT("STRIKE"), {EKeys::SpaceBar}, S == EKeysFish::Bite ? Red : Go); Side(0, TEXT("REEL IN"), {EKeys::R}); Side(1, TEXT("WIND"), {EKeys::Down}); }
	else
	{
		Main(TEXT("CAST"), {EKeys::SpaceBar}, Go);
		Side(0, TEXT("CHASE"), {EKeys::F});
		Side(1, TEXT("TROLL"), {EKeys::Equals});
		Side(2, TEXT("WEIGH"), {EKeys::E}, Warn);
	}
	Top(TEXT("VIEW"), {EKeys::One}); Top(TEXT("ROD"), {EKeys::V}); Top(TEXT("MAP"), {EKeys::M}); Top(TEXT("SONAR"), {EKeys::G});
	Top(TEXT("GEAR"), {EKeys::Tab}); Top(TEXT("DIVE"), {EKeys::J}); Top(TEXT("ASHORE"), {EKeys::Q});
}

// ---------------------------------------------------------------- fingers to keys, and drawing
void AKeysHUD::DrawTouch(AKeysBoatPawn* P)
{
	APlayerController* PC = GetOwningPlayerController();
	if (!PC) return;
	TArray<FTouchCtl> Ctls;
	BuildTouch(P, Ctls);

	// where every finger is this frame (the Mac's mouse is finger 99 in -KRTouch, the script's is -1)
	TMap<int32, FVector2D> Now;
	for (int32 i = 0; i < EKeys::NUM_TOUCH_KEYS; i++)
	{
		float FX = 0, FY = 0;
		bool bDown = false;
		PC->GetInputTouchState(ETouchIndex::Type(i), FX, FY, bDown);
		if (bDown) Now.Add(i, FVector2D(FX, FY));
	}
#if !(PLATFORM_IOS || PLATFORM_ANDROID)
	{
		float MX = 0, MY = 0;
		if (PC->IsInputKeyDown(EKeys::LeftMouseButton) && !TouchHeld.Contains(EKeys::LeftMouseButton) && PC->GetMousePosition(MX, MY)) Now.Add(99, FVector2D(MX, MY));
	}
#endif
	if (!ScriptHold.IsEmpty())
		for (const FTouchCtl& C : Ctls)
			if (C.Label.Equals(ScriptHold, ESearchCase::IgnoreCase)) { Now.Add(-1, C.R.GetCenter() + (C.bStick ? FVector2D(0, -1) : FVector2D::ZeroVector)); break; }

	// fingers that lifted, and new ones: which control (or HUD button, or the look pad) each one started on
	for (auto It = Fingers.CreateIterator(); It; ++It) if (!Now.Contains(It.Key())) It.RemoveCurrent();
	const FString StickMode = [&] { for (const FTouchCtl& C : Ctls) if (C.bStick) return C.Label; return FString(); }();
	for (const TPair<int32, FVector2D>& F : Now)
	{
		FFinger* Fi = Fingers.Find(F.Key);
		if (!Fi)
		{
			FFinger NewF;
			NewF.Start = NewF.Last = F.Value;
			for (int32 c = 0; c < Ctls.Num() && NewF.Ctl < 0; c++) if (!Ctls[c].bStick && Ctls[c].R.IsInsideOrOn(F.Value)) NewF.Ctl = c;
			if (NewF.Ctl < 0) for (const FHUDHitBox& HB : HitBoxMap) if (HB.Contains(F.Value)) { NewF.bHud = true; break; }
			if (NewF.Ctl < 0 && !NewF.bHud) for (int32 c = 0; c < Ctls.Num(); c++) if (Ctls[c].bStick && Ctls[c].R.IsInsideOrOn(F.Value)) NewF.Ctl = c;
			NewF.bLook = NewF.Ctl < 0 && !NewF.bHud;
			if (F.Key == -1 && NewF.Ctl >= 0 && Ctls[NewF.Ctl].bStick) NewF.Start = F.Value + FVector2D(0, 1);   // the script pushes the stick forward
			Fi = &Fingers.Add(F.Key, NewF);
		}
		else if (Fi->Ctl >= Ctls.Num()) Fi->Ctl = -1;   // the controls changed under a held finger: let it go
	}

	// the keys the fingers want held
	TSet<FKey> Want;
	FVector2D Look = ScriptLookDelta;
	ScriptLookDelta = FVector2D::ZeroVector;
	StickKnob = FVector2D::ZeroVector;
	const float Radius = Canvas->SizeY * .11f;
	for (TPair<int32, FFinger>& F : Fingers)
	{
		const FVector2D At = Now[F.Key];
		if (F.Value.bLook) { Look += At - F.Value.Last; F.Value.Last = At; continue; }
		if (F.Value.Ctl < 0) continue;
		const FTouchCtl& C = Ctls[F.Value.Ctl];
		if (!C.bStick) { for (const FKey& K : C.Keys) Want.Add(K); continue; }
		// the stick: centred where the thumb landed, 8-way with a dead zone
		FVector2D D = At - F.Value.Start;
		if (D.Size() > Radius) D = D.GetSafeNormal() * Radius;
		StickKnob = D / Radius;
		const bool bUp = StickKnob.Y < -.35f, bDn = StickKnob.Y > .35f, bL = StickKnob.X < -.35f, bR = StickKnob.X > .35f;
		if (C.Label == TEXT("arrows")) { if (bUp) Want.Add(EKeys::Up); if (bDn) Want.Add(EKeys::Down); if (bL) Want.Add(EKeys::Left); if (bR) Want.Add(EKeys::Right); }
		else
		{
			if (bUp) Want.Add(EKeys::W);
			if (bDn) Want.Add(EKeys::S);
			// walking turns with the arrows and strafes with A / D: a thumb wants to turn
			if (C.Label == TEXT("walk")) { if (bL) Want.Add(EKeys::Left); if (bR) Want.Add(EKeys::Right); }
			else { if (bL) Want.Add(EKeys::A); if (bR) Want.Add(EKeys::D); }
		}
	}
	// a new mode (the stick changed what it drives) lets everything go first, so nothing sticks down across it
	if (StickMode != StickFor) { for (const FKey& K : TouchHeld) SendKey(K, false); TouchHeld.Reset(); StickFor = StickMode; }
	// looking: the boat's camera orbits while a mouse button is held, the first-person views read the mouse directly
	const bool bLooking = Fingers.Num() && Algo::AnyOf(Fingers, [](const TPair<int32, FFinger>& F) { return F.Value.bLook; });
	if (bLooking && !P->IsDiving() && !P->IsAshore()) Want.Add(EKeys::RightMouseButton);
	for (const FKey& K : TouchHeld) if (!Want.Contains(K)) SendKey(K, false);
	for (const FKey& K : Want) if (!TouchHeld.Contains(K)) SendKey(K, true);
	TouchHeld = Want;
	if (!Look.IsZero())
	{
		FViewport* VP = GEngine && GEngine->GameViewport ? GEngine->GameViewport->Viewport : nullptr;
		const FInputDeviceId Dev = IPlatformInputDeviceMapper::Get().GetDefaultInputDevice();
		const float K = 1080.f / FMath::Max(1.f, Canvas->SizeY) * .9f;   // the same turn for the same swipe on any screen
		const float Dt = GetWorld()->GetDeltaSeconds();
		PC->InputKey(FInputKeyEventArgs(VP, Dev, EKeys::MouseX, float(Look.X) * K, Dt, 1, FPlatformTime::Cycles64()));
		PC->InputKey(FInputKeyEventArgs(VP, Dev, EKeys::MouseY, float(-Look.Y) * K, Dt, 1, FPlatformTime::Cycles64()));
	}

	// draw: translucent rounded-ish pads, the held one lit
	UFont* Font = GEngine->GetMediumFont();
	for (int32 c = 0; c < Ctls.Num(); c++)
	{
		const FTouchCtl& C = Ctls[c];
		bool bHeld = false;
		for (const TPair<int32, FFinger>& F : Fingers) if (F.Value.Ctl == c) bHeld = true;
		if (C.bStick)
		{
			// the stick's base sits where a thumb rests unless one is down, then under the thumb
			FVector2D Base(Canvas->SizeX * .33f, C.R.Max.Y - Radius * 1.5f);   // clear of the gauges in the corner
			for (const TPair<int32, FFinger>& F : Fingers) if (F.Value.Ctl == c) Base = F.Value.Start;
			const int32 Seg = 28;
			for (int32 i = 0; i < Seg; i++)
			{
				const float A0 = 2 * PI * i / Seg, A1 = 2 * PI * (i + 1) / Seg;
				DrawLine(Base.X + FMath::Cos(A0) * Radius, Base.Y + FMath::Sin(A0) * Radius, Base.X + FMath::Cos(A1) * Radius, Base.Y + FMath::Sin(A1) * Radius, FLinearColor(1, 1, 1, bHeld ? .55f : .3f), 3 * U);
			}
			const FVector2D Kn = Base + StickKnob * Radius * (bHeld ? 1.f : 0.f);
			const float KR = Radius * .42f;
			DrawRect(FLinearColor(1, 1, 1, bHeld ? .45f : .22f), Kn.X - KR * .5f, Kn.Y - KR * .5f, KR, KR);
			continue;
		}
		const FVector2D Sz = C.R.GetSize();
		DrawRect(FLinearColor(0.01f, .03f, .06f, bHeld ? .7f : .42f), C.R.Min.X, C.R.Min.Y, Sz.X, Sz.Y);
		const FLinearColor Edge(C.Tint.R, C.Tint.G, C.Tint.B, bHeld ? .95f : .55f);
		const float T = FMath::Max(2.f, 2.5f * U);
		DrawRect(Edge, C.R.Min.X, C.R.Min.Y, Sz.X, T); DrawRect(Edge, C.R.Min.X, C.R.Max.Y - T, Sz.X, T);
		DrawRect(Edge, C.R.Min.X, C.R.Min.Y, T, Sz.Y); DrawRect(Edge, C.R.Max.X - T, C.R.Min.Y, T, Sz.Y);
		// the label fits the pad
		float TW = 0, TH = 0;
		GetTextSize(C.Label, TW, TH, Font, 1.f);
		const float Sc = FMath::Min((Sz.X * .82f) / FMath::Max(1.f, TW), (Sz.Y * (C.bBig ? .3f : .36f)) / FMath::Max(1.f, TH));
		const FVector2D Mid = C.R.GetCenter();
		DrawText(C.Label, FLinearColor(0, 0, 0, .6f), Mid.X - TW * Sc * .5f + 1.5f, Mid.Y - TH * Sc * .5f + 1.5f, Font, Sc);
		DrawText(C.Label, FLinearColor(C.Tint.R, C.Tint.G, C.Tint.B, bHeld ? 1.f : .9f), Mid.X - TW * Sc * .5f, Mid.Y - TH * Sc * .5f, Font, Sc);
	}
}
