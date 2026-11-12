#include "KeysStreet.h"
#include "KeysKit.h"
#include "KeysDog.h"
#include "KeysGeo.h"
#include "KeysMesh.h"
#include "KeysTown.h"
#include "KeysTownData.h"
#include "KeysWorld.h"
#include "ProceduralMeshComponent.h"
#include "Algo/Reverse.h"
#include "Kismet/GameplayStatics.h"
#include "Camera/PlayerCameraManager.h"
#include "Engine/World.h"

using namespace KeysKit;
using namespace KeysTown;
using EStance = AKeysStreet::EStance;
using FPt = AKeysStreet::FPt;
using FScript = AKeysStreet::FScript;
using FLifePose = AKeysStreet::FLifePose;

namespace
{
	constexpr double StreetPeriod = 3600, Ramp = .55;
	constexpr double Sea = -PI / 2, Land = PI / 2;
	constexpr double DogRange = 80, CritterRange = 46, GullRange = 110, PeopleRange = 140;

	// one beat of a script: go along points, stay a while in a stance, or be inside (hidden)
	struct FB { uint8 Kind; TArray<FPt> Pts; double Speed = 1; EStance St = EStance::Walk; double Secs = 0; double Yaw = 0; bool bYaw = false; bool bCarry = false; };
	FB Go(const TArray<FPt>& Pts, double Speed, EStance St = EStance::Walk, bool bCarry = false) { FB B; B.Kind = 0; B.Pts = Pts; B.Speed = Speed; B.St = St; B.bCarry = bCarry; return B; }
	FB Stay(double Secs, EStance St, double Yaw, bool bCarry = false) { FB B; B.Kind = 1; B.Secs = Secs; B.St = St; B.Yaw = Yaw; B.bYaw = true; B.bCarry = bCarry; return B; }
	FB Inside(double Secs) { FB B; B.Kind = 2; B.Secs = Secs; return B; }
	FPt P2(double U, double V) { return {U, V, 0}; }

	// town-life.ts compile: lay the beats end to end, then stretch them to a period that divides the hour
	FScript Compile(const FString& Id, const FPt& Start, const TArray<FB>& Beats, double Offset)
	{
		static TArray<double> Periods;
		if (!Periods.Num()) for (int32 D = 30; D <= 3600; D++) if (3600 % D == 0) Periods.Add(D);
		FScript S;
		S.Id = Id; S.Start = Start; S.Offset = Offset;
		FPt Cur = Start;
		double Yaw = 0, T = 0;
		for (int32 i = 0; i < Beats.Num(); i++)
		{
			const FB& B = Beats[i];
			AKeysStreet::FSeg G;
			G.Idx = i; G.bCarry = B.bCarry; G.Speed = 0; G.Len = 0; G.Ramp = 0;
			if (B.Kind == 0)
			{
				G.Kind = 0;
				G.Pts.Add(Cur);
				G.Pts.Append(B.Pts);
				G.Cum.Add(0);
				for (int32 k = 1; k < G.Pts.Num(); k++)
					G.Cum.Add(G.Cum[k - 1] + FMath::Sqrt(FMath::Square(G.Pts[k].U - G.Pts[k - 1].U) + FMath::Square(G.Pts[k].V - G.Pts[k - 1].V) + FMath::Square(G.Pts[k].Y - G.Pts[k - 1].Y)));
				G.Len = G.Cum.Last();
				const double Dur = G.Len / B.Speed + Ramp;
				const FPt& L = G.Pts.Last();
				const FPt& Pv = G.Pts[G.Pts.Num() - 2];
				if (G.Len > 1e-6) Yaw = FMath::Atan2(L.V - Pv.V, L.U - Pv.U);
				G.T0 = T; G.T1 = T + Dur; G.Speed = B.Speed; G.Stance = B.St; G.Yaw = Yaw;
				T += Dur; Cur = L;
			}
			else
			{
				if (B.Kind == 1 && B.bYaw) Yaw = B.Yaw;
				G.Kind = B.Kind; G.Pts.Add(Cur); G.Cum.Add(0); G.T0 = T; G.T1 = T + B.Secs;
				G.Stance = B.Kind == 1 ? B.St : EStance::Stand; G.Yaw = Yaw;
				if (B.Kind == 2) G.bCarry = false;
				T += B.Secs;
			}
			S.Segs.Add(G);
		}
		if (FMath::Sqrt(FMath::Square(Cur.U - Start.U) + FMath::Square(Cur.V - Start.V)) > .02) UE_LOG(LogTemp, Warning, TEXT("Keys Run street: %s does not end where it started"), *Id);
		double Cycle = Periods[0];
		for (double D : Periods) if (FMath::Abs(D / T - 1) < FMath::Abs(Cycle / T - 1)) Cycle = D;
		const double K = Cycle / FMath::Max(T, 1e-3);
		S.Cycle = Cycle;
		for (AKeysStreet::FSeg& G : S.Segs)
		{
			const double D0 = G.T1 - G.T0;
			G.T0 *= K; G.T1 *= K; G.Speed /= K; G.Ramp = FMath::Min(Ramp * K, D0 * K / 3);
		}
		return S;
	}

	void Sample(const FScript& S, double T, FLifePose& Out)
	{
		double Tt = FMath::Fmod(T + S.Offset, S.Cycle);
		if (Tt < 0) Tt += S.Cycle;
		const AKeysStreet::FSeg* G = &S.Segs.Last();
		for (const AKeysStreet::FSeg& Q : S.Segs) if (Tt < Q.T1) { G = &Q; break; }
		Out.Stance = G->Stance; Out.bCarry = G->bCarry; Out.Beat = G->Idx; Out.Bt = Tt - G->T0; Out.bHidden = G->Kind == 2;
		if (G->Kind != 0) { Out.U = G->Pts[0].U; Out.V = G->Pts[0].V; Out.Y = G->Pts[0].Y; Out.Yaw = G->Yaw; Out.Speed = 0; return; }
		const double Tl = G->T1 - G->T0, X = Tt - G->T0, Rp = G->Ramp, Cruise = G->Len > 0 ? G->Len / FMath::Max(1e-6, Tl - Rp) : 0;
		double D, Spd;
		if (X < Rp) { D = Cruise * X * X / (2 * Rp); Spd = Cruise * X / Rp; }
		else if (X > Tl - Rp) { const double Rem = Tl - X; D = G->Len - Cruise * Rem * Rem / (2 * Rp); Spd = Cruise * Rem / Rp; }
		else { D = Cruise * Rp / 2 + Cruise * (X - Rp); Spd = Cruise; }
		D = FMath::Clamp(D, 0.0, G->Len);
		int32 i = 1;
		while (i < G->Cum.Num() - 1 && G->Cum[i] < D) i++;
		const FPt& A = G->Pts[i - 1];
		const FPt& B = G->Pts[i];
		const double Sl = FMath::Max(1e-9, G->Cum[i] - G->Cum[i - 1]), F = (D - G->Cum[i - 1]) / Sl;
		Out.U = A.U + (B.U - A.U) * F; Out.V = A.V + (B.V - A.V) * F; Out.Y = A.Y + (B.Y - A.Y) * F;
		const double Hl = FMath::Sqrt(FMath::Square(B.U - A.U) + FMath::Square(B.V - A.V));
		Out.Yaw = Hl > 1e-6 ? FMath::Atan2(B.V - A.V, B.U - A.U) : G->Yaw;
		Out.Speed = Spd;
	}

	const FLot* LotById(const TCHAR* Id) { for (const FLot& L : Lots()) if (FCString::Strcmp(L.Id, Id) == 0) return &L; return nullptr; }

	// visit(): window-shop, knock, in at the door, a while inside, out with what they bought, along to somewhere, back
	AKeysStreet::FScript Visit(const TCHAR* Id, const TCHAR* LotId, double From, double To, double Lane, double Back, double Secs, double Offset, bool bProp, double Speed = 1.25, double WinU = 1e9, double WinS = 0, double After = 5)
	{
		const FLot* L = LotById(LotId);
		const double Door = L ? L->Door : From, Front = L ? L->V0 - .95 : Lane, Hide = L ? L->V0 + .62 : Lane;
		TArray<FB> B;
		if (WinU < 1e8) { B.Add(Go({P2(WinU, Lane)}, Speed)); B.Add(Stay(WinS, EStance::Look, Land)); }
		B.Add(Go({P2(Door, Lane)}, Speed * .9));
		B.Add(Stay(1.4, EStance::Knock, Land));
		B.Add(Go({P2(Door, Front), P2(Door, Hide)}, .8));
		B.Add(Inside(Secs));
		B.Add(Go({P2(Door, Front), P2(Door + (To > Door ? 1.0 : -1.0), Back)}, .9));
		B.Add(Go({P2(To, Back)}, Speed * 1.1, bProp ? EStance::Carry : EStance::Walk, bProp));
		B.Add(Stay(After, bProp ? EStance::Carry : EStance::Phone, Land, bProp));
		B.Add(Go({P2(From, Back), P2(From, Lane)}, Speed * 1.05, bProp ? EStance::Carry : EStance::Walk, bProp));
		return Compile(Id, P2(From, Lane), B, Offset);
	}

	TArray<FB> Shifted(const TArray<FB>& Beats, double Lat)
	{
		TArray<FB> O = Beats;
		for (FB& B : O) for (FPt& P : B.Pts) P.V += Lat;
		return O;
	}

	// a palm's trunk (palmShape / palmTrunkAt): lean, height, and a point f of the way up
	FPt PalmTrunk(double U, double V, double Hs, double F)
	{
		const double Seed = FMath::Fmod(FMath::Abs(FMath::Sin(U * 12.9898 + V * 78.233) * 43758.5453), 1.0), Lean = (Seed - .5) * .9;
		const double R = .3 - .14 * F;
		return {U + Lean * 1.65 * F * F, V - Lean * .75 * F * F + R + .04, .5 + 6.4 * Hs * F};
	}

	FKeysHumanLook CrowdLook(const FString& Look, int32 I)
	{
		if (Look == TEXT("jogger") || Look == TEXT("jogger-m"))
		{
			FKeysHumanLook L = KeysTownPaint::LookFor(Look == TEXT("jogger") ? TEXT("woman") : TEXT("local"), I);
			L.Shirt = Hex(Look == TEXT("jogger") ? 0xff6b9a : 0x2f8fd8); L.Bottom = Hex(0x15171a); L.bShoes = true; L.Shoes = Hex(0xf2f2f2); L.bCap = Look != TEXT("jogger");
			if (Look == TEXT("jogger")) { L.bLongHair = true; L.HairStyle = 2; }
			return L;
		}
		if (Look == TEXT("kid")) { FKeysHumanLook L = KeysTownPaint::LookFor(TEXT("tourist"), I); L.Shirt = Hex(0xf4c542); L.Bottom = Hex(0x3fb68b); L.bCap = true; L.CapColor = Hex(0xd7263d); return L; }
		if (Look == TEXT("worker")) { FKeysHumanLook L = KeysTownPaint::LookFor(TEXT("local"), I); L.Shirt = Hex(0xf28c28); L.Bottom = Hex(0x3d4a5a); L.bShortsLong = true; L.bCap = true; L.CapColor = Hex(0x23262a); return L; }
		if (Look == TEXT("granny")) { FKeysHumanLook L = KeysTownPaint::LookFor(TEXT("woman"), I); L.Hair = Hex(0xd9d9d9); L.HairStyle = 3; L.Shirt = Hex(0xb38fd1); L.Bottom = Hex(0xe9e1c8); L.bShortsLong = true; L.bGlasses = true; return L; }
		return KeysTownPaint::LookFor(Look, I);
	}

	void Blob(FKeysMesh& M, const FTransform& Xf, const FVector& C, const FVector& R, const FLinearColor& Col, const FQuat& Q = FQuat::Identity)
	{
		KeysShapes::AddBlob(M, Xf.TransformPosition(C), R * Xf.GetScale3D().X, Xf.GetRotation() * Q, Col, 7, 4);
	}
}

AKeysStreet::AKeysStreet()
{
	PrimaryActorTick.bCanEverTick = true;
	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent = Root;
	StaticMesh = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("StreetDetail"));
	StaticMesh->SetupAttachment(Root);
	StaticMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	LiveMesh = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("StreetLife"));
	LiveMesh->SetupAttachment(Root);
	LiveMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

void AKeysStreet::BeginPlay()
{
	Super::BeginPlay();
	// the town's own frame: (u along the frontage, v inland) -> local (u, -v)
	SetActorLocationAndRotation(KeysGeo::ToUE(OX(), 0, OZ()), FRotator(0, Dir() > 0 ? 0 : 180, 0));
	constexpr double HsA = 44.9, HsB = 43.7, SsA = 98.7, SsB = 97.3;
	auto Add = [this](const FString& Id, const FString& Look, const FScript& S, const FString& Prop = FString(), const FString& Group = FString(), double Scale = 1, int32 Bench = -1)
	{
		FPerson P;
		P.Id = Id; P.Look = Look; P.S = S; P.Prop = Prop; P.Group = Group; P.Scale = Scale; P.Bench = Bench;
		P.Hm.Init(CrowdLook(Look, People.Num() * 7 + 3), Bench >= 0);
		People.Add(MoveTemp(P));
	};
	// the errands
	Add(TEXT("v-tackle"), TEXT("angler"), Visit(TEXT("v-tackle"), TEXT("tackle"), -36, -58, HsA, HsB, 40, 10, true, 1.25, -41, 6), TEXT("bags"));
	Add(TEXT("v-diner"), TEXT("woman"), Visit(TEXT("v-diner"), TEXT("diner"), -27, -11, HsA, HsB, 55, 80, true), TEXT("coffee"));
	Add(TEXT("v-liquor"), TEXT("local"), Visit(TEXT("v-liquor"), TEXT("liquor"), -80, -98, HsA, HsB, 28, 40, true, 1.1), TEXT("bags"));
	Add(TEXT("v-bar"), TEXT("local"), Visit(TEXT("v-bar"), TEXT("bar"), 12, 36, HsA, HsB, 75, 150, false, 1.25, 16, 5));
	Add(TEXT("v-gifts"), TEXT("tourist"), Visit(TEXT("v-gifts"), TEXT("gifts"), 70, 96, HsA, HsB, 50, 20, true, 1.05, 76, 7), TEXT("bags"));
	Add(TEXT("v-pawn"), TEXT("local"), Visit(TEXT("v-pawn"), TEXT("pawn"), -96, -78, SsA, SsB, 35, 100, false));
	Add(TEXT("v-laundromat"), TEXT("woman"), Visit(TEXT("v-laundromat"), TEXT("laundromat"), -68, -48, SsA, SsB, 45, 200, true), TEXT("basket"));
	Add(TEXT("v-dive"), TEXT("angler"), Visit(TEXT("v-dive"), TEXT("dive"), -40, -16, SsA, SsB, 38, 60, false));
	Add(TEXT("v-motel"), TEXT("tourist"), Visit(TEXT("v-motel"), TEXT("motel"), 14, 40, SsA, SsB, 42, 140, true), TEXT("suitcase"));
	Add(TEXT("v-icecream"), TEXT("woman"), Visit(TEXT("v-icecream"), TEXT("icecream"), 62, 78, SsA, SsB, 30, 90, true, 1.25, 1e9, 0, 12), TEXT("cone"));
	Add(TEXT("v-stopngo"), TEXT("local"), Visit(TEXT("v-stopngo"), TEXT("stopngo"), 82, 98, SsA, SsB, 32, 30, true), TEXT("beer"));
	// the promenade
	Add(TEXT("jog-w"), TEXT("jogger"), Compile(TEXT("jog-w"), P2(-88, 25.9), {Go({P2(-30, 25.9)}, 3.3, EStance::Jog), Go({P2(40, 25.9)}, 2.8, EStance::Jog), Go({P2(88, 25.9)}, 3.1, EStance::Jog),
		Stay(12, EStance::Stretch, 0), Go({P2(50, 25.9)}, 3.0, EStance::Jog), Go({P2(-88, 25.9)}, 3.4, EStance::Jog), Stay(12, EStance::Stretch, PI)}, 0));
	{
		const TArray<FB> Couple = {Go({P2(-70, 23.5)}, .95), Stay(9, EStance::Phone, Sea), Go({P2(-44, 23.5)}, 1.15), Stay(14, EStance::Look, Sea), Go({P2(-16, 23.5)}, .85),
			Stay(6, EStance::Talk, Land), Go({P2(-60, 23.5)}, 1.25), Go({P2(-86, 23.5)}, 1.0)};
		Add(TEXT("couple-a"), TEXT("tourist"), Compile(TEXT("couple-a"), P2(-86, 23.5), Couple, 40), FString(), TEXT("couple"));
		Add(TEXT("couple-b"), TEXT("woman"), Compile(TEXT("couple-b"), P2(-86, 24.45), Shifted(Couple, .95), 40), FString(), TEXT("couple"));
	}
	Add(TEXT("angler-w"), TEXT("angler"), Compile(TEXT("angler-w"), P2(42, 23.4), {Go({P2(62.6, 23.4), P2(62.6, 21.8)}, 1.3, EStance::Carry, true), Go({P2(68.2, 21.8)}, 1.0, EStance::Carry, true),
		Stay(40, EStance::Stand, Sea, true), Go({P2(62.6, 21.8), P2(62.6, 23.4)}, 1.1, EStance::Carry, true), Go({P2(28, 23.4)}, 1.35, EStance::Carry, true), Stay(10, EStance::Look, Sea, true),
		Go({P2(42, 23.4)}, 1.2, EStance::Carry, true)}, 60), TEXT("rod"));
	Add(TEXT("dogwalk-a"), TEXT("woman"), Compile(TEXT("dogwalk-a"), P2(30, 25.1), {Go({P2(50, 25.1)}, 1.1), Stay(12, EStance::Look, Sea), Go({P2(88, 25.1)}, 1.0), Stay(8, EStance::Phone, Sea),
		Go({P2(30, 25.1)}, 1.3), Stay(10, EStance::Stand, 0)}, 25), FString(), TEXT("dogwalk-a"));
	Add(TEXT("plaque"), TEXT("tourist"), Compile(TEXT("plaque"), P2(26.5, 24.5), {Go({P2(23.5, 24.5), P2(23.5, 21.4)}, 1.2), Go({P2(16.0, 21.4)}, .9), Stay(16, EStance::Look, Land),
		Stay(7, EStance::Phone, Land), Stay(5, EStance::Look, Sea), Go({P2(23.5, 21.4), P2(23.5, 24.5)}, 1.15), Go({P2(26.5, 24.5)}, .9), Stay(9, EStance::Talk, Sea)}, 130));
	Add(TEXT("zebra-drive"), TEXT("tourist"), Compile(TEXT("zebra-drive"), P2(2.8, 27.0), {Stay(7, EStance::Look, Land), Go({P2(2.8, 40.9)}, 1.55), Stay(6, EStance::Phone, Land),
		Go({P2(2.8, 40.3)}, .7), Stay(5, EStance::Look, Sea), Go({P2(2.8, 27.0)}, 1.6)}, 15));
	Add(TEXT("ave-cross"), TEXT("local"), Compile(TEXT("ave-cross"), P2(-5.7, 48), {Go({P2(-5.7, 79.4)}, 1.35), Go({P2(-5.7, 83.2)}, 1.0), Stay(8, EStance::Look, Land), Go({P2(-5.7, 96.6)}, 1.7),
		Go({P2(-5.7, 98.2), P2(-7.4, 99.2)}, 1.0), Go({P2(-14, 99.2)}, 1.3), Stay(7, EStance::Look, Land), Go({P2(-7.4, 99.2), P2(-5.7, 98.2)}, 1.25), Go({P2(-5.7, 96.6)}, 1.0),
		Stay(6, EStance::Look, Land), Go({P2(-5.7, 83.6)}, 1.6), Go({P2(-5.7, 48)}, 1.15)}, 70));
	Add(TEXT("ave-phone"), TEXT("woman"), Compile(TEXT("ave-phone"), P2(-7.3, 48), {Go({P2(-7.3, 74)}, .8, EStance::Phone), Stay(9, EStance::Phone, Land), Go({P2(-7.3, 52)}, .9, EStance::Phone),
		Stay(14, EStance::Talk, Sea), Go({P2(-7.3, 48)}, .8)}, 5));
	Add(TEXT("ave-coffee"), TEXT("angler"), Compile(TEXT("ave-coffee"), P2(7.3, 98), {Go({P2(7.3, 92)}, 1.2, EStance::Carry, true), Go({P2(7.3, 70)}, 1.0, EStance::Carry, true),
		Stay(7, EStance::Look, Land, true), Go({P2(7.3, 52)}, 1.35, EStance::Carry, true), Go({P2(7.3, 98)}, 1.15, EStance::Carry, true)}, 100), TEXT("coffee"));
	{
		const TArray<FB> Mom = {Go({P2(64, 44.8)}, .85), Stay(6, EStance::Look, Land), Go({P2(53, 44.8)}, 1.1), Stay(8, EStance::Talk, Sea), Go({P2(56, 44.8)}, .9)};
		Add(TEXT("mom-a"), TEXT("woman"), Compile(TEXT("mom-a"), P2(56, 44.8), Mom, 50), FString(), TEXT("mom"));
		Add(TEXT("mom-b"), TEXT("kid"), Compile(TEXT("mom-b"), P2(56, 43.95), Shifted(Mom, -.85), 50), FString(), TEXT("mom"), .62);
	}
	Add(TEXT("dogwalk-b"), TEXT("granny"), Compile(TEXT("dogwalk-b"), P2(20, 83.1), {Go({P2(46, 83.1)}, 1.05), Stay(10, EStance::Stand, Land), Go({P2(78, 83.1)}, 1.25), Stay(14, EStance::Phone, Land),
		Go({P2(36, 83.1)}, 1.2), Go({P2(20, 83.1)}, .9)}, 175), FString(), TEXT("dogwalk-b"));
	Add(TEXT("blvd-phone"), TEXT("tourist"), Compile(TEXT("blvd-phone"), P2(-80, 83.1), {Go({P2(-60, 83.1)}, .9, EStance::Phone), Stay(8, EStance::Phone, Land), Go({P2(-24, 83.1)}, 1.3),
		Go({P2(-70, 83.1)}, 1.15), Go({P2(-96, 83.1)}, 1.0), Go({P2(-80, 83.1)}, 1.0)}, 110));
	Add(TEXT("jog-m"), TEXT("jogger-m"), Compile(TEXT("jog-m"), P2(-96, 80.9), {Go({P2(-16, 80.9)}, 2.9, EStance::Jog), Go({P2(-96, 80.9)}, 2.6, EStance::Jog), Go({P2(-60, 80.9)}, 3.0, EStance::Jog),
		Stay(14, EStance::Stretch, 0), Go({P2(-96, 80.9)}, 2.8, EStance::Jog)}, 20));
	Add(TEXT("loader"), TEXT("worker"), Compile(TEXT("loader"), P2(-56.6, 75.4), {Stay(3, EStance::Stand, 0), Go({P2(-56.6, 77.4)}, .8), Stay(1, EStance::Stand, 0, true),
		Go({P2(-53.6, 77.4)}, .8, EStance::Carry, true), Stay(2.5, EStance::Load, 0), Stay(10, EStance::Phone, 0), Stay(2.5, EStance::Load, 0, true), Go({P2(-56.6, 77.4)}, .8, EStance::Carry, true),
		Stay(2, EStance::Stand, 0), Go({P2(-57.6, 75.6)}, 1.0), Stay(5, EStance::Talk, 0), Go({P2(-56.6, 75.4)}, .9)}, 0), TEXT("cooler"));
	auto Talkers = [&](const TCHAR* Id, FPt A, FPt B, double YawA, const TCHAR* LA, const TCHAR* LB, double Off)
	{
		Add(FString(Id) + TEXT("-a"), LA, Compile(FString(Id) + TEXT("-a"), A, {Stay(20, EStance::Talk, YawA), Stay(12, EStance::Look, YawA + .6), Stay(18, EStance::Talk, YawA)}, Off), FString(), Id);
		Add(FString(Id) + TEXT("-b"), LB, Compile(FString(Id) + TEXT("-b"), B, {Stay(20, EStance::Talk, YawA + PI), Stay(12, EStance::Phone, YawA + PI - .5), Stay(18, EStance::Talk, YawA + PI)}, Off + 3), FString(), Id);
	};
	Talkers(TEXT("talk-south"), P2(46.2, 97.4), P2(47.4, 97.6), .2, TEXT("local"), TEXT("woman"), 30);
	Talkers(TEXT("talk-tackle"), P2(-52.6, 42.2), P2(-51.4, 42.4), .3, TEXT("local"), TEXT("granny"), 60);
	auto Seat = [&](const TCHAR* Id, const TCHAR* Look, int32 Bench, double Dx, const TArray<FB>& Beats, double Off, const TCHAR* Prop)
	{
		const double U = Benches().IsValidIndex(Bench) ? Benches()[Bench].U : 0;
		Add(Id, Look, Compile(Id, P2(U + Dx, 19.78), Beats, Off), Prop, FString(), 1, Bench);
	};
	Seat(TEXT("bench-reader"), TEXT("local"), 1, -.42, {Stay(50, EStance::Sit, Sea), Stay(20, EStance::SitPhone, Sea), Stay(30, EStance::Sit, Sea)}, 0, TEXT("newspaper"));
	Seat(TEXT("bench-couple-a"), TEXT("tourist"), 3, -.42, {Stay(40, EStance::Sit, Sea), Stay(25, EStance::Talk, Sea + .8), Stay(25, EStance::Sit, Sea)}, 0, TEXT("cone"));
	Seat(TEXT("bench-couple-b"), TEXT("woman"), 3, .42, {Stay(40, EStance::Sit, Sea), Stay(25, EStance::Talk, Sea - .8), Stay(25, EStance::Sit, Sea)}, 5, TEXT("cone"));
	Seat(TEXT("bench-phone"), TEXT("woman"), 5, .35, {Stay(60, EStance::SitPhone, Sea), Stay(30, EStance::Sit, Sea), Stay(30, EStance::SitPhone, Sea)}, 40, TEXT(""));

	// the dogs
	auto AddDog = [this](const TCHAR* Id, const TCHAR* Breed, const FScript* S, const TCHAR* Owner, double Ahead, double Dv)
	{
		FDog D;
		D.Id = Id; D.Breed = Breed;
		if (S) D.S = *S;
		if (Owner) { D.Owner = Owner; D.bLeash = true; D.Ahead = Ahead; D.Dv = Dv; }
		Dogs.Add(D);
	};
	AddDog(TEXT("dog-a"), TEXT("terrier"), nullptr, TEXT("dogwalk-a"), .7, -.5);
	AddDog(TEXT("dog-b"), TEXT("lab"), nullptr, TEXT("dogwalk-b"), .8, 0);
	{
		const FScript S = Compile(TEXT("dog-bench"), P2(-65.9, 20.9), {Stay(25, EStance::Lie, Land), Stay(8, EStance::Wag, Land), Stay(20, EStance::Sit, 0), Stay(17, EStance::Lie, Land)}, 0);
		AddDog(TEXT("dog-bench"), TEXT("beagle"), &S, nullptr, 0, 0);
	}
	{
		const FScript S = Compile(TEXT("dog-tied"), P2(94.2, 98.9), {Stay(60, EStance::Lie, Sea), Stay(20, EStance::Sit, Sea), Stay(40, EStance::Lie, Land), Stay(20, EStance::Sniff, .5)}, 0);
		AddDog(TEXT("dog-tied"), TEXT("blacklab"), &S, nullptr, 0, 0);
		Dogs.Last().bTie = true; Dogs.Last().TieU = 95.6; Dogs.Last().TieV = 99.3;
	}
	{
		const FScript S = Compile(TEXT("dog-loose-lot"), P2(0, 128), {Go({P2(1, 134)}, 1.4, EStance::Trot), Stay(6, EStance::Sniff, Land), Go({P2(0, 128)}, 1.1, EStance::Walk), Stay(4, EStance::Sniff, Sea),
			Go({P2(-34, 128)}, 1.1, EStance::Walk), Stay(9, EStance::Sniff, 0), Go({P2(-34, 152)}, 1.6, EStance::Trot), Stay(5, EStance::Sit, Land), Go({P2(-34, 162)}, 1.3, EStance::Trot),
			Stay(7, EStance::Sniff, Sea), Go({P2(-34, 128)}, 1.8, EStance::Run), Go({P2(0, 128)}, 1.5, EStance::Trot)}, 30);
		AddDog(TEXT("dog-loose-lot"), TEXT("mutt"), &S, nullptr, 0, 0);
	}
	{
		const FScript S = Compile(TEXT("dog-loose-lane"), P2(-10, 74.2), {Go({P2(-30, 74.2)}, .9, EStance::Walk), Stay(8, EStance::Sniff, PI), Go({P2(-40, 74.2)}, 1.1, EStance::Trot),
			Stay(6, EStance::Sniff, Land), Go({P2(-14, 74.2)}, 1.5, EStance::Trot), Stay(10, EStance::Groom, Land), Go({P2(-10, 74.2)}, .8, EStance::Walk)}, 55);
		AddDog(TEXT("dog-loose-lane"), TEXT("dachshund"), &S, nullptr, 0, 0);
	}
	{
		const FScript S = Compile(TEXT("dog-husky-lot"), P2(40, 162), {Go({P2(56, 162)}, 1.2, EStance::Walk), Stay(8, EStance::Sniff, 0), Go({P2(68, 162)}, 1.5, EStance::Trot),
			Stay(12, EStance::Lie, Sea), Go({P2(40, 162)}, 1.4, EStance::Trot), Stay(6, EStance::Sit, 0)}, 10);
		AddDog(TEXT("dog-husky-lot"), TEXT("husky"), &S, nullptr, 0, 0);
	}
	// squirrels up the palms (squirrelLoop)
	auto Squirrel = [this](const TCHAR* Id, double PU, double PV, double Hs, FPt DartFrom, FPt DartTo, double Off, double F1)
	{
		TArray<FPt> A;
		for (int32 i = 0; i <= 5; i++) A.Add(PalmTrunk(PU, PV, Hs, F1 * i / 5));
		TArray<FPt> Ad = A;
		Algo::Reverse(Ad);
		const FPt Foot = P2(PU, PV + 1.45), Lip = {PU, PV + 1.05, .5};
		TArray<FPt> Down(Ad.GetData(), Ad.Num() - 1);
		Down.Add(A[0]);
		const FScript S = Compile(Id, Foot, {Stay(4, EStance::Sit, -Land), Go({Lip}, 2.4, EStance::Hop), Go(A, 2.1, EStance::Climb), Stay(5, EStance::Flick, Land), Go(Down, 1.6, EStance::Climb),
			Go({P2(PU, PV + 1.45)}, 2.4, EStance::Hop), Stay(2.5, EStance::Sit, Land), Go({DartFrom, DartTo}, 5.8, EStance::Run), Stay(6, EStance::Flick, Land), Go({DartFrom, Foot}, 5.4, EStance::Run)}, Off);
		Critters.Add({Id, 0, S, PU, PV});
	};
	Squirrel(TEXT("squirrel-prom"), -54, 22, 1.15, P2(-54, 24.2), P2(-76, 24.2), 10, .42);
	Squirrel(TEXT("squirrel-blvd"), -9.5, 97.5, 1.0, P2(-9.5, 99.0), P2(-9.5, 83.0), 35, .38);
	Squirrel(TEXT("squirrel-alley"), -38, 110, .9, P2(-38, 111.6), P2(-33, 117.5), 60, .42);
	// skunks round the bins at dusk (skunkRing), and the one who commutes across the avenue
	auto Skunk = [this](const TCHAR* Id, double U, double Off)
	{
		const double A = U - 2.9, B = U + 2.9, N = 75.3, S = 79.5;
		Critters.Add({Id, 1, Compile(Id, P2(A + 1, N), {Go({P2(A + 1, N)}, .38, EStance::Amble), Stay(9, EStance::Sniff, Land), Go({P2(B, N + .2)}, .4, EStance::Amble), Stay(6, EStance::Sniff, 0),
			Go({P2(B + .6, 77.4), P2(B, S)}, .36, EStance::Amble), Stay(12, EStance::Sniff, PI), Go({P2(U, S)}, .4, EStance::Amble), Stay(4, EStance::Freeze, PI),
			Go({P2(A - .6, 77.4), P2(A, N)}, .37, EStance::Amble), Stay(7, EStance::Sniff, Land), Go({P2(A + 1, N)}, .3, EStance::Amble)}, Off)});
	};
	Skunk(TEXT("skunk-diner"), -24, 0);
	Skunk(TEXT("skunk-bar"), 30, 120);
	Critters.Add({TEXT("skunk-commuter"), 1, Compile(TEXT("skunk-commuter"), P2(-19.5, 79.3), {Go({P2(-6, 79.3)}, .55, EStance::Amble), Stay(10, EStance::Sniff, Land), Go({P2(-1.5, 79.3)}, .5, EStance::Amble),
		Stay(16, EStance::Freeze, 0), Go({P2(8, 79.3)}, .62, EStance::Amble), Stay(8, EStance::Sniff, Sea), Go({P2(24.4, 79.3)}, .5, EStance::Amble), Stay(12, EStance::Sniff, 0),
		Go({P2(8, 79.3)}, .6, EStance::Amble), Go({P2(-6, 79.3)}, .55, EStance::Amble), Stay(7, EStance::Sniff, PI), Go({P2(-19.5, 79.3)}, .5, EStance::Amble)}, 90)});
	BuildStatic();
	UE_LOG(LogTemp, Log, TEXT("Keys Run: street life - %d people, %d dogs, %d critters"), People.Num(), Dogs.Num(), Critters.Num());
}

// streetlife.ts: the litter where litter collects, puddles, the washing across the alley, and the street furniture
void AKeysStreet::BuildStatic()
{
	FKeysMesh M;
	const double S = PadY + .035;
	auto At = [](double U, double V, double Y) { return FVector(U, -V, Y); };
	struct FLit { double U, V; int32 K; double Rot; };
	// cup, can, bottle, paper, butts, bag, box, wrapper
	const FLit Litter[] = {{7.6, 44.9, 4, .4}, {5.6, 44.3, 4, 1.9}, {23.4, 45.3, 0, .5}, {18.8, 45.6, 7, 2.2}, {-11.4, 40.5, 1, 1.1}, {-33.0, 40.6, 3, .3}, {72.4, 40.7, 2, 2.6}, {-70.5, 40.5, 0, .9},
		{-88.6, 41.4, 5, 0}, {-45.4, 45.4, 4, .8}, {-22.4, 75.0, 6, .35}, {-26.4, 78.9, 7, 1.4}, {-21.3, 79.3, 5, .8}, {28.6, 79.3, 0, 2.9}, {32.9, 75.4, 1, .6}, {12.0, 79.5, 3, 1.2},
		{-7.3, 70.2, 7, 2.0}, {6.4, 55.0, 1, 1.7}, {18.0, 26.6, 3, .7}, {-60.3, 20.9, 0, 1.0}, {41.6, 21.0, 7, 2.4}, {87.0, 20.7, 4, .2}, {82.2, 97.7, 0, .4}, {84.5, 99.2, 4, 1.3},
		{-16.3, 99.0, 4, 2.1}, {-82.0, 99.1, 5, .5}, {88.0, 140.0, 1, 1.4}, {-20.0, 160.0, 6, .9}, {-36.2, 112.4, 2, .7}, {-41.0, 104.0, 7, 1.8}, {-33.2, 118.0, 3, .4}, {14.2, 98.9, 0, 2.7},
		{52.0, 97.9, 7, .9}, {64.6, 99.0, 4, .1}, {-3.2, 88.0, 3, 1.0}, {3.6, 33.0, 7, 2.3}};
	for (const FLit& L : Litter)
	{
		const FQuat Q(FVector::ZAxisVector, -L.Rot);
		const bool bAlt = int32(L.Rot * 10) % 2 == 1;
		switch (L.K)
		{
			case 0: M.AddBox(At(L.U, L.V, S + .04), FVector(.1, .08, .08), Hex(0xf4f1e8), Q); M.AddBox(At(L.U, L.V, S + .045), FVector(.03, .086, .086), Hex(0xd9342b), Q); break;
			case 1: M.AddBox(At(L.U, L.V, S + .033), FVector(.12, .066, .066), Hex(bAlt ? 0xc6ccd1 : 0x2f8f4a), Q); break;
			case 2: M.AddBox(At(L.U, L.V, S + .03), FVector(.24, .06, .06), Hex(bAlt ? 0x2f6b3a : 0x6b4a2a), Q); break;
			case 3: M.AddBox(At(L.U, L.V, S + .003), FVector(.28, .2, .005), Hex(0xe9e3d2), Q); M.AddBox(At(L.U + .05, L.V + .03, S + .007), FVector(.2, .14, .004), Hex(0xdad2bd), FQuat(FVector::ZAxisVector, -L.Rot - .5)); break;
			case 4: for (int32 i = 0; i < 4; i++) M.AddBox(At(L.U + FMath::Sin(i * 2.1) * .07, L.V + FMath::Cos(i * 1.7) * .07, S + .008), FVector(.055, .016, .014), Hex(i % 3 ? 0xd9c8a0 : 0xe9e4d8), FQuat(FVector::ZAxisVector, -L.Rot - i)); break;
			case 5: M.AddBox(At(L.U, L.V, S + .03), FVector(.26, .2, .06), Hex(0xf0efe8), Q); break;
			case 6: M.AddBox(At(L.U, L.V, S + .05), FVector(.5, .38, .1), Hex(0xa57f52), Q); break;
			default: M.AddBox(At(L.U, L.V, S + .003), FVector(.11, .07, .004), Hex(bAlt ? 0xd9342b : 0xf2c14e), Q); break;
		}
	}
	// puddles: dark glossy ovals where water sits
	struct FPud { double U, V, RX, RZ, Rot; };
	const FPud Puds[] = {{-27.6, 74.0, 1.5, .8, .2}, {25.8, 79.0, 1.2, .7, -.3}, {-36.8, 108.6, 1.8, 1.0, .5}, {-45.0, 39.5, 2.4, .4, 0}, {21.5, 39.4, 1.7, .4, 0}, {-30.0, 28.6, 2.6, .45, 0},
		{60.0, 28.7, 1.4, .4, 0}, {4.4, 62.0, .9, 1.5, .1}, {76.0, 87.0, 2.2, .6, 0}, {-12.4, 135.0, 2.0, 1.2, .4}};
	for (const FPud& Pd : Puds)
	{
		const int32 N = 14;
		const FVector C = At(Pd.U, Pd.V, PadY + .025);
		for (int32 k = 0; k < N; k++)
		{
			const double A0 = 2 * PI * k / N, A1 = 2 * PI * (k + 1) / N, Cr = FMath::Cos(Pd.Rot), Sr = FMath::Sin(Pd.Rot);
			auto E = [&](double A) { const double X = FMath::Cos(A) * Pd.RX, Y = FMath::Sin(A) * Pd.RZ; return C + FVector(X * Cr - Y * Sr, -(X * Sr + Y * Cr), 0); };
			M.AddTri(C, E(A0), E(A1), Hex(0x3a4650), FVector(0, 0, 1));
		}
	}
	// the washing across the alley
	{
		const double U0 = -44, U1 = -30, V = 106.5, Y = PadY + 3.5, Sag = .45;
		FVector Prev = At(U0, V, Y);
		const uint32 Cloth[] = {0xf2f2f2, 0x2f6fa8, 0xd7263d, 0xf4c542, 0x3fb68b, 0xe8a0c0};
		for (int32 i = 1; i <= 14; i++)
		{
			const double F = i / 14.0, U = FMath::Lerp(U0, U1, F), Yy = Y - Sag * 4 * F * (1 - F);
			const FVector Cur = At(U, V, Yy);
			M.AddTube(Prev, Cur, .01, .01, 3, Hex(0xdddddd));
			if (i % 2 == 0 && i < 14) M.AddBox(Cur + FVector(0, 0, -.32), FVector(.55, .02, .6), Hex(Cloth[(i / 2) % 6]));
			Prev = Cur;
		}
	}
	// news boxes, bike racks, sandwich boards, an ice chest, a stack of crates
	auto News = [&](double U, double V, uint32 C) { M.AddBox(At(U, V, PadY + .31), FVector(.08, .08, .62), Hex(0x3a3d42)); M.AddBox(At(U, V, PadY + .87), FVector(.46, .4, .5), Hex(C)); M.AddBox(At(U, V - .2, PadY + .92), FVector(.36, .02, .2), Hex(0x1d2a33)); };
	News(-10.0, 45.7, 0x2a5ea8);
	News(77.0, 99.5, 0xb3202a);
	auto Rack = [&](double U, double V) { for (int32 i = 0; i < 4; i++) M.AddTube(At(U - .9 + i * .6, V, PadY), At(U - .9 + i * .6, V, PadY + .8), .025, .025, 5, Hex(0x9aa0a6)); M.AddTube(At(U - .9, V, PadY + .8), At(U + .9, V, PadY + .8), .025, .025, 5, Hex(0x9aa0a6)); };
	Rack(95.6, 99.4);
	Rack(92.0, 45.75);
	auto AFrame = [&](double U, double V)
	{
		for (int32 s = -1; s <= 1; s += 2)
		{
			const FQuat Q(FVector::XAxisVector, s * .22);
			M.AddBox(At(U, V + s * .17, PadY + .48), FVector(.62, .03, .95), Hex(0x2b2b2b), Q);
			M.AddBox(At(U, V + s * .19, PadY + .5), FVector(.54, .01, .8), Hex(0x1f2a24), Q);
		}
	};
	AFrame(-16.4, 45.7); AFrame(66.0, 99.62); AFrame(-22.5, 99.62);
	M.AddBox(At(-96.5, 45.7, PadY + .4), FVector(1.2, .6, .8), Hex(0xe8eef2));
	M.AddBox(At(-96.5, 45.7, PadY + .82), FVector(1.22, .62, .06), Hex(0x2a5ea8));
	for (int32 i = 0; i < 4; i++) M.AddBox(At(-34.6 + (i % 2) * .5, 45.7, PadY + .2 + (i / 2) * .4), FVector(.45, .45, .38), Hex(i % 3 ? 0x8a6b45 : 0xa57f52));
	M.Commit(StaticMesh, 0, AKeysWorld::VertexColorMaterial());
}

void AKeysStreet::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	FVector Cam = FVector::ZeroVector;
	if (APlayerCameraManager* CM = UGameplayStatics::GetPlayerCameraManager(this, 0)) { const FVector L = CM->GetCameraLocation(); Cam = FVector(L.X / 100, L.Z / 100, L.Y / 100); }
	double CU, CV;
	ToLocal(Cam.X, Cam.Z, CU, CV);
	const bool bNear = FMath::Abs(CU) < 260 && CV > -120 && CV < 300;
	LiveMesh->SetVisibility(bNear);
	StaticMesh->SetVisibility(bNear);
	if (!bNear) return;
	// the street runs on the wall clock, so it is the same scene for everybody (town-life.ts)
	const double T = double(FDateTime::UtcNow().GetTicks() % (int64(StreetPeriod) * ETimespan::TicksPerSecond)) / ETimespan::TicksPerSecond;
	Draw(T, FMath::Min(double(DeltaSeconds), .1), Cam);
}

void AKeysStreet::Draw(double T, double Dt, const FVector& CamGame)
{
	double CU, CV;
	ToLocal(CamGame.X, CamGame.Z, CU, CV);
	const double CY = CamGame.Y;
	auto Near = [&](double U, double V, double R) { return FMath::Square(U - CU) + FMath::Square(V - CV) < R * R; };
	auto At = [](double U, double V, double Y) { return FVector(U, -V, PadY + Y); };
	const double Dusk = AKeysWorld::Dusk(this);
	const double Clock = GetWorld()->GetTimeSeconds();
	FKeysMesh M;
	TMap<FString, FLifePose> Poses;
	// people
	for (int32 i = 0; i < People.Num(); i++)
	{
		FPerson& P = People[i];
		FLifePose Pz;
		Sample(P.S, T, Pz);
		Poses.Add(P.Id, Pz);
		if (Pz.bHidden || !Near(Pz.U, Pz.V, PeopleRange) || CY > PadY + 40) continue;
		FKeysHuman& Hm = P.Hm;
		Hm.Pos = At(Pz.U, Pz.V, Pz.Y + (Hm.bSeated ? .02 : 0)) + FVector(0, 0, 0);
		Hm.Yaw = -Pz.Yaw;
		Hm.Pitch = Hm.Roll = 0;
		Hm.IdleUpdate(Clock + i, 0);
		const double HipY = Hm.HipY;
		auto J = [](double X, double Y, double Z) { return FKeysHuman::J(X, Y, Z); };
		const bool bMove = Pz.Speed > .05;
		if (bMove)
		{
			const bool bJog = Pz.Stance == EStance::Jog;
			P.Gait += Dt * Pz.Speed * (bJog ? 3.4 : 2.6);
			const double G = P.Gait, St = (bJog ? .36 : .26) * FMath::Min(1.0, Pz.Speed);
			Hm.PoseLegs(J(-.105, .085 + FMath::Max(0.0, FMath::Sin(G)) * (bJog ? .2 : .1), FMath::Sin(G) * St), J(.105, .085 + FMath::Max(0.0, -FMath::Sin(G)) * (bJog ? .2 : .1), -FMath::Sin(G) * St));
			if (Pz.bCarry || Pz.Stance == EStance::Carry) Hm.Pose(J(-.24, HipY + .02, -FMath::Sin(G) * .2), J(.2, HipY + .2, .25));
			else if (Pz.Stance == EStance::Phone) Hm.Pose(J(-.24, HipY + .02, -FMath::Sin(G) * .2), J(.12, HipY + .62, .06));
			else if (bJog) Hm.Pose(J(-.22, HipY + .3, -FMath::Sin(G) * .3), J(.22, HipY + .3, FMath::Sin(G) * .3));
			else Hm.Pose(J(-.24, HipY + .02, -FMath::Sin(G) * .22), J(.24, HipY + .02, FMath::Sin(G) * .22));
			Hm.Pos.Z += FMath::Abs(FMath::Sin(G)) * (bJog ? .07 : .03);
		}
		else
		{
			Hm.DefaultPose();
			const double Ph = Clock * 1.3 + i;
			switch (Pz.Stance)
			{
				case EStance::Phone: case EStance::SitPhone: Hm.Pose(J(-.2, HipY + (Hm.bSeated ? .1 : .02), .1), J(.12, HipY + .62, .06)); break;
				case EStance::Talk: Hm.Pose(J(-.22, HipY + .25 + FMath::Max(0.0, FMath::Sin(Ph * 2)) * .15, .3), J(.22, HipY + .2 + FMath::Max(0.0, FMath::Sin(Ph * 2 + 2)) * .15, .3)); break;
				case EStance::Look: Hm.Pose(J(-.12, HipY - .02, -.14), J(.12, HipY - .02, -.14)); Hm.HeadNod = -.08; break;
				case EStance::Knock: Hm.Pose(J(-.24, HipY + .02, .05), J(.15, HipY + .55 + FMath::Abs(FMath::Sin(Ph * 6)) * .05, .45)); break;
				case EStance::Stretch: Hm.Pose(J(-.15, HipY + 1.0 + FMath::Sin(Ph) * .05, .05), J(.15, HipY + 1.0 - FMath::Sin(Ph) * .05, .05)); break;
				case EStance::Load: Hm.Pose(J(-.2, HipY + .25, .45), J(.2, HipY + .25, .45)); break;
				case EStance::Carry: Hm.Pose(J(-.24, HipY + .02, .05), J(.2, HipY + .2, .25)); break;
				default: if (Pz.bCarry) Hm.Pose(J(-.24, HipY + .02, .05), J(.2, HipY + .2, .25)); break;
			}
		}
		if (!FMath::IsNearlyEqual(P.Scale, 1.0))
		{
			// the kid: build into a scratch mesh and scale about the feet
			FKeysMesh K;
			const FVector Feet = Hm.Pos;
			Hm.Pos = FVector::ZeroVector;
			Hm.Build(K);
			Hm.Pos = Feet;
			for (FVector& V : K.V) V = Feet * 100 + V * P.Scale;
			const int32 Base = M.V.Num();
			M.V.Append(K.V); M.N.Append(K.N); M.UV.Append(K.UV); M.C.Append(K.C);
			for (int32 Ix : K.T) M.T.Add(Base + Ix);
		}
		else Hm.Build(M);
		// what they carry: the hand is about where the pose put it
		if (!P.Prop.IsEmpty() && (Pz.bCarry || Pz.Stance == EStance::Carry || P.Bench >= 0 || P.Prop == TEXT("rod")))
		{
			const FTransform Xf = Hm.ToBoat();
			const FVector Hand = Xf.TransformPosition(Hm.HandR);
			const FQuat Q = Xf.GetRotation();
			if (P.Prop == TEXT("bags")) M.AddBox(Hand + FVector(0, 0, -.2), FVector(.3, .14, .32), Hex(0xd8c8a0), Q);
			else if (P.Prop == TEXT("coffee") || P.Prop == TEXT("beer")) M.AddTube(Hand, Hand + FVector(0, 0, .14), .04, .045, 6, Hex(P.Prop == TEXT("beer") ? 0x2f6fa8 : 0xf4f1e8));
			else if (P.Prop == TEXT("cone")) { M.AddTube(Hand, Hand + FVector(0, 0, .12), .005, .035, 6, Hex(0xd9a25a)); KeysShapes::AddBlob(M, Hand + FVector(0, 0, .14), FVector(.04, .04, .04), FQuat::Identity, Hex(0xb5e08a), 6, 3); }
			else if (P.Prop == TEXT("basket")) M.AddBox(Hand + FVector(0, 0, -.1), FVector(.5, .38, .26), Hex(0xd8d0c0), Q);
			else if (P.Prop == TEXT("suitcase")) M.AddBox(Hand + FVector(0, 0, -.3), FVector(.18, .5, .42), Hex(0x2a3d52), Q);
			else if (P.Prop == TEXT("cooler")) M.AddBox(Hand + FVector(0, 0, -.15), FVector(.6, .4, .4), Hex(0xd9342b), Q);
			else if (P.Prop == TEXT("newspaper")) M.AddBox(Hand + FVector(0, 0, .1), FVector(.02, .32, .26), Hex(0xe9e4d8), Q);
			else if (P.Prop == TEXT("rod")) M.AddTube(Hand, Hand + Q.RotateVector(FVector(-.6, 0, 1.9)), .012, .006, 4, Hex(0x23262a));
		}
		// the loader's cooler, put down where he left it
		if (P.Id == TEXT("loader") && !Pz.bCarry && Pz.Stance != EStance::Carry)
		{
			const bool bInBed = Pz.Beat >= 4 && Pz.Beat <= 5;
			M.AddBox(At(bInBed ? -51.4 : -57.8, 77.4, (bInBed ? 1.0 : 0) + .2), FVector(.6, .4, .4), Hex(0xd9342b));
		}
	}
	// dogs
	for (FDog& D : Dogs)
	{
		FLifePose Pz;
		if (D.bLeash)
		{
			const FLifePose* O = Poses.Find(D.Owner);
			if (!O) continue;
			const double K = O->Speed > .05 ? 1 : .35;
			Pz.U = O->U + FMath::Cos(O->Yaw) * D.Ahead * K; Pz.V = O->V + FMath::Sin(O->Yaw) * D.Ahead * K + D.Dv;
			Pz.Yaw = O->Yaw; Pz.Speed = O->Speed; Pz.bHidden = O->bHidden;
			Pz.Stance = O->Speed > 1.7 ? EStance::Trot : O->Speed > .05 ? EStance::Walk : (int32(O->Bt / 6) % 2 ? EStance::Sniff : EStance::Sit);
		}
		else Sample(D.S, T, Pz);
		if (Pz.bHidden || !Near(Pz.U, Pz.V, DogRange)) continue;
		const bool bMove = Pz.Speed > .05;
		D.Step += Dt * (Pz.Stance == EStance::Run ? 16 : 10) * FMath::Max(.4, Pz.Speed);
		KeysDog::FPose Dp;
		Dp.Act = Pz.Stance == EStance::Lie ? KeysDog::EAct::Lie : Pz.Stance == EStance::Sniff ? KeysDog::EAct::Sniff : Pz.Stance == EStance::Sit ? KeysDog::EAct::Sit : bMove ? KeysDog::EAct::Trot : KeysDog::EAct::Sit;
		Dp.bTrotting = bMove; Dp.Step = D.Step; Dp.Wag = Pz.Stance == EStance::Wag ? 1 : bMove ? .6 : .25; Dp.T = Clock; Dp.Ph = D.Id.Len();
		const FTransform Xf(FQuat(FVector::ZAxisVector, -Pz.Yaw), At(Pz.U, Pz.V, 0));
		KeysDog::Build(M, KeysDog::Look(D.Breed), Dp, Xf, 1.15);
		// the lead, from the owner's hand to the collar
		if (D.bLeash)
			if (const FPerson* O = People.FindByPredicate([&D](const FPerson& Q) { return Q.Id == D.Owner; }))
			{
				const FVector Hand = O->Hm.ToBoat().TransformPosition(O->Hm.HandL);
				const FVector Collar = Xf.TransformPosition(FVector(.18, 0, .3) * KeysDog::Look(D.Breed).Scale);
				M.AddTube(Hand, FMath::Lerp(Hand, Collar, .5) + FVector(0, 0, -.12), .008, .008, 3, Hex(0xd7263d));
				M.AddTube(FMath::Lerp(Hand, Collar, .5) + FVector(0, 0, -.12), Collar, .008, .008, 3, Hex(0xd7263d));
			}
		if (D.bTie) M.AddTube(At(D.TieU, D.TieV, .6), Xf.TransformPosition(FVector(.18, 0, .3) * KeysDog::Look(D.Breed).Scale), .008, .008, 3, Hex(0x2f6fa8));
	}
	// squirrels and skunks (critters.ts): a body, a head and the tail that is the whole character
	for (const FCritter& C : Critters)
	{
		if (C.Kind == 1 && Dusk < .12) continue;   // skunks are out from dusk
		FLifePose Pz;
		Sample(C.S, T, Pz);
		if (!Near(Pz.U, Pz.V, CritterRange)) continue;
		const double Grow = C.Kind == 1 ? FMath::Clamp((Dusk - .12) / .18, 0.0, 1.0) : 1;
		const bool bClimb = Pz.Stance == EStance::Climb;
		const double Bob = Pz.Speed > .05 && !bClimb ? FMath::Abs(FMath::Sin(Clock * (C.Kind ? 9 : 18))) * (C.Kind ? .01 : .04) : 0;
		const double Yaw = bClimb ? -PI / 2 : -Pz.Yaw;
		FQuat Q = FQuat(FVector::ZAxisVector, Yaw);
		if (bClimb) Q = Q * FQuat(FVector::YAxisVector, Pz.Speed > 0 && Pz.Beat == 4 ? 1.3 : -1.3);   // nose up the trunk, or down it
		const FTransform Xf(Q, At(Pz.U, Pz.V, Pz.Y + Bob), FVector(Grow * (C.Kind ? 1.0 : 1.3)));
		if (C.Kind == 0)
		{
			const bool bUp = Pz.Stance == EStance::Sit || Pz.Stance == EStance::Flick;
			const FLinearColor Fur = Hex(0x8b7a66) * 1.2f, Belly = Hex(0xd8cbb4);
			Blob(M, Xf, FVector(0, 0, .07), FVector(.1, .05, .055), Fur, bUp ? FQuat(FVector::YAxisVector, -1.0) : FQuat::Identity);
			Blob(M, Xf, bUp ? FVector(.03, 0, .16) : FVector(.1, 0, .09), FVector(.04, .032, .035), Fur);
			Blob(M, Xf, bUp ? FVector(.045, 0, .1) : FVector(.04, 0, .05), FVector(.04, .035, .03), Belly);
			const double Flick = Pz.Stance == EStance::Flick ? FMath::Sin(Clock * 14) * .25 : 0;
			Blob(M, Xf, FVector(-.1, 0, .14), FVector(.05, .045, .09), Fur * 1.05f, FQuat(FVector::YAxisVector, .4 + Flick));
			Blob(M, Xf, FVector(-.13, 0, .24), FVector(.06, .05, .06), Fur * 1.1f, FQuat(FVector::YAxisVector, Flick));
		}
		else
		{
			const FLinearColor Black = Hex(0x17171a), White = Hex(0xf2f2ee);
			Blob(M, Xf, FVector(0, 0, .1), FVector(.2, .09, .085), Black);
			Blob(M, Xf, FVector(0, 0, .165), FVector(.18, .025, .025), White);
			Blob(M, Xf, FVector(.2, 0, .09), FVector(.07, .055, .05), Black);
			Blob(M, Xf, FVector(.24, 0, .12), FVector(.04, .012, .012), White);
			const bool bRaise = Pz.Stance == EStance::Freeze || Near(Pz.U, Pz.V, 4);
			Blob(M, Xf, bRaise ? FVector(-.2, 0, .3) : FVector(-.26, 0, .16), FVector(.08, .07, bRaise ? .16 : .08), Black, FQuat(FVector::YAxisVector, bRaise ? 0 : .8));
			Blob(M, Xf, bRaise ? FVector(-.2, 0, .32) : FVector(-.27, 0, .18), FVector(.02, .072, bRaise ? .14 : .07), White, FQuat(FVector::YAxisVector, bRaise ? 0 : .8));
		}
	}
	// the cat on the dumpster lid: asleep by day, up and grooming at dusk
	if (Near(30.9, 77.4, CritterRange))
	{
		const bool bAwake = Dusk > .3;
		const FTransform Xf(FQuat(FVector::ZAxisVector, -Land), At(30.9, 77.4, 1.4));
		const FLinearColor Ginger = Hex(0xe0893a) * 1.25f, Cream = Hex(0xf2dcb8);
		if (bAwake)
		{
			Blob(M, Xf, FVector(0, 0, .14), FVector(.11, .09, .14), Ginger, FQuat(FVector::YAxisVector, -.5));
			Blob(M, Xf, FVector(.06, 0, .3), FVector(.065, .06, .06), Ginger);
			Blob(M, Xf, FVector(.1, 0, .28), FVector(.03, .04, .03), Cream);
			for (int32 s = -1; s <= 1; s += 2) M.AddTri(Xf.TransformPosition(FVector(.06, s * .03, .35)), Xf.TransformPosition(FVector(.06, s * .055, .35)), Xf.TransformPosition(FVector(.06, s * .045, .4)), Ginger, Xf.GetRotation().RotateVector(FVector(1, 0, 0)));
			Blob(M, Xf, FVector(-.12, 0, .05), FVector(.14, .025, .025), Ginger, FQuat(FVector::ZAxisVector, FMath::Sin(Clock * 2) * .4));
		}
		else
		{
			Blob(M, Xf, FVector(0, 0, .06), FVector(.17, .12, .07), Ginger);
			Blob(M, Xf, FVector(.12, .05, .08), FVector(.06, .06, .05), Ginger);
			Blob(M, Xf, FVector(-.05, -.1, .05), FVector(.14, .03, .03), Ginger);
		}
	}
	// gulls on the rooflines (gullAt): perched, or flapping across to the next perch
	{
		struct FPerch { double U, V, Y, Yaw; };
		auto Roof = [](const TCHAR* Id, double U) { const FLot* L = LotById(Id); return FPerch{U, L ? L->V0 + .06 : 46, L ? L->H + .82 : 5, Sea}; };
		auto Lamp = [](double U, double V) { return FPerch{U, V, 4.5, Sea}; };
		struct FGull { TArray<FPerch> Ps; TArray<double> Hold; double Off; };
		static TArray<FGull> Gulls;
		if (!Gulls.Num())
		{
			Gulls.Add({{Roof(TEXT("tackle"), -50), Lamp(-60, 27)}, {38, 55}, 0});
			Gulls.Add({{Roof(TEXT("tackle"), -49), Roof(TEXT("diner"), -28)}, {52, 30, 44}, 20});
			Gulls.Add({{Roof(TEXT("pawn"), -88), Roof(TEXT("laundromat"), -48)}, {61, 47}, 10});
			Gulls.Add({{Roof(TEXT("stopngo"), 96), Roof(TEXT("icecream"), 62)}, {42, 58}, 33});
			Gulls.Add({{Lamp(30, 41.2), Lamp(60, 27)}, {70, 45}, 15});
			Gulls.Add({{Roof(TEXT("bar"), 30), Roof(TEXT("gifts"), 90)}, {49, 63}, 5});
		}
		for (const FGull& G : Gulls)
		{
			const int32 N = G.Ps.Num();
			const double Fly = 3.2;
			double Cyc = Fly * N;
			for (double H : G.Hold) Cyc += H;
			double Tt = FMath::Fmod(T + G.Off, Cyc);
			FPerch At0 = G.Ps[0];
			double Flap = 0;
			for (int32 i = 0; i < N; i++)
			{
				const double Hold = G.Hold[i % G.Hold.Num()];
				const FPerch& A = G.Ps[i];
				const FPerch& B = G.Ps[(i + 1) % N];
				if (Tt < Hold) { At0 = A; break; }
				Tt -= Hold;
				if (Tt < Fly)
				{
					const double F = Tt / Fly, E = F * F * (3 - 2 * F);
					At0 = {A.U + (B.U - A.U) * E, A.V + (B.V - A.V) * E, A.Y + (B.Y - A.Y) * E + FMath::Sin(F * PI) * 3.4, FMath::Atan2(B.V - A.V, B.U - A.U)};
					Flap = 1;
					break;
				}
				Tt -= Fly;
			}
			if (!Near(At0.U, At0.V, GullRange)) continue;
			const FTransform Xf(FQuat(FVector::ZAxisVector, -At0.Yaw), At(At0.U, At0.V, At0.Y), FVector(1.4));
			const FLinearColor White = Hex(0xf4f4f0), Grey = Hex(0x9aa3ab), Beak = Hex(0xf2c14e);
			Blob(M, Xf, FVector(0, 0, .12), FVector(.2, .075, .08), White);
			Blob(M, Xf, FVector(.16, 0, .2), FVector(.055, .045, .05), White);
			M.AddTube(Xf.TransformPosition(FVector(.2, 0, .2)), Xf.TransformPosition(FVector(.28, 0, .19)), .012 * 1.4, .004, 4, Beak);
			if (Flap > 0)
			{
				const double W = FMath::Sin(Clock * 14) * .6;
				for (int32 s = -1; s <= 1; s += 2)
				{
					const FVector Root0 = Xf.TransformPosition(FVector(0, s * .05, .15)), Tip = Xf.TransformPosition(FVector(-.05, s * .62, .15 + W * .3));
					M.AddTri(Root0 + Xf.GetRotation().RotateVector(FVector(.1, 0, 0)), Tip, Root0 - Xf.GetRotation().RotateVector(FVector(.1, 0, 0)), Grey, FVector(0, 0, 1));
					M.AddTri(Root0 + Xf.GetRotation().RotateVector(FVector(.1, 0, 0)), Root0 - Xf.GetRotation().RotateVector(FVector(.1, 0, 0)), Tip, Grey, FVector(0, 0, -1));
				}
			}
			else for (int32 s = -1; s <= 1; s += 2) Blob(M, Xf, FVector(-.04, s * .06, .15), FVector(.18, .02, .05), Grey);
			for (int32 s = -1; s <= 1; s += 2) if (Flap == 0) M.AddTube(Xf.TransformPosition(FVector(0, s * .03, .06)), Xf.TransformPosition(FVector(0, s * .03, 0)), .006 * 1.4, .006 * 1.4, 3, Beak);
		}
	}
	M.Commit(LiveMesh, 0, AKeysWorld::VertexColorMaterial());
}
