#include "KeysEvents.h"
#include "KeysBoatPawn.h"
#include "KeysGeo.h"
#include "KeysData.h"
#include "KeysMesh.h"
#include "KeysWorld.h"
#include "KeysAudio.h"
#include "KeysSave.h"
#include "ProceduralMeshComponent.h"
#include "EngineUtils.h"
#include "Kismet/GameplayStatics.h"

using namespace KeysGeo;
using namespace KeysData;

namespace
{
	// ---- race (sim/race.ts)
	struct FCourseBuoy { double X, DZ; const TCHAR* Name; };
	const FCourseBuoy MarathonCourse[] = {
		{-1350, 900, TEXT("Start / Finish")}, {-600, 900, TEXT("Hawk 1")}, {250, 900, TEXT("Hawk 2")}, {1100, 1080, TEXT("East Turn")},
		{1450, 1330, TEXT("Reef East")}, {650, 1395, TEXT("Sombrero")}, {-250, 1330, TEXT("Reef West")}, {-1050, 1120, TEXT("West Turn")},
		{-1600, 1010, TEXT("Inshore")}};
	constexpr double BuoyRadius = 55;
	constexpr double RaceCountdown = 3;
	// race/opponents.ts FIELD
	struct FRaceEntry { const TCHAR* Boat; double Pace, Line; const TCHAR* Name; };
	const FRaceEntry Field[] = {{TEXT("mti"), 1.04, 10, TEXT("Reel Deal")}, {TEXT("midnight"), .97, -12, TEXT("Sand Bar")}, {TEXT("freeman"), .91, 22, TEXT("Conch Runner")}};

	// ---- tournament (sim/tournament.ts)
	constexpr int32 TourneyN = 5;
	constexpr double TourneyDuration = 15 * 60, TourneyCountdown = 15, TourneyCeremony = 10;
	constexpr double NpcCatchMean = 215, NpcRunHome0 = 60, NpcRunHome1 = 140;
	const TCHAR* NpcCaptains[] = {TEXT("Capt. Ray Pinder"), TEXT("Capt. Lou Albury"), TEXT("Capt. Dee Sawyer")};

	// rng/index.ts mulberry32
	struct FMulberry
	{
		uint32 A;
		explicit FMulberry(uint32 S) : A(S) {}
		double operator()()
		{
			A += 0x6D2B79F5u;
			uint32 T = A;
			T = (T ^ (T >> 15)) * (T | 1u);
			T ^= T + (T ^ (T >> 7)) * (T | 61u);
			return double(T ^ (T >> 14)) / 4294967296.0;
		}
	};

	// every species a tournament can ask for: common somewhere (zone weight 2+), and legal to keep
	const TArray<FString>& TourneyPool()
	{
		static TArray<FString> Pool;
		if (Pool.Num()) return Pool;
		TMap<FString, double> Best;
		for (const TCHAR* Zn : {TEXT("Creek"), TEXT("Flats"), TEXT("Backcountry"), TEXT("Bridge"), TEXT("Hawk Channel"), TEXT("Reef"), TEXT("Offshore")})
			for (const FZoneRow& R : ZoneTable(Zn)) Best.FindOrAdd(R.Key) = FMath::Max(Best.FindOrAdd(R.Key), R.W);
		for (const TPair<FString, double>& B : Best)
			if (B.Value >= 2 && Species(B.Key) && ReleaseOnlyReason(B.Key).IsEmpty()) Pool.Add(B.Key);
		Pool.Sort();
		return Pool;
	}

	TArray<FString> DrawSpecies(uint32 Seed)
	{
		FMulberry R(Seed ^ 0x5eedu);
		TArray<FString> P = TourneyPool(), Out;
		while (Out.Num() < TourneyN && P.Num()) { const int32 I = FMath::Min(P.Num() - 1, int32(R() * P.Num())); Out.Add(P[I]); P.RemoveAt(I); }
		return Out;
	}

	int32 CountBits(int32 M) { int32 N = 0; for (; M; M &= M - 1) N++; return N; }
	FString Clock(double S) { const int32 I = FMath::Max(0, FMath::FloorToInt(S)); return FString::Printf(TEXT("%d:%02d"), I / 60, I % 60); }
}

AKeysEvents::AKeysEvents()
{
	PrimaryActorTick.bCanEverTick = true;
	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent = Root;
}

AKeysEvents* AKeysEvents::Get(const UObject* WorldContext)
{
	UWorld* W = WorldContext ? WorldContext->GetWorld() : nullptr;
	if (!W) return nullptr;
	TActorIterator<AKeysEvents> It(W);
	return It ? *It : nullptr;
}

AKeysBoatPawn* AKeysEvents::Pawn() const { return Cast<AKeysBoatPawn>(UGameplayStatics::GetPlayerPawn(this, 0)); }

void AKeysEvents::Say(const FString& Msg, double Secs) { if (AKeysBoatPawn* P = Pawn()) P->ShowMsg(Msg, Secs); }

void AKeysEvents::Sound(const TCHAR* Id)
{
	AKeysAudio* A = AKeysAudio::Get(this);
	const AKeysBoatPawn* P = Pawn();
	if (A && P) A->PlayExtra(FName(Id), FVector(P->BoatX(), 1, P->BoatZ()), 1);
}

double AKeysEvents::HarbourX() { return Marinas().Num() ? Marinas()[0].SX : 0; }
double AKeysEvents::HarbourZ() { return Marinas().Num() ? Marinas()[0].EZ : 0; }

void AKeysEvents::BeginPlay()
{
	Super::BeginPlay();
	BuildCourse();
	// the AI field (race/opponents.ts), built once and hidden until a race starts
	for (const FRaceEntry& F : Field)
	{
		FOpp O;
		O.Boat = F.Boat; O.Pace = F.Pace; O.Line = O.CurLine = F.Line;
		Opps.Add(O);
		UProceduralMeshComponent* C = NewObject<UProceduralMeshComponent>(this);
		C->SetupAttachment(Root);
		C->SetUsingAbsoluteLocation(true);
		C->SetUsingAbsoluteRotation(true);
		C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		C->RegisterComponent();
		const int32 BI = BoatIndexOf(F.Boat);
		FKeysMesh M;
		KeysMesh::BuildBoat(M, Boats()[FMath::Max(0, BI)]);
		M.Commit(C, 0, AKeysWorld::VertexColorMaterial());
		C->SetVisibility(false);
		OppMeshes.Add(C);
	}
	// the marks: orange inflatable race buoys with a numbered flag
	for (int32 i = 0; i < Course.Num(); i++)
	{
		UProceduralMeshComponent* C = NewObject<UProceduralMeshComponent>(this);
		C->SetupAttachment(Root);
		C->SetUsingAbsoluteLocation(true);
		C->SetUsingAbsoluteRotation(true);
		C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		C->RegisterComponent();
		FKeysMesh M;
		const FLinearColor Orange(1.f, .28f, .02f), White(.95f, .95f, .95f), Pole(.2f, .2f, .22f);
		const bool bStart = i == 0;
		M.AddTube(FVector(0, 0, -.4), FVector(0, 0, 1.6), 1.1, .9, 16, bStart ? FLinearColor(.95f, .8f, .1f) : Orange);
		M.AddTube(FVector(0, 0, 1.6), FVector(0, 0, 2.4), .9, .35, 16, bStart ? FLinearColor(.95f, .8f, .1f) : Orange);
		M.AddTube(FVector(0, 0, .5), FVector(0, 0, .85), 1.08, 1.04, 16, White);
		M.AddTube(FVector(0, 0, 2.3), FVector(0, 0, 5.2), .05, .04, 6, Pole);
		M.AddQuad(FVector(0, 0, 5.1), FVector(-1.4, 0, 5.1), FVector(-1.4, 0, 4.2), FVector(0, 0, 4.2), bStart ? FLinearColor(.1f, .1f, .1f) : White, FVector(0, 1, 0));
		M.AddQuad(FVector(0, 0, 5.1), FVector(0, 0, 4.2), FVector(-1.4, 0, 4.2), FVector(-1.4, 0, 5.1), bStart ? FLinearColor(.1f, .1f, .1f) : White, FVector(0, -1, 0));
		M.Commit(C, 0, AKeysWorld::VertexColorMaterial());
		C->SetVisibility(false);
		MarkMeshes.Add(C);
	}
	// the beacon over your next mark: a tall green pennant and a ring of floats round the buoy
	NextMesh = NewObject<UProceduralMeshComponent>(this);
	NextMesh->SetupAttachment(Root);
	NextMesh->SetUsingAbsoluteLocation(true);
	NextMesh->SetUsingAbsoluteRotation(true);
	NextMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	NextMesh->RegisterComponent();
	{
		FKeysMesh M;
		const FLinearColor G(.1f, 1.f, .35f);
		M.AddTube(FVector(0, 0, 2), FVector(0, 0, 11), .07, .05, 6, FLinearColor(.9f, .9f, .9f));
		M.AddQuad(FVector(0, 0, 11), FVector(-3.2, 0, 10.2), FVector(-3.2, 0, 9.6), FVector(0, 0, 8.6), G, FVector(0, 1, 0));
		M.AddQuad(FVector(0, 0, 11), FVector(0, 0, 8.6), FVector(-3.2, 0, 9.6), FVector(-3.2, 0, 10.2), G, FVector(0, -1, 0));
		for (int32 k = 0; k < 16; k++)
		{
			const double A = k * 2 * PI / 16;
			M.AddTube(FVector(FMath::Cos(A) * 9, FMath::Sin(A) * 9, -.1), FVector(FMath::Cos(A) * 9, FMath::Sin(A) * 9, .35), .35, .3, 8, k % 2 ? G : FLinearColor(.95f, .95f, .95f));
		}
		M.Commit(NextMesh, 0, AKeysWorld::VertexColorMaterial());
	}
	NextMesh->SetVisibility(false);
}

// ================================================================ race
void AKeysEvents::BuildCourse()
{
	// sim/race.ts MARATHON_COURSE: x along the chain, dz offshore of it. This port's islands are the ellipses rather than
	// the real outlines, so a mark that would sit on land or in skinny water is walked further offshore until it floats.
	Course.Reset();
	for (const FCourseBuoy& B : MarathonCourse)
	{
		double DZ = B.DZ;
		for (int32 k = 0; k < 60 && (DepthAt(B.X, ChainZ(B.X) + DZ) < 3.5 || ShoreInfo(B.X, ChainZ(B.X) + DZ).D < 60); k++) DZ += 15;
		Course.Add({B.Name, B.X, ChainZ(B.X) + DZ});
	}
	// the AI's line: a closed Catmull-Rom through the marks, in 4 m steps
	PathPts.Reset();
	const int32 N = Course.Num();
	for (int32 i = 0; i < N; i++)
	{
		const FVector2D P0(Course[(i + N - 1) % N].X, Course[(i + N - 1) % N].Z), P1(Course[i].X, Course[i].Z), P2(Course[(i + 1) % N].X, Course[(i + 1) % N].Z), P3(Course[(i + 2) % N].X, Course[(i + 2) % N].Z);
		const int32 Steps = FMath::Max(4, FMath::CeilToInt(FVector2D::Distance(P1, P2) / 4));
		for (int32 s = 0; s < Steps; s++)
		{
			const double T = double(s) / Steps, T2 = T * T, T3 = T2 * T;
			PathPts.Add(.5 * (2 * P1 + (P2 - P0) * T + (2 * P0 - 5 * P1 + 4 * P2 - P3) * T2 + (3 * P1 - P0 - 3 * P2 + P3) * T3));
		}
	}
	PathCum.Reset();
	PathCum.Add(0);
	for (int32 i = 1; i <= PathPts.Num(); i++) PathCum.Add(PathCum[i - 1] + FVector2D::Distance(PathPts[i - 1], PathPts[i % PathPts.Num()]));
	PathLen = FMath::Max(1.0, PathCum.Last());
	int32 Dry = 0;
	for (const FVector2D& P : PathPts) if (DepthAt(P.X, P.Y) < 1.6) Dry++;
	UE_LOG(LogTemp, Log, TEXT("Keys Run: race course %.1f km a lap, %d of %d path points in skinny water"), PathLen / 1000, Dry, PathPts.Num());
}

void AKeysEvents::PathAt(double S, double& OX, double& OZ, double& OTX, double& OTZ) const
{
	S = FMath::Fmod(FMath::Fmod(S, PathLen) + PathLen, PathLen);
	int32 Lo = 0, Hi = PathPts.Num() - 1;
	while (Lo < Hi) { const int32 Mid = (Lo + Hi + 1) / 2; if (PathCum[Mid] <= S) Lo = Mid; else Hi = Mid - 1; }
	const FVector2D A = PathPts[Lo], B = PathPts[(Lo + 1) % PathPts.Num()];
	const double Seg = FMath::Max(1e-3, PathCum[Lo + 1] - PathCum[Lo]), F = (S - PathCum[Lo]) / Seg;
	OX = FMath::Lerp(A.X, B.X, F); OZ = FMath::Lerp(A.Y, B.Y, F);
	const FVector2D Tn = (B - A).GetSafeNormal();
	OTX = Tn.X; OTZ = Tn.Y;
}

double AKeysEvents::CurvatureAt(double S) const
{
	const double Span = 55;
	double X, Z, AX, AZ, BX, BZ;
	PathAt(S, X, Z, AX, AZ);
	PathAt(S + Span, X, Z, BX, BZ);
	double D = FMath::Atan2(BZ, BX) - FMath::Atan2(AZ, AX);
	while (D > PI) D -= 2 * PI;
	while (D < -PI) D += 2 * PI;
	return FMath::Abs(D) / Span;
}

void AKeysEvents::SetRaceVisible(bool bVis)
{
	for (UProceduralMeshComponent* C : OppMeshes) if (C) C->SetVisibility(bVis);
	for (UProceduralMeshComponent* C : MarkMeshes) if (C) C->SetVisibility(bVis);
	if (NextMesh) NextMesh->SetVisibility(bVis);
}

void AKeysEvents::LineUp()
{
	for (int32 i = 0; i < Opps.Num(); i++)
	{
		Opps[i].S = PathLen - (i + 1) * 9;
		Opps[i].Speed = 0;
		Opps[i].CurLine = Opps[i].Line;
	}
}

void AKeysEvents::ToggleRace()
{
	if (Race == ERace::Countdown || Race == ERace::Racing)
	{
		Race = ERace::Idle;
		SetRaceVisible(false);
		Say(TEXT("Race abandoned."));
		return;
	}
	if (bTourney && TourneyPhase() != ETourney::Over) { Say(TEXT("Not in the middle of a tournament.")); return; }
	Racers.Reset();
	Racers.Add({TEXT("player"), TEXT("You")});
	for (int32 i = 0; i < Opps.Num(); i++) Racers.Add({FString::Printf(TEXT("ai%d"), i), Field[i].Name});
	Race = ERace::Countdown;
	Clock = RaceCountdown;
	Finished = 0;
	LineUp();
	SetRaceVisible(true);
	const AKeysBoatPawn* P = Pawn();
	const double D = P ? FMath::Sqrt(FMath::Square(P->BoatX() - Course[0].X) + FMath::Square(P->BoatZ() - Course[0].Z)) : 0;
	Say(FString::Printf(TEXT("Race on - %d laps of the Marathon course, %.1f km. Round the Start mark first: it's %.1f km away, the green pennant on the chart. R to abandon."),
		RaceLaps, CourseKm(), D / 1000), 6);
}

void AKeysEvents::Standings(TArray<const FRacer*>& Out) const
{
	Out.Reset();
	for (const FRacer& R : Racers) Out.Add(&R);
	const int32 NM = Course.Num();
	auto Prog = [NM](const FRacer* R) { return R->Lap * NM + R->Next; };
	Out.Sort([&Prog](const FRacer& A, const FRacer& B)
	{
		if (A.Place && B.Place) return A.Place < B.Place;
		if (A.Place || B.Place) return A.Place != 0;
		return Prog(&A) > Prog(&B);
	});
}

void AKeysEvents::OpponentDots(TArray<FVector2D>& Out) const
{
	Out.Reset();
	if (Race == ERace::Idle) return;
	for (int32 i = 1; i < Racers.Num(); i++) Out.Add(FVector2D(Racers[i].X, Racers[i].Z));
}

// sim/race.ts stepRace
void AKeysEvents::StepRace(double Dt)
{
	if (Race == ERace::Countdown)
	{
		const int32 Before = FMath::CeilToInt(Clock);
		Clock -= Dt;
		const int32 After = FMath::CeilToInt(Clock);
		if (After != Before && After > 0) Say(FString::Printf(TEXT("%d"), After), 1);
		if (Clock <= 0) { Race = ERace::Racing; Clock = 0; Say(TEXT("GO!"), 2); Sound(TEXT("AirHorn")); }
		return;
	}
	if (Race != ERace::Racing) return;
	Clock += Dt;
	const int32 NM = Course.Num();
	for (int32 i = 0; i < Racers.Num(); i++)
	{
		FRacer& R = Racers[i];
		if (R.FinishT >= 0) continue;
		if (FMath::Sqrt(FMath::Square(R.X - Course[R.Next].X) + FMath::Square(R.Z - Course[R.Next].Z)) > BuoyRadius) continue;
		const int32 Nx = (R.Next + 1) % NM;
		if (i == 0 && Nx != 0) Say(FString::Printf(TEXT("%s rounded - next: %s."), *Course[R.Next].Name, *Course[Nx].Name), 2.5);
		R.Next = Nx;
		if (Nx != 0) continue;
		R.Lap++;
		if (R.Lap < RaceLaps) { if (i == 0) Say(FString::Printf(TEXT("Lap %d of %d."), R.Lap, RaceLaps)); continue; }
		R.FinishT = Clock;
		R.Place = ++Finished;
		if (i == 0)
		{
			const FString Suffix = R.Place == 1 ? TEXT("Won it.") : R.Place == 2 ? TEXT("Second.") : FString::Printf(TEXT("P%d."), R.Place);
			Say(FString::Printf(TEXT("Finished in %d:%04.1f - %s R to race again."), FMath::FloorToInt(Clock / 60), FMath::Fmod(Clock, 60.0), *Suffix), 7);
			if (AKeysBoatPawn* P = Pawn())
				if (UKeysSave* Sv = P->Board(); Sv && (Sv->RaceBest <= 0 || Clock < Sv->RaceBest)) { Sv->RaceBest = Clock; Sv->Write(); }
			if (R.Place == 1) Sound(TEXT("CrowdRoar"));
		}
		else if (Racers[0].FinishT < 0 && R.Place == 1) Say(FString::Printf(TEXT("%s takes the flag."), *R.Name));
	}
	bool bAll = true;
	for (const FRacer& R : Racers) bAll &= R.FinishT >= 0;
	if (bAll) Race = ERace::Finished;
}

// race/opponents.ts update: full noise down the straights, backed off through the turns, each on its own line
void AKeysEvents::UpdateField(double Dt, double T, bool bReleased)
{
	for (int32 i = 0; i < Opps.Num(); i++)
	{
		FOpp& O = Opps[i];
		const double K = CurvatureAt(O.S), CornerK = 1 / (1 + K * 260);
		const bool bDone = Racers.IsValidIndex(i + 1) && Racers[i + 1].FinishT >= 0;
		const double Want = bReleased ? 30 * O.Pace * (.55 + .45 * CornerK) * (bDone ? .4 : 1) : 0;
		O.Speed = FMath::Lerp(O.Speed, Want, FMath::Min(1.0, Dt * (Want > O.Speed ? .5 : .9)));
		O.S = FMath::Fmod(O.S + O.Speed * Dt, PathLen);
		O.CurLine = FMath::Lerp(O.CurLine, O.Line * (.35 + .65 * CornerK), FMath::Min(1.0, Dt * .35));
		double PX, PZ, TX, TZ;
		PathAt(O.S, PX, PZ, TX, TZ);
		const double X = PX - TZ * O.CurLine, Z = PZ + TX * O.CurLine;
		O.H = FMath::Atan2(-TX, -TZ);
		if (Racers.IsValidIndex(i + 1)) { Racers[i + 1].X = X; Racers[i + 1].Z = Z; }
		const double Fx = -FMath::Sin(O.H), Fz = -FMath::Cos(O.H), Amp = AmpAt(X, Z) * .8;
		const double Hb = WaveH(X + Fx * 6, Z + Fz * 6, T, Amp), Hs = WaveH(X - Fx * 6, Z - Fz * 6, T, Amp);
		if (UProceduralMeshComponent* C = OppMeshes.IsValidIndex(i) ? OppMeshes[i].Get() : nullptr)
		{
			const double Pitch = FMath::Atan2(Hb - Hs, 12.8) + FMath::Min(.12, O.Speed * .004), Roll = FMath::Sin(T * 1.3 + i) * .03;
			const FVector Fw(Fx, Fz, 0), Rt(FMath::Cos(O.H), -FMath::Sin(O.H), 0), Up(0, 0, 1);
			C->SetWorldLocationAndRotation(ToUE(X, (Hb + Hs) / 2 + .2 + FMath::Min(.3, O.Speed * .01), Z),
				FRotationMatrix::MakeFromXY(Fw * FMath::Cos(Pitch) + Up * FMath::Sin(Pitch), Rt * FMath::Cos(Roll) + Up * FMath::Sin(Roll)).Rotator());
		}
		O.WakeT += Dt;
		if (O.WakeT > .2 && O.Speed > 2)
		{
			EmitWake(X - Fx * 6.4, Z - Fz * 6.4, O.Speed, 12.8, T, O.WakeT);
			EmitTrail(300 + i, X - Fx * 6.2, Z - Fz * 6.2, 1.4, FMath::Clamp((O.Speed - 2) / 8, 0.0, 1.0), T);
			O.WakeT = 0;
		}
	}
}

void AKeysEvents::UpdateMarks(double T)
{
	for (int32 i = 0; i < Course.Num() && i < MarkMeshes.Num(); i++)
	{
		const double X = Course[i].X, Z = Course[i].Z, W = WaveH(X, Z, T, AmpAt(X, Z));
		MarkMeshes[i]->SetWorldLocationAndRotation(ToUE(X, W - .3, Z), FRotator(FMath::Sin(T * 1.1 + i) * 4, T * 3 + i * 40, FMath::Cos(T * .9 + i) * 4));
	}
	const FRacer* M = Me();
	const bool bShow = Race == ERace::Racing || Race == ERace::Countdown;
	NextMesh->SetVisibility(bShow && M && M->FinishT < 0);
	if (bShow && M)
	{
		const FMark& N = Course[M->Next];
		NextMesh->SetWorldLocationAndRotation(ToUE(N.X, WaveH(N.X, N.Z, T, AmpAt(N.X, N.Z)), N.Z), FRotator(0, T * 20, 0));
	}
}

// ================================================================ tournament
AKeysEvents::FPlan AKeysEvents::Plan(int32 I) const
{
	// npcPlan: each AI boat's pace from the seed, so their day is fixed the moment the tournament is drawn
	FMulberry R(Seed + 0x9e3779b9u * uint32(I + 1));
	const double Skill = .8 + R() * .45;
	TArray<int32> Order;
	for (int32 k = 0; k < TSpecies.Num(); k++) Order.Add(k);
	for (int32 k = Order.Num() - 1; k > 0; k--) { const int32 J = FMath::Min(k, int32(R() * (k + 1))); Order.Swap(k, J); }
	FPlan P;
	P.CatchAt.Init(0, TSpecies.Num());
	double T = 0;
	for (const int32 k : Order)
	{
		const double Step = (-FMath::Loge(1 - R()) - FMath::Loge(1 - R())) * .5 * NpcCatchMean / Skill;
		T += FMath::Max(25.0, Step);
		P.CatchAt[k] = T;
	}
	for (int32 k = 0; k < TSpecies.Num(); k++) R();   // the weights, which this port doesn't show
	P.HomeAt = T + NpcRunHome0 + R() * (NpcRunHome1 - NpcRunHome0);
	return P;
}

int32 AKeysEvents::NpcMask(int32 I, double El) const
{
	const FPlan P = Plan(I);
	int32 M = 0;
	for (int32 k = 0; k < P.CatchAt.Num(); k++) if (P.CatchAt[k] <= El) M |= 1 << k;
	return M;
}

AKeysEvents::ETourney AKeysEvents::TourneyPhase() const
{
	if (!bTourney) return ETourney::Off;
	const double Now = GetWorld()->GetTimeSeconds();
	if (Now < T0) return ETourney::Countdown;
	if (!WinnerId.IsEmpty()) return Now < T0 + WinAt + TourneyCeremony ? ETourney::Ceremony : ETourney::Over;
	return Now < T0 + TourneyDuration ? ETourney::Live : ETourney::Over;
}

double AKeysEvents::TimeLeft() const { return FMath::Clamp(T0 + TourneyDuration - GetWorld()->GetTimeSeconds(), 0.0, TourneyDuration); }
double AKeysEvents::CountdownLeft() const { return FMath::Max(0.0, T0 - GetWorld()->GetTimeSeconds()); }

FString AKeysEvents::WinnerName() const
{
	if (WinnerId == TEXT("me")) return TEXT("You");
	if (WinnerId.StartsWith(TEXT("npc"))) { const int32 I = FCString::Atoi(*WinnerId.Mid(3)); return Field[FMath::Clamp(I, 0, 2)].Name; }
	return FString();
}

void AKeysEvents::ToggleTourney()
{
	const ETourney Ph = TourneyPhase();
	if (bTourney && Ph != ETourney::Over)
	{
		bTourney = false;
		Say(TEXT("Tournament abandoned."));
		return;
	}
	if (Race == ERace::Countdown || Race == ERace::Racing) { Say(TEXT("Finish the race first.")); return; }
	Seed = uint32(FMath::Rand()) * 2654435761u ^ uint32(FDateTime::Now().GetTicks());
	TSpecies = DrawSpecies(Seed);
	T0 = GetWorld()->GetTimeSeconds() + TourneyCountdown;
	Mask = 0;
	MyW.Init(0, TSpecies.Num());
	WinnerId.Reset();
	WinAt = 0;
	bWinnerMe = false;
	bAnnouncedAll = false;
	for (int32& N : NpcSeen) N = 0;
	bTourney = true;
	LastPhase = ETourney::Off;
}

void AKeysEvents::OnKeep(const FString& Key, double Weight)
{
	if (TourneyPhase() != ETourney::Live) return;
	const int32 K = TSpecies.IndexOfByKey(Key);
	if (K < 0 || (Mask & (1 << K))) return;
	Mask |= 1 << K;
	MyW[K] = Weight;
	const FSpecies* S = Species(Key);
	const FString Name = S ? S->Name : Key;
	if (Mask == (1 << TSpecies.Num()) - 1)
	{
		bAnnouncedAll = true;
		Say(FString::Printf(TEXT("%s - that's all five! Run them in to Boot Key Harbor."), *Name), 5);
	}
	else Say(FString::Printf(TEXT("Tournament: %s checked off - %d/5."), *Name, CountBits(Mask)), 4);
}

void AKeysEvents::TourneyStandings(TArray<FEntry>& Out) const
{
	Out.Reset();
	const double El = FMath::Min(GetWorld()->GetTimeSeconds() - T0, !WinnerId.IsEmpty() ? WinAt : TourneyDuration);
	Out.Add({TEXT("You"), CountBits(Mask), false, WinnerId == TEXT("me"), true});
	for (int32 i = 0; i < 3; i++)
		Out.Add({Field[i].Name, CountBits(NpcMask(i, El)), Plan(i).HomeAt <= El, WinnerId == FString::Printf(TEXT("npc%d"), i), false});
	Out.Sort([](const FEntry& A, const FEntry& B) { return A.bWon != B.bWon ? A.bWon : A.Caught > B.Caught; });
}

// stepTourney / tryDeliver, and the toasts from game/tournament/index.ts
void AKeysEvents::StepTourney()
{
	const ETourney Ph = TourneyPhase();
	const double Now = GetWorld()->GetTimeSeconds();
	if (Ph != LastPhase)
	{
		if (Ph == ETourney::Countdown)
		{
			FString L;
			for (const FString& K : TSpecies) { const FSpecies* S = Species(K); L += (L.IsEmpty() ? TEXT("") : TEXT(", ")) + (S ? S->Name : K); }
			Say(FString::Printf(TEXT("Tournament! Today's five: %s. Lines in at the horn."), *L), 8);
		}
		if (Ph == ETourney::Live && LastPhase == ETourney::Countdown)
		{
			Say(TEXT("Lines in! Keep one of each, then back to Boot Key Harbor. First boat in wins."), 6);
			Sound(TEXT("AirHorn"));
		}
		if (Ph == ETourney::Ceremony)
		{
			if (bWinnerMe)
			{
				int32 Wins = 1;
				if (AKeysBoatPawn* P = Pawn()) if (UKeysSave* Sv = P->Board()) { Sv->TourneyWins++; Wins = Sv->TourneyWins; Sv->Write(); }
				Say(FString::Printf(TEXT("You won the tournament! The trophy goes in your case - %d so far."), Wins), 8);
				Sound(TEXT("CrowdRoar"));
			}
			else Say(FString::Printf(TEXT("%s ran all five in to Boot Key Harbor first - they take the trophy."), *WinnerName()), 7);
		}
		if (Ph == ETourney::Over && LastPhase == ETourney::Live && WinnerId.IsEmpty())
			Say(TEXT("Horn! Nobody brought all five in - no trophy this time."), 6);
		LastPhase = Ph;
	}
	if (Ph != ETourney::Live) return;
	const double El = Now - T0;
	// what the AI boats have boated since last time
	for (int32 i = 0; i < 3; i++)
	{
		const int32 M = NpcMask(i, El);
		const int32 New = M & ~NpcSeen[i];
		if (New)
		{
			for (int32 k = 0; k < TSpecies.Num(); k++)
				if (New & (1 << k)) { const FSpecies* S = Species(TSpecies[k]); Say(FString::Printf(TEXT("%s boated a %s - %d/5."), Field[i].Name, S ? *S->Name : *TSpecies[k], CountBits(M)), 3.5); }
			NpcSeen[i] = M;
		}
	}
	// stepTourney: the first AI boat home wins, unless you get there first
	double BestAt = 1e18;
	int32 Best = -1;
	for (int32 i = 0; i < 3; i++) { const double H = Plan(i).HomeAt; if (H <= El && H < BestAt) { BestAt = H; Best = i; } }
	if (Best >= 0) { WinnerId = FString::Printf(TEXT("npc%d"), Best); WinAt = BestAt; return; }
	// tryDeliver: all five, inside the harbour and slow enough to tie up
	const AKeysBoatPawn* P = Pawn();
	if (P && Mask == (1 << TSpecies.Num()) - 1)
	{
		const double D = FMath::Sqrt(FMath::Square(P->BoatX() - HarbourX()) + FMath::Square(P->BoatZ() - HarbourZ()));
		if (D <= HarbourR && FMath::Abs(P->GetSpeed()) <= 2.5) { WinnerId = TEXT("me"); WinAt = El; bWinnerMe = true; }
	}
}

FString AKeysEvents::DebugCmd(const FString& Cmd)
{
	// ev:tourneyall (tick off all five), ev:tourneylive (skip the countdown), ev:raceat=N (put you at mark N)
	if (Cmd == TEXT("tourneyall")) { Mask = (1 << TSpecies.Num()) - 1; bAnnouncedAll = true; }
	else if (Cmd == TEXT("tourneylive")) T0 = GetWorld()->GetTimeSeconds();
	else if (Cmd == TEXT("tourneyfew")) { if (TSpecies.Num() >= 3) Mask |= 6; }
	return FString();
}

void AKeysEvents::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	const double Dt = FMath::Min(double(DeltaSeconds), 1.0 / 20), T = GetWorld()->GetTimeSeconds();
	if (Race != ERace::Idle)
	{
		if (const AKeysBoatPawn* P = Pawn(); P && Racers.Num()) { Racers[0].X = P->BoatX(); Racers[0].Z = P->BoatZ(); }
		UpdateField(Dt, T, Race == ERace::Racing || Race == ERace::Finished);
		StepRace(Dt);
		UpdateMarks(T);
	}
	if (bTourney) StepTourney();
}
