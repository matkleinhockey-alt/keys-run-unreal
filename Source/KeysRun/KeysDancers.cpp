// The web game's rigged dancers, shared by everyone who parties (see KeysDancers.h).
#include "KeysDancers.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/SkinnedAsset.h"
#include "Animation/AnimSequence.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"

namespace
{
	struct FDancerAsset
	{
		const TCHAR* Mesh;
		const TCHAR* Anim;
		double YawOff;     // degrees: turns the model's own facing to +X
	};
	constexpr int32 NumKinds = 5;
	constexpr int32 GirlKind = 4;
	const FDancerAsset Assets[NumKinds] = {
		{nullptr, nullptr, 0},
		{TEXT("/Game/KeysRun/Crew/Dance01/dance-01/SkeletalMeshes/dance-01.dance-01"), TEXT("/Game/KeysRun/Crew/Dance01/dance-01/SkeletalMeshes/dance-01_Anim.dance-01_Anim"), 90},
		{TEXT("/Game/KeysRun/Crew/Dance02/dance-02/SkeletalMeshes/dance-02.dance-02"), TEXT("/Game/KeysRun/Crew/Dance02/dance-02/SkeletalMeshes/dance-02_Anim.dance-02_Anim"), 90},
		{TEXT("/Game/KeysRun/Crew/Dance03/dance-03/SkeletalMeshes/dance-03.dance-03"), TEXT("/Game/KeysRun/Crew/Dance03/dance-03/SkeletalMeshes/dance-03_Anim.dance-03_Anim"), 90},
		{TEXT("/Game/KeysRun/Crew/Girl/girl/SkeletalMeshes/girl.girl"), TEXT("/Game/KeysRun/Crew/Girl/girl/SkeletalMeshes/girl_Anim.girl_Anim"), 90},
	};

	// Each girl her own look (the M_KR_Girl* parameters, linear albedo): a skin tone, hair that goes with it, eyes, and
	// a swimsuit with its trim. Picked in a fixed stride through the lists, so a group never repeats side by side.
	struct FTone { FLinearColor Skin; int32 Hair[3]; };
	const FTone Tones[6] = {
		{FLinearColor(.60f, .42f, .32f), {4, 3, 2}},     // fair
		{FLinearColor(.53f, .35f, .25f), {2, 5, 4}},     // light
		{FLinearColor(.43f, .27f, .17f), {1, 2, 3}},     // olive / tanned
		{FLinearColor(.33f, .19f, .11f), {0, 1, 2}},     // caramel
		{FLinearColor(.22f, .12f, .07f), {0, 1, 0}},     // brown
		{FLinearColor(.13f, .07f, .045f), {0, 0, 1}},    // deep
	};
	const FLinearColor Hairs[6] = {
		FLinearColor(.012f, .010f, .009f),   // black
		FLinearColor(.035f, .019f, .010f),   // dark brown
		FLinearColor(.085f, .042f, .018f),   // chestnut
		FLinearColor(.17f, .055f, .022f),    // auburn
		FLinearColor(.42f, .29f, .14f),      // blonde
		FLinearColor(.20f, .13f, .07f),      // light brown
	};
	const FLinearColor Eyes[4] = {
		FLinearColor(.09f, .045f, .02f), FLinearColor(.12f, .10f, .04f), FLinearColor(.05f, .11f, .2f), FLinearColor(.06f, .11f, .06f),
	};
	struct FSuit { FLinearColor A, B; };
	const FSuit Suits[9] = {
		{FLinearColor(.62f, .03f, .17f), FLinearColor(.85f, .8f, .78f)},    // hot pink, white trim
		{FLinearColor(.02f, .02f, .022f), FLinearColor(.75f, .6f, .2f)},    // black, gold trim
		{FLinearColor(.0f, .32f, .33f), FLinearColor(.9f, .88f, .8f)},      // turquoise
		{FLinearColor(.5f, .015f, .02f), FLinearColor(.9f, .9f, .88f)},     // red
		{FLinearColor(.75f, .72f, .68f), FLinearColor(.05f, .1f, .35f)},    // white, navy trim
		{FLinearColor(.72f, .48f, .02f), FLinearColor(.06f, .06f, .06f)},   // yellow
		{FLinearColor(.02f, .05f, .26f), FLinearColor(.85f, .85f, .85f)},   // navy and white stripes
		{FLinearColor(.8f, .2f, .1f), FLinearColor(.95f, .85f, .7f)},       // coral
		{FLinearColor(.2f, .55f, .05f), FLinearColor(.9f, .9f, .9f)},       // lime
	};
	int32 GirlCount = 0;

	void DressGirl(USkeletalMeshComponent* Cm)
	{
		const int32 N = GirlCount++;
		const FTone& T = Tones[(N * 4 + 1) % 6];
		const FLinearColor Hair = Hairs[T.Hair[(N / 6 + N) % 3]];
		const FLinearColor Eye = Eyes[(N * 3 + 2) % 4];
		const FSuit& S = Suits[(N * 5 + 3) % 9];
		// a touch of variation inside each choice too
		const float Sk = FMath::FRandRange(.93f, 1.07f), Hr = FMath::FRandRange(.85f, 1.15f);
		for (int32 m = 0; m < Cm->GetNumMaterials(); m++)
		{
			UMaterialInstanceDynamic* D = Cm->CreateDynamicMaterialInstance(m);
			if (!D) continue;
			D->SetVectorParameterValue(TEXT("Tone"), T.Skin * Sk);
			D->SetVectorParameterValue(TEXT("Hair"), Hair * Hr);
			D->SetVectorParameterValue(TEXT("Eye"), Eye);
			D->SetVectorParameterValue(TEXT("Suit"), S.A);
			D->SetVectorParameterValue(TEXT("Suit2"), S.B);
		}
	}
	struct FLoaded { TWeakObjectPtr<USkeletalMesh> M; TWeakObjectPtr<UAnimSequence> A; double H = 1.7; bool bTried = false; };
	FLoaded Loaded[NumKinds];

	const FLoaded* Load(int32 Kind)
	{
		if (Kind < 1 || Kind >= NumKinds) return nullptr;
		static int32 Off = -1;
		if (Off < 0) Off = FParse::Param(FCommandLine::Get(), TEXT("KRNoCrewModels")) ? 1 : 0;
		if (Off) return nullptr;
		FLoaded& L = Loaded[Kind];
		if (!L.bTried)
		{
			L.bTried = true;
			L.M = LoadObject<USkeletalMesh>(nullptr, Assets[Kind].Mesh);
			L.A = LoadObject<UAnimSequence>(nullptr, Assets[Kind].Anim);
			// the model's own standing height (its rest pose bounds), so every figure can be scaled to a real height
			if (L.M.IsValid())
			{
				const FBox B = L.M->GetImportedBounds().GetBox();
				L.H = FMath::Max(.5, B.GetSize().Z / 100.0);
				// dance-01's bounds are in its quantized mesh space: its real standing height
				if (Kind == 1 || Kind == GirlKind) L.H = 1.8298;
				if (Kind == 3) L.H = 1.61;   // Annie's rest pose lies flat (a VRM export); the clip stands her at 1.61 m
				UE_LOG(LogTemp, Log, TEXT("Keys Run: dancer %d is %.2f m tall at rest (bounds %s)"), Kind, L.H, *B.ToString());
			}
		}
		return L.M.IsValid() && L.A.IsValid() ? &L : nullptr;
	}
}

namespace KeysDancers
{
	bool Ready(int32 Kind) { return Load(Kind) != nullptr; }

	int32 KindFor(int32 N)
	{
		// -KRDancer=N puts every figure on that model (for checking one)
		static int32 Only = -1;
		if (Only < 0) { Only = 0; FParse::Value(FCommandLine::Get(), TEXT("KRDancer="), Only); }
		if (Only > 0 && Ready(Only)) return Only;
		// the full-detail girl (Tools/girlglb.py), each in her own colours; the web game's figures if she is missing
		if (Ready(GirlKind)) return GirlKind;
		// dance-03 (Annie) is left out: her VRM rig still comes through Unreal's glTF import with the head and hair
		// parted from the body under the clip (see Tools/fixglb.py)
		static const int32 Cycle[2] = {2, 1};
		for (int32 k = 0; k < 2; k++)
		{
			const int32 Kd = Cycle[(N + k) % 2];
			if (Ready(Kd)) return Kd;
		}
		return 0;
	}
}

bool FKeysDancerFig::Spawn(UObject* Outer, USceneComponent* Parent, int32 InKind, float PlayRate)
{
	const FLoaded* L = Load(InKind);
	if (!L || !Outer || !Parent) return false;
	USkeletalMeshComponent* Cm = NewObject<USkeletalMeshComponent>(Outer);
	Cm->SetSkeletalMesh(L->M.Get());
	Cm->SetupAttachment(Parent);
	Cm->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	// The web game's decimated figures take shadows but don't cast them (they self-shadow into dark facets otherwise,
	// like entities/crew-model/asset.ts); the full-detail girl is smooth enough to cast them.
	Cm->SetCastShadow(InKind == GirlKind);
	// the clip moves her well away from the rest pose's bounds (Annie's lies flat): don't cull a dancing figure
	Cm->SetBoundsScale(3.f);
	Cm->RegisterComponent();
	if (InKind == GirlKind) DressGirl(Cm);
	Cm->SetAnimationMode(EAnimationMode::AnimationSingleNode);
	Cm->PlayAnimation(L->A.Get(), true);
	// a random start in the clip, so a group isn't dancing in lockstep (index.ts spawnFigure)
	Cm->SetPosition(FMath::FRandRange(0.f, L->A->GetPlayLength()), false);
	Cm->SetPlayRate(PlayRate * FMath::FRandRange(.92f, 1.06f));
	C = Cm;
	Kind = InKind;
	Hip = -2;
	HipAvg = FVector2D::ZeroVector;
	return true;
}

void FKeysDancerFig::SetVisible(bool bOn)
{
	if (USkeletalMeshComponent* Cm = C.Get()) if (Cm->IsVisible() != bOn) Cm->SetVisibility(bOn);
}

void FKeysDancerFig::Place(const FVector& LocM, double Yaw, double Height, double Dt)
{
	USkeletalMeshComponent* Cm = C.Get();
	const FLoaded* L = Load(Kind);
	if (!Cm || !L) return;
	const double Sc = Height / L->H;
	const FRotator Rot(0, FMath::RadiansToDegrees(Yaw) + Assets[Kind].YawOff, 0);
	FVector Loc = LocM * 100.0;
	// The Mixamo clip travels: the hips (the rig's root, so nothing locks them) wander across the floor through the
	// loop. Their slow drift is taken back out each frame so the figure dances on its own spot; the sway stays.
	if (Hip == -2 && Cm->GetSkinnedAsset())
	{
		Hip = INDEX_NONE;
		const FReferenceSkeleton& RS = Cm->GetSkinnedAsset()->GetRefSkeleton();
		for (int32 b = 0; b < RS.GetNum(); b++)
		{
			const FString Nm = RS.GetBoneName(b).ToString();
			if (Nm.Contains(TEXT("Hips"))) { Hip = b; break; }
		}
		if (Hip != INDEX_NONE)
		{
			FTransform Acc = FTransform::Identity;
			for (int32 b = Hip; b != INDEX_NONE; b = RS.GetParentIndex(b)) Acc = Acc * RS.GetRefBonePose()[b];
			HipRest = Acc.GetLocation();
		}
	}
	if (Hip >= 0)
	{
		const FVector Hp = Cm->GetBoneLocation(Cm->GetBoneName(Hip), EBoneSpaces::ComponentSpace) - HipRest;
		const double K = FMath::Clamp(Dt / 2.5, 0.0, 1.0);
		HipAvg = FMath::Lerp(HipAvg, FVector2D(Hp.X, Hp.Y), K);
		Loc -= Rot.RotateVector(FVector(HipAvg.X, HipAvg.Y, 0) * Sc);
	}
	Cm->SetRelativeLocationAndRotation(Loc, Rot);
	Cm->SetRelativeScale3D(FVector(Sc));
}
