#include "KeysDiver.h"
#include "KeysReefDen.h"
#include "KeysExtras.h"
#include "KeysSchools.h"
#include "KeysBoatPawn.h"
#include "KeysGeo.h"
#include "KeysSea.h"
#include "KeysMesh.h"
#include "KeysHuman.h"
#include "KeysLife.h"
#include "KeysWorld.h"
#include "KeysData.h"
#include "KeysAudio.h"
#include "KeysScubaSpeargun.h"
#include "ProceduralMeshComponent.h"
#include "Components/PostProcessComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/SpotLightComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/Material.h"
#include "MaterialDomain.h"
#include "Kismet/GameplayStatics.h"
#include "Camera/PlayerCameraManager.h"
#include "GameFramework/PlayerController.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "SceneView.h"
#include "SceneViewExtension.h"

using namespace KeysGeo;
using namespace KeysDive;

namespace KeysDiverEye
{
	// What the renderer's auto exposure last settled on for the game view, read on the game thread as each frame's view
	// is set up: the exposure scale and the exposure bias in force, so the white point it sits at can be put in EV100
	// (the units of AutoExposureMin/MaxBrightness): log2(2^bias / exposure).
	class FProbe : public FSceneViewExtensionBase
	{
	public:
		FProbe(const FAutoRegister& R) : FSceneViewExtensionBase(R) {}
		virtual void SetupView(FSceneViewFamily& Family, FSceneView& View) override
		{
			if (View.bIsSceneCapture || View.bIsReflectionCapture || View.bIsPlanarReflection) return;
			const float E = View.GetLastEyeAdaptationExposure();
			if (E <= 0) return;
			Exposure = E;
			Lum = View.GetLastAverageSceneLuminance();
			Bias = View.FinalPostProcessSettings.AutoExposureBias;
			Frame = GFrameCounter;
		}
		double WhiteEV() const { return Exposure > 0 ? Bias - FMath::Log2(Exposure) : 99; }
		float Exposure = 0, Lum = 0, Bias = 0;
		uint64 Frame = 0;
	};
	static TSharedPtr<FProbe, ESPMode::ThreadSafe> GProbe;
	static FProbe* Get()
	{
		if (!GProbe.IsValid()) GProbe = FSceneViewExtensions::NewExtension<FProbe>();
		return GProbe.Get();
	}
}

namespace
{
	double DvClamp(double V, double A, double B) { return V < A ? A : V > B ? B : V; }
	double DvLerp(double A, double B, double T) { return A + (B - A) * T; }
	double DvRand(double A, double B) { return FMath::FRandRange(A, B); }
	FLinearColor Hex(uint32 H) { return FLinearColor::FromSRGBColor(FColor(uint8(H >> 16), uint8(H >> 8), uint8(H))); }

	// game meters (x, y up, z) <-> Unreal centimeters
	FVector G2U(const FVector& G) { return FVector(G.X * 100, G.Z * 100, G.Y * 100); }
	FVector U2G(const FVector& U) { return FVector(U.X / 100, U.Z / 100, U.Y / 100); }
	FVector GDir2U(const FVector& G) { return FVector(G.X, G.Z, G.Y); }

	// the boat's yaw convention (forward = -sin, -cos in x/z) as an Unreal yaw in degrees
	double YawDeg(double Yaw) { return FMath::RadiansToDegrees(FMath::Atan2(-FMath::Cos(Yaw), -FMath::Sin(Yaw))); }

	UMaterialInterface* LoadMatOr(const TCHAR* Path)
	{
		UMaterialInterface* M = LoadObject<UMaterialInterface>(nullptr, Path);
		return M ? M : AKeysWorld::VertexColorMaterial();
	}

	// ---- smooth-shaded primitives with the winding Unreal wants (front face: Dot(Cross(B-A, C-A), N) < 0)
	void Tri(FKeysMesh& M, const FVector& A, const FVector& B, const FVector& C, const FVector& NA, const FVector& NB, const FVector& NC, const FLinearColor& Col)
	{
		const FVector Avg = NA + NB + NC;
		const bool bFlip = FVector::DotProduct(FVector::CrossProduct(B - A, C - A), Avg) > 0;
		const int32 I0 = M.AddVert(A, NA, Col);
		const int32 I1 = M.AddVert(bFlip ? C : B, bFlip ? NC : NB, Col);
		const int32 I2 = M.AddVert(bFlip ? B : C, bFlip ? NB : NC, Col);
		M.T.Append({I0, I1, I2});
	}

	// ellipsoid at C with radii R (meters, local frame), Seg around and Seg/2 up
	void Ellipsoid(FKeysMesh& M, const FVector& C, const FVector& R, int32 Seg, const FLinearColor& Col, const FQuat& Q = FQuat::Identity)
	{
		const int32 Rings = FMath::Max(3, Seg / 2);
		auto P = [&](int32 i, int32 j, FVector& OutP, FVector& OutN)
		{
			const double Th = PI * j / Rings, Ph = 2 * PI * i / Seg;
			const FVector U(FMath::Sin(Th) * FMath::Cos(Ph), FMath::Sin(Th) * FMath::Sin(Ph), FMath::Cos(Th));
			OutP = C + Q.RotateVector(U * R);
			OutN = Q.RotateVector(FVector(U.X / R.X, U.Y / R.Y, U.Z / R.Z)).GetSafeNormal();
		};
		for (int32 j = 0; j < Rings; j++)
			for (int32 i = 0; i < Seg; i++)
			{
				FVector A, B, Cc, D, NA, NB, NC, ND;
				P(i, j, A, NA); P(i + 1, j, B, NB); P(i + 1, j + 1, Cc, NC); P(i, j + 1, D, ND);
				if (j > 0) Tri(M, A, B, Cc, NA, NB, NC, Col);
				if (j < Rings - 1) Tri(M, A, Cc, D, NA, NC, ND, Col);
			}
	}

	// smooth tube A -> B with end radii
	void Tube(FKeysMesh& M, const FVector& A, const FVector& B, double RA, double RB, int32 Sides, const FLinearColor& Col, bool bCaps = false)
	{
		const FVector Dir = (B - A).GetSafeNormal();
		if (Dir.IsNearlyZero()) return;
		FVector U = FVector::CrossProduct(Dir, FVector::ZAxisVector);
		if (U.SizeSquared() < 1e-6) U = FVector::CrossProduct(Dir, FVector::YAxisVector);
		U.Normalize();
		const FVector W = FVector::CrossProduct(Dir, U);
		const double Slope = (RA - RB) / FMath::Max(1e-6, (B - A).Size());
		for (int32 i = 0; i < Sides; i++)
		{
			const double A0 = 2 * PI * i / Sides, A1 = 2 * PI * (i + 1) / Sides;
			const FVector O0 = U * FMath::Cos(A0) + W * FMath::Sin(A0), O1 = U * FMath::Cos(A1) + W * FMath::Sin(A1);
			const FVector N0 = (O0 + Dir * Slope).GetSafeNormal(), N1 = (O1 + Dir * Slope).GetSafeNormal();
			Tri(M, A + O0 * RA, B + O0 * RB, B + O1 * RB, N0, N0, N1, Col);
			Tri(M, A + O0 * RA, B + O1 * RB, A + O1 * RA, N0, N1, N1, Col);
			if (bCaps)
			{
				Tri(M, A, A + O1 * RA, A + O0 * RA, -Dir, -Dir, -Dir, Col);
				Tri(M, B, B + O0 * RB, B + O1 * RB, Dir, Dir, Dir, Col);
			}
		}
	}

	void Capsule(FKeysMesh& M, const FVector& A, const FVector& B, double R, int32 Sides, const FLinearColor& Col)
	{
		Tube(M, A, B, R, R, Sides, Col);
		Ellipsoid(M, A, FVector(R), Sides, Col);
		Ellipsoid(M, B, FVector(R), Sides, Col);
	}

	UProceduralMeshComponent* MakePMC(AActor* Owner, USceneComponent* Parent, const TCHAR* Name)
	{
		UProceduralMeshComponent* C = Owner->CreateDefaultSubobject<UProceduralMeshComponent>(Name);
		C->SetupAttachment(Parent);
		C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		return C;
	}

	// the gun (gun-viewmodel.ts): a 78 cm roller gun, in its own frame (meters, +X out the muzzle, Z up), rebuilt on the
	// user's reference picture. Its meshes and their sizes are in KeysScubaSpeargun.h; the animation is in UpdateGun.
	using KeysSpeargun::BarrelLen; using KeysSpeargun::BandR; using KeysSpeargun::RailY;
	using KeysSpeargun::ShaftPoke; using KeysSpeargun::BandAnchorX; using KeysSpeargun::BandShort; using KeysSpeargun::BandLong;
	const FVector ReadyPos(.6, .2, -.14), LoadPos(.5, .2, -.3);   // camera frame, meters (forward, right, up)

	// view, camera-frame helper: a camera-relative offset (forward, right, up) in game meters
	FVector CamOffset(const FRotator& Rot, const FVector& Off)
	{
		return U2G(Rot.RotateVector(Off * 100));
	}
}

// the gloved hand closed round a grip (KeysBoatPawn.cpp)
void AddGripHand(FKeysMesh& M, const FVector& C, const FVector& A0, const FVector& N0, double R, double Curl, double IndexLift, bool bMirror,
	const FVector& Elbow, const FLinearColor& Skin, const FLinearColor& Sleeve, const FLinearColor& Nail);

AKeysDiver* AKeysDiver::Get(const UObject* WorldContext)
{
	UWorld* W = WorldContext ? WorldContext->GetWorld() : nullptr;
	if (!W) return nullptr;
	TActorIterator<AKeysDiver> It(W);
	return It ? *It : nullptr;
}

AKeysDiver::AKeysDiver()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickGroup = TG_PostPhysics;   // after the pawn has read the keyboard
	PrimaryActorTick.bTickEvenWhenPaused = false;
	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent = Root;
	Body = MakePMC(this, Root, TEXT("Body"));
	FinL = MakePMC(this, Body, TEXT("FinL"));
	FinR = MakePMC(this, Body, TEXT("FinR"));
	Kit = MakePMC(this, Body, TEXT("ScubaKit"));
	Gauge = MakePMC(this, Root, TEXT("Gauge"));
	GaugeFace = MakePMC(this, Gauge, TEXT("GaugeFace"));
	Gauge->SetCastShadow(false);
	GaugeFace->SetCastShadow(false);
	Gauge->LightingChannels.bChannel1 = true;       // the fill light that shows a speared fish also reads the gauge
	GaugeFace->LightingChannels.bChannel1 = true;
	BoatRack = MakePMC(this, Root, TEXT("BoatRack"));
	BoatFlag = MakePMC(this, Root, TEXT("BoatFlag"));
	Torch = CreateDefaultSubobject<USpotLightComponent>(TEXT("Torch"));
	Torch->SetupAttachment(Root);
	Torch->SetUsingAbsoluteLocation(true);
	Torch->SetUsingAbsoluteRotation(true);
	Torch->SetIntensityUnits(ELightUnits::Candelas);
	Torch->SetCastShadows(false);
	Torch->SetInnerConeAngle(9.f);
	Torch->SetOuterConeAngle(21.f);
	Torch->SetAttenuationRadius(2400.f);
	Torch->SetLightColor(FLinearColor(1.f, .96f, .86f));
	Torch->SetVisibility(false);
	GunBase = MakePMC(this, Root, TEXT("GunBase"));
	GunBandL = MakePMC(this, GunBase, TEXT("GunBandL"));
	GunBandR = MakePMC(this, GunBase, TEXT("GunBandR"));
	GunReel = MakePMC(this, GunBase, TEXT("GunReel"));
	GunShaft = MakePMC(this, GunBase, TEXT("GunShaft"));
	GunGlove = MakePMC(this, GunBase, TEXT("GunGlove"));
	GunHandL = MakePMC(this, GunBase, TEXT("GunHandL"));
	GunWish = MakePMC(this, GunBase, TEXT("GunWish"));
	GunLeader = MakePMC(this, GunBase, TEXT("GunLeader"));
	// the gun is matt black and held in her own shadow: a small fill from over the left shoulder, on a lighting channel
	// of its own so it touches the gun and nothing on the reef
	GunFill = CreateDefaultSubobject<UPointLightComponent>(TEXT("GunFill"));
	GunFill->SetupAttachment(Root);
	GunFill->SetUsingAbsoluteLocation(true);
	GunFill->SetIntensityUnits(ELightUnits::Lumens);
	GunFill->SetCastShadows(false);
	GunFill->SetSourceRadius(20.f);
	GunFill->SetSoftSourceRadius(40.f);
	GunFill->SetAttenuationRadius(420.f);
	GunFill->SetLightColor(FLinearColor(.9f, .97f, 1.f));
	GunFill->LightingChannels.bChannel0 = false;
	GunFill->LightingChannels.bChannel2 = true;
	GunFill->SetVisibility(false);
	GunWorn = MakePMC(this, Body, TEXT("GunWorn"));
	Lines = MakePMC(this, Root, TEXT("Lines"));
	FishOnLine = MakePMC(this, Root, TEXT("FishOnLine"));
	FishOnLine->LightingChannels.bChannel1 = true;
	// under water the sun barely reaches a fish held up at the mask (and the diver's own body shades it), so it read as a
	// black cut-out on the spear and on the catch card: a soft fill from over the shoulder, on a lighting channel of its own
	// so it lights the fish and nothing else on the reef
	FishFill = CreateDefaultSubobject<UPointLightComponent>(TEXT("FishFill"));
	FishFill->SetupAttachment(Root);
	FishFill->SetUsingAbsoluteLocation(true);
	FishFill->SetIntensityUnits(ELightUnits::Lumens);
	FishFill->SetCastShadows(false);
	FishFill->SetSourceRadius(30.f);
	FishFill->SetSoftSourceRadius(50.f);
	FishFill->SetLightColor(FLinearColor(1.f, .97f, .92f));
	FishFill->LightingChannels.bChannel0 = false;
	FishFill->LightingChannels.bChannel1 = true;
	FishFill->SetVisibility(false);
	Particles = MakePMC(this, Root, TEXT("Particles"));
	Particles->SetCastShadow(false);
	Particles->bUseAsyncCooking = false;
	Lines->SetCastShadow(false);
	Post = CreateDefaultSubobject<UPostProcessComponent>(TEXT("Underwater"));
	Post->SetupAttachment(Root);
	Post->bUnbound = true;
	Post->Priority = 10;
}

void AKeysDiver::BeginPlay()
{
	Super::BeginPlay();
	UMaterialInterface* VC = AKeysWorld::VertexColorMaterial();
	// ---- the diver (model.ts): wetsuit capsule, head, mask, back plate, arms and kicking fins. +X is forward.
	{
		FKeysMesh M;
		const FLinearColor Suit = Hex(0x1b2430), Skin = Hex(0xc98a63), MaskC = Hex(0x0a0a0a), Tank = Hex(0x8a8f94);
		Capsule(M, FVector(-.45, 0, 0), FVector(.45, 0, 0), .22, 10, Suit);
		Ellipsoid(M, FVector(.62, 0, .04), FVector(.15), 10, Skin);
		M.AddBox(FVector(.73, 0, .06), FVector(.05, .2, .09), MaskC);
		Capsule(M, FVector(-.3, 0, .18), FVector(.2, 0, .18), .1, 8, Tank);
		for (int32 Side = -1; Side <= 1; Side += 2)
		{
			const FVector Ctr(.25, Side * .3, .02), Dir(0, Side * .976, .217);
			Capsule(M, Ctr - Dir * .275, Ctr + Dir * .275, .06, 6, Suit);
		}
		M.Commit(Body, 0, VC);
		FKeysMesh F;
		// a flat four-sided cone from the pivot back to the tip
		Tube(F, FVector(0, 0, 0), FVector(-.6, 0, 0), .13, .005, 4, Hex(0x0f766e), true);
		F.Commit(FinL, 0, VC);
		F.Commit(FinR, 0, VC);
		FinL->SetRelativeLocation(FVector(-55, -11, 0));
		FinR->SetRelativeLocation(FVector(-55, 11, 0));
		Body->SetVisibility(false, true);
	}
	// ---- the first-person speargun and the gloved hands holding it
	{
		const FLinearColor Glove = Hex(0x1c1e21), Suit = Hex(0x2a2e2c), Grey = Hex(0x55595e);
		FKeysMesh M;
		KeysSpeargun::BuildGun(M, false);
		{
			// the reel's turning part on its own component, built about the axle (local Y), so it can spin as line pays out
			FKeysMesh Rl;
			KeysSpeargun::BuildReel(Rl);
			Rl.Commit(GunReel, 0, VC);
			GunReel->SetRelativeLocation(KeysSpeargun::ReelAt() * 100);
		}
		M.Commit(GunBase, 0, VC);
		// the hands, black gloves and a dark wetsuit: the right round the pistol grip, finger by the trigger (a section
		// of its own, so the gun can be shown without it)
		{
			FKeysMesh Hr;
			const FVector GripTop = KeysSpeargun::GripTop(), GripBot = KeysSpeargun::GripBot();
			AddGripHand(Hr, FMath::Lerp(GripBot, GripTop, .5) + FVector(-.004, .002, 0), GripTop - GripBot, FVector(-.6, .8, 0), .017, .95, .25, true,
				FVector(-.42, .2, -.42), Glove, Suit, Glove);
			Hr.SetSurfFrom(0, SurfRubber);
			Hr.Commit(GunBase, 1, VC);
		}
		// the left hand, under the barrel ahead of the reel, back of the hand toward you (hidden while loading)
		FKeysMesh L;
		AddGripHand(L, FVector(.17, 0, -.002), FVector::XAxisVector, FVector(0, -.5, -.85), .018, .9, 0, true, FVector(-.2, -.42, -.42), Glove, Suit, Glove);
		{
			// a dive computer on the wrist, just up the forearm from the hand
			const FVector Hc(.17, 0, -.002), El(-.2, -.42, -.42);
			const FVector Dw = (El - Hc).GetSafeNormal(), Wp = Hc + Dw * .155 + FVector(0, -.012, -.02);
			Tube(L, Wp - Dw * .011, Wp + Dw * .011, .037, .037, 14, Hex(0x0e0f10));
			L.AddRoundBox(Wp + FVector(-.006, -.03, .008), FVector(.032, .014, .032), .006, Hex(0x2b2e31), FRotationMatrix::MakeFromXZ(Dw, FVector(0, -1, .3)).ToQuat(), 2);
			L.AddRoundBox(Wp + FVector(-.006, -.0375, .009), FVector(.024, .002, .024), .004, Hex(0x3c5560), FRotationMatrix::MakeFromXZ(Dw, FVector(0, -1, .3)).ToQuat(), 1);
		}
		L.SetSurfFrom(0, SurfRubber);
		L.Commit(GunHandL, 0, VC);
		// a unit pair of bands along +X for each side, scaled per frame
		for (int32 Side = -1; Side <= 1; Side += 2)
		{
			FKeysMesh B;
			KeysSpeargun::BuildBandPair(B, Side);
			B.Commit(Side < 0 ? GunBandL : GunBandR, 0, VC);
		}
		FKeysMesh Wb;
		KeysSpeargun::BuildWishbone(Wb);
		Wb.Commit(GunWish, 0, VC);
		FKeysMesh Sh;
		KeysSpeargun::BuildShaft(Sh, false);
		Sh.Commit(GunShaft, 0, VC);
		// the same gun, plainer, in her right hand in the chase view: the gun and its bands, and the shaft with its
		// leader as a second section that goes when the shaft does
		{
			FKeysMesh Cg, Cs;
			KeysSpeargun::BuildCarried(Cg, Cs);
			Cg.Commit(GunWorn, 0, VC);
			Cs.Commit(GunWorn, 1, VC);
			// the grip in the hand at the end of her right arm, the barrel along her line of travel
			GunWorn->SetRelativeLocation(FVector(.34, .568, .16) * 100);
			GunWorn->SetVisibility(false);
		}
		// the loading hand: a glove closed round the wishbone, pulling it back along the gun
		FKeysMesh Gl;
		Gl.Surf = SurfSteel;
		Tube(Gl, FVector(0, -.03, 0), FVector(0, .03, 0), .003, .003, 6, Grey);
		AddGripHand(Gl, FVector::ZeroVector, FVector::YAxisVector, FVector(-1, 0, .4), .008, 1, 0, false, FVector(-.38, -.12, -.2), Glove, Suit, Glove);
		Gl.SetSurfFrom(0, SurfRubber);
		Gl.Commit(GunGlove, 0, VC);
		GunBase->SetVisibility(false, true);
		GunBase->SetCastShadow(false);
		GunBandL->SetCastShadow(false); GunBandR->SetCastShadow(false); GunShaft->SetCastShadow(false); GunGlove->SetCastShadow(false); GunHandL->SetCastShadow(false); GunWish->SetCastShadow(false); GunReel->SetCastShadow(false);
		GunLeader->SetCastShadow(false);
		for (UProceduralMeshComponent* Gc : {GunBase.Get(), GunBandL.Get(), GunBandR.Get(), GunShaft.Get(), GunGlove.Get(), GunHandL.Get(), GunWish.Get(), GunReel.Get(), GunLeader.Get()})
		{
			Gc->LightingChannels.bChannel2 = true;
			Gc->MarkRenderStateDirty();
		}
	}
	// ---- the underwater look: extinction, visibility, lens wetting (world/underwater)
	if (UMaterialInterface* UW = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/KeysRun/Materials/M_KR_Underwater.M_KR_Underwater")))
	{
		PostMID = UMaterialInstanceDynamic::Create(UW, this);
		Post->Settings.AddBlendable(PostMID, 1.f);
		// under water the scene is mostly dim haze, and auto exposure would blow out the lit sand and coral up close
		Post->Settings.bOverride_AutoExposureBias = true;
		Post->Settings.AutoExposureBias = -.1f;
	}
	if (UMaterialInterface* BM = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/KeysRun/Materials/M_KR_Bubble.M_KR_Bubble")))
	{
		BubbleMID = UMaterialInstanceDynamic::Create(BM, this);
		BubbleMID->SetScalarParameterValue(TEXT("Bright"), .55f);
	}
	Gauge->SetVisibility(false, true);
	Kit->SetVisibility(false);
	Pops.SetNum(48);
	Bubbles.SetNum(420);
	Muzzle.SetNum(120);
	for (FPuff& P : Muzzle) P.Life = 0;
	Snow.SetNum(900);
	for (FSnow& Sn : Snow)
	{
		Sn.P = FVector(DvRand(-11, 11), DvRand(-11, 11), DvRand(-11, 11));
		Sn.V = FVector((FMath::FRand() - .5) * .035, -.05 - FMath::FRand() * .09, (FMath::FRand() - .5) * .035);
		Sn.Seed = FMath::FRand();
	}
	SeepB.SetNum(8 * 34);
	for (int32 i = 0; i < SeepB.Num(); i++) SeepB[i].Slot = i / 34;
	Seeps.SetNum(8);
	for (FSeep& Sp : Seeps) Sp.Key = MIN_int64;
}

AKeysBoatPawn* AKeysDiver::Pawn() const { return Cast<AKeysBoatPawn>(UGameplayStatics::GetPlayerPawn(this, 0)); }

// ================================================================ controller
void AKeysDiver::EnterWater(double BoatX, double BoatZ, double BoatH)
{
	if (bDiving) return;
	// over the side: offset to starboard so the diver doesn't start inside the hull
	const double RX = FMath::Cos(BoatH), RZ = -FMath::Sin(BoatH);
	S = CreateState(BoatX + RX * 4, -.2, BoatZ + RZ * 4, BoatH, AirMax * AirMul);
	CamYaw = PrevYaw = BoatH; CamPitch = -.15; CamMode = bDefaultChase ? 1 : 0; YawRate = 0;
	bDiving = true; bCanReboard = false; WakingT = 0; Acc = 0;
	Gun = FGun();
	Shot = FShot();
	Speared = FSpeared();
	bDrawing = bDrawLatched = bHauling = false;
	FishMode = 0;
	bPrevSpearActive = bShotHadFight = false;
	LastEqDepth = 0;
	FovV = 58;
	DiveSecs = DiveMaxDepth = 0;
	EnterT = GetWorld()->GetTimeSeconds();
	BeginScuba();
	Body->SetVisibility(CamMode == 1, true);
	Kit->SetVisibility(CamMode == 1 && Sc.bOn);
	GunBase->SetVisibility(true, true);
}

// ================================================================ scuba (KeysScuba.h)
void AKeysDiver::BeginScuba()
{
	Sc = KeysScuba::FState();
	ExhAcc = 0; GlanceW = 0; GlanceAutoT = 0; PsiUsedBefore = 0;
	bDebugGlance = bDbgOrbit = bDbgTopside = bDebugSprint = bHelloSaid = false;
	DebugAscend = 0; HoverT = GasLogT = FishLogT = -1;
	AKeysBoatPawn* P = Pawn();
	if (!P || !KeysScuba::WillDive(P))
	{
		UE_LOG(LogTemp, Log, TEXT("KRScuba enter: breath-hold (set owned %d, scuba chosen %d)"), KeysScuba::HasSet(P) ? 1 : 0, KeysScuba::WantScuba() ? 1 : 0);
		return;
	}
	const double Now = GetWorld()->GetTimeSeconds();
	CylIx = KeysScuba::BestCylinder(P);
	Sc.bOn = true;
	Sc.Psi = Sc.StartPsi = KeysScuba::CylinderPsi(P, CylIx);
	Sc.bComputer = KeysScuba::Owns(P, KeysScuba::FxComputer);
	Sc.bBailAvail = P->HasGear(TEXT("dive.tank")) && P->CountOf(TEXT("dive.fill")) > 0;
	Sc.RateMul = KeysScuba::CarryRateMul(Now) * (P->HasGear(TEXT("dive.wetsuit")) ? .94 : 1);   // warmth is air
	Sc.bNoSprint = KeysScuba::Aching(Now);
	Sc.NLoad = KeysScuba::CarryLoad(Now);
	KeysScuba::FKitLook Look;
	Look.bComputer = Sc.bComputer;
	Look.bPony = P->HasGear(TEXT("dive.tank"));
	Look.bLight = KeysScuba::Owns(P, KeysScuba::FxLight);
	const int32 Sig = (Look.bComputer ? 1 : 0) | (Look.bPony ? 2 : 0) | (Look.bLight ? 4 : 0);
	if (Sig != KitSig)
	{
		KitSig = Sig;
		FKeysMesh M;
		KitTris = KeysScuba::BuildWornKit(M, Look);
		M.Commit(Kit, 0, AKeysWorld::VertexColorMaterial());
		FKeysMesh G;
		KeysScuba::BuildConsole(G, Sc.bComputer);
		G.Commit(Gauge, 0, AKeysWorld::VertexColorMaterial());
		FaceSig = -1;
	}
	UE_LOG(LogTemp, Log, TEXT("KRScuba enter: scuba, cylinder %d at %.0f psi, computer %d, bail-out %d, gas rate x%.2f, load %.2f, kit %d triangles"),
		CylIx + 1, Sc.Psi, Sc.bComputer ? 1 : 0, Sc.bBailAvail ? 1 : 0, Sc.RateMul, Sc.NLoad, KitTris);
}

// the dive is over (aboard again, by the ladder or hauled out): the cylinder goes back in the rack with what is left in
// it, and the dive goes in the logbook
void AKeysDiver::FinishDive()
{
	AKeysBoatPawn* P = Pawn();
	KeysScuba::FLog L;
	L.Site = KeysScuba::SiteNear(P, S.X, S.Z);
	L.MaxDepth = DiveMaxDepth; L.Secs = DiveSecs; L.bScuba = Sc.bOn;
	if (Sc.bOn)
	{
		const double Now = GetWorld()->GetTimeSeconds();
		KeysScuba::SetCylinderPsi(P, CylIx, Sc.Psi);
		L.PsiUsed = PsiUsedBefore + Sc.StartPsi - Sc.Psi; L.bStop = Sc.bStopDone; L.bFast = Sc.bFastEver;
		KeysScuba::LeaveCarry(Now, Sc.NLoad, Sc.bBent, Sc.RateMul >= 1.5, Sc.bStopDone);
		UE_LOG(LogTemp, Log, TEXT("KRScuba back aboard: cylinder %d at %.0f psi (%.0f used this dive), max %.1f m, %.0f s under"), CylIx + 1, Sc.Psi, L.PsiUsed, DiveMaxDepth, DiveSecs);
	}
	if (DiveMaxDepth >= 2 && DiveSecs >= 8)
	{
		const FString Line = KeysScuba::AddLog(P, L);
		UE_LOG(LogTemp, Log, TEXT("KRScuba log: %s"), *Line);
		if (Sc.bOn && P) P->ShowMsg(TEXT("Logbook: ") + Line, 6);
	}
	if (Sc.bOn && P) P->SaveEconomy();
	Sc = KeysScuba::FState();
}

void AKeysDiver::HandleScubaEvents()
{
	AKeysBoatPawn* P = Pawn();
	for (const KeysScuba::EEvent E : Sc.Events)
	{
		const TCHAR* Name = nullptr;
		switch (E)
		{
		case KeysScuba::EEvent::Reserve:
			Name = TEXT("reserve"); GlanceAutoT = 4;
			if (P) P->ShowMsg(TEXT("Reserve: 500 psi. Turn the dive and start up."), 4.5);
			break;
		case KeysScuba::EEvent::Critical:
			Name = TEXT("critical"); GlanceAutoT = 4;
			if (P) P->ShowMsg(TEXT("200 psi. Surface now - slowly."), 4.5);
			break;
		case KeysScuba::EEvent::Bailout:
			Name = TEXT("bail-out");
			// the pony bottle is the bail-out on scuba: switching to it is what spends its fill
			if (P) { P->UseOne(TEXT("dive.fill")); P->ShowMsg(TEXT("Cylinder empty - you are on the pony bottle. It is a few minutes of air: go up."), 5); }
			break;
		case KeysScuba::EEvent::OutOfGas:
			Name = TEXT("out of gas");
			if (P) P->ShowMsg(TEXT("Out of gas. It is one breath to the surface: swim up, breathing out."), 5);
			break;
		case KeysScuba::EEvent::StopAsked:
			Name = TEXT("stop asked");
			if (P && Sc.bComputer) P->ShowMsg(TEXT("The computer asks for a safety stop: level off at 5 m for a minute."), 5);
			break;
		case KeysScuba::EEvent::StopDone:
			Name = TEXT("stop done");
			if (P) P->ShowMsg(TEXT("Safety stop done: a clean profile. You will breathe easier on the next dive."), 5);
			break;
		case KeysScuba::EEvent::FastAscent:
			Name = TEXT("fast ascent");
			if (P) P->ShowMsg(TEXT("Too fast. Ease off: Space alone comes up at the safe rate."), 4);
			break;
		case KeysScuba::EEvent::Bent:
			Name = TEXT("bent");
			if (P) P->ShowMsg(TEXT("You came up too fast. Your joints ache and your head pounds: no sprinting, and the gas goes quicker, for a while."), 6);
			break;
		case KeysScuba::EEvent::BentBad:
			Name = TEXT("bent badly");
			if (P) P->ShowMsg(TEXT("That was a bad one. You will feel it for the rest of the afternoon."), 5);
			break;
		case KeysScuba::EEvent::Winded: Name = TEXT("winded"); break;
		case KeysScuba::EEvent::BreathIn: BreathSound(true); if (OnBreath) OnBreath(true, S.Depth(), S.Exertion); break;
		case KeysScuba::EEvent::BreathOut: BreathSound(false); if (OnBreath) OnBreath(false, S.Depth(), S.Exertion); break;
		}
		if (Name) UE_LOG(LogTemp, Log, TEXT("KRScuba event: %s at %.1f m, %.0f psi, %.0f s into the dive"), Name, S.Depth(), Sc.Psi, Sc.Secs);
	}
}

// The regulator, through the audio actor's own one-shot synth (it already muffles everything under water): the breath
// in is the demand valve's click and a dry hiss that rises as the lungs fill; the breath out is a low rumble with a
// run of bubble pops over it, more of them and closer together when she is working.
void AKeysDiver::BreathSound(bool bInhale)
{
	AKeysAudio* A = AKeysAudio::Get(this);
	if (!A || !bCamUnder || (DbgOff & 1)) return;
	const double Ex = DvClamp(S.Exertion, 0, 1);
	const double Period = FMath::Lerp(5.2, 2.6, Ex);
	if (bInhale)
	{
		const float Len = float(Period * .38);
		FKeysBurst Click;
		Click.Filter = KeysDSP::EFilter::Bandpass; Click.Freq = 1500; Click.Q = 5; Click.Gain = .05f; Click.Att = .002f; Click.Dur = .035f;
		A->Burst(Click);
		FKeysBurst Hiss;
		Hiss.Filter = KeysDSP::EFilter::Bandpass; Hiss.Freq = 620; Hiss.FreqTo = 980; Hiss.Q = 1.4f;
		Hiss.Gain = float(.085 + .04 * Ex); Hiss.Att = Len * .35f; Hiss.Dur = Len;
		A->Burst(Hiss);
	}
	else
	{
		const float Len = float(Period * .45);
		FKeysBurst Rumble;
		Rumble.Filter = KeysDSP::EFilter::Lowpass; Rumble.Freq = 260; Rumble.FreqTo = 150; Rumble.Q = 1;
		Rumble.Gain = float(.1 + .05 * Ex); Rumble.Att = .06f; Rumble.Dur = Len;
		A->Burst(Rumble);
		const int32 N = 9 + int32(Ex * 8);
		for (int32 i = 0; i < N; i++)
		{
			FKeysBurst B;
			const double U = (i + FMath::FRand()) / N;
			B.Filter = KeysDSP::EFilter::Bandpass; B.Freq = float(DvRand(280, 900)); B.FreqTo = B.Freq * 1.5f; B.Q = 6;
			B.Gain = float(DvRand(.05, .1) * (1 - .6 * U)); B.Att = .004f; B.Dur = float(DvRand(.05, .11)); B.Delay = float(U * Len * .9);
			A->Burst(B);
		}
	}
}

KeysScuba::FHud AKeysDiver::ScubaHud() const
{
	KeysScuba::FHud H;
	if (!bDiving || !Sc.bOn) return H;
	H.bOn = true;
	H.bGas = !Sc.bOutOfGas;
	H.Source = Sc.bOnBailout ? TEXT("Bail-out bottle") : TEXT("Cylinder");
	const double Gas = Sc.bOnBailout ? Sc.BailPsi : Sc.Psi;
	H.Psi = FMath::RoundToInt(Gas / 10) * 10;
	H.Frac = Sc.GasFrac();
	H.bLow = Sc.bOnBailout || Sc.Psi < KeysScuba::ReservePsi;
	H.bCritical = Sc.bOnBailout || Sc.Psi < KeysScuba::CriticalPsi;
	if (Sc.bComputer)
	{
		const double Rate = FMath::Max(Sc.RateNow, KeysScuba::GasRate(S.Depth(), 0, Sc.RateMul)) * (Sc.bOnBailout ? KeysScuba::PonyRatio : 1);
		H.MinLeft = FMath::FloorToInt32(Gas / Rate / 60);
		H.Dive = FString::Printf(TEXT("%d:%02d  max %.1f m"), int32(Sc.Secs) / 60, int32(Sc.Secs) % 60, Sc.MaxDepth);
		if (Sc.bStopAsked && !Sc.bStopDone)
		{
			const int32 Left = FMath::CeilToInt32(KeysScuba::StopSecs - Sc.StopT);
			H.Stop = FString::Printf(TEXT("Stop %d:%02d"), Left / 60, Left % 60);
		}
	}
	return H;
}

FString AKeysDiver::ToggleLight()
{
	if (!KeysScuba::Owns(Pawn(), KeysScuba::FxLight)) return TEXT("No dive light aboard - Scuba Steve's sells one.");
	TorchMode = (TorchMode + 1) % 3;
	return TorchMode == 0 ? TEXT("Dive light: comes on in the dark") : TorchMode == 1 ? TEXT("Dive light on") : TEXT("Dive light off");
}

bool AKeysDiver::TorchWanted() const
{
	if (S.Depth() > 14) return true;
	if (const AKeysWorld* Wd = AKeysWorld::Get(this)) return Wd->SunDir().Z < .14;
	return false;
}

FString AKeysDiver::SwapCylinder()
{
	AKeysBoatPawn* P = Pawn();
	if (!bDiving || !Sc.bOn || !P) return FString();
	if (KeysScuba::Cylinders(P) < 2) return TEXT("There is only the one cylinder. Scuba Steve's sells a second for the boat.");
	if (S.Depth() > 1.5 || !bCanReboard) return TEXT("Cylinders are swapped at the surface, at the boat.");
	const int32 Other = 1 - CylIx;
	const double OtherPsi = KeysScuba::CylinderPsi(P, Other);
	if (OtherPsi <= Sc.Psi) return FString::Printf(TEXT("The other cylinder has less in it (%.0f psi). Keeping this one."), OtherPsi);
	KeysScuba::SetCylinderPsi(P, CylIx, Sc.Psi);
	PsiUsedBefore += Sc.StartPsi - Sc.Psi;
	CylIx = Other;
	Sc.Psi = Sc.StartPsi = OtherPsi;
	Sc.bOutOfGas = Sc.bOnBailout = false;
	Sc.bReserveSaid = Sc.bCriticalSaid = false;
	RackSig = -1;
	UE_LOG(LogTemp, Log, TEXT("KRScuba swap: now on cylinder %d at %.0f psi"), CylIx + 1, Sc.Psi);
	return FString::Printf(TEXT("Cylinders swapped at the ladder: %.0f psi on your back."), Sc.Psi);
}

// the gauge console, lifted into the dive-mask view by the left hand: held up while the key is down, and by itself for a
// few seconds when the gas reaches the reserve
void AKeysDiver::UpdateGauge(double Dt)
{
	const bool bCan = bDiving && Sc.bOn && CamMode == 0 && !S.bBlackedOut;
	const bool bWant = bCan && (bGlanceKey || bDebugGlance || GlanceAutoT > 0);
	GlanceAutoT = FMath::Max(0.0, GlanceAutoT - Dt);
	GlanceW += ((bWant ? 1.0 : 0.0) - GlanceW) * FMath::Min(1.0, Dt * 7);
	const bool bShow = bCan && GlanceW > .02;
	if (Gauge->IsVisible() != bShow) Gauge->SetVisibility(bShow, true);
	if (!bShow) return;
	const double E = GlanceW * GlanceW * (3 - 2 * GlanceW);
	// camera frame (forward, right, up), meters: up from below the left of the view to just left of centre
	const FVector Off = FMath::Lerp(FVector(.42, -.3, -.5), FVector(.34, -.075, -.045), E);
	const FQuat CamQ = CamRotUE.Quaternion();
	Gauge->SetWorldLocationAndRotation(CamPosUE + CamQ.RotateVector(Off * 100), CamQ * FRotator(-6 + (1 - E) * 30, 10, -5).Quaternion());
	const double Gas = Sc.bOnBailout ? Sc.BailPsi : Sc.Psi;
	const KeysScuba::FHud H = ScubaHud();
	const int32 Mins = int32(Sc.Secs) / 60;
	const int32 Sig = FMath::RoundToInt(Gas / 20) + FMath::RoundToInt(S.Depth() * 10) * 211 + Mins * 77003 + (H.MinLeft + 1) * 1300021;
	if (Sig != FaceSig)
	{
		FaceSig = Sig;
		FKeysMesh M;
		KeysScuba::BuildConsoleFace(M, Gas, S.Depth(), Mins, H.MinLeft, Sc.bComputer);
		M.Commit(GaugeFace, 0, AKeysWorld::VertexColorMaterial());
	}
}

// the dive light: a narrow warm beam from the left hand (or from her shoulder in the chase view) along the look
void AKeysDiver::UpdateTorch(double Dt)
{
	const bool bWant = bDiving && !S.bBlackedOut && S.Depth() > .5 && (TorchMode == 1 || (TorchMode == 0 && TorchWanted())) && KeysScuba::Owns(Pawn(), KeysScuba::FxLight);
	TorchW += ((bWant ? 1.0 : 0.0) - TorchW) * FMath::Min(1.0, Dt * 6);
	const bool bOn = TorchW > .02;
	if (Torch->IsVisible() != bOn) Torch->SetVisibility(bOn);
	if (!bOn) return;
	const double Cp = FMath::Cos(CamPitch);
	const FVector Fw(-FMath::Sin(CamYaw) * Cp, FMath::Sin(CamPitch), -FMath::Cos(CamYaw) * Cp);
	const FRotator Rot = GDir2U(Fw).Rotation();
	// (the beam starts past the muzzle in the dive-mask view, so the gun in her other hand is not lit white by it)
	const FVector Pos = CamMode == 0 ? CamPosUE + CamRotUE.Quaternion().RotateVector(FVector(140, -10, -10)) : G2U(FVector(S.X, S.Y, S.Z) + Fw * 1.5);
	Torch->SetWorldLocationAndRotation(Pos, Rot);
	Torch->SetIntensity(float(TorchW * 60));
}

// The cylinder rack in the cockpit and the diver-down flag over it. They ride on the boat's own mesh component (found
// by its name, so the boat's files are not touched): nothing is built, and the boat is as it was, until a cylinder is
// owned. The rigged set stands in the rack until she goes over with it on; the flag flies while she is in the water.
void AKeysDiver::UpdateBoatKit(double T)
{
	AKeysBoatPawn* P = Pawn();
	if (!P) return;
	const int32 Cyl = KeysScuba::Cylinders(P);
	if (Cyl == 0)
	{
		if (RackSig != 0) { RackSig = 0; BoatRack->ClearAllMeshSections(); BoatFlag->ClearAllMeshSections(); }
		return;
	}
	if (!bBoatKitTried)
	{
		bBoatKitTried = true;
		TArray<UProceduralMeshComponent*> Comps;
		P->GetComponents<UProceduralMeshComponent>(Comps);
		for (UProceduralMeshComponent* C : Comps)
			if (C->GetFName() == FName(TEXT("Boat")))
			{
				BoatRack->AttachToComponent(C, FAttachmentTransformRules::SnapToTargetNotIncludingScale);
				BoatFlag->AttachToComponent(C, FAttachmentTransformRules::SnapToTargetNotIncludingScale);
				break;
			}
	}
	const bool bDown = bDiving && Sc.bOn;
	// slot one: the set (or just the cylinder, before the rest is bought); slot two: the other cylinder
	const bool bFirst = !bDown, bSecond = Cyl > 1;
	const bool bRigged = KeysScuba::HasSet(P);
	const int32 Sig = 1 + (bFirst ? 2 : 0) + (bSecond ? 4 : 0) + (bRigged ? 8 : 0) + int32(GetTypeHash(P->Spec().Id) & 0xffff) * 16;
	if (Sig != RackSig)
	{
		RackSig = Sig;
		FKeysMesh M;
		KeysScuba::BuildBoatRack(M, P->Spec(), bFirst, bRigged, bSecond);
		M.Commit(BoatRack, 0, AKeysWorld::VertexColorMaterial());
		UE_LOG(LogTemp, Log, TEXT("KRScuba rack: set aboard %d (rigged %d), second cylinder %d, %d triangles"), bFirst ? 1 : 0, bRigged ? 1 : 0, bSecond ? 1 : 0, M.T.Num() / 3);
	}
	if (bDiving)
	{
		// it flutters only while the camera is out of the water to see it (every other frame); from below it hangs as
		// it was last drawn, so a dive does not pay for a mesh rebuilt every frame
		const bool bSeen = !bCamUnder && (++FlagTick & 1);
		if (BoatFlag->GetNumSections() == 0 || bSeen)
		{
			FKeysMesh F;
			KeysScuba::BuildFlag(F, P->Spec(), T);
			F.Commit(BoatFlag, 0, AKeysWorld::VertexColorMaterial());
		}
	}
	else if (BoatFlag->GetNumSections() > 0) BoatFlag->ClearAllMeshSections();
}

bool AKeysDiver::RequestReboard()
{
	if (!bDiving) return false;
	if (!bCanReboard || S.bBlackedOut) return false;
	ExitToBoat();
	return true;
}

void AKeysDiver::ExitToBoat()
{
	if (bDiving) FinishDive();
	Kit->SetVisibility(false);
	Gauge->SetVisibility(false, true);
	Torch->SetVisibility(false);
	TorchW = 0;
	bDiving = false; bCanReboard = false; WakingT = 0; bWakeDue = false;
	Shot = FShot();
	if (Speared.bOn) EndFight();
	FishMode = 0;
	FishOnLine->ClearAllMeshSections();
	Lines->ClearAllMeshSections();
	bDrawing = bDrawLatched = bHauling = false;
	Body->SetVisibility(false, true);
	GunBase->SetVisibility(false, true);
	if (AKeysLife* L = AKeysLife::Get(this)) L->SetDiver(false, 0, 0, 0);
	// the camera is on deck from the next frame: no underwater look fading out over it (it drew the deck through
	// 0.3 s of haze), just water on the lens
	if (ExFix > 0 && (bCamUnder || UnderAmt > 0))
	{
		bCamUnder = false;
		UnderAmt = 0;
		Wet = 1;
	}
	OutT = GetWorld()->GetTimeSeconds();
	// a cut: the temporal upscaler and Lumen start the deck view fresh instead of from the underwater frames
	if (ExFix > 0)
		if (APlayerCameraManager* C = UGameplayStatics::GetPlayerCameraManager(this, 0)) C->SetGameCameraCutThisFrame();
}

void AKeysDiver::SetButtons(const FKeysDiverButtons& B)
{
	Btn = B;
	if (B.bFire) bFireQueued = true;
	if (B.bDrawToggle) bDrawQueued = true;
	bHauling = B.bHaul || bDebugHaul;
	if (bDiving && !S.bBlackedOut)
	{
		// mouse look (camera.ts: 0.0022 rad per pixel under the pointer lock)
		CamYaw -= B.LookYaw;
		CamPitch = DvClamp(CamPitch - B.LookPitch, -1.3, 1.3);
	}
}

FString AKeysDiver::CycleView()
{
	CamMode = 1 - CamMode;
	Body->SetVisibility(bDiving && CamMode == 1, true);
	Kit->SetVisibility(bDiving && CamMode == 1 && Sc.bOn);
	return CamMode == 0 ? TEXT("Dive mask view") : TEXT("Chase view");
}

FString AKeysDiver::AscentWarning() const
{
	if (!bDiving) return FString();
	if (Sc.OnGas()) return KeysScuba::Warning(Sc, S);
	const double Depth = S.Depth(), Af = AirFrac();
	const double Margin = S.Air - Depth / 1.2;
	if (Af < .12 || Margin < 4) return TEXT("SURFACE NOW - air critical");
	if (Depth < ShallowBlackoutDepth && Af < .18) return TEXT("Shallow-water blackout risk - ascend gently");
	if (Af < .3 && Depth > 3) return TEXT("Air low - start heading up");
	return FString();
}

double AKeysDiver::TunnelAmount() const
{
	if (!bDiving) return 0;
	if (S.bBlackedOut) return 1;
	const double Af = BreathFrac();
	const double Tun = (Af < .25 ? FMath::Min(1.0, (.25 - Af) / .25) : 0) * .85;
	// a bend: the edges of the view throb
	if (Sc.bOn && Sc.bBent) return FMath::Max(Tun, .3 + .12 * FMath::Sin(GetWorld()->GetTimeSeconds() * 2.2));
	return Tun;
}

void AKeysDiver::FixedStep(double Dt, double T)
{
	AKeysBoatPawn* P = Pawn();
	const double BX = P ? P->BoatX() : S.X, BZ = P ? P->BoatZ() : S.Z;
	if (S.bBlackedOut)
	{
		WakingT += Dt;
		FInput In;
		In.LookYaw = CamYaw; In.LookPitch = CamPitch;
		Step(S, In, T, Dt, SwimMul * 1.6);   // 1.6: long freediving fins
		FrameEvents.Append(S.Events);
		if (WakingT >= BlackoutWakeDelay)
		{
			if (P && ExFix > 0) bWakeDue = true;   // (AKeysBoatPawn::Tick takes it)
			else
			{
				ExitToBoat();
				if (P) P->ShowMsg(TEXT("You wake up on the deck, coughing. The crew hauled you aboard."), 4);
			}
		}
		return;
	}
	FInput In;
	In.bFwd = Btn.bFwd || bDebugSwim; In.bBack = Btn.bBack; In.bLeft = Btn.bLeft; In.bRight = Btn.bRight;
	In.bAscend = Btn.bAscend || DebugAscend > 0; In.bDescend = Btn.bDescend; In.bSprint = Btn.bSprint || bDebugSprint || DebugAscend > 1;
	In.LookYaw = CamYaw; In.LookPitch = CamPitch;
	// on scuba the same step runs between the cylinder's rules (both do nothing on a breath-hold dive); the set and
	// its short fins are slower through the water than the freediver's long blades
	KeysScuba::PreStep(Sc, S, In, Dt);
	Step(S, In, T, Dt, Sc.bOn ? SwimMul * KeysScuba::SwimK * (Sc.bBent ? .85 : 1) : SwimMul * 1.6);   // 1.6: long freediving fins
	KeysScuba::PostStep(Sc, S, Dt);
	if (Sc.bOn) HandleScubaEvents();
	if (DebugHoldDepth >= 0) { S.Y = FMath::Max(-DebugHoldDepth, KeysSea::SeafloorHeightAt(S.X, S.Z) + .6); S.VY = 0; }
	if (S.Depth() > KeysDive::SurfaceEps) DiveSecs += Dt;
	DiveMaxDepth = FMath::Max(DiveMaxDepth, S.Depth());
	// test logs: the hover (20 s with the keys let go) and the gas used over a stretch
	if (HoverT >= 0)
	{
		HoverT += Dt;
		if (HoverT >= 20)
		{
			UE_LOG(LogTemp, Log, TEXT("KRScuba hover: 20 s with no input from %.2f m: vertical drift %.3f m, sideways %.2f m (the current), now at %.2f m, scuba %d"),
				-HoverY0, FMath::Abs(S.Y - HoverY0), FVector2D(S.X - HoverX0, S.Z - HoverZ0).Size(), S.Depth(), Sc.bOn ? 1 : 0);
			HoverT = -1;
		}
	}
	if (GasLogT >= 0)
	{
		GasLogT += Dt;
		if (GasLogT >= GasLogSecs)
		{
			const double Used = GasLogPsi0 - Sc.Psi;
			UE_LOG(LogTemp, Log, TEXT("KRScuba gas: %.0f s at %.1f m, exertion %.2f: %.0f psi used (%.3f psi/s); a full cylinder at this rate lasts %.1f min, this one %.1f min more"),
				GasLogT, S.Depth(), S.Exertion, Used, Used / GasLogT, KeysScuba::FullPsi / FMath::Max(1e-6, Used / GasLogT) / 60, Sc.Psi / FMath::Max(1e-6, Used / GasLogT) / 60);
			GasLogT = -1;
		}
	}
	FrameEvents.Append(S.Events);
	bCanReboard = S.Depth() < 1.5 && FVector2D(S.X - BX, S.Z - BZ).Size() < 6;
	UpdateSpear(Dt, T);
}

// ================================================================ speargun
void AKeysDiver::GetTargets(TArray<FTarget>& Out) const
{
	Out.Reset();
	if (const AKeysLife* L = AKeysLife::Get(this)) L->SpearTargetsNear(S.X, S.Y, S.Z, KeysDive::SpearRange + 6 + 12, Out);
}

FVector AKeysDiver::EyeWorld() const { return U2G(CamPosUE); }

FVector AKeysDiver::MuzzleWorld() const
{
	// before the first frame of the view model, fall back to a point just in front of the mask
	return MuzzleG.IsZero() ? EyeWorld() : MuzzleG;
}

bool AKeysDiver::TryFire()
{
	// FishMode: the fish from the last shot is still coming to the hand or held up (4.5 s, longer than the reload). A
	// second fish speared in that time took its place and the first was never landed
	if (Shot.bAlive || Speared.bOn || FishMode != 0) return false;
	if (!CanFire(Gun)) { Nudge = .6; return false; }
	if (S.Air <= 0 || S.bBlackedOut) return false;
	const FVector Origin = MuzzleWorld();
	const FVector Eye = EyeWorld();
	const double Cp = FMath::Cos(CamPitch);
	const FVector Aim(-FMath::Sin(CamYaw) * Cp, FMath::Sin(CamPitch), -FMath::Cos(CamYaw) * Cp);
	TArray<FTarget> Tg;
	GetTargets(Tg);
	int32 Lock;
	// spend the shaft first, so the lead is solved for the range of the one that actually flies (a spare is short)
	bool bSpare = false;
	if (TakeShaft)
	{
		const int32 Left = TakeShaft();
		bSpare = Left < 0;
	}
	ShotRange = FMath::Clamp((SpearRange + FMath::Max(0.0, SpearRangeBonusM)) * (bSpare ? .6 : 1.0), SpearRange * .6, SpearRange + 6);
	const FVector Dir = AssistAim(Origin, Aim, Tg, Lock, AssistStrength, ShotRange, &Eye);
	Shot = Fire(Origin, Dir, ShotRange);
	// a shot at Caelen circling overhead (web caelen.ts resolveSpearShot): the shaft's whole path against his capsule
	if (AKeysExtras* Ex = AKeysExtras::Get(this))
	{
		FVector CA, CB; double CR;
		if (Ex->CaelenHitbox(CA, CB, CR))
		{
			FVector P1, P2;
			FMath::SegmentDistToSegmentSafe(Origin, Origin + Dir * ShotRange, CA, CB, P1, P2);
			const double D = (P1 - P2).Size();
			if (D <= CR) Ex->CaelenShot(true);
			else if (D <= CR + AKeysExtras::CaelenNearMiss) Ex->CaelenShot(false);
		}
	}
	Gun = StartReload();
	KickT = 1;
	// the pressure kick of the bands slamming shut: a brief wobble of the water across the mask
	if (bCamUnder)
	{
		ShockT = 0;
		ShockUV = FVector2D(.55, .62);
		if (APlayerController* PC = GetWorld()->GetFirstPlayerController())
		{
			FVector2D Scr; int32 VX = 0, VY = 0;
			PC->GetViewportSize(VX, VY);
			if (VX > 0 && VY > 0 && PC->ProjectWorldLocationToScreen(G2U(MuzzleG), Scr, true)) ShockUV = FVector2D(Scr.X / VX, Scr.Y / VY);
		}
	}
	KickYaw = (FMath::FRand() - .5) * .05;
	KickRoll = (FMath::FRand() - .5) * .07;
	// a puff of air out of the muzzle as the bands snap forward
	const int32 N = 14 + FMath::RandRange(0, 7);
	for (int32 i = 0; i < N; i++)
	{
		FPuff& Pf = Muzzle[MuzCursor];
		MuzCursor = (MuzCursor + 1) % Muzzle.Num();
		Pf.P = Origin;
		Pf.V = FVector(Dir.X * DvRand(1.2, 2.6) + DvRand(-.35, .35), Dir.Y * DvRand(1.2, 2.6) + DvRand(-.35, .35) + .6, Dir.Z * DvRand(1.2, 2.6) + DvRand(-.35, .35));
		Pf.Life = Pf.Max = DvRand(.35, .8);
	}
	return true;
}

void AKeysDiver::EndFight()
{
	Speared.bOn = false;
}

void AKeysDiver::UpdateSpear(double Dt, double T)
{
	AKeysBoatPawn* P = Pawn();
	const FVector2D Water = KeysSea::CurrentAt(S.X, S.Z, T, S.Depth());
	KickSpeed = FMath::Max(0.0, S.Speed() - .75 * Water.Size());
	if (bDrawQueued)
	{
		bDrawQueued = false;
		bDrawLatched = !bDrawLatched;
		bDrawing = bDrawLatched;
	}
	if (bFireQueued)
	{
		bFireQueued = false;
		const bool bWasOut = Shot.bAlive;
		TryFire();
		// the gun's thump: the school fish around bolt (the fish the shaft passes are spooked when it lands)
		if (!bWasOut && Shot.bAlive) if (AKeysSchools* Sch = AKeysSchools::Get(this)) Sch->Startle(S.X, S.Z, 8);
	}
	// the latch lets go by itself once the gun is seated, or when the hands are busy
	if (bDrawLatched && (CanFire(Gun) || !HandsFree())) { bDrawLatched = false; bDrawing = false; }
	StepReload(Gun, Dt, bDrawing && HandsFree(), KickSpeed, SpearDrawMul);
	if (Nudge > 0) Nudge = FMath::Max(0.0, Nudge - Dt);

	if (Shot.bAlive)
	{
		TArray<FTarget> Tg;
		GetTargets(Tg);
		FHit Hit;
		const FVector Tip0 = Shot.At(Shot.Dist);
		bool bHitNow = StepSpear(Shot, Tg, Dt, Hit);
		// a den's rock stops the shaft (KeysReefDen.h): what it would have reached behind the rock is safe
		if (const double Rock = KeysReefDen::RayHit(Shot.O, Shot.D, Shot.Range); Rock < Shot.Range && (bHitNow ? Hit.AtDist : Shot.Dist) > Rock)
		{
			bHitNow = false; Shot.Dist = Rock; Shot.bAlive = false;
		}
		// the shaft drags a seam of fine bubbles through the water behind it (the line, a thinner trickle)
		if (bDiving)
		{
			const FVector Tip1 = Shot.At(Shot.Dist);
			const double Seg = (Tip1 - Tip0).Size();
			const int32 Nb = FMath::Min(24, int32(Seg * 9) + (FMath::FRand() < .5 ? 1 : 0));
			for (int32 k = 0; k < Nb; k++)
			{
				const FVector Bp = FMath::Lerp(Tip0, Tip1, FMath::FRand()) - Shot.D * DvRand(.05, .9);
				SpawnBubble(Bp + FVector(DvRand(-.012, .012), DvRand(-.012, .012), DvRand(-.012, .012)), DvRand(.0012, .0035));
			}
		}
		if (bHitNow)
		{
			const FTarget* Target = Tg.FindByPredicate([&](const FTarget& X) { return X.Id == Hit.Id; });
			if (Target && Hit.bHeld)
			{
				// a clean kill shot: no fight under water. The fish shudders on the shaft, bleeds a little into the current,
				// and is drawn in to the diver's hand
				Speared.bOn = false;
				Speared.Target = *Target;
				const KeysData::FSpecies* Sp = KeysData::Species(Target->Key);
				UE_LOG(LogTemp, Log, TEXT("KRDive speared %s %.1f lb at %.1f m"), *Speared.Target.Key, Speared.Target.Weight, Hit.AtDist);
				if (AKeysLife* L = AKeysLife::Get(this)) L->TakeSpeared(Target->Id);
				FKeysMesh M;
				const double Scale = Sp ? 1.35 * DvClamp(FMath::Pow(Target->Weight / ((Sp->Min + Sp->Max) / 2), 1.0 / 3), .75, 1.5) : 1.35;
				if (!AKeysLife::BuildCreatureMesh(Target->Key, M, Scale))
					KeysMesh::BuildFish(M, KeysMesh::FishLength(Target->Weight), Sp ? Sp->Color : FLinearColor::Gray, false);
				M.SmoothNormals();
				{
					// the shaft through it, the head and its open flopper out the far side (sized from the fish's own girth)
					double HalfY = .02, HalfZ = .03;
					for (const FVector& Vv : M.V)
						if (FMath::Abs(Vv.X) < 12) { HalfY = FMath::Max(HalfY, FMath::Abs(Vv.Y) / 100); HalfZ = FMath::Max(HalfZ, FMath::Abs(Vv.Z) / 100); }
					KeysSpeargun::AddShaftThroughFish(M, HalfY, HalfZ);
				}
				M.Commit(FishOnLine, 0, AKeysWorld::VertexColorMaterial());
				FishMode = 3; KillT = 0;
				FishPos = FishTgt = Hit.Point;
				FishHead = FMath::Atan2(-(Hit.Point.X - S.X), -(Hit.Point.Z - S.Z)) + PI * .5;
				BleedAt(Hit.Point, 10, .08);
				if (P) P->ShowMsg(TEXT("Stoned it - clean shot."), 2.5);
			}
			else if (Target && P) P->ShowMsg(TEXT("The shot doesn't have enough force left to hold - it glances off."));
			Shot.bAlive = false;
		}
	}

	if (Speared.bOn)
	{
		if (S.bBlackedOut)
		{
			if (P) P->ShowMsg(TEXT("Blacked out - the shaft tears free. You lose the fish, and the gear's lucky to float back to you."), 4);
			FishMode = 2; FleeT = 0;
			FleeDir = FVector(FishTgt.X - S.X, 0, FishTgt.Z - S.Z).GetSafeNormal();
			EndFight();
		}
		else
		{
			StepFight(Speared.F, bHauling, Speared.P, S.X, S.Z, Dt);
			const FFight& F = Speared.F;
			FishTgt = FVector(F.X, S.Y - .3, F.Z);
			FishTension = F.Tension; FishStam = F.Stam;
			if (F.Outcome == EFight::Landed)
			{
				if (P) P->LandSpeared(Speared.Target.Key, Speared.Target.Weight, F.X, F.Z);
				FishMode = 0;
				FishOnLine->ClearAllMeshSections();
				EndFight();
			}
			else if (F.Outcome == EFight::TornFree)
			{
				if (P) P->ShowMsg(TEXT("The shaft tears free - it got away."));
				FishMode = 2; FleeT = 0;
				FleeDir = FVector(FishTgt.X - S.X, 0, FishTgt.Z - S.Z).GetSafeNormal();
				EndFight();
			}
		}
	}

	// a clean miss spooks the fish the shaft hissed past (world.ts: SPEAR_NEAR_MISS_MARGIN)
	const bool bActive = Shot.bAlive || Speared.bOn;
	if (bActive && !bPrevSpearActive) { bShotHadFight = false; ShotO = Shot.O; ShotD = Shot.D; }
	if (bActive && Speared.bOn) bShotHadFight = true;
	if (!bActive && bPrevSpearActive && !bShotHadFight)
	{
		if (AKeysLife* L = AKeysLife::Get(this))
		{
			TArray<FTarget> Tg;
			L->SpearTargetsNear(ShotO.X, ShotO.Y, ShotO.Z, ShotRange + 1.1, Tg);
			const FVector P2 = ShotO + ShotD * ShotRange;
			for (const FTarget& F : Tg)
			{
				double Sx;
				const double Bound = F.Radius + 1.1;
				if (ClosestDistSqSegSeg(ShotO, P2, F.A, F.B, Sx) <= Bound * Bound) L->SpookAt((F.A.X + F.B.X) * .5, (F.A.Z + F.B.Z) * .5);
			}
		}
	}
	bPrevSpearActive = bActive;
}

// ================================================================ per frame
void AKeysDiver::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	const double Dt = FMath::Min(.05, double(DeltaSeconds));
	const double T = GetWorld()->GetTimeSeconds();
	// test hook (dv:prof=1): where this actor's frame goes, in six laps, logged every 150 frames
	double LapT = FPlatformTime::Seconds();
	auto Lap = [&](int32 i) { if (!bDbgProf) return; const double Now = FPlatformTime::Seconds(); ProfMs[i] += (Now - LapT) * 1000; LapT = Now; };
	FrameEvents.Reset();
	if (bDiving)
	{
		Acc += Dt;
		int32 N = 0;
		while (Acc >= KeysDive::StepDt && N < 5)
		{
			SimT = T;
			FixedStep(KeysDive::StepDt, T);
			Acc -= KeysDive::StepDt;
			N++;
			if (!bDiving) break;
		}
		for (const EEvent E : FrameEvents)
		{
			AKeysBoatPawn* P = Pawn();
			if (!P) break;
			if (E == EEvent::BlackoutShallow) P->ShowMsg(TEXT("Shallow-water blackout on the ascent - hauled back aboard unconscious. The gear is safe; the dive is not."), 5);
			else if (E == EEvent::BlackoutAir) P->ShowMsg(TEXT("Blacked out from holding your breath too long - hauled back aboard. You lose the fish and the trip; the gear's safe."), 5);
		}
		if (AKeysLife* L = AKeysLife::Get(this))
		{
			if (bDiving)
			{
				const FVector2D W = WaterVelocity(S.X, S.Y, S.Z, T, KeysSea::SeafloorHeightAt(S.X, S.Z));
				// fish bolt from speed: a diver on scuba, breathing easy and not sprinting, reads as slower than she is,
				// so they let her come closer than a freediver finning down on them
				const double Rel = FVector2D(S.VX - W.X, S.VZ - W.Y).Size();
				const double Seen = Sc.OnGas() && S.Exertion < .8 && !(DbgOff & 16) ? Rel * KeysScuba::FishCalmK : Rel;
				L->SetDiver(true, S.X, S.Z, Seen, S.Y);
				if (FishLogT >= 0 && T >= FishLogT)
				{
					FishLogT = T + 2;
					TArray<FTarget> Tg;
					GetTargets(Tg);
					double Near = 1e9;
					for (const FTarget& F : Tg) Near = FMath::Min(Near, ((F.A + F.B) * .5 - FVector(S.X, S.Y, S.Z)).Size());
					UE_LOG(LogTemp, Log, TEXT("KRScuba fish: swimming %.2f m/s, the fish see %.2f m/s (scuba %d), nearest fish %.1f m of %d"), Rel, Seen, Sc.OnGas() ? 1 : 0, Near, Tg.Num());
				}
			}
		}
	}
	Lap(0);
	if (bDiving)
	{
		UpdateCamera(Dt, T);
		UpdateModel(T);
		UpdateGun(Dt);
		UpdateGauge(Dt);
		UpdateTorch(Dt);
		if (Sc.bOn && !bHelloSaid && T - EnterT > 6.5)
		{
			bHelloSaid = true;
			if (AKeysBoatPawn* P = Pawn())
				P->ShowMsg(FString::Printf(TEXT("On scuba: %.0f psi. You hover where you stop. Space comes up at the safe rate; with Shift it is a bolt, and it costs you."), Sc.Psi), 6);
		}
		if (AKeysBoatPawn* P = Pawn()) P->ApplyDiveCamera(CamPosUE, CamRotUE, CamFovH);
		// the live reticle: what the aim assist would lock onto right now (strength 0, so it only reports)
		{
			TArray<FTarget> Tg;
			GetTargets(Tg);
			const double Cp = FMath::Cos(CamPitch);
			const FVector Aim(-FMath::Sin(CamYaw) * Cp, FMath::Sin(CamPitch), -FMath::Cos(CamYaw) * Cp);
			const FVector Eye = EyeWorld();
			AssistAim(MuzzleWorld(), Aim, Tg, AimTarget, 0, NextShotRange(), &Eye);
		}
	}
	Lap(1);
	UpdateBoatKit(T);
	Lap(2);
	if (DbgBoatView > 0)
	{
		// test hook: a camera standing off the cockpit, on the rack (1), on the flag (2) or on the whole boat (3); it
		// rides the boat's own mesh, in the water or out of it
		if (AKeysBoatPawn* P = Pawn(); P && BoatRack->GetAttachParent() && BoatRack->GetAttachParent() != Root)
		{
			const FTransform Bt = BoatRack->GetAttachParent()->GetComponentTransform();
			const FVector Base = KeysScuba::RackBase(P->Spec());
			const FVector Eye = Base + (DbgBoatView == 1 ? FVector(1.5, 1.7, 1.7) : DbgBoatView == 2 ? FVector(-2.6, 3.4, 2.4) : FVector(-6.5, 5.5, 4.2));
			const FVector Aim = Base + (DbgBoatView == 1 ? FVector(0, 0, .5) : DbgBoatView == 2 ? FVector(-.4, 0, 1.75) : FVector(1.5, .8, .8));
			const FVector EyeW = Bt.TransformPosition(Eye * 100), AimW = Bt.TransformPosition(Aim * 100);
			CamPosUE = EyeW; CamRotUE = (AimW - EyeW).Rotation();
			P->ApplyDiveCamera(CamPosUE, CamRotUE, 70);
		}
	}
	UpdateShaftAndLine(T);
	UpdateSpearedFish(Dt, T);
	UpdateFishFill();
	UpdateUnderwaterLook(Dt);
	UpdateExposure(Dt);
	Lap(3);
	UpdateBubbles(Dt, T);
	UpdateSnow(Dt, T);
	UpdateSeeps(Dt, T);
	Lap(4);
	DrawParticles();
	Lap(5);
	if (bDbgProf && ++ProfN >= 150)
	{
		int32 Alive = 0;
		for (const FBubble& B : Bubbles) if (B.Age >= 0) Alive++;
		UE_LOG(LogTemp, Log, TEXT("KRScuba prof (ms a frame, %d frames, scuba %d): step %.3f, camera gun gauge light %.3f, boat kit %.3f, lines fish look %.3f, bubbles snow seeps %.3f, draw particles %.3f; total %.3f; %d bubbles alive"),
			ProfN, Sc.bOn ? 1 : 0, ProfMs[0] / ProfN, ProfMs[1] / ProfN, ProfMs[2] / ProfN, ProfMs[3] / ProfN, ProfMs[4] / ProfN, ProfMs[5] / ProfN,
			(ProfMs[0] + ProfMs[1] + ProfMs[2] + ProfMs[3] + ProfMs[4] + ProfMs[5]) / ProfN, Alive);
		ProfN = 0;
		for (double& M : ProfMs) M = 0;
	}
}

void AKeysDiver::UpdateCamera(double Dt, double T)
{
	const double Depth = S.Depth(), Af = BreathFrac();
	const double Tunnel = Af < .25 ? FMath::Min(1.0, (.25 - Af) / .25) : 0;
	// The web game's surface transition narrows its 58 degree boat FOV by a quarter under water (a mask magnifies),
	// and the mask camera narrows it further toward 34 degrees as the air runs out. Vertical degrees, like three.js.
	const double Base = 58 * (1 - .25 * UnderAmt);
	const double Speed = S.Speed();
	const double WantFov = Base - (Base - 34) * Tunnel + FMath::Min(4.0, Speed * 1.5) * (1 - Tunnel);
	const double RawRate = Dt > 0 ? (CamYaw - PrevYaw) / Dt : 0;
	PrevYaw = CamYaw;
	YawRate += (DvClamp(RawRate, -6, 6) - YawRate) * FMath::Min(1.0, Dt * 8);
	FovV += (WantFov - FovV) * FMath::Min(1.0, Dt * 3);
	double Aspect = 16.0 / 9;
	if (UGameViewportClient* VPC = GetWorld()->GetGameViewport())
	{
		FVector2D Sz;
		VPC->GetViewportSize(Sz);
		if (Sz.Y > 0) Aspect = Sz.X / Sz.Y;
	}
	CamFovH = float(FMath::RadiansToDegrees(2 * FMath::Atan(FMath::Tan(FMath::DegreesToRadians(FovV) / 2) * Aspect)));

	const double Cp = FMath::Cos(CamPitch);
	const FVector Fw(-FMath::Sin(CamYaw) * Cp, FMath::Sin(CamPitch), -FMath::Cos(CamYaw) * Cp);
	if (CamMode == 0)
	{
		const double SwayK = S.bNarcosis ? 2.2 : 1;
		const double Sway = FMath::Sin(T * 1.3) * .015 * SwayK;
		const double Stroke = S.bKicking ? FMath::Max(0.0, FMath::Sin(S.KickPhase * PI * 2)) : 0;
		const double Surge = Stroke * .05 * FMath::Min(1.0, Speed / 1.2);
		const double Bank = DvClamp(YawRate * .03, -.1, .1);
		const FVector Pos(S.X + Fw.X * Surge, S.Y + .15 + Fw.Y * Surge - Stroke * .012, S.Z + Fw.Z * Surge);
		CamPosUE = G2U(Pos);
		const double Roll = FMath::Cos(T * .9) * .01 * SwayK + Bank;
		CamRotUE = FRotator(FMath::RadiansToDegrees(CamPitch + Sway + Stroke * .006), YawDeg(CamYaw), FMath::RadiansToDegrees(Roll));
	}
	else
	{
		FVector Pos(S.X - Fw.X * 3.4, S.Y + 1.1 - Fw.Y * 1.2, S.Z - Fw.Z * 3.4);
		if (bDbgOrbit)
		{
			// test hook: the chase camera swung round her (yaw 0 is behind her fins)
			const double Oy = CamYaw + DbgOrbitYaw, Op = FMath::Cos(DbgOrbitPitch);
			Pos = FVector(S.X + FMath::Sin(Oy) * Op * DbgOrbitDist, S.Y + .1 + FMath::Sin(DbgOrbitPitch) * DbgOrbitDist, S.Z + FMath::Cos(Oy) * Op * DbgOrbitDist);
		}
		CamPosUE = G2U(Pos);
		CamRotUE = (G2U(FVector(S.X, S.Y + .15, S.Z)) - CamPosUE).Rotation();
	}
	if (ExFix > 0)
	{
		// Never a lens sitting in the surface itself: within a few centimetres of it the sea is drawn from one side and
		// the look from the other (a frame of black slab and bare sand). Her swimming has a flat sea at 0; the drawn one
		// heaves. Floating at the surface her head rides the swell, eyes out of the water; below it the lens is kept
		// 0.2 m clear of the waves on the side it is on, so it crosses once, cleanly, as she comes up or goes down.
		FVector Cg = U2G(CamPosUE);
		const double Sf = WaveH(Cg.X, Cg.Z, T, AmpAt(Cg.X, Cg.Z), false), Rel = Cg.Y - Sf;
		if (S.Depth() <= KeysDive::SurfaceEps && !S.bBlackedOut) Cg.Y = Sf + .2 + FMath::Max(0.0, Cg.Y - .15);
		else
		{
			const bool bSideUnder = bCamUnder ? Rel < 0 : Rel < -.25;
			Cg.Y = bSideUnder ? FMath::Min(Cg.Y, Sf - .2) : FMath::Max(Cg.Y, Sf + .2);
		}
		CamPosUE = G2U(Cg);
	}
	if (bDbgTopside)
	{
		// test hook: standing in the boat, looking over the side at the water above her
		const AKeysBoatPawn* P = Pawn();
		const double BX = P ? P->BoatX() : S.X - 4, BZ = P ? P->BoatZ() : S.Z;
		const FVector2D To = FVector2D(S.X - BX, S.Z - BZ).GetSafeNormal();
		CamPosUE = G2U(FVector(BX + To.X * 1.75, 1.95, BZ + To.Y * 1.75));
		CamRotUE = (G2U(FVector(S.X, 0, S.Z)) - CamPosUE).Rotation();
	}
}

void AKeysDiver::UpdateModel(double T)
{
	Body->SetWorldLocationAndRotation(G2U(FVector(S.X, S.Y, S.Z)), FRotator(0, YawDeg(S.Yaw), 0));
	const double K = FMath::Min(1.0, S.Speed() / 1.8), Rate = 3.5 + K * 7, Ph = T * Rate;
	FinL->SetRelativeRotation(FRotator(FMath::RadiansToDegrees(FMath::Sin(Ph) * (.25 + K * .5)), 0, 0));
	FinR->SetRelativeRotation(FRotator(FMath::RadiansToDegrees(FMath::Sin(Ph + PI) * (.25 + K * .5)), 0, 0));
	Body->SetVisibility(CamMode == 1, true);
	Kit->SetVisibility(CamMode == 1 && Sc.bOn && !(DbgOff & 8));
}

void AKeysDiver::UpdateGun(double Dt)
{
	const bool bLoaded = CanFire(Gun);
	const bool bDrawNow = bDrawing && HandsFree() && !bLoaded && Gun.ReloadT <= 0;
	const double Frac = bLoaded ? 1 : DvClamp(Gun.Draw, 0, 1);
	const double Recover = Gun.ReloadT > 0 ? FMath::Min(1.0, 1 - Gun.ReloadT / SpearReload) : 1;
	// reloading is a new shaft fed in through the muzzle and pushed home until it latches (the bands go on at the end)
	const double LoadE = Gun.ReloadT > 0 ? FMath::SmoothStep(0.0, 1.0, DvClamp((Recover - .15) / .8, 0, 1)) : 1;
	const double ShaftOut = (1 - LoadE) * .85;
	const double Effort = DrawRateFor(KickSpeed);
	GunShaft->SetRelativeLocation(FVector((BarrelLen + ShaftPoke * .5 + ShaftOut) * 100, 0, RailY * 100));
	StrainT += Dt;
	const double Tremble = bDrawNow ? FMath::Sin(StrainT * 41) * .006 * (.4 + Frac) : 0;
	// on the shot the bands slam forward past their rest length, then spring back and settle (KickT runs 1 -> 0)
	const double Whip = KickT > 0 ? KickT * FMath::Cos((1 - KickT) * PI * 4.5) : 0;
	const double BandLen = FMath::Max(.12, DvLerp(BandShort, BandLong, Frac) + Tremble - .14 * Whip);
	const double BandRad = DvLerp(BandR, .0045, Frac) * (1 + .25 * FMath::Max(0.0, Whip));
	// the bands run from their anchor near the muzzle back along the barrel to the hand
	for (int32 Side = -1; Side <= 1; Side += 2)
	{
		UProceduralMeshComponent* Bd = Side < 0 ? GunBandL : GunBandR;
		Bd->SetRelativeLocation(FVector(BandAnchorX * 100, Side * (KeysSpeargun::RollerY - 1.075 * BandRad) * 100, KeysSpeargun::BandZ(BandRad) * 100));   // each pair side by side on top of its roller
		Bd->SetRelativeRotation(FRotator(0, 180, 0));
		Bd->SetRelativeScale3D(FVector(BandLen, BandRad, BandRad));
	}
	PoseW += ((bLoaded ? 0 : 1) - PoseW) * FMath::Min(1.0, Dt * (bLoaded ? 9 : 6));
	const bool bGlove = !bLoaded && PoseW > .3;
	if (bGlove)
	{
		// the loading hand pushes the new shaft back in through the muzzle
		const double HandX = Gun.ReloadT > 0 ? BarrelLen + .05 + ShaftOut * .7 : BandAnchorX - BandLen;
		GunGlove->SetRelativeLocation(FVector(HandX * 100, 0, (RailY + .012) * 100));
		GunGlove->SetRelativeRotation(FRotator(0, 0, bDrawNow ? FMath::RadiansToDegrees(FMath::Sin(StrainT * 7.5) * .18) : 0));
	}
	if (!bWasLoaded && bLoaded) SeatT = 1;
	bWasLoaded = bLoaded;
	const double Pull = bDrawNow ? (.5 + .5 * FMath::Sin(StrainT * 7.5)) * (.018 + .03 * Frac) * (.4 + .6 * Effort) : 0;
	const double ShakeX = bDrawNow ? FMath::Sin(StrainT * 53) * .0035 * Frac : 0, ShakeY = bDrawNow ? FMath::Sin(StrainT * 47 + 1) * .0035 * Frac : 0;
	double Snap = 0;
	if (KickT > 0) { KickT = FMath::Max(0.0, KickT - Dt / .22); const double E = FMath::Sin(KickT * PI); Snap = E * E; }
	const double SeatHop = SeatT > 0 ? FMath::Sin(DvClamp(SeatT, 0, 1) * PI) * .03 : 0;
	if (SeatT > 0) SeatT = FMath::Max(0.0, SeatT - Dt / .18);
	SwayT += Dt * (1 + FMath::Min(1.0, S.Speed() / 2) * 1.6);
	const double Amt = .012 + FMath::Min(1.0, S.Speed() / 2.2) * .02;
	SwayV = FMath::Lerp(SwayV, FVector(0, FMath::Sin(SwayT * 1.1) * Amt, FMath::Sin(SwayT * 1.7 + 1.3) * Amt * .7), FMath::Min(1.0, Dt * 8));
	// camera frame (forward, right, up); three.js +z back is -forward
	const FVector Off(DvLerp(ReadyPos.X, LoadPos.X, PoseW) - .1 * Snap - Pull, DvLerp(ReadyPos.Y, LoadPos.Y, PoseW) + ShakeX, DvLerp(ReadyPos.Z, LoadPos.Z, PoseW) + ShakeY - SeatHop);
	const double PitchR = DvLerp(.05, .3, PoseW) - .16 * Snap + Pull * 1.5 + FMath::Sin(SwayT * 1.7 + 1.3) * Amt * .4;
	const double YawR = DvLerp(.3, .2, PoseW) + KickYaw * Snap;
	const double RollR = DvLerp(-.12, .25, PoseW) + KickRoll * Snap + (bDrawNow ? FMath::Sin(StrainT * 7.5) * .03 : 0) + FMath::Sin(SwayT * 1.1) * Amt * .6;
	// three.js rotations: +x pitches the muzzle up (Unreal +pitch), +y turns it left (Unreal -yaw)
	const FQuat CamQ = CamRotUE.Quaternion();
	FVector PosUE = CamPosUE + CamQ.RotateVector((Off + SwayV) * 100);
	FQuat Q = CamQ * FRotator(FMath::RadiansToDegrees(PitchR), FMath::RadiansToDegrees(-YawR), FMath::RadiansToDegrees(-RollR)).Quaternion();
	if (bDbgGunShow)
	{
		// test hook: the same gun held out and turned to the reference picture's angle (muzzle low and left and toward
		// the eye, the top of the gun showing), for the side-by-side
		const FVector Ax = FVector(-.42, -.86, -.3).GetSafeNormal(), Up = FVector(-.62, .12, .78);
		Q = CamQ * FRotationMatrix::MakeFromXZ(Ax, Up).ToQuat();
		PosUE = CamPosUE + CamQ.RotateVector(FVector(1.5, .0, -.02) * 100) - Q.RotateVector(FVector(.52, 0, 0) * 100);
	}
	GunBase->SetWorldLocationAndRotation(PosUE, Q);
	// the gun is put away while a speared catch is held up for its photo
	const AKeysBoatPawn* Pw = Pawn();
	const bool bShow = CamMode == 0 && !S.bBlackedOut && !(Pw && Pw->FState == EKeysFish::Caught);
	GunBase->SetVisibility(bShow, false);
	if (GunFill->IsVisible() != bShow) GunFill->SetVisibility(bShow);
	if (bShow)
	{
		GunFill->SetWorldLocation(CamPosUE + CamQ.RotateVector(FVector(-10, -45, 55)));
		// less of it where the water is dark (deep, or with the sun down), or the gun would glow
		const AKeysWorld* Wd = AKeysWorld::Get(this);
		const double Amb = DvClamp(1 - S.Depth() / 32, .12, 1) * (Wd && Wd->SunDir().Z < .14 ? .3 : 1);
		GunFill->SetIntensity(float((bDbgGunShow ? 600 : 230) * Amb));
		if (bDbgGunShow != bGunHandOff) { bGunHandOff = bDbgGunShow; GunBase->SetMeshSectionVisible(1, !bGunHandOff); }
	}
	GunBandL->SetVisibility(bShow);
	GunBandR->SetVisibility(bShow);
	GunShaft->SetVisibility(bShow && !Speared.bOn && !(Gun.ReloadT > 0 && Recover < .15));
	GunGlove->SetVisibility(bShow && bGlove);
	GunHandL->SetVisibility(bShow && !bGlove && GlanceW < .25 && !bDbgGunShow);   // the left hand is on the gauge console while it is up
	// the wishbone flicks up off the shaft as the bands slam forward
	GunWish->SetRelativeLocation(FVector((BandAnchorX - BandLen) * 100, 0, (RailY + .025 * FMath::Abs(Whip)) * 100));
	GunWish->SetVisibility(bShow);
	// the leader: from the slide ring on the shaft, in a slack loop, to the hook under the muzzle (rebuilt as the shaft
	// is fed back in on a reload; it goes with the shaft when that is away)
	{
		const bool bLead = GunShaft->IsVisible();
		if (bLead && FMath::Abs(ShaftOut - LeaderOut) > 1e-4)
		{
			LeaderOut = ShaftOut;
			FKeysMesh Ld;
			KeysSpeargun::BuildLeader(Ld, FVector(BarrelLen + ShaftPoke * .5 + ShaftOut + KeysSpeargun::RingX, 0, RailY - .016), KeysSpeargun::LeaderHook(), KeysSpeargun::LeaderLen);
			Ld.Commit(GunLeader, 0, AKeysWorld::VertexColorMaterial());
		}
		GunLeader->SetVisibility(bShow && bLead);
	}
	// the gun in her hand in the chase view: there while she is awake and not holding up a catch; its shaft is on it
	// only while the one in the dive-mask view would be
	{
		const bool bCarry = bDiving && CamMode == 1 && !S.bBlackedOut && !(Pw && Pw->FState == EKeysFish::Caught);
		GunWorn->SetVisibility(bCarry);
		const bool bShaftOn = !Speared.bOn && !Shot.bAlive && FishMode == 0 && !(Gun.ReloadT > 0 && Recover < .15);
		if (bCarry && bShaftOn != bWornShaft) { bWornShaft = bShaftOn; GunWorn->SetMeshSectionVisible(1, bShaftOn); }
	}
	// the reel: the shaft rips line off it in a blur, then it's wound back in while the gun is reloaded
	{
		const double Want = Shot.bAlive ? 70.0 : (Gun.ReloadT > 0 && !Speared.bOn && FishMode == 0 ? -9.0 : 0.0);
		ReelSpin += (Want - ReelSpin) * (1 - FMath::Exp(-Dt / (Shot.bAlive ? .04 : .35)));
		ReelAng = FMath::Fmod(ReelAng + ReelSpin * Dt, 2 * PI);
		GunReel->SetRelativeRotation(FRotator(FMath::RadiansToDegrees(ReelAng), 0, 0));
		GunReel->SetVisibility(bShow);
	}
	MuzzleG = U2G(PosUE + Q.RotateVector(FVector(BarrelLen, 0, RailY) * 100));
}

void AKeysDiver::UpdateShaftAndLine(double T)
{
	FKeysMesh M;
	if (Shot.bAlive)
	{
		const double Tip = Shot.Dist, Tail = FMath::Max(0.0, Shot.Dist - .6);
		auto ToL = [](const FVector& G) { return FVector(G.X, G.Z, G.Y); };   // game -> Unreal axes, meters
		// black, with the barbed head leading (both drawn over size, as the old plain shaft was, to read at range)
		const FVector HeadDir = ToL(Shot.D).GetSafeNormal();
		Tube(M, ToL(Shot.At(Tail)), ToL(Shot.At(Tip)) - HeadDir * .2, .00325 * 2, .00325 * 2, 6, Hex(0x17191c), true);
		KeysSpeargun::AddHead(M, ToL(Shot.At(Tip)), HeadDir, FVector::ZAxisVector, 1.6, 14, true);
		// the shooting line trailing back to the muzzle, sagging a little
		const FVector TailP = Shot.At(Tail);
		FVector Prev = ToL(Shot.O);
		for (int32 i = 1; i <= 8; i++)
		{
			const double U = i / 8.0;
			const double Sag = FMath::Sin(U * PI) * FMath::Min(.12, Tail * .03);
			const FVector P = ToL(FVector(Shot.O.X + (TailP.X - Shot.O.X) * U, Shot.O.Y + (TailP.Y - Shot.O.Y) * U - Sag, Shot.O.Z + (TailP.Z - Shot.O.Z) * U));
			Tube(M, Prev, P, .002, .002, 4, FLinearColor(.8f, .85f, .88f));
			Prev = P;
		}
	}
	if (Speared.bOn && bDiving)
	{
		// the float line from the fish up to the float at the surface, red as it comes tight
		const FVector Fp(Speared.F.X, S.Y, Speared.F.Z);
		const double Surf = WaveH(Fp.X, Fp.Z, T, AmpAt(Fp.X, Fp.Z), false);
		const double Tn = DvClamp(Speared.F.Tension, 0, 1.2);
		const double TFrac = DvClamp((Speared.F.Tension - .25) / .85, 0, 1);
		const double Span = FMath::Abs(Surf - Fp.Y);
		const double Sag = (1 - TFrac) * DvClamp(Span * .22, 0, 1.1);
		const double Judder = Speared.F.Tension > .95 ? FMath::Sin(T * 46) * .03 * DvClamp((Speared.F.Tension - .95) / .25, 0, 1) : 0;
		const FLinearColor LC = FMath::Lerp(FLinearColor::White, Hex(0xff5a3c), float(DvClamp(Tn, 0, 1)));
		FVector Prev;
		for (int32 i = 0; i <= 16; i++)
		{
			const double U = i / 16.0;
			const FVector P(Fp.X + Judder * FMath::Sin(U * 7 + T * 20), Fp.Z + Judder * FMath::Cos(U * 7 + T * 20) - Tn * .35 * U * (1 - U) * 2, Fp.Y + (Surf - Fp.Y) * U - FMath::Sin(U * PI) * Sag);
			if (i) Tube(M, Prev, P, .006, .006, 4, LC);
			Prev = P;
		}
		Ellipsoid(M, FVector(Fp.X, Fp.Z, Surf + .07), FVector(.14), 10, Hex(0xf2c14e));
		Ellipsoid(M, FVector(Fp.X, Fp.Z, Surf - .06), FVector(.13), 10, FLinearColor::White);
	}
	if (M.V.Num()) M.Commit(Lines, 0, AKeysWorld::VertexColorMaterial());
	else Lines->ClearAllMeshSections();
}

void AKeysDiver::UpdateFishFill()
{
	if (!FishFill) return;
	const AKeysBoatPawn* Pw = Pawn();
	const bool bHeld = FishMode == 3 || FishMode == 4;
	const bool bCard = bDiving && Pw && Pw->FState == EKeysFish::Caught && Pw->bCardSpeared;
	const bool bOn = (bCamUnder && (bHeld || bCard)) || (bDiving && GlanceW > .05);   // and the gauge console, while it is up
	if (bOn)
	{
		// up and behind the mask, a little to the right, so the side of the fish facing the lens carries the light
		const FQuat CQ = CamRotUE.Quaternion();
		FishFill->SetWorldLocation(CamPosUE + CQ.RotateVector(FVector(-40, 50, 70)));
		FishFill->SetIntensity(900.f);
		FishFill->SetAttenuationRadius(700.f);
	}
	if (FishFill->IsVisible() != bOn) FishFill->SetVisibility(bOn);
}

void AKeysDiver::UpdateSpearedFish(double Dt, double T)
{
	if (FishMode == 0) { FishOnLine->SetVisibility(false); return; }
	FishOnLine->SetVisibility(true);
	double Pitch = 0, Yaw = 0, Roll = 0, FishSc = 1;
	if (FishMode == 3 || FishMode == 4)
	{
		KillT += Dt;
		const FQuat CQ = CamRotUE.Quaternion();
		// where the diver holds it up: a little below and left of the view, side-on
		const FVector HoldU = CamPosUE + CQ.RotateVector(FVector(1.05, -.32, -.3) * 100);
		const FVector HoldG = U2G(HoldU);
		if (FishMode == 3)
		{
			// 0-0.7 s: kicking on the shaft where it was hit, bleeding; 0.7-1.9 s: drawn in along the line to the hand
			const double Sh = KillT < .7 ? (1 - KillT / .7) : 0;
			if (KillT < .7 && FMath::FRand() < Dt * 6) BleedAt(FishPos, 1, .05);
			const double In = DvClamp((KillT - .7) / 1.2, 0, 1), E = In * In * (3 - 2 * In);
			const FVector P = FMath::Lerp(FishTgt, HoldG, E);
			if (In > 0 && FMath::FRand() < Dt * 2) BleedAt(P, 1, .03);
			Yaw = FishHead + FMath::Sin(KillT * 38) * .35 * Sh;
			Roll = FMath::Sin(KillT * 31 + .7) * .5 * Sh + E * .3;
			Pitch = FMath::Sin(KillT * 24) * .2 * Sh;
			FishOnLine->SetWorldLocationAndRotation(G2U(P), FRotator(FMath::RadiansToDegrees(Pitch), YawDeg(Yaw), FMath::RadiansToDegrees(Roll)));
			if (KillT >= 1.9) { FishMode = 4; KillT = 0; if (AKeysBoatPawn* Pw = Pawn()) Pw->ShowMsg(FString::Printf(TEXT("%s - on the stringer."), *Speared.Target.Key.Replace(TEXT("_"), TEXT(" "))), 2.5); }
			FishOnLine->SetWorldScale3D(FVector(1));
			return;
		}
		// held up: side-on across the view, tail drooping, a last flick or two
		const FRotator R = (CQ * FRotator(-12, 90, 8 + FMath::Sin(KillT * 3) * 3 * FMath::Max(0.0, 1 - KillT)).Quaternion()).Rotator();
		FishOnLine->SetWorldLocationAndRotation(HoldU, R);
		FishOnLine->SetWorldScale3D(FVector(.8));
		if (KillT > 2.6)
		{
			if (AKeysBoatPawn* Pw = Pawn()) Pw->LandSpeared(Speared.Target.Key, Speared.Target.Weight, S.X, S.Z);
			FishMode = 0;
			FishOnLine->ClearAllMeshSections();
		}
		return;
	}
	if (FishMode == 1)
	{
		FishPos.X = DvLerp(FishPos.X, FishTgt.X, FMath::Min(1.0, Dt * 9));
		FishPos.Y = DvLerp(FishPos.Y, FishTgt.Y, FMath::Min(1.0, Dt * 6));
		FishPos.Z = DvLerp(FishPos.Z, FishTgt.Z, FMath::Min(1.0, Dt * 9));
		const double DX = FishTgt.X - FishPos.X, DZ = FishTgt.Z - FishPos.Z;
		if (DX * DX + DZ * DZ > 1e-5) FishHead = FMath::Atan2(-DX, -DZ);
		const double Fight = DvClamp(FishTension, 0, 1.2), Ex = 1 - DvClamp(FishStam, 0, 1);
		const double Amp = .1 + Fight * .3 + Ex * .2, Fq = 5 + Fight * 9 + Ex * 4;
		Pitch = FMath::Sin(T * Fq * .7) * Amp * .5;
		Yaw = FishHead + FMath::Sin(T * Fq) * Amp;
		Roll = FMath::Sin(T * Fq * 1.4 + .6) * Amp * .6;
		FishSc = 1 + FMath::Sin(T * Fq * 2) * .04 * (Fight + Ex);
	}
	else
	{
		FleeT += Dt;
		FishPos += FVector(FleeDir.X * 2.4 * Dt, -.25 * Dt, FleeDir.Z * 2.4 * Dt);
		Pitch = .12;
		Yaw = FMath::Atan2(-FleeDir.X, -FleeDir.Z) + FMath::Sin(T * 13) * .1;
		Roll = FMath::Sin(T * 13 + .5) * .14;
		if (FleeT > 2.4) { FishMode = 0; FishOnLine->ClearAllMeshSections(); return; }
	}
	FishOnLine->SetWorldLocationAndRotation(G2U(FishPos), FRotator(FMath::RadiansToDegrees(Pitch), YawDeg(Yaw), FMath::RadiansToDegrees(Roll)));
	FishOnLine->SetWorldScale3D(FVector(FishSc));
}

// ================================================================ the look under the surface
void AKeysDiver::UpdateUnderwaterLook(double Dt)
{
	FVector CamU = CamPosUE;
	if (!bDiving)
		if (APlayerCameraManager* C = UGameplayStatics::GetPlayerCameraManager(this, 0)) CamU = C->GetCameraLocation();
	CamGame = U2G(CamU);
	const double T = GetWorld()->GetTimeSeconds();
	const double Surf = WaveH(CamGame.X, CamGame.Z, T, AmpAt(CamGame.X, CamGame.Z), false);
	const double Y = CamGame.Y - Surf;
	// (for a moment after ExitToBoat the camera manager still reports the diver's eye under water)
	const bool bUnder = ExFix > 0 && !bDiving && T - OutT < .2 ? false : bCamUnder ? Y <= .15 : Y < -.15;
	if (bUnder != bCamUnder)
	{
		bCamUnder = bUnder;
		Wet = 1;
	}
	UnderAmt += ((bUnder ? 1.0 : 0.0) - UnderAmt) * (1 - FMath::Exp(-Dt / .12));
	if (UnderAmt < 1e-4) UnderAmt = 0; else if (UnderAmt > 1 - 1e-4) UnderAmt = 1;
	Wet *= FMath::Exp(-Dt / .45);
	if (Wet < 1e-3) Wet = 0;
	if (PostMID)
	{
		PostMID->SetScalarParameterValue(TEXT("Under"), float(UnderAmt));
		PostMID->SetScalarParameterValue(TEXT("Wet"), float(Wet));
		PostMID->SetScalarParameterValue(TEXT("Haze"), float(UnderwaterHaze));
		PostMID->SetScalarParameterValue(TEXT("Bonus"), float(ViewBonusM));   // the wide mask sees further
		if (const AKeysWorld* Wd = AKeysWorld::Get(this)) { const FVector Sd = Wd->SunDir(); PostMID->SetVectorParameterValue(TEXT("SunDir"), FLinearColor(Sd.X, Sd.Y, Sd.Z, 0)); }
		PostMID->SetScalarParameterValue(TEXT("Time"), float(GetWorld()->GetTimeSeconds()));
		ShockT += Dt;
		const double Sk = ShockT < .5 ? FMath::Square(1 - ShockT / .5) : 0;
		PostMID->SetScalarParameterValue(TEXT("Shock"), float(Sk * UnderAmt));
		PostMID->SetVectorParameterValue(TEXT("ShockP"), FLinearColor(ShockUV.X, ShockUV.Y, .02 + ShockT * .85, 0));
	}
	Post->bEnabled = UnderAmt > 0 || Wet > 0;
	if (AKeysAudio* A = AKeysAudio::Get(this))
		A->SetUnderwater(bUnder, FMath::Max(0.0, -Y), bDiving ? BreathFrac() : 1, bDiving && !Sc.OnGas() && !S.bBlackedOut && S.Depth() > KeysDive::SurfaceEps, Dt);
	if (AKeysWorld* W = AKeysWorld::Get(this)) W->SetUnderside(bUnder || UnderAmt > .01);
}

void AKeysDiver::UpdateExposure(double Dt)
{
	const KeysDiverEye::FProbe* Pr = KeysDiverEye::Get();
	const double Now = GetWorld()->GetTimeSeconds();
	// the camera clear of the water and the underwater look off: what the eye has settled on up here
	if (Pr && Pr->Exposure > 0 && !Post->bEnabled) DeckLogE = FMath::Log2(Pr->Exposure);
	// under water: auto exposure held where it would give the deck's exposure (times 2^ExOpen). The white point that
	// does it is bias - log2(exposure) in EV100 (PostProcessEyeAdaptation.cpp: exposure = 2^bias / 2^EV). Only while
	// the camera is under: the lens-wet tail after surfacing has the deck's own exposure and bias.
	FPostProcessSettings& St = Post->Settings;
	const bool bUnderNow = UnderAmt > 0;
	const bool bHold = ExFix > 0 && bUnderNow && DeckLogE < 90;
	St.bOverride_AutoExposureBias = ExFix > 0 ? bUnderNow : true;
	St.bOverride_AutoExposureMinBrightness = St.bOverride_AutoExposureMaxBrightness = bHold;
	if (bHold) St.AutoExposureMinBrightness = St.AutoExposureMaxBrightness = float(St.AutoExposureBias - (DeckLogE + ExOpen));
	if (bSurfLog && Pr && Now >= SurfLogNext)
	{
		SurfLogNext = Now + .1;
		UE_LOG(LogTemp, Log, TEXT("KRSurf t %.2f diving %d camUnder %d under %.3f wet %.2f post %d exposure %.4f (%.2f EV from deck) lum %.4f bias %.2f hold %d white %.2f"),
			Now, bDiving ? 1 : 0, bCamUnder ? 1 : 0, UnderAmt, Wet, Post->bEnabled ? 1 : 0, Pr->Exposure, Pr->Exposure > 0 && DeckLogE < 90 ? FMath::Log2(Pr->Exposure) - DeckLogE : 0.0,
			Pr->Lum, Pr->Bias, bHold ? 1 : 0, bHold ? St.AutoExposureMinBrightness : 0.f);
	}
}

int32 AKeysDiver::SpawnBubble(const FVector& P, double R)
{
	int32 I = -1;
	for (int32 n = 0; n < Bubbles.Num(); n++)
	{
		const int32 K = (BubCursor + n) % Bubbles.Num();
		if (Bubbles[K].Age < 0) { I = K; break; }
	}
	if (I < 0) I = BubCursor;
	BubCursor = (I + 1) % Bubbles.Num();
	FBubble& B = Bubbles[I];
	B.Age = 0; B.P = P; B.V = FVector::ZeroVector; B.R0 = R; B.P0 = AtaAt(FMath::Max(0.0, -P.Y));
	B.WobA = .05 + FMath::FRand() * .12; B.WobF = 1.6 + FMath::FRand() * 2.4; B.WobP = FMath::FRand() * PI * 2;
	B.bReg = false;
	return I;
}

void AKeysDiver::Puff(int32 N, const FVector& P, double RMin, double RMax)
{
	for (int32 k = 0; k < N; k++)
		SpawnBubble(P + FVector((FMath::FRand() - .5) * .16, (FMath::FRand() - .5) * .08, (FMath::FRand() - .5) * .16), DvRand(RMin, RMax));
}

// the diver's breath (entities/diver/bubbles.ts): a slow leak that quickens as the air runs low and with effort,
// a clearing puff every 2.4 m of descent, a burst on surfacing and on blackout; bubbles grow as they rise
void AKeysDiver::BleedAt(const FVector& G, int32 N, double Spread)
{
	for (int32 i = 0; i < N * 2; i++)
	{
		FBlood B;
		B.P = G + FVector(DvRand(-1, 1), DvRand(-1, 1), DvRand(-1, 1)) * Spread;
		B.V = FVector(DvRand(-1, 1), DvRand(-.3, .6), DvRand(-1, 1)) * .25;
		B.Age = 0; B.Life = DvRand(7, 13); B.R = DvRand(.035, .09);
		B.Ph = DvRand(0, 2 * PI); B.Tone = FMath::FRand();
		int32 Slot = Blood.IndexOfByPredicate([](const FBlood& X) { return X.Age < 0; });
		if (Slot == INDEX_NONE) { if (Blood.Num() < 420) Slot = Blood.Add(B); else Slot = FMath::RandRange(0, Blood.Num() - 1); }
		Blood[Slot] = B;
	}
}

void AKeysDiver::UpdateBubbles(double Dt, double T)
{
	// the blood: clouds that spread, drift off on the current and thin out
	for (FBlood& B : Blood)
	{
		if (B.Age < 0) continue;
		B.Age += Dt;
		if (B.Age > B.Life) { B.Age = -1; continue; }
		const FVector2D Cur = KeysSea::CurrentAt(B.P.X, B.P.Z, T, FMath::Max(0.0, -B.P.Y));
		B.V *= FMath::Max(0.0, 1 - Dt * 1.2);
		// the swell's orbit: the water under the waves goes round and round, strongest near the surface, so the cloud
		// rocks back and forth and up and down with it instead of only drifting
		const double Orb = FMath::Exp(FMath::Min(0.0, B.P.Y) / 5.0) * .32;
		const double W = T * 1.15 + B.Ph * .25 + B.P.X * .12;
		const FVector Swell(FMath::Cos(W) * Orb, FMath::Sin(W) * Orb * .6, FMath::Cos(W * .8 + 1.3) * Orb * .5);
		B.P += (B.V + FVector(Cur.X, .02, Cur.Y) + Swell) * Dt;
	}
	const double H = FMath::Min(.05, FMath::Max(0.0, Dt));
	if (bDiving)
	{
		const double Cp = FMath::Cos(S.Pitch);
		const FVector Fw(-FMath::Sin(S.Yaw) * Cp, FMath::Sin(S.Pitch), -FMath::Cos(S.Yaw) * Cp);
		const FVector Mouth(S.X + Fw.X * .5, S.Y + .12 + Fw.Y * .5, S.Z + Fw.Z * .5);
		const double Depth = S.Depth();
		if (Depth > .4 && Sc.OnGas())
		{
			// on scuba the only bubbles are the regulator's: nothing on the breath in, then a burst out of both sides of
			// the exhaust under her chin that tails off as the breath ends. They are bigger than a freediver's trickle,
			// rise faster, and break the surface where the boat can see them.
			LastEqDepth = Depth;
			if (Sc.bExhaling && !(DbgOff & 2))
			{
				const double Env = 1 - .6 * DvClamp((Sc.BreathPh - .45) / .45, 0, 1);
				// (fewer leave her the deeper she is: they have further to rise and each one grows on the way, so the
				// column stays about as thick and the pool is not emptied by a deep dive)
				ExhAcc += (15 + 10 * S.Exertion) * Env * H / (1 + Depth / 12);
				const FVector Rt(FMath::Cos(S.Yaw), 0, -FMath::Sin(S.Yaw));
				while (ExhAcc >= 1)
				{
					ExhAcc -= 1;
					const double Sd = FMath::FRand() < .5 ? -1 : 1;
					// (in the dive-mask view they leave from beside her jaw and a little behind the glass, so they stream
					// up past the edges of the view and not across the middle of it)
					const FVector At = CamMode == 0 ? Mouth - Fw * .16 + Rt * (Sd * DvRand(.2, .3)) + FVector(DvRand(-.03, .03), DvRand(-.16, -.08), DvRand(-.03, .03))
						: Mouth + Rt * (Sd * DvRand(.07, .13)) + FVector(DvRand(-.03, .03), DvRand(-.1, -.03), DvRand(-.03, .03));
					Bubbles[SpawnBubble(At, DvRand(.007, .024))].bReg = true;
				}
			}
			else ExhAcc = FMath::Min(ExhAcc, 1.0);
		}
		else if (Depth > .4)
		{
			if (Depth > LastEqDepth + 2.4) { LastEqDepth = Depth; Puff(4 + FMath::RandRange(0, 3), Mouth, .004, .011); }
			else if (Depth < LastEqDepth - 4.8) LastEqDepth = Depth;
			const double Want = 1 - BreathFrac();
			const double Rate = .7 + 7.5 * Want * Want + 3.5 * S.Exertion;
			BubAcc += Rate * H;
			while (BubAcc >= 1) { BubAcc -= 1; Puff(1, Mouth, .003, .009); }
			if (BubAcc > 2) BubAcc = 2;
			// air working out of the mask, the hood and the suit: little clusters wobbling up just in front of the
			// face and off the top of the view, more while kicking and heading down (the air squeezes out)
			const double Desc = FMath::Max(0.0, -S.VY);
			TopAcc += (1.6 + 2.2 * S.Exertion + 3 * FMath::Min(1.0, Desc) + (S.bKicking ? 1.2 : 0)) * H;
			while (TopAcc >= 1)
			{
				TopAcc -= 1;
				const FVector Rt(FMath::Cos(S.Yaw), 0, -FMath::Sin(S.Yaw));
				const FVector At = CamGame + FVector(Fw.X, 0, Fw.Z).GetSafeNormal() * DvRand(.35, .7) + Rt * DvRand(-.35, .35) + FVector(0, DvRand(-.05, .12), 0);
				const int32 Nc = FMath::RandRange(1, 3);
				for (int32 k = 0; k < Nc; k++)
					SpawnBubble(At + FVector(DvRand(-.03, .03), DvRand(-.04, 0), DvRand(-.03, .03)), DvRand(.0035, .011));
			}
			if (TopAcc > 3) TopAcc = 3;
		}
		else LastEqDepth = 0;
		// the duck dive: a rush of air off the suit and mask as the head goes under
		if (Depth > .4 && !bWasUnder) Puff(22, CamGame + FVector(Fw.X, 0, Fw.Z).GetSafeNormal() * .5 + FVector(0, -.1, 0), .004, .016);
		bWasUnder = Depth > .4;
		for (const EEvent E : FrameEvents)
		{
			if (E == EEvent::Surfaced) Puff(24, Mouth - FVector(0, .1, 0), .006, .02);
			if (E == EEvent::BlackoutAir || E == EEvent::BlackoutShallow) Puff(44, Mouth, .006, .024);
		}
	}
	const FVector2D Flow = KeysSea::CurrentAt(CamGame.X, CamGame.Z, T, FMath::Max(0.0, -CamGame.Y)) * .92;
	const double Relax = FMath::Min(1.0, H * 1.7);
	for (FBubble& B : Bubbles)
	{
		if (B.Age < 0) continue;
		B.Age += H;
		const double R = B.R0 * FMath::Pow(B.P0 / AtaAt(FMath::Max(0.0, -B.P.Y)), 1.0 / 3);
		const double Rise = B.bReg ? FMath::Min(.62, .3 * FMath::Sqrt(R / .011)) : FMath::Min(.46, .26 * FMath::Sqrt(R / .011));
		B.V.X += (Flow.X - B.V.X) * Relax;
		B.V.Z += (Flow.Y - B.V.Z) * Relax;
		const double W = B.WobF * (T + B.WobP);
		B.P += FVector((B.V.X + FMath::Cos(W) * B.WobA) * H, Rise * H, (B.V.Z + FMath::Sin(W * .9) * B.WobA) * H);
		if (B.P.Y > -.06 || B.Age > 110)
		{
			// a regulator bubble breaks at the surface: a ring of froth that spreads and goes
			if (B.bReg && B.P.Y > -.06)
			{
				FPop& Pp = Pops[PopCursor];
				PopCursor = (PopCursor + 1) % Pops.Num();
				Pp.P = B.P; Pp.Age = 0; Pp.R = FMath::Max(.09, R * 5);
			}
			B.Age = -1;
		}
	}
	for (FPop& Pp : Pops)
	{
		if (Pp.Age < 0) continue;
		Pp.Age += H;
		Pp.P.X += Flow.X * H; Pp.P.Z += Flow.Y * H;
		if (Pp.Age > 1.8) Pp.Age = -1;
	}
	for (FPuff& P : Muzzle)
	{
		if (P.Life <= 0) continue;
		P.Life -= H;
		P.V.X += (Flow.X - P.V.X) * FMath::Min(1.0, H * 1.5);
		P.V.Z += (Flow.Y - P.V.Z) * FMath::Min(1.0, H * 1.5);
		P.V.Y += H * .8;
		P.P += P.V * H;
	}
}

// marine snow (world/underwater/marine-snow.ts): 900 specks wrapping in an 11 m box around the camera, drifting
// with the current and sinking; thicker with depth
void AKeysDiver::UpdateSnow(double Dt, double T)
{
	const double Frac = (.4 + .6 * FMath::Min(1.0, FMath::Max(0.0, -CamGame.Y) / 25)) * UnderAmt;
	if (Frac <= 0) return;
	const FVector2D Flow = KeysSea::CurrentAt(CamGame.X, CamGame.Z, T, DepthFast(CamGame.X, CamGame.Z)) * 1.6;
	for (FSnow& Sn : Snow)
	{
		Sn.P += FVector(Sn.V.X + Flow.X, Sn.V.Y, Sn.V.Z + Flow.Y) * Dt;
		for (int32 A = 0; A < 3; A++)
		{
			const double Rel = Sn.P[A] - CamGame[A];
			Sn.P[A] = CamGame[A] + (FMath::Fmod(FMath::Fmod(Rel + 11, 22.0) + 22, 22.0) - 11);
		}
	}
}

// seep bubbles (world/underwater/seep-bubbles.ts): a few vents on the bottom near the camera, trickling bubbles
void AKeysDiver::UpdateSeeps(double Dt, double T)
{
	if (UnderAmt <= .01 || CamGame.Y > -.4) return;
	constexpr double Cell = 16;
	auto Hs = [](double A, double B, double Sd) { const double Hh = FMath::Sin(A * 127.1 + B * 311.7 + Sd * 74.7) * 43758.5453; return Hh - FMath::FloorToDouble(Hh); };
	if (FVector2D(CamGame.X - LastSeepX, CamGame.Z - LastSeepZ).Size() > 5)
	{
		LastSeepX = CamGame.X; LastSeepZ = CamGame.Z;
		struct FC { int64 Key; FVector P; double Rate, D; };
		TArray<FC> Cands;
		const int32 IX = FMath::FloorToInt32(CamGame.X / Cell), IZ = FMath::FloorToInt32(CamGame.Z / Cell);
		for (int32 j = -2; j <= 2; j++)
			for (int32 i = -2; i <= 2; i++)
			{
				const int32 GX = IX + i, GZ = IZ + j;
				if (Hs(GX, GZ, 1) > .3) continue;
				const double X = (GX + .15 + Hs(GX, GZ, 2) * .7) * Cell, Z = (GZ + .15 + Hs(GX, GZ, 3) * .7) * Cell;
				if (DepthAt(X, Z) < 1.8) continue;
				Cands.Add({(int64(GX) << 32) ^ int64(uint32(GZ)), FVector(X, KeysSea::SeafloorHeightAt(X, Z), Z), 4 + Hs(GX, GZ, 4) * 6, FVector2D(X - CamGame.X, Z - CamGame.Z).Size()});
			}
		Cands.Sort([](const FC& A, const FC& B) { return A.D < B.D; });
		TArray<int32> Free;
		TSet<int64> Keep;
		for (int32 k = 0; k < FMath::Min(8, Cands.Num()); k++) Keep.Add(Cands[k].Key);
		for (int32 s = 0; s < Seeps.Num(); s++) if (!Keep.Contains(Seeps[s].Key)) { Seeps[s].Key = MIN_int64; Free.Add(s); }
		for (int32 k = 0; k < FMath::Min(8, Cands.Num()); k++)
		{
			bool bHave = false;
			for (const FSeep& Sp : Seeps) if (Sp.Key == Cands[k].Key) { bHave = true; break; }
			if (bHave || !Free.Num()) continue;
			const int32 s = Free.Pop();
			Seeps[s] = {Cands[k].Key, Cands[k].P, Cands[k].Rate, 0};
			for (FSeepB& B : SeepB) if (B.Slot == s) B.Age = -1;
		}
	}
	const FVector2D Flow = KeysSea::CurrentAt(CamGame.X, CamGame.Z, T, FMath::Max(0.0, -CamGame.Y)) * .92;
	const double Relax = FMath::Min(1.0, Dt * 1.6);
	for (int32 s = 0; s < Seeps.Num(); s++)
	{
		FSeep& E = Seeps[s];
		if (E.Key == MIN_int64) continue;
		E.Acc = FMath::Min(2.0, E.Acc + E.Rate * Dt);
		for (int32 b = 0; b < 34; b++)
		{
			FSeepB& B = SeepB[s * 34 + b];
			if (B.Age < 0)
			{
				if (E.Acc < 1) continue;
				E.Acc -= 1;
				B.Age = 0; B.Life = 3.2 + FMath::FRand() * 3;
				B.P = E.P + FVector((FMath::FRand() - .5) * .25, .05, (FMath::FRand() - .5) * .25);
				B.V = FVector(0, .3 * (.75 + FMath::FRand() * .5), 0);
				B.Size = .018 + FMath::FRand() * .03;
			}
			B.Age += Dt;
			B.V.X += (Flow.X - B.V.X) * Relax;
			B.V.Z += (Flow.Y - B.V.Z) * Relax;
			B.P += FVector(B.V.X * Dt, B.V.Y * Dt * (1 + B.Age * .06), B.V.Z * Dt);
			B.Size += Dt * .004;
			if (B.Age > B.Life || B.P.Y > -.1) B.Age = -1;
		}
	}
}

// all the bubbles and the snow as camera-facing quads in one translucent mesh
void AKeysDiver::DrawParticles()
{
	TArray<FVector> V, N;
	TArray<FVector2D> UV;
	TArray<FLinearColor> C;
	TArray<int32> Tr;
	FVector CamU = CamPosUE;
	FRotator CamR = CamRotUE;
	if (!bDiving)
		if (APlayerCameraManager* PCM = UGameplayStatics::GetPlayerCameraManager(this, 0)) { CamU = PCM->GetCameraLocation(); CamR = PCM->GetCameraRotation(); }
	const FQuat Q = CamR.Quaternion();
	const FVector Rt = Q.GetRightVector(), Up = Q.GetUpVector(), Fw = Q.GetForwardVector();
	auto Quad = [&](const FVector& G, double Diam, const FLinearColor& Col, bool bSnow)
	{
		const FVector P = G2U(G);
		const double Dist = FVector::DotProduct(P - CamU, Fw);
		if (Dist < 5) return;
		const double Hs = FMath::Max(Diam * 100, Dist * .0012) * .5;
		const int32 I = V.Num();
		const double U0 = bSnow ? 2 : 0;
		V.Add(P - Rt * Hs - Up * Hs); V.Add(P + Rt * Hs - Up * Hs); V.Add(P + Rt * Hs + Up * Hs); V.Add(P - Rt * Hs + Up * Hs);
		UV.Add(FVector2D(U0, 1)); UV.Add(FVector2D(U0 + .999, 1)); UV.Add(FVector2D(U0 + .999, 0)); UV.Add(FVector2D(U0, 0));
		for (int32 k = 0; k < 4; k++) { N.Add(-Fw); C.Add(Col); }
		// faces the camera: Dot(Cross(B-A, C-A), -Fw) < 0 for this order
		Tr.Append({I, I + 2, I + 1, I, I + 3, I + 2});
	};
	for (const FBubble& B : Bubbles)
	{
		if (B.Age < 0) continue;
		const double R = B.R0 * FMath::Pow(B.P0 / AtaAt(FMath::Max(0.0, -B.P.Y)), 1.0 / 3);
		const double A = FMath::Min(1.0, B.Age * 6) * (B.Age > 108 ? (110 - B.Age) / 2 : 1) * .9;
		Quad(B.P, R * 2.4, FLinearColor(.88f, .97f, 1.f, float(A)), false);
	}
	// the froth where the regulator's bubbles break: flat on the water, seen from the boat (and from below)
	{
		const double Tn = GetWorld()->GetTimeSeconds();
		for (const FPop& Pp : Pops)
		{
			if (Pp.Age < 0) continue;
			const double U = Pp.Age / 1.8;
			const double Y = WaveH(Pp.P.X, Pp.P.Z, Tn, AmpAt(Pp.P.X, Pp.P.Z), false) + .025;
			const FVector P = G2U(FVector(Pp.P.X, Y, Pp.P.Z));
			const double Hs = Pp.R * (1 + 3.2 * U) * 100;
			const FLinearColor Col(.95f, .98f, 1.f, float((1 - U) * (1 - U) * .85));
			for (int32 Pass = 0; Pass < 2; Pass++)
			{
				// a bubble's ring over a soft white patch
				const double U0 = Pass == 0 ? 2 : 0, Sz = Pass == 0 ? Hs * 1.3 : Hs;
				const int32 I = V.Num();
				V.Add(P + FVector(-Sz, -Sz, Pass * .5)); V.Add(P + FVector(Sz, -Sz, Pass * .5)); V.Add(P + FVector(Sz, Sz, Pass * .5)); V.Add(P + FVector(-Sz, Sz, Pass * .5));
				UV.Add(FVector2D(U0, 1)); UV.Add(FVector2D(U0 + .999, 1)); UV.Add(FVector2D(U0 + .999, 0)); UV.Add(FVector2D(U0, 0));
				for (int32 k = 0; k < 4; k++) { N.Add(FVector::UpVector); C.Add(Pass == 0 ? FLinearColor(Col.R, Col.G, Col.B, Col.A * .55f) : Col); }
				Tr.Append({I, I + 2, I + 1, I, I + 3, I + 2, I, I + 1, I + 2, I, I + 2, I + 3});
			}
		}
	}
	for (const FBlood& B : Blood)
		if (B.Age >= 0)
		{
			// blood under water reads dark: a deep maroon going brown-green as the reds are absorbed with depth; it
			// blooms fast, then spreads slowly into a thin haze
			const double U = B.Age / B.Life;
			const double Dep = DvClamp(-B.P.Y / 12.0, 0, 1);
			const FLinearColor Fresh(.16f + .05f * float(B.Tone), .015f, .02f), Deep(.07f, .045f, .03f);
			FLinearColor Col = FMath::Lerp(Fresh, Deep, float(Dep * .8));
			const double Size = B.R * 2 * (1 + 2.2 * (1 - FMath::Exp(-B.Age * 1.4)) + B.Age * .12);
			Col.A = float(FMath::Min(1.0, B.Age * 8) * FMath::Pow(1 - U, 1.5) * (.42 - .18 * U));
			Quad(B.P, Size, Col, true);
		}
	for (const FPuff& P : Muzzle)
		if (P.Life > 0) Quad(P.P, (.04 + (1 - P.Life / P.Max) * .05) * .6, FLinearColor(.85f, .95f, 1.f, float(DvClamp(P.Life / P.Max, 0, 1) * .8)), false);
	if (UnderAmt > .01)
	{
		for (const FSeepB& B : SeepB)
			if (B.Age >= 0)
			{
				const double U = B.Age / B.Life;
				Quad(B.P, B.Size, FLinearColor(.86f, .96f, 1.f, float(FMath::Min(1.0, B.Age * 4) * (1 - U * U) * .75 * UnderAmt)), false);
			}
		const double Dens = (.4 + .6 * FMath::Min(1.0, FMath::Max(0.0, -CamGame.Y) / 25)) * UnderAmt;
		for (const FSnow& Sn : Snow)
		{
			const double Fade = 1 - DvClamp(((Sn.P - CamGame).Size() - 11 * .55) / (11 * .45), 0, 1);
			if (Fade <= 0) continue;
			Quad(Sn.P, (1.1 + Sn.Seed * 1.4) * .126 * .25, FLinearColor(.82f, .88f, .86f, float((.12 + .23 * Sn.Seed) * Dens * Fade)), true);
		}
	}
	if (!V.Num()) { Particles->ClearAllMeshSections(); return; }
	Particles->CreateMeshSection_LinearColor(0, V, Tr, N, UV, C, TArray<FProcMeshTangent>(), false);
	if (BubbleMID) Particles->SetMaterial(0, BubbleMID);
}

void AKeysDiver::DebugCmd(const FString& Cmd, const FString& Arg)
{
	// ---- scuba, on the boat or in the water
	if (Cmd == TEXT("scuba")) { KeysScuba::SetWantScuba(FCString::Atoi(*Arg) != 0); UE_LOG(LogTemp, Log, TEXT("KRScuba chosen: %s"), KeysScuba::WantScuba() ? TEXT("scuba") : TEXT("breath-hold")); return; }
	if (Cmd == TEXT("scubatest")) { KeysScuba::SelfTest(); return; }
	if (Cmd == TEXT("mode")) { UE_LOG(LogTemp, Log, TEXT("KRScuba mode: %s"), *KeysScuba::ToggleMode(Pawn())); return; }
	if (Cmd == TEXT("kit"))
	{
		const AKeysBoatPawn* P = Pawn();
		UE_LOG(LogTemp, Log, TEXT("KRScuba kit: set %d, cylinders %d (%.0f / %.0f psi), computer %d, light %d, will dive on scuba %d, worn kit %d triangles, logbook %d"),
			KeysScuba::HasSet(P) ? 1 : 0, KeysScuba::Cylinders(P), KeysScuba::CylinderPsi(P, 0), KeysScuba::CylinderPsi(P, 1), KeysScuba::Owns(P, KeysScuba::FxComputer) ? 1 : 0,
			KeysScuba::Owns(P, KeysScuba::FxLight) ? 1 : 0, KeysScuba::WillDive(P) ? 1 : 0, KitTris, KeysScuba::Logbook().Num());
		TArray<TPair<FString, bool>> Rows;
		KeysScuba::LoadoutRows(P, Rows);
		for (const TPair<FString, bool>& R : Rows) UE_LOG(LogTemp, Log, TEXT("KRScuba loadout: %s%s"), *R.Key, R.Value ? TEXT("") : TEXT(" (dim)"));
		return;
	}
	if (Cmd == TEXT("boatview")) { DbgBoatView = FCString::Atoi(*Arg); return; }
	if (Cmd == TEXT("off"))
	{
		// test hook: parts of the scuba side switched off, to find what a frame costs (1 the regulator's sound, 2 its
		// bubbles, 4 the rack and flag on the boat, 8 the worn set, 16 the fish's calm round a scuba diver)
		DbgOff = FCString::Atoi(*Arg);
		BoatRack->SetVisibility(!(DbgOff & 4)); BoatFlag->SetVisibility(!(DbgOff & 4));
		UE_LOG(LogTemp, Log, TEXT("KRScuba off %d"), DbgOff);
		return;
	}
	if (Cmd == TEXT("prof")) { bDbgProf = FCString::Atoi(*Arg) != 0; ProfN = 0; for (double& M : ProfMs) M = 0; return; }
	if (Cmd == TEXT("light")) { const int32 V = FCString::Atoi(*Arg); TorchMode = V == 1 ? 1 : V == 0 ? 2 : 0; return; }
	// the exposure across the surface (UpdateExposure): its log, the switch back to the old free exposure, the band
	if (Cmd == TEXT("surflog")) { bSurfLog = FCString::Atoi(*Arg) != 0; return; }
	if (Cmd == TEXT("exfix")) { ExFix = FCString::Atoi(*Arg); UE_LOG(LogTemp, Log, TEXT("KRSurf exfix %d"), ExFix); return; }
	if (Cmd == TEXT("exopen")) { ExOpen = FCString::Atod(*Arg); UE_LOG(LogTemp, Log, TEXT("KRSurf exopen %.2f"), ExOpen); return; }
	if (!bDiving) return;
	if (Cmd == TEXT("psi")) { Sc.Psi = DvClamp(FCString::Atod(*Arg), 0, KeysScuba::FullPsi); }
	else if (Cmd == TEXT("nload")) { Sc.NLoad = DvClamp(FCString::Atod(*Arg), 0, 1); }
	else if (Cmd == TEXT("gauge")) { bDebugGlance = FCString::Atoi(*Arg) != 0; }
	else if (Cmd == TEXT("swap")) { UE_LOG(LogTemp, Log, TEXT("KRScuba swap: %s"), *SwapCylinder()); }
	else if (Cmd == TEXT("orbit"))
	{
		TArray<FString> P;
		Arg.ParseIntoArray(P, TEXT(","));
		bDbgOrbit = P.Num() >= 1 && Arg != TEXT("off");
		if (P.Num() >= 1) DbgOrbitYaw = FCString::Atod(*P[0]);
		if (P.Num() >= 2) DbgOrbitPitch = FCString::Atod(*P[1]);
		if (P.Num() >= 3) DbgOrbitDist = FCString::Atod(*P[2]);
	}
	else if (Cmd == TEXT("topside")) { bDbgTopside = FCString::Atoi(*Arg) != 0; }
	else if (Cmd == TEXT("hover")) { DebugHoldDepth = -1; bDebugSwim = false; DebugAscend = 0; S.VY = 0; HoverT = 0; HoverY0 = S.Y; HoverX0 = S.X; HoverZ0 = S.Z; }
	else if (Cmd == TEXT("gaslog")) { GasLogT = 0; GasLogPsi0 = Sc.Psi; GasLogSecs = Arg.IsEmpty() ? 120 : FCString::Atod(*Arg); }
	else if (Cmd == TEXT("ascend")) { DebugAscend = FCString::Atoi(*Arg); if (DebugAscend) DebugHoldDepth = -1; }
	else if (Cmd == TEXT("sprint")) { bDebugSprint = FCString::Atoi(*Arg) != 0; }
	else if (Cmd == TEXT("nearfish")) { FishLogT = FCString::Atoi(*Arg) != 0 || Arg.IsEmpty() ? 0 : -1; }
	else if (Cmd == TEXT("depth"))
	{
		const double Fl = KeysSea::SeafloorHeightAt(S.X, S.Z);
		S.Y = FMath::Max(-FCString::Atod(*Arg), Fl + .6);
		S.VX = S.VY = S.VZ = 0;
	}
	else if (Cmd == TEXT("at"))
	{
		TArray<FString> P;
		Arg.ParseIntoArray(P, TEXT(","));
		if (P.Num() >= 2) { S.X = FCString::Atod(*P[0]); S.Z = FCString::Atod(*P[1]); S.VX = S.VY = S.VZ = 0; }
	}
	else if (Cmd == TEXT("look"))
	{
		TArray<FString> P;
		Arg.ParseIntoArray(P, TEXT(","));
		if (P.Num() >= 1) CamYaw = FCString::Atod(*P[0]);
		if (P.Num() >= 2) CamPitch = FCString::Atod(*P[1]);
		PrevYaw = CamYaw;
	}
	else if (Cmd == TEXT("aimcaelen"))
	{
		// point the gun at Caelen (from the muzzle), for the spear-the-gull test
		FVector CA, CB; double CR;
		if (AKeysExtras* Ex = AKeysExtras::Get(this); Ex && Ex->CaelenHitbox(CA, CB, CR))
		{
			const FVector D = ((CA + CB) * .5 - MuzzleWorld()).GetSafeNormal();
			CamYaw = FMath::Atan2(-D.X, -D.Z); CamPitch = FMath::Asin(FMath::Clamp(D.Y, -1.0, 1.0));
			UE_LOG(LogTemp, Log, TEXT("KRDive Caelen at %.1f m"), ((CA + CB) * .5 - MuzzleWorld()).Size());
		}
	}
	else if (Cmd == TEXT("aimfish"))
	{
		// look straight at the nearest catchable fish within range
		TArray<FTarget> Tg;
		GetTargets(Tg);
		const FVector Eye(S.X, S.Y + .15, S.Z);
		double Best = 1e9;
		for (const FTarget& T : Tg)
		{
			if (!T.bCatchable) continue;
			const FVector C = (T.A + T.B) * .5;
			const double D = (C - Eye).Size();
			if (D < 2.5 || D > 16 || D > Best) continue;
			Best = D;
			const FVector Dir = (C - Eye) / D;
			CamYaw = FMath::Atan2(-Dir.X, -Dir.Z);
			CamPitch = FMath::Asin(DvClamp(Dir.Y, -1, 1));
		}
		PrevYaw = CamYaw;
		UE_LOG(LogTemp, Log, TEXT("KRDive aimfish: %d targets, nearest %.1f m"), Tg.Num(), Best);
	}
	else if (Cmd == TEXT("fishahead"))
	{
		// fishahead or fishahead=marlin[,dist]: the nearest school (of that species) to a few metres in front of the mask
		FString Ty, Ds;
		if (!Arg.Split(TEXT(","), &Ty, &Ds)) Ty = Arg;
		const double D = Ds.IsEmpty() ? 7 : FCString::Atod(*Ds);
		if (AKeysLife* L = AKeysLife::Get(this)) L->DebugBringSchool(S.X - FMath::Sin(CamYaw) * D, S.Z - FMath::Cos(CamYaw) * D, Ty);
	}
	else if (Cmd == TEXT("hold")) DebugHoldDepth = FCString::Atod(*Arg);
	else if (Cmd == TEXT("spear")) bFireQueued = true;
	else if (Cmd == TEXT("reload")) Gun = FGun();
	else if (Cmd == TEXT("fwd")) { if (AKeysBoatPawn* Pw = Pawn()) { CamYaw = PrevYaw = Pw->BoatH(); CamPitch = FCString::Atod(*Arg); } }   // look along the boat's heading, pitch = Arg
	else if (Cmd == TEXT("gunshow")) bDbgGunShow = FCString::Atoi(*Arg) != 0;
	else if (Cmd == TEXT("air")) S.Air = S.AirMaxV * DvClamp(FCString::Atod(*Arg), 0, 1);
	else if (Cmd == TEXT("swim")) bDebugSwim = FCString::Atoi(*Arg) != 0;
	else if (Cmd == TEXT("haul")) bDebugHaul = FCString::Atoi(*Arg) != 0;
	else if (Cmd == TEXT("chase")) { if (CamMode == 0) CycleView(); }
	else if (Cmd == TEXT("mask")) { if (CamMode == 1) CycleView(); }
	UE_LOG(LogTemp, Log, TEXT("KRDive %s %s: pos %.1f %.1f %.1f air %.0f%% floor %.1f"), *Cmd, *Arg, S.X, S.Y, S.Z, BreathFrac() * 100, KeysSea::SeafloorHeightAt(S.X, S.Z));
	if (Sc.bOn) UE_LOG(LogTemp, Log, TEXT("KRScuba %s %s: %.0f psi (bail-out %.0f), depth %.1f m, %.0f s, ascent %.2f m/s, load %.2f, stop %d/%d %.0f s, hit %.2f, wind %.2f"),
		*Cmd, *Arg, Sc.Psi, Sc.BailPsi, S.Depth(), Sc.Secs, Sc.AscRate, Sc.NLoad, Sc.bStopAsked ? 1 : 0, Sc.bStopDone ? 1 : 0, Sc.StopT, Sc.Hit, Sc.Wind);
}

double AKeysDiver::NextShotRange() const
{
	return FMath::Clamp(KeysDive::SpearRange + FMath::Max(0.0, SpearRangeBonusM), KeysDive::SpearRange * .6, KeysDive::SpearRange + 6);
}
