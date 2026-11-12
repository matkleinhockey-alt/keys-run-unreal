#include "KeysLocals.h"
#include "KeysKit.h"
#include "KeysDog.h"
#include "KeysGeo.h"
#include "KeysSea.h"
#include "KeysMesh.h"
#include "KeysData.h"
#include "KeysWorld.h"
#include "KeysAudio.h"
#include "KeysBoatPawn.h"
#include "ProceduralMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Camera/PlayerCameraManager.h"
#include "Engine/World.h"
#include "EngineUtils.h"

using namespace KeysGeo;
using namespace KeysKit;

namespace
{
	constexpr uint32 Seed = 20240817;   // WORLD_SEED
	constexpr int32 SaltSandbar = 181, SaltLarryOut = 182, SaltLarryRoute = 183, SaltHouseRot = 113;
	// larry.ts
	constexpr double LarrySpeed = 1.05, LarryShow = 1100, LarrySayM = 130, LarrySayCool = 26, LarryWindowS = 11 * 60, LarryOutChance = .42;
	constexpr double DeckW = 3.0, DeckL = 3.4, DeckY = .36;
	const TCHAR* LarryLine = TEXT("yeahhhhh dude");
	// sandbar.ts
	constexpr double CrestMin = .45, CrestMax = 1.1, ReliefM = .6, MinReliefM = -.05, RingR = 140, AccessM = 1.6, MinShoreD = 200, BarR = 58, BarShow = 900, SiteSpacing = 700;
	constexpr double RotateS = 40 * 60;
	// party-boats.ts
	constexpr double PartyMinDepth = 1.8, PartyMinShore = 22, PartyMaxShore = 150, PartyDraw = 1400;

	double WallS() { return double(FDateTime::UtcNow().ToUnixTimestamp()); }

	FKeysRoute MakePath(const TArray<FVector2D>& Pts)
	{
		FKeysRoute R;
		R.Pts = Pts;
		const int32 N = R.Pts.Num();
		R.Cum.Add(0);
		for (int32 i = 1; i <= N; i++) R.Cum.Add(R.Cum[i - 1] + (N ? FVector2D::Distance(R.Pts[i - 1], R.Pts[i % N]) : 0));
		R.Total = N ? R.Cum[N] : 0;
		return R;
	}

	// larry.ts driftLoop: a slow oval of open water, each bearing pushed in or out to find it
	FKeysRoute DriftLoop(double CX, double CZ, double R)
	{
		auto Ok = [](double X, double Z) { return DepthAt(X, Z) >= 1.8 && LandH(X, Z) <= .02 && ShoreInfo(X, Z).D >= 70; };
		TArray<FVector2D> Pts;
		for (int32 i = 0; i < 32; i++)
		{
			const double A = i / 32.0 * PI * 2, CA = FMath::Cos(A), SA = FMath::Sin(A) * .62;
			for (double K : {1.0, 1.12, .88, 1.26, .76, 1.4, .64, .52})
			{
				const double X = CX + CA * R * K, Z = CZ + SA * R * K;
				if (!Ok(X, Z)) continue;
				Pts.Add(FVector2D(X, Z));
				break;
			}
		}
		return MakePath(Pts);
	}

	const KeysData::FBoatSpec& BoatById(const TCHAR* Id)
	{
		const int32 I = KeysData::BoatIndexOf(Id);
		return KeysData::Boats()[I >= 0 ? I : 0];
	}

	FKeysHumanLook FolkLook(int32 I, bool bBikini)
	{
		static const uint32 Skins[] = {0x8d5524, 0xc68642, 0xe0ac69, 0xf1c27d, 0xffdbac, 0x6b4226};
		static const uint32 Shirts[] = {0xffffff, 0x2f8fd8, 0xf26b5b, 0x3fb68b, 0xf4c542, 0x8a5cc2, 0xff8c42, 0x9ad0ec};
		static const uint32 Suits[] = {0xe4572e, 0x2bb3a8, 0xf07aa0, 0x1d3557, 0xf2c14e};
		FKeysHumanLook L;
		L.Skin = Hex(Skins[(I * 7 + 3) % 6]);
		L.bBikini = bBikini;
		L.Shirt = Hex(Shirts[(I * 3 + 1) % 8]);
		L.Bottom = bBikini ? Hex(Suits[I % 5]) : Hex(0x2b4a6e);
		L.bLongHair = bBikini;
		L.HairStyle = I % 4;
		L.bGlasses = I % 3 == 1;
		L.bCap = !bBikini && I % 3 == 0;
		L.CapColor = Hex(Shirts[(I + 5) % 8]);
		L.Hair = Hex(I % 2 ? 0x3b2a1e : 0x8a6a3a);
		return L;
	}

	// a dancing person on a deck (the deck party's beat)
	void Dance(FKeysHuman& Hm, double T, double Bpm, double Ph)
	{
		const double G = T * Bpm / 60 * PI + Ph;
		Hm.IdleUpdate(T, 0);
		Hm.Roll = FMath::Sin(G) * .1;
		Hm.PoseLegs(FKeysHuman::J(-.12, .085, .02), FKeysHuman::J(.12, .085 + FMath::Max(0.0, FMath::Sin(G)) * .12, .08));
		const double Up = FMath::Max(0.0, FMath::Sin(G * .5 + Ph));
		Hm.Pose(FKeysHuman::J(-.24 + FMath::Sin(G) * .06, Hm.HipY + .45 + Up * .55, .12), FKeysHuman::J(.24 + FMath::Sin(G) * .06, Hm.HipY + .45 + (1 - Up) * .5, .12));
		Hm.HeadNod = FMath::Sin(G * 2) * .12;
	}
}

AKeysLocals::AKeysLocals()
{
	PrimaryActorTick.bCanEverTick = true;
	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent = Root;
}

UProceduralMeshComponent* AKeysLocals::NewMesh(const TCHAR* Name)
{
	UProceduralMeshComponent* C = NewObject<UProceduralMeshComponent>(this, FName(Name));
	C->SetupAttachment(Root);
	C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	C->bUseAsyncCooking = true;
	C->RegisterComponent();
	return C;
}

AKeysLocals* AKeysLocals::Get(const UObject* WorldContext)
{
	UWorld* W = WorldContext ? WorldContext->GetWorld() : nullptr;
	if (!W) return nullptr;
	TActorIterator<AKeysLocals> It(W);
	return It ? *It : nullptr;
}

void AKeysLocals::BeginPlay()
{
	Super::BeginPlay();
	SetActorLocation(FVector::ZeroVector);
	BuildFleet();
	BuildLarry();
	BuildSandbar();
	BuildParties();
	Dog = NewMesh(TEXT("BoatDog"));
	Dog->SetCastShadow(true);
}

// ================================================================ the working fleet (commercial.ts)
void AKeysLocals::BuildFleet()
{
	struct FDef { const TCHAR* Name; int32 Kind; FKeysRoute R; double Speed; };
	const TArray<FDef> Defs = {
		{TEXT("Hot Tuna"), 0, AKeysTraffic::MakeRoute(1250, 1420), 3.5},
		{TEXT("Dog Pound"), 1, AKeysTraffic::RouteFrom({FVector2D(-2400, ChainZ(-2400) + 620), FVector2D(-900, ChainZ(-900) + 540), FVector2D(200, ChainZ(200) + 700), FVector2D(-1400, ChainZ(-1400) + 820)}), 2.4},
		{TEXT("Pin Wheel"), 2, AKeysTraffic::MakeRoute(2050, 2400), 2.2},
		{TEXT("No Limits"), 3, AKeysTraffic::RouteFrom({FVector2D(900, ChainZ(900) + 560), FVector2D(2300, ChainZ(2300) + 700), FVector2D(3200, ChainZ(3200) + 520), FVector2D(1700, ChainZ(1700) + 460)}), 2.6},
		{TEXT("Fat Tuna"), 4, AKeysTraffic::MakeRoute(2650, 3050), 2.0}};
	UMaterialInterface* VC = AKeysWorld::VertexColorMaterial();
	const FLinearColor Steel = Hex(0x8d949b), Rust = Hex(0x7a4a2a), Timber = Hex(0x8a6b45), Net = Hex(0x3d4a3a), Buoy = Hex(0xe4572e);
	for (const FDef& D : Defs)
	{
		FKeysMesh M;
		double L = 14, B = 4.4, F = 1.3, Board = 3.4, BoardY = 1.35;
		uint32 HullC = 0xe9e6dc, BootC = 0x1f4f8c, CoveC = 0x1f4f8c, DeckC = 0xcdc7b6;
		bool bCove = true;
		switch (D.Kind)
		{
			case 0: L = 13; B = 4.1; F = 1.25; break;
			case 1: L = 12; B = 4.6; F = 1.15; HullC = 0xdfe6e2; BootC = 0x2c5a3a; bCove = false; DeckC = 0xc9c2ae; Board = 3.6; BoardY = 1.25; break;
			case 2: L = 17; B = 5.4; F = 1.55; HullC = 0xe3dccb; BootC = 0x7a2c1c; CoveC = 0x7a2c1c; DeckC = 0xbfb8a4; Board = 4.4; BoardY = 1.6; break;
			case 3: L = 11.5; B = 4.2; F = 1.1; HullC = 0xf0ead8; BootC = 0xc8a24a; bCove = false; DeckC = 0xcac3af; Board = 3.2; BoardY = 1.2; break;
			default: L = 19; B = 6.0; F = 1.7; HullC = 0xd8d2c2; BootC = 0x2a3d52; CoveC = 0x2a3d52; DeckC = 0xb9b2a0; Board = 5.0; BoardY = 1.75; break;
		}
		// a working hull: the Grady's loft, re-proportioned and painted as a commercial boat
		KeysMesh::FHullDef HD = KeysMesh::HullOf(BoatById(TEXT("grady")));
		HD.F = F; HD.Spring = .42; HD.Dr0 = 18; HD.Dr1 = 42; HD.Rake = 1.1; HD.Steps.Reset(); HD.bCat = false;
		HD.Hull = Hex(HullC); HD.Boot = Hex(BootC); HD.Cove = Hex(CoveC); HD.bCove = bCove; HD.Deck = Hex(DeckC); HD.Cap = Hex(HullC);
		KeysMesh::BuildHullLoft(M, HD, L, B, true);
		auto Traps = [&](double X, double Y, double Z, int32 Cols, int32 Rows)
		{
			for (int32 c = 0; c < Cols; c++) for (int32 r = 0; r < Rows; r++) WebBox(M, .78, .5, .78, X, Y + .27 + r * .54, Z + c * .86, r % 2 ? Timber : Rust);
		};
		if (D.Kind == 0)
		{
			WebBox(M, 3.0, 2.3, 3.6, 0, 2.1, -2.6, Hex(0xf2efe6));
			WebBox(M, 2.4, .9, 2.0, 0, 3.6, -2.6, Hex(0xf2efe6));
			WebTube(M, .09, 6, 0, 5.6, -2.6, Steel);
			for (int32 s = -1; s <= 1; s += 2) { WebTube(M, .11, 13, s * 2.6, 4.2, -1.4, Steel, 0, s * 1.05); WebTube(M, .02, 5.2, s * 7.4, 1.6, -1.0, Steel, 0, s * .12, 4); }
			WebBox(M, 1.6, .7, 1.2, 0, 1.1, 4.2, Rust);
		}
		else if (D.Kind == 1)
		{
			WebBox(M, 3.2, 2.2, 3.0, 0, 2.0, -3.2, Hex(0xeef0ea));
			Traps(-1.3, 1.0, .4, 3, 3);
			Traps(1.3, 1.0, .4, 3, 2);
			WebTube(M, .12, 1.6, B / 2 - .3, 1.8, -.8, Steel);
			WebTube(M, .34, .26, B / 2 - .3, 2.5, -.8, Steel, PI / 2, 0, 10);
			WebBox(M, .9, .9, .9, -1.5, 1.4, -1.6, Buoy);
		}
		else if (D.Kind == 2)
		{
			WebBox(M, 3.6, 2.6, 4.2, 0, 2.6, -4.4, Hex(0xf0ece0));
			WebBox(M, 2.8, 1.0, 2.4, 0, 4.4, -4.4, Hex(0xf0ece0));
			for (int32 s = -1; s <= 1; s += 2)
			{
				WebTube(M, .16, 20, s * 3.0, 4.6, -2.0, Steel, 0, s * 1.22);
				// the nets hanging off the boom ends
				M.AddTube(Web(s * 8.4, 3.8, -.9), Web(s * 10.0, .4, -.3), .2, 1.5, 7, Net);
			}
			WebTube(M, .14, 7.5, 0, 4.4, 5.2, Steel, .5);
			M.AddTube(Web(0, 3.0, 6.6), Web(0, 1.2, 8.2), .15, .8, 6, Net);
		}
		else if (D.Kind == 3)
		{
			WebBox(M, 3.0, 2.0, 2.6, 0, 1.9, -3.0, Hex(0xf4f1e6));
			Traps(-1.2, .95, 1.0, 4, 2);
			Traps(1.2, .95, 1.6, 3, 2);
			WebTube(M, .11, 1.5, B / 2 - .3, 1.7, -.6, Steel);
			WebTube(M, .3, .24, B / 2 - .3, 2.35, -.6, Steel, PI / 2, 0, 10);
			for (int32 i = 0; i < 5; i++) WebTube(M, .05, 1.4, -B / 2 + .25, 1.5, -1.6 + i * .7, Buoy, PI / 2);
		}
		else
		{
			WebBox(M, 4.0, 3.0, 5.0, 0, 2.9, -5.0, Hex(0xeae6da));
			WebBox(M, 3.0, 1.1, 2.8, 0, 4.9, -5.0, Hex(0xeae6da));
			WebTube(M, .1, 5.5, 0, 6.6, -5.0, Steel);
			for (int32 s = -1; s <= 1; s += 2) WebTube(M, .18, 22, s * 3.4, 5.0, -2.4, Steel, 0, s * 1.26);
			// the net drum across the stern: the one fitting that says western rig at any distance
			M.AddTube(Web(-2.3, 2.5, 6.4), Web(2.3, 2.5, 6.4), 1.5, 1.5, 12, Rust);
			M.AddTube(Web(-2.0, 2.5, 6.4), Web(2.0, 2.5, 6.4), 1.75, 1.75, 12, Net);
		}
		// wheelhouse windows: a dark band across the front of each house
		M.SmoothNormals(35);
		UProceduralMeshComponent* C = NewMesh(*FString::Printf(TEXT("Work_%s"), D.Name));
		M.Commit(C, 0, VC);
		C->SetCastShadow(true);
		C->SetVisibility(false);
		FleetMeshes.Add(C);
		// the name board on the transom, and her port under it: painted on, read from astern
		UTextRenderComponent* Tx = NewObject<UTextRenderComponent>(this);
		Tx->SetupAttachment(C);
		Tx->RegisterComponent();
		Tx->SetText(FText::FromString(FString(D.Name).ToUpper() + TEXT("\nMARATHON, FL")));
		Tx->SetHorizontalAlignment(EHTA_Center);
		Tx->SetVerticalAlignment(EVRTA_TextCenter);
		Tx->SetTextRenderColor(FColor(0x10, 0x16, 0x1c));
		Tx->SetWorldSize(Board * 100 * .13);
		Tx->SetRelativeLocationAndRotation(FVector(-L / 2 * 100 - 12, 0, BoardY * 100), FRotator(0, 180, 0));
		Tx->SetCastShadow(false);
		FleetNames.Add(Tx);
		FWorkBoat W;
		W.Name = D.Name; W.Kind = D.Kind; W.L = L; W.B = B; W.R = D.R; W.Speed = D.Speed;
		W.S = FMath::FRand() * FMath::Max(1.0, W.R.Total); W.H = 0; W.Ph = FMath::FRand() * 6.28;
		Fleet.Add(W);
	}
}

void AKeysLocals::TickFleet(double Dt, double T, const FVector& Cam)
{
	for (int32 i = 0; i < Fleet.Num(); i++)
	{
		FWorkBoat& W = Fleet[i];
		if (W.R.Pts.Num() < 2) continue;
		W.S += W.Speed * Dt;
		double X, Z, TX, TZ;
		AKeysTraffic::RouteAt(W.R, W.S, X, Z, TX, TZ);
		W.X = X; W.Z = Z;
		W.H = AngTo(W.H, FMath::Atan2(-TX, -TZ), Dt * 1.4);
		const bool bVis = FMath::Sqrt(FMath::Square(X - Cam.X) + FMath::Square(Z - Cam.Z)) < 1700;
		FleetMeshes[i]->SetVisibility(bVis, true);
		if (!bVis) continue;
		const FPose Ps = RidePose(X, Z, W.H, W.L, W.B, T, .9);
		FleetMeshes[i]->SetWorldLocationAndRotation(ToUE(X, Ps.Y - .15, Z), PoseRot(Ps, W.H));
		if (W.Speed > 1.5) EmitWake(X, Z, W.Speed, W.L, T, Dt);
	}
}

void AKeysLocals::FleetDots(TArray<FVector2D>& Out) const
{
	for (const FWorkBoat& W : Fleet) Out.Add(FVector2D(W.X, W.Z));
}

// ================================================================ Larry's POS (larry.ts)
void AKeysLocals::BuildLarry()
{
	const double Loops[5][3] = {{-2400, ChainZ(-2400) - 700, 520}, {-900, ChainZ(-900) - 820, 460}, {700, ChainZ(700) - 760, 540}, {2300, ChainZ(2300) - 640, 480}, {1500, ChainZ(1500) + 700, 430}};
	for (const auto& Lp : Loops) LarryLoops.Add(DriftLoop(Lp[0], Lp[1], Lp[2]));
	Larry = NewMesh(TEXT("LarryRaft"));
	LarryMan = NewMesh(TEXT("Larry"));
	LarryMan->AttachToComponent(Larry, FAttachmentTransformRules::KeepRelativeTransform);
	FKeysMesh M;
	const FLinearColor Drum = Hex(0x2f6fa8), Ply = Hex(0xb08d5c), PlyDark = Hex(0x8a6c45), PlyGrey = Hex(0x9aa09b), Rope = Hex(0xcdbd96), Rust = Hex(0x7a4a2e);
	// three blue drums lashed under the deck, steel hoops on each
	for (int32 i = 0; i < 3; i++)
	{
		const double Z = -1.15 + i * 1.15;
		M.AddTube(Web(-(DeckW - .1) / 2, .03, Z), Web((DeckW - .1) / 2, .03, Z), .29, .29, 12, Drum);
		for (double O : {-.55, .55}) M.AddTube(Web(O - .03, .03, Z), Web(O + .03, .03, Z), .31, .31, 12, Rust);
	}
	// mismatched decking, not one board flat
	const double Wid[] = {.42, .3, .37, .26, .45, .33, .4, .3, .38, .29};
	double Zc = -DeckL / 2;
	for (int32 i = 0; Zc < DeckL / 2 - .02; i++)
	{
		const double W = FMath::Min(Wid[i % 10], DeckL / 2 - Zc);
		const FLinearColor Cs[3] = {Ply, PlyDark, PlyGrey};
		WebBox(M, DeckW, .06, W - .015, 0, DeckY, Zc + W / 2, Cs[i % 3], i % 3 == 1 ? .012 : -.008);
		Zc += W;
	}
	WebBox(M, .1, .09, DeckL, -1.2, DeckY - .07, 0, PlyDark);
	WebBox(M, .1, .09, DeckL, 1.2, DeckY - .07, 0, PlyDark);
	for (double Z : {-1.15, 0.0, 1.15}) M.AddTube(Web(.6, .12 - .36, Z), Web(.6, .12 + .36, Z), .03, .03, 4, Rope);
	// the tent that has lost a pole: a dome sagging on one side
	{
		const FLinearColor Tent = Hex(0xcf7a3c);
		const int32 NU = 14, NV = 6;
		auto TP = [&](int32 iu, int32 iv)
		{
			const double Lon = 2 * PI * iu / NU, Lat = PI / 2 * iv / NV;
			double X = FMath::Cos(Lat) * FMath::Cos(Lon) * .95, Y = FMath::Sin(Lat) * .95, Z = FMath::Cos(Lat) * FMath::Sin(Lon) * .95;
			const double Sag = FMath::Max(0.0, -X / .95) * FMath::Max(0.0, (Z + .3) / 1.25);
			return Web(-.1 + X * 1.05, DeckY + .03 + Y * (1.18 - .72 * Sag), -.55 + Z * 1.05);
		};
		const FVector Ctr = Web(-.1, DeckY + .4, -.55);
		for (int32 iv = 0; iv < NV; iv++)
			for (int32 iu = 0; iu < NU; iu++)
			{
				const FVector A = TP(iu, iv), B = TP(iu + 1, iv), C = TP(iu + 1, iv + 1), D = TP(iu, iv + 1);
				M.AddQuad(A, B, C, D, Tent, (A + C) * .5 - Ctr);
			}
		WebBox(M, .8, .45, .02, .1, DeckY + .25, .36, Hex(0x241c16), 0, .22);
		WebBox(M, .62, .5, .01, -.68, DeckY + .42, -.62, Hex(0x3f72b0), -.9, .5, .3);   // the blue tarp patch
		WebTube(M, .018, 1.1, -.92, DeckY + .3, -.5, Hex(0x5e6166), .5, .95, 5);       // the pole that gave up
	}
	// a 2 hp kicker clamped to a scrap of two-by-ten
	WebBox(M, .5, .11, .09, 0, DeckY + .08, DeckL / 2 - .02, PlyDark);
	WebBox(M, .23, .26, .3, .12, DeckY + .2, DeckL / 2 + .04, Hex(0x9c2f28));
	WebTube(M, .045, .62, .12, DeckY - .2, DeckL / 2 + .04, Hex(0xd9dde0));
	WebBox(M, .05, .14, .26, .12, DeckY - .48, DeckL / 2 + .06, Hex(0xd9dde0));
	WebTube(M, .022, .5, -.14, DeckY + .21, DeckL / 2 - .08, Hex(0x1f2429), .1, PI / 2 - .25, 5);
	// cooler, jerry can, milk crate, bucket, a rod, empties, and a flag that was white once
	WebBox(M, .62, .36, .4, -.95, DeckY + .21, .78, Hex(0xe3e6e4));
	WebBox(M, .56, .03, .34, -.95, DeckY + .4, .78, Hex(0xcf4b3a));
	WebBox(M, .22, .3, .16, .95, DeckY + .18, -1.05, Hex(0xc8302c));
	WebBox(M, .06, .06, .06, .95, DeckY + .36, -1.05, Hex(0x2a2f33));
	WebBox(M, .36, .3, .36, .78, DeckY + .18, .95, Hex(0x2f7d4f), 0, .3);
	M.AddTube(Web(-1.2, DeckY + .02, -.1), Web(-1.2, DeckY + .32, -.1), .13, .17, 10, Hex(0xe0a63c));
	WebTube(M, .012, 2.4, .86, DeckY + .95, .45, Hex(0x23262a), -.55, .18, 5);
	const uint32 Cans[5] = {0xc0392b, 0xb8bcc0, 0xc0392b, 0x2f6fa8, 0xb8bcc0};
	for (int32 i = 0; i < 5; i++) M.AddTube(Web(-.4 + i * .3 - .06, DeckY + .06, 1.05 - (i % 2) * .22), Web(-.4 + i * .3 + .06, DeckY + .06, 1.05 - (i % 2) * .22 + .02), .033, .033, 7, Hex(Cans[i]));
	WebTube(M, .014, 1.5, -1.3, DeckY + .75, .95, Hex(0x6d5a42), 0, 0, 5);
	WebBox(M, .5, .3, .01, -1.05, DeckY + 1.3, .95, Hex(0xdcd6c4));
	M.Commit(Larry, 0, AKeysWorld::VertexColorMaterial());
	Larry->SetCastShadow(true);
	Larry->SetVisibility(false, true);
	FKeysHumanLook Lk;
	Lk.Shirt = Hex(0x2e8b74); Lk.Bottom = Hex(0xd8cfae); Lk.bCap = true; Lk.CapColor = Hex(0xcfc7a8); Lk.bGlasses = true; Lk.Hair = Hex(0x6b5435); Lk.bShoes = false;
	LarryH0.Init(Lk, true);
	LarryH0.Pos = Web(.78, DeckY + .33, .95);
	LarryH0.Yaw = .5;   // web rotation.y = PI - 0.5, facing forward and a little out
	// a window of the wall clock: is he out, and on which loop (larryAt)
	const int64 Wn = int64(WallS() / LarryWindowS);
	bLarryOut = KeysSea::HashCell(Seed, int32(Wn), 11, SaltLarryOut) < LarryOutChance;
	LarryRoute = FMath::Min(LarryLoops.Num() - 1, int32(KeysSea::HashCell(Seed, int32(Wn), 12, SaltLarryRoute) * LarryLoops.Num()));
	LarryS = FMath::FRand() * FMath::Max(1.0, LarryLoops[LarryRoute].Total);
}

void AKeysLocals::TickLarry(double Dt, double T, const FVector& Cam)
{
	const double DCam = FMath::Sqrt(FMath::Square(LarryX - Cam.X) + FMath::Square(LarryZ - Cam.Z));
	// he only appears, vanishes or changes loop while nobody is near enough to watch
	if (DCam >= LarryShow || !bLarryShown)
	{
		const int64 Wn = int64(WallS() / LarryWindowS);
		const bool bOut = KeysSea::HashCell(Seed, int32(Wn), 11, SaltLarryOut) < LarryOutChance;
		const int32 Route = FMath::Min(LarryLoops.Num() - 1, int32(KeysSea::HashCell(Seed, int32(Wn), 12, SaltLarryRoute) * LarryLoops.Num()));
		if (!bLarryForce && (bOut != bLarryOut || Route != LarryRoute))
		{
			if (Route != LarryRoute) LarryS = FMath::FRand() * FMath::Max(1.0, LarryLoops[Route].Total);
			bLarryOut = bOut; LarryRoute = Route;
		}
	}
	const FKeysRoute& R = LarryLoops[LarryRoute];
	if (!bLarryOut || R.Pts.Num() < 2) { if (bLarryShown) Larry->SetVisibility(false, true); bLarryShown = false; return; }
	LarryS += LarrySpeed * Dt;
	double X, Z, TX, TZ;
	AKeysTraffic::RouteAt(R, LarryS, X, Z, TX, TZ);
	LarryX = X; LarryZ = Z;
	LarryH = AngTo(LarryH, FMath::Atan2(-TX, -TZ), Dt * .6);
	const double D = FMath::Sqrt(FMath::Square(X - Cam.X) + FMath::Square(Z - Cam.Z));
	const bool bVis = D < LarryShow;
	if (bVis != bLarryShown) Larry->SetVisibility(bVis, true);
	bLarryShown = bVis;
	if (!bVis) return;
	// a raft takes the full wave slope on both axes, and then some
	FPose Ps = RidePose(X, Z, LarryH, DeckL, DeckW, T, 1.25);
	Ps.Pitch *= 1.25;
	Larry->SetWorldLocationAndRotation(ToUE(X, Ps.Y - .16, Z), PoseRot(Ps, LarryH));
	if (D < 260)
	{
		FKeysMesh M;
		LarryH0.IdleUpdate(T, 0);
		LarryH0.Pose(FKeysHuman::J(-.16, LarryH0.HipY + .08, .34), FKeysHuman::J(.2, LarryH0.HipY + .12 + FMath::Sin(T * .7) * .03, .3));
		LarryH0.Build(M);
		M.Commit(LarryMan, 0, AKeysWorld::VertexColorMaterial());
	}
	if (LarrySay > 0) LarrySay -= Dt;
	if (LarryBubble > 0) LarryBubble -= Dt;
	if (LarrySay <= 0 && D < LarrySayM)
	{
		LarrySay = LarrySayCool;
		LarryBubble = 5;
		if (AKeysBoatPawn* P = Cast<AKeysBoatPawn>(UGameplayStatics::GetPlayerPawn(this, 0))) P->ShowMsg(FString::Printf(TEXT("Larry: %s"), LarryLine));
	}
	if (D < 260) KeysTags::Add(FVector(X, Ps.Y + 1.9, Z), LarryBubble > 0 ? FString::Printf(TEXT("Larry: \"%s\""), LarryLine) : FString(TEXT("Larry's POS")), 260, LarryBubble > 0);
}

bool AKeysLocals::LarryPos(double& OX, double& OZ) const
{
	if (!bLarryOut) return false;
	OX = LarryX; OZ = LarryZ;
	return true;
}

// ================================================================ the sandbar party (sandbar.ts)
void AKeysLocals::BuildSandbar()
{
	// findSandbars: a crest you could stand on, a real high relative to the water round it, reachable by boat, away from the beach
	TArray<FBar> All;
	for (double X = -3200; X <= 3200; X += 60)
		for (double DZ = -1200; DZ <= 800; DZ += 40)
		{
			const double Z = ChainZ(X) + DZ, D = DepthAt(X, Z);
			if (D < CrestMin || D > CrestMax || LandH(X, Z) > .02 || ShoreInfo(X, Z).D < MinShoreD) continue;
			double Sum = 0, Lo = 1e9, Hi = -1e9;
			for (int32 a = 0; a < 8; a++)
			{
				const double Th = a / 8.0 * PI * 2, Q = DepthAt(X + FMath::Cos(Th) * RingR, Z + FMath::Sin(Th) * RingR);
				Sum += Q; Lo = FMath::Min(Lo, Q); Hi = FMath::Max(Hi, Q);
			}
			const double Rel = Sum / 8 - D;
			if (Rel < ReliefM || Lo - D < MinReliefM || Hi < AccessM) continue;
			All.Add({X, Z, D, Rel});
		}
	All.Sort([](const FBar& A, const FBar& B) { return A.Relief > B.Relief; });
	for (const FBar& C : All)
	{
		bool bNear = false;
		for (const FBar& Q : Bars) if (FMath::Square(Q.X - C.X) + FMath::Square(Q.Z - C.Z) < SiteSpacing * SiteSpacing) bNear = true;
		if (bNear) continue;
		Bars.Add(C);
		if (Bars.Num() >= 8) break;
	}
	UE_LOG(LogTemp, Log, TEXT("Keys Run: %d sandbars"), Bars.Num());
	if (!Bars.Num()) return;
	BarSand = NewMesh(TEXT("SandbarDome"));
	BarPeople = NewMesh(TEXT("SandbarPeople"));
	BarFloatMesh = NewMesh(TEXT("SandbarFloats"));
	const double Arc0 = KeysSea::HashCell(Seed, 1, 0, SaltHouseRot) * PI * 2;
	const TCHAR* Pick[8] = {TEXT("robalo"), TEXT("grady"), TEXT("freeman"), TEXT("contender"), TEXT("yellowfin"), TEXT("robalo"), TEXT("grady"), TEXT("contender")};
	TMap<int32, FKeysMesh> Hulls;
	for (int32 i = 0; i < 8; i++)
	{
		const int32 HI = KeysData::BoatIndexOf(Pick[i]);
		if (HI < 0) continue;
		if (!Hulls.Contains(HI)) { FKeysMesh M; KeysMesh::BuildBoat(M, KeysData::Boats()[HI], true); Hulls.Add(HI, MoveTemp(M)); }
		const double A = Arc0 + (i - 3.5) * .17, R = BarR * .92 + (i % 2) * 4.5;
		FBarBoat Bb;
		Bb.Hull = HI; Bb.LX = FMath::Cos(A) * R; Bb.LZ = FMath::Sin(A) * R;
		Bb.Yaw = FMath::Atan2(-FMath::Cos(A), -FMath::Sin(A)) + PI / 2;
		Bb.Ph = FMath::FRand() * 6.28;
		BarBoats.Add(Bb);
		UProceduralMeshComponent* C = NewMesh(*FString::Printf(TEXT("SandbarBoat%d"), i));
		Hulls[HI].Commit(C, 0, AKeysWorld::VertexColorMaterial());
		C->SetCastShadow(true);
		BarBoatMeshes.Add(C);
	}
	for (int32 i = 0; i < 14; i++)
	{
		const double A = Arc0 + (KeysSea::HashCell(Seed, i, 3, SaltHouseRot) - .5) * 2.4;
		const double R = BarR * (.15 + KeysSea::HashCell(Seed, i, 4, SaltHouseRot) * .66);
		FBarFolk F;
		F.Hm.Init(FolkLook(i, i % 2 == 0), false);
		F.LX = FMath::Cos(A) * R; F.LZ = FMath::Sin(A) * R; F.RR = FMath::Min(1.0, R / BarR);
		F.Yaw = KeysSea::HashCell(Seed, i, 5, SaltHouseRot) * PI * 2;
		BarFolk.Add(F);
	}
	for (int32 i = 0; i < 9; i++)
	{
		const double A = Arc0 + (KeysSea::HashCell(Seed, i, 6, SaltHouseRot) - .5) * 2.2;
		const double R = BarR * (.45 + KeysSea::HashCell(Seed, i, 7, SaltHouseRot) * .6);
		BarFloats.Add({FMath::Cos(A) * R, FMath::Sin(A) * R, 0, i % 3 == 2 ? 4 : i % 4});
	}
	ApplySandbar(FMath::Min(Bars.Num() - 1, int32(KeysSea::HashCell(Seed, int32(WallS() / RotateS), 77, SaltSandbar) * Bars.Num())));
}

void AKeysLocals::ApplySandbar(int32 I)
{
	BarI = I;
	const FBar& S = Bars[I];
	// the sand dome, its crest a hand under the surface, in game meters about the bar's centre
	FKeysMesh M;
	const int32 Seg = 28, Rings = 6;
	const FLinearColor C0 = Hex(0xe8dcb8), C1 = Hex(0x9fb098);
	const double Hh = S.Depth - .12;
	auto P = [&](int32 r, int32 k)
	{
		const double Rr = BarR * r / Rings, A = 2 * PI * k / Seg;
		return FVector(FMath::Cos(A) * Rr, FMath::Sin(A) * Rr, -S.Depth + FMath::Cos(double(r) / Rings * PI / 2) * Hh);
	};
	auto Cc = [&](int32 r) { return FMath::Lerp(C1, C0, float(FMath::Pow(1 - double(r) / Rings, 1.4))); };
	for (int32 r = 0; r < Rings; r++)
		for (int32 k = 0; k < Seg; k++)
		{
			const FVector A = P(r, k), B = P(r, k + 1), C = P(r + 1, k + 1), D = P(r + 1, k);
			if (r == 0) M.AddTri(A, C, D, Cc(r), FVector(0, 0, 1));
			else M.AddQuad(A, B, C, D, Cc(r), FVector(0, 0, 1));
		}
	M.SmoothNormals(80);
	// UE frame: game (x, z) -> (X, Y); the mesh is authored in meters with Z up, so it places directly
	M.Commit(BarSand, 0, AKeysWorld::VertexColorMaterial());
	BarSand->SetWorldLocation(ToUE(S.X, 0, S.Z));
	for (FBarFolk& F : BarFolk) F.Hm.Pos = FVector(F.LX, F.LZ, -S.Depth + FMath::Cos(F.RR * PI / 2) * Hh);
	BarPeople->SetWorldLocation(ToUE(S.X, 0, S.Z));
	BarFloatMesh->SetWorldLocation(ToUE(S.X, 0, S.Z));
	BarPeopleT = 0;
}

bool AKeysLocals::SandbarSite(double& OX, double& OZ) const
{
	if (BarI < 0) return false;
	OX = Bars[BarI].X; OZ = Bars[BarI].Z;
	return true;
}

void AKeysLocals::TickSandbar(double Dt, double T, const FVector& Cam)
{
	if (BarI < 0) return;
	auto Seen = [&Cam](const FBar& S) { return FMath::Square(S.X - Cam.X) + FMath::Square(S.Z - Cam.Z) < BarShow * BarShow; };
	// the party moves bars only while nobody is looking
	if (!Seen(Bars[BarI]) && Bars.Num() > 1)
	{
		const int32 Want = FMath::Min(Bars.Num() - 1, int32(KeysSea::HashCell(Seed, int32(WallS() / RotateS), 77, SaltSandbar) * Bars.Num()));
		if (Want != BarI) ApplySandbar(Want);
	}
	const FBar& S = Bars[BarI];
	const bool bNear = Seen(S);
	BarSand->SetVisibility(bNear);
	BarPeople->SetVisibility(bNear);
	BarFloatMesh->SetVisibility(bNear);
	for (UProceduralMeshComponent* C : BarBoatMeshes) C->SetVisibility(bNear);
	if (!bNear) return;
	for (int32 i = 0; i < BarBoats.Num(); i++)
	{
		const FBarBoat& B = BarBoats[i];
		const KeysData::FBoatSpec& Sp = KeysData::Boats()[B.Hull];
		const double Yaw = B.Yaw + FMath::Sin(T * .21 + B.Ph) * .05, WX = S.X + B.LX, WZ = S.Z + B.LZ;
		const FPose Ps = RidePose(WX, WZ, Yaw, Sp.Len, Sp.Beam, T, .85);
		BarBoatMeshes[i]->SetWorldLocationAndRotation(ToUE(WX, Ps.Y, WZ), PoseRot(Ps, Yaw));
	}
	const double DC = FMath::Sqrt(FMath::Square(S.X - Cam.X) + FMath::Square(S.Z - Cam.Z));
	BarPeopleT -= Dt;
	if (DC < 320 && BarPeopleT <= 0)
	{
		BarPeopleT = .1;
		FKeysMesh M;
		for (int32 i = 0; i < BarFolk.Num(); i++)
		{
			FKeysHuman& Hm = BarFolk[i].Hm;
			Hm.IdleUpdate(T, 0);
			Hm.Yaw = -BarFolk[i].Yaw + FMath::Sin(T * .2 + i) * .3;
			// waist deep: drinks up, a wave, a laugh
			const double G = T * 1.3 + i * 1.7;
			if (i % 3 == 0) Hm.Pose(FKeysHuman::J(-.24, Hm.HipY + .02, .05), FKeysHuman::J(.18, Hm.HipY + .5 + FMath::Max(0.0, FMath::Sin(G)) * .12, .28));
			else if (i % 3 == 1) Hm.Pose(FKeysHuman::J(-.3, Hm.HipY + .95 + FMath::Sin(G * 2) * .05, .05), FKeysHuman::J(.24, Hm.HipY + .02, .05));
			else Hm.DefaultPose();
			Hm.Build(M);
		}
		M.Commit(BarPeople, 0, AKeysWorld::VertexColorMaterial());
		// the floats: rings in four colours and a cooler, riding the swell
		FKeysMesh F;
		const uint32 Rings[4] = {0xf2c14e, 0xe4572e, 0x2bb3a8, 0xf07aa0};
		for (int32 i = 0; i < BarFloats.Num(); i++)
		{
			const FBarFloat& Fl = BarFloats[i];
			const double Y = WaveH(S.X + Fl.LX, S.Z + Fl.LZ, T, AmpAt(S.X, S.Z)) + .06, Rot = T * .12 * (i % 2 ? 1 : -1);
			if (Fl.Kind == 4) { F.AddBox(FVector(Fl.LX, Fl.LZ, Y + .12), FVector(.9, .6, .5), FLinearColor(.92f, .92f, .92f), FQuat(FVector::ZAxisVector, Rot)); continue; }
			const int32 N = 12;
			for (int32 k = 0; k < N; k++)
			{
				const double A0 = Rot + 2 * PI * k / N, A1 = Rot + 2 * PI * (k + 1) / N;
				F.AddTube(FVector(Fl.LX + FMath::Cos(A0) * .62, Fl.LZ + FMath::Sin(A0) * .62, Y), FVector(Fl.LX + FMath::Cos(A1) * .62, Fl.LZ + FMath::Sin(A1) * .62, Y), .21, .21, 6, Hex(Rings[Fl.Kind]));
			}
		}
		F.Commit(BarFloatMesh, 0, AKeysWorld::VertexColorMaterial());
	}
	if (DC < 600) KeysTags::Add(FVector(S.X, 4, S.Z), TEXT("Sandbar party"), 600);
}

// ================================================================ boats on the hook (party-boats.ts)
void AKeysLocals::BuildParties()
{
	struct FAnc { const TCHAR* Name; double X, StartDz; int32 Dir; TArray<const TCHAR*> Boats; };
	const TArray<FAnc> Ancs = {
		{TEXT("Sombrero Beach"), -300, 0, 1, {TEXT("midnight"), TEXT("grady")}},
		{TEXT("East Sister Rock"), 900, 560, 1, {TEXT("freeman")}},
		{TEXT("Bamboo Key"), 350, -1350, -1, {TEXT("robalo")}}};
	MooredRopes = NewMesh(TEXT("PartyRopes"));
	for (const FAnc& A : Ancs)
	{
		// findAnchorage: march away from the island to the first floatable spot close to the shore
		const double Z0 = ChainZ(A.X) + A.StartDz;
		double SX = 0, SZ = 0;
		bool bFound = false;
		for (int32 k = 0; k < 1400 && !bFound; k++)
		{
			const double Z = Z0 + A.Dir * k;
			if (LandH(A.X, Z) > .05) continue;
			const double Sd = ShoreInfo(A.X, Z).D;
			if (Sd < PartyMinShore) continue;
			if (Sd > PartyMaxShore) break;
			if (DepthAt(A.X, Z) < PartyMinDepth) continue;
			SX = A.X; SZ = Z; bFound = true;
		}
		if (!bFound) { UE_LOG(LogTemp, Log, TEXT("Keys Run: no anchorage at %s"), A.Name); continue; }
		for (int32 i = 0; i < A.Boats.Num(); i++)
		{
			const int32 HI = KeysData::BoatIndexOf(A.Boats[i]);
			if (HI < 0) continue;
			FMoored Mo;
			Mo.Hull = HI; Mo.AX = SX + i * 5.2; Mo.AZ = SZ + i * 1.4; Mo.BaseYaw = A.Dir > 0 ? 0 : PI;
			Mo.Ph = FMath::FRand() * 6; Mo.Len = KeysData::Boats()[HI].Len; Mo.TieX = SX + 6; Mo.TieZ = SZ - A.Dir * 16;
			Mo.Bpm = FMath::RandRange(112, 126);
			for (int32 c = 0; c < 3; c++) { FKeysHuman Hm; Hm.Init(FolkLook(c * 5 + i + int32(A.X), c != 1), false); Mo.Crew.Add(Hm); }
			FKeysMesh M;
			KeysMesh::BuildBoat(M, KeysData::Boats()[HI], true);
			UProceduralMeshComponent* C = NewMesh(*FString::Printf(TEXT("Moored_%s_%d"), A.Name, i));
			M.Commit(C, 0, AKeysWorld::VertexColorMaterial());
			C->SetCastShadow(true);
			MooredMeshes.Add(C);
			UProceduralMeshComponent* Cw = NewMesh(*FString::Printf(TEXT("MooredCrew_%s_%d"), A.Name, i));
			Cw->AttachToComponent(C, FAttachmentTransformRules::KeepRelativeTransform);
			MooredCrew.Add(Cw);
			Moored.Add(Mo);
		}
	}
}

void AKeysLocals::TickParties(double Dt, double T, const FVector& Cam)
{
	FKeysMesh Ropes;
	bool bAnyRope = false;
	for (int32 i = 0; i < Moored.Num(); i++)
	{
		FMoored& Mo = Moored[i];
		const double D = FMath::Sqrt(FMath::Square(Mo.AX - Cam.X) + FMath::Square(Mo.AZ - Cam.Z));
		const bool bVis = D < PartyDraw;
		MooredMeshes[i]->SetVisibility(bVis, true);
		if (!bVis) continue;
		// swinging slowly on the hook, bow into the breeze
		const double Yaw = Mo.BaseYaw + FMath::Sin(T * .07 + Mo.Ph) * .35;
		const double X = Mo.AX + FMath::Sin(T * .05 + Mo.Ph) * 1.2, Z = Mo.AZ;
		const KeysData::FBoatSpec& Sp = KeysData::Boats()[Mo.Hull];
		const FPose Ps = RidePose(X, Z, Yaw, Sp.Len, Sp.Beam, T, .85);
		MooredMeshes[i]->SetWorldLocationAndRotation(ToUE(X, Ps.Y, Z), PoseRot(Ps, Yaw));
		if (D < 260)
		{
			FKeysMesh M;
			const double Sx = -Sp.Len * .3;
			for (int32 c = 0; c < Mo.Crew.Num(); c++)
			{
				FKeysHuman& Hm = Mo.Crew[c];
				const double LX = Sx + c * .9, LY = (c - 1) * Sp.Beam * .18;
				Hm.Pos = FVector(LX, LY, KeysMesh::SoleAt(Sp, LX));
				Hm.Yaw = PI + FMath::Sin(T * .3 + c) * .6;
				Dance(Hm, T, Mo.Bpm, c * 1.3 + Mo.Ph);
				Hm.Build(M);
			}
			M.Commit(MooredCrew[i], 0, AKeysWorld::VertexColorMaterial());
			// the music: a beat on the water, every other bar, carried on the wind
			if (D < 160 && FMath::Fmod(T * Mo.Bpm / 60, 2.0) < Dt * Mo.Bpm / 60)
				if (AKeysAudio* Au = AKeysAudio::Get(this)) Au->PlayExtra(TEXT("PartyBeat"), FVector(X, 1, Z), float(FMath::Clamp(1 - D / 160, 0.0, 1.0)));
		}
		// the line from the bow cleat to the piling in the shallows
		const double FX = -FMath::Sin(Yaw), FZ = -FMath::Cos(Yaw);
		const FVector Bow = ToUE(X + FX * Sp.Len * .48, Ps.Y + 1.1, Z + FZ * Sp.Len * .48) / 100.0, Tie = ToUE(Mo.TieX, .6, Mo.TieZ) / 100.0;
		Ropes.AddTube(Bow, Tie, .035, .035, 5, Hex(0xe8e2d2));
		Ropes.AddTube(ToUE(Mo.TieX, -2.5, Mo.TieZ) / 100.0, ToUE(Mo.TieX, 2.0, Mo.TieZ) / 100.0, .2, .17, 7, Hex(0x5c4a38));
		bAnyRope = true;
	}
	if (bAnyRope) Ropes.Commit(MooredRopes, 0, AKeysWorld::VertexColorMaterial());
	MooredRopes->SetVisibility(bAnyRope);
}

// ================================================================ the boat dog (dog.ts)
void AKeysLocals::TickDog(double Dt, double T, AKeysBoatPawn* P)
{
	if (!P || !P->bRunning) { Dog->SetVisibility(false); return; }
	if (DogOn.Get() != P)
	{
		Dog->AttachToComponent(P->GetRootComponent(), FAttachmentTransformRules::KeepRelativeTransform);
		Dog->SetRelativeTransform(FTransform::Identity);
		DogOn = P;
	}
	const KeysData::FBoatSpec& Sp = P->Spec();
	const double Aft = -Sp.Len * .30, Fwd = -Sp.Len * .09;
	if (DogX < Aft - 1 || DogX > Fwd + 1) DogX = WantX = (Aft + Fwd) / 2;
	const double Sp0 = FMath::Abs(P->GetSpeed()), Accel = (Sp0 - LastSp) / FMath::Max(Dt, 1e-3);
	LastSp = Sp0;
	// a fish coming over the gunwale beats whatever it was doing
	const bool bCaught = P->FState == EKeysFish::Caught;
	if (bCaught && LastCaughtSeen < 0) ExciteT = 6;
	LastCaughtSeen = bCaught ? 1 : -1;
	ExciteT -= Dt;
	const bool bExcited = ExciteT > 0 || P->IsInFight();
	if (Sp0 > 5 || P->IsAir()) DogAct = 2;
	else
	{
		ActT -= Dt;
		if (DogAct == 2) { DogAct = 1; ActT = FMath::FRandRange(1.5, 3.0); }
		if (ActT <= 0)
		{
			DogAct = DogAct == 0 ? 1 : 0;
			ActT = DogAct == 0 ? FMath::FRandRange(2.5, 6.0) : FMath::FRandRange(3.0, 9.0);
			if (DogAct == 0) WantX = FMath::FRandRange(Aft, Fwd);
		}
	}
	if (bExcited && DogAct != 2) { DogAct = 0; WantX = Aft; ActT = FMath::Max(ActT, 1.5); }
	const bool bTrot = DogAct == 0 && FMath::Abs(WantX - DogX) > .05;
	if (bTrot)
	{
		DogDir = FMath::Sign(WantX - DogX);
		DogX += DogDir * (bExcited ? 1.9 : 1.1) * Dt;
		if ((DogDir > 0 && DogX > WantX) || (DogDir < 0 && DogX < WantX)) DogX = WantX;
		StepPh += Dt * (bExcited ? 15 : 9);
	}
	else if (DogAct == 0) { DogAct = 1; ActT = FMath::FRandRange(3.0, 9.0); }
	Wag += ((bExcited ? 1 : bTrot ? .65 : DogAct == 1 ? .3 : .15) - Wag) * FMath::Min(1.0, Dt * 3);
	KeysDog::FPose Pz;
	Pz.Act = DogAct == 0 ? KeysDog::EAct::Trot : DogAct == 1 ? KeysDog::EAct::Sit : KeysDog::EAct::Brace;
	Pz.bTrotting = bTrot; Pz.Step = StepPh; Pz.Wag = Wag; Pz.bExcited = bExcited; Pz.T = T;
	Pz.Lean = FMath::Clamp(-Accel * .012, -.25, .25);
	// along the centreline, a little to starboard of the leaning post
	const double Y = KeysMesh::InnerHalfBeam(Sp, DogX) * .45;
	const FTransform Xf(FQuat(FVector::ZAxisVector, DogDir > 0 ? 0 : PI), FVector(DogX, Y, KeysMesh::SoleAt(Sp, DogX)));
	FKeysMesh M;
	KeysDog::Build(M, KeysDog::Look(TEXT("terrier")), Pz, Xf);
	M.Commit(Dog, 0, AKeysWorld::VertexColorMaterial());
	Dog->SetVisibility(true);
	BarkT -= Dt * (bExcited ? 4 : 1);
	if (BarkT <= 0)
	{
		BarkT = bExcited ? FMath::FRandRange(1.2, 2.6) : FMath::FRandRange(14.0, 40.0);
		if (Sp0 < 5) if (AKeysAudio* Au = AKeysAudio::Get(this)) Au->PlayExtra(TEXT("Bark"), FVector(P->BoatX(), 1, P->BoatZ()), 1);
	}
}

// ================================================================ tick
void AKeysLocals::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	const double Dt = FMath::Min(double(DeltaSeconds), .1), T = GetWorld()->GetTimeSeconds();
	FVector Cam = FVector::ZeroVector;
	if (APlayerCameraManager* CM = UGameplayStatics::GetPlayerCameraManager(this, 0))
	{
		const FVector L = CM->GetCameraLocation();
		Cam = FVector(L.X / 100, L.Z / 100, L.Y / 100);
	}
	TickFleet(Dt, T, Cam);
	TickLarry(Dt, T, Cam);
	TickSandbar(Dt, T, Cam);
	TickParties(Dt, T, Cam);
	TickDog(Dt, T, Cast<AKeysBoatPawn>(UGameplayStatics::GetPlayerPawn(this, 0)));
}

FString AKeysLocals::DebugCmd(const FString& Cmd)
{
	AKeysBoatPawn* P = Cast<AKeysBoatPawn>(UGameplayStatics::GetPlayerPawn(this, 0));
	// lc:larry puts him out now, in front of the boat; lc:sandbar / lc:fleetN / lc:party report where to go
	if (Cmd == TEXT("larry") && P)
	{
		bLarryOut = bLarryForce = true;
		double Best = 1e18;
		for (int32 r = 0; r < LarryLoops.Num(); r++)
			for (int32 k = 0; k < LarryLoops[r].Pts.Num(); k++)
			{
				const double D = FMath::Square(LarryLoops[r].Pts[k].X - P->BoatX()) + FMath::Square(LarryLoops[r].Pts[k].Y - P->BoatZ());
				if (D < Best) { Best = D; LarryRoute = r; LarryS = LarryLoops[r].Cum[k]; }
			}
		double X, Z, TX, TZ;
		AKeysTraffic::RouteAt(LarryLoops[LarryRoute], LarryS + 12, X, Z, TX, TZ);
		P->DebugPlace(X - TZ * 16, Z + TX * 16, HeadingOf(TZ * 16, -TX * 16));
		LarrySay = 3;
		return TEXT("larry out");
	}
	if (Cmd == TEXT("sandbar") && P && BarI >= 0)
	{
		const FBar& S = Bars[BarI];
		P->DebugPlace(S.X + 95, S.Z + 40, FMath::Atan2(95.0, 40.0));
		UE_LOG(LogTemp, Log, TEXT("KRScript sandbar %.0f, %.0f depth %.2f"), S.X, S.Z, S.Depth);
		return TEXT("sandbar");
	}
	if (Cmd.StartsWith(TEXT("fleet")) && P)
	{
		const int32 I = FMath::Clamp(FCString::Atoi(*Cmd.Mid(5)), 0, Fleet.Num() - 1);
		const FWorkBoat& W = Fleet[I];
		double X, Z, TX, TZ;
		AKeysTraffic::RouteAt(W.R, W.S - 30, X, Z, TX, TZ);
		const double PX = X - TZ * 60, PZ = Z + TX * 60;
		P->DebugPlace(PX, PZ, HeadingOf(W.X - PX, W.Z - PZ));
		return W.Name;
	}
	if (Cmd.StartsWith(TEXT("party")) && P && Moored.Num())
	{
		const FMoored& Mo = Moored[FMath::Clamp(FCString::Atoi(*Cmd.Mid(5)), 0, Moored.Num() - 1)];
		const double PX = Mo.AX + 40, PZ = Mo.AZ + (Mo.BaseYaw > 1 ? -35 : 35);
		P->DebugPlace(PX, PZ, HeadingOf(Mo.AX - PX, Mo.AZ - PZ));
		UE_LOG(LogTemp, Log, TEXT("KRScript party at %.0f, %.0f"), Mo.AX, Mo.AZ);
		return TEXT("party");
	}
	return FString();
}
