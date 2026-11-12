// The web game's rigged dancers, shared by everyone who parties (see KeysDancers.h).
#include "KeysDancers.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/SkinnedAsset.h"
#include "Animation/AnimSequence.h"
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
	const FDancerAsset Assets[4] = {
		{nullptr, nullptr, 0},
		{TEXT("/Game/KeysRun/Crew/Dance01/dance-01/SkeletalMeshes/dance-01.dance-01"), TEXT("/Game/KeysRun/Crew/Dance01/dance-01/SkeletalMeshes/dance-01_Anim.dance-01_Anim"), 90},
		{TEXT("/Game/KeysRun/Crew/Dance02/dance-02/SkeletalMeshes/dance-02.dance-02"), TEXT("/Game/KeysRun/Crew/Dance02/dance-02/SkeletalMeshes/dance-02_Anim.dance-02_Anim"), 90},
		{TEXT("/Game/KeysRun/Crew/Dance03/dance-03/SkeletalMeshes/dance-03.dance-03"), TEXT("/Game/KeysRun/Crew/Dance03/dance-03/SkeletalMeshes/dance-03_Anim.dance-03_Anim"), 90},
	};
	struct FLoaded { TWeakObjectPtr<USkeletalMesh> M; TWeakObjectPtr<UAnimSequence> A; double H = 1.7; bool bTried = false; };
	FLoaded Loaded[4];

	const FLoaded* Load(int32 Kind)
	{
		if (Kind < 1 || Kind > 3) return nullptr;
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
				if (Kind == 1) L.H = 1.8298;
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
	// like the web game (entities/crew-model/asset.ts): they take shadows but don't cast them (the skinned meshes
	// self-shadow into dark facets otherwise)
	Cm->SetCastShadow(false);
	// the clip moves her well away from the rest pose's bounds (Annie's lies flat): don't cull a dancing figure
	Cm->SetBoundsScale(3.f);
	Cm->RegisterComponent();
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
