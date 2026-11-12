#include "KeysEstates.h"
#include "KeysPlatData.h"
#include "KeysKit.h"
#include "KeysGeo.h"
#include "KeysMesh.h"
#include "KeysHuman.h"
#include "KeysData.h"
#include "KeysLife.h"
#include "KeysTown.h"
#include "KeysTownData.h"
#include "KeysWorld.h"
#include "KeysBoatPawn.h"
#include "ProceduralMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/World.h"
#include "EngineUtils.h"

using namespace KeysGeo;
using namespace KeysKit;

namespace
{
	constexpr double DockTop = 1.425, DockRamp = 8, YachtLen = 60, DockReach = YachtLen + 30;
	constexpr int32 MaxLots = 3, MaxTrophies = 10;
	const TCHAR* TrophySpecies[] = {TEXT("sailfish"), TEXT("mahi"), TEXT("tarpon"), TEXT("wahoo"), TEXT("grouper"), TEXT("yellowfin"), TEXT("permit"), TEXT("snook"), TEXT("amberjack"), TEXT("cobia")};
	struct FRes { const TCHAR* Key; const TCHAR* Label; const TCHAR* Note; double W, D; };
	const FRes Residences[] = {{TEXT("villa"), TEXT("Villa"), TEXT("Two storeys, pool deck"), 34, 22}, {TEXT("shack"), TEXT("Fishing shack"), TEXT("Tin roof, porch, cooler"), 9, 7},
		{TEXT("garden"), TEXT("Japanese garden"), TEXT("Tea house, koi pond, raked gravel"), 14, 10}, {TEXT("stilt"), TEXT("Stilt house"), TEXT("On pilings at the waterline"), 16, 12},
		{TEXT("glass"), TEXT("Modern glass"), TEXT("Flat roof, infinity edge"), 30, 15}};
	struct FDockOpt { const TCHAR* Key; const TCHAR* Label; const TCHAR* Note; double Len, W; };
	const FDockOpt Docks[] = {{TEXT("none"), TEXT("No dock"), TEXT("Beach landing"), 0, 0}, {TEXT("skiff"), TEXT("Skiff dock"), TEXT("12 m, flats boat"), 12, 2.4},
		{TEXT("sport"), TEXT("Sportfish slip"), TEXT("30 m, fighting chair"), 30, 3}, {TEXT("yacht"), TEXT("Superyacht T-head"), TEXT("60 m, helipad"), 60, 6}};
	const FRes& ResOf(const FString& K) { for (const FRes& R : Residences) if (K == R.Key) return R; return Residences[0]; }
	const FDockOpt& DockOf(const FString& K) { for (const FDockOpt& D : Docks) if (K == D.Key) return D; return Docks[0]; }
	bool InPoly(const TArray<FVector2D>& Poly, double X, double Z)
	{
		bool bIn = false;
		for (int32 i = 0, j = Poly.Num() - 1; i < Poly.Num(); j = i++)
			if ((Poly[i].Y > Z) != (Poly[j].Y > Z) && X < (Poly[j].X - Poly[i].X) * (Z - Poly[i].Y) / (Poly[j].Y - Poly[i].Y) + Poly[i].X) bIn = !bIn;
		return bIn;
	}
	const KeysGeo::FIsland* IslandOf(const AKeysEstates::FParcel& P)
	{
		const KeysGeo::FIsland* Best = nullptr;
		double BD = 1e18;
		int32 N = 0;
		for (const KeysGeo::FIsland& I : Islands())
			if (I.Name == P.Island) { N++; const double D = IslandDist(I, P.CX, P.CZ); if (D < BD) { BD = D; Best = &I; } }
		if (!N) return ShoreInfo(P.CX, P.CZ).Isl;
		return Best;
	}
	bool Outward(const KeysGeo::FIsland& I, double X, double Z, double& OX, double& OZ)
	{
		const double H = 2, GX = IslandDist(I, X + H, Z) - IslandDist(I, X - H, Z), GZ = IslandDist(I, X, Z + H) - IslandDist(I, X, Z - H);
		const double N = FMath::Sqrt(GX * GX + GZ * GZ);
		if (N < 1e-6) return false;
		OX = GX / N; OZ = GZ / N;
		return true;
	}
	void Fish(FKeysMesh& M, const FString& Key, double LenM, const FTransform& XfM) { KeysKit::AppendFish(M, Key, LenM, XfM); }
}

AKeysEstates::AKeysEstates()
{
	PrimaryActorTick.bCanEverTick = true;
	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent = Root;
}

AKeysEstates* AKeysEstates::Get(const UObject* WorldContext)
{
	UWorld* W = WorldContext ? WorldContext->GetWorld() : nullptr;
	if (!W) return nullptr;
	TActorIterator<AKeysEstates> It(W);
	return It ? *It : nullptr;
}

const TArray<FString>& AKeysEstates::RegionNames()
{
	static const TArray<FString> R = {TEXT("Whole chart"), TEXT("Marathon"), TEXT("Seven Mile"), TEXT("Biscayne Bay"), TEXT("Boot Key"), TEXT("Harbor town")};
	return R;
}

FString AKeysEstates::TierLabel(ETier T)
{
	static const TCHAR* L[] = {TEXT("Harbor Row"), TEXT("Private Island"), TEXT("Point Estate"), TEXT("Oceanfront Estate"), TEXT("Bayfront Estate"), TEXT("Biscayne Estate"), TEXT("Mainland Bayfront")};
	return L[int32(T)];
}

FString AKeysEstates::TierBlurb(ETier T)
{
	static const TCHAR* L[] = {TEXT("Town lots behind the casino, on the pad you can already walk."), TEXT("A whole key. Every shore is yours, and no neighbour can see in."),
		TEXT("The tip of an island, with water on three sides."), TEXT("Hawk Channel side. Faces Sombrero Reef and the Gulf Stream run."),
		TEXT("Florida Bay side. Sunsets over the backcountry flats."), TEXT("Key Biscayne, looking across the bay at the Miami skyline."), TEXT("Mainland shore west of the city, on Biscayne Bay.")};
	return L[int32(T)];
}

FLinearColor AKeysEstates::TierColor(ETier T)
{
	static const uint32 C[] = {0xf2b84b, 0xe46fa1, 0xb48cf0, 0x4fc3d9, 0x6fcf7f, 0x5b8def, 0x9aa7b2};
	return Hex(C[int32(T)]);
}

void AKeysEstates::BeginPlay()
{
	Super::BeginPlay();
	SetActorLocation(FVector::ZeroVector);
	for (const KeysPlat::FRaw& R : KeysPlat::Parcels)
	{
		FParcel P;
		P.Id = R.Id; P.Name = R.Name; P.Island = R.Island; P.Side = R.Side; P.Tier = ETier(R.Tier);
		P.CX = R.CX; P.CZ = R.CZ; P.Area = R.Area; P.Frontage = R.Frontage;
		P.X0 = P.Z0 = 1e18; P.X1 = P.Z1 = -1e18;
		for (int32 k = 0; k < R.PN; k++)
		{
			const FVector2D Q(KeysPlat::Pts[R.P0 + k][0], KeysPlat::Pts[R.P0 + k][1]);
			P.Poly.Add(Q);
			P.X0 = FMath::Min(P.X0, Q.X); P.X1 = FMath::Max(P.X1, Q.X); P.Z0 = FMath::Min(P.Z0, Q.Y); P.Z1 = FMath::Max(P.Z1, Q.Y);
		}
		Index.Add(P.Id, All.Num());
		All.Add(MoveTemp(P));
	}
	for (int32 t = 0; t < 7; t++)
	{
		TArray<double> A;
		for (const FParcel& P : All) if (int32(P.Tier) == t) A.Add(P.Area);
		A.Sort();
		MedianArea.Add(t, A.Num() ? A[A.Num() >> 1] : 1);
	}
	SetRegion(0);
	UE_LOG(LogTemp, Log, TEXT("Keys Run: Keys Run Estates plat - %d parcels"), All.Num());
}

int64 AKeysEstates::PriceOf(const FParcel& P) const
{
	static const double Base[] = {60000, 120000, 45000, 30000, 22000, 18000, 6000};
	const double* Med = MedianArea.Find(int32(P.Tier));
	return int64(FMath::RoundToDouble(Base[int32(P.Tier)] * FMath::Sqrt(P.Area / (Med ? *Med : 1)) / 500) * 500);
}

const AKeysEstates::FParcel* AKeysEstates::ById(const FString& Id) const { const int32* I = Index.Find(Id); return I ? &All[*I] : nullptr; }

const AKeysEstates::FParcel* AKeysEstates::ParcelAt(double X, double Z) const
{
	for (const FParcel& P : All) if (X >= P.X0 && X <= P.X1 && Z >= P.Z0 && Z <= P.Z1 && InPoly(P.Poly, X, Z)) return &P;
	return nullptr;
}

FString AKeysEstates::Founder(const FString& Id) const
{
	if (Id == TEXT("pigeon-key") || Id == TEXT("harbor-row-1")) return TEXT("Klein");
	if (Id == TEXT("fisher-island") || Id == TEXT("harbor-row-4")) return TEXT("Larry");
	return FString();
}

AKeysEstates::FBuild AKeysEstates::DefaultBuild(const FParcel& P) const
{
	FBuild B;
	const bool bHarbor = P.Tier == ETier::Harbor;
	B.Res = bHarbor ? TEXT("glass") : TEXT("villa"); B.Dock = bHarbor ? TEXT("none") : TEXT("sport"); B.Fish = TEXT("marlin"); B.Lb = 512;
	B.Mount = bHarbor ? TEXT("door") : TEXT("gallows"); B.Trophies = 3; B.bPool = true; B.bFire = false;
	if (P.Id == TEXT("pigeon-key")) { B.Res = TEXT("garden"); B.Dock = TEXT("sport"); B.Fish = TEXT("marlin"); B.Lb = 742; B.Mount = TEXT("gallows"); B.Trophies = 6; B.bPool = false; B.bFire = true; }
	if (P.Id == TEXT("fisher-island")) { B.Res = TEXT("villa"); B.Dock = TEXT("yacht"); B.Fish = TEXT("bluefin"); B.Lb = 688; B.Mount = TEXT("door"); B.Trophies = 4; B.bPool = true; B.bFire = false; }
	return B;
}

FString AKeysEstates::BuildToString(const FBuild& B)
{
	return FString::Printf(TEXT("res=%s;dock=%s;fish=%s;lb=%.1f;mount=%s;trophies=%d;pool=%d;fire=%d"), *B.Res, *B.Dock, *B.Fish, B.Lb, *B.Mount, B.Trophies, B.bPool ? 1 : 0, B.bFire ? 1 : 0);
}

AKeysEstates::FBuild AKeysEstates::BuildFromString(const FParcel& P, const FString& S) const
{
	// sanitizeBuild: anything unknown falls back to the default, the weight to the species' record, no gallows without a dock
	FBuild B = DefaultBuild(P);
	TArray<FString> Parts;
	S.ParseIntoArray(Parts, TEXT(";"));
	for (const FString& Pt : Parts)
	{
		FString K, V;
		if (!Pt.Split(TEXT("="), &K, &V)) continue;
		if (K == TEXT("res")) { for (const FRes& R : Residences) if (V == R.Key) B.Res = V; }
		else if (K == TEXT("dock")) { for (const FDockOpt& D : Docks) if (V == D.Key) B.Dock = V; }
		else if (K == TEXT("fish")) { if (KeysData::Species(V)) B.Fish = V; }
		else if (K == TEXT("lb")) B.Lb = FCString::Atod(*V);
		else if (K == TEXT("mount")) { if (V == TEXT("door") || V == TEXT("gallows") || V == TEXT("pedestal")) B.Mount = V; }
		else if (K == TEXT("trophies")) B.Trophies = FMath::Clamp(FCString::Atoi(*V), 0, MaxTrophies);
		else if (K == TEXT("pool")) B.bPool = V == TEXT("1");
		else if (K == TEXT("fire")) B.bFire = V == TEXT("1");
	}
	const KeysData::FSpecies* Sp = KeysData::Species(B.Fish);
	B.Lb = FMath::Clamp(FMath::RoundToDouble(B.Lb * 10) / 10, 1.0, Sp ? Sp->Max : 1000.0);
	if (P.Tier == ETier::Harbor) B.Dock = TEXT("none");
	if (B.Dock == TEXT("none") && B.Mount == TEXT("gallows")) B.Mount = TEXT("door");
	return B;
}

AKeysEstates::FBuild AKeysEstates::BuildOf(const FString& Id) const
{
	const FParcel* P = ById(Id);
	if (!P) return FBuild();
	const AKeysBoatPawn* Pw = Cast<AKeysBoatPawn>(UGameplayStatics::GetPlayerPawn(this, 0));
	const FString* S = Pw ? Pw->EstateBuilds.Find(Id) : nullptr;
	return S ? BuildFromString(*P, *S) : DefaultBuild(*P);
}

void AKeysEstates::Seaward(const FParcel& P, double& SX, double& SZ) const
{
	if (P.Tier == ETier::Harbor) { SX = 0; SZ = KeysTown::Dir(); return; }
	SX = 0; SZ = 1;
	const KeysGeo::FIsland* I = IslandOf(P);
	if (!I) return;
	if (Outward(*I, P.CX, P.CZ, SX, SZ)) return;
	const double DX = P.CX - I->X, DZ = P.CZ - I->Z, N = FMath::Sqrt(DX * DX + DZ * DZ);
	if (N > 1e-6) { SX = DX / N; SZ = DZ / N; }
}

bool AKeysEstates::DockSite(const FParcel& P, double& OX, double& OZ, double& OSX, double& OSZ, TArray<bool>& Wet) const
{
	// dockSite: the shore vertex with the longest run of open water straight out, nearest the lot's seaward face
	double PX, PZ;
	Seaward(P, PX, PZ);
	const KeysGeo::FIsland* I = IslandOf(P);
	const int32 Step = FMath::Max(1, P.Poly.Num() / 48);
	double Bs = -1e18;
	bool bFound = false;
	auto Ray = [](double X, double Z, double SX, double SZ, TArray<bool>& Out) { Out.Reset(); for (int32 t = 0; t <= int32(DockReach); t++) Out.Add(LandH(X + SX * t, Z + SZ * t) <= 0); };
	TArray<bool> W;
	for (int32 i = 0; i < P.Poly.Num(); i += Step)
	{
		const double X = P.Poly[i].X, Z = P.Poly[i].Y;
		double SX = PX, SZ = PZ;
		if (I) Outward(*I, X, Z, SX, SZ);
		Ray(X, Z, SX, SZ, W);
		const int32 First = W.IndexOfByKey(true);
		if (First < 0 || First > 14) continue;
		int32 Run = 0;
		while (First + Run < W.Num() && W[First + Run]) Run++;
		const double Score = FMath::Min(double(Run), YachtLen + 10) - 3 * First + 15 * (SX * PX + SZ * PZ) + .1 * ((X - P.CX) * PX + (Z - P.CZ) * PZ);
		if (Score > Bs) { Bs = Score; OX = X; OZ = Z; OSX = SX; OSZ = SZ; Wet = W; bFound = true; }
	}
	if (!bFound)
	{
		double Far = -1e18;
		for (const FVector2D& V : P.Poly) { const double D = (V.X - P.CX) * PX + (V.Y - P.CZ) * PZ; if (D > Far) { Far = D; OX = V.X; OZ = V.Y; } }
		OSX = PX; OSZ = PZ;
		Ray(OX, OZ, PX, PZ, Wet);
	}
	return true;
}

bool AKeysEstates::Dry(const FParcel& P, double X, double Z) const
{
	if (!InPoly(P.Poly, X, Z)) return false;
	if (P.Tier == ETier::Harbor)
	{
		TArray<const KeysTown::FRect*> Near;
		KeysTown::CollidersNear(X, Z, 1, Near);
		return Near.Num() == 0;
	}
	return LandH(X, Z) > .3;
}

AKeysEstates::FLayout AKeysEstates::Layout(const FParcel& P, const FBuild& B) const
{
	FLayout L;
	const bool bHarbor = P.Tier == ETier::Harbor;
	double ShX = P.Poly[0].X, ShZ = P.Poly[0].Y, SX, SZ;
	TArray<bool> Wet;
	if (!bHarbor) DockSite(P, ShX, ShZ, SX, SZ, Wet);
	else Seaward(P, SX, SZ);
	L.SX = SX; L.SZ = SZ;
	const FRes& R = ResOf(B.Res);
	double W = R.W, D = R.D;
	const double T = B.Res == TEXT("stilt") ? .75 : .2;
	double HX = P.CX + (ShX - P.CX) * T, HZ = P.CZ + (ShZ - P.CZ) * T;
	if (bHarbor)
	{
		W = FMath::Min(W, P.X1 - P.X0 - 5); D = FMath::Min(D, P.Z1 - P.Z0 - 4);
		HX = (P.X0 + P.X1) / 2; HZ = KeysTown::Dir() > 0 ? P.Z1 - 2 - D / 2 : P.Z0 + 2 + D / 2;
	}
	for (int32 i = 0; i < 6 && !Dry(P, HX, HZ); i++) { HX = (HX + P.CX) / 2; HZ = (HZ + P.CZ) / 2; }
	if (!Dry(P, HX, HZ)) { HX = P.CX; HZ = P.CZ; }
	L.HX = HX; L.HZ = HZ; L.HW = W; L.HD = D;
	L.HY = bHarbor ? KeysTown::PadY : FMath::Max(.3, LandH(HX, HZ));
	L.Heading = FMath::Atan2(SX, SZ);
	const double AX = -SZ, AZ = SX;
	const FDockOpt& Dk = DockOf(B.Dock);
	if (!bHarbor && Dk.Len > 0 && Wet.Num())
	{
		// dockLen: the wet stretch nearest the length wanted, short of the far shore
		const int32 First = Wet.IndexOfByKey(true);
		int32 Len = int32(Dk.Len);
		if (First >= 0)
		{
			int32 Land = First;
			while (Land < Wet.Num() && Wet[Land]) Land++;
			const int32 Hi = Land < Wet.Num() ? Land - 4 : Wet.Num() - 1;
			int32 Best = -1;
			double BD = 1e18;
			for (int32 t = FMath::Max(1, First + 3); t <= Hi; t++) if (Wet[t] && FMath::Abs(t - Dk.Len) < BD) { BD = FMath::Abs(t - Dk.Len); Best = t; }
			Len = Best >= 0 ? Best : (First + Land - 1) / 2;
		}
		L.bDock = true;
		L.DX0 = ShX - SX * DockRamp; L.DZ0 = ShZ - SZ * DockRamp; L.DX1 = ShX + SX * Len; L.DZ1 = ShZ + SZ * Len;
		L.DW = Dk.W; L.DLen = Len + DockRamp; L.THead = B.Dock == TEXT("yacht") && Len >= 30 ? 26 : 0;
	}
	auto Front = [&](double K, double& OX, double& OZ) { OX = HX + SX * (D / 2 + K); OZ = HZ + SZ * (D / 2 + K); };
	double FX, FZ;
	if (B.Mount == TEXT("gallows") && L.bDock) { L.MX = ShX + SX * 3 + AX * (Dk.W / 2 + 1.2); L.MZ = ShZ + SZ * 3 + AZ * (Dk.W / 2 + 1.2); }
	else if (B.Mount == TEXT("pedestal")) { Front(6, FX, FZ); L.MX = FX + AX * (W / 2 + 2); L.MZ = FZ + AZ * (W / 2 + 2); }
	else { Front(.3, FX, FZ); L.MX = FX; L.MZ = FZ; }
	auto Side = [&](double K, double F, double& OX, double& OZ)
	{
		for (double FF : {F, -D - F}) { double X, Z; Front(FF, X, Z); if (Dry(P, X + AX * K, Z + AZ * K)) { OX = X + AX * K; OZ = Z + AZ * K; return; } }
		double X, Z; Front(F, X, Z); OX = X + AX * K; OZ = Z + AZ * K;
	};
	if (B.bPool) { L.bPool = true; Side(-W / 4, 6, L.PX, L.PZ); }
	if (B.bFire) { L.bFire = true; Side(W / 2 + 4, 3, L.FX, L.FZ); }
	for (int32 i = 0; L.Trophies.Num() < B.Trophies && i < 60; i++)
	{
		const double A = i * 2.39996, Rr = FMath::Max(W, D) * .75 + 4 + (i / 10) * 3;
		const double X = HX + FMath::Cos(A) * Rr, Z = HZ + FMath::Sin(A) * Rr;
		if (Dry(P, X, Z)) L.Trophies.Add(FVector2D(X, Z));
	}
	return L;
}

// ================================================================ what stands on a lot (entities/estates/**)
void AKeysEstates::BuildLot(const FParcel& P, const FBuild& B, FKeysMesh& M, TArray<TPair<FVector, FString>>& Signs)
{
	const FLayout L = Layout(P, B);
	const double H = L.Heading, C = FMath::Cos(H), S = FMath::Sin(H);
	// the residence frame: local +z is the sea, +x along the shore; boxes stand on their y
	auto W = [&](double X, double Y, double Z) { return FVector(L.HX + X * C + Z * S, L.HZ - X * S + Z * C, L.HY + Y); };
	const FQuat Q(FVector::ZAxisVector, -H);
	auto Box = [&](double Wd, double Ht, double Dp, uint32 Col, double X, double Y, double Z, const FQuat& R = FQuat::Identity) { M.AddBox(W(X, Y + Ht / 2, Z), FVector(Wd, Dp, Ht), Hex(Col), Q * R); };
	auto Cyl = [&](double R0, double R1, double Ht, uint32 Col, double X, double Y, double Z, int32 Seg = 8) { M.AddTube(W(X, Y, Z), W(X, Y + Ht, Z), R1, R0, Seg, Hex(Col)); };
	auto Hip = [&](double Wd, double Dp, double Ht, uint32 Col, double Y, double Z)
	{
		const FVector Top = W(0, Y + Ht, Z);
		const FVector Cn[4] = {W(-Wd * .54, Y, Z - Dp * .54), W(Wd * .54, Y, Z - Dp * .54), W(Wd * .54, Y, Z + Dp * .54), W(-Wd * .54, Y, Z + Dp * .54)};
		for (int32 k = 0; k < 4; k++) M.AddTri(Cn[k], Cn[(k + 1) % 4], Top, Hex(Col), (Cn[k] + Cn[(k + 1) % 4]) * .5 - W(0, Y, Z));
	};
	auto Windows = [&](double Wd, double Z, double Y, int32 N, double WW = 1.4, double WH = 1.5) { for (int32 i = 0; i < N; i++) Box(WW, WH, .08, 0x1d3346, -Wd / 2 + Wd / N * (i + .5), Y - WH / 2, Z + .04); };
	const double Wd = L.HW, Dp = L.HD;
	double DoorTop = 3, FrontZ = Dp / 2;
	if (B.Res == TEXT("villa"))
	{
		Box(Wd + 1, 2.8, Dp + 1, 0x9a958a, 0, -2.5, 0);
		Box(Wd, 4, Dp, 0xf6efe2, 0, .3, 0);
		Windows(Wd, Dp / 2, 2.6, FMath::Max(3, FMath::RoundToInt(Wd / 5)), 1.6, 2.2);
		const double UW = Wd * .7, UD = Dp * .65;
		Box(UW, 3.6, UD, 0xf6efe2, 0, 4.3, -Dp * .12);
		Windows(UW, -Dp * .12 + UD / 2, 6, FMath::Max(2, FMath::RoundToInt(UW / 5)));
		Hip(UW, UD, 2.6, 0xc8643c, 7.9, -Dp * .12);
		Box(Wd, .3, Dp, 0xffffff, 0, 4.3, 0);
		Box(Wd, .9, .12, 0xffffff, 0, 4.6, Dp / 2 - .06);
		Box(Wd + 2, .35, 7, 0xb08a5e, 0, 0, Dp / 2 + 3.5);
		if (B.bPool) Box(Wd * .55, .12, 4.2, 0x3fb8d8, -Wd * .15, .3, Dp / 2 + 3.6);
		Box(2.2, 2.9, .12, 0x2a2d31, Wd * .3, .3, Dp / 2 + .06);
		for (double X : {-Wd / 2 + .4, Wd / 2 - .4}) Cyl(.25, .3, 4, 0xffffff, X, .3, Dp / 2 + .3);
		DoorTop = 3.6; FrontZ = Dp / 2 + .12;
	}
	else if (B.Res == TEXT("shack"))
	{
		Box(Wd, 2.8, Dp + 3, 0x5c4a38, 0, -2.5, 0);
		Box(Wd, 2.8, Dp, 0x8a6a48, 0, .3, 0);
		Windows(Wd, Dp / 2, 1.9, 2, 1, 1);
		Box(Wd + 1, .12, Dp + 3.4, 0xaeb4ba, 0, 3.6, 1.2, FQuat(FVector::XAxisVector, .16));
		Box(Wd, .2, 2.8, 0xb08a5e, 0, .3, Dp / 2 + 1.4);
		for (double X : {-Wd / 2 + .2, Wd / 2 - .2}) Box(.18, 2.6, .18, 0x5c4a38, X, .5, Dp / 2 + 2.6);
		Box(.9, .5, .5, 0xf4f4f2, Wd / 2 - 1.2, .5, Dp / 2 + 1.2);
		Box(.92, .1, .52, 0x2f8fd8, Wd / 2 - 1.2, 1.0, Dp / 2 + 1.2);
		Box(1.1, 2.2, .1, 0x5c4a38, .6, .3, Dp / 2 + .05);
		DoorTop = 2.7; FrontZ = Dp / 2 + .1;
	}
	else if (B.Res == TEXT("garden"))
	{
		const double GW = Wd * 2.4, GD = Dp * 2.4, Tz = -Dp * .35;
		Box(GW, .25, GD, 0x6e9a4a, 0, -.1, 0);
		Box(GW * .55, .06, GD * .38, 0xd9d4c4, GW * .18, .15, GD * .22);
		for (int32 i = 0; i < 7; i++) Box(GW * .52, .03, .08, 0x9a958a, GW * .18, .21, GD * .22 - GD * .17 + i * GD * .055);
		Box(1.4, .9, 1.1, 0x9a958a, GW * .3, .15, GD * .25);
		Box(GW * .3, .08, GW * .22, 0x2f6f5e, -GW * .24, .12, GD * .18);
		for (int32 i = 0; i < 5; i++) Box(.5, .06, .18, 0xf26b1d, -GW * .24 + FMath::Cos(i * 1.3) * GW * .08, .2, GD * .18 + FMath::Sin(i * 1.3) * GW * .05);
		for (int32 i = 0; i < 6; i++) Cyl(.45, .5, .12, 0x9a958a, -GW * .05 + i * .3, .12, GD * .38 - i * 1.4);
		Box(Wd + 2, .6, Dp + 2, 0x5c4a38, 0, 0, Tz);
		Box(Wd, 2.8, Dp, 0xffffff, 0, .6, Tz);
		for (double X : {-Wd / 2, -Wd / 6, Wd / 6, Wd / 2}) Box(.22, 2.8, .22, 0x5c4a38, X, .6, Tz + Dp / 2);
		Hip(Wd + 3, Dp + 3, 2.4, 0x3b3f45, 3.4, Tz);
		for (int32 s = -1; s <= 1; s += 2) Box(.9, 1.2, GD, 0x2f5a2a, s * (GW / 2 - .45), .1, 0);
		Box(GW, 1.2, .9, 0x2f5a2a, 0, .1, -GD / 2 + .45);
		DoorTop = 3.0; FrontZ = Tz + Dp / 2 + .12;
	}
	else if (B.Res == TEXT("stilt"))
	{
		const double Floor = 3.2;
		for (double X : {-Wd / 2 + .4, 0.0, Wd / 2 - .4}) for (double Z : {-Dp / 2 + .4, 0.0, Dp / 2 - .4}) Cyl(.22, .26, Floor + 4, 0x5c4a38, X, -4, Z, 7);
		Box(Wd + 3, .35, Dp + 3, 0xb08a5e, 0, Floor, 0);
		Box(Wd, 3, Dp, 0xb9d4f5, 0, Floor + .35, 0);
		Windows(Wd, Dp / 2, Floor + 2, 3, 1.2, 1.3);
		// a gable in tin
		const FVector A0 = W(-Wd / 2 - .3, Floor + 3.35, -Dp / 2 - .4), A1 = W(Wd / 2 + .3, Floor + 3.35, -Dp / 2 - .4), B0 = W(-Wd / 2 - .3, Floor + 3.35, Dp / 2 + .4), B1 = W(Wd / 2 + .3, Floor + 3.35, Dp / 2 + .4);
		const FVector R0 = W(-Wd / 2 - .3, Floor + 5.75, 0), R1 = W(Wd / 2 + .3, Floor + 5.75, 0);
		M.AddQuad(A0, A1, R1, R0, Hex(0xaeb4ba), W(0, 1, -1) - W(0, 0, 0));
		M.AddQuad(B0, R0, R1, B1, Hex(0xaeb4ba), W(0, 1, 1) - W(0, 0, 0));
		M.AddTri(A0, R0, B0, Hex(0xb9d4f5), W(-1, 0, 0) - W(0, 0, 0));
		M.AddTri(A1, B1, R1, Hex(0xb9d4f5), W(1, 0, 0) - W(0, 0, 0));
		for (double Z : {-(Dp + 3) / 2, (Dp + 3) / 2}) Box(Wd + 3, .9, .08, 0xffffff, 0, Floor + .35, Z);
		Box(1.4, .2, FMath::Sqrt(Floor * Floor + 25), 0xb08a5e, -Wd / 2 - 2.2, Floor / 2 - .1, Dp / 2 - 1, FQuat(FVector::YAxisVector, FMath::Atan2(Floor, 5.0)));
		Box(1.4, 2.4, .1, 0x2a2d31, Wd * .25, Floor + .35, Dp / 2 + .05);
		DoorTop = Floor + 2.9; FrontZ = Dp / 2 + .1;
	}
	else
	{
		Box(Wd + 1, 2.8, Dp + 1, 0xffffff, 0, -2.5, 0);
		Box(Wd * .35, 3.4, Dp * .4, 0x2a2d31, -Wd * .2, .3, -Dp * .25);
		Box(Wd, 3.4, Dp, 0x8fc4dc, 0, .3, 0);
		Box(Wd + 3, .45, Dp + 2.5, 0xffffff, 0, 3.7, .6);
		Box(Wd * .55, 2.8, Dp * .75, 0x8fc4dc, Wd * .15, 4.15, -Dp * .08);
		Box(Wd * .55 + 1.5, .35, Dp * .75 + 1.5, 0xffffff, Wd * .15, 6.95, -Dp * .08);
		for (double X : {-Wd / 2, 0.0, Wd / 2}) Box(.14, 3.4, .14, 0xffffff, X, .3, Dp / 2);
		Box(Wd + 2, .45, 7, 0xffffff, 0, 0, Dp / 2 + 3.5);
		if (B.bPool) Box(Wd * .8, .08, 5, 0x3fb8d8, 0, .45, Dp / 2 + 4);
		DoorTop = 3.4; FrontZ = Dp / 2 + .1;
	}
	const KeysData::FSpecies* Sp = KeysData::Species(B.Fish);
	const FString Label = FString::Printf(TEXT("%s\n%.1f LB"), Sp ? *Sp->Name.ToUpper() : *B.Fish.ToUpper(), B.Lb);
	const double LenM = KeysMesh::FishLength(B.Lb);
	if (B.Mount == TEXT("door"))
	{
		const double Ln = FMath::Min(LenM, 4.5), Bh = FMath::Max(.8, Ln * .32);
		Box(Ln + .6, Bh, .1, 0x5c4a38, 0, DoorTop + .3, FrontZ + .05);
		Fish(M, B.Fish, Ln, FTransform(FQuat(FVector::ZAxisVector, -H), W(-Ln / 2, DoorTop + .3 + Bh / 2, FrontZ + .3)));
		Signs.Add(TPair<FVector, FString>(W(0, DoorTop + .3 + Bh + .5, FrontZ + .3), Label));
	}
	// the dock: a ramp off the shore, the deck, pilings every 4 m, the T-head with its helipad
	if (L.bDock)
	{
		const double DH = FMath::Atan2(L.SX, L.SZ), DC = FMath::Cos(DH), DS = FMath::Sin(DH);
		auto DW = [&](double X, double Y, double Z) { return FVector(L.DX0 + X * DC + Z * DS, L.DZ0 - X * DS + Z * DC, Y); };
		const FQuat DQ(FVector::ZAxisVector, -DH);
		auto DBox = [&](double Wd2, double Ht, double Dp2, uint32 Col, double X, double Y, double Z, const FQuat& R = FQuat::Identity) { M.AddBox(DW(X, Y + Ht / 2, Z), FVector(Wd2, Dp2, Ht), Hex(Col), DQ * R); };
		const double G0 = FMath::Max(.3, LandH(L.DX0, L.DZ0));
		// the ramp tilt: along the dock's own axis
		{
			const FVector A = DW(-L.DW / 2, G0, 0), Bq = DW(L.DW / 2, G0, 0), Cq = DW(L.DW / 2, DockTop, DockRamp), Dq = DW(-L.DW / 2, DockTop, DockRamp);
			M.AddQuad(A, Bq, Cq, Dq, Hex(0xb08a5e), FVector(0, 0, 1));
		}
		DBox(L.DW, .3, L.DLen - DockRamp, 0xb08a5e, 0, DockTop - .3, DockRamp + (L.DLen - DockRamp) / 2);
		for (double Z = DockRamp; Z <= L.DLen; Z += 4) for (int32 s = -1; s <= 1; s += 2) M.AddTube(DW(s * (L.DW / 2 + .05), -3, Z), DW(s * (L.DW / 2 + .05), DockTop + .6, Z), .17, .17, 6, Hex(0x5c4a38));
		if (L.THead > 0)
		{
			DBox(L.THead, .3, 5, 0xb08a5e, 0, DockTop - .3, L.DLen - 2.5);
			const double HX2 = -L.THead / 2 - 6.5, HZ2 = L.DLen - 2.5;
			DBox(12, .4, 12, 0x2e3238, HX2, DockTop - .4, HZ2);
			for (int32 k = 0; k < 24; k++)
			{
				const double A0 = k * PI / 12, A1 = (k + 1) * PI / 12;
				M.AddTube(DW(HX2 + FMath::Cos(A0) * 3.85, DockTop + .02, HZ2 + FMath::Sin(A0) * 3.85), DW(HX2 + FMath::Cos(A1) * 3.85, DockTop + .02, HZ2 + FMath::Sin(A1) * 3.85), .12, .12, 4, Hex(0xf4f4f2));
			}
			for (int32 o = -1; o <= 1; o += 2) DBox(.4, .03, 3, 0xf4f4f2, HX2 + o, DockTop, HZ2);
			DBox(2, .03, .4, 0xf4f4f2, HX2, DockTop, HZ2);
		}
		if (B.Dock != TEXT("skiff")) DBox(1.4, .7, .7, 0xffffff, L.DW / 2 - .5, DockTop, DockRamp + 2);
	}
	// the catch on a gallows by the dock, or on a pedestal in the garden
	if (B.Mount == TEXT("gallows") || B.Mount == TEXT("pedestal"))
	{
		const double Land = LandH(L.MX, L.MZ);
		const double Base = B.Mount == TEXT("gallows") && Land < .3 ? DockTop : FMath::Max(.3, Land);
		const FVector At(L.MX, L.MZ, Base);
		if (B.Mount == TEXT("gallows"))
		{
			const double Top = FMath::Max(3.6, LenM + 2.2), Half = FMath::Max(1.1, LenM * .35);
			const FVector Al = FVector(-L.SZ, L.SX, 0);
			M.AddTube(At - Al * Half, At - Al * Half + FVector(0, 0, Top), .09, .09, 6, Hex(0x8a6a48));
			M.AddTube(At + Al * Half, At + Al * Half + FVector(0, 0, Top), .09, .09, 6, Hex(0x8a6a48));
			M.AddTube(At - Al * (Half + .15) + FVector(0, 0, Top), At + Al * (Half + .15) + FVector(0, 0, Top), .08, .08, 6, Hex(0x8a6a48));
			M.AddTube(At + FVector(0, 0, Top - .36), At + FVector(0, 0, Top - .6), .01, .01, 3, Hex(0xd9dde2));
			M.AddBox(At + FVector(0, 0, Top - .3), FVector(.12, .22, .22), Hex(0xc8102e));
			// hung by the tail, nose down
			Fish(M, B.Fish, LenM, FTransform(FQuat(FVector::ZAxisVector, FMath::Atan2(L.SZ, L.SX)) * FQuat(FVector::YAxisVector, PI / 2), At + FVector(0, 0, Top - .6)));
			Signs.Add(TPair<FVector, FString>(At + FVector(0, 0, Top + .7), Label));
		}
		else
		{
			M.AddBox(At + FVector(0, 0, .15), FVector(1.1, 1.1, .3), Hex(0x9a958a));
			M.AddTube(At + FVector(0, 0, .3), At + FVector(0, 0, 1.6), .38, .3, 8, Hex(0x9a958a));
			M.AddTube(At + FVector(0, 0, 1.6), At + FVector(0, 0, 2.0), .03, .03, 4, Hex(0xd9dde2));
			Fish(M, B.Fish, LenM, FTransform(FQuat(FVector::ZAxisVector, FMath::Atan2(-L.SX, L.SZ)) * FQuat(FVector::YAxisVector, -.35), At + FVector(-LenM * .45 * -L.SZ, -LenM * .45 * L.SX, 2.0 + LenM * .12)));
			Signs.Add(TPair<FVector, FString>(At + FVector(0, 0, 1.0) + FVector(L.SX, L.SZ, 0) * .45, Label));
		}
	}
	// trophies round the grounds: leaping fish on posts
	for (int32 i = 0; i < L.Trophies.Num(); i++)
	{
		const FVector2D& Tp = L.Trophies[i];
		const double G = P.Tier == ETier::Harbor ? KeysTown::PadY : FMath::Max(.3, LandH(Tp.X, Tp.Y));
		const FString K = TrophySpecies[i % 10];
		M.AddBox(FVector(Tp.X, Tp.Y, G + .2), FVector(.6, .6, .4), Hex(0x9a958a));
		M.AddTube(FVector(Tp.X, Tp.Y, G + .4), FVector(Tp.X, Tp.Y, G + 1.4), .04, .04, 4, Hex(0xd9dde2));
		Fish(M, K, 1.2, FTransform(FQuat(FVector::ZAxisVector, i * 1.1) * FQuat(FVector::YAxisVector, -.5), FVector(Tp.X, Tp.Y, G + 1.4) - FQuat(FVector::ZAxisVector, i * 1.1).RotateVector(FVector(.55, 0, 0))));
	}
	// the pool (the villa and glass houses carry theirs on the deck) and the fire pit
	if (L.bPool && B.Res != TEXT("villa") && B.Res != TEXT("glass"))
	{
		const double G = P.Tier == ETier::Harbor ? KeysTown::PadY : FMath::Max(.3, LandH(L.PX, L.PZ));
		M.AddBox(FVector(L.PX, L.PZ, G + .1), FVector(9, 5, .25), Hex(0xf2efe6), Q);
		M.AddBox(FVector(L.PX, L.PZ, G + .2), FVector(8, 4, .1), Hex(0x3fb8d8), Q);
	}
	if (L.bFire)
	{
		const double G = P.Tier == ETier::Harbor ? KeysTown::PadY : FMath::Max(.3, LandH(L.FX, L.FZ));
		for (int32 k = 0; k < 10; k++) M.AddBox(FVector(L.FX + FMath::Cos(k * PI / 5) * .9, L.FZ + FMath::Sin(k * PI / 5) * .9, G + .15), FVector(.4, .3, .3), Hex(0x9a958a), FQuat(FVector::ZAxisVector, k * PI / 5));
		for (int32 k = 0; k < 4; k++) M.AddTube(FVector(L.FX - .5, L.FZ + (k - 1.5) * .2, G + .15), FVector(L.FX + .5, L.FZ - (k - 1.5) * .2, G + .2), .07, .07, 5, Hex(0x5c4a38));
		KeysShapes::AddBlob(M, FVector(L.FX, L.FZ, G + .2), FVector(.35, .35, .5), FQuat::Identity, Hex(0xff9a3a), 6, 3, 0);
	}
}

// ================================================================ holdings and the land office
FString AKeysEstates::Buy(const FString& Id)
{
	AKeysBoatPawn* P = Cast<AKeysBoatPawn>(UGameplayStatics::GetPlayerPawn(this, 0));
	const FParcel* Pc = ById(Id);
	if (!P || !Pc) return TEXT("There is no such lot.");
	if (P->Estates.Contains(Id)) return FString::Printf(TEXT("%s is already yours."), *Pc->Name);
	if (!Founder(Id).IsEmpty()) return TEXT("That lot belongs to a founder. It is not for sale.");
	if (P->Estates.Num() >= MaxLots) return FString::Printf(TEXT("You already hold %d lots, the most one angler may."), MaxLots);
	const int64 Price = PriceOf(*Pc);
	if (P->Cash < Price) return FString::Printf(TEXT("You need %s more for %s."), *Money(double(Price - P->Cash)), *Pc->Name);
	P->Estates.Add(Id);
	if (!P->EstateBuilds.Contains(Id)) P->EstateBuilds.Add(Id, BuildToString(DefaultBuild(*Pc)));
	P->Debit(Price);
	if (AKeysTown* Tn = AKeysTown::Get(this)) Tn->SetOwnedEstates(P->Estates);
	bDirty = true;
	return FString::Printf(TEXT("%s is yours."), *Pc->Name);
}

void AKeysEstates::SetBuild(const FString& Id, const FBuild& B)
{
	AKeysBoatPawn* P = Cast<AKeysBoatPawn>(UGameplayStatics::GetPlayerPawn(this, 0));
	const FParcel* Pc = ById(Id);
	if (!P || !Pc) return;
	P->EstateBuilds.Add(Id, BuildToString(BuildFromString(*Pc, BuildToString(B))));
	P->SaveEconomy();
	bDirty = true;
}

void AKeysEstates::Refresh() { bDirty = true; }

void AKeysEstates::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	AKeysBoatPawn* P = Cast<AKeysBoatPawn>(UGameplayStatics::GetPlayerPawn(this, 0));
	if (!P) return;
	// what is held: yours, and the founders' lots, each with its build
	TArray<FString> Held = P->Estates;
	for (const FString& F : {FString(TEXT("pigeon-key")), FString(TEXT("harbor-row-1")), FString(TEXT("fisher-island")), FString(TEXT("harbor-row-4"))}) Held.AddUnique(F);
	FString Sig;
	for (const FString& Id : Held) { Sig += Id; if (const FString* S = P->EstateBuilds.Find(Id)) Sig += *S; }
	if (!bDirty && Sig == Built) return;
	bDirty = false; Built = Sig;
	for (UProceduralMeshComponent* C : LotMeshes) if (C) C->DestroyComponent();
	for (UTextRenderComponent* T : LotSigns) if (T) T->DestroyComponent();
	LotMeshes.Reset(); LotSigns.Reset();
	KeysTown::ExtraDocks().Reset();
	for (const FString& Id : Held)
	{
		const FParcel* Pc = ById(Id);
		if (!Pc) continue;
		const FBuild B = BuildOf(Id);
		FKeysMesh M;
		TArray<TPair<FVector, FString>> Signs;
		BuildLot(*Pc, B, M, Signs);
		UProceduralMeshComponent* C = NewObject<UProceduralMeshComponent>(this);
		C->SetupAttachment(Root);
		C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		C->RegisterComponent();
		C->SetWorldLocation(FVector::ZeroVector);
		M.Commit(C, 0, AKeysWorld::VertexColorMaterial());
		C->SetCastShadow(true);
		LotMeshes.Add(C);
		for (const TPair<FVector, FString>& Sg : Signs)
		{
			UTextRenderComponent* T = NewObject<UTextRenderComponent>(this);
			T->SetupAttachment(Root);
			T->RegisterComponent();
			T->SetText(FText::FromString(Sg.Value));
			T->SetHorizontalAlignment(EHTA_Center);
			T->SetVerticalAlignment(EVRTA_TextCenter);
			T->SetTextRenderColor(FColor(30, 24, 18));
			T->SetWorldSize(32);
			const FLayout L = Layout(*Pc, B);
			T->SetWorldLocationAndRotation(Sg.Key * 100, FRotator(0, FMath::RadiansToDegrees(FMath::Atan2(L.SZ, L.SX)), 0));
			LotSigns.Add(T);
		}
		// your dock is somewhere to tie up and walk ashore (estates.ts estateDockBerth / estateGround)
		const FLayout L = Layout(*Pc, B);
		if (L.bDock) KeysTown::ExtraDocks().Add({L.DX0, L.DZ0, L.SX, L.SZ, L.DLen, L.DW / 2, L.THead, FMath::Max(.3, LandH(L.DX0, L.DZ0)), Pc->Name + TEXT(" - your dock")});
	}
}

// ================================================================ the panel
void AKeysEstates::SetRegion(int32 R)
{
	Region = (R + RegionNames().Num()) % RegionNames().Num();
	const TArray<FString> Marathon = {TEXT("Marathon (Vaca Key)"), TEXT("Boot Key"), TEXT("Key Colony Beach"), TEXT("Fat Deer Key"), TEXT("Crawl Key"), TEXT("Grassy Key"), TEXT("Knight's Key")};
	const TArray<FString> Seven = {TEXT("Little Duck Key"), TEXT("Pigeon Key"), TEXT("Knight's Key"), TEXT("Molasses Keys")};
	const KeysGeo::FMarina& TM = Marinas()[0];
	RegionList.Reset();
	for (int32 i = 0; i < All.Num(); i++)
	{
		const FParcel& P = All[i];
		bool bIn = true;
		switch (Region)
		{
			case 1: bIn = Marathon.Contains(P.Island); break;
			case 2: bIn = Seven.Contains(P.Island); break;
			case 3: bIn = P.Tier == ETier::Biscayne || P.Tier == ETier::Mainland || (P.Tier == ETier::Private && P.CZ < -1500); break;
			case 4: bIn = P.Island == TEXT("Boot Key"); break;
			case 5: bIn = P.Tier == ETier::Harbor || (P.Island == TEXT("Boot Key") && FMath::Sqrt(FMath::Square(P.CX - TM.SX) + FMath::Square(P.CZ - TM.SZ)) < 500); break;
			default: break;
		}
		if (bIn) RegionList.Add(i);
	}
	RegionList.Sort([this](int32 A, int32 B) { return All[A].CX < All[B].CX; });
	Sel = FMath::Clamp(Sel, 0, FMath::Max(0, RegionList.Num() - 1));
	SheetRow = -1;
}

void AKeysEstates::RegionBox(double& X0, double& Z0, double& Size) const
{
	double A = 1e18, B = -1e18, C = 1e18, D = -1e18;
	for (int32 i : RegionList) { const FParcel& P = All[i]; A = FMath::Min(A, P.X0); B = FMath::Max(B, P.X1); C = FMath::Min(C, P.Z0); D = FMath::Max(D, P.Z1); }
	if (!RegionList.Num()) { A = -4000; B = 4000; C = -3000; D = 3000; }
	const double W = B - A, H = D - C, Pad = FMath::Max(W, H) * .12 + 40;
	Size = FMath::Max(W, H) + 2 * Pad;
	X0 = (A + B) / 2 - Size / 2; Z0 = (C + D) / 2 - Size / 2;
}

void AKeysEstates::Open(bool bOn)
{
	bOpen = bOn;
	SheetRow = -1;
	if (bOn)
	{
		// start on the lot nearest the boat
		const AKeysBoatPawn* P = Cast<AKeysBoatPawn>(UGameplayStatics::GetPlayerPawn(this, 0));
		if (P && RegionList.Num())
		{
			double BD = 1e18;
			for (int32 k = 0; k < RegionList.Num(); k++)
			{
				const FParcel& Pc = All[RegionList[k]];
				const double D = FMath::Square(Pc.CX - P->BoatX()) + FMath::Square(Pc.CZ - P->BoatZ());
				if (D < BD) { BD = D; Sel = k; }
			}
		}
	}
}

void AKeysEstates::HandleInput(APlayerController* PC)
{
	auto Hit = [PC](const FKey& K) { return PC->WasInputKeyJustPressed(K); };
	AKeysBoatPawn* P = Cast<AKeysBoatPawn>(UGameplayStatics::GetPlayerPawn(this, 0));
	if (!P) return;
	if (Hit(EKeys::Escape) || Hit(EKeys::Comma)) { Open(false); return; }
	const FParcel* Pc = Selected();
	const bool bMine = Pc && P->Estates.Contains(Pc->Id);
	const bool bLeft = Hit(EKeys::Left) || Hit(EKeys::A), bRight = Hit(EKeys::Right) || Hit(EKeys::D);
	const bool bUp = Hit(EKeys::Up) || Hit(EKeys::W), bDown = Hit(EKeys::Down) || Hit(EKeys::S);
	if (Hit(EKeys::Tab) && bMine) SheetRow = SheetRow < 0 ? 0 : -1;
	if (SheetRow < 0)
	{
		const int32 N = FMath::Max(1, RegionList.Num());
		if (bLeft) SetRegion(Region - 1);
		if (bRight) SetRegion(Region + 1);
		if (bUp) Sel = (Sel + N - 1) % N;
		if (bDown) Sel = (Sel + 1) % N;
		if (Hit(EKeys::PageUp)) Sel = (Sel + N - 10 % N) % N;
		if (Hit(EKeys::PageDown)) Sel = (Sel + 10) % N;
		if (Hit(EKeys::Enter) && Pc)
		{
			if (bMine) SheetRow = 0;
			else P->ToastMsg(Buy(Pc->Id), 4);
		}
		return;
	}
	if (!bMine) { SheetRow = -1; return; }
	const bool bHarbor = Pc->Tier == ETier::Harbor;
	TArray<int32> Rows = {0, 1, 2, 3, 4, 5, 6, 7};
	if (bHarbor) Rows.Remove(1);
	int32 RI = FMath::Max(0, Rows.IndexOfByKey(SheetRow));
	if (bUp) { if (RI == 0) { SheetRow = -1; return; } RI--; }
	if (bDown) RI = FMath::Min(Rows.Num() - 1, RI + 1);
	SheetRow = Rows[RI];
	const int32 Dir = bLeft ? -1 : bRight ? 1 : (Hit(EKeys::Enter) || Hit(EKeys::SpaceBar)) ? 1 : 0;
	if (!Dir) return;
	FBuild B = BuildOf(Pc->Id);
	auto Cycle = [Dir](int32 I, int32 N) { return (I + Dir + N) % N; };
	switch (SheetRow)
	{
		case 0: { int32 I = 0; for (int32 k = 0; k < 5; k++) if (B.Res == Residences[k].Key) I = k; B.Res = Residences[Cycle(I, 5)].Key; break; }
		case 1: { int32 I = 0; for (int32 k = 0; k < 4; k++) if (B.Dock == Docks[k].Key) I = k; B.Dock = Docks[Cycle(I, 4)].Key; break; }
		case 2:
		{
			// the species, heaviest first, and the weight goes to four fifths of its record
			TArray<FString> Keys = KeysData::SpeciesKeys();
			Keys.Sort([](const FString& A, const FString& Bk) { return KeysData::Species(A)->Max > KeysData::Species(Bk)->Max; });
			const int32 I = FMath::Max(0, Keys.IndexOfByKey(B.Fish));
			B.Fish = Keys[Cycle(I, Keys.Num())];
			B.Lb = FMath::RoundToDouble(KeysData::Species(B.Fish)->Max * .8);
			break;
		}
		case 3: { const double Mx = KeysData::Species(B.Fish) ? KeysData::Species(B.Fish)->Max : 100; B.Lb = FMath::Clamp(FMath::RoundToDouble(B.Lb + Dir * FMath::Max(1.0, Mx / 20)), 1.0, Mx); break; }
		case 4:
		{
			TArray<FString> Ms = {TEXT("door"), TEXT("gallows"), TEXT("pedestal")};
			if (bHarbor || B.Dock == TEXT("none")) Ms.Remove(TEXT("gallows"));
			B.Mount = Ms[Cycle(FMath::Max(0, Ms.IndexOfByKey(B.Mount)), Ms.Num())];
			break;
		}
		case 5: B.Trophies = FMath::Clamp(B.Trophies + Dir, 0, MaxTrophies); break;
		case 6: B.bPool = !B.bPool; break;
		case 7: B.bFire = !B.bFire; break;
		default: break;
	}
	SetBuild(Pc->Id, B);
}

namespace KeysEstatesText
{
	FString ResLabel(const FString& K) { const FRes& R = ResOf(K); return FString::Printf(TEXT("%s - %s"), R.Label, R.Note); }
	FString DockLabel(const FString& K) { const FDockOpt& D = DockOf(K); return FString::Printf(TEXT("%s - %s"), D.Label, D.Note); }
	FString MountLabel(const FString& K) { return K == TEXT("gallows") ? TEXT("Dock weigh-station gallows") : K == TEXT("pedestal") ? TEXT("Garden pedestal") : TEXT("Over the front door"); }
}
