#pragma once

#include "CoreMinimal.h"

class USkeletalMeshComponent;
class USceneComponent;
class UObject;

// The web game's rigged, animated dancers (apps/client/public/models/crew/dance-0N.glb, imported into
// Content/KeysRun/Crew/DanceNN): 1 the bikini_girl boat crew, 2 the stripper, 3 Annie; and 4, the girl: bikini_girl at
// full detail under dance-01's rig (Tools/girlglb.py, Content/KeysRun/Crew/Girl), each one dressed in her own skin tone,
// hair, eyes and swimsuit. Everyone dances as the girl when she is there. All of them play Mixamo's
// "Hip Hop Dancing" clip. dance-02 and dance-03 go through Tools/fixglb.py first (one mesh, one skin, bind matrices
// that match the skeleton's rest pose). Shared by the player's boat party, the Pink Flamingo's stage, the sandbar and
// the party boats on the hook.
namespace KeysDancers
{
	// the model is imported and loads (false with -KRNoCrewModels)
	bool Ready(int32 Kind);
	// a kind for the Nth figure in a group (cycles through the loaded models)
	int32 KindFor(int32 N);
}

// One dancing figure: a skeletal mesh component playing the clip from a random point, held on its spot (the clip's
// hips travel; their slow drift is taken back out each frame) at a given height.
struct FKeysDancerFig
{
	TWeakObjectPtr<USkeletalMeshComponent> C;
	int32 Kind = 0;
	int32 Hip = -2;                     // -2: not looked for yet
	FVector HipRest = FVector::ZeroVector;
	FVector2D HipAvg = FVector2D::ZeroVector;

	// creates the component under Parent (owned by Outer, an actor); false if the model is missing
	bool Spawn(UObject* Outer, USceneComponent* Parent, int32 InKind, float PlayRate = 1.f);
	// Loc: Parent-relative, meters, the feet; Yaw radians, 0 facing +X; Height meters
	void Place(const FVector& LocM, double Yaw, double Height, double Dt);
	void SetVisible(bool bOn);
	bool IsValid() const { return C.IsValid(); }
};
