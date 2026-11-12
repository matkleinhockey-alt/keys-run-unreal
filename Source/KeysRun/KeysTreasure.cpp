#include "KeysTreasure.h"
#include "KeysKit.h"
#include "KeysGeo.h"
#include "KeysSea.h"
#include "KeysMesh.h"
#include "KeysWrecks.h"
#include "KeysHuman.h"
#include "KeysWorld.h"
#include "KeysAudio.h"
#include "KeysDiver.h"
#include "KeysBoatPawn.h"
#include "ProceduralMeshComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/World.h"
#include "EngineUtils.h"

using namespace KeysGeo;
using namespace KeysKit;

namespace
{
	constexpr uint32 Seed = 20240817;
	constexpr int32 SaltKind = 7701, SaltU = 7702, SaltSide = 7703, SaltOff = 7704, SaltYaw = 7705, SaltBuried = 7706, SaltCash = 7710, SaltAmmo = 7711, SaltLabel = 7712, SaltCrateAmmo = 7713;
	constexpr double MaxSiteRelief = .6, MaxSiteRange = .9, SiteRing = 1.6, MinSeparation = 6, ReachM = 3.5, NoticeM = 14;
	constexpr int32 SiteAttempts = 26;
	double H(int32 W, int32 I, int32 Salt) { return KeysSea::HashCell(Seed, W, I, Salt); }

	FVector2D WreckPoint(const KeysWrecks::FWreck& W, double U, double V)
	{
		const FVector2D F(-FMath::Sin(W.Heading), -FMath::Cos(W.Heading));
		const double RX = -F.Y, RZ = F.X;
		double A = U * W.Len, Side = V * W.Beam;
		if (W.bBroken && U < 0)
		{
			const double C = FMath::Cos(W.BreakYaw), S = FMath::Sin(W.BreakYaw), Along = A, Lat = Side;
			A = Along * C - Lat * S - W.Gap; Side = Along * S + Lat * C;
		}
		return FVector2D(W.X + F.X * A + RX * Side, W.Z + F.Y * A + RZ * Side);
	}

	void SiteCost(double X, double Z, double& OutCost)
	{
		const double R0 = KeysSea::ReliefAt(X, Z, DepthAt(X, Z));
		double Lo = R0, Hi = R0;
		for (int32 a = 0; a < 4; a++)
		{
			const double PX = X + FMath::Cos(a * PI / 2) * SiteRing, PZ = Z + FMath::Sin(a * PI / 2) * SiteRing;
			const double R = KeysSea::ReliefAt(PX, PZ, DepthAt(PX, PZ));
			Lo = FMath::Min(Lo, R); Hi = FMath::Max(Hi, R);
		}
		const double Range = Hi - Lo;
		OutCost = FMath::Max(0.0, FMath::Abs(R0) - MaxSiteRelief) + FMath::Max(0.0, Range - MaxSiteRange) + Range * .25 + FMath::Abs(R0) * .25;
	}

	double Stands(AKeysTreasure::EKind K) { return K == AKeysTreasure::EKind::Strongbox ? .45 : K == AKeysTreasure::EKind::Coins ? .25 : K == AKeysTreasure::EKind::Crate ? .7 : .3; }
}

AKeysTreasure::AKeysTreasure()
{
	PrimaryActorTick.bCanEverTick = true;
	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent = Root;
	Mesh = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("Treasure"));
	Mesh->SetupAttachment(Root);
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

AKeysTreasure* AKeysTreasure::Get(const UObject* WorldContext)
{
	UWorld* W = WorldContext ? WorldContext->GetWorld() : nullptr;
	if (!W) return nullptr;
	TActorIterator<AKeysTreasure> It(W);
	return It ? *It : nullptr;
}

void AKeysTreasure::BeginPlay()
{
	Super::BeginPlay();
	SetActorLocation(FVector::ZeroVector);
	// containersFor: the strongbox first, then hashed kinds; each site the best of 26 tries for level sand beside the hull
	const TArray<KeysWrecks::FWreck>& Ws = KeysWrecks::Wrecks();
	for (int32 w = 0; w < Ws.Num(); w++)
	{
		const KeysWrecks::FWreck& W = Ws[w];
		const int32 N = FMath::Min(5, 2 + int32(FMath::FloorToDouble(W.Mass)));
		TArray<FBox> Out;
		for (int32 i = 0; i < N; i++)
		{
			EKind Kind = EKind::Strongbox;
			if (i > 0)
			{
				const double R = H(w, i, SaltKind) * 7.1;
				Kind = R < 3 ? EKind::Coins : R < 5.5 ? EKind::Crate : EKind::AmmoCan;
			}
			double BX = W.X, BZ = W.Z, BS = 3, BC = 1e18;
			for (int32 k = 0; k < SiteAttempts; k++)
			{
				const int32 C = i * 32 + k;
				const double U = -.46 + .92 * ((i + H(w, C, SaltU)) / N);
				const double Side = H(w, C, SaltSide) < .5 ? -1 : 1;
				const double Standoff = 3 + 8 * H(w, C, SaltOff);
				const FVector2D P = WreckPoint(W, U, Side * (.5 + Standoff / W.Beam));
				double Sep = 1e18;
				for (const FBox& Q : Out) Sep = FMath::Min(Sep, FMath::Sqrt(FMath::Square(P.X - Q.X) + FMath::Square(P.Y - Q.Z)));
				double Cost;
				SiteCost(P.X, P.Y, Cost);
				const double Total = Cost + FMath::Max(0.0, MinSeparation - Sep);
				if (Total < BC) { BC = Total; BX = P.X; BZ = P.Y; BS = Standoff; }
				if (Total <= 0) break;
			}
			FBox B;
			B.Id = FString::Printf(TEXT("%s:%d"), W.Id, i); B.Wreck = w; B.Index = i; B.Kind = Kind; B.X = BX; B.Z = BZ; B.Standoff = BS;
			B.Yaw = H(w, i, SaltYaw) * PI * 2;
			B.Buried = Kind == EKind::Coins ? .25 + .25 * H(w, i, SaltBuried) : .2 + .35 * H(w, i, SaltBuried);
			Out.Add(B);
		}
		if (N >= 3 && !Out.ContainsByPredicate([](const FBox& B) { return B.Kind == EKind::AmmoCan; })) Out.Last().Kind = EKind::AmmoCan;
		All.Append(Out);
	}
	int32 Cash = 0;
	for (const FBox& B : All) Cash += LootOf(B).Cash;
	UE_LOG(LogTemp, Log, TEXT("Keys Run: %d treasure containers on the wrecks, $%d in all"), All.Num(), Cash);
}

AKeysTreasure::FLoot AKeysTreasure::LootOf(const FBox& B) const
{
	static const int32 Base[4] = {600, 345, 215, 80};
	const KeysWrecks::FWreck& W = KeysWrecks::Wrecks()[B.Wreck];
	const double Jitter = .8 + .45 * H(B.Wreck, B.Index, SaltCash), DepthMul = .6 + W.SeaDepth / 28;
	FLoot L;
	L.Cash = FMath::RoundToInt(Base[int32(B.Kind)] * DepthMul * Jitter);
	const double Ah = H(B.Wreck, B.Index, SaltAmmo);
	L.Ammo = B.Kind == EKind::AmmoCan ? 16 + int32(Ah * 20) : B.Kind == EKind::Crate && H(B.Wreck, B.Index, SaltCrateAmmo) < .34 ? 4 + int32(Ah * 7) : 0;
	static const TArray<FString> Coin = {TEXT("a fistful of silver reales, fused into a green brick"), TEXT("a sock of silver coins the sand had kept"), TEXT("a spill of coins and ballast stone")};
	static const TArray<FString> Box = {TEXT("the purser's strongbox, hinges gone"), TEXT("a bronze-bound cash box"), TEXT("the ship's box: coin, a gold chain, a sodden ledger")};
	static const TArray<FString> Crate = {TEXT("a burst crate of brass fittings"), TEXT("a crate of ship's tools, one sextant surviving"), TEXT("a crate of copper sheathing")};
	static const TArray<FString> Can = {TEXT("a steel ammunition can, half the rounds still dry"), TEXT("an ammo can, verdigris an inch thick")};
	const TArray<FString>& Ls = B.Kind == EKind::Strongbox ? Box : B.Kind == EKind::Coins ? Coin : B.Kind == EKind::Crate ? Crate : Can;
	L.Label = Ls[int32(H(B.Wreck, B.Index, SaltLabel) * Ls.Num()) % Ls.Num()];
	return L;
}

void AKeysTreasure::Rebuild()
{
	const AKeysBoatPawn* P = Cast<AKeysBoatPawn>(UGameplayStatics::GetPlayerPawn(this, 0));
	FKeysMesh M;
	for (const FBox& B : All)
	{
		const bool bOpen = P && P->TreasureOpened.Contains(B.Id);
		const double Tall = Stands(B.Kind), Floor = KeysSea::SeafloorHeightAt(B.X, B.Z), Y = Floor - B.Buried * Tall;
		const FQuat Q(FVector::ZAxisVector, B.Yaw);
		const FTransform Xf(Q, ToUE(B.X, Y, B.Z) / 100.0);
		auto Bx = [&](const FVector& C, const FVector& S, uint32 Col, const FQuat& R = FQuat::Identity) { M.AddBox(Xf.TransformPosition(C), S, Hex(Col), Q * R); };
		switch (B.Kind)
		{
			case EKind::Strongbox:
				// iron-bound, crusted; open, the lid stands back and it is empty
				Bx(FVector(0, 0, .17), FVector(.62, .42, .34), 0x5a4632);
				for (double X : {-.22, .0, .22}) Bx(FVector(X, 0, .17), FVector(.05, .44, .36), 0x8a6a2a);
				if (bOpen) Bx(FVector(-.2, 0, .45), FVector(.62, .42, .06), 0x5a4632, FQuat(FVector::YAxisVector, 1.3));
				else { Bx(FVector(0, 0, .38), FVector(.64, .44, .08), 0x4a3828); Bx(FVector(.32, 0, .25), FVector(.04, .1, .12), 0xb08a3a); }
				break;
			case EKind::Coins:
				if (!bOpen)
				{
					KeysShapes::AddBlob(M, Xf.TransformPosition(FVector(0, 0, .02)), FVector(.5, .4, .22), Q, Hex(0x6f8a6a), 8, 3, 0);
					for (int32 k = 0; k < 9; k++) M.AddTube(Xf.TransformPosition(FVector(FMath::Sin(k * 2.4) * .35, FMath::Cos(k * 1.7) * .28, .12 + (k % 3) * .03)), Xf.TransformPosition(FVector(FMath::Sin(k * 2.4) * .35, FMath::Cos(k * 1.7) * .28, .135 + (k % 3) * .03)), .035, .035, 8, Hex(k % 2 ? 0xc8c0a0 : 0xa8b090));
				}
				else KeysShapes::AddBlob(M, Xf.TransformPosition(FVector(0, 0, -.05)), FVector(.5, .4, .12), Q, Hex(0xb8ae90), 8, 3, 0);
				break;
			case EKind::Crate:
				Bx(FVector(0, 0, .35), FVector(.9, .7, bOpen ? .55 : .7), 0x6a5236);
				for (double Z : {.12, .58}) Bx(FVector(0, 0, Z), FVector(.92, .72, .06), 0x4e3c28);
				if (bOpen) Bx(FVector(.55, 0, .05), FVector(.9, .7, .05), 0x6a5236, FQuat(FVector::XAxisVector, .2));
				break;
			case EKind::AmmoCan:
				Bx(FVector(0, 0, .15), FVector(.32, .17, .3), 0x3e4a2c);
				if (!bOpen) { Bx(FVector(0, 0, .31), FVector(.33, .18, .03), 0x34401f); Bx(FVector(0, 0, .35), FVector(.12, .03, .03), 0x2a2f22); }
				else Bx(FVector(-.12, 0, .4), FVector(.33, .18, .03), 0x34401f, FQuat(FVector::YAxisVector, 1.5));
				break;
		}
	}
	M.Commit(Mesh, 0, AKeysWorld::VertexColorMaterial());
	bDirty = false;
}

void AKeysTreasure::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (bDirty) Rebuild();
	Prompt.Empty();
	bPromptKey = false;
	Reach = -1;
	const AKeysDiver* D = AKeysDiver::Get(this);
	const AKeysBoatPawn* P = Cast<AKeysBoatPawn>(UGameplayStatics::GetPlayerPawn(this, 0));
	if (!D || !D->IsDiving() || !P) return;
	const KeysDive::FState& S = D->State();
	// nearestUnopened within NOTICE_M, measured to the sea floor under the box
	double Best = NoticeM;
	for (int32 i = 0; i < All.Num(); i++)
	{
		const FBox& B = All[i];
		if (P->TreasureOpened.Contains(B.Id)) continue;
		const double Flat = FMath::Sqrt(FMath::Square(S.X - B.X) + FMath::Square(S.Z - B.Z));
		if (Flat >= Best) continue;
		const double Dd = FMath::Sqrt(Flat * Flat + FMath::Square(S.Y - KeysSea::SeafloorHeightAt(B.X, B.Z)));
		if (Dd < Best) { Best = Dd; Reach = i; ReachDist = Dd; }
	}
	if (Reach < 0) return;
	const FBox& B = All[Reach];
	const TCHAR* What = B.Kind == EKind::Strongbox ? TEXT("strongbox") : B.Kind == EKind::Coins ? TEXT("coin spill") : B.Kind == EKind::Crate ? TEXT("cargo crate") : TEXT("ammunition can");
	if (ReachDist <= ReachM) { Prompt = FString::Printf(TEXT("F  Prise open the %s"), What); bPromptKey = true; }
	else
	{
		const TCHAR* Far = B.Kind == EKind::Coins ? TEXT("Coins in the sand") : B.Kind == EKind::AmmoCan ? TEXT("An ammunition can") : B.Kind == EKind::Crate ? TEXT("A cargo crate") : TEXT("A strongbox");
		Prompt = FString::Printf(TEXT("%s, %d m"), Far, FMath::RoundToInt(ReachDist));
	}
}

bool AKeysTreasure::TryOpen()
{
	AKeysBoatPawn* P = Cast<AKeysBoatPawn>(UGameplayStatics::GetPlayerPawn(this, 0));
	if (!P) return false;
	if (Reach < 0) { P->ToastMsg(TEXT("Nothing within reach down here.")); return false; }
	if (ReachDist > ReachM) { P->ToastMsg(FString::Printf(TEXT("Too far - swim to it (%d m)."), FMath::RoundToInt(ReachDist))); return false; }
	const FBox& B = All[Reach];
	if (P->TreasureOpened.Contains(B.Id)) return false;
	const FLoot L = LootOf(B);
	P->TreasureOpened.Add(B.Id);
	if (L.Ammo > 0) P->AddCount(TEXT("handgun.ammo"), L.Ammo);
	P->Credit(L.Cash);
	const FString Money = KeysKit::Money(L.Cash);
	P->ToastMsg(L.Ammo > 0 ? FString::Printf(TEXT("%s - %s, and %d rounds for the console pistol."), *L.Label, *Money, L.Ammo) : FString::Printf(TEXT("%s - %s."), *L.Label, *Money), 5.5);
	if (AKeysAudio* A = AKeysAudio::Get(this)) { A->PlayExtra(TEXT("Chest"), FVector::ZeroVector, 1); A->PlayExtra(TEXT("Coins"), FVector::ZeroVector, 1); }
	bDirty = true;
	Reach = -1;
	return true;
}

bool AKeysTreasure::DebugNearest(double& OX, double& OZ, double& OY) const
{
	const AKeysBoatPawn* P = Cast<AKeysBoatPawn>(UGameplayStatics::GetPlayerPawn(this, 0));
	for (const FBox& B : All)
		if (!P || !P->TreasureOpened.Contains(B.Id)) { OX = B.X; OZ = B.Z; OY = KeysSea::SeafloorHeightAt(B.X, B.Z); return true; }
	return false;
}
