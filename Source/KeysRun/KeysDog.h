#pragma once

#include "CoreMinimal.h"

struct FKeysMesh;

// One dog, many dogs (web entities/life/dog.ts): the boat's Jack Russell and every dog on the street of Boot Key
// Harbor City are the same boxes stretched by a look (coat, markings, size, body and leg length) and posed by the
// same trot / sit / brace / lie / sniff solver.
namespace KeysDog
{
	struct FLook { uint32 Coat, Mark, Ear, Nose; double Scale, Body, Leg; bool bPrick, bSaddle; double TailUp; };
	// terrier, lab, beagle, dachshund, husky, blacklab, mutt (DOG_LOOKS)
	const FLook& Look(const FString& Breed);
	enum class EAct : uint8 { Trot, Sit, Brace, Lie, Sniff, Wag };
	struct FPose { EAct Act = EAct::Sit; bool bTrotting = false; double Step = 0, Wag = 0, Lean = 0, T = 0, Ph = 0; bool bExcited = false; };
	constexpr double ShoulderH = .34;   // H: a Jack Russell
	// appends the posed dog: Xf places the dog's own frame (X nose, Y side, Z up, feet on Z = 0, meters) where it stands
	void Build(FKeysMesh& M, const FLook& L, const FPose& P, const FTransform& Xf, double Lift = 1);
}
