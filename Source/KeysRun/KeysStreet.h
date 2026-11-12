#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "KeysHuman.h"
#include "KeysStreet.generated.h"

class UProceduralMeshComponent;

// Street life in Boot Key Harbor City, ported from the web game's shared/world/town-life.ts (closed-form scripts on
// the wall clock: where everyone is is a pure function of the time, so it is the same for everybody) and the client's
// entities/town/wildlife.ts, critters.ts and streetlife.ts:
//   people with errands - into the tackle shop, the diner, the liquor store, the bar, the gifts shop, the pawn shop,
//     the laundromat, the dive shop, the motel, the ice-cream parlour and the Stop-N-Go and out again with what they
//     bought; joggers, a couple, an angler with his rod, the plaque reader, people crossing at the zebra, a mum and
//     her kid, people on phones, a man who loads a cooler into his truck and takes it out again, talkers, bench sitters
//   the dogs - two on leads, a beagle by the benches, a black lab tied outside the Stop-N-Go, three loose
//   squirrels up the palms, skunks round the bins at dusk, the cat on the dumpster lid, gulls on the rooflines
//   and the details: litter where it collects, puddles, the washing across the alley, news boxes, bike racks,
//     sandwich boards, an ice chest and a stack of crates
UCLASS()
class KEYSRUN_API AKeysStreet : public AActor
{
	GENERATED_BODY()

public:
	AKeysStreet();
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

	enum class EStance : uint8 { Walk, Jog, Stand, Look, Phone, Talk, Sit, SitPhone, Carry, Load, Knock, Stretch, Trot, Sniff, Lie, Wag, Run, Climb, Hop, Perch, Flick, Freeze, Amble, Groom, Loaf };
	struct FPt { double U, V, Y; };
	struct FSeg { uint8 Kind; double T0, T1; TArray<FPt> Pts; TArray<double> Cum; double Len, Speed; EStance Stance; double Yaw; bool bCarry; int32 Idx; double Ramp; };
	struct FScript { FString Id; double Cycle = 60, Offset = 0; TArray<FSeg> Segs; FPt Start; };
	struct FLifePose { double U = 0, V = 0, Y = 0, Yaw = 0, Speed = 0; EStance Stance = EStance::Stand; bool bHidden = false, bCarry = false; int32 Beat = 0; double Bt = 0; };

private:
	void BuildStatic();
	void Draw(double T, double Dt, const FVector& CamGame);

	UPROPERTY() TObjectPtr<USceneComponent> Root;
	UPROPERTY() TObjectPtr<UProceduralMeshComponent> StaticMesh;
	UPROPERTY() TObjectPtr<UProceduralMeshComponent> LiveMesh;

	struct FPerson { FString Id, Look, Prop, Group; FScript S; double Scale = 1; int32 Bench = -1; FKeysHuman Hm; double Gait = 0; };
	TArray<FPerson> People;
	struct FDog { FString Id, Breed, Owner; double Ahead = 0, Dv = 0; bool bLeash = false; FScript S; bool bTie = false; double TieU = 0, TieV = 0; double Step = 0; };
	TArray<FDog> Dogs;
	struct FCritter { FString Id; uint8 Kind; FScript S; double PalmU = 0, PalmV = 0; };   // 0 squirrel, 1 skunk
	TArray<FCritter> Critters;
	double LiveT = 0;
};
