#include "KeysExtras.h"
#include "KeysAudio.h"
#include "KeysSpeech.h"
#include "KeysDiver.h"
#include "EngineUtils.h"
#include "KeysBoatPawn.h"
#include "KeysData.h"
#include "KeysGeo.h"
#include "KeysMesh.h"
#include "KeysWorld.h"
#include "ProceduralMeshComponent.h"
#include "Camera/CameraComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/World.h"
#include "InputCoreTypes.h"

namespace
{
	constexpr double ESpeedScale = 1.35;
	double ERand(double A, double B) { return FMath::FRandRange(A, B); }
	double EWrapPi(double A) { return FMath::Fmod(FMath::Fmod(A + PI, 2 * PI) + 2 * PI, 2 * PI) - PI; }
	FVector EJ(double X, double Y, double Z) { return FKeysHuman::J(X, Y, Z); }
	double EHash(double A, double B) { const double H = FMath::Sin(A * 127.1 + B * 311.7) * 43758.5453; return H - FMath::FloorToDouble(H); }

	// a weighted pick from the zone table (JS pickSpecies without the hotspot and hump bonuses)
	FString EPickSpecies(const FString& Zone)
	{
		const TArray<KeysData::FZoneRow>& Tbl = KeysData::ZoneTable(Zone);
		if (!Tbl.Num()) return FString();
		double Tot = 0;
		for (const KeysData::FZoneRow& R : Tbl) Tot += R.W;
		double R = FMath::FRand() * Tot;
		for (const KeysData::FZoneRow& Row : Tbl) { R -= Row.W; if (R <= 0) return Row.Key; }
		return Tbl[0].Key;
	}

	UProceduralMeshComponent* EMakeMesh(AActor* Owner, USceneComponent* Parent, const TCHAR* Name, bool bAbsolute)
	{
		UProceduralMeshComponent* M = Owner->CreateDefaultSubobject<UProceduralMeshComponent>(Name);
		M->SetupAttachment(Parent);
		M->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		if (bAbsolute)
		{
			M->SetUsingAbsoluteLocation(true);
			M->SetUsingAbsoluteRotation(true);
			M->SetUsingAbsoluteScale(true);
		}
		return M;
	}
}

AKeysExtras::AKeysExtras()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickGroup = TG_PostPhysics;   // after the boat has moved and set its camera

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent = Root;

	CrewMesh = EMakeMesh(this, Root, TEXT("Crew"), false);
	BuddyMesh = EMakeMesh(this, Root, TEXT("Buddy"), true);
	BuddyMesh->SetCastShadow(false);
	BuddyFish = EMakeMesh(this, Root, TEXT("BuddyFish"), true);
	GullMesh = EMakeMesh(this, Root, TEXT("Caelen"), true);
}

AKeysExtras* AKeysExtras::Get(const UObject* WorldContext)
{
	UWorld* W = WorldContext ? WorldContext->GetWorld() : nullptr;
	if (!W) return nullptr;
	TActorIterator<AKeysExtras> It(W);
	return It ? *It : nullptr;
}

void AKeysExtras::BeginPlay()
{
	Super::BeginPlay();
	KeysGeo::Init();
	BuddyMesh->SetVisibility(false);
	BuddyFish->SetVisibility(false);
	GullMesh->SetVisibility(false);
}

AKeysBoatPawn* AKeysExtras::FindPawn()
{
	AKeysBoatPawn* P = Cast<AKeysBoatPawn>(UGameplayStatics::GetPlayerPawn(this, 0));
	if (P != PawnRef.Get())
	{
		PawnRef = P;
		bAttached = false;
		LayoutFor.Reset();
	}
	return P;
}

double AKeysExtras::Now() const { return GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0; }
double AKeysExtras::PawnSpeed() const { const AKeysBoatPawn* P = PawnRef.Get(); return P ? P->GetSpeed() : 0.0; }
double AKeysExtras::PawnY() const { const AKeysBoatPawn* P = PawnRef.Get(); return P ? P->GetY() : 0.0; }

void AKeysExtras::Toast(const FString& Msg, double Secs)
{
	if (AKeysBoatPawn* P = PawnRef.Get())
	{
		P->Toast = Msg;
		P->ToastUntil = Now() + Secs;
	}
}

void AKeysExtras::Sound(FName Id, const FVector& Pos, float Power)
{
	OnSound.Broadcast(Id, Pos, Power);
}

void AKeysExtras::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	const double Dt = FMath::Min(double(DeltaSeconds), .1), T = Now();
	AKeysBoatPawn* P = FindPawn();
	if (!P) return;
	if (!bAttached && P->GetRootComponent())
	{
		const FAttachmentTransformRules Rules(EAttachmentRule::SnapToTarget, EAttachmentRule::SnapToTarget, EAttachmentRule::KeepWorld, false);
		CrewMesh->AttachToComponent(P->GetRootComponent(), Rules);
		AddTickPrerequisiteActor(P);
		bAttached = true;
	}
	if (LayoutFor != P->Spec().Id) BuildLayout();
	if (APlayerCameraManager* CM = UGameplayStatics::GetPlayerCameraManager(this, 0))
	{
		const FVector C = CM->GetCameraLocation();
		CamX = C.X / 100;
		CamZ = C.Y / 100;
	}
	if (bHandleKeys && P->bRunning && !P->IsCapturingKeys() && !P->IsDiving())
		if (APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0))
		{
			if (PC->WasInputKeyJustPressed(EKeys::Y)) ToggleBuddy();
			if (PC->WasInputKeyJustPressed(EKeys::Z)) ToggleShower();   // J is diving now, as in the web game
		}

	UpdateCrew(Dt, T);
	CommitCrew(T);
	UpdateBuddy(Dt, T);
	UpdateCaelen(Dt, T);
}

// ================================================================ keys
void AKeysExtras::ToggleShower()
{
	if (!bHasShower) { Toast(TEXT("Only the Freeman has a shower.")); return; }
	const bool bCur = ShowerForced >= 0 ? ShowerForced == 1 : bShowerOn;
	ShowerForced = bCur ? 0 : 1;
	Toast(ShowerForced ? TEXT("Shower on.") : TEXT("Shower off."));
	if (FMath::Abs(PawnSpeed()) >= 1.5 && ShowerForced == 1) Toast(TEXT("Shower's on - it runs when the boat is stopped."));
}

bool AKeysExtras::OpenWater(double X, double Z, double Draft, double& OutX, double& OutZ)
{
	using namespace KeysGeo;
	auto Ok = [Draft](double A, double B)
	{
		return DepthAt(A, B) > FMath::Max(Draft + .9, 2.6) && LandH(A, B) < -.6 && A > WBX0 + 50 && A < WBX1 - 50 && B > WBZ0 + 50 && B < WBZ1 - 50;
	};
	if (Ok(X, Z)) { OutX = X; OutZ = Z; return true; }
	for (double R = 30; R <= 900; R += 30)
		for (int32 k = 0; k < 16; k++)
		{
			const double A = k / 16.0 * PI * 2, XX = X + FMath::Cos(A) * R, ZZ = Z + FMath::Sin(A) * R;
			if (Ok(XX, ZZ)) { OutX = XX; OutZ = ZZ; return true; }
		}
	return false;
}

bool AKeysExtras::MiamiDestination(double BoatX, double BoatZ, double Draft, double& OutX, double& OutZ, double& OutH, FString& OutMsg)
{
	using namespace KeysGeo;
	if (RealMap()) { OutMsg = TEXT("Miami is 100 miles up the coast - off this chart. The real Keys map covers the Middle Keys so far."); return false; }
	const bool bToMiami = !InMiami(BoatX, BoatZ);
	const double D = Draft > 0 ? Draft : 1;
	bool bOk = bToMiami
		? (OpenWater(2700, -2350, D, OutX, OutZ) || OpenWater(2400, -1700, D, OutX, OutZ))
		: (OpenWater(-1050, ChainZ(-1050) + 300, D, OutX, OutZ) || OpenWater(-1200, ChainZ(-1200) + 500, D, OutX, OutZ));
	if (!bOk) { OutMsg = TEXT("Couldn't find open water there."); return false; }
	OutH = bToMiami ? PI * .5 : 0;
	OutMsg = bToMiami
		? TEXT("Biscayne Bay - downtown Miami and Brickell off your bow, Miami Beach to the east. X runs you back to the Keys.")
		: TEXT("Back in Boot Key Harbor, Marathon. X runs you up to Miami.");
	return true;
}

// ================================================================ HUD
void AKeysExtras::GetTags(TArray<FKeysExtrasTag>& Out) const
{
	const AKeysBoatPawn* P = PawnRef.Get();
	if (!P || !P->bRunning) return;
	if (bBuddyOn && BuddyIdx >= 0)
	{
		const double Dc = FMath::Sqrt(FMath::Square(BX - CamX) + FMath::Square(BZ - CamZ));
		if (Dc < 900 && Dc > 25)
		{
			FKeysExtrasTag& Tg = Out.AddDefaulted_GetRef();
			Tg.WorldPos = KeysGeo::ToUE(BX, BY + 6, BZ);
			Tg.Text = FString(TEXT("Buddy")) + (BFight > 0 ? TEXT(" - fish on!") : BStillT > 4 ? TEXT(" - fishing") : TEXT(""));
			Tg.MaxDist = 900; Tg.MinDist = 25;
		}
	}
	if (bCaelenInit && GullMesh->IsVisible())
	{
		const double Dc = FMath::Sqrt(FMath::Square(CPos.X - CamX) + FMath::Square(CPos.Z - CamZ));
		if (Dc < 160)
		{
			FKeysExtrasTag& Tg = Out.AddDefaulted_GetRef();
			Tg.WorldPos = KeysGeo::ToUE(CPos.X, CPos.Y + 1.2, CPos.Z);
			Tg.Text = CBubble > 0 ? FString::Printf(TEXT("Caelen: \"%s\""), *CLine) : FString(TEXT("Caelen"));
			Tg.MaxDist = 160;
		}
	}
}

// ================================================================ buddy boat
void AKeysExtras::BuddyBuild()
{
	AKeysBoatPawn* P = PawnRef.Get();
	if (!P) return;
	static const TCHAR* Order[5] = {TEXT("freeman"), TEXT("grady"), TEXT("midnight"), TEXT("mti"), TEXT("robalo")};
	const TArray<KeysData::FBoatSpec>& Boats = KeysData::Boats();
	int32 Pick = -1;
	for (const TCHAR* Id : Order)
	{
		if (P->Spec().Id == Id) continue;
		for (int32 i = 0; i < Boats.Num(); i++) if (Boats[i].Id == Id) { Pick = i; break; }
		if (Pick >= 0) break;
	}
	if (Pick < 0 || Pick == BuddyIdx) return;
	BuddyIdx = Pick;
	FKeysMesh M;
	KeysMesh::BuildBoat(M, Boats[Pick]);
	M.Commit(BuddyMesh, 0, AKeysWorld::VertexColorMaterial());
	BuddyMesh->ClearMeshSection(1);
}

void AKeysExtras::BuddyPlaceNear()
{
	AKeysBoatPawn* P = PawnRef.Get();
	if (!P || BuddyIdx < 0) return;
	const KeysData::FBoatSpec& S = KeysData::Boats()[BuddyIdx];
	const double H = P->Heading(), Fx = -FMath::Sin(H), Fz = -FMath::Cos(H);
	double NX, NZ;
	if (OpenWater(P->BoatX() - Fx * 450 + FMath::Cos(H) * 180, P->BoatZ() - Fz * 450 - FMath::Sin(H) * 180, S.Draft, NX, NZ)
		|| OpenWater(P->BoatX(), P->BoatZ(), S.Draft, NX, NZ))
	{
		BX = NX;
		BZ = NZ;
	}
	BH = H;
	BSpeed = 0;
	bBSpot = false;
}

void AKeysExtras::ToggleBuddy()
{
	bBuddyOn = !bBuddyOn;
	if (bBuddyOn)
	{
		BuddyBuild();
		BuddyPlaceNear();
		BuddyMesh->SetVisibility(true, true);
		Toast(TEXT("Buddy is out - he'll keep his distance. Y sends him home."));
	}
	else
	{
		BuddyMesh->SetVisibility(false, true);
		BuddyFish->SetVisibility(false);
		bBJump = false;
		Toast(TEXT("Your buddy headed back to the dock. Y calls him back out."));
	}
}

void AKeysExtras::UpdateBuddy(double Dt, double T)
{
	using namespace KeysGeo;
	AKeysBoatPawn* P = PawnRef.Get();
	if (!bBuddyOn || !P || !P->bRunning) return;
	if (BuddyIdx < 0 || KeysData::Boats()[BuddyIdx].Id == P->Spec().Id) { BuddyBuild(); BuddyPlaceNear(); }
	if (BuddyIdx < 0) return;
	const KeysData::FBoatSpec& S = KeysData::Boats()[BuddyIdx];
	const double PX = P->BoatX(), PZ = P->BoatZ(), PH = P->Heading();
	const double TopMs = S.Top * .5144 * ESpeedScale, Ps = FMath::Abs(PawnSpeed());
	const double Pfx = -FMath::Sin(PH), Pfz = -FMath::Cos(PH), Prx = FMath::Cos(PH), Prz = -FMath::Sin(PH);
	double Dist = FMath::Sqrt(FMath::Square(BX - PX) + FMath::Square(BZ - PZ));
	if (Dist > 2200) { BuddyPlaceNear(); Dist = FMath::Sqrt(FMath::Square(BX - PX) + FMath::Square(BZ - PZ)); }

	// where to be: trailing off your quarter while you run; anchored up nearby (not too near) when you stop
	double TX, TZ;
	bool bStop = false;
	if (Ps > 2.2)
	{
		bBSpot = false;
		TX = PX - Pfx * 380 + Prx * 160;
		TZ = PZ - Pfz * 380 + Prz * 160;
		BState = 0;
	}
	else
	{
		if (!bBSpot || FMath::Sqrt(FMath::Square(BSpotX - PX) + FMath::Square(BSpotZ - PZ)) > 600)
		{
			const double Side = FMath::FRand() < .5 ? 1 : -1;
			if (!OpenWater(PX + Prx * Side * 330 - Pfx * 80, PZ + Prz * Side * 330 - Pfz * 80, S.Draft, BSpotX, BSpotZ)) { BSpotX = BX; BSpotZ = BZ; }
			bBSpot = true;
		}
		TX = BSpotX;
		TZ = BSpotZ;
		bStop = FMath::Sqrt(FMath::Square(TX - BX) + FMath::Square(TZ - BZ)) < 18;
		BState = bStop ? 2 : 1;
	}
	const double DX = TX - BX, DZ = TZ - BZ, E = FMath::Sqrt(DX * DX + DZ * DZ);
	double Want = FMath::Atan2(-DX, -DZ);
	double V = bStop ? 0 : FMath::Clamp((E - 15) * .12 + (Ps > 2.2 ? Ps * .95 : 0), 0.0, TopMs * .9);
	if (Dist < 220) { Want = FMath::Atan2(-(BX - PX), -(BZ - PZ)) + PI; V = FMath::Min(V, 6.0); }   // give you room
	{
		const double Bfx = -FMath::Sin(BH), Bfz = -FMath::Cos(BH), Look = 20 + BSpeed * 1.5;
		if (DepthAt(BX + Bfx * Look, BZ + Bfz * Look) < S.Draft + .6 || LandH(BX + Bfx * Look, BZ + Bfz * Look) > -.6) { Want = BH + 1.4; V = FMath::Min(V, 5.0); }
	}
	BH += FMath::Clamp(EWrapPi(Want - BH), -1.0, 1.0) * FMath::Min(1.0, Dt * (.6 + FMath::Min(1.0, BSpeed / 10)));
	BSpeed += FMath::Clamp(V - BSpeed, -4 * Dt, 3 * Dt);
	BX += -FMath::Sin(BH) * BSpeed * Dt;
	BZ += -FMath::Cos(BH) * BSpeed * Dt;

	// ride the water
	const double X = BX, Z = BZ, Fx = -FMath::Sin(BH), Fz = -FMath::Cos(BH), Rx = FMath::Cos(BH), Rz = -FMath::Sin(BH);
	const double Amp = AmpAt(X, Z), HL = S.Len * .45, HB2 = S.Beam * .5;
	const double Hb = WaveH(X + Fx * HL, Z + Fz * HL, T, Amp), Hs = WaveH(X - Fx * HL, Z - Fz * HL, T, Amp);
	const double Hp = WaveH(X - Rx * HB2, Z - Rz * HB2, T, Amp), Hst = WaveH(X + Rx * HB2, Z + Rz * HB2, T, Amp);
	const double Fr = FMath::Min(1.0, BSpeed / TopMs);
	BY = FMath::Lerp(BY, (Hb + Hs + Hp + Hst) / 4 + Fr * .3, FMath::Min(1.0, Dt * 4));
	BPitch = FMath::Lerp(BPitch, FMath::Atan2(Hb - Hs, S.Len) + Fr * .07, FMath::Min(1.0, Dt * 4));
	BRoll = FMath::Lerp(BRoll, FMath::Atan2(Hp - Hst, S.Beam) * .7, FMath::Min(1.0, Dt * 3));
	{
		// same pose construction as the player's hull: bow up for positive pitch; BRoll is port-up positive
		const FVector Fw(Fx, Fz, 0), Rt(Rx, Rz, 0), Up(0, 0, 1);
		const FVector Fp = Fw * FMath::Cos(BPitch) + Up * FMath::Sin(BPitch);
		const FVector Rr = Rt * FMath::Cos(-BRoll) + Up * FMath::Sin(-BRoll);
		BuddyMesh->SetWorldLocationAndRotation(ToUE(X, BY, Z), FRotationMatrix::MakeFromXY(Fp, Rr).Rotator());
	}
	BWakeT += Dt;
	if (BWakeT > .1 && BSpeed > 2.5)
	{
		EmitWake(X - Fx * S.Len * .5, Z - Fz * S.Len * .5, BSpeed, S.Len, T, BWakeT);
		EmitTrail(1, X - Fx * S.Len * .48, Z - Fz * S.Len * .48, S.Beam * .5, FMath::Clamp((BSpeed - 1.5) / 7, 0.0, 1.0), T);
		BWakeT = 0;
	}

	// fishing while parked: rod out, the odd hookup with a jump
	const bool bFishing = BState == 2;
	BStillT = bFishing ? BStillT + Dt : 0;
	const FVector RodBase(-S.Len * .3, KeysMesh::InnerHalfBeam(S, -S.Len * .3) + .1, KeysMesh::SheerAt(S, -S.Len * .3) + .05);   // starboard aft gunwale, hull-local meters
	if (BStillT > 4)
	{
		const double A = .9 + (BFight > 0 ? .3 + FMath::Sin(T * 14) * .08 : 0);
		const FVector Dir(0, FMath::Sin(A), FMath::Cos(A));
		FKeysMesh R;
		R.AddTube(RodBase, RodBase + Dir * 3, .04, .018, 6, FLinearColor(.02f, .02f, .025f));
		KeysShapes::AddBlob(R, RodBase + Dir * .45, FVector(.09, .06, .09), FQuat::Identity, FLinearColor(.6f, .45f, .05f), 8, 4);
		R.Commit(BuddyMesh, 1, AKeysWorld::VertexColorMaterial());

		BHookT -= Dt;
		if (BHookT <= 0 && BFight <= 0)
		{
			BFight = ERand(14, 30);
			BHookT = ERand(45, 110);
			const double Out = RodBase.Y + 18;
			BFX = X + Fx * RodBase.X + Rx * Out;
			BFZ = Z + Fz * RodBase.X + Rz * Out;
			Sound(TEXT("Splash"), FVector(BFX, 0, BFZ), 1.1f * FMath::Min(1.6f, 18.f / 12.f));
			BKey = EPickSpecies(ZoneAt(BFX, BFZ));
		}
		if (BFight > 0)
		{
			BFight -= Dt;
			if (FMath::FRand() < Dt * 1.2) Sound(TEXT("Splash"), FVector(BFX + ERand(-3, 3), 0, BFZ + ERand(-3, 3)), .6f * .5f);
			const KeysData::FSpecies* Sp = KeysData::Species(BKey);
			if (!bBJump && Sp && Sp->bJump && FMath::FRand() < Dt * .25)
			{
				FKeysMesh F;
				const double W = Sp->Min + (Sp->Max - Sp->Min) * .35;
				KeysMesh::BuildFish(F, KeysMesh::FishLength(W), Sp->Color, KeysData::IsBillfish(BKey));
				F.Commit(BuddyFish, 0, AKeysWorld::VertexColorMaterial());
				BuddyFish->SetVisibility(true);
				bBJump = true;
				BJT = 0; BJX = BFX; BJZ = BFZ; BJA = FMath::FRand() * 2 * PI;
				Sound(TEXT("Splash"), FVector(BFX, 0, BFZ), 1.2f * FMath::Min(1.6f, 16.f / 12.f));
			}
		}
	}
	else BuddyMesh->ClearMeshSection(1);

	if (bBJump)
	{
		BJT += Dt;
		const double Pj = BJT / 1.2;
		if (Pj >= 1)
		{
			Sound(TEXT("Splash"), FVector(BJX + FMath::Cos(BJA) * 3, 0, BJZ + FMath::Sin(BJA) * 3), 1.1f * FMath::Min(1.6f, 16.f / 12.f));
			BuddyFish->SetVisibility(false);
			bBJump = false;
		}
		else
		{
			const FVector Pos = ToUE(BJX + FMath::Cos(BJA) * 3 * Pj, -.4 + 4 * 2 * Pj * (1 - Pj), BJZ + FMath::Sin(BJA) * 3 * Pj);
			BuddyFish->SetWorldLocationAndRotation(Pos, FRotator(FMath::RadiansToDegrees((1 - 2 * Pj) * 1.1), FMath::RadiansToDegrees(BJA), FMath::RadiansToDegrees(FMath::Sin(Pj * 20) * .4)));
		}
	}
}

// ================================================================ Caelen the seagull
void AKeysExtras::BuildGull(FKeysMesh& M, double Up, double Hand) const
{
	const double S = 1.45;
	const FLinearColor White(.93f, .93f, .92f), Gray(.55f, .58f, .63f), Tip(.08f, .08f, .1f), Beak(.93f, .74f, .18f);
	KeysShapes::AddBlob(M, FVector::ZeroVector, FVector(.2, .065, .07) * S, FQuat::Identity, White, 8, 5);
	KeysShapes::AddBlob(M, FVector(.17, 0, .05) * S, FVector(.055) * S, FQuat::Identity, White, 7, 4);
	M.AddTube(FVector(.215, 0, .045) * S, FVector(.28, 0, .035) * S, .013 * S, .004 * S, 5, Beak);
	for (int32 Sx = -1; Sx <= 1; Sx += 2)
		KeysShapes::AddBlob(M, FVector(.2, Sx * .035, .065) * S, FVector(.008) * S, FQuat::Identity, Tip, 4, 2);
	M.AddQuad(FVector(-.15, -.05, 0) * S, FVector(-.15, .05, 0) * S, FVector(-.3, .06, .01) * S, FVector(-.3, -.06, .01) * S, White, FVector::ZAxisVector);
	// wings: an inner panel from the shoulder to the elbow, and a swept outer panel with black tips (JS poseBird)
	const double EI = .34, OL = .38;
	const double A2 = Up + Hand - Up * .25;
	for (int32 Sx = -1; Sx <= 1; Sx += 2)
	{
		const FVector S0 = FVector(.03, Sx * .06, .03) * S;
		const FVector DirIn(0, Sx * FMath::Cos(Up), FMath::Sin(Up)), DirOut(0, Sx * FMath::Cos(A2), FMath::Sin(A2));
		const FVector E = S0 + DirIn * EI * S;
		M.AddQuad(S0 + FVector(.07, 0, 0) * S, E + FVector(.05, 0, 0) * S, E + FVector(-.08, 0, 0) * S, S0 + FVector(-.09, 0, 0) * S, Gray, FVector::ZAxisVector);
		const FVector Mid = E + DirOut * OL * .7 * S, End = E + DirOut * OL * S;
		M.AddQuad(E + FVector(.05, 0, 0) * S, Mid + FVector(-.055, 0, 0) * S, Mid + FVector(-.129, 0, 0) * S, E + FVector(-.08, 0, 0) * S, Gray, FVector::ZAxisVector);
		M.AddQuad(Mid + FVector(-.055, 0, 0) * S, End + FVector(-.1, 0, 0) * S, End + FVector(-.15, 0, 0) * S, Mid + FVector(-.129, 0, 0) * S, Tip, FVector::ZAxisVector);
	}
}

namespace
{
	// caelen.ts: what he says unprompted, in rotation, and when shot at
	const TCHAR* CaelenLines[] = {TEXT("I'm an executive"), TEXT("you didn't make this... you didn't make this"), TEXT("I'm an executive, braow"),
		TEXT("I don't even fish, I own the fish"), TEXT("I'm at a conference in Toronto right now... really important stuff..."), TEXT("can I bring Dan on your boat?"), TEXT("spare the portabello")};
	const TCHAR* CaelenDownCheers[] = {TEXT("The whole boat erupts - \"FINAAALLY! He's reallly annoying.\""), TEXT("Hands up all round the deck - \"Finaaally. That bird was reallly annoying.\""),
		TEXT("Cheering on deck - \"FINAAALLY! Nobody liked him. Nobody.\""), TEXT("The deck goes up - \"Finaaally! He's been reallly annoying since Islamorada.\"")};
	constexpr double CaelenAwayS = 120, CaelenLeaveS = 2.4, CaelenExitDelay = 1.9, CaelenHalfLen = .35;
}

void AKeysExtras::CaelenSay(const FString& Line, double Hold)
{
	CLine = Line;
	CBubble = Hold;
	Toast(TEXT("Caelen: ") + Line, 3.2);
	Sound(TEXT("CaelenSquawk"), CPos);
	// and he says it out loud, high and fast (web dj.ts sayCaelenLine: rate 1.08, pitch 1.9)
	if (const AKeysAudio* Au = AKeysAudio::Get(this); Au && Au->IsSoundOn()) KeysSpeech::Speak(Line, 1.08f, 1.9f, .9f, .35f);
}

bool AKeysExtras::CaelenHitbox(FVector& OutA, FVector& OutB, double& OutR) const
{
	if (!bCaelenInit || CAway > 0) return false;
	// along his heading; CYaw is the Unreal yaw of his flight in game x/z (x east, z south)
	const FVector F(FMath::Cos(CYaw) * CaelenHalfLen, 0, FMath::Sin(CYaw) * CaelenHalfLen);
	OutA = CPos - F; OutB = CPos + F; OutR = CaelenRadius;
	return true;
}

void AKeysExtras::CaelenShot(bool bHit)
{
	UE_LOG(LogTemp, Log, TEXT("KRCaelen shot: %s"), bHit ? TEXT("hit") : TEXT("near miss"));
	if (CAway > 0) return;
	if (!bHit) { CaelenSay(TEXT("you suck at this braowww")); return; }
	CaelenSay(TEXT("AYO- that's assault braowww"), 2.4);
	CAway = CaelenAwayS; CLeaveT = CaelenLeaveS; CExitLineT = CaelenExitDelay;
	Sound(TEXT("CrowdRoar"));
	Toast(CaelenDownCheers[FMath::RandRange(0, 3)], 4);
}

void AKeysExtras::UpdateCaelen(double Dt, double T)
{
	AKeysBoatPawn* P = PawnRef.Get();
	if (!P || !P->bRunning) { GullMesh->SetVisibility(false); return; }
	const double Sp = FMath::Abs(PawnSpeed()), BoatY = PawnY();
	if (!bCaelenInit)
	{
		bCaelenInit = true;
		CA = FMath::FRand() * 2 * PI;
		CR = 12; CHt = 8;
		CTalkT = ERand(20, 35);
		CLine = CaelenLines[0];
		CPos = FVector(P->BoatX() + FMath::Cos(CA) * CR, BoatY + CHt, P->BoatZ() + FMath::Sin(CA) * CR);
		CYaw = CA + PI * .5;
		GullMesh->SetVisibility(true);
	}
	FVector NewPos;
	if (CAway > 0)
	{
		// sulking: he climbs away on the heading he was hit on, blinks out for the rest of the two minutes, then comes back
		CAway -= Dt;
		if (CExitLineT > 0) { CExitLineT -= Dt; if (CExitLineT <= 0) CaelenSay(TEXT("fine - I'm going to Milan with 2 beautiful women anyway"), 3.2); }
		if (CBubble > 0) CBubble -= Dt;
		if (CAway <= 0)
		{
			CAway = 0; CLeaveT = 0; CExitLineT = 0;
			CR = 12; CHt = 8; CA = FMath::FRand() * 2 * PI; CSwoop = 0;
			CPos = FVector(P->BoatX() + FMath::Cos(CA) * CR, BoatY + CHt, P->BoatZ() + FMath::Sin(CA) * CR);
			GullMesh->SetVisibility(true);
			CaelenSay(TEXT("I'm back. I'm an executive, you can't ban me"));
			CTalkT = ERand(45, 90);
			return;
		}
		if (CLeaveT <= 0) return;
		CLeaveT -= Dt;
		CR += Dt * 14; CHt += Dt * 7; CA += Dt * .5;
		NewPos = FVector(P->BoatX() + FMath::Cos(CA) * CR, BoatY + CHt, P->BoatZ() + FMath::Sin(CA) * CR);
		if (CLeaveT <= 0) GullMesh->SetVisibility(false);
	}
	else
	{
		// over a diver in the water he comes down and circles them instead (web caelen.ts DIVE_RADIUS / DIVE_HEIGHT):
		// what gulls do to a spearo, and inside a speargun's reach so he can be shot at; slow enough to track
		const AKeysDiver* Dv = AKeysDiver::Get(this);
		const bool bOverDiver = Dv && Dv->IsDiving();
		double OX = P->BoatX(), OZ = P->BoatZ(), OY = BoatY;
		if (bOverDiver) { OX = Dv->State().X; OZ = Dv->State().Z; OY = 0; }
		if (bOverDiver)
		{
			CA += Dt * .22;
			CR = FMath::Lerp(CR, 5.5, FMath::Min(1.0, Dt * .6));
			CHt = FMath::Lerp(CHt, 2.9, FMath::Min(1.0, Dt * .6));
			CSwoop = 0;
		}
		else
		{
		// keep pace with the boat: circle wider and higher when running, swoop low over the cockpit now and then
		CA += Dt * (.35 + FMath::Min(.5, Sp * .012));
		CR = FMath::Lerp(CR, Sp > 8 ? 20.0 : 12.0, Dt * .5);
		CHt = FMath::Lerp(CHt, Sp > 8 ? 10.0 : 7.0, Dt * .5);
		}
		if (CSwoop <= 0 && FMath::FRand() < Dt * .03 && Sp < 6) CSwoop = 3.5;
		double R = CR, Hh = CHt;
		if (CSwoop > 0)
		{
			CSwoop -= Dt;
			const double Q = FMath::Sin(PI * (1 - FMath::Max(0.0, CSwoop) / 3.5));
			R = FMath::Lerp(R, 3.0, Q);
			Hh = FMath::Lerp(Hh, 3.2, Q);
		}
		NewPos = FVector(OX + FMath::Cos(CA) * R, OY + Hh + FMath::Sin(T * 1.7) * (bOverDiver ? .25 : .6), OZ + FMath::Sin(CA) * R);
	}

	// steer: face along the motion and bank into the turn (JS moveBird)
	const FVector Vel = NewPos - CPos;
	const double Horiz = FMath::Sqrt(Vel.X * Vel.X + Vel.Z * Vel.Z);
	double PitchR = 0;
	if (Horiz > 1e-4)
	{
		const double Yaw = FMath::Atan2(Vel.Z, Vel.X), DY = EWrapPi(Yaw - CYaw);
		CYaw += DY * FMath::Min(1.0, Dt * 6);
		CRoll = FMath::Lerp(CRoll, FMath::Clamp(DY / FMath::Max(Dt, 1e-3) * .05, -.9, .9), FMath::Min(1.0, Dt * 3));
		PitchR = FMath::Clamp(FMath::Atan2(Vel.Y, Horiz), -1.3, 1.3);
	}
	CPos = NewPos;

	// flap most of the time, glide now and then
	const double Flap = (FMath::Sin(T * .9) > -.2 || Sp > 10 || CAway > 0) ? 1 : 0;
	CPh += Dt * (CAway > 0 ? 13 : 9) * FMath::Max(.15, Flap);
	const double F = FMath::Sin(CPh), F2 = FMath::Sin(CPh - .7);
	FKeysMesh M;
	BuildGull(M, .12 + F * .75 * Flap, .05 + F2 * .55 * Flap);
	M.Commit(GullMesh, 0, AKeysWorld::VertexColorMaterial());
	GullMesh->SetWorldLocationAndRotation(KeysGeo::ToUE(CPos.X, CPos.Y, CPos.Z),
		FRotator(FMath::RadiansToDegrees(PitchR), FMath::RadiansToDegrees(CYaw), FMath::RadiansToDegrees(CRoll)));
	if (CAway > 0) return;

	// he has opinions
	CTalkT -= Dt;
	if (CTalkT <= 0)
	{
		CTalkT = ERand(45, 90);
		CLineIdx = (CLineIdx + 1) % UE_ARRAY_COUNT(CaelenLines);
		CaelenSay(CaelenLines[CLineIdx]);
	}
	if (CBubble > 0) CBubble -= Dt;
}

void AKeysExtras::AboardPositions(TArray<FVector, TInlineAllocator<16>>& Out) const
{
	if (Captain.bVisible) Out.Add(Captain.Pos);
	if (Crew.bVisible) Out.Add(Crew.Pos);
	if (bMateOn && Mate.bVisible) Out.Add(Mate.Pos);
	for (const FKeysPartyGoer& G : Party) if (G.H.bVisible) Out.Add(G.H.Pos);
}
