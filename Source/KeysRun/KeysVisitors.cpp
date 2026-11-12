#include "KeysVisitors.h"
#include "KeysGeo.h"
#include "KeysSea.h"
#include "KeysMesh.h"
#include "KeysData.h"
#include "KeysWorld.h"
#include "KeysBoatPawn.h"
#include "ProceduralMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Camera/PlayerCameraManager.h"
#include "Engine/World.h"
#include "EngineUtils.h"

using namespace KeysGeo;

namespace
{
	FLinearColor Hx(uint32 C) { return FLinearColor(FColor(uint8((C >> 16) & 255), uint8((C >> 8) & 255), uint8(C & 255))); }
	double Smooth(double A, double B, double X) { const double T = FMath::Clamp((X - A) / (B - A), 0.0, 1.0); return T * T * (3 - 2 * T); }
	double AngLerp(double A, double B, double T) { const double D = FMath::Fmod(FMath::Fmod(B - A + PI, 2 * PI) + 2 * PI, 2 * PI) - PI; return A + D * FMath::Min(1.0, T); }
	// web heading (fwd = (-sin h, -cos h) in x, z) to an Unreal yaw in degrees
	double YawOf(double H) { return FMath::RadiansToDegrees(FMath::Atan2(-FMath::Cos(H), -FMath::Sin(H))); }
	double HeadingTo(double DX, double DZ) { return FMath::Atan2(-DX, -DZ); }
	FString Usd(double N)
	{
		FString S = FString::Printf(TEXT("%lld"), int64(FMath::RoundToDouble(N)));
		for (int32 i = S.Len() - 3; i > 0; i -= 3) S.InsertAt(i, TEXT(","));
		return TEXT("$") + S;
	}

	// ---- shared/world/pirates.ts
	constexpr double PeriodS = 30 * 60, StartLo = .4, StartHi = .6, JoinGrace = 45;
	constexpr int32 SaltStart = 9101, SaltSea = 9102, SaltSpread = 9103, SaltLines = 9110;
	constexpr uint32 Seed = 20240817;   // WORLD_SEED
	// pirate-nav.ts
	constexpr double SailSpeed = 6.5, SpawnDist = 1050, HailDist = 44, MinDepth = 1.6, Look = 46, LostD = 2600, ApproachMax = 330;
	const TCHAR* Captain = TEXT("Cap'n Barnacle Bev");
	const TCHAR* Mate = TEXT("Mr. Mildew");

	bool ClearWater(double X, double Z) { return DepthAt(X, Z) >= MinDepth && LandH(X, Z) < -.3; }

	struct FLoot { double Cash, CoolerLb, Cap; int32 Count; FString Boat; };

	FString Line(const TCHAR* Kind, double U, const FLoot& L)
	{
		auto Pick = [U](const TArray<FString>& Xs) { return Xs[FMath::Min(Xs.Num() - 1, int32(U * Xs.Num()))]; };
		const FString Lb = FString::Printf(TEXT("%d"), FMath::RoundToInt(L.CoolerLb));
		if (FCString::Strcmp(Kind, TEXT("hail")) == 0)
			return Pick({TEXT("AHOY, fibreglass! Fine-looking boat! Ye got any loot aboard?"), L.Boat.Len() ? FString::Printf(TEXT("Ahoy, %s! Any loot to declare?"), *L.Boat) : FString(TEXT("Ahoy, shiny boat! Any loot to declare?")),
				TEXT("Heave to, ye gleaming land-ship! We come in peace! And for loot!"), TEXT("AHOY! Yer boat has an engine! Ours has a bucket! Any loot?"), TEXT("Ahoy, friend! Quick question: loot?")});
		if (FCString::Strcmp(Kind, TEXT("ask")) == 0)
			return Pick({TEXT("Doubloons? Gold? Gift cards? Cap'n would settle for a sandwich."), TEXT("He's asking nicely. It's the new policy. HR made us."),
				TEXT("Any treasure, jewels, or unopened Capri-Suns?"), TEXT("Cap'n says look in the cooler. Cap'n always says look in the cooler.")});
		if (FCString::Strcmp(Kind, TEXT("quip")) == 0)
			return Pick({TEXT("Fine. We'll just be over here. Being historically accurate."), TEXT("We tried robbing a Contender once. It left. It left very fast."),
				TEXT("No robbing today. Mr. Mildew says the paperwork is a nightmare."), TEXT("Do not tell the Coast Guard we were here. We are, uh, not insured."), TEXT("Legally we are required to say we are reenactors.")});
		if (FCString::Strcmp(Kind, TEXT("farewell")) == 0)
			return Pick({TEXT("Fair winds! We'll be back in half an hour. Have the loot ready!"), TEXT("Off we go! Come back with loot or come back with snacks!"),
				TEXT("Keep yer cooler cold and yer tales tall - see ye in thirty minutes!"), TEXT("Back around the clock in thirty minutes. We're very punctual for pirates.")});
		// the verdict: appraiseLoot
		const double Fill = L.Cap > 0 ? L.CoolerLb / L.Cap : 0;
		if (L.Count > 0 && Fill >= .6)
			return Pick({FString::Printf(TEXT("%s lb of fish?! That cooler's heavier than our conscience!"), *Lb), TEXT("Blimey, that's a haul. We'll not touch it. Pirates' code, article 7: respect the fisherman."),
				FString::Printf(TEXT("%s lb! Ye're the real pirate here. We just have a flag."), *Lb), TEXT("That cooler is stuffed! Ye've been robbing the ocean and we're MAD at the competition.")});
		if (L.Cash >= 25000)
			return Pick({FString::Printf(TEXT("%s?! Ye're LOADED! Can we borrow ten? We'll pay ye back in exposure."), *Usd(L.Cash)),
				FString::Printf(TEXT("%s and ye're sailing %s? Cap'n, we're in the wrong business."), *Usd(L.Cash), L.Boat.Len() ? *(TEXT("a ") + L.Boat) : TEXT("that")),
				FString::Printf(TEXT("%s! Ye could buy our whole fleet. Admittedly, it's one boat and it leaks."), *Usd(L.Cash))});
		if (L.Count > 0)
			return Pick({FString::Printf(TEXT("%d fish, %s lb on ice. That's not loot, that's dinner!"), L.Count, *Lb), FString::Printf(TEXT("%s lb of fish. Is that loot? The parrot says it's loot."), *Lb),
				TEXT("A cooler with actual fish! We accept payment in snapper."), FString::Printf(TEXT("%d fish? We'll not take them. We have no ice. And no refrigeration. And no plan."), L.Count)});
		if (L.Cash >= 100)
			return Pick({FString::Printf(TEXT("%s? That's not loot, that's a tip jar."), *Usd(L.Cash)), FString::Printf(TEXT("Ye've got %s and an empty cooler. Respect. Nothing, but respect."), *Usd(L.Cash)),
				FString::Printf(TEXT("%s! We'll leave it. The parrot would charge more to listen to ye."), *Usd(L.Cash))});
		return Pick({TEXT("Not a doubloon! Not a fish! Even the seagull has more than ye."), TEXT("Empty cooler, empty pockets. Ye're the poorest boat we've not robbed all week."),
			TEXT("We checked. It's just vibes aboard. Disgraceful."), TEXT("Nothing! Ye're sailing a boat for fun and have nothing to show for it!")});
	}

	const TCHAR* Asides[] = {TEXT("Does anyone have a sandwich?"), TEXT("Our boat has no engine. Ye can stop asking."), TEXT("Yo ho ho. And a bottle of sparkling water. Doctor's orders."),
		TEXT("Smile for the camera, ye land-lubbers!"), TEXT("We've been sailing since 1722. The boat is on its first coat of paint."), TEXT("Mind the sails. They're mostly patches. Holy patches."),
		TEXT("Is that a Yamaha? We hate it. We love it."), TEXT("The Keys are lovely this time of year. Every time of year. We're lost."),
		TEXT("Mr. Mildew, is this still the Caribbean? - It says Marathon. - Close enough."), TEXT("Arrr. Arrr. Wait, that's not right either.")};

	// banner-plane.ts: each run advertises one of the town's businesses with its own banner line (content/venues.ts)
	const TCHAR* Banners[] = {TEXT("HOOK LINE & SINKER - BLAME THE LURE"), TEXT("DEADEYE GUNS - THE FISH STARTED IT"), TEXT("RUSTY ANCHOR - COLD BEER HOT LIES"),
		TEXT("TIPSY TARPON - TWO FOR ONE ALL DAY"), TEXT("REEL HAMMERED - CHEAP BEER NO JUDGEMENT"), TEXT("PINK FLAMINGO - THE TARPON DANCE TOO"),
		TEXT("GAFF & GILL - LOOSER THAN YOUR DRAG"), TEXT("SALTY DOG - WE COOK WHAT YOU MISSED"), TEXT("SCUBA STEVE - MEET THE ONE THAT GOT AWAY"),
		TEXT("CHEAP BEER & LOTTO - COLDER THAN THE BITE"), TEXT("CONCH REPUBLIC - PROOF YOU WENT FISHING"), TEXT("KEYS PAWN - WE BUY THE ROD YOU SNAPPED"),
		TEXT("SUDS & SUN - GETS THE FISH SMELL OUT"), TEXT("CONE ZONE - KEY LIME BEATS A SKUNKED DAY"), TEXT("SUNSET LODGE - SLEEP IT OFF THE FISH KEEP"),
		TEXT("STOP-N-GO - LIVE BAIT LIVELINESS VARIES")};
	constexpr double BannerAltMin = 95, BannerAltMax = 150, BannerSpMin = 26, BannerSpMax = 32, RunRadius = 1500, GapMin = 95, GapMax = 240;
	constexpr double TowGap = 14, LetterGap = 3.1, LetterH = 5.2;
}

AKeysVisitors::AKeysVisitors()
{
	PrimaryActorTick.bCanEverTick = true;
	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent = Root;
	auto Make = [this](const TCHAR* Name, USceneComponent* Parent)
	{
		UProceduralMeshComponent* C = CreateDefaultSubobject<UProceduralMeshComponent>(Name);
		C->SetupAttachment(Parent);
		C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		return C;
	};
	Ship = Make(TEXT("PirateShip"), Root);
	Plane = Make(TEXT("BannerPlane"), Root);
	Banner = Make(TEXT("Banner"), Plane);
	auto Txt = [this](const TCHAR* Name)
	{
		UTextRenderComponent* T = CreateDefaultSubobject<UTextRenderComponent>(Name);
		T->SetupAttachment(Banner);
		T->SetHorizontalAlignment(EHTA_Center);
		T->SetVerticalAlignment(EVRTA_TextCenter);
		T->SetTextRenderColor(FColor(200, 30, 30));
		T->SetWorldSize(LetterH * 100 * .78);
		T->SetCastShadow(false);
		return T;
	};
	BannerText = Txt(TEXT("BannerText"));
	BannerTextBack = Txt(TEXT("BannerTextBack"));
}

AKeysVisitors* AKeysVisitors::Get(const UObject* WorldContext)
{
	UWorld* W = WorldContext ? WorldContext->GetWorld() : nullptr;
	if (!W) return nullptr;
	TActorIterator<AKeysVisitors> It(W);
	return It ? *It : nullptr;
}

void AKeysVisitors::BeginPlay()
{
	Super::BeginPlay();
	SetActorLocation(FVector::ZeroVector);
	UMaterialInterface* VC = AKeysWorld::VertexColorMaterial();
	// ---- the sloop (pirate-ship.ts): the same hull loft as the fibreglass boats fed the opposite numbers - a deep keel,
	// steep deadrise, a full round bow, a hard sheer spring and a long raked stem - in dark planking with a gold stripe
	{
		FKeysMesh M;
		KeysMesh::FHullDef H;
		H.F = 1.5; H.Spring = .95; H.YK = 1.3; H.Dr0 = 26; H.Dr1 = 56; H.Rake = 2.6; H.Entry = .5;
		H.Bottom = Hx(0x2a2018); H.Boot = Hx(0x1a1712); H.Hull = Hx(0x4a301c); H.Cove = Hx(0xc8a24a); H.bCove = true; H.Rub = Hx(0x2e1e10);
		H.Cap = Hx(0x6a4a2a); H.Liner = Hx(0x5a4030); H.Deck = Hx(0x8a6a44);
		const double L = 13, B = 4.1;
		KeysMesh::BuildHullLoft(M, H, L, B, true);
		const FLinearColor Wood = Hx(0x5a3a22), Spar = Hx(0x6e4e30), Canvas = Hx(0xd9cdb0), Patch = Hx(0xb8a888), Black = Hx(0x111111), White = Hx(0xeeeeee);
		const double Deck = 1.2, MX = L * .12, MastTop = 14;
		M.AddTube(FVector(MX, 0, Deck), FVector(MX, 0, MastTop), .2, .13, 8, Spar);
		M.AddTube(FVector(L * .5 + .6, 0, 2.3), FVector(L * .5 + 4.2, 0, 3.4), .12, .07, 6, Spar);              // bowsprit
		M.AddTube(FVector(MX - .2, 0, Deck + 1.6), FVector(-L * .42, 0, Deck + 1.9), .11, .09, 6, Spar);      // boom
		M.AddTube(FVector(MX - .2, 0, MastTop - 1.2), FVector(-L * .18, 0, MastTop + 1.3), .09, .07, 6, Spar); // gaff
		// the mainsail: four-sided between the boom and the gaff, patched; the jib forward
		const FVector A(MX - .25, 0, Deck + 1.7), Bb(-L * .41, 0, Deck + 2.0), C(-L * .17, 0, MastTop + 1.2), D(MX - .25, 0, MastTop - 1.2);
		M.AddQuad(A, Bb, C, D, Canvas, FVector(0, 1, 0));
		M.AddQuad(A, D, C, Bb, Canvas, FVector(0, -1, 0));
		for (int32 k = 0; k < 3; k++)
		{
			const FVector P0 = FMath::Lerp(FMath::Lerp(A, Bb, .2 + .25 * k), FMath::Lerp(D, C, .2 + .25 * k), .3 + .15 * k);
			for (int32 s = -1; s <= 1; s += 2) M.AddBox(P0 + FVector(0, s * .03, 0), FVector(1.1, .01, .9), Patch);
		}
		const FVector J0(MX + .1, 0, MastTop - 2), J1(L * .5 + 4.0, 0, 3.5), J2(MX + .6, 0, Deck + 1.2);
		M.AddTri(J0, J1, J2, Canvas, FVector(0, 1, 0));
		M.AddTri(J0, J2, J1, Canvas, FVector(0, -1, 0));
		// the Jolly Roger at the masthead
		M.AddQuad(FVector(MX, 0, MastTop + .1), FVector(MX - 2, 0, MastTop + .1), FVector(MX - 2, 0, MastTop - 1.2), FVector(MX, 0, MastTop - 1.2), Black, FVector(0, 1, 0));
		M.AddQuad(FVector(MX, 0, MastTop + .1), FVector(MX, 0, MastTop - 1.2), FVector(MX - 2, 0, MastTop - 1.2), FVector(MX - 2, 0, MastTop + .1), Black, FVector(0, -1, 0));
		for (int32 s = -1; s <= 1; s += 2) M.AddBox(FVector(MX - 1, s * .02, MastTop - .5), FVector(.5, .01, .5), White);
		// a cannon a side, the wheel aft, a barrel and the two of them
		for (int32 s = -1; s <= 1; s += 2) M.AddTube(FVector(-L * .05, s * (B * .5 - .5), Deck + .55), FVector(-L * .05, s * (B * .5 + .4), Deck + .6), .14, .12, 8, Black);
		M.AddTube(FVector(-L * .3, -.2, Deck + .8), FVector(-L * .3, .2, Deck + .8), .45, .45, 10, Wood);
		M.AddTube(FVector(L * .25, 0, Deck), FVector(L * .25, 0, Deck + .8), .35, .35, 8, Wood);
		auto Person = [&M](const FVector& At, const FLinearColor& Coat, bool bHat)
		{
			M.AddBox(At + FVector(0, 0, .45), FVector(.24, .36, .9), Hx(0x2b2b33));
			M.AddBox(At + FVector(0, 0, 1.2), FVector(.28, .46, .62), Coat);
			M.AddBox(At + FVector(0, 0, 1.68), FVector(.24, .22, .26), Hx(0xd8a888));
			if (bHat) { M.AddBox(At + FVector(0, 0, 1.85), FVector(.5, .55, .08), Hx(0x151515)); M.AddBox(At + FVector(0, 0, 1.95), FVector(.28, .3, .16), Hx(0x151515)); }
			else M.AddBox(At + FVector(0, 0, 1.84), FVector(.27, .24, .1), Hx(0xb02020));
		};
		Person(FVector(-L * .3 - .7, 0, Deck), Hx(0x9a1a1a), true);   // Cap'n Bev at the wheel
		Person(FVector(-L * .02, 1.0, Deck), Hx(0xe8e8e8), false);     // Mr. Mildew at the rail
		M.SmoothNormals(40);
		M.Commit(Ship, 0, VC);
		Ship->SetVisibility(false);
	}
	// ---- the banner plane: a high-wing tail-dragger, and the banner on its spar
	{
		FKeysMesh M;
		const FLinearColor Body = Hx(0xf2f2ee), Trim = Hx(0xc8102e), Dark = Hx(0x22262b);
		M.AddTube(FVector(-4.5, 0, 0), FVector(2.2, 0, 0), .45, .7, 10, Body);
		M.AddTube(FVector(2.2, 0, 0), FVector(3.2, 0, -.05), .7, .45, 10, Trim);
		M.AddBox(FVector(.8, 0, .85), FVector(1.6, 11, .14), Body);   // the high wing
		M.AddBox(FVector(1.6, 0, .55), FVector(.9, .9, .5), Dark);   // windscreen
		M.AddBox(FVector(-4.2, 0, .9), FVector(1.1, .1, 1.6), Trim);  // fin
		M.AddBox(FVector(-4.2, 0, .15), FVector(.9, 3.6, .1), Body);  // tailplane
		M.AddTube(FVector(3.25, 0, -.05), FVector(3.35, 0, -.05), .9, .9, 12, Dark * .6f);   // prop disc
		for (int32 s = -1; s <= 1; s += 2) M.AddTube(FVector(1.3, s * .9, -.6), FVector(1.3, s * 1.0, -1.2), .05, .05, 5, Dark);
		M.Commit(Plane, 0, VC);
		Plane->SetVisibility(false, true);
	}
	Gap = 40 + FMath::FRand() * 60;
	// a visit already under way when you load is one you missed (pirateVisitDue's grace)
	Handled = int64(double(FDateTime::UtcNow().ToUnixTimestamp()) / PeriodS) - 1;
}

void AKeysVisitors::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	const double Dt = FMath::Min(double(DeltaSeconds), .1), T = GetWorld()->GetTimeSeconds();
	TickPirates(Dt, T);
	TickBanner(Dt, T);
}

// ================================================================ pirates
void AKeysVisitors::SummonPirates(double Dist) { BeginPirates(int64(double(FDateTime::UtcNow().ToUnixTimestamp()) / PeriodS), Dist); }

bool AKeysVisitors::BeginPirates(int64 Bucket, double Dist)
{
	AKeysBoatPawn* P = Cast<AKeysBoatPawn>(UGameplayStatics::GetPlayerPawn(this, 0));
	if (!P) return false;
	const bool bGulf = KeysSea::HashCell(Seed, int32(Bucket), 0, SaltSea) < .5;
	const double Spread = KeysSea::HashCell(Seed, int32(Bucket), 0, SaltSpread) * 2 - 1;
	// pirate-nav.ts homeBearing / placeSpawn: out of their sea, fanning round the compass until the water is clear
	const double Base = bGulf ? -PI / 2 + Spread * .5 : (Spread < 0 ? PI - (.12 + FMath::Abs(Spread) * .1) : .12 + FMath::Abs(Spread) * .1);
	bool bFound = false;
	for (double F : {1.0, .8, .62, .48, .34})
	{
		for (int32 k = 0; k <= 29 && !bFound; k++)
		{
			const double A = Base + (k % 2 ? 1 : -1) * FMath::CeilToDouble(k / 2.0) * (PI / 15);
			const double X = P->BoatX() + FMath::Cos(A) * Dist * F, Z = P->BoatZ() + FMath::Sin(A) * Dist * F;
			if (!ClearWater(X, Z)) continue;
			bool bLine = true;
			for (double U = .1; U < .92 && bLine; U += .05) bLine = ClearWater(FMath::Lerp(X, P->BoatX(), U), FMath::Lerp(Z, P->BoatZ(), U));
			if (!bLine) continue;
			PX = X; PZ = Z; bFound = true;
		}
		if (bFound) break;
	}
	if (!bFound) return false;
	PH = HeadingTo(P->BoatX() - PX, P->BoatZ() - PZ);
	PSpeed = SailSpeed;
	Phase = 1; PhaseT = 0; NextBeat = 0;
	Handled = Bucket;
	// the script (pirateScript): hail, ask, the verdict on your loot, a quip, two asides, farewell
	FLoot L;
	L.Cash = double(P->Cash); L.CoolerLb = P->CoolerLb(); L.Count = P->Cooler.Num(); L.Cap = P->Spec().CoolerCap; L.Boat = P->Spec().Brand + TEXT(" ") + P->Spec().Name;
	auto U = [Bucket](int32 i) { return KeysSea::HashCell(Seed, int32(Bucket), i, SaltLines); };
	Beats.Reset();
	Beats.Add({0, true, Line(TEXT("hail"), U(0), L)});
	Beats.Add({5.5, false, Line(TEXT("ask"), U(1), L)});
	Beats.Add({11, true, Line(TEXT("verdict"), U(2), L)});
	Beats.Add({17.5, false, Line(TEXT("quip"), U(3), L)});
	const int32 NA = UE_ARRAY_COUNT(Asides), I1 = FMath::Min(NA - 1, int32(U(4) * NA)), I2 = (I1 + 1 + int32(U(5) * (NA - 1))) % NA;
	Beats.Add({24, true, Asides[I1]});
	Beats.Add({30.5, false, Asides[I2]});
	Beats.Add({37, true, Line(TEXT("farewell"), U(6), L)});
	P->ShowMsg(TEXT("A ship's bell, somewhere out on the water..."), 4.2);
	UE_LOG(LogTemp, Log, TEXT("Keys Run: pirates from %.0f, %.0f (%s), %.0f m out"), PX, PZ, bGulf ? TEXT("gulf") : TEXT("channel"), FMath::Sqrt(FMath::Square(PX - P->BoatX()) + FMath::Square(PZ - P->BoatZ())));
	Ship->SetVisibility(true);
	return true;
}

void AKeysVisitors::TickPirates(double Dt, double T)
{
	AKeysBoatPawn* P = Cast<AKeysBoatPawn>(UGameplayStatics::GetPlayerPawn(this, 0));
	if (!P) return;
	if (Phase == 0)
	{
		// pirateVisitDue: once per half-hour bucket, at a hashed moment in its middle fifth, on the wall clock
		const double Now = double(FDateTime::UtcNow().ToUnixTimestamp());
		const int64 Bucket = int64(Now / PeriodS);
		if (Bucket <= Handled) return;
		const double Start = Bucket * PeriodS + (StartLo + (StartHi - StartLo) * KeysSea::HashCell(Seed, int32(Bucket), 0, SaltStart)) * PeriodS;
		if (Now < Start) return;
		if (Now - Start > JoinGrace || P->IsAshore() || P->IsDiving() || !BeginPirates(Bucket, SpawnDist)) Handled = Bucket;
		return;
	}
	PhaseT += Dt;
	const double DX = P->BoatX() - PX, DZ = P->BoatZ() - PZ, D = FMath::Max(1.0, FMath::Sqrt(DX * DX + DZ * DZ));
	double WantH = PH, WantSp = SailSpeed;
	bool bBeam = false;
	if (Phase == 3)
	{
		// out to open water, re-planned every few seconds, then gone
		EscapeT -= Dt;
		if (EscapeT <= 0)
		{
			EscapeT = 4;
			double Best = -1e9;
			for (int32 k = 0; k < 16; k++)
			{
				const double H = k * PI / 8, FX = -FMath::Sin(H), FZ = -FMath::Cos(H);
				double Score = (FX * -DX + FZ * -DZ) / D * 100;
				for (double R : {60.0, 160.0, 320.0}) Score += ClearWater(PX + FX * R, PZ + FZ * R) ? DepthAt(PX + FX * R, PZ + FZ * R) : -500;
				if (Score > Best) { Best = Score; EscapeH = H; }
			}
		}
		WantH = EscapeH;
		WantSp = SailSpeed * Smooth(0, 7, PhaseT);
		if (PhaseT > 120 || D > LostD) { Phase = 0; Ship->SetVisibility(false); return; }
	}
	else
	{
		// heave to HailDist out on the side they came in on, easing off over the last stretch
		const double TX = P->BoatX() - DX / D * HailDist, TZ = P->BoatZ() - DZ / D * HailDist;
		WantH = HeadingTo(TX - PX, TZ - PZ);
		WantSp = SailSpeed * Smooth(HailDist - 1, HailDist + 120, D);
		bBeam = D < HailDist + 16;
		if (Phase == 1 && (D > LostD || PhaseT > ApproachMax)) { Phase = 3; PhaseT = 0; EscapeT = 0; }
		if (Phase == 1 && D < 150) { Phase = 2; PhaseT = 0; }   // HAIL_AT: they start talking, still sailing in
		if (Phase == 2)
		{
			while (NextBeat < Beats.Num() && PhaseT >= Beats[NextBeat].At)
			{
				const FBeat& Bt = Beats[NextBeat++];
				P->ShowMsg(FString::Printf(TEXT("%s: %s"), Bt.bCaptain ? Captain : Mate, *Bt.Text), 4.8);
			}
			if (NextBeat >= Beats.Num() && PhaseT > Beats.Last().At + 5) { Phase = 3; PhaseT = 0; EscapeT = 0; }
		}
	}
	if (bBeam) WantH = FMath::Atan2(-DZ, DX);   // starboard to the player, where the mate is waving from
	else
	{
		// steerHeading: the direct heading if the next stretch is wet, else the nearest one that is
		bool bOk = false;
		for (double Off : {0.0, .3, -.3, .6, -.6, .95, -.95, 1.4, -1.4, 1.9, -1.9, double(PI)})
		{
			const double H = WantH + Off, FX = -FMath::Sin(H), FZ = -FMath::Cos(H);
			if (ClearWater(PX + FX * Look, PZ + FZ * Look) && ClearWater(PX + FX * Look * .5, PZ + FZ * Look * .5)) { WantH = H; bOk = true; break; }
		}
		if (!bOk) WantSp = 0;
	}
	PH = AngLerp(PH, WantH, Dt * (bBeam ? .5 : .9));
	PSpeed += (WantSp - PSpeed) * FMath::Min(1.0, Dt * .6);
	const double FX = -FMath::Sin(PH), FZ = -FMath::Cos(PH);
	double NX = PX + FX * PSpeed * Dt, NZ = PZ + FZ * PSpeed * Dt;
	if (D < 16) { NX -= DX / D * (16 - D) * Dt * 1.2; NZ -= DZ / D * (16 - D) * Dt * 1.2; }
	if (ClearWater(NX, NZ)) { PX = NX; PZ = NZ; } else PSpeed = 0;
	// ride the real waves: bow / stern for pitch, port / starboard for roll, heeled by the wind on the sail
	const double Amp = AmpAt(PX, PZ), HL = 13 * .45, HB = 2.0, RX = FMath::Cos(PH), RZ = -FMath::Sin(PH);
	const double Hb = WaveH(PX + FX * HL, PZ + FZ * HL, T, Amp), Hs = WaveH(PX - FX * HL, PZ - FZ * HL, T, Amp);
	const double Hp = WaveH(PX - RX * HB, PZ - RZ * HB, T, Amp), Hst = WaveH(PX + RX * HB, PZ + RZ * HB, T, Amp);
	const double Heel = .1 * (.35 + .65 * PSpeed / SailSpeed) * FMath::Sin(PH + .4);
	Ship->SetWorldLocationAndRotation(ToUE(PX, (Hb + Hs) / 2 - .2, PZ),
		FRotator(FMath::RadiansToDegrees(FMath::Atan2(Hb - Hs, 13.0)), YawOf(PH), FMath::RadiansToDegrees(FMath::Atan2(Hst - Hp, 4.0) * .7 + Heel)));
	if (PSpeed > 1) EmitWake(PX, PZ, PSpeed, 13, T, Dt);
}

// ================================================================ banner plane
void AKeysVisitors::StartBanner() { Gap = 0; bPass = false; }

void AKeysVisitors::TickBanner(double Dt, double T)
{
	AKeysBoatPawn* P = Cast<AKeysBoatPawn>(UGameplayStatics::GetPlayerPawn(this, 0));
	if (!P) return;
	if (!bPass)
	{
		Gap -= Dt;
		if (Gap > 0) return;
		// a run that passes the boat rather than at it: start to one side, aim across, offset so you watch it go by
		const double Bearing = FMath::FRand() * 2 * PI, Off = FMath::FRandRange(180.0, 520.0) * (FMath::FRand() < .5 ? -1 : 1);
		BH = Bearing + PI / 2;
		BX = P->BoatX() + FMath::Cos(Bearing) * RunRadius - FMath::Sin(BH) * Off;
		BZ = P->BoatZ() + FMath::Sin(Bearing) * RunRadius - FMath::Cos(BH) * Off;
		BSp = FMath::FRandRange(BannerSpMin, BannerSpMax);
		BAlt = FMath::FRandRange(BannerAltMin, BannerAltMax);
		Flown = 0; BLen = RunRadius * 2.2;
		bPass = true;
		const FString Txt = Banners[FMath::RandRange(0, UE_ARRAY_COUNT(Banners) - 1)];
		// the cloth: a white strip with a red hem, long enough for the letters, streaming back from a tow line
		const double Span = TowGap + Txt.Len() * LetterGap;
		FKeysMesh M;
		const FLinearColor Cloth(.97f, .97f, .95f), Hem(.78f, .06f, .12f);
		const int32 NSeg = FMath::Max(4, int32((Span - TowGap) / 2));
		for (int32 i = 0; i < NSeg; i++)
		{
			const double X0 = -TowGap - (Span - TowGap) * i / NSeg, X1 = -TowGap - (Span - TowGap) * (i + 1) / NSeg;
			const double Z0 = -1.5 - .02 * i, Z1 = -1.5 - .02 * (i + 1);
			M.AddQuad(FVector(X0, 0, Z0 - LetterH / 2), FVector(X1, 0, Z1 - LetterH / 2), FVector(X1, 0, Z1 + LetterH / 2), FVector(X0, 0, Z0 + LetterH / 2), Cloth, FVector(0, 1, 0));
			M.AddQuad(FVector(X0, 0, Z0 - LetterH / 2), FVector(X0, 0, Z0 + LetterH / 2), FVector(X1, 0, Z1 + LetterH / 2), FVector(X1, 0, Z1 - LetterH / 2), Cloth, FVector(0, -1, 0));
			for (int32 e = -1; e <= 1; e += 2) M.AddBox(FVector((X0 + X1) / 2, 0, (Z0 + Z1) / 2 + e * (LetterH / 2 - .15)), FVector(FMath::Abs(X1 - X0), .04, .3), Hem);
		}
		M.AddTube(FVector(-4.4, 0, 0), FVector(-TowGap, 0, -1.5), .03, .03, 4, FLinearColor(.2f, .2f, .2f));
		M.AddTube(FVector(-TowGap, 0, -1.5 - LetterH / 2), FVector(-TowGap, 0, -1.5 + LetterH / 2), .06, .06, 5, FLinearColor(.3f, .3f, .3f));
		M.Commit(Banner, 0, AKeysWorld::VertexColorMaterial());
		const double MidX = -(TowGap + Span) / 2 * 100;
		BannerText->SetText(FText::FromString(Txt));
		BannerTextBack->SetText(FText::FromString(Txt));
		BannerText->SetRelativeLocationAndRotation(FVector(MidX, 8, -150 - .02 * NSeg * 50), FRotator(0, 90, 0));
		BannerTextBack->SetRelativeLocationAndRotation(FVector(MidX, -8, -150 - .02 * NSeg * 50), FRotator(0, -90, 0));
		Plane->SetVisibility(true, true);
	}
	const double FX = -FMath::Sin(BH), FZ = -FMath::Cos(BH);
	BX += FX * BSp * Dt; BZ += FZ * BSp * Dt;
	Flown += BSp * Dt;
	if (Flown > BLen)
	{
		bPass = false;
		Gap = FMath::FRandRange(GapMin, GapMax);
		Plane->SetVisibility(false, true);
		return;
	}
	// a hand-flown slog: it wanders a little and never sits still
	const double Bank = FMath::Sin(T * .21 + BAlt) * .09, Y = BAlt + FMath::Sin(T * .17 + BX * .001) * 2.2;
	Plane->SetWorldLocationAndRotation(ToUE(BX, Y, BZ), FRotator(0, YawOf(BH), FMath::RadiansToDegrees(Bank)));
	// the banner sags and ripples behind it
	Banner->SetRelativeRotation(FRotator(FMath::Sin(T * 1.3) * 1.5, FMath::Sin(T * .9) * 2.0, FMath::Sin(T * 1.7) * 4.0));
}
