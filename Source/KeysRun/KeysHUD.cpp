#include "KeysHUD.h"
#include "KeysDiver.h"
#include "KeysBoatPawn.h"
#include "KeysEvents.h"
#include "KeysGeo.h"
#include "KeysData.h"
#include "KeysSave.h"
#include "KeysTraffic.h"
#include "KeysExtras.h"
#include "KeysAudio.h"
#include "KeysLife.h"
#include "KeysOnline.h"
#include "KeysWalker.h"
#include "KeysTownData.h"
#include "KeysKit.h"
#include "KeysHandgun.h"
#include "KeysTourneyShow.h"
#include "EngineUtils.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Engine/Font.h"
#include "Engine/Texture2D.h"
#include "GameFramework/PlayerController.h"

using namespace KeysGeo;

namespace
{
	const FLinearColor Teal(.35f, .85f, .85f), Gold(1.f, .82f, .35f), Red(1.f, .3f, .25f), Dim(.75f, .8f, .85f);
	constexpr int32 ChartN = 320;
	constexpr int32 GpsW = 256, GpsH = 148;      // GPS texture, covering a little more than the screen so it can scroll between redraws
	constexpr double GpsMargin = 1.15;
	constexpr int32 NumSonarCols = 85;             // 256 px canvas / 3 px per ping in the JS
	FLinearColor HudHex(uint32 C, float A = 1.f) { return FLinearColor(((C >> 16) & 255) / 255.f, ((C >> 8) & 255) / 255.f, (C & 255) / 255.f, A); }
	double HudHyp(double A, double B) { return FMath::Sqrt(A * A + B * B); }
	// chart depth bands from the JS chartplotter: land, then 0-6 ft, 6-12, 12-20, 20-66, 66-200 and deeper
	int32 HudBand(double D) { return D <= .36 ? -1 : D < 1.8 ? 0 : D < 3.6 ? 1 : D < 6 ? 2 : D < 20 ? 3 : D < 60 ? 4 : 5; }
	FString HudLatLon(double GX, double GZ)
	{
		const double Lat = 24.7130 - (GZ - 0) / 111320 * 2.5, Lon = 81.0900 - (GX - -120) / 101090 * 2.5;   // Vaca Key center is (-120, 0)
		auto F = [](double V, bool bThree)
		{
			const int32 A = FMath::FloorToInt(V);
			const double M = (V - A) * 60;
			return (bThree ? FString::Printf(TEXT("%03d"), A) : FString::Printf(TEXT("%02d"), A)) + FString::Chr(0x00B0) + FString::Printf(TEXT("%06.3f'"), M);
		};
		return TEXT("N ") + F(Lat, false) + TEXT("   W ") + F(Lon, true);
	}
}

void AKeysHUD::Panel(float X, float Y, float W, float H, float Alpha) { DrawRect(FLinearColor(0.01f, .03f, .06f, Alpha), X, Y, W, H); }

float AKeysHUD::Text(const FString& S, float X, float Y, float Scale, const FLinearColor& C, UFont* Font, bool bCenter)
{
	if (!Font) Font = GEngine->GetMediumFont();
	float W = 0, H = 0;
	GetTextSize(S, W, H, Font, Scale * U);
	if (bCenter) X -= W * .5f;
	DrawText(S, FLinearColor(0, 0, 0, .6f), X + 1.5f * U, Y + 1.5f * U, Font, Scale * U);
	DrawText(S, C, X, Y, Font, Scale * U);
	return H;
}

float AKeysHUD::Wrapped(const FString& S, float X, float Y, float W, float Scale, const FLinearColor& C)
{
	TArray<FString> Words;
	S.ParseIntoArray(Words, TEXT(" "));
	FString Line;
	float Yy = Y;
	UFont* Font = GEngine->GetMediumFont();
	for (const FString& Wd : Words)
	{
		const FString Try = Line.IsEmpty() ? Wd : Line + TEXT(" ") + Wd;
		float TW = 0, TH = 0;
		GetTextSize(Try, TW, TH, Font, Scale * U);
		if (TW > W && !Line.IsEmpty()) { Yy += Text(Line, X, Yy, Scale, C); Line = Wd; }
		else Line = Try;
	}
	if (!Line.IsEmpty()) Yy += Text(Line, X, Yy, Scale, C);
	return Yy - Y;
}

void AKeysHUD::Bar(float X, float Y, float W, float H, float Fill, const FLinearColor& C)
{
	DrawRect(FLinearColor(1, 1, 1, .15f), X, Y, W, H);
	DrawRect(C, X, Y, W * FMath::Clamp(Fill, 0.f, 1.f), H);
}

// nautical-chart style: buff land, blue shallows fading to deep water
void AKeysHUD::BuildChart()
{
	KeysGeo::Init();
	Chart = UTexture2D::CreateTransient(ChartN, ChartN, PF_B8G8R8A8);
	if (!Chart) return;
	Chart->SRGB = true;
	Chart->Filter = TF_Bilinear;
	TArray<FColor> Px;
	Px.SetNumUninitialized(ChartN * ChartN);
	const FLinearColor Land(.86f, .80f, .62f);
	for (int32 j = 0; j < ChartN; j++)
		for (int32 i = 0; i < ChartN; i++)
		{
			const double X = WorldX0 + (i + .5) / ChartN * WorldSize, Z = WorldZ0 + (j + .5) / ChartN * WorldSize;
			const double H = LandH(X, Z);
			FLinearColor C;
			if (H > .1) C = Land;
			else
			{
				// chart blues: pale in the shallows, deeper toward the Gulf Stream (stored as display colors)
				const double D = DepthFast(X, Z);
				const double K = FMath::Clamp(FMath::Loge(D + 1) / FMath::Loge(401.0), 0.0, 1.0);
				C = FMath::Lerp(FLinearColor(.62f, .86f, .93f), FLinearColor(.10f, .28f, .52f), float(K));
			}
			Px[j * ChartN + i] = FColor(uint8(C.R * 255), uint8(C.G * 255), uint8(C.B * 255), 255);
		}
	FTexture2DMipMap& Mip = Chart->GetPlatformData()->Mips[0];
	void* Data = Mip.BulkData.Lock(LOCK_READ_WRITE);
	FMemory::Memcpy(Data, Px.GetData(), Px.Num() * sizeof(FColor));
	Mip.BulkData.Unlock();
	Chart->UpdateResource();
}

FVector2D AKeysHUD::ChartXY(double GX, double GZ, float X, float Y, float Size) const
{
	return FVector2D(X + (GX - WorldX0) / WorldSize * Size, Y + (GZ - WorldZ0) / WorldSize * Size);
}

void AKeysHUD::DrawHUD()
{
	Super::DrawHUD();
	if (!Canvas) return;
	U = Canvas->SizeY / 1080.f;
	AKeysBoatPawn* P = Cast<AKeysBoatPawn>(GetOwningPawn());
	if (!P) return;
	if (!Chart) BuildChart();

	const double Now = GetWorld()->GetTimeSeconds();
	if (!P->bRunning)
	{
		DrawMenu(P);
		if (Now < P->ToastUntil && !P->Toast.IsEmpty())
		{
			float TW = 0, TH = 0;
			GetTextSize(P->Toast, TW, TH, GEngine->GetMediumFont(), 1.05f * U);
			Panel(Canvas->SizeX * .5f - TW * .5f - 18 * U, 28 * U, TW + 36 * U, TH + 16 * U, .7f);
			Text(P->Toast, Canvas->SizeX * .5f, 36 * U, 1.05f, FLinearColor::White, nullptr, true);
		}
		if (P->bBoardOpen) DrawBoard(P);
		if (P->bLoginOpen) DrawLogin(P);
		Contacts.Reset();
		KeysTags::Live().Reset();
		return;
	}
	// paused: just the menu (and whatever it opened) over the scene
	if (P->Menu != AKeysBoatPawn::EMenu::None)
	{
		DrawMenu(P);
		if (P->bBoardOpen) DrawBoard(P);
		if (P->bLoginOpen) DrawLogin(P);
		Contacts.Reset();
		KeysTags::Live().Reset();
		return;
	}
	UpdateElectronics(P, Now);
	AKeysDiver* Dv = AKeysDiver::Get(this);
	const bool bDive = Dv && Dv->IsDiving();
	AKeysWalker* Wk = AKeysWalker::Get(this);
	const bool bAshore = Wk && Wk->IsAshore();
	if (bDive) DrawDiving(P, Dv);
	if (const AKeysTourneyShow* Sh0 = AKeysTourneyShow::Get(this); !Sh0 || !Sh0->bLetterbox) DrawExtras(P);
	DrawFriends(P);
	{
		// the buff strip (web game/buffs.ts): what you ate and drank in town, and how long it has left
		TArray<AKeysBoatPawn::FBuffShow> Bf;
		P->LiveBuffs(Bf);
		float BX = 24 * U;
		for (const AKeysBoatPawn::FBuffShow& B : Bf)
		{
			const FString S = FString::Printf(TEXT("%s %d:%02d"), *B.Label, B.Left / 60, B.Left % 60);
			float TW = 0, TH = 0;
			GetTextSize(S, TW, TH, GEngine->GetMediumFont(), .8f * U);
			Panel(BX, 140 * U, TW + 20 * U, 30 * U, .55f);
			DrawRect(B.bDrink ? FLinearColor(.95f, .55f, .2f) : Teal, BX, 140 * U, 4 * U, 30 * U);
			Text(S, BX + 11 * U, 145 * U, .8f, B.bDrink ? FLinearColor(1.f, .75f, .45f) : FLinearColor::White);
			BX += TW + 28 * U;
		}
	}
	const AKeysHandgun* Gun = AKeysHandgun::Get(this);
	const AKeysTourneyShow* Show = AKeysTourneyShow::Get(this);
	// the pistol has its own HUD, and the tournament's titles and ceremony take the whole screen
	const bool bGun = (Gun && Gun->IsOut()) || (Show && Show->bLetterbox);
	if (bGun) { if (P->bBigMap) DrawChart(P); }
	else if (bAshore)
	{
		DrawAshore(P, Wk);
		if (P->bBigMap) DrawChart(P);
	}
	else if (!bDive)
	{
		// over the side the helm gauges, the chart and the driving hint mean nothing (web setDiveUI)
		DrawGauges(P);
		DrawChart(P);
		DrawFishing(P);
	}
	else if (P->bBigMap) DrawChart(P);
	if (!bDive && !bAshore && !bGun) DrawEvents(P);
	if (P->FState == EKeysFish::Caught) DrawCard(P);
	else if (P->bElectronics && !P->bBigMap) DrawElectronics(P);
	DrawTags();
	DrawMore(P);

	if (Now < P->ToastUntil && !P->Toast.IsEmpty())
	{
		float W = 0, H = 0;
		GetTextSize(P->Toast, W, H, GEngine->GetMediumFont(), 1.05f * U);
		const float Cx = Canvas->SizeX * .5f;
		Panel(Cx - W * .5f - 18 * U, 28 * U, W + 36 * U, H + 16 * U, .55f);
		Text(P->Toast, Cx, 36 * U, 1.05f, FLinearColor::White, nullptr, true);
	}
	const FString Hint = bDive || bAshore || bGun ? FString() : P->Hint();
	if (!Hint.IsEmpty()) Text(Hint, Canvas->SizeX * .5f, Canvas->SizeY - 44 * U, .95f, Dim, nullptr, true);
	if (P->bBoardOpen) DrawBoard(P);
	if (P->bLoginOpen) DrawLogin(P);
	DrawPanels(P);
	Contacts.Reset();
}

void AKeysHUD::Button(FName Box, float X, float Y, float W, float H, bool bHot, int32 Priority)
{
	Panel(X, Y, W, H, bHot ? .78f : .42f);
	if (bHot) DrawRect(Gold, X, Y, 5 * U, H);
	AddHitBox(FVector2D(X, Y), FVector2D(W, H), Box, true, Priority);
}

void AKeysHUD::NotifyHitBoxClick(FName BoxName)
{
	if (AKeysBoatPawn* P = Cast<AKeysBoatPawn>(GetOwningPawn())) P->OnHudClick(BoxName);
}

void AKeysHUD::NotifyHitBoxBeginCursorOver(FName BoxName)
{
	if (AKeysBoatPawn* P = Cast<AKeysBoatPawn>(GetOwningPawn())) P->OnHudHover(BoxName);
}

// ================================================================ menus
void AKeysHUD::DrawMenu(AKeysBoatPawn* P)
{
	using EMenu = AKeysBoatPawn::EMenu;
	const float W = Canvas->SizeX, H = Canvas->SizeY;
	const bool bMain = P->Menu == EMenu::Main;
	// your boat stays visible on the right; the menu column sits on a dark band on the left
	DrawRect(FLinearColor(0, .02f, .05f, bMain ? .15f : .4f), 0, 0, W, H);
	const float X = 100 * U, ColW = 640 * U;
	DrawRect(FLinearColor(0, .025f, .05f, .62f), 0, 0, X + ColW + 70 * U, H);
	UFont* Big = GEngine->GetLargeFont();
	const TCHAR* Title = bMain ? TEXT("KEYS RUN") : P->Menu == EMenu::Pause ? TEXT("Paused") : P->Menu == EMenu::Settings ? TEXT("Settings") : TEXT("Controls");
	Text(Title, X, 70 * U, bMain ? 4.2f : 2.8f, FLinearColor::White, Big);
	if (bMain) Text(TEXT("Center console fishing  -  Marathon, Florida Keys"), X + 6 * U, 196 * U, 1.3f, Teal);

	TArray<AKeysBoatPawn::FMenuItem> Items;
	P->GetMenuItems(Items);
	// a long menu (the pause menu with the land office and run-to) packs its rows tighter
	const float Fit = FMath::Min(1.f, 11.f / FMath::Max(1, Items.Num()) * (Items.Num() > 10 ? .93f : 1.f));
	const float RowH = 74 * U * Fit, BoxH = 64 * U * Fit;
	float Y = (Items.Num() > 10 ? 230 : 280) * U;
	for (int32 i = 0; i < Items.Num(); i++)
	{
		const AKeysBoatPawn::FMenuItem& It = Items[i];
		const bool bHot = i == P->MenuSel;
		const FString Id = It.Id.ToString();
		Button(FName(*(TEXT("m:") + Id)), X, Y, ColW, BoxH, bHot);
		Text(It.Label, X + 30 * U, Y + 12 * U, 1.55f, bHot ? Gold : FLinearColor::White);
		if (It.bArrows)
		{
			// < value >   (the arrows are their own buttons, on top of the row)
			const float AW = 46 * U, AX0 = X + 300 * U, AX1 = X + ColW - AW - 12 * U, AY = Y + 5 * U;
			Button(FName(*(TEXT("m<:") + Id)), AX0, AY, AW, BoxH - 10 * U, false, 1);
			Button(FName(*(TEXT("m>:") + Id)), AX1, AY, AW, BoxH - 10 * U, false, 1);
			Text(TEXT("<"), AX0 + AW * .5f, AY + 8 * U, 1.5f, bHot ? Gold : Dim, nullptr, true);
			Text(TEXT(">"), AX1 + AW * .5f, AY + 8 * U, 1.5f, bHot ? Gold : Dim, nullptr, true);
			FString V = It.Value;
			float TW = 0, TH = 0;
			GetTextSize(V, TW, TH, GEngine->GetMediumFont(), 1.25f * U);
			const float Room = AX1 - AX0 - AW - 16 * U;
			const float Sc = 1.25f * (TW > Room ? FMath::Max(.55f, Room / TW) : 1.f);
			Text(V, (AX0 + AW + AX1) * .5f, Y + 16 * U, Sc, FLinearColor::White, nullptr, true);
		}
		else if (!It.Value.IsEmpty())
		{
			float TW = 0, TH = 0;
			GetTextSize(It.Value, TW, TH, GEngine->GetMediumFont(), 1.0f * U);
			Text(It.Value, X + ColW - 24 * U - FMath::Min(TW, 330 * U), Y + 20 * U, 1.0f * FMath::Min(1.f, 330 * U / FMath::Max(TW, 1.f)), Dim);
		}
		Y += RowH;
	}
	Text(TEXT("Mouse or arrow keys  -  Enter selects  -  Left / Right change  -  Esc back"), X, H - 70 * U, 1.05f, Dim);

	// the right-hand side
	if (bMain)
	{
		const KeysData::FBoatSpec& S = KeysData::Boats()[P->PickIndex];
		const float CW = 620 * U, CX = W - CW - 70 * U, CY = H - 330 * U;
		Panel(CX, CY, CW, 270 * U, .62f);
		float Yy = CY + 18 * U;
		Text(FString::Printf(TEXT("%s %s"), *S.Brand, *S.Name), CX + 26 * U, Yy, 1.45f, Gold, Big);
		Yy += 50 * U;
		Text(FString::Printf(TEXT("%s  -  %.0f ft  -  %.0f mph top  -  draft %s"), *S.Power, S.Len * 3.28, S.Top * 1.15, *S.DraftFt), CX + 26 * U, Yy, 1.05f, Dim);
		Yy += 40 * U;
		Wrapped(S.Desc, CX + 26 * U, Yy, CW - 52 * U, 1.1f);
		// the boat broker: what it costs, and what you have
		const bool bOwn = P->OwnsBoat(S.Id);
		const FString Own = bOwn ? (S.Price == 0 ? FString(TEXT("Yours - one of the original fleet")) : FString(TEXT("Yours - bought from the broker")))
			: FString::Printf(TEXT("$%s  -  you have $%s  -  %s"), *FText::AsNumber(S.Price).ToString(), *FText::AsNumber(P->Cash).ToString(), P->Cash >= S.Price ? TEXT("Enter to buy") : TEXT("sell fillets at a weigh-in"));
		Text(Own, CX + 26 * U, CY + 270 * U - 38 * U, .85f, bOwn ? Teal : Gold);
		Text(FString::Printf(TEXT("Boat %d of %d"), P->PickIndex + 1, KeysData::Boats().Num()), CX + CW - 26 * U - 120 * U, CY + 270 * U - 38 * U, .85f, Dim);
	}
	else if (P->Menu == EMenu::Controls)
	{
		const float CX = X + ColW + 140 * U;
		Panel(CX, 150 * U, W - CX - 80 * U, H - 260 * U, .7f);
		DrawControlsTable(CX + 30 * U, 176 * U, W - CX - 140 * U);
	}
	else if (P->Menu == EMenu::Settings)
	{
		const float CX = X + ColW + 140 * U;
		const FString Sel = Items.IsValidIndex(P->MenuSel) ? Items[P->MenuSel].Id.ToString() : FString();
		FString Help;
		if (Sel == TEXT("sea")) Help = SeaStates()[SeaIndex()].Name + TEXT(": ") + SeaStates()[SeaIndex()].Desc;
		else if (Sel == TEXT("time")) Help = TEXT("Golden hour over the Seven Mile Bridge - Sombrero Key Light's beam sweeps and the town's lamps and neon glow - or full daylight.");
		else if (Sel == TEXT("radio")) Help = TEXT("Three stations: Beno Bonanza, Margaritaville and Hip-hop, with DJ breaks.");
		else if (Sel == TEXT("view")) Help = TEXT("Chase camera behind the boat, or first person from the helm.");
		else if (Sel == TEXT("fpview")) Help = TEXT("First person puts you at the rod with your own hands on the reel whenever the line is out.");
		else if (Sel == TEXT("diveview")) Help = TEXT("How a dive starts (J): looking out through your dive mask with the speargun in hand, or the chase view behind your fins. 1 switches while you swim.");
		else if (Sel == TEXT("haze")) Help = TEXT("How much the water hazes the distance on a dive. Visibility still shrinks with depth, from about 30 m near the surface to 10 m past 20 m.");
		else if (Sel == TEXT("autochase")) Help = TEXT("When a big fish runs, the boat follows it so you don't get spooled.");
		if (!Help.IsEmpty())
		{
			Panel(CX, 270 * U, W - CX - 80 * U, 150 * U, .62f);
			Wrapped(Help, CX + 26 * U, 296 * U, W - CX - 132 * U, 1.2f);
		}
	}
}

void AKeysHUD::DrawControlsTable(float X, float Y, float W)
{
	struct FRow { const TCHAR* K; const TCHAR* A; };
	static const FRow Drive[] = {
		{TEXT("W / S"), TEXT("Throttle / reverse")}, {TEXT("A / D"), TEXT("Steer")}, {TEXT("U"), TEXT("Gear: drive, neutral, reverse")},
		{TEXT("T, then Up / Down"), TEXT("Trim")}, {TEXT("I"), TEXT("Motors on / off")}, {TEXT("M"), TEXT("Chart")},
		{TEXT("G"), TEXT("Chartplotter and fish finder ([ ] zoom)")}, {TEXT("1"), TEXT("Chase, helm or tuna tower camera")}, {TEXT("Mouse drag / wheel"), TEXT("Look around / zoom")}};
	static const FRow Fish[] = {
		{TEXT("Hold Space"), TEXT("Power up a cast, release to throw")}, {TEXT("Space"), TEXT("Strike when a fish bites")},
		{TEXT("Hold Space or S"), TEXT("Reel during the fight")}, {TEXT("T / Shift+T"), TEXT("Drag tighter / looser")}, {TEXT("R"), TEXT("Reel in")},
		{TEXT("K / Space"), TEXT("Keep / release the fish")}, {TEXT("E"), TEXT("Weigh in at a marina dock")}, {TEXT("F / V"), TEXT("Auto-chase / first-person view")},
		{TEXT("="), TEXT("Troll: riggers out, lines out, hold 5 kn")},
		{TEXT("Cast at a wreck"), TEXT("Wreck fish (the crossed hulls on the chart)")}};
	static const FRow More[] = {
		{TEXT("Esc"), TEXT("Menu")}, {TEXT("B"), TEXT("Leaderboard")}, {TEXT("L"), TEXT("Play online / log out")}, {TEXT("H"), TEXT("Sea state")},
		{TEXT("O"), TEXT("Sunset: lighthouse beam, neon")}, {TEXT("P / N"), TEXT("Radio / sound")}, {TEXT("X"), TEXT("Travel to Miami and back")},
		{TEXT("Y / Z"), TEXT("Buddy boat / shower")}, {TEXT("R (lines in)"), TEXT("Marathon race; Esc for a tournament")},
		{TEXT(", (comma)"), TEXT("Land office: buy and build on a lot")}, {TEXT("Tab"), TEXT("Gear: owned, carried, what it adds up to")},
		{TEXT("; (semicolon)"), TEXT("Underwater and cockpit lights")}, {TEXT("Esc > Run to"), TEXT("Run to a marina, a reef, a hump, a rig")}};
	static const FRow Ashore[] = {
		{TEXT("Q"), TEXT("Tie up and go ashore / back aboard")}, {TEXT("W A S D + mouse, 1"), TEXT("Walk (Shift runs), first / third person")},
		{TEXT("F, then 1-9 / Enter"), TEXT("Use a counter, buy, read a sign")}, {TEXT("Casino counter"), TEXT("A slot machine: Space spins, Left / Right bets")}};
	static const FRow Gun[] = {
		{TEXT("9"), TEXT("Draw / holster the boat pistol")}, {TEXT("Click / Space"), TEXT("Fire (fish near the surface, seabirds)")},
		{TEXT("Mouse / arrows"), TEXT("Aim")}, {TEXT("R / Z"), TEXT("Reload / fire a parachute flare")}};
	static const FRow Dive[] = {
		{TEXT("J"), TEXT("Jump in / climb back aboard")}, {TEXT("W A S D + mouse"), TEXT("Swim where you look")},
		{TEXT("Space / C (Ctrl)"), TEXT("Ascend / descend")}, {TEXT("Shift"), TEXT("Sprint (burns air)")},
		{TEXT("Click"), TEXT("Fire the speargun")}, {TEXT("R"), TEXT("Haul the bands back to reload")},
		{TEXT("Hold S"), TEXT("Haul a speared fish in")}, {TEXT("1"), TEXT("Dive mask / chase view")}, {TEXT("F"), TEXT("Prise open a wreck's box (treasure)")}};
	const float ColW = W * .5f, Row = 24.5f * U;
	auto Block = [&](const TCHAR* Head, const FRow* Rows, int32 N, float BX, float BY)
	{
		Text(Head, BX, BY, 1.1f, Teal);
		BY += 34 * U;
		for (int32 i = 0; i < N; i++, BY += Row)
		{
			Text(Rows[i].K, BX, BY, .88f, Gold);
			Text(Rows[i].A, BX + 200 * U, BY, .88f, FLinearColor::White);
		}
		return BY;
	};
	Block(TEXT("Driving"), Drive, UE_ARRAY_COUNT(Drive), X, Y);
	const float Y2 = Block(TEXT("Fishing"), Fish, UE_ARRAY_COUNT(Fish), X + ColW, Y);
	const float Y3 = FMath::Max(Y + 34 * U + Row * UE_ARRAY_COUNT(Drive), Y2) + 16 * U;
	const float Y4 = Block(TEXT("Everything else"), More, UE_ARRAY_COUNT(More), X, Y3);
	const float Y5 = Block(TEXT("Diving and spearfishing"), Dive, UE_ARRAY_COUNT(Dive), X + ColW, Y3);
	Block(TEXT("Ashore in Boot Key Harbor City"), Ashore, UE_ARRAY_COUNT(Ashore), X, Y4 + 14 * U);
	Block(TEXT("The boat pistol"), Gun, UE_ARRAY_COUNT(Gun), X + ColW, Y5 + 14 * U);
}

// ================================================================ online
void AKeysHUD::DrawLogin(AKeysBoatPawn* P)
{
	const float W = Canvas->SizeX, H = Canvas->SizeY, PW = 720 * U, PH = 420 * U, X = W * .5f - PW * .5f, Y = H * .5f - PH * .5f;
	DrawRect(FLinearColor(0, 0, 0, .45f), 0, 0, W, H);
	Panel(X, Y, PW, PH, .92f);
	Text(TEXT("Play online"), X + 32 * U, Y + 20 * U, 1.3f, Gold, GEngine->GetLargeFont());
	Wrapped(TEXT("Pick a name and a crew code. Friends who type the same crew code join your crew: you'll see each other's boats and wakes, and share a crew leaderboard."),
		X + 32 * U, Y + 84 * U, PW - 64 * U, .9f);
	const bool bBlink = FMath::Fmod(GetWorld()->GetRealTimeSeconds(), 1.0) < .55;
	auto Field = [&](const TCHAR* Label, const FString& Val, bool bActive, float Fy)
	{
		Text(Label, X + 32 * U, Fy + 10 * U, 1.0f, bActive ? FLinearColor::White : Dim);
		DrawRect(bActive ? FLinearColor(.1f, .3f, .38f, .9f) : FLinearColor(.06f, .12f, .16f, .9f), X + 220 * U, Fy, PW - 252 * U, 46 * U);
		Text(Val + (bActive && bBlink ? TEXT("_") : TEXT("")), X + 236 * U, Fy + 8 * U, 1.1f, FLinearColor::White);
	};
	Field(TEXT("Your name"), P->LoginName, P->LoginField == 0, Y + 180 * U);
	Field(TEXT("Crew code"), P->LoginCrew, P->LoginField == 1, Y + 246 * U);
	AddHitBox(FVector2D(X + 220 * U, Y + 180 * U), FVector2D(PW - 252 * U, 46 * U), TEXT("login:name"), true, 0);
	AddHitBox(FVector2D(X + 220 * U, Y + 246 * U), FVector2D(PW - 252 * U, 46 * U), TEXT("login:crew"), true, 0);
	const float BW = 200 * U, BY = Y + PH - 104 * U;
	Button(TEXT("login:go"), X + PW - 32 * U - BW, BY, BW, 48 * U, true);
	Text(TEXT("Go online"), X + PW - 32 * U - BW * .5f, BY + 10 * U, 1.05f, Gold, nullptr, true);
	Button(TEXT("login:cancel"), X + PW - 52 * U - 2 * BW, BY, BW, 48 * U, false);
	Text(TEXT("Cancel"), X + PW - 52 * U - BW * 1.5f, BY + 10 * U, 1.05f, FLinearColor::White, nullptr, true);
	if (const AKeysOnline* O = AKeysOnline::Get(this))
		if (O->NetState() == EKeysNet::Failed) Text(O->StatusText(), X + 32 * U, Y + 302 * U, .85f, Red);
	Text(TEXT("Type, Tab switches field, Enter to go online, Esc cancels"), X + 32 * U, Y + PH - 40 * U, .8f, Dim);
}

void AKeysHUD::DrawFriends(AKeysBoatPawn* P)
{
	const AKeysOnline* O = AKeysOnline::Get(this);
	APlayerController* PC = GetOwningPlayerController();
	if (!O || !PC) return;
	TArray<TPair<FVector, FString>> Tags;
	O->GetTags(Tags);
	FVector CamLoc; FRotator CamRot;
	PC->GetPlayerViewPoint(CamLoc, CamRot);
	for (const TPair<FVector, FString>& Tg : Tags)
	{
		if (FVector::DotProduct(Tg.Key - CamLoc, CamRot.Vector()) <= 0) continue;
		const FVector S = Project(Tg.Key);
		if (S.Z <= 0) continue;
		const double Dist = FVector::Dist(Tg.Key, CamLoc) / 100;
		const FString L = Dist > 150 ? FString::Printf(TEXT("%s  %.0f m"), *Tg.Value, Dist) : Tg.Value;
		float TW = 0, TH = 0;
		GetTextSize(L, TW, TH, GEngine->GetSmallFont(), .95f * U);
		Panel(S.X - TW * .5f - 10 * U, S.Y - TH - 16 * U, TW + 20 * U, TH + 8 * U, .55f);
		Text(L, S.X, S.Y - TH - 12 * U, .95f, FLinearColor(.55f, 1.f, .75f), GEngine->GetSmallFont(), true);
	}
}

void AKeysHUD::DrawCrewBoard(float X, float Y, float W)
{
	const AKeysOnline* O = AKeysOnline::Get(this);
	if (!O || !O->IsOnline()) { Text(TEXT("Log in with L to see your crew's board."), X, Y, 1.f, Dim); return; }
	auto SpName = [](const FString& K) { const KeysData::FSpecies* S = KeysData::Species(K); return S ? S->Name : K; };
	Text(FString::Printf(TEXT("Crew \"%s\"  -  biggest fish"), *O->Crew()), X, Y, .9f, Teal);
	float Yy = Y + 40 * U;
	if (!O->Catches().Num()) Text(TEXT("Nobody in the crew has landed a fish yet."), X, Yy, .9f, Dim);
	for (int32 i = 0; i < FMath::Min(O->Catches().Num(), 10); i++)
	{
		const FKeysCrewCatch& C = O->Catches()[i];
		const FLinearColor Col = i == 0 ? Gold : C.Name == O->Name() ? FLinearColor(.55f, 1.f, .75f) : FLinearColor::White;
		Text(FString::Printf(TEXT("%d. %s"), i + 1, *SpName(C.Key)), X, Yy, .9f, Col);
		Text(FString::Printf(TEXT("%.1f lb"), C.W), X + 300 * U, Yy, .9f, Col);
		Text(C.Name, X + 420 * U, Yy, .9f, Col);
		Yy += 36 * U;
	}
	Yy = FMath::Max(Yy, Y + 40 * U + 36 * U) + 20 * U;
	Text(TEXT("Best scores"), X, Yy, .9f, Teal);
	Yy += 36 * U;
	for (int32 i = 0; i < FMath::Min(O->Scores().Num(), 6); i++)
	{
		const FKeysCrewScore& S = O->Scores()[i];
		const FLinearColor Col = i == 0 ? Gold : S.Name == O->Name() ? FLinearColor(.55f, 1.f, .75f) : FLinearColor::White;
		Text(FString::Printf(TEXT("%d. %s"), i + 1, *S.Name), X, Yy, .9f, Col);
		Text(FText::AsNumber(S.Score).ToString(), X + 300 * U, Yy, .9f, Col);
		Yy += 34 * U;
	}
}

void AKeysHUD::DrawGauges(AKeysBoatPawn* P)
{
	const float X = 24 * U, H = Canvas->SizeY;
	// score, top left
	Panel(X, 24 * U, 300 * U, 104 * U);
	Text(FString::Printf(TEXT("Score  %s"), *FText::AsNumber(P->Score).ToString()), X + 16 * U, 32 * U, 1.25f, Gold);
	Text(FString::Printf(TEXT("%d fish  -  cooler %d fish, %d / %.0f lb"), P->Count, P->Cooler.Num(), FMath::RoundToInt(P->CoolerLb()), P->Spec().CoolerCap), X + 16 * U, 70 * U, .85f, Dim);
	Text(FString::Printf(TEXT("%s %s"), *P->Spec().Brand, *P->Spec().Name), X + 16 * U, 96 * U, .85f, Dim);

	// helm, bottom left
	const float GY = H - 236 * U;
	Panel(X, GY, 340 * U, 180 * U);
	Text(FString::Printf(TEXT("%.0f"), P->KnotsNow()), X + 18 * U, GY + 4 * U, 3.0f, FLinearColor::White, GEngine->GetLargeFont());
	Text(TEXT("knots"), X + 18 * U, GY + 96 * U, .85f, Dim);
	const int32 Hdg = (FMath::RoundToInt(-P->Heading() * 180 / PI) % 360 + 360) % 360;
	Text(FString::Printf(TEXT("Gear %s   HDG %03d"), *P->GearName(), Hdg), X + 150 * U, GY + 14 * U, 1.0f, P->Gear == 2 ? Red : FLinearColor::White);
	Text(FString::Printf(TEXT("%.1f ft"), P->DepthHere() * 3.28), X + 150 * U, GY + 44 * U, 1.0f, Teal);
	Text(FString::Printf(TEXT("Trim %d%%%s"), FMath::RoundToInt(P->TrimV * 100), P->bTrimMode ? TEXT("  (trim mode)") : TEXT("")), X + 150 * U, GY + 74 * U, .9f, P->bTrimMode ? Gold : Dim);
	Text(TEXT("Throttle"), X + 18 * U, GY + 124 * U, .8f, Dim);
	Bar(X + 100 * U, GY + 130 * U, 220 * U, 10 * U, float(FMath::Max(0.0, P->Throttle())), Teal);
	Text(P->ZoneName() + TEXT("  -  ") + P->NearName(), X + 18 * U, GY + 150 * U, .8f, FLinearColor::White);
	Text(TEXT("Sea: ") + SeaStates()[SeaIndex()].Name, X + 18 * U, GY - 26 * U, .85f, Dim);

	const FString Warn = P->Warning();
	if (!Warn.IsEmpty()) Text(Warn, X, GY - 56 * U, 1.0f, Red);
}

void AKeysHUD::DrawChart(AKeysBoatPawn* P)
{
	if (!Chart) return;
	const float Size = (P->bBigMap ? 760 : 250) * U;
	const float X = P->bBigMap ? Canvas->SizeX * .5f - Size * .5f : Canvas->SizeX - Size - 24 * U;
	const float Y = P->bBigMap ? Canvas->SizeY * .5f - Size * .5f : 24 * U;
	DrawRect(FLinearColor(0, 0, 0, .5f), X - 4 * U, Y - 4 * U, Size + 8 * U, Size + 8 * U);
	DrawTexture(Chart, X, Y, Size, Size, 0, 0, 1, 1, FLinearColor::White, BLEND_Opaque);
	// marinas
	for (const FMarina& M : Marinas())
	{
		const FVector2D A = ChartXY(M.SX, M.EZ, X, Y, Size);
		DrawRect(FLinearColor(.9f, .2f, .2f), A.X - 3 * U, A.Y - 3 * U, 6 * U, 6 * U);
		if (P->bBigMap) Text(M.Name, A.X + 6 * U, A.Y - 8 * U, .6f, FLinearColor(.3f, .05f, .05f));
	}
	// the boat, with a heading tick
	// rigs, weedlines and bird piles
	if (const AKeysLife* L = AKeysLife::Get(this))
	{
		TArray<FKeysMarker> Mk;
		L->GetMarkers(Mk);
		const double Pulse = .5 + .5 * FMath::Sin(GetWorld()->GetTimeSeconds() * 5);
		for (const FKeysMarker& K : Mk)
		{
			const FVector2D A = ChartXY(K.X, K.Z, X, Y, Size);
			if (K.Kind == 0)
			{
				DrawRect(FLinearColor(.05f, .05f, .07f), A.X - 4 * U, A.Y - 4 * U, 8 * U, 8 * U);
				DrawRect(FLinearColor(1.f, .75f, .2f), A.X - 1.5f * U, A.Y - 1.5f * U, 3 * U, 3 * U);
				if (P->bBigMap) Text(TEXT("RIG"), A.X + 6 * U, A.Y - 8 * U, .55f, FLinearColor(.1f, .1f, .1f));
			}
			else if (K.Kind == 1)
			{
				const double HL = K.Len * .8;
				const FVector2D A0 = ChartXY(K.X - FMath::Cos(K.Ang) * HL, K.Z - FMath::Sin(K.Ang) * HL, X, Y, Size);
				const FVector2D A1 = ChartXY(K.X + FMath::Cos(K.Ang) * HL, K.Z + FMath::Sin(K.Ang) * HL, X, Y, Size);
				DrawLine(A0.X, A0.Y, A1.X, A1.Y, FLinearColor(.78f, .6f, .2f), 2.5f * U);
			}
			else if (K.Kind == 3)
			{
				// the chart's wreck symbol: a hull outline with a cross through it
				const FVector2D Fw(-FMath::Sin(K.Ang), -FMath::Cos(K.Ang));
				const double HL = FMath::Max(K.Len * .5, 40.0);
				const FVector2D A0 = ChartXY(K.X - Fw.X * HL, K.Z - Fw.Y * HL, X, Y, Size), A1 = ChartXY(K.X + Fw.X * HL, K.Z + Fw.Y * HL, X, Y, Size);
				const FLinearColor WC(.12f, .1f, .18f);
				DrawLine(A0.X, A0.Y, A1.X, A1.Y, WC, 2.f * U);
				DrawLine(A.X - 4 * U, A.Y - 4 * U, A.X + 4 * U, A.Y + 4 * U, WC, 1.5f * U);
				DrawLine(A.X - 4 * U, A.Y + 4 * U, A.X + 4 * U, A.Y - 4 * U, WC, 1.5f * U);
				if (P->bBigMap) Text(TEXT("WK"), A.X + 6 * U, A.Y - 8 * U, .55f, FLinearColor(.1f, .1f, .1f));
			}
			else
			{
				const float R = (2.f + float(K.Frenzy * (1.5 + Pulse * 1.5))) * U;
				DrawRect(FLinearColor(.9f, .15f, .15f), A.X - R, A.Y - R, 2 * R, 2 * R);
			}
		}
	}
	// AI boats
	if (TActorIterator<AKeysTraffic> It(GetWorld()); It)
	{
		TArray<FVector2D> Tr, Rc;
		It->GetChartDots(Tr, Rc);
		for (const FVector2D& G : Tr) { const FVector2D A = ChartXY(G.X, G.Y, X, Y, Size); DrawRect(FLinearColor(.016f, .03f, .05f), A.X - 2 * U, A.Y - 2 * U, 4 * U, 4 * U); }
		for (const FVector2D& G : Rc) { const FVector2D A = ChartXY(G.X, G.Y, X, Y, Size); DrawRect(FLinearColor(.8f, .14f, .29f), A.X - 3.5f * U, A.Y - 3.5f * U, 7 * U, 7 * U); }
	}
	// the race marks (next one green), the AI field, and the tournament's finish
	if (const AKeysEvents* Ev = AKeysEvents::Get(this))
	{
		if (Ev->IsRacing())
		{
			const AKeysEvents::FRacer* Me = Ev->Me();
			const TArray<AKeysEvents::FMark>& Mk = Ev->Marks();
			for (int32 i = 0; i < Mk.Num(); i++)
			{
				const FVector2D A = ChartXY(Mk[i].X, Mk[i].Z, X, Y, Size), Bn = ChartXY(Mk[(i + 1) % Mk.Num()].X, Mk[(i + 1) % Mk.Num()].Z, X, Y, Size);
				DrawLine(A.X, A.Y, Bn.X, Bn.Y, FLinearColor(1.f, .45f, .1f, .5f), 1.5f * U);
			}
			for (int32 i = 0; i < Mk.Num(); i++)
			{
				const FVector2D A = ChartXY(Mk[i].X, Mk[i].Z, X, Y, Size);
				const bool bNext = Me && Me->Next == i && Me->FinishT < 0;
				const float R = (bNext ? 6.f : 4.f) * U;
				DrawRect(bNext ? FLinearColor(.1f, 1.f, .35f) : FLinearColor(1.f, .45f, .05f), A.X - R, A.Y - R, 2 * R, 2 * R);
			}
			TArray<FVector2D> Op;
			Ev->OpponentDots(Op);
			for (const FVector2D& G : Op) { const FVector2D A = ChartXY(G.X, G.Y, X, Y, Size); DrawRect(FLinearColor(.95f, .85f, .2f), A.X - 3.5f * U, A.Y - 3.5f * U, 7 * U, 7 * U); }
		}
		if (Ev->InTourney() && Ev->TourneyPhase() == AKeysEvents::ETourney::Live)
		{
			const FVector2D A = ChartXY(AKeysEvents::HarbourX(), AKeysEvents::HarbourZ(), X, Y, Size);
			const float R = (Ev->MyMask() == 31 ? 7.f : 4.5f) * U;
			DrawRect(FLinearColor(1.f, .82f, .2f), A.X - R, A.Y - R, 2 * R, 2 * R);
		}
	}
	DrawChartMore(P, X, Y, Size);
	// your crew: bright green
	if (const AKeysOnline* O = AKeysOnline::Get(this))
	{
		TArray<FVector2D> Fr;
		O->GetChartDots(Fr);
		for (const FVector2D& G : Fr) { const FVector2D A = ChartXY(G.X, G.Y, X, Y, Size); DrawRect(FLinearColor(.1f, .95f, .45f), A.X - 4.5f * U, A.Y - 4.5f * U, 9 * U, 9 * U); }
	}
	const FVector2D B = ChartXY(P->BoatX(), P->BoatZ(), X, Y, Size);
	const double Hh = P->Heading();
	const FVector2D Tip = B + FVector2D(-FMath::Sin(Hh), -FMath::Cos(Hh)) * 14.0 * U;
	DrawLine(B.X, B.Y, Tip.X, Tip.Y, FLinearColor(.95f, .1f, .1f), 3 * U);
	DrawRect(FLinearColor(.95f, .1f, .1f), B.X - 4 * U, B.Y - 4 * U, 8 * U, 8 * U);
	Text(TEXT("N"), X + Size * .5f, Y + 4 * U, .7f, FLinearColor(.1f, .15f, .25f), nullptr, true);
	if (!P->bBigMap) Text(TEXT("M - chart"), X + Size - 76 * U, Y + Size + 6 * U, .7f, Dim);
}

// the race and tournament boards (web game/race/index.ts renderHud, game/tournament/index.ts)
void AKeysHUD::DrawEvents(AKeysBoatPawn* P)
{
	const AKeysEvents* Ev = AKeysEvents::Get(this);
	if (!Ev) return;
	const float W = 340 * U, X = Canvas->SizeX - W - 20 * U;
	float Y = 300 * U;
	auto Fmt = [](double S) { return FString::Printf(TEXT("%d:%04.1f"), FMath::FloorToInt(S / 60), FMath::Fmod(S, 60.0)); };
	if (Ev->IsRacing())
	{
		TArray<const AKeysEvents::FRacer*> Ord;
		Ev->Standings(Ord);
		const AKeysEvents::FRacer* Me = Ev->Me();
		const float H = Ev->RacePhase() == AKeysEvents::ERace::Countdown ? 120 * U : 100 * U + Ord.Num() * 26 * U;
		Panel(X, Y, W, H, .6f);
		DrawRect(FLinearColor(1.f, .45f, .05f), X, Y, 5 * U, H);
		if (Ev->RacePhase() == AKeysEvents::ERace::Countdown)
		{
			Text(FString::Printf(TEXT("%d"), FMath::Max(1, FMath::CeilToInt(Ev->RaceClock()))), X + W * .5f, Y + 6 * U, 2.6f, Gold, GEngine->GetLargeFont(), true);
			Text(FString::Printf(TEXT("%d marks  -  %d laps  -  %.1f km"), Ev->Marks().Num(), AKeysEvents::RaceLaps, Ev->CourseKm()), X + W * .5f, Y + 84 * U, .85f, Dim, nullptr, true);
			return;
		}
		const int32 Place = Ord.IndexOfByKey(Me) + 1;
		Text(FString::Printf(TEXT("P%d/%d"), Place, Ord.Num()), X + 20 * U, Y + 10 * U, 1.5f, Gold);
		Text(FString::Printf(TEXT("Lap %d/%d"), FMath::Min(AKeysEvents::RaceLaps, Me ? Me->Lap + 1 : 1), AKeysEvents::RaceLaps), X + 130 * U, Y + 16 * U, 1.0f, FLinearColor::White);
		Text(Fmt(Ev->RaceClock()), X + W - 100 * U, Y + 16 * U, 1.0f, FLinearColor::White);
		if (Me && Me->FinishT < 0)
		{
			const AKeysEvents::FMark& N = Ev->Marks()[Me->Next];
			const double D = FMath::Sqrt(FMath::Square(N.X - P->BoatX()) + FMath::Square(N.Z - P->BoatZ()));
			Text(FString::Printf(TEXT("Next: %s  -  %s"), *N.Name, D > 1000 ? *FString::Printf(TEXT("%.1f km"), D / 1000) : *FString::Printf(TEXT("%d m"), FMath::RoundToInt(D))), X + 20 * U, Y + 56 * U, .9f, FLinearColor(.4f, 1.f, .55f));
		}
		else Text(TEXT("Finished - R to race again"), X + 20 * U, Y + 56 * U, .9f, Gold);
		float RY = Y + 92 * U;
		for (int32 i = 0; i < Ord.Num(); i++, RY += 26 * U)
		{
			const bool bMe = Ord[i] == Me;
			Text(FString::Printf(TEXT("%d  %s"), i + 1, *Ord[i]->Name), X + 20 * U, RY, .85f, bMe ? Gold : FLinearColor::White);
			Text(Ord[i]->FinishT >= 0 ? Fmt(Ord[i]->FinishT) : FString::Printf(TEXT("L%d"), Ord[i]->Lap + 1), X + W - 90 * U, RY, .85f, Dim);
		}
		return;
	}
	if (!Ev->InTourney()) return;
	const AKeysEvents::ETourney Ph = Ev->TourneyPhase();
	TArray<AKeysEvents::FEntry> St;
	Ev->TourneyStandings(St);
	const TArray<FString>& Sp = Ev->TourneySpecies();
	const float H = 104 * U + Sp.Num() * 25 * U + 12 * U + St.Num() * 25 * U;
	Panel(X, Y, W, H, .6f);
	DrawRect(Gold, X, Y, 5 * U, H);
	FString Top, Sub;
	if (Ph == AKeysEvents::ETourney::Countdown) { Top = FString::Printf(TEXT("Tournament - lines in %d"), FMath::CeilToInt(Ev->CountdownLeft())); Sub = TEXT("Keep one of each, first boat in wins"); }
	else if (Ph == AKeysEvents::ETourney::Live)
	{
		Top = FString::Printf(TEXT("Tournament  %d:%02d left"), FMath::FloorToInt(Ev->TimeLeft()) / 60, FMath::FloorToInt(Ev->TimeLeft()) % 60);
		if (Ev->MyMask() == (1 << Sp.Num()) - 1)
		{
			const double D = FMath::Sqrt(FMath::Square(AKeysEvents::HarbourX() - P->BoatX()) + FMath::Square(AKeysEvents::HarbourZ() - P->BoatZ()));
			Sub = FString::Printf(TEXT("All five! Run in: Boot Key Harbor %s%s"), D > 1000 ? *FString::Printf(TEXT("%.1f km"), D / 1000) : *FString::Printf(TEXT("%d m"), FMath::RoundToInt(D)),
				D < AKeysEvents::HarbourR ? TEXT(" - slow to tie up") : TEXT(""));
		}
		else
		{
			int32 N = 0;
			for (int32 k = 0; k < Sp.Num(); k++) N += (Ev->MyMask() >> k) & 1;
			Sub = FString::Printf(TEXT("%d/5 - keep one of each"), N);
		}
	}
	else if (Ph == AKeysEvents::ETourney::Ceremony) { Top = Ev->bWinnerMe ? FString(TEXT("You win!")) : Ev->WinnerName() + TEXT(" wins"); Sub = TEXT("The trophy on the pier"); }
	else { Top = Ev->WinnerName().IsEmpty() ? FString(TEXT("Horn - nobody brought all five in")) : (Ev->bWinnerMe ? FString(TEXT("You won")) : Ev->WinnerName() + TEXT(" won")); Sub = TEXT("Esc menu - Fishing tournament to fish another"); }
	Text(Top, X + 20 * U, Y + 10 * U, 1.15f, Gold);
	Text(Sub, X + 20 * U, Y + 50 * U, .8f, Ph == AKeysEvents::ETourney::Live && Ev->MyMask() == (1 << Sp.Num()) - 1 ? FLinearColor(.4f, 1.f, .55f) : Dim);
	float RY = Y + 86 * U;
	for (int32 k = 0; k < Sp.Num(); k++, RY += 25 * U)
	{
		const bool bGot = (Ev->MyMask() >> k) & 1;
		const KeysData::FSpecies* S = KeysData::Species(Sp[k]);
		DrawRect(bGot ? FLinearColor(.3f, .95f, .5f) : FLinearColor(.4f, .45f, .5f), X + 22 * U, RY + 6 * U, 10 * U, 10 * U);
		Text(S ? S->Name : Sp[k], X + 42 * U, RY, .85f, bGot ? FLinearColor(.6f, 1.f, .7f) : FLinearColor::White);
	}
	RY += 12 * U;
	for (const AKeysEvents::FEntry& E : St)
	{
		Text(E.Name, X + 22 * U, RY, .85f, E.bMe ? Gold : FLinearColor::White);
		Text(E.bWon ? FString(TEXT("TROPHY")) : E.bHome ? FString(TEXT("in")) : FString::Printf(TEXT("%d/5"), E.Caught), X + W - 90 * U, RY, .85f, E.bWon ? Gold : Dim);
		RY += 25 * U;
	}
}

void AKeysHUD::DrawFishing(AKeysBoatPawn* P)
{
	const float Cx = Canvas->SizeX * .5f, H = Canvas->SizeY;
	if (P->bCharging)
	{
		Panel(Cx - 180 * U, H - 170 * U, 360 * U, 60 * U);
		Text(FString::Printf(TEXT("Cast  %d m"), FMath::RoundToInt(12 + P->Charge * 73)), Cx, H - 166 * U, .9f, FLinearColor::White, nullptr, true);
		Bar(Cx - 160 * U, H - 134 * U, 320 * U, 12 * U, float(P->Charge), Gold);
	}
	if ((P->IsTrolling() || P->RigOut() > 0) && P->FState == EKeysFish::Idle && !P->bCharging)
	{
		// the trolling panel: the speed band the lures swim in, where you are in it, and how long the spread has soaked
		const float W = 420 * U, X = Cx - W * .5f, Y = H - 196 * U;
		Panel(X, Y, W, 96 * U, .55f);
		const double Q = P->TrollQualityNow();
		const FString Head = !P->IsTrolling() ? FString(TEXT("Lines in")) : P->TrollResuming() ? FString(TEXT("Clearing the lines")) : P->RigOut() < 1 ? FString(TEXT("Setting the spread")) : FString(TEXT("Trolling"));
		Text(FString::Printf(TEXT("%s  -  %.1f kn"), *Head, P->KnotsNow()), X + 18 * U, Y + 8 * U, 1.0f, Gold);
		const int32 Soak = FMath::FloorToInt(P->TrollSoak());
		Text(FString::Printf(TEXT("soak %d:%02d"), Soak / 60, Soak % 60), X + W - 120 * U, Y + 12 * U, .8f, Dim);
		// the band: 5 to 9 knots of lure speed, a marker where the boat is
		const float BX = X + 18 * U, BW = W - 36 * U, BY = Y + 48 * U;
		Bar(BX, BY, BW, 12 * U, 0, Teal);
		const double Kmin = 2.6 / 1.35 / .5144, Kmax = 4.6 / 1.35 / .5144, Klo = Kmin - 2, Khi = Kmax + 2;
		auto KX = [&](double K) { return BX + float(FMath::Clamp((K - Klo) / (Khi - Klo), 0.0, 1.0)) * BW; };
		DrawRect(FLinearColor(Teal.R, Teal.G, Teal.B, .35f + .5f * float(Q)), KX(Kmin), BY, KX(Kmax) - KX(Kmin), 12 * U);
		DrawRect(FLinearColor::White, KX(P->KnotsNow()) - 2 * U, BY - 5 * U, 4 * U, 22 * U);
		Text(FString::Printf(TEXT("lures swimming %d%%"), FMath::RoundToInt(Q * 100)), X + 18 * U, Y + 66 * U, .8f, Q > .5 ? Teal : Dim);
	}
	if (P->bStrikeShown && P->FState == EKeysFish::Bite)
		Text(TEXT("STRIKE!"), Cx, H * .36f, 3.0f, Gold, GEngine->GetLargeFont(), true);
	if (P->FState == EKeysFish::Fight)
	{
		const float W = 460 * U, X = Cx - W * .5f, Y = H - 250 * U;
		Panel(X, Y, W, 168 * U, .6f);
		Text(P->FightLabel, Cx, Y + 8 * U, 1.3f, Gold, nullptr, true);
		Text(P->FightSub, Cx, Y + 44 * U, .8f, Dim, nullptr, true);
		const float T = float(P->Tension);
		const FLinearColor TC = T > .82f ? Red : T < .08f ? FLinearColor(.6f, .6f, .65f) : Teal;
		Text(TEXT("Line tension"), X + 18 * U, Y + 76 * U, .8f, Dim);
		Bar(X + 140 * U, Y + 82 * U, W - 160 * U, 14 * U, T, TC);
		Text(TEXT("Fish stamina"), X + 18 * U, Y + 104 * U, .8f, Dim);
		Bar(X + 140 * U, Y + 110 * U, W - 160 * U, 10 * U, float(P->Stamina), Gold);
		FString Pips;
		for (int32 i = 1; i <= 5; i++) Pips += i <= P->Drag ? TEXT("#") : TEXT("-");
		Text(FString::Printf(TEXT("%d m out   -   drag %s"), FMath::RoundToInt(P->LineDist), *Pips), Cx, Y + 132 * U, .85f, FLinearColor::White, nullptr, true);
	}
}

// the diver's HUD (web entities/diver/hud.ts, entities/speargun/reload-hint.ts and fight-ui.ts)
void AKeysHUD::DrawDiving(AKeysBoatPawn* P, AKeysDiver* D)
{
	const float SW = Canvas->SizeX, SH = Canvas->SizeY, Cx = SW * .5f;
	const KeysDive::FState& S = D->State();
	// tunnel vision closing in as the air runs out, black on a blackout
	const float Tun = float(D->TunnelAmount());
	if (Tun > .01f)
	{
		if (!VignetteTex)
		{
			// a radial falloff: clear in the middle, black toward the edges
			constexpr int32 N = 128;
			VignetteTex = UTexture2D::CreateTransient(N, N, PF_B8G8R8A8);
			if (VignetteTex)
			{
				VignetteTex->SRGB = false;
				VignetteTex->Filter = TF_Bilinear;
				VignetteTex->AddressX = VignetteTex->AddressY = TA_Clamp;
				TArray<FColor> Px;
				Px.SetNumUninitialized(N * N);
				for (int32 j = 0; j < N; j++)
					for (int32 i = 0; i < N; i++)
					{
						const float R = FVector2D((i + .5f) / N - .5f, (j + .5f) / N - .5f).Size() * 2;
						const float A = FMath::SmoothStep(.35f, 1.15f, R);
						Px[j * N + i] = FColor(0, 0, 0, uint8(A * 255));
					}
				FTexture2DMipMap& Mip = VignetteTex->GetPlatformData()->Mips[0];
				FMemory::Memcpy(Mip.BulkData.Lock(LOCK_READ_WRITE), Px.GetData(), Px.Num() * sizeof(FColor));
				Mip.BulkData.Unlock();
				VignetteTex->UpdateResource();
			}
		}
		if (VignetteTex)
		{
			// closing in: scale the clear middle down as it gets worse
			const float Grow = 1.f + (1 - Tun) * .8f;
			const float W = SW * Grow, Hh = SH * Grow;
			DrawTexture(VignetteTex, (SW - W) * .5f, (SH - Hh) * .5f, W, Hh, 0, 0, 1, 1, FLinearColor(1, 1, 1, FMath::Min(1.f, Tun * 1.15f)));
			// whatever the stretched texture leaves uncovered at the edges stays dark too
			if (Grow > 1.f) {}
		}
		if (S.bBlackedOut) DrawRect(FLinearColor(0, 0, 0, FMath::Min(1.f, Tun)), 0, 0, SW, SH);
	}
	if (S.bBlackedOut) return;

	// reticle: a ring that turns gold when the aim assist has a fish
	if (!D->IsChaseView())
	{
		const FLinearColor RC = D->AimLocked() ? FLinearColor(Gold.R, Gold.G, Gold.B, .95f) : FLinearColor(1, 1, 1, .55f);
		const float R = (D->AimLocked() ? 16 : 12) * U;
		for (int32 k = 0; k < 24; k++)
		{
			const float A0 = k / 24.f * 2 * PI, A1 = (k + 1) / 24.f * 2 * PI;
			DrawLine(Cx + FMath::Cos(A0) * R, SH * .5f + FMath::Sin(A0) * R, Cx + FMath::Cos(A1) * R, SH * .5f + FMath::Sin(A1) * R, RC, 1.5f * U);
		}
		DrawRect(RC, Cx - 1.5f * U, SH * .5f - 1.5f * U, 3 * U, 3 * U);
	}

	// air and depth
	const float PX = 30 * U, PW = 330 * U, PH = 150 * U, PY = SH - PH - 30 * U;
	Panel(PX, PY, PW, PH, .55f);
	const float Af = float(D->AirFrac());
	const FLinearColor AC = Af < .15f ? Red : Af < .3f ? FLinearColor(1.f, .6f, .2f) : Teal;
	Text(TEXT("Air"), PX + 18 * U, PY + 12 * U, 1.0f, Dim);
	Text(FString::Printf(TEXT("%d%%"), FMath::RoundToInt(Af * 100)), PX + PW - 90 * U, PY + 12 * U, 1.1f, AC);
	Bar(PX + 18 * U, PY + 46 * U, PW - 36 * U, 14 * U, Af, AC);
	Text(TEXT("Depth"), PX + 18 * U, PY + 70 * U, 1.0f, Dim);
	Text(FString::Printf(TEXT("%.1f m"), S.Depth()), PX + PW - 110 * U, PY + 70 * U, 1.1f, FLinearColor::White);
	float Yw = PY + 104 * U;
	const FString Warn = D->AscentWarning();
	if (!Warn.IsEmpty())
	{
		const bool bCrit = Warn.StartsWith(TEXT("SURFACE"));
		const float Pulse = bCrit ? .6f + .4f * FMath::Sin(GetWorld()->GetRealTimeSeconds() * 8) : 1.f;
		Text(Warn, PX + 18 * U, Yw, .82f, bCrit ? FLinearColor(Red.R, Red.G, Red.B, Pulse) : FLinearColor(1.f, .7f, .3f));
		if (bCrit) Text(TEXT("SURFACE NOW"), Cx, SH * .2f, 2.2f, FLinearColor(Red.R, Red.G, Red.B, Pulse), GEngine->GetLargeFont(), true);
	}
	else if (S.bNarcosis) Text(TEXT("Narcosis - the deep plays tricks down here"), PX + 18 * U, Yw, .82f, FLinearColor(.8f, .6f, 1.f));
	if (D->CanReboard()) Text(TEXT("Press J to climb aboard"), Cx, SH - 120 * U, 1.2f, Gold, nullptr, true);

	// the reload prompt: brace, then press R to haul the bands back
	if (!D->IsLoaded() && !D->HasSpeared() && !D->ShotInFlight())
	{
		const float RX = 30 * U, RW = 300 * U, RY = PY - 74 * U;
		Panel(RX, RY, RW, 58 * U, D->NudgeFlash() > 0 ? .85f : .6f);
		if (D->NudgeFlash() > 0) { DrawRect(Gold, RX, RY, RW, 2 * U); DrawRect(Gold, RX, RY + 56 * U, RW, 2 * U); }
		const FString L = D->GunRecoverLeft() > 0 ? TEXT("Bracing the gun...") : D->IsDrawing() ? (D->DrawEffort() < .7 ? TEXT("Hold still - kicking costs you leverage") : TEXT("Hauling the bands...")) : TEXT("Press R - haul the bands back");
		Text(L, RX + 12 * U, RY + 8 * U, .85f, FLinearColor(.87f, .96f, .97f));
		Bar(RX + 12 * U, RY + 38 * U, RW - 24 * U, 6 * U, float(D->GunDraw()), FLinearColor(.95f, .76f, .31f));
	}

	// a fish on the spear: the same meter as a rod fight
	if (D->HasSpeared())
	{
		const KeysDive::FFight& F = D->Fight();
		const float W = 460 * U, X = Cx - W * .5f, Y = SH - 250 * U;
		Panel(X, Y, W, 150 * U, .6f);
		Text(D->FightLabel(), Cx, Y + 8 * U, 1.3f, Gold, nullptr, true);
		const FString Sub = Af < .15f ? TEXT("Air critical - surface now, even mid-fight.") : Af < .3f ? TEXT("Air low - think about surfacing with it on.") : TEXT("Hold S to haul it in - ease off if the bar runs red.");
		Text(Sub, Cx, Y + 44 * U, .8f, Dim, nullptr, true);
		const float T = float(F.Tension);
		const FLinearColor TC = T > .82f ? Red : T < .08f ? FLinearColor(.6f, .6f, .65f) : Teal;
		Text(TEXT("Line tension"), X + 18 * U, Y + 76 * U, .8f, Dim);
		Bar(X + 140 * U, Y + 82 * U, W - 160 * U, 14 * U, T, TC);
		Text(TEXT("Fish stamina"), X + 18 * U, Y + 104 * U, .8f, Dim);
		Bar(X + 140 * U, Y + 110 * U, W - 160 * U, 10 * U, float(F.Stam), Gold);
		Text(FString::Printf(TEXT("%d m"), FMath::RoundToInt(F.Dist)), Cx, Y + 124 * U, .85f, FLinearColor::White, nullptr, true);
	}
	else if (!P->Toast.IsEmpty() && GetWorld()->GetTimeSeconds() < P->ToastUntil) {}
	else Text(TEXT("Click fire  -  R reload  -  Space / C up / down  -  J climb aboard"), Cx, SH - 44 * U, .85f, Dim, nullptr, true);
}

void AKeysHUD::DrawCard(AKeysBoatPawn* P)
{
	const float W = 560 * U, X = Canvas->SizeX - W - 40 * U, Y = Canvas->SizeY * .24f;
	Panel(X, Y, W, 400 * U, .7f);
	float Yy = Y + 16 * U;
	Text(P->CardName, X + 24 * U, Yy, 1.6f, Gold, GEngine->GetLargeFont());
	Yy += 56 * U;
	Text(FString::Printf(TEXT("%.1f lb  -  %d in  -  %s"), P->CardWeight, P->CardInches, *P->CardSex), X + 24 * U, Yy, 1.15f);
	Yy += 36 * U;
	Text(FString::Printf(TEXT("+%s points"), *FText::AsNumber(P->CardPts).ToString()), X + 24 * U, Yy, 1.05f, Teal);
	Yy += 32 * U;
	Text(P->CardZone, X + 24 * U, Yy, .8f, Dim);
	Yy += 30 * U;
	if (!P->CardBonus.IsEmpty()) { Text(P->CardBonus, X + 24 * U, Yy, .95f, Gold); Yy += 30 * U; }
	Yy += Wrapped(P->CardFact, X + 24 * U, Yy, W - 48 * U, .85f) + 10 * U;
	Yy += Wrapped(P->CardNote, X + 24 * U, Yy, W - 48 * U, .8f, P->bCanKeep ? Dim : Red) + 10 * U;
	Text(P->bCanKeep ? TEXT("K  keep it on ice      Space  release (+25%)") : TEXT("Space  release (+25%)"), X + 24 * U, Y + 400 * U - 40 * U, .95f, FLinearColor::White);
}

// ================================================================ electronics: chartplotter and fish finder (JS ELEC)
void AKeysHUD::UpdateElectronics(AKeysBoatPawn* P, double Now)
{
	const double Dt = LastNow < 0 ? 0 : FMath::Clamp(Now - LastNow, 0.0, .25);
	LastNow = Now;
	TrailT -= Dt;
	if (TrailT <= 0)
	{
		TrailT = 1;
		Trail.Add(FVector2D(P->BoatX(), P->BoatZ()));
		if (Trail.Num() > 240) Trail.RemoveAt(0);
	}
	// the sounder pings all the time so the history is there when you look at it
	SonT -= Dt;
	if (SonT <= 0) { SonT = .08; PushSonarColumn(P); }
	if (P->bElectronics)
	{
		GpsT -= Dt;
		const double Range = P->ChartRange;
		const double Drift = FMath::Max(FMath::Abs(P->BoatX() - GpsCX), FMath::Abs(P->BoatZ() - GpsCZ));
		if (GpsT <= 0 || GpsRange != Range || Drift > Range * (GpsMargin - 1) * .5) { GpsT = .25; RenderGps(P); }
	}
}

void AKeysHUD::PushSonarColumn(AKeysBoatPawn* P)
{
	const double Bx = P->BoatX(), Bz = P->BoatZ();
	const double Dep = FMath::Max(.3, P->DepthHere());
	const double Want = FMath::Max(5.0, FMath::CeilToDouble(Dep * 1.35 / 5) * 5);
	if (FMath::Abs(Want - SonMax) > SonMax * .25) SonMax = Want;

	FKeysSonarCol Col;
	Col.Bottom = float(Dep);
	for (int32 k = 0; k < 6; k++) Col.Marks.Add({float(FMath::FRand() * Dep), 1.f, HudHex(0x3c6ebe, FMath::FRand() * .25f)});

	// the live fish schools from the living sea, like the JS
	const AKeysLife* Life = AKeysLife::Get(this);
	if (Life)
	{
		TArray<FVector> FP;
		TArray<double> FL;
		Life->FishNear(Bx, Bz, 24, FP, FL);
		for (int32 i = 0; i < FP.Num(); i++) Contacts.Add({FP[i].X, FP[i].Z, FMath::Max(.3, -FP[i].Y), FL[i]});
		SonarFish.Reset();
	}
	// without it, a few made-up fish hang around the boat (more on the humps, bigger offshore)
	for (int32 i = SonarFish.Num() - 1; i >= 0; i--)
		if (HudHyp(SonarFish[i].X - Bx, SonarFish[i].Z - Bz) > 60 || SonarFish[i].Depth > DepthAt(SonarFish[i].X, SonarFish[i].Z)) SonarFish.RemoveAtSwap(i);
	const int32 WantFish = Life || Dep < 1.2 ? 0 : NearHump(Bx, Bz) ? 12 : 5;
	if (SonarFish.Num() < WantFish && FMath::FRand() < .2)
	{
		const double A = FMath::FRand() * 2 * PI, R = FMath::FRandRange(8.0, 50.0);
		const double FXp = Bx + FMath::Cos(A) * R, FZp = Bz + FMath::Sin(A) * R, D = DepthAt(FXp, FZp);
		if (D > 1.2)
		{
			const double Fd = FMath::FRand() < .6 ? D * FMath::FRandRange(.6, .92) : D * FMath::FRandRange(.15, .6);
			SonarFish.Add({FXp, FZp, Fd, FMath::FRandRange(.5, 1.6) * (1 + OffshoreF(FXp, FZp))});
		}
	}
	// fish under and beside the boat: slant range makes them draw as arches as you pass over
	auto AddFish = [&](const FKeysSonarFish& F)
	{
		const double HD = HudHyp(F.X - Bx, F.Z - Bz);
		if (HD > 22) return;
		const double Sl = HudHyp(FMath::Max(0.0, F.Depth), HD * .55);
		if (Sl > Dep + .5) return;
		const double Sz = FMath::Clamp(F.Size, .4, 4.0);
		Col.Marks.Add({float(Sl), float(FMath::Max(2.0, Sz * 1.8)), Sz > 1.4 ? HudHex(0xff3a2a) : Sz > .8 ? HudHex(0xffb02a) : HudHex(0xf2e04a)});
	};
	for (const FKeysSonarFish& F : SonarFish) AddFish(F);
	for (const FKeysSonarFish& F : Contacts) AddFish(F);
	// the fish you're on (or the one circling the bait) shows as a big red mark
	if (P->HookedSpecies() && P->IsLineOut())
	{
		const double HD = HudHyp(P->FishX() - Bx, P->FishZ() - Bz);
		if (HD < 30) Col.Marks.Add({float(FMath::Min(Dep, HudHyp(.8 + P->FishDeep(), HD * .5))), 6.f, HudHex(0xff2a2a)});
	}
	SonarCols.Add(MoveTemp(Col));
	if (SonarCols.Num() > NumSonarCols) SonarCols.RemoveAt(0);
}

void AKeysHUD::RenderGps(AKeysBoatPawn* P)
{
	static const FColor BandC[6] = {FColor(111, 167, 214), FColor(156, 199, 234), FColor(201, 226, 245), FColor(232, 243, 251), FColor(247, 250, 253), FColor(255, 255, 255)};
	const double Range = P->ChartRange;
	GpsCX = P->BoatX();
	GpsCZ = P->BoatZ();
	GpsRange = Range;
	const double Mpp = Range * 2 * GpsMargin / GpsW;   // meters per texel
	TArray<int8> Bands;
	Bands.SetNumUninitialized(GpsW * GpsH);
	for (int32 j = 0; j < GpsH; j++)
		for (int32 i = 0; i < GpsW; i++)
			Bands[j * GpsW + i] = int8(HudBand(DepthFast(GpsCX + (i - GpsW * .5 + .5) * Mpp, GpsCZ + (j - GpsH * .5 + .5) * Mpp)));
	GpsPx.SetNumUninitialized(GpsW * GpsH);
	for (int32 j = 0; j < GpsH; j++)
		for (int32 i = 0; i < GpsW; i++)
		{
			const int32 K = j * GpsW + i, B = Bands[K];
			FColor C;
			if (B < 0) C = FColor(214, 196, 140);
			else
			{
				C = BandC[B];
				const int32 Rt = i < GpsW - 1 ? Bands[K + 1] : B, Dn = j < GpsH - 1 ? Bands[K + GpsW] : B;
				if ((Rt != B && Rt >= 0) || (Dn != B && Dn >= 0)) C = FColor(70, 110, 150);   // depth contour
			}
			GpsPx[K] = C;
		}
	if (!GpsTex)
	{
		GpsTex = UTexture2D::CreateTransient(GpsW, GpsH, PF_B8G8R8A8);
		if (!GpsTex) return;
		GpsTex->SRGB = true;
		GpsTex->Filter = TF_Bilinear;
		GpsTex->AddressX = TA_Clamp;
		GpsTex->AddressY = TA_Clamp;
		GpsTex->UpdateResource();
	}
	// hand the render thread its own copy of the pixels; it frees them when the upload is done
	const int32 Bytes = GpsW * GpsH * int32(sizeof(FColor));
	uint8* Copy = new uint8[Bytes];
	FMemory::Memcpy(Copy, GpsPx.GetData(), Bytes);
	FUpdateTextureRegion2D* Region = new FUpdateTextureRegion2D(0, 0, 0, 0, GpsW, GpsH);
	GpsTex->UpdateTextureRegions(0, 1, Region, uint32(GpsW * 4), 4u, Copy,
		[](uint8* Data, const FUpdateTextureRegion2D* Regions) { delete[] Data; delete Regions; });
}

void AKeysHUD::DrawElectronics(AKeysBoatPawn* P)
{
	// one glass-helm display, like the JS MFD: wide chart on the left, sonar down the right
	const float W = 640 * U, H = 256 * U, X = Canvas->SizeX - W - 24 * U, Y = Canvas->SizeY - H - 70 * U;
	DrawRect(HudHex(0x0b0c0e), X - 5 * U, Y - 5 * U, W + 10 * U, H + 10 * U);
	DrawGps(P, X, Y, 448 * U, H);
	DrawSonar(P, X + 452 * U, Y, 188 * U, H);
	DrawRect(HudHex(0x0a141e, .82f), X + 452 * U, Y, 188 * U, 18 * U);
	Text(TEXT("SONAR  200 kHz"), X + 458 * U, Y + 1 * U, .6f, FLinearColor::White);
}

void AKeysHUD::DrawGps(AKeysBoatPawn* P, float X, float Y, float W, float H)
{
	const double Range = P->ChartRange, Bx = P->BoatX(), Bz = P->BoatZ();
	const double Mpp = Range * 2 / W;   // meters per screen pixel
	auto Pt = [&](double GX, double GZ) { return FVector2D(X + W * .5 + (GX - Bx) / Mpp, Y + H * .5 + (GZ - Bz) / Mpp); };
	auto Inside = [&](const FVector2D& A) { return A.X > X && A.X < X + W && A.Y > Y && A.Y < Y + H; };
	DrawRect(HudHex(0x6fa7d6), X, Y, W, H);
	if (GpsTex && GpsRange > 0)
	{
		// the window around the boat, inside the slightly bigger area the texture covers
		const double TexHW = GpsRange * GpsMargin, TexHH = TexHW * GpsH / GpsW;
		const double WinHW = Range, WinHH = Range * H / W;
		const float UW = float(WinHW / TexHW), VH = float(FMath::Min(1.0, WinHH / TexHH));
		const float U0 = FMath::Clamp(float((Bx - WinHW - (GpsCX - TexHW)) / (2 * TexHW)), 0.f, FMath::Max(0.f, 1 - UW));
		const float V0 = FMath::Clamp(float((Bz - WinHH - (GpsCZ - TexHH)) / (2 * TexHH)), 0.f, FMath::Max(0.f, 1 - VH));
		DrawTexture(GpsTex, X, Y, W, H, U0, V0, UW, VH, FLinearColor::White, BLEND_Opaque);
	}
	// track
	for (int32 i = 1; i < Trail.Num(); i++)
	{
		const FVector2D A = Pt(Trail[i - 1].X, Trail[i - 1].Y), B = Pt(Trail[i].X, Trail[i].Y);
		if (Inside(A) && Inside(B)) DrawLine(A.X, A.Y, B.X, B.Y, FLinearColor(.78f, .16f, .16f, .85f), 1.5f * U);
	}
	if (Trail.Num())
	{
		const FVector2D A = Pt(Trail.Last().X, Trail.Last().Y), B = Pt(Bx, Bz);
		if (Inside(A)) DrawLine(A.X, A.Y, B.X, B.Y, FLinearColor(.78f, .16f, .16f, .85f), 1.5f * U);
	}
	// marinas
	for (const FMarina& M : Marinas())
	{
		const FVector2D A = Pt(M.SX, M.EZ);
		if (!Inside(A)) continue;
		DrawRect(HudHex(0x1f4f8c), A.X - 4 * U, A.Y - 4 * U, 8 * U, 8 * U);
		DrawRect(FLinearColor::White, A.X - 1 * U, A.Y - 3 * U, 2 * U, 6 * U);
	}
	// own boat: heading line and arrowhead
	{
		const double Hh = P->Heading(), Cs = FMath::Cos(-Hh), Sn = FMath::Sin(-Hh);
		const FVector2D C(X + W * .5f, Y + H * .5f);
		auto R = [&](double PX, double PY) { return C + FVector2D(PX * Cs - PY * Sn, PX * Sn + PY * Cs) * double(U); };
		const FVector2D L = R(0, -60), A = R(0, -9), B = R(6, 7), M = R(0, 3), D = R(-6, 7);
		const FLinearColor Ink = HudHex(0x111111);
		DrawLine(C.X, C.Y, L.X, L.Y, Ink, 1 * U);
		DrawLine(A.X, A.Y, B.X, B.Y, Ink, 2.5f * U);
		DrawLine(B.X, B.Y, M.X, M.Y, Ink, 2.5f * U);
		DrawLine(M.X, M.Y, D.X, D.Y, Ink, 2.5f * U);
		DrawLine(D.X, D.Y, A.X, A.Y, Ink, 2.5f * U);
	}
	// data bars
	const double Kn = P->KnotsNow(), Dep = DepthAt(Bx, Bz) * 3.28;
	const int32 Hdg = (FMath::RoundToInt(-P->Heading() * 180 / PI) % 360 + 360) % 360;
	DrawRect(HudHex(0x0a141e, .82f), X, Y, W, 22 * U);
	DrawRect(HudHex(0x0a141e, .82f), X, Y + H - 18 * U, W, 18 * U);
	Text(FString::Printf(TEXT("SOG %.1f kn"), Kn), X + 6 * U, Y + 2 * U, .62f);
	Text(FString::Printf(TEXT("COG %03d"), Hdg) + FString::Chr(0x00B0), X + W * .375f, Y + 2 * U, .62f);
	Text(FString::Printf(TEXT("%.1f ft"), Dep), X + W - 76 * U, Y + 2 * U, .62f);
	Text(HudLatLon(Bx, Bz), X + 6 * U, Y + H - 17 * U, .52f);
	Text(FString::Printf(TEXT("%.2f nm"), Range * 2 * .00054 * 2.5), X + W - 66 * U, Y + H - 17 * U, .52f);
	Text(TEXT("N"), X + 8 * U, Y + 26 * U, .6f, HudHex(0x1f4f8c));
}

void AKeysHUD::DrawSonar(AKeysBoatPawn* P, float X, float Y, float W, float H)
{
	const float K = H / 192.f, Top = 44 * U, Ah = H - Top, Sy = float(Ah / SonMax), Cw = W / NumSonarCols;
	const float Bot = Y + H;
	DrawRect(HudHex(0x04122a), X, Y, W, H);
	const int32 N = SonarCols.Num();
	for (int32 i = 0; i < N; i++)
	{
		const FKeysSonarCol& Col = SonarCols[i];
		const float CX = X + W - (N - i) * Cw;
		for (const FKeysSonarMark& Mk : Col.Marks)
		{
			const float My = Y + Top + Mk.Depth * Sy, Mh = FMath::Max(1.f, Mk.Px * K);
			if (My - Mh * .5f < Bot) DrawRect(Mk.Color, CX, My - Mh * .5f, Cw + .5f, FMath::Min(Mh, Bot - (My - Mh * .5f)));
		}
		// the bottom return: a bright yellow edge fading to dark red
		const float By = Y + Top + Col.Bottom * Sy - 1;
		if (By >= Bot) continue;
		const float Hs[4] = {4 * K, 6 * K, 6 * K, 9999.f};
		const FLinearColor Cs[4] = {HudHex(0xffc83a), HudHex(0xff4a2a), HudHex(0x8a2a20), HudHex(0x2a1410)};
		float Yy = By;
		for (int32 b = 0; b < 4 && Yy < Bot; b++)
		{
			const float Hh = FMath::Min(Hs[b], Bot - Yy);
			DrawRect(Cs[b], CX, Yy, Cw + .5f, Hh);
			Yy += Hh;
		}
	}
	// header and depth scale
	const double Dep = P->DepthHere();
	DrawRect(HudHex(0x071a33), X, Y, W, Top);
	Text(FString::Printf(TEXT("%.1f ft"), Dep * 3.28), X + 6 * U, Y + 19 * U, .7f);
	Text(FString::Printf(TEXT("%.1f"), Dep > 40 ? 80.6 : 78.4) + FString::Chr(0x00B0) + TEXT("F"), X + W - 54 * U, Y + 22 * U, .55f, Dim);
	for (int32 k = 1; k <= 4; k++)
	{
		const float Ty = Y + Top + Ah * k / 4;
		DrawRect(FLinearColor(1, 1, 1, .25f), X + W - 14 * U, Ty - (k == 4 ? 2 * U : 0), 10 * U, 1 * U);
		const FString L = FString::Printf(TEXT("%d"), int32(SonMax * 3.28 * k / 4));
		float TW = 0, TH = 0;
		GetTextSize(L, TW, TH, GEngine->GetMediumFont(), .5f * U);
		Text(L, X + W - 16 * U - TW, Ty - TH - (k == 4 ? 2 * U : 0), .5f, FLinearColor(1, 1, 1, .7f));
	}
}

// ================================================================ leaderboard (local; JS renderLB)
void AKeysHUD::DrawBoard(AKeysBoatPawn* P)
{
	const UKeysSave* Sv = P->Board();
	const float W = 900 * U, H = 820 * U, X = Canvas->SizeX * .5f - W * .5f, Y = Canvas->SizeY * .5f - H * .5f;
	DrawRect(FLinearColor(0, 0, 0, .35f), 0, 0, Canvas->SizeX, Canvas->SizeY);
	Panel(X, Y, W, H, .88f);
	Text(TEXT("Leaderboard"), X + 32 * U, Y + 18 * U, 1.3f, Gold, GEngine->GetLargeFont());
	const bool bBig = P->BoardTab == 0;
	const AKeysOnline* Onl = AKeysOnline::Get(this);
	const bool bCrewTab = Onl && Onl->IsOnline();
	const TCHAR* TabNames[3] = {TEXT("Biggest fish"), TEXT("Species records"), TEXT("Crew")};
	{
		// tabs and a close button, all clickable
		const int32 NT = bCrewTab ? 3 : 2;
		const float TW = 190 * U, TH = 44 * U, TY = Y + 24 * U;
		float TX = X + W - 32 * U - 56 * U - 12 * U - NT * (TW + 8 * U);
		for (int32 i = 0; i < NT; i++, TX += TW + 8 * U)
		{
			Button(FName(*FString::Printf(TEXT("tab:%d"), i)), TX, TY, TW, TH, i == P->BoardTab, 1);
			Text(TabNames[i], TX + TW * .5f, TY + 9 * U, .9f, i == P->BoardTab ? Gold : FLinearColor::White, nullptr, true);
		}
		Button(TEXT("board:close"), X + W - 32 * U - 56 * U, TY, 56 * U, TH, false, 1);
		Text(TEXT("X"), X + W - 32 * U - 28 * U, TY + 8 * U, 1.0f, FLinearColor::White, nullptr, true);
	}
	float Yy = Y + 92 * U;
	if (P->BoardTab == 2)
	{
		DrawCrewBoard(X + 32 * U, Yy, W - 64 * U);
		Text(TEXT("Shared with your crew online.   Tab or A / D switch tabs  -  B closes"), X + W * .5f, Y + H - 40 * U, .8f, Dim, nullptr, true);
		return;
	}
	if (!Sv)
	{
		Text(TEXT("No save slot available."), X + 32 * U, Yy, 1.f, Dim);
		return;
	}
	const TArray<FString>& Keys = KeysData::SpeciesKeys();
	auto SpName = [](const FString& K) { const KeysData::FSpecies* S = KeysData::Species(K); return S ? S->Name : K; };
	if (bBig)
	{
		if (!Sv->BestKey.IsEmpty())
			Text(FString::Printf(TEXT("Your biggest: %s, %.1f lb  -  %d fish total  -  best score %s"), *SpName(Sv->BestKey), Sv->BestWeight, Sv->Count, *FText::AsNumber(Sv->BestScore).ToString()),
				X + 32 * U, Yy, .9f, Teal);
		Yy += 44 * U;
		if (!Sv->TopCatches.Num())
			Text(TEXT("No fish yet - land one to get on the board"), X + 32 * U, Yy, 1.f, Dim);
		for (int32 i = 0; i < Sv->TopCatches.Num(); i++)
		{
			const FKeysBoardCatch& C = Sv->TopCatches[i];
			const FLinearColor Col = i == 0 ? Gold : FLinearColor::White;
			Text(FString::Printf(TEXT("%d. %s"), i + 1, *SpName(C.Key)), X + 32 * U, Yy, 1.f, Col);
			Text(FString::Printf(TEXT("%.1f lb"), C.Weight), X + 400 * U, Yy, 1.f, Col);
			Text(C.Boat + TEXT("   ") + C.Date, X + 530 * U, Yy + 4 * U, .75f, Dim);
			Yy += 44 * U;
		}
	}
	else
	{
		int32 Mine = 0;
		for (const FString& K : Keys) if (Sv->SpeciesBest.Contains(K)) Mine++;
		Text(FString::Printf(TEXT("You have caught %d of %d species."), Mine, Keys.Num()), X + 32 * U, Yy, .9f, Teal);
		Yy += 40 * U;
		const int32 PerCol = (Keys.Num() + 1) / 2;
		for (int32 i = 0; i < Keys.Num(); i++)
		{
			const float CX = X + 32 * U + (i / PerCol) * (W * .5f - 16 * U), CY = Yy + (i % PerCol) * 31 * U;
			const double* Best = Sv->SpeciesBest.Find(Keys[i]);
			const FLinearColor Col = Best ? Gold : Dim;
			Text(SpName(Keys[i]), CX, CY, .8f, Col);
			Text(Best ? FString::Printf(TEXT("%.1f lb"), *Best) : FString(TEXT("-")), CX + 300 * U, CY, .8f, Col);
		}
	}
	Text(TEXT("Saved on this computer.   Tab or A / D switch tabs  -  B closes"), X + W * .5f, Y + H - 40 * U, .8f, Dim, nullptr, true);
}

void AKeysHUD::DrawExtras(AKeysBoatPawn* P)
{
	const float W = Canvas->SizeX, H = Canvas->SizeY;
	if (AKeysAudio* Au = AKeysAudio::Get(this))
	{
		const FString R = Au->IsSoundOn() ? Au->GetRadioLabel() : FString(TEXT("Sound off (N)"));
		Text(R + TEXT("   [P]"), 24 * U, H - 40 * U, .75f, FLinearColor(1, 1, 1, .8f));
	}
	AKeysExtras* E = nullptr;
	if (TActorIterator<AKeysExtras> It(GetWorld()); It) E = *It;
	if (!E) return;

	// Luigi haze: a warm wash over the screen with a darker rim
	const double Haze = E->GetHazeAlpha();
	if (Haze > .01)
	{
		DrawRect(FLinearColor(1.f, .84f, .59f, float(Haze) * .22f), 0, 0, W, H);
		const float Rim = 90 * U;
		const FLinearColor Edge(.47f, .35f, .24f, float(Haze) * .35f);
		DrawRect(Edge, 0, 0, W, Rim); DrawRect(Edge, 0, H - Rim, W, Rim);
		DrawRect(Edge, 0, Rim, Rim, H - 2 * Rim); DrawRect(Edge, W - Rim, Rim, Rim, H - 2 * Rim);
	}

	// labels over the buddy boat and Caelen
	APlayerController* PC = GetOwningPlayerController();
	FVector CamLoc; FRotator CamRot;
	if (PC) PC->GetPlayerViewPoint(CamLoc, CamRot);
	TArray<FKeysExtrasTag> Tags;
	E->GetTags(Tags);
	for (const FKeysExtrasTag& Tg : Tags)
	{
		if (!PC) break;
		const FVector To = Tg.WorldPos - CamLoc;
		if (FVector::DotProduct(To, CamRot.Vector()) <= 0) continue;
		const FVector S = Project(Tg.WorldPos);
		if (S.Z <= 0) continue;
		float TW = 0, TH = 0;
		GetTextSize(Tg.Text, TW, TH, GEngine->GetSmallFont(), .9f * U);
		Panel(S.X - TW * .5f - 8 * U, S.Y - TH - 14 * U, TW + 16 * U, TH + 6 * U, .45f);
		Text(Tg.Text, S.X, S.Y - TH - 11 * U, .9f, FLinearColor::White, GEngine->GetSmallFont(), true);
	}

	// LUIGI MODE banner and hype callouts
	float A = 0, Sc = 1, Rot = 0;
	if (E->GetLuigiBanner(A, Sc, Rot))
	{
		const FLinearColor BannerGold(1.f, .78f, .25f, A * .6f);
		const float Y = H * .28f;
		for (int32 i = 0; i < 4; i++)
		{
			const float Ox = (i & 1 ? 3.f : -3.f) * U, Oy = (i & 2 ? 3.f : -3.f) * U;
			Text(TEXT("LUIGI MODE"), W * .5f + Ox, Y + Oy, 4.f * Sc, BannerGold, GEngine->GetLargeFont(), true);
		}
		Text(TEXT("LUIGI MODE"), W * .5f, Y, 4.f * Sc, FLinearColor(1, 1, 1, A), GEngine->GetLargeFont(), true);
		Text(TEXT("NECKKKK FUCK"), W * .5f, Y + 90 * U * Sc, 1.6f * Sc, FLinearColor(1.f, .92f, .65f, A), GEngine->GetLargeFont(), true);
	}
	const FString Call = E->GetCallout();
	if (!Call.IsEmpty())
		Text(Call, W * .5f, H * .4f, 2.2f, FLinearColor(1.f, .95f, .75f), GEngine->GetLargeFont(), true);
}

// ================================================================ ashore: where you are, what F does here, the counter panel
void AKeysHUD::DrawAshore(AKeysBoatPawn* P, AKeysWalker* W)
{
	const float SW = Canvas->SizeX, SH = Canvas->SizeY;
	// top left: the place and the wallet
	Panel(24 * U, 24 * U, 420 * U, 92 * U, .5f);
	Text(W->PlaceName(), 40 * U, 32 * U, 1.15f, Gold);
	Text(FString::Printf(TEXT("$%s   -   boat %d m %s"), *FText::AsNumber(P->Cash).ToString(), FMath::RoundToInt(W->BoardDistance()), *W->BoatBearing()), 40 * U, 72 * U, .95f, Dim);
	// the prompt for whatever is in reach
	if (!W->bShopOpen && !W->Prompt().IsEmpty())
	{
		float TW = 0, TH = 0;
		GetTextSize(W->Prompt(), TW, TH, GEngine->GetMediumFont(), 1.2f * U);
		Panel(SW * .5f - TW * .5f - 20 * U, SH * .64f, TW + 40 * U, TH + 18 * U, .6f);
		Text(W->Prompt(), SW * .5f, SH * .64f + 9 * U, 1.2f, FLinearColor::White, nullptr, true);
	}
	if (!W->bShopOpen)
	{
		Text(TEXT("WASD + mouse walk - Shift run - F use - 1 view - M chart - Q back aboard"), SW * .5f, SH - 44 * U, .95f, Dim, nullptr, true);
		return;
	}
	// the counter: name, greeting, the shelf, what you can afford
	const KeysTown::FVenue* V = KeysTown::Venue(W->ShopVenue);
	if (!V) return;
	const int32 N = V->Sells.Num();
	// a long shelf (the gift shop) goes compact: one line a row, the selected item's blurb at the foot
	const bool bCompact = N > 7;
	const float RH = (bCompact ? 38 : 62) * U;
	const float PW = 760 * U, X = SW * .5f - PW * .5f, Y = bCompact ? SH * .06f : SH * .18f;
	const float PH = 190 * U + N * RH + (bCompact ? 100 : 60) * U;
	Panel(X, Y, PW, PH, .88f);
	Text(V->Name, X + 28 * U, Y + 18 * U, 1.35f, Gold, GEngine->GetLargeFont());
	Text(V->Tagline, X + 28 * U, Y + 70 * U, .9f, Teal);
	Wrapped(FString::Printf(TEXT("\"%s\""), *W->ShopGreeting), X + 28 * U, Y + 104 * U, PW - 56 * U, .9f, Dim);
	float RY = Y + 170 * U;
	for (int32 i = 0; i < N; i++, RY += RH)
	{
		const KeysTown::FShelfItem& It = V->Sells[i];
		const bool bSel = i == W->ShopSel;
		const bool bOwned = P->Owned.Contains(It.Effect);
		const FString Slot = FString(It.Effect).StartsWith(TEXT("cosmetic.")) ? FString(It.Effect).Mid(9).Left(FString(It.Effect).Mid(9).Find(TEXT("."))) : FString();
		const bool bWorn = !Slot.IsEmpty() && P->Wearing.FindRef(Slot) == It.Effect;
		if (bSel) DrawRect(FLinearColor(.2f, .5f, .55f, .35f), X + 16 * U, RY - 4 * U, PW - 32 * U, RH - 4 * U);
		Text(FString::Printf(TEXT("%d  %s"), i + 1, It.Name), X + 28 * U, RY, 1.0f, bOwned && !bWorn ? Dim : FLinearColor::White);
		if (!bCompact) Text(It.Blurb, X + 58 * U, RY + 28 * U, .78f, Dim);
		const int32 Cnt = P->CountOf(It.Effect);
		const FString Price = bWorn ? FString(TEXT("wearing")) : bOwned ? FString(Slot.IsEmpty() ? TEXT("owned") : TEXT("owned - wear it")) : It.Price == 0 ? FString(TEXT("sit down")) : FString::Printf(TEXT("$%s%s"), *FText::AsNumber(It.Price).ToString(), Cnt > 0 ? *FString::Printf(TEXT("   (%d on hand)"), Cnt) : TEXT(""));
		float TW = 0, TH = 0;
		GetTextSize(Price, TW, TH, GEngine->GetMediumFont(), 1.0f * U);
		Text(Price, X + PW - 32 * U - TW, RY, 1.0f, bOwned ? (bWorn ? Teal : Dim) : P->Cash >= It.Price ? Gold : Red);
	}
	if (bCompact && V->Sells.IsValidIndex(W->ShopSel)) Wrapped(V->Sells[W->ShopSel].Blurb, X + 28 * U, RY + 6 * U, PW - 56 * U, .85f, Dim);
	Text(FString::Printf(TEXT("You have $%s.   Up / Down + Enter or 1-%d to buy   -   F or Esc to step away"), *FText::AsNumber(P->Cash).ToString(), FMath::Min(9, N)), X + 28 * U, Y + PH - 44 * U, .85f, Dim);
}
