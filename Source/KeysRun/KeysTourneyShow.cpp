#include "KeysTourneyShow.h"
#include "KeysKit.h"
#include "KeysGeo.h"
#include "KeysMesh.h"
#include "KeysData.h"
#include "KeysEvents.h"
#include "KeysTownData.h"
#include "KeysWorld.h"
#include "KeysAudio.h"
#include "KeysBoatPawn.h"
#include "KeysSave.h"
#include "Algo/Reverse.h"
#include "ProceduralMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/World.h"
#include "EngineUtils.h"

using namespace KeysGeo;
using namespace KeysKit;

namespace
{
	constexpr double CountdownS = 15, DurationS = 15 * 60, TitleS = 3.2, GunS = .9, CeremonyS = 10;
	constexpr double AbreastM = 14, StaggerS = 1.4, HarbourMs = 5, RunMs = 24, HarbourLegM = 260, GoneAfterS = 75;
	constexpr double RouteCell = 10, RouteMinDepth = 2;
	const uint32 Skins[] = {0x8d5524, 0xc68642, 0xe0ac69, 0xf1c27d, 0xffdbac, 0x6b4226};
	const uint32 Shirts[] = {0xffffff, 0x2f8fd8, 0xf26b5b, 0x3fb68b, 0xf4c542, 0x8a5cc2, 0xff8c42, 0x9ad0ec};
	FRotator LookRot(const FVector& From, const FVector& To) { const FVector D = To - From; return FVector(D.X, D.Z, D.Y).Rotation(); }
}

AKeysTourneyShow::AKeysTourneyShow()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickGroup = TG_PrePhysics;
	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent = Root;
	Display = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("CeremonyDisplay"));
	Display->SetupAttachment(Root);
	Display->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Party = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("CeremonyParty"));
	Party->SetupAttachment(Root);
	Party->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

AKeysTourneyShow* AKeysTourneyShow::Get(const UObject* WorldContext)
{
	UWorld* W = WorldContext ? WorldContext->GetWorld() : nullptr;
	if (!W) return nullptr;
	TActorIterator<AKeysTourneyShow> It(W);
	return It ? *It : nullptr;
}

void AKeysTourneyShow::BeginPlay()
{
	Super::BeginPlay();
	SetActorLocation(FVector::ZeroVector);
	for (int32 i = 0; i < 3; i++)
	{
		FBoat B;
		B.Hull = FMath::Max(0, KeysData::BoatIndexOf(AKeysEvents::FieldHull(i)));
		Boats.Add(B);
		FKeysMesh M;
		KeysMesh::BuildBoat(M, KeysData::Boats()[B.Hull], true);
		UProceduralMeshComponent* C = NewObject<UProceduralMeshComponent>(this);
		C->SetupAttachment(Root);
		C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		C->RegisterComponent();
		M.Commit(C, 0, AKeysWorld::VertexColorMaterial());
		C->SetVisibility(false);
		FleetMeshes.Add(C);
	}
	Party->SetVisibility(false);
}

// harbourExitRoute: a breadth-first walk over a 10 m grid of water at least 2 m deep (and clear all round) from the
// harbour to the start mark, shortened by line of sight
void AKeysTourneyShow::BuildRoute()
{
	const AKeysEvents* Ev = AKeysEvents::Get(this);
	const double HX = AKeysEvents::HarbourX(), HZ = AKeysEvents::HarbourZ();
	double GX = HX + 600, GZ = HZ + 900;
	if (Ev && Ev->Marks().Num()) { GX = Ev->Marks()[0].X; GZ = Ev->Marks()[0].Z; }
	const double X0 = FMath::Min(HX, GX) - 600, Z0 = FMath::Min(HZ, GZ) - 600;
	const int32 NX = FMath::CeilToInt((FMath::Max(HX, GX) + 600 - X0) / RouteCell), NZ = FMath::CeilToInt((FMath::Max(HZ, GZ) + 600 - Z0) / RouteCell);
	TArray<uint8> Deep;
	Deep.SetNumZeroed(NX * NZ);
	for (int32 j = 0; j < NZ; j++) for (int32 i = 0; i < NX; i++) Deep[j * NX + i] = DepthAt(X0 + i * RouteCell, Z0 + j * RouteCell) >= RouteMinDepth ? 1 : 0;
	auto Ok = [&](int32 i, int32 j)
	{
		for (int32 dj = -1; dj <= 1; dj++) for (int32 di = -1; di <= 1; di++)
		{
			const int32 A = i + di, B = j + dj;
			if (A < 0 || B < 0 || A >= NX || B >= NZ || !Deep[B * NX + A]) return false;
		}
		return true;
	};
	auto Cell = [&](double X, double Z, int32& OI, int32& OJ) { OI = FMath::RoundToInt((X - X0) / RouteCell); OJ = FMath::RoundToInt((Z - Z0) / RouteCell); };
	int32 SI, SJ, GI, GJ;
	Cell(HX, HZ, SI, SJ); Cell(GX, GZ, GI, GJ);
	TArray<int32> Prev, Q;
	Prev.Init(-1, NX * NZ);
	Q.Reserve(NX * NZ);
	const int32 Start = SJ * NX + SI, Goal = GJ * NX + GI;
	Route.Reset();
	if (!Prev.IsValidIndex(Start) || !Prev.IsValidIndex(Goal)) { Route.Add(FVector2D(HX, HZ)); Route.Add(FVector2D(GX, GZ)); }
	else
	{
		Q.Add(Start); Prev[Start] = Start;
		for (int32 Head = 0; Head < Q.Num(); Head++)
		{
			const int32 C = Q[Head];
			if (C == Goal) break;
			const int32 I = C % NX, J = C / NX;
			const int32 Nb[8][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}, {1, 1}, {1, -1}, {-1, 1}, {-1, -1}};
			for (const auto& D : Nb)
			{
				const int32 A = I + D[0], B = J + D[1], N = B * NX + A;
				if (A < 0 || B < 0 || A >= NX || B >= NZ || Prev[N] >= 0 || !Ok(A, B)) continue;
				Prev[N] = C;
				Q.Add(N);
			}
		}
		if (Prev[Goal] < 0) { Route.Add(FVector2D(HX, HZ)); Route.Add(FVector2D(GX, GZ)); }
		else
		{
			TArray<FVector2D> Cells;
			for (int32 C = Goal;; C = Prev[C]) { Cells.Add(FVector2D(X0 + (C % NX) * RouteCell, Z0 + (C / NX) * RouteCell)); if (Prev[C] == C) break; }
			Algo::Reverse(Cells);
			auto Clear = [&](const FVector2D& A, const FVector2D& B)
			{
				const int32 N = FMath::CeilToInt(FVector2D::Distance(A, B) / (RouteCell / 2));
				for (int32 k = 0; k <= N; k++) { int32 I, J; Cell(A.X + (B.X - A.X) * k / FMath::Max(1, N), A.Y + (B.Y - A.Y) * k / FMath::Max(1, N), I, J); if (!Ok(I, J)) return false; }
				return true;
			};
			int32 At = 0;
			Route.Add(Cells[0]);
			while (At < Cells.Num() - 1)
			{
				int32 Far = At + 1;
				for (int32 k = Cells.Num() - 1; k > At + 1; k--) if (Clear(Cells[At], Cells[k])) { Far = k; break; }
				Route.Add(Cells[Far]);
				At = Far;
			}
		}
	}
	Cum.Reset();
	Cum.Add(0);
	for (int32 i = 1; i < Route.Num(); i++) Cum.Add(Cum[i - 1] + FVector2D::Distance(Route[i - 1], Route[i]));
	RouteLen = FMath::Max(1.0, Cum.Last());
	// where the field lines up: the first stretch of the route where all three hulls, abreast, sit in open water
	// clear of every pier, finger and piling (the harbour point itself is beside the town's pier head)
	auto Open = [](double X, double Z)
	{
		if (DepthAt(X, Z) < 1.6) return false;
		double Gy;
		for (int32 k = 0; k < 8; k++)
			if (KeysTown::GroundAt(X + FMath::Cos(k * PI / 4) * 6, Z + FMath::Sin(k * PI / 4) * 6, Gy, false)) return false;
		return !KeysTown::GroundAt(X, Z, Gy, false);
	};
	StartS = 0;
	for (double S0 = 0; S0 < FMath::Min(RouteLen - 40, 600.0); S0 += 5)
	{
		double X, Z, TX, TZ;
		RouteAt(S0, X, Z, TX, TZ);
		bool bOk = true;
		for (int32 i = 0; i < Boats.Num() && bOk; i++)
		{
			const double Off = (i - (Boats.Num() - 1) / 2.0) * AbreastM;
			for (double Al = -8; Al <= 8 && bOk; Al += 4) bOk = Open(X - TZ * Off + TX * Al, Z + TX * Off + TZ * Al);
		}
		if (bOk) { StartS = S0; break; }
	}
}

void AKeysTourneyShow::RouteAt(double S, double& X, double& Z, double& TX, double& TZ) const
{
	S = FMath::Clamp(S, 0.0, RouteLen);
	int32 i = 0;
	while (i < Route.Num() - 2 && Cum[i + 1] < S) i++;
	const FVector2D A = Route[i], B = Route[FMath::Min(i + 1, Route.Num() - 1)];
	const double L = FMath::Max(1e-6, FVector2D::Distance(A, B)), U = FMath::Clamp((S - Cum[i]) / L, 0.0, 1.0);
	X = A.X + (B.X - A.X) * U; Z = A.Y + (B.Y - A.Y) * U;
	TX = (B.X - A.X) / L; TZ = (B.Y - A.Y) / L;
}

// ================================================================ the field leaving the harbour (fleet.ts)
void AKeysTourneyShow::TickFleet(double Dt, double T, double Since)
{
	const bool bOn = Since > -1e8 && Since <= GoneAfterS;
	if (bOn != bFleetOn) { for (UProceduralMeshComponent* C : FleetMeshes) C->SetVisibility(bOn); bFleetOn = bOn; }
	if (!bOn) return;
	if (!Route.Num()) BuildRoute();
	for (int32 i = 0; i < Boats.Num(); i++)
	{
		FBoat& B = Boats[i];
		const KeysData::FBoatSpec& Sp = KeysData::Boats()[B.Hull];
		if (Since < 0) { B.S = StartS; B.Speed = 0; }
		else if (Since >= i * StaggerS)
		{
			const double Want = B.S < StartS + HarbourLegM ? HarbourMs : RunMs;
			B.Speed = FMath::Lerp(B.Speed, Want, FMath::Min(1.0, Dt * (Want > B.Speed ? .6 : 1.2)));
			B.S = FMath::Min(RouteLen - 1, B.S + B.Speed * Dt);
		}
		double X, Z, TX, TZ;
		RouteAt(B.S, X, Z, TX, TZ);
		// abreast across the start of the route, closing into line as they get under way
		const double K = FMath::Max(0.0, 1 - (B.S - StartS) / 80), Off = (i - (Boats.Num() - 1) / 2.0) * AbreastM * K;
		B.X = X - TZ * Off; B.Z = Z + TX * Off; B.H = FMath::Atan2(-TX, -TZ);
		FPose Ps = RidePose(B.X, B.Z, B.H, Sp.Len, Sp.Beam, T, .8);
		Ps.Pitch += FMath::Min(.2, B.Speed * .004);
		FleetMeshes[i]->SetWorldLocationAndRotation(ToUE(B.X, Ps.Y, B.Z), PoseRot(Ps, B.H));
		if (B.Speed > 1) EmitWake(B.X, B.Z, B.Speed, Sp.Len, T, Dt);
		if (B.Speed > 6) EmitTrail(900 + i, B.X - FMath::Sin(B.H) * -Sp.Len * .45, B.Z - FMath::Cos(B.H) * -Sp.Len * .45, Sp.Beam * .8, FMath::Min(1.0, B.Speed / 20), T);
	}
}

// ================================================================ the ceremony (ceremony.ts)
FVector AKeysTourneyShow::Local(double X, double Y, double Z) const
{
	const double C = FMath::Cos(Yaw), S = FMath::Sin(Yaw);
	return Origin + FVector(X * C + Z * S, Y, -X * S + Z * C);
}

void AKeysTourneyShow::BeginCeremony()
{
	const AKeysEvents* Ev = AKeysEvents::Get(this);
	AKeysBoatPawn* P = Cast<AKeysBoatPawn>(UGameplayStatics::GetPlayerPawn(this, 0));
	if (!Ev || !P) return;
	// the promenade at the town marina, off to one side of the pier ramp, facing the water
	double X, Z, SX, SZ;
	// east of the ramp: west of it (the web game's first choice) this port's dry-stack rack stands between the podium
	// and the water, so the wide shot from the water came in through the rack's uprights
	KeysTown::ToWorld(16, KeysTown::PadV0 + 6, X, Z);
	KeysTown::ToWorld(16, KeysTown::PadV0 + 5, SX, SZ);
	Origin = FVector(X, KeysTown::PadY, Z);
	Yaw = FMath::Atan2(-(SZ - Z), SX - X);
	const bool bMe = Ev->WinnerId0() == TEXT("me");
	const int32 NI = bMe ? -1 : FCString::Atoi(*Ev->WinnerId0().Mid(3));
	const FString Name = bMe ? (P->Board() && !P->Board()->OnlineName.IsEmpty() ? P->Board()->OnlineName : FString(TEXT("You"))) : AKeysEvents::FieldCaptain(NI);
	const FString Boat = bMe ? P->Spec().Brand + TEXT(" ") + P->Spec().Name : AKeysEvents::FieldName(NI) + TEXT(" - ") + KeysData::Boats()[FMath::Max(0, KeysData::BoatIndexOf(AKeysEvents::FieldHull(NI)))].Brand + TEXT(" ") + KeysData::Boats()[FMath::Max(0, KeysData::BoatIndexOf(AKeysEvents::FieldHull(NI)))].Name;
	// the display: podium, the rack of five under the banner (it stays after the ceremony)
	FKeysMesh M;
	auto W = [this](double X0, double Y0, double Z0) { const FVector G = Local(X0, Y0, Z0); return FVector(G.X, G.Z, G.Y); };
	const FQuat Q(FVector::ZAxisVector, -Yaw);
	M.AddBox(W(0, .3, 0), FVector(1.4, 1.4, .6), Hex(0xf3f1ea), Q);
	const double RackX = -2.4, RackTop = 3.4, Span = 5.4;
	M.AddTube(W(RackX, 0, -Span / 2), W(RackX, RackTop, -Span / 2), .08, .08, 6, Hex(0x6b4a2f));
	M.AddTube(W(RackX, 0, Span / 2), W(RackX, RackTop, Span / 2), .08, .08, 6, Hex(0x6b4a2f));
	M.AddTube(W(RackX, RackTop, -Span / 2 - .15), W(RackX, RackTop, Span / 2 + .15), .07, .07, 6, Hex(0x6b4a2f));
	M.AddTube(W(RackX - .1, RackTop, -Span / 2), W(RackX - .1, RackTop + 2.2, -Span / 2), .05, .05, 5, Hex(0x6b4a2f));
	M.AddTube(W(RackX - .1, RackTop, Span / 2), W(RackX - .1, RackTop + 2.2, Span / 2), .05, .05, 5, Hex(0x6b4a2f));
	M.AddBox(W(RackX - .12, RackTop + 1.15, 0), FVector(.05, Span, 2.0), Hex(0x15324a), Q);
	for (UTextRenderComponent* T : Texts) if (T) T->DestroyComponent();
	Texts.Reset();
	auto Txt = [&](const FVector& At, const FString& S, float Size, FColor Col)
	{
		UTextRenderComponent* T = NewObject<UTextRenderComponent>(this);
		T->SetupAttachment(Root);
		T->RegisterComponent();
		T->SetText(FText::FromString(S));
		T->SetHorizontalAlignment(EHTA_Center);
		T->SetVerticalAlignment(EVRTA_TextCenter);
		T->SetTextRenderColor(Col);
		T->SetWorldSize(Size * 100);
		T->SetWorldLocationAndRotation(At * 100, FRotator(0, FMath::RadiansToDegrees(-Yaw), 0));
		T->SetCastShadow(false);
		Texts.Add(T);
	};
	const TArray<FString>& Sp = Ev->TourneySpecies();
	for (int32 i = 0; i < Sp.Num(); i++)
	{
		const KeysData::FSpecies* S = KeysData::Species(Sp[i]);
		const double Lb = bMe && Ev->MyWeights().IsValidIndex(i) && Ev->MyWeights()[i] > 0 ? Ev->MyWeights()[i] : (S ? S->Min : 1);
		const double Zc = -Span / 2 + Span * ((i + .5) / Sp.Num()), Len = FMath::Min(RackTop - .6, KeysMesh::FishLength(Lb));
		// hung by the tail, nose down
		AppendFish(M, Sp[i], Len, FTransform(Q * FQuat(FVector::YAxisVector, PI / 2), W(RackX + .15, RackTop - .25 - Len / 2, Zc)));
		Txt(W(RackX + .14, .45, Zc), FString::Printf(TEXT("%s\n%.1f lb"), S ? *S->Name : *Sp[i], Lb), .14, FColor::White);
	}
	Txt(W(RackX - .05, RackTop + 1.75, 0), TEXT("KEYS RUN FISHING TOURNAMENT"), .2, FColor::White);
	Txt(W(RackX - .05, RackTop + 1.35, 0), TEXT("CHAMPION"), .42, FColor(0xf4, 0xc5, 0x42));
	Txt(W(RackX - .05, RackTop + .9, 0), Name, .3, FColor::White);
	Txt(W(RackX - .05, RackTop + .55, 0), Boat, .17, FColor(0xcf, 0xe6, 0xf2));
	Txt(W(.71, .3, 0), TEXT("1"), .45, FColor(0xf4, 0xc5, 0x42));
	M.Commit(Display, 0, AKeysWorld::VertexColorMaterial());
	Display->SetVisibility(true);
	// the winner on the podium, and the crowd
	FKeysHumanLook WL;
	WL.Skin = Hex(bMe ? 0xe0ac69 : 0xc68642); WL.Shirt = Hex(bMe ? 0x2f8fd8 : 0x1f2f46); WL.Bottom = Hex(0xc9b38a); WL.bGlasses = true; WL.bCap = !bMe; WL.CapColor = FLinearColor::White;
	Winner.Init(WL, false);
	Fans.Reset(); FanBase.Reset();
	int32 i = 0;
	for (const auto& Ring : {TPair<double, int32>(3.2, 6), TPair<double, int32>(4.4, 8)})
		for (int32 k = 0; k < Ring.Value; k++)
		{
			const double A = PI * 1.1 * (double(k) / (Ring.Value - 1) - .5);
			const FVector G = Local(FMath::Cos(A) * Ring.Key, 0, FMath::Sin(A) * Ring.Key);
			double Gy;
			if (!KeysTown::GroundAt(G.X, G.Z, Gy, false) || FMath::Abs(Gy - Origin.Y) > .6) continue;
			const int32 S = i * 7 + 3;
			FKeysHumanLook L;
			L.Skin = Hex(Skins[S % 6]); L.Shirt = Hex(Shirts[(S * 3 + 1) % 8]); L.Bottom = Hex(0x2b4a6e); L.bCap = S % 3 == 0; L.CapColor = Hex(Shirts[(S + 5) % 8]); L.bGlasses = S % 4 == 1;
			L.bLongHair = S % 2 == 1; L.HairStyle = S % 4;
			FKeysHuman H;
			H.Init(L, false);
			Fans.Add(H);
			FanBase.Add(FVector(G.X, Gy, G.Z));
			i++;
		}
	Confetti.Reset();
	static const uint32 Cols[] = {0xf2c14e, 0xffffff, 0x2f8fd8, 0xf26b5b, 0x3fb68b};
	for (int32 k = 0; k < 260; k++)
		Confetti.Add({Local((FMath::FRand() - .3) * 6, 4 + FMath::FRand() * 5, (FMath::FRand() - .5) * 7), FVector((FMath::FRand() - .5) * .6, -.8 - FMath::FRand() * .8, (FMath::FRand() - .5) * .6), FMath::FRand() * 6, 2 + FMath::FRand() * 6, Hex(Cols[k % 5])});
	Party->SetVisibility(true);
	bCeremony = true;
	CerT = 0;
	if (AKeysAudio* A = AKeysAudio::Get(this)) A->PlayExtra(TEXT("Cheer"), FVector::ZeroVector, 1.2f);
}

void AKeysTourneyShow::TickCeremony(double Dt, double T, double S)
{
	FKeysMesh M;
	const double HumanYaw = -Yaw;   // facing the crowd and the water (local +x)
	Winner.Pos = ToUE(Origin.X, Origin.Y + .6, Origin.Z) / 100.0;
	Winner.Yaw = HumanYaw;
	Winner.IdleUpdate(T, 0);
	const double Pump = FMath::Abs(FMath::Sin(T * 3.2)) * .06, HY = Winner.HipY;
	Winner.PoseLegs(FKeysHuman::J(-.105, .085, 0), FKeysHuman::J(.105, .085, 0));
	Winner.Pose(FKeysHuman::J(-.13, HY + 1.0 + Pump, .1), FKeysHuman::J(.13, HY + 1.0 + Pump, .1));
	Winner.Build(M);
	// the trophy over their head: foot, stem, a flared bowl, two handles
	{
		const FTransform Xf = Winner.ToBoat();
		const FVector C = Xf.TransformPosition(FKeysHuman::J(0, HY + 1.08 + Pump, .1));
		const FLinearColor Gold = Hex(0xf2c14e);
		M.AddBox(C + FVector(0, 0, -.03), FVector(.2, .2, .06), Hex(0x2a1c12));
		M.AddTube(C, C + FVector(0, 0, .16), .07, .025, 10, Gold);
		M.AddTube(C + FVector(0, 0, .16), C + FVector(0, 0, .42), .03, .13, 14, Gold);
		for (int32 s = -1; s <= 1; s += 2) M.AddTube(C + Xf.GetRotation().RotateVector(FVector(0, s * .13, .38)), C + Xf.GetRotation().RotateVector(FVector(0, s * .18, .28)), .012, .012, 5, Gold);
	}
	for (int32 i = 0; i < Fans.Num(); i++)
	{
		FKeysHuman& H = Fans[i];
		const FVector B = FanBase[i];
		H.IdleUpdate(T, 0);
		const double ToOx = Origin.X - B.X, ToOz = Origin.Z - B.Z;
		H.Yaw = FMath::Atan2(ToOz, ToOx);
		H.PoseLegs(FKeysHuman::J(-.105, .085, 0), FKeysHuman::J(.105, .085, 0));
		const double Ph = H.Ph, HY2 = H.HipY;
		double Lift = 0;
		if (i % 2 == 0)
		{
			const double Wv = FMath::Sin(T * 7 + Ph);
			H.Pose(FKeysHuman::J(-.2 - Wv * .04, HY2 + 1.0, .05), FKeysHuman::J(.2 + Wv * .04, HY2 + 1.0, .05));
			Lift = FMath::Abs(FMath::Sin(T * 5.5 + Ph)) * .09;
		}
		else
		{
			const double Gap = .03 + FMath::Abs(FMath::Sin(T * 9 + Ph)) * .12;
			H.Pose(FKeysHuman::J(-Gap, HY2 + .42, .3), FKeysHuman::J(Gap, HY2 + .42, .3));
		}
		H.HeadNod = -.15;
		H.Pos = ToUE(B.X, B.Y + Lift, B.Z) / 100.0;
		H.Build(M);
	}
	for (FBit& Bt : Confetti)
	{
		Bt.P += Bt.V * Dt;
		Bt.P.X += FMath::Sin(T * 2 + Bt.Spin) * Dt * .3;
		Bt.R += Bt.Spin * Dt;
		if (Bt.P.Y < Origin.Y) Bt.P.Y += 8;
		const FVector C = ToUE(Bt.P.X, Bt.P.Y, Bt.P.Z) / 100.0;
		const FVector Ax(FMath::Cos(Bt.R) * .03, FMath::Sin(Bt.R) * .03, FMath::Sin(Bt.R * .7) * .05), Ay(0, 0, .05);
		M.AddQuad(C - Ax - Ay, C + Ax - Ay, C + Ax + Ay, C - Ax + Ay, Bt.C, FVector::CrossProduct(Ax, Ay));
		M.AddQuad(C - Ax - Ay, C - Ax + Ay, C + Ax + Ay, C + Ax - Ay, Bt.C, -FVector::CrossProduct(Ax, Ay));
	}
	M.Commit(Party, 0, AKeysWorld::VertexColorMaterial());
	// three shots: a wide from the water side, close on the trophy, then along the fish
	bCam = true; CamFov = 60;
	// the wide from over the water (ceremony.ts), a little higher so it clears the rails and the moored boats
	if (S < 3.5) { const double U = S / 3.5; CamPos = Local(11 - U * 2, 3.8 + U * .6, -5 + U * 6); CamLook = Local(-1, 2.2, 0); }
	else if (S < 7) { const double U = (S - 3.5) / 3.5, A = -.5 + U; CamPos = Local(FMath::Cos(A) * 3.4, 2.6 - U * .5, FMath::Sin(A) * 3.4); CamLook = Local(0, 2.55, 0); }
	else { const double U = FMath::Min(1.0, (S - 7) / 3); CamPos = Local(-.6, 2.5, -2.3 + U * 4.6); CamLook = Local(-2.4, 2.35, -2.0 + U * 4.0); }
}

// ================================================================ tick: the titles, the fleet, the ceremony
void AKeysTourneyShow::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	const double Dt = FMath::Min(double(DeltaSeconds), .1), T = GetWorld()->GetTimeSeconds();
	const AKeysEvents* Ev = AKeysEvents::Get(this);
	AKeysBoatPawn* P = Cast<AKeysBoatPawn>(UGameplayStatics::GetPlayerPawn(this, 0));
	bCam = false; bLetterbox = false;
	const int32 OldCard = CardKind;
	CardKind = 0;
	if (!Ev || !P) return;
	const AKeysEvents::ETourney Ph = Ev->TourneyPhase();
	const bool bIn = Ph == AKeysEvents::ETourney::Countdown || Ph == AKeysEvents::ETourney::Live || Ph == AKeysEvents::ETourney::Ceremony;
	const double Since = bIn ? Ev->TourneyClock() : -1e9;
	if (Ph == AKeysEvents::ETourney::Countdown && LastPhase != int32(Ph)) { Route.Reset(); for (FBoat& B : Boats) { B.S = StartS; B.Speed = 0; } }
	TickFleet(Dt, T, Since);
	if (Ph == AKeysEvents::ETourney::Countdown)
	{
		// intro.ts: time-indexed shots over the fifteen seconds
		const double S = CountdownS - Ev->CountdownLeft();
		bLetterbox = true; bCam = true; CamFov = 55;
		double FX = 0, FZ = 0, FH = 0;
		for (const FBoat& B : Boats) { FX += B.X / Boats.Num(); FZ += B.Z / Boats.Num(); }
		if (Boats.Num()) FH = Boats[1].H;
		const double HX = AKeysEvents::HarbourX(), HZ = AKeysEvents::HarbourZ();
		struct FEnt { FString Name, Boat; double X, Z, H; };
		TArray<FEnt> Ents;
		for (int32 i = 0; i < Boats.Num(); i++)
		{
			const KeysData::FBoatSpec& Sp = KeysData::Boats()[Boats[i].Hull];
			Ents.Add({AKeysEvents::FieldCaptain(i), AKeysEvents::FieldName(i) + TEXT(" - ") + Sp.Brand + TEXT(" ") + Sp.Name, Boats[i].X, Boats[i].Z, Boats[i].H});
		}
		Ents.Add({TEXT("You"), P->Spec().Brand + TEXT(" ") + P->Spec().Name, P->BoatX(), P->BoatZ(), P->Heading()});
		const double BoatS = (CountdownS - TitleS - GunS) / Ents.Num();
		if (S < TitleS)
		{
			const double U = S / TitleS;
			CamPos = FVector(HX + 160 - U * 70, 70 - U * 28, HZ - 140 + U * 60);
			CamLook = FVector(FX, 0, FZ);
			CardKind = 1;
			CardTitle = TEXT("Keys Run Fishing Tournament");
			CardSub = FString::Printf(TEXT("%d minutes - one of each - first back to Boot Key Harbor wins"), int32(DurationS / 60));
			CardChips.Reset();
			for (const FString& K : Ev->TourneySpecies()) { const KeysData::FSpecies* Sp = KeysData::Species(K); CardChips.Add(Sp ? Sp->Name : K); }
		}
		else if (S < CountdownS - GunS)
		{
			const int32 K = FMath::Clamp(int32((S - TitleS) / BoatS), 0, Ents.Num() - 1);
			const FEnt& E = Ents[K];
			const double U = (S - TitleS - K * BoatS) / BoatS, Side = K % 2 ? -1 : 1;
			const double Fx = -FMath::Sin(E.H), Fz = -FMath::Cos(E.H), Rx = FMath::Cos(E.H), Rz = -FMath::Sin(E.H);
			const double Along = 5 - U * 4, Out = 9 - U * 1.5;
			CamPos = FVector(E.X + Fx * Along + Rx * Out * Side, 0, E.Z + Fz * Along + Rz * Out * Side);
			CamPos.Y = FMath::Max(WaveH(CamPos.X, CamPos.Z, T, AmpAt(CamPos.X, CamPos.Z)) + 1.4, 2.6 + U * .4);
			CamLook = FVector(E.X + Fx * 1.5, 1.6, E.Z + Fz * 1.5);
			CardKind = 2; CardTitle = E.Name; CardSub = E.Boat;
		}
		else
		{
			const double Fx = -FMath::Sin(FH), Fz = -FMath::Cos(FH);
			CamPos = FVector(FX - Fx * 40, 16, FZ - Fz * 40);
			CamLook = FVector(FX + Fx * 30, 0, FZ + Fz * 30);
			CardKind = 3; CardTitle = TEXT("Lines in"); CardSub = FString::Printf(TEXT("%d minutes on the clock"), int32(DurationS / 60));
		}
	}
	if (Ph == AKeysEvents::ETourney::Ceremony)
	{
		if (!bCeremony) BeginCeremony();
		CerT += Dt;
		if (bCeremony && CerT < CeremonyS)
		{
			TickCeremony(Dt, T, CerT);
			CardKind = 4; CardTitle = TEXT("Champion"); CardSub = Ev->WinnerName();
			bLetterbox = true;
		}
	}
	else if (bCeremony)
	{
		// the ten seconds are up: the crowd goes home, the display stays
		bCeremony = false;
		Party->SetVisibility(false);
	}
	if (bCeremony && CerT >= CeremonyS) { Party->SetVisibility(false); }
	CardAge = CardKind == OldCard ? CardAge + Dt : 0;
	LastPhase = int32(Ph);
}

bool AKeysTourneyShow::CameraFor(FVector& OutPosUE, FRotator& OutRot, float& OutFov) const
{
	if (!bCam) return false;
	OutPosUE = ToUE(CamPos.X, CamPos.Y, CamPos.Z);
	OutRot = LookRot(CamPos, CamLook);
	OutFov = CamFov;
	return true;
}
