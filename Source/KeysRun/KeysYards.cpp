// Cowfish Cove: the dry stack marina and the boatyard on the real map, either side of the little cove on the Gulf
// side just east of Keys Fisheries (Marathon). The cove is their basin: the dry stack's apron, launch well, fuel dock
// and dockmaster's office on the west bank and across the head of it, the boatyard's gravel, travel lift and slip on
// the east bank. Everything is built here in the yard's own frame (X east, Y south, Z up from sea level, metres) under
// one scene component, as a handful of meshes each with its own draw distance; KeysYardProps.cpp makes the boats and
// the clutter, KeysForklift.cpp the forklift.
#include "KeysYards.h"
#include "KeysGeo.h"
#include "KeysWorld.h"
#include "KeysMesh.h"
#include "KeysTownData.h"
#include "KeysEstates.h"
#include "ProceduralMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Components/PointLightComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Camera/PlayerCameraManager.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "EngineUtils.h"

using namespace KeysYardProps;

namespace
{
	constexpr double PadY = 1.8;                    // the top of the made ground, above sea level
	constexpr double BasinDepth = 2.6, SlipDepth = 2.4;
	struct FYRect { double X0, Y0, X1, Y1; bool In(double X, double Y, double M = 0) const { return X >= X0 - M && X <= X1 + M && Y >= Y0 - M && Y <= Y1 + M; } };
	const FYRect Basin = {-18, -48, 18, 0};
	const FYRect Slip = {18, -17, 31, -11};
	// the made ground: the dry stack's apron, the quay across the head of the cove, and the boatyard round its slip
	const FYRect Pads[] = {{-60, -26, -18, 18}, {-18, 0, 18, 18}, {18, -30, 60, -17}, {31, -17, 60, -11}, {18, -11, 60, 22}};
	// the rack: ten bays four high, open to the east
	constexpr double FaceX = -48, RackBackX = -58.5, RackY0 = -24, BayW = 3.6, BunkOff = .8;
	constexpr int32 NBays = 10, NLevels = 4;
	double LevelZ(int32 K) { return 1.0 + 3.6 * K; }   // the top of a level's bunks, above the apron
	double BayY(int32 J) { return RackY0 + (J + .5) * BayW; }
	constexpr double RoofEave = 15.5;
	constexpr double WellY = -13;
	constexpr double DockZ = 1.15;                  // the fuel dock's deck

	// the yard's origin (game metres): the middle of the quay at the head of the cove, placed off Keys Fisheries
	bool Origin(double& OutX, double& OutZ)
	{
		// -KRNoYards leaves them out altogether (for before-and-after frame times)
		static const bool bOff = FParse::Param(FCommandLine::Get(), TEXT("KRNoYards"));
		if (bOff || !KeysGeo::RealMap()) return false;
		const TArray<KeysGeo::FMarina>& Ms = KeysGeo::Marinas();
		if (Ms.Num() < 4) return false;
		OutX = Ms[3].SX + 54;
		OutZ = Ms[3].SZ - Ms[3].Dir * 43;
		return true;
	}

	FVector P3(double X, double Y, double Z) { return FVector(X, Y, Z); }
	void Tube(FKeysMesh& M, const FVector& A, const FVector& B, double R, uint32 C, int32 Sides = 6) { M.AddTube(A, B, R, R, Sides, Hex(C)); }
	// a timber pile with a white cap
	void Pile(FKeysMesh& M, double X, double Y, double Top)
	{
		M.Surf = SurfWood;
		M.AddTube(P3(X, Y, -3.2), P3(X, Y, Top), .17, .15, 8, Hex(0x6e5a44));
		M.Surf = SurfDefault;
		M.AddTube(P3(X, Y, Top), P3(X, Y, Top + .1), .18, .05, 8, Hex(0xe8ebee));
	}
	// a yard light on a pole: the lamp head reaches out along (DX, DY)
	void LightPole(FKeysMesh& S, FKeysMesh& G, TArray<FVector>& Lamps, double X, double Y, double DX, double DY, double H = 8)
	{
		S.Surf = SurfSteel;
		S.AddTube(P3(X, Y, PadY), P3(X, Y, PadY + H), .1, .07, 8, Hex(0x8d9499));
		S.AddTube(P3(X, Y, PadY + H - .1), P3(X + DX * 1.6, Y + DY * 1.6, PadY + H + .15), .05, .05, 6, Hex(0x8d9499));
		S.Surf = SurfMetalPaint;
		Box(S, X + DX * 1.9, Y + DY * 1.9, PadY + H + .12, .7, .7, .14, Hex(0x5b6168), FMath::Atan2(DY, DX));
		S.Surf = SurfDefault;
		Box(G, X + DX * 1.9, Y + DY * 1.9, PadY + H + .03, .5, .5, .04, FLinearColor(1.f, .93f, .74f), FMath::Atan2(DY, DX));
		Lamps.Add(P3(X + DX * 1.9, Y + DY * 1.9, PadY + H - .4));
	}
}

// ---------------------------------------------------------------- the site
namespace KeysYards
{
	const TArray<FSite>& Sites()
	{
		static TArray<FSite> S;
		double X0, Z0;
		if (!Origin(X0, Z0)) { S.Reset(); return S; }
		if (!S.Num())
		{
			S.Add({TEXT("Cowfish Cove Dry Stack"), X0 - 39, Z0 - 4, X0 + 3, Z0 - 30, PI / 2});
			S.Add({TEXT("Cowfish Cove Boatyard"), X0 + 39, Z0 - 4, X0 + 3, Z0 - 22, -PI / 2});
		}
		return S;
	}

	bool Ground(double X, double Z, double& OutH)
	{
		double X0, Z0;
		if (!Origin(X0, Z0)) return false;
		const double LX = X - X0, LY = Z - Z0;
		if (LX < -61 || LX > 61 || LY < -49 || LY > 23) return false;
		if (Basin.In(LX, LY)) { OutH = -BasinDepth; return true; }
		if (Slip.In(LX, LY)) { OutH = -SlipDepth; return true; }
		for (const FYRect& R : Pads) if (R.In(LX, LY)) { OutH = PadY - .12; return true; }
		return false;
	}

	bool InFootprint(double X, double Z, double Margin)
	{
		double X0, Z0;
		if (!Origin(X0, Z0)) return false;
		const double LX = X - X0, LY = Z - Z0;
		if (Basin.In(LX, LY, Margin)) return true;
		for (const FYRect& R : Pads) if (R.In(LX, LY, Margin)) return true;
		return false;
	}
}

AKeysYards::AKeysYards()
{
	PrimaryActorTick.bCanEverTick = true;
	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);
}

AKeysYards* AKeysYards::Get(const UObject* WorldContext)
{
	UWorld* W = WorldContext ? WorldContext->GetWorld() : nullptr;
	if (!W) return nullptr;
	TActorIterator<AKeysYards> It(W);
	return It ? *It : nullptr;
}

UProceduralMeshComponent* AKeysYards::NewMesh(const TCHAR* Name, USceneComponent* Parent, double DrawDistM, bool bShadow)
{
	UProceduralMeshComponent* C = NewObject<UProceduralMeshComponent>(this, FName(Name));
	C->SetupAttachment(Parent);
	C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	C->bUseAsyncCooking = true;
	C->SetCastShadow(bShadow);
	C->RegisterComponent();
	if (DrawDistM > 0) C->SetCachedMaxDrawDistance(float(DrawDistM * 100));
	Meshes.Add(C);
	return C;
}

UTextRenderComponent* AKeysYards::NewText(USceneComponent* Parent, const FString& Text, const FVector& PosM, double YawDeg, double SizeM, const FColor& Color)
{
	UTextRenderComponent* Tx = NewObject<UTextRenderComponent>(this);
	Tx->SetupAttachment(Parent);
	Tx->RegisterComponent();
	Tx->SetText(FText::FromString(Text));
	Tx->SetHorizontalAlignment(EHTA_Center);
	Tx->SetVerticalAlignment(EVRTA_TextCenter);
	Tx->SetTextRenderColor(Color);
	Tx->SetWorldSize(float(SizeM * 100));
	Tx->SetRelativeLocationAndRotation(PosM * 100.0, FRotator(0, YawDeg, 0));
	Tx->SetCastShadow(false);
	Tx->SetCachedMaxDrawDistance(40000.f);
	Texts.Add(Tx);
	return Tx;
}

// ---------------------------------------------------------------- the made ground
void AKeysYards::BuildGround(FKeysMesh& S, FKeysMesh& D)
{
	const FLinearColor Conc = Hex(0xb9b5aa), ConcD = Hex(0x8f8b80), Stain = Hex(0x5d5a50), Gravel = Hex(0xa9a294), Cap = Hex(0xc9c5ba);
	for (int32 r = 0; r < UE_ARRAY_COUNT(Pads); r++)
	{
		const FYRect& R = Pads[r];
		const bool bGravel = R.X0 >= 18;
		S.Surf = bGravel ? SurfSand : SurfConcrete;
		// the top, in squares a shade apart: poured slabs on the apron, raked gravel in the yard
		const double St = bGravel ? 2.5 : 4.0;
		for (double X = R.X0; X < R.X1 - .01; X += St)
			for (double Y = R.Y0; Y < R.Y1 - .01; Y += St)
			{
				const double X1 = FMath::Min(R.X1, X + St), Y1 = FMath::Min(R.Y1, Y + St);
				const double Hh = Hash(FMath::FloorToDouble(X) + r * 31.0, FMath::FloorToDouble(Y));
				FLinearColor C = bGravel ? Gravel * float(.94 + .1 * Hh) : Conc * float(.95 + .07 * Hh);
				if (bGravel && Hh < .07) C = Gravel * .8f;   // a patch of old antifouling dust and oil
				const double Gap = bGravel ? 0 : .02;
				S.AddQuad(P3(X + Gap, Y + Gap, PadY), P3(X1 - Gap, Y + Gap, PadY), P3(X1 - Gap, Y1 - Gap, PadY), P3(X + Gap, Y1 - Gap, PadY), C, FVector::UpVector);
			}
		if (!bGravel) S.AddQuad(P3(R.X0, R.Y0, PadY - .015), P3(R.X1, R.Y0, PadY - .015), P3(R.X1, R.Y1, PadY - .015), P3(R.X0, R.Y1, PadY - .015), ConcD, FVector::UpVector);
		// the walls down into the water, stained dark below the tide line
		S.Surf = SurfConcrete;
		const FVector Cn[4] = {P3(R.X0, R.Y0, 0), P3(R.X1, R.Y0, 0), P3(R.X1, R.Y1, 0), P3(R.X0, R.Y1, 0)};
		const FVector Out[4] = {FVector(0, -1, 0), FVector(1, 0, 0), FVector(0, 1, 0), FVector(-1, 0, 0)};
		for (int32 k = 0; k < 4; k++)
		{
			const FVector A = Cn[k], B = Cn[(k + 1) % 4];
			auto Up = [](const FVector& V, double Z) { return FVector(V.X, V.Y, Z); };
			S.AddQuad(Up(A, .45), Up(B, .45), Up(B, PadY), Up(A, PadY), Conc * .92f, Out[k]);
			S.AddQuad(Up(A, -3.4), Up(B, -3.4), Up(B, .45), Up(A, .45), Stain, Out[k]);
		}
	}
	// the bulkhead cap round the basin and the slip
	S.Surf = SurfConcrete;
	auto CapRun = [&](double X0, double Y0, double X1, double Y1)
	{
		Box(S, (X0 + X1) / 2, (Y0 + Y1) / 2, PadY + .02, FMath::Abs(X1 - X0) + .5, FMath::Abs(Y1 - Y0) + .5, .12, Cap);
	};
	CapRun(-18.25, -26, -18.25, -.25);
	CapRun(-18, .25, 18, .25);
	CapRun(18.25, -30, 18.25, -17.25);
	CapRun(18.25, -10.75, 18.25, -.25);
	CapRun(18.5, -17.25, 31.25, -17.25);
	CapRun(18.5, -10.75, 31.25, -10.75);
	CapRun(31.25, -17, 31.25, -11);
	// the launch well's edge is painted, and the forklift's lane down the apron
	D.Surf = SurfDefault;
	for (int32 k = 0; k < 10; k++) Box(D, -18.25, WellY - 2.5 + k * .5 + .25, PadY + .09, .5, .5, .02, k % 2 ? Hex(0x15171a) : Hex(0xe8b923));
	for (double Yl = RackY0; Yl < RackY0 + NBays * BayW - 1; Yl += 3) Box(D, FaceX + 5.2, Yl + 1, PadY + .012, .14, 1.6, .012, Hex(0xe8b923));
	for (int32 j = 0; j <= NBays; j++) Box(D, FaceX + 1.4, RackY0 + j * BayW, PadY + .012, 2.4, .1, .012, Hex(0xe9e6dc));
	S.Surf = SurfDefault;
}

// ---------------------------------------------------------------- the dry stack
void AKeysYards::BuildDryStack(FKeysMesh& S, FKeysMesh& Bt, FKeysMesh& D, FKeysMesh& G)
{
	const FLinearColor Steel = Hex(0x6f7a82), SteelL = Hex(0x8f999f), Carpet = Hex(0x39414f), Sheet = Hex(0xe4e7e6), RoofC = Hex(0xc3ced4), Trim = Hex(0x1f6f78);
	const double FrontX = FaceX - .6, MidX = (RackBackX + FrontX) / 2, Deep = FrontX - RackBackX;
	const double RackY1 = RackY0 + NBays * BayW;
	// ---- the frame: a column front and back on every bay line, an arm between them at each level
	S.Surf = SurfMetalPaint;
	for (int32 j = 0; j <= NBays; j++)
	{
		const double Y = RackY0 + j * BayW;
		for (double X : {RackBackX, FrontX})
		{
			Box(S, X, Y, PadY + RoofEave / 2, .3, .3, RoofEave, Steel);
			Box(S, X, Y, PadY + .03, .6, .6, .06, SteelL);
		}
		for (int32 k = 0; k < NLevels; k++) Box(S, MidX, Y, PadY + LevelZ(k) - .62, Deep + .3, .18, .3, Steel);
		Box(S, MidX, Y, PadY + RoofEave - .15, Deep + .3, .18, .3, Steel);
		// cross bracing in the back wall, every other bay
		if (j < NBays && j % 2 == 0)
			for (int32 k = 0; k < NLevels; k++)
			{
				const double Z0 = PadY + (k ? LevelZ(k) - .62 : .2), Z1 = PadY + (k + 1 < NLevels ? LevelZ(k + 1) - .62 : RoofEave - .3);
				S.AddTube(P3(RackBackX, Y, Z0), P3(RackBackX, Y + BayW, Z1), .035, .035, 4, SteelL);
				S.AddTube(P3(RackBackX, Y + BayW, Z0), P3(RackBackX, Y, Z1), .035, .035, 4, SteelL);
			}
	}
	// the bracing in the two open ends
	for (double Y : {RackY0, RackY1})
		for (int32 k = 0; k < NLevels; k++)
		{
			const double Z0 = PadY + (k ? LevelZ(k) - .62 : .2), Z1 = PadY + (k + 1 < NLevels ? LevelZ(k + 1) - .62 : RoofEave - .3);
			S.AddTube(P3(RackBackX, Y, Z0), P3(FrontX, Y, Z1), .04, .04, 4, SteelL);
			S.AddTube(P3(FrontX, Y, Z0), P3(RackBackX, Y, Z1), .04, .04, 4, SteelL);
		}
	// a slot's bunks: three cross members between the arms and two carpeted boards on risers
	for (int32 j = 0; j < NBays; j++)
		for (int32 k = 0; k < NLevels; k++)
		{
			const double Y = BayY(j), Z = PadY + LevelZ(k);
			S.Surf = SurfMetalPaint;
			for (double X : {RackBackX + 1.3, MidX, FrontX - 1.3})
			{
				Box(S, X, Y, Z - .55, .12, BayW, .14, Steel);
				for (int32 a = -1; a <= 1; a += 2) Box(S, X, Y + a * BunkOff, Z - .3, .09, .09, .38, Steel);
			}
			S.Surf = SurfCloth;
			for (int32 a = -1; a <= 1; a += 2) Box(S, MidX, Y + a * BunkOff, Z - .06, Deep - .5, .16, .12, Carpet);
		}
	// ---- the roof, the sheeted back wall above head height, gables, and the fascia over the open front
	S.Surf = SurfTin;
	{
		const double X0 = RackBackX - 1.2, X1 = FaceX + 1.8, Xr = (X0 + X1) / 2, Y0 = RackY0 - 1.2, Y1 = RackY1 + 1.2, Ze = PadY + RoofEave, Zr = Ze + 1.4;
		S.AddQuad(P3(X0, Y0, Ze), P3(Xr, Y0, Zr), P3(Xr, Y1, Zr), P3(X0, Y1, Ze), RoofC, FVector(-.3, 0, 1));
		S.AddQuad(P3(Xr, Y0, Zr), P3(X1, Y0, Ze), P3(X1, Y1, Ze), P3(Xr, Y1, Zr), RoofC * .96f, FVector(.3, 0, 1));
		// standing seams
		for (double Y = Y0 + 1; Y < Y1; Y += 1.2)
		{
			S.AddTube(P3(X0, Y, Ze + .03), P3(Xr, Y, Zr + .03), .03, .03, 3, RoofC * .85f);
			S.AddTube(P3(Xr, Y, Zr + .03), P3(X1, Y, Ze + .03), .03, .03, 3, RoofC * .85f);
		}
		for (double Y : {Y0, Y1})
		{
			S.AddTri(P3(X0, Y, Ze), P3(X1, Y, Ze), P3(Xr, Y, Zr), Sheet, FVector(0, Y < 0 ? -1 : 1, 0));
			S.AddQuad(P3(X0, Y, Ze - 1.6), P3(X1, Y, Ze - 1.6), P3(X1, Y, Ze), P3(X0, Y, Ze), Sheet, FVector(0, Y < 0 ? -1 : 1, 0));
		}
		S.Surf = SurfSiding;
		S.AddQuad(P3(RackBackX - .2, Y0, PadY + 4.3), P3(RackBackX - .2, Y1, PadY + 4.3), P3(RackBackX - .2, Y1, Ze), P3(RackBackX - .2, Y0, Ze), Sheet, FVector(-1, 0, 0));
		S.Surf = SurfMetalPaint;
		Box(S, X1, (Y0 + Y1) / 2, Ze - .55, .14, Y1 - Y0, 1.5, Trim);
		NewText(Site, TEXT("COWFISH COVE DRY STACK"), P3(X1 + .09, (Y0 + Y1) / 2, Ze - .55), 0, 1.05, FColor(0xf4, 0xf1, 0xe4));
		// lamps under the fascia
		for (int32 k = 0; k < 4; k++)
		{
			const double Y = RackY0 + 4.5 + k * 9;
			Box(S, X1 - .5, Y, Ze - 1.42, .5, .3, .12, Hex(0x5b6168));
			Box(G, X1 - .5, Y, Ze - 1.5, .4, .24, .04, FLinearColor(1.f, .93f, .74f));
			if (k == 1 || k == 2) LampAt.Add(P3(X1 + 1, Y, Ze - 2.5));
		}
	}
	S.Surf = SurfDefault;

	// ---- the boats. Four of them the forklift can move (their own meshes); a quarter of the bunks empty.
	const int32 LiveSlot[][2] = {{5, 1}, {2, 0}, {7, 2}, {8, 1}};
	auto LiveAt = [&](int32 j, int32 k) { for (int32 i = 0; i < UE_ARRAY_COUNT(LiveSlot); i++) if (LiveSlot[i][0] == j && LiveSlot[i][1] == k) return i; return -1; };
	// the ten emptiest dice are the empty bunks
	TArray<TPair<double, int32>> Dice;
	for (int32 j = 0; j < NBays; j++) for (int32 k = 0; k < NLevels; k++) if (LiveAt(j, k) < 0) Dice.Add({Hash(j * 7.3 + 1.1, k * 3.7 + 2.9), j * NLevels + k});
	Dice.Sort([](const TPair<double, int32>& A, const TPair<double, int32>& B) { return A.Key < B.Key; });
	TSet<int32> Empty;
	for (int32 i = 0; i < 10 && i < Dice.Num(); i++) Empty.Add(Dice[i].Value);
	int32 NRacked = 0, NKind[3] = {0, 0, 0}, NNamed = 0;
	Fork.Boats.Reset();
	for (int32 j = 0; j < NBays; j++)
		for (int32 k = 0; k < NLevels; k++)
		{
			if (Empty.Contains(j * NLevels + k)) continue;
			const int32 Live = LiveAt(j, k);
			// the light boats go up top; the forklift's own four are centre consoles and bay boats it can balance
			FBoat B;
			for (int32 Try = 0; Try < 8; Try++)
			{
				B = MakeBoat(j * 17 + k * 5 + Try * 101 + 3, Live >= 0 ? 8.3 : 9.2, Live >= 0);
				if (k >= 2 && B.Kind == BoatCC && B.L > 8.2 && Try < 7) continue;
				if (k == 0 && B.Kind == BoatSkiff && Try < 3) continue;
				break;
			}
			NRacked++;
			NKind[FMath::Min<int32>(B.Kind, 2)]++;
			const double SlotZ = LevelZ(k) - BottomZ(B, BunkOff);
			if (Live >= 0)
			{
				if (Fork.Boats.Num() <= Live) Fork.Boats.SetNum(Live + 1);
				FKeysForklift::FBoat& F = Fork.Boats[Live];
				F.L = B.L; F.Seat = -BottomZ(B, FKeysForklift::ForkHalf); F.SlotY = BayY(j); F.SlotZ = SlotZ;
				if (LiveBoats.Num() <= Live) LiveBoats.SetNum(Live + 1);
				UProceduralMeshComponent* C = NewMesh(*FString::Printf(TEXT("LiveBoat%d"), Live), Site, 1500);
				FKeysMesh M;
				BuildBoat(M, B);
				M.Commit(C, 0, AKeysWorld::VertexColorMaterial());
				Tris += M.T.Num() / 3;
				LiveBoats[Live] = C;
				NewText(C, B.Name.ToUpper(), P3(-B.L / 2 - .03, 0, SheerZ(B) * .62), 180, .15, FColor(0x10, 0x16, 0x1c));
				UE_LOG(LogTemp, Log, TEXT("KRYard live boat %d \"%s\" (%s, %.1f m) bay %d level %d"), Live, *B.Name, *B.Brand, B.L, j + 1, k + 1);
				continue;
			}
			FKeysMesh M;
			BuildBoat(M, B);
			const FVector At = P3(FaceX - .3 - B.L / 2, BayY(j), PadY + SlotZ);
			Append(Bt, M, FTransform(FQuat(FVector::ZAxisVector, PI), At));
			// the names on the transoms you can read from the apron
			if (k <= 1 && NNamed < 10) { NewText(Site, B.Name.ToUpper(), P3(FaceX - .3 + .03, BayY(j), PadY + SlotZ + SheerZ(B) * .62), 0, .15, FColor(0x10, 0x16, 0x1c)); NNamed++; }
		}
	UE_LOG(LogTemp, Log, TEXT("KRYard dry stack: %d bunks, %d boats (%d skiffs, %d bay boats, %d centre consoles), %d empty"), NBays * NLevels, NRacked, NKind[0], NKind[1], NKind[2], Empty.Num());

	// ---- the launch well: a float to lie a launched boat against, a guide float the other side, piles
	const double BerthFace = WellY + Fork.BerthDY - 1.75;
	S.Surf = SurfWood;
	for (double X = -17.6; X < -7.2; X += .42) Box(S, X, BerthFace - .65, .78, .4, 1.3, .07, Hex(0x9c7a55) * float(.86 + .24 * Hash(X, 3)));
	for (double X = -17.6; X < -9.2; X += .42) Box(S, X, WellY + 3.3, .78, .4, 1.0, .07, Hex(0x9c7a55) * float(.86 + .24 * Hash(X, 7)));
	Box(S, -12.6, BerthFace - .65, .62, 10.6, 1.1, .22, Hex(0x5f4f3c));
	Box(S, -13.5, WellY + 3.3, .62, 8.6, .8, .22, Hex(0x5f4f3c));
	for (double X : {-17.2, -12.4, -7.6}) Pile(S, X, BerthFace - 1.45, 2.3);
	for (double X : {-17.2, -9.6}) Pile(S, X, WellY + 3.95, 2.3);
	// fenders along the float's face and a ramp down to it from the apron
	S.Surf = SurfVinyl;
	for (double X : {-15.5, -12.5, -9.5}) S.AddTube(P3(X, BerthFace + .06, .15), P3(X, BerthFace + .06, .8), .11, .11, 8, Hex(0xf2f2f0));
	S.Surf = SurfWood;
	S.AddBox(P3(-18.9, BerthFace - .65, (PadY + .82) / 2 + .4), FVector(2.4, 1.0, .07), Hex(0x9c7a55), FQuat(FVector::YAxisVector, FMath::Atan2(PadY - .82, 2.2)));
	S.Surf = SurfDefault;

	// ---- the fuel dock across the head of the cove: planks on piles, two pumps, a sign, a ramp down from the quay
	S.Surf = SurfWood;
	for (double X = -14; X < 14; X += .45) Box(S, X + .21, -1.75, DockZ - .04, .42, 3.1, .08, Hex(0x9c7a55) * float(.84 + .26 * Hash(X, 11)));
	Box(S, 0, -1.75, DockZ - .2, 28, 2.7, .22, Hex(0x5f4f3c));
	for (double X = -14; X <= 14.01; X += 7) { Pile(S, X, -3.45, 2.5); Pile(S, X, -.25, 1.2); }
	S.AddBox(P3(-9.5, -.2, (PadY + DockZ) / 2), FVector(1.6, 1.9, .08), Hex(0x9c7a55), FQuat(FVector::XAxisVector, -FMath::Atan2(PadY - DockZ, 1.7)));
	S.AddBox(P3(9.5, -.2, (PadY + DockZ) / 2), FVector(1.6, 1.9, .08), Hex(0x9c7a55), FQuat(FVector::XAxisVector, -FMath::Atan2(PadY - DockZ, 1.7)));
	S.Surf = SurfDefault;
	FuelPump(D, P3(-4.5, -1.5, DockZ), PI / 2, 0xc8302e);
	FuelPump(D, P3(4.5, -1.5, DockZ), PI / 2, 0x2f7d4f);
	NewText(Site, TEXT("REC 90"), P3(-4.5, -1.5 - .27, DockZ + 1.97), -90, .13, FColor(0xb8, 0x22, 0x1e));
	NewText(Site, TEXT("DIESEL"), P3(4.5, -1.5 - .27, DockZ + 1.97), -90, .13, FColor(0x1f, 0x5a, 0x38));
	for (double X = -12.5; X <= 12.6; X += 5) { D.Surf = SurfSteel; Box(D, X, -3.15, DockZ + .05, .32, .1, .09, Hex(0x3a3d40)); D.Surf = SurfDefault; }
	// a life ring on its post, a fire extinguisher cabinet, a hose reel, a bin
	Box(D, 0, -.45, DockZ + .7, .1, .1, 1.4, Hex(0xe8e4d8));
	for (int32 k = 0; k < 10; k++)
	{
		const double A0 = 2 * PI * k / 10, A1 = 2 * PI * (k + 1) / 10;
		D.AddTube(P3(FMath::Cos(A0) * .3, -.52, DockZ + 1.15 + FMath::Sin(A0) * .3), P3(FMath::Cos(A1) * .3, -.52, DockZ + 1.15 + FMath::Sin(A1) * .3), .055, .055, 5, k % 3 == 0 ? Hex(0xf2f2f0) : Hex(0xe4572e));
	}
	Box(D, 1.6, -.4, DockZ + .75, .4, .25, .7, Hex(0xc8102e));
	Wheel(D, P3(-1.7, -.5, DockZ + .5), P3(-1.7, -.3, DockZ + .5), .32, Hex(0x2a5d9c), 12);
	D.AddTube(P3(12.8, -1.0, DockZ), P3(12.8, -1.0, DockZ + .8), .28, .3, 10, Hex(0x3f5a4a));
	SignBoard(S, P3(11.5, 1.6, PadY), -PI / 2, 3.4, 1.3, 3.2, 0xc8302e);
	NewText(Site, TEXT("FUEL"), P3(11.5, 1.6 - .05, PadY + 3.85), -90, 1.0, FColor(0xff, 0xff, 0xff));
	SignBoard(S, P3(11.5, 1.62, PadY + 1.9), -PI / 2, 3.4, 1.0, 0, 0xf2f2f0);
	NewText(Site, TEXT("REC 90   5.89\nDIESEL   4.95"), P3(11.5, 1.62 - .05, PadY + 2.4), -90, .34, FColor(0x15, 0x17, 0x1a));

	// ---- the dockmaster's office on the quay: a small conch-coloured building, door and windows to the water
	{
		const double Cx = -8, Cy = 10, W = 8, Dp = 5.6, Hh = 3.1;
		S.Surf = SurfConcrete;
		Box(S, Cx, Cy, PadY + .15, W + .4, Dp + .4, .3, Hex(0xd8d0be));
		S.Surf = SurfSiding;
		Box(S, Cx, Cy, PadY + .3 + Hh / 2, W, Dp, Hh, Hex(0xa8e0d8));
		S.Surf = SurfTin;
		const double Ze = PadY + .3 + Hh, Zr = Ze + 1.3;
		S.AddQuad(P3(Cx - W / 2 - .5, Cy - Dp / 2 - .9, Ze - .12), P3(Cx + W / 2 + .5, Cy - Dp / 2 - .9, Ze - .12), P3(Cx + W / 2 + .5, Cy, Zr), P3(Cx - W / 2 - .5, Cy, Zr), Hex(0xb8bcc2), FVector(0, -.4, 1));
		S.AddQuad(P3(Cx - W / 2 - .5, Cy + Dp / 2 + .5, Ze - .12), P3(Cx + W / 2 + .5, Cy + Dp / 2 + .5, Ze - .12), P3(Cx + W / 2 + .5, Cy, Zr), P3(Cx - W / 2 - .5, Cy, Zr), Hex(0xb0b4ba), FVector(0, .4, 1));
		S.Surf = SurfSiding;
		for (int32 a = -1; a <= 1; a += 2) S.AddTri(P3(Cx + a * W / 2, Cy - Dp / 2, Ze), P3(Cx + a * W / 2, Cy + Dp / 2, Ze), P3(Cx + a * W / 2, Cy, Zr), Hex(0xa8e0d8), FVector(a, 0, 0));
		// the porch posts under the roof's overhang, the door, two windows, the trim
		S.Surf = SurfWood;
		for (double X : {Cx - W / 2 - .3, Cx, Cx + W / 2 + .3}) Box(S, X, Cy - Dp / 2 - .8, PadY + .3 + (Hh - .3) / 2, .12, .12, Hh - .3, Hex(0xf4f2ec));
		Box(S, Cx, Cy - Dp / 2 - .45, PadY + .2, W + .6, .9, .12, Hex(0x9c7a55));
		S.Surf = SurfDefault;
		const double Fy = Cy - Dp / 2 - .03;
		Box(S, Cx + 1.2, Fy, PadY + .3 + 1.05, 1.0, .06, 2.1, Hex(0x2bb3a8));
		S.Surf = SurfGlass;
		for (double X : {Cx - 2.2, Cx + 3.1}) Box(S, X, Fy, PadY + .3 + 1.6, 1.4, .06, 1.1, Hex(0x27414f));
		Box(S, Cx + 1.2, Fy - .02, PadY + .3 + 1.6, .6, .04, .6, Hex(0x27414f));
		S.Surf = SurfDefault;
		for (double X : {Cx - 2.2, Cx + 3.1})
		{
			Box(G, X, Fy - .04, PadY + .3 + 1.6, 1.3, .02, 1.0, FLinearColor(1.f, .86f, .55f));
			for (int32 a = -1; a <= 1; a += 2) Box(S, X + a * .92, Fy, PadY + .3 + 1.6, .36, .06, 1.2, Hex(0x2bb3a8));
		}
		Box(S, Cx, Fy - .02, PadY + .3 + 2.65, 4.2, .05, .5, Hex(0xf4f1e4));
		NewText(Site, TEXT("DOCKMASTER"), P3(Cx, Fy - .06, PadY + .3 + 2.65), -90, .36, FColor(0x1f, 0x4f, 0x8c));
		LampAt.Add(P3(Cx, Cy - Dp / 2 - 1.4, PadY + 2.6));
		// an ice chest, a bench, the VHF whip and a weather vane's worth of antenna
		D.Surf = SurfMetalPaint;
		D.AddRoundBox(P3(Cx - 3.3, Cy - Dp / 2 - .5, PadY + .26 + .75), FVector(1.5, .8, 1.5), .05, Hex(0xe9ecee));
		D.Surf = SurfDefault;
		NewText(Site, TEXT("ICE"), P3(Cx - 3.3, Cy - Dp / 2 - .92, PadY + 1.5), -90, .4, FColor(0x2f, 0x6f, 0xd0));
		D.Surf = SurfWood;
		Box(D, Cx + 3.1, Cy - Dp / 2 - .5, PadY + .26 + .45, 1.6, .4, .06, Hex(0x8c6f4a));
		for (int32 a = -1; a <= 1; a += 2) Box(D, Cx + 3.1 + a * .7, Cy - Dp / 2 - .5, PadY + .26 + .22, .08, .36, .44, Hex(0x8c6f4a));
		D.Surf = SurfSteel;
		D.AddTube(P3(Cx + W / 2 - .4, Cy, Zr), P3(Cx + W / 2 - .4, Cy, Zr + 3.4), .02, .008, 4, Hex(0xe8ebee));
		D.Surf = SurfDefault;
	}

	// ---- the apron's furniture: lights along the bulkhead, the forklift's diesel tank, drums, pallets, a bin
	LightPole(S, G, LampAt, -19.4, -24.4, -1, 0);
	LightPole(S, G, LampAt, -19.4, -3.2, -1, 0);
	LightPole(S, G, LampAt, -22, 16.6, -.7, -.7);
	D.Surf = SurfMetalPaint;
	Wheel(D, P3(-27, 16.4, PadY + 1.0), P3(-24.2, 16.4, PadY + 1.0), .62, Hex(0xe9ecee), 14);
	for (double X : {-26.6, -24.6}) Box(D, X, 16.4, PadY + .3, .16, 1.1, .6, Hex(0x5b6168));
	D.Surf = SurfDefault;
	Drum(D, P3(-29, 16.8, PadY), 0x2f6fa8);
	Drum(D, P3(-29.7, 16.5, PadY), 0x2f6fa8);
	Drum(D, P3(-29.3, 16.0, PadY), 0xc8302e);
	Pallet(D, P3(-32, 16.4, PadY), .2);
	Pallet(D, P3(-32.1, 16.4, PadY + .15), .05);
	Dumpster(D, P3(-44.5, 15.6, PadY), 0);
	// a wash-down rack by the well: a boat on low stands waiting for its owner
	{
		FBoat B = MakeBoat(911, 7.6, true);
		FKeysMesh M;
		BuildBoat(M, B);
		BuildStands(M, B, -BlockedHeight(B));
		Append(Bt, M, FTransform(FQuat(FVector::ZAxisVector, -PI / 2), P3(-24.5, 6.5, PadY + BlockedHeight(B))));
	}
}

// ---------------------------------------------------------------- the boatyard
void AKeysYards::BuildBoatyard(FKeysMesh& S, FKeysMesh& Bt, FKeysMesh& D, FKeysMesh& G)
{
	// ---- the travel lift over its slip, open end to the water, a centre console just hauled hanging in the slings
	{
		FBoat B;
		B.Kind = BoatCC; B.L = 8.9; B.B = 2.85; B.Hull = 0xf6f1e4; B.Accent = 0x0f6e7a; B.Bottom = 0x3b4a39; B.Engines = 2; B.bTop = true; B.Top = 0xfbfaf6;
		B.Name = TEXT("Tide Loan"); B.Brand = TEXT("Tidewell");
		const double Cx = 24.5, Cy = -14, Keel = 1.25;
		const KeysMesh::FHullDef H = HullOf(B);
		const KeysMesh::FHullSta St = KeysMesh::HullStation(H, B.L, B.B, .3);
		const double Zb = Keel - St.YK;
		FTravelLift TL;
		TL.KeelZ = Keel; TL.ChineZ = Zb + St.YC; TL.ChineHalf = St.Bb; TL.SheerZ = Zb + St.YS; TL.SheerHalf = St.Bs;
		FKeysMesh M;
		TravelLift(M, TL);
		Append(S, M, FTransform(FQuat(FVector::ZAxisVector, PI), P3(Cx, Cy, PadY)));
		FKeysMesh Bm;
		BuildBoat(Bm, B);
		Append(Bt, Bm, FTransform(FQuat::Identity, P3(Cx - .3, Cy, PadY + Zb)));
		NewText(Site, B.Name.ToUpper(), P3(Cx - .3 - B.L / 2 - .03, Cy, PadY + Zb + SheerZ(B) * .62), 180, .15, FColor(0x10, 0x16, 0x1c));
		for (int32 a = -1; a <= 1; a += 2) NewText(Site, TEXT("KEYLIFT 35 TON"), P3(Cx, Cy + a * (TL.W / 2 + .27), PadY + TL.H - .05), a * 90, .38, FColor(0xf4, 0xf1, 0xe4));
	}

	// ---- the boats on the hard
	struct FHard { FBoat B; double X, Y, Yaw; };
	TArray<FHard> Hard;
	auto Add = [&](int32 Seed, uint8 Kind, double X, double Y, double Yaw) -> FBoat&
	{
		FHard Hd;
		for (int32 Try = 0; Try < 40; Try++) { Hd.B = MakeBoat(Seed + Try * 53, 8.8); if (Hd.B.Kind == Kind) break; }
		Hd.X = X; Hd.Y = Y; Hd.Yaw = Yaw;
		Hard.Add(Hd);
		return Hard.Last().B;
	};
	{
		// the sportfisher, outboards off for a repower
		FHard Hd;
		Hd.B.Kind = BoatSport; Hd.B.L = 11.6; Hd.B.B = 3.9; Hd.B.Hull = 0xfbfaf6; Hd.B.Accent = 0x1f2a44; Hd.B.Bottom = 0x8a2a2a; Hd.B.Engines = 3; Hd.B.bNoEngines = true;
		Hd.B.Name = TEXT("Marl Barker"); Hd.B.Brand = TEXT("Hardbottom");
		Hd.X = 46.5; Hd.Y = -23; Hd.Yaw = 0;
		Hard.Add(Hd);
	}
	Add(21, BoatCC, 45.5, -12.5, PI).bSanded = true;          // 1: being sanded
	Add(34, BoatBay, 46, -3.5, 0).bSanded = true;             // 2: sanded, the new bottom paint going on
	Add(47, BoatCC, 46, 5, PI).bWrapped = true;               // 3
	Add(58, BoatSkiff, 39.5, 13, 0).bWrapped = true;          // 4
	Add(69, BoatBay, 50, 14.5, PI).bWrapped = true;           // 5
	Add(73, BoatCC, 25.5, -2.5, PI / 2);                      // 6: on the wash-down pad by the lift
	Add(88, BoatSkiff, 24.5, 15.5, PI * .92);                 // 7: for sale
	Add(95, BoatCC, 34, 4.5, PI / 2).bSanded = true;          // 8
	Hard[2].B.Bottom = 0x1f3a5c;
	Hard[6].B.Bottom = 0x3b4a39;                              // still green from the water
	int32 NSanded = 0, NWrapped = 0;
	for (int32 i = 0; i < Hard.Num(); i++)
	{
		const FHard& Hd = Hard[i];
		FKeysMesh M;
		BuildBoat(M, Hd.B);
		const double Up = BlockedHeight(Hd.B);
		BuildStands(M, Hd.B, -Up);
		const FTransform Tf(FQuat(FVector::ZAxisVector, Hd.Yaw), P3(Hd.X, Hd.Y, PadY + Up));
		Append(Bt, M, Tf);
		NSanded += Hd.B.bSanded; NWrapped += Hd.B.bWrapped;
		if (!Hd.B.bWrapped) NewText(Site, Hd.B.Name.ToUpper(), Tf.TransformPosition(P3(-Hd.B.L / 2 - .03, 0, SheerZ(Hd.B) * .62)), FMath::RadiansToDegrees(Hd.Yaw) + 180, Hd.B.Kind == BoatSport ? .22 : .15, FColor(0x10, 0x16, 0x1c));
		UE_LOG(LogTemp, Log, TEXT("KRYard hard %d \"%s\" (%s) kind %d %.1f m at %.1f %.1f%s%s%s"), i, *Hd.B.Name, *Hd.B.Brand, Hd.B.Kind, Hd.B.L, Hd.X, Hd.Y, Hd.B.bSanded ? TEXT(" sanded") : TEXT(""), Hd.B.bWrapped ? TEXT(" wrapped") : TEXT(""), Hd.B.bNoEngines ? TEXT(" outboards off") : TEXT(""));
	}
	UE_LOG(LogTemp, Log, TEXT("KRYard boatyard: %d boats on stands (%d sanded, %d shrink-wrapped, 1 sportfisher with her outboards off), 1 in the travel lift"), Hard.Num(), NSanded, NWrapped);
	// a point beside a boat on the hard: Along her length, Off her side (+ starboard), on the ground
	auto Beside = [&](int32 i, double Along, double Off) { const FHard& Hd = Hard[i]; return FTransform(FQuat(FVector::ZAxisVector, Hd.Yaw), P3(Hd.X, Hd.Y, PadY)).TransformPosition(P3(Along, Off, 0)); };
	auto FacingHull = [&](int32 i, double Off) { return Hard[i].Yaw + (Off > 0 ? -PI / 2 : PI / 2); };

	// ---- the three at work
	{
		Workers.SetNum(3);
		FKeysHumanLook Lk;
		// sanding the centre console's bottom: a dust mask's worth of grey on his shirt
		Lk.Shirt = Hex(0x8d9499); Lk.Bottom = Hex(0x3a4250); Lk.bShortsLong = true; Lk.bShoes = true; Lk.Shoes = Hex(0x2a2d31); Lk.bCap = true; Lk.CapColor = Hex(0xc8302e); Lk.Skin = Hex(0xb9805c);
		Workers[0].Hm.Init(Lk, false);
		Workers[0].Job = 0;
		Workers[0].Spot = Beside(1, -.8, Hard[1].B.B * .46 + .62);
		Workers[0].Hm.Yaw = FacingHull(1, 1);
		// rolling bottom paint onto the bay boat
		Lk.Shirt = Hex(0xf2f2f0); Lk.Bottom = Hex(0x5a6a4a); Lk.bCap = false; Lk.bGlasses = true; Lk.Skin = Hex(0xd9a27f); Lk.Hair = Hex(0x2b1d14);
		Workers[1].Hm.Init(Lk, false);
		Workers[1].Job = 1;
		Workers[1].Spot = Beside(2, .6, -(Hard[2].B.B * .46 + .78));
		Workers[1].Hm.Yaw = FacingHull(2, -1);
		Workers[1].Paint = 0x1f3a5c;
		// pressure washing the one just hauled
		Lk.Shirt = Hex(0xe8b923); Lk.Bottom = Hex(0x2b3540); Lk.bGlasses = false; Lk.bCap = true; Lk.CapColor = Hex(0x1d3557); Lk.Skin = Hex(0x8d5a3c); Lk.Shoes = Hex(0xf2f2f0);
		Workers[2].Hm.Init(Lk, false);
		Workers[2].Job = 2;
		Workers[2].Spot = Beside(6, -1.2, Hard[6].B.B * .5 + 2.3);
		Workers[2].Hm.Yaw = FacingHull(6, 1);
		for (int32 i = 0; i < 3; i++) { Workers[i].Hm.Pos = Workers[i].Spot; Workers[i].Ph = i * 2.1; }
		WasherAt = Beside(6, -3.4, Hard[6].B.B * .5 + 3.2);
		PressureWasher(D, WasherAt, Hard[6].Yaw + .6);
		// what they have round them: buckets by the painter (one open, with a tray's worth spilt), a sanding box and cord
		const FVector Pb = Beside(2, 1.5, -(Hard[2].B.B * .46 + 1.1));
		PaintBucket(D, Pb, 0x1f3a5c, true);
		PaintBucket(D, Pb + FVector(.42, .18, 0), 0x1f3a5c);
		PaintBucket(D, Pb + FVector(.2, .52, 0), 0x8a2a2a);
		Box(D, Pb.X - .5, Pb.Y + .3, PadY + .012, .5, .36, .02, Hex(0x1f3a5c));
		const FVector Sb = Beside(1, -2.0, Hard[1].B.B * .46 + 1.1);
		Box(D, Sb.X, Sb.Y, PadY + .16, .5, .32, .3, Hex(0x2f6fa8));
		Tube(D, Sb + FVector(0, 0, .05), Beside(1, -.9, Hard[1].B.B * .46 + .9) + FVector(0, 0, .03), .012, 0xe4572e, 4);
		// a puddle under the wash-down pad
		const FVector Wd = Beside(6, -1.0, Hard[6].B.B * .5 + .4);
		for (int32 k = 0; k < 6; k++)
		{
			const double A = k * 1.3, R = .9 + .5 * Hash(k, 3);
			const FVector C = Wd + FVector(FMath::Cos(A) * .9, FMath::Sin(A) * .8, .012 + k * .001);
			D.AddQuad(C + FVector(-R, -R * .7, 0), C + FVector(R, -R * .7, 0), C + FVector(R * .8, R * .7, 0), C + FVector(-R * .8, R * .7, 0), Hex(0x6f6a5e), FVector::UpVector);
		}
		// a ladder up to the sportfisher's cockpit and one against the centre console being sanded
		Ladder(D, Beside(0, -3.6, 3.0), Beside(0, -3.6, 2.0) + FVector(0, 0, BlockedHeight(Hard[0].B) + SheerZ(Hard[0].B) + .25));
		Ladder(D, Beside(1, 2.2, -2.3), Beside(1, 2.2, -1.5) + FVector(0, 0, BlockedHeight(Hard[1].B) + SheerZ(Hard[1].B) + .2));
		// the three outboards that came off her, on stands by the transom, one with its cowl off
		OutboardOnStand(D, Beside(0, -8.2, -1.5), .25, 0xf2f2f2);
		OutboardOnStand(D, Beside(0, -8.5, 0), 0, 0xf2f2f2, true);
		OutboardOnStand(D, Beside(0, -8.2, 1.5), -.3, 0xf2f2f2);
		// the skiff for sale: a hand-painted board on posts beside her
		const FVector Fs = Beside(7, .3, 2.1);
		SignBoard(S, Fs, Hard[7].Yaw + PI / 2 + PI, 1.9, 1.2, .7, 0xf4f1e4);
		const double SaleYaw = FMath::RadiansToDegrees(Hard[7].Yaw + PI / 2 + PI);
		const FVector SaleN = FQuat(FVector::ZAxisVector, Hard[7].Yaw + PI / 2 + PI).RotateVector(FVector(1, 0, 0));
		NewText(Site, TEXT("FOR SALE"), Fs + SaleN * .05 + FVector(0, 0, .7 + .85), SaleYaw, .34, FColor(0xc8, 0x10, 0x2e));
		NewText(Site, FString::Printf(TEXT("17' %s SKIFF\n$9,500 OBO\nASK THE DOCKMASTER"), *Hard[7].B.Brand.ToUpper()), Fs + SaleN * .05 + FVector(0, 0, .7 + .4), SaleYaw, .15, FColor(0x15, 0x17, 0x1a));
		// and the same on the back, for whoever comes the other way
		NewText(Site, TEXT("FOR SALE"), Fs - SaleN * .05 + FVector(0, 0, .7 + .85), SaleYaw + 180, .34, FColor(0xc8, 0x10, 0x2e));
		NewText(Site, FString::Printf(TEXT("17' %s SKIFF\n$9,500 OBO\nASK THE DOCKMASTER"), *Hard[7].B.Brand.ToUpper()), Fs - SaleN * .05 + FVector(0, 0, .7 + .4), SaleYaw + 180, .15, FColor(0x15, 0x17, 0x1a));
	}

	// ---- the clutter: stacked jack stands and spare blocks, old outboards, a trailer, drums, the dumpster, pallets
	JackStandStack(D, P3(56.8, -6.2, PadY), 6);
	JackStandStack(D, P3(57.6, -5.0, PadY), 5);
	JackStandStack(D, P3(56.6, -4.2, PadY), 7);
	JackStandStack(D, P3(57.7, -3.3, PadY), 4);
	JackStand(D, P3(55.4, -5.3, PadY), 1.1, .7);
	JackStand(D, P3(55.2, -3.6, PadY), .9, 2.1);
	D.Surf = SurfWood;
	for (int32 k = 0; k < 9; k++) Box(D, 57.2 - (k % 3) * .32, -8.4 + (k / 3 % 2) * .1, PadY + .11 + (k / 3) * .21, .28, 1.0, .2, Hex(k % 2 ? 0x7d6648 : 0x6a5740));
	D.Surf = SurfDefault;
	OutboardOnStand(D, P3(36.5, 20.2, PadY), -PI / 2, 0x2a2d31);
	OutboardOnStand(D, P3(38.3, 20.4, PadY), -PI / 2 + .2, 0xd9dde2, true);
	OutboardOnStand(D, P3(40.2, 20.1, PadY), -PI / 2 - .15, 0x1f4f8c);
	OutboardOnStand(D, P3(42.0, 20.3, PadY), -PI / 2 + .1, 0x15171a);
	Trailer(D, P3(55.5, 9, PadY), PI / 2 + .08, 7.2);
	Drum(D, P3(57.6, 0.4, PadY), 0x2f6fa8);
	Drum(D, P3(57.7, 1.1, PadY), 0x15171a);
	Drum(D, P3(57.0, .8, PadY), 0xc8302e);
	Dumpster(D, P3(57.6, 19.4, PadY), PI / 2);
	Pallet(D, P3(53.5, 20.2, PadY), .1);
	Pallet(D, P3(53.5, 20.2, PadY + .15), -.08);
	for (int32 k = 0; k < 5; k++) PaintBucket(D, P3(53.2 + (k % 3) * .34, 20.0 + (k / 3) * .36, PadY + .3), k % 2 ? 0x8a2a2a : 0x1f3a5c);
	// rolls of shrink wrap on a pallet, a pair of sawhorses with a rub rail across them
	Pallet(D, P3(50.5, 20.3, PadY), 0);
	D.Surf = SurfVinyl;
	for (int32 k = 0; k < 3; k++) D.AddTube(P3(50.1 + k * .4, 19.9, PadY + .15), P3(50.1 + k * .4, 19.9, PadY + 1.55), .16, .16, 10, Hex(0xf3f6f9));
	D.Surf = SurfWood;
	for (double X : {30.0, 32.2})
	{
		Box(D, X, 19.6, PadY + .78, .1, 1.0, .1, Hex(0x9a8463));
		for (int32 a = -1; a <= 1; a += 2) { Tube(D, P3(X - .3, 19.6 + a * .4, PadY), P3(X, 19.6 + a * .4, PadY + .78), .03, 0x9a8463, 4); Tube(D, P3(X + .3, 19.6 + a * .4, PadY), P3(X, 19.6 + a * .4, PadY + .78), .03, 0x9a8463, 4); }
	}
	D.Surf = SurfSteel;
	Tube(D, P3(29.4, 19.6, PadY + .86), P3(32.8, 19.6, PadY + .86), .03, 0xc4c9cd, 6);
	D.Surf = SurfDefault;

	// ---- the yard's shed in the north-east corner, the sign on the water, lights, and a fence along the landward sides
	{
		const double Cx = 55.5, Cy = -27, W = 7, Dp = 4.6, Hh = 3.6;
		S.Surf = SurfTin;
		Box(S, Cx, Cy, PadY + Hh / 2, W, Dp, Hh, Hex(0xb7c0c4));
		S.AddQuad(P3(Cx - W / 2 - .3, Cy - Dp / 2 - .3, PadY + Hh + .7), P3(Cx + W / 2 + .3, Cy - Dp / 2 - .3, PadY + Hh + .7), P3(Cx + W / 2 + .3, Cy + Dp / 2 + .5, PadY + Hh - .05), P3(Cx - W / 2 - .3, Cy + Dp / 2 + .5, PadY + Hh - .05), Hex(0x9aa5ab), FVector(0, .2, 1));
		for (int32 a = -1; a <= 1; a += 2) S.AddTri(P3(Cx + a * W / 2, Cy - Dp / 2, PadY + Hh), P3(Cx + a * W / 2, Cy + Dp / 2, PadY + Hh), P3(Cx + a * W / 2, Cy - Dp / 2, PadY + Hh + .62), Hex(0xb7c0c4), FVector(a, 0, 0));
		Box(S, Cx, Cy - Dp / 2 - .02, PadY + Hh + .32, W, .05, .64, Hex(0xb7c0c4));
		S.Surf = SurfMetalPaint;
		Box(S, Cx - 1.2, Cy + Dp / 2 + .02, PadY + 1.45, 3.0, .05, 2.9, Hex(0x2a5d9c));
		for (int32 k = 1; k < 8; k++) Box(S, Cx - 1.2, Cy + Dp / 2 + .05, PadY + k * .36, 3.0, .02, .03, Hex(0x1f477a));
		S.Surf = SurfGlass;
		Box(S, Cx + 2.3, Cy + Dp / 2 + .02, PadY + 2.0, 1.1, .05, .9, Hex(0x27414f));
		S.Surf = SurfDefault;
		Box(G, Cx + 2.3, Cy + Dp / 2 + .05, PadY + 2.0, 1.0, .02, .8, FLinearColor(1.f, .86f, .55f));
		NewText(Site, TEXT("COWFISH COVE BOATYARD"), P3(Cx, Cy + Dp / 2 + .08, PadY + Hh - .3), 90, .36, FColor(0x15, 0x17, 0x1a));
		LampAt.Add(P3(Cx, Cy + Dp / 2 + 2, PadY + 3.2));
	}
	SignBoard(S, P3(19.4, -5.6, PadY), 0, 6.4, 1.7, 2.6, 0xf4f1e4);
	NewText(Site, TEXT("COWFISH COVE BOATYARD"), P3(19.4 - .06, -5.6, PadY + 2.6 + 1.15), 180, .42, FColor(0x1f, 0x4f, 0x8c));
	NewText(Site, TEXT("HAUL OUT - BOTTOM PAINT - REPOWER"), P3(19.4 - .06, -5.6, PadY + 2.6 + .5), 180, .26, FColor(0x15, 0x17, 0x1a));
	LightPole(S, G, LampAt, 19.6, -28.6, 1, 0);
	LightPole(S, G, LampAt, 36, 21, 0, -1);
	LightPole(S, G, LampAt, 58.8, -14, -1, 0);
	S.Surf = SurfSteel;
	auto Fence = [&](double X0, double Y0, double X1, double Y1)
	{
		const double Len = FMath::Sqrt(FMath::Square(X1 - X0) + FMath::Square(Y1 - Y0));
		const int32 N = FMath::Max(1, FMath::RoundToInt32(Len / 3));
		for (int32 k = 0; k <= N; k++) { const double F = double(k) / N; S.AddTube(P3(FMath::Lerp(X0, X1, F), FMath::Lerp(Y0, Y1, F), PadY), P3(FMath::Lerp(X0, X1, F), FMath::Lerp(Y0, Y1, F), PadY + 1.9), .035, .035, 5, Hex(0x9aa0a4)); }
		for (double Z : {.15, 1.0, 1.85}) S.AddTube(P3(X0, Y0, PadY + Z), P3(X1, Y1, PadY + Z), .018, .018, 4, Hex(0x9aa0a4));
	};
	Fence(59.7, -30, 59.7, 22);
	Fence(59.7, 21.7, 46, 21.7);
	Fence(41, 21.7, 18.3, 21.7);     // the gate is the gap between
	S.Surf = SurfDefault;
}

// ---------------------------------------------------------------- the forklift
void AKeysYards::BuildForklift()
{
	Fork.FaceX = FaceX;
	Fork.AisleX = FaceX + 12.2;
	Fork.ParkY = 9; Fork.ParkYaw = -PI / 2;
	Fork.WellX = Basin.X0 - 1.1; Fork.WellY = WellY;
	Fork.LowZ = -PadY - 1.65;
	Fork.Park();
	LiftBody = NewMesh(TEXT("Forklift"), Site, 1500);
	LiftMast = NewMesh(TEXT("ForkliftMast"), LiftBody, 1500);
	LiftForks = NewMesh(TEXT("ForkliftForks"), LiftBody, 1500);
	LiftDriver = NewMesh(TEXT("ForkliftDriver"), LiftBody, 400);
	FKeysMesh M;
	FKeysForklift::BuildBody(M);
	M.Commit(LiftBody, 0, AKeysWorld::VertexColorMaterial());
	Tris += M.T.Num() / 3;
	M.Reset();
	FKeysForklift::BuildInnerMast(M);
	M.Commit(LiftMast, 0, AKeysWorld::VertexColorMaterial());
	Tris += M.T.Num() / 3;
	M.Reset();
	FKeysForklift::BuildForks(M);
	M.Commit(LiftForks, 0, AKeysWorld::VertexColorMaterial());
	Tris += M.T.Num() / 3;
	FKeysHumanLook Lk;
	Lk.Shirt = Hex(0x2f7d4f); Lk.Bottom = Hex(0x3a4250); Lk.bShortsLong = true; Lk.bCap = true; Lk.CapColor = Hex(0xf2f2f0); Lk.bGlasses = true; Lk.bShoes = true; Lk.Shoes = Hex(0x5a4a38);
	Driver.Init(Lk, true);
	Driver.Pos = FKeysForklift::DriverSeat();
	Driver.bHoldHead = true;
	TickForklift(0, 0, 0);
}

void AKeysYards::TickForklift(double T, double Dt, double CamDist)
{
	if (!LiftBody) return;
	// the water in the well, as the boats ride it
	const double WX = OX + Fork.WellX + 7, WZ = OZ + Fork.WellY;
	const double Amp = KeysGeo::AmpAt(WX, WZ);
	const double FloatZ = KeysGeo::WaveH(WX, WZ, T, Amp) - PadY;
	if (Dt > 0) Fork.Tick(Dt, FloatZ);
	LiftBody->SetRelativeLocationAndRotation(P3(Fork.X, Fork.Y, PadY) * 100.0, FRotator(0, FMath::RadiansToDegrees(Fork.Yaw), 0));
	LiftMast->SetRelativeLocation(FVector(0, 0, Fork.InnerMastZ() * 100));
	LiftForks->SetRelativeLocation(FVector(0, 0, Fork.ForkZ * 100));
	for (int32 i = 0; i < Fork.Boats.Num() && i < LiveBoats.Num(); i++)
	{
		const FKeysForklift::FBoat& B = Fork.Boats[i];
		if (!LiveBoats[i]) continue;
		// afloat she rolls a little on what comes in from the bay
		const FVector2D Sl = KeysGeo::WaveSlope(OX + B.X, OZ + B.Y, T, Amp);
		const double Roll = B.Float * (FMath::Sin(T * .9 + i) * 1.1 + FMath::RadiansToDegrees(FMath::Atan(Sl.Y)) * .6);
		const double Pitch = B.Float * (FMath::Sin(T * .63 + i * 2) * .5 + FMath::RadiansToDegrees(FMath::Atan(Sl.X)) * .6);
		LiveBoats[i]->SetRelativeLocationAndRotation(P3(B.X, B.Y, PadY + B.Z) * 100.0, FRotator(Pitch, FMath::RadiansToDegrees(B.Yaw), Roll));
	}
	if (CamDist < 240)
	{
		// the driver: hands on the wheel, turning it with the machine, looking over his shoulder to back up and up the
		// mast when the forks are high
		const double A = FMath::Clamp(Fork.Turn * 3.2, -1.6, 1.6) + FMath::Sin(T * 1.7) * .04;
		const double Wy = Driver.HipY + .3, Wz = .45;
		Driver.Pose(FKeysHuman::J(-.17 * FMath::Cos(A), Wy - .17 * FMath::Sin(A), Wz), FKeysHuman::J(.17 * FMath::Cos(A), Wy + .17 * FMath::Sin(A), Wz));
		const double WantYaw = Fork.Reversing() ? 1.25 : FMath::Clamp(-Fork.Turn * 1.4, -.7, .7) + FMath::Sin(T * .31) * .12;
		Driver.HeadYaw += (WantYaw - Driver.HeadYaw) * FMath::Min(1.0, Dt * 3 + (Dt <= 0 ? 1 : 0));
		const double WantNod = Fork.ForkZ > 4 ? -.4 : Fork.ForkZ < 0 ? .25 : 0;
		Driver.HeadNod += (WantNod - Driver.HeadNod) * FMath::Min(1.0, Dt * 2 + (Dt <= 0 ? 1 : 0));
		FKeysMesh M;
		Driver.Build(M);
		M.Commit(LiftDriver, 0, AKeysWorld::VertexColorMaterial());
	}
}

// ---------------------------------------------------------------- the yard's workers
void AKeysYards::TickWorkers(double T, double CamDist)
{
	if (!WorkerMesh || !Workers.Num()) return;
	FKeysMesh M, Sp;
	for (FWorker& W : Workers)
	{
		FKeysHuman& Hm = W.Hm;
		const double Ph = T + W.Ph;
		Hm.bHoldHead = true;
		Hm.Pos = W.Spot;
		const double Y0 = Hm.HipY;
		if (W.Job == 0)
		{
			// sanding: both hands on the sander, working it in ovals low on the hull and shuffling along
			const double Sx = FMath::Sin(Ph * .23) * .22;
			const FVector Tool = FKeysHuman::J(Sx + .16 * FMath::Cos(Ph * 5.2), Y0 - .12 + .1 * FMath::Sin(Ph * 5.2) + .06 * FMath::Sin(Ph * .7), .56);
			Hm.Pose(Tool + FKeysHuman::J(-.1, .04, -.03), Tool + FKeysHuman::J(.06, 0, -.02));
			Hm.Pitch = -.24; Hm.HeadNod = .38; Hm.HeadYaw = Sx * .6;
			Hm.Build(M);
			const FTransform Tf = Hm.ToBoat();
			M.Surf = SurfMetalPaint;
			M.AddBox(Tf.TransformPosition(Tool + FKeysHuman::J(0, 0, .06)), FVector(.1, .16, .1), Hex(0xe8b923), Tf.GetRotation());
			M.AddTube(Tf.TransformPosition(Tool + FKeysHuman::J(0, 0, .11)), Tf.TransformPosition(Tool + FKeysHuman::J(0, 0, .13)), .075, .075, 8, Hex(0x8d9499));
			M.Surf = SurfDefault;
		}
		else if (W.Job == 1)
		{
			// painting: long strokes with a roller on a short pole, a dip toward the bucket now and then
			const double St = FMath::Sin(Ph * 1.5), Sx = FMath::Sin(Ph * .19) * .25;
			const FVector R = FKeysHuman::J(.16 + Sx, Y0 + .3 + .16 * St, .42), L = FKeysHuman::J(-.02 + Sx, Y0 + .02 + .14 * St, .3);
			Hm.Pose(L, R);
			Hm.Pitch = -.1; Hm.HeadNod = .22 - .12 * St; Hm.HeadYaw = Sx * .5;
			Hm.Build(M);
			const FTransform Tf = Hm.ToBoat();
			const FVector Lw = Tf.TransformPosition(L), Rw = Tf.TransformPosition(R), Dir = (Rw - Lw).GetSafeNormal(), Tip = Rw + Dir * .34;
			const FVector Side = Tf.GetRotation().RotateVector(FVector(0, 1, 0));
			M.Surf = SurfWood;
			M.AddTube(Lw - Dir * .12, Tip, .016, .016, 5, Hex(0xc9b98f));
			M.Surf = SurfCloth;
			Wheel(M, Tip - Side * .13, Tip + Side * .13, .055, Hex(W.Paint), 8);
			M.Surf = SurfDefault;
		}
		else
		{
			// pressure washing: the lance in both hands, swinging slowly across the hull, a fan of spray off the tip
			const double Sw = FMath::Sin(Ph * .8) * .42;
			Hm.Yaw += 0;
			const FVector R = FKeysHuman::J(.2, Y0 + .1, .2), L = FKeysHuman::J(.02 + Sw * .2, Y0 + .2, .52);
			Hm.Pose(L, R);
			Hm.Pitch = -.06; Hm.HeadNod = .1; Hm.HeadYaw = Sw * .5;
			Hm.Build(M);
			const FTransform Tf = Hm.ToBoat();
			const FVector Lw = Tf.TransformPosition(L), Rw = Tf.TransformPosition(R);
			const FVector Fwd = Tf.GetRotation().RotateVector(FVector(1, 0, 0)), Side = Tf.GetRotation().RotateVector(FVector(0, 1, 0));
			const FVector Aim = (Fwd + Side * Sw + FVector(0, 0, -.12 + .1 * FMath::Sin(Ph * .37))).GetSafeNormal();
			const FVector Tip = Lw + Aim * .72;
			M.Surf = SurfSteel;
			M.AddTube(Rw - Aim * .1, Tip, .014, .012, 5, Hex(0x2a2d31));
			M.AddBox((Rw + Lw) * .5 - Aim * .1, FVector(.2, .05, .09), Hex(0xc8302e), Tf.GetRotation());
			// the hose back to the machine, sagging onto the gravel
			M.Surf = SurfRubber;
			const FVector H0 = Rw - Aim * .14, H1 = FVector(H0.X, H0.Y, PadY + .03) - Fwd * .5, H2 = WasherAt + FVector(0, 0, .25);
			M.AddTube(H0, H1, .012, .012, 4, Hex(0x1c1c1e));
			M.AddTube(H1, FVector(H2.X, H2.Y, PadY + .03), .012, .012, 4, Hex(0x1c1c1e));
			M.AddTube(FVector(H2.X, H2.Y, PadY + .03), H2, .012, .012, 4, Hex(0x1c1c1e));
			M.Surf = SurfDefault;
			const double Reach = 1.75, Fl = .8 + .2 * FMath::Sin(T * 23);
			const FVector Hit = Tip + Aim * Reach, Up = FVector::CrossProduct(Aim, Side).GetSafeNormal();
			const FLinearColor Wc(1.f, 1.f, 1.f);
			Sp.AddGlowFan(Tip, Hit - Side * .34, Hit + Side * .34, Wc, .85 * Fl, .12);
			Sp.AddGlowFan(Tip, Hit - Up * .1, Hit + Up * .1, Wc, .85 * Fl, .12);
			Sp.AddGlowBlob(Hit, .42, FLinearColor(1.f, 1.f, 1.f, float(.34 * Fl)));
			Sp.AddGlowBlob(Hit - FVector(0, 0, .35), .3, FLinearColor(1.f, 1.f, 1.f, float(.2 * Fl)));
		}
	}
	M.Commit(WorkerMesh, 0, AKeysWorld::VertexColorMaterial());
	if (SprayMesh && SprayMID) Sp.Commit(SprayMesh, 0, SprayMID);
}

// ---------------------------------------------------------------- start-up and the frame
void AKeysYards::BeginPlay()
{
	Super::BeginPlay();
	KeysGeo::Init();
	if (FParse::Param(FCommandLine::Get(), TEXT("KRYardSurvey"))) Survey();
	if (!Origin(OX, OZ)) { UE_LOG(LogTemp, Log, TEXT("KRYard: no yards on this map")); return; }
	Site = NewObject<USceneComponent>(this, TEXT("Site"));
	Site->SetupAttachment(Root);
	Site->RegisterComponent();
	Site->SetWorldLocation(KeysGeo::ToUE(OX, 0, OZ));

	FKeysMesh S, Bt, D, G;
	BuildGround(S, D);
	BuildDryStack(S, Bt, D, G);
	BuildBoatyard(S, Bt, D, G);
	UMaterialInterface* Mat = AKeysWorld::VertexColorMaterial();
	// the buildings, the made ground and the lift from across the bay; the boats from a mile; the small stuff close to
	S.Commit(NewMesh(TEXT("YardStructure"), Site, 3500), 0, Mat);
	Bt.Commit(NewMesh(TEXT("YardBoats"), Site, 1500), 0, Mat);
	D.Commit(NewMesh(TEXT("YardDetail"), Site, 450), 0, Mat);
	NightMesh = NewMesh(TEXT("YardLamps"), Site, 3500, false);
	UMaterialInterface* GlowMat = LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/EngineDebugMaterials/VertexColorMaterial.VertexColorMaterial"));
	G.Commit(NightMesh, 0, GlowMat ? GlowMat : Mat);
	NightMesh->SetVisibility(false);
	Tris += (S.T.Num() + Bt.T.Num() + D.T.Num() + G.T.Num()) / 3;
	{
		int32 Bad = 0;
		for (const FVector& Vv : Bt.V) Bad += Vv.ContainsNaN();
		if (Bad) UE_LOG(LogTemp, Warning, TEXT("KRYard: %d broken vertices in the boats"), Bad);
	}
	UE_LOG(LogTemp, Log, TEXT("KRYard tris: structure %d, boats %d, detail %d, lamps %d"), S.T.Num() / 3, Bt.T.Num() / 3, D.T.Num() / 3, G.T.Num() / 3);
	for (int32 i = 0; i < LampAt.Num() && i < 8; i++)
	{
		UPointLightComponent* Lt = NewObject<UPointLightComponent>(this);
		Lt->SetupAttachment(Site);
		Lt->RegisterComponent();
		Lt->SetRelativeLocation(LampAt[i] * 100.0);
		Lt->SetIntensityUnits(ELightUnits::Lumens);
		Lt->SetIntensity(1800.f);
		Lt->SetLightColor(FLinearColor(1.f, .88f, .66f));
		Lt->SetAttenuationRadius(2600.f);
		Lt->SetCastShadows(false);
		Lt->SetVisibility(false);
		Lamps.Add(Lt);
	}
	BuildForklift();
	WorkerMesh = NewMesh(TEXT("YardWorkers"), Site, 400);
	SprayMesh = NewMesh(TEXT("YardSpray"), Site, 400, false);
	if (UMaterialInterface* GM = AKeysWorld::GlowMaterial())
	{
		SprayMID = UMaterialInstanceDynamic::Create(GM, this);
		SprayMID->SetScalarParameterValue(TEXT("Bright"), 1.1f);
	}
	TickWorkers(0, 0);
	bBuilt = true;
	const TArray<KeysYards::FSite>& Ss = KeysYards::Sites();
	for (const KeysYards::FSite& St : Ss) UE_LOG(LogTemp, Log, TEXT("KRYard site %s at %.0f, %.0f - by boat tp=%.0f,%.0f,%.2f"), *St.Name, St.X, St.Z, St.BoatX, St.BoatZ, St.BoatHeading);
	// what the ground is like under it (the pads want made ground, the basin wants water over the forklift's forks)
	double LandMin = 1e9, LandMax = -1e9, DepthMin = 1e9;
	for (const FYRect& R : Pads) for (double X = R.X0; X <= R.X1; X += 3) for (double Y = R.Y0; Y <= R.Y1; Y += 3) { const double Hh = KeysGeo::LandH(OX + X, OZ + Y); LandMin = FMath::Min(LandMin, Hh); LandMax = FMath::Max(LandMax, Hh); }
	for (double X = Basin.X0 + 1; X <= Basin.X1 - 1; X += 2) for (double Y = Basin.Y0; Y <= Basin.Y1 - 1; Y += 2) DepthMin = FMath::Min(DepthMin, KeysGeo::DepthAt(OX + X, OZ + Y));
	UE_LOG(LogTemp, Log, TEXT("KRYard built: %d tris in %d meshes, %d signs, %d lamps; ground under the pads %.2f .. %.2f m (pad top %.2f), least water in the basin %.2f m"), Tris, Meshes.Num(), Texts.Num(), Lamps.Num(), LandMin, LandMax, PadY, DepthMin);
}

void AKeysYards::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!bBuilt) return;
	FVector Cam = FVector::ZeroVector;
	if (APlayerCameraManager* CM = UGameplayStatics::GetPlayerCameraManager(this, 0))
	{
		const FVector L = CM->GetCameraLocation();
		Cam = FVector(L.X / 100, L.Z / 100, L.Y / 100);
	}
	const double Dist = FMath::Sqrt(FMath::Square(Cam.X - OX) + FMath::Square(Cam.Z - (OZ - 8)));
	// nothing here moves, and nothing is rebuilt, while nobody is within 600 m
	bAwake = Dist < 600;
	if (!bAwake) return;
	const double Dt = FMath::Min(double(DeltaSeconds), .1), T = GetWorld()->GetTimeSeconds();
	TickForklift(T, Dt, Dist);
	// the figures at work: every frame close to, a few times a second from across the cove
	WorkerT -= Dt;
	if (Dist < 260 && (Dist < 110 || WorkerT <= 0)) { WorkerT = .12; TickWorkers(T, Dist); }
	SlowT -= Dt;
	if (SlowT <= 0)
	{
		SlowT = .5;
		const bool bText = Dist < 320;
		if (bText != bTextsShown) { bTextsShown = bText; for (UTextRenderComponent* Tx : Texts) if (Tx) Tx->SetVisibility(bText); }
		const bool bDark = AKeysWorld::Dusk(this) > .3;
		if (bDark != bNight)
		{
			bNight = bDark;
			NightMesh->SetVisibility(bDark);
			for (UPointLightComponent* Lt : Lamps) if (Lt) Lt->SetVisibility(bDark);
		}
	}
	if (bLog)
	{
		LogT -= Dt;
		if (LogT <= 0) { LogT = .5; UE_LOG(LogTemp, Log, TEXT("KRYardLift t %.1f %s"), T, *Fork.Describe()); }
	}
}

FString AKeysYards::DebugCmd(const FString& Cmd)
{
	if (!bBuilt) return TEXT("no yards on this map");
	if (Cmd == TEXT("cycle"))
	{
		// put everything back and start a launch now
		for (FKeysForklift::FBoat& B : Fork.Boats) B.State = 0;
		Fork.Afloat = -1;
		Fork.Park();
		Fork.StartLaunch(0);
		return TEXT("launching boat 0: ") + Fork.Describe();
	}
	if (Cmd == TEXT("fetch")) { if (Fork.Job == 0 && Fork.Afloat >= 0) Fork.StartFetch(Fork.Afloat); return Fork.Describe(); }
	if (Cmd.StartsWith(TEXT("speed="))) { Fork.Rate = FMath::Clamp(FCString::Atod(*Cmd.Mid(6)), .1, 8.0); return FString::Printf(TEXT("rate %.1f"), Fork.Rate); }
	if (Cmd == TEXT("log")) { bLog = !bLog; LogT = 0; return bLog ? TEXT("logging the forklift") : TEXT("log off"); }
	if (Cmd.StartsWith(TEXT("eye=")))
	{
		APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0);
		if (!PC) return TEXT("no player");
		if (Cmd == TEXT("eye=off")) { PC->SetViewTarget(PC->GetPawn()); return TEXT("eye off"); }
		TArray<FString> Pp;
		Cmd.Mid(4).ParseIntoArray(Pp, TEXT(","));
		if (Pp.Num() < 3) return TEXT("eye=x,y,yawdeg[,pitchdeg[,height]]");
		if (!EyeCam)
		{
			EyeCam = GetWorld()->SpawnActor<ACameraActor>();
			if (EyeCam && EyeCam->GetCameraComponent()) { EyeCam->GetCameraComponent()->SetConstraintAspectRatio(false); EyeCam->GetCameraComponent()->SetFieldOfView(78.f); }
		}
		if (!EyeCam) return TEXT("no camera");
		const double Hh = Pp.Num() >= 5 ? FCString::Atod(*Pp[4]) : 1.7;
		const FVector At = Site->GetComponentLocation() + P3(FCString::Atod(*Pp[0]), FCString::Atod(*Pp[1]), PadY + Hh) * 100.0;
		EyeCam->SetActorLocationAndRotation(At, FRotator(Pp.Num() >= 4 ? FCString::Atod(*Pp[3]) : 0, FCString::Atod(*Pp[2]), 0));
		PC->SetViewTarget(EyeCam);
		return FString::Printf(TEXT("eye at %s"), *Cmd.Mid(4));
	}
	if (Cmd == TEXT("where"))
	{
		FString R;
		for (const KeysYards::FSite& St : KeysYards::Sites()) R += FString::Printf(TEXT("%s tp=%.0f,%.0f,%.2f; "), *St.Name, St.BoatX, St.BoatZ, St.BoatHeading);
		return R;
	}
	return FString::Printf(TEXT("awake %d night %d tris %d meshes %d texts %d | %s"), bAwake ? 1 : 0, bNight ? 1 : 0, Tris, Meshes.Num(), Texts.Num(), *Fork.Describe());
}

// -KRYardSurvey: a character map of the ground round the marinas (4 m to a character), for picking the site
void AKeysYards::Survey()
{
	using namespace KeysGeo;
	for (const FMarina& M : Marinas()) UE_LOG(LogTemp, Log, TEXT("KRYard marina %s sx %.0f sz %.0f dir %d chain %.0f"), *M.Name, M.SX, M.SZ, M.Dir, ChainZ(M.SX));
	const double X0 = -780, X1 = -140, St = 4;
	double ZMin = 1e9, ZMax = -1e9;
	for (double X = X0; X <= X1; X += 40) { ZMin = FMath::Min(ZMin, ChainZ(X)); ZMax = FMath::Max(ZMax, ChainZ(X)); }
	ZMin = FMath::FloorToDouble((ZMin - 330) / St) * St; ZMax += 330;
	UE_LOG(LogTemp, Log, TEXT("KRYard map x %.0f..%.0f step %.0f, rows z from %.0f"), X0, X1, St, ZMin);
	for (double Z = ZMin; Z <= ZMax; Z += St)
	{
		FString Row;
		for (double X = X0; X <= X1; X += St)
		{
			const double H = LandH(X, Z);
			TCHAR C;
			if (KeysYards::InFootprint(X, Z)) C = 'Y';
			else if (H > 0.05)
			{
				if (FMath::Abs(Z - ChainZ(X)) < 9) C = 'r';
				else if (KeysTown::InFootprint(X, Z, 0)) C = '#';
				else if (OnMangrove(X, Z)) C = 'M';
				else if (AKeysEstates::PlatParcelAt(X, Z) >= 0) C = 'E';
				else if (KeysTown::NearEstate(X, Z, 30)) C = 'e';
				else if (SurfaceKind(X, Z) != 0) C = 'k';
				else C = TCHAR('0' + FMath::Clamp(int32(H * 2), 0, 9));
			}
			else
			{
				const double D = DepthAt(X, Z);
				C = MangroveNear(X, Z) > .5 ? 'm' : D < .5 ? ':' : D < 1.0 ? '.' : D < 1.6 ? '-' : D < 2.5 ? '~' : ' ';
			}
			Row.AppendChar(C);
		}
		UE_LOG(LogTemp, Log, TEXT("KRYmap %6.0f |%s|"), Z, *Row);
	}
}
